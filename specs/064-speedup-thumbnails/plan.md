# Implementation Plan: Instant Thumbnails in Large Folders

**Branch**: `064-speedup-thumbnails` | **Date**: 2026-08-19 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/064-speedup-thumbnails/spec.md`

## Summary

In Thumbnails view (Alt+5), huge photo folders show no previews for minutes.
Two verified root causes (research.md, RC1–RC4): the icon-reader's phase order
runs a **whole-folder shell overlay sweep before the first thumbnail**, and
each thumbnail is produced by a **full-resolution decode of the entire photo**
(~0.5–1.3 s each) even though the WIC decoder in use offers embedded EXIF
previews (~1–3 ms) and reduced-resolution decode (~10× cheaper) — while the
core's fast/quality two-round protocol sits unused.

**Selected variant (user decision): A+B.**
- **A — viewport-first scheduling**: reorder the reader's phase machine so
  each visibility band gets icons → thumbnails → overlays before any wider
  band (whole-listing overlay sweep last); refresh the visible-items arrays
  synchronously on every scroll; stop holding `ICSleepSection` across plugin
  decodes (adopt the icon branch's leave/revalidate pattern).
- **B — WIC fast path in pictview**: honor `fastThumbnail` with the ladder
  embedded EXIF thumbnail → reduced-resolution decode → full decode, feeding
  the existing `SSTHUMB_ONLY_PREVIEW` + quality-round machinery; apply EXIF
  orientation (fixes the current unrotated-thumbnails defect); demote the
  Thumbs.db probe to a fallback; skip the full-image composite/`BuildLines`
  waste on the thumbnail path.

C (parallel decode pool) and D (persistent cache) are recorded follow-up
candidates, deliberately out of scope (research.md comparison).

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143, non-`_UNICODE`
**Primary Dependencies**: pure WinAPI + WIC (`windowscodecs`, already linked by pictview since feature 006); no new external dependencies
**Storage**: none new — in-memory `CIconCache`/`ThumbnailsCache` only; no registry changes (`Configuration.ThumbnailSize` untouched)
**Testing**: `saltests` (unchanged scope); manual measured scenarios on generated fixtures (quickstart.md); no automated UI harness exists for the panel
**Target Platform**: Windows 11+
**Project Type**: desktop application — core panel reader (`salamand.vcxproj`) + pictview plugin (`pictview.spl`)
**Performance Goals**: SC-001 first visible thumbnail ≤ 2 s at ≥5,000 photos / ≥20 GB; SC-002 viewport repopulation starts ≤ 1 s after a jump; per-thumbnail fast-round cost target ~1–150 ms (EXIF vs reduced decode)
**Constraints**: plugin ABI frozen (`LoadThumbnail`, `SSTHUMB_*`, maker interface used as published — no `LAST_VERSION_OF_SALAMANDER` bump); single reader thread per panel preserved (no new threads); thumbnail appearance unchanged except the deliberate EXIF-rotation fix; no new unbounded memory (reduced decode shrinks transient DIBs 96 MB → ≤1.5 MB)
**Scale/Scope**: 2 subsystems — core reader scheduling (`src/fileswn1.cpp`, `src/fileswna.cpp`, `src/filesbx1.cpp`) and pictview decode path (`src/plugins/pictview/thumbs.cpp`, `wicengine.cpp/.h`); ~6 files touched

## Constitution Check

*GATE: evaluated against Tandem Commander Constitution v3.1.0 — PASS (initial
and re-checked post-design; Complexity Tracking empty).*

- **I. Build Reproducibility** — PASS. No build-system changes.
- **II. Backward Compatibility** — PASS. Performance fix; visible behavior
  changes are (a) thumbnails appear sooner/in priority order, (b) EXIF-rotated
  photos now display correctly (deliberate defect fix, CHANGELOG'd), (c)
  preview-then-quality progression (existing protocol, newly exercised).
  Plugin ABI untouched — all flags and interfaces already published
  (`spl_thum.h`); third-party thumbnail loaders keep working unchanged.
  Icon/overlay results unchanged, only ordered later relative to thumbnails.
- **III. Incremental Modernization** — PASS. A and B are independently
  landable increments inside existing functions; variant C was explicitly
  rejected for this feature because it would rewrite the reader's threading
  choreography (this principle drove the recommendation).
- **IV. Windows Platform Commitment** — PASS. WIC is the platform imaging
  stack; already a dependency.
- **V. Plugin Architecture Preservation** — PASS. The fix *activates* the
  published two-round thumbnail protocol instead of bypassing the plugin
  system; pictview changes are internal.
- **VI. UI Consistency** — PASS. No dialog/control changes.
- **Release Documentation** — CHANGELOG entries required: faster thumbnails
  (user terms), EXIF rotation fix.

## Project Structure

### Documentation (this feature)

```text
specs/064-speedup-thumbnails/
├── spec.md
├── investigation-leads.md    # pre-plan pointers (superseded by research.md)
├── plan.md                   # this file
├── research.md               # RC1–RC4 + variant comparison + user decision A+B
├── data-model.md             # cache/phase states & transitions to preserve
├── quickstart.md             # fixture + measured validation scenarios
├── contracts/
│   └── thumbnail-scheduling-and-fastpath.md   # binding rules C1–C6
├── checklists/requirements.md
└── tasks.md                  # /speckit-tasks output (not created here)
```

### Source Code (repository root)

```text
src/
├── fileswn1.cpp             # A: phase machine reorder (wanted/selectMode), CS release
│                            #    around LoadThumbnail + revalidation, repaint-index scan
├── fileswna.cpp             # A: CVisibleItemsArray — synchronous refresh entry
├── filesbx1.cpp             # A: scroll/geometry sites call immediate RefreshArr
├── fileswn2.cpp             # (quick win, if validated) Alt+5 switch carry-over
└── plugins/pictview/
    ├── thumbs.cpp           # B: fast/quality ladder, ONLY_PREVIEW, orientation flags,
    │                        #    Thumbs.db demoted to fallback
    └── wicengine.cpp/.h     # B: GetThumbnail/IWICBitmapSourceTransform decode paths,
                             #    skip composite/BuildLines for thumbnail output
```

**Structure Decision**: existing app + existing plugin; no new projects, no
new dependencies, no resource/translation changes (no new UI strings).

## Phase 0 — research.md (complete)

All unknowns resolved with file:line evidence; variant comparison presented;
**user selected A+B** (recorded in research.md).

## Phase 1 — Design artifacts (complete)

- [contracts/thumbnail-scheduling-and-fastpath.md](contracts/thumbnail-scheduling-and-fastpath.md) — C1 phase order, C2 scroll freshness, C3 lock/cancel discipline, C4 fast/quality ladder, C5 EXIF orientation, C6 non-goals.
- [data-model.md](data-model.md) — flag/state transitions and invariants.
- [quickstart.md](quickstart.md) — fixtures (EXIF/plain/small) + scenarios mapping to SC-001…SC-005.

## Implementation order (input to /speckit-tasks)

1. **Fixtures & baseline**: fixture generator script; stopwatch baseline of
   current behavior (big folder + small-folder control) for SC-001/SC-004.
2. **B first, standalone win**: WIC fast path in pictview (ladder + skip
   waste + orientation), behind the existing `fastThumbnail` parameter —
   measurable immediately even before A.
3. **A scheduling**: phase-machine reorder (C1); synchronous visible-array
   refresh on scroll (C2); `ICSleepSection` release + revalidation (C3).
4. **Quick wins** (each separately revertible): Thumbs.db demotion (part of
   B); repaint-index scan; Alt+5 carry-over (validate feasibility, else
   defer with a note).
5. **Verification**: quickstart scenarios 1–6, gates, CHANGELOG.

## Complexity Tracking

No constitution violations — table intentionally empty.

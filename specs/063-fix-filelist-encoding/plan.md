# Implementation Plan: Make File List — Correct Encoding and Dialog Layout

**Branch**: `063-fix-filelist-encoding` | **Date**: 2026-08-19 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/063-fix-filelist-encoding/spec.md`

## Summary

Make File List (Ctrl+M) corrupts every non-ASCII name on its clipboard leg: the
list is expanded as UTF-8 (feature-004 contract) but pushed through the ANSI
clipboard helper, which decodes it as CP_ACP (`AddUnicodeToClipboard`,
`src/salamdr4.cpp:1027`). The fix adds a UTF-8 clipboard entry point delegating to
the already-correct wide path and sweeps the eleven confirmed same-defect callers
onto it (research D1). The dialog's garbled line-syntax hint is fixed at the
mechanism level — tooltip text becomes UTF-8-by-contract at intake with a tolerant
renderer that always draws wide (D2). The clipped "Soubor" label is a
master-template + layout-tooling defect affecting all 11 non-English languages;
fixed in `lang.rc` + `translate.layout`, propagated by `relayout` (D7). Three
adjacent real defects on the same feature path are fixed with it: ANSI
`CreateFile`/`DeleteFile` garble a non-ASCII list-file name (D4), an ANSI
`GetTempPath` inside `SalGetTempFileName` breaks Ctrl+M entirely under a non-ASCII
`%TEMP%` (D5), and `:N`/`:max` widths count bytes — misaligning columns and even
splitting UTF-8 sequences on truncation (D6). The viewer leg needs only a UTF-8
BOM on the temp file to make its existing detection unconditional (D3).

All decisions with evidence: [research.md](research.md). Binding encoding rules:
[contracts/filelist-text-encoding.md](contracts/filelist-text-encoding.md).

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022), non-`_UNICODE` build
**Primary Dependencies**: pure WinAPI; house Unicode layer `src/common/salunicode.*`, `src/common/salfileio.*`; no new external dependencies
**Storage**: Windows Registry (existing `Configuration.FileList*` values — unchanged, no migration)
**Testing**: `saltests` (existing suite); `tools/check_encoding.py` contract guard (runs in `build.cmd`); `translate.relayout --dry-run` + `build_langs.cmd --check-layout` for dialog geometry; manual verification per [quickstart.md](quickstart.md)
**Target Platform**: Windows 11+
**Project Type**: desktop application (main app `salamand.vcxproj` → `tandemcommander.exe`) + developer-side Python layout tooling (`tools/translate/`)
**Performance Goals**: none new — expansion stays single-pass per item; ASCII fast path (`SalIsASCII`) keeps the hot path allocation-free
**Constraints**: plugin ABI frozen (interface version 105) — no signature changes to `CopyTextToClipboard`, `CSalamanderPluginInternalViewerData`, or `spl_gui.h` virtuals; ASCII output byte-identical (FR-008); no settings migration (MINORB rule)
**Scale/Scope**: ~8 core source files + `tooltip.cpp`/`gui.cpp` mechanism edits + 15 mechanical `LoadStrU8` conversions + 1 master dialog template + 2 `tools/translate/layout.py` fixes + regenerated geometry rows in 12 `salamand.slt` files

## Constitution Check

*GATE: evaluated against Tandem Commander Constitution v3.1.0 — PASS (pre-research
and re-checked post-design; no violations, Complexity Tracking empty).*

- **I. Build Reproducibility** — PASS. No build-system changes; `relayout` is
  deterministic and offline; geometry regeneration is committed, not build-time.
- **II. Backward Compatibility** — PASS. Bug fix restoring intended behavior;
  ASCII output guarded byte-identical (FR-008/SC-004). Plugin ABI untouched: the
  ANSI `CopyTextToClipboard` keeps CP_ACP semantics (new U8 entry point instead),
  tooltip intake normalization is invisible to plugin callers, no
  `LAST_VERSION_OF_SALAMANDER` bump. No config migration (registry values reused
  as-is). CHANGELOG entry required when this ships in a release (Fixed section).
- **III. Incremental Modernization** — PASS. Each fix is a small, independently
  revertible change at the defect site or its mechanism; the 11-caller clipboard
  sweep is one defect class through one new entry point (precedent: feature 052's
  15-site conversion). No opportunistic refactoring of adjacent code.
- **IV. Windows Platform Commitment** — PASS. Pure WinAPI (`CreateFileW` family
  via existing house wrappers).
- **V. Plugin Architecture Preservation** — PASS. Published plugin GUI/clipboard
  APIs keep their contracts; plugin-supplied ANSI hints keep working via intake
  normalization (tolerance model, not strictness).
- **VI. UI Consistency** — PASS. `IDD_FILELIST` stays `DIALOGEX` + `DS_SHELLFONT`
  + `MS Shell Dlg`; geometry-only template change; no control restyling, no
  process-wide visual behavior touched.
- **Release Documentation** — noted: user-facing fixes (garbled Ctrl+M list,
  garbled hints, clipped labels, non-ASCII list-file name, non-ASCII `%TEMP%`)
  must appear in `CHANGELOG.md` under the next released version.

## Project Structure

### Documentation (this feature)

```text
specs/063-fix-filelist-encoding/
├── spec.md                  # feature specification
├── investigation-leads.md   # pre-plan exploration (superseded by research.md)
├── plan.md                  # this file
├── research.md              # Phase 0 — decisions D1–D7 with evidence
├── data-model.md            # Phase 1 — entities & encoding contracts
├── quickstart.md            # Phase 1 — validation guide
├── contracts/
│   └── filelist-text-encoding.md   # binding encoding contract C1–C5
├── checklists/requirements.md
└── tasks.md                 # Phase 2 (/speckit-tasks — not created here)
```

### Source Code (repository root)

```text
src/
├── salamdr4.cpp         # D1: new CopyTextToClipboardU8 (delegates to CopyHTextToClipboardW)
├── consts.h             # D1: declaration next to CopyTextToClipboard/W
├── mainwnd4.cpp         # D1 caller #1 · D3 viewer BOM · D4 SalCreateFile/SalDeleteFile
├── fileswn9.cpp         # D1 callers #2–#5 (Ctrl+C name/path/UNC copies)
├── fileswn1.cpp         # D1 caller #6 (save selection → clipboard)
├── finddlg1.cpp         # D1 caller #7 (Find window copies)
├── mainwnd1.cpp         # D1 caller #8 (dir/status line Copy)
├── msgbox.cpp           # D1 caller #10 (Ctrl+C copy; wide label reads)
├── viewer3.cpp          # D1 caller #11 (viewer Copy honors VCE_UTF8)
├── gui.cpp              # D1 caller #9 (TextW) · D2 SetToolTipText intake, SetText fallback, boundary clamp
├── tooltip.cpp          # D2 tolerant GetText + unconditional DrawTextW
├── dialogs*.cpp, dialogsp.cpp  # D2: 15× SetActionShowHint(LoadStr→LoadStrU8)
├── salamdr2.cpp         # D6: DoExpandVarString char-based width/pad/boundary-safe cut
├── salamdr3.cpp         # D5: SalGetTempFileName wide temp/system dir
├── fileswn0.cpp         # D6: statics promoted out
├── common/salunicode.h/.cpp    # D6: SalU8CharCount / SalU8Next (promoted)
└── lang/lang.rc         # D7: IDD_FILELIST geometry (radio 27→40, edit/checkbox shift)

tools/
├── translate/layout.py  # D7: dropdown blocker clamp + radio/checkbox glyph allowance
└── check_encoding.py    # C2/C3 contract identifiers

translations/*/salamand.slt   # D7: regenerated geometry rows (12 languages, text untouched)
```

**Structure Decision**: single existing desktop-app project; all changes land in
the main app sources, the shared `src/common` Unicode layer, and the committed
translation/layout tooling. No new projects, no new dependencies.

## Phase 0 — research.md (complete)

No NEEDS CLARIFICATION remained after research; all seven decision areas (D1–D7)
are resolved with file:line evidence and rejected alternatives recorded.

## Phase 1 — Design artifacts (complete)

- [data-model.md](data-model.md) — entities and their encoding contracts.
- [contracts/filelist-text-encoding.md](contracts/filelist-text-encoding.md) —
  binding rules C1–C5 + enforcement (check_encoding.py, --check-layout).
- [quickstart.md](quickstart.md) — build + manual validation scenarios mapping to
  SC-001…SC-005, including the ASCII byte-diff regression guard.

## Implementation order (input to /speckit-tasks)

1. **Foundations**: `SalU8CharCount`/`SalU8Next` promotion (D6.1); `CopyTextToClipboardU8` (D1).
2. **Primary defect**: route `mainwnd4.cpp:315` through U8 copy; D5 temp-path fix — Ctrl+M → clipboard works (User Story 1).
3. **Destinations**: D3 viewer BOM; D4 file-leg `SalCreateFile`/`SalDeleteFile` (User Story 2).
4. **Width semantics**: D6 char-based measuring/padding + boundary-safe cut (FR-005).
5. **Hint mechanism**: D2 intake normalization + tolerant renderer + `LoadStrU8` conversions + label fallback (User Story 3).
6. **Layout**: D7 layout.py fixes → master template → relayout sweep → check-layout (User Story 4).
7. **Sweep**: remaining D1 callers (#2–#11) onto U8/W entry points.
8. **Enforcement & verification**: check_encoding.py identifiers; quickstart validation; CHANGELOG note.

## Complexity Tracking

No constitution violations — table intentionally empty.

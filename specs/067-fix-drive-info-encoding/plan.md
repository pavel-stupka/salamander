# Implementation Plan: Fix Garbled Numbers in Drive Information Dialog

**Branch**: `067-fix-drive-info-encoding` | **Date**: 2026-08-24 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/067-fix-drive-info-encoding/spec.md`

## Summary

Byte counts in the Ctrl+F1 Drive Information dialog (and the two size-results
dialogs plus one message box) render `Â ` at every digit group in Czech
because `PrintDiskSize` composes an ANSI `LoadStr` plural template with a
UTF-8 `NumberToStr` number — the mixed string fails the strict UTF-8→wide
conversion in the `SalSetDlgItemTextU8` sink family and falls back to the
ANSI draw path. Fix: extend the established feature-041 pattern to
`PrintDiskSize` — a `BOOL u8 = FALSE` opt-in that switches every internal
`LoadStr` to `LoadStrU8`, passed as `TRUE` at the eight garbled core dialog
statements; convert the one missed `IDS_NOTENOUGHSPACE` site to `LoadStrU8`
(twin of the already-fixed fileswn6/8 sites); convert the viewer offset
tooltip (the only core ANSI sink still receiving a grouped number) to the
wide tooltip notification. The plugin API boundary keeps byte-identical
output (forwarders never pass `u8=TRUE`), which the plugin audit proved is
required to avoid regressing dbviewer/ftp ANSI sinks. For all shipped
languages the unit abbreviations are ASCII, so the change is byte-identical
everywhere except the defective plural-bytes composition — the no-regression
property the user demanded, by construction.

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022)
**Primary Dependencies**: Pure WinAPI; internal shared libs (`src/common`); no new dependencies
**Storage**: none touched (display-only fix; no registry/config change)
**Testing**: `saltests.exe` (hand-rolled CHECK suite, Debug-only) + `tools/check_encoding.py --strict` build gate + manual quickstart scenarios
**Target Platform**: Windows 11+
**Project Type**: desktop app (single solution, `salamand.sln`)
**Performance Goals**: n/a (identical code paths; one extra UTF-16→UTF-8 string load per dialog refresh)
**Constraints**: MUST NOT regress any currently-correct surface (user directive); plugin API bytes frozen (no `LAST_VERSION_OF_SALAMANDER` bump, stays 106); English/ASCII output byte-identical
**Scale/Scope**: 5 source files + 1 header + `check_encoding.py` + `saltests.cpp`; ~8 call statements changed, 1 parameter added, 1 tooltip converted

## Constitution Check

*GATE: evaluated against Constitution v3.1.0 — PASS (pre- and post-design).*

| Principle | Verdict | Note |
|---|---|---|
| I. Build Reproducibility | PASS | no build-system change; encoding gate already in `build.cmd` |
| II. Backward Compatibility | PASS | display-only; plugin ABI untouched and byte-frozen at the API boundary (audit-verified); no config migration; no interface version bump |
| III. Incremental Modernization | PASS | minimal diff — only defective sites change; adjacent latent items (split-bar tooltip, plugin-internal sinks) recorded, not refactored |
| IV. Windows Platform Commitment | PASS | pure WinAPI (`TTM_ADDTOOLW`, `LoadStringW` paths already in-tree) |
| V. Plugin Architecture Preservation | PASS | `spl_gen.h` gains documentation only; behavior at the boundary frozen; follow-up path for a U8-capable API recorded |
| VI. UI Consistency | PASS | no dialog/control changes; same controls, correct text |
| Release Documentation | PASS (deferred to release) | changelog entry drafted with the fix; version bump only when a release is cut |

No violations → Complexity Tracking not needed.

## Project Structure

### Documentation (this feature)

```text
specs/067-fix-drive-info-encoding/
├── spec.md                  # feature specification (+ evidence screenshot)
├── plan.md                  # this file
├── research.md              # Phase 0: root cause, 3 audits, decisions D1-D4
├── data-model.md            # Phase 1: encoding states, sink classes
├── quickstart.md            # Phase 1: validation guide
├── contracts/
│   └── number-format-encoding.md   # binding encoding contract
├── checklists/requirements.md
└── informace_o_jednotce.png # defect evidence
```

### Source Code (repository root)

```text
src/
├── consts.h            # PrintDiskSize declaration: + BOOL u8 = FALSE (with 041-style doc
│                       #   comment); correct the stale 041 comment at :871-876
├── salamdr6.cpp        # PrintDiskSize definition: u8 selects LoadStrU8 for
│                       #   IDS_PLURAL_X_BYTES + IDS_SIZE_B..EB/KB (all modes)
├── dialogs3.cpp        # :1537-1546 six PrintDiskSize calls → u8=TRUE (Ctrl+F1);
│                       #   :2216 → u8=TRUE (archive size results)
├── dialogs2.cpp        # :412, :452, :475, :478 → u8=TRUE (dir-sizes/occupied space)
├── zip.cpp             # :6566 LoadStr(IDS_NOTENOUGHSPACE) → LoadStrU8
│                       #   (CSalamanderGeneral forwarders NOT touched)
├── viewer3.cpp         # ~:560 TOOLINFOW/TTM_ADDTOOLW registration;
│                       #   ~:3117 TTN_NEEDTEXTW handler, LoadStrU8 + SalU8ToW
├── saltests/saltests.cpp             # + TestNumberCompositionEncoding()
└── plugins/shared/spl_gen.h          # doc comments only: encoding statements

tools/check_encoding.py               # new/extended rule: ANSI LoadStr template
                                      #   composed with UTF-8 number producers
```

**Structure Decision**: single-solution desktop app; all changes are in the
existing main-app project (`salamand.vcxproj`) plus the test exe and the
python gate. No new files, projects, or resources.

## Phase 0: Research — complete

See [research.md](research.md). Method: three parallel audits (51 core sites
classified; every enabled plugin's ~50 call sites classified with sink types;
test/link feasibility established). Key decisions:

- **D1**: `u8` opt-in parameter on `PrintDiskSize` (feature-041 pattern);
  rejected always-UTF-8 (would regress 5 dbviewer sites + FTP log — audit B),
  dialog-only fix (fails FR-003), ANSI-separator revert (re-breaks 041/043).
- **D2**: companion fixes — `zip.cpp:6566` `LoadStrU8` twin; viewer offset
  tooltip → wide notification.
- **D3**: plugin API boundary byte-frozen; doc comments only; stale comments
  corrected; disabled-language latency and split-bar tooltip recorded, untouched.
- **D4**: verification = saltests property test + `check_encoding.py` rule +
  quickstart manual matrix (fixed surfaces + 10-point regression sweep).

## Phase 1: Design & Contracts — complete

- [data-model.md](data-model.md): `FormattedNumberString` encoding states
  (U8/ANSI/MIXED), sink classification, language-exposure table with the
  byte-identity validation rule.
- [contracts/number-format-encoding.md](contracts/number-format-encoding.md):
  producer encodings incl. the new `u8=TRUE` guarantee, the composition
  invariant, the frozen plugin boundary, buffer-size proof, enforcement.
- [quickstart.md](quickstart.md): build/gates, 5 fixed-surface scenarios,
  10-point regression sweep incl. English byte-identity and plugin freeze.

**Post-design constitution re-check**: PASS (table above reflects the final design).

## Implementation notes for /speckit-tasks (Phase 2 input)

1. Order: producers first (`consts.h` + `salamdr6.cpp`), then call sites,
   then viewer tooltip, then tests/gate, then docs/comments.
2. The `u8=TRUE` conversion must cover **all six** `PrintDiskSize` calls in
   `dialogs3.cpp:1537-1546` (mode-0 shorts included — byte-identical today,
   correct-by-construction for any future language) and the four in
   `dialogs2.cpp`; nothing else changes callers.
3. Viewer tooltip: keep the 80-char `szText` limit in mind
   (`_snwprintf_s`, `_TRUNCATE`); longest realistic string ≈ 61 chars.
   Register with `TOOLINFOW` and handle `TTN_NEEDTEXTW`; the notification
   code follows the registration variant.
4. `check_encoding.py` rule must pass `--strict` over the whole tree —
   tune the pattern to the actual composition idioms (ExpandPluralString
   template arg; sprintf of a producers' buffer into a `LoadStr` format)
   and add matching entries to the tracked-identifier docs per feature-052
   practice.
5. Do NOT touch: `CSalamanderGeneral` forwarders, panel/Find/status/menu/
   progress surfaces (regression guard list in research.md R2), plugin code,
   `mainwnd3.cpp` tooltip, disabled-language data.
6. clang-format the touched files; comments in English.

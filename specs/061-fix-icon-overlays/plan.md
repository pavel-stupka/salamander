# Implementation Plan: Restore General Shell Icon Overlay Support

**Branch**: `061-fix-icon-overlays` | **Date**: 2026-08-19 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/061-fix-icon-overlays/spec.md`

## Summary

Third-party icon overlay badges (canonical case: TortoiseGit) do not appear in panels,
although the OneDrive/Google Drive badges fixed in features 058–059 work. Phase 0
research verified two code defects and disproved two environmental suspects, but also
proved the primary symptom is **not yet root-caused**: on the affected machine, four
healthy Tortoise handlers should be loaded (TortoiseOverlays' own 12-handler cascade
sacrifices only Locked/Ignored/ReadOnly/Unversioned) and still no badge shows on ASCII
paths. The implementation therefore runs in three phases: **A** — instrumented runtime
analysis producing `analysis-report.md` (FR-001/SC-004, every suspect confirmed or
refuted); **B** — fixes: the verified defects D1 (ANSI shell-notification path vs
UTF-8 panel path kills the designed Tortoise re-ask loop on non-ASCII paths) and D2
(absent config values force overlays off — hits feature-057-migrated profiles;
FR-009), plus whatever D3 blocker Phase A pinpoints, plus debug-only slot-table
diagnostics (D4); **C** — verification per `quickstart.md` (Explorer-as-floor parity,
058/059 regression guard, config healing, gates). No plugin-ABI or configuration-
layout change.

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022)
**Primary Dependencies**: Pure WinAPI — shell overlay interfaces
(`IShellIconOverlayIdentifier`), `SHChangeNotifyRegister` pipeline, house encoding
helpers `SalU8ToW`/`SalWToU8` (`src/common/salunicode.cpp`); no new external deps
**Storage**: Windows Registry `HKCU\Software\Tandem Commander\0.1\Configuration`
(`Enable Custom Icon Overlays`, `Disabled Custom Icon Overlays`) — location/format
unchanged; HKLM `ShellIconOverlayIdentifiers` read-only enumeration
**Testing**: existing saltests suite; manual validation per `quickstart.md` (Debug
`TRACE_ENABLE`/`TRACE_TO_FILE` instrumented run); full Debug+Release build gates
**Target Platform**: Windows 11+ x64 (pure WinAPI desktop app)
**Project Type**: Desktop application (two-panel file manager), single solution
**Performance Goals**: no regression of icon-reader throughput; overlay pass stays
once-per-cycle; notification coalescing (200 ms) unchanged
**Constraints**: plugin ABI frozen (`CFileData::IconOverlayIndex : 4`,
`ICONOVERLAYINDEX_NOTUSED = 15` in `src/plugins/shared/spl_com.h`); MINORB release must
not move configuration; no Release-visible diagnostics (spec clarification); no new
translated strings
**Scale/Scope**: ~5 source files (`src/shiconov.cpp/.h`, `src/mainwnd3.cpp`,
`src/mainwnd2.cpp`, possibly `src/fileswn1.cpp`/`src/fileswn7.cpp`), 22 registered
handlers on the reference machine, 15-slot table

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment | Status |
|---|---|---|
| I. Build Reproducibility | No build-system changes; optional `TRACE_TO_FILE` is a local Debug convenience, not a pipeline step | PASS |
| II. Backward Compatibility | Baseline behavior restored, not changed: overlays are a TC 0.1.0 capability that regressed. Config location/format untouched; FR-009 heals only *absent* values, stored choices respected; plugin ABI (interface 105, `CFileData` layout) byte-identical | PASS |
| III. Incremental Modernization | Narrow fixes at verified defect sites using the established house pattern (`SalU8ToW`/`SalWToU8`, same class as feature 058); no adjacent refactoring; analysis-first ordering prevents speculative edits | PASS |
| IV. Windows Platform Commitment | Pure WinAPI (wide shell APIs + local conversion); no new dependencies | PASS |
| V. Plugin Architecture Preservation | The 4-bit overlay index contract documented in `contracts/overlay-pipeline-contract.md` **before** modification of the surrounding code; the field itself is not modified | PASS |
| VI. UI Consistency | No new dialogs/controls; config page unchanged; no Release-visible UI additions (per clarification) | PASS |

**Post-Phase-1 re-check**: design artifacts introduce no new violations — contracts C1–C5
codify existing invariants plus the two behavior changes (C3 notification encoding,
C4 absent-value semantics), both constitution-compatible as assessed above. PASS.

## Project Structure

### Documentation (this feature)

```text
specs/061-fix-icon-overlays/
├── plan.md                  # This file
├── spec.md                  # Feature specification (with Clarifications)
├── investigation-leads.md   # Pre-plan code exploration (S1–S7 suspects)
├── research.md              # Phase 0: evidence, decisions R0–R7, defect register D1–D5
├── data-model.md            # Phase 1: entities E1–E5 and invariants
├── quickstart.md            # Phase 1: validation guide V1–V7
├── contracts/
│   └── overlay-pipeline-contract.md   # Phase 1: binding pipeline contract C1–C5
├── analysis-report.md       # Phase A output (created during implementation; FR-001/SC-004)
└── tasks.md                 # Phase 2 output (/speckit-tasks — NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
src/
├── shiconov.h               # E1/E2: overlay item + slot table classes
├── shiconov.cpp             # Handler load ladder, slot table, per-item query,
│                            #   D4 slot-table trace, D5 icon-path encoding (if confirmed)
├── mainwnd3.cpp             # D1: shell change notification handler (SHCNE_UPDATEITEM
│                            #   path → wide API + SalWToU8; audit sibling consumers)
├── mainwnd2.cpp             # D2: LoadIconOvrlsInfo absent-value semantics (FR-009)
├── fileswn1.cpp             # Icon-reader overlay pass (Phase A instrumentation only,
│                            #   unless the D3 blocker lands here)
├── fileswn7.cpp             # IconOverlaysChangedOnPath (gate audit; likely unchanged)
└── common/
    └── salunicode.cpp       # House conversion helpers (used, not modified)
```

**Structure Decision**: single existing solution (`src/vcxproj/salamand.sln`); all
changes are point fixes inside the main application project. No new projects, files
only if the D3 fix demands one (not expected).

## Implementation Phases (input for /speckit-tasks)

- **Phase A — Instrumented analysis (blocks everything else)**: Debug build with
  traces; execute checklist A1–A7 from `research.md` R2 on the reference machine;
  write `analysis-report.md` giving every suspect/defect (S1–S7 legacy, D1–D5 current,
  A-items) a CONFIRMED/REFUTED verdict with evidence. Deliverable of FR-001, measured
  by SC-004.
- **Phase B — Fixes (each independently revertible, per constitution III)**:
  B1 = D1 notification encoding (contract C3); B2 = D2 config healing (contract C4);
  B3 = D3 fix as designed by the analysis report; B4 = D4 slot-table trace (contract
  C5); B5 = D5 icon-path load encoding if Phase A confirms a real-world impact.
- **Phase C — Verification**: quickstart V1–V7; SC-001…SC-007; CHANGELOG entry and
  version/build bump land with the release, per the constitution's Release
  Documentation rules.

## Complexity Tracking

No constitution violations — table not applicable.

# Implementation Plan: Consistent Delete to Recycle Bin

**Branch**: `062-fix-delete-to-recycle` | **Date**: 2026-08-19 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/062-fix-delete-to-recycle/spec.md`

## Summary

DEL silently escalates to a permanent delete (with a confirmation popup, like
SHIFT+DEL) in some folders — reproducibly in the user's OneDrive tree. Phase 0
research verified the cause at code level: the recycle decision's only gate,
`MyGetDriveType(GetPath()) != DRIVE_FIXED`, feeds the UTF-8 panel path (feature 004)
to ANSI `GetDriveTypeA`; on non-ASCII paths (the OneDrive tree has Czech folder
names) the mojibake path classifies as `DRIVE_NO_ROOT_DIR` → Recycle Bin vetoed →
direct-delete branch with its popup. The trigger is path spelling, not OneDrive —
one runtime assumption (the exact `DRIVE_NO_ROOT_DIR` return) is confirmed first in
Phase A. Implementation: **A** — instrumented confirmation on the unfixed build;
**B** — fixes per the clarified scope: the whole classification chain goes wide and
long-path capable (E1/E4), the gate becomes fail-safe toward the Recycle Bin (E5,
FR-005), cloud-placeholder items stop being classified as links via the
name-surrogate tag test (E2), the worker's per-item recycle call goes wide via a
shared helper (E3); **C** — the SC-001…SC-004 matrix over four canonical locations,
run on the instrumented build and re-run on the final build (the user's explicit
"verify repeatedly"), plus Copy/Move and feature-061 regression smokes. No plugin-ABI
or configuration change.

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022)
**Primary Dependencies**: Pure WinAPI — `GetDriveTypeW`, `SHFileOperationW`
(`FOF_ALLOWUNDO`), reparse-point plumbing (`FSCTL_GET_REPARSE_POINT`,
`IsReparseTagNameSurrogate`), house encoding helpers `SalU8ToW`/`SalWToU8`
(`src/common/salunicode.cpp`); no new external dependencies
**Storage**: Windows Registry `HKCU\Software\Tandem Commander\0.1\Configuration`
(`Use Recycle Bin`, `Use Recycle Bin For`, `Confirm File Dir Del`) — read-only for
this feature, location/format unchanged
**Testing**: existing saltests suite; scripted validation per `quickstart.md`
(shell Recycle Bin enumeration as ground truth; Debug `TRACE_ENABLE`/`TRACE_TO_FILE`
instrumented runs; final-build matrix re-run)
**Target Platform**: Windows 11+ x64 (pure WinAPI desktop app)
**Project Type**: Desktop application (two-panel file manager), single solution
**Performance Goals**: no measurable change — the classification adds only UTF-8→W
conversions on a per-operation (not per-item) path; per-item recycle path already
does per-item shell calls
**Constraints**: plugin ABI untouched; MINORB release must not move configuration;
no Release-visible diagnostics (FR-009); data-safety bias mandated by clarification
(fail toward the Recycle Bin)
**Scale/Scope**: ~4 source files (`src/salamdr2.cpp` — classification chain,
`src/fileswn8.cpp` — gate + shared recycle helper, `src/fileswn6.cpp` — item-nature
classification, `src/worker.cpp` — per-item recycle route); 4 canonical test
locations; 3 configured modes × 2 gestures

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment | Status |
|---|---|---|
| I. Build Reproducibility | No build-system changes; `TRACE_TO_FILE` is a local Debug convenience (feature-061 precedent), reverted before commit | PASS |
| II. Backward Compatibility | Restores the TC 0.1.0 baseline behavior (DEL = recycle everywhere the platform allows); config storage untouched; the one deliberate behavior change (fail-safe gate for indeterminate classifications) is spec-clarified, documented, and strictly safety-increasing | PASS |
| III. Incremental Modernization | Point fixes at verified defect sites using the established feature-058 house pattern; the chain conversion is the clarified minimal cohesive unit; adjacent ANSI sites outside the chain explicitly deferred (research.md E6) | PASS |
| IV. Windows Platform Commitment | Pure WinAPI (wide variants + local conversion); no new dependencies | PASS |
| V. Plugin Architecture Preservation | No plugin interface touched; `CFileData` layout unchanged; the classification chain's `char*`/UTF-8 signatures preserved for all consumers | PASS |
| VI. UI Consistency | No new dialogs/controls/strings; existing confirmation dialogs unchanged (FR-004 restores their consistency by fixing the decision, not the dialogs) | PASS |

**Post-Phase-1 re-check**: design artifacts (contract C1–C5, data model E1–E5)
introduce no violations; the two behavior changes (C2 fail-safe, C3 surrogate-tag
link test) are spec-clarified. PASS.

## Project Structure

### Documentation (this feature)

```text
specs/062-fix-delete-to-recycle/
├── plan.md                  # This file
├── spec.md                  # Feature specification (with Clarifications)
├── investigation-leads.md   # Pre-plan exploration (pipeline map, defects, test matrix)
├── research.md              # Phase 0: verified facts, decisions R0–R6, defect register E1–E6
├── data-model.md            # Phase 1: entities E1–E5 and invariants
├── quickstart.md            # Phase 1: validation guide V1–V7 (four canonical locations)
├── contracts/
│   └── delete-pipeline-contract.md   # Phase 1: binding contract C1–C5
├── analysis-report.md       # Phase A/C output (created during implementation; FR-001/SC-005)
└── tasks.md                 # Phase 2 output (/speckit-tasks)
```

### Source Code (repository root)

```text
src/
├── salamdr2.cpp             # E1/E4 fix: MyGetDriveType + reparse chain → wide,
│                            #   long-path capable (R1); signatures unchanged
├── fileswn8.cpp             # E5 fix: fail-safe recycle gate (R2) + gate TRACE (R5);
│                            #   shared wide recycle-list helper factored from
│                            #   DeleteThroughRecycleBinAuxW (R4)
├── fileswn6.cpp             # E2 fix: link-vs-placeholder classification by
│                            #   name-surrogate reparse tag (R3)
├── worker.cpp               # E3 fix: per-item recycle route → shared wide helper (R4)
└── common/
    └── salunicode.cpp       # House conversion helpers (used, not modified)
```

**Structure Decision**: single existing solution (`src/vcxproj/salamand.sln`); all
changes are point fixes in the main application project. No new files expected beyond
possibly a small shared helper declaration in an existing header.

## Implementation Phases (input for /speckit-tasks)

- **Phase A — Instrumented confirmation (blocks fixes)**: gate TRACE (R5, permanent,
  Debug-only) + unfixed-build run over the four locations to confirm the
  `DRIVE_NO_ROOT_DIR` assumption (quickstart V1.1); `analysis-report.md` opened with
  the verdict table. Small: the root cause is already code-verified; this phase
  guards against fixing the wrong rung (constitution III, user's "verify repeatedly").
- **Phase B — Fixes (independently revertible)**: B1 = classification chain wide +
  long-path (R1, `salamdr2.cpp`); B2 = fail-safe gate (R2, `fileswn8.cpp`); B3 =
  surrogate-tag item nature (R3, `fileswn6.cpp` + wording site `fileswn8.cpp`);
  B4 = shared wide recycle helper + worker route (R4, `fileswn8.cpp`/`worker.cpp`).
- **Phase C — Verification**: quickstart V2–V7 on the instrumented build; remove
  temporary instrumentation; final-build matrix re-run (SC-005); CHANGELOG entry;
  gates (Debug+Release builds, saltests).

## Complexity Tracking

No constitution violations — table not applicable.

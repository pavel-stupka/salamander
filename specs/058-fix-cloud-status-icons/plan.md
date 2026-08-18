# Implementation Plan: Cloud Sync Status Icons in File Panels

**Branch**: `058-fix-cloud-status-icons` | **Date**: 2026-08-18 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/058-fix-cloud-status-icons/spec.md`

## Summary

All three reported symptoms — missing cloud sync-status badges, a fruitless
busy-cursor refresh on every window activation, and generic base icons for
Word/PDF files — share one root-cause family: **three call sites that
feature 004 (long paths / Unicode) left behind when panel paths became
UTF-8**. Each site still interprets the panel path as ANSI (`CP_ACP`), so any
path containing non-ASCII characters (`G:\Můj disk` — "ů") is garbled before
it reaches Windows:

1. **RC1 — missing badges**: the icon-reader thread builds the wide path
   prefix for overlay queries with `MultiByteToWideChar(CP_ACP, …)`
   (`src/fileswn1.cpp:496`); every `IShellIconOverlayIdentifier::IsMemberOf`
   call then asks about a nonexistent path and returns "no overlay".
2. **RC2 — busy cursor on focus**: the snooper passes the UTF-8 panel path to
   ANSI `FindFirstChangeNotification` (`src/snooper.cpp:578`, `:720`, `:750`);
   the call fails, `AutomaticRefresh` goes FALSE, and `CFilesWindow::Activate`
   (`src/fileswn6.cpp:82`) then re-checks and re-lists the directory on
   *every* app activation under the `IDC_WAIT` cursor set at
   `src/mainwnd3.cpp:5850` — the observed flash that "does nothing".
3. **RC3 — generic icons**: `SHILCreateFromPath` (`src/geticon.cpp:352`)
   converts the item's full path with `CP_ACP` before `ParseDisplayName`;
   the parse fails, `GetFileIcon` returns FALSE, and the panel falls back to
   the generic icon.

The fix converts these three sites to the established feature-004 house
pattern — **UTF-8 → UTF-16 via `SalU8ToW`/`SalU8ToWAlloc`, with a `CP_ACP`
fallback for invalid UTF-8** (the exact pattern already used five lines above
RC3 in `SalSHGetFileInfoIcons`, `src/geticon.cpp:66`) — and calls the W
variants of the affected APIs. No mechanism is redesigned; the original
Salamander icon/snooper architecture is restored to working order under the
UTF-8 path contract. Full analysis with evidence and rejected alternatives:
[research.md](research.md).

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022)
**Primary Dependencies**: Pure WinAPI (shell: `IShellIconOverlayIdentifier`,
`IShellFolder::ParseDisplayName`, `FindFirstChangeNotificationW`); internal
helpers `SalU8ToW`/`SalU8ToWAlloc` (`src/common/salunicode.*`); no new
external dependencies
**Storage**: N/A (no configuration or registry changes; FR-011 is satisfied
by the existing icon-overlay configuration, untouched)
**Testing**: full Debug+Release build; existing `saltests` suite
(`src/saltests/`, covers `SalU8ToW` helpers); manual validation per
[quickstart.md](quickstart.md) (provider-independent diacritic-folder repro +
real Google Drive / OneDrive parity checks)
**Target Platform**: Windows 11+ (x64)
**Project Type**: Desktop application (two-panel file manager)
**Performance Goals**: badges visible ≤ 2 s for on-screen items (SC-001);
no per-activation directory re-list on monitored paths (SC-002/SC-006);
listing performance unchanged (SC-003)
**Constraints**: incremental change only (constitution III) — three call
sites plus at most one small W overload in the HANDLES layer; plugin-facing
`GetFileIcon` keeps its `const char*` signature and legacy-ACP tolerance;
no plugin interface version bump
**Scale/Scope**: 3 root-cause sites in 3 files (`fileswn1.cpp`,
`snooper.cpp`, `geticon.cpp`), plus `handles.cpp/h` (W overload for
`FindFirstChangeNotification`) — roughly 4–5 files touched, tens of lines

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Verdict | Notes |
|---|-----------|---------|-------|
| I | Build Reproducibility | ✅ Pass | No build-system changes; no new artifacts. |
| II | Backward Compatibility | ✅ Pass | Restores 0.1.0-baseline behavior that regressed (badges, auto-refresh, icons worked in the upstream lineage); no registry/config moves; no identity changes; behavior change is a pure defect fix, not opt-in-worthy. |
| III | Incremental Modernization | ✅ Pass | Surgical conversion of exactly the three defective sites to the existing feature-004 house pattern; adjacent legacy code (e.g. outdated Google Drive handler-name list) deliberately left untouched — see research.md R6. |
| IV | Windows Platform Commitment | ✅ Pass | Pure WinAPI; W-variant APIs of the same calls. |
| V | Plugin Architecture Preservation | ✅ Pass | `CSalamanderGeneral::GetFileIcon` is plugin-facing: signature unchanged, and the UTF-8-first + CP_ACP-fallback conversion keeps legacy-ACP plugin callers working (same tolerance `SalSHGetFileInfoIcons` already has). `LAST_VERSION_OF_SALAMANDER` unchanged. |
| VI | UI Consistency | ✅ Pass | No dialogs, no controls, no visual-style changes. |
| — | Release Documentation | ⚠️ Deferred to release | User-visible fix → `CHANGELOG.md` entry (Fixed) required in the change that ships it; version/build bump only when releasing, per constitution. Tasks must include the changelog entry. |

**Post-Phase-1 re-check**: design artifacts introduce no new projects, no new
dependencies, no API-shape changes — all gates still pass.

## Project Structure

### Documentation (this feature)

```text
specs/058-fix-cloud-status-icons/
├── plan.md              # This file
├── research.md          # Phase 0: root-cause analysis, decisions
├── data-model.md        # Phase 1: pipeline state & encoding contract entities
├── quickstart.md        # Phase 1: build + validation guide (SC mapping)
├── contracts/
│   └── path-encoding-icon-pipeline.md   # Phase 1: internal encoding contract
└── tasks.md             # Phase 2 (/speckit-tasks — not created here)
```

### Source Code (repository root)

```text
src/
├── fileswn1.cpp         # RC1: icon-reader thread — wide path prefix built
│                        #   with CP_ACP (line ~496); wName offset assumes
│                        #   byte length == wide length (fix both)
├── snooper.cpp          # RC2: 3× ANSI FindFirstChangeNotification on UTF-8
│                        #   paths (AddDirectory ~578, ChangeDirectory ~720
│                        #   and ~750); convert at the boundary, preserve
│                        #   MakeCopyWithBackslashIfNeeded semantics
├── geticon.cpp          # RC3: SHILCreateFromPath (line ~352) CP_ACP →
│                        #   SalU8ToWAlloc + CP_ACP fallback (house pattern
│                        #   as in SalSHGetFileInfoIcons, same file)
├── common/
│   ├── handles.cpp/.h   # add FindFirstChangeNotificationW overload to the
│   │                    #   HANDLES tracking layer (mechanical)
│   └── salunicode.*     # existing helpers, unchanged (consumed)
├── shiconov.cpp/.h      # read-only in this feature (overlay registry,
│                        #   GD gating) — documented, not modified (R6)
└── saltests/
    └── saltests.cpp     # optional: regression test if a shared conversion
                         #   helper is extracted (decided in tasks)
```

**Structure Decision**: single existing project (`salamand.vcxproj`); no new
files except possibly none — all fixes land in existing translation units.

## Complexity Tracking

No constitution violations — table not needed.

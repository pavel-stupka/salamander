# Implementation Plan: Instant Markdown Viewer Display

**Branch**: `065-mdview-instant-render` | **Date**: 2026-08-21 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/065-mdview-instant-render/spec.md`

## Summary

The MDView viewer's first open in a session shows a long blank stage because
the first WebView2 controller creation must spawn the shared
`msedgewebview2.exe` process tree, and the tree exits when the last viewer
window closes (research R1). Per the clarified spec, the fix is a **keeper**:
at the first actual Markdown view, a hidden environment + controller is
created asynchronously on the main thread (joint decision R3) and held for
the rest of the session, so the browser tree never shuts down and every
subsequent view — including after all viewer windows were closed — attaches
warm and renders essentially instantly. No work runs before the first view;
a default-on plugin-configuration option (`CONFIG_KEEPREADY`, new
configuration dialog replacing the About placeholder) can turn the behavior
off (R6). Failures are silent with per-view re-arm (R7). The user data
folder becomes app-neutral (`%LOCALAPPDATA%\Tandem Commander\WebView2`) and
the UDF + browser-arguments set is documented as an app-wide sharing
contract, so planned future WebView2 plugins (formatted source, WebGPU)
inherit the warm tree (R9, `contracts/keeper.md`).

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022)
**Primary Dependencies**: vendored WebView2 SDK 1.0.4078.44
(`src/common/dep/webview2/`, static loader) — already present; md4c
(unchanged); no new external dependencies
**Storage**: plugin registry configuration — one new value
`CONFIG_KEEPREADY` (`REG_DWORD`, default 1) under the existing mdview key
**Testing**: existing console harness `tests/mdview_htmlgen_test/`
(untouched, must still pass); manual timing/process validation per
`quickstart.md` (no automated UI test infrastructure)
**Target Platform**: Windows 11+, pure WinAPI; WebView2 Evergreen runtime as
OS component (missing-runtime fallback unchanged)
**Project Type**: viewer plugin (`mdview.spl`) of a desktop WinAPI
application
**Performance Goals**: any view after the first of a session renders within
2× of today's back-to-back warm time (SC-001); engine start-up at most once
per session (SC-002); first view no slower than current build (SC-006)
**Constraints**: zero background work and zero footprint before the first
view (FR-001/SC-003); kept-ready idle footprint bounded by one open viewer
window and stable (FR-004/SC-004); all keeper failures silent (FR-005);
existing viewer behavior and security lockdown unchanged (FR-006)
**Scale/Scope**: single plugin; ~4 source files touched + 1 dialog template
+ translation rows; no build-system, core, or plugin-API changes

## Constitution Check

*GATE: evaluated against Tandem Commander Constitution v3.1.0.*

| # | Principle | Verdict | Notes |
|---|-----------|---------|-------|
| I | Build Reproducibility | PASS | No build-pipeline changes; same projects, same `build.cmd`. |
| II | Backward Compatibility | PASS | Additive performance improvement, default-on with an opt-out restoring today's behavior exactly (FR-008/SC-007); one new registry value under the existing plugin key, tolerant of absence/corruption (clamp, default 1); registry root and plugin ABI untouched. |
| III | Incremental Modernization | PASS | Confined to `src/plugins/mdview/`; no refactoring of adjacent code beyond extracting the env-options helper actually being shared (R5). |
| IV | Windows Platform Commitment | PASS | Pure WinAPI + already-vendored WebView2 SDK; no new dependencies or licensing changes. |
| V | Plugin Architecture Preservation | PASS | Entirely inside the mdview plugin; plugin interface version unchanged. |
| VI | UI Consistency | PASS (by design) | The one new dialog (configuration) is `DIALOGEX` + `DS_SHELLFONT` + `FONT 8, "MS Shell Dlg"`, standard themed controls, dark-mode two-touchpoint pattern (R6); no process-wide visual behavior touched. |

**Post-design re-check** (after Phase 1): no new violations — the keeper is
invisible (no UI), the dialog design follows the house pattern, and no
complexity deviations were introduced. Complexity Tracking stays empty.

## Project Structure

### Documentation (this feature)

```text
specs/065-mdview-instant-render/
├── plan.md              # This file
├── spec.md              # Feature specification (clarified 2026-08-21)
├── investigation-leads.md  # Pre-planning code reconnaissance
├── research.md          # Phase 0 — analysis + decisions R1–R8
├── data-model.md        # Phase 1 — keeper state machine + config value
├── quickstart.md        # Phase 1 — manual validation scenarios
├── contracts/
│   └── keeper.md        # Phase 1 — keeper API contract + invariants
└── tasks.md             # Phase 2 (/speckit-tasks — NOT created by plan)
```

### Source Code (repository root)

```text
src/plugins/mdview/
├── webview.h            # + MdKeeperArm()/MdKeeperDisarm()/MdKeeperArmed()
│                        #   (COM-free declarations, main-thread-only)
├── webview.cpp          # + keeper implementation (hidden window, env,
│                        #   controller, TrySuspend/memory-target,
│                        #   ProcessFailed handling); env-options
│                        #   construction extracted into a shared helper
├── viewer.cpp           # ViewFile(): arm keeper at first actual view
│                        #   (after the RuntimeAvailable gate, before the
│                        #   viewer thread spawn); Debug TRACE timestamps;
│                        #   UserDataFolder() → app-neutral path
│                        #   "...\Tandem Commander\WebView2" + best-effort
│                        #   removal of the old mdview.WebView2 cache (R9)
├── mdview.h / mdview.cpp # g_keepReady + CONFIG_KEEPREADY load/save/clamp;
│                        #   Configuration() → real dialog; Release() →
│                        #   MdKeeperDisarm() alongside ReleaseViewer()
├── mdview.rh / lang/lang.rh  # dialog + control + string IDs
└── lang/lang.rc         # IDD_CFG configuration dialog + strings

translations/<language>/mdview.slt   # rows for the new dialog/strings
                                     # (enabled languages, via the
                                     # translation pipeline; English
                                     # fallback until merged)
```

**Structure Decision**: keeper lives in `webview.cpp` — the one file already
confining all WebView2/COM usage (WRL include gymnastics, `#pragma
push_macro("new")`), and the natural home for the shared env-options helper.
No new files, no project-file changes.

## Design Outline

1. **Arm point** (`viewer.cpp`, `ViewFile`): after the `RuntimeAvailable()`
   gate and before spawning the viewer thread, call `MdKeeperArm()` when
   `g_keepReady` is set. Idempotent; asynchronous; never blocks F3. This is
   the *only* trigger — nothing runs earlier in the session (FR-001).
2. **Keeper** (`webview.cpp`): hidden never-shown `WS_EX_TOOLWINDOW` window
   on the main thread; async environment (shared options helper, identical
   `AdditionalBrowserArguments`) → async controller → `put_IsVisible(FALSE)`
   → best-effort `TrySuspend` + `MemoryUsageTargetLevel(LOW)` (R4). State
   machine per `data-model.md`; failures silent, one re-arm chance per
   subsequent view (R7).
3. **Disarm points**: config toggle-off (dialog OK), plugin
   `Release()` (after windows/threads are down, next to `ReleaseViewer()`),
   process exit. Disarm closes the controller and releases everything;
   with no viewer windows open the browser tree then exits on its own —
   which is exactly the current build's behavior (SC-007).
4. **Configuration** (`mdview.cpp` + `lang.rc`): `IDD_CFG` dialog from
   `Configuration()`; checkbox bound to `g_keepReady`; persisted via
   Load/SaveConfiguration; house-style + dark-mode patterns (R6).
5. **Instrumentation** (`viewer.cpp`/`webview.cpp`): Debug `TRACE_I`
   timestamps (ViewFile → OnReady → NavigationCompleted) + keeper events
   (R8); validation flow in `quickstart.md`.
6. **Sharing contract** (R9): `UserDataFolder()` returns the app-neutral
   `%LOCALAPPDATA%\Tandem Commander\WebView2` (viewer windows and keeper
   alike); the old `mdview.WebView2` cache folder is removed best-effort at
   the first view. The UDF + the options helper's argument set form the
   app-wide contract for future WebView2 plugins
   (`contracts/keeper.md` § Sharing contract); the helper is written to be
   liftable to `src/common/` unchanged. The contract is published for
   future development in `architecture/11-webview2-integration.md` (linked
   from `CLAUDE.md`) — implementation MUST keep that document accurate
   (paths, argument set, measured behavior).

## Complexity Tracking

No constitution violations — table intentionally empty.

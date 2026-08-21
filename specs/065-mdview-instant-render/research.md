# Research — Instant Markdown Viewer Display (065)

**Date**: 2026-08-21 · **Input**: `spec.md`, `investigation-leads.md`, code
reading of `src/plugins/mdview/` (viewer.cpp, webview.cpp, mdview.cpp).

## R1 — Root cause of the first-open delay

**Finding**: The cold start is the spawn of the WebView2 browser-process
tree. The full chain on F3:

1. `ViewFile` (main thread, `viewer.cpp:289`) → engine-availability gate →
   spawns the thread-per-window viewer (`CViewerThread`).
2. `WM_CREATE` (`viewer.cpp:827-859`) → `Web->Create(HWindow,
   UserDataFolder(), cb)` — every window creates its own WebView2
   environment + controller, asynchronously, on the window's thread.
3. `CMdWebHost::Create` (`webview.cpp:406`) →
   `CreateCoreWebView2EnvironmentWithOptions` → (callback) →
   `CreateCoreWebView2Controller` → (callback) → `OnReady` → render +
   `Navigate` → content paints after `NavigationCompleted`.
4. Until then the window shows the theme `docBg` brush (`BgBrush`,
   `SetBackgroundColor`) — the "blank stage" is theme-colored but long on a
   cold start.

All windows pass the same user data folder (`UserDataFolder()`,
`viewer.cpp:82` → `%LOCALAPPDATA%`). WebView2 environments sharing a user
data folder share one `msedgewebview2.exe` process tree (broker + renderer +
GPU + crashpad). The **first** controller creation spawns the tree
(hundreds of ms to seconds); later creations attach to the running tree
(near-instant — the observed "linked documents open instantly"). When the
**last** controller is released, the runtime shuts the tree down after a
short grace period — the next open is cold again. This fully explains the
reported behavior and confirms the user's hypothesis.

## R2 — What keeps the browser process alive

**Decision**: The keeper must hold a live, hidden `ICoreWebView2Controller`.

**Rationale**: Per the WebView2 process model, the browser process exits
when the last WebView2 (controller/CoreWebView2) for its user data folder
closes; merely holding an `ICoreWebView2Environment` object is not
documented to pin the process and must not be relied on. There is no
keep-alive knob in the WebView2 API.

**Alternatives considered**: environment-only pinning (rejected —
undocumented, fragile); a WebView2 configuration option (does not exist).

**Verification**: at implementation time, observe `msedgewebview2.exe` via
`tasklist` — present while the keeper is armed with all viewer windows
closed, gone after disarm (quickstart scenario 2/4).

## R3 — Keeper hosting *(joint decision, user selected 2026-08-21)*

**Decision**: The keeper — a hidden window + environment + controller —
lives on the **main thread**. It is armed asynchronously from `ViewFile` at
the first actual Markdown view (config permitting) and disarmed on
toggle-off, on plugin `Release`, and at application exit.

**Rationale**: All three lifecycle touchpoints (`ViewFile`,
`CPluginInterface::Configuration`, `CPluginInterface::Release`) already run
on the main thread; the main thread is STA with a message pump, satisfying
WebView2's threading requirements. The keeper does no work after creation —
its callbacks are rare and trivially cheap — so a dedicated thread would add
lifecycle/synchronization code (thread queue, teardown ordering) with no
benefit.

**Alternatives considered**: dedicated keeper thread (more code, more
failure modes, no measurable benefit — rejected by user decision); keeping
the last viewer window alive hidden (breaks the viewer lock handshake with
Salamander and the `ViewerWindowQueue`/`Release` close-all logic —
rejected).

## R4 — Keeper footprint minimization

**Decision**: The keeper controller never navigates (stays on the initial
blank document), is created invisible (`put_IsVisible(FALSE)` on a hidden,
never-shown `WS_EX_TOOLWINDOW` window), and best-effort reduces memory:
`TrySuspend` (available on `ICoreWebView2_3`, requires invisible state) and
`MemoryUsageTargetLevel(LOW)` (`ICoreWebView2_19`+). Both are applied via
`QueryInterface` and silently skipped if unavailable — the vendored SDK
(1.0.4078.44) declares both, but the installed Evergreen runtime governs at
run time.

**Expected idle footprint**: the shared tree with one blank suspended
renderer, on the order of 50–90 MB — by construction no more than one open
viewer window (same tree + a rendered document + a visible controller),
which is the SC-004 bound. Actual numbers are measured during
implementation (quickstart scenario 7).

## R5 — Race between the keeper and the first window

**Finding**: Arming from the first `ViewFile` runs keeper creation
concurrently with the first viewer window's own environment/controller
creation (on the window's thread). With the same user data folder, WebView2
serializes browser-process startup; the second requester attaches. No
duplicate tree; the first view stays cold-speed, which the clarification
accepts.

**Constraint**: the keeper's environment options must be **byte-identical**
to the viewer host's (`AdditionalBrowserArguments` in `webview.cpp:416-418`)
— arguments of environments created after the browser process is running are
silently ignored, so drift would produce invisible inconsistencies.
**Action**: extract the option construction into one shared helper used by
both `CMdWebHost::Create` and the keeper.

## R6 — Configuration surface (FR-008)

**Decision**: A real configuration dialog replaces the About placeholder in
`CPluginInterface::Configuration` (`mdview.cpp:174`): one checkbox
("Keep the rendering engine ready for instant viewing") + OK/Cancel.
Persisted as `CONFIG_KEEPREADY` (`REG_DWORD`, default 1, clamped to 0/1) in
`LoadConfiguration`/`SaveConfiguration` beside the existing values.

**Toggle semantics** (per spec FR-008): disable → `MdKeeperDisarm()`
immediately (with viewer windows open the tree lives on until the last one
closes — the current build's behavior); enable → armed at the next view.

**Constitution VI compliance**: `DIALOGEX` with `DS_SHELLFONT`, `FONT 8,
"MS Shell Dlg"`, standard themed controls, dark-mode two-touchpoint pattern
(`ThemeApplyToDialog` + `ThemeHandleCtlColor`, precedent: `FindDlgProc`,
`viewer.cpp:338`).

**Alternatives considered**: a View-menu toggle in the viewer window
(rejected: keep-ready is a global option, not per-document; the View menu
holds per-view options, while Plugins Manager → Configure is the
discoverable global surface — and `Configuration()` is currently a
placeholder waiting for exactly this).

**Translations**: new dialog + strings enter `lang/lang.rc`; the enabled
languages' `mdview.slt` files gain rows through the established translation
pipeline (English fallback until merged). The three disabled languages get
their rows on re-enablement (note the `addrows.py` defect recorded in 056).

## R7 — Failure and crash handling

- Keeper creation failure (environment/controller error): **silent**; the
  keeper stays unarmed and a later `ViewFile` may try again (one attempt per
  view — no retry loops, satisfying FR-005).
- Browser-process death while armed (`ProcessFailed` /
  `BrowserProcessExited`): destroy the keeper quietly; re-arm at the next
  view. The next F3 works regardless — the window creates its own
  environment and spawns a fresh tree (spec edge case honored).
- Engine not installed: `ViewFile` falls back to the text viewer before any
  keeper logic runs — the keeper is never armed (FR-005/SC-005 hold by
  construction).

## R8 — Instrumentation for Success Criteria

**Decision**: Debug-build `TRACE_I` timestamps at `ViewFile` entry,
`OnReady`, and `NavigationCompleted`, plus keeper arm/disarm events.
Cold/warm comparison runs on the Debug build (relative measurement);
process observation (`tasklist /fi "imagename eq msedgewebview2.exe"`) and
Task Manager memory columns verify SC-002/003/004 on both configurations.
No automated UI test infrastructure exists (spec assumption); results are
recorded in the implementation notes.

## R9 — App-neutral user data folder & the sharing contract *(added 2026-08-21 on user request)*

**Context**: future plugins are planned that will also render through
WebView2 (formatted source code, WebGPU). The warm browser tree is shared by
**user data folder**, not by plugin — an environment created with a
different UDF spawns its own separate, cold tree and gains nothing from the
keeper.

**Decision**:

1. `UserDataFolder()` (`viewer.cpp:82`) changes from the plugin-specific
   `%LOCALAPPDATA%\Tandem Commander\mdview.WebView2` to the app-neutral
   `%LOCALAPPDATA%\Tandem Commander\WebView2`. The folder holds only cache
   (mdview never navigates to the web), so nothing is migrated; the old
   `mdview.WebView2` folder is deleted best-effort at the first view of a
   session (failures ignored — e.g. locked by a stale process — retried
   next session).
2. The UDF **and** the browser-arguments set produced by the shared options
   helper are documented as the **app-wide sharing contract**
   (`contracts/keeper.md`): every future WebView2 consumer in the product
   uses the same UDF and the same argument set. `AdditionalBrowserArguments`
   apply only when the browser process starts — later environments' args are
   silently ignored — so any extension of the set is a coordinated change to
   the one helper, never a per-plugin override.
3. The options helper is written to be liftable to `src/common/` unchanged
   when a second consumer appears; per-controller settings (mdview's
   security lockdown) remain per-plugin and constrain nobody else.

**Rationale**: renaming the UDF later costs the same as now but with more
coordination; doing it in this feature makes the keeper's warmth
automatically available to every future WebView2 plugin (each arming its own
keeper at its own first use — any one live controller warms the tree for
all). A core-hosted keeper service via the plugin API is deliberately
deferred until a second consumer exists (it would bump the plugin interface
version).

**Alternatives considered**: keep `mdview.WebView2` (rejected — future
plugins would cold-start their own tree); migrate cache contents (pointless
— cache only); core service now (premature, API bump without a consumer).

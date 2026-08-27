# 11 — WebView2 Integration: the Shared Engine Contract

**Status**: binding for every module that embeds WebView2. Established in
feature 065 (`specs/065-mdview-instant-render/`); first consumer is the
mdview plugin (rendering surface since feature 021, keeper + this contract
since 065), second is the codeview plugin (feature 070). If you are adding a
WebView2-based plugin (WebGPU surface, …), this document tells you what you
inherit for free and what you must not break.

**Where the shared code lives (feature 070)**: `src/common/webhost/` —
`webhost.{h,cpp}` (`CTcWebHost`: environment options, the canonical user data
folder, the availability gate, controller creation, the whole settings
lockdown, resource interception with default-deny, the CSP header,
accelerator routing, zoom, background colour) and `webkeeper.{h,cpp}`
(`CTcWebKeeper`). A plugin configures only what `TcWebHostConfig` exposes:
virtual host name, whether scripts and web messages are enabled, what its
interceptor serves, and its key map. Everything else is an invariant of the
shared code and cannot be relaxed per plugin. Contract:
`specs/070-source-viewer-plugin/contracts/webview-host-sharing.md`.

**Status of the migration**: codeview uses `src/common/webhost/` from the
start. mdview still carries its own copy in `src/plugins/mdview/webview.cpp`;
converting it to the shared host is the outstanding half of the lift and is
tracked in `specs/070-source-viewer-plugin/REMAINING-WORK.md` (it needs the
manual mdview regression pass that only a GUI session can run).

## 1. Why a contract exists

WebView2's process model shares **one browser-process tree**
(`msedgewebview2.exe`: broker + renderer(s) + GPU + crashpad) among all
environments created with the **same user data folder (UDF)**. The first
controller creation spawns the tree (hundreds of ms to seconds — the "cold
start"); every later creation attaches near-instantly; when the last
controller is released, the tree exits after a short grace period.

Feature 065 adds a **keeper** to mdview: at the plugin's first actual view it
creates a hidden environment + controller on the main thread and holds it for
the rest of the session, so the tree stays warm. That warmth is a property of
the *UDF*, not of mdview — any module that follows this contract attaches to
the warm tree and renders instantly whenever any other WebView2 consumer has
already been used in the session.

## 2. The contract (MUST)

1. **Canonical user data folder** — every WebView2 environment in the
   product uses:

   ```
   %LOCALAPPDATA%\Tandem Commander\WebView2
   ```

   Never invent a per-plugin UDF: a different UDF spawns a second, separate,
   cold browser tree and gains nothing from the keeper. The folder holds
   cache only; deleting it is always safe. (Before 065 mdview used
   `...\Tandem Commander\mdview.WebView2`; 065 renames it and removes the
   old folder best-effort at first view.)

2. **One browser-arguments set** — `AdditionalBrowserArguments` apply only
   when the browser process *starts*; arguments passed by environments
   created after that are **silently ignored**. The options helper in
   `src/plugins/mdview/webview.cpp` is therefore the single source of truth.
   Current set:

   ```
   --disable-background-networking --disable-sync --disable-component-update
   --disable-features=msWebOOUI,msPdfOOUI
   ```

   This set is neutral: it does not disable the GPU process or page-level
   networking — WebGPU works on the default modern Evergreen runtime.
   Extending the set (e.g. a GPU flag) is a **coordinated change to the one
   helper**, never a per-plugin override — an override would work or not
   depending on which plugin happened to start the tree first.

3. **Per-controller settings stay per-plugin** — security lockdown, script
   enablement, zoom, resource interception are configured on each plugin's
   own controllers. mdview's strict lockdown (scripts off, default-deny
   network, see `specs/021-mdview-html-renderer/`) constrains nobody else; a
   WebGPU plugin may enable scripts on *its* controllers. Choose your own
   controller settings deliberately — the lockdown is per-WebView, the
   process tree is shared.

4. **Keeper symmetry** — until a shared component exists, each WebView2
   plugin arms its **own** keeper at its **own first use** (never earlier:
   zero background work and zero footprint before the plugin's first real
   use is a product rule, spec 065 FR-001). Any one live controller keeps
   the tree warm for all consumers — whichever plugin is used first warms
   the others. Keeper pattern (contract:
   `specs/065-mdview-instant-render/contracts/keeper.md`):
   - hidden, never-shown `WS_EX_TOOLWINDOW` window on the main thread
     (STA + message pump; WebView2 objects are bound to their creating
     thread);
   - environment + controller via the shared options helper;
     `put_IsVisible(FALSE)`, best-effort `TrySuspend` +
     `MemoryUsageTargetLevel(LOW)` (QueryInterface, skip if unavailable);
   - never navigates, never focused, invisible to Alt+Tab/taskbar;
   - all failures silent (no dialogs outside an actual view attempt);
     re-arm at the next use after `ProcessFailed`/`BrowserProcessExited`;
   - disarmed on the plugin's config toggle-off, in
     `CPluginInterface::Release`, and at process exit;
   - user-facing opt-out: mdview persists `CONFIG_KEEPREADY`
     (`REG_DWORD`, default 1) — mirror the pattern in your plugin.

5. **Second consumer = lift the helper** — the options helper (and
   optionally the keeper) is written to move to `src/common/` unchanged.
   When you add the second WebView2 plugin, do that lift instead of copying
   the code. A core-hosted keeper service exposed through the plugin API is
   deliberately deferred (it would bump `LAST_VERSION_OF_SALAMANDER`).

## 3. Build/source checklist for a new WebView2 plugin

- **SDK**: vendored at `src/common/dep/webview2/` (headers +
  `WebView2LoaderStatic.lib` x86/x64, v1.0.4078.44, BSD-3). Runtime is the
  Evergreen OS component — never distributed. Follow `mdview.props`:
  include dir + `lib\$(ShortPlatform)` + link
  `WebView2LoaderStatic.lib;shlwapi.lib;ole32.lib;version.lib`; `WINVER`
  ≥ `0x0A00`.
- **COM/WRL confinement**: include `<wrl.h>`/WebView2 headers in a single
  .cpp with the debug `new` macro suspended
  (`#pragma push_macro("new")` / `#undef new` / `#pragma pop_macro("new")`)
  — WRL's implements.h is incompatible with the leak-tracking macro
  (precedent: `src/plugins/mdview/webview.cpp`).
- **Availability gate**: check
  `GetAvailableCoreWebView2BrowserVersionString` before relying on the
  engine and provide a graceful fallback (mdview falls back to the internal
  text viewer).
- **Threading**: create environment/controller on an STA thread with a
  message pump; all calls on that thread. Viewer-style windows may each own
  a thread (mdview's thread-per-window); the keeper lives on the main
  thread.
- **Displaying local content**: prefer mdview's pattern — private virtual
  host + `WebResourceRequested` default-deny interception, navigation
  cancelled except for the document (see
  `specs/021-mdview-html-renderer/` and `webview.cpp`).

## 4. References

- `specs/065-mdview-instant-render/contracts/keeper.md` — keeper API,
  invariants, § Sharing contract (authoritative wording)
- `specs/065-mdview-instant-render/research.md` — R1 (cold-start anatomy),
  R2 (what pins the browser process), R4 (footprint), R5 (argument
  identity), R9 (UDF neutralization rationale)
- `specs/021-mdview-html-renderer/` — rendering-surface security lockdown
- `src/plugins/mdview/webview.{h,cpp}` — reference implementation
  (options helper, keeper, host)

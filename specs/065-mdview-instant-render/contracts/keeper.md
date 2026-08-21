# Contract — MDView Engine Keeper (065)

Declared in `src/plugins/mdview/webview.h` (COM-free), implemented in
`webview.cpp`. All functions are **main-thread-only** (assert/document; they
are called from `ViewFile`, the configuration dialog, and
`CPluginInterface::Release`, which all run on the main thread).

## API

```cpp
// Arms the keeper: asynchronously creates a hidden WebView2 environment +
// controller on the main thread so the shared browser-process tree stays
// alive for the rest of the session. Idempotent (no-op while Arming/Armed).
// Never blocks, never shows UI, never fails loudly.
void MdKeeperArm();

// Disarms the keeper: closes the hidden controller and releases everything.
// Idempotent. After disarm (with no viewer windows open) the browser tree
// exits on its own — the current build's behavior.
void MdKeeperDisarm();

// True while the keeper holds (or is creating) its controller.
bool MdKeeperArmed();
```

## Invariants

1. **No early work** (FR-001): `MdKeeperArm()` is called only from
   `ViewFile` on an actual Markdown view with `g_keepReady` enabled — never
   from plugin load, `Connect`, configuration load, or any panel event.
   Before the first view the keeper contributes zero footprint and zero
   work.
2. **Warmth** (FR-002): while Armed, the shared `msedgewebview2.exe` tree is
   alive, so every `CMdWebHost::Create` (any viewer window, any thread)
   attaches warm.
3. **Option identity** (R5): the keeper's environment options are built by
   the same helper as `CMdWebHost::Create`'s — byte-identical
   `AdditionalBrowserArguments`, same user data folder. The helper is the
   single source of truth; neither site duplicates the strings.
4. **Silence** (FR-005): every failure path (environment creation,
   controller creation, `ProcessFailed`, `BrowserProcessExited`) releases
   state quietly. No message box, no log visible to the user, no effect on
   any open or future viewer window. A later `ViewFile` may arm again
   (at most one attempt per view — no retry loops).
5. **Lifecycle** (FR-008): disarmed on config toggle-off (immediately), in
   `CPluginInterface::Release` after viewer windows/threads are down
   (alongside `ReleaseViewer()`), and at process exit. Enabling the option
   arms at the next view, not retroactively.
6. **Invisibility**: the keeper window is created hidden
   (`WS_EX_TOOLWINDOW`, never `ShowWindow`), the controller is
   `IsVisible(FALSE)`, never navigated, never focused; it must never appear
   in Alt+Tab, the taskbar, or the viewer window queue, and must not
   interfere with `Release`'s open-windows check (`ViewerWindowQueue` stays
   keeper-free).
7. **Security posture unchanged** (FR-006): the keeper never displays
   content and never navigates; the viewer windows' lockdown configuration
   is untouched.

## Sharing contract (app-wide, for future WebView2 plugins) — R9

The warmth the keeper maintains is a property of the shared browser-process
tree, keyed by **user data folder** — not of mdview. Any future WebView2
consumer in the product (e.g. a formatted-source viewer or a WebGPU surface)
inherits it by honoring this contract:

1. **Canonical user data folder**:
   `%LOCALAPPDATA%\Tandem Commander\WebView2` — used by every WebView2
   environment in the product (mdview's viewer windows and keeper included;
   renamed from `mdview.WebView2` in this feature, old folder removed
   best-effort at first view — it holds cache only).
2. **One argument set**: `AdditionalBrowserArguments` apply only when the
   browser process starts; later environments' arguments are **silently
   ignored**. The shared options helper in `webview.cpp` is therefore the
   single source of truth for the argument set. Extending it (e.g. a future
   GPU flag) is a coordinated change to the helper — never a per-plugin
   override. Current set: `--disable-background-networking --disable-sync
   --disable-component-update --disable-features=msWebOOUI,msPdfOOUI`
   (neutral: does not disable the GPU process or page networking; WebGPU
   works on the default modern Evergreen runtime).
3. **Per-controller settings stay per-plugin**: security lockdown, script
   enablement, zoom, etc. are configured on each plugin's own controllers;
   mdview's lockdown constrains nobody else.
4. **Keeper symmetry**: until a shared component exists, each WebView2
   plugin arms its own keeper at its own first use. Any one live controller
   keeps the tree warm for all — whichever plugin is used first warms the
   others. When a second consumer appears, lift the options helper (and
   optionally the keeper) into `src/common/` unchanged; a core-hosted
   keeper service via the plugin API is deferred until then (interface
   version bump).

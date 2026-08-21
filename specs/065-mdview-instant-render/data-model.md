# Data Model — Instant Markdown Viewer Display (065)

The feature has no document data; its state is the keeper lifecycle and one
persisted configuration value.

## Keeper (module-level singleton in `webview.cpp`, main thread only)

| Field | Type | Meaning |
|-------|------|---------|
| `state` | enum | `Unarmed` / `Arming` / `Armed` (see transitions) |
| `hwnd` | `HWND` | hidden never-shown host window (`WS_EX_TOOLWINDOW`), NULL unless Arming/Armed |
| `env` | `ICoreWebView2Environment*` | keeper's environment (shared user data folder, shared options helper) |
| `controller` | `ICoreWebView2Controller*` | the live hidden controller that pins the browser process |
| `procTok` / `exitTok` | event tokens | `ProcessFailed` / `BrowserProcessExited` registrations |

### State transitions

| From | Trigger | To | Action |
|------|---------|----|--------|
| Unarmed | `MdKeeperArm()` (first/next view, `g_keepReady` on) | Arming | create hidden window, start async env creation |
| Arming | controller-ready callback OK | Armed | `IsVisible(FALSE)`, best-effort `TrySuspend` + memory target LOW |
| Arming | any creation callback fails | Unarmed | release partial state silently (no dialog) |
| Armed | `ProcessFailed` / `BrowserProcessExited` | Unarmed | release everything silently; next `MdKeeperArm()` may retry |
| Armed / Arming | `MdKeeperDisarm()` (toggle-off, plugin Release, exit) | Unarmed | close controller, release env, destroy window |
| any | `MdKeeperArm()` while Arming/Armed | (unchanged) | idempotent no-op |

**Invariant**: `state == Armed` ⇒ the shared `msedgewebview2.exe` tree is
alive ⇒ any `CMdWebHost::Create` attaches warm. `state == Unarmed` after
disarm reproduces the current build's lifecycle exactly.

## Configuration

| Value | Registry name | Type | Default | Validation |
|-------|---------------|------|---------|------------|
| `g_keepReady` | `CONFIG_KEEPREADY` ("KeepReady") | `REG_DWORD` | `1` (on) | clamp to 0/1 in `LoadConfiguration` (corruption-tolerant, missing ⇒ default) |

Loaded/saved in `CPluginInterface::LoadConfiguration` /
`SaveConfiguration` beside the existing values (scheme, zoom, …); edited by
the new configuration dialog (`Configuration()`); read by `ViewFile` at each
arm decision and by the dialog's toggle-off path
(`MdKeeperDisarm()` immediately on disable; enable applies from the next
view — FR-008).

## Existing entities (unchanged)

`CViewerWindow`, `CMdWebHost`, `MdHtmlResult`, themes, and the viewer lock
handshake are not modified structurally; `ViewFile` gains only the arm call
and Debug-only trace points.

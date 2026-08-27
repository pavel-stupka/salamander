# Contract: Shared WebView2 Host — the mdview → `src/common/webhost/` Lift

**Status**: binding for feature 070 and every later WebView2 consumer.
Implements the "second consumer lifts the helper" clause of
`architecture/11-webview2-integration.md` §2.5 (which this feature updates to
point here). mdview's user-visible behaviour MUST NOT change.

## 1. What moves (from `src/plugins/mdview/webview.{h,cpp}`)

| Today (mdview) | Lifted as | Notes |
|---|---|---|
| `MdBuildEnvOptions()` | `TcWebBuildEnvOptions()` | **the** single browser-arguments source (contract §2.2); argument set unchanged |
| `MdUserDataFolder()` | `TcWebUserDataFolder()` | canonical UDF `%LOCALAPPDATA%\Tandem Commander\WebView2`; old-UDF janitor stays mdview-local (mdview history) |
| `RuntimeAvailable()` | `TcWebRuntimeAvailable()` | availability gate |
| `CMdWebHost` create/lockdown/interception/accelerator plumbing | `CTcWebHost` | parameterised by `TcWebHostConfig` (below) |
| Keeper (`MdKeeperArm/Disarm/Armed`, window class) | `TcWebKeeper` | window-class name becomes a config field — unique per plugin (mdview: `TandemMdKeeperWnd` kept for continuity; codeview: `TandemCvKeeperWnd`) |

Stays in mdview: WinHTTP remote-image fetch, `img/<n>` table, `.md` link
gate, document HTML generation, `doc.html?v=` navigation scheme.

## 2. `TcWebHostConfig` (per-plugin, set before `Create`)

| Field | mdview value | codeview value |
|---|---|---|
| `virtualHost` | `mdview.invalid` | `codeview.invalid` |
| `scriptsEnabled` | **FALSE** | **TRUE** (spec clarification #1) |
| `webMessagesEnabled` | FALSE | TRUE (schema per `host-page-interface.md`) |
| `serveCallback` | mdview `ServeRequest` | codeview asset/text server |
| `acceleratorMap` | mdview keys | codeview keys (built-in-viewer parity map) |
| `defaultBackgroundColor` | theme docBg | scheme editor background |
| `keeperClassName` / `keepReadyFlag` | `TandemMdKeeperWnd` / mdview `KeepReady` | `TandemCvKeeperWnd` / codeview `KeepReady` |

Everything **not** in the config is invariant and applied identically by the
shared lockdown routine: default context menus OFF, DevTools OFF, status bar
OFF, built-in error page OFF, script dialogs OFF, host objects OFF, browser
accelerator keys OFF, autofill/password OFF, pinch/swipe OFF, reputation
checking OFF, zoom control ON, `NavigationStarting` allow-only-own-document,
`NewWindowRequested` handled, `WebResourceRequested` filter `*` +
default-deny, `ProcessFailed` recovery, `NavigationCompleted` focus move,
`DefaultBackgroundColor` before first `put_IsVisible(TRUE)`. Newer-interface
lockdowns (`DownloadStarting`, `LaunchingExternalUriScheme`, `SaveAsUIShowing`,
`ScreenCaptureStarting`, `PermissionRequested` deny) are added to the shared
routine (QueryInterface, silently skipped on older runtimes) — mdview gains
them too (defence-in-depth, no behaviour change).

## 3. Invariants (MUST)

1. One UDF, one argument set, per-controller settings only — contract §2.1–2.3
   unchanged.
2. The shared code is **source-lifted** (compiled into each plugin like
   `src/plugins/shared/*.cpp`); no new DLL, no plugin-API change, interface
   106 untouched.
3. Keeper symmetry: each plugin arms its own keeper at its own first view,
   never earlier (065 FR-001); either keeper alone keeps the tree warm for
   both; disarm on config toggle-off, `Release`, process exit; silent
   failures; re-arm after `ProcessFailed`/`BrowserProcessExited`.
4. Debug builds assert the applied lockdown by reading settings back
   (spec FR-033); an assertion failure names the setting.
5. WRL/COM confinement: the single lifted .cpp includes `<wrl.h>`/WebView2
   headers with the debug-`new` macro suspended (mdview precedent).
6. No profile-wide writes from either plugin: no `PreferredColorScheme`, no
   `WEBVIEW2_DEFAULT_BACKGROUND_COLOR` env var (research D12).

## 4. Verification

- mdview regression pass after the lift: 021 lockdown re-verification test
  (hostile corpus, scripts stay OFF), 065 keeper scenarios (first-view warm,
  close-all-then-open, crash re-arm, `KeepReady` toggle), zoom/find/schemes
  smoke — all unchanged.
- codeview uses the same lifted routine with its config; its scripts-ON
  lockdown is verified by `rendering-lockdown.md`.
- `architecture/11-webview2-integration.md` §2.2/§2.5/§3 updated to point at
  `src/common/webhost/`; mdview's vcxproj gains the shared sources, codeview's
  references them from the start.

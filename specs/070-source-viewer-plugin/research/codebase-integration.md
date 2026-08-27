# Feature 070 — Codebase integration research: source-code viewer plugin

**Date**: 2026-08-26 · **Scope**: read-only investigation of the Tandem Commander
tree at `main` (0.1.5, build 189, plugin interface 106) to establish what a new
WebView2-based F3 viewer plugin for source/configuration files inherits, must
honour, and must avoid. All references are `file:line` at HEAD; line numbers of
long functions are given as ranges.

Sections 1–7 answer the seven research questions; "Implications for the spec"
and "Open questions" close the document.

---

## 1. Viewer registration and priority

### 1.1 The registration API

| Item | Reference |
|---|---|
| `CSalamanderConnectAbstract::AddViewer(const char* masks, BOOL force)` — masks separated by `;` (`;;` escapes a literal `;`), wildcards `*` `?`, "avoid spaces", **`|` forbidden** (no inverse masks). With `force == FALSE` the call is honoured only on plugin *installation*; `force == TRUE` adds the masks always (if not yet present) and must be guarded by the plugin's own config version. | `src/plugins/shared/spl_base.h:281-289` |
| `ForceRemoveViewer(mask)` — removes one mask from this plugin's rows. | `src/plugins/shared/spl_base.h:291-294` |
| The Connect/upgrade protocol (`CURRENT_CONFIG_VERSION`, "leave `force` FALSE in the base part, put `AddViewer(PPP, TRUE)` under `if (ConfigVersion < N)`"). | `src/plugins/shared/spl_base.h:512-560` |
| `SetBasicPluginData(name, FUNCTION_VIEWER | …, version, copyright, description, regKeyName, extensions, fsName)` — `FUNCTION_VIEWER` (`0x0040`) enables `GetInterfaceForViewer()`. | `src/plugins/shared/spl_base.h:433`, `:744-747`, `:654` |
| Viewer interface: `ViewFile(name, left, top, width, height, showCmd, alwaysOnTop, returnLock, lock*, lockOwner*, viewerData, enumFilesSourceUID, enumFilesCurrentIndex)` and `CanViewFile(name)`. | `src/plugins/shared/spl_view.h:54-65` |

### 1.2 What `AddViewer` actually does (core side)

`CSalamanderConnect::AddViewer` — `src/plugins1.cpp:636-771`:

- `|` anywhere in the string → `TRACE_E` and the call is dropped (`:639-643`).
- Effective only when `Viewer || force` (`:644`). `Viewer` is the "viewer
  modifications allowed" flag of `CSalamanderConnect`
  (`src/plugins.h:3359-3383`), computed per load at
  `src/plugins1.cpp:2202` and **narrowed at `:2327` to
  `supportViewer = (!supportViewer && SupportViewer)`** — TRUE only on the load
  in which the plugin gains `FUNCTION_VIEWER` for the first time (installation
  through Plugin Manager / `plugins.ver` auto-install). Passed into the
  connect object at `:2405-2406`. Consequence: **`AddViewer(masks, FALSE)` is
  a one-shot at installation; every later load ignores it**, exactly as the
  header says.
- Installation path (`Viewer && !force`): one
  `CViewerMasksItem(masks, "", "", "", -Index-1, FALSE)` is created and
  **inserted at index 0** of `MainWindow->ViewerMasks` (`:754-770`). Each
  `AddViewer` call is one row; the whole string of that call is the row's
  mask group. Because every call inserts at the top, the *last* call of a
  plugin ends up highest; the plugin's rows always land **above every
  pre-existing row** including the built-in `*.*` row.
- Update path (`!Viewer && force`): the string is copied into a **300-byte
  stack buffer** (`char ext[300]`, `ext2[300]`, `:646-653`; anything beyond
  299 bytes is silently cut), split, de-duplicated against the plugin's
  existing rows (`:689-737`), re-joined, and inserted at index 0 as a new
  row (`:754-770`).

### 1.3 Data model, defaults, persistence

- Row: `CViewerMasksItem { CMaskGroup* Masks; char *Command,*Arguments,*InitDir; int ViewerType; DWORD HandlerID; BOOL OldType; }`
  — `src/cfgdlg.h:76-100`; list `CViewerMasks : TIndirectArray` `:107-114`.
- `ViewerType`: `0` = external program, `1` = built-in text/hex viewer, `< 0`
  = plugin index `-(type)-1` in `Plugins` — `src/plugins.h:2762-2765`
  (`VIEWER_EXTERNAL`/`VIEWER_INTERNAL` at `src/cfgdlg.h:73-74`).
- Two lists on the main window: `ViewerMasks` (F3) and `AltViewerMasks`
  (Alt+F3), guarded by `ViewerMasksCS` — `src/mainwnd.h:443-445`, `:526-528`.
- Fresh-config defaults — `src/mainwnd1.cpp:405-427`: primary list =
  `*.rpm` → `ViewerType = -2` (TAR, "2nd plugin in the default
  configuration"), then `*.*` → internal; alternative list = `*.*` → internal.
- Registry: key `Viewers` / `Alternative Viewers` under the config root
  (`src/mainwnd2.cpp:467-468`), numbered subkeys `1`,`2`,… in list order,
  values `Masks` (REG_SZ), `Type` (DWORD), `Command`/`Arguments`/`InitDir`
  (`:469-473`); `SaveViewers` `src/salamdr2.cpp:2755-2787`, `LoadViewers`
  `:2688-2753`; saved at `src/mainwnd2.cpp:2079-2080`, loaded at
  `:2380-2382`.
- Plugin removal deletes the plugin's rows in both lists and renumbers the
  other plugin rows — `src/plugins1.cpp:2664-2697`; rows pointing at a
  non-existent plugin are pruned at load — `src/plugins2.cpp:2272-2293`.

### 1.4 How F3 chooses a viewer (`ViewFileInt`, `src/fileswn5.cpp:959-1216`)

1. Extension = text after the last `.` of the name part; a leading-dot name
   like `.cvspass` counts as an extension (`:981-988`; matching rule in
   `src/masks.cpp:755-765`).
2. List = `AltViewerMasks` if `altView`, else `ViewerMasks` (`:1008`).
3. If a `handlerID` was supplied (View With…, file history) the row with that
   `HandlerID` is used from either list, **bypassing masks and `CanViewFile`**
   (`:1011-1024`).
4. Otherwise a **linear scan in list order**: the first row whose
   `Masks->AgreeMasks(namePart, ext)` matches is the candidate (`:1026-1061`).
   If the candidate is a plugin row, `plugin->CanViewFile(name)` is asked
   (`:1037-1051`); `FALSE` → `continue` to the next matching row (**cascade**).
   External and internal rows never decline.
5. Dispatch: external → `CreateProcess` (`:1066-1150`); `VIEWER_INTERNAL` →
   `OpenViewer(name, vtText, …)` with the remembered placement
   (`:1152-1177`); plugin → `plugin->ViewFile(name, place…, Configuration.AlwaysOnTop, returnLock, …)`
   (`:1179-1207`). No match at all → `IDS_CANT_VIEW_FILE` (`:1210-1215`).

**There is no size, binary or content gate in the dispatcher.** Whether a
huge or binary file is acceptable is decided entirely by the viewer that wins
(for plugins: by `CanViewFile`, then by `ViewFile`). `CPluginData::CanViewFile`
(`src/plugins1.cpp:3690-3699`) calls `InitDLL` first, so the first F3 on a
matching mask loads the plugin DLL just to ask.

**Who wins when several rows match**: purely the row order — there is no
weighting, no "most specific mask" rule, no per-plugin priority. A plugin can
only lower itself at runtime by returning `FALSE` from `CanViewFile`; it cannot
claim a file whose masks it did not register.

### 1.5 Runtime accept/decline hook

`CPluginInterfaceForViewerAbstract::CanViewFile(const char* name)` —
`src/plugins/shared/spl_view.h:59-65`: "must not show any window; if it
returns FALSE Salamander looks for another viewer in the priority list". This
is exactly the "CanViewFile-style hook" asked for; it is per-file, content-
based, and used today by mdview (binary sniff, `src/plugins/mdview/viewer.cpp:255-275`),
pictview (probes only extensions known to collide: `.scr .pct .pic .pict .img .eps .ept .ai .mov .msp .cdr .cdt .sep`, `src/plugins/pictview/pictview.cpp:1989-2045`),
peviewer and uniso.

### 1.6 The Options ▸ Viewers page and upgrade behaviour

- `CCfgPageViewers` (`src/dialogs5.cpp:1918-2330`), used twice: F3 list and
  "Alternative Viewers" (`alternative` ctor flag, `:1918-1924`). Rows live in
  a `CEditListBox` (`IDL_FILEMASKS`) with **drag re-ordering**
  (`EnableDrag` `:2160`, `EDTLBN_MOVEITEM2` `:2296-2320`), delete
  (`:2322-2328`), inline mask editing (`EDTLBN_GETDISPINFO` `:2237-2274`), and
  a viewer-type combo listing "External", "Internal", then one entry per
  loaded viewer plugin in `Plugins.GetViewerIndex()` order (`:1936-1951`,
  `:2110-2120`). Validation rejects `|` and syntax errors (`Validate`, `:1973-1990`).
- On upgrade nothing touches the user's rows: `AddViewer(…, FALSE)` is inert
  after installation (§1.2), rows persist verbatim in the registry, and the
  plugin's `ViewerType` index is renumbered only if a plugin before it is
  removed. Adding masks later is the plugin's job via
  `AddViewer(new, TRUE)` under a config-version check (`spl_base.h:540-547`).
- New plugins reach the list through `plugins\plugins.ver`: `build.cmd:459-503`
  writes a clock-based monotonically increasing version (minutes since
  2000-01-01) and one `ver:relative\path.spl` line per `.spl` found by
  `for /r` (alphabetical directory enumeration); `SearchForAddedSPLs`
  (`src/plugins2.cpp:2849-2947`) compares the first line with
  `Configuration.LastPluginVer` and lists SPLs whose line version is newer;
  `ReadPluginsVer` (`:2950+`) then `AddPlugin`s them **in file order**, each
  install running `Connect` → `AddViewer` → `Insert(0)`. The
  `THIS_CONFIG_VERSION` note at `src/mainwnd2.cpp:144-148` explains that a
  program-version bump also resets the counter so new plugins auto-install.
  Consequence worth stating in the spec: **the plugin directory name decides
  the default priority among plugins installed in the same batch** — a
  directory sorting after `mdview` and `pictview` (e.g. `srcview`) ends up
  *above* them; one sorting before (e.g. `codeview`) ends up below.

### 1.7 Alt+F3 and View With…

- `CM_ALTVIEW` → `ViewFile(NULL, altView = TRUE, …)` — `src/mainwnd3.cpp:3678-3683`
  (menu `src/menu4.cpp:53`, toolbar `src/toolbar4.cpp:210`). It uses the
  **separate `AltViewerMasks` list**, whose default is a single `*.*` →
  internal row (`src/mainwnd1.cpp:421-427`). `AddViewer` never inserts into
  it (`src/plugins1.cpp:761`), so out of the box **Alt+F3 always opens the
  built-in text viewer**, regardless of plugins — the natural "escape hatch"
  for the new plugin.
- `CM_VIEW_WITH` → `ViewFileWith` (`src/mainwnd3.cpp:3685-3692`,
  `src/fileswn5.cpp:1732-1765`): a popup with **every distinct viewer of both
  lists** (`FillViewWithData` `:1648-1700` de-duplicates by `ViewerType`;
  plugin rows show the plugin name and icon `:1601-1625`), then opens by
  `HandlerID` — masks and `CanViewFile` are bypassed (§1.4 step 3). The Find
  window has the same commands (`src/finddlg1.cpp:2546-2572`, `:3767`).

### 1.8 Limits on mask strings (important for a 200–400-extension plugin)

| Limit | Value | Reference | Effect |
|---|---|---|---|
| `CMaskGroup::MasksString[MAX_GROUPMASK]` | 1001 bytes incl. NUL | `src/masks.h:78`, `src/plugins/shared/spl_gen.h:633`; truncation + `TRACE_E` at `src/masks.cpp:454-457` | in-memory cap per row |
| `LoadViewers` reads `Masks` into `char masks[MAX_PATH]` via `GetValue(…, MAX_PATH)` | **259 bytes** per row | `src/salamdr2.cpp:2696`, `:2706` | `SalRegQueryValueExW8` returns `ERROR_MORE_DATA` when the buffer is too small (`src/salamdr6.cpp:2381-2387`), `GetValueAux` shows an "Error Loading Configuration" box (`quiet == FALSE`, `src/regwork.cpp:115-160`) and returns FALSE, and `LoadViewers` **`break`s (`:2745`) — that row and every row below it are lost** at the next start |
| Options page item buffer | `EDTLB_DISPINFO::Buffer[MAX_PATH]`, copied with `lstrcpyn(…, MAX_PATH)` | `src/edtlbwnd.h:77`, `src/dialogs5.cpp:2250` | a longer row is shown/edited truncated |
| `AddViewer(force = TRUE)` work buffers | 300 bytes | `src/plugins1.cpp:646-653` | update-path masks beyond 299 bytes are dropped |
| `GetViewersAssoc` merges all rows of one viewer into a `CDynString` for next/previous-file filtering | unbounded string, but `PrepareMasks` on it is capped at `MAX_GROUPMASK` | `src/mainwnd.h:528`, `src/fileswnb.cpp:1202-1232` | with > 1000 bytes of masks the "only associated extensions" filter silently sees only the first 1000 |
| Number of rows | no cap (`TIndirectArray`); View With menu capped at `CM_VIEWWITH_MAX - CM_VIEWWITH_MIN` distinct viewers (rows de-duplicated per viewer) | `src/fileswn5.cpp:1628-1632` | none in practice |

Precedent: pictview registers **12 rows of ≤ ~90 bytes each**
(`src/plugins/pictview/pictview.cpp:1037-1047`) and adds later extensions
with `AddViewer(…, TRUE)` under `ConfigVersion < N` checks (`:1050-1135`).
The safe rule for the new plugin is therefore **many short rows** (≤ ~200
bytes ≈ 15–20 masks each; 300 extensions ≈ 20 rows), never one long string.
Two side-effects must be accepted: the Viewers page will show ~20 rows for
the plugin, and the row order inside the plugin is the reverse of the call
order (§1.2).

Mask semantics that matter for source files (`src/masks.h:33-57`,
`src/masks.cpp:755-800`): `*.xxx` is optimised to an extension hash
(`MASK_OPTIMIZE_EXTENSION`, `masks.cpp:589`, `:778`); a literal name mask
(`makefile`, `Dockerfile`, `.gitignore`, `CMakeLists.txt`) works as a full-
name wildcard match (`AgreeMask`, `masks.h:14`); `.gitignore`-style names are
matched with extension `gitignore`, so `*.gitignore` also works. Matching is
case-insensitive (`StrICmp`, `masks.cpp:779`).

---

## 2. mdview plugin anatomy (`src/plugins/mdview/`)

### 2.1 Source files

| File | Lines | Role | Reusable for a 2nd WebView2 plugin? |
|---|---|---|---|
| `mdview.cpp` | 256 | `DllMain`, `SalamanderPluginGetReqVer`/`SalamanderPluginEntry` (`:73-110`), `CPluginInterface` (About/Release/Load/SaveConfiguration/Configuration/Connect/Event), config keys, keeper disarm on Release (`:134`) | pattern to copy 1:1 |
| `mdview.h` | 92 | globals, config externs (`g_scheme`, `g_followSys`, `g_schemeLight/Dark`, `g_zoom`, `g_savePos`, `g_wndPlacement`, `g_keepReady` `:20-28`), `WM_USER_VIEWERCFGCHNG` (`:31`), menu command ids (`:34-51`), interface classes | pattern |
| `viewer.h/.cpp` | 67/995 | `CViewerWindow` (WinLib `CWindow`), accelerator table, thread-per-window (`CViewerThread` `:127-224`), `SpawnViewer` (`:231-250`), `CanViewFile`/`ViewFile` (`:255-330`), Find dialog (`:335-378`), theme selection, menu, file load, find, zoom, link gate, `WindowProc` (`:813-995`) | frame + thread model reusable; content pipeline Markdown-specific |
| `webview.h/.cpp` | 81/901 | `CMdWebHost` (COM-free header) — env options helper `MdBuildEnvOptions` (`webview.cpp:37-44`), request interceptor (`:214-256`), controller lockdown (`:270-455`), `RuntimeAvailable` (`:470-478`), `Create` (`:480-530`), `MdUserDataFolder` (`:615-628`), old-UDF janitor (`:630-668`), **session keeper** (`:670-901`) | **the lift candidate** (contract §2.5 says move to `src/common/`) |
| `render.h/.cpp` | 85/189 | `MdTheme` (10 schemes, `render.cpp:21-84`), `MdSyntax` 9-token palette (`render.h:17-20`), `MdDetectDecode` encoding sniff (`render.cpp:137-173`), `MdRenderLimits`, slug, `HlRun`/`HighlightCode` interface | theme *model* and encoding sniff reusable; palette values Markdown-tuned |
| `htmlgen.h/.cpp` | 53/711 | md4c → HTML (`MdRenderHtml` `:594`), theme CSS (`MdBuildThemeCss` `:527-591`, `.hl-*` classes `:588-590`), **`MdBuildSourceHtml`** (`:655-711`: escaped `<pre class="mdsource">` with `<mark id="mdfind-N">` find marks) | `MdBuildSourceHtml` is literally "plain source in a themed `<pre>`" — the new plugin's no-highlighter fallback |
| `highlight.cpp` | 178 | best-effort lexer for ~20 language aliases (`:15-46`), 8 groups (C-like, `#`-comment, SQL, shell, JSON, XML, diff, plain) | too small for the feature's goal (hundreds of languages); shows the 7 token classes `MDCF_*` (`render.h:69-75`) |
| `darkmenu.h/.cpp` | 36/332 | owner-drawn dark menu bar/popups using `GetThemeSysColor/-Brush` (feature 037); the only dark-native-menu code in any plugin | copy or lift; needed if the new viewer has a native menu bar |
| `precomp.h/.cpp` | 62/4 | PCH: includes `splunicode.h` **before** `spl_*.h` (`:41-43`), `versinfo.rh2`, `spl_com/base/gen/view/vers/gui.h`, `dbg.h`, `mhandles.h`, `arraylt.h`, `winliblt.h`, `auxtools.h`, `lang\lang.rh` | pattern |
| `mdview.rh2` | 80 | resource ids: `IDS_PLUGINNAME 46` …, menu strings 2000+, `IDS_THEME_FIRST 2100` (10 reserved), runtime msgs 2200+, `IDD_FIND 300`, `IDD_CFG 310`/`IDC_CFG_KEEPREADY 311`, `IDC_STATIC_1 3000` + `statics.rh2` (translator needs unique non-negative static ids, `:69-76`) | pattern |
| `mdview.rc`, `mdview.rc2` (`IDB_PLUGINICO`), `mdview.def` (`LIBRARY MDVIEW.SPL`, exports `SalamanderPluginEntry`, `SalamanderPluginGetReqVer`), `versinfo.rh2`, `lang/lang.rc|rc2|rh`, `res/plugico.bmp` | — | resources / language module | pattern |
| `vcxproj/mdview.vcxproj`, `mdview.props`, `lang_mdview.vcxproj`, `lang_mdview.props` | — | see §4 | pattern |
| `IMPLEMENTATION_NOTES.md` | 224 | v1 (RichEdit), v2 (021 WebView2), v2.1 (022 UX), v2.2 (065 keeper) history | reading |
| `tests/mdview_htmlgen_test/` | — | standalone console harness (no vcxproj committed, `IMPLEMENTATION_NOTES.md:222-224`) | pattern for a highlighter unit test |

### 2.2 Entry point and registration

- `SalamanderPluginGetReqVer` returns `LAST_VERSION_OF_SALAMANDER` (`mdview.cpp:73-76`); `SalamanderPluginEntry` refuses older hosts (`:85-90`), loads the `.slg` (`LoadLanguageModule`, `:92`), `InitViewer()` (WinLib init + `SetupWinLibTheme` + accelerators, `viewer.cpp:86-109`), then `SetBasicPluginData(LoadStr(IDS_PLUGINNAME), FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION | FUNCTION_VIEWER, VERSINFO_VERSION_NO_PLATFORM, VERSINFO_COPYRIGHT, LoadStr(IDS_PLUGIN_DESCRIPTION), "MDVIEW", NULL, NULL)` (`:102-105`) and `SetPluginHomePageURL` (`:107`).
- `Connect`: `salamander->AddViewer("*.md;*.markdown", FALSE)` + 16×16 plugin icon bitmap → `SetBitmapWithIcons`/`SetPluginIcon(0)`/`SetPluginMenuAndToolbarIcon(0)` (`mdview.cpp:230-245`). No upgrade section yet (`CURRENT_CONFIG_VERSION 1`, `:35`).
- `Release`: refuses while viewer windows are open unless `force` (`:118-138`), kills the thread queue, `MdKeeperDisarm()`, `ReleaseViewer()`.
- `Event`: only `PLUGINEVENT_SETTINGCHANGE` → broadcast `WM_USER_VIEWERCFGCHNG` to open windows (`:247-251`); `PLUGINEVENT_COLORSCHANGED` is ignored (see §6).

### 2.3 WebView2 embedding (`webview.cpp`)

- **Options helper** `MdBuildEnvOptions()` (`:37-44`): the single source of the
  `AdditionalBrowserArguments`
  `--disable-background-networking --disable-sync --disable-component-update --disable-features=msWebOOUI,msPdfOOUI`.
  Used by both `CMdWebHost::Create` (`:503`) and the keeper (`:820`).
- **User data folder** `MdUserDataFolder()` = `%LOCALAPPDATA%\Tandem Commander\WebView2` (`:615-628`); the pre-065 `mdview.WebView2` folder is deleted best-effort on a janitor thread once per session (`:630-668`).
- **Availability gate** `RuntimeAvailable()` = `GetAvailableCoreWebView2BrowserVersionString` (`:470-478`), checked on the main thread in `ViewFile` (`viewer.cpp:285`) and again in `WM_CREATE` (`viewer.cpp:830`).
- **Create** (`:480-530`): `CoInitializeEx(APARTMENTTHREADED)`, `CreateCoreWebView2EnvironmentWithOptions(NULL, udf, options, …)` → `CreateCoreWebView2Controller(parent, …)` → `ApplyControllerReady`.
- **Lockdown at ready** (`ApplyControllerReady`, `:270-455`): `IsScriptEnabled FALSE` (`:285`), default context menus / DevTools / status bar / built-in error page off, `IsZoomControlEnabled TRUE` (engine owns Ctrl+wheel, `:290`), web messages and host objects off, browser accelerator keys off (`Settings3`), autofill/password off (`Settings4`), pinch zoom off (`Settings5`), swipe navigation off (`Settings6`), reputation checking off (`Settings8`) (`:281-311`).
- **Navigation gate**: `NavigationStarting` allows only `https://mdview.invalid/doc.html…` (`kBase`, `:30`), cancels everything else and forwards the URI to `OnActivateLink` (`:314-331`); `NewWindowRequested` handled + forwarded (`:333-349`).
- **Virtual host + default-deny**: `add_WebResourceRequested` + `AddWebResourceRequestedFilter("*", ALL)` (`:351-360`); `ServeRequest` (`:214-256`) answers the document (`text/html; charset=utf-8`) and `img/<n>` from the document's image table; **everything else → 403**. Note: it is an in-memory interceptor, *not* `SetVirtualHostNameToFolderMapping`.
- `ProcessFailed` → `OnProcessFailed` (`:362-371`); `NavigationCompleted` → `MoveFocus(PROGRAMMATIC)` so arrows/PgUp work without a click (`:375-386`); `ZoomFactorChanged` → `OnZoomChanged` (`:389-400`); `AcceleratorKeyPressed` maps F3/Shift+F3/Esc/F9/Shift+F9/Ctrl+F/Ctrl+U/Ctrl+0 to `WM_COMMAND` on the owner window (`:402-441`).
- `put_DefaultBackgroundColor` (`ICoreWebView2Controller2`) from `SetBackgroundColor(COLORREF)` (`:258-268`, `:553-559`), applied before `put_IsVisible(TRUE)` (`:449-451`) to avoid the white flash; the host window also fills `WM_ERASEBKGND` with a theme brush (`viewer.cpp:865-872`).
- `Navigate(fragment)` builds `doc.html?v=<docVersion>[#fragment]` — a new document version forces a full reload, a fragment-only change is a same-document scroll (`:568-587`).
- **Keeper** (`:670-901`): hidden `WS_EX_TOOLWINDOW` window of class `TandemMdKeeperWnd` on the main thread (`:793-830`), environment + controller via the same helper, `put_IsVisible(FALSE)`, `MemoryUsageTargetLevel(LOW)` + `TrySuspend` best-effort (`:745-791`), `ProcessFailed`/`BrowserProcessExited` → quiet teardown, re-arm at next view; `gen` counter invalidates stale completions; class unregistered on disarm (feature 069 fix, `:857-880`). Armed from `ViewFile` only when `g_keepReady` (`viewer.cpp:311-313`).
- COM/WRL confinement: `<wrl.h>`/WebView2 headers included with the debug `new` macro suspended (`:13-23`).

### 2.4 Threading model

`ViewFile` runs on the main thread (§1.4). It spawns a `CViewerThread`
(`CThread`, registered in `CThreadQueue ThreadQueue("MDView Viewers")`,
`viewer.cpp:16`, `:127-224`) and **blocks on a continue event until the
window exists** (`:317-328`). The thread creates `CViewerWindow`, hands back
the lock event (`GetLock`, `:417-422`; signalled in `OpenFile` when a new file
replaces the old, `:791-797`, or at thread end), applies the remembered
placement (`:180-190`), creates the frame (`CWINDOW_CLASSNAME2`, title
"Markdown Viewer", `WS_EX_TOPMOST` when `alwaysOnTop`, `:192-194`),
`ThemeApplyToTopLevel` (`:195`), then runs its own message loop with
`TranslateAccelerator` (`:212-221`). Every WebView2 object lives on that
thread (STA). Windows register in `ViewerWindowQueue` (`:828`) so
`Release` can refuse/close them.

### 2.5 Colour schemes / dark mode

- 10 schemes (5 light: paper, softgray, warmsepia, solarlight, arcticlight; 5 dark: graphite, midnight, solardark, nordicdark, hicontrast), `render.cpp:21-84`; each has 18 document colours + a 9-token `MdSyntax` (`kw str num cmt type fn op add del`, `render.h:17-31`). Ids are stable ASCII, names localised via `IDS_THEME_FIRST + i` (`render.h:24-25`).
- Selection: View ▸ Color Scheme radio list + "Follow system theme" toggle (`BuildMenu`, `viewer.cpp:471-477`), F9/Shift+F9 cycle (`:105-106`), persisted as `ColorScheme`, `FollowSystemTheme`, `SchemeLight`, `SchemeDark` (`mdview.cpp:37-40`).
- `EffectiveTheme()` (`viewer.cpp:424-452`): with follow-system on, the **app Dark theme (`SalamanderGeneral->IsDarkThemeActive()`) takes precedence**, otherwise Windows `AppsUseLightTheme` is read directly from the registry (`:436-443`); the per-polarity slot is then used. Follow-system is **off by default** (`mdview.cpp:27`), i.e. mdview is plugin-local by default and does not react to `PLUGINEVENT_COLORSCHANGED` (open windows keep their scheme until reopened; native menus snapshot `DarkMenus` at `WM_CREATE`, `viewer.cpp:826`).
- The HTML carries `<meta name="color-scheme" content="dark|light">` and CSS variables from `MdBuildThemeCss` (`htmlgen.cpp:527-591`, source view `:696-700`).

### 2.6 Configuration and registry

`CPluginInterface::LoadConfiguration/SaveConfiguration` via
`CSalamanderRegistryAbstract` in the plugin's private key (`mdview.cpp:147-185`):
`Version` (1), `ColorScheme`, `FollowSystemTheme`, `SchemeLight`, `SchemeDark`,
`ZoomPercent` (50–300), `SavePosition`, `WindowPlacement` (REG_BINARY),
`KeepReady` (REG_DWORD, default 1). Corrupt values are clamped (`:162-170`).
The Configuration dialog (`IDD_CFG`, `:187-228`) has exactly one checkbox
("keep engine ready") and uses the feature-049 two-touchpoint dark pattern
(`ThemeHandleCtlColor` + `ThemeApplyToDialog`).

### 2.7 File intake: encoding, size gate, binary

- `CanViewFile` (`viewer.cpp:255-275`): `SplU8ToWExtAlloc` → `CreateFileW`; unreadable → FALSE (cascade); reads 512 bytes; a NUL without a UTF-16 BOM → FALSE (binary → text/hex viewer); conversion failure → TRUE ("let ViewFile try").
- `RenderDocument` (`:572-637`): whole file read into memory; **`SIZE_GATE` = 20 MB** (`:20`): above it the first 64 MB are shown as raw source (`SourceMode = true`, no Markdown parse); `MdDetectDecode` (`render.cpp:137-173`): UTF-8 BOM / UTF-16 LE / UTF-16 BE BOMs, NUL-in-first-4-KB → binary, strict UTF-8 validation, else `CP_ACP` fallback flagged `[ANSI]` in the title (`UpdateTitle`, `viewer.cpp:514-532`). No user-selectable code page, no OEM/ISO tables (contrast §3).
- Engine unavailable: `ViewFile` (main thread) falls back to the built-in viewer through `ViewFileInPluginViewer(NULL, CSalamanderPluginInternalViewerData{Mode 0}, …)` (`:281-303`); a controller/renderer failure inside the window thread shows `IDS_ENGINE_UNAVAILABLE` and closes (`EngineFailed`, `:709-720`) because `ViewFileInPluginViewer` is main-thread-only (`spl_gen.h:1912-1913`).

### 2.8 Find, zoom, source view, links, next/prev file

- Find: `IDD_FIND` dialog (`:335-378`), term stored in `FindText[256]`; matches are emitted as `<mark id="mdfind-N">` at HTML generation time (script-free), navigation is `#mdfind-N` fragments; Ctrl+F / F3 / Shift+F3 (`DoFind`, `:672-707`). Case-insensitive, no regex, no whole-word.
- Zoom: `put_ZoomFactor` 50–300 %, Ctrl+wheel/Ctrl+± handled by the engine, Ctrl+0 reset by the plugin (`:639-649`, `webview.cpp:427-431`), percent shown in the title.
- Source view toggle Ctrl+U (`CM_FILE_OPENTEXT`, `:926-931`) → `MdBuildSourceHtml`.
- Link gate (`ActivateLink`, `:722-789`): `#anchor` native; relative `.md`/`.markdown` → new viewer window (`SpawnViewer`); other local target → path shown only; `http/https/mailto` → `ShellExecuteW`; everything else blocked.
- Next/previous file: `CM_NEXTFILE/CM_PREVFILE` ids exist (`mdview.h:46-47`) but are **not wired** (deferred, `IMPLEMENTATION_NOTES.md:49-50`); `EnumFilesSourceUID/CurrentIndex` are stored (`viewer.h:40-41`) for that purpose.

### 2.9 Reuse classification for the new plugin

- **Lift to `src/common/` (contract)**: `MdBuildEnvOptions`, `MdUserDataFolder`, the keeper (`MdKeeperArm/Disarm/Armed`, class name must become plugin-neutral or per-plugin), and ideally the whole `CMdWebHost` surface (interceptor + lockdown + accelerator routing) parameterised by virtual host name and accelerator map. `architecture/11-webview2-integration.md:87-91` and `CLAUDE.md` ("second consumer lifts the helper to `src/common/` instead of copying it") make this mandatory for the options helper; the keeper is "optionally".
- **Copy as pattern**: thread-per-window + lock handshake, `ViewFile` runtime fallback, `WM_ERASEBKGND` theme brush, `darkmenu`, Find dialog, config load/save with clamping, `versinfo.rh2`/`.rh2`/`.def`/vcxproj shape.
- **Markdown-specific (not reused)**: md4c, `htmlgen` document renderer, image interception (`img/<n>`, WinHTTP remote fetch, `winhttp.lib`), link gate for `.md`, `MdSlug`, the `MdTheme` document colours (a highlighter theme needs a token palette, not blockquote/table colours).

---

## 3. Built-in text viewer feature set (parity baseline)

Entry `OpenViewer()` `src/viewer2.cpp:356`, called for `VIEWER_INTERNAL` at
`src/fileswn5.cpp:1152-1177`; one thread + message loop per window
(`src/viewer2.cpp:226-329`), `WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_HSCROLL`,
global `Configuration.AlwaysOnTop` (`:258`), dark theme applied before first
show (`:277-282`). Caption = name + ` - Viewer` + ` - [UTF-8]`/` - [<conversion>]`
(`src/viewer3.cpp:25-90`). Position remembered via `Configuration.SavePosition`
/ `WindowPlacement` (`src/viewer3.cpp:3609-3613`).

| Capability | Details | Reference |
|---|---|---|
| Find | forward/backward, whole words, case sensitive, **regular expression**, **hex byte-string** mode, 30-entry history; result becomes the selection; escapable with wait window | `src/viewer.h:68-108`, `:12-17`; `src/viewer3.cpp:1048-1571`; dialog `src/lang/lang.rc:2051-2072` |
| Find next / previous | F3 / Shift+F3 (also F6/Shift+F6, Ctrl+L/N/P); seed from Find-Files grep text (`Configuration.CopyFindText`) | `src/viewer3.cpp:1048+`; `src/finddlg1.cpp:2476-2492` |
| Go to offset (hex/decimal) | Ctrl+G; **no "go to line"** | `src/viewer3.cpp:1878-1902`; `src/viewer.cpp:478-515` |
| Text / hex / auto-select / set-as-default | F5/F4 (Ctrl+T/Ctrl+H); `Configuration.DefViewMode` 0 auto/1 text/2 hex; per-mask forcing (`Text Masks`/`Hex Masks`) | `src/viewer3.cpp:1801-1860`; `src/viewer2.cpp:1029-1041` |
| Wrap | two-state toggle (none / word wrap), F2 or Ctrl+W, `Configuration.WrapText`; disabled in hex | `src/viewer3.cpp:1862-1876`, `:3264` |
| Code pages | dynamic Coding menu from `CCodeTables` (`codetbl.cpp`), Recognize (auto), Set as Default (`Configuration.DefaultConvert`), Next/Prev coding F8/Shift+F8, explicit UTF-8 | `src/viewer3.cpp:3284-3378`, `:1904-1948`, `:1987-2010`, `:97-109` |
| Auto-detection | BOM, then strict UTF-8 validation (`ViewerDetectEncoding`), then legacy `RecognizeFileType()` on the first 10 000 bytes decides text vs hex and the code page | `src/viewer2.cpp:75`, `:1043-1141`; `src/viewer.h:38` |
| Selection / clipboard | mouse block select with autoscroll, Shift+arrows/Home/End extension (contiguous byte range only), Select All, Copy (UTF-8 aware), Copy to File…, Auto-Copy-Selection toggle, drag-out, drop-in opens file, >100 MB selection prompt | `src/viewer3.cpp:2823`, `:2221-2500`, `:1573-1620`, `:1950`, `:2857`, `:611-630`; `src/viewer2.cpp:1954-1980` |
| File navigation | Open…, Refresh, Prev/Next/Prev-selected/Next-selected/First/Last file in the source panel via `GetNextFileNameForViewer` etc. keyed by `EnumFileNamesSourceUID` + index (Space/Backspace family) | `src/viewer3.cpp:918-1037`, `:3200-3251` |
| Change detection | none live; size re-check on `WM_ACTIVATE`/paint, error recovery via `WM_USER_VIEWERREFRESH` | `src/viewer.cpp:882`, `:1081`; `src/viewer2.cpp:1188-1215` |
| Fullscreen | maximise toggle F11 | `src/viewer3.cpp:1039-1046` |
| Hex offset tooltip | hovering a byte shows its offset | `src/viewer3.cpp:580-604` |
| Not present | print, bookmarks, line numbers, go-to-line, open-in-editor, tabs, syntax colouring | — |

**Keyboard** (accelerators `IDA_VIEWERACCELS` `src/salamand.rc:145-163`;
hard-coded `WM_KEYDOWN` `src/viewer3.cpp:3388-3585`): F1 help · F2 wrap ·
F3/Shift+F3 find next/prev · Ctrl+F3 / F7 / Ctrl+F find · F4 hex · F5 text ·
F6/Shift+F6 find next/prev · F8/Shift+F8 next/prev coding · F11 fullscreen ·
Esc / Alt+F4 close · Ctrl+Ins / Ctrl+C copy · Ctrl+A select all · Ctrl+G go
to offset · Ctrl+O open · Ctrl+R refresh · Ctrl+S copy to file · Ctrl+H hex ·
Ctrl+T text · Ctrl+W wrap · Space/Backspace next/prev file (Ctrl = selected
only, Shift = last/first) · arrows/PgUp/PgDn/Home/End (+Ctrl/Shift variants).
Mouse wheel lines/page, Shift+wheel horizontal, right-click context menu
(`IDM_VIEWERCONTEXTMENU`, `src/lang/lang.rc2:100-121`).

**Font / colours**: `UseCustomViewerFont` + `ViewerLogFont` (default Consolas
10 pt, `src/viewer.cpp:32-44`; `ChooseFont` with `CF_FIXEDPITCHONLY` in the
config page `src/dialogs4.cpp:1817-1856`). Only four colours:
`VIEWER_FG/BK_NORMAL`, `VIEWER_FG/BK_SELECTED` (`src/consts.h:1291-1297`),
with a dark counterpart table `DarkViewerColors` repointed by
`UpdateCurrentColorsForTheme` (`src/themes.cpp:202-223`,
`src/salamdr1.cpp:457-484`). Config page `CCfgPageViewer` /
`IDD_CFGPAGE_VIEWER` "Internal Viewer" (`src/dialogs4.cpp:1626-1953`,
`src/lang/lang.rc:205-245`): EOL CRLF/CR/LF/NULL, window position, tab size,
force-text/force-hex masks, font, colours.

**Registry**: key `Viewer` (`SALAMANDER_VIEWER_REG`, `src/mainwnd2.cpp:424`),
values `:425-452` (`Forward Direction`, `Whole Words`, `Case Sensitive`, `Find
Text`, `HEX-mode`, `Regular Expression`, `EOL …`, `Tabelator Size`, `Default
Mode`, `Text Masks`, `Hex Masks`, `Viewer Use Custom Font`, `Viewer Font`,
`Wrap Text`, `Auto-Select`, `Default Convert`, `Auto-Copy Selection`, `Go to
Offset Is Hex`, `Save Window Position`, `Left/Right/Top/Bottom/Show`);
colours `Viewer Fg/Bk Normal|Selected` (`:518-521`); `Viewer History`
(`:224`).

**Huge / binary / Unicode / long paths**: streaming through a single 60 000-byte
sliding buffer (`VIEW_BUFFER_SIZE`, `src/viewer.h:7`; `src/viewer2.cpp:483-800`),
64-bit offsets, effectively **no size limit**; binary → hex by
`RecognizeFileType` (first 10 000 bytes) and the >10 000-char line prompt
(`src/viewer.cpp:1272-1289`, `src/viewer2.cpp:1341-1358`); BOM + strict UTF-8
detection, **UTF-16 detected but left to the legacy path** (lands in hex,
`src/viewer2.cpp:1059-1063`); UTF-8 paths with `SAL_MAX_PATH_UTF8` buffers and
`SalCreateFile` (`src/viewer2.cpp:236-248`, `:556`), caption through
`SalU8ToWAlloc` + `SetWindowTextW` (`src/viewer3.cpp:82-89`). Open/Save common
dialogs are still ANSI/`MAX_PATH` (`src/viewer3.cpp:922-940`, `:1627-1646`).

**Plugin access to it**: `CSalamanderGeneralAbstract::ViewFileInPluginViewer(NULL, …)`
(`src/plugins/shared/spl_gen.h:1893-1917`; implementation `src/zip.cpp:2462-2665`,
internal branch `:2618-2652`) with `CSalamanderPluginInternalViewerData
{Mode 0|1, Caption, WholeCaption}` (`spl_gen.h:394-411`); main-thread only,
`enumFilesSourceUID = -1` (no next/prev file), optional disk-cache staging.

---

## 4. What a new plugin requires in this repo (checklist)

Repo-wide search for `mdview` outside its directory finds only: `plugins.cfg`,
`salamand.sln` (+ generated slnf), 11 `translations/<lang>/mdview.slt`,
`translations/ui-overrides.json`, `tools/translate/uicontext.py`, docs
(`CHANGELOG.md`, `CLAUDE.md`, `architecture/11-*`, `doc/third_party.txt`), and
`tests/mdview_htmlgen_test`. Nothing in `setup/`, `help/`, `saltests`, or
`src/plugins/shared/`.

1. **`plugins.cfg`** (root): one line `<dirname>=on`, alphabetically placed
   (`mdview=on` at `plugins.cfg:26`). `src/vcxproj/gen_plugins_filter.ps1:66-70`
   fails the build if a directory under `src\plugins\` has no line, `:54-57`
   if a line names a missing directory; the **cfg name must equal the
   directory name** (`:36-37`) and the output dir under `plugins\` must equal
   it too, else the reconcile step deletes it as stale (`:113-121`).
2. **`src/vcxproj/salamand.sln`**: two `Project(...)` entries (plugin +
   `lang_<name>`), copied from lines 100-103, with new GUIDs; plus per GUID the
   10 `ProjectConfigurationPlatforms` lines copied from `:201-220` (Debug/
   Release × Win32/x64 `ActiveCfg`+`Build.0`, and `Utils (Release)` **ActiveCfg
   only**). The solution is flat (no `NestedProjects`). The generated
   `salamand.gen.slnf` is gitignored and needs no edit.
3. **`src/plugins/<name>/vcxproj/`** — copy the four mdview files:
   - `<name>.vcxproj` (from `mdview.vcxproj`): new `ProjectGuid` (`:23`),
     `RootNamespace` (`:24`), the four `<Import Project="<name>.props">`
     (`:62,74,86,98`), item lists (`:123-212`), `ProjectReference` to
     `lang_<name>.vcxproj` (`:214-217`). Keep the import order
     `x86|x64.props → plugin_base.props → plugin_debug|release.props → <name>.props`,
     `stdcpplatest`, the shared sources `..\..\shared\{auxtools,dbg,mhandles,winliblt}.cpp`,
     and `precomp.cpp` with `PrecompiledHeader=Create`. A C dependency (like
     md4c, `:132-135`) needs `PrecompiledHeader NotUsing` + a unique
     `ObjectFileName`.
   - `<name>.props` (from `mdview.props`, 19 lines):
     `WINVER=0x0A00;_WIN32_WINNT=0x0A00;_WIN32_IE=0x0A00` (`:8`, required by the
     WebView2 headers; `plugin_base.props` sets no WINVER, `lang_base.props`
     pins 0x0601), include `..\..\..\common\dep\webview2\include` (`:9`),
     `ResourceCompile WINVER=0x0A00` (`:12`), link
     `shell32.lib;shlwapi.lib;ole32.lib;winhttp.lib;version.lib;WebView2LoaderStatic.lib`
     (`:15`; `winhttp` only for remote images), lib dir
     `..\..\..\common\dep\webview2\lib\$(ShortPlatform)` (`:16`). Static loader,
     no delay-load, no redistributed DLL. **If the helper is lifted to
     `src/common/`, the include/lib settings move with it** (a `common`
     property sheet or `src/common` project).
   - `lang_<name>.vcxproj` / `lang_<name>.props` (from the mdview pair): new
     GUID, `RootNamespace`, `ShortProjectName` (`lang_mdview.props:6`) — the
     lang props import **before** `x86/x64.props → lang_base.props → lang_debug|release.props`.
   - Inherited, do not duplicate: `plugin_base.props:7-11` (`OutDir
     …\plugins\$(ProjectName)\`, `TargetExt .spl`), `:29` output name, `:31`
     **`.def` file must be `..\$(ProjectName).def`**, `:19-20` PCH, `:16`
     `/MP /J`; `lang_base.props:7-11,18-19,27` (`english.slg`, `NoEntryPoint`,
     `_LANG`).
4. **Sources/resources** (`src/plugins/<name>/`): `<name>.def`
   (`LIBRARY <NAME>.SPL` + the two exports), `<name>.rc` (`#pragma
   code_page(65001)`), `<name>.rc2` (`versinfo.rh2` + `versinfo.rc2`,
   `IDB_PLUGINICO`), `<name>.rh`, `<name>.rh2` (unique static ids block via
   `statics.rh2`, see `mdview.rh2:69-76`), `precomp.h/.cpp`, `lang/lang.rc|rc2|rh`,
   `res/plugico.bmp` (hand-made; `tools/brand/gen_icons.py` does **not** produce
   plugin bitmaps).
   **`versinfo.rh2`**: copy `src/plugins/mdview/versinfo.rh2`, change guard,
   `VERSINFO_DESCRIPTION`, `VERSINFO_INTERNAL`; keep
   `#define VERSINFO_COPYRIGHT "Copyright © 2026 " VERSINFO_HOLDER_TANDEM`
   (`:21`) — the holder name is never spelled out (CLAUDE.md copyright rule,
   `src/plugins/shared/spl_vers.h:29-30`).
5. **`src/plugins/shared/`**: no edits. `LAST_VERSION_OF_SALAMANDER` (106)
   changes only with the plugin API; a new consumer plugin does not touch it.
   There is no shared plugin-name list.
6. **Translations** (features 038/039): the module set is derived from
   `plugins.cfg` (`src/vcxproj/build_langs.ps1:224-231`,
   `tools/translate/config.py:30,234`) — no `modules.cfg`. Required:
   - `tools/translate/uicontext.py` `_DOMAINS` (`:57-90`) gains
     `"<name>": "a source-code file viewer with syntax highlighting"` (mdview at `:64`);
     without it context-aware machine translation degrades.
   - `translations/<lang>/<name>.slt` (+ `.origin` sidecar) for the 8 enabled
     languages in `translations/languages.cfg` (czech, german, french, dutch,
     hungarian, romanian, slovak, spanish); the 3 disabled ones (russian,
     ukrainian, chinesesimplified) optionally via `--language`.
   - `translations/ui-overrides.json`: optional pins (the plugin display name
     is an identifier → pin it, per `specs/052-.../contracts/plugin-metadata-encoding.md:40-47`).
   - `languages.cfg`: no edit (per-language, not per-module).
   - **Bootstrap sequence** (the "two-stage refresh"):
     ```
     build.cmd full                                  # offline: produces plugins\<name>\lang\english.slg
     src\vcxproj\build_langs.cmd --export-templates  # stage 1, offline: English .slt template per module
     python -m translate.merge --dry-run --module <name>
     python -m translate.merge --module <name>       # stage 2, NETWORK: DeepL key in temp\deepl_key.txt + ANTHROPIC_API_KEY
     python -m translate.merge --module <name> --language ukrainian   # optional, per disabled language
     build.cmd full                                  # stage 3, offline: .slt -> <lang>.slg
     ```
     Behaviour: a **missing** `.slt` is not an error (`build_langs.ps1:315-322`
     counts `skippedNoSource`, prints "skipped (no .slt yet)" `:403-404`) — the
     user then gets a language-chooser prompt for that plugin; a **row-count /
     structure mismatch** (any new string added later without re-running
     stages 1-2) makes `translator.exe -quiet-import-slt` fail and
     `build_langs.ps1:413-418` → `build.cmd:438-441` **aborts `build.cmd
     full`** for every language. Language modules are produced only on
     `build.cmd full`; `lang_<name>.vcxproj` has no translator step.
7. **Installer**: `setup/tandemcommander.iss:103` ships the whole
   `Release_x64\*` tree recursively (`Excludes: *.pdb,*.lib,*.exp`) — no
   per-plugin entry; `plugins.ver` is generated by the build; no WebView2
   bootstrapper (Evergreen runtime is an OS component, never distributed).
   `setup/build_setup.cmd` unchanged.
8. **`tools/check_encoding.py`**: `EXCLUDED` contains `"plugins/"`
   (`:154`) — plugin code is **not scanned**; the guard still gates every build
   (`build.cmd:212-228`). The plugin therefore relies on discipline, not the
   checker (see §5).
9. **Help**: `help/` holds only the main manual (`help/src/hh/salamand/*`);
   mdview has no help pages, no `IDH_*`, no `OpenHtmlHelp`. Precedent = none.
10. **Docs/version**: `CHANGELOG.md` entry + the release bump
    (`VERSINFO_SALAMANDER_MINORB`/`VERSINFO_BUILDNUMBER` in `spl_vers.h:37,128`,
    `MyAppVersion` in `tandemcommander.iss:2`, the CLAUDE.md version line) in
    one change; `doc/third_party.txt:106-115` WebView2 note (+ a section for
    any vendored highlighter library, cf. md4c at `:99-104`); counts in
    `CLAUDE.md` (28 plugins / 76 projects / 18 on) and `architecture/02`,
    `09` drift by one; `architecture/11-webview2-integration.md:44,105,127`
    must be repointed when the helper is lifted.

---

## 5. Encoding and path contract for viewer plugins

- **What `ViewFile`/`CanViewFile` receive**: `const char* name` is the
  **full path in UTF-8** (interface ≥ 104, `doc/plugin-vnext-migration.md:25-31`,
  `:85-94`), composed by `CFilesWindow::ViewFile` from `GetPath()` + `CFileData::Name`
  in `SAL_MAX_PATH_UTF8` buffers (`src/fileswn5.cpp:702-774`, name composition `:744-760`; long paths are
  legal, `SAL_MAX_PATH_UTF8` ≈ 3 × 32767). Files inside archives arrive as a
  disk-cache temp path with `returnLock = TRUE`; the plugin must return a
  non-signalled event in `*lock` and signal it when it is done with the file
  (`spl_view.h:38-53`; mdview `viewer.cpp:171-175`, `:417-422`, `:791-797`).
- **Since feature 066 the bytes are WTF-8**: a name with an unpaired UTF-16
  surrogate carries `ED A0 80…ED BF BF` sequences
  (`specs/066-fix-surrogate-filenames/contracts/name-encoding-wtf8.md:7-21`).
  The contract explicitly leaves plugins on strict converters: "names crossing
  the plugin ABI … may carry WTF-8 … plugin-shared helpers keep strict UTF-8
  converters … no new capability is promised to plugins" (`:75-79`).
- **Plugin-side conversion**: `src/plugins/shared/splunicode.h` —
  `SplU8ToWAlloc` (`:29-40`, `MB_ERR_INVALID_CHARS`, returns NULL on WTF-8
  surrogate bytes), `SplWToU8Alloc` (`:43-54`), buffer variants (`:58-78`),
  `SplU8ToWExtAlloc` (`:96-110`: adds `\\?\` / `\\?\UNC\`, the long-path form
  for `CreateFileW`). mdview opens files with `SplU8ToWExtAlloc` +
  `CreateFileW` (`viewer.cpp:257-263`, `:576-581`) and builds the display path
  with `SplU8ToWAlloc` (`:799-806`). On a conversion failure mdview's
  `CanViewFile` returns TRUE (`:257-259`) and the window then shows a
  "Cannot open" placeholder (`:620-625`) — for the new plugin the better
  contract is **decline (`FALSE`) so the built-in viewer, which is WTF-8-aware
  through `SalCreateFile`, gets the file**.
- **Never use `-A` file APIs** on interface strings (`plugin-vnext-migration.md:46,117,138-144`);
  `MultiByteToWideChar(CP_UTF8, …)` on the *content* of the file is a separate
  matter (mdview `render.cpp:128-135`).
- **Plugin metadata** (`SetBasicPluginData` name/description, menu items) is
  normalised host-side to UTF-8 (`specs/052-.../contracts/plugin-metadata-encoding.md:13-18`);
  plugins keep passing `LoadStr` output. `LoadStr` from the `.slg` returns
  ANSI (`SalamanderGeneral->LoadStr`, `mdview.cpp:59-62`); mdview builds its
  native menu with `AppendMenuA` (`viewer.cpp:458-490`) — acceptable while
  only Latin-script languages are enabled (`languages.cfg`; the 3 non-Latin
  languages are off pending a menu rendering defect, `CLAUDE.md`). Titles go
  through `SetWindowTextW` (`viewer.cpp:530`).
- **Registry**: plugin config through `CSalamanderRegistryAbstract`
  (`mdview.cpp:147-185`); ASCII ids only; `REG_SZ` values cross the facade as
  UTF-8 (`plugin-metadata-encoding.md:25-27`).
- **Guard**: `tools/check_encoding.py` excludes `plugins/` (`:154`), so none
  of its strict rules (e.g. `acp-byte-table-on-name`, `acp-title-seed`) apply
  to the plugin; the spec should restate the rules it needs (UTF-8 in/out,
  `Spl*` helpers, W APIs, WTF-8 decline) as requirements instead.

---

## 6. App-wide colour / dark mode and what plugins can read

- **App theme setting exists**: `Configuration.ThemeMode` (`src/cfgdlg.h:328`),
  registry value `"Theme Mode"` (`src/mainwnd2.cpp:310`, read `:3095`, early
  pre-splash read `src/salamdr1.cpp:4142-4147`), Options ▸ Theme menu radio
  (`src/menu4.cpp:191-192`, handler `src/mainwnd3.cpp:3042-3053`). Modes:
  **`THEME_MODE_DEFAULT 0` / `THEME_MODE_DARK 1` only** (`src/themes.h:17-19`)
  — there is **no "follow Windows" mode** and the core never reads
  `AppsUseLightTheme`/`ShouldAppsUseDarkMode` (the only such read in the tree is
  mdview's, `viewer.cpp:436-443`). Windows High Contrast suppresses Dark
  (`IsDarkThemeActive`, `src/themes.cpp:58-65`).
- **Colour tables**: `CurrentColors` → `SchemeColors`/`DarkColors`,
  `CurrentViewerColors` → `ViewerColors`/`DarkViewerColors`
  (`src/consts.h:1315-1332`, repointed by `UpdateCurrentColorsForTheme`
  `src/themes.cpp:202-223`; dark palettes single-sourced in
  `src/common/themes_palette.h`). The internal viewer has exactly 4 colours
  with light/dark variants (§3) — **no syntax-token colour model exists in the
  core**.
- **Plugin API** (`src/plugins/shared/spl_gen.h:3465-3523`, interface 105/106):
  `IsDarkThemeActive()` (`:3480`, any thread), `GetThemeSysColor/-Brush`
  (`:3487`, `:3493`), `ThemeApplyToDialog` (`:3499`), `ThemeApplyToTopLevel`
  (`:3505`, dark DWM title bar), `ThemeHandleCtlColor` (`:3513`),
  `ThemeSubclassPropSheetFrame` (`:3523`); `GetCurrentColor(SALCOL_*)`
  (`:1559`, ids `:314-351` incl. `SALCOL_VIEWER_FG_NORMAL 30 … BK_SELECTED 33`,
  dark-aware via `src/zip.cpp:1648-1780`). **No `SALCFG_*` id for the theme**
  (`spl_gen.h:425-488`). WinLib dialogs: one call `SetupWinLibTheme()`
  (`src/plugins/shared/winliblt.h:47`, `winliblt.cpp:83`).
- **Change notification**: `PLUGINEVENT_COLORSCHANGED` (`spl_base.h:437-442`)
  is fired on every theme flip (`SetThemeMode` `src/themes.cpp:1405-1438` →
  `ColorsChanged` `src/salamdr1.cpp:3139-3170`, `Plugins.Event` at `:3169`),
  but the documented contract is "theme is read at window creation; the core
  does not push live repaints; reopen adopts"
  (`specs/036-plugin-dark-theme/contracts/plugin-theme-api.md:72-75`).
- **What mdview does**: plugin-local scheme with an opt-in follow-system mode
  in which the app Dark theme wins over the Windows setting (§2.5); ignores
  `PLUGINEVENT_COLORSCHANGED`; native menus and the DWM title bar follow
  `IsDarkThemeActive()` at creation (`viewer.cpp:195`, `:826`, `darkmenu.h:10-12`).
- **Established plugin pattern**: `SetupWinLibTheme` in the entry point; raw
  dialog procs with `ThemeHandleCtlColor` + `ThemeApplyToDialog` (SFTP
  `src/plugins/sftp/dialogs.cpp:81-89`); `ThemeApplyToTopLevel` on viewer
  frames (pictview `pictview.cpp:1704,1833`, dbviewer `dbviewer.cpp:661`);
  `GetThemeSysColor` instead of `GetSysColor` in custom drawing (dbviewer
  `renpaint.cpp:27,132,144,180`); `darkmenu` only in mdview.

---

## 7. Existing viewer registrations (mask overlap map)

All `AddViewer` calls in `src/plugins/*` (enabled state from `plugins.cfg`):

| Plugin (state) | Masks (install; `TRUE` = upgrade adds) | `CanViewFile` | Reference |
|---|---|---|---|
| **mdview** (on) | `*.md;*.markdown` | binary sniff | `src/plugins/mdview/mdview.cpp:234` |
| **dbviewer** (on) | `*.csv;*.dbf` | — (accepts) | `src/plugins/dbviewer/dbviewer.cpp:582` |
| **pictview** (on) | 12 rows: `*.psp*;*.dtx;*.dds;*.nef;*.crw;*.eps;*.ept;*.ai;*.raf;*.mov;*.hpi` / `*.pntg;*.thumb;*.tiff;*.wbmp;*.ani;*.clk;*.mbm;*.thm;*.zno;*.mng` / `*.st;*.cals;*.itiff;*.jfif;*.jpeg;*.macp;*.mpnt;*.paint;*.pict;*.2bp` / `*.stw;*.sun;*.tga;*.tif;*.udi;*.web;*.wpg;*.xar;*.zbr;*.zmf;*.bw` / `*.psd;*.pyx;*.qfx;*.ras;*.rgb;*.rle;*.sam;*.scx;*.sep;*.sgi;*.ska` / `*.pat;*.pbm;*.pc2;*.pcd;*.pct;*.pcx;*.pgm;*.pic;*.png;*.pnm;*.ppm` / `*.jff;*.jif;*.jmx;*.jpe;*.jpg;*.lbm;*.mac;*.mil;*.msp;*.ofx;*.pan` / `*.flc;*.fli;*.gem;*.gif;*.ham;*.hmr;*.hrz;*.icn;*.ico;*.iff;*.img` / `*.cdt;*.cel;*.clp;*.cit;*.cmx;*.cot;*.cpt;*.cur;*.cut;*.dcx;*.dib` / `*.82i;*.83i;*.85i;*.86i;*.89i;*.92i;*.awd;*.bmi;*.bmp;*.cal;*.cdr` / `*.arw;*.blp;*.cr2;*.dng;*.orf;*.pef` (+ upgrade rows `:1050-1135`) | probes only `.scr .pct .pic .pict .img .eps .ept .ai .mov .msp .cdr .cdt .sep` and `.psp*`; **accepts everything else blindly** | `src/plugins/pictview/pictview.cpp:1037-1047`, `:1989-2045` |
| **peviewer** (on) | `*.cpl;*.dll;*.drv;*.exe;*.ocx;*.spl;*.sys;*.scr` (+ `*.scr` TRUE) | PE header check | `src/plugins/peviewer/peviewer.cpp:265-269`, `:444` |
| **uniso** (on) | `*.bin;*.img;*.iso;*.isz;*.nrg;*.pdi;*.cdi;*.cif;*.ncd;*.c2d;*.mdf` (+ `*.dmg`, `*.isz` TRUE) | opens as ISO | `src/plugins/uniso/uniso.cpp:395-438`, `:962` |
| **7zip** (on) | `*.nrg;*.pdi;*.cdi;*.cif;*.ncd` (TRUE), `*.c2d` (TRUE); `*.7z` line commented out | commented out | `src/plugins/7zip/7zip.cpp:607-648`, `:1437` |
| **tar** (on) | `*.rpm` | — | `src/plugins/tar/tardll.cpp:289` (also the hard-coded default row, `src/mainwnd1.cpp:407-412`) |
| mmviewer (off) | audio/video masks (`*.wav;*.wave;*.wma;*.ogg` …) | — | `src/plugins/mmviewer/mmviewer.cpp:491-526` |
| demoplug / demoview (off) | `*.dop;*.dop2;*.dmp2` / `*.dmv` | — | `demoplug.cpp:732-798`, `demoview.cpp:253` |
| built-in | `*.*` (always last by default) | never declines | `src/mainwnd1.cpp:414-419` |

Overlaps a source/config viewer must decide about:

- `*.md;*.markdown` — mdview (rendered Markdown). Leave to mdview; the new
  plugin should not register them (or register and rely on ordering — but
  mdview's rows would be *below* if installed earlier, so the new plugin would
  shadow it). Recommend: exclude.
- `*.csv` — dbviewer (table view). Recommend: exclude (`*.dbf` is binary).
- `*.eps;*.ept;*.ai` — pictview (PostScript / Illustrator are text with a
  preview; pictview only accepts when a thumbnail exists, otherwise cascades).
  Recommend: exclude or accept the cascade.
- Plausible source-code extensions that pictview claims and **accepts blindly**:
  `*.st` (Smalltalk), `*.icn` (Icon), `*.web` (WEB/CWEB literate source),
  `*.sam`, `*.mac`, `*.pat`, `*.cut`, `*.cel`, `*.pic`, `*.img`, `*.pan`,
  `*.sun`, `*.ras`, `*.cal`. Since the new plugin's rows will be above
  pictview's only if it is installed later (§1.6), and pictview never declines
  those, the winner is order-dependent. Recommend: do not register them (or
  register knowingly and state the priority rule).
- `*.spl;*.dll;*.exe;*.sys;*.scr` — peviewer; irrelevant (binary).
- `*.bin;*.img` — uniso/7zip; `*.bin` is sometimes used for text dumps but is
  binary by convention — exclude.
- `*.rpm` — tar; exclude.
- Everything else (`*.c *.cpp *.h *.js *.ts *.py *.java *.php *.yml *.yaml
  *.toml *.ini *.json *.xml *.html *.css *.sql *.sh *.ps1 *.bat *.cmd *.reg
  *.txt …`) is currently caught by the built-in `*.*` row only.

---

## Implications for the spec

1. **Registration model** — the plugin registers its masks in `Connect` with
   `AddViewer(row, FALSE)` in **many short rows (≤ ~200 bytes each)**; a row
   longer than 259 bytes is silently truncated in the Options page and
   **breaks the user's whole Viewers list on the next start** (§1.8). State
   the row-length cap as a requirement and require an upgrade section
   (`CURRENT_CONFIG_VERSION`, `AddViewer(new, TRUE)`, `ForceRemoveViewer`) for
   any later mask change.
2. **Priority is list order** — plugin rows are inserted at the top at
   installation (above `*.*` internal), so the plugin becomes the F3 viewer
   for its masks automatically; there is no separate "primary viewer" switch.
   The user re-orders/deletes rows in Options ▸ Viewers; Alt+F3 (separate
   list, default `*.*` internal) and View With… (bypasses masks and
   `CanViewFile`) always reach the built-in viewer. The spec should name these
   as the escape hatches instead of inventing a plugin-side "disable" option.
3. **Runtime decline is available and should be used**: `CanViewFile` must
   return FALSE for binary content, unreadable files, names that fail strict
   UTF-8 conversion (WTF-8 surrogates), and — a policy decision — files above
   the plugin's size gate, so the built-in streaming viewer (no size limit)
   takes over via the cascade; keep it cheap (first N KB) and dialog-free.
4. **Default priority among plugins depends on the directory name** (§1.6):
   the spec must fix the plugin directory/module name knowing that a name
   sorting after `pictview` lands above pictview's rows for colliding
   extensions and vice versa; better still, avoid the overlaps in §7.
5. **Shared-engine contract is binding** (`architecture/11-webview2-integration.md`):
   canonical UDF, the single options helper, per-controller lockdown, own
   keeper armed at first use only, `KeepReady`-style opt-out; and the
   **second consumer lifts the helper (and preferably keeper + host) into
   `src/common/`** rather than copying — this is a refactor of mdview inside
   the same feature (mdview's vcxproj/props and `architecture/11` must change).
   The keeper window class name must become plugin-neutral or unique per
   plugin.
6. **Security posture**: the new content is untrusted text; keep mdview's
   lockdown as the baseline. If a JavaScript highlighter library is used the
   spec must explicitly relax `IsScriptEnabled` for *this* plugin's controllers
   (allowed by contract §2.3) and state the compensating controls (default-deny
   interception stays, no remote loads, no web messages unless needed, no host
   objects). If highlighting is done natively (C++ tokeniser → HTML), scripts
   stay off and `MdBuildSourceHtml` is the fallback path already proven.
7. **Colour themes**: no app-level syntax palette exists; a highlighter theme
   is necessarily plugin-local. Decide whether to mirror mdview (explicit
   scheme + optional follow-system where the app Dark theme wins) and whether
   to react to `PLUGINEVENT_COLORSCHANGED` (mdview does not; the 036 contract
   says reopen adopts). Provide a `DefaultBackgroundColor` + `WM_ERASEBKGND`
   brush from the scheme to avoid the white flash.
8. **Encoding**: parity with the built-in viewer requires at least BOM /
   UTF-8 / UTF-16 LE+BE / ANSI fallback (mdview `MdDetectDecode` covers this),
   plus a decision on user-selectable code pages (built-in viewer has the full
   `CCodeTables` menu; mdview has none). Long paths via `SplU8ToWExtAlloc`;
   never `-A` file APIs; the checker does not cover plugin code.
9. **Size gate**: state it explicitly (mdview: 20 MB parse gate, 64 MB read
   cap, whole file in memory). For source files a lower highlight gate and a
   "plain text above N MB, decline above M MB" rule are needed; the built-in
   viewer streams without limit.
10. **Parity list** for the spec's functional requirements (from §3): find
    (with case/whole-word; regex optional), find next/prev, select/copy/
    select-all, zoom, wrap toggle, go-to-line (new, natural for code), next/
    previous file in panel (`GetNextFileNameForViewer`, `enumFilesSourceUID`;
    `MAX_PATH` buffer in the API, see open question), remembered placement,
    always-on-top, encoding shown in the title, dark title bar/menus, Esc
    closes. Not needed for parity: hex mode, offset tooltip, drag-out.
11. **Repo integration** (§4): `plugins.cfg`, `salamand.sln` (2 projects),
    four vcxproj/props files, `uicontext._DOMAINS`, 8 `.slt` files through the
    two-stage refresh (network step needs DeepL + Anthropic keys), `CHANGELOG`
    + version bump, `doc/third_party.txt` for any vendored library, docs
    counts. No installer, help, `check_encoding.py` or `spl_vers.h` work.
12. **Plugin ABI unchanged** — everything above is achievable at interface
    106; a core-hosted keeper service is explicitly deferred (`architecture/11:89-91`).

## Open questions

1. **Highlighting engine**: native C++ tokeniser (scripts stay off, hundreds
   of grammars = large maintenance) vs a vendored JavaScript highlighter
   (highlight.js / Prism / Shiki-style) that needs `IsScriptEnabled TRUE` on
   this plugin's controllers — which does the product accept, and what is the
   licence/third-party-notice implication (`doc/third_party.txt`)?
2. **Mask list ownership**: who curates the 200–400 extensions and the
   exclusion list in §7 (`*.md`, `*.csv`, pictview collisions), and is the
   overlap resolved by exclusion or by documented ordering?
3. **Plugin directory / module name** — needed early because it fixes the
   default priority (§1.6), the `.spl`/registry key name, the translation
   module name and the `_DOMAINS` entry.
4. **Lift scope**: options helper only (minimum required by the contract) or
   the whole `CMdWebHost` + keeper as a `src/common/` library shared by both
   plugins (then the keeper's window class and `TRACE` prefixes must be
   generalised and mdview retested)?
5. **Size gate values** for "highlight", "plain text" and "decline to the
   built-in viewer", given the built-in viewer streams any size.
6. **Next/previous file**: mdview left it unimplemented; the API's
   `fileName` buffer is documented as "at least `MAX_PATH`"
   (`spl_gen.h:2703`) — does the host write long UTF-8 paths into it (needs
   `SAL_MAX_PATH_UTF8`)? To verify in `src/zip.cpp`/`fileswnb.cpp` before
   promising the feature.
7. **Code pages**: does parity require the built-in viewer's Coding menu
   (`CCodeTables`), or is BOM/UTF-8/UTF-16/ANSI-fallback enough?
8. **Theme following**: explicit scheme only (simplest), or follow-system
   with `PLUGINEVENT_COLORSCHANGED` re-render (goes beyond the 036 contract
   and mdview's behaviour)?
9. **Files without extension** (`Makefile`, `Dockerfile`, `.env`,
   `.gitignore`): literal-name masks work (§1.8) but each costs row bytes;
   include a curated set or leave them to the built-in viewer?
10. **WTF-8 names**: decline to the built-in viewer (recommended) vs an own
    WTF-8 decoder in the plugin (contract 066 promises nothing to plugins).

# mdview — Implementation Notes (v1)

Feature 020. Spec/plan/tasks: `specs/020-mdview-plugin/`.

## What v1 implements

- Viewer plugin registered for `*.md;*.markdown` (F3). Thread-per-window +
  lock handshake adapted from `demoview`; build wiring modeled on `sftp`.
- **Rendering surface**: a standard **RichEdit 4.1** control fed generated RTF
  (research.md D-R1). Realizes the clarified "static, script-free native
  renderer" — the security invariants (FR-040…046) hold by construction (no
  script engine, no HTML DOM, no network in the rendering path). Selection,
  Ctrl+C/A, in-document search (EM_FINDTEXTEXW) and zoom (EM_SETZOOM) are native.
- **Parser** (`render.cpp`): self-contained block+inline Markdown parser →
  document → RTF. Covers headings, paragraphs, emphasis/strong/strike, inline
  code, fenced+indented code blocks with info string, blockquotes, ordered/
  unordered/nested/task lists, thematic breaks, links, images (placeholder),
  autolinks, GFM pipe tables, entities/escapes, YAML front matter, and the
  degradation rules.
- **Syntax highlighting** (`highlight.cpp`): best-effort lexical highlighter for
  the tier-1 languages + alias table; diff coloring; unknown → plain block.
- **10 color schemes** (`render.cpp` themes table), contrast-corrected per
  analysis/visual.md; live switch via View → Color Scheme (+ F9/Shift+F9),
  persisted; Follow-System-Theme mode (AppsUseLightTheme).
- **Encoding** (FR-050): BOM / strict-UTF-8 / UTF-16 / ANSI(CP_ACP) fallback;
  binary → hand off to text viewer.
- **Link security gate** (`ActivateLinkByCp`): `#anchor` scroll; local `.md` →
  new mdview window; http/https/mailto/ftp → ShellExecute; everything else
  blocked. No network on open; remote images are placeholders.
- **Fallback**: `CanViewFile` declines unreadable/binary → internal text viewer;
  size gate (20 MB) + "Open as Text"; long-path safe (`SplU8ToWExtAlloc`,
  no fixed MAX_PATH).

## Documented deviations from the spec (v1 scope)

1. **Parser is a pragmatic subset**, not full CommonMark 0.31.2 (FR-010). md4c
   (MIT, single-file) is the production upgrade behind the same
   parser→document boundary (research.md D-R2). Edge cases of emphasis
   precedence, reference-link definitions, and tight/loose list nuances are
   approximate.
2. **Images render as labeled placeholders** (FR-020/022 inline display
   deferred). Inline raster via WIC + SVG via the vendored nanosvg is the
   documented follow-up; the relative-path-only / no-remote-fetch security
   rules are already enforced.
3. **Tables** render as aligned monospaced text (not an RTF grid).
4. **RichEdit** instead of a from-scratch DirectWrite renderer (Complexity
   Tracking in plan.md); a custom renderer remains the path to pixel-perfect
   tables/images.
5. **Next/previous-file** (Space/Backspace) and the encoding **warning bar**
   are deferred (encoding shown in the title bar instead).
6. Contrast values are hand-tuned; the automated FR-061 gate check is a
   documented follow-up.

## Key files (v1)

`mdview.cpp` entry/interface/config · `viewer.cpp/.h` window+RichEdit ·
`render.cpp/.h` parser+themes+RTF · `highlight.cpp` lexer · resources in
`*.rh2`/`*.rc`/`lang/`.

---

# v2 — Feature 021: WebView2 HTML rendering surface

Spec/plan/tasks: `specs/021-mdview-html-renderer/`. Analysis:
`specs/020-mdview-plugin/analysis/html-renderer.md`. The v1 RTF/RichEdit path
was **retired** (single HTML backend, FR-038a). Feature-020 Decisions Q1/Q2 were
formally amended (owner-ratified) to allow rendered raw HTML + a browser-class
engine.

## What v2 implements

- **Pipeline**: file → `MdDetectDecode` (kept) → UTF-16 → UTF-8 → **md4c**
  (vendored `src/common/dep/md4c/`, MIT, `MD_DIALECT_GITHUB`, raw HTML NOT
  suppressed) → custom `MD_PARSER` renderer **`htmlgen.cpp`** → self-contained
  HTML document + per-theme CSS → **WebView2** (`webview.cpp` `CMdWebHost`).
- **Rendering surface**: embedded WebView2 (Evergreen, Win11 OS component). SDK
  vendored at `src/common/dep/webview2/` (headers + `WebView2LoaderStatic.lib`
  x86/x64, v1.0.4078.44, BSD-3). Runtime is an OS component (not distributed).
- **Security lockdown** (FR-050..057, by configuration + test): scripts off;
  context-menu/devtools/status-bar/error-page off; browser-accel-keys off; zoom
  control off; autofill/password/SmartScreen/host-objects/web-message/pinch/
  swipe off; `--disable-background-networking`. Content served from a private
  virtual host `https://mdview.invalid/` via `WebResourceRequested` with an
  `AddWebResourceRequestedFilter("*")` **default-deny** net (no content-triggered
  network). `NavigationStarting`/`NewWindowRequested` cancel all navigation
  except the document; `ProcessFailed` → text-viewer fallback.
- **Raw HTML** rendered natively (FR-020); **no sanitizer** (FR-022) — safety is
  the lockdown. Text is HTML-escaped (the XSS boundary); `MD_TEXT_HTML`/
  `MD_BLOCK_HTML` are emitted verbatim.
- **Tables** = real grids with per-column alignment; **margins/reading measure**
  (`max-width:46rem`, full-width toggle); **images**: local relative served by
  the interceptor, remote blocked + placeholder until per-document consent
  (View → Load Remote Images), absolute/UNC/traversal refused; **syntax
  highlighting** via `highlight.cpp` → `hl-*` CSS classes.
- **Parity**: zoom (`put_ZoomFactor`, persisted), 10 schemes + follow-system
  (F9), script-free **find** (`<mark id="mdfind-N">` + `#fragment`, `mark:target`
  CSS), accelerators routed via `add_AcceleratorKeyPressed` → `CM_*`, link gate
  (internal `#anchor` native; local `.md` → new window; other local → path-only;
  http/https/mailto → ShellExecute, **no ftp**), encoding/long-path/size-gate/
  `OpenAsText` kept. Engine unavailable/init-fail → error + text viewer.

## Build integration

`mdview.props`: WebView2 include + `lib\$(ShortPlatform)` dir +
`WebView2LoaderStatic.lib;shlwapi.lib;ole32.lib;winhttp.lib;version.lib`; WINVER
raised to `0x0A00`. `mdview.vcxproj`: `md4c.c` (NotUsing PCH + ObjectFileName),
`htmlgen.cpp`, `webview.cpp`. No changes to sln/slnf/plugins.cfg/build.cmd.
**Note**: `<wrl.h>`/WebView2 headers are included in `webview.cpp` with the debug
`new` macro suspended (`#pragma push_macro/#undef new`) — WRL's implements.h is
incompatible with the leak-tracking macro.

## Tests

`tests/mdview_htmlgen_test/` — standalone console harness (`test_main.cpp`, 25
assertions: tables/alignment, slugs, code+lang, lists/tasks, escaping, raw-HTML
pass-through, local-image rewrite, remote block + consent, find marks, wrapper/
CSS) + `dump_main.cpp` (md → html dumper) + `sample.md`. Build per
`specs/021-mdview-html-renderer/quickstart.md`. All 25 pass. Debug x64 `mdview.spl`
+ `english.slg` build clean.

## v2 follow-ups / known limitations

- **Ctrl+wheel zoom** not wired (keyboard zoom works); Ctrl+C/Ctrl+A rely on
  WebView2 native handling (menu Copy/Select-All items were removed).
- Scheme/consent change re-navigates (scroll resets); scroll-restore is a
  follow-up.
- Remote-image consent is per-document/session (no global) and fetches via
  WinHTTP; consent is a View-menu toggle.
- **Runtime GUI verification (F3 in the app)** is the one manual step — as with
  every prior mdview feature — pending a human at the keyboard.

---

# v2.1 — Feature 022: viewer UX fixes

Spec/plan/tasks: `specs/022-mdview-viewer-ux-fixes/`. Six defects found in
feature-021 GUI testing, all fixed in `webview.{h,cpp}` + `viewer.{h,cpp}`:

1. **Keyboard scroll after F3** — WebView2 content now receives focus via
   `controller->MoveFocus(PROGRAMMATIC)` at controller-ready, on every
   `NavigationCompleted`, and on the window's `WM_SETFOCUS`. Arrows/PgUp/PgDn
   work without a click.
2. **Ctrl+0 on the numeric keypad** — `AcceleratorKeyPressed` and the frame
   accelerator table now accept `VK_NUMPAD0` as well as `'0'` for zoom reset.
3. **Zoom percent in the title** — `UpdateTitle` appends ` (NNN%)`, kept live by
   the `ZoomFactorChanged` callback.
4. **Ctrl+mouse-wheel zoom** — `put_IsZoomControlEnabled(TRUE)` lets the engine
   handle Ctrl+wheel and Ctrl+±; `add_ZoomFactorChanged` syncs `g_zoom` + title.
   The accelerator handler no longer intercepts Ctrl+± (engine owns them); it
   still owns Ctrl+0 reset (a browser-accel key disabled by the lockdown).
5. **Search** — the v021 double-navigation race is gone. `CMdWebHost` tracks a
   `docVersion` (bumped in `SetDocument`); `Navigate` builds
   `doc.html?v=<version>[#frag]`, and the interceptor matches the document by
   path (ignoring `?`/`#`). A new term reloads with fresh `<mark>`s; find
   next/prev is a same-document `#fragment` scroll. `DoFind` rewritten to a
   single navigation.
6. **Open as Text (Ctrl+U)** — root cause: `ViewFileInPluginViewer` is
   main-thread-only but the viewer runs in its own thread. Replaced with a
   robust in-window **View Source** toggle (`SourceMode` → `MdBuildSourceHtml`
   emits the raw text as an escaped `<pre>`; find/zoom still work). The internal
   text viewer is now only used for the engine-unavailable fallback, invoked
   from `ViewFile` on the main thread (which also lets the size-gate show huge
   files cheaply as source). The unsafe cross-thread `OpenAsText` was removed.

Tests: `tests/mdview_htmlgen_test/` extended to 29 assertions (source view +
find-in-source). Debug x64 builds clean. GUI smoke test pending user.

---

# v2.2 — Feature 065: instant display (session keeper)

Spec/plan/tasks: `specs/065-mdview-instant-render/`. Root cause of the
slow first open: the first WebView2 controller creation spawns the shared
`msedgewebview2.exe` tree (cold), and the tree exits when the last viewer
window closes — so the cost recurred in one-document-at-a-time work.

## What v2.2 implements

- **Session keeper** (`webview.cpp`, `MdKeeperArm/Disarm/Armed` in
  `webview.h`): at the **first actual view** (`ViewFile`, main thread) a
  hidden `WS_EX_TOOLWINDOW` window + environment + controller is created
  asynchronously on the main thread and held for the rest of the session,
  so every later view attaches warm. Idle footprint shrunk via
  `put_IsVisible(FALSE)` + best-effort `TrySuspend` (`ICoreWebView2_3`) and
  `MemoryUsageTargetLevel(LOW)` (`ICoreWebView2_19`). Failures are silent;
  `ProcessFailed`/`BrowserProcessExited` → quiet teardown, re-arm at the
  next view; a `gen` counter invalidates stale async completions. Disarm:
  config toggle-off, `CPluginInterface::Release`, process exit. Nothing
  runs before the first view (zero cost for Markdown-free sessions).
- **Shared-engine contract** (feature 065 R9,
  `architecture/11-webview2-integration.md`): user data folder renamed to
  the app-neutral `%LOCALAPPDATA%\Tandem Commander\WebView2`
  (`MdUserDataFolder()`, moved to `webview.cpp`); environment options
  extracted into the single helper `MdBuildEnvOptions()` (viewer hosts and
  keeper share it — later environments' `AdditionalBrowserArguments` are
  silently ignored, so per-site drift would be invisible). The pre-065
  `mdview.WebView2` cache folder is deleted best-effort at the first view
  on a short-lived `CThread` (`CMdUdfJanitorThread`, registered in
  `ThreadQueue`).
- **Configuration** (FR-008): first real Configuration dialog (`IDD_CFG`
  in `lang/lang.rc2`, IDs 310/311 in `mdview.rh2`) replaces the About
  placeholder — one checkbox, default ON, persisted as `KeepReady`
  (`REG_DWORD`, clamped). Turning it off disarms immediately (with no
  viewer open the tree exits ≈ pre-065 behavior); turning it on applies
  from the next view. Dialog follows constitution VI + the feature-049
  two-touchpoint dark-theme pattern.
- **Timing instrumentation** (R8): Debug `TRACE_I` at `ViewFile`,
  controller-ready, `NavigationCompleted`, and keeper transitions.
- **Translations**: stage-1/2/3 pipeline run for module mdview — the new
  dialog's 4 strings are `english_fallback` in the 8 enabled languages
  (recorded in `mdview.origin`); `python -m translate.merge --module mdview`
  with a DeepL key in `temp/deepl_key.txt` machine-translates exactly these
  gaps. Stage-3 import verified (8 language modules build).

## v2.2 verification status

Debug x64 + `build.cmd full release` build clean; `.slt` round-trip
byte-exact (290 files). **Runtime GUI verification (F3 timing, toggle,
crash recovery, footprint) is the one manual step** — protocol and result
tables in `specs/065-mdview-instant-render/baseline.md` (the pre-065
baseline is measured with the option OFF, which reproduces the old
lifecycle exactly). The `tests/mdview_htmlgen_test/` inputs (htmlgen,
render, highlight, md4c) are untouched by this feature; its `.vcxproj` has
never been committed (only the sources), a pre-existing gap.

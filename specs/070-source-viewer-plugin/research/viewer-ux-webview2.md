# Research — Viewer experience inside WebView2 (feature 070, source-code viewer)

**Date**: 2026-08-26 · **Scope**: the seven viewer-experience questions a read-only,
syntax-highlighting code viewer hosted in WebView2 raises, researched so the
specification can state testable requirements.
**Inputs**: `src/plugins/mdview/{webview,viewer,render,htmlgen}.cpp` (reference
host), `architecture/11-webview2-integration.md` (shared-engine contract),
`specs/065-mdview-instant-render/{research,spec,baseline}.md`, the vendored SDK
header `src/common/dep/webview2/include/WebView2.h` (v1.0.4078.44), the
built-in text viewer (`src/viewer*.cpp`, `src/codetbl.cpp`, `src/salamand.rc`),
Microsoft Learn WebView2 reference pages, VS Code / GitHub / Shiki /
highlight.js / Prism / CodeMirror sources and docs.

**Evidence legend** used throughout:
- **[V]** verified — read in the cited source (repo file:line, SDK header line, or URL fetched on 2026-08-26)
- **[M]** from memory — plausible, not re-verified; treat as a claim to check
- **[T]** to be measured — no number exists yet; the spec should demand a measurement

---

## 0. Local baseline the spec inherits (what mdview does today)

All **[V]** unless marked.

- **Rendering host** `src/plugins/mdview/webview.cpp`: one in-memory document
  served from the private virtual host `https://mdview.invalid/doc.html`
  (`kBase`, line 29) through `WebResourceRequested` with an
  `AddWebResourceRequestedFilter("*", ALL)` (line 360); every other URL gets
  `403 Forbidden` (`ServeRequest`, lines 214–256). `NavigationStarting` cancels
  everything but the document (+`#fragment`) and hands the URL to the owner
  (lines 314–331); `NewWindowRequested` is always handled (333–348);
  `ProcessFailed` → text-viewer fallback (362–371).
- **Settings lockdown** (lines 282–311): `IsScriptEnabled=FALSE`,
  default context menus off, DevTools off, status bar off, built-in error page
  off, `IsZoomControlEnabled=TRUE` (engine owns Ctrl+wheel/Ctrl+±),
  `IsWebMessageEnabled=FALSE`, `AreHostObjectsAllowed=FALSE`,
  `AreBrowserAcceleratorKeysEnabled=FALSE`, autofill/password-save off,
  pinch zoom off, swipe navigation off, `IsReputationCheckingRequired=FALSE`.
- **Accelerators**: `add_AcceleratorKeyPressed` (lines 402–443) maps
  F3/Shift+F3, Esc, F9/Shift+F9, Ctrl+F, Ctrl+U, Ctrl+0/Numpad0 to `CM_*`
  commands, sets `Handled(TRUE)` and `PostMessage`s `WM_COMMAND` to the owner
  window. A parallel Win32 accelerator table exists for the frame
  (`viewer.cpp` lines 94–106).
- **Background before paint**: `put_DefaultBackgroundColor`
  (`ICoreWebView2Controller2`, `ApplyBackgroundColor` lines 261–268) is pushed
  **before** `put_IsVisible(TRUE)` (line 449) with the theme `docBg`; the
  window's `WM_ERASEBKGND` brush (`BgBrush`, `viewer.cpp` 539) covers runtimes
  without Controller2.
- **Focus**: `MoveFocus(PROGRAMMATIC)` at controller-ready and on every
  `NavigationCompleted` so keys scroll without a click (lines 375–385, 451).
- **Zoom**: `put_ZoomFactor(pct/100)`; `ZoomFactorChanged` syncs the
  persisted percent and the title (lines 389–399, 448, 546–551).
- **Find**: script-free — the HTML is regenerated with `<mark id="mdfind-N">`
  around every case-insensitive match (`htmlgen.cpp` 185–199, 687) and the
  document is re-served under `?v=<docVersion>` (`Navigate`, lines 568–586);
  next/previous is a same-document `#mdfind-N` fragment scroll
  (`viewer.cpp` `DoFind` 672–704). A new term therefore **reloads the
  document** (scroll position resets).
- **Size gate**: `SIZE_GATE = 20 MB` (`viewer.cpp` line 20). Above it the
  Markdown path is skipped and the file is shown as escaped source in a
  `<pre>` (`SourceMode`), the read capped at 64 MB (lines 588–607). Below it
  the whole file is read into memory and rendered. `CanViewFile` (255–275)
  reads 512 bytes and declines (→ internal text viewer) when a NUL byte is
  present and there is no UTF-16 BOM.
- **Encoding**: `MdDetectDecode` (`render.cpp` 137–173): UTF-8 BOM → UTF-8;
  `FF FE` → UTF-16 LE (bytes reinterpreted); `FE FF` → UTF-16 BE (swapped);
  a NUL in the first 4 KB → binary; structurally valid UTF-8
  (`ValidUtf8`, 108–126 — checks lead/continuation shape only, **not**
  overlong/surrogate/range) → `CP_UTF8`; else `CP_ACP` ("CP1250 on Czech
  Windows"), always via `MultiByteToWideChar` without
  `MB_ERR_INVALID_CHARS` (invalid bytes silently become U+FFFD). The title
  shows ` [ANSI]` / ` [UTF-16]` (`UpdateTitle` 514–530). Text reaches the
  page only HTML-escaped (`Esc`, `htmlgen.cpp` 53–69).
- **Keeper / warm engine** (lines 671–901; contract §2): hidden
  `WS_EX_TOOLWINDOW` window on the **main thread**, one environment +
  controller with the shared options helper `MdBuildEnvOptions` (37–44),
  never navigates, `TrySuspend` + `MemoryUsageTargetLevel(LOW)` best-effort.
  Viewer windows run **one thread each** (`CViewerThread`, `viewer.cpp`
  319–325) and create their own environment + controller on that thread.
- **What 065 measured**: nothing yet — `baseline.md` tables are blank
  ("___ ms"); the 065 spec's working definition of "instant" is *"the empty
  stage is not consciously perceived (on the order of 200–300 ms or less)"*
  (`spec.md` lines 244–249). **[T]** The first 070 task that touches timing
  must fill these numbers using the R8 `TRACE_I` points (`ViewFile` →
  `controller ready` → `navigation completed`).
- **SDK 1.0.4078.44 header facts** (`WebView2.h`): highest core interface
  `ICoreWebView2_29` (line 33914); `ICoreWebView2Find` (49591),
  `ICoreWebView2FindOptions` (50030), `ICoreWebView2_28::get_Find` (32685),
  `ICoreWebView2Environment15::CreateFindOptions` (48099); `PrintToPdf`
  (9302, `ICoreWebView2_7`), `Print`/`ShowPrintUI`/`PrintToPdfStream`
  (18383–18390, `ICoreWebView2_16`); `add_ContextMenuRequested` (13062,
  `ICoreWebView2_11`); `put_DefaultBackgroundColor` (39896,
  `ICoreWebView2Controller2`); `put_PreferredColorScheme` (59142,
  `ICoreWebView2Profile`); `put_ProfileName` (41002,
  `ICoreWebView2ControllerOptions`); `ICoreWebView2AcceleratorKeyPressedEventArgs2`
  (35406, per-key `IsBrowserAcceleratorKeyEnabled`); `PostSharedBufferToScript`
  (19598) + `CreateSharedBuffer` (46999) + `ICoreWebView2SharedBuffer` (66689);
  `add_DownloadStarting` (6902), `add_LaunchingExternalUriScheme` (20646),
  `add_SaveAsUIShowing` (28660), `add_SaveFileSecurityCheckStarting` (30051),
  `add_ScreenCaptureStarting` (31359), `ExecuteScriptWithResult` (23974). The
  header contains **no** size-limit text for `NavigateToString` /
  `PostWebMessage*` / `ExecuteScript` (grep for "2 MB", "larger than",
  "size limit": no matches).

---

## 1. Large files

### What the reference tools actually do

| Tool | Gate / behaviour | Evidence |
|---|---|---|
| **VS Code** | A model is *too large for tokenization* when **> 20 MB** (`LARGE_FILE_SIZE_THRESHOLD = 20*1024*1024`) **or > 300 000 lines** (`LARGE_FILE_LINE_COUNT_THRESHOLD = 300*1000`); then the notification *"tokenization, wrapping, folding, codelens, word highlighting and sticky scroll have been turned off for this large file in order to reduce memory usage and avoid freezing or crashing"* is shown. `editor.largeFileOptimizations` default **true**. `LONG_LINE_BOUNDARY = 10000` decides whether a model is "dominated by long lines". `MODEL_SYNC_LIMIT = 50 MB` (not synced to extensions), `LARGE_FILE_HEAP_OPERATION_THRESHOLD = 256 M chars`. | [V] `textModel.ts`, `largeFileOptimizations.ts`, `textModelDefaults.ts` (raw GitHub, 2026-08-26) |
| **VS Code** | `editor.maxTokenizationLineLength` default **20 000** ("Lines above this length will not be tokenized for performance reasons"); `stopRenderingLineAfter` default **10 000** ("Performance guard: Stop rendering a line after x characters"); `editor.experimental.asyncTokenization` default **true** — tokenization runs on a web worker. Everything below the gates is rendered through **viewport virtualisation** (only visible lines are in the DOM). | [V] `editorConfigurationSchema.ts`, `editorOptions.ts`; virtualisation [M] (VS Code architecture, not re-read) |
| **GitHub web UI** | *"Text files over 1 MB are always displayed as plain text. Code is not syntax highlighted, and prose files are not converted to HTML."* *"Text files over 5 MB are only available through their raw URLs."* Diffs: max 20 000 lines / 1 MB raw per file, 400 lines / 20 KB auto-loaded. | [V] docs.github.com "Repository limits" / enterprise "Limits for viewing content and diffs" (search summary 2026-08-26). A **512 KB** figure I remembered could not be confirmed in current docs — treat GitHub's highlight gate as **0.5–1 MB**. |
| **Shiki** (TextMate grammars, Oniguruma or JS-RegExp engine) | `tokenizeTimeLimit` default **500 ms per line** ("Time limit in milliseconds for tokenizing a single line"); `tokenizeMaxLineLength` default **0 = no limit**. The highlighter instance is *"expensive to create … create once and reuse (singleton)"*. Full bundle **6.4 MB minified / 1.2 MB gzip**, web bundle **3.8 MB / 695 KB**; fine-grained bundles recommended; grammars load lazily. Benchmark (Zenn migration article, as summarised): Prism fastest, highlight.js ≈ ½ Prism, **Shiki ≈ 7× slower than Prism**, server memory 120 MB (Prism) vs ~512 MB (Shiki). | [V] `packages/types/src/tokens.ts`, shiki.style *bundles*/*best-performance*/*regex-engines*; benchmark numbers [V] only as a search summary of zenn.dev article |
| **highlight.js** | Whole-block regex highlighter; README: *"You can run highlighting inside a web worker to avoid freezing the browser window while dealing with very big chunks of code."* GitLab's chunked approach (70-line chunks, first chunk immediately, rest deferred) drew the maintainer's warning: *"several grammars match patterns ACROSS line boundaries … Asking us to highlight content in 70 line chunks is eventually going to break"* — a chunk entered mid-string can be rendered "backwards". Recommended instead: split **after** highlighting, offload to a worker. Issue #954 ("Windows freeze with a huge code block") is the canonical freeze report. | [V] README, GitLab issue #366531 (maintainer quote), hljs #954 (symptom only) |
| **Prism** | Web-worker mode exists but is **off by default**: workers avoid blocking "really large code blocks" but are *slower* (worker creation) and lose plugins. | [V] prismjs.com FAQ |
| **CodeMirror 6** | Handles *"a few million lines"*: only the viewport is rendered; *"The parser contains logic that limits the amount of work it does to avoid wasting too much battery and memory"*; highlighting "stops at some point if you scroll down far enough" and catches up when idle. | [V] codemirror.net/examples/million |
| **Monaco** | Same engine as VS Code (viewport rendering, same tokenization gates). | [M] |

### What full-DOM highlighting costs (order-of-magnitude, **[T]** until measured on the target machine)

A class-based highlighter emits roughly one `<span>` per token; source code averages ~5–8 characters per token, so **1 MB ≈ 150–200 k spans, 5 MB ≈ 0.8–1 M spans, 20 MB ≈ 3–4 M spans** **[M]**. Chromium's per-node cost (element + text node + layout box) is on the order of a few hundred bytes plus style/layout work proportional to node count, so a fully highlighted 1 MB file lands in the **tens of MB of renderer memory and a few hundred ms of layout**; 5 MB is seconds and hundreds of MB; 20 MB is not viable as a single DOM **[M]** — this is exactly why GitHub stops at ~1 MB and every editor virtualises. Tokenization itself is the smaller part for hljs/Prism (tens of ms per 100 KB) and larger for TextMate grammars (Shiki: 7× Prism); regex backtracking on pathological input (minified single-line files, unterminated strings) is unbounded, which is what `tokenizeTimeLimit`/`maxTokenizationLineLength` exist for.

**Plain text is cheap by comparison**: a `<pre>` (or one `<div>` per line) of 20 MB is a handful of nodes and Chromium paints it, but layout/line-wrapping of very long lines still costs, and scrolling a 300 k-line document with per-line elements is fine; per-line elements also give line numbers and go-to-line for free.

### Strategies and how they combine

1. **Size gate with plain-text fallback** (GitHub model) — simplest, deterministic; above the gate no tokenizer runs.
2. **Progressive / time-sliced highlighting** — first screen highlighted synchronously, the rest in idle slices (`requestIdleCallback` / worker). **Correct only with a line-stateful tokenizer** (TextMate/Shiki `codeToTokens`, CodeMirror's incremental parser, VS Code) that carries grammar state from line to line and can pause between lines. hljs/Prism cannot be chunked safely (maintainer statement above); they can only run whole-file in a worker and then the DOM insertion is the cost.
3. **Virtualised rendering** (editor model) — only visible lines in the DOM; unlimited size but: native browser **Find and Select-All see only what is in the DOM** (§2), text selection across the virtual window needs custom code, printing needs a separate path. Substantially more engineering than a viewer needs; VS Code/CodeMirror get it from being editors.
4. **"Highlight only the first N KB"** — GitHub/VS Code do *not* do this; they switch the whole file to plain. Partial highlighting looks broken at the seam; if used, it must be visibly announced ("highlighting stopped at line N").
5. **Long-line gate** — independent of file size: skip tokenizing lines > 10–20 k chars (VS Code 20 000) and treat a file whose *longest* line exceeds the gate or that is "dominated by long lines" (minified JS/CSS, single-line JSON) as plain.

### Recommended default gates for this product

The product already has two hard facts to align with: mdview's **20 MB** gate (same UDF, same user expectations) and the **internal text viewer**, which is memory-mapped and handles any size — and a plugin viewer can hand a file to it simply by returning FALSE from `CanViewFile` (mdview does exactly that for binaries). So the source viewer does not need to solve the "huge file" problem at all; it needs to *decline gracefully* and to be *honest* in the band where it renders but does not highlight.

| Band | Proposed default | Rationale |
|---|---|---|
| ≤ **1 MB** and longest line ≤ 20 000 chars | full syntax highlighting; first screen painted before tokenization completes (progressive if the tokenizer is line-stateful, else plain-first-then-highlight) | GitHub parity; comfortably within the DOM budget; VS Code tokenizes far larger files but virtualised |
| 1 MB – **20 MB** (or > 300 000 lines, or longest line > 20 000) | plain monospaced text with line numbers, themes, find, wrap; a one-line status notice "syntax highlighting is off for this file (size/line length)" | GitHub's 1–5 MB "plain" band widened to mdview's 20 MB so both WebView2 viewers agree; VS Code's 300 k-line and 20 k-char guards |
| > 20 MB | `CanViewFile` returns FALSE → internal text viewer | same gate as mdview; the internal viewer is the right tool |
| NUL/binary (per §5) | `CanViewFile` returns FALSE → internal viewer (hex) | mdview precedent |

Both numbers should be **configurable** (plugin Configuration dialog, mirroring `KeepReady`), with the 1 MB highlight gate the one users will realistically move.

### Recommended requirement(s) for the spec

- **R1.1** Opening a text file of ≤ 1 MB whose longest line is ≤ 20 000 characters shows it fully syntax-highlighted; the first screen of text is visible within the "instant" budget of §7 even if highlighting of the remainder is still in progress, and the window stays responsive to scrolling and keys throughout.
- **R1.2** Opening a text file between the highlight gate and 20 MB shows it as plain monospaced text with all non-highlighting features (line numbers, wrap, find, go-to-line, themes, zoom, copy) working, and tells the user in the viewer why it is not highlighted.
- **R1.3** A file above 20 MB, or one the plugin recognises as binary, opens in the internal text viewer exactly as if the plugin were not installed (no error, no empty window).
- **R1.4** A file consisting of a single 5 MB line (minified script) opens within the plain-text budget, scrolls horizontally, and never triggers a "page unresponsive" state; enabling word-wrap on it completes within a few seconds.
- **R1.5** Both gates are user-configurable in the plugin's configuration; the defaults are 1 MB (highlighting) and 20 MB (viewer).
- **R1.6 (measurement)** The plan records, on the development machine, time-to-first-text and time-to-fully-highlighted for 10 KB, 100 KB, 1 MB, 5 MB and a 1-line 2 MB file, plus renderer memory for each, before the gates are frozen.

---

## 2. Find in page

### Native Find API availability — **verified**

- The **Find API is stable** and **present in the vendored SDK 1.0.4078.44**: `ICoreWebView2Find` (header line 49591: `Start`, `FindNext`, `FindPrevious`, `Stop`, `get_MatchCount`, `get_ActiveMatchIndex`, `add_MatchCountChanged`, `add_ActiveMatchIndexChanged`), `ICoreWebView2FindOptions` (50030: `FindTerm`, `IsCaseSensitive`, `ShouldHighlightAllMatches`, `ShouldMatchWord`, `SuppressDefaultFindDialog`), obtained via `ICoreWebView2_28::get_Find` (32685) and `ICoreWebView2Environment15::CreateFindOptions` (48099). [V]
- Promoted to *Phase 3: Stable in Release* in **SDK 1.0.3405.78 (Aug 11 2025)**; *"For full API compatibility, this Release version of the WebView2 SDK requires WebView2 Runtime 139.0.3405.78 or later."* The reference page lists *Introduced: WebView2 Win32 1.0.3405.78 / Prerelease 1.0.3415*. [V] Microsoft Learn release notes + `ICoreWebView2Find` page
- Semantics [V]: `Start(options, handler)` is asynchronous, *"Displays the Find bar and starts the find session, replacing any existing session. … Supports HTML and TXT document queries"*; *"To start a new session from the first match, call Stop() before Start(). Consecutive calls with the same options continue from the current position … Different search terms always start a new session from the document beginning."* `ActiveMatchIndex` is 1-based, -1 when none; `MatchCount`/`ActiveMatchIndex` events fire only after the start completes. `SuppressDefaultFindDialog` hides the browser's Find bar so the host can draw its own. Known issue: PDF documents (irrelevant here).
- Practical consequence: on runtimes **< 139** `QueryInterface(ICoreWebView2_28)` fails → the plugin needs a fallback (custom in-page search, below). Windows 11's Evergreen runtime auto-updates, so by now (Aug 2026) < 139 is rare, but the fallback must exist because the availability gate (`GetAvailableCoreWebView2BrowserVersionString`) only proves *a* runtime exists. [V] contract §3; version distribution [M]
- Limitation inherent to *any* browser find: it searches the **rendered DOM**. With full-DOM rendering (§1 bands ≤ 20 MB) that is the whole file; with virtualised rendering it is not — another reason to avoid virtualisation.

### Alternatives

| Option | Verdict |
|---|---|
| `window.find()` | **Non-standard** ("not part of any specification and is not recommended for production use"); no match count, no highlight-all control. Reject. [V] MDN |
| Custom JS search over the text buffer + **CSS Custom Highlight API** (`CSS.highlights.set(name, new Highlight(...ranges))`, `::highlight(name)`) | Marks all matches **without touching the DOM** (no `<mark>` churn, no re-tokenization), scroll with `Range.getBoundingClientRect`/`scrollIntoView`; *Baseline 2025 – newly available*, Chromium-backed. Gives regex/whole-word/case options and works on any runtime. The right **fallback** and arguably the better primary for a code viewer (host owns the UI, results identical on every runtime). [V] MDN |
| mdview's regenerate-with-`<mark>` + fragment scroll | Works with scripts disabled, but a new term **reloads the document** (scroll reset) and rewrites the whole DOM — unsuitable for MB-sized files. Do not carry over. [V] `viewer.cpp` 672–704 |

### Keyboard handling

- `AcceleratorKeyPressed` (`ICoreWebView2Controller`, header 39268) *"runs when an accelerator key or key combo is pressed or released while the WebView is focused. A key is considered an accelerator if … Ctrl or Alt is currently being held [or] the pressed key does not map to a character."* (.NET doc adds: the Escape key is always an accelerator.) *"During AcceleratorKeyPressedEvent handler invocation the WebView is blocked waiting for the decision"* → set `Handled(TRUE)` early and `PostMessage` the command (mdview pattern). [V] Learn `ICoreWebView2Controller` + search summary
- `AreBrowserAcceleratorKeysEnabled=FALSE` (`ICoreWebView2Settings3`, 1.0.864.35) disables *"Ctrl-F and F3 for Find on Page, Ctrl-P for Print, Ctrl-R and F5 for Reload, Ctrl-Plus and Ctrl-Minus for zooming, Ctrl-Shift-C and F12 for DevTools, special keys … Back, Forward, Search"* but **not** *"Home, End, Page Up, Page Down, Ctrl-X, Ctrl-C, Ctrl-V, Ctrl-A, Ctrl-Z"*; *"This setting has no effect on the AcceleratorKeyPressed event. The event will be fired for all accelerator keys."* Per-key override: `ICoreWebView2AcceleratorKeyPressedEventArgs2::put_IsBrowserAcceleratorKeyEnabled` (1.0.2210.55) — e.g. keep browser Ctrl+± zoom while blocking Ctrl+P. Processing order: host event → browser feature → web content. [V]
- **Built-in viewer key map the plugin must mirror** [V] `src/salamand.rc` 145–163 (`IDA_VIEWERACCELS`) and `src/viewer3.cpp` 3535–3585: F3/F6 find next · Shift+F3/Shift+F6 find previous · Ctrl+F3/F7/**Ctrl+F** find · **Esc** close · F2/**Ctrl+W** wrap toggle · F4/Ctrl+H hex · F5/Ctrl+T text · F8/Shift+F8 next/previous coding · F11 full screen · Ctrl+Insert/Ctrl+C copy · Ctrl+A select all · **Ctrl+G go to offset** · Ctrl+L/Ctrl+N find next · **Ctrl+P find previous** · Ctrl+O open · Ctrl+R re-read · Ctrl+S copy to file · F1 help. Note the two clashes with browser defaults: **Ctrl+P** (viewer: find previous; browser: print) and **F5** (viewer: text mode; browser: reload) — both must be intercepted in `AcceleratorKeyPressed` or the browser action fires.
- mdview additionally routes F9/Shift+F9 (scheme), Ctrl+U (source), Ctrl+0 (zoom reset); Ctrl+± and Ctrl+wheel are left to the engine (`IsZoomControlEnabled=TRUE`). [V]

### Recommended requirement(s) for the spec

- **R2.1** Ctrl+F opens the viewer's own find UI (host-drawn, dark-theme aware); F3/F6 and Shift+F3/Shift+F6 move to the next/previous match; the current match is scrolled into view and visually distinct from the other matches, all of which are marked; a match counter "n of N" is shown.
- **R2.2** Searching never reloads the document or loses the scroll position/selection; a new term on a 1 MB file returns the first match within 200 ms.
- **R2.3** Find offers case-sensitive and whole-word options (and, if the internal viewer's find dialog offers them, the same set — regular expressions, hex — or an explicit statement of what is omitted).
- **R2.4** Find works identically whether or not the installed engine offers the native Find API (runtime < 139); the plugin never shows the browser's own Find bar.
- **R2.5** Every key listed in the built-in viewer's map above either performs the equivalent viewer action or is a documented no-op; specifically Ctrl+P never prints and F5 never reloads; Esc always closes; Ctrl+A/Ctrl+C/Ctrl+Insert/Home/End/PgUp/PgDn/arrows work without a preceding mouse click.

---

## 3. Viewer features → WebView2/HTML mapping

| Feature | Mapping | Notes / evidence |
|---|---|---|
| **Line numbers** | One block element per line; number drawn by CSS (`::before { content: counter(line) }` or a fixed gutter column with `user-select: none`). | Pseudo-element content is not part of the selection, so copy yields code only [M — standard DOM behaviour]. Per-line blocks also make wrapped continuation lines number-free and give go-to-line anchors. Gutter width = digits of line count. |
| **Word-wrap toggle** (F2/Ctrl+W) | Root class switching `white-space: pre` ↔ `pre-wrap` + `overflow-wrap: anywhere`. | Pure CSS; no re-tokenization; instant except on multi-MB single lines (§1). |
| **Go to line** | Element id per line → `scrollIntoView({block:'center'})` + transient highlight of the target line. | Built-in viewer's Ctrl+G is **go to byte offset** (`CM_GOTOOFFSET`) — spec must choose: line[:column] for a code viewer, optionally also offset. Column ↔ tab-size interplay. |
| **Zoom** | Engine zoom `put_ZoomFactor` (whole page; Ctrl+wheel/Ctrl+± native with `IsZoomControlEnabled=TRUE`; `ZoomFactorChanged` sync; persisted; title shows %) — mdview's pattern. CSS font-size would scale only text. | [V] mdview `webview.cpp` 389–399, 448, 546–551; parity with mdview recommended. |
| **Selection + copy** | Engine-native; Ctrl+C/Ctrl+A/Ctrl+Insert stay enabled under the lockdown ([V] Settings3 doc). | Copied text must contain no line numbers, no zero-width/marker characters, correct line breaks (Chromium converts to CRLF for `CF_UNICODETEXT` on Windows [M] — **test**). Trailing-whitespace/tab preservation is a test case. |
| **Select all** | Native Ctrl+A selects the whole document — keep all in-page chrome (status/notices) `user-select: none`, or put the status bar in a **native Win32 child window** below the WebView (built-in viewer style) so the page is text only. | Recommend the native status bar: it also removes the need for host→page messages for encoding/EOL/zoom. |
| **Context menu** | `AreDefaultContextMenusEnabled=FALSE` (the default menu contains Back/Print/Save as/Inspect — inappropriate). Either (a) `ContextMenuRequested` (`ICoreWebView2_11`, 1.0.1185.39): filter/replace the items, `Handled(TRUE)` + `TrackPopupMenu` with `SelectedCommandId` (Learn sample does exactly this, using `ContextMenuTarget` selection kind), or (b) page `contextmenu` → web message → host menu. mdview has the dark-mode menu helper (`darkmenu.cpp`). | Whether `ContextMenuRequested` fires when default menus are disabled is **not stated** in the docs — the setting doc says only "prevent default context menus from being shown" [V]; **[T]** verify; option (b) is unaffected. |
| **Printing** | `ICoreWebView2_16` (1.0.1518.46): `ShowPrintUI(BROWSER|SYSTEM)`, `Print(settings)`, `PrintToPdfStream`; `ICoreWebView2_7::PrintToPdf` (file). Ctrl+P is a *find previous* key in the built-in viewer; the built-in viewer has no print command. | If printing is in scope: menu item → `ShowPrintUI(SYSTEM)` + a `@media print` stylesheet (light colours, line numbers, wrapped lines). Otherwise state it is out of scope. [V] |
| **Status information** | Native status bar (see Select all): file name/size, line count, **encoding** (UTF-8 / UTF-8 BOM / UTF-16 LE / UTF-16 BE / Windows-1250 … as detected or chosen), **EOL** (CRLF/LF/CR/mixed), **language** (grammar in use or "plain"), zoom %, and — if wanted — **Ln/Col of the selection anchor** (page → host message on `selectionchange`; requires `IsWebMessageEnabled=TRUE`, one-way, validated). | Everything except Ln/Col is known to the host before the page exists. mdview keeps `IsWebMessageEnabled=FALSE`; enabling it is a deliberate per-controller choice (contract §2.3) — see §6. |
| **Whitespace/tab rendering** | `tab-size: 4` (configurable; browsers default to 8). "Render whitespace" toggle: wrap whitespace runs in a span and paint dots/arrows with a CSS `background-image` sized to `1ch` — the DOM text stays the real spaces/tabs, so copy is unaffected. VS Code default `renderWhitespace: "selection"`. | [V] VS Code default; painting technique [M]. Requirement is behavioural: the toggle must not change what Copy produces. |
| **Very long lines** | `white-space: pre` + horizontal scrollbar; no tokenization above the line-length gate (§1); optional "wrap long lines" hint. | VS Code stops *rendering* a line after 10 000 chars; a viewer must still show it all — plain text is fine, highlighted spans are not. [V]/[M] |
| **Trojan-source controls** | Optionally render bidi controls (U+202A–U+202E, U+2066–U+2069) visibly, as VS Code does. | [M] |

### Recommended requirement(s) for the spec

- **R3.1** Line numbers are shown by default (toggleable) and are never part of copied text; Ctrl+C on a selection places exactly the selected source lines on the clipboard with Windows line breaks, tabs and trailing spaces preserved.
- **R3.2** Word-wrap toggles with F2 (and the menu) without reloading; line numbers stay aligned with logical lines when wrapped.
- **R3.3** Go-to-line (Ctrl+G or a menu item) accepts "line" and "line:column", scrolls the target line to the middle of the view and marks it briefly; an out-of-range line clamps to the last line.
- **R3.4** Zoom behaves as in the Markdown viewer: Ctrl+wheel, Ctrl+±, Ctrl+0, persisted across sessions, percent shown in the title.
- **R3.5** A status bar shows file name, size, line count, detected/selected encoding, EOL style, language, and zoom; it is drawn with the product's native controls and follows the light/dark theme.
- **R3.6** Right-click shows only viewer commands (Copy, Select All, Find, Go to line, Wrap, Theme, Encoding…) — never the browser's menu; the menu is dark-theme aware like mdview's.
- **R3.7** Tab width is configurable (default 4); an optional "show whitespace" mode renders spaces/tabs visibly without changing copied text.
- **R3.8** Printing: either "Print…" produces the viewed text with line numbers in a light colour scheme via the system print dialog, or printing is explicitly out of scope.

---

## 4. Themes

- **Switching without reload**: express every colour as a CSS custom property on `:root` and switch by changing one attribute (`data-theme`) — no navigation, scroll and selection preserved. Highlighters that emit **class names** (hljs, Prism) need nothing else; Shiki emits inline `style="color:…"` by default, so it must be driven with its **dual-theme / CSS-variable** output (`themes: {light, dark}`, `defaultColor: false` → per-token `--shiki-light`/`--shiki-dark` variables) or via `codeToTokens` + own class mapping [M — Shiki "Light/Dark Dual Themes" docs, not re-read]. **[T]** on a fully highlighted 1 MB file a root attribute change forces a style recalculation over ~200 k spans; measure that a switch stays under ~100 ms, otherwise switch via a stylesheet swap and accept the same cost.
- **Background before content paints**: `put_DefaultBackgroundColor` (`ICoreWebView2Controller2`, introduced 1.0.774.44) *"is the color WebView renders underneath all web content … before the initial navigation or between navigations"*; only alpha 0 or 255 allowed. The docs add a caveat: *"There is a known issue with background color where setting the color by API can still leave the app with a white flicker before the DefaultBackgroundColor takes effect. Setting the color via environment variable [`WEBVIEW2_DEFAULT_BACKGROUND_COLOR`] solves this issue"* — but that variable is **process-wide and applies once**, i.e. it would be a shared-engine-contract item, not a plugin choice. mdview's belt-and-braces (API before `put_IsVisible(TRUE)` + the window's own `WM_ERASEBKGND` brush) has been adequate; reuse it. Also set `<meta name="color-scheme">` and a `background` on `html` in the served page so the document itself never paints white. [V] Learn + `webview.cpp` 261–268, 449
- **`prefers-color-scheme` / `PreferredColorScheme`**: `ICoreWebView2Profile::put_PreferredColorScheme` (profile introduced 1.0.1210.39) *"sets the color scheme for WebView2 UI like dialogs, prompts, and context menus by setting the media feature prefers-color-scheme for websites to respond to"*; default AUTO follows the OS. **The profile is shared by every WebView2 in the same user data folder + profile name** — flipping it from the source viewer would flip mdview's `prefers-color-scheme` too. Either leave it alone (drive the theme explicitly, which the product must do anyway because it has its own scheme setting independent of the OS) or create the source viewer's controllers with a distinct `ProfileName` (`ICoreWebView2ControllerOptions`, 1.0.1210.39; ASCII, ≤ 64 chars) — still the same browser process/UDF. [V] Learn; "same process for named profiles" [M]
- **Scrollbars and form controls**: `color-scheme: dark` on `:root` (and `<meta name="color-scheme" content="dark light">`) makes the user agent restyle *"the color of the canvas surface, the default colors of scrollbars and other interaction UI, the default colors of form controls"* — this is what makes dark-theme scrollbars dark without custom scrollbar CSS, and it is independent of `prefers-color-scheme`. [V] MDN
- **Fonts on Windows 11** [V] Microsoft Typography font list: **Cascadia Mono** and **Cascadia Code** (all weights, marked "Added in Windows 11"), **Consolas**, **Courier New**, **Lucida Console** ship in-box. Cascadia Code has programming ligatures, Cascadia Mono does not; VS Code's `fontLigatures` default is **off** (`"liga" off, "calt" off`) [V]. Windows has no user-level "system monospace" setting; Chromium's generic `monospace` on Windows resolves to a fixed default (Consolas in current builds [M]). Recommend `font-family: "Cascadia Mono", Consolas, "Courier New", monospace`, ligatures off by default (`font-variant-ligatures: none`), and a family/size setting in the plugin configuration like the internal viewer's font option. Font size is CSS `px` under engine zoom; DPI changes are handled by WebView2 (`ShouldDetectMonitorScaleChanges` default) [M].
- **Which themes**: mdview ships 10 schemes + follow-system (F9/Shift+F9, persisted). A source viewer needs token colours per scheme; reusing mdview's scheme names/palettes for the page chrome and adding token palettes (e.g. GitHub Light/Dark, VS Dark+, Solarized) keeps the two WebView2 viewers consistent.

### Recommended requirement(s) for the spec

- **R4.1** The viewer offers at least one light and one dark scheme plus "follow system", switchable from the View menu and by F9/Shift+F9 like the Markdown viewer; a switch takes effect without reloading and keeps the scroll position and selection.
- **R4.2** From the moment the viewer window appears until the text is painted, no white (or wrong-scheme) area is ever visible in a dark scheme; scrollbars follow the scheme.
- **R4.3** Changing the source viewer's scheme has no effect on the Markdown viewer (or any other WebView2 consumer) in the same session.
- **R4.4** Text is monospaced using the user's configured font (default Cascadia Mono, falling back to Consolas); ligatures are off unless enabled; font family and size are configurable.

---

## 5. Text encoding of the input

### Detection — what exists in the product

- **mdview** (`render.cpp` 137–173): BOM (UTF-8/UTF-16 LE/BE) → structural UTF-8 validity → `CP_ACP`; NUL in first 4 KB = binary. Weaknesses: the validator accepts overlong forms and encoded surrogates; ANSI means whatever the process code page is (correct for CP1250 users, wrong for a CP1252 file on a Czech machine and vice versa); no UTF-16-without-BOM detection; no EOL detection. [V]
- **Built-in text viewer** (feature 015, `viewer2.cpp` 75–116, 1043–1141): on a **10 000-byte sample** (`RECOGNIZE_FILE_TYPE_BUFFER_LEN`, `viewer.h:15`) — BOM; any NUL → legacy/binary path; high bytes present and **strict** UTF-8 (`ViewerIsValidUTF8`: overlong, > U+10FFFF and surrogate ranges rejected, a truncated tail at the sample boundary accepted) → UTF-8; otherwise the legacy **code-page recogniser** `CCodeTables::RecognizeFileType` (`codetbl.cpp` 895 ff.): it runs the sample through every conversion table whose target is the Windows code page (`…→ Windows-1250` on a Czech system: ISO-8859-2, KOI8-CS, CP852 …) and scores each result with a penalty model of letter/digit/punctuation transitions (constants at 971–980); binary heuristics: > 10 consecutive NULs or > 0.5 % control characters (993–1009). The best table is auto-selected (`CodePageAutoSelect`), F8/Shift+F8 cycle codings, `CM_VIEWER_CODING_UTF8` forces UTF-8; UTF-16 is detected but left to the hex path (comment at 1059–1063). Plugins reach the same recogniser through `SalamanderGeneral->RecognizeFileType` (`zip.cpp` 3327; used by dbviewer and filecomp). [V]
- **ICU in Windows** (`icu.dll`, in-box since Windows 10 1903; header `icu.h`; C API only) exposes the charset detector `ucsdet_open/setText/detect/detectAll/getName/getConfidence` — a possible second opinion for code pages, but it targets web charsets and is not what the rest of the product uses. [V] Learn ICU page (API list includes `ucsdet_*`)

### Recommended detection order (parity with the built-in viewer, plus what it lacks)

1. BOM: UTF-8, UTF-16 LE, UTF-16 BE (UTF-32 not needed).
2. No BOM, sample contains NUL: test the **UTF-16-without-BOM** pattern (NULs concentrated at odd or even offsets, decoded text passes the text test) → UTF-16 LE/BE **[M — standard heuristic, as Notepad/VS Code do]**; else binary → decline (`CanViewFile` FALSE).
3. No NUL: strict UTF-8 validation over the sample (built-in viewer's rules) → UTF-8 (a pure-ASCII sample is "UTF-8" for display purposes; the built-in viewer keeps it "legacy" only to stay byte-identical).
4. Otherwise: the product's recogniser (`RecognizeFileType`) picks the conversion table; convert bytes with that table to the Windows code page, then `MultiByteToWideChar(CP_ACP)` — the same path the built-in viewer takes, so a Czech `.ini` in ISO-8859-2 or CP852 displays the same in both viewers. The status bar names the result; the user can override (menu, F8/Shift+F8 cycling the same coding list) and the choice is remembered per session like the built-in viewer's.
5. Invalid sequences: never abort — decode with replacement (U+FFFD), count the replacements, and show "n invalid byte sequences replaced" in the status bar; odd-length UTF-16 drops the final byte and says so. Lone surrogates in UTF-16 input become U+FFFD in the page (browser UTF-8 decoders are strict; feature 066's WTF-8 is a *file-name* contract, not a content one).
6. **EOL**: count CRLF/LF/CR over the whole file; display normalised to LF; status shows CRLF / LF / CR / "mixed (n CRLF, m LF)". Only `\n`, `\r\n`, `\r` are line terminators (VS Code behaviour [M]); U+2028/U+0085 are ordinary characters.
7. **Sample size**: detection on the first 10 000 bytes matches the built-in viewer; the *decode* is of the whole file, and a UTF-8 verdict from the sample must survive an invalid byte later in the file (rule 5), never fall back to ANSI mid-file.

### Handing the text to the page safely

Never as raw HTML: the file content must reach the DOM as **text** (`textContent`/`createTextNode`, or through the highlighter's escaper, which must itself be tested). Transport options, ranked:

| Transport | Facts | Verdict |
|---|---|---|
| **Serve the decoded UTF-8 text as its own resource** (`https://srcview.invalid/text`, `Content-Type: text/plain; charset=utf-8`) from `WebResourceRequested` with an in-memory `IStream` (mdview's `MakeAndSetResponse`, `webview.cpp` 200–212) and `fetch()` it from the page; optionally read it as a `ReadableStream` to paint the first lines early. | No escaping, no JSON, no string-size API limit (mdview already serves up to 64 MB this way); the bytes are already validated/decoded by the host. | **Recommended** |
| `PostSharedBufferToScript` (`ICoreWebView2_17`, header 19598; `CreateSharedBuffer` 46999; `ICoreWebView2SharedBuffer` 66689) | Zero-copy shared memory exposed to the page as an `ArrayBuffer`, read-only mode available; introduced ~1.0.1661.34 [M]; needs web messaging enabled [M]. | Good second option for very large text; more moving parts than fetch. |
| `PostWebMessageAsJson/AsString` | Requires `IsWebMessageEnabled=TRUE`; **no documented size limit** [V] Learn; an **unresolved** report of deadlock/crash with large messages for x64 builds on ARM64 Windows (WebView2Feedback #4589, runtime 1.0.2526-prerelease) [V]. | Use for small control messages only. |
| `AddScriptToExecuteOnDocumentCreated` / `ExecuteScript` with the text as a JS/JSON literal | Runs before any page script (persistent across navigations) / any time; *"no special limitation of the length"* per a Microsoft Q&A answer [V]; `JSON.parse` of a string literal is ~1.7× faster than an equivalent JS literal (V8) [V]; still requires JS-escaping the content and doubles memory. | Avoid for bulk text. |
| `NavigateToString` | **2 MB limit** ("Value does not fall in the expected range") [V] Rick Strahl 2024; the SDK header does not mention it. | Unsuitable. |
| Inline `<pre>` in the served HTML (mdview source mode) | Correct if escaped, but the HTML parser must chew through MBs of text before the first paint and every `&`, `<` costs bytes. | Acceptable for the plain band; fetch is better for both bands. |

### Recommended requirement(s) for the spec

- **R5.1** Files encoded as UTF-8 (with or without BOM), UTF-16 LE/BE (with BOM; without BOM when the content is recognisably UTF-16), and single-byte code pages recognised by the product's existing code-page detection (including Windows-1250, ISO-8859-2 and CP852 for Czech text) display all characters correctly; the detected encoding is named in the status bar and can be overridden from the menu and by F8/Shift+F8 with the same coding list the internal viewer offers.
- **R5.2** A UTF-8 file with a BOM never shows the BOM as a character; a file with invalid byte sequences still opens, shows a replacement character at each invalid sequence, and reports the count.
- **R5.3** The line-ending style (CRLF, LF, CR or mixed) is shown; lines are numbered identically to the internal viewer for the same file.
- **R5.4** A file is treated as binary (and handed to the internal viewer) under the same rules the internal viewer uses for its text/hex decision, except that UTF-16 text files are opened as text.
- **R5.5** File content can never be interpreted as markup or script by the viewer: a file containing HTML tags, `</script>`, event-handler attributes, entity references or `javascript:` URLs displays them literally (this is a test with a fixed corpus, see §6).

---

## 6. Security with scripts enabled

Turning `IsScriptEnabled` on changes the threat model from "no code runs" to "our bundled code runs in a locked-down origin, and a bug that lets *file content* run is a real vulnerability". The lockdowns below are all per-controller (contract §2.3 — mdview's choices constrain nobody) and all verified to exist in the vendored header.

### Lockdowns that remain possible and should be REQUIRED

| Control | Mechanism (SDK) | Note |
|---|---|---|
| Assets | Either embed the bundle (HTML/CSS/JS/grammars/themes) as **resources inside the signed `.spl`** and serve them from memory through `WebResourceRequested` with explicit `Content-Type` (mdview pattern), or `SetVirtualHostNameToFolderMapping` to a dedicated, bundle-only folder with `COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY`. Everything else: `403`. | Folder mapping has an **open** bug where `.wasm` requests intermittently resolve to a real IP (WebView2Feedback #4838, runtime 129, unassigned) [V]; its MIME inference is undocumented; and it exposes every file in the folder. In-memory serving avoids all three and keeps assets tamper-evident under code signing. |
| Network | `AddWebResourceRequestedFilter("*", ALL)` + default-deny, as mdview. Add a **CSP response header** on the document (headers are set in `CreateWebResourceResponse`; a header beats a `<meta>` tag because it is enforced before parsing and can carry every directive): `default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; worker-src 'self'; img-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'` (+ `'wasm-unsafe-eval'` in `script-src` **only** if a WASM engine is used [V] MDN). | CSP is what blocks `WebSocket`/`EventSource`/`navigator.sendBeacon` channels that a request filter may not see [M]; `connect-src 'self'` still allows the text fetch. Avoid inline styles/scripts so no `'unsafe-inline'` is needed (Shiki's default inline `style=` attributes conflict — use its CSS-variable output or classes). Consider `require-trusted-types-for 'script'` so any `innerHTML` with an untrusted string throws unless it passes a policy [M — Chromium supports Trusted Types]. |
| Host access | `AreHostObjectsAllowed=FALSE`, `AreDevToolsEnabled=FALSE`, `IsStatusBarEnabled=FALSE`, `IsBuiltInErrorPageEnabled=FALSE`, `AreDefaultScriptDialogsEnabled=FALSE` (no `alert()`), `IsReputationCheckingRequired=FALSE`, autofill/password/pinch/swipe off — all as mdview. | `IsWebMessageEnabled`: keep FALSE unless the status bar needs Ln/Col; if TRUE, the host treats every message as untrusted (fixed schema, no paths, no commands beyond "selection changed", "ready"). |
| Navigation / windows / downloads | `NavigationStarting` cancel all but the document; `NewWindowRequested` handled; `DownloadStarting` (header 6902) → `Cancel`; `LaunchingExternalUriScheme` (20646) → `Cancel` (blocks `ms-calculator:`-style launches from injected content); `SaveAsUIShowing` (28660) and `SaveFileSecurityCheckStarting` (30051) → cancel/suppress; `ScreenCaptureStarting` (31359) → cancel; `PermissionRequested` → deny everything. | All present in 1.0.4078.44 [V]; each is a `QueryInterface`, skipped silently on older runtimes. |
| Keys | `AreBrowserAcceleratorKeysEnabled=FALSE` + `AcceleratorKeyPressed` routing (§2). | Blocks F12/Ctrl+Shift+C DevTools, Ctrl+P, F5, Ctrl+S. |
| Zoom | `IsZoomControlEnabled=TRUE` is fine (engine zoom is not a security surface); mdview keeps it on. | The task list says "zoom control off" — recommend **on**, as mdview settled after feature 022 (Ctrl+wheel). |
| Extensions / profile | `AreBrowserExtensionsEnabled=FALSE` (environment option, header 48981) — default FALSE anyway. | |

### The injection risk when file content reaches the DOM

- The only place untrusted bytes meet HTML is the highlighter's output. hljs (`hljs.highlight(code)`) and Shiki (`codeToHtml`) escape text nodes; Prism likewise; but the **plugin's own glue** (line wrappers, search marks, whitespace spans, status text) is where mistakes happen. Rules: build the DOM with `textContent`/`createTextNode`; never concatenate file text into HTML strings; treat highlighter HTML as the single audited sink (or use token arrays — Shiki `codeToTokens`, hljs `_emitter` — and build nodes yourself); enable Trusted Types to make violations throw in debug.
- **Test corpus** (a fixed set of hostile files under `test/`): `<img src=x onerror="document.title='pwned'">`, `</script><script>…`, `&lt;`/`&#x3C;`, `<!-- -->`, `]]>`, `javascript:` and `data:` URLs, `<base href>`, `<meta http-equiv>`, `U+2028/2029`, lone surrogates, bidi overrides, a 100 KB line of `<<<<<`, NUL-free but binary-looking data. Pass criterion: literal display, unchanged title, no request other than the document/text/assets (observable via the `WebResourceRequested` log), no navigation.
- With scripts enabled and network denied, the residual damage of a successful injection is confined to the page (defacing its own view) unless web messaging is on — hence the schema rule above.

### WASM (Oniguruma for Shiki)

- Not needed: Shiki's **JavaScript RegExp engine** (`createJavaScriptRegexEngine()`) *"doesn't require loading a large WebAssembly file"*, offers *"smaller bundle size and faster startup time"*, and *"as of version 3.9.1, all built-in languages are supported"*; a `forgiving` option suppresses unsupported-pattern errors ("can result in highlighting mismatches, so check your results"); pre-compiled languages need ES2024 RegExp (fine on an Evergreen Chromium). [V] shiki.style
- If Oniguruma is chosen anyway: `shiki/wasm` ships as a base64-inlined string and is instantiated from bytes (`WebAssembly.instantiate(buffer)`), so no MIME question arises; CSP then needs `'wasm-unsafe-eval'` (*"If a page has a CSP header and 'wasm-unsafe-eval' isn't specified in the script-src directive, WebAssembly is blocked"*) [V]. Serving a separate `.wasm` file works with `WebResourceRequested` because the host sets `Content-Type: application/wasm` itself (required by `instantiateStreaming`) — do not rely on folder mapping (#4838).

### Recommended requirement(s) for the spec

- **R6.1** While a source file is open, the viewer makes no network request of any kind, and no request for anything but the viewer's own page, assets and the file's text is ever answered; this is verifiable from the plugin's request log in a debug build.
- **R6.2** The viewer never opens DevTools, another window, a download, a save/print/share dialog, an external application, or a browser context menu, whatever the file contains and whatever keys are pressed.
- **R6.3** The hostile-content corpus (§6) displays literally in every band (highlighted and plain) and for every supported encoding; adding a file to the corpus is a test, not a code change.
- **R6.4** The viewer's script bundle is part of the signed plugin; nothing is loaded from the user's profile, temp folder or the network.
- **R6.5** The security lockdown is applied by one shared routine before the first navigation and is asserted in a debug build (settings read back), so a regression in any single setting fails loudly.

---

## 7. Instant open (warm engine + highlighter cost)

### What determines time-to-first-paint on the warm tree

From F3 to text on screen, with the shared browser process already running (contract §1, 065 R1):

1. **Host side** (thread + window creation, file read, encoding detection): milliseconds for ≤ 1 MB **[M]**; the whole-file read is synchronous in mdview (`RenderDocument`) — fine at these sizes.
2. **`CreateCoreWebView2Environment` + `CreateCoreWebView2Controller` on the warm tree**: 065 calls this "near-instant" but **never measured it** (`baseline.md` blank). Chromium may or may not reuse a renderer process for a new controller on the same virtual-host origin; a new renderer costs tens of ms plus its memory **[T]**.
3. **Navigation + document parse**: the served HTML is small and in memory; single-digit to low tens of ms **[M]**.
4. **Bundle fetch + parse/compile**: this is the new cost the source viewer adds on **every open** (each viewer window is a fresh page). Sizes: hljs core + common languages ≈ 100–150 KB minified **[M]**; Shiki web bundle 3.8 MB minified (695 KB gzip) **[V]**, fine-grained Shiki (core + JS engine + one grammar + one theme) a few hundred KB **[M]**. V8: parsing is ~2× faster than in Chrome 60, JS processing is "10–30 % of time spent in V8 during page load", and the advice is to split bundles above ~50–100 KB and lazy-load [V] — i.e. a multi-MB bundle is a visible fraction of a 200–300 ms budget on a desktop, and language grammars must be loaded on demand, not all at once.
5. **Code cache**: Chromium caches compiled bytecode after a script has been loaded twice within **72 hours** (cold → warm → hot on the third load); the cache is attached to the **HTTP disk-cache entry for the script URL** and invalidated by a URL change (including query strings) or a fresh 200 instead of 304; scripts under 1 KiB are not cached; inline scripts are not cached; the in-process (isolate) cache is lost with the renderer [V] v8.dev. **Whether responses synthesised in `WebResourceRequested` enter the HTTP cache at all is undocumented** **[T]** — if they do not, every open re-parses the bundle; the fix would be `Cache-Control` headers that make them cacheable, or folder mapping for the static bundle only. This single measurement decides the architecture, so it belongs in the first implementation spike.
6. **Highlighter instantiation**: Shiki's instance is "expensive to create" (grammar → RegExp compilation; with the JS engine every Oniguruma pattern is transpiled at load) **[V]**/[M]; hljs has no such step. Tens of ms per grammar is the expectation **[M]**.
7. **Tokenize + DOM build + layout + paint of the first screen**: small if the first screen is prioritised (§1 progressive strategy) and the text is painted plain first.

### Keeping the page pre-loaded and "swapping" content in — honest assessment

- The 065 keeper is a **hidden controller on the main thread** that *never navigates* (contract §2.4, R4) and is `TrySuspend`ed. WebView2 objects are bound to their creating thread (contract §3; STA rule). mdview's viewer windows run **one thread per window**, so **the keeper's controller cannot be handed to a viewer window on another thread**; `put_ParentWindow` re-parents within the same thread only. Therefore "pre-load the page in the keeper and swap the file in" is impossible with the current thread model — the keeper can only keep the *browser process* warm, which it already does. [V] contract; thread-affinity consequence [M — COM STA semantics]
- What **would** work — option B "warm page on the viewer thread": give the source viewer **one dedicated viewer thread** that owns (a) a hidden, pre-navigated spare page with the bundle loaded and the highlighter instantiated, and (b) every source-viewer window; on F3, re-parent the spare controller into the new window (`put_ParentWindow`, `put_Bounds`, `put_IsVisible`), post the new text, and immediately create the next spare. Costs: all source-viewer windows share one thread (a long synchronous highlight or a modal dialog in one window stalls the others — mitigated by highlighting in a Web Worker and using only non-blocking UI); the spare page holds a loaded highlighter (~+20–60 MB renderer memory **[M]**) which contradicts 065's minimal-footprint keeper unless the contract is amended ("a plugin's keeper may hold a pre-navigated page for its own use"); `TrySuspend` on the spare is incompatible with keeping it hot (suspend pauses timers; resume is automatic on visibility) [V] Learn `TrySuspend`.
- Option A — keep the mdview model (warm process, fresh page per open) and make the per-open page **cheap**: fetch-then-`textContent` plain paint first, lazy grammar per language, small core bundle, cacheable asset responses so the code cache applies, highlighter created after first paint. This is the right starting point; B is the escalation if A misses the budget.
- Option C — pre-creating the *next* controller in advance on the viewer thread without a page: saves only step 2, which is unmeasured; not worth the complexity until step 2 is shown to dominate.

### Recommended requirement(s) for the spec

- **R7.1** After the first source-viewer use of a session, opening a ≤ 100 KB file shows its text within the "instant" budget adopted from feature 065 (on the order of 200–300 ms, verified with the same trace points on the development machine), and never slower than opening the same file in the Markdown viewer's source mode.
- **R7.2** Syntax highlighting of the first screen appears within the same budget; highlighting of the rest never delays scrolling or key handling.
- **R7.3** The first open of a session (cold engine) is no slower than the Markdown viewer's first open, and no engine work happens before the plugin's first actual use (065 FR-001 parity).
- **R7.4** Grammars and themes are loaded only for the language and scheme in use; opening a file of a new language does not re-download or re-parse the whole highlighter.
- **R7.5** Idle footprint after all source-viewer windows are closed is bounded by the same rule as 065 SC-004 (never more than one open viewer window), and keep-ready can be turned off in the plugin configuration exactly as in mdview (shared or mirrored setting — decide in planning).
- **R7.6 (measurement gate)** The plan's first spike records: warm controller-attach time, page + bundle time for each candidate highlighter, whether the code cache takes effect for served assets, and renderer memory per window; the architecture choice (option A vs B) is made on those numbers and written down.

---

## Risks & open questions

1. **Find API on old runtimes** — needs `ICoreWebView2_28` (Runtime ≥ 139, Aug 2025). Fallback (custom search + CSS Custom Highlight API) must be first-class, or the spec accepts a minimum runtime and checks it in the availability gate.
2. **Code cache for `WebResourceRequested` responses** — undocumented; decides whether per-open bundle compile is a one-time or every-time cost (§7 step 5). **Measure first.**
3. **Renderer process per viewer window** — memory and attach time unknown; may push towards a single viewer thread (§7 option B).
4. **PostWebMessage with large payloads on ARM64 hosts running the x64 build** — unresolved deadlock report (#4589); keep bulk data on `fetch`/shared buffer.
5. **Shared profile state** — `PreferredColorScheme` and `WEBVIEW2_DEFAULT_BACKGROUND_COLOR` are process/profile-wide; both must be either untouched or coordinated in the shared helper (contract §2.2 spirit).
6. **Theme switch cost on a fully highlighted 1 MB DOM** — style recalc over ~200 k spans; may need a stylesheet swap or a lower highlight gate.
7. **Highlighter choice couples to strategy**: progressive/chunked highlighting is only correct with a line-stateful tokenizer (TextMate/Shiki); hljs/Prism must highlight whole-file (worker) then insert. Shiki's JS engine `forgiving` mode trades correctness for coverage — decide and test on the product's own language list.
8. **`ContextMenuRequested` when default menus are disabled** — undocumented interaction; verify or use the page-event route.
9. **Copy fidelity** — CRLF conversion, tabs, trailing spaces, exclusion of line numbers/markers: needs a clipboard test, not an assumption.
10. **Ctrl+P / F5 clashes** with browser defaults — must be intercepted; add to the key-map test.
11. **Trusted Types availability in WebView2** — expected (Chromium), unverified.
12. **Code-page name → decode path** — the recogniser yields a conversion-table name (e.g. "KOI8-CS", which has no Windows code page); converting through the product's table to the Windows code page and then `CP_ACP` is the built-in viewer's path and should be reused rather than mapping names to `MultiByteToWideChar` code pages.
13. **UTF-16 without BOM** — the built-in viewer does not handle it; the source viewer would be the first; heuristic false positives on binary files must fall to the binary rule.
14. **GitHub's exact highlight gate** (512 KB vs 1 MB) — docs found say 1 MB; irrelevant to the decision (1 MB proposed) but noted for accuracy.
15. **Fonts on non-standard SKUs** (Windows Server, LTSC) — Cascadia Mono may be absent; the fallback chain covers it.
16. **Folder-mapped `.wasm` bug (#4838)** — only relevant if Oniguruma + folder mapping are both chosen; the recommendation avoids both.

---

## Sources

### Repository (all `E:\Projects\tandemcommander\…`)
- `src/plugins/mdview/webview.cpp` — options helper 37–44; `ReadFileBytes` cap 68; `MakeAndSetResponse` 200–212; `ServeRequest` 214–256; `ApplyBackgroundColor` 261–268; settings lockdown 282–311; navigation/new-window gates 314–348; `WebResourceRequested` 351–360; `NavigationCompleted` focus 375–385; zoom sync 389–399; `AcceleratorKeyPressed` 402–443; background-before-visible 449; keeper 671–901.
- `src/plugins/mdview/viewer.cpp` — `SIZE_GATE` 20; accelerator table 94–106; `CanViewFile` 255–275; `ViewFile` 277–331; `UpdateTitle` 514–530; `RenderDocument` 572–635; `DoFind` 672–704.
- `src/plugins/mdview/render.cpp` — `ValidUtf8` 108–126; `MdDetectDecode` 137–173.
- `src/plugins/mdview/htmlgen.cpp` — `Esc` 53–69; find marks 185–199, 687.
- `src/plugins/mdview/IMPLEMENTATION_NOTES.md` — v2/v2.1/v2.2 sections.
- `src/viewer2.cpp` — `ViewerIsValidUTF8` ~30–73, `ViewerDetectEncoding` 75–116, detection flow 1043–1141; `src/viewer.h` 15 (`RECOGNIZE_FILE_TYPE_BUFFER_LEN 10000`), 21; `src/viewer3.cpp` 3535–3585 (Ctrl keys), 50 (encoding names).
- `src/codetbl.cpp` — `RecognizeFileType` 895 ff., penalties 971–980, binary heuristics 993–1009.
- `src/salamand.rc` 145–163 — `IDA_VIEWERACCELS`.
- `src/zip.cpp` 3327 — `CSalamanderGeneral::RecognizeFileType` (plugin API).
- `architecture/11-webview2-integration.md` — contract §§1–3.
- `specs/065-mdview-instant-render/research.md` (R1–R9), `spec.md` 206–254 (SC-001…007, "200–300 ms"), `baseline.md` (blank tables).
- `src/common/dep/webview2/VERSION.txt`; `src/common/dep/webview2/include/WebView2.h` line references as given in §0.

### Microsoft Learn (fetched 2026-08-26)
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/release-notes/sdk/1-0-3405-78 — Find API promoted to stable; Runtime 139.0.3405.78 requirement
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2find — members, semantics, "Introduced 1.0.3405.78"
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2settings — settings semantics, "changes apply at next navigation"
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2settings3 — `AreBrowserAcceleratorKeysEnabled` key lists (1.0.864.35)
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2acceleratorkeypressedeventargs2 — per-key override (1.0.2210.55), processing order
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2controller — `AcceleratorKeyPressed` definition, "blocked waiting"
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2controller2 — `DefaultBackgroundColor` (1.0.774.44), white-flicker caveat, env var
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2profile — `PreferredColorScheme` (1.0.1210.39)
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2controlleroptions — `ProfileName` rules (1.0.1210.39)
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2_3 — `SetVirtualHostNameToFolderMapping`, `TrySuspend`
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2_11 — `ContextMenuRequested` (1.0.1185.39)
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2_16 — `Print`/`ShowPrintUI`/`PrintToPdfStream` (1.0.1518.46)
- https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2 — `PostWebMessageAsJson`, `AddScriptToExecuteOnDocumentCreated`, `ExecuteScript` (no size limits stated)
- https://learn.microsoft.com/en-us/windows/win32/intl/international-components-for-unicode--icu- — ICU in Windows (1703/1709/1903), `ucsdet_*`
- https://learn.microsoft.com/en-us/typography/fonts/windows_11_font_list — Cascadia Mono/Code "Added in Windows 11", Consolas, Courier New, Lucida Console
- https://learn.microsoft.com/en-us/answers/questions/714727/maximum-script-in-corewebview2-executescriptasync — "no special limitation" (via search)

### Other external
- https://github.com/MicrosoftEdge/WebView2Feedback/issues/4838 — `.wasm` under folder mapping resolves to real IP (open)
- https://github.com/MicrosoftEdge/WebView2Feedback/issues/4589 — large `PostWebMessage*` deadlock, x64 on ARM64 (open)
- https://weblog.west-wind.com/posts/2024/Jul/22/Work-around-the-WebView2-NavigateToString-2mb-Size-Limit — 2 MB `NavigateToString` limit
- https://raw.githubusercontent.com/microsoft/vscode/main/src/vs/editor/common/model/textModel.ts — 20 MB / 300 k lines / 10 000 long-line boundary / 50 MB sync / 256 M heap
- https://raw.githubusercontent.com/microsoft/vscode/main/src/vs/editor/common/config/editorConfigurationSchema.ts — `maxTokenizationLineLength` 20 000, `largeFileOptimizations`, `asyncTokenization`
- https://raw.githubusercontent.com/microsoft/vscode/main/src/vs/editor/common/config/editorOptions.ts — `stopRenderingLineAfter` 10 000, `renderWhitespace` "selection", ligatures off
- https://raw.githubusercontent.com/microsoft/vscode/main/src/vs/workbench/contrib/codeEditor/browser/largeFileOptimizations.ts — large-file notification text
- https://raw.githubusercontent.com/microsoft/vscode/main/src/vs/editor/common/core/misc/textModelDefaults.ts — `largeFileOptimizations: true`
- https://docs.github.com/en/repositories/creating-and-managing-repositories/repository-limits (and enterprise "Limits for viewing content and diffs in a repository") — 1 MB plain, 5 MB raw-only, diff limits
- https://shiki.style/guide/regex-engines · https://shiki.style/guide/bundles · https://shiki.style/guide/best-performance — engines, bundle sizes, singleton advice
- https://raw.githubusercontent.com/shikijs/shiki/main/packages/types/src/tokens.ts — `tokenizeTimeLimit` 500 ms, `tokenizeMaxLineLength` 0
- https://zenn.dev/team_zenn/articles/zenn-prism-to-shiki?locale=en — Prism/hljs/Shiki relative speed and memory (as summarised by search)
- https://raw.githubusercontent.com/highlightjs/highlight.js/main/README.md — Web Workers section
- https://gitlab.com/gitlab-org/gitlab/-/issues/366531 · https://gitlab.com/gitlab-org/gitlab/-/issues/349181 — 70-line chunking and the hljs maintainer's cross-line-grammar caveat
- https://github.com/highlightjs/highlight.js/issues/954 — freeze on huge code block
- https://prismjs.com/faq.html — why async highlighting is off by default
- https://codemirror.net/examples/million/ — viewport rendering, parse budget
- https://developer.mozilla.org/en-US/docs/Web/API/CSS_Custom_Highlight_API — Baseline 2025, DOM-free highlighting
- https://developer.mozilla.org/en-US/docs/Web/API/Window/find — non-standard
- https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Content-Security-Policy/script-src — `'wasm-unsafe-eval'`
- https://developer.mozilla.org/en-US/docs/Web/CSS/color-scheme — scrollbars/form controls, `<meta name="color-scheme">`
- https://v8.dev/blog/code-caching-for-devs — 72 h / third-load rule, URL-keyed, HTTP-cache-attached, 1 KiB minimum
- https://v8.dev/blog/cost-of-javascript-2019 — parse share, bundle-splitting advice, `JSON.parse` 1.7×

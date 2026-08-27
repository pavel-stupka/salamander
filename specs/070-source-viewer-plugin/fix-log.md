# Fix Log — Feature 070 (Code Viewer), first-run defects

**Session 2026-08-27.** The plugin ran for the first time (quickstart scenario 1)
and three defects were reported. This file is the running record of the
diagnosis and the fixes; it is updated as the work progresses.

Reported (in the user's words, translated):

1. Syntax highlighting does not work at all (e.g. `.cpp` files render plain).
2. After switching the colour scheme, syntax colouring is no longer applied.
3. When the viewer window opens, the window must already have the colour the
   WebView page will have (per the active scheme), so nothing flashes —
   the same problem mdview already solved.

Previously fixed in commit `7c47cfe`: every page asset answered 403 because
rc.exe treats `IDR_WEB_FIRST+0` in the resource-id position as a resource
*name*, not arithmetic — ids are now emitted as numeric literals.

---

## Defect 1 — `.cpp` highlighting never works

**Root cause (verified in a headless harness): the `glsl` licence stub does
not register a language, and shiki refuses the whole grammar over it.**
The stub `web/shiki/langs/glsl.mjs` exported an *empty* list on the theory
that "an empty grammar list keeps the importing grammar working". It does
not: `cpp` (and `cpp-macro`, `elm`, `nim`) declare `glsl` in their
`embeddedLangs`, and shiki validates embedded languages at load —
`createHighlighterCore` throws

    Missing languages `glsl`, required by `cpp-macro`, `cpp`

so the worker posts `failed`, the page aborts highlighting, and every
C/C++-family file renders plain, deterministically. Languages that do not
pull in the stub (JavaScript, Python, …) were unaffected — which is exactly
the reported shape ("e.g. .cpp does not work at all").

**Fix:** the stub now registers a minimal no-op grammar
(`{name:'glsl', scopeName:'source.glsl', patterns:[]}`). Embedded GLSL
blocks get no tokens of their own; C++, Elm and Nim highlight again.

**Hardening (same area, not the root cause):** the shared host subscribed to
resource requests with the deprecated `AddWebResourceRequestedFilter`,
documented to cover *document-sourced* requests (dedicated workers are
attributed to their document; shared/service workers are not covered). The
host now uses `ICoreWebView2_22::AddWebResourceRequestedFilterWithRequestSourceKinds(
L"*", CONTEXT_ALL, SOURCE_KINDS_ALL)` with the old call as fallback on
runtimes older than 111, so any future worker kind stays inside the
interceptor's default-deny.

## Defect 2 — scheme switch stops highlighting

Multiple cooperating JS defects in `web/viewer.js` / `web/worker.js`:

- **`done[]` poisoning (the main one).** The worker's control handlers
  (`load`/`retheme`/`relang`) `await ensureHighlighter(...)` — a dynamic
  import that yields to the event loop. A `viewport` message posted by the
  page meanwhile (any scroll/resize/render — `setTheme` itself calls
  `render(true)` right after posting `retheme`) is processed in that gap,
  passes the generation check (the counter is bumped synchronously at the
  start of the control handler), calls `tokenizeRange` with the highlighter
  missing or the new theme not yet loaded, gets `null` — **and still marks
  the chunk `done[c] = true`**. Those chunks are never tokenized again: the
  visible window stays plain forever.
  **Fix:** `onViewport`/`sweepStep` now bail out before touching `done[]`
  unless the highlighter exists *and* the active lang and theme are actually
  loaded (`cvReady()`); the pending control handler re-runs the viewport
  itself once loading finishes, and the sequential sweep covers the rest.
- **Two independent generation counters.** The page sends its `gen` in every
  message, but the worker kept its own `generation++`. They stay in lockstep
  only while every page-side bump pairs with a worker-side one — not true for
  a plain-band init (worker terminated), `setLanguage(null)`, or a worker
  created later (fresh counter vs. an arbitrary page counter): every emitted
  token batch is then dropped by the page's `m.gen !== gen` check.
  **Fix:** the worker now *adopts* the page's generation
  (`generation = m.gen`) in `load`/`retheme`/`relang`.
- **Plain files never re-themed.** `setTheme` with `highlighting == false`
  applied `m.themeInfo` — which the host never sent — so switching schemes on
  a plain-band file changed nothing inside the page.
  **Fix:** the host now sends the scheme's colours (`themeInfo: {type, bg,
  fg}`, from `CvScheme`) in **both** `init` and `setTheme`; the page applies
  them immediately, and the worker's full theme palette (selection, find,
  gutter colours) refines them when tokenization is ready.
- **Lost theme on language re-selection.** `setLanguage` on a window whose
  worker had been terminated called `startWorker({theme: null})`, so
  tokenization ran with no theme. The page now remembers the active scheme id
  (`themeId`) from `init`/`setTheme` and always passes it.

## Defect 3 — colour flash when the window opens

Two layers, matching what mdview solved (`src/plugins/mdview/viewer.cpp`
WM_CREATE + `webview.cpp` ApplyBackgroundColor):

- **Host window / WebView surface.** codeview called
  `Web->SetBackgroundColor()` only from `OnReady` — which the shared host
  invokes *after* `put_IsVisible(TRUE)`, so the controller flashed the
  default white surface. mdview's pattern is to set the colour immediately
  after `Web->Create(...)`: the impl caches it (`bgColorSet`) and
  `ApplyControllerReady` applies it **before** the controller becomes
  visible. codeview now does the same (`viewer.cpp` WM_CREATE); the
  `WM_ERASEBKGND`/`BgBrush` half was already right.
- **Inside the page.** `viewer.css` defaults are the dark palette and
  `viewer.html` is stamped `data-polarity="dark"`, so with a light scheme the
  page painted dark until the worker's theme arrived (hundreds of ms).
  The host now passes the scheme colours in the navigation URL fragment
  (`#bg=...&fg=...&polarity=...`); `viewer.js` applies them synchronously at
  module start (before first paint), and the `themeInfo` now carried by
  `init`/`setTheme` keeps them right thereafter. Every hop of the chain —
  native window → WebView surface → page first paint → tokenized colours —
  is now the same colour.

## Status

- [x] Diagnosis (this file)
- [x] Fix: glsl licence stub registers a real (empty) grammar (web/shiki/langs/glsl.mjs)
- [x] Hardening: request-source-kinds resource filter (webhost.cpp)
- [x] Fix: codeview WM_CREATE background before controller visibility (viewer.cpp)
- [x] Fix: scheme colours in init/setTheme (`themeInfo`) + first-paint URL fragment (webglue.cpp/.h, viewer.cpp)
- [x] Fix: worker generation adoption + ready-guard against the viewport race (worker.js)
- [x] Fix: page remembers the scheme id, applies themeInfo immediately (viewer.js)
- [x] Headless harness verification of the JS pipeline (see Verification)
- [x] Build green: incremental Debug x64 (0 errors), `check_data.py` 26/26,
      `build.cmd full` 189 language modules OK
- [x] Translations for the codeview module (T055) — DeepL only, no Anthropic

## Verification

**Headless worker harness** — new, committed as
`test/harness/test_worker.mjs` (`node src/plugins/codeview/test/harness/test_worker.mjs`;
runs the real `web/worker.js` + shiki bundle in Node with postMessage shims,
fixture = the plugin's own `viewer.cpp`, 1092 lines). All 12 checks pass:

- `load` (lang `cpp`, theme `dark-plus`): `ready` with dark themeInfo, tokens
  for the viewport, full sweep covers 1092/1092 lines, packed triples carry
  real colour keys, no failure. Before the glsl-stub fix this failed
  immediately with ``Missing languages `glsl`, required by `cpp-macro`, `cpp```.
- `retheme` to `github-light` with a deliberately interleaved `viewport`
  message (the race): visible window re-tokenized, full re-sweep, colours
  actually change (`#6A9955` → `#6A737D`).
- Generation adoption: batches carry the page's gen (3/4/9), including a
  worker created when the page counter is already high. Against the
  pre-fix worker.js the same test fails 3 checks (all batches would be
  dropped by the page's gen filter) — the test is not vacuous.

**Still needs a GUI session** (quickstart scenarios 1, 5, 7): F3 on `.cpp`,
F9 scheme cycling on both plain and highlighted files, window open with a
light scheme and a dark scheme (no flash at any hop: native window → WebView
surface → page first paint → tokens).

---

# Second round — defects 4–8 (session 2026-08-27, after the first fixes)

Reported (in the user's words, translated):

4. Not all lines are shown — scrolling a `.cpp` file to the end leaves the
   last 2–3 lines invisible.
5. After F3 the view cannot be scrolled with the keyboard (arrows, PgDn) —
   the WebView appears not to have focus.
6. The bottom bar is empty — "what is it even for?" (screenshot
   `temp/bottom.png`).
7. The default rendering is not as sharp as mdview's — the text looks
   "frayed"/washed out. (Follow-up: the font must stay monospace — it is the
   sharpness, not the font choice.)
8. View ▸ Language is incomprehensible — only letters, most repeated. If it
   is a syntax switcher it is not needed; showing the detected type (e.g.
   C++) in the window title is enough.

## Defect 4 — last lines unreachable at the bottom

Two independent causes, both fixed:

- **Native (the main one):** `CTcWebHost::Resize` was a no-op while the
  controller did not exist yet, and controller-ready sized the surface with
  `GetClientRect(parent)` — mdview's geometry, where the WebView IS the whole
  client area. codeview always calls `Resize(client minus status bar)` from
  `WM_CREATE`, i.e. *before* the async controller exists, so the request was
  dropped and the surface ended up `CV_STATUS_HEIGHT` taller than intended
  until the first manual window resize — the page's bottom (its last lines
  and its horizontal scrollbar) sat in/under the status-bar strip.
  **Fix (shared host):** `Resize` records the size (`pendingCx/Cy`);
  controller-ready applies the recorded size and falls back to
  `GetClientRect` only if no `Resize` ever ran (mdview's case — behaviour
  unchanged there).
- **Page:** the virtual list measured a fractional row height
  (13 px × 1.45 = 18.85 px) and used it in all offset math while the sizer
  height, `scrollHeight` and `scrollTop` round through integers — sub-pixel
  drift between the computed and the real geometry. **Fix:** `measure()` now
  pins the row height to a whole pixel (`--line-height: <N>px` after
  measuring the natural height), so row offsets, the sizer height and the
  scroll range are exact integers by construction. (Re-measuring resets the
  pin first, so font/zoom changes keep working.)

## Defect 5 — keyboard cannot scroll

The host side was fine (controller `MoveFocus` on ready and after every
navigation; `WM_SETFOCUS` forwards). The gap was **inside the page**: the
scrolling element is a `div#scroller` (the document itself is
`overflow:hidden`), a div is not focusable and no element ever took focus, so
arrow/PgDn keydowns hit `<body>` and scrolled the (unscrollable) document.
**Fix (`viewer.html/.css/.js`):** `#scroller` is focusable (`tabindex="-1"`,
no focus ring), is focused on init and whenever the page window regains
focus, and a document-level keydown handler implements the viewer keys
explicitly — Up/Down one line, PgUp/PgDn one page, Space/Shift+Space page,
Left/Right horizontal, Home/End top/bottom (Shift+Home/End left alone) —
with `preventDefault`, so behaviour no longer depends on where focus sits.

## Defect 6 — empty status bar (and a stuck window title)

**Root cause: `LoadStr` returns ANSI, and the status/title text was pushed
through the *strict* UTF-8 decoder `SplU8ToWAlloc`.** In the English UI every
string is ASCII and it happens to work; in the Czech UI "%d řádků" is CP1250,
the strict decode fails, the function returns NULL — and `SetWindowTextW`
was simply never called: the bar stayed forever empty and the window caption
stayed at its creation default. (What the bar shows: lines, Ln/Col, encoding,
EOL, language, zoom — spec FR-022.)

**Fixes (`viewer.cpp`, `webglue.cpp`):**

- `UpdateStatus`/`UpdateTitle` rebuilt wide end-to-end:
  `SalamanderGeneral->LoadStrW` for every localized string, `SplU8ToWAlloc`
  only for the UTF-8 file name, plain widening for the ASCII language display
  names. Same class of fix in `CvMsgInit`: the plain-band notice
  (`plainReason`) now uses `LoadStrW` — with ANSI `LoadStr` it reached the
  page as mojibake in non-English UI.
- While in there, the bar was made presentable: real status font
  (`NONCLIENTMETRICS.lfStatusFont`, owned `HFONT`), height derived from the
  font instead of a fixed 20 px (was clipped on high-DPI),
  `SS_CENTERIMAGE`, and `WM_CTLCOLORSTATIC` routed through
  `ThemeHandleCtlColor` (feature 049's two-touchpoint pattern) so it follows
  the dark theme instead of flashing a light-grey strip.
- `CvLanguageDisplay` (which mixed ANSI `LoadStr` into the same sink) became
  dead code and was removed.

**Rule reaffirmed:** `LoadStr` output must never enter a `Spl*U8*` converter
or any wide/UTF-8 sink — use `LoadStrW` there (feature 069's caption rule).

## Defect 7 — text less sharp than mdview

Two Chromium compositing effects, not the font:

- `#lines` was positioned with `transform: translateY(...)` +
  `will-change: transform` — that promotes the text onto its own composited
  layer, where Chromium abandons subpixel (ClearType) antialiasing.
  **Fix:** position via `top:` (layout), no `will-change`.
- The scrolled contents had no known-opaque backdrop (`#scroller` was
  transparent; only `body` carried the colour), which also forces grayscale
  antialiasing during composited scrolling. **Fix:** `#scroller` gets an
  explicit opaque `background: var(--bg)`.

Together with the integer row offsets from defect 4, glyphs now render in the
main layer, on whole-pixel baselines, over an opaque background — the same
conditions mdview's text renders under. The font stack stays monospace
(`Cascadia Mono`, Consolas fallback) per the user's follow-up.

## Defect 8 — View ▸ Language menu removed (FR-007 amendment)

The picker grouped ~200 grammar-backed languages into submenus keyed by first
letter, re-starting a bucket every 30 items — so the top level really was
"only letters, most of them repeated". Per the user's decision the override
is gone entirely: **the identified language is displayed instead** — in the
window title (`name [C++] - Code Viewer`) and in the status bar. Removed:
menu construction, `CM_LANG_*` commands, `SelectLanguage`, `ForcedLanguage`,
`CvMsgSetLanguage` (the page keeps its `setLanguage` handler per the
contract; the host just never sends it), `IDS_MENU_VIEW_LANGUAGE` +
`IDS_MENU_LANG_AUTO`. Spec FR-007 and US4 scenario 5 are amended in place;
quickstart scenario 4 updated.

Because two strings left `lang.rc2`, the two-stage translation refresh was
run (`build_langs.cmd --export-templates` → `translate.merge --module
codeview` → `build.cmd full`): 8 languages × 97 entries (was 99), 0 gaps,
0 validation failures, **no DeepL characters sent** (removals only).

## Defect 9 — Ctrl+PgUp navigates forward (reported after the second round)

Reported: Ctrl+PgUp followed by Ctrl+PgDn should return to the same file, but
navigation "runs to the end and then stops working".

**Root cause:** `NextFile` used `GetNextFileNameForViewer` for BOTH
directions, passing `dir > 0 ? FALSE : TRUE` as the 4th argument — which is
**`preferSelected`, not a direction**. The API has a separate
`GetPreviousFileNameForViewer` for stepping back (spl_gen.h:2711/2735; the
built-in viewer in `src/viewer3.cpp` CM_PREVFILE/CM_NEXTFILE calls the pair).
So Ctrl+PgUp also stepped *forward* (additionally restricted to selected
files when any were selected), both keys marched to the last file, and from
there every call returned FALSE with `noMoreFiles` — exactly "dojede
nakonec a pak už to nejde".

**Fix (`viewer.cpp NextFile`):** direction now selects the API call —
`GetPreviousFileNameForViewer` for Ctrl+PgUp, `GetNextFileNameForViewer` for
Ctrl+PgDn — with `preferSelected=FALSE` and `onlyAssociatedExtensions=TRUE`,
matching the built-in viewer's plain prev/next commands. Build green
(incremental Debug x64, 0 errors). GUI check: open a file mid-panel,
Ctrl+PgDn then Ctrl+PgUp must return to the original file; at either end the
window stays on the boundary file and later steps in the opposite direction
still work.

## Defect 7, second attempt — the font, not the compositing

Reported again after the third round: the text still reads soft next to mdview.

The compositing fixes from the first attempt (no `translateY`/`will-change`,
opaque scroller background, whole-pixel row heights) were right and stay — but
they were not the whole cause. **The cause is which font wins.** On a machine
with both installed:

| | leading family | size |
|---|---|---|
| mdview code blocks | **Consolas**, then Cascadia Mono | ≈14.4 px |
| codeview | **Cascadia Mono**, then Consolas | 13 px |

Consolas is hand-hinted for ClearType at exactly these sizes; Cascadia Mono is
a modern, lightly hinted face that renders visibly softer at 13–14 px. So the
two viewers were being compared across two different typefaces, and the one
that looked worse was the one that had picked the unhinted face first.

**Fix:** Consolas leads in all three places the decision is stated — the
stylesheet default (`web/viewer.css`), the stack the page appends to a
configured family (`applyView`), and the host's own default
(`config.cpp g_fontFamily`). A guard in `test_page.mjs` fails if the three ever
drift apart, because each one alone still looks right in review.

**Migration:** the default was already persisted for anyone who had run the
plugin, so the change would have done nothing on their machine. The config
version goes 1 → 2 and a stored family that is *still exactly the old default*
("Cascadia Mono") is moved to the new one; a family the user typed themselves
is left untouched.

The size stays 13 px (Visual Studio's own default is ~13.3 px). If it still
reads small, Configuration ▸ Size sets it per user without a rebuild.

---

# Third round — the systematic sweep (session 2026-08-27)

Defect 9 was the turning point: it was not a subtle race but a **misread API
parameter** that anyone could have caught by reading the header next to the
call. Rather than wait for the next one to be reported, the whole plugin was
reviewed by twelve independent agents and then the fixes were reviewed by five
more.

**37 unique defects confirmed, 35 fixed** — including three shipped features
that did not work at all (Copy/Select All, Word Wrap, Show Whitespace), a
use-after-free on closing the window during a cold engine start, and the
language name shown in the title being the name of a *different* language for
most common file types (`.py` said "Easybuild").

The full record — method, every finding with its verdict, what was fixed, the
two findings fixed differently from their own suggestion, the new tests and
the proof that they are not vacuous, what is deferred and why — is in
**`stabilization-review.md`**. It is the document to read; this file stays the
chronological log.

Two process notes worth keeping:

- **The verifiers refuted nothing** (38 of 38 findings CONFIRMED). A 0 %
  refutation rate means they were not an independent gate, so every finding
  was re-checked against the code before it was fixed. Two were then handled
  differently from their own suggestion (`stabilization-review.md` §4).
- **A vacuous test is worse than no test.** The data harness's SC-001 coverage
  rule had been passing on a generator counter that was 63 languages too high;
  it could not have failed on the regression it exists to catch. The counter is
  now derived from the shipped data, and the gap it revealed was closed rather
  than legislated away.

## Status (second round)

- [x] Shared host: pending-bounds fix (src/common/webhost/webhost.cpp) — mdview path unchanged
- [x] Page: integer row height + `top:` positioning + opaque scroller background (viewer.css/.js)
- [x] Page: focusable scroller + explicit viewer keys (viewer.html/.css/.js)
- [x] Host: wide status/title/plain-reason via LoadStrW; themed, DPI-correct status bar (viewer.cpp, webglue.cpp)
- [x] Language menu removed; language shown in title + status bar (viewer.cpp, codeview.h/.rh2, lang.rc2, webglue.*)
- [x] Translations refreshed after the string removal: 8 × 97 entries, 0 gaps, 0 failures
- [x] Build green: incremental Debug x64 (0 errors) and `build.cmd full` (189 language modules OK)
- [x] Headless worker harness still ALL PASS (`test/harness/test_worker.mjs`)
- [ ] GUI re-check of quickstart scenarios 1, 2, 4, 6 (bottom line reachable,
      PgDn/arrows scroll immediately after F3, status bar populated + dark,
      Czech UI title/status text, sharpness vs mdview side by side)

## Translations (T055) — done 2026-08-27

- `_DOMAINS["codeview"]` added to `tools/translate/uicontext.py`.
- Plugin name (`IDS_PLUGINNAME` = "Code Viewer") and the 12 scheme names
  pinned in `translations/ui-overrides.json` for all 8 enabled languages
  (scheme names are identifiers, not prose — feature 055's lesson).
- Two-stage refresh: `build_langs.cmd --export-templates` (21/21 modules) →
  `python -m translate.merge --module codeview` → `build.cmd full`.
- Result: 8 languages × 99 entries (13 human, 76 machine, 10 skip),
  0 validation failures, 0 duplicate accelerators, 93 controls widened.
  DeepL characters sent: 17,872 (quota remaining ≈ 482k).
- **No Anthropic key involved** — `tools/translate/` calls only DeepL
  (`temp/deepl_key.txt`, gitignored); REMAINING-WORK.md §4 overstated the
  requirement.
- Spot check `translations/czech/codeview.slt`: menus/dialogs translated,
  scheme names and plugin name stayed English per the pins.

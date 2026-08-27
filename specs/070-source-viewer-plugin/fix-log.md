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

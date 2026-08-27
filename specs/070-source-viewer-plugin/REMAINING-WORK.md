# Remaining Work — Feature 070 (Code Viewer)

**As of 2026-08-26.** `build.cmd` is green (`codeview.spl` 12.9 MB, mdview
unchanged and still building); `python src/plugins/codeview/test/check_data.py`
passes all 21 data checks. 42 of 59 tasks are code-complete.

> **Update 2026-08-27:** the plugin has now been run. Three defects found on
> first use (no `.cpp` highlighting — the glsl licence stub; highlighting
> lost on scheme switch — worker generation/race; colour flash on window
> open) are diagnosed, fixed and harness-verified in **`fix-log.md`**, which
> also records that **§4 below (translations) is done** — DeepL only, the
> Anthropic key was never needed. A headless worker regression test now
> lives at `test/harness/test_worker.mjs`.
>
> **Update 2026-08-27 (second round):** five more defects from real use are
> fixed and documented in **`fix-log.md` (defects 4–8)**: last lines cut off
> (shared-host pending-bounds + fractional row height), keyboard scrolling
> dead after F3 (unfocusable page scroller), empty status bar / stuck title
> in non-English UI (ANSI `LoadStr` through the strict UTF-8 decoder —
> `LoadStrW` now, plus theme + DPI treatment of the bar), blurry text
> (composited transform + no opaque backdrop), and the unusable
> View ▸ Language menu — **removed by an FR-007 amendment** (spec updated in
> place; the detected language now shows in the title and status bar; the
> two dropped strings required the two-stage translation refresh, 0 DeepL
> chars). The §1 risk-table rows on line-height measurement and the STATIC
> status bar are thereby resolved.

> **Update 2026-08-27 (third round — systematic review):** the plugin was
> reviewed end to end by twelve independent agents and the fixes accepted by
> five more. **37 unique defects confirmed, 35 fixed**, including three
> advertised features that did not work at all (Copy/Select All, Word Wrap,
> Show Whitespace), a use-after-free when the window is closed during a cold
> engine start, and the language label being a *different* language's name for
> most common file types. Two new headless harnesses run everything without a
> GUI: `src\plugins\codeview\test\run_tests.cmd`. Full record:
> **`stabilization-review.md`**. §1 below is therefore obsolete — the plugin
> has been run and reviewed — but its risk table is worth keeping as the record
> of what was predicted: the line-height and STATIC-status-bar rows both turned
> out to be real defects. **Two things it did NOT cover remain open: §2 (mdview
> on the shared host) and the code-table gap (FR-024), now recorded as F16 in
> the review.**

This file is the honest handoff: what is **not** done, and why. Nothing here is
a discovered blocker — every item is either manual verification that needs a
GUI session, a step that needs credentials, or a deliberate deferral.

---

## 1. The plugin has never been run

Everything below the C++/JS boundary compiles and the data is verified, but **no
scenario in `quickstart.md` has been executed**. The first session with the
application open should run them in order; scenario 1 (F3 on ten languages)
either works or reveals something structural, and everything else depends on it.

Specific things most likely to need a fix on first run, with where to look:

| Risk | Why it is a risk | Where |
|---|---|---|
| ESM import of the language modules through the interceptor | the modules use relative `import './x.mjs'`; the browser resolves them against `shiki/langs/`, and each resolution must hit the allow-list | `webglue.cpp` `CvConfigureHost`, `web/worker.js` |
| Worker module loading under the CSP | `new Worker('worker.js', {type:'module'})` needs `worker-src 'self'` **and** the worker's own imports to be served | `webhost.cpp` `kCspScripted` |
| The `text` resource fetch | relative `fetch('text')` must resolve to `https://codeview.invalid/text` | `web/viewer.js` `init()` |
| Line-height measurement before the font loads | the virtual list geometry is computed from one probe row | `web/viewer.js` `measure()` |
| Status bar as a plain `STATIC` | may need owner-draw to follow the dark theme properly | `viewer.cpp` `WM_CREATE` |

## 2. mdview is not yet on the shared host (T006/T007)

`src/common/webhost/` exists, is complete, and codeview uses it. **mdview still
carries its own copy** in `src/plugins/mdview/webview.cpp`.

Deliberately deferred, not overlooked: converting it is a change to a shipping
feature whose acceptance (`contracts/webview-host-sharing.md` §4 — the 021
lockdown re-verification and the 065 keeper scenarios) is a manual GUI pass. It
belongs in its own reviewable, revertible commit (constitution III), after
codeview itself is known to work.

Until then the product carries two copies of the WebView2 host — the exact
duplication the contract exists to prevent — so this is the **first** thing to
finish, and `architecture/11-webview2-integration.md` says so.

## 3. Verification tasks that need the GUI

T019, T024, T027, T028, T029, T032, T034, T040, T051, T053, T054, T057.
These are the quickstart scenarios plus the runtime halves of the corpus checks:
hostile content displayed literally, the request log showing only allow-listed
URLs, the key sweep, copy fidelity, the encoding matrix, decline behaviour, and
the performance budgets. The corpora are already written
(`src/plugins/codeview/test/corpus/hostile/`, `.../encodings/`) — 14 hostile
files and 9 encoding fixtures, including a UTF-16 `.reg` without a BOM, an
MPEG-TS `.ts`, an 8 000-token single line, and bidi overrides.

## 4. Translations (T055) — needs credentials

The new module needs the two-stage refresh, whose second stage calls DeepL and
Anthropic:

```
src\vcxproj\build_langs.cmd --export-templates
python -m translate.merge --module codeview      # network: DeepL + ANTHROPIC_API_KEY
build.cmd full
```

Also still to do: `_DOMAINS["codeview"]` in `tools/translate/uicontext.py`, and
pinning the plugin name plus the 12 scheme names in
`translations/ui-overrides.json` (feature 055's lesson: scheme names are
identifiers, not prose). **`build.cmd full` will fail for every language until
this runs** — that is by design (feature 038), not a defect.

## 5. Ship gate (T059)

`CHANGELOG.md` entry plus the version/build bump in the same change. Not done
because the feature is not shippable yet.

---

## Defects found during implementation, already fixed

- **`*.txt`/`*.log` were claimed by two families at once** — caught by the data
  harness. Deleting the plain-text family would not have stopped the plugin
  opening those files, contradicting FR-009. Fixed in `gen_langmap.py`.
- **`*.pat` collided with the picture viewer** — caught by the intersection
  check; added to `NEVER_CLAIM`.
- **The T013 spike overturned three design decisions** before any of them was
  built on: the JavaScript regex engine (2.2× too slow), whole-file
  tokenisation (0.7 s for 100 KB), and the dual-theme instant flip
  (incompatible with incremental tokenisation). See `spike-results.md`;
  `research.md` D2/D6/D16 are marked REVISED.

## A defect in the plugin API that outlives this feature

`CSalamanderGeneralAbstract::GetNextFileNameForViewer` documents its `fileName`
buffer as *"at least MAX_PATH"* (`src/plugins/shared/spl_gen.h:2703`). The core
actually fills it with `lstrcpyn(fileName, ..., SAL_MAX_PATH_UTF8)`
(`src/salamdr6.cpp:205,223`) and its own callers declare
`char[SAL_MAX_PATH_UTF8]` (`src/viewer3.cpp:967`). **A plugin that believes the
header takes a buffer overflow on a long path**, and `SAL_MAX_PATH_UTF8` lives
in a core-only header (`src/common/salpath.h`), so a plugin cannot even name the
right size.

codeview works around it (`viewer.cpp NextFile`, heap buffer, value restated
with a comment). The real fixes — correcting the header comment, and exporting
the constant to plugins — are a small separate change that every plugin
implementing panel navigation needs. **Worth doing before another plugin copies
the documented, wrong size.**

## Known limitations to state in the changelog when this ships

- 18 grammars and 1 theme are excluded for licence reasons, so `.glsl`,
  `.matlab`, `.tcl`, nginx configs, Ada, gnuplot, Racket and org files open as
  plain text. `glsl` additionally ships as a minimal no-op grammar stub
  because C++, Elm and Nim declare it in `embeddedLangs` — shiki refuses to
  load them if `glsl` is not registered (fix-log.md defect 1; an *empty*
  stub is not enough).
- The plain band (over 1 MB, or lines over 20 000 characters) has no
  highlighting by design.
- "Restore default file types" takes effect at the next application start —
  the viewer list can only be written from `Connect()`.

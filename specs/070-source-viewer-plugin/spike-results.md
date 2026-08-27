# Spike Results — Highlighting Engine & Rendering Strategy (T013)

**Date**: 2026-08-26 · **Gate for**: research decisions D2 (engine), D6 (rendering
strategy & gates), D16 (theme mechanics), and the CSP in
`contracts/rendering-lockdown.md`.

Measured on the development machine (Windows 11, Node v24.19.0 / V8 — the same
engine family as the WebView2 Evergreen runtime; absolute numbers in the browser
will differ somewhat, the ratios are what the decisions rest on).
Corpus: `src/plugins/mdview/viewer.cpp` repeated to the target size (real C++,
~34 chars/line average).

## 1. Whole-file tokenization is NOT viable — the finding that reshaped the design

| Size | Shiki JS RegExp engine | Shiki Oniguruma (WASM) |
|---|---|---|
| 10 KB | 1 269 ms* | 463 ms* |
| 50 KB | 763 ms | 407 ms |
| 100 KB | 1 477 ms | 695 ms |
| 200 KB | 2 902 ms | 1 393 ms |
| 400 KB | 6 319 ms | 2 853 ms |
| 1 MB | (extrapolated ~16 s) | 7 476 ms |

\* first call includes one-time grammar/theme compilation.

**≈15 ms/KB (JS engine) and ≈7 ms/KB (Oniguruma).** A 100 KB file — the size
FR-036/SC-003 requires to be on screen in ≤ 0.3 s — needs 0.7–1.5 s of pure
tokenization. **Highlighting the whole file up front cannot meet the budget at
any of the planned gate values.**

## 2. Oniguruma (WASM) replaces the JavaScript engine — D2 revised

The JS engine was chosen in D2 to avoid WASM (CSP `'wasm-unsafe-eval'`, the
folder-mapping `.wasm` bug, JIT policy). Measured, it is **2.2× slower** across
every size. That trade is not worth it for a viewer whose headline requirement is
instant display.

- `shiki/wasm` ships the binary **inlined as base64** and instantiates from
  bytes — there is no separate `.wasm` request, so the MIME/virtual-host
  concerns (WebView2Feedback #4838) never arise.
- Engine bundle sizes (esbuild, minified, ESM): **JS engine 160 KB**,
  **Oniguruma 718 KB** (incl. inlined WASM). Both are per-open parse cost;
  718 KB is acceptable and far below the 3.8 MB full-Shiki web bundle.
- Engine creation (engine + WASM instantiate + 1 language + 1 theme): **55 ms**.
- **Consequence**: `contracts/rendering-lockdown.md` § 2 must add
  `'wasm-unsafe-eval'` to `script-src`. Everything else in the lockdown is
  unchanged; no network, no separate asset fetch.

## 3. Incremental tokenization works — and is the whole design

`getLastGrammarState()` + `codeToTokens({ grammarState })` resume correctly
across chunk boundaries, so the file can be tokenized in pieces without
re-reading from the top.

| Measurement (Oniguruma, warm) | Result |
|---|---|
| Highlighter create (engine + WASM + 1 lang + 1 theme) | **55 ms** |
| Cold first tokenization (60 lines, incl. grammar+theme compile) | **263 ms** |
| Warm 60 lines (≈ one screen) | **27 ms** |
| Warm 200 lines | **137 ms** |
| Chunked sweep of 1 MB, 500-line chunks, state resumed | **7 071 ms** total, **171 ms** worst chunk, 114 ms average |

**Adopted strategy (D6 revised)**:

1. Render decoded text as plain lines immediately — no tokenizer involved; this
   is what satisfies "text visible within the budget".
2. Highlight the **visible window first** (≈ one screen ≈ 27 ms warm), then
   sweep the rest in chunks, resuming grammar state, always re-prioritising the
   chunk the viewport is on.
3. Chunk size **100–200 lines** (≈ 30–60 ms each), not 500 — the worst-case
   171 ms chunk is a visible stutter if it runs on the UI thread.
4. Run the tokenizer in a **Web Worker** (`worker-src 'self'` is already in the
   CSP) so no chunk size can block scrolling, input or find.

**Honest budget statement** (replaces the optimistic reading of FR-036/R7.2):
*text* appears within the instant budget; the *first screen's colours* land
roughly 300 ms after that on a cold page (55 ms create + 263 ms first
tokenization), and ~30 ms on any later file in the same window. This is what the
feature can deliver and what the quickstart measures — it must not be reported as
"instantly highlighted".

## 4. Dual-theme mode and incremental tokenization are mutually exclusive — D16 revised

`getLastGrammarState()` records a single theme; passing a state produced for one
theme into a `themes: {light, dark}` tokenization throws
`Grammar state themes "…" do not contain highlight theme "…"`.

So the "one CSS-variable flip switches light/dark instantly" mechanism of D16
**cannot be combined** with the incremental tokenization of § 3. Section 3 wins —
it is what makes the viewer usable at all.

**Adopted**: tokenize with the single active theme; a scheme change (including
the application-theme flip) **re-tokenizes** — the visible window first (~30 ms,
imperceptible), the rest by background sweep. Scroll position and selection are
untouched because only the existing line elements are re-styled, and nothing
navigates. FR-014 ("takes effect without reloading, keeps scroll and selection")
is still met; the "no re-tokenization" wording in D16 is not.

## 5. Gate defaults — confirmed, with the reasoning the numbers support

| Gate | Value | Why the measurements support it |
|---|---|---|
| Highlight limit | **1 MB** (configurable) | Viewport is highlighted in ~30 ms regardless of file size; the 7 s full sweep runs in a worker and never blocks. 1 MB is where total background work stays reasonable. |
| Max line length | **20 000 chars** | Unbounded single lines defeat chunking (a chunk is whole lines); a 2 MB single line is one indivisible unit. |
| Viewer limit | **20 MB** | Unchanged (mdview parity); decode + plain-render cost, not tokenizer cost, dominates here. |

## 6. Asset inventory (measured)

| Asset | Size |
|---|---|
| Engine bundle (`@shikijs/core` + Oniguruma + inlined WASM, minified ESM) | 718 KB |
| Language modules (`@shikijs/langs`, shipped as-is with their relative imports) | 9.4 MB across 361 files (242 languages + embedded sub-grammars) |
| Themes (12 shipped, VS Code JSON as ESM) | ~280 KB |

Language modules are served **as-is**: they are ESM with relative
`import './x.mjs'` between grammars, so the browser's own loader fetches exactly
the dependencies a language needs, through the interceptor. No per-language
bundling, no duplication of shared sub-grammars.

## 7. Licence audit outcome (D3, evidence-backed)

`tm-grammars` 1.32.8 metadata: MIT 176 · Apache-2.0 22 · GPL-3.0 5 · GNU 1 ·
NOASSERTION 12 · no field 21 · MPL-2.0 3 · ISC 1 · BSD-3 1 = **242**.

Of the 33 unlicensed/NOASSERTION entries, **21 were resolved to a permissive
licence** by fetching the source repository's licence text
(TextMate-permissive ×9: abap, applescript, erb, logo, mipsasm, po, ssh-config,
toml, yaml; MIT ×8: asciidoc, cue, luau, nim, nsis, purescript, sass, wolfram;
BSD ×2: apache, apex; Apache-2.0 ×2: elixir, llvm).

**Excluded (18)**:
- copyleft (6): `ada`, `ahk2`, `gnuplot`, `nginx`, `org`, `racket`
- unresolvable licence (12): `apl`, `dax`, `dream-maker`, `glsl`, `hurl`,
  `kusto`, `matlab`, `rel`, `sparql`, `tcl`, `ts-tags`, `turtle`
- theme: `aurora-x` (GPL-3.0)

**224 grammars ship** — still above the ≥ 200 of SC-001.

**Dependency finding**: excluded grammars are imported by shipped ones —
`glsl` ← cpp, cpp-macro, elm, es-tag-glsl, nim; `kusto` ← kql; `turtle` ←
sparql; `ts-tags` ← lit. Dropping the file would break the importer, so the
generator writes a **licence stub** (a module exporting `[]`) in its place: the
importing language keeps working and the excluded grammar contributes nothing.
Languages whose *only* content was an excluded grammar are marked "no grammar"
in the language map and open as plain text (FR-003).

## 8. Still to measure inside the real WebView (deferred to integration)

These need the actual embedded browser and are recorded in
`perf-results.md` during US7:

- warm controller attach time and page load through the interceptor;
- whether interceptor-served scripts populate the V8 code cache (decides
  whether the 718 KB engine is parsed once per session or once per window);
- renderer memory per viewer window;
- DOM cost of the virtual line list at 1 MB and the re-style cost of a scheme
  change.

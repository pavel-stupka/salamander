# Feature 070 — Survey of web (JavaScript) syntax-highlighting libraries

**Purpose**: choose the highlighting engine(s) and theme source for the new
F3 source/config viewer plugin (WebView2, HTML/CSS rendering, shared warm
engine per `architecture/11-webview2-integration.md`).
**Priorities from the brief**: (1) largest possible format coverage,
(2) several light *and* dark themes, then (3) offline footprint and
(4) behaviour on large files (1 KB … several MB, instant or graceful).
**Hard constraints**: fully offline (assets ship in the plugin, WebView locked
to a private virtual host), GPLv2-or-later compatible licences (MIT/BSD/ISC
fine; Apache-2.0 only via "or later" — flagged; copyleft-incompatible or
proprietary out), plain vendored files preferred (a documented, reproducible
one-time developer-side bundling step is acceptable).

**Date of survey**: 2026-08-26. **Method**: every version, date, count and
size below marked **[V]** was verified today — either from the npm registry
(`time`, `license`, `dist.unpackedSize`), from the project's own docs/repo, or
by downloading the current npm tarballs into the scratchpad and counting/measuring
files with a script (`measure.js`; tarballs: highlight.js 11.12.0, prismjs 1.30.0,
@shikijs/{core,primitive,types,langs,langs-precompiled,themes,engine-oniguruma} 4.4.3,
tm-grammars 1.32.8, tm-themes 1.12.9, @wooorm/starry-night 3.10.0,
web-tree-sitter 0.26.13, @codemirror/legacy-modes 6.5.3, monaco-editor 0.56.0,
ace-builds 1.44.0, vscode-oniguruma 2.0.1, linguist-languages 9.4.0,
@speed-highlight/core 2.1.0). Statements marked **[M]** are from memory /
general knowledge and were not re-verified; **[E]** are estimates derived
from verified numbers.

---

## 1. Summary & recommendation

### Ranked for *this* feature (coverage → themes → offline size → large files)

| Rank | Library | Why it lands here |
|---|---|---|
| **1** | **Shiki 4.x** (`@shikijs/core` + JS RegExp engine + `tm-grammars` + `tm-themes`) | 242 VS Code-grade TextMate grammars [V], 65 VS Code JSON themes (21 light / 44 dark) [V] and the whole VS Code theme ecosystem is drop-in; JS engine needs **no WASM** and covers 237/238 languages [V]; MIT, very active (27 releases in 12 months) [V]. Costs: ESM-only → one-time bundling; ~10 MB of grammar/theme data on disk [V]; slower than hljs/Prism and highlights whole files at once → needs chunked/virtualised rendering for MB-size files. Licence hygiene needed (6 GPL/GNU grammars, 1 GPL theme, 27 Apache-2.0 items, 10 unresolved) [V]. |
| **2** | **highlight.js 11.12** — as *fallback + auto-detect oracle* | 193 core grammars + 107 third-party listed [V], 258 CSS themes (82 + 176 base16) [V], BSD-3, zero deps, ~1.05 MB minified for *all* languages [V], plain `<script>`; `highlightAuto`/`ignoreIllegals`. Weak: theme/grammar class mismatch on niche languages, freezes on huge blocks (chunk or worker) [V], maintenance resumed Aug 2026 after a 20-month gap [V]. |
| **3** | **starry-night 3.10** (GitHub/linguist grammar set) — as optional *coverage extension* | 694 grammars in `all` [V], **no GPL** in the set (MIT 242, Apache 63, BSD 28, ISC 18, MPL-2.0 5, TextMate-permissive 15 …) [V]; only GitHub Primer CSS themes (7 light/dark variants) [V]; Oniguruma-WASM only; 11.8 MB unminified grammars [V]; author warns it "might be too heavy particularly in browsers" [V]. Its grammars are plain TextMate JSON and could be loaded into Shiki for the long tail (compatibility with the JS engine unverified — prototype question). |
| 4 | **CodeMirror 6** (read-only) | 162 languages (27 Lezer + 135 legacy stream modes) [V]; best large-file behaviour of all candidates (viewport rendering, parser work budget) [V]; 35 themes via `@uiw/codemirror-themes-all` [V]; MIT, active. But themes are CM-specific (Lezer tags → CSS), VS Code themes need conversion, needs a bundler. **The architecture to switch to if MB-size files must scroll instantly and Shiki-based virtualisation proves insufficient.** |
| 5 | **Ace 1.44** | 199 modes, 48 themes, extension map (`ext-modelist`, 195 entries), static-highlight extension, BSD-3, plain `<script>`, virtual renderer [V]. Dated regex-state-machine grammars (lower fidelity), Ace-only theme format, 5–10 MB [V]. |
| 6 | **Monaco 0.56** | 81 Monarch languages [V], 4 built-in themes (+ ~60 converted, approximate) [V], 6.4 MB trimmed / 23 MB full [V], MIT, AMD loader (no build step). Theme JSON is Monarch-token based, not TextMate-scope based → VS Code themes map only approximately. An editor, not a highlighter — overkill. |
| 7 | **web-tree-sitter 0.26** | Highest parsing fidelity, 0.36 MB runtime [V], but ~1.4 MB WASM **per grammar** (tree-sitter-wasms: 51.8 MB for ~37 grammars) [V], each grammar must be compiled (wasi-sdk) [V], no theme ecosystem (capture names → CSS by hand), no highlighter in the web binding. Not a "hundreds of formats" tool. |
| 8 | **Prism 1.30** | 297 languages, 8 + 48 themes, 0.55 MB, MIT, plain `<script>` [V] — but effectively frozen (last release 2025-03-10, 0 in the last 12 months; repo default branch `v2`, "only security-relevant PRs") [V], ReDoS/DOM-clobbering CVE history [V]. hljs is the better-maintained lightweight fallback. |
| — | speed-highlight (35 langs, CC0), sugar-high (JS/TS only), lowlight/refractor (hast wrappers over hljs/Prism) | Not contenders for coverage; wrappers add nothing for a WebView renderer. |

### Recommended combination: **Shiki (primary) + highlight.js (fallback)**

1. **Engine**: `@shikijs/core` + `@shikijs/engine-javascript` (default; no WASM,
   no JIT-policy exposure) with `@shikijs/engine-oniguruma` (`wasm-inlined`,
   622 KB [V]) as an optional switch for the single unsupported grammar/edge
   cases. Do **not** use `@shikijs/langs-precompiled` — its own docs say it is
   "not yet supported, due to a known issue that affects many languages" [V].
2. **Grammars**: `tm-grammars` (the exact data Shiki bundles) vendored as
   one ESM chunk per language, lazily `import()`ed from the virtual host;
   licence-filtered (see §4.3 / §5). Extension → language map from
   `linguist-languages` (MIT; 814 languages, 1,719 extensions; 247 languages /
   775 extensions join to a `tm-grammars` scope via `tmScope`) [V] plus
   Shiki's 104 aliases [V] and a hand-maintained override list.
3. **Themes**: `tm-themes` JSON (VS Code format) — ship a curated 10–15 (see
   §4.5), keep the loader generic so any VS Code theme JSON can be dropped in.
   Shiki emits per-token inline colours (or CSS variables with
   `defaultColor:false` / dual-theme mode [M]) so light/dark switching is a
   theme reload, not a CSS swap.
4. **Fallback**: highlight.js for (a) languages Shiki lacks or that are
   excluded for licence reasons (e.g. `nginx`, `ada`, `racket`, `gnuplot`,
   `org` are GPL-3.0 in tm-grammars [V] — hljs has `nginx`, `ada`, `scheme`
   under BSD [V]), (b) `highlightAuto` for extension-less/unknown files,
   (c) an emergency path for very large inputs (fast, trivially chunkable).
   Theme parity: generate an hljs CSS file from the *same* VS Code theme JSON
   at developer time (hljs has ~30 `hljs-*` classes; map each to a TextMate
   scope and take the theme's colour) so the user's chosen theme applies to
   both engines [design suggestion, not verified against every theme].
5. **Large files**: never build the full DOM at once. Tokenise per chunk
   (Shiki `codeToTokens` on ranges / idle callbacks or a Web Worker), render
   only the visible window from the token array (the plugin owns a simple
   virtual line list — read-only makes this easy), guard with
   `tokenizeMaxLineLength` (default 0 = unlimited [V]; set e.g. 10,000 for
   minified JS) and `tokenizeTimeLimit` (default 500 ms per line [V]); above
   a size threshold (to be benchmarked, e.g. 5–10 MB) show plain text with
   highlighting only for the visible window.

If prototyping shows that a Shiki-based virtual line list cannot make a
5 MB file feel instant, the fallback *architecture* is CodeMirror 6 in
read-only mode (rank 4), accepting fewer languages and CM-specific themes.

---

## 2. Comparison matrix

Sizes are **on-disk, uncompressed, as they would sit in the plugin folder**;
"max coverage" means every language + every theme the library offers.

| | highlight.js 11.12.0 | Prism 1.30.0 | Shiki 4.4.3 | CodeMirror 6 | Monaco 0.56.0 | web-tree-sitter 0.26.13 | starry-night 3.10.0 | Ace 1.44.0 |
|---|---|---|---|---|---|---|---|---|
| Languages | **193 core** (+107 3rd-party listed) [V] | 297 [V] | **242** grammars (+119 alias entries) [V] | 162 (27 Lezer + 135 legacy) [V] | 81 (Monarch) [V] | ~37 prebuilt WASM grammars (tree-sitter-wasms) [V]; ecosystem larger, each needs compiling | **694** (`all`), 34 (`common`) [V] | 199 mode files [V] |
| Alias / extension mapping | name aliases only (193 aliases) [V]; no extension map | name aliases (104) [V]; no extension map | aliases (104) [V]; no extension map — use `linguist-languages` (`tmScope` join: 247 langs / 775 ext) [V] | `LanguageDescription` has `extensions`/`filename` [V] | `languages.getLanguages()` carries `extensions` [M] | none (you pick the grammar) | `flagToScope(name\|ext\|path)` from linguist [V] | `ext-modelist` (195 entries, ext → mode) [V] |
| Themes | **258 CSS** (82 + 176 base16) [V] | 8 official + 48 in prism-themes [V] | **65 VS Code JSON** (21 light / 44 dark) [V]; any VS Code theme loads | 35 (`@uiw/codemirror-themes-all`) [V]; CM `HighlightStyle` format | 4 built-in (`vs`, `vs-dark`, `hc-black`, `hc-light`) [V] + ~60 converted (monaco-themes) [V]; Monaco token-rule JSON | none (capture → CSS by hand) | 7 light/dark CSS variants of GitHub Primer (15 files) [V] | 48 (Ace CSS/JS) [V] |
| Theme format | CSS classes (`hljs-*`) | CSS classes (`token.*`) | VS Code `tokenColors` (TextMate scopes) | JS `HighlightStyle` (Lezer tags) | `{rules:[{token,foreground}]}` (Monarch tokens) | CSS on capture names | CSS classes (`pl-*`) | CSS (`ace_*`) |
| Licence | BSD-3-Clause [V] | MIT [V] | MIT; grammars mixed (see §4.3) [V]; themes MIT/Apache/1×GPL [V] | MIT [V] | MIT (+ThirdPartyNotices) [V] | MIT; grammars per repo | MIT; grammars all permissive, no GPL [V] | BSD-3-Clause [V] |
| Max-coverage offline size | **≈1.4 MB** (20 KB core + 1.02 MB all langs min + 0.32 MB all themes min) [V] | ≈0.8 MB (57 KB core + 0.55 MB comps + themes) [V/E] | ≈10–11 MB (8.2 MB langs ESM + 1.4 MB themes + ~0.5 MB runtime [E] + 0.62 MB optional WASM) [V]; docs: full bundle 6.4 MB min / 1.2 MB gzip [V] | ≈3–5 MB [E] | 6.4 MB trimmed (no language services) / 23.3 MB full `min/vs` [V] | 0.36 MB runtime + ~1.4 MB per grammar [V] | ≈12.5 MB (11.8 MB langs + 0.6 MB textmate/onig + CSS) [V] | 9.6 MB full; ≈5.8 MB viewer subset [V] |
| Needs WASM | no | no | **no** with JS engine; optional Oniguruma (473 KB raw / 622 KB inlined) [V] | no | no | **yes** (runtime + every grammar) | **yes** (vscode-oniguruma) | no |
| Plain vendored files, no build step | yes — CDN build `highlight.min.js` + `languages/*.min.js`; npm tarball has **no** browser bundle (`es/core.js` is a CJS wrapper) [V] | yes (`prism.js` + `components/*.min.js`) [V] | **no** — ESM with bare specifiers → one-time bundle (esbuild) or import map + full dependency closure [V] | no — bundler (rollup) [M] | yes — AMD `min/vs/loader.js` [V] | yes — `web-tree-sitter.js` (+`.wasm`) [V] | no — ESM, bare imports (`vscode-textmate`, `vscode-oniguruma`, `hast-util-to-html`) [V] | yes — `src-min-noconflict/ace.js` [V] |
| Large-file behaviour | full block at once; known freezes (#954); docs → Web Worker; GitLab chunks 70 lines [V] | full block at once; ReDoS history [V] | full file tokenised at once; per-line guards `tokenizeMaxLineLength` (0) / `tokenizeTimeLimit` (500 ms) [V]; slower than Prism by design [V] | **viewport rendering + parser work budget** (million-line demo) [V] | virtualised; `maxTokenizationLineLength` 20,000, `largeFileOptimizations` [V] | full parse (fast, incremental API) [V]; you render | full block at once; "too heavy in browsers" (author) [V] | virtual renderer; >2 MB hang reported (2016, closed not-planned) [V] |
| Auto language detection | `highlightAuto` (relevance, secondBest) [V] — mediocre quality [M] | none | none | none (by name/extension) | none | none | none | none |
| Line numbers | plugin (`highlightjs-line-numbers.js`) [M] | `line-numbers` plugin [V] | per-line `<span class="line">` → CSS counters / transformers [M] | `lineNumbers()` gutter [M] | built-in | you render | AST transform example [V] | built-in |
| Maintenance | 11.12.0 **2026-08-12**, 11.11.2 2026-08-11, before that 11.11.1 2024-12-25 (20-month gap; "inactive since summer 2025" per release notes); commits 2026-08-23 [V] | 1.30.0 **2025-03-10**; 0 releases/12 mo; default branch `v2`, security PRs only [V] | 4.4.3 **2026-08-10**; 27 releases/12 mo; tm-grammars 67 releases/12 mo [V] | view 6.43.9 2026-08-16 (45 rel/12 mo); legacy-modes 6.5.3 2026-05-14; language-data 6.5.2 2025-10-23 [V] | 0.56.0 **2026-07-20**; 99 releases/12 mo (incl. dev builds) [V] | 0.26.13 **2026-08-23**; 14 rel/12 mo [V] | 3.10.0 **2026-06-08** (regenerated per linguist) [V] | 1.44.0 **2026-05-11**; 5 rel/12 mo [V] |
| Notable users | GitLab blob viewer, Stack Overflow, Discord [M] | many docs sites (Docusaurus via prism-react-renderer) [M] | VitePress, Astro, Nuxt Content, Nextra, Expressive Code [M] | Chrome DevTools, Replit, Sourcegraph, Obsidian [M] | VS Code for Web, Azure portal [M] | GitHub code navigation, Neovim, Helix, Zed [M] | GitHub's own grammar set (linguist) [V]; rehype-starry-night [M] | Cloud9 / AWS console [M] |

---

## 3. Per-library notes

### 3.1 highlight.js (hljs)

- **Versions/dates [V]**: npm latest 11.12.0 (2026-08-12), 11.11.2 (2026-08-11),
  11.11.1 (2024-12-25), 11.11.0 (2024-12-14). The 11.11.2 release notes say the
  project had been inactive since summer 2025 and merged PRs "through July
  2025"; the GitHub repo shows commits on 2026-08-18/19/23 (Astro added as
  3rd-party grammar, parser refactor). Reading: *revived*, single-maintainer risk.
- **Coverage [V]**: 193 core language files (`lib/languages`, excluding the
  `*.js.js` wrappers), 193 alias entries; `SUPPORTED_LANGUAGES.md` lists 300
  rows = 193 core + 107 third-party packages (separate repos, separate
  licences). Missing vs Shiki: e.g. no `gnuplot`, no `org`, no `hcl`/`terraform`
  (3rd-party), no `astro` (3rd-party), no `svelte` (3rd-party) [checked in the
  file list; 3rd-party status per README].
- **Themes [V]**: `styles/` 82 regular + `styles/base16/` 176 = 258 CSS themes,
  all under the repo's BSD-3 licence; includes github, github-dark,
  github-dark-dimmed, atom-one-light/dark, vs, vs2015, monokai, monokai-sublime,
  nord, tokyo-night-light/dark, rose-pine trio, night-owl, a11y-light/dark,
  intellij-light, xcode… Minified: 78 KB (regular) + 241 KB (base16).
- **Size [V]**: cdnjs 11.11.1 — `highlight.min.js` (common set, ~40 langs)
  125 KB; `es/core.min.js` 20 KB; all 192 `languages/*.min.js` sum to 1.02 MB
  (largest: 1c 160 KB, mathematica 124 KB, isbl 83 KB). So *all languages*
  ≈ 1.05 MB minified.
- **No build step [V]**: the CDN/website build gives classic-script files
  (`hljs.registerLanguage` per file). Note the **npm tarball ships no browser
  bundle**: `es/core.js` is a 202-byte wrapper that imports CJS `lib/core.js`
  (`es/languages/*.js` *are* real ESM). Vendor the CDN build or run
  `node tools/build.js :common` / a custom language list from the repo.
- **API [V]**: `highlight(code, {language, ignoreIllegals})` — `ignoreIllegals`
  "forces highlighting to finish even in case of detecting illegal syntax …
  instead of throwing"; `highlightAuto(code, languageSubset)` returns
  `language`, `relevance`, `secondBest`; `configure({languages, cssSelector,
  ignoreUnescapedHTML, throwUnescapedHTML})`; `registerAliases`.
- **Large files [V]**: issue #954 "Windows freeze with a huge code block";
  docs recommend running in a Web Worker; GitLab chunks files into 70-line
  pieces and highlights visible chunks via IntersectionObserver.
- **Weak spots [M]**: ~30 semantic classes only; niche grammars emit few
  classes, so themes look flat on them ("theme/grammar mismatch"); auto-detection
  is heuristic and often wrong for short or config-like files — restrict the
  subset.

Sources: https://registry.npmjs.org/highlight.js ·
https://github.com/highlightjs/highlight.js/releases ·
https://raw.githubusercontent.com/highlightjs/highlight.js/main/SUPPORTED_LANGUAGES.md ·
https://highlightjs.readthedocs.io/en/latest/api.html ·
https://highlightjs.readthedocs.io/en/latest/building-testing.html ·
https://github.com/highlightjs/highlight.js/issues/954 ·
https://gitlab.com/gitlab-org/gitlab/-/merge_requests/82806 ·
https://api.cdnjs.com/libraries/highlight.js/11.11.1?fields=files

### 3.2 Prism.js

- **Status [V]**: 1.30.0 published 2025-03-10 (previous 1.29.0 2022-08-23);
  no npm release in the last 12 months; the GitHub default branch is `v2`;
  README: "we are currently working on Prism v2 and will only accept
  security-relevant PRs for the time being"; no v2 prerelease on npm. 13k stars.
- **Coverage/themes [V]**: 297 languages (`components.json`), 104 aliases,
  8 official themes (default, coy, dark, funky, okaidia, solarizedlight,
  tomorrow, twilight), 26 plugins incl. `line-numbers`, `autoloader`,
  `normalize-whitespace`, `show-invisibles`; prism-themes repo: 48 CSS files
  (dracula, nord, one-dark/light, gruvbox, material, night-owl, vsc-dark-plus,
  atom-dark, a11y-dark …). Licence MIT (prism-themes MIT [M]).
- **Size [V]**: npm `prism.js` 57 KB (core + default 4 languages); all 298
  `components/*.min.js` 0.55 MB; site claims "core is 2 KB minified &
  gzipped, languages add 0.3–0.5 KB each".
- **Security [V]**: CVE-2024-53382 (DOM clobbering via `document.currentScript`,
  fixed in 1.30.0, CVSS 3.1), CVE-2022-23647 (command-line plugin XSS),
  CVE-2021-3801 (ReDoS). Relevant because a viewer feeds *untrusted* file
  content to the regexes.
- **Verdict**: fine technically for a lightweight fallback, but a frozen
  1.x with an unreleased rewrite is a worse bet than hljs for a 2026 product.

Sources: https://registry.npmjs.org/prismjs · https://prismjs.com/ ·
https://github.com/PrismJS/prism · https://github.com/orgs/PrismJS/discussions/3531 ·
https://api.github.com/repos/PrismJS/prism-themes/contents/themes ·
https://github.com/advisories/GHSA-x7hr-w5r2-h6wg

### 3.3 Shiki 4.x

- **Versions/dates [V]**: 4.4.3 (2026-08-10), 4.0.0 (2026-02-27, "cleanup of
  deprecated APIs", Node ≥ 20), 3.0.0 (2025-02-18), 1.0.0 (2024-02-07);
  27 releases in the last 12 months; MIT. Package graph: `shiki` (0.6 MB meta)
  → `@shikijs/core` (49 KB) + `@shikijs/primitive` + `@shikijs/types` +
  `@shikijs/vscode-textmate` (fork of vscode-textmate, MIT) +
  `hast-util-to-html` (MIT), `@shikijs/langs` (8.65 MB unpacked; 361 `.mjs`
  = 242 grammars + 119 alias re-exports), `@shikijs/themes` (1.48 MB; 65
  themes), `@shikijs/engine-oniguruma` (644 KB; `dist/wasm-inlined.mjs`
  622 KB base64 of the 473 KB `onig.wasm`), `@shikijs/engine-javascript`
  (11 KB + `oniguruma-to-es` 4.3.6 MIT ~1 MB unpacked incl. types/maps).
- **Grammar data [V]**: `tm-grammars` 1.32.8 (published 2026-08-26; 67
  releases/12 months): 242 grammars, 11.27 MB of JSON, metadata per grammar
  (`name, scopeName, aliases, license, licenseUrl, source, sha, lastUpdate,
  byteSize, categories, embedded`). Licence breakdown and the problem cases
  are in §4.3.
- **Theme data [V]**: `tm-themes` 1.12.9: 65 themes, 21 light / 44 dark,
  1.70 MB JSON; licences MIT 59, Apache-2.0 5 (all five Material variants),
  GPL-3.0 1 (Aurora X). Metadata: `name, displayName, type, license, source`.
- **Engines [V]** (docs "RegExp Engines"): Oniguruma WASM is the default
  ("maximum language compatibility, larger bundle"); the JavaScript engine
  transpiles Oniguruma patterns with `oniguruma-to-es` (uses the ES2024 `v`
  flag, falls back to `u`); compatibility table for 4.3.1 (generated
  2026-07-31): **237 of 238 languages supported, 0 mismatches, only `ahk2`
  fails** (1 of 188 patterns); `forgiving: true` skips failing patterns.
  `@shikijs/langs-precompiled` (skips runtime transpilation) is documented as
  "not yet supported, due to a known issue that affects many languages … use
  with caution" — **avoid**.
- **Bundles [V]**: `shiki/bundle/full` 6.4 MB minified / 1.2 MB gzip (async
  chunks included); `shiki/bundle/web` 3.8 MB / 695 KB; fine-grained
  `shiki/core` + per-language/per-theme modules "does not include any themes
  or languages or the wasm binary".
- **Performance [V]**: docs — highlighter instance is expensive, create once;
  fine-grained imports; JS engine "smaller bundles and faster startup";
  "Shiki highlights code using regular expressions, which can be CPU-intensive
  … consider Web Workers". Options: `tokenizeMaxLineLength` (default 0 = no
  limit; "lines above this length will not be tokenized"), `tokenizeTimeLimit`
  (default 500 ms per line), `includeExplanation` (default false — keep off).
  Discussion #846 (Nov 2024–Jan 2025): setup 760 ms with the web bundle vs
  45 ms with 8 languages; 1,200 markdown files: Prism 10 s, Shiki web bundle
  46 s, fine-grained + async ≈ 15 s ("50 % slower than Prism"); maintainer:
  "being faster than Prism is hard, if ever possible". Shiki output is one
  `<span class="line">` per line, so a virtual line list is natural [M].
- **No build step? [V]** No: ESM with bare specifiers (`@shikijs/types`,
  `@shikijs/primitive`, `hast-util-to-html`, `oniguruma-to-es`, `regex`, …).
  Options: (a) one-time developer-side `esbuild` bundle into
  `viewer-core.js` + per-language chunks (documented, pinned lockfile —
  reproducible); (b) `<script type="importmap">` mapping bare names to
  vendored files, which still means vendoring the full dependency closure
  (~20 packages, all MIT per registry: regex, regex-recursion,
  emoji-regex-xs, hast-util-to-html, @shikijs/vscode-textmate … [V]).
  (a) is the practical choice.
- **WASM note [V]**: the Oniguruma engine can be fed `wasm-inlined` (no fetch),
  a `Response`, an `ArrayBuffer` or a `WebAssembly.Module`; with the JS engine
  no WASM is loaded at all.

Sources: https://registry.npmjs.org/shiki · https://shiki.style/guide/regex-engines ·
https://shiki.style/references/engine-js-compat · https://shiki.style/guide/bundles ·
https://shiki.style/guide/best-performance · https://shiki.style/guide/install ·
https://shiki.style/blog/v4 · https://github.com/shikijs/shiki/discussions/846 ·
https://github.com/shikijs/textmate-grammars-themes (tm-grammars / tm-themes)

### 3.4 CodeMirror 6 (read-only)

- **Versions [V]**: `@codemirror/view` 6.43.9 (2026-08-16; 45 releases/12
  months), `@codemirror/language-data` 6.5.2 (2025-10-23), `@codemirror/legacy-modes`
  6.5.3 (2026-05-14; 103 mode files, 0.83 MB). MIT. Repo commits April 2026.
- **Coverage [V]**: `language-data.ts` has 162 `LanguageDescription` entries:
  27 backed by Lezer parsers (`@codemirror/lang-*`: cpp, css, go, html, java,
  javascript/jsx/tsx, jinja, json, less, liquid, markdown, php, python, rust,
  sass/scss, sql family, vue, angular, wast, xml, yaml) and 135 by legacy
  stream modes (C#, Clojure, CMake, Dockerfile, Erlang, F#, Haskell, Kotlin,
  Lua, Nginx, Objective-C, Pascal, Perl, PowerShell, Properties, Protobuf,
  Ruby, Scala, Shell, Swift, TOML, VB.NET, Verilog/VHDL, …). Each description
  carries `extensions`/`filename` patterns (so extension mapping is built in).
- **Large files [V]**: system guide — "CodeMirror doesn't render the entire
  document … detect which part of the content is currently visible … and only
  render that plus a margin"; million-line example — the parser "limits the
  amount of work it does", highlighting may stop far down and resumes when
  active. This is the best large-file story in the survey.
- **Read-only/theming [M, standard API]**: `EditorState.readOnly.of(true)` +
  `EditorView.editable.of(false)`; `lineNumbers()`; themes are `HighlightStyle`
  (Lezer highlight tags → CSS) plus an `EditorView.theme` for chrome; runtime
  switching via `Compartment.reconfigure` [V]. VS Code JSON themes are not
  loadable directly; community converters exist but tag ↔ scope mapping is
  lossy.
- **Themes [V]**: `@uiw/codemirror-themes-all` 4.25.11 (2026-07-08, MIT) bundles
  35 theme packages (dracula, monokai, nord, solarized, tokyo-night, gruvbox,
  vscode, github, …); `thememirror` 2.0.1 (2022, unmaintained).
- **No build step?** No — bare imports across `@codemirror/*`, `@lezer/*`,
  `style-mod`, `w3c-keyname`, `crelt` [M]; one-time rollup bundle.

Sources: https://registry.npmjs.org/@codemirror/language-data ·
https://raw.githubusercontent.com/codemirror/language-data/main/src/language-data.ts ·
https://codemirror.net/docs/guide/ · https://codemirror.net/examples/million/ ·
https://codemirror.net/examples/config/ · https://registry.npmjs.org/@uiw/codemirror-themes-all

### 3.5 Monaco Editor

- **Version [V]**: 0.56.0 (2026-07-20; 99 releases/12 months incl. dev builds);
  0.56 reorganised ESM into tree-shakeable entry points and added native LSP
  APIs; 0.55 moved language namespaces to top level. MIT + `ThirdPartyNotices.txt`.
- **Size [V]**: npm unpacked 97.9 MB / 1,909 files. `min/vs` (AMD, plain
  `<script>` via `loader.js`) = 23.3 MB — it double-ships the workers
  (`assets/ts.worker-*.js` 7.0 MB **and** `language/typescript/ts.worker.js`
  6.7 MB); without language services/workers the editor is ≈ 6.4 MB (editor
  chunk 2.39 MB, `toggleHighContrast` chunk 1.26 MB, CSS 342 KB, 81 language
  chunks, nls).
- **Languages [V]**: 81 Monarch "basic languages" (abap … yaml; no Haskell,
  no Erlang, no Nginx, no TOML — checked in the chunk list) + 4 language
  services (css/html/json/typescript).
- **Themes [V]**: built-in `vs`, `vs-dark`, `hc-black`, `hc-light`;
  `monaco-themes` (brijeshb42, MIT) has 61 entries (~60 themes) converted from
  TextMate/VS Code with `parseTmTheme` — approximate, because Monaco themes
  colour **Monarch token names**, not TextMate scopes [M]. Real VS Code themes
  need `monaco-vscode-api`/TextMate service overrides, which add many MB [M].
- **Large files [V]**: virtualised; `maxTokenizationLineLength` default 20,000
  (issue #3025: lowering it had no effect in some versions); `largeFileOptimizations`
  on by default. `monaco.editor.colorize`/`colorizeElement` exist for static HTML [V].
- **Verdict**: a full editor with the weakest coverage in the top group and a
  theme format that does not match the VS Code ecosystem; not worth 6–23 MB.

Sources: https://registry.npmjs.org/monaco-editor ·
https://raw.githubusercontent.com/microsoft/monaco-editor/main/CHANGELOG.md ·
https://github.com/brijeshb42/monaco-themes ·
https://github.com/microsoft/monaco-editor/issues/3025

### 3.6 web-tree-sitter

- **Version [V]**: 0.26.13 (2026-08-23; 14 releases/12 months); MIT; 0.25
  rewrote the binding in TypeScript (ABI 15); since 0.26.1 `tree-sitter build
  --wasm` uses wasi-sdk (auto-downloaded). Files: `web-tree-sitter.js` 154 KB
  (ESM) / `.cjs` 166 KB, `web-tree-sitter.wasm` 202 KB. Script-tag use
  documented (`window.TreeSitter`), `Parser.init({locateFile})`,
  `Language.load('…/tree-sitter-x.wasm')`.
- **Grammars [V]**: every language is its own WASM; `tree-sitter-wasms` 0.1.13
  (2025-10-07, Unlicense) ships 39 files / 51.8 MB (≈ 1.4 MB per grammar).
  Getting to "hundreds of formats" means compiling hundreds of grammars
  yourself; many config formats have no tree-sitter grammar at all.
- **Highlighting [M]**: the web binding has no highlighter; you run `Query`
  with each grammar's `highlights.scm` and map capture names (`@keyword`,
  `@string`, …) to CSS. Capture conventions differ between ecosystems
  (nvim-treesitter vs Helix vs GitHub), injections (embedded languages) are
  manual, and there is no theme collection to import.
- **Verdict**: excellent fidelity for a handful of languages; wrong tool for
  breadth + themes.

Sources: https://registry.npmjs.org/web-tree-sitter ·
https://github.com/tree-sitter/tree-sitter/blob/master/lib/binding_web/README.md ·
https://registry.npmjs.org/tree-sitter-wasms

### 3.7 starry-night (GitHub's TextMate set)

- **Version [V]**: 3.10.0 (2026-06-08, "Regenerate for linguist@9.6.0";
  3.9.0 2026-01-19); MIT; 14.6 MB unpacked / 2,216 files.
- **Coverage [V]**: `lib/all.js` imports 694 grammars, `lib/common.js` 34;
  `lang/` holds 719 files (11.8 MB, unminified ESM JSON), typical grammar
  3–5 KB; `flagToScope()` maps language names, extensions or paths via
  linguist data. README: "Bundled, minified, and gzipped, starry-night and
  the WASM binary are 185 kB" (common set).
- **Licences [V]** (from the shipped `notice`, SPDX per grammar): MIT 242,
  Apache-2.0 63, BSD-3 21, ISC 18, Unlicense 11, BSD-2 7, MPL-2.0 5
  (earthfile, overpassql, hcl/terraform, minizinc, vb6/vba), Zlib 2, WTFPL 1,
  CC0 1, "permissive" 15 (the classic TextMate-bundle licence: *"Permission to
  copy, use, modify, sell and distribute this software is granted …"* — used by
  c/c++, java, latex, make, perl, toml, d, sml, …). **No GPL** — linguist only
  admits permissive grammars.
- **Engine/deps [V]**: `vscode-textmate` 9.x (95 KB, MIT) + `vscode-oniguruma`
  2.0.1 (MIT; `onig.wasm` 473 KB + 20 KB JS; last release 2023-09) — WASM
  mandatory, no JS-regex option. ESM-only with bare imports → bundler.
- **Themes [V]**: 15 CSS files = core + light/dark/both for default, dimmed
  (dark), high-contrast, colorblind, tritanopia; `both.css` auto-switches via
  `prefers-color-scheme`. Classes are GitHub's `pl-*` names — a different
  vocabulary from hljs/Prism; only GitHub-look themes exist.
- **Performance [V]**: README: "starry-night might be too heavy particularly in
  browsers", recommends lowlight/refractor for lighter needs.
- **Use for us**: the grammar *set* (largest, GPL-free) is the interesting
  part. Its files are ordinary TextMate grammars and can be registered in Shiki
  as custom languages; whether each passes the JS-engine transpiler is
  unverified — a prototype question (§5).

Sources: https://registry.npmjs.org/@wooorm/starry-night ·
https://github.com/wooorm/starry-night · https://github.com/wooorm/starry-night/releases

### 3.8 Ace (added candidate: plain-script, no bundler)

- **Version [V]**: `ace-builds` 1.44.0 (2026-05-11; 5 releases/12 months);
  BSD-3-Clause; 55 MB unpacked (4 build flavours). `src-min-noconflict/` =
  9.63 MB: `ace.js` 464 KB, 199 `mode-*.js` (5.13 MB), 48 `theme-*.js`
  (204 KB), 11 workers, snippets; `ext-modelist.js` (195 mode entries with
  extension patterns) and `ext-static_highlight.js` (render highlighted HTML
  without an editor instance) are present.
- **Themes [V]**: ambiance, chaos, chrome, cloud9_day/night, clouds, cobalt,
  crimson_editor, dawn, dracula, dreamweaver, eclipse, github, github_dark,
  github_light_default, gruvbox (+hard variants), idle_fingers, iplastic,
  katzenmilch, kr_theme, kuroir, merbivore, monokai, mono_industrial,
  nord_dark, one_dark, pastel_on_dark, solarized_dark/light, sqlserver,
  terminal, textmate, tomorrow family, twilight, vibrant_ink, xcode.
- **Large files**: virtual renderer [M]; issue #3112 (2016) reports hangs
  above ~2 MB, closed "not planned" [V] — behaviour of 1.44 unverified.
- **Verdict**: the only "download two folders and go" option with decent
  breadth, but grammars are regex state machines of variable quality and the
  theme format is Ace-only; a viable plan B if bundling Shiki is rejected.

Sources: https://registry.npmjs.org/ace-builds · https://github.com/ajaxorg/ace/issues/3112

### 3.9 Others (checked, not shortlisted)

- **@speed-highlight/core 2.1.0** (2026-08-25, CC0-1.0): 35 languages, 7 themes
  (atom-dark, dark, default, github-dark/dim/light, visual-studio-dark),
  183 KB, ESM with relative imports only (works as a plain module) [V]. Too
  narrow.
- **sugar-high 2.1.0** (MIT): JS/TS-only tokenizer [V]. Not applicable.
- **lowlight 3.3.0** (hljs → hast, MIT, 2024-12) and **refractor 5.0.0**
  (Prism → hast, MIT, 2025-03) [V]: wrappers; same coverage as their engines;
  no benefit when we render HTML ourselves.
- **@vscode/vscode-languagedetection 1.0.23** (MIT, 1.8 MB; guesslang ML model
  on TF.js) [V]: content-based language detection; heavy for the marginal
  case of extension-less files — keep as an idea only.
- **linguist-languages 9.4.0** (MIT, 2026-06-08) [V]: 814 languages (546
  programming / 181 data / 69 markup), 1,719 extensions, plus `tmScope`
  (751 languages), `aceMode` (355), `codemirrorMode` (302). The `tmScope`
  field joins 247 languages (775 extensions) directly to `tm-grammars`
  scopes — the recommended source for the extension → grammar map.

---

## 4. Theme ecosystem

### 4.1 Portable theme collections

| Collection | Format | Count | Licence | Consumable by |
|---|---|---|---|---|
| VS Code themes via `tm-themes` [V] | VS Code JSON (`tokenColors` with TextMate scopes + `colors`) | 65 (21 light / 44 dark) | MIT 59 · Apache-2.0 5 (Material) · GPL-3.0 1 (Aurora X) | Shiki natively; Monaco only via lossy conversion; CodeMirror via conversion; hljs/Prism no |
| Any VS Code theme extension [M] | same | thousands on the Marketplace | per theme (most MIT) | Shiki (`loadTheme(json)`) |
| highlight.js `styles/` [V] | CSS (`.hljs-*`) | 82 + 176 base16 = 258 | BSD-3 (part of hljs) | hljs (and lowlight) |
| Prism official + `prism-themes` [V] | CSS (`.token.*`) | 8 + 48 | MIT | Prism (and refractor) |
| base16 / base24 — `tinted-theming/schemes` [V] | YAML palettes, builders emit any format | hundreds | MIT | anything via a builder; hljs already ships 176 base16 CSS |
| Ace `theme-*.js` [V] | CSS-in-JS (`.ace_*`) | 48 | BSD-3 | Ace |
| Monaco built-in + `monaco-themes` [V] | Monaco token rules JSON | 4 + ~60 | MIT | Monaco |
| `@uiw/codemirror-themes-all` [V] | CM6 `HighlightStyle` | 35 | MIT | CodeMirror 6 |
| starry-night `style/` [V] | CSS (`.pl-*`), GitHub Primer | 7 light/dark variants | MIT | starry-night |

Take-away: **only the VS Code JSON format is a real ecosystem**; every other
collection is bound to one library's class vocabulary. Choosing Shiki means
"any VS Code theme works"; choosing hljs/Prism/Ace means "the shipped CSS
list is the ecosystem".

### 4.2 Shortlist for the plugin (light + dark, all verified in `tm-themes` metadata unless noted)

| Theme | Light | Dark | Source repo | Licence | Also available as |
|---|---|---|---|---|---|
| GitHub Light / Dark (+ Default, Dimmed, High-Contrast variants) | ✔ | ✔ | primer/github-vscode-theme | MIT | hljs `github`, `github-dark`, `github-dark-dimmed` (BSD); starry-night CSS (MIT) |
| Light+ / Dark+ ("Visual Studio Light/Dark") | ✔ | ✔ | microsoft/vscode `theme-defaults` | MIT | Monaco `vs`/`vs-dark`; hljs `vs`, `vs2015` |
| One Light / One Dark Pro | ✔ | ✔ | akamud/vscode-theme-onelight, Binaryify/OneDark-Pro | MIT | hljs `atom-one-light/dark`; prism-themes; Ace `one_dark` |
| Solarized Light / Dark | ✔ | ✔ | microsoft/vscode (Schoonover palette, MIT) | MIT | hljs base16 `solarized-*`; Prism `solarizedlight`; Ace both |
| Dracula (+ Dracula Soft) | — | ✔ | dracula/visual-studio-code | MIT | hljs base16 `dracula`; prism-themes; Ace; monaco-themes |
| Nord | — | ✔ | arcticicestudio/nord-visual-studio-code | MIT | hljs `nord`; prism-themes; Ace `nord_dark` |
| Monokai | — | ✔ | microsoft/vscode `theme-monokai` | MIT | hljs `monokai`, `monokai-sublime`; Ace. (*Monokai Pro* is proprietary — avoid [M]) |
| Catppuccin Latte / Frappé / Macchiato / Mocha | ✔ (Latte) | ✔ ×3 | catppuccin/vscode | MIT | — |
| Gruvbox Light / Dark (hard/medium/soft) | ✔ | ✔ | jdinhify/vscode-theme-gruvbox | MIT | Ace `gruvbox*`; prism-themes |
| Tokyo Night | — | ✔ | enkia/tokyo-night-vscode-theme | MIT | hljs `tokyo-night-dark/light` |
| Rosé Pine / Moon / Dawn | ✔ (Dawn) | ✔ ×2 | rose-pine/vscode | MIT | hljs `rose-pine*` |
| Ayu Light / Mirage / Dark | ✔ | ✔ ×2 | ayu-theme/vscode-ayu | MIT | — |
| Everforest Light / Dark · Kanagawa Lotus / Wave / Dragon · Night Owl (+Light) · Vitesse Light / Dark / Black · Min Light / Dark · Slack Ochin / Dark · Snazzy Light | ✔ | ✔ | see tm-themes | MIT | hljs `night-owl` |
| **Material Theme** (5 variants incl. Lighter) | ✔ | ✔ | antfu/vsc-material-theme | **Apache-2.0 — flag** (OK only via "or later") | prism-themes material-* |
| **Aurora X** | — | ✔ | marqu3ss/Aurora-X | **GPL-3.0 — exclude** (or accept GPLv3 for the combined work) | — |

A default set of ~12 (GitHub L/D, Light+/Dark+, One L/D, Solarized L/D,
Dracula, Nord, Monokai, Catppuccin Latte+Mocha, Gruvbox L/D, Tokyo Night,
Rosé Pine Dawn) is entirely MIT.

### 4.3 Grammar licence audit for the Shiki path (tm-grammars 1.32.8) [V]

Licence field distribution: MIT 176 · Apache-2.0 22 · ISC 1 · BSD-3 1 ·
MPL-2.0 3 · GPL-3.0 5 · "GNU" 1 · NOASSERTION 12 · missing 21 (= 242).

- **Copyleft — exclude or accept GPLv3 terms for the combination**:
  `ada` (AdaCore/ada_language_server), `gnuplot`, `nginx`
  (hangxingliu/vscode-nginx-conf-hint), `org`, `racket` — GPL-3.0; `ahk2`
  (thqby/vscode-autohotkey2-lsp) — "GNU" (LICENSE is a GNU licence; version
  not verified). Under "GPLv2 or later" a GPL-3.0 grammar is *legal* but forces
  the combined distribution onto GPLv3 terms — a product decision. hljs
  covers `nginx`, `ada` (BSD); `scheme` stands in for `racket`.
- **Apache-2.0 (22)** — actionscript-3, ara, ballerina, cadence, cairo, chapel,
  erlang, gleam, gn, jsonnet, kdl, lean, mojo, moonbit, ocaml, pkl, polar,
  prisma, puppet, smithy, surrealql, typst; plus `elixir` (NOASSERTION in
  metadata, Apache-2.0 in its LICENSE) and `llvm` (Apache-2.0 WITH
  LLVM-exception [M]). Acceptable only via the "or later" clause — flag in
  the notices.
- **MPL-2.0 (3)** — bird2, hcl, terraform (hashicorp/syntax). GPL-compatible
  unless files carry the "Incompatible With Secondary Licenses" Exhibit B
  notice; the LICENSE file is the plain MPL text (verify per file).
- **Resolved from the source repos** (metadata blank/NOASSERTION, licence
  actually permissive): yaml, toml, ssh-config, po, erb, applescript, logo,
  mipsasm, abap → TextMate-bundle permissive licence ("Permission to copy,
  use, modify, sell and distribute this software is granted"); sass MIT;
  apache (colinta/ApacheConf.tmLanguage) BSD; apex (forcedotcom) BSD; asciidoc,
  cue, nim, nsis, luau, wolfram, purescript, rel — MIT.
- **Still unresolved (no licence file found at repo root)** — glsl
  (polym0rph/GLSL.tmbundle), matlab (mathworks/MATLAB-Language-grammar), tcl
  (sleutho/tcl), sparql + turtle (stardog-union/stardog-vsc), apl, dream-maker,
  hurl, dax, kusto; `ts-tags` has no source at all. Verify manually (licence
  may sit in `package.json` or a sub-folder) or drop; hljs has `glsl`,
  `matlab`, `tcl` (BSD) as substitutes.

The starry-night set needs no such audit (no GPL, §3.7), which is one reason
it is attractive as the long-tail extension.

---

## 5. Risks & open questions

### Risks

1. **Shiki throughput on MB-size files.** Regex tokenisation of a whole file
   before first paint is the wrong shape for a viewer; the maintainer states
   Shiki will not beat Prism. Mitigation is architectural (chunked/idle
   tokenisation, render only the visible window, `tokenizeMaxLineLength` for
   minified single-line files, plain-text above a threshold, optional Web
   Worker). **Must be benchmarked on 1 / 5 / 20 MB samples (log, JSON,
   minified JS, C++) before the design is frozen.**
2. **Cold start.** Discussion #846: 760 ms to set up the *web bundle* vs 45 ms
   with 8 languages — load one grammar + one theme first and lazy-`import()`
   the rest; the warm WebView2 process (feature 065) removes browser start-up
   but not JS module parsing. Whether Chromium's code cache applies to modules
   served through `WebResourceRequested` is unknown — measure.
3. **Oniguruma WASM.** 473 KB to decode/compile on first use; the
   `DefaultJavaScriptJitSetting` Edge policy (JIT off) "can also disable parts
   of JavaScript including WebAssembly" [V], and WebView2 has profile-level
   Enhanced Security Mode (off by default) [V]. Defaulting to the JS engine
   sidesteps all of it; keep WASM as opt-in. If WASM is used, serve it via the
   interception path with `Content-Type: application/wasm` (or use
   `wasm-inlined`) — issue #4838 reports intermittent 404s for `.wasm` under
   `SetVirtualHostNameToFolderMapping` (WinUI3, runtime 129, open) [V]; a CSP
   would need `'wasm-unsafe-eval'` [M].
4. **Licence hygiene** (§4.3): 6 copyleft grammars + 1 GPL theme to exclude,
   27 Apache-2.0 items to flag, 10 unresolved grammars to verify or drop, MPL
   Exhibit-B check. Also `tm-grammars` churns (67 releases/12 mo) — pin and
   regenerate via a script that re-runs the audit.
5. **hljs fallback quirks**: theme/grammar class mismatch on niche languages,
   `highlightAuto` misfires, freezes on huge blocks (chunk it), single
   maintainer with a 20-month gap in 2025–26.
6. **Build step**: Shiki needs a one-time esbuild bundle (pinned lockfile,
   documented in `tools/`); this is the first npm-derived artefact in the repo —
   confirm it is acceptable under the "plain vendored files" preference.
7. **Precompiled grammars** are advertised as broken by Shiki's own docs —
   do not chase the startup gain there.
8. **Extension ambiguity**: `.h` (C vs C++ vs Objective-C), `.pro` (Qt vs
   INI vs Prolog), `.m` (MATLAB vs Objective-C), `.ts` (TypeScript vs XML
   TS files) — needs an override list and possibly a content sniff.

### Open questions

1. Benchmark results and the size thresholds (instant / chunked / plain).
2. Product decision on GPL-3.0 grammars/theme (exclude vs accept GPLv3 for
   the combined work) and on Apache-2.0 items (flag-and-accept via "or later").
3. Load starry-night/linguist grammars into Shiki for the long tail? Requires
   a compatibility run of all 694 grammars through `oniguruma-to-es`
   (`forgiving` fallback → Oniguruma engine → hljs).
4. Theme parity for the hljs fallback: generate hljs CSS from the chosen VS
   Code theme JSON (developer-side) vs ship a fixed small CSS set.
5. Serving ~600 grammar/theme chunks: virtual-host folder mapping vs mdview's
   in-memory interception (precedent: default-deny + `CreateWebResourceResponse`)
   — and whether a Web Worker script can be loaded under that lockdown.
6. Ownership of the extension map (linguist-languages + Shiki aliases +
   overrides) and how it is regenerated.

---

## 6. Sources

Registry/metadata (npm): https://registry.npmjs.org/highlight.js ·
https://registry.npmjs.org/prismjs · https://registry.npmjs.org/shiki ·
https://registry.npmjs.org/@shikijs/langs · https://registry.npmjs.org/@shikijs/themes ·
https://registry.npmjs.org/@shikijs/engine-oniguruma · https://registry.npmjs.org/@shikijs/engine-javascript ·
https://registry.npmjs.org/tm-grammars · https://registry.npmjs.org/tm-themes ·
https://registry.npmjs.org/@wooorm/starry-night · https://registry.npmjs.org/monaco-editor ·
https://registry.npmjs.org/web-tree-sitter · https://registry.npmjs.org/tree-sitter-wasms ·
https://registry.npmjs.org/@codemirror/language-data · https://registry.npmjs.org/@codemirror/legacy-modes ·
https://registry.npmjs.org/@codemirror/view · https://registry.npmjs.org/@uiw/codemirror-themes-all ·
https://registry.npmjs.org/ace-builds · https://registry.npmjs.org/@speed-highlight/core ·
https://registry.npmjs.org/sugar-high · https://registry.npmjs.org/lowlight · https://registry.npmjs.org/refractor ·
https://registry.npmjs.org/vscode-oniguruma · https://registry.npmjs.org/vscode-textmate ·
https://registry.npmjs.org/oniguruma-to-es · https://registry.npmjs.org/linguist-languages ·
https://registry.npmjs.org/@vscode/vscode-languagedetection

Docs/repos: https://github.com/highlightjs/highlight.js/releases ·
https://raw.githubusercontent.com/highlightjs/highlight.js/main/CHANGES.md ·
https://raw.githubusercontent.com/highlightjs/highlight.js/main/SUPPORTED_LANGUAGES.md ·
https://highlightjs.readthedocs.io/en/latest/api.html ·
https://highlightjs.readthedocs.io/en/latest/building-testing.html ·
https://highlightjs.org/download · https://github.com/highlightjs/highlight.js/issues/954 ·
https://gitlab.com/gitlab-org/gitlab/-/merge_requests/82806 ·
https://prismjs.com/ · https://github.com/PrismJS/prism · https://github.com/PrismJS/prism/releases ·
https://github.com/orgs/PrismJS/discussions/3531 · https://github.com/advisories/GHSA-x7hr-w5r2-h6wg ·
https://shiki.style/guide/regex-engines · https://shiki.style/references/engine-js-compat ·
https://shiki.style/guide/bundles · https://shiki.style/guide/best-performance ·
https://shiki.style/guide/install · https://shiki.style/languages · https://shiki.style/themes ·
https://shiki.style/blog/v4 · https://github.com/shikijs/shiki/discussions/846 ·
https://github.com/shikijs/textmate-grammars-themes ·
https://raw.githubusercontent.com/codemirror/language-data/main/src/language-data.ts ·
https://codemirror.net/docs/guide/ · https://codemirror.net/examples/million/ ·
https://codemirror.net/examples/config/ ·
https://raw.githubusercontent.com/microsoft/monaco-editor/main/CHANGELOG.md ·
https://github.com/brijeshb42/monaco-themes · https://github.com/microsoft/monaco-editor/issues/3025 ·
https://github.com/tree-sitter/tree-sitter/blob/master/lib/binding_web/README.md ·
https://github.com/tree-sitter/tree-sitter/releases ·
https://github.com/wooorm/starry-night · https://github.com/wooorm/starry-night/releases ·
https://github.com/ajaxorg/ace/issues/3112 · https://github.com/tinted-theming/schemes ·
https://api.github.com/repos/PrismJS/prism-themes/contents/themes ·
https://api.github.com/repos/brijeshb42/monaco-themes/contents/themes ·
grammar source repos listed in §4.3 (raw LICENSE / README.mdown fetched 2026-08-26)

WebView2: https://github.com/MicrosoftEdge/WebView2Feedback/issues/4838 ·
https://learn.microsoft.com/en-us/microsoft-edge/webview2/how-to/webresourcerequested ·
https://learn.microsoft.com/en-us/deployedge/microsoft-edge-browser-policies/defaultjavascriptjitsetting ·
https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/security ·
`src/plugins/mdview/webview.cpp` (interception precedent) ·
`architecture/11-webview2-integration.md`

Local measurements: scratchpad `pkgs/measure.js` over the tarballs listed in
the header (not part of the repo).

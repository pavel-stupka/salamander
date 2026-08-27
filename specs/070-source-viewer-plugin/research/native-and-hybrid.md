# Research: native (C/C++) and hybrid alternatives to a WebView2 + JS highlighter

**Feature**: 070-source-viewer-plugin · **Date**: 2026-08-26 · **Status**: input to `plan.md`
**Question**: Is the "WebView2 web view + JavaScript highlighting library" route the best fit for the
source viewer's priorities — (1) maximum number of formats, (2) several light **and** dark themes,
(3) instant display, (4) graceful large-file behaviour — or does a native or hybrid route beat it
on coverage, themes or robustness?

Verification legend used throughout: **[V]** verified today from a primary source (URL in
§ Sources; GitHub facts via the REST API, counts are directory listings at HEAD), **[R]** read
in this repository (file:line), **[M]** from memory / general knowledge, not re-verified today.
Claims marked **[M]** that are load-bearing for a decision are listed again in § Open questions.

---

## 1. Summary & recommendation

**Answer: yes — for these four priorities the WebView2 + JS-highlighter route is the best fit,
and no native or hybrid route beats it on coverage or themes.** One hybrid route beats it on
*robustness* (no script engine in the rendering path, no regex-DoS surface), and one native
route beats it on *large files* — neither of those is priority 1 or 2, and both are achievable
inside the JS route with gates that mdview already has.

Ranked (best fit first):

| # | Route | Formats | Light+dark themes | Instant | Large files | Effort | Verdict |
|---|-------|---------|-------------------|---------|-------------|--------|---------|
| 1 | **WebView2 + JS highlighter** (scripts ON for *this* plugin's controllers) | 194 languages (highlight.js core) or 260 TextMate grammars (Shiki) [V] | 90 CSS themes (highlight.js) / 65 VS Code themes (Shiki) [V] | warm shared engine (contract 065) | size gate + optional JS virtualisation | lowest for the coverage obtained | **Recommended** |
| 2 | **Hybrid: C++ tokenizer → static HTML, scripts OFF** (Lexilla and/or Scintillua) | 125 Lexilla lexers [V] (+160 Scintillua LPeg lexers [V], union larger) | CSS variables, themes authored by us; permissive template sources exist | warm shared engine | static DOM; needs a highlight cap or paging | medium: vendoring + per-lexer style→class table | **Fallback** if scripts-on is rejected |
| 3 | **Native Scintilla + Lexilla control** | 125 (+160) | per-style colours; per-lexer theme tables must be authored (the Notepad++ XML source is GPLv3) | native, no browser | best of all routes (hundreds of MB, lazy styling) | medium-high: ~250-file vendoring + a second rendering technology | Only if large files outrank formats/themes |
| 4 | **Colorer (colorer-take5) library** → hybrid HTML | 367 HRC scheme files [V] (≈300 languages, project claim [M]) | 17 RGB HRD schemes, ~half dark [V] | as hybrid | line-cached parser, backtracking limit [V] | high: CMake-only, libxml2 + ICU-or-legacy strings | Coverage booster for #2; not first choice |
| 5 | **Tree-sitter (C)** | 1 grammar per vendored parser.c (C++ alone is 25.9 MB of C) [V] | mapping written by us (nvim: Apache-2.0, helix: MPL-2.0 queries) | as hybrid | linear parse, whole-file tree in RAM | very high for "hundreds" | Not for this feature |
| 6 | **TextMate engine in C++** | 260 grammars available [V] — but **no C++ engine exists** [V-search] | 65 themes | as hybrid | ours | highest (port vscode-textmate + vendor Oniguruma, which is archived [V]) | Not recommended |
| 7 | **Extend the built-in text viewer** | whatever lexer we write/port | 4 colours today [R] | native | streaming (60 KB window) [R] | invasive in a 7.4 k-line legacy core | Not the primary route |

The three facts that decide it:

1. **Coverage gap is 2× in favour of web libraries**: Lexilla ships 125 lexers, Scintillua 160,
   Colorer 367 scheme files; highlight.js ships 194 language modules and Shiki's grammar package 260
   TextMate grammars [V]. Only Colorer approaches web-library breadth, and it is the hardest to
   integrate (§4.4).
2. **Themes are free only on the web route**: 90 highlight.js CSS themes / 65 Shiki themes are
   ready-made, licensed permissively, and light/dark in roughly equal numbers [V]. Every native route
   needs a per-lexer style→role mapping written by us (Lexilla has no cross-language token classes —
   each lexer numbers its own `SCE_*` styles [M, high confidence]); the obvious ready-made source,
   Notepad++'s 22 theme XML files, is **GPLv3** [V] and therefore a licensing conflict under
   constitution IV.
3. **The robustness argument for native is real but bounded**: hand-written Lexilla lexers are
   linear state machines (no regex, no catastrophic backtracking); JS highlighters are regex-driven
   and have a documented ReDoS history [M]. In the JS route this is mitigated with the gates mdview
   already has (size gate, `ProcessFailed` → text-viewer fallback [R]) plus a Web-Worker timeout;
   it is not a reason to give up 2× coverage.

What the JS route must accept (and the plan must state explicitly): **scripts are enabled on this
plugin's controllers**. The shared-engine contract allows this per controller
(`architecture/11-webview2-integration.md` § 2.3: "a WebGPU plugin may enable scripts on *its*
controllers" [R]); mdview's own lockdown is untouched. The C++ side stays the "server": encoding
detection, size gate, chunking and the default-deny virtual host are reused unchanged — the JS
code only receives already-escaped text and a theme id.

---

## 2. Comparison matrix

| Criterion | Scintilla + Lexilla (native control) | Lexilla/Scintillua → static HTML (hybrid) | Colorer → HTML (hybrid) | Tree-sitter (C) | TextMate engine (C++) | Built-in viewer + lexer | WebView2 + JS (baseline) |
|---|---|---|---|---|---|---|---|
| Format coverage | 125 lexers [V]; +160 Scintillua LPeg lexers via Lua [V] | same as left | 367 `.hrc` files [V]; ~300 languages [M] | 1 per vendored grammar; nvim queries 323 dirs, helix 341 dirs [V] | 260 grammars (Shiki tm-grammars) [V] | ours (port a few) | 194 (hljs) / 260 (Shiki) [V] |
| Coverage kind | hand-written C++ lexers | same (+ LPeg) | XML regex schemes | generated LR parsers + `.scm` queries | Oniguruma-regex grammars | ad hoc | regex JS modules / TextMate grammars |
| Theme story | per-style colours; per-lexer tables authored by us; SciTE `.properties` is a permissive role-mapping template [V] | CSS vars (mdview pattern) + same mapping table | 17 RGB HRD (dark: black, blue, navy, mirror, neo, violet, FalloutRed…) [V], LGPL-2.1 | capture names → colours, written by us | tm-themes 65 [V], per-theme licences | 4 colours in config [R] | 90 CSS (hljs, BSD-3 [M]) / 65 (Shiki) [V] |
| Dark themes | yes (data) | yes (CSS) | yes (≈8 of 17 RGB) | yes (data) | yes | none today | yes, dozens |
| Licence vs GPLv2-or-later | HPND-style permissive [V] — compatible | same + MIT (Scintillua, Lua, LPeg [M]) | MIT lib + LGPL-2.1 schemes [V] — compatible | MIT core [V]; grammars mostly MIT (4 sampled MIT [V]); queries Apache-2.0 (nvim) **incompatible with GPLv2-only**, MPL-2.0 (helix) compatible [M] | Oniguruma BSD-2 [V]; vscode-textmate MIT [V] | n/a | MIT/BSD-3 [M] |
| Footprint (shipped) | ~250 source files; DLL ~1–2 MB [M] | Lexilla only: ~130 files [M]; +Lua/LPeg ~40 files [M] | library + libxml2 (+ICU or legacy strings) + schemes dir (MBs of XML) [M] | 1–26 MB of C **per grammar** [V]; binary MBs [M] | Oniguruma ~100 k LOC C [M] + engine ~8 k LOC C++ [M] | none | JS bundle 1–2 MB (hljs) / 10 MB+ (Shiki wasm+grammars) [M] |
| Large files | best: lazy styling, `SC_DOCUMENTOPTION_TEXT_LARGE` for >2 GB [V]; whole file in RAM ×2 (style bytes) [M] | static DOM; highlight cap or paging needed | line-level cache; backtracking limit 10⁶ steps [V] | whole tree in RAM; parse is fast [M] | whole file tokenised; regex cost | streaming 60 KB window [R] — ideal, but stateful lexing awkward | static DOM unless scripted virtualisation |
| Instant display | native window | warm engine (065) | warm engine | warm engine | warm engine | native | warm engine |
| Integration effort | vcxproj for Scintilla+Lexilla, `STATIC_BUILD` + `Scintilla_RegisterClasses` [V], new window/plumbing, theme tables | vcxproj for Lexilla, fake `IDocument`, style→class table, lift mdview host | CMake→vcxproj port, 2 new deps, UTF-16 bridge, HRD→CSS | grammar build farm, query runner incl. predicates + injections, theme mapping | full engine port | core changes across `viewer*.cpp`, config, dark colours | copy mdview host, enable scripts, bundle JS, CSS themes |
| Maintenance (2026) | Scintilla 5.6.6 / Lexilla 5.5.3 on 2026-08-12 [V] | same; Scintillua pushed 2026-08-06 [V] | lib v1.6.0 2026-08-21, FarColorer 1.7.1 2026-08-26 [V] | 0.26.13 2026-08-23 [V] | Oniguruma **archived 2025-04-24** [V]; vscode-textmate active [V] | n/a | hljs 11.12.0 2026-08-12 [V] |
| Main risk | second rendering technology; per-lexer themes; DPI/dark scrollbars | coverage ceiling; theme authoring | build/deps; own regex engine; UTF-16 bridge | size/toolchain; C highlight API is Rust [V] | effort; archived regex engine | invasive; no theme infra | scripts on; ReDoS hang; bundle size |

---

## 3. Per-option notes

### 3.1 Scintilla + Lexilla (native Win32 editing control, read-only)

**What it is.** Scintilla is the editing control behind Notepad++, SciTE, Geany, etc.; Lexilla is
its separated lexer library (Scintilla 5 split them). Both are maintained by Neil Hodgson; current
releases **Scintilla 5.6.6 and Lexilla 5.5.3, both 2026-08-12** [V — ScintillaHistory /
LexillaHistory]; releases roughly monthly through 2026 (5.6.4 on 07-06, 5.6.5 on 08-04).

**Coverage.** The `lexers/` directory of Lexilla at HEAD contains **125 `Lex*.cxx` files** [V —
GitHub API]. The feature's named formats are all covered: C/C++ (LexCPP also does JS/TS/Java/C#
family via keyword lists), YAML, TOML, Properties/INI (LexProps), PHP/HTML (LexHTML), JSON, Bash,
Batch, PowerShell, Python, Rust, Markdown, Makefile, CMake, Diff, Registry, Inno, NSIS, SQL/MySQL,
Lua, Perl, Ruby, Nix, Zig, Dart, GDScript, etc. Missing vs. web libraries (spot check, [M]): Kotlin,
Swift, Go (handled by LexCPP keyword sets in practice), Dockerfile, GraphQL, Protobuf, Terraform/HCL,
Elixir, Clojure, Scala have no dedicated lexer. No 2026 release added a lexer [V — LexillaHistory].
**Scintillua** (MIT, pushed 2026-08-06) supplies **160 LPeg lexers** written in Lua, usable as a
Lexilla-compatible lexer library or "as a standalone Lua library for syntax highlighting support"
[V — GitHub description]; it requires Scintilla ≥ 5.0.1 / Lexilla ≥ 5.1.0 for drop-in use [V —
search]. Vendoring Lua + LPeg (both MIT [M]) is small. Scintillua also uses a *fixed set of named
tags* (`COMMENT`, `STRING`, `KEYWORD`, `TYPE`, `FUNCTION`, …) [M, high confidence], which is exactly
the cross-language token-class abstraction Lexilla lacks.

**Themes.** Scintilla styling is per style number: each lexer defines its own `SCE_<LEXER>_*`
numbers (0–127 usable; 32–39 reserved for defaults/line numbers/etc.) [M, high confidence];
`SCI_STYLESETFORE/BACK/BOLD/ITALIC` set colours per style [V — ScintillaDoc]. Therefore a
"theme" is a table *per lexer*, not per token class. Ready-made sources:
- **Notepad++** ships 22 theme XML files in `PowerEditor/installer/themes` (incl.
  `DarkModeDefault.xml`, Monokai, Solarized, Zenburn, Obsidian, Twilight, …) [V — API listing]
  and 6 more in `nppThemes` [V]. **Both are GPL-3.0** [V — API licence field; Notepad++ moved
  GPLv2→v3 in January 2021, search-verified]. Shipping them would force the distributed
  combination to GPLv3, which the constitution's principle IV ("MUST NOT introduce licensing
  conflicts with GPLv2") rules out. Colour *values* are not copyrightable but the XML files are;
  re-typing a palette from a permissively licensed origin (Solarized, Dracula, Nord, Gruvbox,
  Catppuccin, One Dark, GitHub — all MIT [M]) is the clean path.
- **SciTE's `*.properties`** (same permissive Scintilla licence) map every lexer's style numbers
  to shared role variables — e.g. `style.cpp.1=$(colour.code.comment.box)`,
  `style.cpp.4=$(colour.number)`, `style.cpp.5=$(colour.keyword),bold`,
  `style.cpp.6=$(colour.string)` [V — SciTE `cpp.properties`]. That is a permissive, complete
  **style→role table for all lexers** — the cheapest way to get "N themes × 125 lexers" from
  "N palettes × ~12 roles". This is the key enabler for any Lexilla-based route.
- **KSyntaxHighlighting** has 30 `.theme` JSON files (atom-one, ayu, breeze, catppuccin ×4,
  dracula, github ×2, gruvbox ×2, monokai, nord, solarized ×2, tokyo-night ×3, vscodium-dark …)
  [V — API listing]; per-file MIT [M] — usable as palette sources, not as Scintilla themes.

**Rendering.** Win32 platform layer supports GDI or Direct2D/DirectWrite via
`SCI_SETTECHNOLOGY(SC_TECHNOLOGY_DIRECTWRITE)` [V — search/Scintilla.h]; 2026 releases fixed
DirectWrite sharpness and multi-monitor scaling [V — ScintillaHistory 5.6.2]. Dark mode: text
colours are styles; the native scrollbars need `SetWindowTheme(..., L"DarkMode_Explorer")` the way
Notepad++ does [M]. DPI: handled inside `ScintillaWin` for font metrics [M — unverified whether
`WM_DPICHANGED` is fully internal].

**Read-only & features.** `SCI_SETREADONLY(true)` (modification attempts raise
`SCN_MODIFYATTEMPTRO`) [V]; line-number margin, folding, `SCI_SETWRAPMODE`, `SCI_SETZOOM`,
`SCI_SEARCHINTARGET`/`SCI_FINDTEXT` all present [V — ScintillaDoc]. Idle styling
(`SCI_SETIDLESTYLING`, values NONE/TOVISIBLE/AFTERVISIBLE/ALL) styles lazily [V].

**Large files.** The whole file is loaded into the control (text + one style byte per byte [M])
— memory ≈ 2× file size plus the line index; documents above 2 GB need
`SC_DOCUMENTOPTION_TEXT_LARGE` (`SCI_CREATEDOCUMENT` option) [V — ScintillaHistory]. Styling is
lazy and only the visible region is lexed at open, so a 200 MB log opens in seconds (Notepad++
behaviour [M]); wrap mode on huge files is slow because layout of all lines is needed for the
scroll extent (idle wrap mitigates) [M]. This is the best large-file story of all routes except
the streaming built-in viewer.

**Licence.** "Permission to use, copy, modify, and distribute this software and its documentation
for any purpose and without fee is hereby granted, provided that the above copyright notice appear
in all copies …" (Neil Hodgson, 1998–2021) [V — License.txt] — HPND-style, GPLv2-compatible; add
to `doc/` third-party notices.

**Vendoring / build.** Source layout `scintilla/{include,src,win32}` + `lexilla/{include,lexlib,
src,lexers}`; Scintilla requires C++17 [M] (the product is `/std:c++latest`, fine). Upstream builds
with `nmake -f scintilla.mak` / `nmake -f lexilla.mak` [V — search]. For MSBuild: one static-lib
`.vcxproj` per library under `src/common/dep/scintilla/` and `src/common/dep/lexilla/` (the
`webview2`/`md4c` precedent in `src/common/dep/` [R] and `mdview.props` include/lib wiring [R
`src/plugins/mdview/vcxproj/mdview.props:9,15,16`]). Static linking: define `STATIC_BUILD` and
call `Scintilla_RegisterClasses(hInstance)` (registers the `"Scintilla"` window class; prevents a
conflicting `DllMain`) [V — search]; Lexilla defines `LEXILLA_NO_EXPORT` for static use and the
application calls `CreateLexer("cpp")` → `ILexer5` [V — LexillaDoc]. No Scintilla, Lua or regex
engine is currently vendored (`src/common/dep/`: bzip2 crypt fmt libssh2 md4c nanosvg pnglite
sqlite webview2 wil zlib) [R]. Roughly 250 files; the largest vendored dep so far is libssh2.

**Constitution VI.** Scintilla is its **own window class**, not a restyled standard control. The
text of VI forbids restyling *standard* controls and process-wide visual changes (no
`ICC_STANDARD_CLASSES`, no manifest, no owner-drawn edit "purely to restyle") [R
`.specify/memory/constitution.md:131-143`]; a self-contained custom control as a viewer *surface* is
the same category as the WebView2 surface mdview already uses, and the plugin's dialogs stay
`DIALOGEX`/`DS_SHELLFONT`. Not a violation by the letter — but it is a **second rendering
technology** for text viewing next to WebView2, which is the "one program" spirit VI protects.
Flag for the owner.

**Risks.** Two viewer technologies; per-lexer theme tables (mitigated by SciTE properties); dark
scrollbars/DPI polish; larger vendoring diff; the warm-WebView2 investment of feature 065 unused.

### 3.2 Tree-sitter (native C library)

**Facts.** Core is MIT, v0.26.13 released 2026-08-23 (0.26.10/11/12 in June–August) [V —
releases page/API]. `TREE_SITTER_LANGUAGE_VERSION 15`, `MIN_COMPATIBLE 13` [V — api.h].
Grammars are generated C: `parser.c` sizes at HEAD — **C++ 25,857,209 bytes**, TypeScript
8,745,894, PHP 7,283,963, C 3,872,012, JavaScript 2,855,934, YAML 1,289,047 [V — API]; several
also ship a hand-written `scanner.c`/`.cc`. Sampled grammar licences: cpp, php, yaml, toml all MIT
[V]. Highlight queries: nvim-treesitter `runtime/queries` has **323** language directories,
licence Apache-2.0, repo **not archived** (pushed 2026-08-23) [V — API; an earlier web-search
summary claiming "archived April 2026" was wrong]; Helix `runtime/queries` has **341**
directories, MPL-2.0 [V]. Apache-2.0 is incompatible with GPLv2-only [M — FSF list]; MPL-2.0 is
compatible via its secondary-licence clause [M] — so Helix is the cleaner query source.

**What we would have to write.** The C library exposes queries (`ts_query_new`,
`ts_query_cursor_exec`, `ts_query_cursor_next_capture`, `ts_query_predicates_for_pattern`) but
**does not evaluate predicates** (`#match?`, `#eq?`, `#any-of?`) — they are returned as step
arrays for the caller [V — api.h comment; standard binding behaviour]. Injections (embedded
languages: HTML↔JS↔CSS, Markdown fences), locals, capture-name precedence and the
capture-name→colour theme mapping are all ours. The official highlighter with a **C header**
(`ts_highlighter_new/add_language/highlight`, `ts_highlight_buffer_*`) exists in
`crates/highlight/include/tree_sitter/highlight.h` — but it is implemented in
`crates/highlight/src/c_lib.rs`, i.e. **Rust** [V — header fetched, `c_lib.rs` listed]. Using it
means adding cargo to the build or vendoring a prebuilt Rust static lib — neither fits "MSBuild +
VS2022 only" (constitution IV).

**Realistic vendoring.** 20–30 grammars ≈ 100 MB+ of generated C source, minutes of compile,
tens of MB of parser tables in the binary [M]; "hundreds" is out of the question. Highlight quality
is the best of all options (real parse trees), but priority 1 is breadth.

**Verdict.** Not for this feature. Keep in mind for a later "structural" upgrade (e.g. folding by
syntax) restricted to 5–10 languages.

### 3.3 TextMate-grammar engines usable from C++

**Search result.** No maintained C/C++ implementation of TextMate tokenisation was found
[V — two targeted searches]; known ports are TypeScript (`vscode-textmate`, MIT, active [V]),
Rust (`syntect`), Java (`tm4e`), Kotlin, C# (`TextMateSharp`), Go. A C++ engine would be a port of
`vscode-textmate` (grammar loading from JSON/plist, rule compilation, begin/end/while patterns,
back-references, capture scopes, injections, embedded grammars, scope-stack theme matching) —
roughly 6–10 k LOC [M].

**Regex engine.** TextMate grammars use the Oniguruma dialect; Oniguruma is BSD-2-Clause
("Copyright (c) 2002-2021 K.Kosako") [V — COPYING] — **but the repository was archived by the
owner on 2025-04-24 and is read-only**; last release 6.9.10 (2025-01-01) [V — repo page + API].
Note this also affects the web route: `vscode-oniguruma` is a WASM build of the same library;
Shiki offers a pure-JS regex engine as an alternative with some grammar incompatibilities [M].

**Grammars & themes.** Shiki's `tm-grammars` package: **260** grammar files; `tm-themes`: **65**
themes [V — API]; each carries its upstream licence (mostly MIT; check individually) [M].

**Verdict.** Feasible but the largest effort of all routes for a result (VS-Code-identical
highlighting) that the JS route obtains for free. Only sensible if WebView2 were rejected outright.

### 3.4 Other native engines

- **Colorer (colorer-take5 / Colorer-library)** — the one that is genuinely relevant; see § 3.5.
- **KSyntaxHighlighting (KDE)** — Qt framework; **411** syntax XML definitions and **30** themes
  [V — API]. Rule out as an engine (Qt is forbidden by principle IV); its Kate-XML *data* would need
  a from-scratch engine with QRegularExpression (PCRE2) semantics — too large. Themes are a palette
  source only.
- **GtkSourceView** — GLib/GTK, LGPL-2.1 [M]; **180** `.lang` files [V]. Rule out.
- **Vim / Emacs syntax files** — Vim ships **785** `runtime/syntax/*.vim` [V] under the Vim
  licence; they need Vim's regex and `:syntax` engine. Emacs modes are GPLv3 Lisp. Rule out.
- **Windows-native** — nothing: RichEdit, DirectWrite, Windows Notepad and Terminal expose no
  syntax-highlighting API [M].
- **Lite-XL / Textadept** — Lite-XL: **104** `language_*.lua` plugins (MIT, Lua patterns) [V],
  simplistic tokenizers; Textadept uses Scintillua (§ 3.1). Note only.
- **André Simon `highlight`** — C++ with Lua language definitions and HTML/RTF/… output; the
  GitHub mirror (archived 2018) has 221 `.lang` and 88 themes, **GPL-3.0** [V — API]; current
  home is GitLab (`saalen/highlight`), version not verified today (site certificate expired). GPLv3
  → same conflict as Notepad++ themes. Rule out unless licensing policy changes. **GNU
  source-highlight** — GPLv3, Boost.Regex, dormant since ~2018 [M]. Rule out.

### 3.5 Colorer (colorer-take5) — HRC/HRD highlighter used by FAR Manager's Colorer plugin

**Facts.** `colorer/Colorer-library`: **MIT**, v1.6.0 released 2026-08-21, pushed 2026-08-21
[V]; `colorer/Colorer-schemes`: **LGPL-2.1**, pushed 2026-08-26 [V]; `colorer/FarColorer`: MIT,
v1.7.1 released 2026-08-26 [V]. Very actively maintained (Windows-first project). Windows build:
CMake + vcpkg, "Visual Studio 2019 / gcc 7 / clang 7 or higher", triplets
`x64-win-static`/`x86-win-static`/`arm64-win-static` [V — README]. Dependencies: **libxml2**
(replaced Xerces-C in v1.4.0), **ICU** for strings with an optional legacy implementation
(`COLORER_USE_ICU_STRINGS`), zlib + minizip optional for zipped schemes
(`COLORER_USE_ZIPINPUTSOURCE`) [V — README/CHANGELOG]. v1.6.0: "Parser and regexp performance,
incremental single-line reparse, and CRegExp correctness", regex **backtracking limit default
1,000,000 steps**, chunked line colouring [V — CHANGELOG].

**Coverage / themes.** **367 `.hrc` files** in the schemes repo (includes shared/helper modules;
distinct languages fewer — project claims ~300 [M]) and **39 `.hrd` files**: 20 `console/`
(16-colour), **17 `rgb/`** (GUI: black, blue, eclipse, far2l, grayscale, hs, mirice, mirror,
navy, neo, violet, white, FalloutRed + 4 contrib), 2 `text/` (`htmlcss.hrd`, `tags.hrd` — used for
HTML output) [V — recursive tree]. Roughly half the RGB schemes are dark. HRD is a small XML
(region → fore/back/style) — trivially converted to CSS variables.

**Integration.** Text is handled as UTF-16 (`UnicodeString`) [M]; schemes are XML loaded at
runtime (needs a `hrc/` catalog next to the plugin, MBs) — or the zipped form. No MSBuild
project: a hand-written `.vcxproj` for `src/colorer` plus libxml2 (MIT, sizeable) and either ICU
(system `icu.dll` exists on Windows 10 1903+ [M] but the CMake build expects vcpkg ICU) or the
legacy string mode. Effort high; the parser is designed for editors (line source + parse cache) so
windowed/partial highlighting is natural [M]. Regex-based → the backtracking limit is the DoS
guard.

**Verdict.** The only native engine whose breadth approaches web libraries, permissively
licensed and alive — but heavy to bring in. Candidate to *boost* route #2 later, not a first choice.

### 3.6 Hybrid: tokenize in C++, render static HTML in the WebView (scripts OFF)

**Shape.** C++ (Lexilla via a small in-memory `IDocument` — precedent: Lexilla's own test
harness implements one [M]) produces per-token `<span class="…">` runs; the plugin serves a
self-contained HTML document from the private virtual host exactly as mdview does
(`https://mdview.invalid/doc.html`, `WebResourceRequested` default-deny, `NavigationStarting`
cancel, `put_IsScriptEnabled(FALSE)`, no host objects/web messages) [R
`src/plugins/mdview/webview.cpp:29,285-292,314,360`]. Themes are CSS variables — mdview already
emits `hl-kw/hl-str/hl-num/hl-cmt/hl-type/hl-fn/hl-op` classes and `.hl-kw{color:var(--kw)}`
rules from a 10-entry theme table [R `htmlgen.cpp:89-95,588-590`; `render.cpp:21,85,95`]. The
host/keeper/options helper is lifted to `src/common/` per contract § 2.5 [R].

**Pros.** Keeps mdview's security posture by construction (no script engine, no wasm); linear
lexers → no ReDoS; identical host code and keeper; themes trivially share the mdview scheme list
(paper/graphite defaults [R]); C++ side controls CPU (cap highlighted bytes, background thread).

**Cons.** Coverage ceiling 125 (+160) vs 194/260; the per-lexer style→class table must be
authored (SciTE properties as template — § 3.1); no VS-Code-fidelity; **large files**: with
scripts off there is no virtualisation — a 10 MB file becomes millions of spans (DOM in the
hundreds of MB, layout in seconds [M]). Mitigations: highlight only the first N MB and emit the
rest as plain `<pre>` with a banner; or page by re-navigation (`doc.html?from=<line>`, the
`?v=`/`#frag` trick mdview's find uses [R IMPLEMENTATION_NOTES v2.1 §5]); keep mdview's 20 MB
size gate → internal text viewer. JS virtualisation (scripts on) scales better.

**Effort.** Medium: Lexilla vendoring (~130 files, one static lib), `IDocument` shim, table for
125 lexers, HTML emitter (mostly `htmlgen.cpp` reuse), extension→lexer map. Less C++ than the
native control, more than the JS route.

### 3.7 Extending the built-in text viewer

**How it renders.** `CViewerWindow` streams the file through a **60,000-byte sliding buffer**
(`VIEW_BUFFER_SIZE`, `Buffer/Seek/Loaded`, `Prepare/LoadBefore/LoadBehind`) [R `src/viewer.h:7`,
members ~`:329-335`; `src/viewer3.cpp:126-130`], locates lines by scanning for EOLs, keeps a
per-visible-line `LineOffset` triple [R `viewer.h:~348`], and paints each line into a memory bitmap
with fixed-cell monospace `TextOut`/`ExtTextOutW` segments — already split into up to three
segments for the selection with `SetTextColor` switches [R `src/viewer.cpp:744-800,1417-1435`].
Colour runs are therefore *drawable* with modest changes (more segments per line). There are **no
colouring hooks** and only **4 configurable colours** (`VIEWER_FG/BK_NORMAL/SELECTED`,
`NUMBER_OF_VIEWERCOLORS 4`, light + dark sets) [R `src/consts.h:1291-1297,1328,1332`;
`viewer.cpp:1528-1534`].

**What it would take.** File-type detection; a lexer whose state is *resumable at an arbitrary
file offset* — the 60 KB window means a `/* … */` or heredoc opened before the window must be
recovered by lexing from the file start (cost O(offset), acceptable up to tens of MB) or by
heuristics; per-line style runs; N new colour slots with light/dark defaults in the config dialog;
wrap/hex/find interplay. Rendering stays GDI; DirectWrite is not involved. Everything lives in the
core (`viewer*.cpp`, 7,425 lines, plus `cfgdlg`), affecting all users and every language file.

**Verdict.** Technically feasible and the only route with true streaming, but invasive, with no
theme infrastructure and a lexer we would have to write or port ourselves. Not the primary route;
a candidate for a later "lite highlighting" of the fallback viewer.

---

## 4. Risks & open questions

**Risks of the recommended route (JS in WebView2)**
- Scripts on: the plugin must re-derive its own lockdown (scripts on, everything else identical to
  mdview: default-deny network, no host objects, no web messages, navigation cancelled,
  `ProcessFailed` → text viewer). The plan must state this as a deliberate per-controller decision
  (contract § 2.3) and keep the bundle local (no CDN).
- ReDoS / long-running highlight on adversarial input: size gate (mdview: 20 MB), line-length cap,
  run the highlighter in a Web Worker with a timeout, "plain text" fallback banner.
- WASM (Shiki/Oniguruma) inside the locked-down host needs a CSP allowing `wasm-unsafe-eval` [M];
  Oniguruma upstream is archived [V] — prefer highlight.js (no wasm) or Shiki's JS regex engine.
- Bundle size/licences: audit every language module and theme file (highlight.js BSD-3 [M];
  Shiki grammars/themes per-file).
- Extension registration: `AddViewer(masks, force)` takes a `;`-separated wildcard list, `|`
  forbidden [R `src/plugins/shared/spl_base.h:281-289`]; "hundreds" of masks means a long string
  and interplay with the built-in viewer's defaults — verify limits.

**Risks if a native/hybrid route is chosen instead**
- Theme authoring cost per lexer (mitigated by SciTE properties, but still ~125 rows to verify).
- Notepad++ theme XML (GPLv3) must not be copied; palettes re-derived from MIT origins.
- Scintilla: second rendering technology; dark scrollbars/DPI polish; constitution VI judgement.
- Colorer: CMake→MSBuild port and two new deps; UTF-16 bridge; runtime scheme catalog.

**Open questions (need an owner decision or a measurement)**
1. Is enabling scripts on this plugin's WebView2 controllers acceptable? (If not → route #2.)
2. highlight.js (194 langs, regex, no wasm, 90 CSS themes) vs Shiki (260 grammars, VS Code
   themes, wasm or JS regex engine, 10 MB+ bundle) — the web-route research should size both.
3. Large-file target: what size must still open highlighted? (Static HTML: cap ~1–2 MB
   highlighted; JS virtualised: tens of MB; Scintilla: hundreds of MB.)
4. Unverified [M] items that matter: Scintilla per-byte style memory and DPI handling;
   Scintillua's fixed tag set; Colorer's line-source/parse-cache API; DOM cost figures for
   multi-million-span pages; Shiki JS-engine grammar incompatibilities.

---

## 5. Sources

Primary (fetched 2026-08-26):
- Scintilla licence — https://www.scintilla.org/License.txt
- Scintilla documentation (read-only, wrap, zoom, find, idle styling) — https://www.scintilla.org/ScintillaDoc.html
- Scintilla history (5.6.6 / 5.6.5 / 5.6.4; DirectWrite, scaling, wrap fixes) — https://scintilla.org/ScintillaHistory.html
- Lexilla page (5.5.3) — https://scintilla.org/Lexilla.html · history — https://scintilla.org/LexillaHistory.html · docs (`CreateLexer`, `ILexer5`, `LEXILLA_NO_EXPORT`) — https://scintilla.org/LexillaDoc.html
- Lexilla lexers directory (125 `Lex*.cxx`) — https://github.com/ScintillaOrg/lexilla/tree/master/lexers
- SciTE `cpp.properties` (style→role variables) — https://sourceforge.net/p/scintilla/scite/ci/default/tree/src/cpp.properties
- Scintilla static build / `Scintilla_RegisterClasses` / nmake — https://scintilla-interest.narkive.com/XUpyrWbT/compiling-and-using-scintilla-as-static-library-in-my-app , https://github.com/alexprabhat99/notepad-plus-plus/blob/master/BUILD.md
- `SC_TECHNOLOGY_DIRECTWRITE`, `SC_DOCUMENTOPTION_TEXT_LARGE`, `SCI_SETIDLESTYLING` — https://fossies.org/linux/scintilla/include/Scintilla.h , https://fossies.org/linux/scintilla/doc/ScintillaHistory.html
- Scintillua (160 lexers, MIT) — https://github.com/orbitalquark/scintillua , https://orbitalquark.github.io/scintillua/
- Colorer-library (MIT, v1.6.0, deps, build) — https://github.com/colorer/Colorer-library , README, CHANGELOG.md · schemes (LGPL-2.1, 367 hrc / 39 hrd) — https://github.com/colorer/Colorer-schemes · FarColorer v1.7.1 — https://github.com/colorer/FarColorer
- Tree-sitter releases (0.26.13) — https://github.com/tree-sitter/tree-sitter/releases · `api.h` — https://raw.githubusercontent.com/tree-sitter/tree-sitter/master/lib/include/tree_sitter/api.h · `highlight.h` (C header, Rust `c_lib.rs`) — https://raw.githubusercontent.com/tree-sitter/tree-sitter/master/crates/highlight/include/tree_sitter/highlight.h · syntax-highlighting chapter — https://tree-sitter.github.io/tree-sitter/3-syntax-highlighting.html · grammar `parser.c` sizes via GitHub contents API (tree-sitter-cpp, -typescript, -php, -c, -javascript, tree-sitter-grammars/tree-sitter-yaml)
- nvim-treesitter (Apache-2.0, not archived, 323 query dirs) — https://github.com/nvim-treesitter/nvim-treesitter · Helix (MPL-2.0, 341 query dirs) — https://github.com/helix-editor/helix
- Oniguruma (BSD-2, archived 2025-04-24, 6.9.10) — https://github.com/kkos/oniguruma , https://raw.githubusercontent.com/kkos/oniguruma/master/COPYING
- vscode-textmate (MIT, active) — https://github.com/microsoft/vscode-textmate · C++ engine search: https://github.com/topics/tokenizer?l=c%2B%2B (no hit)
- Notepad++ licence GPLv3 — https://github.com/notepad-plus-plus/notepad-plus-plus/blob/master/LICENSE · themes (22) — https://github.com/notepad-plus-plus/notepad-plus-plus/tree/master/PowerEditor/installer/themes · nppThemes (GPLv3) — https://github.com/notepad-plus-plus/nppThemes · user manual — https://npp-user-manual.org/docs/themes/
- KSyntaxHighlighting (411 syntax, 30 themes) — https://github.com/KDE/syntax-highlighting · GtkSourceView (180 lang) — https://github.com/GNOME/gtksourceview · Vim syntax (785) — https://github.com/vim/vim/tree/master/runtime/syntax · Lite-XL plugins (104) — https://github.com/lite-xl/lite-xl-plugins
- highlight.js (194 languages, 90 styles, 11.12.0) — https://github.com/highlightjs/highlight.js · Shiki tm-grammars (260) / tm-themes (65) — https://github.com/shikijs/textmate-grammars-themes
- André Simon highlight (GPL-3.0, mirror archived) — https://github.com/andre-simon/highlight , https://gitlab.com/saalen/highlight

Repository:
- `src/viewer.h`, `src/viewer.cpp`, `src/viewer3.cpp`, `src/consts.h` (built-in viewer model)
- `src/plugins/mdview/webview.cpp`, `htmlgen.cpp`, `render.cpp`, `highlight.cpp`, `mdview.cpp`, `IMPLEMENTATION_NOTES.md`, `vcxproj/mdview.props`
- `src/plugins/shared/spl_base.h` (`AddViewer`), `src/common/dep/` (vendored set)
- `architecture/11-webview2-integration.md`, `.specify/memory/constitution.md` (IV, VI)

# Feature 070 — Research: file-type → language detection and the claimed-extension policy

**Date**: 2026-08-26 · **Scope**: which file names the new source/config viewer plugin should claim in Options ▸ Viewers, how it should pick a grammar once opened, and what it should do with unknown, ambiguous or binary content.
**Verification legend**: **[V]** verified today (fetched and, where a number is quoted, counted locally from the downloaded upstream file — Linguist `languages.yml`/`heuristics.yml`, all 97 VS Code `extensions/*/package.json`, highlight.js `SUPPORTED_LANGUAGES.md`, Shiki `tm-grammars/README.md`, Prism `components.json`, or read in this repository at the cited `file:line`); **[M]** from memory, not re-verified.

---

## 0. Summary

- **Mapping source**: generate the plugin's map from **GitHub Linguist `languages.yml` + `heuristics.yml`** (MIT, 833 languages, 1,486 extensions, 419 exact filenames, 138 content-disambiguation rule blocks) with a **VS Code built-in overlay** (14 `firstLine` regexes, 26 filename patterns, JSONC/`.log` entries) and a **small hand-maintained Windows/dev-tooling overlay** (`.rc/.rh/.rc2`, `.iss`, `.inf`, `.idl`, `.sln`, `.slt`…) that neither upstream covers. Commit the generated table; a build-time/unit check keeps it consistent with the highlighter's actual grammar set, and a mapped language without a grammar **opens as plain text** — never fails.
- **Detection order**: exact filename → longest multi-dot suffix → extension → *only if none or ambiguous*: shebang → Emacs/Vim modeline → Linguist heuristics for that extension → first-bytes signature → plain text. Path evidence beats content evidence (VS Code's rule); content is consulted to disambiguate, not to override.
- **Claim policy**: claim ≈ 780 masks (≈ 225 grammar families) in ≤ 8 Viewers entries grouped by family, **never** `.md/.markdown` (mdview), `.csv` (dbviewer), `.st/.pyx/.dtx` (pictview), `.txt`, `.log`, `.tsv`, and never anything another shipped plugin registers. Prepending at the top of the list is the product's existing installation mechanism (`ViewerMasks->Insert(0, …)`), applied identically to new and upgraded installs; user entries keep their relative order and Alt+F3 always reaches the built-in viewer.
- **Binary**: `CanViewFile` sniffs ≤ 8 KB (BOM → text; UTF-16 pattern → text; any NUL otherwise → binary; > 0.5 % WHATWG "binary data bytes" → binary) and **returns FALSE so the core cascades to the next viewer** (the built-in hex viewer) — the same contract mdview and peviewer already use.

---

## 1. Data sources for extension/filename → language mapping

### 1.1 GitHub Linguist `languages.yml` + `heuristics.yml` — **recommended primary source**

| Fact | Value | |
|---|---|---|
| Licence | MIT, © 2017 GitHub, Inc. | [V] |
| Languages | **833** — `programming` 560, `data` 184, `markup` 71, `prose` 18 | [V] counted from `main` |
| Distinct extensions | **1,486**; **173** are listed under more than one language | [V] |
| Extensions by type | programming 966, data 402, markup 144, prose 42 | [V] |
| Exact filenames | **419** (`Dockerfile`, `Makefile`, `CMakeLists.txt`, `.gitignore`, `Gemfile`, …) | [V] |
| Languages with `interpreters` (shebang names) | 88 | [V] |
| Languages with `tm_scope: none` (no TextMate grammar anywhere) | 63 | [V] |
| Heuristics | 138 `disambiguations` blocks (extension list + ordered `rules`, each `language` + `pattern`/`negative_pattern`/`named_pattern`/`and`) | [V] |

Fields (quoted from the file header [V]): `type` — "Either 'data', 'programming', 'markup' or 'prose'"; `extensions` — "List of associated file extensions, sorted in ascending ASCII order, except for the language's primary extension, which is always listed first"; `filenames` — "List of associated filenames"; `interpreters` — "List of programs that execute the language"; `aliases` — "implicitly includes lowercased forms of language's name … used in fenced code-blocks"; `tm_scope` — "The TextMate scope that represents this programming language … Use 'none' if there is no TextMate grammar"; `ace_mode`, `codemirror_mode`, `group`, `color`, `language_id`, `wrap`.

Why it is the right generator input: it is the only source that carries all four things the plugin needs in one place — extension **and** filename **and** interpreter tables, a `type` to separate prose/data from code, `tm_scope` (a direct bridge to TextMate-based highlighters such as Shiki), and an ordered, tested content-disambiguation rule set for the ambiguous extensions. It is also what `hyperpolyglot` (Rust) and `go-enry` (Go) embed [V], so "generate from Linguist" is an established pattern. Its bias is GitHub's (repository content, Unix-centric): `.rc`, `.rc2`, `.rh`, `.inf`, `.def`, `.idl`, `.log`, `.conf`, `.config`, `.resx` (only via XML), `.slt`, `.manifest`, `.vcproj` are **not** in it [V] — hence the overlay.

### 1.2 VS Code built-in language contributions — **recommended overlay**

`extensions/*/package.json` → `contributes.languages` [V, all 97 directories fetched]: 83 contributions, 74 distinct ids, **342** extensions, 72 `filenames`, 26 `filenamePatterns` (globs such as `tsconfig.*.json`, `Dockerfile.*`, `*docker*compose*.yml`, `**/.git/config`), **14** `firstLine` regexes. No extension is claimed by two ids [V]. Documented fields [V]: `id`, `extensions`, `filenames`, `filenamePatterns`, `firstLine` ("Matches files beginning with specific content, such as shebangs"), `aliases` (first = human label), `mimetypes`, `configuration`, `icon`. Repository licence MIT [M]; individual grammars inside `syntaxes/` carry their upstream licences (cgmanifest) [M].

What it adds over Linguist: pragmatic editor decisions (`.svg`→xml, `.log`→`log` grammar, `.cfg/.conf`→properties, `.h`→cpp, `.ipynb`→json, `.vcxproj/.props/.nuspec/.wxs`→xml, JSONC for `tsconfig*.json`/`.code-workspace`, `.env.*` patterns) and the 14 first-line regexes (§2.4). What it lacks for this product [V]: `.sln`, `.reg`, `.rc`, `.iss`, `.inf`, `.idl`, `.def`, `.manifest`, `.resx`, `.toml`, `.csv`, `.txt`, `.config` have **no built-in id** (they are Marketplace extensions).

### 1.3 Highlighter alias tables

| Library | Size | Alias mechanism | Notes |
|---|---|---|---|
| **highlight.js** | "over 180 languages in the core library" [V README]; `SUPPORTED_LANGUAGES.md` has 308 rows, ≈ 199 core + third-party packages [V] | `registerAliases`, `getLanguage(name)` looks up "by name or alias" [V] | Aliases are language *names* (`js`, `c++`), not file extensions; no filename table. Licence BSD-3-Clause [M]. |
| **Shiki / `tm-grammars`** | **260** grammar ids in the README table (includes sub-grammars: `angular-*`, `vue-*`, `es-tag-*`, `markdown-vue`…; ≈ 230 user-facing languages), 104 aliases [V] | id + aliases; no extension/filename table | Per-grammar licences [V tally]: MIT 188, Apache-2.0 22, **GPL-3.0 5** (e.g. Ada from AdaCore), MPL-2.0 3, BSD-3 1, ISC 1, a few unlabeled. GPL-3.0 grammars combine with this GPLv2-**or-later** product only under the "or later" option — flag for review or exclude those five. |
| **Prism** | **297** languages in `components.json` [V] | `alias` per component (e.g. `markup` → html, xml, svg, mathml, ssml, atom, rss) [V] | No extension table. MIT [M]. |

None of the three is a usable *file-name* map; they are the **target** side of the mapping (grammar id), not the source. The generated table must therefore carry `linguist language → grammar id` explicitly (a bridge table; ≈ 225 Linguist languages bridge to a Shiki grammar by name/alias [V, my script], 63 Linguist languages have no grammar anywhere [V]).

### 1.4 Ports and ML detectors

- **hyperpolyglot** (Rust) — "fast programming language detector … based on GitHub's Linguist" [V]; **go-enry** (Go) — Linguist port, "on average enry is 211 % faster than linguist" [V]. Neither is C++; both prove the Linguist data can be embedded offline.
- **Google Magika** — Apache-2.0 [V]; 1.0 (Nov 2025) rebuilt in Rust, ONNX Runtime inference, "200+ content types (covering both binary and textual file formats)", ~99 % claimed accuracy, few-MB model, "only uses a limited subset of the file's content"; clients: Python, Rust CLI/crate, TypeScript/npm, Go WIP — **no C/C++ binding** [V]. **Verdict: not worth it.** It is a *content-type* classifier (C vs C++, JSON vs JSONL, CSV vs TSV, ~40 of its types are code), not a per-grammar detector; using it from a WinAPI plugin means either shipping ONNX Runtime (tens of MB) or hand-writing a C ABI over the Rust crate; and the plugin already has the strongest signal — the file name. Revisit only for the "no extension, no signature" residue, where its 200-type granularity still under-fits a 230-grammar highlighter.
- **`@vscode/vscode-languagedetection`** — MIT, guesslang model, **30 languages** [V]. VS Code runs it only when no path-based guess exists (`quickGuess` first) [V], samples 10,000 characters, accepts a result only above `expectedRelativeConfidence = 0.2`, adds +0.05 for JS/HTML/JSON/TS/CSS/Python/XML/PHP, +0.025 for C++/Shell/Java/C#/C, and **−0.5** for Batch/INI/Makefile/SQL/CSV/TOML because "we've had issues like #131912 that caused incorrect guesses" [V]. Since the plugin renders in WebView2, this JS model *could* run inside the page for name-less files later; it is not a v1 need.

### 1.5 Recommendation — the generator and the sync rule

1. `tools/langmap/gen_langmap.py` (developer-side, like `tools/brand/`): input = pinned Linguist commit (`languages.yml`, `heuristics.yml`), the VS Code overlay (checked-in JSON), the hand overlay (checked-in JSON), and the **grammar manifest of the chosen highlighter** (Shiki `bundledLanguagesInfo` or the vendored grammar directory). Output = one committed generated file (e.g. `src/plugins/<name>/langmap.generated.h` or JSON embedded as a resource) with four tables: `filename → lang`, `suffix → lang[]` (longest-first, lower-cased), `interpreter → lang`, `lang → {grammar id | none, type, display name, heuristics[]}`.
2. **Sync rule**: every `grammar id` referenced by the table must exist in the shipped grammar set (build fails otherwise), and every shipped grammar must be reachable from at least one table entry (warning). A language whose grammar is `none` (Linguist `tm_scope: none`, or a grammar we chose not to ship, e.g. the GPL-3.0 ones) is still in the table and renders as **plain text with the language name shown** — the user sees "Inno Setup (no highlighting)" rather than a failure or a wrong grammar.
3. Re-generation is a one-line command; the diff of the generated file is reviewed like any code change (this is how `translations/` and `tools/brand` already work in this repo).

---

## 2. Detection heuristics, in order

### 2.1 Reference orders

- **Linguist** [V]: "1 Vim or Emacs modeline · 2 commonly used filename · 3 shell shebang · 4 file extension · 5 XML header · 6 man-page section · 7 heuristics · 8 naïve Bayesian classification" — "each step either identifying the precise language or reducing the number of likely languages passed down to the next strategy". Note that a modeline *overrides* the extension.
- **VS Code** [V, `languagesAssociations.ts`]: "User configured mappings have highest priority", "Registered mappings have middle priority", "Firstline has lowest priority"; within a tier: exact filename → longest glob → **longest extension**; "the last registered association wins"; `firstLine` is consulted **only when nothing matches by path**. The ML detector runs only when even that fails.

### 2.2 Recommended pipeline for the plugin

| # | Step | Input | Examples | Cost |
|---|---|---|---|---|
| 1 | **Exact filename** (case-insensitive — NTFS) | table of ≈ 420 names (Linguist 419 ∪ VS Code 72) | `Dockerfile`, `Containerfile`, `Makefile`, `GNUmakefile`, `CMakeLists.txt`, `Jenkinsfile`, `Gemfile`, `Rakefile`, `Vagrantfile`, `Podfile`, `Brewfile`, `.gitignore`, `.gitattributes`, `.gitmodules`, `.gitconfig`, `.editorconfig`, `.env`, `.bashrc`, `.bash_profile`, `.zshrc`, `.profile`, `.npmrc`, `.clang-format`, `.clang-tidy`, `go.mod`, `go.sum`, `Cargo.lock`, `Pipfile`, `package.json`, `tsconfig.json` (→ JSONC), `COMMIT_EDITMSG`, `git-rebase-todo` | O(1) hash |
| 2 | **Filename patterns** (few) | ≈ 30 globs | `Dockerfile.*`, `Jenkinsfile*`, `.env.*`, `tsconfig.*.json`, `jsconfig-*.json`, `*docker*compose*.yml`, `compose.*.yaml` | small |
| 3 | **Longest multi-dot suffix first** | table sorted by suffix length | `.d.ts`→TS decl. (grammar `typescript`), `.blade.php`→Blade, `.test.js`/`.spec.ts`→JS/TS (no separate grammar; keep for future), `.h.in`/`.cmake.in`→C/CMake, `.rs.in`, `.toml.example`, `.yml.mysql`, `.dll.config`→XML, `.tar.gz`→n/a (never claimed) | O(k) |
| 4 | **Extension** | ≈ 780 claimed suffixes | `.cpp`, `.yml`, … | O(1) |
| 5 | **Disambiguation — only if step 4 returned > 1 candidate or nothing** | first 8 KB (already read for the binary sniff) | see 2.3–2.5 | ≤ a few regexes |
| 6 | **Fallback** | — | plain text, language "Plain Text"; never an error | — |

Rationale for "path beats content": it is deterministic (the same file always opens the same way), cheap, matches the editor most users compare against, and cannot be spoofed by a stray `#!` in a data file. Content evidence is used to *choose among* the candidates the name allows, plus one deliberate exception: a file with **no** extension and no filename match is classified by shebang/modeline/signature (Linguist steps 1, 3, 5).

### 2.3 Shebang [V, Linguist `shebang.rb`]

First line must start with `#!`; take the last path component; if it is `env`, skip `-flags` and `VAR=value` words and take the next token; strip a trailing version (`python2.6` → `python2` → table maps `python2`/`python3` → Python); handle the `sh` re-exec trick (`exec perl "$0" "$@"` in the first 5 lines); `osascript -l <lang>` returns nothing. VS Code's equivalent regexes [V]: `^#!.*\b(bash|fish|zsh|sh|ksh|dtksh|pdksh|mksh|ash|dash|yash|sh|csh|jcsh|tcsh|itcsh).*`, `^#!\s*/?.*\bpython[0-9.-]*\b`, `^#!.*\bnode`, `^#!.*\b(deno|bun|ts-node)\b`, `^#!.*\bperl\b`, `^#!\s*/.*\bphp\b`, `^#!\s*/.*\bruby\b`, `^#!\s*/.*\bpwsh\b`, `^#!\s*/usr/bin/make`, `^#!.*\bgroovy\b`, `^#!\s*/.*\bjulia[0-9.-]*\b`. Use the Linguist `interpreters` table (88 languages) rather than per-language regexes.

### 2.4 Modelines [V, Linguist `modeline.rb`]

Search the **first 5 and last 5 lines** (`SEARCH_SCOPE = 5`). Emacs: `-*- ruby -*-` or `-*- mode: ruby; … -*-` (regex `EMACS_MODELINE`). Vim: `vim:`/`vi:`/`ex:` with `set`/`se` forms and `ft=`/`filetype=`/`syntax=` (regex `VIM_MODELINE`, "syntax documented in E520"). The captured name is resolved through Linguist **aliases** (`Language.find_by_alias`). Files containing `UseVimball` are skipped. VS Code also honours `^#\s*-\*-[^*]*mode:\s*shell-script[^*]*-\*-` for shell [V].

### 2.5 First-bytes signatures (cheap, high precision)

| Signature (after BOM, leading whitespace) | Language | Source |
|---|---|---|
| `<?xml` | XML (then `<svg` → SVG, `<plist` → plist, `<TS` → Qt `.ts`, `<Project` → MSBuild) | VS Code `xml` firstLine `(\<\?xml.*)\|(\<svg)\|(\<\!doctype\s+svg)` [V]; Linguist step 5 |
| `<!DOCTYPE html`, `<html` | HTML | [M] common |
| `{` or `[` as first non-space byte | JSON (Linguist `named_pattern json := \A\s*[{\[]` [V]) — only as a *disambiguator* (`.yy`), never to override YAML/JS | |
| `---` first line | YAML (Linguist uses `negative_pattern: '---'` to *exclude* MiniYAML [V]) | |
| `[section]` first non-comment line | INI family (`.cfg/.conf/.inf/.url/.reg`) | [M] |
| `Windows Registry Editor Version 5.00` / `REGEDIT4` | Windows Registry Entries; the former is **UTF-16LE with BOM** in every modern export, the latter ANSI [V] | |
| `#cloud-config` | YAML (VS Code) [V] | |
| `diff --git`, `Index:`, `--- a/` + `+++ b/` | Diff | [M] |
| `%PDF`, `PK\x03\x04`, `MZ`, `\x7fELF`, `\x89PNG`, `GIF8`, `\xff\xd8\xff`, `RIFF`, `OggS`, `\x1f\x8b` | **binary signatures** — hand back (§5) | [M] |
| `G` (0x47) every 188 bytes | MPEG-TS (the `.ts` video case) — binary | [M] |

### 2.6 Content statistics — what they can and cannot do

- `hljs.highlightAuto(code, subset?)` returns `{language, relevance, secondBest}` [V]. Relevance is a keyword-hit score, so it is unreliable on short snippets, on data-heavy files (JSON/YAML/INI/CSV), and when the candidate set is large; the README nudges users to pass the language or restrict `languageSubset` [V]. Suitable only as a **tie-breaker among ≤ 3 name-derived candidates** (e.g. `.h`: pass `['c','cpp','objectivec']`) — never as a free-for-all over 230 grammars.
- guesslang/VS Code: 30 languages, bias-corrected thresholds (§1.4); VS Code itself distrusts it for Batch/INI/Makefile/SQL/CSV/TOML.
- The built-in viewer's own `RecognizeFileType` [V `src/codetbl.cpp:895`] is a *code-page* scorer (penalties for odd letter-case transitions, repeated punctuation, `?` runs) that happens to also emit `isText`; it is not a language detector.

---

## 3. Ambiguous extensions — table with recommended defaults

Columns: Linguist candidates and its rule order [V `heuristics.yml`], VS Code built-in choice [V], recommended default and the disambiguation the plugin performs, and whether the plugin claims the mask.

| Ext | Linguist candidates → rule order | VS Code | **Recommended default** | Disambiguation (8 KB sample) | Claim |
|---|---|---|---|---|---|
| `.h` | Objective-C (`^\s*(@interface\|@class\|@protocol\|@property\|@end\|@implementation…\|#import …\.h)`) → C++ (`#include <cstdint\|string\|vector\|iostream…>`, `template\s*<`, `class\|namespace\s+\w+`, `public:`, `std::\w+`) → **C** default | `cpp` | **C++** grammar (a superset of C for highlighting purposes; VS Code does the same) | Obj-C pattern → `objective-c` | yes |
| `.m` | Objective-C → Mercury (`:- module`) → MUF → M (`^\s*;`) → Wolfram (`(*`…`*)`) → MATLAB (`^\s*%`) → Limbo | `objective-c` | **Objective-C** (Windows dev audience rarely has MATLAB) | `^\s*%` or `function ` → `matlab`; `(*` → `wolfram` | yes |
| `.pl` | Prolog (`^[^#]*:-`) → Perl (`use strict`/`use v5`, not `use v6`) → Raku | `perl` | **Perl** | `:-` outside comments → `prolog` | yes |
| `.ts` | XML (`<TS\b`, Qt Linguist) → **TypeScript** default | `typescript` | **TypeScript** | `<TS` / `<?xml` → `xml`; **MPEG-TS sync byte 0x47 at 0/188/376 or any NUL → binary → hand back** | yes |
| `.rs` | Rust (`^(use \|fn \|mod \|pub \|macro_rules\|impl\|#!?\[)`) → RenderScript → XML (`^\s*<\?xml`) | `rust` | **Rust** | `<?xml` → `xml` | yes |
| `.v` | Rocq/Coq (`Proof.`/`Qed.`/`Require Import`) → Verilog (`module … (`, `` `define``, `always @`, `initial`) → V (`fn name(...) {`, `$if`) | — | **Verilog** (most `.v` files in the wild) | Coq → `coq`; V → `v` | yes |
| `.sql` | PLpgSQL (`\i`, `DO $`, `LANGUAGE plpgsql`) → SQLPL (DB2) → PLSQL (`$$PLSQL_`, `XMLTYPE`, `CONNECT BY`, `AUTHID`) → TSQL (`^\s*GO\b`, `BEGIN TRY`, `DECLARE @`, `[dbo]`) → **SQL** default | `sql` | **SQL** (generic) | T-SQL/PL-SQL patterns select dialect grammar if shipped; otherwise generic | yes |
| `.inc` | m68k asm → PHP (`^<\?(php)?`) → SourcePawn → NASL → POV-Ray (`#declare`) → Pascal (`{$mode…}`, `end.`) → BitBake → Assembly (`mov …,`) | — | **plain text** unless a rule fires (12 Linguist candidates; no safe default) | PHP/Pascal/asm rules → grammar; C-ish `#include`/`#define` → `cpp` | yes |
| `.cls` | VB6 (`VERSION x.x CLASS` + `BEGIN MultiUse`) → VBA (`VERSION x.x CLASS`) → TeX (`\NeedsTeXFormat`/`\ProvidesClass`) → ObjectScript (`^Class\s`) → OpenEdge ABL; also Apex | `tex` | **VB** (VS Code's `.cls`→TeX is a LaTeX-author bias; Windows users see VB6/VBA classes more) | TeX rule → `latex`; `public class`/`@isTest` → `apex` | yes |
| `.t` | Perl → Raku (`use v6`) → Turing; also Terra | `perl` | **Perl** (test files) | as `.pl` | yes |
| `.md` | Markdown (default) vs GCC Machine Description (`^(;;\|\(define_)`) | `markdown` | **owned by mdview — NOT claimed** (`src/plugins/mdview/mdview.cpp:234` registers `*.md;*.markdown`) | — | **no** |
| `.txt` | Vim Help (modeline `ft=help`) → Hosts (IPv4 lines) → Adblock (`[Adblock Plus …]`) → **Text** | — (no id) | **not claimed**: prose; the built-in viewer's wrap/hex/huge-file strengths matter; also in `Configuration.TextModeMasks` default `*.txt;*.602;*.xml` [V `src/dialogs4.cpp:454`] | — | **no** (user may add) |
| `.log` | not in Linguist | `log` grammar | **not claimed by default** (often huge, appended while open, ANSI colour escapes; the built-in viewer streams). Argument for the opposite: VS Code ships a `log` grammar (timestamps/levels colouring). Offer as an *opt-in* second entry the user can enable. | — | **no** (opt-in) |
| `.csv` | CSV (data) | — | **owned by dbviewer** (`*.csv;*.dbf`, `src/plugins/dbviewer/dbviewer.cpp:582`) — NOT claimed; same for `.tsv` (future dbviewer territory) | — | **no** |
| `.xml` | XML | `xml` | **XML** | first bytes may refine (`<svg`, `<plist`, `<Project`) but keep the `xml` grammar | yes |
| `.html/.htm` | Ecmarkup (`<emu-…>`) → HTML | `html` | **HTML** | — | yes |
| `.json` | OASv2/OASv3 (`"swagger": "2.…"`/`"openapi": "3.…"`) → JSON | `json` | **JSON** (JSONC grammar for `tsconfig*.json`, `.code-workspace`, `.jsonc`, `settings.json` by filename) | — | yes |
| `.svg` | SVG (data, `text.xml.svg`) | `xml` | **XML** grammar — claimed **conditionally**: no shipped plugin registers `*.svg` today [V grep] and pictview's WIC engine (feature 006) has no SVG path [V grep]; if pictview ever claims `.svg`, the plugin's entry must yield (it is listed *after* image plugins by design in §4.4) | binary-sniff only | yes (see open question) |
| `.cfg/.conf/.config` | `.cfg`: HAProxy → INI; `.conf`/`.config` not in Linguist | `.cfg/.conf`→`properties`; `.config` none | **INI** grammar for `.cfg/.conf`; `.config` → XML if `<?xml`/`<configuration` else INI (.NET `app.config` is XML) | signature | yes |
| `.reg` | Windows Registry Entries | — | **`reg` grammar** (exists in tm-grammars [V]); **must decode UTF-16LE BOM** — every modern export is "Windows Registry Editor Version 5.00" in UTF-16LE [V]; `REGEDIT4` files are ANSI | BOM/UTF-16 detection *before* the NUL rule | yes |
| `.bat/.cmd/.ps1` | Batchfile; PowerShell | `bat`; `powershell` | Batch; PowerShell (+ `.psm1/.psd1/.ps1xml`) | — | yes |
| `.resx` | XML | — | **XML** | — | yes |
| `.rc/.rc2/.rh` (this codebase) | not in Linguist / VS Code | — | **C/C++ grammar** (preprocessor, strings, comments highlight well; `.rh` is a header). Hand overlay. | — | yes |
| `.props/.vcxproj/.filters/.targets/.sln/.slnx` | XML … ; `.sln` = "Microsoft Visual Studio Solution" (`source.solution`, no Shiki grammar); `.slnx` XML | `xml` for the XML ones | XML grammar; **`.sln` → plain text with language name "Visual Studio Solution"** (demonstrates the "mapped, no grammar" path) | — | yes |
| `.iss/.isl` (Inno Setup) | Inno Setup (`source.inno`, no Shiki grammar) | — | **INI grammar as best effort** (sections + `key=value`; the `[Code]` Pascal block stays plain) or plain — decide in design | — | yes |
| `.slt` (this repo's translation archives) | — | — | plain text, UTF-8-BOM; **open question** whether claiming buys anything over the built-in viewer | — | ? |
| `.d` | D (`module …;`, `import …;`) → DTrace → Makefile (dependency files) | — | **D**; `.d` from `gcc -MD` matches the Makefile rule | rule | yes |
| `.cs` | Smalltalk (`![\w\s]+methodsFor: `) → C# | `csharp` | **C#** | — | yes |
| `.fs` | Forth (`^: `) → F# (`^\s*(#light\|open\|let\|module\|namespace\|type)`) → GLSL (`#version`, `uniform`, `vec[234]`) → Filterscript | `fsharp` | **F#** | GLSL rule → `glsl` | yes |
| `.pro` | Proguard → Prolog → INI (`last_client=`) → QMake (`HEADERS` + `SOURCES`) → IDL | — | **QMake** on Windows dev boxes is the common case; use rules, default plain | rules | yes |
| `.mod` | XML (`<!ENTITY `) → NMODL → Modula-2 → Linux Kernel Module / AMPL | `xml` | XML if rule fires, else plain | rule | yes |
| `.php` | Hack (`<?hh`) → PHP | `php` | PHP | — | yes |
| `.tsx` | XML (`<?xml version`) → TSX | `typescriptreact` | TSX | rule | yes |
| `.plist` | XML plist (`<?xml`/`<!DOCTYPE plist`/`<plist`) → OpenStep plist | — | XML if rule fires else plain; **binary plist (`bplist00`) → hand back** | signature | yes |
| `.yml/.yaml` | MiniYAML (tab-indented, no `---`) → OASv2/3 → YAML | `yaml` | YAML | — | yes |
| `.x` | DirectX 3D (`^xof 030(2\|3)(txt\|bin\|tzip\|bzip)`) → RPC → Logos → Linker Script | — | plain unless rule; the `bin`/`tzip`/`bzip` variants are **binary** → sniff hands back | rules | yes |

---

## 4. Which files the plugin should claim in the F3 Viewers list

### 4.1 How the Viewers list works in this product [V]

- The core walks `ViewerMasks` **in list order** and takes the **first** entry whose masks match; for a plugin entry it calls `CanViewFile(name)` and, on FALSE, `continue`s to the next matching entry (`src/fileswn5.cpp:1029-1052`). The alternate list (`AltViewerMasks`, **Alt+F3**, `src/salamand.rc:70`) defaults to a single `*.*` → internal viewer (`src/mainwnd1.cpp:421-427`), so the built-in viewer is always one keystroke away regardless of what the plugin claims.
- `CSalamanderConnectAbstract::AddViewer(masks, force)` (`src/plugins/shared/spl_base.h:289`): `;`-separated `*`/`?` masks, `|` forbidden. With `force=FALSE` the call is honoured **only during plugin installation** (first load where viewer support is newly detected — `supportViewer = (!supportViewer && SupportViewer)`, `src/plugins1.cpp:2327`); later loads ignore it, which is how user deletions survive. With `force=TRUE` (upgrade section, guarded by the plugin's own config version) missing masks are re-added.
- A new entry is **inserted at index 0** — the top of the list, above the internal `*.*` (`MainWindow->ViewerMasks->Insert(0, item)`, `src/plugins1.cpp:762`). Each `AddViewer` call creates **one entry**; pictview registers 11 (`src/plugins/pictview/pictview.cpp:1037-1047`). One entry's mask string is capped at `MAX_GROUPMASK = 1001` characters (`src/plugins/shared/spl_gen.h:633`), and the `force` update path truncates at 299 (`char ext[300]`, `src/plugins1.cpp:650`) — so ≈ 780 masks need **6–8 entries**, and any *upgrade* additions must be ≤ 299 characters per call.
- Masks match the full name, so extension-less names (`Dockerfile`, `Makefile`) and dot-files (`.gitignore` — treated as an extension, `src/fileswn5.cpp:985`) are claimable with plain masks.

### 4.2 Policy (a): the generated "source + config + structured text" set

Claim = (Linguist `programming` ∪ `markup` ∪ `data` languages that bridge to a shipped grammar) ∪ (VS Code overlay) ∪ (Windows/dev overlay) − (masks registered by any other shipped plugin) − (`prose` types) − (known binary look-alikes).

Counted from today's data [V, my script]: **776 extensions across 225 language→grammar pairs** before subtraction; minus 4 collisions with shipped plugins (`.csv` dbviewer; `.st`, `.pyx`, `.dtx` pictview) and ≈ 8 binary look-alikes (`.res` compiled resources, `.pt` PyTorch, `.pkl` pickle, `.msg` Outlook, `.app`, `.cdf`, `.prc` PalmOS, `.frm` MySQL — the last two can stay with sniffing) plus ≈ 25 overlay masks → **≈ 780 masks**. A curated **Tier 1** of ≈ 300 masks (the everyday languages: C/C++, C#, Java/Kotlin, JS/TS, Python, PHP, Go, Rust, Ruby, Perl, shell/Batch/PowerShell, HTML/CSS, XML family, JSON/YAML/TOML/INI, SQL, Markdown-adjacent docs, build files, diff) is what the acceptance tests should exercise; the long tail rides along in the same generated file. Appendix A lists the families with counts.

Grouping the entries by family (e.g. "C/C++ & Windows resources", "Web", ".NET/JVM", "Scripting", "Data & config", "XML-based", "Build/CI/shell", "Long tail") gives the user a **one-delete opt-out per family** in Options ▸ Viewers, which the current UI supports without change.

### 4.3 Policy (b): explicitly NOT claimed

| Not claimed | Reason |
|---|---|
| `*.md;*.markdown` | mdview owns them; two plugins claiming the same mask would silently shadow each other by list order. |
| `*.csv;*.dbf`, and `*.tsv` | dbviewer (`*.csv;*.dbf`) — tabular data belongs to a table viewer; `.tsv` reserved for the same reason. |
| `*.st;*.pyx;*.dtx` | pictview registers them (image formats). Cython `.pyx` users lose highlighting by default — documented; they can reorder. |
| `*.txt` | prose; built-in viewer's wrap, hex toggle, code-page auto-detect and streaming of multi-GB files; also the product's own `TextModeMasks` default. |
| `*.log` | huge/append-while-open/streaming; offered as an *opt-in* entry (VS Code's `log` grammar is the argument for it). |
| `*.svg` | claimed only while no image plugin does — placed in the last (long-tail) entry so any image plugin registered later takes precedence; see open question 3. |
| `*.bin;*.img;*.iso;…`, `*.rpm`, `*.exe;*.dll;*.spl;*.sys;…`, image formats, disc images | registered by uniso/7zip/tar/peviewer/pictview; and binary anyway. |
| `*.dat`, `*.lnk`, `*.pdb`, `*.res`, `*.pt`, `*.pkl`, `*.msg` | binary or usually binary; no grammar value. |
| `*.*` | never — the plugin must never become the catch-all; the internal viewer's `*.*` entry stays the terminal fallback. |
| `prose` types with no grammar value (`.rst`, `.adoc`, `.org`, `.textile`, `.pod`, `.wiki`) | keep the built-in wrapping viewer; **exception**: `.rst`/`.adoc` have Shiki grammars — treat as an open question (lean: claim, they are code-adjacent docs). |

### 4.4 Policy (c): new vs existing installs — user-facing behaviour

The existing mechanism (§4.1) is the same for both cases: when the product update ships the plugin, `plugins.ver` auto-installs it on first start (`src/plugins2.cpp:2976-3024`), the plugin's `Connect()` runs with `Viewer=TRUE`, and its entries are prepended. This is exactly what happened for mdview and every plugin since Open Salamander 2.5 — it is the product's convention, not a new decision. Proposed requirements (technology-agnostic):

1. **Fresh install**: after first start, F3 on any claimed type opens the new viewer; the built-in viewer remains the `*.*` fallback and Alt+F3 target.
2. **Existing install (upgrade)**: the plugin's entries appear **at the top** of Options ▸ Viewers, exactly as any newly installed plugin's would; **no pre-existing entry is removed, edited or reordered relative to the others**; a user entry that previously handled a now-claimed type (e.g. an external editor for `*.cpp`) is shadowed, not lost, and moving it above the plugin's entry restores it with one Move Up.
3. **Removals are remembered**: masks or entries the user deletes are never re-added by a later start (this is the `force=FALSE` semantic); a future plugin version may add *new* masks but must not resurrect deleted ones (upgrade section, ≤ 299 chars per addition).
4. **Discoverability**: the first time the plugin opens a file after installation it shows a one-line, dismissable hint naming Alt+F3 (built-in viewer) and Options ▸ Viewers (turn families off). No modal dialog.
5. **Reset**: Options ▸ Viewers already has per-entry editing; the plugin's own configuration page offers "Restore default file types" which re-issues the generated entries (removing its own stale ones first) without touching other entries.

---

## 5. Binary vs text

### 5.1 Reference rules [V]

| Rule | Sample | Criterion |
|---|---|---|
| **Git** `buffer_is_binary` | first 8,000 bytes | any NUL → binary (`.gitattributes` overrides) |
| **WHATWG MIME sniffing** | first 1,445 bytes | UTF-16BE/LE or UTF-8 **BOM → text/plain**; otherwise text iff **no "binary data byte"**: 0x00–0x08, 0x0B, 0x0E–0x1A, 0x1C–0x1F (note: TAB, LF, VT?, FF, CR, **ESC 0x1B**, 0x7F are *not* binary bytes — ANSI-coloured logs pass) |
| **Perl `-T`/`-B`** | "first block or so" | valid UTF-8 with non-ASCII → text; else if > ⅓ of bytes are "odd" (control codes, high bit) → binary; **any zero byte → binary** |
| **Built-in viewer**, `ViewerDetectEncoding` (`src/viewer2.cpp:76-113`) | `RECOGNIZE_FILE_TYPE_BUFFER_LEN = 10000` (`src/viewer.h:15`) | BOM (UTF-8/UTF-16LE/BE) → text; **any NUL → `VCE_LEGACY`/binary path** ("a binary file that happens to be valid UTF-8 is not forced to text"); strict UTF-8 validator rejects overlongs, surrogates, > U+10FFFF; then legacy `RecognizeFileType` (`src/codetbl.cpp:895-1122`): control bytes other than `\a \b \t \n \v \f \r 0x1A 0x04` count as "disallowed" — **> 0.5 % of the sample → binary**; **> 10 consecutive NULs → binary**; user overrides via `TextModeMasks`/`HexModeMasks` (`*.txt;*.602;*.xml` / empty). Caveat in the same file: UTF-16 without the 16-bit line scanner still "falls to the existing path (shown as hex)" (`src/viewer2.cpp:1049-1058`). |
| **mdview** `CanViewFile` (`src/plugins/mdview/viewer.cpp:255-278`) | first 512 bytes | UTF-16 BOM → accept; any NUL → **return FALSE** ("binary → let the text/hex viewer handle it"); unreadable → FALSE; 20 MB size gate inside the viewer with "Open as Text". |
| **peviewer** `CanViewFile` (`src/plugins/peviewer/peviewer.cpp:444-465`) | header | accepts only MZ/NE/LE/PE signatures — the mirror image: a *binary* viewer declining text. |
| `IsTextUnicode` (Win32) | — | statistical, famous false positives ("Bush hid the facts": ASCII text mis-detected as UTF-16LE) [V]; **do not use**. |

### 5.2 Recommended rule for the plugin (consistent with the built-in viewer, tolerant of UTF-16)

Sample = first **8 KB** (one read; reused for shebang/modeline/signature checks). Decide in this order:

1. Unreadable / locked → **not ours** (FALSE).
2. **BOM** UTF-8 / UTF-16LE / UTF-16BE → text (decode accordingly; `.reg` lands here).
3. **UTF-16 without BOM** (unlike the built-in viewer today): if ≥ 90 % of odd (or even) bytes are 0x00 *and* the decoded UTF-16 units contain no binary-data code points other than TAB/LF/CR → text, UTF-16LE (or BE). This covers `.reg` files saved without BOM and many Windows tool logs/configs. This must run **before** the NUL rule.
4. Otherwise **any NUL → binary**.
5. Count WHATWG binary-data bytes (0x00–0x08, 0x0B, 0x0E–0x1A, 0x1C–0x1F); **> 0.5 % of the sample → binary** (the built-in viewer's threshold; a single stray control byte in a source file — e.g. a `0x0C` form feed, which WHATWG treats as whitespace anyway — does not flip it).
6. Known binary signatures from §2.5 (`%PDF`, `PK\x03\x04`, `MZ`, `\x7fELF`, PNG/GIF/JPEG/RIFF/gzip, `bplist00`, MPEG-TS sync pattern) → binary even when steps 4–5 pass (e.g. a tiny text-only ZIP comment header cannot pass, but `xof 0302bin` `.x` files can).
7. Text: strict UTF-8 valid → UTF-8; else the legacy code page (the product's existing WTF-8/ANSI house rules apply to *names*; for *content* reuse the built-in viewer's code-page detection or default to CP_ACP with a status-bar encoding indicator and manual override).

### 5.3 What to do when a claimed file turns out to be binary — options and recommendation

| Option | Behaviour | Assessment |
|---|---|---|
| A. Show as plain text with a warning | opens the plugin window with replacement characters | Worst: unreadable, slow (render of NULs), contradicts the built-in viewer's auto-hex. |
| B. Built-in hex mode inside the plugin | re-implement a hex viewer in WebView | Duplicates a mature core feature (find, go-to, code pages, huge files). |
| **C. Hand back (`CanViewFile` → FALSE)** | the core continues down the Viewers list; the internal `*.*` entry opens the file, which its own heuristic renders as **hex** | **Recommended.** Zero UI invention, identical to mdview/peviewer, respects user-edited lists (if the user put an external tool below the plugin, that is what runs), and Alt+F3 remains the manual escape. |
| D. C + in-viewer fallback | as C; additionally, if the file becomes binary *after* `CanViewFile` (changed on disk, or opened via the plugin's own next/previous-file navigation which bypasses the list) the plugin shows a one-line notice with an "Open in built-in viewer" action (`ViewFileInPluginViewer`, as mdview's engine-unavailable path does at `src/plugins/mdview/viewer.cpp:289-303`) | Recommended as the completion of C. |

Same hand-back for **oversized** files (mdview uses a 20 MB gate): sniffing must not read more than the sample, and `CanViewFile` must decline (or the viewer must offer "Open in built-in viewer") above a configurable size so that a 4 GB `.log`-renamed-`.sql` never stalls the UI. `CanViewFile` runs on the main thread before the viewer window exists — budget: one open + one ≤ 8 KB read.

---

## 6. This repository — masks already registered, and the built-in binary rule

### 6.1 Shipped viewer plugins and their masks (`plugins.cfg` on; grep `AddViewer` in `src/plugins/`) [V]

| Plugin (on) | Masks registered with `AddViewer` | `CanViewFile` |
|---|---|---|
| **mdview** | `*.md;*.markdown` (`mdview.cpp:234`) | declines unreadable/NUL-containing (§5.1) |
| **dbviewer** | `*.csv;*.dbf` (`dbviewer.cpp:582`) | always TRUE (`dbviewer.h:57`) |
| **pictview** | 11 entries, ≈ 130 image masks incl. `*.psp*`, `*.st`, `*.pyx`, `*.dtx`, `*.img`, `*.eps;*.ept;*.ai`, `*.mov`, `*.scr`, `*.cdr`, `*.sep`, `*.msp`… (`pictview.cpp:1037-1047`, upgrades `1052-1134`) | header test only for the ≈ 14 masks known to collide (`pictview.cpp:1989-2030`: `.SCR` → peviewer, `.IMG` → uniso, `.MOV`, `.CDR`, `.MSP`…) |
| **peviewer** | `*.cpl;*.dll;*.drv;*.exe;*.ocx;*.spl;*.sys;*.scr` (`peviewer.cpp:265`) | MZ/NE/LE/PE signature |
| **uniso** | `*.bin;*.img;*.iso;*.isz;*.nrg;*.pdi;*.cdi;*.cif;*.ncd;*.c2d;*.mdf` (+`*.dmg`, `*.isz` upgrades) (`uniso.cpp:395-438`) | — |
| **7zip** | `*.nrg;*.pdi;*.cdi;*.cif;*.ncd`, `*.c2d` (upgrade section, `7zip.cpp:642-648`) | — |
| **tar** | `*.rpm` (`tardll.cpp:289`; also the default config's first entry, `src/mainwnd1.cpp:405-410`) | — |
| off (not shipped): mmviewer (`*.wav;*.wave;*.wma;*.ogg` …), demoview (`*.dmv`), demoplug (`*.dop;*.dop2;*.dmp2`) | — | — |
| **Default core list** | `*.rpm` → tar, then `*.*` → internal; Alt list: `*.*` → internal (`src/mainwnd1.cpp:405-427`) | — |

Overlap between the data-derived candidate set and these masks is exactly **`.csv`, `.st`, `.pyx`, `.dtx`** [V]; between VS Code's built-in extensions and these masks: `.cpt`, `.jmx` (pictview) plus `.md/.markdown` [V]. The generator must take the exclusion list from the other plugins' `AddViewer` strings (a checked-in copy, verified by a test against the sources) so a future pictview/dbviewer addition is caught.

### 6.2 The built-in viewer's binary decision (for consistency)

`src/viewer2.cpp:1023-1131`: if `DefViewMode == Auto`, `TextModeMasks` / `HexModeMasks` (defaults `*.txt;*.602;*.xml` / empty, `src/dialogs4.cpp:454-457`) force a mode; else `ViewerDetectEncoding` over the first 10,000 bytes (BOM → Unicode text; NUL → legacy/binary path; strict UTF-8 with a high byte → UTF-8 text), then the legacy `RecognizeFileType` (`src/codetbl.cpp:895`) sets `isText` — false when disallowed control bytes exceed 0.5 % or > 10 NULs run together — and picks a code page; `isText` → `vtText` else `vtHex`. UTF-16 without BOM is currently hex. Long lines (`TEXT_MAX_LINE_LEN`) can also flip a text file to hex with a prompt (`src/viewer.cpp:1272-1290`). The plugin rule in §5.2 is a superset (adds UTF-16 detection and signatures) and agrees on every case the built-in viewer would call binary — so a hand-back always lands in hex, never in a second "text" rendering.

---

## Recommended requirements for the spec (testable, technology-agnostic, user-facing)

**Mapping and grammars**
- FR-D01: The set of file names the viewer claims and the name→language table SHALL be generated from a versioned external source (GitHub Linguist at a pinned revision) plus two checked-in overlays (editor conventions; Windows/dev tooling), and the generated table SHALL be committed and reviewed like source. *Test: regenerating with the pinned inputs yields a byte-identical file.*
- FR-D02: Every language in the table SHALL either reference a grammar that ships with the viewer or be marked "no grammar"; a build/test SHALL fail if a referenced grammar is missing. *Test: remove one grammar → build fails naming the language.*
- FR-D03: Opening a claimed file whose language has no grammar SHALL display the file as plain text with the language name shown, using the same theme, line numbers and encoding handling as highlighted files. *Test: F3 on `foo.sln` → plain rendering, status shows "Visual Studio Solution".*

**Detection**
- FR-D04: Language selection SHALL consider, in order: exact file name (case-insensitive), file-name pattern, longest multi-dot suffix, extension; content SHALL be consulted only when the name yields no language or more than one. *Tests: `Dockerfile` → Dockerfile; `x.d.ts` → TypeScript; `a.blade.php` → Blade; `README` (no extension, no signature) → plain.*
- FR-D05: For an extension-less or ambiguous name, the viewer SHALL recognise a shebang on line 1 (including `/usr/bin/env` forms and versioned interpreters), an Emacs or Vim modeline within the first or last five lines, and the documented first-bytes signatures. *Tests: `run` with `#!/usr/bin/env python3` → Python; `notes` with `# vim: ft=yaml` → YAML; `data` starting `<?xml` → XML.*
- FR-D06: The ambiguous extensions in §3 SHALL resolve to the stated defaults, and their listed disambiguation rules SHALL select the alternative. *Tests: `.h` with `@interface` → Objective-C, else C++; `.ts` containing `<TS` → XML; `.pl` with a `:-` clause → Prolog; `.v` with `module … (` → Verilog; `.sql` with `DECLARE @` → T-SQL if shipped else SQL.*
- FR-D07: The user SHALL be able to override the detected language for the open file (a language picker), and the choice SHALL not be persisted per file unless explicitly saved (out of scope for v1 to persist).

**Claiming**
- FR-D08: On installation the viewer SHALL register its file types as a small number (≤ 8) of family-grouped entries in Options ▸ Viewers; removing one entry SHALL disable exactly that family. *Test: delete the "XML-based" entry → `.vcxproj` opens in the built-in viewer, `.cpp` still in the plugin.*
- FR-D09: The viewer SHALL NOT claim `*.md`, `*.markdown`, `*.csv`, `*.dbf`, `*.tsv`, `*.txt`, `*.log`, `*.*`, or any mask registered by another shipped viewer plugin; a test SHALL compare the generated masks against the other plugins' registrations. *Test: intersection is empty.*
- FR-D10: `*.log` SHALL be available as an opt-in entry the user can enable from the plugin's configuration; it SHALL be off by default.
- FR-D11: On upgrade of an existing installation, no pre-existing Viewers entry SHALL be deleted, modified or reordered relative to other pre-existing entries; the viewer's entries are added in front of them, and masks or entries the user later removes SHALL never be re-added automatically. *Test: seed a list with a custom `*.cpp` external entry, upgrade, verify it is intact and one Move Up restores it; delete a plugin entry, restart, verify it stays deleted.*
- FR-D12: The first time the viewer opens a file after installation it SHALL show a dismissable, non-modal hint that Alt+F3 opens the built-in viewer and that file types are configured in Options ▸ Viewers.
- FR-D13: The plugin's configuration SHALL offer "Restore default file types", which re-creates the viewer's own entries and touches no other entry.

**Binary and encodings**
- FR-D14: Before opening, the viewer SHALL classify the file as text or binary from at most the first 8 KB using: BOM → text; UTF-16 without BOM → text; otherwise any NUL → binary; > 0.5 % control bytes (0x00–0x08, 0x0B, 0x0E–0x1A, 0x1C–0x1F) → binary; known binary signatures → binary. *Tests: a `.ts` MPEG stream, a `bplist00` `.plist`, an `.x` "xof 0302bin", a `.h` that is a WinHelp file → binary; a UTF-16LE `.reg` with and without BOM, an ANSI-coloured `.sh`, a CP1250 `.ini` → text.*
- FR-D15: A binary-classified file SHALL be declined so the next matching viewer in the user's list opens it (by default the built-in viewer, which shows hex); the classification SHALL take ≤ 50 ms on a local disk and never read the whole file. *Test: F3 on a 2 GB file renamed `.sql` opens the built-in viewer within a second.*
- FR-D16: If content becomes binary after the pre-open check (file replaced on disk, or reached via in-viewer next/previous navigation), the viewer SHALL show a one-line notice with an "Open in built-in viewer" action instead of rendering.
- FR-D17: Files larger than a configurable limit (default 20 MB, consistent with mdview) SHALL be declined to the built-in viewer or offered "Open in built-in viewer".
- FR-D18: The viewer SHALL decode UTF-8 (with/without BOM), UTF-16LE/BE (with/without BOM) and the system ANSI code page, indicate the encoding in its status area, and allow a manual override; the text/binary rule SHALL agree with the built-in viewer's on every file the built-in viewer would show as hex.

## Open questions

1. **Highlighter choice fixes the grammar set** (Shiki/TextMate ≈ 230 languages vs highlight.js ≈ 190 core vs Prism 297): the bridge table and the count in Appendix A assume the Shiki `tm-grammars` set; if highlight.js is chosen, regenerate and expect ≈ 15 % fewer bridged languages (no Verilog/VHDL/CMake/Solidity split the same way) but ≈ 40 % smaller payload. Also decide whether to ship the five GPL-3.0 grammars.
2. **`.rst`, `.adoc`, `.tex`, `.bib`** (prose/markup with grammars): claim or leave to the built-in viewer? Lean: claim `.tex/.bib` (code-like), leave `.rst/.adoc` unless mdview grows a docs role.
3. **`.svg`**: claim as XML now (nobody else does) or reserve for pictview's future SVG support? If claimed, keep it in the last entry so a later image plugin, inserted above, wins automatically.
4. **`.pyx`, `.st`, `.dtx`**: accept losing Cython/Smalltalk/TeX-doc highlighting to pictview's obscure image formats, or ask pictview to drop those three masks in its next upgrade section (`ForceRemoveViewer`)? Cython is the only one with a real audience.
5. **`.slt`** and other product-internal formats (`plugins.cfg`, `languages.cfg`): claim as INI/plain or not at all?
6. **UTF-16 without BOM**: implement the even/odd-NUL detector in the plugin only (recommended) or also fix the built-in viewer (`src/viewer2.cpp:1049-1058` comment says the 16-bit line scanner is a pending follow-up)?
7. **`.iss`**: INI grammar as a best-effort, plain text, or vendor an Inno Setup grammar (exists on the Marketplace, licence to check)?
8. **Name-less files fallback**: is a JS-side guesslang (30 languages) inside the WebView worth ≈ 4 MB and a warm-up for the rare `README`/`LICENSE`/`notes` case? Default answer: no for v1.
9. **`.log` opt-in UI**: a checkbox in the plugin's configuration that adds/removes one Viewers entry, or just documentation telling the user to add `*.log` to an entry? The checkbox needs `AddViewer(force)`/`ForceRemoveViewer` outside `Connect()` — check that the connect interface is available at configuration time.

## Sources

**Upstream data and docs (fetched 2026-08-26)**
- Linguist `languages.yml` (header fields, counts): https://github.com/github-linguist/linguist/blob/main/lib/linguist/languages.yml · raw counted locally
- Linguist licence (MIT): https://raw.githubusercontent.com/github-linguist/linguist/main/LICENSE
- Linguist `heuristics.yml` (138 blocks; rules quoted in §3): https://github.com/github-linguist/linguist/blob/main/lib/linguist/heuristics.yml
- Linguist strategy order: https://github.com/github-linguist/linguist/blob/main/docs/how-linguist-works.md
- Linguist modeline and shebang strategies: https://raw.githubusercontent.com/github-linguist/linguist/main/lib/linguist/strategy/modeline.rb · https://raw.githubusercontent.com/github-linguist/linguist/main/lib/linguist/shebang.rb
- VS Code `contributes.languages` reference: https://code.visualstudio.com/api/references/contribution-points#contributes.languages
- VS Code built-in extensions (all `extensions/*/package.json`, e.g. typescript-basics): https://github.com/microsoft/vscode/tree/main/extensions
- VS Code association priority: https://raw.githubusercontent.com/microsoft/vscode/main/src/vs/editor/common/services/languagesAssociations.ts
- VS Code ML detection thresholds: https://raw.githubusercontent.com/microsoft/vscode/main/src/vs/workbench/services/languageDetection/browser/languageDetectionWebWorker.ts · service: …/languageDetectionWorkerServiceImpl.ts
- `@vscode/vscode-languagedetection` (MIT, guesslang, 30 languages): https://github.com/microsoft/vscode-languagedetection
- highlight.js API (`highlightAuto`, `registerAliases`): https://highlightjs.readthedocs.io/en/latest/api.html · README ("over 180 languages"): https://github.com/highlightjs/highlight.js · `SUPPORTED_LANGUAGES.md`
- Shiki grammars and licences: https://github.com/shikijs/textmate-grammars-themes/blob/main/packages/tm-grammars/README.md · https://shiki.style/languages
- Prism `components.json`: https://github.com/PrismJS/prism/blob/master/components.json
- hyperpolyglot: https://github.com/monkslc/hyperpolyglot · go-enry: https://github.com/go-enry/go-enry
- Magika (Apache-2.0; 1.0 announcement): https://github.com/google/magika · https://opensource.googleblog.com/2025/11/announcing-magika-10-now-faster-smarter.html · https://www.infoq.com/news/2025/12/magika-rust-file-type-detector/
- WHATWG MIME Sniffing ("binary data byte", text-or-binary rules): https://mimesniff.spec.whatwg.org/
- Perl `-T`/`-B` heuristic: https://perldoc.perl.org/functions/-X
- Git binary detection (NUL in first 8,000 bytes): https://github.com/sharkdp/content_inspector · https://secure.phabricator.com/T13143
- `.reg` encoding (UTF-16LE "Version 5.00" vs ANSI `REGEDIT4`): https://filetypedb.com/system/reg/ · https://libguestfs.org/hivexregedit.1.html
- `IsTextUnicode` false positives: https://en.wikipedia.org/wiki/Bush_hid_the_facts · https://www.sixfoisneuf.fr/posts/bush-hid-the-facts/

**This repository**
- Viewer registration contract: `src/plugins/shared/spl_base.h:283-294` (`AddViewer`, `ForceRemoveViewer`), `:515-560` (upgrade section rules); `src/plugins/shared/spl_view.h:62-65` (`CanViewFile` cascade contract)
- Core: `src/plugins1.cpp:636-773` (`CSalamanderConnect::AddViewer`, `Insert(0)`, 300-byte update buffer), `:2198-2327` (install vs upgrade flags), `src/plugins2.cpp:2976-3024` (plugins.ver auto-install), `src/fileswn5.cpp:985-1052` (list walk, `CanViewFile` → `continue`), `src/mainwnd1.cpp:405-427` (default entries), `src/salamand.rc:70` (Alt+F3), `src/plugins.h:2762-2764` (`ViewerType` encoding), `src/plugins/shared/spl_gen.h:633` (`MAX_GROUPMASK 1001`)
- Built-in viewer binary rule: `src/viewer2.cpp:76-113` (`ViewerDetectEncoding`), `:1023-1131` (mode decision), `src/viewer.h:15`, `src/codetbl.cpp:895-1122` (`RecognizeFileType`), `src/dialogs4.cpp:454-457` (`TextModeMasks` default), `src/viewer.cpp:1272-1290` (long-line hex switch)
- Other plugins' masks: `src/plugins/mdview/mdview.cpp:234`, `src/plugins/mdview/viewer.cpp:255-303`, `src/plugins/mdview/IMPLEMENTATION_NOTES.md:30-33`, `src/plugins/dbviewer/dbviewer.cpp:582`, `src/plugins/dbviewer/dbviewer.h:57`, `src/plugins/pictview/pictview.cpp:1037-1134`, `:1989-2030`, `src/plugins/peviewer/peviewer.cpp:265-269`, `:444-465`, `src/plugins/uniso/uniso.cpp:395-438`, `src/plugins/7zip/7zip.cpp:642-648`, `src/plugins/tar/tardll.cpp:289`; `plugins.cfg` (18 on / 10 off)
- Feature brief: `features/source_files_viewer.md`

---

## Appendix A — Proposed claimed set, grouped by grammar family (data-derived) [V]

Derived from Linguist `programming`/`markup`/`data` languages bridged to a Shiki `tm-grammars` id; the four other-plugin collisions and the binary look-alikes are struck; the Windows/dev overlay is appended. Counts are per family after exclusions. **Total ≈ 780 masks / ≈ 225 language→grammar pairs**; Tier 1 (bold families) ≈ 300 masks.

**C/C++ family & Windows native (≈ 45)** — `cpp` (20): .c++ .cc .cp .cpp .cppm .cxx .h .h++ .hh .hpp .hxx .inc .inl .ino .ipp .ixx .re .tcc .tpp .txx · `c` (5): .c .cats .h .h.in .idc · `objective-c`/`objective-cpp` (3): .h .m .mm · `cuda` (2): .cu .cuh · **overlay** (≈ 10): .rc .rc2 .rh .dlg .idl .odl .def(plain) .manifest(xml) .inf(ini) .natvis(xml, already) · `cmake` (2): .cmake .cmake.in + `CMakeLists.txt` · `make` (6): .d .mak .make .makefile .mk .mkfile + `Makefile`/`GNUmakefile`
**.NET (≈ 20)** — `csharp` (5): .cake .cs .cs.pp .csx .linq · `fsharp` (3): .fs .fsi .fsx · `vb` (9): .bas .cls .ctl .dsr .frm .vb .vba .vbhtml .vbs · `razor` (8): .asax .ascx .ashx .asmx .aspx .axd .cshtml .razor · `powershell` (3): .ps1 .psd1 .psm1 (+ .ps1xml xml) · `bat` (2): .bat .cmd
**JVM (≈ 20)** — `java` (3): .jav .java .jsh · `kotlin` (3): .kt .ktm .kts · `scala` (4): .kojo .sbt .sc .scala · `groovy` (5): .gradle .groovy .grt .gtpl .gvy · `clojure` (10): .bb .boot .cl2 .clj .cljc .cljs .cljs.hl .cljscm .cljx .hic
**Web (≈ 60)** — `javascript` (25): .js .mjs .cjs .jsx .es .es6 .jsm .jss .pac … · `typescript` (3): .ts .mts .cts · `tsx` (1) · `html` (9): .htm .html .xht .xhtml .hta .jsp .tag .inc .html.hl · `css`/`scss`/`sass`/`less`/`stylus`/`postcss` (7) · `vue` `svelte` `astro` `marko` `imba` `edge` `templ` (7) · `handlebars` (3): .handlebars .hbs .mustache · `pug` (2) · `haml` `slim` `twig` `liquid` `jinja` (4: .j2 .jinja .jinja2 .njk) `erb` (3) `blade` (2: .blade .blade.php) `soy` `glimmer-js/ts` · `php` (10): .php .php3-5 .phps .phpt .inc .ctp .aw .fcgi · `hack` (4) · `graphql` (3) · `http`/`hurl` (2)
**Scripting (≈ 75)** — `python` (17): .py .pyi .pyw .py3 .gyp .gypi .rpy .wsgi .spec … · `cython` (3): .pxd .pxi ~~.pyx~~ · `ruby` (22): .rb .rbw .rake .gemspec .podspec .thor .jbuilder … + `Gemfile`/`Rakefile`/`Vagrantfile`/`Podfile`/`Brewfile` · `perl` (10): .pl .pm .t .cgi .ph .plx .psgi .al .perl .fcgi · `raku` (13) · `lua` (8) `luau` · `tcl` (6) · `r` (4): .r .rd .rhistory .rsx · `julia` (1) · `shellscript` (16): .sh .bash .zsh .ksh .bats .command .tool … + `.bashrc` `.bash_profile` `.zshrc` `.profile` · `fish` `nushell` · `awk` (5) · `applescript` (2) · `ahk` (4) · `autoit` (1) · `viml` (4): .vim .vimrc .vba(!) .vmb · `elixir` (2) `erlang` (8) · `gnuplot` (6)
**Systems & functional (≈ 70)** — `rust` (2): .rs .rs.in · `go` (6): .go .gohtml .gotmpl .tmpl .tpl .html.tmpl + `go.mod` `go.sum` · `zig` (2) · `d` (2) · `nim` (5) · `swift` (1) · `dart` (1) · `haskell` (3) · `ocaml` (7) · `elm` `purescript` `gleam` `crystal` `odin` `v` `mojo` `moonbit` `c3` `cairo` `move` `vyper` `solidity` (1 each) · `pascal` (7): .pas .dpr .dfm .lpr .pp .inc .pascal · `ada` (3) · `cobol` (5) · `fortran-free-form` (8) `fortran-fixed-form` (2) · `lisp` (8) `emacs-lisp` (3) `scheme` (6) `racket` (4) `fennel` `hy` · `prolog` (5) · `smalltalk` (2: .cs ~~.st~~) · `apl` (2) · `abap` (1) · `sas` `stata` (8) · `matlab` (2) · `wolfram` (10) · `apex` (3) · `ballerina` `chapel` `clarity` `codeql` `lean` `coq` (2: .coq .v) · `asm` (9): .asm .s .S .nasm .inc … · `llvm` (1) · `wasm` (2) · `smali` (1)
**Hardware & shaders (≈ 45)** — `verilog` (2) `system-verilog` (3) `vhdl` (8) · `glsl` (23): .glsl .vert .frag .geom .tesc .tese .comp … · `hlsl` (5): .hlsl .hlsli .fx .fxh .cginc · `wgsl` `shaderlab` `gdshader` (2) · `gdscript` (1) `gdresource`
**Data & configuration (≈ 90)** — `json` family: `jsonl` grammar covers .json .jsonl .geojson .topojson .har .webmanifest .avsc .gltf .sarif .tfstate .mcmeta .yy .yyp … (22) · `jsonc` (19): .jsonc .code-workspace .code-snippets .sublime-* .tsconfig.json + `tsconfig.json`/`jsconfig.json`/`settings.json`/`launch.json`/`tasks.json` · `json5` `hjson` · `yaml` (10): .yml .yaml .sublime-syntax .mir … · `toml` (2) + `Cargo.lock` `Pipfile` · `ini` (17): .ini .cfg .cnf .prefs .properties .url .service .socket .timer .mount .network .target .container .pro .dof .frm .lektorproject · **overlay**: .conf .config .inf .iss .isl (ini best-effort) · `properties` (1) · `dotenv` (1) + `.env` `.env.*` · `editorconfig` (1) · `gitignore` `git-config` `git-commit` `git-rebase` + `.gitignore` `.gitattributes` `.gitmodules` `.gitconfig` · `reg` (1): .reg · `desktop` (2) · `systemd` · `ssh-config` · `nginx` (3) `apache` (2) · `hcl` (6): .hcl .tf .tfvars .nomad .tofu `terraform` · `docker` (2) + `Dockerfile` `Dockerfile.*` `Containerfile` · `proto` (1) · `prisma` (1) · `graphql` · `kdl` `ron` `pkl` (~~.pkl~~ excluded: pickles) `cue` `jsonnet` (2) `nix` (1) `bicep` (2) `kusto` (2) `powerquery` `dax` · `csv`/`tsv` grammars exist but the masks are **not claimed**
**XML-based (≈ 105)** — `xml` (112 − exclusions): .xml .xsd .xsl(t via `xsl` 2) .dtd(overlay) .svg(?) .resx .config(?) .props .targets .vcxproj .filters .csproj .vbproj .fsproj .proj .nuspec .wxs .wxi .wxl .wixproj .xaml .axaml .storyboard .xib .ui .plist(XML variant) .rss .rdf .kml .gpx .osm .xliff .xlf .xmp .mjml .natvis .vsixmanifest .vstemplate .vssettings .dotsettings .iml .launch .ant .jelly .ivy .wsdl .xul .scxml .x3d .dita .ditamap .admx .adml .clixml .ps1xml .psc1 .slnx .mod .ts(Qt, via sniff) .tsx(sniff) .rs(sniff) .gml .sch .typ(sniff) … ; excluded: ~~.res~~ ~~.pt~~ ~~.msg~~(ROS) ~~.app~~ · `xsl` (2)
**Docs & markup adjacent (≈ 25)** — `latex`/`tex` (14): .tex .sty .cls .bib(`bibtex` 2) .ltx .ins ~~.dtx~~ … · `rst` (open q.) · `asciidoc` (open q.) · `org` · `wikitext` · `mdx` · `mermaid` (2) · `typst` (1) · `po` (Gettext .po/.pot) · `diff` (2): .diff .patch · `log` (**opt-in only**) · `sql` (10): .sql .ddl .mysql .pgsql .db2 .tab .udf .viw .prc .inc · `plsql` (16): .pls .plb .pks .pkb .trg .fnc .prc .spc .bdy .vw … · `sparql` `cypher` (3) `surrealql` `splunk`
**Build, CI & tooling (≈ 30)** — `cmake` `make` (above) · `just` `gn` (2) `hxml` `jison` `regexp` (2) `rosmsg` (3) `gherkin` (2) `codeowners` + `CODEOWNERS` · `Jenkinsfile*` (groovy) · `.clang-format` `.clang-tidy` (yaml) · `meson.build`(plain) `BUILD`/`WORKSPACE`(python-like, plain) · `shellsession` (1)

**Excluded from the generated set**: other plugins' masks (`.csv .dbf .st .pyx .dtx` + all image/disc/PE masks), `.md .markdown .txt .log(default) .tsv`, binary look-alikes (`.res .pt .pkl .msg .app .cdf`), `prose` types without grammars, and `*.*`.

## Appendix B — Where the numbers come from

All counts in this report were produced by two throw-away scripts run against the upstream files downloaded on 2026-08-26 (kept only in the session scratchpad, not in the repository): Linguist `languages.yml` (833 languages / 1,486 extensions / 419 filenames / 173 ambiguous), `heuristics.yml` (138 blocks; parsed with PyYAML), VS Code `extensions/*/package.json` (97 dirs → 83 contributions, 342 extensions, 72 filenames, 26 patterns, 14 firstLine), highlight.js `SUPPORTED_LANGUAGES.md` (308 rows), Shiki `tm-grammars/README.md` (260 ids, licence tally), Prism `components.json` (297). The generator described in §1.5 should reproduce them deterministically from a pinned revision.

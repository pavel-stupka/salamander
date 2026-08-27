# Phase 0 Research — Source & Configuration File Viewer (codeview)

**Date**: 2026-08-26 · **Inputs**: the five specification-phase reports under
[`research/`](research/) (web-highlighters, native-and-hybrid,
codebase-integration, viewer-ux-webview2, language-detection), the spec's
Clarifications (2026-08-26), the shared-engine contract
(`architecture/11-webview2-integration.md`), and mdview as the reference
implementation. Every decision below resolves a Technical Context unknown or
a risk flagged by those reports; no NEEDS CLARIFICATION remains.

---

## D1. Rendering route: WebView2 + bundled JavaScript highlighter

- **Decision**: Render in the shared WebView2 engine; enable `IsScriptEnabled`
  on **this plugin's controllers only**; the highlighter is the plugin's own
  bundled JS. (Spec clarification #1.)
- **Rationale**: 2× format coverage and ready-made themes vs. every native
  route; permitted per-controller by contract §2.3; mdview's zero-script
  posture unchanged. (`native-and-hybrid.md` §1.)
- **Alternatives considered**: native Scintilla+Lexilla control (best large
  files, but a second rendering technology, ~half the coverage, themes
  authored in-house — Notepad++ theme XMLs are GPLv3); hybrid C++ tokenizer →
  static HTML scripts-off (recorded as the fallback architecture if the
  scripts-on decision were ever reversed); extending the built-in viewer
  (invasive in 7.4 k-line legacy core).

## D2. Highlighting library: Shiki, single engine in v1

- **Decision**: `@shikijs/core` + **`@shikijs/engine-oniguruma`** (WASM,
  inlined as base64 by `shiki/wasm` — no separate `.wasm` request),
  grammars from `tm-grammars`, themes from `tm-themes`.
  **REVISED 2026-08-26 by the T013 spike** (`spike-results.md` §2): the
  JavaScript RegExp engine originally chosen here measured **2.2× slower**
  at every size (15 ms/KB vs 7 ms/KB). The reasons to avoid WASM do not
  apply to the inlined build, so the faster engine wins; the CSP gains
  `'wasm-unsafe-eval'`.
  One engine ships in v1; formats without a shippable grammar open as plain
  text per FR-003. highlight.js is the **recorded extension**, not shipped:
  adding it later can restore the few licence-excluded languages
  (nginx, ada, …) and a `highlightAuto` oracle, at the cost of a second
  engine to audit and theme-parity generation.
- **Rationale**: 242 VS Code-grade grammars, MIT, and the whole VS Code theme
  ecosystem is drop-in (`web-highlighters.md` §1, §3.3). The engine choice is
  now made on measurement (`spike-results.md` §2): Oniguruma is 2.2× faster,
  and because `shiki/wasm` inlines the binary as base64 there is no separate
  `.wasm` request — the folder-mapping `.wasm` bug (#4838) and the MIME
  question never arise. The only cost is `'wasm-unsafe-eval'` in the CSP.
- **Alternatives considered**: highlight.js alone (1.4 MB, but ~190 langs,
  theme/grammar class mismatches on niche languages, no line-stateful
  tokenisation → cannot highlight progressively); Prism (frozen since
  2025-03); CodeMirror 6 read-only (**the fallback architecture** if the
  measured spike shows a Shiki virtual line list cannot make multi-MB files
  feel instant — fewer languages, CM-specific themes); Monaco (size, Monarch
  themes); web-tree-sitter (~1.4 MB WASM per grammar); starry-night full set
  (694 grammars, GPL-free — a possible long-tail extension once JS-engine
  compatibility is prototyped); Shiki `langs-precompiled` (its own docs: not
  yet supported).
- **Engine options pinned**: `tokenizeMaxLineLength` = the line-length gate
  (20 000 default), `tokenizeTimeLimit` default 500 ms/line kept. The
  JavaScript RegExp engine is **not** shipped (superseded by the spike); it
  remains the recorded fallback if `'wasm-unsafe-eval'` ever becomes
  unacceptable, at 2.2× the tokenisation cost.

## D3. Licence policy for vendored assets

- **Decision**: Ship only GPLv2-compatible assets (spec clarification #2).
  **Audit executed 2026-08-26** (`spike-results.md` §7): of the 33
  unlicensed/NOASSERTION grammars, 21 were resolved to a permissive licence
  from their source repositories (9 TextMate-permissive, 8 MIT, 2 BSD,
  2 Apache-2.0) and ship. **18 assets are excluded**: `ada`, `ahk2`,
  `gnuplot`, `nginx`, `org`, `racket` (copyleft); `apl`, `dax`,
  `dream-maker`, `glsl`, `hurl`, `kusto`, `matlab`, `rel`, `sparql`, `tcl`,
  `ts-tags`, `turtle` (licence unresolvable); theme `aurora-x` (GPL-3.0).
  **224 grammars ship** (≥ 200 per SC-001). Excluded grammars that other
  grammars import (`glsl` ← cpp/elm/nim, `kusto` ← kql, `turtle` ← sparql,
  `ts-tags` ← lit) are replaced by a **licence stub** exporting `[]`, so the
  importing language keeps working and contributes no excluded content.
  Apache-2.0 grammars (22 + elixir + llvm) ship, flagged in
  `doc/third_party.txt` as compatible via "or later". MPL-2.0 (bird2, hcl,
  terraform) ship after per-file Exhibit B check. Excluded theme: Aurora X
  (GPL-3.0). `tools/codeview/build_web.py` enforces this as an **automated
  licence audit** — an asset without a resolved compatible licence fails the
  generation run, and the shipped manifest is committed.
- **Rationale**: constitution IV; clarification #2; audit keeps the set
  honest as upstream versions move. (`web-highlighters.md` §4.3.)
- **Alternatives considered**: accepting GPLv3 for the combined work
  (rejected by clarification); per-asset legal review only (no automation —
  rejected: regresses silently on upgrade).

## D4. Asset packaging: embedded resources served from memory

- **Decision**: All web assets (page shell, CSS, viewer.js, Shiki engine,
  per-language grammar chunks, theme JSONs) are committed prebuilt under
  `src/plugins/codeview/web/` and embedded as `RCDATA` resources in
  `codeview.spl`; the interceptor serves them from memory with explicit
  `Content-Type` (mdview's in-memory `ServeRequest` pattern, not
  `SetVirtualHostNameToFolderMapping`).
- **Rationale**: FR-033 ("inside the signed plugin") — the code-signing sweep
  signs the `.spl`, making assets tamper-evident; avoids folder mapping's
  undocumented MIME inference and its exposure of a whole directory; the
  `.spl` grows to ~12 MB, acceptable for a viewer plugin.
  (`viewer-ux-webview2.md` §6.)
- **Alternatives considered**: loose files under `plugins\codeview\web\`
  (unsigned, tamperable, folder-mapping bugs); a single monolithic JS bundle
  (violates FR-036 lazy loading — grammars must load per language).

## D5. Text delivery to the page

- **Decision**: The host decodes the file (D9) and serves the decoded text as
  a UTF-8 resource on the virtual host (`/text`); the page retrieves it via
  `fetch` (allowed by CSP `connect-src 'self'`, answered by the interceptor).
  The text is **never** concatenated into HTML; the page inserts it with
  `textContent` per line. Web messaging (`IsWebMessageEnabled TRUE`) carries
  only small, schema-validated JSON both ways (D7/D8) — never file content.
- **Rationale**: avoids `PostWebMessage` large-payload issues (ARM64 deadlock
  report #4589), keeps the single audited injection sink, works for 20 MB
  plain band. (`viewer-ux-webview2.md` §5.)
- **Alternatives considered**: `AddScriptToExecuteOnDocumentCreated` with the
  text inlined (string-size limits, escaping risk); building final HTML on
  the C++ side mdview-style (loses progressive highlighting and virtual
  rendering).

## D6. Rendering strategy & gates

- **Decision** (**REVISED 2026-08-26 by the T013 spike**, `spike-results.md`
  §3): The page owns a **virtual line list**: only a window around the
  viewport is materialised in the DOM. Plain text renders first (no
  tokenizer involved) — that is what meets the visible-text budget.
  Highlighting is **viewport-first**: the visible window is tokenised
  immediately (~30 ms warm), the rest swept in **100–200 line chunks**
  that resume the previous chunk's grammar state (`getLastGrammarState` +
  `codeToTokens({grammarState})`, verified working), always re-prioritising
  the chunk the viewport is on. The tokenizer runs in a **Web Worker**
  (`worker-src 'self'` already in the CSP) so no chunk can block scrolling
  or find. Gates confirmed: highlight ≤ 1 MB, longest line ≤ 20 000 chars,
  plain ≤ 20 MB, above/binary → decline. Whole-file up-front tokenisation is
  **rejected by measurement** (100 KB = 0.7 s, 1 MB = 7.5 s).
  Honest budget (supersedes the optimistic reading of FR-036/R7.2): text
  appears within the instant budget; the first screen's *colours* land
  ~300 ms later on a cold page (55 ms engine create + 263 ms first
  tokenisation), ~30 ms for later files in the same window.
- **Rationale**: full-DOM highlighting of MB files is the known failure mode
  of hljs/Prism-style usage; Shiki's stateful tokenizer is the only shortlist
  engine that supports correct chunking; GitHub/VS Code precedents for the
  gate values. (`viewer-ux-webview2.md` §1, §7; `web-highlighters.md` §1.5.)
- **Alternatives considered**: whole-file `codeToHtml` (simple, dies on MB
  files); plain-first-then-full-swap (jank at swap; loses scroll anchoring on
  huge files); no highlighting above 100 KB (too conservative vs. SC-001
  expectations).

## D7. Find

- **Decision**: One implementation for all runtimes: JS-side search over the
  in-memory line list (literal, case-fold, whole-word), match positions
  painted via the **CSS Custom Highlight API** (no DOM mutation, works with
  the virtual list), current match scrolled into view; host-drawn find bar
  (mdview `IDD_FIND` pattern) drives it via web messages and receives
  `{current, total}` for "n of N". The native `ICoreWebView2Find` API
  (needs Runtime ≥ 139) is **not used**.
- **Rationale**: uniform behaviour regardless of runtime version (R2.4);
  Custom Highlight API is Baseline 2025 (Evergreen is always newer); native
  Find cannot search unmaterialised virtual lines anyway.
  (`viewer-ux-webview2.md` §2.)
- **Alternatives considered**: native Find API with custom fallback (two code
  paths, and broken by virtualisation); mdview's `<mark>`-regeneration find
  (reloads the document — unacceptable at MB sizes, R2.2).

## D8. Native chrome: status bar, menus, context menu

- **Decision**: Status bar is a **native Win32 child window** below the
  WebView (built-in-viewer style, feature-049 dark pattern): name, size,
  line count, encoding, EOL, language, zoom; Ln/Col of the caret/selection
  fed by a throttled one-way web message. Menu bar reuses mdview's
  `darkmenu`. Context menu: `AreDefaultContextMenusEnabled FALSE`; the page's
  `contextmenu` event posts a web message and the host shows a native popup
  (Copy, Select All, Find, Go to line, Wrap, Language, Encoding, Theme) —
  the documented-behaviour route, since `ContextMenuRequested` with default
  menus disabled is undocumented.
- **Rationale**: constitution VI (native, themed chrome); keeps select-all
  clean (page contains only text); avoids the undocumented interaction.
  (`viewer-ux-webview2.md` §3.)
- **Alternatives considered**: in-page HTML status bar (pollutes select-all,
  needs `user-select` tricks); `ContextMenuRequested` filtering (undocumented
  when default menus are off).

## D9. Encoding stack

- **Decision**: Extend mdview's `MdDetectDecode` into codeview's `intake.cpp`:
  BOMs (UTF-8/UTF-16 LE/BE) → strict UTF-8 validation → **UTF-16-without-BOM
  heuristic** (even/odd NUL distribution) → code-page fallback. Manual
  override menu built from the product's code-table machinery
  (`EnumConversionTables` plugin service — same list as the built-in viewer's
  Coding menu, FR-024), converting via the product's tables (the
  069 lesson: table names, not `MultiByteToWideChar` guesses). Detected
  encoding + EOL style + replacement-character count surface in the status
  bar. Binary rule per `language-detection.md` §5.2 (BOM → text; UTF-16
  no-BOM detected; else NUL → binary; > 0.5 % WHATWG control bytes → binary;
  known signatures), consistent with the built-in viewer except UTF-16 counts
  as text.
- **Alternatives considered**: BOM/UTF-8/ANSI only (mdview's set — fails
  FR-024 parity and `.reg` UTF-16 files); IMultiLanguage2 detection
  (COM legacy, unpredictable).

## D10. Language map & mask generation

- **Decision**: `tools/codeview/gen_langmap.py` (dev-side, pinned Linguist
  revision + `linguist-languages` npm data + two committed overlays: editor
  conventions, Windows/dev tooling) emits (a) `langmap.cpp` — exact-name,
  pattern, multi-dot-suffix, extension tables and heuristic rules (shebang,
  modeline, first-bytes, ambiguity tie-breakers), (b) the `AddViewer` mask
  rows, (c) a JSON manifest for tests. Regeneration is byte-identical from
  pinned inputs (FR-008); a test fails if a mapped language references a
  missing grammar chunk or if masks intersect another shipped plugin's
  registrations (FR-010).
- **Rationale**: Linguist is the maintained authority (833 languages, 1 486
  extensions, 138 disambiguation blocks, MIT); committed outputs keep the
  build offline. (`language-detection.md` §1, §4.)
- **Alternatives considered**: hand-written table (unmaintainable at 780
  masks); VS Code contributions as primary (per-extension licence sprawl);
  ML detection — Magika/guesslang (size, non-determinism; rejected for v1).

## D11. Lift scope: shared WebView2 host in `src/common/webhost/`

- **Decision**: Move from mdview to `src/common/webhost/` (compiled into each
  consuming plugin, like `src/plugins/shared/*.cpp`): the options helper,
  UDF helper, availability gate, the generic host (environment/controller
  creation, lockdown application from a per-plugin settings struct, request
  interception dispatch, accelerator forwarding, `DefaultBackgroundColor`)
  and the keeper (parameterised window-class name + config flag). mdview
  keeps only its glue (virtual host name `mdview.invalid`, its interceptor
  data, its accelerator map, scripts-off settings). WinHTTP remote-image
  code stays mdview-local. `architecture/11-webview2-integration.md` is
  repointed. Contract: [contracts/webview-host-sharing.md](contracts/webview-host-sharing.md).
- **Rationale**: contract §2.5 mandates lifting the helper at the second
  consumer and prefers lifting the keeper; a parameterised host avoids two
  drifting copies of 900 lines of COM plumbing. Behaviour-preserving; mdview
  is retested (its 021/065 guarantees re-verified).
- **Alternatives considered**: lift options helper only, copy the rest
  (contract minimum — rejected: 2× maintenance of security-critical code);
  core-hosted keeper service via plugin API (explicitly deferred by
  `architecture/11` §2.5 — would bump interface 106).

## D12. Shared-profile hygiene

- **Decision**: codeview does not touch process/profile-wide state: no
  `PreferredColorScheme` writes, no `WEBVIEW2_DEFAULT_BACKGROUND_COLOR`
  environment variable, same browser-arguments set, same UDF, default
  profile. Theme is driven explicitly in the page (`data-theme` attribute +
  `color-scheme` CSS; background via `put_DefaultBackgroundColor` + host
  `WM_ERASEBKGND` brush, mdview's belt-and-braces). Guarantees FR-016/R4.3.
- **Alternatives considered**: distinct `ProfileName` for codeview (allowed,
  but unnecessary once nothing profile-wide is written; adds a second cookie/
  cache namespace).

## D13. Identity & naming

- **Decision**: module/directory **codeview** (clarification #4):
  `src/plugins/codeview/`, `CODEVIEW.SPL`, registry key `"CODEVIEW"`,
  translation module `codeview`, `_DOMAINS["codeview"]`. Display name
  (English) **"Code Viewer"**; viewer window title "Code Viewer"; display
  name pinned in `translations/ui-overrides.json` (052 contract). Alphabetical
  position before `dbviewer`/`mdview`/`pictview` is harmless — FR-010
  guarantees an empty mask intersection.

## D14. Viewer-list registration mechanics

- **Decision**: `Connect()` calls `AddViewer(row, FALSE)` once per family
  row, ≤ 8 rows, each ≤ ~200 bytes (hard cap 259 — the `LoadViewers`
  `MAX_PATH` buffer), ordered so the **last** call is the family that should
  land highest (rows insert at index 0). `*.txt;*.log` is its own row
  (clarification #5). Future mask changes go through the
  `CURRENT_CONFIG_VERSION` upgrade protocol (`AddViewer(new, TRUE)` +
  `ForceRemoveViewer`), never by re-registering. "Restore default file
  types" re-issues the plugin's rows via the force path.
- **Rationale**: `codebase-integration.md` §1 — one-shot installation
  semantics give FR-011 for free; the 259-byte row overflow breaks the whole
  user list, so the generator enforces the cap.

## D15. Next/previous file (FR-041)

- **Decision**: Wire `CM_NEXTFILE`/`CM_PREVFILE` (ids already reserved in the
  mdview pattern) via `GetNextFileNameForViewer` with the stored
  `EnumFilesSourceUID/CurrentIndex`. **Early verification task**: confirm the
  host writes long UTF-8 paths safely (documented "at least MAX_PATH" —
  check `src/zip.cpp`/`fileswnb.cpp` call sites, pass a `SAL_MAX_PATH_UTF8`
  buffer). Same window is reused: new intake → new `/text` resource → page
  swaps content without navigation flicker; decline rules show the FR-029
  notice in-window instead of cascading (the cascade exists only at F3 time).
- **Rationale**: clarification #3 made it required; mdview already stores the
  enumeration state for exactly this purpose.

## D16. Theme set & fonts

- **Decision**: Ship 12 themes, all MIT (from `tm-themes`): GitHub Light,
  GitHub Dark, Light+ (VS), Dark+ (VS), One Light, One Dark Pro, Solarized
  Light, Solarized Dark, Catppuccin Latte, Catppuccin Mocha, Gruvbox Dark,
  Nord → 5 light + 7 dark (≥ 3 + 3 per FR-013), plus "follow application
  theme" default mapping Default→GitHub Light, Dark→GitHub Dark (per-polarity
  slots configurable, mdview `EffectiveTheme` pattern). Loader stays generic
  (any VS Code theme JSON).
  **REVISED 2026-08-26 by the T013 spike** (`spike-results.md` §4): Shiki's
  dual-theme CSS-variable mode **cannot be combined** with incremental
  tokenisation — a grammar state records one theme and throws when reused
  for another. Incremental tokenisation wins: the page tokenises with the
  **single active theme** and a scheme change **re-tokenises** (visible
  window first, ~30 ms; remainder by background sweep). Nothing navigates
  and only existing line elements are re-styled, so scroll and selection
  survive and FR-014 still holds. `gruvbox-dark` is shipped as its real
  package name `gruvbox-dark-medium`. Font:
  `"Cascadia Mono", Consolas, "Courier New", monospace`, ligatures off,
  family/size configurable (FR-038); `tab-size` default 4.
- **Rationale**: `web-highlighters.md` §4.2 (licence-verified shortlist);
  `viewer-ux-webview2.md` §4 (dual-theme mechanics, Windows 11 font facts).

## D17. Testing strategy

- **Decision**: `src/plugins/codeview/test/` standalone harness (sftp/mdview
  precedent, not saltests): (1) detection-table tests from the generator's
  JSON manifest (FR-005/006 cases incl. the ambiguous-extension table);
  (2) mask-intersection + row-length checks against other plugins'
  registrations (FR-010/011); (3) licence-audit re-run over the committed
  manifest (D3); (4) intake tests — binary sniff corpus, encoding matrix
  (SC-009), gates; (5) hostile-content corpus as files + a scripted check
  that the served page escapes them (FR-030, corpus-as-data per R6.3);
  (6) manual quickstart scenarios for the WebView-dependent behaviour
  (find, themes, copy fidelity, budgets — clipboard CRLF fidelity is a
  listed manual test). Measurement spike results recorded in
  `spike-results.md` before gates are frozen (R1.6/R7.6).
- **Rationale**: everything deterministic is automated host-side; end-to-end
  WebView behaviour is validated by the quickstart protocol like features
  021/065 did.

## D18. Translations & docs integration

- **Decision**: Follow the 038/039 pipeline: add `_DOMAINS["codeview"]`,
  run the two-stage `.slt` refresh for the 8 enabled languages (network step:
  DeepL + Anthropic keys), pin the display name and theme names in
  `ui-overrides.json` (055 lesson: theme names are identifiers). Update
  `doc/third_party.txt` (Shiki, tm-grammars, tm-themes, per-grammar notices
  from the licence manifest), `CHANGELOG.md` + version bump at ship,
  `CLAUDE.md`/`architecture/02`,`09`,`11` counts and pointers.
- **Rationale**: `codebase-integration.md` §4.6/§4.10; memory: new UI strings
  fail `build.cmd full` for all languages until the `.slt` refresh runs —
  the refresh is sequenced before the first full build with the new module's
  strings.

---

**All Technical Context items resolved; no NEEDS CLARIFICATION remains.**

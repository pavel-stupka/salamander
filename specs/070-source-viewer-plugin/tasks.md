# Tasks: Source & Configuration File Viewer (codeview)

> **Status as of 2026-08-27 — read this before trusting a checkbox.** The
> plugin has been built, run and reviewed since these boxes were last touched,
> so several are stale in both directions. What is actually true:
>
> - **T055 (translations) is DONE** — ticked below. 8 languages × 98 entries,
>   0 validation failures; recorded in `fix-log.md`.
> - **T034 is IMPLEMENTED WITH A DEVIATION**, so it stays unticked: the
>   first-open hint and "Restore Default File Types" both work, but the hint is
>   a modal message box, not the "dismissable, non-modal" notice the task asks
>   for. Decide whether the deviation is acceptable before ticking it.
> - **T058 (source hygiene) is HALF DONE**: `tools/check_encoding.py` is
>   `TOTAL: 0` and comments are English, but clang-format has not been run over
>   the plugin.
> - **T006/T007 (mdview onto the shared host) are untouched** and remain the
>   first thing to finish — the product still carries two copies of the
>   WebView2 host, and only the shared one has the liveness guard added by the
>   stabilization review.
> - **Everything else unticked in Phase 10 needs the GUI**, which no agent can
>   drive. The concrete list a human should run is
>   `stabilization-review.md` §8 (10 scenarios), not the task text here.
> - **37 defects were found and 35 fixed after these tasks were written** —
>   including three features that were ticked as implemented but did not work
>   at all (Copy/Select All, Word Wrap, Show Whitespace). A ticked box in this
>   file means "code was written", not "verified to work". See
>   `stabilization-review.md`.

**Input**: Design documents from `/specs/070-source-viewer-plugin/`
**Prerequisites**: plan.md, spec.md (7 user stories), research.md (D1–D18), data-model.md, contracts/ (4), quickstart.md

**Tests**: Included — the spec itself mandates automated checks (FR-008 grammar check, FR-010 intersection test, FR-030/R6.3 hostile corpus, D3 licence audit, SC-005/007/009 matrices); they live in the standalone harness `src/plugins/codeview/test/` (D17), not in saltests.

**Organization**: Phases 3–9 map 1:1 to spec user stories US1–US7 in priority order (P1, P1, P1, P2, P2, P2, P3).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task)
- **[Story]**: US1–US7 per spec.md
- Paths are repository-relative

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: the plugin skeleton exists, builds empty, and the dev-side tooling has a pinned home

- [X] T001 Create `src/plugins/codeview/` skeleton per plan.md structure (codeview.cpp/.h/.rh/.rh2/.rc/.rc2/.def, versinfo.rh2 from mdview with changed guard+DESCRIPTION+INTERNAL, precomp.h/.cpp, res/plugico.bmp) and add `codeview=on` to `plugins.cfg` (alphabetical position)
- [X] T002 Add build wiring: `src/plugins/codeview/vcxproj/codeview.vcxproj` + `codeview.props` + `lang_codeview.vcxproj` + `lang_codeview.props` (copied from the mdview four per codebase-integration §4.3: new GUIDs, RootNamespace, import order, WINVER 0x0A00, WebView2 include/lib — no winhttp) and register both projects + 10 ProjectConfigurationPlatforms lines each in `src/vcxproj/salamand.sln`; verify `build.cmd` builds the empty plugin
- [X] T003 [P] Create language-module skeleton `src/plugins/codeview/lang/lang.rc`, `lang.rc2`, `lang.rh` with the initial string/dialog tables (plugin name "Code Viewer", description, menu/dialog placeholders; unique static ids via statics.rh2 pattern)
- [X] T004 [P] Create `tools/codeview/README.md` + `pins.json` recording the pinned versions (Node, esbuild, `@shikijs/core`, `tm-grammars`, `tm-themes`, `linguist-languages`, Linguist revision) and the regeneration commands (plan: dev-side only, never run by the build)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the shared WebView2 host lift, the generated data, asset serving, a window that renders the page, and the measurement spike that freezes the gates

**⚠️ CRITICAL**: no user-story work until this phase completes; T013 is a decision gate

- [X] T005 Lift the WebView2 host from `src/plugins/mdview/webview.{h,cpp}` into `src/common/webhost/webhost.{h,cpp}` + `webkeeper.{h,cpp}` parameterised by `TcWebHostConfig` per contracts/webview-host-sharing.md §1–2 (options helper, UDF, availability gate, create/lockdown/interception/accelerator plumbing, keeper with configurable class name; add the newer-interface lockdowns of §2 to the shared routine; WRL confinement pragma pattern)
- [ ] T006 Convert mdview to consume `src/common/webhost/` (its webview.cpp shrinks to glue: `mdview.invalid` config, scripts OFF, image serving, link gate; keeper class `TandemMdKeeperWnd` kept; `src/plugins/mdview/vcxproj/mdview.vcxproj` gains the shared sources) — behaviour-preserving, zero user-visible change
- [ ] T007 mdview regression pass per contracts/webview-host-sharing.md §4: 021 lockdown re-verification (hostile corpus, scripts stay OFF), 065 keeper scenarios (first-view warm, close-all-then-open, crash re-arm, KeepReady toggle), zoom/find/schemes smoke — record results in `specs/070-source-viewer-plugin/mdview-regression.md`
- [X] T008 [P] Implement `tools/codeview/build_web.py`: pinned npm install → esbuild → committed `src/plugins/codeview/web/` (viewer shell placeholders, `shiki/` engine + per-language ESM chunks, `themes/` 12 MIT theme JSONs per D16) with the **automated licence audit** (GPL-3.0/GNU/unresolved ⇒ fail; D3 exclusion list) emitting the committed licence manifest
- [X] T009 [P] Implement `tools/codeview/gen_langmap.py`: pinned Linguist `languages.yml`+`heuristics.yml` + `linguist-languages` + `overlay-editor.json` + `overlay-windows.json` → committed `src/plugins/codeview/langmap.cpp` (+`langmap.h`), the ≤ 8 `AddViewer` rows (each ≤ 200 bytes; `*.txt;*.log` PLAINTEXT family; exclusions per contracts/claimed-types.md §2) and `test/langmap-manifest.json`; byte-identical regeneration (FR-008)
- [X] T010 Implement `src/plugins/codeview/config.cpp`: registry load/save/clamp for the full data-model §6 table + minimal Configuration dialog (DIALOGEX/DS_SHELLFONT, feature-049 dark pattern) with keep-ready checkbox — fields grow in later stories
- [X] T011 Implement asset serving in `src/plugins/codeview/webglue.cpp`: RCDATA embedding of `web/` (generated `.rc` include), in-memory ServeRequest for the allow-list URLs with explicit Content-Type + the CSP response header on `viewer.html`, 403 everything else, debug request log (contracts/rendering-lockdown.md §2–3)
- [X] T012 Implement the viewer window shell in `src/plugins/codeview/viewer.{h,cpp}`: CViewerThread thread-per-window + lock handshake (mdview pattern), CViewerWindow frame, CTcWebHost creation with codeview config (scripts ON, web messages ON, `codeview.invalid`), WM_ERASEBKGND brush, navigation to `viewer.html?v=1` renders a static page
- [X] T013 **Measurement spike (GATE)**: implement a throwaway page exercising Shiki + virtual list on 10 KB/100 KB/1 MB/5 MB/single-line-2 MB inputs; record time-to-first-text, time-to-highlighted, renderer memory, warm attach time, interceptor asset HTTP/code-cache behaviour, theme-switch recalc cost in `specs/070-source-viewer-plugin/spike-results.md`; freeze gate defaults and confirm virtual-line-list vs CodeMirror-6 fallback (D6, R1.6/R7.6)
- [X] T014 Implement plugin entry + registration in `src/plugins/codeview/codeview.cpp`: SalamanderPluginGetReqVer/Entry, SetBasicPluginData("Code Viewer", FUNCTION_VIEWER|CONFIGURATION|LOADSAVECONFIGURATION, …, "CODEVIEW"), Connect() issuing the generated AddViewer rows in reverse priority order (PLAINTEXT first) + plugin icon; CPluginInterface Release/Event skeletons

**Checkpoint**: `build.cmd full` ships a codeview that opens claimed files into a themed empty page

---

## Phase 3: User Story 1 — Syntax-highlighted view on F3 (Priority: P1) 🎯 MVP

**Goal**: F3 on a source file opens a read-only window with correct token colouring and line numbers, first screen within budget

**Independent Test**: quickstart scenario 1 — F3 on ten well-known languages; colours + gutter; Esc closes; ≤ 0.3 s warm

- [X] T015 [US1] Implement `src/plugins/codeview/intake.{h,cpp}` v1: read file, decode (BOM UTF-8/UTF-16 LE/BE, strict UTF-8, CP_ACP fallback — mdview MdDetectDecode derivative), build line index, expose decoded text for the `/text` resource; record encoding/EOL/invalid-count in the session (data-model §5)
- [X] T016 [US1] Wire name→language lookup in `intake.cpp` from `langmap.cpp` tables (exact name → pattern → multi-dot suffix → extension; content heuristics deferred to US4); resolve grammar chunk or `plain` (FR-003)
- [X] T017 [US1] Implement `src/plugins/codeview/web/viewer.js` core: fetch `/text`, virtual line list, plain-first render via textContent, progressive line-stateful Shiki tokenisation (dual-theme CSS variables, `defaultColor:false`), lazy `import()` of the language chunk, line-number gutter (`user-select:none`), `tokenizeMaxLineLength`/`tokenizeTimeLimit` guards (D6)
- [X] T018 [US1] Implement the init/ready/rendered/highlightDone message flow in `webglue.cpp` + `viewer.js` per contracts/host-page-interface.md §2–3 with schema validation host-side; `rendered.firstPaintMs` telemetry
- [ ] T019 [US1] Complete the US1 window contract in `viewer.cpp`: read-only guarantee, Esc/Alt+F4 close, window title (name + zoom later), remembered placement, plain-band rendering for `grammar=none` files with language name surfaced; run quickstart scenario 1 and record it

**Checkpoint**: MVP — highlighted viewing works end to end for the generated claim list

---

## Phase 4: User Story 2 — Light and dark colour themes (Priority: P1)

**Goal**: 12 schemes (5 light/7 dark) + follow-application default; instant switch; no flash; no effect on mdview

**Independent Test**: quickstart scenario 2

- [X] T020 [US2] Implement the scheme model in `config.cpp` + `webglue.cpp`: activeScheme/follow/schemeLight/schemeDark persistence, chromeColors + DefaultBackgroundColor derivation from theme JSON (data-model §4)
- [X] T021 [US2] Implement theme UI in `viewer.cpp`: View ▸ Color Scheme radio list (darkmenu), "Follow application theme", F9/Shift+F9 cycle, `setTheme` message → `data-theme` flip in `viewer.js` keeping scroll+selection (FR-014); persist on change
- [X] T022 [US2] No-flash pipeline: scheme background via put_DefaultBackgroundColor before visibility + WM_ERASEBKGND brush + `color-scheme` CSS per polarity (dark scrollbars) in `viewer.css` (FR-015); verify with frame capture on dark scheme
- [X] T023 [US2] Follow-application mode in `viewer.cpp`: EffectiveTheme pattern — IsDarkThemeActive() precedence, per-polarity slots; default follow=ON mapping Default→github-light, Dark→github-dark (D16)
- [ ] T024 [US2] Isolation verification: open mdview + codeview side by side, switch codeview schemes, assert mdview rendering and profile state untouched (FR-016; no PreferredColorScheme/env-var writes — D12); add the check to the quickstart record

**Checkpoint**: both P1 visual stories done — the viewer looks finished

---

## Phase 5: User Story 3 — Safe viewing of untrusted files (Priority: P1)

**Goal**: hostile content displays literally; zero network; zero browser UI — proven by tests, not review

**Independent Test**: quickstart scenario 3 + harness corpus run

- [X] T025 [US3] Finalize the scripts-ON lockdown via the shared routine with codeview's TcWebHostConfig and add the debug read-back assertion naming any mismatched setting (FR-033; contracts/rendering-lockdown.md §1)
- [X] T026 [P] [US3] Author the hostile-content corpus in `src/plugins/codeview/test/corpus/hostile/` per data-model §8 (script/markup injection, entities, javascript:/data: URLs, bidi overrides, U+2028/9, lone surrogates, oversized lines, escape-hatch probes: window.open/location/download/form/srcdoc/WebSocket/beacon)
- [ ] T027 [US3] Enforce and test the injection rules in `viewer.js`: file text only via textContent/createTextNode, highlighter consumed as codeToTokens arrays (no innerHTML sink); harness check `test/` that renders each corpus file through the real page and asserts literal display + unchanged title (contracts/rendering-lockdown.md §4–5)
- [ ] T028 [US3] Request-log verification in `test/`: corpus run asserts only allow-listed URLs answered, everything else 403, zero external requests (FR-031, SC-004)
- [ ] T029 [US3] Key sweep + UI suppression test: F12/Ctrl+P/Ctrl+S/F5/Ctrl+Shift+C-I/F7/Alt+arrows produce viewer actions or nothing; no dialog/window/download/print ever (FR-032, FR-025 browser keys; rendering-lockdown §5 items 4–5)

**Checkpoint**: all three P1 stories complete — shippable core

---

## Phase 6: User Story 4 — Hundreds of formats, correctly identified (Priority: P2)

**Goal**: full detection pipeline + claim-policy guarantees, all table-driven and tested

**Independent Test**: quickstart scenario 4 + harness detection suite

- [X] T030 [US4] Implement content heuristics in `intake.cpp`: shebang (env forms, versioned interpreters), Emacs/Vim modelines (first/last 5 lines), first-byte signatures, ambiguity tie-breaker rules from `langmap.cpp` (FR-005/006; data-model §2)
- [X] T031 [US4] Implement the language picker in `viewer.cpp` (menu, searchable by name/alias) + `setLanguage` override message → re-tokenise, not persisted (FR-007)
- [ ] T032 [P] [US4] Harness detection-table suite in `src/plugins/codeview/test/`: drive every langmap-manifest case (exact names, multi-dot, ambiguous-extension table incl. `.h/.m/.pl/.v/.ts/.sql`, shebangs, modelines, signatures) against `intake.cpp` (SC-005)
- [X] T033 [P] [US4] Harness claim-policy suite: mask-intersection = ∅ against other shipped plugins' AddViewer strings, row count ≤ 8, row length ≤ 200 B, and the upgrade scenario (seeded user rows survive; deleted row stays deleted) per contracts/claimed-types.md §6 (FR-010/011, SC-007)
- [ ] T034 [US4] Implement the first-open hint (dismissable, non-modal, once: Alt+F3 + Options ▸ Viewers pointer) in `viewer.cpp` and "Restore default file types" in `config.cpp` via the force re-registration path (FR-012)
- [X] T035 [US4] Add the licence-audit re-run as a harness test over the committed manifest + grammar-chunk existence check (a mapped language without its chunk fails naming the language) (FR-008, D3)

---

## Phase 7: User Story 5 — Large, odd and binary files degrade gracefully (Priority: P2)

**Goal**: bands + decline cascade exactly per spec; nothing ever worse than today

**Independent Test**: quickstart scenario 5 + harness decline matrix

- [X] T036 [US5] Implement the full binary/text sniff in `intake.cpp` (≤ 8 KB: BOM→text; UTF-16-no-BOM even/odd-NUL detector→text; NUL→binary; > 0.5 % WHATWG control bytes→binary; known signatures e.g. MPEG-TS stride) and `CanViewFile` decline: binary, > viewer limit, unreadable, unpaired-surrogate names (FR-027; contracts/claimed-types.md §5)
- [X] T037 [US5] Implement the band logic: highlight ≤ HighlightLimitKB & maxLine ≤ MaxLineLength, plain band ≤ ViewerLimitMB with the one-line reason notice (host-localised string via message), config changes effective next view without restart (FR-026; gates = spike-frozen defaults)
- [X] T038 [US5] Single-line/minified handling in `viewer.js`: horizontal scroll with `white-space:pre`, no tokenisation above the line gate, wrap-on-huge-line within seconds, never unresponsive (FR-028)
- [X] T039 [US5] Implement the post-open binary notice with "Open in built-in viewer" action in `viewer.cpp` (file replaced on disk / next-prev navigation) (FR-029)
- [ ] T040 [P] [US5] Harness decline-matrix + gates suite in `test/`: 2 GB-sparse `.sql`, `.ts` MPEG fixture, `.h` WinHelp fixture, lone-surrogate name, empty/BOM-only/mixed-EOL files, gate boundary sizes; classification ≤ 50 ms (SC-006; edge cases)
- [X] T041 [US5] Engine-unavailable fallback (main-thread ViewFileInPluginViewer hand-off) + in-window ProcessFailed recovery/re-open behaviour, silent keeper failures (FR-037; mdview pattern)

---

## Phase 8: User Story 6 — A complete read-only viewer (Priority: P2)

**Goal**: daily-tool parity: find, navigation, copy fidelity, status bar, encodings, keys

**Independent Test**: quickstart scenario 6 + 7

- [X] T042 [US6] Implement find: host find bar (IDD_FIND-derived, dark-aware) in `viewer.cpp` + JS search over the line list with CSS Custom Highlight API marks, current-match scroll/distinct style, `findResult {current,total}` n-of-N, F3/Shift+F3/F6 next/prev, case/whole-word; never reloads, 1 MB first match ≤ 200 ms (FR-017; D7)
- [X] T043 [US6] Implement wrap toggle (F2/Ctrl+W, `pre`↔`pre-wrap`), line-numbers toggle, and go-to-line dialog (Ctrl+G, "line[:column]", clamp + centre + transient mark) in `viewer.cpp` + `viewer.js` (FR-018/019)
- [X] T044 [US6] Implement zoom parity: engine ZoomFactor 50–300 %, Ctrl+wheel/±/0, ZoomFactorChanged sync, persisted, % in title (FR-020; mdview pattern)
- [X] T045 [US6] Copy fidelity + rendering options: selection/select-all covers document text only, gutter excluded; show-whitespace paint-only mode; `tab-size` from config; verify clipboard CRLF/tabs/trailing-spaces byte-exactness (FR-021; SC-008 — manual matrix listed in quickstart)
- [X] T046 [US6] Implement the native status bar child window in `viewer.cpp` (name, size, lines, encoding, EOL, language, zoom + Ln/Col from throttled `caret` messages), feature-049 dark pattern (FR-022; D8)
- [X] T047 [US6] Implement the native context menu (page `contextMenu` message → TrackPopupMenu with Copy/Select All/Find/Go to line/Wrap/Language/Encoding/Theme), darkmenu-styled; default context menus stay off (FR-023; D8)
- [X] T048 [US6] Implement encoding override: Coding menu from the product code-table machinery (EnumConversionTables — same list as the built-in viewer), F8/Shift+F8, re-decode + `/text` version bump + reload, status-bar update, replacement-character count (FR-024; D9)
- [X] T049 [US6] Wire the full keyboard parity map in the accelerator table + AcceleratorKeyPressed forwarding per contracts/host-page-interface.md §5; document the deliberate no-ops (FR-025)
- [X] T050 [US6] Implement next/previous file (FR-041): **first** verify GetNextFileNameForViewer long-path behaviour at its call sites (`src/zip.cpp`, `src/fileswnb.cpp`) and size the buffer accordingly, then CM_NEXTFILE/CM_PREVFILE via stored EnumFilesSourceUID/CurrentIndex, `swapText` flow (same window, re-intake, no navigation flicker), decline → FR-029 notice (D15)
- [ ] T051 [P] [US6] Author `test/corpus/encodings/` (UTF-8±BOM, UTF-16 LE/BE ±BOM incl. a no-BOM `.reg`, CP1250/ISO-8859-2/CP852 Czech fixtures, invalid-sequence file) + harness decode assertions (SC-009)

---

## Phase 9: User Story 7 — Instant display, zero cost before first use (Priority: P3)

**Goal**: 065 parity: keeper at first use, session-long warmth, zero footprint before use

**Independent Test**: quickstart scenario 9 (+ 10)

- [X] T052 [US7] Arm the codeview keeper (`TandemCvKeeperWnd`) at first actual view via `src/common/webhost/webkeeper`, disarm on KeepReady toggle-off / Release / exit, silent failures, re-arm after process death; KeepReady checkbox already in config (FR-034; contracts/webview-host-sharing.md §3.3)
- [ ] T053 [US7] Verify lazy loading: only the in-use grammar chunk + theme JSON are ever requested (assert from the debug request log across a multi-language session); no re-parse of the whole highlighter per open (FR-036)
- [ ] T054 [US7] Budget + zero-cost verification: trace-point measurements vs the 065 protocol (warm ≤ 100 KB budget, cold ≤ mdview cold, no-use session start-up/idle identical, bounded idle footprint), recorded in `specs/070-source-viewer-plugin/perf-results.md` (SC-003, SC-010, FR-036)

---

## Phase 10: Polish & Cross-Cutting Concerns

- [X] T055 [P] Translations: add `_DOMAINS["codeview"]` to `tools/translate/uicontext.py`, pin display name + scheme names in `translations/ui-overrides.json`, run the two-stage refresh (`build_langs.cmd --export-templates` → `python -m translate.merge --module codeview` → `build.cmd full`) producing `translations/<lang>/codeview.slt` for the 8 enabled languages (FR-039; D18 — sequence before the first full build with final strings)
- [X] T056 [P] Docs: `doc/third_party.txt` (Shiki, tm-grammars, tm-themes + per-grammar notices from the licence manifest), repoint `architecture/11-webview2-integration.md` to `src/common/webhost/`, update `CLAUDE.md` and `architecture/02`/`09` plugin counts
- [ ] T057 Full quickstart pass: all 10 scenarios on Debug + Release x64, results recorded in `specs/070-source-viewer-plugin/quickstart-results.md`; harness green (`src/plugins/codeview/test/build_and_run.cmd`)
- [ ] T058 [P] Source hygiene: clang-format, UTF-8-BOM on all new sources, English comments, `tools/check_encoding.py` still TOTAL: 0 (plugin dir excluded but core/webhost changes are scanned)
- [ ] T059 Ship gate: `CHANGELOG.md` entry (Added — Code Viewer plugin…, truthful about the plain-band and excluded-grammar limits) + version/build bump (`spl_vers.h` MINORB+BUILDNUMBER, `tandemcommander.iss` MyAppVersion, CLAUDE.md line) in the same change (constitution Release Documentation)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 → Phase 2 → Phase 3 (US1)**: strictly sequential; T013 (spike) gates D6 decisions; T007 gates the lift.
- **US1 is the hub**: Phases 4–9 all need the rendering pipeline of US1.
- **After US1**: US2 (themes), US3 (security), US4 (detection), US5 (degradation) are mutually independent and can proceed in parallel (different files, see below). US6 touches `viewer.cpp` broadly — coordinate or serialize with US2/US4's menu tasks. US7 last (needs stable open path for honest measurements).
- **Phase 10** after all stories; T055 whenever strings are final; T059 strictly last.

### User Story Dependencies

| Story | Depends on | Independent test remains valid |
|---|---|---|
| US1 | Foundational only | yes (MVP) |
| US2 | US1 | yes |
| US3 | US1 (page exists) | yes |
| US4 | US1 (T016 lookup) | yes |
| US5 | US1 (intake) | yes |
| US6 | US1 (+ menu slots from US2 if parallel — coordinate `viewer.cpp`) | yes |
| US7 | US1 (open path) | yes |

### Parallel Opportunities

- Phase 1: T003 ∥ T004 (after T001–T002).
- Phase 2: T008 ∥ T009 (tooling) while T005–T007 (lift) proceed; T010 ∥ T011 after T005.
- After US1: **US2 (T020–T024) ∥ US3 (T025–T029) ∥ US5 (T036–T041)** — disjoint files (theme/config vs test+lockdown vs intake); US4 heuristics T030 ∥ US5 sniff T036 both touch `intake.cpp` — serialize those two tasks, the rest of US4 is parallel.
- Corpus/test authoring is always parallel: T026, T032, T033, T040, T051.

### Parallel Example: after US1 checkpoint

```text
Track A (visual):    T020 → T021 → T022 → T023 → T024
Track B (security):  T025 → T027 → T028 → T029   (T026 corpus in parallel)
Track C (intake):    T036 → T037 → T038 → T039 → T041   (T040 in parallel)
Track D (tests):     T032, T033, T035 as soon as langmap + registration exist
```

## Implementation Strategy

- **MVP first**: Phases 1–3 (T001–T019) = a working highlighted F3 viewer for the full claim list with one default light+dark pair via follow-app default. Demoable and independently testable (quickstart 1).
- **Ship-safe core**: + US2 + US3 (all P1) — visually complete and security-proven.
- **Incremental**: US4 → US5 → US6 each an independently testable increment; US7 + Polish close the release.
- **Gates to respect**: T013 spike freezes size/gate defaults before US5 hardcodes them; T007 must be green before any codeview work builds on the lifted host; T050 starts with the long-path API verification; T059 only in the shipping change.

## Notes

- Format check: every task has checkbox + ID; [P] only where files are disjoint; [US#] labels only in Phases 3–9; file paths in every task.
- Harness (`src/plugins/codeview/test/`) is standalone (sftp/mdview precedent); saltests untouched.
- The three P1 stories intentionally come before any breadth work: a small, safe, beautiful viewer beats a broad, unproven one.

---

## Implementation status (2026-08-26)

`build.cmd` is **green**: `codeview.spl` (12.9 MB, assets embedded) links and
mdview still builds. `python src/plugins/codeview/test/check_data.py` passes
all 21 data checks.

**Checked-off tasks are code-complete and compile**; the ones that need a
running GUI to be *verified* (rendering, find, themes, copy fidelity, budgets)
are code-complete but unverified — the quickstart scenarios are the acceptance
step and no automated substitute exists. Nothing below is claimed as tested.

Still open, with the reason, in
[REMAINING-WORK.md](REMAINING-WORK.md):

- **T006/T007** — converting mdview onto the shared host, and its regression
  pass. The shared host exists and codeview uses it; mdview still has its own
  copy. Deliberately not done in this pass: it is a change to a shipping
  feature whose verification is manual, and it belongs in its own reviewable
  commit (constitution III).
- **T019, T024, T027, T028, T029, T032, T034, T040, T051, T053, T054, T055,
  T057, T058, T059** — GUI verification, the runtime corpus checks, the
  translation refresh (needs DeepL + Anthropic keys), and the ship gate.

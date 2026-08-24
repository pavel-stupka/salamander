# Tasks: Encoding Regression Review and Stabilization

**Input**: Design documents from `/specs/068-encoding-regression-review/`
**Prerequisites**: plan.md, spec.md (US1–US4, FR-001…FR-015, Clarifications Q1–Q4),
research.md (R1–R11), data-model.md, contracts/encoding-contract-checklist.md
(DC-01…DC-20, contracts B1–B12, ledger L01–L89), quickstart.md (G1–G7, timing
method, sweep W1–W20)

**Tests**: Included by construction — FR-010 requires a fail-before/pass-after
check for every fix and SC-008 a named durable check per confirmed defect
class. Guard rules are drafted **before** the fixes (report-only), proven on
the pre-fix tree, and promoted to strict once the tree is clean.

**Organization**: US1 = the audit (inventory, sibling sweeps, contract
compliance, ledger re-examination) — the MVP; US2 = independently verified
fixes with independent regression review; US3 = gates and the on-screen
sweep; US4 = the report and the durable guards. Review perspectives P1–P7 run
as parallel read-only subagents (research R2/R4); the main context never
verdicts its own finding nor accepts its own fix (R5).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an unfinished task)
- **[Story]**: US1–US4 from spec.md

## Path Conventions

Core under review: `src/*.cpp`, `src/*.h`, `src/common/**` (not `src/common/dep/**`);
plugin boundary: `src/zip.cpp`, `src/plugins1/2/3.cpp`, `src/packers.cpp`,
`src/plugins/shared/*.h`, enabled plugins' boundary sites only. Guards:
`tools/check_encoding.py`, `src/saltests/saltests.cpp`. Feature records:
`specs/068-encoding-regression-review/` (`F` = that directory below).

---

## Phase 1: Setup (Baseline & fixtures)

**Purpose**: capture the green pre-change baseline that every byte-identity,
count and timing comparison refers to, and stage the fixtures.

- [X] T001 Baseline: run `build.cmd full` (Debug x64) then `%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\saltests\saltests.exe` and `python tools\check_encoding.py --strict`; record the saltests count (expected 1229/0), the guard result (expected TOTAL 0), the MSBuild warning lines for later "no new warnings" comparison, and the baseline commit hash in a new `F/review-report.md` created with the section skeleton of research.md R10 (Scope & method · Coverage · Inventory summary · Defect-class sweep · Contract compliance · Findings · Fixes · Deferred items · Gates · Sweep · Verdict).
- [X] T002 [P] Pre-fix reference binary: `build.cmd full release` on the baseline commit and copy `%OPENSAL_BUILD_DIR%tandemcommander\Release_x64\` to `%OPENSAL_BUILD_DIR%tandemcommander\Release_x64_prefix\` (used by G6 "before" timing runs, G7 English side-by-side, plugin-facing byte comparisons); note the path in `F/review-report.md` Scope.
- [X] T003 [P] Fixtures per quickstart.md Prerequisites: `powershell -ExecutionPolicy Bypass -File tools\create-test-fixtures.ps1 -Perf` (Unicode names + the 100,000-file `%TEMP%\salamander-test\perf`), the Czech sweep folder `D:\Zkouška\Můj disk\` (`příloha.txt`, `žluťoučký kůň.docx`, `1 000 000.pdf` with a real NBSP), the Hungarian folder `D:\Zkouška\Árvíztűrő tükörfúrógép\bájt.txt`, and one unpaired-surrogate file (one-liner from `specs/066-fix-surrogate-filenames/quickstart.md`); record the paths in `F/review-report.md` Sweep.
- [X] T004 [P] Locate or build the Trace Server for G5: find `tserver.exe` under `%OPENSAL_BUILD_DIR%` or build `src\vcxproj\tserver\tserver.vcxproj` (Debug x64) with MSBuild; record its path in `F/quickstart.md` G5 row.
  *(Done 2026-08-24 — outcome: `tserver` does NOT build (pre-existing `UNICODE` collision in `src/common/handles.h`, both x64 and Win32; review-report D01). G5 waived to the feature-060 observable bar; quickstart G5 row updated.)*
- [X] T005 [P] Create `F/inventory.md` with the skeleton: one section per boundary B1–B8 (table columns = data-model Site fields: ID, Location, Pattern, Data, Classification, Evidence, Perspective, DC, Finding), a "Cross-cutting machinery" section (P3), the "Defect-class sweep" table DC-01…DC-20 (status pending), the "Contract compliance" table B1–B12 (one row per obligation bullet from `F/contracts/encoding-contract-checklist.md` Part B, verdict pending), and the "Ledger re-examination" table L01–L89 (disposition pending) — row IDs copied verbatim from the contract checklist.

---

## Phase 2: Foundational (work queues, charters, draft guards)

**Purpose**: the mechanical candidate lists the perspectives classify, the
written charters that make the perspectives independent, and the draft guard
rules whose fail-before proof US2 depends on.

**⚠️ CRITICAL**: No user-story work begins before this phase completes.

- [X] T006 Write `F/charters.md`: one section per perspective P1–P7 with boundaries, defect classes, the file list (research.md R4 + the top files of research R1), seeded questions (R7), the Finding output shape (data-model), and the rules (raise only — no verdicts; a failure scenario with surface + locale/UI language + what the user sees is mandatory; classification needs one evidence line; group sites per pattern-in-function but list every location); plus a **Verifier** section (refute-first: CONFIRMED with `file:line` + reproduced data path, or REFUTED with the evidence the value cannot be UTF-8 / cannot reach the sink / is ASCII in all 8 shipped translations) and a **Regression reviewer** section (enumerate consumers by grep; per-surface verdict; English/ASCII byte-identity; plugin-facing bytes; 066/067 quickstart scenarios touched; timing record present for per-item paths; ACCEPTED/REJECTED).
- [X] T007 [P] Generate the Tier-1 work queues into `F/candidates/` (one `file:line: text` list per file, ripgrep over the core scope of research R1, comments excluded where feasible): `dc01-ansi-fs-shell-process-registry.txt` (the un-suffixed/`A` name-taking calls of the triage lists), `dc02-cp-acp.txt` (`MultiByteToWideChar(CP_ACP` / `WideCharToMultiByte(CP_ACP`), `dc03-05-19-loadstr-compositions.txt` (printf-family with `LoadStr(` format; `ExpandPluralString(LoadStr(`), `dc06-ansi-ui-sinks.txt` (A UI calls + `WM_SETTEXT`/`CB_*STRING`/`LB_*STRING`/`LVITEM`/`TVITEM`/`SB_SETTEXT` tokens), `dc08-tooltips.txt`, `dc13-strict-probe.txt` (`MB_ERR_INVALID_CHARS` outside `src/common/salunicode.cpp`), `dc15-signed-char.txt` (`Name[…] <= ' '`-style and `>= 32` on name buffers; `IsAlpha`/`IsNotAlphaNorNum` consumers), `dc18-missed-twins.txt` (every `IDS_*` id used with `LoadStrU8(` that still appears with `LoadStr(`; every control/resource with both a `Sal*U8` and an `A` twin), `converters.txt` (all `SalU8ToW*`/`SalWToU8*`/`SalLegacyToU8Alloc`/`SalU8ToWDisplay*` call sites), `suppressions.txt` (the 4 `encoding-check: allow` lines), `registry-old-wrappers.txt` (`SalRegQueryValue(Ex)` uses). Record the line counts in `F/review-report.md` Coverage.
- [X] T008 [P] Draft the new guard rules in `tools/check_encoding.py` **report-only**: add a `DRAFT_RULES` set and a `--draft` flag (strict runs and `build.cmd` unaffected) implementing research R8 — `ansi-api-on-utf8-path`, `cp-acp-utf8-source` (A→W direction), `ansi-tooltip-handler`, `strict-probe-rejects-wtf8`, `lossy-lenient-at-intake`, `signed-char-name-byte`, `missed-twin`; document each in the module docstring in the existing style; run `python tools\check_encoding.py --draft --format list > F/candidates/guard-draft.txt`; confirm `python tools\check_encoding.py --strict` still reports TOTAL 0.
- [X] T009 [P] Add `static void TestEncodingReview068()` to `src/saltests/saltests.cpp` (registered in `main()` after `TestWtf8FileOps`) with a first property block pinning the converter behaviors the review relies on (`SalU8ToW` returns 0 for both a too-small buffer and invalid input — DC-20 as-is; `SalLegacyToU8Alloc` keeps WTF-8 bytes; `SalU8ToWDisplay` never fails); `build.cmd`, run saltests, record the new count as the working baseline in `F/review-report.md`.
- [X] T010 [P] Write `F/delta-manifest.md`: `git diff --stat v0.1.4..HEAD -- src tools` plus `git diff --stat v0.1.1..v0.1.2 -- src` (the 052–055 delta whose encoding perspective never reported — ledger L87), listing every code file with insertion counts — P6's line-level input.

**Checkpoint**: queues, charters, draft guards and baselines in place.

---

## Phase 3: User Story 1 — Systematic audit of everything that handles text encoding (Priority: P1) 🎯 MVP

**Goal**: every encoding boundary inventoried and classified with evidence,
every defect class sibling-swept, every contract checked, every ledger row
re-examined; a findings pool with mandatory failure scenarios.

**Independent Test**: `F/inventory.md` has every boundary B1–B8 populated,
every DC row at *complete*, every contract obligation with a verdict, every
L row with a re-examination note; every Tier-1 candidate line is accounted
for (T018).

### Implementation for User Story 1

- [X] T011 [P] [US1] Run perspective **P1** (file-system, shell & launch boundary) as a fresh read-only subagent with `F/charters.md` §P1, `F/candidates/dc01-*.txt` + `dc02-cp-acp.txt`, seeds C-a…C-e and ledger L01–L05, L08–L11; it appends Site rows to `F/inventory.md` §B1/§B2/§B5 (Tier-2 groups with every location) and writes its findings to `F/findings/P1.md` in the data-model Finding shape with a coverage list of files read.
- [X] T012 [P] [US1] Run **P2** (UI text sinks & composition): `dc03-05-19-*.txt`, `dc06-*.txt`, `dc08-tooltips.txt`, `dc18-missed-twins.txt`, `suppressions.txt`, ledger L06, L12–L16, L30 → `F/inventory.md` §B3/§B4 + `F/findings/P2.md`.
- [X] T013 [P] [US1] Run **P3** (converter & measurement machinery): `converters.txt`, `dc13-strict-probe.txt`, `dc15-signed-char.txt`, seeds C-f, C-g, ledger L07, L21, L23–L28; verdicts for the obligations of contracts B2, B3, B4, B10, B11 → `F/inventory.md` §Cross-cutting + Contract compliance rows + `F/findings/P3.md`.
- [X] T014 [P] [US1] Run **P4** (configuration, clipboard & external channels): the raw registry calls and `registry-old-wrappers.txt` (seed C-j), every `CopyTextToClipboard(` caller, logs/command lines/environment writers, the DC-17 sweep (custom packer/unpacker titles `src/packers.cpp:734`, `src/dialogsp.cpp`, `src/edtlbwnd.cpp`; seed S5 / L17), ledger L32; verdicts for contracts B6, B9 → `F/inventory.md` §B5/§B7 + `F/findings/P4.md`.
- [X] T015 [P] [US1] Run **P5** (plugin boundary): seeds S1–S10, ledger L38–L50, `src/plugins/shared/spl_*.h` encoding statements, `CSalamanderGeneral` text services in `src/zip.cpp`, plugin→core intakes in `src/plugins1.cpp`/`plugins2.cpp`/`plugins3.cpp`/`src/packers.cpp`, `src/pluglegacy.*`, `src/plugins/shared/splunicode.h`, the 065 boundary (`src/plugins/mdview/viewer.cpp:257/577/801`), `src/plugins/sftp/operats.cpp:24-43`, `src/plugins/ftp/ftputils.cpp:3349-3436`; FR-012 pre-classification (user-visible in a shipped configuration? local? enumerable surface?) for every plugin-internal site; verdicts for contracts B1, B7, B12 → `F/inventory.md` §B8 + `F/findings/P5.md`.
- [X] T016 [P] [US1] Run **P6** (user input & the unreleased delta): B6 intakes (`src/common/winlib.cpp` `EditLine`, `src/fileswn0.cpp` quick-search `WM_CHAR`, `src/editwnd.cpp` command line, `src/finddlg*.cpp` masks, `src/dialogs4.cpp` hot-path entry, rename/new-folder dialogs), then every file in `F/delta-manifest.md` line by line with a general regression lens (lifetimes, buffers, failure paths, threads — not only encoding), including the 052–055 diff for L87 → `F/inventory.md` §B6 + `F/findings/P6.md`.
- [X] T017 [US1] Consolidate: merge `F/findings/P*.md` into the Findings table of `F/review-report.md` (assign `F<n>`, dedupe convergent findings keeping all raising perspectives, drop entries without a failure scenario into a "Notes" list); complete the per-boundary counts per classification in `F/inventory.md`; set every DC-01…DC-20 sweep row to complete or partial-with-reason; fill every contract obligation verdict (deviation → `F<n>`); give every ledger row L01–L89 a re-examination note (still-open / closed-by-`<feature>` / fix-candidate `F<n>` / by-design).
- [X] T018 [US1] Coverage check (SC-001/SC-002): a script or grep pass confirming every `file:line` in `F/candidates/*.txt` appears in `F/inventory.md` (individually or inside a listed group), every DC row is complete, every L row dispositioned, every boundary section non-empty; record counts and any gaps (then close them) in `F/review-report.md` Coverage.

**Checkpoint**: the audit is a complete, auditable deliverable on its own — MVP.

---

## Phase 4: User Story 2 — Verified, regression-free fixes for every confirmed defect (Priority: P1)

**Goal**: every finding independently verified; every confirmed, in-scope
defect fixed minimally, with an independent regression review, byte-identity
evidence, timing where required, and a proven fail-before/pass-after check.

**Independent Test**: for any code change in `git diff <baseline>..HEAD -- src tools`,
`F/review-report.md` shows its `F<n>` (CONFIRMED, with scenario), its `X<n>`
(ACCEPTED regression review, affected surfaces, byte-identity, timing if
per-item, check with fail-before proof).

### Implementation for User Story 2

- [X] T019 [US2] Batch the Findings table by file/class and spawn one fresh **verifier** subagent per batch (`F/charters.md` §Verifier; the verifier is never the raising perspective); record CONFIRMED/REFUTED, evidence `file:line`, and the reproduced failure scenario in `F/review-report.md`; contested or high-impact findings (S1–S5, anything in `src/common/`) get a second verifier; REFUTED → disposition no-change.
- [X] T020 [US2] Scope-test every CONFIRMED finding per research R5 step 3 and record the disposition: core & shipping → fix; plugin-internal → FR-012 test (user-visible in a shipped configuration, local, enumerable surface) else Deferred `D<n>`; disabled-language-only → latent (re-enable checklist); non-encoding → FR-015 test (data-only or one-line local) else Deferred; vendored/dev tooling → Deferred.
- [X] T021 [US2] Fail-before proof, per fix, **before** changing product code: run the matching draft rule (`python tools\check_encoding.py --draft --rule <id> --format list`) or add a property check to `TestEncodingReview068()` in `src/saltests/saltests.cpp` that fails on the current tree; paste the failing line/CHECK into the Fix record; where neither is possible (on-screen-only defect) write the manual scenario into `F/quickstart.md` Sweep as a new W row.
- [X] T022 [US2] Apply the minimal fixes with the house helpers (`SalU8ToW*` + W API, facades `SalCreateFile`/`SalDeleteFile`/…, `LoadStrU8`, `Sal*U8` sinks, `CopyTextToClipboardU8`, `SalLegacyToU8Alloc` at intakes) — one change per finding or per file cluster, no adjacent refactoring; for each, write the Fix record `X<n>` in `F/review-report.md` with files, the affected-surface list (grep every consumer of the changed symbol/resource/control), and the check reference.
- [X] T023 [US2] Spawn one fresh **regression-review** subagent per fix (or per file batch) with the diff and the affected-surface list (`F/charters.md` §Regression reviewer) → per-surface verdicts, English/ASCII byte-identity argument (ASCII `LoadStrU8 == LoadStr`; W call on ASCII == A call), plugin-facing bytes (`git diff <baseline> -- src/plugins/shared/spl_gen.h src/plugins.h src/plugins/shared/spl_vers.h` documentation-only; `src/zip.cpp` forwarders never pass `u8`), 066/067 quickstart scenarios touched; ACCEPTED → keep; REJECTED → rework and re-review, or withdraw to Deferred with the reason.
- [X] T024 [US2] Per-item-path fixes (folder listing, sorting, icon/overlay reading, per-name conversion — e.g. anything in `src/fileswn1.cpp`, `src/fileswn4.cpp`, `src/sort.cpp`, `src/shiconov.cpp`, `src/snooper.cpp`, `src/common/salfileio.cpp`, `SalConvertFindDataW`): run the G6 timing method from `F/quickstart.md` — warm-up, 5 runs on the pre-fix binary (`Release_x64_prefix`, T002), 5 runs after — record all ten values in `X<n>`; accept only if the after-median lies within the baseline min–max.
- [X] T025 [US2] Plugin-local fixes admitted by T020 under FR-012 (expected candidates: L43 `src/plugins/zip/dialogs.cpp:1839-1840,1925-1926` → the plugin's own `SetDlgItemTextU8`; L44 `src/plugins/filecomp/mainwnd.cpp:2043-2046,2135-2137` blank-title fallback; S10 `src/plugins/sftp/operats.cpp:24-43` if admitted; S3 `src/plugins/shared/splunicode.h` only if P5 proved the FR-012 conditions): same T021–T023 discipline per fix, rebuild the plugin (`build.cmd full`), and add a plugin-specific manual scenario to W20 in `F/quickstart.md`.
- [X] T026 [US2] Non-encoding trivial fixes admitted by T020 under FR-015 (expected candidates: L51 French `octetss` — pin `octet{s|0||1|s}` for string 12820 in `translations/ui-overrides.json` and apply it to `translations/french/salamand.slt` with the `translate.merge` step documented in `specs/055-contextual-retranslation/run-notes.md`; L45 `src/plugins/regedt/fs4.cpp:479` `CQuadWord` format argument): each with its own fail-before proof (slt round-trip / build warning or a printf check) and regression verdict.
- [X] T027 [US2] Shared-machinery rule: any accepted change under `src/common/salunicode.*`, `src/common/winlib.*`, `src/common/salfileio.*`, `src/common/salpath.*`, `LoadStr*` (`src/salamdr2.cpp`), `NumberToStr`/`PrintDiskSize` (`src/salamdr1.cpp`/`salamdr6.cpp`), the registry facade (`src/salamdr6.cpp`), or the clipboard helpers (`src/salamdr4.cpp`) is flagged "full sweep" in its `X<n>` — Phase 5 then runs W1–W20 in both languages instead of the affected subset.
- [X] T028 [US2] Promote guards: after the fixes, `build.cmd`, run saltests (count ≥ T009 baseline) and `python tools\check_encoding.py --draft --format list`; for every draft rule that proved ≥ 1 real defect and now reports 0 (after annotating legitimate sites with `// encoding-check: allow <rule> - <reason>`), move it from `DRAFT_RULES` to `RULES` in `tools/check_encoding.py` so `build.cmd` enforces it; rules still noisy stay draft and are recorded with their false-positive count in `F/review-report.md`; confirm `python tools\check_encoding.py --strict` = TOTAL 0.

**Checkpoint**: every accepted change is finding-traceable, regression-reviewed and guarded.

---

## Phase 5: User Story 3 — Whole-product stability gates and regression sweep (Priority: P2)

**Goal**: evidence that the product with every fix in is at least as stable
as 0.1.4 — gates G1–G7 green or waived with reason, sweep W1–W20 in Czech and
Hungarian UI recorded.

**Independent Test**: the Gate and Sweep tables in `F/review-report.md` are
complete, every row PASS or WAIVED(reason), the English spot-check shows no
difference against the pre-fix Release build.

### Implementation for User Story 3

- [X] T029 [US3] G1: `build.cmd full` (Debug) — 0 errors; diff the MSBuild warning lines for files in `git diff --name-only <baseline>..HEAD -- src` against the T001 capture (no new warnings); record in `F/review-report.md` Gates.
- [X] T030 [US3] G2: `build.cmd full release` — same bar; record.
- [X] T031 [US3] G3: run `%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\saltests\saltests.exe` — `N checks, 0 failed`, N ≥ 1229 and ≥ the T009 working baseline; record N.
- [X] T032 [US3] G4: `python tools\check_encoding.py --strict` = TOTAL 0; then for each promoted rule re-demonstrate fail-before on the baseline commit (`git worktree add ..\tc-prefix <baseline>` → run `python tools\check_encoding.py --rule <id> --format list` there → the pre-fix site is flagged → `git worktree remove ..\tc-prefix`); paste the flagged lines into `F/review-report.md` Gates.
- [X] T033 [US3] G5: start `tserver.exe` (T004), launch `%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\tandemcommander.exe`, browse `D:\Zkouška\Můj disk\` and the surrogate file, stay ≥ 10 s, close with Alt+F4; record exit code, absence of the "monitored handles remained opened" and "_CrtCheckMemory failed" boxes, no assertion, and the listing of `%LOCALAPPDATA%\Tandem Commander` crash reports before/after.
- [X] T034 [US3] Sweep record: fill the W1–W20 × {cs, hu} table in `F/review-report.md` with the fixture paths and the exact steps from `F/quickstart.md`, mark the rows a full-sweep fix (T027) forces versus the affected subset, and state that the on-screen execution is the user's manual pass (precedent 067); everything automatable in a row (clipboard content via `Get-Clipboard`, file-list byte diff via `fc /b`, registry value read-back via `reg query`) is executed and recorded now.
- [X] T035 [US3] G7 automated part: `fc /b` of a Make File List ASCII output produced by `Release_x64_prefix` vs the fixed Release build; `git diff <baseline> -- src/plugins/shared/spl_gen.h src/plugins.h src/plugins/shared/spl_vers.h` limited to comments; `grep LAST_VERSION_OF_SALAMANDER src/plugins/shared/spl_vers.h` = 106; the English on-screen side-by-side of W1–W6, W13 is listed for the user's pass; record in `F/review-report.md` Gates.
- [ ] T036 [US3] Any FAIL in G1–G7 or in a sweep row executed so far → new Finding `F<n>` → back through T019–T028 (bounded re-verification: only the Sites and gates the fix touches); WAIVED rows carry their reason (no cloud drive for W15, no non-ASCII ZIP for W20 …) — never a silent skip.

**Checkpoint**: gates green, sweep table handed over with every automatable cell filled.

---

## Phase 6: User Story 4 — Auditable record and durable guards (Priority: P2)

**Goal**: one report a reader can audit end to end; every confirmed defect
class guarded; user-visible fixes in the changelog.

**Independent Test**: `F/quickstart.md` "Auditing the report" items 1–8 all
hold; any 3 random code changes trace to `F`/`X` rows; every confirmed DC has
a named check proven on the pre-fix tree; every user-visible fix has a
changelog entry.

### Implementation for User Story 4

- [X] T037 [US4] Assemble `F/review-report.md` in full per research R10: Scope & method (link research.md; perspectives with their coverage lists from `F/findings/P*.md`), Coverage (T018 numbers), Inventory summary per boundary, Defect-class sweep table, Contract compliance table, Findings table, Fixes table, Deferred items (ledger rows with fresh dispositions first, then new `D<n>` rows with justification and where recorded), Gates, Sweep, and the stability verdict.
- [X] T038 [P] [US4] Durable-guard record in `F/review-report.md`: table confirmed DC → named rule (`tools/check_encoding.py` `RULES`) or test (`src/saltests/saltests.cpp` `TestEncodingReview068`), with the fail-before line and the pass-after result; confirm the module docstring of `tools/check_encoding.py` documents every promoted rule and that `build.cmd:222` runs strict unchanged.
- [X] T039 [P] [US4] `CHANGELOG.md` Unreleased → Fixed: one entry per user-visible fix in the user's terms (the symptom that is gone, the language/locale it affected, what remains deferred), truthfully scoped per the constitution's Release Documentation rules; internal-only changes (guards, tests) mentioned only if they change what a plugin author can rely on.
- [X] T040 [P] [US4] Latent disabled-language items (L52 + any new): add the notes to the re-enable checklist location feature 067 used (grep `re-enable` in `translations/` and `specs/067-fix-drive-info-encoding/`; if it is `translations/languages.cfg` comments, keep that style); reference the `F<n>`/`L<n>` ids.
- [X] T041 [P] [US4] Traceability spot-check (SC-003/SC-009): pick 3 random hunks from `git diff <baseline>..HEAD -- src tools translations` and trace each to its `F<n>` and ACCEPTED `X<n>`; record the three in `F/review-report.md` Coverage.
- [X] T042 [US4] Write the stability verdict: explicit statement over gates, findings dispositions and the sweep; list exactly which sweep rows still await the user's on-screen pass (cs, hu, en) and which fixes they cover — the report is complete when every automatable gate is green and the manual matrix is handed over.

**Checkpoint**: report auditable per quickstart §Auditing; guards durable.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T043 [P] Format every changed source file with `clang-format` (or `normalize.ps1` per the constitution) and confirm `git diff --stat <baseline>..HEAD -- src` contains only the files with confirmed-finding changes, guards and tests.
- [X] T044 [P] Add the 068 entry to `CLAUDE.md` "Recent Changes" (house convention, ≤ 12 lines: what was reviewed, how many findings/fixes, which guards are new, what stays deferred).
- [X] T045 Post-polish re-run of G1, G3, G4: `build.cmd full`, saltests, `python tools\check_encoding.py --strict`; update the Gates table if any number changed.
- [X] T046 Run `F/quickstart.md` "Auditing the report" items 1–8 and tick them at the end of `F/review-report.md`.
- [X] T047 Housekeeping: keep `F/candidates/`, `F/findings/`, `F/charters.md`, `F/delta-manifest.md` as evidence; ensure `.specify/feature.json` still points at `specs/068-encoding-regression-review`; scratchpad files are not committed.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: none — T001 first (baseline numbers), T002–T005 in parallel with it.
- **Foundational (Phase 2)**: after Phase 1 — T006 (charters) sequential; T007–T010 in parallel. **Blocks all stories.**
- **US1 (Phase 3)**: after Phase 2 — T011–T016 fully parallel (six subagents); T017 after all six; T018 after T017.
- **US2 (Phase 4)**: after US1's findings pool (T017) — T019 → T020 → per-fix loop T021 → T022 → T023 (→ T024 when per-item) → T025/T026 as admitted → T027 flagging → T028 last.
- **US3 (Phase 5)**: after US2 (or after US1 for a fix-free run) — T029–T033 sequential builds/runs, T034/T035 after, T036 loops back to Phase 4 on FAIL.
- **US4 (Phase 6)**: after US3 — T037 first; T038–T041 parallel; T042 last.
- **Polish (Phase 7)**: after US4.

### User Story Dependencies

- **US1** delivers value alone (the audit) — the MVP.
- **US2** consumes US1's findings pool; verifiers and regression reviewers are separate agents from the perspectives and from the fixer (R2).
- **US3** verifies US2's output; with zero confirmed fixes it still runs (clean-verdict case, spec edge case).
- **US4** consumes US1–US3 records.

### Parallel Opportunities

- Phase 1: T002, T003, T004, T005 alongside T001.
- Phase 2: T007, T008, T009, T010 alongside each other (T006 independent of them too — sequential only because the perspectives need it first).
- Phase 3: T011–T016 — six subagents at once (the largest parallel block).
- Phase 4: verifier batches (T019) and regression reviews (T023) run per batch in parallel; T024 timing runs are sequential on the machine.
- Phase 6: T038–T041.

---

## Parallel Example: User Story 1

```text
# After T006–T010, launch the six perspectives together (read-only subagents):
Task: "P1 file-system/shell/launch boundary → F/inventory.md §B1/B2/B5 + F/findings/P1.md"
Task: "P2 UI text sinks & composition → F/inventory.md §B3/B4 + F/findings/P2.md"
Task: "P3 converter & measurement machinery → F/inventory.md §Cross-cutting + F/findings/P3.md"
Task: "P4 configuration/clipboard/external → F/inventory.md §B5/B7 + F/findings/P4.md"
Task: "P5 plugin boundary → F/inventory.md §B8 + F/findings/P5.md"
Task: "P6 user input + unreleased delta → F/inventory.md §B6 + F/findings/P6.md"
# Then T017 consolidation, T018 coverage check.
```

## Parallel Example: User Story 2

```text
# Per findings batch (by file), independent agents:
Task: "Verifier for batch src/cache.cpp (F3, F7, F12) — refute-first"
Task: "Verifier for batch src/pack1.cpp+pack2.cpp (F4, F5, F9) — refute-first"
# After fixes land, per fix batch:
Task: "Regression review of X1–X3 (diff of src/cache.cpp) — find a regression"
Task: "Regression review of X4 (diff of src/zip.cpp CopyTextToClipboard wrapper) — plugin-facing bytes"
```

---

## Implementation Strategy

### MVP First (User Story 1 only)

1. Phase 1 baseline + fixtures; Phase 2 queues, charters, draft guards.
2. Phase 3: the six perspectives, consolidation, coverage check.
3. **STOP and VALIDATE**: `F/inventory.md` complete, findings pool with scenarios — the review alone already answers "where are the remaining encoding defects".

### Incremental Delivery

1. US2 on top: verification → fixes → regression review → guard promotion (each fix is independently accepted; a fix-free outcome is a legitimate result).
2. US3: gates and the sweep record; the user's on-screen pass in cs/hu/en.
3. US4: report, changelog, durable-guard record, verdict.
4. Polish: formatting, CLAUDE.md, final gate re-run.

### Notes

- Every code change traces to one CONFIRMED `F<n>` and one ACCEPTED `X<n>` — no exceptions (FR-007, SC-003).
- Plugin-facing services stay byte-identical; interface version stays 106 (FR-009).
- Findings without a concrete failure scenario are notes, not findings (FR-006).
- A REJECTED regression review means rework or withdrawal — never "merge and see".
- Any gate FAIL or sweep FAIL re-enters Phase 4 with bounded re-verification.
- Stop at any checkpoint; US1 alone is a shippable deliverable (the audit record).

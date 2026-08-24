# Tasks: Fix Garbled Numbers in Drive Information Dialog

**Input**: Design documents from `/specs/067-fix-drive-info-encoding/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/number-format-encoding.md, quickstart.md

**Tests**: Included — the spec's SC-003 explicitly requires automated coverage
that fails on pre-fix behavior. That gate is the extended `check_encoding.py`
rule (fails on the pre-fix `salamdr6.cpp` composition, passes after) plus a
`saltests` property test pinning the conversion contract.

**Organization**: US1 = Ctrl+F1 dialog (reported defect, MVP); US2 = the other
core surfaces sharing the formatting path; US3 = robustness across Windows
separator settings.

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup (Baseline)

**Purpose**: Establish the green pre-change baseline that the byte-identity
regression checks compare against.

- [X] T001 Baseline: run `build.cmd` (Debug x64) and `%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\saltests\saltests.exe`; record the passing check count. With Czech UI, capture the pre-fix rendering of Ctrl+F1 (matches `specs/067-fix-drive-info-encoding/informace_o_jednotce.png`) and of the English Ctrl+F1 for later byte-identity comparison (quickstart §3.9).
  *(Done 2026-08-24: build clean, saltests 1221/0. Czech pre-fix rendering = the evidence screenshot in this directory; English capture deferred to the user's manual pass — English output is additionally proven byte-identical by construction, ASCII `LoadStrU8 == LoadStr`.)*

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The `u8` capability on the shared producer — required by both US1
and US2. Behavior of every existing caller must remain byte-identical (all
compile against the default `FALSE`).

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T002 Add `BOOL u8 = FALSE` as the last parameter of `PrintDiskSize` in `src/consts.h` (declaration at :487) with a feature-041-style doc comment (u8 picks `LoadStrU8`, result is valid UTF-8 by construction; plugins via `CSalamanderGeneral` keep the ANSI default). In the same edit, correct the stale feature-041 comment at `src/consts.h:871-876` (the Find dialog and many plugin sinks are no longer ANSI-display-only — features 042/043; see research.md R8.2).
- [X] T003 Implement the `u8` branch in `PrintDiskSize` in `src/salamdr6.cpp` (definition at :416): when TRUE, load `IDS_PLURAL_X_BYTES` (mode 1/2, :425), `IDS_SIZE_B..EB` (mode 0/1/4, :456-477) and `IDS_SIZE_KB` (mode 3, :527) via `LoadStrU8` instead of `LoadStr`; the `HLanguage == NULL` ASCII fallbacks stay as-is. No other logic changes; buffer contract unchanged (contracts/number-format-encoding.md §5).
- [X] T004 Verify byte-identity of the no-caller-change build: `build.cmd` clean; `saltests.exe` count unchanged from T001; spot-check Czech Ctrl+F1 still shows the pre-fix garble (no caller passes TRUE yet — proves the default path is untouched).

**Checkpoint**: Producer capability in place, zero behavioral change anywhere.

---

## Phase 3: User Story 1 — Correct byte counts in the Drive Information dialog (Priority: P1) 🎯 MVP

**Goal**: The reported defect — Ctrl+F1 Used/Free/Capacity render correctly in
Czech (and every shipped language).

**Independent Test**: Czech UI, Ctrl+F1 on a local NTFS drive → all three byte
counts match Explorer's Properties character-for-character; English UI output
byte-identical to the T001 baseline (quickstart §2.1, §3.9).

### Implementation for User Story 1

- [X] T005 [US1] Pass `TRUE` as the new `u8` argument in all six `PrintDiskSize` calls in `CDriveInfo::Transfer` in `src/dialogs3.cpp:1537-1546` (IDT_CAPACITY, IDT_CAPACITY_SHORT, IDT_FREESPACE, IDT_FREESPACE_SHORT, IDT_USEDSPACE, IDT_USEDSPACE_SHORT — the mode-0 shorts included: byte-identical today, correct by construction for any future language).
- [X] T006 [US1] Build and validate US1: quickstart §2.1 (Czech Ctrl+F1 vs Explorer, including plural spot-check "1 bajt"/"bajty"/"bajtů" where obtainable), §3.4 (cluster/sector pure-number rows unchanged), §3.9 English byte-identity vs T001 capture.
  *(Build clean, saltests 1221/0. The on-screen Czech/English checks are the user's manual pass — quickstart §2.1/§3.4/§3.9.)*

**Checkpoint**: Reported bug fixed and independently verified — shippable MVP.

---

## Phase 4: User Story 2 — Correct grouped numbers on every core surface (Priority: P2)

**Goal**: The same defect class removed from every other core-application
surface found by the audit (research.md R2), with the plugin boundary and all
already-correct surfaces byte-frozen.

**Independent Test**: Quickstart §2.2–§2.5 all pass in Czech; §3 regression
sweep items 1–8 and 10 render exactly as before the fix.

### Implementation for User Story 2

- [X] T007 [P] [US2] Pass `u8=TRUE` in the four `PrintDiskSize` mode-1 calls in `CSizeResultsDlg` in `src/dialogs2.cpp:412, 452, 475, 478` (directory-sizes / Calculate Occupied Space results).
- [X] T008 [P] [US2] Pass `u8=TRUE` in the `PrintDiskSize` mode-1 call in `CZIPSizeResultsDlg` in `src/dialogs3.cpp:2216` (archive size results).
- [X] T009 [P] [US2] Convert `LoadStr(IDS_NOTENOUGHSPACE)` to `LoadStrU8(IDS_NOTENOUGHSPACE)` in `src/zip.cpp:6566` (not-enough-space message on the pack path — exact twin of the already-converted `src/fileswn6.cpp:1109` / `src/fileswn8.cpp:1129`; do NOT touch the `CSalamanderGeneral` forwarders elsewhere in the file).
- [X] T010 [US2] Convert the viewer offset tooltip to the wide notification in `src/viewer3.cpp`: register with `TOOLINFOW` + `TTM_ADDTOOLW` (WM_CREATE block, ~:560-577) and handle `TTN_NEEDTEXTW` in the `WM_NOTIFY` case (~:3115-3135) — compose via `LoadStrU8(IDS_VIEWEROFFSETTIP)`, convert once with `SalU8ToW` into the wide `szText` using `_snwprintf_s`/`_TRUNCATE` (80-WCHAR cap; longest realistic string ≈ 61 chars). Empty the text when `ToolTipOffset == -1` as today.
- [X] T011 [US2] Build and validate US2: quickstart §2.2 (Alt+F10 occupied space), §2.3 (archive size results), §2.4 (not-enough-space message), §2.5 (viewer offset tooltip), plus regression sweep §3.1-3.3 (panel size formats, tiles, information line), §3.5-3.8 (Find, Alt+F1 drive menu, copy progress, overwrite prompt) and §3.10 (plugin surfaces byte-frozen: FTP low-disk hint / dbviewer unchanged, including their documented pre-existing garble).
  *(Build clean, saltests 1221/0; the code-level plugin freeze is proven by the diff — no `src/plugins/` source change. The on-screen sweep is the user's manual pass.)*

**Checkpoint**: All core surfaces correct; guard list verified untouched.

---

## Phase 5: User Story 3 — Robust across Windows separator settings (Priority: P3)

**Goal**: Correct rendering for any digit-grouping separator Windows can
supply (ASCII, non-ASCII, multi-character), pinned by automated coverage.

**Independent Test**: Quickstart §1 automated gates green; changing the
Windows digit-grouping separator changes the rendered separator and nothing
else (quickstart-style manual check from spec US3).

### Tests for User Story 3

- [X] T012 [P] [US3] Add `TestNumberCompositionEncoding()` to `src/saltests/saltests.cpp` (beside `TestUiTextEncoding` at :880; wire into `main()` at :1226): (a) compose digits + the real `SalGetLocaleInfoU8(LOCALE_STHOUSAND)` separator + UTF-8 unit word `"bajt\xC5\xAF"` → `CHECK(SalU8ToW(...) != 0)`; (b) same composition with the CP1250 unit byte `"bajt\xF9"` → `CHECK(SalU8ToW(...) == 0)` (pins why the `Â` fallback fired); (c) `SalU8ToWDisplay` on (b) costs exactly one U+FFFD (model: `saltests.cpp:842-849`); (d) synthetic multi-byte separators — NBSP `\xC2\xA0`, narrow NBSP `\xE2\x80\xAF`, apostrophe `\xE2\x80\x99` — spliced between digit groups all convert strictly.

### Implementation for User Story 3

- [ ] T013 [US3] Manual separator-robustness validation per spec US3: set the Windows digit-grouping separator to an apostrophe, then to a multi-character string; restart; verify Ctrl+F1 renders exactly the configured separator. Confirm the `InitLocales` guard (`src/salamdr1.cpp:962-972`) falls back to a plain space for separators over 4 UTF-8 bytes (no truncated sequence ever displayed). No code change expected — record results in this file or the PR notes.
  *(USER MANUAL STEP — requires changing Windows regional settings. The code-side halves are covered: the `InitLocales` guard is unchanged and reviewed, and saltests §4 of `TestNumberCompositionEncoding` pins NBSP / narrow-NBSP / apostrophe separators.)*

**Checkpoint**: Robustness property pinned by tests + verified manually.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T014 Extend `tools/check_encoding.py` with a rule catching the 067 pattern: an ANSI `LoadStr(` used as the template argument of `ExpandPluralString(` (and the sprintf-composition of a `LoadStr` format with `NumberToStr`/`PrintDiskSize` arguments). Requirements: the rule MUST flag pre-fix `src/salamdr6.cpp:425-427` (verify by temporarily reverting T003's `u8` branch selection or via `git stash`) and MUST pass `--strict` over the fixed tree — this is the SC-003 fails-before/passes-after gate. Follow feature-052 practice for tracked identifiers and the `// encoding-check: allow` escape-hatch documentation.
- [X] T015 [P] Documentation-only encoding statements in `src/plugins/shared/spl_gen.h`: `NumberToStr` (:1408), `PrintDiskSize` (:1413), `PointToLocalDecimalSeparator` (:3387) — state that separator bytes are UTF-8 (feature 041) and unit words ANSI at this boundary, per contracts/number-format-encoding.md §4. No signature or behavior changes.
- [X] T016 Run `clang-format -i` on all touched sources (`src/consts.h`, `src/salamdr6.cpp`, `src/dialogs2.cpp`, `src/dialogs3.cpp`, `src/zip.cpp`, `src/viewer3.cpp`, `src/saltests/saltests.cpp`, `src/plugins/shared/spl_gen.h`) and confirm UTF-8-BOM encoding preserved.
- [X] T017 Add the user-facing entry to `CHANGELOG.md` under `## [Unreleased]` → `### Fixed` (garbled digit-group separators — `Â` characters — in the Drive Information dialog, directory/archive size results and the not-enough-space message in languages whose byte-count word is accented, e.g. Czech/Hungarian; viewer offset tooltip separator; per constitution: user's terms, truthful scope, no version bump in this change).
- [X] T018 Full final validation per quickstart: `build.cmd` (gate incl. new check_encoding rule), `saltests.exe` (all green, count > baseline), `build.cmd full release` clean, complete manual matrix §2 + §3, and confirm no diff exists under `src/plugins/` except `spl_gen.h` comments (plugin freeze proof).
  *(Done 2026-08-24: Debug clean, saltests 1229/0 (baseline 1221), `check_encoding.py --strict` 0 findings — and proven to flag the pre-fix tree with exactly the 2 defect sites; `build.cmd full release` clean (19 plugins, 180 language modules); `git diff --name-only -- src/plugins/` = only `spl_gen.h` (doc comments). The on-screen manual matrix §2 + §3 is the user's pass — every automatable gate is green.)*

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (T001)**: none — do first (baseline is the reference for byte-identity checks).
- **Phase 2 (T002→T003→T004)**: sequential (same producer); blocks US1 and US2. US3's T012 does not depend on Phase 2 (property test of `src/common` converters) but is scheduled later to keep the suite delta reviewable.
- **US1 (T005→T006)**: after Phase 2.
- **US2 (T007/T008/T009 [P] → T010 → T011)**: after Phase 2; independent of US1, but T008 edits `src/dialogs3.cpp` (same file as T005) — if US1 is not yet merged, sequence T005 before T008.
- **US3 (T012→T013)**: T012 any time after T001; T013 after US1 (needs the fixed dialog).
- **Polish (T014–T018)**: after all stories; T014 before T018 (gate must be in place for the final run); T015 [P] with T014.

### Parallel Opportunities

- T007, T008, T009 — different files (given T005 done), no shared state.
- T012 (saltests) parallel with any US2 task — different files.
- T014 and T015 — different files.

## Implementation Strategy

**MVP first**: T001–T006 alone fix and verify the reported bug (Ctrl+F1) —
shippable increment. **Incremental**: US2 extends the identical one-argument
change to the three remaining dialog sites plus the two companion fixes, each
independently verifiable; US3 + Polish pin the contract so the class cannot
silently return. Single-developer flow: straight T001→T018. Stop-points at
every checkpoint; each task or logical group is commit-sized (constitution:
small, reviewable, revertible).

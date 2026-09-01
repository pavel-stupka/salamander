---

description: "Task list for feature 074 — fixed-width line-number gutter in the code viewer"
---

# Tasks: Fixed-width line-number gutter in the code viewer

**Input**: Design documents from `/specs/074-fix-codeview-gutter/`
**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/gutter-geometry.md](contracts/gutter-geometry.md),
[quickstart.md](quickstart.md)

**Tests**: Included. Not because the template offers them, but because this
feature's defect is precisely a value with no consumer — nothing observable was
wrong with the *logic*, so only shape guards can prevent its return
(contract §S7). Each new guard is **proven to fire on a planted defect**, the
idiom established in feature 068.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on unfinished work)
- **[Story]**: US1 / US2 / US3 from [spec.md](spec.md); Setup, Foundational and
  Polish tasks carry none

## Path Conventions

This feature touches the codeview plugin's embedded web page and its harness:

- `src/plugins/codeview/web/viewer.css` — the `.gut` rule
- `src/plugins/codeview/web/viewer.js` — digit count, `layout()`, `makeLine`
- `src/plugins/codeview/test/harness/test_page.mjs` — the guards
- Fixtures live in `%TEMP%\cvalign\` and are not committed

No C++ file, no configuration, no resource table, no plugin interface.

---

## Phase 1: Setup (baseline and evidence)

**Purpose**: know exactly what the tree does *before* the change, so every later
claim has something to be measured against.

- [X] T001 [P] Create the alignment fixtures per [quickstart.md](quickstart.md) §3 into `%TEMP%\cvalign\` (`a9.txt`, `a120.txt`, `a1200.txt`, `abig.txt`, `along.txt`); confirm `a120.txt` has 120 lines and each begins with `|`
- [X] T002 Build the unchanged tree with `build.cmd` (Debug x64) and reproduce the reported defect: F3 on `%TEMP%\cvalign\a120.txt` shows the `|` column stepping right at line 10 and again at line 100; capture a screenshot alongside `temp/radky.png` as the before-evidence
- [X] T003 [P] Record the baseline: `src\plugins\codeview\test\run_tests.cmd` reports `RESULT: all codeview checks passed` on the unchanged tree, and note the current check count so the three new ones are visible as additions

**Checkpoint**: the defect is reproduced on a fixture that makes it unmissable, and the harness is green before anything is touched.

---

## Phase 2: Foundational (the testable seam)

**Purpose**: the digit rule must be reachable by the headless harness before any
guard can assert on it. Blocks US1 and US2.

**⚠️ No user story work begins until this phase is complete.**

- [X] T004 Extract the digit rule into a named function in `src/plugins/codeview/web/viewer.js`: add `function gutterDigitsFor(count) { return String(Math.max(1, count)).length }` and call it from `layout()` in place of the inline expression; keep the file's existing style (2-space indent, no semicolons, no BOM)
- [X] T005 Add lifted unit checks to `src/plugins/codeview/test/harness/test_page.mjs`: `lift('gutterDigitsFor')` and assert the table in [contracts/gutter-geometry.md](contracts/gutter-geometry.md) §S3 — 0→1, 1→1, 9→1, 10→2, 99→2, 100→3, 999→3, 1000→4, 1000000→7
- [X] T006 Prove T005 fires: plant a defect in `gutterDigitsFor` in `src/plugins/codeview/web/viewer.js`, run the harness, confirm a FAIL, then restore. **As executed**: dropping `Math.max(1, …)` did *not* fail anything — `String(0).length` is already 1, so that floor is defensive only and its check does not discriminate. A realistic off-by-one (`count - 1`, the last index instead of the count) was planted instead and failed 4 of the 6 checks

**Checkpoint**: the digit rule is named, tested, and the test is known to be capable of failing.

---

## Phase 3: User Story 1 — Source text starts at the same place on every line (Priority: P1) 🎯 MVP

**Goal**: one width for the number column, so the code column never moves.

**Independent Test**: F3 on `%TEMP%\cvalign\a120.txt` — the `|` characters form
one unbroken vertical line through 9→10 and 99→100; same on `a1200.txt` through
999→1000.

- [X] T007 [US1] Add the width guard to `src/plugins/codeview/test/harness/test_page.mjs`: assert that `viewer.css`'s `.gut` rule declares both `width: var(--gutter-min)` and `box-sizing: content-box`; run the harness and confirm it FAILS against the current stylesheet — that failure *is* the reported defect, expressed as a test. **`width`, not the `min-width` this task originally named**: measurement showed `1ch` is 0.0117 px narrower than a rendered digit, so `min-width` does not bind on the widest rows (research R2b)
- [X] T008 [US1] Fix the rule in `src/plugins/codeview/web/viewer.css`: add `width: var(--gutter-min)` (no `var()` fallback — an absent property must degrade to `auto`, not to a too-narrow column) and `box-sizing: content-box` to `.gut`, with a comment stating both load-bearing reasons: content-box is deliberate against the page-wide `border-box` (under which the 20 px padding swallows the constraint and makes it inert, research R2), and `width` rather than `min-width` (research R2b). `text-align`, `padding`, `position: sticky` and `user-select` untouched
- [X] T009 [US1] Delete the dead `g.style.minWidth = ''` line from `makeLine` in `src/plugins/codeview/web/viewer.js` — it clears an inline style nothing sets and is a per-row write in the scroll hot path
- [X] T010 [US1] Add the second shape guard to `src/plugins/codeview/test/harness/test_page.mjs`: `layout()` still publishes `--gutter-min`, and `makeLine` contains no `style.minWidth` write; prove it fires by pasting the deleted line back, running `run_tests.cmd`, then removing it again
- [X] T011 [US1] Reorder `layout()` in `src/plugins/codeview/web/viewer.js` so `--gutter-min` is published **before** `resetGeometry()` and `render(true)` (research R5); add a one-line comment saying why (wrap heights depend on the gutter width)
- [X] T012 [US1] Run `src\plugins\codeview\test\run_tests.cmd` — all checks green, including the three added ones
- [ ] T013 [US1] Rebuild (`build.cmd`; if the CSS change does not appear, rebuild the plugin per [quickstart.md](quickstart.md) §1 rather than editing the stylesheet again) and verify [quickstart.md](quickstart.md) scenarios 1, 3, 5 and 9 on the fixtures: no step at any power-of-ten boundary, short file not padded, 120 000-line file scrolls with no horizontal movement, and a new document in the same window re-sizes the column with no post-first-frame reflow — **build done and verified** (`codeview.spl` rebuilt; both web assets confirmed byte-for-byte inside it). The four GUI scenarios remain a human step; scenarios 1/3/9 are additionally covered at stylesheet level by the measurement matrix in fix-log.md.

**Checkpoint**: the reported defect is gone and cannot silently return; US1 is independently demonstrable against the T002 before-evidence.

---

## Phase 4: User Story 2 — Line numbers are right-aligned in their column (Priority: P2)

**Goal**: units under units, tens under tens.

**Independent Test**: in `%TEMP%\cvalign\a120.txt`, the final digits of 7, 42 and
118 sit in one column, and 42's tens digit sits under 118's tens digit.

**Note**: no new declaration is needed — `text-align: right` has been on `.gut`
since feature 070 and was inert only because the box hugged its content. This
phase locks that in and proves it, rather than inventing work.

- [X] T014 [US2] Add the alignment guard to `src/plugins/codeview/test/harness/test_page.mjs`: assert `viewer.css`'s `.gut` rule declares `text-align: right`; prove it fires by temporarily removing that declaration, running `run_tests.cmd`, then restoring it
- [ ] T015 [US2] Verify [quickstart.md](quickstart.md) scenario 2 in the built plugin on `%TEMP%\cvalign\a120.txt` and `a1200.txt`: numbers flush right at every digit width, and the gap between the numbers and the code identical on every line — covered at stylesheet level (number right edges within 0.016 px); GUI confirmation outstanding.

**Checkpoint**: US1 and US2 both hold and are separately checkable.

---

## Phase 5: User Story 3 — Alignment survives every viewer interaction (Priority: P3)

**Goal**: alignment is a property of the view, not of the first frame.

**Independent Test**: on `%TEMP%\cvalign\a1200.txt`, zoom two steps in and out,
toggle wrap and line numbering, and confirm the column never breaks.

- [ ] T016 [P] [US3] Verify [quickstart.md](quickstart.md) scenario 6 (zoom: Ctrl+`+`/`-`/`0` and Ctrl+wheel) on `%TEMP%\cvalign\a1200.txt` — numbers and code scale together, alignment holds at every step, numbers stay flush right; no JavaScript is expected to run for this, so a failure here means the `ch` unit is not resolving where research R1 says it does — covered at stylesheet level (zoom 8/24/40 px, spread 0.000 px); GUI confirmation outstanding.
- [ ] T017 [P] [US3] Verify [quickstart.md](quickstart.md) scenario 7 (word wrap on `along.txt`): one number per logical line, every continuation row starting at the text column; toggle back off and confirm alignment is unchanged — covered at stylesheet level (wrap on, spread 0.000 px); GUI confirmation outstanding.
- [ ] T018 [P] [US3] Verify [quickstart.md](quickstart.md) scenario 8 (line numbering off/on): no empty column left behind when off, aligned column restored when on, no reload, scroll position kept — covered at stylesheet level (gutter off: no column, text at 0 px); GUI confirmation outstanding.
- [X] T019 [US3] Answer the open research question R6: on `%TEMP%\cvalign\along.txt` with wrap off, scroll right and observe whether the code text is visible *through* the sticky number column ([quickstart.md](quickstart.md) scenario 4). Record the observation in the fix log either way — this task produces an answer, not a change
- [X] T020 [US3] **Conditional on T019 confirming the show-through**: in `applyThemeColors` in `src/plugins/codeview/web/viewer.js`, add `set('--gutter-bg', c['editorGutter.background'], 'var(--bg)')` next to the other theme colours, then re-run scenario 4 in light and dark schemes. If T019 found no show-through, close this task as not-needed with the observation recorded — nothing else in the feature depends on it

**Checkpoint**: all three stories hold; the one adjacent question is settled with evidence rather than assumption.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [ ] T021 [P] Verify copy fidelity ([quickstart.md](quickstart.md) scenario 10): a selection spanning the 9→10 boundary, Select All, and a selection crossing wrapped lines all paste as the file's own text — no line numbers, no padding spaces, no leading zeros (FR-010, contract §S6) — no character is inserted anywhere, so nothing can reach the clipboard that did not before; needs the running viewer to confirm.
- [ ] T022 [P] Verify [quickstart.md](quickstart.md) scenario 11 (F9/Shift+F9 through all schemes, light and dark): numbers legible, alignment unaffected, no other colour changed — the gutter is opaque on all three theme paths (verified); the full scheme sweep needs the running viewer.
- [ ] T023 [P] Verify [quickstart.md](quickstart.md) scenario 12 (no performance regression): compare `rendered.firstPaintMs` on a large source file against the T002 baseline build, and scroll `abig.txt` for smoothness; the change removes a per-row CSSOM write, so the expected direction is neutral-to-better — the change removes a per-row CSSOM write; needs the running viewer to measure.
- [X] T024 Conformance check: `git diff --stat` lists only `src/plugins/codeview/web/viewer.css`, `src/plugins/codeview/web/viewer.js` and `src/plugins/codeview/test/harness/test_page.mjs`; no BOM added to any of them (they are UTF-8 without BOM, unlike the C++ sources); style matches each file's existing convention; `LAST_VERSION_OF_SALAMANDER` and every `.cpp`/`.h`/`.rc` untouched
- [X] T025 Write `specs/074-fix-codeview-gutter/fix-log.md` in the feature-070 style: the defect as reported, what the diagnosis actually found (a computed property with no consumer, and the border-box arithmetic that made the obvious fix inert), what changed, T019's answer, and anything left open
- [ ] T026 **Ship gate — release only, not part of this feature.** In the release that ships this, and in one change: a `CHANGELOG.md` *Fixed* entry in the user's terms ("in the Code Viewer the line number column now keeps one width with the numbers right-aligned, so the code no longer shifts sideways at line 10, 100 and 1000") plus the version/build bump in `src/plugins/shared/spl_vers.h` (`VERSINFO_SALAMANDER_MINORB`, `VERSINFO_BUILDNUMBER` + its comment), `MyAppVersion` in `setup/tandemcommander.iss`, and the version line in `CLAUDE.md`. `LAST_VERSION_OF_SALAMANDER` (106) stays — the plugin API does not change

---

## Dependencies & Execution Order

### Phase dependencies

- **Phase 1 (Setup)**: no dependencies. T002 needs T001's fixtures.
- **Phase 2 (Foundational)**: after Phase 1. **Blocks US1 and US2** — the guards
  cannot lift a function that has no name.
- **Phase 3 (US1)**: after Phase 2.
- **Phase 4 (US2)**: after Phase 2. Its *verification* (T015) needs the build
  from T013, so in practice it follows US1 even though its guard (T014) does not.
- **Phase 5 (US3)**: after US1 — it verifies that US1's property survives
  interaction, so there is nothing to check before US1 lands.
- **Phase 6 (Polish)**: after the stories it checks. T026 is deferred to a
  release and is not a completion criterion for this feature.

### Story dependencies

- **US1 (P1)**: independent once Phase 2 is done. Delivers the fix on its own.
- **US2 (P2)**: shares the same CSS rule as US1 but is separately assertable;
  a build with a fixed-width, left-aligned column would satisfy US1 and fail US2.
- **US3 (P3)**: verification-only over US1's property, plus the conditional
  T020. No US3 task changes US1 or US2 behaviour.

### Within a story

Guard first, then the change, then prove the guard can fail, then verify in the
built plugin. T007 is deliberately ordered *before* T008 so the harness is seen
failing against the shipped stylesheet.

### Parallel opportunities

- T001 and T003 (Setup, different concerns)
- T016, T017, T018 (US3 verifications: independent interactions, one build)
- T021, T022, T023 (Polish verifications, one build)
- T014's guard could be written while US1 is in progress — but it touches the
  same harness file as T007 and T010, so it is **not** marked `[P]`

---

## Parallel Example: User Story 3

```text
# One build, three independent GUI checks:
Task: "T016 zoom matrix on %TEMP%\cvalign\a1200.txt (quickstart scenario 6)"
Task: "T017 word wrap on %TEMP%\cvalign\along.txt (quickstart scenario 7)"
Task: "T018 line numbering off/on (quickstart scenario 8)"
```

---

## Implementation Strategy

### MVP (User Story 1 only)

1. Phase 1 — reproduce the defect on a fixture that makes it obvious.
2. Phase 2 — name and test the digit rule.
3. Phase 3 — guard, fix, verify.
4. **Stop and validate** against the T002 screenshot. At this point the reported
   defect is fixed and shippable; US2 and US3 add alignment polish and proof of
   durability.

### Incremental delivery

US1 → US2 → US3 → Polish. Each phase leaves the viewer in a shippable state, and
no later phase can regress an earlier one — the guards from Phases 2-4 run on
every `run_tests.cmd`.

### Notes

- Commit per task or per logical group; the four source changes (T004, T008,
  T009+T011, T020) are separately revertible by design (constitution III).
- The GUI scenarios are human steps, as in features 070 and 071 — there is no
  automated pixel check in this project, and adding one is out of scope here.
- T019 may retire T020. That is a successful outcome, not an incomplete one.

# Tasks: Pre-Release Stabilization Review (Features 058 + 059)

**Input**: Design documents from `/specs/060-prerelease-stabilization/`
**Prerequisites**: plan.md, spec.md, research.md (perspective charters R3,
verification protocol R4, gates R5), data-model.md, quickstart.md

**Tests**: The feature IS the testing — gates G1–G7 are its test tasks.

**Organization**: US1 = the six-perspective adversarial review (P1), US2 =
stability gates (P1), US3 = the release-readiness report (P2). Review
perspectives run as parallel read-only subagents (research.md R2);
verification and any fixes happen in the main context.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (independent agents / disjoint checks)
- **[Story]**: US1 = delta review, US2 = stability gates, US3 = report

## Phase 1: Setup

- [X] T001 Produce the delta manifest for the reviewers: exact commit range `b74875b..HEAD`, per-file diff extracts of the 18 delta files (research.md R1), and the seeded questions from research.md R7; store as `specs/060-prerelease-stabilization/delta-manifest.md` so every perspective agent receives identical scope

---

## Phase 2: Foundational

No blocking prerequisites beyond T001 — perspectives, gates and the report
skeleton are all independent afterwards.

---

## Phase 3: User Story 1 — Independent deep review of the delta (Priority: P1) 🎯 MVP

**Goal**: six independent perspectives over the delta, every finding
adversarially verified, confirmed release-relevant defects fixed minimally
(FR-001…FR-004).

**Independent Test**: findings table complete — every raised finding has a
failure scenario, a verdict with code evidence, and a disposition; every
code change traces to a CONFIRMED finding.

### Implementation for User Story 1

- [X] T002 [P] [US1] Run perspective P1 (memory & object lifecycle) as a read-only subagent over `src/shiconov.cpp/.h`, `src/geticon.cpp`, `src/common/handles.cpp/.h` with the R3 charter + R7 seeds; collect findings in data-model.md Finding format
- [X] T003 [P] [US1] Run perspective P2 (concurrency & thread affinity) over `src/shiconov.cpp/.h`, `src/fileswn1.cpp`, `src/snooper.cpp` with the R3 charter + R7 seeds; collect findings
- [X] T004 [P] [US1] Run perspective P3 (error & failure paths) over the whole core C++ delta with the R3 charter + R7 seeds; collect findings
- [X] T005 [P] [US1] Run perspective P4 (encoding & buffer correctness) over `src/fileswn1.cpp`, `src/snooper.cpp`, `src/geticon.cpp`, `src/shiconov.cpp` with the R3 charter; collect findings
- [X] T006 [P] [US1] Run perspective P5 (performance) over `src/shiconov.cpp`, `src/fileswn1.cpp` with the R3 charter; collect findings
- [X] T007 [P] [US1] Run perspective P6 (security / credentials / tooling) over `utils/migrate-altap-settings.cmd`, `utils/test/**`, `utils/README.md`, `tools/brand/gen_overlay_syncpend.py`, plus the cldapi load and property-value handling in `src/shiconov.cpp`, with the R3 charter + R7 password-exposure seed; collect findings
- [X] T008 [US1] Adversarially verify every finding from T002–T007 against the actual code (research.md R4 protocol): reproduce or refute each failure scenario with file:line evidence; verdict CONFIRMED/REFUTED per finding; spawn a second independent refute-first verification agent for any contested or high-impact finding; record all verdicts (depends on T002–T007)
- [X] T009 [US1] Fix every CONFIRMED release-relevant finding with a minimal change traceable to its finding ID; classify CONFIRMED out-of-delta findings per FR-004 (fix only if release-relevant, else defer with justification); zero changes without a CONFIRMED finding (depends on T008)
- [X] T010 [US1] Bounded re-verification: for each fix from T009, re-run the raising perspective's relevant check on the changed code and note the result (depends on T009; skip if T009 landed no fixes)

**Checkpoint**: findings table final; code stable per review.

---

## Phase 4: User Story 2 — Whole-product stability gates (Priority: P1)

**Goal**: gates G1–G7 green (or explicitly waived) on the post-fix code
(FR-005).

**Independent Test**: gate table in the report, each row PASS/WAIVED with
evidence.

### Implementation for User Story 2

- [X] T011 [US2] G1+G2: run `build.cmd full` and `build.cmd full release` from the repo root; both 0 errors; grep both logs for warnings in the delta files — none new; record evidence (depends on T009)
- [X] T012 [P] [US2] G3: build `src/vcxproj/saltests/saltests.vcxproj` (Debug x64), run the binary — expect `1145 checks, 0 failed`; clean up the stray output dir `src/vcxproj/saltests/tandemcommander/` afterwards (depends on T011)
- [X] T013 [P] [US2] G4: start `build\tandemcommander\Debug_x64\tandemcommander.exe`, keep alive ≥ 10 s, close gracefully (WM_CLOSE to the main window, not process kill), verify exit code 0, no new crash report in `%LOCALAPPDATA%\Tandem Commander`, no lingering salmon processes; record evidence (depends on T011)
- [X] T014 [P] [US2] G5: run `utils\test\run_migration_tests.cmd` and record the scenario results — all pass (depends on T009; independent of T011's binaries)
- [X] T015 [P] [US2] G7: scripted `LoadImage` check that `src/res/syncpend.ico` yields non-NULL icons at 16/32/48 (depends on T009)
- [X] T016 [US2] G6: determine which (if any) 058/059 user-validated scenarios are affected by T009 fixes; unaffected scenarios stand on `specs/058-*/evidence.md` + `specs/059-*/evidence.md`; affected ones get listed for the user's manual re-run and the result recorded (depends on T009)

**Checkpoint**: gate table complete.

---

## Phase 5: User Story 3 — Release-readiness record (Priority: P2)

**Goal**: single auditable report + go/no-go (FR-006, SC-005).

### Implementation for User Story 3

- [X] T017 [US3] Write `specs/060-prerelease-stabilization/review-report.md`: scope (delta manifest), method (perspectives, verification protocol), per-perspective coverage lists (SC-001), full findings table with verdicts/evidence/dispositions (SC-002), deferrals with justifications, gate table with evidence (SC-004), and the explicit go/no-go verdict (depends on T008–T016)
- [X] T018 [US3] Traceability audit per quickstart.md: every delta file appears in ≥1 coverage list; pick 3 code changes made by this feature (if any) and verify each maps to a CONFIRMED finding ID; verify no user-visible behavior changed beyond confirmed fixes (FR-007) — note any changelog impact (depends on T017)

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T019 If T009 landed fixes: clang-format + UTF-8-BOM check on changed files; update `CHANGELOG.md` `[Unreleased]` only for user-visible fixes (FR-007); final `git diff` sweep — every change traceable, no refactoring, `LAST_VERSION_OF_SALAMANDER` untouched

---

## Dependencies & Execution Order

```text
T001 ──► { T002 ∥ T003 ∥ T004 ∥ T005 ∥ T006 ∥ T007 }   (six agents in parallel)
              └──► T008 (verify all) ──► T009 (fix confirmed) ──► T010 (re-verify)
T009 ──► T011 (G1+G2) ──► { T012 ∥ T013 } ; T009 ──► { T014 ∥ T015 } ; T009 ──► T016
all gates + findings ──► T017 (report) ──► T018 (audit) ──► T019 (polish, conditional)
```

### Parallel Opportunities

- **T002–T007**: all six perspective agents launch in one batch.
- T012 ∥ T013 after the Debug build; T014 ∥ T015 independent of it.

## Implementation Strategy

US1 first and fully (findings decide everything downstream); gates run on
the post-fix state so they certify what will ship; the report last, as the
single source of truth. A clean review (zero confirmed findings) is a valid
fast path: T009/T010/T019 become no-ops, gates run on the unchanged delta.

## Notes

- Perspective agents are READ-ONLY; only the main context edits code, and
  only against CONFIRMED finding IDs (FR-003).
- The 057 harness (T014) has never been run in this session — treat a
  failure there as a finding, not merely a red gate.
- evidence trail: delta-manifest.md (T001) + review-report.md (T017) are the
  feature's persistent artifacts.

# Tasks: Cloud Sync Status Icons in File Panels

**Input**: Design documents from `/specs/058-fix-cloud-status-icons/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md,
contracts/path-encoding-icon-pipeline.md, quickstart.md

**Tests**: No automated test tasks were requested; the three fixes are
thread/UI-bound. Verification is build + existing `saltests` + the manual
quickstart scenarios (deterministic, provider-independent where possible).
Each fix task carries its own validation task.

**Organization**: One phase per user story. The three code fixes live in
three disjoint files (`fileswn1.cpp`, `snooper.cpp`+`handles.*`,
`geticon.cpp`), so the stories are fully independent and parallelizable
after Setup.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 = badges (P1), US2 = no fruitless focus loading (P1),
  US3 = existing providers keep working (P2), US4 = correct file-type icons (P1)

## Phase 1: Setup (baseline & repro fixtures)

**Purpose**: Prove the pre-fix symptoms so post-fix validation has evidence,
and establish a clean build baseline.

- [X] T001 Build the pre-fix Debug x64 baseline with `build.cmd full` from the repo root; record that it is clean (no new warnings) before touching code
- [X] T002 [P] Create repro fixtures per quickstart.md §1: `D:\Test\Zkouška` and `D:\Test\Control`, each with a copy of the same `.docx` and `.pdf`; plus a diacritic-named subfolder with a few files inside the OneDrive sync root (mark some online-only via Explorer)
- [X] T003 Capture pre-fix evidence in `specs/058-fix-cloud-status-icons/evidence.md`: (a) generic Word/PDF icons in `D:\Test\Zkouška` vs correct icons in `D:\Test\Control`; (b) debug TRACE line `Unable to receive change notifications for directory 'D:\Test\Zkouška'`; (c) busy-cursor flash on Alt-Tab with the panel on `Zkouška` / `G:\Můj disk`; (d) badges missing in the OneDrive diacritic subfolder and in `G:\Můj disk` while Explorer shows them

**Checkpoint**: symptoms reproduced and recorded — every later validation
compares against this.

---

## Phase 2: Foundational

No blocking prerequisites: all three fixes consume the existing
`SalU8ToW`/`SalU8ToWAlloc` helpers (`src/common/salunicode.h`) and touch
disjoint files. Proceed directly to the user stories (in any order, or in
parallel).

---

## Phase 3: User Story 1 — Status badges on a Google Drive mounted drive (Priority: P1) 🎯 MVP

**Goal**: Sync-status badges appear in `G:\Můj disk` (and any non-ASCII
path) exactly as Windows Explorer shows them — fixes RC1 (research.md R1/R4).

**Independent Test**: quickstart.md §2 — badges in the OneDrive diacritic
subfolder and in `G:\Můj disk` match Explorer item-by-item; on-screen items
badge within 2 s (SC-001).

### Implementation for User Story 1

- [X] T004 [P] [US1] Fix the icon-reader wide-prefix build in `src/fileswn1.cpp` (~lines 490–503): convert the UTF-8 panel-path prefix with `SalU8ToW` (fall back to the existing `MultiByteToWideChar(CP_ACP, …)` only when the prefix is not valid UTF-8, mirroring `src/shiconov.cpp:812-813`), and set `wName` to the end of the **converted wide** prefix instead of reusing the UTF-8 byte length `l` as a WCHAR offset; leave the `char* name = path + l` byte-side untouched; add a brief comment stating the UTF-8 contract (feature 004) at the conversion
- [X] T005 [US1] Re-read `CShellIconOverlays::GetIconOverlayIndex` (`src/shiconov.cpp:799-853`) against the new caller state and confirm no change is needed there (wName-relative writes, MAX_PATH guard with mixed units stays conservative); document the conclusion in a code comment only if something non-obvious was preserved
- [X] T006 [US1] Build Debug x64 (`build.cmd`) and validate per quickstart.md §2: badges appear in the OneDrive diacritic subfolder and in `G:\Můj disk`, match Explorer item-by-item (SC-001, including the 2-second on-screen timing), and update after a state change (US1 acceptance scenario 2); append results to `specs/058-fix-cloud-status-icons/evidence.md`

**Checkpoint**: badges work in non-ASCII paths — the headline defect is gone;
US1 is demonstrable on its own.

---

## Phase 4: User Story 2 — No fruitless loading on window focus (Priority: P1)

**Goal**: Auto-refresh (change monitoring) works on non-ASCII paths again, so
regaining focus no longer triggers a pointless re-list under a busy cursor —
fixes RC2 (research.md R1/R3), satisfies FR-004/FR-013/FR-010.

**Independent Test**: quickstart.md §1 rows 2–4 — the `Unable to receive
change notifications` TRACE is gone for `D:\Test\Zkouška`, externally created
files appear automatically (SC-008), and 10 focus cycles produce no
busy-cursor flash (SC-002).

### Implementation for User Story 2

- [X] T007 [P] [US2] Add a `FindFirstChangeNotificationW` overload to the HANDLES tracking layer: declaration in `src/common/handles.h` beside the ANSI one, implementation in `src/common/handles.cpp` (~line 2226) using the same `__htChangeNotification` / `__hoFindFirstChangeNotification` bookkeeping
- [X] T008 [US2] Convert the three snooper call sites in `src/snooper.cpp` — `AddDirectory` (~line 578) and both `ChangeDirectory` sites (~lines 720, 750): after `MakeCopyWithBackslashIfNeeded` (which swaps the pointer by reference — convert the **final** pointer), convert UTF-8 → UTF-16 via `SalU8ToW` (CP_ACP conversion fallback for invalid UTF-8) and call the new `HANDLES_Q(FindFirstChangeNotificationW(…))`; failure behavior (`SetAutomaticRefresh(FALSE)` + `TRACE_W`) stays unchanged (depends on T007)
- [X] T009 [US2] Build Debug x64 and validate per quickstart.md §1: no `Unable to receive change notifications` TRACE for `D:\Test\Zkouška`; `echo x> "D:\Test\Zkouška\new.txt"` appears automatically (SC-008); 10× Alt-Tab produces no busy cursor beyond the `Control` baseline (SC-002); stop/start the Google Drive client and re-enter `G:\Můj disk` to confirm listing-driven recovery with no background polling (FR-010); append results to `specs/058-fix-cloud-status-icons/evidence.md`

**Checkpoint**: activation is quiet, auto-refresh restored product-wide for
non-ASCII folders; independently demonstrable even without US1.

---

## Phase 5: User Story 4 — Correct file-type icons in affected folders (Priority: P1)

**Goal**: Word/PDF/etc. show their real icons in non-ASCII paths — fixes RC3
(research.md R1/R5), satisfies FR-012, preserves the plugin-facing
`GetFileIcon` contract (constitution V).

**Independent Test**: quickstart.md §1 row 1 — icons in `D:\Test\Zkouška`
identical to `D:\Test\Control` and Explorer (SC-007). No cloud client needed.

### Implementation for User Story 4

- [X] T010 [P] [US4] Convert `SHILCreateFromPath` in `src/geticon.cpp` (~lines 343–360) to the house pattern: try `SalU8ToWAlloc(pszPath)` first and pass the result to `ParseDisplayName`; when it returns NULL (input is not valid UTF-8 — e.g. a legacy-ACP plugin caller of `CSalamanderGeneral::GetFileIcon`), keep the existing `MultiByteToWideChar(CP_ACP, …)` branch verbatim; model the shape on `SalSHGetFileInfoIcons` in the same file (lines 64–81) and free the allocated buffer on all paths
- [X] T011 [US4] Build Debug x64 and validate: icons in `D:\Test\Zkouška` match `D:\Test\Control` and Explorer for `.docx`, `.pdf`, an image, and an `.exe` (SC-007); spot-check an archive panel and one plugin surface that retrieves icons (e.g. ZIP panel listing) to confirm no legacy-caller regression; append results to `specs/058-fix-cloud-status-icons/evidence.md`

**Checkpoint**: all three P1 stories delivered; every reported symptom fixed.

---

## Phase 6: User Story 3 — Existing providers keep working (Priority: P2)

**Goal**: Prove no regression anywhere the product already worked — OneDrive
ASCII paths, plain local folders, and the configuration switch (FR-008,
FR-009, FR-011, SC-005).

**Independent Test**: quickstart.md §3 in full.

### Verification for User Story 3

- [X] T012 [US3] Run the no-regression suite per quickstart.md §3: (a) OneDrive ASCII folder with mixed sync states — badges identical to pre-fix evidence and to Explorer (SC-005); (b) plain ASCII local folder — icons, listing speed, activation behavior unchanged (FR-009/SC-003); (c) Configuration → icon overlays disabled → no badges anywhere including `G:\Můj disk`, re-enable → badges return (FR-011); (d) provider stopped: no busy-cursor loop, no repeated probing (SC-006); record all outcomes in `specs/058-fix-cloud-status-icons/evidence.md`

**Checkpoint**: all four stories validated independently.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T013 Format the touched files (`src/fileswn1.cpp`, `src/snooper.cpp`, `src/geticon.cpp`, `src/common/handles.cpp`, `src/common/handles.h`) with the repository clang-format / `normalize.ps1` and confirm UTF-8-BOM encoding is preserved
- [X] T014 Run the automated gates per quickstart.md §4: full `build.cmd full` (Debug) and `build.cmd full release` clean, `saltests.exe` all-pass (baseline 1145/0), zero new warnings at the touched sites
- [X] T015 [P] Add a `Fixed` entry to `CHANGELOG.md` under `[Unreleased]` describing the user-visible fix in the user's terms: in folders whose path contains non-ASCII characters (e.g. Google Drive's `G:\Můj disk`), cloud sync-status badges were missing, file icons fell back to a generic icon, and the window flashed a busy cursor on every activation because automatic change monitoring was silently broken; all three were one regression from the Unicode/long-path work (feature 004). No version bump (happens at release per constitution)
- [X] T016 [P] Update `specs/058-fix-cloud-status-icons/contracts/path-encoding-icon-pipeline.md` status line to "implemented" and cross-check every governed call site listed there against the final diff (contract table must match reality)
- [X] T017 Final review sweep: `git diff` against the feature branch base — confirm the change touches only the five planned files (+ changelog/spec docs), no adjacent refactoring (constitution III), comments in English, and `LAST_VERSION_OF_SALAMANDER` untouched

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies; T001 before T003 (need a build to
  observe symptoms); T002 anytime.
- **Foundational (Phase 2)**: empty — stories start right after Setup.
- **US1 (Phase 3)**, **US2 (Phase 4)**, **US4 (Phase 5)**: independent of
  each other — disjoint files. Only internal ordering: T007 → T008 (handles
  overload before snooper conversion); T004 → T006, T008 → T009, T010 → T011
  (fix before its validation).
- **US3 (Phase 6)**: verification-only; requires Phases 3–5 complete.
- **Polish (Phase 7)**: after all stories; T013/T014 after last code change;
  T015/T016 parallel to each other; T017 last.

### Parallel Opportunities

- T002 ∥ T001 (fixtures while building).
- **T004 ∥ T007 ∥ T010** — the three root-cause fixes are in different files
  and can be implemented simultaneously.
- T008 can start as soon as T007 lands, independent of T004/T010.
- Validations T006/T009/T011 are independent of each other (one built binary
  serves all three).
- T015 ∥ T016 in Polish.

```text
Setup:        T001 ──► T003          T002 (parallel)
Stories:      T004 ──► T005 ──► T006      (US1)
              T007 ──► T008 ──► T009      (US2)   ← all three columns parallel
              T010 ─────────► T011        (US4)
Verification: T012                        (US3, after US1+US2+US4)
Polish:       T013 ──► T014 ──► (T015 ∥ T016) ──► T017
```

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 1 (Setup): baseline + fixtures + pre-fix evidence.
2. Phase 3 (US1): the RC1 one-site fix in `fileswn1.cpp` → badges appear in
   `G:\Můj disk`.
3. **STOP and VALIDATE** against Explorer (SC-001) — demonstrable MVP for the
   headline complaint.

### Incremental Delivery

Each story is a self-contained fix with its own validation; ship order
US1 → US2 → US4 → US3-verification, or implement all three fixes in parallel
(single developer: T004, T007+T008, T010 in one sitting — they never touch
the same file) and validate together. US3 + Polish close the feature.

## Notes

- The three fixes deliberately mirror existing in-repo patterns
  (`shiconov.cpp:812`, `geticon.cpp:66`) — reviewers should diff against
  those reference sites.
- No new configuration, no registry changes, no plugin interface bump.
- `evidence.md` (created in T003, appended by T006/T009/T011/T012) is the
  release-gate record for SC-001…SC-008.

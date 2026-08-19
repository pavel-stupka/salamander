# Tasks: Consistent Delete to Recycle Bin

**Input**: Design documents from `/specs/062-fix-delete-to-recycle/`
**Prerequisites**: plan.md, spec.md (Clarifications included), research.md (R0–R6, E1–E6),
data-model.md (E1–E5), contracts/delete-pipeline-contract.md (C1–C5), quickstart.md (V1–V7)

**Tests**: No new automated test tasks — the spec mandates scripted validation with the
shell Recycle Bin as ground truth (quickstart V1–V7) plus the existing saltests suite as
gates. Validation tasks are included per story, and the whole matrix is re-run on the
final build (SC-005, the user's explicit "verify repeatedly").

**Organization**: The root cause is already code-verified with ONE open runtime
assumption (research.md R0); Phase 2 confirms it on the **unfixed** build before any fix
lands — smaller than feature 061's analysis phase, but still a hard gate.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 (DEL respects the bin), US2 (SHIFT+DEL semantics), US3 (cloud folder
  delete), US4 (masks mode)
- Include exact file paths in descriptions

## Path Conventions

Single existing solution; all code changes are point fixes in `src/`. Feature documents
live in `specs/062-fix-delete-to-recycle/`. Test locations (quickstart): L1
`D:\Temp\tc062\plain`, L2 `D:\Temp\tc062\Zkouška`, L3 `E:\OneDrive - Simopt,
s.r.o\tc062test`, L4 `…\tc062test\Zkouška`.

---

## Phase 1: Setup (instrumentation & fixtures)

**Purpose**: Make the instrumented confirmation and the validation matrix executable
and repeatable.

- [X] T001 Prepare the instrumented Debug build: add `TRACE_TO_FILE` to
      `src/vcxproj/sal_debug.props` as a **local, uncommitted** change (feature-061
      precedent; revert in T017); `build.cmd`; confirm traces land in
      `%TEMP%\altap_traces_<N>.log`.
- [X] T002 [P] Create fixtures and helpers: test folders L1–L4 with fresh test files
      (incl. diacritic file names), a junction fixture (`mklink /J` to a folder with a
      canary file), and a scripted Recycle Bin checker (COM `Shell.Application`,
      namespace 0x0A, match by original path). Create
      `specs/062-fix-delete-to-recycle/analysis-report.md` skeleton with sections for
      the R0 runtime assumption, E1–E6 verdicts, and the V1–V7 matrix (two run
      columns: instrumented build / final build).

**Checkpoint**: traces visible, fixtures + bin checker ready, report skeleton exists.

---

## Phase 2: Foundational — confirm the root cause on the unfixed build (BLOCKS all fixes)

**Purpose**: Verify the one open runtime assumption before trusting the fix design
(research.md R0; quickstart V1.1). **⚠️ No fix may land before T004 is complete.**

- [X] T003 Implement the permanent gate TRACE (contract C5, decision R5) in
      `src/fileswn8.cpp` at the recycle gate (~line 370): TRACE_I of the classified
      drive type, effective `recycle` and `canUseRecycleBin`. Debug-only (TRACE
      compiles away in Release).
- [X] T004 Run the unfixed instrumented build and record in
      `specs/062-fix-delete-to-recycle/analysis-report.md`: trigger the delete gate in
      L1–L4 (DEL may be answered/cancelled — only the TRACE matters) and confirm L2/L4
      classify as non-FIXED (expected `DRIVE_NO_ROOT_DIR`) while L1/L3 classify
      `DRIVE_FIXED`. **GATE**: if the observed value contradicts the E1 mechanism,
      stop and re-plan before implementing; if it is `DRIVE_UNKNOWN`-based, adjust the
      R2 fail-safe mapping accordingly and document.

**Checkpoint**: root cause confirmed at runtime; fixes may proceed — B1/B2 (US1),
B3 (US3) and B4 (US4) touch disjoint files and can run in parallel.

---

## Phase 3: User Story 1 — DEL always respects the Recycle Bin (Priority: P1) 🎯 MVP

**Goal**: DEL sends files to the Recycle Bin in all four canonical locations with
identical dialog behavior (Explorer-grade consistency), fail-safe under classification
uncertainty.

**Independent Test**: quickstart V2 — DEL in L1–L4, 4/4 in the bin and restorable, no
popup differences; V1.2 — all four classify `DRIVE_FIXED` in the gate TRACE.

### Implementation for User Story 1

- [X] T005 [US1] Fix E1+E4 per contract C1 (decision R1) in `src/salamdr2.cpp`:
      convert `MyGetDriveType` (~:1670) and its chain —
      `ResolveLocalPathWithReparsePoints` (~:1260), `GetCurrentLocalReparsePoint`
      (~:1630), `GetReparsePointDestination` (~:1490) incl. their internal
      `GetFileAttributes`/`CreateFile`/CP_ACP conversions — to the house pattern
      (`SalU8ToW`/`SalWToU8` + CP_ACP fallback, wide WinAPI), with long-path-capable
      internal buffers (no MAX_PATH truncation). Signatures stay `char*`/UTF-8.
- [X] T006 [US1] Fix E5 per contract C2 (decision R2) in `src/fileswn8.cpp` (~:370):
      veto the Recycle Bin only for `DRIVE_REMOVABLE`/`DRIVE_REMOTE`/`DRIVE_CDROM`/
      `DRIVE_RAMDISK`; `DRIVE_FIXED` and indeterminate results allow the configured
      mode (fail-safe toward the bin, FR-005). Keep the T003 TRACE aligned with the
      new mapping.
- [X] T007 [US1] Validate quickstart V1.2 + V2 on the instrumented build and record
      in analysis-report.md: gate TRACE shows `DRIVE_FIXED` in L1–L4; DEL (mode 1)
      puts 100% of test files into the Recycle Bin, all matched by original path,
      restorable, no confirmation popup anywhere (SC-001).

**Checkpoint**: MVP — the reported defect is gone and proven in all four locations.

---

## Phase 4: User Story 2 — SHIFT+DEL stays the explicit permanent delete (Priority: P2)

**Goal**: SHIFT semantics (pure inversion) intact everywhere; permanent delete always
confirmed. Verification-only phase (the inversion code is untouched by design).

**Independent Test**: quickstart V3 in L1–L4.

- [X] T008 [US2] Validate quickstart V3 and record in analysis-report.md: SHIFT+DEL
      (or the equivalent mode-0 + DEL path, plus at least one interactive SHIFT+DEL
      smoke via SendInput) shows the confirmation and bypasses the bin in 4/4
      locations; mode 0 + SHIFT+DEL routes to the bin (inversion preserved) (SC-002).

**Checkpoint**: both gestures verified consistent across all locations.

---

## Phase 5: User Story 3 — Cloud-synced folders can be deleted (Priority: P2)

**Goal**: OneDrive placeholder folders delete on both routes; genuine links keep the
protective link-only behavior.

**Independent Test**: quickstart V5 (placeholder folder via DEL and SHIFT+DEL;
junction canary intact).

### Implementation for User Story 3

- [X] T009 [P] [US3] Fix E2 per contract C3 (decision R3): in `src/fileswn6.cpp`
      (~:1700-1722) classify a reparse directory as `ocDeleteDirLink` only when its
      reparse tag is a name surrogate (`IsReparseTagNameSurrogate`; tag available
      from directory enumeration data); non-surrogate reparse dirs are normal
      directories. Apply the same test to the prompt-wording site in
      `src/fileswn8.cpp` (~:420-432). Worker backstop (`DoDeleteDirLinkAux`) stays.
- [X] T010 [US3] Validate quickstart V5 and record in analysis-report.md: placeholder
      folder → bin via DEL; second copy → permanent via SHIFT+DEL with **no**
      "directory link" error; Explorer parity on a third copy; junction delete removes
      the junction only — canary file intact (SC-004).

**Checkpoint**: cloud folders delete like in Explorer; link protection proven intact.

---

## Phase 6: User Story 4 — The masks mode works uniformly (Priority: P3)

**Goal**: Mode 2 (by masks) recycles matching files and directly deletes the rest —
identically in all locations, including non-ASCII names.

**Independent Test**: quickstart V4 in L1, L2, L4.

### Implementation for User Story 4

- [X] T011 [P] [US4] Fix E3 per contract C4 (decision R4): factor the wide
      double-NUL-list `SHFileOperationW`+`FOF_ALLOWUNDO` helper out of
      `DeleteThroughRecycleBinAuxW` in `src/fileswn8.cpp` (~:74-158) and use it from
      the worker's per-item recycle sites in `src/worker.cpp` (~:6300-6323 file,
      ~:6932-6965 directory), replacing the ANSI `SHFileOperation` calls.
- [X] T012 [US4] Validate quickstart V4 and record in analysis-report.md: mode 2 with
      `*.txt` — `zkouška.txt` lands in the bin, `zkouška.bin` deletes directly after
      the masks-mode prompt, in L1, L2 and L4 (SC-003). Restore the user's original
      configuration (`Use Recycle Bin = 1`, original masks) afterwards.

**Checkpoint**: all three modes verified across locations.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T013 [P] Validate quickstart V6 and record in analysis-report.md: a bin-less
      location (UNC share or removable drive) still direct-deletes with the
      configured confirmation — unchanged (FR-008, SC-007).
- [X] T014 [P] Validate quickstart V7.1+V7.2 and record in analysis-report.md:
      Copy/Move smoke across L1–L4 incl. across a junction (shared classification
      chain, FR-010); feature-061 overlay smoke (TortoiseGit badges + cloud badges
      still show).
- [X] T015 Remove temporary analysis instrumentation if any beyond the permanent gate
      TRACE (T003 stays per contract C5); revert `src/vcxproj/sal_debug.props`
      (T001); verify with `git diff` that the delta contains only
      `src/salamdr2.cpp`, `src/fileswn8.cpp`, `src/fileswn6.cpp`, `src/worker.cpp`
      (+ any shared-helper declaration in an existing header).
- [X] T016 [P] Run formatting per constitution (`clang-format` x64 from VS2022) on all
      touched source files; UTF-8-BOM preserved.
- [X] T017 Final-build matrix re-run (SC-005): rebuild Debug (`build.cmd`), re-run
      V2–V5 on the cleaned build, and fill the second run column in
      `specs/062-fix-delete-to-recycle/analysis-report.md`; finalize the report as
      the FR-001 deliverable (every E1–E6 item CONFIRMED/REFUTED with evidence,
      pipeline narrative, Explorer-parity notes).
- [X] T018 Add a `CHANGELOG.md` entry (Fixed) under Unreleased, in user terms: DEL no
      longer permanently deletes in folders with non-ASCII characters (incl. OneDrive
      trees) — files go to the Recycle Bin as configured; OneDrive folders delete
      without the spurious "directory link" error; the masks mode handles non-ASCII
      names; classification failures now fail toward the Recycle Bin. Truthful about
      the deliberate fail-safe behavior change. No version bump (lands with the
      release change per constitution).
- [X] T019 Run gates and record in analysis-report.md: `build.cmd rebuild` (Debug) and
      `build.cmd full release` clean; saltests 0 new failures (SC-006).

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: T001 ∥ T002.
- **Foundational (Phase 2)**: needs Phase 1; T003 → T004. **BLOCKS Phases 3–6.**
- **US1 (Phase 3)**: needs T004. T005 → T006 (same feature area; T006 depends on the
  chain returning correct types) → T007.
- **US2 (Phase 4)**: needs Phase 3 complete (verifies the fixed gate); T008.
- **US3 (Phase 5)**: needs T004 only — T009 is parallel with Phase 3 code (disjoint
  files `fileswn6.cpp` vs `salamdr2.cpp`; the small `fileswn8.cpp` wording site is
  disjoint from T006's gate lines — coordinate the merge); T010 after T009 + T007.
- **US4 (Phase 6)**: needs T004 only — T011 parallel with T005/T009 (files
  `worker.cpp` + `fileswn8.cpp` helper; coordinate with T006 in `fileswn8.cpp`);
  T012 after T011 + T007.
- **Polish (Phase 7)**: after all stories; T013 ∥ T014, T016 ∥ (T015), then
  T017 → T018 → T019.

### User Story Dependencies

- US1 (P1): only on Foundational. **MVP.**
- US2 (P2): verification-only; depends on US1's fixes being in.
- US3 (P2): code independent of US1 (different defect); validation uses the fixed
  gate, so run T010 after T007.
- US4 (P3): code independent; validation after T007.

### Parallel Opportunities

- Phase 1: T001 ∥ T002.
- After T004: **T005 ∥ T009 ∥ T011** (three disjoint code fixes; the two small
  `fileswn8.cpp` touches — T006 gate, T009 wording, T011 helper — are separate
  regions of one file: apply sequentially within that file).
- Phase 7: T013 ∥ T014 ∥ T016.

## Parallel Example: after the T004 gate

```text
Task: "T005 [US1] classification chain → wide (src/salamdr2.cpp)"
Task: "T009 [US3] surrogate-tag item nature (src/fileswn6.cpp)"
Task: "T011 [US4] wide per-item recycle helper (src/worker.cpp + fileswn8.cpp helper region)"
# then T006 (gate, fileswn8.cpp) once T005 is in
```

## Implementation Strategy

### MVP First (US1 only)

1. Phases 1–2 (fixtures + unfixed-build confirmation — cheap but mandatory).
2. Phase 3 (US1): chain fix + fail-safe gate, validate V1.2+V2. **STOP and VALIDATE**
   — this alone removes the data-loss defect the user reported.
3. Then US2 verification, US3/US4 fixes+verification, Polish with the final-build
   matrix re-run.

### Incremental Delivery

Each fix (T005, T006, T009, T011) is independently revertible (constitution III) with
its own validation; any can be dropped without breaking the others.

---

## Notes

- The T004 gate protects against fixing the wrong rung — the single unobserved step
  of the mechanism is confirmed on the unfixed build first (user's "verify
  repeatedly" + constitution III).
- All validation writes into `analysis-report.md` with TWO run columns (instrumented
  and final build) — SC-005 requires the matrix re-run after cleanup.
- Registry state (delete mode, masks) is restored after every scenario that changes
  it; test deletions only ever touch dedicated `tc062` fixtures.
- Commit after each task or logical group; keep `sal_debug.props` instrumentation out
  of commits (T001/T015).

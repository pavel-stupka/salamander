# Tasks: Restore General Shell Icon Overlay Support

**Input**: Design documents from `/specs/061-fix-icon-overlays/`
**Prerequisites**: plan.md, spec.md (Clarifications included), research.md (R0–R7, D1–D5),
data-model.md (E1–E5), contracts/overlay-pipeline-contract.md (C1–C5), quickstart.md (V1–V7)

**Tests**: No automated test tasks — the spec mandates manual validation against File
Explorer (quickstart V1–V7) plus the existing saltests suite as gates. Validation tasks
are included per story instead.

**Organization**: The defining constraint from plan.md: **Phase A (instrumented
analysis) is Foundational and blocks all fixes** — the primary defect D3 has no
confirmed root cause yet (research.md R7), and FR-001 makes the analysis a deliverable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 (TortoiseGit badges), US2 (live refresh), US3 (cloud regression
  guard), US4 (user control)
- Include exact file paths in descriptions

## Path Conventions

Single existing solution; all code changes are point fixes in `src/` of the repository
root. Feature documents live in `specs/061-fix-icon-overlays/`.

---

## Phase 1: Setup (instrumentation & fixtures)

**Purpose**: Make the instrumented analysis executable and repeatable.

- [X] T001 Prepare an instrumented Debug build: `build.cmd full` (Debug x64); enable
      TRACE capture — either attach the Trace Server, or add `TRACE_TO_FILE` to the
      Debug preprocessor defines in `src/vcxproj/sal_debug.props` as a **local,
      uncommitted** change (revert before any commit). Confirm `InitShellIconOverlays`
      TRACE lines are being captured.
- [X] T002 [P] Create test fixtures and the analysis-report skeleton: Git working
      copies with clean/modified/untracked (and one conflicted) files at an ASCII path
      (e.g. `D:\Temp\tc061\repo`) and a non-ASCII path (`D:\Temp\Zkouška\repo`);
      verify TortoiseGit is installed and Explorer shows badges somewhere. Create
      `specs/061-fix-icon-overlays/analysis-report.md` with sections for A1–A7,
      D1–D5, S1–S7 verdicts and fixture paths recorded.

**Checkpoint**: traces visible, fixtures ready, report skeleton exists.

---

## Phase 2: Foundational — instrumented analysis (Phase A; BLOCKS all user stories)

**Purpose**: Name the active D3 blocker with evidence and give every suspect a
CONFIRMED/REFUTED verdict (FR-001, SC-004). **⚠️ No fix beyond T003's debug trace may
land before T008 is complete.**

- [X] T003 Implement the consolidated slot-table TRACE (contract C5.2) in
      `src/shiconov.cpp`: after `InitShellIconOverlays()` finishes, one `TRACE_I`
      block listing every slot (index, handler name incl. leading spaces, priority)
      plus the synthetic-entry status; audit that every load-ladder rejection already
      traces name+reason (C5.1) and fill gaps. Debug-only (TRACE compiles away in
      Release) — this is fix D4 and it powers all following analysis tasks.
- [X] T004 Run analysis items A1+A2 on the reference machine and record in
      `specs/061-fix-icon-overlays/analysis-report.md`: does the slot table match the
      research.md R1 prediction (OneDrive×7, GoogleDrive×4, Tortoise Normal/Modified/
      Conflict/Deleted; Locked/ReadOnly skipped by the shim; Added+ refused by cap)?
      Is Tortoise `IsMemberOf` invoked for working-copy items and what does the first
      ask return? Temporary instrumentation in `src/shiconov.cpp` /
      `src/fileswn1.cpp` is allowed and must be tagged for removal (T018).
- [X] T005 Run analysis items A3+A4 and record in analysis-report.md: does
      TortoiseGit's `SHCNE_UPDATEITEM` reach the `CMainWindow` notification handler
      (`src/mainwnd3.cpp:1380-1422`); which gate in `IconOverlaysChangedOnPath`
      (`src/fileswn7.cpp:2083-2121`) passes/fails on an ASCII path; does the re-woken
      icon reader actually re-run the overlay pass (IconOverlayDone reset)?
- [X] T006 Run analysis items A5+A6 and record in analysis-report.md: TortoiseGit
      environmental gates (overlays-only-in-Explorer-type settings, TGitCache serving
      a non-Explorer client, elevation of the TC process) and the Explorer
      ground-truth badge set on the same working copy (this fixes the SC-001 floor).
- [X] T007 Run analysis item A7 and record in analysis-report.md: non-ASCII-path
      variant — initial listing vs. refresh behavior (expected: D1 kills only the
      refresh); also verify whether any real handler's icon path is non-ACP-
      representable (D5 impact check).
- [X] T008 Write the defect disposition in
      `specs/061-fix-icon-overlays/analysis-report.md`: name the active D3 root cause
      with evidence; design its fix (mechanism, exact files/functions, risk assessment
      per constitution III); give every item S1–S7, D1–D5, A1–A7 a CONFIRMED/REFUTED
      verdict. **GATE**: if the D3 fix would require a plugin-ABI or
      configuration-layout change, STOP and re-plan (constitution II/V) before any
      implementation.

**Checkpoint**: analysis-report.md names the blocker; fixes may now proceed — US1, US2
touch disjoint files and can run in parallel.

---

## Phase 3: User Story 1 — Version-control status badges appear (Priority: P1) 🎯 MVP

**Goal**: TortoiseGit badges show in panels (Explorer-as-floor), including on profiles
healed per FR-009.

**Independent Test**: quickstart V1+V2 (slot table + badge parity vs Explorer on the
ASCII and non-ASCII fixtures) and V5 (config healing) pass without touching US2's code.

### Implementation for User Story 1

- [X] T009 [US1] Implement the D3 fix exactly as designed in analysis-report.md
      (expected location per plan.md: `src/shiconov.cpp` and/or `src/fileswn1.cpp`;
      the report's design section names the actual files/functions). Keep the change
      minimal and independently revertible (constitution III).
- [X] T010 [P] [US1] Implement D2 config healing per contract C4 in
      `src/mainwnd2.cpp` (`LoadIconOvrlsInfo`, lines ~2345-2349): absent overlay
      values ⇒ factory default (enabled, none disabled) regardless of config version;
      keep the conservative force-disable for stored-but-unreadable values (FR-009).
- [X] T011 [P] [US1] Apply the D5 verdict from T007: if CONFIRMED, convert the overlay
      icon-path handling in `src/shiconov.cpp` (load at `:263`, `ColorsChanged` at
      `:991`) to the wide/UTF-8 house pattern with CP_ACP fallback; if REFUTED, record
      no-change in analysis-report.md and close D5.
- [X] T012 [US1] Validate quickstart V1+V2+V5 and record results in
      analysis-report.md: slot table matches R1 (SC-004 evidence); 100% of
      Explorer-badged items badged in TC on ASCII and non-ASCII initial listings
      (SC-001, floor semantics — TC-only extras documented); deleting the two HKCU
      overlay values → overlays ON at next start, stored `Enable=0` → stays OFF
      (SC-007).

**Checkpoint**: MVP — TortoiseGit badges visible and config healing verified,
independent of US2.

---

## Phase 4: User Story 2 — Badges stay current as file status changes (Priority: P2)

**Goal**: The designed async re-ask loop (Tortoise: S_FALSE first, `SHCNE_UPDATEITEM`
later) works on every panel path, regardless of characters in it.

**Independent Test**: quickstart V3 — modify/revert a file externally; badge flips
without manual refresh on both fixtures.

### Implementation for User Story 2

- [X] T013 [US2] Fix D1 per contract C3 in `src/mainwnd3.cpp` (notification handler,
      `:1380-1422`): produce the shell-change-notification path via the wide API
      (`SHGetPathFromIDListW`) and convert with `SalWToU8` (CP_ACP fallback) before it
      reaches `IconOverlaysChangedOnPath`; coalescing semantics (200 ms window,
      deferral while icons load) unchanged.
- [X] T014 [US2] Audit every other consumer of the same notification-path buffer in
      the `src/mainwnd3.cpp` handler for ANSI-vs-UTF-8 comparisons against
      feature-004 panel state (same defect class as feature 058); fix confirmed
      mismatches with the same pattern and record a per-site verdict in
      analysis-report.md.
- [X] T015 [US2] Validate quickstart V3 (SC-002) and record in analysis-report.md:
      external modify → "modified" badge without manual refresh (latency compared
      against Explorer); `git checkout -- <file>` → clean badge; identical behavior
      on the ASCII and non-ASCII fixtures.

**Checkpoint**: US1 and US2 both work independently; all planned code changes are in.

---

## Phase 5: User Story 3 — Existing cloud badges keep working (Priority: P2)

**Goal**: Zero regressions in the feature 058/059 behavior (the user's hard
constraint). Verification-only phase — **run after all code-changing phases (3, 4)**.

**Independent Test**: quickstart V4 re-runs the 058/059 acceptance scenarios on the
fixed build.

- [X] T016 [US3] Validate quickstart V4 (SC-003) and record in analysis-report.md:
      Google Drive `G:\Můj disk` — sync badges, real file icons, auto-refresh without
      busy-cursor relist (058); OneDrive — synced/pending/error badges incl. the
      sync-pending blue-arrows badge (059); synthetic-entry self-disable TRACE when
      the slot table is full (on the reference machine it is exactly full, R1).

**Checkpoint**: cloud behavior byte-for-byte equivalent to pre-change.

---

## Phase 6: User Story 4 — User control over overlay providers (Priority: P3)

**Goal**: The existing control surface keeps its semantics and reflects reality.

**Independent Test**: quickstart V6 on the built binary; no code change expected in
this phase (config page code is untouched by design).

- [X] T017 [US4] Validate quickstart V6 (SC-005) and record in analysis-report.md:
      Configuration → Icon Overlays lists the detected providers (incl. Tortoise
      entries, names with leading spaces displayed as today); uncheck one → OK →
      restart → its badge family gone, others intact; re-enable → restart → badges
      return; crash-disable escape hatch behavior unchanged (inspection).

**Checkpoint**: all four user stories independently validated.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T018 [P] Remove all temporary analysis instrumentation added in T004–T007 (keep
      only the contract-C5 slot-table trace from T003); verify with `git diff` that
      the delta contains only the intended changes in `src/shiconov.cpp`,
      `src/mainwnd2.cpp`, `src/mainwnd3.cpp` (+ files named by the D3 design) and
      `src/vcxproj/sal_debug.props` is reverted (T001).
- [X] T019 [P] Run formatting per constitution Development Workflow (`clang-format` /
      `normalize.ps1`) on all touched source files; UTF-8-BOM preserved.
- [X] T020 Finalize `specs/061-fix-icon-overlays/analysis-report.md` as the FR-001
      deliverable: end-to-end narrative (discovery → selection under the limit →
      per-item query → refresh → configuration), every candidate cause CONFIRMED or
      REFUTED with evidence (SC-004), Explorer-floor deviations documented.
- [X] T021 Add a `CHANGELOG.md` entry (Fixed) for the next unreleased version, in
      user terms: third-party overlay badges (e.g. TortoiseGit) restored; badge
      refresh fixed on paths with non-ASCII characters; profiles migrated from Altap
      no longer start with overlays silently disabled. Truthful about remaining
      platform limits (states sacrificed system-wide by TortoiseOverlays crowding).
      No version bump here — that lands with the release change per constitution.
- [X] T022 Run quickstart V7 gates and record in analysis-report.md: `build.cmd
      rebuild` (Debug) and `build.cmd full release` both clean; saltests 0 new
      failures; FR-008 inspection — shortcut arrow / shared hand / offline clock
      still render for items without provider badges (SC-006).

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies; T001 ∥ T002.
- **Foundational (Phase 2)**: needs Phase 1. T003 first (powers the analysis);
  T004→T005→T006→T007 run on the same instrumented binary (sequential on one
  machine); T008 consumes all of them. **BLOCKS Phases 3–6.**
- **US1 (Phase 3)**: needs T008 (D3 design + D5 verdict). T009 ∥ T010 ∥ T011
  (disjoint files); T012 after all three.
- **US2 (Phase 4)**: needs T008 only — **can run fully in parallel with Phase 3**
  (disjoint file `src/mainwnd3.cpp`); T013→T014→T015.
- **US3 (Phase 5)**: verification of the whole delta — after Phases 3 and 4.
- **US4 (Phase 6)**: after Phase 3 (needs loaded Tortoise entries to exercise the
  page); independent of Phase 4.
- **Polish (Phase 7)**: after all stories; T018 ∥ T019, then T020→T021→T022.

### User Story Dependencies

- US1 (P1): only on Foundational. **MVP.**
- US2 (P2): only on Foundational; shares no files with US1.
- US3 (P2): verification-only; depends on US1+US2 code being final.
- US4 (P3): verification-only; depends on US1 (needs Tortoise entries in the table).

### Parallel Opportunities

- Phase 1: T001 ∥ T002.
- After T008: `{T009, T010, T011}` within US1 ∥ `T013` (US2) — four disjoint-file
  code tasks can be in flight at once.
- Phase 7: T018 ∥ T019.

## Parallel Example: after the T008 gate

```text
# All four code fixes touch disjoint files and can start together:
Task: "T009 [US1] D3 fix per analysis-report.md (src/shiconov.cpp / src/fileswn1.cpp)"
Task: "T010 [US1] D2 config healing (src/mainwnd2.cpp)"
Task: "T011 [US1] D5 icon-path encoding if confirmed (src/shiconov.cpp — coordinate with T009 if same file)"
Task: "T013 [US2] D1 notification encoding (src/mainwnd3.cpp)"
```

(If T009's design lands in `src/shiconov.cpp`, run T011 after T009 — same file.)

## Implementation Strategy

### MVP First (US1 only)

1. Phases 1–2 (instrumented analysis — the non-negotiable core of this feature).
2. Phase 3 (US1): D3 + D2 (+D5) fixes, validate V1/V2/V5. **STOP and VALIDATE** — this
   alone delivers the reported value (TortoiseGit badges visible).
3. Deliver/demo; then US2 (refresh), then verification stories US3/US4, then Polish.

### Incremental Delivery

Each fix (T009, T010, T011, T013) is a small, independently revertible change
(constitution III) with its own validation task; any of them can be dropped without
breaking the others if verification uncovers a problem.

---

## Notes

- The T008 gate is the safety valve for the spec's hard no-regression constraint: no
  speculative fixing of D3 before its root cause is named with evidence.
- All validation tasks write their results into `analysis-report.md` — one artifact
  carries FR-001 (analysis), SC-004 (verdicts) and the acceptance evidence.
- Commit after each task or logical group; keep `sal_debug.props` instrumentation out
  of commits (T001/T018).

# Tasks: Sync-In-Progress Badge Parity with Explorer

**Input**: Design documents from `/specs/059-fix-onedrive-syncing-badge/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md,
contracts/cloud-state-badge-fallback.md, quickstart.md

**Tests**: No automated test tasks requested; the fallback is thread/shell
bound. Verification = build + saltests unchanged-pass + the deterministic
manual repro (pause OneDrive → pending badges) from quickstart.md.

**Organization**: US1 carries the whole implementation; US2 is the recorded
diagnosis (analysis already performed in research.md); US3 is the
no-regression suite over feature 058.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no pending dependencies)
- **[Story]**: US1 = syncing-badge parity (P1), US2 = diagnosis record (P2),
  US3 = no regression of feature 058 (P2)

## Phase 1: Setup

- [X] T001 Build the pre-change Debug x64 baseline with `build.cmd` from the repo root (must be clean); confirm the deterministic repro works on this machine: pause OneDrive syncing from the tray, touch a file in the sync root, verify Explorer shows the blue pending arrows on the file and its parent folder, then resume syncing (quickstart.md §1 steps 1–3)

---

## Phase 2: Foundational

No blocking prerequisites — the icon asset and the code changes all belong to
US1 and touch disjoint files. Proceed directly.

---

## Phase 3: User Story 1 — Sync-in-progress badge matches Explorer (Priority: P1) 🎯 MVP

**Goal**: items (files AND folders) that Explorer marks with the blue pending
arrows get the same badge in panels — via the `PKEY_StorageProviderState`
fallback defined in contracts/cloud-state-badge-fallback.md.

**Independent Test**: quickstart.md §1–§2 — pause-syncing repro gives
item-by-item parity with Explorer (SC-001) and the resume cycle follows
transitions (SC-002).

### Implementation for User Story 1

- [X] T002 [P] [US1] Create the sync-pending overlay icon: original blue circular-arrows badge artwork in the Windows badge style (NOT a copy of Microsoft's icon), sizes 16/32/48 in one `src/res/syncpend.ico`; add the source to `tools/brand/` per its README conventions and register the icon in the main app resource script (`src/salamand.rc`/`salamand.rc2` — model on the existing `googledr.ico` entry); note the asset in `tools/brand/README.md`
- [X] T003 [US1] Extend the overlay registry in `src/shiconov.h`: `GetIconOverlayIndex` gains a `BOOL isCloudSyncRootPath` parameter (beside `isGoogleDrivePath`); add `InitCloudSyncPendingOverlay()` (called from `InitShellIconOverlays`), a stored synthetic-entry index (`int CloudSyncPendingIndex`, -1 = absent/disabled), and a static `IsCloudSyncRootPath(const WCHAR* wPath)` helper declaration (cldapi.dll loaded dynamically once, per research R5)
- [X] T004 [US1] Implement in `src/shiconov.cpp`: (a) `InitCloudSyncPendingOverlay` — load `syncpend.ico` 16/32/48 from salamand resources into a `CShellIconOverlayItem` with `Identifier = NULL` and name `TandemCloudSyncPending`, skip when `IsDisabledCustomIconOverlays("TandemCloudSyncPending")`, append after registry handlers and record its index; also add a `CShellIconOverlayItem2` row to `ListOfShellIconOverlays` so the configuration page lists the name; (b) guard NULL `Identifier` in `ColorsChangedAuxAux`/`ColorsChanged` (reload the synthetic entry's icons from resources there instead of calling COM) — `CreateIconReadersIconOverlayIds`/`GetIconOverlayIndexAuxAux` already tolerate NULL slots; (c) `IsCloudSyncRootPath` via dynamically loaded `CfGetSyncRootInfoByPath`; (d) the property fallback at the end of `GetIconOverlayIndex`: when no handler claimed the item AND `isCloudSyncRootPath` AND `CloudSyncPendingIndex >= 0`, call `SHGetPropertyStoreFromParsingName(wPath, NULL, GPS_DELAYCREATION | GPS_BESTEFFORT, …)`, read `INIT_PKEY_StorageProviderState` (propkey.h constant; accept only `vt == VT_UI4`, read `ulVal` directly), map `{4,5,6,10}` → `CloudSyncPendingIndex`, everything else/failure → `ICONOVERLAYINDEX_NOTUSED`; SEH-guard like the neighboring handler calls (depends on T002, T003)
- [X] T005 [US1] Plumb the gate in `src/fileswn1.cpp` icon reader: next to `isGoogleDrivePath` (~line 502, after the feature-058 wide-prefix conversion) compute `BOOL isCloudSyncRootPath = ShellIconOverlays.IsCloudSyncRootPath(wPath)` once per work cycle and pass it to the `GetIconOverlayIndex` call (~line 694+) (depends on T003)
- [X] T006 [US1] Build Debug x64 (`build.cmd`) and validate per quickstart.md §1–§2: pause-syncing repro shows the pending badge on the touched file AND its parent folder matching Explorer item-by-item (SC-001, 2 s on-screen target); resume cycle transitions match on notification or manual refresh — record which mechanism fired (SC-002, research R7); append results to `specs/059-fix-onedrive-syncing-badge/evidence.md` (depends on T004, T005)

**Checkpoint**: the missing badge state displays; parity with Explorer
demonstrated on demand.

---

## Phase 4: User Story 2 — Diagnosis of the reported location (Priority: P2)

**Goal**: the user's "is my OneDrive stuck?" question answered on the record,
re-runnably (FR-006/SC-005). The analysis itself was completed during
planning (research.md R1/R2); this phase persists it as the feature's
evidence.

**Independent Test**: `evidence.md` names the channel, the observed states,
the stall verdict, and user guidance; each claim points at a re-runnable
probe.

### Implementation for User Story 2

- [X] T007 [P] [US2] Create `specs/059-fix-onedrive-syncing-badge/evidence.md` with the diagnosis record: channel = `PKEY_StorageProviderState` (all seven OneDrive handlers returned S_FALSE on the pending folder); observed states (folder 10 PENDING_UNSPECIFIED, document 4 PENDING_UPLOAD, control 3 PINNED); verdict — the stall was provider-real and drained on its own during analysis; user guidance for future stalls (OneDrive activity center, close the app holding the document — Word locks defer uploads, pause/resume); reference research.md R1/R2 for the re-runnable probes and leave placeholders for T006/T008 results

**Checkpoint**: SC-005 satisfied.

---

## Phase 5: User Story 3 — No regression of feature-058 badge behavior (Priority: P2)

**Goal**: everything feature 058 delivered still behaves identically
(SC-003), plus the stalled-provider endurance check (SC-004).

**Independent Test**: quickstart.md §3–§4 in full.

### Verification for User Story 3

- [X] T008 [US3] Run quickstart.md §3–§4 with the fixed build: (a) feature-058 evidence scenarios re-pass — `G:\Můj disk` badges (fallback inert on non-CFAPI root), OneDrive ASCII + diacritic folders, base icons, auto-refresh, activation; (b) configuration toggles — global overlays off hides all badges including the new one, `TandemCloudSyncPending` in the per-handler disable list hides only the new badge; (c) plain local folder unchanged; (d) endurance: syncing paused ~1 h with pending badges showing — flat CPU/memory, no busy cursor, badges clear after resume (SC-004); append all results to `specs/059-fix-onedrive-syncing-badge/evidence.md` (depends on T006)

**Checkpoint**: all three stories validated.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T009 Format touched files (`src/shiconov.h`, `src/shiconov.cpp`, `src/fileswn1.cpp`, resource script) with the repository clang-format and verify UTF-8-BOM preserved
- [X] T010 Run automated gates: `build.cmd full` (Debug) and `build.cmd full release` both clean, zero new warnings at touched sites; rebuild and run `saltests` (`src/vcxproj/saltests/saltests.vcxproj`, Debug x64) — baseline all-pass
- [X] T011 [P] Add the `CHANGELOG.md` `[Unreleased]` entry (Fixed): the sync-in-progress (blue arrows) badge now displays in cloud-synced folders as in Explorer, including on folders whose contents are pending — a state never shown before, even prior to the Open Salamander fork; no version bump (release-time per constitution)
- [X] T012 [P] Update `specs/059-fix-onedrive-syncing-badge/contracts/cloud-state-badge-fallback.md` status to "implemented" and cross-check every rule (precedence, gate, flags, mapping, threading, configuration, artwork) against the final diff
- [X] T013 Final review sweep: `git diff` — only the planned files touched (shiconov.h/.cpp, fileswn1.cpp, resource script + icon, brand assets, changelog, spec docs), no adjacent refactoring, English comments, `LAST_VERSION_OF_SALAMANDER` untouched, handler precedence code path provably unchanged for claimed items

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (T001)**: first — baseline + repro sanity.
- **US1**: T002 ∥ T003 → T004 → (T005 needs only T003) → T006.
- **US2 (T007)**: independent of US1 code — can be written any time after
  planning ([P] with T002–T005).
- **US3 (T008)**: needs T006 (fixed build validated).
- **Polish**: T009/T010 after last code change; T011 ∥ T012; T013 last.

### Parallel Opportunities

- **T002 (icon) ∥ T003 (header) ∥ T007 (evidence)** — disjoint files.
- T005 can start once T003 lands, in parallel with T004's fallback body
  (same feature but different files: fileswn1.cpp vs shiconov.cpp).
- T011 ∥ T012 in Polish.

```text
T001 ──► { T002 ∥ T003 ∥ T007 }
              T003 ──► T004 ──► T006 ──► T008 ──► T009 ──► T010 ──► (T011 ∥ T012) ──► T013
              T003 ──► T005 ──┘
              T002 ──► T004
```

## Implementation Strategy

### MVP First (User Story 1 Only)

T001 → T002/T003 → T004/T005 → T006: the badge appears and matches Explorer
on the pause-syncing repro — demonstrable MVP. US2 is a documentation
increment; US3 closes the regression gate before polish.

### Notes

- The synthetic entry's `Identifier == NULL` is the single sharp edge —
  T004(b) exists because `ColorsChangedAuxAux` dereferences `Identifier`
  unconditionally today; the reader-array and `IsMemberOf` paths already
  tolerate NULL (verified in research).
- All new API usage is documented-Windows-SDK only; no new link-time
  dependencies (`cldapi.dll` via `LoadLibrary`, PKEY as compile-time
  constant).
- `evidence.md` (T007, appended by T006/T008) is the release-gate record for
  SC-001…SC-005.

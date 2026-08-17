# Tasks: Altap Salamander Settings Migration Utility

**Input**: Design documents from `specs/057-altap-settings-migration/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md,
contracts/category-mapping.md, contracts/wizard-flow.md, quickstart.md

**Tests**: Included — the plan's Technical Context defines the scripted
harness (`utils/test/run_migration_tests.cmd`) as part of the deliverable
(research R11); each story lands with its harness scenarios.

**Organization**: Tasks are grouped by user story. Note the structural
reality: almost all implementation lives in ONE file
(`utils/migrate-altap-settings.cmd`), so script tasks within and across
phases are sequential by design; only fixtures, README, and harness files
parallelize.

## Format: `[ID] [P?] [Story] Description`

## Phase 1: Setup

**Purpose**: Skeletons of the three deliverable areas.

- [X] T001 Create `utils/migrate-altap-settings.cmd` polyglot skeleton: ASCII-only, no BOM; batch header that relaunches the embedded PowerShell 5.1 payload with `-NoProfile -ExecutionPolicy Bypass` and forwards the exit code; PS payload with: W1 identification banner, stdin-safe prompt helper (works under redirected stdin, 3-invalid-inputs abort → exit 2), exit-code constants (0/2/3/10/11/12/13), test-only env overrides `TCMIG_SOURCE_ROOT`/`TCMIG_DEST_ROOT`/`TCMIG_OUT_DIR`/`TCMIG_SKIP_PROCCHECK`, and output-dir resolution (script dir → `%USERPROFILE%\Documents` fallback) per contracts/wizard-flow.md
- [X] T002 [P] Create `utils/README.md` skeleton: what the utility does, one-file download instruction, how to run (double-click), placeholder sections for categories/restore/master-password notes
- [X] T003 [P] Create `utils/test/run_migration_tests.cmd` harness skeleton: scratch-root setup/teardown under `HKCU\Software\TCMigTest\`, fixture import via `reg import`, wizard invocation with env overrides + stdin answer files, assertion helpers (registry export byte-compare, exit-code check, summary-text grep), PASS/FAIL reporting, non-zero exit on any FAIL

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The copy engine, source model, category table, and write-safety
core that every story's transfer path uses. All in
`utils/migrate-altap-settings.cmd` — sequential.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T004 Implement the type-exact registry engine in `utils/migrate-altap-settings.cmd` using `Microsoft.Win32.Registry` (never the PS provider, never ANSI APIs): open-read-only source access, recursive subtree copy preserving all value kinds byte-for-byte (REG_SZ/EXPAND_SZ/MULTI_SZ/DWORD/QWORD/BINARY), recursive subtree delete, per-value name/data filter hooks and transform hooks (research R1)
- [X] T005 Embed the source scan list and source model in `utils/migrate-altap-settings.cmd`: the ~79 historical roots verbatim from pre-032 `SalamanderConfigurationRoots[]` (`git show 3945ecf:src/mainwnd2.cpp`, excluding `Software\Open Salamander\5.0`; exact literals incl. `"(DB 72)"`-style spaces); source qualification (root exists AND has `Configuration` subkey); `Version\Configuration` read with TC's fallback rule (no key ⇒ 1, no value ⇒ 2); generation classification AS4/AS3/AS25/Ancient; root last-write timestamp (research R2, R3)
- [X] T006 Encode the category table in `utils/migrate-altap-settings.cmd` exactly per contracts/category-mapping.md: 11 categories with Id, display name, source/destination scopes, per-generation verdicts, item-count rules, transform/filter/exclude references (T1, T2, F1, F2, P1, X-CONFIG, X-VIEWER, PLUGSET), plus the never-copy list and the not-offered reasons (packers, session state, foreign plugins, Internal ZIP Packer)
- [X] T007 Implement destination integrity + backup-before-write in `utils/migrate-altap-settings.cmd`: `reg.exe export` of the entire destination root to `tc-settings-backup-<timestamp>.reg` before the first write, recording whether the root pre-existed; export failure ⇒ abort exit 12 with zero writes; ensure `Version\Configuration = 105` (create, never lower, never copy) and ensure `Configuration` subkey exists on virgin destinations; hard guards that no code path ever writes `Save In Progress`/`AutoImportConfig`/`Copy Is OK` or opens any source key writable (research R4, R10; category-mapping "Destination integrity rules")

**Checkpoint**: Engine + data model ready — story phases can begin.

---

## Phase 3: User Story 1 — Selective one-shot settings transfer (Priority: P1) 🎯 MVP

**Goal**: Detect a source, show the category checklist with item counts,
transfer the selected categories, print the per-category summary.

**Independent Test** (spec US1): with an AS 4.0 fixture and empty
destination, select only hot paths + FTP bookmarks; both appear mapped in
the destination, nothing else is written, summary accounts for everything
(quickstart S1, S8).

- [X] T008 [US1] Implement single-source W3 + presence scan in `utils/migrate-altap-settings.cmd`: pick newest qualifying source (full multi-source UI comes in US3), compute per-category `Presence` (Present(count)/Empty/Absent) and `Offered` per the mapping contract's count rules
- [X] T009 [US1] Implement W4 category checklist in `utils/migrate-altap-settings.cmd`: numbered toggles, A/N/D commands, empty-category lock ("(empty — nothing to transfer)"), "Not transferable from this source" listing with reasons, zero-selection exit 3 (contracts/wizard-flow.md W4)
- [X] T010 [US1] Implement W5 + W6 in `utils/migrate-altap-settings.cmd`: announce backup/restore/summary paths before confirmation; explicit `Continue? [y/N]` default-No gate; decline ⇒ exit 3 with zero writes (FR-012)
- [X] T011 [US1] Implement the core-category transfer executor in `utils/migrate-altap-settings.cmd`: per selected category delete destination scope then copy per mapping — `hotpaths` (slots 0–30, transform T1 when cfg<47), `usermenu`, `viewers-editors` (filter F1 drop Type<0 and `|`-masks + renumber, transform T2 when cfg<44), `confirmations`, `colors` (incl. `Color Scheme` value + `Panel Items Hilighting`), `viewtemplates`, `viewer-settings` (X-VIEWER excludes), `defaultdirs` (filter F2), `general-config` (X-CONFIG excludes, preserve destination `Theme Mode` on delete); W7 progress lines; per-item skip recording
- [X] T012 [US1] Implement plugin-config transfer in `utils/migrate-altap-settings.cmd`: source↔destination `Configuration Key` resolution by `DLL`-value join over both roots' `Plugins\<n>` (literal PLUGSET fallback for virgin destinations); `ftp` category = whole `Plugins Configuration\FTP` subtree incl. its `Version` value, with password rule P1 (strip `PasswordE` + clear `Save Password` when destination has its own master password; atomic `Password Manager` pair copy when only the source uses one; NOTES text per FR-010); `plugin-configs` category = the 13 PLUGSET subtrees present in the source (research R6, R7)
- [X] T013 [US1] Implement W8 summary in `utils/migrate-altap-settings.cmd`: TRANSFERRED/PARTIAL/SKIPPED blocks with per-item reasons, not-offered block, NOTES (master-password guidance, values pointing into a detected Altap installation directory, `undelete` `Temp Path`, source `Save In Progress` warning), backup + restore instructions, identical console/`tc-migration-summary-<timestamp>.txt` output, redirected-stdin-aware final pause (FR-011)
- [X] T014 [P] [US1] Author fixture `utils/test/fixtures/altap40-full.reg`: synthetic `Altap Salamander 4.0` hive (Version=103) populating every offered category — 7 hot paths, user menu (incl. one command pointing into the fake Altap install dir), viewers/editors (incl. one Type=-1 row and one `|`-mask row), confirmations, colors + highlighting, view templates, viewer settings, default directories (incl. one invalid value for F2), Configuration scalars (incl. `Language`=`polish.slg` and toolbar layouts to prove exclusion), 12 FTP bookmarks (mix of `PasswordS` and `PasswordE` blobs) + proxies + server types, one PLUGSET plugin config (`ZIP`), one foreign plugin config (`UnRAR`), `Packers & Unpackers` content, histories
- [X] T015 [US1] Implement harness scenarios S1 + S8 in `utils/test/run_migration_tests.cmd` per quickstart.md: S1 selective transfer (hot paths + FTP only; assert mapped content matches, nothing else exists, counts in summary); S8 skip transparency (assert version markers/foreign-plugin/packers absent from destination, every dropped item named in summary, Altap-path entry flagged under NOTES)

**Checkpoint**: MVP — the utility migrates a real AS 4.0 profile end to end.

---

## Phase 4: User Story 2 — Safe by default: backup, no source changes, restore (Priority: P2)

**Goal**: Refusals while apps run, generated one-click restore, provable
source immutability, interruption safety.

**Independent Test** (spec US2): migrate over a customized destination, run
the generated restore script, destination is byte-identical to the pre-run
export; source export unchanged after every scenario (quickstart S2–S4).

- [X] T016 [US2] Implement W2 environment checks in `utils/migrate-altap-settings.cmd`: running-process refusal for `tandemcommander`/`salamand` (exit 10, message per wizard contract, honoring `TCMIG_SKIP_PROCCHECK`), re-check immediately before the first write, destination-writability probe (exit 12), and the mid-transfer failure path (exit 13: summary + restore instructions still emitted)
- [X] T017 [US2] Implement restore-script generation in `utils/migrate-altap-settings.cmd`: emit `tc-settings-restore-<timestamp>.cmd` beside the backup — deletes the current destination root then `reg import`s the backup `.reg`, or delete-only when the root did not pre-exist (recorded by T007); double-click runnable; referenced from W5 and W8 texts
- [X] T018 [US2] Source read-only audit of `utils/migrate-altap-settings.cmd`: verify every source access path opens with read-only rights (including scan, counting, and copy), add the source `Save In Progress` corruption warning at W3, and make Ctrl+C/EOF before W6 provably write-free (exit 3)
- [X] T019 [P] [US2] Author fixture `utils/test/fixtures/tc-preexisting.reg`: destination root (Version=105) with user-created hot paths, one FTP bookmark, a `Theme Mode` value, and TC-side packer settings (to prove unselected categories and TC-only values survive)
- [X] T020 [US2] Implement harness scenarios S2 + S3 + S4 in `utils/test/run_migration_tests.cmd` per quickstart.md: S2 replace semantics + backup/restore byte-roundtrip (incl. `Theme Mode` preserved); S3 source-immutability export-compare wrapped around EVERY scenario; S4 cancellation (decline confirm, zero selection) and refusal paths (missing source root ⇒ exit 11) with destination-unchanged assertions

**Checkpoint**: US1 + US2 — the tool is safe enough to hand to users.

---

## Phase 5: User Story 3 — Multiple Altap Salamander versions (Priority: P3)

**Goal**: Full source-selection UI and old-generation (best-effort) support.

**Independent Test** (spec US3): with AS 4.0 + AS 2.5-era fixtures imported
together, both are listed newest-first, default = newest, choosing the older
one migrates its data with the generation transforms applied (quickstart
S5, S6).

- [X] T021 [US3] Implement full W3 source selection in `utils/migrate-altap-settings.cmd`: list every qualifying source newest-first (scan-list order), product name + subkey count + last-write time, numeric pick with default 1, no-source exit 11; wire generation-dependent verdicts (Ancient: `colors`/`viewtemplates` not offered with stated reasons; `viewers-editors` requires cfg ≥ 6)
- [X] T022 [P] [US3] Author fixture `utils/test/fixtures/altap25-minimal.reg`: `Altap Salamander 2.51` hive with `Version\Configuration = 40` containing only hot paths (with unescaped `$` in one path), viewers with uppercase mask extensions, and confirmations — exercises transforms T1 + T2 and absent-category handling
- [X] T023 [US3] Implement harness scenarios S5 + S6 in `utils/test/run_migration_tests.cmd` per quickstart.md: S5 old/minimal source (only present categories offered; `$` doubled in migrated hot path; masks lowercased); S6 two sources imported (assert listing order, newest default, older source's unique value lands when picked)

**Checkpoint**: All source generations handled.

---

## Phase 6: User Story 4 — Repeatable, predictable re-run (Priority: P3)

**Goal**: Deterministic re-runs, no duplicates, no cross-category bleed.

**Independent Test** (spec US4): run twice — hot paths first, FTP second —
first migration survives, second adds without duplicating; same-selection
re-run is byte-identical (quickstart S7).

- [X] T024 [US4] Idempotence hardening review of `utils/migrate-altap-settings.cmd`: verify every list-category replace clears stale numbered subkeys that would otherwise be loaded (`Hot Paths` legacy `"0"` slot, `Bookmarks`/`Proxy Servers`/`Server Types` beyond the source count, renumbering after F1 drops), and that a re-run with the same selection produces byte-identical destination subtrees; fix any gap found
- [X] T025 [US4] Implement harness scenario S7 in `utils/test/run_migration_tests.cmd` per quickstart.md: run S1 twice with identical selection (assert export byte-identity of selected subtrees), then the staggered run (hot paths → FTP) asserting category A survives category B's run

**Checkpoint**: All four user stories functional and covered by the harness.

---

## Phase 7: Polish & Cross-Cutting

- [X] T026 [P] Finalize `utils/README.md`: category table (what transfers, what never does and why — packers, session state, foreign plugins), master-password behavior, restore procedure, supported source versions (2.5x–4.0 incl. Servant-branded roots, best-effort below 2.5), the "close both applications" rule, and the one-file download link pattern for the public repository
- [X] T027 Add `CHANGELOG.md` entry under the next release's "Added": standalone Altap Salamander settings-migration utility available in the repository's `utils/` directory (repo-only — no version/build bump, release artifacts unchanged, per plan.md Constitution Check)
- [X] T028 Run the full automated suite `utils\test\run_migration_tests.cmd` clean (all scenarios PASS, source-immutability wrapper green on every one) and fix anything it surfaces
- [ ] T029 Manual release-gate smoke per quickstart.md on this machine (real Altap Salamander 4.0 profile + real Tandem Commander install): migrate hot paths + FTP bookmarks, verify in the TC UI, run the generated restore, verify Altap Salamander still starts unchanged — record results in `specs/057-altap-settings-migration/quickstart.md` notes or a short `run-notes.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: none — start immediately; T002/T003 parallel to T001
- **Foundational (Phase 2)**: needs T001; T004 → T005 → T006 → T007 (same file, each builds on the previous) — BLOCKS all stories
- **US1 (Phase 3)**: needs Phase 2; T008 → T009 → T010 → T011 → T012 → T013 (same file); T014 parallel to any of them; T015 needs T013 + T014 + T003
- **US2 (Phase 4)**: needs Phase 2 (T016/T017 touch the same script — schedule after US1's script tasks to avoid file contention); T019 parallel; T020 needs T017 + T019
- **US3 (Phase 5)**: needs Phase 2; T021 after US2 script tasks (same file); T022 parallel; T023 needs T021 + T022
- **US4 (Phase 6)**: needs US1 (verifies its replace semantics); T025 needs T024
- **Polish (Phase 7)**: T026 anytime after US1; T027 anytime; T028 needs all harness scenarios (T015, T020, T023, T025); T029 last

### Story independence note

Each story is independently *testable* (its own harness scenarios + fixtures),
but story implementation tasks serialize on the single script file — the
practical order is US1 → US2 → US3 → US4, with fixtures/README/harness
skeleton as the parallel track.

### Parallel Opportunities

```text
# While T001 (script skeleton) is in progress:
T002 utils/README.md skeleton
T003 utils/test/run_migration_tests.cmd skeleton

# While US1 script tasks (T008–T013) are in progress:
T014 utils/test/fixtures/altap40-full.reg
T019 utils/test/fixtures/tc-preexisting.reg
T022 utils/test/fixtures/altap25-minimal.reg
T026 utils/README.md finalization (drafting)
```

---

## Implementation Strategy

**MVP first (US1)**: Phases 1–3 give a working migrator for the majority
case (one AS 4.0 source, fresh TC). Stop, run S1/S8, demo.

**Incremental**: US2 makes it shippable (safety guarantees), US3 widens the
audience (old versions), US4 locks in determinism. Each checkpoint leaves a
strictly more capable, still-working tool.

**Suggested checkpoint validation**: after each story phase, run the full
harness (earlier scenarios must stay green — the source-immutability wrapper
S3 applies to every scenario from T020 on).

## Notes

- Total: 29 tasks (Setup 3, Foundational 4, US1 8, US2 5, US3 3, US4 2, Polish 4)
- [P] tasks touch files nobody else is editing at that moment; the script
  itself is a single-file bottleneck by design (FR-001)
- Commit after each task or logical group
- The two contracts are the acceptance authority: category-mapping.md for
  what lands where, wizard-flow.md for screens/exit codes the harness drives

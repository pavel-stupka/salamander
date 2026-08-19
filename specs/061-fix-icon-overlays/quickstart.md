# Quickstart: Validating the Icon Overlay Fix

**Feature**: 061-fix-icon-overlays | **Date**: 2026-08-19
Validation guide for the acceptance criteria in `spec.md` (SC-001…SC-007). Details of
entities and invariants: `data-model.md`; binding behavior: `contracts/overlay-pipeline-contract.md`.

## Prerequisites

- Windows 11 machine with **TortoiseGit** (standard install) and at least one cloud
  provider active (OneDrive and/or Google Drive) — the development machine qualifies
  (22 registered overlay handlers, see `research.md` R0).
- VS2022 + `OPENSAL_BUILD_DIR` set; build via `build.cmd` from the repo root.
- A Git working copy with a mix of clean / modified / untracked files. For the
  non-ASCII scenarios, clone any repo into a path with diacritics, e.g.
  `D:\Temp\Zkouška\repo`.
- File Explorer open side by side as the reference (floor semantics: every badge
  Explorer shows must appear in TC; TC showing more is acceptable).

## V1 — Instrumented analysis run (Phase A; FR-001, SC-004)

1. `build.cmd full` (Debug x64). Debug builds define `TRACE_ENABLE`; to capture traces
   without the external Trace Server, add `TRACE_TO_FILE` to the Debug defines for the
   run (messages land in a file under `%TEMP%`).
2. Start `tandemcommander.exe`, open the Git working copy in a panel.
3. In the trace output, check the `InitShellIconOverlays` lines against the expected
   slot table in `research.md` R1 (OneDrive×7, GoogleDrive×4, Tortoise Normal /
   Modified / Conflict / Deleted; Locked+ReadOnly skipped by the shim; Added and later
   refused by the 15 cap).
4. Record which Phase A checklist item (R2: A1–A7) each observation confirms/refutes
   in `analysis-report.md`.

**Expected after the fix**: the consolidated slot-table TRACE (contract C5.2) matches
R1; Tortoise `IsMemberOf` is asked and badge indices are assigned.

## V2 — TortoiseGit badge parity (US1; SC-001)

1. Open the working copy in TC and in Explorer.
2. For every item badged in Explorer (clean ✓, modified !, conflicted, added…),
   verify the equivalent badge in TC. 100% of Explorer-badged items must be badged in
   TC; TC-only extras are acceptable and noted in `analysis-report.md`.
3. Repeat in the non-ASCII working copy (`D:\Temp\Zkouška\repo`).

## V3 — Live badge refresh (US2; SC-002)

1. With the working copy visible in a panel, modify a clean file from another program
   (e.g. `notepad`), save.
2. The badge must flip to "modified" without manual refresh (allow the provider's own
   delay — compare with Explorer's latency on the same machine).
3. Revert the file (`git checkout -- file`) — badge returns to clean.
4. Repeat in the non-ASCII working copy — same behavior (this is the D1 fix; before
   the fix the refresh never fires there).

## V4 — Cloud regression guard (US3; SC-003)

Re-run the acceptance scenarios of features 058 and 059:

1. Google Drive (`G:\Můj disk`): sync badges present, real file icons (not generic),
   auto-refresh works (no busy-cursor relist on activation).
2. OneDrive folder: synced / pending / error badges as in Explorer, including the
   "sync in progress" blue-arrows badge on pending items (059 property fallback).
3. Confirm the 059 synthetic badge still self-disables gracefully if the slot table is
   full (trace line; on this machine the table is exactly full — see R1 slot 14).

## V5 — Configuration healing (FR-009; SC-007)

1. Export, then delete the two values under `HKCU\Software\Tandem Commander\0.1\Configuration`:
   `Enable Custom Icon Overlays`, `Disabled Custom Icon Overlays`.
2. Start TC → overlays are ON (badges appear; config page checkbox checked).
3. Set `Enable Custom Icon Overlays` = 0, restart → overlays stay OFF (stored choice
   respected). Restore exported values afterwards.

## V6 — User control (US4; SC-005)

1. Configuration → Icon Overlays: the list shows the detected providers (incl. the
   Tortoise entries from the slot table).
2. Uncheck one Tortoise entry → OK → restart → that badge family is gone, others
   (incl. OneDrive) remain. Re-enable → restart → badges return.

## V7 — Release gates (SC-006)

1. `build.cmd rebuild` (Debug) and `build.cmd full release` — both clean.
2. Run the existing automated test suite (saltests) — 0 new failures.
3. Verify FR-008 by inspection: shortcut arrow / shared hand / offline clock still
   render for items without provider badges.

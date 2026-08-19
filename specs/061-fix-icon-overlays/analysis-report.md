# Analysis Report: Shell Icon Overlay System (feature 061)

**Status**: COMPLETE (Phase A analysis + Phase B fixes + Phase C validation)
**Date**: 2026-08-19
**Machine**: development machine = the machine the defect was reported on
(Windows 11, **125% display scaling**, 22 registered overlay handlers — see
`research.md` R0; TortoiseGit 2.19.1 with TortoiseOverlays delegating all 9 states
to Git handlers; OneDrive ×7 + Google Drive ×4 handlers installed)

This is the FR-001 deliverable: the end-to-end narrative of the overlay pipeline and
a CONFIRMED/REFUTED verdict for every analysis item (A1–A7), defect (D1–D5) and
legacy suspect (S1–S7), each with evidence (SC-004).

## Executive summary — the root cause nobody predicted

**TortoiseGit badges were lost in the icon-extraction rung of the handler load
ladder, and the trigger is display DPI scaling.** The 2023 open-sourcing of
Salamander replaced the closed-source `ExtractIcons` reimplementation with a thin
`SHDefExtractIcon` shim (`src/geticon.cpp`, present since the initial commit
`3945ecf`). `SHDefExtractIcon` fills its small-icon output **only for standard
sizes**; at 125% scaling the panels request 20/40/60 px, and for **.ico-file**
sources the small icon comes back NULL, while **PE-resource** sources (DLLs) are
scaled fine. The overlay loader requires all three sizes or drops the handler
(`shiconov.cpp:289`) — so every TortoiseOverlays handler (all use .ico files) was
dropped, while OneDrive/Google Drive/Windows handlers (all use DLL resources)
survived. On a 100% DPI machine the defect is invisible. This also explains "it
worked in Altap Salamander": Altap's closed-source extraction handled .ico groups
correctly; the shim never did.

Two more defects were confirmed and fixed along the way (D1 encoding in the badge
refresh loop, D2 config kill switch for migrated profiles), and one new regression
was prevented before it could ship (the feature-059 sync-pending badge would have
self-disabled once the fix let the slot table fill up).

## Fixtures (T002)

| Fixture | Path | Contents |
|---|---|---|
| ASCII working copy | `D:\Temp\tc061\repo` | committed `clean.txt`, `clean2.txt`, `sub\inner.txt`; locally modified `modified.txt`; untracked `untracked.txt` |
| Non-ASCII working copy | `D:\Temp\Zkouška\repo` | identical layout |

## Instrumentation

- Debug build, `TRACE_ENABLE` + temporary `TRACE_TO_FILE` (local edit of
  `src/vcxproj/sal_debug.props`, reverted; traces in `%TEMP%\altap_traces_<N>.log`).
- Permanent (contract C5, T003): consolidated slot-table TRACE at the end of
  `InitShellIconOverlays()`; TRACE of handlers skipped as disabled-in-configuration.
- Temporary `TEMP-061` traces (IsMemberOf results, SHCNE_UPDATEITEM paths,
  IconOverlaysChangedOnPath gate values, per-handler icon-extraction results) —
  removed in T018; `git diff` verified clean.

## A1 — Slot table vs. research.md R1 prediction: **DIVERGED, decisively**

First instrumented run (pre-fix): only **14 slots**, **zero Tortoise handlers**:

```
unable to get icons of all sizes for:   Tortoise1Normal / 2Modified / 3Conflict / 6Deleted / 7Added
GetOverlayInfo method returns error for:   Tortoise4Locked / 5ReadOnly / 8Ignored / 9Unversioned
slots: OneDrive1-7, GoogleDrive×4, EnhancedStorageShell, Offline Files + synthetic
```

- The TortoiseOverlays 12-handler cascade behaved exactly as predicted in R1
  (Locked/ReadOnly/Ignored/Unversioned refused system-wide at 22 registered).
- The five *served* states failed one rung later — icon extraction. R1's expectation
  that they would load was wrong; that discrepancy exposed D3.

Per-handler extraction trace (the decisive evidence):

```
OneDrive1..7:  file=...\FileSyncShell64.dll idx=N  sizes=20/40/60  got: 16=1 32=1 48=1
GoogleDrive×4: file=...\drivefsext.dll      idx=N  sizes=20/40/60  got: 16=1 32=1 48=1
Tortoise*:     file=...\TortoiseOverlays\icons\XPStyle\*.ico idx=0 sizes=20/40/60  got: 16=0 32=1 48=1
EnhancedStorage/Offline Files (system DLLs):                       got: 16=1 32=1 48=1
```

Standalone API probes confirmed the asymmetry independently of Tandem Commander:
`SHDefExtractIconA` on a Tortoise `.ico` fills both handles at exactly 32/16, but at
40/20, 48/24 (and any HIWORD≠standard) returns S_OK with the small handle NULL;
extracting each size separately as the "large" icon succeeds at every size
(20/40/60), for both .ico and PE sources.

**D3 root cause (CONFIRMED)**: the `ExtractIcons` shim (`src/geticon.cpp:8`,
introduced by the 2023 open-source initial commit) passed the packed two-size
request to a single `SHDefExtractIcon` call; small icons of .ico-file sources are
silently lost at non-100% DPI, and `shiconov.cpp:289` then drops the handler.

**Fix (T009)**: the shim now extracts each requested size with its own
`SHDefExtractIcon` call, taking the (always scaled) large handle per size. This
also repairs the two other paired callers: panel file icons from .ico files
(`geticon.cpp:257`) and the shortcut-overlay loader (`salamdr1.cpp:2136`) at scaled
DPI. Post-fix slot table:

```
slot 0:  synthetic "TandemCloudSyncPending"   (see D6 below)
slots 1-7:   OneDrive1..7          slots 8-11: GoogleDrive×4
slot 12: Tortoise1Normal   slot 13: Tortoise2Modified   slot 14: Tortoise3Conflict
refused by the full table: Tortoise6Deleted, Tortoise7Added, EnhancedStorageShell, Offline Files
```

## A2 — Tortoise `IsMemberOf` invocation: **CONFIRMED working post-fix**

Trace shows per-item asks in slot order; `Tortoise1Normal = S_OK` for clean items
(`clean.txt`, `sub`), `Tortoise2Modified = S_OK` for modified items. Pre-fix this
question was moot (no handler loaded). TGitCache spawned on first ask — its earlier
absence was itself evidence that no process on the machine (Explorer included) was
querying Tortoise status.

## A3/A4 — Notification loop and re-ask: **CONFIRMED working**

`SHCNE_UPDATEITEM` from TortoiseGit reaches `CMainWindow` (traces for item, parent
and `.git` paths), `IconOverlaysChangedOnPath` gates pass on the panel path
(`same=1`), the icon reader wakes and the overlay pass re-runs. Live transition
captured in trace: after `git checkout -- modified.txt`,
`Tortoise2Modified` stopped claiming and `Tortoise1Normal = S_OK` took over — the
badge flipped without user action.

## A5 — Environmental gates: **REFUTED as blockers**

`HKCU\Software\TortoiseGit` contains no cache-type override (default = TGitCache),
no overlays-only-in-Explorer style restriction, no drive-type exclusions. The test
process runs non-elevated. TortoiseOverlays state handlers all registered (9× Git
CLSIDs under `HKLM\Software\TortoiseOverlays`). Icon set: XPStyle, all .ico files
present on disk.

## A6 — Explorer ground truth (SC-001 floor): **Explorer shows NO Tortoise badges**

Explorer opened on the same ASCII working copy shows plain icons everywhere — the
first 11 alphabetical registrations (OneDrive×7 + GoogleDrive×4) consume Explorer's
effective third-party overlay capacity, so Tortoise gets nothing (consistent with
TGitCache not running before these tests). Under the clarified Explorer-as-floor
semantics: floor = ∅, TC's Normal/Modified/Conflict badges are permitted extras.
**SC-001: 100% of Explorer-badged items are badged in TC (vacuously), extras
documented — PASS.** Screenshot: `explorer_ascii.png` (session scratchpad).

## A7 — Non-ASCII path variant & D5 impact: **D1 CONFIRMED fixed; D5 no real-world impact here**

On `D:\Temp\Zkouška\repo` post-fix: initial badges correct; the
`IconOverlaysChangedOnPath` gate now logs `same=1` (UTF-8 notified path matches the
UTF-8 panel path — impossible pre-fix, when the ANSI `SHGetPathFromIDList` output
never byte-matched the UTF-8 panel path), and the live Modified→Normal flip after
`git checkout` was observed both in trace and visually. All overlay icon paths on
this machine are ASCII (Program Files / System32), so the D5 CP_ACP conversion of
the icon *file path* (`shiconov.cpp:263`, `:991`) had no real-world effect here.

## Defect disposition

| ID | Verdict | Evidence | Fix |
|---|---|---|---|
| D1 (ANSI notification path vs UTF-8 panel path kills the async re-ask loop on non-ASCII paths) | **CONFIRMED, fixed** | pre-fix `same=0` structurally guaranteed on non-ASCII paths; post-fix `same=1` + live badge flip on `D:\Temp\Zkouška\repo` | `mainwnd3.cpp`: `SHGetPathFromIDListW` + `SalWToU8` (CP_ACP fallback), contract C3. T014 audit: the only other consumer of the buffer is `HasTheSameRootPath(CheckPathRootWithRetryMsgBox, szPath)` (media-inserted retry), which compares against post-004 UTF-8 state — now consistent; `CutDirectory` is byte-delimiter based, encoding-agnostic |
| D2 (absent overlay config values force overlays off at config version ≥ 41 — hits every feature-057-migrated profile) | **CONFIRMED, fixed** | code path `mainwnd2.cpp:2345-2349`; V5a: with both values deleted, 15 slots load and Tortoise badges answer | `LoadIconOvrlsInfo`: absent ⇒ factory default (enabled); version probe removed as dead; stored-but-unreadable values keep the conservative force-disable (FR-009, contract C4) |
| D3 (primary: zero Tortoise badges) | **CONFIRMED, fixed** | A1 above | `geticon.cpp` `ExtractIcons` shim: per-size extraction (see A1) |
| D4 (no consolidated diagnostics) | **CONFIRMED, fixed** | first instrumented run required reconstructing the table from scattered lines | permanent slot-table TRACE + disabled-skip TRACE (contract C5; Debug-only per clarification) |
| D5 (CP_ACP conversion of overlay icon path at load) | **CONFIRMED latent, NOT fixed (no-change decision)** | all real handlers' icon paths are ASCII on the reference machine; extraction verified working for every loaded handler | left as-is per constitution III (do not touch what the analysis cannot exercise); noted as a candidate for a future encoding sweep |
| D6 (new, found during design: feature-059 sync-pending badge would self-disable once D3's fix fills the slot table) | **CONFIRMED, prevented** | pre-fix the synthetic entry sat at slot 13 of 14; post-fix 15 real handlers would load and the appended-last entry would get `CloudSyncPendingIndex = -1` | `InitCloudSyncPendingOverlay()` is now called *before* handler enumeration — the synthetic entry reserves slot 0; the query loop skips it, so real-handler ask order is unchanged (contract C1.3 amended). Cost: the 15th real handler (here `Tortoise6Deleted`) is refused — Explorer does not show it either (A6), so the floor holds; a "deleted" item is rarely visible in a listing at all |

## Legacy suspect verdicts (S1–S7 from investigation-leads.md)

| ID | Verdict | Evidence |
|---|---|---|
| S1 (= D1) | **CONFIRMED** (as a real defect; *not* the cause of the primary ASCII-path symptom) | see D1 |
| S2 (15-slot ceiling drops Tortoise) | **REFUTED as the primary cause** | pre-fix only 14 slots were in use — Tortoise never got far enough to be capped. Post-fix the cap does bite (Deleted/Added refused), matching Explorer's own starvation |
| S3 (config kill switch active on this machine) | **REFUTED for this machine** (values present: Enable=1, Disabled=''), **CONFIRMED as latent defect** for 057-migrated profiles | registry dump (research.md R0); V5a healing test |
| S4 (sticky crash-disable) | **REFUTED** | `Disabled Custom Icon Overlays` was empty |
| S5 (= D5) | **CONFIRMED latent, no impact here** | see D5 |
| S6 (bitness mismatch) | **REFUTED** | all handlers x64; every `CoCreateInstance` succeeded (no "unable to create object" traces) |
| S7 (059 synthetic displaces handlers) | **REFUTED as stated** (it displaced nothing), but the *reverse* interaction was real — see D6 |

## Validation results

| Check | Result |
|---|---|
| V1 slot table (post-fix) | PASS — see A1 table; every rejection traced with reason |
| V2 badge parity, ASCII (`tc_fix_initial.png`) | PASS — Normal ✓ on `sub`/`clean.txt`, Modified ! on modified items, no badge on untracked (Unversioned refused system-wide by TortoiseOverlays' own cascade — same in Explorer); Explorer floor = ∅ (A6) |
| V2 badge parity, non-ASCII (`tc_na_initial.png`) | PASS — identical behavior under `D:\Temp\Zkouška\repo` |
| V3 live refresh, ASCII | PASS — external modify flipped `clean2.txt` to Modified within one refresh cycle (trace + capture) |
| V3 live refresh, non-ASCII (`tc_na_afterrevert.png`) | PASS — `git checkout` flipped `modified.txt` back to Normal ✓ without user action (impossible pre-D1-fix) |
| V4 Google Drive (`tc_gdrive.png`) | PASS — `G:\Můj disk` folders show green synced badges, real icons, listing healthy (feature 058 intact) |
| V4 OneDrive (`tc_cloud2.png`) | PASS — cloud-state badges on all items of `E:\OneDrive - Simopt, s.r.o`; sync-pending mechanism verified structurally (synthetic entry owns slot 0 — D6); a live pending state could not be staged deterministically |
| V5a config healing (SC-007) | PASS — both values deleted ⇒ 15 slots, Tortoise answering |
| V5b stored disable respected | PASS — `Enable=0` ⇒ 0 slots, every handler traced as skipped |
| V6 per-provider disable (SC-005) | PASS — `Disabled='  Tortoise2Modified'` ⇒ exactly that handler skipped, 15 slots (freed slot reused), others unaffected; registry state restored afterwards. The configuration page itself is untouched code (writes exactly this value; lists `ListOfShellIconOverlays`, which the traces show populated incl. the synthetic entry) — page UI re-verified by inspection only |
| V7 gates | recorded in tasks/completion: final Debug + Release builds clean, saltests pass (see below) |

## Explorer-floor deviations (documented per FR-002/FR-003)

Tandem Commander displays **more** badges than Explorer on this machine (Tortoise
Normal/Modified/Conflict vs. none), permitted by the Clarifications. Not displayed
by either program: Tortoise Locked/ReadOnly/Ignored/Unversioned (refused by
TortoiseOverlays' own 12-handler cascade at 22 registrations — resolvable only by
the user reducing registered handlers), Tortoise Deleted/Added, EnhancedStorage,
Offline Files (slot-table capacity; Explorer starves them identically or worse).

## End-to-end pipeline narrative (FR-001)

1. **Discovery**: `InitShellIconOverlays()` enumerates
   `HKLM\...\ShellIconOverlayIdentifiers` case-insensitively ascending (22 keys
   here), reads each CLSID, and — unless disabled in configuration — runs the load
   ladder: `CoCreateInstance` → `GetOverlayInfo` → extract 16/32/48 badge icons →
   `Add` into the slot table (cap 15; failures consume no slot). Feature 061 fixed
   the extraction rung (D3) and made the resulting table observable (D4).
2. **Selection under the limit**: alphabetical order = Explorer's priority rule;
   TC's capacity (15) exceeds Explorer's effective third-party capacity, so TC's
   set ⊇ Explorer's set structurally. The feature-059 synthetic entry now reserves
   slot 0 (D6).
3. **Per-item query**: icon-reader threads hold per-thread handler instances; the
   overlay pass runs once per listing cycle after icons, asking `IsMemberOf` (wide
   path) in slot order, first S_OK wins; the 059 property fallback covers cloud
   sync-pending items no handler claims.
4. **Refresh**: asynchronous providers answer S_FALSE first and broadcast
   `SHCNE_UPDATEITEM` when status is computed; the notification path is now UTF-8
   end-to-end (D1), so the re-ask loop works for every panel path.
5. **Configuration**: global enable + per-provider disable list (names with leading
   spaces), crash-disable escape hatch; absent values now mean factory default (D2).

## Artifacts

Screenshots and trace excerpts referenced above live in the session scratchpad
(`tc_fix_initial.png`, `tc_na_initial.png`, `tc_na_afterrevert.png`,
`explorer_ascii.png`, `tc_gdrive.png`, `tc_cloud2.png`); the decisive trace excerpts
are quoted inline in this report. Code changes: `src/geticon.cpp`,
`src/shiconov.cpp`, `src/shiconov.h`, `src/mainwnd3.cpp`, `src/mainwnd2.cpp`.

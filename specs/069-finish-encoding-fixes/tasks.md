---
description: "Task list for feature 069 — finish the contained encoding fixes"
---

# Tasks: Finish the Contained Encoding Fixes

**Input**: Design documents from `/specs/069-finish-encoding-fixes/`
**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/fix-protocol.md](contracts/fix-protocol.md),
[quickstart.md](quickstart.md)

**Tests**: Test tasks ARE included — spec FR-008 requires a fail-before/pass-after
check for every fixed defect. Each check is a `saltests` unit test, a
`check_encoding.py` rule, or a written maintainer scenario (research.md R6).

**Organization**: by user story (spec US1–US8). Every code task carries the
finding id and the re-located `file:line` sites (verified at HEAD 2026-08-24).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task)
- **[Story]**: US1–US8 from spec.md
- Coupling groups **C1–C12** (research.md R4) are the commit unit: all code
  tasks of one group land in **one commit**, reviewed once.

## Non-negotiables for every code task

Per [contracts/fix-protocol.md](contracts/fix-protocol.md):
**A0** confirm the defect is still present at HEAD (three handoff items were
already fixed) · **A1** re-read the verdict and write down confirmed / refuted /
latent · **A2** trace producer→sink and enumerate every other consumer yourself
· **A3** minimal change in the house shape, copy the twin that already exists ·
**A4** never blank text, never skip an operation — fall back to the pre-fix
narrow call · **A5** check proven to fail before · **A6** fill the fix record ·
**A7** hand to an independent reviewer.

---

## Phase 1: Setup

**Purpose**: baseline and fixtures — nothing may be changed before the "before"
state is captured, because every fix's acceptance is measured against it.

- [X] T001 Capture the pre-fix baseline: run `build.cmd full`, then `build\tandemcommander\Debug_x64\saltests\saltests.exe` and `python tools\check_encoding.py --strict` and `--draft`; record the three numbers (expected: 1257 checks / 0 failed, strict TOTAL 0, draft TOTAL 183) in `specs/069-finish-encoding-fixes/closing-report.md` under "Baseline"
- [X] T002 Build and preserve the pre-fix Release reference for the English byte-identity gate: `build.cmd full release` then `robocopy build\tandemcommander\Release_x64 build\tandemcommander\Release_x64_prefix069 /E`
- [X] T003 [P] Create the sweep fixtures FX-CS / FX-HU / FX-SUR and the auxiliary folders with the PowerShell block in [quickstart.md](quickstart.md) ("Fixtures"); `D:\Zkouška\` does not exist on this machine
- [X] T004 [P] Create the timing fixture: `powershell -ExecutionPolicy Bypass -File tools\create-test-fixtures.ps1 -Perf` (100,000 files, needed by G6 for F-P1-27)
- [X] T005 [P] Configure 7-Zip (`C:\Program Files\7-Zip\7z.exe`) as a custom external packer **and** unpacker in Options ▸ Archivers and record the exact configuration in `quickstart.md` — there is no RAR on this machine and the external-archiver scenarios (US5) need one

---

## Phase 2: Foundational — the no-regression machinery (blocks every story)

**Purpose**: US1 is a property of every other story, so its machinery is built
first. Nothing here changes product behaviour.

- [X] T006 Create `specs/069-finish-encoding-fixes/closing-report.md` with the inventory table from spec.md ("The defect inventory") and empty Disposition / Fix / Verdict / Check columns — the single record FR-014 requires
- [X] T007 [P] Verify-close **F-P1-03**: confirm at HEAD that `src/cache.cpp:1484` uses `GetTempPathW`, `:1499` `SalFindFirstFile`, and that `tmpDir` reaches `RemoveTemporaryDir` (`src/salamdr3.cpp`) and `WM_USER_FOCUSFILE` as UTF-8; record "verify-closed, X06/X07" with the evidence lines. No code change
- [X] T008 [P] Verify-close **F-P2-10**: confirm `src/dialogs5.cpp:495` is `SalGetDlgItemTextU8` (fixed by X02 as F-P6-02 — same site); record "verify-closed, duplicate of F-P6-02". No code change
- [X] T009 [P] Verify-close the **jump-list half of F-P1-25**: confirm `src/jumplist.cpp` uses `IShellLinkW`/`SalU8ToW` throughout (X03); record it, and confirm the remaining F-P1-25 sites (T041) are the seven `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, …)` ones plus the shortcut probe
- [X] T010 Remove the void guard rule `signed-char-name-byte` from `tools/check_encoding.py` (its premise — signed `char` — is void under `/J`; its replacement belongs to cluster B-2) and record the new `--draft` count
- [X] T011 Add the `saltests` scaffold for this feature: a `TestEncodingFixes069()` function in `src/saltests/saltests.cpp` called from `main()`, so every later check has a home (keeps the count monotonic)

**Checkpoint**: baseline captured, three items verify-closed, harness ready.

---

## Phase 3: User Story 1 — Nothing that works today stops working (P1)

**Goal**: every fix in Phases 4–10 is provably non-regressive.
**Independent test**: gates G1–G7 green after every accepted fix; G8 sweep
clean; each fix has an ACCEPTED review record.

These tasks are executed **repeatedly**, once per coupling group — they are
listed once and referenced by every later phase.

- [ ] T012 [US1] For each coupling group: write its fix record into `closing-report.md` per the [data-model.md](data-model.md) "Fix record" fields (chain, change, not-touched, affected surfaces, byte identity, timing, check, changelog text)
- [ ] T013 [US1] For each coupling group: run an **independent regression review** (a reviewer that did not write the fix) against [contracts/fix-protocol.md](contracts/fix-protocol.md) Part B; write `specs/069-finish-encoding-fixes/findings/regression-X<nn>.md`; ACCEPTED ⇒ commit, REJECTED ⇒ at most two reworks then defer with the reviewer's reason
- [ ] T014 [US1] After each accepted group: run G1–G4 (`build.cmd full`, `saltests.exe`, `python tools\check_encoding.py --strict`, and G2 `build.cmd full release` at least once per phase) and append the numbers to the group's fix record

**Checkpoint**: the review loop is the gate on every subsequent phase.

---

## Phase 4: User Story 2 — Everyday operations act on the real file (P1)

**Goal**: no silent failure or data loss on accented names, paths or TEMP.
**Independent test**: quickstart V-01…V-08 on FX-CS / FX-TEMP fail on the
pre-fix binary and pass on the new one.

### C1 — command line and drops (one commit)

- [ ] T015 [US2] Fix **F-P6-04** in `src/editwnd.cpp:355` (`CEditLine::InsertText`): convert with `SalU8ToWAlloc` and send `SendMessageW(HWindow, EM_REPLACESEL, TRUE, (LPARAM)w)`, falling back to the existing ANSI `SendMessage` when conversion fails; the control's contract is already "ACP characters inside, UTF-8 at the boundary" (`:576` `SalSetWindowTextU8`, `:1769` `SalComboAddStringU8`, `:390`/`:2044` `SalGetWindowTextU8`) — per research.md R2 do **not** convert the window, do **not** touch any `EM_SETSEL`/`EM_CHARFROMPOS` offset, the word-break proc or `WM_CHAR`
- [ ] T016 [US2] Fix the same sink on the internal drag payload path at `src/editwnd.cpp:1198` (`CEditDropTarget::InsertText`, the `SALCF_FAKE_REALPATH` branch) — the verifier confirmed only this branch carries UTF-8
- [ ] T017 [P] [US2] Fix **F-P1-26** at `src/editwnd.cpp:1251`, `src/stswnd.cpp:1465` and `src/toolbar5.cpp:168`: replace `WideCharToMultiByte(CP_ACP, 0, fileW, …)` with `SalWToU8` (total since feature 066 — no failure branch needed); note `src/toolbar5.cpp:189`'s strict `FileExists` is what silently refuses the drop today
- [ ] T018 [P] [US2] Fix the viewer half of **F-P1-26** at `src/viewer3.cpp:596,599`: `DragQueryFileW` + `SalWToU8` before `SalGetFullName`/`OpenFile`
- [ ] T019 [US2] Check for C1: add to `TestEncodingFixes069()` a `SalWToU8` round-trip assertion for the drop payload shape (wide → UTF-8 → wide) and record the manual scenarios V-01/V-02 in `quickstart.md`; the command-line insert itself is window behaviour and is covered by V-01 (manual), stated as such
- [ ] T020 [US2] Fix record + independent review + gates for C1 (T012–T014)

### C10 singles — file-system and shell operations

- [ ] T021 [P] [US2] Fix **F-P1-19** in `src/mainwnd5.cpp`: `:292`/`:294` `HANDLES_Q(CreateFile(...))` → `SalCreateFile`; `:572` `HANDLES_Q(FindFirstFile(path, &data))` → `SalFindFirstFile` (registers its own handle — **not** wrapped in `HANDLES_Q`, closed with `HANDLES(FindClose(h))`) with `:696` `SalFindNextFile` and `WIN32_FIND_DATAW`→UTF-8 conversion; keep the `:575-588` error branch behaviour intact
- [ ] T022 [P] [US2] Fix **F-P1-20** in `src/salamdr3.cpp:3218-3276`: build the double-NUL `from`/`to` multi-strings as UTF-16 and call `SHFileOperationW` exactly as `src/fileswn8.cpp:43` does, keeping the ANSI call as the conversion-failure fallback (`src/finddlg2.cpp:190-205` is the house shape); stop discarding the result — check the return value and `fAnyOperationsAborted` and report failure through the existing message path of `src/dialogs5.cpp:1477`'s caller, adding no new UI
- [ ] T023 [P] [US2] Fix **F-P1-22** at `src/salamdr3.cpp:2330` and `:2713`: copy the picker's idiom from `src/dialogs3.cpp:2318,2324` verbatim (`SalU8ToW` + `ExtractIconExW`, legacy `ExtractIconEx` on failure)
- [ ] T024 [P] [US2] Fix **F-P1-23** at `src/fileswn9.cpp:666` and `src/icncache.cpp:796`: `ExpandEnvironmentStringsW` on the `SalU8ToW`-converted input, back with `SalWToU8`; at `src/mainwnd4.cpp:1059,1061` use `SetEnvironmentVariableW` with converted name and value. Note in the record that `src/icncache.cpp:788` reads the value through the old ANSI `SalRegQueryValueEx` wrapper — a producer this task does **not** change; state the resulting limitation
- [ ] T025 [P] [US2] Fix **F-P1-25** at `src/shellsup.cpp:536`, `src/mainwnd3.cpp:7124`, `src/dialogs6.cpp:554`, `src/fileswn0.cpp:341`, `src/fileswn2.cpp:150`, `src/shellib.cpp:2649`, `src/worker.cpp:6212`: try `SalU8ToW`/`SalU8ToWAlloc` first and keep `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, …)` only as the fallback — the shape `src/shellib.cpp:1600-1630` already uses. Do **not** touch `src/fileswn1.cpp:503`, `src/salamdr2.cpp:1533,1721`, `src/shiconov.cpp:450,932` (not in the finding)
- [ ] T026 [US2] Fix the unraised twin the verifier folded into **F-P1-25**: `src/shellsup.cpp:536-546` uses `IID_IShellLink` (the A interface), so `link->GetPath(linkTgt, …)` returns ACP bytes that the strict `SalGetFileAttributes` at `:541` then rejects, making a shortcut *to* an accented folder look like a file — switch to `IID_IShellLinkW`/`IPersistFile` wide and convert the target with `SalWToU8`
- [ ] T027 [P] [US2] Fix **F-P1-21** group 2–4 (`src/salshlib.cpp:652,689` archive freshness `CreateFile` → `SalCreateFile`; `src/shellsup.cpp:1004,1416` `CreateDirectory` → `SalCreateDirectory`; `src/shellsup.cpp:623,630` `FindFirstFile`/`FindNextFile` → the facade with `WIN32_FIND_DATAW`)
- [ ] T028 [P] [US2] Fix **F-P1-21** group 5–8 (`src/mainwnd4.cpp:834` `CreateFile` → `SalCreateFile` and `:538,551` `GetShortPathName` → `SalGetShortPathName`; `src/packac.cpp:467,656,722` SFX creation; `src/dialogs6.cpp:2458` mask listing; `src/worker.cpp:7058` link operation)
- [ ] T029 [P] [US2] Fix **F-P1-21** group 9 (`$(DOSPath)` family): `src/editwnd.cpp:996` and `src/execute.cpp:1213` → `SalGetShortPathName`; leave `src/execute.cpp:854,866` (Windows/system dir, verified ASCII) unchanged and say so in the record
- [ ] T030 [US2] Conditional item — F-P1-21 group 1 (`src/zip.cpp:2499,2516,2527,2535`, the `ViewFileInPluginViewer` temp file): first prove no byte a plugin receives changes (the file name string handed to the plugin, the temp file's existence and content). If proven, convert to the facade; if not, defer under FR-012 with the reason written into `closing-report.md`
- [ ] T031 [US2] Checks for the C10/US2 group: extend `TestEncodingFixes069()` with real-NTFS facade round trips on an accented and a surrogate name for the operations converted here (the `TestWtf8FileOps` pattern); where a site is not reachable from `saltests`, record V-03…V-08 in `quickstart.md` instead — and state which is which
- [ ] T032 [US2] Fix records + independent reviews + gates for the US2 singles (T012–T014), one review per commit

**Checkpoint**: US2 delivers on its own — accented everyday operations work.

---

## Phase 5: User Story 3 — The application finds its own locations (P1)

**Goal**: help, config import, cloud entries, browse buttons and external
archivers work when the account name or install path is accented.
**Independent test**: quickstart V-09/V-10 under FX-ACCOUNT and FX-INST.
**Risk**: highest in the feature (C5) — scheduled last in the execution order.

### C5 — application locations (one commit; the DC-09 trap lives here)

- [ ] T033 [US3] Enumerate and classify, before changing anything, every consumer of the `GetModuleFileName` buffers (`src/salamdr1.cpp:3566,3669,4052,4352`, `src/mainwnd3.cpp:171,2903`, `src/execute.cpp:828,919`, `src/mainwnd4.cpp:935,1000`, `src/pack3.cpp:363`) as strict-UTF-8 / legacy-ANSI-and-working / tolerant-sink, and write the table into the fix record — the plugin `.spl` loader, `lang\*.slg`, `convert\` tables and `plugins.ver` are legacy chains that work today and must keep working
- [ ] T034 [US3] Fix **F-P1-10** using the classification from T033: prefer repairing at the strict consumers with `SalLegacyToU8Alloc` over converting a buffer shared with working legacy loaders; where a producer is converted (`GetModuleFileNameW` + `SalWToU8`), convert every ANSI consumer of that same buffer in this commit. Cover the three confirmed consequences: F1 help (`src/mainwnd3.cpp:171-174`), `config.reg` next to the exe (`src/salamdr1.cpp:3566,3570`), `$(SalDir)`/`$(SalPath)` (`src/execute.cpp:828`)
- [ ] T035 [US3] Fix **F-P1-08** at `src/salamdr5.cpp:1855,1864` (`SHGetFolderPath` A → `SHGetFolderPathW` + `SalWToU8`) in `GetOurPathInRoamingAPPDATA` **and** `CreateOurPathInRoamingAPPDATA` together — the verifier found the write side works while the read side cannot find what it wrote; also the Google Drive (`src/shiconov.cpp`) and Dropbox (`src/drivelst.cpp:1330-1387`) path producers if not already done by C2
- [ ] T036 [US3] Fix **F-P1-07** at `src/pack3.cpp:363` (and the second copy of the shape in `src/pack1.cpp:613-629,717`): take the helper-executable path wide at the source so the whole command line — including correctly encoded archive and file names — is no longer discarded
- [ ] T037 [US3] Fix **F-P2-13** at `src/mainwnd3.cpp:2844` (`LoadStr(IDS_SAVECFG_EXPFILEEXISTS)` → `LoadStrU8`) **only after** T034 settles `ConfigurationName`'s producer encoding, and correct the stale suppression comment at `:2842`. Delivers US7 acceptance scenario 6
- [ ] T038 [US3] Fix **F-P1-24**: `src/execute.cpp:2152` `GetOpenFileName` → the W twin + `SalWToU8`, converting the surrounding edit-line I/O at `:2129-2130,2156` in the same change, and re-check `BrowseCommand`'s other two callers (`src/dialogs2.cpp:1271`, `src/dialogs3.cpp:2446`); `src/salamdr6.cpp:1699-1733` (use the existing `SafeGetSaveFileNameW`), `src/shellib.cpp:2503,2543,2547` and `GetMyDocumentsOrDesktopPath` (`:2943,2954`) with its users (`src/fileswn3.cpp`, `src/salamdr6.cpp:1769`, `src/fileswn0.cpp`). The dialogs stay ANSI windows (cluster B-1) — the verifier refuted the display claim, so the edit line must keep displaying the picked path correctly; if a sub-case cannot be made correct without converting the dialog, defer that sub-case with the reason
- [ ] T039 [US3] Checks for C5: a `saltests` assertion that `SalLegacyToU8Alloc` keeps valid UTF-8 unchanged and repairs ACP input (the T034 mechanism), plus scenarios V-09/V-10 recorded in `quickstart.md`; the account-name-gated part is verified at the producer level and recorded as such, never claimed as an on-screen pass
- [ ] T040 [US3] Fix record + independent review + gates for C5 — the hardest review in the feature; the reviewer must re-derive the T033 classification independently

**Checkpoint**: US3 delivers independently of US2.

---

## Phase 6: User Story 4 — Cloud, drive, volume and share entries (P2)

**Goal**: cloud roots, mounted volumes, subst drives, labels and shares behave
like their ASCII counterparts.
**Independent test**: quickstart V-11…V-13.

### C2 — cloud roots (one commit)

- [ ] T041 [P] [US4] Fix **F-P1-09 + F-P4-05** together at `src/drivelst.cpp:1481` (OneDrive personal), `:1384` (Dropbox) and `src/shiconov.cpp:161` (Google Drive sync root): replace the CP_ACP-defaulting `ConvertU2A` with `SalWToU8` — `src/shiconov.cpp:146` already passes `CP_UTF8` fifteen lines up, so the twin is in the file. Do **not** touch the commented-out `shellsup.cpp` prefix test (refuted)
- [ ] T042 [US4] Fix the OneDrive Business half at `src/drivelst.cpp:1503-1541`: the three registry values read through the old ANSI `SalRegQueryValueEx` wrapper → `SalRegQueryValueExW8`; check whether the Business drive-menu label at `:2021` (an ANSI/ANSI composition that renders correctly today) must move with them, and say so either way
- [ ] T043 [US4] Check for C2: a `saltests` `SalWToU8` assertion is not enough on its own — record V-11 in `quickstart.md` and add a `check_encoding.py` observation that `ConvertU2A` on a wide shell path is gone from these three files (the `Sal`-prefixed lookbehind makes the existing rule blind to `SalRegQueryValueEx`, per P7)
- [ ] T044 [US4] Fix record + independent review + gates for C2

### C3 — volume, subst, labels and the Drive Information type line (one commit)

- [ ] T045 [US4] Fix **F-P1-12** at `src/salamdr2.cpp:1440,1482` (`GetVolumeInformation` A → W with `SalU8ToW`) and `MyGetDiskFreeSpace`; the refuted case (ordinary accented paths on a plain volume, where the walk-up to the root is designed behaviour) must stay byte-identical
- [ ] T046 [US4] Fix the strengthened half of **F-P1-12**: `src/mainwnd5.cpp:1226,1232` read an **uninitialised** `char fileSystem[20]` after ignoring `MyGetVolumeInformation`'s return value, making Compare Directories' FAT timestamp tolerance a coin flip — initialise and honour the return value
- [ ] T047 [US4] Fix **F-P1-13** at `src/salamdr2.cpp:1781` (`QueryDosDevice` A → W + `SalWToU8`) so `ResolveSubsts`' splice at `:1266-1277` is homogeneous UTF-8; verify the consumers `src/fileswn8.cpp:437-449` (delete confirmation link type) and `src/salamdr1.cpp:1284-1289,1386`
- [ ] T048 [US4] Fix **F-P1-14** at `src/drivelst.cpp:1739,2622` (`GetVolumeInformation`), `:1769` (`WNetGetConnection`), `:1115` (`SHGetFileInfo` display name — note `SalSHGetFileInfoIcons` discards strings and is **not** a drop-in) and `src/dialogs3.cpp:1614`: take the W twins and convert with `SalWToU8`; code-page-representable labels render correctly today and must be unchanged
- [ ] T049 [US4] Fix **F-P2-07** in the same commit: convert the whole `switch` at `src/dialogs3.cpp:1621-1648` to `LoadStrU8` (all of `IDS_INFODLGTYPE1..9`, not just the junction append at `:1635`) — T045/T047/T048 make the UNC and SUBST arguments UTF-8, so the currently-correct rows would turn to mojibake if the template stayed ANSI. This is the coupling that makes C3 one commit. Delivers US7 acceptance scenario 2
- [ ] T050 [US4] Check for C3: `saltests` cannot reach these files — add a `check_encoding.py` observation for the converted sites and record V-12 in `quickstart.md` (FX-SUBST + FX-JUNC), including the explicit "UNC and SUBST rows unchanged" expectation
- [ ] T051 [US4] Fix record + independent review + gates for C3; the reviewer must verify per-row that Drive Information is unchanged for ASCII and for code-page-representable input

### C6 — shares (one commit; per-item path)

- [ ] T052 [US4] Fix **F-P1-27** at `src/shares.cpp:105-107` (three `WideCharToMultiByte(CP_ACP, …)` on `NetShareEnum` results → `SalWToU8`) together with both consumer kinds: the per-directory marker `src/fileswn3.cpp:547` and `src/drivelst.cpp:1850` (`Shares.Search`) and the operational `src/fileswn9.cpp:1864,1919` (`Shares.GetUNCPath`). The `src/shellib.cpp:510,579,1565,1848,1954,3039` STRRET sites are the same pattern — convert only those whose consumer is proven to accept UTF-8, and list the rest as untouched
- [ ] T053 [US4] Record the G6 timing for C6 in the fix record in `closing-report.md`: `src/fileswn3.cpp:547` `Shares.Search` runs once per listed directory, so take before/after medians on the 100,000-file fixture per the [quickstart.md](quickstart.md) timing method and accept only inside the baseline `[min, max]`
- [ ] T054 [US4] Check for C6: `saltests` round trip for the share-name conversion if it can be reached, else V-13 in `quickstart.md` (FX-SHARE)
- [ ] T055 [US4] Fix record (including the timing numbers) + independent review + gates for C6

---

## Phase 7: User Story 5 — External archivers (P2)

**Goal**: pack, list, extract and view with an external archiver work with
accented names and an accented TEMP.
**Independent test**: quickstart V-10 with FX-ARC (7-Zip) and FX-TEMP.
**Risk**: high — the pack and list defects cancel each other today.

### C4 — external archivers (one commit, both directions)

- [ ] T056 [US5] Write the OEM conversion helper pair in `src/common/` (UTF-8 ⇄ console OEM code page, via `SalU8ToW` + `WideCharToMultiByte(CP_OEMCP)` and back) and unit-test it in `TestEncodingFixes069()` **before** touching the product path — this is the fail-before evidence for C4 and the only place the round trip is testable
- [ ] T057 [US5] Fix **F-P1-05** in both directions in one change: the pack side `src/pack1.cpp:1493,1498` and `src/pack2.cpp:318,327,345,653,658` (`CharToOem` on UTF-8 → the helper) and the list side `src/pack1.cpp:302` (`OemToCharBuff` into `CFileData::Name` → the helper's inverse). The verifier proved these two cancel today, so a one-sided change would break the currently working extract of ACP-representable names; also handle the `needANSIListFile` branch
- [ ] T058 [US5] Fix **F-P1-06**: `src/pack1.cpp:1477`, `src/pack2.cpp:301,641` narrow `fopen` on a UTF-8 temp path → the wide CRT twin (`_wfopen` with `SalU8ToW`) or the house facade; `src/pack1.cpp:1604,1863`, `src/pack2.cpp:500`, `src/pack3.cpp:1251` `FindFirstFile` → `SalFindFirstFile` with `WIN32_FIND_DATAW`→UTF-8 so the extracted name spliced into the UTF-8 temp dir is no longer ACP; the ANSI `RemoveDirectory`/cleanup calls in the same functions; `GetShortPathName` at `src/pack2.cpp:274,333,445,744` and `src/pack3.cpp:1212,1284,1364,1390,1431` → `SalGetShortPathName`
- [ ] T059 [US5] Enumerate every remaining ANSI consumer of `SalGetTempFileName`'s (UTF-8) output in `pack1.cpp`/`pack2.cpp`/`pack3.cpp` — the verifier counted 40+ — and either convert it in this commit or list it with the reason it is safe
- [ ] T060 [US5] Fix record + independent review + gates for C4; the review must confirm the extract round trip for ASCII and for ACP-representable names is unchanged
- [ ] T061 [US5] Record V-10 in `quickstart.md` with the 7-Zip configuration from T005, noting that RAR (the default mapping for `.rar`) is unavailable here so the archiver path is exercised through a custom archiver

---

## Phase 8: User Story 6 — Viewer settings, captions, instant Markdown (P2)

**Goal**: the viewer's default conversion survives a restart, captions are
readable, and mdview's instant view re-arms after an Unload.
**Independent test**: quickstart V-14, V-15.

### C7 — viewer (one commit)

- [ ] T062 [US6] Fix **F-P4-01** at the single intake `src/codetbl.cpp:155` (`memcpy(name, beg, l)` of a CP1250 `convert.cfg` name): normalize with `SalLegacyToU8Alloc` so `CCodeTablesData::Name` is UTF-8 by contract (mirroring feature 052's plugin-metadata pattern), and document the field. Trace the stored-default round trip (registry write → read → the byte compare at `src/codetbl.cpp:795`) and design the compare so a value **already** persisted by an older build is still recognised — losing a stored default would be a regression
- [ ] T063 [US6] Fix **F-P4-02** trigger T1 at `src/viewer3.cpp:41`: `LoadStr(IDS_VIEWERTITLE)` → `LoadStrU8` (non-ASCII in cs/sk/hu, so every accented file name is mojibake in those UIs from the moment the window opens — the verifier's widening of the finding); T2 is removed by T062. Check the plugin-supplied `Caption` contributor (`src/viewer2.cpp:879`) and state whether it is UTF-8 by contract or left as-is
- [ ] T064 [US6] Check for C7: unit-test the `convert.cfg` name normalization in `TestEncodingFixes069()` (`SalLegacyToU8Alloc` is in `salunicode.cpp`, which `saltests` links) and record V-14; note that the registry facade is not linked into `saltests` (068 ledger L75), so the persistence half is manual
- [ ] T065 [US6] Fix record + independent review + gates for C7

### F-P6-01 — mdview keeper (own commit, plugin-local)

- [ ] T066 [P] [US6] Fix **F-P6-01** in `src/plugins/mdview/webview.cpp`: add `UnregisterClassW(kKeeperClass, DLLInstance)` and reset the registered flag at the end of `MdKeeperReleaseAll()` (`:699`), after the window is destroyed. The alternative of accepting `ERROR_CLASS_ALREADY_EXISTS` at `:804` is **forbidden** — it would create a window on a dangling window procedure and crash. Compare `src/plugins/shared/winliblt.cpp:131-137`
- [ ] T067 [US6] Check for F-P6-01: record V-15 in `quickstart.md` (F3 a `.md` → close → Unload in Plugins Manager → F3 again, second view as fast as the first); no automated check is possible — say so
- [ ] T068 [US6] Fix record + independent review + gates for F-P6-01, confirming no change to the plugin ABI or to what the core hands mdview

---

## Phase 9: User Story 7 — Dialog and list text (P3)

**Goal**: the remaining composed strings render readably in non-English UIs.
**Independent test**: quickstart V-16…V-20. (V-12/V-18 are delivered by
Phases 6 and 5 — F-P2-07 with C3, F-P2-13 with C5.)

- [ ] T069 [P] [US7] Fix **F-P2-04** at `src/fileswn3.cpp:285` and `src/salamdr5.cpp:396`: `LoadStr` → `LoadStrU8` for the wait-window composition (sink `src/dialogs3.cpp:2735-2742` already tries wide then falls back). Decide the seven latent plugin-loading sites (`src/plugins2.cpp:3002,3018,3074,3121,3338,3430,3454`) — convert only if provably a no-op for all shipped plugins (relative ASCII `DLLName`), and record the decision either way
- [ ] T070 [P] [US7] Fix **F-P2-09** at `src/plugins2.cpp:1051,1054,1056`: `ListView_SetItemText` → `SalListViewSetItemTextU8`, matching `:1049` (Name). Confirm in the record that the sink's ANSI fallback still renders the ACP value the same session's `GetOpenFileNameA` produced (the value is UTF-8 only after a restart)
- [ ] T071 [P] [US7] Fix **F-P2-11** at `src/dialogs5.cpp:1091,1094` (`CPluginKeys::RefreshListView`) → `SalListViewSetItemTextU8`; check the 500-byte `lstrcpyn` clamp at `:1067` cannot cut a UTF-8 sequence
- [ ] T072 [P] [US7] Fix **F-P3-07** at `src/stswnd.cpp:1854`: route the clamp through `CopyToolTipAnswer` (declare it in `src/gui.h` and un-static it in `src/gui.cpp:985` — the single refactoring this feature permits, research.md R6 D-T1) so the cut lands on a character boundary. Do **not** touch `src/stswnd.cpp:1878,1884` or `src/drivelst.cpp:2697` (refuted)
- [ ] T073 [US7] Fix **F-P4-03** at the nine seeds `src/packers.cpp:240,249,262,271,1058,1068` and `src/packac.cpp:997,999,1007,1033`: `LoadStr` → `LoadStrU8`; document `CPackerConfigData::Title`/`CUnpackerConfigData::Title` as UTF-8; normalize plugin-supplied titles at `src/plugins1.cpp:584,610` with `SalLegacyToU8Alloc` **only if** it changes no byte handed back to plugins (FR-005). State what happens to titles already persisted with `?` (re-seeded only when `ConfigVersion < 105`, `src/mainwnd2.cpp:2720`) — accept or note, do not migrate
- [ ] T074 [US7] Fix **F-P4-07** at `src/salamdr4.cpp:799-805` and `:1009` (`strcpy(name, LoadStr(resID))` in `CViewTemplates::Load`) → `LoadStrU8`; check `VIEW_NAME_MAX` against UTF-8 growth at `:818`. This is **hygiene** (the verifier refuted "the damage is permanent" — the names are re-seeded on every load) and must not be presented as a user-visible repair. Decide the latent user-menu "(Submenu End)" marker (`src/dialogs4.cpp:2279`) and record the decision
- [ ] T075 [US7] Add the strict guard rule for C9 (`acp-title-seed`): flag `LoadStr(` seeding a field documented UTF-8 (packer titles, view-mode names) in `tools/check_encoding.py`, promoted to strict in the **same commit** as T073/T074 and proven to fire on the pre-fix line (P7's note: added earlier it would flag correct ANSI code)
- [ ] T076 [US7] Checks for US7: unit-test the tooltip boundary clamp in `TestEncodingFixes069()` (reachable once T072 exports it); record V-16, V-17, V-19, V-20 in `quickstart.md`
- [ ] T077 [US7] Fix records + independent reviews + gates for the US7 commits (C8 = T070+T071 as one commit; C9 = T073+T074+T075 as one commit; T069 and T072 separately)

---

## Phase 10: User Story 8 — Plugin, documentation and tooling leftovers (P3)

**Goal**: the small deferred items D01–D05 and the SDK documentation gap.
**Independent test**: quickstart V-21…V-24.

- [ ] T078 [P] [US8] Fix **D01** in `src/common/handles.h:531,538,541,546,615` (+ `src/common/handles.cpp`): the generic `LPCTSTR` wrappers collide with the explicit `…W` ones under `UNICODE`, so `tserver` cannot build. Make the generic names resolve the way `windows.h` does (explicit `…A`/`…W` pair selected by `#ifdef UNICODE`) and prove the non-UNICODE core's behaviour is unchanged — no new warnings, the core's calls bind to the same functions
- [ ] T079 [P] [US8] Verify D01 by building the trace tool: `msbuild src\vcxproj\tserver\tserver.vcxproj` (check its configurations first); then re-enable the Trace Server capture in gate G5 in `quickstart.md`, which feature 068 had to waive
- [ ] T080 [P] [US8] Fix **D05** at `.specify/extensions/git/scripts/powershell/auto-commit.ps1:149`: replace the non-ASCII check-mark with ASCII (preferred over adding a BOM, since a speckit upgrade would overwrite the file and the bash variant is what this machine uses); sweep the sibling scripts in `.specify/extensions/git/scripts/powershell/` and `.specify/scripts/powershell/` for the same shape; verify by parsing the file with `powershell -NoProfile`
- [ ] T081 [US8] Fix **D03 + D04** in one commit (plugin-local, FR-012): `src/plugins/filecomp/controls.cpp:86-92` currently sets `buff[0] = 0` when `SplU8ToW` fails, dropping the path bar's text — fall back to the legacy narrow draw instead (the X09 idiom in `src/plugins/filecomp/mainwnd.cpp`); and `src/plugins/filecomp/worker2.cpp:91-100` composes the ANSI `IDS_MAINWNDHEADER` with UTF-8 names into `SetWindowText`, overwriting X09's fixed caption on the binary-differ path — convert it the same way X09 did, checking the thread context of that `SetWindowText`
- [ ] T082 [US8] Conditional item — fix D02 at `src/plugins/zip/common.cpp:2478-2482` (`GetInfo` mixes `NumberToStr`'s UTF-8 separator with ANSI `GetDateFormat`/`GetTimeFormat` → a stray `Â` for every file ≥ 1000 bytes on cs-CZ): enumerate all five callers, including `zip/add.cpp:1236` → `CSalamanderGeneral::DialogOverwrite` → the core's `COverwriteDlg` (`src/zip.cpp:664,679`). Make `GetInfo` homogeneous UTF-8 **only if** English/ASCII bytes reaching the core are provably identical; otherwise defer to cluster B-5 with the reason in `closing-report.md`
- [ ] T083 [P] [US8] Fix **F-P5-06** (documentation only): state the path-text encoding in the five doc blocks of `src/plugins/shared/spl_fs.h` — `GetCurrentPath` (`:244`), `GetFullName` (`:250`), `GetFullFSPath` (`:260`), `GetRootPath` (`:265`), `ListCurrentPath` (`:330`) — mirroring `spl_com.h`'s statement for `CFileData::Name`; comments only, no ABI change, no plugin rebuild. Note that `spl_gen.h`'s `GetErrorText` statement is the prerequisite for cluster B-3 and is **not** written here
- [ ] T084 [US8] Fix records + independent reviews + gates for the US8 commits (T078/T079, T080, T081, T082, T083)

---

## Phase 11: Polish, gates and the closing record

- [ ] T085 Run the full gate set once at the end: G1 `build.cmd full`, G2 `build.cmd full release`, G3 `saltests.exe` (≥ 1257 + this feature's checks, 0 failed), G4 `python tools\check_encoding.py --strict` = `TOTAL: 0`, G5 start/exit health (now with the Trace Server capture from T079), G6 the recorded timings, G7 the English spot-check against `Release_x64_prefix069`
- [ ] T086 Prove every added guard rule and unit check fail-before/pass-after mechanically (`git stash` → run → fires/fails → `git stash pop` → run → clean/passes) and paste the counts into each fix record
- [ ] T087 [P] Write the `CHANGELOG.md` Unreleased entries for every user-visible fix in the user's terms, truthfully scoped: state F-P6-04's residual limitation (a name outside the system code page now inserts as `?` rather than mojibake — visibly wrong instead of invisibly wrong), and do **not** claim F-P4-07 (hygiene) or the verify-only items as repairs
- [ ] T088 [P] Assemble the maintainer sweep package: the 068 items W1–W20 plus V-01…V-24 in `quickstart.md`, with the fixtures, the language-switching procedure and the pre-fix reference binary named
- [ ] T089 Complete `closing-report.md`: every one of the 34 section-1 items and D01–D05 with its disposition (31 fixed-accepted / 3 verify-closed / any deferred with reason), fix id, regression verdict, check and timing; the gate table; the baseline-vs-final numbers for `saltests` and both guard modes
- [ ] T090 Write `specs/069-finish-encoding-fixes/REMAINING-WORK.md`: the handoff for what stays open — clusters B-1–B-5 (with the complete Unicode command line enumerated as B-1's first candidate, per research.md R2), anything deferred here, and the still-owed on-screen sweep
- [ ] T091 Update `CLAUDE.md`'s "Recent Changes" with a 069 entry in the established style (one paragraph: what was fixed, what was deferred and why, test/guard counts, plugin ABI untouched)

---

## Dependencies & execution order

**Phase order**: Phase 1 → Phase 2 → Phases 4–10 → Phase 11. Phase 3 (US1) is
not a sequential phase: T012–T014 execute once per coupling group inside every
later phase.

**Authoritative execution order across stories** (research.md R10, ascending
risk — this overrides plain story-priority order, so that the two groups most
likely to be rejected land when a deferral costs least):

| # | Group | Tasks | Story | Risk |
|---|---|---|---|---|
| 1 | C12 tooling | T078–T080 | US8 | low (D01 unlocks the G5 capture) |
| 2 | C10 singles + doc | T083, T066–T068, T072, T069, T023, T070–T071, T021, T022, T024, T025–T026, T027–T029 | US2/US6/US7/US8 | low |
| 3 | C11 filecomp | T081 | US8 | low |
| 4 | C7 viewer | T062–T065 | US6 | medium (persisted default) |
| 5 | C9 seeds + guard | T073–T075 | US7 | medium (persisted titles) |
| 6 | C2 cloud roots | T041–T044 | US4 | low |
| 7 | C6 shares | T052–T055 | US4 | medium (per-item timing) |
| 8 | C1 command line | T015–T020 | US2 | medium |
| 9 | C3 volume/drive info | T045–T051 | US4 | high (correct rows could break) |
| 10 | C4 archivers | T056–T061 | US5 | high (cancelling defects) |
| 11 | C5 app locations | T033–T040 | US3 | highest (DC-09 trap) |
| 12 | Conditional | T030, T082 | US2/US8 | analysis-gated |

**Hard dependencies**: T001–T002 before any fix · T033 before T034–T038 ·
T037 after T034 · T045/T047/T048 before T049 (same commit) · T056 before
T057–T058 · T062 before T063 (T062 removes T063's second trigger) · T072
before T076 · T073/T074 before T075 · T078 before T079 · everything before
T085–T091.

**Parallel opportunities**: T003–T005 together; the `[P]`-marked single fixes
in Phase 4 (T021, T023, T024, T025, T027, T028, T029) touch different files
and can be written in parallel, but each still lands as its own reviewed
commit; T041 and T045-group are in different files; T078/T080/T083 are fully
independent of the product code.

## Implementation strategy

- **MVP = Phase 1 + Phase 2 + Phase 4 (US2)**: the silent data-loss and
  silent-failure defects on accented names are the ones a user hits daily; US2
  alone is a shippable improvement, and every later phase is additive.
- **One commit per coupling group**, message prefix `[069]`, each independently
  revertable (spec FR-007). Never batch two groups into one commit — the
  revert granularity is the safety net the user's no-regression rule relies on.
- **A rejected fix is not forced through**: two rework rounds, then defer with
  the reviewer's reason (FR-001). Deferring is a normal outcome here; shipping
  a regression is not.
- **Stop conditions**: if `saltests` drops below baseline, the strict guard is
  non-zero, or a W1–W20 sweep item fails, the current group is reverted before
  anything else proceeds.

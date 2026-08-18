# Evidence Log: 058-fix-cloud-status-icons

Chronological record for SC-001…SC-008 validation. Machine: the reporting
user's Windows 11 x64 (Czech locale), Google Drive for desktop mounted as
`G:\` (`G:\Můj disk`), OneDrive root `E:\OneDrive - Simopt, s.r.o`.

## T001 — Pre-fix baseline build (2026-08-18)

- `build.cmd` (Debug x64, incremental, `OPENSAL_BUILD_DIR` unset → `.\build\`):
  **BUILD SUCCEEDED**, 0 errors, 68 warnings (all pre-existing; libssh2
  LNK4217 set known from feature 056). Duration 25 s.

## T002 — Repro fixtures (2026-08-18)

Created, each populated with `test.docx`, `test.pdf`, `test-app.exe`
(copy of notepad.exe — per-file icon), `test-image.jpg`:

- `D:\Test\Zkouška` (non-ASCII path — trigger)
- `D:\Test\Control` (ASCII sibling — control group)
- `E:\OneDrive - Simopt, s.r.o\TC-Test-Zkouška` (non-ASCII inside a sync root)

Cleanup after the feature: delete `D:\Test\Zkouška`, `D:\Test\Control`,
`…\TC-Test-Zkouška`.

## T003 — Pre-fix symptoms & mechanism proof (2026-08-18)

**User-reported symptoms** (the defect reports driving this feature — all on
this machine, pre-fix):

1. No sync-status badges anywhere under `G:\Můj disk`, while Windows
   Explorer shows them (spec US1).
2. On every window activation with such a path in a panel, the mouse cursor
   briefly turns into a busy indicator, then nothing visibly changes (US2).
3. Word/PDF files show a generic icon instead of their application icon
   (US4).

**Environment facts captured** (session, 2026-08-18):

- `HKLM\...\ShellIconOverlayIdentifiers` holds `OneDrive1..7` and four modern
  Google Drive handlers (`GoogleDriveCloudOverlayIconHandler`,
  `GoogleDriveMirrorBlacklistedOverlayIconHandler`,
  `GoogleDrivePinnedOverlayIconHandler`,
  `GoogleDriveProgressOverlayIconHandler`) — none match the 2015-era name
  list in `shiconov.cpp` (research.md R6).
- Legacy `%LOCALAPPDATA%\Google\Drive\...\sync_config.db`: absent.
  `%LOCALAPPDATA%\Google\DriveFS`: present. `%USERPROFILE%\Google Drive`:
  absent.
- Items under `G:\Můj disk` carry plain attributes (no cloud-files
  placeholder attributes) — Explorer's badges there come from the overlay
  handlers, confirming the overlay pipeline is the right fix target.

**RC2 mechanism proof (empirical, P/Invoke)** — the exact call shape the
pre-fix snooper makes vs. the fix:

```text
FindFirstChangeNotificationA(UTF-8 bytes of "D:\Test\Zkouška")
    → INVALID_HANDLE_VALUE, GetLastError = 2 (FILE_NOT_FOUND)   ← pre-fix
FindFirstChangeNotificationW(L"D:\Test\Zkouška")
    → valid handle                                               ← post-fix
```

RC1/RC3 share the same encoding mismatch (CP_ACP conversion of UTF-8 input
garbles "ů" → path does not exist for the shell); code evidence in
research.md R1.

**Not captured programmatically**: GUI screenshots (badges, cursor). The
agent session cannot observe the running GUI; the user's reports above stand
as the pre-fix record, and post-fix validation of the same scenarios is the
acceptance evidence.

## Post-fix automated results (2026-08-18)

All three fixes implemented (`fileswn1.cpp`, `snooper.cpp` + `handles.*`,
`geticon.cpp` — see the feature diff):

- `build.cmd` Debug x64: **BUILD SUCCEEDED**, 0 errors, **zero warnings in
  any touched file** (checked by grep over the build log).
- `build.cmd full` (Debug) and `build.cmd full release`: both
  **BUILD SUCCEEDED**; 19 plugins registered, 180 language modules.
- `saltests.exe` (freshly rebuilt): **1145 checks, 0 failed** — identical to
  the feature-056 release-gate baseline.
- clang-format 17.0.3 dry-run over the five touched files: no changes
  required; UTF-8 BOM verified intact on all five.
- Smoke test: the fixed Debug binary launches and runs (startup exercises
  `InitShellIconOverlays`, snooper `AddDirectory`, both icon-reader
  threads); alive after 6 s, terminated cleanly. No startup crash.
- RC2 mechanism re-check (see T003): the post-fix call shape
  (`FindFirstChangeNotificationW` + proper UTF-16) was empirically shown to
  return a valid handle for `D:\Test\Zkouška`.

## Manual validation result (2026-08-18)

**All manual scenarios below were executed by the user (Pavel Stupka) on the
repro machine and PASSED** — badges match Explorer in `G:\Můj disk` and the
OneDrive diacritic subfolder, external changes appear automatically with no
busy-cursor flash on activation, base icons render correctly in
`D:\Test\Zkouška`, and the no-regression suite (ASCII OneDrive, ASCII local,
icon-overlay configuration toggle) is clean. SC-001…SC-008 verified.

## T006 — US1 validation (badges) — PASSED (user-verified)

Automated portion (build) done. GUI steps executed by the user:

1. Open `E:\OneDrive - Simopt, s.r.o\TC-Test-Zkouška` in a panel → status
   badges now appear and match Explorer (SC-001).
2. Open `G:\Můj disk` next to Explorer → item-by-item badge parity; visible
   items badge within ~2 s (SC-001).
3. Toggle a file online-only/available and refresh → badge updates.

## T009 — US2 validation (activation / auto-refresh) — PASSED (user-verified)

1. Panel on `D:\Test\Zkouška`; from another app run
   `echo x> "D:\Test\Zkouška\new.txt"` → appears without manual refresh
   (SC-008).
2. Alt-Tab away/back 10× with panel on `D:\Test\Zkouška` and on
   `G:\Můj disk` → no busy-cursor flash (SC-002).
3. (Debug + trace server) entering `D:\Test\Zkouška` no longer logs
   `Unable to receive change notifications`.
4. Quit Google Drive, re-enter `G:\Můj disk` path after starting it again →
   badges return without app restart (FR-010).

## T011 — US4 validation (base icons) — PASSED (user-verified)

1. Panel on `D:\Test\Zkouška` vs `D:\Test\Control`: `test.docx`,
   `test.pdf`, `test-app.exe` (notepad icon), `test-image.jpg` show
   identical, correct icons in both (SC-007).
2. Spot-check a ZIP archive panel listing for unchanged icons (legacy
   `GetFileIcon` callers).

## T012 — US3 no-regression suite — PASSED (user-verified)

Quickstart §3: OneDrive ASCII folder badges unchanged (SC-005); plain ASCII
local folder behavior unchanged (FR-009/SC-003); Configuration → icon
overlays off → no badges anywhere, on → badges return (FR-011); provider
stopped → no busy-cursor loop (SC-006).

## Cleanup after validation

Delete `D:\Test\Zkouška`, `D:\Test\Control`,
`E:\OneDrive - Simopt, s.r.o\TC-Test-Zkouška`.

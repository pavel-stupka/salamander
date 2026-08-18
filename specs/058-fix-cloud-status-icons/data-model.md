# Data Model: Cloud Sync Status Icons in File Panels

**Feature**: 058-fix-cloud-status-icons · **Date**: 2026-08-18

This feature introduces no new persistent data. The "model" is the existing
in-memory state of the icon/overlay/snooper pipeline and the encoding
contract its strings must obey. Entities below document what the fix relies
on and must not disturb.

## Entities

### Panel path (`CFilesWindow::Path`)
- **Encoding contract**: UTF-8 (feature 004), up to `SAL_MAX_PATH_UTF8`
  bytes. Every WinAPI/shell consumer must convert via `SalU8ToW`/
  `SalU8ToWAlloc` (CP_ACP fallback only for invalid UTF-8).
- **Consumers relevant here**: icon-reader thread (wide prefix `wPath`),
  snooper (`FindFirstChangeNotificationW`), `CheckPath`/refresh (already
  compliant).

### Panel item (`CFileData`) — icon-related fields
- `Name` / `NameLen`: UTF-8 item name (feature 004/027).
- `IconOverlayIndex`: index into `CShellIconOverlays::Overlays`, or
  `ICONOVERLAYINDEX_NOTUSED`. Written only by the icon-reader thread under
  `ICSleepSection`; read by panel painting.
- `IconOverlayDone`: 0/1 — per-cycle "already queried" flag, reset at the
  start of each read round (`fileswn1.cpp:550-556`).
- **Validation rule (FR-001..003)**: after a completed read round in a
  monitored folder, `IconOverlayIndex` must equal the handler index whose
  `IsMemberOf(correct wide path)` returned `S_OK` first in registry order,
  or NOTUSED.

### Overlay handler entry (`CShellIconOverlayItem`)
- `IconOverlayName` (registry subkey name, may carry leading spaces),
  `IconOverlayIdCLSID`, `Identifier` (main-thread STA object),
  `IconOverlay[ICONSIZE_COUNT]` (HICONs), `Priority`,
  `GoogleDriveOverlay` (TRUE only for 2015-era name matches — stays FALSE
  for modern DriveFS handlers; see research R6).
- Populated once at startup from
  `HKLM\...\ShellIconOverlayIdentifiers` (first 15, alphabetical), filtered
  by `IsDisabledCustomIconOverlays` → **FR-011 enforcement point**.
- Per-reader-thread twin array: `iconReadersIconOverlayIds` (one
  `IShellIconOverlayIdentifier*` per entry, created in each icon-reader
  thread — COM STA).

### Snooper registration (parallel arrays `WindowArray` / `ObjectArray`)
- One `CFilesWindow*` ↔ one change-notification `HANDLE` per monitored
  panel. Entry lifecycle: `AddDirectory` (path enter) → `ChangeDirectory`
  (path change) → `DetachDirectory` (leave/close).
- **State variable**: `CFilesWindow::AutomaticRefresh` (BOOL).

## State transitions: `AutomaticRefresh`

```
                 FindFirstChangeNotificationW succeeds
  [enter path] ──────────────────────────────────────────▶ TRUE  (monitored)
       │                                                    │ change signaled → panel refresh
       │  fails (path unmonitorable / provider dead)        │
       └──────────────────────────────────────────────────▶ FALSE (unmonitored)
                                                             │
   every WM_ACTIVATEAPP: Activate() → CheckPath +            │
   WM_USER_REFRESH_DIR_EX  (the busy-cursor branch,          │
   fileswn6.cpp:82 — correct ONLY for genuinely              │
   unmonitorable paths)                                      │
                                                             ▼
   next listing / path re-enter / refresh  ──▶ re-attempt → TRUE  (FR-010 recovery)
```

**Defect being fixed (RC2)**: non-ASCII paths land in FALSE not because the
path is unmonitorable but because the ANSI API received UTF-8 bytes. After
the fix, FALSE remains reachable only for genuinely unmonitorable targets
(FR-009), and recovery stays listing-driven with no background polling
(FR-010).

## Encoding conversion points (the contract's site table)

| # | Site | Input | Today | After fix |
|---|------|-------|-------|-----------|
| 1 | `fileswn1.cpp:496` icon-reader prefix | UTF-8 panel path | `CP_ACP` → garbled `wPath`; `wName` offset in bytes | `SalU8ToW` (+ACP fallback); `wName` = end of converted prefix |
| 2 | `snooper.cpp:578/720/750` | UTF-8 panel path | ANSI `FindFirstChangeNotification` | `SalU8ToW` (+ACP fallback) → `FindFirstChangeNotificationW` via new HANDLES W overload |
| 3 | `geticon.cpp:352` `SHILCreateFromPath` | UTF-8 full item path (or legacy-ACP from plugins) | `CP_ACP` → `ParseDisplayName` fails | `SalU8ToWAlloc`; NULL → legacy `CP_ACP` path preserved |
| ✓ | `shiconov.cpp:812` name part | UTF-8 item name | already `SalU8ToW` + ACP fallback | unchanged (reference pattern) |
| ✓ | `geticon.cpp:66` `SalSHGetFileInfoIcons` | UTF-8/legacy path | already compliant | unchanged (reference pattern) |

Full normative statement: [contracts/path-encoding-icon-pipeline.md](contracts/path-encoding-icon-pipeline.md).

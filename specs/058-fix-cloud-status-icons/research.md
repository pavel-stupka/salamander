# Phase 0 Research: Cloud Sync Status Icons in File Panels

**Feature**: 058-fix-cloud-status-icons · **Date**: 2026-08-18
**Method**: source analysis of the icon/overlay/snooper pipeline, git history
(`git log --follow`), and live inspection of the reporting user's machine
(registry overlay handlers, Google Drive client layout, `G:\Můj disk`).

## R1. Symptom → root-cause mapping

All three symptoms fire **iff the folder's full path contains characters
whose UTF-8 encoding differs from their ANSI (CP_ACP) encoding** — i.e. any
non-ASCII path. `G:\Můj disk` ("ů" = UTF-8 `C5 AF`) triggers all three;
`C:\Users\pavel\OneDrive` (ASCII) triggers none, which produced the
misleading "OneDrive works, Google Drive doesn't" picture. The provider is
irrelevant.

Feature 004 made panel paths and item names UTF-8 (`CFilesWindow::Path`,
`CFileData::Name`; see `SAL_MAX_PATH_UTF8` uses in `fileswn5.cpp:744`,
`fileswn7.cpp:2039`, and the "panel paths, feature 004" comments in
`shellib.cpp`). Three consumers of those strings were never converted:

### RC1 — sync-status badges missing (US1, FR-001/FR-002)

- `src/fileswn1.cpp:496` (icon-reader thread `IconThreadThreadFBody`):
  `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, path, l, wPath, …)` converts
  the **UTF-8** panel-path prefix with **CP_ACP** → `wPath` holds
  `G:\MÅ¯j disk\`.
- Feature 004 *did* fix the name half: `CShellIconOverlays::GetIconOverlayIndex`
  converts the item name with `SalU8ToW` (`src/shiconov.cpp:812`, comment
  "name is UTF-8 (feature 004)", commit `14d384c`) — but the prefix at the
  call site stayed ANSI. Since the initial commit that line was correct for
  ANSI paths; the data contract underneath it changed.
- Every overlay handler's `IsMemberOf(wPath, attrs)`
  (`src/shiconov.cpp:756`) is asked about a nonexistent path → `S_FALSE` /
  `0x80070002` → `ICONOVERLAYINDEX_NOTUSED` for every item → no badges.
- Secondary latent bug at the same site: `wName = wPath + l` uses the
  **byte** length of the UTF-8 prefix as the **WCHAR** offset. For non-ASCII
  prefixes the wide string is shorter, so even with a correct conversion the
  name would be appended past the terminator. Both must be fixed together
  (convert, then set `wName` to the end of the converted prefix).

### RC2 — busy cursor on every activation, no visible result (US2, FR-004/FR-013)

- `src/snooper.cpp:578` (`AddDirectory`), `:720` and `:750`
  (`ChangeDirectory`): ANSI `FindFirstChangeNotification(path, …)` receives
  the UTF-8 panel path → for non-ASCII paths the named directory does not
  exist → `INVALID_HANDLE_VALUE` → `win->SetAutomaticRefresh(FALSE)` (with
  `TRACE_W "auto-refresh will not work"`). `git log --follow src/snooper.cpp`
  shows the file was **never touched after the comment-translation commit** —
  feature 004 missed it entirely.
- Consequence chain on each activation: `WM_ACTIVATEAPP` →
  `WM_USER_END_SUSPMODE` (`src/mainwnd3.cpp:5833`) sets the wait cursor at
  `:5850`, calls `LeftPanel/RightPanel->Activate(FALSE)` (`:5897`).
  `CFilesWindow::Activate` (`src/fileswn6.cpp:44`) runs `CheckPath` and —
  because `!AutomaticRefresh && !GetNetworkDrive()` (`:82`) — posts
  `WM_USER_REFRESH_DIR_EX` **every time**. On a DriveFS virtual drive the
  check + re-list is slow enough for the cursor flash to be visible, and the
  refresh changes nothing the user can see (badges still missing per RC1) —
  exactly the reported "spinner, then nothing".
- This regression is wider than cloud drives: **auto-refresh is silently dead
  in every non-ASCII folder product-wide** (changes by other programs no
  longer appear automatically). Hence spec FR-013/SC-008.

### RC3 — generic base icons for Word/PDF (US4, FR-012)

- `src/geticon.cpp:352` (`SHILCreateFromPath`): `MultiByteToWideChar(CP_ACP,
  0, pszPath, -1, wszPath, …)` on the UTF-8 full item path →
  `IShellFolder::ParseDisplayName` fails → `GetFileIcon` (`:363`) returns
  FALSE → icon reader falls back to the generic icon
  (`fileswn1.cpp:386-388`).
- The irony documenting the missed-audit pattern: feature 004 added a fully
  compliant UTF-8 wrapper `SalSHGetFileInfoIcons` at `geticon.cpp:66` **in
  the same file**, 280 lines above the defect.

### Confirmation against the user's Altap observation

Altap/Open Salamander stores paths in ANSI, so all three sites are
self-consistent there: overlays, auto-refresh and icons work in
`G:\Můj disk` (CP1250 covers "ů"), and no per-activation refresh happens.
The user's hypothesis — "introduced during Tandem Commander development" —
is confirmed: all three are feature-004 regressions by omission.

## R2. Decision: fix pattern

**Decision**: convert each defective site to the feature-004 house pattern —
UTF-8 → UTF-16 via `SalU8ToW` (stack buffer) or `SalU8ToWAlloc` (heap), with
a `CP_ACP` fallback when the input is not valid UTF-8 — and call the W
variant of the affected API.

**Rationale**: this is the established, review-proven pattern used at every
already-converted site (`SalSHGetFileInfoIcons` `geticon.cpp:66-81`;
`GetIconOverlayIndex` name conversion `shiconov.cpp:812-813`; numerous
`shellib.cpp` sites). The fallback keeps legacy-ACP callers working — which
matters because `GetFileIcon` is exported to plugins via
`CSalamanderGeneral::GetFileIcon` (`spl_gen.h:2972`) and legacy plugins pass
ACP strings (`pluglegacy.h` contract).

**Alternatives considered**:
- *Enable the UTF-8 process code page (manifest `activeCodePage`)* — would
  make CP_ACP == UTF-8 process-wide and silently "fix" every missed site.
  Rejected: a process-wide behavioral change affecting all 28 plugins and
  every ANSI API in the product; violates constitution III (incremental) and
  the feature-004 architecture already committed to explicit conversion;
  legacy plugins passing real ACP strings would break.
- *Rewrite the icon reader / snooper W-native* — rejected: big-bang rewrite
  of working architecture for no additional user value.

## R3. RC2 fix specifics (snooper)

- Convert the path once per call site (after `MakeCopyWithBackslashIfNeeded`,
  which takes the pointer **by reference** and may swap it to the
  backslash-suffixed copy — `salamdr5.cpp:1196`; its semantics must be
  preserved, so convert the *final* pointer).
- Call `FindFirstChangeNotificationW`. The HANDLES tracking layer only wraps
  the ANSI variant (`C__Handles::FindFirstChangeNotification`,
  `common/handles.cpp:2226`); add a W overload there (mechanical, same
  `__hoFindFirstChangeNotification` bookkeeping) so handle-leak tracking is
  preserved.
- Failure behavior unchanged: `SetAutomaticRefresh(FALSE)` + `TRACE_W`
  remains the fallback for genuinely unmonitorable paths, which also
  satisfies FR-009/FR-010 (see R7).

## R4. RC1 fix specifics (icon-reader prefix)

- Replace the `CP_ACP` conversion of the panel-path prefix with `SalU8ToW`
  (+ `CP_ACP` fallback for invalid UTF-8, mirroring `shiconov.cpp:812`).
- Set `wName` to the end of the **converted wide** prefix (its actual WCHAR
  length), not to the UTF-8 byte length `l`. The `char* name = path + l`
  side is correct as-is (UTF-8 bytes) and feeds `GetIconOverlayIndex`'s
  existing `SalU8ToW` name conversion.
- The `MAX_PATH` guard inside `GetIconOverlayIndex` (`shiconov.cpp:806-807`)
  mixes wide-prefix length and UTF-8 name byte-length; the mix is
  conservative (byte count ≥ WCHAR count) and needs no change.

## R5. RC3 fix specifics (SHILCreateFromPath)

- Convert with `SalU8ToWAlloc`; on NULL (invalid UTF-8) fall back to the
  legacy `CP_ACP` conversion — preserving behavior for legacy plugin callers
  of `GetFileIcon` that pass ACP strings (constitution V).
- `MAX_PATH` truncation of `wszPath` is pre-existing and out of scope
  (icon reading is skipped for over-long paths upstream of this call,
  `fileswn1.cpp:484`, feature 027).

## R6. Google Drive handler registry — investigated, deliberately not changed

Live registry on the repro machine
(`HKLM\...\Explorer\ShellIconOverlayIdentifiers`): the modern Drive for
desktop client registers `GoogleDriveCloudOverlayIconHandler`,
`GoogleDriveMirrorBlacklistedOverlayIconHandler`,
`GoogleDrivePinnedOverlayIconHandler`,
`GoogleDriveProgressOverlayIconHandler` (plus `OneDrive1..7`). None of these
match the hardcoded 2015-era name list in `InitShellIconOverlaysAuxAux`
(`shiconov.cpp:261-269`), so:

- the handlers are **not** flagged `GoogleDriveOverlay` and are therefore
  called for every path, un-gated and without the `GD_CS` serialization —
  which is exactly how current Altap behaves too, works in practice, and is
  what will make badges appear on `G:` once RC1 is fixed;
- the legacy gating machinery (`InitGoogleDrivePath` reading the defunct
  Backup-&-Sync `sync_config.db` via `utils\sqlite.dll`;
  `%LOCALAPPDATA%\Google\Drive\...` does not exist on the repro machine,
  `%LOCALAPPDATA%\Google\DriveFS` does) is effectively dead code on modern
  systems.

**Decision**: leave the name list, the gating, and `InitGoogleDrivePath`
untouched. **Rationale**: adding the new names to the list would *re-break*
`G:` badges — the gate only allows GD handlers under the (wrongly detected,
nonexistent) `%USERPROFILE%\Google Drive` — unless DriveFS mount-point
detection were also built, which is new scope with real risk and no
user-visible gain over the un-gated behavior Altap already demonstrates to
be acceptable. The `GD_CS` crash guard targeted the 2015 handler's heap bug;
the modern handler runs un-gated in both Altap and TC today without crashes,
and every `IsMemberOf` call is already wrapped in SEH. Documented as a known
trait; revisit only if the modern handler misbehaves. (This also explains
the pre-existing "syncing badge sometimes missing" imperfection the user
accepts in Altap — it is provider-side, not ours to fix here.)

## R7. Clarification-driven requirements — how the design satisfies them

- **FR-010 (recovery on next listing/refresh, no polling)**: automatic —
  `AddDirectory`/`ChangeDirectory` re-attempt `FindFirstChangeNotificationW`
  on every path change and refresh cycle; no negative caching exists in that
  path. With RC2 fixed, a provider that appears later is picked up the next
  time the folder is listed. No new code needed.
- **FR-011 (existing overlay configuration governs)**: badges remain shell
  icon-overlay handlers filtered by `IsDisabledCustomIconOverlays`
  (`shiconov.cpp:448`, `Configuration.EnableCustomIconOverlays` +
  per-handler disable list) — untouched, so the setting keeps governing all
  cloud badges. No new configuration added.
- **SC-001 2-second visibility**: unchanged mechanics — the icon reader
  prioritizes `VisibleItemsArray` and reads overlays for visible items first
  (`fileswn1.cpp:576-663`); RC1 only fixes the query input. Validated
  manually per quickstart.

## R8. Verification strategy

- **Provider-independent repro** (no Google Drive needed): a local folder
  with a diacritic name (e.g. `D:\Test\Zkouška`) reproduces RC2 (no
  auto-refresh + activation flash) and RC3 (generic icons); a diacritic
  folder inside a OneDrive sync root reproduces RC1 (badges missing).
  This makes acceptance deterministic on any Czech-locale machine.
- **Real-target validation**: `G:\Můj disk` parity with Explorer (SC-001),
  10× focus cycles (SC-002), OneDrive regression check (SC-005).
- **Automated**: full Debug+Release build; existing `saltests` (already
  covers `SalU8ToW` helpers — 25 references). A dedicated unit test is
  practical only if a helper is extracted; the three sites are thread/UI
  bound. Optional: extend `tools/check_encoding.py` tracked identifiers if a
  new contract identifier is introduced (decided at tasks stage).
- **TRACE evidence**: before/after, the debug build's `TRACE_W "Unable to
  receive change notifications"` line must disappear for diacritic paths.

## R9. Out-of-scope notes recorded for future features

- Outdated GD handler-name list + dead `sync_config.db`/`sqlite.dll` gating
  machinery (R6) — candidate for a separate cleanup/modernization feature.
- Explorer's *Status column* (storage-provider properties) is a different
  mechanism from icon overlays and remains out of scope per spec assumption.
- `SHILCreateFromPath`'s `MAX_PATH` bound (long-path icons) — feature 027
  already routes over-long paths away from icon reading.

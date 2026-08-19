# Investigation Leads: Shell Icon Overlay System (feature 061)

**Purpose**: Technical exploration notes gathered while writing the spec. This is
*input for the plan/analysis phase*, not a design decision record. Every lead below
must be confirmed or refuted with evidence during `/speckit-plan`; the spec itself
stays at the behavior level.

**Gathered**: 2026-08-19, by code exploration of the working tree (branch point
`main` @ 254bcba). Line numbers valid as of that revision.

## Architecture map (verified)

| Item | Location |
|---|---|
| Overlay item + manager classes | `src/shiconov.h:27-40` (`CShellIconOverlayItem`), `:42-127` (`CShellIconOverlays`), `:129-133` (`CShellIconOverlayItem2`, config-UI flat list), `:135-136` (globals) |
| Implementation | `src/shiconov.cpp` (globals `:46-47`) |
| Registry enumeration | `shiconov.cpp:368-510` `InitShellIconOverlays()` — `HKLM\Software\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers` (`:382-384`), ANSI `RegEnumKeyEx` (`:386-407`), CLSID from subkey default value (`:418`, `:428`), description from `HKCR\CLSID\<clsid>` (`:432-463`, config dialog only) |
| Ordering | Insertion sort, case-insensitive ascending by key name (`stricmp`, `:396-398`). Comment `:408-409`: Explorer prioritizes alphabetically; "we take only the first 15; Explorer actually takes only the first 11". No early break — extras are still `CoCreateInstance`d, then discarded |
| Hard cap = 15 | `CShellIconOverlays::Add` `shiconov.cpp:676-699` refuses at 15 (`:680-684`). Rooted in `ICONOVERLAYINDEX_NOTUSED 15` (`src/plugins/shared/spl_com.h:200`) and the 4-bit field `IconOverlayIndex : 4` in `CFileData` (`spl_com.h:228`) → only indices 0–14 representable |
| Priority | Sorting by handler priority explicitly disabled (dead code `:685-693`); `GetPriority` stored (`:252-257`, default 100) and used only as a query-time filter (`:923-924`) |
| Per-reader-thread COM instances | `CreateIconReadersIconOverlayIds` `shiconov.cpp:736-763` (one instance per entry per icon-reader thread, STA) |
| Hardcoded name matching | Only Google Drive (`shiconov.cpp:295-303`, 9 exact names + `_DEBUG` rename warning `:310-311`) and the feature-059 synthetic `"TandemCloudSyncPending"` (`:37`). No "Tortoise"/"OneDrive"/"EnhancedStorage" special-casing anywhere |
| Built-in overlays (shortcut arrow, shared hand, offline clock) | Separate mechanism (`HShortcutOverlays` etc., `salamdr1.cpp:2053+`), drawn only when `IconOverlayIndex == ICONOVERLAYINDEX_NOTUSED` (`fileswn4.cpp:2176-2192`, `:2219+`) — **out of scope** for this feature |

## Call path per panel item (verified)

1. Startup: `salamdr1.cpp:4317-4319` — `LoadIconOvrlsInfo(SALAMANDER_ROOT_REG); InitShellIconOverlays();` (runs *before* `MainWindow->LoadConfig()` at `:4432`). Teardown `salamdr1.cpp:4931`.
2. Icon-reader thread `fileswn1.cpp:402` → `OleInitialize` `:411` → per-thread handler objects `:414`.
3. Wide path prefix built once per work cycle `fileswn1.cpp:479-528`; ≥ MAX_PATH panel path ⇒ overlay reading skipped (feature 027, `:485-489`); feature 058 `SalU8ToW` conversion (`:497-508`); feature 059 `IsCloudSyncRootPath` gate (`:525-527`).
4. Overlay pass runs once per work cycle after the icon/thumbnail pass (`canReadIconOverlays` `:592`, `:1107-1112`, `:1129`; per item `IconOverlayDone` `:695-697`, reset `:575-581`).
5. `CShellIconOverlays::GetIconOverlayIndex` `shiconov.cpp:891-958`: length guards `:899-904`; feature-004 `SalU8ToW` name conversion `:905-906`; vestigial `strcpy(aName, name)` `:908` (its consumer `SHGetFileInfo` commented out `:910-911`); handler loop `:918-942` (synthetic skip `:921-922`, priority filter `:923-924`, Google Drive gate `:926-935`); `IsMemberOf(wPath, shAttrs)` via `:791-806`; feature-059 property fallback `:952-956`.
6. Encoding at the COM boundary is sound: `IsMemberOf` receives a genuine `WCHAR*`; `SalU8ToW` terminator arithmetic in `fileswn1.cpp` verified correct against `src/common/salunicode.cpp:15-42`.
7. Pre-existing deviation (unchanged since Altap `3945ecf`): the second `IsMemberOf` argument is `FILE_ATTRIBUTE_*` (`CFileData::Attr`), not `SFGAO_*` shell attributes (the `SHGetFileInfo(SHGFI_ATTRIBUTES)` producer is commented out). Harmless for Tortoise (ignores `dwAttrib`) but wrong in principle.

## Configuration (verified)

- `Configuration.EnableCustomIconOverlays` (`src/cfgdlg.h:422`), `Configuration.DisabledCustomIconOverlays` (`:423`; `;`-separated, `;;` escape — parser `shiconov.cpp:523-563`, writer `:585-624`). Defaults TRUE / NULL (`src/dialogs4.cpp:550-551`).
- Registry: `HKCU\<SALAMANDER_ROOT_REG>\Configuration`, values `"Enable Custom Icon Overlays"` (DWORD) and `"Disabled Custom Icon Overlays"` (SZ) — `mainwnd2.cpp:337-338`; written `:1740-1743`; read early by `LoadIconOvrlsInfo` `mainwnd2.cpp:2303-2355`.
- Enforcement: `IsDisabledCustomIconOverlays` `shiconov.cpp:566-574`, called at `:482` and `:1158`.
- UI: Configuration → Icon Overlays page (`dialogs4.cpp:718`; `CCfgPageIconOvrls` `dialogs6.cpp:2511-2578`); unchecked rows rewritten into the disabled list on OK (`:2549-2560`), restart-required box (`:2565`).
- Crash escape hatch: `InformAboutIconOvrlsHanCrash` `callstk.cpp:576-647` — writes disable state straight into the registry; a single past crash can permanently hide one vendor's overlays.

## Delta vs. Altap-era code (verified)

`git log` over `src/shiconov.cpp|.h`: `3945ecf` (initial) → comment translation only (`4c26789`, `aa615a7`) → `14d384c` [004] → `1548d92` [059] → `a4da9bf` [060 docs]. Filtering `git diff 3945ecf` for non-comment lines, the only semantic changes in the overlay path are: (004) UTF-8 name conversion + ANSI guard; (058) wide-prefix build in the reader; (059) synthetic entry + `Identifier == NULL` skips + `HRESULT res` init fix + property fallback + cldapi gate. Enumeration, cap, ordering, priority handling and `IsMemberOf` invocation are byte-for-byte Altap logic.

## Root-cause suspects (to confirm/refute in plan phase)

- **S1 — VERIFIED code defect (encoding), most likely functional cause of "badge never appears".**
  `mainwnd3.cpp:1400` uses ANSI `SHGetPathFromIDList` (CP_ACP bytes) and passes the result to
  `CFilesWindow::IconOverlaysChangedOnPath` (`mainwnd3.cpp:1414-1419`), which gates on
  `IsTheSamePath(path, GetPath())` (`fileswn7.cpp:2091`) — but `GetPath()` is UTF-8 since
  feature 004. Feature 058 did **not** touch this site. Hypothesis: Tortoise's `IsMemberOf`
  answers S_FALSE on first ask (async cache) and relies on the `SHCNE_UPDATEITEM` →
  re-read loop; overlays are read only once per cycle per item, so the dropped
  notification means the badge never appears. Bites only non-ASCII paths.
- **S2 — HYPOTHESIS (environmental, high plausibility): 15-slot ceiling + alphabetical order.**
  `specs/058-fix-cloud-status-icons/evidence.md:39-44` documents the dev machine's registry:
  `OneDrive1..7` + 4 modern Google Drive handlers = 11 handlers. TortoiseGit registers ~9 keys;
  depending on sort position, few or none fit before `Add()` refuses at 15. Reinforcing comment
  `shiconov.cpp:346`: Tortoise's own `GetOverlayInfo` returns an error when it is beyond the
  shell limit ("more than 12 handlers registered") — the handler is then silently dropped.
- **S3 — HYPOTHESIS (init kill switch).** `LoadIconOvrlsInfo` `mainwnd2.cpp:2345-2349`: if either
  registry value is missing while config version ≥ 41, `EnableCustomIconOverlays` is forced
  FALSE → all handlers skipped. Feature 057's Altap-migration script excludes exactly those two
  values (`utils/migrate-altap-settings.cmd:414`) while writing config version 105 (`:37`).
  Counter-evidence: this would also kill OneDrive badges and the 059 synthetic entry.
- **S4 — HYPOTHESIS (sticky per-handler disable).** A past crash + "Disable this handler" writes
  the name permanently (`callstk.cpp:604-642`). Check `HKCU\…\Configuration\Disabled Custom Icon Overlays`.
- **S5 — VERIFIED (not a regression, Altap-era):** overlay icon file path from `GetOverlayInfo` is
  converted with `WideCharToMultiByte(CP_ACP, …)` (`shiconov.cpp:263`, again in `ColorsChanged`
  `:991`); all three icon sizes must extract or the handler is dropped (`:289`, `:336`).
  Non-ACP-representable icon paths ⇒ silent drop. Affects file-based handlers (Tortoise) more
  than DLL-resource ones (OneDrive).
- **S6 — HYPOTHESIS:** bitness mismatch ⇒ `CoCreateInstance` fails silently (`shiconov.cpp:351`,
  `:719`) — same trap the code documents for Google Drive at `:1107-1116`. (x64 Explorer would
  fail equally, so this cannot explain an Explorer-vs-TC difference.)
- **S7 — VERIFIED, no defect:** the 059 synthetic entry is appended last (`shiconov.cpp:1183-1192`),
  cannot displace a real handler; at 15 loaded handlers the 059 feature silently self-disables
  (`CloudSyncPendingIndex` stays −1).

## Diagnostics gap (verified)

All failure reporting in this subsystem is `TRACE_I`/`TRACE_E` only (≈21 sites in
`shiconov.cpp`); `TRACE_ENABLE` is defined only in `src/vcxproj/sal_debug.props:11`.
In a Release build, every overlay-handler load failure — bad CLSID, `CoCreateInstance`
failure, `GetOverlayInfo` error, missing icon sizes, the 15-handler cap — is invisible.
The only user-visible diagnostic is the crash dialog (`IDS_ICONOVRLS_CRASH`).

## Fastest discriminating experiment

Run a Debug build with the trace server attached, open a Git working copy, read the
`InitShellIconOverlays()` trace lines: they state directly whether Tortoise handlers
were enumerated, whether `GetOverlayInfo` failed, or whether `Add()` hit the 15-handler
cap. That separates S2/S5/S6 (never loaded) from S1 (loaded but never re-asked) in one run.
Additionally dump `HKLM\...\ShellIconOverlayIdentifiers` and
`HKCU\...\Configuration\Disabled Custom Icon Overlays` on the affected machine (S3/S4).

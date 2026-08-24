# Feature 068 — Remaining Work (handoff)

**Written**: 2026-08-24 · **Branch**: `068-encoding-regression-review` · **Last commit at time of writing**: see `git log -1`

This file is self-contained: it is enough to resume without re-reading the 20k-line review record. Full evidence lives in `specs/068-encoding-regression-review/` — `review-report.md` (findings, verdicts, fixes, gates), `findings/P*.md` (raw perspective reports), `findings/verdicts-V*.md` (independent verdicts), `findings/regression-X*.md` (fix reviews).

---

## 0. What was finished (so this is not re-done)

| | |
|---|---|
| Findings | 76 raised → **60 confirmed**, 8 refuted, 4 latent, 2 by-design, 2 withdrawn |
| Fixes | **9**, each independently regression-reviewed and accepted (X01–X09) |
| Rejections survived | 3 — all regressions the fixes themselves introduced |
| Tests | `saltests` 1229 → **1257**, 0 failed |
| Guard | `check_encoding.py`: 3 rules promoted to strict (**9 total**), each proven to fire |
| Ledger | **89/89** rows re-dispositioned |
| Plugin ABI | untouched — no `src/plugins/shared/` diff, no forwarder diff, interface 106 |
| Gates | G1–G5 pass · G6 N/A · G7 automated half passes |

**Fixed already — do not re-open**: X01 command-line stack overrun · X02 the feature-052 regression in Plugins Manager · X03 taskbar jump list · X04 rubber-band over-selection · X05 per-drive remembered directory · X06/X07 disk-cache and temp-tree cleanup (incl. the `RemoveTemporaryDir` plugin service) · X08 ZIP overwrite prompt · X09 filecomp window title.

---

## 1. The main remainder — 34 confirmed, contained, not reached

Each is **verified by an independent refute-first reviewer**, has a recorded failure scenario and a traced data path, and needs **no systemic redesign** — the same shape as the nine already fixed. Ordered by consequence.

### A. Same keystroke as a fix already made — finish the job

#### F-P6-04 — Ctrl+Enter / Ctrl+Space push raw UTF-8 bytes into the ANSI command line, so the command runs against a name that does not exist

- **Verdict**: CONFIRMED (Ctrl+Enter and Ctrl+Space / Ctrl+[ / Ctrl+]); the drag-drop clause is only partly right

- **Symptom**: **Surface** the command line under the panels. **Locale/UI** any; the trigger is any file or folder name that is not plain ASCII — Czech `Přehled.txt` on a CP1250 machine already reproduces it. **Input** focus the command line, focus `Přehled.txt` in the panel, press **Ctrl+Enter** (insert focused …

- **Data path**: `src/editwnd.cpp:891` `s = file->Name` (`CFileData::Name`, UTF-8 by the feature-004 contract) → `:899-901` `memmove(path, s, l); … InsertText(path)` → `src/editwnd.cpp:353-355` `SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM)s)` — `SendMessage` is `SendMessageA` in this build and the edit …


### B. Data or function silently lost

#### F-P4-01 — the viewer's default character-set conversion is silently lost on every restart when its name is non-ASCII (Kamenické / KOI-8 ČS2)

- **Verdict**: CONFIRMED

- **Symptom**: **Internal Viewer → Coding menu**, on any system whose best-matching conversion directory is `convert\centeuro` (Central-European ACP, i.e. the primary Czech/Polish/Hungarian/Slovak install — the shipped Czech, Polish, Hungarian, Slovak languages), **any UI language**. The user opens a file, picks …

- **Data path**: 1. `src/codetbl.cpp:155-157` — the conversion **name is copied raw out of `convert\centeuro\convert.cfg`**: `memcpy(name, beg, l); name[l] = 0;` — no conversion of any kind. That file is **CP1250-encoded** (verified at byte level: `Kameni\xe8t\xed`, `KOI-8 \xc8S2`, 13 non-ASCII bytes). So …


#### F-P4-02 — a non-ASCII coding name poisons the whole viewer window caption, mojibaking the file name

- **Verdict**: CONFIRMED (mechanism and scenario) · cause attribution PARTLY REFUTED

- **Symptom**: Internal Viewer, Central-European install (`convert\centeuro`), any UI language. The user views a file whose **name contains non-ASCII characters** (e.g. `poznámky.txt`) and switches the coding to *"Kamenické - CP1250"*. The window caption is composed as `<path> - [<coding name>]`; the coding name …

- **Data path**: `src/viewer3.cpp:53` `CodeTables.GetCodeName(CodeType, codeName, 200)` (CP1250 bytes, see F-P4-01 step 1) → `:59` `sprintf(caption + strlen(caption), " - [%s]", codeName)` where `caption` already holds the UTF-8 file name → `:62` `WCHAR* captionW = SalU8ToWAlloc(caption);` → NULL → `:69` …


#### F-P1-08 — `SHGetFolderPath` (A) results are fed to the strict UTF-8 facade, so per-user data under a non-ASCII account name is never found

- **Verdict**: CONFIRMED (three consequences checked; one needs correcting)

- **Symptom**: any shipped language, Windows account name with a non-ASCII character (very common for cs/hu/pl users: `Jiří`, `Šárka`, `Kovács`). Three visible consequences: (a) an auto-import `config.reg` placed in `%APPDATA%\Tandem Commander` is never picked up (`src/salamdr1.cpp:3570`); (b) Google Drive is …

- **Data path**: `src/salamdr5.cpp:1855` `SHGetFolderPath(…, buf)` (A ⇒ CP_ACP) → `SalPathAppend` → `src/salamdr1.cpp:3570` `FileExists(curDir)` → `src/salamdr2.cpp:696` `SalGetFileAttributes(fileName)` → `src/common/salfileio.cpp:239` `SalPathToWExtAlloc(u8path)` → `src/common/salpath.cpp:260` `SalU8ToWAlloc` …


#### F-P1-10 — Install-directory paths obtained with `GetModuleFileName` (A) are consumed by strict UTF-8 helpers

- **Verdict**: CONFIRMED

- **Symptom**: an installation directory containing a non-ASCII but ACP-representable character — e.g. the user installs into `D:\Programy\Tandém Commander` on a Czech (CP1250) or Western (CP1252) Windows, or runs the app portably from `E:\Nástroje\TC`. `GetModuleFileNameA` returns those bytes in the ACP; …

- **Data path**: `src/mainwnd3.cpp:171` `GetModuleFileName(HInstance, CurrentHelpDir, MAX_PATH)` → `:174 DirExists(CurrentHelpDir)` → `src/salamdr2.cpp:724` `SalGetFileAttributes(dirName)` (strict). `src/salamdr1.cpp:3566` → `:3570 FileExists(ConfigurationName)`. `src/execute.cpp:828` `ExecuteExpSalDir` → the …


#### F-P1-19 — Compare Directories cannot read files or subdirectories with non-ASCII names

- **Verdict**: CONFIRMED (with one correction)

- **Symptom**: any shipped language. *Commands ▸ Compare Directories* with "by content" on two trees containing `Smlouva – kopie.docx` or a subdirectory `Účetnictví`. `CreateFile` (A) fails, so the pair is reported as an error ("Cannot open the file") or as different; `FindFirstFile` (A) on a non-ASCII …

- **Data path**: `src/mainwnd5.cpp:292,294` `CreateFile(file1/file2, …)` (A); `:572 FindFirstFile(path, &data)` (A) — `path` is built from the panel path in the same function (`LoadStrU8(IDS_CANNOTREADDIR)` at :590 confirms it is UTF-8).


#### F-P1-20 — Writing an edited file back into an archive uses the ANSI `SHFileOperation`

- **Verdict**: CONFIRMED

- **Symptom**: any shipped language. F4-edit a file inside an archive (`CFileTimeStamps`), save it, answer Yes to "update the archive". The copy from the temp directory back to the staging location is done with the ANSI `SHFileOperation`; when the file name is non-ASCII (or `%TEMP%` is), the copy silently does …

- **Data path**: `src/salamdr3.cpp:3218-3243` (`fromStr`/`toStr` built from the temp path + UTF-8 names) → `:3254 SHFileOperation(&fo)` (A). The W twin is used two files away (`src/fileswn8.cpp:39`, `src/finddlg2.cpp:190-200`) — DC-18.


#### F-P6-01 — mdview's keeper window class is never unregistered, so after a Plugins Manager Unload the "instant view" engine keeper silently never arms again

- **Verdict**: CONFIRMED

- **Symptom**: **Surface** Markdown Viewer (mdview), any UI language. **Steps** (1) F3 a `.md` file — mdview loads, `MdKeeperArm()` registers `TandemMdKeeperWnd` with mdview's `DLLInstance` and arms the keeper; (2) close the viewer; (3) Plugins → Plugins Manager → select Markdown Viewer → **Unload** …

- **Data path**: `src/plugins/mdview/webview.cpp:795-810` (`MdKeeperArm` → `RegisterClassW(&wc)`, `wc.hInstance = DLLInstance`; `if (RegisterClassW(&wc) == 0) return;`) · no `UnregisterClassW` anywhere in `src/plugins/mdview/*.cpp` (verified by grep) · contrast `src/plugins/shared/winliblt.cpp:134-137` …


#### F-P1-03 — Startup cleanup of leftover `SAL*.tmp` directories misses them under a non-ACP `%TEMP%`, and hands an ACP path to a UTF-8 consumer

- **Verdict**: CONFIRMED IN PART / REFUTED IN PART

- **Symptom**: `ClearTEMPIfNeeded` runs at startup and offers to delete leftover `SAL*.tmp` directories. Its `tmpDir` is ACP; the "Focus" button then sends that ACP path to the panel as a UTF-8 path (`WM_USER_FOCUSFILE`), and the "Delete" button calls the ANSI `RemoveTemporaryDir` (F-P1-04). With an ACP- …

- **Data path**: `src/cache.cpp:1461` `GetTempPath` (A) → `:1470 FindFirstFile` (A) → `:1522 RemoveTemporaryDir(tmpDir)` / `:1527 SendMessage(hActivePanel, WM_USER_FOCUSFILE, …, (LPARAM)tmpDir)`.


### C. Cloud / drive / volume paths taken as CP_ACP

#### F-P1-09 — OneDrive / Dropbox root paths are stored as CP_ACP and then used as panel paths (ledger L02)

- **Verdict**: CONFIRMED IN PART / REFUTED IN PART

- **Symptom**: any shipped language, non-ASCII account name; the personal OneDrive root is `C:\Users\Jiří\OneDrive`. Windows hands the path over **wide** (`SHGetKnownFolderPath(FOLDERID_SkyDrive)`), and the code immediately degrades it to CP_ACP. Clicking the OneDrive item in the drive bar / Alt+F1 menu then …

- **Data path**: `src/drivelst.cpp:1477` `DynSHGetKnownFolderPath(my_FOLDERID_SkyDrive, …, &path)` (wide, correct) → `:1481 ConvertU2A(path, -1, OneDrivePath, _countof(OneDrivePath))` — default `codepage = CP_ACP` (`src/common/strutils.h:17-18`) → `src/fileswn3.cpp:2585` `strcpy_s(path, OneDrivePath)` → change- …


#### F-P4-05 — the OneDrive folder path is obtained in CP_ACP and then used as a UTF-8 panel path

- **Verdict**: CONFIRMED (mechanism) · the "OneDrive-specific" framing REFUTED

- **Symptom**: **Drive bar / Change Drive menu (`Alt+F1`) -> OneDrive**, any UI language. On any machine whose OneDrive folder path contains a non-ASCII character — the common cases are a Windows account name with an accent (`C:\Users\Jiri\OneDrive` with diacritics) and a OneDrive **Business** tenant whose …

- **Data path**: S-B7-P4-010, S-B7-P4-011, S-B5-P4-004


#### F-P1-12 — `MyGetVolumeInformation`/`MyGetDiskFreeSpace` degrade silently on non-ASCII and UNC paths (ledger L03)

- **Verdict**: CONFIRMED IN PART (mechanism corrected) / one claim strengthened

- **Symptom**: (a) a volume mounted into a directory whose path contains non-ASCII characters (`C:\Disky\Zálohy\` as a mount point): the ANSI `GetVolumeInformation` fails, the `CutDirectory` loop walks up to `C:\`, and the information line / the copy-move pre-checks report the **parent** volume's file system and …

- **Data path**: `src/salamdr2.cpp:1430-1443`: `ResolveSubsts` → `GetRootPath(ourPath, resPath)` → `ResolveLocalPathWithReparsePoints(ourPath, path, …)` (writes the **full** path into `ourPath`, `src/salamdr2.cpp:1489`) → `:1440 GetVolumeInformation(ourPath, …)` (A).


#### F-P1-13 — `subst` targets are resolved through the ANSI `QueryDosDevice`, producing mixed-encoding paths (ledger L04)

- **Verdict**: CONFIRMED (narrow)

- **Symptom**: `subst X: D:\Dokumenty\Šablony`, then work on `X:` in a panel. `ResolveSubsts` replaces `X:` with the CP_ACP target and appends the UTF-8 remainder, producing a path that is neither valid UTF-8 nor valid ACP. Consequences: the delete confirmation for a reparse point no longer says …

- **Data path**: `src/salamdr2.cpp:1781` `QueryDosDevice(deviceName, target, MAX_PATH)` (A) → `:1803 lstrcpyn(path, target + 4, pathMax)` → `src/salamdr2.cpp:1267` `SalPathAppend(tgt /*ACP*/, resPath + 2 /*UTF-8*/, MAX_PATH)` → `:1277 lstrcpyn(resPath, tgt, MAX_PATH)`.


#### F-P1-14 — Volume labels, drive display names and mapped-drive UNC paths are acquired in CP_ACP (ledger L05)

- **Verdict**: CONFIRMED IN PART / REFUTED IN PART

- **Symptom**: any shipped language. A removable/fixed disk labelled with characters outside the system ACP — e.g. a drive labelled `Резерв` or `Δεδομένα` on a Czech (CP1250) machine, or `Zálohy Dokumentů` on an English (CP1252) machine (`ů` is not in CP1252). The Alt+F1 drive menu and the drive-bar tooltip show …

- **Data path**: `src/drivelst.cpp:1739` `GetVolumeInformation(root, volumeName, MAX_PATH, …)` (A) → `:1743 DuplicateAmpersands(volumeName, MAX_PATH)` → the menu item text; `src/drivelst.cpp:2622` → `:2632 sprintf(text, "%s (%s)", volumeName, freeSpaceText)` → drive-bar tooltip; `src/drivelst.cpp:1769` …


#### F-P1-27 — Shell/network strings degraded to CP_ACP at intake

- **Verdict**: CONFIRMED (narrow)

- **Symptom**: any shipped language. (a) A local share named `Účetnictví` (the API `NetShareEnum` is W-only, so the true name is available): the shared-folder overlay/marker is not applied to that directory because the cached ACP name never matches the UTF-8 panel path. (b) Names obtained from …

- **Data path**: `src/shares.cpp:105-107` (`WideCharToMultiByte(CP_ACP, 0, p->shi502_netname/_path/_remark, …)`); `src/shellib.cpp:510,579,1565,1848,1954,3039`.


### D. External-archiver subsystem

#### F-P1-05 — External-archiver list files are written in the wrong encoding (`CharToOem` on UTF-8), and archiver output is parsed as OEM into UTF-8 name fields

- **Verdict**: CONFIRMED IN PART / REFUTED IN PART

- **Symptom**: an archiver configured in *Options ▸ Archivers* that is an external EXE — by default RAR (`.rar`), ARJ (`.arj`), LHA (`.lzh`), UC2, ACE (`src/pack3.cpp:411-438` `AddDefault` maps those five extensions to external archivers 1/9/3/4/10). Pack `Žluťoučký kůň.txt` into `test.rar` (Alt+F5): the list …

- **Data path**: pack: `nextName(...)` (`CFileData::Name`, UTF-8) → `src/pack1.cpp:1498` `CharToOem(name, namecnv)` → `fprintf(listFile, …)`. list: archiver stdout → `src/pack1.cpp:302` `OemToCharBuff(pomptr, newfile.Name, newfile.NameLen)` → `CFileData::Name`.


#### F-P1-06 — The external-packer subsystem uses ANSI file APIs on UTF-8 temp/archive paths throughout

- **Verdict**: CONFIRMED

- **Symptom**: same configuration as F-P1-05, plus a non-ASCII `%TEMP%` or a non-ASCII target directory. Unpack `test.rar` into `D:\Zálohy`: `SalGetTempFileName(targetDir,…)` creates `D:\Zálohy\PACK###.tmp` correctly (W), then `fopen(tmpListNameBuf,"w")` fails (narrow CRT uses the ACP), the packer aborts with …

- **Data path**: `src/pack1.cpp:1453` `SalGetTempFileName(targetDir,"PACK",tmpDirNameBuf,FALSE)` (UTF-8) → `:1477 fopen(tmpListNameBuf,"w")` / `:1479 RemoveDirectory(tmpDirNameBuf)` / `:1863 FindFirstFile(extractedFile,…)` → `:1890 srcName = tmpDirNameBuf + "\\" + foundFile.cFileName` (UTF-8 + **ACP**) → …


#### F-P1-07 — `salspawn.exe` path taken with `GetModuleFileName` (A)

- **Verdict**: CONFIRMED (narrow: needs a non-ASCII install path **and** an external archiver)

- **Symptom**: covered by Note N-2 (install directory outside the ACP). Listed separately only because `InitSpawnName` feeds the result into the command line of `SalCreateProcess`, which is a **UTF-8** consumer (`src/pack3.cpp:1752-1755` `sprintf(tmpCmdLine, "\"%s\" %s %s", SpawnExe, …)`), so this one is a DC-09 …

- **Data path**: `src/pack3.cpp:363` `GetModuleFileName(NULL, SpawnExe, MAX_PATH)` → `src/pack3.cpp:1752` `sprintf(tmpCmdLine, …, SpawnExe, …)` → `:1755 SalCreateProcess(NULL, tmpCmdLine, …)`.


### E. Composed messages and list/dialog text

#### F-P2-04 — The safe-wait window ("Reading path …", "Checking path …", plugin loading) mixes template + path

- **Verdict**: CONFIRMED for the two path sites; the seven plugin-loading sites are LATENT

- **Symptom**: Czech/German/French/Hungarian/Slovak UI, entering a slow/network path whose name contains a non-ASCII character — the wait window shows the path as mojibake; likewise the plugin-loading splash when the plugin DLL lives under a non-ASCII directory.

- **Data path**: `src/fileswn3.cpp:285` → `CreateSafeWaitWindow` (`src/salamdr2.cpp:556`, stores into `SafeWaitMessageText`) → `src/salamdr2.cpp:401`/`:456` `waitWnd.SetText(SafeWaitMessageText)` → `CWaitWindow::PaintText` → `src/dialogs3.cpp:2735` `SalU8ToWAlloc(Text)` returns NULL → `:2742 DrawText` (A).


#### F-P2-07 — Drive Information: the "type" line mixes ANSI templates with UNC / SUBST / link-target paths

- **Verdict**: CONFIRMED for the junction/symlink row only · UNC and SUBST rows REFUTED

- **Symptom**: Czech/Hungarian/Slovak UI, Ctrl+F1 on a mapped network drive (`IDS_INFODLGTYPE8` non-ASCII cs/hu/sk) or Czech/French/Hungarian on a junction (`IDS_INFODLGTYPE9`); the share/target path renders as mojibake even though feature 067 fixed the numbers in the same dialog.

- **Data path**: S-B3-P2-008


#### F-P2-09 — Plugin Manager: only the Name column was converted; Location (and Version) stayed ANSI

- **Verdict**: CONFIRMED for the Location column (narrow) · LATENT for the Version column

- **Symptom**: Plugin Manager (Plugins → Plugin Manager), any UI language, a plugin whose `.spl` was added from a directory outside `plugins\` whose path contains a non-ASCII character (`D:\Můj plugin\x.spl`) — `DLLName` then keeps the absolute path (`src/plugins2.cpp:1374-1380` only strips the `plugins\` …

- **Data path**: registry read via the feature-004 facade (UTF-8) → `CPlugins::AddPlugin` → `CPluginData::DLLName` → `src/plugins2.cpp:1056` `ListView_SetItemText` (A).


#### F-P2-10 — Plugin Manager: the "Change Drive menu" checkbox mixes an ANSI template read from the dialog with a UTF-8 FS item name

- **Verdict**: CONFIRMED (languages narrower than claimed)

- **Symptom**: Czech UI (also German/Hungarian/Slovak/Spanish — the caption is non-ASCII in all five), Plugins → Plugin Manager, select **UnDelete** (`undelete=on` in `plugins.cfg`). The checkbox should read `Zobrazit položku Obnovení souborů a adresářů v nabídce Změnit jednotku…`; because `ShowInChDrvText` is …

- **Data path**: plugin `SetChangeDriveMenuItem(LoadStr(IDS_UNDELETEINCHDRVMENU))` (`src/plugins/undelete/undelete.cpp:328`) → normalized to UTF-8 at intake (feature-052 contract) → `CPluginData::ChDrvMenuFSItemName` → `src/dialogs5.cpp:353` composed with the ANSI `:495` template → `:354 SalSetWindowTextU8` → …


#### F-P2-11 — Plugin "Keyboard Shortcuts" dialog lists command names through the ANSI list-view call

- **Verdict**: CONFIRMED (not "any UI language" — six of the eight)

- **Symptom**: Plugins → Plugin Manager → *Keyboard Shortcuts* for a plugin whose menu commands are localized with non-ASCII characters (e.g. the Czech `.slg` of a shipped plugin, or any plugin whose command names carry accents) — every command name in the list renders as mojibake, in any UI language.

- **Data path**: S-B3-P2-012


#### F-P2-13 — "Save Configuration" export-exists box mixes an ANSI template with a UTF-8 configuration path

- **Verdict**: CONFIRMED — the L13 suppression premise is stale

- **Symptom**: any of Czech/German/French/Dutch/Hungarian/Slovak/Spanish UI (7 of the 8 shipped languages have a non-ASCII `IDS_SAVECFG_EXPFILEEXISTS`), Tandem Commander started with `-C D:\Zálohy\config.reg` (or with the config file taken from a roaming APPDATA path under a non-ASCII user name), then *Save …

- **Data path**: `src/salamdr1.cpp:3578-3582` (command line taken wide → UTF-8) → `:3665 lstrcpyn(ConfigurationName, argv[i + 1], MAX_PATH)` → `src/mainwnd3.cpp:2844` `LoadStr` composition → `src/msgbox.cpp:475` ANSI fallback.


#### F-P3-07 — status-bar tooltip truncates a UTF-8 path mid-sequence, costing the whole hint (DC-12)

- **Verdict**: CONFIRMED (mechanism), but the severity and the site group are overstated

- **Symptom**: **directory line / status bar, any UI language.** When the path shown in the panel's directory line is longer than `TOOLTIP_TEXT_MAX` bytes, `stswnd.cpp:1854` copies it with a plain byte clamp. If the cut lands inside a multi-byte character, `CToolTip::GetText`'s strict probe (`tooltip.cpp:309`) …

- **Data path**: `stswnd.cpp:1854` `lstrcpyn(text, str, TOOLTIP_TEXT_MAX)` (also `:1878`, `:1884`, and `drivelst.cpp:2697`) → `tooltip.cpp:305` `SendMessage(HNotifyWindow, WM_USER_TTGETTEXT, LastID, (LPARAM)Text)` → `tooltip.cpp:309` `SalU8ToW(Text, TextLen, TextW, TOOLTIP_TEXT_MAX)` fails → `:312` CP_ACP fallback.


### F. Configuration values with no defined encoding at rest

#### F-P4-03 — custom packer/unpacker titles have no defined encoding at rest (the class L17 names)

- **Verdict**: CONFIRMED as an invariant break · one live consequence, in **Hungarian** only

- **Data path**: `src/packers.cpp:240` `SetPacker(index, 1, LoadStr(CustomPackers[idx].Title[0]), ...)` -> `src/packers.cpp:734` `data->Title = DupStr(title);` -> `src/packers.cpp:841` `SetValue(hKey, SALAMANDER_CPU_TITLE, REG_SZ, GetPackerTitle(index), -1)` -> `src/salamdr6.cpp:2432` (ANSI tolerance branch) -> …


#### F-P4-07 — configuration fields documented as UTF-8 are seeded from ANSI `LoadStr`

- **Verdict**: CONFIRMED for the view-mode names · REFUTED on "the damage is permanent" · LATENT for the user-menu item

- **Symptom**: **Czech / Hungarian / German / Slovak UI on a Windows whose ACP cannot represent the language** (a shipped configuration — the language is chosen inside Tandem Commander, independently of the Windows system locale). `LoadStr` = `LoadStringA` substitutes `?` for every character outside the ACP. …

- **Data path**: - `src/salamdr4.cpp:1009` `strcpy(name, LoadStr(resID));` (inside `CViewTemplates::Load`, overwriting the stored name for the seven built-in templates) → `src/salamdr4.cpp:818` `lstrcpyn(Items[index].Name, name, VIEW_NAME_MAX)` → `src/dialogs4.cpp:1022` `SalListViewSetItemTextU8(HListView, i, 0, …


### G. Shell / OLE / drag-drop / misc ANSI hand-offs

#### F-P1-21 — Assorted ANSI file APIs on UTF-8 paths outside the main copy/move engine

- **Verdict**: CONFIRMED (all nine site groups verified as ANSI on UTF-8 values)

- **Data path**: each site takes its argument from `CFileData::Name`/`GetPath()`/`SalGetTempFileName` — see the Location column of the rows.


#### F-P1-22 — User-menu icons are loaded with the ANSI `ExtractIconEx`

- **Verdict**: CONFIRMED

- **Symptom**: any shipped language. A user-menu item whose icon is taken from an executable under a non-ASCII path (`D:\Programy\Můj nástroj\tool.exe`) shows the default icon in the User Menu and on the toolbar. `src/dialogs3.cpp:2318` — the icon **picker** in the very same feature — already tries …

- **Data path**: `CUserMenuItem::FileName` (configuration value, UTF-8) → `src/salamdr3.cpp:2308` / `:2691` `ExtractIconEx(fileName, iconIndex, NULL, &UMIcon, 1)`.


#### F-P1-23 — Environment-variable expansion mixes ACP values into UTF-8 paths

- **Verdict**: CONFIRMED

- **Symptom**: any shipped language, non-ASCII account name.

- **Data path**: `src/fileswn9.cpp:666` `ExpandEnvironmentStrings(buff, expandedBuff, buffSize + 1)` → `:673 lstrcpyn(buff, expandedBuff, buffSize)` → the panel path; `src/icncache.cpp:796`; `src/mainwnd4.cpp:1061 SetEnvironmentVariable(name, dir)`.


#### F-P1-24 — Common file/folder dialogs return CP_ACP paths into UTF-8 fields

- **Verdict**: CONFIRMED IN PART / REFUTED IN PART

- **Symptom**: any shipped language. *Options ▸ …* ▸ Browse (external viewer/editor, archiver executable, hot path, "Copy to" target directory): pick `D:\Programy\Můj editor\edit.exe` in the Browse dialog. `GetOpenFileNameA`/`SHGetPathFromIDListA` return CP1250 bytes, which are stored into the configuration as …

- **Data path**: `src/execute.cpp:2152 GetOpenFileName(&ofn)` → `:2154 SalGetFullName(file)` → `WM_SETTEXT` into the configuration edit line; `src/shellib.cpp:2547 SHGetPathFromIDList(res, path)` → `GetTargetDirectoryAux`'s `path` out-parameter → panel/target path.


#### F-P1-25 — UTF-8 names widened through CP_ACP before being handed to the shell/OLE

- **Verdict**: CONFIRMED (and one consequence the finding missed is worse)

- **Symptom**: any shipped language.

- **Data path**: e.g. `src/shellsup.cpp:536` `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fullName, -1, oleName, MAX_PATH)` → `:538 fileInt->Load(oleName, STGM_READ)`. Contrast `src/shellib.cpp:1600-1630`, which does `SalU8ToWAlloc` first and only then falls back — that is the shape all of these should have.


#### F-P1-26 — Dropped files are read through CP_ACP even when the payload is wide

- **Verdict**: CONFIRMED

- **Symptom**: any shipped language. Drag `Účtenka.pdf` from Explorer onto the command line, the status bar, the toolbar, or the internal viewer window. The `DROPFILES` payload is wide (`data->fWide`), and the code converts it with `WideCharToMultiByte(CP_ACP,…)` (or asks for it with the A `DragQueryFile`), so …

- **Data path**: `src/toolbar5.cpp:168`, `src/stswnd.cpp:1465`, `src/editwnd.cpp:1251` (`WideCharToMultiByte(CP_ACP, 0, fileW, l + 1, path, …)`); `src/viewer3.cpp:599 DragQueryFile((HDROP)wParam, 0, path, MAX_PATH)` → `:601 SalGetFullName(path)` → `:607 OpenFile(path, NULL, FALSE)`.


### H. Documentation contract

#### F-P5-06 — the FS plugin interface states no encoding for any path method

- **Verdict**: CONFIRMED as a documentation gap — but it is a **Note, not a Finding**

- **Data path**: S-B8-P5-010


---
## 2. Deferred — systemic (17 findings, 5 clusters). Each is its own feature.

Not deferred out of caution: no *minimal* fix exists, each touches machinery
that many other fixes also touch, and each needs its own regression matrix.

| # | Cluster | Findings | Why it is feature-sized |
|---|---|---|---|
| B-1 | **ANSI dialog windows** — only 2 of 90 dialog constructions set `unicodeWnd`, so USER32 thunks wide text through the ACP. Change Directory, Find "Look in", Pack/Unpack and the user-menu editor **persist** the damaged value | F-P3-05, F-P6-03, F-P6-06, F-P2-12, F-P3-04 | Per-dialog conversion across ~88 dialogs **plus** property-sheet pages, which are ANSI unconditionally. Two verified hazards: `CKeyForwarderWindow` (`msgbox.cpp:16-21`) would ANSI-ify the buttons of a newly-Unicode message box; and a wide `SetWindowLongPtr` alone is **not enough** — the paint handlers read back with ANSI `GetWindowText`/`DrawText`, so storage *and* paint must be converted. |
| B-2 | **ACP byte tables behind all name comparison** — `LowerCase[]`/`IsAlpha[]`, `StrICmp` and `IsTheSamePath` fold UTF-8 bytes through a 256-entry ACP table: `Č.txt` != `č.txt`, plus false equalities between unrelated accented letters | F-P3-06, F-P5-02 | Changing the case-folding used by *all* name comparison moves sorting, focus-by-name, path identity and auto-refresh at once. The verifier found it reaches ~20 further sites via `IsTheSamePath` that the finding did not list. |
| B-3 | **`GetErrorText` returns UTF-8 but the SDK never says so** — plugins compose it with their own ANSI `LoadStr`, so every plugin system-error message is mojibake in a non-English UI and correct in English | F-P5-12 + the core half of F-P2-01/02/03/06 | Count corrected on verification to **~27 defective sites in 5 plugins** (not 127/19). Critically, a naive sweep would **regress FTP**, whose own `FTPGetErrorText` is internally consistent ANSI. Needs an SDK contract statement plus per-plugin work. |
| B-4 | **`AlterFileName` byte-folds UTF-8 names** for the panel name-format option | F-P5-13 | Highest-risk fix in the whole review: `AlterFileName` also drives **Change Case, which renames files on disk**. Also needs a non-default `FileNameFormat` to trigger. |
| B-5 | **Plugin-facing services are ANSI and frozen** — browse dialogs, `NumberToStr` re-widening, the regedt/undelete cases | F-P5-03, F-P5-07, F-P5-01, F-P5-10, F-P5-11 | Changing them alters the bytes plugins receive, which FR-009 freezes. Needs an interface-version decision. |

## 3. Deferred items D01–D05

| ID | Where | What | Fix size |
|---|---|---|---|
| D01 | `src/common/handles.h:541,546,618` + `handles.cpp` | **The Trace Server cannot be built** — `handles.h` declares both generic (`LPCTSTR`) and explicit `…W` wrappers, which collide under `UNICODE` (C2535/C2084). Core is unaffected; `build.cmd` never builds `tserver`. Consequence: the G5 leak/handle gate runs at the observable bar only. | small, but touches the core debug shim |
| D02 | `src/plugins/zip/common.cpp:2469` `GetInfo` → `IDC_FILEATTR` | Mixes `NumberToStr`'s UTF-8 thousands separator with ANSI `GetDateFormat`/`GetTimeFormat`, so on cs-CZ **every file >= 1000 bytes** shows a stray `Â` in both ZIP overwrite dialogs. The feature-067 defect shape, inside a plugin. | **not plugin-local**: `GetInfo`'s output crosses back into the core (`zip/add.cpp:1236` → `CSalamanderGeneral::DialogOverwrite` → `src/zip.cpp:664,679`), which FR-009 freezes → belongs with B-5 |
| D03 | `src/plugins/filecomp/controls.cpp:88-91` | Path bar drops its text entirely on strict-conversion failure — the same "drop the text" shape as F-P5-09. | ~4 lines; same idiom as X09 |
| D04 | `src/plugins/filecomp/worker2.cpp:85-100` | A **fourth** filecomp title site, invisible to a `: L""` search because it never converts. **Ordering matters**: on the `WN_BINARY_FILES_DIFFER` path it overwrites the caption X09 sets, so a *binary* comparison still looks unfixed. | small; do it with D03 |
| D05 | `.specify/extensions/git/scripts/powershell/auto-commit.ps1:149` | **The Spec Kit git auto-commit hook cannot parse on a CP1250/CP1252 machine.** The file is UTF-8 **without a BOM**, so PowerShell 5.1 reads it as ANSI; the check-mark (`E2 9C 93`) becomes three CP1250 chars ending in `0x93` = U+201C, which **PowerShell accepts as a string delimiter** → unterminated string → whole-file `ParserError`. Same defect class as the product findings, in the tooling. | **one line**: replace the check-mark with ASCII, or save as UTF-8 **with** BOM |

## 4. Still owed: the on-screen sweep (maintainer)

Everything automatable is green; this part needs a human. Full matrix in
`review-report.md` section 10.

- **Binaries**: fixed = `build\tandemcommander\Debug_x64\` (and `Release_x64\`);
  **pre-fix reference for side-by-side** = `build\tandemcommander\Release_x64_prefix\`
- **Fixtures** (already created): `D:\Zkouška\Můj disk\`,
  `D:\Zkouška\Árvíztűrő tükörfúrógép\`, `D:\Zkouška\surrogate\`,
  `%TEMP%\salamander-test\perf` (100 000 files)
- **UI language**: Options → Configuration → Language (restart required)

**Start here** (verifies the three highest-consequence fixes):

1. **F1** — in `D:\Zkouška\Můj disk\` focus `žluťoučký kůň.docx`, press
   **Ctrl+Enter**, then **Ctrl+Space** / **Ctrl+[** / **Ctrl+]**. Expected: the
   text appears and the program does **not** disappear. (The pre-fix binary
   terminates on a ~130-accented-character name.)
2. **F2** — with an accented `%TEMP%`: F3-view a file inside a ZIP, exit, check
   `%TEMP%` for leftover `SAL*.tmp`; restart and confirm the cleanup prompt
   appears and works.
3. **F3** — hot path `Můj disk` → `D:\Zkouška\Můj disk`; right-click the taskbar
   icon: the entry is readable **and** it opens.

Then the W1–W20 regression sweep in **cs** and **hu**, plus an English
spot-check of W1–W6/W13 against `Release_x64_prefix`.

> **Caveat for F8 (filecomp title)**: verify on a **text** comparison. A binary
> comparison still shows the unfixed caption — that is deferred item **D04**,
> not a failed fix.

## 5. How to do the remaining fixes (the protocol that caught 3 regressions)

Do **not** shortcut this. Three of the nine fixes were rejected by review
before reaching the tree, and all three were regressions the fixes themselves
introduced — twice **DC-09** (a strict facade meeting a legacy producer, the
very class this review catalogued), once a fix that would have blanked the file
name on a *destructive* overwrite prompt. **None** would have been caught by
the build, the 1,257 tests, or the static guard.

1. **Re-read the verdict** in `findings/verdicts-V*.md`, not just the finding —
   the verifiers narrowed or corrected most claims (languages, reachability,
   which half is real).
2. **Fix minimally**, using the house pattern: convert with
   `SalU8ToW`/`SalU8ToWAlloc` and call the W API, or use the facade
   (`SalCreateFile`, `SalFindFirstFile`, `SalDeleteFile`, `SalRemoveDirectory`,
   `SalGetFileAttributes`, `SalRegQueryValueExW8`/`SalRegSetValueExW8`,
   `CopyTextToClipboardU8`, `LoadStrU8`, the `Sal*U8` sinks, `CSalPathBuf`).
3. **Walk the WHOLE chain, not the changed line.** Both X07 rejections were
   caused by converting one link while the adjacent producer or consumer still
   spoke the legacy code page. Trace producer → every intermediate → sink.
4. **Never drop text on conversion failure** — always fall back to the legacy
   narrow call (contracts 005/010 C2). Blanking is a regression, not a fix.
5. **Independent regression review** by an agent that did not write the fix,
   using the charter in `charters.md` (section "Regression reviewer"). It must
   enumerate consumers itself, give a per-surface verdict, and confirm
   ASCII/English byte-identity and plugin-facing byte-identity.
6. **Gates**: `build.cmd full` · `build.cmd full release` ·
   `…\Debug_x64\saltests\saltests.exe` (>= 1257, 0 failed) ·
   `python tools\check_encoding.py --strict` (must be `TOTAL: 0`).

## 6. Facts a fresh session needs

- **The core is built without `UNICODE`** → every un-suffixed Win32 text API
  (`SetWindowText(`, `CreateFile(`, `RegOpenKeyEx(`) is the **ANSI** entry
  point. `HANDLES(CreateFile(...))` is still an ANSI call.
- **The product compiles with `/J`** (`sal_base.props:14`) → plain `char` is
  **unsigned**. Any claim resting on a signed `char` is void (this refuted
  ledger row L07 and voided the `signed-char-name-byte` guard rule).
- Internal names/paths are **WTF-8** (UTF-8 + lone surrogates as
  `ED A0 80..ED BF BF`) since feature 066. `SalWToU8` is total; `SalU8ToW` is a
  strict WTF-8 decoder; `SalU8ToWDisplay*` is lenient and **display-only**.
- The plugin-shared `Spl*` helpers are **strict UTF-8 by design** and reject
  WTF-8 (feature-066 contract, boundary notes) — by design, not a bug.
- `SalFindFirstFile` **registers its own handle** with the HANDLES tracker —
  do not wrap it in `HANDLES_Q`; close with `HANDLES(FindClose(h))`.
- `src/consts.h` comments are **stale** for the facade functions; the real
  definitions live in `src/common/salfileio.cpp`.
- Guard: `python tools\check_encoding.py --strict` (9 rules, must be 0) and
  `--draft` (4 report-only rules, each blocked on a deferred fix).
- Baseline commit for this feature: **`c577ff3`**.

## 7. Bookkeeping

- **`tasks.md` T036 stays open by design** — it fires only if the sweep finds a
  failure, and then routes back through verify → fix → review → gates.
- `consolidate.py` regenerates the report's findings/ledger/contract tables
  from `findings/*.md`; `coverage_check.py` re-runs the queue accounting.
- Nothing here is release-blocking on its own, but the **34 items in section 1
  are confirmed user-visible defects** and should not be quietly lost.

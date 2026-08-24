# Verdicts — batch V1 (independent verifier)

Scope: F-P6-01 … F-P6-06 (`findings/P6.md`) and F-P3-04 (`findings/P3.md`).
Charter: refute. A finding survives only where the data path reproduces in the
code and the scenario is reachable in a shipped configuration (the 8 languages
with `enabled = on` in `translations/languages.cfg`: czech, german, french,
dutch, hungarian, romanian, slovak, spanish).

Established build facts re-verified before use:

- The core is built without `UNICODE`: `CDialog::Execute`/`Create`
  (`src/common/winlib.cpp:614-640`) call `DialogBoxParam` / `CreateDialogParam`
  (= the `A` entry points) unless `UnicodeWnd` is set, and `CWindow::CreateEx`
  (`src/common/winlib.cpp:142`) calls `CreateWindowEx` (= `A`). Dialogs created
  through the `A` dialog manager get **ANSI child controls**, so wide text set
  on them is projected through `CP_ACP` inside USER32.
- Exactly four windows opt in to `unicodeWnd = TRUE`: `src/dialogs3.cpp:394`
  (`CCopyMoveDialog`), `src/dialogs3.cpp:588` (`CCopyMoveMoreDialog`),
  `src/filesbx1.cpp:25` (`CFilesBox`), `src/fileswn5.cpp:2914`
  (`CQuickRenameWindow`). Verified by grep over all of `src`.

---

## F-P6-01 · CONFIRMED

**Claim** — mdview's keeper window class is never unregistered, so after a
Plugins Manager *Unload* the "instant view" keeper never arms again for the
rest of the session.

**Scenario** — Surface: Markdown Viewer (mdview); any UI language; no encoding
involvement. (1) F3 a `.md` file: `ViewFile` reaches
`src/plugins/mdview/viewer.cpp:312-313` `if (g_keepReady) MdKeeperArm();` —
`g_keepReady` is `TRUE` by default (`src/plugins/mdview/mdview.cpp:33`), so
`MdKeeperArm` registers class `TandemMdKeeperWnd` with
`wc.hInstance = DLLInstance` and sets `g_keeperClassRegistered = true`.
(2) Plugins → Plugins Manager → Markdown Viewer → **Unload**
(`src/dialogs5.cpp:896-903` → `CPluginData::Unload`, `src/plugins1.cpp:3031`)
→ `PluginIface.Release(...)` → `src/plugins/mdview/mdview.cpp:134`
`MdKeeperDisarm();` + `ReleaseViewer();` → `src/plugins1.cpp:3133`
`HANDLES(FreeLibrary(DLL))`. `MdKeeperDisarm` →`MdKeeperReleaseAll`
(`webview.cpp:864-870`, `699-730`) destroys the keeper *window* but never
unregisters the *class*. (3) F3 a `.md` file again: the DLL statics are fresh,
so `g_keeperClassRegistered == false`, `RegisterClassW` fails
(`ERROR_CLASS_ALREADY_EXISTS`, the stale class is still owned by the same
`hInstance` because a just-unmapped DLL re-maps at its previous base), and
`MdKeeperArm` returns silently. What the user sees: every markdown view for the
rest of the session pays the cold WebView2 browser start again — the exact
regression feature 065 exists to remove — with no message and the
Configuration checkbox still showing the option as on. `g_keeper.state` stays
`kUnarmed`, so every later view retries and fails identically.

**Evidence**

- `src/plugins/mdview/webview.cpp:798-807` — `if (!g_keeperClassRegistered) {
  … wc.hInstance = DLLInstance; … if (RegisterClassW(&wc) == 0) return;
  // silent (FR-005); the next view tries again` — the comment's premise is
  false once a stale class of the same name/hInstance exists.
- No `UnregisterClass` anywhere under `src/plugins/mdview/` (grep over
  `*.cpp`/`*.h`) — re-verified independently.
- **Decisive in-repo evidence for the OS premise**: the shared plugin WinLib
  does exactly this, with the reason spelled out —
  `src/plugins/shared/winliblt.cpp:131-137`:
  `// provedeme odregistrovani trid, aby pri dalsim loadu pluginu mohly byt
  znovu zaregistrovany` ("we unregister the classes so that they can be
  registered again on the plugin's next load") followed by
  `UnregisterClass(CWINDOW_CLASSNAME2, dllInstance)` (`:134`) /
  `UnregisterClass(CWINDOW_CLASSNAME, dllInstance)` (`:136`). The project's own
  shared layer therefore states that `FreeLibrary` does **not** unregister a
  DLL's classes and that failing to unregister breaks re-registration on the
  next load. `ReleaseWinLib(DLLInstance)` is reached from
  `src/plugins/mdview/viewer.cpp:120` — the keeper class is the one class in
  mdview that departs from that pattern.

**Notes**

- One caveat the finding does not state: the failure requires the reloaded DLL
  to receive the *same* `HINSTANCE`. Class ownership is keyed by (atom,
  hInstance), so if Windows happened to map the reloaded `.spl` at a different
  base the registration would succeed. In practice the previous base is free
  again at reload time and is reused, which is precisely the case
  `winliblt.cpp` was written for. I could not execute the product to
  demonstrate it (read-only charter), so this is confirmed by code + the
  project's own documented premise, not by a run.
- Severity beyond the finding: the stale class keeps a `lpfnWndProc` pointing
  into unmapped memory. `MdKeeperArm`'s early `return` is what prevents a crash
  — accepting `ERROR_CLASS_ALREADY_EXISTS` as success (one of the finding's two
  suggested fixes) would create a window on a dangling window procedure and
  crash. Only the `UnregisterClassW` in `MdKeeperReleaseAll` variant is safe.

---

## F-P6-02 · CONFIRMED (with a corrected language list — French is REFUTED)

**Claim** — feature 052 made `ChDrvMenuFSItemName` UTF-8 but left its checkbox
template read ANSI, so the Plugins Manager renders the plugin's Change-Drive
item name as mojibake in cs/sk/fr/hu.

**Verified data path** (every step opened)

1. Plugin side, ANSI: `src/plugins/undelete/undelete.cpp:328`
   `salamander->SetChangeDriveMenuItem(String<char>::LoadStr(IDS_UNDELETEINCHDRVMENU), 0);`
2. Intake, **now UTF-8**: `src/plugins1.cpp:1242-1243`
   `// feature 052: normalize to UTF-8 … p->ChDrvMenuFSItemName = SalLegacyToU8Alloc(title, MAX_PATH - 1);`
3. Template read, **still ANSI**: `src/dialogs5.cpp:490-495` — the two reads
   stand one blank line apart:
   `SalGetDlgItemTextU8(HWindow, IDC_PLUGINSHOWINBAR, ShowInBarText, 200);`
   then `GetDlgItemText(HWindow, IDC_PLUGINSHOWINCHDRV, ShowInChDrvText, 200);`
   `CPluginsDlg` is an ANSI dialog (`src/dialogs5.cpp:27`
   `: CCommonDialog(HLanguage, IDD_PLUGINS, IDD_PLUGINS, hParent)` — no
   `unicodeWnd`), so its control text is the `CP_ACP` projection of the `.slg`
   template and `GetDlgItemTextA` returns ACP bytes.
4. Mixed composition: `src/dialogs5.cpp:353` `sprintf(buff, ShowInChDrvText, itemText);`
   (ACP template + UTF-8 name; `itemText` is the post-tab part of
   `ChDrvMenuFSItemName`, `:331-350`). Contrast `:327`
   `sprintf(buff, ShowInBarText, pluginName);` — both halves UTF-8, correct.
5. Sink: `src/dialogs5.cpp:354` `SalSetWindowTextU8(showInChDrv, buff);` →
   `src/common/winlib.cpp:1103-1113`: `SalU8ToWAlloc` returns NULL for the
   mixed string, so the legacy branch `SetWindowText(hWnd, u8Text)` draws the
   whole buffer through the ACP — template correct, UTF-8 name mojibake.

**Reachability — the finding's language list is wrong.** The defect needs
**both** halves non-ASCII: the template (else the composed string is valid
UTF-8 and takes the correct wide path) **and** the plugin's item name (else
nothing visibly breaks). Checked against the shipped `.slt` sources:

| language | template 2621 (`salamand.slt:1318`) | undelete name (`undelete.slt:174`, id 102) | portables name (`portables.slt:17`, id 8192) | verdict |
|---|---|---|---|---|
| czech | `Zo&brazit položku %s v nabídce Změnit jednotku…` non-ASCII | `\tObnovení souborů a adresářů` non-ASCII | `Přenosná zařízení` non-ASCII | **mojibake** |
| slovak | `Zo&braziť položku %s v ponuke Zmeniť jednotku…` non-ASCII | `\tObnovenie súborov a adresárov` non-ASCII | `Prenosné zariadenia` non-ASCII | **mojibake** |
| hungarian | `%s elem a Meghajtó váltása menüben és s&orban` non-ASCII | `\tVisszaállítás` non-ASCII | `Hordozható eszközök` non-ASCII | **mojibake** |
| german | `%s im Laufwerkmenü und in der Laufwerkleiste an&zeigen` non-ASCII | `\tWiederherstellen` ASCII | `Tragbare Geräte` non-ASCII | **mojibake** (via portables) |
| spanish | `V&er elemento %s en menú Cambiar unidad…` non-ASCII | `\tDesborrado` ASCII | `Dispositivos portátiles` non-ASCII | **mojibake** (via portables) |
| **french** | `Affic&her %s dans le menu Changer de lecteur et dans la barre des lecteurs` — **pure ASCII** | `\tRécupération` non-ASCII | `Appareils portables` ASCII | **no defect** — buff is valid UTF-8, `SetWindowTextW` renders `Récupération` correctly |
| dutch | ASCII | `\tHerstellen` ASCII | `Draagbare apparaten` ASCII | no defect |
| romanian | ASCII | `\tUndelete` ASCII | `Dispozitive portabile` ASCII | no defect |

Other enabled FS plugins that register a Change-Drive item (`folders`,
`regedt`, `ftp`, `sftp` — all `on` in `plugins.cfg`) have ASCII item names in
every shipped language (`"'\tDesktop"`, `"\tWindows registr"`,
`"/\tFTP klient"`, `"/\tSFTP Client"`), so they never show the defect.

**Exact failure scenario** — Configuration → Plugins (Plugins Manager), Czech
(or Slovak / Hungarian) UI on the matching ACP, select **Undelete** in the
list: the checkbox "Zobrazit položku *…* v nabídce Změnit jednotku a v panelu
jednotek" renders the item name as mojibake while the sibling checkbox one line
above ("Zobrazit ikonu *Undelete* v panelu pluginů") is correct. Same in German
and Spanish (and in cs/sk/hu) when **Portable Devices** is selected. Note: the
illustration in the finding (`ObnovenÃ­ souborÅ¯…`) is the CP1252 rendering; on
a Czech CP1250 machine the mojibake reads `ObnovenĹ™…`-style — the substance is
unchanged.

**History claim — CONFIRMED.** `git log -L 1243,1244:src/plugins1.cpp`:
commit `6173c8a [052] fix plugin name mojibake in Plugins Manager…` replaced
`p->ChDrvMenuFSItemName = DupStr(title);` with `SalLegacyToU8Alloc(title, …)`.
`git log -L 490,496:src/dialogs5.cpp` shows the **same commit** converting only
the `IDC_PLUGINSHOWINBAR` read to `SalGetDlgItemTextU8` and leaving the
`IDC_PLUGINSHOWINCHDRV` read as `GetDlgItemText`. The sink
`SalSetWindowTextU8` had been in place since `f950681 [010]`. So before 052 the
composition was ANSI+ANSI → invalid UTF-8 → ANSI fallback → correct; 052
introduced the asymmetry. The finding's regression attribution is exact.

**Notes**

- Ironically, 052 *fixed* French at this site (its ASCII template plus the
  now-UTF-8 name takes the wide path, where before it took the ANSI fallback).
- The suggested one-line fix (`SalGetDlgItemTextU8` at `src/dialogs5.cpp:495`)
  is correct and matches line 492.

---

## F-P6-03 · CONFIRMED (Change Directory + Find "Look in"); the third cited site is dead code

**Claim** — a path the panel already holds is destroyed before the user can act
on it: Change Directory and Find "Look in" prefill an ANSI dialog through the
ACP.

**Scenario** — Surface: Change Directory (Shift+F7) and Find (Alt+F7). Any UI
language among the eight shipped; the trigger is the machine's ANSI code page,
not the UI language: stand a disk panel in a folder whose name contains
characters the ACP cannot represent (Greek `D:\Δοκιμή`, Cyrillic `D:\проект`,
CJK or an emoji folder on a Czech/Western ACP) and press Shift+F7 (or Alt+F7).
The path combo is prefilled `D:\??????` — the user typed nothing and the value
is already wrong. Pressing OK reads the same `?`s back, and the change-directory
or "Find Now" then operates on a path that does not exist.

**Evidence chain — Change Directory**

- Source is UTF-8: `src/fileswn3.cpp:1979` `GetGeneralPath(path, SAL_MAX_PATH_UTF8, TRUE);`
  → `src/fileswn1.cpp:150-157` copies `GetPath()` verbatim for a `ptDisk`
  panel; `GetPath()` returns `char Path[SAL_MAX_PATH_UTF8]`
  (`src/fileswnd.h:479,555`), UTF-8 by the feature-004 contract.
- Dialog is ANSI: `src/dialogs3.cpp:1180`
  `CChangeDirDlg::CChangeDirDlg(…) : CCommonDialog(HLanguage, IDD_CHANGEDIR, IDD_CHANGEDIR, parent)`
  — no `unicodeWnd` argument, so `UnicodeWnd == FALSE` (default in
  `src/salamand.h:768,782`) and `CDialog::Execute` takes `DialogBoxParam`
  (= the `A` entry, `src/common/winlib.cpp:625`).
- Wide setter on that ANSI combo: `src/dialogs3.cpp:1197`
  `SalSetWindowTextU8(hWnd, Path); // Path carries UTF-8 (feature 005)` →
  `src/common/winlib.cpp:1103-1108` `SetWindowTextW(hWnd, w)` → USER32 projects
  W→A through `CP_ACP` (default character `?`) for the ANSI control.
- Read-back re-encodes the damage: `src/dialogs3.cpp:1201`
  `SalGetWindowTextU8(hWnd, Path, 2 * MAX_PATH);` →
  `src/common/winlib.cpp:1121-1127` `GetWindowTextW` + `SalWToU8`, which
  faithfully encodes the `?`s as UTF-8.

**Evidence chain — Find "Look in"**

- `src/fileswn5.cpp:699` `OpenFindDialog(MainWindow->HWindow, Is(ptDisk) ? GetPath() : "");`
  → `src/find.cpp:2150` `new CFindDialog(hCenterAgainst, initPath)` →
  `src/finddlg1.cpp:1438-1454` copies `initPath` into `Data.LookInText`.
- Dialog is ANSI: `src/finddlg1.cpp:1365-1366`
  `CFindDialog::CFindDialog(HWND hCenterAgainst, const char* initPath)
  : CCommonDialog(HLanguage, IDD_FIND, NULL, ooStandard, hCenterAgainst)` — no
  `unicodeWnd`.
- `src/finddlg1.cpp:1813` `HistoryComboBox(HWindow, ti, IDC_FIND_LOOKIN, Data.LookInText, LOOKIN_TEXT_LEN, …)`
  → `src/viewer.cpp:62` `SalSetWindowTextU8(hwnd, Text); // mask/path/search text is UTF-8 (feature 005)`
  — the same wide-setter-on-an-ANSI-window loss.
- Read-back: `src/finddlg1.cpp:1792` `SalGetWindowTextU8(hLookInWnd, Data.LookInText, LOOKIN_TEXT_LEN);`
  and `:1844` `SalGetDlgItemTextU8(HWindow, IDC_FIND_LOOKIN, …)`.

**Corroboration that the mechanism is real and known to the project** —
`src/dialogs3.cpp:391-394`: `// feature 015: Unicode dialog so the path/name
combo can show characters outside the ANSI code page (emoji, Cyrillic, ...)
instead of '?'` … `ooStandard, NULL, TRUE /*unicodeWnd*/`. Feature 015 fixed
exactly this loss for `CCopyMoveDialog`; `CChangeDirDlg` and `CFindDialog` were
not converted.

**Partly REFUTED** — the third site the finding offers as supporting evidence,
`src/dialogs3.cpp:1811-1821` (`CEnterPasswdDialog`,
`SetWindowTextW(GetDlgItem(HWindow, IDS_NETPATH), pathW)`), is **dead code in a
shipped configuration**. `CEnterPasswdDialog` is never executed: a grep for
`Execute(` over `src/drivelst.cpp` returns nothing, and `IDD_ENTERPASSWD`
appears only at `src/dialogs3.cpp:1773` and in `lang.rc`. The object is
constructed at `src/drivelst.cpp:506` solely as a buffer holder for
`credUIPromptForCredentialsA` (`:645-647`, `:722-724`), which is gated on
`!Windows7AndLater` (`src/salamdr1.cpp:3905`
`Windows7AndLater = SalIsWindowsVersionOrGreater(6, 1, 0)`) — unreachable on
the product's Windows 11+ baseline. On Windows 7+ the code always sets
`connectInteractive = TRUE` and lets
`WNetAddConnection3(…, CONNECT_INTERACTIVE)` (`src/drivelst.cpp:617`) show the
*system's* Unicode credential UI. The dialog's `IDS_NETPATH` static therefore
never renders. The two live sites stand.

**Notes**

- Also affected by the same mechanism, not listed in the finding: `CFindDialog`'s
  "Named" combo (`src/finddlg1.cpp:1811`) and every other `SalSetWindowTextU8` /
  `HistoryComboBox` prefill on an ANSI dialog.
- The suggested fix (add `TRUE /*unicodeWnd*/`) is the right shape for the two
  live dialogs; it is pointless for `CEnterPasswdDialog`.

---

## F-P6-04 · CONFIRMED (Ctrl+Enter and Ctrl+Space / Ctrl+[ / Ctrl+]); the drag-drop clause is only partly right

**Claim** — `CEditLine::InsertText` pushes raw UTF-8 into an ANSI
`EM_REPLACESEL`, so the command line fills with mojibake and the command runs
against a name that does not exist.

**Scenario** — Surface: the command line under the panels. Any UI language; the
trigger is any non-ASCII file/folder name — `Přehled.txt` on a Czech CP1250
machine reproduces it. Focus the command line (typing a character with
"Quick search: enter alt" on forwards focus there, `src/fileswn0.cpp:980-987`;
or CM_EDITLINE), focus `Přehled.txt` in the panel, press **Ctrl+Enter**. The
command line fills with `PĹ™ehled.txt` (the CP1250 reading of the UTF-8 bytes
`50 C5 99 65 …`; `PÅ™ehled.txt` on a CP1252 machine) instead of `Přehled.txt`.
Pressing Enter executes that literal mojibake name — "file not found", or a
wildcard command silently operating on nothing. Ctrl+Space / Ctrl+[ / Ctrl+]
insert a panel path the same way.

**Evidence chain**

- The command line's edit control is an ANSI window: `src/editwnd.cpp:1694-1701`
  `CreateEx(0, "ComboBox", …)` → `src/common/winlib.cpp:142` `CreateWindowEx`
  (= `A`) → `src/editwnd.cpp:1714` `EditLine->AttachToWindow(GetWindow(HWindow, GW_CHILD));`
  with `CEditLine::CEditLine() : CWindow(ooStatic)` (`src/editwnd.cpp:346-347`,
  no `unicodeWnd`). The product states this itself at
  `src/fileswn0.cpp:983-987`: *"wParam is a UTF-16 code unit (the panel is a
  unicode window), PostMessageW lets the system convert it for **the ANSI edit
  line**"*.
- Ctrl+Enter: `src/editwnd.cpp:892` `s = file->Name;` (`CFileData::Name`, UTF-8
  by the interface-104 contract, `src/plugins/shared/spl_com.h:205-210`) →
  `:897-901` `l = (int)strlen(s); memmove(path, s, l); path[l++] = ' ';
  path[l] = 0; InsertText(path);`
- Ctrl+Space / Ctrl+[ / Ctrl+]: `src/editwnd.cpp:983-989` `s = …->GetPath()`
  (UTF-8) → `:998` / `:1003` `strcpy(path, s);` → `:1005-1006`
  `SalPathAddBackslash(path, MAX_PATH); InsertText(path);`
- Sink: `src/editwnd.cpp:353-356`
  `void CEditLine::InsertText(char* s) { SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM)s); }`
  — `SendMessage` is `SendMessageA` in this build and the target is an ANSI
  window, so no thunk runs: the raw UTF-8 bytes are stored and drawn as ACP
  bytes.
- The corruption is then baked in on the read side: `src/editwnd.cpp:390`
  `SalGetWindowTextU8(HWindow, cmdLine, SALCMDLINE_MAXLEN + 1); // command line
  is UTF-8 (feature 005)` → `GetWindowTextW` converts the stored ACP bytes into
  the mojibake characters and `SalWToU8` encodes *those* as UTF-8. The write
  side is the only half never converted (contrast the read side at `:390` and
  the history fill `SalComboAddStringU8` at `:1747`).

**Partly REFUTED — the drag-drop clause.** The finding says "dragging a
non-ASCII path onto the command line (`CInnerText::InsertText`) shows the same
corruption". The drop handler is actually `CEditDropTarget::InsertText`
(`src/editwnd.cpp:1146-1176`) and its text comes from `GetNameFromDataObject`
(`:1191-1281`). For an ordinary Explorer or disk-panel drag the `CF_HDROP`
branch converts the wide name with
`WideCharToMultiByte(CP_ACP, 0, fileW, l + 1, path, MAX_PATH, NULL, NULL)`
(`src/editwnd.cpp:1251`) — an ANSI value into an ANSI sink, i.e. **lossy (`?`)
but not mojibake**. Only the Salamander-internal `SALCF_FAKE_REALPATH` branch
(`:1194-1218`, served from `CFakeDragDropDataObject::RealPath`,
`src/salshlib.cpp:242-252`) can carry UTF-8, and that object exists only for
archive / plugin-FS drags (`src/shellsup.cpp:1014`). The drag-drop claim
therefore holds for archive/FS drags only, not for the general case as stated.

**Notes**

- The suggested fix (`SalU8ToWAlloc` + `SendMessageW(EM_REPLACESELW)` with an
  `A` fallback) must also cover `CEditDropTarget::InsertText`
  (`src/editwnd.cpp:1176`) — a second raw `EM_REPLACESEL` on the same control —
  and the `EM_SETSEL` / `EM_CHARFROMPOS` offsets around it
  (`src/editwnd.cpp:1114,1122,1175`), which are byte offsets today and would
  become UTF-16 unit offsets. The finding does not mention that.

---

## F-P6-05 · CONFIRMED (memory safety) — with corrected line numbers and computed bounds

**Claim** — two unbounded copies in the command line overflow 260-byte stack
buffers from a long or non-ASCII name/path.

**Buffer arithmetic (computed, not quoted)**

*Site (a) — Ctrl+Enter, insert focused name.*
`src/editwnd.cpp:878` `char path[MAX_PATH + 1];` → **261 bytes**.
Writes at `src/editwnd.cpp:897-900`:
`l = (int)strlen(s); memmove(path, s, l); path[l++] = ' '; path[l] = 0;` —
`l + 2` bytes touched, so the copy is safe only for `l <= 259`.
Source `s = file->Name` (`:892`) is `CFileData::Name`, heap-allocated per item
(`src/fileswn3.cpp:501` `file.Name = (char*)malloc(len + 1);`, filled from
`char nameU8[3 * MAX_PATH + 4]` at `src/fileswn3.cpp:300,399` via
`SalConvertFindDataW`), and the SDK documents the removed cap:
`src/plugins/shared/spl_com.h:224-226` — *"UTF-8 byte count since interface 104
- a single component may need up to 3*255 bytes, the former (MAX_PATH - 5) cap
is gone (ABI break vs. interface 103)"*.
A single NTFS component is at most 255 UTF-16 units → at most **765 UTF-8/WTF-8
bytes** (≤3 bytes per unit; a surrogate pair is 2 units → 4 bytes).
**Overrun starts at `strlen(Name) >= 260`; worst case writes 765 + 2 − 261 =
506 bytes past the buffer.**
Reachable inputs, all legal on NTFS and all ≤255 characters: 87 CJK characters
(261 B), 130 accented Latin-1/Latin-2 characters (260 B), or a 255-character
Czech name containing ≥5 accented characters (260 B).
The `shiftPressed` variant takes `file->DosName` (8.3) and is harmless.

*Site (b) — Ctrl+Space / Ctrl+[ / Ctrl+], insert panel path.*
`src/editwnd.cpp:978` `char path[MAX_PATH];` → **260 bytes**.
`src/editwnd.cpp:998` and `:1003` `strcpy(path, s);` — unbounded.
Source `s = …->GetPath()` (`:983,986,989`) is `char Path[SAL_MAX_PATH_UTF8]`
(`src/fileswnd.h:479,555`) with
`SAL_MAX_PATH_UTF8 = 3 * SAL_MAX_PATH_W + 1 = 98302` (`src/common/salpath.h:20-21`).
**Overrun starts at `strlen(GetPath()) >= 260`** — any long path (supported for
`ptDisk` panels since feature 004) or ~87 CJK / ~130 accented characters of
path. `SalPathAddBackslash(path, MAX_PATH)` at `:1005` is bounded
(`src/consts.h:300`) but runs after the overrun. The
`GetShortPathName(s, path, MAX_PATH)` branch at `:996` is bounded but falls
through to the same unbounded `strcpy` at `:998` whenever it fails — which it
does on volumes with 8.3 name creation disabled and for the ANSI call on a
non-ACP path.

**Consequence** — a classic stack buffer overrun (CWE-121) in
`CEditLine::WindowProc`. Neither `salamand.vcxproj` nor `src/vcxproj/*.props`
sets `BufferSecurityCheck`, so MSVC's default `/GS` applies: the frame-cookie
check turns most cases into `__report_gsfailure` → immediate process
termination without the normal shutdown, so panel state, history and the
current selection are lost. Locals and saved registers between `path` and the
cookie are corrupted first, so a window of silent wrong behaviour exists before
the cookie is reached.

**Line-number correction** — the finding cites 877 / 898-901 / 977 / 1000 /
1006; the actual lines in the current tree are **878** (`char path[MAX_PATH + 1]`),
**897-901**, **978** (`char path[MAX_PATH]`), **998** and **1003**
(`strcpy(path, s)`), **1006** (`InsertText(path)`). The substance is unchanged.

**Notes**

- The house fix pattern the finding points at is real and applicable:
  `src/fileswn5.cpp:2530` `lstrcpyn(newName, …, MAX_PATH); // was unbounded strcpy`
  (feature 027). Sizing site (a) `SAL_FIND_NAME_U8` and site (b)
  `SAL_MAX_PATH_UTF8` (or `CSalPathBuf`) plus `lstrcpyn` closes both.
- Both sites are also the F-P6-04 sites, so one rewrite of this block can fix
  the overrun and the encoding defect together.

---

## F-P6-06 · CONFIRMED for four of the five surfaces · the password surface is REFUTED

**Claim** — typed text outside the ANSI code page is silently replaced by `?`
on every input surface except the four Unicode windows — *including password
fields*.

**Mechanism — verified.** Every dialog, property-sheet page and subclassed
control in the product except the four listed at the top of this file is an
ANSI window:
`CDialog::Execute`/`Create` → `DialogBoxParam` / `CreateDialogParam`
(`src/common/winlib.cpp:625,638`); `CWindow::CreateEx` → `CreateWindowEx`
(`:142`); the Configuration pages go through `PROPSHEETHEADER` /
`PropertySheet` / `PROPSHEETPAGE` / `CreatePropertySheetPage`
(`src/common/sheets.cpp:266,277,475-499`) — all the `A` entry points. USER32
therefore delivers `WM_CHAR` to those controls as an ACP byte and synthesizes
`CF_TEXT` from `CF_UNICODETEXT` through `CP_ACP` on paste, so a character the
ACP cannot represent never enters the control at all. Any later wide read
(`GetWindowTextW`, `GetDlgItemTextW`, `SalGetWindowTextU8`,
`CTransferInfo::EditLine`) faithfully returns the `?` — the loss is upstream of
the program and no converter can recover it.
The product states this in its own words twice:
`src/salamand.h:765-767` — *"feature 015: 'unicodeWnd' TRUE makes the dialog a
Unicode window (DialogBoxParamW), so controls can hold characters outside the
system ANSI code page and the Sal*U8 (wide) helpers stop being lossy
(?-substitution)"* — and `src/dialogs3.cpp:391-393`.
`UnicodeWnd` really does default to `FALSE`: `src/common/winlib.h:141-142`
(`CWindow`), `src/salamand.h:768,782` (`CCommonDialog`). The finding's citation
`winlib.h:83-105,131` points at `CWindowsObject`, whose constructors take
`unicodeWnd` with **no** default — the defaults live in the two places above.
Minor correction only.

**Per-surface verdicts**

| surface | verdict | evidence |
|---|---|---|
| **Password** (`CEnterPasswdDialog`) | **REFUTED** | The dialog is never shown. `grep 'Execute('` over `src/drivelst.cpp` returns nothing; `IDD_ENTERPASSWD` occurs only at `src/dialogs3.cpp:1773` and in `lang.rc`. The object at `src/drivelst.cpp:506` is a buffer holder for `credUIPromptForCredentialsA` (`:645,722`), gated on `!Windows7AndLater` (`src/salamdr1.cpp:3905`) — dead on the Windows 11+ baseline. `CEnterPasswdDialog::Transfer` (`src/dialogs3.cpp:1800-1803`) never runs; on Windows 7+ the credentials are collected by the **system's** Unicode UI through `WNetAddConnection3(parent, ns, passwd, userName, CONNECT_INTERACTIVE | …)` (`src/drivelst.cpp:617`), where `passwd` is provably `NULL` on that platform — it is initialised to `NULL` at `:508` and assigned only inside the two `!Windows7AndLater` credUI branches (`:567`, `:721`). The finding's "worst consequence first" item does not exist in a shipped configuration. |
| **Find** ("Named", "Containing", "Look in") | **CONFIRMED** | `CFindDialog` is an ANSI `CCommonDialog` (`src/finddlg1.cpp:1365-1366`). Reads at `src/finddlg1.cpp:1778,1792` and `:1842-1846`; fills at `:1811,1813,1817` via `HistoryComboBox` → `src/viewer.cpp:62,68`. Typing a Greek mask `Δοκ*` or a Cyrillic phrase to grep for yields `???*`; Find Now then matches nothing, and "Containing" searches for literal question marks. |
| **Hot Paths** | **CONFIRMED, and the loss is persisted** | `CCfgPageHotPath` is an ANSI property-sheet page. `src/dialogs4.cpp:2891-2894` `GetDlgItemTextW(HWindow, IDC_HOTPATH_PATH, buffW, …)` and `:2907-2910` for the name: the wide read is already `?`, `SalWToU8` encodes it, `Config->SetPath(index, buff)` stores it and it is written to the registry — so the hot key jumps nowhere and the damage survives restarts. |
| **User Menu** | **CONFIRMED, persisted** | `src/dialogs4.cpp:2184-2187` `SalGetDlgItemTextU8(HWindow, IDE_COMMAND/IDE_ARGUMENTS/IDE_INITDIR, …)` on an ANSI dialog — the comment there ("read UTF-8 paths back from the **Unicode controls**") is factually wrong: those controls are ANSI. Also `:1514-1517` (in-place label edit). |
| **Pack / Unpack** | **CONFIRMED** | `CPackDialog` (`src/dialogs3.cpp:1834-1835`) and `CUnpackDialog` are plain ANSI `CCommonDialog`s; `ti.EditLine(IDE_PATH, Path, MAX_PATH)` / `ti.EditLine(IDE_MASK, Mask, MAX_PATH)` (`:2138,2145`) read back the `?`s into an operational archive path/mask. |
| **Command line** | **CONFIRMED** | `CEditLine` is an ANSI window — proven in F-P6-04 (`src/editwnd.cpp:346-347,1694-1714`, and the product's own note at `src/fileswn0.cpp:983-987`). Typed non-ACP characters are lost before `src/editwnd.cpp:390` reads them. |

**Notes**

- Wording correction: the finding says the Find field "shows `???*` as soon as
  focus leaves it". The substitution happens at the keystroke/paste, inside
  USER32 — the field shows `?` immediately, not on focus loss.
- This is not a regression of any recent feature; it is the pre-004 ANSI
  baseline that feature 015 began to lift and stopped after two dialogs. It is
  nonetheless reachable in every shipped language, because the trigger is the
  machine ACP, not the UI language.
- The suggested fix (one `unicodeWnd = TRUE` per dialog) is right in shape, but
  the enumeration of "highest-value three" should drop the password dialog and
  add the Pack/Unpack target path, whose loss is operational rather than
  cosmetic.

---

## F-P3-04 · CONFIRMED (mechanism and one consequence) · the persistence half is REFUTED

**Claim** — `EM_LIMITTEXT` is set in bytes but enforced in characters;
overflow silently degrades to a `CP_ACP` read.

**Mechanism — verified exactly as described.**
`src/common/winlib.cpp:1057` (wide branch) and `:1065` (legacy branch):
`SendMessageW(HWindow, EM_LIMITTEXT, bufferSizeInChars - 1, 0);` —
`EM_LIMITTEXT` counts **characters** (UTF-16 units in the control), while
`bufferSizeInChars` is the caller's `char*` buffer size in **bytes** (every
caller passes `sizeof(buf)` / `MAX_PATH`; see the survey below). On the way
back, `src/common/winlib.cpp:1078-1089`:
`GetWindowTextLengthW` + `GetWindowTextW` + `SalWToU8(w, -1, buffer, bufferSizeInChars)`.
`SalWToU8` is `WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, …)`
(`src/common/salunicode.cpp:286-293`), which returns **0** on
`ERROR_INSUFFICIENT_BUFFER`; the lone-surrogate retry does not apply. Control
then falls to `:1091` `SendMessage(HWindow, WM_GETTEXT, bufferSizeInChars, (LPARAM)buffer);`
— `SendMessageA` on an ANSI control, i.e. **CP_ACP bytes written into a buffer
the rest of the program treats as UTF-8**, with no signal to the caller. The
sibling `SalGetWindowTextU8` (`src/common/winlib.cpp:1116-1133`) has the
identical shape and the identical fallback.

**Reachability in a shipped configuration — CONFIRMED but narrow.**
Because the underlying controls are ANSI (see F-P6-06), their text is always
ACP-representable, so on a single-byte ACP the byte/character ratio is at most
2 (CP1250/CP1252 accented letters are 1 ACP byte and 2 UTF-8 bytes). Overflow
therefore needs roughly `bufferSize/2` accented characters:

- `src/dialogs5.cpp:2741` `ti.EditLine(IDC_TITLEBAR_PREFIX_TEXT, Configuration.TitleBarPrefix, TITLE_PREFIX_MAX)`
  with `TITLE_PREFIX_MAX = 100` (`src/cfgdlg.h:166`) → **50 accented characters
  is enough**. Same for `SEC_SUBMENUNAME_MAX = 100` (`src/shexreg.h:116`,
  `src/dialogse.cpp:54`).
- `src/dialogs2.cpp:1235-1236` `ti.EditLine(IDE_UMC_NAME1/2, CompareName1/2, MAX_PATH)`
  (`CCompareArgsDlg`, `src/dialogs2.cpp:1202-1205`) — these hold **file paths**;
  a ~250-character path with ~10 accented characters already exceeds 260 bytes,
  which is realistic on a Czech/Hungarian machine.
- `src/dialogs3.cpp:280`, `src/dialogs4.cpp:1661,1679,1711,1713,3324`
  (filter / view / hex / recycle mask **lists**, all `MAX_PATH`) — a long
  semicolon-separated list with a few accented masks reaches 260 bytes while
  staying under 259 characters.

**The one consequence that holds — CONFIRMED.** For the mask fields the ACP
bytes are not valid UTF-8, so `AgreeMask` (`src/masks.cpp:124-151`) sees
`SalU8ToWAlloc(mask) == NULL` and drops to `AgreeMaskA` byte matching against a
UTF-8 filename — a non-ASCII mask then matches nothing. The filter/view/hex/
recycle mask semantics silently change.

**REFUTED — the persistence half of the claim.** The finding says "The value is
then persisted (the registry facade sees invalid UTF-8 and re-interprets it as
CP_ACP)" as if that were the damage. It is the opposite: `SalRegSetValueExW8`
(`src/salamdr6.cpp:2416-2442`) probes with `SalU8ToW`, and on failure takes the
documented *transitional tolerance* branch
`MultiByteToWideChar(CP_ACP, 0, lpData, cbData, …)` → `RegSetValueExW`. The
value is therefore stored as **correct UTF-16** and comes back as correct UTF-8
through `SalRegQueryValueExW8`. Nothing is corrupted across a save/restart.

**Also not a consequence — display.** Every U8 sink falls back to the raw `A`
call on invalid UTF-8 (`SalSetWindowTextU8`, `src/common/winlib.cpp:1113`;
`CStaticText`'s CP_ACP ladder), so an ACP-byte title-bar prefix or mask list
still *draws* correctly on the same machine. There is no visible mojibake from
this defect.

**Notes**

- The larger instance of the same shape is in the sibling
  `SalGetWindowTextU8`, not in `EditLine`: `src/fileswn7.cpp:496`
  `CCopyMoveDialog dlg(HWindow, path, MAX_PATH, LoadStr(IDS_UNPACKCOPY), …)`
  gives one of the four **Unicode** dialogs a 260-byte buffer. There the
  control legitimately holds CJK/emoji, so a target path of 87+ CJK characters
  overflows and the fallback `GetWindowText` (A) turns the whole path into
  `?` — an operational failure of the unpack, not just a mask mismatch. The
  Rename dialog (`src/fileswn5.cpp:2386`, `SAL_FIND_NAME_U8` = 780 for a
  ≤765-byte component) and Create Directory (`:1963`, `SAL_MAX_PATH_UTF8`) are
  sized correctly and cannot overflow — so the pattern is understood elsewhere
  in the tree.
- The suggested fix "limit to `bufferSizeInChars/3`" would visibly shorten
  every field for ASCII users and is not byte-identity-preserving; re-encoding
  the fallback CP_ACP → UTF-8 (`SalLegacyToU8Alloc` shape) is the change that
  keeps existing behaviour and removes the silent encoding switch.


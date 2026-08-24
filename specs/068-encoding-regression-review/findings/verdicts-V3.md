# Verdicts — batch V3 (configuration, persistence, panel geometry)

Verifier: independent agent, charter "refute". Read-only on the product.
Established facts used throughout: the core is built **without** `UNICODE`
(un-suffixed Win32 text APIs are ANSI) and **with `/J`** (plain `char` is
unsigned, so `LowerCase[*p]` indexes correctly for bytes ≥ 0x80).
`SalRegQueryValueExW8` / `SalRegSetValueExW8` (`src/salamdr6.cpp:2310-2440`)
treated as correct: write side takes the WTF-8 branch when the payload is valid
WTF-8 and otherwise a **CP_ACP transitional branch** (`:2432-2442`); read side
**always** returns UTF-8 (`:2350` `SalWToU8`). The asymmetry is by design and is
the load-bearing fact in several findings below.

Shipped languages (`translations/languages.cfg`): czech, german, french, dutch,
hungarian, romanian, slovak, spanish (8 `enabled = on`); russian,
chinesesimplified, ukrainian are `enabled = off` ⇒ any claim that depends only
on those is LATENT.

---

## F-P4-01 · CONFIRMED

**Scenario.** Internal Viewer → *Coding* menu → **Set As Default**, on any
installation whose active conversion set is `convert\centeuro`. That set is
chosen automatically when `GetACP() == 1250` (`src/codetbl.cpp:540-546`,
criterion 2) — i.e. every Czech / Slovak / Polish / Hungarian Windows — and can
also be chosen explicitly by the user in Configuration → Viewers → *Conversion
Tables* (`src/dialogs4.cpp:943` `CConversionTablesDialog`, stored in
`Configuration.ConversionTable`, `codetbl.cpp:584-609`, criterion 1). **UI
language is irrelevant** — the defect is driven by the conversion set, not by
the translation.

The user opens a text file, picks one of the eight non-ASCII-named conversions
and chooses *Set As Default*. It holds for the rest of the session. After the
next start of Tandem Commander the default conversion is gone: the file opens
unconverted ("None"), and the *Coding* menu's default marker has moved to the
"None" item. Repeating *Set As Default* never makes it stick.

**Evidence chain.**

1. `convert/centeuro/convert.cfg` is CP1250 at rest and has **exactly eight**
   non-ASCII conversion names — verified byte-level
   (`LC_ALL=C grep -aP '[\x80-\xFF]'`), lines 42, 45, 55, 57, 65, 67, 72, 73:

   ```
   42: Kameni<E8>t<ED> - CP1250=KAME1250.TAB
   45: KOI-8 <C8>S2 - CP1250=KOI81250.TAB
   55: CP1250 - Kameni<E8>t<ED>=1250KAME.TAB
   57: CP1250 - KOI-8 <C8>S2=1250KOI8.TAB
   65: Kameni<E8>t<ED> - ASCII=KAMEASCI.TAB
   67: KOI-8 <C8>S2 - ASCII=KOI8ASCI.TAB
   72: Kameni<E8>t<ED> - CP852=KAMEC852.TAB
   73: CP852 - Kameni<E8>t<ED>=C852KAME.TAB
   ```

   (0xE8 = `č`, 0xED = `í`, 0xC8 = `Č` in CP1250; the names are *Kameničtí* and
   *KOI-8 ČS2*, not "Kamenické" as the finding spells them — immaterial.)
   `convert/cyrillic/convert.cfg` and `convert/westeuro/convert.cfg` are pure
   ASCII, so the defect is specific to the Central-European set, exactly as
   N-P4-11 says. None of these byte sequences is valid UTF-8 (0xE8 is a 3-byte
   lead followed by `t`; 0xC8 a 2-byte lead followed by `S`) and none is a
   WTF-8 lone-surrogate sequence (`ED A0 80..ED BF BF`), so every strict probe
   downstream rejects them.
2. `src/codetbl.cpp:155-156` `memcpy(name, beg, l); name[l] = 0;` — raw copy, no
   conversion at intake; `:247` `code->Name = DupStr(name);`. So
   `CCodeTablesData::Name` holds CP1250 bytes for the whole process lifetime.
3. `src/viewer3.cpp:1886` `CodeTables.GetCodeName(CodeType, DefaultConvert, 200)`
   (`CM_SETDEFAULT_CODING`) → `src/codetbl.cpp:836`
   `strcpy(buff, Table->Data[codeType - 1]->Name)` — CP1250 bytes into
   `CViewerWindow::DefaultConvert`.
4. `src/viewer3.cpp:3594` `strcpy(Configuration.DefaultConvert, DefaultConvert);`
   (on viewer close).
5. `src/mainwnd2.cpp:2001` `SetValue(actKey, VIEWER_DEFAULTCONVERT_REG, REG_SZ,
   Configuration.DefaultConvert, -1)` → `src/regwork.cpp:206-216` `SetValueAux`
   → `SalRegSetValueExW8`. `SalU8ToW` fails (step 1), so the **CP_ACP
   transitional branch** `src/salamdr6.cpp:2432-2442` stores it — correctly, as
   UTF-16 `Kameničtí - CP1250`. The write side is *not* the bug.
   Configuration is saved on exit by default: `src/dialogs4.cpp:273`
   `AutoSave = TRUE;` → `src/mainwnd3.cpp:6779`.
6. `src/mainwnd2.cpp:3638` `GetValue(actKey, VIEWER_DEFAULTCONVERT_REG, REG_SZ,
   Configuration.DefaultConvert, 200)` → `src/regwork.cpp:114-118`
   `SalRegQueryValueExW8` → **UTF-8** `Kameni\xC4\x8Dt\xC3\xAD - CP1250`.
7. `src/viewer.cpp:604` `strcpy(DefaultConvert, Configuration.DefaultConvert);`
   → `src/viewer2.cpp:1131` `if (!CodeTables.GetCodeType(DefaultConvert,
   defCodeType)) defCodeType = 0;` → `src/codetbl.cpp:774-808`, whose comparison
   is byte-wise: `:795 if (LowerCase[*n] != LowerCase[*c] || *n == 0) break;`.
   UTF-8 `C4` vs CP1250 `E8`: the skip loop (`:790-793`) only skips bytes
   `<= ' '`, `'-'`, `'&'`, so neither byte is skipped; `LowerCase[0xC4]` (CP1250
   `Ä`→`ä` = 0xE4) ≠ `LowerCase[0xE8]` (0xE8) → no match → `codeType = 0` →
   `src/viewer2.cpp:1135-1139` `CodeType = 0; UseCodeTable = FALSE;`.
8. Same loss at `src/viewer3.cpp:3273` `CodeTables.GetCodeType(DefaultConvert,
   defCodeType); SetMenuDefaultItem(subMenu, CM_CODING_MIN + defCodeType, ...)`
   — the default marker lands on "None".

**Scope.** Shipped configuration, 8 of ~30 conversions in `convert\centeuro`,
any of the 8 enabled UI languages. Not a display bug: a **persisted setting is
permanently unable to survive a restart**. Loss is silent (no error, no
message).

**Notes.** The finding's own analysis is accurate; the only corrections are the
conversion names (*Kameničtí* / *KOI-8 ČS2*) and the line of the `DupStr`
(`codetbl.cpp:247`, not quoted in the finding). The claim "8 shipped
conversions" is exact. The claim that the round trip is asymmetric is exact.
The suggested fix (normalize at `codetbl.cpp:155`) would also fix F-P4-02.

---

## F-P4-02 · CONFIRMED (mechanism and scenario) · cause attribution PARTLY REFUTED

**Confirmed part.** The composition and the sink behave exactly as claimed.
`src/viewer3.cpp:53` `CodeTables.GetCodeName(CodeType, codeName, 200)` (CP1250
bytes, F-P4-01 step 2) → `:59` `sprintf(caption + strlen(caption), " - [%s]",
codeName)` onto a buffer that already holds the UTF-8 file name (`:30`
`lstrcpyn(caption, FileName, MAX_PATH)`) → `:62` `WCHAR* captionW =
SalU8ToWAlloc(caption);`. `SalU8ToWAlloc` (`src/common/salunicode.cpp:317`) is
strict — it calls `SalU8ToW` (`:237`) = `MultiByteToWideChar(CP_UTF8,
MB_ERR_INVALID_CHARS, …)` with only the WTF-8 lone-surrogate escape hatch
(`:251`), so the CP1250 bytes make it return NULL → `:69 SetWindowText(HWindow,
caption)` = `SetWindowTextA`, which re-reads the whole buffer as CP_ACP. On a
CP1250 machine the *coding name* then renders right and the **UTF-8 file name
renders as mojibake**: `poznámky.txt` = `pozn C3 A1 mky.txt` → `poznĂˇmky.txt`.
That is a real, shipped, user-visible defect (DC-19 verbatim).

**Refuted part — "a non-ASCII *coding name* is what poisons the caption".**
In three of the eight shipped languages the caption is *already* poisoned for
every file with no coding switch at all, because `src/viewer3.cpp:40`
`strcat(caption, LoadStr(IDS_VIEWERTITLE));` is the **ANSI** `LoadStr`:

| language | `IDS_VIEWERTITLE` (10089) | caption valid UTF-8? |
|---|---|---|
| czech | `Prohlížeč` (`translations/czech/salamand.slt:1740`) | **no** |
| slovak | `Prehliadač` (`:1740`) | **no** |
| hungarian | `Néző` (`:1740`) | **no** |
| german | `Dateibetrachter` | yes |
| french | `Visionneur` | yes |
| dutch | `Viewer` | yes |
| romanian | `Vizualizator` | yes |
| spanish | `Visor` | yes |

So on a **Czech, Slovak or Hungarian UI** every viewer caption of a non-ASCII
file name is mojibake from the moment the window opens — no `centeuro`, no
coding switch, no `CM_SETDEFAULT_CODING` required. The coding name is the
*only* trigger in the other five shipped languages plus English.

**Scenario (corrected).** Two independent triggers, one sink:
- **T1 (wider, unmentioned by the finding):** Czech / Slovak / Hungarian UI, any
  ACP, view a file whose name is non-ASCII → caption reads `poznĂˇmky.txt -
  Prohlížeč`. Always.
- **T2 (the finding's):** English / German / French / Dutch / Romanian /
  Spanish UI on a `convert\centeuro` installation, view a non-ASCII-named file
  and switch to one of the eight Kameničtí / KOI-8 ČS2 codings → the caption
  flips to mojibake at the moment of the switch and repairs itself when an
  ASCII-named coding is chosen. Exactly as described.

**Scope.** Display-only (the window title); no operation fails. `Caption` from a
plugin (`viewer2.cpp:879`) is a third potential ANSI contributor, out of this
batch's scope.

**Notes.** The suggested fix (normalize `codetbl.cpp:155`) removes T2 only.
T1 needs `LoadStrU8(IDS_VIEWERTITLE)` at `viewer3.cpp:40` — this is a DC-18
missed twin that the finding does not name, and it is the *more* frequently hit
of the two. Recommend the finding be widened rather than accepted as written.

---

## F-P4-03 · CONFIRMED as an invariant break · one live consequence, in **Hungarian** only

**Plainly:** on the primary configuration (ACP matching the UI language) the
user sees nothing wrong — the finding says so itself and I could not break that.
What is real is (a) the undefined at-rest encoding, and (b) one shipped
combination where a `?` is permanently baked into the user's registry.

**Confirmed — the mechanism.** Every cited line holds:
- `src/packers.cpp:240,249,262,271` and `:1058,1068`
  `SetPacker(index, 1, LoadStr(CustomPackers[idx].Title[0]), …)` /
  `SetUnpacker(index, 1, LoadStr(CustomUnpackers[idx].Title), …)`;
  `src/packac.cpp:999,1007,1033` the same in the archiver auto-detect path.
- `LoadStr` is `LoadStringA`: `src/salamdr2.cpp:53`
  `int size = LoadString(hInstance, resID, act, …);` with no `UNICODE`, versus
  `LoadStrU8` at `:104` `LoadStringW(…)` + `SalWToU8`.
- `src/packers.cpp:734` `data->Title = DupStr(title);` — no normalization.
- `src/packers.cpp:841` `SetValue(hKey, SALAMANDER_CPU_TITLE, REG_SZ,
  GetPackerTitle(index), -1)` → facade → correct UTF-16 either way;
  `src/packers.cpp:934` `SetPacker(index, (int)type, title, …)` on reload, with
  `title` now UTF-8.
- `src/dialogs3.cpp:1861` `SalComboAddStringU8(…)` and
  `src/edtlbwnd.cpp:501/542` are tolerant sinks: `SalListViewSetItemTextU8`
  (`src/common/winlib.cpp`) probes with `SalU8ToWAlloc` and falls back to
  `SendMessageA(lv, LVM_SETITEMTEXTA, …)` — so ACP bytes render correctly on a
  matching ACP. The three encodings the finding lists (ACP from `LoadStr`,
  UTF-8 from `SalGetWindowTextU8`, UTF-8 from the registry) all coexist in the
  same field.

**Confirmed — consequence (1), but only for Hungarian.** I checked every
shipped language's packer titles (`translations/<lang>/salamand.slt`, IDs
10285-10304):

| language | sample | outside CP1252? |
|---|---|---|
| czech | `JAR (Externí Win32, testováno s v1.02)` | no (í, á, o all Latin-1) |
| slovak | `JAR (Externý Win32, testované s v1.02)` | no |
| romanian | `JAR (Extern Win32, testat cu v1.02)` | no (pure ASCII) |
| **hungarian** | `JAR (külső Win32, tesztelve: v1.02)` | **yes — `ő` U+0151** |
| german/french/dutch/spanish | Latin-1 only | no |

So the finding's example (`"ZIP (Zewn?trzny Win32 …)"`, Polish) is from a
language that is **not shipped** — Polish has no section in
`translations/languages.cfg` at all. The reachable case is **Hungarian UI on a
Western/English Windows (CP1252)**: `LoadStr` yields `JAR (küls? Win32,
tesztelve: v1.02)`, `packers.cpp:841` persists that through the facade as
UTF-16 `küls?`, and `packers.cpp:934` reloads it forever — the stored title is
*not* re-seeded (`mainwnd2.cpp:2720` re-seeds only when
`Configuration.ConfigVersion < 105`). The `?` survives even after the user
switches Windows to Hungarian. That is a genuine, permanent, site-specific
consequence and it is the part of F-P4-03 worth acting on.

**Refuted — consequence (2), "an ACP title that happens to parse as UTF-8".**
Theoretical only in a shipped configuration: no shipped packer/unpacker title in
any of the 8 languages contains a CP1250 byte run that is valid UTF-8 (the
finding's own example, CP1250 `ě ž š` = `EC 9E 9A`, does not occur in any of
them; incidentally it decodes to U+C79A, not U+E79A). It could only arise from a
user-typed title, which arrives as real UTF-8 anyway. **LATENT.**

**Scope.** Invariant break: all configurations. Live `?` damage: Hungarian UI on
a non-CP1250-capable ACP. The suggested fix (`LoadStrU8` at the nine seeding
sites) is correct and sufficient for the live half.

---

## F-P4-04 · CONFIRMED — the user loses **function**, not just text

**Plainly:** the user loses a remembered directory. Pressing the drive button no
longer returns to the folder that was open there; the panel silently lands on
the nearest existing ancestor of the mangled path (often the drive root, but
`D:\Dokumenty\Účetnictví` cuts to `D:\Dokumenty`). No error, no warning, and the
loss repeats after every restart — the memory for that drive can never survive
a restart again.

**Scenario.** Main window, any of the 8 shipped UI languages, any locale. The
user leaves a panel in `D:\Dokumenty\Účetnictví`, quits (configuration is saved
on exit by default, `src/dialogs4.cpp:273` `AutoSave = TRUE;`,
`src/mainwnd3.cpp:6779`). On the next start the user presses `Alt+F2` → `D:`
(or the `D:` Drive-bar button) and lands in `D:\Dokumenty`, not
`D:\Dokumenty\Účetnictví`. It works again for the rest of that session (the
in-memory row is rewritten in UTF-8 by `UpdateDefaultDir` the moment a panel
visits the drive) and breaks again on the next start.

Two independent ACP situations, same outcome:
- ACP matches the characters (Czech Windows, CP1250): `RegEnumValueA` yields
  CP1250 bytes, e.g. `Ú` = 0xDA followed by `č` = 0xE8 — 0xDA is a 2-byte UTF-8
  lead and 0xE8 is not a continuation byte, so the strict decoder rejects it.
- ACP cannot represent them (English Windows, CP1252): `RegEnumValueA`
  substitutes `?`, which is an illegal filename character — the path cannot
  exist.

**Evidence chain.**

1. Producer is UTF-8: `src/mainwnd1.cpp:521-552` `CMainWindow::UpdateDefaultDir`
   — `const char* pathActive = active->GetPath(); … strcpy(row, pathActive);`
   into `DefaultDir[26][MAX_PATH]` (`src/salamdr1.cpp:224`).
2. Write through the facade: `src/mainwnd2.cpp:1345` `SetValue(actKey, name,
   REG_SZ, path, -1)` → `src/regwork.cpp:206-216` `SetValueAux` →
   `SalRegSetValueExW8` → the WTF-8 branch (`salamdr6.cpp:2415-2429`) → correct
   UTF-16 in `…\Default Directories\D`.
3. Read **not** through the facade: `src/mainwnd2.cpp:2638`
   `res = RegEnumValue(actKey, i, name, &nameLen, 0, &type, path, &dataLen);` —
   no `UNICODE`, so `RegEnumValueA`; the stored UTF-16 is down-converted to
   CP_ACP by the OS. `:2647 memmove(DefaultDir[d2 - 'a'], path, dataLen);`
   The asymmetry is visible one screen away: the sibling
   `src/mainwnd2.cpp:2234` `LoadPanelConfig` restores the panel path with
   `GetValue(actKey, PANEL_PATH_REG, REG_SZ, …)` → facade → UTF-8, and it
   restores correctly. Only the drive memory is wrong.
4. Consumers treat it as UTF-8: `src/fileswn3.cpp:2749`
   `ChangePathToDisk(HWindow, DefaultDir[LowerCase[drive] - 'a'], …)` and
   `src/toolbar6.cpp:188` (Drive bar).
5. Sink behaviour, read not assumed: `src/fileswn2.cpp:1778`
   `SalCheckAndRestorePathWithCut(parent, changedPath, …)` →
   `src/salamdr5.cpp:614` `SalCheckPath` → `src/salamdr5.cpp:157`
   `SalGetFileAttributes(threadPath)` → `src/common/salfileio.cpp`
   `SalPathToWExtAlloc` → `SalU8ToWAlloc` (strict, `salunicode.cpp:317/237`)
   returns NULL → `SetLastError(ERROR_INVALID_NAME); return
   INVALID_FILE_ATTRIBUTES;`.
   `ERROR_INVALID_NAME` **is** in `IsDirError` (`src/salamdr1.cpp:1500`), so
   `src/salamdr5.cpp:645` `CutDirectory(path); cut = TRUE;` shortens the path
   component by component until it exists. The drive-change call sites pass
   `shorterPathWarning = FALSE` (`fileswn3.cpp:2749`, `toolbar6.cpp:189`), so
   **nothing is reported to the user**.

**Scope.** Every drive except the two the panels happen to be on at startup
(those get rewritten by `UpdateDefaultDir` as soon as they are used). All 8
shipped languages, all locales; ASCII paths are unaffected.

**Notes.** Correction to the finding: the panel does **not** open at `D:\` in
general — `SalCheckAndRestorePathWithCut` cuts to the nearest existing ancestor,
so the user usually lands one or more levels above the remembered directory.
That makes the symptom *less* obviously a bug, not more. The rest of the
finding's chain is exact. The suggested fix (26 explicit `GetValue` calls, value
names are the single letters written at `:1343`) is sound; no registry migration
is needed, because `SalRegQueryValueExW8` reads the stored UTF-16 and therefore
loads values written by any earlier version correctly.

---

## F-P4-05 · CONFIRMED (mechanism) · the "OneDrive-specific" framing REFUTED

**Plainly:** the user loses **function** — the folder does not open. Not a text
defect.

**Confirmed.** The OneDrive folder path is acquired in CP_ACP and then used as a
UTF-8 panel path. All three producers verified:

- `src/drivelst.cpp:1481` `done = ConvertU2A(path, -1, OneDrivePath,
  _countof(OneDrivePath)) != 0;` — `path` is the wide `FOLDERID_SkyDrive` result
  of `SHGetKnownFolderPath` (`:1478`). `ConvertU2A`'s signature is
  `int ConvertU2A(const WCHAR* src, int srcLen, char* buf, int bufSize,
  BOOL compositeCheck = FALSE, UINT codepage = CP_ACP);`
  (`src/common/strutils.h:17-18`) and the implementation is a bare
  `WideCharToMultiByte(codepage, …)` (`src/common/strutils.cpp`) — a
  non-representable character becomes `?` and is **not** reported as an error.
  This is the **live** producer on Windows 8.1+ (the comment at `:1479` says so).
- `src/drivelst.cpp:1503` `SalRegQueryValueEx(hKey, "UserFolder", …)` — the
  legacy ANSI wrapper, `src/salamdr6.cpp:2248-2253`
  `RegQueryValueEx(hKey, lpValueName, lpReserved, &type, lpData, lpcbData);`
  = `RegQueryValueExA`. Reached only when the known-folder call fails (older
  Windows / OneDrive not set up).
- `src/drivelst.cpp:1533,1537` — the same ANSI wrapper for the **Business**
  accounts' `DisplayName` and `UserFolder` under
  `HKCU\Software\Microsoft\OneDrive\Accounts`. This path has **no** wide
  alternative and always runs (`:1516-1550`).

Consumer: `src/fileswn3.cpp:2583-2586` `ChangePathToDrvType` →
`strcpy_s(path, OneDrivePath)` / `strcpy_s(path, userFolderOneDrive)` → `:2588`
`ChangePathToDisk(parent, path)`. Sink behaviour is the same chain proved under
F-P4-04: `SalGetFileAttributes` → `SalPathToWExtAlloc` → strict `SalU8ToWAlloc`
→ NULL → `ERROR_INVALID_NAME` → `IsDirError` true → the path is cut back
component by component, silently.

**Scenario.** `Alt+F1`/`Alt+F2` → *OneDrive* (or the Drive-bar OneDrive button),
any of the 8 shipped UI languages, on a machine where the OneDrive folder path
contains a non-ASCII character. The two realistic cases:
- a non-ASCII Windows profile folder (`C:\Users\Jiří\OneDrive`), and
- **OneDrive Business** with a non-ASCII tenant name
  (`C:\Users\jan\OneDrive - Přátelé s.r.o.`) — common in the Czech / Slovak /
  Hungarian markets these translations serve.

The user clicks OneDrive and the panel lands on the nearest existing ancestor
(`C:\Users`, or `C:\Users\jan` for the Business case), with no error message.
The OneDrive item stays in the menu, so it looks as if the click did nothing.

**Refuted part — "Everything else in the panel (Dropbox, Google Drive, plain
drives) works, which makes it look like a OneDrive-specific bug."** False, and
the finding's own site list contradicts it. Two siblings have the *identical*
CP_ACP down-conversion of a cloud folder path:

- `src/drivelst.cpp:1384` `ConvertU2A(widePath, -1, mbPath, _countof(mbPath))`
  — the **Dropbox** path, decoded from Base64 and widened as UTF-8 at `:1383`
  (`ConvertA2U(secRow, -1, widePath, _countof(widePath), CP_UTF8)`) and then
  thrown back down to CP_ACP with the default parameter;
  `:1387 strcpy_s(DropboxPath, mbPath);`.
- `src/shiconov.cpp:161` `ConvertU2A(widePath, -1, mbPath, _countof(mbPath))`
  — the **Google Drive** sync-root path, read as UTF-8 out of
  `sync_config.db`, widened at `:160` with `CP_UTF8` but narrowed back with
  **CP_ACP**; `:171 strcpy_s(gdPath, gdPathMax, mbPath);`. Contrast
  `shiconov.cpp:146`, fifteen lines further up, which *does* pass `CP_UTF8`
  explicitly — exactly the DC-18 twin the finding names, but the finding then
  forgets it in the scenario.

So the correct scenario is "every folder-based cloud-storage entry in the drive
menu fails on a non-ASCII path"; OneDrive is not special. (A letter-mapped
Google Drive `G:` is a plain drive and is unaffected — consistent with feature
058, which fixed the *icon* pipeline for `G:\Můj disk`, not path acquisition.)

**Scope.** Shipped configuration, all 8 languages. Functional failure, silent.
The display half of the finding (`U+FFFD` in the directory line via
`stswnd.cpp:164 SalU8ToWDisplayAlloc`) is not reached in practice, because the
path is cut before it is ever shown; the drive-menu *label* for Business
accounts is a separate ANSI/ANSI composition (`src/drivelst.cpp:2021`
`sprintf_s(itemText, "%s - %s", LoadStr(IDS_ONEDRIVE),
OneDriveBusinessStorages[i]->DisplayName)`), which renders correctly on a
matching ACP and degrades to `?` otherwise.

---

## F-P4-06 · CONFIRMED — mojibake **and** loss of function

**Plainly:** both. The jump-list entry's title and tooltip show mojibake, and
clicking it does not take the user to the hot path.

**Scenario.** Windows taskbar / Start-menu jump list, any of the 8 shipped UI
languages, any locale whose ACP is not UTF-8. The user assigns a hot path
containing non-ASCII characters — `Ctrl+Shift+<n>` on the current directory
(`src/mainwnd1.cpp:1299-1325` `SetUnescapedHotPath` → `HotPaths.Set(index, "",
buff)`), or names one in Configuration → Hot Paths. The jump list is rebuilt
immediately (`mainwnd1.cpp:1324`, `mainwnd3.cpp:1963`) and at every start
(`salamdr1.cpp:4487`, all guarded only by `Windows7AndLater`). Right-clicking
the taskbar button shows the entry as `D:\PrĂˇce\ĂšÄŤetnictvĂ­` instead of
`D:\Práce\Účetnictví`; clicking it launches Tandem Commander with a path that
does not exist, so the panel ends up somewhere else (or reports the path as not
found) instead of the hot path.

**Evidence chain.**

1. Source is UTF-8. `src/jumplist.cpp:225-226`
   `MainWindow->HotPaths.GetDisplayName(i, name, MAX_PATH);` /
   `MainWindow->HotPaths.GetPath(i, path, HOTPATHITEM_MAXPATH);`.
   `CHotPathItems` (`src/mainwnd.h:130-230`) stores what `Set`/`SetName` are
   given; producers are the UTF-8 panel path (`SetUnescapedHotPath`) and the
   registry facade — `src/mainwnd1.cpp:213-244` `Save` uses
   `SetValue(actKey, SALAMANDER_HOTPATHS_NAME/PATH, REG_SZ, …)` and `:258-305`
   `Load` uses `GetValue(…)`, i.e. `SalRegQueryValueExW8` → UTF-8.
2. `src/jumplist.cpp:153` `sprintf(params, "-AJ \"%s\"", path);` — the UTF-8
   path becomes the launch argument.
3. `src/jumplist.cpp:157` `IShellLink* ret;` + `:159` `CoCreateInstance(…,
   IID_PPV_ARGS(&ret))`. The project sets no `<CharacterSet>` (checked
   `src/vcxproj/salamand.vcxproj` and the shared `.props`), so MSBuild's default
   `MultiByte` applies, `UNICODE` is not defined, `IShellLink` is
   **`IShellLinkA`** and `IID_PPV_ARGS` resolves to `IID_IShellLinkA`.
   Consequently the shell converts every string through CP_ACP:
   - `:167 ret->SetPath(pathName);` (`:164 GetModuleFileName(NULL, …)`, also ANSI)
   - `:168 ret->SetArguments(params);` — **the operational payload**
   - `:173 ret->SetDescription(desc);` — the tooltip, `desc` is the UTF-8 path
   - `:174 ret->SetIconLocation("shell32.dll", -319);` — ASCII literal, harmless.
4. `src/jumplist.cpp:183-185` `pv.vt = VT_LPSTR; pv.pszVal = (LPSTR)name;
   pPS->SetValue(PKEY_Title, pv);` — `VT_LPSTR` is the ANSI PROPVARIANT string
   type; propsys coerces it to `VT_LPWSTR` through CP_ACP. This is the visible
   entry title. (The `HRESULT` is not checked, so if a future shell rejected
   `VT_LPSTR` outright the entry would silently lose its title instead.)
5. Return trip, proving the click really fails: `src/salamdr1.cpp:3583`
   `const WCHAR* cmdLineW = GetCommandLineW();` → `:3601 cmdLineU8 =
   SalWToU8Alloc(argsW, -1);` (feature 004, comment at `:3578-3581`) → `:3651`
   the `-aj` branch fills `cmdLineParams->ActivePath` →
   `src/salamdr1.cpp:4484 ApplyCommandLineParams` →
   `src/mainwnd3.cpp:783 GetActivePanel()->ChangeDir(cmdLineParams->ActivePath);`.
   Because the new process reads the *wide* command line faithfully, it receives
   the ACP-mangled characters as genuine Unicode and re-encodes exactly those —
   the accidental byte round trip a fully-ANSI pipeline would have had is gone.
   Bytes the ACP leaves undefined (CP1250: 0x81 0x83 0x88 0x90 0x98 — e.g.
   Polish `Ł` U+0141 = UTF-8 `C5 81`) are not even recoverable in principle.

**Scope.** Shipped configuration, all 8 languages, any hot path whose name or
path is non-ASCII. ASCII hot paths are unaffected, which is why this has gone
unnoticed. Note the jump list is built unconditionally on every start when at
least one visible hot path has a path (`jumplist.cpp:222-232`).

**Notes.** The finding is accurate throughout; I found no overstatement. Two
additions: (a) `GetModuleFileName` at `:164` is a second ANSI call that breaks
the link entirely under a non-ASCII installation directory (P1's seed C-c);
(b) the suggested fix must also switch `CoCreateInstance` to `IID_IShellLinkW`
explicitly, not just convert the arguments, because `IID_PPV_ARGS(&ret)` derives
the IID from the declared pointer type.

---

## F-P4-07 · CONFIRMED for the view-mode names · REFUTED on "the damage is permanent" · LATENT for the user-menu item

**Confirmed — mechanism and sites.**
- `src/salamdr4.cpp:1006-1009` — inside `CViewTemplates::Load`, after
  `GetValue(actKey, SALAMANDER_VIEWTEMPLATE_NAME, REG_SZ, name, VIEW_NAME_MAX)`
  has already produced a UTF-8 name, `if (resID != -1) strcpy(name,
  LoadStr(resID));` overwrites it with ANSI text for the seven built-in
  templates. Same in the defaults at `src/salamdr4.cpp:799-805`
  (`Set(1, VIEW_MODE_BRIEF, LoadStr(IDS_BRIEF_VIEW), …)`) → `:818
  lstrcpyn(Items[index].Name, name, VIEW_NAME_MAX);`.
- Consumers are UTF-8-contract: `src/dialogs4.cpp:1022`
  `SalListViewSetItemTextU8(HListView, i, 0, Config.Items[i].Name); // name is
  UTF-8 (feature 005)`, and the View-mode menu via
  `src/mainwnd3.cpp:559-600`. User-typed names in slots 8-10 enter the same
  array as real UTF-8 (`src/dialogs4.cpp:1517 SalGetWindowTextU8`). So the
  array genuinely holds two encodings side by side — the DC-18 twin is real.
- `src/dialogs4.cpp:2279` `new CUserMenuItem(LoadStr(IDS_ENDUSERSUBMENU),
  emptyBuffer, …, umitSubmenuEnd, NULL)` → persisted at
  `src/mainwnd2.cpp:2040` and reloaded as UTF-8 at `:2869/:2977`. Confirmed.

**Refuted — "the damage is permanent" for view templates.** It is not.
`CViewTemplates::Load` re-seeds the seven built-in names from `LoadStr` on
*every* load (`salamdr4.cpp:983-1009`), so whatever is stored is discarded. A
`?`-mangled name therefore disappears the moment the ACP can represent the
language again. The finding's "the damage is permanent and visible side by
side with correct text" is true only for the *side by side* half.

**Confirmed — the visible `?` half, for 3 of 8 languages.** Checking the actual
strings (IDs 13680-13687):

| language | non-CP1252 characters |
|---|---|
| **czech** | `&Stručný` (`č` U+010D), `&Dlaždice` (`ž` U+017E) |
| **slovak** | `&Stručný`, `&Dlaždice` |
| **romanian** | `P&lăci` (`ă` U+0103) |
| hungarian | none (`Részletes`, `Típus`, `Bélyegkép` are all Latin-1) |
| german/french/dutch/spanish | none |

So on a CP1252 Windows with the Czech, Slovak or Romanian UI the Views page and
the `Alt+1..0` menu read `Stru?ný` / `Dla?dice` / `Pl?ci`. **However** — and
this materially weakens the finding — the core still has **1263** `LoadStr(`
call sites against **152** `LoadStrU8(` ones, so on that configuration most of
the UI shows `?` too. These sites are not distinguished by their symptom, only
by the fact that a `…U8` consumer was already put in place for them.

**LATENT — the user-menu "(Submenu End)" marker.** All 8 shipped strings are
CP1252-representable — czech `(Konec podnabídky)`, hungarian `(Almenü vége)`,
german `(Untermenü Ende)`, french `(Fin du sous-menu)`, slovak
`(Koniec podponuky)`, romanian/dutch/spanish ASCII. There is no shipped
language × ACP combination in which this item is damaged; it would need an ACP
from a different script entirely (Greek, Cyrillic, CJK), at which point the
whole UI is unusable. Invariant break, no reachable failure.

**Scope.** Invariant break: all configurations, worth fixing as hygiene
(`LoadStrU8` at `salamdr4.cpp:799-805,1009` and `dialogs4.cpp:2279`). Reachable
user-visible symptom: Czech/Slovak/Romanian UI on a CP1252 ACP, view-mode names
only, and not distinguishable from the product-wide `LoadStr` situation.

---

## F-MC-01 · CONFIRMED — but the surface is **rubber-band selection only**, not hit-testing

**What `CFilesMap` is actually used for.** Not click hit-testing. Its only
consumers are the drag-selection ("selection box" / rubber band) handlers:
`src/fileswn1.cpp:1388 FilesMap.SetPanel(this)`,
`src/fileswn9.cpp:1313 if (FilesMap.CreateMap())` … `:1323 SetAnchor` …
`:1326/:1341 SetPoint`, and `src/fileswn0.cpp:544/557 DrawDragBox` /
`DestroyMap`. A plain mouse click is resolved by `CFilesBox`, not by this map.
So the finding's "mouse hit-testing and drag-selection regions disagree" is half
right: **only** the drag-selection region is wrong.

**Plainly:** the user gets **over-selection** — dragging a selection rectangle
through the empty space to the right of a file name still selects that file,
when the name contains non-ASCII characters.

**Scenario.** Panel in **Brief** or **Detailed** view with *Full Row Select*
**off** — which is the **default** (`src/dialogs4.cpp:378 FullRowSelect =
FALSE;`). Any UI language, any locale, any ACP: the trigger is the *file name*,
not the translation. The user presses the left (or right) button on empty panel
background and drags a selection box. Files whose names contain non-ASCII
characters — `žluťoučký kůň.docx`, `Přehled 2026.xlsx` — are selected even when
the rectangle stays entirely to the right of where their name is drawn. In Brief
view the over-reach is unbounded and can extend past the item's column; in
Detailed view it is clamped to the Name column width (`src/filesmap.cpp:148-149`).

**Evidence chain.**
1. `src/filesmap.cpp:107` `char formatedFileName[SAL_FIND_NAME_U8]; // UTF-8
   name (feature 004)` → `:119 AlterFileName(formatedFileName, f->Name, -1, …)`
   → `:122 const char* s = formatedFileName;`.
2. `:134 len = (int)(f->Ext - f->Name - 1);` or `:137 len = f->NameLen;` —
   both **byte** counts. `CFileData::NameLen` is documented as such:
   `src/plugins/shared/spl_com.h:222` `unsigned NameLen; // length of Name in
   bytes (strlen(Name)); UTF-8 byte count since …`.
3. `:146 GetTextExtentPoint32(dc, s, len, &sz);` — no `UNICODE`, so
   `GetTextExtentPoint32A`, which converts those `len` bytes from **CP_ACP**.
   Each 2-byte UTF-8 character is measured as two ACP characters (CP1250:
   `ž` = `C5 BE` → `Ĺ` + `ľ`; CP1252: → `Å` + `¾`), so `sz.cx` is far too large.
4. `:147 width += sz.cx + 4;` → `:154 itemIter->Width = width;`.
5. The width is the selection extent: `src/filesmap.cpp:425-429`
   `int mx = col * Panel->ListBox->ItemWidth; int mw = item->Width;
   if (oldRectLeft > mx + mw) inOld = FALSE;` (and the matching `newRect` arm) —
   an item stays inside the rubber band while the band's left edge is within
   `mx + Width`. Too-large `Width` ⇒ over-selection, never under-selection.
6. The sibling drawing path is correct, which proves this is a missed twin:
   `src/fileswn4.cpp:711` `wNameLen = SalU8ToW(TransferBuffer, nameLen, wbuf,
   _countof(wbuf)) - 1;` then `:738 GetTextExtentPoint32W(hDC, wbuf, wNameLen,
   &fnSZ);` (and `:730 GetTextExtentExPointW` for the clamped case), with the
   byte-wise `A` call kept only as an explicitly commented
   "invalid UTF-8 (should not happen)" fallback. `adjR.right = r.right =
   rect.left + 1 + IconSizes[ICONSIZE_16] + 1 + 2 + fnSZ.cx + 3;` is what the
   user sees; `filesmap.cpp` computes `IconSizes[ICONSIZE_16] + 2 + sz.cx + 4`
   from a different, wrong `sz`.

**Magnitude.** For `žluťoučký kůň.docx` (18 characters, 7 of them two-byte, 25
bytes) the map measures ~25 glyph widths where ~18 are drawn — roughly 40 %
too wide. For an ASCII name the two agree to within the constant 1-pixel offset
difference between the two formulas (`+7` vs `+6`), which is pre-existing and
harmless.

**Scope.** Shipped configuration, **default** settings, all 8 languages, all
ACPs. Cosmetic-plus: no data is lost, but a selection the user did not make can
precede a Delete/Move. The finding's framing as "Czech UI/locale" is too narrow
(the UI language is irrelevant) and its claim about hit-testing is wrong; its
core claim — byte-width measurement of a UTF-8 name by an ANSI API, with the
sibling draw path already converted — is exactly right.

# Verdicts — batch V2 (cross-cutting converter & dialog machinery)

Independent verifier, charter = **refute**. Read-only on the product; no build.
Findings verified: F-P3-01, F-P3-02, F-P3-03, F-P3-05, F-P3-06, F-P3-07,
F-P3-08, F-P3-09 (all from `findings/P3.md`).
Blocks appear in verification order (headline first), not numeric order.

Ground rules applied: core built **without** `UNICODE` (un-suffixed Win32 text
API = ANSI entry point), **with `/J`** (plain `char` unsigned), 8 shipped UI
languages (cs, de, en, es, fr, hu, pl, sk-ish set — ru/uk/zh disabled ⇒ any
claim resting only on those is **LATENT**).

---

## F-P3-05 · CONFIRMED (with two corrections)

**The mechanism is real, and the repository itself documents it.**

### Mechanism (verified independently of the finding)

1. `CWindowsObject` carries `BOOL UnicodeWnd` (`src/common/winlib.h:131`),
   defaulted **FALSE** in every constructor
   (`winlib.h:144,154,166,366,380`; `CCommonDialog` re-defaults it FALSE at
   `src/salamand.h:768,782`).
2. `CDialog::Execute()` (`src/common/winlib.cpp:614-626`) calls
   `DialogBoxParamW` **only** `if (UnicodeWnd)`; otherwise
   `DialogBoxParam` = `DialogBoxParamA`. `CDialog::Create()`
   (`winlib.cpp:628-641`) is the same for modeless dialogs.
   Property-sheet pages are ANSI unconditionally:
   `src/common/sheets.cpp:277` `CreatePropertySheetPage(&psp)` and
   `sheets.cpp:499` `PropertySheet(&psh)` are both the A entry points, with
   no W branch anywhere in the file.
3. A dialog created through the A entry point is an ANSI window and the dialog
   manager creates its child controls ANSI as well; `SetWindowTextW` /
   `SendMessageW(WM_SETTEXT|WM_GETTEXT|CB_ADDSTRING|LB_ADDSTRING|EM_*)` on such
   a window is thunked by USER32 through the process ANSI code page.
4. **In-repo confirmation** — this is not my inference, the team wrote it down
   twice while doing feature 015:
   - `src/salamand.h:763-765`: *"'unicodeWnd' TRUE makes the dialog a Unicode
     window (DialogBoxParamW), so controls can hold characters outside the
     system ANSI code page and the Sal\*U8 (wide) helpers **stop being lossy
     (?-substitution)**."*
   - `src/dialogs3.cpp:391-392`: *"Unicode dialog so the path/name combo can
     show characters outside the ANSI code page (emoji, Cyrillic, ...) instead
     of '?'."*
   - `src/salamdr3.cpp:3872-3875` (`CKeyForwarder`): *"the ANSI variant would
     flip a Unicode control (e.g. a combo edit in a Unicode dialog) to ANSI,
     **turning non-ANSI text into '?'**"* — which also proves the propagation:
     in a Unicode dialog the child combo edit *is* a Unicode control, so in an
     ANSI dialog it is an ANSI control.

### Opt-in census (recounted, not taken from the finding)

`grep ': CCommonDialog(|: CDialog(|: CCommonPropSheetPage(|: CPropSheetPage('`
over `src/*.cpp` → **90** constructions (the finding says 86 — my count is 90;
the difference does not change anything). Exactly **2** pass
`TRUE /*unicodeWnd*/`:
- `src/dialogs3.cpp:394` `CCopyMoveDialog`
- `src/dialogs3.cpp:588` `CCopyMoveMoreDialog`

Two further opt-ins exist but are `CWindow`, not dialogs:
`src/filesbx1.cpp:25` (`CFilesBox`, the panel) and `src/fileswn5.cpp:2914`
(quick-rename edit). So **88 of 90 dialogs/pages are ANSI windows**, and every
`SalSetWindowTextU8` / `SalSetDlgItemTextU8` / `SalGetWindowTextU8` /
`SalGetDlgItemTextU8` / `SalComboAddStringU8` / `SalListBoxAddStringU8` /
`CTransferInfo::EditLine` call on them round-trips through CP_ACP.

Sink implementations read (they do what the finding says):
`SalSetWindowTextU8` → `SetWindowTextW` (`winlib.cpp:1102-1114`);
`SalGetWindowTextU8` → `GetWindowTextLengthW`+`GetWindowTextW`
(`winlib.cpp:1116-1132`); `SalComboAddStringU8` → `SendMessageW(CB_ADDSTRING)`
(`winlib.cpp:1155-1167`); `SalListBoxAddStringU8` →
`SendMessageW(LB_ADDSTRING)` (`winlib.cpp:1188-1200`);
`CTransferInfo::EditLine` → `SendMessageW(WM_SETTEXT)` / `GetWindowTextW`
(`winlib.cpp:1042-1095`, esp. `:1055-1057`, `:1075-1089`).

### The "not affected" list is correct

`SalListViewSetItemTextU8` uses `LVM_SETITEMTEXTW` (`winlib.cpp:1202-1218`),
`SalStatusSetTextU8` uses `SB_SETTEXTW` (`winlib.cpp:1222+`),
`SalInsertMenuItemU8` uses
`InsertMenuItemW` (`winlib.cpp:1169-1186`) — the first two are message numbers
distinct from their A twins (USER32 has no text thunk for them), the third is
not a window message at all. **Verified: not affected.** This asymmetry really
is why the 043/052 listview and 052 status-bar fixes work while the
edit/combo/window-text ones only work for ACP-representable text.

### Per-victim scope (display-only vs actioned) — the part that scopes a fix

| Victim | Construction | Damage |
|---|---|---|
| **Message boxes** (`CMessageBox`) | `msgbox.cpp:54` `CCommonDialog(HInstance, IDD_MSGBOX, parent, ooStandard)`, `msgbox.cpp:89` (2nd ctor, defaults) — no `unicodeWnd`; shown via `.Execute()` (`msgbox.cpp:1280-1294`, `SalMessageBoxEx`) | **DISPLAY-ONLY.** Title (`:472`), body (`:475`), re-wrapped body (`:698` `SetWindowTextW`), button labels (`:925`, `:960`) all render `?`. Nothing is read back into an operation: Ctrl+C recomposes from the *members* `Title`/`Text` (`msgbox.cpp:372-386`), which are still true UTF-8. Only the button labels in the copied text are read back wide from the control (`:405`, `:431`) and therefore inherit the `?`. |
| **Change Directory (Shift+F7)** `CChangeDirDlg` | `dialogs3.cpp:1180` `CCommonDialog(HLanguage, IDD_CHANGEDIR, IDD_CHANGEDIR, parent)` | **ACTIONED + PERSISTED.** `dialogs3.cpp:1198` `SalSetWindowTextU8(hWnd, Path)` shows `?`; `:1201` `SalGetWindowTextU8(hWnd, Path, 2*MAX_PATH)` reads the `?` back into `Path`, which is then the change-directory target **and** is written to the persisted history at `:1202` `AddValueToStdHistoryValues(...)`. |
| **Find "Look in" / "Named" / "Containing"** `CFindDialog` | `finddlg1.cpp:1366` `CCommonDialog(HLanguage, IDD_FIND, NULL, ooStandard, hCenterAgainst)` | **ACTIONED + PERSISTED.** `finddlg1.cpp:1778/1792` `SalGetWindowTextU8` into `Data.NamedText` / `Data.LookInText`; `:1842-1846` `SalGetDlgItemTextU8`; set side `viewer.cpp:62` (`HistoryComboBox` → `SalSetWindowTextU8`). The search root and the mask are the `?` values; `viewer.cpp:105+` stores them into the persisted history. |
| **Pack / Unpack** | `dialogs3.cpp:1833` `CPackDialog : CCommonDialog(HLanguage, IDD_PACK, IDD_PACK, parent)`, `dialogs3.cpp:2079` `CUnpackDialog` likewise | **ACTIONED.** `:1891/:1894` and `:2128/:2131` `SalComboAddStringU8(combo, Path/PathAlt)` populate the combo, `CB_SETCURSEL` puts a `?` string into the edit, `:1898` / `:2138` `ti.EditLine(IDE_PATH, Path, MAX_PATH)` and `:2145` `ti.EditLine(IDE_MASK, ...)` read it back as the archive target path / extraction mask. |
| **User-menu editor** `CCfgPageUserMenu` | property-sheet page ⇒ ANSI by `sheets.cpp:277/499` | **ACTIONED + PERSISTED, and silently destructive.** `dialogs4.cpp:2157,2158,2160` `SalSetDlgItemTextU8` fills command/arguments/init-dir; `:2185,2186,2187` `SalGetDlgItemTextU8` reads them back and `:2190` `item->Set(...)` overwrites the stored item. Merely opening the User Menu page and switching selection rewrites an existing entry containing non-ACP characters with `?`. |

### Scenario (the concrete one to reproduce)

Windows with ACP 1250 (Czech) or 1252 (Western), any of the 8 shipped UI
languages. Panel is in `C:\Документы` (or an emoji-named folder). Press
**Shift+F7**: the combo shows `C:\?????????`; press Enter → Tandem Commander
tries to change to `C:\?????????`, which is not a legal path → "path not found"
error, and the mangled string is added to the Change Directory history in the
registry. Same class of failure with **Alt+F7 Find** (searches a nonexistent
root) and **Alt+F5 Pack** (writes the archive to a `?` path or refuses).
No Cyrillic needed for a Western ACP: on ACP 1252 a plain Czech path
`D:\Můj disk\…` already breaks (`ů` U+016F is not in CP1252).

### Corrections to the finding (partly refuted)

1. **"the delete/overwrite confirmation … shows `?????????.txt`"** — the
   *delete* confirmation is indeed a `CMessageBox` (`fileswn8.cpp:960-967`,
   `fileswn7.cpp:732`, `fileswna.cpp:277`), so that half stands. The
   ***overwrite* confirmation is REFUTED**: `COverwriteDlg::DialogProc`
   (`src/dialogs.cpp`, `WM_INITDIALOG`) renders both names through
   `CStaticText` (`new CStaticText(HWindow, IDS_SOURCENAME, STF_PATH_ELLIPSIS)`
   → `SetTextToDblQuotesIfNeeded`), and `CStaticText::SetText`
   (`gui.cpp:625-637`) builds its own UTF-16 mirror and owner-draws it — the
   ANSI-ness of the host dialog is irrelevant. Only the two *attribute* strings
   (`SalSetDlgItemTextU8(HWindow, IDS_SOURCEATTR/IDS_TARGETATTR, …)`) go
   through the thunk, and those are size+date/time (ASCII in all shipped
   locales — cf. latent row S-X-P3-020). **Every dialog that shows a name via
   `CStaticText` is immune**; the fix scope must not include those.
2. **"every character outside the ACP becomes `?`"** — USER32's W→A thunk uses
   `WideCharToMultiByte(CP_ACP, 0, …)` with default flags, i.e. **best-fit
   mapping is on**. Some characters therefore become an approximation rather
   than `?` (`ů`→`u` on CP1252 is the typical case). The value is lossy and the
   actioned path is still wrong, so the verdict is unchanged, but a fix
   verification that greps for `'?'` will miss half the cases.

### Suggested fix scope, ordered by damage

Actioned/persisted first: `CChangeDirDlg` (`dialogs3.cpp:1180`),
`CFindDialog` (`finddlg1.cpp:1366`), `CPackDialog`/`CUnpackDialog`
(`dialogs3.cpp:1833`, `:2079`), the config property sheet
(`sheets.cpp:277,499` — needs the W entry points, one `unicodeWnd` argument is
not enough for pages). Display-only afterwards: `CMessageBox`
(`msgbox.cpp:54,89`).

**Subclassing hazard for whoever fixes this** (verified, not in the finding):
any `CWindow` subclass attached to a control of a newly-Unicode dialog must
carry `unicodeWnd = TRUE`, otherwise `CWindow::AttachToWindow`
(`winlib.cpp:251-291`) calls the **A** `SetWindowLongPtr` and flips the control
back to ANSI — re-introducing the defect through the back door.
`CreateKeyForwarder` (`salamdr3.cpp:3932-3957`) is already safe: it passes
`IsWindowUnicode(hWindow)`. **`CKeyForwarderWindow` in `msgbox.cpp:16-21` is
not** — `CWindow(hDlg, ctrlID)` with the default `unicodeWnd = FALSE`,
attached to every message-box button at `msgbox.cpp:963`. Flipping
`CMessageBox` to Unicode without fixing that line would ANSI-ify the buttons.
`CHyperLink` (`msgbox.cpp:479`, `dialogs3.cpp:1224`) and `CButton`/`CStaticText`
attachments need the same audit.

---

## F-P3-06 · CONFIRMED in part / REFUTED in part

**CONFIRMED: the `LowerCase[]` half (`StrICmp`/`StrNICmp`/`IsTheSamePath`).**
**REFUTED: the `safefile.cpp:155` operational claim and the `IsAlpha[]` /
`IsNotAlphaNorNum[]` claims (find.cpp, codetbl.cpp).**

### Table construction — verified

- `src/common/str.cpp:109-116` `InitializeCase()`:
  `LowerCase[i] = (char)(UINT_PTR)CharLowerA((LPSTR)(UINT_PTR)i);` (and
  `UpperCase` from `CharUpperA`) — one **ACP byte** in, one ACP byte out.
  Declared `BYTE LowerCase[256]` (`str.cpp:63`, `str.h:22`).
- `src/salamdr1.cpp:936-942` `InitLocales()`:
  `IsNotAlphaNorNum[i] = !IsCharAlphaNumeric((char)i); IsAlpha[i] = IsCharAlpha((char)i);`
  — un-suffixed ⇒ `IsCharAlphaNumericA`/`IsCharAlphaA`, one ACP byte.
- Indexing is safe (no UB): `/J` makes `char` unsigned, so `LowerCase[*s1]`
  is 0..255. The x64 build uses the C++ `StrICmp`/`StrNICmp`
  (`str.cpp:134-146` / `:223-236`, `#ifdef _WIN64`), which index
  `LowerCase[*s1]` directly.

### The arithmetic — verified against the CP1250 table

- `ĥ` U+0125 = `C4 A5`, `Ĺ` U+0139 = `C4 B9`. CP1250: 0xA5 = `Ą` (U+0104),
  0xB9 = `ą` (U+0105), so `CharLowerA(0xA5) == 0xB9 == CharLowerA(0xB9)`.
  Lead byte `C4` folds identically in both. ⇒ **`StrICmp("ĥ","Ĺ") == 0`.**
  Confirmed. The same holds for every CP1250 case pair whose two bytes are
  both in 0x80–0xBF (all valid UTF-8 continuation bytes):
  (8A,9A) (8C,9C) (8D,9D) (8E,9E) (8F,9F) (A3,B3) (A5,B9) (AA,BA) (AF,BF) —
  nine collision pairs, so e.g. `Č` (`C4 8C`) also compares equal to
  `Ĝ` (`C4 9C`).
- `Č.txt` = `C4 8C …`, `č.txt` = `C4 8D …`. CP1250: 0x8C = `Ś`→0x9C,
  0x8D = `Ť`→0x9D. Different ⇒ **`StrICmp("Č.txt","č.txt") != 0`**, although
  NTFS considers them the same file. Confirmed.

### Consumers — verified, and the finding *under*-states the reach

- `src/fileswn1.cpp:2443` and `:2458` — focus-by-name after a refresh, exactly
  as claimed. The loop takes the exact `strcmp` match when present, so the
  damage is confined to (a) case-only renames, where `StrICmp` now fails and
  the item is simply not focused, and (b) two look-alike names present at once,
  where the wrong one is focused.
- **The largest consumer is not in the finding**: `IsTheSamePath`
  (`src/salamdr1.cpp:1398-1412`) walks `LowerCase[*path1] == LowerCase[*path2]`
  byte by byte and is called from ~20 core sites, including the *auto-refresh*
  path matching (`fileswn2.cpp:1788,1793,1817`), the "not on the original path
  → long jump" test (`fileswn2.cpp:407`), `GetPath()` identity
  (`fileswn2.cpp:2037`), the Windows-directory test (`fileswn3.cpp:131`), the
  junction-redirect tests (`fileswn3.cpp:1762,1855`), and the
  "Copy of…" decision (`fileswn6.cpp:698`). `CFilesWindowAncestor::SamePath`
  (`fileswn1.cpp:362-374`) uses `StrNICmp` for the same purpose.
  A false *equality* here makes the panel treat a change notification for
  `…\ĥ` as one for `…\Ĺ`; a false *inequality* is impossible for these,
  because the two paths are byte-identical in the ordinary case.
- `SalNameEqualCI` / `SalCompareNamesUTF8` (`salunicode.h:211,220`,
  `salunicode.cpp:593,627`) are the correct machinery, with an ASCII fast path.
  Correction to the finding: they are already used in **three** places, not
  one — `sort.cpp:29`, `fileswn0.cpp:72,85` (mask matching) and
  `finddlg1.cpp:4051` — which makes the "route the name sites through it"
  suggestion cheaper than the finding implies.

### REFUTED — `safefile.cpp:155-156` does not carry an operational failure

The guard is
`if (StrICmp(tgtName, foundDosName) == 0 && StrICmp(tgtName, foundName) != 0)`
(`src/safefile.cpp:155-156`). `foundDosName` comes from
`dataW.cAlternateFileName` — the 8.3 short name, which Windows generates from
the OEM character set and is ASCII in practice. For the **first** guard to
fire with a non-ASCII `tgtName`, some UTF-8 byte ≥ 0x80 would have to fold to
an ASCII byte. It cannot: under CP1250 (and CP1252, CP1251) `CharLowerA`
maps every byte ≥ 0x80 to another byte ≥ 0x80 (letters to their lowercase,
which is also ≥ 0x80; non-letters unchanged). So the first guard can only fire
when `tgtName` is ASCII in exactly the positions where `foundDosName` is —
i.e. pure ASCII — and ASCII folding is exact. The second guard is then also
exact. **No path from the byte-table defect to the DOS-collision rename.**
Residual caveat, not a shipped-scenario claim: on a **Turkish** ACP (1254)
`CharLowerA(0xDD /* İ */) == 0x69 /* i */`, which *is* an ASCII target and
0xDD *is* a valid UTF-8 lead byte — so on that one ACP the argument above does
not hold. That is a different (and far more remote) scenario than the finding
described; treat it as a note, not a finding.

### REFUTED — the `IsAlpha[]` / `IsNotAlphaNorNum[]` consumers

- `src/find.cpp:1271-1273` and `:1316-1320` ("Whole words only") index the
  tables with **file-content bytes** (`txt[off-1]`, `*(beg+found-1)`), not with
  a UTF-8 name. The searched file has no declared encoding — this is a
  byte-oriented grep by design and its behaviour is unchanged since upstream.
  It is not a feature-004 encoding regression. (A UTF-8-encoded source file
  can get a spurious word boundary at a continuation byte, but that is
  pre-existing grep semantics, and the *search string* damage on that dialog
  is already covered by F-P3-05.)
- `src/codetbl.cpp:979-1047` is the code-page **auto-detection heuristic**: it
  scores a buffer that has just been mapped through a candidate single-byte
  code table. Byte-wise classification is the whole point of the algorithm.
  Not a defect.
- `src/finddlg1.cpp:1303,1305` is the only genuine UTF-8 consumer
  (`format1` comes from `SalGetDateFormatU8`), and it only decides a listview
  **column width**. All shipped short-date formats are ASCII ⇒ **LATENT**,
  and it is already recorded as such in row S-X-P3-020.

### Scenario for the confirmed half

Any UI language, ACP 1250. Panel contains `Č.txt`; press Shift+F6 and rename
it to `č.txt` (a case-only rename, legal on NTFS). After the refresh the panel
cursor does not follow the renamed item — `fileswn1.cpp:2458`
`StrICmp("Č.txt", "č.txt") != 0` and neither the exact nor the ignore-case
index is found. Reproducible with any of the nine CP1250 case pairs above.

### Scope

`src/common/str.cpp:109-116` (table), `str.cpp:134-146`, `:223-236`
(`StrICmp`/`StrNICmp`), `src/salamdr1.cpp:1398-1412` (`IsTheSamePath`), plus
the name-bearing call sites listed above. Do **not** include `find.cpp`,
`codetbl.cpp` or `safefile.cpp` in a fix scoped to this finding.

---

## F-P3-01 · REFUTED (the API observation stands; the named consequence does not)

### What is true

`SalU8ToW` really cannot tell "malformed" from "buffer too small". Verified
line by line:
- `src/common/salunicode.cpp:237-266`: `MultiByteToWideChar(CP_UTF8,
  MB_ERR_INVALID_CHARS, …)` returns 0 for **both** `ERROR_NO_UNICODE_TRANSLATION`
  and `ERROR_INSUFFICIENT_BUFFER`; the `res == 0 && srcLen != 0` guard at
  `:247` routes both into the WTF-8 fallback.
- `SalWtf8ToWBytes` does distinguish them (`-2` too small at `:182`,`:192`;
  `-1` malformed at `:162,169,177`), and `SalU8ToWWtf8`
  (`salunicode.cpp:206-227`) collapses `-1` and `-2` into `return 0` at
  `:210-214` — and again at `:216-221` for the explicit-length case.
- `salunicode.cpp:259-263` is a third capacity-failure-as-0 site
  (`if (res >= bufSize) { buf[0] = 0; return 0; }`).
- The finding's own note is right: the buffer is **not** left indeterminate —
  every failure path writes `buf[0] = 0` (`:210-212`, `:218`, `:261`,
  `:264-265`). Ledger L08's "indeterminate buffer" wording is stale.

### Why the finding is REFUTED

The claimed data path is **factually wrong at its only cited consumer.**
`src/snooper.cpp:582-586` does *not* "read that 0 as 'not convertible' and
return":

```
if (SalU8ToW(path, -1, wPath, _countof(wPath)) == 0)
{
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, path, -1, wPath, _countof(wPath));
    wPath[_countof(wPath) - 1] = 0;
}
HANDLE h = HANDLES_Q(FindFirstChangeNotificationW(wPath, …));
```

There is a CP_ACP fallback and the W call is made unconditionally. The same
shape appears at `snooper.cpp:731-735` and `:769-773`. So "the panel silently
stops auto-refreshing because the conversion returned 0" does not happen —
and it is not silent either: the failure branch logs
`TRACE_W("Unable to receive change notifications for directory …")`
(`snooper.cpp:615-619`, `:745-748`).

Further, the *cure* the finding proposes would not help: even if the caller
could tell the two causes apart, `wPath` is a fixed `WCHAR[3 * MAX_PATH]`
(780) — there is no larger buffer to retry into.

### Residual (a different, real defect — worth raising separately)

The snooper's `WCHAR wPath[3 * MAX_PATH]` (780 units) is measured against
`MAX_PATH`, while the panel path buffer is `char Path[SAL_MAX_PATH_UTF8]`
(`fileswnd.h:479`, `salpath.h:21-22` = 3 × 32767 + 1). For a feature-027 path
longer than 779 UTF-16 units **both** conversions fail on capacity, `wPath`
ends up empty or truncated, `FindFirstChangeNotificationW` fails and
auto-refresh is turned off for that panel. That is a **DC-12 capacity defect
at `snooper.cpp:581,730,768`**, independent of the converter's return-value
ambiguity, and it is the only thing at these sites that can actually hurt a
user. (I also checked the neighbouring
`MakeCopyWithBackslashIfNeeded(path, pathCopy)` at `:577`: it takes `name` by
**reference** and reassigns it (`salamdr5.cpp:1196-1207`), so the fix-up is
not dead code, and it is length-guarded — no overflow.)

### Scope

Converter observation: `src/common/salunicode.cpp:206-227`, `:237-266`. Note
level (contract clarity), no shipped failure. The snooper capacity defect is a
separate item.

---

## F-P3-02 · LATENT

**Mechanism confirmed, consequence nil — the finding says so itself and it is
correct.**

- `src/common/salunicode.cpp:247` `if (res == 0 && srcLen != 0)`: with
  `srcLen == 0` the WTF-8 fallback is skipped, `res > 0` is false, and control
  reaches `:264-266` → `buf[0] = 0; return 0`. Verified.
- `SalWToU8` mirrors it at `:288` (`SalWHasLoneSurrogate(src, 0)` is FALSE) →
  `:307-309` → 0. Verified.
- Note this matches the WinAPI it wraps: `MultiByteToWideChar` /
  `WideCharToMultiByte` themselves fail with `ERROR_INVALID_PARAMETER` when
  the source length is 0, so "0 in ⇒ 0 out" is WinAPI parity, not a deviation
  invented here.
- The one live caller: `src/stswnd.cpp:704`
  `SalWToU8(TextW + item->Offset, item->Chars, buffer, bufSize)` with
  `item->Chars == 0` returns 0, falls to `SalWToU8Alloc(…, 0)` at `:706`,
  which returns NULL (`salunicode.cpp:330+` sizes via the same 0-returning
  call), and `buffer` keeps the `buffer[0] = 0` written at `stswnd.cpp:695`.
  Empty in, empty out — the correct answer. Verified.

**LATENT**: a genuine trap for a future caller that computes a slice length,
no shipped failure. Verdict matches the finding's own "latent" status; it
should be recorded as a Note, not a Finding.

---

## F-P3-09 · REFUTED (as a Finding); the code defect is confirmed as a Note

### The mechanism is exactly as described — verified

`src/finddlg2.cpp:168-174`:
`listW = malloc(listSize * sizeof(WCHAR))` then
`SalU8ToW(list, (int)listSize, listW, (int)listSize)`.
For an all-ASCII double-NUL list, `MultiByteToWideChar` produces exactly
`listSize` WCHARs, so `salunicode.cpp:259` `if (res >= bufSize)` is
`listSize >= listSize` → TRUE → `buf[0] = 0; return 0`. `finddlg2.cpp:171-175`
frees `listW` and the code takes `SHFileOperationA` at `:196-207`.
With one multi-byte character present, WCHARs ≤ `listSize - 1`, the guard does
not fire, and the wide branch at `:180-193` runs. So the wide path is reached
**only** for a non-ASCII selection — the comment at `:166-168` ("convert … so
Unicode and long names survive") does not describe what the code does.

### Why REFUTED as a Finding

No behavioural difference exists for the only input that takes the A branch.
`SHFileOperationA` and `SHFileOperationW` are both MAX_PATH-bound, and for a
pure-ASCII `pFrom` the shell's CP_ACP decode is the identity. I also checked
the WTF-8 case (a lone-surrogate name): `MultiByteToWideChar` fails, the WTF-8
fallback decodes 3 bytes → 1 WCHAR, so `res < bufSize` and the **wide** branch
is taken correctly — the one case where the A path would actually be wrong is
not reachable.

Per the common rules ("no scenario → it is a Note, not a Finding"), this is a
Note. Fix it anyway — the correct shape is one line away and already exists at
`src/fileswn8.cpp:133-141` (`wchars = SalU8ToW(…, NULL, 0)`,
`malloc((wchars+1)*sizeof(WCHAR))`).

---

## F-P3-07 · CONFIRMED (mechanism), but the severity and the site group are overstated

### Mechanism — verified end to end

1. `src/stswnd.cpp:1854` `lstrcpyn(text, str, TOOLTIP_TEXT_MAX)` — a plain byte
   clamp on the UTF-8 directory-line path (`str = Text`, the UTF-8 status text;
   the two `LoadStr` branches above it only apply when `WholeTextVisible`).
   `TOOLTIP_TEXT_MAX` = **5000** (`src/plugins/shared/spl_gui.h:53`).
2. `src/tooltip.cpp:305` `SendMessage(HNotifyWindow, WM_USER_TTGETTEXT, LastID,
   (LPARAM)Text)` brings those bytes into `CToolTip::Text[5000]`
   (`tooltip.h:48`).
3. `src/tooltip.cpp:309` `int lenW = SalU8ToW(Text, TextLen, TextW,
   TOOLTIP_TEXT_MAX)` — the **strict** probe. A cut inside a multi-byte
   sequence makes it fail (a trailing partial sequence is not WTF-8 either).
4. `src/tooltip.cpp:313` `TextLenW = MultiByteToWideChar(CP_ACP, 0, Text,
   TextLen, TextW, TOOLTIP_TEXT_MAX - 1)` — the fallback re-reads the **whole**
   buffer as CP_ACP, so one torn character at the end costs every accented
   character in the hint. Confirmed: all-or-nothing, exactly as claimed.
5. The correct trim exists and is not reused: `CopyToolTipAnswer`
   (`src/gui.cpp:982-1002`) walks back over continuation bytes and drops an
   incomplete lead — feature 063, contract C3 ("clamps cut on UTF-8
   boundaries"). `stswnd.cpp` and `drivelst.cpp` bypass it.

### Corrections — the finding overstates reach

- **The threshold is ~5000 bytes, not "a long path".** The finding reads as if
  an ordinary long accented path triggers it; it needs a directory-line string
  of **4999+ bytes** — roughly 20x `MAX_PATH`, only constructible with
  feature-027 deep nesting. Genuinely reachable (no disabled configuration is
  involved) but exotic. Severity: cosmetic, display-only, self-evident when it
  happens.
- **Two of the four sites in group S-X-P3-013 are REFUTED.**
  `stswnd.cpp:1878` (`ThrobberTooltip`) and `:1884` (`SecurityTooltip`) are fed
  only from `CStatusWindow::SetThrobberTooltip` / `SetSecurityTooltip`
  (`stswnd.cpp:515,540`), whose only callers are the plugin services
  `zip.cpp:4962` and `:5009`. No core producer can approach 5000 bytes there;
  a plugin that does is a plugin bug.
- **`drivelst.cpp:2697` is REFUTED**: it clamps `item->DriveText` for
  `drvtOneDriveBus` — a OneDrive account display name, orders of magnitude
  below 5000 bytes.

### Scenario (for the one confirmed site)

Czech UI on an ACP-1250 machine, panel standing in a deeply nested path whose
UTF-8 form exceeds 4999 bytes and whose byte 5000 falls inside a multi-byte
character. Hover the directory line: instead of the path, the tooltip shows the
whole string as CP1250 mojibake.

### Scope

`src/stswnd.cpp:1854` only. One-line fix: route through `CopyToolTipAnswer`
(export it from `gui.cpp`).

---

## F-P3-08 · LATENT

### Mechanism — verified, and it is a real contract violation

- `src/stswnd.cpp:164` `TextW = SalU8ToWDisplayAlloc(Text);` — the **lenient**
  converter, whose header says *"NEVER use it on a value that will be written
  back into a name, a path, or anything persisted"* (`salunicode.h:90-92`).
- `src/stswnd.cpp:692-714` `GetItemText` rebuilds the UTF-8 answer from that
  mirror (`SalWToU8(TextW + item->Offset, item->Chars, buffer, bufSize)` at
  `:704`); the byte-wise `else` branch at `:713` is reachable only when
  `TextW == NULL`, i.e. on allocation failure. The lenient mirror is the sole
  source.
- The consumers really are operational, and I confirmed both:
  `src/stswnd.cpp:2244` -> `FilesWindow->ChangeDir(path, ...)` (`:2250`), and
  `src/stswnd.cpp:1960` -> the directory-line **drag** payload, after
  `CompleteDirectoryLineHotPath` / `ConvertPathToExternal` hand it to the
  plugin.
- Offsets are coherent (`BuildHotTrackItems`, `stswnd.cpp:220+`, remaps
  `Offset`/`Chars` to WCHAR units when `TextW != NULL`), so this is not an
  index bug — it is purely the lenient decode.
- `SalU8ToWDisplay` (`salunicode.cpp:653-684`) tries strict `SalU8ToW` first
  and only falls to `MultiByteToWideChar(CP_UTF8, 0, ...)` on non-WTF-8 input,
  so every UTF-8/WTF-8 producer round-trips exactly. The finding states this
  correctly.

### Why LATENT: I could not find a shipped producer of non-UTF-8 status text

The directory-line text is built at `src/fileswn1.cpp:1755-1806`. For
`ptDisk`/`ptZIPArchive` it is the panel path — UTF-8/WTF-8 by the facade. For
`ptPluginFS` it is `<fsname>:` + `GetPluginFS()->GetCurrentPath(...)`, with no
normalization in the core. So the question is whether any **enabled** plugin
(`plugins.cfg`: 18 on) returns non-UTF-8 there. I checked every FS plugin that
implements `GetCurrentPath`:

| Plugin | enabled | verdict |
|---|---|---|
| `ftp` | on | **UTF-8** — `FTPListingFieldToU8` (`ftputils.cpp:3442-3494`) for listing names and `FTPDecodeServerReplyPathInPlace` (`ftputils.cpp:1684-1694`, "interface 104") for the PWD reply; the "keep the raw bytes" last resort at `:3487` is unreachable because `MultiByteToWideChar(CP_ACP, 0, ...)` does not fail for a single-byte ACP |
| `sftp` | on | UTF-8 (SFTP names are UTF-8 by protocol; `MakeUserPart`, `sftp/fs.cpp`) |
| `regedt` | on | **UTF-8** — the FS is wide internally (`RegEnumKeyExW`/`RegOpenKeyExW`) and `GetCurrentPath` converts with `WStrToU8` (`regedt/fs2.cpp`) |
| `undelete` | on | UTF-8 — names converted with `WideCharToMultiByte(CP_UTF8, ...)` (`undelete/library/miscstr.cpp:252,269`) |
| `portables` | on | UTF-8 — WPD names via `WideCharToMultiByte(CP_UTF8, ...)` (`portables/device.cpp:69-72`) |
| `folders` | on | returns an empty user part |
| `demoplug` | **off** | not shipped |

So in a shipped configuration the strict fast path always wins and the round
trip is exact. **LATENT** — a real DC-14 contract violation and a real trap for
any third-party plugin, but no reachable failure with the plugins we ship.

Two ways it could become live, both worth recording rather than acting on now:
a third-party FS plugin, and a chain through F-P3-04 (an over-long accented
filter mask read back as raw CP_ACP bytes and appended to the directory-line
text at `fileswn1.cpp:1798`).

### Note on the regression framing

It *is* a regression in shape: pre-feature-010 `GetItemText` copied the raw
bytes (`stswnd.cpp:713`), so a non-UTF-8 plugin path survived the click. The
mirror closed that door. That strengthens the case for fixing it even while it
is latent — keep the UTF-8 `Text` as the authority and use `TextW` only for
measuring and painting.

---

## F-P3-03 · LATENT (and the "two contracts disagree" premise is REFUTED)

### The behaviour is exactly as described — verified

`src/common/salpath.cpp:255-263`:

    WCHAR* w = SalU8ToWAlloc(u8path, -1);
    if (w == NULL)
        return NULL;

and every facade turns NULL into a hard failure with the same block, e.g.
`salfileio.cpp:27-33` (`SalFindFirstFile`), `:83-89` (`SalCreateFile`),
`:94-100` (`SalCreateFileNH`), and the `SalPathOp` / `SalMoveFileEx` /
`SalCopyFile` / `SalGetFileAttributes` / `SalSetFileAttributes` wrappers:
`SetLastError(ERROR_INVALID_NAME); return INVALID_HANDLE_VALUE / FALSE;`.
No CP_ACP fallback anywhere in `salfileio.cpp`. Confirmed.

### REFUTED: B7.2 and the facade do not "disagree"

B7.2 ("Invalid UTF-8 -> legacy CP_ACP/ANSI fallback, never fail the operation
outright") is stated in `specs/058-fix-cloud-status-icons/contracts/path-encoding-icon-pipeline.md`,
whose subject is the **icon / overlay / change-notification** pipeline —
read-only, best-effort surfaces where a wrong-but-plausible result beats no
result, and where the pre-004 code really did operate on those bytes. The
snooper honours it (`snooper.cpp:584,733,771`), as does
`CSalamanderGeneral::GetFileIcon`.

For a **mutating** file operation the same rule would be actively dangerous: a
CP_ACP re-read of a byte string that is not valid UTF-8 names a *different*
file, so `SalDeleteFile` would delete, and `SalCreateFile` would create,
something the caller did not name — silently. Failing with
`ERROR_INVALID_NAME` is the correct degradation for that class, and it is what
keeps the house-wide "valid UTF-8, else ANSI" heuristics safe. The two rules
are correctly different, not in conflict. The finding's defensible core is
narrower: **the divergence is undocumented** — B7.2 is written as if it were
global.

### Why LATENT rather than CONFIRMED

The finding names no shipped producer, and I could not construct one.
`SalPathToWExtAlloc` returns NULL only for: NULL/empty input; a `\\?\` path
over `SAL_MAX_PATH_W`; a relative path whose `GetFullPathNameW` overflows
`WCHAR[2 * MAX_PATH]`; or `SalU8ToWAlloc` failing, i.e. bytes that are neither
valid UTF-8 nor WTF-8. Every path producer I traced yields valid UTF-8/WTF-8:
the panel and the facade intake (`SalConvertFindDataW`, WTF-8), the registry
facade (`SalRegQueryValueExW8` — and note Windows stores registry strings as
UTF-16, so even a pre-004 ANSI-written value reads back as *valid* UTF-8 with
wrong characters, not as invalid UTF-8), and all seven FS plugins in the table
under F-P3-08. The single plugin caller of the facade,
`src/plugins/ftp/operats5.cpp:1308` via `CSalamanderGeneral::SalCreateFileEx`
(`src/zip.cpp:4262-4266`), passes a local disk path assembled from
already-normalized components.

To promote this to CONFIRMED someone must name a shipped producer of
non-UTF-8 path bytes — that is P5's intake territory, not this batch's.

### Scope if acted on

Documentation, not code: state in the facade contract that
`SalCreateFile` / `SalDeleteFile` / ... fail closed on non-UTF-8 input **by
design**, and why B7.2's fallback is confined to read-only/best-effort
surfaces. A code change here should be resisted.

---

## Summary table

| Finding | Verdict | One line |
|---|---|---|
| F-P3-05 | **CONFIRMED** | 88 of 90 dialogs/pages are ANSI windows; the W setters/getters thunk through CP_ACP. Actioned+persisted damage in Change Directory, Find, Pack/Unpack and the user-menu editor; display-only in message boxes. Overwrite dialog and every `CStaticText` name are immune (refuted). |
| F-P3-06 | **CONFIRMED in part** | `LowerCase[]`/`StrICmp`/`StrNICmp`/`IsTheSamePath` half confirmed (and reaches further than claimed). `safefile.cpp:155` and the `IsAlpha[]`/`IsNotAlphaNorNum[]` consumers refuted. |
| F-P3-01 | **REFUTED** | The conflation is real, but the snooper has a CP_ACP fallback, not a `return`; the real defect there is a 780-WCHAR buffer vs a 32767-unit path. |
| F-P3-02 | **LATENT** | Mechanism confirmed (and it is WinAPI parity); the one live caller is benign. Note, not Finding. |
| F-P3-03 | **LATENT** | Behaviour confirmed; the B7.2 conflict premise refuted (fail-closed is right for mutating ops); no shipped producer of non-UTF-8 paths found. |
| F-P3-07 | **CONFIRMED (edge)** | Torn clamp -> whole-tooltip CP_ACP fallback is real at `stswnd.cpp:1854`, but needs a 4999+ byte path; the other three sites in the group are refuted. |
| F-P3-08 | **LATENT** | Lenient mirror -> operational value is a real DC-14 violation, but all seven shipped FS plugins normalize to UTF-8. |
| F-P3-09 | **REFUTED** | Off-by-one confirmed exactly as described; produces no behavioural difference. Note, not Finding. |

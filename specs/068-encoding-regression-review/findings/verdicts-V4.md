# Verdicts — batch V4 (independent verifier)

Scope: `F-P2-01 … F-P2-07`, `F-P2-09 … F-P2-13` (`findings/P2.md`) — composed
messages and UI sinks. Charter: **refute**. A finding survives only where the
data path reproduces in the code *and* the ANSI half is actually non-ASCII in a
**shipped** language (`enabled = on` in `translations/languages.cfg`: czech,
german, french, dutch, hungarian, romanian, slovak, spanish; russian,
ukrainian, chinesesimplified are `off` ⇒ they can only make a finding LATENT).

## Machinery re-verified before use (independent of P2's claims)

- **`LoadStr` is ANSI** — `src/salamdr2.cpp:53` `int size = LoadString(hInstance,
  resID, act, …)`, i.e. `LoadStringA` (core built without `UNICODE`) ⇒ ACP
  bytes. **`LoadStrU8` is UTF-8** — `src/salamdr2.cpp:104-109` `LoadStringW`
  → `SalWToU8`.
- **`GetErrorText` is UTF-8** — `src/salamdr2.cpp:206-232`: `FormatMessageW` →
  `SalWToU8`, truncated only at a UTF-8 boundary; the ACP `FormatMessage`
  branch is reached only when the wide call itself fails.
- **`SalSetWindowTextU8` / `SalSetDlgItemTextU8` are all-or-nothing** —
  `src/common/winlib.cpp:1100-1112`: `SalU8ToWAlloc(u8Text)`; on NULL
  (= *any* byte of the buffer is not valid WTF-8) it falls through to
  `SetWindowText` (A), which projects the whole buffer through the ACP.
  `src/common/winlib.cpp:1131-1136` `SalSetDlgItemTextU8` = `GetDlgItem` +
  `SalSetWindowTextU8`.
- **`CMessageBox` body/title sink** — `src/msgbox.cpp:472,475`
  `SalSetWindowTextU8(HWindow, Title); … SalSetDlgItemTextU8(HWindow,
  IDS_MSGBOX_TEXT, Text.Get());` ⇒ one ACP byte in the composed body forfeits
  the wide path for the entire message.
- ACP assumption: the scenarios below assume the user's ACP matches the UI
  language (Czech Windows ⇒ CP1250). If the ACP cannot represent the UI
  language's characters, `LoadStringA` already best-fits/`?`-substitutes them
  and the template is degraded before any composition — a different,
  pre-existing defect, and in that case the template becomes ASCII and the
  finding's own mechanism disappears. Both are stated per finding where
  relevant.
- Translation lookup method: `IDS_*` → numeric id from `src/texts.rh2` /
  `src/*.rh*`, then the `[STRINGTABLE]` (or `[DIALOG]`) row in
  `translations/<lang>/salamand.slt`, tested for any code point > 127.
  All eight shipped languages checked for every id below; results are quoted
  as `cs de fr nl hu ro sk es`.

## Summary

| Finding | Verdict | Languages live | Scope |
|---|---|---|---|
| F-P2-01 | CONFIRMED (partly — `salamdr3.cpp` sites reach only the plugin `errBuf` channel) | cs de fr hu sk (+es for one id) | display |
| F-P2-02 | CONFIRMED | cs de fr hu sk | display |
| F-P2-03 | CONFIRMED (both halves) | log: cs de fr hu sk es · caption: cs hu sk | display |
| F-P2-04 | CONFIRMED (2 path sites) · LATENT (7 `plugins2.cpp` sites) | cs de fr hu sk (+nl latent) | display |
| F-P2-05 | REFUTED as characterized · CONFIRMED reclassified (DC-06 at `dialogs.cpp:522`, queued progress title only) | all, incl. English | display |
| F-P2-06 | CONFIRMED 4 bullets · 2 near-unreachable · 3 LATENT · 1 REFUTED (`drivelst.cpp` is dead code) | cs de fr hu sk es (per bullet) | display |
| F-P2-07 | CONFIRMED junction row only · UNC + SUBST rows REFUTED (`WNetGetConnectionA` / `QueryDosDeviceA` yield ACP) | cs fr hu ro sk | display |
| F-P2-09 | CONFIRMED Location (narrow) · LATENT Version | all | display |
| F-P2-10 | CONFIRMED | cs hu sk (not de/es — item name is ASCII there) | display |
| F-P2-11 | CONFIRMED | cs de fr hu sk es (not en/nl/ro) | display |
| F-P2-12 | CONFIRMED, incl. the actioned claim | all, incl. English | **stored / actioned** |
| F-P2-13 | CONFIRMED; suppression L13 premise stale | cs de fr nl hu sk es (7 of 8) | display |

---

## F-P2-01 · CONFIRMED (in part — see Notes for `salamdr3.cpp`)

**Claim** — Copy/Move error message boxes compose an ANSI `LoadStr` template
with a UTF-8 path and UTF-8 `GetErrorText`, so the box falls back to the ANSI
path and the path/error text renders as mojibake.

**Scenario** — Czech UI on Czech Windows (ACP 1250). Panel on
`D:\Zálohy\Účetnictví\`, F5 copy a subtree; a directory inside it cannot be
enumerated (permissions, a disconnected share, a cloud placeholder error).
`src/fileswn6.cpp:1943`:

```
_snprintf_s(text, _TRUNCATE, LoadStr(IDS_CANNOTREADDIR), sourcePath, GetErrorText(err));
```

`sourcePath` is `char sourcePath[SAL_MAX_PATH_UTF8 + 10]`
(`src/fileswn6.cpp:1272`) — the UTF-8 panel path that was just handed to the
`SalFindFirstFile` facade at `:1937` (`WIN32_FIND_DATAW fW; HANDLE search =
SalFindFirstFile(finalName, &fW);`), so it is UTF-8 by construction;
`GetErrorText` is UTF-8. The template is CP1250. The buffer therefore contains
both CP1250 and UTF-8 bytes → `params.Text = text` → `SalMessageBoxEx` →
`src/msgbox.cpp:475` → `SalU8ToWAlloc` returns NULL →
`src/common/winlib.cpp:1112` `SetWindowText` (A). **What the user sees**: the
Czech template renders correctly and `D:\ZÃ¡lohy\ÃšÄetnictvÃ­` plus the Czech
system error text render as mojibake, in a box that asks Skip / Skip all /
Cancel — i.e. the user cannot tell which directory failed.

**Languages affected** (template non-ASCII ⇒ defect visible):

| id | cs | de | fr | nl | hu | ro | sk | es |
|---|---|---|---|---|---|---|---|---|
| `IDS_NAMEISTOOLONG` (10135) | ✗ | ✗ | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_TOOLONGNAME2` (11250) | ✗ | ✗ | ✗ | ok | ✗ | ok | ✗ | ✗ |
| `IDS_CANNOTREADDIR` (10137) | ✗ | ok | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_FILEISTOOBIGFORFAT32` (13760) | ✗ | ✗ | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_NAMEUSEDFORFILE` (10257) | ✗ | ✗ | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_CREATEDIRFAILED` (10258) | ✗ | ✗ | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_PATHINARCHIVENOTFOUND` (12201) | ok | ok | ✗ | ok | ✗ | ok | ✗ | ok |

(all seven are non-ASCII in ru/uk/zh too — latent there.) In **Dutch and
Romanian** every one of these templates is pure ASCII, so those two languages
are *not* affected by this finding; **English** likewise. P2's per-id language
lists are accurate.

Note the second half is real independently of the template: `GetErrorText` on a
localized Windows is non-ASCII by itself, but that alone does not break
anything (a homogeneous UTF-8 buffer takes the wide path) — the defect needs
the ANSI template, which is exactly what the table above establishes.

**Evidence chain**

- `src/fileswn6.cpp:1590` `LoadStr(IDS_NAMEISTOOLONG), dirName, sourcePath`;
  `:1659` `LoadStr(IDS_TOOLONGNAME2), targetPath, s2`; `:1943`, `:2161`,
  `:2318` `LoadStr(IDS_CANNOTREADDIR), sourcePath, GetErrorText(err)`;
  `:2594` `LoadStr(IDS_FILEISTOOBIGFORFAT32), op.SourceName` — all verified
  verbatim, all feed `params.Text` (e.g. `:1599`) → `SalMessageBoxEx`.
- `src/salamdr1.cpp:1097` `_snprintf_s(text, _TRUNCATE,
  LoadStr(IDS_NAMEISTOOLONG), name, path);` inside the
  `len >= SAL_MAX_PATH_UTF8` branch → `params.Text` at `:1105+`. Confirmed.
- `src/fileswn3.cpp:2423` `_snprintf_s(errBuf, …, LoadStr(
  IDS_PATHINARCHIVENOTFOUND), end); SalMessageBox(HWindow, errBuf, …)` —
  `end` is a path-inside-archive component from the UTF-8 panel path.
  Confirmed; affects fr/hu/sk only.
- Sink: `src/msgbox.cpp:475` → `src/common/winlib.cpp:1104-1112`.

**Scope** — **display-only**. Every site is an error/notification box; the
composed buffer is never written back or actioned. The underlying operation
proceeds on the true UTF-8 path.

**Notes / partial refutation**

- `src/salamdr3.cpp:1172`, `:1219`, `:1258` are **genuine DC-18 twins** —
  the *same function* already uses `LoadStrU8(IDS_CREATEDIRFAILED)` at
  `src/salamdr3.cpp:1106` and `:1151`, where the buffer goes straight to
  `SalMessageBox(parent, buf, …)`. But at `:1172/:1219/:1258` the composed
  `buf` **never reaches a core message box**: each is followed by
  `if (errBuf != NULL) strncpy_s(errBuf, …)` `else { CFileErrorDlg dlg(parent,
  LoadStr(IDS_ERRORCREATINGDIR), dir, GetErrorText(…), …); }` — the dialog
  takes `dir` and `GetErrorText` as *separate* fields, not `buf`, and the
  `SalMessageBox(parent, buf, …)` line is commented out at `:1187`, `:1234`,
  `:1273`. All four core callers pass `errBuf = NULL`
  (`src/cache.cpp:491,538,1178`, `src/fileswn5.cpp:2010`,
  `src/fileswn7.cpp:1802`). The only live consumer is the plugin forwarder
  `src/zip.cpp:782-786`, used by the shipped **ZIP** plugin
  (`src/plugins/zip/extract.cpp:1238,1275`, `add.cpp:2226`) which passes the
  buffer to `ProcessError(IDS_ERRCREATEDIR, …, errBuf)`. So these three sites
  are defective **only through the plugin boundary**, not in the core UI —
  P2's scenario ("F7-new-directory") does not reach them. Fix scoping should
  reflect that.
- `src/fileswn6.cpp:1590` composes **two** UTF-8 arguments (`dirName`,
  `sourcePath`); `:1659` likewise. Both verified UTF-8 (`SAL_MAX_PATH_UTF8`
  buffers on the panel path).
- Suggested fix (`LoadStr` → `LoadStrU8`) is sound at every confirmed site;
  it is the feature-042/067 recipe and is byte-identical for ASCII templates
  (English, Dutch, Romanian).

---

## F-P2-02 · CONFIRMED

**Claim** — "cannot execute viewer/editor" and user-menu execute errors compose
an ANSI template with a UTF-8 command line and UTF-8 `GetErrorText`.

**Scenario** — Czech UI on Czech Windows. Configure an external viewer whose
path contains a non-ASCII character (Options → Viewers/Editors, e.g.
`D:\Programy\Průzkumník\view.exe`) — or simply F3 a file under
`D:\Zálohy\…`, because `ExpandCommand` substitutes `$(Name)`/`$(FullName)`
from the panel into `expCommand`. The launch fails (file missing, policy,
bad path). `src/fileswn5.cpp:1126`:

```
sprintf(buff, LoadStr(IDS_ERROREXECVIEW), expCommand, GetErrorText(err));
SalMessageBox(parent, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
```

`expCommand` is declared `char expCommand[SAL_MAX_PATH_UTF8]`
(`src/fileswn5.cpp:1070`, `:1393`) and is the exact buffer handed to the UTF-8
facade `SalCreateProcess` at `:1121` / `:1444` — UTF-8 by construction.
Template CP1250 + UTF-8 arguments ⇒ `src/msgbox.cpp:475` →
`src/common/winlib.cpp:1112` ANSI fallback. **What the user sees**: correct
Czech sentence, garbled command line and garbled Czech system error text.

The user-menu twins are identical: `src/mainwnd4.cpp:941` and `:1008`
`sprintf(buff, LoadStr(IDS_EXECERROR), cmdLine, GetErrorText(err));` —
`cmdLine` is the very buffer passed to `SalShellExecuteEx` at `:936` (whose
own comment reads *"W launch: names may be outside the ACP (feature 004)"*)
and to `SalCreateProcess` at `:1000`, so it is UTF-8.

`src/mainwnd4.cpp:254` is the strongest of the group because it needs **no**
non-ASCII path at all:

```
sprintf(errorText, "%s\n\n%s", LoadStr(IDS_ERRORCREATINGTMPFILE), GetErrorText(err));
```

ASCII format, ANSI template, UTF-8 error text. On any localized Windows the
system message is non-ASCII, so on cs/de/fr/hu/sk the box always shows the
system error as mojibake when `SalGetTempFileName` fails.

**Languages affected**

| id | cs | de | fr | nl | hu | ro | sk | es |
|---|---|---|---|---|---|---|---|---|
| `IDS_ERROREXECVIEW` (10085) | ✗ | ✗ | ok | ok | ✗ | ok | ✗ | ok |
| `IDS_ERROREXECEDIT` (10086) | ✗ | ✗ | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_EXECERROR` (10071) | ✗ | ✗ | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_ERRORCREATINGTMPFILE` (12088) | ✗ | ✗ | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_TOOLONGNAME` (10036) | ✗ | ok | ✗ | ok | ✗ | ok | ✗ | ✗ |

P2 wrote "`IDS_ERROREXECVIEW` non-ASCII cs/de/hu/sk" — correct, French is
**not** affected for that one id (it is for `IDS_ERROREXECEDIT`). Dutch,
Romanian and English are unaffected throughout.

**Scope** — **display-only**. `SalCreateProcess`/`SalShellExecuteEx` already
received the true UTF-8 command line before the message is composed; the
composed buffer is never re-executed.

**Notes**

- `src/fileswn5.cpp:1145` / `:1461` mix an ANSI template with a UTF-8
  `expCommand` **and** a second ANSI argument `LoadStr(IDS_TOOLONGNAME)` —
  three-way mix, same outcome. In nl/ro both `LoadStr` halves are ASCII, so
  those two languages are clean even there.
- Latent hazard adjacent to the fix: `src/mainwnd4.cpp:940` and `:1007`
  truncate `cmdLine` with `strcpy(cmdLine + 2 * MAX_PATH - 4, "...")` at a raw
  byte offset. Today it is harmless (the buffer is already going down the ANSI
  path); after a `LoadStr` → `LoadStrU8` fix the cut could land mid-UTF-8
  sequence and re-break the wide path. Any fix must move the cut to a
  character boundary.

---

## F-P2-03 · CONFIRMED (both halves)

**Claim** — (a) Find's Errors/Info log garbles the system error text; (b) the
Find window caption garbles a non-ASCII search mask.

### (a) The Errors/Info log

**Scenario** — Czech UI on Czech Windows. Alt+F7, search `C:\` including
subdirectories; the sweep hits a directory it may not enumerate (a system
folder, an offline share, `System Volume Information`). `src/find.cpp:1701`:

```
sprintf(message, LoadStr(IDS_DIRERRORFORMAT), GetErrorText(err));
FIND_LOG_ITEM log; log.Flags = FLI_ERROR; log.Text = message;
```

→ `src/find.cpp:1706` `SendMessage(data->HWindow, WM_USER_ADDLOG, …)` →
`src/finddlg1.cpp:4495` → `src/finddlg2.cpp:1899`
`SalListViewSetItemTextU8(HListView, i, 1, buff);`. That sink is all-or-nothing
(`src/common/winlib.cpp:1202-1219`: `SalU8ToWAlloc` → `LVM_SETITEMTEXTW`, else
`LVM_SETITEMTEXTA` on the raw bytes). ANSI template + UTF-8 `GetErrorText` ⇒
**the Errors tab shows the Czech system error text as mojibake** while the
"Error reading directory:" prefix renders fine. The *Path* column beside it is
correct — it is set separately and wide (`src/finddlg2.cpp:1900-1906`
`SalU8ToWAlloc(item->Path)` → `LVITEMW`) — which makes the mismatch visible
side by side.

All five sites verified verbatim at the exact lines P2 gives:
`src/find.cpp:853, 905, 1434, 1701, 1743`.

**Languages affected**

| id | cs | de | fr | nl | hu | ro | sk | es |
|---|---|---|---|---|---|---|---|---|
| `IDS_ERROR_READING_FILE2` (10827) | ✗ | ok | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_ERROR_OPENING_FILE2` (10826) | ✗ | ✗ | ✗ | ok | ✗ | ok | ✗ | ok |
| `IDS_GETLINKTGTFILESIZEERROR` (10829) | ✗ | ✗ | ok | ok | ✗ | ok | ✗ | ✗ |
| `IDS_DIRERRORFORMAT` (10825) | ✗ | ok | ✗ | ok | ✗ | ok | ✗ | ok |

### (b) The window caption

**Scenario** — Czech, Hungarian or Slovak UI. Alt+F7, type a mask containing a
non-ASCII character (`*úče*`, `*Zálohy*`) and press Find. The title bar shows
the mask garbled.

The mask really is UTF-8: `src/finddlg1.cpp:1842`
`SalGetDlgItemTextU8(HWindow, IDC_FIND_NAMED, Data.NamedText, NAMED_TEXT_LEN);`
→ normalized into `named` (`:1924`) → `new CSearchForData(begin, named, …)`
(`:1976`) → `CMaskGroup::SetMasksString` (`src/find.cpp:1154`) →
`GetMasksString()`. The caption format itself is ASCII
(`src/finddlg1.cpp:24-25` `"(%d) %s [%s %s]"` / `"%s [%s %s]"`), so the only
ANSI contributors are the two `LoadStr` values — and either one being
non-ASCII forfeits the wide path for the whole caption at
`SalSetWindowTextU8` (`:2165`, `:2420`, `:3342`).

`IDS_FF_NAME` (10735) is non-ASCII in **cs** (`Vyhledat soubory a adresáře`)
and **sk** (`Vyhľadať súbory a adresáre`); `IDS_FF_NAMED` (10736) in
**cs / hu / sk**. Union ⇒ the caption defect is live in **cs, hu, sk** only;
de/fr/nl/ro/es and English are clean (there the buffer is homogeneous UTF-8
and the mask renders correctly). P2's split per id is accurate.

**Scope** — **display-only** for both halves. The log text is never re-parsed
and the caption is never read back; the search itself matches against the true
UTF-8 mask.

**Notes**

- `src/finddlg1.cpp:2416` (the minimized caption) also formats an `int` count
  through the same ASCII format — no extra exposure.
- Adjacent, *not* part of this finding and correct today:
  `src/finddlg2.cpp:1890` sets column 0 with the plain ANSI
  `ListView_SetItemText(HListView, i, 0, LoadStr(IDS_FINDLOG_ERROR…))` —
  homogeneous ANSI on its own control cell, so it renders correctly. It
  becomes a defect only if that cell ever receives UTF-8.

---

## F-P2-04 · CONFIRMED for the two path sites; the seven plugin-loading sites are LATENT

**Claim** — the safe-wait window mixes an ANSI template with a UTF-8 path /
plugin DLL name.

### Confirmed: "Reading path" / "Checking path"

**Scenario** — Czech (also de/fr/hu/sk) UI on Czech Windows. Enter a slow path
whose name contains a non-ASCII character — a network share
`\server\Zálohy`, a spun-down external disk `E:\Účetnictví`, a cloud folder
`G:\Můj disk` — so the listing takes over 2 s. `src/fileswn3.cpp:285`:

```
_snprintf_s(buf, _TRUNCATE, LoadStr(IDS_READINGPATHESC), GetPath());
CreateSafeWaitWindow(buf, NULL, 2000, TRUE, MainWindow->HWindow);
```

`GetPath()` is the UTF-8 panel path (feature 004). The wait window's painter is
all-or-nothing: `src/dialogs3.cpp:2735-2742`

```
WCHAR* textW = SalU8ToWAlloc(Text);
if (textW != NULL) { DrawTextW(…); free(textW); }
else DrawText(hDestDC, Text, (int)strlen(Text), &r, …);
```

⇒ the whole line drops to `DrawTextA` and **the user sees the escape prompt in
correct Czech with the path as mojibake**, for as long as the operation runs.
`src/salamdr5.cpp:396` is the identical shape (`LoadStr(IDS_CHECKINGPATHESC),
path`; `path` is the buffer handed to the UTF-8 facade `SalGetFileAttributes`
at `:333`/`:364`).

**Languages affected**: `IDS_READINGPATHESC` (12181) and `IDS_CHECKINGPATHESC`
(12180) are both non-ASCII in **cs, de, fr, hu, sk**; ASCII in nl, ro, es and
English. Matches P2.

### Latent: the seven plugin-loading sites in `plugins2.cpp`

`src/plugins2.cpp:3002, 3018, 3074, 3121, 3338, 3430, 3454` do compose
`"%s\n%s"` from `LoadStr(IDS_AUTOINSTALLPLUGINS | IDS_LOADINGPLUGINS)` (ANSI)
with `pluginName` / `Data[i]->DLLName` (UTF-8 plugin metadata, feature 052) —
verified verbatim, feeding the same `CWaitWindow::SetText` sink. But the second
half is **ASCII in every shipped configuration**:

- `pluginName` at `:3002`/`:3430` is produced by stripping the plugins-directory
  prefix from a `SearchForAddedSPLs` result (`src/plugins2.cpp:2993-2997`
  `memmove(pluginName, file + strlen(buf) + 1, …)`), i.e. a path **relative to
  `<install>\plugins`** — `zip\zip.spl`, `undelete\undelete.spl` … all ASCII
  for the 18 shipped plugins.
- `DLLName` is likewise stored relative when the plugin lives under `plugins\`
  (`src/plugins2.cpp:1374-1380`).

Reaching mojibake here needs a plugin the user added by hand from a non-ASCII
directory (Plugins Manager → Add) — not a shipped configuration. Classified
**latent**; it becomes live the moment F-P2-09's `DLLName` scenario is
possible, so the two should be fixed together.

**Languages affected (latent)**: `IDS_AUTOINSTALLPLUGINS` (12140) non-ASCII in
cs, **nl**, hu, sk; `IDS_LOADINGPLUGINS` (12148) in cs, hu, sk. P2 wrote
"`IDS_AUTOINSTALLPLUGINS` cs/nl/hu/sk" — correct, and note this is the one id
in the batch where **Dutch** is affected and German/French are not.

**Scope** — **display-only** throughout; the wait window has no read-back.

**Notes**

- `src/dialogs3.cpp:2742` passes `(int)strlen(Text)` as the character count to
  `DrawTextA` — byte count on a UTF-8 buffer. Harmless in the fallback (the
  bytes are what gets drawn), but it means a fixed site must not be left half
  converted.

---

## F-P2-05 · REFUTED as characterized · CONFIRMED (narrower, different class)

**What P2 claims** — "Copy/Move progress dialog subject (`CTruncatedString`)
mixes an ANSI title template with the UTF-8 file name … the progress dialog's
subject line shows the name as mojibake … `IDS_COPYDLGTITLE` cs/hu/sk,
`IDS_MOVEDLGTITLE` cs/fr/hu/sk."

### Refuted: the sink

`src/fileswn8.cpp:1017-1022` stores the composed `subject` into the operation
script:

```
script = new COperations(1000, 500, DupStr(subject), DupStr(GetPath()), DupStr(path));
```

i.e. into `COperations::WaitInQueueSubject` (`src/worker.cpp:509`,
`src/worker.h:280`). Grepping every use of that member across `src` gives
exactly **one** consumer: `src/dialogs.cpp:521`, inside
`CProgressDialog::SetDlgTitle`:

```
sprintf(buf, "(%s) %s", LoadStr(AutoPaused ? IDS_PROGDLGQUEUEPAUSED : IDS_PROGDLGPAUSED),
        AutoPaused && Script != NULL && Script->WaitInQueueSubject != NULL ? Script->WaitInQueueSubject : Caption);
…
SetWindowText(HWindow, buf);                      // src/dialogs.cpp:522 — plain ANSI
```

There is **no `CTruncatedString` and no `Sal*U8` sink** on this value. The
`CTruncatedString` P2 is thinking of is the *sibling* composition 500 lines
earlier in the same function — `src/fileswn8.cpp:489-490`
`sprintf(subject, LoadStrU8(resID), expanded); str.Set(subject, … formatedFileName);`
— which already uses **`LoadStrU8`** and is correct. The two `subject` buffers
share a name and a declaration but not a consumer.

Two consequences that change the finding materially:

1. **The ANSI template is not the operative cause.** The sink is unconditionally
   ANSI, so the UTF-8 file name is mojibake in **all eight shipped languages
   and in English** — the per-language table P2 gives is not the condition.
2. **The surface is not "the progress dialog".** `WaitInQueueSubject` is read
   only when `AutoPaused` is set, which happens only at
   `src/dialogs.cpp:650-656` — `Script->IsCopyOrMoveOperation &&
   OperationsQueue.AddOperation(HWindow, Script->StartOnIdle, &startPaused)`
   with `startPaused` TRUE. `StartOnIdle` defaults to FALSE
   (`src/fileswna.cpp:984`, `src/worker.cpp:491`) and is set only by the
   *"start only when nothing else is running"* checkbox in the Copy/Move
   **More options** page (`src/dialogs3.cpp:692` `ti.CheckBox(
   IDC_CM_STARTONIDLE, Criteria->StartOnIdle)`). The normal, unqueued
   progress dialog uses `Caption` (`LoadStr(IDS_COPY)` …) and renders
   correctly, and the From/To lines are separately and correctly fed the
   raw UTF-8 paths into `CStaticText` (`src/dialogs.cpp:666,669`
   `Source->SetTextToDblQuotesIfNeeded(Script->WaitInQueueFrom)`).

### Confirmed: a real defect at the same site, reclassified

**Scenario** — Any UI language, including English. Start an F5 copy of one
file with a non-ASCII name (`Účtenka 2026.pdf`) with **More options → "Start
only when nothing else is running"** ticked, while another copy/move is already
running. The new operation opens paused; its window title reads
`(Waiting in queue) Copying the file "ÃšÄtenka 2026.pdf" to …` — the name is
mojibake. In Czech the title is doubly broken: the composition also mixes
`LoadStrU8(IDS_QUESTION_FILE / IDS_QUESTION_DIRECTORY)`
(`src/fileswn8.cpp:443,450` — UTF-8, `adresář` in cs, `súbory` in sk) into an
ANSI `LoadStr` template, so even an ASCII name yields a garbled word.

**Defect class** — DC-06 (UTF-8 into an ANSI window-text API) at
`src/dialogs.cpp:522`, *plus* DC-18/DC-19 at `src/fileswn8.cpp:1017,1021`
(missed `LoadStr` twin of the already-converted `LoadStrU8` at `:489`).

**Languages affected** — **all**, English included, for the name half.
For the record, the template ids P2 lists check out: `IDS_COPYDLGTITLE` (13710)
non-ASCII in cs/hu/sk; `IDS_MOVEDLGTITLE` (13711) in cs/fr/hu/sk — they just
are not what makes the title break.

**Scope** — **display-only**. `WaitInQueueSubject` is a title string; the
operation runs off `script->Source/Target`, untouched.

**Notes**

- A fix must change **both** ends: `LoadStr` → `LoadStrU8` at
  `src/fileswn8.cpp:1017,1021` *and* `SetWindowText` → `SalSetWindowTextU8` at
  `src/dialogs.cpp:522`. Converting only the composition leaves the title
  garbled; converting only the sink leaves the cs/hu/sk mix. Note `buf[200]`
  at `src/dialogs.cpp:513` and the `::GetWindowText(HWindow, oldCaption, 200)`
  change-detection read at `:519` — the read must become
  `SalGetWindowTextU8` or the comparison will always differ and the title
  will be re-set on every progress tick.

---

## F-P2-06 · CONFIRMED in part · one bullet REFUTED · four bullets LATENT

This finding is a bag of eleven unrelated sites. Verified one by one; the
mechanism (ANSI `LoadStr` template + UTF-8 argument → `SalMessageBox` →
`src/msgbox.cpp:475` ANSI fallback) reproduces at every one of them, so the
*mechanism* is not in dispute. What differs is whether the UTF-8 half can
actually be non-ASCII in a shipped configuration.

### REFUTED — `src/drivelst.cpp:599-600` (network reconnect prompt)

```
if (!Windows7AndLater)
{
    _snprintf_s(captionBuf, _TRUNCATE, LoadStr(IDS_RECONNET_TITLE), remoteName);
    _snprintf_s(messageBuf, _TRUNCATE, LoadStr(IDS_RECONNET_TEXT), remoteName);
```

`Windows7AndLater = SalIsWindowsVersionOrGreater(6, 1, 0)`
(`src/salamdr1.cpp:3905`) — **TRUE on every platform Tandem Commander
supports** (Windows 11 baseline). This block is dead code; the strings are
never composed and the prompt is never shown. The same guard makes
`src/drivelst.cpp:455, 513, 523, 547, 718` dead too.

### CONFIRMED — four sites with a realistic shipped scenario

1. **`src/fileswn9.cpp:1936` — Copy UNC Name.**
   Function `CopyUNCPathToClipboard` (`src/fileswn9.cpp:1840`). Czech UI;
   focus a file on a local, non-shared path with a non-ASCII name
   (`D:\Zálohy\smlouva.pdf`) and invoke *Copy UNC Name*. Both share lookups
   fail (`:1864`, `:1919`), then
   `wsprintf(uncPath, LoadStr(IDS_CANNOT_CREATE_UNC_NAME), buff)` where
   `buff` is `path` + `name` (UTF-8 panel path + `CFileData::Name`) →
   `SalMessageBox`. The info box shows the path as mojibake.
   Template (10832) non-ASCII in **cs, fr, hu, sk**.

2. **`src/dialogs3.cpp:1293` — Drive Information → volume label.**
   `CDriveInfo::Validate` (`src/dialogs3.cpp:1261`). Czech UI; Ctrl+F1,
   type a new volume name, OK, and the rename fails (no admin rights,
   write-protected media, unsupported FS):
   `sprintf(buf, LoadStr(IDS_UNABLETOCHANGEDRIVELABEL), GetErrorText(err))`.
   Only the UTF-8 system error text is garbled. Template (10694) non-ASCII in
   **cs, de, fr, hu, sk**. Note this lands in the *same dialog* feature 067
   fixed — see F-P2-07.

3. **`src/viewer3.cpp:1499` — internal viewer, text not found.**
   `sprintf(buff, LoadStr(FindDialog.Regular ? IDS_FIND_NOREGEXPMATCH :
   IDS_FIND_NOMATCH), FindDialog.Text)`. `FindDialog.Text` is UTF-8 — proved:
   `CFindSetDialog::Transfer` (`src/viewer.cpp:333`) reads it through
   `HistoryComboBox` → `SalGetWindowTextU8(hwnd, Text, textLen); // feature
   005`. F3 a file, Ctrl+F, search `žluťoučký`, no match ⇒ the box shows the
   pattern garbled. `IDS_FIND_NOMATCH` (12353) non-ASCII in **cs, fr, hu, sk**;
   `IDS_FIND_NOREGEXPMATCH` (12354) in **cs, de, hu, sk, es**.

4. **`src/viewer3.cpp:1318` and `src/finddlg1.cpp:2094` — invalid regexp.**
   `sprintf(buf, LoadStr(IDS_INVALIDREGEXP), RegExp.GetPattern(),
   RegExp.GetLastErrorText())`. The pattern is UTF-8 by the same route
   (`Data.GrepText` via `HistoryComboBox` at `src/finddlg1.cpp:1817` and
   `SalGetDlgItemTextU8` at `:1846`). Alt+F7 → *Containing* → tick *Regular
   expressions* → type `[čž` → the error box shows the pattern garbled.
   `IDS_INVALIDREGEXP` (12345) non-ASCII in **cs, de, fr, hu, sk, es** — the
   broadest of the group.

### CONFIRMED mechanism, near-unreachable surface

5. **`src/icncache.cpp:1239, 1286` — `IDS_UNABLETOGETASSOC` + `GetErrorText`.**
   Composition verified; template (10034) non-ASCII in cs, hu, sk. But the
   branch fires only on a `RegEnumKeyEx` failure other than
   `ERROR_NO_MORE_ITEMS`, and the in-code comment documents this as a single
   historical report on Windows Server 2003. Real, but not something a user
   will meet.

6. **`src/codetbl.cpp:180, 218` — `IDS_VIEWERERROPENFILE`.**
   The mix is real and is a textbook DC-18 twin: `ReadTable`
   (`src/codetbl.cpp:17-51`) returns either `GetErrorText(GetLastError())`
   (**UTF-8**) or `LoadStr(IDS_VIEWERBADFILESIZE)` (ANSI), and the sibling
   `src/codetbl.cpp:364` in the same file already uses
   `LoadStrU8(IDS_VIEWERERROPENCODES)`. Template (12048) non-ASCII in cs, de,
   hu, sk. But the branch needs a `convert\<lang>\convert.cfg` that references
   a missing or wrong-sized code-table file — a damaged or hand-edited
   installation. P2's `:204` / `:230` (`convertCfgFileName`) compose an
   install-directory path and are ASCII in every default install ⇒ latent
   only. `text` then reaches `SalMessageBox` at `src/codetbl.cpp:290`.

### LATENT — the UTF-8 half is ASCII in every shipped configuration

7. **`src/plugins1.cpp:2298, 2465, 2479`.** The DC-18 observation is exactly
   right — `:2299-2301` and `:2466-2468` immediately below each site use
   `LoadStrU8(IDS_OLDPLUGINVERSION / IDS_PLUGININVALID)` for the *named*
   branch with the comment *"plugin metadata is UTF-8 (feature 052)"*, and the
   unnamed branch was left on `LoadStr`. But `s` is only the **base file
   name**: `src/plugins1.cpp:2162-2170` `char* s = DLLName; … s = strrchr(buf,
   '\') + 1;`. All 18 shipped `.spl` files have ASCII names, so the
   composition is homogeneous ANSI today. Live only for a hand-added plugin
   whose *file name* carries non-ASCII characters.

8. **`src/callstk.cpp:581` — icon-overlay crash box.** `iconOvrlsHanName` is
   `CShellIconOverlayItem::IconOverlayName`, i.e. a **registry subkey name**
   under `…\Explorer\ShellIconOverlayIdentifiers` (`src/shiconov.cpp:498`
   `lstrcpyn(item2->IconOverlayName, keyNames[s], MAX_PATH)`). Those are
   programmatic identifiers (`OneDrive1`, ` DropboxExt01`, `TortoiseNormal`)
   — ASCII in practice — and the box appears only when such a handler *faults
   inside our process*. Two independent obstacles ⇒ latent.

9. **`src/salamdr2.cpp:898, 949, 951` — `$(var)` / `$[ENVVAR]` diagnostics.**
   The extracted name is UTF-8 (it is cut out of the user-menu / hot-path
   command string, which the config facade delivers as UTF-8) and the
   templates (10532/10533/10534) are non-ASCII in cs, hu, sk. But the name
   shown is one the *user typed*; Salamander's own `$(…)` set and Windows
   environment variable names are ASCII, so producing a non-ASCII one requires
   deliberately typing accented characters inside `$( )` or `$[ ]`. Latent
   for practical purposes. Note also `src/salamdr2.cpp:948`
   `GetEnvironmentVariable(envVar, buf, MAX_PATH)` is the **ANSI** entry point
   — a separate DC-01 concern that belongs to P1/P4, not here.

**Scope** — every site in this finding is **display-only**: all eleven feed a
message box and nothing reads the composed buffer back.

**Note on the finding's title** — "send-by-email" appears in the headline but
no MAPI site is listed among the bullets; the closest is the *Copy UNC Name*
box (bullet 1). The title should be corrected.

---

## F-P2-07 · CONFIRMED for the junction/symlink row only · UNC and SUBST rows REFUTED

**What P2 claims** — `src/dialogs3.cpp:1635` (`IDS_INFODLGTYPE8` +
`remoteName` + `userName`), `:1659` (`IDS_INFODLGTYPE7` + the SUBST target),
`:1665` (`IDS_INFODLGTYPE9/10` + `junctionOrSymlinkTgt`) all append UTF-8
paths onto an ANSI `volumeName` that is then probed as a whole at `:1668`.

### Refuted — the UNC row (`IDS_INFODLGTYPE8`, `src/dialogs3.cpp:1635`)

`remoteName` and `userName` are **not UTF-8**. `src/dialogs3.cpp:1614-1616`:

```
remoteNameValid = (WNetGetConnection(buff, remoteName, &l) == NO_ERROR);
l = 100;
userNameValid = (WNetGetUser(buff, userName, &l) == NO_ERROR);
```

The core is built without `UNICODE`, so these are `WNetGetConnectionA` /
`WNetGetUserA` — the values are **ACP bytes**. Composed with the ANSI
`LoadStr(IDS_INFODLGTYPE3)` prefix and the ANSI `IDS_INFODLGTYPE8` template,
the buffer is homogeneous ACP; `SalU8ToWAlloc` at `:1668` fails and the ANSI
fallback at `:1675` renders it **correctly**. Nothing is garbled. (A share name
the ACP cannot represent is lost inside `WNetGetConnectionA` — a genuine but
*different* defect, DC-02/DC-01, belonging to P1's queue, not a mixed
composition.)

### Refuted — the SUBST row (`IDS_INFODLGTYPE7`, `src/dialogs3.cpp:1659`)

`buff` comes from `GetSubstInformation(drive - 'A', buff, 300)`, which is
`MyQueryDosDevice` → `src/salamdr2.cpp:1781` `return QueryDosDevice(deviceName,
target, MAX_PATH);` — again the **A** entry point, ACP bytes. Same
conclusion: homogeneous ACP, renders correctly. P2 called this row "correct
today and only latent" but for the wrong reason (template ASCII); the real
reason is that the *argument* is ACP, not UTF-8.

### Confirmed — the junction/symlink row (`src/dialogs3.cpp:1663-1666`)

`junctionOrSymlinkTgt` **is** UTF-8, and the chain is provable:
`src/dialogs3.cpp:1317` `MyGetVolumeInformation(VolumePath,
volumePathWithBackslash, junctionOrSymlinkTgt, &linkType, …)` →
`src/salamdr2.cpp:1438` `ResolveLocalPathWithReparsePoints(…,
junctionOrSymlinkTgt, linkType, NULL)` → `src/salamdr2.cpp:1332-1339`
`GetReparsePointDestination(repPointPath, repPointPath, MAX_PATH, …);
lstrcpyn(junctionOrSymlinkTgt, repPointPath, MAX_PATH)` →
`src/salamdr2.cpp:1653-1654` `if (SalWToU8(s, -1, repPointDstBuf,
repPointDstBufSize) == 0 && WideCharToMultiByte(CP_ACP, …) == 0)` — UTF-8 with
an ACP fallback only when the UTF-8 will not fit.

**Scenario** — Czech UI. Create (or inherit) a junction, e.g.
`mklink /J C:\Data D:\Zálohy\Projekty`, put the panel on `C:\Data\…` and press
**Ctrl+F1** (Drive Information). `volumeName` is built as
`LoadStr(IDS_INFODLGTYPE2)` ("Pevný disk") + `" "` +
`sprintf(… LoadStr(IDS_INFODLGTYPE9), junctionOrSymlinkTgt)`. The buffer now
holds CP1250 *and* UTF-8 bytes → `SalU8ToWAlloc(volumeName)` at `:1668`
returns NULL → `SetWindowText` (A) at `:1675`. The Type line reads
`Pevný disk (junction to "D:\ZÃ¡lohy\Projekty")`.

**Languages affected — broader than P2 states.** P2 lists only
`IDS_INFODLGTYPE9` (cs/fr/hu). But the ANSI contamination does not need the
append template: `volumeName` **always** starts with one of
`IDS_INFODLGTYPE1…6`, and the drive carrying a junction is `DRIVE_FIXED`, so
the prefix is `IDS_INFODLGTYPE2` (10027), non-ASCII in **cs, hu, ro, sk**.
Union with `IDS_INFODLGTYPE9` (10952: cs, fr, hu) ⇒ the junction line is
broken in **cs, fr, hu, ro, sk**. (`IDS_INFODLGTYPE10`, 10953, is ASCII in all
eight; `IDS_INFODLGTYPE1`=cs/hu/ro/sk/es, `:3`=cs/fr/hu/ro/sk, `:4`=cs/sk,
`:5`=hu, `:6`=cs/hu/sk — all of them ANSI-taint the buffer the same way.)
This also means a fix must convert the **whole** `switch` block at
`src/dialogs3.cpp:1621-1648`, not just the three append sites.

**Scope** — **display-only**; the Type line is never read back.

**Notes**

- P2's "missed twin inside feature 067's own dialog" framing is correct and
  verified: `src/dialogs3.cpp:1540-1549` carries an explicit feature-067
  comment and six `SalSetDlgItemTextU8(…, PrintDiskSize(…, TRUE))` calls in
  the same `WM_INITDIALOG`, while the Type line 120 lines below kept the
  `LoadStr` + `SalU8ToWAlloc`-with-ANSI-fallback shape.
- `src/dialogs3.cpp:1293` (`IDS_UNABLETOCHANGEDRIVELABEL`) is a *second*
  residual site in the same dialog — see F-P2-06 bullet 2. The two should be
  fixed together.

---

## F-P2-09 · CONFIRMED for the Location column (narrow) · LATENT for the Version column

**Claim** — feature 052 converted only the Name column of the Plugin Manager
list; Location and Version stayed on the ANSI `ListView_SetItemText`.

**The asymmetry is real and verified verbatim** — `CPlugins::AddNamesToListView`,
`src/plugins2.cpp:1046-1056`:

```
// plugin name is UTF-8 (encoding contract, plugins.h/feature 052); the
// ANSI ListView_SetItemText rendered the cached name of a not-loaded
// plugin as mojibake
SalListViewSetItemTextU8(hListView, i, 0, plugin->Name);
ListView_SetItemText(hListView, i, 1, LoadStr(plugin->GetLoaded() ? … ));   // ANSI, LoadStr — correct
ListView_SetItemText(hListView, i, 2, plugin->Version);                     // ANSI
ListView_SetItemText(hListView, i, 3, plugin->DLLName);                     // ANSI
```

### Location column — CONFIRMED, with a sharper mechanism than P2 gives

P2 says the value arrives UTF-8 from the registry facade. That is right, but
only *after a restart* — and the round trip is what makes it non-ASCII:

1. Plugins Manager → **Add** picks the `.spl` through
   `SafeGetOpenFileName(&ofn)` (`src/dialogs5.cpp:749`) →
   `src/salamdr6.cpp:1701` `GetOpenFileName(lpofn)` = `GetOpenFileNameA`, so
   `oneName` comes back as **ACP** bytes.
2. `pluginName` keeps the absolute path when it is not under
   `<install>\plugins` (`src/dialogs5.cpp:777-783`, and the same rule on the
   registry load path at `src/plugins2.cpp:1374-1380`) →
   `CPluginData::DLLName` = ACP. In *this* session the Location column is ACP
   text in an ANSI cell, so it still renders **correctly**.
3. On save, `SalRegSetValueExW8` (`src/salamdr6.cpp:2416-2440`) probes the
   value with `SalU8ToW`, fails on the ACP bytes and takes the documented
   "transitional tolerance" branch — `MultiByteToWideChar(CP_ACP, …)` +
   `RegSetValueExW` — so the registry stores the **correct UTF-16** path.
4. Next start, `SalRegQueryValueExW8` (`:2352-2378`) reads wide and converts
   with `SalWToU8`, so `DLLName` is now genuine **UTF-8**, and
   `src/plugins2.cpp:1056` draws it through the ANSI call.

**Scenario** — any UI language. Add a plugin from `D:\Můj plugin\thing.spl`,
restart Tandem Commander, open Plugins → Plugin Manager: the Name column is
right and the Location column beside it reads `D:\MÅ¯j plugin\thing.spl`.
**Narrow**: all 18 shipped plugins live under `<install>\plugins\` and are
stored with an ASCII *relative* name, so this needs a hand-added out-of-tree
plugin (a supported action, not a shipped default).

### Version column — LATENT

`CPluginData::Version` *is* UTF-8 by contract (`src/plugins1.cpp:1611`
`s = SalLegacyToU8Alloc(version, MAX_PATH - 1);`), so the sink is wrong in
principle. But every shipped plugin passes a compile-time literal —
`VERSINFO_VERSION_NO_PLATFORM` / `VERSINFO_VERSION` from
`src/plugins/shared/spl_vers.h:44,47`, expanded from numeric macros — which is
ASCII by construction. Only a third-party plugin passing a localized version
string could make this visible.

**Scope** — **display-only**; the list cells are never read back (the value
actioned on Remove/Load is `plugin->DLLName` itself, not the cell text).

**Note** — P2 attributes the miss to the guard's `UTF8_IDENT` tracking
`plugin->Name` but not `DLLName`/`Version`. Plausible, but I did not verify it
against `tools/check_encoding.py`; that belongs to P7.

---

## F-P2-10 · CONFIRMED (languages narrower than claimed)

**Claim** — one of the two Plugin Manager checkbox captions is read with the
ANSI `GetDlgItemText` and then composed with a UTF-8 plugin FS item name.

**Verified verbatim.** `src/dialogs5.cpp:490-496`:

```
// read as UTF-8: the label is later composed with the UTF-8 plugin name
// (feature 052; the composed text goes to SalSetWindowTextU8)
SalGetDlgItemTextU8(HWindow, IDC_PLUGINSHOWINBAR, ShowInBarText, 200);

// copy the Show In Change Drive Menu checkbox text into our buffer
GetDlgItemText(HWindow, IDC_PLUGINSHOWINCHDRV, ShowInChDrvText, 200);
```

The comment on the converted line even states the reason, and the twin two
lines below was left on the ANSI read. Both buffers are then used identically:
`src/dialogs5.cpp:326-327` `sprintf(buff, ShowInBarText, pluginName);
SalSetWindowTextU8(showInBar, buff);` and `:353-354` `sprintf(buff,
ShowInChDrvText, itemText); SalSetWindowTextU8(showInChDrv, buff);`.

`itemText` is `p->ChDrvMenuFSItemName` after the tab-stripping at `:332-347`.
That member is UTF-8 on both of its paths: the live intake
`src/plugins1.cpp:1244` `p->ChDrvMenuFSItemName = SalLegacyToU8Alloc(title,
MAX_PATH - 1);` and the cached registry load `src/plugins2.cpp:1430-1432`
`GetValue(itemKey, SALAMANDER_PLUGINS_FSCMDNAME, REG_SZ, fsCmdName, MAX_PATH)`
→ `DupStr(fsCmdName)`. Because it is persisted, the plugin need not be loaded
in this session.

**Scenario** — Czech UI. Plugins → Plugin Manager → select **UnDelete**
(`undelete=on`). The checkbox should read
`Zobrazit položku Obnovení souborů a adresářů v nabídce Změnit jednotku a v
panelu jednotek`; the caption arrives as CP1250 from `GetDlgItemTextA`, the
item name as UTF-8, so `SalU8ToWAlloc` inside `SalSetWindowTextU8`
(`src/common/winlib.cpp:1104`) returns NULL and the **whole label** is drawn
ANSI — the item name appears as `ObnovenÃ­ souborÅ¯ a adresÃ¡Å™Å¯`.

**Languages affected — narrower than P2 claims.** The defect needs *both*
halves non-ASCII (an ASCII caption leaves a valid-UTF-8 buffer; an ASCII item
name leaves a homogeneous-ACP buffer — both render correctly):

| | caption `IDC_PLUGINSHOWINCHDRV` (2621) | UnDelete item (`undelete.slt` 102) | broken? |
|---|---|---|---|
| cs | non-ASCII | non-ASCII | **yes** |
| de | non-ASCII | ASCII | no |
| fr | ASCII | non-ASCII | no |
| nl | ASCII | ASCII | no |
| hu | non-ASCII | non-ASCII | **yes** |
| ro | ASCII | ASCII | no |
| sk | non-ASCII | non-ASCII | **yes** |
| es | non-ASCII | ASCII | no |

So the live set is **cs, hu, sk** — not the "cs/de/hu/sk/es" P2 states (that is
the caption's language list, not the defect's). Second shipped carrier:
**regedt** (`regedt.slt` 1002) is non-ASCII only in **hu**, already in the set.
`folders` (1004), `ftp` (10030) and `sftp` are ASCII in all eight.

**Scope** — **display-only**. `ShowInChDrvText` is a format template read once
at `WM_INITDIALOG`; the checkbox *state* is stored separately, so nothing
operational is derived from the composed label.

**Note** — P2's one-character fix (`GetDlgItemText` → `SalGetDlgItemTextU8` at
`src/dialogs5.cpp:495`) is correct and complete for this site. P2's "Related
(not defective)" note on `:498` `InstalledPluginsText` also checks out:
`src/dialogs5.cpp:107` composes it with two `int` counts only — homogeneous
ANSI, and `SalSetWindowTextU8` at `:109` preserves the bytes through its
fallback.

---

## F-P2-11 · CONFIRMED (not "any UI language" — six of the eight)

**Claim** — the plugin *Keyboard Shortcuts* dialog lists command names through
the ANSI list-view call.

**Verified.** `CPluginKeys::RefreshListView` (`src/dialogs5.cpp:1030`):
`:1067` `lstrcpyn(buff, item->Name, 500);` … `:1086`
`ListView_SetItemText(HListView, row, 0, buff);` — the ANSI macro, on an ANSI
dialog (`CCommonDialog(HLanguage, IDD_PLUGINKEYS, …)`, no `unicodeWnd`).

`CPluginMenuItem::Name` **is** UTF-8, by the feature-052 contract, with the
comment spelling it out — `src/plugins1.cpp:1915-1919`:

```
// feature 052: normalized to UTF-8 (encoding contract on CPluginData in
// plugins.h) - covers both the plugin intake (ANSI) and the registry
// load (already UTF-8, kept unchanged)
Name = SalLegacyToU8Alloc(name);
```

**Scenario** — Czech UI. Plugins → Plugin Manager → select **Checksum** (or
UnDelete, Renamer, File Comparator) → **Keyboard Shortcuts**. The Command
column should read `Ověřit kontrolní součty` / `Spočítat kontrolní součty`;
every row renders as mojibake, while the *Shortcut* column (from
`GetHotKeyText`, `:1088-1089`) is fine. The plugin is loaded on the spot by
`p->InitDLL(HWindow)` (`src/dialogs5.cpp:688`), so no prior use is needed.

**Languages affected** — the plugin's own `.slg` follows the UI language, so
the set is where shipped plugins' menu strings are non-ASCII: checked
`checksum` ids 65/66 (cs de fr hu sk es / cs de hu sk es), `undelete` 65/67
(cs de fr hu sk), `filecomp` 1002 (hu sk). Union ⇒ **cs, de, fr, hu, sk, es**.
P2's "in any UI language" is wrong: in **English, Dutch and Romanian** these
strings are ASCII and the list renders correctly. That matters — the defect is
not universal, and the fix is byte-identical for those three.

**Scope** — **display-only**. The row identity is carried in `lvi.lParam = i`
(`src/dialogs5.cpp:1063`), not in the text, so hot-key assignment is unaffected.

**Note** — P2 cites `src/plugins2.cpp:1051` as "the sibling list in the same
file"; it is in a different file (`plugins2.cpp` vs `dialogs5.cpp`), but the
point — a converted sibling list view — stands.

---

## F-P2-12 · CONFIRMED — and the "actioned" half is worse than P2 states

**Claim** — the Custom Packers / Custom Unpackers / Viewers-Editors config
pages put UTF-8 command paths through ANSI `WM_SETTEXT`/`WM_GETTEXT`, and the
re-read value is actioned, not merely displayed.

### The pages really are ANSI

`CCfgPagePackers` / `CCfgPageUnpackers` / `CCfgPageViewers` are
`CCommonPropSheetPage`s, created through `src/common/sheets.cpp:277`
`return CreatePropertySheetPage(&psp);` and `:499` `INT_PTR ret =
PropertySheet(&psh);` — un-suffixed, i.e. `CreatePropertySheetPageA` /
`PropertySheetA` in a core built without `UNICODE`. No page sets
`unicodeWnd`. So every child control is an ANSI window and every
`SendDlgItemMessage(…, WM_SETTEXT/WM_GETTEXT, …)` is the ANSI path.

### The value really is UTF-8

`src/packers.cpp:909-919` `ret &= GetValue(hKey, SALAMANDER_CP_EXECCOPY,
REG_SZ, execcopy, max);` → `GetValueAux` (`src/regwork.cpp`) →
`SalRegQueryValueExW8(hKey, name, &gettedType, (BYTE*)buffer, &bufferSize)` →
`RegQueryValueExW` + `SalWToU8` (`src/salamdr6.cpp:2352-2378`). **UTF-8.**

### 22 sites, all verified verbatim

`src/dialogsp.cpp` — Custom Packers `:130, 156, 159, 166, 168` (set) and
`:189, 225, 226, 230, 231` (get); Custom Unpackers `:578, 604, 606` (set) and
`:627, 658, 659` (get); Viewers/Editors `:947, 950` (set) and `:969, 972,
1043` (get) + `:1046` (set). Count matches P2's 22.

### Display — CONFIRMED

**Scenario** — any UI language, English included (no `LoadStr` is involved).
Configure a custom packer whose executable lives under a non-ASCII path, e.g.
`D:\Programy\Sedmička\7z.exe`. Options → Packers/Unpackers → *Custom Packers*,
select the entry: `src/dialogsp.cpp:156` writes the UTF-8 bytes into an ANSI
edit, which interprets them through the ACP ⇒ the Command field reads
`D:\Programy\SedmiÄ\u008dka\7z.exe`. Same for the Arguments fields, for Custom
Unpackers (`:604`), and for the viewer/editor command fields (`:947, 950`).

### Actioned — CONFIRMED, via a mechanism P2 does not name

P2 says only that "any edit re-encodes through the ACP". The decisive fact is
that the consumer is **strict**: `src/common/salfileio.cpp` `SalCreateProcess`
converts with `SalU8ToWAlloc` and, on failure,

```
SetLastError(ERROR_INVALID_NAME);
return FALSE;
```

— **there is no ANSI fallback**. The chain is
`CPackerConfig::ExecutePacker` (`src/packers.cpp:799` / `:810`
`sprintf(command, "%s %s", data->CmdExecCopy, data->CmdArgsCopy);`) →
`PackUniversalCompress` (`src/packers.cpp:813`) → `src/pack1.cpp:709`
`SalCreateProcess(NULL, cmdLine, …)`.

Two ways an **ACP** command gets into the config, both through shipped UI:

1. **Browse.** The `…_BROWSE` buttons run `TrackExecuteMenu`
   (`src/dialogsp.cpp:333, 340, 347, 354, …`) → `src/execute.cpp:2047`
   `BrowseCommand(hParent, editlineResID, filterResID)` →
   `src/execute.cpp:2152` `if (GetOpenFileName(&ofn))` — the **A** common
   dialog — then `:2157` `SendMessage(GetDlgItem(hParent, editlineResID),
   WM_SETTEXT, 0, (LPARAM)file)`. Browsing to `D:\Programy\Sedmička\7z.exe`
   deposits CP1250 bytes.
2. **Typing.** The user "corrects" the mojibake by retyping the accented path;
   `WM_GETTEXT` at `src/dialogsp.cpp:225` returns ACP.

`StoreControls` then stores those ACP bytes into `Config`, and the next F5-to-
archive with that custom packer fails at `SalCreateProcess` with
`ERROR_INVALID_NAME` — the packer never starts. So: **stored / actioned**, not
display-only.

Two refinements worth recording, because they bound the damage:

- The *untouched* round trip is usually lossless, as P2 says, but not by
  construction: the ANSI edit stores WM_SETTEXT bytes via `CP_ACP` and gives
  them back the same way, so any byte the ACP leaves undefined is destroyed.
  On CP1250 those are `0x81 0x83 0x88 0x90 0x98`; Czech/Slovak/Hungarian
  characters happen to encode clear of them in UTF-8, but e.g. CJK does not
  (`E3 81 82` for あ contains `0x81`), so a CJK path in the field is
  irreversibly replaced by `?` on the next `StoreControls`.
- The corruption **self-heals across a restart** for the ACP-representable
  case: `SalRegSetValueExW8`'s "transitional tolerance" branch
  (`src/salamdr6.cpp:2431-2440`) converts the ACP bytes with
  `MultiByteToWideChar(CP_ACP, …)` and stores correct UTF-16, which comes back
  as UTF-8. So the launch failure is *session-scoped* — and then the field
  displays as mojibake again, inviting the same edit. It is a stable loop, not
  a one-off.

**Languages affected** — **all eight plus English**: no `LoadStr` participates,
the defect is purely sink-side.

**Scope** — **display + stored/actioned** (the only finding in this batch that
reaches an operational value).

**Note** — `src/dialogsp.cpp:1043,1046` is an internal copy (read
`IDC_P3_VIEW`, write `IDC_P3_EDIT`) — ANSI on both ends, so it is
byte-transparent today and only needs converting together with the rest.
P2's suggested fix (`SalSetDlgItemTextU8` / `SalGetDlgItemTextU8`) is right for
the 22 sites, but it is **not sufficient**: `BrowseCommand`
(`src/execute.cpp:2126-2160`, ANSI `GetOpenFileName` + ANSI `WM_GETTEXT`/
`WM_SETTEXT`) must be converted in the same change, or Browse will keep
injecting ACP into a now-UTF-8 field. `src/dialogs2.cpp:1271` and
`src/dialogs3.cpp:2446` are its other two callers and must be re-checked too.

---

## F-P2-13 · CONFIRMED — the L13 suppression premise is stale

**Claim** — the Save Configuration "export file exists" box composes an ANSI
template with a UTF-8 configuration path; the standing suppression at
`src/mainwnd3.cpp:2842` rests on a false premise.

**The suppression is provably wrong on its own terms.** `src/mainwnd3.cpp:2839-2856`:

```
if (FileExists(ConfigurationName))
{
    char buff[3000];
    // encoding-check: allow mixed-composition - configuration name chosen in-app, not a file name
    //   (feature 042, FR-010)
    _snprintf_s(buff, _TRUNCATE, LoadStr(IDS_SAVECFG_EXPFILEEXISTS), ConfigurationName);
    int ret = SalMessageBox(HWindow, buff, LoadStr(IDS_INFOTITLE), MB_ICONINFORMATION | MB_OKCANCEL);
    …
        char* s = strrchr(ConfigurationName, '\\');
        …
        SendMessage(activePanel->HWindow, WM_USER_FOCUSFILE, (WPARAM)(s + 1), (LPARAM)path);
```

The same seventeen lines call `FileExists` on it, split it at the last
backslash and hand the directory + file name to the panel for focusing. It is
a **file path**, not an in-app label. The comment must be corrected regardless
of the fix.

**And the path can be UTF-8.** `src/salamdr1.cpp:3578-3605`: the command line
is taken from `GetCommandLineW()` and converted with `SalWToU8Alloc(argsW,
-1)` into `cmdLineU8`, which is what `GetCmdLine(buf, _countof(buf), argv, p,
cmdLine)` tokenizes — so **every `argv[i]` is UTF-8**. The `-C` handler then
does `src/salamdr1.cpp:3665` `lstrcpyn(ConfigurationName, argv[i + 1],
MAX_PATH);` for a full path. (The other two producers,
`GetModuleFileName` (A) and `GetOurPathInRoamingAPPDATA`, yield ACP — so the
member's encoding is genuinely *undefined*, a DC-17 shape, exactly as P2
says.)

**Scenario** — Czech UI. Start `tandemcommander.exe -C D:\Zálohy\config.reg`,
work, then *Options → Save Configuration* while that file already exists. The
info box shows `D:\ZÃ¡lohy\config.reg`; the OK/Cancel prompt asks the user to
confirm overwriting a file whose name they cannot read. Pressing Cancel then
navigates the panel to the *correct* directory (that half uses the raw UTF-8
value), which makes the garbled text in the box demonstrably a display defect
rather than a broken path.

**Languages affected** — `IDS_SAVECFG_EXPFILEEXISTS` (10083) is non-ASCII in
**cs, de, fr, nl, hu, sk, es** — 7 of the 8 shipped languages; only **ro**
(and English) are clean. P2's list is exactly right, and this is the widest
language exposure in the batch.

**Scope** — **display-only**. `FileExists`, the `strrchr` split and
`WM_USER_FOCUSFILE` all consume `ConfigurationName` itself, never the composed
`buff`.

**Note** — the ACP-producer half of the DC-17 shape is a separate, real
concern: when `ConfigurationName` comes from `GetModuleFileName` (A) under a
non-ASCII install directory the value is ACP and would then be a lossy input
to the UTF-8 facades. That belongs to P1's `GetModuleFileName` seed (C-c), not
to this finding, but a fix that converts `:2844` to `LoadStrU8` without
settling the producer's encoding would only be correct for the `-C` case.

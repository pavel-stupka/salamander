# Regression review — X08 (F-P5-08) and X09 (F-P5-09)

**Reviewer**: independent regression reviewer (did not write these fixes).
**Charter**: `charters.md` § "Regression reviewer" — find a regression.
**Baseline**: `c577ff3`. **Diff reviewed**: `git diff -- src/plugins/zip/dialogs.cpp
src/plugins/filecomp/mainwnd.cpp` (11 and 23 changed lines respectively).
**Read-only on the product** — nothing under `src/` was modified by this review.

## Verdicts (summary)

| Fix | Verdict | Reason |
|---|---|---|
| **X08** — `src/plugins/zip/dialogs.cpp` ×2 | **REJECTED** | Introduces a *drop-the-text* path: for a WTF-8 (unpaired-surrogate) archive path — the case feature 066 explicitly ships support for — `SetDlgItemTextU8` returns FALSE and **sets nothing**, so the overwrite prompt now asks "File already exists on the disk. Overwrite?" with **no file name at all**, where it previously showed a mojibake but largely readable path. This is the same defect class as F-P5-09, which the sibling fix in this very batch treats as worth fixing. Also: the recorded partial-fix disclosure is factually wrong (see below). |
| **X09** — `src/plugins/filecomp/mainwnd.cpp` ×2 | **ACCEPTED with a record defect** | Both changed sites are correct: no leak, no changed behavior on any input that worked before, English/ASCII byte-identical, and the cs/fr/hu/sk blank title is genuinely corrected. **But** a third, byte-for-byte identical `: L""` site — `mainwnd.cpp:893-895`, the "Computing Differences" title — was **missed**; it still blanks the same window's title in 5 of the 8 enabled languages (cs, fr, hu, sk **and de** — German is affected only by the missed site, and is not even named in the recorded manual check). The affected-surface list in `review-report.md` §7.1 ("filecomp window title") therefore does not match the code. Fix accepted; **record incomplete**. |

**X08 subclass claim**: **REFUTED** (conclusion right, both halves of the stated
mechanism wrong) — see §1.3, verified empirically on a cs-CZ / CP1250 machine.

---

## 0. Scope containment

`git status --porcelain` shows 13 modified files; 11 of them belong to the
X01–X07 batches. Within the X08/X09 batch **exactly two files changed**, both
plugin-internal:

- `src/plugins/zip/dialogs.cpp` (2 statements + comments)
- `src/plugins/filecomp/mainwnd.cpp` (2 statements + comments)

Nothing else in either plugin, and **nothing outside these two plugins**,
changed. Specifically unchanged: `src/plugins/shared/*.h` (incl. `splunicode.h`,
`spl_vers.h` → `LAST_VERSION_OF_SALAMANDER` untouched), `src/zip.cpp`
(`CSalamanderGeneral` forwarders), `src/plugins/zip/common.cpp`
(`SetDlgItemTextU8` itself is unmodified — the change only adds two callers),
and all resource/translation files. **Plugin-facing services are byte-identical.**

**Build**: `build.cmd full` (Debug x64) re-run by me → BUILD SUCCEEDED, tree up
to date; the fixer's own log
(`scratchpad/build-fix-x08.log:763,934,959,1066,1262,1270`) shows both
`zip/dialogs.cpp` and `filecomp/mainwnd.cpp` were actually recompiled and both
`.spl` re-linked, with **no warnings** attributable to either file.

**Per-item path?** Neither change sits on a per-item path (listing, sorting,
icon reading, per-name conversion). X08 runs once per modal overwrite prompt;
X09's `WN_SET_PROGRESS` is throttled to ≥500 ms (`worker2.cpp:139-145`) and the
success branch is instruction-for-instruction what it was. **No timing numbers
are required and none are missing.**

**Empirical apparatus.** Two probe programs were compiled with the same
toolchain (MSVC v143, no `UNICODE`/`_UNICODE`) and run on this machine
(`ACP = 1250`, user locale `cs-CZ`) — i.e. exactly the shipped Czech
configuration the findings depend on. Sources:
`scratchpad/probe.c` (dialog/static Unicode-ness and text round trips) and
`scratchpad/loc.c` (locale separators, WTF-8 rejection). Raw output is quoted
inline below. Neither program touches the product.

---

## 1. X08 — `src/plugins/zip/dialogs.cpp`, `COverwriteDialog::OnInit` (1839-1845) and `COverwriteDialog2::OnInit` (1929-1932)

```
-    SendDlgItemMessage(Dlg, IDC_FILE, WM_SETTEXT, 0, (LPARAM)File);
+    SetDlgItemTextU8(Dlg, IDC_FILE, File);
```
(`IDC_FILEATTR` deliberately left on the ANSI `WM_SETTEXT` at both sites.)

### 1.1 Q1 — Is `File` really UTF-8 at both call sites? **YES, at both.**

I re-enumerated the callers myself rather than trusting the record; there are
exactly two live ones (`repair.cpp:82` is inside the `/* … */` block that spans
`repair.cpp:5-346` — dead code, not compiled).

**Site A — `COverwriteDialog` ← `OverwriteDialog()` ← `add.cpp:2346`, argument `TempName`:**
- `add_del.h:87` — `char* TempName; //UTF-8, U8_MAX_PATH bytes (heap - long paths)`
- produced at `add.cpp:2185` by `MakeFileName(DiskNum + 1, Options.SeqNames, ZipName, TempName, …)`,
  i.e. a byte-wise derivation (`memcpy` of the stem + an ASCII `.z%02d` / `_%02d`
  volume suffix, `common.cpp:2485-2516`) of `ZipName`
- `common.h:194` — `char* ZipName; //name of zip file, UTF-8, U8_MAX_PATH bytes`
- consumed as UTF-8 by the plugin's own UTF-8 file facade in the same function:
  `add.cpp:2315` `CreateFileU8(TempName, …)`, `add.cpp:165,441` `DeleteFileU8(TempName)`,
  `add.cpp:184` `MoveFileU8(TempName, ZipName)`.
  ⇒ **UTF-8 by construction**, and the surrounding code proves it (a non-UTF-8
  `TempName` would make the whole multi-volume write fail, not just the label).

**Site B — `COverwriteDialog2` ← `OverwriteDialog2()` ← `common.cpp:2246`, argument `name`:**
`name` is `CZipCommon::TestIfExist(const char* name)`'s parameter. Three live callers,
all UTF-8:
- `add.cpp:254` — `name` = heap `U8_MAX_PATH` copy of `ZipName` with the extension
  replaced by `SalamanderGeneral->SalPathRenameExtension(name, ".exe", U8_MAX_PATH)`
  (a core UTF-8 service)
- `add.cpp:455` — `ZipName` itself (after the same `SalPathRenameExtension`)
- `add_del.cpp:500` — `exeName`, built the same way
- and `TestIfExist` itself hands `name` to `SalGetFileAttributes` (`common.cpp:2217`)
  and `SetFileAttributesU8` (`common.cpp:2248`) — both UTF-8 services.
  ⇒ **UTF-8 by construction.**

Verdict for Q1: the fixer's premise holds. `SplU8ToWAlloc` is being fed genuine
UTF-8 in the normal case.

### 1.2 Q2 — Is there an input for which the control now shows *nothing*? **YES — this is the regression.**

`SetDlgItemTextU8` (`zip/common.cpp:337-345`) is all-or-nothing:

```c
BOOL SetDlgItemTextU8(HWND dlg, int item, const char* text)
{
    WCHAR* w = SplU8ToWAlloc(text);
    if (w == NULL)
        return FALSE;          // <-- nothing is set, and both call sites ignore the result
    BOOL ret = SetDlgItemTextW(dlg, item, w);
    free(w);
    return ret;
}
```

`SplU8ToWAlloc` (`src/plugins/shared/splunicode.h:29-40`) is the **strict**
`MB_ERR_INVALID_CHARS` decoder with **no** WTF-8 extension and **no** fallback —
unlike the core pair `SalU8ToWAlloc` (WTF-8-aware since 066,
`src/common/salunicode.cpp:245-249`) and unlike the core sink
`SalSetWindowTextU8` (`src/common/winlib.cpp:1102-1114`), which ends with

```c
    return SetWindowText(hWnd, u8Text); // not valid UTF-8 (transitional): keep the legacy path
```

That legacy fallback is the house contract (feature 005 ui-text C / feature 010
display-conversion C2, restated in `winlib.h:328-331`: *"invalid UTF-8 falls
back to the legacy A call"*). The plugin helper does not implement it, and
neither call site checks the return value.

**The failing input is real and shipped.** Measured on this machine
(`scratchpad/loc.c` output):

```
MB_ERR_INVALID_CHARS on WTF-8 lone surrogate -> 0 (err 1113)
lenient CP_UTF8 on the same -> 9
```

`ERROR_NO_UNICODE_TRANSLATION`. Feature 066 made unpaired-surrogate file names a
**supported, shipped** case (`Lone<U+D800>surrogate.txt` is the contract's own
example; such names are legal on NTFS, the panel lists them, and delete/copy/
move/rename/view were fixed to work on them). Since 066 the core's UTF-8 is
WTF-8, and it hands WTF-8 across the plugin boundary unchanged — the plugin SDK
converter simply cannot decode it.

**IDC_FILE starts empty** in both templates
(`zip/lang/lang.rc:193` and `:280` — `LTEXT "",IDC_FILE,61,14,269,8`), so the
failure leaves the control blank; there is no stale text to fall back on.

#### Failure scenario (concrete)

- **Surface**: ZIP plugin, *Confirm File Overwrite* dialog (both variants:
  multi-volume `CreateNextFile` and the SFX target test `TestIfExist`).
- **Locale / UI language**: any — including English. The defect is name-driven,
  not template-driven.
- **Reproduction**: an archive path containing an unpaired surrogate, e.g.
  pack into `D:\test\Lone<U+D800>surrogate.zip` (Alt+F5 defaults the archive
  name to the focused item's name, so this arises from a file/dir that 066
  supports) with *Multiple volumes* enabled, or convert such an archive to SFX
  where the `.exe` already exists.
- **What the user sees — before the fix**: `Overwrite file:` followed by
  `D:\test\LoníÇÇsurrogate.zip`-style mojibake — garbled, but the drive letter,
  the directory, the ASCII part of the name and the extension were all readable,
  so the user could tell *which* file was about to be overwritten.
- **What the user sees — after the fix**: `Overwrite file:` followed by
  **nothing at all**, then `File already exists on the disk. Overwrite?` and
  Yes / All / Retry / Cancel. A destructive confirmation with no subject.
- **Data path**: `src/salamdr*` panel name (WTF-8, 066) → `ZipName`
  (`zip/common.h:194`) → `MakeFileName` (`zip/common.cpp:2485`) → `TempName`
  → `zip/dialogs.cpp:1845` `SetDlgItemTextU8` → `zip/common.cpp:339`
  `SplU8ToWAlloc` returns NULL → `return FALSE`, control untouched.

This is **exactly the defect class of F-P5-09** ("blanks the title" → CONFIRMED,
`review-report.md:262`), reintroduced in the same batch that fixes it. Per the
charter's step 2 this is a **regressed surface**: an input that previously
produced text now produces none.

**Recommended minimal fix** (house-consistent, 2 lines each, keeps the
correction for the common case):

```c
    if (!SetDlgItemTextU8(Dlg, IDC_FILE, File))
        SendDlgItemMessage(Dlg, IDC_FILE, WM_SETTEXT, 0, (LPARAM)File); // legacy fallback (C2)
```

Do **not** be tempted to put the fallback inside `SetDlgItemTextU8` itself
without further analysis: its other 16 callers
(`dialogs.cpp:376,391,420,449,471,477,788,1417,1464,1495,1595,1601,1627,1638,1658,1706,1737`)
target **edit** controls whose text is read back through `GetDlgItemTextU8`
(`common.cpp:347`, `GetWindowTextW` → `SplWToU8`). Writing legacy bytes into
those would change the bytes that come back out — a round-trip hazard the two
display-only statics do not have. The two-line guard at the two static sites is
the safe minimum. (Making `SplU8ToWAlloc` WTF-8-aware, mirroring
`salunicode.cpp:204-249`, would be the systemic fix and would close this and the
sibling gaps at once — but that is an SDK change, outside FR-012's plugin-local
scope.)

Secondary, much rarer instance of the same hole: on `malloc` failure
`SplU8ToWAlloc` also returns NULL (`splunicode.h:37-39`), so under OOM the label
is blank where the old code needed no allocation at all.

### 1.3 Q3 — The `SubClassStatic` claim: **REFUTED** (conclusion right, mechanism wrong twice)

The comment added by the fix (`dialogs.cpp:1840-1844`) states:

> NOTE: `SubClassStatic()` installs the window proc with the ANSI
> `SetWindowLongPtr`, which makes this control an ANSI window, so characters
> outside the system code page still degrade - removing that limit belongs to
> the dialog-Unicode work deferred as group B-1, not here.

The **conclusion** ("characters outside the system code page still degrade") is
**correct**. The **stated cause is wrong in both directions**, which matters
because the note names the deferred B-1 work and would send it at the wrong
target.

`SubClassStatic` (`dialogs.cpp:138-145`) does use the ANSI variant — the zip
plugin is built without `UNICODE`/`_UNICODE` (confirmed from the actual compile
line, `scratchpad/build-fix-x08.log:763`: no `/D UNICODE`, no `/D _UNICODE`; and
the file compiles `_tcscpy(TCHAR[], LoadStr())` on `char*`), so
`SetWindowLongPtr` → `SetWindowLongPtrA`. But:

**(a) The control is already an ANSI window before the subclass, and
`SetWindowLongPtrA` does not change that.** Both dialogs are created with
`DialogBoxParam` → `DialogBoxParamA` (`dialogs.cpp:1795`, `:1892`), so the
dialog manager creates the child controls through the ANSI path. Measured
(`scratchpad/probe.c`, a `DialogBoxIndirectParamA` dialog with three statics,
mirroring `TextControlProc`):

```
ACP = 1250
dialog IsWindowUnicode = 0
static 100 IsWindowUnicode BEFORE subclass = 0
static 100 IsWindowUnicode AFTER SetWindowLongPtr(A) = 0
...
-- [101] NOT subclassed, SetDlgItemTextW --
static 101 IsWindowUnicode = 0
  GetWindowTextW (W): 0041 010C 003F 0042
```

The **non-subclassed** control degrades U+65E5 to `003F` (`?`) exactly like the
subclassed one. The subclass is not the cause. (For completeness, the A/W choice
*does* matter in the other direction — control 102 shows
`SetWindowLongPtrW` flips `IsWindowUnicode` 0→1 and the flag is sticky across
restoring the original proc — but the code as written never reaches a state the
subclass made worse.)

**(b) Even a Unicode control would still degrade, because the plugin paints the
text itself with ANSI APIs.** `TextControlProc` (`dialogs.cpp:52-88`) fully
handles `WM_PAINT` and `return 0`s — the standard static never paints. What the
user sees is produced by:

```c
        char txt[MAX_PATH];
        ...
        int len = GetWindowText(hWnd, txt, MAX_PATH);   // ANSI: W -> CP_ACP
        DrawText(ps.hdc, txt, lstrlen(txt), &r, format); // ANSI
```

Measured on the deliberately-Unicode control 102, which *does* store the
character correctly:

```
-- [102] subclassed with SetWindowLongPtrW --
static 102 IsWindowUnicode AFTER SetWindowLongPtrW = 1
  GetWindowTextW (W): 0041 010C 65E5 0042      <- stored correctly
  GetWindowTextA (A): 41 C8 3F 42              <- what the paint handler receives
```

So the CP_ACP ceiling is imposed **unconditionally by the plugin's own WM_PAINT
handler**, not by the window's Unicode flag. Making the control Unicode alone
would change nothing on screen; B-1 would additionally have to convert
`TextControlProc`'s `GetWindowText`/`DrawText` to `GetWindowTextW`/`DrawTextW`
with a `WCHAR` buffer.

**Corrected disclosure text (suggested):**

> NOTE: the control cannot show characters outside the system code page. It is
> an ANSI window because the dialog is created with `DialogBoxParamA`
> (`dialogs.cpp:1795`, `:1892`) — `SubClassStatic` does not cause that — and,
> independently, the plugin paints the label itself in `TextControlProc`
> (`dialogs.cpp:58-86`) with the ANSI `GetWindowText`/`DrawText`, which caps the
> result at CP_ACP whatever the window's Unicode flag says. Lifting the limit
> (group B-1) means a wide paint handler, not only a wide `SetWindowLongPtr`.

For the record, the same probe confirms the **pre-fix** defect precisely, so the
finding F-P5-08 itself is sound:

```
-- [100] OLD path on the subclassed control --
  [subclass proc sees WM_SETTEXT lParam bytes]: 41 C4 8C E6 97 A5 42
  GetWindowTextA (what TextControlProc paints) (A): 41 C4 8C E6 97 A5 42
```
(raw UTF-8 bytes painted as CP1250 = mojibake), versus after the fix:
```
-- [100] subclassed, SetDlgItemTextW --
  [subclass proc sees WM_SETTEXT lParam bytes]: 41 C8 3F 42
  GetWindowTextA (what TextControlProc paints) (A): 41 C8 3F 42
```
`C8` is `Č` in CP1250 — correct. The common case genuinely is repaired.

### 1.4 Q4 — Is `IDC_FILEATTR` genuinely independent? **Different control: yes. Homogeneous ANSI value: NO.**

`Attr` is produced by `GetInfo` (`zip/common.cpp:2469-2483`), which composes:

```c
    sprintf(buffer, "%s, %s, %s",
            SalamanderGeneral->NumberToStr(number, CQuadWord().SetUI64(size)), date, time);
```

- `NumberToStr` (`src/salamdr1.cpp:2922-2936`) inserts `ThousandsSeparator`,
  which is filled by **`SalGetLocaleInfoU8`** (`salamdr1.cpp:966`) — i.e. **UTF-8**.
- `date` / `time` come from `GetDateFormat` / `GetTimeFormat` un-suffixed in a
  non-`UNICODE` plugin — i.e. the **ANSI/CP_ACP** entry points.

So `attr` is a **mixed UTF-8 + CP_ACP composition** (the DC-19 shape), not a
homogeneous ANSI value. Measured on this machine (`scratchpad/loc.c`):

```
user locale = cs-CZ
ACP = 1250  OEMCP = 852
LOCALE_STHOUSAND: U+00A0
GetDateFormatA SHORTDATE bytes: 32 34 2E 30 38 2E 32 30 32 36  ("24.08.2026")
```

`U+00A0` → UTF-8 `C2 A0`, so on Czech Windows **every file of 1 000 bytes or
more** puts `C2 A0` into `attr`, and the ANSI `WM_SETTEXT` renders it as
`Â` + NBSP: `1Â 234 567, 24.08.2026, 13:05:09`. (The ANSI half is harmless in
cs-CZ — date and time are ASCII — so the *only* corruption comes from the UTF-8
half.) `IDC_FILEATTR` is not subclassed, so it is the plain static and this is a
straight ANSI-sink-on-UTF-8-bytes defect.

**Assessment**: as a *regression* question — `IDC_FILEATTR` is **unchanged**
(same statement, same bytes, same rendering before and after), and it is a
different control from `IDC_FILE`, so leaving it out does not break the fixed
site. But the implicit premise that it is "ANSI-only, therefore fine" is false;
it is an independent, still-open defect on the same two dialogs, visible in a
shipped configuration (cs-CZ, any file ≥ 1 000 bytes). It should be recorded as
a finding rather than passed over. Note that converting it with the *current*
`SetDlgItemTextU8` would be wrong twice over: the mixed buffer is not valid
UTF-8 whenever a CP_ACP date/time byte is present, so the strict decoder would
fail and blank the line (§1.2 again).

### 1.5 Q5 — ASCII byte-identity: **YES, byte-identical.**

For an ASCII `File`, `SplU8ToWAlloc` is the identity widening, and the
`SetDlgItemTextW` → `SendMessageW` → ANSI-window boundary narrows it back
through CP_ACP; every ACP of the 8 shipped languages (1250/1252/1253-class) is
an ASCII superset, so the subclass proc receives exactly the same bytes it
received from the old `SendDlgItemMessage(…, WM_SETTEXT, …)`, the static stores
the same text, and `GetWindowTextA`/`DrawTextA` paint the same pixels. The probe
shows the mechanism directly (the subclass proc is still reached, with narrow
bytes, on the new path). Additionally, `SetDlgItemTextW` and
`SendDlgItemMessage(…, WM_SETTEXT, …)` are the same message with the same
`GetDlgItem`-returns-NULL no-op behavior, so there is no control-lookup
difference either.

Side effect, in the *improving* direction (not a regression): `txt[MAX_PATH]` in
`TextControlProc` truncates at 259 bytes; the old path stored 2–3 bytes per
non-ASCII character, the new path stores 1, so long accented paths are now less
likely to be clipped.

### 1.6 X08 per-surface verdicts

| Surface | Verdict | Evidence |
|---|---|---|
| `IDC_FILE`, ASCII path (any UI language) | **unchanged** (byte-identical) | §1.5 |
| `IDC_FILE`, non-ASCII path representable in CP_ACP (e.g. `D:\Můj disk\a.zip` on cs-CZ) | **corrected** | probe: `41 C8 3F 42` vs old `41 C4 8C E6 97 A5 42`; `C8` = `Č` in CP1250 |
| `IDC_FILE`, characters outside CP_ACP (e.g. CJK on cs-CZ) | **unchanged-in-effect** (mojibake → `?`; both wrong, neither is text loss) | probe control 100/101/102 all yield `3F` |
| `IDC_FILE`, **WTF-8 / unpaired-surrogate path (066)** | **REGRESSED** — mojibake-but-readable → **empty** | §1.2; `loc.c`: `MB_ERR_INVALID_CHARS … -> 0 (err 1113)` |
| `IDC_FILE`, OOM | **REGRESSED** (minor) — text → empty | `splunicode.h:37-39` |
| `IDC_FILEATTR` (both dialogs) | **unchanged** (pre-existing mixed-composition defect, untouched) | §1.4 |
| Dialog layout / buttons / captions / return values | **unchanged** | no other statement changed |
| Plugin-facing services, SDK headers, interface version | **unchanged** | §0 |

### 1.7 Previously validated behavior touched by X08

- **Feature 066 quickstart** (unpaired-surrogate names must remain operable and
  identifiable): X08 **degrades** it on this dialog — 066's whole premise is that
  such names stay visible and workable; the name is now invisible at the moment
  the user must decide whether to overwrite.
- **Feature 067** (garbled numbers in drive/size dialogs): untouched here, but
  §1.4's `NumberToStr`-into-ANSI-sink is the identical shape one plugin deeper.
- **Features 041/042/043** plugin-dialog diffs: unaffected (`spl_gui.h` and the
  core forwarders unchanged).

### 1.8 X08 verdict: **REJECTED**

Regressed surface named: `IDC_FILE` in `COverwriteDialog` / `COverwriteDialog2`
for WTF-8 (unpaired-surrogate) archive paths — text present before, absent now,
on a destructive confirmation. Add the two-line legacy fallback (§1.2) and the
fix becomes acceptable. Independently, the recorded partial-fix disclosure in
`review-report.md` §7.1 must be corrected per §1.3.

---

## 2. X09 — `src/plugins/filecomp/mainwnd.cpp`, `WN_SET_PROGRESS` (2048-2055) and the `WM_USER_WORKERNOTIFIES` tail (2146-2153)

```
-            SetWindowTextW(HWindow, wBuf != NULL ? wBuf : L"");
-            free(wBuf);
+            if (wBuf != NULL)
+            {
+                SetWindowTextW(HWindow, wBuf);
+                free(wBuf);
+            }
+            else
+                SetWindowTextA(HWindow, buf);
```

### 2.1 Q1 — Is `free(wBuf)` still called on every path? **YES. No leak, no double free, no `free(NULL)` hazard.**

`SplU8ToWAlloc` (`splunicode.h:29-40`) returns non-NULL **iff** the `malloc`
succeeded:

```c
    if (u8 == NULL) return NULL;                       // nothing allocated
    int len = MultiByteToWideChar(...); if (len <= 0) return NULL;  // nothing allocated
    WCHAR* w = (WCHAR*)malloc(len * sizeof(WCHAR));
    if (w != NULL) MultiByteToWideChar(..., w, len);
    return w;                                          // NULL only if malloc failed
```

So `wBuf == NULL` ⇔ **no allocation was made**. The new `else` branch is
therefore not "a `free(NULL)` we now skip" — it is a branch where there is
provably nothing to free. Every allocating path (`wBuf != NULL`) frees exactly
once, before falling out of the block; `wBuf` is a block-scope local at both
sites, is not reused after the `if`, and neither branch can throw between the
allocation and the `free` (`SetWindowTextW` is a Win32 call; the enclosing TU is
`/EHsc`). Both sites are structurally identical. **Verified correct.**

(Style note only: the old code's `free(wBuf)` after the ternary also relied on
`free(NULL)` being a no-op, which it is; the new shape is stricter, not looser.)

### 2.2 Q2 — Is `SetWindowTextA` correct here, and what do cs/fr/hu/sk users now see?

**The window is an ANSI window**, so `SetWindowTextA` is the natural narrow path
and cannot fail to deliver text:

- class registered by `RegisterUniversalClass` → `RegisterClassEx`
  (ANSI, `src/plugins/shared/winliblt.cpp:406`) at `filecomp/dlg_com.cpp:441`
- created by `CWindow::CreateEx` → `CreateWindowEx` (ANSI,
  `winliblt.cpp:164`) at `filecomp/filecomp.cpp:715-727`
- the plugin is built without `UNICODE`/`_UNICODE`
  (compile line, `scratchpad/build-fix-x08.log:934`).

Consequence, which is worth stating plainly: on an ANSI window **both**
`SetWindowTextW` and `SetWindowTextA` end up CP_ACP-limited (the W call is
narrowed at the message boundary — probe §1.3 shows the identical mechanism for
child windows). So the wide branch is not "lossless" either; it is simply
*correct* for everything CP_ACP can represent, which is exactly what the ANSI
UI templates already are. `SetWindowTextA` is therefore **no worse than the W
path for the inputs that reach it**, and strictly better than the previous
`L""`.

**What the four affected languages see.** The trigger is that `buf` mixes an
**ANSI** `LoadStr` template with **UTF-8** file names, so the whole buffer is
usually not valid UTF-8 the moment the template has a non-ASCII letter. I
verified the actual shipped strings rather than assuming:

| String | ID | used at | cs | fr | hu | sk | de | es | nl | ro |
|---|---|---|---|---|---|---|---|---|---|---|
| `IDS_PLUGINNAME` | 1000 | 2144 (`!DataValid`) | **N** | a | **N** | **N** | a | a | a | a |
| `IDS_MAINWNDHEADER` | 1035 | 2129 | **N** | **N** | **N** | **N** | a | a | a | a |
| `IDS_MAINWNDHEADER_NODIF` | 1036 | 2132 | **N** | **N** | **N** | **N** | a | a | a | a |
| `IDS_MAINWNDHEADERCOMPUTING` | 1061 | **893 — MISSED** | **N** | **N** | **N** | **N** | **N** | a | a | a |
| `IDS_MAINWNDHEADERCOMPUTING2` | 1062 | 2132 | **N** | **N** | **N** | **N** | a | a | a | a |
| `…COMPUTING_PROGRESS` | 1063 | 2040 | **N** | **N** | **N** | **N** | a | a | a | a |
| `…COMPUTING_PROGRESS_FOUND` | 1064 | 2035 | **N** | **N** | **N** | **N** | a | a | a | a |

(**N** = contains non-ASCII → strict conversion of `buf` fails → old code blanked
the title; `a` = ASCII. Source: all 8 enabled `translations/*/filecomp.slt`,
IDs mapped positionally against `src/plugins/filecomp/lang/lang.rc2:20,35-41`;
enabled set per `translations/languages.cfg`. Czech examples:
1035 `%s %s: %s %s- Porovnání souborů - %d Rozdíl{…}`,
1063 `%s : %s - Porovnání souborů - Počítám rozdíly (%d%% hotovo)`.)

Note the last column pattern: **German is ASCII for every template used at the
two fixed sites, but non-ASCII for 1061 — the one the fix missed**
(`Drücken Sie ESC um abzubrechen`). So the missed site affects **5** shipped
languages (cs, fr, hu, sk, **de**) — one more than the fixed sites do, and de is
not even named in the recorded manual check.

- **Before**: the strict conversion failed on the template bytes → the title bar
  was set to `L""`, i.e. the File Comparator window showed an **empty caption**
  for its entire life in cs/fr/hu/sk. (Note this also covers the
  `!DataValid` branch, where `buf` is nothing but `LoadStr(IDS_PLUGINNAME)` —
  `Porovnání souborů` in cs — so even the bare plugin name was blanked.)
- **After**: `SetWindowTextA(HWindow, buf)` puts the buffer in as legacy bytes.
  The **template renders correctly** (it is genuine CP_ACP text from the `.slg`),
  and the **UTF-8 file names inside it render as mojibake** if they contain
  non-ASCII (ASCII names, the common case, render correctly). So a Czech user
  goes from *no title at all* to
  `mujsoubor.txt : jiny.txt - Porovnání souborů - Počítám rozdíly (42% hotovo)`.
  **Corrected** — and correct in full whenever both names are ASCII, which is
  the overwhelmingly common case.

This is precisely the C2 "never drop the text" fallback the contract prescribes,
and it matches `SalSetWindowTextU8`'s shape (`src/common/winlib.cpp:1102-1114`).

Residual (a **Note**, not a regression, and not introduced here): the plugin SDK
offers no lenient/display decoder — there is no `SplU8ToWDisplay` counterpart to
the core's `SalU8ToWDisplay` — so a plugin cannot reach the core's display
parity for non-ACP or WTF-8 names. A name with an unpaired surrogate still lands
in the narrow branch and shows as mojibake rather than the Explorer-parity
notdef glyph. Better than blank; not equal to the core.

### 2.3 Q3 — Does English/ASCII still take the wide path, byte-identically? **YES.**

The wide branch is **unmodified code**: same `SplU8ToWAlloc(buf)`, same
`SetWindowTextW(HWindow, wBuf)`, same `free`. In the English UI all six title
templates above are ASCII (`lang.rc2:35-41`), so `buf` is valid UTF-8 for any
valid-UTF-8 file name — including non-ASCII names — the conversion succeeds and
the caption is the same UTF-16 string as before, character for character. The
`else` branch is unreachable for every input that previously succeeded, because
its guard is exactly the old `wBuf == NULL` test. **No behavioral delta on any
previously-working input.**

Thread context is unchanged as well: `WN_SET_PROGRESS` and the completion
notifications arrive via `SendMessage`/`PostMessage` from the worker thread
(`worker2.cpp:145,197,236,330`, `worker.cpp:186-206`, `cwoptim.cpp:247,345`), so
both branches execute on the window's own thread, as before.

Buffer sizes are untouched (`TCHAR buf[MAX_PATH * 2 + 400]` at both sites) and
remain adequate: `Path1`/`Path2` are `char[MAX_PATH]` (`filecomp/filecomp.h:81`),
so each `SalPathFindFileName` result is ≤ 259 bytes and the worst case
(259 + 259 + a ≤127-byte `fmt` + numbers) fits in 920.

### 2.4 Q4 — Other "drop the text" patterns in this plugin that the fix missed

I swept the whole plugin (`rg` for `SplU8ToW`, `SplWToU8`, `MultiByteToWideChar`,
`WideCharToMultiByte`, `SetWindowText[AW]`, `SetDlgItemTextW`, `DrawTextW`,
`GetWindowTextW` across `src/plugins/filecomp/`). Two sites remain; **listed,
not fixed**, per charter.

1. **`src/plugins/filecomp/mainwnd.cpp:893-895` — the same defect, in the same
   file, on the same window, byte-for-byte the pattern that was fixed:**

   ```c
               sprintf(buf, LoadStr(IDS_MAINWNDHEADERCOMPUTING), SG->SalPathFindFileName(path1),
                       SG->SalPathFindFileName(path2));
               // 'buf' is assembled from UTF-8 file names (interface 104) -> show via the W API
               WCHAR* wBuf = SplU8ToWAlloc(buf);
               SetWindowTextW(HWindow, wBuf != NULL ? wBuf : L"");
               free(wBuf);
   ```

   This is `CMainWindow::SpawnWorker`, setting the title when a comparison
   starts. `IDS_MAINWNDHEADERCOMPUTING` (ID 1061) is **non-ASCII in cs, fr, hu,
   sk *and de*** (verified in all eight `.slt` files: `Počítám rozdíly` /
   `Calcul des différences` / `Különbségek számítása` / `Počítam rozdiely` /
   `Drücken Sie ESC um abzubrechen`), so
   in **5 of the 8 enabled languages** the title is still blanked here — for the
   whole duration of the comparison, until one of the two fixed sites happens to
   overwrite it. **This is a missed consumer of F-P5-09, not a separate
   finding**: the fix record's affected-surface entry says "filecomp window
   title", and this *is* the filecomp window title, set from the same class by
   the same idiom. The record is therefore incomplete, and a user testing the
   recorded manual check ("compare two files in cs/fr/hu/sk, title must not be
   empty") can still observe an empty title during the compare.

2. **`src/plugins/filecomp/controls.cpp:88-95` — same class, narrower trigger:**

   ```c
           WCHAR buff[2 * MAX_PATH];
           if (SplU8ToW(Text, buff, _countof(buff)) <= 0)
               buff[0] = 0;
           PathCompactPathW(dc, buff, r.right - r.left);
           DrawTextW(dc, buff, -1, &r, DT_SINGLELINE | DT_NOPREFIX);
   ```

   The file-name header bar paints **nothing** when the conversion fails.
   `Text` is a pure UTF-8 path with no ANSI template mixed in, so this cannot be
   triggered by the UI language — but it *is* triggered by a WTF-8
   (unpaired-surrogate) path, and by a path too long for `2 * MAX_PATH` WCHARs
   (`SplU8ToW` returns 0 on a too-small buffer as well as on malformed input,
   `splunicode.h:66-75` — the two failures are indistinguishable at the call
   site). No legacy fallback.

Neither was touched by X09 and I have not touched them.

### 2.5 X09 per-surface verdicts

| Surface | Verdict | Evidence |
|---|---|---|
| Title on `WN_SET_PROGRESS`, English UI, ASCII names | **unchanged** (identical wide path, identical bytes) | §2.3 |
| Title on `WN_SET_PROGRESS`, English UI, non-ASCII valid-UTF-8 names | **unchanged** (conversion still succeeds) | §2.3 |
| Title on `WN_SET_PROGRESS`, cs/fr/hu/sk | **corrected** (empty → readable template + names) | §2.2, IDs 1063/1064 |
| Title on the `WM_USER_WORKERNOTIFIES` tail (all `WN_*` cases that `break`), English | **unchanged** | §2.3 |
| Title on the `WM_USER_WORKERNOTIFIES` tail, cs/fr/hu/sk (incl. the `!DataValid` → `IDS_PLUGINNAME` branch) | **corrected** | §2.2, IDs 1035/1036/1062/1000 |
| Title with a WTF-8 name (066) | **corrected** (empty → mojibake-but-identifiable) | §2.2 residual note |
| Memory / lifetimes | **unchanged** (no leak, no double free) | §2.1 |
| Thread affinity | **unchanged** (both branches on the UI thread) | §2.3 |
| Buffers | **unchanged** (`buf` sizes untouched, worst case fits) | §2.3 |
| Title on `SpawnWorker` (`mainwnd.cpp:893`) | **still defective** — *missed consumer*, blank in cs/fr/hu/sk/**de** | §2.4 (1) |
| File-name header bar (`controls.cpp:88`) | **still defective** — related, narrower | §2.4 (2) |
| Plugin-facing services, SDK headers, interface version | **unchanged** | §0 |

### 2.6 Previously validated behavior touched by X09

- **Feature 066** (unpaired-surrogate names): improved, not altered — such a
  name previously blanked the title even in the English UI; it now shows.
- **Features 041/042/043** plugin-dialog work: unaffected; only two statements
  inside `CMainWindow::WindowProc` changed.
- **Feature 067**: untouched.

### 2.7 X09 verdict: **ACCEPTED** (record incomplete)

No regressed surface: the wide branch is untouched, every previously-succeeding
input produces byte-identical output, memory handling is strictly tighter than
before, and the cs/fr/hu/sk blank title is genuinely corrected at both changed
sites. The fix record must, however, be amended — the third title site
(`mainwnd.cpp:893-895`) is the same finding on the same surface and is still
open, so the recorded affected-surface list ("filecomp window title") is not
satisfied by the change as applied, and the recorded manual check can still fail.

---

## 3. Record defects to carry back into `review-report.md` §7.1

1. **X08 partial-fix disclosure is factually wrong** — `SubClassStatic` is not
   what makes `IDC_FILE` an ANSI window (`DialogBoxParamA` is), and the binding
   CP_ACP ceiling is the plugin's own ANSI `WM_PAINT` handler, which a
   dialog-Unicode change alone would not lift. Replacement text in §1.3.
2. **X08 has an unrecorded regressed surface** (WTF-8 names → empty label),
   §1.2.
3. **X08's "Affected surfaces" omits `IDC_FILEATTR`**, which is *not* a
   homogeneous ANSI value (UTF-8 thousands separator from `NumberToStr`) and
   carries its own shipped-configuration defect on the very same two dialogs,
   §1.4.
4. **X09's affected-surface list is incomplete** — `mainwnd.cpp:893-895` is the
   same finding on the same surface and was not fixed, §2.4 (1). It affects
   **de** as well as cs/fr/hu/sk, so the recorded manual check
   ("compare two files in cs/fr/hu/sk, title must not be empty") both misses a
   language and can still fail as written.
5. Related but out of both fixes' scope: `filecomp/controls.cpp:88-91` drops the
   path when conversion fails, §2.4 (2).

---

# Re-review after rework

**Trigger**: coordinator reworked both fixes in response to §1.2 and §2.4 (1).
**Bounded scope**: only the changed sites plus the two questions asked, plus the
FR-012 sanity check on the deferred items. Original text above is unchanged.
**Read-only on the product** — this pass modified nothing under `src/`.

**Build**: `build.cmd full` (Debug x64) re-run → BUILD SUCCEEDED. Both plugins
re-linked from the reworked sources (`zip.vcxproj -> …\zip.spl` and
`filecomp.vcxproj -> …\filecomp.spl`, log lines 844 / 856 of
`scratchpad/build-regr-x08x09-rework.log`); **no warnings** attributable to
either file.

**New empirical apparatus**: `scratchpad/probe2.c` — a verbatim copy of
`zip/common.cpp:337` `SetDlgItemTextU8` + `splunicode.h` `SplU8ToWAlloc`,
driven against a `DialogBoxIndirectParamA` dialog whose static is subclassed
exactly as `CDlgRoot::SubClassStatic` does it (including a `TextControlProc`
that swallows `WM_PAINT` and forwards everything else through
`CallWindowProc`). Run on this machine (`ACP = 1250`, `cs-CZ`).

## R1. X08 rework — **ACCEPTED**

```c
    if (!SetDlgItemTextU8(Dlg, IDC_FILE, File))
        SendDlgItemMessage(Dlg, IDC_FILE, WM_SETTEXT, 0, (LPARAM)File);
```
applied at `dialogs.cpp:1848-1849` and `:1938-1939`. This is the guard I
recommended, at the two static sites only, with the helper untouched — so the
round-trip hazard for its 16 edit-box callers
(`GetDlgItemTextU8` → `GetWindowTextW` → `SplWToU8`) is avoided as intended.

### R1.1 A new risk the rework creates, and why it does not bite

Keying the fallback on the helper's return value is only safe if that value is
reliably TRUE on success. `SetDlgItemTextU8` returns **not** a "did I convert"
flag but the result of `SetDlgItemTextW` (`zip/common.cpp:341-344`). Had that
been falsy for a subclassed static — plausible, since `WM_SETTEXT`'s return is
whatever the control's proc returns and `TextControlProc` forwards it through
`CallWindowProc` — the guard would have fired on **every** call and silently
reverted the whole fix to pre-fix mojibake. Measured, and it does not:

```
-- [100] subclassed static, valid UTF-8 name --
  SetDlgItemTextU8 returned 1  -> fallback would NOT run
  stored (A): 44 3A 5C 4D F9 6A 5C 68 61 2E 7A 69 70
-- [101] plain static, valid UTF-8 name --
  SetDlgItemTextU8 returned 1
-- [100] subclassed static, ASCII name --
  SetDlgItemTextU8 returned 1
```

(`F9` is `ů` in CP1250 — `D:\Můj\ha.zip` rendered correctly, the fix doing its
job.) The success path is unaffected by the guard.

Residual sub-case, harmless: if `SetDlgItemTextW` ever did return FALSE, the
wide call would by definition not have set the text, the fallback would then set
the legacy bytes, and the net visible result would still be the pre-fix
rendering. `OnInit` runs on `WM_INITDIALOG`, i.e. before the dialog's first
paint, so there is no flicker either. No path can leave the control empty, and
none can leave it worse than pre-fix.

### R1.2 Does the fallback restore pre-fix behaviour *exactly*? **YES — byte-identical.**

Measured on the WTF-8 case that caused the original rejection
(`D:\Lone<U+D800>surr.zip`):

```
-- [100] WTF-8 lone-surrogate name: reworked guard --
  SetDlgItemTextU8 returned 0 -> fallback runs
  after the failed helper (unchanged?) (A): 44 3A 5C 74 65 73 74 5C 70 6C 61 69 6E 2E 7A 69 70
  after legacy fallback          (A): 44 3A 5C 4C 6F 6E 65 ED A0 80 73 75 72 72 2E 7A 69 70
-- [101] same name, PRE-FIX path (plain ANSI WM_SETTEXT) --
  pre-fix bytes                  (A): 44 3A 5C 4C 6F 6E 65 ED A0 80 73 75 72 72 2E 7A 69 70
```

Two things are proved at once. First, the middle line re-confirms the original
defect independently: after the helper returned FALSE the control still held the
*previous* text — it really does set nothing. Second, the fallback's result is
**byte-for-byte identical** to what the pre-fix `SendDlgItemMessage(…,
WM_SETTEXT, …)` produced on a virgin control. The surrogate case shows the old
mojibake, not nothing. **Regression closed.**

### R1.3 Is the ASCII success path still byte-identical? **YES.**

```
-- [100] ASCII: new path vs old path byte comparison --
  new path (A): 44 3A 5C 74 65 73 74 5C 70 6C 61 69 6E 2E 7A 69 70
  old path (A): 44 3A 5C 74 65 73 74 5C 70 6C 61 69 6E 2E 7A 69 70
```

Identical, and the guard does not fire (helper returned 1), so the ASCII path is
the wide path exactly as in the first version of the fix.

### R1.4 Revised per-surface verdicts (X08)

| Surface | First pass | **After rework** |
|---|---|---|
| `IDC_FILE`, ASCII path | unchanged | **unchanged** (byte-identical, R1.3) |
| `IDC_FILE`, non-ASCII path within CP_ACP | corrected | **corrected** (unchanged by the rework, R1.1) |
| `IDC_FILE`, characters outside CP_ACP | unchanged-in-effect | **unchanged-in-effect** |
| `IDC_FILE`, **WTF-8 / unpaired surrogate** | **REGRESSED** | **unchanged** — exact pre-fix bytes restored (R1.2) |
| `IDC_FILE`, OOM | REGRESSED (minor) | **unchanged** — `SplU8ToWAlloc` returns NULL, helper returns FALSE, fallback sets the raw bytes |
| `IDC_FILEATTR` | unchanged | **unchanged** (still D02) |
| Plugin-facing services / SDK / interface version | unchanged | **unchanged** |

The in-code comment now added at `dialogs.cpp:1839-1847` is factually accurate on
every claim I can check: `SplU8ToWAlloc` is indeed strict with no WTF-8
extension; the helper is indeed all-or-nothing; 066 does ship names the strict
decoder rejects; and the CP_ACP note is now attributed to `TextControlProc`'s
ANSI `GetWindowText`/`DrawText` rather than to the subclass, per R3.

**X08 verdict: ACCEPTED.** No regressed surface remains; the record's
byte-identity claim ("ASCII names unchanged") is now true, and the correction
for the common case is intact.

## R2. X09 completion — **ACCEPTED**

`mainwnd.cpp:893-903` now carries the identical guard. Checked against the same
criteria as the other two:

- **Lifetime**: identical shape — `free(wBuf)` inside the `wBuf != NULL` branch;
  `SplU8ToWAlloc` returns NULL only when nothing was allocated
  (`splunicode.h:29-40`), so the `else` branch has nothing to free. No leak, no
  double free. The trailing `SetWait(TRUE);` is outside the `if/else` and still
  runs on both paths — confirmed in the diff.
- **English/ASCII**: wide branch untouched, guard is the old `wBuf == NULL`
  test, so every previously-succeeding input produces the identical caption.
- **German now covered**: `IDS_MAINWNDHEADERCOMPUTING` (1061) is the one title
  template that is non-ASCII in **de** as well as cs/fr/hu/sk (verified across
  all 8 enabled `.slt` files, table in §2.2). This was the only site that used
  it, so German's blank title during a comparison is closed by exactly this
  change. All 5 affected languages are now covered.
- **Thread context**: `SpawnWorker` runs on the UI thread; unchanged.

**No fourth `: L""` site.** I re-swept independently rather than trusting the
`rg`: every `SplU8ToWAlloc` / `SplU8ToW` / `SplU8ToWExtAlloc` occurrence in the
whole plugin is accounted for — `mainwnd.cpp:896, 2056, 2154` (all three now
guarded), `controls.cpp:91` (D03), `viewwnd3.cpp:163` and `worker.cpp:225`
(both `SplU8ToWExtAlloc` for file APIs, not text sinks, and both NULL-checked
before use). Confirmed: **no remaining blanking site in `mainwnd.cpp`.**

### R2.1 But there *is* a fourth window-title site — `worker2.cpp:85-100` (new item **D04**)

Widening the sweep from "drop-the-text" to "anything that sets this window's
title" turns up one more, which `rg ': L""'` cannot see because it never
converts at all:

```c
                TCHAR buf[MAX_PATH * 2 + 200];
                …
                    SG->ExpandPluralString(fmt, SizeOf(fmt), LoadStr(IDS_MAINWNDHEADER), 1, &qSize);
                    _stprintf(buf, fmt, SG->SalPathFindFileName(Files[0].Name), "",
                              SG->SalPathFindFileName(Files[1].Name), "", changes.size());
                …
                SetWindowText(MainWindow, buf);      // worker2.cpp:100 — ANSI, unconditional
```

Same buffer recipe as the three fixed sites (ANSI `LoadStr` template +
UTF-8 names, `IDS_MAINWNDHEADER` / `IDS_MAINWNDHEADERTOOMANY`), but it goes
straight to the ANSI `SetWindowText`. **This is not a blanking defect and not a
regression** — it is the pre-004 behaviour, i.e. exactly what the three fixed
sites now do in their *fallback* branch. It is milder but never gets the wide
path, so a non-ASCII file name renders as mojibake there **even in the English
UI**, where the three fixed sites now render it correctly.

It matters for this fix's *recorded surface* because of the ordering. For a
**binary** comparison:

1. `worker2.cpp:46/50` sends `WN_BINARY_FILES_DIFFER`; that case ends with
   `break` (`mainwnd.cpp:2004`), so control falls through to the tail — **fixed
   site 3** (`mainwnd.cpp:2154`) sets the title correctly;
2. the worker then fills the combo and, at `worker2.cpp:100`, **overwrites that
   title** with the ANSI-only version;
3. it then posts `WN_CBINIT_FINISHED`, whose handler `return 0`s
   (`mainwnd.cpp:2082`) without touching the caption.

So on the binary-compare path the *final, persistent* caption comes from the
unfixed site, and fixed site 3's benefit is masked there. Text comparisons,
`SpawnWorker` and the progress title are unaffected and fully corrected.

This does not change the verdict — nothing regressed, and closing it is a
different finding needing its own scenario and fail-before/pass-after check —
but the report's affected-surface wording "filecomp window title" is still
broader than what the fix delivers, and the manual check should be run on a
**text** comparison (or the binary case will look unfixed).

For completeness, the same plugin has other pre-existing DC-06 ANSI sinks on
UTF-8-bearing text that are outside both findings and unaffected by this fix:
`controls.cpp:167,303` (tooltip text), `dialogs.cpp:190-197,261,275,279`
(path combo history), `dialogs3.cpp:305,492`. Listed for the inventory only; I
have not analysed them.

### R2.2 Revised per-surface verdicts (X09)

| Surface | First pass | **After rework** |
|---|---|---|
| Title on `SpawnWorker` (`mainwnd.cpp:893`) | **still defective** (cs/fr/hu/sk/de) | **corrected** |
| Title on `WN_SET_PROGRESS` | corrected | **corrected** (unchanged) |
| Title on the `WM_USER_WORKERNOTIFIES` tail | corrected | **corrected** (unchanged); *masked on the binary path by D04* |
| English/ASCII, all three | unchanged (byte-identical) | **unchanged** |
| Memory / thread / buffers | unchanged | **unchanged** |
| Title on `worker2.cpp:100` | not examined | **still defective** — new item **D04**, not a regression |
| File-name header bar (`controls.cpp:88`) | still defective | **still defective** (D03) |

**X09 verdict: ACCEPTED.** All three blanking sites closed, all 5 affected
languages covered, no regressed surface, byte-identity preserved. The record
defect I raised in §2.7 is resolved; a narrower one (D04 / the binary-path
masking) replaces it.

## R3. The corrected subclass disclosure — **accurate, with one refinement**

The restatement is right on all three factual points: the control is already
`IsWindowUnicode = 0` because the dialogs use `DialogBoxParamA`
(`dialogs.cpp:1795, 1892`); `SetWindowLongPtrA` does not change that; and the
operative cap is `TextControlProc` painting through the ANSI
`GetWindowText`/`DrawText` (`dialogs.cpp:58-86`).

**One refinement, so group B-1 is not under-specified.** "…needs a wide **paint**
path, not a wide `SetWindowLongPtr`" reads as though the wide
`SetWindowLongPtr` were unnecessary. It is necessary too — just not sufficient.
The probe separates the two losses:

```
static 100 (ANSI)    GetWindowTextW (W): 0041 010C 003F 0042   <- lost at STORAGE
static 102 (Unicode) GetWindowTextW (W): 0041 010C 65E5 0042   <- survives storage
static 102 (Unicode) GetWindowTextA (A): 41 C8 3F 42           <- lost at READ
```

On an ANSI window the character is already gone by the time it is stored, so a
wide paint handler alone would read back `?`. On a Unicode window the character
survives storage but the ANSI read throws it away. **B-1 needs both**: the
control must be a Unicode window (wide `SetWindowLongPtr`, or creating the
dialog through `DialogBoxParamW`) **and** the paint handler must use
`GetWindowTextW` + `DrawTextW` with a `WCHAR` buffer. Suggested wording: "…needs
a wide paint path, not *only* a wide `SetWindowLongPtr` — in fact both."

## R4. FR-012 sanity check on the deferred items

FR-012 (`spec.md:375-380`) permits fixing a plugin-internal defect only when
**all three** hold: (1) confirmed user-visible in a shipped configuration,
(2) the fix is local to the plugin, (3) its regression surface is enumerated and
verified; otherwise it MUST be deferred with justification.

### D02 — `IDC_FILEATTR` / `GetInfo`: **deferral is REQUIRED, but the recorded reason is wrong**

- **(1) user-visible**: yes, and more so than recorded. Measured on this
  cs-CZ machine, `LOCALE_STHOUSAND = U+00A0` → `NumberToStr` embeds UTF-8
  `C2 A0` → a stray `Â` appears for **every file of 1 000 bytes or more**, i.e.
  effectively every overwrite prompt on a Czech system. That is *far* more
  frequent than the WTF-8 case X08 was reworked for. Locales whose group
  separator is a space-class character (fr — U+202F on current Windows — hu, sk,
  pl) have the same shape; de/es/nl/ro use `.` or `,` and are unaffected.
- **(2) local to the plugin**: **NO — and this is the blocker.** `GetInfo`
  (`common.cpp:2469`) has five call sites, and its output does not stay inside
  the plugin: `add.cpp:1233,1235` feed `attr1`/`attr2` straight into
  **`SalamanderGeneral->DialogOverwrite(…)`** (`add.cpp:1236`), i.e. across the
  plugin boundary into the **core's** `COverwriteDlg` (`src/zip.cpp:664,679,642`;
  `src/dialogs.h:464-478`), and `extract.cpp:1503` feeds it to
  `SafeCreateCFile(…, attr, …)`. Changing what `GetInfo` emits changes bytes a
  core service receives — squarely the frozen **B-5** category, not a
  plugin-local change.
- **(3) regression surface**: five call sites reaching at least three distinct
  dialogs, one of them in the core. Not enumerated, not verified.

⇒ **Deferring is correct — indeed mandatory under FR-012(2).** But the recorded
justification ("not fixable with the current helper without reintroducing the
blanking you just rejected") should be replaced: that obstacle no longer exists
— the rework just established the guarded-fallback idiom, which would apply
verbatim. The true reasons are (a) `attr` is a *mixed* buffer that **no** sink
can render correctly, so it must be made homogeneous in `GetInfo` itself, and
(b) `GetInfo`'s value crosses back into the core via `DialogOverwrite`, so the
change is not plugin-local. Suggested reason line: *"`GetInfo` composes a
UTF-8 `NumberToStr` result with ANSI `GetDateFormat`/`GetTimeFormat`; the buffer
must be made homogeneous at the source, and it is consumed by the core's
`COverwriteDlg` through `CSalamanderGeneral::DialogOverwrite`, so the fix is not
plugin-local (B-5/FR-009). Visible on cs-CZ for every file ≥ 1 000 bytes."*

### D03 — `filecomp/controls.cpp:88-91`: **deferral is a correct judgement call**

- **(1) user-visible**: yes, but narrowly — the path bar blanks only for a WTF-8
  (unpaired-surrogate) path or a path exceeding `2 * MAX_PATH` WCHARs. Note
  `SplU8ToW` returns 0 for both malformed input *and* a too-small buffer
  (`splunicode.h:66-75`), so the two are indistinguishable at the call site.
  Language-independent, so it is not a `.slt`-driven everyday case.
- **(2) local**: yes, and trivially so — the plugin already owns the paint
  handler; ~4 lines (`if (SplU8ToW(…) <= 0) { DrawTextA(dc, Text, …); }`).
- **(3) regression surface**: one control, one paint handler, display-only, no
  round-trip. Fully enumerable.

All three conditions are met, so FR-012 would *permit* fixing it. Deferring is
still the right call, for process rather than technical reasons: it is a
different finding from F-P5-09, it was never raised with its own failure
scenario (charter rule 7), and promoting it now would require a new finding
record, a verifier pass and its own regression review (FR-008/FR-010) for a rare
trigger. Record it with the trigger and the four-line sketch so it is cheap to
pick up later. **No objection.**

### D04 — `filecomp/worker2.cpp:100` (new, from R2.1): **defer, same reasoning as D03**

Not a blanking defect and not a regression; the pre-004 ANSI behaviour. Meets
FR-012(2) and (3) easily (one statement, one window, the fallback idiom already
established three lines away in the same plugin) and (1) in the English UI with a
non-ASCII file name after a **binary** comparison. Defer on the same
process grounds as D03 — but record the ordering fact from R2.1, because it is
the reason a tester following the recorded manual check on a binary comparison
would conclude X09 did not work.

## R5. Final verdicts

| Fix | First pass | **Re-review** |
|---|---|---|
| **X08** — `src/plugins/zip/dialogs.cpp` ×2 | REJECTED | **ACCEPTED** |
| **X09** — `src/plugins/filecomp/mainwnd.cpp` ×3 | ACCEPTED (record incomplete) | **ACCEPTED** (record complete for the blanking defect; D04 replaces the earlier record defect) |

Subclass disclosure: **accurate**, with the "not *only* a wide
`SetWindowLongPtr` — in fact both" refinement in R3.
Deferrals: **D02 required** (with a corrected reason), **D03 correct**,
**D04 new and recommended for the same deferred list**.

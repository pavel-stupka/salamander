# Encoding Contract Checklist — the reviewers' reference

**Feature**: 068-encoding-regression-review · **Status**: Phase-1 artifact,
binding for the audit (FR-003, FR-004, FR-005). Produced 2026-08-24 from the
thirteen encoding features' spec/research/contract/inventory/tasks records,
`tools/check_encoding.py` and `src/saltests/saltests.cpp` as they stand.

This document is what every review perspective works from. It has four
parts: **A** the defect classes (each needs a sibling sweep — FR-003),
**B** the contract obligations (each needs a compliance verdict — FR-004),
**C** the deferred-item ledger (each needs a fresh disposition — FR-005),
**D** sweep-list additions the audit surfaced beyond the spec's US3 list.

Conventions: `A` after an API name = ANSI entry point (the core is built
without `UNICODE`, so every un-suffixed Win32 text API is `A`); "U8 sink" =
the `Sal*U8` helper family in `src/common/winlib.*`; "facade" = the `Sal*`
file/registry wrappers in `src/common/salfileio.*`, `salpath.*`,
`src/salamdr6.cpp`.

---

## Part A — Defect classes (sweep each to **complete**)

| ID | Shape | Fixed instances (features) | Candidate-site pattern to sweep | Guard today |
|---|---|---|---|---|
| DC-01 `ansi-api-on-utf8-path` | A UTF-8 name/path (`CFilesWindow::Path`, `CFileData::Name`, `Configuration.*`, `SalGetTempFileName` output, any `UTF8_SOURCE`) passed to an `A` Windows API — file, shell, change-notification, drive classification, temp path, process launch, environment | 004 (wholesale), 027, 058 (`FindFirstChangeNotificationA`), 062 (`GetDriveTypeA`, `GetFileAttributes`, `CreateFile`), 063 (`CreateFileA`, `DeleteFile`, `GetTempPath`, `GetSystemDirectory`) | un-suffixed/`A` name-taking call reached from a UTF-8 value: 287 FS + 39 shell + 23 process sites in core (triage, research R1); `HANDLES(CreateFile(…))` is still raw | **none** — 058 contract rule 4 in prose only. **New rule required** (research R8) |
| DC-02 `cp-acp-on-utf8` | `MultiByteToWideChar(CP_ACP,…)` on UTF-8 input or `WideCharToMultiByte(CP_ACP,…)` on a wide name, result reaching a display/parse/operation | 042 (`finddlg1.cpp:4181`), 058 (`fileswn1.cpp:496`, `geticon.cpp:352`), 062 (`salamdr2.cpp:1589/1611`) | 43 `MB2WC(CP_ACP` + 32 `WC2MB(CP_ACP` in core; classify each: legitimate legacy fallback (after a failed `SalU8ToW` probe) vs. primary conversion of a UTF-8 value | `cp-acp-display` covers W→A into a *drawing* sink only; the A→W direction is **uncovered** |
| DC-03 `mixed-composition` | ANSI `LoadStr()` printf format + UTF-8 name/path argument → message box | 042 (84 sites), 043, 052 (15) | printf-family with `LoadStr(` format and a name argument; 140 `LoadStr`-format compositions remain in core — each needs "is any argument UTF-8?" | `mixed-composition` (message-box sink only); 2 live suppressions (`dialogs6.cpp:645`, `mainwnd3.cpp:2842`) |
| DC-04 `ansi-template-caption` | ANSI `LoadStr` template + name → `CTruncatedString`/subject caption | 043 (10 sites) | `.Set(` with `LoadStr(` template and a name value | `ansi-template-caption` |
| DC-05 `ansi-template-number` | ANSI `LoadStr` composed with a UTF-8 grouped number — including *inside* a shared formatter | 041, 067 (`PrintDiskSize` modes 1/2; `zip.cpp:6566`) | `LoadStr(IDS_*BYTES` as `ExpandPluralString` template; `NumberToStr`/`PrintDiskSize` output composed with `LoadStr(`; check resource IDs whose spelling is not `*BYTES` too | `ansi-template-number` (only the `IDS_*BYTES` spelling + `ExpandPluralString` shape) |
| DC-06 `utf8-to-legacy-sink` | A UTF-8 value reaching an `A` UI sink: `ListView_SetItemText`, `SetWindowText`/`SetDlgItemText`, `SB_SETTEXT`, `CB_ADDSTRING`, `LB_ADDSTRING`, `InsertMenuItem`, `CopyTextToClipboard` | 005, 010, 043 (42), 052, 063 (11) | 428 `A` UI-text calls in core vs 156 U8 sinks; `WM_SETTEXT` 54, `CB_*STRING` 19, `LB_*STRING` 13, `LVITEM` 20, `TVITEM` 4 message-based sites | `utf8-to-legacy-sink` — value must be a tracked `UTF8_SOURCE`/`UTF8_IDENT`; anything else is invisible; 2 live suppressions (`dialogs2.cpp:909`, `fileswn5.cpp:2764`) |
| DC-07 `dead-dispinfow` | `LVN_GETDISPINFOW` handler in a dialog that never sends `NF_REQUERY` (can never run); `return NFR_UNICODE` from a dialog proc | 042 (`finddlg1.cpp`, `packac.cpp`) | file has `LVN_GETDISPINFOW` and no `NF_REQUERY`; `TVN_GETDISPINFOW` likewise | `dead-dispinfow` — core only; known instances in `tserver/` (L20) and `regedt` (L46) are outside its scope |
| DC-08 `ansi-tooltip-handler` | ANSI `TTN_NEEDTEXT`/`TTN_GETDISPINFO` handler (or `TOOLINFO`/`TTM_ADDTOOL` A registration) receiving UTF-8 | 067 (`viewer3.cpp`) | `TTN_NEEDTEXT`/`TTN_GETDISPINFO`/`TTM_ADDTOOL`/`TOOLINFO` without `W` (core: 4/1/1/4 tokens); known remaining `mainwnd3.cpp:5324` (L06) | **none** |
| DC-09 `ansi-producer-into-strict-probe` | Text from an ANSI producer (`LoadStr`, an `A` OS call, `WNetGetConnection`, plugin `LoadStr`) fed to a consumer that probes strictly for UTF-8 and *degrades differently* on failure | 063 (`CToolTip::GetText`, `SalGetTempFileName`, `fileswn9.cpp:1881`), 052 | every `SalU8ToW*` probe whose failure branch does something other than a lossless legacy fallback; every `A` producer whose output reaches a facade (`SalCreateFile` etc. fail with `ERROR_INVALID_NAME` on CP1250 bytes) | **none** |
| DC-10 `latin1-byte-widening` | Invalid-UTF-8 fallback widening bytes 1:1 to `WCHAR` | 063 (`gui.cpp:621-628`) | `WCHAR` produced from `unsigned char` without `MultiByteToWideChar` | **none** |
| DC-11 `byte-offset-as-wchar-offset` | Byte offset/length of a UTF-8 string reused as a `WCHAR` offset (or vice versa) | 058 (`fileswn1.cpp:496`), 010, 041 | a `strlen`-derived value used as a `WCHAR*` displacement; mixed byte/WCHAR bookkeeping in one path | **none** (058 rule 5, 010 C3 prose) |
| DC-12 `byte-width-measure-truncate` | Width measured / padded / truncated in **bytes** on UTF-8, cutting mid-sequence | 005, 010, 063 (`DoExpandVarString`, `gui.cpp:1056`) | `strlen`/`lstrcpyn`/`memcpy` as a display width or clamp on a UTF-8 value without `SalU8CharCount`/`SalU8Next` | **none** |
| DC-13 `strict-probe-rejects-wtf8` | Raw `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,…)` probe deciding Unicode-vs-legacy on a value that can carry a name | 066 (5 sites) | 1 remaining core site (triage); plugin-shared `Spl*` helpers by design (L48); 066 contract requires non-name sites be *marked reviewed* | **none** |
| DC-14 `lossy-lenient-at-intake` | Lenient conversion (`WideCharToMultiByte(CP_UTF8, 0,…)`, `SalU8ToWDisplay*`) on an **operational** value, baking `U+FFFD` into a name/path/persisted value | 066 (`SalConvertFindDataW`, registry read side) | `WideCharToMultiByte(CP_UTF8, 0` and `SalU8ToWDisplay*` outside paint/measure paths; note `SalConvertFindDataW` still has a lenient last resort (empty string on double failure) | **none** |
| DC-15 `signed-char-name-byte` | `char` byte compared as a control character (`>= 32`, `<= ' '`) rejecting every byte ≥ 0x80 | 005 D4 (3 core + plugin copies) — **and still open** at the `fileswn8.cpp` Recycle-Bin guard (L07) | `>= 32`, `<= ' '`, `< 0x20` on a name-buffer element without `(unsigned char)`; `IsNotAlphaNorNum[256]`/`IsAlpha[256]` byte tables in `InitLocales` (`salamdr1.cpp:939-943`) are the same assumption | **none** |
| DC-16 `ansi-dialog-downconverts-wide` | Correct wide setter issued to a control on a dialog created by `DialogBoxParamA` → USER32 down-converts through the ACP → `?` | 015 (`CCopyMoveDialog` family) | `CDialog` subclass with name/path fields, `UnicodeWnd` unset, using `Sal*U8`/`…W` setters; the guard's `wide_fallback()` even suppresses the legacy branch here | **none** |
| DC-17 `undefined-encoding-of-cached-string` | Persisted/registry-cached translated or free text with no defined encoding, written ANSI by one producer, read as UTF-8 by another | 052 (`CPluginData`) | any `char*` persisted via `SetValue`/`GetValue` that is also written from a plugin/`LoadStringA` producer; **never swept**: custom packer/unpacker titles (`packers.cpp:734` `DupStr`, L17), other cached translated strings | `UTF8_IDENT` tracks the *known* 052 identifiers only |
| DC-18 `missed-twin-of-a-fixed-site` | A site sharing a resource ID / message / sink with an already-converted site, not swept | 067 (`zip.cpp:6566` vs `fileswn6.cpp:1109`/`fileswn8.cpp:1129`, `IDS_NOTENOUGHSPACE`) | for every `LoadStrU8(IDS_X)` a remaining `LoadStr(IDS_X)`; for every `Sal*U8` call an `A` twin on the same control/resource | **none** — mechanically checkable; would have caught 067 |
| DC-19 `u8-sink-fallback-masks-composition` | A U8 sink's "invalid UTF-8 → raw `A` call" tolerance turning an upstream mixed composition into visible mojibake, invisible to sink-based rules | 067 root cause of the guard blind spot | any `Sal*U8` sink fed by a buffer built from **both** `LoadStr(` and a `UTF8_SOURCE` | `ansi-template-number` covers the number sub-case only |
| DC-20 `converter-conflates-too-long-and-invalid` | `SalU8ToW`'s single failure value (0) conflates "buffer too small" with "invalid UTF-8", so callers cannot skip vs. fall back correctly | none — open (060 D2 at `snooper.cpp` ×3, L08) | every `SalU8ToW(...)` whose failure branch does a CP_ACP fallback and then uses the buffer regardless | **none** |

Sweep rule: a class is **complete** when every candidate site matching its
pattern in core is listed in `inventory.md` with a classification; plugin
sites are listed for DC-05/06/07/13 where the pattern is known to occur
(Part C rows L38–L50) and classified per FR-012.

---

## Part B — Contract obligations (verdict per obligation: compliant / deviation → Finding)

The spec named the five contracts of features 052, 058, 063, 066, 067; the
audit surfaced seven older ones that are equally binding. All twelve are
checked.

### B1 — `specs/004-long-paths-unicode/contracts/plugin-interface-vnext.md`, `app-manifest.md`
- Names/paths crossing the plugin ABI are UTF-8 (`char*` shapes unchanged); display/measure inside plugin UI converts to UTF-16.
- Manifest: `longPathAware=true`; **no** `activeCodePage` (rejected four times — L36; do not revisit).

### B2 — `specs/005-fix-unicode-display/contracts/ui-text-contract.md`
- Program→control converts and uses the W message; control→program reads wide and converts back (buffer ≥ 3×WCHAR+1).
- GDI measure/draw wide; truncation only at WCHAR boundaries, never splitting a surrogate pair.
- Invalid UTF-8 falls back to the legacy ANSI call (not `?`, not drop).
- Byte-level name-validation loops treat bytes as `unsigned char` (≥0x80 always legal) — see DC-15.

### B3 — `specs/010-fix-filename-encoding/contracts/display-conversion-contract.md`
- C1 UTF-8 `char*` never to an `A` text API; render via W on a converted buffer, prefer the `Sal*U8` helper.
- C2 mandatory legacy fallback on strict-conversion failure — never drop text, never substitute `?`.
- C3 measurement/truncation/ellipsis/hit-test/offset units are **WCHAR of the converted string**, never bytes; mixed bookkeeping forbidden — see DC-11/12.
- C4 convert on change, not on paint. C5 actioned values come from the stored UTF-8/wide value, never re-read from an ANSI control.
- C6 registry only via `SalRegSetValueExW8`/`SalRegQueryValueExW8`. C7 no `UNICODE` flip, no UTF-8 ACP manifest, no behavior change for ASCII.

### B4 — `specs/041-fix-infoline-encoding/contracts/locale-text.md`, `information-line.md`
- C-1 every narrow string from regional settings is valid UTF-8 from the moment obtained; C-2 access only through `SalGetLocaleInfoU8`/`SalGetDateFormatU8`/`SalGetTimeFormatU8`; direct `A` locale calls not permitted where output reaches a UTF-8 consumer; the six Group-C exemptions (`bugreprt.cpp` ×5, `salamdr1.cpp:3824`) recorded explicitly.
- C-4 separators ≥16-byte UTF-8 buffers, lengths are **byte** counts; C-5 `CSalamanderGeneral::NumberToStr`/`PointToLocalDecimalSeparator` return UTF-8, signatures/vtable unchanged; C-6 strict conversion for operations, lenient display-only for the information line, never on anything persisted.
- Information line: fields independent (no field may affect another's rendering); one `U+FFFD` per unrepresentable character; readable in every shipped language **and** in the Find dialog; truncation never splits a character.

### B5 — `specs/042-fix-find-results-encoding/contracts/composed-message.md`, `notification-format.md`
- A composition containing a name MUST use `LoadStrU8`; a mixed message costs the *whole* message (`CMessageBox` wide path is all-or-nothing).
- English hides the class — covered surfaces verified in all shipped languages.
- **Forbidden**: changing how `SalMessageBox`/`CMessageBox`/`LoadStr` interpret text (plugin-facing); opt-in is per call site.
- Malformed input → lenient `SalU8ToWDisplay`, `U+FFFD` never written back; `?` = defect, `�` = correct handling.
- A dialog owning a name-carrying list view MUST send `WM_NOTIFYFORMAT`/`NF_REQUERY` from `WM_INITDIALOG`; the ANSI handler MUST NOT use `WideCharToMultiByte(CP_ACP,…)`; type-to-search/sorting/clipboard follow the stored names.

### B6 — `specs/052-fix-plugin-name-encoding/contracts/plugin-metadata-encoding.md`
- Every translated/free-text `char*` in `CPluginData` holds valid UTF-8 at all times; producers own the invariant (`SetBasicPluginData` normalizes; the registry facade returns UTF-8); consumers use UTF-8-capable sinks; tolerant fallbacks are defense in depth, not an excuse.
- Guard: `UTF8_IDENT` tracks the metadata identifiers; build fails without python.
- §4 out-of-scope items are ledger rows L12, L13, L17.

### B7 — `specs/058-fix-cloud-status-icons/contracts/path-encoding-icon-pipeline.md`
- Panel paths/item names are UTF-8; any hand-off to a Windows API converts and calls W (`SalU8ToW`/`SalU8ToWAlloc`).
- Invalid UTF-8 → legacy CP_ACP/ANSI fallback, never fail the operation outright (legacy plugin callers).
- No ANSI shell/file APIs on panel-derived paths (`FindFirstChangeNotificationA`, `MultiByteToWideChar(CP_ACP,…)` on UTF-8, `SHGetFileInfoA`).
- Offsets are per-encoding (DC-11). `CSalamanderGeneral::GetFileIcon` unchanged (valid UTF-8 → UTF-16, else CP_ACP).

### B8 — `specs/062-fix-delete-to-recycle/contracts/delete-pipeline-contract.md`
- C1 the classification chain accepts UTF-8 paths of panel length, converts via `SalU8ToW` (CP_ACP fallback), calls wide WinAPI, **no `MAX_PATH` truncation** anywhere in the chain; ASCII-short-path classification byte-identical.
- C2 the bin is vetoed only for removable/remote/CD-ROM/RAM disk; indeterminate → attempt the bin; a classification failure must never silently produce a permanent delete; which popup appears never depends on path spelling/depth/cloud state.
- C3 reparse item is a "link" only when `IsReparseTagNameSurrogate`. C4 every path to the shell recycle operation is wide from UTF-8. C5 one Debug-only TRACE at the gate.
- E6 sites outside the chain are ledger rows L01–L04.

### B9 — `specs/063-fix-filelist-encoding/contracts/filelist-text-encoding.md`
- C1 the generated list is UTF-8 end to end; clipboard via `CopyTextToClipboardU8`; viewer via BOM (viewer destination only); file raw UTF-8 no BOM.
- C2 **`CopyTextToClipboard` keeps CP_ACP semantics forever** (plugin ABI); new core code must not pass UTF-8 into it; `CF_TEXT` is always the best-effort CP_ACP projection of the wide text.
- C3 `ToolTipText` is UTF-8 at rest (normalized at both intakes); `CToolTip` converts tolerantly and always draws wide; clamps cut on UTF-8 boundaries; core producers pass `LoadStrU8`; `CStaticText::SetText` fallback is CP_ACP, never Latin-1 widening.
- C4 `$(Var:N)`/`:max` measure in code points, truncate on boundaries; `varPlacements` stays in bytes. C5 `SalCreateFile`/`SalDeleteFile` on the feature path; `SalGetTempFileName` uses the W API.

### B10 — `specs/066-fix-surrogate-filenames/contracts/name-encoding-wtf8.md`
- Internal name encoding is WTF-8; byte-identical to UTF-8 for valid input; only unpaired surrogates gain the 3-byte form; every other malformed input MUST still be rejected by `SalU8ToW*` (the 004/063 heuristics depend on it).
- `SalWToU8*` total; `SalU8ToWDisplay*` one-way, never for paths/identities. Round-trip law `SalU8ToW(SalWToU8(w)) == w`.
- **Validity-probe obligation**: any "is this valid UTF-8?" decision on a value that can carry a name probes via `SalU8ToW`, never a raw `MB_ERR_INVALID_CHARS` call; non-name strict sites MUST be marked as reviewed (DC-13).
- ≤3 bytes per 16-bit unit; byte equality ⇔ on-disk unit equality; collation falls back to byte order, items never vanish from a sort. Plugin-shared helpers stay strict by design (L48). Explorer is the behavioral reference.

### B11 — `specs/067-fix-drive-info-encoding/contracts/number-format-encoding.md`
- `NumberToStr`/`NumberToStr2`/`PointToLocalDecimalSeparator` always valid UTF-8; `PrintDiskSize(…, u8=FALSE)` legacy and frozen; `PrintDiskSize(…, TRUE)` whole-string UTF-8; `ExpandPluralString` byte-transparent.
- A buffer holding formatter output MUST NOT be composed with `LoadStr()`; MUST reach the screen through a UTF-8-aware sink; MUST NOT go to a genuinely ANSI display API.
- Plugin API boundary **frozen** (`zip.cpp:1397/1402/3878/5170` never pass `u8`); no interface version change. Buffer sizes verified (≤50 / ≥100).

### B12 — `src/plugins.h:2414-2423` (plugin metadata contract of record) and `src/common/salunicode.h:6-48` (converter contract block)
- Name, Version, Copyright, Description, Extensions, ChDrvMenuFSItemName, `CPluginMenuItem::Name` hold valid UTF-8; DLLName/RegKeyName/FSNames/URL/ThumbnailMasks are ASCII by nature.
- Internal narrow strings carrying names/paths are UTF-8; conversion only at OS boundaries; stored names never normalized/case-folded.

---

## Part C — Deferred-item ledger (fresh disposition required for every row)

**Enc** = encoding-related. **Status** as found today. Rows are grouped; IDs are stable and referenced by `review-report.md`.

### C1 — Core application, encoding

| ID | Origin | Location | Item | Enc | Status |
|---|---|---|---|---|---|
| L01 | 062 E6 | `src/fileswn8.cpp:125` `SetCurrentDirectory` | ANSI call on a panel-derived path, outside the classification chain | yes | OPEN |
| L02 | 062 E6 | `src/drivelst.cpp:1481` | ANSI call outside the classification chain | yes | OPEN |
| L03 | 062 E6 | `MyGetVolumeInformation` (`src/salamdr2.cpp`) | ANSI volume-information query on a panel path | yes | OPEN (overlaps L05) |
| L04 | 062 E6 | `QueryDosDevice`/`ResolveSubsts` | ANSI subst-target resolution | yes | OPEN |
| L05 | 010 R6/R9 | `src/drivelst.cpp:1110-1121, 1739` | drive-bar volume-label acquisition ANSI end-to-end (labels outside the ACP cannot render) | yes | OPEN |
| L06 | 067 R5 | `src/mainwnd3.cpp:5324` | split-bar `%` tooltip, ANSI `TTN_NEEDTEXT` composing a decimal-separator value | yes | OPEN (latent — all shipped separators ASCII) |
| L07 | 066 tasks | `src/fileswn8.cpp` Recycle-Bin guard `Name[NameLen-1] <= ' '` | signed-char comparison refuses names whose last byte ≥ 0x80 ("Recycle Bin cannot handle this name") | yes | OPEN — DC-15, believed closed by 005 |
| L08 | 060 D2 | `src/snooper.cpp` ×3 | `SalU8ToW` fails on *length* for paths ≥ ~780 WCHARs; ACP fallback also fails; `FindFirstChangeNotificationW` gets an indeterminate buffer | yes | OPEN — DC-20 |
| L09 | 060 D3 | `src/snooper.cpp`, `src/fileswn1.cpp` | `MB_PRECOMPOSED` makes the ACP fallback fail when the system ACP is UTF-8 | yes | OPEN (latent) |
| L10 | 060 D8 / 058 R5 | `src/geticon.cpp:361-368` | double conversion failure → `ParseDisplayName` on an uninitialized stack buffer | yes | OPEN |
| L11 | 058 R5 | `src/geticon.cpp` `wszPath[MAX_PATH]` | MAX_PATH truncation for long-path icons (guarded upstream by 027) | partly | OPEN |
| L12 | 042/043/052 | `src/dialogs6.cpp:645` | network share name into a `LoadStr` template — live suppression | yes | OPEN |
| L13 | 042/043/052 | `src/mainwnd3.cpp:2842` | configuration name into a `LoadStr` template — live suppression | yes | OPEN |
| L14 | 043 | `src/dialogs2.cpp:909` | `.slg` file name treated as ANSI ("from `FindFirstFile`") — live suppression; **premise stale**: 004/066 made `FindFirstFile` output UTF-8/WTF-8 | yes | OPEN — re-check premise |
| L15 | 043 | `src/fileswn5.cpp:2764` | quick-rename window legacy fallback — live suppression | yes | OPEN by design |
| L16 | 043 | `src/msgbox.cpp` | `MSGBOXEX_PARAMS` asymmetry: body UTF-8, checkbox/hint/URL text ANSI (plugin-reachable) | yes | OPEN |
| L17 | 052 §4 | registry-cached translated strings outside `CPluginData` (custom packer/unpacker titles, `packers.cpp:734`) | DC-17 never swept | yes | OPEN |
| L18 | 041/042 | application-wide `LoadStr` → UTF-8 | attempted, broke Find, reverted; per-site + guard is the standing decision | yes | OPEN by design — not revisited here |
| L19 | 042 R1 | `CDialog::CDialogProc` `WM_NOTIFYFORMAT` | serving pre-`WM_INITDIALOG` notifications centrally (~100 dialogs) | yes | OPEN by design |
| L20 | 042/043 | `src/tserver/tablist.cpp:748` | DC-07 in the Trace Server (separate exe, excluded from the guard) | yes | OPEN |
| L21 | 041 | locale-dependent | a non-ASCII short date/time format was never observed | yes | OPEN — convert to an automated check |
| L22 | 041 | size dialogs / archive browsing | not reached by 041's harness | yes | CLOSED by 067 |
| L23 | 015 R2a | `src/viewer.cpp`, `viewer2.cpp` | UTF-16 viewer rendering (files shown as hex) | yes | OPEN |
| L24 | 015 R2b | `src/viewer.cpp` | caret/selection approximate on multi-byte lines | yes | OPEN |
| L25 | 015 | `src/viewer.cpp` | CJK double-width glyphs one cell each | yes | OPEN |
| L26 | 066 T017 | rename dialog | typing unpaired surrogates out of scope | yes | OPEN by design |
| L27 | 066 | external text channels | lossy rendering of unrepresentable units acceptable | yes | OPEN by design |
| L28 | 066 | converters | CESU-8 idempotence not guaranteed (never produced) | yes | OPEN by design |
| L29 | 010 | `src/plugins/ftp` log-edit caret | byte offsets (clamped by `EM_SETSEL`) | yes | OPEN (cosmetic) |
| L30 | 010 R9 | `src/filesbx2.cpp:230,270` | panel header column titles drawn `TextOutA` (localized text) | yes | OPEN by design — re-check under DC-06 |
| L31 | 005 A6 | `src/mainwnd3.cpp:5077`, `src/viewer3.cpp:561` | ANSI tooltip disp-info handlers | yes | `viewer3` CLOSED by 067; `mainwnd3` → L06 |
| L32 | 063 D4 | `src/mainwnd4.cpp` `char fileName[MAX_PATH]` | Make File List target not long-path capable | no | OPEN |
| L33 | 058 R6/R9 | `src/shiconov.cpp:261-269`, `InitGoogleDrivePath` | 2015-era Google Drive gating machinery, dead code | no | OPEN |
| L34 | 058 | Explorer Status column | different mechanism | no | OPEN by design |
| L35 | 062 | `src/zip.cpp:5154` `ResolveLocalPathWithReparsePoints` | plugin-exported MAX_PATH buffer contract | partly | OPEN by design |
| L36 | 004/005/010/058 | manifest `activeCodePage=UTF-8` | rejected four times | yes | OPEN by design — do not revisit |
| L37 | 004 R10 / 027 | `MAX_PATH` for components, 8.3 names, roots, `DefaultDir[26][MAX_PATH]` | deliberate | no | OPEN by design |

### C2 — Plugins

| ID | Origin | Location | Item | Enc | Status |
|---|---|---|---|---|---|
| L38 | 067 R6 | `src/plugins/ftp/dialogs6.cpp:377-379` | low-disk-space hint separator garbled (U8-first sink; needs a U8 API) | yes | OPEN |
| L39 | 067 R6 | `ftp/fs4.cpp:325-327`, `ftp/operats1.cpp:1210` | plugin ANSI `LoadStr` + API `NumberToStr` | yes | OPEN |
| L40 | 067 R6 | `ftp/operatsb.cpp:1140` | upload log line, same class | yes | OPEN |
| L41 | 067 R6 | `dbviewer/dialogs.cpp:510`, `dbviewer/parser.cpp:302/345/349/1033` | ANSI sinks; separator garbled ≥4 digits (the evidence that froze the API) | yes | OPEN |
| L42 | 067 R6 | `regedt/finddlg.cpp:410` | unconditional CP_ACP conversion of a UTF-8 number | yes | OPEN |
| L43 | 067 R6 | `zip/dialogs.cpp:1839-1840, 1925-1926` | ZIP overwrite dialogs use ANSI `WM_SETTEXT` although the plugin ships `SetDlgItemTextU8` | yes | OPEN — FR-012 candidate (local, 4 lines) |
| L44 | 067 R6 | `filecomp/mainwnd.cpp:2043-2046, 2135-2137` | `SplU8ToWAlloc(...) : L""` — ANSI-Czech title silently blank | yes | OPEN — FR-012 candidate |
| L45 | 067 R8.3 | `regedt/fs4.cpp:479` | `CQuadWord` passed to a `%s/%d` format | no | OPEN — FR-015 candidate |
| L46 | 043 | `regedt/finddlg2.cpp:932` | DC-07 (no `NF_REQUERY` anywhere in the plugin) | yes | OPEN |
| L47 | 041 | automation, checksum, filecomp, ftp, pictview, regedt, renamer, undelete | plugins formatting numbers never runtime-verified | yes | 5 of 8 confirmed by 067 |
| L48 | 066 | `src/plugins/shared/splunicode.h`, `winliblt.cpp` | plugin-shared converters strict UTF-8 (WTF-8 names fail in every plugin) | yes | OPEN by design — re-examine (plugin-boundary seed S3) |
| L49 | 027 | bundled plugins' file-operation UI | long-path buffer audit not run | no | OPEN |
| L50 | 005 E6/E9 | uniso, checksum, nethood, folders, uncab, tar, unchm, unole, unmime (+ removed ones) | name fields not U8-wrapped — "sweep in impl." never completed | yes | OPEN |

### C3 — Translations / language data / tooling

| ID | Origin | Location | Item | Enc | Status |
|---|---|---|---|---|---|
| L51 | 067 R8.1 | `translations/french/salamand.slt` string 12820 `{!}%s octets{s\|0\|\|1\|s}` | every French byte count renders "octetss" | no | OPEN — FR-015 candidate (data-only) |
| L52 | 067 R5 | `translations/languages.cfg` re-enable checklist | ~13 `PrintDiskSize` mode-0/3/4 sites mixed in ru/uk/zh only | yes | latent — checklist note |
| L53 | 056 F3 / 063 | `tools/translate/addrows.py:45`; ru/uk/zh `sftp.slt` 82 vs 87 rows | must be fixed before re-enabling those languages | partly | OPEN (dev-only) |
| L54 | 056 F4 | `tools/translate/addrows.py:81` | mis-ordered `.slt` on gain+loss | no | OPEN (dev-only) |
| L55 | 056 F5 | `tools/translate/relayout.py:79` | stale text can import silently | no | OPEN (dev-only) |
| L56 | 056 P-f | `tools/translate/layout.py:196` | dedupe on `&`-stripped body | no | OPEN (dev-only) |
| L57 | 063 | translator layout validator | already red module-wide; "no new findings" is the gate | no | OPEN (tooling) |
| L58 | 052 D5 | `translations/ui-overrides.json` | pin future identifier-type plugin names | no | obligation, not a defect |

### C4 — Non-encoding items carried from 056 / 060 / 027 (dispose quickly; FR-015 applies)

| ID | Origin | Location | Item | Enc |
|---|---|---|---|---|
| L59 | 056 P-d | `src/plugins1.cpp:2184` | unbounded `sprintf` of plugin name+path+template (same function family 052 converted) | no |
| L60 | 056 P-e | `src/plugins1.cpp:2166` | `strcpy("plugins\\")`+cat in `InitDLL` prologue | no |
| L61 | 056 F2 | `sftp/dialogs.cpp:1131` | label widening beyond a clamped dialog | no |
| L62 | 056 P-a | `sftp/session.cpp:503` | host-key trust TOCTOU on retry | no |
| L63 | 056 P-b | `sftp/session.cpp:718` | password-path auth failures unclassified | no |
| L64 | 056 P-c | `sftp/dialogs.cpp:54` | `GetDlgItemTextU8` temp WCHAR copy not zeroized | no (helper is encoding-related) |
| L65 | 056 P-g | `sftp/lang/lang.rc2:128` | duplicate accelerators | no |
| L66 | 060 D1 | `src/shiconov.cpp:856-868` | SEH handler leaks one `IPropertyStore` | no |
| L67 | 060 D4 | `src/shiconov.cpp`, `src/fileswn1.cpp` | overlay worst-case scanning | no |
| L68 | 060 D5 | `utils/migrate-altap-settings.cmd` | `%`-expansion, echo, no cycle guard | partly |
| L69 | 060 D6 | `src/shiconov.cpp:1168` | relative `LoadLibrary("cldapi.dll")` | no |
| L70 | 060 D7 | `src/shiconov.cpp` | theoretical index staleness | no |
| L71 | 027 | `src/shexreg.h:218` IPC struct | fixed shell-extension ABI | no |
| L72 | 027 §Bounded | Compare Directories, Shift+F4, archive backups, clipboard paste path, hot-path save, window title | long-path truncation without crash | no |
| L73 | 027 | archive-subsystem path buffers | bounded today | no |
| L74 | 027 §External | shell/launch/MAPI/common-dialog MAX_PATH limits | external limits | no |
| L75 | 062 | harness observation | unreproduced `Use Recycle Bin` read-back anomaly | no |
| L76 | 041 | `src/finddlg1.cpp:3865` | `clang-format` failure in a comment | no |
| L77 | 047 | hot-path config | name==path degrades to "unnamed"; subkey `"0"` → index 9 | no |
| L78 | 047 | `src/jumplist.cpp` | no gallery icon | no |

### C5 — Verification debt (observe, or convert to an automated check)

| ID | Origin | Never verified | Enc |
|---|---|---|---|
| L79 | 042 | the 84 bulk-converted message sites, individually (one representative in 9 languages) | yes |
| L80 | 043 | ~25 window-text/number sites individually; config language field at runtime; drag image by an actual drag | yes |
| L81 | 041/042/043 | plugin dialogs at runtime (diff only) | yes |
| L82 | 042 | a non-Central-European legacy code page | yes |
| L83 | 063 | GUI scenarios 1–7 (clipboard fidelity, viewer/file destinations, Czech hint tooltip, DPI labels, non-ASCII `%TEMP%`) | yes |
| L84 | 066 | the 12-row acceptance walk, Explorer parity, fixtures-041 regression, saved-config round trip | yes |
| L85 | 067 | English pre-fix capture (argued by construction) | yes |
| L86 | 062 | interactive SHIFT+DEL smoke | no |
| L87 | 056 | the encoding perspective returned no structured findings; adversarial phase interrupted | yes |
| L88 | 058 | GUI screenshots not captured programmatically | yes |
| L89 | 041 | 8 of 18 plugins not runtime-verified; non-ASCII date/time locale; other-display regression PARTIAL | yes |

---

## Part D — Sweep-list additions beyond the spec's US3 list

Surfaces the thirteen features validated that the spec's sweep list does not
name explicitly; `quickstart.md` W1–W20 absorbs them as sub-steps:

- tiles-view size line; drag image (actual drag); beta-expiry long date;
- Ctrl+C inside a message box; jump-list titles; hot-path settings-page list;
- packer/unpacker/archiver custom entries round trip (DC-17, L17);
- `$(FileName:max)` alignment and `$(FileName:10)` boundary truncation;
- non-ASCII `%TEMP%`; a non-Central-European legacy code page (L82) and a
  non-ASCII date/time format (L21) — both by automated construction, not by
  changing the maintainer's Windows settings;
- byte-identity gates: `git diff v0.1.4 -- src/plugins/shared/spl_gen.h src/plugins.h src/plugins/shared/spl_vers.h` empty except documentation, `LAST_VERSION_OF_SALAMANDER` = 106, plugin-facing output including its documented pre-existing garble unchanged.

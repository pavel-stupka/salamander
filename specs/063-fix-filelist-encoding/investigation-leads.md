# Investigation Leads — 063-fix-filelist-encoding

Preliminary code exploration performed during specification (2026-08-19). These are
**leads, not conclusions** — each must be confirmed (or refuted) with runtime
evidence during planning/implementation. Line numbers are as of commit `15f70c6`.

## Symptom 1 — garbled list on the clipboard (primary)

Confirmed mechanism at the code level; runtime confirmation still required.

Data path:

1. `CMainWindow::MakeFileList()` (`src/mainwnd4.cpp:200`) runs `CFileListDialog`,
   creates a temp file (clipboard/viewer destinations) or the user's file, then calls
   `CFilesWindow::MakeFileList(hFile)`.
2. `CFilesWindow::MakeFileList` (`src/fileswn6.cpp:124`) expands the line template
   per item via `ExpandMakeFileList(...)` using `CFileData::Name` — **UTF-8 by
   contract since feature 004/052** — and writes raw bytes to the file. The file
   content is therefore UTF-8.
3. For the clipboard destination, `mainwnd4.cpp:315` reads the file back and calls
   `CopyTextToClipboard(buff, fileSize, FALSE, NULL)` (`src/salamdr4.cpp:1190`)
   → `CopyHTextToClipboard` (`salamdr4.cpp:1235`) which:
   - calls `AddUnicodeToClipboard(text, textLen)` (`salamdr4.cpp:1021`) — converts
     with **`MultiByteToWideChar(CP_ACP, ...)`** (`salamdr4.cpp:1027,1039`). UTF-8
     bytes interpreted as CP1250 → mojibake in `CF_UNICODETEXT`. This is what every
     modern paste target reads.
   - then puts the raw buffer on the clipboard as `CF_TEXT` — raw UTF-8 bytes,
     mojibake for legacy ANSI consumers too.

Same defect family as features 052/058: a feature-004 regression-by-omission where
UTF-8 data crosses into a CP_ACP-assuming sink.

Fix candidates (house pattern — add a U8-aware entry point, do NOT change the
semantics of the existing ANSI helpers, they have many CP_ACP callers):

- Convert UTF-8 → UTF-16 via `SalU8ToW`/`SalU8ToWAlloc` and use the existing
  `CopyTextToClipboardW` (`src/consts.h:719`), or add `CopyTextToClipboardU8`.
- `CF_TEXT` leg should then be produced from the UTF-16 text via
  `WideCharToMultiByte(CP_ACP, ...)` (best-effort legacy representation), which the
  W path presumably already does — verify.
- Audit the other two destinations: the temp file shown in the **viewer** and the
  **file** destination already receive UTF-8 bytes. Verify the internal viewer's
  code-page auto-detection on UTF-8 without BOM (`RecognizeFileType`,
  `src/consts.h:728`) — decide BOM or explicit viewer code-page hand-off for the
  viewer leg; decide BOM policy for the file leg (FR-004).

## Symptom 2 — garbled line-format hint in the dialog

- `src/dialogs.cpp:1981`: `hl->SetActionShowHint(LoadStr(IDS_FILELISTLINE_HINT))`
  (`CHyperLink::SetActionShowHint`, `src/gui.cpp:1304`); shown via
  `MainWindow->ToolTip` (`CStaticText::ShowHint`, `src/gui.cpp:954`).
- Two hypotheses — determine which side of the feature-004/052 encoding contract the
  tooltip window sits on:
  - (a) **more likely**: the tooltip renderer draws text through a UTF-8-aware path
    (feature-004 conversion), while `LoadStr` returns CP_ACP text → CP1250 input
    rendered as UTF-8 → mojibake. Fix: `LoadStrU8` at the call sites **or** make the
    hint mechanism accept a declared encoding.
  - (b) `LoadStr` output is fine but the tooltip draws via an ANSI API while the
    string is UTF-8.
- Note: ~15 other `SetActionShowHint(LoadStr(...))` sites exist (`dialogs2.cpp:623`,
  `dialogs2.cpp:1256`, `dialogs3.cpp` ×5, `dialogs4.cpp` ×2, `dialogs5.cpp:3169,3172`,
  `dialogs6.cpp:1659`, `dialogsp.cpp:721,1273`) — mostly `IDS_MASKS_HINT`, whose
  translations also carry diacritics. FR-006 requires fixing the mechanism, so all
  these sites must end up correct; prefer one fix at the mechanism level over 15
  call-site edits, but follow whatever the 052 contract dictates.
- English `IDS_FILELISTLINE_HINT` (`src/lang/texts.rc2:1558`) is pure ASCII — that is
  why the defect only shows in localized UIs.

## Symptom 3 — clipped "Soubor" radio label

- Control `IDC_FL_FILE` (radio, dialog `IDD_FILELIST`) — English "File" is short;
  Czech "Soubor" overflows the control width in the Czech dialog template.
- Dialog layouts are per-language (translator toolchain, `translations/czech/…`).
  Check the Czech `salamand.slt` dialog section for `IDD_FILELIST` control geometry;
  widen the control there (and check the other shipped languages / all labels in
  this dialog per FR-007). Alternative: widen in the master template if the layout
  is shared.
- Remember the two-stage `.slt` refresh if any English resource changes are needed
  (see memory: adding/altering lang.rc2 strings breaks `build.cmd full` until the
  refresh runs).

## Possible bonus defect (unconfirmed) — `:max` column alignment

- `CFilesWindow::MakeFileList` phase 1 computes `maxSizes[]` via
  `ExpandMakeFileList(..., ignoreEnvVarNotFound=TRUE, maxSizes, ...)`; if widths are
  measured with `strlen` over UTF-8, accented names count bytes, not characters →
  misaligned columns (FR-005). Verify inside `ExpandVarString`
  (`src/execute.cpp:1682` area) how widths are measured and padded.

## Verification ideas

- Clipboard: paste into Notepad + `Get-Clipboard` in PowerShell; compare against
  panel names (SC-001). Include non-CP1250 characters (Greek/CJK) for FR-002.
- ASCII regression guard: produce a list of ASCII names pre/post fix and diff
  (SC-004 / FR-008).
- Czech UI smoke for the dialog (hint + labels) at 100/150/200% DPI (SC-003).

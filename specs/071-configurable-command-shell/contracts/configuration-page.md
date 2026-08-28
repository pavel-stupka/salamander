# Contract: Configuration → Command Shell page

**Feature**: 071 · **Dialog**: `IDD_CFGPAGE_CMDSHELL` (new, `src/lang/lang.rc`) ·
**Class**: `CCfgPageCmdShell : public CCommonPropSheetPage` (`src/cfgdlg.h`,
`src/dialogs4.cpp`) · **Help**: `[ALIAS] IDD_CFGPAGE_CMDSHELL=hh\salamand\configuration_cmdshell.htm`

## 1. Placement and template

- Added in `CConfigurationDlg::CConfigurationDlg` **right after `PageHotPath`**
  (new index 15; every later `/*NN*/` comment renumbered; the `mode == 3 ? 21`
  literal becomes `22`). Tree label = template caption **"Command Shell"**.
- Template shape is byte-compatible with the other pages:
  `DIALOGEX 0, 0, 299, 231`, `STYLE DS_SETFONT | DS_FIXEDSYS | DS_CONTROL | WS_CHILD | WS_CAPTION`,
  `CAPTION "Command Shell"`, `FONT 8, "MS Shell Dlg", 400, 0, 0x1`.
- Constructor: `CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_CMDSHELL, IDD_CFGPAGE_CMDSHELL, PSP_USETITLE, NULL)`.

## 2. Controls (ids from `_APS_NEXT_CONTROL_VALUE` onward in `src/lang/lang.rh`)

| Id | Control | English text | Notes |
|---|---|---|---|
| `IDT_CMDSHELL_INTRO` | LTEXT | "The Command Shell command (Num /, Ctrl+/) opens this program in the directory of the active panel:" | wraps, 2 lines |
| `IDT_CMDSHELL_PROGRAM` | LTEXT | "&Program:" | label of the combo |
| `IDC_CMDSHELL_PRESET` | COMBOBOX `CBS_DROPDOWNLIST \| WS_VSCROLL \| WS_TABSTOP` | 6 items, fixed order = preset id | not-found presets get the `IDS_CMDSHELL_NOTFOUND` suffix " (not found)" |
| `IDT_CMDSHELL_FOUNDAT` | LTEXT | "Found at:" | |
| `IDE_CMDSHELL_FOUNDAT` | EDITTEXT `ES_AUTOHSCROLL \| ES_READONLY \| NOT WS_TABSTOP` | resolved path, or "(not found)" text | as the Language page does; UTF-8 via `SalSetDlgItemTextU8`; empty and disabled for Custom |
| `IDC_STATIC_n` | GROUPBOX | " Custom program " | shared static pool |
| `IDT_CMDSHELL_CUSTPROG` | LTEXT | "P&rogram:" | |
| `IDE_CMDSHELL_CUSTPROG` | EDITTEXT `ES_AUTOHSCROLL \| WS_GROUP` | | bound with `ti.EditLine(..., Configuration.CommandShellProgram, SAL_MAX_PATH_UTF8)` |
| `IDB_CMDSHELL_BROWSE` | PUSHBUTTON "..." | | opens the **wide** executable browser (§4) |
| `IDT_CMDSHELL_CUSTARGS` | LTEXT | "&Arguments:" | |
| `IDE_CMDSHELL_CUSTARGS` | EDITTEXT `ES_AUTOHSCROLL \| WS_GROUP` | | bound with `ti.EditLine(..., Configuration.CommandShellArguments, USRMNUARGS_MAXLEN)` |
| `IDT_CMDSHELL_HINT` | LTEXT | "Arguments may use $(FullPath) for the panel directory and $[NAME] for environment variables. The program always starts in the panel directory." | wraps, 3 lines |

Accelerators (`&`) must be unique within the page (`P`, `r`, `A`); the
translation merge checks this.

## 3. Behaviour

**Init (`WM_INITDIALOG` / `Transfer(ttDataToWindow)`)**

1. Resolve every preset 0..4 once with the OS probe; keep the results in the page.
2. Fill the combo: `LoadStr(IDS_CMDSHELL_PRESET_*)` per id, appending
   `LoadStr(IDS_CMDSHELL_NOTFOUND)` for presets that were not found; item 5 =
   `IDS_CMDSHELL_PRESET_CUSTOM`.
3. `CB_SETCURSEL` = `Configuration.CommandShellPreset`; remember it as
   `LastPreset`.
4. `UpdateFoundAt()` and `EnableControls()` (§3.1).

**Selection change (`CBN_SELCHANGE`)**

1. `sel` = current selection.
2. If `sel == sspCustom` and **both** Custom edits are empty and `LastPreset`
   is a preset that was found: set the program edit to that preset's resolved
   path and the arguments edit to that preset's recipe (may be empty). Never
   overwrite a non-empty field (US2 scenarios 7–8).
3. `LastPreset = sel` (only when `sel` is a preset, so Custom → Custom keeps
   the last real preset as the pre-fill source).
4. `UpdateFoundAt()`, `EnableControls()`.

### 3.1 `EnableControls()` / `UpdateFoundAt()`

- Custom edits, Browse, their labels and the hint: enabled **iff** `sel == sspCustom`.
- *Found at*: for a preset → the resolved path or the `IDS_CMDSHELL_NOTFOUNDTEXT`
  text "(not found on this computer)"; for Custom → empty, disabled.

**Validate (`Validate(CTransferInfo&)`)** — run by the sheet on OK, focuses the
page and the control on failure:

| Condition | Message (`SalMessageBox`, `MB_OK \| MB_ICONEXCLAMATION`, caption `IDS_ERRORTITLE`) | `ErrorOn` |
|---|---|---|
| selected preset not found | `IDS_CMDSHELL_PRESETNOTFOUND`: "The selected program was not found on this computer. Choose another one, or select Custom program and enter its path." | combo |
| Custom and program blank | `IDS_CMDSHELL_PROGRAMREQUIRED`: "Enter the program to run as the command shell." | program edit |
| Custom and program syntax invalid (`ValidateCommandFile`) | existing `IDS_INCORRECTSYNTAX`; select the bad range (`EM_SETSEL errorPos1..errorPos2`) | program edit |
| Custom and arguments syntax invalid (`ValidateCommandShellArguments`) | existing `IDS_INCORRECTSYNTAX`; select the bad range | arguments edit |

**Transfer (`ttDataFromWindow`)**: `Configuration.CommandShellPreset = CB_GETCURSEL`;
the two edits through `ti.EditLine` (UTF-8 aware). Cancel leaves the
configuration untouched (sheet semantics).

## 4. Browse

`IDB_CMDSHELL_BROWSE` → new `SafeGetOpenFileNameW`-based helper (mirror of the
existing `BrowseFileName` / `SafeGetSaveFileNameW` pattern in `src/dialogs.cpp`):

- initial file = current edit text (`SalGetDlgItemTextU8` → `SalU8ToW`);
- filter = `LoadStrW(IDS_EXEFILTER)` converted to the double-NUL form;
- flags `OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY`;
- result → `SalWToU8` → `SalGetFullName` → `SalSetDlgItemTextU8`.

Rationale: the existing `BrowseCommand` is `GetOpenFileNameA` and would garble a
non-ACP path (FR-016).

## 5. Strings (English source, `src/lang/texts.rc2` + `src/texts.rh2`)

| Id | Text |
|---|---|
| `IDS_CMDSHELL_PRESET_CMD` | "Command Prompt" |
| `IDS_CMDSHELL_PRESET_POWERSHELL` | "Windows PowerShell" |
| `IDS_CMDSHELL_PRESET_PWSH` | "PowerShell 7" |
| `IDS_CMDSHELL_PRESET_WT` | "Windows Terminal" |
| `IDS_CMDSHELL_PRESET_GITBASH` | "Git Bash" |
| `IDS_CMDSHELL_PRESET_CUSTOM` | "Custom program" |
| `IDS_CMDSHELL_NOTFOUND` | " (not found)" |
| `IDS_CMDSHELL_NOTFOUNDTEXT` | "(not found on this computer)" |
| `IDS_CMDSHELL_PRESETNOTFOUND` | see §3 |
| `IDS_CMDSHELL_PROGRAMREQUIRED` | see §3 |
| `IDS_CMDSHELL_BROWSETITLE` | "Select Command Shell Program" |
| `IDS_CMDSHELL_ERRTITLE` | "Error Starting Command Shell" |
| `IDS_CMDSHELL_ERRNOTFOUND` | see [command-shell-setting.md](command-shell-setting.md) §4 |
| `IDS_CMDSHELL_ERREXEC` | see [command-shell-setting.md](command-shell-setting.md) §4 |

Product names (*Windows PowerShell*, *PowerShell 7*, *Windows Terminal*,
*Git Bash*) are proper nouns: pin them in `translations/ui-overrides.json` for
all 8 languages if the machine translation alters them (the merge run's dry-run
shows what DeepL returns).

## 6. Translation and help touch points

- One `[DIALOG IDD_CFGPAGE_CMDSHELL]` block (caption + one row per control) and
  14 `[STRINGTABLE]` rows appear in `translations/<lang>/salamand.slt` for the 8
  enabled languages after `translate.merge --module salamand`.
- `help/src/salamand.hhp` `[ALIAS]` gains
  `IDD_CFGPAGE_CMDSHELL=hh\salamand\configuration_cmdshell.htm`; `salamand.hhc`
  (Configuration branch, alphabetical: … Change Drive Menu, Colors, **Command
  Shell**, Confirmations …) and `salamand.hhk` gain one entry each.

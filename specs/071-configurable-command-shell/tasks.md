# Tasks: Configurable Command Shell

**Input**: Design documents from `/specs/071-configurable-command-shell/`
**Prerequisites**: plan.md, spec.md (3 user stories), research.md (R1–R19), data-model.md, contracts/ (3), quickstart.md

**Tests**: Included for the pure common module only — plan decisions R3/R16 make `saltests` coverage of `src/common/salshell.*` part of the design (fake probe). Everything in the main application (page, expansion table, launch) is verified through the manual matrix in quickstart.md; those verification steps are explicit tasks so a ticked box means "checked", not "written".

**Organization**: Phases 3–5 map 1:1 to spec user stories US1–US3 (P1, P2, P3). Each phase ends with a verification task against quickstart.md and an entry in `fix-log.md` (the running record this project keeps in the specs directory).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task)
- **[Story]**: US1–US3 per spec.md
- Paths are repository-relative

## Path Conventions

- Core application: `src/*.cpp`, `src/*.h`; shared library: `src/common/`; language resources: `src/lang/` (+ `src/texts.rh2`); tests: `src/saltests/saltests.cpp`; projects: `src/vcxproj/`
- Translations: `translations/<lang>/salamand.slt`, tooling under `tools/translate/`
- Manual: `help/src/` (HTML Help project + `hh/salamand/*.htm`)

---

## Phase 1: Setup

**Purpose**: bookkeeping and file scaffolding so every later task lands in a registered file

- [x] T001 Create the running log `specs/071-configurable-command-shell/fix-log.md` (sections: Status checklist per task, Decisions taken while implementing, Verification results, Deviations from the plan) and keep it updated by every task below
- [x] T002 [P] Create empty `src/common/salshell.h` and `src/common/salshell.cpp` (UTF-8 BOM, licence header as `src/common/salpath.cpp`) and register `common\salshell.cpp` as `<ClCompile>` in `src/vcxproj/salamand.vcxproj` and `src/vcxproj/saltests/saltests.vcxproj` (next to `salpath.cpp`); confirm `build.cmd` still links
- [x] T003 [P] Create empty `src/cmdshell.cpp` (UTF-8 BOM, includes `precomp.h`) and register it as `<ClCompile>` in `src/vcxproj/salamand.vcxproj`
- [x] T004 [P] Add `shell`, `preset`, `cmd` to `_WORDS` in `tools/translate/uicontext.py` so the new `IDC_CMDSHELL_*`/`IDS_CMDSHELL_*` symbols split into readable context words for DeepL (research R14)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the preset module, its tests, the persisted setting, and the resource ids/strings/template every story needs

**⚠️ CRITICAL**: no user story can be demonstrated until this phase is complete

- [x] T005 Declare the public API in `src/common/salshell.h` per data-model.md §2/§4 and contracts/command-shell-setting.md §2: `enum CSalShellPreset` (0 cmd … 4 git-bash, 5 custom, `sspCount`), candidate kinds (`EnvValue`, `EnvDir`, `RegString`, `RegEnum`, `PackagePath`), abstract `CSalShellProbe` (`FileExists`, `GetEnv`, `RegReadString`, `RegFirstSubKeyString`, `GetPackagePath`), `CSalShellOsProbe`, `BOOL SalGetEnvVarU8(const char* name, char* u8Buf, int bufSize)`, `BOOL SalShellLocatePreset(int preset, const CSalShellProbe* probe, char* u8Path, int pathSize)`, `const char* SalShellPresetArguments(int preset)`, `const char* SalShellPresetKey(int preset)`
- [x] T006 Implement `SalGetEnvVarU8` (`GetEnvironmentVariableW` + `SalWToU8`, WTF-8 total, FALSE when unset or too long) and `CSalShellOsProbe` in `src/common/salshell.cpp`: `FileExists` via `SalGetFileAttributes` (not a directory), `GetEnv` via `SalGetEnvVarU8`, `RegReadString` via `RegOpenKeyExW`/`RegQueryValueExW` with `ExpandEnvironmentStringsW` for `REG_EXPAND_SZ`, `RegFirstSubKeyString` via `RegEnumKeyExW`, `GetPackagePath` via `GetPackagesByPackageFamily` + `GetPackagePathByFullName` (all W APIs, UTF-8 out; no ANSI registry/env calls — encoding guard)
- [x] T007 Implement the preset table and `SalShellLocatePreset` / `SalShellPresetArguments` / `SalShellPresetKey` in `src/common/salshell.cpp` exactly per contracts/shell-presets.md §2–§3 (candidate order per preset; first existing file wins; a candidate that cannot be evaluated is skipped; Windows Terminal arguments `-d .`, all others empty; Custom → FALSE/empty)
- [x] T008 Add `TestCommandShell071()` to `src/saltests/saltests.cpp` with a `CFakeShellProbe` (in-memory file set, env map, registry map incl. subkeys, package map) covering contracts/shell-presets.md §4: per-preset candidate order and fallback, none present → not found, `EnvDir` with `%LOCALAPPDATA%` = `C:\Users\Jiří Novák\AppData\Local` kept intact, `RegEnum` picks the first subkey whose exe exists, `PackagePath` with the exe missing falls through, recipes (`-d .` only for preset 3, never `$(`), `SalGetEnvVarU8` round trip of a non-ACP value set with `SetEnvironmentVariableW`, and a real-probe smoke that presets 0 and 1 are found; register the call in `main()` after `TestEncodingFixes069();`
- [x] T009 Add `int CommandShellPreset; char CommandShellProgram[SAL_MAX_PATH_UTF8]; char CommandShellArguments[USRMNUARGS_MAXLEN];` to `struct CConfiguration` in `src/cfgdlg.h` (include `usermenu.h`/`salpath.h` as needed) and default them (`0`, `""`, `""`) in `CConfiguration::CConfiguration()` in `src/dialogs4.cpp`
- [x] T010 Add `CONFIG_CMDSHELL_PRESET_REG = "Command Shell Preset"`, `CONFIG_CMDSHELL_PROGRAM_REG = "Command Shell Program"`, `CONFIG_CMDSHELL_ARGS_REG = "Command Shell Arguments"` to the name block in `src/mainwnd2.cpp`; write them in `CMainWindow::SaveConfig` (REG_DWORD + two REG_SZ with `-1` length) next to `CONFIG_EDITNEWFILE_*`, read them in `CMainWindow::LoadConfig` (buffer sizes `SAL_MAX_PATH_UTF8` / `USRMNUARGS_MAXLEN`), and clamp an out-of-range preset to 0 after the read (contracts/command-shell-setting.md §1)
- [x] T011 Add resource ids in `src/lang/lang.rh` from `_APS_NEXT_CONTROL_VALUE` (6223) onward — `IDD_CFGPAGE_CMDSHELL`, `IDT_CMDSHELL_INTRO`, `IDT_CMDSHELL_PROGRAM`, `IDC_CMDSHELL_PRESET`, `IDT_CMDSHELL_FOUNDAT`, `IDE_CMDSHELL_FOUNDAT`, `IDT_CMDSHELL_CUSTPROG`, `IDE_CMDSHELL_CUSTPROG`, `IDB_CMDSHELL_BROWSE`, `IDT_CMDSHELL_CUSTARGS`, `IDE_CMDSHELL_CUSTARGS`, `IDT_CMDSHELL_HINT` — bump `_APS_NEXT_CONTROL_VALUE`; add the 14 `IDS_CMDSHELL_*` ids after `IDS_PLUGINCANTHANDLENAME 14197` in `src/texts.rh2` (names per contracts/configuration-page.md §5 and contracts/command-shell-setting.md §4)
- [x] T012 [P] Add the 14 English strings to `src/lang/texts.rc2` with the exact texts from contracts/configuration-page.md §5 and contracts/command-shell-setting.md §4 (E1/E2 templates with `%s`); keep `IDS_ERROREXECPROMPT` untouched
- [x] T013 [P] Add the `IDD_CFGPAGE_CMDSHELL DIALOGEX 0, 0, 299, 231` template to `src/lang/lang.rc` per contracts/configuration-page.md §1–§2 (`DS_SETFONT | DS_FIXEDSYS | DS_CONTROL | WS_CHILD | WS_CAPTION`, `CAPTION "Command Shell"`, `FONT 8, "MS Shell Dlg", 400, 0, 0x1`; intro text, combo `CBS_DROPDOWNLIST`, read-only *Found at* edit `NOT WS_TABSTOP`, group " Custom program " with program edit + `...` button + arguments edit + hint; unique `&` accelerators) and a `GUIDELINES DESIGNINFO` stub; build the language DLL (`build.cmd`) to confirm the template compiles

**Checkpoint**: `build.cmd` green (encoding guard `TOTAL: 0`), `saltests.exe` reports `0 failed` including the new checks, the setting round-trips through the registry (set it with `reg add`, start the app, exit, `reg query` shows it unchanged)

---

## Phase 3: User Story 1 - Pick a preset shell (Priority: P1) 🎯 MVP

**Goal**: the Configuration dialog gains a *Command Shell* page with the preset list; every entry point of the Command Shell command launches the chosen preset in the active panel's directory; the default (Command Prompt) is indistinguishable from 0.1.5

**Independent Test**: quickstart.md §3 (default parity across all five entry points) and §4 steps 1–7 for *Windows PowerShell*, *Windows Terminal*, *Git Bash*, incl. restart persistence and Cancel

### Implementation for User Story 1

- [x] T014 [US1] Declare `class CCfgPageCmdShell : public CCommonPropSheetPage` in `src/cfgdlg.h` (ctor; `Transfer`; `Validate`; `DialogProc`; `EnableControls()`; `UpdateFoundAt()`; members: per-preset `Found[sspCount]` + `FoundPath[sspCount][SAL_MAX_PATH_UTF8]`, `LastPreset`), add `CCfgPageCmdShell PageCmdShell;` to `CConfigurationDlg`, and in `CConfigurationDlg::CConfigurationDlg` (`src/dialogs4.cpp`) insert `/*15*/ Add(&PageCmdShell);` right after `Add(&PageHotPath);`, renumber the `/*NN*/` comments that follow, and change the `mode == 3 ? 21` literal to `22` (research R11; leave the shouted warning comments in place)
- [x] T015 [US1] Implement `CCfgPageCmdShell` in `src/dialogs4.cpp` per contracts/configuration-page.md §3 for the preset path: ctor `CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_CMDSHELL, IDD_CFGPAGE_CMDSHELL, PSP_USETITLE, NULL)`; on `WM_INITDIALOG` resolve presets 0–4 with `CSalShellOsProbe`, fill the combo with `LoadStr(IDS_CMDSHELL_PRESET_*)` in id order (+ *Custom program*), `Transfer` binds the combo to `Configuration.CommandShellPreset` (`CB_SETCURSEL`/`CB_GETCURSEL`) and the two edits via `ti.EditLine`; `UpdateFoundAt()` writes the resolved path with `SalSetDlgItemTextU8` (empty for Custom); `EnableControls()` keeps the Custom group disabled unless Custom is selected; `CBN_SELCHANGE` → both; `Transfer(ttDataToWindow)` calls both
- [x] T016 [US1] Implement `void CMainWindow::OpenCommandShell(CFilesWindow* activePanel)` (declare in `src/mainwnd.h`) in `src/cmdshell.cpp` per contracts/command-shell-setting.md §4 for presets: `UserWorkedOnThisPath = TRUE`; `SalShellLocatePreset` with the OS probe (not found → E1 placeholder handled in T027, for now the same message box); Group Policy check on the resolved path (existing `IDS_POLICIESRESTRICTION` box with Help — copy the block from `src/mainwnd3.cpp:4298-4311`); command line = `AddDoubleQuotesIfNeeded(program)` + optional `" " + SalShellPresetArguments`; `SetDefaultDirectories()`; `STARTUPINFO` exactly as today (`STARTF_USESHOWWINDOW`, `SW_SHOWNORMAL`, `MultiMonGetDefaultWindowPos` → `STARTF_USEPOSITION`, `lpTitle = LoadStr(IDS_COMMANDSHELL)`); working directory = `GetPath()` for `ptDisk`/`ptZIPArchive` else `NULL`, with the 8.3 retry when the directory's UTF-16 length ≥ `MAX_PATH - 1` (`SalGetShortPathName`, research R19); on `SalCreateProcess` failure show E2 (`IDS_CMDSHELL_ERREXEC` composed with `_snprintf_s` from **`LoadStrU8`**, path pre-trimmed with `...` beyond `2 * MAX_PATH`, `GetErrorText(err)`), caption `IDS_CMDSHELL_ERRTITLE`, via `SalMessageBoxEx` with `MSGBOXEX_HELP`, `ContextHelpId = IDD_CFGPAGE_CMDSHELL`, `HelpCallback = MessageBoxHelpCallback`; on success `HANDLES(CloseHandle)` both handles
- [x] T017 [US1] Replace the body of `case CM_DOSSHELL:` in `src/mainwnd3.cpp` (lines 4290–4356) with `OpenCommandShell(activePanel); return 0;`; verify with `grep` that no other code path launched `%COMSPEC%` for this command and that `Num /`, `Ctrl+/` (panel `src/fileswn0.cpp:1216`, command line `src/editwnd.cpp:863`), menu and toolbar still post `CM_DOSSHELL`
- [x] T018 [US1] Run `build.cmd` (guard `TOTAL: 0`) + `saltests.exe`, then execute quickstart.md §3 and §4 steps 1–7 for the three installed presets (Windows PowerShell, Windows Terminal, Git Bash) including the non-ASCII folder, UNC, archive panel, SFTP panel, restart and Cancel; record every result (and the exact windows/prompts observed) in `specs/071-configurable-command-shell/fix-log.md`

**Checkpoint**: US1 delivers the whole request for preset users; the default configuration behaves exactly as 0.1.5

---

## Phase 4: User Story 2 - Use any other program, with arguments (Priority: P2)

**Goal**: *Custom program* with a program path (Browse) and arguments; `$(FullPath)` / `$[NAME]` placeholders; pre-fill of empty Custom fields from the previous preset; syntax validation on OK

**Independent Test**: quickstart.md §5 steps 1–6 (Windows Terminal profile via pre-fill, PowerShell with `-NoExit -Command "Set-Location -LiteralPath '$(FullPath)'"` from the non-ASCII folder, drive-root quirk, empty/invalid program refused, Browse round trip)

### Implementation for User Story 2

- [x] T019 [P] [US2] Add `CommandShellArgsExpArray[]` (`FullPath`→`ExecuteExpFullPath2`, `WinDir`→`ExecuteExpWinDir2`, `SysDir`→`ExecuteExpSysDir2`, `SalDir`→`ExecuteExpSalDir2`, `{NULL, NULL}`) and `BOOL ExpandCommandShellArguments(HWND msgParent, const char* u8PanelDir, const char* varText, char* buffer, int bufferLen, BOOL ignoreEnvVarNotFoundOrTooLong)` + `BOOL ValidateCommandShellArguments(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2)` to `src/execute.cpp` / `src/execute.h` per contracts/command-shell-setting.md §3.2: `CExecuteExpData` with `Name` = panel dir + trailing `\` (or `""` when `u8PanelDir` is NULL/empty), `DosName = NULL`, `FileNameUsed = NULL`; **no** `RemoveDoubleBackslahesFromPath`
- [x] T020 [P] [US2] Switch the `$[ENV]` read in `DoExpandVarString` (`src/salamdr2.cpp:944`, `GetEnvironmentVariable(envVar, buf, MAX_PATH)`) to `SalGetEnvVarU8(envVar, buf, sizeof(buf))` keeping the existing not-found / too-long error handling and messages (research R9); add an English comment naming feature 071
- [x] T021 [P] [US2] Add `BOOL SafeGetOpenFileNameW(OPENFILENAMEW* lpofn)` to `src/salamdr6.cpp` as the mirror of `SafeGetSaveFileNameW` (same retry-on-problem-path logic) and declare it in `src/consts.h` next to `SafeGetSaveFileNameW`
- [x] T022 [US2] Extend `CCfgPageCmdShell` in `src/dialogs4.cpp` per contracts/configuration-page.md §3–§4: `EnableControls()` enables the Custom group only for Custom; `CBN_SELCHANGE` pre-fills **both empty** Custom edits from `LastPreset`'s resolved path and recipe when switching to Custom and never overwrites non-empty text; `LastPreset` updated only when a preset is chosen; `IDB_CMDSHELL_BROWSE` handler = wide browse (`SalGetDlgItemTextU8` → `SalU8ToW` → `OPENFILENAMEW` with `LoadStrW(IDS_EXEFILTER)` double-NUL filter, title `IDS_CMDSHELL_BROWSETITLE`, `OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY` → `SafeGetOpenFileNameW` → `SalWToU8` → `SalGetFullName` → `SalSetDlgItemTextU8`); `Validate` for Custom: blank program → `IDS_CMDSHELL_PROGRAMREQUIRED` + `ErrorOn`; `ValidateCommandFile` / `ValidateCommandShellArguments` failure → `IDS_INCORRECTSYNTAX`, `EM_SETSEL(errorPos1, errorPos2)`, `ErrorOn`
- [x] T023 [US2] Add the Custom branch to `CMainWindow::OpenCommandShell` in `src/cmdshell.cpp`: program = `ExpandCommand(HWindow, Configuration.CommandShellProgram, buf, SAL_MAX_PATH_UTF8, FALSE)`, arguments = `ExpandCommandShellArguments(HWindow, panelDir-or-NULL, Configuration.CommandShellArguments, …)`; a FALSE from either ends the command silently (the engine already reported); policy check and launch as for presets; verify the total command line stays under `SALCMDLINE_MAXLEN` (`GetCmdLineLimit()`), otherwise show `IDS_TOOLONGPATH`-style error (reuse `IDS_USRMNUTOOLONGCMDORARGS`)
- [x] T024 [US2] Run `build.cmd` + `saltests.exe`, then execute quickstart.md §5 steps 1–6 (pre-fill from Windows Terminal, kept text, `$[SystemRoot]` program, quoted `$(FullPath)` from the non-ASCII folder and from `C:\`, empty/`$(Bogus)` refused, Browse to `git-bash.exe` and from a non-ASCII folder) and record results in `specs/071-configurable-command-shell/fix-log.md`

**Checkpoint**: US1 and US2 both work; a preset user is unaffected by the Custom code paths

---

## Phase 5: User Story 3 - Know when the chosen program is not available (Priority: P3)

**Goal**: presets not installed are marked and cannot be confirmed; the dialog shows which installation a preset resolves to; a program that cannot be started yields an actionable error (program named, reason, pointer to the setting, Help button to the topic) — never a silent no-op or a fallback program

**Independent Test**: quickstart.md §4 (PowerShell 7 "not found" on this machine) and §5 step 7 (`C:\nowhere\shell.exe`, uninstalled preset)

### Implementation for User Story 3

- [x] T025 [US3] In `CCfgPageCmdShell` (`src/dialogs4.cpp`): append `LoadStr(IDS_CMDSHELL_NOTFOUND)` to combo items whose preset was not found; `UpdateFoundAt()` shows `IDS_CMDSHELL_NOTFOUNDTEXT` for them; `Validate` refuses a not-found preset with `IDS_CMDSHELL_PRESETNOTFOUND` + `ErrorOn(IDC_CMDSHELL_PRESET)` (contracts/configuration-page.md §3); when the dialog opens with a stored preset that is now not found, the combo still shows it selected (marked) so the user must change it before OK
- [x] T026 [US3] In `CMainWindow::OpenCommandShell` (`src/cmdshell.cpp`): a preset that `SalShellLocatePreset` cannot find shows E1 — `IDS_CMDSHELL_ERRNOTFOUND` composed with `LoadStrU8` and the preset's display name (`LoadStrU8(IDS_CMDSHELL_PRESET_*)` via a small id → string-id map shared with the page), caption `IDS_CMDSHELL_ERRTITLE`, `SalMessageBoxEx` with `MSGBOXEX_HELP` / `ContextHelpId = IDD_CFGPAGE_CMDSHELL` / `MessageBoxHelpCallback`; confirm E2 (T016) uses the same wiring and that neither path launches anything else
- [x] T027 [US3] Run `build.cmd` + `saltests.exe`, then execute quickstart.md §4 "PowerShell 7 (not found)" (marked item, OK refused, combo focused) and §5 step 7 (nonexistent Custom program → E2 names the path and the system reason; temporarily rename `git-bash.exe` or point a test `HKCU\Software\GitForWindows\InstallPath` at an empty folder → E1 at launch and "(not found)" on reopening the dialog); record results in `specs/071-configurable-command-shell/fix-log.md`

**Checkpoint**: all three stories independently verified; no path in the feature ends silently

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: translations, manual, hygiene, full validation, release record

- [x] T028 Translation refresh (research R14, memory: `.slt` import is positional): `src\vcxproj\build_langs.cmd --export-templates --module salamand` → `cd tools` → `python -m translate.merge --module salamand --dry-run` (record gap count and DeepL cost in fix-log) → `python -m translate.merge --module salamand` → `python -m translate.slt --verify` → check *Windows PowerShell* / *PowerShell 7* / *Windows Terminal* / *Git Bash* survived in `translations/czech/salamand.slt` and one more language; pin any altered product name in `translations/ui-overrides.json` (`salamand` section) and re-run `merge`; commit the 8 regenerated `.slt` files and `.origin` sidecars
- [x] T029 [P] Write `help/src/hh/salamand/configuration_cmdshell.htm` (UTF-8 BOM, skeleton of `configuration_usrmn.htm`; title "Command Shell configuration"; breadcrumb "Tandem Commander: Dialog Boxes: Configuration"; `<dl>` describing every control from contracts/configuration-page.md §2, the pre-fill behaviour, the `$(FullPath)`/`$[NAME]` placeholders incl. the drive-root quoting quirk, the not-found marking; *See Also* → `othertask_shell.htm`, `customize_usrmn.htm`; footer `&#169; 2026 Pavel Stupka` per the CLAUDE.md copyright rule, research R15)
- [x] T030 [P] Register the topic: add `IDD_CFGPAGE_CMDSHELL=hh\salamand\configuration_cmdshell.htm` to `[ALIAS]` in `help/src/salamand.hhp` (with the other `IDD_CFGPAGE_*` lines), a sitemap entry "Command Shell" in the Configuration branch of `help/src/salamand.hhc` (alphabetical: after "Colors", before "Confirmations") and an index entry in `help/src/salamand.hhk`
- [x] T031 [P] Update `help/src/hh/salamand/othertask_shell.htm`: state that the program is chosen on the *Command Shell* configuration page (link), that it opens in the active panel's directory (for an archive: the folder containing the archive), replace the "UNC paths are not supported" remark with "Command Prompt itself does not accept a network path as its working directory and falls back to the Windows directory; PowerShell, Windows Terminal and Git Bash do", mention the Windows starting-directory length limit and the Windows 11 default-terminal behaviour; keep the existing footer
- [x] T032 Source hygiene: `python tools\check_encoding.py --strict` → `TOTAL: 0`; `python tools\check_encoding.py --draft --format list | findstr /i "cmdshell salshell execute.cpp"` → no new findings; run `clang-format -i` (repo `.clang-format`) over `src/common/salshell.h`, `src/common/salshell.cpp`, `src/cmdshell.cpp` and the edited regions of the other files (`normalize.ps1` needs PowerShell 7 — not installed; note in fix-log); confirm every new/edited file is UTF-8 with BOM; new comments in English
- [x] T033 Language and appearance check (quickstart.md §6): `build.cmd full`, switch the UI to Czech and one more language — tree label, controls, combo items, "(not found)" suffix, validation and launch messages translated; Alt+accelerators unique; page renders in light and dark theme like *Hot Paths*; record in fix-log
- [x] T034 Full quickstart.md pass (§1–§7) on the final build incl. the long-directory case (§4 step 6b) and the export → import round trip (§4 step 7); the manual compile (§7) is recorded as "not compiled here" unless HTML Help Workshop is available; every result in `specs/071-configurable-command-shell/fix-log.md`; write `specs/071-configurable-command-shell/closing-report.md` (what shipped, deviations, the four follow-ups from research.md "Open follow-ups")
- [x] T035 [P] Add the feature-071 entry to `CLAUDE.md` "Recent Changes" (setting, presets, `src/common/salshell.*`, the `$[ENV]` wide read, the configuration-page insertion point and the `mode` literal change, the help-page branding decision) — the `update-agent-context` script does not persist edits (memory: env-tooling-quirks), so edit by hand
- [ ] T036 Ship gate (release only, constitution "Release Documentation"): CHANGELOG *Added* entry in the user's terms ("the Command Shell command (Num /) can open Windows Terminal, PowerShell 7, Windows PowerShell, Git Bash or any program of your choice; the default is unchanged") + *Fixed* note for the `$[ENV]` expansion of non-ANSI values, together with the version/build bump in `src/plugins/shared/spl_vers.h` (`VERSINFO_SALAMANDER_MINORB`, `VERSINFO_BUILDNUMBER` + comment), `setup/tandemcommander.iss` `MyAppVersion`, and the `CLAUDE.md` version line — one change; `LAST_VERSION_OF_SALAMANDER` (106) untouched; note that feature 070's entry is still pending its own gate and would share the version

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies; T002–T004 in parallel
- **Foundational (Phase 2)**: T005 → T006 → T007 → T008 (module chain); T009 → T010 (config); T011 → T012, T013 (ids before strings/template). The config chain and the resource chain are independent of the module chain. **Blocks all stories.**
- **US1 (Phase 3)**: needs all of Phase 2; T014 → T015; T016 → T017; T015/T016 can be written in parallel (different files) — T018 after both
- **US2 (Phase 4)**: needs US1's page (T015) and launch (T016); T019, T020, T021 in parallel (different files) → T022 (page) and T023 (launch) in parallel → T024
- **US3 (Phase 5)**: needs US1's page and launch; T025 (page) and T026 (launch) in parallel → T027
- **Polish (Phase 6)**: T028 only after every string/template edit is final (T012/T013 and any fix from T018/T024/T027); T029–T031 in parallel any time after Phase 2; T032 after all code; T033 after T028; T034 after T032/T033; T035 with T034; T036 last, release only

### User Story Dependencies

- **US1 (P1)**: the MVP — presets end-to-end; no dependency on US2/US3
- **US2 (P2)**: extends US1's page and launch function with the Custom branch; independently testable through quickstart §5
- **US3 (P3)**: extends US1's page (marking/refusal) and launch (E1); independently testable through the PowerShell 7 not-found case on this machine

### Parallel Opportunities

- Phase 1: T002 ‖ T003 ‖ T004
- Phase 2: (T005→T006→T007→T008) ‖ (T009→T010) ‖ (T011→(T012 ‖ T013))
- Phase 3: T015 ‖ T016
- Phase 4: T019 ‖ T020 ‖ T021, then T022 ‖ T023
- Phase 5: T025 ‖ T026
- Phase 6: T029 ‖ T030 ‖ T031 ‖ T035; T028 sequential (network + build)

---

## Parallel Example: Foundational phase

```text
# three independent chains, one agent each:
A: T005 salshell.h API → T006 OS probe + SalGetEnvVarU8 → T007 preset table + locate → T008 saltests
B: T009 CConfiguration fields + defaults → T010 registry names + Save/Load
C: T011 resource ids → T012 English strings ‖ T013 dialog template
# join: build.cmd + saltests.exe + registry round trip (Phase 2 checkpoint)
```

## Parallel Example: User Story 2

```text
T019 execute.cpp argument table   ‖   T020 salamdr2.cpp $[ENV] wide read   ‖   T021 SafeGetOpenFileNameW
        ↓ then
T022 page: Custom controls, Browse, pre-fill, Validate   ‖   T023 launch: Custom branch
        ↓ then
T024 build + saltests + quickstart §5 + fix-log
```

---

## Implementation Strategy

### MVP First (User Story 1 only)

1. Phase 1 + Phase 2 (module with tests, persisted setting, resources)
2. Phase 3 — the page with presets and the new launch function
3. **STOP and VALIDATE** with quickstart §3 (parity) and §4 — this already delivers everything the request asked for (choose Windows Terminal / Git Bash / PowerShell instead of cmd)
4. Translations (T028) can be run at this point if an MVP build is to be shown in a non-English UI

### Incremental Delivery

1. + US2 (Custom + placeholders + Browse + pre-fill) → quickstart §5
2. + US3 (not-found marking, E1) → quickstart §4/§5.7
3. Polish: translations, manual, hygiene, full pass, closing report; ship gate when a version is cut

### Notes

- Every phase's last task writes to `fix-log.md`; a ticked box in this file means "verified against quickstart", not "code written" (lesson recorded in feature 070).
- Do not run `build.cmd full` between T012/T013 and T028 — the language build fails by design until the `.slt` refresh; use plain `build.cmd` (English DLL) until then.
- Never touch `src/plugins/shared/`; interface version 106 stays.

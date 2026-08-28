# Quickstart: validating the Configurable Command Shell

**Feature**: 071 · **Contracts**: [contracts/](contracts/) ·
**Data model**: [data-model.md](data-model.md)

This is the validation guide — what to run and what must happen — not an
implementation walk-through. Every step maps to a spec requirement or success
criterion (in brackets).

## Prerequisites

- Windows 11, VS2022 C++ workload, Python on `PATH` (the encoding guard is
  mandatory: `build.cmd` fails without it).
- `temp\deepl_key.txt` present (translation refresh); `pip install -e tools` once.
- Test machine state on 2026-08-28 (Pavel's PC): Windows Terminal — **present**
  (`%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe`), Windows PowerShell —
  present, Git Bash — present (`C:\Program Files\Git\git-bash.exe`),
  **PowerShell 7 — absent** (the real "not found" case), `COMSPEC` =
  `C:\WINDOWS\system32\cmd.exe`. HTML Help Workshop (`hhc.exe`) is **not**
  installed — the `.chm` compile step must run elsewhere or be skipped with a
  note.
- Test folders: `D:\Work` (plain), a folder with spaces + non-ASCII
  (`G:\Můj disk\Nový projekt` or any `…\Test ěščř\`), a UNC share, a ZIP archive
  entered in a panel, an SFTP/FTP plugin panel.

## 1. Build and unit tests

```bat
build.cmd                                   :: Debug x64 incremental; runs tools\check_encoding.py --strict
%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\saltests\saltests.exe
```

Expected: build green with `Encoding guard` reporting `TOTAL: 0`; `saltests`
ends with `saltests: N checks, 0 failed` where N ≥ 1301 + the new
`TestCommandShell071()` checks (exit code 0). The new test covers, with the fake
probe: each preset's candidate order and first-found rule, the not-found result,
`EnvDir` with a non-ASCII `%LOCALAPPDATA%`, `RegEnum` picking the first subkey
whose exe exists, and `SalGetEnvVarU8` round-tripping a non-ACP value; with the
real probe: `cmd` and `powershell` are found on any Windows.

Also run the report-only guard once and confirm no new `missed-twin` /
`ansi-api-on-utf8-path` hits mention `cmdshell`/`salshell`:

```bat
python tools\check_encoding.py --draft --format list | findstr /i "cmdshell salshell"
```

## 2. Translation refresh (once per resource change)

```bat
src\vcxproj\build_langs.cmd --export-templates --module salamand
cd tools
python -m translate.merge --module salamand --dry-run      :: shows gaps (≈14 strings + 1 dialog × 8 languages) and DeepL cost
python -m translate.merge --module salamand
python -m translate.slt --verify
cd ..
build.cmd full                                             :: builds all 8 .slg; fails loudly if a .slt is short
```

Expected: `merge` reports the new rows only (existing entries untouched,
`human: 0` is normal), `slt --verify` passes, `build.cmd full` produces
`lang\<lang>.slg` for the 8 enabled languages. Check the four product names
(*Windows PowerShell*, *PowerShell 7*, *Windows Terminal*, *Git Bash*) survived
in `translations/czech/salamand.slt` (and a second language); pin in
`translations/ui-overrides.json` and re-run `merge` if not [FR-014, SC-007].

## 3. Default behaviour is unchanged (regression gate) [FR-003, SC-002]

Fresh configuration (or a registry export from 0.1.5 imported): in `D:\Work`,
press `Num /`, `Ctrl+/` (panel focus), `Ctrl+/` (command line focus), *Commands →
Command Shell*, and click the toolbar button. Each time a Command Prompt opens at
`D:\Work>` with the same window placement as 0.1.5 (compare side by side with a
0.1.5 build on a second monitor: the new window appears on the main window's
monitor). Open Configuration → *Command Shell*: *Command Prompt* is selected and
*Found at* shows `C:\WINDOWS\system32\cmd.exe`.

## 4. Presets [US1, FR-002, FR-004–FR-007, SC-001, SC-003, SC-004]

For each of *Windows PowerShell*, *Windows Terminal*, *Git Bash*:

1. Configuration → *Command Shell* → select the preset → *Found at* shows its
   path → OK. Stopwatch: under 30 s, ≤ 5 interactions [SC-001].
2. In `D:\Work`: `Num /` → the program opens **in `D:\Work`** (PowerShell prompt
   `PS D:\Work>`; Windows Terminal's tab at `D:\Work`, *not* the user profile;
   Git Bash prompt `/d/Work`).
3. Repeat from the folder with spaces + non-ASCII characters: the program's
   current directory is exactly that folder (PowerShell: `Get-Location`;
   Git Bash: `pwd`; cmd: the prompt) [FR-006, FR-016, SC-003].
4. Repeat via `Ctrl+/` (panel), `Ctrl+/` (command line), menu, toolbar — same
   result each time [SC-004].
5. From a UNC path: PowerShell / Windows Terminal / Git Bash open **in** the UNC
   path; *Command Prompt* prints its own "UNC paths are not supported" note and
   falls back to the Windows directory — as documented [edge case].
6. From a panel showing a ZIP archive: the program opens in the folder that
   contains the archive; from an SFTP/FTP panel: it opens with no working
   directory set (same as 0.1.5) [FR-005].
6b. From a folder whose full path is ≥ 259 characters (create one with
   `New-Item` in PowerShell; `LongPathsEnabled` on): on a volume with 8.3
   names the program opens in that folder (short form used); on a volume
   without them the launch error names the program and reports "The directory
   name is invalid" — never a silent no-op [edge case, R19].
7. Restart the application: `Num /` still opens the chosen program [FR-009,
   SC-006]. Change the selection and press *Cancel*: the previous program still
   opens [US1-6].

*PowerShell 7* on this machine: the combo item reads "PowerShell 7 (not
found)"; selecting it and pressing OK is refused with the message and the combo
focused [US3-1, FR-007]. (On a machine with PowerShell 7: *Found at* shows the
`InstallLocation` from `HKLM\SOFTWARE\Microsoft\PowerShellCore\InstalledVersions`
and the prompt opens at the panel directory.)

## 5. Custom program [US2, FR-008, FR-010, SC-005]

1. Select *Windows Terminal*, then switch to *Custom program* while both Custom
   fields are empty → the program edit shows the `wt.exe` alias path and the
   arguments edit shows `-d .` [US2-7]. Append ` -p "Windows PowerShell"` → OK →
   `Num /` opens Windows Terminal with that profile in the panel directory.
2. Select a preset, then switch back to *Custom*: your edited text is still
   there [US2-8].
3. Program `$[SystemRoot]\System32\WindowsPowerShell\v1.0\powershell.exe`,
   arguments `-NoExit -Command "Set-Location -LiteralPath '$(FullPath)'"` → from
   the folder with spaces + non-ASCII: PowerShell lands in that folder [US2-1,
   US2-4, US2-6].
4. Arguments `$(FullPath)` from a drive root (`C:\`): the program receives
   `C:\` — a quoted `"$(FullPath)"` would receive `C:"` (Windows quoting rule,
   documented) [edge case].
5. Empty program + OK → refused, message shown, program edit focused [US2-2].
   Program `$(Bogus)` + OK → *Incorrect syntax* with the bad range selected.
6. Browse (`...`) → pick `C:\Program Files\Git\git-bash.exe` → the full path
   appears in the edit; do the same from a folder whose name has non-ASCII
   characters — the path is intact [US2-3, FR-016].
7. Program `C:\nowhere\shell.exe` → OK (accepted: existence is not checked) →
   `Num /` → an error box names `C:\nowhere\shell.exe`, quotes the system
   reason ("The system cannot find the file specified"), mentions Configuration /
   Command Shell, and its *Help* button opens the *Command Shell* configuration
   topic (when the `.chm` is deployed) — nothing else opens [US3-2, FR-010,
   SC-005]. Uninstall/rename a preset's exe (or point `InstallPath` elsewhere in
   a test registry) → `Num /` shows the "not found" error instead.
8. Group Policy (optional, admin): with `RestrictRun` listing only `notepad.exe`,
   any preset shows the existing policy message [FR-011].

## 6. Languages and appearance [FR-014, SC-007, constitution VI]

Switch the UI language to Czech (and one more): the *Command Shell* tree
label, every control text, the combo items, the "(not found)" suffix, the
validation and launch error messages are translated; accelerators do not clash
(Alt+letter reaches the right control); the page renders in light and dark
theme like its neighbours (`CCommonPropSheetPage` theming).

## 7. Manual [FR-015]

- `help/src/hh/salamand/configuration_cmdshell.htm` exists, is UTF-8 with BOM,
  and describes every control; `salamand.hhp` `[ALIAS]` maps
  `IDD_CFGPAGE_CMDSHELL` to it; `salamand.hhc`/`.hhk` list it.
- `othertask_shell.htm` mentions the setting, the archive-folder rule and states
  the UNC/long-path behaviour as the chosen program's own.
- On a machine with HTML Help Workshop: `help\src\compileall.bat` → no new
  warnings/errors in `result.log`; F1 on the page and the error box's *Help*
  open the new topic. (Not available on the development PC — record "not
  compiled here" in the fix-log rather than claiming it.)

## 8. Ship gate (release only)

CHANGELOG *Added* entry + `spl_vers.h` (`VERSINFO_SALAMANDER_MINORB`,
`VERSINFO_BUILDNUMBER`), `setup/tandemcommander.iss` `MyAppVersion`, `CLAUDE.md`
version line — one change. `LAST_VERSION_OF_SALAMANDER` (106) untouched.

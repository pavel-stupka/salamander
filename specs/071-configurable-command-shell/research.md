# Research: Configurable Command Shell (Phase 0)

**Feature**: 071 · **Date**: 2026-08-28 · **Inputs**: [spec.md](spec.md),
constitution v3.1.0, four read-only research passes over the codebase and the
terminal programs' vendor documentation.

Every open question a plan of this shape would normally carry is
resolved below; the decisions are binding for [data-model.md](data-model.md),
[contracts/](contracts/) and `tasks.md`.

## Where today's behaviour actually is

- One handler: `case CM_DOSSHELL` in `src/mainwnd3.cpp:4290-4356`. All entry
  points converge on it — the `Num /` accelerator (`src/salamand.rc:125`,
  `IDA_MAINACCELS2`, **skipped while the command line box has focus**), `Ctrl+/`
  in the panel (`src/fileswn0.cpp:1216`) and in the command line box
  (`src/editwnd.cpp:863`), the menu item (`src/menu4.cpp:144`, always enabled,
  no icon) and the toolbar button (`src/toolbar4.cpp:172`, persistence key
  `"CommandShell"`).
- It reads `%COMSPEC%` with the **ANSI** `GetEnvironmentVariable`, runs the
  Group Policy check on the unquoted path (`GetMyCanRun` compares the leaf name,
  case-insensitively), quotes it, calls `SetDefaultDirectories()`, fills
  `STARTUPINFO` (`STARTF_USESHOWWINDOW`, `SW_SHOWNORMAL`, `STARTF_USEPOSITION`
  when the main window is not on the primary monitor) and calls
  `SalCreateProcess` with the working directory = `activePanel->GetPath()` for
  `ptDisk`/`ptZIPArchive`, `NULL` otherwise. On failure: `SalMessageBox`
  with `GetErrorText(err)` and the caption `IDS_ERROREXECPROMPT` — the program
  is not named.
- **`si.lpTitle` is set but never forwarded**: `SalCreateProcess`
  (`src/common/salfileio.cpp:141-155`) copies only the numeric `STARTUPINFO`
  fields (documented in `salfileio.h:60-62`). The "Command Shell" title has not
  applied since feature 004. Same for the two other `lpTitle` sites.
- `GetPath()` of an archive panel is **the folder containing the archive**
  (`src/fileswn2.cpp:2127-2128`, `SetPath` at `:2249`), not the archive file;
  for a plugin FS panel `Path` is stale, which is why the handler passes `NULL`.
- The command line box has its **own** launch path (`src/editwnd.cpp:407-530`,
  `%COMSPEC% /C|/K "<typed line>"`); it is out of scope (FR-012).

## Decisions

### R1 — Persist three values under the existing `Configuration` key

**Decision**: `Command Shell Preset` (REG_DWORD), `Command Shell Program`,
`Command Shell Arguments` (REG_SZ, UTF-8 through the W8 facade), fields
`CommandShellPreset` / `CommandShellProgram[SAL_MAX_PATH_UTF8]` /
`CommandShellArguments[USRMNUARGS_MAXLEN]` in `CConfiguration`, defaults in the
constructor, `SetValue`/`GetValue` lines in `SaveConfig`/`LoadConfig`. No
`THIS_CONFIG_VERSION` bump.
**Rationale**: exactly the `UseEditNewFileDefault` + `EditNewFileDefault` shape
(`src/cfgdlg.h:406-407`, `src/mainwnd2.cpp:1745-1748` / `3305-3308`); a missing
value keeps the default, as `Theme Mode` (feature 028) proved; *Export
Configuration* copies the whole branch (`src/salamdr2.cpp:2890-2923`, only
`*.hidden` names are dropped) so nothing else is needed for FR-009.
**Alternatives**: a single "command line" string (loses the preset semantics,
detection and pre-fill); a dedicated subkey (no benefit, more code).

### R2 — Stable preset ids, Custom = 5

**Decision**: `enum CSalShellPreset { 0 cmd, 1 powershell, 2 pwsh, 3 wt,
4 git-bash, 5 custom }`, append-only. Out-of-range on load → 0.
**Rationale**: the id is what the registry stores and what an exported
configuration carries between machines.

### R3 — Pure preset logic in `src/common/salshell.*` behind a probe

**Decision**: new `src/common/salshell.h/.cpp` (preset table, candidate kinds,
`SalShellLocatePreset`, `SalShellPresetArguments`, `SalGetEnvVarU8`, the OS
probe), compiled into `salamand.vcxproj` **and** `saltests.vcxproj`; the locate
algorithm depends only on an abstract `CSalShellProbe` (file exists /
environment / registry string / first-subkey string).
**Rationale**: `saltests` links `src/common/*.cpp` only (no main-application
sources, `src/vcxproj/saltests/saltests.vcxproj`), so the rules "which
locations, in which order, with which fallbacks" are testable only there; the
probe makes the tests deterministic on any machine (fake file set / env /
registry), and the real probe is a thin W-API wrapper.
**Alternatives**: code in `mainwnd3.cpp` (untestable, grows a 4,000-line
switch); header-only (the OS probe needs a `.cpp` anyway); calling `SearchPath`
(no house wrapper exists and it is on the guard's ANSI blacklist).

### R4 — Keep `SalCreateProcess`, build `"program" args`

**Decision**: `lpApplicationName = NULL`, command line = the program quoted by
`AddDoubleQuotesIfNeeded` (on the program alone) + optional arguments; flags,
`STARTUPINFO`, `SetDefaultDirectories()` and the working-directory rule are
**unchanged**; the policy check runs on the resolved program path.
**Rationale**: byte-for-byte parity with today for preset 0 (SC-002) and the
same `STARTF_USEPOSITION` multi-monitor behaviour; `CreateProcessW` resolves an
App Execution Alias (`wt.exe`) like any executable, which is how `cmd` runs `wt`.
**Alternatives**: `SalShellExecuteEx` as the User Menu's direct mode uses
(handles `.cpl`/`.scr`, irrelevant for terminals; loses `STARTUPINFO` position
parity and the identical policy semantics).

### R5 — Placeholders: reuse `ExpandVarString`; a dedicated argument table

**Decision**: program field → existing `ExpandCommand` (`src/execute.cpp:1929`:
`$(WinDir)`, `$(SysDir)`, `$(SalDir)`, `$[ENV]`, self-contained, already used by
external viewers/editors and packers); arguments field → new
`CommandShellArgsExpArray` + `ExpandCommandShellArguments` /
`ValidateCommandShellArguments` in `src/execute.cpp`, `$(FullPath)` bound to
`ExecuteExpFullPath2` (no trailing backslash, drive root keeps `C:\`),
`$(WinDir)/$(SysDir)/$(SalDir)` bound to the `*2` (no-backslash) executors,
`Name` = panel directory + `\`, **no** `RemoveDoubleBackslahesFromPath` pass.
**Rationale**: the User Menu *Arguments* `$(FullPath)` (`ExecuteExpFullPath`)
always ends with a backslash — `"$(FullPath)"` in quotes then reaches the
program as `D:\Work"` under Windows argument parsing, which would break US2
scenario 6 for every directory; the *Initial Directory* variant already has the
right meaning. `ExpandInitDir` cannot be reused as-is because its
double-backslash collapse skips only the first two characters and would mangle a
UNC path appearing later in an argument string; `ExpandArguments` requires a
file name and exposes file-only variables. Spec FR-008 was refined accordingly
(2026-08-28).
**Alternatives**: a bespoke mini-expander (duplicates validation, error boxes and
`$[ENV]` handling the engine already has); quote-aware doubling of a trailing
backslash (clever, undocumented, still wrong for programs that use different
parsing rules).

### R6 — Preset locate strategies and launch recipes

**Decision**: presets never use placeholders; each is launched with the working
directory set to the panel directory and the arguments fixed by
[contracts/shell-presets.md](contracts/shell-presets.md) — `cmd`, `powershell`,
`pwsh` and `git-bash` with **no** arguments (all honour the process working
directory), Windows Terminal with `-d .` (its profile `startingDirectory`
defaults to `%USERPROFILE%` and would otherwise win; `.` is resolved against the
`wt.exe` process's working directory and avoids the trailing-backslash quirk of
`-d "C:\"`, which fails with `0x8007010B`). Locate order per preset (full
candidate tables in the contract): Command Prompt = `%COMSPEC%` →
`%SystemRoot%\System32\cmd.exe`; Windows PowerShell =
`%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe`; PowerShell 7 =
`%ProgramFiles%\PowerShell\7\pwsh.exe` → the MSIX alias
`%LOCALAPPDATA%\Microsoft\WindowsApps\pwsh.exe` → its per-family alias →
`HKLM\SOFTWARE\Microsoft\PowerShellCore\InstalledVersions\*\InstallLocation` →
`App Paths\pwsh.exe`; Windows Terminal = the App Execution Alias
`%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe` → its per-family alias → the
package path via `GetPackagesByPackageFamily`; Git Bash = `HKCU` then `HKLM`
(+ `WOW6432Node`) `Software\GitForWindows\InstallPath` → the `Git_is1`
uninstall entry → `%ProgramFiles%\Git` → `%LOCALAPPDATA%\Programs\Git`, each +
`git-bash.exe`.
**Rationale**: verified against vendor documentation and source, plus local
`CreateProcessW` experiments (App Execution Aliases resolve natively; `wt.exe`
is a shim that forwards its working directory; `git-bash.exe` passes its
working directory to mintty unless `--cd-to-home`; Windows PowerShell and pwsh
start in the inherited directory). PowerShell 7's MSI — and with it the
registry key — is being phased out (winget installs MSIX from 7.6, no MSI from
7.7), so alias/MSIX candidates come before the registry, in the order Windows
Terminal's own profile generator uses. Anything else is *Custom*.
**Alternatives**: scanning `PATH` (no house wrapper; `SearchPath` is on the
guard's blacklist; `git-bash.exe` is not on `PATH` at all); `-WorkingDirectory`
for pwsh (treats `[`/`]` as wildcards, runs after the profile); `-w new` for
Windows Terminal (would override the user's `windowingBehavior`; the captured
directory is honoured even in an existing window); launching through `start`
(loses the process handle and the policy check).

### R7 — Working-directory rule stays exactly as it is

**Decision**: `ptDisk`/`ptZIPArchive` → `GetPath()` (the archive's folder),
plugin FS → `NULL`; `$(FullPath)` for a plugin FS panel expands to `""`.
**Rationale**: spec edge case "archive or plugin file system: unchanged"; the
manual will *describe* the archive case ("the folder that contains the archive")
because the research showed nobody had written it down.

### R8 — `lpTitle` stays un-forwarded

**Decision**: keep passing `si.lpTitle` for parity but do **not** teach
`SalCreateProcess` to forward it.
**Rationale**: forwarding would retitle every console the product launches
(command line box, User Menu through shell, Command Shell) for every user — a
behaviour change outside this feature's opt-in setting (constitution II). Logged
in the closing notes as a candidate for its own small decision.

### R9 — Environment reads go wide

**Decision**: new `SalGetEnvVarU8(name, buf, size)` in `src/common/salshell.*`
(`GetEnvironmentVariableW` + `SalWToU8`, WTF-8 total); the OS probe uses it, the
Command Prompt preset reads `COMSPEC` through it, and the single shared `$[ENV]`
read in `DoExpandVarString` (`src/salamdr2.cpp:944`, ANSI) is switched to it.
**Rationale**: FR-016 — this page documents `$[NAME]` as a feature, and a value
outside the ANSI code page (a user profile path with non-ACP characters) was
lossy; the 068 review already listed the ANSI `GetEnvironmentVariable` reads as
"output-side producers reviewed by hand". The change is one call site in the
shared engine plus new code; the other `COMSPEC` reads (command line box, User
Menu `.bat` path) are out of scope and untouched.
**Alternatives**: leave ANSI (fails FR-016 for exactly the users the feature
targets — accented Windows user names are common in this product's locales).

### R10 — Browse needs a wide open-file dialog

**Decision**: add `SafeGetOpenFileNameW` (`src/salamdr6.cpp`, `src/consts.h`)
as the mirror of the existing `SafeGetSaveFileNameW`, and a page-local Browse
handler following `BrowseFileName` (`src/dialogs.cpp:1815-1846`): wide
`OPENFILENAMEW`, `IDS_EXEFILTER`, `SalWToU8` → `SalGetFullName` →
`SalSetDlgItemTextU8`.
**Rationale**: the only executable browser today, `BrowseCommand`
(`src/execute.cpp:2132`), is `GetOpenFileNameA` and would garble a non-ACP path;
FR-016 forbids that on this page.
**Alternatives**: reuse `TrackExecuteMenu` (the User Menu's variable pop-up,
built on the ANSI browser); a directory picker (wrong control for an exe).

### R11 — Page placement: top-level, right after *Hot Paths*

**Decision**: `Add(&PageCmdShell)` after `PageHotPath` → index 15; renumber the
`/*NN*/` comments; change the `mode == 3 ? 21` literal to `22`
(`src/dialogs4.cpp:681-686`); accept that `Configuration.LastFocusedPage` values
≥ 15 point one page further after the upgrade (cosmetic, one-time).
**Rationale**: thematically the page belongs with *User Menu* and *Hot Paths*
(programs and places the user launches); of the six literals in the mode map
only one is ≥ 15, so the blast radius the source warns about ("in 1.6b2 this
fooled me") is a single, documented edit.
**Alternatives**: appending after the *Archivers* group (no renumbering, but the
page lands at the bottom of an otherwise thematic tree); a child of *Viewers and
Editors* (wrong group, and its expander state is persisted); right after
*System* (five literals shift).

### R12 — Not-found presets are refused on OK; a Custom program is not existence-checked

**Decision**: the page marks not-found presets " (not found)", shows the
resolved path of a found one in a read-only edit, and `Validate` refuses OK on a
not-found preset (spec US3-1, FR-007). For *Custom* only blankness and
placeholder syntax are validated; existence is checked at launch (E2).
**Rationale**: `ValidatePathIsNotEmpty` (`src/dialogs4.cpp:121-140`) records
that an earlier existence check was deliberately dropped ("network issues");
a Custom path may legitimately be on a share that is offline while configuring.

### R13 — Error messages name the program and carry a Help button to the topic

**Decision**: new `IDS_CMDSHELL_ERRTITLE`, `IDS_CMDSHELL_ERRNOTFOUND`,
`IDS_CMDSHELL_ERREXEC` (see the setting contract), composed with `_snprintf_s`
from `LoadStrU8` templates into `char buff[4 * MAX_PATH]`, shown via
`SalMessageBoxEx` with `MSGBOXEX_HELP`, `ContextHelpId = IDD_CFGPAGE_CMDSHELL`
and `MessageBoxHelpCallback` — the same wiring the policy box uses. The existing
`IDS_ERROREXECPROMPT` caption stays defined (unused) so no translation row moves.
**Rationale**: `GetErrorText` returns UTF-8 (documented in the 068 report) and a
`sprintf(LoadStr(...), path, GetErrorText())` is the guard's `mixed-composition`
shape; the help project's `[MAP]` includes `lang.rh`, so a dialog id is a valid
help context and lands on the new topic.
**Alternatives**: a custom "Configure…" button (`AliasBtnNames` + return-code
handling + opening the dialog on the right page — more code for the same
outcome); changing `IDS_ERROREXECPROMPT`'s text (would silently keep stale
translations, since `.slt` rows are matched by identity).

### R14 — Translations: the documented two-stage refresh

**Decision**: after the resources change: `build.cmd` →
`src\vcxproj\build_langs.cmd --export-templates --module salamand` →
`python -m translate.merge --module salamand --dry-run` (cost + what DeepL
returns) → `python -m translate.merge --module salamand` →
`python -m translate.slt --verify` → `build.cmd full`. Pin the four product
names in `translations/ui-overrides.json` wherever DeepL alters them. Add
`shell`, `preset`, `cmd` to `_WORDS` in `tools/translate/uicontext.py` so the
new symbols split into readable context words (dev-only tooling).
**Rationale**: `.slt` import is positional — one new row breaks all 8 languages
until the refresh (memory + `specs/051-*/quickstart.md:110-123`); `addrows`
cannot carry string-table rows; `_DOMAINS` already has `salamand`; the DeepL
key file is present. The three disabled languages stay untouched (their
`.slt` is already structurally stale by policy).

### R15 — Help: new topic, alias, TOC/index; branding of a 2026 page

**Decision**: add `help/src/hh/salamand/configuration_cmdshell.htm` (skeleton
of `configuration_usrmn.htm`), the `[ALIAS]` line in `salamand.hhp`, entries in
`salamand.hhc` (Configuration branch, alphabetical) and `salamand.hhk`; update
`othertask_shell.htm` (the setting, the archive-folder rule, the UNC remark
reworded as the program's own behaviour, Windows Terminal/PowerShell accept
UNC). The **new** page names the product "Tandem Commander" and carries
`© 2026 Pavel Stupka` per the CLAUDE.md copyright rule; the **edited** page keeps
its existing footer. The `.chm` is compiled by hand (`help\src\compileall.bat`,
HTML Help Workshop) — not part of `build.cmd` or the installer's content unless
present in the build tree; that is the repository's pre-existing state.
**Rationale**: no feature since the rebrand has touched help content; all 236
pages still say "Open Salamander" with a 2023 footer. A page authored in 2026 by
the current holder cannot honestly carry the 2023 notice. The manual-wide
rebrand is recorded as a separate follow-up, not smuggled in here.

### R16 — Tests: `saltests` for the pure helper, a manual matrix for the rest

**Decision**: `TestCommandShell071()` in `src/saltests/saltests.cpp` (fake
probe: candidate order per preset, fallbacks, not-found, `EnvDir` with a
non-ASCII `%LOCALAPPDATA%`, `RegEnum` first-existing subkey, `SalGetEnvVarU8`
round trip; plus a real-probe smoke that `cmd` and `powershell` are found on
any Windows). Everything in the main application (expansion table, page,
launch) is covered by [quickstart.md](quickstart.md)'s manual matrix and the
strict encoding guard.
**Rationale**: `saltests` cannot link `src/execute.cpp` or the dialogs; moving
`ExpandVarString` to `src/common/` is out of proportion for this feature.

### R17 — Changelog and version at the ship gate

**Decision**: an *Added* entry (user's terms: "the Command Shell command can
open Windows Terminal, PowerShell 7, Git Bash … or any program; default
unchanged") plus the `MINORB`/`VERSINFO_BUILDNUMBER` / `MyAppVersion` /
`CLAUDE.md` bump in the same change, as the constitution requires. There is no
*Unreleased* section by convention; feature 070's own entry is still pending its
ship gate, so both would ship in the next version — the user's call.

### R18 — Windows 11 "default terminal application"

**Decision**: no special handling. When Windows Terminal is the user's default
terminal, console programs (`cmd`, `powershell`, `pwsh`) started by the product
are hosted in it automatically; `STARTF_USEPOSITION` then applies to the
console session, not necessarily to the hosting window. This is documented in
the manual as the platform's behaviour, not the file manager's.

### R19 — Long working directories: retry with the 8.3 form

**Decision**: when the panel directory's UTF-16 length is ≥ `MAX_PATH - 1`,
retry the launch once with `SalGetShortPathName(dir)`; if that is still too
long or the volume has no short names, show E2 with the system's reason.
**Rationale**: `CreateProcessW` refuses an `lpCurrentDirectory` of ≥ 259
characters for every program regardless of long-path awareness (documented for
`SetCurrentDirectory`, verified locally: 258 works, 259 fails with
`ERROR_DIRECTORY`, `\\?\` fails too); 0.1.5 fails outright there. The spec's
edge case was corrected on 2026-08-28 to state the platform limit honestly.
**Alternatives**: starting in the nearest short ancestor and `cd`-ing inside the
shell (shell-specific syntax, not possible for a generic Custom program);
doing nothing (the error would name the program and the reason, but a short
name is a free improvement).

## Confirmed non-issues

- `IDS_MENU_CMD_SHELL` / `IDS_TBTT_COMMANDSHELL` / the toolbar icon are static
  and untouched (FR-013).
- `CTransferInfo::EditLine` is UTF-8 aware (`src/common/winlib.cpp:1042-1090`),
  so `char[]` UTF-8 fields bind directly.
- `SalRegQueryValueExW8` reads values written by older ANSI builds correctly —
  no migration path is needed for the new strings.
- `MultiMonGetDefaultWindowPos` and the `HANDLES` tracking of the process/thread
  handles remain exactly as today.

## Open follow-ups recorded (not in scope)

1. `lpTitle` forwarding in `SalCreateProcess` (R8).
2. The other two ANSI `COMSPEC` reads (command line box, User Menu `.bat`).
3. Manual-wide rebrand of `help/src/hh/salamand/*.htm` and its footers (R15).
4. Letting the command line box use the chosen shell (spec: explicitly deferred).

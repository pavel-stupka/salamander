# Data Model: Configurable Command Shell

**Feature**: 071 · **Date**: 2026-08-28 · **Spec**: [spec.md](spec.md) ·
**Contracts**: [contracts/](contracts/)

Four entities: the persisted **Command Shell setting**, the static **Shell
preset** table, the transient **Launch context**, and the **Probe** the locate
logic talks to (which is what makes the logic unit-testable).

## 1. Command Shell setting (persisted)

Lives in the global `CConfiguration` (`src/cfgdlg.h`), defaulted in
`CConfiguration::CConfiguration()` (`src/dialogs4.cpp`), saved/loaded in
`CMainWindow::SaveConfig` / `LoadConfig` (`src/mainwnd2.cpp`) under
`HKCU\Software\Tandem Commander\0.1\Configuration`. Exactly one per user
configuration. Value names are fixed by
[contracts/command-shell-setting.md](contracts/command-shell-setting.md).

| Field (C++) | Type | Registry value | Default | Meaning |
|-------------|------|----------------|---------|---------|
| `CommandShellPreset` | `int` (enum `CSalShellPreset`) | `Command Shell Preset` (REG_DWORD) | `0` = Command Prompt | Which recipe launches. `5` = Custom. |
| `CommandShellProgram` | `char[SAL_MAX_PATH_UTF8]`, UTF-8 | `Command Shell Program` (REG_SZ, UTF-8 via the W8 facade) | `""` | Custom only. Program path; may contain `$(WinDir)`, `$(SysDir)`, `$(SalDir)`, `$[NAME]`. |
| `CommandShellArguments` | `char[USRMNUARGS_MAXLEN]`, UTF-8 | `Command Shell Arguments` (REG_SZ) | `""` | Custom only. May contain `$(FullPath)`, `$(WinDir)`, `$(SysDir)`, `$(SalDir)`, `$[NAME]`. |

**Validation** (enforced by the Configuration page on OK; see
[contracts/configuration-page.md](contracts/configuration-page.md)):

- `CommandShellPreset` ∈ {0..5}. On load, any other value → `0` (default), no message.
- A preset (0..4) can be confirmed only if the locate step found it on this machine.
- Custom (5): `CommandShellProgram` must not be blank and must pass
  `ValidateCommandFile` (placeholder syntax); `CommandShellArguments` must pass
  `ValidateVarString` with the Command Shell argument table (unknown `$(x)`
  rejected, `$[NAME]` accepted). Existence of the program file is **not**
  required at OK time (same decision as the User Menu — network/removable paths).

**Lifecycle**: constructor default → `LoadConfig` (missing value keeps the
default; out-of-range preset → default) → Configuration page `Transfer`
(`ttDataToWindow` / `ttDataFromWindow`) → `SaveConfig`. *Export Configuration*
dumps the whole registry branch, so the values travel with no extra code; a
configuration lacking them (upgrade from ≤ 0.1.5, or an older export) yields the
defaults.

**Invariants**: the Custom fields are retained even while a preset is selected
(switching Custom → preset → Custom restores the user's text, US2 scenario 8);
the setting is read at every launch — nothing about the resolved path is cached
in the configuration.

## 2. Shell preset (static table)

Defined once in `src/common/salshell.cpp`, exposed through `src/common/salshell.h`.
Ids are **stable** (they are what the registry stores) and new presets are only
ever appended.

| Id | Enum | Key (logs/tests) | Display name (app string) | Locate strategy | Launch arguments |
|----|------|------------------|---------------------------|-----------------|------------------|
| 0 | `sspCommandPrompt` | `cmd` | Command Prompt | see [contracts/shell-presets.md](contracts/shell-presets.md) | (none) |
| 1 | `sspWindowsPowerShell` | `powershell` | Windows PowerShell | ″ | (none) |
| 2 | `sspPowerShell7` | `pwsh` | PowerShell 7 | ″ | (none) |
| 3 | `sspWindowsTerminal` | `wt` | Windows Terminal | ″ | `-d .` |
| 4 | `sspGitBash` | `git-bash` | Git Bash | ″ | (none) |
| 5 | `sspCustom` | `custom` | Custom program | — (user fields) | (user field) |

Attributes of a preset record:

- `Id`, `Key` (ASCII, for tests and traces — never shown to users);
- `Candidates[]` — an ordered list of *locate candidates*, each one of:
  `EnvValue(name)` (the variable's value is the program path, e.g. `COMSPEC`),
  `EnvDir(name, relativeExe)` (`$name\relativeExe`),
  `RegString(root, subkey, value, relativeExe)` (registry string + relative exe),
  `RegEnum(root, subkey, value, relativeExe)` (first subkey whose value + relative exe exists),
  `PackagePath(family, relativeExe)` (MSIX package install path + relative exe);
- `Arguments` — the constant argument recipe (never contains placeholders);
- the display name is **not** in the common module (no `LoadStr` there): the
  page maps id → `IDS_CMDSHELL_PRESET_*`.

**Resolution result** (`CSalShellLocateResult`): `Found` (BOOL) and `Path`
(UTF-8, absolute, the first candidate that exists). Resolution is performed on
dialog open (for marking + *found at*), on preset selection change, and at every
launch — it is a handful of attribute/registry reads, never a process launch.

## 3. Launch context (transient)

Built by `OpenCommandShell()` (`src/cmdshell.cpp`) for one invocation and
discarded; nothing here is persisted.

| Field | Source |
|-------|--------|
| `Program` (UTF-8) | preset → `Path` from resolution; Custom → `ExpandCommand(CommandShellProgram)` |
| `Arguments` (UTF-8) | preset → recipe; Custom → `ExpandCommandShellArguments(CommandShellArguments, panelDir)` |
| `CommandLine` | `"Program"` (quoted when it contains a space) + `" "` + `Arguments` (omitted when empty) |
| `WorkingDirectory` | unchanged rule: `activePanel->Is(ptDisk) \|\| Is(ptZIPArchive)` → `activePanel->GetPath()` (for an archive: the folder containing the archive), else `NULL`; if its UTF-16 length is ≥ `MAX_PATH - 1`, the launch is retried once with the 8.3 form (`SalGetShortPathName`) because Windows refuses longer starting directories |
| `Startup` | unchanged: `STARTF_USESHOWWINDOW`, `SW_SHOWNORMAL`, `STARTF_USEPOSITION` from `MultiMonGetDefaultWindowPos` when the main window is not on the primary monitor |
| `PolicySubject` | `Program` — passed to `SystemPolicies.GetMyCanRun` (leaf-name, case-insensitive compare, as today) |

Error outcomes (all end the command without launching anything else):

| Condition | Message |
|-----------|---------|
| Group Policy forbids running / the program is not allowed | existing `IDS_POLICIESRESTRICTION` box with Help (`IDH_GROUPPOLICY`) — unchanged |
| Preset not found at launch time | `IDS_CMDSHELL_ERRNOTFOUND` (names the preset) + Help button → the Command Shell configuration topic |
| Custom template invalid / `$[NAME]` missing and the user cancels | handled by `ExpandVarString`'s own message (existing behaviour of the User Menu) |
| `SalCreateProcess` fails | `IDS_CMDSHELL_ERREXEC` composed from `LoadStrU8` with the resolved program path and `GetErrorText(err)`, caption `IDS_CMDSHELL_ERRTITLE`, Help button → the configuration topic |

## 4. Probe (dependency inversion for testability)

```text
CSalShellProbe (abstract)
  BOOL FileExists(const char* u8Path)                                   // file, not directory
  BOOL GetEnv(const char* name, char* u8Buf, int bufSize)               // wide read, UTF-8 out
  BOOL RegReadString(HKEY root, const char* subKey, const char* value,
                     char* u8Buf, int bufSize)                          // REG_SZ/EXPAND_SZ, W API, UTF-8 out
  BOOL RegFirstSubKeyString(HKEY root, const char* subKey, const char* value,
                     const char* relativeExe, char* u8Buf, int bufSize) // enumerate subkeys, first existing
  BOOL GetPackagePath(const char* family, char* u8Buf, int bufSize)     // GetPackagesByPackageFamily + GetPackagePathByFullName
CSalShellOsProbe : CSalShellProbe   // the real one (src/common/salshell.cpp)
CFakeShellProbe  : CSalShellProbe   // saltests: in-memory file set, env map, registry map, package map
```

`SalShellLocatePreset(id, probe, result)` and `SalShellPresetArguments(id)` are
pure functions of the probe's answers, which is what `TestCommandShell071()`
exercises: candidate order per preset, fallbacks, not-found, `EnvDir` with a
non-ASCII `%LOCALAPPDATA%`, `RegEnum` picking the first subkey whose exe exists.

## Relationships

```text
CConfiguration.CommandShell{Preset,Program,Arguments}   1 ──uses──▶  Shell preset table (by Preset id)
Configuration page  ◀──edits──  setting ; ──queries──▶ SalShellLocatePreset(probe = OS)
OpenCommandShell()  ──reads──▶  setting + active panel  ──builds──▶  Launch context  ──▶ SalCreateProcess
saltests            ──drives──▶ SalShellLocatePreset(probe = fake)
```

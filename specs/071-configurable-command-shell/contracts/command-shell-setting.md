# Contract: Command Shell setting, placeholders and launch

**Feature**: 071 · **Status**: binding for implementation and tests ·
**Consumers**: `src/common/salshell.*`, `src/cmdshell.cpp`, `src/execute.cpp`,
the Configuration page, `saltests`, the user manual.

## 1. Registry values

All three live under the existing key
`HKCU\Software\Tandem Commander\0.1\Configuration` and are written/read through
the same `SetValue` / `GetValue` calls as every other option (UTF-8 payload
through `SalRegSetValueExW8` / `SalRegQueryValueExW8`).

| Value name (`const char*` in `mainwnd2.cpp`) | Type | Range / format | Absent or invalid |
|---|---|---|---|
| `Command Shell Preset` (`CONFIG_CMDSHELL_PRESET_REG`) | REG_DWORD | 0..5, see §2 | → 0 (Command Prompt) |
| `Command Shell Program` (`CONFIG_CMDSHELL_PROGRAM_REG`) | REG_SZ, UTF-8 | ≤ `SAL_MAX_PATH_UTF8`-1 bytes | → `""` |
| `Command Shell Arguments` (`CONFIG_CMDSHELL_ARGS_REG`) | REG_SZ, UTF-8 | ≤ `USRMNUARGS_MAXLEN`-1 bytes | → `""` |

Rules:

- No `THIS_CONFIG_VERSION` bump: a missing value simply keeps the constructor
  default (the same way `Theme Mode` was added in feature 028).
- Names do not end in `.hidden`, so *Export Configuration* includes them.
- The Custom fields are stored even when a preset is selected.
- A MINORB release must not move configuration (constitution): nothing here does.

## 2. Preset identifiers (stable)

```c
enum CSalShellPreset {
    sspCommandPrompt    = 0,   // default; %COMSPEC% — today's behaviour
    sspWindowsPowerShell = 1,
    sspPowerShell7      = 2,
    sspWindowsTerminal  = 3,
    sspGitBash          = 4,
    sspCustom           = 5,
    sspCount            = 6
};
```

Never renumber; append only. Locate strategies and launch recipes per preset
are fixed by [shell-presets.md](shell-presets.md).

## 3. Placeholder grammar (Custom only)

Expansion uses the house `ExpandVarString` engine (`$(name)`, `$(name:width)`,
`$[ENV]`), which already validates syntax and reports unknown variables.

### 3.1 Program field — table `CommandExpArray` (existing, via `ExpandCommand`)

| Placeholder | Value | Note |
|---|---|---|
| `$(WinDir)` | Windows directory | as in the User Menu *Command* field |
| `$(SysDir)` | System directory | ″ |
| `$(SalDir)` | Tandem Commander's directory | ″ (trailing backslash collapsed by `RemoveDoubleBackslahesFromPath`) |
| `$[NAME]` | environment variable `NAME` | read wide (`GetEnvironmentVariableW`), UTF-8 out — **new**: the one shared read in `DoExpandVarString` moves from the ANSI API to the wide helper |

Validation on OK: `ValidateCommandFile` (rejects a blank field and bad syntax).

### 3.2 Arguments field — table `CommandShellArgsExpArray` (new, in `src/execute.cpp`)

| Placeholder | Executor | Value |
|---|---|---|
| `$(FullPath)` | `ExecuteExpFullPath2` | the active panel's directory **without** a trailing backslash (`D:\Work`, `\\server\share\dir`); a drive root keeps it (`C:\`) — the User Menu *Initial Directory* meaning |
| `$(WinDir)` | `ExecuteExpWinDir2` | Windows directory, no trailing backslash |
| `$(SysDir)` | `ExecuteExpSysDir2` | System directory, no trailing backslash |
| `$(SalDir)` | `ExecuteExpSalDir2` | application directory, no trailing backslash |
| `$[NAME]` | engine | environment variable |

Entry point: `BOOL ExpandCommandShellArguments(HWND msgParent, const char* u8PanelDir, const char* varText, char* buffer, int bufferLen, BOOL ignoreEnvVarNotFoundOrTooLong)`.
It builds `CExecuteExpData` with `Name` = the panel directory **with** a trailing
backslash appended (so `ExecuteExpFullPath2` cuts back to the directory; a root
stays `C:\`), `DosName = NULL`, `FileNameUsed = NULL`, and does **not** call
`RemoveDoubleBackslahesFromPath` (it would collapse a UNC `\\server` that
appears later in the argument string). Validation twin:
`ValidateCommandShellArguments` = `ValidateVarString` with the same table.

When the panel has no usable directory (plugin file system), `$(FullPath)`
expands to the empty string and the launch proceeds with no working directory —
the same outcome today's command produces for such panels.

### 3.3 Documented quirk

Windows argument parsing treats `\"` as an escaped quote, so `"$(FullPath)"`
for a **drive root** reaches the program as `C:"`. This is the platform's rule,
not the file manager's; the manual states it, and the presets are immune (they
pass no path — see [shell-presets.md](shell-presets.md)).

## 4. Launch composition (`OpenCommandShell()`, `src/cmdshell.cpp`)

```text
1. activePanel->UserWorkedOnThisPath = TRUE                       (unchanged)
2. resolve:
     preset  → SalShellLocatePreset(id, OS probe) ; not found → error E1
     custom  → ExpandCommand(CommandShellProgram) ; FALSE → stop (engine already reported)
3. arguments:
     preset  → SalShellPresetArguments(id)
     custom  → ExpandCommandShellArguments(CommandShellArguments, panelDir) ; FALSE → stop
4. policy:   SystemPolicies.GetNoRun() || (GetMyRunRestricted() && !GetMyCanRun(program))
             → existing IDS_POLICIESRESTRICTION box (unchanged)                 (E0)
5. cmdLine = quoteIfNeeded(program) [+ " " + arguments]      // AddDoubleQuotesIfNeeded on the program alone
6. SetDefaultDirectories()                                     (unchanged)
7. STARTUPINFO: STARTF_USESHOWWINDOW, SW_SHOWNORMAL, optional STARTF_USEPOSITION   (unchanged)
   lpTitle = LoadStr(IDS_COMMANDSHELL)  — kept for parity; SalCreateProcess does not forward it (pre-existing)
8. SalCreateProcess(NULL, cmdLine, ..., CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                    NULL, (ptDisk || ptZIPArchive) ? GetPath() : NULL, &si, &pi)
   FALSE → error E2 ; TRUE → HANDLES(CloseHandle) both handles                    (unchanged)
```

Guarantees:

- **Default configuration is today's behaviour**: with preset 0 the command line
  is the quoted `%COMSPEC%` and every other parameter is identical to the 0.1.5
  handler (SC-002).
- The policy check runs on the **resolved** program (full path; `GetMyCanRun`
  compares the leaf name case-insensitively, as today).
- A launcher that exits immediately (Windows Terminal) is a success: the handler
  never waits for or inspects the process.

Errors:

| Id | When | Text (English) | Caption | Extras |
|---|---|---|---|---|
| E0 | policy | existing `IDS_POLICIESRESTRICTION` | existing | Help → `IDH_GROUPPOLICY` (unchanged) |
| E1 | preset not found | `IDS_CMDSHELL_ERRNOTFOUND`: "The selected command shell program (%s) was not found on this computer.\n\nChoose a different program in Configuration / Command Shell." (`%s` = preset display name) | `IDS_CMDSHELL_ERRTITLE`: "Error Starting Command Shell" | `MSGBOXEX_HELP`, `ContextHelpId = IDD_CFGPAGE_CMDSHELL`, `MessageBoxHelpCallback` |
| E2 | `SalCreateProcess` fails | `IDS_CMDSHELL_ERREXEC`: "Cannot start the command shell program:\n%s\n\n%s\n\nYou can change the program in Configuration / Command Shell." (`%s` = resolved program path, `%s` = `GetErrorText(err)`) | `IDS_CMDSHELL_ERRTITLE` | same Help wiring; composed with `_snprintf_s` into `char buff[4 * MAX_PATH]` from a **`LoadStrU8`** template (both arguments are UTF-8); an over-long path is pre-trimmed with `...` as the User Menu does |

## 5. Encoding-guard obligations for new code

- Every message that embeds a path uses `LoadStrU8` (`mixed-composition`).
- Text set into dialog controls goes through `SalSetDlgItemTextU8` /
  `SalGetDlgItemTextU8` / `ti.EditLine` (`utf8-to-legacy-sink`).
- Environment reads use the new wide helper (`GetEnvironmentVariableW` +
  `SalWToU8`), registry reads use the W API or the W8 facade, file probes use
  `SalGetFileAttributes` / `FileExists` (`ansi-api-on-utf8-path`).
- Never hand a `SalU8ToWDisplay*` result to `SalCreateProcess`
  (`lossy-lenient-at-intake`).
- `python tools/check_encoding.py --strict` must stay `TOTAL: 0`; the draft
  rule `missed-twin` must not gain a hit for `IDS_CMDSHELL_*` (each new string is
  loaded through exactly one of `LoadStr` / `LoadStrU8`).

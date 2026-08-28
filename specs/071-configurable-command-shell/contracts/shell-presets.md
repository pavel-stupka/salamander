# Contract: shell presets — locate strategy and launch recipe

**Feature**: 071 · **Implemented in**: `src/common/salshell.cpp` (table + locate),
tested by `TestCommandShell071()` · **Verified**: 2026-08-28 against vendor
documentation and source, plus local `CreateProcessW` experiments on Windows 11
25H2 (sources at the end).

## 1. Common rules

- Every preset is launched with `lpCurrentDirectory` = the panel directory
  (unchanged rule) and **never** relies on a placeholder. Only Windows Terminal
  needs an argument, because its profile `startingDirectory` (default
  `%USERPROFILE%`) would otherwise override the inherited directory.
- Locate = walk the candidate list in order; the first candidate whose
  executable **exists as a file** wins. Candidates read the environment through
  the wide helper and the registry through the W API; a candidate that cannot
  be evaluated (missing variable, missing key) is skipped silently.
- A found path is absolute and UTF-8; the launch quotes it when it contains a
  space. Not found → the page marks the preset and OK is refused; at launch →
  error E1.
- The launch never waits for the child. Launchers that exit at once (`wt.exe`)
  and wrappers that stay alive (`git-bash.exe`) are equally fine.
- **Long directories**: Windows refuses `CreateProcessW` when
  `lpCurrentDirectory` is ≥ 259 characters, whatever the manifests say
  (verified: 258 works, 259 fails with `ERROR_DIRECTORY`; `\\?\` fails too).
  The launcher therefore retries once with the directory's short (8.3) form
  (`SalGetShortPathName`) when the wide length is ≥ `MAX_PATH - 1`; if that is
  still too long or unavailable, error E2 shows the system's reason. (0.1.5
  fails outright in this case.)
- **Windows 11 default terminal**: when Windows Terminal is the default
  terminal application, console programs (`cmd`, `powershell`, `pwsh`) are
  hosted in it automatically; the working directory is already set by then,
  `STARTF_USEPOSITION` is not honoured by the host. Nothing to do in the
  product; the manual mentions it.

## 2. Presets

### 0 — Command Prompt (`cmd`) — default

| Candidate | Kind |
|---|---|
| `%COMSPEC%` | `EnvValue("COMSPEC")` — exactly what 0.1.5 launches |
| `%SystemRoot%\System32\cmd.exe` | `EnvDir("SystemRoot", "System32\cmd.exe")` — only if `COMSPEC` is unset/invalid |

Arguments: none. Honours cwd: yes (local paths). UNC panel: cmd prints
"UNC paths are not supported. Defaulting to Windows directory." and continues in
`%SystemRoot%` unless the user sets `DisableUNCCheck=1` under
`HKCU\Software\Microsoft\Command Processor` — its own documented behaviour,
unchanged from 0.1.5.

### 1 — Windows PowerShell (`powershell`)

| Candidate | Kind |
|---|---|
| `%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe` | `EnvDir("SystemRoot", …)` |

Arguments: none (there is no working-directory switch in 5.1; it starts in the
inherited directory — verified, including paths with spaces). Accepts UNC.

### 2 — PowerShell 7 (`pwsh`)

Ordered as Windows Terminal's own profile generator does, alias/MSIX before
the MSI registry key (the MSI is no longer produced from 7.7; winget installs
MSIX from 7.6):

| Candidate | Kind |
|---|---|
| `%ProgramFiles%\PowerShell\7\pwsh.exe` | `EnvDir("ProgramFiles", "PowerShell\7\pwsh.exe")` |
| `%LOCALAPPDATA%\Microsoft\WindowsApps\pwsh.exe` | `EnvDir("LOCALAPPDATA", …)` — MSIX/Store alias |
| `%LOCALAPPDATA%\Microsoft\WindowsApps\Microsoft.PowerShell_8wekyb3d8bbwe\pwsh.exe` | `EnvDir(…)` — per-family alias (survives a disabled root alias) |
| `HKLM\SOFTWARE\Microsoft\PowerShellCore\InstalledVersions\<any>\InstallLocation` + `pwsh.exe` | `RegEnum(HKLM, …, "InstallLocation", "pwsh.exe")` — MSI installs, incl. custom folders |
| `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\pwsh.exe` (default value) | `RegString(HKLM, …, "", "")` |

Arguments: none. Do **not** use `-WorkingDirectory` (a PowerShell path: `[`/`]`
in a folder name act as wildcards; applied after the profile). Accepts UNC.
Previews (`7-preview`, `Microsoft.PowerShellPreview_…`) are deliberately not
presets — *Custom*.

### 3 — Windows Terminal (`wt`)

| Candidate | Kind |
|---|---|
| `%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe` | `EnvDir("LOCALAPPDATA", …)` — App Execution Alias (0-byte reparse point; `CreateProcessW` resolves it natively — verified) |
| `%LOCALAPPDATA%\Microsoft\WindowsApps\Microsoft.WindowsTerminal_8wekyb3d8bbwe\wt.exe` | `EnvDir(…)` — per-family alias, present even when the user disabled the root alias in *Settings → Apps → App execution aliases* |
| package path of family `Microsoft.WindowsTerminal_8wekyb3d8bbwe` + `wt.exe` | `PackagePath("Microsoft.WindowsTerminal_8wekyb3d8bbwe", "wt.exe")` — `GetPackagesByPackageFamily` + `GetPackagePathByFullName` (kernel32, Windows 8.1+); covers both aliases disabled |

Arguments: **`-d .`** — relative values are resolved against the `wt.exe`
process's working directory (`Utils::EvaluateStartingDirectory`), which the
shim forwards unchanged; an absolute `-d "C:\"` would hit the trailing-backslash
quoting rule and fail with `0x8007010B`. No `-w`: the user's `windowingBehavior`
(new window by default) is respected, and the captured directory is honoured
even when the tab lands in an existing window. `wt.exe` is a shim that starts
`WindowsTerminal.exe` and returns at once. `lpTitle`/`STARTF_USEPOSITION` are
not honoured (GUI app with its own `initialPosition`); a specific profile
(`-p`) or `--pos` is a *Custom* configuration. Accepts UNC.

### 4 — Git Bash (`git-bash`)

| Candidate | Kind |
|---|---|
| `HKCU\Software\GitForWindows\InstallPath` + `git-bash.exe` | `RegString(HKCU, …)` — per-user (non-admin) install |
| `HKLM\SOFTWARE\GitForWindows\InstallPath` + `git-bash.exe` | `RegString(HKLM, …)` — admin install (64-bit) |
| `HKLM\SOFTWARE\WOW6432Node\GitForWindows\InstallPath` + `git-bash.exe` | `RegString(HKLM, …)` — 32-bit Git on x64 |
| `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Git_is1\InstallLocation` + `git-bash.exe` | `RegString(HKLM, …)` — Inno uninstall entry (verified present) |
| `%ProgramFiles%\Git\git-bash.exe` | `EnvDir("ProgramFiles", …)` — default admin folder |
| `%LOCALAPPDATA%\Programs\Git\git-bash.exe` | `EnvDir("LOCALAPPDATA", …)` — default per-user folder |

Arguments: none — `git-bash.exe` passes its own working directory straight
through as mintty's `lpCurrentDirectory` unless `--cd-to-home` (the Start-menu
shortcut's flag) or `--cd=` is given; `/etc/profile` has no cd-to-home logic.
It is a GUI wrapper that stays alive until mintty exits — irrelevant to the
launcher. `lpTitle`/`STARTF_USEPOSITION` are dropped by the wrapper (it zeroes
its own `STARTUPINFO`); mintty options (`-t`, `-p`) are reachable only by
launching `usr\bin\mintty.exe` directly — a *Custom* configuration. Accepts
UNC.

### 5 — Custom

No locate; the user's program field (after `ExpandCommand`) and arguments
field (after `ExpandCommandShellArguments`) are used verbatim. See
[command-shell-setting.md](command-shell-setting.md).

## 3. Candidate kinds (the whole vocabulary the table uses)

| Kind | Evaluation |
|---|---|
| `EnvValue(name)` | value of `%name%` is the executable path |
| `EnvDir(name, relative)` | `%name%` + `\` + `relative` |
| `RegString(root, subKey, value, relative)` | REG_SZ/REG_EXPAND_SZ `value` (expanded) + `\` + `relative`; `relative` empty → the value is the exe |
| `RegEnum(root, subKey, value, relative)` | for each subkey of `subKey`, as `RegString`; first existing wins |
| `PackagePath(family, relative)` | first full package name of `family` → package install path + `\` + `relative` |

All five are answered by the `CSalShellProbe` (fake in tests, W-API in the
product); the table itself is data, so adding a candidate is a one-line change
covered by the existing tests.

## 4. Test matrix (`TestCommandShell071`, fake probe)

- For each preset: with only the *n*-th candidate present, the result is that
  candidate (order and fallback); with none present → not found.
- `EnvDir` with `%LOCALAPPDATA%` = `C:\Users\Jiří Novák\AppData\Local` (UTF-8):
  the resolved path keeps the accented characters.
- `RegEnum`: two `InstalledVersions` subkeys, only the second has `pwsh.exe` →
  the second wins.
- `PackagePath`: family present but `wt.exe` missing → falls through to
  not found (no crash).
- Recipe: `SalShellPresetArguments` returns `-d .` for 3, empty for 0/1/2/4,
  and never contains `$(`.
- Real probe smoke: presets 0 and 1 are found on the build machine (they are
  part of Windows).

## Sources

- `CreateProcessW`, `STARTUPINFOW`, `SetCurrentDirectory` ("Setting a current
  directory longer than MAX_PATH causes CreateProcessW to fail"), *Parsing C
  command-line arguments* — learn.microsoft.com
- Windows Terminal: *command-line-arguments*, *profile-general*
  (`startingDirectory`), *startup* (`windowingBehavior`), *install*;
  microsoft/terminal `src/cascadia/wt/shim.cpp`, `src/types/utils.cpp`
  (`EvaluateStartingDirectory`), `Profile.cpp`, `WindowEmperor.cpp`,
  `PowershellCoreProfileGenerator.cpp`; issues #4715, #9518, #18687
- App execution aliases: *desktop-to-uwp-extensions*, `GetPackagesByPackageFamily`
  (learn.microsoft.com); tiraniddo.dev, *Overview of Windows Execution Aliases*
- Git for Windows: gitforwindows.org/git-wrapper.html; build-extra
  `installer/install.iss` (registry keys, default folders); MINGW-packages
  `mingw-w64-git/git-wrapper.c`, `git-bash.rc`; mintty `src/winmain.c`
- PowerShell 7: *Installing PowerShell on Windows* (MSI → MSIX transition),
  `about_Pwsh` (`-WorkingDirectory`), PowerShell `assets/wix/Product.wxs`
  (`PowerShellCore\InstalledVersions`), issues #5752, #16221, #7895
- Windows PowerShell 5.1: `about_PowerShell_exe`, *Starting Windows PowerShell*
- cmd.exe UNC: KB Q156276 (`DisableUNCCheck`), Microsoft Q&A

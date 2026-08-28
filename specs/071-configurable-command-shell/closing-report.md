# Feature 071 — closing report (implementation run of 2026-08-28)

**Branch**: `071-configurable-command-shell` · **Baseline**: 0.1.5 (build 189)
· **Record of the run**: [fix-log.md](fix-log.md) · **Task state**: [tasks.md](tasks.md)

## What shipped in the working tree

The *Command Shell* command (`Num /`, `Ctrl+/`, *Commands → Command Shell*,
toolbar button — all one handler) now opens a user-chosen program:

- **Presets** *Command Prompt* (default — launches `%COMSPEC%` exactly as
  0.1.5 did), *Windows PowerShell*, *PowerShell 7*, *Windows Terminal*, *Git
  Bash*, located at their standard places (`src/common/salshell.*`, table and
  algorithm per [contracts/shell-presets.md](contracts/shell-presets.md)), or
- **Custom program** + arguments with `$(FullPath)` (panel directory, no
  trailing backslash, root excepted), `$(WinDir)/$(SysDir)/$(SalDir)`, `$[ENV]`
  (`CommandShellArgsExpArray` + `ExpandCommandShellArguments` in
  `src/execute.cpp`; the program field uses the existing `ExpandCommand`).
- **Configuration → Command Shell** page (`IDD_CFGPAGE_CMDSHELL`,
  `CCfgPageCmdShell`, inserted after *Hot Paths*): preset list with
  "(not found)" marking and refusal on OK, read-only *Found at* path, Custom
  program (wide Browse) + arguments, pre-fill of empty Custom fields from the
  previous preset, syntax validation.
- **Persistence**: `Command Shell Preset` / `Program` / `Arguments` under the
  existing `Configuration` key; defaults when absent; no config-version bump;
  Export Configuration carries them automatically.
- **Launch** (`CMainWindow::OpenCommandShell`, `src/cmdshell.cpp`): the
  pre-071 working-directory rule, `STARTUPINFO` and Group Policy check; a
  retry with the 8.3 directory name when Windows refuses a ≥ 259-character
  starting directory; errors E1 (preset not found) and E2 (cannot start) name
  the program, quote the reason, point to the setting and carry a Help button
  to the new manual topic.
- **Encoding**: wide environment reads (`SalGetEnvVarU8`, also used by the
  shared `$[ENV]` expansion), W registry/package APIs, `SafeGetOpenFileNameW`,
  `LoadStrU8`-composed messages; strict guard `TOTAL: 0`.
- **Translations**: 22 new rows × 8 languages via DeepL (8,088 characters),
  43 pins for terminology/formality/`\n` hygiene; `.slt` round-trip byte-exact;
  `build.cmd full` green with 189 language modules.
- **Manual**: new `configuration_cmdshell.htm` (+ `[ALIAS]`, TOC, index) and an
  updated `othertask_shell.htm`.
- **Tests**: `saltests` 1301 → **1353 checks, 0 failed** (fake-probe matrix +
  real-machine smoke).

## What was verified, and how

| Gate | Result |
|---|---|
| `build.cmd` (Debug x64) | green; only warnings in touched files are two pre-existing `C4018` in `salamdr2.cpp` |
| `saltests.exe` | 1353 / 0 |
| `tools/check_encoding.py --strict` | `TOTAL: 0`; `--draft` has no finding for the new code |
| `translate.merge` + `translate.slt --verify` | 0 validation failures; 298 files byte-exact |
| `build.cmd full` | 189 language modules, version check OK |
| clang-format 17 (repo style) | 0 changes; BOM + CRLF on all touched files |
| GUI matrix (quickstart §3–§6) | **34 automated cases, all PASS** (fix-log.md "GUI verification"): every preset and entry point, non-ASCII / UNC / archive / long directories, Custom with `$(FullPath)`, E1/E2, the whole page incl. pre-fill, validation, Browse, save/Cancel/persist, Czech |

## What is still NOT verified

The automated run (after the user stopped his own instance) covers what can
be observed from outside the process. Not exercised, and worth one look by a
person:

1. **Multi-monitor placement** (`STARTF_USEPOSITION` when the main window is on
   a secondary monitor) — single-monitor session.
2. **Dark theme** rendering of the page (`CCommonPropSheetPage` theming) — the
   machine runs Theme Mode 0; screenshots are light theme.
3. **Group Policy** `RestrictRun`/`DisallowRun` message (needs an admin registry
   change).
4. **Export Configuration → Import** round trip (the values persist through
   `SaveConfig`/`LoadConfig` — D13 — and export is the generic branch copy).
5. The **`.chm`** compile and the error box's *Help* button landing on the new
   topic (no HTML Help Workshop on this PC).

## Deviations from plan/contracts (all recorded in fix-log.md)

- `RegSubKeyString` is index-based enumeration (a subkey without the value
  returns TRUE + `""`) instead of the data model's `RegFirstSubKeyString`.
- `$(FullPath)` uses a dedicated executor (`ExecuteExpCmdShellDir`) rather
  than `ExecuteExpFullPath2`, which returns `C:\` for an empty name.
- No separate "command line too long" check (E2 reports the system reason).
- Browse sets the picked path verbatim (no `SalGetFullName`, whose default
  buffer contract is `MAX_PATH`).
- Bad placeholder syntax is reported by the expansion engine's own message (it
  names the variable) instead of the generic `IDS_INCORRECTSYNTAX`.
- One pre-existing translation row changed on purpose: French
  `IDS_COMMANDSHELL` "Interpréteur de commande" → "…commandes" (= menu term).

## Follow-ups (not in scope, from research.md)

1. `SalCreateProcess` never forwards `lpTitle` (since feature 004) — decide
   separately whether to forward it (retitles every launched console).
2. The two other ANSI `COMSPEC` reads (command line box, User Menu `.bat`).
3. Manual-wide rebrand of `help/src/hh/salamand/*.htm` (236 pages still say
   "Open Salamander", 2023 footer); the new topic is the first post-rebrand page.
4. Letting the command line box use the chosen shell (spec: deferred by
   decision).
5. Ship gate (T036): CHANGELOG *Added* entry ("the Command Shell command can
   open Windows Terminal, PowerShell 7, Windows PowerShell, Git Bash or any
   program of your choice; the default is unchanged"), *Fixed* note for `$[ENV]`
   values outside the ANSI code page, and the version/build bump in
   `spl_vers.h`, `tandemcommander.iss`, `CLAUDE.md` — together with feature
   070's still-pending entry, when a version is cut.

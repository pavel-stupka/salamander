# Implementation Plan: Configurable Command Shell

**Branch**: `071-configurable-command-shell` | **Date**: 2026-08-28 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/071-configurable-command-shell/spec.md`

## Summary

The *Command Shell* command has exactly one handler (`CM_DOSSHELL`,
`src/mainwnd3.cpp:4290`) behind all of its entry points — `Num /`, `Ctrl+/` in
the panel and in the command line box, *Commands → Command Shell*, and the
toolbar button. Today it launches `%COMSPEC%` in the active panel's directory.
This feature makes the launched program a user setting: one of five presets
(*Command Prompt* — the default and today's behaviour, *Windows PowerShell*,
*PowerShell 7*, *Windows Terminal*, *Git Bash*) located automatically at their
standard installation places, or a *Custom* program with arguments that may
use `$(FullPath)` (the panel directory) and `$[NAME]` (environment variables).

Technical approach (details in [research.md](research.md)):

- **Pure logic in a new common module** `src/common/salshell.{h,cpp}`: the
  preset table (stable ids, locate strategy, launch arguments), the locate
  algorithm behind an injectable *probe* (file-exists / environment / registry),
  and a wide environment-variable helper. It is compiled into both the
  application and `saltests`, so the resolution rules are unit-tested with a
  fake probe.
- **Launch in the existing handler**, restructured into `src/cmdshell.cpp`:
  resolve program + arguments → the existing Group Policy check on the resolved
  program → `SalCreateProcess` with the **unchanged** working-directory rule
  (disk/archive panel → `GetPath()`, plugin FS → none) and the unchanged
  `STARTUPINFO` flags. Presets never rely on placeholders — they open in the
  working directory (Windows Terminal via `-d .`).
- **Argument expansion through the house `ExpandVarString` machinery** with a
  small dedicated variable table in `src/execute.cpp` (`$(FullPath)` with the
  User Menu *Initial Directory* meaning — no trailing backslash, drive root
  excepted — plus `$(WinDir)`, `$(SysDir)`, `$(SalDir)`, `$[ENV]`); the program
  field uses the existing `ExpandCommand` exactly like external viewers/editors.
- **Configuration**: three values under the existing `Configuration` registry
  key (`Command Shell Preset` DWORD, `Command Shell Program` and `Command Shell
  Arguments` REG_SZ, UTF-8 through the feature-004 facade), defaults in the
  `CConfiguration` constructor, no config-version bump, Export/Import covered
  automatically.
- **A new Configuration page** *Command Shell* (`IDD_CFGPAGE_CMDSHELL`,
  `CCfgPageCmdShell : CCommonPropSheetPage`) inserted after *Hot Paths*: preset
  combo (not-found presets marked and refused on OK), read-only *found at*
  path, Custom program + wide Browse + arguments, pre-fill of empty Custom
  fields from the previous preset.
- **Unicode end to end** (FR-016): environment reads via `GetEnvironmentVariableW`
  (new helper; the one existing `$[ENV]` read in `DoExpandVarString` moves to it),
  registry via the W8 facade, a new `SafeGetOpenFileNameW` for Browse, messages
  composed from `LoadStrU8` templates — the strict encoding guard stays at
  `TOTAL: 0`.
- **Translations, help, changelog**: ~14 new strings + 1 dialog for 8 languages
  via the documented `.slt` refresh; a new manual topic
  `configuration_cmdshell.htm` + `[ALIAS]` line + TOC/index entries, and an
  updated *Opening Command Shell* page; CHANGELOG entry at the ship gate.

## Technical Context

**Language/Version**: C++20 (`/std:c++latest`), MSVC v143 (VS2022), pure WinAPI  
**Primary Dependencies**: none new — existing house helpers only
(`SalCreateProcess`, `ExpandVarString`/`ExpandCommand`, registry facade
`SalRegQueryValueExW8`/`GetValue`/`SetValue`, `CTreePropDialog`/
`CCommonPropSheetPage`, `SalU8ToW`/`SalWToU8`)  
**Storage**: Windows registry, `HKCU\Software\Tandem Commander\0.1\Configuration`
— 3 new values (see [data-model.md](data-model.md)); no migration  
**Testing**: `saltests` (Debug x64, `build.cmd` → `saltests.exe`, exit code =
failures) for the pure helper; manual matrix in [quickstart.md](quickstart.md)
for the dialog, launch and every entry point; `tools/check_encoding.py --strict`
(runs inside every build); `python -m translate.slt --verify` for translations  
**Target Platform**: Windows 11 and newer, x64  
**Project Type**: desktop application — core executable + language DLL
(`src/lang`), shared common library (`src/common`)  
**Performance Goals**: preset detection on dialog open well under 50 ms (a few
file-attribute and registry reads, no process launches); launch latency and
window placement unchanged from 0.1.5  
**Constraints**: plugin ABI untouched (interface 106); default behaviour
byte-for-byte today's (`%COMSPEC%`, same working-directory rule, same
`STARTUPINFO`); UTF-8/WTF-8 house rules, encoding guard strict `TOTAL: 0`;
dialog house style (constitution VI); `.slt` refresh before any `build.cmd full`  
**Scale/Scope**: 1 new common module (+ 2 vcxproj entries), 1 new core file,
edits in ~9 existing core files, 1 dialog template + ~14 strings × 8 languages,
1 new + 1 edited help topic, ~20 saltests checks

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Gate | Status |
|-----------|------|--------|
| I. Build Reproducibility | Everything is built by `build.cmd` from committed sources; translations regenerated by the documented commands and committed as `.slt`; no manual steps in the build. The help `.chm` is compiled by hand with HTML Help Workshop — a pre-existing condition of the repository, not introduced here. | PASS |
| II. Backward Compatibility | Default preset = *Command Prompt* reproduces `CM_DOSSHELL` exactly (`%COMSPEC%`, same working-directory rule, same flags); the setting is opt-in; new registry values default when absent (no `THIS_CONFIG_VERSION` bump, MINORB release moves no configuration). Two deliberate, documented side effects: the remembered *last focused page* of Configuration pages after *Hot Paths* shifts by one (cosmetic, one-time), and `$[ENV]` values outside the ANSI code page now expand correctly (previously lossy). | PASS |
| III. Incremental Modernization | New logic in new files (`src/common/salshell.*`, `src/cmdshell.cpp`); edits to existing files are confined to the handler dispatch, one variable table, one environment read, Save/Load lines, the page list; no refactoring of adjacent code. `lpTitle` being dropped by `SalCreateProcess` is left as is (documented in research). | PASS |
| IV. Windows Platform Commitment | WinAPI only; Windows 11 targets (app execution aliases, `cldapi`-free). | PASS |
| V. Plugin Architecture Preservation | Core command, not plugin functionality; no `src/plugins/shared/` change; `LAST_VERSION_OF_SALAMANDER` untouched. | PASS |
| VI. UI Consistency | `IDD_CFGPAGE_CMDSHELL` is a 299×231 `DIALOGEX`, `DS_SETFONT \| DS_FIXEDSYS` (= `DS_SHELLFONT`), `FONT 8, "MS Shell Dlg"`, standard controls only; page derives from `CCommonPropSheetPage` (central dark theming); no manifest/common-controls changes. | PASS |
| Release Documentation | CHANGELOG *Added* entry + version/build bump (`spl_vers.h`, `tandemcommander.iss`, `CLAUDE.md`) in the ship-gate task, same change. | PASS (planned) |

**Post-design re-check (after Phase 1)**: unchanged — the design introduced no
new project, no new dependency, no plugin-interface change and no configuration
migration. See *Complexity Tracking* (empty).

## Project Structure

### Documentation (this feature)

```text
specs/071-configurable-command-shell/
├── plan.md              # This file
├── spec.md              # Feature specification (clarified 2026-08-28)
├── research.md          # Phase 0: decisions with rationale + alternatives
├── data-model.md        # Phase 1: setting, presets, launch context, registry values
├── quickstart.md        # Phase 1: validation matrix (manual + saltests)
├── contracts/
│   ├── command-shell-setting.md   # registry values, preset ids, placeholder syntax
│   ├── shell-presets.md           # per-preset locate strategy + launch recipe
│   └── configuration-page.md      # UI contract of the Command Shell page
├── checklists/requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks — NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
src/
├── common/
│   ├── salshell.h            # NEW: preset ids, probe interface, locate + recipe API, SalGetEnvVarU8
│   └── salshell.cpp          # NEW: pure logic (compiled into salamand + saltests)
├── cmdshell.cpp              # NEW: OpenCommandShell() — resolve, expand, policy check, launch, error
├── mainwnd3.cpp              # CM_DOSSHELL handler → OpenCommandShell()
├── execute.h / execute.cpp   # CommandShellArgsExpArray + Expand/ValidateCommandShellArguments
├── salamdr2.cpp              # DoExpandVarString: $[ENV] read via the wide helper
├── salamdr6.cpp + consts.h   # SafeGetOpenFileNameW (mirror of SafeGetSaveFileNameW)
├── cfgdlg.h                  # CConfiguration fields; CCfgPageCmdShell; CConfigurationDlg member
├── dialogs4.cpp              # CConfiguration defaults; CCfgPageCmdShell impl; Add() after Hot Paths (+ mode map 21→22)
├── mainwnd2.cpp              # CONFIG_CMDSHELL_*_REG names; SaveConfig/LoadConfig lines
├── lang/lang.rc, lang/lang.rh    # IDD_CFGPAGE_CMDSHELL template + control ids
├── lang/texts.rc2, texts.rh2     # new IDS_CMDSHELL_* strings
├── saltests/saltests.cpp         # TestCommandShell071()
└── vcxproj/
    ├── salamand.vcxproj          # + common\salshell.cpp, cmdshell.cpp
    └── saltests/saltests.vcxproj # + common\salshell.cpp

translations/<lang>/salamand.slt  # 8 enabled languages, regenerated by translate.merge
translations/ui-overrides.json    # pins only if a machine translation is wrong

help/src/
├── salamand.hhp                  # [ALIAS] IDD_CFGPAGE_CMDSHELL=hh\salamand\configuration_cmdshell.htm
├── salamand.hhc, salamand.hhk    # TOC + index entries
└── hh/salamand/
    ├── configuration_cmdshell.htm  # NEW topic
    └── othertask_shell.htm         # updated (setting, UNC remark)

CHANGELOG.md, src/plugins/shared/spl_vers.h, setup/tandemcommander.iss, CLAUDE.md   # ship gate
```

**Structure Decision**: single-solution desktop application; the only new
*module* is `src/common/salshell.*`, placed in `src/common/` because `saltests`
links `src/common/*.cpp` only (no main-application sources), and the only new
core file is `src/cmdshell.cpp`, which keeps the launch logic reviewable instead
of growing the 4,000-line `mainwnd3.cpp` handler switch.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

None — no gate is violated.

## Phase Outputs

- **Phase 0** — [research.md](research.md): 19 decisions (R1–R19), every
  open question resolved; includes the terminal programs' verified launch
  semantics (Windows Terminal `-d .`, Git Bash working-directory behaviour,
  PowerShell 7 registry key, app-execution-alias handling).
- **Phase 1** — [data-model.md](data-model.md), [contracts/](contracts/),
  [quickstart.md](quickstart.md).
- **Phase 2** — `/speckit-tasks` generates `tasks.md`. Implementation MUST keep a
  running log (`specs/071-configurable-command-shell/fix-log.md`) as work lands,
  per the project's working convention.

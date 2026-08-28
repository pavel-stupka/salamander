# Feature Specification: Configurable Command Shell

**Feature Branch**: `071-configurable-command-shell`  
**Created**: 2026-08-28  
**Status**: Draft  
**Input**: User description: "Cilem noveho rozireni je pridat moznost konfigurace jaky se otevre command line program pri stisknuti klavesy / na Num klavesnici, resp. na prislusnou ikonu v panelu nastroju. Nyni se otevira prikazovy radek "cmd", ale chtel bych, aby si uzivatel mohl zvolit, resp. zmenit / napr. aby mohl misto toho na Windows zvolit program Terminál, nebo GitBash, nebo PoweShell."

## Background *(today's behaviour)*

The **Command Shell** command — menu *Commands → Command Shell*, the `Num /`
key, `Ctrl+/`, and the *Command Shell* button on the Top or Middle toolbar —
opens the system command interpreter (Command Prompt, `cmd`) with the active
panel's directory as its working directory. Which program opens is not
configurable. The manual (*Opening Command Shell*) documents this and notes
that Command Prompt cannot use a network (UNC) path as its working directory.

A different feature, the **command line** box at the bottom of the main
window, executes *typed* commands through the same system interpreter and has
its own option (*Close shell window after command execution*). It is not part
of this request — see Assumptions.

## Clarifications

### Session 2026-08-28

- Q: Should the chosen shell program also be used by the command line box for
  typed commands, or only by the Command Shell command? → A: Only the Command
  Shell command (`Num /`, `Ctrl+/`, menu, toolbar button); the command line box
  keeps executing typed commands through the system `cmd` (FR-012).
- Q: Where in the Configuration dialog should the setting live — its own page
  or an existing one? → A: A new dedicated *Command Shell* page in the
  Configuration tree; its position among the pages is decided in planning
  (FR-001, FR-015).
- Q: When the user switches to *Custom*, should the program/arguments fields be
  pre-filled from the previously selected preset or start empty? → A:
  Pre-filled with that preset's program path and arguments, but only while the
  Custom fields are still empty — existing custom text is never overwritten
  (FR-008, US2 scenarios 7–8).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Pick a preset shell (Priority: P1)

A user who lives in PowerShell, Windows Terminal or Git Bash opens the
Configuration dialog once, picks that program from a short list and confirms.
From then on, every way of invoking the Command Shell command opens the chosen
program in the active panel's directory. Users who never open the setting keep
exactly today's behaviour (Command Prompt).

**Why this priority**: This is the whole request — one choice, no typing of
paths. The presets cover the three programs named in the request plus both
generations of PowerShell.

**Independent Test**: Select *Windows PowerShell* in Configuration, press
`Num /` in a panel showing `D:\Work` — a PowerShell window appears whose prompt
is at `D:\Work`. Restart the application and press `Num /` again — still
PowerShell. Delivers the complete value of the feature on its own.

**Acceptance Scenarios**:

1. **Given** a fresh installation (setting never touched), **When** the user
   presses `Num /`, **Then** the system Command Prompt opens in the active
   panel's directory, exactly as in version 0.1.5.
2. **Given** the user chose *Windows PowerShell* and confirmed the Configuration
   dialog with OK, **When** the user presses `Num /` while the active panel
   shows `D:\Work`, **Then** a Windows PowerShell window opens with `D:\Work` as
   its current directory.
3. **Given** the user chose *Windows Terminal*, **When** the user clicks the
   Command Shell toolbar button, **Then** Windows Terminal opens (its default
   profile) in the active panel's directory — not in the user's home folder.
4. **Given** the user chose *Git Bash*, **When** the user presses `Ctrl+/` or
   uses *Commands → Command Shell*, **Then** a Git Bash window opens with the
   active panel's directory as its working directory.
5. **Given** a choice was confirmed, **When** the application is closed and
   started again, **Then** the same program opens on `Num /`.
6. **Given** the user changed the selection but left the Configuration dialog
   with Cancel, **When** `Num /` is pressed, **Then** the previously configured
   program opens.
7. **Given** the active panel's directory contains spaces and non-ASCII
   characters (e.g. `G:\Můj disk\Nový projekt`), **When** any preset is
   launched, **Then** its current directory is exactly that folder.

---

### User Story 2 - Use any other program, with arguments (Priority: P2)

A user whose terminal is not in the preset list (cmder, ConEmu, a specific
Windows Terminal profile, a WSL distribution, a portable Git installation in a
non-standard folder, …) picks *Custom*, browses to the executable and
optionally enters arguments. The arguments can refer to the active panel's
directory with the same `$(FullPath)` placeholder the User Menu already uses,
and to environment variables with the same `$[NAME]` form.

**Why this priority**: Presets serve most users; *Custom* is the escape hatch
that makes the feature complete without enumerating every terminal that
exists. It builds on the preset mechanism and can ship after it.

**Independent Test**: Choose *Custom*, program `C:\Tools\cmder\Cmder.exe`,
arguments `/START "$(FullPath)"`; press `Num /` — cmder opens in the panel's
directory.

**Acceptance Scenarios**:

1. **Given** *Custom* is selected with a program path and arguments containing
   `$(FullPath)`, **When** the Command Shell command runs, **Then** the program
   starts with the placeholder replaced by the active panel's directory, and
   the program's working directory is that directory as well.
2. **Given** *Custom* is selected and the program field is empty, **When** the
   user presses OK, **Then** the dialog does not close, explains that a program
   is required and focuses the field.
3. **Given** the user presses *Browse* next to the program field, **When** they
   pick an executable in the file dialog, **Then** its full path appears in the
   program field.
4. **Given** the program path or the arguments contain an environment-variable
   reference (e.g. `$[LOCALAPPDATA]\Microsoft\WindowsApps\wt.exe`), **When**
   the command runs, **Then** the reference is expanded before the program is
   started.
5. **Given** the arguments are left empty, **When** the command runs, **Then**
   the program starts with no arguments and the active panel's directory as its
   working directory.
6. **Given** the active panel's directory contains spaces, **When** the
   arguments contain `"$(FullPath)"` in quotes, **Then** the program receives
   the directory as a single argument.
7. **Given** *Windows Terminal* is the selected preset and the Custom fields are
   empty, **When** the user switches the choice to *Custom*, **Then** the
   program and arguments fields show Windows Terminal's launch command (path
   and the arguments that open it in `$(FullPath)`), ready to be edited — e.g.
   to add a profile.
8. **Given** the Custom fields already contain the user's own program or
   arguments, **When** the user selects a preset and then switches back to
   *Custom*, **Then** the user's text is still there, unchanged.

---

### User Story 3 - Know when the chosen program is not available (Priority: P3)

A user whose configured program is missing — a preset the system did not find
on this machine, a custom path that no longer exists, a configuration imported
from another computer — gets a clear, actionable message instead of nothing
happening or an unexpected program appearing.

**Why this priority**: The default (Command Prompt) is always present; only
users who opted in can hit this. Still, a silent failure would be the worst
possible outcome for a keyboard-driven command, and a user should be able to
tell *which* installation a preset will use.

**Independent Test**: Choose *Custom* with a path to a non-existent executable;
press `Num /`; an error message names the program and the reason, and nothing
else opens.

**Acceptance Scenarios**:

1. **Given** a preset program is not installed on this machine, **When** the
   user opens the setting, **Then** that preset is visibly marked as not found
   and cannot be confirmed as the choice (*Custom* remains available).
2. **Given** the configured program cannot be started at launch time, **When**
   the Command Shell command runs, **Then** an error message names the program
   (its resolved full path), states the system's reason, and points the user to
   the setting in the Configuration dialog; no other program is started
   instead.
3. **Given** the Configuration dialog is open, **When** the user selects a
   preset that was found, **Then** the dialog shows the full path of the
   program that will be launched, so the user can tell which installation is
   used (e.g. PowerShell 7 vs. Windows PowerShell, which Git).

---

### Edge Cases

- **Network (UNC) path in the active panel**: the directory is handed to the
  configured program like any other. Command Prompt itself refuses a UNC
  working directory and falls back to the Windows directory (today's documented
  behaviour); PowerShell, Windows Terminal and Git Bash accept it. The manual
  describes this as the program's own behaviour, not the file manager's.
- **Archive or plugin file system in the active panel** (ZIP, FTP, SFTP, …):
  the behaviour of today's Command Shell command is preserved unchanged (same
  directory rule, same outcome) — only the program differs.
- **Directory longer than the classic 260-character limit**: Windows refuses
  to start *any* program with a starting directory that long (a platform
  limit; version 0.1.5 fails here too). The command retries once with the
  folder's short (8.3) name; if that is unavailable, the launch error names the
  program and the system's reason (US3). Whether the program can then `cd`
  deeper is the program's own capability.
- **The configured program is a launcher that hands off and exits at once**
  (Windows Terminal does this): no error is shown merely because the launcher
  process ended.
- **Group Policy forbids running programs or restricts the allowed list**: the
  configured program is subject to the same policy check as Command Prompt is
  today, and the same policy message appears.
- **Configuration written by a version without this setting** (upgrade from
  0.1.x) or an imported configuration lacking the value: Command Prompt is
  used, without any message.
- **The chosen preset was uninstalled after being chosen**: the launch shows the
  US3 error; reopening the dialog shows the preset as not found and requires a
  new choice before OK.
- **Custom arguments that contain no `$(FullPath)`**: the program still starts
  with the active panel's directory as its working directory (so programs that
  honour the working directory need no arguments at all).
- **Multi-monitor**: the window is requested on the monitor of the main window
  as today; programs that manage their own placement keep their own behaviour.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The Configuration dialog MUST offer a dedicated *Command Shell*
  page holding the setting that determines which program the Command Shell
  command opens (all controls on that page — no sub-dialog; the page's position
  in the Configuration tree is a planning decision).
- **FR-002**: The setting MUST offer at least these presets — *Command Prompt*
  (the system command interpreter), *Windows PowerShell*, *PowerShell 7*,
  *Windows Terminal*, *Git Bash* — plus *Custom*.
- **FR-003**: The default MUST be *Command Prompt* and MUST reproduce today's
  behaviour exactly (same program, same working directory, same window title
  and placement), so that users who never touch the setting see no change
  (constitution principle II: user-facing changes are opt-in).
- **FR-004**: Every entry point of the Command Shell command — `Num /`,
  `Ctrl+/` (in the panel and in the command line box), *Commands → Command
  Shell*, and the Command Shell button on any toolbar — MUST open the
  configured program. There is one setting, not one per entry point.
- **FR-005**: The configured program MUST start with the active panel's
  directory as its working directory, following the same rule the command uses
  today (disk paths; archive and plugin file-system panels unchanged).
- **FR-006**: Each preset MUST open directly in that directory — in particular
  Windows Terminal MUST NOT open in the user's home folder — and MUST work for
  directories containing spaces.
- **FR-007**: Presets MUST be located automatically at their standard
  installation locations. The dialog MUST show which presets were found and the
  full path of the selected preset's program, MUST mark presets that were not
  found, and MUST NOT let the user confirm a preset that was not found.
- **FR-008**: *Custom* MUST accept a program path (with a *Browse* button for
  executables) and optional arguments; the program path is required. The
  arguments MUST support the `$(FullPath)` placeholder for the active panel's
  directory — with the meaning the User Menu gives it in its *Initial
  Directory* field: no trailing backslash (`D:\Work`), drive root excepted
  (`C:\`) — so that `"$(FullPath)"` in quotes reaches the program as one
  argument; both fields MUST support `$[NAME]` environment-variable references
  with the same meaning as in the User Menu. When the user
  switches the choice to *Custom* while both Custom fields are empty, the
  fields MUST be pre-filled with the program path and arguments of the preset
  selected until then; non-empty Custom fields MUST never be overwritten by a
  preset.
- **FR-009**: The setting MUST be stored with the rest of the user's
  configuration: it survives restart, is carried by *Export Configuration* /
  *Import Configuration*, and takes effect for the next Command Shell
  invocation immediately after the dialog is confirmed — no restart.
- **FR-010**: If the configured program cannot be started, the command MUST show
  an error that names the program (its resolved full path) and the system's
  reason and points to the setting; it MUST NOT silently start a different
  program instead.
- **FR-011**: The Group Policy run restrictions that apply to the Command Shell
  command today MUST apply to the configured program.
- **FR-012**: The command line box at the bottom of the main window MUST
  continue to execute typed commands through the system command interpreter
  regardless of this setting (see Assumptions).
- **FR-013**: The menu label, keyboard shortcuts, toolbar icon and tooltip of
  the Command Shell command MUST NOT change with the selected program.
- **FR-014**: All new user-visible text MUST be available in every shipped UI
  language.
- **FR-015**: The user manual MUST gain a page for the new *Command Shell*
  configuration page (listed with the other Configuration pages), and the
  *Opening Command Shell* page MUST be updated —
  including replacing the "UNC paths are not supported" remark with a
  statement that this depends on the chosen program.
- **FR-016**: The panel directory, the program path and the expanded arguments
  MUST be handled as Unicode end to end (the feature-004 house rules): a
  non-ASCII directory or installation path MUST NOT be garbled or replaced by
  `?` anywhere on the way to the launched program.

### Key Entities *(include if feature involves data)*

- **Command Shell setting**: the user's choice — one of the presets or
  *Custom*; for *Custom*, the program path and the argument text. Exactly one
  per user configuration; stored alongside the other options.
- **Shell preset**: a named, built-in launch recipe: display name, how the
  program is located on this machine, the arguments that make it open in a
  given directory, and whether it was found. Presets are fixed by the product;
  users cannot add their own (they use *Custom* instead).
- **Launch context**: the active panel's directory and the entry point that
  triggered the command; consumed by the launch, never stored.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user switches from Command Prompt to an installed preset in at
  most 5 interactions (open Configuration, choose, confirm) and under 30
  seconds end to end, and the very next `Num /` opens the chosen program — no
  restart.
- **SC-002**: With the setting untouched, the Command Shell command is
  indistinguishable from version 0.1.5 in a side-by-side comparison (program,
  working directory, window title, placement) across all entry points.
- **SC-003**: For every preset installed on the test machine, the launched
  window's current directory equals the active panel's directory in 100% of
  trials over a set of test folders that includes spaces, non-ASCII characters
  (`G:\Můj disk`) and — for programs that accept them — a UNC path.
- **SC-004**: All entry points (`Num /`, `Ctrl+/` in the panel and in the
  command line box, menu, toolbar) produce the same result for the same
  setting — zero divergence in a manual matrix test.
- **SC-005**: When the configured program is missing or fails to start, the user
  sees an error naming the program within 2 seconds of the command, in 100% of
  cases; there is no case in which the command does nothing.
- **SC-006**: The setting survives a restart and an *Export Configuration →
  Import Configuration* round trip unchanged (100%).
- **SC-007**: The setting and every message it can show appear fully
  translated in all 8 shipped UI languages — no English text in a non-English
  UI.

## Assumptions

- **The command line box is out of scope.** Commands typed there are written
  in Command Prompt syntax, and the *Close shell window after command
  execution* option is tied to that interpreter; letting it use PowerShell or
  Bash would change the meaning of every typed command. This feature covers
  only the Command Shell command (`Num /`, `Ctrl+/`, the menu item and the
  toolbar button), as the request states; extending the choice to the command
  line is a separate, later decision.
- The *Command Prompt* preset is the system command interpreter exactly as
  today (the `COMSPEC` environment variable, normally `cmd.exe`), so the
  default is today's behaviour bit for bit.
- Preset locations: *Windows PowerShell* is part of Windows; *PowerShell 7*
  and *Git Bash* are looked up at their installers' standard locations;
  *Windows Terminal* is located through its per-user app alias (present
  whenever the Store or inbox app is installed). Installations elsewhere are
  served by *Custom*.
- *Windows Terminal* opens its default profile; a specific profile is a
  *Custom* configuration (the user passes the profile argument themselves).
- Elevated ("run as administrator") shells, per-drive or per-panel shell
  choices, user-defined presets, and changing the toolbar icon per program are
  out of scope.
- The window title "Command Shell" and the placement on the main window's
  monitor are requested from every program as today; programs that ignore them
  (Windows Terminal, Git Bash's own terminal) keep their own behaviour.
- The new configuration controls follow the house style (constitution
  principle VI); no plugin interface change is needed.
- New strings are translated through the existing translation pipeline
  (machine translation with usage context, as in feature 055) and pinned by
  hand where the automatic result is wrong.
- Manual pages affected: *Opening Command Shell* (`othertask_shell.htm`,
  updated) and a new page for the *Command Shell* configuration page (added to
  the Configuration chapter and the help index); the keyboard-shortcuts page
  needs no change (shortcuts are unchanged).

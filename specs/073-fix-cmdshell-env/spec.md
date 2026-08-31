# Feature Specification: Command Shell Environment Parity

**Feature Branch**: `073-fix-cmdshell-env`  
**Created**: 2026-08-31  
**Status**: Parked — not reproduced (decision of 2026-08-31, see Clarifications); no product change; `evidence/capture.cmd` is the one-click capture for the next occurrence  
**Input**: User description: "Nova funkce v aplikaci implementovana v 071-configurable-command-shell zrejme zavedla do projektu chybu. Kdyz pomoc / spustim prikazovy radek (cmd) a pak pres nej muj node.js projekt (npm run dev), dostavam chybu. Chyba zrejme bude souviset s promenymi prostredi, protoze kdyz cmd spustim normalne jako program, tak vse funguje." — followed by an analysis of the user's own project that attributes the failure to Tandem Commander handing its child processes a changed or missing `USERPROFILE` (the project locates model files under `os.homedir()\.cache\huggingface\hub`, and `homedir()` on Windows is `USERPROFILE` inherited from the parent process), and recommends checking `whoami` and `echo %USERPROFILE%` in a shell opened from Tandem Commander.

## Background *(the report, and what is verified)*

### The report

After this morning's update to version 0.1.7, pressing `Num /` in Tandem
Commander and running `npm run dev` in the window that opens fails with a
misleading "Required model files not found", although the files are on disk.
The same command from a Command Prompt started the normal way (Start menu)
works. Nothing in the project changed; the launcher did. The Command Shell
command was made configurable in feature 071 (shipped in 0.1.6 — 0.1.7 changed
only the installer), so the suspicion falls on that change.

**Clarified on 2026-08-31**: the same failure occurs *without* the Command
Shell command. Starting the project's `.bat` file by `Enter`/double-click in
a panel — the batch file starts a further shell and finally runs `npm run
dev` — fails the same way; the same file double-clicked in Explorer works.
That path involves neither the Command Shell command, the preset nor Windows
Terminal, and it runs Command Prompt. The common factor is therefore **the
environment of the Tandem Commander process**, which every program it starts
inherits — the Command Shell, `Enter`/double-click on a file, the User Menu,
the command line box alike.

**Follow-up, later the same day — the failure no longer reproduces.** After
the project had been started successfully once from Explorer (and once by
the project's own analysis session), `npm run dev` started from Tandem
Commander works again by both paths — from the very instance whose
environment was measured identical to Explorer's (V6). The instance that
failed in the morning had been closed before anything was captured. The
day's timeline from the machine: PC booted 07:39:43, logon 07:40:04, Tandem
Commander 0.1.7 installed 07:48:19 unattended (the installer never launches
the application in a silent install), the failing runs and the project
analysis followed from an instance the user started; instances of 09:45 and
10:11 were started from Explorer. The Hugging Face cache the project looks
in is a plain local folder under `C:\Users\<user>\.cache` — no junction, no
late-mounted drive — and its `livekit--turn-detector` entry is unchanged
since 2026-06-17, so the successful runs did not rewrite it either.

### What the command does today

The Command Shell command (`Num /`, `Ctrl+/`, *Commands → Command Shell*, the
toolbar button) starts the program chosen on the *Configuration → Command
Shell* page — a preset (Command Prompt, Windows PowerShell, PowerShell 7,
Windows Terminal, Git Bash) or a Custom program — in the active panel's
directory. It hands the program **the environment of the Tandem Commander
process itself**; it neither builds nor edits an environment of its own. That
was true before feature 071 and is true after it.

Two other mechanisms shape the environment of the Tandem Commander process,
and therefore of every program it starts (the Command Shell, but equally the
User Menu, `Enter` on an executable, and the command line box):

- **The parent.** Tandem Commander inherits the environment of whatever
  started it — normally Explorer (Start menu, taskbar, desktop shortcut,
  double-click). Started from the installer's *Launch Tandem Commander*
  checkbox or from a terminal, it inherits *that* environment instead.
- **"Keep environment variables updated to system values"** (*Configuration →
  General*, on by default, on in the reporter's configuration). With it on,
  Tandem Commander asks Windows to regenerate its environment from the system
  settings **immediately at startup** and again whenever Windows announces an
  environment change. The regenerated environment replaces the inherited one;
  variables the regeneration dropped are put back, but values the regeneration
  *changed* are kept as regenerated — the mechanism explicitly ignores changed
  values. This is Open Salamander code, unchanged since 2023.

### What was verified on the reporting machine (2026-08-31)

The failing window itself could not be opened by anyone but the user (it
needs a key press in the running application), so each mechanism was
exercised in isolation on the reporting machine — Windows 11 26200, Tandem
Commander 0.1.7 in `C:\Program Files`, started from Explorer, Windows
Terminal 1.24.11911.0:

| # | Mechanism | Method | Result |
|---|-----------|--------|--------|
| V1 | The configured launch | Read the configuration | The preset is **Windows Terminal**, and Windows Terminal's default profile on this PC is **Git Bash**. So `Num /` opens a Git Bash tab in Windows Terminal — not Command Prompt. |
| V2 | Command Prompt preset launch (direct child, inherited environment) | Started `cmd /c set` the way the command starts it, from a process carrying a marker variable | All 96 variables of the launcher present, marker included; `USERPROFILE=C:\Users\pavel`. |
| V3 | Windows Terminal preset launch (`wt.exe -d .`) | Same, through the Windows Terminal alias | All 96 variables present, marker included — Windows Terminal 1.24 forwards the launcher's environment into the new tab. Only `WT_SESSION` and `WT_PROFILE_ID` differ. |
| V4 | Startup regeneration ("Keep environment variables updated") | Ran the identical Windows call inside a helper process given Explorer's exact environment (48 variables, read from the running Explorer) | `USERPROFILE`, `HOMEDRIVE`, `HOMEPATH`, `APPDATA`, `LOCALAPPDATA`, `TEMP`, `TMP`, `USERNAME`, `PATH` all unchanged; four variables dropped and put back (Explorer-internal `EFC_*`, the marker). Two value changes first attributed to the regeneration (`PATHEXT`, `PSModulePath`) proved to be the helper's own doing — PowerShell adds `.CPL` and its user module folder to its own process — Explorer's values are the registry's, and V6 shows the product keeps them. |
| V5 | Elevation / identity | Process owner and parent of the running Tandem Commander | Owner `pavel`, parent `explorer.exe`, not elevated. |
| V6 | The real product, started from Explorer | Read the environment blocks of the running Tandem Commander (started 10:11:32 by Explorer, after the startup regeneration had run) and of Explorer itself | **Identical**: 47 vs 48 entries, the only difference being Explorer's internal `=::=::\` bookkeeping entry; `PATHEXT`, `PSModulePath`, `PATH`, every profile-derived variable equal. |

Conclusions the specification rests on:

1. The `USERPROFILE` explanation is **not reproduced**: none of the three
   mechanisms alters `USERPROFILE` or any other profile-derived variable on
   this machine. The report is not doubted — something differs between the
   two windows — but the differing variable, or whether the difference is the
   environment at all, is not yet established.
2. **No deviation** from an Explorer-started process is demonstrated on the
   product today (V6): after the startup regeneration, Tandem Commander's
   environment equals Explorer's. The regeneration remains a *risk* worth
   guaranteeing against — it replaces inherited values with registry-derived
   ones whenever the two differ, for example after an installer rewrote
   `PATH` without telling running programs — which is what the parity
   guarantee below pins down; it is not a demonstrated defect.
3. On this machine the window `Num /` opens is **Git Bash**, while the
   working comparison is **Command Prompt**. A shell difference (Windows
   batch syntax such as `%USERPROFILE%` is not expanded by bash; `HOME` is
   set) could explain the Command Shell path alone — but the `.bat` path
   runs Command Prompt and fails too, so the shell is not the common factor.
   It remains something the reproduction rules out, not the leading
   explanation.
4. Because both paths inherit the same thing, the decisive capture is
   **Tandem Commander's own environment while it is in the failing state,
   compared with Explorer's** (`evidence/penv.ps1` reads both from the
   running processes). If they differ, the differing variable is the cause;
   if they are identical, the cause is not the environment. V6 shows them
   identical at the time of writing — so if the `.bat` still fails from that
   instance, the cause is not an environment variable, and the capture must
   cover the whole process tree (`evidence/treeenv.ps1`: command line,
   working directory and environment of every process under Tandem
   Commander, node included).
5. At the time of writing the report is **not reproducible**, and no
   product-side deviation is demonstrated by V2–V6. What decides the case
   is the next occurrence: `evidence/capture.cmd` is the one-click capture
   to run at that moment, before the failing window or Tandem Commander is
   closed.

The scripts and measured results are kept in `evidence/` so the plan phase
can rerun them.

## Clarifications

### Session 2026-08-31

- Q: In which window was `npm run dev` typed when it failed — the Git Bash
  tab `Num /` opens, a Command Prompt started inside it, or a classic cmd
  window? → A: Both the Command Prompt opened with `/` **and** the project's
  `.bat` file started by `Enter`/double-click from a panel (it starts a
  further shell and finally `npm run dev`) failed; from Explorer both work.
  So the defect is not specific to the Command Shell command — every program
  started from Tandem Commander is affected, which points at the environment
  of the Tandem Commander process itself (Background, "Clarified").
- Q: Can the failure be reproduced now, so that Tandem Commander's live
  environment is captured before planning — or should the plan proceed with
  the parity guarantee and treat the reproduction as the final gate? → A:
  Reproduce now: the reporter starts Tandem Commander, triggers the failure
  (`Enter` on the `.bat`), and runs `evidence/penv.ps1` and
  `evidence/treeenv.ps1` **before** `/speckit-plan`; the plan targets what
  they name (FR-005, US2 scenario 1). If the environments are identical,
  the plan records that the cause is not an environment variable and works
  from the process-tree capture instead.
- Q: (follow-up, same day) Does the failure reproduce with the instance
  measured in V6? → A: **No.** After one successful run from Explorer, `npm
  run dev` started from Tandem Commander works again by both paths, from the
  same instance; the morning's failing instance was closed before any
  capture. The report is therefore not reproducible at present (Background,
  "Follow-up"); `evidence/capture.cmd` stays ready for the next occurrence.
- Q: The failure cannot be reproduced and no environment deviation is
  demonstrated in the product — what happens to this feature: park it,
  continue with documentation only, or implement the full protective parity
  guarantee anyway? → A: **Park it as not reproduced.** No product change;
  this specification, the evidence and `capture.cmd` stay on branch
  `073-fix-cmdshell-env` as the record and the ready tool. At the next
  occurrence the reporter runs `capture.cmd` before closing the failing
  window or Tandem Commander, and the feature resumes from what it captures
  (FR-005). The requirements above describe what would be built if it does.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A program started from Tandem Commander sees the environment it would see from Explorer (Priority: P1)

A user opens the Command Shell from a panel, or presses `Enter` on a `.bat`
file, and runs their tooling — `npm run dev`, a build script, a Python
virtual environment, a `git` command. Every program started that way must
find the same environment variables, with the same values, that it finds
when the same shell is started from the Start menu or the same file is
double-clicked in Explorer:
the same user, the same home folder, the same `PATH`, the same temporary
folder, the same user-defined variables — including values that contain
accented characters. Nothing is missing, nothing is added, nothing is changed.

**Why this priority**: This is the guarantee the report says is broken. It is
also the only guarantee that makes the Command Shell trustworthy for
development work: a launcher that silently hands out a different environment
produces exactly the kind of misleading failure the report describes, and the
user has no way to see it.

**Independent Test**: Open the Command Shell with each preset available on the
test machine and with a Custom program, record the full environment in the
window that opens, record it again from the same shell started from the Start
menu, and compare. Delivers the complete value on its own: the difference is
empty except for the documented allow-list (the shell host's own session
variables, the prompt, and the per-drive current-directory bookkeeping the
command has always set).

**Acceptance Scenarios**:

1. **Given** Tandem Commander was started from Explorer and the *Command
   Prompt* preset is configured, **When** the user presses `Num /` and runs
   `set`, **Then** every variable and value matches a Start-menu Command
   Prompt's `set`, apart from the allow-list.
2. **Given** the *Windows Terminal* preset is configured, **When** the user
   presses `Num /` and lists the environment in the tab that opens, **Then**
   the same holds — including a variable that exists only in Tandem
   Commander's own environment (it must reach the tab).
3. **Given** the *Windows PowerShell*, *PowerShell 7* or *Git Bash* preset, or
   a *Custom* program, is configured, **When** the command runs, **Then** the
   same holds for that program.
4. **Given** a user-level environment variable whose value contains
   characters outside the Windows code page (for example `TC_TEST=Můj
   adresář`), **When** any preset is launched, **Then** the program receives
   the value unchanged, character for character.
5. **Given** "Keep environment variables updated to system values" is **on**
   (the default), **When** the user presses `Num /` right after Tandem
   Commander starts, **Then** the environment matches Tandem Commander's own
   parent (Explorer) exactly — no variable is missing, added or changed by
   the startup regeneration. *(V6 shows this holds today; the scenario pins
   it.)*
6. **Given** the option is **off**, **When** the user presses `Num /`, **Then**
   the environment is the one Tandem Commander was started with, unchanged.
7. **Given** the option is on and the user adds a new user variable (or
   changes `PATH`) in *System Properties* while Tandem Commander is running,
   **When** they press `Num /` afterwards, **Then** the new value is present —
   exactly as it would be in a Command Prompt newly opened from the Start
   menu — and every variable Tandem Commander had inherited only from its
   parent is still present with its original value.
8. **Given** the panel shows a folder with spaces and accented characters,
   **When** the command runs, **Then** the working directory is that folder
   (feature 071's promise, re-checked because the fix touches the same
   launch).
9. **Given** a `.bat` file in the panel, **When** the user presses `Enter` on
   it (or double-clicks it), **Then** the batch file and every program it
   starts in turn see the same environment as when the file is double-clicked
   in Explorer — the reporter's second failing path.

---

### User Story 2 - The reported failure is reproduced, explained and gone (Priority: P1)

The user who reported the problem presses `Num /` and runs `npm run dev` in
the window that opens — or presses `Enter` on the project's `.bat` file — and
the project starts, exactly as it does from a Command Prompt started from the
Start menu or from the file double-clicked in Explorer. Before that, the
failure is reproduced with evidence that names what differed, so that the fix
targets the actual cause rather than the hypothesis.

**Why this priority**: The report is the reason for the feature. A parity fix
that leaves the reporter's scenario failing has not delivered; a fix without a
reproduction cannot claim to have delivered.

**Independent Test**: On the reporting machine, with the reporter's
configuration untouched: (a) before the fix, with Tandem Commander running
and either path failing, capture Tandem Commander's own environment and
Explorer's from the running processes (`evidence/penv.ps1`) and — while the
failing stack, or the shell the `.bat` opened, is still up — the whole
process tree under Tandem Commander (`evidence/treeenv.ps1`: command line,
working directory and environment of each process, `node` included), and
record the difference; additionally capture, from the window `Num /` opens
and from a Start-menu Command Prompt, the full environment (`set` in Command
Prompt, `env` in Git Bash), `whoami`, the home folder the shell reports, and
where `node` and `npm` resolve; (b) after the fix, `npm run dev` succeeds from the
window `Num /` opens and the `.bat` file started with `Enter` runs the
project.

**Acceptance Scenarios**:

1. **Given** the reporter's machine with Tandem Commander in the failing
   state, **When** its environment and the process tree under it are
   compared with Explorer's, and the two windows' captures are compared,
   **Then** the record names every differing variable — or states that the
   environments are identical and names what differed instead (working
   directory, the program actually started, the shell, how Tandem Commander
   was started).
2. **Given** the difference is a variable Tandem Commander changed, dropped or
   added, **When** the fix is applied, **Then** the variable matches
   Explorer's and User Story 1's test covers it permanently.
3. **Given** the difference is not in the environment (same variables — for
   example a different shell, or a different way Tandem Commander was
   started), **When** the investigation closes, **Then** the record says so
   plainly, the product makes the behaviour visible (User Story 3), and the
   reporter is told what to change.
4. **Given** the fix is installed, **When** the reporter presses `Num /` and
   runs `npm run dev`, and separately presses `Enter` on the project's `.bat`
   file, **Then** the project starts on the first attempt both ways, three
   times out of three.

---

### User Story 3 - The user can tell what the Command Shell opens and what it inherits (Priority: P2)

A user who chooses the *Windows Terminal* preset learns — on the configuration
page and in the manual — that it opens Windows Terminal's **default profile**,
which may be any shell (on the reporting machine it is Git Bash, not Command
Prompt). The manual's Command Shell topic also states that the program
inherits Tandem Commander's environment, which in turn is the environment of
whatever started Tandem Commander, and shows how to compare environments when
a program behaves differently than from Explorer.

**Why this priority**: The report calls the opened window "cmd"; it is a Git
Bash tab. When what opens is invisible, every difference — real or not — gets
attributed to the launcher. This closes that gap without changing behaviour.

**Independent Test**: Read the *Command Shell* configuration page and the
manual topic: both name the "default profile" behaviour of the Windows
Terminal preset; the topic explains inheritance and the comparison procedure.

**Acceptance Scenarios**:

1. **Given** the Windows Terminal preset is selected on the configuration
   page, **When** the user looks at the page, **Then** it says the program
   opens Windows Terminal's default profile.
2. **Given** the user opens the manual topic for the Command Shell, **When**
   they read it, **Then** it states which environment the program receives,
   what changes it when Tandem Commander is started from the installer or a
   terminal, and how to compare the environment of a window opened from
   Tandem Commander with one opened from the Start menu.
3. **Given** the fix ships, **When** the user reads `CHANGELOG.md`, **Then**
   the entry describes the symptom that is gone and is truthful about what
   was and was not reproduced.

---

### Edge Cases

- **Tandem Commander started from somewhere other than Explorer** — the
  installer's *Launch Tandem Commander* checkbox, a terminal, a script. It
  inherits *that* parent's environment and so does every shell it opens; this
  is how Windows works and is documented (User Story 3), not "fixed". Parity
  is defined against Tandem Commander's own parent.
- **"Keep environment variables updated" off**: no regeneration ever; a
  system change made while the application runs is not seen until restart —
  today's documented behaviour, unchanged.
- **A system environment change while the application runs** (option on):
  the changed values must win, as in a fresh Explorer window; variables
  inherited only from the parent must survive with exact values.
- **Variable values outside the Windows code page** in a variable the
  regeneration drops and the application puts back: must be put back
  unchanged (today's put-back goes through the code-page string API).
- **Windows Terminal reusing its running process**: Windows Terminal is a
  single process for all its windows; the environment it forwards from the
  launcher (V3) must be what the tab gets even when a Windows Terminal window
  started earlier is already open. A Windows Terminal version that does not
  forward the launcher's environment is a limitation of that version; the
  parity check records the version it ran on.
- **The per-drive current-directory entries** (`=C:` and the like) the
  command has always set for its child are bookkeeping, not variables; the
  comparison ignores them.
- **Elevated Tandem Commander** (*Run as administrator*): shells opened from
  it are elevated too and see the elevated environment; comparing against a
  non-elevated Start-menu shell is not meaningful — out of scope.
- **Custom program with `$[NAME]` in its arguments**: expansion reads the
  application's environment; after the fix that is the parent's environment,
  so the expanded value matches what a Start-menu shell would expand.
- **A `.bat` (or any file) opened from a panel** starts through the file's
  association, not through the Command Shell launch; it inherits the same
  application environment and is covered by the same guarantee and the same
  parity check (User Story 1, scenario 9).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Every program started by the Command Shell command — each
  preset and a Custom program — and every program started by `Enter`/
  double-click on a file in a panel MUST receive the complete environment of
  the Tandem Commander process: every variable, with its exact value
  including characters outside the Windows code page, and no additions
  beyond the per-drive current-directory bookkeeping these launches have
  always set.
- **FR-002**: Immediately after Tandem Commander starts, its environment as
  seen by a program it starts MUST be identical to the environment it
  inherited from its parent — no variable missing, none added, no value
  changed — whether "Keep environment variables updated to system values" is
  on or off. *(V6 shows this holds on the reporting machine today; the
  requirement makes it a guarantee rather than a coincidence of the current
  registry contents.)*
- **FR-003**: With the option on, after Windows announces an environment
  change, a program started afterwards MUST see the changed and added values
  as a newly opened Explorer window would, and every variable Tandem
  Commander inherited only from its parent MUST still be present with its
  original, exact value.
- **FR-004**: A repeatable parity check MUST exist and be documented: start a
  program through the command's launch path and through a plain start,
  compare the two environments, and report every difference outside an
  explicit allow-list (the shell host's session variables such as
  `WT_SESSION`/`WT_PROFILE_ID`, the prompt, the per-drive bookkeeping). The
  check MUST run in the automated test suite for the parts that need no
  desktop interaction, and as a documented manual procedure for the rest.
- **FR-005**: The reported scenario — both the Command Shell path and the
  `.bat`-from-panel path — MUST be reproduced on the reporting machine, or
  refuted with the captures of User Story 2 (including Tandem Commander's
  own environment read from the running process against Explorer's), before
  the feature is complete; the record MUST name the differing variables or
  state that the environments were identical and what differed instead. The
  live capture is taken by the reporter **before planning** (Clarifications,
  2026-08-31) and stored in `evidence/`; the plan MUST start from what it
  names.
- **FR-006**: The *Command Shell* configuration page MUST state, for the
  Windows Terminal preset, that it opens Windows Terminal's default profile;
  the manual's Command Shell topic MUST state what environment the program
  receives, what changes it (how Tandem Commander was started; the
  "Keep environment variables updated" option), and the comparison procedure
  of FR-004.
- **FR-007**: Choosing which Windows Terminal profile the preset opens is
  **out of scope** for this feature. Should the reproduction (FR-005) prove
  the opened shell to be the cause, a profile choice becomes a follow-up
  feature and this one stops at FR-006. *(Decided by default on 2026-08-31:
  the `.bat` path fails too, so the shell is not the common factor.)*
- **FR-008**: The fix MUST NOT change the plugin interface, the configuration
  layout or version, or the Command Prompt preset's launch (working
  directory, window placement, policy check — feature 071's bit-for-bit
  promise) beyond the environment guarantee itself; users who never touched
  either setting keep today's behaviour except that their environment is now
  guaranteed to match their parent's.
- **FR-009**: The release that ships the fix MUST carry a `CHANGELOG.md`
  entry, in the user's terms, truthful about what was reproduced, what was
  fixed and what turned out not to be a defect.

### Key Entities

- **Process environment**: the set of name/value pairs a program starts with;
  inherited from the parent unless the launcher supplies its own. Values are
  Unicode text; a value may contain characters outside the Windows code page.
- **Parity comparison**: two environment captures (one from a program started
  by Tandem Commander, one from the same program started plainly), the
  allow-list of expected differences, and the resulting list of unexpected
  differences — empty when parity holds.
- **Launch path**: how a program is started — a Command Shell preset (Command
  Prompt, Windows PowerShell, PowerShell 7, Windows Terminal, Git Bash), a
  Custom program, or a file opened from a panel by `Enter`/double-click; the
  Windows Terminal path adds a hand-off to a shell host that is not the
  child of Tandem Commander.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On the reporting machine, `npm run dev` started from the window
  the Command Shell command opens, and the project's `.bat` file started with
  `Enter` in a panel, both succeed on the first attempt, three runs out of
  three, exactly as from a Start-menu Command Prompt / Explorer.
- **SC-002**: For every preset installed on the test machine and one Custom
  program, the environment of the started program differs from a plainly
  started one by **zero** variables outside the allow-list.
- **SC-003**: Immediately after Tandem Commander starts, **zero** variables
  differ between its environment and its parent's (V6: zero today — the
  criterion pins the guarantee).
- **SC-004**: A system environment change made while Tandem Commander runs
  appears in the next shell opened from it without a restart, with 100 % of
  the parent-inherited variables still present and unchanged.
- **SC-005**: A variable whose value contains characters outside the Windows
  code page reaches the started program unchanged in 100 % of the presets.
- **SC-006**: A reader of the configuration page and the manual topic can
  answer, from the text alone, which shell the Windows Terminal preset opens
  and how to compare environments (both facts present — verified by
  inspection).
- **SC-007**: The existing automated test suite passes with the new parity
  checks added; the plugin interface version is unchanged.

## Assumptions

- Tandem Commander is normally started from Explorer (Start menu, taskbar,
  shortcut, double-click); that parent's environment is the parity baseline.
  Other parents are documented, not corrected.
- The reporting machine: Windows 11 26200; Tandem Commander 0.1.7 (build 191)
  in `C:\Program Files\Tandem Commander`, started from Explorer, not
  elevated; Windows Terminal 1.24.11911.0 with Git Bash as its default
  profile; Command Shell preset = Windows Terminal; "Keep environment
  variables updated to system values" at its default (on). The reporter's
  configuration stays as it is for the reproduction.
- Windows Terminal 1.24 forwards the launcher's environment into the new tab
  (V3). Older Windows Terminal versions are not a target of this feature.
- The attached project analysis is treated as a hypothesis, not a finding:
  its author did not observe the window opened from Tandem Commander, and its
  `USERPROFILE` claim was tested (V2–V4) and not reproduced.
- The failure has not yet been reproduced with captures by anyone; the
  reporter takes the live capture before planning (Clarifications), and the
  feature is complete only when User Story 2 is (FR-005). The parity
  guarantee (FR-001–FR-004) ships as an invariant with tests even if the
  reproduction finds the cause elsewhere — no deviation is demonstrated
  today (V6), so its value is protection, not repair.
- The environment guarantee is process-wide: it covers the Command Shell,
  `Enter`/double-click on a file (the reporter's second failing path), the
  User Menu and the command line box. Verification exercises the first two,
  which the report is about.
- The Command Prompt preset keeps feature 071's bit-for-bit launch; the
  "Keep environment variables updated" option keeps its purpose (picking up
  system changes without a restart) — only its startup side effect goes.
- No new dependency; no configuration migration; the fix is a `MINORB`
  release per the constitution's release rules.

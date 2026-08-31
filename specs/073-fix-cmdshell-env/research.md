# Research: Command Shell Environment Parity

**Feature**: 073-fix-cmdshell-env · **Date**: 2026-08-31 · **Status**: shelf
design (feature parked — see spec Clarifications)

Every item below was resolved from the source at HEAD and from the
measurements recorded in [evidence/results-2026-08-31.md](evidence/results-2026-08-31.md).
The single fact research cannot supply is the cause of the reported failure;
that is the reproduction gate (FR-005), not a research item.

## R1 — What each launch path inherits (fact base)

**Decision**: treat the **process environment block** of `tandemcommander.exe`
as the single source of truth; no launch path builds or edits an environment
of its own, and none will.

**Rationale** (code at HEAD):

| Path | Code | Environment | Working directory / bookkeeping |
|------|------|-------------|-------------------------------|
| Command Shell (`Num /`, `Ctrl+/`, menu, toolbar) | `CMainWindow::OpenCommandShell`, `src/cmdshell.cpp` → `SalCreateProcess(NULL app, cmdLine, …, lpEnvironment = NULL, curDir, …)` (`src/common/salfileio.cpp:120` → `CreateProcessW`) | inherited block | `SetDefaultDirectories()` first (`=A:`…`=Z:`), then `curDir` = panel path |
| Windows Terminal preset | same, program = `wt.exe` alias, args `-d .` | inherited block handed to `wt.exe`; Windows Terminal 1.24 forwards it into the tab it creates (spec V3, marker variable present) | `-d .` = the inherited cwd |
| `Enter` / double-click on a file (`.bat` included) | `CFilesWindow::Execute` (`src/fileswn2.cpp:306`) → `SetDefaultDirectories()` → `ExecuteAssociation` (`src/shellsup.cpp:2610`) → `IContextMenu2::InvokeCommand` with `CMINVOKECOMMANDINFOEX`, `lpDirectoryW` = panel path — shell32 runs the association **in-process** | inherited block | panel path |
| User Menu, command line box, external viewers/editors | `src/execute.cpp` → `SalCreateProcess` / `SalShellExecuteEx` | inherited block | per command |
| `salmon.exe` (crash reporter, startup) | `src/salmoncl.cpp` — appends its folder to `PATH`, spawns, **restores** `PATH` | inherited block + one `PATH` entry (child only) | install folder |

Measured on the reporting machine (spec V2, V3, V6): identical to the launcher
/ to Explorer, `USERPROFILE` included.

**Alternatives considered**: building a fresh environment block per launch
(`CreateEnvironmentBlock` from the user token) — rejected: it would *discard*
what the parent passed down (the opposite of parity), duplicate the `=X:`
bookkeeping, and change five call sites for a problem that lives in one.

## R2 — Lifecycle of the process block and where parity can break

**Decision**: the only product mechanism that can make the block differ from
the parent's is the *Keep environment variables updated to system values*
regeneration (`src/salamdr7.cpp`); the fix, if one is ever warranted, goes
there and nowhere else.

**Rationale**: at startup (`src/salamdr1.cpp:4470`, when
`Configuration.ReloadEnvVariables`, default TRUE — `src/dialogs4.cpp:305`)
`InitEnvironmentVariablesDifferences()` snapshots the block (ANSI
`GetEnvironmentStrings`, `=`-prefixed entries skipped), calls shell32
`RegenerateUserEnvironment(&prev, TRUE)` — which **replaces** the block with
one generated from the registry and the profile — snapshots again, computes
the difference and re-adds only the variables that vanished (`ENVVARTYPE_ADD`);
deletions and value changes are deliberately ignored ("we decided it is
better not to delete anything… Petr+Honza"; "differences are ignored for now,
for example in PATH"). On `WM_SETTINGCHANGE` with `lParam == "Environment"`
(`src/mainwnd3.cpp:1351`) `RegenEnvironmentVariables()` regenerates again and
re-applies the startup difference. Consequences:

- Immediately after startup the block equals the *regenerated* environment
  plus inherited-only variables — equal to the parent's only while the
  registry-derived values coincide with what the parent had. On the reporting
  machine they coincide (V6; the two changes my V4 simulation reported were
  PowerShell's own edits of the helper process — corrected in the spec).
- Re-added variables pass through the ANSI `SetEnvironmentVariable`: a value
  outside the code page comes back as `?`.
- If a registry `PATH` was rewritten by an installer without a broadcast, a
  Tandem Commander started afterwards would differ from an Explorer that never
  regenerated — the class of difference the report describes, though not
  demonstrated.

**Alternatives considered**: removing the option — rejected (its purpose,
picking up system changes without a restart, is real and Explorer-like).

## R3 — Semantics of the guarantee (FR-002 / FR-003)

**Decision**: **startup = inherited, exactly; change = Explorer's rule.**
At startup: snapshot A (wide) → regenerate → snapshot B → `inheritedOnly =
names(A) \ names(B)` (case-insensitive) → **restore A**: for every entry of A
`SetEnvironmentVariableW(name, value)`, and for every name in B not in A
`SetEnvironmentVariableW(name, NULL)`; `=`-prefixed entries are neither
snapshotted nor touched. On a system change: regenerate → snapshot B' →
re-add every inherited-only variable with its **A value** (wide) — the
regenerated values win for everything else, as in Explorer.

**Rationale**: A is what the user's other windows have; anything else is a
coincidence of registry contents. The change notification is the moment the
system says "values moved" — following it is what Explorer does, so a shell
opened from Tandem Commander after `setx` shows the new value exactly like a
new Start-menu shell. Deletions on change stay ignored (today's rule; a
variable removed from the registry but still in A is inherited-only by then
only if the regeneration drops it — which it does — so it survives; that
matches "do not delete anything").

**Alternatives considered**:
- *Skip the startup regeneration entirely and compute the inherited-only set
  lazily at the first change notification* — simpler, but the set would then
  include anything the process or a plugin set in the meantime, a subtle
  semantic drift; also the cost saved is ~4 ms once. Rejected.
- *Keep today's startup behaviour* — leaves the class of difference in R2
  open; rejected for the guarantee's sake, but note it is what ships today
  and is not demonstrated defective.

## R4 — Module boundary and testability

**Decision**: new `src/common/salenv.{h,cpp}`: wide block → sorted entries
(`CSalEnvBlock`), case-insensitive name compare via
`CompareStringOrdinal(…, TRUE)`, `SalEnvInheritedOnly(A, B)`,
`SalEnvRestore(A, B, os)`, `SalEnvReapply(inheritedOnly, os)`; OS access
through `class CSalEnvOs { virtual GetBlock(); virtual Set(name, value|NULL);
virtual Regenerate(); }` with `CSalEnvOsReal` in the core and a fake in
`saltests`. `salamdr7.cpp` keeps `InitEnvironmentVariablesDifferences()` /
`RegenEnvironmentVariables()` as wrappers holding the module-level
`inheritedOnly` set (replacing `CEnvVariables EnvVariablesDiff`).

**Rationale**: `saltests` compiles `src/common/*.cpp` only
(`src/vcxproj/saltests/saltests.vcxproj`), so this is the only placement that
lets the diff/restore/re-apply rules be checked with fake blocks — the probe
pattern of feature 071 (`CSalShellProbe`). Names and values are UTF-16 end to
end; UTF-8 appears only where the core logs them (`SalWToU8`, WTF-8 total).

**Alternatives considered**: converting `CEnvVariables` to wide in place —
works, but leaves the logic untestable and the file at 300 lines of mixed
concerns; rejected.

## R5 — Parity check: what is automatable

**Decision**: three layers (see [contracts/parity-check.md](contracts/parity-check.md)):

1. **Unit** (`saltests`, fake OS): restore yields A exactly (values with
   U+010D and a lone surrogate, names differing only in case, a variable only
   in B removed, a variable only in A kept); re-apply after a change keeps
   B' values and adds inherited-only with A values; `=`-entries untouched.
2. **Process** (`saltests`, real OS): the test sets `SALTESTS_ENV_MARK` to a
   non-ACP value, spawns **itself** via `SalCreateProcess(NULL, "saltests.exe
   --dump-env <file>", …, lpEnvironment = NULL, …)`, the child writes
   `GetEnvironmentStringsW` as UTF-8 lines and exits 0, the parent compares
   the child's block with its own (`=`-entries excluded): zero differences.
   This exercises the exact primitive `OpenCommandShell` uses.
3. **Manual** (quickstart): presets, Windows Terminal forwarding, the `.bat`
   path, the change notification, the option off — using `evidence/capture.cmd`,
   `penv.ps1`, `treeenv.ps1`, `diag.ps1`.

**Rationale**: the desktop parts (pressing `Num /`, Windows Terminal's
hand-off, Explorer as the reference) cannot run in the test process; the
primitive and the semantics can.

**Alternatives considered**: a GUI harness posting `CM_DOSSHELL` to the
running application (as feature 071's verification did) — useful for the
manual matrix, too brittle for the suite; noted as optional in quickstart.

## R6 — Allow-list of expected differences

**Decision**: compare **the same shell started both ways**; the only tolerated
differences are then the bookkeeping entries (`=A:`…`=Z:`, `=::`, `=ExitCode`)
and the host's session identity (`WT_SESSION`, `WT_PROFILE_ID`). Everything
else is a finding.

**Rationale**: the shells themselves add variables (Command Prompt sets
`PROMPT`; Windows PowerShell prepends its user module folder to `PSModulePath`
and appends `.CPL` to `PATHEXT`; Git Bash sets `HOME`, `MSYSTEM`, `SHELL`,
`TERM`, `ORIGINAL_PATH`, `EXEPATH` …) — identical on both sides when the shell
is the same, so the comparison must never cross shells. Spec V2/V3 confirm
the list: `WT_SESSION`/`WT_PROFILE_ID` were the only differences.

## R7 — Documentation changes (FR-006)

**Decision**:
- `help/src/hh/salamand/configuration_cmdshell.htm` — in the *Program* list,
  Windows Terminal "opens the terminal's **default profile**, which can be any
  shell (Command Prompt, PowerShell, Git Bash, a WSL distribution…)"; new
  *Remarks* paragraph *Environment*: the program inherits Tandem Commander's
  environment, which is the environment of whatever started Tandem Commander
  (normally Explorer); how *Keep environment variables updated* behaves; a
  Tandem Commander started from the installer's *Launch* checkbox or from a
  terminal carries that parent's environment.
- `help/src/hh/salamand/othertask_shell.htm` — same *Environment* remark plus
  the comparison procedure: run `set` in the window Tandem Commander opened and
  in one opened from the Start menu, compare; differences beyond `WT_*` mean
  the two windows were started from different parents.
- Configuration page: when the Windows Terminal preset is selected, the *Found
  at* line reads `<path> — opens its default profile` (new string
  `IDS_CMDSHELL_WT_DEFAULTPROFILE`, 8 languages through the `.slt` refresh;
  pin in `translations/ui-overrides.json` only if the machine translation of
  "default profile" misses the Windows Terminal term the language uses).

**Rationale**: FR-006 asks for the page and the manual; the 071 page hint
(`IDT_CMDSHELL_HINT`, three lines) has no room, the *Found at* line is the
natural place and is already preset-specific. The 2023 footer of
`othertask_shell.htm` stays (the product-wide footer update is a separate
follow-up noted in feature 071).

## R8 — Encoding guard

**Decision**: the new module uses only wide APIs; the ANSI
`GetEnvironmentStrings`/`SetEnvironmentVariable` uses in `salamdr7.cpp`
disappear with the class. `tools/check_encoding.py --strict` stays at
`TOTAL: 0`; the draft-rule hit count for ANSI environment reads (feature 068
review) drops by the sites removed. No new guard rule is needed — the wide
calls are the house pattern.

## R9 — The reproduction gate (FR-005)

**Decision**: `tasks.md` starts with a gate task — "the capture exists
(`evidence/tc-tree-*.txt`, `tc-live-*.txt`, `ref-explorer-*.txt` from a live
failure) and its finding is recorded in `fix-log.md`"; every implementation
task depends on it. If the capture shows identical environments, the cause is
outside this feature's scope (working directory, the program actually
started, the shell) and the plan is revised before any code changes.

**Rationale**: the owner's decision of 2026-08-31 (park, no product change
without a demonstrated defect); constitution III (do not modify working code
without cause).

## R10 — Windows Terminal forwarding

**Decision**: document as observed on 1.24.11911.0 ("Windows Terminal passes
the environment of the program that started it into the new tab"); no
product logic depends on it; the manual matrix records the version.

**Rationale**: it is Windows Terminal's behaviour, verified (spec V3) but not
a contract; older versions are out of scope (spec Assumptions).

## R11 — If `RegenerateUserEnvironment` disappears

**Decision**: `CSalEnvOsReal::Regenerate()` returns FALSE when the export is
missing (today: `TRACE_E` and return); the startup path then records an empty
inherited-only set and skips restore (block untouched = inherited); the
change path does nothing. FR-002 holds unconditionally; FR-003 degrades to
"needs restart" — exactly today's behaviour without the export.

## R12 — Startup ordering and plugin-set variables

**Decision**: keep the call at `src/salamdr1.cpp:4470` (after the main window
is shown, after `salmon.exe` was started and `PATH` restored). Anything set
into the process before that point — `=X:` entries by `SetDefaultDirectories`
(excluded anyway), variables set by plugins loaded at startup — counts as
inherited and is preserved. Documented in the module header.

## R13 — `ReloadEnvVariables = FALSE`

**Decision**: untouched path: no snapshot, no regeneration, block = inherited;
FR-002 holds trivially, FR-003 not applicable (documented in the manual as
"changes are seen after a restart").

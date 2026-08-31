# Data Model: Command Shell Environment Parity

**Feature**: 073-fix-cmdshell-env · **Date**: 2026-08-31 · Shelf design (see
[plan.md](plan.md))

No persisted data changes. The entities are in-memory structures of the new
`src/common/salenv` module and the files the evidence scripts produce.

## Entities

### EnvironmentBlock

The environment of one process as Windows holds it: a sequence of
`NAME=VALUE` UTF-16 strings, double-NUL terminated.

| Field | Type | Rules |
|-------|------|-------|
| entries | ordered list of `EnvEntry` | sorted by `name`, case-insensitive ordinal (`CompareStringOrdinal(…, TRUE)`); the OS order is not preserved (Windows appends new variables at the end, so order carries no meaning) |
| `EnvEntry.name` | UTF-16 string, non-empty | unique within a block up to case; a name beginning with `=` is **bookkeeping** (`=C:` per-drive current directory, `=::`, `=ExitCode`) and is never part of a block's entries |
| `EnvEntry.value` | UTF-16 string, may be empty | compared **exactly** (case-sensitive, code unit by code unit); may contain any UTF-16 unit including lone surrogates (converted to WTF-8 only for logging) |

Source: `GetEnvironmentStringsW()` of the current process
(`CSalEnvOs::GetBlock`), or a fake in tests.

### InheritedSnapshot

The `EnvironmentBlock` captured **before** the first regeneration at startup
(= what the parent process passed down, plus anything set before
`InitEnvironmentVariablesDifferences` ran — R12). Held for the life of the
process; the values of its inherited-only names are what re-application
restores.

### InheritedOnlySet

`names(InheritedSnapshot) \ names(RegeneratedBlock)`, case-insensitive: the
variables the system's regeneration does not produce (typical: variables a
launcher set, Explorer-internal `EFC_*`, tool-injected variables). Each member
carries the snapshot value. Empty when regeneration is unavailable (R11) or
the option is off.

### ParityComparison

| Field | Type | Rules |
|-------|------|-------|
| reference | `EnvironmentBlock` | the plainly started side (Explorer, or the parent) |
| subject | `EnvironmentBlock` | the side started through Tandem Commander |
| allowList | set of names + prefixes | `=*` (bookkeeping), `WT_SESSION`, `WT_PROFILE_ID` (R6) |
| differences | list of `Difference` | empty ⇔ parity holds |
| `Difference.kind` | `missing` / `added` / `changed` | relative to the reference |
| `Difference.name` | name | |
| `Difference.values` | reference value, subject value | for `changed`; for `PATH` additionally the entry-level lost/gained lists |

Validation: `subject` and `reference` MUST be the same program started the
same way apart from the launcher (R6) — comparing Git Bash with Command
Prompt is invalid.

### CaptureSet (files produced by `evidence/capture.cmd`)

One capture = three text files next to the script, sharing a stamp
`yyyyMMdd-HHmmss`:

| File | Producer | Content |
|------|----------|---------|
| `tc-live-<stamp>.txt` | `penv.ps1` | `explorer.exe` and `tandemcommander.exe` blocks read from the running processes: variable count, key variables, and a diff section (`only in explorer:` / `only in TC:` / `<name>: [explorer] … \| [TC] …` / `PATH differs:` / `IDENTICAL`) |
| `tc-tree-<stamp>.txt` | `treeenv.ps1` | every process under `tandemcommander.exe`: `- <name> PID <n> (parent <p> <name>) started <time>`, `cmdline:`, `cwd:`, `env: <count> variables; USERPROFILE=…; HOME=…`, then diff lines against Explorer (`missing:` / `added:` / `changed:` / `PATH differs:` / `(identical to Explorer)`) |
| `ref-explorer-<stamp>.txt` | `capture.cmd` itself (an Explorer-started Command Prompt) | sections `[whoami]`, `[cd]`, `[where node]`, `[where npm]`, `[set]` |

Secrets: values of names matching `KEY|TOKEN|SECRET|PASS` are masked in the
first two; the third is a raw `set` (reviewed before committing). Files are
committed under `evidence/` as the FR-005 record; `env-*.txt` from `diag.ps1`
are gitignored (full dumps of a developer session).

## State transitions of the process environment

```
[start]  block = inherited from parent (+ early sets, R12)
   │  ReloadEnvVariables == FALSE ──────────────────────────────► (no change ever; R13)
   ▼  ReloadEnvVariables == TRUE
 snapshot A ─► Regenerate() ─► snapshot B ─► inheritedOnly = A \ B ─► Restore(A, B)
   │                                                                  block == A   (FR-002)
   ▼
 running: SetDefaultDirectories() rewrites =A:…=Z: before each launch (bookkeeping, excluded)
          salmon launch appends+restores PATH (child only)
   │
   ▼  WM_SETTINGCHANGE "Environment"
 Regenerate() ─► snapshot B' ─► Reapply(inheritedOnly)   block == B' + inheritedOnly(A values)   (FR-003)
```

## Relationships

- `InheritedOnlySet` is derived from `InheritedSnapshot` and one
  `RegeneratedBlock`; it is the only state the change path needs.
- `ParityComparison` never touches the product; it is the test and
  verification view over two `EnvironmentBlock`s.
- `CaptureSet` is the evidence format the reproduction gate (FR-005) consumes.

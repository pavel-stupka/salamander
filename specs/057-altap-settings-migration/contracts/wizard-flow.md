# Contract: Console Wizard Flow

**Feature**: 057-altap-settings-migration
**Deliverable**: `utils/migrate-altap-settings.cmd`
**Covers**: FR-003, FR-008, FR-011, FR-012 (and the interaction decisions in
spec Clarifications, Session 2026-08-17)

The utility is an interactive console wizard. This contract fixes every
screen, its valid inputs, all refusal conditions, the summary format, and the
process exit codes — it is the testable UI surface driven by the test harness
via redirected stdin.

## Global rules

- All output is English, plain ASCII, no ANSI color requirements (colors MAY
  be used when the host supports them; tests match on text, not color).
- Every prompt reads one line from standard input and MUST work identically
  with redirected stdin (test harness) and the interactive console. Prompting
  MUST NOT use console-handle-only APIs that fail under redirection.
- Empty input at a prompt selects the shown default (rendered in brackets,
  e.g. `[Y]`). Invalid input reprints the prompt; after 3 consecutive invalid
  inputs the wizard aborts with exit code 2 (prevents infinite loops when a
  test feeds wrong answers).
- `Ctrl+C` or EOF on stdin at any point before screen W6's confirmation:
  nothing has been written; exit code 3.
- The wizard never writes to any registry location before W6 is confirmed,
  and never writes anywhere except: the Tandem Commander root, the backup
  `.reg` file, the generated `restore.cmd`, and the summary `.txt`.

## Screens

### W1 — Identification & safety notice (FR-012)

Prints: utility name and purpose, feature/version stamp, the statement that
the source (Altap Salamander) is opened read-only and never modified, and
that a backup of Tandem Commander settings will be taken before any change.
No input; proceeds to W2.

### W2 — Environment checks (FR-008, edge cases)

Checks, in order; each failure prints the stated message and exits:

| Check | Failure message (essence) | Exit code |
|-------|---------------------------|-----------|
| Running `tandemcommander` process | "Close Tandem Commander first — it saves its settings on exit and would overwrite the transferred data." | 10 |
| Running Altap/Servant Salamander process (`salamand`) | "Close Altap Salamander first so its configuration is stable while it is read." | 10 |
| At least one source root found (per contracts/category-mapping.md scan list) | "No Altap Salamander configuration was found for this Windows user. Nothing to migrate." | 11 |
| Destination hive writable (open/create TC root with write access) | "Cannot write Tandem Commander settings (registry access denied)." | 12 |

Re-check of the two process conditions is repeated immediately before the
first write (post-W6): a process started while the user was in the wizard
also triggers the exit-10 refusal, before anything is written.

### W3 — Source selection (FR-002, User Story 3)

Lists every discovered source configuration, newest first, numbered:

```
Found Altap Salamander configurations:
  [1] Altap Salamander 4.0        (47 subkeys, last written 2024-11-02)
  [2] Altap Salamander 3.08       (39 subkeys, last written 2019-05-17)
Select source [1]:
```

- Single source found: the list is shown, prompt default is that source.
- Input: index number; default = 1 (newest).
- "last written" timestamp comes from the root key's last-write time; if
  unavailable, omitted.

### W4 — Category checklist (FR-003, FR-004)

Shows every category from contracts/category-mapping.md that is **offered**
for the selected source (present in the source AND compatibility verdict ≠
skip), with item counts:

```
Transferable settings found (all selected by default):
  [X] 1. Directory hot paths            (7 items)
  [X] 2. FTP connection bookmarks       (12 servers)
  [X] 3. User menu commands             (5 items)
  [ ] 4. ...
Toggle number, A=all, N=none, D=done [D]:
```

- Categories NOT offered are listed afterwards under "Not transferable from
  this source:" with a one-line reason each (feeds FR-011; empty categories
  show as "(empty — nothing to transfer)" and cannot be selected, per US1
  acceptance scenario 4).
- Input loop: a category number toggles it; `A` selects all; `N` clears all;
  `D` (or empty = default) finishes. Finishing with zero categories selected
  prints "Nothing selected." and exits with code 3 (no writes happened).

### W5 — Backup announcement (FR-007, User Story 2)

Prints the full paths the run will create BEFORE confirmation:

```
Before writing, a complete backup of current Tandem Commander settings
will be saved:
  Backup : <dir>\tc-settings-backup-YYYYMMDD-HHMMSS.reg
  Restore: <dir>\tc-settings-restore-YYYYMMDD-HHMMSS.cmd   (double-click to undo)
  Summary: <dir>\tc-migration-summary-YYYYMMDD-HHMMSS.txt
```

`<dir>` = directory containing the script if writable, else
`%USERPROFILE%\Documents`. No input; proceeds to W6.

### W6 — Explicit confirmation (FR-012)

```
About to transfer 3 categories from "Altap Salamander 4.0" into
"Tandem Commander 0.1". Selected categories will be REPLACED in
Tandem Commander. Continue? [y/N]:
```

- Default is **No**. Only `y`/`yes` (case-insensitive) proceeds; anything
  else exits with code 3 and no writes.

### W7 — Transfer progress

One line per category as it is processed: `Transferring: <category> ... done
(<n> items)` or `... partial (<n> of <m> items, see summary)` or a failure
line. Order: backup first (its success is mandatory — a failed backup aborts
the run with exit code 12 before any destination write), then categories.

### W8 — Summary (FR-011, US1 acceptance scenario 3)

Printed to the console AND written verbatim to the summary `.txt`:

- One block per **selected** category: `TRANSFERRED` / `PARTIAL` / `SKIPPED`
  (+ counts). PARTIAL and SKIPPED blocks list each skipped item with its
  reason (one line per item).
- One block listing categories that were not offered, with reasons (from W4).
- Flagged items (edge case: entries pointing into the Altap Salamander
  installation directory) appear under `NOTES`, e.g. password-manager
  guidance ("passwords protected by a master password: use the same master
  password in Tandem Commander") and paths that will break if Altap
  Salamander is uninstalled.
- Final lines: backup path + restore instruction ("To undo this migration,
  run: <restore.cmd path>").

Ends with `Press Enter to close.` (skipped when stdin is redirected — i.e.
when EOF/next line is immediately available, the wizard must not block; the
implementation may detect redirection and skip the final pause).

## Exit codes (test-harness contract)

| Code | Meaning |
|------|---------|
| 0 | Transfer completed (all selected categories TRANSFERRED or PARTIAL; summary written) |
| 2 | Aborted: repeated invalid input |
| 3 | User cancelled (declined confirmation, selected nothing, EOF/Ctrl+C before writes) — no registry writes performed |
| 10 | Refused: Tandem Commander or Altap Salamander is running |
| 11 | Refused: no source configuration found |
| 12 | Failed: destination not writable or backup could not be created — no destination write performed |
| 13 | Failed mid-transfer after backup (unexpected error); summary + restore instructions still emitted |

## Test-only environment overrides

Documented in the script header as FOR TESTING ONLY (not user-facing
options; the wizard behaves identically otherwise):

| Variable | Effect |
|----------|--------|
| `TCMIG_SOURCE_ROOT` | Replaces the built-in source scan list with one or more registry paths, semicolon-separated, newest first (e.g. `HKCU\Software\TCMigTest\Altap Salamander 4.0;HKCU\Software\TCMigTest\Altap Salamander 2.51`) |
| `TCMIG_DEST_ROOT` | Redirects the destination root (default `HKCU\Software\Tandem Commander\0.1`) |
| `TCMIG_OUT_DIR` | Redirects where backup/restore/summary files are written |
| `TCMIG_SKIP_PROCCHECK` | `1` = skip the running-process refusal (harness runs while a dev instance may be open) |

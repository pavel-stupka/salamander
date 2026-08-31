# Contract: Parity check (FR-004) and capture formats (FR-005)

**Feature**: 073-fix-cmdshell-env · **Date**: 2026-08-31 · Shelf design

## 1. Comparison rule

Two environment blocks are **at parity** when, after removing every name on
the allow-list, they contain the same names (case-insensitive) with identical
values (exact). The two sides MUST be **the same program started the same way
except for the launcher** (Command Prompt vs Command Prompt, Git Bash vs Git
Bash) — the shells add variables of their own, which cancel out only like for
like.

**Allow-list** (R6):

| Name / pattern | Why tolerated |
|----------------|---------------|
| `=*` (`=C:`, `=::`, `=ExitCode`, …) | per-drive current-directory bookkeeping; Tandem Commander sets `=A:`…`=Z:` on purpose before each launch |
| `WT_SESSION`, `WT_PROFILE_ID` | Windows Terminal's identity of the tab, different for every tab |

Anything else — missing, added, or changed (for `PATH`: any entry lost or
gained, order included) — is a finding and is reported by name.

## 2. Automated checks (`saltests`)

### 2.1 Unit — `TestEnvParity073()` with `CSalEnvOsFake`

Fixture blocks are built from literal UTF-16 strings. Required cases:

| Case | A (inherited) | B (regenerated) | Expect |
|------|---------------|-----------------|--------|
| U1 restore basic | `FOO=1`, `PATH=x;y`, `USERPROFILE=C:\U\p` | `PATH=y;x`, `USERPROFILE=C:\U\p`, `NEWSYS=1` | after restore: block == A; `NEWSYS` absent; inheritedOnly = {`FOO`} |
| U2 exact values | `TC_TEST=M\u010Fj adres\u00E1\u0159`, `TC_LONE=a\uD800b` | (same names, other values) | restored values identical code unit by code unit |
| U3 name case | `Path=…` | `PATH=…` | treated as one name; A's spelling and value restored; one entry only |
| U4 bookkeeping | fake OS block contains `=C:=C:\x`, `=::=::\` | — | never loaded into a block; `Set` never called with a `=` name |
| U5 change path | inheritedOnly = {`FOO=1`}; B' = `PATH=z`, `USERPROFILE=…` | — | after reapply: `PATH=z` (B' wins), `FOO=1` present |
| U6 no regeneration | fake `Regenerate()` returns FALSE | — | `SalEnvInitAndRestore` returns FALSE, inheritedOnly empty, no `Set` calls |

### 2.2 Process — self-spawn through the launch primitive

- `saltests.exe --dump-env <utf8-file>`: the process writes every entry of
  `GetEnvironmentStringsW()` (bookkeeping excluded) as one `NAME=VALUE` line
  in WTF-8 (`SalWToU8`), sorted by name, and exits 0. No other test runs in
  this mode.
- The test sets `SALTESTS_ENV_MARK` to `M\u010Fj \uD800 test` (non-ACP +
  lone surrogate) via `SetEnvironmentVariableW`, starts its own executable
  with `SalCreateProcess(NULL, cmdLine, NULL, NULL, FALSE,
  CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL /*env*/, tempDir,
  &si, &pi)` — the same argument shape as `OpenCommandShell` — waits, reads
  the file, and compares with its own block: **zero** differences, the marker
  present with the exact value.
- Skipped (with a printed reason) only when no temp path is available, as
  `TestWtf8FileOps` does.

Exit code of `saltests.exe` remains the number of failed checks; the new
checks add to the `saltests: N checks, M failed` line.

## 3. Manual procedure (quickstart matrix)

Reference side: a window opened from the **Start menu** (or the file
double-clicked in **Explorer**). Subject side: the same program opened from
Tandem Commander. In both:

```bat
set > "%TEMP%\env-<side>.txt"
```

(Git Bash: `env | sort > /tmp/env-<side>.txt`; PowerShell: `Get-ChildItem env: |
Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" } > env-<side>.txt`.)
Compare with `evidence/diag.ps1`'s diff logic or `fc`, applying the
allow-list. The result is recorded in `fix-log.md` per matrix row.

`evidence/penv.ps1` (product vs Explorer), `evidence/treeenv.ps1` (every
process under the product) and `evidence/capture.cmd` (all three files of a
capture set) are the one-click forms of the same comparison.

## 4. Capture set format (FR-005 evidence)

Produced by `evidence/capture.cmd`, double-clicked in Explorer while the
failure is live; stamp `yyyyMMdd-HHmmss`.

### `tc-live-<stamp>.txt`

```
===== explorer.exe PID <n> started <date time> =====
var count: <k>
  USERPROFILE=…            (19 key variables, "<unset>" when absent)
===== tandemcommander.exe PID <n> started <date time> =====
var count: <k>
  …
===== diff explorer.exe vs tandemcommander.exe =====
  only in explorer: <name>=<value>
  only in TC: <name>=<value>
  <name>: [explorer] <value> | [TC] <value>
  PATH differs: <a> vs <b> entries
    only in explorer: <entry>
    only in TC: <entry>
  IDENTICAL                     (when nothing above)
===== registry env (what a regeneration would produce) =====
  HKCU <name>=<value>
```

### `tc-tree-<stamp>.txt`

```
reference: explorer.exe PID <n> started <date time>, <k> variables
  USERPROFILE=…  USERNAME=…
- tandemcommander.exe PID <n> (parent <p> explorer.exe) started <date time>
    cmdline: <command line, 220 chars max>
    cwd:     <current directory of that process>
    env:     <k> variables; USERPROFILE=…; HOME=…
      (identical to Explorer)  |  missing: <name>  |  added: <name>=<value>  |  changed: <name>: [explorer] … | [this] …  |  PATH differs: … lost:/gained: …
  - <child>.exe PID … (parent … tandemcommander.exe) started …
      …                         (recursively, indented two spaces per level)
```

### `ref-explorer-<stamp>.txt`

```
[whoami]
<DOMAIN\user>
[cd]
<directory of the script>
[where node]
<path(s) or INFO: Could not find files …>
[where npm]
<path(s)>
[set]
<raw output of set>
```

Masking: names matching `KEY|TOKEN|SECRET|PASS` show `<hidden, n chars>` in
the first two files; the third is raw and is reviewed before committing.

### Reading the capture

| Observation | Meaning | Next step |
|-------------|---------|-----------|
| `tc-live`: any `only in` / changed line | Tandem Commander's block differs from Explorer's — the guarantee (G2/G3) is the fix; the named variable is the cause | gate PASS → implement work packages 1–3 |
| `tc-live`: `IDENTICAL`; `tc-tree`: the failing `node.exe`/`cmd.exe` shows `changed:`/`missing:` | something between Tandem Commander and the program altered the environment (a `.bat`, a shell profile) — not the launcher | gate stays NOT MET for this feature; report to the project |
| `tc-live`: `IDENTICAL`; `tc-tree`: `(identical to Explorer)` everywhere | not an environment cause; look at `cwd:` and `cmdline:` of the failing process against a working run | revise the plan before any code change |

# Quickstart: validating Command Shell Environment Parity

**Feature**: 073-fix-cmdshell-env · **Date**: 2026-08-31 · Shelf design — §0
gates everything else.

## 0. Gate — is there evidence? (FR-005)

Before any build or code change:

1. `specs/073-fix-cmdshell-env/evidence/` contains a capture set taken **while
   the failure was live**: `tc-live-<stamp>.txt`, `tc-tree-<stamp>.txt`,
   `ref-explorer-<stamp>.txt` (produced by double-clicking `capture.cmd` in
   Explorer — see [contracts/parity-check.md §4](contracts/parity-check.md)).
2. `fix-log.md` records what the capture shows, using the *Reading the
   capture* table of that contract.
3. Only a capture that shows Tandem Commander's block differing from
   Explorer's (or a variable lost between Tandem Commander and the started
   program by the product) opens §1–§4. Any other reading sends the plan back
   for revision — no product change.

Expected today: the folder holds `tc-live-startup-2026-08-31.txt` (V6,
`IDENTICAL`) and no live-failure capture → gate NOT MET.

## 1. Build and automated checks

Prerequisites: VS2022 with the C++ workload, Python 3.13 on `PATH`
(`build.cmd` fails without it), `OPENSAL_BUILD_DIR` set (optional).

```bat
build.cmd                     :: Debug x64 incremental; runs tools\check_encoding.py --strict (expect TOTAL: 0)
%OPENSAL_BUILD_DIR%\Debug\x64\saltests.exe
```

Expected: `saltests: <N> checks, 0 failed` with `N` ≥ 1301 + the new checks
of `TestEnvParity073()` (unit cases U1–U6 and the self-spawn process check);
no `skipping` line unless the machine has no temp path.

```bat
python -m translate.slt --verify     :: after the .slt refresh for IDS_CMDSHELL_WT_DEFAULTPROFILE
build.cmd full                       :: language modules; lang_policy.ps1 reconciles the output tree
```

## 2. Manual matrix — parity per launch path (FR-001, FR-004)

Reference: a window from the **Start menu** (or the file double-clicked in
**Explorer**). Subject: the same program from Tandem Commander, started
normally from Explorer/Start. In each pair run the `set`/`env` dump of
[contracts/parity-check.md §3](contracts/parity-check.md) and compare with the
allow-list (`=*`, `WT_SESSION`, `WT_PROFILE_ID`).

| Id | Subject | Reference | Expected |
|----|---------|-----------|----------|
| M1 | Command Prompt preset, `Num /` | Start → `cmd` | 0 differences |
| M2 | Windows Terminal preset, `Num /` (tab = WT default profile) | Windows Terminal from Start, same profile | 0 differences beyond `WT_*`; a variable set only in Tandem Commander's process (e.g. `TC073_MARKER` via a Custom-preset test, or `evidence/diag.ps1`'s marker) is present in the tab; record the WT version |
| M3 | Windows PowerShell preset | Start → Windows PowerShell | 0 differences |
| M4 | Git Bash preset | Git Bash from Start | 0 differences |
| M5 | Custom program: `$[SystemRoot]\System32\cmd.exe` `/k set > "$(FullPath)\env-tc.txt"` | Start → `cmd` | 0 differences; file lands in the panel folder |
| M6 | `Enter` on `evidence\dumpenv.cmd` copied into a test folder (writes `env-<arg>.txt` next to itself) | the same file double-clicked in Explorer | 0 differences |
| M7 | Non-ACP value: `setx TC_TEST "Můj adresář"` in a Start-menu cmd **before** starting Tandem Commander; then M1 | — | `set TC_TEST` in the opened shell shows the exact value (also verifies the value survived the startup regeneration) |
| M8 | Option *Keep environment variables updated* **off** (Configuration → General), restart, M1 | Start → `cmd` | 0 differences |
| M9 | Working directory with spaces + diacritics (`G:\Můj disk\Nový projekt`), M1 | — | `cd` shows that folder (feature 071 promise re-checked) |

## 3. Startup parity and change propagation (FR-002, FR-003)

| Id | Steps | Expected |
|----|-------|----------|
| S1 | Start Tandem Commander from Explorer; run `evidence\penv.ps1` (Windows PowerShell 5.1, `-ExecutionPolicy Bypass`) | diff section: `only in explorer: =::=::\` at most — i.e. `IDENTICAL` for variables |
| S2 | Start it from a Command Prompt that has `set TC073_ONLY=parent` (`start "" "C:\Program Files\Tandem Commander\tandemcommander.exe"`); `penv.ps1` against that cmd's PID (adapt the script's process filter) | `TC073_ONLY` present in Tandem Commander with the exact value; nothing else differs |
| S3 | While running: `setx TC_CHANGE "after"` from a Start-menu cmd (broadcasts the change); then `Num /` → `set TC_CHANGE`; also `set TC073_ONLY` from S2 | `after` shown; `TC073_ONLY` still `parent` (inherited-only survives) |
| S4 | While running: change the user `Path` in System Properties (add a folder); `Num /` → `path` | the new entry present, as in a new Start-menu cmd |
| S5 | Option off (M8 state), repeat S3 | `TC_CHANGE` absent until restart (documented behaviour) |

## 4. Documentation (FR-006, FR-009)

| Id | Check | Expected |
|----|-------|----------|
| D1 | Configuration → Command Shell, select *Windows Terminal* | *Found at* reads `<path> — opens its default profile` in English and in each of the 8 enabled languages (`czech`, `german`, `french`, `dutch`, `hungarian`, `romanian`, `slovak`, `spanish` — `translations/languages.cfg`) |
| D2 | Manual: *Command Shell configuration* and *Opening Command Shell* topics | both state what environment the program inherits, what changes it (how Tandem Commander was started; the *Keep environment variables updated* option), and the comparison procedure; Windows Terminal's default-profile behaviour named |
| D3 | `CHANGELOG.md` | entry under the shipping version, user terms, says the original report was not reproduced and what the change guarantees; version/build bump in the same change (`spl_vers.h`, `tandemcommander.iss`, `CLAUDE.md`) |

## 5. Evidence scripts (reference)

| Script | Purpose | Side effects |
|--------|---------|--------------|
| `evidence/capture.cmd` | one-click capture set (§0) — double-click in Explorer | writes 3 files next to itself |
| `evidence/penv.ps1` | product vs Explorer blocks, live | none |
| `evidence/treeenv.ps1` | every process under the product: cmdline, cwd, env diff | none |
| `evidence/diag.ps1` | direct vs `wt.exe -d .` launch with a marker (V2/V3) | opens one Windows Terminal tab briefly; writes `env-*.txt` (gitignored) |
| `evidence/regen.ps1` | regeneration inside Explorer's exact environment (V4; note the PowerShell artifact) | none |

All scripts are read-only with respect to the product and the registry.

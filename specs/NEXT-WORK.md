# Next Work — consolidated continuation

**Written**: 2026-09-02 · **Baseline**: `main` at `f4cefa1` (0.1.7, build 191)

This file is the single entry point for "what do we do next". It consolidates
the per-feature handoffs — `specs/072-winget-distribution/REMAINING-WORK.md`,
`specs/069-finish-encoding-fixes/REMAINING-WORK.md` and
`specs/070-source-viewer-plugin/REMAINING-WORK.md` — into one order. Those files
stay authoritative for the *detail and the reasoning*; this one decides the
**sequence** and records what was verified against HEAD when it was written.

Ordering criterion: what it costs users × what it costs us. Nothing here is a
blocker for anything already shipped.

---

## 1. Small hardening batch (hours, one feature)

Five defects recorded in `069/REMAINING-WORK.md` §3. Feature 069 could not fix
them because its charter (FR-001) forbids a change without a finding behind it —
they were found while doing other work. **The first was re-checked at `f4cefa1`
and is still present.**

| Site | Defect |
|---|---|
| `src/codetbl.cpp:873` | `if (len > bufferLen) len = bufferLen - 1;` must be `>=`. A conversion name of exactly `bufferLen` bytes writes `buffer[bufferLen]` — an **out-of-bounds write**. Callers pass `codeName[200]` (`viewer3.cpp:58`) and `DefaultConvert[200]`; unreachable with the shipped names (longest 33 B), but it is a real overflow. **Verified present.** |
| `src/viewer3.cpp:3291` | `GetCodeType`'s return value ignored → `defCodeType` used uninitialised when the tables are not loaded |
| `src/zip.cpp:3292` | `GetConversionTable` result not NULL-checked (pre-existing, no new exposure) |
| `src/plugins/filecomp/controls.cpp:24,39` | unbounded `strcpy(Text, text)`, safe today only because both sides are `[MAX_PATH]` |
| `src/viewer3.cpp:30,35` | `lstrcpyn(caption, FileName, MAX_PATH)` can cut a path over 259 bytes mid-character, dropping the whole caption to the legacy draw — the F-P4-02 fixes do not help very long non-ASCII paths |

Fold in one unrelated one-liner: `src/plugins/codeview/test/run_tests.cmd`
reports `RESULT: FAILURES` on this machine before *and* after feature 074
(Node v20.18.0 treats `web/worker.js` as CommonJS; passes with
`--experimental-detect-module`, default from Node 22.12 — see
`074/fix-log.md`). While that line is red it masks real regressions.

**Why first**: best risk-to-cost ratio in the whole list, and it makes the test
output trustworthy for everything below.

## 2. Restart Manager — upgrading over a running instance (winget P1)

The one item that **fails for real users** now that the package is in the
catalogue. With the program open, `winget upgrade` (which passes
`/SUPPRESSMSGBOXES`) hits the Abort/Retry/Ignore prompt, answers **Abort**, and
the install rolls back with exit 5. Not a regression — it never mattered while
upgrading meant running the installer by hand. Full evidence in
`072/REMAINING-WORK.md` P1.

Scope note taken at HEAD: the plumbing already exists — `src/mainwnd3.cpp:6220`
onwards has an elaborate `WM_QUERYENDSESSION` / `WM_ENDSESSION` handler
including critical-shutdown handling and configuration backup. The work is
therefore *behave correctly on `ENDSESSION_CLOSEAPP` and actually close*, the
same for `salmon` (also listed as holding files), and a decision on
`RegisterApplicationRestart` — **not** writing Restart Manager support from
scratch.

First step is reproduction with 072 `quickstart.md` §2b and confirming exit 5.
The design question is whether the panels' state survives the restart; the API
is the easy half. Scope is `src/`, not `setup/`. Worth a feature of its own.

## 3. The owed on-screen sweeps (a GUI session, maintainer only)

Three features are complete on paper and unverified on screen:

- **069 §4** — the 068 sweep W1–W20 in the Czech UI and then the Hungarian UI
  (proving 069 did not disturb what earlier features repaired), then V-01…V-24
  from its `quickstart.md`. The side-by-side reference build
  `build\tandemcommander\Release_x64_prefix069\` (347 files) is preserved for
  exactly this and is ageing; **do not delete it before the sweep**. Start with
  V-01 (command line), V-09 (help and `config.reg` under an accented install
  path), V-11 (cloud entries).
- **070 §3** — the codeview quickstart scenarios plus the runtime halves of the
  corpus checks (hostile content, request log, key sweep, copy fidelity,
  encoding matrix, performance budgets). The corpora are already written.
- **074** — the human steps listed at the end of its `fix-log.md`.

Best done **after** items 1 and 2, so the sweep runs once against a final state.
A sweep failure is a finding: back through fix → independent review → gates.

## 4. Architectural debt to repay before it is copied

- **mdview onto the shared `src/common/webhost/`** (`070/REMAINING-WORK.md` §2).
  `src/common/webhost/` exists and codeview uses it; `src/plugins/mdview/webview.cpp`
  is still its own copy — verified at HEAD. The product ships two copies of the
  WebView2 host, the exact duplication
  `architecture/11-webview2-integration.md` exists to prevent. Its acceptance
  (the 021 lockdown re-verification and the 065 keeper scenarios) is a manual
  GUI pass, so it pairs naturally with item 3.
- **`GetNextFileNameForViewer`'s buffer contract.** The header documents *"at
  least MAX_PATH"* (`src/plugins/shared/spl_gen.h:2703`); the core fills it with
  `SAL_MAX_PATH_UTF8` (`src/salamdr6.cpp:205,223`). A plugin that believes the
  header takes a buffer overflow on a long path, and the constant lives in a
  core-only header, so a plugin cannot even name the right size. Correct the
  comment and export the constant — before another plugin copies the documented,
  wrong size.

## 5. Encoding: cluster B-2 next

Of the five systemic clusters in `069/REMAINING-WORK.md` §1, **B-2 is the only
one with a ready work list**: the guard rule `acp-byte-table-on-name`, 33
report-only hits — code-page byte tables behind all name comparison, so
`Č.txt` != `č.txt`. B-1 (ANSI dialog windows) is the natural second: its surface
is fully enumerated for the command line in 069 `research.md` R2 (word-break
callback ABI, the `WM_CHAR` unit, five selection-offset sites, and
`editwnd.cpp:577`). B-4 (`AlterFileName`, which also drives Change Case and so
renames on disk) is the highest-risk fix in the review — last.

Not part of this: **F-P1-05, the archive listing display encoding**
(`pack1.cpp`). Three attempts produced three defects, including a fatal listing
abort and a split directory tree; the listing must move as a whole. See 069 §0b.

## 6. Cheap winget housekeeping, once PR #426090 has settled

`072/REMAINING-WORK.md` gates everything on whether the submission is merged;
check that first, and change nothing under `tools/winget/templates/` while it is
open.

- **P4** — `actions/checkout@v4` / `actions/upload-artifact@v4` run on the
  deprecated Node 20. Bump all four workflows together so the repository does
  not end up with two conventions.
- **P2** — `--scope user` has **never actually been tested**; the entry was
  blamed for the first validation failure and the machine-only manifest then
  failed identically, which refuted that. The procedure needs no new release,
  but it needs Windows Sandbox and a branch of its own.
- **P3** — `checkver` still points at Open Salamander's site. Point it at the
  GitHub Releases API or drop it and declare winget the update channel: a
  product decision, not code.

---

## Recorded, deliberately not on the list

- **Help footers.** 236 of 237 manual pages still carry the 2023 Open Salamander
  footer; only `configuration_cmdshell.htm` (feature 071) was authored after the
  rebrand. Cosmetic, and a single mechanical pass whenever it is wanted.
- **The nine sites 069 chose not to convert**, each with a written reason
  (`069/REMAINING-WORK.md` §2) — `icncache.cpp:796`, the DROPFAKE/CLIPFAKE pair
  that must move together with the ANSI shell extension, the `shellib.cpp`
  `STRRET` sites, `execute.cpp:1213`, the nine `IDS_VIEWERTITLE` call sites, and
  three pieces of dead code. These are decisions, not oversights; re-opening one
  needs a reason the file does not already answer.
- **073** (Command Shell environment) is parked as not reproduced.

## Protocol

`specs/069-finish-encoding-fixes/contracts/fix-protocol.md` is binding for any
fix in items 1, 4 and 5, and it earned its keep: of four review batches, two
were rejected, both for regressions the fixes themselves introduced. Its two
highest-value rules: **check the site is still defective at HEAD first** (three
of 069's 34 items were already fixed, and five site references were stale), and
**enumerate the consumers yourself before writing anything**.

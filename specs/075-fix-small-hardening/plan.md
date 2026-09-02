# Implementation Plan: Small hardening batch — six recorded defects without a finding

**Branch**: `075-fix-small-hardening` | **Date**: 2026-09-02 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/075-fix-small-hardening/spec.md`

## Summary

Six defects that are already written down (`069/REMAINING-WORK.md` §3, `074/fix-log.md`)
but were never fixed, because the feature that found them was not allowed to
touch anything without a review finding. All six were re-confirmed at
`640b94a` (spec, "Where each defect stands at HEAD"); one — the Code Viewer's
test runner — has changed shape since it was recorded (the machine now runs
Node 24 and the runner is green), but its cause is intact and reproducible.

The fixes are all *house shape* and all small:

| # | Site | Change (one sentence) | Twin in the tree |
|---|---|---|---|
| D1 | `src/codetbl.cpp:874` | One bounded copy straight from the name to the caller's buffer, no 1024-byte scratch; a name of exactly the buffer length reports *did not fit* like every longer one. | every `lstrcpyn` in the file |
| D2 | `src/viewer3.cpp:3301` | `defCodeType` starts at 0, so the unloaded-tables path has a defined value. | — (initialiser) |
| D3 | `src/zip.cpp:3301` | The same NULL guard the function already has for `table`, applied to `conversion`. | the `table == NULL` block two lines above |
| D4 | `src/viewer3.cpp:31`, `:36` | After the 259-byte clamp, drop a torn trailing UTF-8 sequence — **only when the copy truncated**. | `src/cmdshell.cpp:232–234` (guarded trim) |
| D5 | `src/plugins/filecomp/controls.cpp:24`, `:39` | Bounded copy into the 260-byte member, then a local walk-back over a torn tail (the plugin cannot call the core helper); `TextLen` recomputed after. | `lstrcpynA` at `controls.cpp:104`, the same file's paint path |
| D6 | `src/plugins/codeview/test/run_tests.cmd` | The worker harness is run with `--experimental-detect-module`, so a Node 20 machine sees `web/worker.js` as the ES module it is. | — (one flag) |

Six commits, one per defect, each with its own fail-first proof and an
independent review — the feature 069 protocol, which this batch inherits
unchanged ([contracts/fix-protocol.md](contracts/fix-protocol.md)). No file
under `src/plugins/shared/` changes; the plugin interface stays at 106; no
byte of a conversion-table name is re-encoded.

## Technical Context

**Language/Version**: C++20 (`/std:c++latest`), MSVC v143, core compiled
without `UNICODE` and with `/J` (plain `char` unsigned — 069 invariant C7).
One Windows batch file (`run_tests.cmd`) and its Node harness.
**Primary Dependencies**: none new. Uses the existing shared helpers
(`lstrcpyn`, `SalU8TrimIncompleteTail` in `src/common/salunicode.cpp`), the
existing trace macros, and the Node flag `--experimental-detect-module`
(Node ≥ 20.10; default-on from 22.12).
**Storage**: none — no registry value, no configuration change. The
conversion-table name bytes (plugin-facing, persisted by dbviewer/filecomp) are
copied, never altered.
**Testing**: `build\tandemcommander\Debug_x64\saltests\saltests.exe`
(baseline 1,301 checks per the 069 handoff — this feature adds none: no fixed
site is reachable from the test program, and the one shared helper it reuses
is already covered at `saltests.cpp:1439–1477`); `python tools\check_encoding.py --strict`;
`src\plugins\codeview\test\run_tests.cmd`; and the recorded scenarios in
[quickstart.md](quickstart.md), each with its fail-first proof (Debug `/RTC1`
stack checks for D1, debugger-forced state for D2/D3/D5, a Czech-UI title for
D4, a Node flag inversion for D6).
**Target Platform**: Windows 11+, x64.
**Project Type**: desktop application (core + two plugins' files + one test
runner).
**Performance Goals**: none of the six sites is on a per-item path (069
protocol A6 "timing if a per-item path" — not applicable; recorded as such in
the fix log).
**Constraints**: FR-009 byte identity for every well-formed input; 069
invariants C1–C10 (see contract); constitution III — no adjacent cleanup, one
revertible commit per defect.
**Scale/Scope**: 5 product source files (≈25 changed lines), 1 test runner
(1 line + a header note), 6 fix-log entries, 6 review records.

## Constitution Check

*GATE: checked before Phase 0 and re-checked after Phase 1 design.*

| Principle | Assessment | Verdict |
|---|---|---|
| I. Build Reproducibility | No new build step, tool, or generated file. The only tooling change is a flag on an existing `node` invocation inside a developer-run test script that the build never calls. | PASS |
| II. Backward Compatibility | For every input handled correctly today the output is byte-identical (FR-009, argued per site in research R1–R6 and re-argued by the reviewer per contract B4). The plugin-facing service (D3) gains a refusal of an argument that crashed before — a defined result replacing undefined behaviour is not a compatibility change; its documented contract already says *FALSE = not found*. `LAST_VERSION_OF_SALAMANDER` stays 106; `src/plugins/shared/` untouched. Conversion-table name bytes untouched (069 F-P4-01). No registry, no config-version change. | PASS |
| III. Incremental Modernization | Six changes, each a few lines, each its own commit, each revertible alone. The temptation this feature must resist is the neighbour: `CFilecompThread`'s two `strcpy`s are the same shape as D5 and are *not* touched (bounded by construction at intake — research R5); `viewer3.cpp`'s nine `IDS_VIEWERTITLE` sites stay as 069 left them. | PASS |
| IV. Windows Platform Commitment | Unchanged. | PASS |
| V. Plugin Architecture Preservation | No interface touched. D5 lives inside the filecomp plugin and uses only what the plugin already links (`lstrcpynA`, a 6-line local walk-back) — the core helper is not exported and the shared headers must not grow for this. D3 makes a plugin-facing service safer without changing its contract. | PASS |
| VI. UI Consistency | No dialog, no control, no visual setting. D4 makes the viewer title render through the same path for long names as for short ones. | PASS |
| Release Documentation | CHANGELOG text is drafted in the fix log (D4 in the user's terms; one honest hardening line for D1/D3/D5; nothing for D2/D6) and applied at the ship gate of the release that carries it, with the version/build bump in the same change — the feature 071/074 pattern. No version bump in this feature. | PASS (planned) |

**Post-Phase-1 re-check**: no violation introduced. The design adds no helper,
no header, no file in the product tree; the contract is the 069 protocol by
reference plus this feature's per-site consumer lists. Complexity Tracking is
empty and omitted.

## Project Structure

### Documentation (this feature)

```text
specs/075-fix-small-hardening/
├── plan.md                      # this file
├── research.md                  # Phase 0 — R0..R9: HEAD check, per-site design, test strategy
├── data-model.md                # Phase 1 — the fix record and the defect-item lifecycle
├── quickstart.md                # Phase 1 — gates G1–G6, fixtures, scenarios S1–S6 with fail-first proofs
├── contracts/
│   └── fix-protocol.md          # Phase 1 — 069 protocol inherited + this feature's per-site consumer lists
├── checklists/
│   └── requirements.md          # spec quality checklist (all green)
├── fix-log.md                   # written during implementation (FR-011)
├── findings/                    # review-D1.md … review-D6.md, written by the reviewer
└── tasks.md                     # Phase 2 — /speckit-tasks, NOT created here
```

### Source code (repository root)

```text
src/
├── codetbl.cpp                  # D1  CCodeTables::GetCodeName — bounded copy, no scratch buffer
├── viewer3.cpp                  # D2  Coding-menu default initialised (≈:3300)
│                                # D4  SetViewerCaption: guarded trim after both clamps (:31, :36)
├── zip.cpp                      # D3  CSalamanderGeneral::GetConversionTable — NULL 'conversion' refused
└── plugins/
    ├── filecomp/controls.cpp    # D5  CFileHeaderWindow ctor + SetText — bounded copy, boundary walk-back
    └── codeview/test/run_tests.cmd   # D6  --experimental-detect-module on the worker harness (+ header note)
```

**Structure Decision**: every change lands in the file that already owns the
defect. No new file, no new helper, no shared-header change. The plugin-side
fix (D5) is self-contained because the plugin ABI is frozen (069 invariant C9)
and a shared helper would be an interface change for a 6-line need.

## Design

### D1 — `CCodeTables::GetCodeName` *(FR-002)* — research R1

Replace the copy-into-`buff[1024]`-then-clamp sequence with a single bounded
copy from the source string:

- pick the source pointer (`LoadStr(IDS_VIEWERNONECODING)` for type 0, else
  `Table->Data[codeType-1]->Name`), take its length once;
- `lstrcpyn(buffer, name, bufferLen)` when `bufferLen > 0`;
- return `nameLen < bufferLen`.

The scratch buffer disappears, and with it the second, unbounded overflow (a
`convert.cfg` name is stored whole by `DupStr` at `codetbl.cpp:247`). For every
name shorter than the buffer the bytes written and the result are identical to
today's; for a longer name the prefix and the *did not fit* result are identical
to today's; the one behavioural change is the boundary case (name length ==
buffer length), which today writes one byte past the buffer and reports *fits*,
and afterwards reports *did not fit* with a clean 199-byte prefix — the spec's
FR-002. A non-positive `bufferLen` — today a `strncpy` with a negative length —
returns FALSE and writes nothing.

Consumers of the result (all three enumerated, research R1): `dialogs3.cpp:136`
(1024-byte buffer, return ignored), `viewer3.cpp:58` (200, return ignored),
`viewer3.cpp:1914` (200, return used — a boundary-length name now clears
`DefaultConvert` instead of storing a truncated default; intended).

### D2 — viewer *Coding* menu default *(FR-003)* — research R2

`int defCodeType = 0;` with a one-line comment naming the unloaded path. Nothing
else: when the tables are loaded, `GetCodeType` always assigns (an entry index
or 0), so the loaded behaviour — including "*none* becomes the default when the
stored name no longer exists" — is untouched. When they are not loaded,
`InitMenu` has already returned early with an empty menu, and
`SetMenuDefaultItem(subMenu, CM_CODING_MIN, FALSE)` on an empty menu fails
harmlessly instead of receiving an arbitrary id.

### D3 — `CSalamanderGeneral::GetConversionTable` *(FR-004)* — research R3

Add, directly after the existing `table == NULL` block and in its exact shape:
`if (conversion == NULL) { TRACE_E("Invalid parametr (conversion==NULL) in CSalamanderGeneral::GetConversionTable!"); return FALSE; }`.
The trace macro on the first line prints `(null)` for a NULL `%s` under the UCRT
and is left where it is (the twin `RemoveFilesFromCache` has the same order).
Non-NULL path: byte-identical. Shared header untouched — its comment already
defines FALSE as *not found / table not valid*.

### D4 — `CViewerWindow::SetViewerCaption` *(FR-005)* — research R4

Both clamps (`FileName` at `:31`, plugin `Caption` at `:36`) get the guarded
trim in the shape of `cmdshell.cpp:232–234`:

```
lstrcpyn(caption, src, MAX_PATH);
if (strlen(src) >= MAX_PATH)        // the copy truncated
    SalU8TrimIncompleteTail(caption); // never cut a UTF-8 sequence in half
```

The guard is the design decision, not a nicety: `FileName` is always WTF-8
(facade output), but `Caption` is plugin-supplied and may be code-page bytes
from a legacy plugin (cluster B-5) — an unconditional trim would drop the last
character of an *untruncated* ANSI caption ending in a byte ≥ 0xC0. With the
guard, any name of 259 bytes or fewer is byte-identical (FR-005), a truncated
UTF-8 name loses only its torn tail, and a truncated ANSI caption may lose one
more byte of an already-cut string, with the legacy fallback draw (`:89`) still
in place. `SalU8TrimIncompleteTail` is WTF-8-aware by construction (a lone
surrogate's three bytes are one sequence) and leaves a complete final character
alone (`saltests.cpp:1439–1477`).

### D5 — `CFileHeaderWindow` *(FR-006)* — research R5

Both `strcpy(Text, text)` become `lstrcpynA(Text, text, _countof(Text))`
followed by a local walk-back that drops trailing continuation bytes and an
incomplete lead (the same six lines as the core helper, kept local because the
plugin cannot see `salunicode.h` and the shared headers must not grow);
`TextLen` is taken from `Text` *after* the trim, not from the argument. `Text`
is UTF-8 by the plugin's own comment (`controls.cpp:88`, "interface 104"), so
the walk-back is the right boundary rule; the paint path already falls back to
the narrow draw when the text is not UTF-8. A text that fits is byte-identical.
The intake is bounded by construction (`filecomp.cpp:497–598`,
`SalPathAppend(…, MAX_PATH)`; `remote.cpp:113`), so this is defensive — and the
neighbouring `CFilecompThread` constructor `strcpy`s, bounded by the same
construction, are deliberately not touched (constitution III).

### D6 — `run_tests.cmd` *(FR-007)* — research R6

The worker harness line becomes
`node --experimental-detect-module "%REPO%\src\plugins\codeview\test\harness\test_worker.mjs"`,
and the script header gains one line: *needs Node ≥ 20.10 (the flag); default
behaviour from 22.12*. `test_page.mjs` lifts source text and never imports the
`.js` file, so it is not touched. Nothing under `web/` changes, so the data
harness's resource-table rule is not in play. Verified on the development
machine: the flag is accepted on Node 24 and the harness passes; inverting it
(`--no-experimental-detect-module`) reproduces the failure class without a
Node 20 install.

### Explicitly not changed

- `CCodeTablesData::Name` bytes and everything that carries them to plugins
  (`EnumConversionTables`, `GetConversionTable`'s lookup, `CodingNameEqual`).
- `CCodeTables::GetCodeType`'s "not found ⇒ 0" contract.
- The `CALL_STACK_MESSAGE` lines in either fixed function.
- `viewer3.cpp`'s nine ANSI `IDS_VIEWERTITLE` sites (069 §2 item 7).
- `CFilecompThread::CFilecompThread` (`filecomp.h:86–93`) — same shape as D5,
  bounded at intake, out of scope.
- `web/worker.js`, `web/assets.rc2`, the resource table, `check_data.py`.
- Any `saltests` change: nothing added, nothing removed.

## Risks and how they are handled

| Risk | Handling |
|---|---|
| A "safe" copy changes a byte for a valid input (the 069 failure mode: two review batches rejected) | Per-site byte-identity argument in research R1–R6; the reviewer re-derives it (contract B4); quickstart S1c/S4c/S5c are the identity checks. |
| D4's trim alters a plugin caption that was fine | The guard (trim only when truncated); the twin shape at `cmdshell.cpp:232`; the legacy fallback draw stays. |
| D1's rewrite silently changes the boundary the callers rely on | The only caller using the return (`viewer3.cpp:1914`) is walked in research R1; the change there is the intended one. |
| D6 lowers the floor for someone on Node < 20.10 | Documented in the runner header; the failure is then an explicit "bad option", not a misleading harness failure. |
| The fix log claims "proven" without a before/after | Contract A5: proof is mechanical and pasted — `git stash` → run → fails, `git stash pop` → run → passes. |
| Scope creep into the neighbours | Contract C10 list; each commit touches one D; the reviewer rejects a diff that touches a site outside its D. |

## Ship gate (release only — not part of this feature)

The release that carries this batch adds, in one change with the version and
build bump (`src/plugins/shared/spl_vers.h`, `setup/tandemcommander.iss`,
`CLAUDE.md`), the CHANGELOG *Fixed* entries drafted in `fix-log.md`:

- D4: "The viewer's title bar showed garbled accented characters for files
  under very long paths (over 259 bytes) — the name was cut in the middle of a
  character and the whole title fell back to the legacy code page."
- D1/D3/D5 (one line): "Hardened three internal copies — the viewer's
  conversion-name lookup, the File Comparator's header, and the argument check
  of a plugin-facing conversion service. No known way to trigger any of them
  with shipped data."

`LAST_VERSION_OF_SALAMANDER` (106) is not touched.

## Phase status

- [x] Phase 0 — research complete, no NEEDS CLARIFICATION remaining
- [x] Phase 1 — data model, contract and quickstart written; Constitution
      Check re-run clean
- [ ] Phase 2 — `tasks.md` (`/speckit-tasks`)

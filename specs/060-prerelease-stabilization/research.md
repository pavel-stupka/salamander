# Phase 0 Research: Pre-Release Stabilization Review

**Feature**: 060-prerelease-stabilization · **Date**: 2026-08-18

## R1. Delta scope (exact)

Baseline = release commit `b74875b` ([056] release 0.1.2, build 186).
Delta = `b74875b..HEAD`: commits `4b469b1` (056 docs), `e1a1650` (**057**),
`4a00dc8` (**058**), `1548d92` (**059**), plus spec/docs commits on the
feature branches. Code-bearing files (2,609 insertions / 28 deletions):

| Area | Files | Feature |
|---|---|---|
| Core C++ | `src/shiconov.cpp` (+215), `src/shiconov.h` (+17), `src/snooper.cpp` (+59/-...), `src/fileswn1.cpp` (+19), `src/geticon.cpp` (+16), `src/common/handles.cpp` (+11), `src/common/handles.h` (+3) | 058 + 059 |
| Resources | `src/res/syncpend.ico`, `src/resource.rh2`, `src/salamand.rc2` | 059 |
| Dev tooling | `tools/brand/gen_overlay_syncpend.py` (+86), `tools/brand/README.md` | 059 |
| Standalone utility | `utils/migrate-altap-settings.cmd` (**+1,339**), `utils/README.md`, `utils/test/run_migration_tests.cmd` (+374), `utils/test/fixtures/*.reg` (3 files) | **057** |
| Docs | `CHANGELOG.md`, `CLAUDE.md`, `specs/058-*/`, `specs/059-*/` | all |

**Finding of this scoping pass**: the delta is NOT only 058+059 — feature
057 (Altap settings migration) ships 1.7k lines of batch/registry tooling
that reads a foreign configuration and **copies stored FTP passwords**
between registry hives. The user prioritized 058/059, but FR-001 covers the
whole delta and 057 is the most security-sensitive part of it. It gets its
own perspective (credentials/tooling), mirroring feature 056's
tooling/data + credentials angles.

## R2. Decision: review vehicle — parallel subagents via the Agent tool

**Decision**: run the perspectives as **parallel read-only subagents**
(Agent tool, one per perspective, each with a written charter and the exact
file list), findings returned in a structured format; **adversarial
verification happens in the main context** against the actual code (with
the option of a dedicated fresh verification agent for contested findings).

**Rationale**: the perspectives must be independent (a single reader
anchors on its own first impressions — exactly what feature 056 avoided);
the Agent tool provides that isolation without requiring the multi-agent
workflow machinery. The main context holds the deepest knowledge of the
058/059 design intent and is therefore the right adversary for verification
— it can refute a plausible-but-wrong finding with line evidence quickly.
The main context author also wrote the delta, so verification verdicts must
cite code evidence, not intent ("the code shows X", never "it was meant to
do X").

**Alternatives considered**: single-pass self-review (rejected: author
blindness, no independence); full multi-agent workflow orchestration
(rejected: not opted into by the user; Agent-tool parallelism achieves the
required independence).

## R3. Perspective roster and charters

Six perspectives, each a separate subagent with read access and a bounded
file list:

| # | Perspective | Primary targets | Charter focus |
|---|---|---|---|
| P1 | Memory & object lifecycle | shiconov.cpp/h, geticon.cpp, handles.* | every new/delete, HICON create/destroy pairs (init failure paths, ColorsChanged swap, Cleanup), SalU8ToWAlloc free-on-all-paths, COM Release / PROPVARIANT clear pairing, leak-on-early-return |
| P2 | Concurrency & thread affinity | shiconov.cpp/h, fileswn1.cpp, snooper.cpp | init-order and visibility of `CfGetSyncRootInfoByPathDyn` and `CloudSyncPendingIndex` vs. icon-reader threads; Overlays array mutation vs. reader iteration; STA requirements of the property query in reader threads; snooper data ownership (DataUsageMutex protocol) around the new conversions; GD_CS unchanged semantics |
| P3 | Error & failure paths | all core delta | every failure branch: SalU8ToW==0 fallbacks and their terminator/truncation behavior, unchecked `LoadIconWithScaleDown` inside `SalLoadIcon` as used by the new code, `Add()` failure cleanup, cldapi absent, property store/GetValue failures, `FindFirstChangeNotificationW` failure semantics unchanged |
| P4 | Encoding & buffer correctness | fileswn1.cpp, snooper.cpp, geticon.cpp, shiconov.cpp | offset math (`wName = wPath + wl - 1`), buffer capacities (`MAX_PATH+10`, `3*MAX_PATH`, `_countof`), CP_ACP fallback termination, the `(wName-wPath)+strlen(name)` guard in mixed units, no byte/WCHAR confusion anywhere in the delta |
| P5 | Performance | shiconov.cpp, fileswn1.cpp | added cost on hot paths (overlay loop skip, per-cycle sync-root call, per-item property query bounds), zero added cost outside sync roots, no new work on the UI thread |
| P6 | Security / credentials / tooling | utils/* (057), gen_overlay_syncpend.py, cldapi load, property blob handling | 057: registry parsing robustness, handling of stored FTP passwords (at-rest copies, echoes to console/files, restore-script contents), destructive-operation guards, injection via crafted .reg/registry values; 059: LoadLibrary("cldapi.dll") search-path discipline, treating property values as untrusted; brand generator (dev-only) sanity |

Docs/spec files are excluded from line review (no executable behavior);
CHANGELOG/CLAUDE.md accuracy is checked by the report author (US3).

## R4. Verification protocol (FR-002)

For every finding raised: reproduce the claimed failure path in the actual
code (cite file:line and the concrete input/state); verdict **CONFIRMED**
(evidence shows the defect) or **REFUTED** (evidence shows why it cannot
happen). Contested or high-impact findings get a second, independent
verification agent with a refute-first charter. Only CONFIRMED findings may
change code (FR-003); each fix re-runs the raising perspective's relevant
check plus affected gates (bounded re-verification per spec edge case).

## R5. Gates (FR-005 / SC-004)

| Gate | Check | Pass bar |
|---|---|---|
| G1 | `build.cmd full` (Debug x64) | 0 errors; no new warnings in delta files |
| G2 | `build.cmd full release` | 0 errors; no new warnings in delta files |
| G3 | saltests (Debug x64) | 1145 checks, 0 failed (baseline) |
| G4 | Startup/exit health | Debug binary: start, ≥10 s alive, **graceful close** (WM_CLOSE), exit code 0, no new crash report in `%LOCALAPPDATA%\Tandem Commander`, no fresh HANDLES/leak complaints observable |
| G5 | 057 utility harness | `utils/test/run_migration_tests.cmd` all scenarios pass |
| G6 | 058/059 validated behavior | stands on existing `evidence.md` unless a stabilization fix touches the covered code — then the affected manual scenario re-runs (user) |
| G7 | Icon/resource sanity | `syncpend.ico` loads at 16/32/48 (automated LoadImage check) |

Note on G4: without a trace server, TRACE-level leak reports are not
capturable; the observable bar is exit-code-0 graceful shutdown + no crash
report + no debug assertion dialogs (a hung dialog would show as a
non-exiting process). Recorded as the gate's defined evidence, per the
spec's "gates record what they can prove" edge case.

## R6. Report

`specs/060-prerelease-stabilization/review-report.md`, modeled on
`specs/056-prerelease-review/review-report.md`: scope, method, per-
perspective findings tables (finding → verdict → evidence → disposition),
gate table, deferrals, go/no-go verdict. Every code change in this feature
must appear in the findings table (SC-005 traceability).

## R7. Known inputs the review must not ignore

Seeded questions for perspectives (from the main context's own doubts —
the reviewers may confirm, refute, or find beyond them):

- `SalLoadIcon` leaves `hIcon` **uninitialized** when `LoadIconWithScaleDown`
  fails (pre-existing; 059's `LoadCloudSyncPendingIcons` relies on it
  returning NULL-or-valid — does it?).
- `InitCloudSyncPendingOverlay` failure path: `delete item` after a failed
  `Add()` — is `Add()` guaranteed not to have taken ownership?
- `CloudSyncPendingIndex`/`CfGetSyncRootInfoByPathDyn` are written at init
  and read from reader threads without explicit synchronization — is the
  happens-before edge (thread creation after init) actually guaranteed on
  every path (panel re-init? config reload?).
- `GetCloudSyncPendingStateAuxAux` runs under the GD critical section? (It
  must not — check the fallback executes after `LeaveCriticalSection`.)
- 058 snooper fallback: `MultiByteToWideChar(..., -1, wPath, _countof)` on
  failure leaves `wPath` in what state?
- 057: does any code path write a stored password into a world-readable
  file or console output?

# Quickstart: Running & Auditing the Stabilization Review

**Feature**: 060-prerelease-stabilization
**Deliverable**: `review-report.md` + green gates; method in
[research.md](research.md), record schema in [data-model.md](data-model.md).

## Prerequisites

- Branch `060-prerelease-stabilization` (delta baseline: commit `b74875b`,
  release 0.1.2).
- VS2022, Python 3.13 (for the 057 harness fixtures and G7 icon check).

## Gates (automated; run after any fixes land)

```batch
build.cmd full                          :: G1 (Debug) - 0 errors, no new warnings in delta files
build.cmd full release                  :: G2 (Release) - same bar
:: G3: build + run saltests (Debug x64) - expect "1145 checks, 0 failed"
:: G5: 057 utility harness
utils\test\run_migration_tests.cmd
```

- **G4** start/exit health: launch
  `build\tandemcommander\Debug_x64\tandemcommander.exe`, keep it running
  ≥ 10 s, close it gracefully (WM_CLOSE / Alt+F4 — not process kill), expect
  exit code 0, no new `*.TXT` crash report in
  `%LOCALAPPDATA%\Tandem Commander`, no assertion dialogs.
- **G7** icon sanity: `LoadImage` of `src/res/syncpend.ico` at 16/32/48 all
  return non-NULL (scripted check).
- **G6** validated behavior: 058/059 `evidence.md` scenarios stand as
  evidence; re-run manually ONLY the scenarios whose covering code a
  stabilization fix touched.

## Auditing the report

Open `review-report.md` and check:

1. Every delta file (18, listed in research.md R1) appears in at least one
   perspective's coverage list (SC-001).
2. Every finding row has a failure scenario, a verdict with code evidence,
   and a disposition (SC-002).
3. Pick any 3 code changes from `git diff` of this feature → each maps to a
   CONFIRMED finding ID (SC-005).
4. Gate table all PASS/WAIVED-with-reason (SC-004).
5. The go/no-go verdict is explicit and consistent with 1–4 (SC-003).

## Outcome interpretation

- **Go** = release preparation may proceed (version bump + changelog
  finalization happen there, not here).
- **No-go** = the report names the blocking finding(s); fix-and-re-verify
  loop continues inside this feature.

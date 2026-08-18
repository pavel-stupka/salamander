# Implementation Plan: Pre-Release Stabilization Review (Features 058 + 059)

**Branch**: `060-prerelease-stabilization` | **Date**: 2026-08-18 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/060-prerelease-stabilization/spec.md`

## Summary

Adversarially reviewed stabilization of the post-0.1.2 delta before release,
following the feature-056 discipline. Phase-0 scoping (research.md R1) fixed
the exact delta — and widened it beyond the user's headline: besides the 058
encoding fixes and the 059 cloud-badge fallback (~340 changed C++ lines in 7
core files + resources), the delta contains **feature 057's 1.7k-line
settings-migration utility that copies stored FTP passwords between registry
hives** — the most security-sensitive item under review. Six independent
read-only review subagents (memory/lifecycle, concurrency, failure paths,
encoding/buffers, performance, security/credentials/tooling) examine the
delta under written charters; every finding is adversarially verified
against code evidence in the main context (second independent verifier for
contested findings); only CONFIRMED findings drive minimal fixes. Seven
stability gates (full Debug/Release builds, saltests baseline, graceful
start/exit health, the 057 test harness, 058/059 validated-behavior
standing, icon-resource sanity) close the exercise, and a single
review-report records findings, verdicts, dispositions, gate results and
the go/no-go verdict.

## Technical Context

**Language/Version**: review targets C++ (C++20, MSVC v143) core delta +
Windows batch/registry tooling (057) + Python 3.13 dev script (059)
**Primary Dependencies**: none added — review-only; fixes (if any) stay
within the delta's existing dependency set
**Storage**: N/A (no configuration changes; 057 utility reviewed, not
modified unless a defect is confirmed)
**Testing**: gates G1–G7 (research.md R5): `build.cmd full` Debug+Release,
`saltests` 1145/0 baseline, graceful start/exit health, 057 harness
`utils/test/run_migration_tests.cmd`, `syncpend.ico` load check; manual
re-validation only where fixes touch validated behavior (G6)
**Target Platform**: Windows 11+ x64
**Project Type**: desktop application — process feature (review +
stabilization), not product development
**Performance Goals**: none new — P5 perspective verifies the delta added
none of its own regressions
**Constraints**: FR-003/FR-007 — zero changes without a CONFIRMED finding,
no refactoring, no user-visible behavior change beyond confirmed-defect
fixes; findings outside the delta classified per FR-004
**Scale/Scope**: 18 delta files (~2.6k inserted lines), 6 review
perspectives, 7 gates, 1 report

## Constitution Check

| # | Principle | Verdict | Notes |
|---|-----------|---------|-------|
| I | Build Reproducibility | ✅ Pass | Gates exercise the standard build; nothing changes the pipeline. |
| II | Backward Compatibility | ✅ Pass | The feature exists to protect it; fixes require confirmed defects. |
| III | Incremental Modernization | ✅ Pass | Minimal-fix rule is FR-003; no refactoring permitted. |
| IV | Windows Platform Commitment | ✅ Pass | Review-only. |
| V | Plugin Architecture Preservation | ✅ Pass | Delta has no plugin-API surface; P1–P4 confirm `GetFileIcon` contract intact. |
| VI | UI Consistency | ✅ Pass | No UI changes; FR-007 guards behavior. |
| — | Release Documentation | ✅ Planned | Report feeds the release go/no-go; changelog updated only if a fix is user-visible (FR-007). |

**Post-Phase-1 re-check**: design introduces no code, no dependencies — all
gates still pass.

## Project Structure

### Documentation (this feature)

```text
specs/060-prerelease-stabilization/
├── plan.md              # This file
├── research.md          # Phase 0: delta scope, roster, protocol, gates
├── data-model.md        # Phase 1: Finding/Gate/Perspective records
├── quickstart.md        # Phase 1: how to run gates & audit the report
├── review-report.md     # THE deliverable (produced during implement)
└── tasks.md             # Phase 2 (/speckit-tasks)
```

(`contracts/` intentionally omitted: a review process exposes no interface
to users or other systems; the report is the deliverable.)

### Source Code (repository root)

```text
# Under review (read; modified ONLY on confirmed findings):
src/shiconov.cpp/.h  src/snooper.cpp  src/fileswn1.cpp  src/geticon.cpp
src/common/handles.cpp/.h  src/resource.rh2  src/salamand.rc2
src/res/syncpend.ico  tools/brand/gen_overlay_syncpend.py
utils/migrate-altap-settings.cmd  utils/test/**  utils/README.md
```

**Structure Decision**: no new source structure; all outputs live under the
feature's spec directory.

## Complexity Tracking

No constitution violations — table not needed.

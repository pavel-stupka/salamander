# Implementation Plan: Encoding Regression Review and Stabilization

**Branch**: `068-encoding-regression-review` | **Date**: 2026-08-24 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/068-encoding-regression-review/spec.md`

## Summary

A product-wide, independently verified review of every place the core
application changes text representation — the sibling sweep the thirteen
earlier encoding features (004 → 067) never did — plus minimal, regression-
reviewed fixes and durable guards for the defect classes found. Phase-0
sizing (research.md R1) established the real frontier: the core is built
without `UNICODE`, so **287 file-system, 39 shell, 23 process, 137 registry
and 428 UI-text ANSI call sites remain** next to 154 house conversions and
156 UTF-8 sinks; 36 core files are half-migrated; 20 distinct defect
classes exist and only 5½ have a build-time guard; an 89-row deferred
ledger from 17 features was never re-dispositioned; and the plugin
boundary carries ten concrete candidate defects (plugin-facing clipboard
service writing raw bytes, ANSI case tables applied to UTF-8 names, strict
plugin-shared converters vs. the WTF-8 core, ANSI browse dialogs, unswept
cached titles). The plan runs seven charted perspectives in parallel over a
tiered inventory (line-level for every defect-class pattern, data-flow
classification for the bulk ANSI calls), verifies every finding with a
separate refute-first agent, regression-reviews every fix with a third,
proves each new guard against the pre-fix tree, and closes with seven gates
and a Czech + Hungarian on-screen sweep of twenty surfaces. Deliverables:
`inventory.md`, `review-report.md`, fixes traceable to findings, guard rules
and tests, changelog entries.

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143; core built
**without** `UNICODE`/`_UNICODE` (un-suffixed Win32 text APIs are ANSI);
Python 3.x for the build-time guard `tools/check_encoding.py`
**Primary Dependencies**: none added — review + minimal fixes inside the
existing house machinery (`src/common/salunicode.*`, `winlib.*`,
`salfileio.*`, `salpath.*`, `LoadStrU8`, `Sal*U8` sinks, `SalReg*W8`,
`CopyTextToClipboardU8`)
**Storage**: registry `HKCU\Software\Tandem Commander\0.1` via the UTF-8/
WTF-8 facade — reviewed (B7), format unchanged, no migration (FR-014)
**Testing**: gates G1–G7 (research.md R6): `build.cmd full` Debug +
Release; `saltests.exe` ≥ 1229/0; `check_encoding.py --strict` = 0 with
each new rule proven against the pre-fix tree; Debug start/exit health with
the Trace Server; stopwatch timing envelope on the 100,000-file fixture for
per-item-path fixes; English spot-check; manual sweep W1–W20 in Czech and
Hungarian UI (quickstart.md)
**Target Platform**: Windows 11 x64; verification machine = Czech Windows 11
(Czech regional format), UI switched between `czech.slg`, `hungarian.slg`,
`english.slg`
**Project Type**: desktop application — process feature (review +
stabilization), not product development
**Performance Goals**: none new; fixes on per-item paths must stay within
the baseline's run-to-run envelope (SC-010)
**Constraints**: FR-007 no change without a CONFIRMED finding; FR-008/FR-009
independent regression review, English/ASCII and plugin-facing bytes
identical, interface version 106 unchanged; FR-012/FR-015 scope tests for
plugin-internal and non-encoding defects; Constitution III (no adjacent
refactoring)
**Scale/Scope**: 217 core files (136 `.cpp`), ~1,000 ANSI-boundary sites in
three triage tiers, 20 defect classes, 12 binding contracts, 89 ledger rows,
10 plugin-boundary seeds, 7 perspectives, 7 gates, 20 sweep surfaces × 2
languages

## Constitution Check

| # | Principle | Verdict | Notes |
|---|-----------|---------|-------|
| I | Build Reproducibility | ✅ Pass | Gates use the standard `build.cmd`; new guard rules run inside it on every build; no pipeline change beyond `tools/check_encoding.py` rules and `saltests` additions. |
| II | Backward Compatibility | ✅ Pass | The feature exists to protect it: fixes only on CONFIRMED findings, byte-identity for English/ASCII and plugin-facing output, interface version 106 unchanged, registry format unchanged. |
| III | Incremental Modernization | ✅ Pass | Minimal-fix rule (FR-007); no refactoring of adjacent code; standing decisions L18/L19/L36 not revisited (research R9). |
| IV | Windows Platform Commitment | ✅ Pass | Pure WinAPI throughout; fixes convert to W entry points via the house helpers. |
| V | Plugin Architecture Preservation | ✅ Pass | Plugin-facing services byte-frozen; plugin-internal fixes only under FR-012; SDK header changes documentation-only (S6/S7 findings become comments, not ABI). |
| VI | UI Consistency | ✅ Pass | No UI changes; user-visible differences are confirmed-defect fixes only (FR-014), described in the changelog. |
| — | Release Documentation | ✅ Planned | Every user-visible fix → `CHANGELOG.md` Unreleased; release itself out of scope. |

**Post-Phase-1 re-check**: the design adds no dependencies, no new
functionality and no structural change — only review records, guard rules,
tests and finding-traceable fixes. All gates still pass.

## Project Structure

### Documentation (this feature)

```text
specs/068-encoding-regression-review/
├── plan.md                                  # This file
├── research.md                              # Phase 0: scope/sizing, vehicle, triage, roster, protocol, gates, seeds, guards
├── data-model.md                            # Phase 1: Boundary/Site/Defect class/Finding/Fix/Deferred/Gate/Sweep records
├── contracts/
│   └── encoding-contract-checklist.md       # Phase 1: 20 defect classes, 12 contracts' obligations, 89-row ledger, sweep additions
├── quickstart.md                            # Phase 1: gates, timing method, language switching, sweep W1–W20, report audit
├── inventory.md                             # produced during implement: sites per boundary, DC sweep, contract compliance
├── review-report.md                         # THE deliverable (produced during implement)
└── tasks.md                                 # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
# Under review (read; modified ONLY on CONFIRMED findings) — research.md R3/R4:
src/*.cpp src/*.h                      # 173 files; Tier 1 = the 36 half-migrated + every defect-class pattern
src/common/*.cpp src/common/*.h        # converters, sinks, facades (P3); excludes src/common/dep/**
src/plugins/shared/spl_*.h             # SDK encoding statements (P5) — documentation changes only
src/plugins/shared/splunicode.h        # plugin-shared converters (S3 / L48)
src/plugins/<enabled>/**               # boundary sites only (P5); fixes only under FR-012
src/zip.cpp src/plugins1.cpp src/plugins2.cpp src/plugins3.cpp src/packers.cpp   # plugin boundary in the core

# Guards and tests (extended):
tools/check_encoding.py                # new rules per research.md R8, proven fail-before/pass-after
src/saltests/saltests.cpp              # property tests per R8; baseline 1229/0 → higher

# Records:
CHANGELOG.md                           # Unreleased: user-visible fixes
translations/languages.cfg             # re-enable checklist notes for latent (disabled-language) items
```

**Structure Decision**: no new source structure. All review outputs live in
the feature directory; code changes are point fixes at the sites of
confirmed findings plus guard/test additions in their existing files.

## Phase outline (input to /speckit-tasks)

- **Phase A — Inventory & findings** (parallel P1–P6, read-only): Tier-1
  line-level sites, Tier-2 data-flow groups, Tier-3 file sanity; the DC-01…
  DC-20 sibling sweeps; the B1–B12 contract compliance verdicts; the L01–L89
  re-examination; the 066/067 delta line review; seeds S1–S10, C-a…C-l.
  Output: `inventory.md` + findings pool. P7 in parallel: guard rules and
  tests designed and proven against the current tree.
- **Phase B — Independent verification**: one fresh verifier per finding
  batch; CONFIRMED/REFUTED with evidence; scope test (FR-012/FR-015).
- **Phase C — Fixes with independent regression review**: minimal fix →
  affected-surface list → fresh regression reviewer → ACCEPTED/REJECTED;
  timing record for per-item paths; check proven fail-before/pass-after;
  shared-machinery fixes trigger the full sweep.
- **Phase D — Gates, sweep, report**: G1–G7; W1–W20 in cs and hu (+ en
  spot-check — the user's manual pass, listed explicitly); ledger
  dispositions; changelog; `review-report.md` with the stability verdict.

## Complexity Tracking

No constitution violations — table not needed.

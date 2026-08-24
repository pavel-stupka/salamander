# Implementation Plan: Finish the Contained Encoding Fixes

**Branch**: `069-finish-encoding-fixes` | **Date**: 2026-08-24 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/069-finish-encoding-fixes/spec.md`

## Summary

Feature 068 confirmed 60 encoding defects and fixed nine; this feature fixes
the contained remainder — the 34 items of its handoff section 1 plus the five
small deferred items D01–D05 — under one governing rule from the user: **no
change may make anything that works today stop working.**

Phase-0 research settled three things that shape the work. First, **scope is
31 fixes, not 34**: F-P2-10 turned out to be the same site as F-P6-02 and was
already fixed by X02, and F-P1-03 plus the jump-list half of F-P1-25 are
already closed by X06/X07 and X03 — so every task begins with a
"still-defective-at-HEAD?" check rather than assuming the handoff is current.
Second, the feature's top item (F-P6-04, the command line) does **not** need
the window-model change everyone would reach for: the control already writes
and reads its text through the wide house helpers and `CEditLine::InsertText`
is its single violator, so the fix is six lines at that sink and moves no
selection offset, no word-break callback and no `WM_CHAR` unit — the complete
"true Unicode command line" variant is enumerated and handed to cluster B-1.
Third, the automated-check surface is bounded by what `saltests` links (four
`src/common` files), so each fix's fail-before/pass-after proof is chosen
deliberately: unit test where the logic is a pure conversion (extracting it
into `src/common/` is the only refactoring permitted, and only when it buys a
test), a proven `check_encoding.py` rule where the defect has a grep-able
shape, and a written maintainer scenario otherwise.

Delivery is 12 coupling groups in ascending risk order, one commit each, each
gated by an independent regression review that re-enumerates consumers itself
— the protocol that rejected three of 068's nine fixes before they landed, all
three for regressions no build, test or guard would have caught. The two
groups most likely to be rejected (external archivers, application locations)
land last, so a deferral costs the feature least.

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143; core built
**without** `UNICODE` (every un-suffixed Win32 text API is the ANSI entry
point) and with `/J` (plain `char` is unsigned); Python 3.x for the build-time
guard
**Primary Dependencies**: none added — every fix uses the existing house
machinery (`src/common/salunicode.*` converters, `winlib.*` `Sal*U8` sinks,
`salfileio.*` facades, `salpath.*`, `LoadStrU8`, `SalReg*W8`,
`CSalPathBuf`); shapes S1–S5 in [research.md](research.md) R3
**Storage**: registry `HKCU\Software\Tandem Commander\0.1` through the
existing UTF-8/WTF-8 facade — format unchanged, **no migration** (a MINORB
release must not move configuration); two items (F-P4-03, F-P4-01) analyse
what happens to values already persisted by older builds
**Testing**: G1–G8 ([data-model.md](data-model.md)): `build.cmd full` Debug +
Release; `saltests.exe` ≥ 1,257 checks / 0 failed plus this feature's checks;
`python tools\check_encoding.py --strict` = `TOTAL: 0` with every added rule
proven to fire on the pre-fix line; Debug start/exit health; timing on the
100,000-file fixture for per-item paths; English spot-check; maintainer
on-screen sweep W1–W20 in Czech and Hungarian
**Target Platform**: Windows 11 x64; verification machine = Czech (CP1250)
Windows 11, UI switched between `czech.slg`, `hungarian.slg`, `english.slg`
**Project Type**: desktop application — defect-fix feature over an existing
review record; no new functionality
**Performance Goals**: none new; per-item-path fixes (F-P1-27's share marker,
any icon/expansion path) must stay inside the baseline's run-to-run envelope
**Constraints**: spec FR-002 fix only what the verdict confirmed · FR-003
convert the whole producer→sink chain · FR-004 never blank text or skip an
operation · FR-005 English/ASCII and plugin-facing bytes byte-identical,
interface version 106 unchanged · FR-006 independent regression review per fix
· FR-007 one small revertable commit per item or coupling group · Constitution
III no refactoring of adjacent code (single exception: extracting a pure
conversion to make it testable, R6 D-T1)
**Scale/Scope**: 31 fixes + 4 tooling/plugin items + 1 documentation item,
across ~35 core files and 3 plugin files, in 12 coupling groups; 5 systemic
clusters (17 findings) explicitly excluded

## Constitution Check

| # | Principle | Verdict | Notes |
|---|-----------|---------|-------|
| I | Build Reproducibility | ✅ Pass | Gates use the standard `build.cmd`; the guard runs inside every build; D01 makes `tserver` build again, removing a waiver rather than adding one. No pipeline change. |
| II | Backward Compatibility | ✅ Pass | The feature's whole purpose. Byte identity for English/ASCII and plugin-facing output (FR-005); registry format unchanged, no migration; every fix reverts independently (FR-007). |
| III | Incremental Modernization | ✅ Pass | Minimal fixes at confirmed sites; no adjacent refactoring. The one exception (extracting a pure conversion so it can be unit-tested) is bounded, declared in R6 D-T1, and buys a required check. |
| IV | Windows Platform Commitment | ✅ Pass | Pure WinAPI; fixes move to W entry points through the house helpers. |
| V | Plugin Architecture Preservation | ✅ Pass | `LAST_VERSION_OF_SALAMANDER` unchanged; `src/plugins/shared/` changes are comments only (F-P5-06); plugin-local fixes (D03, D04, D02-if-possible) under FR-012; F-P1-21 group 1 and D02 proceed only if no plugin-visible byte changes. |
| VI | UI Consistency | ✅ Pass | No UI or layout change; the only visible differences are the confirmed defects being repaired. |
| — | Release Documentation | ✅ Planned | Every user-visible fix → `CHANGELOG.md` Unreleased, truthfully scoped (including F-P6-04's residual limitation and F-P4-07 being hygiene). No version bump — releasing is a separate decision. |

**Post-Phase-1 re-check**: the design adds no dependency, no interface change
and no new functionality; it adds review records, tests, one guard rule and
finding-traceable point fixes. All gates still pass.

## Project Structure

### Documentation (this feature)

```text
specs/069-finish-encoding-fixes/
├── plan.md                          # This file
├── spec.md                          # Requirements (34 + 5 items, FR-001..FR-018)
├── research.md                      # Phase 0: scope correction, command-line decision, shapes, coupling, tests, gates, risks
├── data-model.md                    # Phase 1: item / fix / verdict / check / gate / sweep / fixture records
├── contracts/
│   └── fix-protocol.md              # Phase 1: the binding per-fix procedure and the reviewer's checklist
├── quickstart.md                    # Phase 1: fixture creation, gate commands, per-fix scenarios, sweep
├── checklists/requirements.md       # spec quality checklist (16/16)
├── findings/regression-X<nn>.md      # produced during implement: one per fix
├── closing-report.md                # produced during implement: dispositions, verdicts, gates, sweep
├── REMAINING-WORK.md                # produced during implement: handoff for what stays open (B-1..B-5, …)
└── tasks.md                         # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
# Core — point fixes at confirmed sites only (grouped per research.md R4):
src/editwnd.cpp                      # C1 command line insert + internal drop payload
src/toolbar5.cpp src/stswnd.cpp src/viewer3.cpp        # C1 drops; C7 viewer caption; F-P3-07 tooltip clamp
src/drivelst.cpp src/shiconov.cpp src/fileswn3.cpp     # C2 cloud roots
src/salamdr2.cpp src/mainwnd5.cpp src/dialogs3.cpp     # C3 volume/subst/label + Drive Information type line
src/shares.cpp src/fileswn9.cpp                        # C6 shares (per-item path → timing)
src/pack1.cpp src/pack2.cpp src/pack3.cpp              # C4 external archivers
src/salamdr5.cpp src/salamdr1.cpp src/mainwnd3.cpp src/mainwnd4.cpp src/execute.cpp
src/salamdr6.cpp src/shellib.cpp                       # C5 application locations + browse dialogs
src/codetbl.cpp                                        # C7 conversion-name intake (UTF-8 by contract)
src/packers.cpp src/packac.cpp src/salamdr4.cpp        # C9 configuration seeds
src/plugins2.cpp src/dialogs5.cpp                      # C8 plugin-manager lists
src/salamdr3.cpp src/fileswn8.cpp                      # F-P1-20 archive-edit copy
src/shellsup.cpp src/worker.cpp src/dialogs6.cpp src/fileswn0.cpp src/fileswn2.cpp
src/salshlib.cpp src/icncache.cpp src/zip.cpp          # F-P1-21 groups, F-P1-23, F-P1-25
src/gui.h src/gui.cpp                                  # F-P3-07: CopyToolTipAnswer made shared (R6 D-T1)
src/common/handles.h src/common/handles.cpp            # D01 (must not change core codegen)

# Plugins — only under FR-012:
src/plugins/mdview/webview.cpp                         # F-P6-01 keeper class unregister
src/plugins/filecomp/controls.cpp src/plugins/filecomp/worker2.cpp   # C11 (D03, D04)
src/plugins/zip/common.cpp                             # D02 — only if provably plugin-local
src/plugins/shared/spl_fs.h                            # F-P5-06 — comments only

# Checks, tooling, records:
src/saltests/saltests.cpp                              # new checks (baseline 1,257)
tools/check_encoding.py                                # +1 strict rule (C9), −1 void rule, counts recorded
.specify/extensions/git/scripts/powershell/auto-commit.ps1   # D05
CHANGELOG.md                                           # Unreleased: user-visible fixes
```

**Structure Decision**: no new source structure and no new project. Changes
are point fixes in existing files plus checks in the existing harness. The
only header movement is exporting one existing static helper
(`CopyToolTipAnswer`) so its clamp can be unit-tested and reused by the site
F-P3-07 names — declared in `research.md` R6 D-T1 as this feature's single
permitted refactoring.

## Phase outline (input to /speckit-tasks)

- **Phase A — Preparation**: recreate the fixtures (FX-CS/HU/SUR/PERF/…,
  `quickstart.md`); capture the baseline (build, `saltests` count, guard
  `TOTAL`, and a pre-fix Release build kept aside for the English
  byte-identity spot-check); land **C12** (D01 tooling → unlocks the G5 Trace
  Server capture; D05 developer script).
- **Phase B — Fixes, ascending risk** (research.md R10 order): C10 singles →
  C11 → C7 → C9 → C2 → C6 → C1 → C3 → C4 → C5. Per item: still-defective
  check → chain trace → minimal fix → check written and proven fail-before →
  fix record → **independent regression review** → ACCEPTED ⇒ commit, then
  G1–G4 before the next group; REJECTED ⇒ ≤ 2 reworks, else deferred with the
  reviewer's reason.
- **Phase C — Conditional items**: F-P1-21 group 1 (`zip.cpp` plugin viewer
  temp file) and D02 — proceed only if the analysis proves no plugin-visible
  byte changes (FR-012); otherwise defer with the reason written down.
- **Phase D — Gates and sweep**: G5–G7 automated; assemble the maintainer
  scenario list (W1–W20 plus one per fix) and hand it over; a sweep failure
  routes back through fix → review → gates.
- **Phase E — Records**: `CHANGELOG.md` Unreleased entries (truthful scope,
  including F-P6-04's residual `?` limitation); `closing-report.md` with every
  disposition, verdict, check and gate; `REMAINING-WORK.md` handoff listing
  what stays open (B-1–B-5, the complete command-line fix, anything deferred).

## Complexity Tracking

No constitution violations — table not needed. The one deviation worth naming
is the permitted extraction of `CopyToolTipAnswer` (Constitution III), justified
in `research.md` R6 D-T1: it converts an untestable clamp into a unit-tested
one, which FR-008 requires.

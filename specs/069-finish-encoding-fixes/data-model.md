# Data Model — Finish the Contained Encoding Fixes

**Feature**: 069-finish-encoding-fixes · **Spec**: [spec.md](spec.md) ·
**Plan**: [plan.md](plan.md)

This is a fix feature, not a product feature: its "data" are the records that
make every change traceable (spec FR-001, FR-014) and every fix provably
non-regressive (FR-003–FR-010). The records mirror feature 068's
(`specs/068-encoding-regression-review/data-model.md`) so the two ledgers
read as one; only the fields that differ are described in full.

## In-scope item

One row of the spec's defect inventory (34 section-1 findings + D01–D05).

| Field | Values / meaning |
|---|---|
| ID | `F-P<n>-<seq>` (068 finding) or `D0<n>` (068 deferred item) |
| Verdict scope | verbatim from `findings/verdicts-V*.md`: what is **confirmed**, what is **refuted**, what is **latent** — the fix may only address the confirmed part (FR-002) |
| Story | the spec user story it belongs to (US2–US8) |
| Cluster | the research cluster (R1–R6) and the coupling group it lands with (see `research.md` "Coupling & order") |
| Kind | **fix** / **verify-only** (F-P1-03; jump-list half of F-P1-25) / **doc-only** (F-P5-06) / **conditional** (D02 under FR-012) |
| Disposition | **fixed-accepted** / **verify-closed** / **deferred** (+ justification citing FR-012 or an unresolved regression rejection) |
| Fix record | `X<nn>` (069 numbering continues after 068's X09: X10…) |

Rule (FR-001): every item ends with a disposition; every code change names
exactly one item.

## Fix record

| Field | Values / meaning |
|---|---|
| ID | `X<nn>` |
| Items | one in-scope item, or one coupling group the verdicts tie together (FR-007) |
| Chain | producer → intermediates → sink with `file:line` at HEAD, each hop's encoding **before** and **after**; every consumer of every changed symbol, enumerated by the fixer (FR-003) |
| Change | files and lines touched; helper(s) used per site; the legacy fallback branch (FR-004); any new helper (name, file, what it mirrors) |
| Not touched | the refuted/latent parts of the verdict; sites belonging to clusters B-1–B-5 (FR-002) |
| Affected surfaces | every surface/consumer the changed code reaches, each with the reviewer's verdict **unchanged** / **corrected** / **regressed** |
| Byte identity | English-UI/ASCII output identical to commit `64dcbb5` (evidence: reasoning per site — ASCII `LoadStrU8 == LoadStr`; W call on ASCII == A call — plus the on-screen English spot-check where the surface is in W1–W20); plugin-facing bytes identical (evidence: no `src/zip.cpp` forwarder behaviour change; `src/plugins/shared/*.h` diff comment-only; `LAST_VERSION_OF_SALAMANDER` unchanged) (FR-005) |
| Timing | required iff a per-item path is touched (FR-009): before/after on the 100,000-file fixture per the 068 quickstart method; result inside the baseline's [min, max] |
| Check | the `saltests` test, `tools/check_encoding.py` rule, or recorded manual scenario, with proof it **fails on the pre-fix tree and passes after** (FR-008) |
| Regression verdict | see below; a fix lands only when **ACCEPTED** (FR-006) |
| Changelog | entry text when user-visible (FR-013); "hygiene — no entry" or "verify-only — no entry" otherwise |

## Regression verdict

| Field | Values / meaning |
|---|---|
| Fix | `X<nn>` |
| Reviewer | an agent that did not write the fix, working from the diff and the fix record, charter = `specs/068-encoding-regression-review/charters.md` "Regression reviewer" |
| Consumers re-enumerated | the reviewer's own list; a consumer missing from the fix record is a defect of the record |
| Per-surface verdicts | **unchanged** (bytes/behaviour provably the same for every input that worked before) / **corrected** / **regressed** |
| Earlier scenarios touched | which 058/062/063/066/067/068 quickstart scenarios the change can alter, and whether it does |
| Result | **ACCEPTED** (zero regressed surfaces, record complete) / **REJECTED** (regressed surface or missing evidence named) |
| Record | `findings/regression-X<nn>.md` in this feature's directory |

## Check

| Field | Values / meaning |
|---|---|
| Kind | **unit** (`src/saltests/saltests.cpp` — only code linked into the test exe is reachable: `src/common/salclip.cpp`, `salfileio.cpp`, `salpath.cpp`, `salunicode.cpp`; the registry facade, UI and `gui.cpp` are not) / **guard** (`tools/check_encoding.py` strict rule) / **manual** (scenario in `quickstart.md`) |
| Proof | for unit and guard: run against the pre-fix tree (`git stash` or the pre-fix binary) → fails/flags; run after → passes/clean; counts recorded. For manual: result logged by the person who ran it |
| Baseline | `saltests` 1,257 checks / 0 failed; guard `TOTAL: 0` strict |

## Gate

| ID | What | Pass bar |
|---|---|---|
| G1 | `build.cmd full` (Debug x64) | 0 errors; no new warnings in files touched by this feature |
| G2 | `build.cmd full release` | same |
| G3 | `…\Debug_x64\saltests\saltests.exe` | `N checks, 0 failed`, N ≥ 1,257 + this feature's checks |
| G4 | `python tools\check_encoding.py --strict` | `TOTAL: 0`; every promoted/added rule proven to fire on a planted defect (FR-015) |
| G5 | Debug start/exit health | exit 0; no handle/heap boxes; no crash report; Trace Server capture once D01 makes `tserver` build (otherwise the 068 observable bar, waived with reason) |
| G6 | Timing (per-item paths only) | after-median within the baseline [min, max] |
| G7 | English spot-check | W1–W6, W13 + the English form of every new scenario identical to the pre-feature build |
| G8 | On-screen sweep (maintainer) | W1–W20 in cs and hu PASS; every per-fix scenario PASS |

Result values: **PASS** / **FAIL** / **WAIVED** (justification).

## Sweep item

| Field | Values |
|---|---|
| ID | `W<n>` (068's W1–W20) or `V<nn>` (this feature's per-fix scenario, numbered after its fix) |
| Surface / Language / Scenario | as in the 068 quickstart; new items give the fixture, the steps and the expected text or behaviour |
| Result | **PASS** / **FAIL** (→ back to fix → review → gates) / **WAIVED** |

## Deferred item

Same fields as 068's `Deferred item`; the only admissible justifications
here are **FR-012 (not plugin-local / would change plugin-facing bytes)** and
**regression review REJECTED and not resolvable without a systemic change**
(which routes the item to the relevant cluster B-1–B-5).

## Fixture

| ID | What | Where |
|---|---|---|
| FX-CS | Czech sweep folder `Můj disk\` with `příloha.txt`, `žluťoučký kůň.docx`, `1 000 000.pdf` (real NBSP), `Přehled.txt`, `poznámky.txt`, `Účtenka.pdf`, subfolder `Účetnictví\`, `Smlouva – kopie.docx` | `D:\Zkouška\` (recreate — absent on this machine as of 2026-08-24) |
| FX-HU | `Árvíztűrő tükörfúrógép\bájt.txt` | `D:\Zkouška\` |
| FX-SUR | one unpaired-surrogate name | `D:\Zkouška\surrogate\` |
| FX-PERF | 100,000 files | `%TEMP%\salamander-test\perf` (`tools\create-test-fixtures.ps1 -Perf`) |
| FX-TEMP | accented `%TEMP%` for one session | `set TEMP=D:\Zkouška\Dočasné` before launching the debug exe (no account change needed) |
| FX-INST | accented install path | copy of `Debug_x64\` under `D:\Zkouška\Tandém Commander\` (portable run) |
| FX-SUBST / FX-JUNC | `subst X: D:\Zkouška\Šablony`; `mklink /J D:\Zkouška\Data D:\Zkouška\Zálohy` | command prompt |
| FX-SHARE | local share `Účetnictví` on `D:\Zkouška\Účetnictví` | `net share` (admin) — optional, marker scenario only |
| FX-CLOUD | OneDrive under an accented path | only if available; otherwise the drive-menu scenario is verified by the unit-level check on the producers |
| FX-ARC | external archiver | 7-Zip (`C:\Program Files\7-Zip\7z.exe`) configured as a custom packer/unpacker in Options ▸ Archivers (`$(ListFile)` syntax); RAR not required |
| FX-CONV | Central-European conversion set | shipped `convert\centeuro\` — auto-selected on a CP1250 Windows |

## Closing record

`specs/069-finish-encoding-fixes/closing-report.md` (FR-014): the inventory
table with dispositions; the fix table (X10…) with regression verdicts,
checks and timing; the gate table; the sweep results; the changelog entries;
the updated handoff (`REMAINING-WORK.md` for this feature) listing anything
still open — expected to be exactly the B-1–B-5 clusters, D02 if not
plugin-local, and any item whose fix was rejected.

## State flow

```
in-scope item ─▶ chain traced at HEAD (research.md) ─▶ fix written (minimal, whole chain, legacy fallback)
      │                                                       │
      │                                          check written and proven fail-before
      │                                                       ▼
      │                                    independent regression review (charter)
      │                                        ACCEPTED ──▶ lands (own commit) ──▶ gates G1–G4 after every fix
      │                                        REJECTED ──▶ rework (≤ 2 rounds) ──▶ else deferred w/ reason
      ▼
verify-only ─▶ verified at HEAD ─▶ verify-closed          all fixes landed ─▶ G5–G7 ─▶ maintainer sweep G8 ─▶ closing record
```

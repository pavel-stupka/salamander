# Data Model: Encoding Regression Review and Stabilization

**Feature**: 068-encoding-regression-review · **Date**: 2026-08-24

The feature's data are review records. They live in two documents produced
during implementation — `inventory.md` (Boundaries and Sites; the audit's
raw material) and `review-report.md` (Findings, Fixes, Deferred items,
Gates, Sweep items, verdict; the deliverable) — plus the Phase-1
`contracts/encoding-contract-checklist.md` (Defect classes and Contract
obligations; the fixed reference the reviewers work from).

## Boundary

One of the eight kinds of place where text changes representation (spec
FR-001). Fixed set; the inventory is organized by it.

| ID | Boundary | Text carried | Typical crossing |
|---|---|---|---|
| B1 | Disk → application | names, paths, link targets | directory listing, attribute queries, reparse targets |
| B2 | Application → Windows | names, paths | file/shell/icon/overlay/change-monitoring/Recycle Bin/process-launch/drag-and-drop calls |
| B3 | Language module → screen | translated UI text | resource load → window/dialog/menu/list/tooltip sink |
| B4 | Composition | translated text + names/numbers/dates/plugin text | printf-family, plural expansion, number/date/time formatting |
| B5 | Application → external | any text | clipboard, generated file lists, logs, external programs, command lines |
| B6 | User input → application | user-typed text | rename, path, mask, command, search fields |
| B7 | Application ↔ saved configuration | paths, names, custom titles | registry facade in both directions |
| B8 | Application ↔ plugins | names, paths, plugin-supplied text, formatting services | plugin API in both directions |

## Site

One concrete place where a boundary is crossed (spec FR-001/FR-002).

| Field | Values / meaning |
|---|---|
| ID | `S-B<k>-<n>` |
| Location | `file:line` (function name when helpful) |
| Pattern | the API or house helper reached (e.g. `SetDlgItemText`, `LoadStr`+`sprintf`, `SalU8ToW`→`…W`) |
| Data | name / path / UI text / number / plugin text / user input |
| Classification | **verified-correct** / **defective** / **latent** (wrong only in a non-shipping configuration) / **out-of-scope** (reason mandatory) |
| Evidence | one line: why the classification holds (e.g. "ASCII-only in all 8 shipped translations", "sink is `Sal*U8`", "value is a drive letter") |
| Perspective | P-id that examined it |
| Defect class | `DC-…` when defective or latent |
| Finding | `F<n>` when defective |

Rules: a Site classified **defective** MUST have a Finding; **latent** MUST
name the non-shipping configuration (disabled language, unreachable
separator); **out-of-scope** MUST give the reason (vendored code, dead code,
developer tooling). Sites are grouped, not enumerated one by one, when they
are instances of one pattern in one function (e.g. "the 14 `DeleteFile`
calls in `cache.cpp` on cache temp paths") — the group carries one
classification and one evidence line, but every location is listed.

## Defect class

A recurring shape of encoding error (spec FR-003, Key Entities).

| Field | Values / meaning |
|---|---|
| ID | `DC-<slug>` |
| Shape | one sentence: what is composed with what / what is handed where |
| Identifying pattern | the code pattern a reviewer searches for |
| Origin features | the features that fixed instances |
| Guard | existing `check_encoding.py` rule / saltests test, or **none** |
| Sweep status | complete / partial (reason) |

Every DC listed in the contract checklist MUST reach sweep status
**complete** (SC-002). A DC confirmed during this review that has no Guard
MUST gain one (FR-010, SC-008).

## Contract obligation

One rule from a binding encoding contract (features 052, 058, 063, 066,
067), restated in the contract checklist with the sites it governs; each
gets a compliance verdict (**compliant** / **deviation → Finding**) with
evidence (spec FR-004).

## Perspective

An independent review angle with a written charter, a bounded target list
(boundaries, files, defect classes, seeded questions) and a coverage list
returned in its report. Perspectives only **raise** findings; they never
verdict their own.

## Finding

| Field | Values / meaning |
|---|---|
| ID | `F<n>` sequential |
| Raised by | Perspective |
| Site(s) | `S-…` |
| Defect class | `DC-…` |
| Failure scenario | **mandatory**: surface + locale/UI language + what the user sees (or the operation that fails) — no scenario, no finding |
| Verdict | **CONFIRMED** / **REFUTED**, by an independent verifier (not the raising perspective), with code evidence (`file:line`) |
| Disposition | **fixed** (→ Fix) / **deferred** (→ Deferred item) / **no-change** (REFUTED) |

Rules (FR-006/FR-007): REFUTED ⇒ no-change; CONFIRMED + shipping-relevant
+ in scope ⇒ fixed; CONFIRMED but plugin-internal, disabled-language-only,
vendored, developer-tooling, or non-encoding-and-not-trivial ⇒ deferred
with justification (FR-012, FR-015). Every code change in this feature
maps to exactly one CONFIRMED finding (SC-003).

## Fix

| Field | Values / meaning |
|---|---|
| ID | `X<n>` |
| Finding | `F<n>` |
| Change | files touched (minimal; no adjacent refactoring) |
| Affected surfaces | every surface/consumer the changed code reaches, each with a verdict **unchanged** / **corrected** / **regressed** |
| Regression review | **ACCEPTED** / **REJECTED**, by an independent reviewer (not the fix author) with a refute-first charter; REJECTED ⇒ the fix is reworked or withdrawn |
| Byte-identity | English-UI/ASCII output identical (evidence); plugin-facing output identical (evidence: no plugin-visible byte change, interface version unchanged) |
| Timing | required iff the change is on a per-item path: before/after on a ≥ 50,000-entry folder, method per quickstart; result within run-to-run noise |
| Check | the saltests test / `check_encoding.py` rule added or extended, with proof it **fails on pre-fix code and passes after**; or the recorded manual scenario when no automated check is possible |
| Changelog | entry text when user-visible (FR-014) |

Rules (FR-008/FR-009/FR-010): a Fix is accepted only with regression
review ACCEPTED, byte-identity shown, timing recorded when required, and
check proven. A Fix to shared conversion/formatting machinery additionally
re-runs the whole Sweep (US3).

## Deferred item

| Field | Values / meaning |
|---|---|
| ID | `D<n>` |
| Origin | earlier feature (ledger entry) or `F<n>` |
| Location | `file:line` |
| Description | one line |
| Encoding-related | yes / no |
| Disposition here | **fixed** (→ Fix, for ledger entries picked up) / **deferred again** |
| Justification | mandatory when deferred again |
| Recorded where | plugin follow-up list / language re-enable checklist / non-encoding follow-up list / vendored-code note |

Rule (FR-005): every entry of the consolidated ledger from earlier
features appears here with a fresh disposition — no implicit carry-over.

## Gate

| Field | Values |
|---|---|
| ID | `G<n>` (roster in research.md) |
| Evidence | command output tail, counts, run record |
| Result | **PASS** / **FAIL** / **WAIVED** (justification) |

## Sweep item

| Field | Values |
|---|---|
| ID | `W<n>` |
| Surface | one entry of the US3 sweep list |
| Language | cs / hu / en-spot-check |
| Scenario | what to do, what to look at (quickstart) |
| Result | **PASS** / **FAIL** (→ Finding) / **WAIVED** (justification) |

## Review report

The single record (spec US4/FR-013): scope, method and perspectives with
their coverage lists; inventory summary per Boundary (counts per
classification); Defect-class sweep table; Contract compliance table;
Findings table; Fixes table; Deferred items table (ledger re-dispositions
first); Gate table; Sweep table; stability verdict.

## State flow

```
Phase A  inventory: per Boundary, Sites classified by Perspectives (parallel, read-only)
         + Defect-class sibling sweeps + Contract compliance + Ledger re-examination
            → findings pool (each with failure scenario)
Phase B  independent verification (verifier ≠ raiser, refute-first)
            → CONFIRMED → scope test (FR-012/FR-015) → fix candidate
            → REFUTED  → no-change, recorded
Phase C  fix (minimal) → independent regression review (reviewer ≠ author)
            → affected-surface verdicts, byte-identity, timing (per-item paths),
              fail-before/pass-after check
            → ACCEPTED → merged; REJECTED → rework or withdraw (→ deferred)
         shared-machinery fix ⇒ full Sweep re-run
Phase D  Gates G1..Gn + Sweep W1..Wn (cs, hu, en) → report → stability verdict
```

Bounded re-verification: a Fix that touches already-classified Sites
re-opens exactly those Sites (and their Perspective's check) plus the
affected Gates — never the whole inventory.

# Feature Specification: Pre-Release Stabilization Review (Features 058 + 059)

**Feature Branch**: `060-prerelease-stabilization`
**Created**: 2026-08-18
**Status**: Draft
**Input**: User description: "Cílem aktuálního kroku je provést detailní analýzu celého kódu a ověřit především dvě poslední feature 58 a 59, zdali jsou implementovány bezpečně, tedy, zdali do projektu nezanesly regresní chyby a detailně ověřit, že je celý kód stabilní. Cílem není nový vývoj ani rozvoj, pouze detailní analýza, verifikace a případná oprava nalezených potenciálních problémů - tedy stabilizace nové verze před vydánímm."

## Problem Statement

Since release 0.1.2 (build 186), two features landed that both touch
long-lived, concurrency-sensitive core machinery: **058** (three encoding
fixes in the icon-reading pipeline, the directory-change snooper, and the
shell icon lookup) and **059** (a new cloud sync-state fallback with a
synthetic overlay entry, a per-listing sync-root gate, and a new resource).
Feature 059's integration already exposed one latent upstream defect (an
uninitialized variable that crashed the application at startup), which is
direct evidence that this area of the code rewards adversarial scrutiny.

Before these changes ship in a release, the project needs the same
discipline applied before 0.1.2 (feature 056): an independent, multi-angle
review of everything that changed, adversarial verification of every
finding so only real defects drive changes, fixes for what is confirmed,
and a documented go/no-go record. **This step is explicitly not development
work** — no new capabilities, no refactoring beyond what a confirmed defect
requires; its only output is higher confidence (and, where needed, targeted
fixes) plus the release-readiness record.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Independent deep review of the 058+059 delta (Priority: P1)

The maintainer receives an independent, multi-perspective review of every
change made since release 0.1.2 — the encoding conversions, the
change-monitoring calls, the icon/overlay pipeline additions, the new
synthetic overlay entry and its lifecycle, the sync-root gating, the shell
property query, and the new resource/asset — examined from complementary
angles (memory and object lifecycles, concurrency and thread affinity,
error/failure paths, encoding correctness, performance regressions,
security exposure of the new external-data surfaces). Every raised finding
is adversarially verified against the actual code before it is accepted;
only confirmed findings lead to changes.

**Why this priority**: This is the review the user asked for — the delta is
where new risk lives, and the one latent bug already found there proves the
ground is fertile.

**Independent Test**: The review record lists every perspective, every raised
finding with its verification verdict (confirmed / refuted, with the code
evidence), and for each confirmed finding either a fix or an explicit,
justified deferral.

**Acceptance Scenarios**:

1. **Given** the complete code delta since release 0.1.2, **When** the review
   completes, **Then** every changed line has been examined by at least the
   perspectives relevant to it, and the record shows which perspective
   examined what.
2. **Given** a raised finding, **When** it is verified against the code,
   **Then** the record shows the verdict with evidence, and refuted findings
   lead to no code change.
3. **Given** a confirmed defect, **When** it is fixed, **Then** the fix is
   minimal (no drive-by refactoring) and covered by a re-run of the relevant
   verification.

---

### User Story 2 - Whole-product stability verification (Priority: P1)

The maintainer gets evidence that the product as a whole is stable with the
new changes in: clean full builds of both configurations, the complete unit
test suite passing, startup/shutdown health (no crash reports, no handle or
memory leak reports from the debug instrumentation), and re-confirmation
that the behavior validated in features 058 and 059 still holds after any
stabilization fixes.

**Why this priority**: "The code is stable" is the release claim; it needs
gates, not impressions. Equal priority to US1 because both are release
blockers.

**Independent Test**: A gate table in the release record, every row green or
explicitly waived with justification: Debug + Release full builds, unit
tests at baseline, debug-instrumentation health at startup/shutdown, and the
058/059 validated scenarios unchanged.

**Acceptance Scenarios**:

1. **Given** the stabilized code, **When** the full build and test gates run,
   **Then** all pass with zero errors and no new warnings in changed files.
2. **Given** any stabilization fix landed after the original 058/059
   validations, **When** the affected validated scenario is re-checked,
   **Then** the previously confirmed behavior still holds.
3. **Given** the debug build's leak/handle instrumentation, **When** the
   application is started and exited normally, **Then** no new leak or
   invalid-handle reports appear relative to the pre-058 baseline.

---

### User Story 3 - Release-readiness record (Priority: P2)

The maintainer gets a single written record — the review report — stating
what was examined and how, every finding with its verdict and disposition,
the gate results, known deferred items with their justification, and an
explicit go/no-go conclusion for releasing the current state.

**Why this priority**: The record is what makes the exercise repeatable and
auditable (as `specs/056-prerelease-review/review-report.md` did for 0.1.2);
it depends on US1/US2 content, hence P2.

**Independent Test**: The report exists, covers all perspectives and gates,
and a reader can trace every code change made during stabilization back to a
confirmed finding.

**Acceptance Scenarios**:

1. **Given** the finished review, **When** the report is read, **Then** it
   contains the delta scope, the perspectives applied, all findings with
   verdicts, the gate table, deferrals with reasons, and the go/no-go
   verdict.

---

### Edge Cases

- **Findings that are real but out of the delta's scope** (pre-existing
  upstream defects uncovered while reading): recorded with evidence and
  classified — fixed only when they are release-relevant (crash, data loss,
  regression trigger); otherwise deferred explicitly, following the
  precedent of feature 056's deferred items.
- **Plausible-but-wrong findings**: the adversarial verification step exists
  precisely to kill these; a finding without a concrete failure scenario
  reproducible in the code does not drive a change.
- **A confirmed fix that itself changes reviewed code**: the affected
  perspective's review and the affected gates re-run on the fix (bounded
  re-verification, not a full restart).
- **Gates that cannot run to completion** (e.g., an environment-dependent
  manual scenario): recorded as waived with justification, never silently
  skipped.
- **No confirmed findings at all**: a legitimate outcome — the record then
  documents the clean verdict; absence of fixes is not absence of results.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Every change since release 0.1.2 (the features 058 and 059
  delta, including assets and build-affecting files) MUST be reviewed by
  independent perspectives covering at least: memory/object lifecycles,
  concurrency and thread affinity, error and failure paths, encoding
  correctness, performance, and the security of newly consumed external
  data surfaces.
- **FR-002**: Every raised finding MUST be adversarially verified against
  the actual code before acceptance; the record MUST show the verdict and
  evidence for each. Refuted findings MUST NOT lead to code changes.
- **FR-003**: Every confirmed, release-relevant defect MUST be fixed with a
  minimal change, and each fix MUST be traceable to its finding in the
  record. No change may land during this feature without a confirmed
  finding behind it (no new development, no opportunistic refactoring).
- **FR-004**: Pre-existing defects discovered outside the delta MUST be
  recorded and classified: fixed if release-relevant, otherwise explicitly
  deferred with justification.
- **FR-005**: The stability gates MUST run and pass (or be explicitly waived
  with justification): full Debug and Release builds with zero errors and no
  new warnings in changed files; the complete unit test suite at its
  baseline; debug-instrumentation health (no new leak/handle reports) over a
  normal start/exit cycle; and re-confirmation of the 058/059 validated
  scenarios for any behavior a stabilization fix could have affected.
- **FR-006**: The review report MUST exist as a single document recording
  scope, method, perspectives, findings with verdicts and dispositions, gate
  results, deferrals, and an explicit go/no-go release verdict.
- **FR-007**: The exercise MUST NOT change user-visible behavior except
  where a confirmed defect requires it; any such behavioral fix MUST be
  called out in the report (and in the changelog if user-visible).

### Key Entities

- **Delta**: the complete set of changes between release 0.1.2 and the
  current state (features 058, 059 — code, resources, tooling, docs).
- **Finding**: a suspected defect raised by a review perspective; carries a
  failure scenario, a verification verdict (confirmed/refuted) with
  evidence, and a disposition (fixed / deferred / no change needed).
- **Perspective**: an independent review angle with a defined charter (e.g.,
  concurrency); a finding's credibility comes from verification, not from
  the perspective that raised it.
- **Gate**: a pass/fail stability check with recorded evidence; the set of
  gates plus the findings' dispositions produce the go/no-go verdict.
- **Review report**: the single release-readiness record (US3).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of changed files in the delta are covered by at least one
  relevant review perspective, and the report can show which.
- **SC-002**: 100% of raised findings carry a recorded verification verdict
  with evidence; zero code changes trace to refuted or unverified findings.
- **SC-003**: Zero confirmed release-relevant defects remain unfixed at the
  end; every deferred item carries a written justification.
- **SC-004**: All stability gates green: both full builds clean with no new
  warnings in changed files, unit tests at baseline (currently 1145 passing,
  0 failing), no new debug-instrumentation reports over a start/exit cycle,
  and previously validated 058/059 behavior re-confirmed where affected.
- **SC-005**: The review report exists and a reader can trace every code
  change made in this feature to a confirmed finding (spot-check: any 3
  randomly chosen changes).
- **SC-006**: No user-visible behavior change beyond confirmed-defect fixes
  (verified by re-running the 058/059 validation scenarios).

## Assumptions

- "Detailní analýza celého kódu" is interpreted the way feature 056
  interpreted it: **deep, line-level review of the whole delta since the
  last release**, plus product-wide stability verification through gates and
  targeted review of the code the delta interacts with. A line-level review
  of all ~2,200 pre-existing source files is neither feasible nor what
  stabilizes this release; pre-existing code enters scope where the delta
  touches or depends on it (and via findings, per FR-004).
- The baseline for "no regressions" is release 0.1.2 behavior plus the
  user-validated outcomes of features 058 and 059 (their `evidence.md`
  records).
- Review perspectives follow the feature-056 precedent (independent angles +
  adversarial verification); the exact roster is a planning decision.
- Manual GUI scenarios re-run only where a stabilization fix could affect
  them; unaffected validated scenarios stand on their existing evidence.
- Release itself (version bump, changelog finalization, signing, installer)
  is out of scope — this feature produces the go/no-go input for it.

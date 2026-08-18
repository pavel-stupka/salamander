# Data Model: Pre-Release Stabilization Review

**Feature**: 060-prerelease-stabilization · **Date**: 2026-08-18

The feature's data are review records, all living in `review-report.md`.

## Finding

| Field | Values / meaning |
|---|---|
| ID | `F<n>` sequential |
| Perspective | P1–P6 (research.md R3) which raised it |
| Location | file:line(s) in the delta (or outside, flagged per FR-004) |
| Claim | one-sentence defect statement |
| Failure scenario | concrete input/state → wrong outcome (mandatory — no scenario, no finding) |
| Verdict | CONFIRMED / REFUTED, with code evidence cited |
| Disposition | fixed (commit-traceable) / deferred (justification) / no-change (refuted) |

Rules: REFUTED ⇒ disposition no-change (FR-002); CONFIRMED + release-
relevant ⇒ fixed (FR-003/SC-003); CONFIRMED outside delta ⇒ classified per
FR-004. Every code change in this feature maps to exactly one CONFIRMED
finding (SC-005).

## Gate

| Field | Values |
|---|---|
| ID | G1–G7 (research.md R5) |
| Evidence | log/tail, counts, or run record |
| Result | PASS / FAIL / WAIVED(justification) |

Release verdict requires: all gates PASS or WAIVED-with-justification AND
zero CONFIRMED release-relevant findings undisposed.

## Perspective

Charter text (fixed in research.md R3), file list, and completion mark —
SC-001 is proven by the per-perspective coverage list in the report.

## State flow

```
scoping (done, R1) → perspectives run (parallel, read-only)
   → findings pool → adversarial verification (each)
      → CONFIRMED → fix (minimal) → bounded re-verify + affected gates
      → REFUTED  → recorded, no change
   → gates G1–G7 → report + go/no-go
```

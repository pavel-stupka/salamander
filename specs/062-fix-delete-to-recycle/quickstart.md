# Quickstart: Validating Consistent Delete to Recycle Bin

**Feature**: 062-fix-delete-to-recycle | **Date**: 2026-08-19
Validation guide for SC-001…SC-007. Invariants: `data-model.md`; binding behavior:
`contracts/delete-pipeline-contract.md`; method rationale: `research.md` R6.

## Prerequisites

- The reporting machine (ACP 1250; `E:` fixed NTFS with enabled Recycle Bin; OneDrive
  tree `E:\OneDrive - Simopt, s.r.o` with Czech-named subfolders).
- Debug build (`build.cmd`), optionally with local `TRACE_TO_FILE` for the
  instrumented runs (revert before commit, as in feature 061).
- Test files are created fresh per run; nothing is deleted outside dedicated test
  folders. **The Recycle Bin check is mandatory after every DEL scenario** — enumerate
  the shell Recycle Bin (COM `Shell.Application`, namespace 0x0A) and match items by
  original path.

## The four canonical locations (used by V2–V4)

| L | Location | Today (pre-fix) |
|---|---|---|
| L1 | ASCII disk folder, e.g. `D:\Temp\tc062\plain` | recycle ✔ |
| L2 | non-ASCII disk folder, e.g. `D:\Temp\tc062\Zkouška` | popup + permanent ✘ (proves not-OneDrive) |
| L3 | ASCII OneDrive folder, e.g. `E:\OneDrive - Simopt, s.r.o\tc062test` | recycle ✔ (predicted) |
| L4 | non-ASCII OneDrive folder, e.g. `…\tc062test\Zkouška` | popup + permanent ✘ (user's repro) |

## V1 — Phase A instrumented confirmation (FR-001)

1. On the **unfixed** build with the gate TRACE (R5), press DEL in L2/L4: the trace
   must show the classified type ≠ fixed (expected `DRIVE_NO_ROOT_DIR`) — this
   confirms the one open runtime assumption (research.md R0) before the fix is
   trusted.
2. On the fixed build, repeat: all four locations classify `DRIVE_FIXED`.
3. Record both in `analysis-report.md`.

## V2 — DEL to Recycle Bin, mode "all" (US1; SC-001)

For each of L1–L4: create test files (incl. one with diacritics in the *name*),
delete via the DEL command path, then verify: no confirmation popup appeared (mode 1),
files are absent from the folder, **present in the Recycle Bin with original paths**,
and restorable. 4/4 locations identical.

## V3 — SHIFT+DEL permanent (US2; SC-002)

1. In each location: SHIFT+DEL → confirmation prompt → file permanently gone, **not**
   in the Recycle Bin. (Automation may cover the inversion equivalently via mode 0 +
   DEL — same worker inputs — plus at least one interactive SHIFT+DEL smoke.)
2. Mode 0 ("delete directly") + SHIFT+DEL → files go to the Recycle Bin (inversion).

## V4 — Masks mode (US4; SC-003)

Set mode 2 with masks `*.txt`. In L2 and L4 delete `zkouška.txt` + `zkouška.bin`:
the `.txt` lands in the bin, the `.bin` is deleted directly after the masks-mode
prompt. Repeat in L1 for the ASCII baseline. Restore the user's original mode
(`Use Recycle Bin = 1`) afterwards.

## V5 — Cloud placeholder folder delete (US3; SC-004)

1. Create a subfolder with files in the OneDrive tree, let it sync (placeholder
   state), then DEL → lands in the Recycle Bin.
2. Second copy, SHIFT+DEL → deleted permanently, **no "directory link" error**.
3. Explorer parity: delete a third copy in File Explorer, compare outcomes.
4. Genuine-link protection: create a junction (`mklink /J`) to a folder with a canary
   file; delete the junction in TC → junction gone, target + canary intact.

## V6 — Bin-less locations unchanged (FR-008; SC-007)

On a network share (UNC) or removable drive: DEL → configured confirmation → direct
delete (no bin), unchanged from today.

## V7 — Regression guard & gates (FR-010; SC-006)

1. Copy/Move smoke across L1–L4 (shared classification chain): copy + move a file in
   each location, incl. across a junction; results byte-identical to pre-fix.
2. Feature 061 overlay smoke (TortoiseGit badges still show; cloud badges intact).
3. `build.cmd rebuild` (Debug), `build.cmd full release`, saltests — 0 new failures.
4. **Final-build matrix re-run** (SC-005): repeat V2–V5 on the cleaned final build and
   record in `analysis-report.md`.

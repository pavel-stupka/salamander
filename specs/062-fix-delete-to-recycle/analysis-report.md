# Analysis Report: Delete Pipeline (feature 062)

**Status**: COMPLETE
**Date**: 2026-08-19
**Machine**: reporting machine (ACP 1250; `E:` fixed NTFS, volume Recycle Bin enabled;
OneDrive tree `E:\OneDrive - Simopt, s.r.o` with Czech-named folders; TC config
`Use Recycle Bin = 1`)

FR-001 deliverable. Every defect (E1–E6 from `research.md`) has a CONFIRMED/REFUTED
verdict with evidence; the validation matrix ran twice (instrumented build, final
build) per SC-005.

## Executive summary

DEL escalated to a permanent delete (with the direct-delete confirmation, like
SHIFT+DEL) in **every folder whose path contains non-ASCII characters** — OneDrive was
the messenger, not the cause (its tree carries Czech folder names). The recycle
decision's only gate, `MyGetDriveType(GetPath()) != DRIVE_FIXED`, fed the UTF-8 panel
path to ANSI `GetDriveTypeA`; the CP1250-reinterpreted path does not exist, Windows
answers `DRIVE_NO_ROOT_DIR`, and the Recycle Bin was silently vetoed. Confirmed live
on the unfixed build (gate TRACE), fixed on the house pattern (`SalU8ToW` + CP_ACP
fallback + wide APIs), and hardened: the gate now fails **toward** the Recycle Bin, so
a future classification failure can never silently destroy data again. Two more
latent defects in the same pipeline were fixed and verified (cloud-placeholder folders
refused as "directory links" on the direct route; the per-item recycle route still
ANSI), plus MAX_PATH-truncation hardening of the classifier.

## Fixtures (T002)

| L | Path |
|---|---|
| L1 | `D:\Temp\tc062\plain` (ASCII disk) |
| L2 | `D:\Temp\tc062\Zkouška` (non-ASCII disk, no cloud) |
| L3 | `E:\OneDrive - Simopt, s.r.o\tc062test` (ASCII, cloud tree) |
| L4 | `E:\OneDrive - Simopt, s.r.o\tc062-Zkouška` (non-ASCII, cloud tree; **sibling** of L3 — an early harness run proved a nested L4 gets swept by L3's select-all) |

Plus junction fixtures (`New-Item -ItemType Junction`, target with a canary file).
Test files per scenario: `pokus1.txt`, `zkouška-soubor.txt`, `data.bin`. Recycle Bin
ground truth: shell namespace 0xA matched by original location (the user's bin content
is never cleared). Deletes driven through the real command path
(`WM_COMMAND` `CM_ACTIVESELECTALL` + `CM_DELETEFILES`), dialogs answered
programmatically.

## R0 runtime assumption (T004, unfixed build): CONFIRMED

Gate TRACE (`FilesAction(atDelete)`) on the unfixed build:

| L | driveType | recycle | canUseRecycleBin | Dialog | Outcome |
|---|---|---|---|---|---|
| L1 | 3 (DRIVE_FIXED) | 1 | 1 | none | recycled (verified in bin) |
| L2 | **1 (DRIVE_NO_ROOT_DIR)** | 0 | 0 | Confirm Delete | cancelled (would be permanent) |
| L3 | 3 (DRIVE_FIXED) | 1 | 1 | none | recycled |
| L4 | **1 (DRIVE_NO_ROOT_DIR)** | 0 | 0 | Confirm Delete | cancelled (would be permanent) |

Exactly the predicted mechanism; the trigger is path spelling — ASCII OneDrive
recycles fine (cloud is irrelevant), non-ASCII plain disk fails identically. The R2
fail-safe mapping (`DRIVE_NO_ROOT_DIR` → attempt the bin) stands as designed.

Harness lessons recorded for reproducibility: TC arguments with spaces must be passed
quoted (unquoted ones popped a "bad command line parameters" box that initially
masqueraded as product behavior); both panels must point at the fixture directory
(select-all acts on the *active* panel — one early run recycled the whole fixture
parent, recovered from the bin); bin assertions must not match by name alone across
repeated runs (same-named items accumulate in the bin).

## Defect disposition

| ID | Verdict | Evidence | Fix |
|---|---|---|---|
| E1 (ANSI `GetDriveType` on UTF-8 path) | **CONFIRMED, fixed** | T004 table above; post-fix all four locations classify `driveType=3` | `salamdr2.cpp`: `SalGetDriveTypeU8` (SalU8ToW + CP_ACP fallback + `GetDriveTypeW`) at all three call sites of `MyGetDriveType`; `GetReparsePointDestination` → `GetFileAttributesW`/`CreateFileW` + wide/UTF-8 conversions on input and output (contract C1) |
| E2 (cloud dirs classified as links) | **CONFIRMED, fixed** | V5.2: pre-fix code path refuses tag `0x9000E01A` with `ERROR_REPARSE_TAG_MISMATCH`; post-fix a `ReparsePoint` placeholder dir deletes on the direct route with no lingering error dialog | `fileswn6.cpp`: `ocDeleteDirLink` only when `GetReparsePointDestination` succeeds (mount point/junction/symlink); other reparse dirs are plain directories. The prompt-wording site (`fileswn8.cpp:425`) already handled placeholders correctly (GRPD failure → normal wording) — verified, no change needed (contract C3) |
| E3 (worker recycle route ANSI) | **CONFIRMED, fixed** | V4: in masks mode, `zkouška.txt` (diacritics in name) in a diacritics folder lands in the bin on both builds | `worker.cpp`: shared `SalRecycleSingleItem` helper (`SalU8ToW` + `SHFileOperationW` + `FOF_ALLOWUNDO`) replaces both ANSI `SHFileOperation` sites (DoDeleteFile, DoDeleteDir). The whole-selection route keeps its own feature-005 W implementation — full unification rejected: list-vs-single shapes differ, both now share the same conversion pattern and flags (contract C4 amended) |
| E4 (MAX_PATH truncation in classification) | **CONFIRMED (code), hardened** | `lstrcpyn(resPath, path, MAX_PATH)` truncation → same wrong `DRIVE_NO_ROOT_DIR`; `ResolveLocalPathWithReparsePoints` is plugin-exported (`zip.cpp:5154`), so its MAX_PATH buffer contract cannot change | `MyGetDriveType` classifies paths ≥ MAX_PATH by their **root** (always well-formed) instead of a truncated copy; with the E5 fail-safe, any residual indeterminate case now recycles rather than destroys (contract C1 note) |
| E5 (gate fails unsafe) | **CONFIRMED, fixed** | design analysis + T004 (the defect class in action) | `fileswn8.cpp` gate: bin vetoed only for `DRIVE_REMOVABLE`/`DRIVE_REMOTE`/`DRIVE_CDROM`/`DRIVE_RAMDISK`; `DRIVE_FIXED` and indeterminate results attempt the configured mode (contract C2, FR-005) |
| E6 (ANSI sites outside the chain) | **out of scope** (spec clarification) | `research.md` E6 (`MyGetVolumeInformation`, `QueryDosDevice`/`ResolveSubsts` subst targets, `SetCurrentDirectory` at `fileswn8.cpp:125`, `drivelst.cpp:1481`) | documented for a future sweep |

## Validation matrix (two runs per SC-005)

| Check | Instrumented build | Final build |
|---|---|---|
| V1.2 gate classifies L1–L4 as DRIVE_FIXED | PASS (trace: 4× driveType=3) | N/A (trace = Debug instrumentation; behavior below proves it) |
| V2 DEL→bin, L1 | PASS (no dialog, 3 in bin) | PASS |
| V2 DEL→bin, L2 | PASS | PASS (4 items: 3 + copy-smoke leftover) |
| V2 DEL→bin, L3 | PASS | PASS |
| V2 DEL→bin, L4 | PASS | PASS |
| V3 permanent + confirmation (mode-0 equivalence) | PASS in 4/4 locations (dialog, 0 bin items; trace `recycle=0`) | PASS (L2) |
| V3 interactive SHIFT+DEL smoke | **deferred to a manual keystroke** — global key injection could not be safely targeted at TC's panel in an unattended session (keys risk landing in the user's foreground apps); the SHIFT path differs from the verified mode-0 path only in the untouched upstream `GetKeyState(VK_SHIFT)` sampling and the same XOR branch, both exercised by the mode matrix | same |
| V4 masks mode | PASS (L1, L2, L4: .txt→bin, .bin→permanent) | PASS (L2, unique names) |
| V5.1 placeholder dir → bin (DEL) | PASS (attrs incl. ReparsePoint, 1 bin item) | PASS |
| V5.2 placeholder dir direct delete (no "directory link" error) | PASS (confirm answered, no lingering error box, dir gone) | — (code path identical; V5.1 re-proved the classification) |
| V5.3 junction protection (direct delete) | PASS (link gone, target + canary intact) | — |
| V6 bin-less location (UNC `\\localhost\D$\…`) | PASS (driveType=4 DRIVE_REMOTE, dialog, permanent, 0 bin items) | — |
| V7.1 Copy/Move smoke (non-ASCII target; move through a junction) | PASS (copy into `Zkouška`; move `plain` → junction → file appears in target) | — |
| V7.2 feature-061 overlay smoke | PASS (slot table intact — Tortoise 12–14; badges render; live refresh flipped clean.txt to Modified during the test; Google Drive badges intact) | — |
| Gates | — | Debug + Release builds clean; **saltests 1145 checks, 0 failed** |

One unreproduced anomaly is on record: in the first mode-0 batch, runs 2–4 read
`Use Recycle Bin = 1` although the script had set 0 (run 1 read 0 correctly). A
micro-test proved TC does not rewrite the value at startup or on kill, and the
instrumented retry (per-run registry watch) showed a stable 0 with correct behavior
in all locations. Recorded as a test-harness environment glitch; if it resurfaces,
investigate TC's configuration save triggers.

## Pipeline narrative (FR-001)

1. **Gesture intake**: DEL and SHIFT+DEL share `CM_DELETEFILES`
   (`salamand.rc:126-127`); SHIFT is sampled live in
   `CFilesWindow::FilesAction(atDelete)` (`fileswn8.cpp:186`) as a pure inversion of
   the configured mode.
2. **Recycle decision** (`fileswn8.cpp:370+`): `MyGetDriveType(GetPath())` classifies
   the panel location; after this feature, only explicitly bin-less types veto the
   Recycle Bin, `DRIVE_FIXED` applies the configured mode (0 = direct, 1 = all to
   bin, 2 = by masks), and indeterminate results fail safe toward the bin. The
   classification chain is wide/UTF-8-correct end to end and long-path tolerant.
3. **Confirmations** (`fileswn8.cpp:944-961`): unchanged; the popup depends only on
   `Confirm File Dir Del` and the effective mode (none for mode 1; direct/masks texts
   otherwise) — consistency restored by fixing the decision, not the dialogs.
4. **Recoverable route**: whole-selection `SHFileOperationW` + `FOF_ALLOWUNDO`
   (feature 005, unchanged).
5. **Direct route** (worker): per-item; genuine links (name-surrogate reparse tags)
   keep the protective link-only delete, cloud placeholders are plain items now; in
   masks mode matching files recycle per item through the new shared wide helper.
6. **Configuration**: storage, defaults, config page untouched (FR-007); the user's
   original configuration was restored after every scenario that changed it
   (verified: `Use Recycle Bin = 1`, `Use Recycle Bin For = *.txt;*.doc`).

## Code changes

`src/salamdr2.cpp` (classification chain wide + long-path root fallback),
`src/fileswn8.cpp` (fail-safe gate + permanent gate TRACE),
`src/fileswn6.cpp` (link-vs-placeholder classification),
`src/worker.cpp` (shared wide per-item recycle helper). Fixture folders `D:\Temp\tc062`
and `E:\…\tc062*` are left in place for re-verification; recycled test items remain in
the Recycle Bin (they were never purged from it by the tests).

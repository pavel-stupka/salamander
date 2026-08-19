# Phase 0 Research: Consistent Delete to Recycle Bin

**Feature**: 062-fix-delete-to-recycle | **Date**: 2026-08-19
**Inputs**: `spec.md` (incl. Clarifications), `investigation-leads.md` (code + registry +
environment exploration), direct verification of the key code sites this session.

## R0 — Verified facts (code + environment)

- **Decision gate** (`src/fileswn8.cpp:370-391`): for delete, the *only* recycle gate is
  `MyGetDriveType(GetPath()) != DRIVE_FIXED` ⇒ `recycle = 0; canUseRecycleBin = FALSE`.
  With the user's mode (`UseRecycleBin == 1`, "for all"), a popup + permanent delete can
  only come from this gate misfiring. Confirmation popup appears only when
  `Configuration.CnfrmFileDirDel && recycle != 1` (`fileswn8.cpp:944-961`) — the user's
  observation ("popup, then gone for good") pins the defect to the gate.
- **`MyGetDriveType`** (`src/salamdr2.cpp:1670-1700`, read directly this session): builds
  the root (`GetRootPath`), checks `GetDriveType` (ANSI — the project defines no
  `UNICODE`); for fixed disks calls `ResolveLocalPathWithReparsePoints`, which leaves
  `ourPath` = the **full panel path** when no junction/symlink is found (cloud reparse
  tags are rejected by `GetReparsePointDestination`, `salamdr2.cpp:1520-1528`); then
  loops `GetDriveType(ourPath)` retrying **only on `DRIVE_UNKNOWN`**. Panel paths are
  UTF-8 (feature 004) — on ACP 1250 a path containing e.g. `š` mojibakes to a
  non-existent path.
- **Open runtime assumption** (the one step not observed yet): `GetDriveTypeA` on the
  mojibake path returns `DRIVE_NO_ROOT_DIR` (not `DRIVE_UNKNOWN`), so the loop exits
  without cutting back to the root. Documented semantics support this; Phase A verifies
  it with one TRACE before any fix is trusted.
- **SHIFT+DEL** is a pure inversion sampled at `fileswn8.cpp:186`; `CanUseRecycleBin`
  is a hard veto SHIFT cannot re-enable ⇒ on affected paths DEL ≡ SHIFT+DEL — matches
  the report exactly.
- **Environment** (reporting machine): `E:` fixed NTFS, volume Recycle Bin enabled
  (`NukeOnDelete=0`); OneDrive tree = cloud reparse points with Czech folder names;
  ACP 1250; TC config `Use Recycle Bin = 1` (masks inert, default masks string).
- **The recycle executor is already W-correct** (`DeleteThroughRecycleBinAuxW`,
  `fileswn8.cpp:74-158`, `SHFileOperationW` + `FOF_ALLOWUNDO`, feature 005) — the fix
  does not touch its mechanics, only who reaches it.
- The permanent executor `SalDeleteFile` is W-correct too — which is why the defect
  destroys data instead of erroring.

## R1 — Decision: fix the whole classification chain on the house pattern (Clarification 1)

**Decision**: Convert `MyGetDriveType` and its reparse chain —
`ResolveLocalPathWithReparsePoints`, `GetCurrentLocalReparsePoint`,
`GetReparsePointDestination` and the ANSI/CP_ACP companions inside them
(`GetFileAttributes` `salamdr2.cpp:1502`, `CreateFile` `:1509`,
`MultiByteToWideChar(CP_ACP)` `:1589`, `WideCharToMultiByte(CP_ACP)` `:1611`) — to the
feature-058 house pattern: UTF-8 in/out at the existing `char*` signatures, `SalU8ToW`/
`SalWToU8` (+ CP_ACP fallback) internally, wide WinAPI calls (`GetDriveTypeW`,
`GetFileAttributesW`, `CreateFileW`). Internal buffers sized for long paths
(`SAL_MAX_PATH_UTF8` / wide equivalents), removing the silent `MAX_PATH` truncation
(defect #3 in `investigation-leads.md`).

**Rationale**: one cohesive unit in one file, one defect class, per the spec
clarification; signatures unchanged ⇒ every caller (Copy/Move link resolution, drive
bar) benefits without call-site edits; FR-010 regression scenarios cover them.

**Alternatives considered**: minimal gate-only conversion — rejected by clarification
(leaves the same silent defect in link-following); repository-wide ANSI sweep —
rejected (out of scope, stabilization risk).

## R2 — Decision: fail-safe gate semantics (Clarification 2, FR-005)

**Decision**: The `fileswn8.cpp:370-391` gate stops requiring `DRIVE_FIXED` and instead
disables the Recycle Bin only for **explicitly bin-less** classifications:
`DRIVE_REMOVABLE`, `DRIVE_REMOTE`, `DRIVE_CDROM`, `DRIVE_RAMDISK`. `DRIVE_FIXED`
enables it; indeterminate results (`DRIVE_UNKNOWN`, `DRIVE_NO_ROOT_DIR`) now **attempt
the Recycle Bin route** — the system operation reports visibly if recycling is truly
impossible. This is the belt-and-braces change that makes the 062 defect class
structurally unrepeatable.

**Rationale**: spec clarification (asymmetry of harm); `SHFileOperationW` fails loudly,
never silently, when a location cannot recycle.

**Alternatives considered**: keeping `!= DRIVE_FIXED ⇒ direct` with only the encoding
fix — rejected (any future classification failure would again silently destroy data);
blocking error dialog — rejected by clarification.

## R3 — Decision: cloud-placeholder directories are not links (defect #1)

**Decision**: The direct-route script builder (`fileswn6.cpp:1700-1722`) and the delete
prompt wording site (`fileswn8.cpp:420-432`) classify a reparse directory as a *link*
(`ocDeleteDirLink`) only when its reparse tag is a **name surrogate**
(`IsReparseTagNameSurrogate` — true for mount points and symlinks, false for cloud
tags like `0x9000701A`). Non-surrogate reparse directories are treated as normal
directories (recursed and deleted). The worker's existing tag check
(`DoDeleteDirLinkAux`, `worker.cpp:7121-7127`) stays as the protective backstop for
genuine links.

**Rationale**: restores delete of OneDrive folders on the direct route (US3) without
weakening the "delete the link, not the target" protection (FR-006); the tag is already
available where the classification happens (directory enumeration provides it).

**Alternatives considered**: whitelisting cloud tags — rejected (future tags would
regress again); treating all reparse dirs as normal — rejected (would recurse into
junction targets: data loss).

## R4 — Decision: the worker's recycle call goes wide (defect #2)

**Decision**: `worker.cpp:6300-6323` (file) and `:6932-6965` (directory) switch from
ANSI `SHFileOperation` to the wide pattern already used by
`DeleteThroughRecycleBinAuxW` (UTF-8 → `SalU8ToW`, double-NUL list, `SHFileOperationW`
+ `FOF_ALLOWUNDO`), factored into a shared helper so both call sites and
`fileswn8.cpp` use one implementation.

**Rationale**: this route is live in masks mode (`UseRecycleBin == 2`) and under
SHIFT-inversion from mode 0; with non-ASCII names it currently corrupts the list —
US4/SC-003 cannot pass without it.

**Alternatives considered**: leaving it and documenting — rejected (US4 in scope by
spec; identical defect class).

## R5 — Decision: diagnostics (FR-009)

**Decision**: one `TRACE_I` at the gate logging the classified drive type, the
effective `recycle` mode and `canUseRecycleBin` — Debug-only (TRACE compiles away),
consistent with the feature-061 clarification precedent. It also serves Phase A as the
instrument that confirms the open runtime assumption in R0.

## R6 — Verification method (FR-001, SC-005; the feature-061 playbook)

- **Phase A instrumented runs** (Debug + local `TRACE_TO_FILE`): confirm the
  `DRIVE_NO_ROOT_DIR` assumption on the unfixed build, then the fixed classification,
  over the discriminating matrix from `investigation-leads.md` (ASCII disk / non-ASCII
  disk / ASCII OneDrive root / non-ASCII OneDrive subfolder).
- **Recycle Bin ground truth**: after each scripted delete, enumerate the shell
  Recycle Bin namespace (COM `Shell.Application`, folder 0x0A) and match items by
  original path; restorability = the item exists there with its original location.
- **Gesture automation**: deletes are driven by posting the delete command to the main
  window (same code path as DEL) after focusing the target item; SHIFT+DEL semantics
  are additionally covered by the mode-0 + DEL equivalence (`InvertRecycleBin` XOR —
  same worker inputs) plus at least one interactive SHIFT+DEL smoke with `SendInput`.
  Dialog automation: the direct-route confirmation is answered programmatically; the
  recycle route must show no popup (its absence is part of the assertion).
- **Matrix re-run on the final build** (user's "verify repeatedly"): the whole
  SC-001…SC-004 matrix is executed once on the instrumented build and re-run on the
  cleaned final build; both runs recorded in `analysis-report.md`.
- Explorer parity for SC-004: delete the same placeholder folder in Explorer and
  compare outcomes.

## Defect register (running; final disposition in analysis-report.md)

| ID | Status | Description | Fix decision |
|---|---|---|---|
| E1 | Verified (code; 1 runtime assumption open) | ANSI `GetDriveType` on UTF-8 panel path ⇒ non-ASCII paths lose the Recycle Bin (popup + permanent delete) | R1 + R2 |
| E2 | Verified (code) | Cloud-placeholder dirs classified as links ⇒ direct-route delete fails with "directory link" error | R3 |
| E3 | Verified (code) | Worker recycle route still ANSI `SHFileOperation` ⇒ non-ASCII names break in masks/inverted modes | R4 |
| E4 | Verified (code) | `MyGetDriveType` MAX_PATH truncation ⇒ long paths lose the bin the same way | R1 (buffer widening) |
| E5 | Verified (code) | Gate fails *unsafe* (unknown ⇒ permanent) | R2 |
| E6 | Noted, out of scope | ANSI sites outside the chain (`MyGetVolumeInformation`, `QueryDosDevice`, `SetCurrentDirectory` at `fileswn8.cpp:125`, `drivelst.cpp:1481`) | documented for a future sweep; not touched (Clarification 1) |

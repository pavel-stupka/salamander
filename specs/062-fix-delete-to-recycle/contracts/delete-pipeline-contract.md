# Contract: Delete Pipeline (Recycle Decision & Routes)

**Feature**: 062-fix-delete-to-recycle | **Date**: 2026-08-19
**Style**: internal behavioral contract, same genre as feature 058's path-encoding
contract and feature 061's overlay-pipeline contract. Binding for this feature's
implementation and future changes to the delete flow.

## C1 — Location classification

1. `MyGetDriveType` (and its reparse-resolution chain:
   `ResolveLocalPathWithReparsePoints`, `GetCurrentLocalReparsePoint`,
   `GetReparsePointDestination`) accepts UTF-8 paths of panel length
   (`SAL_MAX_PATH_UTF8`), converts internally via `SalU8ToW` (CP_ACP fallback) and
   calls wide WinAPI. No `MAX_PATH` truncation anywhere in the chain.
2. Signatures stay `char*`/UTF-8 — all existing consumers (delete gate, Copy/Move
   link resolution, drive bar) keep working without call-site changes.
3. The classification of an ASCII path shorter than MAX_PATH is byte-identical to the
   pre-fix behavior (regression bar for FR-010).

## C2 — Recycle decision (fail-safe)

1. The Recycle Bin is vetoed (`canUseRecycleBin = FALSE`, `recycle = 0`) **only** for
   explicitly bin-less classifications: `DRIVE_REMOVABLE`, `DRIVE_REMOTE`,
   `DRIVE_CDROM`, `DRIVE_RAMDISK`.
2. `DRIVE_FIXED` enables the configured mode. Indeterminate classifications
   (`DRIVE_UNKNOWN`, `DRIVE_NO_ROOT_DIR`) attempt the Recycle Bin route; a genuine
   inability to recycle is reported visibly by the system operation. **A
   classification failure must never silently produce a permanent delete** (FR-005).
3. SHIFT stays a pure inversion of the configured mode; it can never re-enable a
   vetoed bin (unchanged semantics).
4. Which confirmation popup appears depends only on `Configuration.CnfrmFileDirDel`
   and the effective `recycle` value — never on path spelling, depth, or cloud state.

## C3 — Item nature on the direct route

1. A directory (or file) with `FILE_ATTRIBUTE_REPARSE_POINT` is treated as a *link*
   only when its reparse tag is a name surrogate (`IsReparseTagNameSurrogate`:
   mount points, symlinks). Links keep the protective behavior: the link is removed,
   its target untouched.
2. Non-surrogate reparse items (cloud placeholders, present and future provider tags)
   are treated as plain files/directories.
3. The worker's tag verification before deleting a link stays as the backstop.

## C4 — Recycle executors

1. Every path handed to the shell recycle operation is wide, produced from UTF-8 via
   `SalU8ToW` (CP_ACP fallback): the whole-selection route (existing
   `DeleteThroughRecycleBinAuxW`) and the worker's per-item route (this feature) share
   one helper.
2. `FOF_ALLOWUNDO` semantics, progress, and error surfaces of the shell operation are
   unchanged.

## C5 — Diagnostics (Debug builds only)

1. One TRACE at the decision gate: classified drive type, effective `recycle`,
   `canUseRecycleBin`.
2. No Release-visible diagnostic surface (no UI, no log file, no new strings).

## Out of scope (guarded, not changed)

- Delete mode configuration storage/format/UI (`Use Recycle Bin`,
  `Use Recycle Bin For`, `Confirm File Dir Del`) — FR-007.
- Behavior on genuinely bin-less locations (removable/network/optical) — FR-008.
- ANSI sites outside the classification chain (`MyGetVolumeInformation`,
  `QueryDosDevice`, `SetCurrentDirectory` in `fileswn8.cpp:125`, `drivelst.cpp:1481`)
  — documented in `research.md` E6 for a future sweep.

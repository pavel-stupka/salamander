# Data Model: Consistent Delete to Recycle Bin

**Feature**: 062-fix-delete-to-recycle | **Date**: 2026-08-19
**Scope**: entities of the delete pipeline as they exist in code, with the invariants
this feature relies on or changes. No configuration moves.

## E1 — Delete operation intent

Produced per gesture in `CFilesWindow::FilesAction(atDelete, …)` (`src/fileswn8.cpp`).

| Field | Meaning | Constraints |
|---|---|---|
| `invertRecycleBin` | SHIFT held at command time (`fileswn8.cpp:186`) | pure inversion of the configured mode; sampled once per operation |
| `recycle` | effective mode for this operation: 0 = direct, 1 = all to bin, 2 = by masks | derived from `Configuration.UseRecycleBin` ⊕ inversion, gated by E2 |
| `canUseRecycleBin` | hard veto (location cannot recycle) | **changed by this feature**: set FALSE only for explicitly bin-less drive types (removable/remote/CD-ROM/RAM); indeterminate ⇒ TRUE (fail-safe, FR-005/R2) |

## E2 — Location classification (`MyGetDriveType`, `src/salamdr2.cpp:1670-1700`)

Input: UTF-8 panel path (`SAL_MAX_PATH_UTF8`). Output: `DRIVE_*` type of the volume
actually hosting the path (following substs, mount points, symlinks).

**Contract changed by this feature (R1)**: internally wide (`GetDriveTypeW` and the
reparse chain on `SalU8ToW` + CP_ACP fallback), long-path capable (no MAX_PATH
truncation), signatures unchanged (UTF-8 `char*`). Shared consumers: delete gate,
Copy/Move link resolution, drive bar — behavior for ASCII short paths byte-identical.

## E3 — Item nature (direct-route classification, `fileswn6.cpp:1700-1722`)

| Nature | Detection | Direct-route handling |
|---|---|---|
| plain file | no reparse attr | per-file delete (+ masks decision in mode 2) |
| plain directory | no reparse attr | recurse + delete |
| genuine link (junction/symlink) | reparse attr **and name-surrogate tag** (changed: tag check added, R3) | delete the link only; target untouched |
| cloud placeholder (file or dir) | reparse attr, non-surrogate tag | **changed (R3)**: treated as plain file/directory |

Worker backstop unchanged: `DoDeleteDirLinkAux` still refuses non-link tags — after R3
it can only receive genuine links.

## E4 — Delete mode configuration (unchanged; `cfgdlg.h:321-322`)

| Value (HKCU\…\Configuration) | Meaning |
|---|---|
| `Use Recycle Bin` (DWORD 0/1/2) | 0 = never, 1 = always, 2 = by masks; default 1 |
| `Use Recycle Bin For` (SZ) | masks list; default `*.txt;*.doc` |
| `Confirm File Dir Del` (DWORD) | operation confirmation; default TRUE |

Storage, defaults, config page (`CCfgPageSystem`) — all unchanged (FR-007).

## E5 — Deletion routes

- **Recoverable route**: `DeleteThroughRecycleBin` → `SHFileOperationW` +
  `FOF_ALLOWUNDO` (whole selection at once; W-correct since feature 005). Used when
  `recycle == 1` and `canUseRecycleBin`.
- **Direct route**: script build (`BuildScriptMain`) → worker thread → per item
  `DoDeleteFile`/`DoDeleteDir`/`DoDeleteDirLink`; in mode 2 each file re-decides via
  masks and recycles individually — **changed (R4)**: that per-item recycle call goes
  wide, shared helper with the recoverable route.
- Confirmation popup: only `CnfrmFileDirDel && recycle != 1` (`fileswn8.cpp:944-961`);
  text per mode (`:454-467`). Unchanged — consistency is restored by fixing E2, not by
  changing dialogs (FR-004).

## Relationships

```text
gesture (DEL / SHIFT+DEL) ──> E1 intent ──uses──> E2 location classification (fixed: wide, fail-safe)
                                   │
                     recycle==1 && canUseRecycleBin
                    ┌──────────────┴──────────────┐
            recoverable route                direct route (worker)
        (SHFileOperationW+ALLOWUNDO)   per item: E3 nature (fixed: surrogate-tag)
                                        mode 2: masks → per-item recycle (fixed: wide)
```

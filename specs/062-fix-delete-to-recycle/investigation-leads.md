# Investigation Leads: Delete Pipeline (feature 062)

**Purpose**: technical exploration notes gathered while writing the spec — *input for
the plan/analysis phase*, not a design record. Every claim below must be confirmed or
refuted with runtime evidence during implementation (the feature-061 method).

**Gathered**: 2026-08-19, read-only code exploration + registry/environment probes on
the reporting machine (branch point: 061-fix-icon-overlays @ d3aecdf). Line numbers
valid as of that revision.

## Primary suspect (code-verified; one runtime assumption open)

**The DEL/SHIFT+DEL inconsistency is not OneDrive- or cloud-specific. It is a
feature-004 regression-by-omission of the same family as features 058/059/061-D1:
the UTF-8 panel path is fed to ANSI `GetDriveTypeA` inside `MyGetDriveType()`.**

- Decision gate (`src/fileswn8.cpp:370-391`): for `atDelete`,
  `if (MyGetDriveType(GetPath()) != DRIVE_FIXED) { recycle = 0; canUseRecycleBin = FALSE; }`
  — the **only** gate; no `SHQueryRecycleBin`, no `$RECYCLE.BIN` probe, no UNC/MAX_PATH
  check.
- `MyGetDriveType` (`src/salamdr2.cpp:1670-1700`): root check `GetDriveType("E:\")`
  passes (DRIVE_FIXED), but then `ResolveLocalPathWithReparsePoints` leaves `ourPath`
  = the **full panel path** (OneDrive dirs carry cloud reparse tags `0x9000701A`/
  `0x9000E01A`, which `GetReparsePointDestination` rejects — only mount points and
  symlinks are followed, `salamdr2.cpp:1520-1528`), and `GetDriveTypeA` is called on
  it. On CP1250, UTF-8 `š` (`C5 A1`) mojibakes to a non-existent path.
- The retry loop only cuts the path back on `DRIVE_UNKNOWN` (0); a non-existent path
  returns `DRIVE_NO_ROOT_DIR` (1) → immediate exit, wrong verdict. **Open runtime
  assumption**: `GetDriveTypeA` returns `DRIVE_NO_ROOT_DIR` (not `DRIVE_UNKNOWN`) for
  the mojibake path — one `TRACE_I` at `fileswn8.cpp:374` settles it.
- `Path` is UTF-8 `char[SAL_MAX_PATH_UTF8]` (`src/fileswnd.h:479`); the project defines
  no `UNICODE` (verified) → `GetDriveType` = `GetDriveTypeA`. Machine ACP = 1250.
- The permanent-delete executor `SalDeleteFile` **is** UTF-8-correct
  (`src/common/salfileio.cpp:247-250`, `DeleteFileW`) — which is why the file really
  disappears instead of erroring.

### Discriminating test matrix (predictions to verify)

| Panel path | Prediction |
|---|---|
| `D:\...` ASCII plain disk | recycle, no popup ✔ |
| `E:\OneDrive - Simopt, s.r.o` (ASCII root of the cloud tree) | recycle, no popup — cloud is irrelevant |
| `E:\OneDrive - Simopt, s.r.o\TC-Test-Zkouška` | popup + permanent ✘ |
| `D:\...\Zkouška` (plain disk, non-ASCII, no cloud) | popup + permanent ✘ ← proves not-OneDrive |
| ASCII dir, non-ASCII *file* name | recycle ✔ (why feature 005's `SHFileOperationW` fix tested green) |

## Pipeline map (verified)

| Step | Location |
|---|---|
| DEL / SHIFT+DEL accelerator (same command) | `src/salamand.rc:126-127` (`CM_DELETEFILES`; F8 pair `:88-89`) |
| Command handler | `src/mainwnd3.cpp:3361-3401` → `FilesAction(atDelete, …)` |
| Decision site | `src/fileswn8.cpp:177` (`FilesAction`), gate `:370-391` |
| SHIFT sampling | `fileswn8.cpp:186` `GetKeyState(VK_SHIFT)` → pure inversion `:381-387`; `CanUseRecycleBin` is a hard veto SHIFT cannot re-enable (`worker.h:253`) — on affected paths DEL ≡ SHIFT+DEL, matching the report |
| Recycle branch | `fileswn8.cpp:981-993` → `DeleteThroughRecycleBin()` (`:74-158`, `SHFileOperationW` + `FOF_ALLOWUNDO`, feature 005) |
| Direct branch | `fileswn8.cpp:1059-1067` → `BuildScriptMain()` (`fileswn6.cpp`) → worker |
| Worker executors | `worker.cpp:8187-8217` → `DoDeleteFile` `:6203`, `DoDeleteDir` `:6914`, `DoDeleteDirLink` `:7179`; per-file mask decision `:6252-6292`, recycle call `:6300-6323`, permanent `:6327` |
| Confirmation dialogs | `fileswn8.cpp:944-961`: popup only when `Configuration.CnfrmFileDirDel && recycle != 1`; text per mode `:454-467` (`IDS_CONFIRM_DELETE` for direct, `IDS_CONFIRM_DELETE2` for masks mode, none for recycle-all) → "popup appeared" ⇒ direct-delete branch, pinning the bug at the gate |

## Configuration surface (verified)

- `Configuration.UseRecycleBin` (`cfgdlg.h:321`): **0 = don't use, 1 = for all,
  2 = per RecycleMasks**; default 1 (`dialogs4.cpp:278`); registry
  `"Use Recycle Bin"` (`mainwnd2.cpp:207`, save `:1572`, load `:3082`).
- `Configuration.RecycleMasks` (`cfgdlg.h:322`): default `"*.txt;*.doc"`
  (`dialogs4.cpp:282`); registry `"Use Recycle Bin For"` (`mainwnd2.cpp:208`).
- Config UI: `CCfgPageSystem`, radios `IDR_RECYCLE1/2/3` + `IDE_RECYCLEMASKS`
  (`dialogs4.cpp:3312-3348`).
- **This machine**: `Use Recycle Bin = 1` (for all → masks inert),
  `Use Recycle Bin For = *.txt;*.doc` (the built-in default, not a migration artifact),
  `Confirm File Dir Del` absent (default TRUE). Earlier session hypothesis that the
  masks caused the symptom is **REFUTED** — mode 1 ignores masks.

## Environment facts (verified, read-only)

- `E:` is DriveType 3/Fixed, NTFS; Recycle Bin enabled for the volume
  (`BitBucket\Volume\{95b9d14a-…}` `NukeOnDelete=0`, `E:\$RECYCLE.BIN` exists; no
  restrictive policies) → "shell refused to recycle" is ruled out.
- OneDrive tree: every folder is a cloud reparse point (`0x431` attrs, tags above);
  files are `Archive|ReparsePoint` placeholders; longest path 100 chars → MAX_PATH
  truncation ruled out *for this repro*.

## Additional defects found in the same pipeline (each needs a verdict + decision)

1. **Cloud-placeholder directories misclassified as links**: `fileswn6.cpp:1700-1722`
   treats any `FILE_ATTRIBUTE_REPARSE_POINT` directory as `ocDeleteDirLink`;
   `DoDeleteDirLinkAux` (`worker.cpp:7121-7127`) rejects non-mount-point/symlink tags
   with `ERROR_REPARSE_TAG_MISMATCH` → deleting any OneDrive folder via the
   non-recycle path fails with a confusing "error deleting directory link"
   (`IDS_ERRORDELETINGDIRLINK`). Same misclassification wording at
   `fileswn8.cpp:420-432`.
2. **Worker's recycle call is still ANSI**: `worker.cpp:6322` and `:6958` use
   `SHFileOperation` (A) with UTF-8 names (feature 005 fixed only
   `DeleteThroughRecycleBinAuxW` in `fileswn8.cpp`). Live whenever
   `UseRecycleBin == 2`, or SHIFT+DEL with `UseRecycleBin == 0`.
3. **`MyGetDriveType` is MAX_PATH-bound** (`salamdr2.cpp:1672-1674`) while panel paths
   are `SAL_MAX_PATH_UTF8` — truncation yields the same wrong `DRIVE_NO_ROOT_DIR`
   verdict for ≥260-byte paths; no spec ever swept this function.
4. **Companion ANSI/CP_ACP gaps in the same call chain** (all receive UTF-8):
   `GetFileAttributes` `salamdr2.cpp:1502`, `CreateFile` `:1509`,
   `WideCharToMultiByte(CP_ACP)` `:1611`, `MultiByteToWideChar(CP_ACP)` `:1589`,
   `MyGetVolumeInformation` → `GetVolumeInformation` `:1456`, `QueryDosDevice` `:1713`;
   `SetCurrentDirectory(GetPath())` (A) `fileswn8.cpp:125`; `ConvertU2A` of the
   OneDrive path `drivelst.cpp:1481`. The chain also serves Copy/Move and the drive
   bar — scope decision for the plan.
5. **UNC branch of `MyGetDriveType`** has the same ANSI flaw (`salamdr2.cpp:1698`).

## Fix sketch (for the plan to confirm)

Minimal: `MyGetDriveType` → house pattern (`SalU8ToW` + CP_ACP fallback, cf.
`snooper.cpp:578-586` from feature 058) + `GetDriveTypeW`; widen the local buffers
beyond MAX_PATH; treat the same chain (`ResolveLocalPathWithReparsePoints`,
`GetCurrentLocalReparsePoint`, `GetReparsePointDestination`) consistently. Defensive
belt: at the `fileswn8.cpp:374` gate, fail *safe toward the Recycle Bin* — only
explicitly known bin-less types (removable/CD-ROM/remote/RAM/unknown) disable it, so a
classification failure can never silently escalate to permanent deletion.

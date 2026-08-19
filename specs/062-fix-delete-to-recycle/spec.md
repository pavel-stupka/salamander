# Feature Specification: Consistent Delete to Recycle Bin

**Feature Branch**: `062-fix-delete-to-recycle`
**Created**: 2026-08-19
**Status**: Draft
**Input**: User description: "Detailně a hloubkově prověř chování aplikace při mazání souborů po stisknutí klávesy DEL resp. zvolení položky 'Odstranit' z kontextové nabídky. Chování napříč adresáři není konzistentní. V některých adresářích na disku funguje správně, tedy soubor, resp. skupina souborů se rovnou odstraní — tj. přesune správně do koše. Ale v některých adresářích — např. na připojeném OneDrive — se při stisknutí DEL zobrazí pop-up okno pro potvrzení smazání a při potvrzení se soubor rovnou smaže — tedy stejně jako při stisknutí SHIFT+DEL. Toto je z principu chyba, chování by mělo být konzistentní. Identifikuj příčinu a navrhni opravu, kterou následně implementuj. Vše detailně a několikrát prověřuj."

## Problem Statement

Deleting files with the DEL key (or the Delete command) behaves differently depending
on which folder the panel shows. In most folders the selection goes straight to the
Windows Recycle Bin — silently, recoverably, as configured. But in some folders —
reproducibly inside the user's OneDrive tree — the same DEL press first shows a
"Confirm Delete" popup and then **permanently deletes** the files, indistinguishable
from SHIFT+DEL. A file manager silently escalating a recoverable delete into a
permanent one is a data-loss class defect: the user's muscle memory says "DEL is safe",
and in the affected folders it is not.

The application must be analyzed end to end for the delete flow (command → decision
whether the Recycle Bin applies → confirmation dialogs → the actual deletion), the
cause identified with evidence, fixed, and the fix verified repeatedly — across folder
locations, path spellings, drive types, cloud-synced folders, both delete gestures, and
all three configured recycle-bin modes (always / never / by file masks). The fix must
not introduce regressions.

Preliminary code exploration (recorded in `investigation-leads.md`, to be confirmed or
refuted with runtime evidence during implementation) indicates the trigger is **not**
OneDrive itself but the characters in the folder path, and found additional latent
defects in the same delete pipeline (cloud-synced folders failing to delete with a
confusing "directory link" error; one deletion route still mishandling non-ASCII names;
very long paths degrading the same way).

## Clarifications

### Session 2026-08-19

- Q: How deep should the fix go in the shared location-classification chain — only
  the drive-type function feeding the recycle decision, or also the follow-on
  reparse-point (junction/symlink) resolution machinery that the same chain provides
  to Copy/Move and the drive bar? → A: **The whole chain** — the classification
  function and its reparse-resolution helpers are converted together (one cohesive
  unit, one defect class, one house pattern); Copy/Move and the drive bar are covered
  by regression scenarios (FR-010). A broader sweep of unrelated ANSI sites beyond
  this chain stays out of scope.
- Q: When the location cannot be classified (drive type undeterminable) even after
  the fix, what exactly should DEL do in the "delete to Recycle Bin" mode? → A:
  **Attempt the Recycle Bin route.** Under uncertainty the recoverable route is used;
  if recycling is genuinely impossible on that location, the system operation reports
  the failure visibly. Never a silent permanent delete, and no blocking error dialog
  ahead of the attempt.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - DEL always respects the Recycle Bin (Priority: P1)

A user presses DEL (or picks Delete from the context menu) on files or folders. With
the default configuration ("delete to Recycle Bin"), the selection lands in the
Recycle Bin and is restorable — in **every** folder the panel can display the same
way: plain disk folders, folders with non-ASCII characters (diacritics) anywhere in
the path, and OneDrive/cloud-synced folders alike.

**Why this priority**: This is the reported defect and a silent-data-loss risk. The
whole value of the feature is that DEL is uniformly safe.

**Independent Test**: Delete an identical set of test files via DEL in four locations
(ASCII disk folder, non-ASCII disk folder, ASCII cloud folder, non-ASCII cloud folder)
and verify every file is in the Recycle Bin and restorable, with identical dialog
behavior in all four.

**Acceptance Scenarios**:

1. **Given** the "delete to Recycle Bin" mode and a file in a folder whose path
   contains diacritics (e.g. `…\Zkouška`), **When** the user presses DEL and confirms
   whatever prompt is configured, **Then** the file is in the Recycle Bin and
   restorable — not permanently deleted.
2. **Given** the same file in an ASCII-named folder on the same drive, **When**
   deleted the same way, **Then** the outcome and the sequence of dialogs are
   identical to scenario 1.
3. **Given** a folder inside a OneDrive sync tree, **When** files are deleted with
   DEL, **Then** they land in the Recycle Bin exactly as from any other folder on
   that drive (cloud sync state must not change the outcome).
4. **Given** any internal failure to classify the folder's location, **Then** the
   deletion attempts the Recycle Bin route (per Clarifications) — never a silent
   escalation to permanent deletion; a genuine inability to recycle is reported
   visibly by the operation.

---

### User Story 2 - SHIFT+DEL stays the explicit permanent delete (Priority: P2)

A user presses SHIFT+DEL to delete permanently on purpose. The confirmation prompt
appears and the files bypass the Recycle Bin — again identically in every folder. The
inversion meaning of SHIFT (permanent when the mode is "recycle", recycle when the
mode is "delete directly") keeps its current semantics.

**Why this priority**: The gesture pair is the contract this defect broke; fixing DEL
must not distort its intentional counterpart.

**Independent Test**: SHIFT+DEL of a test file in the same four locations as US1 —
prompt shown, file not in the Recycle Bin, in 4 of 4 locations.

**Acceptance Scenarios**:

1. **Given** any of the four US1 locations, **When** SHIFT+DEL is used, **Then** the
   confirmation prompt appears and the file is permanently deleted (not in the bin).
2. **Given** the "delete directly" configured mode, **When** SHIFT+DEL is used,
   **Then** files go to the Recycle Bin (inversion preserved).

---

### User Story 3 - Cloud-synced folders can be deleted (Priority: P2)

A user deletes a whole folder inside the OneDrive tree. It is removed (to the Recycle
Bin per mode), regardless of the folder being a cloud placeholder — no confusing
"cannot delete directory link"-style failure, and no route that only works when files
happen to be downloaded.

**Why this priority**: Exploration indicates cloud-synced folders can hit a delete
route that refuses them outright; the user explicitly asked for deep verification of
deletion in OneDrive folders.

**Independent Test**: Delete a placeholder subfolder in the OneDrive tree via DEL and
via SHIFT+DEL; both succeed with behavior matching the same operation in File Explorer.

**Acceptance Scenarios**:

1. **Given** a cloud-placeholder folder with placeholder files, **When** deleted with
   DEL, **Then** it lands in the Recycle Bin like any local folder.
2. **Given** the same folder, **When** deleted with SHIFT+DEL, **Then** it is removed
   permanently without spurious "link"/reparse errors, and genuine links
   (junctions/symlinks) keep today's protective behavior (link is removed, its target
   is untouched).

---

### User Story 4 - The masks mode works uniformly (Priority: P3)

A user configures "delete to Recycle Bin only for files matching masks". Matching
files go to the bin, non-matching files are deleted directly (after the configured
prompt) — with names and paths in any script, in any folder.

**Why this priority**: The masks mode shares the affected machinery and one of its
routes is suspected to mishandle non-ASCII names; it is the least-used mode, hence P3.

**Independent Test**: In masks mode with `*.txt`, delete `zkouška.txt` and
`zkouška.bin` in a non-ASCII folder: the `.txt` lands in the bin, the `.bin` is
deleted directly; same result in an ASCII folder.

**Acceptance Scenarios**:

1. **Given** masks mode and a mask-matching file with diacritics in its name inside a
   diacritics-named folder, **When** deleted with DEL, **Then** it is in the Recycle
   Bin and restorable.
2. **Given** a non-matching file, **When** deleted with DEL, **Then** the masks-mode
   confirmation appears and the file is deleted directly — same as today on ASCII
   paths.

---

### Edge Cases

- Folders on drives that genuinely have no Recycle Bin (removable media, network
  shares, CD-ROM): direct delete with confirmation stays — that is correct behavior,
  not part of the defect; the fix must not pretend a bin exists there.
- A volume whose Recycle Bin the user disabled in Windows ("remove files
  immediately"): behavior must match File Explorer's on that volume.
- Paths at or beyond the classic Windows path-length limit: the location
  classification must not silently degrade to "no Recycle Bin" just because the path
  is long; if the platform cannot recycle such a path, the failure must be explicit,
  not a silent permanent delete.
- Network (UNC) folders with non-ASCII characters in server/share/folder names must
  classify the same as their ASCII equivalents (i.e. as network, not as an error).
- Mixed selections (files + folders + genuine links + cloud placeholders) in one
  DEL: each item handled per its own nature; a genuine junction/symlink keeps the
  existing safe "remove link only" handling.
- The per-operation confirmations configured by the user (confirm on delete, confirm
  on non-empty directory, system/hidden file prompts) keep firing exactly as
  configured — the fix changes *where files go*, never *what is asked*.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The feature MUST produce a written end-to-end analysis of the delete
  flow — gesture/command intake, the recycle-vs-direct decision, confirmation
  dialogs, and both deletion routes — with the root cause of the reported
  inconsistency identified with runtime evidence, every candidate cause from the
  preliminary exploration explicitly confirmed or refuted, and the verification steps
  repeated on the fixed build. The analysis is a reviewable artifact in this
  feature's directory.
- **FR-002**: With the "delete to Recycle Bin" mode, DEL and the Delete command MUST
  send the selection to the Recycle Bin in every folder of every fixed local drive
  the panel can display — regardless of characters in the path, path depth, cloud
  sync state, or how the folder is reached — with identical dialog sequences for
  identical configurations.
- **FR-003**: The SHIFT modifier MUST keep its exact current meaning (inversion of
  the configured mode) everywhere; the confirmation prompt for permanent deletion
  MUST keep appearing per the user's confirmation settings.
- **FR-004**: Which confirmation popup appears MUST depend only on the user's
  configuration and the effective delete mode — never on the folder's location,
  path spelling, or cloud state.
- **FR-005**: A failure to classify a location (drive type, volume capabilities)
  MUST NOT silently escalate a recoverable delete into a permanent one; per
  Clarifications, the system attempts the Recycle Bin route, and if recycling is
  genuinely impossible there, the failure is reported visibly by the operation
  itself.
- **FR-006**: Deleting cloud-placeholder folders and files MUST work with both
  gestures; genuine links (junctions/symlinks) MUST keep the existing protective
  "remove the link, not the target" behavior, and items that are neither MUST NOT be
  treated as links.
- **FR-007**: All three configured modes (always to bin / never / by masks) and the
  masks list MUST keep their semantics, their storage location and format, and their
  configuration page unchanged.
- **FR-008**: Behavior on locations without Recycle Bin support (removable, network,
  optical) MUST remain as today: direct delete with the configured confirmation.
- **FR-009**: The recycle-vs-direct decision MUST be discoverable in the project's
  standard debug diagnostics (the classified location and the chosen route), with no
  new Release-visible diagnostic surface.
- **FR-010**: The change MUST NOT regress any behavior delivered by features 058,
  059 and 061 (cloud badges, icons, auto-refresh, overlay badges), nor the Copy/Move
  operations that share location-classification machinery with the delete flow.

### Key Entities

- **Delete gesture**: DEL / Delete command (configured mode) vs. SHIFT+DEL (inverted
  mode); both funnel into one decision about the destination of the deleted items.
- **Recycle decision**: the per-operation classification "can this location's items
  go to the Recycle Bin, and does the configured mode want that?" — the defect lives
  here; its inputs are the panel folder, the drive's nature, and the configured mode.
- **Delete mode configuration**: always / never / by masks + the masks list;
  persisted user choice; unchanged by this feature.
- **Deletion routes**: the recoverable route (system-managed move to the Recycle Bin)
  and the direct route (permanent removal, item by item, with per-item prompts).
- **Item nature**: plain file/folder, genuine link (junction/symlink), cloud
  placeholder — each with defined handling on the direct route.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Deleting an identical test set with DEL ("to Recycle Bin" mode) in the
  four canonical locations — ASCII disk folder, non-ASCII disk folder, ASCII
  OneDrive folder, non-ASCII OneDrive folder — puts 100% of the items into the
  Recycle Bin, all restorable, with identical dialog sequences in all four (today:
  two of the four locations permanently delete).
- **SC-002**: SHIFT+DEL in the same four locations permanently deletes with the
  confirmation prompt in 4 of 4 locations; with the "delete directly" mode the
  inversion still routes to the Recycle Bin.
- **SC-003**: In masks mode, matching files land in the bin and non-matching files
  are deleted directly in 4 of 4 locations, including names with diacritics.
- **SC-004**: A cloud-placeholder folder deletes successfully with both gestures,
  matching File Explorer's outcome on the same folder; genuine junctions/symlinks
  still delete as link-only (target intact) — 0 spurious "link" errors on
  placeholders.
- **SC-005**: The analysis report gives every candidate cause a CONFIRMED or REFUTED
  verdict with evidence, and the full verification matrix (SC-001…SC-004) is recorded
  as re-run on the final build — no candidate left unresolved.
- **SC-006**: Full existing verification suite (Debug and Release builds, automated
  tests) passes with 0 new failures.
- **SC-007**: Behavior on a location without Recycle Bin support (e.g. a network
  share) is unchanged: direct delete with the configured confirmation.

## Assumptions

- Reference behavior is File Explorer on the same machine for "where do deleted items
  end up" per location; the application's own configured confirmation dialogs are the
  product's design and are not being changed, only their consistency across locations.
- "Consistent" means: for the same configuration and gesture, the destination of
  deleted items and the dialog sequence depend only on the drive's actual
  capabilities (fixed disk with a bin vs. media without one), never on path spelling,
  folder depth, or cloud state.
- The acceptance machine is the reporting machine (OneDrive tree with Czech folder
  names on a fixed NTFS drive with an enabled Recycle Bin; system code page CP1250).
- The scope is the delete flow plus the **entire** location-classification chain it
  uses — the drive-type function together with its reparse-point resolution helpers
  (per Clarifications) — shared with Copy/Move and the drive bar, which therefore get
  regression coverage per FR-010; a repository-wide encoding sweep of ANSI call
  sites outside this chain is out of scope.
- Safety-first bias is intended and explicit (FR-005): under uncertainty the system
  must prefer the recoverable route, because the asymmetry of harm (permanent loss
  vs. an extra Recycle Bin entry) is absolute.
- Preliminary code exploration with concrete suspects and a discriminating test
  matrix is recorded in `investigation-leads.md` as input for the planning phase.

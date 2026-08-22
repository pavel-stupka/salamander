# Feature Specification: Fix File Operations on Names with Unpaired Surrogates

**Feature Branch**: `066-fix-surrogate-filenames`
**Created**: 2026-08-22
**Status**: Draft
**Input**: User description: "Tandem Commander neumi smazat, presunout ani kopirovat soubor ./temp/fixtures-041/Lone�surrogate.txt Je tam nejaky necitelny znak, ktery je spravne zobrazen pomoci otazniku (i kdyz Windows explorer jej zobrazuje jako ctverecek), ale Tandem Commander neumi tento soubor zkopirovat ani smazat."

## Problem Statement

Windows file systems permit file names that are **not valid Unicode text** — the
name is stored as a raw sequence of 16-bit units, and nothing stops a name from
containing an *unpaired surrogate* (a code unit reserved for two-unit pairs,
appearing alone). The repository's own test fixture
`temp\fixtures-041\Lone�surrogate.txt` is exactly such a file: its name is
`Lone` + unpaired high surrogate `U+D800` + `surrogate.txt`.

Tandem Commander lists the file correctly (the unreadable unit is shown as a
replacement question mark; Windows Explorer shows a box glyph — both are
acceptable displays), but **every operation on the file fails**: it cannot be
deleted, copied, or moved. Windows Explorer handles the same file without any
problem. The observed cause class: when the name makes a round trip through the
application's text handling, the unrepresentable unit is replaced, so the
operation targets a name that does not exist on disk.

A file the user can see but can neither delete nor copy is a trust-breaking
defect for a file manager, whose core promise is "any file you can see, you can
manage."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Delete a file with an unrepresentable name (Priority: P1)

A user finds a file whose name contains an unreadable character (shown as `?`)
in a panel, selects it, and deletes it — either to the Recycle Bin or
permanently. The file is gone afterwards.

**Why this priority**: Deletion is the escape hatch. If a defective or unwanted
file with such a name lands on the user's disk (downloads, extracted archives,
files created by other tools), the user must at minimum be able to get rid of
it without resorting to Explorer or the command line. This is the operation the
defect report leads with.

**Independent Test**: Create/restore the feature-041 fixture set, select
`Lone�surrogate.txt` in a panel, press F8 (and in a second run Shift+F8 /
permanent delete). Delivers value alone: the stuck file can be removed.

**Acceptance Scenarios**:

1. **Given** `temp\fixtures-041\Lone�surrogate.txt` exists and is shown in the
   active panel, **When** the user deletes it to the Recycle Bin, **Then** the
   operation completes without any error dialog, the file disappears from the
   panel, and it appears in the Recycle Bin.
2. **Given** the same file, **When** the user deletes it permanently,
   **Then** the operation completes without error and the file no longer exists
   on disk.
3. **Given** a folder that contains files with unpaired-surrogate names (or
   whose own name contains one), **When** the user deletes the folder,
   **Then** the recursive delete completes without error and nothing is left
   behind.

---

### User Story 2 - Copy and move preserve the exact name (Priority: P2)

A user copies (F5) or moves (F6) a file with an unreadable character in its
name to another folder. The operation succeeds, and the created entry's name is
**identical to the original, unit for unit** — the unreadable character is not
replaced by `?` or any substitute in the real on-disk name.

**Why this priority**: Copy/move are the other two operations named in the
defect report. Name fidelity matters beyond mere success: silently renaming the
file during copy would corrupt data for round-trip scenarios (backups, syncing
a tree, moving fixtures) and would make the copy differ from what Explorer
produces.

**Independent Test**: Copy the fixture file to a second folder, then enumerate
the destination and compare the raw name units against the source.

**Acceptance Scenarios**:

1. **Given** the fixture file in the source panel and an ordinary folder in the
   target panel, **When** the user copies it, **Then** the copy completes
   without error and the destination entry's name is unit-for-unit identical to
   the source name.
2. **Given** the same setup, **When** the user moves the file, **Then** the
   move completes without error, the source entry is gone, and the destination
   name is unit-for-unit identical.
3. **Given** a destination that already contains a file of the identical name,
   **When** the user copies over it, **Then** the overwrite confirmation
   appears and behaves as for any ordinary name collision.
4. **Given** a folder tree containing such names at several depths, **When**
   the user copies the tree, **Then** every entry arrives with its exact
   original name.

---

### User Story 3 - Remaining panel operations act on the true file (Priority: P3)

A user performs the other everyday panel operations on such a file — views it
(F3), opens the rename dialog and renames it, changes its attributes, checks
its size/occupied space — and each operation acts on the real file instead of
failing with "file not found".

**Why this priority**: Same root defect, lower urgency — these operations are
less destructive to a user's workflow than being unable to delete or copy, but
leaving them broken would keep the file "second-class" and the defect only
half-fixed.

**Independent Test**: Run each operation from the panel against the fixture
file and confirm it targets the real file (content shown, rename applied,
attributes changed).

**Acceptance Scenarios**:

1. **Given** the fixture file, **When** the user presses F3, **Then** the
   viewer opens the actual file content.
2. **Given** the fixture file, **When** the user renames it to a new ordinary
   name, **Then** the rename succeeds and the entry now carries the new name.
3. **Given** the fixture file, **When** the user changes its attributes (e.g.
   sets read-only), **Then** the change is applied to the real file.

---

### Edge Cases

- **Other unrepresentable sequences**: an unpaired *low* surrogate, a surrogate
  as the first or last unit of the name, multiple unpaired surrogates in one
  name, and a high surrogate followed by a non-surrogate — all must behave the
  same as the fixture case.
- **Folders**: a *folder* whose name contains an unpaired surrogate — the user
  can enter it, operate on files inside it (the defective unit then sits in
  every child path), and delete or copy the folder recursively.
- **Look-alike collisions**: two sibling files whose names differ only in the
  unrepresentable unit (e.g. `U+D800` vs `U+DC00`) display identically with
  `?` but MUST remain distinct objects — selecting and deleting one leaves the
  other untouched.
- **Lossy hand-offs**: where a name genuinely cannot be conveyed to a receiving
  program or interface outside the application's control, the operation must
  fail with a clear per-file error — never silently act on a different or
  nonexistent name.
- **Interaction with long paths**: the same names inside very long paths and
  UNC paths (the feature-004 territory) must behave identically.
- **Progress and confirmation dialogs**: dialogs that display the name during
  an operation may show the replacement character, but the operation underneath
  must still use the true name.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Every file operation initiated on a directory entry shown in a
  panel MUST act on the entry's true on-disk name, including names containing
  unpaired surrogate units or any other unit sequence Windows permits in a
  name.
- **FR-002**: Delete — both to the Recycle Bin and permanent — MUST succeed on
  files and folders whose name, or any path component above them, contains such
  units.
- **FR-003**: Copy and move MUST create the destination entry with a name
  unit-for-unit identical to the source name; a replacement or substitute
  character MUST never be written into a real on-disk name.
- **FR-004**: Recursive operations (copy, move, delete of a folder tree) MUST
  succeed when such names occur anywhere in the tree, at any depth.
- **FR-005**: Display substitution (showing `?` for an unreadable unit) MUST
  remain purely visual and MUST NOT leak into any operation, path composition,
  or name comparison that decides which on-disk object is affected.
- **FR-006**: Directory entries whose names differ only in unrepresentable
  units MUST remain distinct: selection, focus, and operations MUST affect
  exactly the entry the user chose.
- **FR-007**: If a specific hand-off genuinely cannot carry the true name
  (an external recipient with a text channel that cannot represent it), the
  operation MUST fail with an error message identifying the affected file —
  never proceed against a mangled name.
- **FR-008**: Behavior for all names that are valid Unicode text MUST remain
  unchanged (no regression to features 004, 052, 058, 062, 063).

### Key Entities

- **Directory entry name**: as stored by Windows — an arbitrary sequence of
  16-bit units, not guaranteed to be valid Unicode text. The authoritative
  identity of a file; must survive every round trip through the application
  losslessly.
- **Display name**: what the panel renders — may substitute a replacement
  character for unreadable units. Derived from the directory entry name,
  one-way; never used as the source for an operation.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The reported repro passes: `temp\fixtures-041\Lone�surrogate.txt`
  can be deleted (Recycle Bin and permanent), copied, and moved on the first
  attempt with zero error dialogs.
- **SC-002**: After a copy or move, enumeration of the destination folder shows
  a name unit-for-unit identical to the source — verified for 100% of a
  fixture set covering lone high surrogate, lone low surrogate,
  leading/trailing surrogate, multiple surrogates, and a surrogate in a folder
  name.
- **SC-003**: Operation parity with Windows Explorer on that fixture set:
  every operation Explorer completes on these entries (delete, copy, move,
  rename, attribute change, open/view), Tandem Commander completes as well.
- **SC-004**: In the look-alike collision scenario (two entries differing only
  in the invalid unit), the non-selected entry survives 100% of delete/move
  operations performed on its sibling.
- **SC-005**: No regression on ordinary names: the existing automated test
  suite passes, and the feature-041 fixture set (valid non-ASCII names)
  continues to work for all operations.

## Assumptions

- **Display stays as it is**: rendering the unreadable unit as `?` is accepted
  and matches the current, correct behavior; matching Explorer's box glyph is
  explicitly *not* required by this feature.
- **Scope is the local file-system panels**: disk drives, UNC and long paths.
  Contents of archives and plugin file systems (FTP, SFTP, WinRT, …) are out of
  scope; plugins keep their current behavior.
- **Operations on existing entries are the scope**: the feature covers acting
  on names that already exist on disk. Enabling users to *type* unpaired
  surrogates into name-input dialogs (new folder, rename target) is out of
  scope.
- **Windows Explorer is the behavioral reference** for what must succeed on
  such names.
- **The feature-041 fixture set is the canonical repro** and may be extended
  with the additional surrogate variants listed in Edge Cases; fixtures live
  under `temp\` and are not shipped.
- **Names of this kind are rare but legitimate**: they typically arrive from
  other tools, archives extracted by third-party software, or network shares —
  the file manager must cope with them rather than assume they never occur.

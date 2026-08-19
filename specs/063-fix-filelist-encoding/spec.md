# Feature Specification: Make File List — Correct Encoding and Dialog Layout

**Feature Branch**: `063-fix-filelist-encoding`
**Created**: 2026-08-19
**Status**: Draft
**Input**: User description: "Při kopírování seznamu souborů pro vložení např. do schránky - pomocí funkce CTRL+M se soubory vloží se špatným kódováním, např. v případě, že se kopírují české znaky, tak je vložený text ve špatně zobrazený. Navíc v pop-up okně, které se zobrazí při stisknutí CTRL+M se zobrazuje špatně textové pole 'nápověda k řádku' - tento text je špatně kódovaný. V tomto pop-up okně je navíc textové pole 'Soubor' v originále asi 'File', ale slovo 'Soubor' se nezobrazí celé - je ořízlé. Proveď detailní analýzu popsaných symptomů - především španého kódování seznamu souborů, navrhni a implementuj opravu."

## Problem Statement

The **Make File List** command (Ctrl+M) lets the user turn the current selection into
a text list — one formatted line per file — and send it to the clipboard, the internal
viewer, or a file on disk. Three defects were reported, all visible in the Czech
environment:

1. **Garbled list content (primary)**: when the listed names contain Czech characters
   (diacritics such as `ř`, `č`, `ž`), the text pasted from the clipboard shows
   garbage in place of those characters. A file list whose names cannot be trusted is
   useless for its main purposes — sharing, documentation, feeding other tools.

2. **Garbled help text in the dialog**: the Make File List dialog offers a "hint"
   link explaining the line-format syntax (width modifiers like `$(FileName:max)`).
   In the Czech UI the displayed help text itself is mis-encoded.

3. **Clipped label in the dialog**: the destination choice labeled "File" in English
   ("Soubor" in Czech) is cut off — the Czech word does not fit the control and is
   not fully readable.

The user asks for a detailed analysis of the symptoms — above all the wrong encoding
of the produced list — and for the fix to be designed and implemented. Preliminary
code exploration is recorded in `investigation-leads.md` (to be confirmed with
runtime evidence during planning/implementation); it indicates the list-content
defect is another regression-by-omission of the product-wide UTF-8 migration
(the same defect family as features 052 and 058), affecting the clipboard leg of the
feature, and that the two dialog defects are independent localization-surface issues.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Correct file list on the clipboard (Priority: P1)

A user selects files whose names contain Czech characters (e.g.
`Příloha č. 1 — žádost.pdf`, `Smlouva_údržba.docx`), presses Ctrl+M, keeps the
default destination (clipboard), and confirms. Pasting into any application —
Notepad, an e-mail, a spreadsheet — reproduces every name exactly as the panel
shows it.

**Why this priority**: This is the core purpose of the feature and the primary
reported defect. Every non-ASCII name is corrupted today, which makes the command
unusable for the product's home market.

**Independent Test**: Can be fully tested by selecting files with diacritic names,
pressing Ctrl+M → clipboard → OK, and pasting into Notepad; delivers the feature's
main value on its own.

**Acceptance Scenarios**:

1. **Given** a panel with files named using Czech diacritics, **When** the user runs
   Make File List with destination "clipboard" and pastes into a Unicode-aware
   application, **Then** every character of every name matches the panel display
   exactly (zero substituted or garbled characters).
2. **Given** file names containing characters outside the legacy Czech code page
   (e.g. Greek, Cyrillic, CJK), **When** the list is pasted into a Unicode-aware
   application, **Then** those characters are also reproduced exactly.
3. **Given** file names that are pure ASCII, **When** the list is produced, **Then**
   the output is identical to the current behavior (no regression).

---

### User Story 2 - Correct list at the viewer and file destinations (Priority: P2)

The same list generation serves two more destinations: "viewer" (the list opens in
the internal text viewer) and "file" (the list is written or appended to a file the
user names). The user expects the same character fidelity there.

**Why this priority**: Same defect surface and same data path as User Story 1, but
these destinations were not explicitly reported and the file leg may already be
closer to correct; still, the feature must be consistent end to end.

**Independent Test**: Run Ctrl+M twice on the same diacritic-named selection with
destinations "viewer" and "file"; inspect the viewer window and open the saved file
in Notepad.

**Acceptance Scenarios**:

1. **Given** a diacritic-named selection, **When** the destination is "viewer",
   **Then** the internal viewer displays every name exactly as the panel shows it.
2. **Given** a diacritic-named selection, **When** the destination is "file" and the
   saved file is opened in a standard Windows text editor (e.g. Notepad), **Then**
   every name reads correctly; the chosen text encoding is lossless for any name
   Windows allows.
3. **Given** the "append" option and an existing list file previously produced by
   this same fixed version, **When** a second list is appended, **Then** the whole
   file still reads correctly.

---

### User Story 3 - Readable line-format help in the dialog (Priority: P3)

A Czech-UI user opens the Make File List dialog and clicks the hint link that
explains the line-format syntax. The help text appears with all Czech characters
rendered correctly.

**Why this priority**: Display-only defect; it does not corrupt data, but garbled
help text looks broken and makes the syntax explanation unreadable.

**Independent Test**: Switch the UI language to Czech, open Ctrl+M, activate the
hint link, and visually verify the tooltip text.

**Acceptance Scenarios**:

1. **Given** the Czech UI, **When** the user activates the line-format hint in the
   Make File List dialog, **Then** the help text displays with correct diacritics
   (no mojibake).
2. **Given** any other shipped language whose hint text contains non-ASCII
   characters, **When** the hint is shown, **Then** it is also rendered correctly
   (the fix must cover the shared display mechanism, not one string).

---

### User Story 4 - Fully visible "File" label (Priority: P3)

A Czech-UI user opens the Make File List dialog and can read the destination label
"Soubor" in full — nothing is clipped.

**Why this priority**: Cosmetic layout defect, lowest impact, but part of the same
dialog and explicitly reported.

**Independent Test**: Open Ctrl+M in the Czech UI and visually verify the "Soubor"
radio label at standard and scaled DPI.

**Acceptance Scenarios**:

1. **Given** the Czech UI, **When** the Make File List dialog opens, **Then** the
   "Soubor" label is displayed in full (not truncated), at 100%, 150%, and 200%
   display scaling.
2. **Given** every other shipped language, **When** the dialog opens, **Then** no
   label in the dialog is clipped.

---

### Edge Cases

- Names mixing accented and plain characters together with fixed-width line
  variables (`$(FileName:20)`, `$(FileName:max)`): column alignment must be computed
  from displayed characters, not internal storage size — otherwise accented names
  visibly misalign the columns.
- Names containing characters not representable in the legacy system code page at
  all (CJK, symbols): the clipboard must still deliver them intact to Unicode-aware
  applications; applications that can only accept legacy-code-page text receive a
  best-effort representation (characters representable in that code page must be
  correct).
- A line at or near the maximum supported line length whose last characters are
  multi-byte: no character may be torn in half by truncation or buffer limits.
- Appending to a list file produced by an older (pre-fix) version: the new content
  must be written correctly; the old garbled content is not repaired (documented
  behavior, no silent re-encoding of existing file content).
- The command invoked in an archive or plugin-filesystem panel where it is
  available: same fidelity guarantees apply.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The generated file list MUST reproduce every listed file and directory
  name character-for-character as displayed in the panel, for any name Windows
  allows (full Unicode), at all three destinations.
- **FR-002**: The clipboard destination MUST provide the list such that
  Unicode-aware applications paste it with full fidelity; applications consuming
  only legacy-code-page text MUST receive a best-effort rendition in the user's
  system code page (all characters representable in that code page correct).
- **FR-003**: The viewer destination MUST display the list with all characters
  correct in the internal viewer without manual encoding selection.
- **FR-004**: The file destination MUST write the list in a text encoding that is
  lossless for all Unicode names and that standard Windows text editors open
  correctly by default; append mode MUST keep the whole file consistently readable
  across repeated appends by the fixed version.
- **FR-005**: Fixed-width line variables (`:N` and `:max` width modifiers) MUST pad
  and align by displayed character count so columns line up regardless of accents
  or other multi-byte characters in the names.
- **FR-006**: The line-format hint text in the Make File List dialog MUST render
  with correct characters in every shipped language; the fix MUST address the
  display mechanism shared with other dialogs' hints so the identical defect does
  not persist elsewhere.
- **FR-007**: Every label in the Make File List dialog MUST be fully visible (no
  clipping) in every shipped language, specifically the Czech "Soubor" destination
  label.
- **FR-008**: Lists consisting solely of ASCII characters MUST be produced
  byte-identically to the current behavior at all destinations (no regression for
  the unaffected majority case).

### Key Entities

- **File list**: the generated text — one formatted line per selected file or
  directory, built from a user-editable line template.
- **Line template**: the format string with variables (name, size, date, …) and
  optional width modifiers; stored in history between uses.
- **Destination**: one of clipboard, internal viewer, file on disk (with optional
  append).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A list of at least 20 names covering the full set of Czech diacritics
  pastes from the clipboard into a Unicode-aware editor with 100% character fidelity
  (zero garbled characters), verified against the panel display.
- **SC-002**: The same selection produces character-identical, correct text at all
  three destinations (clipboard paste, viewer display, saved file opened in
  Notepad) in a single session.
- **SC-003**: In the Czech UI, the Make File List dialog shows the line-format hint
  and all labels correctly and in full at 100%, 150%, and 200% display scaling
  (visual verification, zero defects).
- **SC-004**: For ASCII-only selections, the produced output is unchanged from the
  pre-fix version (empty diff).
- **SC-005**: A `:max`-aligned listing that mixes accented and unaccented names
  shows perfectly aligned columns (equal visual column start for the variable that
  follows the padded one).

## Assumptions

- The file-on-disk output encoding will be the product's standard lossless text
  encoding (UTF-8, the internal contract since feature 004). This changes the bytes
  written for non-ASCII names compared to the original Open Salamander ANSI output;
  that is accepted and unavoidable, because a legacy code page cannot represent all
  names. Whether a byte-order mark is written is an implementation decision to be
  made during planning with the internal viewer's encoding detection in mind.
- The garbled hint reproduces only in localized UIs whose hint translation contains
  non-ASCII characters (the English hint is pure ASCII), which is why it was
  reported from the Czech UI. The fix targets the shared hint-display mechanism;
  other dialogs using the same mechanism benefit, but a product-wide encoding audit
  beyond that mechanism is out of scope (the project addresses those incrementally,
  as in features 052 and 058).
- The clipped "Soubor" label is a localization layout issue (the Czech word is
  longer than the English original); the fix may adjust the localized dialog layout
  or the control size, with no functional change to the dialog.
- Verification is manual/visual for the dialog items and manual paste/diff for the
  list content; no automated UI test infrastructure exists for this dialog and none
  is required by this feature.
- Existing user configuration (stored line templates, chosen destination, list file
  name) continues to work unchanged; no settings migration is needed.

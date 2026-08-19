# Data Model — 063-fix-filelist-encoding

This is a defect fix; no new persistent data is introduced. The entities below are
the existing objects the fix touches, with their **encoding contract** — the core
"data model" question of this feature.

## Entities

### File list (generated text)

- **What**: the text produced by Make File List — one formatted line per selected
  file/directory, terminated per the line template (typically `$(CRLF)`).
- **Producer**: line-template expansion over the panel's items
  (`CFilesWindow::MakeFileList`, `src/fileswn6.cpp:124`).
- **Encoding contract**: **UTF-8 end to end** (names come from `CFileData::Name`,
  UTF-8 by contract since feature 004/052). Every consumer leg must either consume
  UTF-8 or convert explicitly at the boundary:
  - clipboard leg → convert UTF-8 → UTF-16 (`CF_UNICODETEXT`) + best-effort CP_ACP
    (`CF_TEXT`),
  - viewer leg → viewer must interpret the temp file as UTF-8,
  - file leg → file bytes are UTF-8 (BOM policy per research.md decision D4).
- **Validation**: character-for-character fidelity against panel display (spec
  FR-001); ASCII-only lists byte-identical to pre-fix output (FR-008).

### Line template

- **What**: user-editable format string with variables (`$(FileName)`, `$(Size)`,
  …) and width modifiers (`:N`, `:max`); stored in `Configuration.FileListHistory`
  (registry-backed history, UTF-8 since feature 005 — see `dialogs.cpp:1878`).
- **State**: unchanged by this feature; already UTF-8.
- **Width semantics** (FR-005): `:N`/`:max` measure and pad by **displayed
  characters**, not bytes (decision D6 in research.md).

### Destination

- **What**: enum-like configuration `Configuration.FileListDestination`
  (0 clipboard / 1 viewer / 2 file) + `FileListName` (target path, UTF-8) +
  `FileListAppend` (BOOL).
- **State**: unchanged; no settings migration (constitution: MINORB release must
  not move configuration).

### Dialog strings & layout (localization surface)

- **What**: `IDS_FILELISTLINE_HINT` (line-syntax help) and the `IDD_FILELIST`
  dialog template geometry per language (`translations/<lang>/salamand.slt`).
- **Encoding contract**: hint text must reach the tooltip renderer in the encoding
  the renderer assumes (see contracts/filelist-text-encoding.md).
- **Layout rule**: every control caption fits its control rectangle in every
  shipped language (FR-007).

## No state transitions, no new storage

All configuration keys, history formats, and registry locations stay as-is.

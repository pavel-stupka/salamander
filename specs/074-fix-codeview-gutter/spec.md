# Feature Specification: Fixed-width line-number gutter in the code viewer

**Feature Branch**: `074-fix-codeview-gutter`
**Created**: 2026-09-01
**Status**: Draft
**Input**: User description: "Podivej se na screenshot s CodeView pluginu v ./temp/radky.png. Zasadni chyba zobrazeni je, ze cislovani radku ovlivnuje zarovnani celeho radku a radky tak jsou posunuty. Toto je zasadne spatne. Ve vsech editorech, kde je cislovani radku musi mit sloupec s cislama radku stejnou sirku. Idealni tedy je v tomto sloupci cisla radku zarovnavat doprava / coz i idpovida analogii zapisu cisel / tj. jednotky pod sebe, desitky pod sebe, stovky pod sebe atd.."

## Context

The code viewer (Prohlížeč kódu, feature 070) shows a line number in front of
every line. The width of that number column is not fixed: it is whatever the
number on that particular line happens to need. A one-digit number reserves
less room than a two-digit one, so the source text does not start in the same
place on every line.

The supplied screenshot (`temp/radky.png`, a JSON file of 21 lines) shows the
result: lines 1–9 begin one character to the left of lines 10–21. The whole
document is visibly ragged at the 9 → 10 boundary, and the same step repeats at
99 → 100 and at every further power of ten. Indentation — the main structural
cue in source code — can no longer be read down the page, because the text
column moves.

This is a display defect in shipped behaviour, not a new capability. Every
editor with line numbering keeps the number column at one width for the whole
document, and numbers within it right-aligned, so that units sit under units,
tens under tens and hundreds under hundreds.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Source text starts at the same place on every line (Priority: P1)

A user opens a source file of more than nine lines in the code viewer. The
first character of every line of code sits in exactly the same column,
regardless of how many digits that line's number has. Indentation reads as a
straight vertical edge down the page, and the eye can follow a nesting level
without being deflected at line 10, 100 or 1000.

**Why this priority**: This is the reported defect. Any file longer than nine
lines — which is nearly every real file — is currently displayed misaligned,
which undermines the viewer's core purpose of reading structured code.

**Independent Test**: Open the screenshot's own file (a 21-line JSON document)
and compare the horizontal start of the text on lines 9 and 10, and lines 99
and 100 in a longer file. Delivers the fix on its own even if numbers were
still left-aligned inside the column.

**Acceptance Scenarios**:

1. **Given** a file of 21 lines shown with line numbering on, **When** the user
   looks at the boundary between line 9 and line 10, **Then** both lines' text
   begins at the identical horizontal position.
2. **Given** a file of several thousand lines, **When** the user compares any
   two lines anywhere in the document, **Then** their text begins at the
   identical horizontal position.
3. **Given** a file whose lines are indented, **When** the user follows an
   indentation level down the page, **Then** the indent forms an unbroken
   vertical line with no step at any power-of-ten boundary.

---

### User Story 2 - Line numbers are right-aligned in their column (Priority: P2)

The numbers themselves are set flush to the right-hand edge of the number
column, so that the units digits form one vertical column, the tens another,
and so on — the way numbers are conventionally written under one another.

**Why this priority**: It is the user's stated preference and the universal
convention, and it makes the numbers themselves scannable. It is separable
from P1: a fixed-width column with left-aligned numbers would already fix the
text alignment but would still look wrong.

**Independent Test**: In a document spanning at least three digit widths (e.g.
120 lines), check that the last digit of 7, of 42 and of 118 sit in one column.

**Acceptance Scenarios**:

1. **Given** a file of at least 120 lines, **When** the user compares the line
   numbers 7, 42 and 118, **Then** their final digits sit in the same vertical
   column, and 42's tens digit sits under 118's tens digit.
2. **Given** a file of at least 1000 lines, **When** the user scrolls from the
   start to the end, **Then** every number remains flush right against the same
   edge, and the gap between that edge and the text column is constant.

---

### User Story 3 - Alignment survives every viewer interaction (Priority: P3)

Alignment is a property of the view, not of the moment it was first drawn. It
holds while the user scrolls a large file, zooms in and out, toggles word wrap,
switches colour scheme, turns line numbering off and on again, and opens
another file in the same window.

**Why this priority**: The viewer materialises only the lines currently in
view, so an alignment computed from what is on screen would drift as the user
scrolls. Making the guarantee explicit is what keeps the fix from being partial.

**Independent Test**: Open a file of ~100,000 lines, scroll from the first line
to the last, and confirm the text column never moves; then zoom in two steps
and repeat the check.

**Acceptance Scenarios**:

1. **Given** a file of ~100,000 lines, **When** the user scrolls from line 1 to
   the last line, **Then** the source text column does not shift horizontally
   at any point.
2. **Given** any open file, **When** the user zooms in or out, **Then** the
   number column and the text column scale together and stay aligned, with the
   numbers still flush right.
3. **Given** any open file, **When** the user toggles word wrap on, **Then**
   the number is shown once per logical line and the continuation rows of a
   wrapped line begin at the text column, not under the numbers.
4. **Given** any open file, **When** the user turns line numbering off,
   **Then** no empty column is left behind — the text starts at the left edge —
   and turning numbering back on restores the aligned column.
5. **Given** a 40-line file is open, **When** the user opens a 4,000-line file
   in the same window, **Then** the number column is re-sized for the new
   document and every line of the new document is aligned.

---

### Edge Cases

- A file with fewer than ten lines: the column is exactly as wide as it needs
  to be, all lines align trivially, and no needless empty space is reserved.
- A file with one line, and a completely empty file (no lines at all): the
  viewer shows no number for a line that does not exist and reserves no
  misleading column.
- A very large file whose highest line number has seven or more digits: the
  column is wider, still constant, and the text column starts correspondingly
  further right — this is expected, not a defect.
- Horizontal scrolling of long lines: the number column stays pinned at the
  left edge and always covers the same width, so text scrolled underneath it is
  never partially hidden by a column of changing width.
- Find and go-to-line reveal a match on a long line: the reveal accounts for the
  pinned number column, so the match is never left underneath it.
- Whitespace markers (space dots, tab arrows) and any current-line or match
  highlighting line up with the text column, not with a per-line offset.
- Copying and Select All: the alignment is achieved without inserting any
  character into the document text, so nothing new can reach the clipboard.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The line-number column MUST have one single width for the whole
  document, so the source text of every line begins at the identical horizontal
  position.
- **FR-002**: That width MUST be derived from the widest line number the
  document can show — i.e. from the document's total line count — and not from
  the numbers that happen to be on screen.
- **FR-003**: Line numbers MUST be right-aligned within the column: units under
  units, tens under tens, hundreds under hundreds.
- **FR-004**: The width MUST NOT change while the user scrolls the document,
  including in files large enough that only a fraction of the lines is ever
  drawn at once.
- **FR-005**: The width MUST be recomputed when the displayed document changes
  (a different file, or the same file re-read with a different line count), and
  MUST be correct from the first frame the user sees — no visible reflow of the
  text column after opening.
- **FR-006**: The column and the numbers in it MUST scale with the viewer's
  zoom level and with any change of font metrics, remaining aligned at every
  zoom step.
- **FR-007**: With line numbering turned off, the column MUST occupy no space
  at all; turning it back on MUST restore the same aligned column without
  reloading the document.
- **FR-008**: With word wrap on, each logical line MUST carry its number once,
  and the wrapped continuation rows MUST begin at the text column — never under
  the number column.
- **FR-009**: The separation between the numbers and the source text MUST stay
  visually clear at every width, so a four- or seven-digit number never appears
  to touch the code.
- **FR-010**: Alignment MUST be achieved without inserting any character into
  the document text (no padding spaces, no substitute digits), so copied text,
  Select All, find matches and reported column positions are byte-for-byte
  unchanged by this feature.
- **FR-011**: Horizontal scrolling MUST keep the number column pinned at the
  left edge with its constant width, and any automatic reveal of a match or a
  target column MUST account for that width so the target is not left hidden
  beneath it.
- **FR-012**: The change MUST be display-only: no new setting, no change to
  stored configuration, no change to the plugin interface, and no change to any
  other viewer behaviour (theme, encoding, find, go-to-line, copy).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a document of any length, the horizontal start of the source
  text is identical on 100 % of lines — measured on the screenshot's own 21-line
  file and on files crossing the 99 → 100 and 999 → 1000 boundaries, zero lines
  deviate.
- **SC-002**: In a document spanning three digit widths, the last digit of
  every line number occupies the same column on 100 % of lines.
- **SC-003**: Scrolling a ~100,000-line file from the first to the last line
  produces zero horizontal movement of the text column.
- **SC-004**: Text copied from the viewer is byte-identical to the copied text
  before the change, for every case in the existing copy checks (selection,
  select-all, selections crossing wrapped lines).
- **SC-005**: Opening a file and toggling zoom, wrap, and line numbering in any
  order leaves the view aligned in 100 % of the combinations exercised by the
  viewer's manual GUI matrix.
- **SC-006**: No measurable regression in the time to first paint or in scroll
  smoothness against the targets already set for the viewer.

## Assumptions

- The viewer renders source text in a monospaced font, so a column width
  expressed in character widths is exact; the existing zoom mechanism changes
  the character width and the column scales with it.
- The column is sized to exactly the number of digits the document needs, with
  no artificial minimum: a short file keeps the code as far left as possible.
  This is a deliberate choice over reserving a fixed two- or three-digit
  minimum, and can be revisited if the column is felt to be too narrow on very
  short files.
- The existing horizontal padding between the number column and the code, and
  the existing colours for the numbers, are kept as they are — only the width
  and the alignment inside the column change.
- Scope is the code viewer plugin only. The Markdown viewer does not number
  lines and is unaffected; the classic internal text viewer is out of scope.
- No user-visible strings, help topics or translations are affected, so no
  translation work is implied.

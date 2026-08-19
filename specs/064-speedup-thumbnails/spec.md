# Feature Specification: Instant Thumbnails in Large Folders

**Feature Branch**: `064-speedup-thumbnails`
**Created**: 2026-08-19
**Status**: Draft
**Input**: User description: "Nyní je potřeba vylepšit, tj. zrychlit zobrazování náhledových obrázků v režimu zobrazení obrázků u souborů v panelu při nastavení ALT+5. Pokud má uživatel nastavený na zobrazení 'miniatury' (ALT+5), tak se zobrazují přímo náhledy obrázků, které se postupně vykreslují. Problém ale je, že pokud jsem v adresáři, který má mnoho obrázků, velkých fotek, řádově tisíce a více, a celková velikost jsou desítky GB, tak se náhledy v podstatě nezobrazují, resp. až za strašně dlouhý čas. Proveď detailní analýzu tohoto chování a navrhni a implementuj řešení, ve kterém se v podstatě ihned začnou zobrazovat náhledy u miniatur souborů, bez ohledu na to, kolik obrázků souborů v adresáři je. Vždy by se tedy měly upřednostňovat ty soubory, které jsou právě vidět. Ale to nechám na zvážení tobě. Detailně to celé prozkoumej a navrhni různá řešení, společně pak vybereme to nejefektivnější."

## Problem Statement

The Thumbnails view (Alt+5) renders a small preview picture for every image
file in the panel. In an ordinary folder this works: previews fill in
progressively within moments. But in a large photo folder — thousands of
images, tens of gigabytes — thumbnails effectively never arrive: the user
stares at generic icons for minutes, and the previews of the files actually on
screen appear no sooner than anything else. The bigger the folder, the worse
the experience, precisely where thumbnails matter most (finding one photo
among thousands).

The user asks for a detailed analysis of the current behavior and for a fix in
which visible files get their previews essentially immediately, **independent
of how many images the folder contains**. The user suggested prioritizing the
currently visible files and explicitly requested that **several solution
approaches be worked out and the most effective one selected together** before
implementation. Preliminary code pointers are recorded in
`investigation-leads.md`; the full analysis and the comparison of candidate
solutions is the planning phase's deliverable.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Visible previews appear immediately (Priority: P1)

A user opens a folder containing thousands of large photos (tens of GB total)
in Thumbnails view. The previews of the files currently on screen start
appearing essentially immediately — just as fast as they would in a folder
with only a hundred images.

**Why this priority**: This is the reported defect and the core value: the
view must be usable the moment it opens, at any folder size.

**Independent Test**: Open a fixture folder with ≥5,000 large photos in
Thumbnails view; measure the time until the first visible thumbnail appears
and until the visible screen is fully populated; compare with a 100-photo
folder.

**Acceptance Scenarios**:

1. **Given** a folder with thousands of large photos, **When** the user enters
   it in Thumbnails view, **Then** the first thumbnails of visible files
   appear within seconds — with no dependence on the total number or total
   size of files in the folder.
2. **Given** the same folder, **When** the visible screen has finished
   populating, **Then** preview generation continues in the background without
   the user having to do anything.
3. **Given** a folder with only a few images (e.g. 100), **When** entered,
   **Then** behavior is at least as fast as today (no regression in small
   folders).

---

### User Story 2 - Scrolling reprioritizes to the new viewport (Priority: P1)

The user scrolls (or jumps with Home/End/PgDn, or types to quick-search) deep
into the same huge folder. The files now on screen become the top priority:
their previews start appearing right away, ahead of any off-screen backlog.

**Why this priority**: Prioritizing "what I look at now" is the explicit user
requirement; without it the immediate-first-screen fix would only help at the
top of the folder.

**Independent Test**: In the fixture folder, jump to the middle and to the
end; measure time until previews of the new viewport start appearing and
verify they complete ahead of off-screen items.

**Acceptance Scenarios**:

1. **Given** preview generation is still running for a huge folder, **When**
   the user scrolls to any position, **Then** previews for the newly visible
   files start appearing promptly (comparably fast as for the first screen)
   and are not stuck behind the off-screen backlog.
2. **Given** rapid continuous scrolling, **When** the user stops, **Then** the
   viewport at the stopping point is populated next — work is not wasted on
   long-since-passed positions.

---

### User Story 3 - The panel stays responsive throughout (Priority: P2)

While previews are being generated for a huge folder, the user can scroll,
select files, invoke commands (view, copy, delete, change sort order, leave
the folder) without the interface stuttering or blocking.

**Why this priority**: Immediacy is worthless if the price is a frozen or
laggy panel; conversely today's implementation is already asynchronous, so
this protects an existing quality while the pipeline is reworked.

**Independent Test**: During generation in the fixture folder, exercise
scrolling, selection, sort change, folder change; observe no perceptible input
lag or freezes.

**Acceptance Scenarios**:

1. **Given** generation in progress, **When** the user scrolls or navigates,
   **Then** input is processed without perceptible delay.
2. **Given** generation in progress, **When** the user leaves the folder or
   switches view mode, **Then** the pending work stops promptly and does not
   slow down the new location.
3. **Given** generation in progress, **When** the user changes the sort order
   or a refresh arrives, **Then** already-shown previews of unchanged files do
   not vanish or regenerate, and prioritization adapts to the new visible set.

---

### Edge Cases

- A corrupt or unreadable image file: the file keeps its generic icon and the
  pipeline moves on — one bad file must not stall or slow the rest.
- Extremely large single images (hundreds of MPix): must not monopolize the
  pipeline; other visible previews continue arriving.
- Mixed folders (images among documents and folders): non-image files are
  skipped without cost; their icons behave as today.
- Network or otherwise slow media: the prioritization still holds (visible
  first); slowness affects only the pace, not the order.
- A file changes on disk (new size/date) while its preview is shown: the
  preview refreshes for that file only.
- The panel is resized or the thumbnail grid re-flows: the new visible set
  becomes the priority.
- Repeated refreshes (e.g. files being added by a running copy): previously
  generated previews of unchanged files survive; only new/changed files are
  (re)generated.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: In Thumbnails view, previews of the files currently visible in
  the panel MUST begin appearing promptly after the folder listing is shown,
  with no dependence on the total number of image files or their total size in
  the folder.
- **FR-002**: Files currently visible in the panel MUST take priority over
  off-screen files at all times; any change of the visible set (scrolling,
  jumping, resize, sort change) MUST redirect the generation priority to the
  new visible set.
- **FR-003**: The panel and the whole application MUST remain responsive
  during preview generation: navigation, selection, and commands work without
  perceptible delay.
- **FR-004**: Off-screen previews MUST continue to be generated in the
  background so the folder eventually becomes fully populated, but this
  background work MUST always yield to newly visible files.
- **FR-005**: Leaving the folder, switching view mode, or closing the panel
  MUST stop pending preview work promptly and free its resources; obsolete
  work (for positions no longer relevant) MUST NOT be finished at the expense
  of current requests.
- **FR-006**: A failure to decode an individual file MUST NOT stall or abort
  the pipeline; the file keeps its generic icon and an internal note prevents
  repeated futile attempts within the session.
- **FR-007**: A refresh or sort change MUST NOT discard or regenerate previews
  of files whose content is unchanged (same name, size, and timestamp) within
  the session; only new or changed files are (re)generated.
- **FR-008**: The change MUST NOT alter the visual appearance of thumbnails
  (size, quality, layout) or the behavior of other view modes and non-image
  files.

### Key Entities

- **Preview (thumbnail)**: the small rendered picture for one image file;
  keyed by the file's identity (name, size, timestamp) within the session.
- **Visible set (viewport)**: the files currently shown in the panel — the
  priority driver; changes with scrolling, resizing, and re-sorting.
- **Generation backlog**: image files of the folder not yet populated;
  processed in the background, always after the visible set.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a fixture folder with ≥5,000 photos totaling ≥20 GB, the
  first visible thumbnails appear within 2 seconds of the listing being shown,
  and the time to the first visible thumbnail differs from a 100-photo folder
  by no more than a factor of 2.
- **SC-002**: After a jump to any position in the same folder, previews for
  the new viewport start appearing within 1 second of the jump and the whole
  visible screen populates before any off-screen backlog progresses further.
- **SC-003**: During generation, panel input (scrolling, cursor movement,
  selection) shows no freeze perceivable by the user (no single input stall
  approaching the threshold of annoyance, ~100 ms).
- **SC-004**: In a 100-photo folder, time to a fully populated first screen is
  no worse than the pre-change build (no small-folder regression).
- **SC-005**: Previews of unchanged files survive a refresh and a sort change
  with zero regenerations (verified by observing that repopulation after
  re-sort is immediate).

## Assumptions

- **Solution selection is a joint decision**: the planning phase will present
  several candidate approaches with trade-offs (for example: viewport-first
  scheduling of the existing pipeline; using reduced/embedded preview data
  that large photos typically carry; decoding at reduced resolution;
  parallelizing the decode work; persisting previews across sessions), and
  the user picks the approach — or combination — to implement before any
  implementation task starts. This is an explicit user request.
- The scope is the panel's Thumbnails view (Alt+5) in the main application;
  the source of preview pixels (built-in and plugin-provided) keeps its
  existing plugin contract unless the chosen solution explicitly extends it.
- Target environment: Windows 11 machines with local disks; slow/network
  media benefit from the same prioritization but get no separate targets.
- Verification is by stopwatch/scripted measurement on a generated fixture
  folder (large synthetic photos) plus manual scrolling checks; no automated
  UI test infrastructure exists for the panel.
- Memory footprint stays within the current thumbnail cache design; no new
  unbounded caches (folders with tens of thousands of images must not exhaust
  memory).

# Feature Specification: Cloud Sync Status Icons in File Panels

**Feature Branch**: `058-fix-cloud-status-icons`
**Created**: 2026-08-18
**Status**: Draft
**Input**: User description: "Důkladně analyzuj a oprav chování zobrazení výpisu souborů a adresářů v připojených OneDrive, resp. Google Drive adresářích / cestách. U OneDrive, resp. Google Drive a možná i dalších se u ikon adresářů a souborů zobrazují ještě dodatečné ikony, které říkají, zdali je adresář celý sycnhronizvaný, nebo pouze v cloudu, nebo se nahrává, nebo je chyba atd. V podstatě tedy status souboru vůči cloudovému úložišti. V některých případech se ale tyto status ikony nezobrazí vůbec, např. v adresáři \"G:\Můj disk\", což je připojený Google Drive. Navíc v těchto případech, kdy nejsou ikony zobrazené, se při získání focusu okna Tandem Salamanderu na chvíli zobrazí namísto kurzoru myší loading kolečko - takže něco se asi program snaží načíst a pak ale se nic nestane a stavové ikony nejsou zobrazené."

## Problem Statement

Windows shows a small per-item badge on files and folders that live inside a
cloud-synchronized location (OneDrive, Google Drive, and similar providers):
fully synced, available online only, sync in progress, sync error, and so on.
Tandem Commander displays these status badges in some cloud locations but not
in others. The reported failing case is a Google Drive mounted drive — e.g.
`G:\Můj disk` — where **no status badges appear at all**, even though Windows
Explorer shows them for the same items.

In exactly those locations where the badges are missing, a second symptom
appears: whenever the Tandem Commander window regains focus, the mouse cursor
briefly turns into a busy/loading indicator, then nothing visible happens —
the program evidently attempts to load something on every activation, the
attempt yields no result, and the wasted attempt repeats on every focus
change.

A third symptom was reported in the same locations: **base file-type icons
are wrong** — documents such as Word or PDF files show a generic blank icon
instead of their real application icon.

Two additional facts scope the problem. First, the original Altap/Open
Salamander shows the badges and icons in the same locations correctly (apart
from a minor pre-existing imperfection in the "syncing" state) and shows no
busy cursor — so the defects were introduced during Tandem Commander
development, not inherited. Second, analysis confirmed the trigger is **not
the cloud provider**: all three symptoms occur in any folder whose full path
contains characters outside the ASCII range (the reported `G:\Můj disk`
contains "ů"), while ASCII-only paths — including typical OneDrive folders —
are unaffected. Cloud drives are simply where non-ASCII folder names
("Můj disk") are guaranteed to appear.

The feature has three goals: (1) status badges appear in all
cloud-synchronized locations where Windows Explorer shows them, including
Google Drive mounted drives, (2) the repeated fruitless loading attempt on
window focus is eliminated, and (3) items in affected folders show their
correct file-type icons.

## Clarifications

### Session 2026-08-18

- Q: When the cloud provider becomes available only after its folder is
  already listed (e.g. the Google Drive client starts while Tandem Commander
  is running), when should the status badges appear? → A: On the next listing
  or refresh of the folder (manual refresh, path change, or automatic
  directory-change notification) — no background polling for provider
  availability, no application restart required.
- Q: The application's configuration already lets users disable icon
  overlays (the badge mechanism), globally and per handler. Should cloud
  sync-status badges obey that existing setting even if they are internally
  obtained by a different mechanism? → A: Yes — cloud badges follow the
  existing icon-overlay configuration (disabled = no cloud badges); the
  feature adds no new configuration options.
- Q: How quickly must sync-status badges be visible after a folder listing
  appears (~100 visible items, provider running normally), so the
  Explorer-parity acceptance test has a decidable verdict? → A: Within
  2 seconds for all on-screen items under nominal conditions; off-screen
  items may continue to fill in progressively.
- User-supplied additions during planning: (1) the original Altap/Open
  Salamander behaves correctly in the same locations (no missing badges
  apart from a minor "syncing"-state imperfection, no busy cursor), so the
  defect was introduced during Tandem Commander development; (2) a third
  symptom exists in the same locations — base file-type icons (e.g. Word,
  PDF) display as a generic icon. Both are folded into Problem Statement,
  User Story 4, FR-012/FR-013 and SC-007/SC-008.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Status badges on a Google Drive mounted drive (Priority: P1)

A user has the Google Drive desktop client installed, which exposes their
cloud content as a mounted drive (typically `G:\`, containing `Můj disk` /
"My Drive"). They open that path in a Tandem Commander panel. Each file and
folder shows the same sync-status badge that Windows Explorer shows for it:
synced, online-only, syncing, or error.

**Why this priority**: This is the reported defect — today the user gets no
status information at all in this location, so they cannot tell whether a
file is locally available or online-only before opening or copying it. That
distinction directly affects whether an operation is instant or triggers a
download.

**Independent Test**: On a machine with the Google Drive client running, open
`G:\Můj disk` in a panel next to the same folder in Windows Explorer and
compare item by item: every item that carries a status badge in Explorer
carries a corresponding badge in the panel.

**Acceptance Scenarios**:

1. **Given** the Google Drive client is running and its drive is mounted,
   **When** the user lists a folder on that drive in a panel, **Then** every
   file and folder displays the sync-status badge matching what Windows
   Explorer displays for the same item.
2. **Given** a folder on the mounted drive is listed and an item's sync state
   changes (e.g. a download completes), **When** the panel refreshes (manual
   refresh or automatic change notification), **Then** the badge reflects the
   new state.
3. **Given** the Google Drive client mounts under a different drive letter
   than the default, **When** the user lists a folder on that drive, **Then**
   badges display the same way (no dependence on a specific letter).

---

### User Story 2 - No fruitless loading on window focus (Priority: P1)

A user works with a cloud-synchronized folder open in a panel. They switch to
another application and back. Tandem Commander regains focus without flashing
a busy cursor that produces no visible result.

**Why this priority**: The busy-cursor flash occurs on *every* window
activation for affected paths — it is a constant, visible irritation that
signals wasted work, and the user explicitly reported it. It is the second
half of the same defect: the program starts a loading attempt that never
succeeds.

**Independent Test**: Open an affected cloud folder in a panel, repeatedly
switch focus away and back (e.g. 10×), and observe the cursor: no busy
indicator appears unless the activation actually results in updated panel
content.

**Acceptance Scenarios**:

1. **Given** a cloud-synchronized folder is listed in a panel and its status
   badges are already displayed and current, **When** the main window regains
   focus, **Then** no busy/loading cursor is shown.
2. **Given** a folder where status badges genuinely need re-reading after
   activation, **When** the main window regains focus, **Then** any loading
   happens without blocking the user's input and ends with the badges
   visibly updated.

---

### User Story 3 - Existing providers keep working (Priority: P2)

A user with OneDrive (or another sync provider that already shows badges in
Tandem Commander today) continues to see the same status badges after the
fix, with unchanged panel responsiveness.

**Why this priority**: The fix must not trade one provider for another —
OneDrive is the most widespread provider and currently works; a regression
there would be worse than the original defect.

**Independent Test**: On a machine with OneDrive configured, list a OneDrive
folder containing items in several sync states before and after the change
and verify the badges are identical and listing feels equally fast.

**Acceptance Scenarios**:

1. **Given** a OneDrive folder with items in different sync states, **When**
   the user lists it in a panel, **Then** each item shows the same badge as
   before the change (and as Windows Explorer).
2. **Given** any non-cloud local folder, **When** the user lists it, **Then**
   panel display and responsiveness are unchanged (badges never appear where
   none belong).

---

### User Story 4 - Correct file-type icons in affected folders (Priority: P1)

A user opens a folder whose path contains non-ASCII characters (e.g.
`G:\Můj disk`) in a panel. Word documents, PDF files, and other registered
file types show their real application icons — the same icons Windows
Explorer shows — not a generic blank-document icon.

**Why this priority**: Wrong base icons in the same locations are part of the
same reported defect and are even more visible than missing badges: the user
cannot recognize file types at a glance, which is core file-manager
functionality.

**Independent Test**: Place a `.docx` and a `.pdf` file in a folder whose
path contains a non-ASCII character (no cloud provider needed, e.g.
`D:\Test\Zkouška`), list it in a panel, and compare icons with an
ASCII-named sibling folder holding copies of the same files.

**Acceptance Scenarios**:

1. **Given** a folder whose path contains non-ASCII characters, **When** the
   user lists it in a panel, **Then** every item shows the same base icon as
   the identical item in an ASCII-only folder (and as Windows Explorer).
2. **Given** any folder, **When** items show icons, **Then** icon
   correctness does not depend on which characters the folder's path
   contains.

---

### Edge Cases

- **Provider installed but not running**: the Google Drive client is
  installed but stopped (mounted drive absent, or present but dead). Panels
  must list local content normally, show no stale badges, and must not retry
  a doomed status query on every activation. Once the client starts, badges
  recover on the next listing/refresh of an affected folder (see FR-010) —
  a failed attempt is never cached for the rest of the application's run.
- **Provider not installed at all**: no badge machinery runs, no repeated
  probing for it, no busy cursor.
- **Multiple providers side by side** (OneDrive + Google Drive + e.g.
  Dropbox): each location shows its own provider's badges; providers do not
  interfere with each other.
- **Windows badge-slot limits**: Windows caps how many badge providers can be
  active system-wide, so Explorer itself sometimes shows no badge for a
  provider that lost the contest. Parity with Explorer on the same machine is
  the requirement — Tandem Commander is not required to show badges Explorer
  itself cannot show.
- **Online-only files**: determining and displaying an item's status must not
  cause the file's content to be downloaded ("hydrated"). Listing a folder of
  1,000 online-only files must leave all 1,000 online-only.
- **Slow or misbehaving provider component**: a provider whose status query
  is slow, hangs, or crashes must not freeze panels, block the main window,
  or crash the application; in the worst case that provider's badges are
  simply absent. (The codebase already records that the Google Drive handler
  has historically been both slow and crash-prone — the fix must not
  reintroduce those failures.)
- **Non-ASCII paths are the general trigger, not a corner case**: the failing
  path itself (`G:\Můj disk`) contains a non-ASCII character, and all three
  symptoms (missing badges, busy-cursor refresh loop, generic file icons)
  reproduce in *any* folder whose full path contains non-ASCII characters —
  cloud or local. Badges, base icons, and automatic change monitoring must
  work for items and paths with any Unicode name; acceptance tests must
  cover both a cloud location and a plain local folder with a diacritic
  name.
- **Network offline**: with no connectivity, badges may show error/pending
  states as the provider reports them; the panels themselves stay responsive.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: In any folder belonging to a cloud-synchronized location whose
  provider is integrated with Windows, file panels MUST display a per-item
  sync-status badge for every item for which Windows Explorer displays one on
  the same machine, with a state that matches Explorer's.
- **FR-002**: Status badges MUST display on Google Drive mounted-drive paths
  (e.g. `G:\Můj disk`), regardless of which drive letter the provider mounts,
  and regardless of how the provider exposes the location (dedicated drive
  letter or a folder inside the user profile).
- **FR-003**: The set of displayed states is the provider's own (e.g. synced,
  online-only, syncing, error); the application renders what the provider
  reports and MUST NOT invent or reinterpret states.
- **FR-004**: Regaining main-window focus MUST NOT show a busy/loading cursor
  unless the activation actually produces updated panel content; in
  particular, a status-loading attempt that cannot succeed MUST NOT be
  retried on every window activation.
- **FR-010**: If a provider becomes available while the application is
  running (e.g. its client is started later), status badges MUST appear on
  the next listing or refresh of an affected folder (manual refresh, path
  change, or automatic directory-change notification) — without restarting
  the application and without the application polling in the background for
  the provider's availability.
- **FR-011**: Cloud sync-status badges MUST obey the application's existing
  icon-overlay configuration: where the user has disabled icon overlays
  (globally or for the relevant handler), no cloud badges display and no
  status queries run for them. The feature MUST NOT add new configuration
  options.
- **FR-012**: Items in any folder MUST display their correct base file-type
  icons (as Windows Explorer shows them) regardless of which characters the
  folder's path contains; a generic-icon fallback caused purely by the
  path's characters is a defect.
- **FR-013**: Automatic change monitoring (auto-refresh) MUST work in
  folders whose path contains non-ASCII characters exactly as it does in
  ASCII-only folders: changes made by other programs appear without user
  action, and the panel does not fall back to refresh-on-every-activation.
- **FR-005**: Determining an item's sync status MUST NOT trigger download
  ("hydration") of online-only content.
- **FR-006**: Status determination MUST NOT block the user interface: folder
  contents (names, sizes, base icons) display without waiting for badges, and
  badges appear progressively as they become available.
- **FR-007**: A provider component that is slow, unresponsive, or faulty MUST
  NOT freeze or crash the application; the failure is contained to that
  provider's badges being absent.
- **FR-008**: Badge display for currently working providers (OneDrive and any
  other provider whose badges display today) MUST NOT regress in correctness
  or perceived performance.
- **FR-009**: When no sync provider applies to a path (plain local or network
  folder), no badge machinery may add user-visible cost (no probing delays,
  no busy cursor, no badges).

### Key Entities

- **Cloud-synchronized location**: a folder subtree managed by a sync
  provider — exposed either as a folder in the user profile (OneDrive style)
  or as a mounted drive (Google Drive style); the trigger for badge display.
- **Sync-status badge**: the small secondary icon rendered with an item's
  base icon, conveying the item's state relative to the cloud copy; its
  states and artwork come from the provider.
- **Sync provider**: third-party software (OneDrive, Google Drive, …)
  registered with Windows that owns the location and answers per-item status
  queries.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a Google Drive mounted-drive folder, 100% of items that show
  a sync-status badge in Windows Explorer show a matching badge in a Tandem
  Commander panel on the same machine; for on-screen items the badges are
  visible within 2 seconds of the listing appearing (nominal conditions,
  ~100 visible items), while off-screen items may fill in progressively.
- **SC-002**: Ten consecutive focus-away/focus-back cycles over an affected
  cloud folder produce zero busy-cursor flashes that end without a visible
  content update.
- **SC-003**: Listing a cloud folder of 1,000 items renders the item names
  and base icons without perceptible extra delay compared to a same-size
  local folder; badges may arrive progressively while the panel stays
  interactive.
- **SC-004**: Listing a folder of online-only files leaves 100% of them
  online-only (zero unintended downloads).
- **SC-005**: On a OneDrive test folder covering all provider states, badge
  display after the change is byte-for-byte identical in state mapping to the
  display before the change (zero regressions).
- **SC-006**: With the provider stopped or uninstalled, panels over the
  affected paths show no busy cursor on activation and no measurable
  slowdown versus plain local folders.
- **SC-007**: In a folder whose path contains non-ASCII characters, 100% of
  items show base icons identical to the same items in an ASCII-only folder
  (verified with at least Word, PDF, image, and executable file types).
- **SC-008**: A file created, renamed, or deleted by another program in a
  non-ASCII-path folder appears/disappears in the panel automatically,
  within the same time as in an ASCII-only folder, with zero manual
  refreshes.

## Assumptions

- "Status icons" means the per-item badge rendered with the file/folder icon
  in the panels — the same visual concept Explorer uses on icons. Adding a
  separate "Status" *column* (as Explorer's details view has) is out of
  scope.
- Parity is defined against Windows Explorer **on the same machine**: where
  Explorer itself shows no badge (e.g. because of the system-wide limit on
  concurrent badge providers), Tandem Commander showing none is correct.
- Providers in scope for verification are OneDrive and Google Drive with
  their current desktop clients (2026); the fix is expected to be generic so
  other providers registered with Windows benefit, but only these two are
  explicitly tested.
- Displaying status is the whole scope; commands that act on the status
  (e.g. "keep offline", "free up space") are out of scope, and so is any new
  configuration UI — the existing icon-overlay settings govern cloud badges
  (see FR-011).
- The application targets Windows 11+ per the constitution; behavior on older
  Windows is out of scope.
- The user's mention of "Tandem Salamander" refers to Tandem Commander (this
  product).

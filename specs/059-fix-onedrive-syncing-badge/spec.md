# Feature Specification: Sync-In-Progress Badge Parity with Explorer

**Feature Branch**: `059-fix-onedrive-syncing-badge`
**Created**: 2026-08-18
**Status**: Draft
**Input**: User description: "V rámci feature 58 se již badge overlay zobrazují jako v původním Altap Salamamnderu. Ale i v půdodním Altap Salamander byl např. problém při zobrazení badge ikony synchronizace (modré šipky), které se v Exploreru zobrazují, ale v Tandem Commanderu ne, viz např. aktuálně adresář \"E:\Simopt, s.r.o\HSČR - Dokumenty\Schůzky\2026_08_19\". Může to být ale nějaký problém obecně celé synchronizace OneDrive, protože mi spíše přijde, že tam ta sychronizace \"visí\" a nic reálně neprobíhá, ale Explorer ukazuje modré šipky a Tandem COmmander nic. Detailně to celé analyzuj a prověř a navrhni šetrné řešení tak, aby to fungovalo stejně jako Explorer."

## Problem Statement

Feature 058 restored sync-status badges in Tandem Commander to the level of
the original Altap Salamander: synced, online-only and error states now
display. One state, however, was **already missing in Altap Salamander and is
still missing**: the **sync-in-progress badge** (blue circular arrows).
Windows Explorer shows it — currently, for example, on the folder
`E:\Simopt, s.r.o\HSČR - Dokumenty\Schůzky\2026_08_19` (a OneDrive/SharePoint
synced library with Files-On-Demand) — while Tandem Commander shows either no
badge or a different state for the same item at the same moment.

The user also observes that the OneDrive synchronization of that location
appears to be **stuck**: Explorer has shown the blue arrows for an extended
time with no actual transfer visibly progressing. That raises two distinct
possibilities the analysis must separate:

1. Explorer obtains the "syncing" state through a channel Tandem Commander
   does not consult (so the product misses a real, reportable state), and/or
2. the provider itself is in a long-lived "pending" state (so the blue
   arrows are truthful in Explorer, and Tandem Commander's absence of any
   badge is the defect — not the arrows' longevity).

The requested outcome is explicitly conservative ("šetrné řešení"): a
minimal, low-risk change that makes Tandem Commander display the same
sync-state badge Explorer displays for the same item at the same moment — no
redesign of the badge pipeline restored in feature 058.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Sync-in-progress badge matches Explorer (Priority: P1)

A user has a OneDrive/SharePoint-synced folder in which items are currently
being synchronized (or are queued/pending). Windows Explorer shows the blue
sync-in-progress arrows on those items. The user opens the same folder in a
Tandem Commander panel and sees the same sync-in-progress badge on the same
items.

**Why this priority**: This is the one remaining state where the product
visibly disagrees with Explorer — the user cannot tell from the panel that an
item is not yet fully synchronized, which is exactly the risk case (working
with a file whose cloud copy is stale or whose upload is pending).

**Independent Test**: With the reported folder (or any folder where Explorer
currently shows blue arrows) open side-by-side in Explorer and a panel:
item-by-item badge comparison shows the same state in both, including
"syncing".

**Acceptance Scenarios**:

1. **Given** an item on which Explorer shows the sync-in-progress badge,
   **When** the user lists its folder in a panel, **Then** the panel shows
   the sync-in-progress badge on that item.
2. **Given** an item whose state transitions (syncing → synced, or synced →
   syncing after an edit), **When** the panel refreshes (automatic change
   notification or manual refresh), **Then** the badge follows the new state
   as Explorer does.
3. **Given** a provider whose sync is stalled in a pending state for a long
   time (Explorer keeps showing blue arrows), **When** the user views the
   folder in a panel, **Then** the panel keeps showing the same
   sync-in-progress badge for as long as Explorer does — parity is with
   Explorer's display, not with an assumption about how long syncing "should"
   take.

---

### User Story 2 - Diagnosis of the reported location (Priority: P2)

The user wants to know what is actually happening with the reported folder:
whether the long-lived blue arrows reflect a genuinely stuck OneDrive
synchronization (provider-side condition) or a display artifact. The feature's
analysis phase examines the reported location and records a clear finding:
which mechanism carries the "syncing" state there, whether the state is real,
and what (if anything) a user can do about a stuck state.

**Why this priority**: The user explicitly asked for a thorough check of the
whole situation ("může to být problém obecně celé synchronizace OneDrive").
The finding directs the fix and tells the user whether their OneDrive needs
attention independently of Tandem Commander.

**Independent Test**: The feature's analysis document contains a reproducible
determination for the reported folder: the mechanism Explorer uses for the
badge there, the item states observed, and whether the pending state is
provider-real; each claim is backed by an observable check that can be
re-run.

**Acceptance Scenarios**:

1. **Given** the reported folder, **When** the analysis is complete, **Then**
   it names the channel Explorer's blue arrows come from on that machine and
   whether Tandem Commander can read the same channel.
2. **Given** the analysis outcome, **When** the provider state is found to be
   genuinely stuck, **Then** the finding says so explicitly (with the
   evidence), so the user knows the arrows are truthful and the OneDrive
   client — not the file manager — is the thing to nudge.

---

### User Story 3 - No regression of feature-058 badge behavior (Priority: P2)

All badge behavior delivered by feature 058 keeps working unchanged: synced /
online-only / error badges in OneDrive and Google Drive locations (ASCII and
non-ASCII paths), progressive loading, no UI blocking, the icon-overlay
configuration switch, and unchanged behavior in folders with no provider.

**Why this priority**: The requested solution is explicitly conservative; the
newly working feature-058 behavior must not be traded for the one extra
state.

**Independent Test**: Re-run the feature-058 validation set (its
`evidence.md` scenarios) after the change: identical outcomes, plus the new
syncing state.

**Acceptance Scenarios**:

1. **Given** the feature-058 validation scenarios, **When** they are re-run
   after this change, **Then** all pass with unchanged results.
2. **Given** a folder with no cloud provider, **When** it is listed, **Then**
   panel behavior and performance are unchanged.

---

### Edge Cases

- **Transient states**: sync-in-progress is inherently short-lived under
  normal operation; the acceptance comparison must be made while Explorer
  still shows the state (the stalled folder is the stable repro). A state
  that ends between the panel's read and the comparison is not a failure —
  scenario 2 of US1 (transition follow-up) covers freshness.
- **Directory vs. file badges**: Explorer shows the syncing badge on folders
  whose *contents* are pending, not only on the files themselves (the
  reported case is a folder). Parity must hold for both.
- **Pinned/online-only interplay**: the reported items carry "always keep on
  this device" marks while content is still pending — i.e. multiple states
  can be true at once; the displayed badge must match Explorer's precedence,
  whatever it shows.
- **Provider restart mid-view**: if the OneDrive client restarts while the
  folder is displayed, badges follow feature-058 recovery semantics (next
  listing/refresh; no background polling).
- **Stuck provider**: a provider that never leaves the pending state must not
  cause repeated re-reading loops, UI blocking, or growing resource use in
  the panel — the badge simply stays.
- **Other providers**: if the syncing state for Google Drive mounted drives
  turns out to flow through a different channel than OneDrive's, the fix for
  OneDrive must not break the Google Drive states restored in feature 058;
  Google Drive syncing-state parity is in scope only where Explorer itself
  shows it on that machine.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: For every item on which Windows Explorer displays a
  sync-in-progress badge, a Tandem Commander panel listing the same folder at
  the same time MUST display the sync-in-progress badge on the same item.
- **FR-002**: Badge parity MUST hold for both files and folders (Explorer
  marks folders whose contents are pending).
- **FR-003**: When an item's sync state changes, the panel's badge MUST
  follow on the next automatic change notification or manual refresh —
  matching the freshness feature 058 established for the other states.
- **FR-004**: The solution MUST be conservative with respect to feature 058:
  the existing badge pipeline's behavior for all other states, its
  progressive/non-blocking loading, its configuration switch (icon overlays
  on/off), and its no-provider behavior MUST remain unchanged.
- **FR-005**: A provider stuck in a pending state MUST NOT degrade the panel:
  no repeated futile loading, no UI blocking, no unbounded resource growth —
  the badge persists exactly as long as Explorer's does.
- **FR-006**: The analysis MUST produce a recorded, re-runnable determination
  for the reported location (`E:\Simopt, s.r.o\HSČR - Dokumenty\Schůzky\…`):
  which channel carries Explorer's blue arrows there, whether the pending
  state is provider-real, and the resulting user guidance if the provider is
  stuck.
- **FR-007**: If the analysis proves the state is not obtainable by a
  third-party application through any supported channel, the feature MUST
  record that finding with evidence and stop at the analysis (documented
  limitation) rather than ship an unreliable approximation — parity claims
  the product cannot keep are worse than a documented gap.

### Key Entities

- **Sync-in-progress state**: the provider-reported condition "content
  transfer pending or running" for an item; distinct from synced,
  online-only, and error; may legitimately persist for a long time when the
  provider stalls.
- **State channel**: the mechanism through which Explorer learns an item's
  sync state on a given sync root (the analysis determines which one carries
  the blue arrows for the reported location and whether it is readable by
  other applications).
- **Sync root**: a folder subtree managed by a provider with per-item states
  (the reported location is a OneDrive/SharePoint library with
  Files-On-Demand).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On the reported folder (while Explorer shows blue arrows),
  item-by-item comparison shows the same sync-in-progress badge in the panel
  for 100% of the items Explorer marks — including the folder itself when
  Explorer marks it.
- **SC-002**: Across a full sync cycle induced on a test file (edit → upload
  → settled), the panel shows the same badge sequence as Explorer, each state
  appearing on the panel's next refresh/notification after Explorer shows it.
- **SC-003**: The complete feature-058 validation set re-runs with unchanged
  results (zero regressions).
- **SC-004**: With the provider stalled for one hour, panel CPU usage and
  memory attributable to badge handling remain flat (no growth, no periodic
  busy cursor).
- **SC-005**: The analysis record for the reported location exists, names the
  state channel, states whether the stall is provider-real, and each finding
  carries a re-runnable check (a reader can reproduce it on the same
  machine).

## Assumptions

- Parity is defined against Windows Explorer on the same machine and Windows
  build; states Explorer itself does not show are out of scope.
- The reported location is a OneDrive/SharePoint-synced library using
  Files-On-Demand (confirmed during specification: the folder is a
  cloud-placeholder directory carrying the "always keep on this device"
  mark) — it is the primary test bed; a generic fix benefiting other
  providers is welcome but only OneDrive-family and (for no-regression)
  Google Drive locations are explicitly verified.
- "Šetrné řešení" (conservative solution) means: minimal change on top of the
  feature-058 pipeline, no new configuration, no redesign; if a trade-off
  arises between covering exotic cases and keeping the change small, small
  wins and the gap is documented.
- Diagnosing/unsticking the OneDrive client itself is analysis output (user
  guidance), not product code: Tandem Commander displays states, it does not
  manage the provider's sync queue.
- Feature-058 semantics (recovery on next listing, no background polling,
  existing icon-overlay configuration governs badges, 2-second visibility
  target for on-screen items) carry over unchanged to the new state.

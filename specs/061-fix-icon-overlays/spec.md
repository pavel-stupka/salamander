# Feature Specification: Restore General Shell Icon Overlay Support

**Feature Branch**: `061-fix-icon-overlays`
**Created**: 2026-08-19
**Status**: Draft
**Input**: User description: "Cílem této opravy je jasně a detailně analyzovat celý systém fungování překryvů pro ikony a opravit fungování tak, aby fungovalo správně. V minulých úpravách se opravilo fungování překryvů pro OneDrive a Google Drive, ale obecně překryvy nefungují — např. TortoiseGit překryvy ikon se vůbec nezobrazují. V původním Altap Salamander to fungovalo správně. Detailně analyzuj systém fungování pro překryvy ikon, identifikuj chyby a navrhni a implementuj opravy tak, aby vše fungovalo — tj. nejen OneDrive a Google Drive, ale obecně systém překryvů ikon. Úprava NESMÍ zanést nové, regresní, chyby."

## Problem Statement

Windows lets any installed application (version-control clients such as TortoiseGit,
cloud storage providers, backup tools, …) register an *icon overlay handler* — a small
badge drawn over a file's icon that communicates per-file status (modified, up to date,
conflicted, synced, …). File Explorer shows these badges; Tandem Commander has the same
capability inherited from Open Salamander, and recent work (features 058 and 059) fixed
the OneDrive and Google Drive badges specifically.

The reported defect: **overlays from other providers do not appear at all** — the
canonical example is TortoiseGit, whose status badges are visible in File Explorer on
the same machine and the same folders, and were visible in the original Altap Salamander,
but are absent in Tandem Commander panels. The overlay system must be analyzed end to
end (handler discovery, selection, per-file querying, refresh on change, user
configuration), each defect identified with evidence, and fixed so that overlays work
*generally* — for any properly registered handler — not only for the two cloud
providers addressed previously. The fix must not regress anything that works today,
in particular the OneDrive/Google Drive badge behavior delivered by features 058–059.

Preliminary code exploration (recorded in `investigation-leads.md` in this feature
directory) identified several candidate root causes; the analysis phase must confirm or
refute each with evidence before any fix is implemented.

## Clarifications

### Session 2026-08-19

- Q: When Tandem Commander can display a badge from a healthy provider that File
  Explorer no longer shows (Explorer has a lower effective cap on concurrent overlay
  providers), should the badge be displayed, or is strict two-way parity with Explorer
  required? → A: Explorer is the **floor**, not an exact template — every badge
  Explorer shows must also appear in Tandem Commander; an *additional* badge from a
  healthy provider is allowed (and documented in the analysis report). Parity criteria
  are measured one-directionally (Explorer ⇒ Tandem Commander).
- Q: When the analysis confirms that existing configurations have overlays silently
  disabled because the stored overlay settings are absent (typically after the
  feature-057 Altap settings migration), should the fix heal those configurations
  automatically? → A: Yes — **absent settings mean the factory default "overlays
  enabled"**. This heals silently broken configurations without overriding any
  explicitly stored user choice (an explicit "disabled" in storage stays respected).
  Fixing the feature-057 migration tool itself is not required by this decision.
- Q: What diagnostic level should provider-load failures have in the Release build,
  where today nothing is visible at all? → A: **Debug-trace diagnostics only** —
  failures (provider name and reason) must be discoverable in the project's debug
  diagnostics; no new Release-visible UI, log file, or translated strings are in scope
  for this fix. The configuration page continues to list detected providers (FR-006),
  and machine-specific causes are covered by the analysis report (FR-001).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Version-control status badges appear (Priority: P1)

A developer keeps Git working copies on disk and uses TortoiseGit. When they browse a
working copy in a Tandem Commander panel, every file and folder shows the same status
badge (up to date, modified, added, conflicted, ignored, …) that File Explorer shows
for the same item at the same moment.

**Why this priority**: This is the reported defect and the core value of the feature —
overlay badges from any registered provider, not just the two cloud vendors, must work.
TortoiseGit is the concrete reproduction case and acceptance vehicle.

**Independent Test**: On a machine with TortoiseGit installed, open a Git working copy
containing a mix of clean, modified, and untracked files side by side in Tandem
Commander and File Explorer, and compare badges item by item.

**Acceptance Scenarios**:

1. **Given** a machine where File Explorer shows TortoiseGit badges in a working copy,
   **When** the same folder is listed in a Tandem Commander panel, **Then** every item
   that carries a badge in Explorer carries the equivalent badge in the panel.
2. **Given** a working copy in a path containing non-ASCII characters (e.g. diacritics),
   **When** it is listed in a panel, **Then** badges appear exactly as they do for an
   all-ASCII path.
3. **Given** the analysis identifies a machine-specific blocker (e.g. a handler disabled
   by an old crash record, or more providers registered than the system-wide badge
   limit allows), **Then** the behavior matches File Explorer's on the same machine and
   the cause is documented in the analysis report, so "no badge" is never an unexplained
   silent failure of Tandem Commander itself.

---

### User Story 2 - Badges stay current as file status changes (Priority: P2)

While a panel stays open on a working copy, the user edits a file (status becomes
"modified"), commits it (status returns to "up to date"), or reverts it. The badge on
that file updates automatically, without the user forcing a manual refresh — matching
how File Explorer behaves.

**Why this priority**: Some providers report status asynchronously — the first answer
may be "no badge" and the real state arrives via a change notification moments later.
If the refresh path is broken, badges appear stale or never appear at all, so this
story is inseparable from a *reliable* fix of Story 1, yet independently testable.

**Independent Test**: With a working copy visible in a panel, modify a clean file from
another program and observe the badge change without manual intervention; repeat in a
folder whose path contains non-ASCII characters.

**Acceptance Scenarios**:

1. **Given** a clean file visible in a panel, **When** it is modified externally,
   **Then** its badge changes to the provider's "modified" badge without a manual
   refresh, within the same time frame Explorer needs on that machine.
2. **Given** a panel open on a path containing non-ASCII characters, **When** an item's
   provider status changes, **Then** the badge updates just as it does for ASCII paths.

---

### User Story 3 - Existing cloud badges keep working (regression guard) (Priority: P2)

A user of OneDrive and Google Drive continues to see everything features 058 and 059
delivered: sync-state badges (including the "sync in progress" blue-arrows badge),
correct file icons and automatic refresh in non-ASCII paths.

**Why this priority**: The user's explicit hard constraint is that this fix must not
introduce regressions. The recently repaired cloud behavior is the highest-risk
neighborhood of this change.

**Independent Test**: Re-run the acceptance scenarios of features 058 and 059 on the
build containing this fix.

**Acceptance Scenarios**:

1. **Given** a OneDrive folder with items in synced, pending, and error states,
   **When** it is listed in a panel, **Then** each state's badge matches what feature
   059 delivered (parity with Explorer, including the pending "blue arrows" badge).
2. **Given** a Google Drive folder in a non-ASCII path (e.g. `G:\Můj disk`), **When**
   it is listed, **Then** sync badges, file icons, and automatic refresh all behave as
   feature 058 delivered.

---

### User Story 4 - User control over overlay providers is preserved (Priority: P3)

A user opens the configuration page for icon overlays, sees the list of overlay
providers actually detected on their system, and can disable individual providers or
all of them. Their choices survive restarts and are honored by the panels.

**Why this priority**: The control surface already exists and interacts with the defect
(a provider disabled here — including automatically after a past crash — shows no
badges by design). It must keep working and must reflect reality, but it is not the
reported defect itself.

**Acceptance Scenarios**:

1. **Given** the icon-overlay configuration page, **When** it is opened on a machine
   with TortoiseGit and a cloud provider installed, **Then** the detected providers are
   listed, and the set shown is consistent with the providers whose badges the panels
   can actually display.
2. **Given** a provider is unchecked and the application restarted, **When** a folder
   served by that provider is listed, **Then** its badges are absent while other
   providers' badges still appear.

---

### Edge Cases

- More overlay providers are registered than the system-wide badge limit allows
  (Windows itself caps concurrently usable overlay badges; Explorer resolves the
  contention by a documented priority order). Tandem Commander's selection must never
  drop a provider that File Explorer displays on the same machine; serving additional
  healthy providers beyond Explorer's effective cutoff is acceptable (Explorer-as-floor
  semantics per Clarifications).
- A provider was automatically disabled after a past crash inside its handler. The
  disable must keep protecting the user, but the analysis must confirm the mechanism
  cannot silently suppress a healthy provider, and the configuration page must let the
  user re-enable it.
- A provider fails to load (broken registration, missing component, provider refuses
  because it lost the system-wide priority contest). The failure must not destabilize
  the application, and the cause must be discoverable in the project's debug
  diagnostics rather than fully silent.
- Paths at or beyond the classic Windows path-length limit: overlay reading is
  deliberately skipped there today; this boundary behavior must be preserved and
  documented, not accidentally changed.
- Panel paths and provider-supplied resources containing characters outside the
  system's legacy code page (the exact area features 004/058 worked in) must not break
  badge display, badge refresh, or provider loading.
- A provider that answers "no badge" first and delivers the real status via a later
  change notification (asynchronous providers) must still end up showing the badge.
- A user profile where the overlay settings are entirely absent (fresh install, or a
  profile produced by the feature-057 Altap settings migration, which does not carry
  the overlay values). Overlays must come up enabled — absent settings mean the
  factory default, never a silent global disable.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The feature MUST produce a written end-to-end analysis of the icon
  overlay system — provider discovery, provider selection under the badge limit,
  per-item badge querying, badge refresh on change notifications, and user
  configuration — with each identified defect backed by evidence (reproduction,
  trace, or code-path proof) and each previously suspected cause explicitly confirmed
  or refuted. The analysis is a reviewable artifact in this feature's directory.
- **FR-002**: Overlay badges from any properly registered, healthy overlay provider
  MUST appear on panel items. File Explorer on the same machine is the floor: every
  item that carries a badge in Explorer MUST carry the equivalent badge in the panel;
  an additional badge from a healthy provider that Explorer's lower effective cap
  excluded is permitted. TortoiseGit is the mandatory acceptance case.
- **FR-003**: When more providers are registered than the system-wide badge limit
  allows, Tandem Commander MUST NOT lose any provider that File Explorer displays on
  the same machine (same-or-better selection); it MAY additionally serve healthy
  providers beyond Explorer's effective cutoff, and the analysis report MUST document
  the selection rule used.
- **FR-004**: Badge updates triggered by provider change notifications MUST work for
  every panel path the application can display, regardless of the characters the path
  contains.
- **FR-005**: All behavior delivered by features 058 and 059 (OneDrive/Google Drive
  badges, sync-pending badge, icons and auto-refresh in non-ASCII paths) MUST remain
  intact, verified by re-running those features' acceptance scenarios.
- **FR-006**: The existing user controls — the global overlay enable switch, the
  per-provider disable list, and the automatic disable offered after a provider
  crash — MUST keep their current semantics and storage, and the configuration page
  MUST list the providers actually detected on the system.
- **FR-007**: Provider loading or querying failures MUST be discoverable through the
  project's standard debug diagnostics (naming the provider and the reason), and MUST
  never crash or hang the application; a misbehaving provider degrades only its own
  badges. Per Clarifications, no new Release-visible diagnostic surface (UI element,
  log file, or translated string) is in scope for this fix.
- **FR-008**: The change MUST NOT alter the display of the application's built-in
  status marks (shortcut arrow, shared hand, offline clock), which are a separate
  mechanism and out of scope.
- **FR-009**: When the stored overlay configuration values are absent (e.g. a fresh
  profile or one produced by the feature-057 Altap settings migration), the system
  MUST behave as the factory default — overlays enabled, no providers disabled. An
  explicitly stored user choice (global disable or per-provider disable) MUST remain
  respected; healing applies only to *absent* values, never to stored ones.

### Key Entities

- **Overlay provider (handler)**: A third-party component registered with Windows that
  claims files/folders and supplies a badge. Attributes: registration name (which also
  determines its priority in the system-wide contest), display name, enabled/disabled
  state in Tandem Commander's configuration.
- **Badge slot**: One of the limited number of concurrently usable overlay badges the
  platform allows; providers compete for slots by registration-name order.
- **Disabled-provider list**: The user's (or crash handler's) persisted set of
  providers Tandem Commander must ignore.
- **Panel item badge state**: The per-file/per-folder answer "which badge, if any" —
  produced on listing and refreshed when providers signal a change.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a machine where File Explorer shows TortoiseGit badges, an item-by-item
  comparison over a working copy containing at least the clean, modified, and untracked
  states shows that 100% of items badged in File Explorer carry the equivalent badge in
  Tandem Commander panels (Explorer-as-floor; additional badges from healthy providers
  are acceptable and documented), in both ASCII and non-ASCII paths.
- **SC-002**: After a file's provider status changes while its panel is open, the badge
  updates without any manual user action, in 100% of attempts across ASCII and
  non-ASCII paths (allowing only the provider's own reporting delay, measured against
  Explorer on the same machine).
- **SC-003**: All acceptance scenarios of features 058 and 059 pass unchanged on the
  fixed build (0 regressions in cloud badges, icons, and auto-refresh).
- **SC-004**: The analysis report enumerates every candidate cause raised before or
  during the work and marks each one confirmed (with the fix that addresses it) or
  refuted (with the evidence) — no candidate left unresolved.
- **SC-005**: Disabling a provider in configuration removes exactly that provider's
  badges after restart; re-enabling restores them — verified for at least one
  version-control and one cloud provider.
- **SC-006**: The full existing verification suite for a release-quality change (build
  of Debug and Release, existing automated tests) passes with 0 new failures.
- **SC-007**: On a user profile whose overlay settings are absent (e.g. produced by the
  feature-057 Altap settings migration), overlay badges appear on first start without
  the user touching configuration; an explicitly disabled provider or a global
  "overlays off" choice stored before the update remains disabled after it.

## Assumptions

- The system-wide overlay badge limit is a Windows platform property shared by all
  applications including File Explorer. Explorer-as-floor is the correctness target
  (see Clarifications): every badge Explorer shows must appear; additional badges from
  healthy providers beyond Explorer's effective cutoff are permitted, not required.
- "It worked in original Altap Salamander" is treated as a credible report that generic
  overlays must work, not as proof that the historical machine had the same set of
  providers registered; today's machines carry many more cloud providers competing for
  the limited badge slots, and the analysis must separate environmental causes from
  code defects.
- Reference behavior is File Explorer on the same machine, same session, same folders.
  Providers that fail identically in Explorer (e.g. a provider whose component cannot
  load on the machine at all) are environmental, not defects of this feature.
- The acceptance machine has TortoiseGit installed with default settings and at least
  one cloud provider (OneDrive or Google Drive) active, so contention for badge slots
  is realistic.
- The existing configuration storage location and format for overlay settings remain
  unchanged (a maintenance release must not move configuration, per the constitution).
- Built-in status marks (shortcut arrow, shared hand, offline clock) use a separate
  mechanism and are out of scope except for the guarantee that they keep working where
  they work today.
- Preliminary code exploration notes with concrete candidate causes are recorded in
  `investigation-leads.md` in this feature directory as input for the planning phase.

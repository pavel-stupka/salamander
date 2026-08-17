# Feature Specification: Altap Salamander Settings Migration Utility

**Feature Branch**: `057-altap-settings-migration`
**Created**: 2026-08-17
**Status**: Draft
**Input**: User description: "Cilem rozsireni je priprava windows skriptu pro prenos nastaveni z puvodniho Altap Salamander do Tandem Commander. Bude se jednat o jednorazovou utilitku - jeden spustitelny skript pro Windows, pomoci ktereho bude moci uzivatel prenest nastaveni ze sveho puvodniho Altap Salamanderu do noveho Tandem Commanderu / jako jsou adresarove zalozky, nastaveni FTP pripojeni atd.. proste vse co bude mozne prenest. Skript by mel umoznost uzivateli selektivne zvolit, jaka nastaveni se maji prenest a jaka ne."

## Constitutional Boundary

The Tandem Commander constitution (Principle II) forbids the **application**
from ever reading or writing its predecessors' configuration — that rule is
unchanged by this feature. This utility is a **separate, standalone,
user-initiated tool**: the user runs it deliberately, exactly once (re-running
is safe), outside the application. The application itself is not modified in
any way, never invokes the utility, and remains unaware of it. The utility
reads the predecessor's settings strictly read-only and writes only to
Tandem Commander's own settings store.

## Clarifications

### Session 2026-08-17

- Q: When a selected category already has content in Tandem Commander, what
  should the transfer do with that existing content? → A: Replace the category
  wholesale with the source's content (backup taken first is the undo);
  unselected categories untouched.
- Q: How should the user interact with the utility — what form does the
  category selection and confirmation take? → A: Interactive console wizard
  (detected source → category checklist with item counts → explicit
  confirmation → summary), launchable by double-click, no command-line
  knowledge required.
- Q: How should the migration utility reach users — where do they get it
  from? → A: Repository only: a single file in a new top-level `utils/`
  directory of the source repository; users download that one file and run it
  on their PC. Not shipped by the installer, not bundled into the
  installation tree.
- Q: Which Altap Salamander versions must the utility accept as a migration
  source? → A: Every per-user configuration it finds (2.5x–4.0 era, including
  Servant Salamander–branded ones), best effort: newest preselected,
  categories a version lacks simply aren't offered.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Selective one-shot settings transfer (Priority: P1)

A long-time Altap Salamander user installs Tandem Commander and wants to keep
the working environment built up over years — directory hot paths, FTP
connection bookmarks, user menu commands, viewer/editor associations, colors,
and other preferences — without re-entering everything by hand. They run the
migration utility, which finds their existing Altap Salamander settings, shows
a list of transferable setting categories (with the number of items found in
each), lets them tick the categories they want, and copies the selected
categories into Tandem Commander. When they next start Tandem Commander, the
selected settings are in place.

**Why this priority**: This is the entire purpose of the feature — without a
working selective transfer there is no product. Lowering the switching cost
from Altap Salamander is a direct adoption driver for Tandem Commander.

**Independent Test**: On a machine with an existing Altap Salamander
configuration and a fresh Tandem Commander installation, run the utility,
select only "hot paths" and "FTP bookmarks", complete the transfer, start
Tandem Commander and verify both categories appear exactly as they were in
Altap Salamander while all other Tandem Commander settings remain at defaults.

**Acceptance Scenarios**:

1. **Given** an Altap Salamander configuration with 7 hot paths and 12 FTP
   bookmarks and a default Tandem Commander configuration, **When** the user
   selects only those two categories and completes the transfer, **Then**
   Tandem Commander shows the 7 hot paths and 12 FTP bookmarks and no other
   setting differs from its previous state.
2. **Given** the category selection screen, **When** the user deselects a
   category, **Then** nothing from that category is written to Tandem
   Commander.
3. **Given** a completed transfer, **When** the user reads the closing
   summary, **Then** every selected category is listed as transferred,
   partially transferred (with skipped items named), or skipped, each with a
   stated reason.
4. **Given** an Altap Salamander configuration where a category is empty or
   absent (e.g., no user menu defined), **When** the utility lists categories,
   **Then** that category is shown as empty/unavailable and cannot produce a
   misleading "transferred" result.

---

### User Story 2 - Safe by default: backup, no source changes, restore path (Priority: P2)

A cautious user who already spent time configuring Tandem Commander wants to
pull in the remaining settings from Altap Salamander without risking what they
already set up. Before anything is written, the utility saves a complete
backup of the current Tandem Commander settings and tells the user where the
backup is and how to restore it. The original Altap Salamander settings are
never touched, so the old application keeps working unchanged regardless of
the outcome.

**Why this priority**: Settings corruption or silent loss would destroy trust
at the exact moment a new user is evaluating the product. The constitution's
rationale for identity separation is protecting users from cross-corruption of
settings; this utility must uphold that spirit.

**Independent Test**: Run a migration over a customized Tandem Commander
configuration, then restore the backup and verify the configuration is
byte-for-byte identical to the pre-migration state; verify the Altap
Salamander configuration is byte-for-byte unchanged after any run.

**Acceptance Scenarios**:

1. **Given** any migration run that will write at least one value, **When**
   writing begins, **Then** a complete backup of the current Tandem Commander
   settings already exists and its location and restore procedure have been
   shown to the user.
2. **Given** a completed or failed migration, **When** the user restores the
   backup, **Then** Tandem Commander's settings are exactly as they were
   before the run.
3. **Given** any run of the utility (successful, failed, or cancelled),
   **When** the Altap Salamander configuration is compared to its state before
   the run, **Then** it is unchanged.
4. **Given** Tandem Commander or Altap Salamander is currently running,
   **When** the user starts a transfer, **Then** the utility refuses to write
   and explains that both applications must be closed first (Tandem Commander
   saves its settings on exit and would overwrite the transferred data).
5. **Given** a migration interrupted mid-write (e.g., forced termination),
   **When** the user restores the backup, **Then** Tandem Commander starts
   with its pre-migration settings intact.

---

### User Story 3 - Multiple Altap Salamander versions on one machine (Priority: P3)

A user who upgraded Altap Salamander over the years has settings from more
than one version on the machine (e.g., 3.1 and 4.0). The utility lists every
Altap Salamander configuration it finds, preselects the newest one, and lets
the user choose which one to migrate from.

**Why this priority**: Common enough among long-time users to matter, but a
machine with a single configuration — the majority case — is fully served by
Stories 1 and 2.

**Independent Test**: On a machine with settings from two Altap Salamander
versions, run the utility and verify both are offered, the newest is the
default, and choosing the older one migrates that one's data.

**Acceptance Scenarios**:

1. **Given** configurations from two Altap Salamander versions, **When** the
   utility starts, **Then** both are listed with version identification and
   the newest is preselected.
2. **Given** the user picks the older configuration, **When** the transfer
   completes, **Then** the transferred data comes from the older
   configuration only.
3. **Given** no Altap Salamander configuration exists on the machine,
   **When** the utility starts, **Then** it says so clearly and exits without
   changing anything.

---

### User Story 4 - Repeatable, predictable re-run (Priority: P3)

A user migrates hot paths only, uses Tandem Commander for a week, then decides
they also want their FTP bookmarks and color scheme. They run the utility
again. Previously migrated categories can be re-selected (a re-selected
category is replaced with the source's current content) and newly selected
categories transfer normally; categories left unselected are untouched.

**Why this priority**: "One-shot" describes intent, not a technical lock —
users change their minds, and a second run must never make things worse or
duplicate data.

**Independent Test**: Run the utility twice — first hot paths only, then FTP
bookmarks only — and verify hot paths from the first run survive the second
run and FTP bookmarks arrive without duplicating anything.

**Acceptance Scenarios**:

1. **Given** a previous migration of category A, **When** the user re-runs the
   utility and selects only category B, **Then** category A's migrated
   content is untouched and category B transfers normally.
2. **Given** a previous migration of category A, **When** the user re-runs the
   utility and selects category A again, **Then** the result equals the
   source's content once — no duplicated entries.

---

### Edge Cases

- **Tandem Commander never started yet** (no settings stored at all): the
  transfer must still work, and the next Tandem Commander start must accept
  the migrated settings rather than resetting or rejecting them.
- **Category exists in the source but its content cannot be represented in
  Tandem Commander** (format from a different product generation, plugin not
  shipped by Tandem Commander — e.g., settings of a plugin that Tandem
  Commander excludes): the category or item is skipped and named in the
  summary with the reason; nothing is guessed or partially written.
- **FTP passwords protected by a master password**: encrypted secrets that
  cannot be made usable without the user's master password are transferred
  only in a form the user can unlock with that same master password, and the
  summary tells the user the master password carries over; if that is not
  achievable for an item, the item's connection data transfers without the
  secret and the summary says the password must be re-entered.
- **Settings pointing into the Altap Salamander installation folder** (e.g., a
  viewer or user-menu command referencing a bundled tool): transferred
  unchanged but flagged in the summary, because the referenced program will
  disappear if Altap Salamander is uninstalled.
- **Very old source versions** (2.5x era) that lack whole categories: only the
  categories actually present are offered.
- **Interrupted run** (crash, forced close, power loss): the source is
  untouched by design; the destination is recoverable from the backup taken
  before writing began.
- **Insufficient permissions or blocked settings store**: the utility reports
  the failure plainly and makes no partial, unreported changes.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The utility MUST be a single, self-contained runnable file for
  Windows requiring no installation and no components beyond a stock
  Windows 11 system.
- **FR-002**: The utility MUST automatically discover all Altap Salamander
  per-user configurations present for the current user across the 2.5x–4.0
  era (including Servant Salamander–branded ones), identify each by product
  version, and — when more than one exists — let the user choose the source,
  defaulting to the newest.
- **FR-003**: The utility MUST run as an interactive console wizard —
  launchable by double-click, requiring no command-line knowledge — that
  presents the transferable setting categories found in the chosen source,
  each with an indication of how much content it holds (e.g., item counts),
  and lets the user independently include or exclude every category before
  anything is written.
- **FR-004**: The set of offered categories MUST cover everything that can be
  transferred faithfully, including at least: directory hot paths, FTP
  connection bookmarks, user menu commands, external viewer and editor
  associations, confirmation-prompt preferences, color scheme and panel item
  highlighting, panel view templates (column layouts), general configuration
  options, per-drive default directories, and configurations of plugins that
  Tandem Commander ships. The definitive per-category list MUST be fixed
  during planning by a compatibility analysis of each category.
- **FR-005**: The utility MUST NOT transfer anything that could put Tandem
  Commander into an invalid state, including version markers of the source
  product, licensing data, source-product file locations used internally, and
  settings of plugins Tandem Commander does not ship. Such content MUST be
  skipped and reported, never written.
- **FR-006**: The utility MUST treat the source configuration as strictly
  read-only in every code path, including failure paths.
- **FR-007**: Before its first write of a run, the utility MUST create a
  complete backup of the current Tandem Commander settings, tell the user
  where it is and how to restore it, and the restore MUST return the settings
  exactly to their pre-run state.
- **FR-008**: The utility MUST detect a running Tandem Commander or Altap
  Salamander instance before writing and MUST refuse to proceed until they
  are closed, explaining why.
- **FR-009**: Transferring a selected category MUST replace that category's
  content in Tandem Commander with the source's content (after backup);
  unselected categories MUST remain untouched. Re-running the utility MUST be
  safe and produce no duplicated entries.
- **FR-010**: For stored FTP secrets, the utility MUST transfer a secret only
  when it remains usable by the user afterwards (directly, or by entering the
  same master password they already know); otherwise the item MUST transfer
  without the secret and the summary MUST say the password needs re-entering.
- **FR-011**: The utility MUST end every run with a per-category summary —
  transferred, partially transferred (listing skipped items), or skipped —
  each with a human-readable reason, and MUST report zero categories silently.
- **FR-012**: The utility MUST clearly identify itself, the detected source
  and destination, and require an explicit user confirmation before any write.
- **FR-013**: All user-facing text of the utility MUST be in English (the
  utility is a developer-distributed one-off tool, not part of the translated
  product UI).
- **FR-014**: The Tandem Commander application, its installer, and its build
  MUST remain completely unchanged by this feature; the utility is added to
  the source repository only, as a single file in a new top-level `utils/`
  directory. The application MUST NOT gain any code path that reads
  predecessor configuration.

### Key Entities

- **Source configuration**: an Altap Salamander per-user settings store,
  identified by product version; there may be several on one machine; always
  read-only.
- **Destination configuration**: the Tandem Commander per-user settings store
  (single, version-bound); the only place the utility writes, apart from its
  backup and summary files.
- **Setting category**: a named, user-recognizable group of settings (hot
  paths, FTP bookmarks, user menu, …) that is selected, transferred, and
  reported as a unit; carries a source location, a destination location, a
  compatibility status, and an item count.
- **Migration backup**: a complete, restorable snapshot of the destination
  configuration taken before the first write of a run, with a documented
  restore procedure.
- **Migration summary**: the per-category outcome record of a run (transferred
  / partial / skipped, with reasons), shown to the user at the end.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user with an existing Altap Salamander setup completes a
  selective migration — from starting the utility to seeing the summary — in
  under 5 minutes, without manually editing any settings store.
- **SC-002**: 100% of entries in the transferred categories that the utility
  reported as "transferred" are present and functional in Tandem Commander
  (verified for hot paths and FTP bookmarks by opening each in the UI).
- **SC-003**: 0 settings the user did not select are modified, and restoring
  the backup reproduces the pre-migration destination state exactly, in 100%
  of runs including interrupted ones.
- **SC-004**: The source Altap Salamander configuration is unchanged after
  100% of runs (successful, cancelled, and failed).
- **SC-005**: 0 items are dropped silently: every non-transferred item or
  category in a selected set appears in the summary with a reason.
- **SC-006**: A second run over an already-migrated destination produces no
  duplicate entries in 100% of cases.

## Assumptions

- The migration source is an Altap Salamander per-user configuration (any
  version from the 2.5x–4.0 era) belonging to the same Windows user on the
  same machine as the Tandem Commander installation. Cross-machine or
  cross-user transfer is out of scope (a user can move settings between
  machines with Altap Salamander's own export before migrating).
- Because Tandem Commander descends from the same product line, most setting
  structures are compatible; where the source product generation diverges,
  the planning-phase compatibility analysis decides per category whether to
  transfer, transform, or skip — skipping (with a reported reason) is always
  preferred over guessing.
- A selected category is replaced wholesale in the destination (after
  backup); there is no per-item merge of source and destination content
  within one category. This keeps outcomes deterministic and re-runs
  duplicate-free. (Confirmed in Clarifications, Session 2026-08-17.)
- The utility lives only in the source repository, as a single file in a new
  top-level `utils/` directory; users download that file (the repository is
  public) and run it on their PC. It is never run automatically — not by the
  installer, not by the application — and release artifacts do not include
  it. (Confirmed in Clarifications, Session 2026-08-17.)
- Window/panel geometry and transient session state (open paths, history
  lists) are low-value and MAY be excluded by the compatibility analysis
  without violating FR-004's "everything transferable" intent.
- Only per-user settings are in scope; the products store no machine-wide
  settings that would be worth migrating.
- The utility targets the currently released Tandem Commander configuration
  format (0.1 line); it does not attempt to write formats for future
  incompatible versions.

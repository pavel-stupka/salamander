# Feature Specification: Finish the Contained Encoding Fixes (feature 068 handoff)

**Feature Branch**: `069-finish-encoding-fixes`
**Created**: 2026-08-24
**Status**: Draft
**Input**: User description: "Nacti dokument z posledniho commitu, resp. posledni
realizovane feature: ./specs/068-encoding-regression-review/REMAINING-WORK.md
A na jeho zaklade navrhni a realizuj implementaci vsech zbyvajicich oprav.
Zasadni pravidlo je, ze nesmis zanest zadne regresni chyby, ktere by zpusobily
nefunkcnost programu."

## Background

Feature 068 reviewed the whole core application for text-encoding defects,
raised 76 findings, had 60 of them confirmed by independent refute-first
verifiers, and fixed 9 (X01–X09). Its handoff document
(`specs/068-encoding-regression-review/REMAINING-WORK.md`) lists what is left:

- **34 confirmed, contained defects** (section 1) — each verified, each with a
  recorded failure scenario, each fixable without redesigning shared machinery,
  the same shape as the nine already fixed. One of them (F-P5-06) is a
  documentation gap, not a code defect.
- **5 small deferred items D01–D05** (section 3) — two plugin-local leftovers in
  the file comparator, one plugin item that crosses into the core, one build
  problem in the developer trace tool, one broken developer script.
- **17 systemic findings in 5 clusters B-1–B-5** (section 2) — each declared
  "its own feature" by the review because no minimal fix exists.

This feature delivers the first two groups under one governing rule set by the
user: **no fix may make anything that works today stop working.** Three of the
nine 068 fixes were rejected by review before landing, all three because the
fix itself introduced a regression that no build, test or static guard would
have caught — the process that caught them is therefore a requirement here,
not an option.

## Scope

| In scope | Out of scope |
|---|---|
| The 34 items of handoff section 1 (33 code defects + 1 documentation gap), each fixed to exactly the extent its verifier confirmed | The 5 systemic clusters B-1–B-5 (ANSI dialog windows; code-page byte tables behind name comparison; the undocumented error-text encoding used by plugins; name-format folding that also drives Change Case; the frozen plugin-facing text services) — each is a separate feature with its own regression matrix |
| Deferred items D01 (trace tool build), D03 and D04 (file comparator title/path bar), D05 (developer commit script) | Any change to what plugins receive from the core (bytes or interface version) |
| D02 (ZIP overwrite dialog attribute line) **only if** it can be fixed inside the ZIP plugin without changing what the core receives; otherwise it stays recorded with cluster B-5 | New functionality, refactoring of adjacent code, opportunistic cleanup |
| Items already resolved by X01–X09 on the way (F-P1-03 startup temp cleanup; the jump-list half of F-P1-25) are **verified closed**, not re-done | Releasing a version (the changelog's unreleased section is updated; the version bump is a separate decision) |
| The automated half of every gate, plus the written scenarios for the maintainer's on-screen sweep | Defects reachable only in the three disabled languages (Russian, Ukrainian, Simplified Chinese) |

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Nothing that works today stops working (Priority: P1)

A user on an English Windows with ASCII-only file names, a plugin author whose
plugin talks to the core, and a Czech or Hungarian user who relies on the
surfaces repaired by features 004–068 all upgrade to a build containing these
fixes and notice **no difference** in anything they did before — the same
text, the same bytes handed to plugins, the same behaviour on every previously
repaired surface. Only the defective scenarios listed below change.

**Why this priority**: it is the user's stated governing rule. A fix that
breaks a working surface is worse than the defect it repairs, and the 068
review showed that such regressions slip past every automated check.

**Independent Test**: build the product, run the complete unit-test suite,
the strict encoding guard, an English/ASCII byte-identity comparison against
the pre-feature build, and the recorded on-screen regression sweep (W1–W20)
in the Czech and Hungarian UI plus the English spot-check; every item must
report "unchanged" or "corrected", never "regressed".

**Acceptance Scenarios**:

1. **Given** an English UI and ASCII-only names, paths, TEMP and account name, **When** any surface touched by a fix is exercised, **Then** the text shown and the data written are byte-for-byte identical to the pre-feature build.
2. **Given** a plugin that calls any core text or formatting service, **When** the plugin is run against the new build, **Then** every byte it receives is identical to the pre-feature build and the plugin interface version is unchanged.
3. **Given** each of the on-screen regression sweep items W1–W20 recorded by feature 068, **When** it is run in the Czech UI and in the Hungarian UI, **Then** it passes exactly as it passed before this feature.
4. **Given** any fix in this feature, **When** it is reviewed by a reviewer who did not write it, following the 068 regression-reviewer charter, **Then** every affected surface the reviewer enumerates themselves is verdicted "unchanged" or "corrected", and no fix lands with a "regressed" surface.
5. **Given** a text conversion that fails at any repaired site (malformed or unrepresentable text), **When** the surface is rendered or the operation runs, **Then** the pre-feature legacy behaviour is used — text is never blanked and an operation is never silently skipped where it previously ran.

---

### User Story 2 — Everyday operations act on the real file (Priority: P1)

A user whose files, folders, TEMP directory or Windows account name contain
accented (or otherwise non-ASCII) characters performs ordinary file-manager
operations and they act on the file they see — instead of silently doing
nothing, acting on a name that does not exist, or losing an edit.

**Why this priority**: these are silent losses of data or function on a
Czech/Slovak/Hungarian/Polish machine where accented names are the norm; one
of them (the archive edit) loses the user's work with no message at all.

**Independent Test**: on the existing fixture `D:\Zkouška\Můj disk\` (and an
account/TEMP path with an accent), run each acceptance scenario below on the
new build; each must succeed where the pre-feature build fails.

**Acceptance Scenarios**:

1. **Given** `Přehled.txt` focused in a panel and the command line focused, **When** the user presses Ctrl+Enter, **Then** the command line shows `Přehled.txt` exactly (no mojibake) and pressing Enter runs the command against that file; the same holds for Ctrl+Space, Ctrl+[ and Ctrl+] inserting the panel path, and for a name dragged from an archive or plugin panel onto the command line. (F-P6-04)
2. **Given** the command line after such an insertion, **When** the user moves the caret, selects text or inserts a second name, **Then** the caret and selection land where they visibly should — the fix must not shift positions on lines containing accented characters.
3. **Given** a file with an accented name inside an archive (or an accented TEMP), **When** the user F4-edits it, saves, and uses *Copy To…* in the changed-files dialog, **Then** the edited file is copied to the chosen target; if the copy fails, the user is told, never left believing it succeeded. (F-P1-20)
4. **Given** two directory trees containing `Smlouva – kopie.docx` and a subdirectory `Účetnictví`, **When** the user runs *Commands ▸ Compare Directories* by content, **Then** the files are compared and the subdirectory is descended — no "Cannot open the file" and no "Cannot read directory" prompt per accented subdirectory. (F-P1-19)
5. **Given** `Účtenka.pdf` in Explorer, **When** the user drags it onto the command line, the status bar, the toolbar, or an open internal viewer window, **Then** the drop is accepted and the correct file is used (opened, inserted, or navigated to). (F-P1-26)
6. **Given** a Windows account name `Jiří` (so `%USERPROFILE%` contains an accent), **When** the user types `%USERPROFILE%\Desktop` into Change Directory (Shift+F7) or the command line, **Then** the panel changes to that directory; file types whose icon is registered under `%USERPROFILE%`/`%LOCALAPPDATA%` show their real icon; programs launched from the application inherit correctly encoded per-drive current directories. (F-P1-23)
7. **Given** a shortcut (`.lnk`) that sits in an accented folder, **or** that points to an accented folder, **When** the user presses Enter on it, **Then** the panel follows the shortcut into the target folder instead of treating it as a plain file; the Share dialog and the shell context menu / Properties work on names outside the system code page. (F-P1-25; the jump-list part is already fixed by X03 and is verified only)
8. **Given** an accented TEMP path or accented panel names, **When** the user (a) drags a file out of an archive or plugin panel to Explorer, (b) creates a self-extracting archive containing accented files, (c) creates a link (junction/symlink) from an accented source, (d) runs a user-menu item "through a batch file" or one using `$(DOSFullName)`/`$(DOSPath)`-family variables, (e) types a mask or name in a dialog that lists matching files, **Then** each operation completes on the right files; and (f) the post-operation refresh and the archive freshness check behave as on ASCII paths (no needless re-listing), and (g) temporary files created for plugin viewers are cleaned up. (F-P1-21, all nine site groups; group (g) only if the bytes handed to the plugin are provably unchanged)
9. **Given** a user-menu item whose icon is taken from `D:\Programy\Můj nástroj\tool.exe`, **When** the User Menu or its toolbar is shown, **Then** the real icon appears (the icon picker already shows it correctly). (F-P1-22)

---

### User Story 3 — The application finds its own locations under accented names (Priority: P1)

A user whose Windows account name, installation directory or portable folder
contains accented characters gets every feature that depends on those
locations — help, configuration auto-import, cloud drive entries, browse
buttons, external archivers — instead of silent absence or a misleading error.

**Why this priority**: an accented account name (`Jiří`, `Šárka`, `Kovács`) is
common for the shipped languages, and the failures are silent: the user has
no way to learn why the feature is missing.

**Independent Test**: run the product under an accented account name and from
`D:\Programy\Tandém Commander`; each scenario must succeed where the pre-feature
build fails, while an ASCII installation behaves identically to before.

**Acceptance Scenarios**:

1. **Given** account `Jiří` and a `config.reg` placed in `%APPDATA%\Tandem Commander`, **When** the application starts, **Then** the configuration is auto-imported; Google Drive and Dropbox (when installed) appear in the drive bar and Alt+F1 menu and the Google Drive sync badges are shown; anything the application writes under that folder it can also read back. (F-P1-08)
2. **Given** an installation at `D:\Programy\Tandém Commander` (or a portable copy under `C:\Users\Jiří\Downloads\TC\`), **When** the user presses F1, places `config.reg` next to the executable, or runs a user-menu item using `$(SalDir)`/`$(SalPath)`, **Then** help opens, the configuration is imported, and the item launches — while plugin, language and conversion-table loading (which work today from those same locations) keep working. (F-P1-10)
3. **Given** the same accented installation and an external archiver (RAR/ARJ/LHA/UC2/ACE) configured, **When** the user packs into `test.rar`, **Then** the operation runs; the error "could not start the archiver" is no longer shown for a problem that is the application's own path. (F-P1-07)
4. **Given** *Options → Browse* for an external viewer/editor, an archiver executable, a hot path, or the Copy/Move "Browse" target, **When** the user picks `D:\Programy\Můj editor\edit.exe` (or an accented folder), **Then** the value is stored so that the program later launches / the target is accepted; the browse dialog still opens in the right initial directory and the edit line still shows the picked path correctly (it does today and must continue to). (F-P1-24)
5. **Given** account `Jiří`, **When** the user clicks *My Documents* in the drive bar, or the application falls back to "go here" for an inaccessible path, **Then** the panel lands on the user's Documents folder. (F-P1-24)
6. **Given** the startup "delete leftover temporary directories?" prompt under an accented TEMP, **When** the user presses Focus or Delete, **Then** the panel focuses the directory and the directory is deleted — verified as already fixed by X06/X07; changed only if verification fails. (F-P1-03)

---

### User Story 4 — Cloud, drive, volume and share entries work with accented paths (Priority: P2)

A user opens OneDrive, Dropbox or Google Drive from the drive bar, works on a
volume mounted into an accented folder, on a `subst` drive pointing at an
accented folder, on a UNC share with an accented name, or shares an accented
local folder — and the application handles these like their ASCII
counterparts.

**Why this priority**: cloud entries silently landing on the wrong folder is
loss of function on a very common configuration (accented account name); the
remaining items are narrower or display-only.

**Independent Test**: with `C:\Users\Jiří\OneDrive`, a relocated
`D:\Zálohy\Dropbox`, `subst X: D:\Dokumenty\Šablony`, a volume mounted at
`C:\Disky\Zálohy\` and a share `Účetnictví`, exercise each scenario.

**Acceptance Scenarios**:

1. **Given** OneDrive (personal or Business) under an accented path, **When** the user picks OneDrive in the drive bar or Alt+F1/Alt+F2, **Then** the panel changes to the OneDrive folder — not silently to `C:\Users` or another ancestor; the same for Dropbox and for the Google Drive sync root (`G:\Můj disk` badges and detection). All three are fixed together because the "OneDrive-specific" framing was refuted. (F-P1-09, F-P4-05)
2. **Given** a UNC share `\\server\Účetnictví` or a volume mounted into `C:\Disky\Zálohy\`, **When** the information line, a copy/move pre-check or Compare Directories asks for volume information, **Then** the file system name, label and flags are reported (not empty), free space is the directory's quota-aware value, and Compare Directories' timestamp tolerance is decided from real data rather than an uninitialised value. Ordinary accented paths on a plain volume already behave correctly and must not change. (F-P1-12)
3. **Given** `subst X: D:\Dokumenty\Šablony`, **When** the user deletes a junction or symlink on `X:`, **Then** the confirmation names it as a link (junction / symlink / mount point) and deletes the link, not its target. (F-P1-13)
4. **Given** a disk labelled `Резерв` or `Δεδομένα` on a Czech Windows (characters outside the system code page), **When** the Alt+F1 menu or drive-bar tooltip is shown, **Then** the label is readable rather than `?`; labels representable in the system code page (`Zálohy`) already render correctly and must continue to. (F-P1-14)
5. **Given** a local share named `Účetnictví`, **When** the panel lists its folder, **Then** the shared-folder marker is shown; mapping an accented local path to its UNC form works; names browsed from the network neighbourhood are readable. (F-P1-27; per-item timing rule applies to the marker check)

---

### User Story 5 — External archivers handle accented names and TEMP (Priority: P2)

A user who packs or unpacks with an external archiver (RAR, ARJ, LHA, UC2,
ACE — the default for those extensions) can pack a file named
`Žluťoučký kůň.txt`, unpack into `D:\Zálohy`, and view a file inside a `.rar`
under an accented TEMP.

**Why this priority**: the operations fail outright (pack: "file not found";
unpack: "cannot create the file list" / "MoveFile" error) and leave temporary
directories behind, but only users of external archivers are affected.

**Independent Test**: with RAR present and the fixture files, pack, list,
extract and F3-view under an accented TEMP; every operation completes and no
temporary directory is left behind.

**Acceptance Scenarios**:

1. **Given** `Žluťoučký kůň.txt` in the panel, **When** the user packs it into `test.rar` (Alt+F5), **Then** the archiver receives the real name and the archive contains the file. (F-P1-05, pack side)
2. **Given** `test.rar` listed in the panel, **When** the user extracts or F3-views a file with an accented name, **Then** it still works — the pack-side and list-side encodings are changed together so the currently working round trip does not regress. (F-P1-05, list side)
3. **Given** an accented TEMP or an accented target directory `D:\Zálohy`, **When** the user unpacks or views from `test.rar`, **Then** the operation completes, the extracted file appears in the panel, and the temporary directory is removed afterwards. (F-P1-06)

---

### User Story 6 — Viewer settings persist, captions are readable, Markdown view stays instant (Priority: P2)

A user of the internal viewer on a Central-European Windows keeps their
chosen default character-set conversion across restarts, sees the file name
correctly in the viewer title, and a Markdown Viewer user keeps "instant
view" after unloading and reloading the plugin.

**Why this priority**: the first is a silently lost setting on every
Czech/Slovak/Polish/Hungarian Windows; the second is visible in every viewer
window in the Czech, Slovak and Hungarian UI; the third is a regression of an
unreleased feature (065) in the current tree.

**Independent Test**: set a Kameničtí/KOI-8 ČS2 conversion as default,
restart, and check it held; view `poznámky.txt` in the Czech UI and read the
caption; unload/reload the Markdown Viewer and time the next view.

**Acceptance Scenarios**:

1. **Given** the Central-European conversion set and any UI language, **When** the user picks one of the eight accented-name conversions in *Viewer → Coding* and chooses *Set As Default*, exits and restarts, **Then** the default is still that conversion and files open converted. (F-P4-01)
2. **Given** the Czech, Slovak or Hungarian UI, **When** the user views `poznámky.txt`, **Then** the viewer title shows `poznámky.txt` and the translated viewer name correctly from the moment the window opens; and on any UI language, switching to an accented-name conversion keeps the caption readable. (F-P4-02, both triggers)
3. **Given** the Markdown Viewer has been used once, **When** the user unloads it in the Plugins Manager and views a `.md` file again, **Then** the second view is as fast as the first ("instant view" re-arms); the fix must not create a window on a stale class — the only accepted repair is unregistering on release. (F-P6-01)

---

### User Story 7 — Dialog and list text is readable everywhere (Priority: P3)

A user of a non-English UI sees accented paths and names rendered correctly
in the remaining places where a translated template is combined with a name
or path: the wait window, Drive Information, the Plugins Manager, the plugin
Keyboard Shortcuts list, the Save Configuration prompt, the directory-line
tooltip, packer titles and view-mode names.

**Why this priority**: display-only defects; nothing is lost, but the user
is asked to confirm actions on names they cannot read.

**Independent Test**: Czech UI (and Hungarian where named), reproduce each
surface with the fixture names; every string is readable; English UI output
is unchanged.

**Acceptance Scenarios**:

1. **Given** the cs/de/fr/hu/sk UI, **When** the user enters a slow path `\\server\Zálohy` and the "Reading path…" wait window appears, **Then** the path in it is readable. (F-P2-04; the seven plugin-loading messages are latent and may only be changed as provable no-ops)
2. **Given** the cs/fr/hu/ro/sk UI and the panel on a junction to `D:\Zálohy\Projekty`, **When** the user presses Ctrl+F1, **Then** the "type" line shows the link target readably; the UNC and SUBST rows, which render correctly today, are unchanged. (F-P2-07)
3. **Given** a plugin added by hand from `D:\Můj plugin\thing.spl` and a restart, **When** the Plugins Manager is opened, **Then** the Location column is readable like the Name column. (F-P2-09)
4. **Given** the cs/hu/sk UI, **When** UnDelete is selected in the Plugins Manager, **Then** the "Show … in Change Drive menu" checkbox label is readable in full. (F-P2-10)
5. **Given** the cs/de/fr/hu/sk/es UI, **When** the user opens *Keyboard Shortcuts* for Checksum, UnDelete, Renamer or File Comparator, **Then** the Command column is readable. (F-P2-11)
6. **Given** any of the 7 affected languages and the application started with `-C D:\Zálohy\config.reg`, **When** the user runs *Save Configuration* and the file exists, **Then** the "file exists" prompt shows the path readably. (F-P2-13; the stale suppression note in the code is corrected with it)
7. **Given** a directory path whose text exceeds the tooltip limit and is cut inside an accented character, **When** the directory line is hovered, **Then** the tooltip is readable (cut on a character boundary); the three sites the verifier refuted are untouched. (F-P3-07)
8. **Given** the Hungarian UI on a Western-code-page Windows, **When** the packer/unpacker titles are first seeded, **Then** `külső` is stored and shown with its accent and no `?` is persisted into the configuration; titles supplied by plugins are stored with a defined encoding. (F-P4-03)
9. **Given** the Czech, Slovak or Romanian UI on a Western-code-page Windows, **When** the Views configuration page or the view-mode menu is shown, **Then** the built-in view-mode names carry their accents like user-defined ones; this is hygiene of an invariant and is not presented as a broader fix. (F-P4-07)

---

### User Story 8 — Plugin, documentation and tooling leftovers (Priority: P3)

The maintainer and plugin authors get the small items the 068 review deferred
on process grounds: the file comparator's title and path bar, the ZIP overwrite
attribute line where it can be done inside the plugin, the encoding statement
in the file-system plugin interface documentation, the trace tool building
again, and the developer commit script running on a Czech machine.

**Why this priority**: low consequence each, but each was confirmed and would
otherwise be quietly lost.

**Independent Test**: compare a binary pair in the file comparator and read
the caption; open its path bar on an accented path; build the trace tool;
run the commit script on a CP1250 machine; read the interface header.

**Acceptance Scenarios**:

1. **Given** a binary comparison of two accented-name files, **When** the comparator reports they differ, **Then** the window title is the corrected one (the worker no longer overwrites the fixed caption); **and** the path bar keeps its text on any input rather than dropping it. (D04, D03)
2. **Given** a ZIP overwrite prompt for a file ≥ 1000 bytes on a Czech locale, **When** the attribute line is shown, **Then** it has no stray `Â` — **only if** the fix stays inside the ZIP plugin and the bytes the core receives for English/ASCII input are unchanged; otherwise the item remains recorded with cluster B-5 and the reason is written down. (D02)
3. **Given** the file-system plugin interface documentation, **When** a plugin author reads the five path methods, **Then** each states the encoding of its path text; the change is comment-only and requires no plugin rebuild. (F-P5-06)
4. **Given** the developer trace tool project, **When** it is built, **Then** it compiles; the core's own (non-Unicode) build is unchanged. (D01)
5. **Given** the Spec Kit commit script on a CP1250/CP1252 machine, **When** it is invoked, **Then** it parses and runs. (D05)

---

### Edge Cases

- **Conversion failure at a repaired site** (malformed text, a name outside any code page): the legacy behaviour of the pre-feature build is used; text is never blanked; an operation that ran before still runs.
- **Names with unpaired surrogates** (legal on NTFS, handled since feature 066): every repaired file-system path must accept them exactly as the existing file facade does; the strict plugin-side helpers are left as they are by design.
- **ASCII-only environment**: every repaired surface is byte-identical to the pre-feature build.
- **Characters outside the system code page** (Cyrillic/Greek names on a Czech Windows): repaired sites now carry them; sites that previously substituted `?` before the application saw the text may only improve, never regress.
- **Paths longer than the historical limit**: repaired sites keep their existing length limits — no new truncation and no new crash; the deliberate length limits at the shell/common-dialog boundary stay as they are.
- **Two defects that cancel each other** (external-archiver pack list vs. listing): fixed together or not at all; fixing one side alone would regress the working extract round trip.
- **Producer and consumer of one value in different code** (e.g. browse buttons feeding an edit line; per-user folder written and read; shares cached and compared): converted in the same change; a half-converted chain is a rejected fix.
- **A fix whose only safe form is known** (Markdown keeper): the alternative that would create a window on a stale class is explicitly forbidden.
- **Refuted or latent halves of a finding**: never drive a change; latent sibling sites in the same function may be converted only when provably a no-op for every shipped configuration and listed in the fix record.
- **Per-item paths** (share marker checked per listed directory, icon-location expansion): timing on a folder of at least 50,000 entries, within run-to-run noise.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001 (Scope and traceability)**: Every item of handoff section 1 (34) and section 3 (D01–D05) MUST receive one of two dispositions: *fixed and accepted* or *deferred with written justification*; deferral is allowed only for the plugin-local rule (FR-012) or an unresolved regression-review rejection. Every code change MUST trace to exactly one finding or deferred item; no change may land without one behind it.
- **FR-002 (Verdict-bounded fixes)**: Each fix MUST repair what the independent verdict confirmed and nothing that it refuted; the refuted halves recorded in the verdicts (among them: the startup-cleanup Delete button, the OneDrive prefix-test consumer, ordinary accented paths in volume lookup, code-page-representable volume labels, the UNC/SUBST rows of Drive Information, three of four tooltip sites, the "permanent damage" claim for view-mode names, the browse-dialog display claim, the external-archiver panel-mojibake claim) MUST NOT drive changes. Latent sibling sites MAY be converted only as provable no-ops for every shipped configuration and MUST be listed in the fix record.
- **FR-003 (Whole chain)**: Every fix MUST trace the value from its producer through every intermediate to its sink and convert the whole chain, including every adjacent producer/consumer the verifier named as "fix together" (per-user folder read and write; the three cloud-root producers; browse producers and their edit-line read/write; the shares cache and both consumer kinds; the icon-location registry read; the archive pack and list sides; the uninitialised file-system read in Compare Directories; the configuration-file name producer for the Save Configuration prompt; the shortcut-target probe). A fix that converts one link while an adjacent link still speaks the legacy code page MUST be rejected.
- **FR-004 (Never blank, never skip)**: On any conversion failure, a repaired site MUST fall back to the legacy narrow behaviour of the pre-feature build; no fix may blank text, drop a message, or skip an operation that previously ran.
- **FR-005 (Byte identity)**: Every fix MUST keep English-UI and ASCII-only output byte-for-byte identical to the pre-feature build (commit `64dcbb5`, and therefore to release 0.1.4 on everything feature 068 did not touch), MUST keep every text and formatting service exposed to plugins byte-for-byte identical, MUST NOT change the plugin interface version, and MUST limit changes under the shared plugin headers to documentation comments.
- **FR-006 (Independent regression review)**: Before it lands, every fix MUST pass a regression review by a reviewer who did not write it, following the 068 "Regression reviewer" charter: the reviewer enumerates the consumers of every changed symbol themselves, gives a per-surface verdict (unchanged / corrected / regressed), confirms English/ASCII and plugin-facing byte identity, and states which earlier-feature scenarios the change can touch. A fix is accepted only with zero regressed surfaces; a rejected fix is reworked and re-reviewed or deferred under FR-001.
- **FR-007 (Small, revertable units)**: Fixes MUST land as small, independently reviewable and revertable changes — one finding, or one group the verdicts tie together — with no refactoring of adjacent code and no opportunistic cleanup.
- **FR-008 (Fail-before / pass-after)**: Every fixed defect MUST gain an automated check (unit test or guard rule) demonstrated to fail on the pre-fix code and pass after; where a defect can only be shown on screen, a written manual scenario MUST be recorded and its result logged.
- **FR-009 (Per-item paths)**: A fix on a path that runs once per listed item (listing, sorting, per-directory marker, per-name conversion, icon reading) MUST record a before/after timing on a folder of at least 50,000 entries and is accepted only within run-to-run noise.
- **FR-010 (Automated gates)**: The feature MUST pass: full Debug and Release builds with zero errors and no new warnings in changed files; the complete unit-test suite at or above its baseline (1,257 passing, 0 failing) plus the new checks of FR-008; the strict encoding guard at zero findings; and no new leak or invalid-handle reports over a normal start/exit cycle of the debug build.
- **FR-011 (On-screen sweep)**: The feature MUST deliver written scenarios for the maintainer's on-screen sweep — the 068 items W1–W20 plus one scenario per fix — to be run in the Czech UI and the Hungarian UI with an English spot-check against the pre-feature build; a failure routes back through fix → review → gates before the feature is closed.
- **FR-012 (Plugin-local rule)**: A defect inside a shipped plugin (D02, D03, D04, and the plugin-viewer temp-file group of F-P1-21) MUST be fixed only when it is confirmed user-visible in a shipped configuration, the change is local to the plugin or leaves every byte the plugin receives from the core unchanged, and its regression surface is enumerated and verified under FR-006; otherwise it MUST be deferred with the reason.
- **FR-013 (Truthful changelog)**: Every user-visible fix MUST be described in the changelog's unreleased section in the user's terms — the symptom that is gone, the condition that triggered it — truthful about scope; hygiene-only fixes (F-P4-07) and verify-only items MUST NOT be presented as user-visible repairs beyond what they change.
- **FR-014 (Closing record)**: The feature MUST end with a single record listing every section-1 and section-3 item with its disposition, its regression verdict and its check, and an updated handoff for anything still open, so that nothing confirmed is quietly lost.
- **FR-015 (Guard hygiene)**: Report-only guard rules whose blocking fix lands in this feature MUST be promoted to strict, each proven to fire on a planted defect and proven not to flag correct code; no rule may be promoted before its blocking fix lands.
- **FR-016 (No new behaviour)**: The feature MUST NOT change user-visible behaviour except where a confirmed defect requires it, and MUST NOT add functionality.
- **FR-017 (Verify-only items)**: Items already resolved by X01–X09 (the startup temp cleanup of F-P1-03; the Plugins Manager checkbox of F-P2-10, which is the same site as F-P6-02; the jump-list part of F-P1-25) MUST be verified against the current tree and recorded as closed; they are changed only if verification fails. Every other item MUST likewise be confirmed still defective at HEAD before any code is written, since the handoff was written before the last fixes landed.
- **FR-018 (Tooling items)**: D01 MUST leave the core's own build unchanged (no behaviour change, no new warnings) while making the trace tool build; D05 MUST be a one-line encoding repair of the developer script with no change to what it does.

### The defect inventory (what is fixed, to what extent)

| ID | Surface | What the user sees today | Confirmed scope of the fix | Story |
|---|---|---|---|---|
| F-P6-04 | Command line | Ctrl+Enter / Ctrl+Space / Ctrl+[ / Ctrl+] insert mojibake; Enter runs against a nonexistent name | The four keystrokes, the internal drag payload, the read-back; caret/selection positions must stay right | US2 |
| F-P1-20 | Archive edit → Copy To | Edit copied nowhere, no message (data loss) | Whole; result no longer discarded | US2 |
| F-P1-19 | Compare Directories | Accented files/subdirectories cannot be read | Both file and subdirectory paths | US2 |
| F-P1-26 | Drag from Explorer | Drops onto command line / status bar / toolbar / viewer fail silently | Four sites | US2 |
| F-P1-23 | `%VAR%` expansion | Accented account: path "does not exist", generic icons, mis-encoded child-process directories | Three sites plus the adjacent icon-location read | US2 |
| F-P1-25 | Shortcuts, share dialog, shell menu | Shortcut at/to an accented path opened as a file; share creation fails; unresolvable names to the shell | Seven sites plus the shortcut-target probe; jump list verify-only (X03) | US2 |
| F-P1-21 | Nine assorted operations | Silent failures under accented TEMP/names (drag out, SFX, link, batch user menu, masks, freshness, refresh, `$(DOSPath)`, plugin-viewer temp) | All nine; plugin-viewer group under FR-012 | US2 |
| F-P1-22 | User Menu icons | Default icon for executables under accented paths | Both runtime sites (picker already right) | US2 |
| F-P1-08 | Per-user folders | Accented account: no config auto-import, no Google Drive / Dropbox entries or badges | Read and write side together | US3 |
| F-P1-10 | Install / portable folder | F1 help error, config.reg ignored, `$(SalDir)` launches fail | Strict consumers; working legacy loaders preserved | US3 |
| F-P1-07 | External archiver launch | "Could not start archiver" on an accented install path | Helper-executable path, both copies | US3 |
| F-P1-24 | Browse buttons, My Documents | Stored values fail at use time; My Documents fails under accented account; empty initial directory | Producers and their edit-line I/O together; display behaviour unchanged | US3 |
| F-P1-03 | Startup temp cleanup | (already fixed by X06/X07) | Verify-only | US3 |
| F-P1-09 / F-P4-05 | Cloud entries | OneDrive / Dropbox / Google Drive entries land on an ancestor or fail | All three cloud producers together | US4 |
| F-P1-12 | Volume information | UNC accented share / mounted volume: no info; uninitialised tolerance decision | Those cases plus the uninitialised read; ordinary paths unchanged | US4 |
| F-P1-13 | `subst` drives | Link deletion misidentified as file/dir | subst resolution | US4 |
| F-P1-14 | Volume labels / drive names | `?` for characters outside the code page | Producers (display-only); representable labels unchanged | US4 |
| F-P1-27 | Shares, shell namespace | No share marker; UNC mapping fails; `?` in browsed names | Cache producer + both consumers; timing rule | US4 |
| F-P1-05 | External archiver pack/list | Pack of an accented name fails | Pack and list sides together | US5 |
| F-P1-06 | External archiver temp/archive paths | "Cannot create file list", "MoveFile" error, temp dir left behind | Whole subsystem incl. cleanup | US5 |
| F-P4-01 | Viewer default coding | Lost on every restart | Whole | US6 |
| F-P4-02 | Viewer caption | Mojibake in cs/sk/hu for any accented name; and on coding switch | Both triggers | US6 |
| F-P6-01 | Markdown Viewer | "Instant view" never re-arms after Unload | Unregister-on-release only | US6 |
| F-P2-04 | Wait window | Path mojibake (cs/de/fr/hu/sk) | Two path sites; plugin-loading sites latent | US7 |
| F-P2-07 | Drive Information | Junction target mojibake (cs/fr/hu/ro/sk) | Whole type-line composition; UNC/SUBST rows unchanged | US7 |
| F-P2-09 | Plugins Manager | Location column mojibake for out-of-tree plugin | Location (+ Version as no-op) | US7 |
| F-P2-10 | Plugins Manager | (already fixed by X02 — same site as F-P6-02) | Verify-only | US7 |
| F-P2-11 | Keyboard Shortcuts | Command names mojibake (cs/de/fr/hu/sk/es) | One list | US7 |
| F-P2-13 | Save Configuration | Path mojibake in "file exists" prompt (7 languages) | Composition + stale note; producer settled with F-P1-10 | US7 |
| F-P3-07 | Directory-line tooltip | Whole hint mojibake when cut mid-character | One site; three refuted sites untouched | US7 |
| F-P4-03 | Packer titles | `?` persisted (Hungarian on Western code page) | Nine seeds + defined encoding + plugin titles | US7 |
| F-P4-07 | View-mode names | `?` next to correct user names (cs/sk/ro on Western code page) | Seeds; hygiene | US7 |
| F-P5-06 | FS plugin interface docs | No encoding stated | Comment-only | US8 |
| D01 | Trace tool | Does not build | Small; core unchanged | US8 |
| D02 | ZIP overwrite prompt | Stray `Â` for files ≥ 1000 bytes (cs) | Only if plugin-local (FR-012) | US8 |
| D03 / D04 | File comparator | Path bar drops text; binary-compare caption overwrites the fix | ~4 lines + small; together | US8 |
| D05 | Developer commit script | Parser error on CP1250/CP1252 | One line | US8 |

### Key Entities

- **Finding**: a confirmed defect from the 068 review, identified by its ID, with its independent verdict (what is confirmed, refuted, latent) and its recorded failure scenario; the unit of scope.
- **Fix record**: one change traceable to one finding or one coupled group; carries the traced producer→sink chain, the enumerated affected surfaces, the fail-before/pass-after check, and (per-item paths) the timing.
- **Regression verdict**: the independent reviewer's per-surface result (unchanged / corrected / regressed) and overall decision (accepted / rejected) for one fix record.
- **Gate**: a pass/fail stability check with evidence (build, tests, guard, leak/handle report, on-screen sweep).
- **Deferred item**: a confirmed item not fixed here, with its justification and where it is recorded for the next feature.
- **Fixture**: a concrete reproduction environment (accented names, TEMP, account, install path, cloud roots, subst, share) reused across scenarios.
- **Closing record**: the single document of FR-014.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of the 34 section-1 items and the 5 section-3 items carry a recorded disposition; **31 of the 34 are *fixed and accepted* and 3 are *verify-closed*** (F-P1-03 and F-P2-10 — the latter is the same site as F-P6-02 and was already fixed by X02 — plus the jump-list half of F-P1-25; established in `research.md` R1), and every deferral cites FR-012 or an unresolved regression rejection.
- **SC-002**: 100% of fixes carry an independent regression verdict of *accepted* with zero regressed surfaces, and 100% carry an automated check (or recorded manual scenario) proven to fail before and pass after the fix.
- **SC-003**: Zero byte differences in English-UI / ASCII output and in plugin-facing text and formatting services against the pre-feature build; plugin interface version unchanged; shared plugin headers changed in comments only.
- **SC-004**: All automated gates green: both full builds clean with no new warnings in changed files; unit tests at or above 1,257 passing with 0 failing and at least one new check per fixed defect; strict encoding guard at 0 findings; no new leak or handle reports over a start/exit cycle.
- **SC-005**: Zero regressions in the Czech-UI and Hungarian-UI on-screen sweep (W1–W20) and in the English spot-check; 100% of the per-fix scenarios pass on screen.
- **SC-006**: Every acceptance scenario of US2–US7 that reproduces on the pre-feature build no longer reproduces on the new build, on the recorded fixtures.
- **SC-007**: 100% of per-item-path fixes carry a timing on ≥ 50,000 entries within run-to-run noise.
- **SC-008**: Every user-visible fix appears in the changelog's unreleased section in the user's terms; zero hygiene-only or verify-only items are presented as repairs.
- **SC-009**: A reader of the closing record can trace every code change to its finding, its regression verdict and its check, and finds every still-open item in the updated handoff.

## Assumptions

- **Baseline** is the current tree (commit `64dcbb5`, post-068, unreleased); byte-identity is measured against it. Release 0.1.4 (2026-08-19) remains the released baseline for everything 068 did not touch.
- **Systemic clusters B-1–B-5 are out of scope** because the 068 review established that no minimal fix exists for them and each needs its own regression matrix; bundling them here would contradict the no-regression rule. They remain recorded for separate features.
- **D02** is attempted under the plugin-local rule (FR-012); if the fix cannot avoid changing what the core receives, it stays with cluster B-5.
- **Adjacent sites** are included only when a verifier tied them to an in-scope finding as "fix together" and they share its defect shape; adjacent sites belonging to a deferred cluster (e.g. the error-text composition of cluster B-3) are not pulled in.
- **No release** is made by this feature; the changelog's unreleased section is updated. A version bump, if the maintainer decides to release, follows the constitution's rule in a separate change.
- **Test machine** is a Czech (CP1250) Windows 11; scenarios that need a Western (CP1252) code page (F-P4-03, F-P4-07) are verified by automated checks on the stored/displayed text rather than on a second machine.
- **Fixtures** already exist from feature 068 (`D:\Zkouška\Můj disk\`, `D:\Zkouška\Árvíztűrő tükörfúrógép\`, `D:\Zkouška\surrogate\`, the 100,000-file performance folder); accented account/TEMP/install-path/cloud/subst/share fixtures are created as needed.
- **The manual on-screen sweep** needs a human and is run by the maintainer using the delivered scenarios; the feature delivers everything automatable plus those scenarios.
- **Disabled languages** (Russian, Ukrainian, Simplified Chinese) are not considered; nothing here changes their re-enable checklist.
- **Process artefacts** (verdicts, charters, guard, test harness) of feature 068 are reused as they are; the regression-reviewer charter is applied verbatim.

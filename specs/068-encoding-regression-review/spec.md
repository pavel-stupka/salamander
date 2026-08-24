# Feature Specification: Encoding Regression Review and Stabilization

**Feature Branch**: `068-encoding-regression-review`
**Created**: 2026-08-24
**Status**: Draft
**Input**: User description: "Cilem teto upravy/feature je regresni stabilizace kodu - tedy detailni analyza provedenych uprav a identifiakce potencialnich mist s chybou a stabilizace, oprava s cilem zvyseni kvality programu, stability a chovani. V zadnem pripade nesmi tato uprava zanest regresni chyby / jejim cilem je pravy opak, identifikovat a najit pripade chyby a opravit je. Jako napr. v minule feature 067-fix-drive-info-encoding kdy uživatel odhalil chybu v zobrazení textu (kódování), které již mělo být opravené. Cílem tedy je převížně revize těch částí kódu, které pracují s kódování a případné opravy. Pokud bude nalezena chyba, nebo potenciální chyba, agent musí nezávisle ověřit, že případná oprava nevytvoří chybu novou, nezanese regresení chybu. Toto není implementace nové funkce, pouze revizní kontrola a případné opravy chyb."

## Problem Statement

Text encoding is the product's longest-running defect class. Since the
Unicode/long-path rework (feature 004) made file names, paths and most UI
text Unicode inside the application, twelve follow-up features (005, 010,
015, 041, 042, 043, 052, 058, 062, 063, 066, 067) have each repaired one
surface where some code still treated that text as legacy 8-bit — dialog
fields, the information line, Find results, plugin names, the icon and
change-monitoring pipeline, Recycle Bin routing, file lists and clipboard
copies, names containing broken characters, and grouped-number formatting.
Nearly every one was found **by the user, on a Czech system, in a place the
earlier fixes were believed to cover**. Feature 067 is the latest example:
the Drive Information dialog garbled byte counts through a formatting path
that feature 041 had already converted; the fix then uncovered a missed twin
of two sites that had been fixed earlier, a second garbled surface (the
viewer's offset tooltip), and seven plugin-side sites of the same class that
were recorded and deferred.

The pattern is the problem: fixes were applied per reported surface, and the
sibling sites of the same defect class were not always swept. This feature
turns the direction around — a proactive, systematic review of everything
that handles text encoding, finding the remaining and latent sites before
users do, fixing what is confirmed, and proving for every fix that nothing
which works today stops working. It is explicitly **not development work**:
no new capability, no refactoring beyond what a confirmed defect requires.
Its outputs are higher confidence, targeted and independently verified fixes,
durable automated guards for the defect classes found, and an auditable
record — the discipline of features 056 and 060, applied to one defect class
across the whole product instead of to one release delta.

## Clarifications

### Session 2026-08-24

- Q: Which code should this review cover — the whole core application's
  encoding-handling code, or only the unreleased changes? → A: The whole core
  application's encoding code (all thirteen earlier fixes and their sibling
  sites) plus the plugin boundary; defects inside shipped plugins are fixed
  only when user-visible, local to the plugin, and with an enumerable
  regression surface — otherwise recorded. A full plugin-side sweep and a
  delta-only review were both explicitly not chosen.
- Q: Must a fix on code that runs once per file (listing, sorting, icon or
  badge reading, per-name conversion) also prove it does not slow those
  operations down, and how? → A: Yes, for per-item paths only: a
  before/after timing on a folder of at least 50,000 entries, accepted only
  when the difference is within run-to-run noise. Fixes elsewhere need no
  timing; performance otherwise remains a code-reading perspective.
- Q: In which UI languages should the on-screen regression sweep of the
  previously repaired surfaces be run? → A: Czech and Hungarian on screen
  (Hungarian being the only other shipped language feature 067 proved
  affected), plus the English spot-check; the remaining five shipped
  languages by inspection of their translations and automated checks.
- Q: When the review finds a confirmed defect that is not an encoding
  problem (e.g. the French "octetss" plural, a wrong argument type in a
  plugin message), should it be fixed here or only recorded? → A: Fixed
  only when trivial and self-contained — a data-only change (translation
  text) or a one-line local code change, confirmed user-visible, with its
  own independent regression verdict and fail-before/pass-after check;
  everything else recorded in the report's deferred list.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Systematic audit of everything that handles text encoding (Priority: P1)

The maintainer receives a complete inventory of the places in the product
where text changes representation or crosses a boundary — file names and
paths arriving from disk; names and paths handed to Windows for every file,
shell, icon, change-monitoring and Recycle Bin operation; translated text
leaving a language module for the screen; messages composed from translated
text plus names, numbers, dates or plugin-supplied text; text leaving the
application through the clipboard, generated file lists, logs and external
programs; text entered by the user in rename, path, mask and command fields;
values written to and read back from the saved configuration; and text and
names exchanged with plugins in both directions. Every inventoried site is
classified with evidence as *verified correct*, *defective*, *latent* (wrong
only in a configuration that does not ship, e.g. a disabled language), or
*out of scope with a stated reason*. For every encoding defect class fixed by
an earlier feature, all sibling sites of that class are enumerated and each is
confirmed fixed or raised as a finding — the sweep feature 067 showed was
missing. Compliance with each binding encoding contract recorded by earlier
features (features 052, 058, 063, 066, 067) is checked site by site, and every
item earlier features recorded as "deferred" is re-examined rather than
silently carried forward.

**Why this priority**: This is the review the user asked for. The inventory
and the class-based sibling sweep are what convert "we fixed the reported
dialog" into "this class of defect is gone"; without them fixes are guesses.

**Independent Test**: The review record lists every boundary above with its
inventoried sites and their classifications; for each prior defect class it
lists the sibling sites swept; for each contract it lists the checked sites;
and every earlier deferred item appears with a fresh disposition. A reader
can pick any surface that displays or transports text and find it in the
inventory.

**Acceptance Scenarios**:

1. **Given** the complete product source, **When** the audit completes,
   **Then** every boundary listed above has an inventory with at least one
   site, every site carries a classification and its evidence, and the
   record shows which review perspective examined it.
2. **Given** a defect class fixed by an earlier feature (e.g. "translated
   text composed with a Unicode name and drawn through the legacy path"),
   **When** the sibling sweep for that class completes, **Then** every site
   where the same composition or the same sink occurs is listed as verified
   or raised as a finding — none is absent.
3. **Given** an item an earlier feature recorded as deferred, **When** the
   audit completes, **Then** the item has a new disposition (fixed, or
   deferred again with a written justification) — never an implicit
   carry-over.
4. **Given** a site that is wrong only in a language that does not ship,
   **When** it is classified, **Then** it is recorded as latent on the
   re-enable checklist for that language, and no code change is made for it
   alone.

---

### User Story 2 - Verified, regression-free fixes for every confirmed defect (Priority: P1)

Every finding raised by the audit is verified against the actual code by a
reviewer independent of the one who raised it, with a concrete failure
scenario (what the user sees, on which locale/UI language, on which surface);
findings that cannot be shown to fail lead to no change. Every confirmed,
shipping-relevant defect is fixed with the smallest change that removes it,
traceable to its finding. **Before any fix is accepted, an independent
reviewer examines it with the goal of finding a regression**: enumerates every
surface and consumer the changed code affects, confirms each is either
unchanged or corrected, confirms that English UI and ASCII-only output stay
byte-for-byte identical, that the plugin-facing behavior stays byte-for-byte
identical, and that behavior validated by earlier features still holds. Every
fix is accompanied by an automated check that fails on the pre-fix code and
passes after it; where a defect can only be demonstrated on screen, a written
manual scenario is recorded and executed.

**Why this priority**: The user's hard constraint is that this work must not
introduce a single regression — its purpose is the opposite. Independent
verification of both the finding and the fix is what makes that constraint
checkable rather than hoped for. Equal priority to US1 because an audit that
produces unverified fixes is worse than no audit.

**Independent Test**: For any code change made in this feature, the record
shows the confirmed finding behind it, the independent regression verdict
with the list of affected surfaces and their status, and the automated (or
recorded manual) check that fails before and passes after.

**Acceptance Scenarios**:

1. **Given** a raised finding, **When** it is independently verified,
   **Then** the record shows the verdict with the concrete failure scenario
   or the refutation evidence, and a refuted finding leads to no code change.
2. **Given** a confirmed defect, **When** it is fixed, **Then** the fix is
   minimal (no drive-by refactoring, no adjacent code touched), and an
   automated check exists that fails on the pre-fix code and passes after.
3. **Given** a fix, **When** the independent regression review runs,
   **Then** every surface affected by the changed code is listed with a
   verdict, English/ASCII output is confirmed byte-identical, plugin-facing
   output is confirmed byte-identical, and the fix is accepted only when no
   surface regressed.
4. **Given** a fix that touches shared text-conversion or formatting
   machinery used by many surfaces, **When** it is verified, **Then** the
   whole set of previously validated surfaces (US3 sweep list) is re-checked,
   not only the surface that motivated the fix.
5. **Given** two candidate fixes for one defect with different regression
   surfaces, **When** one is chosen, **Then** the record states why the one
   with the smaller surface was chosen (or why the larger was unavoidable).
6. **Given** a fix on a per-item path (listing, sorting, icon/overlay
   reading, per-name conversion), **When** it is verified, **Then** a
   before/after timing on a folder of at least 50,000 entries is recorded,
   and the fix is accepted only if the difference is within run-to-run
   noise.

---

### User Story 3 - Whole-product stability gates and regression sweep (Priority: P2)

The maintainer gets evidence that the product as a whole is at least as
stable as release 0.1.4 with every fix in: clean full builds of both
configurations, the complete unit test suite passing at or above its current
baseline, the build-time encoding guard clean in strict mode, no new leak or
handle reports from the debug instrumentation over a normal start/exit cycle,
and an on-screen regression sweep — on a Czech Windows system, once with the
Czech UI and once with the Hungarian UI — of every surface repaired by
earlier encoding features: panel names and
size columns, information line, directory line and free space, Find results
and status bar, message boxes and confirmations, Plugins Manager names, Drive
Information, directory/archive size dialogs, the drive menu, Make File List
to clipboard/viewer/file, copy name/path commands, tooltips and hints, the
rename field, the internal viewer, Recycle Bin deletion in non-ASCII paths,
icon and overlay badges with automatic refresh in non-ASCII paths, operations
on names with broken characters, language selection, hot path names, and
saved-configuration round trips of non-ASCII and broken-character paths.

**Why this priority**: "No regressions" is the release claim of this feature;
it needs gates with recorded evidence, not impressions. P2 only because the
gates verify the output of US1/US2.

**Independent Test**: A gate table in the review record, every row green or
explicitly waived with justification; the sweep list above with a pass/fail
per surface and the language/locale it was checked in.

**Acceptance Scenarios**:

1. **Given** the stabilized code, **When** the full build and test gates run,
   **Then** both configurations build with zero errors and no new warnings in
   changed files, the unit tests pass at or above baseline, and the strict
   encoding guard reports zero findings.
2. **Given** the debug build, **When** the application is started and exited
   normally, **Then** no new leak or invalid-handle reports appear relative
   to the pre-feature state.
3. **Given** the sweep list, **When** it is executed on a Czech system with
   the Czech UI and again with the Hungarian UI, **Then** every surface
   renders text, names and numbers correctly in both, and the result per
   surface and language is recorded.
4. **Given** the English UI on an ASCII-separator locale, **When** the same
   surfaces are spot-checked, **Then** output is unchanged from release
   0.1.4.

---

### User Story 4 - Auditable record and durable guards (Priority: P2)

The maintainer gets a single written record — the review report — stating
the scope and method, the inventory summary per boundary, every finding with
its verdict, evidence and disposition, every fix with its independent
regression verdict, the gate and sweep results, all deferred items with
justification, and an explicit stability verdict. Every defect class found is
turned into a durable check — a unit test or a build-time guard rule — so
that the next occurrence fails the build instead of reaching a user, and
user-visible fixes are described in the changelog in the user's terms.

**Why this priority**: The record makes the exercise repeatable and
auditable (as the feature-056 and feature-060 reports did); the guards are
what stop the "already fixed, found again" cycle the user described. Depends
on US1–US3 content, hence P2.

**Independent Test**: The report exists; a reader can trace every code
change to a confirmed finding and its regression verdict; every defect class
found has a named automated check; every user-visible fix has a changelog
entry.

**Acceptance Scenarios**:

1. **Given** the finished review, **When** the report is read, **Then** it
   contains scope, method and perspectives, the per-boundary inventory
   summary, all findings with verdicts and dispositions, all fixes with
   regression verdicts, the gate table, the sweep table, deferrals with
   reasons, and the stability verdict.
2. **Given** a defect class confirmed during the review, **When** the
   feature completes, **Then** an automated check named in the report
   detects that class, and the check is proven by running it against the
   pre-fix code.
3. **Given** a fix that changes what a user sees, **When** the feature
   completes, **Then** the changelog's unreleased section describes the
   symptom that is gone, truthfully scoped.

---

### Edge Cases

- **No confirmed findings in an area**: a legitimate outcome — the inventory
  and the clean classification are the result; absence of fixes is not
  absence of work.
- **Plausible-but-wrong findings** (a site that looks mixed but is
  ASCII-only in every shipped language, a legacy sink that is never reached
  with Unicode text): independent verification exists to kill these; a
  finding without a concrete, demonstrable failure scenario drives no change.
- **A defect whose only correct fix would change the bytes plugins receive
  from the shared formatting and text services**: the plugin-facing behavior
  is frozen (feature 067 established that changing it garbles shipped
  plugins); such a defect is fixed on the core side only, and the
  plugin-facing part is deferred with justification.
- **A defect inside a shipped plugin** (the feature-067 deferred list and
  anything new): fixed only when it is confirmed user-visible in a shipped
  configuration, the fix is local to that plugin, and its regression surface
  can be enumerated and verified; otherwise deferred with justification.
- **A defect reachable only in a disabled language** (Russian, Ukrainian,
  Simplified Chinese): classified latent, recorded on that language's
  re-enable checklist, not fixed alone.
- **A defect in vendored third-party code**: out of scope unless it causes a
  shipped, user-visible defect; recorded either way.
- **A confirmed defect that is not an encoding problem** (a translation
  plural, a wrong argument type in a message): fixed only when the fix is a
  data-only change or a one-line local code change, confirmed user-visible,
  with its own regression verdict and fail-before/pass-after check;
  otherwise recorded in the deferred list.
- **A defect demonstrable only on a locale or separator setting the
  maintainer's machine does not have**: verified by an automated check that
  constructs the relevant input (separator, code page, broken character)
  rather than by a manual scenario.
- **A fix that itself changes reviewed code**: the affected inventory
  entries, the regression review and the affected gates re-run on the fix
  (bounded re-verification, not a full restart).
- **A gate or sweep item that cannot run** (environment-dependent manual
  scenario): recorded as waived with justification, never silently skipped;
  on-screen scenarios are the user's manual pass, listed explicitly.
- **Names or text that are legitimately lossy on an external channel**
  (a broken character copied as plain text, written to a log): acceptable per
  the feature-066 contract; only operational channels (file operations,
  process launches, saved configuration) must carry the exact identity.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The review MUST inventory every core-application site where
  text changes representation or crosses a boundary, covering at least:
  disk → application (listings, names, paths, link targets); application →
  Windows (file, shell, icon, overlay, change-monitoring, Recycle Bin,
  process-launch and drag-and-drop operations); language module → screen;
  composition of translated text with names, numbers, dates/times and
  plugin-supplied text; application → clipboard, file lists, logs and
  external programs; user-entered text (rename, path, mask, command and
  search fields); saved configuration in both directions; and the plugin
  boundary in both directions (names/paths handed to plugins, plugin text
  shown by the core, shared formatting and text services offered to plugins).
- **FR-002**: Every inventoried site MUST be classified as verified correct,
  defective, latent (unreachable in any shipped configuration), or out of
  scope with a stated reason, each with the evidence that supports the
  classification and the perspective that examined it.
- **FR-003**: For every encoding defect class repaired by an earlier feature
  (004, 005, 010, 015, 041, 042, 043, 052, 058, 062, 063, 066, 067), the
  review MUST enumerate all sibling sites of that class — the same kind of
  composition or the same kind of sink — and confirm each as fixed or raise
  it as a finding.
- **FR-004**: The review MUST check the code against every binding encoding
  contract recorded by earlier features — at least the five of features
  052, 058, 063, 066 and 067 (plugin metadata, path pipeline, file-list and
  tooltip text, name encoding, number formatting) and every older one the
  planning research enumerates — site by site; every deviation is a
  finding.
- **FR-005**: Every encoding-related item recorded as deferred or follow-up
  by an earlier feature MUST be re-examined and receive a fresh disposition
  (fixed, or deferred again with written justification).
- **FR-006**: Every raised finding MUST be verified against the actual code
  by a reviewer independent of the one who raised it, with a concrete failure
  scenario (surface, locale/UI language, what the user sees). Refuted or
  undemonstrable findings MUST NOT lead to code changes.
- **FR-007**: Every confirmed, shipping-relevant defect MUST be fixed with a
  minimal change traceable to its finding. No change may land without a
  confirmed finding behind it — no new functionality, no refactoring of
  adjacent code, no opportunistic cleanup.
- **FR-008**: Before acceptance, every fix MUST pass an independent
  regression review that enumerates every surface and consumer affected by
  the changed code and records a verdict per surface; a fix is accepted only
  when no surface regresses. A fix to shared conversion or formatting
  machinery MUST re-verify the complete sweep list of US3. A fix on a
  per-item path (folder listing, sorting, icon/overlay reading, per-name
  conversion) MUST additionally record a before/after timing on a folder of
  at least 50,000 entries and is accepted only if the difference is within
  run-to-run noise; fixes elsewhere carry no timing requirement.
- **FR-009**: Every fix MUST keep English-UI and ASCII-only output
  byte-for-byte identical to release 0.1.4, and MUST keep the text and
  formatting services exposed to plugins byte-for-byte identical, with no
  change to the plugin interface version.
- **FR-010**: Every defect fixed in this feature MUST gain an automated
  check (unit test or build-time guard rule) that fails on the pre-fix code
  and passes after it, proven by running it against both; where a defect can
  only be demonstrated on screen, a written manual scenario MUST be recorded
  and its result logged.
- **FR-011**: The stability gates MUST run and pass, or be explicitly waived
  with justification: full Debug and Release builds with zero errors and no
  new warnings in changed files; the complete unit test suite at or above its
  current baseline (1229 passing, 0 failing); the build-time encoding guard
  clean in strict mode; no new leak or invalid-handle reports over a normal
  start/exit cycle of the debug build; and the on-screen regression sweep of
  US3 on a Czech system in the Czech UI and in the Hungarian UI, plus an
  English spot-check.
- **FR-012**: Defects inside shipped plugins MUST be fixed only when
  confirmed user-visible in a shipped configuration, the fix is local to the
  plugin, and its regression surface is enumerated and verified under FR-008;
  otherwise they MUST be deferred with justification. Defects reachable only
  in disabled languages MUST be recorded on that language's re-enable
  checklist and not fixed alone.
- **FR-013**: The review report MUST exist as a single document recording
  scope, method and perspectives, the per-boundary inventory summary, all
  findings with verdicts, evidence and dispositions, all fixes with their
  regression verdicts, the gate and sweep tables, deferrals with reasons, and
  an explicit stability verdict.
- **FR-014**: The feature MUST NOT change user-visible behavior except where
  a confirmed defect requires it; every such change MUST be described in the
  changelog's unreleased section in the user's terms, truthfully scoped.
- **FR-015**: A confirmed defect that is not an encoding defect MUST be
  fixed in this feature only when the fix is trivial and self-contained — a
  data-only change (translation text) or a one-line local code change —
  confirmed user-visible in a shipped configuration, and carries its own
  independent regression verdict (FR-008) and fail-before/pass-after check
  (FR-010); every other non-encoding defect MUST be recorded in the report's
  deferred list.

### Key Entities

- **Boundary**: a kind of place where text changes representation or leaves
  or enters the application (disk, Windows, language module, clipboard,
  configuration, plugin, user input); the inventory is organized by boundary.
- **Site**: one concrete place in the product where a boundary is crossed;
  carries a classification (verified correct / defective / latent / out of
  scope), evidence, and the perspective that examined it.
- **Defect class**: a recurring shape of encoding error (e.g. translated
  text composed with a Unicode name and drawn through the legacy path; a
  Unicode path handed to a legacy operation; a lossy conversion on a path
  that ends in a display); each class fixed by an earlier feature has a
  sibling sweep, and each class confirmed here gets a durable check.
- **Contract**: a binding encoding rule recorded by an earlier feature
  (features 052, 058, 063, 066, 067); compliance is checked site by site.
- **Finding**: a suspected defect raised at a site; carries a failure
  scenario, an independent verification verdict (confirmed / refuted) with
  evidence, and a disposition (fixed / deferred / no change needed).
- **Fix**: a minimal change traceable to one confirmed finding; carries the
  list of affected surfaces, the independent regression verdict per surface,
  and its fail-before/pass-after check.
- **Deferred item**: a confirmed defect not fixed here, with its
  justification and where it is recorded (plugin follow-up list, language
  re-enable checklist, non-encoding follow-up list, or vendored-code note).
- **Gate / Sweep item**: a pass/fail stability check with recorded evidence
  (build, tests, guard, instrumentation, on-screen surface).
- **Review report**: the single record (US4).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of the boundaries listed in FR-001 have an inventory, and
  100% of inventoried sites carry a classification with evidence and an
  examining perspective.
- **SC-002**: 100% of the defect classes repaired by the thirteen earlier
  encoding features have a completed sibling sweep, and 100% of earlier
  deferred encoding items have a fresh recorded disposition.
- **SC-003**: 100% of raised findings carry an independent verification
  verdict with evidence; zero code changes trace to refuted or unverified
  findings (spot-check: any 3 randomly chosen changes).
- **SC-004**: 100% of fixes carry an independent regression verdict listing
  their affected surfaces, and 100% carry an automated check (or a recorded
  manual scenario) demonstrated to fail before and pass after the fix.
- **SC-005**: Zero regressions in the Czech-UI and Hungarian-UI on-screen
  sweeps of every surface repaired by earlier encoding features, and zero
  differences in the English-UI/ASCII spot-check against release 0.1.4.
- **SC-006**: All stability gates green: both full builds clean with no new
  warnings in changed files, unit tests at or above 1229 passing with 0
  failing, strict encoding guard at 0 findings, and no new leak or handle
  reports over a start/exit cycle.
- **SC-007**: Plugin-facing text and formatting output byte-identical to
  release 0.1.4 and the plugin interface version unchanged; any plugin-local
  fix has its own recorded regression verdict.
- **SC-008**: Every defect class confirmed during the review is covered by a
  named durable check that is proven to flag the pre-fix code.
- **SC-009**: The review report exists and a reader can trace every code
  change to its finding and its regression verdict; every user-visible fix
  appears in the changelog.
- **SC-010**: 100% of fixes on per-item paths carry a recorded before/after
  timing on a folder of at least 50,000 entries, and zero of them exceed
  run-to-run noise.

## Assumptions

- **Scope (confirmed, clarification Q1).** "Detailní analýza provedených
  úprav" means not the last release delta alone but the *encoding-handling
  code of the whole core application* — the sites
  modified by the thirteen encoding features and every sibling site of the
  same classes — reviewed at line level. The unreleased encoding changes
  (features 066 and 067, the newest and least field-tested) receive full
  line-level review as part of that. Non-encoding code enters scope only
  where a finding leads there. The unreleased non-encoding change (feature
  065) is in scope only for its encoding boundary (paths handed to the
  embedded viewer), not for a general review.
- **Plugins (confirmed, clarification Q1).** Plugin code is inventoried at
  its boundary with the core and where it displays names, paths or numbers
  the core hands it; the feature-067 deferred plugin-side list is re-examined
  under FR-005. Fixes inside plugins follow FR-012 (local, confirmed
  user-visible, regression surface bounded); the plugin-facing services stay
  byte-frozen per feature 067. A full sweep of every plugin's own text
  handling was offered and explicitly not chosen.
- **Developer-side tooling** (the standalone settings-migration script, the
  translation pipeline, build scripts) is neither the core application nor a
  plugin and is therefore outside the review by the Q1 decision; it keeps
  its own test harnesses. A finding that leads there is recorded, not fixed
  here.
- **Baseline for "no regressions"**: release 0.1.4 behavior plus the
  user-validated outcomes of features 066 and 067 (their quickstart
  scenarios), and the byte-identity of English/ASCII output.
- **Verification environment (confirmed, clarification Q3)**: a Czech
  Windows 11 system is the manual environment (the one every reported defect
  came from); the on-screen sweep runs in the Czech UI and in the Hungarian
  UI (the only other shipped language feature 067 proved affected), with an
  English spot-check; the remaining five shipped languages are covered by
  inspection of their translations and by automated checks; on-screen
  scenarios are the user's manual pass, listed explicitly as in feature 067.
  Disabled languages (Russian, Ukrainian, Simplified Chinese) are not
  verification targets.
- **Independence**: "independently verify" means the verification of a
  finding, and the regression review of a fix, are performed by a reviewer
  that did not raise the finding or author the fix; the exact roster of
  review perspectives is a planning decision (feature-056/060 precedent).
- **Durable guards are not new functionality**: unit tests and build-time
  guard rules are the established stabilization mechanism (features 042,
  052, 063, 066, 067) and are in scope; product features are not.
- **Release** (version bump, changelog finalization, signing, installer) is
  out of scope; this feature produces the stability record that a release
  can rely on.

# Feature Specification: Small hardening batch — six recorded defects without a finding

**Feature Branch**: `075-fix-small-hardening`
**Created**: 2026-09-02
**Status**: Draft
**Input**: User description: "Nyni priprav specifikaci pro doporucovany bod cislo 1: Drobná bezpečnostní dávka (hodiny, jedna feature). Pět konkrétních defektů zapsaných v 069 §3, žádný nemá finding, proto je 069 nesměla opravit. Ověřil jsem, že první z nich je stále na HEAD: src/codetbl.cpp:873 — `if (len > bufferLen) len = bufferLen - 1;` má být `>=`; jméno konverze dlouhé přesně bufferLen zapíše buffer[bufferLen]. Zápis mimo buffer, i když dnes nedosažitelný (nejdelší jméno 33 B). viewer3.cpp:3291 neošetřená návratová hodnota GetCodeType → defCodeType se čte neinicializovaný; zip.cpp:3292 chybí NULL-check; filecomp/controls.cpp:24,39 neomezené strcpy; viewer3.cpp:30/35 useknutí captionu uprostřed znaku. Přidat sem i jednořádkovku: src/plugins/codeview/test/run_tests.cmd hlásí RESULT: FAILURES kvůli Node 20 vs. ESM — dokud to svítí červeně, maskuje to skutečné regrese."

## Context

This is item 1 of `specs/NEXT-WORK.md`, the consolidated continuation written
on 2026-09-02. It closes six small defects that are already **recorded** but
were never **fixed**:

- Five were found by feature 069 while it worked on other things and written
  down in `specs/069-finish-encoding-fixes/REMAINING-WORK.md` §3. Feature
  069's charter (its FR-001) forbade any code change without a review finding
  behind it, so it recorded them and moved on. Every one of them is a
  *robustness* defect — a write past the end of a buffer, a value read before
  it was set, a missing check on a caller-supplied argument, an unbounded copy,
  and a text cut in the middle of a character. None of them is reachable with
  the data the product ships today, which is exactly why they have stayed
  open: nobody can see them, so nobody has been hurt by them yet.
- One was found by feature 074 and recorded at the end of its `fix-log.md`:
  the Code Viewer's automated test runner reported `RESULT: FAILURES` on the
  development machine before *and* after that feature, for a reason that had
  nothing to do with the product. A check that is always red cannot tell
  anyone when it turns red for a real reason.

The batch is deliberately small. Its value is not any one fix but the fact
that, once it lands, the recorded-but-open list from 069 §3 is empty, so the
next reader of that handoff meets only the genuinely feature-sized work.

### Where each defect stands at HEAD (`640b94a`, 2026-09-02)

Feature 069's own protocol (`contracts/fix-protocol.md`, step A0 — *still
defective at HEAD?*) caught three of its 34 items already fixed and five site
references stale. The same check was run for this batch before writing this
document. Line numbers below are the ones valid at `640b94a`; the handoff's
numbers were written on 2026-08-24 and have drifted by one to ten lines.

| # | Site at HEAD | Recorded as | Status at HEAD |
|---|---|---|---|
| D1 | `src/codetbl.cpp:874` (`CCodeTables::GetCodeName`) | one-byte overflow — a conversion name of exactly the caller's buffer length is copied whole and its terminator written one past the end | **Present.** `if (len > bufferLen)` still reads `>`; the callers pass 200-byte (`viewer3.cpp:58`, `:1914`) and 1024-byte (`dialogs3.cpp:136`) buffers. The longest shipped name is 33 bytes. The same function also copies the name into a fixed 1024-byte scratch buffer without a bound. **Correction (2026-09-02):** the parser clamps every stored name to 199 bytes (`codetbl.cpp:59,154`), so neither overflow is reachable even with a hand-edited `convert.cfg` — see research.md R1. The fix stands; only this reachability claim was wrong |
| D2 | `src/viewer3.cpp:3301` (viewer's *Coding* menu initialisation) | the result of the name → table lookup is ignored, so when the tables are not loaded the default-item id is read from an unset variable | **Present.** Reachable only when the table object could not be created at all (the loader accepts even a damaged or missing `convert.cfg` by falling back to the best available set, so an *unloaded* state means an allocation failure); the menu is then built empty and the default-item call receives an arbitrary id. No fixture provokes it — it is a code-level defect with no reproducer |
| D3 | `src/zip.cpp:3301` (`CSalamanderGeneral::GetConversionTable`, a plugin-facing service) | the `conversion` argument is not checked for NULL, unlike `table` two lines above; a NULL from a plugin reaches the trace formatter and the lookup | **Present.** Pre-existing; no shipped plugin passes NULL |
| D4 | `src/viewer3.cpp:31` and `:36` (`CViewerWindow::SetViewerCaption`) | the file name (or the plugin-supplied caption) is copied into the title with a 259-byte cut that can land in the middle of a multi-byte character; the title is then not valid text, the Unicode title path rejects it and the whole title — file name included — is drawn through the legacy code page | **Present.** File names are stored long-path capable (up to ~98 KB of UTF-8 in principle, `viewer.cpp:564`), so any accented path longer than 259 bytes can hit it; feature 069's title fixes (F-P4-02) do not help these paths |
| D5 | `src/plugins/filecomp/controls.cpp:24` and `:39` (`CFileHeaderWindow`, the File Comparator's per-file header) | text is copied into a fixed 260-byte member without a bound | **Present.** Safe today only by construction: every caller hands it either an empty string or a path that itself lives in a 260-byte buffer |
| D6 | `src/plugins/codeview/test/run_tests.cmd` (the Code Viewer's automated checks) | the tokenizer-worker harness fails under Node 20 because `web/worker.js` is an ES module in a tree with no `package.json`, and Node 20 treats a bare `.js` file as CommonJS; Node 22.7+ detects the module kind by default | **Changed since it was recorded.** The development machine now runs Node **v24.19.0** and `run_tests.cmd` reports *all codeview checks passed* (verified 2026-09-02). The cause is environmental and still latent: the same tree on any Node 20 machine fails the same way. No repository workflow runs these checks, so there is no CI signal either way |

### What this feature is not

It is not a continuation of the encoding review. The five systemic clusters
(B-1 to B-5), the nine sites 069 chose not to convert, and the archive-listing
encoding (F-P1-05) are recorded with their reasons in
`specs/069-finish-encoding-fixes/REMAINING-WORK.md` and stay there. In
particular, the bytes of a conversion table's **name** are handed to plugins
and stored by two of them; this feature reads those names and copies them
safely, and does not change a single byte of them.

## Scope

**In**: the six sites D1–D6 above, each fixed minimally and in the shape the
surrounding code already uses. Every change traces to exactly one of them.

**Out**: any other site, however adjacent; any change to what a plugin sends
or receives (interface version stays at 106; `src/plugins/shared/` is not
touched); any change to shipped data (`convert.cfg`, language files, the
Code Viewer's web assets and their resource table); any change to the meaning
of the six functions for well-formed input — a caller that gets a correct
result today gets a byte-identical result afterwards.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The application never writes past its own storage while handling names and headers (Priority: P1)

A user works with the viewer's conversion tables and the File Comparator as
they always have. Whatever the length of a conversion name in their
`convert.cfg`, and whatever the length of the paths the comparator shows above
its two panes, the application keeps every copy inside the space it owns. No
combination of name length and buffer size can corrupt memory — not the
one-byte case that exists today (D1), and not the unbounded ones behind it (D1's
scratch copy, D5).

**Why this priority**: memory corruption is the one class of defect whose
consequence is unbounded — a crash at best, silently wrong data at worst — and
it is invisible until it is not. Closing it costs an hour; leaving it open costs
a debugging week the day someone edits `convert.cfg` or a long path reaches the
comparator.

**Independent Test**: call the conversion-name lookup with a buffer exactly as
long as the name it will receive, and hand the comparator's header a text longer
than its storage, under a build that detects out-of-bounds writes (the Debug
build's heap checks, or a deliberately fenced buffer in a unit test). Delivers a
self-contained hardening even if nothing else in this feature landed.

**Acceptance Scenarios**:

1. **Given** a conversion name of exactly *N* bytes, **When** it is requested
   into a buffer of *N* bytes, **Then** the buffer receives the first *N−1*
   bytes and a terminator, nothing is written beyond it, and the call reports
   that the name did not fit — exactly as it already does for a name of *N+1*
   bytes.
2. **Given** a conversion name longer than the function's own scratch space
   (1024 bytes, editable in `convert.cfg`), **When** it is requested, **Then**
   nothing is written past that scratch space and the caller still receives a
   terminated, truncated name and the *did not fit* result.
3. **Given** a name of any length below the caller's buffer, **When** it is
   requested, **Then** the result is byte-identical to today's.
4. **Given** the File Comparator's header receives a text longer than its
   storage, **When** it is set, **Then** the header stores and shows a
   terminated prefix cut on a character boundary, and nothing is written past
   its storage; a text that fits is stored and shown unchanged.

---

### User Story 2 - The viewer and the conversion service behave predictably when the tables are unavailable (Priority: P1)

The conversion tables are loaded from `convert.cfg` at the first viewer window.
When that load fails — the folder is missing, the file is damaged, memory is
short — the viewer still opens (it does today) and its *Coding* menu must not
act on a value that was never set (D2). Likewise, a plugin that asks the core
for a conversion table by name must get a clean *not found* for a missing name,
not a crash (D3).

**Why this priority**: an unset value used as a menu-item id is undefined
behaviour with no reproducer — the kind of bug that shows up once in a crash
report and never again. The NULL check is the plugin-facing twin: the service
already refuses a NULL output buffer with a trace line, and refusing a NULL
input name the same way costs two lines.

**Independent Test**: the unloaded state cannot be provoked with data (a
missing or damaged `convert.cfg` falls back to another set), so D2 is shown by
forcing the state in a debugger or by a test-side harness of the lookup, and
D3 by calling the conversion-table service with a NULL name from a test or a
throwaway plugin. Opening the viewer with the conversion folder removed is
still worth doing as the "nothing else changed" check.

**Acceptance Scenarios**:

1. **Given** the conversion tables failed to load, **When** the viewer's
   *Coding* menu is prepared with auto-select off, **Then** no default item is
   set from an unset value: the (empty) menu ends up with no default and the
   viewer keeps working. When the tables *are* loaded but the stored default
   names a conversion that no longer exists, today's outcome — the *none*
   entry becomes the default — is unchanged.
2. **Given** the tables loaded normally and the stored default names an
   existing conversion, **When** the menu is prepared, **Then** that item is the
   default, exactly as today.
3. **Given** a plugin asks for a conversion table with a NULL name, **When** the
   service is called, **Then** it returns *not found*, records the invalid
   argument in the trace the way it already does for a NULL table buffer, and
   does not touch the tables.
4. **Given** a plugin asks with any non-NULL name, **When** the service is
   called, **Then** the result and the table bytes are identical to today's.

---

### User Story 3 - The viewer's title stays readable for very long accented paths (Priority: P2)

A user opens a file whose full path is longer than 259 bytes and contains
accented characters — deep folder trees with Czech, Slovak or Hungarian names
reach this easily, and the product has supported such paths since feature 012.
The viewer's title bar shows the file name, then *Viewer* and the coding, in the
user's language, exactly as it does for short paths. Today the 259-byte cut can
fall inside a character; the title then fails as text and the *entire* title —
including the translated word *Viewer* and the file name — is drawn through the
legacy code page, which shows the accented characters wrong (D4).

**Why this priority**: it is user-visible and it undoes a fix users already
have (feature 069's F-P4-02 made these titles render correctly for short paths).
Lower than the memory items only because the failure is cosmetic.

**Independent Test**: open a file under a path of 300+ bytes with accented
folder names in the Czech UI and read the title bar; compare with the same file
under a short path.

**Acceptance Scenarios**:

1. **Given** a file whose full UTF-8 path is longer than 259 bytes and whose
   byte 259 falls inside a multi-byte character, **When** the viewer opens it,
   **Then** the title is never torn: it shows a file-name prefix that ends on a
   character boundary, followed by the translated *Viewer* and coding, all
   rendered through the same title path as a short name, with every accented
   character correct.
2. **Given** a plugin supplies its own caption longer than 259 bytes, **When**
   the viewer is shown, **Then** the same rule applies to that caption.
3. **Given** a path of 259 bytes or fewer, **When** the viewer opens it,
   **Then** the title is byte-identical to today's.

---

### User Story 4 - The Code Viewer's automated checks give the same verdict on every supported Node version (Priority: P3)

A maintainer runs `src\plugins\codeview\test\run_tests.cmd` before a commit or a
release. The verdict reflects the plugin's sources, not which Node major happens
to be installed. On Node 20 the tokenizer-worker harness currently fails for an
environmental reason; on the development machine, now on Node 24, it passes.
A red line that means "wrong Node" is indistinguishable from a red line that
means "the plugin broke" (D6).

**Why this priority**: it is developer-facing only and it is currently green
where it is run. It stays in the batch because the two-state harness is a
trap for any second machine or a future CI job, and the remedy is one line.

**Independent Test**: run the harness on Node 20 and on Node 22+ and compare
verdicts.

**Acceptance Scenarios**:

1. **Given** Node 20 is the installed runtime, **When** the runner is executed
   against the committed sources, **Then** every harness runs and the overall
   verdict is *all codeview checks passed*.
2. **Given** Node 22 or newer, **When** the runner is executed, **Then** the
   verdict is unchanged from today (*all codeview checks passed*).
3. **Given** a real regression in the tokenizer worker, **When** the runner is
   executed on either Node major, **Then** the worker harness fails and the
   overall verdict is *FAILURES* — the fix must not silence the check.
4. **Given** the plugin's data harness rule that the resource table lists
   every generated web asset, **When** the remedy is applied, **Then** that
   harness still passes and no new file is shipped inside the plugin.

---

### Edge Cases

- A conversion name of exactly the buffer length (the D1 boundary), one byte
  shorter, one byte longer, and zero length (the *none* entry, which is a
  translated string).
- A conversion name in `convert.cfg` longer than 1024 bytes: the parser stores
  it whole; the lookup must not assume any maximum.
- The viewer opened when `convert.cfg` is absent, unreadable, or empty (the
  loader falls back to the best available set — must stay that way); and
  opened when it is present but the stored default names a conversion that no
  longer exists (already handled today — the default becomes *none*). The
  D2 state — no table object at all — is the one case beyond both.
- A plugin-supplied caption (D4's second site) that is itself torn or not
  valid UTF-8 before the viewer sees it: the viewer must not make it worse and
  must keep the fallback that exists today for text it cannot decode.
- A path exactly 259 bytes long ending in a complete multi-byte character —
  must not lose that character (the "obvious" trim eats it; feature 069's
  helper is defined precisely to leave a complete character alone).
- A path whose cut point lands on a lone-surrogate encoding (WTF-8, feature
  066): the boundary rule treats its three bytes as one character.
- The File Comparator's header set to an empty string (the common case on
  every reset) — unchanged.
- The Code Viewer harness on a machine with no Node at all: fails today, fails
  afterwards, out of scope.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001 (Traceability)**: Every code change MUST trace to exactly one of D1–D6, and every one of D1–D6 MUST receive a recorded disposition — *fixed and accepted* or, for D6 only, *verify-closed* if the maintainer decides the environmental fix is not wanted (see Assumptions). No change without an item behind it; no adjacent cleanup, however tempting (constitution principle III).
- **FR-002 (D1 — bounded name copy)**: The conversion-name lookup MUST never write past the caller's buffer or past its own scratch space for a name of **any** length, MUST always leave the caller's buffer terminated, and MUST keep its existing contract: the full name when it fits and a *fits* result; a terminated prefix and a *did not fit* result otherwise. The boundary case — a name exactly as long as the buffer — MUST be treated as *did not fit*, like every longer name.
- **FR-003 (D2 — no unset default)**: When the conversion tables are not loaded, the viewer's *Coding* menu MUST reach a defined state without reading a value that was never set. When the tables are loaded — whether the stored default resolves to an entry or falls back to *none* — behaviour MUST be unchanged.
- **FR-004 (D3 — NULL name refused)**: The plugin-facing conversion-table service MUST refuse a NULL conversion name the same way it already refuses a NULL table buffer — return *not found*, record the invalid argument in the trace, touch nothing — and MUST behave identically to today for every non-NULL name. The service's documented contract in the shared plugin header is unchanged (it already says *not found* means the table is not valid) and the header is not edited.
- **FR-005 (D4 — a title that is never torn)**: The viewer title MUST never contain a torn multi-byte sequence from the file name or the plugin caption. Shortening is permitted (a title bar cannot show hundreds of characters anyway) but MUST cut only on a character boundary — WTF-8 aware, and leaving a *complete* final character alone. A title built from a name of 259 bytes or fewer MUST be byte-identical to today's. The existing fallback for a title the Unicode path cannot decode MUST remain (never blank the title; principle A4 of the 069 protocol).
- **FR-006 (D5 — bounded header text)**: The File Comparator's header MUST accept a text of any length without writing past its storage, MUST store a terminated prefix cut on a character boundary when the text does not fit, and MUST store and display a text that fits unchanged.
- **FR-007 (D6 — one verdict per source tree)**: The Code Viewer's test runner MUST produce the same verdict on Node 20 and on Node 22+ for the same sources, MUST still fail when a harness genuinely fails, and MUST NOT add a file to the shipped plugin or change what the data harness checks. Any remedy that adds a file under `web/` MUST be validated against the data harness's resource-table rule before it is chosen.
- **FR-008 (Fail-first evidence)**: Every fix MUST carry a check that was shown to fail before the change and pass after it: a unit test in the test program where the logic is reachable from it, otherwise a recorded scenario with the exact reproduction (command, fixture, observed output before and after). "It compiles" is not evidence.
- **FR-009 (No behavioural change for well-formed input)**: For every input a function handles correctly today, the output MUST be byte-identical after the change — English and non-English UI alike. The plugin interface version stays at 106 and no file under `src/plugins/shared/` changes.
- **FR-010 (Protocol)**: The per-fix protocol of feature 069 (`specs/069-finish-encoding-fixes/contracts/fix-protocol.md`) is binding: confirm the defect at HEAD before touching it (done above, re-done at implementation time), enumerate the consumers of every changed symbol yourself, fix in the house shape using the existing helpers, never blank text or skip an operation, and have each fix reviewed by someone who did not write it before it lands.
- **FR-011 (Record)**: A running `fix-log.md` in the feature directory MUST record, per item, the HEAD check, the consumers found, the check written, and the review verdict — so that the next handoff can say "closed" with a line of evidence rather than a claim. On completion, `specs/069-finish-encoding-fixes/REMAINING-WORK.md` §3 and `specs/NEXT-WORK.md` item 1 MUST be marked closed with a pointer to that log.
- **FR-012 (Changelog)**: User-visible fixes MUST appear in `CHANGELOG.md` in the user's terms and truthful about scope. D4 is user-visible (title of a long accented path). D1, D3 and D5 are hardening with no shipped reproducer and MUST NOT be presented as repairs of something users saw — at most one honest line ("hardened … no known way to trigger it"). D2 is recorded only if a reproducer with shipped data is found. D6 is developer-only and stays out of the changelog. No version bump is part of this feature.

### The defect inventory

| # | Site (HEAD `640b94a`) | Class | User-visible today? | Disposition wanted |
|---|---|---|---|---|
| D1 | `src/codetbl.cpp:874` + the unbounded scratch copy above it | out-of-bounds write | No (longest shipped name 33 B) | fixed |
| D2 | `src/viewer3.cpp:3301` | read of an unset value | Only if table loading fails | fixed |
| D3 | `src/zip.cpp:3301` | missing argument check on a plugin-facing service | No (no plugin passes NULL) | fixed |
| D4 | `src/viewer3.cpp:31`, `:36` | torn text → wrong rendering | Yes, for accented paths > 259 B | fixed |
| D5 | `src/plugins/filecomp/controls.cpp:24`, `:39` | unbounded copy | No (callers are bounded) | fixed |
| D6 | `src/plugins/codeview/test/run_tests.cmd` | environment-dependent test verdict | Developer-only; green on Node 24, red on Node 20 | fixed, or verify-closed with the Node minimum written into the runner's header |

### Key Entities

- **Conversion name**: the display name of one entry in `convert\<set>\convert.cfg` (e.g. "ISO-8859-2 - CP1250"), stored as the file's own bytes, handed to plugins and persisted by two of them — read and copied here, never re-encoded.
- **Viewer title**: the window caption composed of a file name or plugin caption, the translated word *Viewer*, and the coding; UTF-8 throughout since feature 069.
- **Comparator header**: the one-line text above each pane of the File Comparator, showing the compared file's path.
- **Test verdict**: the single *passed* / *FAILURES* line the Code Viewer's runner ends with, computed from three harnesses.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of D1–D6 carry a recorded disposition with evidence; the recorded-but-open list of `069/REMAINING-WORK.md` §3 is empty afterwards.
- **SC-002**: 100% of fixes carry a check shown to fail before and pass after; 100% carry an *accepted* review verdict from a reviewer who did not write the fix, with zero regressed surfaces.
- **SC-003**: Zero byte differences, against the pre-feature build, in the output of the six functions for every well-formed input exercised by the checks, in the English and in a non-English UI; plugin interface version unchanged; no diff under `src/plugins/shared/`.
- **SC-004**: All automated gates green: full Debug and Release builds with no new warnings in changed files; the test program at or above its HEAD count (1,301 per the 069 handoff) with 0 failing and at least one new check per fix where the logic is reachable; the strict encoding guard at `TOTAL: 0`; no new leak or handle report over a start/exit cycle with a viewer and the comparator opened.
- **SC-005**: The Code Viewer's runner reports *all codeview checks passed* on Node 20 and on Node 24 for the same sources, and reports *FAILURES* on both when the worker harness is made to fail deliberately.
- **SC-006**: In the Czech UI, a file under a 300+-byte accented path opens in the viewer with every accented character in the title correct, side by side with the pre-feature build showing the defect.
- **SC-007**: The changelog contains the user-visible fix in the user's terms and presents no hardening-only item as a repair of something users saw.

## Assumptions

- **Shortening the viewer title is acceptable.** The title bar cannot display a 300-character path; keeping a boundary-safe cut at the existing length is the minimal fix and matches what users see today for long ASCII paths. Showing the whole path would be a behaviour change and is not wanted here.
- **D6 is worth one line, not a policy.** Node 24 is now installed on the development machine and the harness is green there; no repository workflow runs it. The feature makes the runner Node-major-independent because the cost is one line and the alternative — a note saying "needs Node 22.7+" — leaves the trap in place for the next machine. If the maintainer prefers the note, D6 is closed *verify-closed* with the note in the runner's header (FR-001 allows it).
- **The test program cannot reach the core's table code directly.** `saltests` links the shared `src/common/` sources only, so D1–D3 are covered by recorded scenarios or by a small test-side harness, and D4's boundary rule by a unit test on the shared trimming helper it reuses. The plan decides which.
- **The 069 protocol applies unchanged**, including independent review — the batch is small, but two of 069's four review batches were rejected for regressions the fixes themselves introduced, and none of those looked risky either.
- **No version bump and no release** are part of this feature; the changelog entry goes under the next unreleased version when one is opened.
- **`convert.cfg` names are trusted for encoding but not for length.** Their bytes are a plugin-facing contract (feature 069, F-P4-01) and are not touched; their length is user-controlled and is what D1 must survive.

# Feature Specification: Source & Configuration File Viewer

**Feature Branch**: `070-source-viewer-plugin`
**Created**: 2026-08-26
**Status**: Draft
**Input**: User description: "Načti specifikaci zadání implementace nového pluginu ze souboru ./features/source_files_viewer.md. Alokuj několik nezávislých agentů na průzkum možností realizace a především na vhodné existující nástroje / frameworky, které by bylo možné použít." (Load the feature brief from `features/source_files_viewer.md`; allocate several independent research agents to explore implementation options and especially suitable existing tools/frameworks.)

**Origin**: `features/source_files_viewer.md` — a new F3 viewer plugin for source
code and text configuration files with syntax highlighting and several light
and dark colour themes, built analogously to the mdview plugin and sharing the
product's warm WebView2 engine per `architecture/11-webview2-integration.md`.

**Research inputs** (written during specification, 2026-08-26; conclusions
feed the planning phase, the requirements below are the user-facing contract):

| Report | Question answered |
|---|---|
| [research/web-highlighters.md](research/web-highlighters.md) | Which web highlighting libraries exist, their coverage, themes, licences |
| [research/native-and-hybrid.md](research/native-and-hybrid.md) | Do native (C++) or hybrid routes beat the web route on coverage/themes/robustness |
| [research/codebase-integration.md](research/codebase-integration.md) | Viewer registration & priority, mdview anatomy, built-in viewer parity, new-plugin checklist |
| [research/viewer-ux-webview2.md](research/viewer-ux-webview2.md) | Large files, find, themes, encoding, security-with-scripts, instant-open in WebView2 |
| [research/language-detection.md](research/language-detection.md) | File-name → language mapping sources, ambiguous extensions, claim policy, binary sniffing |

---

## Problem Statement

Pressing F3 on a source or configuration file — `.js`, `.ts`, `.cpp`, `.c`,
`.yml`, `.yaml`, `.toml`, `.ini`, `.php`, `.java` and hundreds of other text
formats — opens the built-in text viewer, which shows the file without any
syntax highlighting. The user wants a fast, read-only preview with syntax
colouring appropriate to the file's language, in a choice of several light and
dark colour themes.

The feature adds a new viewer plugin that becomes the primary F3 viewer for
these file types. The brief's stated technical direction — confirmed by the
research above as the best fit for the two stated priorities (maximum format
coverage first, themes second) — is a viewer analogous to the mdview plugin:
an embedded web rendering surface that shares the product's warm, keep-ready
engine (feature 065) so opening is essentially instant, with highlighting
supplied by an established web highlighting library and themes expressed as
ready-made colour schemes. The built-in text viewer remains available for
everything the plugin does not claim or declines (huge files, binary content),
and via Alt+F3 always.

---

## Clarifications

### Session 2026-08-26

- Q: May the viewer's embedded rendering surface execute the plugin's own
  bundled highlighting code (scripts enabled on this plugin's controllers
  only), unlike the Markdown viewer which keeps scripts off? → A: Yes —
  scripts are enabled only on this plugin's own controllers, with the full
  compensating lockdown (FR-030…FR-033); the Markdown viewer's zero-script
  posture is unchanged. This selects the web-highlighting-library route
  (≈240 formats, ready-made themes) over the native/hybrid fallback.
- Q: Should highlighting definitions and themes licensed GPL-3.0-only be
  excluded from the shipped plugin, even at the cost of a handful of
  languages losing highlighting? → A: Yes — only GPLv2-compatible
  (MIT/BSD/ISC/Apache-class) grammar and theme files ship; formats whose
  only definition is GPL-3.0 open as plain text or use a permissively
  licensed alternative. Distribution stays cleanly GPLv2-or-later.
- Q: Must the viewer support moving to the next/previous file of the panel
  from within an open viewer window (as the built-in text viewer does), or
  may that be deferred? → A: Required in v1 (FR-041) — parity with the
  built-in viewer on the types the plugin takes over; the known long-path
  API question is verified early in planning and solved, not used as a
  deferral reason.
- Q: What should the plugin's internal module/directory name be (it fixes
  the `.spl` name, registry key, translation-module identity and default
  ordering among plugins)? → A: **codeview**. Because `codeview` sorts
  alphabetically before other viewer plugins, later-installed plugins' rows
  land above its rows on a default install — harmless because FR-010
  guarantees an empty mask intersection with them.
- Q: Which plain-text file types should the viewer claim by default — is the
  specified policy (never claim `*.txt`; `*.log` only as an off-by-default
  opt-in) the intended behaviour? → A: **Claim both `*.txt` and `*.log` by
  default** — they open in the new viewer as plain text with line numbers,
  themes and all viewer functions; the built-in viewer takes over above the
  viewer limit (and for binary content) via the normal decline cascade. The
  earlier opt-in mechanism is dropped; users who prefer the built-in viewer
  for these types remove the corresponding family entry.

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Syntax-highlighted view on F3 (Priority: P1)

A user presses F3 on a source file (`main.cpp`, `app.ts`, `config.yaml`,
`setup.iss`, `Dockerfile`). A read-only viewer window opens essentially
immediately and shows the file with syntax highlighting appropriate to its
language: comments, strings, keywords, numbers and other token classes are
visually distinguished in the active colour scheme, with line numbers in a
gutter.

**Why this priority**: This is the core motivation of the feature — the
minimum viable product. Even with a single default theme and a reduced format
list, a highlighted read-only view on F3 already delivers the user value the
brief asks for.

**Independent Test**: Press F3 on files of ten well-known languages (C++,
TypeScript, YAML, JSON, Python, shell, XML, INI, PHP, Java); verify each opens
in the new viewer with visibly correct token colouring and line numbers, and
that Esc closes the window.

**Acceptance Scenarios**:

1. **Given** a `.cpp` file under 1 MB, **When** the user presses F3, **Then**
   a read-only viewer opens showing the file with C++ highlighting — comments,
   strings, keywords, numbers and preprocessor lines in distinct colours of
   the active scheme — and with line numbers.
2. **Given** a claimed file whose language has no highlighting definition,
   **When** it is opened, **Then** it displays as plain monospaced text with
   the same theme, line numbers and encoding handling as highlighted files,
   and the status area names the identified format.
3. **Given** any file open in the viewer, **When** the user attempts to type
   or edit, **Then** the content never changes and the file on disk is never
   modified (read-only in the strict sense).
4. **Given** a file within the highlighting limit, **When** it opens, **Then**
   the first screen of text is visible within the instant-open budget even if
   highlighting of the remainder is still completing, and scrolling and keys
   stay responsive throughout.

---

### User Story 2 - Light and dark colour themes (Priority: P1)

A user working with the application's dark theme opens a source file and gets
a dark highlighted view by default; a user on the light theme gets a light
one. From the viewer's menu (and a keyboard shortcut) the user switches among
several built-in schemes — at least three light and three dark — and the
choice persists across sessions.

**Why this priority**: The brief names "at least several light and dark
themes" as a must-have, and dark-mode readers are the primary audience of a
code viewer.

**Independent Test**: Cycle through every shipped scheme on one open file;
verify the switch is immediate (no reload, scroll and selection kept), that
scrollbars and all viewer chrome follow the scheme, and that the choice is
remembered after closing and reopening the application.

**Acceptance Scenarios**:

1. **Given** the viewer ships its scheme set, **When** the user opens the
   scheme menu, **Then** at least three light and three dark schemes plus a
   "follow the application theme" option are available.
2. **Given** an open file and a scheme switch, **When** the user selects a
   different scheme, **Then** the view recolours without reloading, keeping
   scroll position and selection.
3. **Given** the dark application theme and a dark scheme, **When** a viewer
   window opens, **Then** no white or wrong-coloured area is visible at any
   moment from window appearance to painted text, and scrollbars are dark.
4. **Given** the source viewer's scheme is changed, **When** a Markdown file
   is opened in the Markdown viewer in the same session, **Then** the Markdown
   viewer's appearance is unaffected.

---

### User Story 3 - Safe viewing of untrusted files (Priority: P1)

A user presses F3 on a file downloaded from an untrusted source. Whatever the
file contains — markup, script fragments, hostile escape sequences — the
viewer displays it as literal text and nothing else happens: no code from the
file runs, no network request is made, no window, download, dialog or external
application is launched.

**Why this priority**: A file manager's viewer is pointed at hostile input as
a matter of course. The mdview plugin established the product's lockdown
posture for web-rendered content; a source viewer whose rendering surface
executes its own bundled highlighting code has a strictly larger attack
surface and must prove the same user-visible guarantees.

**Independent Test**: Open a fixed corpus of hostile files (embedded
`<script>`, event-handler attributes, entity references, `javascript:` URLs,
bidi override characters, oversized single lines, binary-looking data) with a
network monitor attached; verify literal display, zero unsolicited network
I/O, zero navigation, zero UI beyond the viewer itself.

**Acceptance Scenarios**:

1. **Given** a file containing HTML tags, `</script>` sequences,
   event-handler attributes or entity references, **When** it is viewed in
   any band (highlighted or plain) and any supported encoding, **Then** the
   content displays literally, character for character.
2. **Given** any file and any key combination, **When** it is open in the
   viewer, **Then** no developer tools, browser menu, print/save dialog,
   download, new window or external application ever appears.
3. **Given** a viewing session, **When** network traffic is observed, **Then**
   the viewer makes no network request of any kind; everything it renders is
   served from the plugin's own shipped assets.

---

### User Story 4 - Hundreds of formats, correctly identified (Priority: P2)

A user browses a real-world repository: `.tsx`, `.rs`, `.toml`, `.gradle`,
`.ps1`, `.cmake`, `Dockerfile`, `Makefile`, `.gitignore`, `.vcxproj`, `.reg`,
`.iss` — F3 opens each in the viewer with the right language. Ambiguous names
resolve sensibly (`.h` as C++ unless the content says Objective-C; a `.ts`
that is actually an XML translation file or a video stream is not treated as
TypeScript). Extension-less scripts are recognised by their first line.

**Why this priority**: Breadth is the brief's explicitly stated first
priority ("hundreds of source/config formats"), but it builds on US1's
working viewer and is separately testable and shippable as an expanding
claim list.

**Independent Test**: Run the language-identification test set from the
research (exact names, multi-dot suffixes, ambiguous extensions, shebangs,
modelines, first-byte signatures) and verify each resolves to the documented
language or documented default.

**Acceptance Scenarios**:

1. **Given** the shipped claim list, **When** it is generated and audited,
   **Then** it covers at least 200 languages/formats and at least 700
   file-name patterns, organised into the 8 families of FR-009, and contains
   no pattern claimed by another shipped viewer plugin (no `*.md`,
   `*.markdown`, `*.csv`, `*.dbf`, `*.tsv`) and no `*.*`; `*.txt` and
   `*.log` are claimed as their own removable family.
2. **Given** `Dockerfile`, `Makefile`, `CMakeLists.txt`, `.gitignore`,
   `.editorconfig`, **When** each is opened, **Then** the correct format is
   identified from the exact file name.
3. **Given** an extension-less file starting `#!/usr/bin/env python3` or a
   file with a Vim/Emacs modeline, **When** opened, **Then** the interpreter
   or modeline language is used.
4. **Given** the documented ambiguous extensions (`.h`, `.m`, `.pl`, `.v`,
   `.ts`, `.inc`, `.sql`, …), **When** files with distinguishing content are
   opened, **Then** each resolves per the documented default and
   disambiguation rules.
5. **Given** an open file, **When** the user overrides the language from the
   viewer's language picker, **Then** the view re-highlights in the selected
   language for this view.

---

### User Story 5 - Large, odd and binary files degrade gracefully (Priority: P2)

A user opens a 3 MB generated SQL dump: it appears as plain monospaced text
with line numbers, find and themes, and a one-line notice says highlighting is
off for this file. A 50 MB log-like file or a `.ts` file that is really a
video opens in the built-in text viewer exactly as if the plugin were not
installed. A minified single-line JavaScript file scrolls smoothly.

**Why this priority**: The viewer must never make any file *worse* to open
than today. Degradation rules protect the P1 experience against real-world
inputs.

**Independent Test**: Open files at 10 KB / 100 KB / 1 MB / 5 MB / 25 MB, a
single-line multi-megabyte file, and binary files with claimed extensions;
verify each lands in the documented band (highlighted / plain-with-notice /
built-in viewer) within its budget.

**Acceptance Scenarios**:

1. **Given** a text file over the highlighting limit but under the viewer
   limit, **When** opened, **Then** it displays as plain text with line
   numbers, wrap, find, go-to-line, themes, zoom and copy all working, and
   the viewer states why highlighting is off.
2. **Given** a file over the viewer limit (default 20 MB) or a file whose
   content is binary, **When** the user presses F3, **Then** the built-in
   text viewer opens it directly — no error message, no empty window, no
   extra step — and the decision itself is imperceptible (well under a
   second, without reading the whole file).
3. **Given** a single-line file of several megabytes (minified script),
   **When** opened, **Then** it appears within the plain-text budget, scrolls
   horizontally, never triggers an unresponsive state, and toggling wrap
   completes within a few seconds.
4. **Given** both limits in the plugin's configuration, **When** the user
   changes them, **Then** the new values take effect for the next view
   without restarting.

---

### User Story 6 - A complete read-only viewer (Priority: P2)

The viewer is a daily tool, not a demo: the user searches with match
highlighting and a match counter, jumps to a line, toggles word wrap, zooms,
selects and copies text that pastes byte-faithfully, sees the file's
encoding, line-ending style, language and position in a status bar, and can
override a misdetected encoding — with the keyboard behaving like the
built-in viewer wherever the two overlap.

**Why this priority**: Feature parity with the built-in viewer's everyday
functions is what makes "primary F3 viewer" acceptable; without it users
would revert.

**Independent Test**: Execute the keyboard/feature parity matrix (find,
next/prev match, wrap, go-to-line, zoom, select/copy, select-all, encoding
override, Esc) against both viewers on the same file set and compare.

**Acceptance Scenarios**:

1. **Given** an open file and a search term, **When** the user searches,
   **Then** all matches are marked, the current one is visually distinct and
   scrolled into view, a "n of N" counter is shown, next/previous match works
   from the keyboard, and the search never reloads the document or loses the
   scroll position; case-sensitive and whole-word options are available.
2. **Given** a selection, **When** the user copies it, **Then** the clipboard
   contains exactly the selected source text — Windows line breaks, tabs and
   trailing spaces preserved, line numbers and any viewer markers excluded.
3. **Given** the go-to-line command with "line" or "line:column" input,
   **When** confirmed, **Then** the target line scrolls to the middle of the
   view and is briefly marked; an out-of-range number clamps to the last line.
4. **Given** the viewer window, **Then** a status bar drawn in the product's
   native style shows file name, size, line count, detected or selected
   encoding, line-ending style (CRLF/LF/CR/mixed), identified language and
   zoom level, and follows the light/dark theme.
5. **Given** a file in UTF-8 (with or without BOM), UTF-16 LE/BE, or a
   single-byte code page recognised by the product (including Windows-1250,
   ISO-8859-2 and CP852), **When** opened, **Then** all characters display
   correctly, the encoding is named in the status bar, and the user can
   override it from the menu using the same coding list the built-in viewer
   offers; a BOM is never shown as a character; invalid byte sequences show
   a replacement character and the file still opens.
6. **Given** any keyboard shortcut the built-in text viewer defines, **When**
   pressed in the new viewer, **Then** it performs the equivalent action or
   is a documented no-op — specifically Esc always closes, and no key ever
   triggers a browser behaviour (reload, print, save).

---

### User Story 7 - Instant display, zero cost before first use (Priority: P3)

The first source file of a session may pay the engine start-up moment (as the
Markdown viewer's first open does); every later open — including after all
viewer windows were closed — shows text essentially instantly. A user who
never opens a source file gets today's application byte-for-byte: no
background work, no start-up difference, no idle footprint.

**Why this priority**: This inherits feature 065's established behaviour and
infrastructure; it is a strong expectation but rides on an existing, proven
mechanism rather than new invention.

**Independent Test**: Repeat feature 065's measurement protocol against the
source viewer: cold first open, warm subsequent opens after closing all
windows, application start-up and idle footprint in a session that never
views a source file.

**Acceptance Scenarios**:

1. **Given** a warm session (any WebView2 viewer used earlier), **When** the
   user presses F3 on a ≤ 100 KB source file, **Then** its text is visible
   within the instant-open budget adopted from feature 065, and never slower
   than the same file in the Markdown viewer's source mode.
2. **Given** a fresh session, **When** the first source file is opened,
   **Then** it is no slower than the Markdown viewer's first open of a
   session, and no engine work happened before this first use.
3. **Given** a session in which no source file is ever viewed, **Then**
   start-up time, memory and background activity are identical to the
   current build.
4. **Given** the keep-ready option in the plugin's configuration is turned
   off, **Then** the plugin releases the engine when its last window closes,
   exactly as the Markdown viewer's equivalent option behaves.

---

### Edge Cases

- **Binary masquerading as text** (a `.ts` MPEG transport stream, a `.h`
  WinHelp file, a 2 GB database renamed `.sql`): classified binary from the
  first few KB and declined to the built-in viewer — which shows hex — with
  no error and no full-file read.
- **File replaced on disk after the pre-open check** (or reached by in-viewer
  navigation) turning out binary: a one-line notice with an "open in built-in
  viewer" action, never a garbled render.
- **Empty file, file containing only a BOM, file with mixed line endings**:
  opens cleanly; status bar reports 0 lines / the BOM-less emptiness / "mixed"
  EOL.
- **File names with unpaired UTF-16 surrogates** (legal on NTFS, feature 066):
  the plugin declines them to the built-in viewer rather than risking a wrong
  path recomposition.
- **Rendering engine not installed / disabled**: F3 falls back to the built-in
  text viewer silently (mdview parity); keep-ready failures are silent and
  never produce a dialog outside an actual view attempt.
- **Engine process crash while viewing or while idle-warm**: the open window
  recovers or closes with a clear message; the next F3 works (re-arm or
  degrade), never a permanently broken viewer.
- **Two viewer windows open simultaneously** (different files, different
  languages): independent scroll/find/language state; closing one never
  affects the other.
- **Scheme switched while highlighting is still in progress**: no crash, no
  mixed-theme rendering after completion.
- **Viewing a file inside an archive** (viewer receives an extracted
  temporary copy): works identically; the temporary file's name still drives
  language detection.
- **Extremely deep nesting / pathological tokenisation input**: highlighting
  may give up (plain band) but the viewer never hangs the application; the
  window stays closable.
- **User removed or reordered the plugin's viewer entries**: respected
  permanently; "restore default file types" in the plugin configuration is
  the only way they come back, and it touches nothing else.

## Requirements *(mandatory)*

### Functional Requirements

**Viewing & highlighting**

- **FR-001**: The plugin MUST provide a read-only viewer window opened by the
  product's viewer mechanism (F3 and equivalents) for the file types it
  registers; it MUST never modify the viewed file.
- **FR-002**: Files within the highlighting limit MUST display with syntax
  highlighting for the identified language: at minimum comments, strings,
  keywords, numbers and preprocessor/annotation constructs are visually
  distinguished per the active colour scheme.
- **FR-003**: A claimed file whose identified format has no highlighting
  definition MUST display as plain text with identical chrome (theme, line
  numbers, encoding handling) and the format's name shown in the status area.
- **FR-004**: The first screen of text MUST be visible within the instant-open
  budget (FR-036) even while highlighting of the remainder continues;
  scrolling and key handling MUST stay responsive during highlighting.

**Language identification**

- **FR-005**: Language selection MUST consider, in order: exact file name
  (case-insensitive), file-name pattern, longest multi-dot suffix, extension;
  content MUST be consulted only when the name yields no language or several —
  recognising a first-line shebang (including `env` forms), an Emacs/Vim
  modeline in the first or last five lines, and documented first-byte
  signatures (e.g. `<?xml`).
- **FR-006**: The documented ambiguous extensions (research
  `language-detection.md` §3) MUST resolve to their stated defaults, and the
  stated disambiguation rules MUST select the alternative (e.g. `.h` → C++
  unless Objective-C markers; `.ts` containing an XML translation header →
  XML, never TypeScript).
- **FR-007**: The user MUST be able to override the identified language for
  the open view from a language picker; the override applies to that view and
  is not persisted per file.
- **FR-008**: The claimed-type list and the name→language table MUST be
  generated from a versioned, pinned external source plus checked-in overlays,
  committed and reviewed like source; an automated check MUST fail when a
  table entry references a highlighting definition that does not ship
  (such entries must be explicitly marked "no grammar" instead).

**Claimed types & viewer-list integration**

- **FR-009**: On installation the plugin MUST register as the first matching
  F3 viewer for its claimed types, organised into **8 named families**
  (source code, scripts, web, data/configuration, XML-based, build/tooling,
  documentation-adjacent, plain text) whose entries are contiguous in
  Options ▸ Viewers, so removing a family's entries disables exactly that
  family. A family occupies **as many entries as the configuration storage
  allows** — a stored entry is capped at 259 bytes and an over-long entry
  discards every entry below it, so the ~1 000 claimed patterns need about
  50 entries of ≤ 200 bytes (the established house pattern: the picture
  viewer uses 11). An earlier draft of this requirement said "≤ 8 entries";
  that is arithmetically impossible at this coverage and was corrected
  during implementation.
- **FR-010**: The plugin MUST NOT claim `*.md`, `*.markdown`, `*.csv`,
  `*.dbf`, `*.tsv`, `*.*`, or any mask registered by another shipped viewer
  plugin; an automated check MUST prove the intersection with other shipped
  plugins' registrations is empty. `*.txt` and `*.log` ARE claimed by
  default (decided in clarification 2026-08-26) as their own removable
  family entry, opening as plain text per FR-003; the built-in viewer takes
  over above the viewer limit and for binary content via FR-027.
- **FR-011**: On upgrade of an existing installation, no pre-existing viewer
  entry may be deleted, modified or reordered relative to other pre-existing
  entries, and masks or entries the user removed MUST never be re-added
  automatically. Registration MUST never corrupt or truncate the user's
  stored viewer configuration (entries are kept safely under the
  configuration storage's per-entry limits).
- **FR-012**: The built-in text viewer MUST remain reachable for every file:
  by the alternate-viewer command (Alt+F3), by the cascade when the plugin
  declines a file, and by the user removing the plugin's entries. The plugin
  configuration MUST offer "restore default file types", re-creating only the
  plugin's own entries.

**Colour themes**

- **FR-013**: The viewer MUST ship at least three light and three dark colour
  schemes plus a "follow the application theme" mode, which is the default
  (the application's Default theme maps to a light scheme, its Dark theme to
  a dark one).
- **FR-014**: The scheme MUST be switchable from the viewer's menu and by
  keyboard, take effect immediately without reloading (scroll position and
  selection preserved), and persist across sessions.
- **FR-015**: In a dark scheme, no white or wrong-scheme area may ever be
  visible from window appearance to painted text; scrollbars and all in-view
  chrome follow the scheme.
- **FR-016**: The source viewer's scheme choice MUST have no effect on the
  Markdown viewer or any other part of the product in the same session.

**Viewer functions**

- **FR-017**: Find MUST offer case-sensitive and whole-word options, mark all
  matches with the current match visually distinct and scrolled into view,
  show an "n of N" counter, support next/previous from the keyboard, and
  never reload the document or lose the scroll position. The product's own
  find UI is shown — never a browser find bar.
- **FR-018**: Line numbers MUST be shown by default and be toggleable; they
  are never part of copied text. Word wrap MUST toggle from the keyboard and
  menu without reloading, with line numbers staying aligned to logical lines.
- **FR-019**: Go-to-line MUST accept "line" and "line:column", scroll the
  target to mid-view and mark it briefly; out-of-range input clamps to the
  last line.
- **FR-020**: Zoom MUST behave as in the Markdown viewer: Ctrl+wheel and
  Ctrl+±/0, persisted across sessions, current percentage visible.
- **FR-021**: Copy MUST reproduce the selected source text exactly — Windows
  line breaks, tabs and trailing whitespace preserved; select-all selects the
  document text only (no viewer chrome). Tab width MUST be configurable
  (default 4) and an optional show-whitespace mode MUST NOT change what copy
  produces.
- **FR-022**: A status bar in the product's native style MUST show file name,
  size, line count, detected/selected encoding, line-ending style
  (CRLF/LF/CR/mixed), identified language and zoom, and follow the
  application theme (constitution principle VI applies to all native UI).
- **FR-023**: The context menu MUST contain only viewer commands and follow
  the dark theme; the rendering surface's own menus, developer tools, status
  and error UI MUST never appear.
- **FR-024**: The viewer MUST decode UTF-8 (with/without BOM), UTF-16 LE/BE
  (with BOM; without BOM when recognisably UTF-16), and single-byte code
  pages via the product's existing code-page machinery (including
  Windows-1250, ISO-8859-2, CP852), name the result in the status bar, and
  offer a manual override with the same coding list as the built-in viewer.
  A BOM is never displayed; invalid sequences show a replacement character
  and never prevent opening.
- **FR-025**: Every keyboard shortcut of the built-in text viewer MUST map to
  the equivalent action or be a documented no-op; Esc always closes; no key
  may trigger a browser behaviour (reload, print, save, devtools). Printing
  is out of scope for this feature (explicitly documented, not silently
  missing).
- **FR-041**: From an open viewer window the user MUST be able to move to the
  next/previous file of the originating panel using the built-in viewer's
  keys (built-in viewer parity; decided in clarification 2026-08-26). The
  same window is reused (no close-and-reopen flicker), language and encoding
  identification re-run for each file, the decline rules (FR-027) apply —
  showing the notice of FR-029 instead of rendering when the next file is
  binary or over-limit — and long paths work.

**Large files & binary content**

- **FR-026**: Highlighting applies up to a configurable size limit (default
  1 MB) and line-length limit (default 20 000 characters). Between the
  highlighting limit and the viewer limit (default 20 MB) files display as
  plain text with all non-highlighting features working and a one-line
  explanation. Both limits are configurable and take effect without restart.
- **FR-027**: Files above the viewer limit, files classified as binary, and
  files whose names cannot be safely handled (unpaired-surrogate names per
  feature 066) MUST be declined before opening so that the next matching
  viewer — by default the built-in text viewer — opens directly, with no
  error and no empty window. Classification MUST read at most the first few
  KB, complete imperceptibly fast, and agree with the built-in viewer's
  text/hex decision except that UTF-16 text (with or without BOM) counts as
  text.
- **FR-028**: A single-line file of several megabytes MUST open within the
  plain-text budget, scroll, and never render the window or application
  unresponsive; toggling wrap on it completes within a few seconds.
- **FR-029**: If content proves binary only after opening (file replaced on
  disk, in-viewer navigation), the viewer MUST show a notice with an "open in
  built-in viewer" action instead of rendering garbage.

**Security**

- **FR-030**: File content MUST never be interpreted as markup, style or
  code: the hostile-content corpus (script tags, event handlers, entity
  references, `javascript:`/`data:` URLs, bidi overrides, oversized lines)
  displays literally in every band and encoding. The corpus is a maintained
  test asset — extending it is a test change, not a code change.
- **FR-031**: While any viewer window is open, the viewer makes no network
  request of any kind; only the plugin's own shipped assets and the viewed
  text are ever served to the rendering surface, and this is observable in a
  debug build.
- **FR-032**: Whatever the file contains and whatever keys are pressed, the
  viewer never opens developer tools, another window, a download, a
  save/print dialog, or an external application.
- **FR-033**: All rendering assets (page, styles, highlighting definitions,
  themes) ship inside the signed plugin; nothing is loaded from the user
  profile, temporary folders or the network. Script execution is enabled on
  this plugin's own rendering surface only (decided in clarification
  2026-08-26) and is limited to this bundled code; the Markdown viewer's
  stricter zero-script lockdown is unchanged. The full lockdown is applied
  by one shared routine before first navigation and asserted in debug
  builds.

**Performance & shared engine**

- **FR-034**: No background or preparatory work related to this plugin runs
  before its first actual use in a session (feature 065 FR-001 parity);
  after first use the engine stays warm for the session, with the same
  bounded-footprint and silent-failure rules, and an off-by-default-able
  keep-ready option in the plugin configuration mirroring the Markdown
  viewer's.
- **FR-035**: The plugin MUST comply with the WebView2 shared-engine contract
  (`architecture/11-webview2-integration.md`): canonical user data folder,
  the single shared browser-arguments helper — lifted to the shared location
  as this is the second consumer, with the Markdown viewer retested — and
  per-controller settings only; the Markdown viewer's user-visible behaviour
  is unchanged.
- **FR-036**: After the session's first use, opening a ≤ 100 KB file shows
  its text within the instant-open budget adopted from feature 065 (order of
  0.3 s on the reference machine, verified by the same trace-point method)
  and never slower than the Markdown viewer's source mode on the same file;
  the session's first open is no slower than the Markdown viewer's first
  open. Highlighting definitions and themes are loaded only for the language
  and scheme in use.
- **FR-037**: When the rendering engine is unavailable, F3 on a claimed file
  falls back to the built-in text viewer silently; readiness failures never
  surface outside an actual view attempt.

**Configuration & product integration**

- **FR-038**: The plugin MUST have a configuration dialog in the product's
  house style covering: colour scheme, font family and size (default a
  monospaced font shipped with the OS), tab width, the two size limits,
  keep-ready, and "restore default file types". Settings persist under the
  plugin's own configuration storage.
- **FR-039**: All user-visible plugin text MUST be translated into every
  enabled product language through the established translation pipeline (a
  new module registered per the language build policy).
- **FR-040**: The feature MUST NOT change the plugin interface version, the
  Markdown viewer's behaviour, or any existing viewer's registration; the
  product's changelog records the feature per the constitution's release
  documentation rules.

### Key Entities

- **Claimed-type registry**: the generated, committed list of file-name
  patterns the plugin registers, grouped into family entries; derived from a
  pinned external mapping source plus overlays; excludes other plugins'
  masks and the documented non-claims.
- **Language map**: file name/pattern/content-rule → format identity →
  highlighting definition (or "no grammar"); drives detection order FR-005
  and the plain-text fallback FR-003.
- **Colour scheme**: a named light or dark palette for token classes and
  viewer chrome; at least 3 + 3 shipped; one active per user, plus
  follow-application mode.
- **Viewer session/window**: one open file's state — language (detected or
  overridden), encoding (detected or overridden), scroll, selection, find
  state, wrap, zoom; multiple windows are independent.
- **Plugin configuration**: persisted user choices (scheme, font, tab width,
  limits, keep-ready) under the plugin's registry storage.
- **Hostile-content corpus**: the maintained set of adversarial files used to
  verify FR-030…FR-033.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: At least 200 languages/formats open with syntax highlighting
  and at least 700 file-name patterns are claimed, with zero overlap with
  other shipped viewers' registrations.
- **SC-002**: At least 3 light and 3 dark schemes ship; switching schemes on
  an open 1 MB file completes without reload and keeps position/selection.
- **SC-003**: On the reference machine, a warm open of a ≤ 100 KB source file
  shows text in ≤ 0.3 s; the session's first open is no slower than the
  Markdown viewer's first open measured the same day.
- **SC-004**: 100 % of the hostile-content corpus displays literally, with
  zero network requests and zero unexpected UI across the whole test run.
- **SC-005**: The language-identification test set (exact names, multi-dot
  suffixes, ambiguous extensions with content rules, shebangs, modelines,
  signatures) passes 100 %.
- **SC-006**: A 5 MB text file reaches readable plain text within 2 s and
  stays responsive; a 2 GB file with a claimed extension reaches the built-in
  viewer in under 1 s; a multi-megabyte single-line file never produces an
  unresponsive window.
- **SC-007**: In the upgrade scenario (seeded custom viewer entries), 100 %
  of pre-existing entries survive unchanged and user deletions are never
  resurrected across restarts.
- **SC-008**: The copy-fidelity matrix (CRLF/LF, tabs, trailing spaces,
  selections crossing wrapped lines, select-all) is byte-exact in 100 % of
  cases; line numbers never appear in the clipboard.
- **SC-009**: The encoding matrix (UTF-8 ± BOM, UTF-16 LE/BE ± BOM,
  Windows-1250, ISO-8859-2, CP852, invalid-sequence files) renders every
  character correctly or with the documented replacement behaviour, 100 %.
- **SC-010**: In a session that never opens the viewer, application start-up
  time, idle memory and background activity are indistinguishable from the
  current build (feature 065 measurement protocol).

## Assumptions

- **Rendering direction**: per the brief, the research
  (`native-and-hybrid.md`) and clarification 2026-08-26, the viewer renders
  in the shared WebView2 engine with an established web highlighting
  library; script execution is enabled only on this plugin's own controllers
  (permitted per-controller by the shared-engine contract §2.3) with the
  compensating lockdown of FR-030…033. The specific library, and whether
  progressive highlighting is used, are planning-phase decisions taken on
  the recorded measurements (`viewer-ux-webview2.md` §7).
- **Licensing** (decided in clarification 2026-08-26): all vendored
  highlighting/theme assets are GPLv2-compatible (MIT/BSD/ISC/Apache-class);
  every GPL-3.0-only grammar or theme file is excluded, and an automated
  licence audit of the vendored asset set is part of the build/test story
  (constitution: Technical Constraints). Third-party notices are recorded in
  `doc/third_party.txt`.
- **Claim policy** (updated in clarification 2026-08-26): `*.txt` and
  `*.log` are claimed by default and open as plain text (own removable
  family entry); `.md`/`.markdown` belong to the Markdown viewer;
  `.csv`/`.dbf`/`.tsv` to the database viewer; conflicts with the picture
  viewer's legacy masks (`.pyx`, `.st`, `.dtx`) are resolved by not claiming
  those three in v1 (revisit via the picture viewer's own upgrade path).
- **Default limits**: 1 MB highlighting / 20 000-character line / 20 MB
  viewer, aligned with the Markdown viewer's existing 20 MB gate and
  mainstream code-hosting behaviour; both size limits configurable.
- **Theme scope**: the product has no application-wide syntax palette; the
  scheme is plugin-local, with "follow the application theme" (Default/Dark)
  as the default mode. Reacting live to an application theme change while a
  window is open is not required (reopen adopts), matching the Markdown
  viewer's behaviour.
- **Parity boundaries**: hex mode, printing, byte-offset navigation and
  drag-out remain built-in-viewer capabilities and are out of scope;
  next/previous-file navigation within the panel is required in v1
  (clarification 2026-08-26, FR-041) — the long-path API question is
  verified early in planning.
- **Fallbacks**: the built-in text viewer is the universal fallback (engine
  missing, binary, oversized, declined); it remains fully functional and
  reachable via Alt+F3.
- **Repository integration**: the plugin follows the established new-plugin
  checklist (build policy entry, solution projects, translation module for
  all enabled languages via the two-stage refresh, changelog + version bump);
  the plugin ABI (interface 106) is unchanged.
- **Naming** (decided in clarification 2026-08-26): the module/directory
  name is **codeview** (`src/plugins/codeview/`, `codeview.spl`, translation
  module `codeview`); the plugin's user-visible display name is chosen in
  planning consistent with the other viewers' style.

# Feature Specification: Instant Markdown Viewer Display

**Feature Branch**: `065-mdview-instant-render`
**Created**: 2026-08-21
**Status**: Draft
**Input**: User description: "Proveď detailní analýzu plugin MDView pro prohlížečí Markdown souborů pomocí F3. Při prvním zobrazení Markdown souboru se na chvíli zobrazí prázdné pozadí a až potom se vykreslí obsah. Při robrazení dalšíh prolinkových markdown souborů je zobrazení již v podstatě okamžité. Předpokádám, že je to tím, že se používá nějaký webview pro zobrazení, který když není žádné okno aktivní není načtený v paměti a musí se inicializovat / spustit. Cílem je, aby každé zobrazení Markdown souboru bylo v podstatě okamžité."

## Problem Statement

Pressing F3 on a Markdown file opens the MDView viewer. The **first** time
this happens — after application start, or after every viewer window has been
closed for a while — the viewer window appears but shows only an empty
background for a noticeable moment before the document is drawn. Opening
further Markdown files while a viewer is already open (for example by
following links between documents) is essentially instant.

The user's hypothesis matches the code: the rendering surface is an embedded
web engine that is not resident in memory while no viewer window exists, so
the first open pays its start-up cost in full, visibly, inside the freshly
opened window. The goal — narrowed by clarification — is that this start-up
cost is paid **at most once per application session**, at the first actual
Markdown view: every later display, including after all viewer windows have
been closed, is essentially instant. No preparatory work runs before the
first view (no application-start or predictive background preparation).

The user asks for a detailed analysis of the current behavior first.
Preliminary code reconnaissance is recorded in `investigation-leads.md`; the
full analysis and the comparison of candidate keep-ready mechanisms (with
their idle-memory trade-offs) is the planning phase's deliverable, and the
mechanism is selected together with the user before implementation — the
same working mode as feature 064.

## Clarifications

### Session 2026-08-21

- Q: When may rendering-engine preparation start, so that even the very
  first Markdown view of a session is instant? → A: Only at first actual
  use — preparation begins with the first real Markdown view; the first view
  of a session may pay the engine start-up cost (at most once per session),
  and no background preparation happens before it.
- Q: How long should the engine stay ready ("warm") after the last viewer
  window closes? → A: For the whole session — released only at application
  exit; no idle timeout.
- Q: Should keeping the engine ready be user-configurable, or always on? →
  A: A plugin-configuration option, enabled by default; disabling it
  restores today's behavior (engine released when the last viewer window
  closes).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Every view after the first is instant (Priority: P1)

The user viewed a Markdown file at some point in the session. From then on,
every further Markdown view — following links between documents, opening
another file right away, or pressing F3 again minutes after closing the last
viewer window — shows the rendered document essentially immediately.

**Why this priority**: Closing the last viewer window currently returns the
plugin to the cold state, so the start-up penalty is paid again and again in
normal, one-document-at-a-time work. Making the cost a one-time-per-session
event is the core value of this feature (per clarification 2026-08-21).

**Independent Test**: Open a Markdown file (cold), close the viewer, wait
(e.g. a minute), open another Markdown file; measure the time to rendered
content and compare with today's back-to-back (linked-file) case.

**Acceptance Scenarios**:

1. **Given** a Markdown file was viewed earlier in the session and all viewer
   windows are now closed, **When** the user presses F3 on another Markdown
   file, **Then** the document appears as fast as in the back-to-back
   (linked-file) case — with no empty-background stage the user consciously
   perceives.
2. **Given** a viewer window is open, **When** the user follows a link to
   another Markdown document or opens a second file, **Then** it renders as
   fast as today's best case (no regression).

---

### User Story 2 - The cold start happens at most once per session (Priority: P2)

The user starts the application and presses F3 on a Markdown file for the
first time in the session. This first view may take the brief engine start-up
it takes today — but it is the only time in the whole session the wait
occurs, and it is no worse than the current build.

**Why this priority**: The clarification accepted the one-time cost in
exchange for zero background work before first use; what must hold is that
the cost never recurs and never grows.

**Independent Test**: In a fresh session, measure the first view (compare
with the current build), then close all viewers and open further documents at
intervals, verifying none of them pays the start-up cost again.

**Acceptance Scenarios**:

1. **Given** a freshly started application, **When** the user presses F3 on a
   Markdown file for the first time, **Then** the document renders no slower
   than in the current build, with the theme-colored background (never a
   white flash) during the wait.
2. **Given** the first view of the session has completed, **When** the user
   views any number of further documents at any later time in the session,
   **Then** none of them exhibits the cold-start wait again.

---

### User Story 3 - No cost to the rest of the application (Priority: P2)

A user who never views a Markdown file gets today's application, exactly:
start-up feels the same, panels behave the same, and no background cost of
any kind appears before the first Markdown view. After the first view, the
kept-ready state stays within a bounded, stable footprint.

**Why this priority**: Instant viewing must not be bought with a slower
application start or an unbounded resident footprint; that would trade a
visible defect for a diffuse one. Zero cost before first use is a decided
requirement (clarification 2026-08-21), not a trade-off left open.

**Independent Test**: Measure application start-up time and idle resource
footprint before and after the change, in a session that never opens a
Markdown file and in one that does.

**Acceptance Scenarios**:

1. **Given** a session in which no Markdown file is ever viewed, **When**
   start-up time and idle footprint are compared with the previous build,
   **Then** there is no difference at all (zero background work before first
   use).
2. **Given** the first Markdown view has happened, **When** the session
   continues with viewers closed, **Then** the kept-ready footprint stays
   bounded and stable (never exceeding one open viewer window).
3. **Given** the rendering engine is unavailable or its keep-ready state
   fails, **When** the user works with the application and views files,
   **Then** no error appears outside an actual viewing attempt, and viewing
   behaves no worse than today (including the existing text-viewer fallback).

---

### Edge Cases

- The user opens a second Markdown file while the first (cold) view is still
  initializing: the second open must attach cleanly to the initialization in
  progress — never pay the start-up cost twice, never deadlock or race.
- The web rendering engine is not installed / not available on the system:
  the existing fallback to the internal text viewer is preserved unchanged;
  keep-ready work must fail silently.
- The rendering engine's kept-ready background process crashes or is killed
  while no viewer window is open: the next F3 must still work (readiness
  recovers or degrades to today's behavior, never to a broken open).
- Several viewer windows opened and closed in quick succession: no leaks, no
  degradation of subsequent opens.
- Whatever brief preparation remains visible, the viewer window shows the
  document color scheme's background (dark scheme included) — never a white
  flash; this already works today and must not regress.
- System under memory pressure: any kept-ready engine has a bounded, stable
  footprint; it must not grow over time.
- The keep-ready option is toggled mid-session: disabling releases the
  kept-ready engine once no viewer window is open; enabling takes effect
  from the next view — no restart needed, no stuck intermediate state.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: No preparatory or background work related to Markdown viewing
  runs before the first actual Markdown view of the session: application
  start-up, idle footprint, and panel behavior are identical to the current
  build until the user first views a Markdown file (decided in clarification
  2026-08-21).
- **FR-002**: The engine start-up cost MUST be paid at most once per
  application session: every display after the first — including after all
  viewer windows have been closed — MUST be essentially instant, at least as
  fast as today's best case (opening a document while another viewer window
  is already open).
- **FR-003**: The first display of a session MUST be no slower than in the
  current build, and ordinary panel work MUST NOT be perceptibly affected at
  any point.
- **FR-004**: The kept-ready state established by the first view MUST last
  for the rest of the application session — released only at application
  exit, with no idle timeout — and MUST have a bounded, stable, documented
  idle footprint (decided in clarification 2026-08-21).
- **FR-005**: Failure of any preparatory/readiness work MUST be silent and
  harmless: no error dialogs outside an actual viewing attempt, and a
  subsequent F3 behaves no worse than today, including the existing
  engine-unavailable fallback to the text viewer.
- **FR-006**: All existing viewer behavior MUST remain unchanged: the
  security lockdown of the rendering surface, color schemes and dark mode,
  zoom, find, link handling, source view, encoding detection, and the size
  gate.
- **FR-007**: Any residual moment before content appears MUST show the
  document theme's background color (no white flash, no visual artifacts) —
  preserving today's behavior.
- **FR-008**: Keeping the engine ready MUST be a plugin-configuration
  option, enabled by default. When disabled, the plugin behaves as the
  current build (the engine is released when the last viewer window closes).
  The setting persists with the plugin's other settings, and a change takes
  effect without restarting the application: disabling releases the
  kept-ready engine as soon as no viewer window is open; enabling applies
  from the next view.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On the development machine, any view after the first of the
  session — back-to-back, or after all viewer windows were closed — shows
  rendered content of a typical document (≤ 100 KB) within the warm baseline:
  within 2× of today's back-to-back (linked-file) time, measured with
  instrumented timestamps.
- **SC-002**: Instrumentation confirms the engine start-up occurs at most
  once per session: across a session with ≥ 5 views separated by
  close-all-viewers gaps, no view after the first exhibits the cold-start
  wait.
- **SC-003**: For sessions that never open a Markdown file, application
  start-up time and idle footprint are unchanged from the current build
  (within measurement noise) — zero cost before first use.
- **SC-004**: After the first view, the idle resource footprint attributable
  to viewer readiness is bounded (never exceeding the footprint of one open
  viewer window) and stable over an idle period.
- **SC-005**: With the rendering engine unavailable, all scenarios behave
  exactly as the current build (fallback to text viewer, no new messages).
- **SC-006**: The first view of a session renders no slower than in the
  current build (within measurement noise), measured the same way.
- **SC-007**: With the keep-ready option disabled, timing and idle footprint
  match the current build; after re-enabling, the instant behavior resumes
  (following the one-time engine start of the next view).

## Assumptions

- **Analysis-first, joint solution selection**: the planning phase delivers
  the detailed analysis the user asked for. The *when to prepare* question is
  decided (only at first actual use — clarification 2026-08-21), and so is
  the lifetime (the whole session); planning presents candidate **keep-ready
  mechanisms** (what exactly is kept alive after the first view and where it
  lives) with their trade-offs, and the user picks before implementation
  tasks start — the working mode established in feature 064. Preliminary
  code pointers live in `investigation-leads.md`.
- Scope is the MDView viewer (F3 on `*.md`/`*.markdown`); no other viewer,
  plugin, or panel behavior changes.
- Target environment is Windows 11 with the standard OS web-engine component
  present; the missing-engine fallback path is out of scope except for "must
  not regress".
- "Essentially instant" (applies to every view after the first of a session)
  means the empty stage is not consciously perceived (on the order of
  200–300 ms or less); the concrete verification numbers are in Success
  Criteria and are measured on the development machine with instrumented
  timestamps plus manual observation — no automated UI test infrastructure
  exists.
- A small, bounded idle cost (memory of the kept-ready engine) after the
  first view is the accepted currency for instant subsequent viewing; the
  engine stays ready for the whole session, with a default-on
  plugin-configuration option to turn keep-ready off (clarification
  2026-08-21).

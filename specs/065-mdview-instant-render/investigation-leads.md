# Investigation Leads — 065-mdview-instant-render

Preliminary code reconnaissance done during specification (2026-08-21).
These are pointers for the planning-phase analysis, not conclusions of it.

## How a Markdown view opens today

1. F3 → `ViewFile` (main thread, `src/plugins/mdview/viewer.cpp:293`):
   checks `CMdWebHost::RuntimeAvailable()` (engine-missing fallback), then
   spawns the **thread-per-window** viewer (demoview-derived model).
2. `CViewerWindow` `WM_CREATE` (`viewer.cpp:827-859`): builds the menu, then
   `Web->Create(HWindow, UserDataFolder(), cb)` — **each window creates its
   own WebView2 environment + controller**, asynchronously, on the window's
   own thread.
3. `CMdWebHost::Create` (`webview.cpp:406`):
   `CreateCoreWebView2EnvironmentWithOptions` → callback →
   `CreateCoreWebView2Controller` → callback → `OnReady` → the viewer
   renders (`RenderDocument` → `Navigate`) → content paints after
   `NavigationCompleted`.
4. Until then the window shows the theme `docBg` brush (`viewer.h` `BgBrush`
   + `CMdWebHost::SetBackgroundColor`) — so the "blank stage" is
   theme-colored, not white; it is simply *long* on a cold start.

## Why the first open is slow and the rest are instant

- All windows pass the **same user data folder**
  (`UserDataFolder()`, `viewer.cpp:82` → `%LOCALAPPDATA%\...`). WebView2
  environments sharing a user data folder share one browser-process tree
  (`msedgewebview2.exe` broker + renderer + GPU…).
- The **first** environment/controller creation must spawn that process tree
  — this is the cold start the user sees (typically several hundred ms to
  seconds).
- While at least one controller lives, later `Create` calls attach to the
  running tree → near-instant. This exactly matches the observed
  "linked documents open instantly".
- When the **last** controller/environment is released (last viewer window
  closed), the runtime shuts the process tree down (after a short grace
  period) → the next open is cold again. Matches the user's hypothesis.

## Constraints to respect in planning

- **WebView2 threading**: environment/controller objects are bound to the
  thread that created them (STA, message loop required). The viewer uses
  thread-per-window, so a pre-created controller cannot simply be handed to
  the next window's thread. However, *any* live controller (e.g. a hidden
  keeper on a dedicated thread) keeps the shared browser process warm, which
  is what makes per-window creation fast.
- Plugin lifecycle: mdview is a viewer plugin; "application start" hooks are
  really "plugin load / first plugin entry" unless the plugin is made
  load-on-start (Plugin Manager has such a flag; policy implications).
- The engine-unavailable fallback (`RuntimeAvailable`, text viewer) and the
  security lockdown (feature 021 contract,
  `specs/021-mdview-html-renderer/`) must survive unchanged.
- `--disable-background-networking` etc. are per-environment browser
  arguments; a keeper environment must use identical arguments (same user
  data folder + different args is a WebView2 error path).

## Candidate approach space (to be analyzed and priced in plan)

> **Narrowed by clarification 2026-08-21** (see spec `## Clarifications`):
> preparation starts only at the first actual view, the kept-ready state
> lasts the whole session (no timeout), and keep-ready is a default-on
> plugin-configuration option. That rules out **B** (app-start pre-warm) and
> **C** (predictive trigger); the plan analyzes the *mechanism* behind
> **A/D** (what exactly stays alive after the first view, on which thread it
> lives, how the config toggle releases/re-arms it).

- **A. Keeper controller for the session**: on first viewer open (or plugin
  load), create a hidden environment+controller on a dedicated keeper thread
  and keep it for the session → the browser tree never shuts down; every
  later open is warm. Cost: resident browser processes (~tens of MB) after
  first use; zero before.
- **B. Pre-warm at application start**: same keeper, created eagerly (plugin
  loaded at start). Makes even the *first* open of the session warm; costs
  memory in sessions that never view Markdown + a plugin-load-at-start
  policy decision.
- **C. Early-trigger warm-up**: create the keeper on a cheap predictive
  trigger (e.g. `CanViewFile` being asked about a `.md`, or panel cursor on
  a Markdown file) so the tree is spawning while the user decides to press
  F3. Latency hidden in think-time; zero cost for non-users.
- **D. Keep-alive after last close**: don't tear the keeper down when the
  last window closes (optionally with a timeout). Fixes User Story 2 alone;
  combines with A/C for Story 1.
- **E. Perceived-latency masking**: window-level tricks (e.g. showing the
  decoded source instantly, swapping in the rendered document) — likely
  poor value next to A–D; note for completeness.

## Measurement hooks

- Instrument: timestamp at `ViewFile` entry, `OnReady`, and
  `NavigationCompleted` (add temporary `TRACE`/log lines) → cold vs warm
  numbers on the dev machine, before and after.
- Observe process tree: `msedgewebview2.exe` instances appearing on first
  open and disappearing after last close (Task Manager / `tasklist`).

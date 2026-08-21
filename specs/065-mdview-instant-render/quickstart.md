# Quickstart — Validating Instant Markdown Viewer Display (065)

Manual validation guide (no automated UI infrastructure exists). References:
success criteria in [spec.md](spec.md), keeper invariants in
[contracts/keeper.md](contracts/keeper.md).

## Prerequisites

- Debug x64 build: `build.cmd full` (Debug traces carry the R8 timestamps);
  repeat key scenarios on Release for real-world timing.
- A test folder with 2+ Markdown files that link to each other
  (e.g. `a.md` containing `[b](b.md)`).
- A way to watch WebView2 processes:
  `tasklist /fi "imagename eq msedgewebview2.exe"` (or Task Manager,
  grouped under Tandem Commander).

## Scenario 1 — Cold first view, then instant forever (SC-001, SC-002, SC-006)

1. Start `tandemcommander.exe` fresh. Confirm **no** `msedgewebview2.exe`
   belonging to it exists yet.
2. F3 on `a.md` → the one-time cold start (no slower than the current
   build; theme-colored background, never white). Note the
   ViewFile→NavigationCompleted trace delta (Debug).
   **Also expected** (R9): `%LOCALAPPDATA%\Tandem Commander\WebView2` now
   exists, and the old `...\Tandem Commander\mdview.WebView2` folder (if
   present from a previous build) is gone.
3. Close the viewer. Wait ≥ 60 s. **Expected**: the `msedgewebview2.exe`
   tree is still running (keeper armed).
4. F3 on `b.md` → **instant** (within 2× of the back-to-back time; no
   perceptible blank stage). Repeat with gaps ≥ 5 views: none after the
   first shows the cold wait.

## Scenario 2 — Zero cost before first use (SC-003)

1. Start fresh; work in panels, view text/images, do **not** view Markdown.
2. **Expected**: no `msedgewebview2.exe` under Tandem Commander at any
   point; start-up time and idle footprint match the previous build.

## Scenario 3 — Config toggle (FR-008, SC-007)

1. Plugins Manager → Markdown Viewer → Configure → uncheck "Keep the
   rendering engine ready…" → OK, with no viewer window open.
   **Expected**: the WebView2 tree exits within ~1 minute (grace period).
2. F3 on a Markdown file → cold start again (current-build behavior);
   close; F3 again → cold again (keeper stays off).
3. Re-enable the option → next view arms the keeper: view → close → view is
   instant again.
4. Restart the app: the setting persisted (registry `KeepReady`).

## Scenario 4 — Crash recovery (spec edge case, R7)

1. With the keeper armed and all viewers closed, kill the tree:
   `taskkill /f /im msedgewebview2.exe`.
2. F3 on a Markdown file → **works** (one cold start, fresh tree); close;
   F3 again → instant (keeper re-armed).

## Scenario 5 — Rapid double open during cold init (spec edge case)

1. Keeper off or fresh session. F3 on `a.md` and immediately F3 on `b.md`
   (or follow the link the moment content appears).
2. **Expected**: both windows render; no crash, no error, no second
   full-length cold wait for `b.md`.

## Scenario 6 — Bounded footprint (SC-004)

1. Keeper armed, all viewers closed. Record the WebView2 tree's total
   memory; compare with one open viewer window showing a document.
2. **Expected**: kept-ready total ≤ one-open-viewer total; stable after
   10 min idle (no growth).

## Scenario 7 — No regressions (FR-006, SC-005)

- Viewer smoke: schemes + F9/Shift+F9, follow-system theme, zoom
  (Ctrl+wheel, Ctrl+0), find next/prev, link kinds (#anchor, local .md,
  http), View Source (Ctrl+U), remote-image consent, dark menus.
- `tests/mdview_htmlgen_test/` still passes (29 assertions).
- Plugin unload (Plugins Manager → unload with keeper armed, no viewers):
  unloads cleanly, tree exits; reload + view works.
- (If testable) machine without WebView2 runtime: text-viewer fallback as
  today, no new messages.

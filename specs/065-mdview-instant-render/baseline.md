# Baseline & Measurements — 065-mdview-instant-render

**Status**: instrumentation and builds are in place; the timing numbers
require a human at the keyboard (GUI F3), as with every prior mdview
feature. Fill the tables below when running the quickstart scenarios.

## How to measure

- Build: Debug x64 (`build.cmd`) — the R8 `TRACE_I` lines require a Debug
  build plus the Trace Server; Release runs are stopwatch/manual.
- Trace points (all in ms via `GetTickCount64`):
  - `mdview: ViewFile (t=...)` — F3 accepted (main thread)
  - `mdview: controller ready (t=...)` — engine attached for that window
  - `mdview: navigation completed (t=...)` — content painted
  - `mdview keeper: arming/armed/disarmed/...` — keeper lifecycle
- Cold vs warm: **the pre-change baseline is measured with the keep-ready
  option OFF** (Plugins Manager → Markdown Viewer → Configure) — with the
  option off the plugin reproduces the pre-065 lifecycle exactly (SC-007),
  so the baseline is measurable on this build without checking out an old
  revision. The new behavior is the same runs with the option ON (default).
- Process observation:
  `tasklist /fi "imagename eq msedgewebview2.exe"`.

## Table 1 — cold/warm times (typical document ≤ 100 KB)

| Scenario | keep-ready OFF (baseline) | keep-ready ON | Target |
|---|---|---|---|
| First view of session (ViewFile→NavCompleted) | ___ ms | ___ ms | ON no slower than OFF (SC-006) |
| Back-to-back second view | ___ ms | ___ ms | no regression (FR-002) |
| View after close-all + 60 s | ___ ms (≈ first view) | ___ ms | ≈ back-to-back; ≤ 2× warm (SC-001) |
| ≥ 5 views with close-all gaps | cold every time | cold only 1st (SC-002) | trace shows one arming |

## Table 2 — footprint (Task Manager, WebView2 processes under Tandem Commander)

| State | Memory | Target |
|---|---|---|
| Session, no Markdown viewed | ___ (expected: no WebView2 processes) | 0 (SC-003) |
| Keeper armed, all viewers closed | ___ MB | ≤ one open viewer (SC-004) |
| Same, after 10 min idle | ___ MB | stable (SC-004) |
| One viewer open (reference) | ___ MB | — |

## Checks done without the GUI (this session, 2026-08-21)

- Debug x64 build clean after all 065 changes (`BUILD SUCCEEDED`).
- Stage-1 template export: `[DIALOG 310]` present in the mdview template.
- Stage-2 merge: 8 enabled languages × 4 new rows → `english_fallback`
  (provenance in `mdview.origin`); byte-exact round-trip verified for all
  290 committed `.slt` files.
- Stage-3 `build_langs.cmd --module mdview`: 8 language modules built from
  the new structure, version check OK (positional import accepted).

## Pending user (GUI)

Quickstart scenarios 1–7 (`quickstart.md`): timing tables above, toggle
semantics, crash recovery (`taskkill /f /im msedgewebview2.exe`), rapid
double open, footprint, and the viewer regression smoke.

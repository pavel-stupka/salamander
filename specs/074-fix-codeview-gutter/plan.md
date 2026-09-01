# Implementation Plan: Fixed-width line-number gutter in the code viewer

**Branch**: `074-fix-codeview-gutter` | **Date**: 2026-09-01 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/074-fix-codeview-gutter/spec.md`

## Summary

The code viewer sizes each line's number box to that line's own number, so the
source text starts one character further right on line 10 than on line 9, and
again at 100 and at 1000 — the ragged left edge in `temp/radky.png`. The fix
gives the number column one width for the whole document, derived from the
document's total line count, with the numbers right-aligned inside it.

The mechanism is already half-built and disconnected: `layout()` computes the
digit count and publishes it as the custom property `--gutter-min`, and **no
CSS rule reads it** (research R0). Reconnecting it is one declaration — but
only if the box model is handled: under the page-wide `border-box`, a
`min-width` in `ch` is smaller than the natural content-plus-padding width for
every realistic digit count and would be silently inert (research R2, the crux
of this feature). `.gut` therefore gets `box-sizing: content-box`, so the
constraint covers the digits and the 20 px of padding sits outside it — and
`width` rather than `min-width`, because `1ch` is 0.0117 px narrower than a
rendered digit, so `min-width` would not bind on the widest rows (research R2b,
found by measurement during implementation).

Two small supporting changes: the digit expression becomes a named function so
the existing headless harness can lift and test it, and the dead
`g.style.minWidth = ''` disappears from the per-row hot path. One further item
— the sticky gutter has a transparent background, so code is visible *through*
the numbers when scrolling right — was gated on reproducing it first; it
**reproduced**, so the gutter is made opaque (research R6).

Scope: two web assets, one test harness, no C++.

## Technical Context

**Language/Version**: JavaScript (ES2022 modules) + CSS, running in the
WebView2/Chromium page of the codeview plugin. No C++ change expected.
**Primary Dependencies**: WebView2 (vendored SDK 1.0.4078.44, unchanged);
shiki (vendored, untouched). No new dependency.
**Storage**: none — no setting, no registry value, no persisted state.
**Testing**: `src\plugins\codeview\test\run_tests.cmd` (Node page harness with
source-lifting + shape assertions; Python data harness) plus the manual GUI
scenarios in [quickstart.md](quickstart.md).
**Target Platform**: Windows 11+, x64.
**Project Type**: desktop application plugin with a web-rendered viewer page.
**Performance Goals**: no regression in `rendered.firstPaintMs` or scroll
smoothness; the change removes one CSSOM write per row per scroll frame.
**Constraints**: CSP forbids inline `style` attributes
(070 `contracts/rendering-lockdown.md` §2); the virtual list rebuilds every
visible row on every scroll frame, so per-row work must stay minimal; copied
text must remain byte-identical (070 FR-021).
**Scale/Scope**: documents from 0 lines to millions (7+ digit numbers); two
files changed in `src/plugins/codeview/web/`, one test file extended.

## Constitution Check

*GATE: checked before Phase 0 and re-checked after Phase 1 design.*

| Principle | Assessment | Verdict |
|---|---|---|
| I. Build Reproducibility | No new build step, no new tool, no generated file. Assets are embedded from the existing `RCDATA` references, so `assets_table.inc` and `build_web.py` are untouched (research R7). | PASS |
| II. Backward Compatibility | Display-only. No configuration, no registry, no file format, no host↔page message, no plugin ABI (`LAST_VERSION_OF_SALAMANDER` = 106 untouched). Copy output stays byte-identical — FR-010 makes that an explicit constraint, and the quickstart checks it. The only behaviour that changes is the one the user reported as wrong. | PASS |
| III. Incremental Modernization | Three files, each change small and independently revertible. The one structural edit (extracting `gutterDigitsFor`) is required by the test strategy, not taste (research R4); no adjacent code is refactored. | PASS |
| IV. Windows Platform Commitment | Unchanged. WebView2 is the plugin's existing, architecture-approved rendering path (`architecture/11-webview2-integration.md`). | PASS |
| V. Plugin Architecture Preservation | No plugin interface touched. The page-internal contract this feature relies on is written down in [contracts/gutter-geometry.md](contracts/gutter-geometry.md) before the change, alongside feature 070's contracts. | PASS |
| VI. UI Consistency | No dialog, no WinAPI control, no process-wide visual setting. The change makes the viewer match the universal editor convention it currently breaks. | PASS |
| Release Documentation | CHANGELOG entry + version/build bump are a **ship-gate** task belonging to the release that ships this, per the feature 071 pattern — not part of this feature's completion. Stated in Ship gate below. | PASS (planned) |

**Post-Phase-1 re-check**: no violation introduced. The design adds one CSS
declaration pair, one named function, one deleted line, one contract document
and three test assertions. Complexity Tracking is therefore empty and the
section is omitted.

## Project Structure

### Documentation (this feature)

```text
specs/074-fix-codeview-gutter/
├── plan.md                        # this file
├── research.md                    # Phase 0 — R0..R8, decisions D1..D8
├── data-model.md                  # Phase 1 — derived view values, no persisted state
├── quickstart.md                  # Phase 1 — build, harness, 12 validation scenarios
├── contracts/
│   └── gutter-geometry.md         # Phase 1 — the producer/consumer contract
├── checklists/
│   └── requirements.md            # spec quality checklist (all green)
└── tasks.md                       # Phase 2 — /speckit-tasks, NOT created here
```

### Source code (repository root)

```text
src/plugins/codeview/
├── web/
│   ├── viewer.css                 # .gut rule: min-width: var(--gutter-min) + content-box
│   └── viewer.js                  # gutterDigitsFor(); layout() ordering; drop dead minWidth
│                                  #   (+ conditional: --gutter-bg in applyThemeColors)
└── test/
    └── harness/test_page.mjs      # 2 shape assertions + gutterDigitsFor unit checks
```

**Structure Decision**: the change lives entirely in the codeview plugin's
embedded web page (`src/plugins/codeview/web/`), which is where the viewer's
rendering already lives; its regression tests live in the plugin's own harness
(`src/plugins/codeview/test/`), run by `run_tests.cmd`. No new directory, no new
file in the product tree. The C++ side of the plugin (`viewer.cpp`, `intake.cpp`,
`webglue.cpp`) is not involved: it serves the document text and view state, and
neither carries any notion of gutter width.

## Design

### Change 1 — `viewer.css`, the `.gut` rule *(FR-001, FR-002, FR-003, FR-006, FR-009)*

Add to the existing rule at `viewer.css:99`:

- `width: var(--gutter-min)` — consume the property the page already publishes;
  `width` and not `min-width`, per research R2b;
- `box-sizing: content-box` — with a comment explaining that it is deliberate
  against the page-wide `border-box`, and *why* border-box would make the
  constraint inert (research R2). Without the comment this looks like a stray
  exception and will be "tidied" away.

`text-align: right` (already present) and the padding (already present) are not
touched. Right alignment then works because the box is finally wider than its
content — no new declaration for FR-003.

Zoom (FR-006) needs no code: `ch` is resolved at the use site against `.gut`'s
own font, which is the code font at the current size.

### Change 2 — `viewer.js`, `gutterDigitsFor` + `layout()` *(FR-002, FR-004, FR-005)*

- Extract `String(Math.max(1, lines.length)).length` into
  `function gutterDigitsFor(count)`, called from `layout()`. Required so the
  harness's `lift()` can extract and test it (research R4).
- Publish `--gutter-min` **before** `resetGeometry()` and `render(true)`
  (research R5) — the current order is benign but leaves a window where wrap
  geometry is reset against the previous document's width.
- Delete `g.style.minWidth = ''` from `makeLine`: it clears an inline style
  nothing sets, and it is a per-row write in the scroll hot path.

FR-004 (no shift while scrolling) and FR-005 (correct on the first frame) need
no new code once the width is a document-level property: `layout()` already
runs on load and on every `setView`, and the value is derived from
`lines.length`, never from the rendered window.

### Change 3 — `test_page.mjs`, three guards *(contract §S7)*

- Shape assertion over `viewer.css`: the `.gut` rule declares all three of
  `min-width: var(--gutter-min)`, `box-sizing: content-box`, `text-align: right`.
- Shape assertion over `viewer.js`: `layout()` sets `--gutter-min`, and
  `makeLine` contains no `style.minWidth` write.
- Lift `gutterDigitsFor` and check it against the §S3 table
  (0→1, 9→1, 10→2, 99→2, 100→3, 999→3, 1000→4, 1000000→7).

The shape assertions are the point: the original defect was a computed value
with no consumer, which no behavioural test could have caught. Each new check
must be seen to **fail** on the pre-change file before it is trusted.

### Change 4 — opaque gutter background *(FR-011)* — condition met, applied

`--gutter-bg` is declared `transparent` and never assigned by
`applyThemeColors`, while `.gut` is `position: sticky`. Code should therefore
be visible through the numbers when a long line is scrolled right.

**Reproduced** (2026-09-01): rendering the shipped stylesheet scrolled right
shows the numbers drawn *over* the code, both visible and smeared together. The
fix is two lines — `--gutter-bg` defaults to `var(--bg)` in the stylesheet, and
`applyThemeColors` sets it from `editorGutter.background` with the same
`var(--bg)` fallback VS Code uses when a theme defines no gutter colour.

This is in scope because FR-011 requires the pinned column to remain a column
during horizontal scrolling. It is the one item a reviewer could reasonably
call adjacent, so it is isolated in its own change and can be dropped without
touching anything else.

### Explicitly not changed

- `revealColumn` (`viewer.js:707`) already measures the gutter before scrolling
  a find match into view, and reads the right element; a constant width simply
  makes that measurement stable.
- `geometryKey` gains no field: it contains `docGen`, and the digit count is a
  function of the document (research R5).
- `#sizer`, the sticky/scroll model, the wrap-height measurement, `user-select`,
  `body.nogutter`, and everything on the C++ side.
- No minimum digit count is introduced (spec Assumptions): a short file keeps
  its code as far left as the padding allows.

## Risks and how they are handled

| Risk | Handling |
|---|---|
| The `min-width` is added but stays inert (the original failure mode) | `box-sizing: content-box` with the arithmetic recorded in research R2 and the reason in a code comment; quickstart scenario 1 is a direct visual check; contract §S4 states the invariant. |
| The producer/consumer pair is disconnected again by a later edit | Two shape assertions in `test_page.mjs`, each proven to fail on the pre-change file. |
| Wrap-mode heights measured against the old gutter width | Property published before `resetGeometry()`; `geometryKey` already keyed on the document; heights are corrected by `measureRendered` after render regardless. |
| A CSS edit appears not to take effect after an incremental build | Called out in quickstart §1: `RCDATA` dependency tracking is unreliable — rebuild before debugging the stylesheet. |
| Scope creep into the gutter's colours | Change 4 is conditional, isolated, and droppable. |

## Ship gate (release only — not part of this feature)

Per the constitution's Release Documentation section and the feature 071
pattern, the release that ships this fix must, **in one change**: add a
`CHANGELOG.md` *Fixed* entry in the user's terms ("in the Code Viewer, the line
number column now has a fixed width with the numbers right-aligned, so the code
no longer shifts sideways at line 10, 100 and 1000"), and bump
`VERSINFO_SALAMANDER_MINORB` + `VERSINFO_BUILDNUMBER` in
`src/plugins/shared/spl_vers.h`, `MyAppVersion` in `setup/tandemcommander.iss`
and the version line in `CLAUDE.md`. `LAST_VERSION_OF_SALAMANDER` (106) is not
touched — the plugin API does not change.

## Phase status

- [x] Phase 0 — research complete, no NEEDS CLARIFICATION remaining
- [x] Phase 1 — data model, contract and quickstart written; Constitution
      Check re-run clean
- [ ] Phase 2 — `tasks.md` (`/speckit-tasks`)

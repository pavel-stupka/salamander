# Phase 0 Research: Fixed-width line-number gutter in the code viewer

**Feature**: 074-fix-codeview-gutter | **Date**: 2026-09-01

All questions below were resolved by reading the shipped sources; no
NEEDS CLARIFICATION remains.

## R0 — What the code actually does today

The viewer page builds one `<div class="ln">` per line, a flex row containing
`<span class="gut">` (the number) and `<span class="tx">` (the code).

`src/plugins/codeview/web/viewer.css:99-110`

```css
.gut {
  flex: 0 0 auto;
  text-align: right;
  padding: 0 12px 0 8px;
  ...
}
```

`src/plugins/codeview/web/viewer.js:188-204` (`makeLine`)

```js
const g = document.createElement('span')
g.className = 'gut'
g.textContent = String(i + 1)
g.style.minWidth = ''            // <-- clears an inline style nobody sets
```

`src/plugins/codeview/web/viewer.js:349-355` (`layout`)

```js
gutterDigits = String(Math.max(1, lines.length)).length
document.documentElement.style.setProperty('--gutter-min', gutterDigits + 'ch')
```

**Finding**: the intended mechanism is already half-built. The digit count is
computed and published as the custom property `--gutter-min` on every layout —
and **no CSS rule anywhere consumes it** (`grep -n "gutter-min" web/*` returns
exactly the one line above). `.gut` therefore has `width: auto` with
`flex-basis: auto`, so each row's number box is sized to its own content, and
`text-align: right` has nothing to align against. Text starts one character
further right on line 10 than on line 9, exactly as `temp/radky.png` shows.

The dead `g.style.minWidth = ''` in the hot row-building path is the fossil of
an abandoned per-row attempt, and is itself evidence for R2 below.

## R1 — Where the width should be enforced: CSS rule vs. per-row inline style

**Decision**: one CSS rule on `.gut`, fed by the existing `--gutter-min`
custom property. No per-row JavaScript.

**Rationale**:

- The virtual list rebuilds every visible row on every scroll frame
  (`render` → `makeLine`). A per-row `style.minWidth` write is O(rows) of
  extra CSSOM work per frame for a value that is constant across the document.
- `ch` inside a custom property is substituted as a token stream and resolved
  at the *use site*, against `.gut`'s own font. `.gut` inherits `--font-size`
  from `body`, the same size the code uses, so the width tracks zoom with no
  JavaScript at all (FR-006 falls out for free).
- The property is already recomputed by `layout()`, which runs on document
  load (`init` → `measure()` → `layout()`, `viewer.js:402-403`) and on every
  `setView` — so FR-005 (correct on the first frame) and the re-size on a new
  document are already wired.

**Alternatives considered**:

| Alternative | Rejected because |
|---|---|
| Per-row inline `minWidth` in `makeLine` | Costs a style write per row per scroll frame; duplicates in JS a value CSS can derive; the abandoned remnant of this approach is the dead line quoted above. |
| Zero-pad the numbers (`007`) | Changes what is displayed. In a *code* viewer a leading-zero number reads as an octal literal; and padding characters would sit in the DOM, which FR-010 forbids. |
| Move the gutter into its own column element outside the rows | A large restructuring of the virtual list: breaks the per-row wrap-height measurement (`measureRendered`), the sticky/scroll model and the "gutter is never selected" guarantee. Disproportionate to a width bug. |

## R2 — Why `min-width` alone does not work: the box model (the crux)

`viewer.css:32` sets `* { box-sizing: border-box; }` globally, and `.gut`
carries `padding: 0 12px 0 8px` — 20 px of padding.

Under **border-box**, `min-width: Nch` constrains the *border* box, which
already includes those 20 px. Sizing a `flex: 0 0 auto` item takes its base
size from `width: auto` → max-content → *digits + 20 px*. At the default
13 px Consolas, `1ch ≈ 7.15 px`:

| document | digits | `min-width` = N ch | base size (1-digit row) | which wins |
|---|---|---|---|---|
| 21 lines | 2 | 14.3 px | 7.15 + 20 = 27.2 px | base — min-width **inert** |
| 5 000 lines | 4 | 28.6 px | 27.2 px | base, near-tie — still ragged |
| 500 000 lines | 6 | 42.9 px | 27.2 px | min-width, by accident |

So a naive `min-width: var(--gutter-min)` would appear to do nothing for every
realistic file — which is very likely why the original attempt was abandoned
with the property left dangling. This is the single fact that has to be right.

**Decision**: give `.gut` `box-sizing: content-box`, so the constraint applies
to the *content* box at exactly N digit widths and the padding is added outside
it. Every row is then `Nch + 20 px` wide, whatever number it holds.

**Measured, not assumed** (implementation, 2026-09-01): the table above was
confirmed in Edge/Chromium against the shipped stylesheet — `min-width`
computed to `auto` (nothing consumed `--gutter-min`), `box-sizing` was
`border-box`, and the text column stood at four distinct offsets
27.16 / 34.30 / 41.45 / 48.59 px, one digit width apart. Adding a bare
`min-width: var(--gutter-min)` under border-box moved the 1-digit rows from
27.16 to 28.58 px and left the other three offsets untouched — inert, exactly
as predicted.

## R2b — `width`, not `min-width`: the `ch` unit is not the digit advance

Found by measuring during implementation, and it changes D2.

With `box-sizing: content-box` and `min-width: 4ch`, the 1200-line probe still
produced **two** text-column offsets, 0.0156 px apart. The cause:

| quantity | value at 13 px Consolas |
|---|---|
| `1ch` (the engine's advance for the "0" glyph) | 7.14453 px |
| a rendered digit's advance in shaped text | 7.15625 px |
| difference, per digit | 0.01172 px |

`ch` is *not* the same number the text layout uses. So for the widest rows —
the ones whose number actually has N digits — the natural content is fractionally
wider than `Nch`, `min-width` does not bind, and those rows keep their own width.

**Decision**: use `width: var(--gutter-min)` rather than `min-width`. Every row
then gets the identical box, and the longest number overflows its content box by
1/64 px into the 8 px of left padding, where nothing can see it. Measured
result: a single text-column offset across all 1200 rows (spread 0.000 px), and
the number glyph right edges within 0.016 px.

**No `var()` fallback** is given: if the property were ever missing, the
declaration is invalid at computed-value time and `width` falls back to `auto`
— the pre-074 behaviour — rather than to a column too narrow for the numbers.

**Alternative rejected**: measuring the real digit advance in JavaScript and
publishing pixels. Exact, but it reintroduces a cached font metric to
invalidate on every zoom, font and DPI change — the state `data-model.md`
deliberately refuses.

**Alternative considered** (for R2): keep border-box and write
`min-width: calc(var(--gutter-min) + var(--gutter-pad-l) + var(--gutter-pad-r))`,
hoisting the two padding values into custom properties so the calc cannot
drift from the `padding` declaration. Correct, but three declarations and a
cross-reference where one keyword does the job; `content-box` cannot fall out
of sync with a later padding edit. Recorded here because it is the fallback if
`content-box` ever proves awkward.

## R3 — Right alignment

**Decision**: no change. `text-align: right` is already on `.gut`
(`viewer.css:101`); it is inert only because the box hugs its content. Once
R2 widens the box, the numbers align flush right — units under units — with no
further declaration. `.gut` is a flex item, hence blockified, so `text-align`
applies to its inline content as expected.

## R4 — The digit count, and making it testable

`String(Math.max(1, lines.length)).length` is correct as it stands, including
the empty document (0 lines → 1 digit, and no rows are drawn anyway).

**Decision**: lift the expression into a named one-line function
`gutterDigitsFor(count)` and call it from `layout()`. The reason is not style:
`test/harness/test_page.mjs` tests the page's DOM-free logic by *extracting a
named function body from the shipped file* (`lift(name)`, harness lines 41-52),
so only a named function can be regression-tested. Free-standing expressions
inside `layout()` cannot be.

This is the only structural edit to `viewer.js` beyond deleting the dead line,
and it is required by the test strategy rather than by taste (constitution
principle III — modernize only the code being touched).

## R5 — Ordering inside `layout()`

`layout()` today calls `resetGeometry()` *before* it publishes the new
`--gutter-min`. In wrap mode the gutter width changes the text width and hence
the measured line heights, so the order is worth getting right.

**Decision**: publish the property first, then `resetGeometry()`, then
`render(true)`.

The current order is benign — wrap heights start as estimates and are corrected
by `measureRendered` after the first render — but the corrected order removes a
window in which geometry is reset against the previous document's gutter width,
and costs nothing. `geometryKey` already contains `docGen`, and the digit count
is a function of the document, so the cached-heights invalidation (`viewer.js:483`)
needs no new field.

## R6 — Horizontal scrolling: the gutter is sticky **and transparent**

`.gut` is `position: sticky; left: 0; z-index: 1` with
`background: var(--gutter-bg)`, and `--gutter-bg` is declared `transparent`
(`viewer.css:14`) and **never assigned** by `applyThemeColors` — the theme
loop sets `--bg`, `--fg`, `--gutter-fg`, `--sel-bg` and the two find colours,
but no gutter background (`viewer.js:88-95`).

A sticky element with a transparent background does not mask what scrolls
beneath it: on a file with long lines, scrolling right should draw the code
*through* the line numbers. FR-011 and the spec's horizontal-scroll edge case
require the column to remain a column while scrolling.

**Reproduced (2026-09-01)**: rendering the shipped stylesheet with
`scrollLeft = 260` shows the line numbers drawn *over* the scrolled code, both
visible and smeared together at the left edge. The defect is real and is worse
than "text is hidden" — nothing is hidden, everything overlaps.

**Decision**: treat this as in scope but **verify before changing** — the fix
is one line (`set('--gutter-bg', c['editorGutter.background'], 'var(--bg)')`,
matching how VS Code falls back to the editor background when a theme defines
no gutter colour), and it must not be applied on the strength of code reading
alone. The verification step is in `quickstart.md` §4. If the show-through is
not reproducible, the task is dropped with a note; the rest of the feature is
unaffected either way.

**Not changed**: `revealColumn` (`viewer.js:707`) already measures the gutter
before scrolling a find match into view, and reads the correct element (the
`.tx` node's `previousElementSibling` *is* its `.gut`). A constant width makes
that measurement stable rather than dependent on which row the match landed on;
no code change.

## R7 — Build and packaging impact

Web assets are embedded as `RCDATA` pointing at the files
(`web/assets.rc2:7-8`: `5001 RCDATA "web\\viewer.css"`), and
`web/assets_table.inc` maps URL paths to those resource ids. Editing the
*content* of an existing asset therefore needs no regeneration of
`assets_table.inc` and no run of `tools/codeview/build_web.py` — only a
rebuild of `codeview.spl`. No file is added or removed, so
`test/check_data.py`'s asset checks are unaffected.

Caveat for the quickstart: MSBuild's dependency tracking for files referenced
by `RCDATA` is not always reliable across incremental builds; if a change does
not appear, rebuild the plugin rather than assuming the CSS is wrong.

## R8 — Content Security Policy

`contracts/rendering-lockdown.md` §2 forbids inline `style` attributes. This
feature adds a rule to the *stylesheet*, and the JavaScript side keeps using
`documentElement.style.setProperty` through the CSSOM, which the page already
does for every theme colour and is not restricted by CSP. The change removes
one CSSOM write per row rather than adding any.

## Summary of decisions

| # | Decision |
|---|---|
| D1 | One CSS rule on `.gut` consuming the existing `--gutter-min`; no per-row JS. |
| D2 | `box-sizing: content-box` on `.gut` so the constraint covers the digits, not the digits-minus-padding. |
| D2b | `width`, not `min-width`: `1ch` is 0.0117 px narrower than a rendered digit, so `min-width` would not bind on the widest rows (R2b). |
| D3 | Right alignment needs no new declaration. |
| D4 | Extract `gutterDigitsFor(count)` so the node harness can lift and test it. |
| D5 | Publish `--gutter-min` before `resetGeometry()` in `layout()`. |
| D6 | Opaque gutter background — show-through **reproduced** (see R6), so applied: `--gutter-bg` defaults to `var(--bg)` and `applyThemeColors` sets it from `editorGutter.background`. |
| D7 | No asset-table regeneration; rebuild `codeview.spl` only. |
| D8 | No C++ change, no configuration, no plugin ABI change. |

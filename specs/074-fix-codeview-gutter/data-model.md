# Phase 1 Data Model: Fixed-width line-number gutter

**Feature**: 074-fix-codeview-gutter | **Date**: 2026-09-01

This feature stores nothing and defines no persisted entity: there is no new
setting, no registry value, no file format and no message added to the
host↔page protocol. What follows is the small set of **derived view values**
the fix introduces or repairs, recorded here because their invariants are what
the tests assert.

## Derived values

### `gutterDigits` — number of digits the column must hold

| | |
|---|---|
| Owner | the page (`viewer.js`), module-level state |
| Derived from | `lines.length` — the document's **total** line count |
| Rule | `gutterDigitsFor(count) = String(Math.max(1, count)).length` |
| Recomputed | on document load and on every `setView` (both go through `layout()`) |
| Range | 1 … 7+ (7 at one million lines) |
| Invariant | never derived from the lines currently rendered; the virtual list must not be able to influence it |

Existing state, previously computed inside `layout()`; this feature moves the
expression into a named function so the headless harness can test it
(research R4).

### `--gutter-min` — the published column width

| | |
|---|---|
| Owner | CSS custom property on `document.documentElement` |
| Value | `<gutterDigits>ch` |
| Producer | `layout()`, via `style.setProperty` |
| Consumer | the `.gut` rule in `viewer.css`, as `min-width` |
| Unit rationale | `ch` resolves at the use site against `.gut`'s own font, so the width follows zoom with no script |

Existing state — but **unconsumed until this feature**, which is the defect
(research R0). The producer/consumer pair is specified in
`contracts/gutter-geometry.md` §S2.

### Used width of `.gut` — what the reader actually sees

| | |
|---|---|
| Value | `gutterDigits` digit widths + `20 px` horizontal padding |
| Determined by | `min-width: var(--gutter-min)` under `box-sizing: content-box` |
| Invariant | identical for every row of a document, independent of the number that row holds |

The `content-box` exception to the page-wide `border-box` is load-bearing;
see `contracts/gutter-geometry.md` §S4.

## Values deliberately *not* introduced

- **No minimum digit count.** The column is exactly as wide as the document
  needs (spec Assumptions). No two- or three-digit floor is stored or
  configured.
- **No measured character width.** Nothing measures a digit in JavaScript; the
  `ch` unit does it in the engine, so there is no cached metric to invalidate
  on zoom, font change or DPI change.
- **No new field in `geometryKey`.** The wrap-mode height cache is keyed on
  `docGen` among others, and the digit count is a function of the document, so
  a document change already invalidates it (research R5).

## Relationships to existing state

```
lines.length ──► gutterDigits ──► --gutter-min ──► .gut min-width ──► text column offset
                                                          ▲
--font-size (zoom) ───────────────────────────────────────┘   (via the ch unit)

showGutter (host: lineNumbers) ──► body.nogutter ──► .gut display:none  (column absent)
```

No other viewer state participates: theme colours, tab size, whitespace
markers, find highlights, encoding and the tokenizer are untouched.

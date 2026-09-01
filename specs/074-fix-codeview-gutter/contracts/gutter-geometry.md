# Contract: line-number gutter geometry (codeview page)

**Feature**: 074-fix-codeview-gutter
**Applies to**: `src/plugins/codeview/web/viewer.js`, `src/plugins/codeview/web/viewer.css`
**Companion to**: `specs/070-source-viewer-plugin/contracts/rendering-lockdown.md`,
`specs/070-source-viewer-plugin/contracts/host-page-interface.md`

This contract exists because the mechanism it describes was already half-built
and silently disconnected: the page computed the gutter width and published it,
and no stylesheet rule ever read it. The producer and the consumer are written
down here as one thing so the two halves cannot drift apart again.

## S1 — The invariant

For a displayed document, the line-number column has **one width**, and every
line's source text begins at the same horizontal offset. The width does not
depend on which lines are currently materialised by the virtual list.

## S2 — The protocol between script and stylesheet

`--gutter-min` is the single channel. It is a **producer/consumer pair**:

| Side | Obligation |
|---|---|
| **Producer** — `viewer.js`, `layout()` | Writes `--gutter-min` on `document.documentElement` as `<N>ch`, where `N = gutterDigitsFor(lines.length)`, on every layout: document load and every `setView`. Writes it **before** `resetGeometry()` and `render()`. |
| **Consumer** — `viewer.css`, the `.gut` rule | Reads it as `width: var(--gutter-min)`, with no `var()` fallback. This is the only place the width is set. |

Neither side may be changed alone. A `--gutter-min` that no rule consumes is
the defect this feature fixes, and `test_page.mjs` asserts both halves exist.

`ch` is deliberate: it is substituted as a token stream and resolved at the use
site against `.gut`'s own font, which is the code font at the current zoom, so
the column scales with zoom with no script involvement.

## S3 — Digit count

```
gutterDigitsFor(count) === String(Math.max(1, count)).length
```

| lines | digits |
|---|---|
| 0 (empty document) | 1 |
| 1 … 9 | 1 |
| 10 … 99 | 2 |
| 100 … 999 | 3 |
| 1 000 000 | 7 |

The floor of 1 keeps the value defined for the empty document, which draws no
rows at all.

The count is the document's **total** line count, never the numbers on screen.
Deriving it from the visible window would make the column shift while scrolling.

## S4 — Box model: two load-bearing choices

Both were established by measuring a real engine, not by reading the spec.

**`box-sizing: content-box`**, against the page-wide
`* { box-sizing: border-box }`. The width must cover N digit widths with the
`padding: 0 12px 0 8px` outside it. Under border-box the 20 px of padding is
inside the constraint, which is then smaller than the natural
content-plus-padding size for every realistic digit count — the declaration
computes and does nothing, and the bug returns silently.

**`width`, not `min-width`.** `1ch` is the engine's advance for the "0" glyph
(7.14453 px at 13 px Consolas); a digit in shaped text advances 7.15625 px.
The two differ by 0.0117 px per digit, so under `min-width` the widest rows —
those whose number really has N digits — are not bound by the constraint and
keep their own slightly larger width. `width` pins every row to the same box;
the longest number then overflows its content box by 1/64 px into the 8 px of
left padding, where nothing can see it.

**No `var()` fallback.** If `--gutter-min` were ever absent the declaration is
invalid at computed-value time and `width` falls back to `auto` — the pre-074
behaviour — rather than to a column too narrow to hold the numbers.

Any future change to `.gut`'s padding or box-sizing must preserve:

> used width of `.gut` = `N` × `1ch` + horizontal padding, identical for every
> row of the document.

Measured on the shipped stylesheet, 1200 rows sampled at every digit boundary:
one text-column offset, spread 0.000 px (before the fix: four offsets,
27.16 / 34.30 / 41.45 / 48.59 px).

## S5 — Alignment

`text-align: right`. Units under units, tens under tens. The numbers are the
element's text content; alignment is achieved by the box, never by padding the
string.

## S6 — What must not change

- **No characters are inserted into the document text.** No padding spaces, no
  leading zeros, no substitute digits. Copy, Select All, find offsets and
  reported column positions stay byte-identical
  (feature 070 FR-021, this feature FR-010).
- **`user-select: none` stays** on `.gut` — line numbers never reach the
  clipboard.
- **The column stays opaque.** `.gut` is `position: sticky`, so long lines
  scroll underneath it; `--gutter-bg` must resolve to a real colour (the
  theme's `editorGutter.background`, else the editor background). Transparent —
  as it shipped — draws the code *through* the numbers.
- **No inline `style` attribute** is written per row. The width lives in the
  stylesheet; the only CSSOM write is the one custom property on the root
  element, once per layout (rendering-lockdown §2).
- **`body.nogutter .gut { display: none }` stays**: with line numbering off the
  column occupies no space, and the width machinery has no effect.
- **Wrapped continuation rows** begin at the text column: the number belongs to
  `.gut`, the wrapping happens inside `.tx`, and the two are separate flex
  items. Nothing in this contract changes that.

## S7 — Guards

`src/plugins/codeview/test/harness/test_page.mjs` enforces this contract
without a DOM:

1. **Shape** — `viewer.css` contains a `.gut` rule that declares
   `width: var(--gutter-min)`, `box-sizing: content-box` and
   `text-align: right`.
2. **Shape** — `viewer.js` writes `--gutter-min` in `layout()`, and `makeLine`
   contains no `style.minWidth` write.
3. **Behaviour** — `gutterDigitsFor` is lifted from the shipped file and
   checked against the S3 table.

A failure of guard 1 or 2 means the producer/consumer pair has been broken
again; that is the regression this contract is for.

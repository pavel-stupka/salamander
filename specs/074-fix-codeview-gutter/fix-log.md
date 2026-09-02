# Fix Log — Feature 074 (Code Viewer, line-number gutter)

**Session 2026-09-01.** Reported from a screenshot (`temp/radky.png`, a 21-line
JSON file) in the user's words, translated:

> The fundamental display defect is that the line numbering affects the
> alignment of the whole line, so the lines are shifted. In every editor with
> line numbering the number column must have the same width. Ideally the
> numbers are right-aligned in that column — units under units, tens under
> tens, hundreds under hundreds.

---

## What the diagnosis actually found

The mechanism was **already built and disconnected**. `layout()` computed the
document's digit count and published it as the custom property `--gutter-min`
on every layout — and **no CSS rule anywhere read it**. `.gut` therefore kept
`width: auto`, each row's number box was sized to its own number, and
`text-align: right` (present since feature 070) had nothing to align against.

The fossil of the abandoned attempt was still in the per-row hot path:

```js
g.style.minWidth = ''      // clears an inline style nothing sets
```

Nothing was wrong with any function's *logic*, which is why no behavioural test
could have caught it — see "Guards" below.

## Why the obvious fix would not have worked

Adding `min-width: var(--gutter-min)` alone would have been **silently inert**.
`* { box-sizing: border-box }` is page-wide and `.gut` carries 20 px of
padding, so the constraint sits *inside* the padding and loses to the natural
content-plus-padding width for every realistic digit count.

Measured in Edge/Chromium against the shipped stylesheet, 1200 rows:

| variant | `.gut` box-sizing | distinct text-column offsets |
|---|---|---|
| **as shipped** | border-box | **4** — 27.16 / 34.30 / 41.45 / 48.59 px |
| `+ min-width` only | border-box | **4** — 28.58 / 34.30 / 41.45 / 48.59 px (inert) |
| `+ min-width` + content-box | content-box | **2** — 48.58 / 48.59 px |
| `+ width` + content-box **(shipped fix)** | content-box | **1** — 48.58 px |

## The second surprise: `ch` is not the digit advance

`content-box` + `min-width` still left **two** offsets, 0.0156 px apart. The
cause is a genuine metric difference, not a rounding artefact:

| quantity | 13 px Consolas |
|---|---|
| `1ch` — the engine's advance for the "0" glyph | 7.14453 px |
| a digit's advance in shaped text | 7.15625 px |
| difference per digit | 0.01172 px |

So on the widest rows — the ones whose number really has N digits — the natural
content is fractionally wider than `Nch` and `min-width` does not bind. The fix
uses **`width`**, which pins every row to the same box; the longest number
overflows its content box by 1/64 px into the 8 px of left padding, where
nothing can see it. This changed decision D2 of `research.md` and is recorded
there as R2b.

No `var()` fallback is given on purpose: a missing property makes the
declaration invalid at computed-value time and `width` falls back to `auto` —
the pre-074 behaviour — rather than to a column too narrow for the numbers.

## The open question R6, answered: yes, and worse than described

`.gut` is `position: sticky` and `--gutter-bg` was declared `transparent` and
never assigned by `applyThemeColors`. Rendering the shipped stylesheet scrolled
right showed the line numbers drawn **over** the scrolled code, both visible and
smeared together at the left edge — nothing was hidden, everything overlapped.

Applied: `--gutter-bg` now defaults to `var(--bg)` in the stylesheet, and
`applyThemeColors` sets it from the theme's `editorGutter.background` with the
same `var(--bg)` fallback VS Code uses. Verified opaque on all three paths
(no theme info; light theme without the key; theme with the key).

## What changed

| File | Change |
|---|---|
| `web/viewer.css` | `.gut` gains `width: var(--gutter-min)` and `box-sizing: content-box`, with the two load-bearing reasons in a comment; `--gutter-bg` defaults to `var(--bg)` |
| `web/viewer.js` | `gutterDigitsFor(count)` extracted and named; `layout()` publishes the width *before* `resetGeometry()`; the dead `g.style.minWidth` write deleted; `--gutter-bg` added to the theme loop |
| `test/harness/test_page.mjs` | section 6: 6 unit checks + 7 shape checks |

No C++ file, no resource table, no configuration, no plugin ABI. Both web
assets verified **byte-for-byte** inside the built `codeview.spl` — the
quickstart's warning about `RCDATA` dependency tracking did not bite on this
build.

## Guards, each proven to fire

The defect was a computed value with no consumer, so the guards are shape
assertions over the shipped files. Every one was planted-and-observed:

| Guard | Planted defect | Fired |
|---|---|---|
| `.gut` consumes `--gutter-min` as its width | `min-width` instead of `width` | ✔ |
| `.gut` is content-box | `border-box` | ✔ |
| numbers flush right | `text-align: right` removed | ✔ |
| sticky column not transparent | `--gutter-bg: transparent` | ✔ |
| `layout()` publishes `--gutter-min` | (covered by the next row) | — |
| width comes from the whole document | `gutterDigitsFor(lastRendered)` | ✔ |
| `makeLine` writes no per-row width | the deleted line pasted back | ✔ |
| digit table (0→1 … 1000000→7) | `count - 1` (last index for count) | ✔ |

**One check does not discriminate and is kept knowingly**: `gutterDigitsFor(0)
=== 1` passes with or without the `Math.max(1, …)`, because `String(0).length`
is already 1. The floor is defensive only; the check documents the empty
document rather than testing the guard.

## Verified by measurement (no GUI)

The shipped stylesheet, driven through the cases the quickstart checks by hand.
`text_left_spread` is the difference between the largest and smallest text-column
offset over lines sampled at every digit boundary:

| case | text-left spread | number right-edge spread | verdict |
|---|---|---|---|
| 9 lines | 0.000 px | 0.000 px | pass |
| 120 lines | 0.000 px | 0.016 px | pass |
| 1200 lines | 0.000 px | 0.016 px | pass |
| 120 000 lines | 0.000 px | 0.016 px | pass |
| 1200 @ 8 px / 24 px / 40 px | 0.000 px | 0.000 px | pass — the column scales with zoom, with no script |
| 1200, wrap on | 0.000 px | 0.016 px | pass |
| 1200, gutter off | 0.000 px | — | pass — no column, text starts at 0 px |

The residual 0.016 px on the number glyph edges is the `ch`-vs-advance
difference above, on the longest number only. It is 1/64 px.

## Still to check in the running application

These are human steps, as in features 070 and 071. Everything above is
stylesheet-level evidence; it does not exercise the plugin's own scrolling,
clipboard or theme plumbing. `codeview.spl` is built and carries the fix.

- quickstart scenarios 1, 2, 3, 5, 9 — the fixtures in `%TEMP%\cvalign\`
- scenario 4 — horizontal scroll (the R6 fix, in the real viewer)
- scenario 10 — copy fidelity across the 9→10 boundary, Select All, wrapped lines
- scenario 11 — F9/Shift+F9 through all schemes, light and dark
- scenario 12 — `rendered.firstPaintMs` and scroll smoothness against a
  pre-change build

## Unrelated, pre-existing, not fixed here

> **Fixed by feature 075 (D6)**, commit `eecfca5`: the runner now passes
> `--experimental-detect-module` to the worker harness, so the verdict is the
> same on Node 20 and Node 22+. The third remedy guessed at below — a
> `web/package.json` — was rejected in that feature for the reason suspected
> here (a new file in the shipped asset tree, with an unverified interaction
> with `check_data.py`'s resource-table rule).

`src\plugins\codeview\test\run_tests.cmd` reports `RESULT: FAILURES` on this
machine **before and after** this feature. The cause is the tokenizer-worker
harness, not the product: `test_worker.mjs` imports `web/worker.js`, and the
installed **Node v20.18.0** treats a `.js` file as CommonJS because there is no
`package.json` with `"type": "module"` in that tree. It passes with
`node --experimental-detect-module`, which is on by default from Node 22.12.

Three one-line remedies exist (bump Node; add the flag to `run_tests.cmd`; add
`web/package.json` with `{"type":"module"}` — the last would need checking
against `check_data.py`'s "the resource table lists every generated asset"
rule). Left alone deliberately: it is outside this feature, and the red overall
`RESULT` line would otherwise be mistaken for a 074 regression.

`check_data.py` and `test_page.mjs` are green, the latter including all 13 new
gutter checks.

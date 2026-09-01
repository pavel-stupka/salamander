# Quickstart — Fixed-width line-number gutter (codeview)

Validation guide: how to build the change and prove it. The reasoning lives in
[research.md](research.md) and [contracts/gutter-geometry.md](contracts/gutter-geometry.md);
this file only says how to check it works.

## Prerequisites

- Windows 11, VS2022 C++ workload, `OPENSAL_BUILD_DIR` set (or the default
  `.\build\`), WebView2 Evergreen runtime present.
- Node (for the page harness) and Python 3.13 (for the data harness) — the
  versions pinned in `tools/codeview/README.md`. Neither is needed to *build*.
- `codeview=on` in `plugins.cfg` (it is, by default).

## 1. Build

```batch
build.cmd                 :: Debug x64 incremental; rebuilds plugins\codeview\codeview.spl
```

The page assets are embedded as `RCDATA` pointing at `web\viewer.css` and
`web\viewer.js` (`web\assets.rc2`), so no asset table is regenerated and
`tools\codeview\build_web.py` is not run — only the plugin's resources are
recompiled.

> **If a CSS change does not appear in the running viewer**, do not go hunting
> in the stylesheet: MSBuild's dependency tracking for `RCDATA`-referenced
> files is unreliable across incremental builds. Rebuild the plugin
> (`build.cmd rebuild`, or touch `src\plugins\codeview\codeview.rc`) and look
> again.

## 2. Automated checks

```batch
src\plugins\codeview\test\run_tests.cmd
```

Expect `RESULT: all codeview checks passed`. Three of the checks are new and
must be present and passing:

- shape: `viewer.css`'s `.gut` rule declares `min-width: var(--gutter-min)`,
  `box-sizing: content-box` and `text-align: right`;
- shape: `viewer.js` publishes `--gutter-min` in `layout()` and `makeLine`
  writes no `style.minWidth`;
- behaviour: `gutterDigitsFor` lifted from the shipped file matches the table
  in `contracts/gutter-geometry.md` §S3 (0→1, 9→1, 10→2, 99→2, 100→3, 999→3,
  1000→4, 1000000→7).

Guard 1 and 2 are what stop the producer/consumer pair from being silently
disconnected again — the exact shape of the original defect.

## 3. Fixtures

Alignment is easiest to judge on lines that all start with the same character:
any step in the text column then reads as a break in a solid vertical bar.

```powershell
$d = "$env:TEMP\cvalign"; New-Item -ItemType Directory -Force $d | Out-Null
1..120     | % { "|--- line $_" }            | Set-Content "$d\a120.txt"  -Encoding utf8
1..1200    | % { "|--- line $_" }            | Set-Content "$d\a1200.txt" -Encoding utf8
1..120000  | % { "|--- line $_" }            | Set-Content "$d\abig.txt"  -Encoding utf8
1..9       | % { "|--- line $_" }            | Set-Content "$d\a9.txt"    -Encoding utf8
1..40      | % { "|" + ("x" * 400) + " $_" } | Set-Content "$d\along.txt" -Encoding utf8
```

Also keep the reported case to hand: any JSON file of 20-30 lines, e.g. the
one in the screenshot `temp\radky.png`.

## 4. Validation scenarios

Each maps to a user story / success criterion in [spec.md](spec.md).

1. **The reported defect is gone (US1 / SC-001)** — F3 on `a120.txt`. The `|`
   characters form one unbroken vertical line from line 1 to line 120, with no
   step at 9→10 and none at 99→100. Repeat on `a1200.txt` for the 999→1000
   boundary, and on the JSON file from the screenshot.
2. **Right-aligned numbers (US2 / SC-002)** — in `a120.txt`, the final digits
   of `7`, `42` and `118` sit in one column, and `42`'s tens digit sits under
   `118`'s tens digit. The gap between the numbers and the `|` column is the
   same on every line.
3. **Short document (edge case)** — `a9.txt`: one digit wide, no wasted space,
   text as far left as the padding allows.
4. **Horizontal scroll (US1 / FR-011)** — `along.txt`, word wrap **off**;
   scroll right with the horizontal scrollbar or End. The number column stays
   pinned at the left edge at a constant width. **Watch specifically whether
   the code text is visible *through* the numbers** — that is the
   research R6 question. Record the answer in the fix log:
   - shows through → apply the one-line `--gutter-bg` fix and re-check here;
   - does not → drop that task with a note; nothing else changes.
5. **Large file, scrolling (US3 / SC-003)** — `abig.txt` (120 000 lines).
   Ctrl+End to the last line, then scroll back to the top with the wheel and
   the scrollbar. The text column never moves horizontally, at any point,
   including while rows are being materialised.
6. **Zoom (US3 / SC-005)** — Ctrl+`+` / Ctrl+`-` / Ctrl+`0` and Ctrl+wheel on
   `a1200.txt`: numbers and code scale together, alignment holds at every step,
   numbers stay flush right.
7. **Word wrap (US3 / FR-008)** — toggle wrap on `along.txt`: each logical line
   carries its number once, and every continuation row starts at the text
   column, not under the numbers. Toggle back off; alignment unchanged.
8. **Line numbers off/on (US3 / FR-007)** — toggle line numbering off: the text
   starts at the left edge with no empty column left behind. Toggle on: the
   aligned column returns, no reload, scroll position kept.
9. **New document in the same window (US3 / FR-005)** — with `a9.txt` open,
   open `a1200.txt` (Ctrl+PgDn from the panel, or F3 on the other file). The
   column resizes for the new document and the whole file is aligned; there is
   no visible reflow *after* the first frame.
10. **Copy is untouched (SC-004 / FR-010)** — select a block spanning the 9→10
    boundary and copy; paste into a plain-text editor. The text is exactly the
    file's own, with no line numbers, no leading spaces and no padding. Repeat
    with Select All, and with a selection crossing wrapped lines.
11. **Themes (FR-012)** — F9/Shift+F9 through the schemes on `a1200.txt`, light
    and dark: numbers stay legible, alignment unaffected, no other colour
    changes.
12. **No performance regression (SC-006)** — reopen a large source file and
    compare the reported `rendered.firstPaintMs` with a build from before the
    change; scroll `abig.txt` and confirm it is no less smooth. The change
    removes one CSSOM write per row per frame, so the expected direction is
    neutral-to-better.

## 5. Optional: measure instead of eyeball

If a pixel-level answer is wanted for scenario 1, the CSS behaviour can be
checked outside the plugin: copy `web/viewer.css` and a handful of hand-written
`.ln/.gut/.tx` rows into a scratch HTML file, open it in Chrome, and compare
`document.querySelectorAll('.tx')` bounding-rect `left` values — they must all
be equal. This proves the stylesheet rule, not the shipped plugin, so it
supplements scenario 1 rather than replacing it.

## 6. Definition of done

- `run_tests.cmd` green, including the three new checks.
- Scenarios 1-11 pass; scenario 4's observation recorded either way.
- No change to any `.cpp`/`.h` file, no configuration value, no plugin
  interface change (`LAST_VERSION_OF_SALAMANDER` untouched).
- CHANGELOG entry and version bump are **not** part of this feature; they
  belong to the release that ships it (see plan.md, ship gate).

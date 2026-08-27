# Quickstart — Source & Configuration File Viewer (codeview)

Validation guide: how to build, run and prove the feature end-to-end.
Details live in [plan.md](plan.md), [data-model.md](data-model.md) and
[contracts/](contracts/); this file only tells you how to check it works.

## Prerequisites

- Windows 11, VS2022 C++ workload, `OPENSAL_BUILD_DIR` set (or default
  `.\build\`), WebView2 Evergreen runtime present (it is on any stock
  Windows 11 — one scenario below removes it).
- Dev-side regeneration only (not needed to build): Python 3.13 and Node
  (pinned versions in `tools/codeview/README.md`).

## Build

```batch
build.cmd full            :: Debug x64; produces plugins\codeview\codeview.spl
                          :: + lang\english.slg; codeview auto-registers via plugins.ver
```

New-module translation bootstrap (once, before the first `build.cmd full`
after codeview's strings exist — otherwise language builds fail by design):

```batch
src\vcxproj\build_langs.cmd --export-templates
python -m translate.merge --module codeview        :: network: DeepL + Anthropic keys
build.cmd full
```

Harness (host-side automated checks — detection table, masks, licences,
intake, corpus escaping):

```batch
src\plugins\codeview\test\build_and_run.cmd
```

## Validation scenarios (map to spec user stories / SCs)

1. **Highlight on F3 (US1/SC-003)** — fresh app; F3 on `src\viewer.cpp`
   (or any `.cpp`): Code Viewer opens, C++ colours + line numbers; Esc
   closes. Re-open after closing all windows: text visible ≤ ~0.3 s
   (trace points log `rendered.firstPaintMs`).
2. **Themes (US2/SC-002)** — F9/Shift+F9 cycles schemes; View ▸ Color Scheme
   lists 12 (5 light / 7 dark) + Follow application theme; switch on an open
   1 MB file keeps scroll/selection, no reload; dark app theme ⇒ dark
   default, no white flash at open (watch the first frames), dark
   scrollbars. Markdown viewer opened alongside is unaffected.
3. **Safety (US3/SC-004)** — open every file in `test\corpus\hostile\`:
   literal display; debug request log shows only allow-listed URLs; F12,
   Ctrl+P, Ctrl+S, F5 do nothing browserish; no dialog/window/download ever.
4. **Detection breadth (US4/SC-001/SC-005)** — harness covers the table; spot
   checks: `Dockerfile`, `CMakeLists.txt`, `.gitignore`, `x.d.ts`,
   `setup.iss`, `a.reg` (UTF-16), extension-less `#!/usr/bin/env python3`
   file, `.h` with `@interface` (→ Objective-C), Qt `.ts` XML (→ XML).
   Language picker override re-highlights (menu ▸ Language).
5. **Degradation (US5/SC-006)** — 3 MB SQL dump: plain band + notice, find/
   wrap/themes work; 25 MB file and `.ts` video: built-in viewer opens
   directly, < 1 s; single-line 2 MB minified JS: opens, scrolls, wrap
   completes in seconds; change limits in configuration ⇒ next view honours
   them.
6. **Viewer parity (US6/SC-008/SC-009)** — find "n of N", F3/Shift+F3, case/
   whole-word; Ctrl+G `120:5`; copy a selection with tabs + trailing spaces →
   paste into an editor: byte-exact, CRLF, no line numbers; status bar shows
   encoding/EOL/language/zoom; F8 cycles code pages on a CP1250 `.ini`
   (`test\corpus\encodings\` has the matrix incl. UTF-16 no-BOM `.reg`);
   BOM never shown; invalid-UTF-8 file opens with replacement chars +
   count.
7. **Next/prev file (FR-041)** — panel with mixed files; open first, press
   the next-file key repeatedly: same window swaps content, language/encoding
   re-detected; a binary file in the sequence shows the in-window notice with
   "Open in built-in viewer".
8. **Claim policy & upgrade (SC-007)** — fresh config: Options ▸ Viewers
   shows ≤ 8 codeview rows above `*.*`; `.md` still opens in Markdown
   Viewer, `.csv` in dbviewer, `.txt`/`.log` in Code Viewer (plain); Alt+F3
   always = built-in viewer. Upgrade sim (harness): seeded custom `*.cpp`
   row survives; deleted codeview row stays deleted after restart; plugin
   config "Restore default file types" brings only codeview rows back.
9. **Zero cost before first use / keeper (US7/SC-010)** — fresh session,
   never open a source file: no `msedgewebview2.exe` beyond mdview's own
   use, startup unchanged (065 protocol). Open once, close all windows,
   wait, open again: warm (≤ budget). Configuration ▸ keep-ready off ⇒
   engine released when last window closes. Kill the browser process while
   idle-warm ⇒ next F3 works.
10. **Engine missing** — rename/disable the WebView2 runtime (test VM):
    F3 on `.cpp` opens the built-in viewer silently; no error outside a view
    attempt.

## Measurement spike (first implementation task — gates R1.6/R7.6)

Record in `spike-results.md` on the dev machine before freezing defaults:
time-to-first-text and time-to-highlighted for 10 KB / 100 KB / 1 MB / 5 MB /
single-line 2 MB; renderer memory per window; warm controller-attach time;
whether interceptor-served assets hit the HTTP/code cache; theme-switch
style-recalc cost on the 1 MB DOM. Decisions gated on it: gate defaults,
virtual-list confirmation (vs CodeMirror 6 fallback), theme-switch mechanism.

## Expected release notes (when shipping)

`CHANGELOG.md`: Added — Code Viewer plugin: syntax-highlighted read-only F3
viewer for ~225 source/configuration formats, 12 light/dark themes, claims
`*.txt`/`*.log`; built-in viewer remains via Alt+F3. Version + build bump per
constitution.

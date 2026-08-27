# Contract: Host ↔ Page Interface (codeview)

**Status**: binding. The complete surface between the C++ host and the
bundled page; anything not listed here does not exist. Complements
`rendering-lockdown.md` (which constrains it).

## 1. Virtual host resources (`https://codeview.invalid/`)

| URL | Type | Content |
|---|---|---|
| `viewer.html` | `text/html; charset=utf-8` | page shell; carries the CSP header (lockdown §2) |
| `viewer.css` | `text/css` | layout + token CSS variables + `data-theme` blocks |
| `viewer.js` | `text/javascript` | virtual line list, progressive highlight, find, message handling |
| `shiki/engine.js` | `text/javascript` | Shiki core + Oniguruma engine (WASM inlined as base64) |
| `worker.js` | `text/javascript` | tokenizer worker (viewport-first + chunked sweep) |
| `shiki/langs/<name>.mjs` | `text/javascript` | one language module, shipped as-is; its relative imports pull only the sub-grammars that language needs |
| `shiki/themes/<id>.mjs` | `text/javascript` | one shipped theme (VS Code JSON as an ESM module) |
| `text` | `text/plain; charset=utf-8` | the decoded document text (host re-issues a new version on next/prev or encoding override) |

Navigation: only `viewer.html?v=<n>` is permitted; `v` increments per opened
file when a full reload is wanted (encoding override), while next/prev file
swaps content via messages without navigation (FR-041, research D15).

## 2. Host → page messages (JSON via `PostWebMessageAsJson`)

| `type` | Payload | Purpose |
|---|---|---|
| `init` | `{lang, highlight:boolean, theme, themeInfo, wrap, lineNumbers, showWhitespace, tabSize, fontFamily, fontSize, maxLineLength, trailingNewline, plainReason?, v}` | first render config (band decided host-side) |
| `setTheme` | `{theme, themeInfo}` | attribute flip, no reload (FR-014); `themeInfo` carries the scheme's own colours so a plain-band file re-themes too |
| `setView` | `{wrap, lineNumbers, showWhitespace, tabSize, fontFamily, fontSize}` | toggles (FR-018/021). These six are the page's GEOMETRY inputs: it keeps measured wrap heights only while they are unchanged, so none may be dropped. A scheme change broadcasts `setView` as well. |
| `setLanguage` | `{lang}` | ~~user override (FR-007)~~ — the page still handles it, but the host stopped sending it when the language picker was removed (FR-007 amendment, 2026-08-27) |
| `find` | `{term, caseSensitive, wholeWord, dir:+1\|-1\|0}` | 0 = new search; ±1 = next/prev (FR-017) |
| `gotoLine` | `{line, col?}` | clamp + centre + transient mark (FR-019) |
| `swapText` | `{v, lang, …init fields}` | next/prev file: refetch `text`, reset state — find state, scroll position and Select-All state included (FR-041) |
| `copy` | `{}` | Edit ▸ Copy: the page answers with `copyText` (the HOST writes the clipboard — see §3) |
| `selectAll` | `{}` | Edit ▸ Select All: selects the document and remembers it; **must not touch the clipboard** |
| `notice` | `{text}` | show a localized line in the notice bar (empty hides it), e.g. "highlighting was stopped" |

## 3. Page → host messages (validated; unknown/malformed ⇒ ignored + debug log)

| `type` | Payload | Purpose |
|---|---|---|
| `ready` | `{}` | page loaded; host may `init` |
| `rendered` | `{firstPaintMs, lines}` | budget telemetry (SC-003 measurement) |
| `findResult` | `{current, total}` | "n of N" in the find bar (FR-017) |
| `caret` | `{line, col}` throttled | status bar Ln/Col (research D8) |
| `contextMenu` | `{x, y, hasSelection}` | host shows the native popup (D8) |
| `highlightDone` | `{ms}` \| `highlightAborted {reason}` | telemetry; abort ⇒ stays plain, host shows notice (edge case: pathological input) |
| `copyText` | `{all:true}` \| `{all:false, text}` | answer to `copy`. `all` asks the host to copy the WHOLE document from its own intake — the page must never assemble it from the DOM, which holds only the materialised rows and the gutter's line numbers. The clipboard is written host-side because a command from a native menu leaves the page without user activation and the shared host denies every permission request. |

Constraints: no paths and no HTML in any message; the ONE message that carries
file content is `copyText` with `all:false`, whose text is bounded by what the
virtual list has materialised and is length-capped host-side; payload
bounds checked (numbers clamped, strings length-capped); messages never
trigger navigation.

## 4. Page behaviour requirements

- Plain text visible first (virtual lines from `/text`), tokenisation
  progressive and abortable; scroll/keys never blocked (FR-004).
- Selection/copy: document text only; gutter numbers `user-select:none` and
  outside the text flow (FR-018/021); copy fidelity is host-verified in the
  quickstart (CRLF, tabs, trailing spaces).
- `color-scheme` set per theme polarity (dark scrollbars, FR-015);
  `font-variant-ligatures: none` default (D16).
- Whitespace rendering (optional mode) is paint-only — DOM text unchanged
  (FR-021).

## 5. Accelerator routing (host side)

`AcceleratorKeyPressed` forwards the parity map (built-in viewer keys →
`WM_COMMAND`): Esc close; Ctrl+F find; F3/Shift+F3 (and F6) next/prev match;
Ctrl+G goto; F2/Ctrl+W wrap; F8/Shift+F8 encoding next/prev; F9/Shift+F9
scheme; **Ctrl+C copy and Ctrl+A select-all** → `CM_EDIT_COPY`/
`CM_EDIT_SELALL` (they must NOT stay with the engine: the DOM holds only the
materialised rows, so an engine-side select-all/copy truncates a long file);
Ctrl+wheel zoom stays the engine's, but **Ctrl+0, Ctrl+plus and Ctrl+minus**
→ `CM_VIEW_ZOOMRESET/ZOOMIN/ZOOMOUT`, because the zoom KEYS are browser
accelerators and the shared lockdown disables those; **Ctrl+PgDn/Ctrl+PgUp**
→ `CM_NEXTFILE/CM_PREVFILE`; Ctrl+P, F5, F12, Ctrl+S swallowed (documented
no-ops; FR-025).

Keys the host does not claim reach the page, which implements the scrolling
set itself (arrows, PgUp/PgDn, Space/Shift+Space, Home/End): the scrolled
element is a `div`, so nothing scrolls unless the page has focus and handles
them (`web/viewer.js`).

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
| `shiki/worker.js` | `text/javascript` | tokenizer worker (viewport-first + chunked sweep) |
| `shiki/langs/<name>.mjs` | `text/javascript` | one language module, shipped as-is; its relative imports pull only the sub-grammars that language needs |
| `shiki/themes/<id>.mjs` | `text/javascript` | one shipped theme (VS Code JSON as an ESM module) |
| `text` | `text/plain; charset=utf-8` | the decoded document text (host re-issues a new version on next/prev or encoding override) |

Navigation: only `viewer.html?v=<n>` is permitted; `v` increments per opened
file when a full reload is wanted (encoding override), while next/prev file
swaps content via messages without navigation (FR-041, research D15).

## 2. Host → page messages (JSON via `PostWebMessageAsJson`)

| `type` | Payload | Purpose |
|---|---|---|
| `init` | `{lang, theme, wrap, lineNumbers, tabSize, fontFamily, fontSize, maxLineLen, highlight:boolean, plainReason?}` | first render config (band decided host-side) |
| `setTheme` | `{theme}` | attribute flip, no reload (FR-014) |
| `setView` | `{wrap?, lineNumbers?, tabSize?, showWhitespace?}` | toggles (FR-018/021) |
| `setLanguage` | `{lang}` | user override (FR-007) → re-tokenise |
| `find` | `{term, caseSensitive, wholeWord, dir:+1\|-1\|0}` | 0 = new search; ±1 = next/prev (FR-017) |
| `gotoLine` | `{line, col?}` | clamp + centre + transient mark (FR-019) |
| `swapText` | `{v, lang, …init fields}` | next/prev file: refetch `text`, reset state (FR-041) |

## 3. Page → host messages (validated; unknown/malformed ⇒ ignored + debug log)

| `type` | Payload | Purpose |
|---|---|---|
| `ready` | `{}` | page loaded; host may `init` |
| `rendered` | `{firstPaintMs, lines}` | budget telemetry (SC-003 measurement) |
| `findResult` | `{current, total}` | "n of N" in the find bar (FR-017) |
| `caret` | `{line, col}` throttled | status bar Ln/Col (research D8) |
| `contextMenu` | `{x, y, hasSelection}` | host shows the native popup (D8) |
| `highlightDone` | `{ms}` \| `highlightAborted {reason}` | telemetry; abort ⇒ stays plain, host shows notice (edge case: pathological input) |

Constraints: no file content, no paths, no HTML in any message; payload
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
Ctrl+G goto; F2/Ctrl+W wrap; F8/Shift+F8 encoding next/menu; F9/Shift+F9
scheme; Ctrl+0/±/wheel zoom (engine); Backspace/Space or the built-in
viewer's actual next/prev-file keys (verified in implementation) →
`CM_NEXTFILE/CM_PREVFILE`; Ctrl+A/C/Insert native; Ctrl+P, F5, F12, Ctrl+S
swallowed (documented no-ops; FR-025).

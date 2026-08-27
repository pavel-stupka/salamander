# Contract: codeview Rendering Lockdown (scripts enabled)

**Status**: binding. Implements spec FR-030…FR-033 under the decision that
`IsScriptEnabled` is TRUE on codeview's controllers only (clarification
2026-08-26). Threat model: the *viewed file* is hostile; the only code that
may run is the plugin's bundled, signed assets.

## 1. Settings matrix (applied by the shared routine, `webview-host-sharing.md` §2–3)

Everything mdview locks down stays locked down, except exactly:
`IsScriptEnabled=TRUE`, `IsWebMessageEnabled=TRUE`. Explicitly retained:
`AreHostObjectsAllowed=FALSE`, `AreDevToolsEnabled=FALSE`,
`AreDefaultScriptDialogsEnabled=FALSE` (no `alert`), default context menus
OFF, downloads/new windows/external-scheme launches/save-as/screen-capture
cancelled, permissions denied, browser accelerator keys OFF.

## 2. Content Security Policy (response header on the document, not `<meta>`)

```
default-src 'none'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self';
connect-src 'self'; img-src 'none'; object-src 'none'; worker-src 'self';
base-uri 'none'; form-action 'none'; frame-ancestors 'none'
```

- No `'unsafe-inline'` anywhere ⇒ the page carries no inline `<script>`,
  `<style>` or `style=` attribute. Token colours therefore become **generated
  CSS classes**: on theme load the page collects the theme's distinct
  colour/font-style combinations (a few dozen) and inserts one rule each
  through the CSSOM (`insertRule` on a stylesheet the page owns), which CSP
  permits — only inline style *attributes and elements* are blocked. Token
  spans then carry `class`, never `style`.
- `'wasm-unsafe-eval'` **is required** in `script-src`: the T013 spike replaced
  the JavaScript RegExp engine with Oniguruma (2.2× faster, `spike-results.md`
  §2). `shiki/wasm` inlines the binary as base64 and instantiates it from bytes,
  so no `.wasm` URL is ever requested and the asset allow-list below is
  unchanged.
- Served by the interceptor via `CreateWebResourceResponse` headers.

## 3. Asset serving (default-deny)

- All assets are `RCDATA` resources in the signed `codeview.spl`, served from
  memory with explicit `Content-Type` (+ `charset=utf-8` where textual).
- Allowed URLs on `https://codeview.invalid/`: `viewer.html`, `viewer.css`,
  `viewer.js`, `shiki/<chunk>.js` (existing chunks only), `themes/<id>.json`
  (shipped ids only), `text` (the decoded document, `text/plain;
  charset=utf-8`). **Everything else → 403.** No filesystem folder mapping.
- A debug-build request log records every answered/denied URL (FR-031 is
  verifiable from it).

## 4. Injection rules (the page code)

- File text enters the DOM exclusively via `textContent`/`createTextNode`;
  never string-concatenated into HTML; highlighter output is consumed as
  **token arrays** (`codeToTokens`), spans built by the page — there is no
  `innerHTML` sink for file-derived data.
- Web messages (both directions) are JSON with a fixed schema
  (`host-page-interface.md`); the host validates type/shape/bounds and
  ignores anything else; messages never carry file content or paths.
- Find/status/notice strings rendered by the page come only from the host's
  localized resources, passed by id or as text set via `textContent`.

## 5. Verification (all are tests, not review items)

1. **Hostile corpus** (`test/corpus/hostile/`, data-model §8): literal
   display, zero denied-log anomalies, zero navigation, unchanged title —
   in highlighted band, plain band, and every supported encoding (FR-030,
   SC-004).
2. **Lockdown assertion**: debug build reads back every setting after apply;
   mismatch fails loudly naming the setting (FR-033).
3. **Network probe**: corpus run with the request log — no URL outside §3's
   allow-list is ever *answered*; no `WebSocket`/beacon escapes (CSP blocks;
   probe pages included in the corpus).
4. **Key sweep**: F12, Ctrl+P, Ctrl+S, F5, Ctrl+F5, Ctrl+Shift+C/I, F7,
   Alt+←/→ produce viewer actions or nothing (FR-025/FR-032).
5. **Escape-hatch checks**: `window.open`, `location=`, `<a download>`,
   form submit, `srcdoc` iframe in corpus files stay inert.

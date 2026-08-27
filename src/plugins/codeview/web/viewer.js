// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// codeview page controller.
//
// Contract: contracts/host-page-interface.md (resources + message schema) and
// contracts/rendering-lockdown.md (no innerHTML for file data, no inline
// styles, no network beyond the interceptor).
//
// The file's text is DATA. It reaches the DOM only through textContent /
// createTextNode -- there is no string-concatenated HTML anywhere in this file.

const scroller = document.getElementById('scroller')
const sizer = document.getElementById('sizer')
const linesEl = document.getElementById('lines')
const noticeEl = document.getElementById('notice')

// First-paint colours: the host passes the active scheme in the URL fragment
// (#bg=RRGGBB&fg=RRGGBB&polarity=...) so the page never paints the stylesheet
// defaults in a scheme of the other polarity. This runs synchronously at
// module evaluation, before the first paint (spec FR-015).
;(function applyFragmentColors() {
  const p = new URLSearchParams(location.hash.slice(1))
  const hex = (v) => (v && /^[0-9a-fA-F]{6}$/.test(v)) ? '#' + v : null
  const r = document.documentElement.style
  const bg = hex(p.get('bg'))
  const fg = hex(p.get('fg'))
  if (bg) r.setProperty('--bg', bg)
  if (fg) r.setProperty('--fg', fg)
  const pol = p.get('polarity') === 'light' ? 'light' : 'dark'
  document.documentElement.dataset.polarity = pol
  const meta = document.querySelector('meta[name=color-scheme]')
  if (meta) meta.setAttribute('content', pol)
})()

let lines = []              // logical lines of the document
let lineHeight = 18         // measured
let firstRendered = -1      // first materialised line index
let lastRendered = -1
let tokens = []             // tokens[i] = packed triples for line i, or undefined
let styleClass = new Map()  // "colour|fontStyle" -> class name
let tokenSheet = null
let worker = null
let gen = 0                 // token generation (worker batches carry it)
let docGen = 0              // DOCUMENT generation: only a new file cancels a load
let lang = null
let themeId = null          // active scheme id (from init/setTheme)
let showGutter = true
let showWhitespace = false
let gutterDigits = 1
let highlighting = false
let selectAllActive = false // Select All covers the whole document, not the DOM

const OVERSCAN = 40         // lines rendered above/below the viewport

// ---------------------------------------------------------------- host bridge

function post(msg) {
  if (window.chrome && window.chrome.webview) window.chrome.webview.postMessage(msg)
}

function onHostMessage(e) {
  const m = e.data
  if (!m || typeof m.type !== 'string') return
  switch (m.type) {
    case 'init': return init(m)
    case 'swapText': return init(m)
    case 'setTheme': return setTheme(m)
    case 'setView': return setView(m)
    case 'setLanguage': return setLanguage(m.lang)
    case 'find': return doFind(m)
    case 'gotoLine': return gotoLine(m.line | 0, m.col | 0)
    case 'selectAll': return doSelectAll()
    case 'copy': return doCopy()
    case 'notice': return showNotice(m.text || '')
  }
}

// ------------------------------------------------------------------- theming

function applyThemeColors(t) {
  if (!t) return
  const r = document.documentElement.style
  const c = t.colors || {}
  // EVERY property is written on every theme change. Setting only the keys the
  // incoming theme happens to define leaves the previous theme's value behind
  // for the rest -- e.g. a light scheme keeping the dark scheme's selection
  // colour, invisible against the new background. Themes that do not define a
  // colour get one derived from their own foreground instead.
  const set = (name, v, fallback) => r.setProperty(name, v || fallback)
  set('--bg', c['editor.background'] || t.bg, '#1e1e1e')
  set('--fg', c['editor.foreground'] || t.fg, '#d4d4d4')
  set('--gutter-fg', c['editorLineNumber.foreground'], 'color-mix(in srgb, var(--fg) 55%, transparent)')
  set('--sel-bg', c['editor.selectionBackground'], 'color-mix(in srgb, var(--fg) 25%, transparent)')
  set('--find-bg', c['editor.findMatchHighlightBackground'], 'color-mix(in srgb, var(--fg) 22%, transparent)')
  set('--find-current-bg', c['editor.findMatchBackground'], 'color-mix(in srgb, var(--fg) 42%, transparent)')
  document.documentElement.dataset.polarity = t.type === 'light' ? 'light' : 'dark'
  document.querySelector('meta[name=color-scheme]')
    .setAttribute('content', t.type === 'light' ? 'light' : 'dark')
}

// Token colours become generated CSS classes: the CSP forbids inline style
// attributes, and a theme only has a few dozen distinct colour/style runs.
function classFor(key) {
  if (!key) return null
  let cls = styleClass.get(key)
  if (cls) return cls
  if (!tokenSheet) {
    tokenSheet = new CSSStyleSheet()
    document.adoptedStyleSheets = [...document.adoptedStyleSheets, tokenSheet]
  }
  cls = 't' + styleClass.size
  const [color, fs] = key.split('|')
  const bits = fs ? parseInt(fs, 10) : 0
  let decl = ''
  if (color && /^#[0-9a-fA-F]{3,8}$/.test(color)) decl += `color:${color};`
  if (bits & 1) decl += 'font-style:italic;'
  if (bits & 2) decl += 'font-weight:bold;'
  if (bits & 4) decl += 'text-decoration:underline;'
  if (bits & 8) decl += 'text-decoration:line-through;'
  try { tokenSheet.insertRule(`.${cls}{${decl}}`, tokenSheet.cssRules.length) } catch (e) { return null }
  styleClass.set(key, cls)
  return cls
}

function resetTokenClasses() {
  styleClass = new Map()
  if (tokenSheet) {
    while (tokenSheet.cssRules.length) tokenSheet.deleteRule(0)
  }
}

// -------------------------------------------------------------- line building

function styledNode(text, cls) {
  if (!cls) return document.createTextNode(text)
  const span = document.createElement('span')
  span.className = cls
  span.textContent = text                 // file text: always textContent
  return span
}

// Whitespace marks are DECORATION drawn by CSS over spans that still contain
// the real spaces and tabs, so Copy stays byte-exact (spec FR-021). Before
// this, "Show Whitespace" only toggled a body class no rule could ever match:
// the stylesheet styled a ".sp" element the page never created.
function appendPiece(frag, text, cls) {
  if (!showWhitespace || !/[ \t]/.test(text)) {
    frag.appendChild(styledNode(text, cls))
    return
  }
  let run = 0
  for (let j = 1; j <= text.length; j++) {
    const prev = text[j - 1]
    const cur = j < text.length ? text[j] : null
    const kind = (ch) => (ch === ' ' ? 1 : ch === '\t' ? 2 : 0)
    if (cur !== null && kind(cur) === kind(prev))
      continue
    const piece = text.slice(run, j)
    const k = kind(prev)
    if (k === 0)
      frag.appendChild(styledNode(piece, cls))
    else
      frag.appendChild(styledNode(piece, (cls ? cls + ' ' : '') + (k === 2 ? 'tb' : 'sp')))
    run = j
  }
}

function buildLineText(i) {
  const frag = document.createDocumentFragment()
  const text = lines[i]
  const tk = tokens[i]
  if (!tk || tk.length === 0) {
    appendPiece(frag, text, null)
    return frag
  }
  let pos = 0
  for (let k = 0; k < tk.length; k += 3) {
    const off = tk[k], len = tk[k + 1], key = tk[k + 2]
    if (off > pos) appendPiece(frag, text.slice(pos, off), null)
    appendPiece(frag, text.substr(off, len), classFor(key))
    pos = off + len
  }
  if (pos < text.length) appendPiece(frag, text.slice(pos), null)
  return frag
}

function makeLine(i) {
  const row = document.createElement('div')
  row.className = 'ln'
  row.dataset.i = String(i)
  if (showGutter) {
    const g = document.createElement('span')
    g.className = 'gut'
    g.textContent = String(i + 1)
    g.style.minWidth = ''
    row.appendChild(g)
  }
  const tx = document.createElement('span')
  tx.className = 'tx'
  tx.appendChild(buildLineText(i))
  row.appendChild(tx)
  return row
}

function measure() {
  const r = document.documentElement.style
  r.removeProperty('--line-height')   // measure the font's natural height...
  const probe = document.createElement('div')
  probe.className = 'ln'
  probe.textContent = 'X'
  linesEl.appendChild(probe)
  const natural = probe.getBoundingClientRect().height || 18
  probe.remove()
  // ...then pin the row height to a whole pixel. Every row offset, the sizer
  // height and the scroll range are then exact integers: no cumulative
  // fraction can push the last lines beyond the reachable scroll range, and
  // glyphs never land on half-pixel baselines (fix-log defects 4 and 7).
  lineHeight = Math.max(1, Math.round(natural))
  r.setProperty('--line-height', lineHeight + 'px')
}

// ------------------------------------------------------------------ geometry
//
// Without wrap every row is exactly lineHeight tall and index<->pixel is one
// multiplication. With wrap a logical line occupies as many visual rows as it
// needs, so the mapping is driven by a measured height per line: `heights`
// holds the current estimate (lineHeight until the line has been rendered at
// least once) and `tops` its prefix sums. Before this, wrap mode kept the
// uniform math and every offset, the sizer height and the scroll range were
// wrong the moment one line wrapped -- the file end became unreachable again
// (fix-log defect 4, reintroduced whenever wrap was on).
let heights = null
let tops = null
let wrapMode = false
let pendingScrollTop = -1   // a scroll position WE set, awaiting its event
// Everything that changes how tall a line is. Only a change here invalidates
// the measured heights; a colour scheme does not.
let geometryKey = ''
let lastGeometryKey = null

function resetGeometry() {
  if (!wrapMode) { heights = null; tops = null; return }
  // Keep what has already been measured when the document and the metrics are
  // the same: a scheme switch now reaches every window as a setView broadcast,
  // and rebuilding from estimates there would shrink the sizer under the
  // reader and throw the scroll position forward.
  if (heights && heights.length === lines.length && geometryKey === lastGeometryKey)
    return
  heights = new Float64Array(lines.length)
  heights.fill(lineHeight)
  lastGeometryKey = geometryKey
  rebuildTops(0)
}

// Prefix sums from 'start' onward. Heights only ever change inside the window
// just rendered, so the suffix is all that can move -- walking the whole array
// on every scroll frame was an O(lines) pass per frame in wrap mode.
function rebuildTops(start) {
  const n = heights ? heights.length : 0
  if (!tops || tops.length !== n + 1) { tops = new Float64Array(n + 1); start = 0 }
  let i = Math.max(0, start | 0)
  let acc = i === 0 ? 0 : tops[i]
  for (; i < n; i++) { tops[i] = acc; acc += heights[i] }
  tops[n] = acc
}

function lineTop(i) {
  if (!wrapMode || !tops) return i * lineHeight
  return tops[Math.max(0, Math.min(i, tops.length - 1))]
}

function lineHeightAt(i) {
  if (!wrapMode || !heights || i < 0 || i >= heights.length) return lineHeight
  return heights[i]
}

function totalHeight() {
  if (!wrapMode || !tops) return lines.length * lineHeight
  return tops[tops.length - 1]
}

// The line containing pixel offset `px`.
function lineAt(px) {
  if (!wrapMode || !tops) return Math.floor(px / lineHeight)
  if (px <= 0 || tops.length < 2) return 0
  let lo = 0, hi = tops.length - 2
  while (lo < hi) {
    const mid = (lo + hi + 1) >> 1
    if (tops[mid] <= px) lo = mid
    else hi = mid - 1
  }
  return lo
}

// Replace the estimated heights of the rows now in the DOM with their real
// ones, keeping the line under the viewport top where the user sees it.
function measureRendered(from) {
  if (!wrapMode || !heights) return
  const rows = linesEl.children
  let first = -1
  for (let k = 0; k < rows.length; k++) {
    const i = from + k
    if (i >= heights.length) break
    const h = rows[k].offsetHeight
    if (h > 0 && Math.abs(h - heights[i]) > 0.5) { heights[i] = h; if (first < 0) first = i }
  }
  if (first < 0) return
  const anchor = lineAt(scroller.scrollTop)
  const within = scroller.scrollTop - lineTop(anchor)
  rebuildTops(first)
  sizer.style.height = totalHeight() + 'px'
  linesEl.style.top = lineTop(from) + 'px'
  const want = Math.max(0, lineTop(anchor) + within)
  if (Math.abs(want - scroller.scrollTop) > 0.5) {
    // The scroll event this fires is delivered LATER, so the correction is
    // remembered by VALUE until that event arrives. A plain boolean cleared
    // here suppressed nothing (the event had not run yet), and a boolean left
    // set would swallow the next real user scroll if the event never came.
    pendingScrollTop = want
    scroller.scrollTop = want
  }
}

function render(force) {
  const top = scroller.scrollTop
  const height = scroller.clientHeight || 400
  let from = Math.max(0, lineAt(top) - OVERSCAN)
  let to = Math.min(lines.length, lineAt(top + height) + 1 + OVERSCAN)
  if (!force && from === firstRendered && to === lastRendered) return
  firstRendered = from
  lastRendered = to
  const frag = document.createDocumentFragment()
  for (let i = from; i < to; i++) frag.appendChild(makeLine(i))
  linesEl.replaceChildren(frag)
  // Layout offset, not translateY: a transform would composite the text onto
  // its own layer and cost subpixel antialiasing (fix-log defect 7).
  linesEl.style.top = lineTop(from) + 'px'
  measureRendered(from)
  if (highlighting) {
    // Ask the worker for the window actually on screen (viewport-first).
    post0({ type: 'viewport', from, to, gen })
  }
  reapplyFindHighlights()
}

function post0(msg) { if (worker) worker.postMessage(msg) }

function layout() {
  resetGeometry()
  sizer.style.height = totalHeight() + 'px'
  gutterDigits = String(Math.max(1, lines.length)).length
  document.documentElement.style.setProperty('--gutter-min', gutterDigits + 'ch')
  render(true)
}

// --------------------------------------------------------------------- notice

function showNotice(text) {
  if (!text) { noticeEl.hidden = true; noticeEl.textContent = ''; return }
  noticeEl.textContent = text           // host-provided, localized; still textContent
  noticeEl.hidden = false
}

// ------------------------------------------------------------------ lifecycle

async function init(m) {
  gen++
  // The DOCUMENT generation, not the token generation: `gen` is also bumped by
  // a scheme switch or a language change, so aborting on it meant that pressing
  // F9 while a file was still loading abandoned the load and left the window
  // empty for good.
  docGen++
  const myDoc = docGen
  if (m.theme) themeId = m.theme
  // The host-known colours apply NOW; the worker's full palette (selection,
  // find, gutter) refines them once the theme module is loaded.
  applyThemeColors(m.themeInfo || null)
  applyView(m)
  const res = await fetch('text', { cache: 'no-store' })
  const text = await res.text()
  if (myDoc !== docGen) return
  // A new document starts clean. Without this, Ctrl+PgDn carried the previous
  // file's find matches (painted at its line/column positions, and F3 stepped
  // through them because the term had not changed) and its scroll offset into
  // the new file -- while the host had already reset its own counters.
  clearFind()
  selectAllActive = false
  pendingScrollTop = 0
  scroller.scrollTop = 0
  scroller.scrollLeft = 0
  // An empty file has no lines at all: splitting "" yields one empty element,
  // which drew a gutter row "1" against the host's status bar saying 0 lines.
  lines = text === '' ? [] : text.split('\n')
  // A trailing newline yields a final empty element; keep it only if the file
  // really ends without a newline (the host tells us via m.trailingNewline).
  if (lines.length > 1 && lines[lines.length - 1] === '' && m.trailingNewline !== false) lines.pop()
  tokens = new Array(lines.length)
  resetTokenClasses()
  lang = m.highlight ? (m.lang || null) : null
  showNotice(m.plainReason || '')
  measure()
  layout()
  post({ type: 'rendered', firstPaintMs: Math.round(performance.now()), lines: lines.length })
  focusScroller()
  highlighting = !!lang
  if (highlighting) startWorker(m)
  else if (worker) { worker.terminate(); worker = null }
}

function startWorker(m) {
  if (!worker) {
    worker = new Worker('worker.js', { type: 'module' })
    worker.onmessage = onWorkerMessage
    worker.onerror = () => {
      highlighting = false
      post({ type: 'highlightAborted', reason: 'worker' })
    }
  }
  worker.postMessage({
    type: 'load', lines, lang, theme: m.theme || themeId,
    maxLineLength: m.maxLineLength || 20000,
    from: firstRendered, to: lastRendered, gen,
  })
}

function onWorkerMessage(e) {
  const m = e.data
  if (!m) return
  if (m.type === 'tokens') {
    if (m.gen !== gen) return
    for (let i = 0; i < m.tokens.length; i++) tokens[m.firstLine + i] = m.tokens[i]
    // Repaint only the lines currently on screen that this chunk covered.
    const lo = Math.max(m.firstLine, firstRendered)
    const hi = Math.min(m.firstLine + m.tokens.length, lastRendered)
    for (let i = lo; i < hi; i++) {
      const row = linesEl.querySelector(`.ln[data-i="${i}"] .tx`)
      if (row) row.replaceChildren(buildLineText(i))
    }
    if (hi > lo) reapplyFindHighlights()
  } else if (m.type === 'ready') {
    applyThemeColors(m.theme)
  } else if (m.type === 'sweepDone') {
    post({ type: 'highlightDone', ms: Math.round(performance.now()) })
  } else if (m.type === 'failed') {
    highlighting = false
    post({ type: 'highlightAborted', reason: m.message || 'error' })
  }
}

// ----------------------------------------------------------------- view state

function applyView(m) {
  const r = document.documentElement.style
  if (m.tabSize) r.setProperty('--tab-size', String(m.tabSize))
  // The configured family is appended to the stylesheet's fallback stack, not
  // substituted for it: a family that is not installed (a typo, or a profile
  // carried to another machine) would otherwise drop the code view to the
  // engine's proportional default instead of Consolas.
  if (m.fontFamily)
    r.setProperty('--font-family',
      JSON.stringify(String(m.fontFamily)) + ', "Cascadia Mono", Consolas, "Courier New", monospace')
  // 0 is the documented "use the page default" value, not an absent field --
  // testing for truthiness meant setting the size back to 0 never took effect.
  if (typeof m.fontSize === 'number')
    m.fontSize > 0 ? r.setProperty('--font-size', m.fontSize + 'px') : r.removeProperty('--font-size')
  if (typeof m.wrap === 'boolean') {
    wrapMode = m.wrap
    document.body.classList.toggle('wrap', m.wrap)
  }
  if (typeof m.lineNumbers === 'boolean') {
    showGutter = m.lineNumbers
    document.body.classList.toggle('nogutter', !m.lineNumbers)
  }
  if (typeof m.showWhitespace === 'boolean') {
    showWhitespace = m.showWhitespace
    document.body.classList.toggle('ws', m.showWhitespace)
  }
  // Recomputed after every view message: resetGeometry compares it to decide
  // whether the measured wrap heights are still valid. A colour scheme is not
  // in it, so switching schemes no longer discards them -- but the DOCUMENT is,
  // because another file of the same length wraps differently.
  geometryKey = [docGen, wrapMode, showWhitespace, showGutter, m.tabSize || '',
                 m.fontFamily || '', m.fontSize || ''].join('|')
}

function setView(m) {
  applyView(m)
  measure()
  layout()
}

function setTheme(m) {
  if (m.theme) themeId = m.theme
  // Applies to plain files too -- the host always sends themeInfo, so a
  // scheme switch recolours the page even when nothing is tokenized.
  applyThemeColors(m.themeInfo || null)
  if (!highlighting) return
  resetTokenClasses()
  tokens = new Array(lines.length)
  gen++
  post0({ type: 'retheme', theme: themeId, from: firstRendered, to: lastRendered, gen })
  render(true)
}

function setLanguage(next) {
  lang = next || null
  resetTokenClasses()
  tokens = new Array(lines.length)
  gen++
  if (!lang) { highlighting = false; render(true); return }
  highlighting = true
  if (!worker) { startWorker({ theme: themeId }); return }
  post0({ type: 'relang', lang, from: firstRendered, to: lastRendered, gen })
  render(true)
}

// ---------------------------------------------------------------------- find

let findMatches = []     // {line, start, end}
let findCurrent = -1
let findTerm = ''

function clearFind() {
  findMatches = []
  findCurrent = -1
  findTerm = ''
  if ('highlights' in CSS) {
    CSS.highlights.delete('cv-find')
    CSS.highlights.delete('cv-find-current')
  }
}

// Lower-casing must not move offsets: the match positions computed on the
// folded text are applied to the ORIGINAL line. Exactly one BMP code point
// (U+0130, dotted capital I) folds to two units, and it shifted every
// highlight after it on that line -- past the end of the line for a few of
// them. Such a character keeps its own case instead.
function lowerKeepLen(s) {
  if (!/[İ]/.test(s)) return s.toLowerCase()
  let out = ''
  for (const ch of s) {
    const l = ch.toLowerCase()
    out += l.length === ch.length ? l : ch
  }
  return out
}

function doFind(m) {
  const term = m.term || ''
  if (m.dir === 0 || term !== findTerm) {
    findTerm = term
    findMatches = []
    findCurrent = -1
    if (term) searchAll(term, !!m.caseSensitive, !!m.wholeWord)
    if (findMatches.length) findCurrent = nearestMatch()
  } else if (findMatches.length) {
    findCurrent = (findCurrent + (m.dir < 0 ? -1 : 1) + findMatches.length) % findMatches.length
  }
  post({ type: 'findResult', current: findMatches.length ? findCurrent + 1 : 0, total: findMatches.length })
  if (findCurrent >= 0) {
    revealLine(findMatches[findCurrent].line)
    render(true)
    revealColumn(findMatches[findCurrent])
  }
  reapplyFindHighlights()
}

function isWordChar(ch) { return /[A-Za-z0-9_]/.test(ch) }

function searchAll(term, caseSensitive, wholeWord) {
  const needle = caseSensitive ? term : lowerKeepLen(term)
  for (let i = 0; i < lines.length; i++) {
    const hay = caseSensitive ? lines[i] : lowerKeepLen(lines[i])
    let at = 0
    for (;;) {
      const p = hay.indexOf(needle, at)
      if (p < 0) break
      const before = p > 0 ? hay[p - 1] : ''
      const after = p + needle.length < hay.length ? hay[p + needle.length] : ''
      if (!wholeWord || ((!before || !isWordChar(before)) && (!after || !isWordChar(after))))
        findMatches.push({ line: i, start: p, end: p + needle.length })
      at = p + Math.max(1, needle.length)
      if (findMatches.length > 100000) return      // pathological input guard
    }
  }
}

function nearestMatch() {
  const firstVisible = lineAt(scroller.scrollTop)
  for (let i = 0; i < findMatches.length; i++) if (findMatches[i].line >= firstVisible) return i
  return 0
}

// Highlights are painted with the CSS Custom Highlight API: no DOM mutation,
// so the virtual list, the selection and Copy are all unaffected.
function reapplyFindHighlights() {
  if (!('highlights' in CSS)) return
  CSS.highlights.delete('cv-find')
  CSS.highlights.delete('cv-find-current')
  if (!findMatches.length) return
  const all = new Highlight()
  const cur = new Highlight()
  for (let k = 0; k < findMatches.length; k++) {
    const mm = findMatches[k]
    if (mm.line < firstRendered || mm.line >= lastRendered) continue
    const row = linesEl.querySelector(`.ln[data-i="${mm.line}"] .tx`)
    if (!row) continue
    const range = rangeInLine(row, mm.start, mm.end)
    if (!range) continue
    if (k === findCurrent) cur.add(range); else all.add(range)
  }
  if (all.size) CSS.highlights.set('cv-find', all)
  if (cur.size) CSS.highlights.set('cv-find-current', cur)
}

function rangeInLine(container, start, end) {
  const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT)
  let pos = 0, range = document.createRange(), started = false
  for (let n = walker.nextNode(); n; n = walker.nextNode()) {
    const len = n.nodeValue.length
    if (!started && start >= pos && start <= pos + len) {
      range.setStart(n, start - pos)
      started = true
    }
    if (started && end >= pos && end <= pos + len) {
      range.setEnd(n, end - pos)
      return range
    }
    pos += len
  }
  return started ? null : null
}

// ------------------------------------------------------------ copy / select
//
// The clipboard is written by the HOST, never here: this page has no transient
// user activation when the command arrives from a native menu, and the shared
// host denies every permission request, so navigator.clipboard would fail
// silently. The text is built from `lines` rather than from a DOM range for
// two reasons the virtual list forces: only the rendered window exists in the
// DOM (a DOM select-all would silently truncate a long file), and the gutter
// span sits inside each row (a DOM copy would carry the line numbers, which
// spec FR-021 forbids).

// Select All SELECTS. It must not touch the clipboard -- it only remembers
// that the whole document is selected, so a later Copy answers with the whole
// document instead of the materialised rows.
function doSelectAll() {
  selectAllActive = true
  const sel = document.getSelection()
  if (sel && linesEl.firstChild) {
    const range = document.createRange()
    range.selectNodeContents(linesEl)
    sel.removeAllRanges()
    sel.addRange(range) // visual feedback over the materialised rows
  }
}

function doCopy() {
  const sel = document.getSelection()
  const text = sel ? String(sel) : ''
  if (selectAllActive || (text && linesEl.contains(sel.anchorNode) && isWholeRendered(sel))) {
    post({ type: 'copyText', all: true })
    return
  }
  post({ type: 'copyText', text, all: false })
}

// A selection that really spans the whole materialised list, on a document
// that is materialised in full, is the user asking for the document. The test
// is exact on purpose: a tolerance would turn "almost everything" in a short
// file into a whole-file copy the user did not ask for.
function isWholeRendered(sel) {
  if (!sel.rangeCount || firstRendered > 0 || lastRendered < lines.length) return false
  const r = sel.getRangeAt(0)
  const whole = document.createRange()
  whole.selectNodeContents(linesEl)
  return r.compareBoundaryPoints(Range.START_TO_START, whole) <= 0 &&
         r.compareBoundaryPoints(Range.END_TO_END, whole) >= 0
}

// -------------------------------------------------------------- goto / scroll

function revealLine(i) {
  const target = lineTop(i)
  const rowH = lineHeightAt(i)
  const top = scroller.scrollTop
  const h = scroller.clientHeight
  if (target < top || target + rowH > top + h) {
    scroller.scrollTop = Math.max(0, target - Math.floor(h / 2) + rowH)
    render(true)
  }
}

// Vertical alone is not enough: a match far to the right of a long line was
// scrolled onto the right screen row and stayed outside the window, so the
// user saw a "1 of N" that pointed at nothing. Runs after the row exists.
function revealColumn(m) {
  if (document.body.classList.contains('wrap')) return // no horizontal scroll
  const row = linesEl.querySelector(`.ln[data-i="${m.line}"] .tx`)
  if (!row) return
  const range = rangeInLine(row, m.start, m.end)
  if (!range) return
  const r = range.getBoundingClientRect()
  const view = scroller.getBoundingClientRect()
  const gutter = showGutter ? (row.previousElementSibling ? row.previousElementSibling.getBoundingClientRect().width : 0) : 0
  const margin = 40
  if (r.left < view.left + gutter + margin)
    scroller.scrollLeft = Math.max(0, scroller.scrollLeft + (r.left - view.left - gutter - margin))
  else if (r.right > view.right - margin)
    scroller.scrollLeft += r.right - view.right + margin
}

function gotoLine(line, col) {
  if (!lines.length) return
  const i = Math.min(Math.max(1, line), lines.length) - 1
  revealLine(i)
  render(true)
  const c = Math.max(1, col || 1)
  revealColumn({ line: i, start: c - 1, end: c })
  const row = linesEl.querySelector(`.ln[data-i="${i}"]`)
  if (row) { row.classList.remove('flash'); void row.offsetWidth; row.classList.add('flash') }
  post({ type: 'caret', line: i + 1, col: c })
}

// ------------------------------------------------------------------- wiring

scroller.addEventListener('scroll', () => {
  const mine = pendingScrollTop >= 0 && Math.abs(scroller.scrollTop - pendingScrollTop) < 1
  pendingScrollTop = -1
  if (mine) return          // our own anchor correction, already rendered
  render(false)
}, { passive: true })
window.addEventListener('resize', () => { measure(); layout() })
// A LEFT click starts a new selection, so a pending "Select All" no longer
// stands. Right-click must not clear it: mousedown fires before contextmenu,
// so clearing here made "Select All, right-click, Copy" copy the render window
// instead of the document -- silently, which is exactly the truncation the
// host-side whole-document copy exists to prevent.
document.addEventListener('mousedown', (e) => { if (e.button === 0) selectAllActive = false })

// Keyboard scrolling (fix-log defect 5). The scroller is the page's only focus
// target, but the host's programmatic focus lands on <body>, so the viewer
// keys are handled here explicitly instead of depending on where focus sits.
function focusScroller() {
  try { scroller.focus({ preventScroll: true }) } catch (e) {}
}
window.addEventListener('focus', focusScroller)

document.addEventListener('keydown', (e) => {
  if (e.defaultPrevented || e.altKey) return
  const page = Math.max(lineHeight, scroller.clientHeight - lineHeight)
  let handled = true
  switch (e.key) {
    case 'ArrowDown': scroller.scrollTop += lineHeight; break
    case 'ArrowUp': scroller.scrollTop -= lineHeight; break
    case 'ArrowRight': scroller.scrollLeft += 40; break
    case 'ArrowLeft': scroller.scrollLeft -= 40; break
    case 'PageDown': scroller.scrollTop += page; break
    case 'PageUp': scroller.scrollTop -= page; break
    case ' ': scroller.scrollTop += e.shiftKey ? -page : page; break
    case 'Home':
      if (e.shiftKey) { handled = false; break }
      scroller.scrollTop = 0
      scroller.scrollLeft = 0
      break
    case 'End':
      if (e.shiftKey) { handled = false; break }
      scroller.scrollTop = scroller.scrollHeight
      break
    default:
      handled = false
  }
  if (handled) e.preventDefault()
})

document.addEventListener('selectionchange', throttle(() => {
  const sel = document.getSelection()
  if (!sel || sel.rangeCount === 0) return
  const node = sel.focusNode
  const el = node && (node.nodeType === 1 ? node : node.parentElement)
  const ln = el && el.closest ? el.closest('.ln') : null
  if (!ln) return
  if (el.closest('.gut')) return          // the line number is not a column
  const tx = ln.querySelector('.tx')
  const i = parseInt(ln.dataset.i, 10)
  // focusOffset is an offset inside ONE text node, and a highlighted line is
  // split into one node per token -- reporting it raw made the status bar's
  // column the offset within the clicked token. Accumulate to the line start.
  let col = 0
  if (tx && node) {
    const walker = document.createTreeWalker(tx, NodeFilter.SHOW_TEXT)
    let pos = 0, found = false
    for (let n = walker.nextNode(); n; n = walker.nextNode()) {
      if (n === node) { col = pos + sel.focusOffset; found = true; break }
      pos += n.nodeValue.length
    }
    if (!found) col = node.nodeType === 1 ? 0 : sel.focusOffset
  }
  post({ type: 'caret', line: i + 1, col: col + 1 })
}, 120))

document.addEventListener('contextmenu', (e) => {
  e.preventDefault()
  const sel = document.getSelection()
  // DEVICE pixels: clientX/Y are CSS pixels, but the host's client area (the
  // controller bounds it feeds to ClientToScreen) is physical. devicePixelRatio
  // carries both the monitor scale and the engine zoom, so without it the menu
  // pops up far from the cursor on any scaled display or non-100% zoom.
  const s = window.devicePixelRatio || 1
  post({
    type: 'contextMenu',
    x: Math.round(e.clientX * s),
    y: Math.round(e.clientY * s),
    hasSelection: !!(sel && String(sel).length),
  })
})

function throttle(fn, ms) {
  let t = 0, pending = false
  return (...a) => {
    const now = performance.now()
    if (now - t > ms) { t = now; fn(...a) }
    else if (!pending) {
      pending = true
      setTimeout(() => { pending = false; t = performance.now(); fn(...a) }, ms)
    }
  }
}

if (window.chrome && window.chrome.webview) {
  window.chrome.webview.addEventListener('message', onHostMessage)
}
post({ type: 'ready' })

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

let lines = []              // logical lines of the document
let lineHeight = 18         // measured
let firstRendered = -1      // first materialised line index
let lastRendered = -1
let tokens = []             // tokens[i] = packed triples for line i, or undefined
let styleClass = new Map()  // "colour|fontStyle" -> class name
let tokenSheet = null
let worker = null
let gen = 0
let lang = null
let showGutter = true
let gutterDigits = 1
let highlighting = false

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
  }
}

// ------------------------------------------------------------------- theming

function applyThemeColors(t) {
  if (!t) return
  const r = document.documentElement.style
  const c = t.colors || {}
  const set = (name, v) => { if (v) r.setProperty(name, v) }
  set('--bg', c['editor.background'] || t.bg)
  set('--fg', c['editor.foreground'] || t.fg)
  set('--gutter-fg', c['editorLineNumber.foreground'])
  set('--sel-bg', c['editor.selectionBackground'])
  set('--find-bg', c['editor.findMatchHighlightBackground'])
  set('--find-current-bg', c['editor.findMatchBackground'])
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

function buildLineText(i) {
  const frag = document.createDocumentFragment()
  const text = lines[i]
  const tk = tokens[i]
  if (!tk || tk.length === 0) {
    frag.appendChild(document.createTextNode(text))
    return frag
  }
  let pos = 0
  for (let k = 0; k < tk.length; k += 3) {
    const off = tk[k], len = tk[k + 1], key = tk[k + 2]
    if (off > pos) frag.appendChild(document.createTextNode(text.slice(pos, off)))
    const cls = classFor(key)
    const piece = text.substr(off, len)
    if (cls) {
      const span = document.createElement('span')
      span.className = cls
      span.textContent = piece            // file text: always textContent
      frag.appendChild(span)
    } else {
      frag.appendChild(document.createTextNode(piece))
    }
    pos = off + len
  }
  if (pos < text.length) frag.appendChild(document.createTextNode(text.slice(pos)))
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
  const probe = document.createElement('div')
  probe.className = 'ln'
  probe.textContent = 'X'
  linesEl.appendChild(probe)
  lineHeight = probe.getBoundingClientRect().height || 18
  probe.remove()
}

function render(force) {
  const top = scroller.scrollTop
  const height = scroller.clientHeight || 400
  let from = Math.max(0, Math.floor(top / lineHeight) - OVERSCAN)
  let to = Math.min(lines.length, Math.ceil((top + height) / lineHeight) + OVERSCAN)
  if (!force && from === firstRendered && to === lastRendered) return
  firstRendered = from
  lastRendered = to
  const frag = document.createDocumentFragment()
  for (let i = from; i < to; i++) frag.appendChild(makeLine(i))
  linesEl.replaceChildren(frag)
  linesEl.style.transform = `translateY(${from * lineHeight}px)`
  if (highlighting) {
    // Ask the worker for the window actually on screen (viewport-first).
    post0({ type: 'viewport', from, to, gen })
  }
  reapplyFindHighlights()
}

function post0(msg) { if (worker) worker.postMessage(msg) }

function layout() {
  sizer.style.height = (lines.length * lineHeight) + 'px'
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
  const myGen = gen
  applyView(m)
  const res = await fetch('text', { cache: 'no-store' })
  const text = await res.text()
  if (myGen !== gen) return
  lines = text.split('\n')
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
    type: 'load', lines, lang, theme: m.theme, maxLineLength: m.maxLineLength || 20000,
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
  if (m.fontFamily) r.setProperty('--font-family', m.fontFamily)
  if (m.fontSize) r.setProperty('--font-size', m.fontSize + 'px')
  if (typeof m.wrap === 'boolean') document.body.classList.toggle('wrap', m.wrap)
  if (typeof m.lineNumbers === 'boolean') {
    showGutter = m.lineNumbers
    document.body.classList.toggle('nogutter', !m.lineNumbers)
  }
  if (typeof m.showWhitespace === 'boolean') document.body.classList.toggle('ws', m.showWhitespace)
}

function setView(m) {
  applyView(m)
  measure()
  layout()
}

function setTheme(m) {
  if (!highlighting) { applyThemeColors(m.themeInfo || null); return }
  resetTokenClasses()
  tokens = new Array(lines.length)
  gen++
  post0({ type: 'retheme', theme: m.theme, from: firstRendered, to: lastRendered, gen })
  render(true)
}

function setLanguage(next) {
  lang = next || null
  resetTokenClasses()
  tokens = new Array(lines.length)
  gen++
  if (!lang) { highlighting = false; render(true); return }
  highlighting = true
  if (!worker) { startWorker({ theme: null }); return }
  post0({ type: 'relang', lang, from: firstRendered, to: lastRendered, gen })
  render(true)
}

// ---------------------------------------------------------------------- find

let findMatches = []     // {line, start, end}
let findCurrent = -1
let findTerm = ''

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
  if (findCurrent >= 0) revealLine(findMatches[findCurrent].line)
  reapplyFindHighlights()
}

function isWordChar(ch) { return /[A-Za-z0-9_]/.test(ch) }

function searchAll(term, caseSensitive, wholeWord) {
  const needle = caseSensitive ? term : term.toLowerCase()
  for (let i = 0; i < lines.length; i++) {
    const hay = caseSensitive ? lines[i] : lines[i].toLowerCase()
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
  const firstVisible = Math.floor(scroller.scrollTop / lineHeight)
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

// -------------------------------------------------------------- goto / scroll

function revealLine(i) {
  const target = i * lineHeight
  const top = scroller.scrollTop
  const h = scroller.clientHeight
  if (target < top || target > top + h - lineHeight) {
    scroller.scrollTop = Math.max(0, target - Math.floor(h / 2) + lineHeight)
    render(true)
  }
}

function gotoLine(line, col) {
  if (!lines.length) return
  const i = Math.min(Math.max(1, line), lines.length) - 1
  revealLine(i)
  render(true)
  const row = linesEl.querySelector(`.ln[data-i="${i}"]`)
  if (row) { row.classList.remove('flash'); void row.offsetWidth; row.classList.add('flash') }
  post({ type: 'caret', line: i + 1, col: Math.max(1, col || 1) })
}

// ------------------------------------------------------------------- wiring

scroller.addEventListener('scroll', () => render(false), { passive: true })
window.addEventListener('resize', () => { measure(); layout() })

document.addEventListener('selectionchange', throttle(() => {
  const sel = document.getSelection()
  if (!sel || sel.rangeCount === 0) return
  const node = sel.focusNode
  const row = node && (node.nodeType === 1 ? node : node.parentElement)
  const ln = row && row.closest ? row.closest('.ln') : null
  if (!ln) return
  const i = parseInt(ln.dataset.i, 10)
  post({ type: 'caret', line: i + 1, col: sel.focusOffset + 1 })
}, 120))

document.addEventListener('contextmenu', (e) => {
  e.preventDefault()
  const sel = document.getSelection()
  post({ type: 'contextMenu', x: e.clientX, y: e.clientY, hasSelection: !!(sel && String(sel).length) })
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

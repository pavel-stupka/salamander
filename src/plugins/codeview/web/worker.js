// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// codeview tokenizer worker.
//
// Why a worker at all: tokenizing costs ~7 ms/KB (spike-results.md S1), so a
// single 500-line chunk can take 170 ms. On the UI thread that is a visible
// stutter while scrolling; here it is invisible.
//
// Strategy (research D6, revised by the T013 spike):
//   * the visible window is tokenized FIRST and answered immediately;
//   * the rest of the file is swept in small chunks that RESUME the previous
//     chunk's grammar state, so no chunk re-reads the file from the top;
//   * a new viewport request pre-empts the sweep.
//
// Only the plugin's own bundled code runs here; the file's text is data.

import { createHighlighterCore, getLastGrammarState } from './shiki/engine.js'
import { createOnigurumaEngine, wasmBinary } from './shiki/engine.js'

const CHUNK = 128            // lines per sweep chunk (~30-60 ms, spike S3)
const CHECKPOINT = CHUNK     // grammar state is stored at every chunk boundary

let highlighter = null
let enginePromise = null
let lines = []               // the document, split into logical lines
let lang = null              // active language id, or null for plain
let theme = null             // active theme id
let maxLineLength = 20000
let states = []              // states[i] = grammar state BEFORE line i*CHUNK
let done = []                // done[chunkIndex] = true once emitted AUTHORITATIVELY
let cold = []                // cold[chunkIndex] = already answered without the
                             // predecessor state; not authoritative, but it must
                             // not be recomputed on every scroll frame either
let sweepTimer = 0
let generation = 0           // bumped on every reset; stale results are dropped

async function engine() {
  if (!enginePromise) {
    enginePromise = createOnigurumaEngine(wasmBinary)
  }
  return enginePromise
}

// TRUE only when tokenization can actually run: the highlighter exists and
// the ACTIVE lang and theme are loaded. A 'viewport' message that interleaves
// with a pending load/retheme/relang (their ensureHighlighter await yields to
// the event loop) must not reach tokenizeRange: it would fail, yet mark its
// chunks done[] -- permanently untokenized. The pending control handler
// re-runs the viewport itself once loading finishes.
function cvReady() {
  if (!highlighter || !lang) return false
  if (!highlighter.getLoadedLanguages().includes(lang)) return false
  if (theme && !highlighter.getLoadedThemes().includes(theme)) return false
  return true
}

// Creation and every module load are memoized by their in-flight promise.
// Without that, two control messages arriving before the first one resolves
// (a cold start takes hundreds of ms, and F9/Ctrl+PgDn are one keystroke away)
// each start their own createHighlighterCore and the LAST to resolve wins:
// the survivor lacks the live lang/theme, cvReady() then answers false for
// ever and the document stays plain with no error anywhere.
let highlighterPromise = null
const langLoads = new Map()
const themeLoads = new Map()

async function ensureHighlighter(nextLang, nextTheme) {
  if (!highlighter) {
    if (!highlighterPromise) {
      highlighterPromise = (async () => await createHighlighterCore({
        langs: nextLang ? [import(`./shiki/langs/${nextLang}.mjs`)] : [],
        themes: nextTheme ? [import(`./shiki/themes/${nextTheme}.mjs`)] : [],
        engine: await engine(),
      }))()
      // A failed creation must not poison every later attempt.
      highlighterPromise.catch(() => { highlighterPromise = null })
    }
    highlighter = await highlighterPromise
    // Fall through: a caller that joined someone else's creation still has to
    // make sure ITS language and theme are loaded.
  }
  if (nextLang && !highlighter.getLoadedLanguages().includes(nextLang)) {
    let p = langLoads.get(nextLang)
    if (!p) {
      p = highlighter.loadLanguage(import(`./shiki/langs/${nextLang}.mjs`))
      langLoads.set(nextLang, p)
      p.catch(() => {}).then(() => langLoads.delete(nextLang))
    }
    await p
  }
  if (nextTheme && !highlighter.getLoadedThemes().includes(nextTheme)) {
    let p = themeLoads.get(nextTheme)
    if (!p) {
      p = highlighter.loadTheme(import(`./shiki/themes/${nextTheme}.mjs`))
      themeLoads.set(nextTheme, p)
      p.catch(() => {}).then(() => themeLoads.delete(nextTheme))
    }
    await p
  }
}

// Shiki returns a token per style run with an absolute offset inside the chunk.
// We hand the main thread compact triples [offsetInLine, length, styleKey] so it
// can slice its own line text -- the text itself never travels twice.
function packChunk(result, firstLine) {
  const out = []
  for (let i = 0; i < result.tokens.length; i++) {
    const toks = result.tokens[i]
    const packed = []
    let col = 0
    for (const t of toks) {
      const len = t.content.length
      if (len > 0) packed.push(col, len, styleKey(t))
      col += len
    }
    out.push(packed)
  }
  return { firstLine, tokens: out }
}

function styleKey(t) {
  // "colour|fontStyle" -- the main thread turns distinct keys into CSS classes.
  const fs = t.fontStyle || 0
  return (t.color || '') + (fs ? '|' + fs : '')
}

function tokenizeRange(from, to) {
  // from/to are line indices; returns packed tokens or null when the range
  // cannot be tokenized (no language, or a line over the length gate).
  if (!lang || !highlighter) return null
  const slice = lines.slice(from, to)
  for (const l of slice) if (l.length > maxLineLength) return null
  const opts = { lang, theme }
  const chunkIndex = Math.floor(from / CHUNK)
  const state = from > 0 ? states[chunkIndex] : undefined
  const o = state ? { ...opts, grammarState: state } : opts
  let res
  try {
    res = highlighter.codeToTokens(slice.join('\n'), o)
  } catch (e) {
    // A resumed state can be rejected if the theme changed underneath; retry
    // cold rather than failing the whole document.
    try {
      res = highlighter.codeToTokens(slice.join('\n'), opts)
    } catch (e2) {
      return null
    }
  }
  // Record the state for the NEXT chunk boundary so the sweep can continue --
  // but ONLY when this chunk itself resumed a valid state. A checkpoint taken
  // after a cold viewport tokenization (see onViewport) would hand the sweep a
  // wrong state and its error would then propagate down the whole file.
  const resumed = from === 0 || state !== undefined;
  if (resumed && to % CHECKPOINT === 0) {
    try {
      const s = highlighter.getLastGrammarState(slice.join('\n'), o)
      states[chunkIndex + 1] = s
    } catch (e) { /* checkpoint is an optimisation, never fatal */ }
  }
  return packChunk(res, from)
}

function emit(range, gen) {
  if (gen !== generation || !range) return
  postMessage({ type: 'tokens', gen, ...range })
}

function sweepStep() {
  sweepTimer = 0
  if (!cvReady()) return
  const gen = generation
  // The first chunk whose state is known and which has not been emitted.
  for (let c = 0; c * CHUNK < lines.length; c++) {
    if (done[c]) continue
    const from = c * CHUNK
    // Wait for the predecessor -- but only while it is still pending. If it
    // has been emitted and still left no checkpoint (getLastGrammarState can
    // throw), waiting is forever: the sweep would stall here and report
    // sweepDone with the rest of the file permanently plain.
    if (from > 0 && states[c] === undefined && !done[c - 1])
      break
    const packed = tokenizeRange(from, Math.min(from + CHUNK, lines.length))
    done[c] = true
    emit(packed, gen)
    scheduleSweep()
    return
  }
  postMessage({ type: 'sweepDone', gen })
}

function scheduleSweep() {
  if (sweepTimer || !lang) return
  sweepTimer = setTimeout(sweepStep, 0)
}

async function onViewport(from, to, gen) {
  if (gen !== generation || !cvReady()) return
  from = Math.max(0, from)
  const firstChunk = Math.floor(from / CHUNK)
  const lastChunk = Math.floor((to - 1) / CHUNK)
  for (let c = firstChunk; c <= lastChunk; c++) {
    if (done[c]) continue
    const start = c * CHUNK
    const isCold = start > 0 && states[c] === undefined
    // A chunk the sweep has not reached yet is tokenized WITHOUT the previous
    // chunk's grammar state: colours are right except for constructs that
    // began above (a long block comment, a here-doc). It is emitted but NOT
    // marked done, so the sequential sweep re-emits it authoritatively later.
    // Before this, the visible window simply stayed plain until the sweep had
    // walked the whole file to it -- the opposite of the viewport-first
    // promise in this module's header and research D6.
    // Answering it ONCE is the point: without the cold[] mark, every scroll
    // event covering the same chunk re-ran a 128-line tokenization that the
    // page already has.
    if (isCold && cold[c])
      continue
    const packed = tokenizeRange(start, Math.min(start + CHUNK, lines.length))
    if (isCold)
      cold[c] = true
    else
      done[c] = true
    emit(packed, gen)
  }
  scheduleSweep()
}

onmessage = async (e) => {
  const m = e.data
  try {
    if (m.type === 'load') {
      // The PAGE owns the generation counter; the worker adopts it. Two
      // independent counters drift apart (plain init terminates the worker, a
      // later one starts at 1 again) and every emitted batch is then dropped
      // by the page's gen check.
      generation = typeof m.gen === 'number' ? m.gen : generation + 1
      const gen = generation
      lines = m.lines
      lang = m.lang || null
      theme = m.theme || null
      maxLineLength = m.maxLineLength || 20000
      states = []
      done = []
      cold = []
      if (sweepTimer) { clearTimeout(sweepTimer); sweepTimer = 0 }
      if (!lang) { postMessage({ type: 'sweepDone', gen }); return }
      await ensureHighlighter(lang, theme)
      if (gen !== generation) return
      postMessage({ type: 'ready', gen, theme: themeInfo() })
      // The visible window first, then the background sweep.
      await onViewport(m.from | 0, Math.max(m.to | 0, (m.from | 0) + 1), gen)
    } else if (m.type === 'viewport') {
      await onViewport(m.from | 0, m.to | 0, m.gen)
    } else if (m.type === 'retheme') {
      generation = typeof m.gen === 'number' ? m.gen : generation + 1
      const gen = generation
      theme = m.theme
      states = []
      done = []
      cold = []
      await ensureHighlighter(lang, theme)
      if (gen !== generation) return
      postMessage({ type: 'ready', gen, theme: themeInfo() })
      await onViewport(m.from | 0, m.to | 0, gen)
    } else if (m.type === 'relang') {
      generation = typeof m.gen === 'number' ? m.gen : generation + 1
      const gen = generation
      lang = m.lang || null
      states = []
      done = []
      cold = []
      if (!lang) { postMessage({ type: 'sweepDone', gen }); return }
      await ensureHighlighter(lang, theme)
      if (gen !== generation) return
      await onViewport(m.from | 0, m.to | 0, gen)
    }
  } catch (err) {
    postMessage({ type: 'failed', gen: generation, message: String(err && err.message || err) })
  }
}

function themeInfo() {
  if (!highlighter || !theme) return null
  try {
    const t = highlighter.getTheme(theme)
    return { name: t.name, type: t.type, bg: t.bg, fg: t.fg, colors: t.colors || {} }
  } catch (e) {
    return null
  }
}

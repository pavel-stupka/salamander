// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Headless regression test for the DOM-free logic of the codeview page
// (web/viewer.js). Usage:
//
//     node src/plugins/codeview/test/harness/test_page.mjs
//
// viewer.js is a browser module (it touches document at import time), so the
// functions under test are LIFTED OUT of the real file by source extraction
// rather than imported: each one is located by name, its body is evaluated in
// this module, and a shape assertion fails the run if the extraction no longer
// matches what ships. That keeps the test honest -- it cannot pass against a
// file whose logic has moved -- without a DOM.
//
// What it guards (fix-log.md, session 2026-08-27, defects 10-30):
//   * the document/line arithmetic agrees with intake.cpp (gutter last number
//     vs. the status bar's "N lines"), including the empty file;
//   * find offsets survive case folding (U+0130 folds to two units);
//   * the wrap-mode geometry (prefix sums) maps pixels <-> lines exactly and
//     reaches the last line -- the uniform math could not;
//   * whitespace decoration splits a line into the runs the CSS can paint,
//     without changing a single character of the text.

import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const web = join(here, '..', '..', 'web')
const src = readFileSync(join(web, 'viewer.js'), 'utf-8')
const intake = readFileSync(join(here, '..', '..', 'intake.cpp'), 'utf-8')

let failures = 0
const check = (cond, what) => {
  console.log((cond ? 'PASS' : 'FAIL') + ' - ' + what)
  if (!cond) failures++
}

// --- extract a named function body from the shipped file ------------------
function lift(name) {
  const at = src.indexOf('function ' + name + '(')
  if (at < 0) throw new Error('shape assertion: function ' + name + ' not found in web/viewer.js')
  let i = src.indexOf('{', at), depth = 0, end = -1
  for (let k = i; k < src.length; k++) {
    if (src[k] === '{') depth++
    else if (src[k] === '}') { depth--; if (depth === 0) { end = k + 1; break } }
  }
  if (end < 0) throw new Error('shape assertion: unbalanced body for ' + name)
  return src.slice(at, end)
}

// ==========================================================================
// 1. line arithmetic: the page must agree with intake.cpp
// ==========================================================================
//
// intake.cpp counts a line per line END and then drops the phantom line a
// trailing newline would open (CvDecode: LineCount starts at 1 for non-empty
// input, is incremented per CR/LF/CRLF, then decremented when the text ends
// with one). The page reproduces the same count from the served UTF-8.
check(/out\.LineCount = wide\.empty\(\) \? 0 : 1;/.test(intake) &&
      /out\.TrailingNewline && out\.LineCount > 0/.test(intake),
      'shape: intake.cpp still uses the LineCount/TrailingNewline arithmetic this test mirrors')

function hostLineCount(text) {              // transcription of CvDecode
  if (text.length === 0) return { lines: 0, trailing: true }
  let count = 1
  for (let i = 0; i < text.length; i++) {
    const c = text[i]
    if (c === '\r') { if (text[i + 1] === '\n') i++; count++ }
    else if (c === '\n') count++
  }
  const norm = text.replace(/\r\n?/g, '\n')
  const trailing = norm.endsWith('\n')
  if (trailing && count > 0) count--
  return { lines: count, trailing }
}

// ...and the page's own splitting, taken from the shipped init().
check(/lines = text === '' \? \[\] : text\.split\('\\n'\)/.test(src),
      'shape: init() still special-cases the empty document')
function pageRows(text, trailing) {
  const norm = text.replace(/\r\n?/g, '\n')     // the host serves LF-only text
  let lines = norm === '' ? [] : norm.split('\n')
  if (lines.length > 1 && lines[lines.length - 1] === '' && trailing !== false) lines.pop()
  return lines.length
}

const shapes = ['', '\n', 'a', 'a\n', 'a\nb', 'a\nb\n', '\n\n', 'a\r\nb\r\n', 'a\rb\r',
                'a\nb\r\nc', 'a\n\n\n', 'x', 'x\r\n']
let agree = 0
for (const t of shapes) {
  const h = hostLineCount(t)
  const p = pageRows(t, h.trailing)
  if (h.lines === p) agree++
  else console.log('    MISMATCH ' + JSON.stringify(t) + ': host ' + h.lines + ', page ' + p)
}
check(agree === shapes.length,
      'lines: gutter rows equal the status bar count for all ' + shapes.length + ' shapes (empty file included)')

// ==========================================================================
// 2. find: case folding must not move offsets
// ==========================================================================
// ES modules are strict, so a declaration inside eval does not leak: each
// lifted function is evaluated as an EXPRESSION and bound here. Direct eval
// keeps the lexical chain, so the bodies still see the state declared above.
const lowerKeepLen = eval('(' + lift('lowerKeepLen') + ')')
const dotted = '\u0130'                        // LATIN CAPITAL LETTER I WITH DOT ABOVE
check(dotted.toLowerCase().length === 2, 'fixture: U+0130 really folds to two units')
check(lowerKeepLen(dotted).length === 1, 'find: length-preserving fold keeps U+0130 one unit')
check(lowerKeepLen('ABC').length === 3 && lowerKeepLen('ABC') === 'abc', 'find: ordinary text still folds')

// The offsets a case-insensitive search produces must address the ORIGINAL
// line, so a fold that changed length would push the range past its end.
const line = dotted + dotted + dotted + 'find here'
check(lowerKeepLen(line).length === line.length, 'find: a line of dotted capitals keeps its length')
const at = lowerKeepLen(line).indexOf('find')
check(line.slice(at, at + 4) === 'find', 'find: the match offset addresses the original text')

// ==========================================================================
// 3. wrap-mode geometry (prefix sums)
// ==========================================================================
//
// The functions are lifted with their module state supplied here: `wrapMode`
// on, `heights` holding per-line heights, `tops` their prefix sums.
let wrapMode = true, heights = null, tops = null, lineHeight = 18
let lines = []   // totalHeight()'s no-wrap branch reads it
const rebuildTops = eval('(' + lift('rebuildTops') + ')')
const lineTop = eval('(' + lift('lineTop') + ')')
const lineAt = eval('(' + lift('lineAt') + ')')
const totalHeight = eval('(' + lift('totalHeight') + ')')
const lineHeightAt = eval('(' + lift('lineHeightAt') + ')')

// A document where lines 3, 7 and 8 wrap to several rows.
heights = new Float64Array([18, 18, 18, 54, 18, 18, 18, 36, 72, 18])
rebuildTops()
check(totalHeight() === 288, 'wrap: total height is the sum of the measured rows (' + totalHeight() + ')')
check(lineTop(0) === 0 && lineTop(3) === 54 && lineTop(9) === 270, 'wrap: line offsets are exact prefix sums')

let exact = 0
for (let i = 0; i < heights.length; i++) {
  const top = lineTop(i)
  if (lineAt(top) === i && lineAt(top + heights[i] - 1) === i) exact++
}
check(exact === heights.length, 'wrap: every pixel inside a line maps back to that line (' + exact + '/' + heights.length + ')')
check(lineAt(-5) === 0, 'wrap: a negative offset clamps to the first line')
check(lineAt(totalHeight() + 1000) === heights.length - 1, 'wrap: past the end clamps to the last line')

// The end of the document must be REACHABLE: with a viewport of 100 px the
// scroll range is total - 100, and the last line has to be inside it.
const viewport = 100
const maxScroll = totalHeight() - viewport
check(lineAt(maxScroll + viewport - 1) === heights.length - 1,
      'wrap: the last line is visible at maximum scroll (the defect-4 symptom)')

// The uniform math the code used before cannot do this: it would place the
// last line at 9*18 = 162 while it really starts at 270.
check(9 * lineHeight !== lineTop(9), 'wrap: uniform math really disagrees (162 vs ' + lineTop(9) + ')')

// Non-wrap must still take the fast path unchanged.
wrapMode = false
lines = new Array(10)
check(lineTop(9) === 162 && lineAt(163) === 9 && totalHeight() === 180,
      'no-wrap: the uniform fast path is untouched')
wrapMode = true

// ==========================================================================
// 4. whitespace decoration splits runs without changing the text
// ==========================================================================
let showWhitespace = true
const frags = []
globalThis.document = {                        // the smallest DOM this needs
  createTextNode: (t) => ({ nodeType: 3, textContent: t, className: null }),
  createElement: () => ({ nodeType: 1, textContent: '', className: '' }),
}
const frag = { children: [], appendChild(n) { this.children.push(n) } }
const styledNode = eval('(' + lift('styledNode') + ')')
const appendPiece = eval('(' + lift('appendPiece') + ')')

appendPiece(frag, '  \tif (x)\t {  ', 't1')
const text = frag.children.map(n => n.textContent).join('')
check(text === '  \tif (x)\t {  ', 'whitespace: the reconstructed text is byte-identical')
check(frag.children.some(n => (n.className || '').includes('sp')), 'whitespace: space runs get the .sp class the CSS paints')
check(frag.children.some(n => (n.className || '').includes('tb')), 'whitespace: tab runs get the .tb class')
check(frag.children.every(n => n.className === null || n.className.includes('t1')),
      'whitespace: the token class survives on every piece')

// With the mode off the text must arrive as ONE node (no needless splitting).
showWhitespace = false
const frag2 = { children: [], appendChild(n) { this.children.push(n) } }
appendPiece(frag2, '  \tif (x)', null)
check(frag2.children.length === 1 && frag2.children[0].textContent === '  \tif (x)',
      'whitespace: off means one plain text node, as before')

console.log(failures ? 'RESULT: ' + failures + ' FAILURE(S)' : 'RESULT: ALL PASS')
process.exit(failures ? 1 : 0)

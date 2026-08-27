// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Headless regression test for the codeview tokenizer worker (web/worker.js).
// Runs the REAL worker module in Node with postMessage/onmessage shims -- no
// browser, no WebView2, no build. Usage:
//
//     node src/plugins/codeview/test/harness/test_worker.mjs
//
// What it guards (fix-log.md, session 2026-08-27):
//   * the glsl licence stub registers a language -- an empty stub makes shiki
//     refuse cpp/cpp-macro/elm/nim entirely ("Missing languages `glsl`");
//   * the worker ADOPTS the page's generation counter (two independent
//     counters drift and every token batch is dropped by the page);
//   * a viewport message interleaved with a pending load/retheme (their
//     ensureHighlighter await yields) must not poison done[] -- the visible
//     window would stay plain after a scheme switch.

import { readFileSync } from 'node:fs'
import { fileURLToPath, pathToFileURL } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const web = join(here, '..', '..', 'web')
const out = []          // worker -> page messages
let failures = 0

globalThis.onmessage = null
globalThis.postMessage = (m) => { out.push(m) }

await import(pathToFileURL(join(web, 'worker.js')).href)
const send = (m) => globalThis.onmessage({ data: m })
const settle = (ms = 50) => new Promise(r => setTimeout(r, ms))
const check = (cond, what) => {
  console.log((cond ? 'PASS' : 'FAIL') + ' - ' + what)
  if (!cond) failures++
}
const waitFor = async (type, rounds = 400) => {
  for (let i = 0; i < rounds && !out.some(m => m.type === type || m.type === 'failed'); i++)
    await settle(50)
}

// A real C++ file from this plugin as the fixture.
const text = readFileSync(join(here, '..', '..', 'viewer.cpp'), 'utf-8')
const lines = text.split('\n')
console.log('fixture lines: ' + lines.length)

// --- 1. load: visible window + full sweep, page-owned generation ----------
send({ type: 'load', lines, lang: 'cpp', theme: 'dark-plus', maxLineLength: 20000, from: 0, to: 60, gen: 3 })
await waitFor('sweepDone')

const failed1 = out.find(m => m.type === 'failed')
check(!failed1, 'load: no failure' + (failed1 ? ' (' + failed1.message + ')' : ''))
check(out.some(m => m.type === 'ready' && m.theme && m.theme.type === 'dark'), 'load: ready with dark themeInfo')
const tok1 = out.filter(m => m.type === 'tokens')
check(tok1.length > 0, 'load: tokens emitted (' + tok1.length + ' batches)')
check(tok1.every(m => m.gen === 3), 'load: generation adopted from the page (gen 3)')
const covered = new Set()
for (const m of tok1) for (let i = 0; i < m.tokens.length; i++) covered.add(m.firstLine + i)
check(covered.size >= lines.length, 'load: full sweep covered every line (' + covered.size + '/' + lines.length + ')')
const anyColored = tok1.some(m => m.tokens.some(l => l.some((v, i) => i % 3 === 2 && /^#[0-9a-fA-F]{6}/.test(String(v)))))
check(anyColored, 'load: packed triples carry real colour keys')

// --- 2. retheme with an interleaved viewport (the race) -------------------
out.length = 0
// page: gen++ -> 4, posts retheme, then render(true) posts viewport(gen 4)
send({ type: 'retheme', theme: 'github-light', from: 0, to: 60, gen: 4 })
send({ type: 'viewport', from: 0, to: 60, gen: 4 })   // interloper during ensureHighlighter await
await waitFor('sweepDone')

check(out.some(m => m.type === 'ready' && m.theme && m.theme.type === 'light'), 'retheme: ready with light themeInfo')
const tok2 = out.filter(m => m.type === 'tokens')
check(tok2.length > 0 && tok2.every(m => m.gen === 4), 'retheme: gen 4 on every batch')
const covered2 = new Set()
for (const m of tok2) for (let i = 0; i < m.tokens.length; i++) covered2.add(m.firstLine + i)
check(covered2.has(0) && covered2.has(59), 'retheme: the VISIBLE window was re-tokenized despite the race')
check(covered2.size >= lines.length, 'retheme: full sweep re-covered every line (' + covered2.size + ')')
const key1 = tok1.flatMap(m => m.tokens.flat()).filter(v => typeof v === 'string')
const key2 = tok2.flatMap(m => m.tokens.flat()).filter(v => typeof v === 'string')
check(key2.length > 0 && key1[0] !== key2[0], 'retheme: colours actually changed (' + key1[0] + ' -> ' + key2[0] + ')')

// --- 3. worker (re)created while the page counter is already high ---------
out.length = 0
send({ type: 'load', lines: ['int x = 1;'], lang: 'c', theme: 'github-light', maxLineLength: 20000, from: 0, to: 1, gen: 9 })
await waitFor('sweepDone', 200)
check(out.some(m => m.type === 'tokens') && out.filter(m => m.type === 'tokens').every(m => m.gen === 9),
      'late worker: adopts page gen 9')

// --- 4. viewport BEYOND the sweep frontier is answered immediately --------
// The visible window used to stay plain until the strictly sequential sweep
// had walked the whole file to it (defect W2): a viewport whose predecessor
// grammar state was missing simply returned. It is now tokenized cold, emitted,
// and re-emitted authoritatively when the sweep arrives.
out.length = 0
const big = []
for (let i = 0; i < 4000; i++) big.push('int v' + i + ' = ' + i + '; // line ' + i)
send({ type: 'load', lines: big, lang: 'cpp', theme: 'dark-plus', maxLineLength: 20000, from: 0, to: 40, gen: 11 })
await settle(120)                                  // let the load settle, sweep still near the top
const mark = out.length                            // keep earlier batches: they count as coverage
send({ type: 'viewport', from: 3000, to: 3040, gen: 11 })
await settle(150)
const far = out.slice(mark).filter(m => m.type === 'tokens' && m.firstLine >= 2900 && m.firstLine <= 3100)
check(far.length > 0, 'far viewport: answered before the sweep reached it (' + far.length + ' batch(es))')
const sweptBefore = out.slice(mark).filter(m => m.type === 'tokens').every(m => m.firstLine < 2900)
check(!sweptBefore, 'far viewport: the answer came from the viewport, not from the sweep frontier')
await waitFor('sweepDone', 600)
const covered4 = new Set()
for (const m of out.filter(m => m.type === 'tokens'))
  for (let i = 0; i < m.tokens.length; i++) covered4.add(m.firstLine + i)
check(covered4.size >= big.length - 1, 'far viewport: the sweep still covered the whole file (' + covered4.size + '/' + big.length + ')')

// --- 5. two control messages racing the FIRST highlighter creation --------
// ensureHighlighter used to assign the shared highlighter after its await with
// no memoization (defect W1): two messages that both saw `highlighter === null`
// each built one, the LAST to resolve won, and if that was the stale one the
// document stayed plain for ever with no error. The creation promise is now
// memoized, so the second message joins the first and then loads its own
// language on top.
// HONEST LIMIT: unlike checks 4 and 6, this one does NOT fail against the
// pre-fix worker -- Node resolves the two creations in issue order, which is
// the benign interleaving. It is an INVARIANT test (the active language must
// survive a control message that arrives during a cold start), kept so a
// future change to the creation path cannot silently reintroduce the defect.
// A HEAVY grammar first and a light one second, so the stale creation is the
// one that resolves LAST -- the losing order the memoization has to prevent.
// The document is long enough that the sweep is still running when the stale
// creation lands: that is when the damage shows, because the replacement
// highlighter does not have the ACTIVE language loaded, cvReady() turns false
// and the sweep dies silently in mid-file.
out.length = 0
const race = []
for (let i = 0; i < 2500; i++) race.push('key' + i + ' = value' + i)
send({ type: 'load', lines: race, lang: 'cpp', theme: 'dark-plus', maxLineLength: 20000, from: 0, to: 30, gen: 21 })
send({ type: 'relang', lang: 'ini', from: 0, to: 30, gen: 22 })   // arrives during the await
await waitFor('sweepDone', 600)
await settle(400)                                   // let a late stale creation land
const failed5 = out.find(m => m.type === 'failed')
check(!failed5, 'creation race: no failure' + (failed5 ? ' (' + failed5.message + ')' : ''))
const tok5 = out.filter(m => m.type === 'tokens')
check(tok5.length > 0 && tok5.every(m => m.gen === 22), 'creation race: the SURVIVING generation is the newest (22)')
const covered5 = new Set()
for (const m of tok5) for (let i = 0; i < m.tokens.length; i++) covered5.add(m.firstLine + i)
check(covered5.size >= race.length - 1,
      'creation race: the sweep survived the stale creation (' + covered5.size + '/' + race.length + ')')

// --- 6. a checkpoint-less chunk must not stall the sweep ------------------
// sweepStep waited for states[c] unconditionally; if a checkpoint was ever
// missing the sweep stalled there and reported sweepDone with the rest of the
// file permanently plain. It now proceeds once the predecessor is emitted.
out.length = 0
const mid = []
for (let i = 0; i < 600; i++) mid.push('const a' + i + ' = ' + i)
send({ type: 'load', lines: mid, lang: 'javascript', theme: 'dark-plus', maxLineLength: 20000, from: 0, to: 30, gen: 31 })
await waitFor('sweepDone', 400)
const covered6 = new Set()
for (const m of out.filter(m => m.type === 'tokens'))
  for (let i = 0; i < m.tokens.length; i++) covered6.add(m.firstLine + i)
check(covered6.size >= mid.length - 1, 'sweep: every chunk reached (' + covered6.size + '/' + mid.length + ')')

console.log(failures ? 'RESULT: ' + failures + ' FAILURE(S)' : 'RESULT: ALL PASS')
process.exit(failures ? 1 : 0)

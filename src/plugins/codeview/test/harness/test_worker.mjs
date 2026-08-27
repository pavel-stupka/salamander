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

console.log(failures ? 'RESULT: ' + failures + ' FAILURE(S)' : 'RESULT: ALL PASS')
process.exit(failures ? 1 : 0)

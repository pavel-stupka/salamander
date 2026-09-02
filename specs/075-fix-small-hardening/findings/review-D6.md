# Review — D6 · `src/plugins/codeview/test/run_tests.cmd`

**Reviewer**: independent agent; did not write the fix.
**Charter**: find a regression, not approve one.
**Diff reviewed**: `git diff -- src/plugins/codeview/test/run_tests.cmd` on
branch `075-fix-small-hardening` (base `c554f4d`) — 8 insertions / 1 deletion,
diff md5 `637fbbb8e1351a19f1478a8d1e99410b`.
**Protocol**: `contracts/fix-protocol.md` Parts B and C (C13); spec FR-007,
FR-009; research R6; plan Design D6.

The change is one flag on one `node` invocation, plus a six-line header
paragraph.

---

## The flag's factual claims

Verified against the environment and against Node's own release notes:

| Claim in the new header | Verdict |
|---|---|
| "Needs Node >= 20.10 … `--experimental-detect-module`" | **correct** — the flag was introduced in Node **v20.10.0** ("Node.js can now detect ES module syntax in ambiguous files"; ambiguous = `.js` or extensionless with no `package.json` `type` field, which is exactly `web/worker.js`) |
| "Node treats a bare .js file as CommonJS until 22.7 and refuses it" | **correct** — module syntax detection became the default in **v22.7.0** |
| "(it is the default from **22.12** and accepted before that)" | **inaccurate, comment only** — see below |
| the flag is accepted where it is already the default | **verified on this machine**: `node --version` → `v24.19.0`; `node --experimental-detect-module -e "…"` → runs, exit 0 |

**The one factual error**: detection has been on by default since **22.7.0**,
not 22.12. Node **22.12.0**'s release note marks `require(esm)` stable
(`--experimental-require-module` unflagged) and says nothing about
`--experimental-detect-module`. The header's own preceding sentence already
says "until 22.7", so the two clauses contradict each other. Research R6 words
it correctly ("on by default from 22.7, stable in 22.12"); the compression into
the runner header lost that. This is a comment in a developer-only script, it
changes no behaviour and no verdict, and the actionable number — the Node floor
of 20.10 — is right. **Recorded as a required wording correction, not as a
regression**; suggested replacement for the parenthesis: *(it has been the
default since 22.7 and is accepted from 20.10 on)*.

## Does the change silence any check? **No — proven, not argued.**

I ran the planted-defect experiment myself rather than trusting the fix log's.

1. Baseline, current tree: `run_tests.cmd` → ` RESULT: all codeview checks passed`,
   process exit **0**.
2. Planted defect — appended `this is not valid javascript (((` to
   `src/plugins/codeview/web/worker.js`
   (SHA256 before `8572456D…F2AF`, after `1B8C9F11…D985`), then
   `run_tests.cmd`:

   ```
   SyntaxError: Unexpected identifier 'is'
   RESULT: ALL PASS                 <- test_page.mjs, which reads viewer.js as text
    RESULT: FAILURES -- see above
   ```

   runner exit **1**.
3. **Restored**: `git checkout -- src/plugins/codeview/web/worker.js`; SHA256
   back to `8572456D9D0BCF91B5F8E5ED69C3687E768482D60B1948DEB45454946C2FF2AF`
   — byte-identical to the pre-experiment file. `git status --porcelain
   src/plugins/codeview/` afterwards lists **only**
   `M src/plugins/codeview/test/run_tests.cmd`. Re-ran the runner: exit 0,
   `all codeview checks passed`. **The temporary edit is fully reverted.**

So the flag changes how the module is *loaded*, not what is *checked*: a real
defect in the worker still turns the runner red and still yields exit 1, which
is what `if errorlevel 1 set FAILED=1` consumes.

I also confirmed the failing path propagates its exit code correctly, since the
whole value of the fix depends on it:
`node --no-experimental-detect-module …\test_worker.mjs` → exit **1**
(`ERR_REQUIRE_CYCLE_MODULE: Cannot require() ES Module …\web\worker.js`),
`node --experimental-detect-module …` → exit **0**. That inversion reproduces
the Node-20 failure class on Node 24 and is a genuine fail-first for the
mechanism.

## B4 · Identity on Node 22+

*"With Node 22+: identical verdict and identical harness output."*

Ran the worker harness twice on this machine, pre-fix invocation (bare `node`)
and post-fix invocation (with the flag), capturing stdout+stderr to files:

```
pre  exit=0   SHA256 0888D7175036490230F8C75FF9411A4768F35FFE9AD94A2287B71538E2FB8230
post exit=0   SHA256 0888D7175036490230F8C75FF9411A4768F35FFE9AD94A2287B71538E2FB8230
```

**Byte-identical output, identical exit code.** On a runtime where detection is
already the default the flag is a no-op, as claimed.

## C13 · Nothing under `web/` changed; the data harness is unaffected

* `git status --porcelain src/plugins/codeview/` → `M src/plugins/codeview/test/run_tests.cmd`
  and nothing else, both before and after my planted-defect experiment.
* `python src\plugins\codeview\test\check_data.py` → `All data checks passed.`,
  exit 0, including `every asset named in the table exists on disk` and the
  `IDR_WEB_FIRST` id rules — identical to the baseline recorded in `fix-log.md`
  T003.
* No new file is shipped: the remedy adds **nothing** to `web/`, which is what
  FR-007's "any remedy that adds a file under `web/` must be validated against
  the resource-table rule" was guarding against. The rejected alternatives
  (`web/package.json`, renaming to `worker.mjs`) would both have touched the
  shipped tree; neither was taken.

`run_tests.cmd` itself is a test-side script — no packaging list, no
`.github/workflows/*` and no `.vcxproj` references it (own sweep: the only
non-spec reference to `run_tests.cmd` in the repository is the file itself).
So the change cannot reach the installer or the plugin binary.

## Is `test_page.mjs` correctly left alone? **Yes.**

`test_page.mjs` has three `import` statements and they are all `node:` builtins
(`node:fs`, `node:url`, `node:path`). It obtains `web/viewer.js` with
`readFileSync(join(web, 'viewer.js'), 'utf-8')` (`:31`) and lifts named
function bodies out of that **text** — its own header explains why ("viewer.js
is a browser module (it touches document at import time) … LIFTED OUT … by
source extraction rather than imported"). It never hands a `.js` path to the
module loader, so module-kind detection cannot apply to it and the flag would
be dead weight there. Leaving that line bare is correct, not an oversight.

By contrast `test_worker.mjs:31` does
`await import(pathToFileURL(join(web, 'worker.js')).href)` — the one place the
loader sees an ambiguous `.js` — and that is precisely the line that got the
flag. The two harnesses are treated according to what they actually do.

## B2 / B5 · Per-surface verdict and failure paths

| Surface | Verdict |
|---|---|
| Node 22.7+ / 24 (this machine) | **unchanged** — byte-identical output, exit 0 |
| Node 20.10 … 22.6 | **corrected** (by design; not runnable here — see the gap below) |
| Node < 20.10 | **still fails**, with a different message: `bad option: --experimental-detect-module` instead of the ESM/CJS error. Both are red; the plan records this trade-off explicitly and the header now states the floor, which is the mitigation |
| Node absent | fails before and after — out of scope per the spec's edge cases |
| data harness (`check_data.py`) line | **unchanged** — not in the diff |
| page harness (`test_page.mjs`) line | **unchanged** — not in the diff, correctly |
| `RESULT:` aggregation and exit codes | **unchanged** — `FAILED`/`errorlevel` logic not touched |

## Evidence gap, recorded openly (not a reason to reject)

Neither the fixer nor I could run a real **Node 20**: `nvm`, `fnm` and `volta`
are all absent from this machine and `npx --yes node@20.19.0 --version`
produced no usable binary (same outcome the fix log records). So SC-005's
"reports *all codeview checks passed* on Node 20" is **still unverified** — it
is asserted from Node's documented semantics (detection applies to ambiguous
`.js` files, `web/worker.js` is one) plus the inverted-flag reproduction, not
demonstrated.

That gap was pre-authorised: research R6 calls a genuine Node 20 run "a
should-do, not a gate", quickstart S6 carries it as a human step, and
`fix-log.md` records it as *not performed* rather than claiming it. It is an
open item for the feature's success criteria, not a defect in this diff — the
diff cannot make Node 20 worse than the status quo (today Node 20 fails
outright), so the worst case of the assertion being wrong is *unchanged*, not
regressed.

## B7 · Earlier scenarios touched

None. Feature 074's gutter checks and the 070 worker checks all run through the
same two harnesses and passed unchanged in my full runs (the tail of the page
harness still reports every `gutter:` check).

## B8 · Per-item path

None.

## Conclusion

One flag, on the one invocation that actually imports an ambiguous `.js`, with
the other two harness lines correctly left alone. It provably does not silence
anything (a broken `worker.js` still produces `RESULT: FAILURES` and exit 1),
its output on this machine's Node 24 is byte-identical to the pre-fix
invocation, nothing under `web/` moved, and `check_data.py` is unchanged. The
only defect I found is a factual slip inside the new header comment — detection
became the default in **22.7**, not 22.12 — which contradicts the same
paragraph's earlier sentence and should be corrected before the commit lands,
but it changes no behaviour, no verdict and no shipped file.

**VERDICT: ACCEPTED** (with one required comment correction: "the default from
22.12" → "the default since 22.7"; and the Node 20 run of SC-005 remains open,
as the record already states)

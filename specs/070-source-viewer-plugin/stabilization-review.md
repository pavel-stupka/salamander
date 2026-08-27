# Stabilization Review — Feature 070 (Code Viewer)

**Session 2026-08-27 (third round).** After nine defects had been found by
simply *using* the plugin (`fix-log.md`, defects 1–9), the last of which was a
misread API parameter that made Ctrl+PgUp navigate forward, the goal changed
from "fix what was reported" to **"find the rest before the user does"**.

This file records the review: how it was run, what it found, what was fixed,
what was proven by test, and what is deliberately left.

> **Everything below the "Fixes" heading is code that is already in the tree.**
> The remaining GUI verification is listed at the end and is the only part a
> human still has to do.

---

## 1. Method

Twelve independent agents, none of which could see another's work, in two
stages:

| Stage | Agents | What they were given |
|---|---|---|
| **Find** | 8 reviewers, one per dimension | the code, the contracts (`spl_gen.h`/`spl_base.h`/`spl_menu.h` — the Czech comments *are* the specification), the feature's own spec and contracts, the reference plugins (`demoview`, `mdview`) and the built-in viewer, plus `fix-log.md` with the instruction **not** to re-report the nine known defects but to hunt more of their *classes* |
| **Verify** | 8 adversarial verifiers | each finding, with the instruction to **refute** it: CONFIRMED only when the verifier independently re-derived the defect from the code and could state the concrete user-visible failure; "default to REFUTED on weak evidence" |

The eight dimensions: plugin-API contracts · Win32 window/menu/accelerators ·
page controller logic · tokenizer worker · file intake and decoding · the
host↔page glue and the shared WebView2 host · configuration and process
lifecycle · a hands-on dynamic tester that only reported what it could *run*.

Two dimensions overlapped on purpose, and the overlap paid: the dead Copy /
Select All menu items were found independently by two reviewers, the
use-after-free by two, the stale find state by three. A finding raised by two
reviewers who could not see each other is much stronger evidence than one
raised twice by the same one.

**Cost and honesty of the process.** 32 agent runs, ≈5.5 M tokens, 1,480 tool
calls (12 finding + verifying, 5 accepting, 3 re-accepting after the
corrections). The first workflow lost 8 of its 12 agents to a session limit; those
dimensions were re-run in a second workflow rather than dropped. Of the 38
findings raised, **38 survived verification** — a 0 % refutation rate, which is
itself worth flagging: it means the verifiers were not adversarial enough to
be treated as an independent gate. Every finding was therefore re-checked
against the code before it was fixed, and **two were corrected in the process**
(see §4). The acceptance pass in §6 exists for the same reason.

---

## 2. What was found

**37 unique confirmed defects** (38 findings; two dimensions reported the same
cross-window defect). Severity is the verifier's, after re-assessment.

### High — a shipped feature does not work at all

| # | Where | Defect |
|---|---|---|
| F01 | `viewer.cpp` Edit menu + context menu | **Copy and Select All were dead.** Both commands were appended to two menus but had no `WM_COMMAND` handler and no host→page channel — clicking them did nothing. Only the engine's own Ctrl+C/Ctrl+A appeared to work, and those cover just the ~200 materialised rows of the virtual list, so even they could not satisfy FR-021 on a long file. |
| F05 | `web/viewer.js` | **Word Wrap broke the whole virtual list.** Every offset, the sizer height and the scroll range assumed one visual row per logical line; with wrap on, a wrapped line is several rows tall, so the scrollbar lied, blank bands appeared and the end of the file became unreachable — the exact symptom of defect 4, reintroduced whenever wrap was on. |
| F10 | `web/viewer.js` + `viewer.css` | **Show Whitespace was a no-op.** The stylesheet painted dots on `.sp` elements; the page never created any. The menu item toggled a class nothing could match. |
| F16 | `intake.cpp` | The ANSI decode path is hardwired to `CP_ACP`; the code-table machinery FR-024 calls for (ISO-8859-2, CP852, …) is absent, so those files cannot be read correctly and F8 offers 5 fixed encodings instead of the built-in viewer's table. **Deferred — see §7.** |
| R14 | `langmap.cpp` (generated) | **The language shown for most common file types was the name of a different, obscure language**: `.py` → "Easybuild", `.js` → "Cycript", `.cs` → "Beef", `.java` → "ChucK", `.json` → "Ecere Projects", `.xml` → "COLLADA", `.sh` → "Alpine Abuild", `.yml` → "BuildStream", `.rb` → "Mirah". Highlighting was correct; the *label* was not. 327 of 773 extension patterns were affected. This one became far more visible the moment the previous session put the language in the window title. |

### Medium — wrong behaviour in ordinary use

| # | Where | Defect |
|---|---|---|
| F02 | `viewer.cpp` / `webhost.cpp` | Closing the window during a cold engine start freed the host while WebView2's creation completion was still queued on the same thread → use-after-free. `mdview`, the reference this host was lifted from, does not do this: it deletes the host after the message loop drains. |
| F03/F04 | `web/viewer.js` | Ctrl+PgDn carried the previous file's find matches (painted at its line/column positions, and F3 stepped through them because the term had not changed) and its scroll offset into the new file — while the host had already reset its own counters. |
| F06 | `web/viewer.js` | The status bar's column was the offset inside the clicked *token*, not the line: `focusOffset` is relative to one text node and a highlighted line is one node per style run. |
| F07 | `web/viewer.js` / `viewer.cpp` | The context menu opened far from the cursor on any scaled display or non-100 % zoom: the page sent CSS pixels, the host fed them to `ClientToScreen`, which is physical. |
| F08 | `webglue.cpp` | Ctrl+Plus / Ctrl+Minus did nothing. They are *browser accelerators*, which the shared lockdown disables, and the frame's accelerator table never sees them because focus lives inside the WebView — the same reasoning the code already applied to Ctrl+0. |
| F09 | `viewer.cpp` | FR-029's "open in built-in viewer" action was never implemented (`CM_OPEN_BUILTIN` was reserved and unused), and a declined file **blocked panel navigation for good**: the enumeration anchor did not advance, so every further Ctrl+PgDn re-offered the same file. |
| F11 | `web/viewer.js` | Pressing F9 while a file was still loading abandoned the load and left the window empty: the fetch guard compared the *token* generation, which a scheme switch also bumps. |
| F12 | `web/viewer.js` | A theme that did not define a colour left the previous theme's value in place — a light scheme could keep a dark scheme's selection colour. |
| F13 | `web/viewer.js` | A match far to the right of a long line was scrolled onto the right row and stayed off-screen: reveal was vertical only. |
| F15 | `web/viewer.js` | Case-insensitive search computed offsets on the folded line and applied them to the original. Exactly one BMP code point (U+0130) folds to two units, and it pushed every later highlight on that line — past the end of the line for some. |
| F17 | `viewer.cpp` | A failed encoding re-read (F8) left the window with an **empty** intake over content it was still showing: `CvLoadFile` resets its output before doing anything. |
| R01/R06 | `viewer.cpp` | Wrap / Line Numbers / Show Whitespace / scheme are process-wide settings applied only to the window that issued them. With two windows open the second showed the old state with a check mark that disagreed with the global — so its next toggle flipped the global back and appeared to do nothing. |
| R04 | `web/worker.js` | `ensureHighlighter` was not re-entrancy-safe: two control messages during a cold start each built a highlighter and the last to resolve won. When that was the stale one, the survivor lacked the active language, `cvReady()` answered false for ever and the document stayed plain with no error. |
| R05 | `web/worker.js` | The viewport never pre-empted the sweep: scrolling to line 20 000 of a 0.85 MB file left the screen the user was looking at plain until the strictly sequential sweep had walked the whole file to it — the opposite of the module's own header and research D6. |
| R09 | `config.cpp` | The font family had two disagreeing encodings (ANSI dialog in, UTF-8 to the page out): a non-ASCII family name could never match. |
| R10 | `web/viewer.js` | The configured family *replaced* the fallback stack, so a typo or a font missing on another machine dropped the code view to a proportional font. |
| R15 | `langmap.cpp` (generated) | `*.php` was tokenized with the **Hack** grammar and labelled "Hack". |

### Low — edge cases, dead code, honesty of the harness

F14 (empty file: gutter row "1" against a status bar saying 0 lines) ·
F18 (`IDS_LOAD_ERROR` unreachable — an unreadable file was reported as binary) ·
F19 (forcing "UTF-8 with BOM" on a file without one ate its first three bytes) ·
F20 (a name with any extra dot lost its compound suffix: 25 of the 26 shipped
suffixes were unreachable) · F21 (an aborted tokenizer said nothing to the
user) · R02 (Next/Previous File permanently enabled although a file opened from
an archive has no enumeration source) · R03 (`SetEvent` on a lock handle the
thread had already closed) · R07 (a second window reported the first window's
zoom over text it was not rendering at) · R08 ("Restore Default File Types"
committed immediately and survived Cancel) · R11 (dialog clamps disagreed with
the loader's, and reset to the factory default instead of the nearest legal
value) · R12 (setting the font size back to 0 did nothing to open windows) ·
R13 (the engine-unavailable fallback reported success unconditionally) ·
R16 (the RISC-V probe was dead code — `lang("riscv")` named a language row that
did not exist) · R17 (**the data harness's SC-001 gate was vacuous**: it
asserted against a generator counter that said 256 while the shipped table had
193 grammar-backed rows, so it could not fail on the regression it exists to
catch).

---

## 3. Fixes

**35 of the 37 defects are fixed.** Grouped by where the work landed:

**Shared WebView2 host** (`src/common/webhost/webhost.cpp`) — the async
creation callbacks now carry a liveness token (a `shared_ptr<bool>` they own a
copy of, cleared by `Destroy()`), so a completion that lands after the host is
gone is a no-op instead of a write into freed memory. Every future consumer of
the shared host inherits the guard; `mdview` still uses its own copy and is
untouched.

**Native viewer** (`viewer.cpp/.h`, `codeview.cpp/.h`, `config.cpp`) — Copy and
Select All implemented end to end (the page reports, the **host** writes the
clipboard through `CopyTextToClipboardW`, because a command from a native menu
gives the page no user activation and the shared host denies every permission
request; a whole-document copy is built from the intake, never from the DOM, so
virtualization cannot truncate it and the gutter's line numbers cannot leak
into it, and the file's own line ends are restored); the host is deleted after
the message loop drains, as in `mdview`; a declined file is now **skipped**
during panel navigation instead of blocking it, and its notice offers the
built-in viewer — reached from the viewer thread through a menu extension with
no menu items, because `ViewFileInPluginViewer` is documented main-thread-only
and `PostMenuExtCommand` is the one documented cross-thread route to it; Next /
Previous File are greyed out when there is no enumeration source; the encoding
re-read commits only after it has succeeded; zoom became per-window (the global
stays as the persisted starting value); every process-wide view setting is
broadcast to all windows; the fallback reports what actually happened; the lock
handle is forgotten on the window-creation failure path; the configuration
dialog is wide end-to-end, clamps to the nearest legal value with the loader's
own bounds, and "Restore Default File Types" is pending until OK.

**Page** (`web/viewer.js`, `viewer.css`) — variable-height geometry for wrap
mode (a measured height per line and its prefix sums, with the line under the
viewport top held in place while estimates are replaced by real heights; the
no-wrap path keeps the old uniform arithmetic unchanged); whitespace painted
over `.sp`/`.tb` runs that still contain the real characters; document state
reset on every file swap; the document generation separated from the token
generation; all theme properties written on every theme change, with fallbacks
derived from the theme's own foreground; horizontal reveal; a length-preserving
case fold; the caret column accumulated across a line's text nodes; the
configured font appended to the fallback stack rather than replacing it;
context-menu coordinates sent in device pixels.

**Worker** (`web/worker.js`) — the highlighter creation and every module load
memoized by their in-flight promise; a viewport beyond the sweep frontier
tokenized immediately without being marked done, so the sweep still re-emits it
authoritatively; a checkpoint is recorded only when the chunk itself resumed a
valid state; the sweep no longer stalls for ever on a missing checkpoint.

**Intake** (`intake.cpp/.h`) — I/O failure distinguished from "not text";
the BOM skipped only when there is one; every dot tried for a compound suffix;
the RISC-V probe given specific directives instead of the word "riscv"
anywhere in the first 8 KB.

**Language map** (`tools/codeview/gen_langmap.py`, `overlay-editor.json`, and
the regenerated `langmap.cpp`) — one root cause behind both R14 and R15: many
Linguist languages collapse onto one grammar, and the generator's
`setdefault`-based merge let the **alphabetically first** one win the display
name and the extension. It now visits the *canonical* language first (the one
whose own name or alias is the grammar name) and resolves an extension to the
language it is **primary** for. Ties keep alphabetical order, so the output
stays byte-reproducible. `shellscript` and `riscv` needed a display name and a
language row, which is what the committed overlay is for.

---

## 4. Two findings were fixed differently from their own suggestion

- **R17 (vacuous SC-001 gate).** The obvious fix — correct the counter — makes
  the gate fail at 194 < 200. Lowering the threshold would re-hide the gap, so
  the gap was **closed instead**: ten shipped grammars that no language row
  could reach (Fortran free/fixed form, Hjson, reStructuredText, AsciiDoc,
  Gettext catalogs, Qt style sheets, Beancount, MIPS assembly, Logo) were given
  rows and their — previously unclaimed — extensions. The gate now passes at
  **204 with an honest number**, and ten more formats highlight with grammars
  the .SPL was already carrying.
- **F16 (code tables).** Not fixed. See §7: it is a feature, not a repair, and
  doing it badly would break the encoding menu that works today.

---

## 5. Tests

Everything here runs without the application, a GUI or a build:

```
src\plugins\codeview\test\run_tests.cmd      :: all three, one exit code
```

| Harness | What it covers | New |
|---|---|---|
| `test/check_data.py` | 26 data rules: masks, tables, assets, licence manifest, coverage | the SC-001 rule now gates an honest number |
| `test/harness/test_worker.mjs` | the real `web/worker.js` in Node — 17 checks | +6: a viewport beyond the sweep frontier, the sweep surviving a missing checkpoint, and the creation race |
| `test/harness/test_page.mjs` | the DOM-free logic of `web/viewer.js` — 21 checks | **new file**: line arithmetic against `intake.cpp`, case-fold offsets, the wrap geometry, whitespace splitting |

`test_page.mjs` lifts the functions out of the shipped `viewer.js` by source
extraction and asserts the shape it extracted, so it cannot quietly pass
against a file whose logic has moved.

**The tests were proven not to be vacuous.** Run against the pre-fix files:

- `test_worker.mjs` → `FAIL - far viewport: answered before the sweep reached
  it (0 batch(es))` and `FAIL - far viewport: the answer came from the
  viewport, not from the sweep frontier`.
- `test_page.mjs` → `FAIL - shape: init() still special-cases the empty
  document`, then it stops on `Error: shape assertion: function lowerKeepLen
  not found in web/viewer.js`.

One check is honestly labelled as **not** proven this way: the
highlighter-creation race (R04) cannot be forced into its losing interleaving
from outside the module in Node — Node resolves the two creations in issue
order, which is the benign one. That check is an invariant test and says so in
the file.

Build gates: incremental Debug x64 **0 errors**, `build.cmd full` **0 errors,
189 language modules**, translations refreshed for the one new string
(8 languages × 98 entries, 0 validation failures, 0 duplicate accelerators,
368 DeepL characters).

---

## 6. Acceptance — the fixes were rejected once, and that was the point

Five further independent agents reviewed the fixes they had not written, each
told that *a fix which introduces a new defect is worse than the defect it
cured*: native memory and lifetime · the page and worker JavaScript ·
contracts, encoding and localization · the data generator and its output · a
whole-diff regression sweep.

**Four of the five REJECTED the change.** Five defects *in the fixes
themselves*, all in the newly written code:

| | What the fix broke |
|---|---|
| **A** | `doSelectAll` posted a copy message, so **Select All silently overwrote the clipboard** with the whole file. Select All is not a copy. |
| **B** | The new `mousedown` listener cleared the Select-All state on the **right** click that opens the context menu (mousedown precedes contextmenu), so "Select All → right-click → Copy" copied only the materialised rows — the exact truncation the host-side copy exists to prevent. |
| **C** | Routing scheme changes through the new broadcast made them send `setView` too, and `setView` rebuilt the wrap geometry from estimates: **F9 threw the reader's scroll position away** in a wrapped file — undoing part of fix F05. |
| **D** | A viewport chunk tokenized cold was never recorded, so **every later scroll re-tokenized it** for as long as the sweep had not arrived. |
| **E** | `contracts/host-page-interface.md`, which opens with *"binding … anything not listed here does not exist"*, was not updated for the new messages. |

Plus eleven minor ones: an allocator mismatch (`SalamanderGeneral->Free` on a
`malloc`ed buffer), copies stopping at an embedded NUL, a font name truncated
mid-UTF-8-character, a scroll guard that suppressed nothing, an O(lines) prefix
rebuild per scroll frame, a whole-document copy triggered by a
two-characters-short selection, a single-slot request that two windows could
overwrite, and — in the generator — a rank order that quietly moved `.cp`,
`.pp` and `.cl` to marginal languages, a family table left incomplete, and a
`.asc` claim that belongs to PGP armor, not AsciiDoc.

All sixteen were fixed, and a **second acceptance pass** by three more agents
confirmed every one of them resolved, with the geometry rework fuzz-tested
(2,625 randomized partial-rebuild cases against a full recomputation, 0
mismatches) and the worker correction proven against the real module (three
viewports over an unswept chunk → exactly one cold tokenization; two total
after the sweep; coverage 4000/4000).

That pass found **one more real defect**, which the first had missed and which
this review's own §8 hand-test list would probably not have caught either:
**Ctrl+C and Ctrl+A are printed in the Edit menu but were claimed by neither
accelerator route**, so they stayed with the engine and acted on the DOM —
i.e. on the ~200 materialised rows. The menu commands were now correct while
the shortcuts the menu itself advertises were not. Both keys are now claimed
and land in the same handlers.

Everything else the second pass raised was documentation-grade and is fixed:
the contract's accelerator section (which this change had itself invalidated by
claiming Ctrl+Plus/Minus), three stale payload rows — one of them the
`setView` fields that fix C's geometry key now depends on — a worker URL that
never existed, an inaccurate comment about which extensions the new rank order
rescues, and the two overlay pins that freeze `.pp`/`.sch`, which now say so.

One item was verified as **pre-existing and deliberately left**: 16 languages
(28 extensions, including `*.rc`, `*.iss`, `*.sln`) fall through to the
"Data and configuration" family because `FAMILY_OF_LANG` does not name them.
Identical at HEAD, so not a regression — but it means removing that family in
Options ▸ Viewers loses more than its name suggests. It belongs in the same
pass as the code-table work.

---

## 7. Deferred, with reasons

- **F16 — code tables for the ANSI band (FR-024).** The plugin decodes a
  single-byte file with `CP_ACP` only; the product's `EnumConversionTables`
  machinery (which the built-in viewer uses for ISO-8859-2, CP852, …) is not
  wired in, and the F8 menu is a fixed list of five. This is a feature-sized
  piece of work with its own UI question (how the table list is presented and
  remembered), and feature 069 established that those bytes reach plugins as
  code-page bytes, which the current UTF-8 pipeline would have to respect. It
  is the single largest remaining gap against the spec.
- **mdview on the shared host (T006/T007).** Unchanged by this session and
  still the first thing to finish: the product carries two copies of the
  WebView2 host, and only one of them has the liveness guard added here.
- **The GUI verification below.** No agent can press a key.

## 8. What still needs a human at the keyboard

Everything in this review was found and fixed by reading, running headless
harnesses and building. The following need the application open, and are the
honest remainder:

1. **Copy / Select All** — select with the mouse, Edit ▸ Copy; Edit ▸ Select
   All then Copy on a file longer than the render window (the whole file must
   arrive, with CRLF and without line numbers); the same with **Ctrl+A then
   Ctrl+C**, and once more via the right-click menu; and check that Select All
   alone does NOT change the clipboard.
2. **Word Wrap (F2)** on a file with long lines: scroll to the very end, use
   find and Ctrl+G, toggle wrap back off.
3. **Show Whitespace** — dots on spaces, an arrow at each tab run.
4. **Two windows at once** — F2/scheme in one must reach the other, including
   its menu check marks; the zoom in each window's title must match its own
   text.
5. **Ctrl+Plus / Ctrl+Minus / Ctrl+0**, and the right-click menu position on a
   scaled display at 100 % and 200 % zoom.
6. **Esc during the first F3 of a session** (the cold-start use-after-free).
7. **A declined file mid-panel** — Ctrl+PgDn must step over it, and the notice
   must offer the built-in viewer and actually open it.
8. **Czech UI** — status bar, window title, the new "open in the built-in
   viewer?" question.
9. **`.py`, `.php`, `.cs`, `.java`** — the title must now read `[Python]`,
   `[PHP]`, `[C#]`, `[Java]`, and a PHP template must colour as PHP.

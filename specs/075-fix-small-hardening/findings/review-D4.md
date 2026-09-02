# Review — D4 · `CViewerWindow::SetViewerCaption` guarded title trim

**Reviewer**: independent (did not write the fix) · **Date**: 2026-09-02 ·
**Branch**: `075-fix-small-hardening` · **Diff reviewed**:
`git diff -- src/viewer3.cpp` (working tree, uncommitted, **+19 / −0**, one
hunk at default context, lines 28–55).

**Protocol**: `specs/075-fix-small-hardening/contracts/fix-protocol.md`
Parts B and C (069 Part B/C verbatim). Governing FRs: spec **FR-005**, **FR-009**;
design `plan.md` D4, `research.md` R4.

**Charter note.** I reviewed this with the intent of finding a regression, and
specifically the one its sibling D5 was rejected for in round 1 (an
*unconditional* `SalU8TrimIncompleteTail` that ate the last character of an
untruncated code-page string — `findings/review-D5.md`, §B5). I could not
reproduce that shape here, or any other, and the reasoning below says why, from
my own tracing and my own probe, not from the fixer's record.

---

## B1 · Consumers and producers, re-enumerated independently

### (a) `FileName` — where it is allocated, from what, its guarantee and its maximum

`char* FileName` is declared at `src/viewer.h:329` ("currently viewed file").
Two writers, both `malloc`+`strcpy` of a **full path**:

| # | Site | Source | Bound |
|---|---|---|---|
| 1 | `src/viewer.cpp:564–573` (ctor) | `char name[SAL_MAX_PATH_UTF8]; lstrcpyn(name, fileName, SAL_MAX_PATH_UTF8); if (SalGetFullName(name)) FileName = malloc(strlen(name)+1)` | ≤ `SAL_MAX_PATH_UTF8 − 1` |
| 2 | `src/viewer2.cpp:884–887` (`OpenFile`) | `FileName = malloc(strlen(file)+1); strcpy(FileName, file)`; `file` is the caller's already-`SalGetFullName`'d buffer | same |

`SAL_MAX_PATH_UTF8` = `3 * SAL_MAX_PATH_W + 1` = `3 * 32767 + 1` = **98 302**
(`src/common/salpath.h:19,21`). The four `OpenFile` callers
(`viewer2.cpp:311`; `viewer3.cpp:654, 970, 1033`) all feed a
`char[SAL_MAX_PATH_UTF8]` that has been through `SalGetFullName`.

**Encoding guarantee**: WTF-8 — it is the feature-004/012/066 house path
encoding, produced by `SalGetFullName` and consumed by the wide facade.
**Maximum length**: 98 301 bytes, i.e. *far* past the 259-byte clamp. So the
`FileName` branch is genuinely reachable for the defect and genuinely safe for
the trim: a torn tail here really is a torn UTF-8 character.

### (b) `Caption` — traced to the plugin

`char* Caption` (`src/viewer.h:411`) is written in three places, always
`DupStr(caption)` or `NULL`:

`viewer.cpp:610` (ctor) and `viewer2.cpp:875–883` (`OpenFile`). Walking the
chain backwards from there:

```
plugin fills CSalamanderPluginInternalViewerData::Caption     (spl_gen.h:404–411)
  -> CSalamanderGeneral::ViewFileInPluginViewer(pluginData…)  (spl_gen.h:1914)
  -> viewer2.cpp:391   data.Caption = intViewerData->Caption
  -> viewer2.cpp:239   char captionBuf[SAL_MAX_PATH_UTF8];
                       lstrcpyn(captionBuf, data->Caption, SAL_MAX_PATH_UTF8)
  -> viewer2.cpp:311   view->OpenFile(name, caption, wholeCaption)
  -> viewer2.cpp:879   Caption = DupStr(caption)
```

`spl_gen.h:404–411` documents the field as
`const char* Caption; // NULL -> obsahuje caption okna FileName, jinak Caption`
— **no encoding stated and no length stated**. It is therefore an ANSI-facing
plugin surface (cluster B-5, protocol C10).

I did not take that on trust; I read what shipped plugins actually put there:

| Producer | Construction | Encoding of the result |
|---|---|---|
| `src/plugins/7zip/7zip.cpp:1417` | `sprintf(caption, "%s - %s", name, LoadStr(IDS_UNISO))`, `char caption[2000]` | **mixed** — `name` is UTF-8 (interface 104), `LoadStr` is the plugin's **ANSI** loader → code-page bytes |
| `src/plugins/peviewer/peviewer.cpp:420–423` | `_sntprintf(caption, …, _T("%s - %s"), name, LoadStr(IDS_PLUGIN_NAME))` | same mixture |
| `src/plugins/automation/salamanderaut.cpp:458–460` | `StringCchPrintf(caption, …, "%s - %s", fileU8, SalamanderGeneral->LoadStr(g_hLangInst, IDS_PLUGINNAME))` | same mixture |
| `src/plugins/demoplug/menu.cpp:277` | `"My file test.txt"` | ASCII |

**Answer to the question B1 asks**: `Caption` is **not** guaranteed UTF-8. A
plugin can and today *does* send a string that is part UTF-8 and part code-page,
of any length up to 98 301 bytes. This is exactly the condition that made an
unconditional trim a regression in D5, so the guard is not decorative here — it
is required. (Note the fixer's comment says so, and R4 says so; I confirmed it
independently against the four producers above rather than reading it off.)

### (c) Consumers of the local `caption` buffer, to the end of the function

`char caption[MAX_PATH + 300]` (= 560 bytes). Every read/write of it after the
copy block, in order:

| Line | Use |
|---|---|
| `viewer3.cpp:58` | `if (caption[0] != 0)` |
| `:59` | `strcat(caption, " - ")` |
| `:65` | `strcat(caption, LoadStrU8(IDS_VIEWERTITLE))` — feature 069 F-P4-02 |
| `:72` | `sprintf(caption + strlen(caption), " - [%s]", encName)` (≤ 16 B) |
| `:98` | `sprintf(caption + strlen(caption), " - [%s]", codeName)` (≤ 206 B) |
| `:101` | `WCHAR* captionW = SalU8ToWAlloc(caption)` — strict |
| `:104` | **`SetWindowTextW(HWindow, captionW)`** — sink 1 |
| `:108` | **`SetWindowText(HWindow, caption)`** — sink 2, the legacy fallback (`SetWindowTextA`, the core is ANSI-built: the `CL.exe` line for `viewer3.cpp` in the build log has no `/D UNICODE`) |

**Two `SetWindowText*` calls — confirmed**, and no other consumer: `caption` is
a local, its address is never taken, and it is not passed to anything but the
`strcat`/`sprintf`/`SalU8ToWAlloc` above. Nothing downstream reads the viewer's
title back either — the only `GetWindowText*` in the viewer files is
`viewer.cpp:68`, which reads a *find-dialog edit control*, not the frame.

The nine `SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE))` reset sites live in
`viewer2.cpp` (`:663, 685, 817, 839, 908, 1004, 1180`, plus the `CreateEx`
title at `:260` and the message-box captions) — a different surface, ANSI, out
of scope, and untouched (§C12).

---

## B2 · Per-surface verdict

| Surface | Verdict | Evidence |
|---|---|---|
| short ASCII name (`C:\a.txt`) | **unchanged** | `strlen = 8 < 260` → guard false, no trim; probe row `SAME` |
| short accented UTF-8 name (`C:\t0075\Přehled.txt`) | **unchanged** | same; probe row `SAME` |
| long (> 259 B) accented name, cut inside a character | **corrected** | the torn lead byte is dropped, `SalU8ToWAlloc` now succeeds, `SetWindowTextW` draws the whole title (name + *Prohlížeč* + coding) correctly instead of the whole thing falling to the code page |
| long (> 259 B) accented name, cut *between* characters | **unchanged** | the helper leaves a complete final character alone; probe row "3-byte complete just before the cut" = `SAME` |
| plugin caption that fits (≤ 259 B) | **unchanged** | guard false |
| plugin caption that is **code-page bytes and fits** (the D5 round-1 killer) | **unchanged** | guard false — this is the surface D5 regressed and D4 does not; verified for `0xE1`, `0xF9`, `0xE9`, `0xC8`, `0xF0`, `0xF9 B0 B1` tails *and* by an exhaustive sweep of every length 0…259 over all bytes `0x80–0xFF` (§B4) |
| plugin caption longer than 259 B, valid UTF-8 | **corrected** (same as the long-name row) | |
| plugin caption longer than 259 B, code-page bytes at the cut | **shortened by ≤ 3 further bytes**, spec-permitted, *not* a regression | the pre-fix string was already cut mid-character and already failed the strict decode; FR-005 requires byte identity only up to 259 B and explicitly permits shortening beyond it; R4 records the residual. See the accuracy nit in §B4 |
| `FileName == NULL && Caption == NULL` | **unchanged** | `caption[0] = 0`, untouched (§B4) |
| legacy fallback `SetWindowText` | **unchanged** and still reachable | §B5 |

No surface is regressed.

---

## B4 · Byte identity, argued (not asserted)

### The guard's boundary — is it exactly right?

`lstrcpyn` in this translation unit is `lstrcpynA` (no `UNICODE` define; the
build log's `CL.exe` command line for `viewer3.cpp` shows `/J` and no
`/D UNICODE`). `lstrcpynA(dst, src, n)` copies **at most `n − 1` bytes** and
always writes the terminator. With `n = MAX_PATH = 260` it keeps **259 bytes**.

Therefore it truncates **iff `strlen(src) ≥ 260`**, i.e. **iff
`strlen(src) >= MAX_PATH`** — which is character-for-character the new code's
predicate. The two are complements over the same quantity, so the trim runs on
exactly the truncated copies and on nothing else. This is the one place an
off-by-one would be fatal, so I measured it rather than reasoning about it:

```
strlen(src)=257 -> lstrcpynA stored 257  truncated=no   guard fires=no
strlen(src)=258 -> lstrcpynA stored 258  truncated=no   guard fires=no
strlen(src)=259 -> lstrcpynA stored 259  truncated=no   guard fires=no
strlen(src)=260 -> lstrcpynA stored 259  truncated=YES  guard fires=YES
strlen(src)=261 -> lstrcpynA stored 259  truncated=YES  guard fires=YES
strlen(src)=262 -> lstrcpynA stored 259  truncated=YES  guard fires=YES
```

`>` instead of `>=` would skip the trim on the first genuinely truncated length
(260) — the defect would survive for exactly one path length. `>= MAX_PATH - 1`
would trim an untruncated 259-byte string — the D5 round-1 regression, at the
boundary. **The code has neither.**

### A source of exactly `MAX_PATH − 1` = 259 bytes ending in a complete multi-byte character

**Untouched**, and by the *guard*, not by luck: `259 < 260`, so
`SalU8TrimIncompleteTail` is never called. Measured for a 259-byte source ending
in a complete 2-byte (`C5 99`), 3-byte (`E4 B8 AD`), 4-byte (`F0 9F 98 80`) and
WTF-8 lone-surrogate (`ED A0 80`) character — all `SAME`. (The helper would also
have left them alone on its own; the guard makes that argument unnecessary,
which is strictly safer.)

### A source of exactly `MAX_PATH` = 260 bytes

The guard **fires**, and that is right: `lstrcpynA` kept 259 of the 260 bytes, so
one byte was dropped and the tail may be torn. Measured above.

### Any source at or below the clamp — byte-identical to before?

**Yes, for every input, well-formed or not.** Beyond the named fixtures I ran an
exhaustive sweep: for every length `n` in `0…259`, a source whose bytes cycle
through **all of `0x80–0xFF`** (so every lead-byte class, every continuation
byte, every code-page accent, and every torn shape appears at every offset),
before vs. after:

```
fits-cases that DIFFER (must be 0): 0
```

together with the explicit shapes `Archiv \xE1`, `D:\tmp\Petrů` (`0xF9`),
`D:\tmp\résumé` (`0xE9`), `Soubor \xC8`, `Name \xF0`, `Name \xF9\xB0\xB1`,
`abc\x80\x80`, a *torn* UTF-8 tail that fits (`…\xC5`), and 258/259-byte ASCII.
FR-005's "a title built from a name of 259 bytes or fewer MUST be
byte-identical" and FR-009 hold **unconditionally**, not merely for valid UTF-8.

### The `caption[0] = 0` path (`FileName == NULL`)

**Unchanged.** The only edit to that branch is the pair of braces the new
`if` inside the `then` arm requires; the `else caption[0] = 0;` still binds to
`if (FileName != NULL)` exactly as before. Downstream, `if (caption[0] != 0)`
skips the `" - "` and `strcat(caption, LoadStrU8(IDS_VIEWERTITLE))` produces the
bare title — identical to HEAD.

### Two further identities I checked because nobody asked for them

- **The trim never turns a title that decoded into one that does not.** When the
  trim modifies anything, the bytes it removes are, by the helper's own
  predicate, a lead byte plus fewer continuation bytes than it promises — i.e.
  the pre-fix string was *already* invalid UTF-8. So the transition
  valid → invalid is impossible by construction. Confirmed empirically over a
  20 000-string pseudo-random corpus of 200–399-byte sources:
  `valid->invalid 0, invalid->valid 0`.
- **The trim can never blank the title.** In the trim branch the copy is always
  exactly 259 bytes, and the helper's write is `buf[i-1] = 0` only when
  `259 − (i−1) < seqLen ≤ 4`, hence `i−1 ≥ 256`. Brute-forced over all
  `255³ = 16 581 375` three-byte tails at the cut:
  `worst bytes lost beyond the clamp: 3 ; sources blanked: 0`.

**Accuracy nit (non-blocking, documentation only).** `research.md` R4 says a
truncated code-page caption "can lose at most **one more byte**". The true bound
is **three** (a `≥ 0xF0` byte followed by exactly two bytes in `0x80–0xBF`, e.g.
CP1250 `ů°±` = `F9 B0 B1`, measured above). It changes nothing about the verdict
— the affected string was already cut mid-character before the fix and FR-005
permits shortening past 259 bytes — but the sentence in R4 (and the "the last
character" phrasing in the source comment) understates the bound by two bytes.
Worth a one-word correction in the record, not a reject.

---

## B5 · Failure paths

- **The legacy fallback is still there, still reachable, still unchanged.**
  `viewer3.cpp:101–108` is entirely outside the diff. `SalU8ToWAlloc` is strict,
  so any caption still carrying code-page bytes (the mixed 7zip / peviewer /
  automation captions of §B1b, or the `codeName` path when
  `SalLegacyToU8Alloc` returns NULL) takes `SetWindowText(HWindow, caption)`
  exactly as before. Protocol **A4** and **C5** satisfied.
- **The fix never blanks the title** — proven two ways in §B4 (the index
  argument and the 16.5-million-case brute force). Even in the pathological
  all-continuation-bytes input the helper walks `i` to 0 and writes nothing.
- **New read of the source.** `strlen(FileName)` / `strlen(Caption)` is a new
  dereference, but it is evaluated *after* `lstrcpyn` has already walked the
  same string, and both pointers are `malloc`'d NUL-terminated copies
  (`viewer.cpp:568`, `viewer2.cpp:885`; `DupStr` at `viewer.cpp:610`,
  `viewer2.cpp:879`). No new failure mode. Cost is one extra O(n) scan of at
  most 98 KB, once per title update — not on any hot path (four call sites:
  `viewer2.cpp:910`, `viewer3.cpp:127, 597, 1962`).
- **`SalU8TrimIncompleteTail(NULL)`** cannot happen (`caption` is a local
  array), and the helper guards it anyway.

## B6 · Buffers

- `caption` is `char[MAX_PATH + 300]` = **560 bytes**. After the copy it holds
  ≤ 259 bytes; the trim only ever writes a `0` at a *lower* index, so the string
  can only get shorter. **The fix cannot disturb any later length assumption —
  it strictly relaxes every one of them.**
  Worst case after the fix, as before it: 259 + 3 (`" - "`) +
  `strlen(LoadStrU8(IDS_VIEWERTITLE))` (~10 B in every shipped language) +
  7 + 199 (`codeName`) ≈ 478 < 560. Unchanged headroom, pre-existing, not this
  fix's business.
- **Is `SalU8TrimIncompleteTail` safe on every input this can hand it?**
  Inputs are always a NUL-terminated buffer of exactly 259 bytes (the trim
  branch) inside a 560-byte array. Reading `salunicode.cpp:612–630`: the `while`
  guard `i > 0` and the `if (i > 0)` both dominate every `buf[i-1]` access, so
  the lowest index read is 0; the only write is `buf[i-1] = 0` with
  `1 ≤ i ≤ len`. **A buffer whose first byte is 0**: `len = 0`, `i = 0`, the
  loop is not entered, `if (i > 0)` is false — it returns having read one byte
  and written none. Safe. The `& 0xC0` tests cast to `unsigned char`, so `/J`
  (C7 — confirmed present on the `viewer3.cpp` command line) is irrelevant
  either way.
- WTF-8 (feature 066): a lone surrogate is an ordinary 3-byte sequence to the
  helper — kept when complete, dropped when torn. Both measured.

## B7 · Earlier scenarios — feature 069 F-P4-02

F-P4-02 is the two commented blocks *below* the diff, at `viewer3.cpp:60–65`
(`LoadStrU8(IDS_VIEWERTITLE)` instead of the ANSI `LoadStr`) and `:79–93` (the
`SalLegacyToU8Alloc(codeName, …)` normalisation of the conversion name for
display, with `CCodeTablesData::Name` itself left untouched per F-P4-01).

- **Neither block is in the diff.** The hunk ends at line 55; F-P4-02's code
  starts at 60.
- **Does D4 disturb it?** No — it *completes* it. F-P4-02's whole purpose was to
  stop one non-UTF-8 fragment from dropping the entire title to the code page;
  it fixed the two fragments it could see (the translated word, the conversion
  name) and left the third (a torn file name) because no finding covered it.
  D4 removes the third. The three are independent: the trim runs before any
  append, touches only `caption`, and never touches `codeName` (a separate
  200-byte local) or the `SalLegacyToU8Alloc` result.
- **Interaction check**: after the trim, `caption` is *more* likely to decode
  strictly, so `SalU8ToWAlloc` succeeds in strictly more cases than before —
  never fewer (§B4, `valid → invalid = 0`). The F-P4-02 comment's claim that
  "the maxBytes clamp cuts only on a UTF-8 boundary" is about `codeName` and is
  unaffected.
- Feature 069's D1 site (`CodeTables.GetCodeName(CodeType, codeName, 200)` at
  `:77`) is this batch's D1, already committed at `8102dd8`; it is not in this
  diff and I did not re-review it.

## B3 · Nothing refuted was changed

Nothing in the 068/069 refuted or by-design lists touches `SetViewerCaption`'s
copy block, and the diff reaches nothing else.

## B8 · Per-item path

None. One function, no new branch beyond the two guards, no configuration flag,
no per-caller special case. The two guards are the same predicate applied to the
two existing copies.

---

## C12 · Scope

- `git diff --stat` → the only source file is `src/viewer3.cpp`; everything else
  is `specs/075-fix-small-hardening/*`. `git diff --numstat -- src/viewer3.cpp`
  → **19 insertions, 0 deletions**, one hunk (default context) spanning lines
  28–55: the comment block plus the two guards plus the braces they need.
  **No line is deleted or moved**, so nothing outside the block can have
  changed.
- The `IDS_VIEWERTITLE` ANSI `LoadStr` sites are **confirmed untouched**: the
  nine `SetWindowText(HWindow, LoadStr(IDS_VIEWERTITLE))` / `CreateEx` title
  sites are all in `viewer2.cpp` (`:260, 663, 685, 817, 839, 908, 1004, 1180`
  and the message-box captions), which is not in the diff; the three in
  `viewer3.cpp` itself (`:449` commented out, `:1115`, `:1689`) are far below
  the hunk and unchanged.
- The already-committed D2 coding-menu change (`~:3300`) is in `HEAD`
  (`ff1c684`), not in the working-tree diff — correctly separated, as C12
  requires one item per commit.
- `git diff --stat -- src/plugins/shared/` → **empty**;
  `LAST_VERSION_OF_SALAMANDER` still **106**. C9 satisfied.

**C12: satisfied.**

---

## Standing invariants

| # | Status |
|---|---|
| C1 (core is ANSI-built) | confirmed from the build log: no `/D UNICODE` on `viewer3.cpp` → `lstrcpynA`, `SetWindowTextA` |
| C2 (`SalU8ToW` strict) | yes — `SalU8ToWAlloc` at `:101` is what the torn tail defeated; unchanged |
| C4 (`LoadStr` / `LoadStrU8`) | no `LoadStr` in the diff; F-P4-02's `LoadStrU8` untouched |
| C5 (the tolerant sink's fallback is load-bearing) | **honoured** — the fallback is unchanged *and* the guard exists precisely so it keeps receiving whole code-page captions |
| C7 (`/J`) | `/J` present on the command line; the helper casts to `unsigned char` regardless |
| C9 (interface 106, no shared-header diff) | satisfied |
| C10 (cluster B-5 out of scope) | respected — the fix does not try to fix the plugin caption's encoding; it also does not damage it |
| C11 (`CCodeTablesData::Name` untouched) | not reached by this diff |
| C14 (no `saltests` count change) | **1353 checks, 0 failed** — identical to the T003 baseline |

---

## Evidence I produced myself

1. **Build**: forced a recompile of `viewer3.cpp` (`touch` + `build.cmd`) —
   `viewer3.cpp` genuinely rebuilt (`/W3 /RTCc /RTC1 /J`, no `/D UNICODE`),
   **0 upozornění, 0 chyb, BUILD SUCCEEDED**.
2. **Unit tests**: `saltests.exe` → **1353 checks, 0 failed** (C14 baseline).
3. **The batch probe**: `specs\075-fix-small-hardening\probe\run_probe.cmd` →
   **35 checks, 0 failed**, including the D4 before/after and the "unguarded
   would drop the last character" rationale check.
4. **My own probe** (written independently, kept out of the repository in the
   session scratchpad — same `/J /RTC1 /Od /MDd` switches, the helper and both
   copy bodies transcribed from `salunicode.cpp:612–630` and the diff):
   **10 checks, 0 failed**. It adds what the batch probe does not cover:
   - the truncation boundary measured at `strlen(src)` = 257…262, asserting
     `guard fires == lstrcpynA truncated` at each;
   - a 259-byte identity set ending in a complete 2-, 3-, 4-byte and WTF-8
     character, and in code-page `0xE1` / `0xF9` / a torn `0xC5` — the exact
     shapes D5's round-1 review found fatal;
   - an **exhaustive** identity sweep over lengths 0…259 with all bytes
     `0x80–0xFF` → `fits-cases that DIFFER: 0`;
   - a brute force over all `255³` three-byte tails at the cut →
     `blanked: 0`, `worst loss beyond the clamp: 3`;
   - a 20 000-case corpus →
     `valid->invalid 0` (the trim never breaks a title that already decoded).

**Evidence gap I would still like closed, non-blocking**: the batch probe's D4
identity fixtures are 21, 8 and 0 bytes — all short, all ending in ASCII. The
two shapes that killed D5 round 1 (a **fitting code-page** source ending
`≥ 0xC0`, and a **259-byte** source of any content) are not in it; had the guard
been omitted here, the batch's own D4 checks would still have passed. My probe
covers both and reports zero differences, and the "D4 rationale: the UNGUARDED
trim would drop its last character" check does demonstrate the mechanism on a
short fixture — so this is a completeness remark about the committed probe, not
a doubt about the fix. Two lines in `TestD4`'s `shortPath[]` (`"Archiv \xE1"`
and a 259-byte string) would close it.

The site-level GUI evidence (quickstart **S4**, Czech UI, the 289-byte `č` path)
remains a human step, as `fix-log.md` T008 records.

---

## Summary

I went looking for the D5 round-1 regression and for anything else, and found
neither. The guard is **correct** (its predicate is the exact complement of
`lstrcpynA`'s truncation condition, verified at every length from 257 to 262)
and **sufficient** (every source of ≤ 259 bytes is byte-identical for *all*
byte content, proven by exhaustive sweep — not merely for valid UTF-8, which is
the distinction D5 round 1 turned on, and which matters here because `Caption`
provably carries code-page bytes from three shipped plugins). The trim cannot
blank the title, cannot lose more than three bytes beyond a cut that had already
happened, cannot break a title that decoded before, and cannot disturb the later
buffer arithmetic because it only ever shortens. The `caption[0] = 0` path, the
`SalU8ToWAlloc` sink, the legacy `SetWindowText` fallback and feature 069's
F-P4-02 blocks are all outside the diff and unaffected; the ABI is untouched;
the scope is exactly the caption-copy block.

Two non-blocking record notes: `research.md` R4 (and the new source comment)
understate the worst-case residual loss on an *already truncated* code-page
caption as one byte where it is three; and the committed probe's D4 identity
fixtures do not include the two shapes that would have caught an unguarded
trim.

**VERDICT: ACCEPTED**

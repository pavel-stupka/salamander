# Research — Small hardening batch (075)

**Branch**: `075-fix-small-hardening` · **HEAD checked**: `640b94a` · **Date**: 2026-09-02

Every item below was established by reading the code at HEAD, not from the
handoff text. Line numbers are HEAD's. Where the handoff and HEAD disagree, HEAD
is recorded and the difference noted.

---

## R0 — Still defective at HEAD? (069 protocol A0)

| # | Handoff said | HEAD (`640b94a`) | Delta |
|---|---|---|---|
| D1 | `codetbl.cpp:873` | `:874` — `if (len > bufferLen)`; plus `strcpy(buff, …)` into `char buff[1024]` at `:870/:872` with no bound; the name is stored whole by `DupStr(name)` at `:247` when `convert.cfg` is read | line +1; **the unbounded scratch copy is a second overflow in the same function**, not in the handoff |
| D2 | `viewer3.cpp:3291` | `:3300–3301` — `int defCodeType; CodeTables.GetCodeType(DefaultConvert, defCodeType);` | +9 |
| D3 | `zip.cpp:3292` | `:3301` — `GetConversionTable(HWND, char* table, const char* conversion)`; only `table` is NULL-checked (`:3304`) | +9 |
| D4 | `viewer3.cpp:30/35` | `:31` and `:36` — `lstrcpyn(caption, FileName/Caption, MAX_PATH)` | +1 |
| D5 | `controls.cpp:24,39` | `:24`, `:39` — `strcpy(Text, text)`; `Text` is `char[MAX_PATH]` (`controls.h:18`) | 0 |
| D6 | `run_tests.cmd` red on Node 20 | machine now on **Node v24.19.0**; `run_tests.cmd` → *all codeview checks passed*; `node --no-experimental-detect-module …\test_worker.mjs` → fails (`ERR_REQUIRE_CYCLE_MODULE`, i.e. `worker.js` loaded as CommonJS); `node --experimental-detect-module …` → `RESULT: ALL PASS` | environment changed, cause intact |

**Decision**: all six proceed as *fixed*; none is verify-closed. D6's fix is
kept (spec Assumptions) because the reproduction is one flag away on any
machine.

---

## R1 — D1: the shape of the bounded copy

**Function** (`codetbl.cpp:856–881`): copies the name into `buff[1024]` with
`strcpy`, clamps `len` to `bufferLen-1` only when `len > bufferLen`, `strncpy`s
`len` bytes and writes `buffer[len] = 0`, returns `strlen(buff) <= bufferLen`.

**Defects in it** (all in the one function):
1. `len == bufferLen` → `buffer[bufferLen] = 0` — one byte past the caller's
   buffer, result *fits* (wrong).
2. `strlen(Name) >= 1024` → `strcpy(buff, …)` overflows the scratch buffer.
3. `bufferLen <= 0` → `len = bufferLen - 1` is negative → `strncpy` with a huge
   `size_t`. (Not called that way today; guarded anyway because the fix makes
   it free.)

> **CORRECTION (2026-09-02, found while running the GUI proof).** This section
> first said `Name` is unbounded — "`DupStr` of whatever `convert.cfg` line the
> user wrote". **That is wrong.** The parser clamps every name before storing
> it: `InitAux` reads into `char nameBuf[200]` with
> `int l = (int)min(txt - beg, 199)` (`codetbl.cpp:59,154`), and the only other
> names are two translated strings. The longest storable name is therefore
> **199 bytes**.
>
> Both overflows were consequently **unreachable, not merely unreached**:
> defect 2 needs a name ≥ 1024 bytes, and defect 1 needs a name of exactly
> `bufferLen`, which with the three callers means exactly 200 or exactly 1024 —
> all above the 199-byte ceiling. A hand-edited `convert.cfg` cannot produce
> them either; a 200-`A` line is stored as 199 `A`s.
>
> This does not change the fix or its verdict. It makes the function correct for
> *any* buffer size, which is what it should have been; it changes only what this
> record may claim about how it could be triggered. Quickstart S1's fixture is
> updated to say it cannot discriminate, and why.

**Decision**: one bounded copy from the source, no scratch:

```
const char* name = codeType == 0 ? LoadStr(IDS_VIEWERNONECODING) : Table->Data[codeType - 1]->Name;
int nameLen = (int)strlen(name);
if (bufferLen > 0) lstrcpyn(buffer, name, bufferLen);
return nameLen < bufferLen;
```

`lstrcpyn(dst, src, n)` copies at most `n-1` bytes and terminates — the house
call throughout this file. `LoadStr` returns a rotating static buffer; it is
consumed immediately, as today.

**Byte-identity argument** (FR-009), by case:
- `nameLen < bufferLen`: full copy + terminator, return TRUE — identical.
- `nameLen > bufferLen`: `bufferLen-1` bytes + terminator, return FALSE —
  identical (today: `len = bufferLen-1`, `strncpy`, terminator, FALSE).
- `nameLen == bufferLen`: today `bufferLen` bytes + terminator **one past the
  end**, return TRUE. After: `bufferLen-1` bytes + terminator, return FALSE.
  This is the fix (spec FR-002 boundary rule).
- Encoding: bytes are copied verbatim in both versions. `Name` keeps the
  `convert.cfg` bytes (069 F-P4-01, plugin-facing) — untouched.

**Consumers** (own `rg`, `GetCodeName(`):
| Site | Buffer | Uses return? | Effect of the change |
|---|---|---|---|
| `dialogs3.cpp:136` `CConvertFilesDlg::UpdateCodingText` | `buff[1024]`, passes 1024 | no | none below 1024 bytes; a ≥1024-byte name no longer overflows (either buffer) |
| `viewer3.cpp:58` `SetViewerCaption` | `codeName[200]`, passes 200 | no | none below 200; a 200-byte name no longer writes `codeName[200]` (the `/RTC1` reproducer, quickstart S1) |
| `viewer3.cpp:1914` `CM_SETDEFAULT_CODING` | `DefaultConvert[200]`, passes 200 | **yes** — clears the default on FALSE | a 200-byte name: today the clamp does **not** fire (`len > bufferLen` is false at `len == bufferLen`), so the *whole* 200-byte name is copied and the terminator is written one past the array — the overflow lands in whatever follows `Configuration.DefaultConvert` and the stored "string" runs on into it, and the call still reports success. After the fix the default is cleared instead. Intended: a name that does not fit could never match again anyway (`GetCodeType` compares whole names). *(Corrected after the D1 review, which showed the earlier wording — "stores a 199-byte truncated default" — was wrong about the pre-fix behaviour.)* |

**Alternatives rejected**: (a) just `>` → `>=` — leaves defect 2 in place;
(b) `strncpy_s`/`strcpy_s` — not the house call, and `_TRUNCATE` semantics are
what `lstrcpyn` already gives.

---

## R2 — D2: the unset default

**Path** (`viewer3.cpp:3284–3302`): `CodeTables.InitMenu(subMenu, CodeType)` —
returns early with a trace when `!Loaded` (`codetbl.cpp:624`), leaving the menu
empty — then, with auto-select off, `int defCodeType; GetCodeType(DefaultConvert, defCodeType);`
`SetMenuDefaultItem(subMenu, CM_CODING_MIN + defCodeType, FALSE)`.

`GetCodeType` (`codetbl.cpp:792–842`) assigns `codeType` on every path **except**
`!Loaded` (`:795–799`, returns FALSE without touching it). So the unset read is
exactly the unloaded case.

**When is `Loaded` FALSE after a viewer opened?** `CCodeTables::Init`
(`:576–618`) falls back to the best available set when the configured
`convert.cfg` is missing or damaged, and "accepts anything" for the fallback —
`Loaded = Table != NULL`, i.e. FALSE only when `new CCodeTable` returned NULL
(allocation failure, traced as `LOW_MEMORY`). No data fixture provokes it.

**Decision**: `int defCodeType = 0;` + comment. On the unloaded path the call
becomes `SetMenuDefaultItem(<empty menu>, CM_CODING_MIN, FALSE)`, which fails
harmlessly. On the loaded path `GetCodeType` overwrites the 0 as today
(entry index, or 0 for not-found → the *none* item becomes the default —
unchanged).

**Alternative rejected**: skip `SetMenuDefaultItem` when `GetCodeType` returns
FALSE — that would also skip the loaded-not-found case, where today the *none*
item is highlighted; FR-009 forbids it.

**Proof**: no fixture. Debugger scenario (quickstart S2): force `Loaded = 0` in
a Watch window before `:3291` executes, step to `:3302`, read `defCodeType` in
Locals — `0xCCCCCCCC` (the `/RTC1` fill) before the fix, `0` after.

---

## R3 — D3: the NULL guard

**Function** (`zip.cpp:3301–3317`): `CALL_STACK_MESSAGE2("…(%s)", conversion)`;
`if (table == NULL) { TRACE_E(…); return FALSE; }`; `Init`; `GetCodeType(conversion, …)`;
`GetCode`.

**What a NULL `conversion` does today**: `GetCodeType` → `SalU8ToW(NULL, …)`
returns 0 (safe) → then the loop calls `CodingNameEqual(n, NULL)` → dereference
→ access violation inside a plugin call.

**The trace macro**: `CALL_STACK_MESSAGE2` expands to a `printf` only when
`__CallStk_T` is false (`callstk.h:199`), and the UCRT prints `(null)` for a
NULL `%s`. The twin `RemoveFilesFromCache` (`:3278`) has the identical order
(macro, then NULL check). Keep the order.

**Decision**: add the guard in the exact shape of the `table == NULL` block,
message `"Invalid parametr (conversion==NULL) in CSalamanderGeneral::GetConversionTable!"`
(the file's spelling, kept for `rg`-ability).

**Plugin-facing**: the shared header's comment (`spl_gen.h:2194–2201`) says
FALSE ⇒ the table is not valid. A NULL name returning FALSE is inside that
contract. No header edit; interface stays 106.

**Proof**: quickstart S3 — a debugger Immediate-window call with a NULL name
against the Debug build: access violation before, `FALSE` + trace line after.

---

## R4 — D4: the guarded trim

**Sites** (`viewer3.cpp:25–92`): `caption[MAX_PATH + 300]`;
`lstrcpyn(caption, FileName, MAX_PATH)` (`:31`) or
`lstrcpyn(caption, Caption, MAX_PATH)` (`:36`); then `" - "`, `LoadStrU8(IDS_VIEWERTITLE)`,
the encoding suffix; then `SalU8ToWAlloc(caption)` → `SetWindowTextW`, else
`SetWindowText(caption)` (legacy fallback, `:82–89`).

**Producers**: `FileName` — heap copy of `SalGetFullName` output
(`viewer.cpp:564–570`), WTF-8, up to `SAL_MAX_PATH_UTF8` (98,302 bytes).
`Caption` — `DupStr(caption)` from the internal-viewer request
(`viewer.cpp:610`), which comes from a plugin (`viewer2.cpp:391`,
`intViewerData->Caption`); **encoding not guaranteed** (plugin-facing ANSI
services, cluster B-5).

**Why the cut tears**: `lstrcpyn(…, MAX_PATH)` keeps 259 bytes. If byte 259 is
a lead byte or a mid-sequence continuation, the strict `SalU8ToWAlloc` rejects
the whole caption and the translated *Prohlížeč* + the file name render through
the code page.

**Existing shapes in the tree** for "clamp, then trim":
- unconditional: `dialogs5.cpp:1077`, `fileswn3.cpp:293`, `shellib.cpp:2989`
  (all on values known to be UTF-8);
- **guarded** by "did the clamp truncate?": `cmdshell.cpp:232–234`
  (`if (strlen(program) >= 2 * MAX_PATH) SalU8TrimIncompleteTail(shown);`).

**Decision**: the guarded shape at both sites, because of `Caption`: an
unconditional trim would drop the final byte of an *untruncated* code-page
caption whose last byte is ≥ 0xC0 (e.g. CP1250 `á` = 0xE1 looks like a 3-byte
lead with no continuation) — a regression on a working legacy plugin. With the
guard, a name of ≤ 259 bytes is never touched (FR-005 byte identity), and a
truncated code-page caption can lose **up to three** more bytes of an already-cut
string while the legacy fallback still draws it.

**Helper facts** (`salunicode.cpp:612–630`, tests `saltests.cpp:1439–1477`):
walks back over continuation bytes, drops an incomplete lead, leaves a complete
final character alone; WTF-8 lone surrogates are ordinary 3-byte sequences to
it. No new unit test is needed for the helper; the *site* is proven by the
title scenario.

**Consumers of `caption`**: the two `SetWindowText*` calls only.

**Proof**: quickstart S4 — a 289-byte path built so that byte 259 is the lead
byte of a `č` (fixture in the quickstart); Czech UI; title garbled before,
correct after; a ≤ 259-byte accented path identical before/after.

**Alternative rejected**: enlarge the cut to the whole name — `caption` is
`MAX_PATH + 300`, so that means a 98 KB stack buffer or a heap path; a
behaviour change the spec rules out.

---

## R5 — D5: the plugin-side bounded copy

**Sites** (`controls.cpp:20–41`): ctor and `SetText` both `strcpy(Text, text)`
then `TextLen = strlen(text)`. `Text` is `char[MAX_PATH]` (`controls.h:18`),
documented as a UTF-8 path (`controls.cpp:88`, "interface 104").

**Intake is bounded by construction** — every caller of `SetText`
(`mainwnd.cpp:714, 715, 992, 993, 1015, 1016, 1439, 1440, 1974, 1975, 1997, 1998`)
passes `""` or `Path1/Path2`, which are `char[MAX_PATH]` filled by `strcpy` from
`res->Files[n].Name` (`mainwnd.cpp:990`), which is the thread's `Path1[MAX_PATH]`
(`filecomp.h:81`), filled by `strcpy` in the ctor (`filecomp.h:89`) from
`file1[MAX_PATH]` built with `GetPanelPath(…, MAX_PATH)` +
`SalPathAppend(…, MAX_PATH)` (`filecomp.cpp:497–598`) or from
`msg->Path1[MAX_PATH]` (`remote.cpp:113–123`). So no path over 259 bytes reaches
the header today; D5 is defensive.

**Decision**: `lstrcpynA(Text, text, _countof(Text))` (already used in the same
file, `:104`) + a local six-line walk-back (drop trailing continuation bytes,
then an incomplete lead) + `TextLen = (int)strlen(Text)`. Local because the
plugin cannot include `salunicode.h` and `src/plugins/shared/` must not grow
(069 C9). Byte-identical for any text under 260 bytes.

**Not touched, recorded**: `CFilecompThread::CFilecompThread` (`filecomp.h:89–90`)
and `mainwnd.cpp:990, 993, 1972, 1975` `strcpy`s — the same shape, bounded by
the same construction, outside D5 (constitution III). Listed in the fix log
under "seen, not changed" so the next reader does not re-discover them.

**Proof**: quickstart S5 — Debug build, Immediate-window `SetText` with a
300-byte literal: heap-corruption report on window close before, truncated
header and no report after.

---

## R6 — D6: the runner flag

**Cause**: `test_worker.mjs:31` does `await import(pathToFileURL(join(web, 'worker.js')).href)`.
`web/worker.js` is an ES module (`import` statements) in a directory with no
`package.json`; Node < 22.7 treats a bare `.js` as CommonJS unless
`--experimental-detect-module` is given (flag exists from 20.10; on by default
from 22.7, the default from 22.7). `test_page.mjs` reads the sources as text and
never imports the `.js` file — unaffected.

**Verified on this machine** (Node 24.19.0):
- `node --experimental-detect-module …\test_worker.mjs` → `RESULT: ALL PASS`
  (flag accepted where it is the default);
- `node --no-experimental-detect-module …\test_worker.mjs` → exit 1,
  `ERR_REQUIRE_CYCLE_MODULE … Cannot require() ES Module …\web\worker.js` —
  the same root (CJS loader applied to an ESM file), a usable fail-first proof
  without a Node 20 install. (`npx node@20.18.0` produced no output here, so a
  genuine Node 20 run needs nvm-windows/fnm and is a should-do, not a gate.)
- No repository workflow runs `run_tests.cmd` (`rg run_tests .github` → none),
  so there is no CI signal to preserve or fix.

**Decision**: add the flag to the worker line in `run_tests.cmd`, and one header
line stating the Node floor (≥ 20.10). Nothing else.

**Alternatives rejected**: `web/package.json {"type":"module"}` — a new file in
the shipped plugin's source tree next to embedded assets, with an unverified
interaction with `build_web.py`/`check_data.py`; renaming `worker.js` →
`worker.mjs` — changes a shipped asset, the resource table, the
`new Worker('worker.js')` reference and the host allow-list; a doc-only Node
floor — the spec's fallback (verify-closed), kept only if the maintainer
prefers it.

---

## R7 — Test strategy and fail-first proofs (FR-008)

`saltests` links only `src/common/{salclip,salfileio,salpath,salshell,salunicode}.cpp`
(`saltests.vcxproj:80–90`), so none of D1–D5's sites is reachable from it, and
the one shared helper D4 reuses is already covered. This feature therefore adds
**no unit checks** and relies on recorded scenarios, each mechanical:

| # | Proof before (must fail) | Proof after (must pass) | Where |
|---|---|---|---|
| D1 | Debug build, `convert.cfg` with a 200-byte name; select it in the viewer → `/RTC1` "Stack around the variable 'codeName' was corrupted" on leaving `SetViewerCaption`; with an 1100-byte name → the same for `buff` inside `GetCodeName` | no RTC report; title shows the 199-byte prefix | quickstart S1 |
| D2 | Watch-window `Loaded = 0`, Locals show `defCodeType == 0xCCCCCCCC` | `0` | S2 |
| D3 | Immediate-window call with NULL name → access violation | `FALSE`, trace line | S3 |
| D4 | Czech UI, 289-byte fixture path → garbled title | correct title; short path identical | S4 |
| D5 | Immediate-window `SetText` 300 bytes → heap corruption on close | truncated header, no report | S5 |
| D6 | `node --no-experimental-detect-module …` fails on Node 24 (and Node 20 plain fails, if available) | `run_tests.cmd` → passed on Node 24 (and 20) | S6 |

The proof is pasted into `fix-log.md` per item, before/after, from a
`git stash` / `git stash pop` cycle (069 A5).

---

## R8 — Commit and review granularity

One commit per defect (`[075] D1 …` … `[075] D6 …`), each independently
revertible (constitution III), each reviewed by an agent that did not write it
(FR-010, contract Part B), verdict in `findings/review-D<n>.md`. A REJECTED
verdict reworks that one commit only.

---

## R9 — Changelog

D4 is user-visible → *Fixed* entry in the user's terms. D1/D3/D5 → one honest
hardening line. D2 → no reproducer with shipped data → no entry. D6 →
developer-only → no entry. Applied at the ship gate of the release that carries
the batch, with the version/build bump (the 071/074 pattern; text drafted in
`plan.md` "Ship gate").

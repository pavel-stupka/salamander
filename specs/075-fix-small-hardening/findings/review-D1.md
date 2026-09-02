# Review — D1 · `CCodeTables::GetCodeName` (`src/codetbl.cpp`)

**Reviewer**: independent agent; did not write the fix.
**Charter**: find a regression, not approve one.
**Diff reviewed**: `git diff -- src/codetbl.cpp` at working tree of branch
`075-fix-small-hardening` (base `c554f4d`), 14 insertions / 13 deletions,
diff md5 `489f73815d74d633b195ea72ff28af19` (re-checked unchanged at the end of
the review — other items of the batch were being edited in parallel).
**Protocol**: `contracts/fix-protocol.md` Parts B and C (069 protocol by
reference); spec FR-002, FR-009; research R1; plan Design D1.

---

## B1 · Consumers, re-enumerated independently

Own sweep, whole repository, no path filter:

```
rg "GetCodeName"          →  7 hits outside specs/
src\codetbl.h:107        declaration
src\codetbl.cpp:856      definition
src\codetbl.cpp:858      CALL_STACK_MESSAGE3 text
src\dialogs3.cpp:136     call
src\viewer3.cpp:58       call
src\viewer3.cpp:1914     call
```

(A second sweep with the looser pattern `GetCodeName\s*\(` over the whole tree,
`src/plugins/` included, returned the same set. No plugin, no test, no
generated file calls it.)

| # | Site | Buffer | `bufferLen` | Return used? | What it does with the result |
|---|---|---|---|---|---|
| C1 | `src/dialogs3.cpp:136` `CConvertFilesDlg::UpdateCodingText` | `char buff[1024]` (local) | `1024` | **no** | `RemoveAmpersands(buff)` → `SetDlgItemText(HWindow, IDC_CHC_CODING, buff)` |
| C2 | `src/viewer3.cpp:58` `CViewerWindow::SetViewerCaption` | `char codeName[200]` (local) | `200` | **no** | `RemoveAmpersands` → `SalLegacyToU8Alloc(codeName, 199)` → `lstrcpyn` back → trailing-space trim → `sprintf(caption+…, " - [%s]", codeName)` |
| C3 | `src/viewer3.cpp:1914` `CM_SETDEFAULT_CODING` | `char DefaultConvert[200]` (member of `CViewerWindow`, `viewer.h:384`) | `200` | **yes** — `if (!…) DefaultConvert[0] = 0;` | the value is later matched by `GetCodeType` (`viewer2.cpp:1131`, `viewer3.cpp:3301`) and, on viewer close, copied into `Configuration.DefaultConvert` (`viewer3.cpp:3618–3622`) and persisted to the registry (`mainwnd2.cpp:2014`) |

**No fourth caller.** Research R1's list is complete and its buffer sizes are
correct. The function is **not** plugin-facing: the services in `zip.cpp` that
touch the tables are `EnumConversionTables` → `EnumCodeTables` (:3298),
`GetConversionTable` → `GetCodeType` + `GetCode` (:3323–3325) and
`GetWindowsCodePage` → `GetWinCodePage` (:3334). `GetCodeName` appears in none
of them, so there is no plugin ABI surface here and no FR-009 plugin exposure
beyond the name bytes themselves (see C11).

---

## B2 · Per-surface verdict

Proofs, not assertions — see B4 for the case analysis and the executed sweep.

| # | Site | Verdict | Proof |
|---|---|---|---|
| C1 | `dialogs3.cpp:136` (1024) | **unchanged** for every name of ≤ 1023 bytes; **corrected** for ≥ 1024 | Reviewer sweep, `bufferLen = 1024`, `nameLen = 0…1100`: the caller's 1024 bytes and the (unused) return value are identical for every length except `nameLen == 1024`; return value is not read here, so even that length is a no-op for this surface. Names ≥ 1024 previously overran the 1024-byte **scratch** (`strcpy(buff, Name)`) — the sweep shows the scratch canary broken at `+0` for every `n ≥ 1024`; after, no scratch exists. Longest shipped name is 33 bytes (`EBCDIC IBM International - CP1250`, `convert/centeuro/convert.cfg`), so nothing user-visible moves. |
| C2 | `viewer3.cpp:58` (200) | **unchanged** for every name of ≤ 199 bytes; **corrected** for ≥ 200 | Sweep, `bufferLen = 200`, `nameLen = 0…300`: byte-identical buffer and identical return for every length except `nameLen == 200`. At `nameLen == 200` the old body wrote `codeName[200]` — one byte past a 200-byte **stack** array (sweep: `overrun before=+0`); the new body writes a 199-byte terminated prefix and stays inside. The return value is not read here, so the changed return is invisible to this surface. |
| C3 | `viewer3.cpp:1914` (200) | **unchanged** for every name of ≤ 199 bytes; **intended change** at exactly 200; **corrected** for > 200 | Same sweep. Traced in full in B4 case 3 below. |

No surface is **regressed**.

---

## B3 · Nothing refuted was changed

The one standing decision in this area is feature 069 **F-P4-01**, which
explicitly *refused* to re-encode `CCodeTablesData::Name` (the bytes are handed
to plugins and persisted by dbviewer/filecomp) and repaired the *lookup*
instead. The diff honours that decision, cites it in its comment, and adds no
conversion. Nothing that 068/069 marked refuted, latent or by-design is touched.

---

## B4 · Byte identity, argued from the new code

New body (the whole of it):

```c
const char* name = codeType == 0 ? LoadStr(IDS_VIEWERNONECODING) : Table->Data[codeType - 1]->Name;
int nameLen = (int)strlen(name);
if (bufferLen > 0)
    lstrcpyn(buffer, name, bufferLen);
return nameLen < bufferLen;
```

Old body: `strcpy` into `char buff[1024]`; `len = strlen(buff)`;
`if (len > bufferLen) len = bufferLen - 1;` `strncpy(buffer, buff, len);`
`buffer[len] = 0;` `return strlen(buff) <= bufferLen;`.

### `strlen(name) < bufferLen` → same bytes, same TRUE?

**Yes.** Old: `len == nameLen`, the clamp does not fire (`nameLen > bufferLen`
is false), `strncpy(buffer, buff, nameLen)` copies exactly the name's bytes —
`strncpy` pads with NULs only when `n > strlen(src)`, and here `n == strlen(src)`,
so it copies `nameLen` bytes and no more — then `buffer[nameLen] = 0`. Result:
name + terminator in `buffer[0..nameLen]`, all inside the buffer because
`nameLen ≤ bufferLen - 1`. Return `nameLen > bufferLen` is false → TRUE.
New: `lstrcpyn(buffer, name, bufferLen)` copies up to `bufferLen - 1` bytes,
stops at the source NUL after `nameLen` bytes, terminates at `buffer[nameLen]`.
Identical bytes. Return `nameLen < bufferLen` → TRUE. **Identical.**

### `strlen(name) > bufferLen` → same `bufferLen-1` bytes + NUL, same FALSE?

**Yes.** Old: clamp fires, `len = bufferLen - 1`, `strncpy` copies exactly
`bufferLen - 1` bytes (no padding — the source is longer), `buffer[bufferLen-1] = 0`.
Return `nameLen > bufferLen` → FALSE. New: `lstrcpyn` copies `bufferLen - 1`
bytes and terminates at `buffer[bufferLen-1]`. Return `nameLen < bufferLen` →
FALSE. **Identical.**

*The one gap in that argument is whether `lstrcpyn` truncates byte-for-byte or
does something multibyte-aware.* Settled two ways:
1. **Statically** — in this translation unit `lstrcpyn` is the Win32 macro
   (see C1 below); the project's own drop-in `_sal_lstrcpynA`
   (`src/common/lstrfix.inc`) is a plain `while (dst < end && *src) *dst++ = *src++; *dst = 0;`
   with an `iMaxLength <= 0` early-out, and kernel32's `lstrcpynA` is the same
   plain byte copy. Neither resolution changes the semantics, so the fix is
   correct whichever one a future include change produces.
2. **Empirically** — the reviewer sweep below feeds names built from
   `0xC4 / 0x8D / 'A'` (multi-byte lead + continuation + ASCII) precisely so a
   DBCS/UTF-8-aware copy would show; no divergence appeared at any length.

### `strlen(name) == bufferLen` → the INTENDED change

**Confirmed intended**, and it is the item's whole point: spec **FR-002**
("The boundary case — a name exactly as long as the buffer — MUST be treated as
*did not fit*, like every longer name") and plan Design D1.

Old, at `nameLen == bufferLen`: the clamp tests `len > bufferLen` → **false**,
so `len` stays `bufferLen`; `strncpy` copies all `bufferLen` bytes; then
`buffer[bufferLen] = 0` — **one byte past the caller's storage** — and
`strlen(buff) > bufferLen` is false → **TRUE**. New: 199-byte prefix +
terminator, **FALSE**.

**Trace of the one caller that reads the return value** (`viewer3.cpp:1914`,
`CM_SETDEFAULT_CODING`, buffer `DefaultConvert[200]`):

- *Before*: `DefaultConvert` received all 200 bytes and the terminator landed on
  `CViewerWindow`'s next member region (`BOOL ExitTextMode`, `viewer.h:386` —
  in practice the alignment padding after the array). The member then held an
  **unterminated** 200-char string that only "worked" because of that
  out-of-bounds NUL. `TRUE` was returned, so the branch that clears it did not
  run. On viewer close `strcmp(Configuration.DefaultConvert, DefaultConvert)`
  and `strcpy(Configuration.DefaultConvert, DefaultConvert)` (`viewer3.cpp:3618–3622`)
  then read 200 chars and wrote **201 bytes into `Configuration.DefaultConvert[200]`**
  (`cfgdlg.h:366`) — the overflow cascaded into the global configuration and
  from there into the registry (`mainwnd2.cpp:2014`).
- *After*: `FALSE` → `DefaultConvert[0] = 0`. The viewer's default coding is
  *cleared* instead of set. Downstream: `GetCodeType("")` finds nothing →
  `defCodeType = 0` → the *none* entry is the menu default (`viewer3.cpp:3301`)
  and `SetCodeType` is not applied (`viewer2.cpp:1131`) — exactly the
  documented "stored default no longer resolves" outcome the spec (US2
  scenario 1) says must stay unchanged.

**Is that a regression?** No. (a) It is FR-002 verbatim. (b) FR-009 protects
inputs "a function handles **correctly** today" — a 200-byte name is not
handled correctly today, it corrupts two objects. (c) It is unreachable with
shipped data: the longest name in the three shipped `convert.cfg` files is
33 bytes, and reaching 200 needs a hand-edited configuration file.

*One correction to the record, no code impact*: research R1's row for
`viewer3.cpp:1914` says the old code "stores a 199-byte truncated default and
reports success". It does not — it stores the **full 200 bytes** with the
terminator out of bounds, so the old default *would* have matched again later.
The change at this length is therefore "a working-but-corrupting default
becomes a cleared default", not "a useless truncated default becomes a cleared
default". The disposition is the same and FR-002 mandates it; the fix-log /
research sentence should be corrected for accuracy.

### `bufferLen <= 0`

- *Old*: the pre-clear was skipped, but `buffer[len] = 0` was **not** guarded.
  For `bufferLen == 0` and an empty name: `len = 0`, clamp not taken,
  `strncpy(…, 0)`, then `buffer[0] = 0` — **a write into a zero-length buffer**
  — and it returned **TRUE**. For `bufferLen <= 0` and a non-empty name:
  `len = bufferLen - 1` is negative and `strncpy(buffer, buff, (size_t)-1)`
  follows — unbounded copy plus NUL-padding to `SIZE_MAX`. (Not executed in the
  probe, for obvious reasons; read straight off the old source.)
- *New*: both the pre-clear and the copy are behind `if (bufferLen > 0)`, so
  **nothing is written at all**, and `nameLen < bufferLen` is false for every
  `nameLen ≥ 0` → **FALSE**. `lstrcpyn` would additionally no-op on `n <= 0`
  in both resolutions, so the guard is belt-and-braces.
- **Divergence**: `bufferLen == 0` + empty name returns FALSE where it used to
  return TRUE. Unreachable — all three callers pass a positive literal (200,
  200, 1024) — and the old TRUE came with an out-of-bounds write. Strict
  improvement; recorded, not held against the fix.

### `codeType == 0` (the "none" entry, a translated `LoadStr` string)

**Unchanged for every shipped string, hardened in general.** Old:
`strcpy(buff, LoadStr(IDS_VIEWERNONECODING))` into `buff[1024]` — a translated
string longer than 1023 bytes would have overrun the scratch (`LoadStr`'s
rotating buffer is 10000 bytes, so that was reachable in principle from a
`.slg` alone). New: the pointer is used directly and the copy is bounded by the
caller's `bufferLen`. For the actual strings (a handful of bytes in all eight
enabled languages) both produce the same bytes and the same TRUE.

Two second-order points checked and cleared:
- `LoadStr` never returns NULL (on failure it returns the static
  `"ERROR LOADING STRING"`, `salamdr2.cpp:34–83`), so the unconditional
  `strlen(name)` is safe on this branch.
- The `LoadStr` pointer is now held across **two** reads (`strlen`, then
  `lstrcpyn`) instead of one (`strcpy`). `LoadStr` hands out a slot in a
  rotating static buffer, so a concurrent `LoadStr` from another thread could in
  principle recycle it. The window widens from one pass to two over a
  ~10-byte string, and the file's own idioms hold that pointer far longer
  (`sprintf(textBuf, LoadStr(IDS_VIEWERINVALIDLINE), …)`, `codetbl.cpp:230`;
  `lstrcpyn(WinCodePage, LoadStr(IDS_VIEWERANSICODEPAGE), 101)`, `:370`).
  Not a regression; recorded as an observation.

### Does `lstrcpyn` resolve to ANSI `lstrcpynA` here? Does it matter for the bytes?

**Yes, ANSI — proven, not assumed.** `salamand.vcxproj` declares no
`<CharacterSet>` and no `UNICODE`/`_UNICODE` in any `PreprocessorDefinitions`
(invariant C1), so `winbase.h` maps `lstrcpyn` → `lstrcpynA`. Independently and
conclusively: **the same file already calls `lstrcpyn` with `char*` arguments
at `:370`, `:945`, `:1143`, `:1160` and compiles today** — a wide resolution
would be a hard type error (`char*` → `LPWSTR`). The new call is the file's own
house twin, exactly as protocol A3 requires.

`codetbl.cpp` includes `precomp.h` only and **not** `src/common/lstrfix.h`, so
the call goes to kernel32's `lstrcpynA` rather than the project's
`_sal_lstrcpynA`. It does not matter for the bytes: both copy at most `n-1`
bytes verbatim and terminate. (It matters marginally for *fault* behaviour —
kernel32's version is SEH-guarded and returns NULL on bad memory instead of
faulting — but that is the pre-existing property of every other `lstrcpyn` in
this file and reaches no new path here.)

### Executed evidence (reviewer's own probe, not the fixer's)

Independent probe (`scratchpad/rev_d1.cpp`), compiled with the product's Debug
switches (`/J /RTC1 /Od /MDd`), running the **verbatim** pre-fix and post-fix
bodies against canary-fenced arenas, sweeping every name length:

```
== bufferLen = 200 ==     (nameLen 0..300)
  n= 200  bytesDIFF  ret before=1 after=0  overrun before=+0 after=-1  scratchOverrun=-1

== bufferLen = 1024 ==    (nameLen 0..1100)
  n=1024  bytesDIFF  ret before=1 after=0  overrun before=+0 after=-1  scratchOverrun=+0
  n=1025..1100  identical result, but BEFORE overran: out=-1 scratch=+0

== bufferLen = 0, empty name ==
  before: ret=1, wrote at +0 (out of a zero-length buffer)
  after : ret=0, wrote at -1 (-1 = nothing)

divergences (excluding the pure before-overrun note): 2
```

Read: over 1,404 (length, bufferLen) combinations the caller's bytes and the
return value are **identical** except at the two `nameLen == bufferLen`
boundaries, which are the intended change, and the `bufferLen == 0` case
discussed above. The `after` column never shows an overrun; the `before` column
shows one at every `nameLen ≥ 1024` (scratch) and at both boundaries (caller's
buffer). This independently reproduces the fixer's probe result and extends it
to every length rather than four samples.

---

## B5/B6 · Failure paths and buffers

**Is `Valid(codeType)`'s guarantee sufficient for the new dereference?**
Yes. `Valid` (`codetbl.cpp:844–854`) returns TRUE only when
`Loaded && codeType >= 0 && codeType < Table->Data.Count + 1 && (codeType == 0 || Table->Data[codeType-1]->Name != NULL)`.
The new code dereferences `Table->Data[codeType - 1]->Name` **only** on the
`codeType != 0` branch, i.e. `1 ≤ codeType ≤ Data.Count`, index
`0 … Count-1` — in range — with `Name != NULL` explicitly asserted by the same
call. The `NULL` name is the separator marker, and `Valid` is precisely the
gate that excludes it. This is exactly the guarantee the **old** code relied on
for its `strcpy(buff, Table->Data[codeType-1]->Name)`; no new requirement is
introduced. `Loaded` is re-tested inside `Valid` after the function's own
`!Loaded` early-out, and `Loaded == (Table != NULL)` (`:614`), so `Table` is
non-NULL.

**Is the `buffer[0] = 0` pre-clear still done in the same order relative to
`Valid()`?** Yes — both lines are diff context, unmodified:
`if (bufferLen > 0) buffer[0] = 0;` still precedes `if (!Valid(codeType)) return FALSE;`.
So every early-return path (`!Loaded`, `!Valid`) still leaves the caller with an
empty terminated buffer exactly as before. Verified against the three callers:
C1 and C2 read `buff`/`codeName` unconditionally without checking the return
value, and both still get `""` rather than uninitialised stack on those paths.

**Any path where `buffer` is written when `bufferLen <= 0`?** **No** — both
writes are guarded by `bufferLen > 0`. This is a *removal* of such a path: the
old `buffer[len] = 0` was unguarded (see B4).

**Any new truncation introduced anywhere?** Only at `nameLen == bufferLen`,
where the old alternative was an out-of-bounds write, and it is FR-002's
mandated boundary. For `nameLen > bufferLen` the truncation point is identical
(`bufferLen - 1`); for `nameLen < bufferLen` there is none. Proven over every
length by the sweep. Downstream at C2, the new 199-byte prefix flows into
`SalLegacyToU8Alloc(codeName, 199)`, which clamps its output **on a UTF-8
sequence boundary** (`salunicode.cpp:384–390`) and can therefore never hand back
a torn character or more than 199 bytes — see B7.

**Is removing the 1024-byte scratch safe for every input?** Yes. The scratch had
exactly two roles: it received a full unbounded `strcpy` of the name, and `len`
was measured from it. `strlen(buff) == strlen(name)` by construction, so
`nameLen` replaces it exactly; nothing else read `buff`, and it was a plain
local with no address taken. Removing it deletes an overflow (proven: scratch
canary broken for every `nameLen ≥ 1024`) and changes no observable value.
Names are stored unbounded (`DupStr(name)`, `codetbl.cpp:247`, from a
user-editable `convert.cfg` line), so the removal is the load-bearing half of
the fix, not cosmetics.

**3-byte WTF-8 units / clamps on boundaries (B6 proper)**: not applicable to
this site by design — `GetCodeName` is a byte-transparent copy of
`convert.cfg`'s own bytes and must stay one (C11). The boundary-aware trimming
in this batch belongs to D4/D5. A name whose bytes happen to be UTF-8 and gets
truncated at `bufferLen - 1` may end on a partial sequence, exactly as it could
before this change for any name > `bufferLen`; the consumer that cares (C2)
re-validates and re-clamps on a boundary itself.

---

## C11 · Are the name bytes still only read and copied?

**Yes.** The source is bound as `const char* name`, so nothing can be written
through it. The only operations applied are `strlen` (read) and `lstrcpyn`
(read from `LPCSTR`, write to the caller's buffer). No `CharUpper`, no code-page
round trip, no `SalLegacyToU8*`, no space/ampersand trimming — the callers do
their own `RemoveAmpersands` on **their own copy** (C1, C2), which is
pre-existing and untouched. `Table->Data[i]->Name` remains exactly the bytes
`DupStr`'d from `convert.cfg`, which is what `EnumConversionTables`
(`zip.cpp:3289–3299`) hands to plugins as `const char** name` — a pointer
straight to that storage — and what dbviewer/filecomp persist. **No automatic
REJECT trigger.**

---

## C12 · Scope

The diff to `src/codetbl.cpp` touches **only the body of
`CCodeTables::GetCodeName`**, between its `!Valid` early-return and its closing
brace. Verified line by line:

- `CALL_STACK_MESSAGE3` — untouched (diff context).
- the `!Loaded` block — untouched.
- the `bufferLen > 0` pre-clear and the `Valid` call — untouched.
- `GetCodeType`, `CodingNameEqual`, `Valid`, `GetWinCodePage` (the immediate
  neighbours) — untouched.
- includes, file header, any other function — untouched.
- the only additions are the replacement statements plus an explanatory
  comment; no reformatting elsewhere (`.clang-format` has `ColumnLimit: 0`, so
  the 103-char line is conformant and no reflow was triggered).

**Advisory, not a finding**: at review time the working tree also carries the
in-flight edits for D2/D4 (`src/viewer3.cpp`), D3 (`src/zip.cpp`), D5
(`src/plugins/filecomp/controls.cpp`) and D6 (`run_tests.cmd`). C12 governs the
**commit**, so the D1 commit must be made with an explicit pathspec
(`git commit -- src/codetbl.cpp`) or it will sweep the neighbours in and become
a C12 violation after the fact. The `codetbl.cpp` diff itself is clean.

---

## C14 · saltests

**No `saltests` file changed.** `git status --porcelain -uall` lists five
modified product files (the six items of the batch) and three untracked files
under `specs/075-fix-small-hardening/`; nothing under `src/saltests` or
`src/common`. `git diff --name-only | grep -i saltest` is empty. Consistent
with plan.md's statement that this feature adds no test-program check because
the site is not reachable from it.

Gate spot-check run by the reviewer: `python tools\check_encoding.py --strict`
→ `TOTAL: 0 finding(s)` with the fix in the tree.

---

## B7 · Earlier scenarios

Features whose recorded scenarios could reach this function or the viewer's
*Coding* menu:

| Feature | Scenario | Reaches D1? | Can this change alter it? |
|---|---|---|---|
| 063 (Make File List encoding) | clipboard/file-list text | no | no — no path through `CCodeTables` |
| 066 (surrogate filenames) | WTF-8 file names, viewer open | only via C2's caption, and only for the *file name* half, which D1 does not touch | no |
| 067 (drive information numbers) | drive info template | no | no |
| 068 (F-P4-01 / F-P4-02, `viewer3.cpp:53`) | conversion name appended to a UTF-8 caption | **yes** (C2) | no — identical bytes for every name < 200 B; longest shipped is 33 B |
| 069 **V-14** (`quickstart.md`) | *Viewer ▸ Coding → a Kameničtí / KOI-8 ČS2 entry → Set As Default; exit, restart; view `poznámky.txt` in the Czech UI*; expected: "the default holds; the caption reads `poznámky.txt` and the translated viewer name correctly" | **yes — both C2 and C3, the return-value caller** | **no.** The Central-European names are 20–33 bytes, far below 200, so `GetCodeName` returns TRUE with byte-identical content, `DefaultConvert` is stored exactly as before, `GetCodeType` matches it on restart through 069's dual-spelling lookup, and the caption is composed from the same bytes. V-14 must still pass unchanged; it is the scenario most worth re-running by hand. |
| 069 V-21 (filecomp captions) | — | no | that is D5's surface, not D1's |

**Specific question — does the new truncation boundary interact with
`SalLegacyToU8Alloc(codeName, _countof(codeName) - 1)` at `viewer3.cpp:66`?**
No.

- The clamp argument is `199`, and `SalLegacyToU8Alloc`'s `maxBytes` clamps the
  **output** to at most 199 bytes cutting only at a UTF-8 sequence boundary
  (`salunicode.cpp:384–390`), so the `lstrcpyn(codeName, codeNameU8, 200)` that
  copies it back always fits (199 + terminator) and never truncates — the
  comment at `viewer3.cpp:63–65` is still accurate after D1.
- For every name of ≤ 198 bytes nothing changes at all (identical `codeName`).
- At `nameLen == 199`, old and new both leave the full 199-byte name — the
  sweep confirms `n=199` is byte-identical.
- At `nameLen == 200` the input to `SalLegacyToU8Alloc` becomes a 199-byte
  prefix instead of the full 200 bytes (which the old code only produced by
  overrunning the stack array). The helper is total: a prefix that is not valid
  UTF-8 takes the CP_ACP branch and still yields valid UTF-8, so
  `SalU8ToWAlloc(caption)` still succeeds and the title still renders through
  the wide path — F-P4-02's fix is **not** undone. Only the last character of a
  200-byte name would be missing, and no such name ships.
- Above 200 bytes the prefix is the same 199 bytes as before, so the helper sees
  identical input.

**B8 · Per-item path**: none. `GetCodeName` is called once per caption update
and once per dialog refresh, never per file or per line; the change removes a
1024-byte stack buffer and one full string copy, so if anything it is cheaper.

---

## Summary of divergences found (all deliberate or unreachable)

1. `nameLen == bufferLen` → FALSE + 199-byte prefix instead of TRUE + a
   one-byte overflow. **FR-002, intended.** Only `viewer3.cpp:1914` observes it;
   the result is a cleared default coding, which is the same defined state a
   non-resolving stored default already produces.
2. `bufferLen == 0` with an empty name → FALSE instead of TRUE, and no write
   instead of one out-of-bounds byte. **Unreachable** (no caller passes ≤ 0);
   strict improvement.
3. Research R1's description of case 1 at `viewer3.cpp:1914` is inaccurate (the
   old code stored the *full* name, not a 199-byte truncation). **Documentation
   only**; correct it in `research.md` / `fix-log.md`. No code consequence, and
   the disposition FR-002 dictates is unaffected.

Nothing regressed. The fix is minimal, in the file's own house shape, removes
two out-of-bounds writes (caller's buffer at the boundary, scratch buffer for
names ≥ 1024) and one negative-length `strncpy`, keeps the name bytes verbatim,
and preserves the pre-clear ordering and every early-return contract.

**VERDICT: ACCEPTED**

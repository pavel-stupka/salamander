# Fix log — feature 075 (small hardening batch)

**Branch**: `075-fix-small-hardening` · started 2026-09-02 from `c554f4d`

One record per defect, per [data-model.md](data-model.md) §1. Proofs are pasted
tool output, not prose.

---

## Phase 1 — baseline (T001–T005)

### T001 · A0 re-check at `c554f4d`

All six confirmed **present**. Line numbers at the branch tip:

| # | Site at `c554f4d` | Evidence |
|---|---|---|
| D1 | `src/codetbl.cpp:874` (`if (len > bufferLen)`), scratch `char buff[1024]` at `:864`, unbounded `strcpy(buff, …)` at `:870`/`:872` | `grep -n` output below |
| D2 | `src/viewer3.cpp:3300` (`int defCodeType;`) | ditto |
| D3 | `src/zip.cpp:3301` (`GetConversionTable`, only `table` NULL-checked) | ditto |
| D4 | `src/viewer3.cpp:31`, `:36` (`lstrcpyn(caption, …, MAX_PATH)`) | ditto |
| D5 | `src/plugins/filecomp/controls.cpp:24`, `:39` (`strcpy(Text, text);`) | ditto |
| D6 | `src/plugins/codeview/test/run_tests.cmd:25` (bare `node …test_worker.mjs`) | ditto |

```
=== D1 codetbl.cpp ===
864:    char buff[1024];
870:        strcpy(buff, LoadStr(IDS_VIEWERNONECODING));
872:        strcpy(buff, Table->Data[codeType - 1]->Name);
874:    if (len > bufferLen)
=== D2 viewer3 defCodeType ===
3300:                    int defCodeType;
=== D3 zip.cpp ===
3301:BOOL CSalamanderGeneral::GetConversionTable(HWND parent, char* table, const char* conversion)
=== D4 viewer3 clamps ===
31:            lstrcpyn(caption, FileName, MAX_PATH); // caption according to the file
36:        lstrcpyn(caption, Caption, MAX_PATH); // caption according to the plug-in request
=== D5 controls.cpp ===
24:    strcpy(Text, text);
39:    strcpy(Text, text);
=== D6 run_tests.cmd ===
25:node "%REPO%\src\plugins\codeview\test\harness\test_worker.mjs"
```

Research R0's line numbers (written against `640b94a`) all still hold; no item
is verify-closed.

### T002 · Baseline builds

`build.cmd full` (Debug x64) — **exit 0**, 0 warnings in the log,
20 plugins registered, 189 language modules, 51 s (incremental over a current
tree).

`build.cmd full release` — **exit 0**, `BUILD SUCCEEDED`, 0 warnings, 58 s.

### T003 · Gate baselines

| Gate | Baseline at `c554f4d` |
|---|---|
| `python tools\check_encoding.py --strict` | `TOTAL: 0 finding(s)` |
| `src\plugins\codeview\test\run_tests.cmd` | `RESULT: all codeview checks passed` |
| `node --version` | `v24.19.0` |
| `saltests` | `1353 checks, 0 failed` (the 069 handoff's 1301 has grown since — 1353 is this feature's baseline and must not move, contract C14) |

### T004 / T005 · Fixtures

Deferred to the human GUI pass — see T008 below. The two data fixtures
(`convert.cfg` entries; the 289-byte accented path) are only consumable by the
GUI scenarios that this session cannot drive, so they are created by the person
running the sweep, from [quickstart.md](quickstart.md) S1 and S4.

---

## Phase 2 — evidence scaffolding (T006–T008)

### T007 · Are the Debug runtime checks live? **Yes.**

The concern was that `sal_debug.props` sets only `SmallerTypeCheck` and no
`BasicRuntimeChecks`, which would mean no stack-frame checking and therefore a
S1 before-proof that cannot fire. The compiler command line settles it — from
the last full compile's tlog:

```
D:\Build\OpenSal\tandemcommander\Debug_x64\Intermediate\salamand.tlog\CL.command.1.tlog
    162 /GS      162 /J      162 /Od      162 /RTC1      161 /RTCc

D:\Build\OpenSal\tandemcommander\Debug_x64\plugins\filecomp\Intermediate\filecomp.tlog\CL.command.1.tlog
     31 /RTC1     31 /RTCc     31 _CRTDBG_MAP_ALLOC
```

`/RTC1` = `/RTCsu`, so stack-frame guard bytes are in place for D1's
`codeName[200]` and `buff[1024]`, and `_CRTDBG_MAP_ALLOC` covers D5's
heap-allocated `CFileHeaderWindow`. The property sheets have not changed since
that tlog was written (`git log --since=2026-08-25` over the three files shows
only `cb6cb41`, which added two `<ClCompile Include>` entries).

### T008 · Debugger / GUI route — **not available in this session**

S1, S2, S3, S4 and S5 as written need an interactive GUI and, for three of
them, a debugger's Watch and Immediate windows. This session can build, run
console programs and read files; it cannot drive the application's UI or a
debugger. Recorded rather than worked around, per the protocol's "a proof that
cannot fail is not evidence".

**Substitute, not replacement**: a committed probe,
[probe/probe.cpp](probe/probe.cpp), compiled by
[probe/run_probe.cmd](probe/run_probe.cmd) with the product's own Debug
switches (`/J /RTC1 /Od /MDd`). It contains the **verbatim pre-fix and post-fix
bodies** of D1, D4 and D5 (and D3's NULL path under SEH), each operating inside
an arena with canary bytes, so every read and write is defined behaviour while
the arithmetic is faithful. It gives a mechanical, repeatable before/after for
the *logic* of four defects.

It does **not** exercise the sites. The site-level evidence is (a) the
independent review of each diff, and (b) the GUI scenarios S1–S5, which remain
a human step — the same split features 070, 071 and 074 recorded.

D2 gets no runtime proof in either place: the state it needs (`Loaded == FALSE`)
is reachable only on an allocation failure, so its evidence is the static
argument in [research.md](research.md) R2 plus the review.

---

## The probe — mechanical before/after for D1, D3, D4, D5

[probe/probe.cpp](probe/probe.cpp), run by
[probe/run_probe.cmd](probe/run_probe.cmd) (compiles with the product's own
`/J /RTC1 /Od /MDd`). Verbatim pre-fix and post-fix bodies inside canary
arenas. **37 checks, 0 failed** — 31 at first; four were added to the D5 section
after its review and two to the D4 section after its review, each time the
fixture class the reviewer showed was missing. Output quoted per defect below.

Two of its own assertions were wrong on the first run and both were fixture
bugs, not fix bugs — recorded because they are the reason to write the probe
before trusting it:

1. The torn-caption fixture put the lead byte at index 259. `lstrcpyn(dst, src, 260)`
   keeps indices 0–258, so the lead was never copied and nothing tore.
   Quickstart S4's "byte 259" (1-based) is right; the probe was off by one.
   Corrected to index 258.
2. After the trim drops that lone lead byte, 258 bytes remain, not 259. The
   assertion expected 259.

---

## D1 — `CCodeTables::GetCodeName` (US1)

| Field | Value |
|---|---|
| Site | `src/codetbl.cpp:864–881` at `c554f4d` |
| Class | out-of-bounds write (two of them) |
| Status at HEAD | present |
| Per-item path | no |

**Consumers** (own `rg "GetCodeName\("`, three, matching research R1):

| Caller | Buffer | Uses return? |
|---|---|---|
| `src/dialogs3.cpp:136` | `buff[1024]`, passes 1024 | no |
| `src/viewer3.cpp:58` | `codeName[200]`, passes 200 | no |
| `src/viewer3.cpp:1914` | `DefaultConvert[200]`, passes 200 | **yes** — clears the default on FALSE |

At `nameLen == 200` the pre-fix code did **not** truncate: `len > bufferLen` is
false at the boundary, so the whole 200-byte name was copied and the terminator
written one past `Configuration.DefaultConvert`, with the stored value running
on into whatever follows it — and the call still reported success. After the
fix that case clears the default. (Corrected here after the D1 review; the
first wording of this record said "stores a 199-byte truncated default", which
was wrong about the pre-fix behaviour, though not about the disposition.)

A second, unreachable divergence: with `bufferLen == 0` and an empty name the
old code returned TRUE after writing one byte out of bounds; the new code
returns FALSE and writes nothing. No caller passes 0.

**Change**: the 1024-byte scratch and the `len > bufferLen` clamp are replaced
by one `lstrcpyn(buffer, name, bufferLen)` from the source name, returning
`nameLen < bufferLen`. The `Loaded` check, `Valid()`, the `buffer[0] = 0`
pre-clear and the `CALL_STACK_MESSAGE` are untouched, in that order.
`Table->Data[…]->Name` is read only (C11).

**Not touched**: `GetCodeType`, `Valid`, `GetCode`, `InitMenu`, everything else
in the file.

**Proof** (probe, D1 section):

```
       before(exact): return=1, wrote past the buffer at +0
ok   - D1 before: a name of exactly bufferLen writes buffer[bufferLen]
ok   - D1 before: and reports success while doing so
ok   - D1 after:  nothing written past the buffer
ok   - D1 after:  reports 'did not fit', like every longer name
ok   - D1 after:  a terminated 199-byte prefix
       before(1100 B): scratch overrun at +0
ok   - D1 before: an 1100-byte name overruns the 1024-byte scratch
ok   - D1 after:  an 1100-byte name writes nothing past the buffer
ok   - D1 after:  and reports 'did not fit'
ok   - D1 identity: a 33-byte name (the longest shipped) -- same bytes and same result
ok   - D1 identity: a real conversion name -- same bytes and same result
ok   - D1 identity: the empty name -- same bytes and same result
ok   - D1 identity: a name longer than the buffer -- same bytes and same result
```

Build: `BUILD SUCCEEDED`, `codetbl.cpp` recompiled with `/RTCc /RTC1`
(the command line in this build's own log, which supersedes the tlog reading in
T007), no compiler warning. `saltests: 1353 checks, 0 failed` — unchanged.

**Review**: [findings/review-D1.md](findings/review-D1.md) — **ACCEPTED**. The
reviewer compiled both bodies himself and swept every name length 0–300 at
`bufferLen` 200 and 0–1100 at 1024, with mixed high bytes so a multibyte-aware
`lstrcpyn` would show: bytes and return values identical at every length except
the intended boundary and the unreachable `bufferLen == 0`.
**Commit**: `8102dd8`
**Changelog**: part of the shared hardening line (R9)

---

## D2 — viewer `Coding` menu default (US2)

| Field | Value |
|---|---|
| Site | `src/viewer3.cpp:3300` at `c554f4d` |
| Class | read of an unset value |
| Status at HEAD | present |
| Per-item path | no |

**Chain**: `InitMenu` returns early (empty menu) when `!Loaded`; `GetCodeType`
is then the only assignment of `defCodeType`, and it is the one path that does
**not** assign. `Init` falls back to the best available conversion set when
`convert.cfg` is missing or damaged, so `Loaded == FALSE` is reachable only on
an allocation failure — hence no data fixture.

**Change**: `int defCodeType = 0;` plus a comment. `SetMenuDefaultItem` stays
unconditional, so the loaded-but-not-found case still highlights *none*.

**Proof**: none available — see T008. The defect is established statically
(`GetCodeType`'s non-assigning path) and the fix is a one-token initialiser
whose byte-identity for every loaded path is trivially checkable and is what
the review is asked to confirm.

**Review**: [findings/review-D2.md](findings/review-D2.md) — **ACCEPTED**. The
reviewer confirmed `GetCodeType` has exactly three exits with only `!Loaded`
non-assigning, that `Init` really falls back for a missing or damaged
`convert.cfg`, and that the loaded paths are byte-identical by
overwrite-before-read. One wording correction applied: on the unloaded path
`InitMenu` returns without adding any coding item — not the same as the menu
being empty on every opening (the `firstTime` block appends ids 6070–6101,
which cannot collide with `CM_CODING_MIN` = 871).
**Commit**: `ff1c684`
**Changelog**: `hygiene — no entry` (no reproducer with shipped data)

---

## D3 — `CSalamanderGeneral::GetConversionTable` (US2)

| Field | Value |
|---|---|
| Site | `src/zip.cpp:3301` at `c554f4d` |
| Class | missing argument check on a plugin-facing service |
| Status at HEAD | present |
| Per-item path | no |

**Chain**: a NULL `conversion` passes the `table` guard, reaches
`CCodeTables::GetCodeType`, and is dereferenced by `CodingNameEqual` inside the
lookup loop. Plugin callers: `dbviewer/renmain.cpp`, `filecomp/textio.cpp`,
`unmime/parser.cpp`, `demoplug/menu.cpp`.

**Change**: the `conversion == NULL` guard in the exact shape and message style
of the `table == NULL` guard above it. `CALL_STACK_MESSAGE2` stays first (the
same order the twin `RemoveFilesFromCache` uses). `spl_gen.h` untouched — its
documented contract already defines FALSE as *not found, `table` not valid*.

**Proof** (probe, D3 section):

```
       before: exception 0xC0000005 reaching the name comparison
ok   - D3 before: a NULL name faults inside the table lookup
ok   - D3 after:  the guard returns FALSE before any lookup happens
```

**Warning provenance** — `zip.cpp` produced `warning C4244` at line 5796 after
the change, which was not in the baseline log (the baseline build was
incremental and did not recompile the file). Proven pre-existing by a control
build with the change reverted: the same warning appears at line **5786**, ten
lines earlier — exactly the ten lines this fix adds above it.

**Review**: [findings/review-D3.md](findings/review-D3.md) — **ACCEPTED**. The
reviewer re-enumerated all seven call sites (none can pass NULL: fixed arrays,
already-guarded, or inside a comment block), confirmed the empty
`src/plugins/shared/` diff and interface 106, and separately compiled a probe
proving MSVC renders a NULL `%s` as `(null)` without tripping the
invalid-parameter handler — unchanged from before the fix, since that line
already ran first.
**Commit**: `2230787`
**Changelog**: part of the shared hardening line

---

## D4 — `CViewerWindow::SetViewerCaption` (US3)

| Field | Value |
|---|---|
| Site | `src/viewer3.cpp:31`, `:36` at `c554f4d` |
| Class | torn text → wrong rendering |
| Status at HEAD | present |
| Per-item path | no |

Written after D2 was reviewed and committed, because both edit
`src/viewer3.cpp` and each commit must revert on its own.

**Producers**: `FileName` — heap copy of `SalGetFullName` output
(`viewer.cpp:564–570`), WTF-8, up to `SAL_MAX_PATH_UTF8`. `Caption` —
`DupStr` of a plugin-supplied string; `spl_gen.h:404–411` documents **neither**
its encoding nor its length, and the D4 reviewer found three shipped plugins
(7zip, peviewer, automation) that build it as `"%s - %s"` from a UTF-8 path plus
an **ANSI** `LoadStr`. That is what makes the guard load-bearing rather than
cautious.
**Consumers of `caption`**: the two `SetWindowText*` calls at the end of the
function — the wide one through strict `SalU8ToWAlloc`, and the legacy narrow
fallback.

**Change**: both clamps keep `lstrcpyn(…, MAX_PATH)` and gain
`if ((int)strlen(src) >= MAX_PATH) SalU8TrimIncompleteTail(caption);` — the
`cmdshell.cpp:232–234` shape. Nothing else in the function moves.

**Not touched**: the `caption[0] = 0` path, the `" - "` composition,
`LoadStrU8(IDS_VIEWERTITLE)`, the encoding suffix, feature 069's F-P4-02 blocks,
the legacy fallback, and the nine ANSI `IDS_VIEWERTITLE` sites (which are in
`viewer2.cpp`, as the reviewer noted — 069 §2 item 7 says `viewer3.cpp` ×8).

**Proof** (probe, D4 section) — including the reason the trim is **guarded**:

```
ok   - D4 before: the 259-byte cut leaves a lone lead byte -- not valid UTF-8
       before: last byte = 0xC4, length 259
ok   - D4 after:  the torn tail is dropped -- valid UTF-8, the title renders
ok   - D4 after:  only the torn byte is dropped, the rest of the prefix stays
ok   - D4 after:  an untruncated code-page caption is byte-identical
ok   - D4 rationale: the UNGUARDED trim would drop its last character
       unguarded would give: "Archiv " (was "Archiv <0xE1>")
ok   - D4 identity: a 21-byte source is unchanged by the fix
ok   - D4 identity: a 8-byte source is unchanged by the fix
ok   - D4 identity: a 0-byte source is unchanged by the fix
ok   - D4 after:  a complete final character at the clamp boundary is kept
```

---

**Review**: [findings/review-D4.md](findings/review-D4.md) — **ACCEPTED**. The
reviewer proved the guard fires exactly when `lstrcpyn` truncates (measured at
lengths 257–262), swept every source length 0–259 over all bytes `0x80–0xFF` for
byte identity rather than trusting fixtures, and brute-forced the tails at the
cut: 0 blanked titles out of 255³, and 0 cases where a title that decoded
stopped decoding out of 20,000. Two record corrections applied: the bound on an
already-truncated code-page caption is **three** bytes, not one (a lead plus its
continuations, e.g. CP1250 `F9 B0 B1`); and the probe's D4 identity fixtures
were all ASCII-terminated, so they would have passed with the guard omitted — a
code-page caption and a 259-byte source were added, which is why the probe is
now 37 checks.
**Commit**: `e197a11`
**Changelog**: the user-visible *Fixed* entry (R9)

---

## D5 — `CFileHeaderWindow` (US1)

| Field | Value |
|---|---|
| Site | `src/plugins/filecomp/controls.cpp:24`, `:39` at `c554f4d` |
| Class | unbounded copy |
| Status at HEAD | present |
| Per-item path | no |

**Consumers**: twelve `SetText` calls in `mainwnd.cpp` (all pass `""`, `Path1`
or `Path2`) and two constructions. Every producer is bounded by construction —
`GetPanelPath(…, MAX_PATH)` + `SalPathAppend(…, MAX_PATH)` in `filecomp.cpp`,
or `msg->Path1[MAX_PATH]` in `remote.cpp` — so the fix is **defensive**, not a
repair of anything a user can see.

**Change**: both entry points call a new file-local
`StoreHeaderText(dst, dstSize, text)` — `lstrcpynA` plus, **only when the copy
truncated**, the walk-back over a torn UTF-8 tail — and take `TextLen` from the
stored text. A file-local helper rather than the six lines twice; the core's
`SalU8TrimIncompleteTail` is not reachable from a plugin and
`src/plugins/shared/` must not grow (C9).

### ⚠ REJECTED on first review, reworked

The first version ran the walk-back **unconditionally**. The reviewer rejected
it and was right: a header text that *fits* but carries code-page rather than
UTF-8 bytes and ends in a byte ≥ 0xC0 silently lost its last character
(`D:\Petrů` → `D:\Petr`, `D:\résumé` → `D:\résum`). That breaks FR-006 and
FR-009, and it is **reachable**: `fcremote.exe` is an ANSI build —
`fcremote/fcremote.cpp:359` calls `GetCommandLine()` and its project defines no
`UNICODE`, so `GetCommandLineA` — and its paths travel
`remote.cpp:112–121` → `mainwnd.cpp:992/1974` → `SetText` as ACP bytes. Those
are exactly the names feature 069's D03 narrow-draw fallback in this same file
exists to render.

Two lessons recorded rather than smoothed over:

1. **I wrote the guard into D4's design and then failed to apply it to D5**,
   in the same batch, on the same day. The plan named the trap; the code did
   not honour it. The independent review is what caught it — the build, the
   1353 tests and the probe were all green.
2. **The probe could not have caught it**: all three of its D5 identity
   fixtures were valid UTF-8 ending in ASCII. The missing fixture class — a
   code-page text that fits — is now in it, checked against both the guarded
   and the original unguarded helper, so this shape fails loudly from now on.

**Rework**: the `cmdshell.cpp:232–234` guarded shape the reviewer named —
`if ((int)strlen(text) < dstSize) return len;` before the walk-back.
**Re-reviewed: ACCEPTED.** The boundary was checked in both directions (258 and
259 bytes take the early return, 260 and 261 do not), the fits-set gives zero
differences against the pre-fix helper, and the truncating path is unchanged
from the version already accepted.

Two non-blocking notes from the re-review, recorded not dismissed:

1. For an **already truncated** code-page source the walk-back can still drop
   up to **three** bytes more than the clamp did — a lead byte plus the
   continuations that follow it, e.g. CP1250 `F9 B0 B1` (the D4 review
   corrected this bound; the first wording said one byte). Unreachable today,
   and that input
   overflowed the buffer before the fix, so nothing that worked is lost.
2. The GUI sweep should add an `fcremote.exe` launch on a file whose name ends
   in an accented character — feature 069's V-21 extended to the ANSI chain
   this review identified. Added to [quickstart.md](quickstart.md) S5.

**Not touched**: `CFilecompThread`'s two `strcpy`s in `filecomp.h` (same shape,
same bounded intake — C12), the WM_PAINT path and its narrow-draw fallback.

**Proof** (probe, D5 section):

```
       before: overran Text[MAX_PATH] at +0
ok   - D5 before: a 300-byte text writes past Text[MAX_PATH]
ok   - D5 after:  nothing written past Text[MAX_PATH]
ok   - D5 after:  TextLen matches the stored text, not the argument
ok   - D5 after:  torn input still writes nothing past the buffer
ok   - D5 after:  the stored text does not end on a lone lead byte
ok   - D5 identity: a 0-byte text is stored unchanged
ok   - D5 identity: a 8-byte text is stored unchanged
ok   - D5 identity: a 32-byte text is stored unchanged
```

Build: `BUILD SUCCEEDED`, `controls.cpp` recompiled with `/RTCc /RTC1
_CRTDBG_MAP_ALLOC`, no warning.

**Review**: [findings/review-D5.md](findings/review-D5.md) — REJECTED, reworked,
**ACCEPTED** (details above).
**Commit**: `1ffdf3b`
**Changelog**: part of the shared hardening line

---

## D6 — `run_tests.cmd` (US4)

| Field | Value |
|---|---|
| Site | `src/plugins/codeview/test/run_tests.cmd:25` at `c554f4d` |
| Class | environment-dependent test verdict |
| Status at HEAD | present (latent — green on this machine's Node 24) |
| Per-item path | no |

**Change**: `--experimental-detect-module` on the worker-harness invocation,
plus a header paragraph stating the Node floor and why the flag is there.
Nothing under `web/` touched (C13).

**Proof before** — the Node 20 default, emulated on Node 24:

```
> node --no-experimental-detect-module src/plugins/codeview/test/harness/test_worker.mjs
Error [ERR_REQUIRE_CYCLE_MODULE]: Cannot require() ES Module
    E:\Projects\tandemcommander\src\plugins\codeview\web\worker.js in a cycle.
exit=1
```

No real Node 20 run: neither `nvm` nor `fnm` is installed on this machine and
`npx node@20.18.0` produced no usable binary. Recorded as not performed, per
quickstart S6.

**Proof after**:

```
> src\plugins\codeview\test\run_tests.cmd
 RESULT: all codeview checks passed
```

**Still fails when it should** — a planted defect in `web/worker.js`
(`throw new Error("075 D6 planted defect")` appended), then restored with
`git checkout`:

```
 RESULT: FAILURES -- see above
```

`python src\plugins\codeview\test\check_data.py` → `All data checks passed.`
(identical to baseline); `git status --porcelain src/plugins/codeview/web/` →
empty.

**Review**: [findings/review-D6.md](findings/review-D6.md) — **ACCEPTED with a
required comment correction**, applied: module detection became the default in
Node **22.7**, not 22.12 (22.12 is when `require(esm)` became stable), and the
first version of the header contradicted itself within the same paragraph. The
same error was corrected in `plan.md`, `spec.md`, `research.md` and `tasks.md`.
The reviewer verified the harness is not silenced (planted syntax error →
`RESULT: FAILURES`, exit 1; restored and SHA256 re-checked) and that the
worker-harness output on Node 24 is byte-identical with and without the flag.
**Open, as research R6 anticipated**: no Node 20 runtime on this machine, so
the Node 20 half of SC-005 stays unverified.
**Commit**: `eecfca5`
**Changelog**: `hygiene — no entry` (developer-only)

---

## Changelog text (T052) — for the ship gate, NOT applied here

To be added under the next unreleased version, in the same change as the
version and build bump (`spl_vers.h`, `tandemcommander.iss`, `CLAUDE.md`).
`LAST_VERSION_OF_SALAMANDER` (106) does not move.

### Fixed

- **The viewer's title bar is readable for files under very long paths.**
  Opening a file whose full path is longer than about 260 bytes showed the
  title — the file name, the word *Viewer* and the coding — with garbled
  accented characters. The name was being cut in the middle of a character,
  which made the whole title fall back to the legacy code page. Paths at or
  below that length were never affected.

- **Hardening, with no known way to trigger it.** Three internal copies that
  could write past their storage were bounded: the viewer's lookup of a
  conversion name, the File Comparator's file header, and the argument check of
  the conversion service offered to plugins. No shipped configuration reaches
  any of them — the longest conversion name in `convert.cfg` is 33 bytes and
  every path shown in the comparator is already length-limited — so nothing
  users have seen is being repaired here.

Deliberately **not** in the changelog: D2 (no reproducer with shipped data —
the state needs an allocation failure) and D6 (a developer-only test runner).

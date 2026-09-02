# Review — D5 · `CFileHeaderWindow` bounded header text

**Reviewer**: independent (did not write the fix) · **Date**: 2026-09-02 ·
**Branch**: `075-fix-small-hardening` · **Diff reviewed**:
`git diff -- src/plugins/filecomp/controls.cpp` (working tree, uncommitted)

**Protocol**: `specs/075-fix-small-hardening/contracts/fix-protocol.md`
Parts B and C (069 Part B/C verbatim). Governing FRs: spec FR-006, FR-009.

**Two rounds.** Round 1 (+35 / −4) was **REJECTED** for an unconditional
walk-back that damaged code-page header texts which fit. Round 2 (+44 / −4)
adds the guard and is **ACCEPTED**. Sections B1–C12 below are round 1 and stand
as written — the consumer map, the producer chains and the buffer analysis are
unchanged by the rework; §"Round 2" at the end re-runs B4, B5/B6, B7, C9 and
C12 against the new code and carries the binding verdict.

---

## B1 · Consumers, re-enumerated independently

Own `rg`/`grep` over `src/plugins/filecomp/` (not the planning list).

### (a) Entry points

| # | Site | Argument |
|---|---|---|
| 1 | `src/plugins/filecomp/mainwnd.cpp:223` | ctor, `""` |
| 2 | `src/plugins/filecomp/mainwnd.cpp:240` | ctor, `""` |
| 3–6 | `mainwnd.cpp:714, 715, 1015, 1016` | `SetText("")` |
| 7–8 | `mainwnd.cpp:1439, 1440` | `SetText("")` |
| 9–10 | `mainwnd.cpp:1997, 1998` | `SetText("")` |
| 11–12 | `mainwnd.cpp:992, 993` | `SetText(Path1 / Path2)` — text compare |
| 13–14 | `mainwnd.cpp:1974, 1975` | `SetText(Path1 / Path2)` — binary compare |

Two constructions, twelve `SetText` calls. Eight of the twelve pass `""`.
No class derives from `CFileHeaderWindow` (`grep "public CFileHeaderWindow"` →
none), so `Text`/`TextLen` are visible only inside `controls.cpp`.

### (b) Producers — traced to first construction

`CMainWindow::Path1/Path2` are **pointers** (`mainwnd.h:54–55`, *not* arrays as
one might assume), aimed at `CFilecompThread::Path1/Path2` (`filecomp.h:81–82`,
`char[MAX_PATH]`) by `mainwnd.cpp:25–26` from `filecomp.cpp:708`. They are
overwritten in place at `mainwnd.cpp:990/991` and `1972/1973` from
`res->Files[n].Name`.

Four independent chains reach `CFilecompThread::Path1`:

| Chain | First construction | Bounded? | Encoding |
|---|---|---|---|
| P1 · panel menu command | `filecomp.cpp:497–598`: `char file1[MAX_PATH]`, `SG->GetPanelPath(…, MAX_PATH, …)` + `SG->SalPathAppend(…, MAX_PATH)` | **yes**, ≤ 259 B | UTF-8 (interface 104) |
| P2 · Compare Files dialog | `dialogs.cpp:104–106` `ti.EditLine(IDE_PATH1, Path1, MAX_PATH)` → `winliblt.cpp:1128–1142` | **yes**, ≤ 259 B | UTF-8 **normally**; the documented `WM_GETTEXT` fallback ("when the UTF-8 result would not fit the caller's buffer, fall back to the legacy A read") stores **ACP** bytes |
| P3 · drop on the dialog edit | `dialogs.cpp:141–143` `DragQueryFile` (A build → ACP) → `SetWindowText` → re-read by P2 | yes | UTF-8 after the round trip |
| P4 · `fcremote.exe` remote control | `fcremote.cpp:184–267`: `char argv[4][MAX_PATH]` from `GetCommandLine()` (**A**), `GetCurrentDirectory(MAX_PATH, …)` (**A**), `lstrcpyn(msg.Path1, …, MAX_PATH)` → `remote.cpp:112–121` `strcpy(Path1, msg->Path1)` + `SalGetFullName` | yes, ≤ 259 B | **ACP** — `fcremote` is an ANSI build (no `CharacterSet`/`UNICODE` in `fcremote.vcxproj`; the `const char*` signatures prove the A forms) |

The result-side hop is also bounded: `worker.cpp:151–152` `strcpy(Files[i].Name,
name0)` into `CWorkerFileData::Name[MAX_PATH]` (`worker.h:261`); the text path
carries it through `CTextFileReader::GetName()` (`cwbase.cpp:28, 80`,
`worker2.cpp:38`) which returns a pointer into that same `[MAX_PATH]` field.

**Verdict on R5's claim** — "every path arrives through a MAX_PATH buffer":
**length-wise, confirmed** on all four chains; nothing over 259 bytes can reach
the header today, so D5 really is defensive. **Encoding-wise, R5 and plan D5
are wrong**: chains P2-fallback and P4 deliver **code-page** bytes, not UTF-8.
This matters, because the new code makes an encoding decision (§B5).

### (c) Readers of `Text` and `TextLen`

| Reader | Member | Note |
|---|---|---|
| `controls.cpp:122` `SplU8ToW(Text, buff, _countof(buff))` | `Text` | strict (`MB_ERR_INVALID_CHARS`, `splunicode.h:70–82`) — fails on ACP bytes |
| `controls.cpp:135` `lstrcpynA(narrow, Text, …)` + `PathCompactPathA`/`DrawTextA` | `Text` | the feature-069 D03 fallback; renders ACP bytes **correctly** |
| — | `TextLen` | **no reader at all** |

`TextLen` is a write-only member of this class. The two `TextLen` uses at
`controls.cpp:444` and `:463` belong to a **different** class, `CToolTipWindow`
(`controls.h:74–75`, `char Text[10]`), and are untouched by the diff.

---

## B2 · Per-surface verdict

| Surface | Verdict | Proof |
|---|---|---|
| ctor `CFileHeaderWindow("")` ×2 | **unchanged** | `len==0` → walk-back loop not entered, `if (i > 0)` false; `Text=""`, `TextLen=0`, identical to `strcpy`+`strlen` |
| `SetText("")` ×8 | **unchanged** | same; `InvalidateRect`/`UpdateWindow` untouched |
| `SetText(Path1/Path2)`, chain P1 (panel command) | **unchanged** *for valid UTF-8*; **regressed** for the one shape below | see §B5 — a valid-UTF-8 path is never touched by the walk-back; but P1 cannot produce a non-UTF-8 path, so P1 alone is clean |
| `SetText(Path1/Path2)`, chain P4 (`fcremote.exe`) | **REGRESSED** | an ACP path whose last byte is ≥ 0xC0 loses its final character — §B4/§B5 |
| `SetText(Path1/Path2)`, chain P2 fallback (very long typed path) | **REGRESSED** | same shape, same cause |
| `> 259`-byte argument (any chain) | **corrected** | was a heap overrun; now a terminated prefix |
| WM_PAINT wide draw | **unchanged** | not in the diff |
| WM_PAINT narrow fallback (069 D03) | **unchanged as code**, but now hands it a text one character short in the P4/P2-fallback case | §B7 |
| `CToolTipWindow::Text/TextLen` | **unchanged** | different class, not in the diff |

---

## B4 · Byte identity, argued

The walk-back is deterministic, so I transcribed `StoreHeaderText` byte-for-byte
into a simulator and ran the boundary set myself (logic-level, exactly the
probe's own methodology; `lstrcpynA(dst,src,n)` modelled as "≤ n−1 bytes + NUL"):

```
SAME  empty                          inlen   0 old   0 new   0
SAME  ascii                          inlen  12 old  12 new  12
SAME  utf8, complete 2-byte tail     inlen  13 old  13 new  13   tail C5 AF
SAME  utf8, complete 3-byte tail     inlen  10 old  10 new  10   tail E4 B8 AD
SAME  utf8, complete 4-byte tail     inlen  11 old  11 new  11   tail F0 9F 98 80
DIFF  CP1250 "…\Petrů" (tail 0xF9)   inlen  12 old  12 new  11   -> "…\Petr"
DIFF  CP1252 "…\résumé" (tail 0xE9)  inlen  13 old  13 new  12   -> "…\résum"
DIFF  CP1250 tail 0xC8               inlen   8 old   8 new   7
SAME  CP1250 accent mid-string       inlen  16 old  16 new  16
SAME  259 ascii                      inlen 259 old 259 new 259
DIFF  259, last byte 0xE9            inlen 259 old 259 new 258
DIFF  260 ascii (old overflowed)     inlen 260 old 260 new 259
SAME  all continuation bytes (80 80) inlen   2 old   2 new   2
```

- **Shorter than MAX_PATH** — byte-identical **only if the text is valid UTF-8
  or ends below 0xC0**. Three of the DIFF rows above are texts that *fit* and
  that the pre-fix code stored and displayed correctly. This breaks FR-006
  ("MUST store and display a text that fits **unchanged**"), FR-009 and the
  protocol's own B4 row for D5 ("any text < 260 bytes: `Text` byte-identical,
  `TextLen` identical").
- **Empty string** (the most common call, 8 of 12 sites) — **unchanged**.
  `strlen("")==0`, the `while` guard `i > 0` and the `if (i > 0)` both stop
  immediately; `Text[0]=0`, `TextLen=0`.
- **Exactly MAX_PATH−1 = 259 bytes** — `lstrcpynA(dst, src, 260)` copies all
  259 + NUL, so nothing is clamped; identical **unless** the 259th byte is
  ≥ 0xC0 without a complete sequence (row 11 above), which is again the
  regression, not a boundary artefact.
- **MAX_PATH or more** — before: `strcpy` wrote 260+ bytes into `Text[260]`,
  running over `TextLen`, `BkColor`, `BkgndBrush` and off the end of the heap
  block (`CFileHeaderWindow` is `new`-allocated); `TextLen` was `strlen(argument)`,
  i.e. a value larger than the buffer. After: a 259-byte prefix (minus a torn
  tail), NUL-terminated, `TextLen == strlen(Text)`. **Intended change**, matches
  FR-006's "terminated prefix cut on a character boundary".
- **`TextLen` from the STORED text instead of the ARGUMENT** — no reader is
  harmed, because **`CFileHeaderWindow::TextLen` has no reader** (§B1c). In the
  only case where the two values differ (argument ≥ 260 bytes) the object was
  already corrupted before the fix, so there is no working behaviour to
  preserve. This part of the change is sound.
- **Walk-back vs `SalU8TrimIncompleteTail`** (`src/common/salunicode.cpp:612–630`)
  — compared line by line:

  | Core | D5 helper | Same? |
  |---|---|---|
  | `int len = (int)strlen(buf);` | `int len = (int)strlen(dst);` | yes |
  | `int i = len;` | `int i = len;` | yes |
  | `while (i > 0 && ((unsigned char)buf[i-1] & 0xC0) == 0x80) i--;` | identical on `dst` | yes |
  | `if (i > 0) { lead = …; if (lead >= 0xC0) { seqLen = lead >= 0xF0 ? 4 : (lead >= 0xE0 ? 3 : 2); if (len - (i-1) < seqLen) buf[i-1] = 0; } }` | identical, plus `len = i - 1;` | yes (the extra `len` update is required for the return value and is correct) |

  **The transcription is faithful.** The divergence is not in the six lines —
  it is in **where they are called from** (§B5).

---

## B5 / B6 · Failure paths and buffers

- **`lstrcpynA` correct here?** Yes. The plugin has no `<CharacterSet>` /
  `UNICODE` in `filecomp.vcxproj` or `plugin_base.props`, so `lstrcpyn` would
  already be the A form; spelling `lstrcpynA` explicitly is stricter and matches
  the twin at `controls.cpp:135`. `lstrcpynA` copies at most `n−1` bytes and
  always terminates, and is SEH-guarded against a faulting source.
- **`_countof(Text)`?** Correct: `char Text[MAX_PATH]` → 260, and for an A-form
  API the count is bytes. (It would silently become wrong if `Text` were ever
  widened to `TCHAR`; not the case today.)
- **Can the walk-back read before the buffer?** No. `while (i > 0 && …)` and
  `if (i > 0)` both dominate every `dst[i-1]` access, so the lowest index read
  is 0. The all-continuation-bytes input (`80 80`) walks `i` to 0 and exits
  cleanly — verified above.
- **Can it write outside?** No. The only write is `dst[i-1] = 0` with
  `1 ≤ i ≤ len ≤ 259`.
- **Pure ASCII** — untouched (last byte < 0x80 → loop not entered, `lead < 0xC0`).
- **Valid UTF-8 ending in a multi-byte character that FITS** — **not damaged**;
  the 2-, 3- and 4-byte tail rows above all come back SAME. Correct.
- **Code-page bytes ending in a byte ≥ 0xC0 that FITS** — **DAMAGED. This is the
  finding.** For `dst[len-1] ≥ 0xC0` with no continuation bytes after it,
  `len − (i−1) == 1 < seqLen` is always true (`seqLen` is 2, 3 or 4), so the
  byte is unconditionally deleted. In CP1250 that is `À Á Â Ă Ä Ĺ Ć Ç Č É Ę Ë Ě
  Í Î Ď Đ Ń Ň Ó Ô Ő Ö Ř Ů Ú Ű Ü Ý Ţ ß à á â ă ä ĺ ć ç č é ę ë ě í î ď đ ń ň ó ô
  ő ö ř ů ú ű ü ý ţ` — i.e. essentially every accented letter; CP1252 is the same
  range.

**Is the unconditional walk-back a defect here? Yes.** Three independent reasons:

1. **`Text` is not UTF-8 by contract.** The fix's own comment asserts "'Text' is
   a UTF-8 path (interface 104)", but the code eight lines below it — the
   feature-069 D03 fallback — exists *precisely because it is not*, and the
   plan's D5 paragraph says so in the same breath ("the paint path already falls
   back to the narrow draw when the text is not UTF-8"). Chain P4
   (`fcremote.exe`, an ANSI build reading `GetCommandLineA` /
   `GetCurrentDirectoryA`) and the P2 `WM_GETTEXT` fallback both deliver ACP
   bytes. Protocol **C5** names that fallback *load-bearing for D5*.
2. **The core helper's own contract is narrower than its use here.**
   `salunicode.h:189–198`: "safe to call unconditionally **on any UTF-8
   buffer**". Every core call site honours that — `dialogs5.cpp:1077`
   (`Name`, UTF-8 by the feature-052 metadata contract), `fileswn3.cpp:293` and
   `salamdr5.cpp:399` (`LoadStrU8` template + UTF-8 path), `salamdr4.cpp:824`
   (`VIEW_NAME_MAX` field, UTF-8). D5 is the first call site whose input is not
   UTF-8 by contract.
3. **The protocol names the guarded shape and the fixer used the unguarded
   one.** A3's twin for D4 is `cmdshell.cpp:232–234`, which reads
   `lstrcpyn(shown, program, 2*MAX_PATH); if (strlen(program) >= 2*MAX_PATH)
   SalU8TrimIncompleteTail(shown);` — trim **only when a clamp actually
   happened**. D5 trims on every call, including calls where nothing was
   clamped and therefore nothing can be torn.

**Impact**: display-only (the header bar), no data written to disk, no crash —
but it is a *working* surface today (069 D03 fixed exactly this bar so it
"keeps its text on any input rather than dropping it"), and D5 makes it drop the
last character of accented remote-launched paths. That is a regression of a
surface the reviewed change was not supposed to touch at all.

**Minimal remedy** (not applied — reviewer does not edit the fix): guard the
walk-back with the D4 twin's condition, e.g.

```
    lstrcpynA(dst, text, dstSize);
    int len = (int)strlen(dst);
    if ((int)strlen(text) >= dstSize)   // only a clamp can tear a character
    {
        ... existing six lines ...
    }
    return len;
```

With that guard all three "fits but DIFF" rows above return to SAME, the
overflow fix and the `> 259`-byte behaviour are unaffected, and FR-006/FR-009
hold for every input.

**Evidence gap that let this through**: the probe's identity fixtures
(`probe/probe.cpp:426`) are `""`, `"C:\a.txt"` and
`"D:\Zkou\xC5\xA1ka\M\xC5\xAFj disk\soubor.txt"` — all valid UTF-8 and all
ending in ASCII. It never tests a fitting text ending in a *complete* multi-byte
character, and never tests a fitting *code-page* text. The three checks named
"D5 identity: a N-byte text is stored unchanged" therefore cannot fail on the
one shape that matters.

---

## B7 · Earlier scenarios

Feature 069 has two fixes in this file's neighbourhood: **D03**
(`controls.cpp` WM_PAINT — the bar used to blank itself when `SplU8ToW` failed;
it now falls back to `PathCompactPathA` + `DrawTextA` on a local copy) and
**D04** (`worker2.cpp`, the caption). Recorded in
`specs/069-finish-encoding-fixes/closing-report.md:211–216`, task T081, manual
scenario V-21.

- The D03 fallback is at `controls.cpp:127–138`. **Not in the diff — unchanged
  as code.**
- **Still reachable?** Yes, and reached by exactly the inputs §B5 identifies:
  `SplU8ToW` is strict, so any ACP `Text` takes the fallback.
- **But its result is now degraded**: for chain P4 it renders a path whose final
  accented character D5 deleted. 069's own acceptance sentence for D03 is "the
  path bar keeps its text on any input rather than dropping it" (spec scenario,
  quickstart V-21). D5 partially re-opens that: not the whole text, one
  character. Re-running V-21 with an `fcremote.exe`-launched accented path is
  the check this fix should have carried.

Feature 068 has no work in this file (`grep` over
`specs/068-encoding-regression-review/` → filecomp appears only in the deferred
cluster B-5 discussion).

---

## B8 · Per-item path

None. `StoreHeaderText` is one `static` function used by both entry points; no
new branch, no configuration flag, no per-caller special case.

---

## C9 · Plugin ABI

- `git diff --stat -- src/plugins/shared/` → **empty**. No shared header, source
  or property sheet changed.
- `LAST_VERSION_OF_SALAMANDER` = **106** in `src/plugins/shared/spl_vers.h:246`,
  unchanged (not in the diff).
- `src/plugins/filecomp/filecomp.def` — **no diff**; no new export.
- `controls.h` — **no diff**; `Text`, `TextLen` and both member signatures are
  as they were. `StoreHeaderText` is file-local `static`, so it adds no symbol.
- Nothing plugin-facing crosses the boundary: the change is entirely inside the
  plugin's own translation unit.

**C9: satisfied.**

---

## C12 · Scope

- The diff touches **exactly** the two `CFileHeaderWindow` entry points
  (`controls.cpp:54–59` ctor, `:68–74` `SetText`) plus the new file-local
  `static int StoreHeaderText` and its comment block. Nothing else in
  `controls.cpp` moved (`CSplitBarWindow`, `CToolTipWindow`, `CComboBox`, the
  WM_PAINT body are all outside the hunks).
- `CFilecompThread::CFilecompThread`'s `strcpy(Path1, file1)` /
  `strcpy(Path2, file2)` in `src/plugins/filecomp/filecomp.h:89–90` —
  **confirmed NOT touched** (`git diff --stat -- src/plugins/filecomp/` lists
  `controls.cpp` only). Correctly recorded as "seen, not changed" in
  `fix-log.md`.
- `mainwnd.cpp:990/991/1972/1973`'s `strcpy`s — also untouched.

**C12: satisfied for the D5 diff.**

*Process note, not a reject reason*: the working tree carries an uncommitted
`src/codetbl.cpp` change (D1) alongside this one. C12 requires one commit per
item; nothing is committed yet, so commit separation cannot be verified from
here. Flagging so the fixer keeps the two staged apart.

---

## Standing invariants

| # | Status |
|---|---|
| C1 (core ANSI-built) | n/a — plugin-local |
| C2 (`SalU8ToW` strict) | the plugin twin `SplU8ToW` is strict (`MB_ERR_INVALID_CHARS`) — this is *why* §B5 matters |
| C4 (`LoadStr`/`LoadStrU8`) | no `LoadStr` in the diff |
| C5 (tolerant sink's fallback is load-bearing for D5) | **violated in effect** — the fallback still runs, but is now fed a shortened string |
| C7 (`/J`, `char` unsigned) | the walk-back casts to `unsigned char` anyway; correct either way |
| C9 | satisfied (above) |
| C10 (clusters out of scope) | respected — the fix does not attempt to fix the ACP producers; but it must not *damage* their output either |
| C14 (no `saltests` count change) | none added; correct — the site is not reachable from the test program |

---

## Summary (round 1)

One concrete regression, one evidence gap.

1. **Regression (blocking)** — `StoreHeaderText` applies the UTF-8 walk-back
   **unconditionally**, including on calls where `lstrcpynA` clamped nothing.
   For a header text that fits (< 260 bytes) but is code-page rather than UTF-8
   and ends in a byte ≥ 0xC0, the final character is deleted. Reachable through
   `fcremote.exe` (ANSI build → ACP command line and current directory →
   `remote.cpp:112–121` → `CFilecompThread::Path1` → `SetText`), and through the
   documented `WM_GETTEXT` fallback in `CTransferInfo::EditLine`. Before the fix
   these paths displayed correctly via the feature-069 D03 narrow-draw fallback.
   Violates FR-006 ("store and display a text that fits unchanged"), FR-009 and
   the protocol's B4 identity row for D5; the A3 twin (`cmdshell.cpp:232–234`)
   shows the guarded shape that avoids it.
2. **Evidence gap** — the probe's three D5 identity fixtures are all valid UTF-8
   ending in ASCII, so no check in the batch can fail on the shape above; and
   no check covers a fitting text ending in a *complete* multi-byte character
   either (that one happens to be correct, but it is unproven).

Everything else is right: the overflow is genuinely closed, the buffer
arithmetic is safe at both ends, the six transcribed lines are faithful to
`SalU8TrimIncompleteTail`, `TextLen` has no reader so sourcing it from the
stored text is harmless, the empty-string case (8 of 12 calls) is bit-identical,
the ABI is untouched, and the scope is exactly the two entry points plus one
static helper.

**ROUND 1 VERDICT: REJECTED** *(superseded by round 2 below — kept as the
record)* — regressed surface: `CFileHeaderWindow::SetText` / `Text` for a
**non-UTF-8 (code-page) header text that fits**, i.e. the `fcremote.exe`
remote-launch chain (`fcremote.cpp:263–267` → `remote.cpp:112–121` →
`mainwnd.cpp:992/993`, `1974/1975`) and the `CTransferInfo::EditLine`
`WM_GETTEXT` fallback (`winliblt.cpp:1141`): a path ending in an accented
character (CP1250 `ů` 0xF9, CP1252 `é` 0xE9, any byte ≥ 0xC0) loses that
character from `Text` and from the header bar, where the pre-fix code and the
feature-069 D03 fallback displayed it in full. The walk-back must be guarded by
the D4 twin's condition — trim only when `strlen(text) >= dstSize`, i.e. only
when the clamp actually cut something. Also missing: an identity fixture for a
fitting code-page text and for a fitting text ending in a complete multi-byte
character.

---

# Round 2 — re-review of the reworked fix

**Diff re-read**: `git diff -- src/plugins/filecomp/controls.cpp`, +44 / −4,
one file. The change against round 1 is three lines inside the helper plus six
comment lines:

```
    lstrcpynA(dst, text, dstSize);
    int len = (int)strlen(dst);
    if ((int)strlen(text) < dstSize)
        return len; // nothing was clamped - leave the text exactly as it arrived
    int i = len;
    ... walk-back, byte-for-byte as in round 1 ...
```

This is the remedy named in round 1 and the predicate of the A3 twin
(`cmdshell.cpp:232–234`, "trim iff `strlen(src) >= clamp`") expressed as an
early return. The comment block now records the ANSI `fcremote.exe` chain, the
"code-page bytes ending ≥ 0xC0" case and that those names are what the 069 D03
narrow-draw fallback exists to render — so the next reader will not delete the
guard as redundant.

## B4 (round 2) · Byte identity, re-argued

Re-transcribed the reworked helper into the simulator and re-ran the boundary
set, extended with the shapes round 1 flagged:

```
== INPUTS THAT FIT (must equal the pre-fix strcpy) ==
SAME  empty                              0 B
SAME  ascii                             12 B
SAME  utf8 complete 2-byte tail (C5 AF)  13 B
SAME  utf8 complete 3-byte tail (E4 B8 AD) 10 B
SAME  utf8 complete 4-byte tail (F0 9F 98 80) 11 B
SAME  CP1250 tail 0xF9  "D:\tmp\Petrů"   12 B    <- round 1 said DIFF
SAME  CP1252 tail 0xE9  "D:\tmp\résumé"  13 B    <- round 1 said DIFF
SAME  CP1250 tail 0xC8                    8 B    <- round 1 said DIFF
SAME  CP1250 accent mid-string           16 B
SAME  torn utf8 tail that FITS (…C5)     12 B
SAME  lone continuation bytes (80 80)     9 B
SAME  258 ascii                         258 B
SAME  259 ascii  (== dstSize-1)         259 B
SAME  259, last byte 0xE9               259 B    <- round 1 said DIFF
SAME  259, ends complete 2-byte utf8    259 B
fits-cases that differ: 0
```

- **Shorter than MAX_PATH** — now byte-identical for **every** input, UTF-8 or
  not, well-formed or not. FR-006's "store and display a text that fits
  unchanged" and FR-009 hold unconditionally. The three round-1 DIFF rows and
  the 259-byte row are all SAME again.
- **Empty string** (8 of the 12 `SetText` calls) — unchanged: `lstrcpynA`
  writes `""`, `len = 0`, `strlen(text) = 0 < 260` → early return, `TextLen = 0`.
  Identical to `strcpy` + `strlen`.
- **Exactly MAX_PATH−1 = 259 bytes** — unchanged, and now unchanged *for any
  byte content*, because the early return fires before the walk-back can see it.
- **MAX_PATH or more** — the intended change, and **identical to the round-1
  code I already accepted on this path**; see the regression check below.
- **`TextLen`** — still taken from the stored text. Unchanged reasoning:
  `CFileHeaderWindow::TextLen` has no reader (§B1c), and the early return sets
  it to `strlen(dst)`, which for a fitting text equals `strlen(text)` — the
  pre-fix value exactly.

**Is the boundary exactly right?** Yes, and it is exact, not approximate.
`lstrcpynA(dst, src, n)` truncates **iff** `strlen(src) >= n`. The guard takes
the early return **iff** `strlen(text) < dstSize`. The two predicates are
complements of each other over the same quantities, so the walk-back runs on
exactly the truncated copies and on nothing else. Traced per branch:

```
len(text)=258 (dstSize-2)  -> EARLY RETURN   stored 258   lstrcpyn clamped: no
len(text)=259 (dstSize-1)  -> EARLY RETURN   stored 259   lstrcpyn clamped: no
len(text)=260 (dstSize)    -> walk-back      stored 259   lstrcpyn clamped: YES
len(text)=261 (dstSize+1)  -> walk-back      stored 259   lstrcpyn clamped: YES
```

The two cases the coordinator asked about are the middle pair: **259 bytes takes
the early return** (nothing was clamped — `lstrcpynA` with `n = 260` copies all
259 plus the NUL) and **260 bytes does not** (one byte was dropped). An
off-by-one in either direction would be visible here: `<=` would skip the trim
on the first genuinely truncated length, `<` on `dstSize - 1` would trim an
untruncated 259-byte text. The code has neither.

## B5 / B6 (round 2) · Failure paths and buffers

- **The truncating path is unchanged from round 1.** I ran the reworked helper
  and the round-1 helper side by side over nine over-length inputs — 260/300
  ASCII, clamps landing 1-of-2, 1-of-3, 2-of-3 and 1-of-4 bytes into a
  character, a clamp just after a complete character, and two long ACP paths:
  **0 differences**. So nothing I accepted in round 1 about the overflow fix,
  the torn-tail removal or the stored length has moved.
- **Pure ASCII** — early return, untouched.
- **Valid UTF-8 ending in a complete multi-byte character that FITS** — early
  return, untouched (2-, 3- and 4-byte tails all SAME above). Round 1 reached
  the same result the long way, through the `seqLen` arithmetic; now the guard
  makes it unreachable, which is strictly safer.
- **Code-page bytes ending ≥ 0xC0 that FIT** — **fixed**. Early return; the byte
  survives; the 069 D03 narrow draw renders the name in full as before.
- **Out-of-bounds** — unchanged and still impossible: the walk-back's every
  `dst[i-1]` access is dominated by `i > 0`, the only write is `dst[i-1] = 0`
  with `1 ≤ i ≤ len ≤ 259`, and it now runs on strictly fewer inputs than in
  round 1. `lstrcpynA` + `_countof(Text)` = 260 bytes: still correct for an
  A-form API on `char Text[MAX_PATH]`.
- **New source read** — `strlen(text)` is now evaluated on the *argument* as
  well as the destination. No robustness change: the pre-fix code already did
  `strcpy(Text, text)` and `strlen(text)`, and `CALL_STACK_MESSAGE2("…(%s)",
  text)` dereferences `text` before either. A NULL or unterminated argument was
  a crash before and is a crash now; all 14 call sites pass a literal or a
  `[MAX_PATH]` array.
- **Residual, recorded and accepted**: for an *already truncated* **code-page**
  source, the walk-back can still drop one byte more than the clamp did (a
  279-byte ACP path clamped at a 0xF9 stores 258 bytes, not 259). This is not a
  regression — that input overflowed the buffer before the fix, so there is no
  working behaviour to preserve; FR-006 asks only for "a terminated prefix cut
  on a character boundary", and no producer chain can deliver more than 259
  bytes today (§B1b). Nothing to do.

## B7 (round 2) · Earlier scenarios

The feature-069 D03 narrow-draw fallback (`controls.cpp:127–138` before the
rework, unchanged code, now at `:136–147`) is still outside the diff, still
reachable, and — the point of the rework — **is again handed the full text**.
069's acceptance sentence for D03, "the path bar keeps its text on any input
rather than dropping it", holds again for the `fcremote.exe` chain. The
round-1 objection is closed. Feature 068 still has no work in this file.

## C9 / C12 (round 2) · ABI and scope

- `git diff --stat -- src/plugins/shared/` → **empty**. `LAST_VERSION_OF_SALAMANDER`
  still 106. `controls.h`, `filecomp.h` and `filecomp.def` → **no diff** (checked
  explicitly); no new export, `StoreHeaderText` is still file-local `static`.
- `git diff --stat -- src/plugins/filecomp/` → `controls.cpp` only. The
  neighbouring `CFilecompThread` `strcpy`s (`filecomp.h:89–90`) and the
  `mainwnd.cpp` `strcpy`s are still untouched, as C12 requires.
- The diff is still exactly the two entry points plus the one static helper and
  its comment.
- *Process note (unchanged, still not a reject reason)*: the working tree now
  carries D2/D3/D4/D6 changes as well; C12's one-commit-per-item is a staging
  discipline that cannot be verified from an uncommitted tree.

## Evidence (round 2)

I ran the probe myself: `specs\075-fix-small-hardening\probe\run_probe.cmd` →
**`probe: 35 checks, 0 failed`**. The gap round 1 named is closed by two new
D5 checks that assert both directions on the same fixture:

```
ok   - D5 after:  a code-page text that FITS is stored unchanged (guarded trim)
ok   - D5 regression: the UNGUARDED trim drops its last character -- why the guard exists
       unguarded would give: "D:\Petr" (was "D:\Petrů")
```

`SetText_afterUnguarded` is retained beside `SetText_after`, so the batch's own
evidence now *demonstrates* the defect rather than being silent about it, and
`D:\résumé` and `D:\Petrů` are in the identity list. The probe's
`SetText_after` is a faithful transcription of `StoreHeaderText` — the only
difference is that it recomputes `strlen(Text)` at the end instead of carrying
`len = i - 1`, which is the same value after `dst[i-1] = 0`.

Minor, non-blocking: the identity list still contains no fitting text ending in
a *complete* multi-byte UTF-8 character. My own simulation covers it (SAME), and
the guard now makes that whole class take the early return, so the fixture would
prove little. Optional at most.

The site-level GUI evidence remains a human step, as `fix-log.md` T008 records.
The one scenario worth adding to the sweep: launch the comparator through
`fcremote.exe` on a file whose name ends in an accented character (e.g.
`fcremote.exe "D:\tmp\Petrů" "D:\tmp\Novák"`) and confirm the header bar shows
the full name — that is feature 069's V-21 extended to the chain round 1
identified.

## Verdict (round 2)

Every round-1 objection is answered by the code, not by argument: the three
fitting-text identities that differed are byte-identical again, the guard's
boundary is exact in both directions, the truncating path is bit-for-bit what I
had already accepted, the 069 D03 fallback is whole again, the comment records
why the guard must stay, and the probe now fails on the shape instead of missing
it. ABI and scope are clean.

**VERDICT: ACCEPTED**

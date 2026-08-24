# Regression review — X16–X20 (groups C2, C4, C1, C3, C6) and X21–X22 (C5, D02)

Two independent reviewers, neither of whom wrote the fixes. Charter:
`contracts/fix-protocol.md` Part B. **Both returned REJECTED.** Between them
they found **eight** regressions the fixes themselves introduced, plus one
defect that meant a fix did not actually fix its finding. Every one is
corrected below; nothing was argued away.

---

## Review A — X16–X20 · **REJECTED** → corrected

| # | Sev | Site | Finding | Disposition |
|---|---|---|---|---|
| **R1** | **CRITICAL** | `pack1.cpp:1699` | `SalConvertFindDataW` was called **once, before** the `.`/`..` skip loop; the loop advanced the wide record but never refreshed the UTF-8 name, so the test kept re-reading `"."` and spun until `FindNextFile` failed. **Every external-archiver unpack failed — ASCII archives included** — and the extracted files were then deleted with the temp tree. | **fixed**: re-convert inside the loop after each `SalFindNextFile` |
| **R2** | **CRITICAL** | `pack1.cpp:1966` | The identical defect in `PackUnpackOneFile`: F3-view or extract-one-file from `.rar/.arj/.lzh/.uc2/.ace` always failed. | **fixed**, same way |
| **R3** | **HIGH** | `dialogs3.cpp:1631` | **Stack buffer overflow.** `userName` is `char[100]`, but the new code told `SalWToU8` the destination was `MAX_PATH` while `WNetGetUserW` was allowed to fill 259 chars — smashing the two `BOOL`s next to it. The pre-fix ANSI call was correctly bounded at 100. | **fixed**: the wide buffer is capped to 100 and `sizeof(userName)` is passed |
| **R5** | **HIGH (data)** | `dialogs3.cpp:1333` | **The Drive Information dialog could rename the volume.** `OldVolumeName` became the *true* label while `IDE_VOLNAME` sits in an ANSI dialog and reads back `?` for anything outside the code page, so `Validate` saw a difference and called `SetVolumeLabel` — a drive labelled in Cyrillic/Greek/CJK was renamed to `??????` merely by opening the dialog and pressing OK. | **fixed**: the reference is now seeded from the control, so it is compared against what the user actually sees |
| **R4** | **HIGH** | `pack3.cpp:1443` | `SalGetShortPathName` returns `BOOL`, not a length, but the code compared it with `strlen(buff)` — the 8.3 branch became dead for every install, ASCII included, on a line whose purpose is DOS-program output redirection. | **fixed** |
| **R6** | MEDIUM | `stswnd.cpp:1471`, `toolbar5.cpp:173` | The surviving `path[l] = 0` (a WCHAR count) truncated the longer UTF-8 result by one byte per non-ASCII character, so the drop still failed — the surface the record claimed corrected was not. | **fixed**: the terminator now belongs to the legacy branch only, where one byte per character holds |
| **R7** | MEDIUM | `salunicode.cpp` | `SalU8ToOEM` omitted `WC_NO_BEST_FIT_CHARS`, so the API silently transliterated (`ž`→`z`) and left `usedDefault` FALSE — the exact opposite of the guarantee its own header documents, and it made two of the new unit checks machine-dependent. | **fixed**: flag added; and case (3) rewritten to hold on a CJK OEM code page too ("either it fails cleanly, or it round-trips exactly") |
| **R8** | MEDIUM | `pack1.cpp:329` | DC-09: converting the archive listing's **name** to UTF-8 while its **path** stayed code-page left a tree with mixed components; the in-place guard could only ever be satisfied by pure ASCII, so the conversion was a no-op exactly where it was needed. | **fixed**: the path is converted into its own buffer and *that* is what `AddFile`/`AddDir` receive |
| **R9** | LOW | `dialogs6.cpp:648` | The "stop sharing" confirmation still composed an ANSI template with the now-UTF-8 share name; its suppression comment's premise had just been invalidated by X20. | **fixed** (`LoadStrU8`) |
| **R10** | LOW | `shares.cpp` | With `MAX_PATH` buffers a long accented remark could fail **both** conversions, dropping the share from the list and losing its marker. | **fixed**: buffers sized `3 * MAX_PATH` |
| **R11** | LOW | `drivelst.cpp:1352` | The helper fell back to the ANSI call when the *wide call* failed, costing two volume-information round trips per inaccessible drive on a timeout-sensitive per-item path. | **fixed**: the fallback is now for a failed *conversion* only, matching its `salamdr2.cpp` twin |
| **R12** | record | `mainwnd5.cpp:1239` | The comment claimed the return value "is honoured"; it is not — only the initializer makes the behaviour defined. | **fixed**: comment corrected to say what the code does |
| **O6** | note | `pack1.cpp` | The rewritten extension scan started one byte later than the original, changing ASCII behaviour for a trailing dot. | **fixed**: original start restored |

## Review B — X21–X22 · **REJECTED** → corrected

| # | Sev | Site | Finding | Disposition |
|---|---|---|---|---|
| **R3** | **HIGH** | `zip.cpp:3375` | **FR-005 violation.** `CSalamanderGeneral::GetTargetDirectory` is a plugin-facing service, and making the core's `::GetTargetDirectory` return UTF-8 changed the bytes plugins receive. `undelete` and `pictview` (both shipped `on`) put that value straight into ANSI dialog controls and **pictview persists it into its own configuration**. | **fixed**: converted back at the forwarder with the new `SalU8ToACP`, so the plugin's bytes are exactly what they were — the deferral shape FR-012 prescribes |
| **R1** | **HIGH** | `mainwnd3.cpp:2945` | Export Configuration's Save-As default folder: `CreateOurPathInRoamingAPPDATA` now yields UTF-8 and it went straight to `lpstrInitialDir` of an **ANSI** `OPENFILENAME`. Under an accented account the dialog opened nowhere. | **fixed**: converted back at that sink |
| **R2** | **HIGH** | `salamdr6.cpp:1709,1730` | The same shape in both ANSI common-dialog recovery paths — a file task T038 named and the implementation never touched. A localized Documents folder worked before and was mojibake after. | **fixed**: both sites |
| **R4** | MEDIUM | `zip/dialogs.cpp:1852,1940` | **D02 did not fix D02**: the plugin's own two overwrite dialogs still set `IDC_FILEATTR` with a raw ANSI `WM_SETTEXT`, so the stray `Â` remained there; and the producer change regressed non-ASCII *regional formats* (ko-KR, ar-SA), whose date/time bytes had rendered correctly as ACP. | **fixed**: both sinks now use the plugin's `SetDlgItemTextU8` with the ANSI call as fallback — the same shape as the `IDC_FILE` neighbour, which repairs the finding *and* the regional-format case |
| **R5** | LOW | `shellib.cpp:2985` | `buff` can now exceed `pathLen`, and `lstrcpyn` cuts on a byte boundary. | **fixed**: `SalU8TrimIncompleteTail` — the helper this feature added for exactly this |
| **R6** | LOW | `salamdr2.cpp:2934` | `SalCreateFile` reports `ERROR_INVALID_NAME`, which the `ignoreIfNotExists` suppression did not list — a silent startup would have grown an error box. | **fixed**: code added to the suppression |
| **N2** | note | `salamdr5.cpp:1891` | `SalCreateDirectory` has no narrow fallback, so the legacy branch stopped creating the folder (an A4 deviation). | **fixed**: narrow fallback added |
| **N3** | note | `salamdr5.cpp:1881` | The `static` buffer's "called from the exception handler" comment was stale, and the fix had added stack to that function without re-validating it. | **fixed**: comment corrected |

## What both reviewers confirmed as sound

The parts that were hardest to get right came back clean: the wide multi-string
in `CopyFilesTo` is correctly aligned and double-NUL terminated; `HANDLES`
bookkeeping is right at every new site (`SalFindFirstFile` never wrapped in
`HANDLES_Q`, always closed with `HANDLES(FindClose)`); `SalConvertFindDataW` is
called before first use on every iteration in the *other* four loops (including
across `packac.cpp`'s `continue`); all 16 name renames landed; every `free`
path is balanced; `SalRegQueryValueExW8` preserves the callers' `type`/`size`
semantics exactly; the command-line fix genuinely moves no selection offset; and
the plugin ABI is untouched (`src/plugins/shared/` absent from every diff,
interface 106).

## Re-cut verdict

All fourteen findings of Review A and all eight of Review B are fixed above.
Gates after the corrections: build 0 errors with no new warnings in any changed
file; `saltests` **1288 checks, 0 failed** (one fewer than before because the
CJK case now branches on the machine's OEM code page instead of asserting a
single outcome); `check_encoding.py --strict` **TOTAL: 0**; draft 149.

**Two lessons worth carrying forward**, both of which cost a rejection here:

1. **A conversion inside a loop must be inside the loop.** R1/R2 are the same
   one-line slip in two places, and it broke a whole feature for every user, not
   just for accented names. The four loops that were written correctly used the
   `do { convert; … } while (next)` shape; the two broken ones converted before
   a `while`.
2. **"Plugin-facing" includes what the core *returns* to a plugin.** The D02
   analysis correctly established that a plugin *sending* better-formed text to
   a tolerant sink is safe — and then the same feature changed a service's
   *return* bytes two files away without noticing it was the same rule.

---

## Review C — the corrections themselves · **REJECTED** → corrected

The correction commit (`bcd2cfa`) was new, unreviewed code, which is the shape
that had already failed twice. A third reviewer was given only that diff and the
charter. It rejected it, and it was right on both counts.

| # | Sev | Site | Finding | Disposition |
|---|---|---|---|---|
| **F1** | **CRITICAL** | `pack1.cpp:334` → `zip.cpp:5888` | **An external-archiver listing could stop opening at all.** `AddFile`/`AddDir` refuse a path or name over `MAX_PATH - 5`, and here that refusal is *fatal* — `PackScanLine` turns it into `IDS_PACKERR_FDATA` and `PackList` aborts. The OEM form could never trip it (one byte per character, inside `filename[MAX_PATH]`); the UTF-8 form is up to three times longer. A `.rar`/`.arj`/`.lzh`/`.ace`/`.uc2` holding a ~130-character Cyrillic path, or an ~86-character Japanese one, went from *lists, as mojibake* to *error box, empty panel*. The R8 path fix and the earlier name fix each contributed one half. | **fixed**: name and path are now decided **together**, and the decision weighs the **length** as well as the conversion — if either converted form would be refused, both fall back to the pre-069 conversion. Degrading the encoding is allowed by the charter; failing the operation is not. |
| **F2** | MEDIUM | `shares.cpp:102` → `shares.cpp:19` | R10's widening moved the failure rather than removing it: `CSharesItem` `lstrcpyn`s the path into a `MAX_PATH` buffer, which cuts on a **byte** boundary. A ≥87-character CJK share path used to fail the UTF-8 conversion, fall to the complete code-page form and render correctly through the tolerant sink's ANSI fallback; after R10 it converted, got torn mid-sequence, and rendered as chopped mojibake. | **fixed**: `path` alone returns to `MAX_PATH` (`netname`/`remark` keep the widening they actually needed — they are `DupStr`'d, not copied into a fixed buffer), with the asymmetry explained at the declaration |
| **F3** | MEDIUM | `salunicode.cpp` | A5 evidence missing: `SalU8ToACP` was added to fix three HIGH findings and shipped with **no check at all**, while three sinks branch on its exact contract. | **fixed**: 13 checks — ASCII identity, the never-truncate property that matters for `lpstrInitialDir`, NULL in both positions, the legacy pass-through branch and its size limit, and a code-page-independent bounds/termination check |
| **O1** | LOW | `stswnd.cpp:1470`, `toolbar5.cpp:172` | A comment **I added** asserted "one byte per character", which is false on any DBCS code page — and it sat over a *pre-existing* overrun: the destination was sized from the **source** length while every caller passes `char[MAX_PATH]`, so a 300-character drop wrote 301 bytes into 260. | **fixed**: the bound now comes from the buffer and the terminator from the API's own return value. Pre-existing, but two characters of code and a wrong comment of mine. |
| **O2** | LOW | `salunicode.cpp:576` | Under Windows' "Use Unicode UTF-8 worldwide" setting `GetOEMCP()` is `CP_UTF8`, for which `WideCharToMultiByte` **rejects** both a non-zero flag set and a non-NULL `lpUsedDefaultChar` — so `SalU8ToOEM` would fail for *every* name, ASCII included, pinning the archivers on the legacy path and failing unit case (1). | **fixed**: both arguments are dropped when the OEM code page is UTF-8, where neither has any meaning |
| **O3** | note | `salamdr5.cpp:1895` | The narrow retry is unconditional, not conversion-gated as its comment claimed. The reviewer judged the behaviour itself harmless (the leaf is always ASCII, so an unconvertible parent fails either way). | **comment fixed**, not the code — gating it would need a validity probe this layer does not have and an allocation to free, for no behavioural gain |
| **O4** | note | `dialogs6.cpp:650` | The `mixed-composition` suppression went stale when the site moved to `LoadStrU8`. | **removed** |
| **O5** | record | `pack3.cpp:1444` | The revived 8.3 branch returns an *unquoted* path, so a packer under `D:\My Tools\…` breaks where 8.3 creation is disabled. Correct against the shipped baseline (the branch was alive pre-069; only the parent commit had it dead), so this is pre-existing. | **recorded**, not changed — it is a packer-quoting defect, not an encoding one |

### What Review C confirmed

R1/R2 are genuinely fixed — `SalConvertFindDataW` re-runs after every
`SalFindNextFile` at `pack1.cpp:1719` and `:1989`, the pre-loop conversion
covers the first record, and the reviewer independently re-checked the other
four loops (`pack2.cpp:531`, `packac.cpp:662`, `pack3.cpp:1263`, `pack1.cpp:1695`)
and the `goto _ERR` handle close. Also confirmed: the `salamdr2.cpp` suppression
parses as intended (`&&` binds tighter); `drivelst.cpp` returns `FALSE` with the
buffer untouched and both callers respect it; the only allocation in the diff is
freed on its single path; every tolerant-sink fallback still does the whole job;
`zip.cpp:3382` cannot truncate against the documented `MAX_PATH` contract; and
`SafeGetOpenFileName`/`SafeGetSaveFileName` are *also* plugin services whose
pre-069 bytes the fix restores — which the record had not said, and now does.

### Gates after Review C's corrections

Debug **0 errors**, Release **0 errors**, no warning in any changed file;
`saltests` 1288 → **1301 checks, 0 failed**; guard strict **TOTAL: 0**, draft
149; both configurations launch, paint a correct title and close with exit 0.

### The count that matters

Three reviews, three rejections, **thirty findings** — and the two most severe
in the whole feature (both "the feature stops working entirely, for everyone")
were introduced by *fixes*, and one of those by a *fix to a fix*. Every one was
caught before the work was called done. No automated gate in this project would
have caught any of them.

---

## Review D — the corrections to the corrections · **REJECTED** → corrected by **reverting**

A fourth reviewer, given only the Review-C correction diff. It rejected that too,
and its lead finding is the reason the archive-listing conversion is **not**
shipping in this feature.

| # | Sev | Site | Finding | Disposition |
|---|---|---|---|---|
| **D1** | **HIGH** | `pack1.cpp:321` → `zip.cpp:5771` | **The panel grows a second folder of the same name, with the files divided between them.** `useU8` can only be decided *per item*, but the directory components it yields are shared *between* items, and `CSalamanderDirectory::FindDir` matches them with `SalDirStrCmpEx` — a **byte** comparison. So one item falling back spells its directory differently from its siblings and a second node is created. Both spellings render correctly (one natively, one through the tolerant sink's A fallback), so it reads as data loss rather than as mojibake. Reachable on Japanese Windows with an 86-character name, Cyrillic at 128, Thai at 86 — and more easily through the *path*, which splits at a shared ancestor. | **fixed by reverting** the listing-side conversion — see below |
| **D2** | MEDIUM | `salunicode.cpp:574` | **My O2 "fix" was wrong on both counts.** Its premise (MSDN: `CP_UTF8` rejects a non-zero flag set and a non-NULL `lpUsedDefaultChar`) is false on the supported platform, and the change *deleted a guard that works*: under `CP_UTF8` `usedDefault` is set for an **unpaired surrogate** — the one thing UTF-8 also cannot express — so dropping it removed the only detector. A feature-066 lone-surrogate file would have been written into the list file as `EF BF BD` and **silently left out of the archive**. | **reverted**, and the measurement recorded in the comment so it is not "fixed" again |
| **D3** | LOW | `pack1.cpp:321` | The name-length guard fired even when `pomptr2 == NULL`, where `AddFile` applies no limit at all. | moot — the code is reverted |
| **D4** | LOW | `dialogs6.cpp:650` | O4 removed the first line of a two-line suppression and orphaned the second. | **fixed** |

I verified D2 independently rather than taking it on trust, by calling
`WideCharToMultiByte` on this machine with code page 65001 and a lone surrogate:
all four flag/`usedDefault` combinations return 5 with `usedDefault = 1`. The
reviewer is right and MSDN describes older behaviour.

### Why F-P1-05's listing half is reverted rather than fixed again

The reviewer offered a narrower repair — let the *path* decide and let an
over-long *name* fall back on its own. It is a genuine improvement, but it
still splits the tree whenever the **path** is the thing that overflows, and it
reintroduces the mixed name/path chain the fix existed to remove. Three separate
attempts at this one conversion produced, in order: a fatal listing abort, a
tree split, and a narrower tree split. That is the signal to stop.

So the listing stays in the active code page, exactly as it shipped, and the
reason is written at the site. **What this does not cost**: the user-visible
half of F-P1-05 — the list file handed to the external archiver, and the unpack
side — is fixed and does not depend on the listing's encoding. That is the half
the changelog describes and the half the finding was raised for. The listing's
*display* encoding moves as a whole or not at all, and it belongs with the rest
of the archive-name work in REMAINING-WORK.md.

### Categories Review D checked and found clean

`useU8` on every path including `pomptr2 == NULL`; `pathU8` never read
uninitialized; `newfile.NameLen` correct in both branches; nothing reading
`filename` after the skipped in-place `OemToChar` (it is a per-call local);
the extension scan byte-identical for ASCII; `newfile.Name` freed on all five
error returns; buffer worst case 778 bytes into 780. **`shares.cpp`**: the
`MAX_PATH` sizing is byte-for-byte the pre-069 one, `SalGetFullName` is a pure
byte function so an ACP-fallback path is not dropped, and the `netname`/`remark`
asymmetry is justified (no fixed-buffer sink; `NNLEN` caps a share name at 80).
**`stswnd`/`toolbar5`**: `wrote` does include the terminator, so `path[wrote-1]`
is already the NUL and the index is bounded by `MAX_PATH-1`; on `wrote == 0` the
empty path is rejected downstream and no drop is accepted; all six call sites do
pass `char[MAX_PATH]`. **The 13 `SalU8ToACP` checks** are code-page independent
as written. Plugin ABI untouched; nothing newly persisted; no refuted finding
re-opened.

### Gates after Review D's corrections

Debug **0 errors**, Release **0 errors**, no warning in any changed file;
`saltests` **1301 checks, 0 failed**; guard strict **TOTAL: 0**, draft 149; both
configurations launch, paint a correct title and close with exit 0.

# Regression review — X04, X05, X06, X07

Reviewer: independent regression reviewer (did not write these fixes).
Baseline: `c577ff3`. Diff reviewed: `git diff c577ff3 -- src/filesmap.cpp
src/mainwnd2.cpp src/cache.cpp src/salamdr3.cpp`.
Build: `build.cmd` (Debug x64) — **BUILD SUCCEEDED**, 0 errors, 70 warnings,
none of them in `filesmap.cpp` / `mainwnd2.cpp` / `cache.cpp` / `salamdr3.cpp`
(all 70 are pre-existing `LNK4217` from the vendored libssh2 in the sftp plugin).

Read-only on the product: nothing under `src/` was modified.

---

## Summary of verdicts

| Fix | Verdict | One-line reason |
|---|---|---|
| X04 — F-MC-01 `src/filesmap.cpp` | **ACCEPTED** | Buffer, `- 1` arithmetic and the `s == NULL` path are all correct; ASCII widths byte-identical; only consumer is rubber-band selection |
| X05 — F-P4-04 `src/mainwnd2.cpp` | **ACCEPTED** | `RegEnumValue(…,NULL,NULL)` is documented-legal, `strlen(path)+1` is provably ≤ MAX_PATH and now always NUL-terminated (strictly safer than before); ASCII load byte-identical. One cosmetic double-message-box on an unreachable error path |
| X06 — F-P1-01/02 `src/cache.cpp` | **ACCEPTED (with a required companion change)** | The four converted sites are correct and `Path`/`TmpName` are provably UTF-8. **But** the fix converted only one of the two `GetTempPath` sites in the file; leaving `cache.cpp:1473` ANSI is what turns X07 into a regression (see below) |
| X07 — F-P1-04 `src/salamdr3.cpp` | **REJECTED** | Regressed surface: `CDiskCache::ClearTEMPIfNeeded` (`cache.cpp:1471-1546`) feeds `RemoveTemporaryDir` a **CP_ACP** path from the un-converted `GetTempPath` at `cache.cpp:1473`; the now-strict facade rejects it, so the startup "delete leftover SAL\*.tmp directories" cleanup silently stops working under a non-ASCII `%TEMP%` — it *worked* before this fix |

**Safety answer up front (X07): no. The new `_RemoveTemporaryDir` cannot delete
anything it should not.** The walk is provably confined to `dir`'s subtree
(proof in §X07.3). Its regression is that it deletes *less*, never more.

---

# X04 — F-MC-01 · `src/filesmap.cpp` (`CFilesMap::CreateMap`)

## 1. Consumers re-enumerated (own `rg`, not the fixer's list)

`CFilesMap::CreateMap()` has **exactly one** caller:

| Consumer | Location | What it drives |
|---|---|---|
| `CFilesWindow::…` begin rubber-band drag | `src/fileswn9.cpp:1313` | `FilesMap.CreateMap()` then `SetAnchor` / `SetPoint` |
| rubber-band paint / teardown | `src/fileswn0.cpp:544` (`DrawDragBox`), `:557` (`DestroyMap`) | consume the map built above |
| panel binding | `src/fileswn1.cpp:1388` (`SetPanel`) | — |

`CFilesMapItem::Width` (the only field the change affects) is read only by
`CFilesMap::PointInRect` (`src/fileswnd.h:235`) / `GetMapItem` — i.e. the
hit-test of the rubber-band selection rectangle. **No other surface consumes it**:
it is not the drawing width (that is computed independently in
`src/fileswn4.cpp:711-760`), not the column width, not persisted.
The affected-surface list is therefore complete and small.

## 2. Buffer sizing — `WCHAR wbuf[SAL_FIND_NAME_U8]`

- Source is `char formatedFileName[SAL_FIND_NAME_U8]` (`filesmap.cpp:107`),
  `SAL_FIND_NAME_U8 == 3 * MAX_PATH == 780` (`src/common/salfileio.h:31`).
- WTF-8 → UTF-16 never expands: **at most 1 WCHAR per byte**. For `len` bytes
  the requirement is `len + 1` WCHARs. `len` is `f->NameLen`
  (`spl_com.h:222`, `unsigned`, UTF-8 byte count) or `f->Ext - f->Name - 1`,
  both ≤ `strlen(formatedFileName)` ≤ 779 (the name must fit its own 780-byte
  buffer). Requirement ≤ 780 = `_countof(wbuf)`. **Fits exactly in the worst
  case.**
- Even if it did not: `SalU8ToW` is bounds-checked on both paths
  (`MultiByteToWideChar` with `bufSize`, and `SalWtf8ToWBytes` returns `-2`
  on `out + n > bufSize`, `salunicode.cpp:170/183`), returning 0 → the ANSI
  fallback. **Overflow is impossible.**
- Stack cost: +1560 bytes in `CreateMap`, allocated once (declared in the loop
  body, but the compiler hoists the frame slot). Not a concern.

Verdict: **unchanged / safe**.

## 3. The `- 1` on the explicit-`srcLen` form

Contract (`src/common/salunicode.h:47`): *"Number of WCHARs/bytes written
including the terminating null"*. Implementation for `srcLen >= 0`
(`salunicode.cpp:253-265`): after `MultiByteToWideChar` returns `res` (which
for an explicit `srcLen` does **not** count a terminator), the wrapper writes
`buf[res] = 0` and does `res++`. So the return really is
*characters + 1*, and `- 1` yields the exact character count.

Reference use is identical: `src/fileswn4.cpp:711`
`wNameLen = SalU8ToW(TransferBuffer, nameLen, wbuf, _countof(wbuf)) - 1;`
followed by `if (wNameLen >= 0) GetTextExtentPoint32W(hDC, wbuf, wNameLen, …)`.

Verdict: **correct**, and byte-for-byte the same idiom as the already-validated
panel drawing path.

## 4. The `s == NULL, len == 0` (".." item) case

`filesmap.cpp:121-123` sets `s = NULL` for the up-dir item and `len = 0`.

- `SalU8ToW(NULL, 0, wbuf, 780)` hits the explicit `src == NULL` guard
  (`salunicode.cpp:238-243`): writes `wbuf[0] = 0`, returns **0**. No
  dereference of `src`, **no crash**.
- `wLen = 0 - 1 = -1` → `wLen >= 0` false → `GetTextExtentPoint32(dc, s, len, &sz)`
  with `(dc, NULL, 0, &sz)` — **byte-identical to the pre-fix call**.

Same for the (unreachable) `s != NULL, len == 0` case: `MultiByteToWideChar`
returns 0, the `res == 0 && srcLen != 0` guard is false so the WTF-8 fallback
is skipped, return 0 → ANSI fallback with the identical arguments.

> Pre-existing note (not a regression): when GDI fails on
> `GetTextExtentPoint32(dc, NULL, 0, &sz)`, `sz` is left uninitialized and
> `width += sz.cx + 4` reads it. Identical before and after the fix.

Verdict: **unchanged**.

## 5. ASCII / English byte-identity of the resulting widths

For a pure-ASCII name, `GetTextExtentPoint32A` internally converts the bytes to
UTF-16 through the DC's code page and calls the same W engine; the ASCII range
maps identically in every shipped ACP. `SalU8ToW` on ASCII produces exactly the
same UTF-16 units. **Same font, same units ⇒ identical `sz.cx`.**
An ASCII panel therefore produces a byte-identical `CFilesMapItem::Width` array.

Verdict: **unchanged for ASCII / English UI**; **corrected** for non-ASCII names
(the byte count was previously used as a character count, over-sizing the
hit-test rectangle — and, worse, making the map disagree with what
`fileswn4.cpp` actually drew).

## 6. Per-item path / timing

`CreateMap` is a **per-item** loop (one conversion + one `GetTextExtentPoint32W`
per panel item), but it runs **once per rubber-band drag start**, only when
`Configuration.FullRowSelect == FALSE`, and the pre-fix code already paid an
internal ACP→UTF-16 conversion inside `GetTextExtentPoint32A`. The added cost is
one explicit `MultiByteToWideChar` replacing an implicit one ⇒ **delta ≈ 0**.
The Fix record should *state* this rather than carry measured numbers; flagging
it as a documentation gap only, not a defect.

## 7. Plugin-facing impact

None. `CFilesMap` is core-internal (`src/fileswnd.h:193`), not exported through
`src/zip.cpp` or `spl_*.h`.

## 8. Previously validated behavior touched

066/067 quickstarts on lone-surrogate names: the up-dir/measure path now
converts WTF-8 the same way the drawing path does, so a surrogate name's
hit-test box finally matches its drawn box. No 058/062/063 scenario touches
`CFilesMap`.

## Verdict X04 — **ACCEPTED**

No regressed surface. Buffer proven adequate, `- 1` proven correct against the
implementation (not just the header), the `NULL`/`0` path proven to reach the
identical ANSI call, ASCII widths byte-identical. Only gap: the Fix record
should note the per-item timing rationale (§6).

---

# X05 — F-P4-04 · `src/mainwnd2.cpp` (`CMainWindow::LoadConfig`, default directories)

## 1. Consumers re-enumerated

The only writer/reader pair for `HKCU\…\Default Directories`:

| Role | Location | Encoding |
|---|---|---|
| writer | `src/mainwnd2.cpp:1345` `SetValue(actKey, name, REG_SZ, path, -1)` where `path = DefaultDir[d-'A']` | UTF-8 → `SetValueAux` (`regwork.cpp:196`) → `SalRegSetValueExW8` |
| reader (changed) | `src/mainwnd2.cpp:2648-2657` | now `RegEnumValue` (name) + `GetValue` (data) |
| consumer of `DefaultDir` | `char DefaultDir['z'-'a'+1][MAX_PATH]` (`src/consts.h:1410`), read by the change-drive path (`src/fileswnd.h:981`) | UTF-8 |

Writer and reader are now symmetric — that is the point of the fix. Nothing else
enumerates that key.

## 2. Is `RegEnumValue(…, NULL, NULL)` legal, and does it still return the type?

Yes. `RegEnumValueA`'s documented contract: *lpData* "can be NULL if the data is
not required"; *lpcbData* "can be NULL only if lpData is NULL". `lpType` is
independent and still filled. Consequently:

- `ERROR_SUCCESS` is still returned for every existing index.
- `type` is still `REG_SZ` for the drive values.
- It **removes** a failure mode: `ERROR_MORE_DATA` can no longer be raised for
  the *data* (only for the 2-byte name buffer, exactly as before).

Verdict: **unchanged / corrected**.

## 3. `GetValue` failure semantics vs. the old code — the one real behavior delta

`GetValue` (`src/salamdr2.cpp:2283`) → `CRegistryWorkerThread::GetValue`
(`regwork.cpp:520`) → `GetValueAux(…, quiet = FALSE)` (`regwork.cpp:115`) →
`SalRegQueryValueExW8` (`src/salamdr6.cpp:2310`).

`GetValueAux` **itself shows a message box** on failure, unless the failure is
`ERROR_FILE_NOT_FOUND`. The caller then also shows `IDS_UNEXPECTEDVALUE`.
So on a failing value the user now sees **two** modal boxes where the old code
showed **one**.

Reachability of that path (this decides whether it is a real regression):

- The only realistic failure is `ERROR_MORE_DATA`, i.e. the UTF-8 form of the
  stored value exceeding `MAX_PATH` bytes.
- The value can only have been written by this application, through
  `SetValue(… , -1)` → `dataSize = strlen(DefaultDir[i]) + 1` with
  `DefaultDir[i]` being `char[MAX_PATH]`. **So the stored UTF-8 length is
  always ≤ MAX_PATH** and `needed ≤ MAX_PATH` always holds.
- There is no config import from Open Salamander / Newt Commander
  (`CLAUDE.md`, feature 046), so no foreign producer can have written a longer
  value into `HKCU\Software\Tandem Commander\0.1`.
- Remaining route: a hand-edited registry. Cosmetic (an extra message box)
  in a case that already ended in an error box and a lost value.

Verdict: **unchanged in every reachable configuration**; a cosmetic
double-box in a hand-edited-registry corner. Recorded as a **Note**, not a
regression.

## 4. `dataLen = strlen(path) + 1` vs. the old `dataLen` — and the `memmove`

`SalRegQueryValueExW8` (`salamdr6.cpp:2340-2385`) does the following for
`REG_SZ`: it **strips every stored terminator**, appends exactly one of its own,
converts `wlen + 1` units, sets `needed = SalWToU8(…, NULL, 0) - 1` (i.e. the
byte count *including* that one terminator), refuses with `ERROR_MORE_DATA`
when `*lpcbData < needed`, and `memcpy(lpData, u8, needed)`.

Therefore, on success:

- `path` is **guaranteed NUL-terminated** ⇒ `strlen((char*)path) + 1 == needed`
  exactly. No out-of-bounds `strlen`.
- `needed ≤ bufferSize == MAX_PATH` ⇒ `dataLen ≤ MAX_PATH` ⇒
  `memmove(DefaultDir[d2-'a'], path, dataLen)` into `char[MAX_PATH]`
  **cannot overflow**.

This is **strictly safer than the old code**: the old `dataLen` came straight
from `RegEnumValue` and a REG_SZ value stored *without* a terminator would have
been `memmove`d unterminated into `DefaultDir[…]`, leaving a non-terminated
`char[MAX_PATH]` for every later `strlen`. That hole is now closed by
construction.

Verdict: **corrected**.

## 5. ASCII / English byte-identity

Traced for `D:\Work\Proj`:

- old: `RegEnumValueA` → 13 bytes (`"D:\Work\Proj"` + NUL), `dataLen = 13`.
- new: `RegQueryValueExW` → 12 units, `wlen = 12`, `terminators = 1`,
  `needed = 13`, `memcpy` 13 bytes, `strlen(path)+1 = 13`.

**Byte-identical, same `dataLen`, same `memmove`.** Verified for the whole ASCII
range (`SalWToU8` is the identity on ASCII).

## 6. Does the loop still terminate if `GetValue` fails for one drive?

Yes. The loop is `for (i = 0; i < (int)values; i++)` over the enumeration index;
neither `got` nor `dataLen` participates in the loop condition, and `i` is
advanced unconditionally. A failure for one drive leaves that drive at its reset
value `"X:\"` (set by the `for (d = 'A'; d <= 'Z'; …)` preamble at
`mainwnd2.cpp:2622-2627`) and continues.

Also checked: `path` is **never read when `got == FALSE`** — the `if (got && …)`
short-circuits before `LowerCase[path[0]]`, so no uninitialized read was
introduced (`BYTE path[MAX_PATH]` is uninitialized on that branch).

## 7. Other checks

- `LowerCase[name[0]]` with `/J` (unsigned `char`) indexes 0..255 — safe, unchanged.
- A non-ASCII single-byte value **name** would fail `d2 >= 'a' && d2 <= 'z'`
  *before* reaching `GetValue`, so `SalRegQueryValueExW8`'s strict
  `SalU8ToW(lpValueName)` (which would return `ERROR_INVALID_PARAMETER` on an
  ACP byte) is never exercised. No new failure mode.
- `nameLen = 2` / `dataLen = MAX_PATH` at the top of the loop: `dataLen` is now
  dead (immediately overwritten). Harmless; a tidiness note.
- The dangling-`if`/`else` chain is structurally unchanged.

## 8. Plugin-facing impact

None — `DefaultDir` and this key are core-internal. `LAST_VERSION_OF_SALAMANDER`
untouched; no `src/plugins/shared/*.h` change in this diff.

## Verdict X05 — **ACCEPTED**

No regressed surface. `RegEnumValue(…,NULL,NULL)` is documented-legal and
strictly reduces failure modes; the new `dataLen` is provably bounded by
`MAX_PATH` and provably terminated (an improvement over the old code); ASCII
load is byte-identical; the loop terminates. Only note: the extra
`GetValueAux` message box stacking on top of `IDS_UNEXPECTEDVALUE`, in a
practically unreachable error case.

---

# X06 — F-P1-01/02 · `src/cache.cpp`

## 1. Sites changed and their consumers re-enumerated

| Site | Call | Argument | Producer traced |
|---|---|---|---|
| `cache.cpp:117` | `SalSetFileAttributes(TmpName, …)` | `CCacheData::TmpName` | `CCacheDirData::Path` + tmp name; `Path` ← `newDirPath` ← `SalGetTempFileName` (`salamdr3.cpp`, **`GetTempPathW` + `SalWToU8` ⇒ UTF-8**) or the caller's `rootTmpPath` (UTF-8 panel/plugin path) |
| `cache.cpp:121` | `SalDeleteFile(TmpName)` | same | same |
| `cache.cpp:354/355` | `SalSetFileAttributes(Path, …)` / `SalRemoveDirectory(Path)` in `~CCacheDirData` | `Path` | `CCacheDirData::CCacheDirData(path)` ← `newDirPath` (UTF-8) |
| `cache.cpp:478/481` | same pair in `RemoveEmptyTmpDirsOnlyFromDisk` | `Path` | same |
| `cache.cpp:1158-1166` | `GetTempPathW` + `SalWToU8` → `sysTmpDir` | — | new UTF-8 producer |

`TmpName`/`Path` are **genuinely UTF-8 at every changed site** — confirmed by
reading `SalGetTempFileName` (`salamdr3.cpp`), which since feature 063 uses
`GetTempPathW`/`GetSystemDirectoryW` + `SalWToU8` precisely so its output can be
fed to the UTF-8 facade.

Consumers of the changed functions: `CCacheData::CleanFromDisk` is called from
`CDiskCache` teardown/eviction; `~CCacheDirData` from `CDiskCache` dir removal;
`RemoveEmptyTmpDirsOnlyFromDisk` from `CDiskCache::RemoveEmptyTmpDirsOnlyFromDisk`
(`cache.cpp:1109-1118`). All core-internal; the disk-cache service is reached by
plugins only through `CSalamanderGeneral` cache methods, whose observable
contract (a tmp path string in, deletion out) is unchanged.

## 2. The trailing-backslash trim/restore around the calls (`:349-355`, `:474-481`)

`CCacheDirData::Path` always ends with `'\'` (`cache.cpp:333-338`). Both sites
set `Path[PathLength-1] = 0` before the call; `RemoveEmptyTmpDirsOnlyFromDisk`
restores it, the destructor does not (object dying) — **identical to pre-fix**.

Is the trimmed value what the facade expects? Yes: the facade takes a display-form
UTF-8 path with no `\\?\` prefix (`salfileio.h:11-18`); `SalPathToWExtAlloc`
(`salpath.cpp`) canonicalises and prefixes it. A **trailing separator would in
fact be stripped by the canonicaliser** (`SalPathIsAlreadyCanonicalW` returns
FALSE for it and `SalCanonicalizePathW` removes it), so both the trimmed and the
untrimmed form would work — the trimmed one takes the *fast* path
(`SalPathIsAlreadyCanonicalW` == TRUE, no canonicalisation pass). Correct and
marginally cheaper.

Edge case `PathLength == 0` (empty `Path`): the guard `if (PathLength > 0)`
skips the trim, `Path[0] == 0`, and `SalPathToWExtAlloc` returns NULL for an
empty path → `SetLastError(ERROR_INVALID_NAME)`, FALSE. Pre-fix `SetFileAttributes("")`
also failed. Return values are ignored at both sites. **Unchanged.**

## 3. `SalRemoveDirectory` on a non-empty directory

`SalRemoveDirectory` = `SalPathOp(u8path, RemoveDirectoryW)` — a thin wrapper.
On a non-empty directory `RemoveDirectoryW` fails with `ERROR_DIR_NOT_EMPTY`,
exactly as `RemoveDirectoryA` did. Both call sites **ignore the return value**,
and the comment at `cache.cpp:479` documents that failure as the normal case.
**Unchanged.**

## 4. `GetTempPathW` buffer semantics and `sysTmpDir[0] = 0`

- `WCHAR wSysTmpDir[MAX_PATH]` with `GetTempPathW(MAX_PATH, …)` mirrors the old
  `char sysTmpDir[MAX_PATH]` / `GetTempPath(MAX_PATH, …)` exactly.
- `sysTmpDir[0] = 0` is reached on **both** failure legs:
  `GetTempPathW` returning 0 (short-circuit, `SalWToU8` not called), and
  `SalWToU8` returning 0 (too-small target). `SalWToU8` additionally zeroes
  `buf[0]` itself (`salunicode.cpp:302-303`), so it is belt-and-braces.
- **Pre-existing, not a regression**: neither version checks
  `res > nBufferLength` (the "buffer too small, nothing written" case), so an
  over-259-char `%TEMP%` leaves the buffer uninitialised in *both* versions.
  The new code is in fact *less* bad — a garbage `wcslen` that yields >260 UTF-8
  bytes makes `SalWToU8` fail and forces `sysTmpDir[0] = 0`, whereas the old
  code used the garbage directly. Suggested (not required) tightening:
  `DWORD n = GetTempPathW(…); if (n == 0 || n > MAX_PATH || SalWToU8(…) == 0)`.
- New narrow failure window: a `%TEMP%` whose UTF-8 form exceeds `MAX_PATH`
  bytes while its UTF-16 form fits (≳110 accented characters). Consequence is
  `sysTmpDir[0] = 0`, which routes through `ContainTmpName`'s existing
  "no match" behaviour (`rootTmpPathLen == 0` → `canContainThisName` stays
  FALSE, verified by reading `cache.cpp:359-372`) and simply creates a fresh
  tmp dir — the same outcome as the pre-existing `GetTempPath` failure path.
  Not user-visible. **Note only.**

## 5. ASCII / English byte-identity

For an ASCII `%TEMP%`, `GetTempPathW` + `SalWToU8` yields the same bytes as
`GetTempPathA`; the facade calls resolve to the same target files. Behaviour and
bytes identical.

## 6. Sites in the same file the fix did **not** convert (completeness of the record)

Verified with `rg '\bDeleteFile\(|\bRemoveDirectory\(|\bSetFileAttributes\(|\bGetTempPath\(|\bFindFirstFile\('` over `src/cache.cpp`:

| Location | Call | Argument | Status |
|---|---|---|---|
| `cache.cpp:191` | `CreateDirectory` | — | inside a `/* … */` comment block — **dismissed** |
| `cache.cpp:389` | `HANDLES_Q(FindFirstFile(tmpFullName, &data))` in `ContainTmpName` | `tmpFullName` = `Path` + `tmpName` ⇒ **UTF-8** | still ANSI — remaining defect, out of this fix's scope but the record should list it |
| `cache.cpp:1204`, `:1216` | `RemoveDirectory(newDirPath)` (low-memory rollback) | `newDirPath` ← `SalGetTempFileName` ⇒ **UTF-8** | still ANSI — remaining defect, 30 lines below the site the fix *did* change |
| `cache.cpp:1473` | `GetTempPath(2 * MAX_PATH, tmpDir)` in `ClearTEMPIfNeeded` | — | **still ANSI — and this is the one that breaks X07 (see below)** |

Item 4 is not merely incompleteness: because X07 made `RemoveTemporaryDir`
strict-UTF-8-only, leaving `cache.cpp:1473` ANSI actively **regresses** a
working surface. See X07 §4.

## Verdict X06 — **ACCEPTED, with a required companion change**

The five converted sites are correct, their arguments are provably UTF-8, the
trim/restore is compatible with the facade, `SalRemoveDirectory`'s non-empty
behaviour is identical, and `sysTmpDir[0] = 0` still happens on every failure
leg. ASCII byte-identical.

**Required companion change (must ship in the same commit as X07):** convert
`src/cache.cpp:1473` `GetTempPath(2 * MAX_PATH, tmpDir)` to
`GetTempPathW` + `SalWToU8` exactly like the fix already did at `cache.cpp:1158`
(note the `2 * MAX_PATH` buffer size). Without it, X07 regresses (below).
`cache.cpp:389` / `:1204` / `:1216` should at least be recorded as known
remaining sites.

---

# X07 — F-P1-04 · `src/salamdr3.cpp` (`_RemoveTemporaryDir` + `RemoveTemporaryDir`)

## 1. Consumers re-enumerated (own `rg RemoveTemporaryDir`)

| # | Consumer | Argument | Producer | Encoding |
|---|---|---|---|---|
| C1 | `src/cache.cpp:111` | `TmpName` | `CCacheDirData::Path` ← `SalGetTempFileName` | **UTF-8** ✓ |
| C2 | **`src/cache.cpp:1534`** (`CDiskCache::ClearTEMPIfNeeded`) | `tmpDir` | **`GetTempPath` (ANSI) at `cache.cpp:1473`** + `data.cFileName` from `FindFirstFileA` | **CP_ACP** ✗ |
| C3 | `src/pack1.cpp:1578,1612,1636,1643,1647,1829,1837,1847,1854,1869,1883,1906,1915` | `tmpDirNameBuf` | `SalGetTempFileName(targetDir, "PACK", …)` (`pack1.cpp:1453`, `:1810`) | **UTF-8** ✓ |
| C4 | `src/shellsup.cpp:1092`, `:1530` | `fakeRootDir` | `SalGetTempFileName(NULL, "SAL", …)` (`shellsup.cpp:996`, `:1410`) | **UTF-8** ✓ |
| C5 | plugin service `CSalamanderGeneral::RemoveTemporaryDir` (`src/zip.cpp:828` → `spl_gen.h:1010`) | plugin-supplied | checked zip (`extract.cpp:1789` ← `SalGetTempFileName`/`targetDir`), renamer (`rendlg4.cpp` ×5, `rendlg.cpp:1907`), regedt (`dialogs.cpp:789`), demoplug — **none** derives it from a raw `GetTempPathA` | **UTF-8** ✓ |

`src/sfx7zip/install.c` and `src/plugins/zip/selfextr/selfextr.cpp` carry their
own private `RemoveTemporaryDir` copies (standalone SFX binaries) — unaffected.

**The fixer's record must list C2. It is the one ANSI producer, and it is the
regression.**

## 2. Per-surface verdicts

| Surface | Verdict |
|---|---|
| C1 disk-cache tmp-file/dir cleanup | **corrected** (non-ASCII `%TEMP%` trees are now actually removed) |
| C3 external-packer temp trees | **corrected** for non-ASCII, **unchanged** for ASCII |
| C4 drag-drop / clipboard fake root dirs | **corrected** for non-ASCII, **unchanged** for ASCII |
| C5 plugin service, ASCII path | **unchanged** — same enumerate/delete/rmdir sequence, same `void` return, same "callable from any thread" property; `spl_gen.h` untouched, `LAST_VERSION_OF_SALAMANDER` untouched |
| C5 plugin service, UTF-8 path | **corrected** |
| C5 plugin service, relative path | **unchanged** — `SalPathToWExtAlloc` resolves relative forms via `GetFullPathNameW` (`salpath.cpp`, the `!isDrive && !isUNC` branch) before prefixing, so a relative `dir` still resolves against the CWD as `FindFirstFileA` did |
| **C2 startup TEMP cleanup (`ClearTEMPIfNeeded`)** | **REGRESSED** — see §4 |

## 3. Safety proof — can it delete anything it did not delete before, or outside the tree?

**No.** Line by line:

1. **Path construction is confined.** `nameU8` comes from
   `SalConvertFindDataW(&fileW, NULL, nameU8, sizeof(nameU8), NULL, 0)`, i.e.
   from `FindFirstFileW`/`FindNextFileW` on `dir\*`. A find-data name is a bare
   component: it can never contain `'\'`, never `"."`/`".."` (filtered), never
   empty (filtered). `path` is therefore always exactly `dir` + `'\'` + one
   child component, and the recursion re-enters with that same invariant.
   **The walk cannot leave `dir`'s subtree.**
2. **The empty-name hazard is closed.** The guard is
   `if (nameU8[0] != 0 && strcmp(nameU8, "..") && strcmp(nameU8, ".") && …)` —
   an empty `nameU8` **skips**, so `strcpy(end, nameU8)` is never executed with
   an empty name and `path` can never collapse to the directory itself (which
   would have meant self-recursion / deleting the parent). Verified further:
   `SalConvertFindDataW` can only yield an empty name when **both** `SalWToU8`
   and the lenient `WideCharToMultiByte` fail (`salfileio.cpp`), and `SalWToU8`
   is total since 066 — it can only fail on a too-small buffer.
   `cFileName` is `WCHAR[MAX_PATH]` = at most 259 units + NUL; worst-case WTF-8
   is 3 bytes/unit ⇒ 778 bytes ≤ `SAL_FIND_NAME_U8` (780). **It cannot fail
   here at all.** The guard is defence in depth.
3. **No buffer overflow.** `path` is `char[MAX_PATH + 2]`. The guard
   `(end - path) + strlen(nameU8) < MAX_PATH` means `strcpy(end, nameU8)` writes
   at most `MAX_PATH - 1 + 1 = MAX_PATH` bytes into a `MAX_PATH + 2` buffer.
   Because UTF-8 names are ≥ the old ACP byte length, the guard can only
   **skip more** entries than before — never write more. Safe.
4. **`.`/`..` skip is still effective for UTF-8.** `"."` and `".."` are ASCII;
   `SalWToU8` is the identity on ASCII, so `strcmp` still matches exactly.
5. **Recursion terminates.** One level per directory component, `.`/`..`
   excluded, and each recursive call receives a strictly longer path bounded by
   the `< MAX_PATH` guard.
6. **`HANDLES(FindClose(find))` is the right pairing.** `SalFindFirstFile`
   registers the handle itself (`salfileio.cpp`: `HANDLES_Q(FindFirstFileW(w, data))`),
   and `salfileio.h:20-22` documents *"Close find handles with
   `HANDLES(FindClose(h))`"*. The caller correctly does **not** wrap
   `SalFindFirstFile` in `HANDLES_Q`. Same pattern as the ten other
   `SalFindFirstFile` sites (`worker.cpp:1144,1188,2990,3202,4515,4529,5823,6415`,
   `safefile.cpp:145`, `salamdr5.cpp:175`, `shellib.cpp:2632`). Pre-fix the code
   used `HANDLES_Q(FindFirstFile(...))` + `HANDLES(FindClose(...))` — the same
   register/unregister pair. **Unchanged.**
7. **`FindNextFileW` is correctly paired with a `FindFirstFileW` handle**
   (`SalFindNextFile` is a plain `FindNextFileW` re-export). No A/W handle mix.
8. **Behaviour differences are all in the "deletes more correctly" direction**,
   and all inside the tree: `\\?\` disables trailing-dot/space stripping and
   reserved-name interpretation, so files the ANSI path could not address are
   now removable; and best-fit ACP mapping (which in the old code could in
   principle have resolved a non-representable name onto a *different existing
   file in the same directory*) is gone. Reparse-point following is
   **unchanged** — `FindFirstFileW` and `RemoveDirectoryW` treat junctions
   exactly as their A twins did; if a junction inside a temp tree pointed
   outside, the old code followed it too. Not a new hazard.

**Conclusion: X07 cannot delete anything it should not.**

## 4. The regression — `CDiskCache::ClearTEMPIfNeeded` (`src/cache.cpp:1471-1546`)

The producer chain:

```
cache.cpp:1473   if (GetTempPath(2 * MAX_PATH, tmpDir))      // ANSI -> CP_ACP bytes
cache.cpp:1482   HANDLE find = HANDLES_Q(FindFirstFile(tmpDir, &data));  // ANSI, works on ACP
cache.cpp:1532   lstrcpyn(tmpDirEnd, tmpDirs[i], …);         // + "SAL####.tmp"
cache.cpp:1534   RemoveTemporaryDir(tmpDir);                 // <-- CP_ACP path
```

**Before the fix** (`c577ff3`), with e.g. `%TEMP% = C:\Users\Přemysl\AppData\Local\Temp`
on a Czech (CP1250) system:

- `SetCurrentDirectory(dir)` — ANSI, ACP round-trips ⇒ **works**
- `HANDLES_Q(FindFirstFile(path, &file))` — ANSI ⇒ **enumerates**
- `DeleteFile(path)` / `RemoveDirectory(path)` — ANSI ⇒ **delete**

So the "you have N leftover `SAL*.tmp` directories in TEMP — delete them?"
startup prompt (`IDS_DELETETMPSALDIRS`, answered with *Yes* / `IDABORT`)
**actually removed the directories**.

**After the fix**, the same ACP `dir`:

- `SalU8ToW(dir, -1, wDir, …)` fails (CP1250 `ř` = `0xF8`, an invalid UTF-8
  lead) ⇒ the ANSI `SetCurrentDirectory` fallback runs — that leg is fine.
- `SalFindFirstFile(path, &fileW)` → `SalPathToWExtAlloc` → `SalU8ToWAlloc`
  fails on the same bytes → returns NULL → `SetLastError(ERROR_INVALID_NAME)`,
  `INVALID_HANDLE_VALUE`. **Nothing is enumerated.**
- `SalRemoveDirectory(path)` and `SalRemoveDirectory(dir)` fail the same way.
- All return values are discarded ⇒ **silent no-op**.

**User-visible failure**: on a Czech/Polish/Hungarian (or any non-ASCII-username)
Windows account, the user answers *Yes* to the startup TEMP-cleanup question and
**nothing is deleted, with no error**; the leftover `SAL####.tmp` trees
accumulate forever. This surface **worked before this change**. That is a
regression by the charter's definition (step 2: "any input that worked before
now differs").

This is the classic DC-09 shape the review charter warns about — an **ANSI
producer meeting a newly strict facade** — and it was created by this batch:
X06 converted the `GetTempPath` at `cache.cpp:1155` but not its twin at
`cache.cpp:1473`, while X07 made the consumer strict.

Window of the defect: `%TEMP%` containing non-ASCII characters that **are**
representable in the system ACP (the common Central-European case). For
characters not representable in the ACP the old path was already broken
(`GetTempPathA` best-fits to `?`), so nothing is lost there.

**Minimal fix** (one site, mirrors the change already made at `cache.cpp:1158`):

```c
WCHAR wTmpDir[2 * MAX_PATH];
if (GetTempPathW(2 * MAX_PATH, wTmpDir) &&
    SalWToU8(wTmpDir, -1, tmpDir, 2 * MAX_PATH) != 0)
```

(reported, not applied — read-only on the product).

## 5. `RemoveTemporaryDir` — the remaining specific questions

- **`strlen(dir) < MAX_PATH` guard**: still meaningful and still exactly right.
  `_RemoveTemporaryDir` does `strcpy(path, dir)` into `char[MAX_PATH + 2]`, may
  append `'\'` and then `"*"` ⇒ needs `strlen(dir) + 3 ≤ MAX_PATH + 2`, i.e.
  `strlen(dir) ≤ MAX_PATH - 1`. The guard is in **bytes** on both sides and was
  already operating on a UTF-8 `dir` before the fix. **Unchanged.**
- **`SetCurrentDirectoryW` fallback and process directory state**: the return
  value is discarded in **both** versions, so a failing call leaves the CWD
  untouched exactly as before; and `SetCurrentDirectoryToSystem()` runs
  unconditionally afterwards, so **every branch ends in the system directory** —
  the same terminal state as before, before the final `SalRemoveDirectory(dir)`.
  The only intermediate difference is that for a non-ASCII `dir` the CWD now
  really does move into the temp dir (previously the ANSI call failed) — which
  is the intended optimisation, and it is undone before the directory is removed.
  `SetCurrentDirectoryW` is correctly given the **plain** path, not a `\\?\`
  form (which it would reject).
- `WCHAR wDir[MAX_PATH]`: `SalU8ToW(dir, -1, …)` needs at most
  `strlen(dir) + 1 ≤ MAX_PATH` WCHARs given the caller's guard — fits; and it is
  bounds-checked regardless, falling back to the ANSI call on 0.

## 6. Residual (minor) behaviour narrowing inside the corrected surfaces

Because the `(end - path) + strlen(nameU8) < MAX_PATH` guard now measures
**UTF-8 bytes** while it used to measure single-byte ACP characters, a file whose
name is long *and* non-ASCII can now be **skipped** where the ANSI code deleted
it (e.g. a ~110-character Czech name under a ~45-character temp path). The
result is a leftover file, never a wrong deletion. The real root cause is the
`char path[MAX_PATH + 2]` buffer, which cannot hold a full-length UTF-8 path;
sizing it `3 * MAX_PATH` would remove the narrowing. Recorded as a **Note** —
low severity, but the Fix record should acknowledge it.

## 7. Sibling site left unfixed (record completeness)

`_RemoveEmptyDirs` / `RemoveEmptyDirs` at `src/salamdr3.cpp:1050-1086` is the
byte-for-byte ANSI twin of the function just fixed
(`HANDLES_Q(FindFirstFile(path, &file))` at `:1055`, `FindNextFile` at `:1070`,
`RemoveDirectory` at `:1074`, `SetCurrentDirectory(dir)` at `:1080`,
`RemoveDirectory(dir)` at `:1086`). The originating claim in
`findings/_extracted.json:29` names **both** `RemoveTemporaryDir` *and*
`RemoveEmptyDirs`; only the former was converted. Not a regression, but the Fix
record is incomplete as written.

## 8. Previously validated behavior touched

Feature 066 (surrogate names) quickstart: the temp-tree walk now addresses the
true on-disk name via WTF-8, so a lone-surrogate file inside a temp tree is
deleted instead of leaving an `ERROR_INVALID_NAME`. Feature 063 (`SalGetTempFileName`
producing UTF-8) is the premise the fix relies on and is unchanged. No
058/062 scenario touches these functions.

## 9. Per-item path / timing

`_RemoveTemporaryDir` is a per-entry loop, but it is a **deletion** walk — the
per-name conversion cost is negligible next to the file-system operations, and
the pre-fix ANSI calls performed the same conversion inside the A stubs. No
timing numbers required; the record should say so.

## Verdict X07 — **REJECTED**

Regressed surface: **`CDiskCache::ClearTEMPIfNeeded` → `RemoveTemporaryDir`
(`src/cache.cpp:1534`)**, whose `dir` comes from the un-converted ANSI
`GetTempPath` at `src/cache.cpp:1473`. Under a non-ASCII, ACP-representable
`%TEMP%` the startup "delete leftover SAL\*.tmp directories" cleanup worked
before this change and is now a silent no-op.

The fix is otherwise sound and demonstrably safe (§3). Re-submit with the
one-line producer conversion at `cache.cpp:1473` (X06 §6 / X07 §4), and add C2
plus the `_RemoveEmptyDirs` twin (§7) and the long-name narrowing (§6) to the
Fix record.

---
---

# Re-review after rework

Bounded re-verification requested by the coordinator after the X07 rejection was
acted on. Original text above is left intact as the record.

Re-reviewed diff: `git diff -- src/cache.cpp src/salamdr3.cpp` (still against
baseline `c577ff3`). `src/filesmap.cpp` and `src/mainwnd2.cpp` are byte-identical
to what I reviewed â€” **X04 and X05 stay ACCEPTED, not re-examined.**

Build: `build.cmd` (Debug x64) re-run after the rework â€” **BUILD SUCCEEDED**,
exit 0, **0 warnings** (the two recompiled translation units are clean; the
70 pre-existing libssh2 `LNK4217` warnings did not recur in the incremental link).

## Re-review verdicts

| Fix | Previous | Now | Reason |
|---|---|---|---|
| X04 `filesmap.cpp` | ACCEPTED | **ACCEPTED** (unchanged file) | â€” |
| X05 `mainwnd2.cpp` | ACCEPTED | **ACCEPTED** (unchanged file) | â€” |
| X06 `cache.cpp` | ACCEPTED w/ required companion | **REJECTED** | The companion change at `cache.cpp:1478` closed the *producer* but the **consumer one line later is still ANSI**: `HANDLES_Q(FindFirstFile(tmpDir, &data))` at `cache.cpp:1488` now receives the UTF-8 `tmpDir`. `ClearTEMPIfNeeded` is still broken end to end on a non-ASCII account â€” the break simply moved from the deletion to the enumeration |
| X07 `salamdr3.cpp` | REJECTED | **ACCEPTED** | Every enumerated caller of `RemoveTemporaryDir` **and** `RemoveEmptyDirs` now passes UTF-8; the new `_RemoveEmptyDirs` walk carries the same confinement/termination proof as `_RemoveTemporaryDir`, and it can only delete **empty directories** â€” it never deletes a file at all |

**Safety answer (both walks): no. Neither `_RemoveTemporaryDir` nor
`_RemoveEmptyDirs` can delete anything outside the intended tree, and
`_RemoveEmptyDirs` cannot delete a file under any input.**

---

## R1. Does `ClearTEMPIfNeeded` now work end to end on a non-ASCII account? â€” **NO**

### R1.1 The producer change is correct in isolation

`src/cache.cpp:1474-1479`:

```c
WCHAR wTmpDir[2 * MAX_PATH];
if (GetTempPathW(2 * MAX_PATH, wTmpDir) &&
    SalWToU8(wTmpDir, -1, tmpDir, 2 * MAX_PATH) != 0)
```

`tmpDir` is now genuinely UTF-8. Confirmed by sweep: **`rg 'GetTempPath\(' src/cache.cpp`
returns nothing** â€” no ANSI `GetTempPath` is left in the file.

### R1.2 â€¦but the very next consumer is still the ANSI call â€” **second instance of the same defect**

```
cache.cpp:1478  GetTempPathW + SalWToU8              -> tmpDir is UTF-8      (fixed)
cache.cpp:1481  SalPathAddBackslash(tmpDir, â€¦)       -> appends '\'          (byte op, encoding-neutral)
cache.cpp:1483  SalPathAppend(tmpDir, "SAL*.tmp", â€¦) -> appends ASCII mask   (byte op, encoding-neutral)
cache.cpp:1488  HANDLES_Q(FindFirstFile(tmpDir, &data))   <-- STILL ANSI, fed UTF-8
cache.cpp:1513  FindNextFile(find, &data)                 <-- STILL ANSI
```

With `%TEMP% = C:\Users\PĹ™emysl\AppData\Local\Temp` on a Czech (CP1250) system,
`tmpDir` now holds `Ĺ™` as UTF-8 `C5 99`. `FindFirstFileA` converts those bytes
back through CP_ACP, where `C5` = `Äą` and `99` = `â„˘`, producing
`C:\Users\PÄąâ„˘emysl\â€¦\SAL*.tmp` â€” a path that does not exist.
`find == INVALID_HANDLE_VALUE`, `tmpDirs.Count == 0`, and the function returns
having done nothing.

**Comparison against baseline `c577ff3`** (all-ANSI chain: `GetTempPathA` â†’
`FindFirstFileA` â†’ ANSI `RemoveTemporaryDir`) â€” the ACP bytes round-tripped and
the whole cleanup **worked**. So the surface is *still regressed*:

| | baseline `c577ff3` | after first fix | after rework |
|---|---|---|---|
| enumeration finds the `SAL*.tmp` dirs | **yes** | yes (ACP in, ACP out) | **no** (UTF-8 into `FindFirstFileA`) |
| prompt `IDS_DELETETMPSALDIRS` shown | **yes** | yes | **no** |
| answering *Yes* deletes the trees | **yes** | no (silent no-op) | **no** (never asked) |

The user-visible symptom changed shape (the prompt now never appears at all,
rather than appearing and doing nothing) but the regression is the same one:
on a non-ASCII account, leftover `SAL####.tmp` trees are never cleaned, and
they were at baseline.

**Minimal fix** â€” convert the enumeration the same way `_RemoveTemporaryDir`
was converted (note `SalFindFirstFile` self-registers, so `HANDLES_Q` must be
dropped and `HANDLES(FindClose(find))` kept):

```c
WIN32_FIND_DATAW dataW;
char nameU8[SAL_FIND_NAME_U8];
HANDLE find = SalFindFirstFile(tmpDir, &dataW);
...
    SalConvertFindDataW(&dataW, NULL, nameU8, sizeof(nameU8), NULL, 0);
    if (dataW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { char* s = nameU8 + 3; â€¦ }
    â€¦DupStr(nameU8)â€¦
} while (SalFindNextFile(find, &dataW));
```

(Reported, not applied â€” read-only on the product.)

### R1.3 The values built on `tmpDir` â€” `SalPathAddBackslash` / `SalPathAppend`

Both are pure byte operations that append ASCII (`'\'`, `"SAL*.tmp"`) and only
range-check against the buffer size; they neither inspect nor re-encode the
existing bytes. **Encoding-preserving â€” no defect, no truncation concern**
(`2 * MAX_PATH` = 520 bytes of headroom for a temp path).

### R1.4 Does the `cache.cpp:1488` enumeration output reach `RemoveTemporaryDir`? â€” yes, but it is ASCII by construction

`data.cFileName` â†’ `DupStr` (`:1499`) â†’ `tmpDirs` â†’ `lstrcpyn(tmpDirEnd, tmpDirs[i], â€¦)`
(`:1539`) â†’ `RemoveTemporaryDir(tmpDir)` (`:1540`). So the enumerated name **is**
appended to the UTF-8 prefix and handed to the strict facade.

That is *not* a third defect, because the accepted names are ASCII by
construction:

- the mask `SAL*.tmp` forces the first three characters to `S`,`A`,`L`;
- the filter at `:1493-1498` walks from `cFileName + 3` accepting only
  `0-9`/`a-f`/`A-F` and then requires `StrICmp(s, ".tmp") == 0`.

Every byte of an accepted name is therefore in the ASCII range, where UTF-8 and
every shipped ACP coincide. Mixing it onto a UTF-8 prefix is safe.

> Pre-existing theoretical hole (present at baseline, **not** introduced here):
> `FindFirstFile` matches the mask against the long **or** the 8.3 short name, and
> the filter never validates `cFileName[0..2]`. A directory whose short name is
> `SAL*.tmp` but whose long name begins with non-ASCII could in principle slip
> through with non-ASCII in its first three bytes. Unreachable in practice
> (Windows derives the short name from the long one). Converting `:1488` to the
> facade closes it for free, since the name would then come back as UTF-8.

### R1.5 One thing the rework did correct: the *Focus* branch

`cache.cpp:1546` `SendMessage(hActivePanel, WM_USER_FOCUSFILE, (WPARAM)"", (LPARAM)tmpDir)`.
Every other sender of `WM_USER_FOCUSFILE` passes a UTF-8 panel path
(`zip.cpp:2063`, `mainwnd3.cpp:2829/2856/2971`, `mainwnd4.cpp:2092`,
`finddlg1.cpp:2450`, `finddlg2.cpp:1971`); the handler is `fileswnb.cpp:850`.
This site used to pass CP_ACP bytes and now passes UTF-8 â€” **corrected**,
consistent with every peer sender. (It is unreachable today for the same reason
as R1.2, but the encoding is right.)

## R2. Is the new `if` condition equivalent for the ASCII case? â€” **yes**

| Case | old `if (GetTempPath(2*MAX_PATH, tmpDir))` | new `if (GetTempPathW(â€¦) && SalWToU8(â€¦) != 0)` |
|---|---|---|
| ASCII `%TEMP%` | then-branch, `tmpDir` = ACP bytes | then-branch, `tmpDir` = **identical bytes** (`SalWToU8` is the identity on ASCII) |
| API fails (returns 0) | else â†’ `TRACE_E("â€¦TEMP directory not defined!")` | short-circuits, `SalWToU8` not called â†’ **same else, same TRACE_E** |
| buffer too small (return > `nBufferLength`, nothing written) | nonzero â‡’ then-branch runs on an **uninitialised** `tmpDir` (pre-existing UB) | `SalWToU8` over an uninitialised `wTmpDir`; if it yields >520 bytes it returns 0 â†’ else-branch. **Same class of UB, marginally safer**; unreachable with a 520-unit buffer |
| new-only: UTF-8 form > 520 bytes while UTF-16 fits | n/a | else-branch + `TRACE_E`. Needs a ~260-character accented temp path â€” negligible |

Same branch, same bytes, same failure handling for every reachable input.
Cost: +2080 bytes of stack (`WCHAR wTmpDir[2 * MAX_PATH]`) in a function called
once at startup. **Unchanged.**

## R3. `_RemoveEmptyDirs` / `RemoveEmptyDirs` â€” deletion safety

### R3.1 Can it delete anything it did not before, or outside the tree? â€” **no**

1. **It never deletes a file.** The only destructive call in the whole walk is
   `SalRemoveDirectory(path)` at `salamdr3.cpp:1078` (and `:1095` for the root).
   There is no `SalDeleteFile` anywhere in it. `RemoveDirectoryW` fails with
   `ERROR_DIR_NOT_EMPTY` on a non-empty directory, exactly as `RemoveDirectoryA`
   did, and the return value is discarded â€” so only **empty** directories go.
   Semantics identical to pre-fix.
2. **Confinement.** `nameU8` comes from `SalConvertFindDataW` over
   `FindFirstFileW`/`FindNextFileW` on `dir\*` â€” a bare component that can never
   contain `'\'`. `path` is therefore always `dir` + `'\'` + one child, and the
   recursion re-enters with the same invariant. **The walk cannot leave `dir`'s
   subtree.** Reparse-point following is unchanged from the ANSI version.
3. **Empty-name hazard closed.** `if (nameU8[0] != 0 && strcmp(nameU8, "..") && strcmp(nameU8, "."))`
   skips before any `strcpy`, so `path` can never collapse onto the directory
   itself. And as established in Â§X07.3, `SalConvertFindDataW` cannot yield an
   empty name here (worst case 259 units Ă— 3 bytes = 778 â‰¤ `SAL_FIND_NAME_U8`
   = 780), so the guard is defence in depth.
4. **Widened reach is intended and in-tree only.** Via `\\?\`,
   `SalRemoveDirectory` can now remove empty directories with non-ASCII, long,
   or trailing-dot/space names that the ANSI call could not address â€” all inside
   the tree, all empty. Best-fit ACP mapping (which could in principle have
   resolved a name onto a *different* directory in the same parent) is gone.

### R3.2 `.` / `..` skip intact â€” **yes**

`"."` and `".."` are ASCII and `SalWToU8` is the identity on ASCII, so `strcmp`
matches exactly as it did on the ACP name.

### R3.3 The `< MAX_PATH` guard can only skip â€” **yes**

`(end - path) + strlen(nameU8) < MAX_PATH` sits inside
`if ((fileW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && â€¦)` and `strcpy(end, nameU8)`
is inside that block, so at most `MAX_PATH` bytes land in `char path[MAX_PATH + 2]`.
Non-directories never build a path at all. Because UTF-8 byte lengths are â‰Ą the
old ACP lengths, the guard **skips more, never writes more** â€” leaving an empty
directory behind at worst, never a wrong deletion. (Same residual narrowing noted
in Â§X07.6; same root cause, the `MAX_PATH + 2` buffer.)

### R3.4 Recursion terminates â€” **yes**

One component deeper per level, `.`/`..` excluded, depth bounded by the
`< MAX_PATH` guard. Identical control flow to the pre-fix version;
`*(end - 1) = 0` before the final `SalRemoveDirectory(path)` still truncates to
the current directory (`end` is not modified inside the loop).

### R3.5 Handle pairing, `RemoveEmptyDirs` wrapper

`SalFindFirstFile` self-registers (`HANDLES_Q(FindFirstFileW(â€¦))`) and the caller
correctly does **not** wrap it, closing with `HANDLES(FindClose(find))`
(`salamdr3.cpp:1058` / `:1075`) â€” the documented pairing (`salfileio.h:20-22`).
`FindNextFileW` is paired with a `FindFirstFileW` handle; no A/W mix.
`RemoveEmptyDirs` (`:1081-1096`) mirrors `RemoveTemporaryDir` exactly: the
`strlen(dir) < MAX_PATH` guard still exactly covers `strcpy(path, dir)` + `'\'` +
`"*"` into `char[MAX_PATH + 2]`; the `SetCurrentDirectoryW` return value is
discarded in both versions (a failure leaves the CWD untouched, as before); and
`SetCurrentDirectoryToSystem()` runs unconditionally, so **every branch ends in
the system directory**, the same terminal state as before.

## R4. All producers feeding `RemoveTemporaryDir` / `RemoveEmptyDirs` â€” re-enumerated

`rg RemoveTemporaryDir` / `rg RemoveEmptyDirs` over `src/`:

| Consumer | Argument | Producer | Encoding | Verdict |
|---|---|---|---|---|
| `cache.cpp:111` | `TmpName` | `CCacheDirData::Path` â† `SalGetTempFileName` | UTF-8 | corrected |
| `cache.cpp:1540` | `tmpDir` | `GetTempPathW` + `SalWToU8` (`:1478`) + ASCII-only name from the filter | **UTF-8 now** | producer fixed â€” but unreachable, see R1.2 |
| `pack1.cpp:1578,1612,1636,1643,1647,1829,1837,1847,1854,1869,1883,1906,1915` | `tmpDirNameBuf` | `SalGetTempFileName(targetDir, "PACK", â€¦)` (`:1453`, `:1810`) | UTF-8 | corrected / ASCII unchanged |
| `shellsup.cpp:1092,1530` | `fakeRootDir` | `SalGetTempFileName(NULL, "SAL", â€¦)` (`:996`, `:1410`) | UTF-8 | corrected / ASCII unchanged |
| plugin service `zip.cpp:828` â†’ `spl_gen.h:1010` | plugin-supplied | zip `extract.cpp:1789`, renamer `rendlg4.cpp` Ă—5 + `rendlg.cpp:1907`, regedt `dialogs.cpp:789`, demoplug `archiver.cpp:376,795` â€” all from `SalamanderGeneral->SalGetTempFileName` or a UTF-8 target path; **no plugin derives it from a raw `GetTempPathA`** | UTF-8 | ASCII contract unchanged; relative paths still work (`SalPathToWExtAlloc` resolves via `GetFullPathNameW`) |
| **`RemoveEmptyDirs`** `fileswn7.cpp:680` | `newDirs` | `CheckAndCreateDirectory(path, â€¦, newDirs, â€¦)` â€” UTF-8-aware (`salamdr3.cpp:1111` bounds on `SAL_MAX_PATH_UTF8`), derived from the UTF-8 target path | UTF-8 | corrected / ASCII unchanged |
| **`RemoveEmptyDirs`** `fileswn7.cpp:1812` | `newDir` | same | UTF-8 | corrected / ASCII unchanged |

`RemoveEmptyDirs` is **not** exported to plugins (verified: no forwarder in
`src/zip.cpp`, no entry in `src/plugins/shared/*.h`), so it carries no
plugin-facing contract. `LAST_VERSION_OF_SALAMANDER` and all
`src/plugins/shared/*.h` remain untouched by this diff.

`src/sfx7zip/install.c` and `src/plugins/zip/selfextr/selfextr.cpp` keep private
copies of both functions (standalone SFX binaries) â€” unaffected.

**Answer: no remaining non-UTF-8 producer feeds either function.** The one that
did (`cache.cpp:1473`) is fixed; the break that remains is on the *consumer*
side, one line later, inside `ClearTEMPIfNeeded` itself.

## R5. Sites in `cache.cpp` still ANSI on a UTF-8 argument (record completeness)

| Location | Call | Argument | Regression? |
|---|---|---|---|
| **`cache.cpp:1488`, `:1513`** | `FindFirstFile` / `FindNextFile` | `tmpDir` â€” **UTF-8 since the rework** | **YES â€” the open regression (R1.2)** |
| `cache.cpp:389` | `HANDLES_Q(FindFirstFile(tmpFullName, &data))` | `Path` + `tmpName` = UTF-8 | no â€” UTF-8 there since 004/063, already broken at baseline |
| `cache.cpp:1204`, `:1216` | `RemoveDirectory(newDirPath)` (low-memory rollback) | `SalGetTempFileName` output = UTF-8 | no â€” already broken at baseline |
| `cache.cpp:191` | `CreateDirectory` | â€” | inside a `/* â€¦ */` block â€” dismissed |

Only the first row is a regression; the others are pre-existing defects of the
same class that the Fix record should list.

---

## Final re-review verdicts

- **X04 â€” ACCEPTED** (file unchanged since the accepted review).
- **X05 â€” ACCEPTED** (file unchanged since the accepted review).
- **X06 â€” REJECTED.** Regressed surface: `CDiskCache::ClearTEMPIfNeeded`
  (`src/cache.cpp:1470-1552`, reached at startup from `src/salamdr1.cpp:4552`).
  The companion change at `cache.cpp:1478` correctly makes `tmpDir` UTF-8, but
  `HANDLES_Q(FindFirstFile(tmpDir, &data))` at `cache.cpp:1488` (and
  `FindNextFile` at `:1513`) is still the ANSI call, so on a non-ASCII,
  ACP-representable `%TEMP%` the `SAL*.tmp` enumeration now fails and the startup
  cleanup â€” which **worked at baseline `c577ff3`** â€” never even prompts.
  Convert those two calls to `SalFindFirstFile` / `SalFindNextFile` +
  `SalConvertFindDataW` (dropping `HANDLES_Q`, keeping `HANDLES(FindClose)`),
  and the chain is closed.
- **X07 â€” ACCEPTED.** All five consumer families of `RemoveTemporaryDir` and
  both consumers of `RemoveEmptyDirs` now pass UTF-8; both walks are provably
  confined to their subtree, terminate, and can only skip â€” never overflow â€”
  under the byte-based `< MAX_PATH` guard; `_RemoveEmptyDirs` additionally never
  deletes a file. ASCII/English behaviour byte-identical; the plugin-facing
  contract of `CSalamanderGeneral::RemoveTemporaryDir` is unchanged for ASCII and
  for relative paths. Remaining record gaps (non-blocking): the long-non-ASCII-name
  narrowing (Â§X07.6) and the `MAX_PATH + 2` buffer that causes it.

---
---

# Second re-review

Requested after the second rejection. Earlier text above is preserved as the
record. This pass walks the **whole chain end to end**, not only the changed
lines, as instructed.

Re-reviewed diff: `git diff -- src/cache.cpp` (baseline `c577ff3`).
`src/filesmap.cpp`, `src/mainwnd2.cpp` and `src/salamdr3.cpp` are unchanged
since I accepted them â€” **X04, X05, X07 stay ACCEPTED, not re-examined.**

Build: `build.cmd` (Debug x64) re-run â€” **BUILD SUCCEEDED**, exit 0,
**0 warnings**.

Sweep: `rg '\bFindFirstFile\(|\bFindNextFile\(|\bRemoveDirectory\(|\bDeleteFile\(|\bSetFileAttributes\(|\bGetFileAttributes\(|\bGetTempPath\(|\bCreateDirectory\(|\bMoveFile\(|\bCreateFile\(' src/cache.cpp`
returns **one** hit, `cache.cpp:191`, which is inside a `/* â€¦ */` block
(dead code). Confirmed: no live ANSI file API remains in `cache.cpp`.

## Second re-review verdicts

| Fix | Previous | Now | Reason |
|---|---|---|---|
| X04 `filesmap.cpp` | ACCEPTED | **ACCEPTED** (unchanged file) | â€” |
| X05 `mainwnd2.cpp` | ACCEPTED | **ACCEPTED** (unchanged file) | â€” |
| X06 `cache.cpp` | REJECTED | **ACCEPTED** | Every link of the `ClearTEMPIfNeeded` chain is now UTF-8 â€” producer, enumeration, name, path recomposition, `RemoveTemporaryDir`, and the `WM_USER_FOCUSFILE` send. `ContainTmpName`'s comparison operands both changed encoding *together*, and `tmpName` is UTF-8 at every producer, so no previously-matching comparison was altered |
| X07 `salamdr3.cpp` | ACCEPTED | **ACCEPTED** (unchanged file) | â€” |

**The startup cleanup now actually works on a non-ASCII account name.**
Full trace in Â§S1.

---

## S1. `ClearTEMPIfNeeded` â€” every step, encoding named

| # | Step | `file:line` | Encoding / effect |
|---|---|---|---|
| 1 | `GetTempPathW(2 * MAX_PATH, wTmpDir)` | `cache.cpp:1485` | UTF-16 from the OS |
| 2 | `SalWToU8(wTmpDir, -1, tmpDir, 2 * MAX_PATH) != 0` | `:1486` | **UTF-8** in `tmpDir` (total converter; 0 only on a too-small buffer, which diverts to the `else`) |
| 3 | `SalPathAddBackslash(tmpDir, 2 * MAX_PATH)` | `:1488` | appends the ASCII byte `'\'` â€” pure byte op, **encoding-preserving** |
| 4 | `tmpDirEnd = tmpDir + strlen(tmpDir)` | `:1489` | byte offset landing immediately after an ASCII `'\'` â‡’ a **character boundary**, safe for UTF-8 |
| 5 | `SalPathAppend(tmpDir, "SAL*.tmp", 2 * MAX_PATH)` | `:1490` | read the implementation (`src/common/str.cpp`): only `strlen`/`memcpy`/`'\\'`, no ACP logic, and it **fails wholesale** rather than truncating â‡’ **encoding-preserving, cannot cut mid-sequence** |
| 6 | `SalFindFirstFile(tmpDir, &dataW)` | `:1500` | **UTF-8 in â†’ W enumeration.** This was the broken link; it is now the facade |
| 7 | `dataW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY` | `:1506` | wide find-data, no text |
| 8 | `SalConvertFindDataW(&dataW, NULL, nameU8, sizeof(nameU8), NULL, 0)` | `:1508` | **UTF-8** name; `SAL_FIND_NAME_U8` = 780 â‰Ą 259 units Ă— 3 + 1 = 778 â‡’ cannot fail |
| 9 | `if (nameU8[0] == 0) continue;` | `:1509-1510` | see Â§S2 â€” correct, and dead defensive code |
| 10 | hex/`.tmp` filter on `nameU8 + 3` | `:1511-1515` | ASCII-only predicate â€” see Â§S4: the **accepted set is identical** to the ACP version |
| 11 | `DupStr(nameU8)` â†’ `tmpDirs` | `:1517` | heap copy, **UTF-8** |
| 12 | `while (SalFindNextFile(find, &dataW))` / `HANDLES(FindClose(find))` | `:1527`, `:1528` | W enumeration; `SalFindFirstFile` self-registers so `HANDLES_Q` is correctly dropped and `HANDLES(FindClose)` kept (`salfileio.h:20-22`; same pattern as the 11 other `SalFindFirstFile` sites) |
| 13 | message box (`ExpandPluralString` + `_snprintf_s` with `tmpDirs.Count`) | `:1533-1549` | **no name is composed into the text** â€” only the count. Encoding-neutral |
| 14 | `lstrcpyn(tmpDirEnd, tmpDirs[i], 2 * MAX_PATH - (tmpDirEnd - tmpDir))` | `:1554` | byte copy of a UTF-8 name onto a UTF-8 prefix at a character boundary (step 4) â‡’ **valid UTF-8 path**. Names are â‰¤ ~12 ASCII bytes against â‰Ą 260 bytes of room â€” no truncation |
| 15 | `RemoveTemporaryDir(tmpDir)` | `:1555` | **UTF-8 â†’ the strict facade â†’ works** (X07, already accepted) |
| 16 | `SendMessage(hActivePanel, WM_USER_FOCUSFILE, (WPARAM)"", (LPARAM)tmpDir)` | `:1561` | handler `fileswnb.cpp:850` passes `lParam` to `IsTheSamePath(GetPath(), â€¦)` and `ChangeDir(â€¦)` â€” both take the **UTF-8** panel path. Matches every peer sender (`zip.cpp:2063`, `mainwnd3.cpp:2829/2856/2971`, `mainwnd4.cpp:2092`, `finddlg1.cpp:2450`, `finddlg2.cpp:1971`). **Corrected** (it used to send CP_ACP bytes) |

**No ANSI link remains anywhere in the chain.** On
`%TEMP% = C:\Users\PĹ™emysl\AppData\Local\Temp` the enumeration now finds the
`SAL####.tmp` directories, the prompt appears, and answering *Yes* removes the
trees â€” restoring and exceeding the baseline `c577ff3` behaviour.

| | baseline `c577ff3` | after fix 1 | after fix 2 | **now** |
|---|---|---|---|---|
| enumeration finds the dirs | yes | yes | no | **yes** |
| prompt shown | yes | yes | no | **yes** |
| *Yes* deletes the trees | yes | **no** | no | **yes** |
| non-ASCII *name inside* the tree deleted | no | n/a | n/a | **yes** (X07) |

## S2. The `continue` in the `do { â€¦ } while (â€¦)` â€” **you have it right**

`[stmt.cont]`: `continue` passes control to the *loop-continuation portion* of
the enclosing iteration statement. For `do { body } while (expr);` that is the
evaluation of `expr`. So `continue` at `cache.cpp:1510` evaluates
`SalFindNextFile(find, &dataW)` at `:1527` and the loop advances.
**No infinite loop, no skipped entry beyond the intended one.**

(The classic `continue`-skips-the-advance bug needs the advance to be *inside*
the body; here the advance **is** the controlling expression, so `continue` is
safe. It is also behaviourally identical to wrapping the remainder in an
`else`, since nothing follows the `if` block in the loop body.)

The guard itself is **dead defensive code**: `SalConvertFindDataW` empties
`nameU8` only when both `SalWToU8` and the lenient `WideCharToMultiByte` fail,
and `SalWToU8` (total since 066) can fail only on a too-small buffer â€” 780 bytes
against a 778-byte worst case. Harmless, and it makes step 10 marginally safer.

## S3. Pre-existing, unchanged: `nameU8 + 3`

`char* s = nameU8 + 3;` reads past the terminator for a name shorter than 3
bytes (uninitialised bytes *inside* the 780-byte array â€” not out of bounds of
the object). Identical to the pre-fix `data.cFileName + 3` over a `MAX_PATH`
array, and practically unreachable (`FindFirstFile` only returns entries whose
long **or** 8.3 name matches `SAL*.tmp`). **Not a regression** â€” recorded as a
Note. The new `nameU8[0] == 0` guard narrows it slightly.

## S4. Is the accepted-name set identical? â€” **yes** (and one pre-existing hole is now closed)

The filter accepts only names whose bytes from index 3 onward are ASCII hex
digits followed by `.tmp`; any byte â‰Ą 0x80 there is neither a hex digit nor
`'.'`, so the `while` stops and `StrICmp(s, ".tmp")` fails. Bytes 0..2 are
forced to `S`,`A`,`L` by the mask when the long name matched. Consequently every
accepted name is **pure ASCII**, where UTF-8 and every shipped ACP coincide â‡’
**the accepted set is byte-for-byte the same as before.**

The theoretical hole I flagged in the first re-review (Â§R1.4 â€” a directory whose
*short* name matches `SAL*.tmp` while its long name starts with non-ASCII) is
now **closed for free**: `nameU8` is the true UTF-8 long name, so if such a name
ever reached `DupStr` it would be handed to `RemoveTemporaryDir` as UTF-8, which
is exactly what that function now expects. Under the old code it would have been
ACP fed to the strict facade.

## S5. `ContainTmpName` â€” is `tmpName` UTF-8? Could a matching comparison have been altered?

### S5.1 Every producer of `tmpName` (own `rg` on `DiskCache.GetName`, 2nd argument)

| Call site | `tmpName` argument | Producer | Encoding |
|---|---|---|---|
| `src/fileswn6.cpp:3223` | `f->Name` | `CFileData::Name` â€” **UTF-8 by contract** (`spl_com.h:222`: "length of Name in bytes â€¦ UTF-8 byte count since â€¦") | UTF-8 |
| `src/fileswn5.cpp:850` | `validTmpName[0] != 0 ? validTmpName : f->Name` | `lstrcpyn(validTmpName, f->Name, MAX_PATH)` + `SalMakeValidFileNameComponent` â€” byte-wise, replaces only ASCII-invalid characters (`/J` unsigned `char` â‡’ bytes â‰Ą 0x80 are `> ' '` and untouched) | UTF-8 |
| `src/mainwnd4.cpp:824` | `"usermenu.bat"` | ASCII literal | ASCII |
| `src/zip.cpp:2508` | `fileNameInCache` | plugin-supplied (`ViewFileInPluginViewer`) | UTF-8 by the plugin-metadata contract |
| `src/zip.cpp:3235` | `nameInCache` | plugin-supplied (`MoveFileToCache`) | UTF-8 by contract |
| `src/zip.cpp:5511` | `nameInCache` | plugin-supplied (`AllocFileNameInCache`) | UTF-8 by contract |
| `src/zip.cpp:3180` | `NULL` | â€” | n/a (lookup-only, never reaches `ContainTmpName`) |

**`tmpName` is never an ACP value and never an 8.3 short name.** It is the
*desired* name in the cache (a display file name or an ASCII literal), chosen by
the caller â€” never read back from the file system.

### S5.2 Was any previously-matching comparison altered?

Both operands of the two comparisons changed encoding **together**
(`data.cFileName` â†’ `foundNameU8`, `data.cAlternateFileName` â†’ `foundDosNameU8`),
while `tmpName` did not change at all. Case analysis:

- **`tmpName` ASCII (the dominant case)** and the found entry's name ASCII:
  ACP and UTF-8 renderings are the same bytes â‡’ `StrICmp` result
  **byte-identical**. âś” unchanged
- **`tmpName` ASCII**, found entry's name non-ASCII: unequal before (ASCII vs
  ACP) and unequal now (ASCII vs UTF-8) â‡’ **same outcome**, falls to the
  DOS-name branch. âś” unchanged
- **DOS-name branch, `tmpName` ASCII**: 8.3 short names are normally
  uppercase ASCII + digits + `~`, identical in both encodings â‡’ **same result**.
  A non-ASCII short name compares unequal to an ASCII `tmpName` either way.
  âś” unchanged
- **`tmpName` non-ASCII UTF-8** (e.g. F3 on a Czech file name): before, the ANSI
  `FindFirstFile(tmpFullName)` re-read the UTF-8 bytes as ACP and returned
  `INVALID_HANDLE_VALUE`, so **no comparison happened at all** and
  `canContainThisName` stayed TRUE â€” the collision check was silently blind.
  Now the file is found and the comparison correctly reports the collision,
  so `canContainThisName = FALSE` and the caller picks a different tmp
  directory (`cache.cpp:1194-1201`), which is the documented fallback.
  **Corrected, not regressed** â€” the check exists precisely to stop the cache
  from opening a stranger's file.
- **Our own tmp files are still excluded before the check**: the
  `Names[i]->TmpNameEqual(tmpFullName)` loop at `:381-385` compares two strings
  both built as UTF-8 `Path` + UTF-8 name â€” unchanged by this diff and still
  matching. âś”
- **`StrICmp`** folds through the ACP `LowerCase[256]` table
  (`src/common/str.cpp:138`). Identical byte sequences fold identically through
  *any* table, so equality is preserved on both sides; the operands changed
  symmetrically. The residual "two different sequences fold alike" risk is the
  pre-existing DC-15 class, present in the ACP version too. âś” not introduced here

**Conclusion: no comparison that previously matched now fails.**

### S5.3 Rest of the `ContainTmpName` chain

`tmpFullName` = `Path` (UTF-8, from `SalGetTempFileName`) + `tmpName` (UTF-8),
guarded by `PathLength + strlen(tmpName) + 1 <= MAX_PATH` â€” a byte guard over
values that were **already UTF-8 before this diff**, so its skip behaviour is
unchanged. `StrNICmp(Path, rootTmpPath, rootTmpPathLen)` at `:365` now compares
UTF-8 against UTF-8 (`rootTmpPathExp` became UTF-8 with the F-P1-02 fix at
`:1161`), and `sysTmpDir` ends with an ASCII `'\'` so the byte prefix ends on a
character boundary. âś”

## S6. `SalConvertFindDataW`'s DOS-name output

- **Buffer size.** `WIN32_FIND_DATAW::cAlternateFileName` is `WCHAR[14]`
  (â‰¤ 13 characters + NUL). `SalWToU8(src, -1, buf, 44)` converts including the
  terminator: worst case 14 units Ă— 3 bytes = **42 â‰¤ 44**.
  `SAL_FIND_DOSNAME_U8` = `3 * 14 + 2` = 44 is **correct, with margin**.
  So `SalWToU8` cannot fail here, and the lenient fallback never runs.
- **Can `foundDosNameU8` be empty where `data.cAlternateFileName` was not?**
  **No.** `SalConvertFindDataW` empties the buffer only when *both* converters
  fail, which the sizing above rules out. Both APIs report the same short name
  from the same field, and `FindFirstFileA` would render an unrepresentable
  short name with `?`, never as an empty string. The one condition that yields
  an empty `foundDosNameU8` â€” `cAlternateFileName[0] == 0`, i.e. the volume has
  no short name for the entry â€” is exactly what the old `data.cAlternateFileName[0] != 0`
  test detected. **The guard's meaning is unchanged.**
- The reverse direction is also safe: nothing can make `foundDosNameU8`
  *non*-empty where the ANSI field was empty.

## S7. ASCII / English byte-identity â€” all three sites

| Site | ASCII behaviour |
|---|---|
| `ClearTEMPIfNeeded` (`:1485-1500`, `:1508-1527`) | `GetTempPathW` + `SalWToU8` yields the **same bytes** as `GetTempPathA` on ASCII; `SalFindFirstFile`/`SalFindNextFile` enumerate the same set as the A pair; `SalConvertFindDataW` yields the same ASCII name; the filter, `DupStr`, `lstrcpyn` and `RemoveTemporaryDir` all see identical bytes. **Byte-identical** |
| `ContainTmpName` (`:388-411`) | For an ASCII `tmpName` and ASCII on-disk names, `foundNameU8` / `foundDosNameU8` hold the same bytes `data.cFileName` / `data.cAlternateFileName` did, so both `StrICmp` results and both TRACE messages are unchanged. **Byte-identical** |
| `SalRemoveDirectory(newDirPath)` Ă—2 (`:1210`, `:1222`) | `RemoveDirectoryW("\\?\C:\â€¦")` targets the same directory as `RemoveDirectoryA("C:\â€¦")` on an ASCII path; both are low-memory rollback paths whose return value is discarded. `newDirPath` traces to `SalGetTempFileName` (`GetTempPathW` + `SalWToU8`) â‡’ UTF-8. **Byte-identical** |

## S8. Plugin-facing surface

**Nothing changed.** All three sites are core-internal:

- `ClearTEMPIfNeeded` is called only from `src/salamdr1.cpp:4552` (first
  instance, startup); it is not exported.
- `ContainTmpName` is a protected member of `CCacheDirData`, reached only via
  `CDiskCache::GetName`. The plugin-visible services built on it
  (`CSalamanderGeneral::ViewFileInPluginViewer` `zip.cpp:2508`,
  `MoveFileToCache` `:3235`, `CSalamanderForViewFileOnFS::AllocFileNameInCache`
  `:5511`) keep their exact signatures and observable behaviour: the same
  `nameInCache` in, the same cached path out, the same
  `exists` / `alreadyExists` semantics. For an ASCII `nameInCache` the result is
  byte-identical.
- `src/plugins/shared/*.h` is untouched by this diff and
  `LAST_VERSION_OF_SALAMANDER` is unchanged.

## S9. Remaining notes (non-blocking, all pre-existing)

- `nameU8 + 3` on a sub-3-byte name (Â§S3).
- `SalMakeValidFileNameComponent` (`src/common/str.cpp`) truncates at
  `MAX_PATH - 4` **bytes** and can cut a UTF-8 sequence in half (DC-12 class).
  Consequence in this chain is benign: an invalid-UTF-8 `tmpName` makes
  `SalFindFirstFile(tmpFullName)` fail with `ERROR_INVALID_NAME`, which is the
  same `INVALID_HANDLE_VALUE` outcome the ANSI call produced â€” **no behaviour
  change**. Needs a >256-byte name to trigger.
- `StrICmp` / `StrNICmp` fold UTF-8 bytes through the ACP `LowerCase` table
  (DC-15 class). Symmetric on both operands, so equality is preserved.
- `cache.cpp:191` `CreateDirectory` â€” inside a `/* â€¦ */` block, dead code.

---

## Final verdicts (second re-review)

- **X04 â€” ACCEPTED** (file unchanged).
- **X05 â€” ACCEPTED** (file unchanged).
- **X06 â€” ACCEPTED.** The `ClearTEMPIfNeeded` chain is UTF-8 at all sixteen
  steps from `GetTempPathW` to `RemoveTemporaryDir` and to the
  `WM_USER_FOCUSFILE` send; the startup cleanup now works on a non-ASCII account
  name. The `continue` in the `do/while` does advance the enumeration.
  `ContainTmpName`'s `tmpName` is UTF-8 at every one of its six producers and is
  never an ACP or 8.3 value, both comparison operands changed encoding together,
  and no previously-matching comparison was altered (the only behaviour change
  is that the non-ASCII collision check, previously blind, now works â€”
  a correction). `SAL_FIND_DOSNAME_U8` = 44 is correct against a 42-byte worst
  case, and `foundDosNameU8` is empty exactly when `cAlternateFileName` was.
  ASCII/English byte-identical at all three sites; nothing plugin-facing changed.
- **X07 â€” ACCEPTED** (file unchanged).

No regressed surface remains in this batch.

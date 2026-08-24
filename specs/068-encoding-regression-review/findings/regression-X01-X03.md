# Regression review — X01 (F-P6-05), X02 (F-P6-02), X03 (F-P4-06)

Reviewer: independent regression agent (did not author the fixes).
Baseline: `c577ff3`. Diff reviewed: `git diff -- src/editwnd.cpp src/editwnd.h
src/dialogs5.cpp src/jumplist.cpp` (working tree). `src/saltests/saltests.cpp`
and `tools/check_encoding.py` excluded per charter; `src/cache.cpp`,
`src/filesmap.cpp`, `src/mainwnd2.cpp`, `src/salamdr3.cpp` are other fixes and
were only checked for overlap with these three (none — `SalPathAddBackslash`
in `salamdr3.cpp` is untouched).

Build evidence: forced recompilation of the three changed TUs
(`dialogs5.cpp`, `editwnd.cpp`, `jumplist.cpp` — objects deleted from
`build\tandemcommander\Debug_x64\Intermediate\`, then `build.cmd`):
**0 warnings, 0 errors** at `/W3 /J /RTCc /RTC1 /std:c++latest`, link OK.
Read-only on the product: nothing under `src/`, `tools/`, `translations/` was
modified.

---

## Fix X01 — F-P6-05, stack overrun (`src/editwnd.cpp`, `src/editwnd.h`)

### Surfaces I enumerated myself

`rg`-derived, not taken from the fix record:

| # | Surface | Location | Verdict |
|---|---|---|---|
| 1 | `CEditLine::InsertText` declaration | `src/editwnd.h:21` | corrected (const-qualified) |
| 2 | `CEditLine::InsertText` definition | `src/editwnd.cpp:353` | unchanged (same `EM_REPLACESEL`, same ANSI message) |
| 3 | Caller — Ctrl+Enter (file name) | `src/editwnd.cpp:912` | corrected |
| 4 | Caller — Ctrl+Shift+Enter (DOS name, same site) | `src/editwnd.cpp:891,912` | unchanged |
| 5 | Caller — Ctrl+Space / Ctrl+`[` / Ctrl+`]` (panel path) | `src/editwnd.cpp:1022` | corrected |
| 6 | Caller — Ctrl+Shift+Space/`[`/`]` (8.3 path) | `src/editwnd.cpp:1010-1015` | unchanged |
| 7 | `CImpDropTarget::InsertText(POINTL, const char*)` + its 2 callers | `src/editwnd.cpp:1162,1474,1501` | unchanged — different class, different arity, already `const` in the baseline |
| 8 | `SAL_FIND_NAME_U8` (new consumer) | def. `src/common/salfileio.h:31`, reachable via `src/precomp.h:64` | unchanged |
| 9 | `CSalPathBuf` (new consumer) | `src/common/salpath.h:33` / `salpath.cpp:16-155`, via `src/precomp.h:63` | unchanged |
| 10 | `SalPathAddBackslash` — loses one core caller | def. `src/salamdr3.cpp:55`; ~20 remaining core callers + plugin service `src/zip.cpp:1447` / `spl_gen.h:1487` | unchanged (function itself untouched) |

Complete enumeration of `InsertText` in the whole tree:
`src/editwnd.h:21`, `src/editwnd.cpp:353,912,1022,1162,1474,1501` — **no other
translation unit, no `.def` export, no `spl_*.h` declaration, not virtual, no
address taken.** The fix record's surface list is therefore complete; the only
things it did not name are #7 (harmless) and #10 (informational).

### Site (a) — Ctrl+Enter buffer and clamp

- **Buffer size.** `char path[SAL_FIND_NAME_U8 + 2]` = `3*260 + 2` = **782**.
  Worst case for a disk panel: `WIN32_FIND_DATAW::cFileName` is `WCHAR[MAX_PATH]`
  (≤ 259 units + NUL) → ≤ 777 WTF-8 bytes; `SalConvertFindDataW` writes into
  `char[SAL_FIND_NAME_U8]` (`src/common/salfileio.h:31`). `l ≤ 780` after the
  clamp, then `path[780]=' '`, `path[781]=0` — the last two bytes of the array.
  **Exactly sized, no off-by-one.** `DosName` is 8.3 (`SAL_FIND_DOSNAME_U8` = 44)
  — far inside. For plugin FS / archive panels `CFileData::Name` is
  plugin-allocated and unbounded (`spl_com.h:205-224` — the `MAX_PATH-5` cap is
  gone since interface 104), so the clamp is *not* dead code; it is the real
  bound there. **Verdict: corrected.**
- **Clamp arithmetic.** Confirmed it reads `s[l]`, not `path[l]` — correct, and
  it is the only correct choice: `memmove` copies `s[0..l-1]`, so `s[l]` is the
  first byte *not* copied; backing up while `s[l]` is a continuation byte
  (`(b & 0xC0) == 0x80`) leaves `l` on a character start. Verified against the
  three cases: `s[780]` a lead byte → loop does not run, whole characters copied;
  `s[780]` a continuation of a sequence starting at 779 → `l` becomes 779, the
  incomplete lead at 779 is dropped; a WTF-8 lone surrogate `ED A0 80` behaves
  identically (its `A0`/`80` are ordinary continuation bytes). **Cannot cut
  before 0** — the `l > 0` guard; a pathological all-continuation-bytes string
  yields `l = 0` → `path` = `" "`, no overrun. **No out-of-bounds read** — the
  clamp only runs when `strlen(s) > 780`, so `s[780]` is inside the string.
  The `(unsigned char)` cast is redundant under `/J` but harmless, and `/RTCc`
  does not fire on a same-width cast.
- **Regression check.** Old buffer was 261 bytes with an unbounded `memmove`;
  every name > 259 bytes smashed the frame (`/GS` fast-fail). So there is **no
  input that worked before and does not work now**: 0–259 bytes byte-identical,
  260–780 bytes now correct instead of crashing, > 780 bytes now truncated on a
  character boundary instead of crashing.
  Cross-check with the new `saltests` item (10): a *byte* clamp that splits a
  sequence makes `SalU8ToW` reject the whole string — this clamp avoids exactly
  that.

### Site (b) — `CSalPathBuf` vs `strcpy` + `SalPathAddBackslash`

I read `CSalPathBuf` in `src/common/salpath.cpp:16-155`:

- `Get()` returns `Buffer`, which is `Inline` (an in-object `char[8]`,
  zero-terminated in every constructor) until the first successful `Reserve` —
  **never NULL**, confirmed for the default ctor, copy ctor and after a failed
  `Set`.
- `Set()` on allocation failure returns FALSE *before* touching the content
  (`Reserve` fails first) — the previous value stays intact and terminated, so
  the buffer is always usable.
- `Set` / `AddBackslash` vs. `strcpy` + `SalPathAddBackslash(path, MAX_PATH)`
  for an ordinary short ASCII path: **identical bytes**. `AddBackslash` is a
  no-op when the last byte is already `\`, so a root `"C:\"` stays `"C:\"` —
  same as `SalPathAddBackslash` (`src/salamdr3.cpp:62-72`, which also skips when
  `path[l-1] == '\\'`). Non-root `"C:\dir"` → `"C:\dir\"` in both.
- 8.3 branch: the failure fallback is preserved exactly — `GetShortPathName`
  returns 0 → `path.Set(s)` (the full path), which is the old
  `if (!GetShortPathName(...)) strcpy(path, s);`. **Verdict: unchanged.**

### The one behavioural difference I found (not a shipped regression)

`AddBackslash()` on an **empty** buffer produces `"\"`, whereas
`SalPathAddBackslash` on an empty string is a no-op. The buffer can only be
empty here if `path.Set(...)` failed, i.e. `malloc` returned NULL — and the
return value of `Set()` is **not checked** at `editwnd.cpp:1012/1014/1018`. Under
OOM the old code would still have inserted the (short) path via `strcpy`; the
new code inserts a bare `"\"` into the command line. Malloc-failure only,
no data loss, no corruption. **Note, not a regression finding.** Minimal
hardening: `if (path.Set(s)) { path.AddBackslash(); InsertText(path.Get()); }`.

### Pre-existing defects the fix neither introduces nor removes (Notes)

- `GetShortPathName` is checked for `!= 0` only. When the 8.3 form needs more
  than `MAX_PATH` the API returns the *required size* (non-zero) and writes
  nothing, so `shortPath` is read uninitialised — the same shape the baseline
  had with `path`. Practically unreachable for the **A** variant (which cannot
  exceed `MAX_PATH` anyway, and fails outright on a > 260 path). Correct test
  would be `ret > 0 && ret < MAX_PATH`.
- `GetShortPathName` is still the **ANSI** call fed a UTF-8 path (DC-01). For a
  non-ASCII path it fails and the code falls back to the full UTF-8 path —
  unchanged from the baseline; belongs to P1's queue, not to this fix.
- `InsertText` still posts `EM_REPLACESEL` to the **ANSI** command-line edit, so
  non-ASCII characters still hop through the ACP (F-P6-04/F-P6-06). This fix is
  memory-safety only and deliberately does not change that; behaviour for every
  previously working input is byte-identical.

### ASCII / English-UI byte-identity

**Yes, byte-identical.** No `LoadStr`/`LoadStrU8` is involved and no converter
is called on either site; for an ASCII name ≤ 259 bytes site (a) produces the
same `name + ' '` bytes, and for an ASCII path site (b) produces the same
`path + '\'` bytes. Language-independent — no resource string participates.

### Plugin-facing

**None.** `CEditLine` is core-internal (`src/editwnd.h`, no `spl_*.h`
declaration, no export). `src/zip.cpp` and `src/plugins/shared/*` are untouched
(`git diff --stat` empty for both); `LAST_VERSION_OF_SALAMANDER` unchanged. The
plugin service `CSalamanderGeneral::SalPathAddBackslash` (`src/zip.cpp:1447`)
still forwards to the unmodified `::SalPathAddBackslash`.

### Previously validated behaviour (066/067 quickstarts)

`specs/066-fix-surrogate-filenames/quickstart.md` §4/§5 (copy/move/delete/view/
rename/attributes/info-line/config round trip) and 067 (drive info) do **not**
touch the command line. The only interaction is item 11's spirit — a WTF-8 name
pushed to the command line by Ctrl+Enter: the clamp treats `ED A0 80`'s
continuation bytes exactly like any other, and for a ≤ 259-byte name nothing
changes at all.

### Per-item path?

**No.** Both sites are one-shot keystroke handlers (`WM_KEYDOWN` on the command
line), not listing/sorting/icon-reading loops. No timing evidence is required
and none is missing.

### Verdict X01

**ACCEPTED.** No regressed surface. The buffer is exactly sized for the
`SalConvertFindDataW` contract (782 = 780 + `' '` + NUL), the clamp reads the
right byte (`s[l]`) and can neither underflow nor read out of bounds,
`CSalPathBuf` reproduces `strcpy`+`SalPathAddBackslash` byte-for-byte on
ordinary paths including roots, `Get()` is never NULL, and the 8.3 fallback is
preserved. Single caveat (documented above, not blocking): `Set()`'s return
value is ignored, so on `malloc` failure a bare `"\"` is inserted.

---

## Fix X02 — F-P6-02 (`src/dialogs5.cpp:495`)

### Surfaces I enumerated myself

`rg "ShowInChDrvText|ShowInBarText|InstalledPluginsText"` and
`rg "IDC_PLUGINSHOWINCHDRV|IDC_PLUGINSHOWINBAR|IDC_PLUGINHEADER"` over
`src/**/*.{cpp,h,rc}`:

| # | Surface | Location | Verdict |
|---|---|---|---|
| 1 | `ShowInChDrvText` member | `src/dialogs.h:975` (`char[200]`) | unchanged (type/size untouched) |
| 2 | producer (the change) | `src/dialogs5.cpp:500` | corrected |
| 3 | ctor init | `src/dialogs5.cpp:37` | unchanged |
| 4 | **only** consumer | `src/dialogs5.cpp:353` `sprintf(buff, ShowInChDrvText, itemText)` → `:354 SalSetWindowTextU8` | corrected |
| 5 | sibling `ShowInBarText` | `:492` (already U8 since 052) → `:327` → `:328` | unchanged |
| 6 | sibling `InstalledPluginsText` | `:503` (left ANSI) → `:107` → `:109 SalSetWindowTextU8` | unchanged — see below |
| 7 | other uses of `IDC_PLUGINSHOWINCHDRV` | `:210, :370, :371, :643, :649` — `GetDlgItem`, `CheckDlgButton`, `EnableWindow`, `IsDlgButtonChecked` | unchanged (no text read/write) |
| 8 | `SalGetDlgItemTextU8` | `src/common/winlib.h:337`, `winlib.cpp:1143` → `SalGetWindowTextU8:1116` | unchanged |

The control's text is read **once**, at `WM_INITDIALOG`, before any
composition overwrites it at `:354` — ordering is correct, and there is no
second reader that could now see a different encoding.

### Truncation / return semantics vs. `GetDlgItemText`

Read `SalGetWindowTextU8` (`src/common/winlib.cpp:1116-1131`) and
`SalWToU8` (`src/common/salunicode.cpp:278-306`):

- `SalWToU8` is **all-or-nothing**: `WideCharToMultiByte` with an insufficient
  buffer returns 0, and `SalGetWindowTextU8` then zeroes `u8Buf` and falls back
  to the **same** `GetWindowText(hWnd, u8Buf, u8BufSize)` ANSI call the baseline
  used. So a template that does not fit into 200 UTF-8 bytes lands in exactly
  the pre-fix state (ANSI, truncated at 199 bytes) — **there is no
  mid-UTF-8-character truncation path at all.** This was the sharpest question
  and it is answered by construction, not by "no shipped string is that long".
- Return value: `len - 1` = bytes excluding the terminator = `GetDlgItemText`'s
  contract. (The site ignores it either way.)
- Measured the actual shipped strings (`translations/*/salamand.slt`, dialog
  2600, control 2621): longest enabled language is French at **78 bytes**,
  Czech/Slovak 73, German 59 — all far under 200, so the fallback is never taken
  in a shipped configuration. (ru 109 / uk 127 also fit; they are disabled.)

### Consumer consistency and composition safety

`:353 sprintf(buff, ShowInChDrvText, itemText)` where `itemText` is
`p->ChDrvMenuFSItemName` (UTF-8 by the 052 contract, `src/plugins.h:2417`,
normalised at `src/plugins1.cpp:1244` via `SalLegacyToU8Alloc`) or the literal
`"FS"`, and `:354 SalSetWindowTextU8`. **Both halves are now UTF-8 — the
composition is consistent.** Pre-fix the CP1250 template + UTF-8 argument was
invalid UTF-8, so `SalSetWindowTextU8` fell through to the raw `A` call
(`winlib.cpp:1112`); post-fix the wide path is taken.

Regression check on that switch: the dialog is **ANSI** (`CCommonDialog` →
`CDialog(..., BOOL unicodeWnd = FALSE)`, `src/common/winlib.h:366`), so
`SetWindowTextW` is projected back through the ACP by USER32. For an
ACP-representable label the rendered result is the same as the old `A` path;
for a label character the ACP cannot represent, both paths already showed `?`
(the resource → ANSI conversion at dialog creation happens before either read).
**No visible difference for any input that rendered correctly before.**

### ASCII / English-UI byte-identity

**Yes.** The English label `"Sh&ow %s item in Change Drive menu and Drive bar"`
is pure ASCII; `GetWindowTextW` + `SalWToU8` yields byte-identical content to
`GetDlgItemText`, so `ShowInChDrvText`, the composed `buff` and the final
`SetWindowTextW`/`SetWindowText` output are identical to the baseline. Same for
Dutch/Romanian (ASCII) and, at the rendered level, for every other enabled
language.

### The untouched sibling at `:503` (`InstalledPluginsText`)

**Genuinely unaffected.** Its two `sprintf` arguments are `int`s
(`Plugins.GetCount()`, `numOfLoaded`), so no UTF-8 value is ever mixed in; the
whole composed string is uniformly ACP. `SalSetWindowTextU8` at `:109` probes
it, fails (verified: the Czech template
`"Nainstalované plu&giny: (celkem: %d, načtených: %d)"` in CP1250 has `0xE9`
followed by an ASCII byte — not valid UTF-8) and falls back to the `A` call,
which renders correctly. It is a latent inconsistency (an ANSI value handed to
a U8 sink), not a defect, and this fix does not change it in either direction.

### Notes (pre-existing, not caused by X02)

- `char buff[MAX_PATH + 200]` = 460 at `:323`. Worst case at `:353` is
  199 (template) + `strlen(ChDrvMenuFSItemName)`, and in the no-tab branch
  (`:338`) `itemText` points at the raw plugin string, which
  `SalLegacyToU8Alloc(title, MAX_PATH - 1)` can make up to ~777 bytes — an
  unbounded `sprintf`. The sibling at `:327` is worse on paper (199 + 299).
  Both predate this fix; X02 moves the threshold by only the UTF-8 − ANSI delta
  of the template (+3 bytes for Czech, 0 for English), and no shipped plugin
  registers a change-drive item anywhere near that length. Pre-existing
  finding material for P2/P5, not a regression.
- `:353` does not `DuplicateAmpersands(itemText)` although the sibling at `:326`
  does for the plugin name. Pre-existing, unrelated to encoding.

### Plugin-facing

**None.** `SalGetDlgItemTextU8` lives behind
`#if defined(INSIDE_SALAMANDER) && !defined(_UNICODE)` (`winlib.cpp:1098`) and is
not a plugin service; `src/zip.cpp` and `src/plugins/shared/*` untouched.

### Previously validated behaviour / per-item path

No 066/067 quickstart scenario opens the Plugins Manager detail pane. Not a
per-item path (one read at `WM_INITDIALOG`, one composition per selection
change) — no timing evidence required.

### Verdict X02

**ACCEPTED.** One-line change, one consumer, and the truncation question is
closed structurally: `SalWToU8` is all-or-nothing, so an oversized template
falls back to the *identical* legacy `A` read rather than producing a
half-character. English and all ASCII templates are byte-identical; the sibling
at `:503` is genuinely unaffected because its only arguments are integers.

---

## Fix X03 — F-P4-06 (`src/jumplist.cpp`)

### Surfaces I enumerated myself

| # | Surface | Location | Verdict |
|---|---|---|---|
| 1 | `CreateShellLink` signature | `src/jumplist.cpp:156` | corrected — file-local in practice: **not** declared in `src/jumplist.h` (which exports only `CreateJumpList`), and `rg CreateShellLink` finds no other TU |
| 2 | only caller | `src/jumplist.cpp:247` (`IShellLinkW* psl` at `:232`) | corrected |
| 3 | `AddTasksToList` → `poc->AddObject(psl)` | `:250` | unchanged — `AddObject(IUnknown*)`; `IShellLinkW*` converts unambiguously (compiles clean) |
| 4 | `AddTasksToList` caller | `CreateJumpList:318` | unchanged |
| 5 | `CreateJumpList` callers | `src/mainwnd1.cpp:1324`, `src/mainwnd3.cpp:1963`, `src/salamdr1.cpp:4487` | unchanged — all main/UI thread (hot-path set, config change, startup), same STA |
| 6 | inputs | `HotPaths.GetDisplayName(i, name, MAX_PATH)`, `GetPath(i, path, HOTPATHITEM_MAXPATH)` — `src/jumplist.cpp:243-244` | unchanged (UTF-8 producers) |
| 7 | receiver of the arguments | `src/salamdr1.cpp:3583-3604` `GetCommandLineW` → `SalWToU8Alloc`; `-aj` handled at `:3647` | corrected — this is what makes the wide `SetArguments` the *right* fix, confirmed by reading the parser |
| 8 | other core `IShellLink` (A) users | `src/fileswn0.cpp:333`, `src/fileswn2.cpp:154`, `src/shellsup.cpp:527` | unchanged — separate sites, separate findings; no shared helper was refactored, so no consistency break |
| 9 | `PKEY_Title` | `src/jumplist.cpp:21` only | unchanged |
| 10 | already-wide neighbours | `src/shellib.cpp:2638`, `src/plugins/nethood/cache.cpp:2635` | unchanged — the fix matches the house pattern already used there |

The fix record's surface list matches; #5, #7 and #8 were not named and are
worth recording (especially #7, which is the evidence for the fix's premise).

### Memory

- `wPath = SalU8ToWAlloc(path)` — `free(wPath)` at `:183`, inside the only
  branch that allocates, **before** `SetDescription`; no `return`, `goto` or
  early exit between allocation and free. No leak, no use-after-free
  (`desc` is a copy made by `lstrcpynW` *before* the free).
- `wName = SalU8ToWAlloc(name)` — `free(wName)` at `:203`, inside the only
  branch that allocates. No leak.
- **Freeing `wName` after `SetValue`/`Commit` is correct.**
  `IPropertyStore::SetValue` copies the `PROPVARIANT` into the store (this is
  the documented MSDN jump-list pattern, which calls `PropVariantClear`
  immediately after `SetValue`); `Commit` then persists. Freeing after `Commit`
  is strictly later than the reference pattern, so it is safe.
- **Skipping `PropVariantClear` is correct and required.** `pv.pwszVal` points
  at a `malloc`ed buffer; `PropVariantClear` would call `CoTaskMemFree` on it
  (heap mismatch) and, combined with the explicit `free`, would double-free.
  The baseline had the same shape (`pv.pszVal = (LPSTR)name`, a caller stack
  buffer, no clear), so this is a faithful port. The inline comment says so.
- Stack growth: `+1140 WCHAR` (`paramsW`) `+520 B` (`desc` widened) ≈ +2.3 KB in
  a function called at most `HOT_PATHS_COUNT` times sequentially on a 3 MB stack
  (`/STACK:3145728`). Irrelevant.

### Correctness

- `IID_PPV_ARGS(&ret)` with `IShellLinkW* ret` expands to
  `__uuidof(**(&ret))` = `__uuidof(IShellLinkW)` = `IID_IShellLinkW`. Confirmed
  — the change of the variable's type is what re-targets the IID, and the
  comment on `:166` states it. `CLSID_ShellLink` supports both A and W.
- Buffer sizing: `params` is `char[HOTPATHITEM_MAXPATH + 100]` = 1140 **bytes**,
  `paramsW` is `WCHAR[HOTPATHITEM_MAXPATH + 100]` = 1140 **WCHARs**. One WCHAR
  per UTF-8 byte **is always sufficient**: a BMP code point costs 1–3 bytes for
  1 unit, an astral one 4 bytes for 2 units, a WTF-8 lone surrogate 3 bytes for
  1 unit — units ≤ bytes in every case, and the terminator is covered because
  the guard already caps `strlen(params) < INFOTIPSIZE` (1024) < 1140. **No
  overflow possible.**
- `wcscpy(desc + _countof(desc) - 4, L"...")` writes `desc[256..258]` plus the
  NUL at `desc[259]`; `desc` is `WCHAR[MAX_PATH]` = indices 0..259. **In
  bounds**, exactly as the ANSI original was.
- Truncation for an ASCII path is **byte-identical**: `lstrcpynW(desc, wPath,
  260)` + `if (wcslen(wPath) >= 260)` reproduces `lstrcpyn(desc, path, 260)` +
  `if (strlen(path) >= 260)` one-for-one when `wcslen == strlen`.
- `strlen(params) < INFOTIPSIZE` still measures the right thing. The W1
  `SetArguments` limit is in characters; since units ≤ bytes, guarding on the
  byte count is **conservative** — it can never let a wide string longer than
  `INFOTIPSIZE` through. (It is now slightly stricter than necessary for
  non-ASCII paths, which only makes it safer, and it was equally strict before.)
- `GetModuleFileNameW(NULL, pathName, _countof(pathName) - 1)` = 259, matching
  the old `sizeof(pathName) - 1` = 259 for `char[MAX_PATH]`. Same cap, but now
  counted in the right unit — for a non-ASCII install directory this is strictly
  better.
- `ret->SetIconLocation(L"shell32.dll", -319)` — ASCII literal, identical
  resulting link.

### Converter-failure branches — the one behavioural difference

`SalU8ToW`/`SalU8ToWAlloc` return 0/NULL **only** for input that is neither
valid UTF-8 nor valid WTF-8, or on `malloc` failure (`src/common/salunicode.cpp:
237-271`; verified the WTF-8 fallback is tried before giving up). In that case:

- `SetArguments` is skipped (was previously unconditional) → the jump-list entry
  launches Tandem Commander with no path;
- `SetDescription` is skipped (was previously unconditional) → no tooltip;
- `SetValue(PKEY_Title)`/`Commit` are skipped → the entry falls back to a
  shell-default title.

The link object is still created and still added, so **no crash and no
uninitialised or stale buffer is ever handed to the shell** — `desc` is only
read inside `if (wPath != NULL)`, `paramsW` only inside the `!= 0` branch, and
`pv` only inside `if (wName != NULL)`. That is the important safety property and
it holds on every path.

Is the skip a regression? Only for a hot path/name that is **not** valid WTF-8,
which violates the encoding contract (hot paths come from the registry facade,
`src/mainwnd1.cpp:271-293`, which produces UTF-8/WTF-8 by construction). For
such a value the baseline would have passed the raw bytes to the `A` interface,
where a CP1250-encoded legacy value could still have worked. Since there is no
config import from Open Salamander/Newt Commander (CLAUDE.md, feature 046) such
a value cannot be present in a shipped installation. **Classified `latent`, not
regressed** — but the fix record should say so rather than leaving it implicit,
and a one-line hardening (fall back to the un-truncated `path`, or to
`SalU8ToWDisplay`, for the *description* and *title*, which are display-only)
would remove even the theoretical case.

### Pre-existing defects preserved verbatim (Notes, not regressions)

- If `QueryInterface(IPropertyStore)` fails, `hres` is overwritten with the
  failure and returned while `*psl = ret` still holds a live link — the caller
  skips `AddObject` and never `Release`s it. Leak, identical in the baseline.
- `*psl = ret` uses `ret` even when `CoCreateInstance` failed; that is defined
  only because COM guarantees `*ppv = NULL` on failure, and the caller checks
  `SUCCEEDED`. Identical in the baseline.
- `SetPath(pathName)` on a hypothetical `GetModuleFileNameW` failure reads an
  uninitialised buffer. Identical in the baseline.

### ASCII / English-UI byte-identity

**Yes.** For an ASCII hot path and an ASCII hot-path name the produced shell
link is identical: `IShellLinkA` stores its strings by converting to UTF-16
internally, so `SetPath`/`SetArguments`/`SetDescription`/`SetIconLocation` end
up with the same wide data; `VT_LPSTR "Foo"` and `VT_LPWSTR L"Foo"` produce the
same displayed title. The function contains no `LoadStr`/`LoadStrU8` at all, so
it is UI-language independent — English and every other language behave
identically.

### Plugin-facing

**None.** `CreateShellLink` is internal to `jumplist.cpp`; `src/zip.cpp` and
`src/plugins/shared/*` are untouched; `LAST_VERSION_OF_SALAMANDER` unchanged.
The three other core `IShellLink` (A) call sites were deliberately left alone,
so no shared type or helper changed underneath any other consumer.

### Previously validated behaviour / per-item path

No 066/067 quickstart scenario builds a jump list. Not a per-item path — the
jump list is rebuilt at startup, on a hot-path change and on a config change,
at most `HOT_PATHS_COUNT` links per rebuild; no timing evidence required.

### Verdict X03

**ACCEPTED.** Memory is balanced on every path (both allocations freed in the
branch that made them, no early exit in between), `free` after
`SetValue`/`Commit` is the documented pattern and `PropVariantClear` is
correctly omitted for a `malloc`ed buffer, `IID_PPV_ARGS` really resolves to
`IID_IShellLinkW`, one WCHAR per UTF-8 byte is provably sufficient, the `...`
truncation stays in bounds and byte-identical for ASCII, and no converter
failure can leave a stale or uninitialised buffer in front of a shell call. The
only behavioural delta is that `SetArguments`/`SetDescription`/title are skipped
for input that is not valid WTF-8 — unreachable in a shipped configuration
(latent), worth one line in the fix record.

---

## Summary

| Fix | Verdict | Most important observation |
|---|---|---|
| X01 | **ACCEPTED** | Buffer is exactly right (782 = `SAL_FIND_NAME_U8` + `' '` + NUL) and the clamp correctly reads `s[l]`; only gap is that `CSalPathBuf::Set()`'s return is ignored, so an OOM inserts a bare `"\"` instead of the path. |
| X02 | **ACCEPTED** | `SalWToU8` is all-or-nothing, so an oversized template falls back to the *identical* legacy ANSI read — a mid-UTF-8-character truncation is structurally impossible, not merely unlikely. |
| X03 | **ACCEPTED** | Memory and IID are correct and ASCII output is byte-identical; the only delta is that a non-WTF-8 hot path now silently loses its arguments/description/title instead of getting ACP-mangled ones (latent — no config import path can produce one). |

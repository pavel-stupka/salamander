# Research: Fix File Operations on Names with Unpaired Surrogates

**Feature**: 066-fix-surrogate-filenames · **Date**: 2026-08-22
**Status**: All unknowns resolved; findings verified against the codebase and the live fixture.

## R1 — Root cause (verified end to end)

**Ground truth**: `temp\fixtures-041\Lone�surrogate.txt` has the on-disk name
`004C 006F 006E 0065 D800 0073 0075 0072 0072 006F 0067 0061 0074 0065 002E 0074 0078 0074`
— a lone high surrogate `U+D800` between "Lone" and "surrogate" (dumped via
directory enumeration during this research). NTFS names are arbitrary 16-bit
unit sequences; unpaired surrogates are legal on disk but are **not valid
Unicode text** and cannot be encoded as valid UTF-8.

**Failure chain** (all sites read and confirmed):

1. **Intake destroys the name.** `SalConvertFindDataW`
   (`src/common/salfileio.cpp:61-68`) first tries the strict house converter
   `SalWToU8`, which uses `WC_ERR_INVALID_CHARS`
   (`src/common/salunicode.cpp:57`) and **fails** on the unpaired surrogate.
   It then falls back to lenient `WideCharToMultiByte(CP_UTF8, 0, …)`, which
   substitutes `U+FFFD` and succeeds — so `CFileData::Name` stores
   `Lone\xEF\xBF\xBDsurrogate.txt`. The true name is unrecoverable from this
   point on.
2. **Operations recompose the wrong path.** Copy/move/delete compose the full
   UTF-8 path from `Name` and pass it through `SalPathToWExtAlloc`
   (`src/common/salpath.cpp:256`) → `SalU8ToWAlloc` → a wide path containing a
   real `U+FFFD` character → `DeleteFileW` / `CopyFileW` / `MoveFileExW` /
   `SHFileOperationW` report `ERROR_FILE_NOT_FOUND`. Every name-carrying
   operation fails identically (matches the report: delete, move, copy).
3. **Display is a red herring.** The panel shows `?` for the substituted
   `U+FFFD`; Explorer shows the font's notdef box for the real `U+D800`. Both
   are acceptable displays — the defect is purely that the *operational*
   identity was lost at intake.

## R2 — Decision: WTF-8 as the internal name encoding

**Decision**: Extend the house converter pair to **WTF-8** ("Wobbly
Transformation Format-8"): a strict superset of UTF-8 that additionally encodes
each unpaired surrogate `U+D800`–`U+DFFF` as its 3-byte sequence
(`ED A0 80`–`ED BF BF`). Properly paired surrogates continue to encode as one
4-byte sequence, so **WTF-8 output is byte-identical to UTF-8 for every valid
Unicode string** — the encoding change is observable only for names that are
broken today.

**Rationale**:
- Lossless: the wide→WTF-8→wide round trip is the identity for *every*
  16-bit sequence Windows can store in a name. One fix at the conversion core
  heals intake, path composition, clipboard, process arguments, registry
  round trips — everything that flows through the feature-004 facades.
- Zero-risk for valid names: byte-identical output preserves stored config,
  name comparisons, caches, and every current behavior (spec FR-008,
  constitution principle II).
- Structurally UTF-8-shaped: 3-byte sequences with lead byte `ED` + two
  continuation bytes, so byte-structural helpers (`SalU8Next`,
  `SalU8CharCount`, sequence-boundary clamping in `SalLegacyToU8Alloc`) work
  unchanged.
- Industry precedent: Rust's `OsStr`/`OsString` uses exactly WTF-8 on Windows
  for exactly this problem.

**Alternatives considered**:
- **Keep the original wide name alongside `CFileData::Name`** — rejected:
  grows a struct used by ~every panel consumer, touches hundreds of sites,
  risks the plugin ABI, and still leaves every UTF-8 path-composition route
  lossy.
- **Use the 8.3 alternate name as the operational identity** — rejected: 8.3
  generation is per-volume and commonly disabled; unavailable on exFAT/network
  shares; changes the destination name on copy.
- **Open-by-file-ID / NT native APIs** — rejected: exotic, does not solve
  naming the *destination* of a copy/move, violates the house facade pattern.
- **Lenient substitution + refuse operations with a clear error** — rejected:
  spec requires the operations to *work* (Explorer parity), not to fail
  politely.

## R3 — Codec placement and exact semantics

All changes live in `src/common/salunicode.cpp` behind the existing strict
WinAPI fast paths, so the hot path for valid names is untouched:

- **`SalWToU8` / `SalWToU8Alloc`** (wide → WTF-8): fast path
  `WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, …)` unchanged; when it
  fails (for UTF-16 input the only possible cause is an unpaired surrogate),
  run a custom total encoder: paired surrogates → 4-byte sequence, unpaired
  surrogate unit → 3-byte sequence, everything else as usual. The function
  becomes **total** — it never fails for any input wide string (allocation
  and buffer-size failures aside; the existing too-small-buffer → empty-string
  fail-safe is preserved).
- **`SalU8ToW` / `SalU8ToWAlloc`** (WTF-8 → wide): fast path
  `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, …)` unchanged; when it
  fails, run a custom decoder that accepts strict UTF-8 **plus** the
  surrogate 3-byte sequences and still **rejects every other malformed
  input** (overlongs, truncated sequences, stray continuation bytes, > U+10FFFF).
  This preservation of failure is load-bearing: the feature-004/063
  transitional heuristics ("valid UTF-8, else treat as ANSI") depend on
  `SalU8ToW` failing for CP1250 bytes.
- **`SalU8ToWDisplay` / `SalU8ToWDisplayAlloc`**: decode WTF-8 surrogate
  sequences to their actual UTF-16 unit (GDI then renders the font's notdef
  box — identical to Explorer); all other malformed input keeps today's
  lenient `U+FFFD` substitution. One bad unit costs one glyph, satisfying
  spec FR-005, and the display pipeline needs no per-site changes.
- **CESU-8 corner** (an encoded surrogate *pair* as two 3-byte sequences):
  never produced by our encoder; the decoder treats each 3-byte surrogate
  sequence independently, so externally crafted CESU-8 decodes to a valid
  pair and re-encodes as the 4-byte form. `U8→W→U8` idempotence is therefore
  not guaranteed for byte strings we never generate — documented in the
  contract, no practical impact.

## R4 — Interaction-site survey (all verified by reading the code)

| Site | Today | After the codec change |
|------|-------|------------------------|
| `SalConvertFindDataW` (`salfileio.cpp:61`) | strict fails → lenient bakes `U+FFFD` | strict path succeeds with WTF-8; keep the lenient branch as a last-resort fail-safe (now effectively dead) |
| `SalPathToWExtAlloc` (`salpath.cpp:256`) | `U+FFFD` path → wrong file | true wide path; `GetFullPathNameW` passes surrogates through |
| Full-path round trip (`salpath.cpp:376-382`, `SalWToU8Alloc` on `GetFullPathNameW` result) | returns NULL for surrogate paths | succeeds losslessly |
| `SalRegSetValueExW8` (`salamdr6.cpp:2387-2407`) | **needs a fix**: probes validity with raw `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS)`; a WTF-8 path value would be misclassified as ANSI and stored corrupted | probe must go through the WTF-8-aware `SalU8ToW` (read side `SalRegQueryValueExW8` heals automatically via `SalWToU8`) |
| `CStaticText::SetText` (`gui.cpp:619-632`) | **needs a fix**: raw `MB_ERR_INVALID_CHARS` probe → CP_ACP fallback would render WTF-8 names as mojibake in the info line and dialogs | probe via WTF-8-aware conversion first; CP_ACP fallback stays for genuine ANSI producers |
| Remaining core probes (`salamdr4.cpp:1208/1221`, others found by grepping `MB_ERR_INVALID_CHARS` in core `src/*.cpp`) | same pattern | same treatment; enumerate exhaustively in tasks phase (plugins and `src/common/dep/` excluded) |
| `winlib.cpp` UI text setters (SetWindowText/listview/combo/menu U8 wrappers) | route through `SalU8ToWAlloc`; strict failure → legacy ANSI path | heal automatically — conversion now succeeds, wide APIs draw the notdef box |
| Sorting/equality (`SalCompareNamesUTF8`, `SalNameEquivalent`, `SalNameEqualCI`, `salunicode.cpp:335-405`) | unconvertible → deterministic byte-wise fallback | `SalU8ToWAlloc` now succeeds but `NormalizeString` fails on unpaired surrogates (`SalNormalizeNFCAlloc` → NULL) → same deterministic byte-wise fallback; distinctness of look-alike names guaranteed by differing WTF-8 bytes (spec FR-006) |
| `SalU8Next` / `SalU8CharCount` / UTF-8 boundary clamp | byte-structural | WTF-8 sequences have the same lead+continuation shape — unchanged |
| Clipboard `salclip.cpp` (CF_HDROP wide build) | invalid UTF-8 → legacy route | true wide names → paste into Explorer works |
| `SalCreateProcess` / `SalShellExecuteEx` (`salfileio.cpp:118-181`) | NULL conversion for surrogate paths | wide command lines carry surrogates fine — external viewer/editor hand-off works (spec FR-007 largely dissolves; a genuinely lossy channel still errors per-file) |
| Plugin-shared helpers (`splunicode.h`, `winliblt.cpp`) | strict, fail on invalid UTF-8 | **unchanged by design** — plugins are out of scope (spec); a WTF-8 name failing there is no worse than today's `U+FFFD` name |

## R5 — Windows platform facts relied upon

- `WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, …)` fails on unpaired
  surrogates — already asserted by the existing test
  `saltests.cpp:70` (`CHECK(SalWToU8Alloc(L"\xD83D") == NULL)`), which this
  feature **updates** to the new contract (returns `ED A0 BD`).
- `FindFirstFileW`, `CreateFileW`, `DeleteFileW`, `CopyFileW`, `MoveFileExW`,
  `SHFileOperationW`/`IFileOperation` all accept and preserve unpaired
  surrogates in paths (the fixture exists; Explorer operates on it).
- `NormalizeString(NormalizationC, …)` fails on unpaired surrogates
  (`ERROR_NO_UNICODE_TRANSLATION`) — covered by the existing NULL-fallback in
  the comparison helpers; a regression test pins this.
- PowerShell 5.1 / .NET Framework file APIs pass UTF-16 strings through to the
  W WinAPI without Unicode validation — fixture files can be created with
  `[char]0xD800` composition (verified during research by enumerating
  `fixtures-041`).

## R4a — T010 probe-survey results (implementation addendum, 2026-08-22)

Exhaustive grep for `MB_ERR_INVALID_CHARS`/`WC_ERR_INVALID_CHARS` in core
sources (excluding `src/plugins/` and `src/common/dep/`), verdict per site:

| Site | Verdict |
|------|---------|
| `src/common/salunicode.cpp` (SalU8ToW/SalWToU8 internals) | the strict fast paths themselves — WTF-8 fallback added behind them (T003/T004) |
| `src/common/salunicode.cpp` `SalLegacyToU8Alloc` probe | **converted** — probes via WTF-8-aware `SalU8ToW`; WTF-8 metadata is kept byte-unchanged instead of being reinterpreted as ANSI |
| `src/gui.cpp:623` `CStaticText::SetText` | **converted** (T009) — info line/dialog text; WTF-8 name paints as true units |
| `src/salamdr4.cpp` `CopyTextToClipboardU8` | **converted** — name-capable (Alt+Insert copy name/path); true units now reach the clipboard; CP_ACP tolerance branch kept |
| `src/salamdr6.cpp` `SalRegSetValueExW8` | **converted** (T008) — write probe WTF-8-aware; read side `SalRegQueryValueExW8` additionally switched from lenient `WideCharToMultiByte(CP_UTF8, 0, …)` to total `SalWToU8` so stored surrogate values load as WTF-8 instead of `U+FFFD` (found during implementation; without it the config round trip stayed lossy) |
| `src/common/winlib.cpp` UI setters, `src/common/salclip.cpp` | no probe of their own — route through `SalU8ToWAlloc`, healed automatically |
| `src/plugins/shared/*`, plugin sources | out of scope by design (spec/contract boundary) — left strict |

## R6 — Testing strategy

- **saltests** (unit, Debug build, exit code = failures):
  - encoder: every standalone unit `D800`–`DFFF` round-trips W→U8→W; mixed
    valid+invalid strings; output byte-identical to `CP_UTF8` fast path for
    valid inputs (property check over the existing test corpus);
  - decoder: accepts the 3-byte surrogate sequences; still rejects overlongs
    (`C0 80`), truncated tails, stray continuations, `FF/FE`;
  - `SalConvertFindDataW` round trip with a surrogate `cFileName`;
  - comparison: look-alike names (`…D800…` vs `…DC00…`) unequal and stably
    ordered; no crash in `SalCompareNamesUTF8`/`SalNameEqualCI`;
  - display: WTF-8 name decodes to the true units; malformed non-WTF-8 input
    still yields `U+FFFD`;
  - updated expectation at `saltests.cpp:70` and the buffer-bound suite
    (3 bytes per lone unit fits all `SAL_FIND_NAME_U8` maths).
- **Fixture-based manual validation** (quickstart.md): generated
  `temp\fixtures-066\` set (lone high/low, leading/trailing, multiple,
  look-alike pair, surrogate-named folder with children); walk the spec's
  acceptance scenarios (F8/Shift+F8, F5 with code-unit comparison, F6, F3,
  rename, attributes, recursive folder ops, info-line display).
- **Regression**: full existing saltests suite; the feature-041 valid-Unicode
  fixture set continues to pass all operations.

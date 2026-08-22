# Contract: Internal Name Encoding is WTF-8

**Feature**: 066-fix-surrogate-filenames · **Status**: Binding once implemented
**Extends**: feature 004 ("paths are UTF-8"), feature 052 ("plugin metadata is
UTF-8 by contract"), feature 058 (path-encoding icon pipeline)

## Definition

Every file name and path handled by the core application (`CFileData::Name`,
composed panel paths, values crossing the feature-004 W-API facades) is encoded
as **WTF-8**:

- All valid Unicode text encodes exactly as strict UTF-8 — including astral
  characters, which encode as one 4-byte sequence per properly paired
  surrogate pair. **For valid input, WTF-8 output is byte-identical to UTF-8.**
- Additionally, each *unpaired* UTF-16 surrogate unit `U+D800`–`U+DFFF`
  encodes as the 3-byte sequence `ED A0 80`–`ED BF BF` (lead byte `0xED`,
  two continuation bytes — the same shape as any 3-byte UTF-8 sequence).
- No other extension: overlong sequences, truncated sequences, stray
  continuation bytes, lead bytes `F5`–`FF`, and code points above `U+10FFFF`
  remain ill-formed and MUST be rejected by the strict decoder.

## Converter obligations (`src/common/salunicode.{h,cpp}`)

| Function | Obligation |
|----------|------------|
| `SalWToU8` / `SalWToU8Alloc` | **Total encoder.** Never fails for any input unit sequence (except buffer-too-small → existing empty-string fail-safe, and allocation failure). Fast path (`WC_ERR_INVALID_CHARS`) first; custom WTF-8 encoder only when it fails. Output for valid Unicode input MUST be byte-identical to the fast path. |
| `SalU8ToW` / `SalU8ToWAlloc` | **Strict WTF-8 decoder.** Accepts strict UTF-8 plus the surrogate 3-byte sequences; MUST keep failing for every other malformed input — the feature-004/063 transitional heuristics ("valid UTF-8, else treat as ANSI") depend on that failure to route genuine ANSI bytes. |
| `SalU8ToWDisplay` / `…Alloc` | Decodes surrogate sequences to their true UTF-16 unit (font renders notdef, Explorer parity); other malformed input degrades to `U+FFFD`. Display output is one-way and MUST NOT be used to compose paths or compare identities. |

**Round-trip law**: `SalU8ToW(SalWToU8(w)) == w` for every wide string `w`.
The reverse (`SalWToU8(SalU8ToW(b)) == b`) holds for every byte string this
codebase produces; it is *not* guaranteed for externally crafted CESU-8 (an
encoded surrogate *pair* as two 3-byte sequences), which decodes to a valid
pair and re-encodes as the canonical 4-byte form. The encoder never emits
CESU-8.

## Validity-probe obligations

Any core code that asks "is this byte string valid UTF-8?" to decide between
the Unicode path and a legacy/ANSI fallback MUST probe via the WTF-8-aware
`SalU8ToW` (or an equivalent WTF-8 validity check) — never via a raw
`MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, …)` call — wherever the
probed value can carry a file name or path. Known sites fixed by this feature:

- `SalRegSetValueExW8` (`src/salamdr6.cpp`) — write-side probe; without this,
  a surrogate-bearing path saved to configuration is misclassified as ANSI and
  stored corrupted. The read side (`SalRegQueryValueExW8`) round-trips by
  construction once `SalWToU8` is total.
- `CStaticText::SetText` (`src/gui.cpp`) — info line / dialog text; without
  this, a WTF-8 name falls into the CP_ACP branch and renders as mojibake.
- Remaining core probes enumerated in the tasks phase (grep
  `MB_ERR_INVALID_CHARS` under `src/`, excluding `src/plugins/` and
  `src/common/dep/`); each site that can receive a name/path is converted,
  sites that handle non-name text (e.g. translated UI strings) may stay
  strict UTF-8 and MUST be marked as reviewed.

## Byte-structural guarantees (already satisfied, relied upon)

- WTF-8 sequences have the standard lead+continuation shape, so
  `SalU8Next`, `SalU8CharCount`, and the sequence-boundary clamp in
  `SalLegacyToU8Alloc` work unchanged.
- Worst-case expansion stays ≤ 3 bytes per 16-bit unit (a lone surrogate costs
  3 bytes; a valid pair costs 4 bytes for 2 units), so every buffer sized for
  UTF-8 of a maximum-length component (`SAL_FIND_NAME_U8` maths) remains
  sufficient.
- Name equality by `strcmp`/byte comparison ⇔ on-disk unit equality; names
  differing only in surrogate units stay distinct (spec FR-006). Linguistic
  collation falls back to deterministic byte order when NFC normalization
  fails (`NormalizeString` rejects unpaired surrogates) — items must never
  vanish from a sort.

## Boundary notes

- **Plugin ABI**: unchanged (interface version untouched). Names crossing the
  plugin ABI as `char*` may now carry WTF-8 sequences where they previously
  carried `U+FFFD` substitutions; plugin-shared helpers keep strict UTF-8
  converters, so plugins treat such names as they treated the broken names
  before — no new capability is promised to plugins by this feature.
- **External text channels** (clipboard as text, file lists, logs): lossy
  rendering of unrepresentable units is acceptable there; only *operational*
  channels (CF_HDROP wide paths, process command lines, shell operations)
  carry the true units — which the wide WinAPI supports natively.
- **Windows Explorer is the behavioral reference**: every operation Explorer
  completes on a surrogate-bearing name, the core application completes too.

## Enforcement

- `saltests` pins: round-trip totality, UTF-8 byte-compatibility, decoder
  strictness for non-WTF-8 malformed input, comparison distinctness/stability,
  display derivation, `SalConvertFindDataW` intake fidelity.
- The contract-comment block in `src/common/salunicode.h` names this document.

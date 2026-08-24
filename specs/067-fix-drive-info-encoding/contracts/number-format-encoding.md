# Contract: Number-Formatting Encoding

**Feature**: 067-fix-drive-info-encoding · Binding for all core code and for
the plugin API boundary. Extends the feature-041 separator decision and the
feature-052 "UTF-8 by contract" line to the number formatters.

## 1. Producers

| Function | Output encoding | Notes |
|---|---|---|
| `NumberToStr(buf, n)` / `NumberToStr2(buf, n)` (`src/salamdr1.cpp`) | **always valid UTF-8** | digits + `ThousandsSeparator` (UTF-8 since feature 041; `InitLocales` refuses separators >4 bytes to protect the 50-byte buffer contract) |
| `PointToLocalDecimalSeparator(buf, size)` | **always valid UTF-8** | splices `DecimalSeparator` (UTF-8) |
| `PrintDiskSize(buf, size, mode)` — `u8` omitted / `FALSE` | **legacy**: UTF-8 digits/separators + ANSI unit words | frozen at pre-067 bytes; ASCII unit words in all shipped languages make modes 0/3/4 valid UTF-8 in practice, mode 1/2 MIXED in Czech/Hungarian |
| `PrintDiskSize(buf, size, mode, TRUE)` — **new** | **always valid UTF-8, whole string** | every internal `LoadStr` becomes `LoadStrU8` (`IDS_PLURAL_X_BYTES`, `IDS_SIZE_B..EB`, `IDS_SIZE_KB`); English built-in fallbacks are ASCII and unchanged |
| `ExpandPluralString(out, max, fmt, ...)` | same encoding as `fmt` (byte-transparent) | metacharacters are ASCII; safe for both ANSI and UTF-8 templates |
| `ExpandPluralFilesDirs` / `ExpandPluralBytesFilesDirs(..., u8)` | ANSI when `u8=FALSE`, UTF-8 when `u8=TRUE` | pre-existing feature-041 contract, unchanged |

## 2. Composition rule (the invariant the defect violated)

A buffer that contains the output of `NumberToStr`/`NumberToStr2`/
`PointToLocalDecimalSeparator`/`PrintDiskSize(..., TRUE)` is UTF-8 data:

- it MUST NOT be composed with `LoadStr()` text (use `LoadStrU8()`);
- it MUST reach the screen through a UTF-8-aware sink (`Sal*U8` helpers,
  strict `SalU8ToW` + W API, or the lenient `SalU8ToWDisplay*` mirror);
- it MUST NOT be handed to a genuinely ANSI display API.

The model conversions: `src/fileswn6.cpp:1108-1110` (`LoadStrU8` +
`NumberToStr` → `SalMessageBox`) and `src/fileswnb.cpp:1000`
(`ExpandPluralBytesFilesDirs(..., u8=TRUE)` → information line).

## 3. Consumers changed by this feature (`u8=TRUE` / U8 composition)

| Site | Surface |
|---|---|
| `src/dialogs3.cpp:1537-1546` (6 calls) | Ctrl+F1 Drive Information: Capacity/Free/Used, long + short |
| `src/dialogs3.cpp:2216` | Archive size results |
| `src/dialogs2.cpp:412/452/475/478` | Directory-sizes / occupied-space results |
| `src/zip.cpp:6566` | not-enough-space message: `LoadStr(IDS_NOTENOUGHSPACE)` → `LoadStrU8` |
| `src/viewer3.cpp` (~560, ~3117) | viewer offset tooltip: wide tool registration (`TOOLINFOW`/`TTM_ADDTOOLW`), `TTN_NEEDTEXTW` handler, `LoadStrU8` template, one `SalU8ToW` into wide `szText` |

## 4. Plugin API boundary — FROZEN

`CSalamanderGeneral::NumberToStr` / `PrintDiskSize` /
`ExpandPluralBytesFilesDirs` / `PointToLocalDecimalSeparator`
(`src/zip.cpp:1397/1402/3878/5170`) keep pre-067 byte-for-byte behavior:
the forwarders never pass `u8=TRUE`. No vtable change; no
`LAST_VERSION_OF_SALAMANDER` bump (stays 106). The `spl_gen.h` doc comments
for these methods gain an explicit encoding statement (separator bytes UTF-8;
unit words ANSI) — documentation only. A future U8-capable plugin API is
follow-up work (research.md R6), not part of this feature.

## 5. Buffer sizes (unchanged, verified)

`NumberToStr` ≤ 50 bytes incl. terminator (20 digits + 6×4-byte separators is
refused up front by `InitLocales`). `PrintDiskSize` documented minimum 100
bytes holds: worst mode-1 UTF-8 string ≈ 54 bytes. No caller buffer changes.

## 6. Enforcement

- `tools/check_encoding.py` (runs `--strict` on every `build.cmd`): already
  treats `NumberToStr`/`PrintDiskSize`/`PointToLocalDecimalSeparator` as
  UTF-8 producers; extended by this feature to flag ANSI `LoadStr(` used as
  an `ExpandPluralString` template or composed with a UTF-8 producer in the
  formatting layer (the exact 067 pattern at `salamdr6.cpp:425-427`), with
  the tracked-identifier list updated per feature-052 practice.
- `saltests`: `TestNumberCompositionEncoding()` pins the conversion property
  (all-UTF-8 composition converts strictly; one ANSI byte forfeits the wide
  path; lenient mirror costs exactly one U+FFFD).

## 7. Explicitly out of contract

Plugin-internal compositions and sinks (dbviewer, ftp, regedt, zip overwrite
dialogs, filecomp titles — research.md R6): pre-existing, recorded, to be
fixed plugin-side in a follow-up feature. `src/mainwnd3.cpp:5324` split-bar
tooltip: latent only (no shipped locale has a non-ASCII decimal separator);
recorded, untouched.

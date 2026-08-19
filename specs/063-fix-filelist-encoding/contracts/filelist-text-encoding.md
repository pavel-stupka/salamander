# Contract: Make File List & tooltip text encoding

**Feature**: 063-fix-filelist-encoding · **Status**: binding once implemented
**Extends**: feature-004 UTF-8 migration, feature-052 plugin-metadata contract,
feature-010 wide clipboard/tooltip work (contract C5).

## C1 — The generated file list is UTF-8 end to end

The text produced by Make File List (line-template expansion over
`CFileData::Name` and `SalGetDateFormatU8`/`SalGetTimeFormatU8` values) is UTF-8
from expansion to every sink. Each sink converts explicitly at its boundary:

| Sink | Conversion | Where |
|------|-----------|-------|
| Clipboard | `CopyTextToClipboardU8` → UTF-16 `CF_UNICODETEXT` + best-effort CP_ACP `CF_TEXT` | `src/salamdr4.cpp` (new), caller `src/mainwnd4.cpp` |
| Internal viewer | UTF-8 **BOM written at temp-file head, viewer destination only**; viewer's existing `VCE_UTF8` path renders it | `src/mainwnd4.cpp` |
| File on disk | raw UTF-8, **no BOM** (byte-identical to pre-fix output for ASCII; append-safe) | `src/fileswn6.cpp` (unchanged bytes) |

## C2 — Clipboard entry points

- `CopyTextToClipboardU8(const char* u8, int byteLen, …)`: input is UTF-8 by
  contract; conversion probes UTF-8 strictly and falls back to CP_ACP
  (`SalLegacyToU8Alloc` tolerance model) so a stray legacy caller degrades to the
  old behavior instead of corrupting.
- `CopyTextToClipboard` (ANSI) keeps CP_ACP semantics forever — it is exposed to
  the plugin ABI. **New core code must not pass UTF-8 into it.** All in-tree
  callers that hold UTF-8 use the U8 (or W) entry point.
- `CopyTextToClipboardW` remains the canonical wide path; `CF_TEXT` is always the
  best-effort CP_ACP projection of the wide text, never raw multi-byte input.

## C3 — Tooltip/hint text is UTF-8 by contract at rest

- `CStaticText::ToolTipText` and `CButton::ToolTipText` are **UTF-8**: both
  `SetToolTipText` intakes (and therefore `SetActionShowHint` and the published
  plugin GUI wrappers in `src/plugins3.cpp`) normalize with `SalLegacyToU8Alloc`.
  Plugin callers may keep passing CP_ACP — normalization happens at intake.
- `CToolTip` text (`WM_USER_TTGETTEXT` answers) is converted tolerantly (UTF-8
  probe, CP_ACP fallback) and **always drawn wide** (`DrawTextW`); the ANSI draw
  branch is removed. Byte clamps on tooltip text cut on UTF-8 boundaries only.
- Core producers pass `LoadStrU8` (not `LoadStr`) into hint/tooltip intakes.
- `CStaticText::SetText`'s invalid-UTF-8 fallback converts via **CP_ACP**
  (never Latin-1 byte widening).

## C4 — Width modifiers measure displayed characters

`$(Var:N)` / `$(Var:max)` in `DoExpandVarString` measure value width in **UTF-8
code points** (`SalU8CharCount`), pad with `width`-based space counts, and
truncate only on UTF-8 character boundaries (`SalU8Next`). The `varPlacements`
output (`MAKELPARAM(offset, length)`) stays in **bytes** — its consumers index the
byte buffer directly.

## C5 — File-system calls on the feature path take UTF-8

`SalCreateFile`/`SalDeleteFile` (never ANSI `CreateFile`/`DeleteFile`) for the
list target file and temp file; `SalGetTempFileName` obtains the temp/system
directory via the W API + `SalWToU8` (its output path is UTF-8).

## Enforcement

- `tools/check_encoding.py`: add the contract identifiers
  (`CopyTextToClipboardU8`, `SetToolTipText`, `SetActionShowHint`) to the tracked
  sink/producer lists, mirroring feature 052.
- Dialog geometry: every control caption fits its control in every shipped
  language; `translate.layout` accounts for radio/checkbox glyphs and ignores
  dropdown dropped-list heights when scanning free space; `--check-layout` gates
  the language build.

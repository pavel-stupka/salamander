# Research — 063-fix-filelist-encoding

**Date**: 2026-08-19 · **Input**: spec.md + investigation-leads.md
**Method**: three parallel read-only code investigations (clipboard/viewer/file legs;
hint-tooltip mechanism; width semantics + dialog layout), consolidated here. All
line numbers as of commit `15f70c6`.

Every hypothesis from `investigation-leads.md` was confirmed or sharpened; three
adjacent real defects on the same feature path were additionally found (D4, D5, and
the UTF-8-splitting truncation inside D6).

---

## D1 — Clipboard leg: add `CopyTextToClipboardU8`, route UTF-8 producers through it

**Decision**: Add `CopyTextToClipboardU8(const char* u8Text, int byteLen, …)` in
`src/salamdr4.cpp`: convert UTF-8 → UTF-16 (strict probe, CP_ACP fallback for
robustness — the `SalLegacyToU8Alloc` tolerance model) and delegate to the existing
correct `CopyHTextToClipboardW`. Route Make File List (`src/mainwnd4.cpp:315`)
through it. Keep `CopyTextToClipboard` (ANSI) unchanged — it is plugin-ABI-exposed
(`src/plugins/shared/spl_gen.h:1832`, thunk `src/zip.cpp:2418`).

**Evidence**:
- The W path is already correct-by-construction: `CopyTextToClipboardW`
  (`salamdr4.cpp:1144`) → `CopyHTextToClipboardW` (`salamdr4.cpp:1098`) sets true
  `CF_UNICODETEXT` (`:1123`) **and** best-effort `CF_TEXT` via
  `WideCharToMultiByte(CP_ACP, …)` (`AddMultibyteToClipboard`, `salamdr4.cpp:1061`).
  Precedent: `src/stswnd.cpp:2265` (feature 010, contract C5).
- The A path is its mirror with the wrong assumption:
  `AddUnicodeToClipboard` (`salamdr4.cpp:1021`) does
  `MultiByteToWideChar(CP_ACP, …)` (`:1027`, `:1039`) — UTF-8 in, mojibake out —
  and puts raw input bytes on `CF_TEXT`.

**Same-defect callers (all already pass UTF-8 into the A path)** — in scope as a
mechanical sweep through the new U8 entry point (same defect class, house
precedent: feature 052 converted 15 sites in one feature):

| # | Site | What it copies |
|---|------|----------------|
| 1 | `src/mainwnd4.cpp:315` | Make File List → clipboard (**primary**) |
| 2 | `src/fileswn9.cpp:1989` | Ctrl+C copy name (short/full) |
| 3 | `src/fileswn9.cpp:2003` | plugin-FS full name |
| 4 | `src/fileswn9.cpp:2017` | copy current path |
| 5 | `src/fileswn9.cpp:1854,1872,1892,1923` | copy UNC path (the `WNetGetConnection` branch at `:1881` additionally injects ACP into UTF-8 — normalize it with `SalLegacyToU8Alloc`) |
| 6 | `src/fileswn1.cpp:1955` | save selection → clipboard |
| 7 | `src/finddlg1.cpp:2785,2794,2800` | Find window copy name/path |
| 8 | `src/mainwnd1.cpp:2676,2682` | directory/status line context-menu Copy (source documented UTF-8 at `stswnd.cpp:2244`; inconsistent with correct sibling `stswnd.cpp:2265`) |
| 9 | `src/gui.cpp:1360` | hyperlink context-menu copy (`CStaticText` has a `TextW` mirror — use it with `CopyTextToClipboardW`) |
| 10 | `src/msgbox.cpp:435` | Ctrl+C in message box — **mixed** UTF-8 (Title/Text) + CP_ACP (button labels read via ANSI `GetDlgItemText` at `:401,:423`); fix by reading labels wide (`GetDlgItemTextW` + `SalWToU8`) so the whole buffer is UTF-8, then U8 copy |
| 11 | `src/viewer3.cpp:1556,2805` | internal viewer Copy (via `CopyHTextToClipboard`): `GetSelectedText` returns raw file bytes — when `ContentEncoding == VCE_UTF8` convert via `CP_UTF8`, else keep legacy path |

`src/stswnd.cpp:2266` (narrow fallback when `TextW == NULL`) stays as-is.

**Alternatives rejected**:
- Change `CopyTextToClipboard` itself to assume UTF-8 — breaks the plugin ABI
  contract (plugins pass CP_ACP) and contradicts the documented decision at
  `src/consts.h:800-808` ("Converting LoadStr() itself was tried and rejected" —
  same principle: never flip an existing entry point's encoding).
- Fix only site #1 — leaves ten user-visible same-defect sites (Ctrl+C copy name
  is arguably more used than Ctrl+M) and fails the spirit of FR-001/FR-002.

## D2 — Hint tooltip: normalize at intake + tolerant renderer

**Evidence (mechanism, confirmed)**: The tooltip renderer is UTF-8-first since
feature 010: `CToolTip::GetText` (`src/tooltip.cpp:298-319`) probes strictly with
`SalU8ToW`; on failure (`TextLenW == 0`) measurement and paint fall back to
**`DrawTextA`** (`tooltip.cpp:330-333`, `:650-653`). The Czech hint arrives from
`LoadStr` (= `LoadStringA`, CP_ACP — `src/salamdr2.cpp:34-83`) so the strict probe
fails (`ů` = 0xF9 is an invalid UTF-8 lead byte) and the ANSI branch draws through
whatever charset the tooltip font carries (`ncm.lfStatusFont`,
`src/mainwnd1.cpp:1378-1385`) — the environment-dependent garble reported.
A second, deterministic defect sits in the same dialog: `CStaticText::SetText`
(`src/gui.cpp:619-628`) falls back to **Latin-1 byte widening** for invalid UTF-8
(`ě`→`ì`, `č`→`è`, `š`→C1 control) and always draws wide (`gui.cpp:1183,1204`).

**Decision** (per the split house rule — owned producers use `LoadStrU8`; unowned
producers are normalized at intake, feature-052 contract):

1. **Intake normalization**: `CStaticText::SetToolTipText` (`src/gui.cpp:873`) and
   `CButton::SetToolTipText` (`src/gui.cpp:1965`) store
   `SalLegacyToU8Alloc(text)` instead of `DupStr(text)`; the stored
   `ToolTipText` becomes **UTF-8 by contract**. (Keep the `strcmp` early-out at
   `gui.cpp:875` comparing against the normalized form.) This covers all 15
   `SetActionShowHint(LoadStr(…))` sites, `msgbox.cpp:516` (plugin-reachable
   `MSGBOXEX_HINT`), and the published plugin GUI API
   (`src/plugins3.cpp:128-135,195-207,242-249`) without touching callers.
2. **Tolerant renderer**: `CToolTip::GetText` (`tooltip.cpp:298-319`) — probe
   UTF-8, else convert CP_ACP → UTF-16; always produce `TextW`; collapse the
   measure/paint branches (`tooltip.cpp:330-333`, `:650-653`) to unconditional
   `DrawTextW`. This also fixes the six ANSI `LoadStr` producers that feed the
   tooltip directly via `WM_USER_TTGETTEXT` (`src/stswnd.cpp:1827-1872`) and
   plugin toolbars.
3. **Owned producers**: convert the 15 core `SetActionShowHint(LoadStr(…))` sites
   to `LoadStrU8` (mechanical; 042/043 precedent converted 84 sites) so our own
   strings never rely on the fallback heuristic.
4. **Label fallback**: `CStaticText::SetText` (`gui.cpp:621-628`) — replace the
   Latin-1 widening with `MultiByteToWideChar(CP_ACP, …)`.
5. **Boundary safety**: the `WM_USER_TTGETTEXT` answer is clamped in bytes
   (`lstrcpyn(…, TOOLTIP_TEXT_MAX)`, `gui.cpp:1056`) and can split a UTF-8
   sequence — clamp on a UTF-8 boundary (idiom: `salunicode.cpp:150-156`).
6. **Contract enforcement**: add `SetToolTipText`/`SetActionShowHint` identifiers
   to `tools/check_encoding.py`, mirroring feature 052 (`CLAUDE.md` records this
   as the house mechanism).

**Alternatives rejected**:
- Only `LoadStrU8` at `dialogs.cpp:1981` — violates FR-006 (mechanism-level fix);
  leaves 14 sibling sites, `stswnd` producers, and plugin hints broken.
- `SalU8ToWDisplay` in the renderer — substitutes U+FFFD; would silently corrupt
  legitimately-ANSI plugin hints instead of converting them.

## D3 — Viewer leg: UTF-8 BOM on the temp file, viewer destination only

**Evidence**: the internal viewer already auto-detects UTF-8
(`ViewerDetectEncoding`, `src/viewer2.cpp:75-116`; wide rendering
`viewer.cpp:776-792`), so the list usually displays correctly today. Two failure
modes remain: (a) detection samples only the first 10,000 bytes
(`RECOGNIZE_FILE_TYPE_BUFFER_LEN`, `src/viewer.h:15`) — a list whose first 10k
bytes are pure ASCII renders the later Czech names as mojibake; (b) forced-hex
defaults skip detection. `CSalamanderPluginInternalViewerData` has **no encoding
field** (`spl_gen.h:394-412`) — adding one would touch the plugin ABI.

**Decision**: write the UTF-8 BOM (EF BB BF) at the head of the temp file **only
when the destination is the viewer** (`mainwnd4.cpp`, before
`panel->MakeFileList(hFile)`). `ViewerDetectEncoding` short-circuits on the BOM
(`viewer2.cpp:82-87`) and `ContentBOMLen` keeps it invisible. No BOM for the
clipboard destination (bytes are read back verbatim at `mainwnd4.cpp:305-315`).

**Alternatives rejected**: extending `CSalamanderPluginInternalViewerData` with an
encoding field (plugin ABI change, unnecessary given BOM support); BOM on all
destinations (would corrupt append mode and change file-leg content unnecessarily).

## D4 — File leg: UTF-8 content without BOM; fix ANSI file-system calls

**Decision (content)**: the saved list stays **UTF-8 without BOM** — byte-identical
to today for ASCII (FR-008), lossless for all names (FR-004), append-safe, and
Windows 11 Notepad/modern editors auto-detect UTF-8 without BOM.

**Decision (path — confirmed adjacent defect)**: the target file is created with
ANSI `CreateFileA` (`HANDLES_Q(CreateFile(…))`, `src/mainwnd4.cpp:284`; no
`#define CreateFile` remap exists, build is non-`_UNICODE`), while `fileName` is
genuinely UTF-8 (`Configuration.FileListName` filled via wide read → UTF-8 store,
`src/common/winlib.cpp:1074-1082`). A list saved as `seznam-příloh.txt` is created
with a garbled on-disk name. Fix: `SalCreateFile` (`src/common/salfileio.cpp:76`,
the house wrapper used at ~40 other sites) and `SalDeleteFile` for
`DeleteFile(fileName)` at `mainwnd4.cpp:332`. `SalGetFullName` was verified
byte-oriented and UTF-8-safe (`src/salamdr3.cpp:424+`) — no change needed.

**Out of scope (documented)**: `char fileName[MAX_PATH]` is not long-path capable;
pre-existing, unrelated to encoding.

## D5 — Temp-file path: fix the ANSI hole in `SalGetTempFileName`

**Evidence (confirmed adjacent defect)**: `SalGetTempFileName`
(`src/salamdr3.cpp:216-302`) calls ANSI `GetTempPath` (`:222`) /
`GetSystemDirectory` (`:232`) — CP_ACP bytes — then feeds the result to
`SalCreateFile` (`:270`), which converts strictly as UTF-8
(`MB_ERR_INVALID_CHARS`, no fallback). With a non-ASCII `%TEMP%` (e.g. user
`Přemysl`), temp-file creation fails and Ctrl+M dies with "error creating temp
file" for the clipboard and viewer destinations.

**Decision**: `GetTempPathW`/`GetSystemDirectoryW` + `SalWToU8` (pattern already
used at `src/common/trace.cpp:441`). In scope: it is the same feature's critical
path.

## D6 — `:N` / `:max` width: measure and pad in characters, cut on UTF-8 boundaries

**Evidence (confirmed, plus a worse sibling)**: `DoExpandVarString`
(`src/salamdr2.cpp`) measures with `strlen` (`:1011`), stores byte maxima
(`:1014-1018`), and pads/truncates in bytes (`:1025-1041`). Accented names
misalign columns (2 bytes/char), and `$(FileName:20)` **cuts mid-sequence**,
emitting invalid UTF-8. The `maxVarWidths` two-pass protocol has exactly one
caller in the repo (Make File List, `src/fileswn6.cpp:165-194`); the numeric-width
branch also serves user-menu/info-line/pack commands and the plugin API, where
character-based padding is equally correct. Hard constraint: `varPlacements`
(`MAKELPARAM(out - buffer, totalLen)`, `salamdr2.cpp:1030`) is consumed as **byte**
offsets/lengths by the info-line code (`fileswn2.cpp:1184-1208` etc.) and must stay
byte-based.

**Decision**:
1. Promote the existing file-static helpers `NextUTF8Char`/`CountUTF8Chars`
   (`src/fileswn0.cpp:37-57`) to `src/common/salunicode.h/.cpp` as
   `SalU8Next`/`SalU8CharCount`; re-point `fileswn0.cpp`.
2. In `DoExpandVarString`: compute `width = SalIsASCII(value,len) ? len :
   SalU8CharCount(value,len)`; compare/store `width` in `maxVarWidths`; pad
   `valueOutLen - width` spaces after all `len` bytes; truncate by walking
   `valueOutLen` characters with `SalU8Next` (boundary-safe). `varPlacements`
   keeps emitted byte lengths.
3. ASCII fast path makes the change a no-op for ASCII-only lists (FR-008 for
   free).

**Alternatives rejected**: converting to UTF-16 for measurement (allocation churn
in a hot expansion loop); changing `varPlacements` to character units (breaks the
info-line consumers).

## D7 — Dialog layout: fix the widener, give the master template room, relayout all languages

**Evidence (root cause is three layers deep)**: `IDC_FL_FILE` has `cx=27` (master
`src/lang/lang.rc:1193`) — enough for "&File:" but not for any translation; the
radio glyph costs ~11-12 DLU before text. **All 11 non-English languages carry the
same 27** and clip. The automatic widener (`tools/translate/layout.py::widen`, runs
during `merge` and `relayout`) correctly computes that "&Soubor:" needs 30 DLU but
is blocked: the `IDC_FL_LINE` combobox row stores its **dropped-list height
(cy=105)** so `_overlaps_vertically` (`layout.py:62-65`) treats it as a full-height
wall at x=27, clamping every control at x=13 to 25 DLU. Additionally
`estimate_width` (`layout.py:41-50`) has no radio/checkbox glyph allowance. The
same blocker leaves German/Russian/Ukrainian/Romanian "Internal Viewer" radios
clipped in this dialog. Hand-editing the `.slt` alone is **not durable** — `merge`
/`relayout` regenerate geometry from the English template (`merge.py:226,307`).

**Decision** (procedure per house precedent — commit `7903bd9`, features 053/054):
1. `layout.py`: clamp the blocking extent of tall empty-text rows (dropdown
   dropped heights) to a closed-control height in the free-space scan.
2. `layout.py`: add a radio/checkbox glyph allowance using the English template
   row's control class (the `widen(section, english)` hook already receives it).
3. Master `src/lang/lang.rc` `IDD_FILELIST`: radio `IDC_FL_FILE` cx 27→40, edit
   `IDC_FL_FILENAME` x 47→57 / cx 226→216 (right edge unchanged), checkbox
   `IDC_FL_APPEND` x 47→57. English visual appearance unchanged (extra width is
   click area).
4. Propagate text-untouched: `build_langs.cmd --export-templates --module
   salamand` → `python -m translate.relayout --module salamand` (all 12 languages
   incl. disabled) → `build_langs.cmd --check-layout`. `relayout` cannot alter a
   translated character by construction (`relayout.py:44-49`).
5. Czech UI smoke at 100/150/200% DPI (SC-003).

**Alternatives rejected**: hand-editing 12 `.slt` files (reverted by the next
relayout — known-transient); widening only Czech (FR-007 says every language, and
the audit shows all 11 clip).

---

## Verification approach (feeds quickstart.md)

- Clipboard fidelity: paste into Notepad + `Get-Clipboard` in PowerShell against a
  fixture selection covering Czech diacritics + non-CP1250 chars (SC-001, FR-002).
- Destination consistency: same selection → clipboard/viewer/file (SC-002).
- ASCII regression: pre/post byte diff of an ASCII-only list (SC-004, FR-008).
- Alignment: `$(FileName:max)` over mixed accented/plain names (SC-005).
- Czech UI dialog smoke: hint + labels at 3 DPI scales (SC-003).
- Build gates: full Debug build + `saltests`, `check_encoding.py` (runs inside
  `build.cmd`), `--check-layout`, `relayout --dry-run` clean.

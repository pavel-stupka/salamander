# Phase 0 Research: Fix Garbled Numbers in Drive Information Dialog

**Feature**: 067-fix-drive-info-encoding · **Date**: 2026-08-24
**Method**: three parallel audits — (A) every core consumer of the number
formatting, (B) the plugin API boundary and every enabled plugin's call sites,
(C) test infrastructure and the U8 sink-helper family — plus direct reads of
the producers and the reported dialog.

## R1. Root cause (confirmed, complete)

The defect is a **mixed-encoding composition inside `PrintDiskSize`**, not in
the Drive Information dialog:

| Piece | Encoding | Evidence |
|---|---|---|
| `ThousandsSeparator` / `DecimalSeparator` globals | **UTF-8** since feature 041 (Czech = NBSP = `C2 A0`) | `src/salamdr1.cpp:135-138`, `InitLocales` at `:945-977` (`SalGetLocaleInfoU8`; >4-byte separators refused to protect the 50-byte `NumberToStr` buffer contract) |
| `NumberToStr` / `NumberToStr2` / `PointToLocalDecimalSeparator` output | **UTF-8** (digits + spliced separator) | `src/salamdr1.cpp:2922/2939/2959` |
| `LoadStr()` | **ANSI** (deliberate; converting it was tried and rejected in feature 041) | `src/consts.h:805-814` |
| `PrintDiskSize` modes 1/2 | **MIXED**: `ExpandPluralString(LoadStr(IDS_PLURAL_X_BYTES))` (ANSI) + `NumberToStr` (UTF-8) | `src/salamdr6.cpp:425-427` |
| `PrintDiskSize` modes 0/1/4 unit, mode 3 unit | ANSI `LoadStr(IDS_SIZE_B..EB / IDS_SIZE_KB)` — ASCII in every **shipped** language, so valid UTF-8 by accident | `src/salamdr6.cpp:456-516, 525-527` |
| U8 display sinks (`SalSetDlgItemTextU8` family) | strict `SalU8ToWAlloc` → W call; **invalid UTF-8 falls back to the legacy A call** | `src/common/winlib.h:326-331`, `winlib.cpp:1102-1114` |

Mechanism of the screenshot: mode-2 output in Czech is invalid UTF-8 (ANSI `ů`
= `F9` + UTF-8 NBSP = `C2 A0`) → strict conversion fails → A fallback →
`C2 A0` renders as `Â `. Pure-number rows (cluster count) are valid UTF-8 →
W path → correct. Mode-0 rows ("901 GB") are pure ASCII → correct.

**Language exposure** (from `translations/*/salamand.slt`): the mode-1/2
plural-bytes string (12820) is non-ASCII in **Czech** (`bajtů`) and
**Hungarian** (`bájt`) among shipped languages; unit abbreviations
(13980–13986) are **ASCII in all 8 shipped languages** (non-ASCII only in
disabled Russian/Ukrainian/Chinese). English and ASCII-locale output cannot
change: `LoadStrU8` of an ASCII string is byte-identical to `LoadStr`.

## R2. Complete defect inventory (what is garbled today)

Audit A classified **51 core sites** (9 garbled-mixed, 1 garbled-ANSI-sink,
17 correct, 16 ASCII-only-safe, 8 non-UI). The garbled ones:

**Core, mixed composition through U8 sinks (fixed by this feature):**

| Site | Surface | Mode |
|---|---|---|
| `src/dialogs3.cpp:1537/1539/1545` | **Ctrl+F1 Drive Information** — Capacity / Free / Used (reported) | 2 |
| `src/dialogs3.cpp:2216` | Archive size results dialog (`CZIPSizeResultsDlg`) | 1 |
| `src/dialogs2.cpp:412/452/475/478` | Directory-sizes / Calculate Occupied Space results (`CSizeResultsDlg`) | 1 |
| `src/zip.cpp:6566-6567` | "Not enough space" message (pack path, `IDS_NOTENOUGHSPACE` still via ANSI `LoadStr`) | `NumberToStr` |

The `zip.cpp:6566` site is the missed twin of `fileswn6.cpp:1109` /
`fileswn8.cpp:1129`, which were already converted to `LoadStrU8` — same
resource ID 10181, same message, same sink.

**Core, valid UTF-8 through a genuinely ANSI sink (fixed by this feature):**

| Site | Surface |
|---|---|
| `src/viewer3.cpp:3117-3131` (tool registered at `:560-574`) | Internal viewer offset tooltip — ANSI `TTN_NEEDTEXT` writes `NumberToStr` output byte-wise into `szText` |

**Already correct — MUST NOT change (regression guard list):** panel size
column and tiles view (strict `SalU8ToW` → `ExtTextOutW`,
`src/fileswn4.cpp:908/956-996`), information line (lenient
`SalU8ToWDisplayAlloc`, feature 041 — `src/fileswnb.cpp:1000` with `u8=TRUE`),
Find size column (`LVN_GETDISPINFOW`, feature 042) and Find status bar
(`SalStatusSetTextU8`, feature 043), directory-line free space
(`src/stswnd.cpp:611`), Alt+F1 drive menu (`src/drivelst.cpp:1820`), progress
dialog (`src/dialogs.cpp:947-973`, mode 4 = ASCII), overwrite dialog
(`src/worker.cpp:1114`), not-enough-space messages already on `LoadStrU8`
(`fileswn6/8.cpp`), cluster-count rows in Ctrl+F1, bug-report file output.

## R3. Decision D1 — fix shape: feature-041 `u8` opt-in on `PrintDiskSize`

**Decision**: Add `BOOL u8 = FALSE` as the last parameter of the core
`::PrintDiskSize` (declaration `src/consts.h:487`, definition
`src/salamdr6.cpp:416`). When TRUE, every localized text inside it —
`IDS_PLURAL_X_BYTES` and `IDS_SIZE_B..EB/KB`, all modes — loads via
`LoadStrU8`, making the whole result **valid UTF-8 by construction**. The
eight garbled dialog statements pass `TRUE` (including the mode-0 "short"
rows in the same statements, so the whole dialog is correct by construction);
every other caller — including the plugin-API forwarder — compiles unchanged
against the default and keeps **byte-identical** output.

**Why this shape** (established house pattern): identical to feature 041's
`ExpandPluralFilesDirs/ExpandPluralBytesFilesDirs(..., BOOL u8 = FALSE)`
(`src/consts.h:871-880`), which solved this exact defect class for the
information line. The precedent for "LoadStrU8 + NumberToStr composition" is
`fileswn6.cpp:1108-1110`.

**Alternatives rejected**:

1. **Make `PrintDiskSize` unconditionally UTF-8** (no parameter). Audit B
   proved this regresses shipped plugins on a Czech system: dbviewer renders
   `PrintDiskSize` mode-1/2 output through genuinely ANSI sinks at 5 sites
   (`dbviewer/dialogs.cpp:510`, `parser.cpp:302/345/349/1033` — today the
   unit word "bajtů" renders correctly there, afterwards it would garble to
   `bajtÅ¯`), and the FTP upload log line (`ftp/operatsb.cpp:1140`) would
   degrade the same way. The user's directive — no regressions — rules this
   out. The plugin boundary keeps today's bytes.
2. **Fix only the Ctrl+F1 dialog** (convert its sink or post-convert the
   string). Leaves the identical, already-reproducible garble in the
   directory-sizes and archive-size dialogs, and leaves the shared-formatting
   defect in place — fails FR-003.
3. **Revert `ThousandsSeparator` to ANSI**. Undoes feature 041, re-breaking
   the information line and the panel size column (both consume UTF-8 numbers
   through strict wide paths); also breaks feature 043's status-bar contract.
   Not considered further.
4. **New plugin-API method (`PrintDiskSizeU8`) + interface version bump**.
   Not needed for any shipped defect in the core product; deferred with the
   plugin follow-up work (R6). Keeps this change ABI-silent.

`ExpandPluralString` itself is encoding-transparent (byte-wise; its
metacharacters are ASCII and cannot collide with UTF-8 continuation bytes or
CP1250 bytes), so passing it a UTF-8 template is safe — already proven by the
feature-041 information-line path.

## R4. Decision D2 — the two companion core fixes

1. **`src/zip.cpp:6566`**: convert `LoadStr(IDS_NOTENOUGHSPACE)` →
   `LoadStrU8(...)`, exactly matching the fileswn6/fileswn8 precedent. Sink is
   `SalMessageBox` → `SalSetDlgItemTextU8` (U8-first) — becomes correct.
2. **Viewer offset tooltip (`src/viewer3.cpp`)**: register the tool with the
   wide structure/message (`TOOLINFOW` + `TTM_ADDTOOLW`) and handle the wide
   notification (`TTN_NEEDTEXTW`), composing via `LoadStrU8(IDS_VIEWEROFFSETTIP)`
   and converting once with `SalU8ToW` into the wide `szText`
   (`_snwprintf_s`-guarded; the longest possible string ≈ 61 chars < 80).
   This is the only core surface where a *valid UTF-8* number meets a
   *genuinely ANSI* sink today.

## R5. Decision D3 — what deliberately does NOT change

- **Plugin API bytes are frozen** at today's behavior: `CSalamanderGeneral::
  PrintDiskSize/NumberToStr/ExpandPluralBytesFilesDirs/PointToLocalDecimalSeparator`
  (`src/zip.cpp:1397/1402/3878/5170`) forward with `u8` unset → byte-identical
  for every plugin, in every language. No vtable change, no
  `LAST_VERSION_OF_SALAMANDER` bump (stays 106).
- **`src/mainwnd3.cpp:5324`** (split-bar `%` tooltip, ANSI `TTN_NEEDTEXT`):
  latent only — every shipped locale's decimal separator is ASCII (`,`/`.`).
  Recorded, not touched (Constitution III: don't fix untriggered adjacent
  code in the same change).
- **Disabled languages (ru/uk/zh)**: Cyrillic unit abbreviations make ~13
  additional mode-0/3/4 sites mixed *in those languages only*. They do not
  ship; the languages.cfg re-enable checklist gains a note (this feature's
  spec/plan) but no code path for them changes beyond what the `u8=TRUE`
  sites gain for free.
- **Documentation-only**: `spl_gen.h` comments for `NumberToStr`/`PrintDiskSize`/
  `PointToLocalDecimalSeparator` gain an explicit statement of the byte
  encoding (separator bytes are UTF-8; unit words ANSI at the API boundary),
  and the stale feature-041 comment at `src/consts.h:871-876` (which still
  claims the Find dialog and plugins are ANSI-display-only) is corrected —
  comments only, zero behavior.

## R6. Plugin-side defects found by audit B — recorded as FOLLOW-UP (out of scope)

Garbled today, pre-existing, each needs plugin-local work (or a future
`u8`-aware API) and carries its own regression surface:

| Site | Defect |
|---|---|
| `ftp/dialogs6.cpp:377-379` | low-disk-space hint: mode-1 separator garbled (U8-first sink; would need a U8-capable API) |
| `ftp/fs4.cpp:325-327`, `ftp/operats1.cpp:1210` | plugin's own ANSI `LoadStr` composed with API `NumberToStr` |
| `ftp/operatsb.cpp:1140` | upload log line, same class |
| `dbviewer/dialogs.cpp:510`, `parser.cpp:302/345/349/1033` | ANSI listview/edit sinks; separator garbled ≥ 4 digits |
| `regedt/finddlg.cpp:410` | unconditional `CP_ACP` conversion (`StrToWStr`) of a UTF-8 number |
| `zip/dialogs.cpp:1839-1840, 1925-1926` | both ZIP overwrite dialogs use `WM_SETTEXT` (A) although the plugin ships its own `SetDlgItemTextU8` |
| `filecomp/mainwnd.cpp:2043-2046, 2135-2137` | `SplU8ToWAlloc(...) : L""` — ANSI-Czech title silently blank (most brittle sink found) |

## R7. Decision D4 — verification strategy

- **Unit (saltests)**: `NumberToStr`/`PrintDiskSize` are not linkable into
  `saltests.exe` (project compiles only `src/common` + its own file;
  `salamdr*.cpp` pull in the whole app). Per the feature-052 precedent
  (stated at `saltests.cpp:943-953`), a new `TestNumberCompositionEncoding()`
  asserts the **property** the fix restores: a number carrying the real
  locale separator composed with the UTF-8 unit word converts strictly
  (`SalU8ToW != 0`); the same composition with the CP1250 unit byte fails
  strictly (`== 0`, pinning why `Â` appeared); the lenient display
  conversion costs exactly one U+FFFD. Modeled on `TestUiTextEncoding`
  (`saltests.cpp:880-941`) and `TestComposedMessageEncoding` (`:816-874`).
- **Static gate**: `tools/check_encoding.py` already lists `NumberToStr` and
  `PrintDiskSize` as UTF-8 producers, but its sink rules exempt `Sal*U8`
  sinks, so this defect was invisible to it. Extend it minimally: flag an
  ANSI `LoadStr(` used as the template argument of `ExpandPluralString(`
  and of the `sprintf`-composition inside `PrintDiskSize` (i.e., the
  pattern that caused 067), and require the `--strict` run to stay clean —
  the guard runs on every `build.cmd` (line 222) and fails the build when
  python is missing (feature 052).
- **Manual (quickstart)**: Czech UI scenarios for every fixed surface +
  regression sweep of every already-correct surface from the R2 guard list,
  plus an English smoke to demonstrate byte-identical output.
- **Buffer safety**: worst-case mode-1 UTF-8 output ≈ 54 bytes, documented
  ≥100-byte caller buffers hold; `InitLocales` already caps the separator at
  4 bytes for the 50-byte `NumberToStr` contract. No buffer changes needed.

## R8. Incidental observations (not this feature; recorded for the user)

1. **French translation defect**: `12820` is `{!}%s octets{s|0||1|s}` — the
   base already ends in "s", so every French byte count renders "octetss"
   (and singular "1 octets"). Should be `octet{s|0||1|s}`. A one-line
   translation/ui-overrides fix, independent of this feature.
2. `consts.h:812-813` claim about the Find dialog being ANSI is stale
   (features 042/043 fixed Find) — corrected as part of D3 comment updates.
3. `regedt/fs4.cpp:479` passes a `CQuadWord` to a `%s/%d` format — separate
   pre-existing bug, plugin follow-up list.

All NEEDS CLARIFICATION: none remained (the spec had none; research produced
decisions D1–D4 with no open user choices).

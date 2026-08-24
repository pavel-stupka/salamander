# Regression review — X12 (group C7) and X13 (group C11)

**Reviewed**: F-P4-01, F-P4-02 (T1 + T2), D03, D04 — `codetbl.cpp`,
`viewer3.cpp`, `filecomp/controls.cpp`, `filecomp/worker2.cpp` (178 lines) ·
**Reviewer**: independent agent, did not write the fixes · **Charter**:
`contracts/fix-protocol.md` Part B · **Date**: 2026-08-24

## Verdict

**ACCEPTED** — 24 surfaces examined, none regressed. Three notes, all acted on.

### The evidence that mattered most

- **F-P4-01, the ordering hazard** (could the added alternate spelling steal a
  match from an earlier entry?) — proven impossible on shipped data: the six
  accented `centeuro` names are `Kameni\xE8t\xED` and `KOI-8 \xC8S2`, verified
  at byte level, and `\xE8`+`t` / `\xC8`+`S` are lead bytes followed by
  non-continuation bytes. They are therefore invalid UTF-8, `SalU8ToW` returns
  0, and **no alternate is produced at all** for exactly the strings that used
  to match literally.
- **ASCII / English**: a pure-ASCII `coding` round-trips UTF-8→W→CP_ACP as the
  identity, so `strcmp(legacy, coding) == 0` and `codingAlt` stays NULL — zero
  added work, zero behaviour delta. The `westeuro` and `cyrillic` conversion
  tables were dumped and are 100% ASCII.
- **The plugin boundary held**: `Name` is untouched, `EnumCodeTables` still
  hands out the raw pointer, and a plugin-stored ACP name is invalid UTF-8 →
  no alternate → byte-identical lookup. Where a plugin's own value came back
  UTF-8 through the same facade, it now matches too — a gain, not a change.
- **F-P4-02 T1 buffer**: worst case 259 + 3 + 15 + 6 + 199 = 482 bytes against
  `caption[MAX_PATH+300]` = 560; the longest shipped translation of id 10089 is
  `Dateibetrachter` (15 bytes). English/nl/fr/de/ro/es are ASCII → byte-identical.
- **D04 ordering**: `WN_BINARY_FILES_DIFFER` falls through to the shared
  F-P5-09 caption block and the worker then sets the caption again, so D04's
  site is genuinely last; `WN_CBINIT_FINISHED` returns before that block, so it
  does not overwrite D04. Text comparisons are unaffected.
- **D04 byte identity**: filecomp is built without `UNICODE`, so the pre-fix
  `SetWindowText` *is* `SetWindowTextA`; in cs/sk/hu/fr/de/es the ANSI template
  makes the buffer invalid UTF-8, `SplU8ToWAlloc` returns NULL and the exact
  pre-fix call is made.
- **D03**: `Text[MAX_PATH]` fed from `Path1[MAX_PATH]`, so `lstrcpynA(narrow,
  Text, 260)` cannot truncate (it is stricter than the pre-existing `strcpy`);
  `PathCompactPathA`'s documented ≥MAX_PATH buffer is exactly met; `shlwapi.lib`
  is in `filecomp.props`; the A and W draws use identical flags, rect and DC
  state. The failure branch previously drew `L""` — a blank bar.
- **Plugin ABI**: no `src/plugins/shared/` file in this diff; interface 106
  untouched.

## Notes and what was done

| # | Severity | Note | Action |
|---|---|---|---|
| **R-1** | Low | `lstrcpyn(codeName, codeNameU8, 200)` in `viewer3.cpp` is a blind byte copy (`_sal_lstrcpynA`), so a ≥100-character accented conversion name in a user-authored `convert.cfg` could be cut mid-sequence. Unreachable with shipped data (longest shipped name is 33 ASCII bytes) so not a regressed surface, but it violates protocol A4/B6 literally. | **Fixed**: the copy now uses `SalLegacyToU8Alloc(codeName, _countof(codeName) - 1)`, whose `maxBytes` clamp cuts only on a UTF-8 boundary, so the `lstrcpyn` afterwards cannot truncate at all. |
| **R-2** | Low | `WideCharToMultiByte(CP_ACP, 0, …)` allows *best-fit* transliteration, so `"Kāmeničtí"` (U+0101) could best-fit onto the bytes of `"Kameničtí"` and match. Can only turn a previous non-match into a match, never redirect a match, and none of the three callers is harmed by accepting more — not a regression, but worth tightening at zero cost. | **Fixed**: `WC_NO_BEST_FIT_CHARS` added. |
| **R-3** | Record defect | The fix deliberately contradicts `tasks.md` T062 (which prescribed normalizing `CCodeTablesData::Name` at intake). The reviewer verified the premise independently and confirms **T062 as written would have been the regression**: `dbviewer` persists such a name in `CfgDefaultCoding`/`CONFIG_DEFAULT_CODING` and feeds it back through `GetConversionTable`, and `filecomp` does the same with `ASCII8InputEncTableName`/`CONFIG_INPUTENCTABLE0/1`. Consequently `research.md` C7's note that "the conversion-name intake removes F-P4-02's second trigger" no longer holds — which is exactly why T2 exists as a separate change. | **Recorded**: `research.md` R4/C7 annotated, `tasks.md` T062 annotated, and stated in `closing-report.md`. |

## Pre-existing defects found on the changed paths

Recorded so they are not mistaken for this feature's work, and so they are not
lost. **None is introduced or widened by these fixes**, and none is in an 068
finding, so under FR-001 (no change without a finding behind it) they are
carried to the handoff rather than fixed here.

1. **`src/codetbl.cpp:873`** — `if (len > bufferLen) len = bufferLen - 1;`
   should be `>=`: a name of exactly `bufferLen` bytes writes
   `buffer[bufferLen]`, a **one-byte overflow** of `codeName[200]`
   (`viewer3.cpp:58`) and of `DefaultConvert[200]` (`viewer3.cpp:1904`).
   Unreachable with shipped names (longest 33 bytes) but a real out-of-bounds
   write. **Highest-value item in this list.**
2. **`src/viewer3.cpp:3291`** — `GetCodeType`'s return value ignored;
   `defCodeType` stays uninitialized when the tables are not loaded and is then
   used as `CM_CODING_MIN + defCodeType`.
3. **`src/zip.cpp:3292`** — `GetConversionTable` does not NULL-check
   `conversion`; the pre-fix loop faulted there too (`SalU8ToW(NULL, …)`
   returns 0 first, so no new crash and no new protection).
4. **`src/viewer3.cpp:30/35`** — `lstrcpyn(caption, FileName, MAX_PATH)` cuts a
   path longer than 259 bytes, possibly mid-sequence, which drops the whole
   caption to the ANSI draw; T1/T2 therefore do not help very long non-ASCII
   paths. Pre-existing feature-004 site.
5. **`src/plugins/filecomp/controls.cpp:24,39`** — unbounded `strcpy(Text,
   text)`, safe today only because the source and destination are both
   `[MAX_PATH]`.

## Timing and earlier scenarios

No changed site is a per-item path, so no G6 timing is owed: `GetCodeType` has
three callers (none per-item) and the alternate spelling is computed **once
before** the entry loop and is NULL for every ASCII and every legacy-byte input,
so the per-entry cost is unchanged. `WM_PAINT` and the worker caption run once
per paint / per comparison.

Earlier quickstart scenarios touched: **068 W12** (viewer caption — altered for
the better; cs was mojibake), **063** viewer-caption path only, **066 step 8**
unaltered (the core converters accept WTF-8, so that caption already took the
wide path), **067** not reached.

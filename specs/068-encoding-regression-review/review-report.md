# Encoding Regression Review — Report

**Feature**: 068-encoding-regression-review · **Started**: 2026-08-24
**Spec**: [spec.md](spec.md) · **Method**: [research.md](research.md) R2–R8 ·
**Records**: [data-model.md](data-model.md) · **Reference**:
[contracts/encoding-contract-checklist.md](contracts/encoding-contract-checklist.md) ·
**Inventory**: [inventory.md](inventory.md) · **Runbook**: [quickstart.md](quickstart.md)

## 1. Scope & method

- **Baseline**: commit `c577ff3` (2026-08-24 13:52 +0200, branch
  `068-encoding-regression-review` = `main` + nothing; last release tag
  `v0.1.4`, build 188). Unreleased delta on top of `v0.1.4`: features 065,
  066, 067 — see [delta-manifest.md](delta-manifest.md).
- **Scope** (spec Q1): the whole core application's encoding-handling code
  (`src/*.cpp`, `src/*.h`, `src/common/**` minus `dep/`) plus the plugin
  boundary; plugin-internal fixes only under FR-012; developer tooling and
  vendored code recorded, not fixed.
- **Vehicle** (research R2): seven charted perspectives as parallel read-only
  subagents ([charters.md](charters.md)); every finding verified by a
  separate refute-first agent; every fix regression-reviewed by a third
  agent; the main context orchestrates, fixes, runs gates, and never
  verdicts its own work.
- **Pre-fix reference binaries**: Debug tree
  `build\tandemcommander\Debug_x64\` (this baseline); Release copy
  `build\tandemcommander\Release_x64_prefix\` (T002).

### Baseline gates (T001, 2026-08-24 16:06)

| Gate | Result | Evidence |
|---|---|---|
| Debug full build (`build.cmd full`) | 0 errors, 43 s (warm intermediates), 19 plugins, 180 language modules | `scratchpad/build-debug-baseline.log`; 140 distinct warning lines captured in `warnings-baseline.txt` as the "no new warnings" reference |
| `saltests.exe` | **1229 checks, 0 failed** (baseline); **1235/0** after the T009 scaffold (`TestEncodingReview068`, 6 property checks) = working baseline | exit 0 |
| `python tools\check_encoding.py --strict` | **TOTAL 0** (6 strict rules) | exit 0 |
| Fixtures | `D:\Zkouška\Můj disk\` (3 files incl. real NBSP), `D:\Zkouška\Árvíztűrő tükörfúrógép\bájt.txt`, `D:\Zkouška\surrogate\` (Lone/twin D800/D801 + `dir<D800>sub\child.txt`), `%TEMP%\salamander-test\perf` (100,000 files), `unicode`, deep path (354 chars) | T003 log |

## 2. Coverage

*(T007 queue sizes; T018 accounting; filled during US1)*

| Queue | Lines | Accounted in inventory |
|---|---|---|
| `candidates/dc01-ansi-fs-shell-process-registry.txt` | 462 | pending |
| `candidates/dc02-cp-acp.txt` | 75 | pending |
| `candidates/dc03-05-19-loadstr-compositions.txt` | 160 (multi-line matches) | pending |
| `candidates/dc06-ansi-ui-sinks.txt` | 1215 | pending |
| `candidates/dc06b-message-token-sinks.txt` | 133 | pending |
| `candidates/dc08-tooltips.txt` | 6 | pending |
| `candidates/dc13-strict-probe.txt` | 3 | pending |
| `candidates/dc15-signed-char.txt` | 129 | pending |
| `candidates/dc18-missed-twins.txt` | 58 (21 distinct ids; `IDS_QUESTION` ×43) | pending |
| `candidates/converters.txt` | 250 | pending |
| `candidates/suppressions.txt` | 4 | pending |
| `candidates/registry-old-wrappers.txt` | 34 | pending |

Perspective coverage lists: see §"Perspectives" below (from `findings/P*.md`).

### T018 coverage accounting (SC-001)

`python coverage_check.py` classifies every candidate line three ways: **cited**
(the exact `file:line` appears in a report), **grouped** (its file is
inventoried or dismissed as a group — the charter permits grouping sites per
pattern-in-function), **gap** (its file is mentioned by no perspective).

| Queue | Lines | Cited | Grouped | Gap |
|---|---|---|---|---|
| dc01-ansi-fs-shell-process-registry | 462 | 462 | 0 | 0 |
| dc02-cp-acp | 75 | 75 | 0 | 0 |
| dc03-05-19-loadstr-compositions | 160 | 88 | 72 | 0 |
| dc06-ansi-ui-sinks | 1215 | 181 | 1013 | **21** |
| dc06b-message-token-sinks | 133 | 33 | 100 | 0 |
| dc08-tooltips | 6 | 2 | 4 | 0 |
| dc13-strict-probe | 3 | 2 | 1 | 0 |
| dc15-signed-char | 129 | 21 | 108 | 0 |
| dc18-missed-twins | 58 | 29 | 29 | 0 |
| converters | 250 | 143 | 107 | 0 |
| registry-old-wrappers | 34 | 29 | 5 | 0 |
| suppressions | 4 | 4 | 0 | 0 |
| **total** | **2529** | **1069** | **1439** | **21** |

The 21 gaps were resolved by hand and are classified here:

| Lines | File | Classification | Evidence |
|---|---|---|---|
| 4 | `src/toolbar1.cpp:1034,1041,1104,1124` | verified-correct | non-text messages (`WM_USER_TBDROPDOWN`, `WM_MOUSEMOVE`, `WM_NOTIFY`, `WM_USER_TBGETTOOLTIP` — the tooltip answer is composed elsewhere, covered by P2's DC-08 group) |
| 3 | `src/fileswnb.cpp:320,863,1129` | verified-correct | internal notifications with no text argument (`WM_USER_DONEXTFOCUS`, `WM_USER_INACTREFRESH_DIR`, blind forward of `uMsg`) |
| 10 | `src/common/messages.h:74,93,116,121,140,145,164,167,180,186` | out-of-scope (debug facility) | the `C__Messages`/`C__MessagesW` debug message-box wrapper; captions are literals, both a narrow and a wide class exist. Header — never scanned by the guard (P7 blind-spot item) |
| 3 | `src/common/allochan.cpp:111,122,138` | verified-correct | allocation-handler warnings built from fixed ASCII literals |
| **1** | **`src/filesmap.cpp:146`** | **defective → F-MC-01** | see below |

**F-MC-01 — panel hit-test geometry measures UTF-8 names with the ANSI
text-extent API** (raised by the coverage check, not by a perspective —
`src/filesmap.cpp` is mentioned in **no** perspective report).

- **Site**: `src/filesmap.cpp:146` `GetTextExtentPoint32(dc, s, len, &sz)`.
- **Defect class**: DC-12 (byte-width measurement on UTF-8) + DC-06.
- **Data path**: `formatedFileName` is declared `char[SAL_FIND_NAME_U8]` and
  commented "UTF-8 name (feature 004)" (`:107`) → `AlterFileName(formatedFileName,
  f->Name, …)` (`:119`) → `s = formatedFileName` (`:122`) → `len = f->NameLen`
  or the extension split, both **byte** counts (`:134,:137`) → the ANSI
  `GetTextExtentPoint32` at `:146`, which reads those bytes as CP1250
  characters.
- **Failure scenario**: Czech UI/locale, panel in Brief or Detailed view with
  *Full row select* **off**; for a name like `žluťoučký kůň.docx` every 2-byte
  UTF-8 character is measured as two CP1250 characters, so the item's
  clickable/selection width in `CFilesMap` is computed far too wide — mouse
  hit-testing and drag-selection regions disagree with what is drawn.
- **Reference implementation in the sibling file**: `src/fileswn4.cpp:711`
  converts with `SalU8ToW` and `:738` measures with `GetTextExtentPoint32W` —
  i.e. the drawing path is correct and only the map path was missed (a DC-18
  missed twin).
- Verdict: **pending** independent verification like every other finding.


**Status 2026-08-24 (checkpoint)**: Phases 1–2 complete (baseline, fixtures,
queues, charters, draft guard rules, test scaffold). Phase 3 (US1) was
launched — all seven perspectives P1–P7 terminated early on an account
session limit before writing any `findings/P*.md`; **no perspective output
exists yet** and no product code has been changed by the review. Re-run
tasks T011–T016 (perspectives) and P7's design task, then T017/T018.


## 3. Inventory summary

*(from inventory.md at consolidation)*

## 4. Defect-class sweep

*(inventory.md table, final status)*

## 5. Contract compliance

<!-- BEGIN GENERATED: contracts -->
| Obligation | Persp. | Verdict + evidence |
|---|---|---|
| B1.1 | P5 | **deviation → F-P5-06** (documentation) and **→ F-P5-03, F-P5-08, F-P5-09, F-P5-11** (practice) · the invariant is stated only in `spl_com.h:205-210` for `CFileData::Name`; `spl_fs.h:244,250,260,265,330` state nothing, and the core normalizes no FS path intake. Several enabled plugins do *not* convert for display (`zip/dialogs.cpp:1839`) or convert lenient… |
| B1.2 | P5 | not P5's surface (P1/P6 own the manifest); the plugin-side consequence is a deviation: sftp relies on it instead of the `\\?\` prefix (F-P5-11) while ftp and `SplU8ToWExtAlloc` use the prefix · `splunicode.h:94-119` vs `sftp/operats.cpp:35-42` |
| B10.1 | P3 | **compliant** · `salunicode.cpp:131-202` — the decoder's only relaxation is `seq==3 && cp in D800..DFFF`; all overlong/range/continuation checks intact |
| B10.2 | P3 | **compliant** · `salunicode.cpp:288-292` reaches the WTF-8 encoder for every lone-surrogate input; encoder (`:52-96`) and decoder (`:131-202`) are exact inverses on 16-bit unit sequences; `SalU8ToWDisplay*` is documented and used only on draw paths — except F-P3-08 |
| B10.3 | P3 | **compliant in core; open in plugins** · core has **zero** raw strict probes outside the converter module (seed C-g); `salamdr6.cpp:2414` documents why. The remaining ones are the L48 plugin-shared helpers plus four archive plugins that probe **entry names** — P5's scope |
| B10.4 | P3 | **compliant, with F-P3-06 as a caveat** · `SAL_FIND_NAME_U8 = 3*MAX_PATH` (`salfileio.h:31`) and the `3*MAX_PATH`/`3*100`/`3*300` buffers everywhere; `SalCompareNamesUTF8` falls back to `memcmp` (`salunicode.cpp:600-607`). Caveat: "byte equality ⇔ unit equality" is honoured by `memcmp`/`strcmp` but **… |
| B10.5 | P5 | **compliant as written; the premise is now wrong** → F-P5-04 · `splunicode.h:29,43,59,70` strict; the core produces WTF-8 since 066 (`src/common/salunicode.cpp`). Amending B10.5 is part of the fix, not a side effect. |
| B11.1 | P3 | **compliant** · `salamdr1.cpp:2922-2973` (UTF-8 separators, byte arithmetic, 45 ≤ 50 worst case); `salamdr6.cpp:424` selects `loadStr` once from `u8` and uses it for every fragment; `ExpandPluralString` (`salamdr4.cpp:411+`) copies bytes; `u8=FALSE` mixing is latent-only (S-X… |
| B11.2 | P2 | **deviation, currently latent** · `src/dialogs.cpp:947→951`, `:966→970/973`, `:1041→1042`; `src/drivelst.cpp:2626→2629`; `src/fileswn8.cpp:1403→1404`; `src/dialogs5.cpp:3395→3402` all compose `PrintDiskSize`/`SalGetDateFormatU8` output with an ANSI `LoadStr` template. The contract's own §7 alr… |
| B11.3 | P5 | **compliant** (the freeze holds exactly) — and the frozen behavior is itself the defect → F-P5-07 · `zip.cpp:1397,1402,3870,3878,5173` all call the free functions without `u8`; `consts.h:497` defaults it `FALSE`; `plugins.h:1989` vtable slot has 3 parameters. No core consumer of the forwarders exists. |
| B12.1 | P5 | **compliant** · the six normalizing intakes listed above; the ASCII-by-nature fields are only ever compared/appended, and `undelete/fs1.cpp:74` folding an FS name is safe for exactly that reason. |
| B12.2 | P3 | **deviation → F-P3-06** · storage and NFC discipline are respected (`SalNormalizeNFC` output is transient, `salunicode.cpp:555-563`, `masks.cpp:136-141`, `sort.cpp:29`), but the `StrICmp`/`IsAlpha` family *case-folds and classifies* stored name bytes with an ACP table (`common/str.cpp:… |
| B2.1 | P3 | **deviation → F-P3-04, F-P3-05** · `winlib.cpp:1057` limits the control in characters while the buffer is sized in bytes (the "≥3×" rule is inverted); `winlib.cpp:1058/1091` use thunked messages on ANSI dialogs |
| B2.2 | P3 | **mostly compliant, one deviation** · compliant: `gui.cpp:660-836` (END_ELLIPSIS guards at `:703-704`), `salamdr4.cpp:186,241`, `fileswn2.cpp` column paths. Deviation: `gui.cpp:730-793` STF_PATH_ELLIPSIS has no surrogate guard (S-X-P3-019, display-only) |
| B2.3 | P3 | **deviation → F-P3-05** · the fallback exists everywhere (`winlib.cpp:1113,1166,1199,…`, `gui.cpp:627`), but the *primary* wide path itself yields literal `?` on an ANSI dialog. Also `gui.cpp:628-632` keeps a Latin-1 widening last resort that B9.3 forbids (S-X-P3-016, unreachable) |
| B2.4 | P3 | **compliant (by `/J`, not by casts)** · `sal_base.props:14`, `plugin_base.props:16`, runtime assert `salamdr1.cpp:3811-3821`; no explicit `(unsigned char)` cast at `fileswn8.cpp:62,110` or `gui.cpp:648` — the guarantee is a build flag, which is fragile but currently effective |
| B3.1 | P2 | **deviation** → F-P2-09, F-P2-11, F-P2-12 · `src/plugins2.cpp:1054,1056`, `src/dialogs5.cpp:1086`, `src/dialogsp.cpp` ×22 pass UTF-8 values to ANSI sinks. The rest of the corpus (302 of 455 text-bearing lines) complies via the wide-then-fallback or homogeneous-ANSI patterns |
| B3.2 | P3 | **deviation → F-P3-05, F-P3-07** · F-P3-05 produces `?`; F-P3-07 turns a torn clamp into a whole-string CP_ACP render |
| B3.3 | P3 | **compliant in the machinery, two deviations at consumers** · compliant: `gui.cpp`, `salamdr4.cpp`, `fileswn2/4.cpp`, `fileswn5.cpp` (byte→WCHAR offset idiom `SalU8ToW(s, byteLen, NULL, 0)`), `stswnd.cpp:186-…` (`U8BytesToWChars`). Deviations: `stswnd.cpp:1854` byte clamp (F-P3-07), `finddlg1.cpp:1290-1341` ANSI width me… |
| B3.4 | P3 | **deviation → F-P3-05 (C5)** · C4 compliant (`CStaticText::SetText`, `CStatusWindow::SetText` keep mirrors). C5 violated at `dialogs3.cpp:1201` and `finddlg1.cpp:1778-1846`: the actioned path/mask is re-read from an ANSI control instead of the stored value |
| B3.5 | P2 | **compliant** in the paths I own · quick rename reads `GetWindowTextW` first and only falls back (`src/fileswn5.cpp:2617-2621`, `:2827-2831`); `CTransferInfo::EditLineW` exists for wide fields. The one re-read pattern I found (Group G4) feeds *display*, not an action — except `src/dialogsp.cpp`… |
| B3.6 | P4 | **deviation → F-P4-04, F-P4-05** · The whole `SetValue`/`GetValue`/`GetSize` configuration surface complies (`regwork.cpp:118,164,206,216,250`), and so does every `REG_SZ` value in `mainwnd2.cpp` (S-B7-P4-031). Three classes of exception remain: (1) **`RegEnumValueA`** is used where the facade … |
| B3.7 | P3 | **compliant** · the core still builds without `UNICODE` (`winlib.h:82-131` `#ifndef _UNICODE` blocks); no `activeCodePage` anywhere; every path read has an ASCII fast path or an identity argument (`SalIsASCII` in `sort.cpp:19`, `masks.cpp:130`, `salamdr2.cpp:1015`) |
| B4.1 | P3 | **compliant** · `SalGetLocaleInfoU8`/`SalGetDateFormatU8`/`SalGetTimeFormatU8` call the W API and transcode (`salunicode.cpp:719-780`) |
| B4.2 | P3 | **compliant** · the only remaining direct `GetLocaleInfo(` in the shipped core are `bugreprt.cpp:1733,1735,1740,1742,1747` and `salamdr1.cpp:3857` — exactly the six recorded exemptions (the checklist's line number 3824 has drifted to 3857). `translator/` and `tserver/` are de… |
| B4.3 | P3 | **compliant** · `consts.h:1713-1715` / `salamdr1.cpp:135-137`: `char DecimalSeparator[16]`, `char ThousandsSeparator[16]`; `DecimalSeparatorLen`/`ThousandsSeparatorLen` are byte counts (`salamdr1.cpp:957-976`); the 4-byte cap keeps `NumberToStr`'s 20-digit worst case at 20+6×… |
| B4.4 | P5 | **compliant** · `zip.cpp:1397-1400` and `:5173-5177` forward unchanged; `salamdr1.cpp` `NumberToStr` inserts the UTF-8 `ThousandsSeparator`. The *consequence* — mixing with ANSI `LoadStr` — is B11.3's problem, not this one. |
| B4.5 | P3 | **deviation → F-P3-08** · `stswnd.cpp:164` builds the mirror leniently and `stswnd.cpp:698-710` converts it back into the value handed to ChangeDir / clipboard / plugins |
| B4.6 | P2 | **compliant for the information line; deviation for the Find dialog's *caption* and *error log*** → F-P2-03 · the information line itself uses `ExpandPluralBytesFilesDirs(..., u8=TRUE)` and the wide draw path (`src/fileswn4.cpp` wide branches, `src/stswnd.cpp:718-726`); but the contract explicitly extends readability to the Find dialog, and the Find caption (`finddlg1… |
| B5.1 | P2 | **deviation** → F-P2-01, F-P2-02, F-P2-03, F-P2-04, F-P2-05, F-P2-06, F-P2-07, F-P2-10, F-P2-13 · 30+ surviving `LoadStr(` format sites compose a name/path/error text; the contract's own rationale ("all-or-nothing") is what makes them visible |
| B5.2 | P5 | **compliant** on the freeze reading (`zip.cpp:1625` `LoadStr` untouched, `SalMessageBox` forwarders unchanged); **deviation** on the verification reading → F-P5-09 is precisely a defect English hides (the English format string is ASCII, so `SplU8ToWAlloc` succ… · `zip.cpp:1625-1633`; `filecomp/mainwnd.cpp:2036-2046` |
| B5.3 | P3 | **deviation → F-P3-05** · message boxes are ANSI dialogs, so non-ACP characters render as literal `?` — the contract's own definition of a defect |
| B5.4 | P2 | **compliant** in the sinks I own · none of the sinks in Part 3 substitutes `?`; the ANSI fallback preserves bytes verbatim (`src/common/winlib.cpp:1112,1176,1199,1212,1231`), and `CStaticText`/`CMessageBox` use the lenient mirror only for display |
| B5.5 | P2 | **compliant for the disp-info route; deviation for the direct-set route** → F-P2-09, F-P2-11 · `src/finddlg1.cpp:3016` and `src/packac.cpp:70-73` both send `NF_REQUERY` and answer `NFR_UNICODE` (`finddlg1.cpp:4001-4003`, `packac.cpp:184-187`); no `CP_ACP` in either ANSI handler. The other name-carrying list views set text directly and must use `SalListV… |
| B5.6 | P6 | **compliant** · *Requery*: `src/finddlg1.cpp:3016` sends `SendMessage(FoundFilesListView->HWindow, WM_NOTIFYFORMAT, (WPARAM)HWindow, NF_REQUERY)` from `WM_INITDIALOG`, and `:4001-4004` answers `NF_QUERY`/`NF_REQUERY` with `NFR_UNICODE` via `DWLP_MSGRESULT` (not `return NFR_UN… |
| B6.1 | P5 | **compliant for `CPluginData`**, **deviation for the neighbouring packer/unpacker titles** → F-P5-05 · normalized: `plugins1.cpp:1244` (`ChDrvMenuFSItemName`), `:1605-1631` (Name/Version/Copyright/Extensions/Description), `:1919` (`CPluginMenuItem::Name`). Not normalized: `plugins1.cpp:584,610` → `packers.cpp:734`. Latent because all enabled titles are ASCII. |
| B6.2 | P2 | **deviation (incomplete identifier list)** · `tools/check_encoding.py:187-199` tracks `plugin->Name`, `Plugin->Name`, `p->Name`, `pluginData->Name`, `pluginName`, `\w+->Description`, `\w+->Copyright`, `\w+->Extensions`, `\w+->ChDrvMenuFSItemName` — but **not** `DLLName`, `Version`, or `item->Name` for pl… |
| B7.1 | P1 | **deviation** → F-P1-01, F-P1-04, F-P1-11, F-P1-19, F-P1-20, F-P1-21 · 100+ ANSI FS/shell calls still reached from `GetPath()`/`CFileData::Name`/`SalGetTempFileName` output; the contract was applied to the 058 sites only |
| B7.2 | P1 | **compliant with a caveat** → F-P1-16, F-P1-18 · the fallbacks exist at all four 058-era sites (`src/geticon.cpp:364`, `src/snooper.cpp:584/733/770`, `src/shellib.cpp:113/1628`, `src/shiconov.cpp:932`), but three of them do not check the fallback's own result and use the buffer anyway |
| B7.3 | P1 | **deviation** → F-P1-25, F-P1-26, F-P1-28 · `FindFirstChangeNotificationA` is gone (seed C-e), but `MultiByteToWideChar(CP_ACP,…)` on a UTF-8 panel value survives at `src/shellsup.cpp:536`, `src/fileswn0.cpp:341`, `src/fileswn2.cpp:150`, `src/shellib.cpp:2649`, `src/worker.cpp:6212`, `src/dialogs6.cpp:5… |
| B7.4 | P1 | **compliant** · `src/geticon.cpp:350-368` still does `SalU8ToWAlloc` first with a CP_ACP fallback; the signature and the fallback are intact (the unchecked-result bug in F-P1-18 does not change the documented behaviour for valid input) |
| B7.4 | P5 | **compliant** · the exported `GetFileIcon` forwarder is unchanged and 058's W overload was added below the boundary (`geticon.cpp`); no plugin-visible signature moved. Offsets: one deviation found, `zip.cpp:5868,5966` measures a UTF-8 path/name in bytes against `MAX_PATH` (No… |
| B8.1 | P1 | **compliant inside the chain** · `MyGetDriveType` (`src/salamdr2.cpp:1743-1772`) uses `SalGetDriveTypeU8`; the recycle path (`src/fileswn8.cpp:130-156`) allocates from the measured length |
| B8.2 | P1 | **compliant** · `src/fileswn8.cpp` gate unchanged; no new classification input found |
| B8.3 | P1 | **compliant** · unchanged since 062 |
| B8.4 | P1 | **compliant** · `DeleteThroughRecycleBinAuxW` is the only live path; the ANSI helper is dead code (S-B2-P1-068) |
| B8.5 | P1 | **compliant** · unchanged |
| B9.1 | P4 | **compliant** · `src/mainwnd4.cpp:243-345`: temp file via `SalGetTempFileName` (`:250`), `SalCreateFile` (`:283`), BOM written **only** for the viewer destination (`:313-315`), clipboard via `CopyTextToClipboardU8(buff, fileSize, FALSE, NULL)` (`:328`), file destination gets … |
| B9.2 | P4 | **compliant** · `src/salamdr4.cpp:1246` is unchanged and is exported unchanged through `src/zip.cpp:2418`. Exactly **one** core caller remains — `src/stswnd.cpp:2266` — and it is the `else` arm of a `TextW != NULL ? CopyTextToClipboardW(...) : ...` ternary whose condition can… |
| B9.3 | P3 | **partial deviation → F-P3-07 (+ S-X-P3-016)** · "UTF-8 at rest" and "tolerant convert" are compliant (`gui.cpp:896` `SalLegacyToU8Alloc`, `tooltip.cpp:309-313`). "Clamps cut on UTF-8 boundaries" holds only in `gui.cpp:982-1000`; `stswnd.cpp:1854,1878,1884` and `drivelst.cpp:2697` clamp blindly. The Latin-1 … |
| B9.4 | P3 | **compliant** · `salamdr2.cpp:1015`, `:1038-1044`, `:1049-1056` |
| B9.5 | P1 | **compliant for `SalGetTempFileName`**, **deviation for its consumers** → F-P1-01, F-P1-06, F-P1-21 · `src/salamdr3.cpp:223-244` uses `GetTempPathW`/`GetSystemDirectoryW` + `SalWToU8` and creates via `SalCreateFile`/`SalCreateDirectory`; but 40+ call sites then delete/enumerate that same output with ANSI APIs |

**51 contract obligations carry a verdict.**
<!-- END GENERATED: contracts -->

## 6. Findings

<!-- BEGIN GENERATED: findings -->
| ID | Persp. | Claim | Verdict | Batch | Disposition |
|---|---|---|---|---|---|
| F-P1-01 | P1 | Disk-cache temp files and directories are deleted through ANSI APIs, so nothing is cleaned up when `%TEMP%` is non-ASCII | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-02 | P1 | The disk cache never reuses a temp directory when `%TEMP%` is non-ASCII | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-03 | P1 | Startup cleanup of leftover `SAL*.tmp` directories misses them under a non-ACP `%TEMP%`, and hands an ACP path to a UTF-8 consumer | **CONFIRMED (in part)** | V5 | fix candidate (scope test T020) |
| F-P1-04 | P1 | `RemoveTemporaryDir` / `RemoveEmptyDirs` are ANSI end to end, so no non-ASCII temp tree is ever removed (also a plugin-facing service) | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-05 | P1 | External-archiver list files are written in the wrong encoding (`CharToOem` on UTF-8), and archiver output is parsed as OEM into UTF-8 name fields | **CONFIRMED (in part)** | V5 | fix candidate (scope test T020) |
| F-P1-06 | P1 | The external-packer subsystem uses ANSI file APIs on UTF-8 temp/archive paths throughout | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-07 | P1 | `salspawn.exe` path taken with `GetModuleFileName` (A) | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-08 | P1 | `SHGetFolderPath` (A) results are fed to the strict UTF-8 facade, so per-user data under a non-ASCII account name is never found | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-09 | P1 | OneDrive / Dropbox root paths are stored as CP_ACP and then used as panel paths (ledger L02) | **CONFIRMED (in part)** | V5 | fix candidate (scope test T020) |
| F-P1-10 | P1 | Install-directory paths obtained with `GetModuleFileName` (A) are consumed by strict UTF-8 helpers | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-11 | P1 | WITHDRAWN by the author before submission (`SetCurrentDirectory` on the panel path, ledger L01) | **WITHDRAWN (by author)** | — | no change |
| F-P1-12 | P1 | `MyGetVolumeInformation`/`MyGetDiskFreeSpace` degrade silently on non-ASCII and UNC paths (ledger L03) | **CONFIRMED (in part)** | V5 | fix candidate (scope test T020) |
| F-P1-13 | P1 | `subst` targets are resolved through the ANSI `QueryDosDevice`, producing mixed-encoding paths (ledger L04) | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-14 | P1 | Volume labels, drive display names and mapped-drive UNC paths are acquired in CP_ACP (ledger L05) | **CONFIRMED (in part)** | V5 | fix candidate (scope test T020) |
| F-P1-15 | P1 | `GetDriveType` called on a full path instead of a root | **REFUTED** | V5 | no change |
| F-P1-16 | P1 | `snooper.cpp` uses an indeterminate stack buffer when both conversions fail (ledger L08/L09) | **REFUTED** | V5 | no change |
| F-P1-17 | P1 | `MakeCopyWithBackslashIfNeeded` result is never used in `snooper.cpp` (pre-existing, not a regression) | **REFUTED** | V5 | no change |
| F-P1-18 | P1 | `SHILCreateFromPath` passes an uninitialised buffer to `ParseDisplayName` when both conversions fail (ledger L10/L11) | **REFUTED** | V5 | no change |
| F-P1-19 | P1 | Compare Directories cannot read files or subdirectories with non-ASCII names | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-20 | P1 | Writing an edited file back into an archive uses the ANSI `SHFileOperation` | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-21 | P1 | Assorted ANSI file APIs on UTF-8 paths outside the main copy/move engine | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-22 | P1 | User-menu icons are loaded with the ANSI `ExtractIconEx` | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-23 | P1 | Environment-variable expansion mixes ACP values into UTF-8 paths | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-24 | P1 | Common file/folder dialogs return CP_ACP paths into UTF-8 fields | **CONFIRMED (in part)** | V5 | fix candidate (scope test T020) |
| F-P1-25 | P1 | UTF-8 names widened through CP_ACP before being handed to the shell/OLE | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-26 | P1 | Dropped files are read through CP_ACP even when the payload is wide | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-27 | P1 | Shell/network strings degraded to CP_ACP at intake | **CONFIRMED** | V5 | fix candidate (scope test T020) |
| F-P1-28 | P1 | Icon-overlay and icon-location paths degraded to CP_ACP (058 twins) | **REFUTED** | V5 | no change |
| F-P2-01 | P2 | Copy/Move error message boxes mix an ANSI template with a UTF-8 path and error text | **CONFIRMED (in part)** | V4 | fix candidate (scope test T020) |
| F-P2-02 | P2 | "cannot execute viewer/editor" and user-menu execute errors mix template + command line + error text | **CONFIRMED** | V4 | fix candidate (scope test T020) |
| F-P2-03 | P2 | Find: the Errors/Info log garbles error text, and the window caption garbles the mask | **CONFIRMED** | V4 | fix candidate (scope test T020) |
| F-P2-04 | P2 | The safe-wait window ("Reading path …", "Checking path …", plugin loading) mixes template + path | **CONFIRMED** | V4 | fix candidate (scope test T020) |
| F-P2-05 | P2 | Copy/Move progress dialog subject (`CTruncatedString`) mixes an ANSI title template with the UTF-8 file name | **REFUTED** | V4 | no change |
| F-P2-06 | P2 | Send-by-email, plugin-load errors, icon-overlay crash box, env/UNC/viewer boxes | **CONFIRMED (in part)** | V4 | fix candidate (scope test T020) |
| F-P2-07 | P2 | Drive Information: the "type" line mixes ANSI templates with UNC / SUBST / link-target paths | **CONFIRMED** | V4 | fix candidate (scope test T020) |
| F-P2-08 | P2 | WITHDRAWN (refuted by the sink's per-item probe) | **WITHDRAWN (by author)** | — | no change |
| F-P2-09 | P2 | Plugin Manager: only the Name column was converted; Location (and Version) stayed ANSI | **CONFIRMED** | V4 | fix candidate (scope test T020) |
| F-P2-10 | P2 | Plugin Manager: the "Change Drive menu" checkbox mixes an ANSI template read from the dialog with a UTF-8 FS item name | **CONFIRMED** | V4 | fix candidate (scope test T020) |
| F-P2-11 | P2 | Plugin "Keyboard Shortcuts" dialog lists command names through the ANSI list-view call | **CONFIRMED** | V4 | fix candidate (scope test T020) |
| F-P2-13 | P2 | "Save Configuration" export-exists box mixes an ANSI template with a UTF-8 configuration path | **CONFIRMED** | V4 | fix candidate (scope test T020) |
| F-P2-12 | P2 | Custom Packers / Unpackers / Viewers-Editors config pages show UTF-8 command paths through ANSI `WM_SETTEXT`/`WM_GETTEXT` | **CONFIRMED** | V4 | fix candidate (scope test T020) |
| F-P3-01 | P3 | `SalU8ToW` conflates "invalid input" with "buffer too small" (DC-20) | **REFUTED** | V2 | no change |
| F-P3-02 | P3 | `srcLen == 0` is reported as conversion failure (DC-20) | **LATENT** | V2 | deferred — latent |
| F-P3-03 | P3 | the file facade fails operations on ANSI-producer paths instead of falling back (DC-09) | **LATENT** | V2 | deferred — latent |
| F-P3-04 | P3 | `EM_LIMITTEXT` is set in bytes but enforced in characters; overflow silently degrades to a CP_ACP read (DC-12 + DC-09) | **CONFIRMED** | V1 | fix candidate (scope test T020) |
| F-P3-05 | P3 | wide setters on ANSI dialogs: every character outside the ACP becomes `?` (DC-16) | **CONFIRMED** | V2 | fix candidate (scope test T020) |
| F-P3-06 | P3 | ACP byte tables applied to UTF-8 names: `StrICmp` / `IsAlpha` give wrong answers (DC-15, seed C-f) | **CONFIRMED (in part)** | V2 | fix candidate (scope test T020) |
| F-P3-07 | P3 | status-bar tooltip truncates a UTF-8 path mid-sequence, costing the whole hint (DC-12) | **CONFIRMED** | V2 | fix candidate (scope test T020) |
| F-P3-08 | P3 | status-window hot-track path is rebuilt from a LENIENT conversion and then used operationally (DC-14) | **LATENT** | V2 | deferred — latent |
| F-P3-09 | P3 | Find → Delete never takes the wide shell path for an all-ASCII selection (off-by-one) | **REFUTED** | V2 | no change |
| F-P4-01 | P4 | the viewer's default character-set conversion is silently lost on every restart when its name is non-ASCII (Kamenické / KOI-8 ČS2) | **CONFIRMED** | V3 | fix candidate (scope test T020) |
| F-P4-02 | P4 | a non-ASCII coding name poisons the whole viewer window caption, mojibaking the file name | **CONFIRMED (in part)** | V3 | fix candidate (scope test T020) |
| F-P4-03 | P4 | custom packer/unpacker titles have no defined encoding at rest (the class L17 names) | **CONFIRMED** | V3 | fix candidate (scope test T020) |
| F-P4-04 | P4 | the per-drive "remembered directory" is read back from the registry through the ANSI API although it was written UTF-8: a non-ASCII remembered path is… | **CONFIRMED** | V3 | fix candidate (scope test T020) |
| F-P4-05 | P4 | the OneDrive folder path is obtained in CP_ACP and then used as a UTF-8 panel path | **CONFIRMED** | V3 | fix candidate (scope test T020) |
| F-P4-06 | P4 | the Windows Jump List is built through the ANSI `IShellLink`: a non-ASCII hot path shows as mojibake in the taskbar and does not open when clicked | **CONFIRMED** | V3 | fix candidate (scope test T020) |
| F-P4-07 | P4 | configuration fields documented as UTF-8 are seeded from ANSI `LoadStr` | **CONFIRMED** | V3 | fix candidate (scope test T020) |
| F-P5-01 | P5 | plugin "Copy Name/Full Name" puts CP1250-mojibake on the clipboard | **CONFIRMED** | V6 | fix candidate (scope test T020) |
| F-P5-02 | P5 | exported ACP case tables corrupt UTF-8 names passed by plugins | **CONFIRMED (in part)** | V6 | fix candidate (scope test T020) |
| F-P5-03 | P5 | the plugin-facing browse dialogs are ANSI; paths outside the ACP are lost | **CONFIRMED** | V6 | fix candidate (scope test T020) |
| F-P5-04 | P5 | plugin-shared converters reject the WTF-8 names the core hands them | **BY-DESIGN** | V6 | no change — by design |
| F-P5-05 | P5 | custom packer/unpacker titles bypass the 052 normalization | **LATENT** | V6 | deferred — latent |
| F-P5-06 | P5 | the FS plugin interface states no encoding for any path method | **CONFIRMED** | V6 | fix candidate (scope test T020) |
| F-P5-07 | P5 | `PrintDiskSize`/`NumberToStr` across the plugin boundary produce a mixed-encoding buffer | **BY-DESIGN** | V6 | no change — by design |
| F-P5-08 | P5 | ZIP overwrite dialogs show file names through an ANSI `WM_SETTEXT` | **CONFIRMED** | V6 | fix candidate (scope test T020) |
| F-P5-09 | P5 | filecomp blanks its own window title in every non-English UI | **CONFIRMED** | V6 | fix candidate (scope test T020) |
| F-P5-10 | P5 | the Registry Editor Find results list has no live `LVN_GETDISPINFO` handler | **CONFIRMED** | V6 | fix candidate (scope test T020) |
| F-P5-11 | P5 | sftp converts local paths leniently and never uses the extended-length form | **CONFIRMED (in part)** | V6 | fix candidate (scope test T020) |
| F-P5-12 | P5 | `GetErrorText` is UTF-8 but the SDK does not say so; 127 plugin sites compose it with ANSI text | **CONFIRMED** | V6 | fix candidate (scope test T020) |
| F-P5-13 | P5 | `AlterFileName` byte-folds UTF-8 names for the panel name-format option | **CONFIRMED** | V6 | fix candidate (scope test T020) |
| F-P6-01 | P6 | mdview's keeper window class is never unregistered, so after a Plugins Manager Unload the "instant view" engine keeper silently never arms again | **CONFIRMED** | V1 | fix candidate (scope test T020) |
| F-P6-02 | P6 | feature 052 made `ChDrvMenuFSItemName` UTF-8 but left its checkbox template ANSI, so the Plugins Manager now shows the plugin's Change-Drive item name… | **CONFIRMED** | V1 | fix candidate (scope test T020) |
| F-P6-03 | P6 | a path the panel already holds is destroyed before the user can act on it: Change Directory and Find "Look in" prefill an ANSI dialog through the ACP | **CONFIRMED** | V1 | fix candidate (scope test T020) |
| F-P6-04 | P6 | Ctrl+Enter / Ctrl+Space push raw UTF-8 bytes into the ANSI command line, so the command runs against a name that does not exist | **CONFIRMED (in part)** | V1 | fix candidate (scope test T020) |
| F-P6-05 | P6 | two unbounded copies in the command line overflow 260-byte stack buffers from a long or non-ASCII name/path | **CONFIRMED** | V1 | fix candidate (scope test T020) |
| F-P6-06 | P6 | typed text outside the ANSI code page is silently replaced by `?` on every input surface except the four Unicode windows — including password fields | **CONFIRMED** | V1 | fix candidate (scope test T020) |

**74 live findings** (76 raised, 2 withdrawn by their author). Verdicts: BY-DESIGN 2, CONFIRMED 60, LATENT 4, REFUTED 8, WITHDRAWN 2. Every verdict is written by an independent refute-first verifier that did not raise the finding (research R5); only CONFIRMED findings may drive a code change (FR-006/FR-007).
<!-- END GENERATED: findings -->

## 7. Fixes

### 7.0 Scope test (T020, FR-012/FR-015)

The 60 confirmed findings were split by whether a **minimal** fix exists — the
test FR-007 imposes and the user's "under no circumstances introduce a
regression" constraint enforces.

**Group A — fix in this feature** (contained: a local change with an
enumerable blast radius, no shared machinery redesign).

**Group B — deferred with justification** (no minimal fix exists; each is
feature-sized, each would touch code that Group A's fixes also touch, and
attempting them inside a *stabilization* feature would create exactly the
regression risk this feature exists to remove). These are not dismissals:
each carries verified evidence and a scoping note for its own feature.

| # | Deferred (Group B) | Findings | Why no minimal fix |
|---|---|---|---|
| B-1 | **ANSI dialog windows** — 88 of 90 dialogs never opt into Unicode, so USER32 thunks wide text through the ACP | F-P3-05, F-P6-03, F-P6-06, F-P2-12, F-P3-04 | The fix is per-dialog (`unicodeWnd`) across ~88 dialogs plus property-sheet pages, which are ANSI *unconditionally*. V2 additionally found a crash hazard: `CKeyForwarderWindow` (`msgbox.cpp:16-21`) would ANSI-ify the buttons of a newly-Unicode message box. Needs its own feature with per-dialog verification. |
| B-2 | **ACP byte tables on UTF-8 names** — `LowerCase[]`/`IsAlpha[]`, `StrICmp`, `IsTheSamePath` | F-P3-06, F-P5-02 | Replacing the case-folding used by *all* name comparison changes sorting, focus-by-name, path identity and auto-refresh at once. V2 found it reaches ~20 sites via `IsTheSamePath` that the finding did not list. |
| B-3 | **`GetErrorText` is UTF-8 but undocumented** in the SDK | F-P5-12 (+ the core half in F-P2-01/02/03/06) | V6 corrected the count to ~27 defective sites in 5 plugins (not 127/19) **and** showed a naive sweep would *regress* FTP, whose own `FTPGetErrorText` is internally consistent ANSI. Needs an SDK contract statement plus per-plugin work. |
| B-4 | **`AlterFileName` byte-folds UTF-8** | F-P5-13 | V6: highest-risk fix in the review — `AlterFileName` also drives **Change Case, which renames files on disk**. Requires its own regression matrix. |
| B-5 | **Plugin-facing ANSI browse dialogs / services** | F-P5-03, F-P1-21 (plugin-facing group), F-P5-07 (`NumberToStr` re-widening) | Changing them alters bytes plugins receive; FR-009 freezes that. Needs an interface-version decision. |
| B-6 | **The remaining Group-A-shaped sites not yet reached** | the rest of the confirmed P1/P2 sets | Recorded per finding; each is individually fixable and is queued behind the Group-A set below. |

### 7.1 Fixes applied

| ID | Finding | Change (files) | Affected surfaces | Regression review | Byte-identity | Check |
|---|---|---|---|---|---|---|
| X01 | F-P6-05 (memory safety) | `src/editwnd.cpp`, `src/editwnd.h` — buffer sized `SAL_FIND_NAME_U8 + 2` + boundary-safe clamp (Ctrl+Enter); `CSalPathBuf` replaces `char[MAX_PATH]` + `strcpy` (Ctrl+Space/[/]); `InsertText` takes `const char*` | command line insert-name / insert-path | pending (X01–X03 batch) | ASCII paths unchanged (same bytes, same trailing backslash) | build clean; manual scenario W-new (Ctrl+Enter on a 130-accented-char name must not terminate the process) |
| X02 | F-P6-02 (052 regression) | `src/dialogs5.cpp:495` `GetDlgItemText` → `SalGetDlgItemTextU8` | Plugins Manager "Show in Change Drive menu" label | pending | ASCII/English templates byte-identical (`LoadStrU8 == LoadStr` for ASCII) | cs/sk/hu + de/es manual check |
| X03 | F-P4-06 (jump list) | `src/jumplist.cpp` — `IShellLinkW`, `GetModuleFileNameW`, wide arguments/description/icon, `VT_LPWSTR` title | taskbar jump list (hot paths) | pending | ASCII hot paths produce the same link and title | manual: non-ASCII hot path appears correctly **and opens** |
| X04 | F-MC-01 (rubber-band geometry) | `src/filesmap.cpp:146` — `SalU8ToW` + `GetTextExtentPoint32W`, byte-wise fallback retained | panel rubber-band selection (default config) | pending | ASCII names measure identically | manual: drag-select over an accented name |
| X05 | F-P4-04 (remembered directory) | `src/mainwnd2.cpp` — name enumerated narrow, **data read through the facade** (`GetValue`) instead of `RegEnumValueA` | per-drive remembered directory across restarts | pending | ASCII paths unchanged | manual: set a non-ASCII dir on a drive, restart, return to that drive |

| X06 | F-P1-01, F-P1-02 (temp cleanup) | `src/cache.cpp` — `SalSetFileAttributes`/`SalDeleteFile`/`SalRemoveDirectory`; `GetTempPath` → `GetTempPathW` + `SalWToU8` | disk cache cleanup and temp-dir reuse | pending (X04–X07 batch) | ASCII `%TEMP%` unchanged | manual: non-ASCII `%TEMP%`, view a file, exit — no `SAL*.tmp` left |
| X07 | F-P1-04 (plugin service) | `src/salamdr3.cpp` — `_RemoveTemporaryDir` walks via `SalFindFirstFile`/`SalConvertFindDataW`/`SalDeleteFile`/`SalRemoveDirectory`; `SetCurrentDirectoryW` + ANSI fallback | `RemoveTemporaryDir` (core **and** the plugin service `spl_gen.h:1010` → `zip.cpp:828`, used by regedt/renamer/zip) | pending (X04–X07 batch) | ASCII paths unchanged; plugin contract observably unchanged | manual: plugin temp tree under a non-ASCII `%TEMP%` is removed |

| X08 | F-P5-08 (plugin-local, FR-012) | `src/plugins/zip/dialogs.cpp` ×2 — ANSI `WM_SETTEXT` → the plugin's own `SetDlgItemTextU8` | ZIP overwrite dialogs (both) | pending (X08–X09 batch) | ASCII names unchanged | manual: overwrite prompt for an accented name in a ZIP |
| X09 | F-P5-09 (plugin-local, FR-012) | `src/plugins/filecomp/mainwnd.cpp` ×2 — legacy narrow fallback instead of blanking the title | filecomp window title | pending (X08–X09 batch) | English unchanged (conversion succeeds, W path as before) | manual: compare two files in cs/fr/hu/sk, title must not be empty |

**Partial-fix disclosure (X08) — corrected after review.** My first
explanation was **refuted**: I claimed `SubClassStatic`'s ANSI
`SetWindowLongPtr` made the control an ANSI window. The reviewer verified
empirically on a cs-CZ/CP1250 machine that the static is already
`IsWindowUnicode = 0` *before* the subclass (the dialogs use `DialogBoxParamA`)
and that `SetWindowLongPtrA` does not change it — and that even a deliberately
Unicode control still yields `3F` for U+65E5, because **`TextControlProc`
(`zip/dialogs.cpp:58-86`) handles `WM_PAINT` itself and paints with the ANSI
`GetWindowText`/`DrawText`**, capping the result at the system code page
whatever is stored. So the residual limit is real but the cause is the paint
handler. Refined once more on re-review: group **B-1** needs **both** — the
probe separates the two losses, since on the ANSI control the character is
already `003F` *at storage* (a wide paint would read back `?`), while on a
Unicode control it stores as `65E5` but `GetWindowTextA` still yields `3F`.
Conclusion unchanged; mechanism corrected twice.

### 7.2 Regression reviews

| Batch | Fixes | Verdict | Reviewer's material findings |
|---|---|---|---|
| X01–X03 | memory safety, 052 regression, jump list | **ACCEPTED** (all three) | Independently re-derived the buffer arithmetic (782 = 780 + `' '` + NUL) and confirmed the clamp reads `s[l]`, "the only correct choice"; proved the X02 truncation concern is structurally impossible (`SalWToU8` is all-or-nothing, so a mid-character cut cannot occur); confirmed `IID_PPV_ARGS` resolves to `IID_IShellLinkW` and that freeing after `SetValue`/`Commit` without `PropVariantClear` is *required* for a malloc'd buffer; verified the jump-list premise by reading the receiving `-aj` parser. Caveats: X01 ignored `Set()`'s return (an OOM-only path could insert a bare `""`) — **closed in X01b**; X03 now skips `SetArguments`/`SetDescription` for input that is not valid WTF-8 (latent — no path can produce such a hot path). Three pre-existing `jumplist.cpp` bugs confirmed preserved, not introduced. No plugin-facing surface touched; no per-item path, so no timing evidence required. |
| X04–X07 | filesmap, remembered directory, cache, `RemoveTemporaryDir` | **X04, X05, X06 ACCEPTED · X07 REJECTED → reworked, re-review pending** | X04: buffer adequacy re-derived independently; the `s == NULL` ".." case proven to reach the identical ANSI fallback. X05: the new read is *strictly safer* — `SalRegQueryValueExW8` always NUL-terminates, whereas the old `RegEnumValueA` could `memmove` an unterminated value into `DefaultDir[]`. X06 accepted **with a required companion change**. **X07 REJECTED — a regression this batch introduced**: converting the walk to the strict facade broke `CDiskCache::ClearTEMPIfNeeded` (`cache.cpp:1471`, run at startup), which still fed it a CP_ACP path from an un-converted `GetTempPath` at `cache.cpp:1473`. Pre-fix the whole chain was consistently ANSI and the "delete leftover SAL*.tmp" cleanup **worked** on a Czech/Polish/Hungarian account; post-fix it became a **silent no-op** — textbook DC-09. The reviewer also confirmed X07 cannot delete anything it should not ("its only fault is deleting *less*"), and named the untouched ANSI twin `_RemoveEmptyDirs`. |
| X07b | rework #1 | **X07 ACCEPTED · X06 REJECTED** | `cache.cpp:1473` → `GetTempPathW` + `SalWToU8`; `_RemoveEmptyDirs`/`RemoveEmptyDirs` converted. The reviewer confirmed X07 is no longer the site of the break — but found the rework had **relocated** the regression rather than closed it: nine lines below the fixed producer, `FindFirstFile(tmpDir, …)` (`cache.cpp:1488`) was still ANSI, so the now-UTF-8 path found nothing and the cleanup prompt **stopped appearing at all**. Same regression, different symptom. It also verified `_RemoveEmptyDirs` never deletes a file (its only destructive call is `SalRemoveDirectory`, which still fails on a non-empty directory). |
| X06b | rework #2 | **X04, X05, X06, X07 ALL ACCEPTED** — no regressed surface remains | The reviewer walked all 16 steps of `ClearTEMPIfNeeded` end to end and confirmed the startup cleanup now works on a non-ASCII account name; verified `SalPathAppend` fails wholesale rather than truncating (so it cannot cut a UTF-8 sequence); confirmed the `continue`-in-`do/while` advances per `[stmt.cont]`; proved `SAL_FIND_DOSNAME_U8` (44) covers the 42-byte worst case so the DOS-name guard's meaning is unchanged in both directions; and enumerated all six producers of `tmpName` to show none is an ACP or 8.3 value. **Bonus correction found**: `ContainTmpName`'s collision check was previously *silently blind* for non-ASCII names (the ANSI call returned `INVALID_HANDLE_VALUE`, so no comparison ran at all) — the fix restores a check that never executed. `WM_USER_FOCUSFILE` is now consistent with its six peer senders. ASCII byte-identity at all sites; nothing plugin-facing changed. The whole `cache.cpp` chain converted: `ClearTEMPIfNeeded`'s enumeration (`SalFindFirstFile`/`SalFindNextFile`/`SalConvertFindDataW`), `ContainTmpName`'s existence check and its name/DOS-name comparisons, and the two `RemoveDirectory(newDirPath)` error paths. No ANSI file API remains in the file outside a comment block. |

**Process note.** Two consecutive rejections on the same fix, both for the
same shape: converting one link of a call chain to the strict facade while an
adjacent link still produced or consumed legacy bytes — **DC-09, the very
class this review catalogued, reproduced by its own remediation.** Neither
would have been caught by the build, the unit tests or the static guard: the
first left the operation a silent no-op, the second removed a prompt. They
were caught only because a reviewer that did not write the fix was required
to trace the data path end to end. This is the strongest evidence in the
feature for why the separated-roles protocol (research R5) is not ceremony —
and it is why the group-B deferrals are the right call rather than caution.
| X08–X09 | ZIP overwrite dialogs, filecomp title | **X09 ACCEPTED (record incomplete) · X08 REJECTED → reworked** | **X08 REJECTED**: `SetDlgItemTextU8` is all-or-nothing and the fix ignored its result, so a ZIP entry whose name carries an unpaired surrogate (feature 066 ships support for exactly those) would show `Overwrite file:` followed by **nothing** — on a *destructive* confirmation, where the previous mojibake at least kept the drive/dir/extension readable. The same defect class as F-P5-09, introduced by the batch fixing it. **X09 ACCEPTED** but the record was incomplete: a third byte-identical `: L""` site at `filecomp/mainwnd.cpp:893` was missed, blanking the same window's title in **5 of 8** languages — including **German**, which was not in the recorded manual check. |
| X08b/X09b | rework | **BOTH ACCEPTED** | The reviewer verified the rework **empirically** with a purpose-built probe (a verbatim copy of the helper against a `DialogBoxIndirectParamA` dialog subclassed like the real one, ACP 1250): for a lone-surrogate name the fallback writes bytes **byte-identical** to the pre-fix `WM_SETTEXT` (old mojibake restored, not blank), and the ASCII success path is unchanged. It also checked a hazard the *rework* could have introduced — `SetDlgItemTextU8` returns `SetDlgItemTextW`'s result, not a "did I convert" flag, so a falsy return on a subclassed static would have made the guard fire every time and silently revert the fix; measured **1** in both cases, so the success path is unaffected. X09's third site confirmed to cover German (string 1061 is non-ASCII in `de` and this was its only use), and the reviewer re-swept every `SplU8ToW*` in the plugin rather than trusting my `rg`. |

Gates after X01–X07: `build.cmd` clean, `saltests` **1257 checks / 0 failed**,
`check_encoding.py --strict` **TOTAL 0**.

## 8. Deferred items

### 8.1 Ledger re-dispositions (L01–L89)

<!-- BEGIN GENERATED: ledger -->
| L-row | Perspectives | Proposed disposition (verbatim from the perspective) |
|---|---|---|
| L01 | P1 | `src/fileswn8.cpp:125` `SetCurrentDirectory(GetPath())` is a documented optimisation ("for faster operation"); the recycle list is built from full paths and the W shell call at :156 does the real work… · **latent** (optionally a low-priority consistency fix: 15 DC-18 twins of the :2357 pattern) |
| L02 | P1 | `src/drivelst.cpp:1481` is not merely "an ANSI call outside the chain": it **down-converts a wide OneDrive path to CP_ACP** (`ConvertU2A` defaults to `CP_ACP`) and stores it in the global `OneDrivePat… · **fix-candidate**, escalated (F-P1-09) |
| L03 | P1 | Still open and broader than recorded: the ANSI `GetVolumeInformation` at `src/salamdr2.cpp:1440` runs on the **full** resolved path (not the root), so mounted volumes under non-ASCII paths report the … · **fix-candidate** (F-P1-12); overlaps L05 as noted |
| L04 | P1 | Confirmed and characterised: `MyQueryDosDevice` (`src/salamdr2.cpp:1781`) returns a CP_ACP target that `ResolveSubsts` splices in front of a UTF-8 remainder, producing a path in neither encoding. Reac… · **still-open → fix-candidate** (F-P1-13) |
| L05 | P1 | Confirmed at all three recorded sites plus `SHGetFileInfo(SHGFI_DISPLAYNAME)` (`src/drivelst.cpp:1115`) and `WNetGetConnection` (:1769). The consumer side already assumes UTF-8 (`src/dialogs3.cpp:1325… · **fix-candidate** (F-P1-14) |
| L06 | P2 | `src/mainwnd3.cpp:5320-5325` unchanged: ANSI `TTN_NEEDTEXT` writing `PointToLocalDecimalSeparator` output into `szText`, tool registered ANSI at `:5160-5168`. Only the decimal separator can be non-ASC… · **latent** (unchanged); becomes a defect only if the locale sweep ever widens |
| L07 | P3 | `fileswn8.cpp:110` `oneFile->Name[oneFile->NameLen - 1] <= ' '` (and `:62`). `Name` is `char*`, and the product compiles with `/J` (`src/vcxproj/sal_base.props:14`), asserted at runtime in `salamdr1.c… · **closed-by-/J** (never a defect; the ledger note is wrong). Recommend converting the reasoning into a guard exemption + a saltests assertion that `/J` is in effect, so the row cannot silently reopen … |
| L08 | P1 | Still open, unchanged by 058: `SalU8ToW`'s single failure value conflates "too small" with "invalid", the CP_ACP fallback's result is unchecked, and the buffer is used regardless — at all three sites. · **still-open → fix-candidate** (F-P1-16); the converter-level part (DC-20) belongs to P3 |
| L08 | P3 | Still open, but the wording is stale. `SalU8ToW` does conflate "invalid" with "too small" (F-P3-01) — however the destination is **not** indeterminate: every failure path writes `buf[0] = 0` (`salunic… · **fix-candidate** (F-P3-01): make the capacity failure distinguishable; `SalWtf8ToWBytes` already returns `-2` for it. |
| L09 | P1 | Still open and still latent in shipped configurations: `MB_PRECOMPOSED` breaks the fallback only when the system ACP is UTF-8 (65001), which no shipped configuration sets. · **latent** |
| L10 | P1 | Confirmed: `src/geticon.cpp:364-367` does not check `MultiByteToWideChar`'s result, so `wszPathBuf` (an uninitialised `WCHAR[MAX_PATH]` local) can reach `ParseDisplayName`. A two-line guard fixes it. · **still-open → fix-candidate** (F-P1-18) |
| L11 | P1 | `WCHAR wszPathBuf[MAX_PATH]` still truncates long paths, but the strict arm allocates (`SalU8ToWAlloc`) and only the fallback is bounded — so a long **valid** UTF-8 path is fine and only a long **inva… · **by-design** (subsumed by the F-P1-18 guard) |
| L12 | P2 | Premise valid but mis-stated; the ACP conversion happens in `src/shares.cpp:105-107`, and the same conversion also hits `shi502_path` (operational) · **still-open** — rewrite the suppression comment; hand the `shi502_path` half to P1/P3 (DC-02) |
| L13 | P2 | Premise **false**: `ConfigurationName` is a path and is UTF-8 on the `-C` branch · **fix-candidate** → F-P2-13 |
| L14 | P2 | Premise **still true** at this site (raw `FindFirstFileA`), for a reason the comment states correctly; but the site is a DC-01 candidate and is inconsistent with `:907` in the same loop · **still-open** — keep the suppression, add the DC-01 cross-reference; P1 owns the enumeration |
| L15 | P2 | Verified by design; wide `CreateExW` above, ANSI branch now unreachable for panel names after 066 · **by-design** (annotate as effectively dead) |
| L16 | P2 | Asymmetry confirmed (checkbox/hint ANSI, body/title/URL/buttons U8); every shipped producer is ANSI, and the hint body is normalized by `SetToolTipText` · **still-open** as a documentation item (`spl_gen.h` must state the encoding); not a shipped defect |
| L17 | P4 | The class is real and still unswept. `packers.cpp:734` is confirmed (F-P4-03) but is *tolerated* by its sinks; the sweep found three more members of the same class, one of which is a hard, user-visibl… · **fix-candidate** — F-P4-01/02 first (real defect), then the invariant work: document the four fields as UTF-8, `LoadStrU8` at the seeds, `SalLegacyToU8Alloc` at the `convert.cfg` intake |
| L19 | P2 | `WM_NOTIFYFORMAT`/`NF_REQUERY` is still handled per dialog (`src/finddlg1.cpp:3016,3981-4003`, `src/packac.cpp:70-73,184-187`), not centrally in `CDialog::CDialogProc` · **still-open by design** — only two dialogs need it today; centralizing would change ~100 dialogs' notification format |
| L21 | P3 | A concrete flow now exists: `finddlg1.cpp:1290-1341` sizes the Find date/time columns by calling **`ListView_GetStringWidth` (ANSI)** on `SalGetDateFormatU8`/`SalGetTimeFormatU8` output and probes it … · **still-open**, now **automatable**: a saltests check that feeds a synthetic non-ASCII `LOCALE_SSHORTDATE`/long-date through `SalGetDateFormatU8` into the two consumers (`finddlg1.cpp` column sizing, … |
| L22 | P2 | The size/archive number fields were converted by 067 (`dialogs2.cpp:414,454,477,480`, `dialogs3.cpp:1540-1549,2219` all `PrintDiskSize(…, TRUE)` + `SalSetDlgItemTextU8`); no core mode-1/2 caller with … · **closed-by-067** — but note the adjacent *type line* in the same dialog is still defective (F-P2-07) |
| L23 | P3 | Confirmed unchanged: `viewer2.cpp:1059-1062` explicitly leaves UTF-16 content to the hex path ("leave UTF-16 to the existing path (shown as hex)"). Only `VCE_UTF8` gets the wide draw (`viewer.cpp:781-… · **still-open** (by design for this feature; a separate viewer feature). |
| L24 | P3 | Confirmed unchanged: the viewer's cell model is one cell per **UTF-16 unit** (`Utf8SegCells`, `viewer.cpp:771-778`), so caret/selection on a multi-byte line is exact per code unit but not per grapheme… · **still-open** (cosmetic). |
| L25 | P3 | Confirmed unchanged: `Utf8SegCells` returns UTF-16 unit count, so a CJK glyph still occupies one fixed cell and overlaps its neighbour. · **still-open** (cosmetic; same viewer feature as L23/L24). |
| L26 | P6 | The row conflates two things and only one is still true. **Round-trip**: the Rename dialog is `CCopyMoveDialog`, one of the four Unicode windows (`src/fileswn5.cpp:2386-2388` → `src/dialogs3.cpp:394`)… · `closed-by-066` for the round-trip half; `by-design` for the "typing" half (unreachable input, not a limitation anyone can hit) |
| L27 | P4 | "External text channels — lossy rendering of unrepresentable units acceptable." Still correct for the channels it was written about: `TRACE_*` into the Trace Server (S-B5-P4-008) and the bug report. I… · **by-design (keep), with a scope clarification** |
| L28 | P3 | Verified against the code: `SalWtf8ToWBytes` (`salunicode.cpp:174-188`) accepts a **pair** of 3-byte surrogate sequences and folds it into one supplementary code point, so `SalWToU8(SalU8ToW(cesu8)) !… · **by-design** (unchanged); worth one sentence in `name-encoding-wtf8.md` stating that CESU-8 input is *accepted* (not rejected) by `SalU8ToW`, since a "is this valid UTF-8?" probe therefore also accep… |
| L29 | P5 | `ftp/ctrlcon2.cpp:1574,1617` still pass `d->Text.Length` (bytes) to `EM_SETSEL`. The log edit is an ANSI control fed raw UTF-8 bytes, so byte offsets and control offsets agree today; the row is only a… · by-design (re-open if the log window goes wide) |
| L30 | P2 | `src/filesbx2.cpp:230,270` still ANSI `TextOut` on `column->Name`; core names are `LoadStr`, plugin names arrive from the plugin's own `LoadStringA`; `spl_com.h:476` defines no encoding · **still-open by design** (consistent-ANSI today) + **new**: record the undefined plugin-side encoding as a DC-17 item for P5 |
| L31 | P2 | `src/viewer3.cpp` converted to `TOOLINFOW`/`TTM_ADDTOOLW`/`TTN_NEEDTEXTW` by 067 (contract §3); `src/mainwnd3.cpp:5077/5324` is L06 · **closed-by-067** for `viewer3`; the `mainwnd3` half merges into L06 |
| L32 | P4 | `src/mainwnd4.cpp:243` `char fileName[MAX_PATH]` in `CMainWindow::MakeFileList` — unchanged, still not long-path capable, while everything around it is (`SalGetTempFileName`, `SalCreateFile`, `SalDele… · **still-open** (non-encoding, low priority) |
| L33 | P1 | `src/shiconov.cpp:261-269` (`InitGoogleDrivePath` gating) is unreachable 2015-era Google-Drive machinery; its `WideCharToMultiByte(CP_ACP,…)` at :263 never runs. Nothing has changed since the row was … · **still-open, out-of-scope** (dead code; delete when the Google-Drive detection is reworked) |
| L34 | P1 | Explorer's *Status* column uses a different mechanism (property-store driven); 059 added the `PKEY_StorageProviderState` fallback for the **overlay**, not a column. No change. · **by-design** |
| L35 | P5 | `zip.cpp:5154` `ResolveLocalPathWithReparsePoints` unchanged; it is one instance of a wider pattern — the plugin-exported `MAX_PATH` buffer contracts are byte limits on UTF-8 (`zip.cpp:5868,5966`, `sp… · still-open (widen to the S6/Note N-5 statement) |
| L37 | P1 | `MAX_PATH` for components / 8.3 names / roots / `DefaultDir[26][MAX_PATH]` is still deliberate; my sweep found no site where a component legitimately exceeds `MAX_PATH`. Note that WTF-8 makes a compon… · **by-design** |
| L38 | P5 | Confirmed verbatim at `ftp/dialogs6.cpp:377-381`; `PrintDiskSize(…,1)` ×3 into an ANSI `LoadStr` template into `SetActionShowHint` (`gui.cpp:1340`, ANSI). · still-open → F-P5-07 |
| L39 | P5 | Confirmed at `ftp/fs4.cpp:325-327` (`NumberToStr` ×2 + ANSI `ExpandPluralString` template) and `ftp/operats1.cpp:1207,1210`. · still-open → F-P5-07 |
| L40 | P5 | Confirmed at `ftp/operatsb.cpp:1140` (`PrintDiskSize(num, …, 2)` into `LoadStr(IDS_LOGMSGUPLOAD…)` → `Logs.LogMessage`). · still-open → F-P5-07 |
| L41 | P5 | Confirmed at `dbviewer/dialogs.cpp:510` (`ListView_SetItemText`) and `dbviewer/parser.cpp:302,345,349,1033` (`sprintf` + `EM_REPLACESEL`). This is still the sharpest evidence of the frozen-API cost. · still-open → F-P5-07 |
| L42 | P5 | Confirmed at `regedt/finddlg.cpp:410-412`: `SG->NumberToStr` (UTF-8) → `StrToWStr` = `MultiByteToWideChar(CP_ACP,…)` (`regedt/utils.h:33`). Unconditional, no fallback. · still-open → F-P5-07 (and the cheapest of the family to fix locally) |
| L43 | P5 | Confirmed at `zip/dialogs.cpp:1839-1840, 1925-1926`; the plugin's own `SetDlgItemTextU8` (`zip/common.cpp:337`) is used at 10 other sites in the same file. · fix-candidate (FR-012: local, 4 lines) → F-P5-08 |
| L44 | P5 | Confirmed at `filecomp/mainwnd.cpp:2043-2046, 2135-2137`. Worse than recorded: the `L""` fallback is not only "ANSI-Czech title blank" — contract B2/B3-C2 forbids dropping text, the legacy `SetWindowT… · fix-candidate (FR-012: local, 6 lines) → F-P5-09 |
| L45 | P5 | Confirmed verbatim at `regedt/fs4.cpp:479`: `SalPrintf(buffer, 1000, formatA, selectedSize)` with a `CQuadWord` in varargs. Not an encoding defect; UB. · still-open (FR-015 candidate; not P5's class) |
| L46 | P5 | Confirmed: `regedt/finddlg2.cpp:932` is the only display-info handler, the list is `LVS_OWNERDATA` (`regedt/lang/lang.rc:170`), the dialog is ANSI (`winliblt.cpp:445,452`), and `NF_REQUERY` appears no… · fix-candidate **after runtime confirmation** → F-P5-10 |
| L47 | P5 | Re-examined against the enabled set: 067 confirmed 5 of 8. The number-formatting sites still unverified at runtime in enabled plugins are `checksum`, `filecomp`, `pictview`, `renamer`, `undelete`; the… · still-open (runtime verification, folds into the F-P5-07 fix batch) |
| L48 | P5 | Confirmed unchanged: `splunicode.h:29,43,59,70,94` still strict; 228 call sites in the 19 enabled plugins (303 across all 30); every failure is fail-closed (NULL / `L""`), so a WTF-8 relaxation cannot… · fix-candidate (contract B10.5 must be amended in the same change) → F-P5-04 |
| L49 | P5 | Not re-run (an audit, not a site). Two concrete data points found: `zip.cpp:5868/5966` cap plugin listing at `MAX_PATH-5` **bytes**, and sftp never uses the extended-length prefix (F-P5-11) while ftp … · still-open (narrowed: the audit now has two named starting points) |
| L50 | P5 | Re-examined per plugin. `uniso`, `tar`, `folders` have **no** ANSI UI text sinks at all (nothing to wrap). `checksum`'s remaining `SetDlgItemText`/`SetWindowText` carry `LoadStr` text only (`dialogs.c… · partly closed; residue = `uncab` → F-P5-03 |
| L51 | P2 | Re-checked the data: `translations/french/salamand.slt` id 12820 is still `{!}%s octets{s\ · 0\ · \ · 1\ · s}` — base word already plural, default suffix `s` ⇒ "octetss" for every count ≠ 0 · **still-open** (data-only fix; FR-015 candidate). Same table shows cs `bajt{ů…}`, sk `bajt{ov…}`, hu `bájt{…}` are correct |
| L52 | P2 | Confirmed the mechanism: `IDS_PLURAL_X_BYTES` is non-ASCII only in cs/hu among enabled languages, and only `PrintDiskSize` modes 1/2 with `u8=FALSE` compose it with `NumberToStr`. Core has **no** such… · **latent** — keep as a re-enable checklist note; add "also re-check every enabled plugin's `PrintDiskSize` mode-1/2 use" |
| L59 | P5 | Confirmed at `plugins1.cpp:2184`: `sprintf` of `Name` (UTF-8, up to MAX_PATH) + full DLL path + `GetErrorText` into `char bufText[MAX_PATH + 200]` (`:2154`). · still-open (not encoding; Note N-6) |
| L60 | P5 | Confirmed at `plugins1.cpp:2166-2169`: `GetModuleFileName(HInstance, buf, MAX_PATH)` then `strcpy(s, "plugins\\")` + `strcat(s, DLLName)` with no bound. Also the ANSI `GetModuleFileName` is P1's seed … · still-open (Note N-6; coordinate with P1) |
| L61 | P5 | Site present (`sftp/dialogs.cpp:1125-1140`, the label-widening loop). Purely geometric, no encoding content; cannot be judged without running the dialog. · still-open (not P5's class) |
| L62 | P5 | `sftp/session.cpp:503` — host-key trust TOCTOU on retry. Not an encoding item; not re-derived. · still-open (out of this feature's scope) |
| L63 | P5 | `sftp/session.cpp:718` — password-path auth failures unclassified. Not an encoding item. · still-open (out of scope) |
| L64 | P5 | Confirmed at `sftp/dialogs.cpp:41-54`: `GetDlgItemTextU8` frees the temporary WCHAR copy (`free(w)`, `:54`) without zeroizing, and the helper serves credential fields. The encoding helper is the vehic… · fix-candidate (one `SecureZeroMemory` before `free`) → Note N-7 |
| L65 | P5 | `sftp/lang/lang.rc2:128` duplicate accelerators — translation/resource item, not encoding. · still-open (translation queue) |
| L66 | P1 | `src/shiconov.cpp:856-868` — the SEH handler still leaks one `IPropertyStore` on the exception path. Unchanged by 059 (which added the property-store fallback in `GetIconOverlayIndex`, a different fun… · **still-open** (low) |
| L67 | P1 | Overlay worst-case scanning is unchanged; 059 added one extra `CfGetSyncRootInfoByPath` call on the "every handler declined" path, which is cached per panel path. No new cost found. · **by-design** |
| L69 | P1 | `src/shiconov.cpp:1192` `NOHANDLES(LoadLibrary("cldapi.dll"))` — still a relative load. `cldapi.dll` is a system DLL present in `System32`, and the process is not marked with a custom DLL search path,… · **still-open → fix-candidate** (one-line `LOAD_LIBRARY_SEARCH_SYSTEM32`); non-encoding |
| L70 | P1 | Theoretical overlay index staleness — no new evidence either way; the index is rebuilt on `WM_SETTINGCHANGE`. · **by-design** |
| L71 | P1 | `src/shexreg.h:218` IPC struct is a fixed shell-extension ABI shared with the 32-bit shell extension; changing it breaks the mixed-bitness contract. Unchanged. · **by-design** |
| L72 | P1 | Long-path truncation without a crash in Compare Directories / Shift+F4 / archive backups / clipboard paste / hot-path save / window title. My sweep adds one datum: Compare Directories has a *different… · **still-open** (unchanged priority) |
| L73 | P1 | Archive-subsystem path buffers are still bounded by `MAX_PATH`; the external-packer subsystem (`pack1/2/3`) explicitly refuses longer paths (`src/salamdr3.cpp:255` `"Too long base path in SalGetTempFi… · **by-design** |
| L74 | P1 | External `MAX_PATH` limits at the shell/launch/MAPI/common-dialog boundary: still true; the common-dialog sites additionally have an *encoding* defect (F-P1-24) that is independent of the length limit… · **by-design** (length) + **fix-candidate** (encoding, F-P1-24) |
| L77 | P4 | Hot-path configuration. Both halves still present and both are deliberate: `CHotPathItems::Save` writes the *effective label* into the `Name` value "so older builds read this configuration without any… · **by-design** for the two named behaviours; the buffer asymmetry is a new **still-open** note |
| L78 | P4 | `src/jumplist.cpp` — "no gallery icon" unchanged (`SetIconLocation("shell32.dll", -319)` at `:174`). But the file is **not** otherwise clean: it is built entirely on `IShellLinkA` and a `VT_LPSTR` `PK… · **still-open** (icon), and the file gains **fix-candidate** F-P4-06 |
| L79 | P2 | The 84 sites converted by 042 were never individually verified per language. This review verified the *mechanism* (M2) and checked the actual translated strings for the ids it touched; it did **not** … · **still-open** — but F-P2-01…F-P2-07 show the residual `LoadStr` siblings of exactly that sweep are defective, so the sweep was incomplete rather than wrong |
| L80 | P2 | Same for the ~25 window-text/number sites of 043; the config *language field* at runtime and the drag image were never exercised · **still-open**; `src/dialogs2.cpp:875` (`Web`, ANSI) is one unverified survivor, currently latent |
| L81 | P5 | Plugin dialogs at runtime were verified by diff only in 041/042/043. This review adds three concrete runtime-verification targets: regedt Find results (F-P5-10), filecomp window title (F-P5-09), zip o… · still-open (narrowed to a 3-item runtime checklist) |
| L82 | P3 | Not verified by execution (read-only review), but two machinery items are ACP-shaped and would behave differently on a non-Central-European ACP: (a) the DC-15 tables (F-P3-06) — the *specific* collisi… · **still-open**, and now **testable without changing Windows settings** for (a): the byte tables can be rebuilt in a test from a chosen code page and the collision property asserted. |
| L83 | P4 | 063 GUI scenarios 1–7 still unverified by this review (it is read-only, no runtime). Two of the seven now have a concrete pre-condition worth adding to the quickstart before they are run: scenario "cl… · **still-open** — carry forward, with the two additions above |
| L84 | P6 | Still unrun; I could not execute it (read-only, no builds). What I *could* establish statically raises the value of running it rather than lowering it: the 066 delta touches five callers whose behavio… · `still-open` — and it is the delta's largest single evidence gap, since 065/066/067 ship unreleased |
| L85 | P2 | 067's English pre-fix capture was argued by construction; still no capture · **still-open** — low value now: this review re-derived the property (ASCII `LoadStrU8` == `LoadStr`, M1) from the implementation |
| L86 | P1 | Interactive SHIFT+DEL smoke test — still the only way to validate the recycle-bin path end to end. Note that the ANSI helper `DeleteThroughRecycleBinAux` (`src/fileswn8.cpp:22`) is now dead code, so t… · **still-open** (test-only) |
| L87 | P6 | Discharged here: I read every core file of `v0.1.1..v0.1.2` line-level with the encoding lens (rows S-X-P6-030 to S-X-P6-039) and the pass was not empty — it produced **F-P6-02**, a mojibake regressio… · `closed-by-068` (the review debt is paid; F-P6-02 is the finding it was hiding) |
| L88 | P1 | GUI screenshots are still not captured programmatically; every visual claim in this report is code-derived, not screenshot-verified. · **still-open** (test-only) |
| L89 | P5 | The "8 of 18 plugins not runtime-verified" set overlaps L47. Non-ASCII date/time locale: no plugin site found that formats dates itself except `dbviewer/parser.cpp:298,1029` (`GetTimeFormat` **A** int… · still-open (one new site named) |

**77 of 89 ledger rows re-examined** so far (78 dispositions from 6 perspectives).
<!-- END GENERATED: ledger -->

### 8.2 New deferred items

| ID | Origin | Location | Description | Enc | Justification | Recorded where |
|---|---|---|---|---|---|---|
| D02 | X08–X09 regression review (2026-08-24) | `src/plugins/zip/common.cpp:2469` `GetInfo`, rendered at `IDC_FILEATTR` in both ZIP overwrite dialogs | **`IDC_FILEATTR` is not the homogeneous ANSI value the fix record assumed.** `GetInfo` mixes `NumberToStr`'s UTF-8 thousands separator (measured `LOCALE_STHOUSAND = U+00A0` on cs-CZ) with ANSI `GetDateFormat`/`GetTimeFormat`, so on cs-CZ **every file ≥ 1000 bytes** shows a stray `Â` — the feature-067 defect shape, inside a plugin, and *more* frequently visible than the case X08 was reworked for. | yes | **Reason corrected on re-review**: my first justification (the helper would reintroduce blanking) is obsolete — the X08 rework established the idiom that would apply verbatim. The real blocker is **FR-012(2): the fix is not plugin-local.** `GetInfo` has five callers and its output crosses back into the core — `zip/add.cpp:1236` feeds it to `CSalamanderGeneral::DialogOverwrite` → the core's `COverwriteDlg` (`src/zip.cpp:664,679`), whose bytes are frozen by FR-009 (group **B-5**). | this report; plugin follow-up list |
| D03 | X08–X09 regression review (2026-08-24) | `src/plugins/filecomp/controls.cpp:88-91` | The path bar drops its text entirely on strict-conversion failure — the same "drop the text" shape as F-P5-09, at a site the finding did not name. | yes | Deferred on **process** grounds, not technical ones: the reviewer confirms it meets all three FR-012 conditions and is ~4 lines. Kept out so this feature's plugin changes stay confined to the sites its findings actually name. **Trigger + sketch**: same `if (w != NULL) { …W; free } else …A` idiom as X09. | this report; plugin follow-up list |
| D05 | `/speckit-git-commit` invocation, 2026-08-24 | `.specify/extensions/git/scripts/powershell/auto-commit.ps1:149` | **The Spec Kit git auto-commit hook cannot run on a CP1250/CP1252 machine.** The script is UTF-8 **without a BOM**, so Windows PowerShell 5.1 reads it as ANSI; the `✓` on line 149 (`E2 9C 93`) becomes three CP1250 characters, the last of which (`0x93`) maps to `"` U+201C — **which PowerShell accepts as a string delimiter**. The result is an unterminated string and a whole-file `ParserError`, so the hook fails before doing anything. Same defect class as the product findings (UTF-8 bytes read through the legacy code page), in the tooling. | yes | Developer-side tooling, explicitly outside the review by spec clarification Q1 — recorded, not fixed, to keep this feature's diff confined to what its findings cover. **One-line fix**: replace the `✓` with ASCII, or save the file as UTF-8 **with** BOM. | this report; tooling follow-up |
| D04 | X08–X09 re-review (2026-08-24) | `src/plugins/filecomp/worker2.cpp:85-100` | A **fourth** filecomp title site, invisible to a `: L""` search because it never converts: it composes the same ANSI `IDS_MAINWNDHEADER` + UTF-8 names straight through the ANSI `SetWindowText`. Not a blanking defect and not a regression — but **ordering matters**: on the `WN_BINARY_FILES_DIFFER` path the fixed site sets the caption and the worker then **overwrites** it here, so the binary-compare path still shows the unfixed caption. | yes | Same class, outside the finding's named sites; deferred with D03. **Affects the manual check**: verify X09 on a *text* comparison — a binary comparison will look unfixed. | this report; plugin follow-up list; quickstart W-note |
| D01 | T004 (this review, 2026-08-24) | `src/common/handles.h:541,546,618` + `src/common/handles.cpp:1381,1394,2237`; consumer `src/vcxproj/tserver/tserver.vcxproj` (`CharacterSet=Unicode`, Win32 only) | **The Trace Server cannot be built.** `handles.h` declares generic wrappers `C__Handles::RegCreateKeyEx/RegOpenKeyEx/FindFirstChangeNotification(LPCTSTR…)` *and* explicit `…W` overloads (registry W work; `FindFirstChangeNotificationW` from feature 058). In a `UNICODE` consumer windows.h's macros rename the generic ones to `…W` → C2535/C2084 duplicate definitions. The core (non-UNICODE) is unaffected; `build.cmd` never builds `tserver`. | no (build) | Developer tooling is outside the review by spec Q1; a fix touches the core's debug shim header (not a one-line FR-015 change). Consequence for this review: G5 runs at the feature-060 observable bar (exit 0, no handle/heap dialogs, no crash report) — the Trace Server capture is **waived** with this reason. | this report; `tserver` note for a future tooling feature |

## 9. Gates

| Gate | Check | Result | Evidence |
|---|---|---|---|
| G1 | `build.cmd full` (Debug x64) | **PASS** | exit 0; 19 plugins, 180 language modules; **no warnings in any file this feature changed** (compared against the 140-line baseline capture) |
| G2 | `build.cmd full release` (Release x64) | **PASS** | exit 0; all 11 changed files verified recompiled in that run (not a stale success) and `tandemcommander.exe` freshly linked; no warnings in changed files |
| G3 | `saltests.exe` ≥ 1229/0 | **PASS** | **1257 checks, 0 failed** (baseline 1229 + 6 scaffold + 22 property checks from P7's design) |
| G4 | `check_encoding.py --strict` = 0 + fail-before proofs | **PASS** | `TOTAL: 0 finding(s)`, exit 0, with **9 rules** enforced (6 pre-existing + 3 promoted). Each promoted rule proven to fire on a planted instance of its shape → flagged, exit 1. Two defects in the promotion itself found and fixed (see §11) |
| G5 | start/exit health | **PASS** | Debug binary started, main window created, alive ≥ 12 s, closed via `CloseMainWindow()` (WM_CLOSE), **exit code 0**; no new `*.TXT` crash report under `%LOCALAPPDATA%\Tandem Commander`; no handle-leak or `_CrtCheckMemory` dialog (either would have blocked the graceful exit). Trace Server capture **waived** — `tserver` does not build (D01) |
| G6 | timing (per-item-path fixes only) | **N/A — none required** | No accepted fix is on a per-item path. `filesmap.cpp` (X04) runs **once per rubber-band drag**, not per item; the others are startup, dialog-init or one-shot paths. Verified against the FR-008 trigger list rather than assumed |
| G7 | English spot-check | **partial — automated part PASS** | Plugin-facing surface unchanged **by construction**: measured against this feature's baseline `c577ff3`, `git diff -- src/plugins/shared/` is empty and so is `git diff -- src/zip.cpp src/plugins1.cpp src/plugins2.cpp src/plugins3.cpp src/packers.cpp` — no SDK header and no `CSalamanderGeneral` forwarder was touched. `LAST_VERSION_OF_SALAMANDER` = 106 unchanged. (The 11-line `spl_gen.h` delta against tag `v0.1.4` predates this feature — it is feature 067's documentation.) The on-screen English comparison against `Release_x64_prefix` is part of the user's manual pass |

## 10. Sweep — the manual pass (for the maintainer)

Everything automatable is already green (§9). What remains needs a human at
the screen. Binaries are built and waiting:

- **fixed**: `build	andemcommander\Debug_x64	andemcommander.exe` (and `Release_x64\`)
- **pre-fix reference** for side-by-side: `build	andemcommander\Release_x64_prefix	andemcommander.exe`
- fixtures: `D:\Zkouška\Můj disk\`, `D:\Zkouška\Árvíztűrő tükörfúrógép\`,
  `D:\Zkouška\surrogate\`, `%TEMP%\salamander-test\perf` (100 000 files)
- UI language: Options → Configuration → Language (restart required)

### 10.1 Fix verification — the nine accepted fixes

Ordered by consequence. **F1–F3 are the ones worth doing first.**

| # | Fix | Steps | Expected |
|---|---|---|---|
| F1 | X01 crash | In `D:\Zkouška\Můj disk\` focus `žluťoučký kůň.docx`, press **Ctrl+Enter**; then **Ctrl+Space**, **Ctrl+[**, **Ctrl+]**. Repeat in a folder with a very long path. | Name/path appears on the command line; **the program does not disappear**. On the pre-fix binary a ~130-accented-character name terminates it outright. |
| F2 | X06/X07 temp cleanup | With `%TEMP%` under an accented profile: F3-view a file inside a ZIP, exit, look in `%TEMP%` for `SAL*.tmp`. Restart the app. | No `SAL*.tmp` left behind; if any exist from before, the startup prompt **appears** and deleting them works. |
| F3 | X03 jump list | Add a hot path named `Můj disk` → `D:\Zkouška\Můj disk`. Right-click the taskbar icon. | Entry reads `Můj disk` (not mojibake) **and clicking it opens that folder**. |
| F4 | X05 remembered dir | On drive `D:` enter `D:\Zkouška\Můj disk`, switch to another drive, exit, restart, return to `D:`. | Panel returns to `…\Můj disk`, not to `D:\`. |
| F5 | X02 Plugins Manager | Czech (also sk/hu/de/es): Plugins → Plugins Manager, select **UnDelete** or **Portables**, read the "Show in Change Drive menu" checkbox. | Label and plugin name both readable; no mojibake. |
| F6 | X04 rubber band | Detailed view, *Full row select* **off**: drag a selection rectangle past `žluťoučký kůň.docx`. | Only files the rectangle actually touches get selected. |
| F7 | X08 ZIP | Extract onto an existing accented file inside a ZIP to trigger the overwrite prompt. | The file name is readable in the prompt. |
| F8 | X09 filecomp | Czech/French/Hungarian/Slovak: compare two files with accented names. **Use a *text* comparison** — see the note below. | Window title is present and readable (it was empty). |

> **F8 caveat (deferred item D04).** A fourth title site in filecomp is still
> unconverted and **overwrites the caption on the binary-compare path**. A
> binary comparison will therefore still look unfixed — that is expected and
> recorded, not a failed fix.

### 10.2 Regression sweep — surfaces that must be **unchanged**

W1–W20 from [quickstart.md](quickstart.md), in **Czech** and **Hungarian**,
plus an **English** spot-check of W1–W6/W13 against `Release_x64_prefix`.
Highest value, given what this feature touched: W1 panel names and size
column · W4 Ctrl+F1 · W9 Make File List · W12 rename + viewer · W14 Del to
Recycle Bin in `D:\Zkouška\Můj disk\` · W16 operations on the surrogate
names · W17 exit/restart with both panels on accented paths.

| W | Surface | cs | hu | en |
|---|---|---|---|---|
| W1 | Panel names, size column, tiles | | | |
| W2 | Information line | | | |
| W3 | Directory line / free space | | | |
| W4 | Drive Information (Ctrl+F1) | | | |
| W5 | Directory sizes; archive size | | | |
| W6 | Message boxes; Ctrl+C in a box | | | |
| W7 | Find results + status bar | | | |
| W8 | Plugins Manager names | | | |
| W9 | Make File List (clipboard/viewer/file) | | | |
| W10 | Copy name / path / UNC | | | |
| W11 | Tooltips and hints | | | |
| W12 | Rename field + internal viewer | | | |
| W13 | Alt+F1 drive menu | | | |
| W14 | Recycle Bin on non-ASCII path | | | |
| W15 | Icons, overlays, auto-refresh (cloud) | | | |
| W16 | Surrogate-name operations | | | |
| W17 | Saved-configuration round trip | | | |
| W18 | Language selection + rename caption | | | |
| W19 | Hot path names | | | |
| W20 | Plugin surfaces at the boundary | | | |

Anything that fails re-enters the fix loop as a new finding (T036): verify →
minimal fix → independent regression review → gates.

## 11. Durable guards

**Tests applied (T009 + P7 design)** — `src/saltests/saltests.cpp`,
`TestEncodingReview068()`: the T009 scaffold (6 checks) plus P7's blocks (5)–(12)
(22 checks). Suite **1229 → 1257 checks, 0 failed**. These pin the properties every
finding's evidence chain rests on, so a later change to the converter machinery
breaks a test instead of silently invalidating the audit:

| Block | Pins | Serves |
|---|---|---|
| (5) | `srcLen == 0` reported as failure, both sides still 0-terminate | F-P3-02 |
| (6) | capacity failure needs room for the terminator (exact-fit fails) | F-P3-09 |
| (7) | the facade rejects an ANSI-producer path with `ERROR_INVALID_NAME` | F-P3-03 |
| (8) | `SalLegacyToU8Alloc` repairs an ANSI path so the facade accepts it | F-P1-08/10 |
| (9) | the legacy probe is **all-or-nothing** — one legacy byte re-encodes the whole buffer | F-P2-01, F-P6-02 |
| (10) | a byte clamp cuts a sequence and costs the whole string; the character-safe way | F-P3-07 |
| (11) | on failure the converter **0-terminates** — a careless caller gets an empty string, never indeterminate stack | F-P1-16/18 (evidence **against** part of the claim) |
| (12) | WTF-8 stays byte-identical to UTF-8; display keeps the true unit | 066 contract |

Block (13) (a fail-before test for a distinguishable "buffer too small" result)
is **not applied**: its finding F-P3-01 was REFUTED, so the fix it belonged to
does not happen.

**Rules promoted to the strict build gate** (T028). Each was classified hit by
hit by P7, annotated where a hit is legitimate, and — critically — **proven to
fire**: a planted instance of its shape in a scanned file makes
`check_encoding.py --strict` report it and exit 1.

| Rule | Hits in tree | Disposition | Fail-before proof | Strict now |
|---|---|---|---|---|
| `strict-probe-rejects-wtf8` | 0 | pure forward guard: makes the feature-066 WTF-8 invariant unbreakable in core | planted `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fileName, …)` → **flagged, exit 1** | yes |
| `lossy-lenient-at-intake` | 2 | both are the documented last-resort fail-safes in `SalConvertFindDataW`; annotated in place | planted `WideCharToMultiByte(CP_UTF8, 0, …, fileName, …)` → **flagged, exit 1** | yes |
| `ansi-tooltip-handler` | 1 | the deferred ledger row L06 (latent — all shipped decimal separators are ASCII); annotated in place | planted ANSI `TTN_NEEDTEXT` + `LoadStrU8` → **flagged, exit 1** | yes |

Two defects found in the promotion itself, both fixed:

1. **The rules were listed but never executed.** Their implementations live in
   `scan_draft()`, so adding them to `RULES` made `--strict` print
   "0 finding(s)" for checks that never ran — the **`dead-dispinfow` shape, in
   the guard itself**. `main()` now merges `PROMOTED_FROM_DRAFT` into the
   strict run, and the fail-before proofs above exist precisely so this cannot
   recur silently.
2. **The suppression markers did not apply.** They were placed above the `if`,
   but the flagged line is the third line of a multi-line condition and
   `suppressed()` walks only the contiguous comment block *directly* above the
   offending line. Moved inside the condition, where they now take effect.

**Still report-only** (`--draft`), each blocked on a fix this feature deferred:
`ansi-api-on-utf8-path` (~160 hits after tuning; the facade migration is group
B-6), `cp-acp-utf8-source` (promotable once F-P1-25/16/18/28 land),
`missed-twin` (promotable with F-P2-01's fix). **`signed-char-name-byte` is
retained but its premise is void** — the product compiles with `/J`, so plain
`char` is unsigned; P7's replacement `acp-byte-table-on-name` belongs with the
group B-2 work.

| DC | Mechanism | Rule / test | Fail-before evidence | Pass-after |
|---|---|---|---|---|

## 12. Perspectives (coverage lists)

*(one subsection per P1–P7 from `findings/P*.md`)*

## 13. Stability verdict

**The automated half of this feature is complete and green. The verdict is
conditional on the manual sweep (§10), which is the maintainer's pass.**

### What was done

| | |
|---|---|
| Sites inventoried | 2,529 candidate lines across 8 boundaries; 1,069 cited individually, 1,439 in explicit groups, 21 gaps resolved by hand (one became a finding) |
| Findings | **76 raised → 60 CONFIRMED**, 8 refuted, 4 latent, 2 by-design, 2 withdrawn by their authors |
| Fixes | **9, all independently accepted** after 3 rejections |
| Deferred | 6 systemic clusters (group B) + 4 items D01–D04, each with verified evidence |
| Guards | 3 rules promoted to the strict build gate (9 total), each **proven to fire**; 22 new property tests |
| Tests | saltests **1229 → 1257**, 0 failed |
| Plugin ABI | untouched — no `src/plugins/shared/` diff, no forwarder diff, interface 106 |

### Why the verdict is conditional

Three fixes were **rejected by independent review before reaching the tree**,
and all three rejections were regressions *this feature had introduced*:

1. **X07** — converting the temp walk to the strict facade broke startup
   cleanup, because its caller still produced legacy bytes (**DC-09, the
   defect class this very review catalogued**).
2. **X07 rework** — the fix *relocated* the same break one link down the
   chain: the prompt stopped appearing instead of the deletion silently
   failing.
3. **X08** — would have blanked the file name on a *destructive* overwrite
   confirmation for names with broken characters, where mojibake was strictly
   better than nothing.

None of the three would have been caught by the build, the 1,257 unit tests
or the static guard. They were caught only because a reviewer that did not
write the fix was required to trace the data path end to end. Two further
things I asserted were **refuted** by review (the ZIP control's code-page
mechanism, and the "homogeneous ANSI" claim about the attributes line, which
became D02).

That record is the argument for the **group-B deferrals**: at this rejection
rate on *contained* fixes, the systemic clusters — 88 dialogs, the ACP byte
tables behind all name comparison, the SDK error-text contract where a naive
sweep would *regress* FTP — carry materially more risk than anything
attempted here, and each is feature-sized with its own regression matrix.
Deferring them is the conservative reading of "this change must not introduce
regressions", not a retreat from the task.

### Verdict

- **Automated gates**: G1–G5 **PASS**, G6 **N/A** (no accepted fix is on a
  per-item path), G7 **partial** (its automatable half passes; the on-screen
  English comparison is in §10).
- **Recommendation**: **conditional go** — the tree is releasable from the
  automated evidence, and the manual sweep in §10 is what converts that into
  an unconditional one. Start with F1–F3; a failure anywhere re-enters the
  fix loop (verify → minimal fix → independent regression review → gates).
- **Not release-blocking, but do not lose**: D01–D04 and the six group-B
  clusters, each with the evidence needed to scope it as its own feature.

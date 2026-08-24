# Closing report — feature 069, finish the contained encoding fixes

**Branch**: `069-finish-encoding-fixes` · **Baseline**: `64dcbb5` (post-068,
unreleased) · **Started**: 2026-08-24

This is the record spec FR-014 requires: every item of the 068 handoff's
section 1 and section 3 with its disposition, its fix, its independent
regression verdict and its check.

## Disposition of every item (FR-001, FR-014)

34 items from the 068 handoff's section 1, plus D01–D05 from its section 3.
**31 fixed · 3 verify-closed · 0 abandoned.** "Check" is the fail-before /
pass-after evidence required by FR-008; "manual V-nn" points at
[quickstart.md](quickstart.md), whose scenarios are the maintainer's sweep.

| Item | Disposition | Fix | Check |
|---|---|---|---|
| F-P6-04 command line | **fixed** | X18 | manual V-01 (window behaviour; not reachable from `saltests`) |
| F-P4-01 viewer default coding | **fixed** — in the lookup, *not* at the intake the finding proposed (the names cross the plugin boundary both ways) | X12 | manual V-14 |
| F-P4-02 viewer caption | **fixed**, both triggers | X12 | manual V-14 |
| F-P1-08 per-user folders | **fixed**, read and write together | X21 | manual V-09 (account-name-gated; verified at the producer level otherwise) |
| F-P1-10 install/portable folder | **fixed**, all three consequences, whole chains | X21 | manual V-09 |
| F-P1-19 Compare Directories | **fixed** | X15 | facade round trips in `saltests` + manual V-03 |
| F-P1-20 archive-edit *Copy To…* | **fixed** (the copy; the *reporting* of a genuine shell failure is deferred — no translated string exists) | X15 | manual V-04 |
| F-P6-01 mdview keeper | **fixed**, and not the way the finding suggested (`MdKeeperReleaseAll` also runs mid-session) | X11 | manual V-15 |
| F-P1-03 startup temp cleanup | **verify-closed** (X06/X07) | — | evidence recorded above |
| F-P1-09 + F-P4-05 cloud roots | **fixed**, all three producers | X16 | manual V-11 |
| F-P1-12 volume information | **fixed**, incl. the uninitialised `fileSystem[20]` | X19 | manual V-12 |
| F-P1-13 `subst` targets | **fixed** | X19 | manual V-12 |
| F-P1-14 labels / mapped drives | **fixed** | X19 | manual V-12 |
| F-P1-27 shares | **fixed** (producer-only; the `shellib.cpp` STRRET half deferred) | X20 | manual V-13 |
| F-P1-05 archiver list files | **fixed**, both directions in one change | X17 | **19 unit checks** on the new converter pair |
| F-P1-06 archiver file APIs | **fixed** | X17 | manual V-10 (7-Zip as a custom archiver; no RAR on this machine) |
| F-P1-07 `salspawn.exe` path | **fixed** | X17 | manual V-10 |
| F-P2-04 wait window | **fixed**, the two confirmed sites (+ a torn-tail trim added on review) | X11 / X15 | manual V-16 |
| F-P2-07 Drive Information type line | **fixed**, the whole `switch` | X19 | manual V-12 |
| F-P2-09 Plugins Manager columns | **fixed** | X11 | manual V-17 |
| F-P2-10 Plugins Manager checkbox | **verify-closed** (X02; the same site as F-P6-02) | — | evidence recorded above |
| F-P2-11 Keyboard Shortcuts | **fixed** | X11 | **`SalU8TrimIncompleteTail`, 12 unit checks** + manual V-17 |
| F-P2-13 Save Configuration prompt | **fixed**, after its producer was settled | X21 | manual V-18 |
| F-P3-07 directory-line tooltip | **fixed** | X11 | manual V-19 |
| F-P4-03 packer titles | **fixed** | X14 | **guard rule `acp-title-seed`**, proven to fire |
| F-P4-07 view-mode names | **fixed** (hygiene, as the verdict scoped it) | X14 | same guard rule + the trim's unit checks |
| F-P1-21 nine site groups | **fixed** — 8 groups; group 3 (DROPFAKE) **reverted on review** and deferred, group 8 is dead code | X15 | manual V-05 |
| F-P1-22 user-menu icons | **fixed** | X11 | manual V-06 |
| F-P1-23 environment expansion | **fixed**, 2 of 3 sites; `icncache.cpp` deferred with reason | X15 | manual V-07 |
| F-P1-24 browse dialogs | **fixed**, the operational half; the ANSI-dialog half analysed as already correct for any code-page-representable path | X21 | manual V-09 |
| F-P1-25 shell/OLE | **fixed**, 4 sites + the shortcut-target probe; jump-list half **verify-closed** | X15 | manual V-08 |
| F-P1-26 Explorer drops | **fixed**, 4 sites | X18 | manual V-02 |
| F-P5-06 FS plugin interface docs | **fixed** (comments only, no ABI) | X11 | inspection: 16 insertions, 0 non-comment lines |
| **D01** Trace Server build | **fixed** | X10 | `msbuild tserver.vcxproj`: `C2535` before, links after |
| **D02** ZIP overwrite `Â` | **fixed** — the 068 blocker no longer holds (the plugin *sends* to a tolerant sink; English bytes identical) | X22 | manual V-22 |
| **D03** filecomp path bar | **fixed** | X13 | manual V-21 |
| **D04** filecomp title | **fixed** | X13 | manual V-21 |
| **D05** spec-kit commit hook | **fixed** | X10 | PowerShell parser: 1 error before, 0 after |

## Baseline (T001, T002)

| What | Value | Evidence |
|---|---|---|
| Debug full build | **SUCCEEDED**, 48 s | `build.cmd full`, Debug x64, 19 plugins, 180 language modules |
| Release full build | **SUCCEEDED** | `build.cmd full release` |
| Unit tests | **1257 checks, 0 failed** | `build\tandemcommander\Debug_x64\saltests\saltests.exe` |
| Encoding guard, strict | **TOTAL: 0** | `python tools\check_encoding.py --strict` |
| Encoding guard, draft | **TOTAL: 183** (`ansi-api-on-utf8-path` 88, `cp-acp-utf8-source` 11, `signed-char-name-byte` 42, `missed-twin` 42) | `--draft` |
| Pre-fix English reference | `build\tandemcommander\Release_x64_prefix069\`, **347 files** | robocopy of `Release_x64` before any change |

Fixtures created (T003, T004): `D:\Zkouška\Můj disk\` (Czech sweep folder with
`Přehled.txt`, `poznámky.txt`, `Účtenka.pdf`, `žluťoučký kůň.docx`,
`příloha.txt`, `Smlouva – kopie.docx`, `1 000 000.pdf` with real NBSP),
`D:\Zkouška\Árvíztűrő tükörfúrógép\bájt.txt`, `D:\Zkouška\Účetnictví\`,
`D:\Zkouška\Kopie\`, `D:\Zkouška\surrogate\Lone<U+D800>surrogate.txt`,
`D:\Zkouška\Dočasné\`, `D:\Zkouška\Šablony\`, `D:\Zkouška\Zálohy\Projekty\`.

## Verify-closed items (T007–T009)

These three were listed as remaining by the handoff but are already fixed in
the tree; **no code change** was made for them (spec FR-017).

| Item | Evidence at HEAD | Disposition |
|---|---|---|
| **F-P1-03** startup `SAL*.tmp` cleanup | `src/cache.cpp:1484-1486` `GetTempPathW` + `SalWToU8` produce a UTF-8 `tmpDir`; `:1499` `SalFindFirstFile` enumerates through the facade; `:1554` hands UTF-8 to `RemoveTemporaryDir`, which itself converts (`src/salamdr3.cpp:1026-1038`, `SalU8ToW` + `SetCurrentDirectoryW`, ANSI fallback); `:1560` `WM_USER_FOCUSFILE` receives the same UTF-8 value | **verify-closed** (X06/X07) |
| **F-P2-10** Plugins Manager "Change Drive menu" checkbox | `src/dialogs5.cpp:495` is `SalGetDlgItemTextU8`, with a comment naming *F-P6-02* — the same site found twice by two perspectives; the sibling at `:492` was converted in the same fix | **verify-closed** (X02, duplicate of F-P6-02) |
| **F-P1-25**, jump-list half | `src/jumplist.cpp:151-163` records the defect and uses `IShellLinkW`/`CreateShellLink(const char*, const char*, IShellLinkW**)`; the ANSI `IShellLink` + `VT_LPSTR` title are gone | **verify-closed** (X03 = F-P4-06); the seven `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, …)` sites and the shortcut-target probe remain in scope |

**Consequence for the scope**: 34 section-1 items = **31 to fix** + 3
verify-closed. Recorded in `research.md` R1 and spec SC-001.

## Guard hygiene (T010)

`signed-char-name-byte` was **narrowed and renamed** to
`acp-byte-table-on-name` rather than deleted, which is what tasks.md T010
proposed. Reason: the rule had two halves and only one premise is void. The
signed-char half rests on plain `char` being signed, which `/J` makes false
(068 ledger L07); the byte-table half (`LowerCase[]`/`IsAlpha[]` indexed by a
UTF-8 name byte) is the real cluster B-2 defect, and the 068 report says the
rule is kept until its replacement lands. Deleting it would have dropped that
signal with nothing in its place.

Result: draft `TOTAL: 183 → 174`; the rule's own count `42 → 33` (the 9 removed
hits were signed-char-only), strict stays `TOTAL: 0`. Line endings (CRLF) and
the file's encoding preserved; 23 insertions, 17 deletions.

## Fixes

| Fix | Item(s) | Group | Change | Regression verdict | Check |
|---|---|---|---|---|---|
| **X10** | D01, D05 | C12 | `handles.h`/`handles.cpp` (3 members → explicit A twins); 3 spec-kit `.ps1` files → pure ASCII | self-evident (build + parser); committed `d06506a` | tserver: C2535 before / links after · PowerShell parser: 1 error before / 0 after |
| **X11** | F-P1-22, F-P2-09, F-P2-11, F-P2-04, F-P6-01, F-P3-07, F-P5-06 | C10 + C8 | 10 files | **REJECTED → corrected → ACCEPTED** ([regression-X11.md](findings/regression-X11.md)) | unit (12 checks, proven) + manual V-06/V-15/V-16/V-17/V-19 |
| **X12** | F-P4-01, F-P4-02 | C7 | `codetbl.cpp`, `viewer3.cpp` | **ACCEPTED** ([regression-X12-X13.md](findings/regression-X12-X13.md)) | manual V-14 |
| **X13** | D03, D04 | C11 | `filecomp/controls.cpp`, `filecomp/worker2.cpp` | **ACCEPTED** (same record) | manual V-21 |
| **X14** | F-P4-03, F-P4-07 | C9 | `packers.cpp`, `packac.cpp`, `salamdr4.cpp`, new `SalU8TrimIncompleteTail`, new guard rule | **review pending** (its helper and the R1 defect were covered by the X11 review) | unit (12 checks) + guard `acp-title-seed`, both proven |
| **X15** | F-P1-19, F-P1-20, F-P1-21 (all 9 groups), F-P1-23, F-P1-25 | C10 rest | 13 files | *review running* | manual V-03/V-04/V-05/V-07/V-08 |
| **X16** | F-P1-09, F-P4-05 | C2 | `drivelst.cpp`, `shiconov.cpp` | *pending* | manual V-11 |
| **X17** | F-P1-05, F-P1-06, F-P1-07 | C4 | `pack1/2/3.cpp` + new `SalU8ToOEM`/`SalOEMToU8` | *pending* | unit: 19 checks · manual V-10 |
| **X18** | F-P6-04, F-P1-26 | C1 | `editwnd.cpp`, `stswnd.cpp`, `toolbar5.cpp`, `viewer3.cpp` | *pending* | manual V-01/V-02 |
| **X19** | F-P1-12, F-P1-13, F-P1-14, F-P2-07 | C3 | `salamdr2.cpp`, `mainwnd5.cpp`, `dialogs3.cpp`, `drivelst.cpp` | *pending* | manual V-12 |
| **X20** | F-P1-27 | C6 | `shares.cpp` (producer only) | *pending* | manual V-13 |
| **X21** | F-P1-08, F-P1-10, F-P2-13, F-P1-24 | C5 | `salamdr5.cpp`, `mainwnd3.cpp`, `salamdr1.cpp`, `salamdr2.cpp`, `execute.cpp`, `shellib.cpp`, `consts.h` | *pending* | manual V-09/V-18 |
| **X22** | D02 | conditional | `plugins/zip/common.cpp` | *pending* | manual V-22 |

No fix in this set lands on a per-item path in the G6 sense (the user-menu icon
reader runs once per *menu item*, the list views once per row — the reviewer
measured both and judged the added cost immaterial). F-P1-27, the per-directory
share marker, will need a timing record.

### X11 — the C10/C8 singles

- **F-P1-22** user-menu icons: new `static UINT ExtractOneIconU8()` in
  `salamdr3.cpp` (the icon picker's idiom from `dialogs3.cpp:2318`), used at
  `:2345` and `:2728`; the legacy `ExtractIconEx` stays as the
  conversion-failure fallback. A lone-surrogate path (feature 066) now loads
  its icon too, which the A call could not.
- **F-P2-09** Plugins Manager: `plugins2.cpp` Version and Location →
  `SalListViewSetItemTextU8`, matching the Name column. `DLLName` is *not*
  normalized at intake — it must stay loadable by the ANSI `LoadLibrary` — so it
  is ACP in the session it was added and UTF-8 after a restart; this sink
  handles both (wide when the value converts, the legacy call when it does not).
  The "Loaded" column stays ANSI (`LoadStr` only — homogeneous).
- **F-P2-11** plugin Keyboard Shortcuts: `dialogs5.cpp` Command column →
  `SalListViewSetItemTextU8`, plus `SalU8TrimIncompleteTail` after the 500-byte
  clamp. The Shortcut column stays ANSI: it carries `GetKeyNameText` output,
  i.e. the keyboard layout's own ANSI text.
- **F-P2-04** safe-wait window: `fileswn3.cpp` and `salamdr5.cpp` →
  `LoadStrU8`, each followed by `SalU8TrimIncompleteTail` so that a long path
  cut mid-character degrades to "path cut short, everything readable" instead of
  sending the whole line — the translated half included — through the legacy
  draw. That trim was added **on review** (finding R3).
  The seven latent plugin-loading messages in `plugins2.cpp` were converted and
  then **reverted on review** (finding R4): their second half is a plugin path
  that is deliberately not normalized, so a UTF-8 template would make the
  buffer invalid UTF-8 and drop the translated half to the legacy draw as well.
  FR-002 permits a latent conversion only as a provable no-op, and this is a
  degradation in the non-shipped case. All twelve uses in that file are ANSI
  again, as at baseline, with a comment recording why.
- **F-P6-01** mdview keeper: new `MdKeeperUnregisterClass()`, called from
  `MdKeeperDisarm()`. **Deviation from the verdict's literal suggestion** ("at
  the end of `MdKeeperReleaseAll`"), for two reasons found while tracing:
  `MdKeeperReleaseAll` also runs when the shared browser process dies
  mid-session (unregistering there would churn the class and would run from
  inside the keeper window's own window procedure), and `MdKeeperDisarm`
  early-returned when the keeper was already unarmed — so a keeper torn down by
  a browser death would have left the class behind exactly as before. The
  reviewer checked all four hazards of this restructuring and found them clear.
- **F-P3-07** directory-line tooltip: `CopyToolTipAnswer` un-`static`'d and
  declared in `gui.h` (the one refactoring this feature permits, research.md R6
  D-T1); `stswnd.cpp` routes through it, so the clamp cuts on a character
  boundary. Below the 5000-byte limit it *is* `lstrcpyn`, so the common case is
  byte-identical. The three refuted sites are untouched.
- **F-P5-06** FS plugin interface: 16 comment lines across five doc blocks in
  `spl_fs.h`. Comments only (zero non-comment diff lines); interface version
  untouched.

### X12 — the viewer (C7)

**F-P4-01 is deliberately not the fix the verdict suggested.** The verdict
recommended normalizing `CCodeTablesData::Name` at intake. Tracing the
consumers turned up one its list did not name: `src/zip.cpp:3289` hands these
names to plugins through `CSalamanderGeneral::EnumConversionTables`, and
`:3304 GetConversionTable()` accepts them back — `dbviewer` and `filecomp` each
store such a name in their own configuration (`dbviewer/renmain.cpp:324`,
`filecomp/textio.cpp:560`). Re-encoding `Name` would change the bytes plugins
receive and invalidate their saved settings: an FR-005 violation and a new
defect in two shipped plugins.

Instead `CCodeTables::GetCodeType` now accepts **both spellings** of the
caller's string (the comparison loop extracted verbatim into
`CodingNameEqual`). The change is purely additive — every string that matched
before still matches — and the viewer's stored default is found again, including
one written by an earlier build, so nothing needs migrating. The invariant break
(the field still has no defined encoding) is **not** fixed; it is recorded for
cluster B-5.

**F-P4-02**: T1 `viewer3.cpp:41` `LoadStr(IDS_VIEWERTITLE)` → `LoadStrU8` (the
wider trigger — every accented file name in cs/sk/hu). T2 the conversion name
appended to the caption is normalized with `SalLegacyToU8Alloc` **for display
only**, leaving the plugin-facing bytes alone.

Side effect, recorded rather than tidied away: the draft guard's `missed-twin`
count rose by 9. `IDS_VIEWERTITLE` is still loaded with `LoadStr` at nine
homogeneous-ANSI sites (`viewer2.cpp` ×8, `viewer3.cpp:1660`) that render
correctly today; converting them would be opportunistic cleanup (FR-007), so
they are left as a documented list for a future viewer-title feature.

### X13 — the file comparator (C11)

**D03** `controls.cpp`: the path bar dropped its text entirely on a conversion
failure; it now falls back to `PathCompactPathA` + `DrawTextA` on a local copy,
the idiom X09 established. **D04** `worker2.cpp`: the fourth title site uses
`SplU8ToWAlloc` + `SetWindowTextW` with an `A` fallback. Beyond the recorded
"ordering" defect this is a real improvement: with an ASCII template and UTF-8
names (en/nl/ro) that title was mojibake and now renders correctly.

### X14 — configuration seeds (C9)

**F-P4-03**: nine seeds (`packers.cpp` ×6, `packac.cpp` ×3) → `LoadStrU8`, and
the `Title` field documented as UTF-8 at `packers.cpp:734`. Evidence that UTF-8
is the field's real contract: `dialogs3.cpp:1861/2111` already use
`SalComboAddStringU8`, and `edtlbwnd.cpp:501/542` already round-trip the Options
list through `SalSetWindowTextU8`/`SalGetWindowTextU8` — so a user-edited title
was already stored as UTF-8 while a seeded one was ACP. The three `TRACE_I`
sites stay ANSI (diagnostic-only text, out of scope by charter).

**F-P4-07**: seven constructor seeds plus the `CViewTemplates::Load` re-seed →
`LoadStrU8`. Measured before changing anything: the longest view-mode name
across the eight shipped languages is **16 bytes** UTF-8 against the 30-byte
`VIEW_NAME_MAX`, so nothing truncates. Russian (30 B) and Ukrainian (38 B)
would — recorded for the language re-enable checklist — and
`CViewTemplates::Set` now calls `SalU8TrimIncompleteTail` so a clamp can never
leave a torn character behind. Every caller of `Set` was checked to pass UTF-8
(the two `LoadStrU8` seed paths, the registry through
`GetValueAux`→`SalRegQueryValueExW8`, and `SalGetWindowTextU8` for a
user-typed name), which is what makes that call safe.

**New shared helper `SalU8TrimIncompleteTail`** (`src/common/salunicode.*`):
drops a trailing *incomplete* UTF-8 sequence in place and leaves a complete
character alone. It is a new symbol — no existing behaviour changes — and it
lives in one of the four files `saltests` links, which is what makes the trims
testable at all.

**A bug in the first attempt was caught and fixed before review.** The obvious
"strip trailing continuation bytes, then the lead byte" loop eats a *complete*
accented character at the end, because the last byte of `ý` (C3 BD) is itself a
continuation byte. Both hand-written trims had it. The 12 unit checks encode
exactly that case and were proven to fail — 4 failures — against the naive
implementation planted deliberately, then pass against the correct one. The
independent reviewer of X11 raised the same defect (finding R1) and confirmed
the tree already carried the guarded version.

**Guard rule `acp-title-seed`**, added and promoted to strict in the same change
as the fix (the feature-052 pattern, so it never flags code that was still
correct). Proven: `TOTAL: 0` on the fixed tree; 2 findings (`packers.cpp:240`,
`salamdr4.cpp:804`) with one seed of each kind reverted.

### X15 — file-system and shell operations (the rest of C10)

| Item | Change |
|---|---|
| **F-P1-19** Compare Directories | `mainwnd5.cpp`: the by-content pair → `SalCreateFile`; the directory enumeration → `SalFindFirstFile` + `WIN32_FIND_DATAW` + `SalConvertFindDataW`, with every former `data.cFileName` use moved to the UTF-8 buffer (the converter empties that field, so a missed use would have broken the comparison outright). The extension scan moves to the converted name — a byte offset taken in the wide record would not survive. |
| **F-P1-20** archive-edit *Copy To…* | `salamdr3.cpp`: the double-NUL multi-strings are now built twice, UTF-8 (the legacy fallback) and UTF-16, and `SHFileOperationW` is used when every name converted. The result is no longer discarded — it is traced. **Not fixed**: there is no translated string for "the copy failed" and adding one would touch all eight shipped languages, so the failure is still not *shown*; what this fix removes is the cause of the silent failure. |
| **F-P1-21** nine site groups | `salshlib.cpp` (archive freshness ×2), `shellsup.cpp` (`CountNumberOfItemsOnPath`, DROPFAKE ×2), `mainwnd4.cpp` (batch wrapper, `$(DOSFullName)` ×2), `packac.cpp` (SFX probe + the SFX enumeration with all 15 name uses moved), `dialogs6.cpp` (drive-accessibility probe), `editwnd.cpp` (`$(DOSPath)`), and group 1 in `zip.cpp`. |
| **F-P1-21 group 1** (conditional, FR-012) | **Proceeded.** `zip.cpp`'s `ViewFileInPluginViewer` already calls `::SalMoveFile` two lines away, so the three `::DeleteFile` and one `CreateFile` were the inconsistent leftovers; the plugin is handed `fileName`/`fileNameInCache`, which this does not touch, so no plugin-visible byte changes. Under a non-ASCII `%TEMP%` the viewed file's size read as 0 and the temp file was never deleted. |
| **F-P1-23** environment expansion | `fileswn9.cpp` (the panel path: expand wide, convert back, legacy fallback) and `mainwnd4.cpp` (`SetEnvironmentVariableW` for the per-drive `=A:` values a child process inherits). **`icncache.cpp` deliberately not converted** — its value is read one line earlier through the *old* ANSI `SalRegQueryValueEx` wrapper and flows into the icon cache; converting only the expansion would create exactly the mixed chain that gets fixes rejected. Recorded for the remaining-facade-migration cluster. |
| **F-P1-25** shell/OLE | `shellsup.cpp` moves to `IShellLinkW` — two defects at one site: the `.lnk` could not be loaded under an accented path, **and** the A interface returned the target in the code page, which the strict `SalGetFileAttributes` then rejected, so a shortcut *to* an accented folder was taken for a file. Plus `mainwnd3.cpp`, `dialogs6.cpp`, `fileswn0.cpp`, `fileswn2.cpp`: try `SalU8ToW` first, keep the code-page widening as the fallback. |

**Scope corrected while doing the work**: the finding lists five sites that were
already correct and are left alone — `worker.cpp:6212` and `shellib.cpp:2649`
(feature 062 and 004 had already put the `SalU8ToW`-first pattern there;
wrapping them again broke the success path in the first attempt and was
reverted), `shellib.cpp:1628` (that *is* the reference implementation),
`execute.cpp:854/866` (Windows/system directory, verified ASCII), and
`worker.cpp:7058` — F-P1-21's "link operation" site, which turns out to sit
inside a `/* … */` design sketch and is **dead code**, out of scope by charter.

### X16 — cloud roots (C2)

**F-P1-09 + F-P4-05** in one change, because the verifier refuted the
"OneDrive-specific" framing: `drivelst.cpp` OneDrive personal and Dropbox, and
`shiconov.cpp` the Google Drive sync root, all took a path Windows hands over
*wide* and degraded it with `ConvertU2A`, whose default code page is the ACP —
silently, with no `WC_ERR_INVALID_CHARS` and no `lpUsedDefaultChar`, so the
caller could not even tell. All three now use `SalWToU8`. The Google Drive site
is the one that needs no accented account name at all: its sibling fifteen lines
up already passed `CP_UTF8`, so `IsGoogleDrivePath` could never match a panel
path like `G:\Můj disk`.

The three OneDrive registry reads (`UserFolder`, and the Business
`DisplayName`/`UserFolder`) move from the old ANSI `SalRegQueryValueEx` wrapper
to `SalRegQueryValueExW8`. The three network-drive reads at the top of the same
file are a different function and a different finding — left alone.

### X17 — external archivers (C4)

**F-P1-05 both directions in one commit.** The verifier's warning was decisive:
`CharToOem(<UTF-8>)` on the pack side and `OemToCharBuff(<OEM> → <UTF-8 field>)`
on the list side are both wrong but wrong in *opposite* directions, so an
ACP-representable name survived a pack → list → extract round trip by accident
while a *pack* of that same name failed outright. Converting one direction alone
would have broken the round trip that works today.

New converter pair `SalU8ToOEM` / `SalOEMToU8` (`src/common/salunicode.*`) with
19 unit checks: ASCII byte-identical both ways, an accented name round-tripping
exactly while differing in between, a CJK name failing cleanly rather than
becoming `?` (the archiver must not be handed a name that does not exist),
invalid UTF-8 in, too-small targets, and NULL. This machine is ACP 1250 /
**OEM 852**, so the accented case is genuinely exercised here.

Applied at nine call sites across `pack1.cpp` and `pack2.cpp`. On the list side
the name length is taken from the converted result (the UTF-8 form can be
longer) and the extension is scanned in the converted name.

**F-P1-06**: 54 mechanical facade substitutions across `pack1/2/3.cpp`
(`SalDeleteFile`, `SalRemoveDirectory`, `SalGetShortPathName`, `SalCreateFile`),
the three list-file `fopen` calls → `_wfopen` on the converted path (the narrow
CRT resolves through the ACP, which is why the packer aborted with "cannot
create the file list" under a non-ASCII `%TEMP%`), and four enumerations moved
to the facade. **F-P1-07**: the `salspawn.exe` path taken wide at the source —
its value is concatenated into a command line that `SalCreateProcess` consumes,
so one code-page byte discarded the *whole* line and the user was told the
*archiver* could not be started.

One warning was introduced and removed in the same group: duplicating a
`NameLen = strlen(...)` statement produced a second C4267, so both halves now
carry an explicit cast — which also removes the pre-existing warning at the
line it came from.

### X18 — the command line (C1)

**F-P6-04** is six lines, not a window conversion. `research.md` R2 established
that the control already has a consistent contract — `SalSetWindowTextU8` and
`SalComboAddStringU8` write it through a wide call that USER32 down-converts,
`SalGetWindowTextU8` reads it back wide — and `CEditLine::InsertText` was its
one violator, pushing raw UTF-8 bytes in. Sending them wide instead moves no
selection offset, no word-break callback ABI and no `WM_CHAR` unit. The same
sink on the drop-target path is converted with it.

**Residual limitation, stated in the changelog**: a character the code page
cannot express now inserts as `?` instead of mojibake — visibly wrong instead of
invisibly wrong. The complete fix needs a Unicode control, which is cluster B-1;
its full surface is enumerated in research.md R2 for whoever takes that on.

**F-P1-26**: the `DROPFILES` payload really is wide (all three sites branch on
`data->fWide`), and converting it through the code page discarded exactly what
the wide payload carried — `toolbar5.cpp`'s strict `FileExists` one line later
then refused the drop silently. Four sites; the viewer additionally moves to
`DragQueryFileW`, since the ANSI form makes the OS itself do the lossy
conversion.

### X19 — volume, subst, labels and Drive Information (C3)

The four items land together **because two rows of that dialog render correctly
today only while their arguments stay code-page bytes**. Converting the
producers without the template would have turned them into mojibake — which is
precisely why the verifier had refuted the UNC and SUBST halves of F-P2-07.

- **F-P1-12**: new `GetVolumeInformationU8` wrapper in `salamdr2.cpp` (wide
  call, UTF-8 out, legacy fallback) used by both call sites of
  `MyGetVolumeInformation`; `MyGetDiskFreeSpace` likewise. Plus the strengthened
  half the verifier found: `mainwnd5.cpp` read an **uninitialised**
  `char fileSystem[20]` after ignoring the result, so whether Compare
  Directories applied the 2-second FAT timestamp tolerance was undefined.
- **F-P1-13**: `MyQueryDosDevice` takes the wide entry point, so `ResolveSubsts`
  no longer splices a code-page target in front of a UTF-8 remainder — the
  delete confirmation can tell a junction from a directory again.
- **F-P1-14**: two local helpers in `drivelst.cpp` for the volume label and the
  mapped-drive name (both were `?` per character for anything outside the code
  page, decided *before* the application saw the string).
- **F-P2-07**: the **whole** type-line `switch` in `dialogs3.cpp` → `LoadStrU8`,
  and its two `WNetGetConnection`/`WNetGetUser` producers converted with it.

### X20 — shares (C6)

**F-P1-27** is **producer-only**: `CShares::Search` and `GetUNCPath` compare the
caller's UTF-8 value against the cached strings, so once `shares.cpp` stops
degrading the W-only `NetShareEnum` results to the code page, the comparisons
match and no consumer needs touching. That also means **no G6 timing is owed**:
the per-item path (`Shares.Search` once per listed directory) is not on the
diff, and the comparison it performs is the same byte comparison as before.

The `shellib.cpp` STRRET half of this finding is **not** converted: each of
those six sites needs its own consumer analysis, and the verifier scoped the
finding's confirmed consequence to the share marker and `GetUNCPath`. Recorded.

## Deferred here, with the reason

| Item | Reason | Recorded for |
|---|---|---|
| **F-P1-23**, the `icncache.cpp` icon-location site | Its value is read one line earlier through the *old* ANSI `SalRegQueryValueEx` wrapper and then flows into the icon cache. Converting only the expansion would leave a mixed chain — the exact DC-09 trap that got two of feature 068's fixes rejected — and settling it means converting that registry read plus every consumer of `iconLocation`. The 068 verifier itself handed this read to another perspective ("cross-perspective hand-off N-10"). | the remaining-facade-migration cluster |
| **F-P1-27**, the `shellib.cpp` STRRET half (6 sites) | Each site needs its own consumer analysis; the verifier scoped the finding's confirmed consequence to the shared-folder marker and `GetUNCPath`, both of which are fixed. | REMAINING-WORK |
| **F-P1-20**, reporting a failed copy to the user | The encoding cause of the silent failure is fixed and the result is now checked, but there is no translated string for "the copy failed" and adding one would touch all eight shipped languages — out of proportion for a fix feature, and not what the finding is about. | REMAINING-WORK |
| **F-P1-21 group 8** (`worker.cpp:7058`, "link operation") | The site is inside a `/* … */` design sketch — **dead code**, out of scope by charter. | recorded here only |
| **F-P4-01**, the invariant itself | Fixed the user-visible symptom, not the "no defined encoding at rest" invariant: the table's names are handed to plugins by `EnumConversionTables` and accepted back by `GetConversionTable`, and two shipped plugins persist them. | cluster B-5 |
| **F-P4-07** on Russian / Ukrainian | Their view-mode names are 30 and 38 bytes in UTF-8 against a 30-byte field, so they would truncate if those languages were re-enabled. `SalU8TrimIncompleteTail` makes the truncation clean rather than torn. | the language re-enable checklist |
| **D02** ZIP overwrite `Â` | Not yet analysed — the conditional item is scheduled after C5. | pending |

## Gates

Run after every accepted group; the figures below are the state after the
second fix commit (`3ad87bb`).

| Gate | Result | Evidence |
|---|---|---|
| **G1** Debug build | PASS | 0 errors. No new warnings in any changed file; one pre-existing `C4267` in `pack1.cpp` disappeared as a side effect of a cast this feature needed. Two warnings this feature *did* introduce (`drivelst.cpp` unreferenced `dummy` locals, after a helper took over their out-params) were removed in the same group. |
| **G2** Release build | *(pending — run once C5 lands)* | |
| **G3** Unit tests | PASS | 1257 → **1289 checks, 0 failed** (+12 `SalU8TrimIncompleteTail`, +19 the OEM converter pair, +1 the G6 enumeration equivalence) |
| **G4** Guard, strict | PASS | `TOTAL: 0` with 10 rules, `acp-title-seed` added and proven to fire |
| **G4** Guard, draft | 183 → **151** | `ansi-api-on-utf8-path` 88→57 (−31, the file-system fixes) · `cp-acp-utf8-source` 11→10 · `signed-char-name-byte` 42→0 (retired) · `acp-byte-table-on-name` 0→33 (its successor — the real cluster B-2 signal) · `missed-twin` 42→51 (+9, all `IDS_VIEWERTITLE`, documented under X12). *The X15 commit message quotes 148: that figure was taken mid-work, before the last two groups. 151 is the measured total and this table is the authoritative record.* |
| **G5** Start/exit health | *(pending)* | |
| **G6** Timing | PASS | The enumeration change is a per-item path, so `saltests` now measures both paths over the 100,000-file fixture in one run. Five runs each over 100,002 entries: **ANSI min 16 / median 31 / max 31 ms**; **facade+convert min 16 / median 16 / max 32 ms**. The wide path is not slower — the ANSI API performs the same conversion inside kernel32, as the X15 reviewer predicted — and each median lies inside the other's range. Coarse by construction: `GetTickCount` granularity is ~15.6 ms. |
| **G7** English spot-check | *(pending — `Release_x64_prefix069` is kept for it)* | |
| **G8** On-screen sweep | *(maintainer — W1–W20 + V-01…V-24)* | |

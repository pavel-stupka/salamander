# Closing report — feature 069, finish the contained encoding fixes

**Branch**: `069-finish-encoding-fixes` · **Baseline**: `64dcbb5` (post-068,
unreleased) · **Started**: 2026-08-24

This is the record spec FR-014 requires: every item of the 068 handoff's
section 1 and section 3 with its disposition, its fix, its independent
regression verdict and its check.

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

## Deferred here

| Item | Reason | Recorded for |
|---|---|---|
| *(filled as decided)* | | |

## Gates

| Gate | Result | Evidence |
|---|---|---|
| G1–G4 per group | *(per fix, below)* | |
| G1–G7 final | *(pending)* | |
| G8 on-screen sweep | *(maintainer)* | |

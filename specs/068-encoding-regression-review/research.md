# Phase 0 Research: Encoding Regression Review and Stabilization

**Feature**: 068-encoding-regression-review · **Date**: 2026-08-24
**Method**: four parallel read-only explorations of the repository — (A) the
encoding machinery, guard, tests and gates; (B) the thirteen encoding
features' records (defect classes, contracts, deferred items, validated
surfaces); (C) a sized triage of every remaining ANSI boundary in the core;
(D) the plugin text boundary in both directions — consolidated here. The
reviewers' reference distilled from (B) is
[contracts/encoding-contract-checklist.md](contracts/encoding-contract-checklist.md).
No open clarification markers remained after `/speckit-clarify` (Q1–Q4 in
spec.md); the one item deferred to planning — how "run-to-run noise" is
measured — is decided in R6.

## R1. Scope and sizing (clarification Q1: whole core + plugin boundary)

**Baseline**: tag `v0.1.4` (build 188). Unreleased delta `v0.1.4..HEAD` =
commits `3245809` (065 mdview instant render), `f57a851` (066 surrogate
names), `c577ff3` (067 drive-info numbers). 066/067 get full line-level
review (P6); 065 only at its path boundary (P5, research D: clean UTF-16 —
the file path never enters the WebView2 URL; only the plugin-shared strict
converter at `viewer.cpp:257/577/801` is an encoding risk).

**Core under review**: `src/*.cpp`, `src/*.h`, `src/common/**` minus
`src/common/dep/**` — 217 files, 136 `.cpp`. **The core is built without
`UNICODE`/`_UNICODE`** (`salamand.vcxproj` has no `CharacterSet`; toolset
default `NotSet`; `sal_base.props:16` defines neither), so **every
un-suffixed Win32 text API is an ANSI call**. Sizing (non-comment matching
lines, ±few %):

| Boundary | ANSI/un-suffixed sites | files | W / house sites |
|---|---|---|---|
| File-system APIs (B1/B2) | 287 (`DeleteFile` 41, `GetModuleFileName` 40, `GetDriveType` 26, `CreateFile` 26, `LoadLibrary` 24, `FindFirstFile` 23, `SetCurrentDirectory` 21, `GetShortPathName` 21, `RemoveDirectory` 16, `FindNextFile` 14, …) | 52 | 47 W (`worker.cpp`, `common/salfileio.cpp`) |
| Shell APIs (B2) | 39 (`SHGetFileInfo` 8, `SHGetFolderPath` 7, `SHGetPathFromIDList` 6, `ExtractIconEx` 5, `ShellExecute` 4, `SHFileOperation` 3, `GetOpen/SaveFileName` 5, `DragQueryFile` 2, `SHBrowseForFolder` 1) | 20 | 13 W |
| Process/environment (B2/B5) | 23 (`CreateProcess` 7, `Set/GetEnvironmentVariable` 12, …) | 10 | 2 W |
| Registry (B7) | 77 raw (`RegOpenKeyEx` 34, `RegOpenKey` 13, …) + 51+9 uses of the **older ANSI wrappers** `SalRegQueryValue(Ex)` (`consts.h:2501-2502`) | 15 | 9 facade (`SalReg*W8`, `regwork.cpp`) + 17 raw W |
| UI text APIs (B3/B4) | 428 (`SetWindowText` 65, `GetTextExtentPoint32` 47, `DrawText` 47, `SetDlgItemText` 44, `InsertMenu` 43, `MessageBox` 39, `ExtTextOut` 29, `GetDlgItemText` 28, …) + message-based: `WM_SETTEXT/GETTEXT` 54, `CB_*STRING` 19, `LB_*STRING` 13, `LVITEM` 20, `TVITEM` 4, tooltip `TOOLINFO`/`TTM_ADDTOOL`/`TTN_NEEDTEXT` 4/1/1 | 59 | 156 W + U8 sinks 156 (excl. 383 `SalMessageBox`) |
| Conversions/probes | `MB2WC(CP_ACP)` 43, `WC2MB(CP_ACP)` 32, `CharToOem`/`OemToChar` 17 (`pack1/2`, `codetbl`, `mainwnd4`), `IsCharAlpha*` 13, `CompareString` 7, `lstrcmp(i)` 4, `CharNext/Prev` 4, `MB_ERR_INVALID_CHARS` raw probe 1 | — | `SalU8ToW*` 154, `SalWToU8*` 61, `SalLegacyToU8Alloc` 11 |
| Composition | printf with `LoadStr(` format **140**; `ExpandPluralString(LoadStr(` 4; bare `LoadStr(` 1453 | — | `LoadStrU8(` format 110; bare 160 |

**Frontier**: 44 core `.cpp` contain a house conversion, 89 contain a raw
ANSI FS/UI/shell/process call, **36 contain both** (half-migrated — highest
review value), 53 are ANSI-only, 8 conversion-only, 39 neither.
`common/handles.cpp/.h` inflate FS/process/registry by ~25 (debug shims: one
boundary each).

**Reference inputs** (details in the contract checklist): 20 defect classes
DC-01…DC-20, of which only DC-03/04/05/06/07 have a build-time rule (DC-02
half); 12 binding contracts (the spec named 5 — 052/058/063/066/067 — and
the audit found 7 older ones: 004, 005, 010, 041 ×2, 042 ×2, 062); an
89-row deferred ledger L01–L89 (37 core-encoding, 13 plugin, 8 translation/
tooling, 20 non-encoding, 11 verification-debt); the guard
`tools/check_encoding.py` is **currently clean** (0 findings, 4 live
suppressions), scans **core `.cpp` only** (no headers, no plugins);
`saltests` = 1229/0 and links only `src/common/*` (so `PrintDiskSize`,
`NumberToStr` and the registry facade are testable by property only).

## R2. Decision — review vehicle: parallel subagents, three separated roles

**Decision**: perspectives run as **parallel read-only subagents** (Agent
tool), one per perspective with a written charter, a bounded file/class list
and the seeded questions of R7; findings return in the data-model Finding
shape. **Verification** of a finding is a *separate fresh agent* with a
refute-first charter (never the raising agent, never the main context that
will author the fix). **Regression review** of a fix is a *third fresh agent*
given the diff and the fixer's affected-surface list. The main context
orchestrates, applies fixes, runs gates and writes the report; it never
verdicts a finding it raised nor accepts a fix it wrote.

**Rationale**: FR-006 and FR-008 require independence in both directions;
feature 060 used the main context as verifier because it had the deepest
design knowledge of a small delta — here the delta is the whole core, and
the main context authors fixes, so it is the wrong adversary for its own
work. Agent-tool parallelism gives the isolation without the multi-agent
workflow machinery (not opted into by the user; if the user opts in later,
the same charters run unchanged as a workflow).

**Alternatives rejected**: single-pass self-review (author blindness);
verifier = main context (would violate the "reviewer ≠ author" rule the user
asked for in "agent musí nezávisle ověřit").

## R3. Decision — tiered triage (how ~1,000 sites become a finite audit)

Line-by-line reading of every ANSI call is neither feasible nor informative;
what matters is **which value** reaches each call. The inventory therefore
classifies by data flow in three tiers:

- **Tier 1 — line-level, mandatory** (the defect-class patterns): the 36
  half-migrated files in full; every `SalU8ToW*`/`SalWToU8*` call (154+61 —
  probe/offset/buffer correctness, DC-11/12/13/14/20); every `CP_ACP`
  conversion (75 — DC-02: legitimate post-probe fallback vs. primary
  conversion of UTF-8); every `LoadStr`-format composition (140 — DC-03/05/
  19: is any argument UTF-8?); every message-based ANSI sink with a
  candidate UTF-8 value (DC-06); the 4 suppressions; the tooltip tokens
  (DC-08); the raw strict probe (DC-13); signed-char patterns and the
  `InitLocales` byte tables (DC-15); the missed-twin scan (DC-18: for every
  `LoadStrU8(IDS_X)`/`Sal*U8` a remaining `LoadStr(IDS_X)`/`A` twin); ledger
  rows L01–L21, L30, L47–L48.
- **Tier 2 — data-flow classification** of the 287 FS / 39 shell / 23
  process / 77+60 registry ANSI calls: (a) receives a UTF-8 value (panel
  path, `CFileData::Name`, `SalGetTempFileName` output, configuration value,
  any `UTF8_SOURCE`) → **defective** (DC-01) unless proven ASCII-only by
  construction; (b) ASCII by construction (drive roots, fixed literals,
  ASCII registry key names) → verified-correct with that evidence; (c) an
  ANSI producer→ANSI consumer chain that is consistent today → verified-
  correct-consistent, but **flagged when the value can carry non-ACP text**
  (L05 volume labels) or later meets a strict facade (DC-09). Sites are
  grouped per pattern-in-function (data-model Site rule) — every location
  listed, one evidence line.
- **Tier 3 — file-level sanity** of the 53 ANSI-only files: confirm which
  receive UTF-8 values at all (`src/common/{trace,heap,handles,messages,
  sheets,strutils,allochan}.cpp`, `bugreprt`, `callstk`, `codetbl` mostly
  carry ASCII/diagnostic text — recorded as such, not read line by line);
  `cache.cpp` (14 `DeleteFile` on temp paths) and `drivelst.cpp` are
  promoted to Tier 1 by the seeds in R7.
- **Plugin boundary (B8)**: the 10 seeds of R7, ledger L38–L50, the SDK
  headers' encoding statements, `CSalamanderGeneral` text services in
  `src/zip.cpp`, plugin→core intakes (`plugins1/2/3.cpp`, `packers.cpp`),
  the plugin-shared `Spl*` helpers, and — per enabled plugin — only the
  sites where it consumes names/paths/numbers the core hands it. Plugin-
  internal fixes follow FR-012; `portables` (0 UTF-8 handling) is flagged
  for classification.

Coverage proof (SC-001): the inventory lists, per boundary B1–B8, every
Tier-1 site individually and every Tier-2/3 group with its locations.

## R4. Decision — perspective roster (7) and boundary coverage

| # | Perspective | Boundaries | Defect classes | Primary files | Ledger rows |
|---|---|---|---|---|---|
| P1 | File-system, shell & launch boundary | B1, B2, B5 (launch) | DC-01, DC-02 (A→W), DC-09 (ANSI producer → strict facade) | `pack1/2/3`, `salamdr1/2/3`, `cache`, `fileswn6/8`, `plugins2`, `drivelst`, `shellib`, `shiconov`, `snooper`, `geticon`, `mainwnd3/4`, `editwnd`, `execute`, `worker`, `salmoncl`, `common/handles` | L01–L05, L08–L11 |
| P2 | UI text sinks & composition | B3, B4 | DC-03/04/05/06/08/18/19 | `dialogs*`, `fileswn*`, `gui`, `finddlg*`, `pwdmngr`, `shellsup`, `mainwnd*`, `viewer3`, `stswnd`, `msgbox`, `menu*`, `toolbar*`, `filesbx2` | L06, L12–L16, L30 |
| P3 | Converter & measurement machinery | cross-cutting | DC-10/11/12/13/14/15/16/20 | `common/salunicode`, `salfileio`, `salpath`, `winlib`, `gui`, `tooltip`, `stswnd`, `msgbox`, `salamdr4` (`CTruncatedString`), `salamdr2` (`DoExpandVarString`), `salamdr1` (`InitLocales`), `fileswn0/4/5` | L07, L21, L23–L28 |
| P4 | Configuration, clipboard & external channels | B5, B7 | DC-06 (clipboard), DC-17 | `mainwnd2`, `icncache`, `regwork`, `drivelst`, `shiconov`, `salmoncl`, `bugreprt`, `salamdr4/6`, `mainwnd4`, `packers`, `dialogsp`, `edtlbwnd` | L17, L32 |
| P5 | Plugin boundary | B8 | DC-05/06/07/13 at the boundary, seeds S1–S10 | `plugins/shared/spl_*.h`, `splunicode.h`, `winliblt.cpp`, `src/zip.cpp` (`CSalamanderGeneral`), `plugins1/2/3.cpp`, `packers.cpp`, `pluglegacy.*`, `mdview/viewer.cpp`, `sftp/operats.cpp`, `ftp/ftputils.cpp`, plugin sites of L38–L50 | L38–L50, L48 |
| P6 | User input & the unreleased delta | B6 + 066/067 line-level | any | `winlib` (`EditLine`), `fileswn0` (quick search `WM_CHAR`), `editwnd` (command line), `finddlg*` (masks), `dialogs4` (hot paths), and **every line of `git diff v0.1.4..HEAD -- src`** with a general regression lens (memory, lifetimes, buffers, failure paths — not only encoding) | L26, L84, L85 |
| P7 | Guard & test designer | cross-cutting | the 14 unguarded classes | `tools/check_encoding.py`, `src/saltests/saltests.cpp`, `build.cmd:193-230` | L20 (scope), L87 |

P7 raises no product findings; it produces the rule/test designs of R8 and
audits the guard's own blind spots (headers never scanned; `wide_fallback()`
suppresses DC-16's legacy branch; plugins excluded by 042 policy — stays
excluded, the plugin sweep is P5's job). Every boundary B1–B8 has at least
one perspective; every Tier-1 file has at least one owner; P6 doubles as the
056-gap closer (L87: the last release review's encoding perspective returned
nothing structured for the 052–055 delta — P6 re-reads those diffs too).

## R5. Decision — verification and fix-acceptance protocol (FR-006…FR-010)

1. **Finding** — raised by P1–P6 with Site(s), class, and a mandatory
   failure scenario (surface, locale/UI language, what the user sees or
   which operation fails). No scenario → not a finding (recorded as a note).
2. **Verification** — a fresh agent per finding (batched by file for cost),
   charter: *refute this*; must return CONFIRMED with `file:line` evidence
   and a reproduced data path, or REFUTED with the evidence that the value
   cannot be UTF-8 / cannot reach the sink / is ASCII in all 8 shipped
   translations. Contested or high-impact findings get a second verifier.
   REFUTED → no change (FR-006).
3. **Scope test** — CONFIRMED findings are classified: core & shipping →
   fix; plugin-internal → FR-012 test (user-visible in a shipped
   configuration, local, enumerable regression surface) else deferred;
   disabled-language-only → latent, re-enable checklist; non-encoding →
   FR-015 test (data-only or one-line local) else deferred; vendored/dev
   tooling → deferred (Q1).
4. **Fix** — minimal, by the main context (or a fixer agent for plugin-
   local fixes), with the affected-surface list (grep of every consumer of
   the changed symbol/resource) and the check of R8.
5. **Regression review** — a fresh agent per fix (or per batch of fixes to
   one file), charter: *find a regression*; verifies each affected surface
   (unchanged/corrected), English/ASCII byte-identity (ASCII `LoadStrU8 ==
   LoadStr`, W call with ASCII input == A call), plugin-facing bytes
   (`git diff -- src/plugins/shared` documentation-only; forwarders never
   pass `u8`), previously validated behavior (066/067 quickstart scenarios
   touched by the change), and — for per-item paths — that a timing record
   exists (R6). ACCEPTED or REJECTED with reasons; REJECTED → rework or
   withdraw to deferred.
6. **Bounded re-verification** — a fix re-opens exactly the Sites it
   touches and the affected gates; a fix to `src/common/salunicode.*`,
   `winlib.*`, `salfileio.*`, `salpath.*`, `LoadStr*`, `NumberToStr`/
   `PrintDiskSize`, the registry facade or the clipboard helpers re-runs the
   full sweep W1–W20.

## R6. Gates, sweep and the timing method (FR-011, Q2, Q3)

| Gate | Check | Pass bar |
|---|---|---|
| G1 | `build.cmd full` (Debug x64) | 0 errors; no new warnings in files changed by this feature |
| G2 | `build.cmd full release` | same |
| G3 | `…\Debug_x64\saltests\saltests.exe` (built by G1, never run by `build.cmd`) | `N checks, 0 failed`, N ≥ 1229 |
| G4 | `python tools\check_encoding.py --strict` | `TOTAL 0`; each new rule proven to flag the pre-fix tree (`git stash` demonstration) |
| G5 | Start/exit health, Debug binary, Trace Server running | exit 0 after graceful close; no "monitored handles remained opened" box; no `_CrtCheckMemory failed` box; no crash report; no assertion. (The CRT leak dump is disabled — `salamdr1.cpp:3772-3774` commented out — so this is the observable bar, as in 060 G4) |
| G6 | Timing, only for fixes on per-item paths | below |
| G7 | English spot-check (W1–W6, W13) | identical to a `v0.1.4` Release build side by side |

**Sweep**: W1–W20 (quickstart) in Czech UI and Hungarian UI (Q3; both
`.slg` modules are shipped and enabled), each PASS/FAIL/WAIVED per language.

**Timing method (Q2, "run-to-run noise")**: fixture
`tools\create-test-fixtures.ps1 -Perf` (100,000 files at
`%TEMP%\salamander-test\perf` — the only large-folder generator in the
repository; no timing harness exists, so stopwatch is the instrument). One
discarded warm-up, then **5 runs before** (pre-fix binary) and **5 after**
of the affected operation (listing: Enter into the folder; sorting: Ctrl+F4
then Ctrl+F3; icon reading: Alt+3/Alt+4 then Alt+2). **Noise = max − min of
the five baseline runs; pass iff the after-median lies within
[baseline-min, baseline-max].** All ten values go into the Fix record.
Rationale: a percentage threshold is meaningless at this fixture size
(sub-second listing, human stopwatch); the min–max envelope of the baseline
is exactly what "within run-to-run noise" means and is reproducible.

## R7. Seeded questions (inputs the perspectives must confirm or refute — never accept)

*Plugin boundary (exploration D, ranked):*
- S1 `CSalamanderGeneral::CopyTextToClipboard` (`zip.cpp:2418` → `salamdr4.cpp:1246`) puts **raw bytes** on `CF_TEXT`; the correct `CopyTextToClipboardU8` (`salamdr4.cpp:1190`) is not wired to the plugin wrapper. Plugin "copy name" → mojibake?
- S2 `ToLowerCase`/`ToUpperCase`/`GetLowerAndUpperCase`/`StrICmp*` (`zip.cpp:887/898/878/919`) are 256-byte ANSI tables applied in place to UTF-8 names → continuation-byte corruption? Which enabled plugins call them on names?
- S3 plugin-shared `Spl*` helpers (`splunicode.h:29/43/59/70/94`) are strict UTF-8 while the core is WTF-8 → every plugin fails on surrogate names (L48). FR-012 test: is a WTF-8-aware `Spl*` a local, enumerable change?
- S4 `SafeGetOpenFileName`/`SafeGetSaveFileName` exposed to plugins are ANSI (`salamdr6.cpp:1699/1720`; W variant `:1744` core-internal) → plugins receive CP_ACP paths from browse dialogs?
- S5 custom packer/unpacker titles skip `SalLegacyToU8Alloc` (`plugins1.cpp:584/610` → `packers.cpp:734`) — the DC-17 class 052 left unswept (L17).
- S6 `spl_fs.h:244/250/260/265/330` and `spl_com/arc/base/gui.h` carry **no encoding contract**; `GetCurrentPath`/`GetFullName`/`GetFullFSPath`/`ListCurrentPath` outputs are consumed by the core as UTF-8 without normalization (`fileswn1/2/8/9.cpp`, `shellsup.cpp`, `zip.cpp:5863/5962`).
- S7 `pluglegacy.h/.cpp` and `PluginSupportsUTF8` are dead (zero call sites) while `spl_com.h:210` and `plugins.h:16-23` still describe the shim as live.
- S8 frozen `PrintDiskSize`/`ExpandPluralBytesFilesDirs` mixed output at the boundary (L38–L41): confirm the freeze still holds and that no *core* consumer of the forwarders exists.
- S9 `CSalamanderGeneral::LoadStr` (`zip.cpp:1625`) is `LoadStringA`; no UTF-8 counterpart is exported — record as contract gap, not a defect.
- S10 `sftp/operats.cpp:24-43` lenient `MultiByteToWideChar(CP_UTF8, 0)` and no `\\?\` prefix vs. the ftp reference (`ftputils.cpp:3349`) — plugin-local; FR-012.

*Core (explorations A, C, and the ledger):*
- C-a `cache.cpp` — 14 ANSI `DeleteFile` on cache temp paths; `SalGetTempFileName` now returns UTF-8 (063) — does a non-ASCII `%TEMP%` leave cache files undeleted?
- C-b `pack1/2/3.cpp` — 62 ANSI FS calls plus the `CharToOem`/`OemToChar` cluster on packer command lines; which receive panel/archive names?
- C-c `GetModuleFileName` ×40 ANSI — the executable path; is a non-ASCII installation directory (e.g. `C:\Users\Jiří\Apps\`) handled, or do `.slg`/plugin/help paths break? (L14's premise depends on this.)
- C-d `GetShortPathName` ×21, `SetCurrentDirectory` ×21, `GetDriveType` ×26 ANSI — `SalGetDriveTypeU8` exists (062): DC-18 twins?
- C-e `FindFirstChangeNotification` 3 ANSI tokens after 058 converted 3 sites — the HANDLES shims or a residual site?
- C-f `InitLocales` fills `IsNotAlphaNorNum[256]`/`IsAlpha[256]` byte-wise (`salamdr1.cpp:939-943`) — every consumer on a UTF-8 name is DC-15.
- C-g the single raw `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS)` outside `salunicode.cpp` (DC-13; 066 required it to be marked reviewed).
- C-h the 4 live suppressions (L12–L15) — L14's "from `FindFirstFile`, therefore ANSI" premise is stale since 004/066.
- C-i `msgbox.cpp` `MSGBOXEX_PARAMS` checkbox/hint/URL ANSI (L16) — reachable from a shipped plugin with non-ASCII text?
- C-j the older ANSI registry wrappers `SalRegQueryValue(Ex)` (51+9 uses) — which values can carry names/paths?
- C-k `consts.h:105/133/100/110/188/234` stale pre-004 comments on facade functions — documentation finding (reviewers must not use them as evidence).
- C-l L07 (`fileswn8.cpp` signed-char Recycle-Bin guard), L01–L05 (062 E6 sites), L08/L10 (converter API gap, uninitialized buffer), L21 (non-ASCII date/time format → automated construction).

## R8. Decision — durable guards to add (FR-010, SC-008)

Each new rule/test is written **before** the corresponding fixes, proven to
flag the pre-fix tree, and left in `--strict` mode on every `build.cmd`:

| Class | Mechanism | Shape |
|---|---|---|
| DC-01 `ansi-api-on-utf8-path` | `check_encoding.py` rule | un-suffixed/`A` name-taking Win32 call whose argument is a `UTF8_IDENT`/`UTF8_SOURCE` value (`GetPath()`, `->Name`, `*Path*`, `SalGetTempFileName`), outside `HANDLES()` shim definitions; facade names (`Sal*`) exempt |
| DC-02 (A→W half) | extend `cp-acp-display` | `MultiByteToWideChar(CP_ACP` whose source is a tracked UTF-8 value and which is not the `else` branch of a `SalU8ToW*` attempt |
| DC-08 `ansi-tooltip-handler` | rule | `TTN_NEEDTEXT`/`TTN_GETDISPINFO`/`TTM_ADDTOOL`/`TOOLINFO` without `W` in a file that composes with a `UTF8_SOURCE` |
| DC-13 `strict-probe-rejects-wtf8` | rule | `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS` outside `src/common/salunicode.cpp` without a `// encoding-check: allow … - non-name text` marker |
| DC-14 `lossy-lenient-at-intake` | rule | `WideCharToMultiByte(CP_UTF8, 0,` / `SalU8ToWDisplay*` whose result is assigned to a `UTF8_IDENT` (name/path) or passed to a facade |
| DC-15 `signed-char-name-byte` | rule + saltests | `[Nn]ame\[…\]\s*(<=|<|>=|>)\s*(' '|32|0x20)` without `(unsigned char)`; saltests pins the corrected helper if one is introduced |
| DC-18 `missed-twin` | rule | for each `IDS_*` id used with `LoadStrU8(` anywhere, flag a remaining `LoadStr(` of the same id (unless marked `allow missed-twin - ANSI sink by design`) |
| DC-20 | saltests | if `SalU8ToW` gains a distinguishable too-long result (fix for L08), pin it: too-long ≠ invalid, both still 0-terminate |
| DC-17 | inventory-driven | `UTF8_IDENT` gains the identifiers of any newly contracted cached string (packer titles) per the 052 pattern |
| registry facade | saltests property | still not linkable — WTF-8 write-probe and read-side totality pinned through `SalU8ToW`/`SalWToU8` on the facade's exact call shapes (the 052 precedent) |

Rules that would produce false positives beyond a handful of annotated sites
are shipped report-only first, tuned on the inventory, then made strict —
the guard must stay at 0 findings on `build.cmd`.

## R9. What deliberately does not change

- The plugin-facing text/formatting services (`NumberToStr`, `PrintDiskSize`,
  `ExpandPlural*`, `PointToLocalDecimalSeparator`, `CopyTextToClipboard`,
  `LoadStr`, `SalMessageBox*`) keep byte-identical output; interface
  version stays 106 (FR-009, spec Q1). A plugin-facing defect (S1, S4) is
  fixed only if the fix is byte-identical for every existing caller — e.g.
  a *new* U8 entry point is out (interface bump); re-routing an existing
  entry point is in only if its bytes for valid-ACP input are unchanged
  (the regression reviewer decides on evidence).
- L18 (global `LoadStr` conversion), L19 (central `WM_NOTIFYFORMAT`), L36
  (`activeCodePage` manifest): standing architectural decisions — not
  revisited.
- Disabled languages (ru/uk/zh): latent findings go to the re-enable
  checklist (L52, L53).
- `src/common/dep/**`, `src/tserver`, `utils/`, `tools/translate` (Q1):
  findings recorded (L20, L53–L57), not fixed.
- The strict plugin-shared `Spl*` helpers (L48/S3) change only if P5 proves
  the FR-012 conditions; otherwise re-deferred with the reason.

## R10. Artifacts produced during implementation

- `inventory.md` — per boundary B1–B8: Tier-1 sites individually, Tier-2/3
  groups with all locations; classification, evidence, perspective; the
  DC sweep table (20 rows → complete/partial) and the contract compliance
  table (B1–B12 obligations → verdict).
- `review-report.md` — scope and method (this file), perspective coverage
  lists, inventory summary, Findings (F), Fixes (X), Deferred items (D —
  every L-row re-dispositioned first), Gates (G1–G7), Sweep (W1–W20 ×
  cs/hu, + en), stability verdict. Every code change traces to one `F` and
  one ACCEPTED `X` (SC-003).
- `CHANGELOG.md` Unreleased entries for user-visible fixes (FR-014).
- `translations/languages.cfg` re-enable checklist note(s) for latent items.

## R11. Alternatives considered and rejected

- **Delta-only review** (like 056/060): rejected by Q1 — the user's example
  defect lived outside any recent delta.
- **Full plugin-internal sweep**: rejected by Q1 — unbounded regression
  surface across 19 plugins; boundary + FR-012-local fixes instead.
- **Application-wide `LoadStr` → UTF-8**: rejected (L18, tried in 041).
- **Building the core with `UNICODE`**: rejected (004 R1; would touch every
  file — the opposite of a stabilization).
- **Running the guard over plugins**: rejected (042 policy: the guard must
  not change plugin behavior; P5 covers plugins by reading).
- **A timing harness inside the app** (Debug TRACE timers): rejected — new
  development; the stopwatch envelope method (R6) is sufficient for a
  "within noise" gate on a 100k-entry fixture.

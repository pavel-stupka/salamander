# Research — Finish the Contained Encoding Fixes

**Feature**: 069-finish-encoding-fixes · **Phase**: 0 · **Date**: 2026-08-24
**Baseline**: `64dcbb5` (post-068, unreleased; release 0.1.4 = build 188)

Evidence base: the 068 verdicts (`specs/068-encoding-regression-review/findings/verdicts-V*.md`)
were re-extracted in full for all 34 section-1 items and D01–D05, then every
primary site was re-located in the tree at HEAD. This document records the
decisions that follow; per-site chains go into `tasks.md`.

---

## R1 — Scope correction: two items are already done, one is a duplicate

Verified at HEAD (`grep -rn "feature 068" src`, which lists every marker the
nine landed fixes left):

| Item | Handoff said | Tree at HEAD | Decision |
|---|---|---|---|
| **F-P2-10** Plugins Manager checkbox | remaining | **already fixed** — `src/dialogs5.cpp:495` is `SalGetDlgItemTextU8`, with a comment naming *F-P6-02* and feature 052 | **verify-closed**. F-P2-10 and F-P6-02 are the same site found by two perspectives; X02 fixed it. No change. |
| **F-P1-03** startup temp cleanup | already fixed by X06/X07 (verify) | `src/cache.cpp:1484` `GetTempPathW`, `:1499` `SalFindFirstFile`, UTF-8 `tmpDir` into `RemoveTemporaryDir` (itself converted, `salamdr3.cpp`) and `WM_USER_FOCUSFILE` | **verify-closed** |
| **F-P1-25** jump-list half | already fixed by X03 (verify) | `src/jumplist.cpp` carries the F-P4-06 fix (7 `IShellLinkW`/`SalU8ToW` uses) | jump-list half **verify-closed**; the seven `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, …)` sites and the `IShellLinkA::GetPath` probe remain |
| F-P6-04, F-P1-19, F-P1-20, F-P2-09, F-P2-11, F-P4-02, … | remaining | confirmed still defective at HEAD (spot-checked each primary site) | fix |

**Corrected count**: 34 section-1 items = **31 to fix** + 3 verify-closed
(F-P1-03, F-P2-10, and — as a half — the jump list), of which one (F-P5-06)
is documentation-only. Plus D01, D03, D04, D05 to fix and D02 conditional.
The spec's SC-001 ("at least 33 of 34 fixed-and-accepted, F-P1-03 counts as
closed") is amended by this finding: **31 fixed + 3 verify-closed**; the
closing record states it.

**Why this matters beyond bookkeeping**: it confirms the handoff's own warning
that the review's section 1 was written before the last fixes landed. Every
item therefore gets a "still defective at HEAD?" check as the first step of its
task, before any code is written (see `contracts/fix-protocol.md` step 1).

---

## R2 — The command line (F-P6-04): fix at the sink, do not convert the window

This is the spec's top item and the one design question worth resolving before
planning, because the obvious reading ("the control is ANSI, so this needs a
Unicode window") would pull the out-of-scope cluster B-1 into the feature.

**What the tree actually shows** (`src/editwnd.cpp`):

| Path | Call | Effect today |
|---|---|---|
| set whole text (after running a command) | `:576` `SalSetWindowTextU8(HWindow, command)` | `SetWindowTextW` → USER32 thunks W→ACP (the combobox is ANSI: `:1716` `CreateEx(0, "ComboBox", …)`) ⇒ control holds **ACP characters** |
| history | `:1769` `SalComboAddStringU8` | same wide-then-thunk path ⇒ **ACP characters** |
| read back | `:390`, `:2044` `SalGetWindowTextU8` | `GetWindowTextW` → `SalWToU8` ⇒ correct UTF-8 out of ACP characters |
| **insert a name/path** | `:355` `InsertText` → `SendMessage(EM_REPLACESEL)` | raw **UTF-8 bytes** pushed into an ANSI control ⇒ stored as mojibake characters ⇒ read back as UTF-8 *of the mojibake* |

So the control already has a consistent contract — *ACP characters inside,
UTF-8 at the application boundary* — and `InsertText` is its **only**
violator. The defect is not the window model; it is one function bypassing the
house pattern the same file already uses two functions away.

**Decision**: convert `CEditLine::InsertText` to the wide path the rest of the
control already uses — `SalU8ToWAlloc(s)` + `SendMessageW(HWindow,
EM_REPLACESEL, TRUE, (LPARAM)w)`, falling back to the existing ANSI
`SendMessage` when conversion fails (FR-004). Apply the same to the internal
`SALCF_FAKE_REALPATH` drop path (`:1198`) which feeds the same sink.

**Consequences, all verified as non-changes** — this is why the alternative was
rejected:

- **Selection offsets are untouched.** `:577` `EM_SETSEL(selFrom, selTo)` takes
  UTF-8 byte offsets into `command` while the control holds ACP characters —
  already mismatched for accented text *today*, unchanged by this fix. It is a
  separate, pre-existing defect and is **not** in scope (recorded as a note).
  Option (b) below would have *changed* that unit and forced a pass over
  `:1136` `EM_CHARFROMPOS`, `:1144` `EM_POSFROMCHAR`, `:1197`, `:2045`/`:2056`.
- **No word-break-proc change.** `InstallWordBreakProc` (`:273`) installs an
  `EDITWORDBREAKPROCA` callback that receives ANSI bytes; a Unicode control
  would hand it WCHARs — a callback-ABI change this fix avoids entirely.
- **No `WM_CHAR` change.** `CEditLine::WindowProc`'s `(TCHAR)wParam` switch
  keeps receiving ANSI units.
- **No measuring change.** `:1369` `GetWindowText` (A) feeds text width
  measurement; the control's stored characters are ACP before and after.

**Rejected alternative (b)** — create the combobox with `CreateExW` and set
`UnicodeWnd` (winlib already supports this: `winlib.cpp:203` `CreateWindowExW`,
`:391/408/429/465` pick the W subclass proc and W APIs). It is the *complete*
fix: it would additionally make names outside the system code page work
(Cyrillic on a Czech machine). It is rejected here because it changes the
window model of a control whose every text path, the word-break callback ABI,
the `WM_CHAR` unit and all five selection-offset sites move with it — the exact
shape and hazard set the review isolated as cluster **B-1**. It is recorded in
the closing handoff as B-1's first candidate, with this enumeration.

**Residual limitation, stated honestly** (FR-013): after the fix, a name with
characters the system code page cannot represent inserts as `?` (Windows' own
substitution on the W→ACP thunk) instead of mojibake. That is strictly better
— visibly wrong rather than invisibly wrong — but it is not "fixed", and the
changelog says so.

---

## R3 — The three fix shapes, and which item uses which

Every in-scope item reduces to one of four shapes. Naming them up front keeps
the fixes uniform and makes the reviewer's job mechanical.

| Shape | What it does | Items |
|---|---|---|
| **S1 — facade swap** | replace an ANSI file/shell API with the house facade or its W twin fed by `SalU8ToW`; keep the A call as the failure fallback | F-P1-19, F-P1-20, F-P1-21 (all groups), F-P1-06, F-P1-22, F-P1-23, F-P1-25, F-P1-26, F-P1-12, F-P1-13, F-P1-14, F-P1-27 |
| **S2 — producer at the source** | take the wide entry point (`…W`) at the point the value enters the application and convert once with `SalWToU8` (total, cannot fail) | F-P1-08, F-P1-10, F-P1-07, F-P1-24, F-P1-09/F-P4-05 |
| **S3 — `LoadStrU8` composition** | the translated half of a composed string becomes UTF-8 so the whole buffer is valid UTF-8 and the existing `Sal*U8` sink takes its wide path | F-P2-04, F-P2-07, F-P2-13, F-P4-02 (T1), F-P4-03, F-P4-07 |
| **S4 — UTF-8-by-contract at intake** | normalize a value of undefined encoding once where it enters (`SalLegacyToU8Alloc`), document the field, mirroring feature 052's plugin-metadata contract | F-P4-01 (conversion names), F-P4-03 (plugin-supplied titles) |
| **S5 — sink swap** | replace an ANSI UI sink with its `Sal*U8` twin (the twin already falls back to the A call on invalid input) | F-P2-09, F-P2-11, F-P3-07 (via `CopyToolTipAnswer`) |

`S1`, `S3` and `S5` are byte-identical for ASCII by construction (ASCII
`LoadStrU8 == LoadStr`; a W call on ASCII == the A call). `S2` and `S4` need
per-site argument: the same buffer may feed legacy-ANSI consumers that work
today (R5).

---

## R4 — Coupling groups (the unit of commit and of review)

The verdicts tie certain items together: fixing one alone would regress the
other. These groups are the commit granularity (FR-007) and each gets one
regression review (FR-006).

| # | Group | Items | Why they cannot be split |
|---|---|---|---|
| **C1** | Command line & drops | F-P6-04, F-P1-26 | same file and same sink (`editwnd.cpp` insert path); the drop handler feeds `InsertText` |
| **C2** | Cloud roots | F-P1-09, F-P4-05 | one defect in three producers (OneDrive personal + Business, Dropbox, Google Drive); the verdict refuted the "OneDrive-specific" framing |
| **C3** | Volume / subst / label / Drive Information | F-P1-12, F-P1-13, F-P1-14, F-P2-07 | `MyQueryDosDevice`/`MyGetVolumeInformation` output feeds the Drive Information type line, whose ANSI template renders those rows correctly *only while* they stay ACP — converting the producers without the template turns two currently-correct rows into mojibake |
| **C4** | External archivers | F-P1-05, F-P1-06, F-P1-07 | the pack (`CharToOem`) and list (`OemToCharBuff`) defects cancel today: fixing one direction alone breaks the working extract round trip |
| **C5** | Application locations | F-P1-08, F-P1-10, F-P2-13 | `ConfigurationName` and the help/`$(SalDir)` buffers have several producers; the Save Configuration prompt can only become `LoadStrU8` once its producer encoding is settled |
| **C6** | Shares | F-P1-27 | producer + the two consumers (per-directory marker, UNC mapping) in one commit; per-item path ⇒ timing |
| **C7** | Viewer | F-P4-01, F-P4-02 | the conversion-name intake removes F-P4-02's second trigger; the caption's first trigger is independent (`LoadStrU8`) |
| **C8** | Plugin-manager lists | F-P2-09, F-P2-11 | same dialog file, same sink swap, one review |
| **C9** | Configuration seeds | F-P4-03, F-P4-07 | same shape (`LoadStr` seed persisted into a UTF-8 field); the guard rule for it must land with them (R8) |
| **C10** | Singles | F-P1-19, F-P1-20, F-P1-21, F-P1-22, F-P1-23, F-P1-25, F-P2-04, F-P3-07, F-P6-01, F-P5-06 | independent; F-P1-21's nine site groups may be split further, one commit per group |
| **C11** | File comparator | D03, D04 | D04 overwrites the caption D03's sibling fix sets; the handoff says do them together |
| **C12** | Tooling | D01, D05 | independent of the product; land first (D01 unlocks the G5 Trace Server capture) |

---

## R5 — The regression protocol (and the three traps it exists to catch)

Adopted verbatim from `specs/068-encoding-regression-review/charters.md`
("Regression reviewer"), because it caught three regressions there that no
build, test or guard would have caught. Additions for this feature:

1. **Step 0 — still defective at HEAD?** (from R1). An item already fixed is
   recorded verify-closed, not re-fixed.
2. **Rework cap**: a REJECTED fix gets at most two rework rounds; after that it
   is deferred with the reviewer's reason (FR-001), never forced through.
3. **The three traps**, each of which must be answered explicitly in every fix
   record:
   - **DC-09** — a strict facade meeting a legacy producer. Both 068 rejections
     were this: converting one link while an adjacent producer still spoke the
     code page. Consequence for `S2` fixes: enumerate *every* consumer of the
     buffer and classify it (strict-UTF-8 / legacy-ANSI-and-working /
     tolerant-sink) before changing the producer.
   - **Blanking** — a fix that drops text on conversion failure. Forbidden
     (FR-004); the fallback is always the pre-fix narrow call.
   - **The load-bearing fallback** — the `Sal*U8` sinks and `CStaticText`
     render ACP bytes correctly *because* they fall back to the A call. A fix
     that makes a producer UTF-8 must not assume the sink was broken; and one
     that changes a sink must not assume the producer is UTF-8.

---

## R6 — What can be tested automatically, and what cannot

`src/vcxproj/saltests/saltests.vcxproj` compiles **only** `src/common/salclip.cpp`,
`salfileio.cpp`, `salpath.cpp`, `salunicode.cpp` and the test file itself. So:

| Testable as a `saltests` unit test | Not reachable from `saltests` |
|---|---|
| conversion helpers and any **new** helper this feature adds (put it in `src/common/`) | anything in `src/*.cpp` (UI, dialogs, panels, `codetbl.cpp`, `packers.cpp`, `drivelst.cpp`) |
| file-facade round trips on a real temp directory (the `TestWtf8FileOps` pattern, incl. accented and surrogate names) | the registry facade (`salamdr6.cpp`) — 068 ledger L75 |
| the OEM list-file round trip of C4, if the conversion is put in a helper in `src/common/` | `LoadStr`/`LoadStrU8` (needs the language module) |
| the UTF-8-boundary clamp of F-P3-07, if `CopyToolTipAnswer` is lifted out of its `static` in `gui.cpp` | window/message behaviour (C1's insert path) |

**Decisions**:
- **D-T1**: where a fix's logic is a pure conversion or clamp, extract it into
  `src/common/` (or make an existing static non-static) so it becomes unit
  testable. This is the only refactoring this feature permits, and only when it
  buys a test (FR-008). Concretely: `CopyToolTipAnswer` → declared in
  `src/gui.h`, used by `stswnd.cpp:1854` (F-P3-07); the OEM round trip of C4 →
  a named helper pair.
- **D-T2**: everything not reachable from `saltests` gets a
  `tools/check_encoding.py` rule where the defect has a grep-able shape (the
  guard already carries nine strict rules and four report-only ones), else a
  written manual scenario in `quickstart.md`. Every rule is proven by planting
  the pre-fix line back and watching it fire (FR-015).
- **D-T3**: the fail-before proof for unit tests is run mechanically —
  `git stash` → run → expect failures → `git stash pop` → run → expect 0 —
  and the counts are pasted into the fix record.

---

## R7 — Gates, environment and fixtures

Gates G1–G8 are in `data-model.md`; the procedure is in `quickstart.md`.
Environment facts established for this machine (2026-08-24):

- `D:\Zkouška\` **does not exist** — the 068 fixtures must be recreated
  (`tools\create-test-fixtures.ps1` covers only the `%TEMP%` set and `-Perf`).
  A `quickstart.md` script section creates all of FX-CS/HU/SUR.
- **No RAR** on this machine; **7-Zip is at `C:\Program Files\7-Zip\7z.exe`**
  (not on PATH). Decision: C4's manual scenarios use 7-Zip configured as a
  *custom* external packer/unpacker in Options ▸ Archivers (the same
  list-file/`$(ListFile)` code path the RAR defaults use). Where the archiver
  cannot be driven at all, the OEM round-trip helper's unit test (D-T1) is the
  fail-before/pass-after evidence and the manual scenario is recorded as
  maintainer work.
- An accented `%TEMP%` does **not** need a new Windows account: `set TEMP=…`
  in the launching shell reproduces every `%TEMP%`-gated item (FX-TEMP).
  Items gated on the *account name* (`%APPDATA%`, `%USERPROFILE%`) do need
  one, or are verified at the producer level plus a code-reading argument —
  recorded as such, not claimed as an on-screen pass.
- `pwsh` 7 is absent, so `normalize.ps1` cannot run: formatting is verified by
  matching the surrounding style and by byte-level BOM/EOL checks (as in 068).
- `build.cmd` requires Python on PATH (the guard is mandatory).

---

## R8 — Guard rules: what gets promoted, and when

`tools/check_encoding.py` has 9 strict rules (`TOTAL: 0` at baseline) and 4
report-only ones (183 findings), each blocked on a deferred fix:

| Draft rule | Blocked on | This feature |
|---|---|---|
| `ansi-api-on-utf8-path` | the S1 sweep | **cannot** be promoted: hundreds of legitimate consistent-ANSI chains remain (B-1/B-5). Keep report-only; record the reduced count as evidence of progress. |
| `cp-acp-utf8-source` | F-P1-25 + F-P1-16/18/28 (P7's note) | F-P1-25 lands here; F-P1-16/18/28 do **not** (not in section 1). **Not promotable** — say so in the closing record rather than leaving the reader guessing. |
| `signed-char-name-byte` | premise void (`/J`) | **remove** the rule (its replacement belongs to cluster B-2). One-line tooling change, FR-015. |
| `missed-twin` | the DC-18 sweep | stays report-only; this feature closes several twins, so the count drops — recorded. |
| **new** `acp-title-seed` | C9 | **add** a rule that flags `LoadStr(` seeding a field documented UTF-8 (packer titles, view names), promoted to strict in the same commit as C9, proven on the pre-fix line. |

---

## R9 — Explicitly out of scope (and why touching it would be a regression)

- **B-1 ANSI dialog windows** — includes the complete command-line fix (R2),
  Change Directory / Find "Look in" / Pack-Unpack / user-menu-editor input
  persistence. Converting a dialog needs storage *and* paint moved together
  plus `CKeyForwarderWindow`; 88 dialogs and the property-sheet pages.
- **B-2 code-page byte tables behind name comparison** (`LowerCase[]`,
  `StrICmp`, `IsTheSamePath` and ~20 sites it reaches).
- **B-3 `GetErrorText` UTF-8 but undocumented** (~27 plugin sites; a naive
  sweep would *regress* FTP, whose own error text is internally consistent
  ANSI). Consequence for this feature: **any composed message whose other half
  is `GetErrorText` is out of scope**, even when it sits next to an in-scope
  site. Each such neighbour is named in the task and left alone.
- **B-4 `AlterFileName`** (also drives Change Case, which renames on disk).
- **B-5 plugin-facing ANSI services** (FR-009 freeze). Consequence: F-P1-21
  group 1 (`src/zip.cpp` `ViewFileInPluginViewer` temp file) is in scope **only
  if** the analysis shows no byte a plugin receives changes; D02 likewise.
- The 8 refuted findings, the 4 latent ones, the 2 by-design and 2 withdrawn.
- Any refuted *half* of an in-scope finding (the list is in spec FR-002).

---

## R10 — Risk register and the resulting order

| Risk | Item | Mitigation |
|---|---|---|
| Highest — a producer change breaking a working legacy consumer (DC-09) | C5 (`GetModuleFileName`, `SHGetFolderPath`) | enumerate and classify every consumer first; prefer repairing at the strict consumer (`SalLegacyToU8Alloc`) over converting a buffer that legacy loaders share; land late, review hardest |
| High — two defects that cancel | C4 archivers | one commit, both directions; unit-test the round trip before touching the product path |
| High — currently-correct rows turning to mojibake | C3 Drive Information | template and producers in one commit |
| Medium — offsets/units | C1 | resolved by R2: the chosen shape does not move any offset |
| Medium — persisted values | C9 | tolerant compare / re-seed behaviour analysed before the change; no migration |
| Low | C2, C6, C7, C8, C10, C11, C12 | standard shapes with existing twins in the tree |

**Order** (each step = build + tests + guard, then the next):
`C12` (tooling; D01 unlocks the G5 capture) → `C10` singles by ascending risk
(F-P5-06 doc, F-P6-01, F-P3-07, F-P2-04, F-P1-22, F-P2-09/C8, F-P1-19,
F-P1-20, F-P1-23, F-P1-25, F-P1-21 group by group) → `C11` → `C7` → `C9` →
`C2` → `C6` → `C1` → `C3` → `C4` → `C5` → gates → maintainer sweep → closing
record.

Rationale: cheap, self-evident fixes first build the review rhythm and keep
the risky ones from being reviewed under time pressure; the two groups most
likely to be rejected (C4, C5) come last, when a deferral costs the feature
least.

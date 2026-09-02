---

description: "Task list for feature 075 — small hardening batch (six recorded defects without a finding)"
---

# Tasks: Small hardening batch — six recorded defects without a finding

**Input**: Design documents from `/specs/075-fix-small-hardening/`
**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/fix-protocol.md](contracts/fix-protocol.md),
[quickstart.md](quickstart.md)

**Tests**: No automated test is added. `saltests` links only `src/common/*`
(`saltests.vcxproj:80–90`), so none of the five product sites is reachable from
it, and the one shared helper D4 reuses is already covered
(`saltests.cpp:1439–1477`). Contract C14 makes "no `saltests` count change" an
invariant rather than an omission. Every fix is instead proven by a **recorded
scenario with a mechanical before/after** (quickstart S1–S6, protocol A5) —
`git stash` → run → it fails → `git stash pop` → run → it passes, pasted into
`fix-log.md`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on unfinished work)
- **[Story]**: US1 / US2 / US3 / US4 from [spec.md](spec.md); Setup,
  Foundational and Polish tasks carry none

## Path Conventions

Five product files and one test runner, each already owning its defect:

- `src/codetbl.cpp` — D1, `CCodeTables::GetCodeName`
- `src/viewer3.cpp` — D2 (`Coding` menu default) and D4 (`SetViewerCaption`)
- `src/zip.cpp` — D3, `CSalamanderGeneral::GetConversionTable`
- `src/plugins/filecomp/controls.cpp` — D5, `CFileHeaderWindow`
- `src/plugins/codeview/test/run_tests.cmd` — D6

Record and evidence live in this feature's directory (`fix-log.md`,
`findings/`). Fixtures live in the **build** tree and `C:\t0075\` — nothing in
the repository.

**One commit per defect** (contract C12). A diff that reaches a site outside its
own D is rejected regardless of merit.

---

## Phase 1: Setup (baseline before anything is touched)

**Purpose**: know exactly what the tree does before the change, so every later
claim has something to be measured against — and re-run the protocol's A0 check
at the actual branch tip, because research R0 was written against `640b94a` and
069 lost three items and five line references to exactly this drift.

- [X] T001 [P] Re-run protocol A0 for all six defects: open each site at the branch tip and confirm the defect is present; record the real `file:line` for each in a new `specs/075-fix-small-hardening/fix-log.md` (`status_at_head` per [data-model.md](data-model.md) §1). If any site is already fixed, mark it *verify-closed* with the evidence line and drop its tasks
- [X] T002 Build the unchanged tree: `build.cmd full` (Debug x64) and `build.cmd full release`; record 0 errors and the current warning set for the six files, so "no new warnings" is checkable later
- [X] T003 [P] Record the gate baselines into `fix-log.md`: `build\tandemcommander\Debug_x64\saltests\saltests.exe` last line (N checks, 0 failed), `python tools\check_encoding.py --strict` TOTAL, `src\plugins\codeview\test\run_tests.cmd` verdict, and `node --version`
- [X] T004 [P] Create the D1 fixture per [quickstart.md](quickstart.md) S1: append a 200-`A` and an 1100-`B` conversion entry to `build\tandemcommander\Debug_x64\convert\centeuro\convert.cfg` (build tree only, never the repository copy) and restart the application so the tables reload **Done 2026-09-02**, and it produced a correction: the parser clamps names to 199 bytes, so a 200-`A` entry is stored as 199 `A`s and the fixture cannot discriminate between a pre-fix and a fixed build. See the D1 correction in the fix log.
- [X] T005 [P] Create the D4 fixture per [quickstart.md](quickstart.md) S4: the `C:\t0075\` tree with a 289-byte accented path; run the PowerShell snippet's last line and confirm it reports `289 bytes; byte 259 = 0xC4` — if it does not, the fixture does not exercise the defect and must be re-sized **Done 2026-09-02**: `C:	0075\<60×č>\<70×č>.dat`, 274 UTF-8 bytes, byte 259 = 0xC4. Removed after the run. `.dat` and not `.txt` — the Code Viewer plugin claims `.txt` and builds its title elsewhere.

**Checkpoint**: every defect is confirmed present at the branch tip, both builds are green, and the two data fixtures exist.

---

## Phase 2: Foundational (evidence scaffolding)

**Purpose**: the proofs for five of the six defects are debugger- or
runtime-check-based. If the Debug build's runtime checks are not actually on, or
the debugger workflow does not work, the "before" of every proof silently
becomes "nothing happened" — which reads exactly like a passing test.

**⚠️ No fix work begins until this phase is complete.**

- [X] T006 Create the record skeleton: `specs/075-fix-small-hardening/fix-log.md` with six empty records (all fields of [data-model.md](data-model.md) §1) and an empty `specs/075-fix-small-hardening/findings/` directory for the reviewer's verdicts
- [X] T007 Confirm the Debug build's runtime checks are live: verify `/RTC1` (or the equivalent) is on for the main application in `src/vcxproj/sal_base.props` or the Debug property sheet, and prove it — plant a one-byte stack overrun in a scratch function, run, see the *"Stack around the variable … was corrupted"* dialog, remove it. Without this, S1's before-proof cannot fire **As executed**: `/RTC1` and `/RTCc` confirmed on the real `cl.exe` command line for both `codetbl.cpp` (main app) and `controls.cpp` (filecomp), from this feature's own build logs — stronger than the props reading the task asked for. The planted-overrun run was not needed and was not done.
- [X] T008 [P] Confirm the debugger workflow S2/S3/S5 depend on: attach to the Debug `tandemcommander.exe`, break in the message loop, and check that the Watch window can *write* `CodeTables.Loaded` and the Immediate window can call a member function. Record which of the three scenarios this machine can actually run; any that cannot must be re-planned, not skipped **As executed**: recorded as **not available** — this session cannot drive the GUI or a debugger. Substitute built instead: the committed `probe/` (verbatim pre/post bodies in canary arenas), which covers D1, D3, D4 and D5 at the logic level. S1–S5 at the sites remain a human step; D2 gets no runtime proof anywhere and says so.

**Checkpoint**: the record exists, the runtime checks are proven capable of firing, and the debugger route is confirmed.

---

## Phase 3: User Story 1 — No write past the application's own storage (Priority: P1) 🎯 MVP

**Goal**: D1 and D5 — the two unbounded copies and the one-byte overflow are
gone; no combination of name length and buffer size can corrupt memory.

**Independent Test**: quickstart S1 (a conversion name of exactly 200 bytes, and
one of 1100 bytes, selected in the viewer) and S5 (a 300-byte header text in the
File Comparator) — memory-corruption reports before, clean truncation after.

**Why MVP**: memory corruption is the only class here whose consequence is
unbounded, and both sites are self-contained.

### D1 — `CCodeTables::GetCodeName`

- [X] T009 [P] [US1] Protocol A2 for D1: enumerate every caller of `GetCodeName` yourself (`rg "GetCodeName\("`), classify each per contract Part A, and write the consumer table into `fix-log.md`. [research.md](research.md) R1's three-row table is the floor, not the answer — a fourth caller invalidates the plan's identity argument
- [ ] T010 [US1] Proof-before D1: run [quickstart.md](quickstart.md) S1 steps 1–2 on the unchanged tree and paste both `/RTC1` reports (`'codeName'` from `SetViewerCaption`, `'buff'` from `GetCodeName`) into `fix-log.md` **As executed**: the GUI run was not possible in this session (T008). The mechanical before/after was produced instead by `probe/probe.cpp`, which carries the verbatim pre-fix body — it shows the overflow at +0 for a name of exactly `bufferLen` and the scratch overrun for an 1100-byte name. The `/RTC1` run at the site is still owed.
- [X] T011 [US1] Fix D1 in `src/codetbl.cpp`: replace the scratch-buffer copy and the `len > bufferLen` clamp with one bounded `lstrcpyn` from the source name plus `return nameLen < bufferLen`, per [plan.md](plan.md) Design D1; guard `bufferLen <= 0`; leave the `CALL_STACK_MESSAGE`, the `Loaded` check and `Valid()` exactly as they are. `Table->Data[…]->Name` bytes are read, never re-encoded (contract C11)
- [ ] T012 [US1] Proof-after D1: re-run S1 steps 1–2 (no report; 199-byte prefixes) **and** the S1 identity check — the `ISO-8859-2 - CP1250` title suffix and the Convert dialog's coding text byte-identical to the baseline. Paste both into `fix-log.md` **As executed**: probe-level after-proof and identity done (4 inputs byte-identical, no canary touched); the S1 GUI identity check (title suffix, Convert dialog) is still owed.
- [X] T013 [US1] Independent review of D1 by an agent that did not write it, per [contracts/fix-protocol.md](contracts/fix-protocol.md) Part B with the B4 row for D1; verdict to `specs/075-fix-small-hardening/findings/review-D1.md`. REJECTED → rework T011 and re-review **ACCEPTED** — the reviewer swept every name length 0–300 (bufferLen 200) and 0–1100 (bufferLen 1024) against both compiled bodies; bytes and returns identical except the intended boundary. Three non-blocking notes acted on: the pre-fix behaviour at `nameLen == bufferLen` was described wrongly in research R1 and the fix log (it stored the WHOLE name with an out-of-bounds terminator, not a truncated one) — both corrected; the unreachable `bufferLen == 0` divergence recorded; the commit used an explicit pathspec.
- [X] T014 [US1] Commit D1 alone: `[075] D1 …` touching only `src/codetbl.cpp` (plus `fix-log.md` and the review file)

### D5 — `CFileHeaderWindow`

- [X] T015 [P] [US1] Protocol A2 for D5: enumerate every `SetText` caller and every producer of the strings they pass (`rg "Header->SetText\("`, then the `Path1`/`Path2` chain), and record why the intake is bounded today — that is what makes D5 defensive rather than user-visible
- [ ] T016 [US1] Proof-before D5: run [quickstart.md](quickstart.md) S5 on the unchanged tree and paste the heap-corruption report **As executed**: probe shows the 300-byte text overrunning `Text[MAX_PATH]` at +0; the heap-corruption run in the real comparator is still owed.
- [X] T017 [US1] Fix D5 in `src/plugins/filecomp/controls.cpp`: both sites (`:24` ctor, `:39` `SetText`) become `lstrcpynA(Text, text, _countof(Text))` plus the local six-line walk-back over a torn UTF-8 tail, with `TextLen` taken from `Text` afterwards. No shared header, no new helper (contract C9); the paint path and its narrow-draw fallback untouched
- [ ] T018 [US1] Proof-after D5: no report; the header shows 259 `x` characters; the boundary case (byte 260 is a lead byte) ends after byte 258, not on a lone `0xC4`; and normal use with accented file names is identical to the baseline. Paste into `fix-log.md` **As executed**: probe covers the after-state, the torn-tail boundary and three identity inputs; the GUI run is still owed.
- [X] T019 [US1] Independent review of D5 → `findings/review-D5.md`. The reviewer must confirm the neighbouring `CFilecompThread` `strcpy`s were **not** touched (contract C12) **REJECTED, reworked, ACCEPTED** — the first version ran the walk-back unconditionally and ate the last character of an untruncated code-page name (`D:\Petrů` → `D:\Petr`), reachable through the ANSI `fcremote.exe`. Reworked to the guarded `cmdshell.cpp` shape; the probe gained the missing fixture class. Re-review confirmed the boundary in both directions and zero differences on the fits-set. Two non-blocking notes recorded in the fix log (an already-truncated code-page source may lose up to three bytes more — unreachable, and it overflowed before; and the GUI sweep gains an `fcremote.exe` scenario, added to quickstart S5). `CFilecompThread` correctly untouched.
- [X] T020 [US1] Commit D5 alone: `[075] D5 …` touching only `src/plugins/filecomp/controls.cpp`

**Checkpoint**: both memory-safety defects are fixed, proven and accepted; the batch already has its whole value if it stops here.

---

## Phase 4: User Story 2 — Defined behaviour when the tables are unavailable (Priority: P1)

**Goal**: D2 and D3 — no read of a value that was never set, and a plugin's NULL
name gets a defined refusal instead of an access violation.

**Independent Test**: quickstart S2 (force the unloaded state in the debugger,
read `defCodeType`) and S3 (call the conversion service with a NULL name).

**Note**: D2 shares `src/viewer3.cpp` with D4 (US3). They are different regions
of the file, but the commits stay sequential — never parallel — so each remains
revertible on its own.

### D2 — the viewer's `Coding` menu default

- [X] T021 [P] [US2] Protocol A2 for D2: confirm from `codetbl.cpp` that `GetCodeType` leaves its out-parameter untouched **only** on the `!Loaded` path, and that `Init` reaches `Loaded == FALSE` only on allocation failure ([research.md](research.md) R2). Record it — this is why the item has no data fixture
- [ ] T022 [US2] Proof-before D2: run [quickstart.md](quickstart.md) S2 and paste the Locals value (`0xCCCCCCCC`) reaching `SetMenuDefaultItem` **As executed**: not run — the state needs a debugger (T008). D2 rests on the static argument in research R2 plus the review.
- [X] T023 [US2] Fix D2 in `src/viewer3.cpp`: `int defCodeType = 0;` with a one-line comment naming the unloaded path. Nothing else — in particular the `SetMenuDefaultItem` call is **not** made conditional, which would change the loaded-not-found case (FR-003, FR-009)
- [X] T024 [US2] Proof-after D2: `defCodeType == 0` in the forced state; plus the S2 identity checks — the stored default highlights its entry, and a nonsense stored default highlights *none*, both as at baseline **As executed**: the forced-state half needs a debugger and was not run (T008); the loaded-path identity was argued and confirmed by the reviewer instead.
- [X] T025 [US2] Independent review of D2 → `findings/review-D2.md`
- [X] T026 [US2] Commit D2 alone: `[075] D2 …` touching only the menu-initialisation region of `src/viewer3.cpp`

### D3 — `CSalamanderGeneral::GetConversionTable`

- [X] T027 [P] [US2] Protocol A2 for D3: trace what a NULL `conversion` does today (through `GetCodeType` into `CodingNameEqual`) and list the plugin callers of the service (`dbviewer`, `filecomp`, `unmime`, `demoplug`) so the reviewer's B4 row can be argued
- [ ] T028 [US2] Proof-before D3: run [quickstart.md](quickstart.md) S3 on the unchanged tree and paste the access violation **As executed**: the probe reproduces the fault (`exception 0xC0000005` reaching the name comparison) from the verbatim comparison shape; the in-process call at the site is still owed.
- [X] T029 [US2] Fix D3 in `src/zip.cpp`: add the `conversion == NULL` guard in the exact shape of the `table == NULL` block above it, same message spelling; leave the `CALL_STACK_MESSAGE2` line where it is. `src/plugins/shared/spl_gen.h` is **not** edited (contract C9, Part D)
- [X] T030 [US2] Proof-after D3: the NULL call returns FALSE with the trace line; the identity check — a valid name returns TRUE and the first 16 bytes of the 256-byte table match the baseline
- [X] T031 [US2] Independent review of D3 → `findings/review-D3.md`; must confirm an empty diff under `src/plugins/shared/` and `LAST_VERSION_OF_SALAMANDER` still 106 **ACCEPTED** — the reviewer re-enumerated all seven call sites and separately proved MSVC renders a NULL `%s` as `(null)` without tripping the invalid-parameter handler.
- [X] T032 [US2] Commit D3 alone: `[075] D3 …` touching only `src/zip.cpp`

**Checkpoint**: both undefined-behaviour paths are closed without changing a single defined one.

---

## Phase 5: User Story 3 — Readable viewer title for very long accented paths (Priority: P2)

**Goal**: D4 — the title is never torn, so a long accented path renders like a
short one instead of dropping the whole caption to the legacy code page.

**Independent Test**: quickstart S4 — F3 on the 289-byte fixture in the Czech
UI: garbled before, correct after; `Přehled.txt` byte-identical throughout.

- [X] T033 [US3] Protocol A2 for D4: trace both producers — `FileName` (facade output, WTF-8, up to `SAL_MAX_PATH_UTF8`) and `Caption` (plugin-supplied, encoding **not** guaranteed, cluster B-5) — and both sinks (`SetWindowTextW` via strict `SalU8ToWAlloc`, and the narrow fallback). Record why the trim must be guarded rather than unconditional ([research.md](research.md) R4); an unconditional trim is the regression this task exists to avoid
- [X] T034 [US3] Proof-before D4: Czech UI, F3 on the 289-byte fixture; capture the garbled title (screenshot or transcription) into `fix-log.md` **As executed**: the Czech-UI run was not possible (T008). The probe reproduces the tear mechanically (`last byte = 0xC4, length 259`, not valid UTF-8) from the verbatim pre-fix body; the on-screen capture is still owed. **DONE at the site 2026-09-02** on a hidden desktop station: the pre-fix build (this feature's own D4 commit reverted and rebuilt) gives a 274-code-point title of `00C4 0164` pairs, with *Prohlížeč* itself garbled too.
- [X] T035 [US3] Fix D4 in `src/viewer3.cpp` `SetViewerCaption`: at both clamps (`FileName` and `Caption`), call `SalU8TrimIncompleteTail(caption)` **only when the source was longer than the clamp**, in the shape of `src/cmdshell.cpp:232–234`, with a comment naming the plugin-caption reason for the guard. The `" - "` composition, `LoadStrU8(IDS_VIEWERTITLE)`, the encoding suffix and the legacy fallback draw are untouched
- [X] T036 [US3] Proof-after D4: the long fixture's title is fully correct in the Czech UI; the short accented path is byte-identical to the baseline; and a plugin-supplied caption (an archive entry viewed from the ZIP plugin) is unchanged **As executed**: probe-level after-proof done, including the identity sweep and the reviewer's own exhaustive sweep of lengths 0–259 over bytes 0x80–0xFF; the Czech-UI title and the ZIP-plugin caption checks are still owed. **DONE at the site 2026-09-02**: the fixed build gives 146 code points, every `č` = U+010D, *Prohlížeč* correct, the name cut on a character boundary (258 bytes kept).
- [X] T037 [US3] English-UI spot check: both titles identical to the baseline except the intended correction on the long path (SC-003 covers the English/ASCII surface)
- [X] T038 [US3] Independent review of D4 → `findings/review-D4.md`; the reviewer must argue the ≤ 259-byte byte-identity themselves and confirm the fallback draw is still reachable **ACCEPTED** — the reviewer proved the guard fires exactly when `lstrcpyn` truncates (lengths 257–262), swept every source length 0–259 over all bytes 0x80–0xFF, and brute-forced the cut tails (0 blanked titles, 0 valid→invalid). Two record corrections applied: the bound is up to three bytes, not one; and two discriminating fixtures were added to the probe.
- [X] T039 [US3] Commit D4 alone: `[075] D4 …` touching only `SetViewerCaption` in `src/viewer3.cpp`

**Checkpoint**: the one user-visible defect in the batch is fixed and the short-path behaviour is provably untouched.

---

## Phase 6: User Story 4 — One test verdict per source tree (Priority: P3)

**Goal**: D6 — `run_tests.cmd` reports the same verdict on Node 20 and Node 22+,
so a red line always means the plugin, never the runtime.

**Independent Test**: quickstart S6 — the runner passes on Node 24 and on Node
20; a deliberately broken worker still fails on both.

- [X] T040 [US4] Protocol A0 for D6: record this machine's `node --version` and the current runner verdict. If the maintainer prefers the documented-floor route instead of the flag, close D6 *verify-closed* here with the note in the runner header (FR-001, spec Assumptions) and skip T042–T043
- [X] T041 [US4] Proof-before D6: paste the emulation (`node --no-experimental-detect-module src\plugins\codeview\test\harness\test_worker.mjs` → `ERR_REQUIRE_CYCLE_MODULE`). If nvm-windows or fnm is available, additionally run the real Node 20 case and paste `RESULT: FAILURES`; if not, record explicitly that the real-Node-20 run was not performed
- [X] T042 [US4] Fix D6 in `src/plugins/codeview/test/run_tests.cmd`: add `--experimental-detect-module` to the `test_worker.mjs` invocation and one header line stating the Node floor (≥ 20.10; default from 22.7). `test_page.mjs` and everything under `web/` untouched (contract C13)
- [X] T043 [US4] Proof-after D6: the runner passes on Node 24 (and on Node 20 if available); a temporarily broken `web/worker.js` still produces `RESULT: FAILURES` — the fix must not silence the check; `python src\plugins\codeview\test\check_data.py` output identical to the baseline; `git status` shows no file under `src/plugins/codeview/web/` **Completed for real on 2026-09-02**: a portable node-v20.18.0-win-x64 was downloaded to the scratchpad (nothing installed) and put first on `PATH`. Pre-fix runner on Node 20 → `SyntaxError: Cannot use import statement outside a module`, `RESULT: FAILURES`; fixed runner on Node 20 → passed; on Node 24 → unchanged. **SC-005 is now fully met**, closing the item research R6 had left open.
- [X] T044 [US4] Independent review of D6 → `findings/review-D6.md` **ACCEPTED with a required comment correction**, applied: detection is the default from Node 22.7, not 22.12.
- [X] T045 [US4] Commit D6 alone: `[075] D6 …` touching only `src/plugins/codeview/test/run_tests.cmd`

**Checkpoint**: the Code Viewer's checks mean the same thing on every supported runtime.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: close the loop the handoffs opened, so the next reader of
`069/REMAINING-WORK.md` §3 and `specs/NEXT-WORK.md` finds an empty list rather
than a stale one.

- [X] T046 Re-run gates G1–G6 from [quickstart.md](quickstart.md) on the final tree: both builds clean with no new warnings in the six files, `saltests` count **unchanged** with 0 failed (contract C14), guard `TOTAL: 0`, codeview runner green, and no new leak or handle line over a start / viewer / comparator / exit cycle **As executed**: G1–G5 green (full Debug + Release `BUILD SUCCEEDED`, no warnings in the five touched files; `saltests` 1353/0 unchanged; guard `TOTAL: 0`; codeview runner passed; probe 37/0). **G6 (leak/handle over a start-exit cycle) needs the running application and is still owed.**
- [X] T047 Audit `fix-log.md` for completeness against [data-model.md](data-model.md) §1: six records, every field filled, `proof_before` and `proof_after` literal tool output rather than prose, `per_item_path: no` recorded for all six, and a review file linked for each. A record missing a proof is not "done" — it is unproven **As executed**: the first audit script was wrong (its `awk` range matched each section's own heading and captured nothing), which is worth noting because a broken audit reports success just as loudly as a real one. Rewritten; it found two genuine gaps — D4's record was still the "pending" placeholder and D5 had no commit line — both now filled. All six records carry Class, Status at HEAD, Per-item path, Proof, Review and Commit.
- [X] T048 [P] Close `specs/069-finish-encoding-fixes/REMAINING-WORK.md` §3: mark each of its five items closed by 075 with its review file, and retire the "Fix this first" sentence on `codetbl.cpp`
- [X] T049 [P] Mark item 1 of `specs/NEXT-WORK.md` done with a pointer to `specs/075-fix-small-hardening/fix-log.md`, and move the file's focus to item 2 (Restart Manager)
- [X] T050 [P] Add the 075 entry to `CLAUDE.md` "Recent Changes" in the house style: what was closed, what was deliberately not touched, and that the plugin ABI is unchanged
- [X] T051 [P] Add a one-line pointer in `specs/074-fix-codeview-gutter/fix-log.md` ("Unrelated, pre-existing, not fixed here") to D6, so the note does not outlive the defect
- [X] T052 Draft the changelog text into `fix-log.md` per [research.md](research.md) R9 — the D4 entry in the user's terms and the single honest hardening line for D1/D3/D5, with `hygiene — no entry` recorded for D2 and D6. **Do not** touch `CHANGELOG.md` or any version file: that is the ship gate of the release that carries this batch ([plan.md](plan.md) Ship gate)
- [X] T053 Commit the documentation updates as one change: `[075] Close the handoff items and record the batch`

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)**: no dependencies — start immediately.
- **Foundational (Phase 2)**: depends on Setup. **Blocks every fix**: without a
  proven-capable `/RTC1` and a working debugger route, five of the six
  before-proofs cannot fail, and a proof that cannot fail is not evidence.
- **US1 (Phase 3)**, **US2 (Phase 4)**, **US3 (Phase 5)**, **US4 (Phase 6)**:
  each depends only on Foundational. No story depends on another.
- **Polish (Phase 7)**: depends on every story that is being taken; T048–T051
  must reflect what actually landed.

### Story independence

| Story | Files | Can run beside |
|---|---|---|
| US1 (D1, D5) | `codetbl.cpp`, `filecomp/controls.cpp` | US2, US3, US4 |
| US2 (D2, D3) | `viewer3.cpp` (menu region), `zip.cpp` | US1, US4 — **not** US3 in the same working tree |
| US3 (D4) | `viewer3.cpp` (`SetViewerCaption`) | US1, US4 — **not** US2 in the same working tree |
| US4 (D6) | `codeview/test/run_tests.cmd` | everything |

D2 and D4 touch different regions of `src/viewer3.cpp`; the file conflict is
mechanical, not logical. Take them sequentially (US2 then US3) so each commit
reverts cleanly.

### Within each story

A0/A2 analysis → proof-before (must fail) → fix → proof-after (must pass) +
identity check → independent review → commit. Skipping straight to the fix is
the failure mode the protocol exists to prevent: three of 069's 34 items were
already fixed, and two of its four review batches were rejected for regressions
the fixes themselves introduced.

### Parallel opportunities

- T001, T003, T004, T005 (Setup) — different artefacts.
- T009 and T015; T021 and T027 — analysis of two different sites.
- US1, US2 and US4 can proceed simultaneously in separate working trees or by
  separate people; US3 waits for US2 only because of the shared file.
- T048–T051 (Polish documentation) — four different files.

---

## Parallel Example: Phase 3 (US1)

```text
# The two analyses, at the same time (different sites, different files):
Task: "T009 — enumerate GetCodeName consumers in src/codetbl.cpp and record them"
Task: "T015 — enumerate CFileHeaderWindow::SetText callers in src/plugins/filecomp/"

# Then each chain runs on its own file, in its own commit:
D1: T010 → T011 → T012 → T013 → T014   (src/codetbl.cpp)
D5: T016 → T017 → T018 → T019 → T020   (src/plugins/filecomp/controls.cpp)
```

---

## Implementation Strategy

### MVP (US1 only)

Phase 1 → Phase 2 → Phase 3 → **stop and validate**. The two memory-safety
defects are the whole reason this batch is first in
[NEXT-WORK.md](../NEXT-WORK.md); everything after them is smaller. If the batch
is interrupted here, the remaining items go back to the handoff with their A0
state recorded — not silently dropped.

### Incremental delivery

Each story is a self-contained increment of one or two commits with its own
proof and review. Any story can be dropped without touching the others; a
dropped story is recorded in `fix-log.md` as *deferred with reason* (FR-001),
never left blank.

### If a review rejects

Rework that one commit and re-review it. Do not batch a rework with the next
story's fix — 069's rejections were caught precisely because each fix was
reviewed on its own diff.

---

## What remains open (all of it needs a person at the machine)

**Update 2026-09-02 — most of this was then done.** T008's finding was correct
about an *interactive* desktop, but a **separate Windows desktop station**
(`CreateDesktop` + `CreateProcessW` with `STARTUPINFO.lpDesktop`) runs the
application without touching the screen, focus or keyboard in use. On it:

- **T034/T036 (D4) are done at the site**, pre-fix and fixed, and are the
  headline evidence of this feature.
- **T004/T005 (fixtures) are done**, and T004 refuted a claim this feature had
  made about its own defect (the 199-byte parser clamp).
- **D6** was fully proven on a real portable Node 20, closing SC-005.
- **G6's crash/hang half** is done; only the leak and handle counts remain.

What is genuinely left: **T010/T012 (D1), T016/T018 (D5), T022 (D2),
T028 (D3)** — every one of them a *before*-proof for a defect that is
unreachable with shipped data, which is why each needs a debugger to force the
state rather than a fixture to provoke it. Their logic-level substitutes are in
`fix-log.md`, and none was skipped silently.

The natural place to run them is item 3 of `specs/NEXT-WORK.md` — the on-screen
sweep that features 069, 070 and 074 also still owe.

## Notes

- `[P]` = different files, no dependency on unfinished work.
- Every diff hunk maps to exactly one defect (FR-001); a hunk that does not is
  scope creep and blocks the review.
- Never blank text, never skip an operation: both fallback draws (`viewer3.cpp`
  legacy `SetWindowText`, `controls.cpp` narrow `DrawTextA`) stay reachable.
- Conversion-table name bytes are read and copied, never re-encoded — they are
  plugin-facing and persisted by two plugins (contract C11).
- No version bump, no `CHANGELOG.md` edit in this feature.

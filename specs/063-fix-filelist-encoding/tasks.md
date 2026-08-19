# Tasks: Make File List — Correct Encoding and Dialog Layout

**Input**: Design documents from `/specs/063-fix-filelist-encoding/`
**Prerequisites**: plan.md, spec.md, research.md (decisions D1–D7), contracts/filelist-text-encoding.md (C1–C5), quickstart.md

**Tests**: No automated UI test infrastructure exists for this surface (spec Assumptions); verification is the manual scenario set in quickstart.md plus the existing `saltests` suite, the `check_encoding.py` guard, and the layout gates. No new test tasks are generated; verification tasks are explicit instead.

**Organization**: Tasks grouped by user story (US1 clipboard P1, US2 viewer+file P2, US3 hint P3, US4 label P3). The D1 same-defect caller sweep and contract enforcement are cross-cutting (Polish phase).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task)
- **[Story]**: US1–US4 per spec.md
- All line numbers refer to commit `15f70c6` (pre-fix state); re-locate by content if drifted.

## Phase 1: Setup (baselines & fixtures)

**Purpose**: capture pre-fix evidence needed by the regression guards before any source change.

- [X] T001 Capture pre-fix baseline with the current build: produce an ASCII-only Make File List to a file and via clipboard-paste, save both byte-exact into `specs/063-fix-filelist-encoding/baseline/` (feeds SC-004/FR-008 byte-diff in T033); note/screenshot the three Czech-UI defects (garbled paste, garbled hint, clipped "Soubor") for the record
- [X] T002 [P] Create fixture script `specs/063-fix-filelist-encoding/fixtures/make_fixture.ps1` that creates the test folder from quickstart.md Prerequisites (Czech-diacritic names incl. full `ěščřžýáíéúůďťň` set, one Greek+CJK name, several ASCII names)

---

## Phase 2: Foundational (shared helpers — BLOCK all stories)

**Purpose**: the two shared primitives every story leg builds on.

- [X] T003 [P] Promote file-static UTF-8 helpers `NextUTF8Char`/`CountUTF8Chars` from `src/fileswn0.cpp:37-57` into `src/common/salunicode.h` + `src/common/salunicode.cpp` as `SalU8Next`/`SalU8CharCount` (bodies verbatim); re-point `src/fileswn0.cpp` to the shared versions and delete the statics (research D6.1)
- [X] T004 [P] Implement `CopyTextToClipboardU8(const char* u8Text, int byteLen = -1, BOOL showEcho = FALSE, HWND hEchoParent = NULL)` in `src/salamdr4.cpp` (declare in `src/consts.h` next to lines 718–720): strict UTF-8→UTF-16 probe with CP_ACP fallback (SalLegacyToU8Alloc tolerance model), delegate to `CopyHTextToClipboardW`; document contract C2 at the declaration (research D1)
- [X] T005 Compile gate: `build.cmd` (Debug x64 incremental) passes with T003+T004 in place

**Checkpoint**: foundation ready — user stories can start (US1/US2 touch `src/mainwnd4.cpp`, so run those two phases sequentially or coordinate edits).

---

## Phase 3: User Story 1 — Correct file list on the clipboard (Priority: P1) 🎯 MVP

**Goal**: Ctrl+M → Clipboard pastes every name (Czech diacritics, non-CP1250 chars) character-for-character; `:N`/`:max` columns align; works under non-ASCII `%TEMP%`.

**Independent Test**: quickstart Scenario 1 (paste fidelity incl. Greek/CJK), Scenario 4 (alignment + boundary-safe truncation), Scenario 6 (non-ASCII %TEMP%), plus ASCII paste unchanged.

### Implementation for User Story 1

- [X] T006 [US1] Route the Make File List clipboard leg through the U8 entry point: `src/mainwnd4.cpp:315` `CopyTextToClipboard(buff, fileSize, …)` → `CopyTextToClipboardU8(buff, fileSize, …)` (research D1, caller #1)
- [X] T007 [P] [US1] Fix the ANSI temp-path hole in `SalGetTempFileName`: `src/salamdr3.cpp:222` `GetTempPath` and `:232` `GetSystemDirectory` → W variants + `SalWToU8` (pattern: `src/common/trace.cpp:441`), so the UTF-8-consuming `SalCreateFile` at `:270` gets valid UTF-8 (research D5)
- [X] T008 [US1] Character-based width semantics in `DoExpandVarString` (`src/salamdr2.cpp`): at `:1011` keep byte `len`, add `width = SalIsASCII(value,len) ? len : SalU8CharCount(value,len)`; store/compare `width` in `maxVarWidths` (`:1014-1018`); in the emit block (`:1025-1041`) pad `valueOutLen - width` spaces after all `len` bytes, truncate by walking `valueOutLen` chars with `SalU8Next` (never split a sequence), keep `varPlacements` `MAKELPARAM(byteOffset, byteLen)` byte-based for the info-line consumers (`src/fileswn2.cpp:1184-1208`) (research D6, contract C4)
- [ ] T009 [US1] Verify US1: run quickstart Scenarios 1, 4, 6 on the T002 fixture; confirm ASCII-only paste is byte-identical to the T001 baseline

**Checkpoint**: primary reported defect fixed and independently demonstrable — MVP.

---

## Phase 4: User Story 2 — Correct list at viewer and file destinations (Priority: P2)

**Goal**: the same list renders correctly in the internal viewer (even when the first 10,000 bytes are ASCII) and is saved to a file whose **name and content** are correct for non-ASCII input.

**Independent Test**: quickstart Scenario 2 (viewer incl. ASCII-prefix case; file named `seznam-příloh.txt` created with the correct on-disk name; append round-trip), Scenario 3 (file byte-diff for ASCII).

### Implementation for User Story 2

- [X] T010 [US2] Write the UTF-8 BOM (EF BB BF) at the head of the temp file **only when `Configuration.FileListDestination == 1` (viewer)** in `src/mainwnd4.cpp` (after `CreateFile`/`SetFilePointer`, before `panel->MakeFileList(hFile)` at `:296`); never for clipboard (bytes are read back at `:305-315`) or file destination (research D3, contract C1)
- [X] T011 [US2] Convert the file-leg file-system calls to the UTF-8-aware house wrappers in `src/mainwnd4.cpp`: `HANDLES_Q(CreateFile(…))` at `:284` → `SalCreateFile` (`src/common/salfileio.h:42`), `DeleteFile(fileName)` at `:332` → `SalDeleteFile` (`salfileio.h:74`) — fixes the garbled on-disk name for non-ASCII list-file names (research D4, contract C5). Note: T010+T011 edit the same region of `src/mainwnd4.cpp` as T006 — apply within one coordinated edit session, not in parallel with Phase 3
- [ ] T012 [US2] Verify US2: quickstart Scenario 2 end-to-end (incl. a >10,000-byte list with all-ASCII head and accented tail in the viewer) and Scenario 3 file byte-diff against the T001 baseline

**Checkpoint**: all three destinations consistent (SC-002).

---

## Phase 5: User Story 3 — Readable line-format help in the dialog (Priority: P3)

**Goal**: the hint tooltip renders correct diacritics in every shipped language — fixed at the shared mechanism, covering all hint/tooltip producers incl. plugins.

**Independent Test**: quickstart Scenario 5 steps 1–2 (Czech Ctrl+M hint), plus a masks-hint dialog (e.g. Configuration → file masks) and one plugin hint (checksum) as mechanism spot-checks.

### Implementation for User Story 3

- [X] T013 [US3] Normalize tooltip text at intake to UTF-8-by-contract: `CStaticText::SetToolTipText` (`src/gui.cpp:873-902`) and `CButton::SetToolTipText` (`src/gui.cpp:1965-1988`) store `SalLegacyToU8Alloc(text)` instead of `DupStr(text)`; keep the `strcmp` early-out at `gui.cpp:875` comparing against the normalized form; document `ToolTipText` members as UTF-8 (research D2.1, contract C3)
- [X] T014 [P] [US3] Tolerant always-wide tooltip renderer: `CToolTip::GetText` (`src/tooltip.cpp:298-319`) — strict UTF-8 probe, on failure convert CP_ACP→UTF-16 so `TextLenW` is always valid; collapse the measure/paint branches at `tooltip.cpp:330-333` and `:650-653` to unconditional `DrawTextW`; do **not** use `SalU8ToWDisplay` (U+FFFD substitution would corrupt legitimate ANSI plugin hints) (research D2.2)
- [X] T015 [US3] Fix `CStaticText::SetText` invalid-UTF-8 fallback (`src/gui.cpp:619-628`): replace Latin-1 byte widening with `MultiByteToWideChar(CP_ACP, …)` (research D2.4); same file as T013 — sequential
- [X] T016 [US3] Make the `WM_USER_TTGETTEXT` byte clamp UTF-8-boundary-safe: `src/gui.cpp:1053-1058` `lstrcpyn(…, TOOLTIP_TEXT_MAX)` must not split a multi-byte sequence (idiom: `src/common/salunicode.cpp:150-156`); same file as T013/T015 — sequential (research D2.5)
- [X] T017 [P] [US3] Convert the 15 owned hint producers from `LoadStr` to `LoadStrU8`: `src/dialogs.cpp:1981`, `src/dialogs2.cpp:623`, `src/dialogs2.cpp:1256`, `src/dialogs3.cpp:176`, `:347`, `:969`, `:1225`, `:2170`, `src/dialogs4.cpp:1799`, `:3363`, `src/dialogs5.cpp:3169`, `:3172`, `src/dialogs6.cpp:1659`, `src/dialogsp.cpp:721`, `:1273` (research D2.3; 042/043 precedent)
- [ ] T018 [US3] Verify US3: Czech UI hint in Ctrl+M correct; masks hint in one configuration dialog correct; checksum-plugin hint (ANSI producer through intake normalization) correct; status-bar/dir-line tooltips (direct `WM_USER_TTGETTEXT` producers, `src/stswnd.cpp:1827-1884`) still correct in both encodings

**Checkpoint**: hint mechanism fixed product-wide (FR-006).

---

## Phase 6: User Story 4 — Fully visible "File" label (Priority: P3)

**Goal**: "Soubor:" (and every label in `IDD_FILELIST`) fully visible in all 12 languages, durably — the automatic widener fixed, not hand-edited `.slt` geometry.

**Independent Test**: quickstart Scenario 5 steps 3–5 (Czech at 100/150/200% DPI; German "Interner Dateibetrachter" spot check); `--check-layout` green.

### Implementation for User Story 4

- [X] T019 [US4] Fix the free-space scan in `tools/translate/layout.py` (`_overlaps_vertically` use around `:62-65`, blocker loop `:88-97`): clamp the **blocking** vertical extent of a tall empty-text row (dropdown stores its dropped-list height, e.g. cy=105) to a closed-control height (~14 DLU) so comboboxes stop walling off controls to their left (research D7.1)
- [X] T020 [US4] Add radio/checkbox glyph allowance to `estimate_width` (`tools/translate/layout.py:41-50`): use the English template row's control class via the existing-but-unused `english` parameter of `widen(section, english)` (`relayout.py:51` already loads the template); allowance ~11-12 DLU for `BS_AUTORADIOBUTTON`/`BS_AUTOCHECKBOX` rows (research D7.2); same file as T019 — sequential
- [X] T021 [P] [US4] Widen the master template `src/lang/lang.rc:1193-1196` (`IDD_FILELIST`): `IDC_FL_FILE` cx 27→40; `IDC_FL_FILENAME` x 47→57, cx 226→216 (right edge 273 unchanged); `IDC_FL_APPEND` x 47→57. Geometry only — **no string changes** (so no `.slt` two-stage text refresh is triggered) (research D7.3)
- [X] T022 [US4] Propagate geometry text-untouched (depends on T019–T021): `src\vcxproj\build_langs.cmd --export-templates --module salamand` → `python -m translate.relayout --module salamand --dry-run` (review) → `python -m translate.relayout --module salamand` (all 12 languages incl. disabled) → `src\vcxproj\build_langs.cmd --check-layout`; commit the regenerated `translations/*/salamand.slt` geometry rows (research D7.4)
- [ ] T023 [US4] Verify US4: `build.cmd full`, Czech Ctrl+M dialog at 100/150/200% DPI — every label whole; German spot check; confirm zero translated-string changes in the `.slt` diff (geometry-only)

**Checkpoint**: all four user stories independently done.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: sweep the remaining confirmed same-defect clipboard callers onto the U8/W entry points (research D1 table #2–#11), enforce the contract, document, and run the full gate set.

- [X] T024 [P] Sweep `src/fileswn9.cpp` clipboard copies to `CopyTextToClipboardU8`: `:1854`, `:1872`, `:1892`, `:1923` (UNC path — also normalize the ANSI `WNetGetConnection` fragment at `:1881` with `SalLegacyToU8Alloc` before concatenation), `:1989`, `:2003`, `:2017`
- [X] T025 [P] Sweep `src/fileswn1.cpp:1955` (save selection → clipboard) to `CopyTextToClipboardU8`
- [X] T026 [P] Sweep `src/finddlg1.cpp:2785`, `:2794`, `:2800` (Find window copies) to `CopyTextToClipboardU8`
- [X] T027 [P] Sweep `src/mainwnd1.cpp:2676`, `:2682` (directory/status line context-menu Copy) to `CopyTextToClipboardU8` (source documented UTF-8 at `src/stswnd.cpp:2244`)
- [X] T028 [P] Fix `src/gui.cpp:1360` (hyperlink context-menu copy): use the existing `TextW` mirror with `CopyTextToClipboardW` (fall back to U8 copy of `Text` when `TextW` is NULL)
- [X] T029 [P] Fix `src/msgbox.cpp` Ctrl+C copy (`:435`): read button/checkbox labels wide (`GetDlgItemTextW` + `SalWToU8` replacing ANSI reads at `:401`, `:423`) so the composed buffer is uniformly UTF-8, then `CopyTextToClipboardU8`
- [X] T030 [P] Fix internal-viewer Copy for UTF-8 content: `src/viewer3.cpp:1556` and `:2805` — when `ContentEncoding == VCE_UTF8`, convert the selection `CP_UTF8`→UTF-16 and use the W clipboard path (legacy encodings keep the existing behavior); covers "view Ctrl+M list → copy from viewer"
- [X] T031 [P] Add contract identifiers to `tools/check_encoding.py` (sink/producer regex lists around `:123-130`, `:255-269`): `CopyTextToClipboardU8`, `SetToolTipText`, `SetActionShowHint` — mirroring the feature-052 mechanism (contract Enforcement)
- [X] T032 Add `CHANGELOG.md` entries (Fixed, next unreleased version) in user terms: garbled Ctrl+M list for accented names (all destinations + Ctrl+C/Find/viewer copies), garbled hint tooltips in localized UIs, clipped dialog labels in all non-English languages, non-ASCII list-file name garbled on disk, Ctrl+M failing under non-ASCII %TEMP%, `:max` column misalignment
- [ ] T033 Full verification sweep per quickstart Gates: `build.cmd full` (Debug, includes `check_encoding.py`), `build.cmd full release`, `saltests` green, `relayout --dry-run` clean + `--check-layout` green, manual Scenarios 1–7 zero defects, ASCII byte-diffs (file + clipboard) identical to T001 baseline
- [X] T034 Format touched C++ files with `clang-format` (repo config; note: `normalize.ps1` requires pwsh7 which is unavailable — run clang-format directly) and review the final diff for constitution III scope discipline (no adjacent refactoring)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: T001 must run **before any source change** (pre-fix baseline). T002 anytime.
- **Foundational (Phase 2)**: after Setup; **blocks all stories** (T004 blocks US1/Polish sweep; T003 blocks T008/T016).
- **US1 (Phase 3)**: after Phase 2. T006 & T010/T011 edit the same `src/mainwnd4.cpp` region — run Phase 3 before Phase 4 (as ordered).
- **US2 (Phase 4)**: after Phase 3 (file overlap only; functionally independent and independently testable).
- **US3 (Phase 5)**: after Phase 2 (T016 uses the boundary idiom; T003 recommended first). Independent of US1/US2.
- **US4 (Phase 6)**: independent of all other stories (tooling + resources only); can run in parallel with Phases 3–5 by a second developer.
- **Polish (Phase 7)**: T024–T030 after T004; T031 after T013/T017 (identifiers must exist); T032–T034 last.

### Parallel Opportunities

- Phase 2: T003 ∥ T004 (different files).
- Phase 3: T007 ∥ T006/T008 (different files).
- Phase 5: T014 ∥ T013→T015→T016 (tooltip.cpp vs gui.cpp); T017 ∥ everything in the phase.
- Phase 6: T021 ∥ T019→T020 (lang.rc vs layout.py).
- Phase 7: T024–T031 all ∥ (eight different files).
- Story-level: US4 (tooling/resources) fully parallel to US1–US3 (C++).

## Implementation Strategy

**MVP first**: Phases 1–3 alone fix the reported primary defect (garbled clipboard list) — stop, run T009, demo. Then increments: US2 (destinations), US3 (hint), US4 (layout), Polish (sweep + gates). Each checkpoint is independently revertible (constitution III).

**Single-developer order** (recommended): T001→T002→T003/T004→T005→T006/T007/T008→T009→T010/T011→T012→T013→T014/T017→T015→T016→T018→T019→T020/T021→T022→T023→T024…T031→T032→T033→T034.

## Notes

- No new UI strings are added anywhere (LoadStrU8 reuses existing IDs; lang.rc change is geometry-only) — the "new strings break `build.cmd full`" trap from memory does not apply.
- Plugin ABI: no signature or interface-version changes anywhere (contract C2/C3 keep published entry points' semantics).
- `varPlacements` stays byte-based (T008) — do not "fix" it to characters; the info-line consumers index the byte buffer.

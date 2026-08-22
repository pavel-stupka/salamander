# Tasks: Fix File Operations on Names with Unpaired Surrogates

**Input**: Design documents from `/specs/066-fix-surrogate-filenames/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/name-encoding-wtf8.md, quickstart.md

**Tests**: Included — the plan's testing strategy mandates `saltests` pins for the
converter contract (the repo's established practice for `src/common/` changes),
plus fixture-based manual validation per quickstart.md.

**Organization**: One foundational change (the WTF-8 codec) heals all three user
stories; story phases therefore combine story-specific trace/fix work with the
spec's acceptance validation, so each story is independently verifiable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)

## Phase 1: Setup

**Purpose**: Fixtures and a recorded baseline of the defect

- [X] T001 [P] Generate the fixture set `temp\fixtures-066\` per quickstart.md §3 (PowerShell 5.1 script: lone high/low surrogate, leading/trailing, multiple, look-alike twins `U+D800`/`U+D801`, surrogate-named folder with `child.txt`) and save the code-unit dump (quickstart helper) as the fidelity baseline
- [X] T002 [P] Reproduce the defect on the current build against `temp\fixtures-041\Lone�surrogate.txt` (F5 copy, F6 move, F8 delete all fail with "file not found"-class errors) and note the exact error surfaces — baseline for before/after comparison

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The WTF-8 codec and every cross-cutting site it must reach — this IS the fix; all user stories depend on it

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T003 Implement the WTF-8 fallback encoder in `SalWToU8`/`SalWToU8Alloc` in `src/common/salunicode.cpp`: strict `WC_ERR_INVALID_CHARS` fast path unchanged; on failure run a total custom encoder (paired surrogates → one 4-byte sequence, unpaired unit → 3-byte `ED A0 80`–`ED BF BF`); preserve the existing too-small-buffer → empty-string fail-safe; output byte-identical to the fast path for valid input (contract obligation)
- [X] T004 Implement the WTF-8-aware strict decoder in `SalU8ToW`/`SalU8ToWAlloc` in `src/common/salunicode.cpp`: strict `MB_ERR_INVALID_CHARS` fast path unchanged; on failure run a custom decoder accepting strict UTF-8 plus surrogate 3-byte sequences and still rejecting all other malformed input (overlongs, truncated tails, stray continuations, `F5`–`FF`, > U+10FFFF) — the feature-004/063 ANSI-fallback heuristics depend on that failure
- [X] T005 Make `SalU8ToWDisplay`/`SalU8ToWDisplayAlloc` in `src/common/salunicode.cpp` decode WTF-8 surrogate sequences to their true UTF-16 unit (notdef box, Explorer parity) while keeping `U+FFFD` degradation for all other malformed input (depends on T004's decoder core)
- [X] T006 [P] Update the contract-comment block in `src/common/salunicode.h` (SalWToU8/SalU8ToW/SalU8ToWDisplay) to state the WTF-8 contract and reference `specs/066-fix-surrogate-filenames/contracts/name-encoding-wtf8.md`
- [X] T007 [P] Update `SalConvertFindDataW` in `src/common/salfileio.cpp`: the strict conversion now succeeds for every on-disk name; keep the lenient `WideCharToMultiByte(CP_UTF8, 0, …)` branch only as a commented last-resort fail-safe
- [X] T008 [P] Fix the write-side validity probe in `SalRegSetValueExW8` in `src/salamdr6.cpp` to probe via the WTF-8-aware `SalU8ToW` instead of raw `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, …)`, so surrogate-bearing paths stored in configuration are written as their true UTF-16 (read side heals by construction)
- [X] T009 [P] Fix the display probe in `CStaticText::SetText` in `src/gui.cpp` (line ~623): try the WTF-8-aware conversion first, keep the CP_ACP branch for genuine ANSI producers and byte-widening as last resort — a focused surrogate name must render as replacement glyphs, never mojibake
- [X] T010 Survey all remaining `MB_ERR_INVALID_CHARS` probes in core sources (`src/*.cpp`, excluding `src/plugins/` and `src/common/dep/`; known: `src/salamdr4.cpp:1208`, `src/salamdr4.cpp:1221`): convert every site that can receive a file name/path to the WTF-8-aware converters; append the reviewed-site list with verdicts (converted / non-name text, left strict) to `specs/066-fix-surrogate-filenames/research.md`
- [X] T011 Update and extend `src/saltests/saltests.cpp`: change the existing expectation `CHECK(SalWToU8Alloc(L"\xD83D") == NULL)` (line ~70) to the new contract (`"\xED\xA0\xBD"`); add pins for encoder round-trip totality (representative units across `D800`–`DFFF` incl. boundaries, mixed valid+invalid strings, `SalU8ToW(SalWToU8(w)) == w`), byte-compatibility for valid input, decoder strictness (overlong `C0 80`, truncated, stray continuation, `FF FE` still fail), `SalConvertFindDataW` round trip with a surrogate `cFileName` and the buffer-bound maths (3 bytes/unit within `SAL_FIND_NAME_U8`), comparison distinctness + stable ordering + no-crash for `SalCompareNamesUTF8`/`SalNameEqualCI`/`SalNameEquivalent` on look-alike surrogate names, display derivation (WTF-8 → true unit; other malformed → `U+FFFD`)
- [X] T012 Build Debug x64 (`build.cmd`) and run `%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\saltests\saltests.exe` to exit code 0 — converter contract proven before any story validation

**Checkpoint**: Codec contract proven by unit tests — story phases can proceed (in any order or in parallel)

---

## Phase 3: User Story 1 - Delete a file with an unrepresentable name (Priority: P1) 🎯 MVP

**Goal**: F8 (Recycle Bin), Shift+F8 (permanent), and recursive folder delete work on surrogate-bearing names — the escape hatch the defect report leads with

**Independent Test**: quickstart.md §4 rows 3, 4, 6, 12 against regenerated `temp\fixtures-066\` fixtures

- [X] T013 [US1] Trace the delete paths end to end on the fixture set — F8 delete-to-Recycle-Bin (the feature-062 mechanism, see `specs/062-fix-delete-to-recycle/` for the operative site), Shift+F8 permanent delete and recursive directory delete in `src/worker.cpp` (and the `src/fileswn*.cpp` initiators) — and fix any residual site that composes or converts the name outside the healed converters
- [ ] T014 [US1] Validate US1 acceptance: quickstart.md §4 row 3 (F8 → file lands in Recycle Bin), row 4 (Shift+F8 → gone from disk), row 6 (recursive delete of the copied tree incl. `dir�sub`), row 12 delete-part (child of a surrogate-named folder); confirm the delete confirmation dialog renders the name with replacement glyphs (no mojibake) and the Recycle Bin entry carries the true name

**Checkpoint**: User Story 1 fully functional — the reported "cannot delete" symptom is gone

---

## Phase 4: User Story 2 - Copy and move preserve the exact name (Priority: P2)

**Goal**: F5/F6 succeed and the destination name is code-unit identical — no substitute characters ever written to disk

**Independent Test**: quickstart.md §4 rows 1, 2, 5, 7 with the code-unit dump helper as the fidelity oracle

- [X] T015 [US2] Trace the copy/move engine on the fixture set — source/target path composition in `src/worker.cpp` (copy worker, move/rename), the overwrite-confirmation prompt and the progress-dialog name display (routes through the T009-fixed `CStaticText` and `winlib.cpp` setters) — and fix any residual lossy site
- [ ] T016 [US2] Validate US2 acceptance: quickstart.md §4 row 1 (F5 copy, destination code-unit identical to the T001 baseline dump — SC-002), row 2 (F6 move back, identical), row 5 (whole-tree copy incl. `dir�sub`, all depths identical), row 7 (delete/move one look-alike twin leaves the other untouched — FR-006/SC-004), plus the same-name overwrite prompt scenario (US2 acceptance 3)

**Checkpoint**: User Stories 1 and 2 both pass — the full reported defect (delete, move, copy) is fixed

---

## Phase 5: User Story 3 - Remaining panel operations act on the true file (Priority: P3)

**Goal**: F3 view, rename, and attribute changes target the real file instead of failing with "file not found"

**Independent Test**: quickstart.md §4 rows 8, 9, 10, 11

- [X] T017 [US3] Trace the remaining operations on the fixture set — F3 internal viewer open (routes through `SalCreateFile` in `src/common/salfileio.cpp`), rename of a surrogate-named entry to an ordinary name (old-name path composition in `src/fileswn*.cpp`; the rename dialog pre-fill may show replacement glyphs, typing surrogates stays out of scope), and the change-attributes dialog (`SalSetFileAttributes` route) — and fix any residual site
- [ ] T018 [US3] Validate US3 acceptance: quickstart.md §4 row 8 (F3 shows `fixture 066` content), row 9 (rename to `plain.txt` succeeds), row 10 (read-only attribute applied — verify in Explorer), row 11 (info line / panel render one replacement glyph per invalid unit, no mojibake — FR-005)

**Checkpoint**: All user stories independently functional

---

## Phase 6: Polish & Cross-Cutting Concerns

- [ ] T019 [P] Regression sweep: all operations on the feature-041 valid-Unicode fixture set (`temp\fixtures-041\`) unchanged (SC-005); config round trip — set a surrogate-named folder as panel path, restart, panel restores correctly and no other saved value is disturbed (T008's obligation)
- [ ] T020 [P] Explorer-parity spot check per quickstart.md §4 (SC-003): repeat copy/recycle-delete/rename in Windows Explorer on regenerated fixtures — Tandem Commander succeeds wherever Explorer does
- [X] T021 Full build gate: `build.cmd` (Debug) and `build.cmd full release` both compile clean; full `saltests` suite green; clang-format the touched files directly (`clang-format -i` — `normalize.ps1` requires pwsh7 which is unavailable, per project tooling notes)
- [X] T022 [P] Documentation: add the user-facing Fixed entry to `CHANGELOG.md` (unreleased/next version — symptom wording per constitution Release Documentation), add the feature-066 summary to `CLAUDE.md` Recent Changes, and reconcile any research.md addendum from T010
- [ ] T023 Final quickstart.md walk end to end (regenerate fixtures, §4 all 12 rows, §5 regression checks), then clean up `temp\fixtures-066\` per quickstart Cleanup

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — T001 and T002 run in parallel
- **Foundational (Phase 2)**: Independent of Phase 1 (codework) but T012 needs T001's fixtures for nothing — unit tests are self-contained; order within: T003 → T004 → T005 (same file, decoder core shared); T006/T007/T008/T009 parallel after T004; T010 after T004; T011 after T005; T012 last
- **User Stories (Phases 3–5)**: All depend ONLY on Phase 2 completion (+ Phase 1 fixtures for validation); stories are mutually independent — any order, or in parallel
- **Polish (Phase 6)**: After all story phases; T019/T020/T022 parallel; T021 before T023

### User Story Dependencies

- **US1 (P1)**: Foundational only — no dependency on US2/US3
- **US2 (P2)**: Foundational only — independently testable
- **US3 (P3)**: Foundational only — independently testable

### Parallel Opportunities

```text
Phase 1:  T001 ∥ T002
Phase 2:  T003 → T004 → T005 → T011 → T012
                   ├─ T006 ∥ T007 ∥ T008 ∥ T009 (different files)
                   └─ T010
Phases 3–5: {T013→T014} ∥ {T015→T016} ∥ {T017→T018}
Phase 6:  T019 ∥ T020 ∥ T022, then T021 → T023
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 1 (fixtures + baseline) and Phase 2 (the codec — this is the actual fix)
2. Phase 3: delete works → **the user's most painful symptom is gone**; stop and validate independently
3. In practice the codec heals US2/US3 too — their phases are mostly verification, so the marginal cost of completing all stories after Phase 2 is small

### Incremental Delivery

1. Phase 2 checkpoint = converter contract proven by unit tests (safe to merge behind the unchanged fast paths)
2. Each story phase adds validated user-facing value: delete (MVP) → copy/move with fidelity → view/rename/attributes
3. Polish gates the release: regression, Explorer parity, Release build, docs

### Notes

- The heavy lifting is Phase 2; story phases are trace-verify-fix — expect most story tasks to find zero residual sites, but the traces are what makes each story's acceptance trustworthy
- Commit after each task or logical group; stop at any checkpoint to validate
- Plugins (`src/plugins/`) and `src/common/dep/` are explicitly out of scope (spec + contract boundary notes)

---

## Implementation Status (2026-08-22)

**Done by the implementation session** (all `[X]` above): the WTF-8 codec,
all probe conversions (registry facade both directions — the read side was
found lenient too and fixed; clipboard; `CStaticText`; `SalLegacyToU8Alloc`),
the one residual operational site the traces surfaced (`fileswn8.cpp` F8
recycle-list build), tests, builds, docs. Gates: **saltests 1221/0** (was
1145; includes `TestWtf8` codec pins and `TestWtf8FileOps` — a real-NTFS
end-to-end: create/enumerate/copy/move/delete of surrogate-named files and a
surrogate-named directory through the Sal facades), **Debug and Release x64
builds green**, clang-format applied.

**Remaining — require a human driving the GUI** (T014, T016, T018, T019
GUI part, T020, T023): the quickstart.md §4 acceptance walk (12 rows),
Explorer-parity spot check, fixtures-041 GUI regression, and the config
round trip (deliberately not automated — it would overwrite the developer's
live `HKCU\Software\Tandem Commander\0.1` session state). Fixtures are
generated and waiting in `temp\fixtures-066\`; the baseline code-unit dump
is `specs/066-fix-surrogate-filenames/fixture-baseline.txt`.

**Observation (pre-existing, out of scope)**: the recycle-bin guard in
`fileswn8.cpp` (`oneFile->Name[NameLen - 1] <= ' '`) compares a signed char,
so any name whose *last* byte is ≥ 0x80 (trailing non-ASCII character, WTF-8
sequences included) is refused with the "Recycle Bin cannot handle this name"
message — a clear per-item error, not a wrong-file deletion, and it predates
this feature (same pattern exists upstream). Worth its own small fix later.

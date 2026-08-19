# Tasks: Instant Thumbnails in Large Folders

**Input**: Design documents from `/specs/064-speedup-thumbnails/`
**Prerequisites**: plan.md (variant A+B, user decision), spec.md, research.md (RC1–RC4), contracts/thumbnail-scheduling-and-fastpath.md (C1–C6), data-model.md, quickstart.md

**Tests**: No automated UI/perf harness exists for the panel (spec Assumptions); verification is the measured manual scenario set in quickstart.md plus `saltests` and build gates. No new test tasks; verification tasks are explicit.

**Organization**: Variant B (cheap decode) is **foundational** — every story's success criteria assume it. The scheduling work (variant A) maps to the stories: US1 = phase reorder, US2 = scroll signal, US3 = lock discipline.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task)
- **[Story]**: US1–US3 per spec.md
- Line numbers refer to commit `db9f09f`; re-locate by content if drifted.

## Phase 1: Setup (fixtures & baseline)

**Purpose**: measurable fixtures and a pre-fix baseline for SC-001/SC-004, captured before any source change.

- [X] T001 [P] Create fixture generator `specs/064-speedup-thumbnails/fixtures/make_photo_fixture.ps1`: builds `big-plain/` (≥2,000 synthetic JPEGs without embedded thumbnails, ≥4 MB each), `small/` (100 JPEGs), and documents how to stage `big-exif/` (≥5,000 real camera JPEGs with EXIF previews — synthetic generation cannot produce representative EXIF thumbnails; use a copy of a real archive) incl. the extras from quickstart Prerequisites (portrait/EXIF-rotated photos, one corrupt file, one ≥90 MPix image, non-image files)
- [ ] T002 Capture the pre-fix baseline with the current build into `specs/064-speedup-thumbnails/baseline.md`: stopwatch time-to-first-visible-thumbnail and time-to-full-first-screen in `big-exif/` (or the largest available real folder), `big-plain/`, and `small/`; note CPU/disk of the test machine — feeds SC-001/SC-004 comparisons in T017

---

## Phase 2: Foundational — variant B: WIC fast-path decode (BLOCKS all stories)

**Purpose**: drop per-thumbnail cost from ~0.5–1.3 s to ~1–150 ms (research RC2, contract C4/C5). Independently landable and measurable before any scheduling change.

- [X] T003 Implement the thumbnail decode ladder in the WIC engine `src/plugins/pictview/wicengine.cpp` + `wicengine.h`: when an image was opened with `PVOF_THUMBNAIL` (store the open-time flag in the engine's image object — today ignored, `thumbs.cpp:858`), provide (1) embedded decoder thumbnail via `IWICBitmapFrameDecode::GetThumbnail()` (fast round only), (2) reduced-resolution decode via `IWICBitmapSourceTransform::GetClosestSize/CopyPixels` at the smallest supported size ≥ the requested thumbnail, (3) today's full `DecodeFrame` as last resort; expose the chosen effective source dimensions to the caller (internal PVW32DLL surface — engine-internal, not plugin ABI); on the thumbnail path skip `CompositeOverBackground` for alpha-free sources and skip `BuildLines` (`wicengine.cpp:158-193,430-503,908-965`)
- [X] T004 Drive the ladder from `src/plugins/pictview/thumbs.cpp`: pass the requested thumbnail size and `fastThumbnail` mode into the engine before `SetParameters` (`thumbs.cpp:1015-1041` — parameters must reflect the *effective* decoded dimensions, not the full image); set `SSTHUMB_ONLY_PREVIEW` when the delivered pixels are smaller than the requested thumbnail (arms the existing quality round, `fileswn1.cpp:937,1069-1070`); quality round (`fastThumbnail == FALSE`) uses reduced decode ≥ requested size (never the EXIF preview)
- [X] T005 [P] EXIF orientation for thumbnails (contract C5): read the orientation property in the WIC engine, report it (engine-internal flags, e.g. finally setting the never-set `PVFF_EXIF`/rotation info, `wicengine.cpp:576`), and map it to `SSTHUMB_MIRROR_*`/`SSTHUMB_ROTATE_*` maker flags in `src/plugins/pictview/thumbs.cpp:896-923` (code exists, currently unreachable); both EXIF-preview and reduced-decode results carry the same orientation
- [X] T006 Demote the `Thumbs.db`/ADS probe: `src/plugins/pictview/thumbs.cpp:851` (`ExtractWinThumbnail`) runs only when the WIC ladder cannot serve the file, not unconditionally before it
- [ ] T007 Foundational gate: `build.cmd` compiles; single-file cost spot-check on `big-exif/` and `big-plain/` samples (thumbnails visibly faster even with today's ordering); portrait photo renders rotated correctly in the panel

**Checkpoint**: per-thumbnail cost is orders of magnitude lower; ordering still wrong (huge folders still delay start) — stories fix that.

---

## Phase 3: User Story 1 — Visible previews appear immediately (Priority: P1) 🎯 MVP

**Goal**: first visible thumbnails within ~2 s of entering a huge folder — the whole-listing overlay sweep no longer precedes them (research RC1, contract C1).

**Independent Test**: quickstart Scenario 1 (big-exif, big-plain, small control) against the T002 baseline.

### Implementation for User Story 1

- [X] T008 [US1] Reorder the reader's phase machine in `src/fileswn1.cpp`: per visibility band, icons → thumbnails (fast round) → icon overlays; the whole-listing overlay sweep runs after the visible+surround thumbnail bands; stale icons and the quality thumbnail round (`wanted == 6`) follow as today (anchors: overlay interleave `:1107-1129`, `wanted` machine `:1131-1167`, `selectMode` reset `:1171-1179`, band gate `:625-688`); non-thumbnail view modes keep today's order exactly (contract C1)
- [ ] T009 [US1] Verify US1: quickstart Scenario 1 — first visible thumbnail ≤ 2 s in `big-exif/`; ≤ 2× the `small/` time; `big-plain/` prompt; SC-004 no small-folder regression vs T002 baseline

**Checkpoint**: primary reported defect fixed — MVP demonstrable.

---

## Phase 4: User Story 2 — Scrolling reprioritizes to the new viewport (Priority: P1)

**Goal**: any viewport change redirects generation priority immediately (research RC3 first half, contract C2).

**Independent Test**: quickstart Scenario 2 (jumps, rapid scrolling) in `big-exif/`.

### Implementation for User Story 2

- [X] T010 [US2] Make visible-array refresh synchronous on every viewport change in `src/filesbx1.cpp`: at each site that today only calls `InvalidateArr()` (`:72-73, :603-604, :657-658, :713-714, :948-949, :1086-1092, :1167-1173, :1241-1247, :2373-2425, :2587-2588`) also call `RefreshArr(Parent)` (pattern already used for thumb-drag at `:1241-1247`); keep the idle refresh (`src/fileswna.cpp:327-335`, `mainwnd1.cpp:3171-3175`) as the safety net; confirm `RefreshArr` cost is acceptable per wheel notch (it snapshots ~visible names only, `fileswna.cpp:838-902`)
- [ ] T011 [US2] Verify US2: quickstart Scenario 2 — after End/PgUp/scrollbar jumps, new-viewport previews start ≤ 1 s and complete before off-screen backlog resumes; rapid wheel scrolling wastes no work on passed positions

**Checkpoint**: both P1 stories done.

---

## Phase 5: User Story 3 — The panel stays responsive throughout (Priority: P2)

**Goal**: no UI stall waits for a running decode (research RC3 second half, contract C3).

**Independent Test**: quickstart Scenario 3 (input latency, folder change and sort change mid-generation).

### Implementation for User Story 3

- [X] T012 [US3] Release `ICSleepSection` around the plugin `LoadThumbnail` call in the thumbnail branch of `src/fileswn1.cpp:915-953`, mirroring the icon branch's leave/re-enter + revalidate pattern (`:769-797`, incl. the `fileDataCheck`-style PRUSER consistency checks); after re-entering, revalidate the item before storing the result into the cache/`thumbMaker` state (a stale result is discarded); existing cooperative cancel (`ICStopWork`) unchanged
- [ ] T013 [US3] Verify US3: quickstart Scenario 3 — no perceptible input freeze during generation; leaving the folder / changing sort mid-decode is immediate; corrupt file and ≥90 MPix guard behave per Scenario 4 items 3–4

**Checkpoint**: all three stories independently verified.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: bundled quick wins (research "minor frictions"), documentation, and the full gate sweep.

- [X] T014 [P] Replace the per-completed-thumbnail linear `strcmp` scan over `Files` (`src/fileswn1.cpp:1078-1088`) with a cheaper lookup — **decision: left as-is**. The cache record carries no panel index (name is the only key), the panel arrays are not name-sorted, and the scan (pointer walk + `strcmp`) costs microseconds per completed thumbnail even at 10,000 items — noise next to a decode. A name→index map would add refresh-invalidation complexity for no measurable gain.
- [X] T015 [P] Investigate the Alt+5 re-entry carry-over (research RC4) — **decision: deferred**. The carry-over transfer is gated on equal icon size (`fileswn0.cpp:2802`), and a view-mode switch changes the icon size (Thumbnails uses 48 px), so `TemporarilySimpleIcons` is not the only obstacle: thumbnails would need a transfer decoupled from the icon-size gate, touching the refresh/merge core (`icncache.cpp:469-665`). Disproportionate risk for a convenience win; recorded as a follow-up candidate alongside variant D (persistent cache would subsume it).
- [X] T016 [P] Add `CHANGELOG.md` entries (Unreleased → Fixed/Changed, user terms): thumbnails in large folders start immediately and follow the visible screen; per-photo generation is far cheaper (embedded preview/reduced decode with background quality pass); portrait photos are no longer shown unrotated; panel no longer stalls on folder change during generation
- [ ] T017 Full verification sweep: quickstart Scenarios 1–6 + Scenario 4 quality-upgrade check; gates — `build.cmd full` (Debug), `build.cmd full release`, `saltests` green, Alt+1..4 icon/overlay smoke (ordering-only change), record measurements vs `baseline.md`
- [X] T018 Format touched C++ files with `clang-format` (repo config; `normalize.ps1` needs pwsh7 — run clang-format directly) and review the diff for constitution III scope discipline

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: T002 requires the current (pre-fix) build — run before any source change; T001 anytime.
- **Foundational (Phase 2)**: blocks all stories (their SC numbers assume cheap decode). T003 → T004 (same subsystem, engine before driver); T005 parallel to T004 after T003; T006 after T004.
- **US1 (Phase 3)**: after Phase 2. Touches `fileswn1.cpp`.
- **US2 (Phase 4)**: independent of US1 in files (`filesbx1.cpp`), but verification assumes US1's ordering — run after US1.
- **US3 (Phase 5)**: touches `fileswn1.cpp` — after US1 (same file, sequential).
- **Polish (Phase 6)**: T014 after T012 (same file region); T015 independent; T016 anytime after Phase 2; T017/T018 last.

### Parallel Opportunities

- Phase 1: T001 ∥ T002.
- Phase 2: T005 ∥ T004 (after T003).
- Phase 6: T014 ∥ T015 ∥ T016.
- US2 (filesbx1.cpp) could be developed in parallel with US1/US3 (fileswn1.cpp) by a second developer.

## Implementation Strategy

**MVP** = Phases 1–3: cheap decode + visible-first ordering already deliver the reported fix (Scenario 1). Then US2 (scroll priority), US3 (responsiveness), Polish. Each checkpoint is independently revertible; B alone is a shippable improvement if A needed to be backed out (constitution III).

## Notes

- No new UI strings, no dialog changes → no translation work triggered.
- Plugin ABI untouched: all flags/interfaces used are published (`spl_thum.h`); `PVW32DLL.h` is pictview-internal and may be extended freely.
- Do not change: refresh carry-over semantics, sort-change no-re-decode, thumbnail size/layout, non-thumbnail view-mode ordering (contract C6).

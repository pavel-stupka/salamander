# Quickstart — validating 064-speedup-thumbnails

Validation guide for variant A+B (viewport-first scheduling + WIC fast-path
decode). Success criteria from [spec.md](spec.md); binding rules in
[contracts/thumbnail-scheduling-and-fastpath.md](contracts/thumbnail-scheduling-and-fastpath.md).

## Prerequisites

- Windows 11, VS2022, `build.cmd full` (pictview must be built — it is the
  thumbnail loader).
- **Fixture folders** (script: `fixtures/make_photo_fixture.ps1`):
  - `big-exif/` — ≥5,000 JPEGs with embedded EXIF thumbnails (camera-style),
    tens of GB total. Generated synthetically or a copy of a real photo
    archive (real photos preferred for the EXIF path).
  - `big-plain/` — ≥2,000 JPEGs **without** embedded thumbnails (synthetic;
    exercises the reduced-decode path).
  - `small/` — 100 JPEGs (control; SC-004).
  - Include several portrait (EXIF-rotated) photos, one corrupt file, one
    ≥90 MPix image, and a mix of non-image files.

## Scenario 1 — first screen at any folder size (SC-001, US1)

1. Open `big-exif/` in Thumbnails view (Alt+5). Stopwatch: first visible
   thumbnail ≤ 2 s; visible screen fully populated shortly after.
2. Repeat in `small/` — time to first thumbnail differs by ≤ 2×.
3. Repeat in `big-plain/` — first thumbnails still start promptly (reduced
   decode, ~tens of ms each).

## Scenario 2 — scroll reprioritization (SC-002, US2)

1. While `big-exif/` is still generating, press End, then PgUp several times,
   then drag the scrollbar to the middle.
2. At every stop: new viewport previews start ≤ 1 s and the screen fills
   before background (off-screen) progress resumes.
3. Rapid wheel scrolling: no stalls; on stop, the stopping viewport populates
   next.

## Scenario 3 — responsiveness & cancellation (SC-003, US3)

1. During generation: scroll, move cursor, select, change sort, invoke Alt+F10;
   no perceptible input freeze (~100 ms threshold).
2. Change to another folder mid-generation — the switch is immediate (no wait
   for a running decode; contract C3).
3. Change sort order — already-shown thumbnails stay (SC-005, FR-007); the new
   visible set populates first.

## Scenario 4 — quality upgrade & correctness (contract C4/C5)

1. In `big-exif/`, observe: previews appear near-instantly; within the
   background quality round they are replaced by final-quality thumbnails
   (no visible size/layout change; quality improves for items whose EXIF
   preview was smaller than the thumbnail size).
2. Portrait photos display **correctly rotated** (this is a fixed defect —
   verify against Explorer's thumbnails).
3. The corrupt file keeps a generic icon; the pipeline continues (FR-006).
4. The ≥90 MPix image respects the existing size guard (generic icon, no
   stall).

## Scenario 5 — refresh & re-entry behavior (FR-007)

1. Copy a few new photos into the folder while thumbnails show → only new
   files are generated; existing ones do not flicker or regenerate.
2. Alt+1 then Alt+5 — [if the RC4 quick win ships] thumbnails reappear
   without re-decode; otherwise document as unchanged known behavior.

## Scenario 6 — no small-folder regression (SC-004)

Time to fully populated first screen in `small/` is no worse than the pre-fix
build (compare against a stopwatch measurement taken on `main` before the
change).

## Gates (all must pass before merge)

| Gate | Command / method |
|------|------------------|
| Full build incl. encoding guard | `build.cmd full` |
| Release build sanity | `build.cmd full release` |
| Unit tests | `saltests` green |
| Icon/overlay behavior unchanged in non-thumbnail modes | manual smoke: Alt+1..4 icons + overlay badges (OneDrive folder) appear as before |
| Manual scenarios | 1–6 above, zero defects |

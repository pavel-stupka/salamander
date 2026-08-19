# Pre-fix baseline (T002)

Recorded 2026-08-19 on branch `064-speedup-thumbnails` at commit `db9f09f`
(pre-fix state).

## Analytical baseline (from research.md, code-derived)

- Order of work before the first thumbnail: whole-listing icon+overlay sweep
  first — in a 5,000-item folder that is ~5,000 per-file shell
  `GetIconOverlayIndex` calls (RC1).
- Per-thumbnail cost: full-resolution decode + full-image shrink,
  ≈ 0.5–1.3 s per 24 MPix JPEG, sequential (RC2).
- Expected user-visible behavior (matches the report): minutes with generic
  icons in a large folder; a visible screen of ~30 photos alone costs
  ~15–40 s once thumbnails finally start; scrolling does not reprioritize
  until idle.

## Stopwatch measurements (to be filled on the test machine, pre-fix build)

Machine: __________________ (CPU / disk / photo source)

| Folder | Items | First visible thumbnail | Visible screen complete |
|--------|-------|------------------------|------------------------|
| big-exif | ≥5,000 | ______ s | ______ s |
| big-plain | ≥2,000 | ______ s | ______ s |
| small | 100 | ______ s | ______ s |

Same table is re-measured on the fixed build in T017 (quickstart Scenarios 1–2);
SC-001 compares the two.

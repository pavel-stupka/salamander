# Investigation Leads — 064-speedup-thumbnails

Preliminary code pointers gathered during specification (2026-08-19). These are
**starting points for the planning-phase analysis, not conclusions**. The
planning deliverable is a measured root-cause analysis plus several candidate
solutions with trade-offs for a joint decision (explicit user request).

## Pipeline map (to be verified and measured)

- **View mode**: Alt+5 = `vmThumbnails`; `CFilesWindow::UseThumbnails` decided
  in `src/fileswn3.cpp:105-126` (only when a thumbnail-loader plugin claims the
  extension, `fileswn3.cpp:716-755`).
- **Worker**: the per-panel **icon-reader thread** `IconThreadThreadFBody`
  (`src/fileswn1.cpp:402+`) reads icons, overlays **and thumbnails** in one
  loop (`readThumbnails = window->UseThumbnails`, `fileswn1.cpp:541`).
- **Cache**: `IconCache` (`CIconCache`) holds icons + thumbnails per item;
  `GetThumbnail` consumed at draw time in `src/fileswn4.cpp:1376`.
  Sleep/wake choreography in `src/fileswn0.cpp` (`SleepIconCacheThread`,
  `WakeupIconCacheThread`, `IconCacheValid`).
- **Thumbnail production**: `CSalamanderThumbnailMaker` (`src/thumbnl.h/.cpp`)
  receives full-resolution rows from the loader plugin and shrinks them;
  the shipped loader is **pictview** on the WIC engine
  (`src/plugins/pictview/wicengine.*`, feature 006); plugin contract
  `src/plugins/shared/spl_thum.h`.

## Hypotheses to test during planning (each needs evidence)

1. **Reading order is not viewport-driven** — the icon-reader likely walks the
   `IconCache` in its stored order, so in a 5,000-file folder the visible rows
   wait behind everything before them. (Salamander historically sorted the
   icon cache for locality, and there is *some* visibility handling around the
   icon reader — find it and establish what it actually prioritizes for
   thumbnails.)
2. **Full-size decode cost** — thumbnails may be produced by decoding the full
   image (tens of MB / tens of MPix per photo) and shrinking, i.e. seconds per
   file × thousands of files. WIC can decode JPEGs at reduced resolution
   (IWICBitmapSourceTransform) and most large photos carry an embedded EXIF
   preview that is orders of magnitude cheaper to read — check what the
   pictview WIC engine actually does for the thumbnail path.
3. **Single-threaded decode** — one icon-reader thread per panel does icons +
   overlays + thumbnails serially; decode parallelism (or a separate thumbnail
   worker pool) may be part of a candidate solution.
4. **Restart behavior** — refreshes/sort changes may restart the reader and
   possibly re-enqueue everything; verify what survives (`fileswn0.cpp:3066`
   suggests a "nothing to do" fast path exists).
5. **Scroll interaction** — establish how (and whether) the panel notifies the
   reader about the visible range today (search for `EnsureVisible`/first
   visible index handed to the reader).

## Candidate solution directions (to be elaborated + compared in plan.md)

A. Viewport-first scheduling in the existing reader (priority queue driven by
   the visible index range; re-prioritize on scroll/resize/sort).
B. Cheap pixels first: use WIC reduced-resolution decode and/or embedded EXIF
   thumbnails when present; full decode only as fallback.
C. Parallel thumbnail decoding (worker pool separate from the icon/overlay
   reader), bounded by CPU count and memory.
D. Persistent on-disk thumbnail cache across sessions (biggest win on revisit;
   new scope: cache location, invalidation, size cap — likely a follow-up
   feature rather than part of this one).
E. Combinations: A is almost certainly the backbone; B/C multiply its effect;
   D is optional persistence on top.

## Fixture

`fixtures/` should get a script generating a synthetic large-photo folder
(e.g. 5,000 × ~4-8 MB JPEGs, ≥20 GB total, plus a 100-photo control folder)
so SC-001/SC-002 are measurable without the user's real photo archive.

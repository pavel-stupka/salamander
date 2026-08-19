# Contract: Thumbnail scheduling & fast-path decode

**Feature**: 064-speedup-thumbnails · **Status**: binding once implemented
**Variant**: A+B (user decision, research.md) · Line numbers at `db9f09f`.

## C1 — Reader phase order guarantees visible thumbnails early

Binding rule: **the whole-listing overlay sweep MUST NOT precede the visible
thumbnail band** (pre-fix it did — RC1), and visible thumbnails MUST come
before any whole-listing per-file shell work. Non-thumbnail view modes keep
the pre-fix order exactly.

Implemented order (Thumbnails view, first round): icons+overlays for the
visible band → icons+overlays for the surround band → remaining icons (in a
photo folder these are few: images are thumbnail-cache entries, not icon
entries) → **thumbnails visible → surround → all** → the deferred
whole-listing overlay sweep → stale icons → quality thumbnail round
(visible → surround → all). This satisfies the binding rule with a minimal,
revertible change to the phase machine (`deferOverlaysAll` in
`src/fileswn1.cpp`); a strict per-band icons→thumbnails→overlays interleave
would require inverting the `wanted`/`selectMode` loop nesting and was
rejected as disproportionate risk (constitution III).

## C2 — Scroll signal freshness

Every viewport change (wheel, keyboard, thumb-drag, resize, sort, jump)
refreshes `VisibleItemsArray`/`Surround` **immediately** (not only at idle),
so the reader never runs a sweep against a stale or invalidated array while
the user is scrolling. The idle refresh stays as a safety net. The reader's
existing version-numbered restart (`fileswn1.cpp:661-686`) remains the sole
reprioritization mechanism (no new threads, no queue).

## C3 — Lock discipline & cancellation

- `ICSleepSection` MUST NOT be held across a plugin `LoadThumbnail` call: the
  thumbnail branch adopts the leave/re-enter + revalidate pattern the icon
  branch already uses (`fileswn1.cpp:769-797`), so `SleepIconCacheThread()`
  (path change, refresh, sort) is never blocked behind a running decode.
- After re-entering, the item MUST be revalidated (cache generation/name)
  before its result is stored; a stale result is discarded.
- The fast path keeps decode units small (EXIF preview or reduced-resolution
  decode), which bounds the worst-case cancellation latency; the existing
  cooperative cancel (`ICStopWork` via `GetCancelProcessing`/`ProcessBuffer`)
  stays in place.

## C4 — Fast/quality thumbnail protocol (existing ABI, now honored)

- Fast round (`fastThumbnail == TRUE`): pictview MUST try, in order:
  1. embedded decoder thumbnail (`IWICBitmapFrameDecode::GetThumbnail`);
     if its dimensions are smaller than the requested thumbnail size, the
     result is delivered with `SSTHUMB_ONLY_PREVIEW` → the core re-queues the
     item for the quality round (existing machinery,
     `fileswn1.cpp:937,1069-1070`);
  2. reduced-resolution decode (`IWICBitmapSourceTransform`, closest
     supported size ≥ requested) — full final quality, no preview flag;
  3. full-frame decode (today's path) as last resort.
- Quality round (`fastThumbnail == FALSE`): reduced-resolution decode at ≥
  requested size (or full decode when unsupported); the result replaces the
  preview and is flagged final.
- `Thumbs.db`/ADS probing (`thumbs.cpp:851`) MUST NOT run before the WIC fast
  path; it remains only as a fallback for files WIC cannot serve.
- No plugin ABI change: `LoadThumbnail` signature, `SSTHUMB_*` flags and
  `CSalamanderThumbnailMakerAbstract` are used as published
  (`spl_thum.h:29-138`).

## C5 — EXIF orientation

Panel thumbnails MUST honor the photo's EXIF orientation (via the existing
`SSTHUMB_MIRROR_*`/`SSTHUMB_ROTATE_*` maker flags). This fixes the current
regression (orientation never applied: `pv.Flags = 0`, `wicengine.cpp:576`).
Both the embedded-preview and the reduced-decode results carry the same
orientation handling.

## C6 — What must not change

- Thumbnail visual size/layout (`Configuration.ThumbnailSize`, spacing) and
  the appearance of non-image items.
- Refresh carry-over semantics (name + Size + LastWrite ⇒ keep, changed ⇒
  quality-round re-read — `icncache.cpp:607-641`).
- Sort change still never re-decodes unchanged thumbnails
  (`fileswn2.cpp:583-587`).
- Icon/overlay results themselves (only their ordering relative to
  thumbnails moves).
- Plugin ABI and `LAST_VERSION_OF_SALAMANDER`.

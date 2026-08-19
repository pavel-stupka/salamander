# Data Model — 064-speedup-thumbnails

No new persistent data. The feature re-orders and re-costs work over existing
in-memory structures; this file pins their states and transitions as the
implementation must leave them. Line numbers at `db9f09f`.

## CIconData (per cached item, `src/icncache.h:11-52`)

- Key: DWORD-aligned `NameAndData` (name; for thumbnails followed by
  `Size` + `LastWrite` + NULL-terminated loader-plugin list).
- `Flag` (3 bits): icons `0` unread / `1` ok / `2` stale / `3` icon-location;
  thumbnails `4` unread / `5` **final** / `6` **stale or preview-quality**.
- `ReadingDone` (1 bit): "attempted in this wake-up cycle".
- `Index` (28 bits): slot in `IconsCache` or `ThumbnailsCache` (raw 32bpp DDB
  bits, ~`W*H*4` B; no eviction — unchanged by this feature).

### Thumbnail state transitions (after the fix)

```
4 (unread)
  └─ fast round (wanted==4):
       EXIF preview smaller than requested  → 6 (preview shown, re-queued)
       reduced-decode / full decode         → 5 (final)
       decode failure                        → stays 4, ReadingDone=1 (generic icon)
6 (preview or stale)
  └─ quality round (wanted==6): reduced/full decode → 5 (final)
refresh carry-over (icncache.cpp:607-641):
  old 5 + same Size+LastWrite → 5 (kept, no re-decode)
  old 5/6 + changed stamp     → 6 (pixels kept for display, re-decoded)
```

## Reader phase machine (`wanted`, `fileswn1.cpp:1131-1167`)

Current: `0 (new icons) → 4 (new thumbs) → 2 (stale icons) → 6 (stale/preview
thumbs)`, with overlays interleaved into every icon sweep and the visible→
surround→all `selectMode` sweep inside each phase.

Target (contract C1): per visibility band, icons → thumbnails(fast) →
overlays; whole-listing overlay sweep last; then stale icons and the quality
thumbnail round. `selectMode` semantics (1 unfiltered / 2 visible /
3 surround / 4 sequential-with-valid-array) and the version-restart protocol
are unchanged.

## Visible set (`CVisibleItemsArray`, `src/fileswnd.h:652-699`)

Snapshot of visible (and ±1 page) item **names** + `ArrVersionNum`. New
behavioral rule (contract C2): refreshed synchronously on every viewport
change; consumed by the reader exactly as today.

## Fast-path decode source (pictview, per file)

Order of pixel sources for one thumbnail (contract C4):

1. embedded decoder thumbnail (EXIF) — may be `SSTHUMB_ONLY_PREVIEW`;
2. reduced-resolution frame decode (`IWICBitmapSourceTransform`);
3. full-frame decode (legacy path);
4. `Thumbs.db`/ADS extraction only as fallback when WIC cannot serve.

Orientation from EXIF maps to the maker's `SSTHUMB_MIRROR_*`/`SSTHUMB_ROTATE_*`
transform flags (contract C5).

## Invariants to preserve

- One `CIconData` per file: icon XOR thumbnail entry (`fileswn3.cpp:813`).
- Cache is name-sorted; panel order never matters to the cache
  (`icncache.cpp:57-144`).
- `ThumbnailsCache` bits are written only under `ICSectionUsingThumb` and read
  by the painter under the same section when `!IconCacheValid`.
- No new memory growth: the fast path only makes filling the same slots
  cheaper (and the reduced decode shrinks transient DIBs from ~96 MB to
  ≤ ~1.5 MB per image in flight).

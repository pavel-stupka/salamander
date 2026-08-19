# Research — 064-speedup-thumbnails

**Date**: 2026-08-19 · **Input**: spec.md + investigation-leads.md
**Method**: three parallel read-only code investigations (reader scheduling;
decode cost; cache/UI hooks), consolidated. Line numbers at commit `db9f09f`.
Timing figures are code-shape estimates, not benchmarks — to be confirmed by
measurement during implementation.

## Root causes (all verified in code)

### RC1 — Phase ordering starves thumbnails behind a whole-folder overlay sweep

The per-panel icon-reader thread (`IconThreadThreadFBody`, `src/fileswn1.cpp:402+`)
processes work in phases (`wanted` 0→4→2→6, `fileswn1.cpp:1131-1167`) crossed
with a visibility sweep (`selectMode` visible→surround→all,
`fileswn1.cpp:586-688`) — and **icon overlays are interleaved into every icon
sweep** (`fileswn1.cpp:1107-1129`). The effective global order is:

1. icons (visible) → 2. **overlays (visible)** → 3. icons (surround) →
4. overlays (surround) → 5. icons (all) → 6. **overlays (ALL: one shell
`GetIconOverlayIndex` call per file/dir over the whole listing,
`fileswn1.cpp:719`)** → 7. **thumbnails (visible)** → 8. thumbnails (surround)
→ 9. thumbnails (all) → 10. stale rounds.

In a 5,000-photo folder, thousands of per-file shell overlay queries run
**before the first thumbnail is even attempted**. (Photos are thumbnail-type
cache entries, not icon entries — `fileswn3.cpp:813` — so phases 1-5 are
quick; phase 6 is not.)

Good news: the **visible-first machinery already exists and covers
thumbnails** — `CVisibleItemsArray`/`Surround` (`src/fileswnd.h:652-699`,
`fileswna.cpp:760-969`) gates the reader via name lookup
(`fileswn1.cpp:625-647`) with version-numbered restarts
(`fileswn1.cpp:661-686`). The defect is purely the phase order above it.

### RC2 — One thumbnail costs ~0.5–1.3 s: full-resolution decode of the whole photo

pictview (the only real thumbnail loader, `pictview.cpp:1177-1187`) decodes
the **entire frame at full resolution** for every thumbnail:
`LoadThumbnail` (`thumbs.cpp:835`) → `WicSaveImage` (`wicengine.cpp:908`) →
`DecodeFrame` (`wicengine.cpp:430`): full-frame `IWICFormatConverter` to
32bppPBGRA (`:458-476`) into a `CreateDIBSection` of W×H×4 (**96 MB for a
24 MPix photo**, `:134-156`), a full-image alpha composite pass (`:173-193`,
pure waste for JPEG), an unused `BuildLines` allocation (`:158-169`), then a
second full pass over all pixels in the scalar box-filter shrinker
(`CShrinkImage::ProcessRows`, `thumbnl.cpp:142-360`). Estimated ≈0.5–1.3 s
per photo warm, sequential.

**The cheap paths exist and are unused**: the WIC decoder is already created
with `WICDecodeMetadataCacheOnDemand` (`wicengine.cpp:271`) — ready for
`IWICBitmapFrameDecode::GetThumbnail()` (embedded EXIF preview, typically
160×120, ~1-3 ms) and `IWICBitmapSourceTransform` (JPEG DCT-domain 1/2-1/8
decode; 94 px target from 6000 px → 1/8 = 750×500, ~an order of magnitude
cheaper, DIB 96 MB → ~1.5 MB). Neither API is referenced anywhere in
pictview. The core's **two-phase fast/quality protocol is alive but
effectively dead**: `fastThumbnail`/`PVOF_THUMBNAIL` is passed
(`thumbs.cpp:858`) and ignored by the WIC engine (zero references);
`SSTHUMB_ONLY_PREVIEW` is set only on a Thumbs.db hit (`thumbs.cpp:903-908`),
so the quality round (`wanted == 6`, `fileswn1.cpp:1155`, re-queue at
`:1069-1070`) never runs in practice.

**Side findings** (same code): EXIF orientation is never applied to panel
thumbnails (`pv.Flags = 0`, `wicengine.cpp:576` → rotation blocks in
`thumbs.cpp:896-923` unreachable) — visible product defect for portrait
photos; a `Thumbs.db`/ADS probe runs per file per round
(`thumbs.cpp:851`, `ExtractWinThumbnail` `:592-827`) with an O(catalog) scan
when Thumbs.db exists.

### RC3 — Scroll-to-priority latency and a lock held across whole decodes

Scrolling only invalidates the visible arrays; they refill **at idle**
(`filesbx1.cpp:1241-1247`, idle driver `mainwnd1.cpp:3171-3175`) — except
during scrollbar thumb-drag, which refreshes immediately. While invalid, the
reader degrades to **unfiltered sequential order** (`fileswn1.cpp:650-657`).
The reader reacts to viewport changes only at item boundaries, and one item
can be a >1 s decode with **no cancellation point inside WIC `CopyPixels`**
(`wicengine.cpp:450-503`). Unlike the icon branch (`fileswn1.cpp:769-797`),
the thumbnail branch **never leaves `ICSleepSection`** around the slow call
(`fileswn1.cpp:915-953`), so `SleepIconCacheThread()` — i.e. every path
change, refresh, sort change — blocks until the current photo finishes.

### RC4 — Entering Alt+5 always re-decodes everything

A view-mode switch sets `TemporarilySimpleIcons` (`fileswn2.cpp:1124-1136`),
which forfeits the icon-cache carry-over (`fileswn0.cpp:2432`) and releases
the cache (`fileswn3.cpp:82-87`). Refresh carry-over itself is healthy —
name + Size + LastWrite matching keeps flag-5 thumbnails without re-decode
(`icncache.cpp:607-641`), and a sort change does not touch the cache at all
(`fileswn2.cpp:583-587`) — but leaving and re-entering Thumbnails view starts
from zero.

### Minor frictions (cheap to fix alongside)

- Per-completed-thumbnail linear `strcmp` scan over `Files` to find the panel
  index for repaint (`fileswn1.cpp:1078-1088`), O(N) per item, under
  `ICSleepSection`.
- Per-paint `CreateBitmap`/`DeleteObject` from cached bits
  (`fileswn4.cpp:1379`, `:1526`).
- No memory cap on `ThumbnailsCache` (`icncache.h:91`; ~35 KB/item at the
  default 94 px — 10,000 photos ≈ 350 MB *only if* the whole folder gets
  decoded; visible-first does not change the ceiling, D would).

## Candidate solutions

### A — Viewport-first scheduling (reorder phases + immediate scroll signal)

Reorder the phase machine so the visible band gets **icons → thumbnails →
overlays** before any wider band, and the whole-listing overlay sweep runs
last; refresh `VisibleItemsArray` immediately on every scroll (not only at
idle/thumb-drag); release `ICSleepSection` around `LoadThumbnail` like the
icon branch already does. All inside the existing, proven `selectMode`
machinery — no new threads, no plugin ABI change.

- **Effect**: first visible thumbnail attempt starts within tens of ms of
  listing/scroll. Alone it does NOT meet SC-001 in huge folders — each visible
  thumbnail still costs 0.5–1.3 s (a 30-item screen = 15–40 s).
- **Risk**: low-medium (phase machine surgery in one function; regression
  surface = icon/overlay loading order).

### B — Cheap pixels first (WIC fast path in pictview) + quality round

Honor the existing `fastThumbnail` contract in the WIC engine: fast round
tries **embedded EXIF thumbnail** (`GetThumbnail()`, ~1-3 ms), else
**reduced-resolution decode** (`IWICBitmapSourceTransform::GetClosestSize`,
~50-150 ms for 24 MPix JPEG), setting `SSTHUMB_ONLY_PREVIEW` when the result
is smaller than the requested thumbnail — which re-arms the already-built
quality round (`wanted == 6`) to upgrade in the background. Skip
`CompositeOverBackground`/`BuildLines` on the thumbnail path; skip the
Thumbs.db probe when WIC can serve; **apply EXIF orientation** (fixes the
rotation defect as a bonus). Plugin-internal — no ABI change (the flags all
exist).

- **Effect**: the multiplier. Per-thumbnail cost drops 2-3 orders of magnitude
  when EXIF previews exist (typical camera/phone JPEGs), ~10× otherwise.
  A+B together: visible screen populated in well under a second for typical
  photo folders → meets SC-001/SC-002.
- **Risk**: medium (WIC API usage — stride/format/rotation correctness;
  quality-vs-speed of EXIF previews is mitigated by the round-6 upgrade).

### C — Parallel thumbnail decoding (worker pool)

Decode N thumbnails concurrently (N ≈ cores/2, bounded), separate from the
icon/overlay reader.

- **Effect**: multiplies B by ~4-8× — mainly shortens **whole-folder**
  background completion; visible-first UX is already delivered by A+B.
- **Risk**: high. The reader/panel choreography (`ICSleepSection`, sleep/wake
  full-restart semantics, cache writes, `thumbMaker` reuse) is single-threaded
  by design; parallelizing it safely is a significant rework of 25-year-old
  choreography (constitution III argues against doing this in the same
  increment).

### D — Persistent on-disk thumbnail cache

Store generated thumbnails across sessions (keyed by path+size+mtime), e.g.
under `%LOCALAPPDATA%\Tandem Commander` with a size cap/LRU. Prior art:
`CDiskCache` (temp-file LRU cache, `src/cache.h`), vendored SQLite (currently
read-only consumer, `shiconov.cpp:84-109`), `GetOurPathInRoamingAPPDATA`
(`salamdr5.cpp:1853-1875`). Would also neutralize RC4 (Alt+5 re-entry).

- **Effect**: instant thumbnails on **revisit**; does nothing for the first
  visit (needs A+B anyway).
- **Risk/scope**: a feature of its own — format, invalidation, privacy
  (thumbnails of deleted files persist), size management. Recommended as a
  separate follow-up feature, not part of 064.

### Quick wins bundled with A/B regardless (low risk)

- RC4: preserve the thumbnail cache across an Alt+5 view-mode switch within
  the same folder (carry-over instead of `TemporarilySimpleIcons` forfeit)
  — [to be validated during implementation; if risky, defer].
- Repaint index: remember the cache→panel index once per wake-up or use the
  existing name search instead of the linear scan.

## Comparison & recommendation

| Variant | Meets SC-001/SC-002 (first visit) | Whole-folder completion | Risk | Scope |
|---------|-----------------------------------|------------------------|------|-------|
| A alone | ✗ (visible screen still 15-40 s) | unchanged (~1-2 h/5k) | low-med | small |
| **A+B (recommended)** | ✓ (sub-second typical, ≤ a few s worst) | ~10-100× faster (~minutes) | med | medium |
| A+B+C | ✓ | further ~4-8× (~tens of s) | high | large |
| A+B+D | ✓ + instant revisits | as A+B; revisits instant | med+new persistence | large, D separable |

**Recommendation: A+B** (with the bundled quick wins). It attacks both
verified root causes with the smallest coherent change set, stays inside the
existing thread and plugin contracts, and the already-present two-phase
protocol gives quality upgrades for free. C and D remain natural follow-up
features once A+B's effect is measured.

**Decision (user, 2026-08-19): A+B** — viewport-first scheduling + WIC fast
path with the bundled quick wins. C (parallel pool) and D (persistent cache)
are recorded as candidate follow-up features to be considered after A+B's
effect is measured.

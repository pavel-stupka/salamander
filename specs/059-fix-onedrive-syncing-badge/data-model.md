# Data Model: Sync-In-Progress Badge Parity

**Feature**: 059-fix-onedrive-syncing-badge · **Date**: 2026-08-18

No persistent data. The model is the state mapping and the two runtime gates
added to the existing overlay pipeline (feature 058's model carries over).

## Entities

### Cloud sync state (`PKEY_StorageProviderState`, VT_UI4)
Per-item value supplied by the sync provider through the shell property
system; the documented source of Explorer's state icon.

| Value | SDK name | Explorer display | This feature maps to |
|---|---|---|---|
| 0 | NONE | (no badge) | no badge (unchanged) |
| 1 | SPARSE | cloud/online-only | no badge from fallback (handler-claimed today) |
| 2 | IN_SYNC | green check | no badge from fallback (handler-claimed today) |
| 3 | PINNED | solid green check | no badge from fallback (handler-claimed today) |
| **4** | **PENDING_UPLOAD** | blue arrows | **syncing badge** (files usually handler-claimed first) |
| **5** | **PENDING_DOWNLOAD** | blue arrows | **syncing badge** |
| **6** | **TRANSFERRING** | blue arrows | **syncing badge** |
| 7 | ERROR | red X | no badge from fallback (handler-claimed today; see research R8) |
| 8 | WARNING | warning glyph | no badge from fallback |
| 9 | EXCLUDED | grey minus | no badge from fallback |
| **10** | **PENDING_UNSPECIFIED** | blue arrows | **syncing badge** (the reported folder case) |

Validation rule (FR-001/FR-002): pending family = `{4, 5, 6, 10}`; anything
else — including query failure, `VT_EMPTY`, or unknown future values — yields
today's outcome (`ICONOVERLAYINDEX_NOTUSED`).

### Synthetic overlay entry (`TandemCloudSyncPending`)
One additional `CShellIconOverlayItem`-shaped entry appended after the
registry-sourced handlers in `CShellIconOverlays`:
- `IconOverlayName` = `"TandemCloudSyncPending"` (stable, documented; matched
  by the existing per-handler disable list → FR-011-style governance for free)
- `Identifier` = NULL (never called; the entry only lends its index + icons)
- `IconOverlay[ICONSIZE_16/32/48]` = shipped blue-arrows resource icons
- Never returned by the handler loop — only by the property fallback.
- Excluded from `CreateIconReadersIconOverlayIds` COM instantiation
  (`Identifier == NULL` ⇒ reader array slot stays NULL; loop already
  tolerates NULL slots).

### Per-listing gate (`isCloudSyncRootPath`)
BOOL computed once per icon-reader work cycle beside `isGoogleDrivePath`
(`fileswn1.cpp` icon reader), via `CfGetSyncRootInfoByPath` on the panel's
wide path; `cldapi.dll` resolved once per process (missing DLL ⇒ gate
permanently FALSE ⇒ feature inert).

## State flow (per item, per read cycle)

```
handler loop (unchanged, absolute precedence)
   │ some handler S_OK ──────────────► that handler's index   (unchanged)
   ▼ all declined
isCloudSyncRootPath? ── no ──────────► NOTUSED                (unchanged)
   ▼ yes
read PKEY_StorageProviderState (DELAYCREATION|BESTEFFORT)
   │ value ∈ {4,5,6,10} ─────────────► synthetic entry index  (NEW badge)
   └ else / failure ─────────────────► NOTUSED                (unchanged)
```

Freshness: unchanged mechanisms — `IconOverlayDone` per-cycle semantics,
shell-change notifications (`IconOverlaysChangedOnPath`), manual refresh
(research R7).

## Invariants

- Painting code receives an ordinary overlay index — no renderer changes.
- The fallback runs only in the icon-reader thread (OLE initialized there),
  never on the UI thread.
- No polling, no timers, no caches beyond the existing per-cycle flags
  (SC-004 flat usage).
- Google Drive mounted drives are not CFAPI sync roots ⇒ gate FALSE ⇒
  feature-058 behavior there is bit-for-bit unchanged (US3).

# Data Model: Restore General Shell Icon Overlay Support

**Feature**: 061-fix-icon-overlays | **Date**: 2026-08-19
**Scope**: entities of the icon overlay pipeline as they exist in code, with the
invariants this feature relies on or changes. No storage moves (constitution:
a MINORB release must not move configuration).

## E1 — Overlay handler item (`CShellIconOverlayItem`, `src/shiconov.h:27-40`)

One registered provider loaded into the process.

| Field | Meaning | Constraints |
|---|---|---|
| `IconOverlayName` | Registry subkey name **including leading spaces** | Identity for the disabled list and crash dialog; never trimmed |
| `IconOverlayIdCLSID` | Provider CLSID | From the subkey's default value |
| `Identifier` | `IShellIconOverlayIdentifier*` (main thread) | `NULL` ⇔ synthetic feature-059 entry (skipped in the handler loop) |
| `Priority` | 0–100 (0 = highest) | From `GetPriority`; 100 on failure; used only as query-time filter vs `minPriority` |
| `IconOverlay[3]` | 16/32/48 px badge icons | All three mandatory at load, else the handler is rejected |
| `GoogleDriveOverlay` | Google Drive gate flag | Set by exact-name match at load |

**Lifecycle (load path, `shiconov.cpp:240-352`)**:
`enumerated` → `created` (CoCreateInstance) → `info-queried` (GetOverlayInfo) →
`icons-extracted` (all 3 sizes) → `added` (slot assigned) | `rejected(reason)`.
Invariant: **a rejected handler consumes no slot**. Every rejection has a TRACE reason
(D4/R6 adds the consolidated slot-table summary).

## E2 — Slot table (`CShellIconOverlays`, `src/shiconov.h:42-127`)

Ordered array of E1 items; the array index **is** the badge identity stored per file.

- Capacity: **15** (`Add` refuses at 15) — rooted in the 4-bit
  `CFileData::IconOverlayIndex` (`src/plugins/shared/spl_com.h:228`) with sentinel
  `ICONOVERLAYINDEX_NOTUSED = 15` (`spl_com.h:200`). **Plugin ABI — unchanged by this
  feature (R5).**
- Order: case-insensitive ascending registry-subkey order (Explorer's priority rule).
  Invariant: TC's loaded set ⊇ Explorer's effective set (Explorer's usable third-party
  capacity is below 15) — this is what guarantees the Explorer-as-floor clarification.
- The feature-059 synthetic entry is added first (feature 061: it reserves its slot
  before enumeration — appended-last self-disabled once the icon-extraction fix let
  real handlers fill the table); it is skipped by the query loop, so the ask order of
  real handlers is unchanged. `CloudSyncPendingIndex == -1` still means
  absent/disabled (cldapi unavailable or disabled in configuration).
- Per-icon-reader-thread `IShellIconOverlayIdentifier` instances mirror the table by
  index (`CreateIconReadersIconOverlayIds`); indices must stay stable for the session.

## E3 — Overlay configuration (`Configuration`, `src/cfgdlg.h:422-423`)

| Value (HKCU\…\Configuration) | Type | Absent means | Stored means |
|---|---|---|---|
| `Enable Custom Icon Overlays` | DWORD | **factory default: enabled** (FR-009, changed by this feature — was: force-disabled at config version ≥ 41) | respected as stored |
| `Disabled Custom Icon Overlays` | SZ | factory default: none disabled | `;`-separated E1 names (leading spaces significant), `;;` escapes a literal `;` |

Failure paths (stored but unreadable value / allocation failure) keep the conservative
force-disable — FR-009 heals only *absent* values (R4).

Consumers: `LoadIconOvrlsInfo` (early, before main config load), config page
(`CCfgPageIconOvrls`), crash dialog (writes disable state directly to the registry).

## E4 — Panel item badge state (`CFileData` + icon reader)

- `IconOverlayIndex : 4` — slot index or 15 (= no badge / use built-in marks).
- `IconOverlayDone : 1` — set when the overlay pass answered for this item in the
  current cycle; reset when a new overlay pass is scheduled.
- Produced by the overlay pass (once per work cycle, after icons/thumbnails), via
  `GetIconOverlayIndex` per item: first handler in slot order whose `IsMemberOf`
  (called with the **wide** absolute path) returns S_OK wins; otherwise the
  feature-059 property fallback; otherwise 15.
- Built-in marks (shortcut/share/offline) render only when `IconOverlayIndex == 15` —
  out of scope, must not change (FR-008).

## E5 — Change notification (refresh trigger)

`SHCNE_UPDATEITEM` → `CMainWindow` notification handler (`mainwnd3.cpp:1380-1422`) →
`CFilesWindow::IconOverlaysChangedOnPath(path)` for both panels, for the item's path
and its parent (`CutDirectory`).

**Contract changed by this feature (D1/R3)**: the notified path must reach
`IconOverlaysChangedOnPath` as **UTF-8** (house pattern: wide API + `SalWToU8`), because
the gate `IsTheSamePath(path, GetPath())` compares against the UTF-8 panel path
(feature 004). Coalescing gates (200 ms window, `NextIconOvrRefreshTime`,
`IconCacheValid`) are existing behavior and stay.

## Relationships

```text
Registry HKLM\...\ShellIconOverlayIdentifiers (22 subkeys, alphabetical)
        │ enumerate + load (E1 lifecycle; rejected handlers consume no slot)
        ▼
E2 Slot table (≤15 items; index = badge identity) ←── E3 disabled list filters at load
        │ per item: IsMemberOf(widePath) in slot order
        ▼
E4 CFileData.IconOverlayIndex (4-bit; 15 = none → built-in marks may draw)
        ▲ re-ask (reset IconOverlayDone, wake icon reader)
        │
E5 SHCNE_UPDATEITEM (UTF-8 path after this feature) → IconOverlaysChangedOnPath
```

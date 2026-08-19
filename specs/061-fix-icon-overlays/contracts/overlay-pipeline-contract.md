# Contract: Shell Icon Overlay Pipeline

**Feature**: 061-fix-icon-overlays | **Date**: 2026-08-19
**Style**: internal behavioral contract, same genre as
`specs/058-fix-cloud-status-icons/contracts/path-encoding-icon-pipeline.md`.
Binding for this feature's implementation and for future changes to the overlay path.

## C1 — Handler selection

1. Providers are enumerated from
   `HKLM\Software\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers`
   in case-insensitive ascending subkey order; the subkey name **including leading
   spaces** is the provider's identity everywhere (disabled list, crash dialog, traces).
2. A provider enters the slot table only after the full load ladder succeeds:
   instance created → `GetOverlayInfo` returns S_OK with an icon location → all three
   badge sizes (16/32/48) extracted. **A provider that fails any rung consumes no
   slot.**
3. The slot table holds at most **15** providers (plugin-ABI 4-bit index; sentinel 15 =
   no badge). The feature-059 synthetic entry is added **before** the registered
   providers are enumerated, so it owns a slot even on a machine with 15+ loadable
   providers (amended by feature 061 — the original appended-last design silently
   self-disabled the sync-pending badge once the icon-extraction fix let the table
   fill up). The query loop skips the entry, so the ask order of real providers is
   unchanged.
4. **Floor guarantee**: because ordering is Explorer's and capacity (15) is not below
   Explorer's effective third-party capacity, every provider Explorer displays is in
   the table. Providers beyond Explorer's cutoff MAY be in the table (clarified
   Explorer-as-floor semantics).
5. Disabled providers (E3 list) are skipped at load and never occupy a slot.

## C2 — Per-item query

1. `IsMemberOf` is always called with the item's **wide** absolute path (UTF-8 panel
   data converted via `SalU8ToW`, CP_ACP fallback for legacy callers).
2. Slot order = ask order; first S_OK wins; ties are impossible by construction.
3. The `Priority > minPriority` filter and the Google-Drive path gate are load-bearing
   existing behavior and stay unchanged.
4. The feature-059 property fallback runs only when no provider claimed the item and
   the path is under a CFAPI sync root.
5. A provider that crashes or misbehaves during a query degrades only its own badge;
   the exception path (SEH + crash dialog with disable offer) stays.

## C3 — Change-notification refresh (changed by this feature)

1. Every path extracted from a shell change notification (`SHCNE_UPDATEITEM` and
   siblings in `CMainWindow`'s handler) MUST be produced by a **wide** shell API and
   converted to UTF-8 (`SalWToU8`, CP_ACP fallback) before reaching any consumer that
   compares it against feature-004 UTF-8 panel state (`IsTheSamePath(path, GetPath())`
   in `IconOverlaysChangedOnPath`, and any sibling consumer found by the Phase A
   audit).
2. Rationale: the badge-refresh loop is the designed mechanism for asynchronous
   providers (Tortoise answers S_FALSE first, then notifies); an encoding mismatch
   silently kills it for non-ASCII paths — the same defect class feature 058 fixed in
   the icon-reader, `geticon.cpp`, and `snooper.cpp`.
3. Existing coalescing behavior (200 ms window, refresh deferral while icons are
   loading) is unchanged.

## C4 — Configuration semantics (changed by this feature)

1. `Enable Custom Icon Overlays` / `Disabled Custom Icon Overlays` **absent** ⇒
   factory default (enabled / none disabled) — regardless of config version. This
   heals profiles produced by the feature-057 migration (FR-009, SC-007).
2. A **stored** value is always respected as stored; a stored-but-unreadable value
   keeps the legacy conservative force-disable.
3. Storage location, value names, formats, and the restart-required semantics of the
   config page are unchanged (constitution: MINORB releases must not move
   configuration).

## C5 — Diagnostics (Debug builds only, per clarification)

1. Every load-ladder rejection carries a TRACE naming the provider and the rung.
2. After initialization, one consolidated TRACE line lists the final slot table
   (index, provider name, priority).
3. No Release-visible diagnostic surface is added (no UI, no log file, no strings).

## Out of scope (guarded, not changed)

- Built-in status marks (shortcut arrow, shared hand, offline clock) — separate
  mechanism, drawn only when no provider badge exists (FR-008).
- Plugin ABI: `CFileData::IconOverlayIndex : 4` and `ICONOVERLAYINDEX_NOTUSED = 15`
  stay byte-identical (`src/plugins/shared/spl_com.h`).

# Quickstart: Validating Sync-In-Progress Badge Parity

**Feature**: 059-fix-onedrive-syncing-badge
**Proves**: spec SC-001…SC-005. Mechanism and evidence: [research.md](research.md).

## Prerequisites

- Windows 11, VS2022, repo on branch `059-fix-onedrive-syncing-badge`.
- OneDrive (personal or business) signed in with Files-On-Demand — any synced
  library works; the reported one is
  `E:\Simopt, s.r.o\HSČR - Dokumenty\Schůzky`.
- Explorer window and a Tandem Commander panel side by side on the same
  folder.

## Build

```batch
build.cmd full
```

Run `%OPENSAL_BUILD_DIR%\...\Debug_x64\tandemcommander.exe` (default
`.\build\tandemcommander\Debug_x64\`).

## 1. Deterministic pending-state repro (SC-001)

Real stalls are transient (the reported one drained during analysis), so
create the state on demand:

1. OneDrive tray icon → Settings gear → **Pause syncing** (2 hours).
2. In the sync root, create a new subfolder with a copy of any document, and
   modify another existing file.
3. **Explorer**: the touched items and their parent folder show the blue
   pending arrows (column "Stav dostupnosti" = "Čekající synchronizace").
4. **Tandem Commander (fixed build)**: the same items show the syncing badge
   — item-by-item parity, folders included (SC-001; on-screen items within
   the feature-058 2-second target).
5. Pre-fix control (optional): the previous build shows folder badges missing
   while Explorer shows arrows.

## 2. Full-cycle transition (SC-002)

1. With the panel open, **Resume syncing** in the OneDrive tray menu.
2. Watch both windows: arrows disappear in Explorer as items settle; the
   panel follows on its change notification or, at the latest, on a manual
   refresh (Ctrl+R). Record which of the two mechanisms fired (research R7).
3. Edit a synced document and save: synced → pending → synced sequence
   matches Explorer at each panel refresh point.

## 3. No-regression suite (SC-003)

- Re-run the feature-058 validation set
  (`specs/058-fix-cloud-status-icons/evidence.md` scenarios): badges in
  `G:\Můj disk` (Google Drive — non-CFAPI, fallback must stay inert),
  OneDrive ASCII + diacritic folders, base icons, auto-refresh, activation
  behavior — all unchanged.
- Configuration → icon overlays **off** → no badges anywhere (including the
  new one); **on** → badges return. Optionally add
  `TandemCloudSyncPending` to the per-handler disable list → only the new
  badge disappears.
- Plain local folder (`D:\Test\Control`): behavior and listing speed
  unchanged.

## 4. Stalled-provider endurance (SC-004)

With syncing paused (pending badges showing), leave the panel open ~1 hour:
no busy cursor, no CPU/memory growth attributable to the panel (Task
Manager), badges simply persist — then resume and confirm they clear.

## 5. Analysis record check (SC-005)

`research.md` R1/R2 hold the reported-location determination (channel =
`PKEY_StorageProviderState`; the stall was provider-real — a pending upload
that later drained) and the re-runnable probes. User guidance for future
stalls: check the OneDrive activity center, close the app holding the
document (Word locks defer uploads), or pause/resume syncing.

## 6. Automated gates

```batch
build.cmd full            :: Debug clean
build.cmd full release    :: Release clean
:: saltests: build src\vcxproj\saltests\saltests.vcxproj (Debug x64) and run — expect baseline all-pass
```

## Release note (when shipping)

CHANGELOG `[Unreleased]`: the sync-in-progress (blue arrows) badge now shows
in cloud-synced folders exactly as in Explorer — including on folders whose
contents are pending — a state that was never displayed, even before the
Open Salamander fork.

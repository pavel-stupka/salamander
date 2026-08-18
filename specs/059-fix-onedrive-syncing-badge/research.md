# Phase 0 Research: Sync-In-Progress Badge Parity with Explorer

**Feature**: 059-fix-onedrive-syncing-badge · **Date**: 2026-08-18
**Method**: live experiments on the reporting user's machine (COM probes of
every registered OneDrive overlay handler, shell property-store queries with
flag matrix, icon extraction with visual identification, Windows SDK header
verification). All probes are re-runnable; commands are preserved in this
session's history and summarized per finding.

## R1. The two channels — and which one carries the blue arrows

Windows exposes cloud sync state to Explorer through **two independent
channels**:

1. **Icon-overlay handlers** (`IShellIconOverlayIdentifier::IsMemberOf`) —
   the only channel Salamander/Tandem Commander has ever consulted (the
   feature-058 pipeline).
2. **The shell property system** — `PKEY_StorageProviderState`
   (`System.StorageProviderState`, VT_UI4), documented in the Windows SDK
   `propkey.h` as *"Property for the cloud file state icon."* Explorer's
   "Availability status" column and its per-item state icons are built from
   it (the serialized `System.StorageProviderUIStatus` blob read from the
   items literally references `prop:System.StorageProviderState;…`).

**Decisive experiment** (COM probe of all seven registered OneDrive handlers,
called exactly as the product calls them, on the reported folder while
Explorer showed blue arrows):

| Item (state at probe time) | OneDrive1–7 `IsMemberOf` | `StorageProviderState` |
|---|---|---|
| `…\Schůzky\2026_08_19` (blue arrows in Explorer) | **all S_FALSE** | **10 = PENDING_UNSPECIFIED** |
| `…\2026_08_19\HSČR Schůzka 19.8.2026.docx` | **OneDrive5 S_OK** (icon = blue arrows) | 4 = PENDING_UPLOAD |
| `…\Schůzky\2026_06_22` (control, synced) | OneDrive4 S_OK (green check) | 3 = PINNED |
| `…\TC-Test-Zkouška\test.docx` (other sync root) | OneDrive7 S_OK | 2 = IN_SYNC |

Conclusion: **files** in an active pending state are claimed by a handler
(OneDrive5 — whose overlay icon, extracted and visually verified, IS the blue
circular arrows, `FileSyncShell64.dll` index 1), so Tandem Commander already
shows arrows on such files since feature 058. **Folders** (and items in
`PENDING_UNSPECIFIED`) are claimed by **no handler** — their arrows exist
only in the property channel. That is why neither Altap Salamander nor Tandem
Commander ever showed the folder's syncing badge: the overlay-only pipeline
cannot see it. Explorer parity for this state requires reading
`PKEY_StorageProviderState`.

`propkey.h` (SDK 10.0.26100) documents the full value set:
`0 NONE, 1 SPARSE, 2 IN_SYNC, 3 PINNED, 4 PENDING_UPLOAD,
5 PENDING_DOWNLOAD, 6 TRANSFERRING, 7 ERROR, 8 WARNING, 9 EXCLUDED,
10 PENDING_UNSPECIFIED`. The "blue arrows" (pending/syncing) family is
**{4, 5, 6, 10}**.

Attributes are NOT a usable channel: the pending folder and a synced sibling
carried byte-identical attributes (0x80431), and `System.FilePlaceholderStatus`
was 15 for both.

## R2. US2 — diagnosis of the reported location (finding: the stall was real)

- At probe time, OneDrive's own store reported the folder as
  `PENDING_UNSPECIFIED` (10) and the document inside as `PENDING_UPLOAD` (4)
  — i.e. **a genuinely pending upload**, hours after the file's creation
  (created 9:20, still pending mid-afternoon; both OneDrive processes running
  since 7:53). Explorer's arrows were truthful; nothing was wrong with
  Explorer or the file manager.
- **During the analysis session the state drained on its own**: a re-check
  showed the folder, the sibling and the document all at
  "Vždy k dispozici na tomto zařízení" (PINNED family). Long-lived pending
  uploads of Office documents most commonly mean the document was held open
  by Word (Office lock defers the upload) or the client's queue was busy.
- **User guidance** (recorded per FR-006): when arrows persist for hours,
  check the OneDrive activity center (tray icon) for the file, close the
  application holding the document, or pause/resume syncing; the state is the
  provider's, and Tandem Commander (like Explorer) only mirrors it.
- **Deterministic repro for validation** (since live stalls are transient):
  pause syncing in the OneDrive tray menu, then create/modify a file in the
  sync root — Explorer immediately shows pending arrows on the file and its
  parent folders until syncing is resumed. This drives quickstart SC-001/002.

## R3. Decision: property-fallback in the overlay step (the conservative fix)

**Decision**: keep the feature-058 handler pipeline exactly as is; add a
**fallback** in `CShellIconOverlays::GetIconOverlayIndex` (`src/shiconov.cpp`):
when **every** handler has declined an item (today's `ICONOVERLAYINDEX_NOTUSED`
outcome) **and** the panel path lies under a cloud-files sync root, read the
item's `PKEY_StorageProviderState` and, if the value is in the pending family
{4, 5, 6, 10}, return a synthetic "cloud sync pending" overlay entry. All
other values (and any query failure) keep today's "no badge" outcome.

**Rationale**:
- Handlers keep absolute precedence → zero behavioral change for every state
  that works today (FR-004); the fallback can only *add* the one missing
  badge.
- Explorer-faithful: the property is the exact source Explorer documents and
  uses for its state icon.
- Provider-independent: works for any CFAPI sync root; inert elsewhere.

**Alternatives considered**:
- *Query the property first (Explorer-style precedence)* — rejected: changes
  the code path for states that already work; higher risk for no user-visible
  gain (both channels agree where they overlap).
- *Derive from file attributes / `CfGetPlaceholderInfo`* — rejected: measured
  attributes and placeholder status do not carry the pending state at all
  (R1).
- *Parse the localized "Availability status" column text* — rejected:
  locale-dependent, string-fragile; the numeric PKEY is the canonical source.
- *Do nothing (documented limitation per FR-007)* — rejected: the state IS
  obtainable through a supported, documented channel (proven empirically),
  so FR-007's escape clause does not apply.

## R4. Property query mechanics (measured)

- API: `SHGetPropertyStoreFromParsingName(wPath, NULL, flags,
  IID_IPropertyStore, …)` + `GetValue(PKEY_StorageProviderState)`. The PKEY
  is a compile-time constant (`INIT_PKEY_StorageProviderState`, propkey.h) —
  **no propsys.dll import needed**; read `PROPVARIANT.ulVal` for `VT_UI4`
  directly and treat any other `vt` as "no state".
- **Flags — measured matrix** (broken-content .docx and a plain local file as
  robustness probes):

  | Flags | pending dir | broken docx | local file | verdict |
  |---|---|---|---|---|
  | `GPS_DEFAULT` | state ✔ | **fails 0x80030050** | fails | file-format property handlers initialize and can fail the whole store |
  | `GPS_FASTPROPERTIESONLY` | empty | empty | empty | state is not a "fast" property |
  | `GPS_VOLATILEPROPERTIESONLY` | empty | empty | empty | not volatile either |
  | **`GPS_DELAYCREATION`** | **state ✔** | **state ✔** | state=0 | handlers init lazily; the file-format handler never runs for this key |
  | `GPS_DELAYCREATION\|GPS_BESTEFFORT` | state ✔ | state ✔ | state=0 | belt-and-braces |

  **Decision**: `GPS_DELAYCREATION | GPS_BESTEFFORT`. Critically, the
  file-format (content) property handler does not execute → **no risk of
  hydrating online-only files (058 FR-005) and no failure on malformed
  documents**.
- **Cost** (warm, this machine): ~6 ms per file, ~13 ms per directory. Runs
  only in the icon-reader background thread (which already initializes OLE),
  only for items no handler claimed, only under a sync root, once per read
  cycle per item (`IconOverlayDone` semantics unchanged).

## R5. Sync-root gate (keeps non-cloud folders at zero cost)

**Decision**: once per listing (same place the feature-058 pipeline computes
`isGoogleDrivePath`, `src/fileswn1.cpp:~502`), determine whether the panel
path lies under a cloud-files sync root via **`CfGetSyncRootInfoByPath`**
(`cldapi.dll`, present on all Windows 11; loaded dynamically once, pattern
identical to the product's other optional-DLL loads). Pass the boolean into
`GetIconOverlayIndex` alongside the existing `isGoogleDrivePath`.

- Plain local/network folders: gate false → fallback never runs → FR-009/058
  semantics untouched (SC-004 flat usage).
- Google Drive mounted drive (`G:`): not a CFAPI sync root → gate false →
  feature-058 behavior there is bit-for-bit unchanged (US3).
- Panel paths are UTF-8 → the gate call uses the already-converted `wPath`
  (feature-058 contract; no new conversions).

## R6. Badge artwork for the synthetic overlay

**Decision**: ship a small "sync pending" overlay icon (blue circular arrows,
16/32/48 px, drawn in the Windows badge style) as a salamand resource, added
through the existing brand pipeline (`tools/brand/`), and register it as one
**synthetic entry** appended to `CShellIconOverlays` after the registry
handlers, with the internal name `TandemCloudSyncPending`.

**Rationale**: the natural alternative — reusing the registered OneDrive
handler's arrows icon — requires knowing *which* of the seven anonymous
handlers is "syncing"; that mapping is only discoverable by icon-index
convention (`FileSyncShell64.dll#1` today) and breaks silently on any OneDrive
update, and there is no handler at all to borrow from on machines where a
different CFAPI provider owns the sync root. A shipped icon is deterministic,
provider-independent, and renders through the existing overlay drawing path
with **no changes to painting code** (the fallback returns a normal overlay
index).

**Configuration (FR-011 / 058-FR-011)**: the synthetic entry participates in
the existing governance for free — the global "enable icon overlays" switch
already gates the entire overlay-reading step, and the per-handler disable
list matches by name, so `TandemCloudSyncPending` is individually
disable-able through the existing UI with zero new configuration.

## R7. Freshness (FR-003)

State transitions do not necessarily change file attributes or timestamps, so
the snooper alone may not fire. Two existing mechanisms cover FR-003 without
new code: (a) the product's shell-change-notification route
(`CFilesWindow::IconOverlaysChangedOnPath`, driven from the main window's
`SHChangeNotify` registration — the same route that refreshes Tortoise
overlays; OneDrive raises `SHCNE_UPDATEITEM` on state changes), and (b)
manual refresh / re-listing. SC-002 validates (a) empirically during a real
pause→edit→resume cycle; if (a) proves not to fire for property-only changes
on some Windows build, FR-003's "manual refresh" arm still holds and the
finding is recorded in evidence (no polling is added either way).

## R8. Out-of-scope notes

- Other unclaimed property states (7 ERROR, 8 WARNING, 9 EXCLUDED) map to no
  badge — exactly today's behavior; OneDrive's error states were observed to
  be handler-claimed. Extending the mapping is a one-line follow-up if a real
  parity gap is ever reported.
- `PKEY_StorageProviderTransferProgress` (per-item progress %) — displaying
  progress is beyond badge parity.
- The GDrive handler-name modernization remains out of scope (058 R6).

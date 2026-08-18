# Implementation Plan: Sync-In-Progress Badge Parity with Explorer

**Branch**: `059-fix-onedrive-syncing-badge` | **Date**: 2026-08-18 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/059-fix-onedrive-syncing-badge/spec.md`

## Summary

Explorer's blue "sync pending" arrows come from a channel the product has
never consulted: the shell property `PKEY_StorageProviderState` (documented in
the SDK as *"Property for the cloud file state icon"*). Live probes on the
reporting machine proved that **no icon-overlay handler claims a folder in the
pending state** (all seven OneDrive handlers return S_FALSE), while the
property reports `PENDING_UNSPECIFIED (10)` — which is why the folder badge
was missing in Altap Salamander and Tandem Commander alike. Pending **files**
are already claimed by a handler (blue-arrows icon) and display correctly
since feature 058.

The conservative fix: in `CShellIconOverlays::GetIconOverlayIndex`, when every
handler declines an item **and** the panel path lies under a cloud-files sync
root (detected once per listing via `CfGetSyncRootInfoByPath`), read the
item's `PKEY_StorageProviderState` (`SHGetPropertyStoreFromParsingName` with
`GPS_DELAYCREATION | GPS_BESTEFFORT` — measured safe: no content handlers run,
no hydration risk, ~6–13 ms per item in the background icon-reader thread) and
map the pending family `{4 PENDING_UPLOAD, 5 PENDING_DOWNLOAD,
6 TRANSFERRING, 10 PENDING_UNSPECIFIED}` to a new synthetic overlay entry
`TandemCloudSyncPending` whose blue-arrows icon ships as a salamand resource
(brand pipeline). Handlers keep absolute precedence; every other state and
every non-sync-root folder is bit-for-bit unchanged. The user's "stuck sync"
suspicion was confirmed real and provider-side — the pending upload drained
hours later on its own; guidance is recorded per FR-006. Full evidence:
[research.md](research.md).

## Technical Context

**Language/Version**: C++ (C++20, `/std:c++latest`), MSVC v143 (VS2022)
**Primary Dependencies**: Pure WinAPI — `SHGetPropertyStoreFromParsingName` +
`IPropertyStore` (shell32/ole32, already linked), `PKEY_StorageProviderState`
compile-time constant from `propkey.h` (no propsys import),
`CfGetSyncRootInfoByPath` from `cldapi.dll` (loaded dynamically once; present
on Windows 11)
**Storage**: N/A — no configuration changes; the synthetic overlay obeys the
existing icon-overlay settings by name (`TandemCloudSyncPending`)
**Testing**: full Debug+Release build; `saltests` unchanged-pass; manual
validation per [quickstart.md](quickstart.md) with a deterministic repro
(pause OneDrive → edit file → Explorer shows arrows → compare)
**Target Platform**: Windows 11+ (x64)
**Performance Goals**: property fallback only for handler-declined items
under a sync root, ≤ ~13 ms/item in the background reader (measured warm);
zero added cost outside sync roots (SC-004); feature-058 2-second visible-item
target unchanged
**Constraints**: conservative per spec — handler pipeline untouched, painting
code untouched (fallback returns an ordinary overlay index), no new
configuration, no polling; icon added via the established brand pipeline
**Scale/Scope**: `src/shiconov.cpp/.h` (fallback + synthetic entry),
`src/fileswn1.cpp` (per-listing sync-root flag, one call site), one new icon
resource in `src/` + `tools/brand/` source — 3 code files + assets

## Constitution Check

| # | Principle | Verdict | Notes |
|---|-----------|---------|-------|
| I | Build Reproducibility | ✅ Pass | New icon generated/committed via existing `tools/brand/gen_icons.py` flow; no build-system changes. |
| II | Backward Compatibility | ✅ Pass | Purely additive badge state; handler precedence preserved; no config/registry moves. |
| III | Incremental Modernization | ✅ Pass | One fallback in the existing overlay step + one gate flag, mirroring the `isGoogleDrivePath` pattern already in place. |
| IV | Windows Platform Commitment | ✅ Pass | Documented shell property + Cloud Files API; Win11 baseline satisfies `cldapi.dll`. |
| V | Plugin Architecture Preservation | ✅ Pass | No plugin-facing API change; `LAST_VERSION_OF_SALAMANDER` untouched. |
| VI | UI Consistency | ✅ Pass | Badge rendered by the existing overlay painter; icon drawn in the Windows badge style. |
| — | Release Documentation | ⚠️ Deferred to release | `CHANGELOG.md` entry (Fixed/Added) required with the change that ships it. |

**Post-Phase-1 re-check**: no new projects, no new external dependencies
(cldapi loaded dynamically), no API-shape changes — all gates still pass.

## Project Structure

### Documentation (this feature)

```text
specs/059-fix-onedrive-syncing-badge/
├── plan.md              # This file
├── research.md          # Phase 0: channel identification, flag matrix, diagnosis
├── data-model.md        # Phase 1: state mapping, synthetic entry, gates
├── quickstart.md        # Phase 1: deterministic repro + validation guide
├── contracts/
│   └── cloud-state-badge-fallback.md   # Phase 1: fallback contract
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
src/
├── shiconov.h           # + synthetic-entry state, GetIconOverlayIndex gains
│                        #   isCloudSyncRootPath param (beside isGoogleDrivePath)
├── shiconov.cpp         # + InitCloudSyncPendingOverlay (loads shipped icon,
│                        #   appends synthetic entry, honours disable list);
│                        #   + property fallback in GetIconOverlayIndex
│                        #   (SHGetPropertyStoreFromParsingName, DELAYCREATION|
│                        #   BESTEFFORT, PKEY_StorageProviderState, map {4,5,6,10});
│                        #   + dynamic cldapi.dll load + IsCloudSyncRootPath helper
├── fileswn1.cpp         # icon reader: compute isCloudSyncRootPath once per
│                        #   listing (next to isGoogleDrivePath, ~line 502) and
│                        #   pass it through (wPath already available post-058)
├── lang/ / res          # new overlay icon resource (16/32/48) in salamand
└── tools/brand/         # icon source + regeneration entry (README note)
```

**Structure Decision**: single existing project; no new translation units —
the fallback lives in `shiconov.cpp` where the overlay pipeline already is.

## Complexity Tracking

No constitution violations — table not needed.

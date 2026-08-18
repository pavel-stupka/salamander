# Contract: Cloud-State Badge Fallback

**Feature**: 059-fix-onedrive-syncing-badge · **Status**: implemented
(2026-08-18) — every rule below cross-checked against the final diff.
Implementation deviations from the plan, both recorded in evidence.md:
`cfapi.h` cannot be included at the project's `_WIN32_WINNT` level, so the
two needed ABI-stable declarations are mirrored locally in `shiconov.cpp`;
and integrating the NULL-`Identifier` synthetic entry exposed a latent
upstream bug (uninitialized `HRESULT res` in `GetIconOverlayIndexAuxAux`
when a reader slot is NULL — RTC startup crash), fixed by initializing
`res = S_FALSE` and skipping synthetic entries in the handler loop.
**Scope**: core overlay pipeline (`src/shiconov.cpp/.h`, icon reader in
`src/fileswn1.cpp`). No plugin-facing surface.

## Rules

1. **Precedence**: registered icon-overlay handlers are consulted first,
   exactly as in feature 058. The property fallback runs only when every
   handler declined the item.
2. **Gate**: the fallback runs only when the panel path lies under a
   cloud-files sync root, determined once per listing by
   `CfGetSyncRootInfoByPath` on the converted wide panel path. `cldapi.dll`
   is loaded dynamically once per process; if unavailable, the fallback is
   permanently inert.
3. **Source of truth**: `PKEY_StorageProviderState`
   (`{E77E90DF-6271-4F5B-834F-2DD1F245DDA4}, 3`, VT_UI4), read via
   `SHGetPropertyStoreFromParsingName` with
   `GPS_DELAYCREATION | GPS_BESTEFFORT`. These flags are part of the
   contract: they are what prevents file-format property handlers from
   running (no hydration of online-only content — 058 FR-005; no failures on
   malformed documents). The PKEY constant comes from `propkey.h`
   (`INIT_PKEY_StorageProviderState`); `PROPVARIANT.ulVal` is read directly
   for `vt == VT_UI4`, any other `vt` counts as "no state".
4. **Mapping**: values `{4, 5, 6, 10}` (PENDING_UPLOAD, PENDING_DOWNLOAD,
   TRANSFERRING, PENDING_UNSPECIFIED) → the synthetic
   `TandemCloudSyncPending` overlay entry. **Every other value, unknown
   future values, and every failure path → `ICONOVERLAYINDEX_NOTUSED`**
   (today's behavior). The fallback can only add the pending badge, never
   change or remove another.
5. **Threading**: fallback executes only in icon-reader threads (OLE-STA
   initialized there); never on the UI thread; SEH-guarded like the
   neighboring handler calls.
6. **Configuration**: the synthetic entry is governed by the existing
   settings — the global icon-overlay switch gates the whole overlay step,
   and the name `TandemCloudSyncPending` participates in the per-handler
   disable list. No new configuration exists.
7. **Artwork**: the badge icon ships in salamand resources (16/32/48),
   sourced through `tools/brand/`; the painter is unchanged and draws the
   synthetic entry via its ordinary overlay index.

## Non-goals

- No property queries for handler-claimed items; no Explorer-precedence
  emulation beyond the pending family; no progress display
  (`PKEY_StorageProviderTransferProgress`); no changes on non-CFAPI roots
  (Google Drive letter drives); no provider management (stuck queues are the
  provider's to drain — user guidance lives in research R2/quickstart).

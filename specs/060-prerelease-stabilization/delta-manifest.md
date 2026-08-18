# Delta Manifest — review scope for feature 060

**Commit range**: `b74875b..HEAD` (baseline = release 0.1.2, build 186).
Commits: `4b469b1` (056 docs), `e1a1650` (057 Altap settings migration),
`4a00dc8` (058 non-ASCII path fixes), `1548d92` (059 sync-pending badge).
Inspect the exact changes with `git diff b74875b..HEAD -- <file>` from the
repo root `D:\Projects\tandemcommander`.

## Delta files (18)

Core C++ (058+059):
- `src/shiconov.cpp` (+215: synthetic overlay entry `TandemCloudSyncPending`,
  `InitCloudSyncPendingOverlay`, `LoadCloudSyncPendingIcons`, property
  fallback `GetCloudSyncPendingStateAux(AuxAux)`, `IsCloudSyncRootPath` +
  dynamic cldapi load, loop-skip for `Identifier == NULL`, `res = S_FALSE`
  init fix, `ColorsChanged` NULL-Identifier branch)
- `src/shiconov.h` (+17: `CloudSyncPendingIndex`, new method decls, new
  `GetIconOverlayIndex` param)
- `src/snooper.cpp` (three sites converted to `FindFirstChangeNotificationW`
  with `SalU8ToW` + CP_ACP fallback)
- `src/fileswn1.cpp` (icon-reader wide-prefix `SalU8ToW` conversion +
  `wName` offset from wide length; `isCloudSyncRootPath` per-cycle gate)
- `src/geticon.cpp` (`SHILCreateFromPath`: `SalU8ToWAlloc` first, CP_ACP
  fallback, free on all paths)
- `src/common/handles.cpp/.h` (`FindFirstChangeNotificationW` overload in
  the HANDLES tracking layer)

Resources / dev tooling (059):
- `src/res/syncpend.ico` (new, 16/32/48 BMP frames, Pillow-generated)
- `src/resource.rh2` (`IDI_SYNCPENDING 984`), `src/salamand.rc2` (entry)
- `tools/brand/gen_overlay_syncpend.py` (+86, dev-only generator),
  `tools/brand/README.md`

Standalone utility (057, never yet reviewed in-session):
- `utils/migrate-altap-settings.cmd` (+1,339 — reads Altap/Servant
  Salamander registry config incl. stored FTP passwords, writes into
  `HKCU\Software\Tandem Commander\0.1`, creates backup + restore script)
- `utils/test/run_migration_tests.cmd` (+374), `utils/test/fixtures/*.reg`
  (3 files), `utils/README.md`

Docs (no line review): CHANGELOG.md, CLAUDE.md, specs/058-*, specs/059-*.

## Seeded questions (verify, refute, or go beyond)

1. `SalLoadIcon` (`src/salamdr1.cpp:1055`) leaves `hIcon` **uninitialized**
   when `LoadIconWithScaleDown` fails; 059's `LoadCloudSyncPendingIcons`
   assumes NULL-or-valid. Does `LoadIconWithScaleDown` guarantee writing
   the out param on failure? What happens downstream with a garbage handle?
2. `InitCloudSyncPendingOverlay`: after `Add(item)` fails, code does
   `delete item` — does `Add()` ever keep the pointer on failure
   (`TIndirectArray` semantics)? Double-free/ownership risk?
3. `CloudSyncPendingIndex` + `CfGetSyncRootInfoByPathDyn` are written during
   `InitShellIconOverlays` and read by icon-reader threads without explicit
   synchronization. Is init strictly ordered before any reader thread starts
   on ALL paths (including panel re-creation, config reload,
   `ReleaseShellIconOverlays` + re-init)?
4. The 059 property fallback in `GetIconOverlayIndex` runs AFTER the handler
   loop — confirm it can never run while the Google Drive critical section
   (`GD_CS`) is still held.
5. 058 snooper fallback: `MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, path,
   -1, wPath, _countof(wPath))` — on failure (0), what is in `wPath`, and is
   it terminated before `FindFirstChangeNotificationW` reads it?
6. 057: can any code path write a stored password to console output, a log,
   a world-readable temp file, or leave it in the backup/restore artifacts
   with weaker protection than the registry origin?
7. 058 `fileswn1.cpp`: the CP_ACP fallback writes `wPath[wl] = 0` after
   `MultiByteToWideChar(..., l, wPath, MAX_PATH + 10 - 1)` — bounds-safe for
   all `l`? And `wName = wPath + (wl - 1)` when the fallback also fails
   (wl==0 → wl becomes 1 → wName = wPath) — downstream safe?
8. `GetCloudSyncPendingStateAuxAux`: PROPVARIANT handling — is
   `PropVariantClear` guaranteed on every path where `GetValue` succeeded?
   Is `store->Release()` guaranteed when `SHGetPropertyStoreFromParsingName`
   returned success but a later step throws?

## Output format required from every perspective

For each finding: `location (file:line)` · one-sentence claim · concrete
failure scenario (input/state → wrong outcome) · confidence (high/medium/
low). Also list the files you actually examined (coverage list). If you
find nothing in a charter area, say so explicitly — a clean verdict with
coverage is a result. You are READ-ONLY: never edit files.

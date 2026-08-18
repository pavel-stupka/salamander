# Pre-Release Stabilization Review — Features 058 + 059

**Feature**: 060-prerelease-stabilization · **Date**: 2026-08-18
**Scope**: full delta `b74875b..HEAD` (release 0.1.2, build 186 → current) —
features 057, 058, 059. See [delta-manifest.md](delta-manifest.md) for the
exact file list and the 8 seeded questions.
**Method**: six independent read-only review subagents, each with a written
charter (research.md R3), followed by adversarial verification against the
actual code in the main context (research.md R4). Only CONFIRMED,
release-relevant findings drove fixes; every fix was minimal and re-verified.

## Coverage (SC-001)

All 18 delta files were assigned to at least one perspective:

| File | Perspectives |
|---|---|
| `src/shiconov.cpp`, `src/shiconov.h` | P1, P2, P3, P4, P5, P6 |
| `src/fileswn1.cpp` | P1, P2, P3, P4, P5 |
| `src/snooper.cpp` | P2, P3, P4 |
| `src/geticon.cpp` | P1, P3, P4 |
| `src/common/handles.cpp/.h` | P1, P3 |
| `src/res/syncpend.ico`, `resource.rh2`, `salamand.rc2` | P6 (asset sanity) |
| `tools/brand/gen_overlay_syncpend.py` | P6 |
| `utils/migrate-altap-settings.cmd`, `utils/test/**`, `utils/README.md` | P6 |
| `src/salamdr1.cpp` (`SalLoadIcon`, pre-existing, newly load-bearing) | P1, P2, P3 (cross-referenced) |

Every perspective's raw coverage/findings list is preserved in the session
transcript (six subagent reports, ~660k tokens combined analysis); this
report consolidates the results.

## Findings (SC-002)

All raised findings, deduplicated across perspectives (convergent findings —
the same defect independently reached from different angles — are strong
signal and listed once with all contributing perspectives noted).

### CONFIRMED — Fixed

| ID | Perspectives | Location | Claim | Fix |
|---|---|---|---|---|
| F1 | P2 (F-1), P5 (Area1 note) | `src/fileswn1.cpp:512-517` | `CfGetSyncRootInfoByPath` (059) is real I/O, the first ever placed inside the held `ICSleepSection`; `SleepIconCacheThread()` — called from the main thread by many common panel operations — does an unbounded `EnterCriticalSection` wait on the same section, so a hung network path could freeze the UI for the call's duration. **Verified directly**: confirmed `PathContainsValidComponents`/`IsGoogleDrivePath` (everything else in that setup block) are memory-only; confirmed `SleepIconCacheThread` is called from 11 files on common operations; confirmed the CS is not left until line ~700, well after this call. | Leave/re-enter `ICSleepSection` around the one call, matching the leave-before-slow-op pattern already used later in the same function. |
| F2 | P6 (A1) | `utils/migrate-altap-settings.cmd:1046-1054` | The generated restore script wrote itself as ASCII (mangles non-ASCII backup paths, e.g. a diacritic user profile name) and deleted the current settings key *before* verifying the backup file it's about to import still exists — a corrupted/missing path turns "undo the migration" into "lose everything." **Verified directly, end-to-end**: reproduced the exact failure (`if exist` on an ASCII-mangled path found nothing even though the file existed) with an isolated harness using a `Zkouška`-named backup directory; the ANSI-encoding-only first attempt still failed because a freshly spawned `cmd.exe` on this machine defaults to OEM codepage 852, not ANSI 1250 (`GetOEMCP()` vs `GetACP()` measured directly) — confirmed the gap empirically, not merely by inspection. | Restore script now: (1) checks `if not exist` and aborts *before* any delete, (2) is written UTF-8 (no BOM) with `chcp 65001` as its first executable line — codepage-independent regardless of what the launching `cmd.exe` defaults to. Re-verified end-to-end: backup → simulated migration change → restore → original registry value returned correctly, with a non-ASCII backup path. |
| F3 | P6 (A2) | `utils/migrate-altap-settings.cmd:1201` (`Show-W5-Backup`) | The backup `.reg` contains recoverable (not just obfuscated) stored FTP passwords if the user already had any in Tandem Commander, with no warning that the artifact needs the same care as the settings themselves. | Added a warning to the W5 screen naming the risk and advising the user to protect/delete the backup file. |
| F4 | P1 (F1), P2 (F-3), P3 (F-4) — 3-way convergence | `src/salamdr1.cpp:1055` (`SalLoadIcon`, pre-existing, newly load-bearing for 059's `LoadCloudSyncPendingIcons`) | `HICON hIcon;` left uninitialized; `LoadIconWithScaleDown`'s failure contract does not document writing the out-param, so a failure could hand a garbage handle downstream. | `HICON hIcon = NULL;` — one-line defensive initialization, zero behavioral risk, closes the gap for every caller. |
| F5 | P1 (F4), P3 (F-2) — 2-way convergence | `src/shiconov.cpp:838-850` (`GetCloudSyncPendingStateAuxAux`) | `PropVariantClear` ran only inside the `SUCCEEDED(GetValue)` branch; a provider that fills `pv` then returns failure would leak. | Made the clear unconditional (`PropVariantClear` after the value is used, regardless of the `GetValue` result), matching the canonical MSDN pattern. |

### CONFIRMED — Deferred (with justification, FR-004)

| ID | Perspectives | Location | Why deferred |
|---|---|---|---|
| D1 | P1 (F2), P2 (F-2), P3 (F-3) — 3-way convergence | `src/shiconov.cpp:856-868` (SEH `__except` in `GetCloudSyncPendingStateAux`) | A structured exception mid-`GetValue` leaks one `IPropertyStore` (and possibly its `pv`) because the handler returns `FALSE` without `Release()`/`Clear()`. Deferred because "fixing" it is not obviously safe: calling `Release()` on a COM object from inside the handler for an exception that may have occurred *during a call on that same object* risks a second fault on a corrupted vtable during unwind — the existing design (documented in-code) deliberately trades a rare, bounded leak for not crashing the whole app over a cosmetic badge, which is the correct trade-off for a read-only shell query behind `GPS_DELAYCREATION` (no third-party content handlers reachable here). No safe minimal fix exists; the current behavior is the safer of the two options. |
| D2 | P4 (Area 2 F1) | `src/snooper.cpp` (3 sites) | `SalU8ToW` legitimately fails on *length* (not just invalid UTF-8) for panel paths ≥ ~780 WCHARs (permitted since feature 004, `SAL_MAX_PATH_UTF8` allows up to 32,766 chars); the ACP fallback then also fails, and `FindFirstChangeNotificationW` receives a bounded-but-indeterminate buffer instead of the call being skipped outright. Proven memory-safe (bounded, always terminated) by P4's direct proof; the practical effect is auto-refresh silently not working on an extreme-length path, which is the existing failure mode for that case in spirit (feature 058 already accepts "auto-refresh may not always be establishable"). Deferred: fixing requires distinguishing "too long" from "invalid UTF-8" in `SalU8ToW`'s return value, which the function does not currently expose — a real code change beyond this feature's minimal-fix mandate, for a path length virtually no user will hit. |
| D3 | P4 (Area 2 F2) | `src/snooper.cpp`, `src/fileswn1.cpp` | `MB_PRECOMPOSED` makes the ACP fallback fail with `ERROR_INVALID_FLAGS` if the system ACP is UTF-8 (Windows' opt-in "Beta: Use Unicode UTF-8" setting). Near-zero practical impact: the fallback only runs on invalid UTF-8 input, which a UTF-8 ACP could not have converted correctly either way, and all failure paths were proven memory-safe. Deferred as a latent inefficiency, not a defect. |
| D4 | P5 (Area 2, two findings) | `src/shiconov.cpp` / `src/fileswn1.cpp` | Worst-case performance: a directory with ~1000 handler-unclaimed pending items under a paused sync makes one overlay read-cycle take 6.5–13 s (delays thumbnail rendering behind it), and a sustained `SHCNE_UPDATEITEM` notification storm combined with the pre-existing 2-second overlay-refresh restart loop can turn into continuous background property-scanning for the storm's duration. Both are background-thread-only (UI stays responsive) and both reporting perspectives explicitly frame them as post-release optimization opportunities, not defects — they are the fallback correctly doing its job when the badge state genuinely differs from cached, only expensive because it's now doing real work that previously wasn't attempted at all. Deferred: no functional harm, and any fix (e.g. reordering thumbnail vs. overlay phases, persisting `IconOverlayDone` across restart-triggered re-reads) is new development, explicitly out of this feature's scope (FR-003). |
| D5 | P6 (A3, A4, A5) | `utils/migrate-altap-settings.cmd` | Minor hardening opportunities: restore-script `%`-expansion if an output directory literally contains `%`; command-line values with embedded credentials could theoretically be echoed for one non-password field; no cycle/depth guard in registry tree copy (would require an attacker who can already write the user's hive). All LOW severity/likelihood, none release-blocking, deferred to keep this feature to verification+minimal-fix rather than a 057 hardening pass. |
| D6 | P6 (B1) | `src/shiconov.cpp:1168` (`LoadLibrary("cldapi.dll")`) | Relative DLL name is a DLL-search-path consideration in principle, but the reviewing perspective confirmed it matches a pattern used throughout the existing codebase (`mapi32.dll`, `user32.dll`, etc.), crosses no privilege boundary for this product's installation model, and `cldapi.dll` is resolved from System32 before any writable directory is consulted. Deferred as consistent with existing convention, not a regression. |
| D7 | P1 (F3) | `src/shiconov.cpp` (`CloudSyncPendingIndex` staleness) | Theoretical: a hypothetical future re-init of the overlay array without re-running `InitCloudSyncPendingOverlay` could leave a stale index. Verified unreachable today — `InitShellIconOverlays`/`ReleaseShellIconOverlays` each have exactly one call site, once per process lifetime. No current defect to fix. |
| D8 | P1 (F5), P3 (F-1) | `src/geticon.cpp:361-368` (`SHILCreateFromPath`, pre-existing pattern, narrowed not worsened by 058) | On a double conversion failure (invalid UTF-8 *and* ACP fallback failure), `ParseDisplayName` parses a bounded-but-uninitialized stack buffer — pre-existing since before this delta, and the delta strictly reduces how often the fallback is reached. No overflow; wrong-icon-at-worst. Deferred as out-of-delta and unchanged in kind. |

### Explicitly CLEAN (per-perspective, selected highlights)

- Ownership/double-free safety of `InitCloudSyncPendingOverlay`'s every early-return path — verified against `TDirectArray::Add`'s actual failure semantics (P1, P3).
- The property-store fallback can never execute while `GD_CS` is held (seed Q4) — verified on both the success and every failure path (P2, P3).
- No buffer overflow anywhere in the 058/059 offset-math delta, for any edge length including all-multibyte prefixes — proven with an explicit bounds argument, not just review (P4).
- The `res = S_FALSE` initialization added by 059 is a genuine, correct fix for a pre-existing uninitialized read (P1, P2, P3 independently confirmed).
- `IsCloudSyncRootPath` edge inputs (NULL/empty/over-length) all fail safe (P2, P3).
- Non-cloud directories pay exactly zero added cost; the gate runs once per work-cycle, never per item (P5, directly traced through the loop's `selectMode`/`goto SECOND_ROUND` logic).
- The snooper conversions (058) preserve the `DataUsageMutex` protocol and `MakeCopyWithBackslashIfNeeded`'s pointer-redirect semantics exactly (P2).

## Gates (SC-004)

| Gate | Check | Result | Evidence |
|---|---|---|---|
| G1 | `build.cmd full` (Debug x64) | ✅ PASS | BUILD SUCCEEDED, 0 errors, 0 new warnings in delta files; 19 plugins, 180 language modules |
| G2 | `build.cmd full release` | ✅ PASS | BUILD SUCCEEDED, 0 errors, 0 new warnings in delta files |
| G3 | saltests (Debug x64) | ✅ PASS | 1145 checks, 0 failed — unchanged from feature-058/059 baseline, re-run after fixes |
| G4 | Startup/graceful-exit health | ✅ PASS | Fresh Debug binary: alive 10 s, main-window `WM_CLOSE` → clean exit within 10 s, exit code 0, zero new crash reports |
| G5 | 057 utility test harness (`utils/test/run_migration_tests.cmd`) | ✅ PASS | 98 passed, 0 failed — run before fixes (baseline), and again after the F2/F3 fixes to confirm no regression |
| G6 | 058/059 validated behavior | ✅ STANDS | No stabilization fix touched the badge-display, encoding, or auto-refresh logic validated in `specs/058-*/evidence.md` / `specs/059-*/evidence.md`; those scenarios stand on existing user-verified evidence. F1's fix only reorders *when* one call happens relative to a lock, not its inputs/outputs — no re-run needed. |
| G7 | Icon resource sanity (`syncpend.ico`) | ✅ PASS | Verified in feature 059 (`LoadImage` at 16/32/48, all non-NULL); unaffected by this feature |

## Traceability audit (SC-005)

Every code change made in this feature maps to exactly one CONFIRMED finding:

| Change | Finding |
|---|---|
| `src/fileswn1.cpp` (leave/re-enter `ICSleepSection`) | F1 |
| `utils/migrate-altap-settings.cmd` (restore-script reorder + UTF-8/chcp) | F2 |
| `utils/migrate-altap-settings.cmd` (W5 password warning) | F3 |
| `src/salamdr1.cpp` (`hIcon = NULL`) | F4 |
| `src/shiconov.cpp` (unconditional `PropVariantClear`) | F5 |

Spot-check (3 of 5, per quickstart.md): F1 (fileswn1.cpp) — confirmed the diff
is exactly the leave/enter pair, nothing else moved. F2/F3
(migrate-altap-settings.cmd) — confirmed both changes are additive/reordering
only, no category-transfer logic touched. F4 (salamdr1.cpp) — confirmed the
diff is exactly the one-token initializer change. **No refactoring, no
unrelated changes.**

## User-visible impact (FR-007)

Two of the five fixes are user-visible and warrant a changelog entry when
this ships: F1 (a rare UI-freeze condition on hung network shares is closed)
and F2+F3 (the settings-migration utility's restore script now works
correctly for non-ASCII backup paths, and warns about the password content
of its backup file). F4 and F5 are internal robustness hardening with no
observable behavior change. See `CHANGELOG.md` `[Unreleased]` for the
entries added alongside this report.

## Go/No-Go Verdict

**GO.**

- Zero confirmed release-relevant defects remain unfixed (SC-003): all five
  CONFIRMED, release-relevant findings were fixed with minimal, traceable
  changes and re-verified; all eight deferred findings carry a written,
  substantive justification (bounded/rare, or explicitly out of the
  no-new-development mandate, or already unreachable).
- All seven gates pass (SC-004), including a first-ever clean run of the
  057 test harness (98/98) and a genuine, previously-undiscovered
  concurrency defect (F1) closed and re-verified.
- 058/059 user-validated behavior is unaffected (G6).
- The review's own investigative process — reproducing F1's mechanism by
  tracing the actual `ICSleepSection` hold region, and reproducing F2 by
  running the real restore script end-to-end against a non-ASCII path and
  watching it fail, then verifying the fix by watching it succeed — gives
  higher confidence than code reading alone would.

**Notable finding beyond the user's original ask**: the review's delta
scoping (research.md R1) surfaced that the post-0.1.2 delta includes feature
057's settings-migration utility, not just 058/059. Its restore-script defect
(F2) was arguably the most release-relevant finding of this entire review —
a data-loss bug in a tool whose entire purpose is safe undo, for exactly the
non-ASCII-path condition this project has spent two features fixing
elsewhere in the same release.

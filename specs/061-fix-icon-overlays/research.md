# Phase 0 Research: Restore General Shell Icon Overlay Support

**Feature**: 061-fix-icon-overlays | **Date**: 2026-08-19
**Inputs**: `spec.md` (incl. Clarifications), `investigation-leads.md` (code exploration),
runtime evidence gathered on the development machine (the machine the defect was
reported on), TortoiseOverlays upstream source.

## R0 — Evidence gathered (this machine, 2026-08-19)

### Registered overlay handlers (HKLM ShellIconOverlayIdentifiers)

22 subkeys; case-insensitive ascending order (= the priority order both Explorer and
Tandem Commander use). Leading spaces are part of the names (vendors use them to win
the alphabetical contest):

| # | Key (spaces shown) | Vendor |
|---|---|---|
| 1–7 | `     OneDrive1` … `     OneDrive7` (5 spaces) | OneDrive |
| 8–11 | `    GoogleDrive{Cloud,MirrorBlacklisted,Pinned,Progress}OverlayIconHandler` (4 spaces) | Google Drive |
| 12–20 | `  Tortoise1Normal` … `  Tortoise9Unversioned` (2 spaces) | TortoiseOverlays (SVN/Git shared) |
| 21 | `EnhancedStorageShell` | Windows |
| 22 | `Offline Files` | Windows |

All Tortoise subkeys carry valid CLSIDs (`{C5994560..68-53D9-4125-87C9-F193FC689CB2}`).

### Tandem Commander overlay configuration (HKCU\Software\Tandem Commander\0.1\Configuration)

- `Enable Custom Icon Overlays` = **1** (present)
- `Disabled Custom Icon Overlays` = **''** (present, empty)

**Consequence**: suspects S3 (missing-values kill switch) and S4 (sticky crash-disable)
from `investigation-leads.md` are **REFUTED as the active cause on this machine** —
the configuration is healthy. S3 remains a real latent defect for profiles that lack
the values (fresh profiles are fine — they run the in-memory default — but the
feature-057 migration writes a config-version marker without these values, which trips
`LoadIconOvrlsInfo`'s force-disable); it is fixed under FR-009 regardless.

### Code-site verification (this session, working tree @ 254bcba)

- `mainwnd3.cpp:1400` — `SHGetPathFromIDList` (ANSI) fills `szPath` with CP_ACP bytes;
  `:1409-1421` passes it to `IconOverlaysChangedOnPath`. **Confirmed.**
- `fileswn7.cpp:2083-2121` — `IconOverlaysChangedOnPath` gates on
  `IsTheSamePath(path, GetPath())` where `GetPath()` is UTF-8 (feature 004), then wakes
  the icon reader (`SleepIconCacheThread`/`WakeupIconCacheThread`) with a 200 ms
  coalescing window *designed explicitly around Tortoise notifications* (comment at
  `:2114`). **Confirmed: the async re-ask loop is the intended Tortoise mechanism, and
  its path comparison breaks on non-ASCII paths (defect D1).**
- `mainwnd2.cpp:2345-2349` — absent overlay values + config version ≥ 41 ⇒
  `EnableCustomIconOverlays = FALSE`. Two additional silent force-disables on
  malloc/`GetValue` failure (`:2331`, `:2340`). **Confirmed (defect D2, latent).**
- `shiconov.cpp:240-352` — handler load path: `CoCreateInstance` →
  `GetOverlayInfo` (error ⇒ handler skipped, `TRACE_I` at `:346` with the Altap-era
  note "Tortoise does this when more than 12 handlers are registered") → icon
  extraction via `WideCharToMultiByte(CP_ACP)` + `ExtractIcons`, all three sizes
  mandatory (`:289`) → `ShellIconOverlays.Add` (refuses at 15, `:680-684`).
  **Confirmed: load failures do NOT consume slots** — only successfully loaded
  handlers count toward the 15 cap.
- `shiconov.cpp:891-958` — query path: per-item loop in slot order; synthetic-entry
  skip; `Priority > minPriority` filter (`minPriority` = 100 for normal items, 9 for
  link/shared/offline items); Google Drive handlers gated to Google Drive paths;
  feature-059 property fallback only when no handler claimed. **Confirmed.**

## R1 — TortoiseOverlays upstream behavior (decisive external fact)

Source: TortoiseOverlays shim (`src/TortoiseOverlays/IconOverlay.cpp` in the
TortoiseSVN repository — the same shim serves TortoiseGit; Documentation.txt in the
same directory).

- `GetOverlayInfo` enforces its own limit `const int nOverlayLimit = 12`, counting
  the subkeys of HKLM `ShellIconOverlayIdentifiers` (`GetInstalledOverlays()`). When
  the count exceeds it, the shim **cascades — it sacrifices whole states, not itself**:
  >12 registered ⇒ Locked answers S_FALSE; >13 ⇒ +Ignored; >14 ⇒ +ReadOnly;
  >15 ⇒ +Unversioned. States Normal, Modified, Conflict, Deleted, Added keep
  answering S_OK with a valid icon path regardless of crowding.
- `GetPriority` returns 0–8 by state (Conflict = 0 highest).
- `IsMemberOf` delegates to the real TortoiseSVN/TortoiseGit handlers listed under
  `HKLM\Software\TortoiseOverlays\<State>` (loaded DLLs iterated per call).

**Consequence for this machine (22 registered)**: the shim drops Locked, Ignored,
ReadOnly, Unversioned system-wide (Explorer shows those four nowhere either), and
serves Normal, Modified, Conflict, Deleted, Added. Combined with R0 ordering and the
verified load path, Tandem Commander's expected slot table is:

| Slot | Handler | Expectation |
|---|---|---|
| 0–6 | OneDrive1–7 | loads |
| 7–10 | GoogleDrive ×4 | loads |
| 11 | Tortoise1Normal | loads (shim serves it) |
| 12 | Tortoise2Modified | loads |
| 13 | Tortoise3Conflict | loads |
| — | Tortoise4Locked, 5ReadOnly | GetOverlayInfo S_FALSE ⇒ skipped, no slot consumed |
| 14 | Tortoise6Deleted | loads |
| — | Tortoise7Added | **refused — 15-handler cap reached** |
| — | Tortoise8Ignored, 9Unversioned | shim S_FALSE ⇒ skipped |
| — | EnhancedStorageShell, Offline Files | refused — cap |

**Key deduction**: four functional Tortoise handlers (Normal, Modified, Conflict,
Deleted) *should* be loaded and answering on this machine — yet the user observes zero
Tortoise badges. **The active blocker is therefore downstream of handler loading and
is not explained by any suspect verified so far.** This is exactly what FR-001's
instrumented analysis must pinpoint (see R7).

Sources:
- https://github.com/TortoiseGit/tortoisesvn/tree/master/src/TortoiseOverlays (shim source; `IconOverlay.cpp`, `Documentation.txt`)
- https://github.com/TortoiseGit/tortoisesvn/blob/master/src/TortoiseShell/IconOverlay.cpp (real handler side)
- https://learn.microsoft.com/en-us/windows/desktop/api/shobjidl_core/nf-shobjidl_core-ishelliconoverlayidentifier-getoverlayinfo (interface contract)

## R2 — Decision: analysis-first implementation (Phase A)

**Decision**: The first implementation phase is an instrumented runtime analysis on
the affected machine (Debug build, `TRACE_ENABLE` — optionally `TRACE_TO_FILE` to a
TEMP file so no external Trace Server is needed), producing
`analysis-report.md` in this feature directory. Only after the report names the active
blocker(s) with evidence do the corresponding fixes land.

**Rationale**: FR-001/SC-004 make the analysis a deliverable; R1 proves the primary
symptom is *not yet root-caused* (loaded, healthy Tortoise handlers produce no badges).
Fixing blind would risk exactly the regressions FR-005 forbids.

**Alternatives considered**: fixing the already-verified defects (D1, D2) first and
re-testing — rejected as the sole strategy because D1 only affects non-ASCII paths
while the symptom reproduces in ASCII paths too (`D:\Projects\...` working copies),
so at least one more defect must be active.

**Phase A checklist (each item ends CONFIRMED or REFUTED in analysis-report.md, SC-004)**:

- A1: Do `InitShellIconOverlays` traces show the R1-predicted slot table? (If not —
  which handler fails at which step: create / GetOverlayInfo / icon extraction / cap.)
- A2: Does the overlay pass call Tortoise `IsMemberOf` at all for working-copy items
  (trace in `GetIconOverlayIndex`), and what does it return on first ask?
- A3: Does TortoiseGit's `SHCNE_UPDATEITEM` reach `CMainWindow`'s notification handler,
  and does `IconOverlaysChangedOnPath` pass its gates (`NextIconOvrRefreshTime`,
  `UseSystemIcons`, `IsTheSamePath`) on an ASCII path?
- A4: Does the re-woken icon reader actually re-run the overlay pass (`IconOverlayDone`
  reset semantics)?
- A5: Environmental gates: TortoiseGit "show overlays only in Explorer"-type settings,
  TGitCache process serving a non-Explorer client, elevation of the TC process.
- A6: Explorer ground truth on the same working copy (which badges Explorer shows) —
  fixes the floor for SC-001.
- A7: Non-ASCII path variant (confirms D1's user-visible effect after the primary
  blocker is fixed).

## R3 — Decision: fix the change-notification encoding defect (D1)

**Decision**: Convert the shell-change-notification path handling in
`CMainWindow::WindowProc` (`mainwnd3.cpp:1400` area) to the house pattern: obtain the
path wide (`SHGetPathFromIDListW`) and convert to UTF-8 (`SalWToU8`) before it reaches
UTF-8 consumers (`IconOverlaysChangedOnPath` → `IsTheSamePath(path, GetPath())`).
Audit the *other* consumers of the same `szPath` buffer in that handler in Phase A and
fix all that compare against feature-004 UTF-8 panel state, in the same
regression-by-omission class as feature 058's three sites.

**Rationale**: verified defect; identical class and identical house-pattern fix as
feature 058 (`SalU8ToW`/`SalWToU8` + CP_ACP fallback); notification loss silently
kills the designed Tortoise refresh mechanism on non-ASCII paths.

**Alternatives considered**: converting `GetPath()` to ANSI at the comparison —
rejected (reintroduces the encoding ambiguity feature 052/058 eliminated; lossy).

## R4 — Decision: absent overlay configuration means factory default (D2, FR-009)

**Decision**: In `LoadIconOvrlsInfo` (`mainwnd2.cpp:2345-2349`), absent values no
longer force `EnableCustomIconOverlays = FALSE` — absent means the factory default
(enabled, no providers disabled), per the spec clarification. The two *failure* paths
(malloc failure / unreadable stored value, `:2331`, `:2340`) keep their conservative
force-disable: they concern a *stored but unreadable* value, which FR-009 explicitly
leaves alone, and the safety rationale (handler crashes) still applies there.

**Rationale**: the Altap-era rule "absent at config version ≥ 41 = tampering" is false
in Tandem Commander: the feature-057 migration legitimately produces profiles with a
modern config version and no overlay values, silently killing all overlays (SC-007).

**Alternatives considered**: patching the 057 migration tool to emit the two values —
explicitly not required by the clarification; may be done later, independently.

## R5 — Decision: slot policy stays (alphabetical, cap 15, no ABI change)

**Decision**: Keep the existing selection: case-insensitive alphabetical registry
order, first 15 *successfully loaded* handlers, load failures consume no slot.
`CFileData::IconOverlayIndex` stays 4 bits (`spl_com.h:228`), `ICONOVERLAYINDEX_NOTUSED`
stays 15 — no plugin-ABI change.

**Rationale**: The Windows shell itself has a hard 15-slot overlay limit (shared image
list), several of which Explorer reserves for system overlays, so Explorer's effective
third-party capacity (~11–12) is *below* Tandem Commander's 15. With identical
ordering, TC's loaded set is a superset of Explorer's ⇒ the clarified Explorer-as-floor
semantics hold structurally, no ABI break needed. Constitution II (plugin ABI carried
unchanged) and V (interfaces documented before modification) both favor this.

**Alternatives considered**: widening the bit-field to raise the cap — rejected
(plugin ABI break for zero user-visible gain; nothing beyond 15 can matter while the
platform-wide contest already resolves below that). Smart/priority-based selection
diverging from alphabetical — rejected (would make TC's badge set diverge from
Explorer's in both directions, violating the floor guarantee).

## R6 — Decision: diagnostics stay debug-only, but become sufficient

**Decision**: Per the spec clarification, no Release-visible diagnostics. In Debug
traces, add one summary line after `InitShellIconOverlays` (final slot table: index,
name, priority) and make every rejection reason traceable (most already are; the cap
refusal at `Add` names the handler). No new UI, no new strings, no log file.

**Rationale**: the Phase A experiment and every future "provider X missing" triage
needs the slot table in one place; today it must be reconstructed from scattered lines.

## R7 — Resolution of the remaining unknown

The single open question — *why do loaded, healthy Tortoise handlers produce no
badges in TC on ASCII paths* — is deliberately **not** guessed in this plan. It is
resolved by Phase A (R2) before any fix beyond D1/D2 is designed; candidate mechanisms
are enumerated in A2–A5. The tasks file must order Phase A strictly before the
corresponding fix tasks, and `analysis-report.md` is the FR-001/SC-004 deliverable.

## Defect register (running; final disposition in analysis-report.md)

| ID | Status | Description | Fix decision |
|---|---|---|---|
| D1 | Verified (code) | ANSI notification path vs UTF-8 panel path — Tortoise re-ask loop dead on non-ASCII paths | R3 |
| D2 | Verified (code, latent) | Absent config values force overlays off at config version ≥ 41 (hits 057-migrated profiles) | R4 |
| D3 | Open (primary symptom) | Loaded Tortoise handlers produce no badges on ASCII paths | Phase A → then fix |
| D4 | Verified (quality) | No consolidated debug diagnostics of the final slot table | R6 |
| D5 | Verified (accepted) | Overlay icon path converted via CP_ACP at load (`shiconov.cpp:263`) — non-ACP-representable icon paths drop the handler | Convert to `SalWToU8`-based load or wide `ExtractIconsW` in the same pass, low risk; confirm in Phase A whether any real handler is affected |

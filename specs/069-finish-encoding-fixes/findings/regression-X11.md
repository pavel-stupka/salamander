# Regression review — X11 (group C10 + C8)

**Reviewed**: the diff of F-P1-22, F-P2-09, F-P2-11, F-P2-04, F-P6-01, F-P3-07,
F-P5-06 (10 files, +136/−25) · **Reviewer**: independent agent, did not write
the fixes · **Charter**: `contracts/fix-protocol.md` Part B · **Date**: 2026-08-24

## Verdict as delivered

**REJECTED** — two regressed surfaces:

| # | Severity | Site | Finding |
|---|---|---|---|
| R1 | HIGH | `dialogs5.cpp:1074-1079` | the hand-written boundary trim deleted a **complete** trailing multi-byte character (the last byte of `ý` = C3 BD is itself a continuation byte). Concrete shipped input: FTP plugin, Czech UI, Plugins Manager → FTP Client → Keyboard Shortcuts — `&Binární` → `Binárn`, `&Automatický` → `Automatick`; also Slovak `O&bnoviť` → `Obnovi`. Reachable because those two ftp commands register with `hotKey == 0`, so the `\t` strip does not mask it. |
| R3 | LOW-MEDIUM | `fileswn3.cpp:289`, `salamdr5.cpp:398` | with `_snprintf_s(_TRUNCATE)` cutting a long path mid-sequence, the buffer stays invalid UTF-8 and the wait window falls back to the legacy draw — which after the fix garbles the **translated template too**, where the pre-fix ANSI template stayed readable. Non-ASCII template in 7 of 8 shipped languages; reachable at ~580 bytes of path (fileswn3) / ~315 (salamdr5). |

Plus four non-blocking findings: **R2** the comment said "four sites left
alone" where there were three and all three had been converted; **R4** the
justification for converting the seven plugin-loading messages was factually
wrong (`DLLName` is deliberately *not* normalized — `plugins.h`'s own 052
contract lists it as not-normalized and `plugins1.cpp:2177` loads the plugin
with the ANSI `LoadLibrary` on it; `pluginName` comes from an ANSI
`FindFirstFile`); **R5** the shortcut column carries `GetKeyNameText` output,
not `LoadStr`; **R6** no fix record existed to review (this file).

## Disposition

| Finding | Action |
|---|---|
| **R1** | Already fixed before the review landed, and independently: the same bug was found while writing C9 and both hand-written trims were replaced by the new shared `SalU8TrimIncompleteTail` (`src/common/salunicode.cpp`), which carries the truncation **and** completeness guards. The reviewer verified the working tree and confirms `dialogs5.cpp:1077` is now correct and that `saltests.cpp` asserts exactly the `…n\xC3\xBD` case. The reviewer also confirmed this hunk is the **only** difference between the reviewed diff and the tree — items 1, 2, 4–8 were byte-identical. |
| **R3** | **Fixed**: `SalU8TrimIncompleteTail(buf)` after both `_snprintf_s` calls, so a truncating path degrades to "path cut short, everything readable" instead of "whole line through the legacy code page". |
| **R4 + R2** | **Reverted**: all twelve `IDS_AUTOINSTALLPLUGINS` / `IDS_AUTOINSTALLPLUGINS_INIT` / `IDS_LOADINGPLUGINS` uses in `plugins2.cpp` are ANSI `LoadStr` again, i.e. back to the baseline, and the comment now states the real reason they must stay ANSI. FR-002 allows a latent conversion only as a *provable no-op*; the reviewer showed it is a degradation in the non-shipped case, so it goes back. F-P2-04's confirmed pair (the two path messages) remains fixed. |
| **R5** | Comment corrected. |
| **R6** | This record; the per-fix records are in `closing-report.md`. The reviewer additionally measured the three per-item paths itself (one `MultiByteToWideChar` on a 260-byte stack buffer per user-menu icon, dwarfed by `ExtractIconEx`'s file I/O; one `malloc`+convert+`free` per list row, bounded by tens of rows) and judged the cost immaterial — no G6 timing required for this group. |
| F-P2-09 comment | Also corrected: the reviewer showed `DLLName` is ACP in the session it is added and UTF-8 after a restart, and that the U8 sink handles both. The fix stands (the sink is tolerant); only the comment overstated it. |

## Surfaces confirmed unchanged or corrected by the reviewer

- **F-P1-22**: `ExtractIconExA/W` share the `UINT` contract, so `== 1` is
  unchanged for 0/1/`(UINT)-1`; `WCHAR[MAX_PATH]` provably fits a
  `char[MAX_PATH]` UTF-8 name (≤259 bytes → ≤259 units + NUL); the helper is
  stack-only, so safe on the background icon-reader thread; `HANDLES_ADD` still
  registers exactly one handle. Bonus: a lone-surrogate path (feature 066) now
  loads its icon where the A call could not.
- **F-P2-09**: no `ListView_GetItemText`/`LVM_GETITEMTEXT` anywhere in
  `src/*.cpp` → the cells are never read back; mixing `LVM_SETITEMTEXTW` with
  the A call on one list view was already proven by the Name column in 052.
- **F-P6-01**: all four hazards checked and clear — a mid-session caller exists
  (`mdview.cpp:209`, un-checking *keep ready*) and re-arms correctly because
  the flag is cleared only when `UnregisterClassW` succeeded; the fix is **not**
  reachable from `MdKeeperWndProc`; on failure the flag stays `true` and
  `CreateWindowExW` then succeeds on the still-registered class; and
  `Release` returning FALSE never reaches `MdKeeperDisarm`, so the class is
  never unregistered while the DLL stays loaded.
- **F-P3-07**: the destination is `CToolTip::Text` = `char[TOOLTIP_TEXT_MAX]`,
  exactly what `CopyToolTipAnswer` writes; below 5000 bytes it *is* `lstrcpyn`
  (the trim sits behind `strlen >= TOOLTIP_TEXT_MAX`) → byte-identical; adding
  `#include "gui.h"` to `stswnd.cpp` introduces no macro or overload collision
  (the only shared name is a compatible `class CToolBar;` forward declaration);
  the three refuted sites are untouched.
- **F-P5-06**: 16 insertions / 0 deletions, `spl_vers.h` untouched (interface
  106), no forwarder diff; the statements verified against `spl_com.h:207` and
  against `CSalamanderDirectory::AddFile` performing no normalization.
- **Out-of-scope clusters**: nothing in B-1…B-5 touched; two adjacent B-scope
  items correctly left alone (the "Loaded" column's ANSI `LoadStr`, and the
  ANSI `GetOpenFileNameA` that produces the ACP `DLLName`).

## Re-cut verdict

With R1 already superseded in the tree and R3/R4/R2/R5 fixed as above, the
reviewer's own re-cut statement applies: *"re-cut against that state, F-P2-11
becomes **corrected** and only R3 remains as a required fix"* — and R3 is now
fixed. **ACCEPTED.**

Gates after the corrections: Debug build 0 errors and no new warnings in the
changed files; `saltests` **1269 checks, 0 failed**; `check_encoding.py
--strict` **TOTAL: 0**; draft 182 (the +9 `IDS_VIEWERTITLE` twins from X12,
deliberately left).

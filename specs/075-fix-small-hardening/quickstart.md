# Quickstart — Small hardening batch (075)

Validation guide: how to build, and how to prove each of the six fixes with a
**before** that fails and an **after** that passes. The reasoning is in
[research.md](research.md); the rules in
[contracts/fix-protocol.md](contracts/fix-protocol.md). Paste the observed
output of every step into `fix-log.md` (protocol A5/A6).

## Prerequisites

- Windows 11, VS2022 C++ workload, Python on `PATH` (`build.cmd` refuses to
  run without it), Node ≥ 20.10 (24.19.0 on the development machine).
- `OPENSAL_BUILD_DIR` set or the default `.\build\` — paths below use
  `build\tandemcommander\`.
- A Czech UI for S4 (Options ▸ Configuration ▸ Appearance ▸ Language, or the
  equivalent setting); English for the identity checks.
- Fixtures S1 and S4 write only under the **build** tree and `C:\t0075\` —
  nothing in the repository.

## G — Gates (run before the first commit and after the last)

| Gate | Command | Pass |
|---|---|---|
| G1 Debug build | `build.cmd full` | 0 errors; no new warnings in the six changed files |
| G2 Release build | `build.cmd full release` | same (Release does not build `saltests` — expected) |
| G3 Unit tests | `build\tandemcommander\Debug_x64\saltests\saltests.exe` | last line `saltests: N checks, 0 failed`, **N unchanged** from HEAD (this feature adds none — contract C14) |
| G4 Encoding guard | `python tools\check_encoding.py --strict` | `TOTAL: 0` |
| G5 Code Viewer harness | `src\plugins\codeview\test\run_tests.cmd` | `RESULT: all codeview checks passed` |
| G6 Leak / handle report | Debug build: start, open the viewer on any file, open the File Comparator on two files, close all, exit; read the debugger output / TRACE server | no new leak or handle line vs. HEAD |

## S1 — D1: the conversion-name copy *(FR-002)*

**Fixture.** Find the conversion set in use: open any text file in the viewer,
menu *Coding* — on a Central-European Windows the entries are those of
`convert\centeuro\convert.cfg`. Edit the **build** copy
`build\tandemcommander\Debug_x64\convert\centeuro\convert.cfg` and append:

```
AAAA…(exactly 200 × A)=ISO21250.TAB
BBBB…(exactly 1100 × B)=ISO21250.TAB
```

(`=` and `&` are the only reserved characters; any `.TAB` of the set will do.)
Restart the application so the tables reload.

**Before (Debug build, must fail):**
1. Open a text file in the viewer, *Coding* ▸ the 200-A entry.
   Expected: `/RTC1` dialog *"Run-Time Check Failure #2 — Stack around the
   variable 'codeName' was corrupted"* when `SetViewerCaption` returns.
2. *Coding* ▸ the 1100-B entry.
   Expected: the same for `'buff'` inside `CCodeTables::GetCodeName`.

**After (must pass):**
1. 200-A entry: no dialog; the title shows 199 × `A`.
2. 1100-B entry: no dialog; the title shows 199 × `B`.
3. **Identity** — *Coding* ▸ `ISO-8859-2 - CP1250`: the title suffix
   `- [ISO-8859-2 - CP1250]` is identical to HEAD; Files ▸ Convert dialog shows
   the same coding text as HEAD.

**Restore**: `build.cmd full` recopies `convert\` (or delete the two lines).

## S2 — D2: the unset default *(FR-003)*

No data fixture exists (research R2). Debug build, debugger attached.

1. Viewer on any file; *Options* — make sure *Recognize code page automatically*
   is **off** (menu *Coding*).
2. Breakpoint at `viewer3.cpp` on the `CodeTables.InitMenu(subMenu, CodeType);`
   line (≈ `:3291`). Open the *Coding* menu.
3. At the breakpoint, in a Watch window set `CodeTables.Loaded = 0`. Step over
   `InitMenu` (it traces *Table is not loaded* and returns) to the
   `SetMenuDefaultItem` line.

**Before:** Locals show `defCodeType == 0xCCCCCCCC` (the `/RTC1` fill) and that
value is passed to `SetMenuDefaultItem`.
**After:** `defCodeType == 0`.
4. Set `CodeTables.Loaded = 1` again and continue; close the viewer.
**Identity:** with `Loaded` untouched, set the default coding (*Coding* ▸ *Set
as default*) and reopen the menu: that entry is highlighted — same as HEAD;
then (application closed) edit the stored default name in the registry to a
nonsense value — `HKCU\Software\Tandem Commander\0.1\Viewer`, value
`Default Convert` — start again and reopen the menu: the *none* entry is
highlighted — same as HEAD.

## S3 — D3: NULL conversion name *(FR-004)*

Debug build, debugger attached. There is one `CSalamanderGeneral` instance per
loaded plugin (`CPluginData::SalamanderGeneral`, `plugins.h:3301`), so the
easiest handle is a live call: breakpoint on the first line of
`CSalamanderGeneral::GetConversionTable` (`zip.cpp:3301`), then trigger any
plugin call to it — the File Comparator with a conversion selected in its
options (`filecomp/textio.cpp`) or the Database Viewer's coding change
(`dbviewer/renmain.cpp`). At the breakpoint, in the Immediate window, reuse the
caller's buffer with a NULL name:

```
this->GetConversionTable(0, table, 0)
```

**Before:** access violation inside `CCodeTables::GetCodeType` /
`CodingNameEqual`.
**After:** returns `0` (FALSE); the trace shows
`Invalid parametr (conversion==NULL) in CSalamanderGeneral::GetConversionTable!`.
**Identity:** the same call with `"ISO-8859-2 - CP1250"` returns `1` and the
256-byte table is byte-identical to HEAD's (compare the first 16 bytes in a
Memory window against a HEAD run).

## S4 — D4: the viewer title under a long accented path *(FR-005)*

**Fixture** (Windows PowerShell; builds a 289-byte UTF-8 path whose byte 259 is
the lead byte of a `č`):

```powershell
$c   = [string][char]0x010D                 # č, 2 bytes in UTF-8
$dir = 'C:\t0075\' + ($c * 100)             # 9 + 200 bytes
New-Item -ItemType Directory -Force $dir | Out-Null
$file = Join-Path $dir ($c * 40)            # + 1 + 80 bytes = 289
Set-Content -Path $file -Value 'hello' -Encoding ascii
$b = [Text.Encoding]::UTF8.GetBytes($file)
"{0} bytes; byte 259 = 0x{1:X2} (expect 0xC4)" -f $b.Length, $b[258]
```

Also keep a short accented path for the identity check, e.g.
`C:\t0075\Přehled.txt`.

**Before (Czech UI):** F3 on the long file — the title bar shows the path and
the word *Prohlížeč* with garbled accented characters (each `č` as two
code-page characters).
**After:** the title reads `C:\t0075\ččč…\ččč…(24 × č) - Prohlížeč - […]`, every
character correct; the file name prefix ends after the 24th complete `č`.
**Identity:** F3 on `Přehled.txt` — title identical to HEAD (`Přehled.txt -
Prohlížeč …`). English UI: both titles identical to HEAD except for the D4
correction on the long one.
**Plugin caption path** (second site): open a file through a plugin that sets
its own caption (e.g. an archive entry viewed from the ZIP plugin) — title
identical to HEAD for the usual short captions.

**Restore**: `Remove-Item -Recurse -Force C:\t0075`.

## S5 — D5: the File Comparator header *(FR-006)*

Intake is bounded (research R5), so the overflow is provoked directly. Debug
build, debugger attached.

1. Files ▸ Compare Files on any two text files; the comparator window opens.
2. Break in `CFileHeaderWindow::SetText` (or anywhere on the comparator's
   thread), then in the Immediate window:
   `LeftHeader->SetText("xxxx…(300 × x)")`
   (`LeftHeader` is `CMainWindow`'s member; the literal is typed in full).
3. Continue, then close the comparator window.

**Before:** on close, the Debug CRT reports heap corruption after the window
object (`HEAP CORRUPTION DETECTED: after Normal block …`) — the 300-byte copy
overran `Text[260]`.
**After:** no report; while open, the left header shows 259 × `x` (compacted
with an ellipsis to the width).
**Boundary:** repeat with a 300-byte literal whose byte 260 is the lead of a
2-byte character (e.g. 258 × `x` followed by `\xC4\x8D` `\xC4\x8D`…) — the
stored text ends after byte 258, not with a lone `\xC4`.
**Identity:** normal use — compare two files with accented names: both headers
identical to HEAD.

## S6 — D6: one verdict per source tree *(FR-007)*

On the development machine (Node 24):

```
node --no-experimental-detect-module src\plugins\codeview\test\harness\test_worker.mjs
```
**Before and after (the emulation of a Node 20 default):** exit 1,
`ERR_REQUIRE_CYCLE_MODULE … web\worker.js` — this proves the failure class is
real; it is not the runner.

```
src\plugins\codeview\test\run_tests.cmd
```
**Before (HEAD runner on Node 24):** passes — because Node 24 detects the
module by default. **This is why the HEAD reproduction needs a Node 20
install**: with nvm-windows/fnm, `nvm use 20` then the runner →
`RESULT: FAILURES` with a `SyntaxError: Cannot use import statement outside a
module` from `worker.js`. If no Node 20 is available, record that and rely on
the emulation above.
**After:** the runner passes on Node 24 **and** on Node 20 (if available); the
worker line in the script carries `--experimental-detect-module`.
**Still fails when it should:** temporarily break `web/worker.js` (e.g. rename
an exported symbol), run the runner → `RESULT: FAILURES`; restore.
**Nothing shipped changed:** `git status` shows no file under
`src/plugins/codeview/web/`; `python src\plugins\codeview\test\check_data.py`
output identical to HEAD.

## Closing (after all six are ACCEPTED)

1. `fix-log.md` has six complete records (data-model §1) with pasted proofs and
   a link to each `findings/review-D<n>.md`.
2. `specs/069-finish-encoding-fixes/REMAINING-WORK.md` §3: each of the five
   items marked *closed by 075* with the review file; the "Fix this first"
   sentence retired.
3. `specs/NEXT-WORK.md` item 1 marked done with a pointer to the fix log;
   `CLAUDE.md` "Recent Changes" gains the 075 line.
4. `074/fix-log.md`'s "Unrelated, pre-existing, not fixed here" note about the
   runner gets a one-line pointer to D6.
5. Gates G1–G6 re-run green on the final tree.
6. Changelog text for D4 and the hardening line sits in `fix-log.md` ready for
   the release's ship gate — not applied here.

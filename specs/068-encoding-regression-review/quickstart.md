# Quickstart: Running the Encoding Review, Gates and Regression Sweep

**Feature**: 068-encoding-regression-review
**Deliverables**: `inventory.md` (audit) + `review-report.md` (findings, fixes,
gates, sweep, verdict) + green gates. Method in [research.md](research.md);
record schema in [data-model.md](data-model.md); the reviewers' reference in
[contracts/encoding-contract-checklist.md](contracts/encoding-contract-checklist.md).

## Prerequisites

- Branch `068-encoding-regression-review`; baseline = tag `v0.1.4` (build 188)
  plus the unreleased 065–067 commits (`git log v0.1.4..HEAD`).
- VS2022 (C++ Desktop workload), Python 3.x on `PATH` (the build refuses to run
  without it — the encoding guard cannot be skipped), `OPENSAL_BUILD_DIR` set
  (defaults to `.\build\`).
- A Czech Windows 11 system (Czech regional format: no-break-space digit
  grouping, `,` decimal). The UI language is switched inside the app — see
  "Switching the UI language" below; Czech, Hungarian and English `.slg`
  modules are all produced by `build.cmd full`.
- Fixtures (create once):

  ```powershell
  # Unicode/long-path names (NFC, NFD, Greek, Japanese, emoji, >300-char path)
  # plus, with -Perf, a 100,000-file folder at <Base>\perf for the timing gate
  powershell -ExecutionPolicy Bypass -File tools\create-test-fixtures.ps1 -Perf
  # default <Base> = %TEMP%\salamander-test
  ```

  Add by hand (PowerShell, `\\?\`-prefixed paths work):
  - a folder `D:\Zkouška\Můj disk\` with files `příloha.txt`, `žluťoučký kůň.docx`,
    `1 000 000.pdf` (real NBSP in the name) — the Czech sweep folder;
  - a folder `D:\Zkouška\Árvíztűrő tükörfúrógép\` with `bájt.txt` — the
    Hungarian sweep folder;
  - one file with an unpaired surrogate in its name (feature 066 quickstart
    gives the one-liner; `TestWtf8FileOps` creates and deletes its own).

## Gates (automated; run after every accepted fix and once at the end)

| Gate | Command / procedure | Pass bar |
|---|---|---|
| G1 Debug build | `build.cmd full` | 0 errors; no new warnings in files touched by this feature (compare `git diff --name-only v0.1.4` against the MSBuild output) |
| G2 Release build | `build.cmd full release` | same bar (Release does not build `saltests`; that is expected) |
| G3 Unit tests | `%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\saltests\saltests.exe` (after G1 — `build.cmd` builds it but never runs it) | last line `saltests: N checks, 0 failed` with N ≥ 1229; exit code 0 |
| G4 Encoding guard | `python tools\check_encoding.py --strict` (also runs inside every `build.cmd`) | `TOTAL 0` findings; every rule added by this feature is additionally proven with `git stash` → guard flags the pre-fix site → `git stash pop` → clean |
| G5 Start/exit health | Launch `…\Debug_x64\tandemcommander.exe`, browse the Czech sweep folder and the surrogate file, stay ≥ 10 s, close with Alt+F4 (graceful — not process kill). **Trace Server capture waived**: `tserver` cannot be built (pre-existing UNICODE collision in `src/common/handles.h`, review-report D01), so the gate runs at the feature-060 observable bar | exit code 0; no "Some monitored handles remained opened" box; no "_CrtCheckMemory failed" box during the run; no new `*.TXT` crash report under `%LOCALAPPDATA%\Tandem Commander`; no assertion dialogs. Note: the CRT leak dump is disabled in this code base (`_CRTDBG_LEAK_CHECK_DF` commented out), so the leak signal is the handle box + heap check, as in feature 060's G4 |
| G6 Timing (only for fixes on per-item paths — FR-008) | see "Timing method" below | after-fix median within the baseline's [min, max] |
| G7 English spot-check | switch UI to English on the same machine, re-run sweep items W1–W6, W13 | output identical to release 0.1.4 (compare with a `v0.1.4` Release build side by side) |

## Timing method (G6)

Applies to any fix whose changed code runs once per listed item (folder
listing, sorting, icon/overlay reading, per-name conversion).

1. Fixture: `%TEMP%\salamander-test\perf` (100,000 files, from
   `create-test-fixtures.ps1 -Perf`). Open it once and discard that run (cache
   warm-up).
2. **Before** the fix (pre-fix build, e.g. `git stash` or the `v0.1.4`/pre-fix
   binary): 5 runs of the affected operation, stopwatch from the key press to
   the panel being fully populated (directory line shows the final item
   count / sort completes / all visible icons drawn):
   - listing: Enter into `perf` from its parent (Alt+F1/F2 to the drive, then
     Enter);
   - sorting: Ctrl+F4 then Ctrl+F3 (by extension, back to name);
   - icon/overlay reading: switch to the icon views (Alt+3, Alt+4) and back to
     detailed (Alt+2);
   - per-name conversion: whichever of the above reaches the changed code.
   Record min, median, max. **Noise = max − min of these five.**
3. **After** the fix: 5 runs the same way. Pass iff the after-median lies
   within [before-min, before-max]. Record all ten numbers in the Fix record
   (`review-report.md`).

## Switching the UI language

Options → Configuration → **Language** page → **Language…** → pick
`czech.slg` / `hungarian.slg` / `english.slg` → OK → restart the application
(the choice is stored as the `Language` value under
`HKCU\Software\Tandem Commander\0.1`). Regional format (separators) is the
Windows setting and stays Czech throughout — that is the combination every
reported defect came from.

## Regression sweep (manual; run in Czech UI, then Hungarian UI; W1–W6 + W13 again in English for G7)

Every item is an earlier encoding feature's surface. Expected result for every
item: names, translated text and grouped numbers render exactly (diacritics
intact, no `Â`, no `?`, no mojibake, no blank field); operations succeed on
the non-ASCII and surrogate names. Record PASS/FAIL per item and language in
`review-report.md` (Sweep table).

| W | Surface (origin feature) | What to do |
|---|---|---|
| W1 | Panel names, size column, tiles (004/010) | Open the sweep folder in Detailed and Tiles views; check names and sizes |
| W2 | Information line (041) | Select 2 files + 1 folder; read the selection summary |
| W3 | Directory line / free space (010) | Look at the path shown above the panel and the free-space text |
| W4 | Drive Information (067) | Ctrl+F1 on the drive: Capacity/Free/Used — long and short forms |
| W5 | Directory sizes / occupied space (067) | Commands → Calculate Occupied Space on the sweep folder; also the archive-size dialog inside a ZIP |
| W6 | Message boxes (042/052) | Provoke "not enough space"/overwrite/delete confirmations on the sweep files; Ctrl+C in a message box and paste |
| W7 | Find window results + status bar (042/043) | Alt+F7, search `*.txt` in the sweep tree; check result rows, status text; F3 from results |
| W8 | Plugins Manager names (052) | Plugins → Plugins Manager; names of loaded and not-loaded plugins |
| W9 | Make File List (063) | Ctrl+M with a name column width `:20`; destinations clipboard (paste into Notepad), viewer, file `seznam-příloh.txt` |
| W10 | Copy name / path commands (063) | Copy name, full path, UNC path of a sweep file; paste into Notepad |
| W11 | Tooltips and hints (063) | Hover the Make File List syntax hint, a file-mask hint, a hot-path button |
| W12 | Rename field + internal viewer (015) | F2 on `žluťoučký kůň.docx`, edit and confirm; F3 on a UTF-8 text file, check the offset tooltip and Copy |
| W13 | Alt+F1 drive menu (041) | Open the drive menu; free-space text per drive |
| W14 | Recycle Bin on non-ASCII path (062) | Del on a scratch file inside `D:\Zkouška\Můj disk\` → confirmation says Recycle Bin; file appears in the Recycle Bin |
| W15 | Icons, overlays, auto-refresh on non-ASCII path (058/059/061) | Sweep folder inside a OneDrive/Google Drive tree if available: badges present; modify a file externally → panel refreshes without a busy-cursor re-list |
| W16 | Surrogate-name operations (066) | Copy (F5), move (F6), rename (F2), view (F3), delete (F8) the surrogate-named file; copy its name to the clipboard |
| W17 | Saved-configuration round trip (066/004) | Leave a panel on `D:\Zkouška\Můj disk\` and on the surrogate folder, exit, restart: both panels reopen there; hot path with a non-ASCII name survives restart |
| W18 | Language selection dialog + rename caption (043) | Options → Configuration → Language: the current language name; the rename dialog caption with the file name |
| W19 | Hot path names (047) | Add a hot path named `Můj disk`; its button/menu label |
| W20 | Plugin surfaces at the boundary (008) | ZIP: open an archive with non-ASCII entries, extract to the sweep folder; SFTP/FTP not required unless a fix touches them; mdview: F3 on a `.md` in the sweep folder (065) |

Items that cannot run on the machine (no cloud drive for W15, no ZIP with
non-ASCII entries) are recorded **WAIVED** with the reason — never skipped
silently.

## Auditing the report

Open `review-report.md` and check:

1. Every Boundary B1–B8 has an inventory section in `inventory.md`, and every
   Site there carries a classification, evidence and a perspective (SC-001).
2. Every defect class in the contract checklist shows sweep status
   **complete**; every ledger item from earlier features has a fresh
   disposition (SC-002).
3. Every Finding has an independent verdict with `file:line` evidence; pick
   any 3 code changes from `git diff v0.1.4 -- src tools` → each maps to a
   CONFIRMED Finding and an ACCEPTED Fix record (SC-003).
4. Every Fix lists its affected surfaces with verdicts, its byte-identity
   evidence, its check with the fail-before/pass-after proof, and — for
   per-item paths — the ten timing numbers (SC-004, SC-010).
5. Gate table G1–G7 all PASS or WAIVED-with-reason; Sweep table W1–W20 × {cs,
   hu} (+ en for G7) all PASS or WAIVED-with-reason (SC-005, SC-006, SC-007).
6. Every confirmed defect class names its durable check, proven against the
   pre-fix tree (SC-008).
7. `CHANGELOG.md` Unreleased lists every user-visible fix (SC-009).
8. The stability verdict is explicit and consistent with 1–7.

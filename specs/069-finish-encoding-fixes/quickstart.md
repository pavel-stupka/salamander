# Quickstart — validating feature 069

**Feature**: 069-finish-encoding-fixes · **Spec**: [spec.md](spec.md) ·
**Plan**: [plan.md](plan.md) · **Protocol**: [contracts/fix-protocol.md](contracts/fix-protocol.md)

Everything here is runnable. The gates G1–G7 are automated and run after every
accepted fix; G8 (the on-screen sweep) needs a person and is handed to the
maintainer with the per-fix scenarios at the end.

## Prerequisites

- Branch `069-finish-encoding-fixes`; baseline commit `64dcbb5` (post-068).
- VS2022 (C++ Desktop workload), **Python on `PATH`** (`build.cmd` refuses to
  run without it — the encoding guard cannot be skipped), `OPENSAL_BUILD_DIR`
  set (defaults to `.\build\`).
- Czech (CP1250) Windows 11 with Czech regional format. The UI language is
  switched inside the application (Options → Configuration → Language →
  restart); `czech.slg`, `hungarian.slg` and `english.slg` all come from
  `build.cmd full`.
- **Keep a pre-fix Release build aside** for the English byte-identity
  spot-check (G7):

  ```powershell
  # once, before the first fix
  build.cmd full release
  robocopy build\tandemcommander\Release_x64 build\tandemcommander\Release_x64_prefix069 /E /NFL /NDL /NJH /NJS
  ```

## Fixtures (create once)

`D:\Zkouška\` does **not** exist on this machine — the 068 fixtures must be
recreated. `tools\create-test-fixtures.ps1` covers only the `%TEMP%` set.

```powershell
# 1. the %TEMP% set + the 100,000-file timing folder (G6)
powershell -ExecutionPolicy Bypass -File tools\create-test-fixtures.ps1 -Perf

# 2. the Czech / Hungarian / surrogate sweep folders  (FX-CS, FX-HU, FX-SUR)
$cs = 'D:\Zkouška\Můj disk'; $hu = 'D:\Zkouška\Árvíztűrő tükörfúrógép'
New-Item -ItemType Directory -Force $cs, $hu, 'D:\Zkouška\Účetnictví',
    'D:\Zkouška\surrogate', 'D:\Zkouška\Dočasné', 'D:\Zkouška\Šablony',
    'D:\Zkouška\Zálohy\Projekty' | Out-Null
'x' | Set-Content -Encoding utf8 "$cs\příloha.txt"
'x' | Set-Content -Encoding utf8 "$cs\žluťoučký kůň.docx"
'x' | Set-Content -Encoding utf8 "$cs\Přehled.txt"          # F-P6-04
'x' | Set-Content -Encoding utf8 "$cs\poznámky.txt"         # F-P4-02
'x' | Set-Content -Encoding utf8 "$cs\Účtenka.pdf"          # F-P1-26
'x' | Set-Content -Encoding utf8 "$cs\Smlouva – kopie.docx" # F-P1-19
'x' | Set-Content -Encoding utf8 ("$cs\1" + [char]0x00A0 + "000" + [char]0x00A0 + "000.pdf")
'x' | Set-Content -Encoding utf8 "$hu\bájt.txt"
Copy-Item "$cs\Smlouva – kopie.docx" 'D:\Zkouška\Účetnictví\'
# a second tree for Compare Directories (F-P1-19)
New-Item -ItemType Directory -Force 'D:\Zkouška\Kopie\Účetnictví' | Out-Null
Copy-Item "$cs\Smlouva – kopie.docx" 'D:\Zkouška\Kopie\'
# surrogate name (feature 066 one-liner)
[System.IO.File]::WriteAllText("\\?\D:\Zkouška\surrogate\Lone`u{D800}surrogate.txt", 'x')
```

Configuration-dependent fixtures, created when the owning fix is verified:

| ID | How |
|---|---|
| FX-TEMP (accented `%TEMP%`) | `cmd /c "set TEMP=D:\Zkouška\Dočasné && set TMP=D:\Zkouška\Dočasné && build\tandemcommander\Debug_x64\tandemcommander.exe"` — no new Windows account needed |
| FX-INST (accented install path) | `robocopy build\tandemcommander\Debug_x64 "D:\Zkouška\Tandém Commander" /E` and run it from there |
| FX-SUBST | `subst X: D:\Zkouška\Šablony` (`subst X: /D` to remove) |
| FX-JUNC | `mklink /J D:\Zkouška\Data D:\Zkouška\Zálohy\Projekty` |
| FX-SHARE | `net share Ucetnictvi="D:\Zkouška\Účetnictví"` — elevated; rename the share to `Účetnictví` in *Computer Management* for the non-ASCII case |
| FX-ARC | Options ▸ Archivers → add a custom packer/unpacker using `C:\Program Files\7-Zip\7z.exe` (no RAR on this machine) |
| FX-ACCOUNT | items gated on the **account name** (`%APPDATA%`, `%USERPROFILE%`) need a Windows user with an accent (e.g. `Jiří`); where one is not created, the fix is verified at the producer level and recorded as such — never claimed as an on-screen pass |

## Gates (after every accepted fix, and once at the end)

| Gate | Command | Pass bar |
|---|---|---|
| G1 Debug build | `build.cmd full` | 0 errors; no new warnings in the files this feature touches |
| G2 Release build | `build.cmd full release` | same (Release does not build `saltests` — expected) |
| G3 Unit tests | `build\tandemcommander\Debug_x64\saltests\saltests.exe` | last line `saltests: N checks, 0 failed`, N ≥ 1257 + this feature's checks; exit code 0 |
| G4 Encoding guard | `python tools\check_encoding.py --strict` | `TOTAL: 0`; each added rule additionally proven: plant the pre-fix line → rule fires → restore → clean |
| G5 Start/exit health | run `…\Debug_x64\tandemcommander.exe`, browse FX-CS and FX-SUR, ≥ 10 s, close with Alt+F4 | exit 0; no "monitored handles remained opened" box; no `_CrtCheckMemory failed`; no new crash report under `%LOCALAPPDATA%\Tandem Commander`. After D01 lands, capture the Trace Server log as well (068 waived this) |
| G6 Timing | per-item-path fixes only — see below | after-median inside the baseline `[min, max]` |
| G7 English spot-check | switch UI to English; re-run sweep W1–W6, W13 and the English form of each new scenario against `Release_x64_prefix069` | output identical |

`--draft` is informational: record the count before and after
(`python tools\check_encoding.py --draft | tail -1`) so the closing report can
state how many report-only findings this feature removed.

### Timing method (G6)

Fixture `%TEMP%\salamander-test\perf` (100,000 files). Open it once and discard
that run (cache warm-up). Then 5 runs before the fix and 5 after, stopwatch
from the keypress to the panel being fully populated; record min/median/max of
both. Noise = max − min of the before set. Pass iff the after-median lies
within `[before-min, before-max]`. Applies to F-P1-27 (the per-directory share
marker) and to any fix that lands on the listing, sorting, icon-reading or
per-name-conversion path.

## Per-fix scenarios (V-items — the maintainer's list)

Each is "reproduce on the pre-fix binary, then confirm on the new one".
`Release_x64_prefix069` is the pre-fix reference.

| ID | Item | Fixture | Do this | Expected after |
|---|---|---|---|---|
| V-01 | F-P6-04 | FX-CS | focus the command line, focus `Přehled.txt`, **Ctrl+Enter**; then **Ctrl+Space**, **Ctrl+[**, **Ctrl+]**; press Enter on a `dir` command | the name/path appears readable (not `PĹ™ehled.txt`) and the command finds the file. Caret/selection behave as before |
| V-02 | F-P1-26 | FX-CS | drag `Účtenka.pdf` from Explorer onto the command line, the status bar, the toolbar, and an open viewer | each target accepts it and uses the right file |
| V-03 | F-P1-19 | FX-CS + `D:\Zkouška\Kopie` | Commands ▸ Compare Directories, by content | `Smlouva – kopie.docx` compares; `Účetnictví` is descended; no "Cannot read directory" prompt |
| V-04 | F-P1-20 | FX-CS + a ZIP containing `příloha.txt` | F4-edit the file in the archive, save, then *Copy To…* in the changed-files dialog | the file is copied; on failure an error appears (never silence) |
| V-05 | F-P1-21 | FX-TEMP + FX-CS | drag out of an archive to Explorer; make an SFX; create a junction; run a user-menu item "through a batch file" and one using `$(DOSFullName)`; type a mask in a dialog that lists files | each acts on the right files; no leftover `SAL*.tmp` |
| V-06 | F-P1-22 | FX-INST | user-menu item with its icon from an exe under an accented path | the real icon in the User Menu and on the toolbar |
| V-07 | F-P1-23 | FX-ACCOUNT | type `%USERPROFILE%\Desktop` in Shift+F7 and on the command line | the panel changes there |
| V-08 | F-P1-25 | FX-CS | Enter on a `.lnk` in an accented folder, and on a `.lnk` whose **target** is accented | the shortcut is followed into the folder |
| V-09 | F-P1-08 / F-P1-10 | FX-ACCOUNT / FX-INST | F1 (help); `config.reg` next to the exe and in `%APPDATA%\Tandem Commander`; a user-menu item using `$(SalDir)` | help opens, config imports, the item launches; **and** plugins, language and conversion tables still load |
| V-10 | F-P1-07 / F-P1-05 / F-P1-06 | FX-INST + FX-ARC + FX-TEMP | pack `žluťoučký kůň.docx`; list and extract it again; unpack into `D:\Zkouška\Zálohy` | pack succeeds; the previously working extract still works; no temp directory left behind |
| V-11 | F-P1-09 / F-P4-05 | FX-CLOUD | Alt+F1 → OneDrive / Dropbox / Google Drive | the panel lands in the cloud folder, not an ancestor |
| V-12 | F-P1-12 / F-P1-13 / F-P1-14 / F-P2-07 | FX-SUBST, FX-JUNC | Ctrl+F1 on a junction (Czech UI); delete a link on `X:`; Alt+F1 drive menu labels | link target readable; the confirmation says junction/symlink; UNC and SUBST rows unchanged; labels unchanged for code-page-representable ones |
| V-13 | F-P1-27 | FX-SHARE | list the parent of the shared folder | the shared-folder marker appears (+ G6 timing) |
| V-14 | F-P4-01 / F-P4-02 | FX-CS | Viewer ▸ Coding → a *Kameničtí* / *KOI-8 ČS2* entry → Set As Default; exit, restart; view `poznámky.txt` in the Czech UI | the default holds; the caption reads `poznámky.txt` and the translated viewer name correctly |
| V-15 | F-P6-01 | any `.md` | F3 a `.md`; close; Plugins Manager → Markdown Viewer → **Unload**; F3 the `.md` again | the second view is as fast as the first |
| V-16 | F-P2-04 | FX-CS, Czech UI | enter a slow path (`\\server\Zálohy` or a spun-down disk) | the wait window shows the path readably |
| V-17 | F-P2-09 / F-P2-11 | a plugin from `D:\Můj plugin\` | Plugins Manager (after a restart) → Location column; then *Keyboard Shortcuts* for Checksum | both readable |
| V-18 | F-P2-13 | FX-CS | start with `-C "D:\Zkouška\config.reg"`, then Options ▸ Save Configuration with that file present | the prompt shows the path readably |
| V-19 | F-P3-07 | a path whose text exceeds 4999 bytes | hover the directory line | the tooltip is readable |
| V-20 | F-P4-03 / F-P4-07 | Hungarian UI / Czech UI | Alt+F5 archiver combo; Configuration ▸ Views | titles and view-mode names carry their accents; no `?` persisted |
| V-21 | D03 / D04 | FX-CS | File Comparator on two accented-name files — **binary** and **text** | the caption is correct in both; the path bar keeps its text |
| V-22 | D02 (if fixed) | FX-CS, Czech locale | ZIP overwrite prompt for a file ≥ 1000 bytes | no stray `Â` |
| V-23 | D01 | — | `msbuild src\vcxproj\tserver\tserver.vcxproj` | it builds; the core build is unchanged |
| V-24 | D05 | — | `powershell -NoProfile -File .specify\extensions\git\scripts\powershell\auto-commit.ps1 -WhatIf` (or parse it) | no `ParserError` |

## Regression sweep (G8 — Czech UI, then Hungarian UI)

Run the 068 list W1–W20 unchanged (the table is in
`specs/068-encoding-regression-review/quickstart.md`, section "Regression
sweep") on the FX-CS / FX-HU folders. Every item must pass exactly as before
this feature — these are the surfaces earlier features repaired, and the
purpose of the sweep is to prove this feature did not disturb them. Then the
V-items above. A failure is a finding: back to fix → review → gates.

## Auditing the result

1. Every item of the spec's inventory table has a disposition in
   `closing-report.md`, with its fix id, regression verdict and check.
2. Every commit names exactly one item or coupling group.
3. `findings/regression-X<nn>.md` exists for every fix and says **ACCEPTED**.
4. `saltests` and the guard are at or above baseline; each added check has its
   fail-before/pass-after proof recorded.
5. `CHANGELOG.md` Unreleased describes every user-visible fix in the user's
   terms — and does **not** claim the hygiene-only (F-P4-07) or verify-only
   items as repairs, nor hide F-P6-04's residual limitation.
6. `REMAINING-WORK.md` lists what is still open: clusters B-1–B-5, the complete
   Unicode command line, and anything deferred here with its reason.

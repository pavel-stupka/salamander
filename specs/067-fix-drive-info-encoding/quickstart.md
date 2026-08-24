# Quickstart: Validating Feature 067 (Drive Information Number Encoding)

**Prerequisites**: Windows 11, VS2022 with C++ workload, Python 3 on PATH
(`build.cmd` requires it for the encoding gate), `OPENSAL_BUILD_DIR` set
(defaults to `.\build\`). Manual scenarios need the Czech UI language
(Options → Configuration → Appearance → Language, or a Czech-locale system —
the Czech thousands separator is a no-break space, which is what triggers the
defect).

## 1. Build + automated gates

```batch
build.cmd                       :: Debug x64; runs tools\check_encoding.py --strict
%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\saltests\saltests.exe
build.cmd full release          :: Release must also build clean
```

Expected: build clean; `check_encoding.py --strict` reports no violations
(including the new composition rule); `saltests` prints `N checks, 0 failed`
with the new `TestNumberCompositionEncoding` included (N > 1221).

## 2. Fixed surfaces — Czech UI (each must show no `Â`, correct "bajtů")

Reference for the defect: [informace_o_jednotce.png](informace_o_jednotce.png).

| # | Scenario | Expected |
|---|---|---|
| 1 | Focus a local NTFS drive, **Ctrl+F1** | Využité/Volné místo and Kapacita read e.g. `967 709 523 968 bajtů` — digit groups separated by a plain-looking space, word "bajtů" intact; compare character-for-character with Explorer drive Properties |
| 2 | Select several large dirs, **Alt+F10** (Calculate occupied space) | "Occupied space"/size rows formatted correctly (mode-1 `bytes (X.Y GB)` form) |
| 3 | Open a ZIP archive, select files, Alt+F10 | Archive size results dialog size row correct |
| 4 | Attempt to pack more than the target drive's free space | "Není dostatek místa…" message shows both byte counts and the word "bajtů" correctly |
| 5 | F3 view a file > 1 MB, hover the offset area of the view (mouse over text after clicking/dragging so the offset tooltip shows) | Tooltip `pozice: 0x… (1 234 567)` with correct separators |

Plural forms spot-check (scenario 1 on a tiny RAM-disk/volume or scenario 4's
message with small numbers): 1 → "1 bajt", 2–4 → "bajty", ≥5 → "bajtů".

## 3. Regression sweep — surfaces that must render EXACTLY as before

Czech UI unless noted:

| # | Surface | Check |
|---|---|---|
| 1 | Panel size column — Options → … → Size format: **Bytes**, then **KB**, then **Mixed** | grouped digits correct in all three (already correct pre-fix) |
| 2 | Tiles view (large icons with size line) | size line unchanged |
| 3 | Information line (bottom of panel) with files selected | "X bajtů v N vybraných souborech" correct (feature-041 surface) |
| 4 | Ctrl+F1 cluster/sector rows | pure numbers unchanged |
| 5 | Find (Alt+F7), search yielding results | Size column + status-bar counts correct (features 042/043 surfaces) |
| 6 | Alt+F1 drive menu | free-space column per drive unchanged |
| 7 | Copy a large file (F5) | progress dialog "Zkopírováno: X z Y" and speed line unchanged |
| 8 | File-overwrite prompt (copy over existing) | size/date lines unchanged |
| 9 | **English UI** (switch language): repeat scenarios 2.1–2.4 | output byte-identical to pre-fix build (`1,000,186,310,656 bytes`; ASCII `LoadStrU8 == LoadStr`) |
| 10 | Plugins byte-frozen: FTP low-disk hint & dbviewer file-info (if exercised) | unchanged vs. pre-fix build — including their pre-existing separator garble (documented follow-up, research.md R6); no NEW garbling of unit words |

## 4. Contract / design references

- Binding rules: [contracts/number-format-encoding.md](contracts/number-format-encoding.md)
- Full audit and decisions: [research.md](research.md)
- Encoding states: [data-model.md](data-model.md)

## 5. Release checklist hooks (when this ships)

Changelog entry under Fixed (user's terms: garbled numbers in Drive
Information and size dialogs in Czech/Hungarian); version bump per
constitution only when a release is cut, not with this feature's merge.
No registry/config change; no plugin interface version change.

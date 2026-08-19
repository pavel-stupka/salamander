# Quickstart — validating 063-fix-filelist-encoding

Validation guide for the Make File List encoding + dialog fixes. Success criteria
referenced from [spec.md](spec.md); binding rules in
[contracts/filelist-text-encoding.md](contracts/filelist-text-encoding.md).

## Prerequisites

- Windows 11, VS2022 C++ workload, Python 3.13 (`build.cmd` requires it for the
  encoding guard).
- Full build so language modules are produced:

```batch
build.cmd full            :: Debug x64 + runtime data + plugins.ver
```

- Test fixture: a folder with files covering the fidelity matrix, e.g.
  `Příloha č. 1 — žádost.pdf`, `Smlouva_údržba.docx`, `ěščřžýáíéúůďťň.txt`,
  `Δοκιμή-测试.txt` (non-CP1250), plus several ASCII names.

## Scenario 1 — clipboard fidelity (SC-001, FR-001/FR-002, User Story 1)

1. Focus the fixture folder, select all, press **Ctrl+M**, destination
   **Clipboard**, line `$(FileName)$(CRLF)`, OK.
2. Paste into Notepad → every name matches the panel character-for-character.
3. Scriptable check (PowerShell):
   ```powershell
   Get-Clipboard | Set-Content -Encoding utf8 pasted.txt
   # compare against a dir listing of the fixture folder
   ```
4. Include the Greek/CJK file — it must survive as well (`CF_UNICODETEXT`).

## Scenario 2 — viewer and file destinations (SC-002, FR-003/FR-004, User Story 2)

1. Same selection, **Ctrl+M → Internal Viewer** → all names render correctly.
   Repeat with a large list whose first ~10,000 bytes are ASCII names and the
   accented names come last (validates the BOM decision D3).
2. **Ctrl+M → File**, target `seznam-příloh.txt`:
   - the file **name on disk** is exactly `seznam-příloh.txt` (D4);
   - opened in Notepad, the content reads correctly (UTF-8, no BOM);
   - run again with **Append to file** checked → whole file still reads correctly.

## Scenario 3 — ASCII regression guard (SC-004, FR-008)

On a folder with only ASCII names, produce the list to a file with the pre-fix
build and the fixed build; binary-compare (`fc /b`). Must be identical. Repeat for
the clipboard (paste → save → compare).

## Scenario 4 — column alignment (SC-005, FR-005)

Line template `$(FileName:max) | $(Size)` over mixed accented/plain names →
the `|` column lines up for every row in a fixed-width font. Also
`$(FileName:10)` on a long accented name → truncated output is valid UTF-8
(no `�`/mojibake at the cut point) — validates D6.

## Scenario 5 — Czech dialog surface (SC-003, FR-006/FR-007, User Stories 3–4)

1. Switch UI language to Czech (Options → Configuration → Appearance, or run with
   the Czech `.slg`), open **Ctrl+M**.
2. Click the "nápověda k řádku" hint link → tooltip text shows correct diacritics.
3. The "Soubor:" radio label is fully visible; check all labels in the dialog.
4. Repeat at 100%, 150%, 200% display scaling.
5. Spot-check one more language from the audit (German: "Interner
   Dateibetrachter" radio no longer clipped).

## Scenario 6 — non-ASCII %TEMP% (D5)

```powershell
$env:TMP = 'C:\Users\pavel\AppData\Local\Temp\čeština'; mkdir $env:TMP -Force
# launch tandemcommander.exe from this shell, then Ctrl+M → Clipboard
```
Must produce the list (pre-fix: "error creating temp file").

## Scenario 7 — clipboard sweep spot checks (D1 callers)

- Ctrl+C on an accented file name → paste correct (fileswn9).
- Find (Ctrl+F) an accented file → context menu Copy Name/Path → paste correct.
- Click the directory-line path segment of an accented path → copy → paste.
- Internal viewer: view the Ctrl+M list, select an accented line, Ctrl+C → paste.
- A message box whose text contains diacritics: Ctrl+C → paste.

## Gates (all must pass before merge)

| Gate | Command / method |
|------|------------------|
| Full build incl. encoding guard | `build.cmd full` (fails if `check_encoding.py` fails) |
| Release build sanity | `build.cmd full release` |
| Unit tests | `saltests` (existing suite, must stay green) |
| Layout round-trip | `python -m translate.relayout --module salamand --enabled-only` is content-idempotent (running it twice produces an identical diff); no clipped radio/label remains in `IDD_FILELIST` per the estimator+glyph metric |
| Manual scenarios | 1–7 above, zero defects |
| ASCII byte-diff | Scenario 3 identical |

**`--check-layout` reality (measured 2026-08-19)**: the Translator's DC-based
layout validator was **already red for `salamand` on the pre-fix state** (it
reports every clipped/overlapping control module-wide, quiet mode cannot
enumerate them, and no ignore-list is wired in), so "check-layout green" is not
an achievable gate for this feature. The honest gate is *no new findings*: the
estimator metric above proves `IDD_FILELIST` fixed, geometry changes are
grow-only into free space, and translated text is byte-untouched. The three
**disabled** languages (russian, ukrainian, chinesesimplified) are refused by
`relayout` because their `.slt` is structurally stale (pre-existing, noted in
feature 056) — they receive the new geometry automatically via the `merge` that
re-enabling them requires anyway.

## Verification status (2026-08-19, implementation session)

Automated gates: full Debug build (180 language modules) ✅, Release build ✅
(see completion report), `saltests` 1152/0 ✅ (7 new checks for
`SalU8CharCount`/`SalU8Next`), `check_encoding.py` 0 findings with the new
`CopyTextToClipboard` sink armed ✅, relayout content-idempotent ✅, per-language
fit of `IDD_FILELIST` ✅, Czech `.slt` diff verified numbers-only ✅.

Still requiring hands-on GUI verification (Scenarios 1–7): clipboard paste
fidelity, viewer/file destinations, Czech hint tooltip, label visibility at
DPI scales, non-ASCII `%TEMP%`, and the clipboard-sweep spot checks.

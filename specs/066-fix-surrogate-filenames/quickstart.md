# Quickstart: Validating the Unpaired-Surrogate Name Fix

**Feature**: 066-fix-surrogate-filenames
**References**: [spec.md](spec.md) acceptance scenarios · [contracts/name-encoding-wtf8.md](contracts/name-encoding-wtf8.md)

## Prerequisites

- Windows 11, VS2022 with the C++ Desktop workload (per `CLAUDE.md`).
- `OPENSAL_BUILD_DIR` set (e.g. `D:\Build\OpenSal\`).
- Repo root: `E:\Projects\tandemcommander`.

## 1. Build (Debug x64 — includes saltests)

```batch
build.cmd
```

Outputs land in `%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\`
(`salamand\tandemcommander.exe`, `saltests\saltests.exe`).

## 2. Unit tests (converter contract)

```batch
%OPENSAL_BUILD_DIR%tandemcommander\Debug_x64\saltests\saltests.exe
```

**Expected**: exit code 0, `FAIL` lines absent. The suite includes the new
WTF-8 pins (round-trip totality for `U+D800`–`U+DFFF`, UTF-8
byte-compatibility, decoder strictness, `SalConvertFindDataW` intake fidelity,
comparison distinctness, display derivation) alongside the full pre-existing
suite (regression gate, spec SC-005).

## 3. Generate the fixture set

Run in **Windows PowerShell 5.1** (`.NET` passes UTF-16 through unvalidated):

```powershell
$dir = "E:\Projects\tandemcommander\temp\fixtures-066"
New-Item -ItemType Directory -Force $dir | Out-Null
$names = @(
  ("Lone" + [char]0xD800 + "surrogate.txt"),      # the reported repro
  ("Lone" + [char]0xDC00 + "low.txt"),            # lone low surrogate
  ([char]0xD800 + "leading.txt"),                 # first unit
  ("trailing" + [char]0xDFFF + ".txt"),           # last unit before extension dot? keep as-is
  ("multi" + [char]0xD800 + "x" + [char]0xDBFF + "y.txt"),
  ("twin" + [char]0xD800 + ".txt"),               # look-alike pair:
  ("twin" + [char]0xD801 + ".txt")                #   differ only in the invalid unit
)
foreach ($n in $names) {
  Set-Content -LiteralPath (Join-Path $dir $n) -Value "fixture 066" -Encoding Ascii
}
$sub = Join-Path $dir ("dir" + [char]0xD800 + "sub")   # surrogate-named folder
New-Item -ItemType Directory -Force $sub | Out-Null
Set-Content -LiteralPath (Join-Path $sub "child.txt") -Value "child" -Encoding Ascii
```

Helper to dump true code units (used for the fidelity checks below):

```powershell
Get-ChildItem -LiteralPath $dir -Recurse -Force | ForEach-Object {
  $cu = ($_.Name.ToCharArray() | ForEach-Object { '{0:X4}' -f [int]$_ }) -join ' '
  "$($_.FullName) | $cu"
}
```

## 4. Manual acceptance walk (maps to spec user stories)

Start `tandemcommander.exe`, navigate one panel to `temp\fixtures-066\`,
the other to an empty scratch folder.

| # | Action | Expected (spec ref) |
|---|--------|---------------------|
| 1 | Focus `Lone�surrogate.txt`, **F5** copy | Completes with no error; destination name **code-unit identical** (verify with the dump helper) (US2, SC-002) |
| 2 | **F6** move the copy back | Completes; source gone, name identical (US2) |
| 3 | **F8** delete → Recycle Bin | Completes; file appears in Recycle Bin (US1, SC-001) |
| 4 | Recreate fixtures; **Shift+F8** permanent delete | Completes; file gone from disk (US1) |
| 5 | Copy the whole `fixtures-066` tree (includes `dir�sub`) | Every entry arrives, names identical at all depths (US2/FR-004) |
| 6 | Delete the copied tree | Recursive delete completes, nothing left (US1/FR-004) |
| 7 | Focus `twin�.txt` (`D800`), **F8** it | Only that twin disappears; the `D801` twin survives (FR-006, SC-004) |
| 8 | **F3** on a surrogate-named file | Internal viewer shows `fixture 066` content (US3) |
| 9 | Rename a surrogate-named file to `plain.txt` | Rename succeeds (US3) |
| 10 | Change attributes (read-only) on one | Applied to the real file — verify in Explorer (US3) |
| 11 | Look at the info line / panel while focused | Name renders with one replacement glyph (`?` or notdef box) per invalid unit — **no mojibake**, no multi-character garbage (FR-005) |
| 12 | Enter `dir�sub`, operate on `child.txt` | Listing, copy, delete of the child all work (edge case: surrogate in an ancestor component) |

**Explorer parity spot-check** (SC-003): perform 1, 3, 9 in Windows Explorer
on regenerated fixtures — Tandem Commander must succeed wherever Explorer does.

## 5. Regression checks

- Feature-041 fixture set (`temp\fixtures-041\`, valid non-ASCII names):
  F5/F6/F8/F3 all still work; panel display unchanged (SC-005).
- Saved configuration: set a surrogate-named folder as the panel path, exit,
  relaunch — the panel restores to that folder (registry facade round trip,
  contract obligation), and no other saved value is disturbed.
- Full `saltests` suite green (step 2 covers it).

## Cleanup

```powershell
Remove-Item -LiteralPath "E:\Projects\tandemcommander\temp\fixtures-066" -Recurse -Force
```

(`temp\` is not shipped and not part of any build output.)

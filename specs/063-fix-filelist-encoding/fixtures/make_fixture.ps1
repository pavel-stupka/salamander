# Creates the Make File List test fixture folder (quickstart.md Prerequisites).
# Usage: powershell -ExecutionPolicy Bypass -File make_fixture.ps1 [-Path <folder>]
param([string]$Path = "$env:USERPROFILE\Desktop\mfl-fixture")

New-Item -ItemType Directory -Force $Path | Out-Null

$names = @(
    # Czech diacritics (the reported case)
    'Příloha č. 1 — žádost.pdf',
    'Smlouva_údržba.docx',
    'ěščřžýáíéúůďťň.txt',
    # outside CP1250 (FR-002 acceptance scenario 2)
    'Δοκιμή-测试.txt',
    # ASCII controls (FR-008 regression guard)
    'plain.txt',
    'readme.md',
    'a-very-long-ascii-name-for-alignment-tests.log',
    # mixed accents for the :max alignment check (SC-005)
    'krátký.txt',
    'x.txt'
)

foreach ($n in $names) {
    $f = Join-Path $Path $n
    if (-not (Test-Path -LiteralPath $f)) {
        Set-Content -LiteralPath $f -Value "fixture" -Encoding ascii
    }
}
Write-Host "Fixture created in $Path ($($names.Count) files)"

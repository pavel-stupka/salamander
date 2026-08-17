<# : ============================================================================
@echo off
setlocal
set "TCMIG_TESTSELF=%~f0"
powershell -NoProfile -ExecutionPolicy Bypass -Command "[ScriptBlock]::Create([IO.File]::ReadAllText($env:TCMIG_TESTSELF)).Invoke()"
exit /b %ERRORLEVEL%
: ============================================================================
: Test harness for utils\migrate-altap-settings.cmd (feature 057).
: Runs the wizard end-to-end against fixture hives imported under
: HKCU\Software\TCMigTest (never the real product roots), driving it via
: redirected stdin, and asserts registry state, exit codes and summary text.
: Scenarios: see specs/057-altap-settings-migration/quickstart.md (S1-S8).
: ============================================================================
#>

$ErrorActionPreference = 'Stop'

$TestDir = Split-Path -Parent $env:TCMIG_TESTSELF
$UtilPath = Join-Path (Split-Path -Parent $TestDir) 'migrate-altap-settings.cmd'
$FixDir = Join-Path $TestDir 'fixtures'

$ScratchBase = 'Software\TCMigTest'
$SrcAS40 = "$ScratchBase\Altap Salamander 4.0"
$SrcAS25 = "$ScratchBase\Altap Salamander 2.51"
$Dst = "$ScratchBase\Tandem Commander 0.1"

$WorkBase = Join-Path $env:TEMP ('tcmig-tests-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
[void](New-Item -ItemType Directory -Path $WorkBase -Force)

$RegExe = Join-Path $env:SystemRoot 'System32\reg.exe'
$HKCU = [Microsoft.Win32.Registry]::CurrentUser

$script:Pass = 0
$script:Fail = 0
$script:CurrentScenario = ''

function Check([string]$name, [bool]$cond, [string]$detail = '') {
    if ($cond) {
        $script:Pass++
        Write-Host ("  PASS  " + $script:CurrentScenario + ": " + $name)
    } else {
        $script:Fail++
        $msg = "  FAIL  " + $script:CurrentScenario + ": " + $name
        if ($detail -ne '') { $msg += "  [" + $detail + "]" }
        Write-Host $msg
    }
}

function Reset-Scratch {
    # reg.exe stderr must never surface into PowerShell (PS 5.1 wraps native
    # stderr into error records) - let cmd.exe swallow it.
    cmd /c ('reg delete "HKCU\' + $ScratchBase + '" /f >nul 2>&1')
    $global:LASTEXITCODE = 0
}

function Import-Fixture([string]$name) {
    $f = Join-Path $FixDir $name
    cmd /c ('reg import "' + $f + '" >nul 2>&1')
    if ($LASTEXITCODE -ne 0) { throw "fixture import failed: $name" }
}

function Export-Key([string]$path) {
    # Returns the reg-export text of a key, or $null when the key is absent.
    $k = $HKCU.OpenSubKey($path, $false)
    if ($null -eq $k) { return $null }
    $k.Close()
    $tmp = Join-Path $WorkBase ('export-' + [Guid]::NewGuid().ToString('N') + '.reg')
    cmd /c ('reg export "HKCU\' + $path + '" "' + $tmp + '" /y >nul 2>&1')
    if ($LASTEXITCODE -ne 0) { return $null }
    $text = [IO.File]::ReadAllText($tmp)
    Remove-Item -LiteralPath $tmp -Force
    return $text
}

function Test-Key([string]$path) {
    $k = $HKCU.OpenSubKey($path, $false)
    if ($null -ne $k) { $k.Close(); return $true }
    return $false
}

function Get-Val([string]$path, [string]$name, $default = $null) {
    $k = $HKCU.OpenSubKey($path, $false)
    if ($null -eq $k) { return $default }
    try { return $k.GetValue($name, $default) } finally { $k.Close() }
}

function Test-ValueExists([string]$path, [string]$name) {
    $k = $HKCU.OpenSubKey($path, $false)
    if ($null -eq $k) { return $false }
    try { return @($k.GetValueNames()) -contains $name } finally { $k.Close() }
}

function Count-Consecutive([string]$path) {
    $n = 0; $i = 1
    while (Test-Key ($path + '\' + $i)) { $n++; $i++ }
    return $n
}

function Count-HotPathSlots([string]$root) {
    $n = 0
    for ($i = 0; $i -le 30; $i++) {
        $p = Get-Val ($root + '\Hot Paths\' + $i) 'Path' ''
        if ($p -is [string] -and $p -ne '') { $n++ }
    }
    return $n
}

$script:RunCounter = 0
function Run-Wizard {
    # Runs the wizard with the given stdin answers; returns @{Exit; Output; OutDir}
    param([string]$srcOverride, [string[]]$answers)
    $script:RunCounter++
    $outDir = Join-Path $WorkBase ("run" + $script:RunCounter)
    [void](New-Item -ItemType Directory -Path $outDir -Force)
    $ansFile = Join-Path $outDir 'answers.txt'
    $logFile = Join-Path $outDir 'output.txt'
    [IO.File]::WriteAllLines($ansFile, $answers, [Text.Encoding]::ASCII)

    $env:TCMIG_SOURCE_ROOT = $srcOverride
    $env:TCMIG_DEST_ROOT = 'HKCU\' + $Dst
    $env:TCMIG_OUT_DIR = $outDir
    $env:TCMIG_SKIP_PROCCHECK = '1'
    try {
        $cmdLine = '"' + $UtilPath + '" < "' + $ansFile + '" > "' + $logFile + '" 2>&1'
        cmd /c $cmdLine
        $code = $LASTEXITCODE
    } finally {
        Remove-Item env:TCMIG_SOURCE_ROOT, env:TCMIG_DEST_ROOT, env:TCMIG_OUT_DIR, env:TCMIG_SKIP_PROCCHECK -ErrorAction SilentlyContinue
    }
    $output = ''
    if (Test-Path -LiteralPath $logFile) { $output = [IO.File]::ReadAllText($logFile) }
    return @{ Exit = $code; Output = $output; OutDir = $outDir }
}

function Contains([string]$haystack, [string]$needle) {
    return $haystack.IndexOf($needle, [StringComparison]::OrdinalIgnoreCase) -ge 0
}

# Source-immutability wrapper (quickstart S3): each scenario calls
# Snap-Sources right AFTER importing its fixtures; Run-Scenario verifies
# byte-identity of every snapshotted source at the end.
$script:SrcSnaps = @{}

function Snap-Sources([string[]]$roots) {
    foreach ($r in $roots) { $script:SrcSnaps[$r] = Export-Key $r }
}

function Run-Scenario {
    param([string]$name, [scriptblock]$body)
    $script:CurrentScenario = $name
    $script:SrcSnaps = @{}
    Write-Host ''
    Write-Host ("--- " + $name + " ---")
    & $body
    foreach ($r in @($script:SrcSnaps.Keys)) {
        $after = Export-Key $r
        Check "S3 source unchanged ($r)" ($after -eq $script:SrcSnaps[$r])
    }
}

$SelAll = @('', 'D', 'y')                       # W3 default, W4 done (all), confirm
$SelHotFtp = @('', 'N', '1', '10', 'D', 'y')    # only hotpaths (#1) + ftp (#10)
$SelHotOnly = @('', 'N', '1', 'D', 'y')
$SelFtpOnly = @('', 'N', '10', 'D', 'y')

# ============================================================================
Write-Host ("Harness work dir: " + $WorkBase)

# --- S1: selective transfer (US1) -------------------------------------------
Run-Scenario 'S1 selective transfer' {
    Reset-Scratch
    Import-Fixture 'altap40-full.reg'
    Snap-Sources @($SrcAS40)
    $r = Run-Wizard ('HKCU\' + $SrcAS40) $SelHotFtp
    Check 'exit 0' ($r.Exit -eq 0) ("exit=" + $r.Exit)
    Check 'hot path 1 content' ((Get-Val ($Dst + '\Hot Paths\1') 'Path' '') -eq 'E:\Projects')
    Check '7 hot path slots' ((Count-HotPathSlots $Dst) -eq 7)
    Check '12 FTP bookmarks' ((Count-Consecutive ($Dst + '\Plugins Configuration\FTP\Bookmarks')) -eq 12)
    Check 'PasswordS blob intact' (((Get-Val ($Dst + '\Plugins Configuration\FTP\Bookmarks\1') 'PasswordS' @()) | Measure-Object).Count -eq 18)
    Check 'no User Menu written' (-not (Test-Key ($Dst + '\User Menu')))
    Check 'no Colors written' (-not (Test-Key ($Dst + '\Colors')))
    Check 'no Viewers written' (-not (Test-Key ($Dst + '\Viewers')))
    Check 'Version stamped 105' ((Get-Val ($Dst + '\Version') 'Configuration' 0) -eq 105)
    Check 'Configuration subkey exists, empty' ((Test-Key ($Dst + '\Configuration')) -and -not (Test-ValueExists ($Dst + '\Configuration') 'Skill Level'))
    Check 'summary reports hot paths' (Contains $r.Output 'TRANSFERRED  Directory hot paths (7 items)')
    Check 'summary reports 12 servers' (Contains $r.Output '(12 servers)')
    Check 'no password manager copied' (-not (Test-Key ($Dst + '\Password Manager')))
}

# --- S8: skip transparency (US1 / FR-005, FR-011) ----------------------------
Run-Scenario 'S8 skip transparency' {
    Reset-Scratch
    Import-Fixture 'altap40-full.reg'
    Snap-Sources @($SrcAS40)
    $r = Run-Wizard ('HKCU\' + $SrcAS40) $SelAll
    Check 'exit 0' ($r.Exit -eq 0) ("exit=" + $r.Exit)
    Check 'no Packers & Unpackers' (-not (Test-Key ($Dst + '\Packers & Unpackers')))
    Check 'no Internal ZIP Packer' (-not (Test-Key ($Dst + '\Internal ZIP Packer')))
    Check 'no Window key' (-not (Test-Key ($Dst + '\Window')))
    Check 'no Left Panel key' (-not (Test-Key ($Dst + '\Left Panel')))
    Check 'no foreign plugin config' (-not (Test-Key ($Dst + '\Plugins Configuration\UnRAR')))
    Check 'no Plugins registration copied' (-not (Test-Key ($Dst + '\Plugins')))
    Check 'Version stays 105 (not 103)' ((Get-Val ($Dst + '\Version') 'Configuration' 0) -eq 105)
    Check 'Language excluded' (-not (Test-ValueExists ($Dst + '\Configuration') 'Language'))
    Check 'Top ToolBar excluded' (-not (Test-ValueExists ($Dst + '\Configuration') 'Top ToolBar'))
    Check 'Plugins.ver counter excluded' (-not (Test-ValueExists ($Dst + '\Configuration') 'Plugins.ver Version (x64)'))
    Check 'Conversion Table excluded' (-not (Test-ValueExists ($Dst + '\Configuration') 'Conversion Table'))
    Check 'Skill Level copied' ((Get-Val ($Dst + '\Configuration') 'Skill Level' 0) -eq 2)
    Check 'histories excluded' (-not (Test-Key ($Dst + '\Configuration\Command History')))
    Check 'Working Directories excluded' (-not (Test-Key ($Dst + '\Configuration\Working Directories')))
    Check 'Find Options copied' (Test-Key ($Dst + '\Configuration\Find Options\1'))
    Check 'viewers renumbered to 2 rows' ((Count-Consecutive ($Dst + '\Viewers')) -eq 2)
    Check 'viewer row 2 is *.log' ((Get-Val ($Dst + '\Viewers\2') 'Masks' '') -eq '*.log')
    Check 'plugin-position row reported' (Contains $r.Output 'plugin viewer by position')
    Check 'pipe-mask row reported' (Contains $r.Output "masks containing '|'")
    Check 'default dirs C kept' (Test-ValueExists ($Dst + '\Default Directories') 'C')
    Check 'default dirs E dropped' (-not (Test-ValueExists ($Dst + '\Default Directories') 'E'))
    Check 'default dirs E reported' (Contains $r.Output "drive value 'E'")
    Check 'viewer settings copied' (Test-ValueExists ($Dst + '\Viewer') 'Tabelator Size')
    Check 'viewer Find Text excluded' (-not (Test-ValueExists ($Dst + '\Viewer') 'Find Text'))
    Check 'Altap path flagged in NOTES' (Contains $r.Output 'references the old Altap Salamander installation')
    Check 'archiver skip reason shown' (Contains $r.Output 'Archiver settings')
    Check 'foreign plugins reported' (Contains $r.Output 'UnRAR')
    Check 'highlight rule copied' ((Get-Val ($Dst + '\Colors\Panel Items Hilighting\1') 'Masks' '') -eq '*.exe;*.bat')
    Check 'ZIP plugin config copied' ((Get-Val ($Dst + '\Plugins Configuration\ZIP') 'Compression Level' 0) -eq 9)
    Check 'Confirmation copied' ((Get-Val ($Dst + '\Configuration\Confirmation') 'File Overwrite' 0) -eq 1)
}

# --- S2: replace semantics + backup/restore (US2) ----------------------------
Run-Scenario 'S2 replace + restore' {
    Reset-Scratch
    Import-Fixture 'altap40-full.reg'
    Import-Fixture 'tc-preexisting.reg'
    Snap-Sources @($SrcAS40)
    $preExport = Export-Key $Dst
    $r = Run-Wizard ('HKCU\' + $SrcAS40) $SelHotOnly
    Check 'exit 0' ($r.Exit -eq 0) ("exit=" + $r.Exit)
    Check 'hot paths replaced' ((Get-Val ($Dst + '\Hot Paths\1') 'Path' '') -eq 'E:\Projects')
    $tcPathGone = $true
    for ($i = 0; $i -le 30; $i++) {
        if ((Get-Val ($Dst + '\Hot Paths\' + $i) 'Path' '') -eq 'C:\TC') { $tcPathGone = $false }
    }
    Check 'pre-existing TC hot path gone (wholesale replace)' $tcPathGone
    Check 'FTP untouched' ((Get-Val ($Dst + '\Plugins Configuration\FTP\Bookmarks\1') 'Name' '') -eq 'TC Own Server')
    Check 'FTP count still 1' ((Count-Consecutive ($Dst + '\Plugins Configuration\FTP\Bookmarks')) -eq 1)
    Check 'Theme Mode untouched' ((Get-Val ($Dst + '\Configuration') 'Theme Mode' 0) -eq 1)
    Check 'TC packer settings untouched' ((Get-Val ($Dst + '\Packers & Unpackers\Custom Packers\1') 'Title' '') -eq 'TC packer')
    $backup = @(Get-ChildItem -Path $r.OutDir -Filter 'tc-settings-backup-*.reg')
    $restore = @(Get-ChildItem -Path $r.OutDir -Filter 'tc-settings-restore-*.cmd')
    Check 'backup .reg exists' ($backup.Count -eq 1)
    Check 'restore .cmd exists' ($restore.Count -eq 1)
    if ($restore.Count -eq 1) {
        cmd /c ('"' + $restore[0].FullName + '" < nul > nul 2>&1')
        $postExport = Export-Key $Dst
        Check 'restore returns pre-run state byte-for-byte' ($postExport -eq $preExport)
    }
}

# --- S4: cancellation and refusal paths (US2 / FR-008, FR-012) ---------------
Run-Scenario 'S4 cancel and refusals' {
    Reset-Scratch
    Import-Fixture 'altap40-full.reg'
    Snap-Sources @($SrcAS40)

    $r = Run-Wizard ('HKCU\' + $SrcAS40) @('', 'D', 'n')
    Check 'declined confirm: exit 3' ($r.Exit -eq 3) ("exit=" + $r.Exit)
    Check 'declined confirm: no destination created' (-not (Test-Key $Dst))
    $files = @(Get-ChildItem -Path $r.OutDir -Exclude 'answers.txt', 'output.txt')
    Check 'declined confirm: no backup/summary files' ($files.Count -eq 0)

    $r = Run-Wizard ('HKCU\' + $SrcAS40) @('', 'N', 'D')
    Check 'zero selection: exit 3' ($r.Exit -eq 3) ("exit=" + $r.Exit)
    Check 'zero selection: no destination created' (-not (Test-Key $Dst))

    $r = Run-Wizard 'HKCU\Software\TCMigTest\NoSuchRoot' @('')
    Check 'missing source: exit 11' ($r.Exit -eq 11) ("exit=" + $r.Exit)

    $r = Run-Wizard ('HKCU\' + $SrcAS40) @('', 'zz', 'zz', 'zz')
    Check 'repeated invalid input: exit 2' ($r.Exit -eq 2) ("exit=" + $r.Exit)
    Check 'invalid input: no destination created' (-not (Test-Key $Dst))
}

# --- S5: old/minimal source with transforms (US3) -----------------------------
Run-Scenario 'S5 old minimal source' {
    Reset-Scratch
    Import-Fixture 'altap25-minimal.reg'
    Snap-Sources @($SrcAS25)
    $r = Run-Wizard ('HKCU\' + $SrcAS25) $SelAll
    Check 'exit 0' ($r.Exit -eq 0) ("exit=" + $r.Exit)
    Check 'FTP not offered' (-not (Contains $r.Output 'FTP connections'))
    Check 'T1: dollars doubled in hot path' ((Get-Val ($Dst + '\Hot Paths\1') 'Path' '') -eq 'C:\$$MONEY$$')
    Check 'T2: masks lowercased' ((Get-Val ($Dst + '\Viewers\1') 'Masks' '') -eq '*.txt;*.doc')
    Check 'confirmations copied' ((Get-Val ($Dst + '\Configuration\Confirmation') 'File Overwrite' 0) -eq 1)
}

# --- S6: multiple sources (US3) -----------------------------------------------
Run-Scenario 'S6 multiple sources' {
    Reset-Scratch
    Import-Fixture 'altap40-full.reg'
    Import-Fixture 'altap25-minimal.reg'
    Snap-Sources @($SrcAS40, $SrcAS25)
    $both = ('HKCU\' + $SrcAS40 + ';HKCU\' + $SrcAS25)
    $r = Run-Wizard $both @('2', 'D', 'y')
    Check 'exit 0' ($r.Exit -eq 0) ("exit=" + $r.Exit)
    Check 'both sources listed' ((Contains $r.Output '[1] Altap Salamander 4.0') -and (Contains $r.Output '[2] Altap Salamander 2.51'))
    Check 'older source data landed' ((Get-Val ($Dst + '\Hot Paths\1') 'Path' '') -eq 'C:\$$MONEY$$')
    Check 'newer source data NOT landed' ((Get-Val ($Dst + '\Hot Paths\2') 'Path' '') -eq '')
}

# --- S7: re-run idempotence (US4) ---------------------------------------------
Run-Scenario 'S7 re-run idempotence' {
    Reset-Scratch
    Import-Fixture 'altap40-full.reg'
    Snap-Sources @($SrcAS40)
    $r1 = Run-Wizard ('HKCU\' + $SrcAS40) $SelHotFtp
    $exp1hot = Export-Key ($Dst + '\Hot Paths')
    $exp1ftp = Export-Key ($Dst + '\Plugins Configuration')
    $r2 = Run-Wizard ('HKCU\' + $SrcAS40) $SelHotFtp
    $exp2hot = Export-Key ($Dst + '\Hot Paths')
    $exp2ftp = Export-Key ($Dst + '\Plugins Configuration')
    Check 'both runs exit 0' (($r1.Exit -eq 0) -and ($r2.Exit -eq 0))
    Check 'hot paths byte-identical after re-run' ($exp1hot -eq $exp2hot)
    Check 'plugin configs byte-identical after re-run' ($exp1ftp -eq $exp2ftp)

    Reset-Scratch
    Import-Fixture 'altap40-full.reg'
    Snap-Sources @($SrcAS40)
    $r1 = Run-Wizard ('HKCU\' + $SrcAS40) $SelHotOnly
    $r2 = Run-Wizard ('HKCU\' + $SrcAS40) $SelFtpOnly
    Check 'staggered runs exit 0' (($r1.Exit -eq 0) -and ($r2.Exit -eq 0))
    Check 'category A survives category B run' ((Count-HotPathSlots $Dst) -eq 7)
    Check 'category B landed, no duplicates' ((Count-Consecutive ($Dst + '\Plugins Configuration\FTP\Bookmarks')) -eq 12)
}

# --- S9: master-password handling (US1 / FR-010, research R7) -----------------
Run-Scenario 'S9 master password rules' {
    # (a) destination already has its own master password: PasswordE stripped,
    #     Save Password cleared, destination verifier untouched.
    Reset-Scratch
    Import-Fixture 'altap40-full.reg'
    Snap-Sources @($SrcAS40)
    cmd /c ('reg add "HKCU\' + $Dst + '\Password Manager" /v "Use Master Password" /t REG_DWORD /d 1 /f >nul 2>&1')
    cmd /c ('reg add "HKCU\' + $Dst + '\Password Manager" /v "Master Password Verifier" /t REG_BINARY /d 99887766 /f >nul 2>&1')
    $r = Run-Wizard ('HKCU\' + $SrcAS40) $SelFtpOnly
    Check 'dest-MP: exit 0' ($r.Exit -eq 0) ("exit=" + $r.Exit)
    Check 'dest-MP: PasswordE stripped' (-not (Test-ValueExists ($Dst + '\Plugins Configuration\FTP\Bookmarks\2') 'PasswordE'))
    Check 'dest-MP: Save Password cleared' (-not (Test-ValueExists ($Dst + '\Plugins Configuration\FTP\Bookmarks\2') 'Save Password'))
    Check 'dest-MP: rest of bookmark kept' ((Get-Val ($Dst + '\Plugins Configuration\FTP\Bookmarks\2') 'User' '') -eq 'bob')
    Check 'dest-MP: scrambled password kept' (Test-ValueExists ($Dst + '\Plugins Configuration\FTP\Bookmarks\1') 'PasswordS')
    Check 'dest-MP: re-enter reported' (Contains $r.Output 'stored password not transferred')
    $ver = Get-Val ($Dst + '\Password Manager') 'Master Password Verifier' @()
    Check 'dest-MP: destination verifier untouched' ((($ver | Measure-Object).Count -eq 4) -and ($ver[0] -eq 0x99))

    # (b) only the source uses a master password: the pair is copied atomically.
    Reset-Scratch
    Import-Fixture 'altap40-full.reg'
    cmd /c ('reg add "HKCU\' + $SrcAS40 + '\Password Manager" /v "Use Master Password" /t REG_DWORD /d 1 /f >nul 2>&1')
    cmd /c ('reg add "HKCU\' + $SrcAS40 + '\Password Manager" /v "Master Password Verifier" /t REG_BINARY /d 11223344 /f >nul 2>&1')
    Snap-Sources @($SrcAS40)
    $r = Run-Wizard ('HKCU\' + $SrcAS40) $SelFtpOnly
    Check 'src-MP: exit 0' ($r.Exit -eq 0) ("exit=" + $r.Exit)
    Check 'src-MP: flag copied' ((Get-Val ($Dst + '\Password Manager') 'Use Master Password' 0) -eq 1)
    $ver = Get-Val ($Dst + '\Password Manager') 'Master Password Verifier' @()
    Check 'src-MP: verifier copied' ((($ver | Measure-Object).Count -eq 4) -and ($ver[0] -eq 0x11))
    Check 'src-MP: PasswordE kept' (Test-ValueExists ($Dst + '\Plugins Configuration\FTP\Bookmarks\2') 'PasswordE')
    Check 'src-MP: guidance in NOTES' (Contains $r.Output 'use the same master password')
}

# ============================================================================
Reset-Scratch
Write-Host ''
Write-Host ("Result: " + $script:Pass + " passed, " + $script:Fail + " failed.")
if ($script:Fail -gt 0) { exit 1 }
exit 0

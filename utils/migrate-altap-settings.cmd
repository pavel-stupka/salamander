<# : ============================================================================
@echo off
setlocal
set "TCMIG_SELF=%~f0"
powershell -NoProfile -ExecutionPolicy Bypass -Command "[ScriptBlock]::Create([IO.File]::ReadAllText($env:TCMIG_SELF)).Invoke()"
exit /b %ERRORLEVEL%
: ============================================================================
: Tandem Commander - Altap Salamander settings migration utility
: (feature 057; see specs/057-altap-settings-migration/ in the repository)
:
: A one-time, standalone helper: copies selected per-user settings from an
: Altap/Servant Salamander configuration (HKCU\Software\Altap\..., read-only)
: into Tandem Commander's registry root (HKCU\Software\Tandem Commander\0.1).
: Double-click to run. Requires only stock Windows 11 (PowerShell 5.1).
:
: The batch header above relaunches the PowerShell payload below.
: This file must stay pure ASCII with no byte-order mark.
: ============================================================================
#>

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

# ----------------------------------------------------------------------------
# Constants and test-only environment overrides
# (TCMIG_* variables are FOR TESTING ONLY - see contracts/wizard-flow.md)
# ----------------------------------------------------------------------------

$EXIT_OK       = 0
$EXIT_BADINPUT = 2
$EXIT_CANCEL   = 3
$EXIT_RUNNING  = 10
$EXIT_NOSOURCE = 11
$EXIT_DESTFAIL = 12
$EXIT_MIDFAIL  = 13

$TC_CONFIG_VERSION = 105   # THIS_CONFIG_VERSION of Tandem Commander 0.1

function Strip-HivePrefix([string]$p) {
    $s = $p.Trim()
    foreach ($prefix in @('HKEY_CURRENT_USER\', 'HKCU:\', 'HKCU\')) {
        if ($s.ToUpperInvariant().StartsWith($prefix.ToUpperInvariant())) {
            return $s.Substring($prefix.Length)
        }
    }
    return $s
}

$DestRoot = 'Software\Tandem Commander\0.1'
if ($env:TCMIG_DEST_ROOT) { $DestRoot = Strip-HivePrefix $env:TCMIG_DEST_ROOT }

$SkipProcCheck = ($env:TCMIG_SKIP_PROCCHECK -eq '1')

# Historical per-user configuration roots, verbatim from the product's own
# pre-feature-032 SalamanderConfigurationRoots[] (newest first). Note the
# 2.5x-era build tags contain a space ("(DB 72)"), the 3.x/4.0 ones do not.
$BuiltinSourceRoots = @(
    'Software\Altap\Altap Salamander 4.0',
    'Software\Altap\Altap Salamander 4.0 beta 1 (DB177)',
    'Software\Altap\Altap Salamander 4.0 beta 1 (DB171)',
    'Software\Altap\Altap Salamander 3.08',
    'Software\Altap\Altap Salamander 4.0 beta 1 (DB168)',
    'Software\Altap\Altap Salamander 3.07',
    'Software\Altap\Altap Salamander 3.1 beta 1 (DB162)',
    'Software\Altap\Altap Salamander 3.1 beta 1 (DB159)',
    'Software\Altap\Altap Salamander 3.06',
    'Software\Altap\Altap Salamander 3.1 beta 1 (DB153)',
    'Software\Altap\Altap Salamander 3.05',
    'Software\Altap\Altap Salamander 3.1 beta 1 (DB147)',
    'Software\Altap\Altap Salamander 3.04',
    'Software\Altap\Altap Salamander 3.1 beta 1 (DB141)',
    'Software\Altap\Altap Salamander 3.03',
    'Software\Altap\Altap Salamander 3.1 beta 1 (DB135)',
    'Software\Altap\Altap Salamander 3.02',
    'Software\Altap\Altap Salamander 3.1 beta 1 (DB129)',
    'Software\Altap\Altap Salamander 3.01',
    'Software\Altap\Altap Salamander 3.1 beta 1 (DB123)',
    'Software\Altap\Altap Salamander 3.0',
    'Software\Altap\Altap Salamander 3.0 beta 5 (DB117)',
    'Software\Altap\Altap Salamander 3.0 beta 4',
    'Software\Altap\Altap Salamander 3.0 beta 4 (DB111)',
    'Software\Altap\Altap Salamander 3.0 beta 3',
    'Software\Altap\Altap Salamander 3.0 beta 3 (DB105)',
    'Software\Altap\Altap Salamander 3.0 beta 3 (PB103)',
    'Software\Altap\Altap Salamander 3.0 beta 3 (DB100)',
    'Software\Altap\Altap Salamander 3.0 beta 2',
    'Software\Altap\Altap Salamander 3.0 beta 2 (DB94)',
    'Software\Altap\Altap Salamander 3.0 beta 1',
    'Software\Altap\Altap Salamander 3.0 beta 1 (DB88)',
    'Software\Altap\Altap Salamander 3.0 beta 1 (PB87)',
    'Software\Altap\Altap Salamander 3.0 beta 1 (DB83)',
    'Software\Altap\Altap Salamander 3.0 beta 1 (DB80)',
    'Software\Altap\Altap Salamander 3.0 beta 1 (PB79)',
    'Software\Altap\Altap Salamander 3.0 beta 1 (DB76)',
    'Software\Altap\Altap Salamander 3.0 beta 1 (PB75)',
    'Software\Altap\Altap Salamander 2.55 beta 1 (DB 72)',
    'Software\Altap\Altap Salamander 2.54',
    'Software\Altap\Altap Salamander 2.54 beta 1 (DB 66)',
    'Software\Altap\Altap Salamander 2.53',
    'Software\Altap\Altap Salamander 2.53 (DB 60)',
    'Software\Altap\Altap Salamander 2.53 beta 2',
    'Software\Altap\Altap Salamander 2.53 beta 2 (IB 55)',
    'Software\Altap\Altap Salamander 2.53 (DB 52)',
    'Software\Altap\Altap Salamander 2.53 beta 1',
    'Software\Altap\Altap Salamander 2.53 beta 1 (DB 46)',
    'Software\Altap\Altap Salamander 2.53 beta 1 (PB 44)',
    'Software\Altap\Altap Salamander 2.53 beta 1 (DB 41)',
    'Software\Altap\Altap Salamander 2.53 beta 1 (DB 39)',
    'Software\Altap\Altap Salamander 2.53 beta 1 (PB 38)',
    'Software\Altap\Altap Salamander 2.53 beta 1 (DB 36)',
    'Software\Altap\Altap Salamander 2.53 beta 1 (DB 33)',
    'Software\Altap\Altap Salamander 2.52',
    'Software\Altap\Altap Salamander 2.52 (DB 30)',
    'Software\Altap\Altap Salamander 2.52 beta 2',
    'Software\Altap\Altap Salamander 2.52 beta 1',
    'Software\Altap\Altap Salamander 2.51',
    'Software\Altap\Altap Salamander 2.5',
    'Software\Altap\Altap Salamander 2.5 RC3',
    'Software\Altap\Servant Salamander 2.5 RC3',
    'Software\Altap\Servant Salamander 2.5 RC2',
    'Software\Altap\Servant Salamander 2.5 RC1',
    'Software\Altap\Servant Salamander 2.5 beta 12',
    'Software\Altap\Servant Salamander 2.5 beta 11',
    'Software\Altap\Servant Salamander 2.5 beta 10',
    'Software\Altap\Servant Salamander 2.5 beta 9',
    'Software\Altap\Servant Salamander 2.5 beta 8',
    'Software\Altap\Servant Salamander 2.5 beta 7',
    'Software\Altap\Servant Salamander 2.5 beta 6',
    'Software\Altap\Servant Salamander 2.5 beta 5',
    'Software\Altap\Servant Salamander 2.5 beta 4',
    'Software\Altap\Servant Salamander 2.5 beta 3',
    'Software\Altap\Servant Salamander 2.5 beta 2',
    'Software\Altap\Servant Salamander 2.5 beta 1',
    'Software\Altap\Servant Salamander 2.1 beta 1',
    'Software\Altap\Servant Salamander 2.0',
    'Software\Altap\Servant Salamander 1.6 beta 7',
    'Software\Altap\Servant Salamander 1.6 beta 6',
    'Software\Altap\Servant Salamander',
    'Software\Salamander'
)

$SourceRootList = $BuiltinSourceRoots
if ($env:TCMIG_SOURCE_ROOT) {
    $SourceRootList = @($env:TCMIG_SOURCE_ROOT -split ';' | Where-Object { $_.Trim() -ne '' } | ForEach-Object { Strip-HivePrefix $_ })
}

# Output directory: next to the script if writable, else Documents.
function Resolve-OutDir {
    if ($env:TCMIG_OUT_DIR) { return $env:TCMIG_OUT_DIR }
    $selfDir = $null
    if ($env:TCMIG_SELF) { $selfDir = Split-Path -Parent $env:TCMIG_SELF }
    if ($selfDir) {
        try {
            $probe = Join-Path $selfDir ('.tcmig-probe-' + [Guid]::NewGuid().ToString('N') + '.tmp')
            [IO.File]::WriteAllText($probe, 'x')
            Remove-Item -LiteralPath $probe -Force
            return $selfDir
        } catch { }
    }
    return [Environment]::GetFolderPath('MyDocuments')
}

# ----------------------------------------------------------------------------
# Console I/O helpers (must work with redirected stdin - see wizard contract)
# ----------------------------------------------------------------------------

$script:InvalidStreak = 0

function Read-Answer([string]$prompt, [string]$default) {
    # Returns the trimmed answer; empty input selects the default.
    # EOF on stdin means "user is gone" -> cancel (exit 3, no writes happen
    # after this point unless the transfer already started).
    Write-Host -NoNewline ($prompt + ' ')
    $line = [Console]::In.ReadLine()
    if ($null -eq $line) {
        Write-Host ''
        Write-Host 'Input ended - cancelled. Nothing was changed.'
        exit $EXIT_CANCEL
    }
    $line = $line.Trim()
    if ($line -eq '') { return $default }
    return $line
}

function Register-InvalidInput([string]$what) {
    $script:InvalidStreak++
    Write-Host ("Invalid input: " + $what)
    if ($script:InvalidStreak -ge 3) {
        Write-Host 'Three invalid inputs in a row - aborting. Nothing was changed.'
        exit $EXIT_BADINPUT
    }
}

function Reset-InvalidStreak { $script:InvalidStreak = 0 }

function Pause-BeforeClose {
    if (-not [Console]::IsInputRedirected) {
        Write-Host ''
        Write-Host -NoNewline 'Press Enter to close.'
        [void][Console]::In.ReadLine()
    }
}

# ----------------------------------------------------------------------------
# Registry engine (type-exact, wide-char; source strictly read-only)
# ----------------------------------------------------------------------------

$HKCU = [Microsoft.Win32.Registry]::CurrentUser

function Open-ReadKey([string]$path) {
    # The ONLY way this utility opens source keys: read-only.
    return $HKCU.OpenSubKey($path, $false)
}

function Test-RegKey([string]$path) {
    $k = Open-ReadKey $path
    if ($null -ne $k) { $k.Close(); return $true }
    return $false
}

function Get-RegValueSafe([string]$path, [string]$name, $default) {
    $k = Open-ReadKey $path
    if ($null -eq $k) { return $default }
    try {
        $v = $k.GetValue($name, $default, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
        # the leading comma stops PowerShell from unrolling array values
        # (REG_BINARY byte[] / REG_MULTI_SZ string[]) into Object[]
        return ,$v
    } finally { $k.Close() }
}

function Remove-RegSubtree([string]$path) {
    # Deletes an entire destination subtree if it exists (never used on sources).
    $parentPath = Split-Path -Parent $path
    $leaf = Split-Path -Leaf $path
    $parent = $HKCU.OpenSubKey($parentPath, $true)
    if ($null -eq $parent) { return }
    try {
        if ($null -ne $parent.OpenSubKey($leaf, $false)) {
            $parent.DeleteSubKeyTree($leaf, $false)
        }
    } finally { $parent.Close() }
}

function Copy-RegTree {
    # Recursively copies srcPath -> dstPath preserving exact value kinds.
    # valueHook: scriptblock param($relKey,$name,$kind,$value) -> $null to skip
    #            the value, or @($kind,$value) (possibly transformed) to write.
    # Returns the number of values written.
    param(
        [string]$srcPath,
        [string]$dstPath,
        [scriptblock]$valueHook = $null
    )
    $count = 0
    $stack = New-Object System.Collections.Stack
    $stack.Push('')
    while ($stack.Count -gt 0) {
        $rel = $stack.Pop()
        $sp = $srcPath; if ($rel -ne '') { $sp = $srcPath + '\' + $rel }
        $dp = $dstPath; if ($rel -ne '') { $dp = $dstPath + '\' + $rel }
        $src = Open-ReadKey $sp
        if ($null -eq $src) { continue }
        try {
            $dst = $HKCU.CreateSubKey($dp)
            try {
                foreach ($name in $src.GetValueNames()) {
                    $kind = $src.GetValueKind($name)
                    $val = $src.GetValue($name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                    if ($null -ne $valueHook) {
                        $res = & $valueHook $rel $name $kind $val
                        if ($null -eq $res) { continue }
                        $kind = $res[0]; $val = $res[1]
                    }
                    $dst.SetValue($name, $val, $kind)
                    $count++
                }
            } finally { $dst.Close() }
            foreach ($sub in $src.GetSubKeyNames()) {
                if ($rel -eq '') { $stack.Push($sub) } else { $stack.Push($rel + '\' + $sub) }
            }
        } finally { $src.Close() }
    }
    return $count
}

function Copy-RegValues {
    # Copies only the immediate values of srcPath -> dstPath (no subkeys),
    # honoring the same valueHook contract as Copy-RegTree.
    param(
        [string]$srcPath,
        [string]$dstPath,
        [scriptblock]$valueHook = $null
    )
    $count = 0
    $src = Open-ReadKey $srcPath
    if ($null -eq $src) { return 0 }
    try {
        $dst = $HKCU.CreateSubKey($dstPath)
        try {
            foreach ($name in $src.GetValueNames()) {
                $kind = $src.GetValueKind($name)
                $val = $src.GetValue($name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                if ($null -ne $valueHook) {
                    $res = & $valueHook '' $name $kind $val
                    if ($null -eq $res) { continue }
                    $kind = $res[0]; $val = $res[1]
                }
                $dst.SetValue($name, $val, $kind)
                $count++
            }
        } finally { $dst.Close() }
    } finally { $src.Close() }
    return $count
}

function Get-ConsecutiveItems([string]$path) {
    # Salamander list keys use numbered subkeys "1".."n"; loaders stop at the
    # first gap. Returns the names of the consecutive run (possibly empty).
    $items = @()
    $i = 1
    while (Test-RegKey ($path + '\' + $i)) { $items += "$i"; $i++ }
    return ,$items
}

# Registry key last-write time (display only; silently omitted on failure).
$regTimeType = @'
using System;
using System.Runtime.InteropServices;
public static class TcMigRegTime {
    [DllImport("advapi32.dll", CharSet = CharSet.Unicode)]
    public static extern int RegQueryInfoKeyW(IntPtr hKey, IntPtr lpClass,
        IntPtr lpcchClass, IntPtr lpReserved, IntPtr lpcSubKeys, IntPtr lpcbMaxSubKeyLen,
        IntPtr lpcbMaxClassLen, IntPtr lpcValues, IntPtr lpcbMaxValueNameLen,
        IntPtr lpcbMaxValueLen, IntPtr lpcbSecurityDescriptor, out long lpftLastWriteTime);
}
'@
try { Add-Type -TypeDefinition $regTimeType -ErrorAction Stop } catch { }

function Get-KeyLastWrite([string]$path) {
    try {
        $k = Open-ReadKey $path
        if ($null -eq $k) { return $null }
        try {
            $ft = 0L
            $rc = [TcMigRegTime]::RegQueryInfoKeyW($k.Handle.DangerousGetHandle(), [IntPtr]::Zero,
                [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero,
                [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [ref]$ft)
            if ($rc -eq 0 -and $ft -gt 0) { return [DateTime]::FromFileTime($ft) }
        } finally { $k.Close() }
    } catch { }
    return $null
}

# ----------------------------------------------------------------------------
# Source discovery and generation classification (research R2, R3)
# ----------------------------------------------------------------------------

function Get-SourceDisplayName([string]$rootPath) {
    $leaf = Split-Path -Leaf $rootPath
    if ($rootPath -eq 'Software\Salamander') { return 'Salamander (1.52 or older)' }
    if ($leaf -eq 'Servant Salamander') { return 'Servant Salamander (1.6 beta 1-5)' }
    return $leaf
}

function Get-SourceConfigVersion([string]$rootPath) {
    # Mirrors the product's own fallback: no Version key => 1, no value => 2.
    if (-not (Test-RegKey ($rootPath + '\Version'))) { return 1 }
    $v = Get-RegValueSafe ($rootPath + '\Version') 'Configuration' $null
    if ($v -is [int]) { return [int]$v }
    return 2
}

function Get-Generation([int]$cfgVersion) {
    if ($cfgVersion -ge 100) { return 'AS4' }
    if ($cfgVersion -ge 66)  { return 'AS3' }
    if ($cfgVersion -ge 39)  { return 'AS25' }
    return 'Ancient'
}

function Find-Sources {
    $found = @()
    foreach ($root in $SourceRootList) {
        if (-not (Test-RegKey $root)) { continue }
        # Same qualification rule the product applies to its own root:
        # a configuration exists only if the "Configuration" subkey exists.
        if (-not (Test-RegKey ($root + '\Configuration'))) { continue }
        $cfgVer = Get-SourceConfigVersion $root
        $subkeyCount = 0
        $k = Open-ReadKey $root
        if ($null -ne $k) { try { $subkeyCount = $k.SubKeyCount } finally { $k.Close() } }
        $sip = Get-RegValueSafe $root 'Save In Progress' $null
        $found += New-Object PSObject -Property @{
            Path          = $root
            Display       = Get-SourceDisplayName $root
            CfgVersion    = $cfgVer
            Generation    = Get-Generation $cfgVer
            SubkeyCount   = $subkeyCount
            LastWrite     = Get-KeyLastWrite $root
            SaveInProgress = ($null -ne $sip)
        }
    }
    return ,$found
}

# ----------------------------------------------------------------------------
# Category table (contracts/category-mapping.md is the authority)
# ----------------------------------------------------------------------------

# X-CONFIG: Configuration values never copied (case-insensitive).
$XConfigValues = @(
    'Language', 'Use Alternate Language for Plugins', 'Alternate Language for Plugins',
    'Language Changed',
    'Plugins.ver Version (x64)', 'Plugins.ver Version (x86)',
    'Enable Custom Icon Overlays', 'Disabled Custom Icon Overlays',
    'Top ToolBar', 'Middle ToolBar', 'Left ToolBar', 'Right ToolBar',
    'Conversion Table', 'Main window icon index', 'Only One Instance',
    'Last Focused Page', 'Configuration Height',
    'Viewers And Editors Expanded', 'Packers And Unpackers Expanded',
    'Current Tip Index', 'Theme Mode'
)
# Configuration subkeys that belong to the general-config category.
$ConfigIncludedSubkeys = @('Drive Special Settings', 'Copy Move Options', 'Find Options', 'Find Ignore')

# X-VIEWER: Viewer values never copied (geometry / search-session state).
$XViewerValues = @('Left', 'Right', 'Top', 'Bottom', 'Show', 'Save Window Position', 'Find Text', 'HEX-mode')

# PLUGSET: shipped plugins with migratable config: DLL value -> literal
# Configuration Key (both stable across installations; research R6).
$PlugSet = @(
    @{ Dll = 'zip\zip.spl';           Key = 'ZIP';             Name = 'ZIP' },
    @{ Dll = '7zip\7zip.spl';         Key = '7zip';            Name = '7-Zip' },
    @{ Dll = 'checksum\checksum.spl'; Key = 'Checksum';        Name = 'Checksum' },
    @{ Dll = 'dbviewer\dbviewer.spl'; Key = 'DBVIEWER';        Name = 'Database Viewer' },
    @{ Dll = 'diskmap\diskmap.spl';   Key = 'DISKMAP';         Name = 'Disk Map' },
    @{ Dll = 'filecomp\filecomp.spl'; Key = 'File Comparator'; Name = 'File Comparator' },
    @{ Dll = 'peviewer\peviewer.spl'; Key = 'PEVIEWER';        Name = 'PE Viewer' },
    @{ Dll = 'pictview\pictview.spl'; Key = 'PictView';        Name = 'PictView' },
    @{ Dll = 'regedt\regedt.spl';     Key = 'RegEdit';         Name = 'Registry Editor' },
    @{ Dll = 'renamer\renamer.spl';   Key = 'Renamer';         Name = 'Renamer' },
    @{ Dll = 'uncab\uncab.spl';       Key = 'UnCAB';           Name = 'UnCAB' },
    @{ Dll = 'undelete\undelete.spl'; Key = 'UNDELETE';        Name = 'Undelete' },
    @{ Dll = 'uniso\uniso.spl';       Key = 'UnISO';           Name = 'UnISO' }
)
$FtpDll = 'ftp\ftp.spl'
$FtpLiteralKey = 'FTP'

# Known source-side plugin config keys with no Tandem Commander counterpart
# (reported, never copied).
$ForeignPluginKeys = @('Automation', 'CHECKVER', 'Encrypt & Decrypt', 'IEVIEWER', 'MMVIEWER',
    'nethood', 'SplitCombine', 'UnARJ', 'UnMIME', 'UnRAR', 'WMOBILE', 'PAK', 'UnFAT', 'UnLHA')

function Get-PluginKeyMap([string]$rootPath) {
    # Maps lower(DLL value) -> Configuration Key by walking <root>\Plugins\<n>.
    # Numeric subkey order is installation-specific; the DLL value is the join key.
    $map = @{}
    $plugins = Open-ReadKey ($rootPath + '\Plugins')
    if ($null -eq $plugins) { return $map }
    try {
        foreach ($sub in $plugins.GetSubKeyNames()) {
            $k = Open-ReadKey ($rootPath + '\Plugins\' + $sub)
            if ($null -eq $k) { continue }
            try {
                $dll = $k.GetValue('DLL', $null)
                $cfgKey = $k.GetValue('Configuration Key', $null)
                if ($dll -is [string] -and $cfgKey -is [string] -and $cfgKey -ne '') {
                    $map[$dll.ToLowerInvariant()] = $cfgKey
                }
            } finally { $k.Close() }
        }
    } finally { $plugins.Close() }
    return $map
}

function Resolve-PluginCfgKey([string]$rootPath, [hashtable]$map, [string]$dll, [string]$literal) {
    if ($map.ContainsKey($dll.ToLowerInvariant())) { return $map[$dll.ToLowerInvariant()] }
    return $literal
}

# --- per-category presence / item counting -----------------------------------

function Count-HotPaths([string]$src) {
    $n = 0
    for ($i = 0; $i -le 30; $i++) {
        $p = Get-RegValueSafe ($src + '\Hot Paths\' + $i) 'Path' ''
        if ($p -is [string] -and $p -ne '') { $n++ }
    }
    return $n
}

function Count-ViewerRows([string]$src, [int]$cfgVer) {
    # Rows that will be KEPT after filter F1, across the three keys.
    $n = 0
    foreach ($keyName in @('Viewers', 'Alternative Viewers', 'Editors')) {
        $isViewer = ($keyName -ne 'Editors')
        foreach ($item in (Get-ConsecutiveItems ($src + '\' + $keyName))) {
            $itemPath = $src + '\' + $keyName + '\' + $item
            $masks = Get-RegValueSafe $itemPath 'Masks' $null
            if (-not ($masks -is [string])) { continue }
            if ($masks.Contains('|')) { continue }
            if ($isViewer) {
                $type = Get-RegValueSafe $itemPath 'Type' $null
                if (-not ($type -is [int])) { continue }
                if ([int]$type -lt 0) { continue }
            }
            $n++
        }
    }
    return $n
}

function Count-Values([string]$path, [string[]]$excludeNames) {
    $k = Open-ReadKey $path
    if ($null -eq $k) { return 0 }
    try {
        $n = 0
        foreach ($name in $k.GetValueNames()) {
            $skip = $false
            if ($null -ne $excludeNames) {
                foreach ($x in $excludeNames) { if ($name -ieq $x) { $skip = $true; break } }
            }
            if (-not $skip) { $n++ }
        }
        return $n
    } finally { $k.Close() }
}

function Count-Subkeys([string]$path) {
    $k = Open-ReadKey $path
    if ($null -eq $k) { return 0 }
    try { return $k.SubKeyCount } finally { $k.Close() }
}

function Count-DefaultDirs([string]$src) {
    $k = Open-ReadKey ($src + '\Default Directories')
    if ($null -eq $k) { return 0 }
    try {
        $n = 0
        foreach ($name in $k.GetValueNames()) {
            if (Test-DefaultDirValue $k $name) { $n++ }
        }
        return $n
    } finally { $k.Close() }
}

function Test-DefaultDirValue($key, [string]$name) {
    # Filter F2: name must be a single letter A-Z, data a REG_SZ starting with
    # the same letter + ":\" and longer than 3 chars (else Tandem Commander
    # shows modal error boxes at every start).
    if ($name.Length -ne 1) { return $false }
    $c = [char]::ToUpperInvariant($name[0])
    if ($c -lt 'A' -or $c -gt 'Z') { return $false }
    if ($key.GetValueKind($name) -ne [Microsoft.Win32.RegistryValueKind]::String) { return $false }
    $v = $key.GetValue($name, '')
    if (-not ($v -is [string])) { return $false }
    if ($v.Length -le 3) { return $false }
    if ([char]::ToUpperInvariant($v[0]) -ne $c) { return $false }
    if ($v.Substring(1, 2) -ne ':\') { return $false }
    return $true
}

function Count-GeneralConfig([string]$src) {
    $n = Count-Values ($src + '\Configuration') $XConfigValues
    foreach ($sub in $ConfigIncludedSubkeys) {
        $n += (Count-Subkeys ($src + '\Configuration\' + $sub))
        $n += (Count-Values ($src + '\Configuration\' + $sub) $null)
    }
    return $n
}

function Get-Categories([PSObject]$source) {
    # Builds the offered/not-offered category lists for the selected source.
    $src = $source.Path
    $gen = $source.Generation
    $cfgVer = $source.CfgVersion
    $srcPlugMap = Get-PluginKeyMap $src

    $cats = @()

    function New-Cat([string]$id, [string]$name, [int]$count, [string]$countNoun, [string]$skipReason) {
        return New-Object PSObject -Property @{
            Id = $id; Name = $name; Count = $count; CountNoun = $countNoun
            SkipReason = $skipReason   # non-empty => not offered (present but not transferable)
            Selected = $false
        }
    }

    # hotpaths
    if (Test-RegKey ($src + '\Hot Paths')) {
        $cats += New-Cat 'hotpaths' 'Directory hot paths' (Count-HotPaths $src) 'items' ''
    }
    # usermenu
    if (Test-RegKey ($src + '\User Menu')) {
        $cats += New-Cat 'usermenu' 'User menu commands' ((Get-ConsecutiveItems ($src + '\User Menu')).Count) 'items' ''
    }
    # viewers-editors
    $veExists = (Test-RegKey ($src + '\Viewers')) -or (Test-RegKey ($src + '\Alternative Viewers')) -or (Test-RegKey ($src + '\Editors'))
    if ($veExists) {
        $reason = ''
        if ($gen -eq 'Ancient' -and $cfgVer -lt 6) { $reason = 'source configuration too old (pre-1.6b4 viewer/editor format)' }
        $cats += New-Cat 'viewers-editors' 'Viewer & editor associations' (Count-ViewerRows $src $cfgVer) 'entries' $reason
    }
    # confirmations
    if (Test-RegKey ($src + '\Configuration\Confirmation')) {
        $cats += New-Cat 'confirmations' 'Confirmation prompts' (Count-Values ($src + '\Configuration\Confirmation') $null) 'options' ''
    }
    # colors
    $colorsExist = (Test-RegKey ($src + '\Colors')) -or (Test-RegKey ($src + '\Custom Colors'))
    if ($colorsExist) {
        $reason = ''
        if ($gen -eq 'Ancient') { $reason = 'color format conversions predate Altap Salamander 2.5' }
        $cnt = Count-Subkeys ($src + '\Colors\Panel Items Hilighting')
        if ((Count-Values ($src + '\Colors') $null) -gt 0 -or (Count-Values ($src + '\Custom Colors') $null) -gt 0) { $cnt++ }
        $cats += New-Cat 'colors' 'Colors & panel highlighting' $cnt 'entries' $reason
    }
    # viewtemplates
    if (Test-RegKey ($src + '\View Templates')) {
        $reason = ''
        if ($gen -eq 'Ancient') { $reason = 'pre-2.5 view template conversions are not reproduced' }
        $cats += New-Cat 'viewtemplates' 'Panel view templates' (Count-Subkeys ($src + '\View Templates')) 'templates' $reason
    }
    # viewer-settings
    if (Test-RegKey ($src + '\Viewer')) {
        $cats += New-Cat 'viewer-settings' 'Internal viewer settings' (Count-Values ($src + '\Viewer') $XViewerValues) 'options' ''
    }
    # defaultdirs
    if (Test-RegKey ($src + '\Default Directories')) {
        $cats += New-Cat 'defaultdirs' 'Per-drive default directories' (Count-DefaultDirs $src) 'drives' ''
    }
    # general-config
    if (Test-RegKey ($src + '\Configuration')) {
        $cats += New-Cat 'general-config' 'General configuration' (Count-GeneralConfig $src) 'values' ''
    }
    # ftp
    $srcFtpKey = Resolve-PluginCfgKey $src $srcPlugMap $FtpDll $FtpLiteralKey
    if (Test-RegKey ($src + '\Plugins Configuration\' + $srcFtpKey)) {
        $bkCount = (Get-ConsecutiveItems ($src + '\Plugins Configuration\' + $srcFtpKey + '\Bookmarks')).Count
        $cats += New-Cat 'ftp' 'FTP connections (bookmarks, proxies, server types)' $bkCount 'servers' ''
    }
    # plugin-configs
    if (Test-RegKey ($src + '\Plugins Configuration')) {
        $present = 0
        foreach ($p in $PlugSet) {
            $k = Resolve-PluginCfgKey $src $srcPlugMap $p.Dll $p.Key
            if (Test-RegKey ($src + '\Plugins Configuration\' + $k)) { $present++ }
        }
        if ($present -gt 0) {
            $cats += New-Cat 'plugin-configs' 'Other plugin settings' $present 'plugins' ''
        }
    }

    return ,$cats
}

function Get-NotTransferableInfo([PSObject]$source) {
    # Material present in the source that is never offered, with reasons
    # (summary "not offered" block; FR-011 / research R9).
    $src = $source.Path
    $list = @()
    if (Test-RegKey ($src + '\Packers & Unpackers')) {
        $list += 'Archiver settings (packers, unpackers, archive associations): Tandem Commander rebuilds these from defaults (UTF-8 configuration baseline); entries also carry installation-specific plugin references'
    }
    if (Test-RegKey ($src + '\Internal ZIP Packer')) {
        $list += 'Internal ZIP Packer: obsolete settings with no counterpart in Tandem Commander'
    }
    foreach ($k in @('Left Panel', 'Right Panel', 'Window')) {
        if (Test-RegKey ($src + '\' + $k)) {
            $list += ($k + ': window/panel session state, excluded by design')
            break
        }
    }
    $histFound = $false
    $cfgKey = Open-ReadKey ($src + '\Configuration')
    if ($null -ne $cfgKey) {
        try {
            foreach ($sub in $cfgKey.GetSubKeyNames()) {
                if ($sub -like '* History' -or $sub -ieq 'Working Directories') { $histFound = $true; break }
            }
        } finally { $cfgKey.Close() }
    }
    if ($histFound) {
        $list += 'History lists (commands, paths, searches): transient session state, excluded by design'
    }
    # Foreign plugin configs present in the source
    $pc = Open-ReadKey ($src + '\Plugins Configuration')
    if ($null -ne $pc) {
        try {
            $foreign = @()
            foreach ($sub in $pc.GetSubKeyNames()) {
                foreach ($f in $ForeignPluginKeys) {
                    if ($sub -ieq $f) { $foreign += $sub; break }
                }
            }
            if ($foreign.Count -gt 0) {
                $list += ('Settings of plugins Tandem Commander does not ship: ' + ($foreign -join ', '))
            }
        } finally { $pc.Close() }
    }
    return ,$list
}

# ----------------------------------------------------------------------------
# Transfer engine (W7): per-category replace with transforms/filters
# ----------------------------------------------------------------------------

$script:AltapPathNotes = New-Object System.Collections.ArrayList

function Test-AltapPathValue([string]$value) {
    # Heuristic for the "points into the Altap Salamander installation" flag:
    # a path-like string mentioning the predecessor product.
    if (-not ($value -is [string])) { return $false }
    return ($value -match '(?i)[a-z]:\\[^"]*(altap|salamand)')
}

function Note-AltapPaths([string]$dstSubtree, [string]$categoryName, [string[]]$valueNames) {
    # Scans the freshly written destination subtree for values referencing the
    # old installation; adds NOTES entries.
    $stack = New-Object System.Collections.Stack
    $stack.Push($dstSubtree)
    while ($stack.Count -gt 0) {
        $p = $stack.Pop()
        $k = Open-ReadKey $p
        if ($null -eq $k) { continue }
        try {
            foreach ($name in $k.GetValueNames()) {
                $matchName = $false
                foreach ($vn in $valueNames) { if ($name -ieq $vn) { $matchName = $true; break } }
                if (-not $matchName) { continue }
                $v = $k.GetValue($name, '')
                if (($v -is [string]) -and (Test-AltapPathValue $v)) {
                    [void]$script:AltapPathNotes.Add(($categoryName + ': "' + $v + '" references the old Altap Salamander installation and will stop working if it is uninstalled'))
                }
            }
            foreach ($sub in $k.GetSubKeyNames()) { $stack.Push($p + '\' + $sub) }
        } finally { $k.Close() }
    }
}

function Invoke-CategoryTransfer {
    # Returns a result object: Status TRANSFERRED/PARTIAL, Copied, Skipped (list of strings).
    param([PSObject]$cat, [PSObject]$source)

    $src = $source.Path
    $cfgVer = $source.CfgVersion
    $skipped = New-Object System.Collections.ArrayList
    $copied = 0

    switch ($cat.Id) {

        'hotpaths' {
            Remove-RegSubtree ($DestRoot + '\Hot Paths')
            $needDollar = ($cfgVer -lt 47)
            $hook = {
                param($rel, $name, $kind, $val)
                if ($needDollar -and ($name -ieq 'Path') -and ($val -is [string])) {
                    return @($kind, $val.Replace('$', '$$'))
                }
                return @($kind, $val)
            }.GetNewClosure()
            [void](Copy-RegTree ($src + '\Hot Paths') ($DestRoot + '\Hot Paths') $hook)
            $copied = Count-HotPaths $DestRoot
        }

        'usermenu' {
            Remove-RegSubtree ($DestRoot + '\User Menu')
            $items = Get-ConsecutiveItems ($src + '\User Menu')
            foreach ($item in $items) {
                [void](Copy-RegTree ($src + '\User Menu\' + $item) ($DestRoot + '\User Menu\' + $item))
                $copied++
            }
            # numeric subkeys beyond the first gap are invisible to the product
            $umKey = Open-ReadKey ($src + '\User Menu')
            if ($null -ne $umKey) {
                try {
                    foreach ($sub in $umKey.GetSubKeyNames()) {
                        $n = 0
                        if ([int]::TryParse($sub, [ref]$n)) {
                            if ($n -gt $items.Count) {
                                [void]$skipped.Add("entry $sub : unreachable (gap in the source's numbering)")
                            }
                        }
                    }
                } finally { $umKey.Close() }
            }
            Note-AltapPaths ($DestRoot + '\User Menu') 'User menu' @('Command', 'Icon', 'Initial Directory')
        }

        'viewers-editors' {
            $lowercase = ($cfgVer -lt 44)
            foreach ($keyName in @('Viewers', 'Alternative Viewers', 'Editors')) {
                Remove-RegSubtree ($DestRoot + '\' + $keyName)
                if (-not (Test-RegKey ($src + '\' + $keyName))) { continue }
                $isViewer = ($keyName -ne 'Editors')
                $outIdx = 0
                foreach ($item in (Get-ConsecutiveItems ($src + '\' + $keyName))) {
                    $itemPath = $src + '\' + $keyName + '\' + $item
                    $masks = Get-RegValueSafe $itemPath 'Masks' $null
                    if (-not ($masks -is [string])) {
                        [void]$skipped.Add("$keyName entry $item : no file masks stored")
                        continue
                    }
                    if ($masks.Contains('|')) {
                        [void]$skipped.Add("$keyName entry $item ($masks): masks containing '|' are not supported")
                        continue
                    }
                    if ($isViewer) {
                        $type = Get-RegValueSafe $itemPath 'Type' $null
                        if (-not ($type -is [int])) {
                            [void]$skipped.Add("$keyName entry $item ($masks): incomplete entry (no viewer type)")
                            continue
                        }
                        if ([int]$type -lt 0) {
                            [void]$skipped.Add("$keyName entry $item ($masks): references a plugin viewer by position; plugin sets differ between the products")
                            continue
                        }
                    }
                    $outIdx++
                    $hook = {
                        param($rel, $name, $kind, $val)
                        if ($lowercase -and ($name -ieq 'Masks') -and ($val -is [string])) {
                            return @($kind, $val.ToLowerInvariant())
                        }
                        return @($kind, $val)
                    }.GetNewClosure()
                    [void](Copy-RegTree $itemPath ($DestRoot + '\' + $keyName + '\' + $outIdx) $hook)
                    $copied++
                }
                Note-AltapPaths ($DestRoot + '\' + $keyName) $keyName @('Command', 'Initial Directory')
            }
        }

        'confirmations' {
            Remove-RegSubtree ($DestRoot + '\Configuration\Confirmation')
            $copied = Copy-RegValues ($src + '\Configuration\Confirmation') ($DestRoot + '\Configuration\Confirmation')
        }

        'colors' {
            Remove-RegSubtree ($DestRoot + '\Colors')
            Remove-RegSubtree ($DestRoot + '\Custom Colors')
            [void](Copy-RegTree ($src + '\Colors') ($DestRoot + '\Colors'))
            [void](Copy-RegTree ($src + '\Custom Colors') ($DestRoot + '\Custom Colors'))
            # report with the same item rule the checklist used
            $copied = Count-Subkeys ($DestRoot + '\Colors\Panel Items Hilighting')
            if ((Count-Values ($DestRoot + '\Colors') $null) -gt 0 -or (Count-Values ($DestRoot + '\Custom Colors') $null) -gt 0) { $copied++ }
        }

        'viewtemplates' {
            Remove-RegSubtree ($DestRoot + '\View Templates')
            [void](Copy-RegTree ($src + '\View Templates') ($DestRoot + '\View Templates'))
            $copied = Count-Subkeys ($DestRoot + '\View Templates')
        }

        'viewer-settings' {
            Remove-RegSubtree ($DestRoot + '\Viewer')
            $hook = {
                param($rel, $name, $kind, $val)
                foreach ($x in $XViewerValues) { if ($name -ieq $x) { return $null } }
                return @($kind, $val)
            }
            $copied = Copy-RegValues ($src + '\Viewer') ($DestRoot + '\Viewer') $hook
        }

        'defaultdirs' {
            Remove-RegSubtree ($DestRoot + '\Default Directories')
            $srcKey = Open-ReadKey ($src + '\Default Directories')
            if ($null -ne $srcKey) {
                try {
                    $dst = $HKCU.CreateSubKey($DestRoot + '\Default Directories')
                    try {
                        foreach ($name in $srcKey.GetValueNames()) {
                            if (Test-DefaultDirValue $srcKey $name) {
                                $dst.SetValue($name, $srcKey.GetValue($name, ''), [Microsoft.Win32.RegistryValueKind]::String)
                                $copied++
                            } else {
                                [void]$skipped.Add("drive value '$name': not a valid per-drive directory entry")
                            }
                        }
                    } finally { $dst.Close() }
                } finally { $srcKey.Close() }
            }
        }

        'general-config' {
            # Delete owned scope: all Configuration values except the TC-only
            # 'Theme Mode', plus the four included subkeys. Histories, the
            # Confirmation subkey (own category) and anything else stay.
            $dstCfg = $HKCU.CreateSubKey($DestRoot + '\Configuration')
            try {
                foreach ($name in @($dstCfg.GetValueNames())) {
                    if ($name -ieq 'Theme Mode') { continue }
                    $dstCfg.DeleteValue($name, $false)
                }
            } finally { $dstCfg.Close() }
            foreach ($sub in $ConfigIncludedSubkeys) {
                Remove-RegSubtree ($DestRoot + '\Configuration\' + $sub)
            }
            $hook = {
                param($rel, $name, $kind, $val)
                foreach ($x in $XConfigValues) { if ($name -ieq $x) { return $null } }
                return @($kind, $val)
            }
            [void](Copy-RegValues ($src + '\Configuration') ($DestRoot + '\Configuration') $hook)
            foreach ($sub in $ConfigIncludedSubkeys) {
                if (Test-RegKey ($src + '\Configuration\' + $sub)) {
                    [void](Copy-RegTree ($src + '\Configuration\' + $sub) ($DestRoot + '\Configuration\' + $sub))
                }
            }
            # report with the same item rule the checklist used
            $copied = Count-GeneralConfig $DestRoot
        }

        'ftp' {
            $srcMap = Get-PluginKeyMap $src
            $dstMap = Get-PluginKeyMap $DestRoot
            $srcKeyName = Resolve-PluginCfgKey $src $srcMap $FtpDll $FtpLiteralKey
            $dstKeyName = Resolve-PluginCfgKey $DestRoot $dstMap $FtpDll $FtpLiteralKey
            $srcTree = $src + '\Plugins Configuration\' + $srcKeyName
            $dstTree = $DestRoot + '\Plugins Configuration\' + $dstKeyName

            $destUsesMP = ((Get-RegValueSafe ($DestRoot + '\Password Manager') 'Use Master Password' 0) -eq 1)
            $srcUsesMP = ((Get-RegValueSafe ($src + '\Password Manager') 'Use Master Password' 0) -eq 1)

            Remove-RegSubtree $dstTree

            $strippedKeys = New-Object System.Collections.ArrayList
            $hook = $null
            if ($destUsesMP) {
                $hook = {
                    param($rel, $name, $kind, $val)
                    if ($name -ieq 'PasswordE') {
                        [void]$strippedKeys.Add($rel)
                        return $null
                    }
                    return @($kind, $val)
                }.GetNewClosure()
            }
            $copied = Copy-RegTree $srcTree $dstTree $hook

            if ($destUsesMP -and $strippedKeys.Count -gt 0) {
                foreach ($rel in $strippedKeys) {
                    $k = $HKCU.OpenSubKey($dstTree + '\' + $rel, $true)
                    if ($null -ne $k) {
                        try {
                            $k.DeleteValue('Save Password', $false)
                            $nameVal = $k.GetValue('Name', $rel)
                            [void]$skipped.Add("'" + $nameVal + "': stored password not transferred - Tandem Commander uses a different master password; re-enter it there")
                        } finally { $k.Close() }
                    }
                }
                [void]$script:AltapPathNotes.Add('Some FTP passwords were protected by the Altap Salamander master password; because Tandem Commander already uses its own master password, those passwords were not transferred (identical master passwords cannot be verified by this utility)')
            }
            if ($srcUsesMP -and -not $destUsesMP) {
                $verifier = Get-RegValueSafe ($src + '\Password Manager') 'Master Password Verifier' $null
                if ($verifier -is [byte[]]) {
                    $pm = $HKCU.CreateSubKey($DestRoot + '\Password Manager')
                    try {
                        $pm.SetValue('Use Master Password', 1, [Microsoft.Win32.RegistryValueKind]::DWord)
                        $pm.SetValue('Master Password Verifier', $verifier, [Microsoft.Win32.RegistryValueKind]::Binary)
                    } finally { $pm.Close() }
                    [void]$script:AltapPathNotes.Add('Your Altap Salamander master password now also protects the passwords stored in Tandem Commander - use the same master password there')
                } else {
                    [void]$skipped.Add('master password settings: verifier record missing in the source; encrypted passwords will ask to be re-entered')
                }
            }
            # count = bookmarks landed
            $copied = (Get-ConsecutiveItems ($dstTree + '\Bookmarks')).Count
        }

        'plugin-configs' {
            $srcMap = Get-PluginKeyMap $src
            $dstMap = Get-PluginKeyMap $DestRoot
            foreach ($p in $PlugSet) {
                $srcKeyName = Resolve-PluginCfgKey $src $srcMap $p.Dll $p.Key
                if (-not (Test-RegKey ($src + '\Plugins Configuration\' + $srcKeyName))) { continue }
                $dstKeyName = Resolve-PluginCfgKey $DestRoot $dstMap $p.Dll $p.Key
                $dstTree = $DestRoot + '\Plugins Configuration\' + $dstKeyName
                Remove-RegSubtree $dstTree
                [void](Copy-RegTree ($src + '\Plugins Configuration\' + $srcKeyName) $dstTree)
                $copied++
                if ($p.Key -eq 'UNDELETE') {
                    $tmp = Get-RegValueSafe $dstTree 'Temp Path' ''
                    if (($tmp -is [string]) -and $tmp -ne '') {
                        [void]$script:AltapPathNotes.Add('Undelete plugin: verify its temporary path "' + $tmp + '" still exists on this machine')
                    }
                }
            }
        }

        default { throw "Unknown category id '$($cat.Id)'" }
    }

    $status = 'TRANSFERRED'
    if ($skipped.Count -gt 0) { $status = 'PARTIAL' }
    return New-Object PSObject -Property @{
        Category = $cat; Status = $status; Copied = $copied; Skipped = $skipped
    }
}

# ----------------------------------------------------------------------------
# Destination integrity + backup (T007 / research R4, R10)
# ----------------------------------------------------------------------------

function Ensure-DestinationIntegrity {
    # Never copied from any source: Version key content, root marker values.
    $root = $HKCU.CreateSubKey($DestRoot)
    $root.Close()
    $verKey = $HKCU.CreateSubKey($DestRoot + '\Version')
    try {
        $cur = $verKey.GetValue('Configuration', $null)
        if ($null -eq $cur) {
            $verKey.SetValue('Configuration', $TC_CONFIG_VERSION, [Microsoft.Win32.RegistryValueKind]::DWord)
        }
        # never lower an existing value
    } finally { $verKey.Close() }
    # Without a Configuration subkey Tandem Commander ignores the whole root
    # and overwrites it with defaults on first start.
    $cfgKey = $HKCU.CreateSubKey($DestRoot + '\Configuration')
    $cfgKey.Close()
}

function New-Backup([string]$outDir, [string]$stamp) {
    # Returns @{ PreExisted; RegFile; RestoreFile } or aborts with exit 12.
    $preExisted = Test-RegKey $DestRoot
    $regFile = Join-Path $outDir ("tc-settings-backup-$stamp.reg")
    $restoreFile = Join-Path $outDir ("tc-settings-restore-$stamp.cmd")

    if ($preExisted) {
        # cmd-level redirection: reg.exe stderr must not surface into
        # PowerShell 5.1 (it would become a terminating error record)
        cmd /c ('reg export "HKCU\' + $DestRoot + '" "' + $regFile + '" /y >nul 2>&1')
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $regFile)) {
            Write-Host 'ERROR: could not create the backup of current Tandem Commander settings.'
            Write-Host 'Nothing was changed.'
            Pause-BeforeClose
            exit $EXIT_DESTFAIL
        }
    }

    $lines = @()
    $lines += '@echo off'
    $lines += 'echo This restores the Tandem Commander settings saved by the migration'
    $lines += ('echo utility on ' + $stamp + ' and removes everything written after that.')
    $lines += 'echo('
    $lines += 'echo Close Tandem Commander before continuing.'
    $lines += 'pause'
    $lines += ('reg delete "HKCU\' + $DestRoot + '" /f >nul 2>&1')
    if ($preExisted) {
        $lines += ('reg import "' + $regFile + '"')
    } else {
        $lines += 'echo (No settings existed before the migration - the settings key was removed.)'
    }
    $lines += 'echo Done.'
    $lines += 'pause'
    [IO.File]::WriteAllLines($restoreFile, $lines, [Text.Encoding]::ASCII)

    return @{ PreExisted = $preExisted; RegFile = $regFile; RestoreFile = $restoreFile }
}

# ----------------------------------------------------------------------------
# Wizard screens
# ----------------------------------------------------------------------------

function Show-W1 {
    Write-Host '============================================================================'
    Write-Host ' Tandem Commander - settings migration from Altap Salamander'
    Write-Host ' (one-time utility, feature 057)'
    Write-Host '============================================================================'
    Write-Host ''
    Write-Host 'This tool copies settings you select from an existing Altap (or Servant)'
    Write-Host 'Salamander configuration into Tandem Commander.'
    Write-Host ''
    Write-Host ' * Your Altap Salamander settings are opened READ-ONLY and never changed.'
    Write-Host ' * Before anything is written, your current Tandem Commander settings are'
    Write-Host '   backed up, and you get a restore script to undo the whole migration.'
    Write-Host ''
}

function Test-ProcessesRunning {
    if ($SkipProcCheck) { return }
    $tc = Get-Process -Name 'tandemcommander' -ErrorAction SilentlyContinue
    if ($tc) {
        Write-Host 'Close Tandem Commander first - it saves its settings on exit and would'
        Write-Host 'overwrite the transferred data. Then run this utility again.'
        Pause-BeforeClose
        exit $EXIT_RUNNING
    }
    $as = Get-Process -Name 'salamand' -ErrorAction SilentlyContinue
    if ($as) {
        Write-Host 'Close Altap Salamander first so its configuration is stable while it'
        Write-Host 'is read. Then run this utility again.'
        Pause-BeforeClose
        exit $EXIT_RUNNING
    }
}

function Test-W2-Environment {
    Test-ProcessesRunning
    # Destination writability probe WITHOUT creating anything (no registry
    # write may happen before the W6 confirmation): open the deepest existing
    # ancestor of the destination root with write access.
    try {
        $probe = $DestRoot
        while ($probe -ne '' -and -not (Test-RegKey $probe)) {
            $parent = Split-Path -Parent $probe
            if ($parent -eq $probe) { break }
            $probe = $parent
        }
        if ($probe -eq '') { $probe = 'Software' }
        $k = $HKCU.OpenSubKey($probe, $true)
        if ($null -eq $k) { throw 'access denied' }
        $k.Close()
    } catch {
        Write-Host 'Cannot write Tandem Commander settings (registry access denied).'
        Pause-BeforeClose
        exit $EXIT_DESTFAIL
    }
}

function Select-W3-Source([array]$sources) {
    Write-Host 'Found Altap Salamander configurations:'
    for ($i = 0; $i -lt $sources.Count; $i++) {
        $s = $sources[$i]
        $extra = "($($s.SubkeyCount) subkeys"
        if ($null -ne $s.LastWrite) { $extra += ', last written ' + $s.LastWrite.ToString('yyyy-MM-dd') }
        $extra += ')'
        Write-Host ("  [{0}] {1,-34} {2}" -f ($i + 1), $s.Display, $extra)
    }
    Write-Host ''
    Reset-InvalidStreak
    while ($true) {
        $ans = Read-Answer "Select source [1]:" '1'
        $idx = 0
        if ([int]::TryParse($ans, [ref]$idx)) {
            if ($idx -ge 1 -and $idx -le $sources.Count) {
                $sel = $sources[$idx - 1]
                if ($sel.SaveInProgress) {
                    Write-Host ''
                    Write-Host 'WARNING: this configuration carries an interrupted-save marker; Altap'
                    Write-Host 'Salamander may not have finished writing it last time. It will be read'
                    Write-Host 'as-is (and never modified).'
                    Write-Host ''
                }
                return $sel
            }
        }
        Register-InvalidInput "enter a number between 1 and $($sources.Count)"
    }
}

function Select-W4-Categories([array]$cats, [array]$notTransferable) {
    # Returns the selected (offered) categories; exits 3 on empty selection.
    $offered = @($cats | Where-Object { $_.SkipReason -eq '' -and $_.Count -gt 0 })
    $empty   = @($cats | Where-Object { $_.SkipReason -eq '' -and $_.Count -eq 0 })
    $blocked = @($cats | Where-Object { $_.SkipReason -ne '' })

    foreach ($c in $offered) { $c.Selected = $true }

    Write-Host ''
    Write-Host 'Transferable settings found (all selected by default):'
    Reset-InvalidStreak
    while ($true) {
        for ($i = 0; $i -lt $offered.Count; $i++) {
            $c = $offered[$i]
            $mark = ' '
            if ($c.Selected) { $mark = 'X' }
            Write-Host ("  [{0}] {1}. {2,-42} ({3} {4})" -f $mark, ($i + 1), $c.Name, $c.Count, $c.CountNoun)
        }
        foreach ($c in $empty) {
            Write-Host ("  [-] -. {0,-42} (empty - nothing to transfer)" -f $c.Name)
        }
        if ($blocked.Count -gt 0 -or $notTransferable.Count -gt 0) {
            Write-Host ''
            Write-Host 'Not transferable from this source:'
            foreach ($c in $blocked) { Write-Host ("   - {0}: {1}" -f $c.Name, $c.SkipReason) }
            foreach ($n in $notTransferable) { Write-Host ("   - " + $n) }
        }
        Write-Host ''
        $ans = Read-Answer 'Toggle number, A=all, N=none, D=done [D]:' 'D'
        $upper = $ans.ToUpperInvariant()
        if ($upper -eq 'D') {
            $selected = @($offered | Where-Object { $_.Selected })
            if ($selected.Count -eq 0) {
                Write-Host 'Nothing selected. Nothing was changed.'
                Pause-BeforeClose
                exit $EXIT_CANCEL
            }
            return ,$selected
        }
        if ($upper -eq 'A') { foreach ($c in $offered) { $c.Selected = $true }; Write-Host ''; continue }
        if ($upper -eq 'N') { foreach ($c in $offered) { $c.Selected = $false }; Write-Host ''; continue }
        $idx = 0
        if ([int]::TryParse($ans, [ref]$idx) -and $idx -ge 1 -and $idx -le $offered.Count) {
            $offered[$idx - 1].Selected = -not $offered[$idx - 1].Selected
            Write-Host ''
            continue
        }
        Register-InvalidInput "enter a category number, A, N or D"
    }
}

function Show-W5-Backup([hashtable]$paths) {
    Write-Host ''
    Write-Host 'Before writing, a complete backup of current Tandem Commander settings'
    Write-Host 'will be saved:'
    Write-Host ('  Backup : ' + $paths.RegFile)
    Write-Host ('  Restore: ' + $paths.RestoreFile + '   (double-click to undo)')
    Write-Host ('  Summary: ' + $paths.SummaryFile)
    Write-Host ''
}

function Confirm-W6([array]$selected, [PSObject]$source) {
    Reset-InvalidStreak
    $prompt = ('About to transfer ' + $selected.Count + ' categories from "' + $source.Display +
               '" into "Tandem Commander 0.1". Selected categories will be REPLACED in' +
               [Environment]::NewLine + 'Tandem Commander. Continue? [y/N]:')
    $ans = Read-Answer $prompt 'N'
    if ($ans -match '^(?i)y(es)?$') { return }
    Write-Host 'Cancelled. Nothing was changed.'
    Pause-BeforeClose
    exit $EXIT_CANCEL
}

# ----------------------------------------------------------------------------
# Summary (W8)
# ----------------------------------------------------------------------------

function Build-Summary {
    param([array]$results, [array]$blocked, [array]$notTransferable, [hashtable]$backup, [string]$summaryFile, [PSObject]$source, [string]$failNote)
    $L = New-Object System.Collections.ArrayList
    [void]$L.Add('============================================================================')
    [void]$L.Add(' Migration summary - ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
    [void]$L.Add(' Source     : ' + $source.Display + '  (HKCU\' + $source.Path + ')')
    [void]$L.Add(' Destination: Tandem Commander 0.1  (HKCU\' + $DestRoot + ')')
    [void]$L.Add('============================================================================')
    [void]$L.Add('')
    foreach ($r in $results) {
        $line = ' ' + $r.Status + '  ' + $r.Category.Name
        if ($r.Status -ne 'FAILED') { $line += ' (' + $r.Copied + ' ' + $r.Category.CountNoun + ')' }
        [void]$L.Add($line)
        foreach ($s in $r.Skipped) { [void]$L.Add('     skipped: ' + $s) }
        if ($r.Status -eq 'FAILED') { [void]$L.Add('     error: ' + $r.Error) }
    }
    if ($failNote) { [void]$L.Add(''); [void]$L.Add(' ' + $failNote) }
    if ($blocked.Count -gt 0 -or $notTransferable.Count -gt 0) {
        [void]$L.Add('')
        [void]$L.Add(' Not transferable from this source:')
        foreach ($c in $blocked) { [void]$L.Add('   - ' + $c.Name + ': ' + $c.SkipReason) }
        foreach ($n in $notTransferable) { [void]$L.Add('   - ' + $n) }
    }
    if ($script:AltapPathNotes.Count -gt 0) {
        [void]$L.Add('')
        [void]$L.Add(' NOTES:')
        foreach ($n in $script:AltapPathNotes) { [void]$L.Add('   - ' + $n) }
    }
    [void]$L.Add('')
    [void]$L.Add(' Backup : ' + $backup.RegFile)
    if (-not $backup.PreExisted) { [void]$L.Add('          (no Tandem Commander settings existed before this run)') }
    [void]$L.Add(' To undo this migration, run: ' + $backup.RestoreFile)
    [void]$L.Add('')
    try { [IO.File]::WriteAllLines($summaryFile, [string[]]$L, [Text.Encoding]::UTF8) } catch { }
    foreach ($line in $L) { Write-Host $line }
}

# ----------------------------------------------------------------------------
# Main flow
# ----------------------------------------------------------------------------

Show-W1
Test-W2-Environment

$sources = Find-Sources
if ($sources.Count -eq 0) {
    Write-Host 'No Altap Salamander configuration was found for this Windows user.'
    Write-Host 'Nothing to migrate.'
    Pause-BeforeClose
    exit $EXIT_NOSOURCE
}

$source = Select-W3-Source $sources
$cats = Get-Categories $source
$notTransferable = Get-NotTransferableInfo $source
$blocked = @($cats | Where-Object { $_.SkipReason -ne '' })

$selected = Select-W4-Categories $cats $notTransferable

$outDir = Resolve-OutDir
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
# never overwrite a previous run's backup set (re-runs within one second)
$suffix = 0
while (Test-Path -LiteralPath (Join-Path $outDir ("tc-settings-backup-$stamp.reg"))) {
    $suffix++
    $stamp = (Get-Date -Format 'yyyyMMdd-HHmmss') + "-$suffix"
}
$summaryFile = Join-Path $outDir ("tc-migration-summary-$stamp.txt")
$plannedPaths = @{
    RegFile = (Join-Path $outDir ("tc-settings-backup-$stamp.reg"))
    RestoreFile = (Join-Path $outDir ("tc-settings-restore-$stamp.cmd"))
    SummaryFile = $summaryFile
}
Show-W5-Backup $plannedPaths
Confirm-W6 $selected $source

# Re-check the running processes immediately before the first write.
Test-ProcessesRunning

$backup = New-Backup $outDir $stamp
Ensure-DestinationIntegrity

$results = @()
$failNote = $null
$failed = $false
Write-Host ''
foreach ($cat in $selected) {
    Write-Host -NoNewline ('Transferring: ' + $cat.Name + ' ... ')
    try {
        $r = Invoke-CategoryTransfer $cat $source
        $results += $r
        if ($r.Status -eq 'TRANSFERRED') {
            Write-Host ('done (' + $r.Copied + ' ' + $cat.CountNoun + ')')
        } else {
            Write-Host ('partial (' + $r.Copied + ' ' + $cat.CountNoun + ', see summary)')
        }
    } catch {
        Write-Host 'FAILED'
        $results += New-Object PSObject -Property @{
            Category = $cat; Status = 'FAILED'; Copied = 0
            Skipped = @(); Error = $_.Exception.Message
        }
        $failNote = 'The transfer did not finish. Your previous settings are safe in the backup - to return to them, run the restore script below.'
        $failed = $true
        break
    }
}

Write-Host ''
Build-Summary $results $blocked $notTransferable $backup $summaryFile $source $failNote
Pause-BeforeClose
if ($failed) { exit $EXIT_MIDFAIL }
exit $EXIT_OK

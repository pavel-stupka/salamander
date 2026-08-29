<#
.SYNOPSIS
    Generates - and optionally submits - the winget manifests for a released
    version of Tandem Commander.

.DESCRIPTION
    Renders the three manifest templates in tools\winget\templates into
    tools\winget\manifests\<version>\, filling in the version, the release date,
    the SHA256 of the published installer and a one-paragraph release summary
    taken from CHANGELOG.md.

    The installer is fetched from its GitHub Releases URL (or taken from disk
    with -LocalFile) and its Authenticode signature is verified against the
    thumbprint in tools\codesign\codesign.cfg before anything is generated, so
    an unsigned or foreign binary can never be published to the catalogue.

    With -Submit the manifests are sent to microsoft/winget-pkgs as a pull
    request through wingetcreate (downloaded on demand when not on PATH).

    The manifests offer both install scopes. That depends on one directive in
    setup\tandemcommander.iss - PrivilegesRequiredOverridesAllowed, which Inno
    Setup treats as enabling the /ALLUSERS and /CURRENTUSER command line
    parameters that winget uses to pick the scope. Its presence is asserted
    below, so removing it fails the run loudly instead of shipping manifests
    whose silent installs would break.

    Contract: specs\072-winget-distribution\contracts\winget-manifest.md
    Windows PowerShell 5.1 compatible. ASCII only.

.PARAMETER Version
    Version to publish, e.g. 0.1.5. Default: MyAppVersion from
    setup\tandemcommander.iss.

.PARAMETER ReleaseDate
    Release date as YYYY-MM-DD. Default: the date on this version's heading in
    CHANGELOG.md.

.PARAMETER LocalFile
    Hash this local installer instead of downloading the release asset. For a
    dry run before the release is uploaded - the hash must match the file that
    is uploaded later, so never submit from a local build that will be rebuilt.

.PARAMETER Submit
    Create the pull request in microsoft/winget-pkgs. Without it the script
    only generates and validates.

.PARAMETER Token
    GitHub token used by wingetcreate. Default: the WINGET_PAT environment
    variable. A classic PAT with the public_repo scope; the account owning it
    must have a fork of microsoft/winget-pkgs.

.PARAMETER SkipSignatureCheck
    Skip the Authenticode verification. For testing an unsigned local build
    only - never for a real submission.

.EXAMPLE
    publish.ps1 -Version 0.1.5
    Generates and validates the manifests for 0.1.5. Submits nothing.

.EXAMPLE
    publish.ps1 -Version 0.1.6 -Submit
    Generates, validates and opens the pull request (needs WINGET_PAT).
#>

[CmdletBinding()]
param(
    [string]$Version,
    [string]$ReleaseDate,
    [string]$LocalFile,
    [switch]$Submit,
    [string]$Token,
    [switch]$SkipSignatureCheck
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

#------------------------------------------------------------------
# Constants
#------------------------------------------------------------------
$PackageIdentifier = 'PavelStupka.TandemCommander'
$WingetCreateUrl   = 'https://aka.ms/wingetcreate/latest'
$SummaryMaxChars   = 900
$SummaryWrapColumn = 76

function Fail([string]$Message) { Write-Host "ERROR: $Message"; exit 1 }
function Warn([string]$Message) { Write-Host "WARNING: $Message" }

#------------------------------------------------------------------
# Locate the repository
#------------------------------------------------------------------
$ScriptDir     = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot      = Split-Path -Parent (Split-Path -Parent $ScriptDir)
$TemplateDir   = Join-Path $ScriptDir 'templates'
$IssPath       = Join-Path $RepoRoot 'setup\tandemcommander.iss'
$ChangelogPath = Join-Path $RepoRoot 'CHANGELOG.md'
$CodesignCfg   = Join-Path $RepoRoot 'tools\codesign\codesign.cfg'

foreach ($p in @($TemplateDir, $IssPath, $ChangelogPath)) {
    if (-not (Test-Path -LiteralPath $p)) { Fail "not found: $p (is this the Tandem Commander repository?)" }
}

#------------------------------------------------------------------
# Version: parameter, else MyAppVersion from the installer script
#------------------------------------------------------------------
if (-not $Version) {
    $m = Select-String -LiteralPath $IssPath -Pattern '^\s*#define\s+MyAppVersion\s+"([^"]+)"' |
         Select-Object -First 1
    if (-not $m) { Fail "could not read MyAppVersion from $IssPath - pass -Version" }
    $Version = $m.Matches[0].Groups[1].Value
}
if ($Version -notmatch '^\d+\.\d+\.\d+$') { Fail "version must look like 0.1.5, got '$Version'" }

# The manifests advertise both install scopes, which works only because the
# installer permits the /ALLUSERS and /CURRENTUSER command line parameters.
# Inno Setup enables them for either override mode, so any value will do - but
# the directive must be there. Without it winget's silent install breaks in a
# way nothing else here would catch.
if (-not (Select-String -LiteralPath $IssPath -Pattern '^\s*PrivilegesRequiredOverridesAllowed\s*=\s*\S' -Quiet)) {
    Fail "setup\tandemcommander.iss has no PrivilegesRequiredOverridesAllowed - the manifests offer a per-user install the installer would refuse (contracts/winget-manifest.md section 3)"
}

#------------------------------------------------------------------
# Release date and release summary: from CHANGELOG.md
#------------------------------------------------------------------
$changelog = Get-Content -LiteralPath $ChangelogPath -Encoding UTF8
$headingPattern = '^##\s+\[' + [regex]::Escape($Version) + '\]\D*(\d{4}-\d{2}-\d{2})'
$headingIndex = -1
for ($i = 0; $i -lt $changelog.Count; $i++) {
    if ($changelog[$i] -match $headingPattern) {
        $headingIndex = $i
        if (-not $ReleaseDate) { $ReleaseDate = $Matches[1] }
        break
    }
}
if (-not $ReleaseDate) {
    $ReleaseDate = (Get-Date).ToString('yyyy-MM-dd')
    Warn "no CHANGELOG.md heading for $Version - using today's date $ReleaseDate"
}
if ($ReleaseDate -notmatch '^\d{4}-\d{2}-\d{2}$') { Fail "release date must be YYYY-MM-DD, got '$ReleaseDate'" }

# Lead paragraph of the version's changelog section, flattened to plain text.
function Get-ReleaseSummary([string[]]$Lines, [int]$Start) {
    if ($Start -lt 0) { return $null }
    $collected = New-Object System.Collections.Generic.List[string]
    for ($i = $Start + 1; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]
        if ($line -match '^#{1,6}\s') { break }
        if ($line.Trim().Length -eq 0) { if ($collected.Count -gt 0) { break } else { continue } }
        $collected.Add($line.Trim())
    }
    if ($collected.Count -eq 0) { return $null }

    $text = [string]::Join(' ', $collected)
    $text = $text -replace '\[([^\]]+)\]\([^)]*\)', '$1'   # markdown link -> its text
    $text = $text -replace '\*\*', ''                       # bold
    $text = $text -replace '`', ''                          # inline code
    # Typographic characters -> ASCII, so the manifest stays plain text.
    $text = $text -replace ([char]0x2014), '-' -replace ([char]0x2013), '-'
    $text = $text -replace ([char]0x201C), '"' -replace ([char]0x201D), '"'
    $text = $text -replace ([char]0x2018), "'" -replace ([char]0x2019), "'"
    $text = ($text -replace '\s+', ' ').Trim()

    if ($text.Length -gt $script:SummaryMaxChars) {
        $cut = $text.Substring(0, $script:SummaryMaxChars)
        $space = $cut.LastIndexOf(' ')
        if ($space -gt 0) { $cut = $cut.Substring(0, $space) }
        $text = $cut.TrimEnd(' ', ',', ';', '-') + '...'
    }
    return $text
}

# Wrap into a two-space indented YAML block scalar.
function Format-BlockScalar([string]$Text, [int]$Column) {
    $out = New-Object System.Collections.Generic.List[string]
    $line = ''
    foreach ($word in ($Text -split ' ')) {
        if ($line.Length -eq 0) { $line = $word }
        elseif (($line.Length + 1 + $word.Length) -le $Column) { $line = "$line $word" }
        else { $out.Add('  ' + $line); $line = $word }
    }
    if ($line.Length -gt 0) { $out.Add('  ' + $line) }
    return [string]::Join("`n", $out)
}

$summaryText = Get-ReleaseSummary -Lines $changelog -Start $headingIndex
if (-not $summaryText) {
    $summaryText = "Tandem Commander $Version. See the release notes for the full list of changes."
    Warn "no lead paragraph found in CHANGELOG.md for $Version - using a generic release summary"
}
$ReleaseSummary = Format-BlockScalar -Text $summaryText -Column $SummaryWrapColumn

#------------------------------------------------------------------
# Rendering
#------------------------------------------------------------------
function Render-Template([string]$Name, [hashtable]$Values) {
    $path = Join-Path $script:TemplateDir $Name
    if (-not (Test-Path -LiteralPath $path)) { Fail "template not found: $path" }
    $text = [System.IO.File]::ReadAllText($path)

    # 1. Substitute the placeholders. RELEASE_SUMMARY owns its whole line, so
    #    the indented block scalar it expands to keeps the surrounding layout.
    foreach ($key in $Values.Keys) { $text = $text.Replace('{{' + $key + '}}', [string]$Values[$key]) }
    if ($text -match '\{\{([A-Z_0-9]+)\}\}') { Fail "$Name still contains the placeholder {{$($Matches[1])}}" }
    $text = $text -replace "`r`n", "`n"

    # 2. Drop the authoring comments; keep only the schema line, then add a
    #    short provenance header. Submitted manifests stay clean and the
    #    explanations live in the template, where they are maintained.
    $body = New-Object System.Collections.Generic.List[string]
    foreach ($line in ($text -split "`n")) {
        if ($line.TrimStart().StartsWith('#')) {
            if ($line -match '^#\s*yaml-language-server:') { $body.Add($line) }
            continue
        }
        if ($body.Count -le 1 -and $line.Trim().Length -eq 0) { continue }
        $body.Add($line)
    }
    $body.Insert(1, '# Generated by tools/winget/publish.ps1 in the Tandem Commander repository.')
    while ($body.Count -gt 0 -and $body[$body.Count - 1].Trim().Length -eq 0) { $body.RemoveAt($body.Count - 1) }
    return ([string]::Join("`n", $body) + "`n")
}

$values = @{
    VERSION         = $Version
    RELEASE_DATE    = $ReleaseDate
    RELEASE_SUMMARY = $ReleaseSummary
    SHA256          = 'SHA256PLACEHOLDER'   # replaced once the installer is hashed
}

#------------------------------------------------------------------
# The installer: locate, verify, hash
#------------------------------------------------------------------
$installerYaml = Render-Template -Name 'installer.yaml.in' -Values $values
if ($installerYaml -notmatch 'InstallerUrl:\s*(\S+)') { Fail 'installer.yaml.in produced no InstallerUrl' }
$InstallerUrl = $Matches[1]

if ($LocalFile) {
    if (-not (Test-Path -LiteralPath $LocalFile)) { Fail "local installer not found: $LocalFile" }
    $installerPath = (Resolve-Path -LiteralPath $LocalFile).Path
    $installerOrigin = 'local file'
} else {
    $installerPath = Join-Path $env:TEMP ("tandemcommander-winget-$Version-x64-setup.exe")
    Write-Host "Downloading $InstallerUrl"
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $InstallerUrl -OutFile $installerPath -UseBasicParsing
    } catch {
        Fail "could not download the release asset: $($_.Exception.Message)"
    }
    $installerOrigin = 'GitHub release'
}

$installerItem = Get-Item -LiteralPath $installerPath
if ($installerItem.Length -lt 1MB) { Fail "the installer is only $($installerItem.Length) bytes - wrong file?" }

$fileVersion = $installerItem.VersionInfo.ProductVersion
if ($fileVersion -and -not $fileVersion.StartsWith($Version)) {
    Warn "the installer reports version '$fileVersion' but $Version is being published"
}

if ($SkipSignatureCheck) {
    Warn 'Authenticode verification skipped - do NOT submit this run'
} else {
    if (-not (Test-Path -LiteralPath $CodesignCfg)) { Fail "not found: $CodesignCfg" }
    $thumb = $null
    foreach ($line in (Get-Content -LiteralPath $CodesignCfg)) {
        if ($line -match '^\s*thumbprint\s*=\s*(.+?)\s*$') { $thumb = $Matches[1] }
    }
    if (-not $thumb) { Fail "could not read 'thumbprint' from $CodesignCfg" }
    $sig = Get-AuthenticodeSignature -LiteralPath $installerPath
    if ($sig.Status -ne 'Valid') { Fail "the installer is not validly signed (status: $($sig.Status))" }
    if ($sig.SignerCertificate.Thumbprint -ne $thumb.ToUpper()) {
        Fail "the installer is signed by an unexpected certificate ($($sig.SignerCertificate.Subject))"
    }
}

$Sha256 = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash
$installerYaml = $installerYaml.Replace('SHA256PLACEHOLDER', $Sha256)

#------------------------------------------------------------------
# Banner
#------------------------------------------------------------------
$submitText = if ($Submit) { 'yes' } else { 'no (generate and validate only)' }

Write-Host ''
Write-Host '============================================================'
Write-Host ' Tandem Commander - winget manifests'
Write-Host '============================================================'
Write-Host (' Package       : {0}' -f $PackageIdentifier)
Write-Host (' Version       : {0}' -f $Version)
Write-Host (' Release date  : {0}' -f $ReleaseDate)
Write-Host (' Installer     : {0} ({1:N0} bytes)' -f $installerOrigin, $installerItem.Length)
Write-Host (' SHA256        : {0}' -f $Sha256)
Write-Host (' Scopes        : machine + user')
Write-Host (' Submit        : {0}' -f $submitText)
Write-Host '============================================================'
Write-Host ''

#------------------------------------------------------------------
# Write the manifests
#------------------------------------------------------------------
$OutDir = Join-Path $ScriptDir ('manifests\' + $Version)
if (Test-Path -LiteralPath $OutDir) { Remove-Item -LiteralPath $OutDir -Recurse -Force }
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$utf8 = New-Object System.Text.UTF8Encoding($false)
$written = @(
    @{ File = "$PackageIdentifier.yaml";              Text = (Render-Template -Name 'version.yaml.in' -Values $values) },
    @{ File = "$PackageIdentifier.installer.yaml";    Text = $installerYaml },
    @{ File = "$PackageIdentifier.locale.en-US.yaml"; Text = (Render-Template -Name 'locale.en-US.yaml.in' -Values $values) }
)
foreach ($w in $written) {
    $path = Join-Path $OutDir $w.File
    [System.IO.File]::WriteAllText($path, ($w.Text -replace "`n", "`r`n"), $utf8)
    Write-Host ('Wrote {0}' -f $path)
}

#------------------------------------------------------------------
# Validate
#------------------------------------------------------------------
$wingetCmd = Get-Command winget -ErrorAction SilentlyContinue
if ($wingetCmd) {
    Write-Host ''
    $validation = & $wingetCmd.Source validate --manifest $OutDir 2>&1 | Out-String
    Write-Host $validation.Trim()
    if ($validation -notmatch 'Manifest validation succeeded') {
        Fail 'winget validate rejected the manifests'
    }
} else {
    Warn 'winget is not available here - manifests not validated locally (wingetcreate validates on submit)'
}

#------------------------------------------------------------------
# Submit
#------------------------------------------------------------------
if (-not $Submit) {
    Write-Host ''
    Write-Host 'Manifests ready. Review them, then re-run with -Submit to open the pull request.'
    exit 0
}

if (-not $Token) { $Token = $env:WINGET_PAT }
if (-not $Token) {
    Fail 'no GitHub token - pass -Token or set WINGET_PAT (classic PAT with the public_repo scope)'
}

$wc = Get-Command wingetcreate -ErrorAction SilentlyContinue
if ($wc) {
    $wingetCreate = $wc.Source
} else {
    $wingetCreate = Join-Path $env:TEMP 'wingetcreate.exe'
    if (-not (Test-Path -LiteralPath $wingetCreate)) {
        Write-Host "wingetcreate not on PATH - downloading $WingetCreateUrl"
        try {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $WingetCreateUrl -OutFile $wingetCreate -UseBasicParsing
        } catch {
            Fail "could not obtain wingetcreate: $($_.Exception.Message)"
        }
    }
}

Write-Host ''
Write-Host "Submitting to microsoft/winget-pkgs via $wingetCreate"
& $wingetCreate submit --token $Token $OutDir
if ($LASTEXITCODE -ne 0) { Fail "wingetcreate submit failed with exit code $LASTEXITCODE" }

Write-Host ''
Write-Host "Pull request opened for $PackageIdentifier $Version."
exit 0

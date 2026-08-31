$s = $PSScriptRoot
$bat = Join-Path $s 'dumpenv.cmd'
$wd = 'D:\Projects\tandemcommander'
$env:TC073_MARKER = 'from-launcher'
Write-Output '===== WT running before ====='
Get-CimInstance Win32_Process -Filter "Name='WindowsTerminal.exe'" | ForEach-Object { 'PID {0} parent {1} started {2}' -f $_.ProcessId, $_.ParentProcessId, $_.CreationDate }
$wt = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\wt.exe'
Write-Output ('wt alias exists: ' + (Test-Path $wt))
Write-Output ('WT version: ' + ((Get-AppxPackage Microsoft.WindowsTerminal).Version))
$shell = New-Object -ComObject Shell.Application
$shell.ShellExecute('cmd.exe', ('/c "' + $bat + '" explorer'), $wd, 'open', 0)
function StartDirect($file, $argv, $hide) {
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $file; $psi.Arguments = $argv; $psi.UseShellExecute = $false
  $psi.WorkingDirectory = $wd; $psi.CreateNoWindow = $hide
  [System.Diagnostics.Process]::Start($psi) | Out-Null
}
StartDirect 'cmd.exe' ('/c "' + $bat + '" direct') $true
StartDirect $wt ('-d . cmd.exe /c "' + $bat + '" wt') $false
Start-Sleep 7
Write-Output '===== WT running after ====='
Get-CimInstance Win32_Process -Filter "Name='WindowsTerminal.exe'" | ForEach-Object { 'PID {0} parent {1} started {2}' -f $_.ProcessId, $_.ParentProcessId, $_.CreationDate }
Write-Output '===== files ====='
Get-ChildItem (Join-Path $s 'env-*.txt') | ForEach-Object { '{0} {1} bytes' -f $_.Name, $_.Length }
function LoadEnv($name) {
  $f = Join-Path $s ('env-' + $name + '.txt')
  if (-not (Test-Path $f)) { return $null }
  $t = Get-Content $f
  $h = @{}
  $inSet = $false
  foreach ($line in $t) {
    if ($line.Trim() -eq '[set]') { $inSet = $true; continue }
    if ($inSet -and $line -match '^([^=]+)=(.*)$') { $h[$matches[1]] = $matches[2] }
  }
  return @{ head = ($t | Select-Object -First 4); vars = $h }
}
$keys = 'USERPROFILE','HOMEDRIVE','HOMEPATH','HOME','TC073_MARKER','HF_HOME','HF_HUB_CACHE','APPDATA','LOCALAPPDATA','TEMP','USERNAME','WT_SESSION','SESSIONNAME','OPENAI_API_KEY','NODE_OPTIONS'
$envs = @{}
foreach ($n in 'explorer','direct','wt') {
  Write-Output ('===== ' + $n + ' =====')
  $e = LoadEnv $n
  if ($e -eq $null) { Write-Output 'MISSING'; continue }
  $envs[$n] = $e.vars
  $e.head | ForEach-Object { Write-Output $_ }
  Write-Output ('var count: ' + $e.vars.Count)
  foreach ($k in $keys) {
    if ($e.vars.ContainsKey($k)) {
      $v = $e.vars[$k]
      if ($k -eq 'OPENAI_API_KEY') { $v = '<set, ' + $v.Length + ' chars>' }
      Write-Output ('  ' + $k + '=' + $v)
    } else { Write-Output ('  ' + $k + ' <unset>') }
  }
}
function DiffEnv($a, $b, $na, $nb) {
  Write-Output ('===== diff ' + $na + ' vs ' + $nb + ' =====')
  if (-not $envs.ContainsKey($a) -or -not $envs.ContainsKey($b)) { Write-Output 'n/a'; return }
  $A = $envs[$a]; $B = $envs[$b]
  foreach ($k in ($A.Keys | Sort-Object)) { if (-not $B.ContainsKey($k)) { Write-Output ('  only in ' + $na + ': ' + $k) } }
  foreach ($k in ($B.Keys | Sort-Object)) { if (-not $A.ContainsKey($k)) { Write-Output ('  only in ' + $nb + ': ' + $k) } }
  foreach ($k in ($A.Keys | Sort-Object)) {
    if ($B.ContainsKey($k) -and $A[$k] -ne $B[$k]) {
      if ($k -ieq 'PATH') {
        $pa = $A[$k] -split ';' | Where-Object { $_ }; $pb = $B[$k] -split ';' | Where-Object { $_ }
        Write-Output ('  PATH differs: ' + $pa.Count + ' vs ' + $pb.Count + ' entries')
        $pa | Where-Object { $pb -notcontains $_ } | ForEach-Object { '    PATH only in ' + $na + ': ' + $_ }
        $pb | Where-Object { $pa -notcontains $_ } | ForEach-Object { '    PATH only in ' + $nb + ': ' + $_ }
      } elseif ($k -match 'KEY|TOKEN|SECRET|PASS') { Write-Output ('  ' + $k + ' differs (values hidden)') }
      else { Write-Output ('  ' + $k + ': [' + $na + '] ' + $A[$k] + ' | [' + $nb + '] ' + $B[$k]) }
    }
  }
}
DiffEnv 'explorer' 'direct' 'explorer' 'direct'
DiffEnv 'explorer' 'wt' 'explorer' 'wt'
Write-Output '===== WT settings ====='
$cfg = Join-Path $env:LOCALAPPDATA 'Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json'
if (Test-Path $cfg) { Select-String -Path $cfg -Pattern 'reloadEnvironmentVariables|defaultProfile|windowingBehavior|"commandline"|"name"|"guid"' | ForEach-Object { $_.Line.Trim() } | Select-Object -First 30 } else { 'no settings.json' }

$s = $PSScriptRoot
. (Join-Path $s 'penv-lib.ps1')
$exp = Get-CimInstance Win32_Process -Filter "Name='explorer.exe'" | Select-Object -First 1
$arr = [PEnv]::Get([int]$exp.ProcessId)
$m = @{}; foreach ($e in $arr) { $i = $e.IndexOf('=', 1); if ($i -gt 0) { $m[$e.Substring(0, $i)] = $e.Substring($i + 1) } }
Write-Output ('explorer.exe PID ' + $exp.ProcessId + ': ' + $m.Count + ' vars (this is the environment TC inherits when started from Explorer)')
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
$psi.Arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $s 'regen-child.ps1') + '"'
$psi.UseShellExecute = $false; $psi.RedirectStandardOutput = $true; $psi.CreateNoWindow = $true
$psi.EnvironmentVariables.Clear()
foreach ($k in $m.Keys) { $psi.EnvironmentVariables[$k] = $m[$k] }
$psi.EnvironmentVariables['TC073_MARKER'] = 'inherited-only'
$p = [System.Diagnostics.Process]::Start($psi)
$out = $p.StandardOutput.ReadToEnd(); $p.WaitForExit()
Write-Output $out

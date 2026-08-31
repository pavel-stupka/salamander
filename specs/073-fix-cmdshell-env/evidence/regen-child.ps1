Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public static class Regen { [DllImport("shell32.dll", SetLastError=true)] public static extern bool RegenerateUserEnvironment(out IntPtr prev, bool setCurrent); }
"@
function Snap { $m = @{}; $e = [Environment]::GetEnvironmentVariables(); foreach ($k in $e.Keys) { $m[[string]$k] = [string]$e[$k] }; return $m }
function Mask($k, $v) { if ($k -match 'KEY|TOKEN|SECRET|PASS') { return '<hidden>' } else { return $v } }
$before = Snap
Write-Output ('before: ' + $before.Count + ' vars; marker=' + $env:TC073_MARKER)
$prev = [IntPtr]::Zero
$ok = [Regen]::RegenerateUserEnvironment([ref]$prev, $true)
Write-Output ('RegenerateUserEnvironment returned ' + $ok + ' (lasterr ' + [Runtime.InteropServices.Marshal]::GetLastWin32Error() + ')')
$after = Snap
Write-Output ('after: ' + $after.Count + ' vars')
$dropped = @(); $added = @(); $changed = @()
foreach ($k in ($before.Keys | Sort-Object)) { if (-not $after.ContainsKey($k)) { $dropped += $k } elseif ($before[$k] -cne $after[$k]) { $changed += $k } }
foreach ($k in ($after.Keys | Sort-Object)) { if (-not $before.ContainsKey($k)) { $added += $k } }
Write-Output ('DROPPED by regeneration (TC re-adds these): ' + ($dropped -join ', '))
Write-Output ('ADDED by regeneration (TC keeps these): ' + ($added -join ', '))
Write-Output ('CHANGED by regeneration (TC keeps the NEW value):')
foreach ($k in $changed) {
  if ($k -ieq 'PATH') {
    $pa = $before[$k] -split ';' | Where-Object { $_ }; $pb = $after[$k] -split ';' | Where-Object { $_ }
    Write-Output ('  PATH: ' + $pa.Count + ' -> ' + $pb.Count + ' entries; same set: ' + (@(Compare-Object $pa $pb).Count -eq 0) + '; same order: ' + (($pa -join ';') -ceq ($pb -join ';')))
    $pa | Where-Object { $pb -notcontains $_ } | ForEach-Object { '    lost: ' + $_ }
    $pb | Where-Object { $pa -notcontains $_ } | ForEach-Object { '    gained: ' + $_ }
    Write-Output '    before (first 8):'; $pa | Select-Object -First 8 | ForEach-Object { '      ' + $_ }
    Write-Output '    after (first 8):'; $pb | Select-Object -First 8 | ForEach-Object { '      ' + $_ }
  } else { Write-Output ('  ' + $k + ': [before] ' + (Mask $k $before[$k]) + ' | [after] ' + (Mask $k $after[$k])) }
}
foreach ($k in 'USERPROFILE','HOMEDRIVE','HOMEPATH','APPDATA','LOCALAPPDATA','TEMP','TMP','USERNAME','COMSPEC','OneDrive','TC073_MARKER') { Write-Output ('  final ' + $k + '=' + $(if ($after.ContainsKey($k)) { $after[$k] } else { '<unset>' })) }

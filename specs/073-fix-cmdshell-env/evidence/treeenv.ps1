# treeenv.ps1 - capture the failing state: every process under Tandem Commander
# (the shell, the .bat's cmd, node.exe, ...) with its command line, working
# directory and the difference of its environment from Explorer's.
#
# Run it WHILE the failing stack is up (or while the shell the .bat opened is
# still open), from Windows PowerShell 5.1:
#   .\treeenv.ps1 > tc-tree-<date>.txt
# Values of variables whose name contains KEY/TOKEN/SECRET/PASS are masked.
# Read-only: opens processes for reading only, changes nothing.

. (Join-Path $PSScriptRoot 'penv-lib.ps1')

Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices; using System.Text;
public static class PCwd {
  [StructLayout(LayoutKind.Sequential)] public struct PBI { public IntPtr R1; public IntPtr PebBaseAddress; public IntPtr R2a; public IntPtr R2b; public IntPtr Pid; public IntPtr R3; }
  [DllImport("ntdll.dll")] static extern int NtQueryInformationProcess(IntPtr h, int cls, ref PBI pbi, int len, out int ret);
  [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, IntPtr size, out IntPtr read);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  static byte[] Read(IntPtr h, IntPtr addr, int n) { var b = new byte[n]; IntPtr r; if (!ReadProcessMemory(h, addr, b, (IntPtr)n, out r)) throw new Exception("ReadProcessMemory failed: " + Marshal.GetLastWin32Error()); return b; }
  public static string Get(int pid) {
    IntPtr h = OpenProcess(0x0010 | 0x0400, false, pid);
    if (h == IntPtr.Zero) throw new Exception("OpenProcess failed: " + Marshal.GetLastWin32Error());
    try {
      PBI pbi = new PBI(); int ret;
      int st = NtQueryInformationProcess(h, 0, ref pbi, Marshal.SizeOf(pbi), out ret);
      if (st != 0) throw new Exception("NtQueryInformationProcess: " + st);
      IntPtr pp = (IntPtr)BitConverter.ToInt64(Read(h, pbi.PebBaseAddress + 0x20, 8), 0);
      ushort len = BitConverter.ToUInt16(Read(h, pp + 0x38, 2), 0);            // CurrentDirectory.DosPath.Length
      IntPtr buf = (IntPtr)BitConverter.ToInt64(Read(h, pp + 0x40, 8), 0);   // CurrentDirectory.DosPath.Buffer
      if (len == 0 || buf == IntPtr.Zero) return "";
      return Encoding.Unicode.GetString(Read(h, buf, len));
    } finally { CloseHandle(h); }
  }
}
"@

function ToMap($arr) { $m = @{}; foreach ($e in $arr) { $i = $e.IndexOf('=', 1); if ($i -gt 0) { $m[$e.Substring(0, $i)] = $e.Substring($i + 1) } }; return $m }
function Mask($k, $v) { if ($k -match 'KEY|TOKEN|SECRET|PASS') { return '<hidden, ' + $v.Length + ' chars>' } else { return $v } }
function DiffEnv($ref, $m) {
  $lines = @()
  foreach ($k in ($ref.Keys | Sort-Object)) { if (-not $m.ContainsKey($k) -and $k -notlike '=*') { $lines += ('      missing: ' + $k) } }
  foreach ($k in ($m.Keys | Sort-Object)) { if (-not $ref.ContainsKey($k) -and $k -notlike '=*') { $lines += ('      added:   ' + $k + '=' + (Mask $k $m[$k])) } }
  foreach ($k in ($ref.Keys | Sort-Object)) {
    if ($m.ContainsKey($k) -and $ref[$k] -cne $m[$k]) {
      if ($k -ieq 'PATH') {
        $pa = $ref[$k] -split ';' | Where-Object { $_ }; $pb = $m[$k] -split ';' | Where-Object { $_ }
        $lines += ('      PATH differs: ' + $pa.Count + ' vs ' + $pb.Count + ' entries')
        $pa | Where-Object { $pb -notcontains $_ } | ForEach-Object { $lines += ('        lost:   ' + $_) }
        $pb | Where-Object { $pa -notcontains $_ } | ForEach-Object { $lines += ('        gained: ' + $_) }
      } else { $lines += ('      changed: ' + $k + ': [explorer] ' + (Mask $k $ref[$k]) + ' | [this] ' + (Mask $k $m[$k])) }
    }
  }
  if ($lines.Count -eq 0) { $lines += '      (identical to Explorer)' }
  return $lines
}

$all = Get-CimInstance Win32_Process
$byPid = @{}; foreach ($p in $all) { $byPid[[int]$p.ProcessId] = $p }
$exp = $all | Where-Object { $_.Name -eq 'explorer.exe' } | Sort-Object CreationDate | Select-Object -First 1
$ref = ToMap ([PEnv]::Get([int]$exp.ProcessId))
Write-Output ('reference: explorer.exe PID ' + $exp.ProcessId + ' started ' + $exp.CreationDate + ', ' + $ref.Count + ' variables')
Write-Output ('  USERPROFILE=' + $ref['USERPROFILE'] + '  USERNAME=' + $ref['USERNAME'])

$roots = @($all | Where-Object { $_.Name -eq 'tandemcommander.exe' })
if ($roots.Count -eq 0) { Write-Output 'tandemcommander.exe is not running - start it, reproduce the failure, then run this script.'; exit 1 }

function Descend($p, $depth) {
  $indent = '  ' * $depth
  $par = $byPid[[int]$p.ParentProcessId]
  $parName = if ($par) { $par.Name } else { '<gone>' }
  Write-Output ($indent + '- ' + $p.Name + ' PID ' + $p.ProcessId + ' (parent ' + $p.ParentProcessId + ' ' + $parName + ') started ' + $p.CreationDate)
  $cl = [string]$p.CommandLine; if ($cl.Length -gt 220) { $cl = $cl.Substring(0, 220) + '...' }
  Write-Output ($indent + '    cmdline: ' + $cl)
  try { Write-Output ($indent + '    cwd:     ' + [PCwd]::Get([int]$p.ProcessId)) } catch { Write-Output ($indent + '    cwd:     <' + $_.Exception.Message + '>') }
  try {
    $m = ToMap ([PEnv]::Get([int]$p.ProcessId))
    Write-Output ($indent + '    env:     ' + $m.Count + ' variables; USERPROFILE=' + $(if ($m.ContainsKey('USERPROFILE')) { $m['USERPROFILE'] } else { '<unset>' }) + '; HOME=' + $(if ($m.ContainsKey('HOME')) { $m['HOME'] } else { '<unset>' }))
    DiffEnv $ref $m | ForEach-Object { Write-Output ($indent + $_) }
  } catch { Write-Output ($indent + '    env:     <' + $_.Exception.Message + '>') }
  foreach ($c in ($all | Where-Object { [int]$_.ParentProcessId -eq [int]$p.ProcessId -and $_.CreationDate -ge $p.CreationDate } | Sort-Object CreationDate)) { Descend $c ($depth + 1) }
}
foreach ($r in $roots) { Descend $r 0 }

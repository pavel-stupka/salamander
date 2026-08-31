Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices; using System.Text; using System.Collections.Generic;
public static class PEnv {
  [StructLayout(LayoutKind.Sequential)] public struct PBI { public IntPtr R1; public IntPtr PebBaseAddress; public IntPtr R2a; public IntPtr R2b; public IntPtr Pid; public IntPtr R3; }
  [DllImport("ntdll.dll")] static extern int NtQueryInformationProcess(IntPtr h, int cls, ref PBI pbi, int len, out int ret);
  [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, IntPtr size, out IntPtr read);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  static byte[] Read(IntPtr h, IntPtr addr, int n) { var b = new byte[n]; IntPtr r; if (!ReadProcessMemory(h, addr, b, (IntPtr)n, out r)) throw new Exception("ReadProcessMemory failed: " + Marshal.GetLastWin32Error()); return b; }
  public static string[] Get(int pid) {
    IntPtr h = OpenProcess(0x0010 | 0x0400, false, pid);
    if (h == IntPtr.Zero) throw new Exception("OpenProcess failed: " + Marshal.GetLastWin32Error());
    try {
      PBI pbi = new PBI(); int ret;
      int st = NtQueryInformationProcess(h, 0, ref pbi, Marshal.SizeOf(pbi), out ret);
      if (st != 0) throw new Exception("NtQueryInformationProcess: " + st);
      IntPtr pp = (IntPtr)BitConverter.ToInt64(Read(h, pbi.PebBaseAddress + 0x20, 8), 0);
      IntPtr env = (IntPtr)BitConverter.ToInt64(Read(h, pp + 0x80, 8), 0);
      long size = BitConverter.ToInt64(Read(h, pp + 0x3F0, 8), 0);
      if (size <= 0 || size > 8 * 1024 * 1024) throw new Exception("implausible EnvironmentSize " + size);
      byte[] block = Read(h, env, (int)size);
      string s = Encoding.Unicode.GetString(block);
      var list = new List<string>();
      foreach (string e in s.Split('\0')) { if (e.Length > 0) list.Add(e); }
      return list.ToArray();
    } finally { CloseHandle(h); }
  }
}
"@
function ToMap($arr) { $m = @{}; foreach ($e in $arr) { $i = $e.IndexOf('=', 1); if ($i -gt 0) { $m[$e.Substring(0, $i)] = $e.Substring($i + 1) } }; return $m }
function Mask($k, $v) { if ($k -match 'KEY|TOKEN|SECRET|PASS') { return '<hidden, ' + $v.Length + ' chars>' } else { return $v } }
$procs = @{}
foreach ($name in 'explorer.exe','tandemcommander.exe') {
  $p = Get-CimInstance Win32_Process -Filter "Name='$name'" | Select-Object -First 1
  Write-Output ('===== ' + $name + ' PID ' + $p.ProcessId + ' started ' + $p.CreationDate + ' =====')
  try { $arr = [PEnv]::Get([int]$p.ProcessId); $m = ToMap $arr; $procs[$name] = $m; Write-Output ('var count: ' + $m.Count)
    foreach ($k in 'USERPROFILE','HOMEDRIVE','HOMEPATH','HOME','APPDATA','LOCALAPPDATA','TEMP','TMP','USERNAME','COMSPEC','XDG_CACHE_HOME','HF_HOME','HF_HUB_CACHE','HF_HUB_OFFLINE','VIRTUAL_ENV','CONDA_PREFIX','NODE_OPTIONS','PYTHONPATH','OPENAI_API_KEY') { if ($m.ContainsKey($k)) { Write-Output ('  ' + $k + '=' + (Mask $k $m[$k])) } else { Write-Output ('  ' + $k + ' <unset>') } }
  } catch { Write-Output ('  ERROR: ' + $_.Exception.Message) }
}
Write-Output '===== diff explorer.exe vs tandemcommander.exe ====='
$A = $procs['explorer.exe']; $B = $procs['tandemcommander.exe']
if ($A -and $B) {
  $any = $false
  foreach ($k in ($A.Keys | Sort-Object)) { if (-not $B.ContainsKey($k)) { Write-Output ('  only in explorer: ' + $k + '=' + (Mask $k $A[$k])); $any = $true } }
  foreach ($k in ($B.Keys | Sort-Object)) { if (-not $A.ContainsKey($k)) { Write-Output ('  only in TC: ' + $k + '=' + (Mask $k $B[$k])); $any = $true } }
  foreach ($k in ($A.Keys | Sort-Object)) { if ($B.ContainsKey($k) -and $A[$k] -ne $B[$k]) { $any = $true
    if ($k -ieq 'PATH') { $pa = $A[$k] -split ';' | Where-Object { $_ }; $pb = $B[$k] -split ';' | Where-Object { $_ }; Write-Output ('  PATH differs: ' + $pa.Count + ' vs ' + $pb.Count + ' entries'); $pa | Where-Object { $pb -notcontains $_ } | ForEach-Object { '    only in explorer: ' + $_ }; $pb | Where-Object { $pa -notcontains $_ } | ForEach-Object { '    only in TC: ' + $_ } }
    else { Write-Output ('  ' + $k + ': [explorer] ' + (Mask $k $A[$k]) + ' | [TC] ' + (Mask $k $B[$k])) } } }
  if (-not $any) { Write-Output '  IDENTICAL' }
} else { Write-Output '  n/a' }
Write-Output '===== registry env (what a regeneration would produce) ====='
$u = Get-ItemProperty 'HKCU:\Environment'; $u.PSObject.Properties | Where-Object { $_.Name -notlike 'PS*' } | ForEach-Object { '  HKCU ' + $_.Name + '=' + (Mask $_.Name ([string]$_.Value)) }

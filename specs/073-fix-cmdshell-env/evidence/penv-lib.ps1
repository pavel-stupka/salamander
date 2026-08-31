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

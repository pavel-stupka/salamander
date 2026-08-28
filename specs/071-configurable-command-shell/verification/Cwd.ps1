# Reads the current directory of another 64-bit process from its PEB
# (RTL_USER_PROCESS_PARAMETERS.CurrentDirectory.DosPath). Used by the 071
# GUI verification to prove where a launched shell really started.
if (-not ("Peb071" -as [type])) {
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class Peb071 {
    [StructLayout(LayoutKind.Sequential)]
    struct PROCESS_BASIC_INFORMATION { public IntPtr Reserved1; public IntPtr PebBaseAddress; public IntPtr Reserved2_0; public IntPtr Reserved2_1; public IntPtr UniqueProcessId; public IntPtr Reserved3; }
    [DllImport("ntdll.dll")] static extern int NtQueryInformationProcess(IntPtr h, int cls, ref PROCESS_BASIC_INFORMATION pbi, int size, out int ret);
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int size, out IntPtr read);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    static IntPtr ReadPtr(IntPtr h, IntPtr addr) { byte[] b = new byte[8]; IntPtr r; if (!ReadProcessMemory(h, addr, b, 8, out r)) throw new Exception("RPM ptr @" + addr.ToString("X")); return (IntPtr)BitConverter.ToInt64(b, 0); }
    public static string GetCwd(int pid) {
        IntPtr h = OpenProcess(0x0410 /*QUERY_INFORMATION|VM_READ*/, false, pid);
        if (h == IntPtr.Zero) throw new Exception("OpenProcess failed: " + Marshal.GetLastWin32Error());
        try {
            PROCESS_BASIC_INFORMATION pbi = new PROCESS_BASIC_INFORMATION(); int ret;
            int st = NtQueryInformationProcess(h, 0, ref pbi, Marshal.SizeOf(pbi), out ret);
            if (st != 0) throw new Exception("NtQIP status " + st.ToString("X"));
            IntPtr pp = ReadPtr(h, pbi.PebBaseAddress + 0x20);            // PEB.ProcessParameters (x64)
            // RTL_USER_PROCESS_PARAMETERS.CurrentDirectory at 0x38: UNICODE_STRING DosPath {USHORT Length, USHORT MaxLength, PWSTR Buffer}
            byte[] us = new byte[16]; IntPtr r;
            if (!ReadProcessMemory(h, pp + 0x38, us, 16, out r)) throw new Exception("RPM curdir");
            ushort len = BitConverter.ToUInt16(us, 0);
            IntPtr buf = (IntPtr)BitConverter.ToInt64(us, 8);
            byte[] s = new byte[len];
            if (len > 0 && !ReadProcessMemory(h, buf, s, len, out r)) throw new Exception("RPM curdir buf");
            return Encoding.Unicode.GetString(s, 0, len);
        } finally { CloseHandle(h); }
    }
}
"@
}
function Get-ProcessCwd([int]$ProcessId) { [Peb071]::GetCwd($ProcessId) }

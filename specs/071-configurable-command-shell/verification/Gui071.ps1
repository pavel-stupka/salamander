# Feature 071 - automated GUI verification of the Command Shell setting (v2, Win32-driven).
# Drives the Debug build with PostMessage/keybd_event, finds its windows with EnumWindows,
# reads message boxes from their Static controls, and reads the working directory of every
# launched shell from its PEB.
param([string]$Only = "")

$ErrorActionPreference = "Continue"
$scratch = "C:\Users\pavel\AppData\Local\Temp\claude\E--Projects-tandemcommander\5e8133f4-e803-4818-badb-8187f76aaa29\scratchpad"
. "$scratch\Cwd.ps1"
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing, Microsoft.VisualBasic

if (-not ("W071" -as [type])) {
Add-Type -TypeDefinition @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public static class W071 {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint msg, IntPtr w, StringBuilder l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint msg, IntPtr w, string l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  public static void ForceForeground(IntPtr h) { uint pid1; uint pid2; uint fg = GetWindowThreadProcessId(GetForegroundWindow(), out pid1); uint me = GetCurrentThreadId(); uint target = GetWindowThreadProcessId(h, out pid2); AttachThreadInput(me, fg, true); AttachThreadInput(me, target, true); BringWindowToTop(h); SetForegroundWindow(h); AttachThreadInput(me, fg, false); AttachThreadInput(me, target, false); }
  public static int[] Rect(IntPtr h) { RECT r; GetWindowRect(h, out r); return new[] { r.L, r.T, r.R, r.B }; }
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  public static string Text(IntPtr h) { int n = (int)SendMessageW(h, 0x000E /*WM_GETTEXTLENGTH*/, IntPtr.Zero, IntPtr.Zero); var sb = new StringBuilder(n + 2); SendMessageW(h, 0x000D /*WM_GETTEXT*/, (IntPtr)(n + 1), sb); return sb.ToString(); }
  public static string Cls(IntPtr h) { var sb = new StringBuilder(256); GetClassNameW(h, sb, 256); return sb.ToString(); }
  public static List<IntPtr> TopWindows(uint pid) { var r = new List<IntPtr>(); EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) r.Add(h); return true; }, IntPtr.Zero); return r; }
  public static List<IntPtr> Children(IntPtr h) { var r = new List<IntPtr>(); EnumChildWindows(h, (c, l) => { r.Add(c); return true; }, IntPtr.Zero); return r; }
  public static IntPtr FindDlgItemDeep(IntPtr root, int id) { foreach (var c in Children(root)) if (GetDlgCtrlID(c) == id) return c; return IntPtr.Zero; }
  public static string StaticTexts(IntPtr h) { var sb = new StringBuilder(); foreach (var c in Children(h)) if (Cls(c) == "Static") { var t = Text(c); if (t.Length > 0) { if (sb.Length > 0) sb.Append(" | "); sb.Append(t); } } return sb.ToString(); }
  public static string ComboItem(IntPtr h, int i) { var sb = new StringBuilder(1024); SendMessageW(h, 0x0148, (IntPtr)i, sb); return sb.ToString(); }
  public static int ComboCount(IntPtr h) { return (int)SendMessageW(h, 0x0146, IntPtr.Zero, IntPtr.Zero); }
  public static int ComboSel(IntPtr h) { return (int)SendMessageW(h, 0x0147, IntPtr.Zero, IntPtr.Zero); }
  public static void ComboSelect(IntPtr combo, int i) { SendMessageW(combo, 0x014E /*CB_SETCURSEL*/, (IntPtr)i, IntPtr.Zero); IntPtr page = GetParent(combo); int id = GetDlgCtrlID(combo); SendMessageW(page, 0x0111, (IntPtr)((1 << 16) | id) /*CBN_SELCHANGE*/, combo); }
  public static void SetText(IntPtr h, string s) { SendMessageW(h, 0x000C, IntPtr.Zero, s); }
  public static void Command(IntPtr dlg, int id) { PostMessage(dlg, 0x0111, (IntPtr)id, IntPtr.Zero); }
  public static void ClickButton(IntPtr page, IntPtr button) { PostMessage(page, 0x0111, (IntPtr)GetDlgCtrlID(button), button); }
}
"@
}

$exe = "E:\Projects\tandemcommander\build\tandemcommander\Debug_x64\tandemcommander.exe"
$regKey = "HKCU:\Software\Tandem Commander\0.1\Configuration"
$root = "E:\Projects\tandemcommander\temp\071test"
$plain = "$root\plain"
$nonAscii = "$root\Můj disk\Nový projekt"
$longDir = (Get-Content "$root\longpath.txt" -Encoding UTF8).Trim()
$longDirC = (Get-Content "$root\longpath-c.txt" -Encoding UTF8).Trim()
$unc = "\\localhost\E$\Projects\tandemcommander\temp\071test\plain"
$logFile = "$scratch\gui-results.md"
$script:results = @()
$shellNames = @("cmd", "powershell", "pwsh", "WindowsTerminal", "OpenConsole", "git-bash", "mintty", "bash", "sh")

function Log([string]$s) { $s | Out-File -Append -Encoding utf8 $logFile; Write-Host $s }
function Result([string]$id, [bool]$ok, [string]$detail) {
    $script:results += [pscustomobject]@{ Case = $id; Result = $(if ($ok) { "PASS" } else { "FAIL" }); Detail = $detail }
    Log ("{0} {1} - {2}" -f $(if ($ok) { "PASS" } else { "FAIL" }), $id, $detail)
}
function Norm([string]$p) { if ($null -eq $p) { return "" } return $p.TrimEnd("\").ToLowerInvariant() }
function Set-Shell([int]$preset, [string]$program, [string]$shellArgs) {
    Set-ItemProperty -Path $regKey -Name "Command Shell Preset" -Value $preset -Type DWord
    Set-ItemProperty -Path $regKey -Name "Command Shell Program" -Value $program -Type String
    Set-ItemProperty -Path $regKey -Name "Command Shell Arguments" -Value $shellArgs -Type String
}
function App-Windows($p) { [W071]::TopWindows([uint32]$p.Id) }
function Find-Win($p, [string]$titleRegex, [int]$timeoutSec = 6) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    do {
        foreach ($h in (App-Windows $p)) { if (([W071]::Text($h)) -match $titleRegex) { return $h } }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)
    return [IntPtr]::Zero
}
function Extra-Windows($p) { (App-Windows $p) | Where-Object { $_ -ne $p.MainWindowHandle } | ForEach-Object { "[" + [W071]::Text($_) + ": " + [W071]::StaticTexts($_) + "]" } }
function Dismiss-Box($p, [string]$titleRegex, [int]$timeoutSec = 5) {
    $h = Find-Win $p $titleRegex $timeoutSec
    if ($h -eq [IntPtr]::Zero) { return $null }
    $text = [W071]::StaticTexts($h)
    [W071]::Command($h, 1)   # IDOK
    Start-Sleep -Milliseconds 500
    return $text
}
function Start-App([string]$activePath) {
    $p = Start-Process -FilePath $exe -ArgumentList ("-a `"" + $activePath + "`"") -PassThru
    $deadline = (Get-Date).AddSeconds(25)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 300
        $p.Refresh()
        if ($p.MainWindowHandle -ne 0 -and $p.MainWindowTitle -ne "") { break }
    }
    Start-Sleep -Milliseconds 1500
    $p.Refresh()
    $extra = Extra-Windows $p
    if ($extra) { Log ("  unexpected window(s) at start: " + ($extra -join " ")); foreach ($h in (App-Windows $p)) { if ($h -ne $p.MainWindowHandle) { [W071]::Command($h, 1) } }; Start-Sleep -Milliseconds 500 }
    return $p
}
function Stop-App($p) {
    if ($p -and -not $p.HasExited) {
        foreach ($h in (App-Windows $p)) { if ($h -ne $p.MainWindowHandle) { Log ("  closing leftover window [" + [W071]::Text($h) + "]"); [W071]::Command($h, 2); Start-Sleep -Milliseconds 300 } }
        $p.CloseMainWindow() | Out-Null
        if (-not $p.WaitForExit(15000)) { Log ("  (app did not exit on WM_CLOSE - windows: " + ((Extra-Windows $p) -join " ") + " - killing)"); Stop-Process -Id $p.Id -Force }
    }
    Start-Sleep -Milliseconds 500
}
function Activate($p) {
    for ($i = 0; $i -lt 4; $i++) {
        try { [Microsoft.VisualBasic.Interaction]::AppActivate($p.Id) } catch {}
        [W071]::ForceForeground($p.MainWindowHandle)
        Start-Sleep -Milliseconds 500
        if ([W071]::GetForegroundWindow() -eq $p.MainWindowHandle) { return }
    }
    Log "  (could not bring the app to the foreground)"
}
function Key([byte]$vk, [bool]$ctrl = $false) {
    if ($ctrl) { [W071]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero) }
    [W071]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
    [W071]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)
    if ($ctrl) { [W071]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero) }
}
function Focus-Panel($p) {
    # click into the active (right) panel's file list so the panel, not the command line box, owns the keyboard
    $r = [W071]::Rect($p.MainWindowHandle)
    $x = $r[2] - 200; $y = $r[1] + 260
    [W071]::SetCursorPos($x, $y) | Out-Null
    [W071]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60; [W071]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 400
}
function Trigger($p, [string]$how) {
    switch ($how) {
        "msg"       { [W071]::Command($p.MainWindowHandle, 738) }   # CM_DOSSHELL
        "numdiv"    { Activate $p; Focus-Panel $p; Key 0x6F }                       # VK_DIVIDE (accelerator table 2 - skipped while the command line box has the focus)
        "ctrlslash" { Activate $p; Focus-Panel $p; Key 0xBF $true }                 # Ctrl + VK_OEM_2
    }
}
function New-Shells([datetime]$since, [int]$timeoutSec = 8) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    do {
        Start-Sleep -Milliseconds 500
        $found = Get-Process | Where-Object { $shellNames -contains $_.ProcessName -and $_.StartTime -gt $since } | Sort-Object StartTime
        if ($found) { Start-Sleep -Milliseconds 1200; return @(Get-Process | Where-Object { $shellNames -contains $_.ProcessName -and $_.StartTime -gt $since } | Sort-Object StartTime) }
    } while ((Get-Date) -lt $deadline)
    return @()
}
function Kill-Shells([datetime]$since) { Get-Process | Where-Object { $shellNames -contains $_.ProcessName -and $_.StartTime -gt $since } | ForEach-Object { try { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue } catch {} }; Start-Sleep -Milliseconds 400 }
function Cwd-Of($proc) { try { return [Peb071]::GetCwd($proc.Id) } catch { return "<unreadable: $($_.Exception.Message)>" } }
function Describe($procs) { ($procs | ForEach-Object { "{0}({1}) cwd=[{2}]" -f $_.ProcessName, $_.Id, (Cwd-Of $_) }) -join "; " }
function Screenshot([IntPtr]$h, [string]$file) {
    $r = [W071]::Rect($h)
    if ($r[2] -le $r[0] -or $r[3] -le $r[1]) { Log "  (screenshot: empty rect for $h)"; return }
    $bmp = New-Object System.Drawing.Bitmap ($r[2] - $r[0]), ($r[3] - $r[1])
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($r[0], $r[1], 0, 0, $bmp.Size)
    $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
}
function Select-Page([IntPtr]$dlg, [string]$name) {
    # the Command Shell page is the 16th top-level tree item (index 15, inserted after Hot Paths);
    # walk TVGN_ROOT/TVGN_NEXT and select with TVGN_CARET - no focus, no UIA needed
    $tree = [IntPtr]::Zero
    foreach ($c in [W071]::Children($dlg)) { if ([W071]::Cls($c) -eq "SysTreeView32") { $tree = $c; break } }
    if ($tree -eq [IntPtr]::Zero) { Log "  no SysTreeView32 in the dialog"; return $false }
    $item = [W071]::SendMessageW($tree, 0x110A, [IntPtr]0, [IntPtr]0)          # TVM_GETNEXTITEM TVGN_ROOT
    for ($i = 0; $i -lt 15 -and $item -ne [IntPtr]::Zero; $i++) { $item = [W071]::SendMessageW($tree, 0x110A, [IntPtr]1, $item) }   # TVGN_NEXT
    if ($item -eq [IntPtr]::Zero) { Log "  tree has fewer than 16 items"; return $false }
    [W071]::SendMessageW($tree, 0x110B, [IntPtr]9, $item) | Out-Null            # TVM_SELECTITEM TVGN_CARET
    Start-Sleep -Milliseconds 800
    $combo = [W071]::FindDlgItemDeep($dlg, 6226)
    if ($combo -eq [IntPtr]::Zero) { Log "  page selected but IDC_CMDSHELL_PRESET not found (wrong page?)"; return $false }
    return $true
}
function Open-Config($p, [string]$dlgTitle, [string]$pageName) {
    [W071]::Command($p.MainWindowHandle, 686)   # CM_CONFIGURATION
    $dlg = Find-Win $p ("^" + [regex]::Escape($dlgTitle) + "$") 8
    if ($dlg -eq [IntPtr]::Zero) { Log "  Configuration dialog not found; windows: $((Extra-Windows $p) -join ' ')"; return [IntPtr]::Zero }
    if (-not (Select-Page $dlg $pageName)) { Log "  tree item '$pageName' not found"; return [IntPtr]::Zero }
    return $dlg
}
function Ctl([IntPtr]$dlg, [int]$id) { [W071]::FindDlgItemDeep($dlg, $id) }
function Page-Combo-Select([IntPtr]$dlg, [int]$index) { [W071]::ComboSelect((Ctl $dlg 6226), $index); Start-Sleep -Milliseconds 400 }
function Dlg-Button([IntPtr]$dlg, [string[]]$texts) {
    foreach ($c in [W071]::Children($dlg)) {
        if ([W071]::Cls($c) -eq "Button" -and ($texts -contains [W071]::Text($c))) { [W071]::ClickButton([W071]::GetParent($c), $c); Start-Sleep -Milliseconds 800; return $true }
    }
    Log ("  button " + ($texts -join "/") + " not found in dialog " + [W071]::Text($dlg)); return $false
}
function Dlg-OK([IntPtr]$dlg) { Dlg-Button $dlg @("OK") | Out-Null }
function Dlg-Cancel([IntPtr]$dlg) { Dlg-Button $dlg @("Cancel", "Storno") | Out-Null }

# ---------------------------------------------------------------------------
"# GUI verification run $(Get-Date -Format s)" | Out-File -Encoding utf8 $logFile
$orig = Get-ItemProperty -Path $regKey
$origPreset = $orig."Command Shell Preset"; $origProgram = $orig."Command Shell Program"; $origArgs = $orig."Command Shell Arguments"; $origLang = $orig."Language"
Log "original: preset=$origPreset program=[$origProgram] args=[$origArgs] language=$origLang"
Set-ItemProperty -Path $regKey -Name "Language" -Value "english.slg" -Type String
Remove-Item -LiteralPath "$nonAscii\cwd071.txt" -ErrorAction SilentlyContinue

function Launch-Case([string]$id, [int]$preset, [string]$program, [string]$shellArgs, [string]$activePath, [string]$how, [string]$expectName, [string]$expectCwd, [string]$expectBoxTitle = "", [string]$expectBoxText = "", [string]$waitFile = "") {
    if ($Only -and $id -notmatch $Only) { return }
    Set-Shell $preset $program $shellArgs
    $p = Start-App $activePath
    if ($p.HasExited -or $p.MainWindowHandle -eq 0) { Result $id $false "app did not start"; return }
    $title = $p.MainWindowTitle
    $since = Get-Date
    Trigger $p $how
    if ($expectBoxTitle) {
        $text = Dismiss-Box $p ("^" + [regex]::Escape($expectBoxTitle) + "$") 8
        $ok = ($null -ne $text) -and ($text -match [regex]::Escape($expectBoxText))
        $procs = New-Shells $since 2
        Result $id ($ok -and $procs.Count -eq 0) ("box=[" + $text + "] launched=" + $procs.Count)
    } else {
        $procs = New-Shells $since 8
        $target = $procs | Where-Object { $_.ProcessName -eq $expectName } | Select-Object -First 1
        $script:lastCmdLine = if ($target) { (Get-CimInstance Win32_Process -Filter ("ProcessId = " + $target.Id)).CommandLine } else { "" }
        $cwd = if ($target) { Cwd-Of $target } else { "" }
        $resolved = ""
        if ($cwd -and (Test-Path -LiteralPath $cwd)) { $resolved = (Get-Item -LiteralPath $cwd).FullName }
        $ok = ($null -ne $target) -and (((Norm $cwd) -eq (Norm $expectCwd)) -or ((Norm $resolved) -eq (Norm $expectCwd)))
        Result $id $ok ("via $how, panel=[" + $title + "] -> " + (Describe $procs) + $(if ($resolved -and (Norm $resolved) -ne (Norm $cwd)) { " (resolves to [$resolved])" } else { "" }))
    }
    if ($waitFile) { $dl = (Get-Date).AddSeconds(8); while ((Get-Date) -lt $dl -and -not (Test-Path -LiteralPath $waitFile)) { Start-Sleep -Milliseconds 300 } }
    Kill-Shells $since
    Stop-App $p
}

# ---- launch cases (registry-configured) ----
Launch-Case "L01-cmd-default-msg"      0 "" "" $plain "msg"       "cmd"        $plain
Launch-Case "L02-cmd-numdiv"           0 "" "" $plain "numdiv"    "cmd"        $plain
Launch-Case "L03-cmd-ctrlslash"        0 "" "" $plain "ctrlslash" "cmd"        $plain
Launch-Case "L04-powershell"           1 "" "" $plain "msg"       "powershell" $plain
Launch-Case "L05-wt-defaultprofile"    3 "" "" $plain "msg"       "bash"       $plain
Launch-Case "L06-gitbash"              4 "" "" $plain "msg"       "bash"       $plain
Launch-Case "L07-powershell-nonascii"  1 "" "" $nonAscii "msg"    "powershell" $nonAscii
Launch-Case "L08-gitbash-nonascii"     4 "" "" $nonAscii "msg"    "bash"       $nonAscii
Launch-Case "L09-cmd-longpath-8dot3"   0 "" "" $longDirC "msg"    "cmd"        $longDirC
Launch-Case "L09b-longpath-no8dot3-E2" 0 "" "" $longDir "msg" "" "" "Error Starting Command Shell" "cmd.exe"
Launch-Case "L10-powershell-unc"       1 "" "" $unc "msg"         "powershell" $unc
Launch-Case "L11-cmd-unc-fallback"     0 "" "" $unc "msg"         "cmd"        "$env:SystemRoot"
Launch-Case "L12-cmd-archive-panel"    0 "" "" "$plain\test.zip" "msg" "cmd"   $plain
Launch-Case "L13-custom-fullpath"      5 "`$[SystemRoot]\System32\cmd.exe" "/k cd > `"`$(FullPath)\cwd071.txt`"" $nonAscii "msg" "cmd" $nonAscii "" "" "$nonAscii\cwd071.txt"
if (-not $Only -or "L13" -match $Only) {
    $written = if (Test-Path -LiteralPath "$nonAscii\cwd071.txt") { [System.Text.Encoding]::GetEncoding(852).GetString([System.IO.File]::ReadAllBytes("$nonAscii\cwd071.txt")).Trim() } else { "<no file>" }
    $listing = (Get-ChildItem -LiteralPath $nonAscii -Force | Select-Object -ExpandProperty Name) -join ","
    Result "L13b-custom-fullpath-file" ((Norm $written) -eq (Norm $nonAscii)) ("cmd wrote its cwd as [" + $written + "]; dir=[" + $listing + "]; cmdline=[" + $script:lastCmdLine + "]")
}
Launch-Case "L14-custom-missing-E2"    5 "C:\nowhere\shell.exe" "" $plain "msg" "" "" "Error Starting Command Shell" "C:\nowhere\shell.exe"
Launch-Case "L15-pwsh7-notfound-E1"    2 "" "" $plain "msg" "" "" "Error Starting Command Shell" "PowerShell 7"

# ---- dialog cases ----
if (-not $Only -or "D00" -match $Only) {
    Set-Shell 3 "" ""
    $p = Start-App $plain
    $dlg = Open-Config $p "Configuration" "Command Shell"
    if ($dlg -eq [IntPtr]::Zero) { Result "D00-open-page" $false "Configuration dialog / Command Shell page not reachable" }
    else {
        Start-Sleep -Milliseconds 500
        Screenshot $dlg "$scratch\page-en.png"
        $combo = Ctl $dlg 6226; $foundAt = Ctl $dlg 6228; $prog = Ctl $dlg 6230; $args = Ctl $dlg 6233; $browse = Ctl $dlg 6231
        $items = 0..([W071]::ComboCount($combo) - 1) | ForEach-Object { [W071]::ComboItem($combo, $_) }
        Result "D01-combo-items" ($items.Count -eq 6 -and $items[2] -eq "PowerShell 7 (not found)" -and $items[3] -eq "Windows Terminal" -and $items[5] -eq "Custom program") ("items=[" + ($items -join " / ") + "]")
        $sel = [W071]::ComboSel($combo); $fa = [W071]::Text($foundAt)
        Result "D02-initial-selection-foundat" ($sel -eq 3 -and $fa -like "*\Microsoft\WindowsApps\wt.exe" -and -not [W071]::IsWindowEnabled($prog)) ("sel=$sel foundAt=[$fa] customEnabled=" + [W071]::IsWindowEnabled($prog))
        Page-Combo-Select $dlg 2
        $fa = [W071]::Text($foundAt)
        Result "D03-notfound-text" ($fa -eq "(not found on this computer)") ("foundAt=[$fa]")
        Dlg-OK $dlg
        $text = Dismiss-Box $p "^Error$" 5
        $still = Find-Win $p "^Configuration$" 2
        Result "D04-notfound-refused" (($text -match "not found on this computer") -and ($still -ne [IntPtr]::Zero)) ("box=[$text] dialogStillOpen=" + ($still -ne [IntPtr]::Zero))
        [W071]::SetText($prog, ""); [W071]::SetText($args, "")
        Page-Combo-Select $dlg 3; Page-Combo-Select $dlg 5
        $pv = [W071]::Text($prog); $av = [W071]::Text($args)
        Result "D05-prefill-from-wt" (($pv -like "*\wt.exe") -and ($av -eq "-d .") -and [W071]::IsWindowEnabled($prog) -and [W071]::IsWindowEnabled($browse)) ("program=[$pv] args=[$av] enabled=" + [W071]::IsWindowEnabled($prog))
        Screenshot $dlg "$scratch\page-en-custom.png"
        [W071]::SetText($prog, "X:\my own.exe"); [W071]::SetText($args, "--flag")
        Page-Combo-Select $dlg 1; Page-Combo-Select $dlg 5
        $pv = [W071]::Text($prog); $av = [W071]::Text($args)
        Result "D06-custom-text-kept" (($pv -eq "X:\my own.exe") -and ($av -eq "--flag")) ("program=[$pv] args=[$av]")
        [W071]::SetText($prog, "   ")
        Dlg-OK $dlg
        $text = Dismiss-Box $p "^Error$" 5
        Result "D07-blank-program-refused" ($text -match "Enter the program") ("box=[$text]")
        [W071]::SetText($prog, "C:\x\`$(Bogus)\a.exe")
        Dlg-OK $dlg
        $text = Dismiss-Box $p "^Error$" 5
        $still = Find-Win $p "^Configuration$" 2
        Result "D08-bad-placeholder-refused" (($null -ne $text) -and ($still -ne [IntPtr]::Zero)) ("box=[$text]")
        [W071]::ClickButton([W071]::GetParent($browse), $browse); Start-Sleep -Milliseconds 1500
        $fd = Find-Win $p "^Select Command Shell Program$" 5
        Result "D09-browse-dialog" ($fd -ne [IntPtr]::Zero) ("open-file dialog found=" + ($fd -ne [IntPtr]::Zero))
        if ($fd -ne [IntPtr]::Zero) { [W071]::Command($fd, 2); Start-Sleep -Milliseconds 800 }
        [W071]::SetText($prog, "`$[SystemRoot]\System32\cmd.exe"); [W071]::SetText($args, "/k echo `$(FullPath)")
        Dlg-OK $dlg
        $still = Find-Win $p "^Configuration$" 1
        Result "D10-valid-custom-accepted" ($still -eq [IntPtr]::Zero) ("dialog closed=" + ($still -eq [IntPtr]::Zero))
        $dlg = Open-Config $p "Configuration" "Command Shell"
        $sel = [W071]::ComboSel((Ctl $dlg 6226)); $pv = [W071]::Text((Ctl $dlg 6230)); $av = [W071]::Text((Ctl $dlg 6233))
        Result "D11-reopen-shows-custom" (($sel -eq 5) -and ($pv -eq "`$[SystemRoot]\System32\cmd.exe") -and ($av -eq "/k echo `$(FullPath)")) ("sel=$sel program=[$pv] args=[$av]")
        Page-Combo-Select $dlg 0
        Dlg-Cancel $dlg
        $dlg = Open-Config $p "Configuration" "Command Shell"
        $sel = [W071]::ComboSel((Ctl $dlg 6226))
        Result "D12-cancel-keeps-previous" ($sel -eq 5) ("sel after Cancel+reopen=$sel")
        Dlg-Cancel $dlg
    }
    Stop-App $p
    $saved = Get-ItemProperty -Path $regKey
    Result "D13-persisted-on-exit" (($saved."Command Shell Preset" -eq 5) -and ($saved."Command Shell Program" -eq "`$[SystemRoot]\System32\cmd.exe")) ("registry preset=" + $saved."Command Shell Preset" + " program=[" + $saved."Command Shell Program" + "] args=[" + $saved."Command Shell Arguments" + "]")
    $p = Start-App $plain
    $since = Get-Date
    Trigger $p "msg"   # the Num / keystroke path is proven by L02; here the point is the persisted Custom setting
    $procs = New-Shells $since 8
    $c = $procs | Where-Object { $_.ProcessName -eq "cmd" } | Select-Object -First 1
    Result "D14-restart-custom-launch" (($null -ne $c) -and ((Norm (Cwd-Of $c)) -eq (Norm $plain))) (Describe $procs)
    Kill-Shells $since
    Stop-App $p
}

# ---- Czech look ----
if (-not $Only -or "CZ01" -match $Only) {
    Set-ItemProperty -Path $regKey -Name "Language" -Value "czech.slg" -Type String
    Set-Shell 3 "" ""
    $p = Start-App $plain
    $dlg = Open-Config $p "Konfigurace" "Příkazový řádek"
    if ($dlg -ne [IntPtr]::Zero) {
        Start-Sleep -Milliseconds 500
        Screenshot $dlg "$scratch\page-cs.png"
        $combo = Ctl $dlg 6226
        $items = 0..([W071]::ComboCount($combo) - 1) | ForEach-Object { [W071]::ComboItem($combo, $_) }
        Result "CZ01-page-czech" ($items[0] -eq "Příkazový řádek" -and $items[2] -eq "PowerShell 7 (nenalezeno)" -and $items[5] -eq "Vlastní program") ("items=[" + ($items -join " / ") + "]")
        Page-Combo-Select $dlg 2
        Dlg-OK $dlg
        $text = Dismiss-Box $p "^Chyba$" 5
        Result "CZ02-czech-message" ($text -match "nebyl na tomto počítači nalezen") ("box=[$text]")
        Dlg-Cancel (Find-Win $p "^Konfigurace$" 2)
    } else { Result "CZ01-page-czech" $false "dialog not reachable" }
    Stop-App $p
}

# ---- restore ----
Set-ItemProperty -Path $regKey -Name "Language" -Value $origLang -Type String
Set-Shell $origPreset $origProgram $origArgs
Log ("restored: preset=$origPreset program=[$origProgram] args=[$origArgs] language=$origLang")
""
$script:results | Format-Table -AutoSize | Out-String -Width 300
"PASS: " + @($script:results | Where-Object Result -eq "PASS").Count + "  FAIL: " + @($script:results | Where-Object Result -eq "FAIL").Count

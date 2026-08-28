# GUI verification - last result of every case (2026-08-28, runs 2-8 in chronological order)

`D00-open-page` (logged only when the page could not be reached, runs 2-3) is superseded by D01-D14 passing in run 8 and is omitted.

| Case | Result | Detail (truncated) | Run |
|---|---|---|---|
| CZ01-page-czech | PASS | items=[P��kazov� ��dek / Windows PowerShell / PowerShell 7 (nenalezeno) / Termin�l Windows / Git Bash / Vlastn� program] | gui-run7.txt |
| CZ02-czech-message | PASS | box=[Vybran� program nebyl na tomto po��ta�i nalezen. Vyberte jin� nebo zvolte Vlastn� program a zadejte jeho cestu.] | gui-run7.txt |
| D01-combo-items | PASS | items=[Command Prompt / Windows PowerShell / PowerShell 7 (not found) / Windows Terminal / Git Bash / Custom program] | gui-run8.txt |
| D02-initial-selection-foundat | PASS | sel=3 foundAt=[C:\Users\pavel\AppData\Local\Microsoft\WindowsApps\wt.exe] customEnabled=False | gui-run8.txt |
| D03-notfound-text | PASS | foundAt=[(not found on this computer)] | gui-run8.txt |
| D04-notfound-refused | PASS | box=[The selected program was not found on this computer. Choose another one, or select Custom program and enter its path.] dialogStillOpen=True | gui-run8.txt |
| D05-prefill-from-wt | PASS | program=[C:\Users\pavel\AppData\Local\Microsoft\WindowsApps\wt.exe] args=[-d .] enabled=True | gui-run8.txt |
| D06-custom-text-kept | PASS | program=[X:\my own.exe] args=[--flag] | gui-run8.txt |
| D07-blank-program-refused | PASS | box=[Enter the program to run as the command shell.] | gui-run8.txt |
| D08-bad-placeholder-refused | PASS | box=[Variable "Bogus" was not found.] | gui-run8.txt |
| D09-browse-dialog | PASS | open-file dialog found=True | gui-run8.txt |
| D10-valid-custom-accepted | PASS | dialog closed=True | gui-run8.txt |
| D11-reopen-shows-custom | PASS | sel=5 program=[$[SystemRoot]\System32\cmd.exe] args=[/k echo $(FullPath)] | gui-run8.txt |
| D12-cancel-keeps-previous | PASS | sel after Cancel+reopen=5 | gui-run8.txt |
| D13-persisted-on-exit | PASS | registry preset=5 program=[$[SystemRoot]\System32\cmd.exe] args=[/k echo $(FullPath)] | gui-run8.txt |
| D14-restart-custom-launch | PASS | cmd(38900) cwd=[E:\Projects\tandemcommander\temp\071test\plain\]; OpenConsole(33908) cwd=[C:\WINDOWS\system32\] | gui-run8.txt |
| L01-cmd-default-msg | PASS | via msg, panel=[plain - Tandem Commander 0.1.5 (x64) ST] -> cmd(28208) cwd=[E:\Projects\tandemcommander\temp\071test\plain\]; OpenConsole(18992) cwd=[C:\WINDOWS\system32\] | gui-run3.txt |
| L02-cmd-numdiv | PASS | via numdiv, panel=[plain - Tandem Commander 0.1.5 (x64) ST] -> cmd(43336) cwd=[E:\Projects\tandemcommander\temp\071test\plain\]; OpenConsole(32444) cwd=[C:\WINDOWS\system32\] | gui-run6.txt |
| L03-cmd-ctrlslash | PASS | via ctrlslash, panel=[plain - Tandem Commander 0.1.5 (x64) ST] -> cmd(10256) cwd=[E:\Projects\tandemcommander\temp\071test\plain\]; OpenConsole(35384) cwd=[C:\WINDOWS\system32\] | gui-run5.txt |
| L04-powershell | PASS | via msg, panel=[plain - Tandem Commander 0.1.5 (x64) ST] -> powershell(37428) cwd=[E:\Projects\tandemcommander\temp\071test\plain\]; OpenConsole(14184) cwd=[C:\WINDOWS\system32\] | gui-run2.log |
| L05-wt-defaultprofile | PASS | via msg, panel=[plain - Tandem Commander 0.1.5 (x64) ST] -> OpenConsole(38892) cwd=[C:\WINDOWS\system32\]; bash(38600) cwd=[E:\Projects\tandemcommander\temp\071test\plain\]; bash(37820) cwd=[E:\Projects\tandemco | gui-run3.txt |
| L06-gitbash | PASS | via msg, panel=[plain - Tandem Commander 0.1.5 (x64) ST] -> git-bash(28236) cwd=[E:\Projects\tandemcommander\temp\071test\plain\]; mintty(42860) cwd=[E:\Projects\tandemcommander\temp\071test\plain\]; bash(9864) cwd=[E:\P | gui-run2.log |
| L07-powershell-nonascii | PASS | via msg, panel=[Nov� projekt - Tandem Commander 0.1.5 (x64) ST] -> powershell(8888) cwd=[E:\Projects\tandemcommander\temp\071test\M�j disk\Nov� projekt\]; OpenConsole(41444) cwd=[C:\WINDOWS\system32\] | gui-run2.log |
| L08-gitbash-nonascii | PASS | via msg, panel=[Nov� projekt - Tandem Commander 0.1.5 (x64) ST] -> git-bash(2996) cwd=[E:\Projects\tandemcommander\temp\071test\M�j disk\Nov� projekt\]; mintty(28788) cwd=[E:\Projects\tandemcommander\temp\071test\M�j dis | gui-run2.log |
| L09-cmd-longpath-8dot3 | PASS | via msg, panel=[abcdefghij_klmnopqrst_uvwxyz_0123456789 - Tandem Commander 0.1.5 (x64) ST] -> cmd(8152) cwd=[C:\Users\pavel\AppData\Local\Temp\071long\ABCDEF~1\ABCDEF~1\ABCDEF~1\ABCDEF~1\ABCDEF~1\ABCDEF~1\]; Op | gui-run3.txt |
| L09b-longpath-no8dot3-E2 | PASS | box=[Cannot start the command shell program: | gui-run3.txt |
| L10-powershell-unc | PASS | via msg, panel=[plain - Tandem Commander 0.1.5 (x64) ST] -> powershell(27948) cwd=[\\localhost\E$\Projects\tandemcommander\temp\071test\plain\]; OpenConsole(8968) cwd=[C:\WINDOWS\system32\] | gui-run2.log |
| L11-cmd-unc-fallback | PASS | via msg, panel=[plain - Tandem Commander 0.1.5 (x64) ST] -> cmd(21780) cwd=[C:\Windows\]; OpenConsole(45136) cwd=[C:\WINDOWS\system32\] | gui-run3.txt |
| L12-cmd-archive-panel | PASS | via msg, panel=[test.zip - Tandem Commander 0.1.5 (x64) ST] -> cmd(10016) cwd=[E:\Projects\tandemcommander\temp\071test\plain\]; OpenConsole(34904) cwd=[C:\WINDOWS\system32\] | gui-run3.txt |
| L13-custom-fullpath | PASS | via msg, panel=[Nov� projekt - Tandem Commander 0.1.5 (x64) ST] -> cmd(45148) cwd=[E:\Projects\tandemcommander\temp\071test\M�j disk\Nov� projekt\]; OpenConsole(12188) cwd=[C:\WINDOWS\system32\] | gui-run8.txt |
| L13b-custom-fullpath-file | PASS | cmd wrote its cwd as [E:\Projects\tandemcommander\temp\071test\M�j disk\Nov� projekt]; dir=[cwd071.txt]; cmdline=[C:\WINDOWS\System32\cmd.exe /k cd > "E:\Projects\tandemcommander\temp\071test\M�j disk\Nov� projekt\cwd071 | gui-run8.txt |
| L14-custom-missing-E2 | PASS | box=[Cannot start the command shell program: | gui-run2.log |
| L15-pwsh7-notfound-E1 | PASS | box=[The selected command shell program (PowerShell 7) was not found on this computer. | gui-run3.txt |

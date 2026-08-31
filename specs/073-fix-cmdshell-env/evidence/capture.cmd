@echo off
setlocal
rem capture.cmd - one-click capture of the failing state (spec 073, User Story 2).
rem
rem   1. Reproduce the failure in Tandem Commander (Enter on the .bat, or Num /
rem      and "npm run dev") and LEAVE that window open.
rem   2. Double-click THIS file in Windows Explorer (not from Tandem Commander,
rem      not from a terminal): the "ref-explorer" file must come from a
rem      Command Prompt that Explorer started - that is the working reference.
rem
rem Writes three files next to this script, all read-only captures:
rem   tc-live-<stamp>.txt      Tandem Commander's environment vs Explorer's
rem   tc-tree-<stamp>.txt      every process under Tandem Commander: cmdline, cwd, env diff
rem   ref-explorer-<stamp>.txt this (Explorer-started) cmd: whoami, where node/npm, set
rem "capture.cmd nopause" skips the final pause (for automated runs).
rem System tools are called by full path: Git for Windows puts its own find,
rem whoami etc. on PATH and they would be picked up instead.

set "SYS=%SystemRoot%\System32"
cd /d "%~dp0"
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%i"

echo.
echo === Tandem Commander environment capture (%STAMP%) ===
echo.
"%SYS%\tasklist.exe" /fi "imagename eq tandemcommander.exe" | "%SYS%\findstr.exe" /i "tandemcommander.exe" >nul
if errorlevel 1 (
  echo !!! tandemcommander.exe is NOT running. Start it from the Start menu, reproduce
  echo !!! the failure, leave that window open, then run this file again.
  goto :end
)

echo [1/3] Tandem Commander vs Explorer ...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0penv.ps1" > "%~dp0tc-live-%STAMP%.txt" 2>&1
echo [2/3] process tree under Tandem Commander ...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0treeenv.ps1" > "%~dp0tc-tree-%STAMP%.txt" 2>&1
echo [3/3] reference from this (Explorer-started) Command Prompt ...
(
  echo [whoami]
  "%SYS%\whoami.exe"
  echo [cd]
  cd
  echo [where node]
  "%SYS%\where.exe" node
  echo [where npm]
  "%SYS%\where.exe" npm
  echo [set]
  set
) > "%~dp0ref-explorer-%STAMP%.txt" 2>&1

echo.
echo Done. Files written to:
echo   %~dp0
echo     tc-live-%STAMP%.txt
echo     tc-tree-%STAMP%.txt
echo     ref-explorer-%STAMP%.txt
echo.
echo --- quick look: Tandem Commander vs Explorer ---
"%SYS%\findstr.exe" /b /c:"  only in" /c:"  IDENTICAL" /c:"  PATH differs" /c:"  ERROR" "%~dp0tc-live-%STAMP%.txt"
echo.

:end
if /i not "%~1"=="nopause" pause
endlocal

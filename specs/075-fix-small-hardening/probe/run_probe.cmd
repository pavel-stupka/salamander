@echo off
:: SPDX-FileCopyrightText: 2026 Pavel Stupka
:: SPDX-License-Identifier: GPL-2.0-or-later
::
:: Compiles and runs the feature 075 evidence probe with the product's own
:: Debug switches. Not part of any build; nothing here ships.
::
::     specs\075-fix-small-hardening\probe\run_probe.cmd
::
setlocal
set HERE=%~dp0
set OUT=%TEMP%\tc075probe

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSPATH=%%i
if not defined VSPATH (
    echo ERROR: Visual Studio with the C++ toolset was not found.
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

if not exist "%OUT%" mkdir "%OUT%"
:: /J unsigned char, /RTC1 runtime checks, /Od no optimization, /MDd debug CRT
:: -- the same switches src\vcxproj\sal_debug.props gives the product
cl /nologo /J /RTC1 /Od /MDd /EHa /W3 /D_CRT_SECURE_NO_WARNINGS ^
   /Fe"%OUT%\probe.exe" /Fo"%OUT%\\" "%HERE%probe.cpp" user32.lib || exit /b 1

echo.
"%OUT%\probe.exe"
exit /b %ERRORLEVEL%

@echo off
:: SPDX-FileCopyrightText: 2026 Pavel Stupka
:: SPDX-License-Identifier: GPL-2.0-or-later
::
:: Every automated check the codeview plugin has. None of them needs the
:: application, a GUI or a build -- they run against the committed sources, so
:: they belong in any pre-commit or pre-release sweep.
::
::     src\plugins\codeview\test\run_tests.cmd
::
setlocal
set REPO=%~dp0..\..\..\..
set FAILED=0

echo ============================================================
echo  codeview: data harness (tables, masks, assets)
echo ============================================================
python "%REPO%\src\plugins\codeview\test\check_data.py"
if errorlevel 1 set FAILED=1

echo.
echo ============================================================
echo  codeview: tokenizer worker (web\worker.js)
echo ============================================================
node "%REPO%\src\plugins\codeview\test\harness\test_worker.mjs"
if errorlevel 1 set FAILED=1

echo.
echo ============================================================
echo  codeview: page logic (web\viewer.js, DOM-free parts)
echo ============================================================
node "%REPO%\src\plugins\codeview\test\harness\test_page.mjs"
if errorlevel 1 set FAILED=1

echo.
echo ============================================================
if "%FAILED%"=="1" (
    echo  RESULT: FAILURES -- see above
    exit /b 1
)
echo  RESULT: all codeview checks passed
exit /b 0

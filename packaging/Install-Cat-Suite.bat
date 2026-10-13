@echo off
setlocal
title Cat Suite installer
rem Installs every Cat Suite plug-in (the .vst3 folders next to this file) into your own
rem VST3 folder:  %LOCALAPPDATA%\Programs\Common\VST3   (no administrator rights needed)
cd /d "%~dp0"
set "DEST=%LOCALAPPDATA%\Programs\Common\VST3"

echo.
echo   =====  Cat Suite installer  =====
echo.

:checkCubase
tasklist /FI "IMAGENAME eq Cubase*" 2>nul | find /I "Cubase" >nul
if not errorlevel 1 (
    echo   Cubase is still open. Please close Cubase, then press any key to continue.
    pause >nul
    goto checkCubase
)

if not exist "%DEST%" mkdir "%DEST%"
set FAILED=0
for /D %%P in ("VST3\*.vst3") do (
    echo   Installing %%~nP ...
    xcopy /E /I /Y /Q "%%P" "%DEST%\%%~nxP" >nul
    if errorlevel 1 (
        echo     COULD NOT COPY %%~nP
        set FAILED=1
    )
)

echo.
if "%FAILED%"=="1" (
    echo   Some plug-ins could not be installed. Close Cubase and run this again.
) else (
    echo   Done! All Cats are installed in:
    echo   %DEST%
    echo.
    echo   In Cubase: Studio ^> VST Plug-in Manager ^> Rescan.
)
echo.
pause

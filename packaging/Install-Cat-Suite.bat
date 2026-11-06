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
rem Micro-Cat's MTS-ESP mode needs ODDSound's free MTS-ESP library (one tuning for every MTS-ESP synth)
if exist "%CommonProgramFiles%\MTS-ESP\LIBMTS.dll" goto mtsDone
if not exist "MTS-ESP\libMTSWin_v1.03.exe" goto mtsDone
echo.
echo   Micro-Cat can tune every MTS-ESP synth at once (Surge XT, Vital, Pigments, Serum 2 ...).
echo   For that it needs the free MTS-ESP library by ODDSound (Windows asks for permission).
choice /C YN /M "  Install the MTS-ESP library now"
if errorlevel 2 goto mtsDone
start "" /wait "MTS-ESP\libMTSWin_v1.03.exe"
if exist "%CommonProgramFiles%\MTS-ESP\LIBMTS.dll" (echo   MTS-ESP library installed.) else (echo   MTS-ESP was not installed - you can run MTS-ESP\libMTSWin_v1.03.exe later.)
:mtsDone
echo.
pause

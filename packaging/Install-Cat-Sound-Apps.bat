@echo off
setlocal
title Cat Suite - sound apps installer
rem Installs the Cat Suite instruments that make sound:
rem   - as standalone apps   (%LOCALAPPDATA%\Programs\Cat Suite, with Start menu + Desktop shortcuts)
rem   - as VST3 plug-ins     (%LOCALAPPDATA%\Programs\Common\VST3, for Cubase)
rem No administrator rights are needed.
cd /d "%~dp0"
set "APPS=%LOCALAPPDATA%\Programs\Cat Suite"
set "VST=%LOCALAPPDATA%\Programs\Common\VST3"

echo.
echo   =====  Cat Suite - sound apps  =====
echo   Sing-Cat, Talk-Cat, Glide-Cat, Life-Cat, Muse-Cat and the Cat Hub
echo.

:checkRunning
set "RUNNING="
for %%A in (Sing-Cat Talk-Cat Glide-Cat Life-Cat Muse-Cat Cat-Hub) do (
    tasklist /FI "IMAGENAME eq %%A.exe" 2>nul | find /I "%%A.exe" >nul && set "RUNNING=%%A"
)
tasklist /FI "IMAGENAME eq Cubase*" 2>nul | find /I "Cubase" >nul && set "RUNNING=Cubase"
if defined RUNNING (
    echo   %RUNNING% is still open. Please close it, then press any key to continue.
    pause >nul
    goto checkRunning
)

if not exist "%APPS%" mkdir "%APPS%"
if not exist "%VST%" mkdir "%VST%"
set FAILED=0

for %%F in ("Apps\*.exe") do (
    echo   Installing app %%~nF ...
    copy /Y "%%F" "%APPS%\" >nul || set FAILED=1
)
if exist "Hub Cats" (
    echo   Installing the Cats for the Cat Hub ...
    if not exist "%APPS%\Hub Cats" mkdir "%APPS%\Hub Cats"
    for /D %%P in ("Hub Cats\*.vst3") do (
        xcopy /E /I /Y /Q "%%P" "%APPS%\Hub Cats\%%~nxP" >nul || set FAILED=1
    )
)
for /D %%P in ("VST3\*.vst3") do (
    echo   Installing plug-in %%~nP ...
    xcopy /E /I /Y /Q "%%P" "%VST%\%%~nxP" >nul || set FAILED=1
)

echo   Creating shortcuts in the Start menu and on the Desktop ...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ws = New-Object -ComObject WScript.Shell; $dir = $env:APPS; $menu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Cat Suite'; New-Item -ItemType Directory -Force $menu | Out-Null; $desk = [Environment]::GetFolderPath('Desktop'); Get-ChildItem -LiteralPath $dir -Filter *.exe | ForEach-Object { foreach ($d in @($menu, $desk)) { $s = $ws.CreateShortcut((Join-Path $d ($_.BaseName + '.lnk'))); $s.TargetPath = $_.FullName; $s.WorkingDirectory = $dir; $s.Save() } }"
if errorlevel 1 echo     (could not create the shortcuts - the apps are in "%APPS%")

echo.
if "%FAILED%"=="1" (
    echo   Something could not be copied. Close all Cat apps and Cubase, then run this again.
) else (
    echo   Done! Start Sing-Cat, Talk-Cat, Glide-Cat, Life-Cat, Muse-Cat or the Cat Hub from the Start menu or your Desktop.
    echo.
    echo   Cat Hub: Chord-Cat, Bass-Cat, Drum-Cat and Life-Cat play together - just press Play.
    echo       Space = play / stop       keys 1-8 = hold a chord pad
    echo.
    echo   Play them with your computer keyboard:
    echo       A S D F G H J K L  = white keys      W E T Y U O P = black keys
    echo       Left / Right arrow = octave          Down / Up arrow = velocity
    echo   Sound card or a MIDI keyboard: click "Options" ^(top left^) ^> Audio/MIDI Settings.
    echo.
    echo   The same instruments are also installed as VST3 plug-ins for Cubase
    echo   ^(Studio ^> VST Plug-in Manager ^> Rescan^).
)
echo.
pause

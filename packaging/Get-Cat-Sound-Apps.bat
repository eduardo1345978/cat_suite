@echo off
setlocal
title Cat Suite - get the sound apps
rem Downloads ONLY the Cat Suite instruments that make sound (Sing-Cat, Talk-Cat, Glide-Cat,
rem Life-Cat) from the newest GitHub build and installs them as apps + VST3 plug-ins.
set "URL=https://github.com/eduardo1345978/cat_suite/releases/download/latest/Cat-Sound-Apps-Windows.zip"
set "WORK=%TEMP%\CatSoundApps"

echo.
echo   =====  Cat Suite - get the sound apps  =====
echo.
if exist "%WORK%" rmdir /S /Q "%WORK%"
mkdir "%WORK%"

echo   Downloading the newest build from GitHub ...
curl.exe -L --fail --progress-bar -o "%WORK%\apps.zip" "%URL%"
if errorlevel 1 (
    echo   curl did not work, trying PowerShell ...
    powershell -NoProfile -Command "[Net.ServicePointManager]::SecurityProtocol = 'Tls12'; Invoke-WebRequest -UseBasicParsing -Uri $env:URL -OutFile (Join-Path $env:WORK 'apps.zip')"
)
if not exist "%WORK%\apps.zip" (
    echo.
    echo   The download failed. Check your internet connection and try again, or download
    echo   Cat-Sound-Apps-Windows.zip yourself from:
    echo   https://github.com/eduardo1345978/cat_suite/releases
    echo.
    pause
    exit /b 1
)

echo   Unpacking ...
powershell -NoProfile -Command "Expand-Archive -LiteralPath (Join-Path $env:WORK 'apps.zip') -DestinationPath $env:WORK -Force"
if not exist "%WORK%\Install-Cat-Sound-Apps.bat" (
    echo   Could not unpack the download. Please try again.
    pause
    exit /b 1
)

call "%WORK%\Install-Cat-Sound-Apps.bat"

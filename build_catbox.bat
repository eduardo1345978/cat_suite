@echo off
rem Builds only Cat Box (the groovebox: Bubny / Arp / Bass), runs its self test and starts it.
rem Log: build_catbox_log.txt next to this script.
setlocal
cd /d "%~dp0"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"
set "LOG=%~dp0build_catbox_log.txt"

echo Building Cat Box - the first time this takes a few minutes. Log: %LOG%
echo === Configure === > "%LOG%"
"%CMAKE%" -B build-box -A x64 -DCAT_BOX_ONLY=ON >> "%LOG%" 2>&1 || goto :fail
echo === Build === >> "%LOG%"
"%CMAKE%" --build build-box --config Release --target CatBox >> "%LOG%" 2>&1 || goto :fail

set "EXE=%~dp0build-box\CatBox\CatBox_artefacts\Release\Cat-Box.exe"
echo === Self test === >> "%LOG%"
echo Running the self test...
start "" /wait "%EXE%" --selftest "%~dp0build-box\catbox-test.wav"
if errorlevel 1 goto :fail
type "%~dp0build-box\catbox-test.txt" >> "%LOG%"
echo === DONE === >> "%LOG%"
echo Done. Starting Cat Box...
start "" "%EXE%"
exit /b 0

:fail
echo === FAILED === >> "%LOG%"
type "%~dp0build-box\catbox-test.txt" 2>nul
echo.
echo FAILED - see build_catbox_log.txt
pause
exit /b 1

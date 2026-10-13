@echo off
rem Builds the Cat Suite (Release), runs the headless host test and installs the
rem VST3 bundles into your user VST3 folder (no administrator rights needed).
rem Everything is logged to build_log.txt next to this script.
setlocal
cd /d "%~dp0"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"
set "LOG=%~dp0build_log.txt"

echo Building the Cat Suite - this takes a few minutes. Log: %LOG%
echo === Configure === > "%LOG%"
"%CMAKE%" -B build -A x64 -DCAT_BUILD_TESTS=ON >> "%LOG%" 2>&1 || goto :fail

echo === Build === >> "%LOG%"
echo Compiling...
"%CMAKE%" --build build --config Release --parallel >> "%LOG%" 2>&1 || goto :fail

echo === Tests === >> "%LOG%"
echo Running tests...
"build\Tests\CatSuiteHostTest_artefacts\Release\CatSuiteHostTest.exe" >> "%LOG%" 2>&1 || goto :fail

echo === Install === >> "%LOG%"
echo Installing to your user VST3 folder...
call "%~dp0install_vst3.bat"
type "%~dp0install_log.txt" >> "%LOG%" 2>nul
echo === DONE === >> "%LOG%"
echo.
echo Done. Rescan in Cubase: Studio ^> VST Plug-in Manager.
pause
exit /b 0

:fail
echo === FAILED === >> "%LOG%"
echo.
echo FAILED - see build_log.txt
pause
exit /b 1

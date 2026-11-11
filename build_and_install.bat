@echo off
rem Builds the Cat Suite (Release), runs the headless host test and installs the
rem VST3 bundles into your user VST3 folder (no administrator rights needed).
rem Everything is logged to build_log.txt next to this script.
setlocal
cd /d "%~dp0"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"
set "LOG=%~dp0build_log.txt"

rem How much of the computer to use: build_and_install.bat 50  (or pick below)
set "PCT=%~1"
if not "%PCT%"=="" goto :havepct
echo How much CPU should the build use? With less you can keep using the PC.
echo   1 = 25%%    2 = 50%%    3 = 75%%    4 = 100%% (fastest)
choice /c 1234 /n /t 15 /d 2 /m "Pick 1-4 (after 15 s it takes 2 = 50%%): "
set "PCT=25"
if errorlevel 2 set "PCT=50"
if errorlevel 3 set "PCT=75"
if errorlevel 4 set "PCT=100"
:havepct
rem only numbers 1..100
set /a PCT=%PCT% 2>nul
if %PCT% LSS 1 set "PCT=50"
if %PCT% GTR 100 set "PCT=100"
set /a JOBS=%NUMBER_OF_PROCESSORS% * %PCT% / 100
if %JOBS% LSS 1 set "JOBS=1"
echo Using %JOBS% of %NUMBER_OF_PROCESSORS% CPU threads (%PCT%%%), at low priority.

echo Building the Cat Suite - this takes a few minutes. Log: %LOG%
echo === Configure === > "%LOG%"
"%CMAKE%" -B build -A x64 -DCAT_BUILD_TESTS=ON >> "%LOG%" 2>&1 || goto :fail

echo === Build === >> "%LOG%"
echo Compiling...
rem JOBS files at once: MSBuild builds JOBS projects side by side and _CL_=/MP1 makes each compiler use one
rem thread (JUCE asks for /MP = every core, the _CL_ options come last and win); low priority keeps the PC usable
set "_CL_=/MP1"
start "" /b /wait /belownormal "%CMAKE%" --build build --config Release --parallel %JOBS% >> "%LOG%" 2>&1
if errorlevel 1 goto :fail

echo === Tests === >> "%LOG%"
echo Running tests...
"build\Tests\CatSuiteHostTest_artefacts\Release\CatSuiteHostTest.exe" >> "%LOG%" 2>&1 || goto :fail

echo === Install === >> "%LOG%"
echo Installing to your user VST3 folder...
call "%~dp0install_vst3.bat"
type "%~dp0install_log.txt" >> "%LOG%" 2>nul
echo === DONE === >> "%LOG%"
echo.
echo Done. Rescan the plug-ins in your DAW.
pause
exit /b 0

:fail
echo === FAILED === >> "%LOG%"
echo.
echo FAILED - see build_log.txt
pause
exit /b 1

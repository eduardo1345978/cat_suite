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
if "%PCT%"=="" (
    echo How much CPU should the build use? You can keep using the PC with less.
    echo   1 = 25%%   2 = 50%%   3 = 75%%   4 = 100%% ^(fastest^)
    choice /c 1234 /n /t 15 /d 2 /m "Pick 1-4 (in 15 s it takes 2 = 50%%): "
    if errorlevel 4 (set "PCT=100") else if errorlevel 3 (set "PCT=75") else if errorlevel 2 (set "PCT=50") else (set "PCT=25")
)
set /a JOBS=%NUMBER_OF_PROCESSORS% * %PCT% / 100
if %JOBS% LSS 1 set "JOBS=1"
echo Using %JOBS% of %NUMBER_OF_PROCESSORS% CPU threads (%PCT%%%), at low priority.

echo Building the Cat Suite - this takes a few minutes. Log: %LOG%
echo === Configure === > "%LOG%"
"%CMAKE%" -B build -A x64 -DCAT_BUILD_TESTS=ON >> "%LOG%" 2>&1 || goto :fail

echo === Build === >> "%LOG%"
echo Compiling...
rem low priority: the PC stays usable; -- /m:N limits MSBuild, /p:CL_MPCount=1 stops each compiler from taking every core
start "" /b /wait /belownormal "%CMAKE%" --build build --config Release --parallel %JOBS% -- /p:CL_MPCount=1 >> "%LOG%" 2>&1
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

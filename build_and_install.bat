@echo off
rem Builds the Cat Suite (Release), runs the host test and installs the VST3 plug-ins into your user VST3
rem folder (no administrator rights). Asks how much of the PC to use and whether to build the Pro versions;
rem remembers the answers (build_settings.txt). Everything is logged to build_log.txt.
rem   build_and_install.bat            ask (15 s, then the last answers)
rem   build_and_install.bat 50 nopro   no questions: 50 %% of the CPU, without the Pro versions
setlocal EnableDelayedExpansion
cd /d "%~dp0"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"
set "LOG=%~dp0build_log.txt"
set "SETTINGS=%~dp0build_settings.txt"

rem colours (Windows 10+)
for /f %%a in ('echo prompt $E^| cmd') do set "E=%%a"
set "C0=%E%[0m" & set "CT=%E%[38;5;215m" & set "CD=%E%[38;5;245m" & set "CG=%E%[38;5;114m" & set "CR=%E%[38;5;203m" & set "CY=%E%[38;5;222m"

echo.
echo %CT%      /\_/\    %C0%  CAT SUITE  -  build ^& install
echo %CT%     ( o.o )   %C0%%CD%  every Cat, compiled on your PC%C0%
echo %CT%      ^> ^^ ^<    %C0%
echo.

rem ---- the last answers
set "PCT=50" & set "PRO=no"
if exist "%SETTINGS%" for /f "usebackq tokens=1,2 delims==" %%a in ("%SETTINGS%") do set "%%a=%%b"

if not "%~1"=="" (
    set "PCT=%~1"
    if /i "%~2"=="nopro" (set "PRO=no") else if /i "%~2"=="pro" (set "PRO=yes")
    goto :answered
)

echo %CY%How much of the CPU?%C0%  %CD%less = you can keep using the PC, it just takes longer%C0%
echo    1 = 25%%    2 = 50%%    3 = 75%%    4 = 100%%  (fastest)
set "DEF=2"
if "%PCT%"=="25" set "DEF=1"
if "%PCT%"=="75" set "DEF=3"
if "%PCT%"=="100" set "DEF=4"
choice /c 1234 /n /t 15 /d %DEF% /m "   pick 1-4 (15 s, then %DEF%): "
set "PCT=25"
if errorlevel 2 set "PCT=50"
if errorlevel 3 set "PCT=75"
if errorlevel 4 set "PCT=100"
echo.
echo %CY%Build the Pro versions too?%C0%  %CD%Y = twice as long (every Cat again with the Pro panel), N = only the Cats%C0%
set "DEFP=N"
if /i "%PRO%"=="yes" set "DEFP=Y"
choice /c YN /n /t 15 /d %DEFP% /m "   Y / N (15 s, then %DEFP%): "
if errorlevel 2 (set "PRO=no") else (set "PRO=yes")

:answered
set /a PCT=%PCT% 2>nul
if %PCT% LSS 1 set "PCT=50"
if %PCT% GTR 100 set "PCT=100"
set /a JOBS=%NUMBER_OF_PROCESSORS% * %PCT% / 100
if %JOBS% LSS 1 set "JOBS=1"
(echo PCT=%PCT%& echo PRO=%PRO%) > "%SETTINGS%"
set "PROFLAG=OFF"
if /i "%PRO%"=="yes" set "PROFLAG=ON"

echo.
echo %CD%  CPU:%C0% %JOBS% of %NUMBER_OF_PROCESSORS% threads (%PCT%%%), low priority   %CD%Pro versions:%C0% %PRO%
echo %CD%  log:%C0% %LOG%
echo.
for /f %%s in ('powershell -NoProfile -Command "[DateTimeOffset]::Now.ToUnixTimeSeconds()"') do set "T0=%%s"

echo === Configure === > "%LOG%"
echo %CT%[1/4]%C0% Preparing...
"%CMAKE%" -B build -A x64 -DCAT_BUILD_TESTS=ON -DCAT_BUILD_PRO=%PROFLAG% >> "%LOG%" 2>&1 || goto :fail

echo === Build === >> "%LOG%"
echo %CT%[2/4]%C0% Compiling...  %CD%(the first time takes long; next time only what changed)%C0%
rem JOBS files at once: MSBuild builds JOBS projects side by side and _CL_=/MP1 makes each compiler use one
rem thread (JUCE asks for /MP = every core; the _CL_ options come last and win). Low priority keeps the PC usable.
set "_CL_=/MP1"
start "" /b /wait /belownormal "%CMAKE%" --build build --config Release --parallel %JOBS% >> "%LOG%" 2>&1
if errorlevel 1 goto :fail
set "_CL_="

echo === Tests === >> "%LOG%"
echo %CT%[3/4]%C0% Testing every Cat in a pretend DAW...
"build\Tests\CatSuiteHostTest_artefacts\Release\CatSuiteHostTest.exe" >> "%LOG%" 2>&1 || goto :fail

echo === Install === >> "%LOG%"
echo %CT%[4/4]%C0% Installing...
call "%~dp0install_vst3.bat"
type "%~dp0install_log.txt" >> "%LOG%" 2>nul
echo === DONE === >> "%LOG%"

set /a COUNT=0
for /d %%d in ("%LOCALAPPDATA%\Programs\Common\VST3\*.vst3") do set /a COUNT+=1
call :elapsed
echo.
echo %CG%  Done in %MINS% min.%C0%  %COUNT% plug-ins are in your VST3 folder.
echo   Rescan the plug-ins in your DAW.
findstr /c:"NOT BUILT" /c:"COPY FAILED" "%~dp0install_log.txt" >nul 2>&1 && (echo %CY%  Some were not copied - see install_log.txt ^(is your DAW still open?^)%C0%)
echo.
powershell -NoProfile -Command "[console]::beep(660,120);[console]::beep(880,120);[console]::beep(1320,220)" >nul 2>&1
pause
exit /b 0

:fail
echo === FAILED === >> "%LOG%"
call :elapsed
echo.
echo %CR%  FAILED after %MINS% min.%C0%  The first errors:
powershell -NoProfile -Command "Get-Content -LiteralPath '%LOG%' | Select-String -Pattern ' error |error C|error LNK|FAIL' | Select-Object -First 8 | ForEach-Object { '   ' + $_.Line.Trim() }" 2>nul
echo   The whole log: build_log.txt  (send me the end of it)
powershell -NoProfile -Command "[console]::beep(440,300);[console]::beep(330,400)" >nul 2>&1
pause
exit /b 1

:elapsed
rem whole minutes since T0
set "T1=%T0%"
for /f %%s in ('powershell -NoProfile -Command "[DateTimeOffset]::Now.ToUnixTimeSeconds()"') do set "T1=%%s"
set /a MINS=(T1-T0+30)/60
exit /b 0

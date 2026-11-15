@echo off
rem Builds the Cat Suite (Release), runs the host test and installs the VST3 plug-ins into your user VST3
rem folder (no administrator rights). Asks how much of the PC to use and whether to build the Pro versions;
rem remembers the answers (build_settings.txt). Everything is logged to build_log.txt.
rem   build_and_install.bat            ask (15 s, then the last answers)
rem   build_and_install.bat 50 nopro   no questions: 50 %% of the CPU, without the Pro versions, every Cat
rem   build_and_install.bat 50 nopro GateCat   the same, only Gate-Cat
setlocal EnableDelayedExpansion
cd /d "%~dp0"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"
set "LOG=%~dp0build_log.txt"
set "SETTINGS=%~dp0build_settings.txt"

rem colours (Windows 10+). Not "CD": that name is the current folder, which Visual Studio's set-up reads
for /f %%a in ('echo prompt $E^| cmd') do set "E=%%a"
set "C0=%E%[0m" & set "CT=%E%[38;5;215m" & set "CM=%E%[38;5;245m" & set "CG=%E%[38;5;114m" & set "CR=%E%[38;5;203m" & set "CY=%E%[38;5;222m"

echo.
echo %CT%      /\_/\    %C0%  CAT SUITE  -  build ^& install
echo %CT%     ( o.o )   %C0%%CM%  every Cat, compiled on your PC%C0%
echo %CT%      ^> ^^ ^<    %C0%
echo.

rem ---- the newest code from GitHub (when this folder is a git clone)
if exist ".git\" (
    where git >nul 2>&1 && (
        echo %CM%Getting the newest code from GitHub...%C0%
        git pull --ff-only > "%TEMP%\cat_git.txt" 2>&1 && (findstr /c:"Already up to date" "%TEMP%\cat_git.txt" >nul && echo %CM%   already up to date%C0% || echo %CG%   updated%C0%) || (echo %CY%   could not update ^(changed files here?^) - building what is here%C0%)
        echo.
    )
)

rem ---- the last answers
set "PCT=50" & set "PRO=no" & set "WHAT=all"
if exist "%SETTINGS%" for /f "usebackq tokens=1,2 delims==" %%a in ("%SETTINGS%") do set "%%a=%%b"

if not "%~1"=="" (
    set "PCT=%~1"
    if /i "%~2"=="nopro" (set "PRO=no") else if /i "%~2"=="pro" (set "PRO=yes")
    set "WHAT=all"
    if not "%~3"=="" set "WHAT=%~3"
    goto :answered
)

echo %CY%How much of the CPU?%C0%  %CM%less = you can keep using the PC, it just takes longer%C0%
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
echo %CY%Build the Pro versions too?%C0%  %CM%Y = twice as long (every Cat again with the Pro panel), N = only the Cats%C0%
set "DEFP=N"
if /i "%PRO%"=="yes" set "DEFP=Y"
choice /c YN /n /t 15 /d %DEFP% /m "   Y / N (15 s, then %DEFP%): "
if errorlevel 2 (set "PRO=no") else (set "PRO=yes")
echo.
echo %CY%Which Cats?%C0%  %CM%only one = much faster when you are fixing one%C0%
echo    1 = all    2 = only the recommended (Chord, Patt, Orbit, Sing, Glide, Swarm)    3 = only one (type its name)
set "DEFW=1"
if /i "%WHAT%"=="recommended" set "DEFW=2"
choice /c 123 /n /t 15 /d %DEFW% /m "   pick 1-3 (15 s, then %DEFW%): "
if errorlevel 3 (
    set "ONE="
    set /p "ONE=   which one? (e.g. Gate-Cat, Crypt, Poly Board): "
    if "!ONE!"=="" (set "WHAT=all") else (set "WHAT=!ONE!")
) else if errorlevel 2 (set "WHAT=recommended") else (set "WHAT=all")

:answered
set /a PCT=%PCT% 2>nul
if %PCT% LSS 1 set "PCT=50"
if %PCT% GTR 100 set "PCT=100"
set /a JOBS=%NUMBER_OF_PROCESSORS% * %PCT% / 100
if %JOBS% LSS 1 set "JOBS=1"
set "SAVEW=all"
if /i "%WHAT%"=="recommended" set "SAVEW=recommended"
(echo PCT=%PCT%& echo PRO=%PRO%& echo WHAT=%SAVEW%) > "%SETTINGS%"
set "PROFLAG=OFF"
if /i "%PRO%"=="yes" set "PROFLAG=ON"

rem ---- which build targets: all = everything; else the Cats' VST3 targets (folder name without "-" and spaces)
set "TARGETS="
set "NAMES=%WHAT%"
if /i "%WHAT%"=="recommended" set "NAMES=ChordCat PattCat OrbitCat SingCat GlideCat Swarm"
if /i not "%WHAT%"=="all" (
    set "NAMES=!NAMES:-=!"
    if /i not "%WHAT%"=="recommended" set "NAMES=!NAMES: =!"
    for %%n in (!NAMES!) do (
        set "TARGETS=!TARGETS! %%n_VST3"
        if /i "%PRO%"=="yes" set "TARGETS=!TARGETS! %%nPro_VST3"
    )
)

echo.
echo %CM%  CPU:%C0% %JOBS% of %NUMBER_OF_PROCESSORS% threads (%PCT%%%), low priority   %CM%Pro versions:%C0% %PRO%   %CM%Cats:%C0% %WHAT%
echo %CM%  log:%C0% %LOG%
echo.
for /f %%s in ('powershell -NoProfile -Command "[DateTimeOffset]::Now.ToUnixTimeSeconds()"') do set "T0=%%s"

echo === Configure === > "%LOG%"
echo %CT%[1/4]%C0% Preparing...
rem Faster builds at home: Ninja (spreads single files over the cores, instead of whole projects) and no
rem link-time optimisation (CAT_LTO=OFF: the plug-ins are a few % slower, the build much faster).
rem Ninja and the compiler come with Visual Studio; vcvars64.bat puts them on the PATH.
set "GEN=-A x64"
set "VCVARS="
for %%v in ("C:\Program Files\Microsoft Visual Studio\18\Community" "C:\Program Files\Microsoft Visual Studio\2022\Community") do (
    if not defined VCVARS if exist "%%~v\VC\Auxiliary\Build\vcvars64.bat" if exist "%%~v\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" (
        set "VCVARS=%%~v\VC\Auxiliary\Build\vcvars64.bat"
        set "NINJA=%%~v\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    )
)
if defined VCVARS (
    call "!VCVARS!" >> "%LOG%" 2>&1
    set "GEN=-G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_MAKE_PROGRAM=!NINJA!""
    rem a build folder made by the other (Visual Studio) generator can't be reused: start it fresh, once
    if exist "build\CMakeCache.txt" findstr /c:"CMAKE_GENERATOR:INTERNAL=Visual Studio" "build\CMakeCache.txt" >nul && (
        echo %CM%   switching to the faster Ninja build: the first build starts from scratch%C0%
        rmdir /s /q build
    )
    echo %CM%   fast mode: Ninja, no link-time optimisation%C0%
) else (
    echo %CM%   Ninja not found - the normal Visual Studio build%C0%
)
"%CMAKE%" -B build !GEN! -DCAT_BUILD_TESTS=ON -DCAT_BUILD_PRO=%PROFLAG% -DCAT_LTO=OFF >> "%LOG%" 2>&1 || goto :fail

echo === Build === >> "%LOG%"
echo %CT%[2/4]%C0% Compiling...  %CM%(the first time takes long; next time only what changed)%C0%
rem JOBS files at once: MSBuild builds JOBS projects side by side and _CL_=/MP1 makes each compiler use one
rem thread (JUCE asks for /MP = every core; the _CL_ options come last and win). Low priority keeps the PC usable.
set "_CL_=/MP1"
rem the progress bar: tools\build_progress.ps1 runs the build (low priority) and counts the finished parts
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build_progress.ps1" -Cmake "%CMAKE%" -Jobs %JOBS% -Log "%LOG%" -Key "%SAVEW%-%PRO%" -Targets "!TARGETS!"
if errorlevel 1 goto :fail
set "_CL_="

echo === Tests === >> "%LOG%"
if /i not "%WHAT%"=="all" (
    echo %CT%[3/4]%C0% %CM%Tests skipped ^(they need every Cat - build all to run them^)%C0%
    goto :install
)
echo %CT%[3/4]%C0% Testing every Cat in a pretend DAW...
"build\Tests\CatSuiteHostTest_artefacts\Release\CatSuiteHostTest.exe" >> "%LOG%" 2>&1 || goto :fail

:install
rem a running DAW keeps the plug-in files locked: they could not be replaced
:checkdaw
set "DAW="
for %%x in (Cubase14.exe Cubase13.exe Cubase12.exe Cubase.exe "Cubase LE AI Elements 13.exe" FL64.exe "Ableton Live 12 Suite.exe" Reaper.exe "Cat-Hub.exe") do (
    tasklist /fi "imagename eq %%~x" 2>nul | find /i "%%~x" >nul && set "DAW=%%~x"
)
if defined DAW (
    echo %CY%  !DAW! is open - it keeps the plug-ins locked, so they can't be replaced.%C0%
    choice /c CS /n /m "   close it, then press C to check again  (S = skip, install anyway): "
    if not errorlevel 2 goto :checkdaw
)

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
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build_progress.ps1" -Notify "Cat Suite is ready" -Text "Built and installed in %MINS% min. Rescan the plug-ins in your DAW." >nul 2>&1
pause
exit /b 0

:fail
echo === FAILED === >> "%LOG%"
call :elapsed
echo.
echo %CR%  FAILED after %MINS% min.%C0%  The first errors:
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build_progress.ps1" -Errors -Log "%LOG%" 2>nul
echo   The whole log: build_log.txt  (send me the end of it)
powershell -NoProfile -Command "[console]::beep(440,300);[console]::beep(330,400)" >nul 2>&1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build_progress.ps1" -Notify "Cat Suite build FAILED" -Text "After %MINS% min. The errors are in the window." -Bad >nul 2>&1
pause
exit /b 1

:elapsed
rem whole minutes since T0
set "T1=%T0%"
for /f %%s in ('powershell -NoProfile -Command "[DateTimeOffset]::Now.ToUnixTimeSeconds()"') do set "T1=%%s"
set /a MINS=(T1-T0+30)/60
exit /b 0

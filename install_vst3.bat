@echo off
rem Installs everything that was built:
rem  - the VST3 plug-ins into the per-user VST3 folder that Cubase scans:
rem    %LOCALAPPDATA%\Programs\Common\VST3  (no administrator rights needed)
rem  - the apps (Cat Hub, Cat Ocean, Muse-Cat) into the "Cat Hub" folder next to this script,
rem    with a copy of the Cats in "Cat Hub\Hub Cats" for Cat Hub to load
setlocal
cd /d "%~dp0"
set "DEST=%LOCALAPPDATA%\Programs\Common\VST3"
set "APPS=%~dp0Cat Hub"
if not exist "%DEST%" mkdir "%DEST%"
if not exist "%APPS%\Hub Cats" mkdir "%APPS%\Hub Cats"
set "LOG=%~dp0install_log.txt"
echo Installing to %DEST% > "%LOG%"
for %%C in (Chord Patt Bend Drum Orbit Sing Bass Glide Talk Life Muse Echo) do call :plugin %%C
echo. >> "%LOG%"
echo Apps to %APPS% >> "%LOG%"
call :app CatHub Cat-Hub.exe
call :app CatOcean Cat-Ocean.exe
call :app MuseCat Standalone\Muse-Cat.exe
copy /Y "packaging\Cat-Ocean-YouTube-Description.txt" "%APPS%\" >> "%LOG%" 2>&1
echo INSTALL FINISHED >> "%LOG%"
exit /b 0

:plugin
set "SRC=build\%1Cat\%1Cat_artefacts\Release\VST3\%1-Cat.vst3"
if not exist "%SRC%" (echo NOT BUILT: %1-Cat >> "%LOG%" & exit /b 0)
xcopy /E /I /Y "%SRC%" "%DEST%\%1-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: %1-Cat (is Cubase still running?) >> "%LOG%"
if /I not "%1"=="Muse" if /I not "%1"=="Echo" xcopy /E /I /Y "%SRC%" "%APPS%\Hub Cats\%1-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Hub Cats\%1-Cat (is Cat Hub still running?) >> "%LOG%"
exit /b 0

:app
set "SRC=build\%1\%1_artefacts\Release\%2"
if not exist "%SRC%" (echo NOT BUILT: %2 >> "%LOG%" & exit /b 0)
copy /Y "%SRC%" "%APPS%\" >> "%LOG%" 2>&1 || echo COPY FAILED: %2 (is it still running?) >> "%LOG%"
exit /b 0

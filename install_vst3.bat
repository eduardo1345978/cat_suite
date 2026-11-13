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
for %%C in (Chord Patt Bend Drum Orbit Sing Bass Glide Talk Life Muse Echo Micro Phase Hum Glitch Meow) do call :plugin %%C
call :fx Molten
call :fx Swarm
call :fx Vortex
call :fx Negative
call :fx Crypt
set "SRC=build\CryptFX\CryptFX_artefacts\Release\VST3\Crypt-FX.vst3"
if exist "%SRC%" (xcopy /E /I /Y "%SRC%" "%DEST%\Crypt-FX.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Crypt-FX >> "%LOG%") else (echo NOT BUILT: Crypt-FX >> "%LOG%")
echo. >> "%LOG%"
echo Apps to %APPS% >> "%LOG%"
call :app CatHub Cat-Hub.exe
call :app CatOcean Cat-Ocean.exe
call :app MuseCat Standalone\Muse-Cat.exe
call :app Negative Standalone\Negative.exe
call :app PolyBoard Standalone\Poly-Board.exe
set "SRC=build\PolyBoard\PolyBoard_artefacts\Release\VST3\Poly-Board.vst3"
if exist "%SRC%" (xcopy /E /I /Y "%SRC%" "%DEST%\Poly-Board.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Poly-Board >> "%LOG%") else (echo NOT BUILT: Poly-Board >> "%LOG%")
call :app CatLoop Cat-Loop.exe
copy /Y "packaging\Cat-Ocean-YouTube-Description.txt" "%APPS%\" >> "%LOG%" 2>&1
echo INSTALL FINISHED >> "%LOG%"
rem the Pro versions, if they were built ("X Pro.vst3")
for /d %%D in (build\*) do for /d %%A in ("%%D\*Pro_artefacts") do for /d %%P in ("%%A\Release\VST3\*.vst3") do xcopy /E /I /Y "%%P" "%DEST%\%%~nxP" >> "%LOG%" 2>&1
exit /b 0


:plugin
set "SRC=build\%1Cat\%1Cat_artefacts\Release\VST3\%1-Cat.vst3"
if not exist "%SRC%" (echo NOT BUILT: %1-Cat >> "%LOG%" & exit /b 0)
xcopy /E /I /Y "%SRC%" "%DEST%\%1-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: %1-Cat (is your DAW still running?) >> "%LOG%"
if /I not "%1"=="Muse" if /I not "%1"=="Echo" if /I not "%1"=="Micro" if /I not "%1"=="Meow" xcopy /E /I /Y "%SRC%" "%APPS%\Hub Cats\%1-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Hub Cats\%1-Cat (is Cat Hub still running?) >> "%LOG%"
exit /b 0

:fx
rem an effect without "-Cat" in its name (Molten): only into the VST3 folder
set "SRC=build\%1\%1_artefacts\Release\VST3\%1.vst3"
if not exist "%SRC%" (echo NOT BUILT: %1 >> "%LOG%" & exit /b 0)
xcopy /E /I /Y "%SRC%" "%DEST%\%1.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: %1 (is your DAW still running?) >> "%LOG%"
exit /b 0

:app
set "SRC=build\%1\%1_artefacts\Release\%2"
if not exist "%SRC%" (echo NOT BUILT: %2 >> "%LOG%" & exit /b 0)
copy /Y "%SRC%" "%APPS%\" >> "%LOG%" 2>&1 || echo COPY FAILED: %2 (is it still running?) >> "%LOG%"
exit /b 0

@echo off
rem Copies the built VST3 bundles into the per-user VST3 folder that Cubase scans:
rem %LOCALAPPDATA%\Programs\Common\VST3  (no administrator rights needed)
cd /d "%~dp0"
set "DEST=%LOCALAPPDATA%\Programs\Common\VST3"
if not exist "%DEST%" mkdir "%DEST%"
set "LOG=%~dp0install_log.txt"
echo Installing to %DEST% > "%LOG%"
xcopy /E /I /Y "build\ChordCat\ChordCat_artefacts\Release\VST3\Chord-Cat.vst3" "%DEST%\Chord-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Chord-Cat (is Cubase still running?) >> "%LOG%"
xcopy /E /I /Y "build\PattCat\PattCat_artefacts\Release\VST3\Patt-Cat.vst3" "%DEST%\Patt-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Patt-Cat (is Cubase still running?) >> "%LOG%"
xcopy /E /I /Y "build\BendCat\BendCat_artefacts\Release\VST3\Bend-Cat.vst3" "%DEST%\Bend-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Bend-Cat (is Cubase still running?) >> "%LOG%"
xcopy /E /I /Y "build\DrumCat\DrumCat_artefacts\Release\VST3\Drum-Cat.vst3" "%DEST%\Drum-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Drum-Cat (is Cubase still running?) >> "%LOG%"
xcopy /E /I /Y "build\OrbitCat\OrbitCat_artefacts\Release\VST3\Orbit-Cat.vst3" "%DEST%\Orbit-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Orbit-Cat (is Cubase still running?) >> "%LOG%"
xcopy /E /I /Y "build\SingCat\SingCat_artefacts\Release\VST3\Sing-Cat.vst3" "%DEST%\Sing-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Sing-Cat (is Cubase still running?) >> "%LOG%"
xcopy /E /I /Y "build\BassCat\BassCat_artefacts\Release\VST3\Bass-Cat.vst3" "%DEST%\Bass-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Bass-Cat (is Cubase still running?) >> "%LOG%"
xcopy /E /I /Y "build\GlideCat\GlideCat_artefacts\Release\VST3\Glide-Cat.vst3" "%DEST%\Glide-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Glide-Cat (is Cubase still running?) >> "%LOG%"
xcopy /E /I /Y "build\TalkCat\TalkCat_artefacts\Release\VST3\Talk-Cat.vst3" "%DEST%\Talk-Cat.vst3" >> "%LOG%" 2>&1 || echo COPY FAILED: Talk-Cat (is Cubase still running?) >> "%LOG%"
echo INSTALL FINISHED >> "%LOG%"

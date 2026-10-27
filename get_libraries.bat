@echo off
rem Cat Box: downloads the free sample libraries into CatBox\library (about 6 GB, once).
rem   VSCO 2 Community Edition - orchestra (CC0, free for anything)   https://github.com/sgossner/VSCO-2-CE
rem   Dirt-Samples (TidalCycles) - drums, breaks, synths, voices        https://github.com/tidalcycles/Dirt-Samples
rem Cat Box finds them by itself the next time it starts (Sampler, Granular and Kit sounds, the browser's LIB folders).
setlocal
cd /d "%~dp0"
if not exist "CatBox\library" mkdir "CatBox\library"
where git >nul 2>nul
if not errorlevel 1 (
  rem with git: faster and it can continue where it stopped
  if not exist "CatBox\library\Dirt-Samples" git clone --depth 1 https://github.com/tidalcycles/Dirt-Samples "CatBox\library\Dirt-Samples"
  if not exist "CatBox\library\VSCO-2-CE" git clone --depth 1 https://github.com/sgossner/VSCO-2-CE "CatBox\library\VSCO-2-CE"
  if exist "CatBox\library\VSCO-2-CE\Strings" goto done
)
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference = 'Stop';" ^
  "$ProgressPreference = 'SilentlyContinue';" ^
  "$lib = Join-Path (Get-Location) 'CatBox\library';" ^
  "$packs = @(@{ name = 'Dirt-Samples'; url = 'https://codeload.github.com/tidalcycles/Dirt-Samples/zip/refs/heads/master' }, @{ name = 'VSCO-2-CE'; url = 'https://codeload.github.com/sgossner/VSCO-2-CE/zip/refs/heads/master' });" ^
  "foreach ($p in $packs) {" ^
  "  $dest = Join-Path $lib $p.name;" ^
  "  if (Test-Path $dest) { Write-Host ($p.name + ' is already there'); continue }" ^
  "  $zip = Join-Path $env:TEMP ($p.name + '.zip');" ^
  "  Write-Host ('Downloading ' + $p.name + ' ... (this takes a while)');" ^
  "  Invoke-WebRequest -Uri $p.url -OutFile $zip;" ^
  "  Write-Host ('Unpacking ' + $p.name + ' ...');" ^
  "  $tmp = Join-Path $lib ('_' + $p.name);" ^
  "  Expand-Archive -Path $zip -DestinationPath $tmp -Force;" ^
  "  $inner = Get-ChildItem $tmp | Select-Object -First 1;" ^
  "  Move-Item $inner.FullName $dest;" ^
  "  Remove-Item $tmp -Recurse -Force; Remove-Item $zip -Force;" ^
  "  Write-Host ($p.name + ' done') }"
if errorlevel 1 (
  echo.
  echo Something went wrong. Check the internet connection and run get_libraries.bat again.
  pause
  exit /b 1
)
:done
echo.
echo The libraries are in CatBox\library. Start Cat Box: SOUND, Sound again (browser), engine Sampler / Granular / Kit.
pause

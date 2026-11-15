# Runs the Cat Suite build and draws a progress bar while it compiles (used by build_and_install.bat).
# The build's output goes to a temporary file, the bar counts the projects MSBuild has finished
# ("Name.vcxproj -> ..." lines) against all the projects CMake made, and shows the file being compiled.
param([string]$Cmake, [int]$Jobs = 2, [string]$Log)

$ErrorActionPreference = 'Stop'
$out = Join-Path $env:TEMP ("cat_build_" + [guid]::NewGuid().ToString('N') + ".txt")
$err = $out + ".err"

# every project the build will make (not the helpers CMake adds to every solution)
$skip = 'ALL_BUILD|ZERO_CHECK|INSTALL|RUN_TESTS|PACKAGE|_rc_lib'
$total = @(Get-ChildItem -Path build -Recurse -Filter *.vcxproj -ErrorAction SilentlyContinue |
           Where-Object { $_.FullName -notmatch '\\_deps\\' -and $_.BaseName -notmatch $skip }).Count
if ($total -lt 1) { $total = 1 }

$p = Start-Process -FilePath $Cmake -ArgumentList @('--build', 'build', '--config', 'Release', '--parallel', "$Jobs") `
                   -NoNewWindow -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
$null = $p.Handle                     # keeps the exit code readable later (a PowerShell quirk)
try { $p.PriorityClass = 'BelowNormal' } catch {}

$start = Get-Date
$width = 30
$seen = @{}
$file = ''
while (-not $p.HasExited) {
    Start-Sleep -Milliseconds 700
    if (Test-Path $out) {
        $lines = Get-Content -LiteralPath $out -ErrorAction SilentlyContinue
        foreach ($l in $lines) {
            if ($l -match '^\s*(\S+)\.vcxproj -> ') { $seen[$Matches[1]] = 1 }
            elseif ($l -match '^\s+([\w\-]+\.(cpp|c|mm))\s*$') { $file = $Matches[1] }
        }
    }
    $done = [math]::Min($seen.Count, $total)
    $pct = [math]::Min(99, [int](100 * $done / $total))
    $fill = [int]($width * $pct / 100)
    $bar = ('#' * $fill) + ('-' * ($width - $fill))
    $t = (Get-Date) - $start
    $name = if ($file.Length -gt 28) { $file.Substring(0, 28) } else { $file }
    $text = "`r   [{0}] {1,3}%  {2}/{3} parts  {4:mm\:ss}  {5,-28}" -f $bar, $pct, $done, $total, $t, $name
    [Console]::Write($text)
}
$p.WaitForExit()
$code = $p.ExitCode
if ($null -eq $code) { $code = 1 }
$done = $seen.Count
if ($code -eq 0) {
    [Console]::Write("`r   [{0}] 100%  {1}/{1} parts  {2:mm\:ss}  {3,-28}" -f ('#' * $width), [math]::Max($done, $total), ((Get-Date) - $start), 'done')
}
[Console]::WriteLine()

# everything into the log, as before
foreach ($f in @($out, $err)) {
    if (Test-Path $f) { Get-Content -LiteralPath $f | Add-Content -LiteralPath $Log; Remove-Item -LiteralPath $f -ErrorAction SilentlyContinue }
}
exit $code

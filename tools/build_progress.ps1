# Helper for build_and_install.bat. Three jobs:
#   -Cmake ... -Jobs N -Log file [-Targets "A_VST3 B_VST3"] [-Key all-no]
#        runs the build (low priority) and draws a progress bar with the time left;
#        remembers how long each kind of build took (build_times.txt) for the next estimate
#   -Errors -Log file       prints the first errors of a failed build, readable (Cat, file, line, message)
#   -Notify "title" -Text "text" [-Bad]   a Windows notification bubble
param([string]$Cmake, [int]$Jobs = 2, [string]$Log, [string]$Targets = '', [string]$Key = 'all',
      [switch]$Errors, [string]$Notify, [string]$Text = '', [switch]$Bad)

$root = Split-Path -Parent $PSScriptRoot

# ---------------------------------------------------------------- notification
if ($Notify) {
    try {
        Add-Type -AssemblyName System.Windows.Forms, System.Drawing
        $n = New-Object System.Windows.Forms.NotifyIcon
        $n.Icon = if ($Bad) { [System.Drawing.SystemIcons]::Error } else { [System.Drawing.SystemIcons]::Information }
        $n.Visible = $true
        $n.ShowBalloonTip(8000, $Notify, $Text, $(if ($Bad) { 'Error' } else { 'Info' }))
        Start-Sleep -Seconds 6
        $n.Dispose()
    } catch {}
    exit 0
}

# ---------------------------------------------------------------- readable errors
if ($Errors) {
    $lines = Get-Content -LiteralPath $Log -ErrorAction SilentlyContinue
    $shown = @{}
    $count = 0
    foreach ($l in $lines) {
        $msg = $null
        # D:\...\Source\File.cpp(123,5): error C2065: 'x': undeclared identifier [D:\...\build\GateCat\GateCat.vcxproj]
        if ($l -match '([^\\/]+\.(cpp|h|c|mm))\((\d+)(,\d+)?\):\s*(fatal\s+)?error\s+\w+:\s*(.+?)\s*(\[(.+\\)?([^\\]+?)\.vcxproj\])?\s*$') {
            $cat = if ($Matches[9]) { $Matches[9] -replace '_(VST3|Standalone|SharedCode)$', '' } else { '?' }
            $msg = "{0,-14} {1} line {2}: {3}" -f $cat, $Matches[1], $Matches[3], $Matches[6]
        }
        elseif ($l -match 'error LNK\d+:\s*(.+?)\s*(\[(.+\\)?([^\\]+?)\.vcxproj\])?\s*$') {
            $cat = if ($Matches[4]) { $Matches[4] } else { '?' }
            $msg = "{0,-14} linking: {1}" -f $cat, $Matches[1]
        }
        elseif ($l -match 'MSB1009|Project file does not exist') { $msg = "No Cat with that name - check the spelling (e.g. Gate-Cat, Crypt, Poly Board)" }
        elseif ($l -match '^\s*\[FAIL\]\s*(.+)$') { $msg = "test failed:    " + $Matches[1] }
        elseif ($l -match '^FAIL\s+(.+)$') { $msg = "test failed:    " + $Matches[1] }
        if ($msg -and -not $shown.ContainsKey($msg)) {
            $shown[$msg] = 1
            if ($msg.Length -gt 150) { $msg = $msg.Substring(0, 147) + '...' }
            Write-Host ("   " + $msg)
            if (++$count -ge 8) { break }
        }
    }
    if ($count -eq 0) { Get-Content -LiteralPath $Log -Tail 6 | ForEach-Object { Write-Host ("   " + $_) } }
    exit 0
}

# ---------------------------------------------------------------- the build with a progress bar
$out = Join-Path $env:TEMP ("cat_build_" + [guid]::NewGuid().ToString('N') + ".txt")
$err = $out + ".err"
$timesFile = Join-Path $root 'build_times.txt'

# how many parts: all = every project CMake made (not its helpers); some Cats = about 2 each + the shared parts
$targetList = @($Targets -split '\s+' | Where-Object { $_ })
if ($targetList.Count -gt 0) {
    $total = 2 * $targetList.Count + 2
} else {
    $skip = 'ALL_BUILD|ZERO_CHECK|INSTALL|RUN_TESTS|PACKAGE|_rc_lib'
    $total = @(Get-ChildItem -Path (Join-Path $root 'build') -Recurse -Filter *.vcxproj -ErrorAction SilentlyContinue |
               Where-Object { $_.FullName -notmatch '\\_deps\\' -and $_.BaseName -notmatch $skip }).Count
}
if ($total -lt 1) { $total = 1 }

# the last time this kind of build took (seconds)
$last = 0
if (Test-Path $timesFile) {
    foreach ($l in Get-Content $timesFile) { if ($l -match "^$([regex]::Escape($Key))=(\d+)$") { $last = [int]$Matches[1] } }
}

$cmArgs = @('--build', 'build', '--config', 'Release', '--parallel', "$Jobs")
if ($targetList.Count -gt 0) { $cmArgs += '--target'; $cmArgs += $targetList }
$p = Start-Process -FilePath $Cmake -ArgumentList $cmArgs -WorkingDirectory $root -NoNewWindow -PassThru `
                   -RedirectStandardOutput $out -RedirectStandardError $err
$null = $p.Handle                     # keeps the exit code readable later (a PowerShell quirk)
try { $p.PriorityClass = 'BelowNormal' } catch {}

function Left([double]$s) {
    if ($s -lt 60) { return 'under a minute left' }
    return ('about {0} min left' -f [math]::Ceiling($s / 60))
}

$start = Get-Date
$width = 28
$seen = @{}
$file = ''
while (-not $p.HasExited) {
    Start-Sleep -Milliseconds 700
    if (Test-Path $out) {
        foreach ($l in (Get-Content -LiteralPath $out -ErrorAction SilentlyContinue)) {
            if ($l -match '^\s*(\S+)\.vcxproj -> ') { $seen[$Matches[1]] = 1 }
            elseif ($l -match '^\s+([\w\-]+\.(cpp|c|mm))\s*$') { $file = $Matches[1] }
        }
    }
    $done = [math]::Min($seen.Count, $total)
    $pct = [math]::Min(99, [int](100 * $done / $total))
    $el = ((Get-Date) - $start).TotalSeconds
    # the time left: from the last build of the same kind, and (once it is under way) from the speed so far
    $eta = 'estimating...'
    $guesses = @()
    if ($last -gt 0) { $guesses += [math]::Max(0, $last - $el) }
    if ($pct -ge 10) { $guesses += [math]::Max(0, $el * (100 - $pct) / $pct) }
    if ($guesses.Count -gt 0) {
        $s = ($guesses | Measure-Object -Average).Average
        $eta = if ($s -le 0) { 'almost done' } else { Left $s }
    }
    $fill = [int]($width * $pct / 100)
    $bar = ('#' * $fill) + ('-' * ($width - $fill))
    $name = if ($file.Length -gt 22) { $file.Substring(0, 22) } else { $file }
    [Console]::Write(("`r   [{0}] {1,3}%  {2:mm\:ss}  {3,-20} {4,-22}" -f $bar, $pct, ((Get-Date) - $start), $eta, $name))
}
$p.WaitForExit()
$code = $p.ExitCode
if ($null -eq $code) { $code = 1 }
$el = [int]((Get-Date) - $start).TotalSeconds
if ($code -eq 0) {
    [Console]::Write(("`r   [{0}] 100%  {1:mm\:ss}  {2,-20} {3,-22}" -f ('#' * $width), ((Get-Date) - $start), 'done', ''))
    # remember how long it took (only when something was really built: a "nothing changed" build takes seconds)
    if ($el -gt 20) {
        $keep = @()
        if (Test-Path $timesFile) { $keep = @(Get-Content $timesFile | Where-Object { $_ -notmatch "^$([regex]::Escape($Key))=" }) }
        ($keep + "$Key=$el") | Set-Content $timesFile
    }
}
[Console]::WriteLine()

foreach ($f in @($out, $err)) {
    if (Test-Path $f) { Get-Content -LiteralPath $f | Add-Content -LiteralPath $Log; Remove-Item -LiteralPath $f -ErrorAction SilentlyContinue }
}
exit $code

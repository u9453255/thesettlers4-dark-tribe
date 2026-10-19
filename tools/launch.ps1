$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'find-python.ps1')
$manifestPath = Join-Path $projectRoot 'runtime\manifest.json'
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'runtime\native\DarkTribe-0.2.15.dll')) -or -not (Test-Path -LiteralPath $manifestPath)) {
    throw 'Dark Tribe is not built yet. Build it first (README.md: python tools\setup.py, then python tools\workflow.py).'
}
# The game the native build was verified against (written by tools\build-native.py).
$gameExe = (Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json).game_executable
$gameDir = Split-Path -Parent $gameExe
if (-not (Test-Path -LiteralPath $gameExe)) { throw ('The game was not found at ' + $gameExe + '. Rebuild after configuring the game folder (README.md).') }
$pythonExe = Find-DarkTribePython -ProjectRoot $projectRoot
$pythonWindowlessExe = Join-Path (Split-Path -Parent $pythonExe) 'pythonw.exe'
if (-not (Test-Path -LiteralPath $pythonWindowlessExe)) { $pythonWindowlessExe = $pythonExe }
if (Get-Process S4_Main -ErrorAction SilentlyContinue) { throw 'Close The Settlers IV first, then run Launch Dark Tribe.cmd again.' }
& $pythonExe (Join-Path $PSScriptRoot 'load-native.py')
if ($LASTEXITCODE -ne 0) { throw 'Native files or game build failed verification.' }
& $pythonExe (Join-Path $PSScriptRoot 'install.py') install --game $gameDir
if ($LASTEXITCODE -ne 0) { throw 'Installation verification failed.' }
$statusPath = Join-Path $projectRoot 'evidence\startup-status-0215.json'
if (Test-Path -LiteralPath $statusPath) { Remove-Item -LiteralPath $statusPath }
Start-Process -FilePath $pythonWindowlessExe -ArgumentList ('"' + (Join-Path $PSScriptRoot 'watch-human-slot.py') + '"') -WorkingDirectory $projectRoot -WindowStyle Hidden
if (-not (Get-Process S4_Main -ErrorAction SilentlyContinue)) {
    Start-Process -FilePath $gameExe -WorkingDirectory $gameDir -WindowStyle Normal
}
Write-Host 'Keep the game at its main menu until this launcher reports Dark Tribe ready.'
$deadline = (Get-Date).AddSeconds(180)
while ((Get-Date) -lt $deadline) {
    if (Test-Path -LiteralPath $statusPath) {
        $startupStatus = Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json
        if ($startupStatus.status -eq 'ready') { Write-Host 'Dark Tribe 0.2.15 ready. Start a new First Dawn, Iron Kingdom or The Alliance custom scenario.'; exit 0 }
        if ($startupStatus.status -eq 'failed') { throw 'Native startup failed. Close the game; details are in evidence\session-helper.jsonl.' }
    }
    Start-Sleep -Milliseconds 200
}
throw 'Native startup was not confirmed. Close the game and keep evidence\session-helper.jsonl for diagnosis.'

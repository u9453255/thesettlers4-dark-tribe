$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'find-python.ps1')
if (Get-Process S4_Main -ErrorAction SilentlyContinue) { throw 'Close The Settlers IV before restoring original files.' }
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'backups\install-manifest.json'))) { throw 'Nothing to restore: Dark Tribe has not been installed from this folder.' }
$pythonExe = Find-DarkTribePython -ProjectRoot $projectRoot
& $pythonExe (Join-Path $PSScriptRoot 'install.py') restore
if ($LASTEXITCODE -ne 0) { throw 'Restore failed; see the message above.' }

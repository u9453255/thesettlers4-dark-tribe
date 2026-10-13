# Dot-sourced by launch.ps1 and restore.ps1. Finds a real Python 3.10+ interpreter:
# DARK_TRIBE_PYTHON, then "python" in local-config.json, then python.exe on PATH, then the py launcher.
function Find-DarkTribePython {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)
    $candidates = @()
    if ($env:DARK_TRIBE_PYTHON) { $candidates += , @($env:DARK_TRIBE_PYTHON) }
    $configPath = Join-Path $ProjectRoot 'local-config.json'
    if (Test-Path -LiteralPath $configPath) {
        $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
        if ($config.python) { $candidates += , @($config.python) }
    }
    $candidates += , @('python.exe')
    $candidates += , @('py.exe', '-3')
    foreach ($candidate in $candidates) {
        $command = $candidate[0]
        $extra = @($candidate | Select-Object -Skip 1)
        if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { continue }
        try {
            # The Microsoft Store alias python.exe fails here and is skipped.
            $found = & $command @extra -c 'import sys; assert sys.version_info >= (3, 10); print(sys.executable)' 2>$null
        } catch { continue }
        if ($LASTEXITCODE -eq 0 -and $found) {
            $found = ([string]$found).Trim()
            if (Test-Path -LiteralPath $found) { return $found }
        }
    }
    throw 'Python 3.10 or newer was not found. Install it from python.org, or set DARK_TRIBE_PYTHON to the full path of python.exe.'
}

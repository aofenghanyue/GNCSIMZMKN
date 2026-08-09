[CmdletBinding()]
param(
    [ValidateSet('dev', 'release')]
    [string]$Preset = 'dev'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

Push-Location $repoRoot
try {
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE." }

    & cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE." }

    & ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE." }

    & (Join-Path $PSScriptRoot 'verify-repository.ps1')
    if ($LASTEXITCODE -ne 0) { throw "Repository verification failed with exit code $LASTEXITCODE." }
}
finally {
    Pop-Location
}

Write-Host "Bootstrap checks passed for preset '$Preset'."

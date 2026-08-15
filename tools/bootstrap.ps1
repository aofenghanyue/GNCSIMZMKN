[CmdletBinding()]
param(
    [ValidateSet('dev', 'release')]
    [string]$Preset = 'dev',
    [string]$EigenArchive
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

Push-Location $repoRoot
try {
    if ([string]::IsNullOrWhiteSpace($EigenArchive)) {
        $eigenPrefix = & (Join-Path $PSScriptRoot 'install-eigen.ps1') `
            -DownloadIfMissing
    }
    else {
        $eigenPrefix = & (Join-Path $PSScriptRoot 'install-eigen.ps1') `
            -ArchivePath $EigenArchive
    }
    if ($LASTEXITCODE -ne 0) { throw "Eigen setup failed with exit code $LASTEXITCODE." }
    $eigenConfig = Join-Path $eigenPrefix 'share/eigen3/cmake'

    & cmake --preset $Preset "-DEigen3_DIR=$eigenConfig"
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

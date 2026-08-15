[CmdletBinding()]
param(
    [string]$Destination,
    [string]$ArchivePath,
    [switch]$DownloadIfMissing
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$version = '3.4.0'
$expectedSha256 = 'eba3f3d414d2f8cba2919c78ec6daab08fc71ba2ba4ae502b7e5d4d99fc02cda'
$expectedBytes = 3704940
$sourceUri = 'https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip'
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $repoRoot 'build/dependencies/eigen-3.4.0'
}
$destinationPath = [System.IO.Path]::GetFullPath($Destination)
$installPath = Join-Path $destinationPath 'install'
$configPath = Join-Path $installPath 'share/eigen3/cmake/Eigen3Config.cmake'
if (Test-Path -LiteralPath $configPath -PathType Leaf) {
    Write-Output $installPath
    return
}

if ([string]::IsNullOrWhiteSpace($ArchivePath)) {
    $ArchivePath = Join-Path $destinationPath 'eigen-3.4.0.zip'
}
$archiveFullPath = [System.IO.Path]::GetFullPath($ArchivePath)
if (-not (Test-Path -LiteralPath $archiveFullPath -PathType Leaf)) {
    if (-not $DownloadIfMissing) {
        throw "Eigen archive is absent: $archiveFullPath. Supply -ArchivePath or -DownloadIfMissing."
    }
    $archiveParent = Split-Path -Parent $archiveFullPath
    New-Item -ItemType Directory -Path $archiveParent -Force | Out-Null
    Invoke-WebRequest -Uri $sourceUri -OutFile $archiveFullPath
}

$archiveItem = Get-Item -LiteralPath $archiveFullPath
if ($archiveItem.Length -ne $expectedBytes) {
    throw "Eigen archive byte count differs: $($archiveItem.Length)"
}
$actualSha256 = (Get-FileHash -LiteralPath $archiveFullPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha256 -ne $expectedSha256) {
    throw "Eigen archive SHA-256 differs: $actualSha256"
}

$sourceParent = Join-Path $destinationPath 'source'
$sourcePath = Join-Path $sourceParent "eigen-$version"
if (-not (Test-Path -LiteralPath (Join-Path $sourcePath 'CMakeLists.txt') -PathType Leaf)) {
    New-Item -ItemType Directory -Path $sourceParent -Force | Out-Null
    Expand-Archive -LiteralPath $archiveFullPath -DestinationPath $sourceParent -Force
}
if (-not (Test-Path -LiteralPath (Join-Path $sourcePath 'CMakeLists.txt') -PathType Leaf)) {
    throw "Eigen archive did not produce the expected source root: $sourcePath"
}

$buildPath = Join-Path $destinationPath 'cmake-build'
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null
& cmake -S $sourcePath -B $buildPath `
    "-DCMAKE_INSTALL_PREFIX=$installPath" `
    '-DBUILD_TESTING=OFF' `
    '-DEIGEN_BUILD_DOC=OFF' | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Eigen configure failed with exit code $LASTEXITCODE"
}
& cmake --install $buildPath --config Release | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Eigen install failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
    throw "Eigen installation did not produce Eigen3Config.cmake"
}

Write-Output $installPath

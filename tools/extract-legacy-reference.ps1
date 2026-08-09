[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$legacyRoot = Join-Path $repoRoot 'reference\legacy'
$manifestPath = Join-Path $legacyRoot 'source-manifest.json'

if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "Legacy manifest is missing: $manifestPath"
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json
$archivePath = Join-Path $repoRoot $manifest.archive.path
$checksumPath = Join-Path $legacyRoot 'legacy-source.sha256'
$destination = Join-Path $legacyRoot 'extracted'

if (-not (Test-Path -LiteralPath $archivePath)) {
    throw "Legacy archive is missing: $archivePath"
}

if (Test-Path -LiteralPath $destination) {
    throw "Destination already exists. Remove it explicitly after checking the path: $destination"
}

$actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
$expectedHash = (Get-Content -LiteralPath $checksumPath -Raw -Encoding utf8).Trim().Split(' ')[0].ToLowerInvariant()
if ($actualHash -ne $expectedHash) {
    throw "Legacy archive hash mismatch. Expected $expectedHash, got $actualHash."
}

New-Item -ItemType Directory -Path $destination | Out-Null
Expand-Archive -LiteralPath $archivePath -DestinationPath $destination

Write-Host "Legacy reference extracted to: $destination"
Write-Host "The extracted tree is read-only reference material and is excluded from the new build."

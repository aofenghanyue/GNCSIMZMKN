[CmdletBinding()]
param([switch]$Quiet)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$architectureModulePath = Join-Path $PSScriptRoot 'modules\ArchitectureBaseline.psm1'
$sourceBoundaryModulePath = Join-Path $PSScriptRoot 'modules\SourceBoundaryFitness.psm1'
$strictJsonModulePath = Join-Path $PSScriptRoot 'modules\JsonSchemaSubset.psm1'
$registryPath = Join-Path $repoRoot 'docs\architecture\authority-registry.json'

Import-Module -Name $architectureModulePath -Force
Import-Module -Name $sourceBoundaryModulePath -Force
Import-Module -Name $strictJsonModulePath -Force

try {
    $registry = Read-StrictJsonFile -Path $registryPath
    $adrPath = Join-Path $repoRoot ([string]$registry.dependency_authority)
    $dependency = ConvertFrom-DependencySources `
        -AdrText (Get-Content -LiteralPath $adrPath -Raw -Encoding utf8) `
        -CMakeText (Get-Content -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') -Raw -Encoding utf8)
    $policy = New-SourceBoundaryPolicy -Registry $registry -Dependency $dependency
    $inventory = Get-SourceBoundaryInventory -RepoRoot $repoRoot -Policy $policy
}
catch {
    Write-Host "Source-boundary input parsing failed: $($_.Exception.Message)"
    exit 1
}

$validation = Test-SourceBoundaryInventory -Inventory $inventory -Policy $policy
if (-not $validation.IsValid) {
    Write-Host "Source-boundary validation failed with $($validation.Findings.Count) finding(s):"
    foreach ($finding in @($validation.Findings)) {
        $location = if ([int]$finding.line -gt 0) { "$($finding.path):$($finding.line)" } else { [string]$finding.path }
        Write-Host " - [$($finding.rule_id)] ${location}: $($finding.message)"
    }
    exit 1
}

$negativeCases = Invoke-SourceBoundaryNegativeCases -RepoRoot $repoRoot -Policy $policy
if (-not $negativeCases.IsValid) {
    Write-Host "Source-boundary negative cases failed with $($negativeCases.Failures.Count) issue(s):"
    foreach ($failure in @($negativeCases.Failures)) { Write-Host " - $failure" }
    exit 1
}

if (-not $Quiet) {
    Write-Host 'Source-boundary validation passed.'
    Write-Host "Validated production C/C++ files: $($validation.SourceFileCount)"
    Write-Host "Validated production include directives: $($validation.IncludeCount)"
    Write-Host "Validated production CMake files: $($validation.CMakeFileCount)"
    Write-Host "Rejected source-boundary negative cases: $($negativeCases.CaseCount)"
}

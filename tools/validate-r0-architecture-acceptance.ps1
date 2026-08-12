[CmdletBinding()]
param([switch]$Quiet)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$strictJsonModule = Join-Path $PSScriptRoot 'modules\JsonSchemaSubset.psm1'
$acceptanceModule = Join-Path $PSScriptRoot 'modules\R0ArchitectureAcceptance.psm1'
Import-Module -Name $strictJsonModule -Force
Import-Module -Name $acceptanceModule -Force

$strictPaths = @(
    'docs/governance/r0-owner-authorization.json',
    'docs/architecture/r0-architecture-review-contract.json',
    'docs/architecture/architecture-baseline.json',
    'docs/quality/terminology-conformance-report.json',
    'docs/governance/adr-dispositions/ADR-0005-2026-08-12.json',
    'docs/governance/reconciliation-dispositions/RECON-DEC-006-2026-08-12.json',
    'docs/governance/reconciliation-dispositions/RECON-DEC-007-2026-08-12.json',
    'docs/quality/task-acceptance-R0-ARCH-001.json',
    'docs/tasks/backlog.json',
    'project-manifest.json',
    'docs/governance/provenance-inventory.json'
)
try {
    foreach ($relativePath in $strictPaths) {
        [void](Read-StrictJsonFile -Path (Join-Path $repoRoot $relativePath))
    }
}
catch {
    Write-Host "R0 architecture acceptance strict JSON failed: $($_.Exception.Message)"
    exit 1
}

$result = Test-R0ArchitectureAcceptance -RepoRoot $repoRoot -RunMutations
if (@($result.Issues).Count -gt 0) {
    Write-Host "R0 architecture acceptance failed with $(@($result.Issues).Count) issue(s):"
    foreach ($issue in @($result.Issues)) { Write-Host " - $issue" }
    exit 1
}

if (-not $Quiet) {
    Write-Host 'R0 architecture acceptance passed.'
    Write-Host "Validated acceptance mutations: $($result.MutationCount)"
}

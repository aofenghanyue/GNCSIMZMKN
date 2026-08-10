[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [string]$CaseManifest = 'tests/r0-spec-001/cases.json',
    [string]$ProvenanceRegister = 'docs/quality/provenance-register.json',
    [string]$ManifestClassification = 'docs/quality/r0-spec-001/existing-manifest-classification.json',
    [string]$RoleAssignments = 'docs/team/role-assignments.json',
    [string]$TaskBacklog = 'docs/tasks/backlog.json'
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Split-Path -Parent $PSScriptRoot
}

try {
    $pythonCommand = Get-Command python -ErrorAction Stop
    $validatorPath = Join-Path $RepositoryRoot 'tools/r0_spec_schema_validator.py'

    & $pythonCommand.Source -B $validatorPath `
        --root $RepositoryRoot `
        --case-manifest $CaseManifest `
        --provenance-register $ProvenanceRegister `
        --manifest-classification $ManifestClassification `
        --role-assignments $RoleAssignments `
        --task-backlog $TaskBacklog

    if ($LASTEXITCODE -ne 0) {
        throw "R0-SPEC-001 validator exited with code $LASTEXITCODE"
    }

    Write-Host 'R0-SPEC-001 schema conformance passed.'
    $global:LASTEXITCODE = 0
}
catch {
    Write-Error $_
    $global:LASTEXITCODE = 1
    exit 1
}

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$validator = Join-Path $PSScriptRoot 'verify-provenance-record.ps1'
$engine = (Get-Process -Id $PID).Path

function Invoke-PolicyCase(
    [string]$RelativePath,
    [bool]$ExpectSuccess,
    [string]$ExpectedPattern
) {
    $casePath = Join-Path $repoRoot $RelativePath
    $output = & $engine -NoProfile -ExecutionPolicy Bypass -File $validator -Path $casePath 2>&1
    $exitCode = $LASTEXITCODE
    $outputText = ($output | Out-String).Trim()

    if ($ExpectSuccess) {
        if ($exitCode -ne 0) {
            throw "Expected provenance case to pass: $RelativePath`n$outputText"
        }
        Write-Host "Expected pass: $RelativePath"
        return
    }

    if ($exitCode -eq 0) {
        throw "Expected provenance case to fail: $RelativePath"
    }
    if ($outputText -notmatch $ExpectedPattern) {
        throw "Provenance case failed without expected evidence '$ExpectedPattern': $RelativePath`n$outputText"
    }
    Write-Host "Expected rejection: $RelativePath ($ExpectedPattern)"
}

Invoke-PolicyCase 'docs/quality/provenance-register.json' $true ''
Invoke-PolicyCase 'tests/provenance-record-valid-scientific.json' $true ''
Invoke-PolicyCase 'tests/provenance-record-invalid-missing-rights.json' $false "missing 'rights'"
Invoke-PolicyCase 'tests/provenance-record-invalid-scientific-data.json' $false "scientific_context"
Invoke-PolicyCase 'tests/provenance-record-invalid-independent-reference.json' $false "independent_reference_confirmed=true"
Invoke-PolicyCase 'tests/provenance-record-invalid-inherit-false.json' $false "inherit_restrictions=true"
Invoke-PolicyCase 'tests/provenance-record-invalid-generated-upstream.json' $false "upstream_record_refs"
Invoke-PolicyCase 'tests/provenance-record-invalid-generated-upstream.json' $false "classification 'public-candidate' is weaker"
Invoke-PolicyCase 'tests/provenance-record-invalid-subject-type.json' $false "subject type"
Invoke-PolicyCase 'tests/provenance-record-invalid-noassertion.json' $false "cannot authorize external sharing"

Write-Host 'Provenance policy conformance checks passed.'
$global:LASTEXITCODE = 0

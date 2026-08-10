[CmdletBinding()]
param(
    [string]$SchemaPath,
    [string]$InstancePath,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$modulePath = Join-Path $PSScriptRoot 'modules\JsonSchemaSubset.psm1'
Import-Module -Name $modulePath -Force

function Get-SemanticErrors {
    param(
        [string]$SchemaName,
        [string]$Path
    )

    $errors = [System.Collections.Generic.List[string]]::new()
    $instance = Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json

    if ($SchemaName -eq 'fixture-manifest.schema.json') {
        $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        foreach ($fact in @($instance.expected_facts)) {
            if (-not $seen.Add([string]$fact.id)) {
                [void]$errors.Add("$.expected_facts: duplicate expected fact id '$($fact.id)'")
            }
        }
    }
    elseif ($SchemaName -eq 'oracle-manifest.schema.json') {
        $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        foreach ($oracle in @($instance.oracles)) {
            if (-not $seen.Add([string]$oracle.id)) {
                [void]$errors.Add("$.oracles: duplicate oracle id '$($oracle.id)'")
            }
        }
    }

    return @($errors)
}

function Invoke-ContractValidation {
    param(
        [string]$ResolvedSchemaPath,
        [string]$ResolvedInstancePath
    )

    $result = Test-JsonSchemaSubset -SchemaPath $ResolvedSchemaPath -InstancePath $ResolvedInstancePath
    $combinedErrors = [System.Collections.Generic.List[string]]::new()
    foreach ($message in @($result.Errors)) {
        [void]$combinedErrors.Add([string]$message)
    }

    if ($result.IsValid) {
        $schemaName = Split-Path -Leaf $ResolvedSchemaPath
        foreach ($message in @(Get-SemanticErrors -SchemaName $schemaName -Path $ResolvedInstancePath)) {
            [void]$combinedErrors.Add([string]$message)
        }
    }

    return [PSCustomObject]@{
        IsValid = $combinedErrors.Count -eq 0
        Errors = @($combinedErrors)
    }
}

if ([string]::IsNullOrWhiteSpace($SchemaPath) -xor [string]::IsNullOrWhiteSpace($InstancePath)) {
    Write-Host 'SchemaPath and InstancePath must be provided together.'
    exit 2
}

if (-not [string]::IsNullOrWhiteSpace($SchemaPath)) {
    $result = Invoke-ContractValidation -ResolvedSchemaPath $SchemaPath -ResolvedInstancePath $InstancePath
    if (-not $result.IsValid) {
        Write-Host "Schema validation failed for '$InstancePath':"
        foreach ($message in @($result.Errors)) {
            Write-Host " - $message"
        }
        exit 1
    }
    if (-not $Quiet) {
        Write-Host "Schema validation passed for '$InstancePath'."
    }
    exit 0
}

$contracts = @(
    [PSCustomObject]@{
        Name = 'fixture-manifest'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        ExampleRoot = Join-Path $repoRoot 'specs\examples\fixture-manifest'
        Actual = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'fixtures') -Recurse -File -Filter 'fixture-manifest.json' | Select-Object -ExpandProperty FullName)
    },
    [PSCustomObject]@{
        Name = 'oracle-manifest'
        Schema = Join-Path $repoRoot 'specs\oracle-manifest.schema.json'
        ExampleRoot = Join-Path $repoRoot 'specs\examples\oracle-manifest'
        Actual = @((Join-Path $repoRoot 'oracles\oracle-manifest.json'))
    },
    [PSCustomObject]@{
        Name = 'plan-proof-record'
        Schema = Join-Path $repoRoot 'specs\plan-proof-record.schema.json'
        ExampleRoot = Join-Path $repoRoot 'specs\examples\plan-proof-record'
        Actual = @()
    }
)

$failures = [System.Collections.Generic.List[string]]::new()
$actualCount = 0
$validExampleCount = 0
$invalidExampleCount = 0
$validatorFailureCaseCount = 0

foreach ($contract in $contracts) {
    $validRoot = Join-Path $contract.ExampleRoot 'valid'
    $invalidRoot = Join-Path $contract.ExampleRoot 'invalid'
    $validExamples = @(Get-ChildItem -LiteralPath $validRoot -File -Filter '*.json')
    $invalidExamples = @(Get-ChildItem -LiteralPath $invalidRoot -File -Filter '*.json')

    if ($validExamples.Count -eq 0) {
        [void]$failures.Add("$($contract.Name): no valid example is registered")
    }
    if ($invalidExamples.Count -eq 0) {
        [void]$failures.Add("$($contract.Name): no invalid example is registered")
    }

    foreach ($actualPath in @($contract.Actual)) {
        ++$actualCount
        $result = Invoke-ContractValidation -ResolvedSchemaPath $contract.Schema -ResolvedInstancePath $actualPath
        if (-not $result.IsValid) {
            [void]$failures.Add("actual manifest should be valid: $actualPath :: $($result.Errors -join ' | ')")
        }
    }

    foreach ($example in $validExamples) {
        ++$validExampleCount
        $result = Invoke-ContractValidation -ResolvedSchemaPath $contract.Schema -ResolvedInstancePath $example.FullName
        if (-not $result.IsValid) {
            [void]$failures.Add("valid example was rejected: $($example.FullName) :: $($result.Errors -join ' | ')")
        }
    }

    foreach ($example in $invalidExamples) {
        ++$invalidExampleCount
        $result = Invoke-ContractValidation -ResolvedSchemaPath $contract.Schema -ResolvedInstancePath $example.FullName
        if ($result.IsValid) {
            [void]$failures.Add("invalid example was accepted: $($example.FullName)")
        }
    }
}

$validatorFailureCases = @(
    [PSCustomObject]@{
        Name = 'unsupported schema keyword'
        Schema = Join-Path $repoRoot 'specs\examples\validator\unsupported-keyword.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\empty-object.json'
    },
    [PSCustomObject]@{
        Name = 'malformed JSON instance'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\malformed-instance.txt'
    },
    [PSCustomObject]@{
        Name = 'null root against object schema'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\null-instance.txt'
    }
)

foreach ($failureCase in $validatorFailureCases) {
    ++$validatorFailureCaseCount
    $result = Invoke-ContractValidation -ResolvedSchemaPath $failureCase.Schema -ResolvedInstancePath $failureCase.Instance
    if ($result.IsValid) {
        [void]$failures.Add("validator failure case was accepted: $($failureCase.Name)")
    }
}

if ($failures.Count -gt 0) {
    Write-Host "R0 schema validation failed with $($failures.Count) issue(s):"
    foreach ($failure in $failures) {
        Write-Host " - $failure"
    }
    exit 1
}

if (-not $Quiet) {
    Write-Host 'R0 schema validation passed.'
    Write-Host "Validated schemas: $($contracts.Count)"
    Write-Host "Validated actual manifests: $actualCount"
    Write-Host "Validated valid examples: $validExampleCount"
    Write-Host "Validated invalid examples: $invalidExampleCount"
    Write-Host "Validated validator failure cases: $validatorFailureCaseCount"
}

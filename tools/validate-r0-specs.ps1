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

$roleAssignmentsPath = Join-Path $repoRoot 'docs\team\role-assignments.json'
$taskBacklogPath = Join-Path $repoRoot 'docs\tasks\backlog.json'
$roleAssignments = Read-StrictJsonFile -Path $roleAssignmentsPath
$taskBacklog = Read-StrictJsonFile -Path $taskBacklogPath
$knownRoleIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($role in @($roleAssignments.roles)) {
    [void]$knownRoleIds.Add([string]$role.id)
}
$taskStatusById = [System.Collections.Generic.Dictionary[string, string]]::new([System.StringComparer]::Ordinal)
foreach ($task in @($taskBacklog.tasks)) {
    $taskStatusById.Add([string]$task.id, [string]$task.status)
}

$pathComparison = if ($env:OS -eq 'Windows_NT') {
    [System.StringComparison]::OrdinalIgnoreCase
}
else {
    [System.StringComparison]::Ordinal
}
$repoRootFull = [System.IO.Path]::GetFullPath($repoRoot)
$trimChars = [char[]]@([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
$repoRootPrefix = $repoRootFull.TrimEnd($trimChars) + [System.IO.Path]::DirectorySeparatorChar

function Get-InstanceProperty {
    param(
        $Object,
        [string]$Name
    )

    if ($null -ne $Object) {
        foreach ($property in $Object.PSObject.Properties) {
            if ([string]::Equals($property.Name, $Name, [System.StringComparison]::Ordinal)) {
                return [PSCustomObject]@{ Exists = $true; Value = $property.Value }
            }
        }
    }

    return [PSCustomObject]@{ Exists = $false; Value = $null }
}

function Test-RepositoryArtifactFile {
    param(
        [string]$ManifestPath,
        [string]$Reference
    )

    if ([string]::IsNullOrWhiteSpace($Reference) -or [System.IO.Path]::IsPathRooted($Reference)) {
        return $false
    }

    $manifestDirectory = Split-Path -Parent (Resolve-Path -LiteralPath $ManifestPath).Path
    foreach ($basePath in @($manifestDirectory, $repoRootFull)) {
        try {
            $candidate = [System.IO.Path]::GetFullPath((Join-Path $basePath $Reference))
        }
        catch {
            continue
        }

        if (-not $candidate.StartsWith($repoRootPrefix, $pathComparison)) { continue }
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $true }
    }

    return $false
}

function Get-SemanticErrors {
    param(
        [string]$SchemaName,
        [string]$Path
    )

    $errors = [System.Collections.Generic.List[string]]::new()
    $instance = Read-StrictJsonFile -Path $Path

    if ($SchemaName -eq 'fixture-manifest.schema.json') {
        $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        foreach ($fact in @($instance.expected_facts)) {
            if (-not $seen.Add([string]$fact.id)) {
                [void]$errors.Add("$.expected_facts: duplicate expected fact id '$($fact.id)'")
            }
        }

        if (-not $knownRoleIds.Contains([string]$instance.authority)) {
            [void]$errors.Add("$.authority: role '$($instance.authority)' does not resolve in docs/team/role-assignments.json")
        }

        $openTasksProperty = Get-InstanceProperty -Object $instance -Name 'open_tasks'
        if ($openTasksProperty.Exists) {
            foreach ($taskId in @($openTasksProperty.Value)) {
                $taskKey = [string]$taskId
                if (-not $taskStatusById.ContainsKey($taskKey)) {
                    [void]$errors.Add("$.open_tasks: task '$taskKey' does not resolve in docs/tasks/backlog.json")
                }
                elseif ($taskStatusById[$taskKey] -ceq 'done') {
                    [void]$errors.Add("$.open_tasks: task '$taskKey' is already done")
                }
            }
        }

        if ([string]$instance.status -cin @('executable', 'qualified')) {
            $artifactIndex = 0
            foreach ($artifactRef in @($instance.required_artifacts)) {
                if (-not (Test-RepositoryArtifactFile -ManifestPath $Path -Reference ([string]$artifactRef))) {
                    [void]$errors.Add("$.required_artifacts[$artifactIndex]: executable evidence file '$artifactRef' does not resolve inside the repository")
                }
                ++$artifactIndex
            }

            $factIndex = 0
            foreach ($fact in @($instance.expected_facts)) {
                $evidenceProperty = Get-InstanceProperty -Object $fact -Name 'evidence_refs'
                if (-not $evidenceProperty.Exists -or @($evidenceProperty.Value).Count -eq 0) {
                    [void]$errors.Add("$.expected_facts[$factIndex].evidence_refs: executable fact requires at least one evidence file")
                }
                else {
                    $evidenceIndex = 0
                    foreach ($evidenceRef in @($evidenceProperty.Value)) {
                        if (-not (Test-RepositoryArtifactFile -ManifestPath $Path -Reference ([string]$evidenceRef))) {
                            [void]$errors.Add("$.expected_facts[$factIndex].evidence_refs[$evidenceIndex]: evidence file '$evidenceRef' does not resolve inside the repository")
                        }
                        ++$evidenceIndex
                    }
                }
                ++$factIndex
            }
        }
    }
    elseif ($SchemaName -eq 'oracle-manifest.schema.json') {
        $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        $oracleIndex = 0
        foreach ($oracle in @($instance.oracles)) {
            if (-not $seen.Add([string]$oracle.id)) {
                [void]$errors.Add("$.oracles: duplicate oracle id '$($oracle.id)'")
            }

            if ([string]$oracle.status -cin @('executable', 'qualified')) {
                $artifactProperty = Get-InstanceProperty -Object $oracle -Name 'artifact_refs'
                if (-not $artifactProperty.Exists -or @($artifactProperty.Value).Count -eq 0) {
                    [void]$errors.Add("$.oracles[$oracleIndex].artifact_refs: executable oracle requires at least one evidence file")
                }
                else {
                    $artifactIndex = 0
                    foreach ($artifactRef in @($artifactProperty.Value)) {
                        if (-not (Test-RepositoryArtifactFile -ManifestPath $Path -Reference ([string]$artifactRef))) {
                            [void]$errors.Add("$.oracles[$oracleIndex].artifact_refs[$artifactIndex]: evidence file '$artifactRef' does not resolve inside the repository")
                        }
                        ++$artifactIndex
                    }
                }
            }
            ++$oracleIndex
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

function Get-StableIdentityEntries {
    param(
        [string]$SchemaName,
        $Instance
    )

    if ($SchemaName -eq 'fixture-manifest.schema.json') {
        [PSCustomObject]@{ Kind = 'fixture_id'; Value = [string]$Instance.fixture_id }
        foreach ($fact in @($Instance.expected_facts)) {
            [PSCustomObject]@{ Kind = 'expected_fact_id'; Value = [string]$fact.id }
        }
    }
    elseif ($SchemaName -eq 'oracle-manifest.schema.json') {
        [PSCustomObject]@{ Kind = 'oracle_set_id'; Value = [string]$Instance.oracle_set_id }
        foreach ($oracle in @($Instance.oracles)) {
            [PSCustomObject]@{ Kind = 'oracle_id'; Value = [string]$oracle.id }
        }
    }
    elseif ($SchemaName -eq 'plan-proof-record.schema.json') {
        [PSCustomObject]@{ Kind = 'proof_id'; Value = [string]$Instance.proof_id }
    }
}

function Get-IdentityRegistryErrors {
    param([object[]]$Documents)

    $errors = [System.Collections.Generic.List[string]]::new()
    $seen = [System.Collections.Generic.Dictionary[string, string]]::new([System.StringComparer]::Ordinal)
    foreach ($document in @($Documents)) {
        foreach ($identity in @(Get-StableIdentityEntries -SchemaName $document.SchemaName -Instance $document.Instance)) {
            $registryKey = $identity.Kind + [char]0x1F + $identity.Value
            if ($seen.ContainsKey($registryKey)) {
                [void]$errors.Add("duplicate $($identity.Kind) '$($identity.Value)' in '$($seen[$registryKey])' and '$($document.Path)'")
            }
            else {
                $seen.Add($registryKey, [string]$document.Path)
            }
        }
    }

    return @($errors)
}

function Copy-JsonValue {
    param($Value)

    return $Value | ConvertTo-Json -Depth 100 | ConvertFrom-Json
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
$repositoryDocuments = [System.Collections.Generic.List[object]]::new()
$invalidExpectedErrors = @{
    'fixture-manifest/duplicate-fact-id.json' = 'duplicate expected fact id'
    'fixture-manifest/missing-provenance.json' = "missing required property 'provenance'"
    'fixture-manifest/missing-tolerance.json' = "missing required property 'tolerance_policy'"
    'fixture-manifest/unknown-authority.json' = 'does not resolve in docs/team/role-assignments.json'
    'fixture-manifest/unresolved-open-task.json' = 'does not resolve in docs/tasks/backlog.json'
    'fixture-manifest/completed-open-task.json' = 'is already done'
    'fixture-manifest/executable-missing-evidence.json' = 'executable fact requires at least one evidence file'
    'fixture-manifest/executable-unresolved-artifact.json' = 'does not resolve inside the repository'
    'oracle-manifest/duplicate-oracle-id.json' = 'duplicate oracle id'
    'oracle-manifest/missing-expected-facts.json' = "missing required property 'expected_facts'"
    'oracle-manifest/executable-without-artifact.json' = 'executable oracle requires at least one evidence file'
    'plan-proof-record/legacy-unsupported-result.json' = 'value is not in the declared enum'
    'plan-proof-record/rejected-without-diagnostic.json' = 'expected at least 1 items'
    'plan-proof-record/rejected-with-operator.json' = 'expected at most 0 items'
    'plan-proof-record/unsupported-with-operator.json' = 'expected at most 0 items'
    'plan-proof-record/deferred-without-diagnostic.json' = 'expected at least 1 items'
}

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
        else {
            [void]$repositoryDocuments.Add([PSCustomObject]@{
                SchemaName = Split-Path -Leaf $contract.Schema
                Path = $actualPath
                Instance = Read-StrictJsonFile -Path $actualPath
            })
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
        $expectationKey = "$($contract.Name)/$($example.Name)"
        if (-not $invalidExpectedErrors.ContainsKey($expectationKey)) {
            [void]$failures.Add("invalid example has no expected diagnostic: $($example.FullName)")
        }
        $result = Invoke-ContractValidation -ResolvedSchemaPath $contract.Schema -ResolvedInstancePath $example.FullName
        if ($result.IsValid) {
            [void]$failures.Add("invalid example was accepted: $($example.FullName)")
        }
        elseif ($invalidExpectedErrors.ContainsKey($expectationKey) -and
                ($result.Errors -join ' | ').IndexOf($invalidExpectedErrors[$expectationKey], [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            [void]$failures.Add("invalid example produced the wrong diagnostic: $($example.FullName) :: $($result.Errors -join ' | ')")
        }
    }
}

$repositoryIdentityCount = 0
foreach ($document in @($repositoryDocuments)) {
    $repositoryIdentityCount += @(Get-StableIdentityEntries -SchemaName $document.SchemaName -Instance $document.Instance).Count
}
foreach ($message in @(Get-IdentityRegistryErrors -Documents @($repositoryDocuments))) {
    [void]$failures.Add("repository stable identity registry: $message")
}

$fixtureIdentityBase = Read-StrictJsonFile -Path (Join-Path $repoRoot 'specs\examples\fixture-manifest\valid\minimal.json')
$fixtureFactDuplicate = Copy-JsonValue $fixtureIdentityBase
$fixtureFactDuplicate.fixture_id = 'REF-EXAMPLE-002'
$oracleIdentityBase = Read-StrictJsonFile -Path (Join-Path $repoRoot 'specs\examples\oracle-manifest\valid\minimal.json')
$oracleItemDuplicate = Copy-JsonValue $oracleIdentityBase
$oracleItemDuplicate.oracle_set_id = 'ORACLE-EXAMPLE-002'
$proofIdentityBase = Read-StrictJsonFile -Path (Join-Path $repoRoot 'specs\examples\plan-proof-record\valid\yyz-guidance-rate.json')

$identityMutationCases = @(
    [PSCustomObject]@{
        Name = 'duplicate fixture identity across manifests'
        ExpectedErrorContains = 'duplicate fixture_id'
        Documents = @(
            [PSCustomObject]@{ SchemaName = 'fixture-manifest.schema.json'; Path = 'mutation/fixture-a.json'; Instance = $fixtureIdentityBase },
            [PSCustomObject]@{ SchemaName = 'fixture-manifest.schema.json'; Path = 'mutation/fixture-b.json'; Instance = $fixtureIdentityBase }
        )
    },
    [PSCustomObject]@{
        Name = 'duplicate expected-fact identity across manifests'
        ExpectedErrorContains = 'duplicate expected_fact_id'
        Documents = @(
            [PSCustomObject]@{ SchemaName = 'fixture-manifest.schema.json'; Path = 'mutation/fact-a.json'; Instance = $fixtureIdentityBase },
            [PSCustomObject]@{ SchemaName = 'fixture-manifest.schema.json'; Path = 'mutation/fact-b.json'; Instance = $fixtureFactDuplicate }
        )
    },
    [PSCustomObject]@{
        Name = 'duplicate oracle-set identity across manifests'
        ExpectedErrorContains = 'duplicate oracle_set_id'
        Documents = @(
            [PSCustomObject]@{ SchemaName = 'oracle-manifest.schema.json'; Path = 'mutation/oracle-set-a.json'; Instance = $oracleIdentityBase },
            [PSCustomObject]@{ SchemaName = 'oracle-manifest.schema.json'; Path = 'mutation/oracle-set-b.json'; Instance = $oracleIdentityBase }
        )
    },
    [PSCustomObject]@{
        Name = 'duplicate oracle identity across manifests'
        ExpectedErrorContains = 'duplicate oracle_id'
        Documents = @(
            [PSCustomObject]@{ SchemaName = 'oracle-manifest.schema.json'; Path = 'mutation/oracle-a.json'; Instance = $oracleIdentityBase },
            [PSCustomObject]@{ SchemaName = 'oracle-manifest.schema.json'; Path = 'mutation/oracle-b.json'; Instance = $oracleItemDuplicate }
        )
    },
    [PSCustomObject]@{
        Name = 'duplicate proof identity across records'
        ExpectedErrorContains = 'duplicate proof_id'
        Documents = @(
            [PSCustomObject]@{ SchemaName = 'plan-proof-record.schema.json'; Path = 'mutation/proof-a.json'; Instance = $proofIdentityBase },
            [PSCustomObject]@{ SchemaName = 'plan-proof-record.schema.json'; Path = 'mutation/proof-b.json'; Instance = $proofIdentityBase }
        )
    }
)

$identityMutationCaseCount = 0
foreach ($mutationCase in $identityMutationCases) {
    ++$identityMutationCaseCount
    $messages = @(Get-IdentityRegistryErrors -Documents @($mutationCase.Documents))
    $matchingMessage = @($messages | Where-Object {
        $_.IndexOf($mutationCase.ExpectedErrorContains, [System.StringComparison]::Ordinal) -ge 0
    })
    if ($matchingMessage.Count -eq 0) {
        [void]$failures.Add("identity mutation was not rejected as expected: $($mutationCase.Name) :: $($messages -join ' | ')")
    }
}

$validatorFailureCases = @(
    [PSCustomObject]@{
        Name = 'unsupported schema keyword'
        Schema = Join-Path $repoRoot 'specs\examples\validator\unsupported-keyword.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\empty-object.json'
        ExpectedErrorContains = 'schema keyword is not supported'
    },
    [PSCustomObject]@{
        Name = 'malformed JSON instance'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\malformed-instance.txt'
        ExpectedErrorContains = 'cannot read or parse'
    },
    [PSCustomObject]@{
        Name = 'null root against object schema'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\null-instance.txt'
        ExpectedErrorContains = "expected type 'object'"
    },
    [PSCustomObject]@{
        Name = 'array root against object schema'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\array-root-instance.txt'
        ExpectedErrorContains = "expected type 'object'"
    },
    [PSCustomObject]@{
        Name = 'duplicate JSON object key'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\duplicate-json-key.txt'
        ExpectedErrorContains = 'duplicate JSON object key'
    },
    [PSCustomObject]@{
        Name = 'escaped duplicate JSON object key'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\duplicate-escaped-json-key.txt'
        ExpectedErrorContains = 'duplicate JSON object key'
    },
    [PSCustomObject]@{
        Name = 'NaN JSON token'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\non-finite-number.txt'
        ExpectedErrorContains = 'non-finite JSON token'
    },
    [PSCustomObject]@{
        Name = 'positive infinity JSON token'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\positive-infinity.txt'
        ExpectedErrorContains = 'non-finite JSON token'
    },
    [PSCustomObject]@{
        Name = 'negative infinity JSON token'
        Schema = Join-Path $repoRoot 'specs\fixture-manifest.schema.json'
        Instance = Join-Path $repoRoot 'specs\examples\validator\negative-infinity.txt'
        ExpectedErrorContains = 'non-finite JSON token'
    }
)

foreach ($failureCase in $validatorFailureCases) {
    ++$validatorFailureCaseCount
    $result = Invoke-ContractValidation -ResolvedSchemaPath $failureCase.Schema -ResolvedInstancePath $failureCase.Instance
    if ($result.IsValid) {
        [void]$failures.Add("validator failure case was accepted: $($failureCase.Name)")
    }
    elseif (($result.Errors -join ' | ').IndexOf($failureCase.ExpectedErrorContains, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
        [void]$failures.Add("validator failure case produced the wrong diagnostic: $($failureCase.Name) :: $($result.Errors -join ' | ')")
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
    Write-Host "Validated actual-manifest stable identities: $repositoryIdentityCount"
    Write-Host "Validated identity mutation cases: $identityMutationCaseCount"
}

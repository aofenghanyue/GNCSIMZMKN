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
$schemaLockPath = Join-Path $repoRoot 'specs\r0-schema-contract-lock.json'
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

function Get-PropertyValue {
    param(
        $Object,
        [string]$Name
    )

    $property = Get-InstanceProperty -Object $Object -Name $Name
    if (-not $property.Exists) { return $null }
    return $property.Value
}

function Get-RawSha256 {
    param([string]$Path)

    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.IO.File]::ReadAllBytes($Path)
        return ([System.BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Get-OrdinalSortedStrings {
    param([object[]]$Values)

    $result = [string[]]@($Values | ForEach-Object { [string]$_ })
    [System.Array]::Sort($result, [System.StringComparer]::Ordinal)
    return @($result)
}

function Test-ExactStringSequence {
    param(
        [object[]]$Actual,
        [string[]]$Expected
    )

    $actualValues = @($Actual | ForEach-Object { [string]$_ })
    if ($actualValues.Count -ne $Expected.Count) { return $false }
    for ($index = 0; $index -lt $Expected.Count; ++$index) {
        if ($actualValues[$index] -cne $Expected[$index]) { return $false }
    }
    return $true
}

function Add-ExactPropertyErrors {
    param(
        $Object,
        [string[]]$Required,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Errors
    )

    if ($null -eq $Object -or $Object -isnot [System.Management.Automation.PSCustomObject]) {
        [void]$Errors.Add("$Label must be a JSON object")
        return
    }

    $actual = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($property in @($Object.PSObject.Properties)) {
        [void]$actual.Add([string]$property.Name)
    }
    $allowed = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($name in $Required) { [void]$allowed.Add($name) }
    foreach ($name in $Required) {
        if (-not $actual.Contains($name)) { [void]$Errors.Add("$Label is missing '$name'") }
    }
    foreach ($name in $actual) {
        if (-not $allowed.Contains($name)) { [void]$Errors.Add("$Label has unknown property '$name'") }
    }
}

function Get-TrackedRepositoryBlobFact {
    param([string]$Reference)

    $fact = [PSCustomObject]@{
        Tracked = $false
        ExactPath = $false
        Mode = ''
        ObjectType = ''
        BlobBytes = [long]0
        WorktreeRegular = $false
        WorktreeBytes = [long]0
    }

    $entries = @(& git -C $repoRootFull -c core.quotepath=false --literal-pathspecs ls-files --stage -- $Reference 2>$null)
    if ($LASTEXITCODE -ne 0 -or $entries.Count -ne 1) { return $fact }
    $match = [regex]::Match(
        [string]$entries[0],
        '^(?<mode>[0-9]{6}) (?<object>[0-9a-f]{40,64}) 0\t(?<path>.+)$')
    if (-not $match.Success) { return $fact }

    $fact.Tracked = $true
    $fact.ExactPath = $match.Groups['path'].Value -ceq $Reference
    $fact.Mode = $match.Groups['mode'].Value
    $objectId = $match.Groups['object'].Value
    $objectType = @(& git -C $repoRootFull cat-file -t $objectId 2>$null)
    if ($LASTEXITCODE -eq 0 -and $objectType.Count -eq 1) {
        $fact.ObjectType = [string]$objectType[0]
    }
    $objectSize = @(& git -C $repoRootFull cat-file -s $objectId 2>$null)
    if ($LASTEXITCODE -eq 0 -and $objectSize.Count -eq 1) {
        $parsedSize = [long]0
        if ([long]::TryParse([string]$objectSize[0], [ref]$parsedSize)) {
            $fact.BlobBytes = $parsedSize
        }
    }

    try {
        $candidate = [System.IO.Path]::GetFullPath((Join-Path $repoRootFull $Reference))
        if ($candidate.StartsWith($repoRootPrefix, $pathComparison) -and
            (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            $item = Get-Item -LiteralPath $candidate -Force
            $fact.WorktreeRegular = $item -is [System.IO.FileInfo] -and
                (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0)
            if ($fact.WorktreeRegular) { $fact.WorktreeBytes = [long]$item.Length }
        }
    }
    catch {
        $fact.WorktreeRegular = $false
    }
    return $fact
}

function Test-RepositoryEvidenceLocator {
    param(
        [string]$Reference,
        $BlobFact = $null
    )

    if ([string]::IsNullOrWhiteSpace($Reference) -or
        $Reference.StartsWith('/', [System.StringComparison]::Ordinal) -or
        $Reference.StartsWith('\', [System.StringComparison]::Ordinal) -or
        $Reference.EndsWith('/', [System.StringComparison]::Ordinal) -or
        $Reference.Contains('\') -or
        $Reference.Contains('//') -or
        $Reference -match '(^|/)(\.|\.\.)(/|$)' -or
        $Reference -match '[\x00-\x1F:*?"<>|#\[\]]') {
        return [PSCustomObject]@{
            IsValid = $false
            Diagnostic = "evidence locator '$Reference' must be a repository-root-relative path using forward slashes"
        }
    }

    try {
        $candidate = [System.IO.Path]::GetFullPath((Join-Path $repoRootFull $Reference))
    }
    catch {
        return [PSCustomObject]@{ IsValid = $false; Diagnostic = "evidence locator '$Reference' is not a valid repository path" }
    }
    if (-not $candidate.StartsWith($repoRootPrefix, $pathComparison)) {
        return [PSCustomObject]@{ IsValid = $false; Diagnostic = "evidence locator '$Reference' escapes the repository" }
    }

    if ($null -eq $BlobFact) { $BlobFact = Get-TrackedRepositoryBlobFact -Reference $Reference }
    if (-not [bool]$BlobFact.Tracked -or -not [bool]$BlobFact.ExactPath) {
        return [PSCustomObject]@{ IsValid = $false; Diagnostic = "evidence locator '$Reference' does not name an exact tracked stage-0 Git path" }
    }
    if ([string]$BlobFact.Mode -notin @('100644', '100755') -or [string]$BlobFact.ObjectType -cne 'blob' -or
        -not [bool]$BlobFact.WorktreeRegular) {
        return [PSCustomObject]@{ IsValid = $false; Diagnostic = "evidence locator '$Reference' must resolve to a regular Git blob and regular worktree file" }
    }
    if ([long]$BlobFact.BlobBytes -le 0 -or [long]$BlobFact.WorktreeBytes -le 0) {
        return [PSCustomObject]@{ IsValid = $false; Diagnostic = "evidence locator '$Reference' must resolve to a nonempty Git blob and nonempty worktree file" }
    }
    return [PSCustomObject]@{ IsValid = $true; Diagnostic = '' }
}

function Get-PlanProofPremiseErrors {
    param($Instance)

    $errors = [System.Collections.Generic.List[string]]::new()
    $premises = Get-PropertyValue -Object $Instance -Name 'premises'
    if ($null -eq $premises -or $premises -isnot [System.Management.Automation.PSCustomObject]) {
        return @($errors)
    }
    foreach ($property in @($premises.PSObject.Properties)) {
        $value = $property.Value
        if ($value -is [System.Management.Automation.PSCustomObject] -or
            ($value -is [System.Collections.IEnumerable] -and $value -isnot [string])) {
            [void]$errors.Add("$.premises.$($property.Name): v1 premise snapshot values must be scalar; typed proof prerequisites require schema v2")
        }
    }
    return @($errors)
}

function Add-SchemaObjectFieldGraphNodes {
    param(
        $Schema,
        [string]$Path,
        [System.Collections.Generic.List[object]]$Nodes
    )

    if ($null -eq $Schema -or $Schema -isnot [System.Management.Automation.PSCustomObject]) { return }
    $type = [string](Get-PropertyValue -Object $Schema -Name 'type')
    $propertiesProperty = Get-InstanceProperty -Object $Schema -Name 'properties'
    $requiredProperty = Get-InstanceProperty -Object $Schema -Name 'required'
    if ($type -ceq 'object' -or $propertiesProperty.Exists -or $requiredProperty.Exists) {
        $propertyNames = if ($propertiesProperty.Exists) {
            @(Get-OrdinalSortedStrings -Values @($propertiesProperty.Value.PSObject.Properties.Name))
        }
        else { @() }
        $required = if ($requiredProperty.Exists) {
            @(Get-OrdinalSortedStrings -Values @($requiredProperty.Value))
        }
        else { @() }
        $requiredSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        foreach ($name in $required) { [void]$requiredSet.Add($name) }
        $optional = @(Get-OrdinalSortedStrings -Values @($propertyNames | Where-Object { -not $requiredSet.Contains($_) }))
        $additionalProperty = Get-InstanceProperty -Object $Schema -Name 'additionalProperties'
        $additional = if (-not $additionalProperty.Exists -or [bool]$additionalProperty.Value) { 'allowed' } else { 'forbidden' }
        [void]$Nodes.Add([PSCustomObject][ordered]@{
            path = $Path
            required_fields = @($required)
            optional_fields = @($optional)
            additional_properties = $additional
        })
    }
    if ($propertiesProperty.Exists) {
        foreach ($name in @(Get-OrdinalSortedStrings -Values @($propertiesProperty.Value.PSObject.Properties.Name))) {
            Add-SchemaObjectFieldGraphNodes `
                -Schema $propertiesProperty.Value.PSObject.Properties[$name].Value `
                -Path "$Path.$name" `
                -Nodes $Nodes
        }
    }
    $itemsProperty = Get-InstanceProperty -Object $Schema -Name 'items'
    if ($itemsProperty.Exists) {
        Add-SchemaObjectFieldGraphNodes -Schema $itemsProperty.Value -Path "$Path[]" -Nodes $Nodes
    }
}

function Get-SchemaObjectFieldGraph {
    param($Schema)

    $nodes = [System.Collections.Generic.List[object]]::new()
    Add-SchemaObjectFieldGraphNodes -Schema $Schema -Path '$' -Nodes $nodes
    return @($nodes)
}

function Get-FieldGraphSignature {
    param([object[]]$Nodes)

    $rows = [System.Collections.Generic.List[string]]::new()
    foreach ($node in @($Nodes | Sort-Object -Property @{ Expression = { [string]$_.path }; Ascending = $true })) {
        $required = @(Get-OrdinalSortedStrings -Values @($node.required_fields))
        $optional = @(Get-OrdinalSortedStrings -Values @($node.optional_fields))
        [void]$rows.Add("$($node.path)`t$($required -join ',')`t$($optional -join ',')`t$($node.additional_properties)")
    }
    return $rows -join "`n"
}

function Get-SchemaContractExpectations {
    return @(
        [PSCustomObject]@{
            Name = 'fixture-manifest'
            Path = 'specs/fixture-manifest.schema.json'
            SchemaId = 'https://internal.gnczmkn/schemas/fixture-manifest/1'
            InstanceVersion = 'gnczmkn.fixture-manifest/1'
            Bytes = [long]2499
            Sha256 = 'd532903919d6583e7615b5598772f3b4576761f60bfe3118c2b108de03f91c56'
            FieldGraph = @(
                [PSCustomObject]@{ path = '$'; required_fields = @('acceptance', 'authority', 'expected_facts', 'fixture_id', 'provenance', 'purpose', 'required_artifacts', 'schema_version', 'status'); optional_fields = @('open_tasks'); additional_properties = 'forbidden' },
                [PSCustomObject]@{ path = '$.expected_facts[]'; required_fields = @('claim', 'id', 'tolerance_policy'); optional_fields = @('evidence_refs'); additional_properties = 'forbidden' },
                [PSCustomObject]@{ path = '$.provenance'; required_fields = @('source_refs'); optional_fields = @('notes'); additional_properties = 'forbidden' })
        },
        [PSCustomObject]@{
            Name = 'oracle-manifest'
            Path = 'specs/oracle-manifest.schema.json'
            SchemaId = 'https://internal.gnczmkn/schemas/oracle-manifest/1'
            InstanceVersion = 'gnczmkn.oracle-manifest/1'
            Bytes = [long]2526
            Sha256 = '2818ce6169105e49b563ca995cd4becc2797267220bdc332711d0751fabf7ca9'
            FieldGraph = @(
                [PSCustomObject]@{ path = '$'; required_fields = @('oracle_set_id', 'oracles', 'provenance', 'schema_version', 'status'); optional_fields = @(); additional_properties = 'forbidden' },
                [PSCustomObject]@{ path = '$.oracles[]'; required_fields = @('claim', 'expected_facts', 'id', 'level', 'reference_kind', 'source_refs', 'status', 'tolerance_policy'); optional_fields = @('artifact_refs'); additional_properties = 'forbidden' },
                [PSCustomObject]@{ path = '$.provenance'; required_fields = @('source_refs'); optional_fields = @('notes'); additional_properties = 'forbidden' })
        },
        [PSCustomObject]@{
            Name = 'plan-proof-record'
            Path = 'specs/plan-proof-record.schema.json'
            SchemaId = 'https://internal.gnczmkn/schemas/plan-proof-record/1'
            InstanceVersion = 'gnczmkn.plan-proof-record/1'
            Bytes = [long]2351
            Sha256 = 'f2047ff0369576ceae90d1b9556b85724daa487f89b83dc5fa4b4b3cbe5a0c78'
            FieldGraph = @(
                [PSCustomObject]@{ path = '$'; required_fields = @('assertion_code', 'diagnostic_ids', 'generated_by_pass', 'lowered_operator_refs', 'premises', 'proof_id', 'proof_kind', 'result', 'schema_version', 'source_refs', 'subject_refs'); optional_fields = @(); additional_properties = 'forbidden' },
                [PSCustomObject]@{ path = '$.premises'; required_fields = @(); optional_fields = @(); additional_properties = 'allowed' })
        })
}

function Get-ContractLockErrors {
    param(
        $LockContract,
        $Schema,
        [long]$RawByteLength,
        [string]$RawSha256,
        $Expected
    )

    $errors = [System.Collections.Generic.List[string]]::new()
    Add-ExactPropertyErrors -Object $LockContract -Required @(
        'name', 'schema_path', 'schema_id', 'instance_version', 'schema_maturity',
        'raw_byte_length', 'raw_sha256', 'field_graph') -Label "Schema lock contract '$($Expected.Name)'" -Errors $errors
    if ([string](Get-PropertyValue $LockContract 'name') -cne $Expected.Name -or
        [string](Get-PropertyValue $LockContract 'schema_path') -cne $Expected.Path) {
        [void]$errors.Add("Schema lock contract '$($Expected.Name)' identity/path differs from the v1 lock")
    }
    if ([string](Get-PropertyValue $LockContract 'schema_id') -cne $Expected.SchemaId -or
        [string](Get-PropertyValue $Schema '$id') -cne $Expected.SchemaId) {
        [void]$errors.Add("Schema lock contract '$($Expected.Name)' `$id differs from the frozen v1 identity")
    }
    $schemaProperties = Get-PropertyValue $Schema 'properties'
    $versionSchema = if ($null -ne $schemaProperties) { Get-PropertyValue $schemaProperties 'schema_version' } else { $null }
    if ([string](Get-PropertyValue $LockContract 'instance_version') -cne $Expected.InstanceVersion -or
        [string](Get-PropertyValue $versionSchema 'const') -cne $Expected.InstanceVersion) {
        [void]$errors.Add("Schema lock contract '$($Expected.Name)' instance version differs from the frozen v1 identity")
    }
    if ([string](Get-PropertyValue $LockContract 'schema_maturity') -cne 'Fixture' -or
        [string](Get-PropertyValue $Schema 'x-maturity') -cne 'Fixture') {
        [void]$errors.Add("Schema lock contract '$($Expected.Name)' maturity must remain Fixture")
    }
    if ([long](Get-PropertyValue $LockContract 'raw_byte_length') -ne $Expected.Bytes -or
        $RawByteLength -ne $Expected.Bytes) {
        [void]$errors.Add("Schema lock contract '$($Expected.Name)' raw byte length differs from the frozen v1 bytes")
    }
    if ([string](Get-PropertyValue $LockContract 'raw_sha256') -cne $Expected.Sha256 -or
        $RawSha256 -cne $Expected.Sha256) {
        [void]$errors.Add("Schema lock contract '$($Expected.Name)' raw SHA-256 differs from the frozen v1 bytes")
    }
    $expectedGraph = Get-FieldGraphSignature -Nodes @($Expected.FieldGraph)
    $lockGraph = Get-FieldGraphSignature -Nodes @((Get-PropertyValue $LockContract 'field_graph'))
    $schemaGraph = Get-FieldGraphSignature -Nodes @(Get-SchemaObjectFieldGraph -Schema $Schema)
    if ($lockGraph -cne $expectedGraph -or $schemaGraph -cne $expectedGraph) {
        [void]$errors.Add("Schema lock contract '$($Expected.Name)' field graph differs from the frozen v1 graph")
    }
    return @($errors)
}

function Get-SchemaLockErrors {
    param($Lock)

    $errors = [System.Collections.Generic.List[string]]::new()
    Add-ExactPropertyErrors -Object $Lock -Required @(
        'schema_version', 'lock_id', 'task_id', 'adr_ref', 'decision_status', 'maturity',
        'reconciliation_decisions', 'contracts', 'reference_policy', 'plan_proof_v1_policy',
        'consumer_policy', 'v2_migration_policy') -Label 'R0 schema contract lock' -Errors $errors
    if ([string](Get-PropertyValue $Lock 'schema_version') -cne 'gnczmkn.r0-schema-contract-lock/1' -or
        [string](Get-PropertyValue $Lock 'lock_id') -cne 'R0-SPEC-001-V1-CONTRACT-LOCK' -or
        [string](Get-PropertyValue $Lock 'task_id') -cne 'R0-SPEC-001' -or
        [string](Get-PropertyValue $Lock 'adr_ref') -cne 'docs/adr/0004-r0-json-schema-contracts.md' -or
        [string](Get-PropertyValue $Lock 'decision_status') -cne 'proposed' -or
        [string](Get-PropertyValue $Lock 'maturity') -cne 'governance-contract-no-runtime-consumer') {
        [void]$errors.Add('R0 schema contract lock identity, status or maturity is invalid')
    }

    $expectedDecisions = [ordered]@{
        'RECON-DEC-001' = 'keep-current'
        'RECON-DEC-002' = 'repository-root-only'
        'RECON-DEC-003' = 'keep-current'
    }
    $decisionMap = @{}
    foreach ($decision in @((Get-PropertyValue $Lock 'reconciliation_decisions'))) {
        Add-ExactPropertyErrors -Object $decision -Required @('id', 'proposed_disposition', 'boundary') -Label 'R0 schema reconciliation decision' -Errors $errors
        $id = [string](Get-PropertyValue $decision 'id')
        if ($decisionMap.ContainsKey($id)) { [void]$errors.Add("Duplicate schema reconciliation decision '$id'") }
        else { $decisionMap[$id] = $decision }
    }
    foreach ($id in $expectedDecisions.Keys) {
        if (-not $decisionMap.ContainsKey($id) -or
            [string](Get-PropertyValue $decisionMap[$id] 'proposed_disposition') -cne [string]$expectedDecisions[$id] -or
            [string]::IsNullOrWhiteSpace([string](Get-PropertyValue $decisionMap[$id] 'boundary'))) {
            [void]$errors.Add("Schema reconciliation decision '$id' is missing or differs from the proposed disposition")
        }
    }
    if ($decisionMap.Count -ne $expectedDecisions.Count) {
        [void]$errors.Add('R0 schema reconciliation decision set must contain exactly RECON-DEC-001 through RECON-DEC-003')
    }

    $contractMap = @{}
    foreach ($contract in @((Get-PropertyValue $Lock 'contracts'))) {
        $name = [string](Get-PropertyValue $contract 'name')
        if ($contractMap.ContainsKey($name)) { [void]$errors.Add("Duplicate schema lock contract '$name'") }
        else { $contractMap[$name] = $contract }
    }
    $expectations = @(Get-SchemaContractExpectations)
    foreach ($expected in $expectations) {
        if (-not $contractMap.ContainsKey($expected.Name)) {
            [void]$errors.Add("Schema lock is missing contract '$($expected.Name)'")
            continue
        }
        $schemaPath = Join-Path $repoRoot $expected.Path
        try {
            $schema = Read-StrictJsonFile -Path $schemaPath
            foreach ($message in @(Get-ContractLockErrors `
                    -LockContract $contractMap[$expected.Name] `
                    -Schema $schema `
                    -RawByteLength ([System.IO.FileInfo]::new($schemaPath).Length) `
                    -RawSha256 (Get-RawSha256 -Path $schemaPath) `
                    -Expected $expected)) {
                [void]$errors.Add($message)
            }
        }
        catch {
            [void]$errors.Add("Schema lock cannot read '$($expected.Path)': $($_.Exception.Message)")
        }
    }
    if ($contractMap.Count -ne $expectations.Count) {
        [void]$errors.Add('R0 schema contract lock must contain exactly three v1 contracts')
    }

    $referencePolicy = Get-PropertyValue $Lock 'reference_policy'
    Add-ExactPropertyErrors -Object $referencePolicy -Required @(
        'executable_evidence_fields', 'locator_form', 'git_object_requirement',
        'nonempty_blob_required', 'nonempty_worktree_file_required', 'source_refs_semantics') `
        -Label 'R0 executable evidence reference policy' -Errors $errors
    if (-not (Test-ExactStringSequence -Actual @((Get-PropertyValue $referencePolicy 'executable_evidence_fields')) -Expected @(
                'fixture.required_artifacts[]', 'fixture.expected_facts[].evidence_refs[]', 'oracle.oracles[].artifact_refs[]')) -or
        [string](Get-PropertyValue $referencePolicy 'locator_form') -cne 'repository-root-relative-forward-slash' -or
        [string](Get-PropertyValue $referencePolicy 'git_object_requirement') -cne 'tracked-stage-0-regular-blob' -or
        (Get-PropertyValue $referencePolicy 'nonempty_blob_required') -ne $true -or
        (Get-PropertyValue $referencePolicy 'nonempty_worktree_file_required') -ne $true -or
        [string](Get-PropertyValue $referencePolicy 'source_refs_semantics') -cne 'opaque-provenance-locator') {
        [void]$errors.Add('R0 executable evidence reference policy differs from the repository-root-only boundary')
    }

    $proofPolicy = Get-PropertyValue $Lock 'plan_proof_v1_policy'
    Add-ExactPropertyErrors -Object $proofPolicy -Required @(
        'premises_representation', 'typed_prerequisite_graph', 'plan_proof_index') -Label 'PlanProofRecord v1 policy' -Errors $errors
    if ([string](Get-PropertyValue $proofPolicy 'premises_representation') -cne 'flat-object-map-scalar-snapshot' -or
        [string](Get-PropertyValue $proofPolicy 'typed_prerequisite_graph') -cne 'forbidden-in-v1' -or
        [string](Get-PropertyValue $proofPolicy 'plan_proof_index') -cne 'outside-v1-contract') {
        [void]$errors.Add('PlanProofRecord v1 object-map snapshot policy drifted')
    }

    $consumerPolicy = Get-PropertyValue $Lock 'consumer_policy'
    Add-ExactPropertyErrors -Object $consumerPolicy -Required @('runtime_consumers', 'forbidden_roots', 'allowed_uses') -Label 'R0 schema consumer policy' -Errors $errors
    if ([int](Get-PropertyValue $consumerPolicy 'runtime_consumers') -ne 0 -or
        -not (Test-ExactStringSequence -Actual @((Get-PropertyValue $consumerPolicy 'forbidden_roots')) -Expected @('framework', 'packages', 'adapters', 'apps', 'user')) -or
        -not (Test-ExactStringSequence -Actual @((Get-PropertyValue $consumerPolicy 'allowed_uses')) -Expected @('repository-validation', 'fixture-authoring', 'oracle-authoring', 'test-evidence', 'governance-review'))) {
        [void]$errors.Add('R0 schema lock must retain zero runtime consumers and the exact consumer boundary')
    }

    $migrationPolicy = Get-PropertyValue $Lock 'v2_migration_policy'
    Add-ExactPropertyErrors -Object $migrationPolicy -Required @('trigger', 'requirements') -Label 'R0 schema v2 migration policy' -Errors $errors
    if ([string]::IsNullOrWhiteSpace([string](Get-PropertyValue $migrationPolicy 'trigger')) -or
        -not (Test-ExactStringSequence -Actual @((Get-PropertyValue $migrationPolicy 'requirements')) -Expected @(
                'new-schema-id-and-instance-version', 'v1-to-v2-migration', 'dual-version-positive-and-negative-fixtures',
                'independent-validator-conformance', 'consumer-evidence', 'superseding-adr'))) {
        [void]$errors.Add('R0 schema v2 migration policy is incomplete')
    }
    return @($errors)
}

function Get-PublicConsumerErrors {
    param([object[]]$Documents = @())

    if ($Documents.Count -eq 0) {
        $collected = [System.Collections.Generic.List[object]]::new()
        $extensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.ixx', '.mpp', '.json', '.yaml', '.yml', '.cmake')
        foreach ($rootName in @('framework', 'packages', 'adapters', 'apps', 'user')) {
            $rootPath = Join-Path $repoRoot $rootName
            if (-not (Test-Path -LiteralPath $rootPath -PathType Container)) { continue }
            foreach ($file in @(Get-ChildItem -LiteralPath $rootPath -Recurse -File)) {
                if ($file.Extension.ToLowerInvariant() -notin $extensions -and $file.Name -cne 'CMakeLists.txt') { continue }
                [void]$collected.Add([PSCustomObject]@{
                    Path = $file.FullName.Substring($repoRootFull.Length + 1).Replace('\', '/')
                    Text = Get-Content -LiteralPath $file.FullName -Raw -Encoding utf8
                })
            }
        }
        $Documents = @($collected)
    }

    $tokens = @(
        'specs/fixture-manifest.schema.json',
        'specs/oracle-manifest.schema.json',
        'specs/plan-proof-record.schema.json',
        'specs/r0-schema-contract-lock.json',
        'https://internal.gnczmkn/schemas/fixture-manifest/1',
        'https://internal.gnczmkn/schemas/oracle-manifest/1',
        'https://internal.gnczmkn/schemas/plan-proof-record/1',
        'gnczmkn.fixture-manifest/1',
        'gnczmkn.oracle-manifest/1',
        'gnczmkn.plan-proof-record/1')
    $errors = [System.Collections.Generic.List[string]]::new()
    foreach ($document in $Documents) {
        foreach ($token in $tokens) {
            if ([string]$document.Text -and ([string]$document.Text).IndexOf($token, [System.StringComparison]::Ordinal) -ge 0) {
                [void]$errors.Add("R0 Fixture schema has forbidden product/public consumer '$($document.Path)' via token '$token'")
                break
            }
        }
    }
    return @($errors)
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
                $locator = Test-RepositoryEvidenceLocator -Reference ([string]$artifactRef)
                if (-not $locator.IsValid) {
                    [void]$errors.Add("$.required_artifacts[$artifactIndex]: $($locator.Diagnostic)")
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
                        $locator = Test-RepositoryEvidenceLocator -Reference ([string]$evidenceRef)
                        if (-not $locator.IsValid) {
                            [void]$errors.Add("$.expected_facts[$factIndex].evidence_refs[$evidenceIndex]: $($locator.Diagnostic)")
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
                        $locator = Test-RepositoryEvidenceLocator -Reference ([string]$artifactRef)
                        if (-not $locator.IsValid) {
                            [void]$errors.Add("$.oracles[$oracleIndex].artifact_refs[$artifactIndex]: $($locator.Diagnostic)")
                        }
                        ++$artifactIndex
                    }
                }
            }
            ++$oracleIndex
        }
    }
    elseif ($SchemaName -eq 'plan-proof-record.schema.json') {
        foreach ($message in @(Get-PlanProofPremiseErrors -Instance $instance)) {
            [void]$errors.Add($message)
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

function Add-ExpectedMutationResult {
    param(
        [string]$Name,
        [object[]]$Messages,
        [string]$ExpectedErrorContains,
        [System.Collections.Generic.List[string]]$Failures
    )

    $combined = @($Messages | ForEach-Object { [string]$_ }) -join ' | '
    if ($combined.IndexOf($ExpectedErrorContains, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
        [void]$Failures.Add("contract mutation was not rejected as expected: $Name :: $combined")
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
$contractMutationCaseCount = 0
$repositoryDocuments = [System.Collections.Generic.List[object]]::new()
$schemaLock = $null
try {
    $schemaLock = Read-StrictJsonFile -Path $schemaLockPath
    foreach ($message in @(Get-SchemaLockErrors -Lock $schemaLock)) {
        [void]$failures.Add("schema contract lock: $message")
    }
}
catch {
    [void]$failures.Add("schema contract lock cannot be read: $($_.Exception.Message)")
}
foreach ($message in @(Get-PublicConsumerErrors)) {
    [void]$failures.Add("schema consumer boundary: $message")
}
$invalidExpectedErrors = @{
    'fixture-manifest/duplicate-fact-id.json' = 'duplicate expected fact id'
    'fixture-manifest/missing-provenance.json' = "missing required property 'provenance'"
    'fixture-manifest/missing-tolerance.json' = "missing required property 'tolerance_policy'"
    'fixture-manifest/unknown-authority.json' = 'does not resolve in docs/team/role-assignments.json'
    'fixture-manifest/unresolved-open-task.json' = 'does not resolve in docs/tasks/backlog.json'
    'fixture-manifest/completed-open-task.json' = 'is already done'
    'fixture-manifest/executable-missing-evidence.json' = 'executable fact requires at least one evidence file'
    'fixture-manifest/executable-unresolved-artifact.json' = 'does not name an exact tracked stage-0 Git path'
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

$fixtureExpectation = @(Get-SchemaContractExpectations | Where-Object { $_.Name -ceq 'fixture-manifest' })[0]
$fixtureSchema = Read-StrictJsonFile -Path (Join-Path $repoRoot $fixtureExpectation.Path)
$fixtureLockContract = @($schemaLock.contracts | Where-Object { $_.name -ceq 'fixture-manifest' })[0]
$contractMutations = @(
    [PSCustomObject]@{
        Name = 'same-version-schema-id-drift'
        ExpectedErrorContains = 'frozen v1 identity'
        Evaluate = {
            $mutatedSchema = Copy-JsonValue $fixtureSchema
            $mutatedSchema.'$id' = 'urn:gnczmkn:schema:r0:fixture-manifest:1'
            @(Get-ContractLockErrors `
                -LockContract $fixtureLockContract `
                -Schema $mutatedSchema `
                -RawByteLength $fixtureExpectation.Bytes `
                -RawSha256 $fixtureExpectation.Sha256 `
                -Expected $fixtureExpectation)
        }
    },
    [PSCustomObject]@{
        Name = 'same-version-field-graph-drift'
        ExpectedErrorContains = 'field graph differs'
        Evaluate = {
            $mutatedSchema = Copy-JsonValue $fixtureSchema
            $mutatedSchema.properties | Add-Member -MemberType NoteProperty -Name 'oracle_refs' -Value ([PSCustomObject]@{ type = 'array' })
            @(Get-ContractLockErrors `
                -LockContract $fixtureLockContract `
                -Schema $mutatedSchema `
                -RawByteLength $fixtureExpectation.Bytes `
                -RawSha256 $fixtureExpectation.Sha256 `
                -Expected $fixtureExpectation)
        }
    },
    [PSCustomObject]@{
        Name = 'same-version-raw-byte-drift'
        ExpectedErrorContains = 'raw SHA-256 differs'
        Evaluate = {
            @(Get-ContractLockErrors `
                -LockContract $fixtureLockContract `
                -Schema $fixtureSchema `
                -RawByteLength $fixtureExpectation.Bytes `
                -RawSha256 ('0' * 64) `
                -Expected $fixtureExpectation)
        }
    },
    [PSCustomObject]@{
        Name = 'manifest-relative-evidence-locator'
        ExpectedErrorContains = 'does not name an exact tracked stage-0 Git path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference 'conventions.json').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'rooted-evidence-locator'
        ExpectedErrorContains = 'repository-root-relative path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference '/specs/README.md').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'backslash-evidence-locator'
        ExpectedErrorContains = 'repository-root-relative path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference 'specs\README.md').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'drive-relative-evidence-locator'
        ExpectedErrorContains = 'repository-root-relative path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference 'C:specs/README.md').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'file-uri-evidence-locator'
        ExpectedErrorContains = 'repository-root-relative path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference 'file:/specs/README.md').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'escaping-evidence-locator'
        ExpectedErrorContains = 'repository-root-relative path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference '../README.md').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'query-evidence-locator'
        ExpectedErrorContains = 'repository-root-relative path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference 'specs/README.md?view=raw').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'fragment-evidence-locator'
        ExpectedErrorContains = 'repository-root-relative path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference 'specs/README.md#contract').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'percent-encoded-evidence-locator'
        ExpectedErrorContains = 'does not name an exact tracked stage-0 Git path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference 'specs/%52EADME.md').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'untracked-evidence-locator'
        ExpectedErrorContains = 'does not name an exact tracked stage-0 Git path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference 'specs/untracked-evidence.txt').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'case-alias-evidence-locator'
        ExpectedErrorContains = 'does not name an exact tracked stage-0 Git path'
        Evaluate = { @((Test-RepositoryEvidenceLocator -Reference 'Specs/README.md').Diagnostic) }
    },
    [PSCustomObject]@{
        Name = 'empty-evidence-blob'
        ExpectedErrorContains = 'nonempty Git blob'
        Evaluate = {
            $fact = [PSCustomObject]@{
                Tracked = $true; ExactPath = $true; Mode = '100644'; ObjectType = 'blob'
                BlobBytes = [long]0; WorktreeRegular = $true; WorktreeBytes = [long]1
            }
            @((Test-RepositoryEvidenceLocator -Reference 'specs/README.md' -BlobFact $fact).Diagnostic)
        }
    },
    [PSCustomObject]@{
        Name = 'nonregular-evidence-blob'
        ExpectedErrorContains = 'regular Git blob and regular worktree file'
        Evaluate = {
            $fact = [PSCustomObject]@{
                Tracked = $true; ExactPath = $true; Mode = '120000'; ObjectType = 'blob'
                BlobBytes = [long]12; WorktreeRegular = $false; WorktreeBytes = [long]0
            }
            @((Test-RepositoryEvidenceLocator -Reference 'specs/README.md' -BlobFact $fact).Diagnostic)
        }
    },
    [PSCustomObject]@{
        Name = 'empty-evidence-worktree-file'
        ExpectedErrorContains = 'nonempty Git blob'
        Evaluate = {
            $fact = [PSCustomObject]@{
                Tracked = $true; ExactPath = $true; Mode = '100644'; ObjectType = 'blob'
                BlobBytes = [long]1; WorktreeRegular = $true; WorktreeBytes = [long]0
            }
            @((Test-RepositoryEvidenceLocator -Reference 'specs/README.md' -BlobFact $fact).Diagnostic)
        }
    },
    [PSCustomObject]@{
        Name = 'typed-proof-premise-overlay'
        ExpectedErrorContains = 'typed proof prerequisites require schema v2'
        Evaluate = {
            $proof = Copy-JsonValue $proofIdentityBase
            $proof.premises | Add-Member -MemberType NoteProperty -Name 'prerequisite_proofs' -Value @('proof:other')
            @(Get-PlanProofPremiseErrors -Instance $proof)
        }
    },
    [PSCustomObject]@{
        Name = 'fixture-schema-runtime-consumer'
        ExpectedErrorContains = 'forbidden product/public consumer'
        Evaluate = {
            @(Get-PublicConsumerErrors -Documents @(
                [PSCustomObject]@{
                    Path = 'framework/include/gnc/contracts/fixture_runtime.hpp'
                    Text = '#include "specs/fixture-manifest.schema.json"'
                }))
        }
    },
    [PSCustomObject]@{
        Name = 'fixture-lock-runtime-consumer-count'
        ExpectedErrorContains = 'zero runtime consumers'
        Evaluate = {
            $mutatedLock = Copy-JsonValue $schemaLock
            $mutatedLock.consumer_policy.runtime_consumers = 1
            @(Get-SchemaLockErrors -Lock $mutatedLock)
        }
    })

foreach ($mutationCase in $contractMutations) {
    ++$contractMutationCaseCount
    $messages = @(& $mutationCase.Evaluate)
    Add-ExpectedMutationResult `
        -Name $mutationCase.Name `
        -Messages $messages `
        -ExpectedErrorContains $mutationCase.ExpectedErrorContains `
        -Failures $failures
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
    Write-Host "Validated schema lock/locator/consumer mutations: $contractMutationCaseCount"
}

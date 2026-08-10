[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$errors = [System.Collections.Generic.List[string]]::new()

function Add-ValidationError([string]$Message) {
    [void]$script:errors.Add($Message)
}

function Get-Field($Object, [string]$Name, [string]$Context) {
    if ($null -eq $Object) {
        Add-ValidationError "$Context is missing object '$Name'."
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        Add-ValidationError "$Context is missing '$Name'."
        return $null
    }
    return $property.Value
}

function Require-Text($Object, [string]$Name, [string]$Context) {
    $value = Get-Field $Object $Name $Context
    if ([string]::IsNullOrWhiteSpace([string]$value)) {
        Add-ValidationError "$Context field '$Name' must be non-empty text."
        return $null
    }
    return [string]$value
}

function Require-Array($Object, [string]$Name, [string]$Context, [bool]$AllowEmpty = $false) {
    $value = Get-Field $Object $Name $Context
    if ($null -eq $value) {
        return @()
    }

    $items = @($value)
    if (-not $AllowEmpty -and $items.Count -eq 0) {
        Add-ValidationError "$Context field '$Name' must contain at least one item."
    }
    foreach ($item in $items) {
        if ([string]::IsNullOrWhiteSpace([string]$item)) {
            Add-ValidationError "$Context field '$Name' contains an empty item."
        }
    }
    return $items
}

try {
    $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $document = Get-Content -LiteralPath $resolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
}
catch {
    Write-Host "Provenance validation failed: $($_.Exception.Message)"
    exit 1
}

$schemaVersion = Require-Text $document 'schema_version' 'register'
if ($null -ne $schemaVersion -and $schemaVersion -ne 'gnczmkn.provenance-register/1') {
    Add-ValidationError "register has unsupported schema_version '$schemaVersion'."
}
[void](Require-Text $document 'updated_at' 'register')
[void](Require-Text $document 'policy_ref' 'register')
$recordsValue = Get-Field $document 'records' 'register'
[array]$records = if ($null -eq $recordsValue) { @() } else { @($recordsValue) }
if ($records.Count -eq 0) {
    Add-ValidationError 'register must contain at least one provenance record.'
}

$recordById = @{}
$subjectTypes = @(
    'source-code', 'documentation', 'dependency', 'tool', 'formula', 'parameter',
    'dataset', 'asset', 'model', 'model-asset', 'table', 'coefficient-set',
    'fixture', 'scientific-fixture', 'reference-archive', 'template',
    'generated-artifact', 'evidence-bundle'
)
$alwaysScientificTypes = @(
    'formula', 'parameter', 'dataset', 'model', 'model-asset', 'table',
    'coefficient-set', 'scientific-fixture'
)
$generatedTypes = @('generated-artifact', 'evidence-bundle')
$rightsStatuses = @('cleared', 'restricted', 'unreviewed')
$reviewDecisions = @('approved-internal-only', 'approved-external', 'blocked')
$classificationRanks = @{
    'public' = 0
    'public-candidate' = 1
    'project' = 2
    'internal-research' = 3
    'internal-restricted' = 4
}

foreach ($record in $records) {
    $recordId = Require-Text $record 'record_id' 'record'
    $context = if ($null -eq $recordId) { 'record' } else { "record '$recordId'" }
    if ($null -ne $recordId) {
        if ($recordById.ContainsKey($recordId)) {
            Add-ValidationError "Duplicate provenance record_id '$recordId'."
        }
        else {
            $recordById[$recordId] = $record
        }
    }

    $subject = Get-Field $record 'subject' $context
    $subjectType = Require-Text $subject 'type' "$context subject"
    if ($null -ne $subjectType -and $subjectType -notin $subjectTypes) {
        Add-ValidationError "$context subject type '$subjectType' is unsupported."
    }
    $scientificRequiredProperty = if ($null -eq $subject) { $null } else { $subject.PSObject.Properties['scientific_context_required'] }
    $scientificContextRequired = $null
    if ($null -eq $scientificRequiredProperty -or $scientificRequiredProperty.Value -isnot [bool]) {
        Add-ValidationError "$context subject field 'scientific_context_required' must be boolean."
    }
    else {
        $scientificContextRequired = [bool]$scientificRequiredProperty.Value
    }
    if ($subjectType -in $alwaysScientificTypes -and $scientificContextRequired -ne $true) {
        Add-ValidationError "$context subject type '$subjectType' requires scientific_context_required=true."
    }
    [void](Require-Text $subject 'identity' "$context subject")
    [void](Require-Text $subject 'path_or_uri' "$context subject")

    $origin = Get-Field $record 'origin' $context
    [void](Require-Text $origin 'kind' "$context origin")
    [void](Require-Text $origin 'source' "$context origin")
    [void](Require-Text $origin 'version_or_date' "$context origin")

    $integrity = Get-Field $record 'integrity' $context
    [void](Require-Text $integrity 'algorithm' "$context integrity")
    [void](Require-Text $integrity 'value' "$context integrity")

    $rights = Get-Field $record 'rights' $context
    $rightsStatus = Require-Text $rights 'status' "$context rights"
    if ($null -ne $rightsStatus -and $rightsStatus -notin $rightsStatuses) {
        Add-ValidationError "$context rights status '$rightsStatus' is unsupported."
    }
    [void](Require-Text $rights 'basis_ref' "$context rights")
    $allowedUses = Require-Array $rights 'allowed_uses' "$context rights"
    $prohibitedUses = Require-Array $rights 'prohibited_uses' "$context rights" ($rightsStatus -eq 'cleared')

    $licenseProperty = if ($null -eq $rights) { $null } else { $rights.PSObject.Properties['license_expression'] }
    $permissionProperty = if ($null -eq $rights) { $null } else { $rights.PSObject.Properties['permission_ref'] }
    $licenseExpression = if ($null -eq $licenseProperty) { '' } else { [string]$licenseProperty.Value }
    $permissionRef = if ($null -eq $permissionProperty) { '' } else { [string]$permissionProperty.Value }
    $normalizedLicenseExpression = $licenseExpression.Trim().ToUpperInvariant()
    if ($rightsStatus -eq 'cleared' -and $normalizedLicenseExpression -in @('NOASSERTION', 'NONE')) {
        Add-ValidationError "$context license_expression '$licenseExpression' cannot authorize external sharing."
    }
    if ($rightsStatus -eq 'cleared' -and
        [string]::IsNullOrWhiteSpace($licenseExpression) -and
        [string]::IsNullOrWhiteSpace($permissionRef)) {
        Add-ValidationError "$context cleared rights require license_expression or permission_ref."
    }
    if ($rightsStatus -in @('restricted', 'unreviewed') -and 'external-publication' -notin $prohibitedUses) {
        Add-ValidationError "$context restricted or unreviewed rights must prohibit external-publication."
    }

    $classification = Require-Text $record 'classification' $context
    if ($null -ne $classification -and -not $classificationRanks.ContainsKey($classification)) {
        Add-ValidationError "$context classification '$classification' is unsupported."
    }
    $propagation = Get-Field $record 'propagation' $context
    $inheritProperty = if ($null -eq $propagation) { $null } else { $propagation.PSObject.Properties['inherit_restrictions'] }
    $inheritRestrictions = $null
    if ($null -eq $inheritProperty -or $inheritProperty.Value -isnot [bool]) {
        Add-ValidationError "$context propagation field 'inherit_restrictions' must be boolean."
    }
    else {
        $inheritRestrictions = [bool]$inheritProperty.Value
    }
    [array]$propagationRequirements = @(Require-Array $propagation 'requirements' "$context propagation")
    if ($rightsStatus -in @('restricted', 'unreviewed') -and $inheritRestrictions -ne $true) {
        Add-ValidationError "$context restricted or unreviewed rights require inherit_restrictions=true."
    }
    if ($rightsStatus -in @('restricted', 'unreviewed') -and 'block-external-sharing' -notin $propagationRequirements) {
        Add-ValidationError "$context restricted or unreviewed rights must propagate block-external-sharing."
    }
    if ($subjectType -in $generatedTypes) {
        [void](Require-Array $propagation 'upstream_record_refs' "$context propagation")
        [void](Require-Array $propagation 'lineage_refs' "$context propagation")
        if ($inheritRestrictions -ne $true) {
            Add-ValidationError "$context generated output requires inherit_restrictions=true."
        }
    }

    $review = Get-Field $record 'review' $context
    $reviewDecision = Require-Text $review 'decision' "$context review"
    if ($null -ne $reviewDecision -and $reviewDecision -notin $reviewDecisions) {
        Add-ValidationError "$context review decision '$reviewDecision' is unsupported."
    }
    [void](Require-Text $review 'reviewer' "$context review")
    [void](Require-Text $review 'reviewed_at' "$context review")

    if ($reviewDecision -eq 'approved-external') {
        if ($rightsStatus -ne 'cleared') {
            Add-ValidationError "$context approved-external requires rights.status=cleared."
        }
        if ('external-publication' -notin $allowedUses) {
            Add-ValidationError "$context approved-external must allow external-publication."
        }
    }

    $scientificProperty = $record.PSObject.Properties['scientific_context']
    if ($scientificContextRequired -eq $true) {
        $scientific = if ($null -eq $scientificProperty) { Get-Field $record 'scientific_context' $context } else { $scientificProperty.Value }
        [void](Require-Text $scientific 'applicable_domain' "$context scientific_context")
        [void](Require-Text $scientific 'units' "$context scientific_context")
        [void](Require-Text $scientific 'frames' "$context scientific_context")
        [void](Require-Text $scientific 'time_convention' "$context scientific_context")
        [void](Require-Array $scientific 'reference_or_oracle_refs' "$context scientific_context")
        $independentProperty = if ($null -eq $scientific) { $null } else { $scientific.PSObject.Properties['independent_reference_confirmed'] }
        if ($null -eq $independentProperty -or $independentProperty.Value -isnot [bool] -or -not [bool]$independentProperty.Value) {
            Add-ValidationError "$context scientific_context requires independent_reference_confirmed=true."
        }
        [void](Require-Text $scientific 'independence_basis' "$context scientific_context")
        [void](Require-Text $scientific 'target_identity' "$context scientific_context")
    }
    elseif ($null -ne $scientificProperty) {
        Add-ValidationError "$context provides scientific_context while scientific_context_required is false."
    }
}

foreach ($recordId in $recordById.Keys) {
    $record = $recordById[$recordId]
    $subjectProperty = $record.PSObject.Properties['subject']
    if ($null -eq $subjectProperty) { continue }
    $subject = $subjectProperty.Value
    $subjectTypeProperty = $subject.PSObject.Properties['type']
    if ($null -eq $subjectTypeProperty) { continue }
    $subjectType = [string]$subjectTypeProperty.Value
    if ($subjectType -notin $generatedTypes) { continue }

    $context = "record '$recordId'"
    $propagationProperty = $record.PSObject.Properties['propagation']
    if ($null -eq $propagationProperty) { continue }
    $propagation = $propagationProperty.Value
    $upstreamProperty = $propagation.PSObject.Properties['upstream_record_refs']
    [array]$upstreamRefs = if ($null -eq $upstreamProperty) { @() } else { @($upstreamProperty.Value) }
    $requirementsProperty = $propagation.PSObject.Properties['requirements']
    [array]$requirements = if ($null -eq $requirementsProperty) { @() } else { @($requirementsProperty.Value) }
    $classificationProperty = $record.PSObject.Properties['classification']
    $classification = if ($null -eq $classificationProperty) { '' } else { [string]$classificationProperty.Value }
    $reviewProperty = $record.PSObject.Properties['review']
    $reviewDecisionProperty = if ($null -eq $reviewProperty) { $null } else { $reviewProperty.Value.PSObject.Properties['decision'] }
    $reviewDecision = if ($null -eq $reviewDecisionProperty) { '' } else { [string]$reviewDecisionProperty.Value }

    foreach ($upstreamRef in $upstreamRefs) {
        if (-not $recordById.ContainsKey([string]$upstreamRef)) {
            Add-ValidationError "$context references missing upstream provenance record '$upstreamRef'."
            continue
        }

        $upstream = $recordById[[string]$upstreamRef]
        $upstreamClassificationProperty = $upstream.PSObject.Properties['classification']
        $upstreamClassification = if ($null -eq $upstreamClassificationProperty) { '' } else { [string]$upstreamClassificationProperty.Value }
        if ($classificationRanks.ContainsKey($classification) -and
            $classificationRanks.ContainsKey($upstreamClassification) -and
            $classificationRanks[$classification] -lt $classificationRanks[$upstreamClassification]) {
            Add-ValidationError "$context classification '$classification' is weaker than upstream '$upstreamClassification'."
        }

        $upstreamPropagationProperty = $upstream.PSObject.Properties['propagation']
        $upstreamRequirementsProperty = if ($null -eq $upstreamPropagationProperty) { $null } else { $upstreamPropagationProperty.Value.PSObject.Properties['requirements'] }
        [array]$upstreamRequirements = if ($null -eq $upstreamRequirementsProperty) { @() } else { @($upstreamRequirementsProperty.Value) }
        foreach ($upstreamRequirement in $upstreamRequirements) {
            if ($upstreamRequirement -notin $requirements) {
                Add-ValidationError "$context omits upstream propagation requirement '$upstreamRequirement'."
            }
        }

        $upstreamRightsProperty = $upstream.PSObject.Properties['rights']
        $upstreamRightsStatusProperty = if ($null -eq $upstreamRightsProperty) { $null } else { $upstreamRightsProperty.Value.PSObject.Properties['status'] }
        $upstreamRightsStatus = if ($null -eq $upstreamRightsStatusProperty) { '' } else { [string]$upstreamRightsStatusProperty.Value }
        if ($upstreamRightsStatus -ne 'cleared' -and $reviewDecision -eq 'approved-external') {
            Add-ValidationError "$context cannot be approved-external while upstream '$upstreamRef' is not cleared."
        }
    }
}

if ($errors.Count -gt 0) {
    Write-Host "Provenance validation failed with $($errors.Count) issue(s):"
    foreach ($message in $errors) {
        Write-Host " - $message"
    }
    exit 1
}

Write-Host "Provenance validation passed: $($records.Count) record(s)."

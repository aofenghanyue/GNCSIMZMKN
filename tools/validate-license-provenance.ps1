[CmdletBinding()]
param(
    [switch]$UpdateReport,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$inventoryPath = Join-Path $repoRoot 'docs\governance\provenance-inventory.json'
$reportPath = Join-Path $repoRoot 'docs\quality\license-provenance-conformance-report.json'
$errors = [System.Collections.Generic.List[string]]::new()
$mutationResults = [System.Collections.Generic.List[object]]::new()
Import-Module (Join-Path $repoRoot 'tools\modules\JsonSchemaSubset.psm1') -Force
Import-Module (Join-Path $repoRoot 'tools\modules\R0GovernanceReview.psm1') -Force

function Add-Error([string]$Message) {
    $script:errors.Add($Message)
}

function Read-Json([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-Error "Required JSON is missing: $Path"
        return $null
    }
    try {
        return Read-StrictJsonFile -Path $Path
    }
    catch {
        Add-Error "Invalid JSON: $Path :: $($_.Exception.Message)"
        return $null
    }
}

function Get-Field([object]$Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Test-HasField([object]$Object, [string]$Name) {
    return $null -ne $Object -and $null -ne $Object.PSObject.Properties[$Name]
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-NormalizedTextSha256([string]$Path) {
    $text = [System.IO.File]::ReadAllText(
        $Path, [System.Text.UTF8Encoding]::new($false, $true))
    $normalized = $text.Replace("`r`n", "`n").Replace("`r", "`n")
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($normalized)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString(
                $algorithm.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Get-ObjectCanonicalSha256([object]$Value) {
    $convertToJsonParameters = @{
        Depth = 100
        Compress = $true
    }
    # Windows PowerShell 5.1 defaults to EscapeHtml; align PowerShell 7 explicitly.
    if ((Get-Command ConvertTo-Json).Parameters.ContainsKey('EscapeHandling')) {
        $convertToJsonParameters.EscapeHandling = 'EscapeHtml'
    }
    $json = $Value | ConvertTo-Json @convertToJsonParameters
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($json)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString(
                $algorithm.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Test-RepositoryRelativeLocator([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value) -or
        $Value.Contains('\') -or
        $Value.Contains('%') -or
        $Value.Contains('?') -or
        $Value.Contains('#') -or
        $Value -match '^[A-Za-z]:' -or
        $Value -match '^[A-Za-z][A-Za-z0-9+.-]*:' -or
        [System.IO.Path]::IsPathRooted($Value)) {
        return $false
    }
    $segments = @($Value.Split('/'))
    return $segments.Count -gt 0 -and
        @($segments | Where-Object { $_ -eq '' -or $_ -eq '.' -or $_ -eq '..' }).Count -eq 0
}

function Test-TrackedRepositoryLocator([string]$Value) {
    if (-not (Test-RepositoryRelativeLocator $Value)) {
        return $false
    }
    $stageLines = @(& git -C $repoRoot -c core.quotepath=false --literal-pathspecs `
        ls-files --stage -- $Value)
    if ($LASTEXITCODE -ne 0 -or $stageLines.Count -ne 1) {
        return $false
    }
    $stage = [string]$stageLines[0]
    if ($stage -cnotmatch '^(100(?:644|755))\s+([0-9a-f]{40,64})\s+0\t(.+)$' -or
        $Matches[3] -cne $Value) {
        return $false
    }
    $resolved = Join-Path $repoRoot $Value
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) { return $false }
    $item = Get-Item -LiteralPath $resolved -Force
    if ($item -isnot [System.IO.FileInfo] -or
        ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 -or
        $item.Length -le 0) { return $false }
    $rootFull = [System.IO.Path]::GetFullPath($repoRoot).TrimEnd('\', '/')
    $cursor = $item.Directory
    while ($null -ne $cursor -and
        $cursor.FullName.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        if (($cursor.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            return $false
        }
        if ($cursor.FullName -ceq $rootFull) { break }
        $cursor = $cursor.Parent
    }
    return $true
}

function Test-InventoryObject([object]$Inventory) {
    $issues = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $Inventory) {
        $issues.Add('Inventory is null.')
        return $issues.ToArray()
    }

    if ((Get-Field $Inventory 'schema_version') -ne 'gnczmkn.provenance-inventory/1') {
        $issues.Add('Unsupported provenance inventory schema.')
    }
    if ((Get-Field $Inventory 'maturity') -ne 'governance-evidence-no-runtime-consumer') {
        $issues.Add('Inventory maturity must remain governance-only during R0.')
    }
    if ((Get-Field $Inventory 'task_id') -ne 'R0-GOV-002') {
        $issues.Add('Inventory does not belong to R0-GOV-002.')
    }

    $policy = Get-Field $Inventory 'policy'
    if ((Get-Field $policy 'id') -ne 'GNC-LIC-PROV-001' -or
        (Get-Field $policy 'status') -ne 'proposed') {
        $issues.Add('Inventory policy identity/status differs from the Proposed policy.')
    }
    $decisionOwners = @(Get-Field $policy 'decision_owners')
    if (($decisionOwners -join ',') -ne 'product_owner,architecture_lead') {
        $issues.Add('License decision owners must be Product Owner and Architecture Lead.')
    }

    $repositoryLicense = Get-Field $Inventory 'repository_license'
    if ((Get-Field $repositoryLicense 'selected') -ne $false) {
        $issues.Add('Repository license cannot be selected while ADR-0008 is Proposed.')
    }
    if ((Get-Field $repositoryLicense 'detected_expression') -ne 'NONE' -or
        (Get-Field $repositoryLicense 'concluded_expression') -ne 'NOASSERTION') {
        $issues.Add('Repository license detection/conclusion must remain NONE/NOASSERTION.')
    }
    if ((Get-Field $repositoryLicense 'external_distribution') -ne
        'blocked-pending-accepted-adr') {
        $issues.Add('Repository external distribution must remain blocked.')
    }

    $vocabulary = Get-Field $Inventory 'vocabulary'
    $allowedScans = @(Get-Field $vocabulary 'scan_results')
    $allowedPresence = @(Get-Field $vocabulary 'repository_presence')
    $allowedInternal = @(Get-Field $vocabulary 'internal_handling')
    $allowedExternal = @(Get-Field $vocabulary 'external_distribution')
    $categoryClassifications = [System.Collections.Generic.Dictionary[string, string]]::new(
        [System.StringComparer]::Ordinal)
    $categoryClassifications.Add('repository-governed-content', 'internal-research')
    $categoryClassifications.Add('imported-design-source', 'internal-research')
    $categoryClassifications.Add('fixture-oracle-and-generated-evidence', 'internal-research')
    $categoryClassifications.Add('frozen-reference-archive', 'internal-research')
    $categoryClassifications.Add('external-build-dependency', 'external-dependency')
    $categoryClassifications.Add('external-tool-bundle', 'external-tool')
    $categoryClassifications.Add('external-validation-tools', 'external-tool')
    $categoryClassifications.Add('external-ci-action', 'external-tool')
    $allowedClassifications = @('internal-research', 'external-dependency', 'external-tool')
    $allowedIntegrityKinds = @(
        'per-file-git-object',
        'manifest-and-per-file-sha256',
        'sha256-zip-audit',
        'sha256',
        'environment-identity',
        'git-commit')
    if (($allowedScans -join ',') -cne
        'no-license-information-found,license-information-present,not-scanned-nonredistributed-tool') {
        $issues.Add('Provenance scan vocabulary differs from the reviewed technical inventory.')
    }
    if (($allowedPresence -join ',') -cne 'tracked,embedded-archive,external-untracked') {
        $issues.Add('Repository-presence vocabulary differs from the reviewed technical inventory.')
    }
    if (($allowedInternal -join ',') -cne
        'existing-access-only,evidence-only,subject-to-upstream-terms') {
        $issues.Add('Internal-handling vocabulary differs from the reviewed technical inventory.')
    }
    if (($allowedExternal -join ',') -cne
        'blocked-pending-accepted-adr,blocked-pending-item-review,not-redistributed') {
        $issues.Add('External-distribution vocabulary differs from the reviewed technical inventory.')
    }
    foreach ($expected in @(
            'no-license-information-found',
            'license-information-present',
            'not-scanned-nonredistributed-tool')) {
        if ($expected -notin $allowedScans) {
            $issues.Add("Missing scan vocabulary value: $expected")
        }
    }
    foreach ($expected in @('tracked', 'embedded-archive', 'external-untracked')) {
        if ($expected -notin $allowedPresence) {
            $issues.Add("Missing repository-presence vocabulary value: $expected")
        }
    }
    foreach ($expected in @('existing-access-only', 'evidence-only', 'subject-to-upstream-terms')) {
        if ($expected -notin $allowedInternal) {
            $issues.Add("Missing internal-handling vocabulary value: $expected")
        }
    }
    foreach ($expected in @(
            'blocked-pending-accepted-adr',
            'blocked-pending-item-review',
            'not-redistributed')) {
        if ($expected -notin $allowedExternal) {
            $issues.Add("Missing external-distribution vocabulary value: $expected")
        }
    }

    $items = @(Get-Field $Inventory 'items')
    if ($items.Count -eq 0) {
        $issues.Add('Inventory has no items.')
        return $issues.ToArray()
    }

    $requiredFields = @(
        'id', 'category', 'scope', 'owner_role', 'purpose', 'source_refs',
        'version', 'integrity', 'repository_presence', 'internal_handling',
        'external_distribution', 'classification', 'license', 'lineage_parents')
    $ids = @{}
    foreach ($item in $items) {
        foreach ($field in $requiredFields) {
            if (-not (Test-HasField $item $field)) {
                $issues.Add("Inventory item is missing required field '$field'.")
            }
        }

        $id = [string](Get-Field $item 'id')
        if ([string]::IsNullOrWhiteSpace($id)) {
            $issues.Add('Inventory item id is empty.')
        }
        elseif ($ids.ContainsKey($id)) {
            $issues.Add("Duplicate inventory item id: $id")
        }
        else {
            $ids[$id] = $item
        }

        $category = [string](Get-Field $item 'category')
        $classification = [string](Get-Field $item 'classification')
        if (-not $categoryClassifications.ContainsKey($category)) {
            $issues.Add("Inventory item '$id' has unsupported category '$category'.")
        }
        if ($classification -cnotin $allowedClassifications) {
            $issues.Add("Inventory item '$id' has unsupported classification '$classification'.")
        }
        elseif ($categoryClassifications.ContainsKey($category) -and
            $classification -cne [string]$categoryClassifications[$category]) {
            $issues.Add("Inventory item '$id' classification '$classification' is incompatible with category '$category'.")
        }

        $integrity = Get-Field $item 'integrity'
        $integrityKind = [string](Get-Field $integrity 'kind')
        if ($integrityKind -cnotin $allowedIntegrityKinds) {
            $issues.Add("Inventory item '$id' has unsupported integrity kind '$integrityKind'.")
        }
        elseif ($integrityKind -cin @(
                'per-file-git-object',
                'manifest-and-per-file-sha256',
                'environment-identity',
                'git-commit') -and
            [string]::IsNullOrWhiteSpace([string](Get-Field $integrity 'value'))) {
            $issues.Add("Inventory item '$id' integrity value is empty.")
        }
        elseif ($integrityKind -cin @('sha256-zip-audit', 'sha256')) {
            $integritySha = [string](Get-Field $integrity 'sha256')
            $integrityBytes = Get-Field $integrity 'bytes'
            if ($integritySha -cnotmatch '^[0-9a-f]{64}$') {
                $issues.Add("Inventory item '$id' has an invalid SHA-256 integrity value.")
            }
            if (($integrityBytes -isnot [int] -and $integrityBytes -isnot [long]) -or
                $integrityBytes -le 0) {
                $issues.Add("Inventory item '$id' has an invalid integrity byte count.")
            }
        }

        $sourceRefs = @(Get-Field $item 'source_refs')
        if ($sourceRefs.Count -eq 0) {
            $issues.Add("Inventory item '$id' has no source reference.")
        }
        foreach ($sourceRef in $sourceRefs) {
            $sourceValue = [string]$sourceRef
            if ([string]::IsNullOrWhiteSpace($sourceValue)) {
                $issues.Add("Inventory item '$id' has an empty source reference.")
            }
            elseif ($sourceValue -cnotmatch '^[A-Za-z][A-Za-z0-9+.-]*://' -and
                -not (Test-TrackedRepositoryLocator $sourceValue)) {
                $issues.Add("Inventory item '$id' has an invalid or untracked local source locator '$sourceValue'.")
            }
        }

        if ([string]::IsNullOrWhiteSpace([string](Get-Field $item 'purpose'))) {
            $issues.Add("Inventory item '$id' purpose is empty.")
        }
        if ([string]::IsNullOrWhiteSpace([string](Get-Field $item 'version'))) {
            $issues.Add("Inventory item '$id' version is empty.")
        }

        $scope = Get-Field $item 'scope'
        if ([string]::IsNullOrWhiteSpace([string](Get-Field $scope 'mode')) -or
            @(Get-Field $scope 'include').Count -eq 0) {
            $issues.Add("Inventory item '$id' has an incomplete scope.")
        }

        $presence = [string](Get-Field $item 'repository_presence')
        $internal = [string](Get-Field $item 'internal_handling')
        $external = [string](Get-Field $item 'external_distribution')
        if ($presence -notin $allowedPresence) {
            $issues.Add("Inventory item '$id' has unsupported repository presence '$presence'.")
        }
        if ($internal -notin $allowedInternal) {
            $issues.Add("Inventory item '$id' has unsupported internal handling '$internal'.")
        }
        if ($external -notin $allowedExternal) {
            $issues.Add("Inventory item '$id' has unsupported external distribution '$external'.")
        }
        if ($presence -eq 'external-untracked' -and
            @($sourceRefs | Where-Object { [string]$_ -match '^https://' }).Count -eq 0 -and
            $id -ne 'host-validation-toolchain') {
            $issues.Add("External item '$id' has no HTTPS upstream source.")
        }

        $license = Get-Field $item 'license'
        foreach ($field in @(
                'scan_result', 'detected_expression', 'concluded_expression',
                'evidence_refs', 'scope_note', 'follow_up')) {
            if ($null -eq (Get-Field $license $field)) {
                $issues.Add("Inventory item '$id' license block is missing '$field'.")
            }
        }
        $scanResult = [string](Get-Field $license 'scan_result')
        $detectedExpression = [string](Get-Field $license 'detected_expression')
        $concludedExpression = [string](Get-Field $license 'concluded_expression')
        if ($scanResult -notin $allowedScans) {
            $issues.Add("Inventory item '$id' has unsupported scan result '$scanResult'.")
        }
        if ($concludedExpression -ne 'NOASSERTION') {
            $issues.Add("Inventory item '$id' has an unapproved license conclusion '$concludedExpression'.")
        }
        if ($scanResult -eq 'no-license-information-found' -and
            $detectedExpression -ne 'NONE') {
            $issues.Add("Inventory item '$id' must record detected expression NONE.")
        }
        if ($concludedExpression -eq 'NOASSERTION' -and
            $external -notmatch '^(blocked-|not-redistributed$)') {
            $issues.Add("NOASSERTION item '$id' cannot be externally distributable.")
        }
        if ($concludedExpression -match 'LicenseRef-' -and
            @((Get-Field $license 'evidence_refs') | Where-Object {
                    [string]$_ -match '^(LICENSES|licenses)/'
                }).Count -eq 0) {
            $issues.Add("LicenseRef item '$id' has no preserved custom license text.")
        }
        foreach ($evidenceRef in @(Get-Field $license 'evidence_refs')) {
            $evidenceValue = [string]$evidenceRef
            if ($evidenceValue -cnotmatch '^[A-Za-z][A-Za-z0-9+.-]*://' -and
                -not (Test-TrackedRepositoryLocator $evidenceValue)) {
                $issues.Add("Inventory item '$id' has an invalid or untracked local license-evidence locator '$evidenceValue'.")
            }
        }
    }

    $requiredIds = @(
        'repository-default-content',
        'architecture-blueprint',
        'r0-research-evidence',
        'legacy-source-archive',
        'eigen-3.4.0-legacy-reproduction',
        'w64devkit-2.9.1-legacy-reproduction',
        'host-validation-toolchain',
        'github-actions-checkout-6.0.2')
    foreach ($requiredId in $requiredIds) {
        if (-not $ids.ContainsKey($requiredId)) {
            $issues.Add("Required inventory item is missing: $requiredId")
        }
    }
    if ((@($items | ForEach-Object { [string](Get-Field $_ 'id') }) -join ',') -cne
        ($requiredIds -join ',')) {
        $issues.Add('Inventory item set/order differs from the reviewed technical inventory.')
    }

    $reviewedItemHashes = [ordered]@{
        'repository-default-content' = '84634e86eb5353a2e99a22c64e082412991ee9befcda9af536681ed236e1a169'
        'architecture-blueprint' = 'c6cdd3a14b61736729e6a7540e4f1b57462c16d2e60f5a256484f6d326151198'
        'r0-research-evidence' = '2fab3ebf5f37be6c49e9f4e8db00d26ffdcd2fe631890d745a903db38333955f'
        'legacy-source-archive' = 'd4af233b70553bcd82149a14fe12b112d1b54f65f16c25854eee5310457dddbe'
        'eigen-3.4.0-legacy-reproduction' = 'e55713b44c84f4527516bdc1ed0925ff026fc658923937bc6442af9c407fef3d'
        'w64devkit-2.9.1-legacy-reproduction' = 'f64465d3d59c24923de4082992c2db7e757688efdf53a53d366f0c3e16e2a08c'
        'host-validation-toolchain' = '62e81a5b30adfa75c6b3e371bd1185f52bdd77c0d133fb1d28da6ef9da450321'
        'github-actions-checkout-6.0.2' = '9c87fd5655be91d713162a973ef6409da96a4d714f548e167942b6a432d69471'
    }
    foreach ($id in $reviewedItemHashes.Keys) {
        if ($ids.ContainsKey($id) -and
            (Get-ObjectCanonicalSha256 $ids[$id]) -cne $reviewedItemHashes[$id]) {
            $issues.Add("Inventory item '$id' reviewed semantic projection differs from the technical inventory.")
        }
    }

    foreach ($item in $items) {
        $id = [string](Get-Field $item 'id')
        $parents = @(Get-Field $item 'lineage_parents')
        $category = [string](Get-Field $item 'category')
        $external = [string](Get-Field $item 'external_distribution')
        if ($category -ceq 'fixture-oracle-and-generated-evidence') {
            if ($parents.Count -eq 0) {
                $issues.Add("Generated evidence item '$id' has no lineage parents.")
            }
            if ($external -cnotmatch '^blocked-') {
                $issues.Add("Generated evidence item '$id' must remain externally blocked while upstream rights are unresolved.")
            }
        }
        if (@($parents | Sort-Object -Unique).Count -ne $parents.Count) {
            $issues.Add("Inventory item '$id' has duplicate lineage parents.")
        }
        foreach ($parent in $parents) {
            if ([string]$parent -eq $id) {
                $issues.Add("Inventory item '$id' is its own lineage parent.")
            }
            elseif (-not $ids.ContainsKey([string]$parent)) {
                $issues.Add("Inventory item '$id' has missing lineage parent '$parent'.")
            }
        }
    }

    $remaining = @{}
    foreach ($item in $items) {
        $remaining[[string](Get-Field $item 'id')] = @(
            Get-Field $item 'lineage_parents')
    }
    while ($remaining.Count -gt 0) {
        $ready = [System.Collections.Generic.List[string]]::new()
        foreach ($id in @($remaining.Keys)) {
            $hasUnresolvedParent = $false
            foreach ($parent in @($remaining[$id])) {
                if ($remaining.ContainsKey([string]$parent)) {
                    $hasUnresolvedParent = $true
                    break
                }
            }
            if (-not $hasUnresolvedParent) {
                $ready.Add([string]$id)
            }
        }
        if ($ready.Count -eq 0) {
            $issues.Add('Inventory lineage contains a cycle.')
            break
        }
        foreach ($id in $ready) {
            $remaining.Remove($id)
        }
    }

    if ($ids.ContainsKey('legacy-source-archive')) {
        $legacy = $ids['legacy-source-archive']
        $integrity = Get-Field $legacy 'integrity'
        $expected = [ordered]@{
            sha256 = '2159a324fd897e4bd508c140a36c9165d744e4e4e61861c5b568201707f988e5'
            bytes = 990450
            zip_entries = 510
            file_entries = 391
            directory_entries = 119
            uncompressed_file_bytes = 2708191
            license_named_entries = 0
            license_text_signal_entries = 0
        }
        foreach ($name in $expected.Keys) {
            if ((Get-Field $integrity $name) -ne $expected[$name]) {
                $issues.Add("Legacy inventory integrity field '$name' differs from the frozen audit.")
            }
        }
        if ((Get-Field $legacy 'internal_handling') -ne 'evidence-only' -or
            (Get-Field $legacy 'external_distribution') -ne
            'blocked-pending-item-review') {
            $issues.Add('Legacy archive must remain evidence-only and externally blocked.')
        }
    }

    $externalExpected = [ordered]@{
        'eigen-3.4.0-legacy-reproduction' = [ordered]@{
            version = '3.4.0'
            sha256 = 'eba3f3d414d2f8cba2919c78ec6daab08fc71ba2ba4ae502b7e5d4d99fc02cda'
            bytes = 3704940
        }
        'w64devkit-2.9.1-legacy-reproduction' = [ordered]@{
            version = '2.9.1'
            sha256 = '9208c19755cd4964b7915b9afcf02c66d493a4c870c4b3e83f6c538d9c1237a5'
            bytes = 61462208
        }
    }
    foreach ($id in $externalExpected.Keys) {
        if (-not $ids.ContainsKey($id)) { continue }
        $item = $ids[$id]
        $expected = $externalExpected[$id]
        $integrity = Get-Field $item 'integrity'
        if ((Get-Field $item 'version') -ne $expected.version -or
            (Get-Field $integrity 'sha256') -ne $expected.sha256 -or
            (Get-Field $integrity 'bytes') -ne $expected.bytes) {
            $issues.Add("Pinned external identity differs for '$id'.")
        }
        if ((Get-Field $item 'repository_presence') -ne 'external-untracked' -or
            (Get-Field $item 'external_distribution') -ne 'not-redistributed') {
            $issues.Add("Pinned external item '$id' must remain untracked and not redistributed.")
        }
    }

    if ($ids.ContainsKey('github-actions-checkout-6.0.2')) {
        $checkout = $ids['github-actions-checkout-6.0.2']
        $integrity = Get-Field $checkout 'integrity'
        if ((Get-Field $checkout 'version') -ne '6.0.2' -or
            (Get-Field $integrity 'kind') -ne 'git-commit' -or
            (Get-Field $integrity 'value') -ne
            'de0fac2e4500dabe0009e67214ff5f5447ce83dd') {
            $issues.Add('Pinned actions/checkout identity differs from the reviewed v6.0.2 commit.')
        }
        if ((Get-Field $checkout 'repository_presence') -ne 'external-untracked' -or
            (Get-Field $checkout 'external_distribution') -ne 'not-redistributed') {
            $issues.Add('Pinned actions/checkout must remain untracked and not redistributed.')
        }
    }

    return $issues.ToArray()
}

function Invoke-Mutation(
    [string]$Name,
    [string]$ExpectedDiagnostic,
    [scriptblock]$Mutation) {
    $copy = $script:inventory | ConvertTo-Json -Depth 100 | ConvertFrom-Json
    & $Mutation $copy
    $mutationIssues = @(Test-InventoryObject $copy)
    $diagnosticMatched = @($mutationIssues | Where-Object {
            $_.IndexOf($ExpectedDiagnostic, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        }).Count -gt 0
    $rejected = $mutationIssues.Count -gt 0 -and $diagnosticMatched
    if ($mutationIssues.Count -eq 0) {
        Add-Error "Mutation was not rejected: $Name"
    }
    elseif (-not $diagnosticMatched) {
        Add-Error "Mutation '$Name' failed for the wrong reason: $($mutationIssues -join ' | ')"
    }
    $script:mutationResults.Add([pscustomobject][ordered]@{
            name = $Name
            rejected = $rejected
            expected_diagnostic = $ExpectedDiagnostic
            diagnostic_matched = $diagnosticMatched
            detected_issue_count = $mutationIssues.Count
        })
}

$requiredPaths = @(
    'LICENSE-STATUS.md',
    'docs/adr/0008-internal-default-license-and-provenance-gate.md',
    'docs/governance/license-and-provenance-policy.md',
    'docs/governance/provenance-inventory.json',
    'docs/governance/r0-governance-review-contract.json',
    'docs/governance/r0-owner-authorization.json',
    'docs/governance/schemas/scientific-context.schema.json',
    'docs/team/role-assignments.json',
    'docs/quality/provenance-review-checklist.md',
    'docs/quality/r0-first-wave-reconciliation-audit.md',
    'docs/quality/r0-g0-g1-readiness-audit.md',
    'docs/quality/scientific-conventions-cross-tool-report.json',
    'docs/tasks/backlog.json',
    'docs/tasks/work-packages/R0-GOV-002.md',
    'docs/tasks/work-packages/R0-GATE-001.md',
    'docs/tasks/work-packages/R0-SCI-001.md',
    'project-manifest.json',
    'docs/adr/0006-si-frame-and-simulation-time-conventions.md',
    'docs/adr/0007-passive-hamilton-quaternion-convention.md',
    'design-notes/gnczmkn-architecture-roadmap/03-mathematics-and-numerical-foundation.md',
    'design-notes/gnczmkn-architecture-roadmap/06-simulation-kernel-time-and-lifecycle.md',
    'fixtures/ref-scientific-conventions/fixture-manifest.json',
    'fixtures/ref-scientific-conventions/scientific-context.json',
    'fixtures/ref-scientific-conventions/conventions.json',
    'fixtures/ref-scientific-conventions/cases.json',
    'fixtures/ref-minimal-3dof/fixture-manifest.json',
    'fixtures/ref-yyz-001/fixture-manifest.json',
    'fixtures/ref-cavh-formula/fixture-manifest.json',
    'tests/scientific_conventions.cpp',
    'tools/scientific_conventions_reference.py',
    'tools/validate-scientific-conventions.ps1',
    'specs/fixture-manifest.schema.json',
    'specs/oracle-manifest.schema.json',
    'specs/plan-proof-record.schema.json',
    'specs/r0-schema-contract-lock.json',
    'tools/modules/JsonSchemaSubset.psm1',
    'CMakeLists.txt',
    'tools/verify-repository.ps1',
    'reference/legacy/source-manifest.json',
    'reference/legacy/legacy-source.zip',
    'reference/legacy/legacy-source.sha256',
    'reference/legacy/reproduction/current.json',
    'tools/modules/R0GovernanceReview.psm1',
    'tools/validate-license-provenance.ps1')
foreach ($relativePath in $requiredPaths) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $relativePath) -PathType Leaf)) {
        Add-Error "Required license/provenance path is missing: $relativePath"
    }
}

$inventory = Read-Json $inventoryPath
$script:inventory = $inventory
if ($null -ne $inventory) {
    foreach ($issue in @(Test-InventoryObject $inventory)) {
        Add-Error $issue
    }

    foreach ($item in @(Get-Field $inventory 'items')) {
        $id = [string](Get-Field $item 'id')
        foreach ($sourceRef in @(Get-Field $item 'source_refs')) {
            if ([string]$sourceRef -match '^https://') { continue }
            $sourcePath = Join-Path $repoRoot ([string]$sourceRef)
            if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
                Add-Error "Local source reference for '$id' is missing: $sourceRef"
            }
        }
        foreach ($evidenceRef in @(Get-Field (Get-Field $item 'license') 'evidence_refs')) {
            if ([string]$evidenceRef -match '^https://') { continue }
            $evidencePath = Join-Path $repoRoot ([string]$evidenceRef)
            if (-not (Test-Path -LiteralPath $evidencePath -PathType Leaf)) {
                Add-Error "Local license evidence for '$id' is missing: $evidenceRef"
            }
        }
    }
}

$licenseStatusPath = Join-Path $repoRoot 'LICENSE-STATUS.md'
if (Test-Path -LiteralPath $licenseStatusPath -PathType Leaf) {
    $licenseStatus = Get-Content -LiteralPath $licenseStatusPath -Raw -Encoding utf8
    foreach ($requiredText in @(
            'No distribution license has been selected for this repository.',
            'This status notice is not a license grant.',
            'External sharing is blocked',
            'The frozen Legacy archive remains evidence-only')) {
        if (-not $licenseStatus.Contains($requiredText)) {
            Add-Error "LICENSE-STATUS.md is missing required safeguard text: $requiredText"
        }
    }
}

$adrPath = Join-Path $repoRoot 'docs\adr\0008-internal-default-license-and-provenance-gate.md'
if (Test-Path -LiteralPath $adrPath -PathType Leaf) {
    $adrText = Get-Content -LiteralPath $adrPath -Raw -Encoding utf8
    if ($adrText -notmatch '(?m)^- Status: Proposed$' -or
        $adrText -notmatch '(?m)^- Owners: Product Owner, Architecture Lead$') {
        Add-Error 'ADR-0008 must remain Proposed with both decision-owner roles.'
    }
}

$rootLicenseFiles = @(Get-ChildItem -LiteralPath $repoRoot -File | Where-Object {
        $_.Name -match '^(?i)(LICENSE|LICENCE|COPYING|UNLICENSE)(\..*)?$'
    })
if ($rootLicenseFiles.Count -gt 0) {
    Add-Error "A repository distribution license file exists while ADR-0008 is Proposed: $($rootLicenseFiles.Name -join ', ')"
}

$legacyArchivePath = Join-Path $repoRoot 'reference\legacy\legacy-source.zip'
$legacyAudit = $null
if (Test-Path -LiteralPath $legacyArchivePath -PathType Leaf) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
    $archive = [System.IO.Compression.ZipFile]::OpenRead($legacyArchivePath)
    try {
        $fileEntries = @($archive.Entries | Where-Object {
                -not [string]::IsNullOrEmpty($_.Name)
            })
        $directoryEntries = @($archive.Entries | Where-Object {
                [string]::IsNullOrEmpty($_.Name)
            })
        $licenseNamePattern = '(?i)(^|/)(licen[cs]e|copying|notice|copyright)(\.[^/]*)?$'
        $licenseTextPattern = '(?im)SPDX-License-Identifier|Copyright\s*(\(c\)|©)|Licensed under the|Permission is hereby granted, free of charge|GNU (GENERAL|LESSER) PUBLIC LICENSE|Mozilla Public License'
        $licenseNamedEntries = @($fileEntries | Where-Object {
                $_.FullName -match $licenseNamePattern
            })
        $licenseTextSignalEntries = [System.Collections.Generic.List[string]]::new()
        $textExtensions = @(
            '', '.md', '.txt', '.hpp', '.h', '.cpp', '.c', '.cmake',
            '.json', '.ps1', '.py', '.m', '.gitignore')
        foreach ($entry in $fileEntries) {
            if ($entry.Length -gt 1048576 -or
                [System.IO.Path]::GetExtension($entry.FullName).ToLowerInvariant() -notin
                $textExtensions) {
                continue
            }
            $reader = [System.IO.StreamReader]::new(
                $entry.Open(), [System.Text.Encoding]::UTF8, $true)
            try {
                $content = $reader.ReadToEnd()
            }
            finally {
                $reader.Dispose()
            }
            if ($content -match $licenseTextPattern) {
                $licenseTextSignalEntries.Add($entry.FullName)
            }
        }
        $legacyAudit = [pscustomobject][ordered]@{
            sha256 = Get-Sha256 $legacyArchivePath
            bytes = (Get-Item -LiteralPath $legacyArchivePath).Length
            zip_entries = $archive.Entries.Count
            file_entries = $fileEntries.Count
            directory_entries = $directoryEntries.Count
            uncompressed_file_bytes = [long](
                $fileEntries | Measure-Object -Property Length -Sum).Sum
            license_named_entries = $licenseNamedEntries.Count
            license_text_signal_entries = $licenseTextSignalEntries.Count
        }
    }
    finally {
        $archive.Dispose()
    }
}

if ($null -ne $legacyAudit -and $null -ne $inventory) {
    $legacyItem = @((Get-Field $inventory 'items') | Where-Object {
            (Get-Field $_ 'id') -eq 'legacy-source-archive'
        }) | Select-Object -First 1
    if ($null -ne $legacyItem) {
        $integrity = Get-Field $legacyItem 'integrity'
        foreach ($field in @(
                'sha256', 'bytes', 'zip_entries', 'file_entries',
                'directory_entries', 'uncompressed_file_bytes',
                'license_named_entries', 'license_text_signal_entries')) {
            if ((Get-Field $legacyAudit $field) -ne (Get-Field $integrity $field)) {
                Add-Error "Live Legacy audit differs from inventory field '$field'."
            }
        }
    }
}

$legacyManifest = Read-Json (Join-Path $repoRoot 'reference\legacy\source-manifest.json')
if ($null -ne $legacyManifest -and $null -ne $legacyAudit) {
    $manifestArchive = Get-Field $legacyManifest 'archive'
    if ((Get-Field $manifestArchive 'sha256') -ne $legacyAudit.sha256 -or
        (Get-Field $manifestArchive 'bytes') -ne $legacyAudit.bytes) {
        Add-Error 'Legacy source manifest differs from the live archive audit.'
    }
}

$pointer = Read-Json (Join-Path $repoRoot 'reference\legacy\reproduction\current.json')
$environmentRelativePath = $null
$environment = $null
if ($null -ne $pointer) {
    $environmentRelativePath = ([string](Get-Field $pointer 'path')).Replace('\', '/') +
        '/environment-manifest.json'
    $environment = Read-Json (Join-Path $repoRoot $environmentRelativePath)
}
if ($null -ne $environment -and $null -ne $inventory) {
    $items = @(Get-Field $inventory 'items')
    $dependencyPairs = [ordered]@{
        'eigen-3.4.0-legacy-reproduction' = 'eigen'
        'w64devkit-2.9.1-legacy-reproduction' = 'w64devkit'
    }
    foreach ($itemId in $dependencyPairs.Keys) {
        $item = @($items | Where-Object { (Get-Field $_ 'id') -eq $itemId }) |
            Select-Object -First 1
        $dependency = Get-Field (Get-Field $environment 'dependencies') $dependencyPairs[$itemId]
        if ($null -eq $item -or $null -eq $dependency) {
            Add-Error "Cannot compare reproduction dependency identity for '$itemId'."
            continue
        }
        $integrity = Get-Field $item 'integrity'
        if ((Get-Field $item 'version') -ne (Get-Field $dependency 'version') -or
            (Get-Field $integrity 'sha256') -ne (Get-Field $dependency 'archive_sha256') -or
            (Get-Field $integrity 'bytes') -ne (Get-Field $dependency 'archive_bytes')) {
            Add-Error "Inventory differs from reproduction evidence for '$itemId'."
        }
    }
}

if ($null -ne $inventory) {
    Invoke-Mutation 'duplicate-item-id' 'Duplicate inventory item id' {
        param($value)
        $value.items[1].id = $value.items[0].id
    }
    Invoke-Mutation 'missing-source-provenance' 'has no source reference' {
        param($value)
        $value.items[0].source_refs = @()
    }
    Invoke-Mutation 'noassertion-external-distribution' 'cannot be externally distributable' {
        param($value)
        $value.items[0].external_distribution = 'allowed'
    }
    Invoke-Mutation 'legacy-hash-drift' "Legacy inventory integrity field 'sha256' differs" {
        param($value)
        $value.items[3].integrity.sha256 = ('0' * 64)
    }
    Invoke-Mutation 'unapproved-license-conclusion' 'has an unapproved license conclusion' {
        param($value)
        $value.items[0].license.concluded_expression = 'MIT'
    }
    Invoke-Mutation 'license-ref-without-text' 'has no preserved custom license text' {
        param($value)
        $value.items[0].license.concluded_expression = 'LicenseRef-GNC-Private'
    }
    Invoke-Mutation 'repository-license-status-contradiction' 'Repository license cannot be selected' {
        param($value)
        $value.repository_license.selected = $true
    }
    Invoke-Mutation 'checkout-action-pin-drift' 'Pinned actions/checkout identity differs' {
        param($value)
        $checkout = @($value.items | Where-Object {
                $_.id -eq 'github-actions-checkout-6.0.2'
            }) | Select-Object -First 1
        $checkout.integrity.value = ('0' * 40)
    }
    Invoke-Mutation 'unknown-item-category' 'has unsupported category' {
        param($value)
        $value.items[2].category = 'Fixture-Oracle-And-Generated-Evidence'
    }
    Invoke-Mutation 'generated-classification-mismatch' 'is incompatible with category' {
        param($value)
        $value.items[2].classification = 'external-tool'
    }
    Invoke-Mutation 'generated-missing-lineage' 'has no lineage parents' {
        param($value)
        $value.items[2].lineage_parents = @()
    }
    Invoke-Mutation 'generated-unresolved-lineage' 'has missing lineage parent' {
        param($value)
        $value.items[2].lineage_parents = @($value.items[2].lineage_parents) +
            @('missing-upstream-record')
    }
    Invoke-Mutation 'generated-restriction-downgrade' 'must remain externally blocked' {
        param($value)
        $value.items[2].external_distribution = 'not-redistributed'
    }
    Invoke-Mutation 'generated-missing-integrity' 'integrity value is empty' {
        param($value)
        $value.items[2].integrity.value = ''
    }
    Invoke-Mutation 'extra-inventory-item' 'Inventory item set/order differs' {
        param($value)
        $extra = $value.items[0] | ConvertTo-Json -Depth 100 | ConvertFrom-Json
        $extra.id = 'extra-inventory-item'
        $value.items = @($value.items) + @($extra)
    }
    Invoke-Mutation 'inventory-item-order-drift' 'Inventory item set/order differs' {
        param($value)
        $first = $value.items[0]
        $value.items[0] = $value.items[1]
        $value.items[1] = $first
    }
    Invoke-Mutation 'inventory-vocabulary-extra' 'scan vocabulary differs' {
        param($value)
        $value.vocabulary.scan_results = @($value.vocabulary.scan_results) + @('unknown')
    }
    Invoke-Mutation 'inventory-owner-drift' 'reviewed semantic projection differs' {
        param($value)
        $value.items[0].owner_role = 'architecture_lead'
    }
    Invoke-Mutation 'inventory-purpose-empty' 'purpose is empty' {
        param($value)
        $value.items[0].purpose = ''
    }
    Invoke-Mutation 'inventory-version-empty' 'version is empty' {
        param($value)
        $value.items[0].version = ''
    }
    Invoke-Mutation 'inventory-scope-drift' 'reviewed semantic projection differs' {
        param($value)
        $value.items[0].scope.exclude = @($value.items[0].scope.exclude) + @('framework/**')
    }
    Invoke-Mutation 'generated-valid-parent-dropped' 'reviewed semantic projection differs' {
        param($value)
        $value.items[2].lineage_parents = @($value.items[2].lineage_parents | Select-Object -Skip 1)
    }
    Invoke-Mutation 'generated-false-existing-parent-added' 'reviewed semantic projection differs' {
        param($value)
        $value.items[7].lineage_parents = @('repository-default-content')
    }
    Invoke-Mutation 'local-source-absolute-locator' 'invalid or untracked local source locator' {
        param($value)
        $value.items[0].source_refs[1] = 'C:/review/LICENSE-STATUS.md'
    }
    Invoke-Mutation 'local-source-percent-locator' 'invalid or untracked local source locator' {
        param($value)
        $value.items[0].source_refs[1] = 'docs/%71uality/provenance-review-checklist.md'
    }
    Invoke-Mutation 'local-evidence-untracked-locator' 'invalid or untracked local license-evidence locator' {
        param($value)
        $value.items[0].license.evidence_refs[0] = 'docs/governance/not-tracked.txt'
    }
}

$governanceReview = Test-R0GovernanceReview -RepoRoot $repoRoot -RunMutations
foreach ($issue in @($governanceReview.Issues)) {
    Add-Error ([string]$issue)
}

$roles = Read-Json (Join-Path $repoRoot 'docs\team\role-assignments.json')
$roleState = [ordered]@{}
foreach ($roleId in @('product_owner', 'architecture_lead')) {
    $role = @((Get-Field $roles 'roles') | Where-Object {
            (Get-Field $_ 'id') -eq $roleId
        }) | Select-Object -First 1
    $roleState[$roleId + '_assigned'] = (
        $null -ne $role -and
        -not [string]::IsNullOrWhiteSpace([string](Get-Field $role 'assignee')))
}

$inputHashes = @(Get-R0GovernanceReportInputFacts -RepoRoot $repoRoot `
    -EnvironmentRelativePath $environmentRelativePath)

$items = if ($null -ne $inventory) { @(Get-Field $inventory 'items') } else { @() }
$noAssertionCount = @($items | Where-Object {
        (Get-Field (Get-Field $_ 'license') 'concluded_expression') -eq 'NOASSERTION'
    }).Count
$blockedCount = @($items | Where-Object {
        [string](Get-Field $_ 'external_distribution') -match '^blocked-'
    }).Count
$notRedistributedCount = @($items | Where-Object {
        (Get-Field $_ 'external_distribution') -eq 'not-redistributed'
    }).Count

$expectedReport = [pscustomobject][ordered]@{
    schema_version = 'gnczmkn.license-provenance-conformance/1'
    task_id = 'R0-GOV-002'
    reviewed_on = '2026-08-12'
    status = 'passed'
    policy = [ordered]@{
        id = 'GNC-LIC-PROV-001'
        adr_status = 'Proposed'
        repository_distribution_license_selected = $false
        root_distribution_license_files = $rootLicenseFiles.Count
        decision_roles = $roleState
    }
    inventory = [ordered]@{
        item_count = $items.Count
        noassertion_count = $noAssertionCount
        externally_blocked_count = $blockedCount
        external_not_redistributed_count = $notRedistributedCount
        runtime_consumers = 0
    }
    scientific_context = [ordered]@{
        schema_id = 'https://internal.gnczmkn/schemas/scientific-context/1'
        instance_version = 'gnczmkn.scientific-context/1'
        context_count = $governanceReview.ContextCount
        review_state = 'registered-pending-scientific-acceptance'
        runtime_consumers = $governanceReview.RuntimeConsumerCount
        implementation_independence = 'confirmed'
        scientific_source_independence = 'not-claimed'
        rights_effect = 'no-change'
        external_distribution = 'blocked-pending-accepted-adr'
        mutation_tests = @($governanceReview.MutationResults)
    }
    legacy_archive_audit = $legacyAudit
    pinned_external_inputs = @(
        [ordered]@{
            id = 'eigen-3.4.0-legacy-reproduction'
            version = '3.4.0'
            sha256 = 'eba3f3d414d2f8cba2919c78ec6daab08fc71ba2ba4ae502b7e5d4d99fc02cda'
            repository_presence = 'external-untracked'
            external_distribution = 'not-redistributed'
        },
        [ordered]@{
            id = 'w64devkit-2.9.1-legacy-reproduction'
            version = '2.9.1'
            sha256 = '9208c19755cd4964b7915b9afcf02c66d493a4c870c4b3e83f6c538d9c1237a5'
            repository_presence = 'external-untracked'
            external_distribution = 'not-redistributed'
        },
        [ordered]@{
            id = 'github-actions-checkout-6.0.2'
            version = '6.0.2'
            git_commit = 'de0fac2e4500dabe0009e67214ff5f5447ce83dd'
            repository_presence = 'external-untracked'
            external_distribution = 'not-redistributed'
        })
    mutation_tests = @($mutationResults)
    input_hashes = @($inputHashes)
}

if ($errors.Count -eq 0) {
    if ($UpdateReport) {
        $json = $expectedReport | ConvertTo-Json -Depth 20
        $json = $json.Replace("`r`n", "`n").Replace("`r", "`n")
        [System.IO.File]::WriteAllText(
            $reportPath,
            $json + "`n",
            [System.Text.UTF8Encoding]::new($false))
    }
    else {
        $actualReport = Read-Json $reportPath
        if ($null -ne $actualReport) {
            $actualCanonical = $actualReport | ConvertTo-Json -Depth 20 -Compress
            $expectedCanonical = $expectedReport | ConvertTo-Json -Depth 20 -Compress
            if ($actualCanonical -ne $expectedCanonical) {
                Add-Error 'License/provenance conformance report is stale; run with -UpdateReport.'
            }
        }
    }
}

if ($errors.Count -gt 0) {
    Write-Host "License/provenance validation failed with $($errors.Count) issue(s):"
    foreach ($errorMessage in $errors) {
        Write-Host " - $errorMessage"
    }
    exit 1
}

if (-not $Quiet) {
    Write-Host 'License/provenance validation passed.'
    Write-Host "Inventory items: $($items.Count); NOASSERTION: $noAssertionCount"
    Write-Host "Legacy archive: $($legacyAudit.file_entries) files; license signals: $($legacyAudit.license_named_entries + $legacyAudit.license_text_signal_entries)"
    Write-Host "Mutation tests rejected: $(@($mutationResults | Where-Object { $_.rejected }).Count)/$($mutationResults.Count)"
    Write-Host "Scientific context mutations rejected: $(@($governanceReview.MutationResults | Where-Object { $_.rejected }).Count)/$($governanceReview.MutationCount)"
}

Set-StrictMode -Version Latest

function Add-R0ArchitectureReviewIssue {
    param([System.Collections.Generic.List[string]]$Issues, [string]$Message)
    [void]$Issues.Add($Message)
}

function Test-R0ArchitectureReviewProperties {
    param(
        $Object,
        [string[]]$Expected,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    if ($null -eq $Object -or $Object -isnot [System.Management.Automation.PSCustomObject]) {
        Add-R0ArchitectureReviewIssue $Issues "$Label must be a JSON object."
        return
    }
    $actual = @($Object.PSObject.Properties | ForEach-Object { [string]$_.Name })
    foreach ($propertyName in $Expected) {
        if ($propertyName -cnotin $actual) {
            Add-R0ArchitectureReviewIssue $Issues "$Label is missing property '$propertyName'."
        }
    }
    foreach ($propertyName in $actual) {
        if ($propertyName -cnotin $Expected) {
            Add-R0ArchitectureReviewIssue $Issues "$Label has unknown property '$propertyName'."
        }
    }
}

function Test-R0ArchitectureReviewSequence {
    param(
        [object[]]$Actual,
        [object[]]$Expected,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    if ($Actual.Count -ne $Expected.Count) {
        Add-R0ArchitectureReviewIssue $Issues "$Label count must be $($Expected.Count); found $($Actual.Count)."
        return
    }
    for ($index = 0; $index -lt $Expected.Count; ++$index) {
        if ([string]$Actual[$index] -cne [string]$Expected[$index]) {
            Add-R0ArchitectureReviewIssue $Issues "$Label entry $index must be '$($Expected[$index])'; found '$($Actual[$index])'."
        }
    }
}

function Copy-R0ArchitectureReviewJson {
    param($Value)
    return $Value | ConvertTo-Json -Depth 100 | ConvertFrom-Json
}

function Get-R0ArchitectureReviewSha256 {
    param([byte[]]$Bytes)

    $hash = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($hash.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $hash.Dispose()
    }
}

function Get-R0ArchitectureNormalizedSourceRecord {
    param(
        [string]$RepoRoot,
        [string]$RelativePath,
        [hashtable]$ContentOverrides,
        [System.Collections.Generic.List[string]]$Issues
    )

    if ($RelativePath -notmatch '^[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*$') {
        Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' is not canonical."
        return $null
    }
    $fullPath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $RelativePath))
    $repoPrefix = $RepoRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' escapes the repository."
        return $null
    }
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' does not exist."
        return $null
    }

    $stageRows = @(& git -C $RepoRoot ls-files --stage -- $RelativePath 2>$null)
    if ($LASTEXITCODE -ne 0) {
        Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' could not be resolved through Git."
    }
    elseif ($stageRows.Count -eq 0) {
        $candidateRows = @(& git -C $RepoRoot ls-files --others --exclude-standard -- $RelativePath 2>$null)
        if ($LASTEXITCODE -ne 0 -or $candidateRows.Count -ne 1 -or [string]$candidateRows[0] -cne $RelativePath) {
            Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' must be tracked or a non-ignored review candidate."
        }
    }
    elseif ($stageRows.Count -ne 1) {
        Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' must resolve to one stage-0 entry."
    }
    else {
        $stageMatch = [regex]::Match([string]$stageRows[0], '^(?<mode>[0-9]{6}) [0-9a-f]+ 0\t(?<path>.+)$')
        if (-not $stageMatch.Success -or $stageMatch.Groups['path'].Value -cne $RelativePath) {
            Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' has a non-canonical Git entry."
        }
        elseif ($stageMatch.Groups['mode'].Value -notin @('100644', '100755')) {
            Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' must be a regular Git blob."
        }
    }

    $bytes = if ($null -ne $ContentOverrides -and $ContentOverrides.ContainsKey($RelativePath)) {
        $overrideValue = $ContentOverrides[$RelativePath]
        if ($overrideValue -is [byte[]]) { [byte[]]$overrideValue }
        else { [System.Text.UTF8Encoding]::new($false).GetBytes([string]$overrideValue) }
    }
    else {
        [System.IO.File]::ReadAllBytes($fullPath)
    }
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' must not contain a UTF-8 BOM."
        return $null
    }
    try {
        $text = [System.Text.UTF8Encoding]::new($false, $true).GetString($bytes)
    }
    catch {
        Add-R0ArchitectureReviewIssue $Issues "Authority snapshot path '$RelativePath' is not strict UTF-8."
        return $null
    }
    $normalizedText = ($text -replace "`r`n", "`n") -replace "`r", "`n"
    $normalizedBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($normalizedText)
    return [PSCustomObject]([ordered]@{
        path = $RelativePath
        normalized_byte_length = $normalizedBytes.Length
        normalized_sha256 = Get-R0ArchitectureReviewSha256 -Bytes $normalizedBytes
    })
}

function Get-R0ArchitectureSnapshotRecords {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$RepoRoot)

    $issues = [System.Collections.Generic.List[string]]::new()
    $paths = @(
        'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md',
        'docs/adr/0003-initial-module-dependency-dag.md',
        'design-notes/gnczmkn-architecture-roadmap/02-layered-reference-architecture.md',
        'docs/architecture/authority-registry.json',
        'CMakeLists.txt',
        'tools/modules/ArchitectureBaseline.psm1',
        'tools/modules/R0ArchitectureReview.psm1',
        'tools/modules/JsonSchemaSubset.psm1',
        'tools/validate-architecture-baseline.ps1',
        'docs/architecture/architecture-baseline.json',
        'docs/quality/terminology-conformance-report.json')
    $records = @($paths | ForEach-Object {
            Get-R0ArchitectureNormalizedSourceRecord -RepoRoot $RepoRoot -RelativePath $_ -ContentOverrides $null -Issues $issues
        })
    if ($issues.Count -gt 0) { throw ($issues -join ' | ') }
    return @($records)
}

function Test-R0ArchitectureRuntimeConsumers {
    param(
        [string]$RepoRoot,
        $Policy,
        [hashtable]$SyntheticConsumers,
        [System.Collections.Generic.List[string]]$Issues
    )

    $hits = [System.Collections.Generic.List[string]]::new()
    $extensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.ixx', '.py', '.ps1', '.cmake', '.json')
    $paths = @(& git -C $RepoRoot ls-files --cached --others --exclude-standard -- @($Policy.scan_roots) 2>$null)
    if ($LASTEXITCODE -ne 0) {
        Add-R0ArchitectureReviewIssue $Issues 'Runtime consumer scan could not enumerate repository paths.'
        return
    }
    $contentByPath = @{}
    foreach ($relativePath in $paths) {
        $extension = [System.IO.Path]::GetExtension([string]$relativePath).ToLowerInvariant()
        if ($extension -notin $extensions -and [System.IO.Path]::GetFileName([string]$relativePath) -cne 'CMakeLists.txt') { continue }
        $fullPath = Join-Path $RepoRoot ([string]$relativePath)
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            $contentByPath[[string]$relativePath] = Get-Content -LiteralPath $fullPath -Raw -Encoding utf8
        }
    }
    if ($null -ne $SyntheticConsumers) {
        foreach ($syntheticPath in $SyntheticConsumers.Keys) {
            $contentByPath[[string]$syntheticPath] = [string]$SyntheticConsumers[$syntheticPath]
        }
    }
    foreach ($relativePath in $contentByPath.Keys) {
        foreach ($token in @($Policy.forbidden_path_tokens)) {
            if ([string]$contentByPath[$relativePath] -match [regex]::Escape([string]$token)) {
                [void]$hits.Add("$relativePath -> $token")
            }
        }
    }
    if ($hits.Count -ne [int]$Policy.allowed_consumer_count) {
        Add-R0ArchitectureReviewIssue $Issues "Architecture governance artifacts have $($hits.Count) runtime consumer(s); expected $($Policy.allowed_consumer_count): $($hits -join ', ')."
    }
}

function Test-R0ArchitectureReviewContract {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]$Contract,
        [Parameter(Mandatory = $true)]$Registry,
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [hashtable]$ContentOverrides,
        [hashtable]$SyntheticConsumers
    )

    $issues = [System.Collections.Generic.List[string]]::new()
    Test-R0ArchitectureReviewProperties -Object $Contract -Expected @(
        'schema_version', 'maturity', 'task_id', 'adr_id', 'reviewed_base', 'decisions',
        'physical_dag', 'logical_boundaries', 'legacy_reconciliation', 'authority_snapshot',
        'runtime_consumer_policy', 'required_mutation_ids') -Label 'Architecture review contract' -Issues $issues
    if ([string]$Contract.schema_version -cne 'gnczmkn.r0-architecture-review-contract/1') {
        Add-R0ArchitectureReviewIssue $issues 'Architecture review contract schema version is unsupported.'
    }
    if ([string]$Contract.maturity -cne 'review-candidate-governance-only-no-runtime-consumer') {
        Add-R0ArchitectureReviewIssue $issues 'Architecture review contract maturity drifted.'
    }
    if ([string]$Contract.task_id -cne 'R0-ARCH-001' -or [string]$Contract.adr_id -cne 'ADR-0005') {
        Add-R0ArchitectureReviewIssue $issues 'Architecture review contract task or ADR identity drifted.'
    }
    if ([string]$Contract.reviewed_base -cne 'e0bba2b99e96a2a6ded646302b1d1424d323c362') {
        Add-R0ArchitectureReviewIssue $issues 'Architecture review contract base drifted.'
    }

    Test-R0ArchitectureReviewProperties -Object $Contract.decisions -Expected @('RECON-DEC-006', 'RECON-DEC-007') -Label 'Architecture decisions' -Issues $issues
    if ([string]$Contract.decisions.'RECON-DEC-006' -cne 'logical-only-keep-current') {
        Add-R0ArchitectureReviewIssue $issues 'RECON-DEC-006 outcome drifted.'
    }
    if ([string]$Contract.decisions.'RECON-DEC-007' -cne 'keep-current-22-owner-consumer-map') {
        Add-R0ArchitectureReviewIssue $issues 'RECON-DEC-007 outcome drifted.'
    }

    Test-R0ArchitectureReviewProperties -Object $Contract.physical_dag -Expected @(
        'authority', 'graph_version', 'registry_schema_version', 'module_names',
        'logical_labels_forbidden_as_physical_modules', 'superseding_adr_required_for_change') -Label 'Physical DAG policy' -Issues $issues
    if ([string]$Contract.physical_dag.authority -cne 'docs/adr/0003-initial-module-dependency-dag.md' -or
        [string]$Contract.physical_dag.graph_version -cne 'adr-0003/1' -or
        [string]$Contract.physical_dag.registry_schema_version -cne 'gnczmkn.architecture-authority-registry/1' -or
        -not [bool]$Contract.physical_dag.superseding_adr_required_for_change) {
        Add-R0ArchitectureReviewIssue $issues 'Physical DAG authority policy drifted.'
    }
    $expectedModules = @('foundation', 'contracts', 'model_sdk', 'compiler', 'kernel', 'evidence', 'workflow', 'application', 'adapters')
    Test-R0ArchitectureReviewSequence -Actual @($Contract.physical_dag.module_names) -Expected $expectedModules -Label 'Physical module sequence' -Issues $issues
    Test-R0ArchitectureReviewSequence -Actual @($Contract.physical_dag.logical_labels_forbidden_as_physical_modules) -Expected @('packages_user', 'composition_root') -Label 'Logical-only labels' -Issues $issues
    Test-R0ArchitectureReviewSequence -Actual @($Registry.modules.name) -Expected $expectedModules -Label 'Registry physical modules' -Issues $issues

    $expectedBoundaryNames = @('packages_user', 'composition_root')
    if (@($Contract.logical_boundaries).Count -ne 2) {
        Add-R0ArchitectureReviewIssue $issues 'Architecture review contract must contain two logical boundaries.'
    }
    else {
        for ($index = 0; $index -lt 2; ++$index) {
            $boundary = $Contract.logical_boundaries[$index]
            Test-R0ArchitectureReviewProperties -Object $boundary -Expected @(
                'name', 'kind', 'authority', 'representation', 'source_roots', 'relationship', 'physical_module') -Label "Logical boundary row $index" -Issues $issues
            if ([string]$boundary.name -cne $expectedBoundaryNames[$index] -or [bool]$boundary.physical_module) {
                Add-R0ArchitectureReviewIssue $issues "Logical boundary row $index identity or physical-module flag drifted."
            }
            if ([string]$boundary.authority -cne 'design-notes/gnczmkn-architecture-roadmap/02-layered-reference-architecture.md#13' -or
                [string]$boundary.representation -cne 'source-rule-label-only') {
                Add-R0ArchitectureReviewIssue $issues "Logical boundary row $index authority or representation drifted."
            }
        }
        if ([string]$Contract.logical_boundaries[0].kind -cne 'logical-contribution-boundary' -or
            [string]$Contract.logical_boundaries[0].relationship -cne 'contributes-through-model-sdk-descriptors') {
            Add-R0ArchitectureReviewIssue $issues 'packages_user logical boundary drifted.'
        }
        if ([string]$Contract.logical_boundaries[1].kind -cne 'logical-composition-boundary' -or
            [string]$Contract.logical_boundaries[1].relationship -cne 'only-boundary-with-packages-adapters-and-application-host') {
            Add-R0ArchitectureReviewIssue $issues 'composition_root logical boundary drifted.'
        }
        Test-R0ArchitectureReviewSequence -Actual @($Contract.logical_boundaries[0].source_roots) -Expected @('packages', 'user') -Label 'packages_user source roots' -Issues $issues
        Test-R0ArchitectureReviewSequence -Actual @($Contract.logical_boundaries[1].source_roots) -Expected @('apps') -Label 'composition_root source roots' -Issues $issues
    }

    $legacyPolicy = $Contract.legacy_reconciliation
    Test-R0ArchitectureReviewProperties -Object $legacyPolicy -Expected @(
        'outcome', 'authority_mapping_count', 'classified_responsibility_count', 'aligned_responsibility_ids',
        'deferred_owner_split_ids', 'logical_route_ids', 'unregistered_legacy_names',
        'unregistered_responsibility_ids', 'responsibility_overlay_forbidden_in_v1',
        'glossary_migration_and_superseding_adr_required_for_change') -Label 'Legacy reconciliation policy' -Issues $issues
    if ([string]$legacyPolicy.outcome -cne 'keep-current-22-owner-consumer-map' -or
        [int]$legacyPolicy.authority_mapping_count -ne 22 -or
        [int]$legacyPolicy.classified_responsibility_count -ne 33 -or
        -not [bool]$legacyPolicy.responsibility_overlay_forbidden_in_v1 -or
        -not [bool]$legacyPolicy.glossary_migration_and_superseding_adr_required_for_change) {
        Add-R0ArchitectureReviewIssue $issues 'Legacy reconciliation policy drifted.'
    }
    if (@($Registry.legacy_ownership).Count -ne 22) {
        Add-R0ArchitectureReviewIssue $issues "Authority registry must retain 22 Legacy ownership mappings; found $(@($Registry.legacy_ownership).Count)."
    }
    Test-R0ArchitectureReviewSequence -Actual @($legacyPolicy.aligned_responsibility_ids) -Expected @(
        'assembly-context-lowering', 'auto-data-recording', 'config-manager-compile', 'config-node-source',
        'config-reader-frontend', 'discrete-node-descriptor', 'execution-phase-transaction', 'framework-catalog-view',
        'discrete-task-obligation', 'observable-projection-plan', 'record-sink-port', 'mission-assembler-passes',
        'node-registry-plan-handles', 'node-registry-session-state', 'observable-field-contract', 'onboard-state-contracts',
        'simflow-experiment', 'simulation-builder-operation', 'simulation-node-definition', 'simulator-session',
        'simulator-control', 'simulator-comparison') -Label 'Aligned responsibility ids' -Issues $issues
    Test-R0ArchitectureReviewSequence -Actual @($legacyPolicy.deferred_owner_split_ids) -Expected @(
        'execution-phase-plan', 'onboard-state-input-view', 'simulation-builder-plan') -Label 'Deferred owner split ids' -Issues $issues
    Test-R0ArchitectureReviewSequence -Actual @($legacyPolicy.logical_route_ids) -Expected @(
        'guidance-process-recipe', 'node-factory-contribution') -Label 'Logical route ids' -Issues $issues
    Test-R0ArchitectureReviewSequence -Actual @($legacyPolicy.unregistered_legacy_names) -Expected @(
        'IContinuousGroup', 'IIntegrator', 'ISummaryObserver', 'math_types.hpp', 'SimulationSummary') -Label 'Unregistered Legacy names' -Issues $issues
    Test-R0ArchitectureReviewSequence -Actual @($legacyPolicy.unregistered_responsibility_ids) -Expected @(
        'continuous-group-plan', 'integrator-plan', 'integrator-outcome', 'summary-observer-metrics',
        'math-types-contract-boundaries', 'simulation-summary-workflow') -Label 'Unregistered responsibility ids' -Issues $issues

    Test-R0ArchitectureReviewProperties -Object $Contract.authority_snapshot -Expected @('normalization', 'entries') -Label 'Authority snapshot' -Issues $issues
    if ([string]$Contract.authority_snapshot.normalization -cne 'utf8-lf-no-bom') {
        Add-R0ArchitectureReviewIssue $issues 'Authority snapshot normalization drifted.'
    }
    $expectedPaths = @(
        'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md',
        'docs/adr/0003-initial-module-dependency-dag.md',
        'design-notes/gnczmkn-architecture-roadmap/02-layered-reference-architecture.md',
        'docs/architecture/authority-registry.json', 'CMakeLists.txt',
        'tools/modules/ArchitectureBaseline.psm1', 'tools/modules/R0ArchitectureReview.psm1',
        'tools/modules/JsonSchemaSubset.psm1', 'tools/validate-architecture-baseline.ps1',
        'docs/architecture/architecture-baseline.json', 'docs/quality/terminology-conformance-report.json')
    if (@($Contract.authority_snapshot.entries).Count -ne $expectedPaths.Count) {
        Add-R0ArchitectureReviewIssue $issues "Authority snapshot must contain $($expectedPaths.Count) entries."
    }
    else {
        for ($index = 0; $index -lt $expectedPaths.Count; ++$index) {
            $entry = $Contract.authority_snapshot.entries[$index]
            Test-R0ArchitectureReviewProperties -Object $entry -Expected @('path', 'normalized_byte_length', 'normalized_sha256') -Label "Authority snapshot row $index" -Issues $issues
            if ([string]$entry.path -cne $expectedPaths[$index]) {
                Add-R0ArchitectureReviewIssue $issues "Authority snapshot row $index path drifted."
                continue
            }
            $actualRecord = Get-R0ArchitectureNormalizedSourceRecord -RepoRoot $RepoRoot -RelativePath $expectedPaths[$index] -ContentOverrides $ContentOverrides -Issues $issues
            if ($null -ne $actualRecord) {
                if ([int64]$entry.normalized_byte_length -ne [int64]$actualRecord.normalized_byte_length -or
                    [string]$entry.normalized_sha256 -cne [string]$actualRecord.normalized_sha256) {
                    Add-R0ArchitectureReviewIssue $issues "Authority snapshot digest drifted for '$($entry.path)'."
                }
            }
        }
    }

    Test-R0ArchitectureReviewProperties -Object $Contract.runtime_consumer_policy -Expected @(
        'allowed_consumer_count', 'scan_roots', 'forbidden_path_tokens') -Label 'Runtime consumer policy' -Issues $issues
    if ([int]$Contract.runtime_consumer_policy.allowed_consumer_count -ne 0) {
        Add-R0ArchitectureReviewIssue $issues 'Runtime consumer allowance must remain zero.'
    }
    Test-R0ArchitectureReviewSequence -Actual @($Contract.runtime_consumer_policy.scan_roots) -Expected @(
        'framework', 'packages', 'adapters', 'apps', 'user') -Label 'Runtime scan roots' -Issues $issues
    Test-R0ArchitectureReviewSequence -Actual @($Contract.runtime_consumer_policy.forbidden_path_tokens) -Expected @(
        'docs/architecture/architecture-baseline.json', 'docs/architecture/authority-registry.json',
        'docs/architecture/r0-architecture-review-contract.json', 'docs/quality/terminology-conformance-report.json') -Label 'Runtime forbidden tokens' -Issues $issues
    Test-R0ArchitectureRuntimeConsumers -RepoRoot $RepoRoot -Policy $Contract.runtime_consumer_policy -SyntheticConsumers $SyntheticConsumers -Issues $issues

    $expectedMutationIds = @(
        'missing-capability-section', 'duplicate-capability-identity', 'empty-capability-commitment',
        'empty-capability-activation-gate', 'unresolved-capability-authority', 'capability-status-promotion-drift',
        'module-source-root-drift', 'module-source-root-dot-segment', 'module-source-root-case-drift',
        'module-source-root-untracked', 'module-source-root-symlink', 'shared-symbol-owner-drift',
        'shared-symbol-kind-drift', 'shared-symbol-authority-drift', 'legacy-primary-owner-drift',
        'legacy-disposition-drift', 'legacy-secondary-consumer-drift', 'logical-boundary-physical-promotion',
        'legacy-reconciliation-outcome-drift', 'authority-source-hash-drift', 'derived-baseline-bom',
        'derived-report-crlf', 'cmake-unknown-module-dependency', 'cmake-variable-dependency',
        'cmake-generator-expression-dependency', 'cmake-alias-dependency', 'cmake-non-interface-visibility',
        'adr-second-dependency-block', 'adr-unsupported-dependency-line',
        'coordinated-dag-change-without-superseding-snapshot', 'registry-duplicate-json-key',
        'coordinated-new-legacy-mapping-without-superseding-snapshot', 'runtime-consumer', 'packages-user-physical-module',
        'composition-root-physical-module')
    Test-R0ArchitectureReviewSequence -Actual @($Contract.required_mutation_ids) -Expected $expectedMutationIds -Label 'Required mutation ids' -Issues $issues

    return [PSCustomObject]([ordered]@{ IsValid = $issues.Count -eq 0; Errors = @($issues) })
}

function Assert-R0ArchitectureReviewFailure {
    param(
        [System.Collections.Generic.List[string]]$Failures,
        [string]$Name,
        $Result,
        [string]$ExpectedText
    )
    if ($Result.IsValid) { [void]$Failures.Add("$Name was accepted"); return }
    if (-not (@($Result.Errors) | Where-Object {
                ([string]$_).IndexOf($ExpectedText, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
            } | Select-Object -First 1)) {
        [void]$Failures.Add("$Name failed for the wrong reason: $($Result.Errors -join ' | ')")
    }
}

function Assert-R0ArchitectureReviewThrow {
    param(
        [System.Collections.Generic.List[string]]$Failures,
        [string]$Name,
        [scriptblock]$Action,
        [string]$ExpectedText
    )
    try {
        & $Action
        [void]$Failures.Add("$Name was accepted")
    }
    catch {
        if ($_.Exception.Message.IndexOf($ExpectedText, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            [void]$Failures.Add("$Name failed for the wrong reason: $($_.Exception.Message)")
        }
    }
}

function Invoke-R0ArchitectureReviewMutations {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]$Contract,
        [Parameter(Mandatory = $true)]$Glossary,
        [Parameter(Mandatory = $true)]$Dependency,
        [Parameter(Mandatory = $true)]$Registry,
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$GlossaryText,
        [Parameter(Mandatory = $true)][string]$AdrText,
        [Parameter(Mandatory = $true)][string]$CMakeText,
        [Parameter(Mandatory = $true)][string]$ExpectedBaselineJson,
        [Parameter(Mandatory = $true)][string]$ExpectedReportJson
    )

    $failures = [System.Collections.Generic.List[string]]::new()
    $caseCount = 0

    foreach ($mutation in @(
            @{ name = 'missing-capability-section'; expected = 'No capability rows'; apply = { param($g) $g.Capabilities = @() } },
            @{ name = 'duplicate-capability-identity'; expected = 'Duplicate capability identity'; apply = { param($g) $g.Capabilities = @($g.Capabilities) + @($g.Capabilities[0]) } },
            @{ name = 'empty-capability-commitment'; expected = 'no current commitment'; apply = { param($g) $g.Capabilities[0].current_commitment = '' } },
            @{ name = 'empty-capability-activation-gate'; expected = 'no activation gate'; apply = { param($g) $g.Capabilities[0].activation_gate = '' } },
            @{ name = 'unresolved-capability-authority'; expected = 'unresolved authority reference'; apply = { param($g) $g.Capabilities[0].authority_refs = @('MISSING') } })) {
        ++$caseCount
        $mutatedGlossary = Copy-R0ArchitectureReviewJson $Glossary
        & $mutation.apply $mutatedGlossary
        $result = Test-ArchitectureInputs -Glossary $mutatedGlossary -Dependency $Dependency -Registry $Registry -RepoRoot $RepoRoot
        Assert-R0ArchitectureReviewFailure -Failures $failures -Name $mutation.name -Result $result -ExpectedText $mutation.expected
    }

    ++$caseCount
    $capabilitySectionStart = $GlossaryText.IndexOf('## 8.', [System.StringComparison]::Ordinal)
    $legacySectionStart = if ($capabilitySectionStart -ge 0) {
        $GlossaryText.IndexOf('## 9.', $capabilitySectionStart, [System.StringComparison]::Ordinal)
    }
    else {
        -1
    }
    if ($capabilitySectionStart -lt 0 -or $legacySectionStart -le $capabilitySectionStart) {
        $failures.Add('capability-status-promotion-drift mutation could not isolate the capability section.')
    }
    else {
        $capabilitySectionText = $GlossaryText.Substring(
            $capabilitySectionStart,
            $legacySectionStart - $capabilitySectionStart)
        $capabilityStatusPattern = [regex]::new(
            '^(\| `RenderSnapshot`[^|\r\n]*\| )PressureOnly( \|)',
            [System.Text.RegularExpressions.RegexOptions]::Multiline)
        $mutatedCapabilitySectionText = $capabilityStatusPattern.Replace(
            $capabilitySectionText,
            '${1}V1${2}',
            1)
        if ($mutatedCapabilitySectionText -ceq $capabilitySectionText) {
            $failures.Add('capability-status-promotion-drift mutation did not alter the target capability row.')
        }
        else {
            $mutatedGlossaryText =
                $GlossaryText.Substring(0, $capabilitySectionStart) +
                $mutatedCapabilitySectionText +
                $GlossaryText.Substring($legacySectionStart)
            $result = Test-R0ArchitectureReviewContract -Contract $Contract -Registry $Registry -RepoRoot $RepoRoot -ContentOverrides @{
                'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md' = $mutatedGlossaryText }
            Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'capability-status-promotion-drift' -Result $result -ExpectedText 'snapshot digest drifted'
        }
    }

    foreach ($mutation in @(
            @{ name = 'module-source-root-drift'; expected = 'snapshot digest drifted'; apply = { param($r) $temp = $r.modules[0].source_root; $r.modules[0].source_root = $r.modules[1].source_root; $r.modules[1].source_root = $temp } },
            @{ name = 'shared-symbol-owner-drift'; expected = 'snapshot digest drifted'; apply = { param($r) $r.shared_symbols[0].owner_module = 'foundation' } },
            @{ name = 'shared-symbol-kind-drift'; expected = 'snapshot digest drifted'; apply = { param($r) $r.shared_symbols[0].kind = 'key' } },
            @{ name = 'shared-symbol-authority-drift'; expected = 'snapshot digest drifted'; apply = { param($r) $r.shared_symbols[0].semantic_authority = 'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md' } },
            @{ name = 'legacy-primary-owner-drift'; expected = 'snapshot digest drifted'; apply = { param($r) $r.legacy_ownership[0].primary_owner = 'compiler' } },
            @{ name = 'legacy-disposition-drift'; expected = 'snapshot digest drifted'; apply = { param($r) $r.legacy_ownership[0].disposition = 'replace' } },
            @{ name = 'legacy-secondary-consumer-drift'; expected = 'snapshot digest drifted'; apply = { param($r) $r.legacy_ownership[0].secondary_consumers = @('application') } })) {
        ++$caseCount
        $mutatedRegistry = Copy-R0ArchitectureReviewJson $Registry
        & $mutation.apply $mutatedRegistry
        $mutatedRegistryJson = ConvertTo-DeterministicJson -Value $mutatedRegistry
        $result = Test-R0ArchitectureReviewContract -Contract $Contract -Registry $mutatedRegistry -RepoRoot $RepoRoot -ContentOverrides @{
            'docs/architecture/authority-registry.json' = $mutatedRegistryJson }
        Assert-R0ArchitectureReviewFailure -Failures $failures -Name $mutation.name -Result $result -ExpectedText $mutation.expected
    }

    foreach ($sourceRootMutation in @(
            @{ name = 'module-source-root-dot-segment'; value = 'framework/include/gnc/../gnc/foundation' },
            @{ name = 'module-source-root-case-drift'; value = 'Framework/include/gnc/foundation' },
            @{ name = 'module-source-root-untracked'; value = 'build' })) {
        ++$caseCount
        $mutatedRegistry = Copy-R0ArchitectureReviewJson $Registry
        $mutatedRegistry.modules[0].source_root = $sourceRootMutation.value
        $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
        Assert-R0ArchitectureReviewFailure -Failures $failures -Name $sourceRootMutation.name -Result $result -ExpectedText 'source root'
    }

    ++$caseCount
    $syntheticSourceRootIssues = [System.Collections.Generic.List[string]]::new()
    Test-ArchitectureModuleSourceRootFacts `
        -ModuleName 'foundation' `
        -RelativePath 'framework/include/gnc/foundation' `
        -EscapesRepository $false `
        -Exists $true `
        -IsContainer $true `
        -IsReparsePoint $true `
        -HasExactTrackedFile $true `
        -Issues $syntheticSourceRootIssues
    $result = [PSCustomObject]@{ IsValid = $syntheticSourceRootIssues.Count -eq 0; Errors = @($syntheticSourceRootIssues) }
    Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'module-source-root-symlink' -Result $result -ExpectedText 'symlink or reparse point'

    ++$caseCount
    $mutatedContract = Copy-R0ArchitectureReviewJson $Contract
    $mutatedContract.logical_boundaries[0].physical_module = $true
    $result = Test-R0ArchitectureReviewContract -Contract $mutatedContract -Registry $Registry -RepoRoot $RepoRoot
    Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'logical-boundary-physical-promotion' -Result $result -ExpectedText 'physical-module flag drifted'

    ++$caseCount
    $mutatedContract = Copy-R0ArchitectureReviewJson $Contract
    $mutatedContract.legacy_reconciliation.outcome = 'adopt-candidate-overlay'
    $result = Test-R0ArchitectureReviewContract -Contract $mutatedContract -Registry $Registry -RepoRoot $RepoRoot
    Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'legacy-reconciliation-outcome-drift' -Result $result -ExpectedText 'policy drifted'

    ++$caseCount
    $mutatedContract = Copy-R0ArchitectureReviewJson $Contract
    $mutatedContract.authority_snapshot.entries[0].normalized_sha256 = '0' * 64
    $result = Test-R0ArchitectureReviewContract -Contract $mutatedContract -Registry $Registry -RepoRoot $RepoRoot
    Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'authority-source-hash-drift' -Result $result -ExpectedText 'snapshot digest drifted'

    $temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('gnczmkn-r0-arch-' + [guid]::NewGuid().ToString('N'))
    [void][System.IO.Directory]::CreateDirectory($temporaryRoot)
    try {
        ++$caseCount
        $bomPath = Join-Path $temporaryRoot 'baseline-bom.json'
        $bomBytes = [byte[]](0xEF, 0xBB, 0xBF) + [System.Text.UTF8Encoding]::new($false).GetBytes($ExpectedBaselineJson)
        [System.IO.File]::WriteAllBytes($bomPath, $bomBytes)
        if (Test-GeneratedContent -Path $bomPath -Expected $ExpectedBaselineJson) {
            [void]$failures.Add('derived-baseline-bom was accepted')
        }

        ++$caseCount
        $crlfPath = Join-Path $temporaryRoot 'report-crlf.json'
        $crlfText = $ExpectedReportJson.Replace("`n", "`r`n")
        [System.IO.File]::WriteAllBytes($crlfPath, [System.Text.UTF8Encoding]::new($false).GetBytes($crlfText))
        if (Test-GeneratedContent -Path $crlfPath -Expected $ExpectedReportJson) {
            [void]$failures.Add('derived-report-crlf was accepted')
        }

        ++$caseCount
        $duplicateJsonPath = Join-Path $temporaryRoot 'duplicate.json'
        [System.IO.File]::WriteAllText($duplicateJsonPath, '{"schema_version":"a","schema_version":"b"}', [System.Text.UTF8Encoding]::new($false))
        Assert-R0ArchitectureReviewThrow -Failures $failures -Name 'registry-duplicate-json-key' -ExpectedText 'duplicate JSON object key' -Action {
            [void](Read-StrictJsonFile -Path $duplicateJsonPath)
        }
    }
    finally {
        if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
            [System.IO.Directory]::Delete($temporaryRoot, $true)
        }
    }

    foreach ($cmakeMutation in @(
            @{ name = 'cmake-unknown-module-dependency'; expected = 'unknown or external target'; text = $CMakeText.Replace('target_link_libraries(kernel INTERFACE foundation contracts)', 'target_link_libraries(kernel INTERFACE foundation contracts external_runtime)') },
            @{ name = 'cmake-variable-dependency'; expected = 'unsupported dependency syntax'; text = $CMakeText.Replace('target_link_libraries(kernel INTERFACE foundation contracts)', 'target_link_libraries(kernel INTERFACE foundation contracts ${HIDDEN_DEP})') },
            @{ name = 'cmake-generator-expression-dependency'; expected = 'unsupported dependency syntax'; text = $CMakeText.Replace('target_link_libraries(kernel INTERFACE foundation contracts)', 'target_link_libraries(kernel INTERFACE foundation contracts $<LINK_ONLY:compiler>)') },
            @{ name = 'cmake-alias-dependency'; expected = 'unsupported dependency syntax'; text = $CMakeText.Replace('target_link_libraries(kernel INTERFACE foundation contracts)', 'target_link_libraries(kernel INTERFACE foundation contracts gnc::compiler)') },
            @{ name = 'cmake-non-interface-visibility'; expected = 'INTERFACE visibility'; text = $CMakeText.Replace('target_link_libraries(kernel INTERFACE foundation contracts)', 'target_link_libraries(kernel PUBLIC foundation contracts)') })) {
        ++$caseCount
        Assert-R0ArchitectureReviewThrow -Failures $failures -Name $cmakeMutation.name -ExpectedText $cmakeMutation.expected -Action {
            [void](ConvertFrom-DependencySources -AdrText $AdrText -CMakeText $cmakeMutation.text)
        }
    }

    ++$caseCount
    $fence = ([char]96).ToString() + ([char]96).ToString() + ([char]96).ToString()
    $secondBlock = $AdrText + "`n${fence}text`nfoundation <- contracts`n$fence`n"
    Assert-R0ArchitectureReviewThrow -Failures $failures -Name 'adr-second-dependency-block' -ExpectedText 'exactly one dependency text block' -Action {
        [void](ConvertFrom-DependencySources -AdrText $secondBlock -CMakeText $CMakeText)
    }

    ++$caseCount
    $unsupportedAdr = $AdrText.Replace('application <- adapters', "application <- adapters`nunsupported ???")
    Assert-R0ArchitectureReviewThrow -Failures $failures -Name 'adr-unsupported-dependency-line' -ExpectedText 'Unsupported ADR dependency expression' -Action {
        [void](ConvertFrom-DependencySources -AdrText $unsupportedAdr -CMakeText $CMakeText)
    }

    ++$caseCount
    $coordinatedAdr = $AdrText.Replace('foundation + contracts <- kernel', 'foundation + contracts + compiler <- kernel')
    $coordinatedCMake = $CMakeText.Replace('target_link_libraries(kernel INTERFACE foundation contracts)', 'target_link_libraries(kernel INTERFACE foundation contracts compiler)')
    $result = Test-R0ArchitectureReviewContract -Contract $Contract -Registry $Registry -RepoRoot $RepoRoot -ContentOverrides @{
        'docs/adr/0003-initial-module-dependency-dag.md' = $coordinatedAdr
        'CMakeLists.txt' = $coordinatedCMake }
    Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'coordinated-dag-change-without-superseding-snapshot' -Result $result -ExpectedText 'snapshot digest drifted'

    ++$caseCount
    $mutatedRegistry = Copy-R0ArchitectureReviewJson $Registry
    $mutatedRegistry.legacy_ownership = @($mutatedRegistry.legacy_ownership) + @(
        [PSCustomObject]@{ legacy_name = 'IIntegrator'; disposition = 'replace'; primary_owner = 'foundation' })
    $mutatedRegistryJson = ConvertTo-DeterministicJson -Value $mutatedRegistry
    $mutatedGlossaryText = $GlossaryText.TrimEnd() + "`n" + '| `IIntegrator` | Legacy | `NumericalExtension` | 01 |' + "`n"
    $result = Test-R0ArchitectureReviewContract -Contract $Contract -Registry $mutatedRegistry -RepoRoot $RepoRoot -ContentOverrides @{
        'docs/architecture/authority-registry.json' = $mutatedRegistryJson
        'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md' = $mutatedGlossaryText }
    Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'coordinated-new-legacy-mapping-without-superseding-snapshot' -Result $result -ExpectedText 'snapshot digest drifted'

    ++$caseCount
    $result = Test-R0ArchitectureReviewContract -Contract $Contract -Registry $Registry -RepoRoot $RepoRoot -SyntheticConsumers @{
        'framework/include/gnc/foundation/forbidden.hpp' = 'load docs/architecture/architecture-baseline.json' }
    Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'runtime-consumer' -Result $result -ExpectedText 'runtime consumer'

    ++$caseCount
    $mutatedRegistry = Copy-R0ArchitectureReviewJson $Registry
    $mutatedRegistry.modules = @($mutatedRegistry.modules) + @([PSCustomObject]@{ name = 'packages_user'; source_root = 'packages' })
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'packages-user-physical-module' -Result $result -ExpectedText "module 'packages_user' is absent from ADR-0003"

    ++$caseCount
    $mutatedRegistry = Copy-R0ArchitectureReviewJson $Registry
    $mutatedRegistry.modules = @($mutatedRegistry.modules) + @([PSCustomObject]@{ name = 'composition_root'; source_root = 'apps' })
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    Assert-R0ArchitectureReviewFailure -Failures $failures -Name 'composition-root-physical-module' -Result $result -ExpectedText "module 'composition_root' is absent from ADR-0003"

    return [PSCustomObject]([ordered]@{
        IsValid = $failures.Count -eq 0
        CaseCount = $caseCount
        Failures = @($failures)
    })
}

Export-ModuleMember -Function @(
    'Get-R0ArchitectureSnapshotRecords',
    'Test-R0ArchitectureReviewContract',
    'Invoke-R0ArchitectureReviewMutations')

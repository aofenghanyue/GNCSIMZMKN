[CmdletBinding()]
param(
    [switch]$RequireDecisionReady,
    [switch]$Json,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$errors = [System.Collections.Generic.List[string]]::new()
$negativeResults = [System.Collections.Generic.List[object]]::new()

function Get-Field([object]$Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Test-HasField([object]$Object, [string]$Name) {
    return $null -ne $Object -and $null -ne $Object.PSObject.Properties[$Name]
}

function Read-Json([string]$RelativePath) {
    $path = Join-Path $repoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required R0 gate input is missing: $RelativePath"
    }
    return Get-Content -LiteralPath $path -Raw -Encoding utf8 | ConvertFrom-Json
}

function Copy-JsonObject([object]$Value) {
    return $Value | ConvertTo-Json -Depth 100 | ConvertFrom-Json
}

function Test-ExactStringSet([string[]]$Actual, [string[]]$Expected) {
    $actualText = (@($Actual | Sort-Object) -join "`n")
    $expectedText = (@($Expected | Sort-Object) -join "`n")
    return $actualText -ceq $expectedText
}

function Test-GateInputs(
    [object]$Backlog,
    [object]$ArchitectureReport,
    [object]$LegacyOracleManifest,
    [object]$ConventionFixture,
    [object]$ConventionReport,
    [object]$MinimalFixture,
    [object]$MinimalReference,
    [object]$YyzFixture,
    [object]$YyzReference,
    [object]$CavhFixture,
    [object]$CavhReference,
    [object]$ProvenanceInventory) {
    $issues = [System.Collections.Generic.List[string]]::new()
    $blockers = [System.Collections.Generic.List[string]]::new()

    $expectedDependencies = @(
        'R0-GOV-001',
        'R0-GOV-002',
        'R0-ARCH-002',
        'R0-LEG-002',
        'R0-SCI-002',
        'R0-SCI-003',
        'R0-SCI-004',
        'R0-SPEC-001',
        'R0-PERF-001')
    $tasks = @(Get-Field $Backlog 'tasks')
    $gateTasks = @($tasks | Where-Object {
            [string](Get-Field $_ 'id') -ceq 'R0-GATE-001'
        })
    $dependencyDone = 0
    if ($gateTasks.Count -ne 1) {
        $issues.Add('Backlog must contain exactly one R0-GATE-001 task.')
    }
    else {
        $gateTask = $gateTasks[0]
        $actualDependencies = @(
            @(Get-Field $gateTask 'depends_on') | ForEach-Object { [string]$_ })
        if (-not (Test-ExactStringSet $actualDependencies $expectedDependencies)) {
            $issues.Add('R0-GATE-001 dependency set differs from the accepted R0 gate inputs.')
        }
        foreach ($dependencyId in $expectedDependencies) {
            $matches = @($tasks | Where-Object {
                    [string](Get-Field $_ 'id') -ceq $dependencyId
                })
            if ($matches.Count -ne 1) {
                $issues.Add("Gate dependency is missing or duplicated: $dependencyId")
            }
            elseif ((Get-Field $matches[0] 'status') -ne 'done') {
                $issues.Add("Gate dependency is not done: $dependencyId")
            }
            else {
                $dependencyDone += 1
            }
        }
    }

    $architectureChecks = @(Get-Field $ArchitectureReport 'checks')
    if ((Get-Field $ArchitectureReport 'status') -ne 'conformant') {
        $issues.Add('G0 architecture terminology/dependency report is not conformant.')
    }
    foreach ($check in $architectureChecks) {
        if ((Get-Field $check 'status') -ne 'passed') {
            $issues.Add("G0 architecture check did not pass: $([string](Get-Field $check 'id'))")
        }
    }

    $legacyOracles = @(Get-Field $LegacyOracleManifest 'oracles')
    if ((Get-Field $LegacyOracleManifest 'status') -ne 'executable') {
        $issues.Add('Legacy behavior oracle manifest is not executable.')
    }
    foreach ($oracle in $legacyOracles) {
        if ((Get-Field $oracle 'status') -ne 'executable') {
            $issues.Add("Legacy behavior oracle is not executable: $([string](Get-Field $oracle 'id'))")
        }
        if (@(Get-Field $oracle 'artifact_refs').Count -eq 0) {
            $issues.Add("Legacy behavior oracle has no executable artifact: $([string](Get-Field $oracle 'id'))")
        }
    }

    if ((Get-Field $ConventionFixture 'status') -ne 'executable' -or
        (Get-Field $ConventionReport 'status') -ne 'pass') {
        $issues.Add('Scientific convention fixture and cross-tool result are not executable/pass.')
    }

    $requiredMinimalCases = @(
        'CASE-MIN3D-CONSTANT-ACCELERATION',
        'CASE-MIN3D-LINEAR-DRAG-CONVERGENCE',
        'CASE-MIN3D-EXACT-GRID-TERMINATION',
        'CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE')
    $minimalCases = Get-Field $MinimalReference 'cases'
    $minimalCaseIds = if ($null -eq $minimalCases) {
        @()
    }
    else {
        @($minimalCases.PSObject.Properties | ForEach-Object { [string]$_.Name })
    }
    if ((Get-Field $MinimalFixture 'status') -ne 'executable' -or
        -not (Test-ExactStringSet $minimalCaseIds $requiredMinimalCases)) {
        $issues.Add('Minimal 3DoF does not expose the accepted analytic, convergence, termination and stage-failure cases.')
    }
    $minimalInvalidCases = @(Get-Field $MinimalReference 'invalid_input_cases')
    if ($minimalInvalidCases.Count -eq 0 -or
        @($minimalInvalidCases | Where-Object {
                (Get-Field $_ 'expected_status') -ne 'input-domain-error'
            }).Count -gt 0) {
        $issues.Add('Minimal 3DoF input-domain failures are incomplete.')
    }

    $yyzDifferenceReport = Get-Field $YyzReference 'difference_report'
    $yyzFailedFields = 0
    if (-not (Test-HasField $yyzDifferenceReport 'failed_field_count')) {
        $issues.Add('YYZ qualification has no failed-field count.')
    }
    else {
        $yyzFailedFields = [int](Get-Field $yyzDifferenceReport 'failed_field_count')
        if ($yyzFailedFields -ne 0) {
            $issues.Add('YYZ qualification contains unresolved scientific differences.')
        }
    }
    $yyzSourceIdentity = Get-Field $YyzReference 'source_identity'
    if ((Get-Field $YyzFixture 'status') -ne 'executable' -or
        (Get-Field $yyzSourceIdentity 'qualification_status') -ne 'executable') {
        $issues.Add('YYZ R0 qualification bundle is not executable.')
    }
    if ((Get-Field $yyzSourceIdentity 'target_status') -ne 'target_pending' -or
        (Get-Field $yyzSourceIdentity 'target_scientific_verdict_contribution') -ne 'none') {
        $issues.Add('The future YYZ target profile leaked into the R0 scientific verdict.')
    }
    foreach ($component in @(Get-Field (Get-Field $YyzReference 'asset_resolution') 'components')) {
        if ((Get-Field $component 'status') -ne 'passed') {
            $issues.Add("YYZ component qualification did not pass: $([string](Get-Field $component 'role'))")
        }
    }
    foreach ($asset in @(Get-Field (Get-Field $YyzReference 'asset_resolution') 'selected_assets')) {
        if ((Get-Field $asset 'status') -ne 'passed') {
            $issues.Add("YYZ selected asset did not pass: $([string](Get-Field $asset 'role'))")
        }
    }

    if ((Get-Field $CavhFixture 'status') -ne 'executable' -or
        (Get-Field $CavhReference 'status') -ne 'executable') {
        $issues.Add('CAVH formula qualification bundle is not executable.')
    }
    $cavhMutations = @(Get-Field $CavhReference 'mutation_results')
    if ($cavhMutations.Count -eq 0 -or
        @($cavhMutations | Where-Object {
                (Get-Field $_ 'status') -ne 'rejected'
            }).Count -gt 0) {
        $issues.Add('CAVH scientific mutation rejection is incomplete.')
    }
    $cavhInvalidStatuses = @(
        @(Get-Field $CavhReference 'invalid_input_results') | ForEach-Object {
            [string](Get-Field $_ 'status')
        })
    foreach ($requiredStatus in @('derivative-degenerate', 'formula-singularity')) {
        if ($requiredStatus -notin $cavhInvalidStatuses) {
            $issues.Add("CAVH required failure result is missing: $requiredStatus")
        }
    }

    $repository = Get-Field $ProvenanceInventory 'repository'
    if ((Get-Field $repository 'owner_decision') -ne 'accepted' -or
        (Get-Field $repository 'g1_scope') -ne
            'internal-development-no-new-external-distribution' -or
        (Get-Field $repository 'external_distribution') -ne 'blocked') {
        $issues.Add('Accepted internal-development G1 distribution boundary is inconsistent.')
    }
    $publicExposure = Get-Field $repository 'existing_public_exposure'
    $originVisibility = [string](Get-Field $publicExposure 'origin_visibility')
    $knownPublicForks = @(Get-Field $publicExposure 'known_public_forks')
    if ($originVisibility -ne 'private' -or
        (Get-Field $publicExposure 'owner_disposition') -ne 'resolved') {
        $blockers.Add('public-origin-private-transition-required')
    }

    return [pscustomobject][ordered]@{
        issues = @($issues)
        decision_blockers = @($blockers)
        facts = [pscustomobject][ordered]@{
            dependencies_done = $dependencyDone
            dependencies_required = $expectedDependencies.Count
            architecture_checks_passed = @($architectureChecks | Where-Object {
                    (Get-Field $_ 'status') -eq 'passed'
                }).Count
            architecture_checks_required = $architectureChecks.Count
            legacy_oracles_executable = @($legacyOracles | Where-Object {
                    (Get-Field $_ 'status') -eq 'executable'
                }).Count
            legacy_oracles_required = $legacyOracles.Count
            minimal_3dof_cases = $minimalCaseIds.Count
            yyz_failed_fields = $yyzFailedFields
            cavh_mutations_rejected = @($cavhMutations | Where-Object {
                    (Get-Field $_ 'status') -eq 'rejected'
                }).Count
            origin_visibility = $originVisibility
            known_public_forks = $knownPublicForks.Count
        }
    }
}

function Invoke-NegativeCase(
    [string]$Name,
    [string]$ExpectedDiagnostic,
    [object]$Evaluation) {
    $issues = @($Evaluation.issues)
    $matched = @($issues | Where-Object {
            $_.IndexOf($ExpectedDiagnostic, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
        }).Count -gt 0
    if (-not $matched) {
        $script:errors.Add(
            "Negative case '$Name' did not produce '$ExpectedDiagnostic': $($issues -join ' | ')")
    }
    $script:negativeResults.Add([pscustomobject][ordered]@{
            name = $Name
            rejected = $matched
        })
}

try {
    $backlog = Read-Json 'docs\tasks\backlog.json'
    $architectureReport = Read-Json 'docs\quality\terminology-conformance-report.json'
    $legacyOracleManifest = Read-Json 'oracles\oracle-manifest.json'
    $conventionFixture = Read-Json 'fixtures\ref-scientific-conventions\fixture-manifest.json'
    $conventionReport = Read-Json 'docs\quality\scientific-conventions-cross-tool-report.json'
    $minimalFixture = Read-Json 'fixtures\ref-minimal-3dof\fixture-manifest.json'
    $minimalReference = Read-Json 'oracles\ref-minimal-3dof\reference.json'
    $yyzFixture = Read-Json 'fixtures\ref-yyz-001\fixture-manifest.json'
    $yyzReference = Read-Json 'oracles\ref-yyz-001\reference.json'
    $cavhFixture = Read-Json 'fixtures\ref-cavh-formula\fixture-manifest.json'
    $cavhReference = Read-Json 'oracles\ref-cavh-formula\reference.json'
    $provenanceInventory = Read-Json 'docs\governance\provenance-inventory.json'

    $evaluation = Test-GateInputs `
        $backlog `
        $architectureReport `
        $legacyOracleManifest `
        $conventionFixture `
        $conventionReport `
        $minimalFixture `
        $minimalReference `
        $yyzFixture `
        $yyzReference `
        $cavhFixture `
        $cavhReference `
        $provenanceInventory
    foreach ($issue in @($evaluation.issues)) {
        $errors.Add([string]$issue)
    }

    $candidate = Copy-JsonObject $backlog
    $candidateDependency = @($candidate.tasks | Where-Object {
            $_.id -ceq 'R0-GOV-002'
        })[0]
    $candidateDependency.status = 'blocked'
    Invoke-NegativeCase `
        'incomplete-gate-dependency' `
        'Gate dependency is not done' `
        (Test-GateInputs `
            $candidate $architectureReport $legacyOracleManifest `
            $conventionFixture $conventionReport $minimalFixture $minimalReference `
            $yyzFixture $yyzReference $cavhFixture $cavhReference $provenanceInventory)

    $candidate = Copy-JsonObject $architectureReport
    $candidate.status = 'nonconformant'
    Invoke-NegativeCase `
        'nonconformant-architecture' `
        'architecture terminology/dependency report is not conformant' `
        (Test-GateInputs `
            $backlog $candidate $legacyOracleManifest `
            $conventionFixture $conventionReport $minimalFixture $minimalReference `
            $yyzFixture $yyzReference $cavhFixture $cavhReference $provenanceInventory)

    $candidate = Copy-JsonObject $legacyOracleManifest
    $candidate.oracles[0].status = 'capturing'
    Invoke-NegativeCase `
        'non-executable-legacy-oracle' `
        'Legacy behavior oracle is not executable' `
        (Test-GateInputs `
            $backlog $architectureReport $candidate `
            $conventionFixture $conventionReport $minimalFixture $minimalReference `
            $yyzFixture $yyzReference $cavhFixture $cavhReference $provenanceInventory)

    $candidate = Copy-JsonObject $yyzReference
    $candidate.difference_report.failed_field_count = 1
    Invoke-NegativeCase `
        'unresolved-yyz-difference' `
        'unresolved scientific differences' `
        (Test-GateInputs `
            $backlog $architectureReport $legacyOracleManifest `
            $conventionFixture $conventionReport $minimalFixture $minimalReference `
            $yyzFixture $candidate $cavhFixture $cavhReference $provenanceInventory)

    $decisionBlockers = @($evaluation.decision_blockers)
    $summary = [pscustomobject][ordered]@{
        task_id = 'R0-GATE-001'
        validation = if ($errors.Count -eq 0) { 'passed' } else { 'failed' }
        technical_inputs = [pscustomobject][ordered]@{
            ready = $errors.Count -eq 0
            facts = $evaluation.facts
            unexplained_scientific_differences = $evaluation.facts.yyz_failed_fields
        }
        owner_decision = [pscustomobject][ordered]@{
            ready = $errors.Count -eq 0 -and $decisionBlockers.Count -eq 0
            required = $true
            blockers = $decisionBlockers
        }
        negative_cases = @($negativeResults)
        validation_errors = @($errors)
    }

    if ($errors.Count -gt 0) {
        if ($Json) {
            $summary | ConvertTo-Json -Depth 10
        }
        else {
            Write-Host "R0 gate readiness validation failed with $($errors.Count) issue(s):"
            foreach ($errorMessage in $errors) {
                Write-Host " - $errorMessage"
            }
        }
        exit 1
    }

    if ($RequireDecisionReady -and $decisionBlockers.Count -gt 0) {
        if ($Json) {
            $summary | ConvertTo-Json -Depth 10
        }
        elseif (-not $Quiet) {
            Write-Host 'R0 gate inputs pass, but owner decision readiness is blocked:'
            foreach ($blocker in $decisionBlockers) {
                Write-Host " - $blocker"
            }
        }
        exit 2
    }

    if ($Json) {
        $summary | ConvertTo-Json -Depth 10
    }
    elseif (-not $Quiet) {
        Write-Host 'R0 gate technical inputs passed.'
        Write-Host "Gate dependencies: $($evaluation.facts.dependencies_done)/$($evaluation.facts.dependencies_required)"
        Write-Host "Unexplained scientific differences: $($evaluation.facts.yyz_failed_fields)"
        Write-Host "Owner decision readiness blockers: $($decisionBlockers.Count)"
        Write-Host "Negative cases rejected: $(@($negativeResults | Where-Object { $_.rejected }).Count)/$($negativeResults.Count)"
    }
}
catch {
    if ($Json) {
        [pscustomobject][ordered]@{
            task_id = 'R0-GATE-001'
            validation = 'failed'
            error = $_.Exception.Message
        } | ConvertTo-Json -Depth 4
    }
    else {
        Write-Host "R0 gate readiness validation failed: $($_.Exception.Message)"
    }
    exit 1
}

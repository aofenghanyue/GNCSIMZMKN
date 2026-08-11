[CmdletBinding()]
param(
    [switch]$UpdateReport,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$rolesPath = Join-Path $repoRoot 'docs\team\role-assignments.json'
$matrixPath = Join-Path $repoRoot 'docs\governance\toolchain-support-matrix.json'
$backlogPath = Join-Path $repoRoot 'docs\tasks\backlog.json'
$presetsPath = Join-Path $repoRoot 'CMakePresets.json'
$manifestPath = Join-Path $repoRoot 'project-manifest.json'
$adrPath = Join-Path $repoRoot 'docs\adr\0009-accountable-roles-and-candidate-toolchain.md'
$workflowPath = Join-Path $repoRoot '.github\workflows\ci.yml'
$cmakePath = Join-Path $repoRoot 'CMakeLists.txt'
$reportPath = Join-Path $repoRoot 'docs\quality\team-toolchain-readiness-report.json'
$errors = [System.Collections.Generic.List[string]]::new()
$mutationResults = [System.Collections.Generic.List[object]]::new()

function Add-Error([string]$Message) {
    $script:errors.Add($Message)
}

function Read-Json([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-Error "Required JSON is missing: $Path"
        return $null
    }
    try {
        return Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
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

function Test-Identity([object]$Value) {
    $text = [string]$Value
    if ([string]::IsNullOrWhiteSpace($text)) { return $false }
    return $text.Trim() -notmatch '^(?i:codex(?:[\s_-].*)?|unassigned|tbd|todo|unknown|n/?a|none|null|placeholder)$'
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

function Copy-JsonObject([object]$Object) {
    return $Object | ConvertTo-Json -Depth 100 | ConvertFrom-Json
}

function Get-RoleReadiness([object]$Roles) {
    $required = @()
    if ($null -ne $Roles) {
        $required = @((Get-Field $Roles 'roles') | Where-Object {
                (Get-Field $_ 'required') -eq $true
            })
    }
    $assigned = @($required | Where-Object {
            Test-Identity (Get-Field $_ 'assignee')
        }).Count
    $reviewed = @($required | Where-Object {
            Test-Identity (Get-Field $_ 'reviewer')
        }).Count
    $validPairs = @($required | Where-Object {
            $assignee = [string](Get-Field $_ 'assignee')
            $reviewer = [string](Get-Field $_ 'reviewer')
            (Test-Identity $assignee) -and
            (Test-Identity $reviewer) -and
            $assignee -ne $reviewer
        }).Count

    $highRiskIndependent = $false
    if ($null -ne $Roles) {
        $scientific = @((Get-Field $Roles 'roles') | Where-Object {
                (Get-Field $_ 'id') -eq 'scientific_authority'
            }) | Select-Object -First 1
        $architecture = @((Get-Field $Roles 'roles') | Where-Object {
                (Get-Field $_ 'id') -eq 'architecture_lead'
            }) | Select-Object -First 1
        $scientificAssignee = [string](Get-Field $scientific 'assignee')
        $architectureAssignee = [string](Get-Field $architecture 'assignee')
        $highRiskIndependent =
            (Test-Identity $scientificAssignee) -and
            (Test-Identity $architectureAssignee) -and
            $scientificAssignee -ne $architectureAssignee
    }

    return [pscustomobject][ordered]@{
        required_role_count = $required.Count
        assigned_required_roles = $assigned
        reviewed_required_roles = $reviewed
        valid_required_pairs = $validPairs
        missing_required_slots = (2 * $required.Count) - $assigned - $reviewed
        high_risk_assignees_independent = $highRiskIndependent
        ready = (
            $required.Count -gt 0 -and
            $validPairs -eq $required.Count -and
            $highRiskIndependent)
    }
}

function Test-GovernanceObjects(
    [object]$Roles,
    [object]$Matrix,
    [object]$Backlog,
    [object]$Presets,
    [object]$Manifest,
    [string]$AdrText,
    [string]$WorkflowText,
    [string]$CMakeText) {
    $issues = [System.Collections.Generic.List[string]]::new()

    if ((Get-Field $Roles 'schema_version') -ne 'gnczmkn.team-roles/2') {
        $issues.Add('Unsupported team-role schema.')
    }
    $assignmentPolicy = Get-Field $Roles 'assignment_policy'
    if ((Get-Field $assignmentPolicy 'reviewer_must_differ_from_assignee') -ne $true -or
        (Get-Field $assignmentPolicy 'placeholder_assignments_forbidden') -ne $true) {
        $issues.Add('Team-role assignment policy does not enforce independent, real identities.')
    }
    $highRiskPolicy = Get-Field $assignmentPolicy 'high_risk_independence'
    if ((@(Get-Field $highRiskPolicy 'roles') -join ',') -ne
        'scientific_authority,architecture_lead' -or
        (Get-Field $highRiskPolicy 'assignees_must_differ') -ne $true) {
        $issues.Add('Scientific/architecture high-risk independence policy is incomplete.')
    }

    $expectedRoleIds = @(
        'product_owner',
        'scientific_authority',
        'architecture_lead',
        'model_sdk_lead',
        'compiler_lead',
        'runtime_numerics_lead',
        'evidence_workflow_lead',
        'application_lead',
        'validation_lead')
    $roleById = @{}
    foreach ($role in @(Get-Field $Roles 'roles')) {
        foreach ($field in @('id', 'assignee', 'reviewer', 'required')) {
            if (-not (Test-HasField $role $field)) {
                $issues.Add("Role is missing required field '$field'.")
            }
        }
        $id = [string](Get-Field $role 'id')
        if ($roleById.ContainsKey($id)) {
            $issues.Add("Duplicate role id: $id")
        }
        else {
            $roleById[$id] = $role
        }
        $assignee = Get-Field $role 'assignee'
        $reviewer = Get-Field $role 'reviewer'
        if ($null -ne $assignee -and -not (Test-Identity $assignee)) {
            $issues.Add("Role '$id' has a placeholder assignee.")
        }
        if ($null -ne $reviewer -and -not (Test-Identity $reviewer)) {
            $issues.Add("Role '$id' has a placeholder reviewer.")
        }
        if ((Test-Identity $assignee) -xor (Test-Identity $reviewer)) {
            $issues.Add("Role '$id' has only one side of the assignee/reviewer pair.")
        }
        if ((Test-Identity $assignee) -and (Test-Identity $reviewer) -and
            [string]$assignee -eq [string]$reviewer) {
            $issues.Add("Role '$id' has the same assignee and reviewer.")
        }
    }
    if ((($roleById.Keys | Sort-Object) -join ',') -ne
        (($expectedRoleIds | Sort-Object) -join ',')) {
        $issues.Add('Team-role identity set differs from the nine-role operating model.')
    }
    foreach ($id in $expectedRoleIds) {
        if (-not $roleById.ContainsKey($id)) { continue }
        $expectedRequired = $id -ne 'application_lead'
        if ((Get-Field $roleById[$id] 'required') -ne $expectedRequired) {
            $issues.Add("Role '$id' has an incorrect required flag.")
        }
    }

    $roleReadiness = Get-RoleReadiness $Roles
    if ($roleById.ContainsKey('scientific_authority') -and
        $roleById.ContainsKey('architecture_lead')) {
        $scientificAssignee = Get-Field $roleById['scientific_authority'] 'assignee'
        $architectureAssignee = Get-Field $roleById['architecture_lead'] 'assignee'
        if ((Test-Identity $scientificAssignee) -and
            (Test-Identity $architectureAssignee) -and
            [string]$scientificAssignee -eq [string]$architectureAssignee) {
            $issues.Add('Scientific Authority and Architecture Lead share the same high-risk assignee.')
        }
    }
    $roleDecisionStatus = [string](Get-Field $Roles 'decision_status')
    if ($roleDecisionStatus -notin @('incomplete', 'complete')) {
        $issues.Add("Unsupported role decision status '$roleDecisionStatus'.")
    }
    if ($roleDecisionStatus -eq 'complete' -and -not $roleReadiness.ready) {
        $issues.Add('Role decision claims complete while required assignments are not ready.')
    }

    if ((Get-Field $Matrix 'schema_version') -ne 'gnczmkn.toolchain-support-matrix/1' -or
        (Get-Field $Matrix 'task_id') -ne 'R0-GOV-001') {
        $issues.Add('Unsupported toolchain matrix identity.')
    }
    if ((Get-Field $Matrix 'language_standard') -ne 'C++17') {
        $issues.Add('Toolchain matrix language standard must remain C++17.')
    }
    $cmakeContract = Get-Field $Matrix 'cmake_contract'
    if ((Get-Field $cmakeContract 'declared_minimum') -ne '3.20.0' -or
        (Get-Field $cmakeContract 'preset_schema_version') -ne 2 -or
        (Get-Field $cmakeContract 'preset_schema_minimum') -ne '3.20.0') {
        $issues.Add('Toolchain matrix CMake/preset contract differs from the D-003 floor.')
    }
    $qualificationPolicy = Get-Field $Matrix 'qualification_policy'
    if ((@(Get-Field $qualificationPolicy 'supported_requires') -join ',') -ne
        'accepted-adr,complete-required-role-assignments,successful-fixed-runner-ci' -or
        (Get-Field $qualificationPolicy 'hosted_runner_identity') -ne
        'fixed-image-family-plus-exact-per-run-identity' -or
        (Get-Field $qualificationPolicy 'warnings_as_errors') -ne $true -or
        (Get-Field $qualificationPolicy 'required_configuration') -ne 'Release') {
        $issues.Add('Toolchain qualification policy is incomplete.')
    }

    $profiles = @(Get-Field $Matrix 'profiles')
    $profileById = @{}
    foreach ($profile in $profiles) {
        $id = [string](Get-Field $profile 'id')
        if ($profileById.ContainsKey($id)) {
            $issues.Add("Duplicate toolchain profile id: $id")
        }
        else {
            $profileById[$id] = $profile
        }
        if ((Get-Field $profile 'classification') -ne 'candidate-primary') {
            $issues.Add("Profile '$id' is not candidate-primary.")
        }
        $platform = Get-Field $profile 'platform'
        $compiler = Get-Field $profile 'compiler'
        $build = Get-Field $profile 'build'
        $tools = Get-Field $profile 'required_tools'
        $versionRange = [string](Get-Field $compiler 'version_range')
        if ((Get-Field $platform 'architecture') -ne 'x64' -or
            [string]::IsNullOrWhiteSpace([string](Get-Field $platform 'os_family')) -or
            [string]::IsNullOrWhiteSpace([string](Get-Field $platform 'hosted_runner')) -or
            [string]::IsNullOrWhiteSpace([string](Get-Field $compiler 'family')) -or
            $versionRange -notmatch '^>=[0-9]+\.[0-9]+\.[0-9]+ <[0-9]+\.[0-9]+\.[0-9]+$' -or
            (Get-Field $build 'declared_cmake_minimum') -ne '3.20.0' -or
            [string]::IsNullOrWhiteSpace([string](Get-Field $build 'ci_generator')) -or
            (Get-Field $build 'configuration') -ne 'Release' -or
            (Get-Field $build 'warnings_as_errors') -ne $true -or
            [string]::IsNullOrWhiteSpace([string](Get-Field $tools 'powershell')) -or
            (Get-Field $tools 'python') -ne '>=3.8' -or
            @(Get-Field $profile 'evidence_refs').Count -eq 0) {
            $issues.Add("Candidate profile '$id' is missing a bounded compiler/build/evidence contract.")
        }
    }
    $expectedProfileIds = @('ubuntu-24.04-x64-gcc-13', 'windows-x64-msvc-19.5x')
    if ((($profileById.Keys | Sort-Object) -join ',') -ne
        (($expectedProfileIds | Sort-Object) -join ',')) {
        $issues.Add('Candidate-primary profile set must contain exactly Windows/MSVC and Ubuntu/GCC.')
    }
    if ($profileById.ContainsKey('windows-x64-msvc-19.5x')) {
        $profile = $profileById['windows-x64-msvc-19.5x']
        if ((Get-Field (Get-Field $profile 'platform') 'hosted_runner') -ne
            'windows-2025-vs2026' -or
            (Get-Field (Get-Field $profile 'compiler') 'family') -ne 'MSVC' -or
            (Get-Field (Get-Field $profile 'compiler') 'version_range') -ne
            '>=19.50.0 <19.60.0' -or
            (Get-Field (Get-Field $profile 'build') 'ci_generator') -ne
            'Visual Studio 18 2026' -or
            (Get-Field (Get-Field $profile 'build') 'ci_generator_cmake_minimum') -ne
            '4.2.0') {
            $issues.Add('Windows candidate profile differs from the frozen MSVC 19.5x contract.')
        }
    }
    if ($profileById.ContainsKey('ubuntu-24.04-x64-gcc-13')) {
        $profile = $profileById['ubuntu-24.04-x64-gcc-13']
        if ((Get-Field (Get-Field $profile 'platform') 'hosted_runner') -ne
            'ubuntu-24.04' -or
            (Get-Field (Get-Field $profile 'compiler') 'family') -ne 'GCC' -or
            (Get-Field (Get-Field $profile 'compiler') 'version_range') -ne
            '>=13.0.0 <14.0.0' -or
            (Get-Field (Get-Field $profile 'build') 'ci_generator') -ne 'Ninja') {
            $issues.Add('Ubuntu candidate profile differs from the frozen GCC 13 contract.')
        }
    }

    $legacyProfiles = @(Get-Field $Matrix 'evidence_only_profiles')
    $legacy = @($legacyProfiles | Where-Object {
            (Get-Field $_ 'id') -eq 'legacy-w64devkit-2.9.1-gcc-16.2'
        }) | Select-Object -First 1
    if ($legacyProfiles.Count -ne 1 -or $null -eq $legacy -or
        (Get-Field $legacy 'classification') -ne 'evidence-only' -or
        (Get-Field $legacy 'product_qualification') -ne $false -or
        (Get-Field $legacy 'provenance_item') -ne
        'w64devkit-2.9.1-legacy-reproduction') {
        $issues.Add('Legacy w64devkit profile must remain evidence-only and unqualified.')
    }
    $expectedUnqualified = @(
        'clang', 'macos', 'arm', 'mingw-product-build', 'sanitizers',
        'dynamic-package-abi', 'python-wheels')
    if (((@(Get-Field $Matrix 'not_qualified') | Sort-Object) -join ',') -ne
        (($expectedUnqualified | Sort-Object) -join ',')) {
        $issues.Add('Toolchain not-qualified boundary differs from the frozen list.')
    }

    if ((Get-Field $Presets 'version') -ne 2) {
        $issues.Add('CMake preset schema must be version 2 for the declared CMake 3.20 floor.')
    }
    $presetMinimum = Get-Field $Presets 'cmakeMinimumRequired'
    if ((Get-Field $presetMinimum 'major') -ne 3 -or
        (Get-Field $presetMinimum 'minor') -ne 20 -or
        (Get-Field $presetMinimum 'patch') -ne 0) {
        $issues.Add('CMake preset minimum must be 3.20.0.')
    }
    $configureByName = @{}
    foreach ($preset in @(Get-Field $Presets 'configurePresets')) {
        $configureByName[[string](Get-Field $preset 'name')] = $preset
    }
    foreach ($name in @('dev', 'release')) {
        if (-not $configureByName.ContainsKey($name)) {
            $issues.Add("Missing configure preset '$name'.")
        }
    }
    if ($configureByName.ContainsKey('dev')) {
        $variables = Get-Field $configureByName['dev'] 'cacheVariables'
        if ((Get-Field $variables 'CMAKE_BUILD_TYPE') -ne 'Debug' -or
            (Get-Field $variables 'GNC_BUILD_TESTS') -ne 'ON' -or
            (Get-Field $variables 'GNC_WARNINGS_AS_ERRORS') -ne 'OFF') {
            $issues.Add('Development preset semantics drifted.')
        }
    }
    if ($configureByName.ContainsKey('release')) {
        $variables = Get-Field $configureByName['release'] 'cacheVariables'
        if ((Get-Field $variables 'CMAKE_BUILD_TYPE') -ne 'Release' -or
            (Get-Field $variables 'GNC_BUILD_TESTS') -ne 'ON' -or
            (Get-Field $variables 'GNC_WARNINGS_AS_ERRORS') -ne 'ON') {
            $issues.Add('Release preset semantics drifted.')
        }
    }

    $language = Get-Field $Manifest 'language'
    if ((Get-Field $language 'standard') -ne 'C++17' -or
        (Get-Field $language 'build_system') -ne 'CMake 3.20+') {
        $issues.Add('Project manifest language/build contract differs from C++17/CMake 3.20+.')
    }
    if ($CMakeText -notmatch 'cmake_minimum_required\(VERSION 3\.20\)' -or
        $CMakeText -notmatch 'target_compile_features\([^\r\n]*cxx_std_17\)') {
        $issues.Add('CMake project does not enforce the C++17/CMake 3.20 contract.')
    }

    foreach ($requiredText in @(
            'ubuntu-24.04',
            'windows-2025-vs2026',
            'actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd',
            '-DGNC_BUILD_TESTS=ON',
            '-DGNC_WARNINGS_AS_ERRORS=ON',
            '--config Release',
            'ctest --test-dir build/ci -C Release --output-on-failure',
            './tools/verify-repository.ps1',
            'ImageOS',
            'ImageVersion',
            'CMAKE_GENERATOR',
            'CMakeCXXCompiler.cmake',
            'cmake --version',
            'ninja --version',
            'python --version',
            'pwsh --version',
            'git --version',
            'Verify Windows PowerShell 5.1 compatibility',
            'shell: powershell',
            './tools/validate-r0-specs.ps1',
            './tools/validate-architecture-baseline.ps1')) {
        if (-not $WorkflowText.Contains($requiredText)) {
            $issues.Add("CI workflow is missing required evidence/configuration text: $requiredText")
        }
    }
    if ($WorkflowText -match '(?i)(ubuntu|windows)-latest' -or
        $WorkflowText -match '(?i)preview' -or
        $WorkflowText -match 'actions/checkout@(?!de0fac2e4500dabe0009e67214ff5f5447ce83dd)') {
        $issues.Add('CI workflow uses a rolling/preview runner or a floating checkout reference.')
    }

    $adrStatusMatch = [regex]::Match($AdrText, '(?m)^- Status: (Proposed|Accepted)$')
    if (-not $adrStatusMatch.Success) {
        $issues.Add('ADR-0009 has no supported status line.')
    }
    $adrStatus = if ($adrStatusMatch.Success) { $adrStatusMatch.Groups[1].Value } else { 'Invalid' }
    if ($adrStatus -eq 'Accepted' -and -not $roleReadiness.ready) {
        $issues.Add('ADR-0009 is Accepted without complete accountable-role assignments.')
    }
    $matrixDecision = [string](Get-Field $Matrix 'decision_status')
    if (($adrStatus -eq 'Proposed' -and $matrixDecision -ne 'proposed') -or
        ($adrStatus -eq 'Accepted' -and $matrixDecision -ne 'accepted')) {
        $issues.Add('ADR and toolchain matrix decision statuses differ.')
    }

    $hostedCiStatus = [string](Get-Field $qualificationPolicy 'hosted_ci_status')
    if ($hostedCiStatus -notin @('pending-push-and-run', 'passed-with-evidence')) {
        $issues.Add("Unsupported hosted CI status '$hostedCiStatus'.")
    }
    $qualificationStatus = [string](Get-Field $Matrix 'qualification_status')
    $qualified = (
        $roleReadiness.ready -and
        $adrStatus -eq 'Accepted' -and
        $hostedCiStatus -eq 'passed-with-evidence')
    if (($qualificationStatus -eq 'supported') -ne $qualified) {
        $issues.Add('Toolchain qualification status contradicts role/ADR/hosted-CI evidence.')
    }
    if ($qualificationStatus -notin @('candidate-not-supported', 'supported')) {
        $issues.Add("Unsupported qualification status '$qualificationStatus'.")
    }

    $task = @((Get-Field $Backlog 'tasks') | Where-Object {
            (Get-Field $_ 'id') -eq 'R0-GOV-001'
        }) | Select-Object -First 1
    if ($null -eq $task) {
        $issues.Add('R0-GOV-001 is missing from the backlog.')
    }
    else {
        if ((Get-Field $task 'status') -in @('in_progress', 'review', 'done') -and
            [string]::IsNullOrWhiteSpace([string](Get-Field $task 'assignee'))) {
            $issues.Add('Active R0-GOV-001 must have an implementation assignee.')
        }
        if ((Get-Field $task 'status') -eq 'done' -and -not $qualified) {
            $issues.Add('R0-GOV-001 is done without roles, Accepted ADR and hosted-CI evidence.')
        }
    }

    return $issues.ToArray()
}

function Invoke-Mutation([string]$Name, [scriptblock]$Mutation) {
    $context = [ordered]@{
        roles = Copy-JsonObject $script:roles
        matrix = Copy-JsonObject $script:matrix
        backlog = Copy-JsonObject $script:backlog
        presets = Copy-JsonObject $script:presets
        manifest = Copy-JsonObject $script:manifest
        adr_text = [string]$script:adrText
        workflow_text = [string]$script:workflowText
        cmake_text = [string]$script:cmakeText
    }
    & $Mutation $context
    $mutationIssues = @(Test-GovernanceObjects `
            $context.roles `
            $context.matrix `
            $context.backlog `
            $context.presets `
            $context.manifest `
            $context.adr_text `
            $context.workflow_text `
            $context.cmake_text)
    $rejected = $mutationIssues.Count -gt 0
    if (-not $rejected) {
        Add-Error "Mutation was not rejected: $Name"
    }
    $script:mutationResults.Add([pscustomobject][ordered]@{
            name = $Name
            rejected = $rejected
            detected_issue_count = $mutationIssues.Count
        })
}

$requiredPaths = @(
    'docs/team/role-assignments.json',
    'docs/governance/toolchain-support-matrix.json',
    'docs/adr/0009-accountable-roles-and-candidate-toolchain.md',
    'docs/tasks/work-packages/R0-GOV-001.md',
    'docs/tasks/backlog.json',
    'docs/handoff/team-operating-model.md',
    'docs/handoff/open-decisions.md',
    'CMakeLists.txt',
    'CMakePresets.json',
    'project-manifest.json',
    '.github/workflows/ci.yml',
    'tools/validate-team-toolchain.ps1')
foreach ($relativePath in $requiredPaths) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $relativePath) -PathType Leaf)) {
        Add-Error "Required team/toolchain path is missing: $relativePath"
    }
}

$roles = Read-Json $rolesPath
$matrix = Read-Json $matrixPath
$backlog = Read-Json $backlogPath
$presets = Read-Json $presetsPath
$manifest = Read-Json $manifestPath
$adrText = if (Test-Path -LiteralPath $adrPath -PathType Leaf) {
    Get-Content -LiteralPath $adrPath -Raw -Encoding utf8
}
else { '' }
$workflowText = if (Test-Path -LiteralPath $workflowPath -PathType Leaf) {
    Get-Content -LiteralPath $workflowPath -Raw -Encoding utf8
}
else { '' }
$cmakeText = if (Test-Path -LiteralPath $cmakePath -PathType Leaf) {
    Get-Content -LiteralPath $cmakePath -Raw -Encoding utf8
}
else { '' }

$script:roles = $roles
$script:matrix = $matrix
$script:backlog = $backlog
$script:presets = $presets
$script:manifest = $manifest
$script:adrText = $adrText
$script:workflowText = $workflowText
$script:cmakeText = $cmakeText

if ($null -ne $roles -and $null -ne $matrix -and $null -ne $backlog -and
    $null -ne $presets -and $null -ne $manifest) {
    foreach ($issue in @(Test-GovernanceObjects `
            $roles $matrix $backlog $presets $manifest `
            $adrText $workflowText $cmakeText)) {
        Add-Error $issue
    }

    foreach ($profile in @(Get-Field $matrix 'profiles')) {
        foreach ($evidenceRef in @(Get-Field $profile 'evidence_refs')) {
            if (-not (Test-Path -LiteralPath (Join-Path $repoRoot ([string]$evidenceRef)) -PathType Leaf)) {
                Add-Error "Toolchain evidence reference is missing: $evidenceRef"
            }
        }
    }
    foreach ($sourceRef in @(Get-Field $matrix 'source_refs')) {
        if ([string]$sourceRef -notmatch '^https://') {
            Add-Error "Toolchain source reference is not HTTPS: $sourceRef"
        }
    }

    Invoke-Mutation 'missing-reviewer-field' {
        param($value)
        $value.roles.roles[0].PSObject.Properties.Remove('reviewer')
    }
    Invoke-Mutation 'same-assignee-and-reviewer' {
        param($value)
        $value.roles.roles[0].assignee = 'Alice Example'
        $value.roles.roles[0].reviewer = 'Alice Example'
    }
    Invoke-Mutation 'shared-science-architecture-assignee' {
        param($value)
        $scientific = @($value.roles.roles | Where-Object {
                $_.id -eq 'scientific_authority'
            }) | Select-Object -First 1
        $architecture = @($value.roles.roles | Where-Object {
                $_.id -eq 'architecture_lead'
            }) | Select-Object -First 1
        $scientific.assignee = 'Alice Example'
        $scientific.reviewer = 'Bob Example'
        $architecture.assignee = 'Alice Example'
        $architecture.reviewer = 'Carol Example'
    }
    Invoke-Mutation 'accepted-adr-without-roles' {
        param($value)
        $value.adr_text = $value.adr_text.Replace('- Status: Proposed', '- Status: Accepted')
        $value.matrix.decision_status = 'accepted'
    }
    Invoke-Mutation 'preset-schema-exceeds-declared-floor' {
        param($value)
        $value.presets.version = 6
    }
    Invoke-Mutation 'latest-runner-label' {
        param($value)
        $value.workflow_text = $value.workflow_text.Replace('ubuntu-24.04', 'ubuntu-latest')
    }
    Invoke-Mutation 'floating-checkout-action' {
        param($value)
        $value.workflow_text = $value.workflow_text.Replace(
            'actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd',
            'actions/checkout@v6')
    }
    Invoke-Mutation 'unbounded-compiler-range' {
        param($value)
        $value.matrix.profiles[1].compiler.version_range = '>=13.0.0'
    }
    Invoke-Mutation 'legacy-toolchain-promoted' {
        param($value)
        $value.matrix.evidence_only_profiles[0].classification = 'candidate-primary'
        $value.matrix.evidence_only_profiles[0].product_qualification = $true
    }
    Invoke-Mutation 'virtual-role-readiness' {
        param($value)
        $value.roles.decision_status = 'complete'
    }
    Invoke-Mutation 'distinct-codex-seat-assignments' {
        param($value)
        $index = 0
        foreach ($role in @($value.roles.roles)) {
            $role.assignee = "codex-r0-seat-$index-assignee"
            $role.reviewer = "codex-r0-seat-$index-reviewer"
            ++$index
        }
        $value.roles.decision_status = 'complete'
    }
    Invoke-Mutation 'premature-task-completion' {
        param($value)
        $task = @($value.backlog.tasks | Where-Object {
                $_.id -eq 'R0-GOV-001'
            }) | Select-Object -First 1
        $task.status = 'done'
    }
}

$roleReadiness = Get-RoleReadiness $roles
$adrStatusMatch = [regex]::Match($adrText, '(?m)^- Status: (Proposed|Accepted)$')
$adrStatus = if ($adrStatusMatch.Success) { $adrStatusMatch.Groups[1].Value } else { 'Invalid' }
$qualificationPolicy = Get-Field $matrix 'qualification_policy'
$hostedCiStatus = [string](Get-Field $qualificationPolicy 'hosted_ci_status')
$governanceReady = (
    $roleReadiness.ready -and
    $adrStatus -eq 'Accepted' -and
    $hostedCiStatus -eq 'passed-with-evidence')

$blockers = [System.Collections.Generic.List[object]]::new()
if (-not $roleReadiness.ready) {
    $blockers.Add([pscustomobject][ordered]@{
            code = 'ROLE-ASSIGNMENTS-MISSING'
            owner_role = 'product_owner'
            condition = "$($roleReadiness.missing_required_slots) required assignee/reviewer slots remain empty or invalid."
        })
}
if ($adrStatus -ne 'Accepted') {
    $blockers.Add([pscustomobject][ordered]@{
            code = 'ADR-NOT-ACCEPTED'
            owner_role = 'architecture_lead'
            condition = 'ADR-0009 remains Proposed pending Product Owner and Architecture Lead acceptance.'
        })
}
if ($hostedCiStatus -ne 'passed-with-evidence') {
    $blockers.Add([pscustomobject][ordered]@{
            code = 'HOSTED-CI-PENDING'
            owner_role = 'validation_lead'
            condition = 'The fixed-runner workflow has not been pushed and completed with retained run evidence.'
        })
}

$inputHashes = [System.Collections.Generic.List[object]]::new()
foreach ($relativePath in $requiredPaths) {
    $absolutePath = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) { continue }
    $inputHashes.Add([pscustomobject][ordered]@{
            path = $relativePath.Replace('\', '/')
            sha256 = Get-NormalizedTextSha256 $absolutePath
            hash_normalization = 'utf8-lf-no-bom'
        })
}

$candidateProfiles = @()
if ($null -ne $matrix) {
    $candidateProfiles = @((Get-Field $matrix 'profiles') | ForEach-Object {
            [pscustomobject][ordered]@{
                id = Get-Field $_ 'id'
                runner = Get-Field (Get-Field $_ 'platform') 'hosted_runner'
                compiler_family = Get-Field (Get-Field $_ 'compiler') 'family'
                compiler_range = Get-Field (Get-Field $_ 'compiler') 'version_range'
            }
        })
}

$expectedReport = [pscustomobject][ordered]@{
    schema_version = 'gnczmkn.team-toolchain-readiness/1'
    task_id = 'R0-GOV-001'
    reviewed_on = '2026-08-10'
    status = if ($governanceReady) { 'ready' } else { 'conformant-with-blockers' }
    configuration_validation = if ($errors.Count -eq 0) { 'passed' } else { 'failed' }
    governance_ready = $governanceReady
    blockers = @($blockers)
    roles = $roleReadiness
    toolchain = [ordered]@{
        adr_status = $adrStatus
        decision_status = Get-Field $matrix 'decision_status'
        qualification_status = Get-Field $matrix 'qualification_status'
        language_standard = Get-Field $matrix 'language_standard'
        cmake_declared_minimum = Get-Field (Get-Field $matrix 'cmake_contract') 'declared_minimum'
        preset_schema_version = Get-Field (Get-Field $matrix 'cmake_contract') 'preset_schema_version'
        hosted_ci_status = $hostedCiStatus
        candidate_primary_profiles = $candidateProfiles
        evidence_only_profile_count = @(Get-Field $matrix 'evidence_only_profiles').Count
        not_qualified = @(Get-Field $matrix 'not_qualified')
    }
    workflow = [ordered]@{
        runner_labels = @('ubuntu-24.04', 'windows-2025-vs2026')
        checkout_version = '6.0.2'
        checkout_commit = 'de0fac2e4500dabe0009e67214ff5f5447ce83dd'
        configuration = 'Release'
        warnings_as_errors = $true
        exact_identity_recording = $true
    }
    mutation_tests = @($mutationResults)
    input_hashes = @($inputHashes)
}

if ($errors.Count -eq 0) {
    if ($UpdateReport) {
        $json = $expectedReport | ConvertTo-Json -Depth 20
        [System.IO.File]::WriteAllText(
            $reportPath,
            $json + [Environment]::NewLine,
            [System.Text.UTF8Encoding]::new($false))
    }
    else {
        $actualReport = Read-Json $reportPath
        if ($null -ne $actualReport) {
            $actualCanonical = $actualReport | ConvertTo-Json -Depth 20 -Compress
            $expectedCanonical = $expectedReport | ConvertTo-Json -Depth 20 -Compress
            if ($actualCanonical -ne $expectedCanonical) {
                Add-Error 'Team/toolchain readiness report is stale; run with -UpdateReport.'
            }
        }
    }
}

if ($errors.Count -gt 0) {
    Write-Host "Team/toolchain validation failed with $($errors.Count) issue(s):"
    foreach ($errorMessage in $errors) {
        Write-Host " - $errorMessage"
    }
    exit 1
}

if (-not $Quiet) {
    Write-Host 'Team/toolchain configuration validation passed.'
    Write-Host "Required role slots missing: $($roleReadiness.missing_required_slots)"
    Write-Host "Candidate-primary profiles: $($candidateProfiles.Count)"
    Write-Host "Governance ready: $governanceReady; blockers: $($blockers.Count)"
    Write-Host "Mutation tests rejected: $(@($mutationResults | Where-Object { $_.rejected }).Count)/$($mutationResults.Count)"
}

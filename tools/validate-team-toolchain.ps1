[CmdletBinding()]
param(
    [switch]$UpdateReport,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$rolesPath = Join-Path $repoRoot 'docs\team\role-assignments.json'
$authorizationPath = Join-Path $repoRoot 'docs\governance\r0-owner-authorization.json'
$dispositionPath = Join-Path $repoRoot 'docs\governance\adr-dispositions\ADR-0009-2026-08-12.json'
$matrixPath = Join-Path $repoRoot 'docs\governance\toolchain-support-matrix.json'
$hostedCiEvidencePath = Join-Path $repoRoot 'docs\quality\hosted-ci-evidence-R0-GOV-001.json'
$backlogPath = Join-Path $repoRoot 'docs\tasks\backlog.json'
$presetsPath = Join-Path $repoRoot 'CMakePresets.json'
$manifestPath = Join-Path $repoRoot 'project-manifest.json'
$adrPath = Join-Path $repoRoot 'docs\adr\0009-accountable-roles-and-candidate-toolchain.md'
$workflowPath = Join-Path $repoRoot '.github\workflows\ci.yml'
$cmakePath = Join-Path $repoRoot 'CMakeLists.txt'
$reportPath = Join-Path $repoRoot 'docs\quality\team-toolchain-readiness-report.json'
$errors = [System.Collections.Generic.List[string]]::new()
$mutationResults = [System.Collections.Generic.List[object]]::new()
$reviewedFilesetCache = @{}

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

function Test-IdentityToken([object]$Value) {
    $text = [string]$Value
    if ([string]::IsNullOrWhiteSpace($text)) { return $false }
    return $text.Trim() -notmatch '^(?i:codex(?:[\s_-].*)?|unassigned|tbd|todo|unknown|n/?a|none|null|placeholder)$'
}

function Get-ActorMap([object]$Authorization) {
    $actorById = @{}
    if ($null -eq $Authorization) { return $actorById }
    foreach ($actor in @(Get-Field $Authorization 'actors')) {
        $id = [string](Get-Field $actor 'id')
        if (-not [string]::IsNullOrWhiteSpace($id) -and -not $actorById.ContainsKey($id)) {
            $actorById[$id] = $actor
        }
    }
    return $actorById
}

function Test-AuthorizedActor(
    [object]$Value,
    [hashtable]$ActorById,
    [string]$RequiredRole = '') {
    $id = [string]$Value
    if (-not (Test-IdentityToken $id) -or -not $ActorById.ContainsKey($id)) {
        return $false
    }
    $actor = $ActorById[$id]
    if ((Get-Field $actor 'kind') -ne 'machine_agent' -or
        (Get-Field $actor 'identity_disclosure') -ne 'machine-agent' -or
        [string]::IsNullOrWhiteSpace([string](Get-Field $actor 'task_path'))) {
        return $false
    }
    if (-not [string]::IsNullOrWhiteSpace($RequiredRole) -and
        $RequiredRole -notin @(Get-Field $actor 'authorized_roles')) {
        return $false
    }
    return $true
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

function Get-Sha256Hex([byte[]]$Bytes) {
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString(
                $algorithm.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Get-Sha1Hex([byte[]]$Bytes) {
    $algorithm = [System.Security.Cryptography.SHA1]::Create()
    try {
        return ([System.BitConverter]::ToString(
                $algorithm.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Get-RetainedGitCommitFact([object]$Object) {
    if ((Get-Field $Object 'object_type') -ne 'commit' -or
        (Get-Field $Object 'content_encoding') -ne 'base64') {
        throw 'Retained Git object type or encoding is invalid.'
    }
    try {
        [byte[]]$content = [System.Convert]::FromBase64String(
            [string](Get-Field $Object 'content_base64'))
    }
    catch {
        throw "Retained Git commit base64 is invalid: $($_.Exception.Message)"
    }
    if ($content.Length -ne (Get-Field $Object 'byte_length')) {
        throw 'Retained Git commit byte length is incorrect.'
    }
    [byte[]]$header = [System.Text.Encoding]::ASCII.GetBytes(
        "commit $($content.Length)`0")
    [byte[]]$fullObject = New-Object byte[] ($header.Length + $content.Length)
    [System.Array]::Copy($header, 0, $fullObject, 0, $header.Length)
    [System.Array]::Copy($content, 0, $fullObject, $header.Length, $content.Length)
    $sha1 = Get-Sha1Hex $fullObject
    if ($sha1 -ne (Get-Field $Object 'sha1')) {
        throw 'Retained Git commit SHA-1 is incorrect.'
    }
    $text = [System.Text.UTF8Encoding]::new($false, $true).GetString($content)
    $treeMatch = [regex]::Match($text, '(?m)^tree ([0-9a-f]{40})$')
    $parentMatches = [regex]::Matches($text, '(?m)^parent ([0-9a-f]{40})$')
    if (-not $treeMatch.Success -or $parentMatches.Count -eq 0) {
        throw 'Retained Git commit has no readable tree or parent headers.'
    }
    return [pscustomobject][ordered]@{
        sha1 = $sha1
        byte_length = $content.Length
        tree = $treeMatch.Groups[1].Value
        parents = @($parentMatches | ForEach-Object { $_.Groups[1].Value })
    }
}

function Get-GitBlobFact([string]$Commit, [string]$Path) {
    $objectId = [string](& git -C $repoRoot rev-parse "$Commit`:$Path" 2>$null)
    if ($LASTEXITCODE -ne 0 -or $objectId.Trim() -notmatch '^[0-9a-f]{40}$') {
        throw "Cannot resolve Git blob '$Commit`:$Path'."
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = 'git'
    $startInfo.Arguments = "cat-file blob $($objectId.Trim())"
    $startInfo.WorkingDirectory = $repoRoot
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $memory = [System.IO.MemoryStream]::new()
    try {
        $process.StartInfo = $startInfo
        [void]$process.Start()
        $process.StandardOutput.BaseStream.CopyTo($memory)
        $standardError = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "Cannot read Git blob '$Commit`:$Path': $standardError"
        }
        $bytes = $memory.ToArray()
        return [pscustomobject][ordered]@{
            path = $Path
            byte_length = $bytes.Length
            sha256 = Get-Sha256Hex $bytes
        }
    }
    finally {
        $memory.Dispose()
        $process.Dispose()
    }
}

function Get-ReviewedFileset([string]$Commit, [string]$Parent) {
    $cacheKey = "$Commit|$Parent"
    if ($script:reviewedFilesetCache.ContainsKey($cacheKey)) {
        return $script:reviewedFilesetCache[$cacheKey]
    }
    if ($Commit -notmatch '^[0-9a-f]{40}$' -or $Parent -notmatch '^[0-9a-f]{40}$') {
        throw 'Reviewed commit and parent must be lowercase full SHA-1 values.'
    }
    $actualParent = [string](& git -C $repoRoot rev-parse "$Commit^" 2>$null)
    if ($LASTEXITCODE -ne 0 -or $actualParent.Trim() -ne $Parent) {
        throw "Reviewed commit '$Commit' does not have recorded parent '$Parent'."
    }
    [string[]]$paths = @(& git -C $repoRoot diff-tree --no-commit-id --name-only -r $Commit)
    if ($LASTEXITCODE -ne 0 -or $paths.Count -eq 0) {
        throw "Reviewed commit '$Commit' has no readable changed-path set."
    }
    [System.Array]::Sort($paths, [System.StringComparer]::Ordinal)
    $entries = [System.Collections.Generic.List[object]]::new()
    $manifestLines = [System.Collections.Generic.List[string]]::new()
    foreach ($path in $paths) {
        $entry = Get-GitBlobFact $Commit $path
        $entries.Add($entry)
        $manifestLines.Add(
            "$($entry.path)`t$($entry.byte_length)`t$($entry.sha256)")
    }
    $manifest = [string]::Join("`n", $manifestLines) + "`n"
    $manifestBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($manifest)
    $result = [pscustomobject][ordered]@{
        algorithm_id = 'gnczmkn.fileset-manifest/1'
        path_order = 'ordinal-ascending'
        entry_format = 'path<TAB>raw_git_blob_byte_length<TAB>lowercase_sha256(raw_git_blob_bytes)'
        manifest_encoding = 'UTF-8 without BOM'
        line_ending = 'LF with a final LF'
        path_count = $entries.Count
        sha256 = Get-Sha256Hex $manifestBytes
        entries = @($entries)
    }
    $script:reviewedFilesetCache[$cacheKey] = $result
    return $result
}

function Copy-JsonObject([object]$Object) {
    return $Object | ConvertTo-Json -Depth 100 | ConvertFrom-Json
}

function Get-RoleReadiness([object]$Roles, [object]$Authorization) {
    $actorById = Get-ActorMap $Authorization
    $required = @()
    if ($null -ne $Roles) {
        $required = @((Get-Field $Roles 'roles') | Where-Object {
                (Get-Field $_ 'required') -eq $true
            })
    }
    $assigned = @($required | Where-Object {
            Test-AuthorizedActor (Get-Field $_ 'assignee') $actorById ([string](Get-Field $_ 'id'))
        }).Count
    $reviewed = @($required | Where-Object {
            Test-AuthorizedActor (Get-Field $_ 'reviewer') $actorById
        }).Count
    $validPairs = @($required | Where-Object {
            $assignee = [string](Get-Field $_ 'assignee')
            $reviewer = [string](Get-Field $_ 'reviewer')
            (Test-AuthorizedActor $assignee $actorById ([string](Get-Field $_ 'id'))) -and
            (Test-AuthorizedActor $reviewer $actorById) -and
            $assignee -ne $reviewer -and
            (Get-Field $actorById[$assignee] 'task_path') -ne
            (Get-Field $actorById[$reviewer] 'task_path')
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
            (Test-AuthorizedActor $scientificAssignee $actorById 'scientific_authority') -and
            (Test-AuthorizedActor $architectureAssignee $actorById 'architecture_lead') -and
            $scientificAssignee -ne $architectureAssignee -and
            (Get-Field $actorById[$scientificAssignee] 'task_path') -ne
            (Get-Field $actorById[$architectureAssignee] 'task_path')
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

function Test-HostedCiEvidence([object]$Evidence, [object]$Authorization) {
    $issues = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $Evidence) {
        $issues.Add('Hosted CI qualification evidence is missing.')
        return $issues.ToArray()
    }

    if ((Get-Field $Evidence 'schema_version') -ne 'gnczmkn.hosted-ci-evidence/1' -or
        (Get-Field $Evidence 'evidence_id') -ne 'R0-GOV-001-HOSTED-CI-32F6ADE7' -or
        (Get-Field $Evidence 'task_id') -ne 'R0-GOV-001' -or
        (Get-Field $Evidence 'evidence_status') -ne 'accepted' -or
        (Get-Field $Evidence 'recorded_on') -ne '2026-08-12' -or
        (Get-Field $Evidence 'adr_ref') -ne
        'docs/adr/0009-accountable-roles-and-candidate-toolchain.md' -or
        (Get-Field $Evidence 'task_ref') -ne
        'docs/tasks/work-packages/R0-GOV-001.md' -or
        (Get-Field $Evidence 'matrix_ref') -ne
        'docs/governance/toolchain-support-matrix.json' -or
        (Get-Field $Evidence 'migration_ref') -ne
        'docs/quality/r0-first-wave-reconciliation-audit.md#14-decision-ledger') {
        $issues.Add('Hosted CI evidence identity or status is invalid.')
    }

    $actorById = Get-ActorMap $Authorization
    $decision = Get-Field $Evidence 'decision'
    if ((Get-Field $decision 'id') -ne 'RECON-DEC-008' -or
        (Get-Field $decision 'outcome') -ne 'keep-current' -or
        (Get-Field $decision 'status') -ne 'accepted' -or
        (Get-Field $decision 'decided_on') -ne '2026-08-12' -or
        (Get-Field $decision 'reviewed_commit') -ne
        '32f6ade7f2feec6fb1792121773d437f6f035581') {
        $issues.Add('RECON-DEC-008 is missing an accepted commit-bound disposition.')
    }
    $expectedDecisionActors = [ordered]@{
        'r0-po-agent' = [ordered]@{
            role = 'product_owner'; task = '/root'; result = 'keep-current'
        }
        'r0-validation-agent' = [ordered]@{
            role = 'validation_lead'; task = '/root/r0_validation_agent'; result = 'approved'
        }
    }
    $seenDecisionActors = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $seenDecisionTasks = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($actor in @(Get-Field $decision 'actors')) {
        $actorId = [string](Get-Field $actor 'actor_id')
        $taskPath = [string](Get-Field $actor 'task_path')
        [void]$seenDecisionActors.Add($actorId)
        [void]$seenDecisionTasks.Add($taskPath)
        if (-not $expectedDecisionActors.Contains($actorId)) {
            $issues.Add("RECON-DEC-008 contains unauthorized actor '$actorId'.")
            continue
        }
        $expected = $expectedDecisionActors[$actorId]
        if (-not (Test-AuthorizedActor $actorId $actorById $expected.role) -or
            (Get-Field $actor 'kind') -ne 'machine_agent' -or
            (Get-Field $actor 'role') -ne $expected.role -or
            $taskPath -ne $expected.task -or
            (Get-Field $actor 'result') -ne $expected.result -or
            (Get-Field $actorById[$actorId] 'task_path') -ne $taskPath) {
            $issues.Add("RECON-DEC-008 actor '$actorId' has an invalid role, task, kind or result.")
        }
    }
    if ($seenDecisionActors.Count -ne 2 -or $seenDecisionTasks.Count -ne 2) {
        $issues.Add('RECON-DEC-008 requires distinct Product Owner and Validation actor/task identities.')
    }
    if (@(Get-Field $decision 'rationale').Count -lt 3) {
        $issues.Add('RECON-DEC-008 rationale is incomplete.')
    }

    $repository = Get-Field $Evidence 'repository'
    if ((Get-Field $repository 'provider') -ne 'github-actions' -or
        [string](Get-Field $repository 'repository_id') -ne '1328549597' -or
        (Get-Field $repository 'node_id') -ne 'R_kgDOTzAO3Q' -or
        (Get-Field $repository 'slug') -ne 'aofenghanyue/GNCSIMZMKN' -or
        (Get-Field $repository 'visibility') -ne 'public' -or
        (Get-Field $repository 'api_url') -ne
        'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN' -or
        (Get-Field $repository 'html_url') -ne
        'https://github.com/aofenghanyue/GNCSIMZMKN') {
        $issues.Add('Hosted CI evidence repository identity is invalid.')
    }

    $qualification = Get-Field $Evidence 'qualification'
    $qualifiedCommit = [string](Get-Field $qualification 'commit')
    $qualifiedParent = [string](Get-Field $qualification 'parent')
    if ((Get-Field $qualification 'status') -ne 'passed-with-evidence' -or
        $qualifiedCommit -ne '32f6ade7f2feec6fb1792121773d437f6f035581' -or
        $qualifiedParent -ne 'cd093d5e91ba5225cc4201506450a7993af32446' -or
        (Get-Field $qualification 'tree') -ne
        '6efb2a55e6ae63bb2e3f50c0a164e55ba57f2815' -or
        (Get-Field $qualification 'branch') -ne 'codex/r0-gov-001') {
        $issues.Add('Hosted CI qualification commit, parent, branch or status is invalid.')
    }
    $actualParent = [string](& git -C $repoRoot rev-parse "$qualifiedCommit^" 2>$null)
    if ($LASTEXITCODE -ne 0 -or $actualParent.Trim() -ne $qualifiedParent) {
        $issues.Add('Hosted CI qualification commit and parent cannot be reproduced from Git history.')
    }
    $actualTree = [string](& git -C $repoRoot rev-parse "${qualifiedCommit}^{tree}" 2>$null)
    if ($LASTEXITCODE -ne 0 -or
        $actualTree.Trim() -ne (Get-Field $qualification 'tree')) {
        $issues.Add('Hosted CI qualification tree cannot be reproduced from Git history.')
    }

    $workflow = Get-Field $qualification 'workflow'
    if ([string](Get-Field $workflow 'workflow_id') -ne '330370398' -or
        (Get-Field $workflow 'name') -ne 'bootstrap-ci' -or
        (Get-Field $workflow 'path') -ne '.github/workflows/ci.yml' -or
        (Get-Field $workflow 'api_url') -ne
        'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/workflows/330370398' -or
        (Get-Field $workflow 'git_blob_oid') -ne 'c9ad4b796ddf321fe2858f9a2ca148defd7f7a9b' -or
        (Get-Field $workflow 'raw_sha256') -ne
        '5338f9b37404a19a651e67766452ac125886aead1b711d8dcc93c1574008803c' -or
        (Get-Field $workflow 'checkout_commit') -ne
        'de0fac2e4500dabe0009e67214ff5f5447ce83dd' -or
        (Get-Field $workflow 'fetch_depth') -ne 0 -or
        (Get-Field $workflow 'configuration') -ne 'Release' -or
        (Get-Field $workflow 'warnings_as_errors') -ne $true) {
        $issues.Add('Hosted CI evidence does not bind the qualified workflow configuration.')
    }
    try {
        $workflowFact = Get-GitBlobFact $qualifiedCommit '.github/workflows/ci.yml'
        if ($workflowFact.sha256 -ne (Get-Field $workflow 'raw_sha256') -or
            ([string](& git -C $repoRoot rev-parse "$qualifiedCommit`:.github/workflows/ci.yml")).Trim() -ne
            (Get-Field $workflow 'git_blob_oid')) {
            $issues.Add('Hosted CI workflow receipt differs from the qualified Git blob.')
        }
    }
    catch {
        $issues.Add("Hosted CI workflow Git blob cannot be reproduced: $($_.Exception.Message)")
    }

    $runs = Get-Field $Evidence 'runs'
    $expectedRuns = [ordered]@{
        primary = [ordered]@{
            id = '31559701566'; event = 'push'; url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559701566'
            api_url = 'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/runs/31559701566'
            jobs_url = 'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/runs/31559701566/jobs'
            artifacts_url = 'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/runs/31559701566/artifacts'
            source_head_sha = '32f6ade7f2feec6fb1792121773d437f6f035581'
            checked_out_ref = 'refs/heads/codex/r0-gov-001'
            checked_out_sha = '32f6ade7f2feec6fb1792121773d437f6f035581'
            created_at = '2026-08-12T03:19:07Z'; started_at = '2026-08-12T03:19:07Z'; completed_at = '2026-08-12T03:20:46Z'
            log_url = 'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/runs/31559701566/logs'
            log_bytes = 31455; log_sha256 = 'bea51b51d806f2ea7089f4964362dab5e13c15f660e449dd0ec25737fee7639b'
        }
        repeat = [ordered]@{
            id = '31559704268'; event = 'pull_request'; url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268'
            api_url = 'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268'
            jobs_url = 'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268/jobs'
            artifacts_url = 'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268/artifacts'
            source_head_sha = '32f6ade7f2feec6fb1792121773d437f6f035581'
            checked_out_ref = 'refs/pull/2/merge'
            checked_out_sha = '4ecb1cafc8fab2dc385d1fb5493aa823b48b08bd'
            created_at = '2026-08-12T03:19:10Z'; started_at = '2026-08-12T03:19:10Z'; completed_at = '2026-08-12T03:20:29Z'
            log_url = 'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268/logs'
            log_bytes = 33033; log_sha256 = '6578a756edf0a72124f8c2d70aaed533fbbbea4482b8122b74cb817703bdac8b'
        }
    }
    $linuxSteps = [ordered]@{
        'Check out repository' = 'success'
        'Record runner and tool identity' = 'success'
        'Configure Ubuntu candidate' = 'success'
        'Configure Windows candidate' = 'skipped'
        'Record configured compiler identity' = 'success'
        'Build' = 'success'
        'Test' = 'success'
        'Repository checks' = 'success'
        'Verify Windows PowerShell 5.1 compatibility' = 'skipped'
    }
    $windowsSteps = [ordered]@{
        'Check out repository' = 'success'
        'Record runner and tool identity' = 'success'
        'Configure Ubuntu candidate' = 'skipped'
        'Configure Windows candidate' = 'success'
        'Record configured compiler identity' = 'success'
        'Build' = 'success'
        'Test' = 'success'
        'Repository checks' = 'success'
        'Verify Windows PowerShell 5.1 compatibility' = 'success'
    }
    $expectedJobSets = [ordered]@{
        primary = [ordered]@{
            '93999328011' = [ordered]@{
                name = 'ubuntu-24.04-gcc-13'; runner_label = 'ubuntu-24.04'; runner_group = 'GitHub Actions'
                runner_name = 'GitHub Actions 1000000081'; started_at = '2026-08-12T03:19:11Z'; completed_at = '2026-08-12T03:20:08Z'
                url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559701566/job/93999328011'; steps = $linuxSteps
            }
            '93999328054' = [ordered]@{
                name = 'windows-2025-vs2026-msvc-19.5x'; runner_label = 'windows-2025-vs2026'; runner_group = 'GitHub Actions'
                runner_name = 'GitHub Actions 1000000082'; started_at = '2026-08-12T03:19:11Z'; completed_at = '2026-08-12T03:20:45Z'
                url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559701566/job/93999328054'; steps = $windowsSteps
            }
        }
        repeat = [ordered]@{
            '93999335576' = [ordered]@{
                name = 'ubuntu-24.04-gcc-13'; runner_label = 'ubuntu-24.04'; runner_group = 'GitHub Actions'
                runner_name = 'GitHub Actions 1000000083'; started_at = '2026-08-12T03:19:14Z'; completed_at = '2026-08-12T03:20:11Z'
                url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268/job/93999335576'; steps = $linuxSteps
            }
            '93999335654' = [ordered]@{
                name = 'windows-2025-vs2026-msvc-19.5x'; runner_label = 'windows-2025-vs2026'; runner_group = 'GitHub Actions'
                runner_name = 'GitHub Actions 1000000084'; started_at = '2026-08-12T03:19:14Z'; completed_at = '2026-08-12T03:20:29Z'
                url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268/job/93999335654'; steps = $windowsSteps
            }
        }
    }
    foreach ($name in @('primary', 'repeat')) {
        $run = Get-Field $runs $name
        $expected = $expectedRuns[$name]
        if ([string](Get-Field $run 'run_id') -ne $expected.id -or
            (Get-Field $run 'run_attempt') -ne 1 -or
            (Get-Field $run 'event') -ne $expected.event -or
            (Get-Field $run 'api_url') -ne $expected.api_url -or
            [string](Get-Field $run 'workflow_id') -ne '330370398' -or
            (Get-Field $run 'jobs_api_url') -ne $expected.jobs_url -or
            (Get-Field $run 'artifacts_api_url') -ne $expected.artifacts_url -or
            (Get-Field $run 'source_head_sha') -ne $expected.source_head_sha -or
            (Get-Field $run 'source_branch') -ne 'codex/r0-gov-001' -or
            (Get-Field $run 'checked_out_ref') -ne $expected.checked_out_ref -or
            (Get-Field $run 'checked_out_sha') -ne $expected.checked_out_sha -or
            (Get-Field $run 'checked_out_tree_sha') -ne
            '6efb2a55e6ae63bb2e3f50c0a164e55ba57f2815' -or
            (Get-Field $run 'conclusion') -ne 'success' -or
            (Get-Field $run 'created_at') -ne $expected.created_at -or
            (Get-Field $run 'started_at') -ne $expected.started_at -or
            (Get-Field $run 'completed_at') -ne $expected.completed_at -or
            (Get-Field $run 'url') -ne $expected.url -or
            (Get-Field $run 'artifact_count') -ne 0 -or
            (Get-Field (Get-Field $run 'log_archive') 'api_url') -ne $expected.log_url -or
            (Get-Field (Get-Field $run 'log_archive') 'archive_format') -ne 'zip' -or
            (Get-Field (Get-Field $run 'log_archive') 'hash_algorithm') -ne 'SHA-256' -or
            (Get-Field (Get-Field $run 'log_archive') 'byte_length') -ne $expected.log_bytes -or
            (Get-Field (Get-Field $run 'log_archive') 'sha256') -ne $expected.log_sha256 -or
            (Get-Field (Get-Field $run 'log_archive') 'repeated_download_match') -ne $true) {
            $issues.Add("Hosted CI $name run receipt is incomplete or inconsistent.")
        }
        $expectedParents = if ($name -eq 'primary') {
            @('cd093d5e91ba5225cc4201506450a7993af32446')
        }
        else {
            @(
                '291cb28b064642f3e7aa14303ee30b03c8d047f0',
                '32f6ade7f2feec6fb1792121773d437f6f035581')
        }
        if ((@(Get-Field $run 'checked_out_parent_shas') -join ',') -ne
            ($expectedParents -join ',')) {
            $issues.Add("Hosted CI $name run checkout-parent receipt is incorrect.")
        }
        if ($name -eq 'repeat') {
            try {
                $retainedCommit = Get-RetainedGitCommitFact(
                    Get-Field $run 'checked_out_commit_object')
                if ($retainedCommit.sha1 -ne (Get-Field $run 'checked_out_sha') -or
                    $retainedCommit.tree -ne (Get-Field $run 'checked_out_tree_sha') -or
                    (($retainedCommit.parents -join ',') -ne ($expectedParents -join ','))) {
                    $issues.Add('Hosted CI PR checkout differs from its retained raw Git commit object.')
                }
            }
            catch {
                $issues.Add("Hosted CI PR Git commit object cannot be reproduced: $($_.Exception.Message)")
            }
        }
        $jobs = @(Get-Field $run 'jobs')
        $expectedJobs = $expectedJobSets[$name]
        $jobById = @{}
        foreach ($job in $jobs) {
            $jobById[[string](Get-Field $job 'job_id')] = $job
        }
        if ($jobs.Count -ne 2 -or
            (($jobById.Keys | Sort-Object) -join ',') -ne
            (($expectedJobs.Keys | Sort-Object) -join ',')) {
            $issues.Add("Hosted CI $name run does not retain the exact two profile jobs.")
        }
        foreach ($jobId in $expectedJobs.Keys) {
            if (-not $jobById.ContainsKey($jobId)) { continue }
            $job = $jobById[$jobId]
            $expectedJob = $expectedJobs[$jobId]
            foreach ($field in @(
                    'name', 'runner_label', 'runner_group', 'runner_name',
                    'started_at', 'completed_at', 'url')) {
                if ((Get-Field $job $field) -ne $expectedJob[$field]) {
                    $issues.Add("Hosted CI $name job '$jobId' field '$field' is incorrect.")
                }
            }
            if ((Get-Field $job 'conclusion') -ne 'success') {
                $issues.Add("Hosted CI $name job '$jobId' did not conclude successfully.")
            }
            $steps = Get-Field $job 'required_step_conclusions'
            $stepCount = if ($null -eq $steps) { 0 } else { @($steps.PSObject.Properties).Count }
            if ($stepCount -ne $expectedJob.steps.Count) {
                $issues.Add("Hosted CI $name job '$jobId' has an incorrect required-step set.")
            }
            foreach ($stepName in $expectedJob.steps.Keys) {
                if ((Get-Field $steps $stepName) -ne $expectedJob.steps[$stepName]) {
                    $issues.Add("Hosted CI $name job '$jobId' step '$stepName' has an incorrect conclusion.")
                }
            }
        }
    }

    $expectedPrimaryProfiles = [ordered]@{
        'ubuntu-24.04-x64-gcc-13' = [ordered]@{
            job_id = '93999328011'; name = 'ubuntu-24.04-gcc-13'; runner_os = 'Linux'; runner_arch = 'X64'
            runner_name = 'GitHub Actions 1000000081'
            job_url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559701566/job/93999328011'
            image_os = 'ubuntu24'; image_version = '20260720.247.2'; generator = 'Ninja'
            compiler_id = 'GNU'; compiler_version = '13.3.0'; cmake = '3.31.6'; python = '3.12.3'
            powershell = '7.6.3'; ninja = '1.13.2'; git = '2.54.0'
            configuration = 'Release'; warnings_as_errors = $true
        }
        'windows-x64-msvc-19.5x' = [ordered]@{
            job_id = '93999328054'; name = 'windows-2025-vs2026-msvc-19.5x'; runner_os = 'Windows'; runner_arch = 'X64'
            runner_name = 'GitHub Actions 1000000082'
            job_url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559701566/job/93999328054'
            image_os = 'win25-vs2026'; image_version = '20260803.193.1'; generator = 'Visual Studio 18 2026'
            compiler_id = 'MSVC'; compiler_version = '19.51.36252.0'; cmake = '4.4.2'; python = '3.12.10'
            powershell = '7.6.4'; ninja = $null; git = '2.55.0.windows.3'
            configuration = 'Release'; warnings_as_errors = $true
        }
    }
    $expectedRepeatProfiles = [ordered]@{
        'ubuntu-24.04-x64-gcc-13' = [ordered]@{
            job_id = '93999335576'; name = 'ubuntu-24.04-gcc-13'; runner_os = 'Linux'; runner_arch = 'X64'
            runner_name = 'GitHub Actions 1000000083'
            job_url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268/job/93999335576'
            image_os = 'ubuntu24'; image_version = '20260720.247.2'; generator = 'Ninja'
            compiler_id = 'GNU'; compiler_version = '13.3.0'; cmake = '3.31.6'; python = '3.12.3'
            powershell = '7.6.3'; ninja = '1.13.2'; git = '2.54.0'
            configuration = 'Release'; warnings_as_errors = $true
        }
        'windows-x64-msvc-19.5x' = [ordered]@{
            job_id = '93999335654'; name = 'windows-2025-vs2026-msvc-19.5x'; runner_os = 'Windows'; runner_arch = 'X64'
            runner_name = 'GitHub Actions 1000000084'
            job_url = 'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268/job/93999335654'
            image_os = 'win25-vs2026'; image_version = '20260803.193.1'; generator = 'Visual Studio 18 2026'
            compiler_id = 'MSVC'; compiler_version = '19.51.36252.0'; cmake = '4.4.2'; python = '3.12.10'
            powershell = '7.6.4'; ninja = $null; git = '2.55.0.windows.3'
            configuration = 'Release'; warnings_as_errors = $true
        }
    }
    $profileSets = [ordered]@{
        profiles = $expectedPrimaryProfiles
        repeat_profiles = $expectedRepeatProfiles
    }
    foreach ($profileSetName in $profileSets.Keys) {
        $expectedProfiles = $profileSets[$profileSetName]
        $profileById = @{}
        foreach ($profile in @(Get-Field $Evidence $profileSetName)) {
            $profileById[[string](Get-Field $profile 'profile_id')] = $profile
        }
        if ((($profileById.Keys | Sort-Object) -join ',') -ne
            (($expectedProfiles.Keys | Sort-Object) -join ',')) {
            $issues.Add("Hosted CI evidence '$profileSetName' set is incomplete.")
        }
        foreach ($profileId in $expectedProfiles.Keys) {
            if (-not $profileById.ContainsKey($profileId)) { continue }
            $profile = $profileById[$profileId]
            $expected = $expectedProfiles[$profileId]
            foreach ($field in $expected.Keys) {
                if ((Get-Field $profile $field) -ne $expected[$field]) {
                    $issues.Add("Hosted CI '$profileSetName' profile '$profileId' field '$field' differs from the retained run identity.")
                }
            }
            $verification = Get-Field $profile 'verification'
            if ((Get-Field $verification 'configure') -ne 'passed' -or
                (Get-Field $verification 'build') -ne 'passed' -or
                (Get-Field $verification 'ctest') -ne '9/9 passed' -or
                (Get-Field $verification 'repository_verifier') -ne 'passed: 57 JSON, 65 tasks, 100 Markdown') {
                $issues.Add("Hosted CI '$profileSetName' profile '$profileId' lacks retained verification results.")
            }
            if ($profileId -eq 'windows-x64-msvc-19.5x') {
                $windowsCompatibility = Get-Field $profile 'windows_powershell_compatibility'
                if ((Get-Field $windowsCompatibility 'shell') -ne
                    'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.EXE' -or
                    (Get-Field $windowsCompatibility 'schema_validator') -ne 'passed' -or
                    (Get-Field $windowsCompatibility 'architecture_validator') -ne 'passed') {
                    $issues.Add("Hosted CI '$profileSetName' Windows profile lacks PowerShell 5.1 verification.")
                }
            }
        }
    }

    $retention = Get-Field $Evidence 'retention'
    if ((Get-Field $retention 'repository_receipt') -ne 'permanent-in-git-history' -or
        (Get-Field $retention 'upstream_logs') -ne 'github-managed-repository-retention' -or
        (Get-Field $retention 'upstream_log_retention_days') -ne 90 -or
        (Get-Field $retention 'maximum_allowed_days') -ne 90 -or
        (Get-Field $retention 'observed_at') -ne '2026-08-12T03:37:37Z' -or
        (Get-Field $retention 'settings_api_url') -ne
        'https://api.github.com/repos/aofenghanyue/GNCSIMZMKN/actions/permissions/artifact-and-log-retention' -or
        (Get-Field $retention 'binary_artifacts') -ne 'not-required-for-governance-only-qualification' -or
        (Get-Field $retention 'artifact_count') -ne 0 -or
        [string]::IsNullOrWhiteSpace([string](Get-Field $retention 'zero_artifact_rationale')) -or
        (Get-Field $retention 'required_facts_permanently_retained') -ne $true -or
        (Get-Field $retention 'append_only') -ne $true -or
        [string]::IsNullOrWhiteSpace([string](Get-Field $retention 'supersession_policy')) -or
        @(Get-Field $retention 'refresh_required_on').Count -lt 4) {
        $issues.Add('Hosted CI evidence retention and refresh policy is incomplete.')
    }
    $expectedBoundaries = @(
        'no-product-runtime-qualification',
        'no-rights-or-external-distribution-decision',
        'no-r0-task-or-gate-acceptance',
        'no-r1-through-r8-unlock')
    if (((@(Get-Field (Get-Field $Evidence 'boundaries') 'does_not_establish') | Sort-Object) -join ',') -ne
        (($expectedBoundaries | Sort-Object) -join ',')) {
        $issues.Add('Hosted CI evidence drops a required fail-closed boundary.')
    }

    return $issues.ToArray()
}

function Test-GovernanceObjects(
    [object]$Roles,
    [object]$Authorization,
    [object]$Disposition,
    [object]$Matrix,
    [object]$HostedCiEvidence,
    [object]$Backlog,
    [object]$Presets,
    [object]$Manifest,
    [string]$AdrText,
    [string]$WorkflowText,
    [string]$CMakeText) {
    $issues = [System.Collections.Generic.List[string]]::new()
    foreach ($issue in @(Test-HostedCiEvidence $HostedCiEvidence $Authorization)) {
        $issues.Add($issue)
    }

    if ((Get-Field $Roles 'schema_version') -ne 'gnczmkn.team-roles/3') {
        $issues.Add('Unsupported team-role schema.')
    }
    if ((Get-Field $Roles 'authorization_ref') -ne
        'docs/governance/r0-owner-authorization.json') {
        $issues.Add('Team roles do not reference the R0 owner authorization record.')
    }
    $assignmentPolicy = Get-Field $Roles 'assignment_policy'
    if ((Get-Field $assignmentPolicy 'reviewer_must_differ_from_assignee') -ne $true -or
        (Get-Field $assignmentPolicy 'reviewer_task_must_differ_from_assignee_task') -ne $true -or
        (Get-Field $assignmentPolicy 'identities_resolve_through_authorization') -ne $true -or
        (Get-Field $assignmentPolicy 'machine_identity_must_be_explicit') -ne $true -or
        (Get-Field $assignmentPolicy 'placeholder_assignments_forbidden') -ne $true) {
        $issues.Add('Team-role assignment policy does not enforce authorized, explicit and independent identities.')
    }
    $highRiskPolicy = Get-Field $assignmentPolicy 'high_risk_independence'
    if ((@(Get-Field $highRiskPolicy 'roles') -join ',') -ne
        'scientific_authority,architecture_lead' -or
        (Get-Field $highRiskPolicy 'assignees_must_differ') -ne $true -or
        (Get-Field $highRiskPolicy 'assignee_tasks_must_differ') -ne $true) {
        $issues.Add('Scientific/architecture high-risk independence policy is incomplete.')
    }

    if ((Get-Field $Authorization 'schema_version') -ne
        'gnczmkn.r0-owner-authorization/1' -or
        (Get-Field $Authorization 'authorization_id') -ne
        'R0-OWNER-AUTH-2026-08-12' -or
        (Get-Field $Authorization 'authorization_status') -ne 'active' -or
        (Get-Field $Authorization 'authorized_on') -ne '2026-08-12') {
        $issues.Add('R0 owner authorization identity or status is invalid.')
    }
    $sourceInstruction = Get-Field $Authorization 'source_instruction'
    if ((Get-Field $sourceInstruction 'digest_algorithm') -ne 'SHA-256' -or
        (Get-Field $sourceInstruction 'sha256') -ne
        '6b220b6425cd90fab5b8bdae262d6c83c8e299e29d47d328d96262dfe69f918b') {
        $issues.Add('R0 owner authorization source instruction digest is invalid.')
    }
    $authorizedRepository = Get-Field $Authorization 'repository'
    if ((Get-Field $authorizedRepository 'slug') -ne 'aofenghanyue/GNCSIMZMKN' -or
        (Get-Field $authorizedRepository 'trusted_baseline_commit') -ne
        '291cb28b064642f3e7aa14303ee30b03c8d047f0' -or
        (Get-Field $Authorization 'shared_thread_id') -ne
        '019ff3be-4a80-7210-a14e-dac71ac15f9f') {
        $issues.Add('R0 owner authorization repository, baseline or shared thread binding is invalid.')
    }
    $authorizedScope = Get-Field $Authorization 'scope'
    if ((Get-Field $authorizedScope 'stage') -ne 'R0' -or
        @(Get-Field $authorizedScope 'authorized_actions').Count -lt 6 -or
        [string](Get-Field $authorizedScope 'stage_boundary') -notmatch 'R1 through R8') {
        $issues.Add('R0 owner authorization scope or stage boundary is incomplete.')
    }
    $identityPolicy = Get-Field $Authorization 'identity_policy'
    if ((Get-Field $identityPolicy 'machine_identity_must_be_explicit') -ne $true -or
        (Get-Field $identityPolicy 'human_impersonation_forbidden') -ne $true -or
        (Get-Field $identityPolicy 'unregistered_actor_aliases_have_no_authority') -ne $true) {
        $issues.Add('R0 owner authorization identity policy is incomplete.')
    }
    $independencePolicy = Get-Field $Authorization 'independence_policy'
    if ((Get-Field $independencePolicy 'minimum_distinct_machine_actors') -ne 4 -or
        (Get-Field $independencePolicy 'actor_ids_must_be_unique') -ne $true -or
        (Get-Field $independencePolicy 'task_paths_must_be_unique') -ne $true -or
        (Get-Field $independencePolicy 'implementer_and_final_reviewer_must_differ') -ne $true -or
        (Get-Field $independencePolicy 'scientific_authority_and_architecture_lead_must_differ') -ne $true) {
        $issues.Add('R0 owner authorization independence policy is incomplete.')
    }
    $failClosedBoundaries = Get-Field $Authorization 'fail_closed_boundaries'
    foreach ($boundary in @(
            'rights_and_provenance', 'external_distribution', 'hosted_ci',
            'toolchain_qualification', 'task_and_gate_status', 'legacy_boundary')) {
        if ([string]::IsNullOrWhiteSpace([string](Get-Field $failClosedBoundaries $boundary))) {
            $issues.Add("R0 owner authorization is missing fail-closed boundary '$boundary'.")
        }
    }

    $actorById = Get-ActorMap $Authorization
    $expectedActors = [ordered]@{
        'r0-po-agent' = [ordered]@{
            task = '/root'; roles = @('product_owner'); reviewer = 'r0-validation-agent'
        }
        'r0-architecture-agent' = [ordered]@{
            task = '/root/r0_architecture_agent'; roles = @('architecture_lead', 'compiler_lead'); reviewer = 'r0-validation-agent'
        }
        'r0-science-agent' = [ordered]@{
            task = '/root/r0_science_agent'; roles = @('scientific_authority', 'model_sdk_lead'); reviewer = 'r0-architecture-agent'
        }
        'r0-validation-agent' = [ordered]@{
            task = '/root/r0_validation_agent'; roles = @('validation_lead', 'runtime_numerics_lead', 'evidence_workflow_lead'); reviewer = 'r0-po-agent'
        }
    }
    if ($actorById.Count -ne 4 -or
        (($actorById.Keys | Sort-Object) -join ',') -ne
        (($expectedActors.Keys | Sort-Object) -join ',')) {
        $issues.Add('R0 owner authorization must bind exactly the four authorized machine actors.')
    }
    $seenTaskPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($actorId in @($actorById.Keys)) {
        $actor = $actorById[$actorId]
        $taskPath = [string](Get-Field $actor 'task_path')
        if (-not (Test-AuthorizedActor $actorId $actorById)) {
            $issues.Add("Actor '$actorId' lacks explicit machine identity or task binding.")
        }
        if (-not $seenTaskPaths.Add($taskPath)) {
            $issues.Add("Actor '$actorId' reuses task path '$taskPath'.")
        }
        if (-not $expectedActors.Contains($actorId)) { continue }
        $expected = $expectedActors[$actorId]
        if ($taskPath -ne $expected.task -or
            ((@(Get-Field $actor 'authorized_roles') | Sort-Object) -join ',') -ne
            ((@($expected.roles) | Sort-Object) -join ',') -or
            (Get-Field $actor 'primary_reviewer_actor_id') -ne $expected.reviewer) {
            $issues.Add("Actor '$actorId' differs from the owner-authorized task, role or reviewer binding.")
        }
        $reviewerId = [string](Get-Field $actor 'primary_reviewer_actor_id')
        if (-not $actorById.ContainsKey($reviewerId) -or $reviewerId -eq $actorId) {
            $issues.Add("Actor '$actorId' has no distinct authorized primary reviewer.")
        }
        elseif ((Get-Field $actorById[$reviewerId] 'task_path') -eq $taskPath) {
            $issues.Add("Actor '$actorId' shares a task path with its primary reviewer.")
        }
    }

    if ($null -eq $Disposition) {
        $issues.Add('ADR-0009 Accepted disposition record is missing.')
    }
    else {
        if ((Get-Field $Disposition 'schema_version') -ne
            'gnczmkn.adr-disposition/1' -or
            (Get-Field $Disposition 'adr_id') -ne 'ADR-0009' -or
            (Get-Field $Disposition 'decision') -ne 'accept-as-written' -or
            (Get-Field $Disposition 'decision_date') -ne '2026-08-12' -or
            (Get-Field $Disposition 'authorization_ref') -ne
            'docs/governance/r0-owner-authorization.json') {
            $issues.Add('ADR-0009 disposition identity, decision or authorization reference is invalid.')
        }

        $reviewedCommit = [string](Get-Field $Disposition 'reviewed_commit')
        $reviewedParent = [string](Get-Field $Disposition 'reviewed_parent')
        $computedFileset = $null
        try {
            $computedFileset = Get-ReviewedFileset $reviewedCommit $reviewedParent
        }
        catch {
            $issues.Add("ADR-0009 disposition reviewed commit cannot be reproduced: $($_.Exception.Message)")
        }
        $recordedFileset = Get-Field $Disposition 'reviewed_fileset'
        if ($null -ne $computedFileset) {
            foreach ($field in @(
                    'algorithm_id', 'path_order', 'entry_format',
                    'manifest_encoding', 'line_ending', 'path_count', 'sha256')) {
                if ((Get-Field $recordedFileset $field) -ne
                    (Get-Field $computedFileset $field)) {
                    $issues.Add("ADR-0009 disposition fileset field '$field' does not match the reviewed Git commit.")
                }
            }
            $recordedEntries = @(Get-Field $recordedFileset 'entries')
            $computedEntries = @(Get-Field $computedFileset 'entries')
            if ($recordedEntries.Count -ne $computedEntries.Count) {
                $issues.Add('ADR-0009 disposition fileset entry count is incorrect.')
            }
            else {
                for ($index = 0; $index -lt $computedEntries.Count; ++$index) {
                    foreach ($field in @('path', 'byte_length', 'sha256')) {
                        if ((Get-Field $recordedEntries[$index] $field) -ne
                            (Get-Field $computedEntries[$index] $field)) {
                            $issues.Add("ADR-0009 disposition fileset entry $index field '$field' is incorrect.")
                        }
                    }
                }
            }
        }

        $expectedDecisionActors = [ordered]@{
            'r0-po-agent' = [ordered]@{
                role = 'product_owner'; task = '/root'
            }
            'r0-architecture-agent' = [ordered]@{
                role = 'architecture_lead'; task = '/root/r0_architecture_agent'
            }
        }
        $decisionActors = @(Get-Field $Disposition 'decision_actors')
        if ($decisionActors.Count -ne 2) {
            $issues.Add('ADR-0009 disposition must have exactly Product and Architecture decision actors.')
        }
        $seenDecisionActors = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        $seenDecisionTasks = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        foreach ($decisionActor in $decisionActors) {
            $actorId = [string](Get-Field $decisionActor 'actor_id')
            $taskPath = [string](Get-Field $decisionActor 'task_path')
            [void]$seenDecisionActors.Add($actorId)
            [void]$seenDecisionTasks.Add($taskPath)
            if (-not $expectedDecisionActors.Contains($actorId)) {
                $issues.Add("ADR-0009 disposition contains unauthorized decision actor '$actorId'.")
                continue
            }
            $expected = $expectedDecisionActors[$actorId]
            if (-not (Test-AuthorizedActor $actorId $actorById $expected.role) -or
                (Get-Field $decisionActor 'kind') -ne 'machine_agent' -or
                (Get-Field $decisionActor 'role') -ne $expected.role -or
                $taskPath -ne $expected.task -or
                (Get-Field $decisionActor 'decision') -ne 'accept-as-written' -or
                (Get-Field $actorById[$actorId] 'task_path') -ne $taskPath) {
                $issues.Add("ADR-0009 decision actor '$actorId' has invalid role, task, kind or decision.")
            }
        }
        if ($seenDecisionActors.Count -ne 2 -or $seenDecisionTasks.Count -ne 2) {
            $issues.Add('ADR-0009 decision actors must use two distinct authorized actor and task identities.')
        }

        $independentReview = Get-Field $Disposition 'independent_review'
        $reviewActorId = [string](Get-Field $independentReview 'actor_id')
        $reviewTaskPath = [string](Get-Field $independentReview 'task_path')
        if ($reviewActorId -ne 'r0-validation-agent' -or
            -not (Test-AuthorizedActor $reviewActorId $actorById 'validation_lead') -or
            (Get-Field $independentReview 'kind') -ne 'machine_agent' -or
            (Get-Field $independentReview 'role') -ne 'validation_lead' -or
            (Get-Field $independentReview 'result') -ne 'approved' -or
            $reviewTaskPath -ne '/root/r0_validation_agent' -or
            (Get-Field $independentReview 'reviewed_commit') -ne $reviewedCommit -or
            (Get-Field $independentReview 'reviewed_fileset_sha256') -ne
            (Get-Field $recordedFileset 'sha256') -or
            $seenDecisionActors.Contains($reviewActorId) -or
            $seenDecisionTasks.Contains($reviewTaskPath)) {
            $issues.Add('ADR-0009 disposition lacks an independent authorized Validation approval for the reviewed commit/fileset.')
        }

        if (@(Get-Field $Disposition 'rationale').Count -lt 3) {
            $issues.Add('ADR-0009 disposition rationale is incomplete.')
        }
        $verification = Get-Field $Disposition 'verification'
        foreach ($field in @(
                'team_toolchain_validator', 'license_provenance_validator',
                'ctest_dev', 'repository_verifier', 'diff_check')) {
            if ([string]::IsNullOrWhiteSpace([string](Get-Field $verification $field))) {
                $issues.Add("ADR-0009 disposition verification field '$field' is empty.")
            }
        }
        $expectedBoundaries = @(
            'hosted-ci-commit-bound-evidence',
            'toolchain-supported-qualification',
            'rights-and-provenance-decisions',
            'external-distribution',
            'task-acceptance',
            'G0-and-G1',
            'R1-production-implementation')
        if (((@(Get-Field $Disposition 'unresolved_boundaries') | Sort-Object) -join ',') -ne
            (($expectedBoundaries | Sort-Object) -join ',')) {
            $issues.Add('ADR-0009 disposition does not preserve the complete fail-closed boundary set.')
        }
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
        if ($null -ne $assignee -and
            -not (Test-AuthorizedActor $assignee $actorById $id)) {
            $issues.Add("Role '$id' has an unauthorized or out-of-scope assignee.")
        }
        if ($null -ne $reviewer -and
            -not (Test-AuthorizedActor $reviewer $actorById)) {
            $issues.Add("Role '$id' has an unauthorized reviewer.")
        }
        if ((Test-AuthorizedActor $assignee $actorById $id) -xor
            (Test-AuthorizedActor $reviewer $actorById)) {
            $issues.Add("Role '$id' has only one side of the assignee/reviewer pair.")
        }
        if ((Test-AuthorizedActor $assignee $actorById $id) -and
            (Test-AuthorizedActor $reviewer $actorById) -and
            [string]$assignee -eq [string]$reviewer) {
            $issues.Add("Role '$id' has the same assignee and reviewer.")
        }
        if ((Test-AuthorizedActor $assignee $actorById $id) -and
            (Test-AuthorizedActor $reviewer $actorById) -and
            (Get-Field $actorById[[string]$assignee] 'task_path') -eq
            (Get-Field $actorById[[string]$reviewer] 'task_path')) {
            $issues.Add("Role '$id' has assignee and reviewer on the same task path.")
        }
        if ((Test-AuthorizedActor $assignee $actorById $id) -and
            (Get-Field $actorById[[string]$assignee] 'primary_reviewer_actor_id') -ne
            [string]$reviewer) {
            $issues.Add("Role '$id' differs from the owner-authorized review pairing.")
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

    $roleReadiness = Get-RoleReadiness $Roles $Authorization
    if ($roleById.ContainsKey('scientific_authority') -and
        $roleById.ContainsKey('architecture_lead')) {
        $scientificAssignee = Get-Field $roleById['scientific_authority'] 'assignee'
        $architectureAssignee = Get-Field $roleById['architecture_lead'] 'assignee'
        if ((Test-AuthorizedActor $scientificAssignee $actorById 'scientific_authority') -and
            (Test-AuthorizedActor $architectureAssignee $actorById 'architecture_lead') -and
            [string]$scientificAssignee -eq [string]$architectureAssignee) {
            $issues.Add('Scientific Authority and Architecture Lead share the same high-risk assignee.')
        }
        if ((Test-AuthorizedActor $scientificAssignee $actorById 'scientific_authority') -and
            (Test-AuthorizedActor $architectureAssignee $actorById 'architecture_lead') -and
            (Get-Field $actorById[[string]$scientificAssignee] 'task_path') -eq
            (Get-Field $actorById[[string]$architectureAssignee] 'task_path')) {
            $issues.Add('Scientific Authority and Architecture Lead share the same high-risk task path.')
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
        (Get-Field $qualificationPolicy 'evidence_receipt') -ne
        'docs/quality/hosted-ci-evidence-R0-GOV-001.json' -or
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
            'fetch-depth: 0',
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
    if ($adrStatus -eq 'Accepted' -and
        -not $AdrText.Contains(
            '../governance/adr-dispositions/ADR-0009-2026-08-12.json')) {
        $issues.Add('ADR-0009 is Accepted without its authoritative disposition reference.')
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
        authorization = Copy-JsonObject $script:authorization
        disposition = Copy-JsonObject $script:disposition
        matrix = Copy-JsonObject $script:matrix
        hosted_ci_evidence = Copy-JsonObject $script:hostedCiEvidence
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
            $context.authorization `
            $context.disposition `
            $context.matrix `
            $context.hosted_ci_evidence `
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
    'docs/governance/r0-owner-authorization.json',
    'docs/governance/adr-dispositions/ADR-0009-2026-08-12.json',
    'docs/governance/toolchain-support-matrix.json',
    'docs/quality/hosted-ci-evidence-R0-GOV-001.json',
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
$authorization = Read-Json $authorizationPath
$disposition = Read-Json $dispositionPath
$matrix = Read-Json $matrixPath
$hostedCiEvidence = Read-Json $hostedCiEvidencePath
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
$script:authorization = $authorization
$script:disposition = $disposition
$script:matrix = $matrix
$script:hostedCiEvidence = $hostedCiEvidence
$script:backlog = $backlog
$script:presets = $presets
$script:manifest = $manifest
$script:adrText = $adrText
$script:workflowText = $workflowText
$script:cmakeText = $cmakeText

if ($null -ne $roles -and $null -ne $authorization -and
    $null -ne $disposition -and
    $null -ne $matrix -and $null -ne $hostedCiEvidence -and
    $null -ne $backlog -and
    $null -ne $presets -and $null -ne $manifest) {
    foreach ($issue in @(Test-GovernanceObjects `
            $roles $authorization $disposition $matrix $hostedCiEvidence $backlog $presets $manifest `
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
        $value.roles.roles[0].assignee = 'r0-po-agent'
        $value.roles.roles[0].reviewer = 'r0-po-agent'
    }
    Invoke-Mutation 'shared-science-architecture-assignee' {
        param($value)
        $scientific = @($value.roles.roles | Where-Object {
                $_.id -eq 'scientific_authority'
            }) | Select-Object -First 1
        $architecture = @($value.roles.roles | Where-Object {
                $_.id -eq 'architecture_lead'
            }) | Select-Object -First 1
        $architecture.assignee = $scientific.assignee
        $architecture.reviewer = $scientific.reviewer
    }
    Invoke-Mutation 'accepted-adr-without-roles' {
        param($value)
        $value.roles.roles[0].reviewer = $null
        $value.adr_text = $value.adr_text.Replace('- Status: Proposed', '- Status: Accepted')
        $value.matrix.decision_status = 'accepted'
    }
    Invoke-Mutation 'authorization-source-digest-missing' {
        param($value)
        $value.authorization.source_instruction.sha256 = ''
    }
    Invoke-Mutation 'machine-actor-impersonates-human' {
        param($value)
        $value.authorization.actors[0].kind = 'human'
    }
    Invoke-Mutation 'duplicate-actor-task-binding' {
        param($value)
        $value.authorization.actors[1].task_path = $value.authorization.actors[0].task_path
    }
    Invoke-Mutation 'fewer-than-four-authorized-actors' {
        param($value)
        $value.authorization.actors = @($value.authorization.actors | Select-Object -First 3)
    }
    Invoke-Mutation 'science-architecture-shared-task-binding' {
        param($value)
        $science = @($value.authorization.actors | Where-Object {
                $_.id -eq 'r0-science-agent'
            }) | Select-Object -First 1
        $architecture = @($value.authorization.actors | Where-Object {
                $_.id -eq 'r0-architecture-agent'
            }) | Select-Object -First 1
        $architecture.task_path = $science.task_path
    }
    Invoke-Mutation 'authorization-scope-expands-to-r1' {
        param($value)
        $value.authorization.scope.stage = 'R1'
    }
    Invoke-Mutation 'unregistered-machine-alias' {
        param($value)
        $value.roles.roles[0].assignee = 'r0-unregistered-agent'
    }
    Invoke-Mutation 'missing-accepted-disposition' {
        param($value)
        $value.disposition = $null
    }
    Invoke-Mutation 'wrong-disposition-reviewed-commit' {
        param($value)
        $value.disposition.reviewed_commit =
            '0000000000000000000000000000000000000000'
    }
    Invoke-Mutation 'wrong-disposition-fileset-hash' {
        param($value)
        $value.disposition.reviewed_fileset.sha256 =
            '0000000000000000000000000000000000000000000000000000000000000000'
    }
    Invoke-Mutation 'wrong-disposition-decision-actor' {
        param($value)
        $value.disposition.decision_actors[0].actor_id = 'r0-science-agent'
    }
    Invoke-Mutation 'disposition-self-review' {
        param($value)
        $value.disposition.independent_review.actor_id = 'r0-po-agent'
        $value.disposition.independent_review.task_path = '/root'
        $value.disposition.independent_review.role = 'product_owner'
    }
    Invoke-Mutation 'disposition-drops-fail-closed-boundary' {
        param($value)
        $value.disposition.unresolved_boundaries = @(
            $value.disposition.unresolved_boundaries | Where-Object {
                $_ -ne 'external-distribution'
            })
    }
    Invoke-Mutation 'missing-hosted-ci-evidence' {
        param($value)
        $value.hosted_ci_evidence = $null
    }
    Invoke-Mutation 'hosted-ci-receipt-wrong-head' {
        param($value)
        $value.hosted_ci_evidence.runs.primary.source_head_sha =
            '0000000000000000000000000000000000000000'
    }
    Invoke-Mutation 'hosted-ci-pr-head-confused-with-checkout' {
        param($value)
        $value.hosted_ci_evidence.runs.repeat.checked_out_sha =
            $value.hosted_ci_evidence.runs.repeat.source_head_sha
    }
    Invoke-Mutation 'hosted-ci-receipt-failed-job' {
        param($value)
        $value.hosted_ci_evidence.runs.primary.jobs[0].conclusion = 'failure'
    }
    Invoke-Mutation 'hosted-ci-receipt-missing-image-identity' {
        param($value)
        $value.hosted_ci_evidence.profiles[0].image_version = ''
    }
    Invoke-Mutation 'hosted-ci-receipt-wrong-workflow-hash' {
        param($value)
        $value.hosted_ci_evidence.qualification.workflow.raw_sha256 =
            '0000000000000000000000000000000000000000000000000000000000000000'
    }
    Invoke-Mutation 'hosted-ci-receipt-wrong-qualification-tree' {
        param($value)
        $value.hosted_ci_evidence.qualification.tree =
            '0000000000000000000000000000000000000000'
    }
    Invoke-Mutation 'hosted-ci-receipt-corrupt-pr-commit-object' {
        param($value)
        $value.hosted_ci_evidence.runs.repeat.checked_out_commit_object.sha1 =
            '0000000000000000000000000000000000000000'
    }
    Invoke-Mutation 'hosted-ci-receipt-missing-required-step' {
        param($value)
        $value.hosted_ci_evidence.runs.primary.jobs[0].required_step_conclusions.PSObject.Properties.Remove(
            'Repository checks')
    }
    Invoke-Mutation 'hosted-ci-receipt-failed-required-step' {
        param($value)
        $value.hosted_ci_evidence.runs.primary.jobs[1].required_step_conclusions.Test = 'failure'
    }
    Invoke-Mutation 'hosted-ci-receipt-wrong-platform-skip' {
        param($value)
        $value.hosted_ci_evidence.runs.repeat.jobs[0].required_step_conclusions.'Configure Windows candidate' = 'success'
    }
    Invoke-Mutation 'hosted-ci-receipt-wrong-repeat-job-url' {
        param($value)
        $value.hosted_ci_evidence.runs.repeat.jobs[1].url =
            'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31559704268/job/0'
    }
    Invoke-Mutation 'hosted-ci-receipt-missing-repeat-image-identity' {
        param($value)
        $value.hosted_ci_evidence.repeat_profiles[1].image_version = ''
    }
    Invoke-Mutation 'hosted-ci-receipt-self-review' {
        param($value)
        $value.hosted_ci_evidence.decision.actors[1].actor_id = 'r0-po-agent'
        $value.hosted_ci_evidence.decision.actors[1].task_path = '/root'
        $value.hosted_ci_evidence.decision.actors[1].role = 'product_owner'
    }
    Invoke-Mutation 'hosted-ci-receipt-drops-retention' {
        param($value)
        $value.hosted_ci_evidence.retention.required_facts_permanently_retained = $false
    }
    Invoke-Mutation 'hosted-ci-receipt-artifact-misclaim' {
        param($value)
        $value.hosted_ci_evidence.runs.primary.artifact_count = 1
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
    Invoke-Mutation 'shallow-checkout-hides-reviewed-commit' {
        param($value)
        $value.workflow_text = $value.workflow_text.Replace(
            'fetch-depth: 0', 'fetch-depth: 1')
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
    Invoke-Mutation 'task-completion-without-hosted-ci' {
        param($value)
        $task = @($value.backlog.tasks | Where-Object {
                $_.id -eq 'R0-GOV-001'
        }) | Select-Object -First 1
        $task.status = 'done'
        $value.matrix.qualification_status = 'candidate-not-supported'
        $value.matrix.qualification_policy.hosted_ci_status = 'pending-push-and-run'
        $value.hosted_ci_evidence = $null
    }
}

$roleReadiness = Get-RoleReadiness $roles $authorization
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
    reviewed_on = '2026-08-12'
    status = if ($governanceReady) { 'ready' } else { 'conformant-with-blockers' }
    configuration_validation = if ($errors.Count -eq 0) { 'passed' } else { 'failed' }
    governance_ready = $governanceReady
    blockers = @($blockers)
    authorization = [ordered]@{
        authorization_id = Get-Field $authorization 'authorization_id'
        authorization_status = Get-Field $authorization 'authorization_status'
        authorized_on = Get-Field $authorization 'authorized_on'
        source_instruction_sha256 = Get-Field (Get-Field $authorization 'source_instruction') 'sha256'
        shared_thread_id = Get-Field $authorization 'shared_thread_id'
        stage = Get-Field (Get-Field $authorization 'scope') 'stage'
        machine_actor_count = @((Get-Field $authorization 'actors') | Where-Object {
                (Get-Field $_ 'kind') -eq 'machine_agent'
            }).Count
        unique_task_binding_count = @((Get-Field $authorization 'actors') | ForEach-Object {
                [string](Get-Field $_ 'task_path')
            } | Sort-Object -Unique).Count
    }
    disposition = [ordered]@{
        adr_id = Get-Field $disposition 'adr_id'
        decision = Get-Field $disposition 'decision'
        decision_date = Get-Field $disposition 'decision_date'
        reviewed_commit = Get-Field $disposition 'reviewed_commit'
        reviewed_parent = Get-Field $disposition 'reviewed_parent'
        fileset_algorithm = Get-Field (Get-Field $disposition 'reviewed_fileset') 'algorithm_id'
        fileset_path_count = Get-Field (Get-Field $disposition 'reviewed_fileset') 'path_count'
        fileset_sha256 = Get-Field (Get-Field $disposition 'reviewed_fileset') 'sha256'
        decision_actor_ids = @((Get-Field $disposition 'decision_actors') | ForEach-Object {
                Get-Field $_ 'actor_id'
            })
        independent_review_actor_id = Get-Field (Get-Field $disposition 'independent_review') 'actor_id'
        independent_review_result = Get-Field (Get-Field $disposition 'independent_review') 'result'
        unresolved_boundaries = @(Get-Field $disposition 'unresolved_boundaries')
    }
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
    hosted_ci_evidence = [ordered]@{
        evidence_id = Get-Field $hostedCiEvidence 'evidence_id'
        evidence_status = Get-Field $hostedCiEvidence 'evidence_status'
        decision_id = Get-Field (Get-Field $hostedCiEvidence 'decision') 'id'
        decision_outcome = Get-Field (Get-Field $hostedCiEvidence 'decision') 'outcome'
        qualification_commit = Get-Field (Get-Field $hostedCiEvidence 'qualification') 'commit'
        primary_run_id = Get-Field (Get-Field (Get-Field $hostedCiEvidence 'runs') 'primary') 'run_id'
        primary_checked_out_sha = Get-Field (Get-Field (Get-Field $hostedCiEvidence 'runs') 'primary') 'checked_out_sha'
        repeat_run_id = Get-Field (Get-Field (Get-Field $hostedCiEvidence 'runs') 'repeat') 'run_id'
        repeat_checked_out_sha = Get-Field (Get-Field (Get-Field $hostedCiEvidence 'runs') 'repeat') 'checked_out_sha'
        successful_profile_count = @((Get-Field $hostedCiEvidence 'profiles') | Where-Object {
                (Get-Field (Get-Field $_ 'verification') 'ctest') -eq '9/9 passed'
            }).Count
        artifact_count = Get-Field (Get-Field $hostedCiEvidence 'retention') 'artifact_count'
        repository_receipt = Get-Field (Get-Field $hostedCiEvidence 'retention') 'repository_receipt'
        upstream_logs = Get-Field (Get-Field $hostedCiEvidence 'retention') 'upstream_logs'
    }
    workflow = [ordered]@{
        runner_labels = @('ubuntu-24.04', 'windows-2025-vs2026')
        checkout_version = '6.0.2'
        checkout_commit = 'de0fac2e4500dabe0009e67214ff5f5447ce83dd'
        fetch_depth = 0
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

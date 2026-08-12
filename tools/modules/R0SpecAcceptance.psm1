Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-R0SpecField {
    param($Object, [string]$Name)

    if ($null -eq $Object) { return $null }
    foreach ($property in @($Object.PSObject.Properties)) {
        if ([string]::Equals($property.Name, $Name, [System.StringComparison]::Ordinal)) {
            return $property.Value
        }
    }
    return $null
}

function Read-R0SpecJson {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    try {
        return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    catch {
        return $null
    }
}

function Copy-R0SpecObject {
    param($Object)

    if ($null -eq $Object) { return $null }
    return $Object | ConvertTo-Json -Depth 100 | ConvertFrom-Json
}

function Get-R0SpecSha256Hex {
    param([byte[]]$Bytes)

    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString(
                $algorithm.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Get-R0SpecSha1Hex {
    param([byte[]]$Bytes)

    $algorithm = [System.Security.Cryptography.SHA1]::Create()
    try {
        return ([System.BitConverter]::ToString(
                $algorithm.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Get-R0SpecGitBlobFact {
    param(
        [string]$RepoRoot,
        [string]$Commit,
        [string]$Path
    )

    $objectId = [string](& git -C $RepoRoot rev-parse "$Commit`:$Path" 2>$null)
    if ($LASTEXITCODE -ne 0 -or $objectId.Trim() -notmatch '^[0-9a-f]{40}$') {
        throw "Cannot resolve Git blob '$Commit`:$Path'."
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = 'git'
    $startInfo.Arguments = "cat-file blob $($objectId.Trim())"
    $startInfo.WorkingDirectory = $RepoRoot
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
        [byte[]]$bytes = $memory.ToArray()
        return [pscustomobject][ordered]@{
            path = $Path
            byte_length = $bytes.Length
            sha256 = Get-R0SpecSha256Hex $bytes
        }
    }
    finally {
        $memory.Dispose()
        $process.Dispose()
    }
}

function Get-R0SpecReviewedFileset {
    param(
        [string]$RepoRoot,
        [string]$Base,
        [string]$Commit,
        [string]$DirectParent
    )

    foreach ($sha in @($Base, $Commit, $DirectParent)) {
        if ($sha -notmatch '^[0-9a-f]{40}$') {
            throw 'Reviewed base, commit and direct parent must be lowercase full SHA-1 values.'
        }
    }
    $actualParent = [string](& git -C $RepoRoot rev-parse "$Commit^" 2>$null)
    if ($LASTEXITCODE -ne 0 -or $actualParent.Trim() -ne $DirectParent) {
        throw "Reviewed commit '$Commit' does not have direct parent '$DirectParent'."
    }
    & git -C $RepoRoot merge-base --is-ancestor $Base $Commit 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Reviewed base '$Base' is not an ancestor of '$Commit'."
    }

    [string[]]$paths = @(& git -C $RepoRoot diff --name-only "$Base..$Commit" --)
    if ($LASTEXITCODE -ne 0 -or $paths.Count -eq 0) {
        throw "Reviewed range '$Base..$Commit' has no readable changed-path set."
    }
    [System.Array]::Sort($paths, [System.StringComparer]::Ordinal)
    $entries = [System.Collections.Generic.List[object]]::new()
    $manifestLines = [System.Collections.Generic.List[string]]::new()
    foreach ($path in $paths) {
        $entry = Get-R0SpecGitBlobFact -RepoRoot $RepoRoot -Commit $Commit -Path $path
        $entries.Add($entry)
        $manifestLines.Add("$($entry.path)`t$($entry.byte_length)`t$($entry.sha256)")
    }
    $manifest = [string]::Join("`n", $manifestLines) + "`n"
    [byte[]]$manifestBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($manifest)
    return [pscustomobject][ordered]@{
        algorithm_id = 'gnczmkn.fileset-manifest/1'
        scope = 'reviewed_base..reviewed_commit'
        path_order = 'ordinal-ascending'
        entry_format = 'path<TAB>raw_git_blob_byte_length<TAB>lowercase_sha256(raw_git_blob_bytes)'
        manifest_encoding = 'UTF-8 without BOM'
        line_ending = 'LF with a final LF'
        path_count = $entries.Count
        sha256 = Get-R0SpecSha256Hex $manifestBytes
        entries = @($entries)
    }
}

function Get-R0SpecRetainedCommitFact {
    param($Object)

    if ((Get-R0SpecField $Object 'object_type') -ne 'commit' -or
        (Get-R0SpecField $Object 'content_encoding') -ne 'base64') {
        throw 'Retained Git object type or encoding is invalid.'
    }
    try {
        [byte[]]$content = [System.Convert]::FromBase64String(
            [string](Get-R0SpecField $Object 'content_base64'))
    }
    catch {
        throw "Retained Git commit base64 is invalid: $($_.Exception.Message)"
    }
    if ($content.Length -ne (Get-R0SpecField $Object 'byte_length')) {
        throw 'Retained Git commit byte length is incorrect.'
    }
    $contentSha256 = Get-R0SpecSha256Hex $content
    if ($contentSha256 -ne (Get-R0SpecField $Object 'content_sha256')) {
        throw 'Retained Git commit content SHA-256 is incorrect.'
    }
    [byte[]]$header = [System.Text.Encoding]::ASCII.GetBytes("commit $($content.Length)`0")
    [byte[]]$fullObject = New-Object byte[] ($header.Length + $content.Length)
    [System.Array]::Copy($header, 0, $fullObject, 0, $header.Length)
    [System.Array]::Copy($content, 0, $fullObject, $header.Length, $content.Length)
    $sha1 = Get-R0SpecSha1Hex $fullObject
    if ($sha1 -ne (Get-R0SpecField $Object 'sha1')) {
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
        content_sha256 = $contentSha256
        tree = $treeMatch.Groups[1].Value
        parents = @($parentMatches | ForEach-Object { $_.Groups[1].Value })
    }
}

function Add-R0SpecIssue {
    param(
        [System.Collections.Generic.List[string]]$Issues,
        [string]$Message
    )

    $Issues.Add($Message)
}

function Test-R0SpecRecordedFileset {
    param(
        $Recorded,
        $Expected,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    if ($null -eq $Recorded) {
        Add-R0SpecIssue $Issues "$Label has no reviewed fileset."
        return
    }
    foreach ($field in @(
            'algorithm_id', 'scope', 'path_order', 'entry_format',
            'manifest_encoding', 'line_ending', 'path_count', 'sha256')) {
        if ((Get-R0SpecField $Recorded $field) -ne (Get-R0SpecField $Expected $field)) {
            Add-R0SpecIssue $Issues "$Label fileset field '$field' is incorrect."
        }
    }
    $recordedEntries = @(Get-R0SpecField $Recorded 'entries')
    $expectedEntries = @(Get-R0SpecField $Expected 'entries')
    if ($recordedEntries.Count -ne $expectedEntries.Count) {
        Add-R0SpecIssue $Issues "$Label fileset entry count is incorrect."
        return
    }
    for ($index = 0; $index -lt $expectedEntries.Count; ++$index) {
        foreach ($field in @('path', 'byte_length', 'sha256')) {
            if ((Get-R0SpecField $recordedEntries[$index] $field) -ne
                (Get-R0SpecField $expectedEntries[$index] $field)) {
                Add-R0SpecIssue $Issues "$Label fileset entry $index field '$field' is incorrect."
            }
        }
    }
}

function Get-R0SpecActorMap {
    param($Authorization)

    $map = @{}
    foreach ($actor in @(Get-R0SpecField $Authorization 'actors')) {
        $id = [string](Get-R0SpecField $actor 'id')
        if (-not [string]::IsNullOrWhiteSpace($id) -and -not $map.ContainsKey($id)) {
            $map[$id] = $actor
        }
    }
    return $map
}

function Test-R0SpecActorRecord {
    param(
        $Record,
        [string]$ExpectedActor,
        [string]$ExpectedTask,
        [string[]]$ExpectedRoles,
        [string]$ExpectedDecision,
        $ActorMap,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    $actorId = [string](Get-R0SpecField $Record 'actor_id')
    $taskPath = [string](Get-R0SpecField $Record 'task_path')
    $roles = @(Get-R0SpecField $Record 'roles')
    if ($actorId -ne $ExpectedActor -or
        (Get-R0SpecField $Record 'kind') -ne 'machine_agent' -or
        $taskPath -ne $ExpectedTask -or
        ((@($roles | Sort-Object) -join ',') -ne (@($ExpectedRoles | Sort-Object) -join ',')) -or
        (Get-R0SpecField $Record 'decision') -ne $ExpectedDecision -or
        -not $ActorMap.ContainsKey($ExpectedActor)) {
        Add-R0SpecIssue $Issues "$Label actor identity, task, roles or decision is invalid."
        return
    }
    $authorized = $ActorMap[$ExpectedActor]
    if ((Get-R0SpecField $authorized 'kind') -ne 'machine_agent' -or
        (Get-R0SpecField $authorized 'task_path') -ne $ExpectedTask) {
        Add-R0SpecIssue $Issues "$Label actor does not match the owner authorization."
    }
    foreach ($role in $ExpectedRoles) {
        if ($role -notin @(Get-R0SpecField $authorized 'authorized_roles')) {
            Add-R0SpecIssue $Issues "$Label actor is not authorized for role '$role'."
        }
    }
}

function Test-R0SpecIndependentReview {
    param(
        $Review,
        $ActorMap,
        [string]$ReviewedCommit,
        [string]$ReviewedFilesetSha256,
        [string]$DecisionActorId,
        [string]$DecisionTaskPath,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    $reviewActor = [string](Get-R0SpecField $Review 'actor_id')
    $reviewTask = [string](Get-R0SpecField $Review 'task_path')
    if ($reviewActor -ne 'r0-validation-agent' -or
        (Get-R0SpecField $Review 'kind') -ne 'machine_agent' -or
        $reviewTask -ne '/root/r0_validation_agent' -or
        (Get-R0SpecField $Review 'role') -ne 'validation_lead' -or
        (Get-R0SpecField $Review 'result') -ne 'approved' -or
        (Get-R0SpecField $Review 'decision') -ne 'accept-as-written' -or
        (Get-R0SpecField $Review 'reviewed_commit') -ne $ReviewedCommit -or
        (Get-R0SpecField $Review 'reviewed_fileset_sha256') -ne $ReviewedFilesetSha256 -or
        $reviewActor -eq $DecisionActorId -or $reviewTask -eq $DecisionTaskPath -or
        -not $ActorMap.ContainsKey($reviewActor)) {
        Add-R0SpecIssue $Issues "$Label lacks the required independent Validation approval."
        return
    }
    $authorized = $ActorMap[$reviewActor]
    if ((Get-R0SpecField $authorized 'task_path') -ne $reviewTask -or
        'validation_lead' -notin @(Get-R0SpecField $authorized 'authorized_roles')) {
        Add-R0SpecIssue $Issues "$Label reviewer does not resolve through owner authorization."
    }
}

function Test-R0SpecCiJobs {
    param(
        $Jobs,
        $ExpectedJobs,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    $actualJobs = @($Jobs)
    if ($actualJobs.Count -ne $ExpectedJobs.Count) {
        Add-R0SpecIssue $Issues "$Label must contain both fixed CI profile jobs."
        return
    }
    foreach ($expected in $ExpectedJobs) {
        $job = @($actualJobs | Where-Object {
                [string](Get-R0SpecField $_ 'job_id') -eq $expected.job_id
            }) | Select-Object -First 1
        if ($null -eq $job -or
            (Get-R0SpecField $job 'name') -ne $expected.name -or
            (Get-R0SpecField $job 'runner_name') -ne $expected.runner_name -or
            (Get-R0SpecField $job 'runner_group') -ne 'GitHub Actions' -or
            (Get-R0SpecField $job 'conclusion') -ne 'success' -or
            [string]::IsNullOrWhiteSpace([string](Get-R0SpecField $job 'url'))) {
            Add-R0SpecIssue $Issues "$Label job '$($expected.job_id)' identity or result is invalid."
            continue
        }
        $steps = Get-R0SpecField $job 'required_step_conclusions'
        foreach ($step in @(
                'Check out repository', 'Record runner and tool identity',
                'Record configured compiler identity', 'Build', 'Test', 'Repository checks')) {
            if ((Get-R0SpecField $steps $step) -ne 'success') {
                Add-R0SpecIssue $Issues "$Label job '$($expected.job_id)' required step '$step' did not succeed."
            }
        }
        if ($expected.name -eq 'ubuntu-24.04-gcc-13') {
            if ((Get-R0SpecField $steps 'Configure Ubuntu candidate') -ne 'success' -or
                (Get-R0SpecField $steps 'Configure Windows candidate') -ne 'skipped' -or
                (Get-R0SpecField $steps 'Verify Windows PowerShell 5.1 compatibility') -ne 'skipped') {
                Add-R0SpecIssue $Issues "$Label Ubuntu job has incorrect platform-specific step results."
            }
        }
        else {
            if ((Get-R0SpecField $steps 'Configure Ubuntu candidate') -ne 'skipped' -or
                (Get-R0SpecField $steps 'Configure Windows candidate') -ne 'success' -or
                (Get-R0SpecField $steps 'Verify Windows PowerShell 5.1 compatibility') -ne 'success') {
                Add-R0SpecIssue $Issues "$Label Windows job has incorrect platform-specific step results."
            }
        }
    }
}

function Get-R0SpecAcceptanceIssues {
    param(
        $Context,
        $ExpectedFileset
    )

    $issues = [System.Collections.Generic.List[string]]::new()
    $expectedBase = '611a48a23ea02ecd0c210a2b101f5c5cbf5df0e6'
    $expectedCommit = 'ee7157359e689114d0259a1ae7884a315b029bc1'
    $expectedParent = 'e5c711cd26c735eafb59084724682155356a9f45'
    $authorization = Get-R0SpecField $Context 'authorization'
    $actorMap = Get-R0SpecActorMap $authorization
    if ((Get-R0SpecField $authorization 'authorization_id') -ne 'R0-OWNER-AUTH-2026-08-12' -or
        (Get-R0SpecField $authorization 'authorization_status') -ne 'active') {
        Add-R0SpecIssue $issues 'R0-SPEC-001 acceptance lacks the active owner authorization.'
    }

    $adrText = [string](Get-R0SpecField $Context 'adr_text')
    if ($adrText -notmatch '(?m)^- Status: Accepted$' -or
        -not $adrText.Contains('../governance/adr-dispositions/ADR-0004-2026-08-12.json') -or
        $adrText.Contains('### Proposed reconciliation dispositions')) {
        Add-R0SpecIssue $issues 'ADR-0004 status, disposition reference or accepted decision text is inconsistent.'
    }

    $lock = Get-R0SpecField $Context 'lock'
    if ((Get-R0SpecField $lock 'decision_status') -ne 'accepted') {
        Add-R0SpecIssue $issues 'R0 schema contract lock is not accepted.'
    }
    $expectedDecisionRefs = [ordered]@{
        'RECON-DEC-001' = 'docs/governance/reconciliation-dispositions/RECON-DEC-001-2026-08-12.json'
        'RECON-DEC-002' = 'docs/governance/reconciliation-dispositions/RECON-DEC-002-2026-08-12.json'
        'RECON-DEC-003' = 'docs/governance/reconciliation-dispositions/RECON-DEC-003-2026-08-12.json'
    }
    $expectedOutcomes = [ordered]@{
        'RECON-DEC-001' = 'keep-current'
        'RECON-DEC-002' = 'repository-root-only'
        'RECON-DEC-003' = 'keep-current'
    }
    $lockDecisions = @{}
    foreach ($decision in @(Get-R0SpecField $lock 'reconciliation_decisions')) {
        $lockDecisions[[string](Get-R0SpecField $decision 'id')] = $decision
    }
    foreach ($id in $expectedDecisionRefs.Keys) {
        if (-not $lockDecisions.ContainsKey($id) -or
            (Get-R0SpecField $lockDecisions[$id] 'status') -ne 'accepted' -or
            (Get-R0SpecField $lockDecisions[$id] 'outcome') -ne $expectedOutcomes[$id] -or
            (Get-R0SpecField $lockDecisions[$id] 'record_ref') -ne $expectedDecisionRefs[$id]) {
            Add-R0SpecIssue $issues "Schema contract lock decision '$id' is incomplete or inconsistent."
        }
    }

    $disposition = Get-R0SpecField $Context 'adr_disposition'
    if ($null -eq $disposition) {
        Add-R0SpecIssue $issues 'ADR-0004 Accepted disposition record is missing.'
    }
    else {
        if ((Get-R0SpecField $disposition 'schema_version') -ne 'gnczmkn.adr-disposition/1' -or
            (Get-R0SpecField $disposition 'adr_id') -ne 'ADR-0004' -or
            (Get-R0SpecField $disposition 'decision') -ne 'accept-as-written' -or
            (Get-R0SpecField $disposition 'decision_date') -ne '2026-08-12' -or
            (Get-R0SpecField $disposition 'reviewed_base') -ne $expectedBase -or
            (Get-R0SpecField $disposition 'reviewed_commit') -ne $expectedCommit -or
            (Get-R0SpecField $disposition 'reviewed_direct_parent') -ne $expectedParent -or
            (Get-R0SpecField $disposition 'authorization_ref') -ne 'docs/governance/r0-owner-authorization.json') {
            Add-R0SpecIssue $issues 'ADR-0004 disposition identity, decision or reviewed Git range is invalid.'
        }
        Test-R0SpecRecordedFileset `
            -Recorded (Get-R0SpecField $disposition 'reviewed_fileset') `
            -Expected $ExpectedFileset -Label 'ADR-0004 disposition' -Issues $issues
        $decisionActor = Get-R0SpecField $disposition 'decision_actor'
        Test-R0SpecActorRecord -Record $decisionActor `
            -ExpectedActor 'r0-architecture-agent' `
            -ExpectedTask '/root/r0_architecture_agent' `
            -ExpectedRoles @('architecture_lead', 'compiler_lead') `
            -ExpectedDecision 'accept-as-written' -ActorMap $actorMap `
            -Label 'ADR-0004 disposition' -Issues $issues
        Test-R0SpecIndependentReview `
            -Review (Get-R0SpecField $disposition 'independent_review') `
            -ActorMap $actorMap -ReviewedCommit $expectedCommit `
            -ReviewedFilesetSha256 $ExpectedFileset.sha256 `
            -DecisionActorId ([string](Get-R0SpecField $decisionActor 'actor_id')) `
            -DecisionTaskPath ([string](Get-R0SpecField $decisionActor 'task_path')) `
            -Label 'ADR-0004 disposition' -Issues $issues
        if (((@(Get-R0SpecField $disposition 'reconciliation_disposition_refs') | Sort-Object) -join ',') -ne
            ((@($expectedDecisionRefs.Values) | Sort-Object) -join ',')) {
            Add-R0SpecIssue $issues 'ADR-0004 disposition does not reference all three accepted reconciliation decisions.'
        }
        $boundaries = Get-R0SpecField $disposition 'boundaries'
        if ((Get-R0SpecField $boundaries 'schema_maturity') -ne 'Fixture' -or
            (Get-R0SpecField $boundaries 'runtime_consumers') -ne 0 -or
            (Get-R0SpecField $boundaries 'rights_and_science') -ne 'remain fail-closed' -or
            (Get-R0SpecField $boundaries 'r0_gate_and_r1') -ne 'remain locked') {
            Add-R0SpecIssue $issues 'ADR-0004 disposition drops a required fail-closed boundary.'
        }
    }

    $reconciliationRecords = Get-R0SpecField $Context 'reconciliation_records'
    $expectedContractIds = [ordered]@{
        'RECON-DEC-001' = 'schema-v1-identity-and-field-graph-fixed'
        'RECON-DEC-002' = 'repository-root-executable-evidence'
        'RECON-DEC-003' = 'plan-proof-v1-scalar-snapshot'
    }
    foreach ($id in $expectedDecisionRefs.Keys) {
        $record = Get-R0SpecField $reconciliationRecords $id
        if ($null -eq $record) {
            Add-R0SpecIssue $issues "$id accepted disposition record is missing."
            continue
        }
        if ((Get-R0SpecField $record 'schema_version') -ne 'gnczmkn.reconciliation-disposition/1' -or
            (Get-R0SpecField $record 'decision_id') -ne $id -or
            (Get-R0SpecField $record 'outcome') -ne $expectedOutcomes[$id] -or
            (Get-R0SpecField $record 'status') -ne 'accepted' -or
            (Get-R0SpecField $record 'decided_on') -ne '2026-08-12' -or
            (Get-R0SpecField $record 'reviewed_base') -ne $expectedBase -or
            (Get-R0SpecField $record 'reviewed_commit') -ne $expectedCommit -or
            (Get-R0SpecField $record 'reviewed_direct_parent') -ne $expectedParent -or
            (Get-R0SpecField $record 'authorization_ref') -ne 'docs/governance/r0-owner-authorization.json') {
            Add-R0SpecIssue $issues "$id identity, outcome or reviewed Git range is invalid."
        }
        Test-R0SpecRecordedFileset -Recorded (Get-R0SpecField $record 'reviewed_fileset') `
            -Expected $ExpectedFileset -Label $id -Issues $issues
        $decisionActor = Get-R0SpecField $record 'decision_actor'
        Test-R0SpecActorRecord -Record $decisionActor `
            -ExpectedActor 'r0-architecture-agent' `
            -ExpectedTask '/root/r0_architecture_agent' `
            -ExpectedRoles @('architecture_lead', 'compiler_lead') `
            -ExpectedDecision 'accept-as-written' -ActorMap $actorMap -Label $id -Issues $issues
        Test-R0SpecIndependentReview -Review (Get-R0SpecField $record 'independent_review') `
            -ActorMap $actorMap -ReviewedCommit $expectedCommit `
            -ReviewedFilesetSha256 $ExpectedFileset.sha256 `
            -DecisionActorId ([string](Get-R0SpecField $decisionActor 'actor_id')) `
            -DecisionTaskPath ([string](Get-R0SpecField $decisionActor 'task_path')) `
            -Label $id -Issues $issues
        $contract = Get-R0SpecField $record 'contract'
        if ((Get-R0SpecField $contract 'contract_id') -ne $expectedContractIds[$id] -or
            [string]::IsNullOrWhiteSpace([string](Get-R0SpecField $contract 'summary')) -or
            @(Get-R0SpecField $record 'required_mutations').Count -lt 2 -or
            [string]::IsNullOrWhiteSpace([string](Get-R0SpecField (Get-R0SpecField $record 'migration') 'rule'))) {
            Add-R0SpecIssue $issues "$id contract, mutation or migration record is incomplete."
        }
        $expectedBoundaries = @(
            'no-schema-runtime-or-public-consumer',
            'no-rights-or-scientific-qualification',
            'no-r0-gate-or-r1-unlock')
        if (((@(Get-R0SpecField $record 'boundaries') | Sort-Object) -join ',') -ne
            (($expectedBoundaries | Sort-Object) -join ',')) {
            Add-R0SpecIssue $issues "$id drops a required fail-closed boundary."
        }
    }

    $acceptance = Get-R0SpecField $Context 'task_acceptance'
    if ($null -eq $acceptance) {
        Add-R0SpecIssue $issues 'R0-SPEC-001 task acceptance record is missing.'
    }
    else {
        if ((Get-R0SpecField $acceptance 'schema_version') -ne 'gnczmkn.task-acceptance/1' -or
            (Get-R0SpecField $acceptance 'acceptance_id') -ne 'R0-SPEC-001-ACCEPTANCE-2026-08-12' -or
            (Get-R0SpecField $acceptance 'task_id') -ne 'R0-SPEC-001' -or
            (Get-R0SpecField $acceptance 'result') -ne 'accepted' -or
            (Get-R0SpecField $acceptance 'accepted_on') -ne '2026-08-12' -or
            (Get-R0SpecField $acceptance 'reviewed_base') -ne $expectedBase -or
            (Get-R0SpecField $acceptance 'reviewed_commit') -ne $expectedCommit -or
            (Get-R0SpecField $acceptance 'reviewed_direct_parent') -ne $expectedParent -or
            (Get-R0SpecField $acceptance 'authorization_ref') -ne 'docs/governance/r0-owner-authorization.json') {
            Add-R0SpecIssue $issues 'R0-SPEC-001 task acceptance identity or reviewed Git range is invalid.'
        }
        Test-R0SpecRecordedFileset -Recorded (Get-R0SpecField $acceptance 'reviewed_fileset') `
            -Expected $ExpectedFileset -Label 'R0-SPEC-001 task acceptance' -Issues $issues
        $implementation = Get-R0SpecField $acceptance 'implementation_actor'
        Test-R0SpecActorRecord -Record $implementation `
            -ExpectedActor 'r0-architecture-agent' `
            -ExpectedTask '/root/r0_architecture_agent' `
            -ExpectedRoles @('architecture_lead', 'compiler_lead') `
            -ExpectedDecision 'implemented' -ActorMap $actorMap `
            -Label 'R0-SPEC-001 task acceptance' -Issues $issues
        Test-R0SpecIndependentReview -Review (Get-R0SpecField $acceptance 'independent_reviewer') `
            -ActorMap $actorMap -ReviewedCommit $expectedCommit `
            -ReviewedFilesetSha256 $ExpectedFileset.sha256 `
            -DecisionActorId ([string](Get-R0SpecField $implementation 'actor_id')) `
            -DecisionTaskPath ([string](Get-R0SpecField $implementation 'task_path')) `
            -Label 'R0-SPEC-001 task acceptance' -Issues $issues

        $checks = Get-R0SpecField $acceptance 'acceptance_checks'
        $deliverables = Get-R0SpecField $checks 'deliverables'
        $criterion = Get-R0SpecField $checks 'acceptance'
        $evidence = Get-R0SpecField $checks 'evidence'
        if ((Get-R0SpecField $deliverables 'versioned_json_schemas') -ne 'passed' -or
            (Get-R0SpecField $deliverables 'valid_and_invalid_examples') -ne 'passed' -or
            (Get-R0SpecField $deliverables 'schema_validation_command') -ne 'passed' -or
            (Get-R0SpecField $criterion 'missing_contract_facts_fail_early') -ne 'passed' -or
            (Get-R0SpecField $evidence 'contract_lock') -ne 'specs/r0-schema-contract-lock.json' -or
            (Get-R0SpecField $evidence 'adr_disposition') -ne 'docs/governance/adr-dispositions/ADR-0004-2026-08-12.json' -or
            ((@(Get-R0SpecField $evidence 'reconciliation_dispositions') -join ',') -ne
            'docs/governance/reconciliation-dispositions/RECON-DEC-001-2026-08-12.json,docs/governance/reconciliation-dispositions/RECON-DEC-002-2026-08-12.json,docs/governance/reconciliation-dispositions/RECON-DEC-003-2026-08-12.json') -or
            (Get-R0SpecField $evidence 'work_package') -ne
            'docs/tasks/work-packages/R0-SPEC-001.md') {
            Add-R0SpecIssue $issues 'R0-SPEC-001 acceptance checks or evidence references are incomplete.'
        }

        $verification = Get-R0SpecField $acceptance 'commit_bound_verification'
        $push = Get-R0SpecField $verification 'push_run'
        $pullRequest = Get-R0SpecField $verification 'pull_request_run'
        if ([string](Get-R0SpecField $push 'run_id') -ne '31565404481' -or
            (Get-R0SpecField $push 'event') -ne 'push' -or
            (Get-R0SpecField $push 'url') -ne
            'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31565404481' -or
            (Get-R0SpecField $push 'source_head_sha') -ne $expectedCommit -or
            (Get-R0SpecField $push 'checked_out_sha') -ne $expectedCommit -or
            (Get-R0SpecField $push 'conclusion') -ne 'success') {
            Add-R0SpecIssue $issues 'R0-SPEC-001 push CI identity or result is invalid.'
        }
        Test-R0SpecCiJobs -Jobs (Get-R0SpecField $push 'jobs') -ExpectedJobs @(
            [pscustomobject]@{ job_id = '94016078875'; name = 'windows-2025-vs2026-msvc-19.5x'; runner_name = 'GitHub Actions 1000000099' },
            [pscustomobject]@{ job_id = '94016078976'; name = 'ubuntu-24.04-gcc-13'; runner_name = 'GitHub Actions 1000000100' }) `
            -Label 'R0-SPEC-001 push CI' -Issues $issues
        if ([string](Get-R0SpecField $pullRequest 'run_id') -ne '31565406888' -or
            (Get-R0SpecField $pullRequest 'event') -ne 'pull_request' -or
            (Get-R0SpecField $pullRequest 'url') -ne
            'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31565406888' -or
            (Get-R0SpecField $pullRequest 'source_head_sha') -ne $expectedCommit -or
            (Get-R0SpecField $pullRequest 'checked_out_ref') -ne 'refs/pull/3/merge' -or
            (Get-R0SpecField $pullRequest 'checked_out_sha') -ne '52a6630394a2c9720971bae677eea9cb2b71a674' -or
            (Get-R0SpecField $pullRequest 'checked_out_tree_sha') -ne '03e49d5b524426b0ae00170d4f36c3798ae83e1e' -or
            ((@(Get-R0SpecField $pullRequest 'checked_out_parent_shas') -join ',') -ne
            "$expectedBase,$expectedCommit") -or
            (Get-R0SpecField $pullRequest 'conclusion') -ne 'success') {
            Add-R0SpecIssue $issues 'R0-SPEC-001 pull-request CI identity or result is invalid.'
        }
        try {
            $retainedCommit = Get-R0SpecRetainedCommitFact `
                (Get-R0SpecField $pullRequest 'checked_out_commit_object')
            if ($retainedCommit.sha1 -ne '52a6630394a2c9720971bae677eea9cb2b71a674' -or
                $retainedCommit.tree -ne '03e49d5b524426b0ae00170d4f36c3798ae83e1e' -or
                ((@($retainedCommit.parents) -join ',') -ne "$expectedBase,$expectedCommit")) {
                Add-R0SpecIssue $issues 'R0-SPEC-001 retained PR merge commit does not match the reviewed tree and parents.'
            }
        }
        catch {
            Add-R0SpecIssue $issues "R0-SPEC-001 retained PR merge commit is invalid: $($_.Exception.Message)"
        }
        Test-R0SpecCiJobs -Jobs (Get-R0SpecField $pullRequest 'jobs') -ExpectedJobs @(
            [pscustomobject]@{ job_id = '94016086092'; name = 'windows-2025-vs2026-msvc-19.5x'; runner_name = 'GitHub Actions 1000000101' },
            [pscustomobject]@{ job_id = '94016086110'; name = 'ubuntu-24.04-gcc-13'; runner_name = 'GitHub Actions 1000000102' }) `
            -Label 'R0-SPEC-001 pull-request CI' -Issues $issues
        $local = Get-R0SpecField $verification 'local_and_repository'
        if ((Get-R0SpecField $local 'schema_validator') -ne 'passed; 20/20 contract mutations' -or
            (Get-R0SpecField $local 'acceptance_validator') -ne 'passed; 25/25 acceptance mutations' -or
            (Get-R0SpecField $local 'ctest') -ne 'passed; 9/9' -or
            (Get-R0SpecField $local 'repository_verifier') -ne 'passed; 60 JSON; 65 tasks; 100 Markdown' -or
            (Get-R0SpecField $local 'diff_check') -ne 'passed' -or
            (Get-R0SpecField $local 'strict_utf8_and_wording') -ne 'passed; 13/13 reviewed paths') {
            Add-R0SpecIssue $issues 'R0-SPEC-001 local or repository verification evidence is incomplete.'
        }
        $boundaries = Get-R0SpecField $acceptance 'boundaries'
        if ((Get-R0SpecField $boundaries 'schema_maturity') -ne 'Fixture' -or
            (Get-R0SpecField $boundaries 'runtime_consumers') -ne 0 -or
            (Get-R0SpecField $boundaries 'rights_and_external_distribution') -ne 'remain fail-closed' -or
            (Get-R0SpecField $boundaries 'scientific_qualification') -ne 'not established' -or
            (Get-R0SpecField $boundaries 'r0_gate') -ne 'R0-GATE-001 remains planned; G0 and G1 are not passed' -or
            (Get-R0SpecField $boundaries 'r1_through_r8') -ne 'remain locked') {
            Add-R0SpecIssue $issues 'R0-SPEC-001 task acceptance drops a required fail-closed boundary.'
        }
    }

    $backlog = Get-R0SpecField $Context 'backlog'
    $task = @((Get-R0SpecField $backlog 'tasks') | Where-Object {
            (Get-R0SpecField $_ 'id') -eq 'R0-SPEC-001'
        }) | Select-Object -First 1
    if ($null -eq $task -or
        (Get-R0SpecField $task 'status') -ne 'done' -or
        (Get-R0SpecField $task 'assignee') -ne 'r0-architecture-agent' -or
        (Get-R0SpecField $task 'reviewer') -ne 'r0-validation-agent') {
        Add-R0SpecIssue $issues 'R0-SPEC-001 backlog state lacks the accepted assignee/reviewer binding.'
    }
    $yyz = Get-R0SpecField $Context 'yyz_manifest'
    if ('R0-SPEC-001' -in @(Get-R0SpecField $yyz 'open_tasks')) {
        Add-R0SpecIssue $issues 'R0-SPEC-001 is done but remains in the YYZ open_tasks registry.'
    }
    return $issues.ToArray()
}

function Get-R0SpecAcceptanceContext {
    param([string]$RepoRoot)

    $reconciliation = [pscustomobject][ordered]@{
        'RECON-DEC-001' = Read-R0SpecJson (Join-Path $RepoRoot 'docs\governance\reconciliation-dispositions\RECON-DEC-001-2026-08-12.json')
        'RECON-DEC-002' = Read-R0SpecJson (Join-Path $RepoRoot 'docs\governance\reconciliation-dispositions\RECON-DEC-002-2026-08-12.json')
        'RECON-DEC-003' = Read-R0SpecJson (Join-Path $RepoRoot 'docs\governance\reconciliation-dispositions\RECON-DEC-003-2026-08-12.json')
    }
    return [pscustomobject][ordered]@{
        authorization = Read-R0SpecJson (Join-Path $RepoRoot 'docs\governance\r0-owner-authorization.json')
        adr_text = if (Test-Path -LiteralPath (Join-Path $RepoRoot 'docs\adr\0004-r0-json-schema-contracts.md')) {
            Get-Content -LiteralPath (Join-Path $RepoRoot 'docs\adr\0004-r0-json-schema-contracts.md') -Raw -Encoding UTF8
        } else { '' }
        lock = Read-R0SpecJson (Join-Path $RepoRoot 'specs\r0-schema-contract-lock.json')
        adr_disposition = Read-R0SpecJson (Join-Path $RepoRoot 'docs\governance\adr-dispositions\ADR-0004-2026-08-12.json')
        reconciliation_records = $reconciliation
        task_acceptance = Read-R0SpecJson (Join-Path $RepoRoot 'docs\quality\task-acceptance-R0-SPEC-001.json')
        backlog = Read-R0SpecJson (Join-Path $RepoRoot 'docs\tasks\backlog.json')
        yyz_manifest = Read-R0SpecJson (Join-Path $RepoRoot 'fixtures\ref-yyz-001\fixture-manifest.json')
    }
}

function Test-R0SpecAcceptance {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [switch]$RunMutations
    )

    $expectedFileset = $null
    $fatalIssues = [System.Collections.Generic.List[string]]::new()
    try {
        $expectedFileset = Get-R0SpecReviewedFileset `
            -RepoRoot $RepoRoot `
            -Base '611a48a23ea02ecd0c210a2b101f5c5cbf5df0e6' `
            -Commit 'ee7157359e689114d0259a1ae7884a315b029bc1' `
            -DirectParent 'e5c711cd26c735eafb59084724682155356a9f45'
    }
    catch {
        $fatalIssues.Add("R0-SPEC-001 reviewed Git range cannot be reproduced: $($_.Exception.Message)")
        return [pscustomobject][ordered]@{ Issues = $fatalIssues.ToArray(); MutationCount = 0 }
    }

    $context = Get-R0SpecAcceptanceContext -RepoRoot $RepoRoot
    $issues = @(Get-R0SpecAcceptanceIssues -Context $context -ExpectedFileset $expectedFileset)
    $mutationCount = 0
    if ($RunMutations -and $issues.Count -eq 0) {
        $mutations = @(
            [pscustomobject]@{ Name = 'missing-adr-0004-disposition'; Apply = { param($c) $c.adr_disposition = $null } },
            [pscustomobject]@{ Name = 'adr-0004-wrong-reviewed-commit'; Apply = { param($c) $c.adr_disposition.reviewed_commit = '0000000000000000000000000000000000000000' } },
            [pscustomobject]@{ Name = 'adr-0004-wrong-fileset-hash'; Apply = { param($c) $c.adr_disposition.reviewed_fileset.sha256 = '0000000000000000000000000000000000000000000000000000000000000000' } },
            [pscustomobject]@{ Name = 'adr-0004-wrong-owner-actor'; Apply = { param($c) $c.adr_disposition.decision_actor.actor_id = 'r0-po-agent' } },
            [pscustomobject]@{ Name = 'adr-0004-self-review'; Apply = { param($c) $c.adr_disposition.independent_review.actor_id = 'r0-architecture-agent'; $c.adr_disposition.independent_review.task_path = '/root/r0_architecture_agent' } },
            [pscustomobject]@{ Name = 'missing-recon-dec-001'; Apply = { param($c) $c.reconciliation_records.'RECON-DEC-001' = $null } },
            [pscustomobject]@{ Name = 'recon-dec-002-wrong-outcome'; Apply = { param($c) $c.reconciliation_records.'RECON-DEC-002'.outcome = 'external-uri-fallback' } },
            [pscustomobject]@{ Name = 'recon-dec-003-wrong-fileset'; Apply = { param($c) $c.reconciliation_records.'RECON-DEC-003'.reviewed_fileset.path_count = 12 } },
            [pscustomobject]@{ Name = 'reconciliation-self-review'; Apply = { param($c) $c.reconciliation_records.'RECON-DEC-001'.independent_review.actor_id = 'r0-architecture-agent'; $c.reconciliation_records.'RECON-DEC-001'.independent_review.task_path = '/root/r0_architecture_agent' } },
            [pscustomobject]@{ Name = 'missing-r0-spec-task-acceptance'; Apply = { param($c) $c.task_acceptance = $null } },
            [pscustomobject]@{ Name = 'r0-spec-task-wrong-reviewed-commit'; Apply = { param($c) $c.task_acceptance.reviewed_commit = '0000000000000000000000000000000000000000' } },
            [pscustomobject]@{ Name = 'r0-spec-task-wrong-fileset'; Apply = { param($c) $c.task_acceptance.reviewed_fileset.entries[0].sha256 = '0000000000000000000000000000000000000000000000000000000000000000' } },
            [pscustomobject]@{ Name = 'r0-spec-task-self-review'; Apply = { param($c) $c.task_acceptance.independent_reviewer.actor_id = 'r0-architecture-agent'; $c.task_acceptance.independent_reviewer.task_path = '/root/r0_architecture_agent' } },
            [pscustomobject]@{ Name = 'r0-spec-task-failed-ci'; Apply = { param($c) $c.task_acceptance.commit_bound_verification.push_run.jobs[0].conclusion = 'failure' } },
            [pscustomobject]@{ Name = 'r0-spec-task-wrong-pr-merge-object'; Apply = { param($c) $c.task_acceptance.commit_bound_verification.pull_request_run.checked_out_commit_object.sha1 = '0000000000000000000000000000000000000000' } },
            [pscustomobject]@{ Name = 'r0-spec-task-wrong-pr-content-sha256'; Apply = { param($c) $c.task_acceptance.commit_bound_verification.pull_request_run.checked_out_commit_object.content_sha256 = '0000000000000000000000000000000000000000000000000000000000000000' } },
            [pscustomobject]@{ Name = 'r0-spec-task-wrong-pr-checkout-ref'; Apply = { param($c) $c.task_acceptance.commit_bound_verification.pull_request_run.checked_out_ref = 'refs/heads/codex/r0-spec-001' } },
            [pscustomobject]@{ Name = 'r0-spec-task-bogus-reconciliation-refs'; Apply = { param($c) $c.task_acceptance.acceptance_checks.evidence.reconciliation_dispositions = @('bogus/one.json', 'bogus/two.json', 'bogus/three.json') } },
            [pscustomobject]@{ Name = 'r0-spec-task-wrong-work-package-ref'; Apply = { param($c) $c.task_acceptance.acceptance_checks.evidence.work_package = 'docs/tasks/work-packages/R0-GOV-001.md' } },
            [pscustomobject]@{ Name = 'r0-spec-task-wrong-run-url'; Apply = { param($c) $c.task_acceptance.commit_bound_verification.push_run.url = 'https://example.invalid/run' } },
            [pscustomobject]@{ Name = 'r0-spec-task-drops-rights-boundary'; Apply = { param($c) $c.task_acceptance.boundaries.rights_and_external_distribution = 'allowed' } },
            [pscustomobject]@{ Name = 'accepted-adr-with-proposed-lock'; Apply = { param($c) $c.lock.decision_status = 'proposed' } },
            [pscustomobject]@{ Name = 'done-task-remains-open'; Apply = { param($c) $c.yyz_manifest.open_tasks += 'R0-SPEC-001' } },
            [pscustomobject]@{ Name = 'done-task-missing-reviewer'; Apply = { param($c) (@($c.backlog.tasks | Where-Object { $_.id -eq 'R0-SPEC-001' })[0]).reviewer = $null } },
            [pscustomobject]@{ Name = 'accepted-task-reverted-to-review'; Apply = { param($c) (@($c.backlog.tasks | Where-Object { $_.id -eq 'R0-SPEC-001' })[0]).status = 'review' } }
        )
        foreach ($mutation in $mutations) {
            ++$mutationCount
            $mutated = Get-R0SpecAcceptanceContext -RepoRoot $RepoRoot
            & $mutation.Apply $mutated
            $mutationIssues = @(Get-R0SpecAcceptanceIssues -Context $mutated -ExpectedFileset $expectedFileset)
            if ($mutationIssues.Count -eq 0) {
                $fatalIssues.Add("R0-SPEC-001 acceptance mutation was not rejected: $($mutation.Name)")
            }
        }
    }
    foreach ($issue in $issues) { $fatalIssues.Add([string]$issue) }
    return [pscustomobject][ordered]@{
        Issues = $fatalIssues.ToArray()
        MutationCount = $mutationCount
    }
}

Export-ModuleMember -Function Test-R0SpecAcceptance

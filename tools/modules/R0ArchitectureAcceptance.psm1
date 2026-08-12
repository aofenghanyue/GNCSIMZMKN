Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-R0ArchitectureAcceptanceField {
    param($Object, [string]$Name)

    if ($null -eq $Object) { return $null }
    foreach ($property in @($Object.PSObject.Properties)) {
        if ([string]::Equals($property.Name, $Name, [System.StringComparison]::Ordinal)) {
            return $property.Value
        }
    }
    return $null
}

function Read-R0ArchitectureAcceptanceJson {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    try {
        return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    catch {
        return $null
    }
}

function Copy-R0ArchitectureAcceptanceObject {
    param($Object)

    if ($null -eq $Object) { return $null }
    return $Object | ConvertTo-Json -Depth 100 | ConvertFrom-Json
}

function ConvertTo-R0ArchitectureAcceptanceCanonicalJson {
    param($Value)

    if ($null -eq $Value) { return 'null' }
    if ($Value -is [string] -or $Value -is [char] -or
        $Value -is [bool] -or $Value.GetType().IsPrimitive -or
        $Value -is [decimal]) {
        return [string]($Value | ConvertTo-Json -Compress)
    }
    if ($Value -is [System.Collections.IDictionary]) {
        [string[]]$keys = @($Value.Keys | ForEach-Object { [string]$_ })
        [System.Array]::Sort($keys, [System.StringComparer]::Ordinal)
        $parts = [System.Collections.Generic.List[string]]::new()
        foreach ($key in $keys) {
            $encodedKey = [string]($key | ConvertTo-Json -Compress)
            $encodedValue = ConvertTo-R0ArchitectureAcceptanceCanonicalJson $Value[$key]
            $parts.Add("$encodedKey`:$encodedValue")
        }
        return '{' + [string]::Join(',', $parts) + '}'
    }
    if ($Value -is [System.Collections.IEnumerable]) {
        $parts = [System.Collections.Generic.List[string]]::new()
        foreach ($item in $Value) {
            $parts.Add((ConvertTo-R0ArchitectureAcceptanceCanonicalJson $item))
        }
        return '[' + [string]::Join(',', $parts) + ']'
    }
    [string[]]$propertyNames = @($Value.PSObject.Properties.Name)
    [System.Array]::Sort($propertyNames, [System.StringComparer]::Ordinal)
    $parts = [System.Collections.Generic.List[string]]::new()
    foreach ($name in $propertyNames) {
        $encodedName = [string]($name | ConvertTo-Json -Compress)
        $encodedValue = ConvertTo-R0ArchitectureAcceptanceCanonicalJson (
            Get-R0ArchitectureAcceptanceField $Value $name)
        $parts.Add("$encodedName`:$encodedValue")
    }
    return '{' + [string]::Join(',', $parts) + '}'
}

function Test-R0ArchitectureAcceptanceExactValue {
    param(
        $Actual,
        $Expected,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    $actualCanonical = ConvertTo-R0ArchitectureAcceptanceCanonicalJson $Actual
    $expectedCanonical = ConvertTo-R0ArchitectureAcceptanceCanonicalJson $Expected
    if ($actualCanonical -cne $expectedCanonical) {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label is not the exact accepted value."
    }
}

function Test-R0ArchitectureAcceptanceTextContract {
    param(
        [string]$Text,
        [string[]]$RequiredFragments,
        [string[]]$ForbiddenFragments,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    foreach ($fragment in $RequiredFragments) {
        if (-not $Text.Contains($fragment)) {
            Add-R0ArchitectureAcceptanceIssue $Issues "$Label is missing required accepted-state text '$fragment'."
        }
    }
    foreach ($fragment in $ForbiddenFragments) {
        if ($Text.Contains($fragment)) {
            Add-R0ArchitectureAcceptanceIssue $Issues "$Label retains stale text '$fragment'."
        }
    }
}

function Get-R0ArchitectureAcceptanceSha256Hex {
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

function Get-R0ArchitectureAcceptanceSha1Hex {
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

function Get-R0ArchitectureAcceptanceGitBlobFact {
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
            sha256 = Get-R0ArchitectureAcceptanceSha256Hex $bytes
        }
    }
    finally {
        $memory.Dispose()
        $process.Dispose()
    }
}

function Get-R0ArchitectureAcceptanceReviewedFileset {
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
        $entry = Get-R0ArchitectureAcceptanceGitBlobFact -RepoRoot $RepoRoot -Commit $Commit -Path $path
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
        sha256 = Get-R0ArchitectureAcceptanceSha256Hex $manifestBytes
        entries = @($entries)
    }
}

function Get-R0ArchitectureAcceptanceRetainedCommitFact {
    param($Object)

    if ((Get-R0ArchitectureAcceptanceField $Object 'object_type') -ne 'commit' -or
        (Get-R0ArchitectureAcceptanceField $Object 'content_encoding') -ne 'base64') {
        throw 'Retained Git object type or encoding is invalid.'
    }
    try {
        [byte[]]$content = [System.Convert]::FromBase64String(
            [string](Get-R0ArchitectureAcceptanceField $Object 'content_base64'))
    }
    catch {
        throw "Retained Git commit base64 is invalid: $($_.Exception.Message)"
    }
    if ($content.Length -ne (Get-R0ArchitectureAcceptanceField $Object 'byte_length')) {
        throw 'Retained Git commit byte length is incorrect.'
    }
    $contentSha256 = Get-R0ArchitectureAcceptanceSha256Hex $content
    if ($contentSha256 -ne (Get-R0ArchitectureAcceptanceField $Object 'content_sha256')) {
        throw 'Retained Git commit content SHA-256 is incorrect.'
    }
    [byte[]]$header = [System.Text.Encoding]::ASCII.GetBytes("commit $($content.Length)`0")
    [byte[]]$fullObject = New-Object byte[] ($header.Length + $content.Length)
    [System.Array]::Copy($header, 0, $fullObject, 0, $header.Length)
    [System.Array]::Copy($content, 0, $fullObject, $header.Length, $content.Length)
    $sha1 = Get-R0ArchitectureAcceptanceSha1Hex $fullObject
    if ($sha1 -ne (Get-R0ArchitectureAcceptanceField $Object 'sha1')) {
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

function Add-R0ArchitectureAcceptanceIssue {
    param(
        [System.Collections.Generic.List[string]]$Issues,
        [string]$Message
    )

    $Issues.Add($Message)
}

function Test-R0ArchitectureAcceptanceRecordedFileset {
    param(
        $Recorded,
        $Expected,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    if ($null -eq $Recorded) {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label has no reviewed fileset."
        return
    }
    foreach ($field in @(
            'algorithm_id', 'scope', 'path_order', 'entry_format',
            'manifest_encoding', 'line_ending', 'path_count', 'sha256')) {
        if ((Get-R0ArchitectureAcceptanceField $Recorded $field) -ne (Get-R0ArchitectureAcceptanceField $Expected $field)) {
            Add-R0ArchitectureAcceptanceIssue $Issues "$Label fileset field '$field' is incorrect."
        }
    }
    $recordedEntries = @(Get-R0ArchitectureAcceptanceField $Recorded 'entries')
    $expectedEntries = @(Get-R0ArchitectureAcceptanceField $Expected 'entries')
    if ($recordedEntries.Count -ne $expectedEntries.Count) {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label fileset entry count is incorrect."
        return
    }
    for ($index = 0; $index -lt $expectedEntries.Count; ++$index) {
        foreach ($field in @('path', 'byte_length', 'sha256')) {
            if ((Get-R0ArchitectureAcceptanceField $recordedEntries[$index] $field) -ne
                (Get-R0ArchitectureAcceptanceField $expectedEntries[$index] $field)) {
                Add-R0ArchitectureAcceptanceIssue $Issues "$Label fileset entry $index field '$field' is incorrect."
            }
        }
    }
}

function Get-R0ArchitectureAcceptanceActorMap {
    param($Authorization)

    $map = @{}
    foreach ($actor in @(Get-R0ArchitectureAcceptanceField $Authorization 'actors')) {
        $id = [string](Get-R0ArchitectureAcceptanceField $actor 'id')
        if (-not [string]::IsNullOrWhiteSpace($id) -and -not $map.ContainsKey($id)) {
            $map[$id] = $actor
        }
    }
    return $map
}

function Test-R0ArchitectureAcceptanceActorRecord {
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

    $actorId = [string](Get-R0ArchitectureAcceptanceField $Record 'actor_id')
    $taskPath = [string](Get-R0ArchitectureAcceptanceField $Record 'task_path')
    $roles = @(Get-R0ArchitectureAcceptanceField $Record 'roles')
    if ($actorId -ne $ExpectedActor -or
        (Get-R0ArchitectureAcceptanceField $Record 'kind') -ne 'machine_agent' -or
        $taskPath -ne $ExpectedTask -or
        ((@($roles | Sort-Object) -join ',') -ne (@($ExpectedRoles | Sort-Object) -join ',')) -or
        (Get-R0ArchitectureAcceptanceField $Record 'decision') -ne $ExpectedDecision -or
        -not $ActorMap.ContainsKey($ExpectedActor)) {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label actor identity, task, roles or decision is invalid."
        return
    }
    $authorized = $ActorMap[$ExpectedActor]
    if ((Get-R0ArchitectureAcceptanceField $authorized 'kind') -ne 'machine_agent' -or
        (Get-R0ArchitectureAcceptanceField $authorized 'task_path') -ne $ExpectedTask) {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label actor does not match the owner authorization."
    }
    foreach ($role in $ExpectedRoles) {
        if ($role -notin @(Get-R0ArchitectureAcceptanceField $authorized 'authorized_roles')) {
            Add-R0ArchitectureAcceptanceIssue $Issues "$Label actor is not authorized for role '$role'."
        }
    }
}

function Test-R0ArchitectureAcceptanceIndependentReview {
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

    $reviewActor = [string](Get-R0ArchitectureAcceptanceField $Review 'actor_id')
    $reviewTask = [string](Get-R0ArchitectureAcceptanceField $Review 'task_path')
    if ($reviewActor -ne 'r0-validation-agent' -or
        (Get-R0ArchitectureAcceptanceField $Review 'kind') -ne 'machine_agent' -or
        $reviewTask -ne '/root/r0_validation_agent' -or
        (Get-R0ArchitectureAcceptanceField $Review 'role') -ne 'validation_lead' -or
        (Get-R0ArchitectureAcceptanceField $Review 'result') -ne 'approved' -or
        (Get-R0ArchitectureAcceptanceField $Review 'decision') -ne 'accept-as-written' -or
        (Get-R0ArchitectureAcceptanceField $Review 'reviewed_commit') -ne $ReviewedCommit -or
        (Get-R0ArchitectureAcceptanceField $Review 'reviewed_fileset_sha256') -ne $ReviewedFilesetSha256 -or
        $reviewActor -eq $DecisionActorId -or $reviewTask -eq $DecisionTaskPath -or
        -not $ActorMap.ContainsKey($reviewActor)) {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label lacks the required independent Validation approval."
        return
    }
    $authorized = $ActorMap[$reviewActor]
    if ((Get-R0ArchitectureAcceptanceField $authorized 'task_path') -ne $reviewTask -or
        'validation_lead' -notin @(Get-R0ArchitectureAcceptanceField $authorized 'authorized_roles')) {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label reviewer does not resolve through owner authorization."
    }
}

function Test-R0ArchitectureAcceptanceCiJobs {
    param(
        $Jobs,
        $ExpectedJobs,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    $actualJobs = @($Jobs)
    if ($actualJobs.Count -ne $ExpectedJobs.Count) {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label must contain both fixed CI profile jobs."
        return
    }
    foreach ($expected in $ExpectedJobs) {
        $job = @($actualJobs | Where-Object {
                [string](Get-R0ArchitectureAcceptanceField $_ 'job_id') -eq $expected.job_id
            }) | Select-Object -First 1
        if ($null -eq $job) {
            Add-R0ArchitectureAcceptanceIssue $Issues "$Label job '$($expected.job_id)' is missing."
            continue
        }
        foreach ($field in @(
                'name', 'runner_name', 'runner_group', 'runner_os', 'runner_arch',
                'runner_image', 'runner_image_version', 'cmake_version',
                'compiler_id', 'compiler_version', 'conclusion', 'url')) {
            if ((Get-R0ArchitectureAcceptanceField $job $field) -ne $expected.$field) {
                Add-R0ArchitectureAcceptanceIssue $Issues "$Label job '$($expected.job_id)' field '$field' is invalid."
            }
        }
        $steps = Get-R0ArchitectureAcceptanceField $job 'required_step_conclusions'
        foreach ($step in @(
                'Check out repository', 'Record runner and tool identity',
                'Record configured compiler identity', 'Build', 'Test', 'Repository checks')) {
            if ((Get-R0ArchitectureAcceptanceField $steps $step) -ne 'success') {
                Add-R0ArchitectureAcceptanceIssue $Issues "$Label job '$($expected.job_id)' required step '$step' did not succeed."
            }
        }
        if ($expected.runner_os -eq 'Linux') {
            if ((Get-R0ArchitectureAcceptanceField $steps 'Configure Ubuntu candidate') -ne 'success' -or
                (Get-R0ArchitectureAcceptanceField $steps 'Configure Windows candidate') -ne 'skipped' -or
                (Get-R0ArchitectureAcceptanceField $steps 'Verify Windows PowerShell 5.1 compatibility') -ne 'skipped') {
                Add-R0ArchitectureAcceptanceIssue $Issues "$Label Ubuntu platform steps are invalid."
            }
        }
        else {
            if ((Get-R0ArchitectureAcceptanceField $steps 'Configure Ubuntu candidate') -ne 'skipped' -or
                (Get-R0ArchitectureAcceptanceField $steps 'Configure Windows candidate') -ne 'success' -or
                (Get-R0ArchitectureAcceptanceField $steps 'Verify Windows PowerShell 5.1 compatibility') -ne 'success') {
                Add-R0ArchitectureAcceptanceIssue $Issues "$Label Windows platform steps are invalid."
            }
        }
    }
}

function Test-R0ArchitectureAcceptanceCore {
    param(
        $Record,
        $ExpectedFileset,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    if ($null -eq $Record) {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label is missing."
        return $false
    }
    if ((Get-R0ArchitectureAcceptanceField $Record 'reviewed_base') -ne
            'e0bba2b99e96a2a6ded646302b1d1424d323c362' -or
        (Get-R0ArchitectureAcceptanceField $Record 'reviewed_commit') -ne
            '29f455efebd72113c1d311bc674a78c638265f34' -or
        (Get-R0ArchitectureAcceptanceField $Record 'reviewed_direct_parent') -ne
            '4aee5e8901e2afd8df38ccfc14cf0de04400cba0' -or
        (Get-R0ArchitectureAcceptanceField $Record 'reviewed_target_tree') -ne
            'f4649afaf194935ada70920fdcee140fdf4bf545' -or
        (Get-R0ArchitectureAcceptanceField $Record 'authorization_ref') -ne
            'docs/governance/r0-owner-authorization.json') {
        Add-R0ArchitectureAcceptanceIssue $Issues "$Label reviewed Git range, tree or authorization is invalid."
    }
    Test-R0ArchitectureAcceptanceRecordedFileset -Recorded (
        Get-R0ArchitectureAcceptanceField $Record 'reviewed_fileset'
    ) -Expected $ExpectedFileset -Label $Label -Issues $Issues
    return $true
}

function Test-R0ArchitectureAcceptanceBoundaries {
    param(
        $Record,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    $boundaries = Get-R0ArchitectureAcceptanceField $Record 'boundaries'
    $expected = [pscustomobject][ordered]@{
        governance_maturity = 'accepted-r0-authority-baseline'
        source_scope = 'listed-authority-source-set-only'
        runtime_consumers = 0
        full_repository_fitness = 'remains R0-ARCH-002'
        rights_and_external_distribution = 'remain fail-closed'
        scientific_qualification = 'not established'
        legacy = 'reference/legacy remains read-only and evidence-only'
        r0_gate = 'R0-GATE-001 remains planned; G0 and G1 are not passed'
        r1_through_r8 = 'remain locked'
    }
    Test-R0ArchitectureAcceptanceExactValue -Actual $boundaries -Expected $expected `
        -Label "$Label boundaries" -Issues $Issues
}

function Get-R0ArchitectureAcceptanceIssues {
    param(
        $Context,
        $ExpectedFileset
    )

    $issues = [System.Collections.Generic.List[string]]::new()
    $expectedBase = 'e0bba2b99e96a2a6ded646302b1d1424d323c362'
    $expectedCommit = '29f455efebd72113c1d311bc674a78c638265f34'
    $expectedParent = '4aee5e8901e2afd8df38ccfc14cf0de04400cba0'
    $expectedTree = 'f4649afaf194935ada70920fdcee140fdf4bf545'
    $authorization = Get-R0ArchitectureAcceptanceField $Context 'authorization'
    $actorMap = Get-R0ArchitectureAcceptanceActorMap $authorization
    if ((Get-R0ArchitectureAcceptanceField $authorization 'authorization_id') -ne
            'R0-OWNER-AUTH-2026-08-12' -or
        (Get-R0ArchitectureAcceptanceField $authorization 'authorization_status') -ne 'active') {
        Add-R0ArchitectureAcceptanceIssue $issues 'R0-ARCH-001 acceptance lacks active owner authorization.'
    }

    $adrText = [string](Get-R0ArchitectureAcceptanceField $Context 'adr_text')
    if ($adrText -notmatch '(?m)^- Status: Accepted$' -or
        -not $adrText.Contains('../governance/adr-dispositions/ADR-0005-2026-08-12.json') -or
        -not $adrText.Contains('../governance/reconciliation-dispositions/RECON-DEC-006-2026-08-12.json') -or
        -not $adrText.Contains('../governance/reconciliation-dispositions/RECON-DEC-007-2026-08-12.json') -or
        $adrText.Contains('amendment remains a review candidate')) {
        Add-R0ArchitectureAcceptanceIssue $issues 'ADR-0005 status or accepted disposition references are inconsistent.'
    }

    $reviewContract = Get-R0ArchitectureAcceptanceField $Context 'review_contract'
    $contractDecisions = Get-R0ArchitectureAcceptanceField $reviewContract 'decisions'
    $physicalDag = Get-R0ArchitectureAcceptanceField $reviewContract 'physical_dag'
    $legacyPolicy = Get-R0ArchitectureAcceptanceField $reviewContract 'legacy_reconciliation'
    $runtimePolicy = Get-R0ArchitectureAcceptanceField $reviewContract 'runtime_consumer_policy'
    if ((Get-R0ArchitectureAcceptanceField $reviewContract 'schema_version') -ne
            'gnczmkn.r0-architecture-review-contract/1' -or
        (Get-R0ArchitectureAcceptanceField $reviewContract 'reviewed_base') -ne $expectedBase -or
        (Get-R0ArchitectureAcceptanceField $contractDecisions 'RECON-DEC-006') -ne
            'logical-only-keep-current' -or
        (Get-R0ArchitectureAcceptanceField $contractDecisions 'RECON-DEC-007') -ne
            'keep-current-22-owner-consumer-map' -or
        @(Get-R0ArchitectureAcceptanceField $physicalDag 'module_names').Count -ne 9 -or
        @(Get-R0ArchitectureAcceptanceField $reviewContract 'logical_boundaries').Count -ne 2 -or
        (Get-R0ArchitectureAcceptanceField $legacyPolicy 'authority_mapping_count') -ne 22 -or
        (Get-R0ArchitectureAcceptanceField $legacyPolicy 'classified_responsibility_count') -ne 33 -or
        (Get-R0ArchitectureAcceptanceField $runtimePolicy 'allowed_consumer_count') -ne 0) {
        Add-R0ArchitectureAcceptanceIssue $issues 'Architecture technical review contract is inconsistent.'
    }

    $baseline = Get-R0ArchitectureAcceptanceField $Context 'baseline'
    $report = Get-R0ArchitectureAcceptanceField $Context 'report'
    $reportScope = Get-R0ArchitectureAcceptanceField $report 'scope'
    if ((Get-R0ArchitectureAcceptanceField $baseline 'generation_policy') -ne
            'deterministic-derived-no-runtime-consumer' -or
        (Get-R0ArchitectureAcceptanceField $report 'status') -ne 'conformant' -or
        (Get-R0ArchitectureAcceptanceField $reportScope 'claim') -ne
            'listed-authority-source-set-only' -or
        (Get-R0ArchitectureAcceptanceField $reportScope 'runtime_consumer_count') -ne 0) {
        Add-R0ArchitectureAcceptanceIssue $issues 'Architecture baseline or conformance report boundary is inconsistent.'
    }

    $expectedTechnicalVerification = [pscustomobject][ordered]@{
        architecture_validator = 'passed; 276 terms; 20 aliases; 10 capabilities; 27 shared symbols; 22 Legacy mappings; 9 modules; 22 CMake edges; 15/15 baseline mutations; 35/35 review-contract mutations'
        ctest = 'passed; 9/9'
        repository_verifier = 'passed; 66 JSON; 65 tasks; 100 Markdown'
        provenance_validator = 'passed; 14/14 mutations; 8/8 NOASSERTION'
        diff_check = 'passed'
        strict_utf8_json_and_wording = 'passed; 15/15 reviewed paths'
        hosted_ci = [pscustomobject][ordered]@{
            push_run_id = '31572060238'
            pull_request_run_id = '31572064035'
            ubuntu = 'success on push and pull_request'
            windows = 'success on push and pull_request'
        }
    }
    $expectedAdrRationale = @(
        'The reviewed slice deterministically derives the terminology, module dependency and Legacy ownership baseline from the listed authority source set.',
        'Nine physical modules, two logical-only boundary labels and twenty-two Legacy ownership rows remain exact and machine guarded.',
        'Thirty-three candidate responsibilities are completely classified without adding an unapproved ownership overlay.',
        'Architecture ownership and independent Validation reproduced the Git fileset, failure paths, local verification and both hosted CI contexts.'
    )
    $expectedDec006Rationale = @(
        'ADR-0003/1 remains the sole physical module DAG authority.',
        'packages_user and composition_root describe source contribution and composition rules without becoming module, owner or CMake identities.'
    )
    $expectedDec007Rationale = @(
        'The accepted registry keeps the twenty-two current primary-owner and secondary-consumer rows exact.',
        'The thirty-three candidate responsibilities are completely classified as twenty-two aligned, three deferred owner splits, two logical routes and six responsibilities across five unregistered names.'
    )
    $expectedDec006Contract = [pscustomobject][ordered]@{
        contract_id = 'logical-boundaries-outside-physical-dag'
        physical_policy = Get-R0ArchitectureAcceptanceField $reviewContract 'physical_dag'
        logical_boundaries = Get-R0ArchitectureAcceptanceField $reviewContract 'logical_boundaries'
    }
    $legacyReconciliation = Get-R0ArchitectureAcceptanceField $reviewContract 'legacy_reconciliation'
    $expectedDec007Contract = [pscustomobject][ordered]@{
        contract_id = 'current-22-owner-consumer-map'
        authority_mapping_count = Get-R0ArchitectureAcceptanceField $legacyReconciliation 'authority_mapping_count'
        classified_responsibility_count = Get-R0ArchitectureAcceptanceField $legacyReconciliation 'classified_responsibility_count'
        classification_counts = [pscustomobject][ordered]@{
            aligned = 22
            deferred_owner_split = 3
            logical_route = 2
            unregistered_responsibility = 6
        }
        aligned_responsibility_ids = Get-R0ArchitectureAcceptanceField $legacyReconciliation 'aligned_responsibility_ids'
        deferred_owner_split_ids = Get-R0ArchitectureAcceptanceField $legacyReconciliation 'deferred_owner_split_ids'
        logical_route_ids = Get-R0ArchitectureAcceptanceField $legacyReconciliation 'logical_route_ids'
        unregistered_legacy_names = Get-R0ArchitectureAcceptanceField $legacyReconciliation 'unregistered_legacy_names'
        unregistered_responsibility_ids = Get-R0ArchitectureAcceptanceField $legacyReconciliation 'unregistered_responsibility_ids'
        responsibility_overlay_forbidden_in_v1 = Get-R0ArchitectureAcceptanceField $legacyReconciliation 'responsibility_overlay_forbidden_in_v1'
        glossary_migration_and_superseding_adr_required_for_change = Get-R0ArchitectureAcceptanceField $legacyReconciliation 'glossary_migration_and_superseding_adr_required_for_change'
    }
    $expectedDec006Mutations = @(
        'logical-boundary-physical-promotion',
        'packages-user-physical-module',
        'composition-root-physical-module',
        'coordinated-dag-change-without-superseding-snapshot',
        'cmake-unknown-module-dependency',
        'runtime-consumer'
    )
    $expectedDec007Mutations = @(
        'legacy-primary-owner-drift',
        'legacy-disposition-drift',
        'legacy-secondary-consumer-drift',
        'legacy-reconciliation-outcome-drift',
        'coordinated-new-legacy-mapping-without-superseding-snapshot',
        'shared-symbol-owner-drift',
        'registry-duplicate-json-key'
    )
    $expectedDec006Migration = [pscustomobject][ordered]@{
        rule = 'A physical module or machine-readable logical-boundary field change requires a superseding ADR, registry schema version and new accepted authority snapshot.'
    }
    $expectedDec007Migration = [pscustomobject][ordered]@{
        rule = 'A new Legacy mapping or finer ownership row first requires a glossary section 9 migration row, owner evidence, a superseding ADR and a registry schema version, followed by a new accepted snapshot.'
    }
    $expectedDec006Boundaries = @(
        'no-physical-promotion',
        'compiler-does-not-read-package-implementation',
        'no-runtime-consumer',
        'no-rights-gate-or-r1-unlock'
    )
    $expectedDec007Boundaries = @(
        'no-responsibility-overlay-in-v1',
        'no-unregistered-legacy-name',
        'no-runtime-consumer',
        'no-rights-gate-or-r1-unlock'
    )

    $adr = Get-R0ArchitectureAcceptanceField $Context 'adr_disposition'
    if (Test-R0ArchitectureAcceptanceCore -Record $adr -ExpectedFileset $ExpectedFileset -Label 'ADR-0005 disposition' -Issues $issues) {
        if ((Get-R0ArchitectureAcceptanceField $adr 'schema_version') -ne 'gnczmkn.adr-disposition/1' -or
            (Get-R0ArchitectureAcceptanceField $adr 'adr_id') -ne 'ADR-0005' -or
            (Get-R0ArchitectureAcceptanceField $adr 'decision') -ne 'accept-as-written' -or
            (Get-R0ArchitectureAcceptanceField $adr 'decision_date') -ne '2026-08-12') {
            Add-R0ArchitectureAcceptanceIssue $issues 'ADR-0005 disposition identity or decision is invalid.'
        }
        $implementation = Get-R0ArchitectureAcceptanceField $adr 'implementation_actor'
        $decisionActor = Get-R0ArchitectureAcceptanceField $adr 'decision_actor'
        Test-R0ArchitectureAcceptanceActorRecord -Record $implementation -ExpectedActor 'r0-po-agent' -ExpectedTask '/root' -ExpectedRoles @('product_owner') -ExpectedDecision 'implemented' -ActorMap $actorMap -Label 'ADR-0005 implementation' -Issues $issues
        Test-R0ArchitectureAcceptanceActorRecord -Record $decisionActor -ExpectedActor 'r0-architecture-agent' -ExpectedTask '/root/r0_architecture_agent' -ExpectedRoles @('architecture_lead','compiler_lead') -ExpectedDecision 'accept-as-written' -ActorMap $actorMap -Label 'ADR-0005 owner' -Issues $issues
        Test-R0ArchitectureAcceptanceIndependentReview -Review (
            Get-R0ArchitectureAcceptanceField $adr 'independent_review'
        ) -ActorMap $actorMap -ReviewedCommit $expectedCommit -ReviewedFilesetSha256 $ExpectedFileset.sha256 -DecisionActorId 'r0-architecture-agent' -DecisionTaskPath '/root/r0_architecture_agent' -Label 'ADR-0005 disposition' -Issues $issues
        $expectedRefs = @(
            'docs/governance/reconciliation-dispositions/RECON-DEC-006-2026-08-12.json',
            'docs/governance/reconciliation-dispositions/RECON-DEC-007-2026-08-12.json')
        if (((@(Get-R0ArchitectureAcceptanceField $adr 'reconciliation_disposition_refs')) -join ',') -ne
            ($expectedRefs -join ',')) {
            Add-R0ArchitectureAcceptanceIssue $issues 'ADR-0005 reconciliation disposition references are invalid.'
        }
        Test-R0ArchitectureAcceptanceExactValue -Actual (
            Get-R0ArchitectureAcceptanceField $adr 'rationale') -Expected $expectedAdrRationale `
            -Label 'ADR-0005 rationale' -Issues $issues
        Test-R0ArchitectureAcceptanceExactValue -Actual (
            Get-R0ArchitectureAcceptanceField $adr 'verification') -Expected $expectedTechnicalVerification `
            -Label 'ADR-0005 verification' -Issues $issues
        Test-R0ArchitectureAcceptanceBoundaries -Record $adr -Label 'ADR-0005 disposition' -Issues $issues
    }

    $records = Get-R0ArchitectureAcceptanceField $Context 'reconciliation_records'
    foreach ($id in @('RECON-DEC-006','RECON-DEC-007')) {
        $record = Get-R0ArchitectureAcceptanceField $records $id
        if (-not (Test-R0ArchitectureAcceptanceCore -Record $record -ExpectedFileset $ExpectedFileset -Label $id -Issues $issues)) {
            continue
        }
        $expectedOutcome = if ($id -eq 'RECON-DEC-006') {
            'logical-only-keep-current'
        }
        else {
            'keep-current-22-owner-consumer-map'
        }
        if ((Get-R0ArchitectureAcceptanceField $record 'schema_version') -ne
                'gnczmkn.reconciliation-disposition/1' -or
            (Get-R0ArchitectureAcceptanceField $record 'decision_id') -ne $id -or
            (Get-R0ArchitectureAcceptanceField $record 'outcome') -ne $expectedOutcome -or
            (Get-R0ArchitectureAcceptanceField $record 'status') -ne 'accepted' -or
            (Get-R0ArchitectureAcceptanceField $record 'decided_on') -ne '2026-08-12') {
            Add-R0ArchitectureAcceptanceIssue $issues "$id identity, outcome or status is invalid."
        }
        $decisionActor = Get-R0ArchitectureAcceptanceField $record 'decision_actor'
        Test-R0ArchitectureAcceptanceActorRecord -Record $decisionActor -ExpectedActor 'r0-architecture-agent' -ExpectedTask '/root/r0_architecture_agent' -ExpectedRoles @('architecture_lead','compiler_lead') -ExpectedDecision 'accept-as-written' -ActorMap $actorMap -Label "$id owner" -Issues $issues
        Test-R0ArchitectureAcceptanceIndependentReview -Review (
            Get-R0ArchitectureAcceptanceField $record 'independent_review'
        ) -ActorMap $actorMap -ReviewedCommit $expectedCommit -ReviewedFilesetSha256 $ExpectedFileset.sha256 -DecisionActorId 'r0-architecture-agent' -DecisionTaskPath '/root/r0_architecture_agent' -Label $id -Issues $issues
        $expectedRationale = if ($id -eq 'RECON-DEC-006') {
            $expectedDec006Rationale
        } else { $expectedDec007Rationale }
        $expectedContract = if ($id -eq 'RECON-DEC-006') {
            $expectedDec006Contract
        } else { $expectedDec007Contract }
        $expectedMutations = if ($id -eq 'RECON-DEC-006') {
            $expectedDec006Mutations
        } else { $expectedDec007Mutations }
        $expectedMigration = if ($id -eq 'RECON-DEC-006') {
            $expectedDec006Migration
        } else { $expectedDec007Migration }
        $expectedBoundaries = if ($id -eq 'RECON-DEC-006') {
            $expectedDec006Boundaries
        } else { $expectedDec007Boundaries }
        Test-R0ArchitectureAcceptanceExactValue -Actual (
            Get-R0ArchitectureAcceptanceField $record 'rationale') -Expected $expectedRationale `
            -Label "$id rationale" -Issues $issues
        Test-R0ArchitectureAcceptanceExactValue -Actual (
            Get-R0ArchitectureAcceptanceField $record 'contract') -Expected $expectedContract `
            -Label "$id contract" -Issues $issues
        Test-R0ArchitectureAcceptanceExactValue -Actual (
            Get-R0ArchitectureAcceptanceField $record 'required_mutations') -Expected $expectedMutations `
            -Label "$id required mutations" -Issues $issues
        Test-R0ArchitectureAcceptanceExactValue -Actual (
            Get-R0ArchitectureAcceptanceField $record 'migration') -Expected $expectedMigration `
            -Label "$id migration" -Issues $issues
        Test-R0ArchitectureAcceptanceExactValue -Actual (
            Get-R0ArchitectureAcceptanceField $record 'boundaries') -Expected $expectedBoundaries `
            -Label "$id boundaries" -Issues $issues
        Test-R0ArchitectureAcceptanceExactValue -Actual (
            Get-R0ArchitectureAcceptanceField $record 'verification') -Expected $expectedTechnicalVerification `
            -Label "$id verification" -Issues $issues
    }

    $acceptance = Get-R0ArchitectureAcceptanceField $Context 'task_acceptance'
    if (Test-R0ArchitectureAcceptanceCore -Record $acceptance -ExpectedFileset $ExpectedFileset -Label 'R0-ARCH-001 task acceptance' -Issues $issues) {
        if ((Get-R0ArchitectureAcceptanceField $acceptance 'schema_version') -ne 'gnczmkn.task-acceptance/1' -or
            (Get-R0ArchitectureAcceptanceField $acceptance 'acceptance_id') -ne
                'R0-ARCH-001-ACCEPTANCE-2026-08-12' -or
            (Get-R0ArchitectureAcceptanceField $acceptance 'task_id') -ne 'R0-ARCH-001' -or
            (Get-R0ArchitectureAcceptanceField $acceptance 'result') -ne 'accepted' -or
            (Get-R0ArchitectureAcceptanceField $acceptance 'accepted_on') -ne '2026-08-12') {
            Add-R0ArchitectureAcceptanceIssue $issues 'R0-ARCH-001 task acceptance identity or result is invalid.'
        }
        $implementation = Get-R0ArchitectureAcceptanceField $acceptance 'implementation_actor'
        $decisionOwner = Get-R0ArchitectureAcceptanceField $acceptance 'decision_owner'
        Test-R0ArchitectureAcceptanceActorRecord -Record $implementation -ExpectedActor 'r0-po-agent' -ExpectedTask '/root' -ExpectedRoles @('product_owner') -ExpectedDecision 'implemented' -ActorMap $actorMap -Label 'R0-ARCH-001 implementation' -Issues $issues
        Test-R0ArchitectureAcceptanceActorRecord -Record $decisionOwner -ExpectedActor 'r0-architecture-agent' -ExpectedTask '/root/r0_architecture_agent' -ExpectedRoles @('architecture_lead','compiler_lead') -ExpectedDecision 'accept-as-written' -ActorMap $actorMap -Label 'R0-ARCH-001 owner' -Issues $issues
        Test-R0ArchitectureAcceptanceIndependentReview -Review (
            Get-R0ArchitectureAcceptanceField $acceptance 'independent_reviewer'
        ) -ActorMap $actorMap -ReviewedCommit $expectedCommit -ReviewedFilesetSha256 $ExpectedFileset.sha256 -DecisionActorId 'r0-architecture-agent' -DecisionTaskPath '/root/r0_architecture_agent' -Label 'R0-ARCH-001 task acceptance' -Issues $issues

        $checks = Get-R0ArchitectureAcceptanceField $acceptance 'acceptance_checks'
        $deliverables = Get-R0ArchitectureAcceptanceField $checks 'deliverables'
        $criterion = Get-R0ArchitectureAcceptanceField $checks 'acceptance'
        $evidence = Get-R0ArchitectureAcceptanceField $checks 'evidence'
        if ((Get-R0ArchitectureAcceptanceField $deliverables 'machine_readable_terminology_baseline') -ne 'passed' -or
            (Get-R0ArchitectureAcceptanceField $deliverables 'module_dependency_map') -ne 'passed' -or
            (Get-R0ArchitectureAcceptanceField $deliverables 'legacy_to_target_ownership_map') -ne 'passed' -or
            (Get-R0ArchitectureAcceptanceField $criterion 'shared_terms_enums_keys_and_owners_have_one_authority') -ne 'passed' -or
            (Get-R0ArchitectureAcceptanceField $evidence 'review_contract') -ne
                'docs/architecture/r0-architecture-review-contract.json' -or
            (Get-R0ArchitectureAcceptanceField $evidence 'architecture_baseline') -ne
                'docs/architecture/architecture-baseline.json' -or
            (Get-R0ArchitectureAcceptanceField $evidence 'terminology_report') -ne
                'docs/quality/terminology-conformance-report.json' -or
            (Get-R0ArchitectureAcceptanceField $evidence 'adr_disposition') -ne
                'docs/governance/adr-dispositions/ADR-0005-2026-08-12.json' -or
            (Get-R0ArchitectureAcceptanceField $evidence 'work_package') -ne
                'docs/tasks/work-packages/R0-ARCH-001.md' -or
            ((@(Get-R0ArchitectureAcceptanceField $evidence 'reconciliation_dispositions')) -join ',') -ne
                'docs/governance/reconciliation-dispositions/RECON-DEC-006-2026-08-12.json,docs/governance/reconciliation-dispositions/RECON-DEC-007-2026-08-12.json') {
            Add-R0ArchitectureAcceptanceIssue $issues 'R0-ARCH-001 deliverable, criterion or evidence binding is incomplete.'
        }

        $commitVerification = Get-R0ArchitectureAcceptanceField $acceptance 'commit_bound_verification'
        $push = Get-R0ArchitectureAcceptanceField $commitVerification 'push_run'
        if ((Get-R0ArchitectureAcceptanceField $push 'run_id') -ne '31572060238' -or
            (Get-R0ArchitectureAcceptanceField $push 'event') -ne 'push' -or
            (Get-R0ArchitectureAcceptanceField $push 'url') -ne
                'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31572060238' -or
            (Get-R0ArchitectureAcceptanceField $push 'source_head_sha') -ne $expectedCommit -or
            (Get-R0ArchitectureAcceptanceField $push 'checked_out_sha') -ne $expectedCommit -or
            (Get-R0ArchitectureAcceptanceField $push 'conclusion') -ne 'success') {
            Add-R0ArchitectureAcceptanceIssue $issues 'R0-ARCH-001 push CI identity or result is invalid.'
        }
        $pushJobs = @(
            [pscustomobject]@{job_id='94035975671';name='windows-2025-vs2026-msvc-19.5x';runner_name='GitHub Actions 1000000113';runner_group='GitHub Actions';runner_os='Windows';runner_arch='X64';runner_image='windows-2025-vs2026';runner_image_version='20260803.193.1';cmake_version='4.4.2';compiler_id='MSVC';compiler_version='19.51.36252.0';conclusion='success';url='https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31572060238/job/94035975671'},
            [pscustomobject]@{job_id='94035975773';name='ubuntu-24.04-gcc-13';runner_name='GitHub Actions 1000000114';runner_group='GitHub Actions';runner_os='Linux';runner_arch='X64';runner_image='ubuntu-24.04';runner_image_version='20260720.247.2';cmake_version='3.31.6';compiler_id='GNU';compiler_version='13.3.0';conclusion='success';url='https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31572060238/job/94035975773'})
        Test-R0ArchitectureAcceptanceCiJobs -Jobs (
            Get-R0ArchitectureAcceptanceField $push 'jobs') -ExpectedJobs $pushJobs -Label 'R0-ARCH-001 push CI' -Issues $issues

        $pullRequest = Get-R0ArchitectureAcceptanceField $commitVerification 'pull_request_run'
        if ((Get-R0ArchitectureAcceptanceField $pullRequest 'run_id') -ne '31572064035' -or
            (Get-R0ArchitectureAcceptanceField $pullRequest 'event') -ne 'pull_request' -or
            (Get-R0ArchitectureAcceptanceField $pullRequest 'url') -ne
                'https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31572064035' -or
            (Get-R0ArchitectureAcceptanceField $pullRequest 'source_head_sha') -ne $expectedCommit -or
            (Get-R0ArchitectureAcceptanceField $pullRequest 'checked_out_ref') -ne 'refs/pull/4/merge' -or
            (Get-R0ArchitectureAcceptanceField $pullRequest 'checked_out_sha') -ne
                '69cf89df622d1c6d9ccd11aefe4e6c84bf7cf8e0' -or
            (Get-R0ArchitectureAcceptanceField $pullRequest 'checked_out_tree_sha') -ne $expectedTree -or
            ((@(Get-R0ArchitectureAcceptanceField $pullRequest 'checked_out_parent_shas')) -join ',') -ne
                "$expectedBase,$expectedCommit" -or
            (Get-R0ArchitectureAcceptanceField $pullRequest 'conclusion') -ne 'success') {
            Add-R0ArchitectureAcceptanceIssue $issues 'R0-ARCH-001 pull-request CI identity or result is invalid.'
        }
        try {
            $retained = Get-R0ArchitectureAcceptanceRetainedCommitFact (
                Get-R0ArchitectureAcceptanceField $pullRequest 'checked_out_commit_object')
            if ($retained.sha1 -ne '69cf89df622d1c6d9ccd11aefe4e6c84bf7cf8e0' -or
                $retained.tree -ne $expectedTree -or
                ((@($retained.parents)) -join ',') -ne "$expectedBase,$expectedCommit") {
                Add-R0ArchitectureAcceptanceIssue $issues 'Retained PR merge object does not match the reviewed tree and parents.'
            }
        }
        catch {
            Add-R0ArchitectureAcceptanceIssue $issues "Retained PR merge object is invalid: $($_.Exception.Message)"
        }
        $prJobs = @(
            [pscustomobject]@{job_id='94035986750';name='windows-2025-vs2026-msvc-19.5x';runner_name='GitHub Actions 1000000116';runner_group='GitHub Actions';runner_os='Windows';runner_arch='X64';runner_image='windows-2025-vs2026';runner_image_version='20260803.193.1';cmake_version='4.4.2';compiler_id='MSVC';compiler_version='19.51.36252.0';conclusion='success';url='https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31572064035/job/94035986750'},
            [pscustomobject]@{job_id='94035986787';name='ubuntu-24.04-gcc-13';runner_name='GitHub Actions 1000000115';runner_group='GitHub Actions';runner_os='Linux';runner_arch='X64';runner_image='ubuntu-24.04';runner_image_version='20260720.247.2';cmake_version='3.31.6';compiler_id='GNU';compiler_version='13.3.0';conclusion='success';url='https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31572064035/job/94035986787'})
        Test-R0ArchitectureAcceptanceCiJobs -Jobs (
            Get-R0ArchitectureAcceptanceField $pullRequest 'jobs') -ExpectedJobs $prJobs -Label 'R0-ARCH-001 pull-request CI' -Issues $issues

        $local = Get-R0ArchitectureAcceptanceField $commitVerification 'local_and_repository'
        $expectedLocal = [pscustomobject][ordered]@{
            architecture_validator = 'passed; 15/15 baseline mutations; 35/35 review-contract mutations'
            acceptance_validator = 'passed; 76/76 acceptance mutations'
            ctest = 'passed; 9/9'
            repository_verifier = 'passed; 70 JSON; 65 tasks; 100 Markdown'
            provenance_validator = 'passed; 14/14 mutations; 8/8 NOASSERTION'
            diff_check = 'passed'
            strict_utf8_json_and_wording = 'passed; 15/15 reviewed paths'
        }
        Test-R0ArchitectureAcceptanceExactValue -Actual $local -Expected $expectedLocal `
            -Label 'R0-ARCH-001 local verification' -Issues $issues
        Test-R0ArchitectureAcceptanceBoundaries -Record $acceptance -Label 'R0-ARCH-001 task acceptance' -Issues $issues
    }

    $backlog = Get-R0ArchitectureAcceptanceField $Context 'backlog'
    $task = @((Get-R0ArchitectureAcceptanceField $backlog 'tasks') | Where-Object {
            (Get-R0ArchitectureAcceptanceField $_ 'id') -eq 'R0-ARCH-001'
        }) | Select-Object -First 1
    $gateTask = @((Get-R0ArchitectureAcceptanceField $backlog 'tasks') | Where-Object {
            (Get-R0ArchitectureAcceptanceField $_ 'id') -eq 'R0-GATE-001'
        }) | Select-Object -First 1
    $arch002 = @((Get-R0ArchitectureAcceptanceField $backlog 'tasks') | Where-Object {
            (Get-R0ArchitectureAcceptanceField $_ 'id') -eq 'R0-ARCH-002'
        }) | Select-Object -First 1
    $futureAdvanced = @((Get-R0ArchitectureAcceptanceField $backlog 'tasks') | Where-Object {
            [string](Get-R0ArchitectureAcceptanceField $_ 'stage') -match '^R[1-8]$' -and
            (Get-R0ArchitectureAcceptanceField $_ 'status') -ne 'planned'
        })
    $expectedTask = [pscustomobject][ordered]@{
        id = 'R0-ARCH-001'
        stage = 'R0'
        title = 'Freeze the terminology registry and architecture dependency map'
        status = 'done'
        priority = 'P0'
        size = 'L'
        owner_role = 'architecture_lead'
        assignee = 'r0-po-agent'
        reviewer = 'r0-validation-agent'
        depends_on = @('B0-VER-001')
        deliverables = @(
            'Machine-readable terminology baseline',
            'module dependency map',
            'legacy-to-target ownership map')
        acceptance = @('Shared terms, enums, keys and owners each have one authority')
        evidence = @('Terminology conformance report')
        architecture_refs = @(
            'reference-glossary',
            ('02 ' + [char]0x00A7 + '13'),
            ('01 ' + [char]0x00A7 + '16'))
    }
    Test-R0ArchitectureAcceptanceExactValue -Actual $task -Expected $expectedTask `
        -Label 'R0-ARCH-001 canonical backlog row' -Issues $issues
    if ((Get-R0ArchitectureAcceptanceField $gateTask 'status') -ne 'planned' -or
        (Get-R0ArchitectureAcceptanceField $arch002 'status') -ne 'planned' -or
        $futureAdvanced.Count -ne 0 -or
        (Get-R0ArchitectureAcceptanceField (
            Get-R0ArchitectureAcceptanceField $Context 'project_manifest') 'current_gate') -ne 'R0') {
        Add-R0ArchitectureAcceptanceIssue $issues 'R0 gate, ARCH-002 or future-stage lock advanced without authority.'
    }

    $workPackage = [string](Get-R0ArchitectureAcceptanceField $Context 'work_package_text')
    $audit = [string](Get-R0ArchitectureAcceptanceField $Context 'audit_text')
    $handoff = [string](Get-R0ArchitectureAcceptanceField $Context 'handoff_text')
    $readiness = [string](Get-R0ArchitectureAcceptanceField $Context 'readiness_text')
    if ($workPackage -notmatch '(?m)^- \u72b6\u6001\uff1aDone$') {
        Add-R0ArchitectureAcceptanceIssue $issues 'R0-ARCH-001 work package status is stale.'
    }
    Test-R0ArchitectureAcceptanceTextContract -Text $workPackage -RequiredFragments @(
        'r0-po-agent',
        '/root/r0_architecture_agent',
        '/root/r0_validation_agent',
        '29f455efebd72113c1d311bc674a78c638265f34',
        '16d566512cd3d0bdb8e4f9fc84f3c8709328708aba7b4f95b4570b8a3f6a9561',
        'R0-ARCH-001-ACCEPTANCE-2026-08-12',
        '31572060238',
        '31572064035',
        '76/76',
        'R0-ARCH-002',
        'rights/provenance',
        'R0-GATE') -ForbiddenFragments @(
        'current technical candidate',
        'awaiting commit-bound disposition') -Label 'R0-ARCH-001 work package' -Issues $issues
    Test-R0ArchitectureAcceptanceTextContract -Text $audit -RequiredFragments @(
        'logical-only-keep-current',
        'keep-current-22-owner-consumer-map',
        'RECON-DEC-006-2026-08-12.json',
        'RECON-DEC-007-2026-08-12.json',
        '29f455efebd72113c1d311bc674a78c638265f34',
        '16d566512cd3d0bdb8e4f9fc84f3c8709328708aba7b4f95b4570b8a3f6a9561',
        '76/76',
        'Current amendment on 2026-08-12',
        'ADR-0004/R0-SPEC-001 and ADR-0005/R0-ARCH-001 are accepted',
        ('`RECON-DEC-004`' + [char]0x3001 + '`005` and `009` remain open')) -ForbiddenFragments @(
        ('`RECON-DEC-004`' + [char]0xFF5E + '`007` and `009` remain open'),
        'awaiting commit-bound disposition',
        'awaiting independent review') -Label 'reconciliation audit' -Issues $issues
    Test-R0ArchitectureAcceptanceTextContract -Text $handoff -RequiredFragments @(
        'R0-ARCH-001-ACCEPTANCE-2026-08-12',
        '29f455efebd72113c1d311bc674a78c638265f34',
        '16d566512cd3d0bdb8e4f9fc84f3c8709328708aba7b4f95b4570b8a3f6a9561',
        '31572060238',
        '31572064035',
        '76',
        '70',
        'R0-ARCH-002',
        'R0-GATE',
        'PR #4') -ForbiddenFragments @(
        'ADR/decision remains Proposed',
        'awaiting commit-bound Architecture/Validation disposition') -Label 'R0 execution handoff' -Issues $issues
    Test-R0ArchitectureAcceptanceTextContract -Text $readiness -RequiredFragments @(
        'R0-SPEC-001 and R0-ARCH-001 commit-bound acceptance',
        'technical target `29f455efebd72113c1d311bc674a78c638265f34`',
        '15 architecture + 35 review-contract + 76 acceptance mutations',
        '| ADR-0005 derived architecture baseline | `Accepted`',
        '| `G1-X-008` | terminology checker passes target blueprint/roadmap excluding raw expert input | accepted scoped report conformant, 276 terms/20 aliases/27 shared symbols; 15 baseline + 35 review-contract + 76 acceptance mutations | `verified_technical` | `closed` for the accepted source set',
        'full-repository evolution coverage remains `R0-ARCH-002`',
        'R0-ARCH-002') -ForbiddenFragments @(
        'R0-ARCH-001 technical candidate',
        '276 terms/9 mutations',
        'accepted derivation authority + frozen reviewed commit rerun + explicit scope record',
        '| ADR-0005 derived architecture baseline | `Proposed`') -Label 'R0 G0/G1 readiness audit' -Issues $issues
    if ($readiness -notmatch '(?m)^R0 .+6 .+dependency-blocked.+R0-ARCH-002.+$') {
        Add-R0ArchitectureAcceptanceIssue $issues 'R0 readiness planned-task classification is stale.'
    }
    if ($audit -notmatch '(?m)^Current amendment on 2026-08-12.*RECON-DEC-001.*003.*006.*007.*accepted.*RECON-DEC-004.*005.*009.*open.*$') {
        Add-R0ArchitectureAcceptanceIssue $issues 'Reconciliation current amendment does not preserve accepted and open decision sets.'
    }
    $gateWorkPackage = [string](Get-R0ArchitectureAcceptanceField $Context 'gate_work_package_text')
    $arch002WorkPackage = [string](Get-R0ArchitectureAcceptanceField $Context 'arch002_work_package_text')
    $fitnessPlan = [string](Get-R0ArchitectureAcceptanceField $Context 'fitness_plan_text')
    Test-R0ArchitectureAcceptanceTextContract -Text $gateWorkPackage -RequiredFragments @(
        'ADR-0006',
        'ADR-0008',
        'ADR-0004',
        'ADR-0005',
        'ADR-0009',
        'ADR-0005/R0-ARCH-001',
        'R0-ARCH-001',
        'commit-bound acceptance',
        'R0-GATE-001',
        'project-manifest.json',
        'current_gate',
        'R0') -ForbiddenFragments @(
        ('ADR-0005' + [char]0xFF5E + '0008'),
        ('`R0-ARCH-001`' + [char]0x3001 + '`R0-LEG-001`' + [char]0x3001 + '`R0-SCI-001`')) `
        -Label 'R0-GATE-001 current work package' -Issues $issues
    Test-R0ArchitectureAcceptanceTextContract -Text $arch002WorkPackage -RequiredFragments @(
        'activation pending',
        'R0-ARCH-001',
        'R0-SPEC-001',
        '15/15',
        '35/35',
        '76/76',
        'Accepted `RECON-DEC-006`',
        'Accepted `RECON-DEC-007`',
        'R0-ARCH-002',
        '`planned`',
        'assignee') -ForbiddenFragments @(
        'dependency blocked',
        '`RECON-DEC-006` / `RECON-DEC-007`') `
        -Label 'R0-ARCH-002 current work package' -Issues $issues
    if ($arch002WorkPackage -notmatch '(?m)^- \u4F9D\u8D56\uFF1A`R0-ARCH-001` \u4E0E `R0-SPEC-001` \u5747\u5DF2\u4E3A `done`\r?$' -or
        $arch002WorkPackage -notmatch '(?m)^- .+R0-ARCH-002.+planned.+assignee.+$') {
        Add-R0ArchitectureAcceptanceIssue $issues 'R0-ARCH-002 dependency or activation boundary is stale.'
    }
    Test-R0ArchitectureAcceptanceTextContract -Text $fitnessPlan -RequiredFragments @(
        'architecture baseline',
        'review contract',
        'task acceptance',
        '76',
        'Accepted `RECON-DEC-006` / `RECON-DEC-007`',
        'R0-ARCH-002',
        '`planned`',
        '`done`') -ForbiddenFragments @(
        ('`RECON-DEC-006` / `RECON-DEC-007` ' + [char]0x5173 + [char]0x95ED + [char]0x524D)) `
        -Label 'architecture fitness coverage plan' -Issues $issues
    if ($fitnessPlan -notmatch '(?m)^.*architecture baseline.*review contract.*task acceptance.*15.*35.*76.*mutation.*$') {
        Add-R0ArchitectureAcceptanceIssue $issues 'Architecture fitness current validation projection is stale.'
    }

    $provenance = Get-R0ArchitectureAcceptanceField $Context 'provenance'
    $items = @(Get-R0ArchitectureAcceptanceField $provenance 'items')
    $rightsOpen = @($items | Where-Object {
            (Get-R0ArchitectureAcceptanceField (
                Get-R0ArchitectureAcceptanceField $_ 'license') 'concluded_expression') -ne 'NOASSERTION' -or
            [string](Get-R0ArchitectureAcceptanceField $_ 'external_distribution') -match
                '^(allowed|approved|redistributable)$'
        })
    if ((Get-R0ArchitectureAcceptanceField (
            Get-R0ArchitectureAcceptanceField $provenance 'repository_license') 'selected') -ne $false -or
        $rightsOpen.Count -ne 0) {
        Add-R0ArchitectureAcceptanceIssue $issues 'Rights or external distribution advanced outside R0-ARCH-001.'
    }
    return $issues.ToArray()
}

function Get-R0ArchitectureAcceptanceContext {
    param([string]$RepoRoot)

    $reconciliation = [pscustomobject][ordered]@{
        'RECON-DEC-006' = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\governance\reconciliation-dispositions\RECON-DEC-006-2026-08-12.json')
        'RECON-DEC-007' = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\governance\reconciliation-dispositions\RECON-DEC-007-2026-08-12.json')
    }
    return [pscustomobject][ordered]@{
        authorization = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\governance\r0-owner-authorization.json')
        adr_text = if (Test-Path -LiteralPath (
            Join-Path $RepoRoot 'docs\adr\0005-derived-architecture-baseline.md')) {
            Get-Content -LiteralPath (
                Join-Path $RepoRoot 'docs\adr\0005-derived-architecture-baseline.md') -Raw -Encoding UTF8
        } else { '' }
        review_contract = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\architecture\r0-architecture-review-contract.json')
        baseline = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\architecture\architecture-baseline.json')
        report = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\quality\terminology-conformance-report.json')
        adr_disposition = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\governance\adr-dispositions\ADR-0005-2026-08-12.json')
        reconciliation_records = $reconciliation
        task_acceptance = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\quality\task-acceptance-R0-ARCH-001.json')
        backlog = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\tasks\backlog.json')
        project_manifest = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'project-manifest.json')
        provenance = Read-R0ArchitectureAcceptanceJson (
            Join-Path $RepoRoot 'docs\governance\provenance-inventory.json')
        work_package_text = if (Test-Path -LiteralPath (
            Join-Path $RepoRoot 'docs\tasks\work-packages\R0-ARCH-001.md')) {
            Get-Content -LiteralPath (
                Join-Path $RepoRoot 'docs\tasks\work-packages\R0-ARCH-001.md') -Raw -Encoding UTF8
        } else { '' }
        audit_text = if (Test-Path -LiteralPath (
            Join-Path $RepoRoot 'docs\quality\r0-first-wave-reconciliation-audit.md')) {
            Get-Content -LiteralPath (
                Join-Path $RepoRoot 'docs\quality\r0-first-wave-reconciliation-audit.md') -Raw -Encoding UTF8
        } else { '' }
        handoff_text = if (Test-Path -LiteralPath (
            Join-Path $RepoRoot 'docs\handoff\r0-execution-state.md')) {
            Get-Content -LiteralPath (
                Join-Path $RepoRoot 'docs\handoff\r0-execution-state.md') -Raw -Encoding UTF8
        } else { '' }
        readiness_text = if (Test-Path -LiteralPath (
            Join-Path $RepoRoot 'docs\quality\r0-g0-g1-readiness-audit.md')) {
            Get-Content -LiteralPath (
                Join-Path $RepoRoot 'docs\quality\r0-g0-g1-readiness-audit.md') -Raw -Encoding UTF8
        } else { '' }
        gate_work_package_text = if (Test-Path -LiteralPath (
            Join-Path $RepoRoot 'docs\tasks\work-packages\R0-GATE-001.md')) {
            Get-Content -LiteralPath (
                Join-Path $RepoRoot 'docs\tasks\work-packages\R0-GATE-001.md') -Raw -Encoding UTF8
        } else { '' }
        arch002_work_package_text = if (Test-Path -LiteralPath (
            Join-Path $RepoRoot 'docs\tasks\work-packages\R0-ARCH-002.md')) {
            Get-Content -LiteralPath (
                Join-Path $RepoRoot 'docs\tasks\work-packages\R0-ARCH-002.md') -Raw -Encoding UTF8
        } else { '' }
        fitness_plan_text = if (Test-Path -LiteralPath (
            Join-Path $RepoRoot 'docs\quality\architecture-fitness-coverage-plan.md')) {
            Get-Content -LiteralPath (
                Join-Path $RepoRoot 'docs\quality\architecture-fitness-coverage-plan.md') -Raw -Encoding UTF8
        } else { '' }
    }
}

function Test-R0ArchitectureAcceptance {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [switch]$RunMutations
    )

    $fatalIssues = [System.Collections.Generic.List[string]]::new()
    try {
        $expectedFileset = Get-R0ArchitectureAcceptanceReviewedFileset -RepoRoot $RepoRoot -Base (
            'e0bba2b99e96a2a6ded646302b1d1424d323c362'
        ) -Commit '29f455efebd72113c1d311bc674a78c638265f34' -DirectParent (
            '4aee5e8901e2afd8df38ccfc14cf0de04400cba0')
        $targetTree = [string](& git -C $RepoRoot rev-parse (
            '29f455efebd72113c1d311bc674a78c638265f34^{tree}') 2>$null)
        if ($LASTEXITCODE -ne 0 -or $targetTree.Trim() -ne
            'f4649afaf194935ada70920fdcee140fdf4bf545') {
            throw 'Reviewed target tree is incorrect.'
        }
    }
    catch {
        $fatalIssues.Add("R0-ARCH-001 reviewed Git range cannot be reproduced: $($_.Exception.Message)")
        return [pscustomobject][ordered]@{ Issues = $fatalIssues.ToArray(); MutationCount = 0 }
    }

    $context = Get-R0ArchitectureAcceptanceContext -RepoRoot $RepoRoot
    $issues = @(Get-R0ArchitectureAcceptanceIssues -Context $context -ExpectedFileset $expectedFileset)
    $mutationCount = 0
    if ($RunMutations -and $issues.Count -eq 0) {
        $mutations = @(
            [pscustomobject]@{ Name='missing-adr-disposition'; Apply={param($c) $c.adr_disposition=$null} },
            [pscustomobject]@{ Name='adr-wrong-commit'; Apply={param($c) $c.adr_disposition.reviewed_commit=('0'*40)} },
            [pscustomobject]@{ Name='adr-wrong-fileset'; Apply={param($c) $c.adr_disposition.reviewed_fileset.sha256=('0'*64)} },
            [pscustomobject]@{ Name='adr-wrong-owner'; Apply={param($c) $c.adr_disposition.decision_actor.actor_id='r0-po-agent'} },
            [pscustomobject]@{ Name='adr-self-review'; Apply={param($c) $c.adr_disposition.independent_review.actor_id='r0-architecture-agent';$c.adr_disposition.independent_review.task_path='/root/r0_architecture_agent'} },
            [pscustomobject]@{ Name='adr-rationale-unlocks-r1'; Apply={param($c) $c.adr_disposition.rationale[0]='R1 unlocked and rights cleared.'} },
            [pscustomobject]@{ Name='adr-verification-failed'; Apply={param($c) $c.adr_disposition.verification.architecture_validator='failed'} },
            [pscustomobject]@{ Name='missing-dec-006'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'=$null} },
            [pscustomobject]@{ Name='dec-006-wrong-outcome'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.outcome='physical-modules'} },
            [pscustomobject]@{ Name='dec-006-wrong-modules'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.contract.physical_policy.module_names+=@('packages_user')} },
            [pscustomobject]@{ Name='dec-006-self-review'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.independent_review.actor_id='r0-architecture-agent';$c.reconciliation_records.'RECON-DEC-006'.independent_review.task_path='/root/r0_architecture_agent'} },
            [pscustomobject]@{ Name='dec-006-wrong-graph-version'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.contract.physical_policy.graph_version='adr-0003/2'} },
            [pscustomobject]@{ Name='dec-006-wrong-registry-version'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.contract.physical_policy.registry_schema_version='gnczmkn.architecture-authority-registry/2'} },
            [pscustomobject]@{ Name='dec-006-wrong-boundary-kind'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.contract.logical_boundaries[0].kind='physical-module'} },
            [pscustomobject]@{ Name='dec-006-wrong-boundary-authority'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.contract.logical_boundaries[0].authority='bogus'} },
            [pscustomobject]@{ Name='dec-006-wrong-source-roots'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.contract.logical_boundaries[0].source_roots=@('framework')} },
            [pscustomobject]@{ Name='dec-006-wrong-representation'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.contract.logical_boundaries[0].representation='physical-module'} },
            [pscustomobject]@{ Name='dec-006-wrong-relationship'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.contract.logical_boundaries[0].relationship='compiler-reads-package-implementation'} },
            [pscustomobject]@{ Name='dec-006-wrong-required-mutations'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.required_mutations[4]='cmake-unknown-target'} },
            [pscustomobject]@{ Name='dec-006-wrong-migration'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.migration.rule='optional'} },
            [pscustomobject]@{ Name='dec-006-wrong-boundaries'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.boundaries[3]='r1-unlocked'} },
            [pscustomobject]@{ Name='dec-006-verification-failed'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-006'.verification.architecture_validator='failed'} },
            [pscustomobject]@{ Name='missing-dec-007'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'=$null} },
            [pscustomobject]@{ Name='dec-007-wrong-outcome'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.outcome='import-candidate-map'} },
            [pscustomobject]@{ Name='dec-007-wrong-count'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.contract.authority_mapping_count=23} },
            [pscustomobject]@{ Name='dec-007-wrong-classification-count'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.contract.classification_counts.aligned=21} },
            [pscustomobject]@{ Name='dec-007-wrong-aligned-id'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.contract.aligned_responsibility_ids[0]='bogus'} },
            [pscustomobject]@{ Name='dec-007-wrong-deferred-id'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.contract.deferred_owner_split_ids[0]='bogus'} },
            [pscustomobject]@{ Name='dec-007-wrong-logical-id'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.contract.logical_route_ids[0]='bogus'} },
            [pscustomobject]@{ Name='dec-007-wrong-unregistered-name'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.contract.unregistered_legacy_names[0]='bogus'} },
            [pscustomobject]@{ Name='dec-007-wrong-unregistered-id'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.contract.unregistered_responsibility_ids[1]='bogus'} },
            [pscustomobject]@{ Name='dec-007-allows-overlay'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.contract.responsibility_overlay_forbidden_in_v1=$false} },
            [pscustomobject]@{ Name='dec-007-drops-migration-policy'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.contract.glossary_migration_and_superseding_adr_required_for_change=$false} },
            [pscustomobject]@{ Name='dec-007-wrong-required-mutations'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.required_mutations[0]='bogus'} },
            [pscustomobject]@{ Name='dec-007-wrong-migration'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.migration.rule='optional'} },
            [pscustomobject]@{ Name='dec-007-wrong-boundaries'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.boundaries[3]='r1-unlocked'} },
            [pscustomobject]@{ Name='dec-007-verification-failed'; Apply={param($c) $c.reconciliation_records.'RECON-DEC-007'.verification.architecture_validator='failed'} },
            [pscustomobject]@{ Name='done-without-task-acceptance'; Apply={param($c) $c.task_acceptance=$null} },
            [pscustomobject]@{ Name='task-wrong-commit'; Apply={param($c) $c.task_acceptance.reviewed_commit=('0'*40)} },
            [pscustomobject]@{ Name='task-wrong-fileset-entry'; Apply={param($c) $c.task_acceptance.reviewed_fileset.entries[0].sha256=('0'*64)} },
            [pscustomobject]@{ Name='task-wrong-implementer'; Apply={param($c) $c.task_acceptance.implementation_actor.actor_id='r0-architecture-agent'} },
            [pscustomobject]@{ Name='task-wrong-owner'; Apply={param($c) $c.task_acceptance.decision_owner.actor_id='r0-po-agent'} },
            [pscustomobject]@{ Name='task-self-review'; Apply={param($c) $c.task_acceptance.independent_reviewer.actor_id='r0-po-agent';$c.task_acceptance.independent_reviewer.task_path='/root'} },
            [pscustomobject]@{ Name='task-wrong-evidence-ref'; Apply={param($c) $c.task_acceptance.acceptance_checks.evidence.work_package='docs/tasks/work-packages/R0-SPEC-001.md'} },
            [pscustomobject]@{ Name='task-wrong-reconciliation-refs'; Apply={param($c) $c.task_acceptance.acceptance_checks.evidence.reconciliation_dispositions=@('bogus-a','bogus-b')} },
            [pscustomobject]@{ Name='push-wrong-run-url'; Apply={param($c) $c.task_acceptance.commit_bound_verification.push_run.url='https://example.invalid/run'} },
            [pscustomobject]@{ Name='push-wrong-checkout'; Apply={param($c) $c.task_acceptance.commit_bound_verification.push_run.checked_out_sha=('0'*40)} },
            [pscustomobject]@{ Name='push-job-failure'; Apply={param($c) $c.task_acceptance.commit_bound_verification.push_run.jobs[0].conclusion='failure'} },
            [pscustomobject]@{ Name='push-wrong-compiler'; Apply={param($c) $c.task_acceptance.commit_bound_verification.push_run.jobs[1].compiler_version='14.0.0'} },
            [pscustomobject]@{ Name='pr-wrong-ref'; Apply={param($c) $c.task_acceptance.commit_bound_verification.pull_request_run.checked_out_ref='refs/heads/codex/r0-arch-001'} },
            [pscustomobject]@{ Name='pr-wrong-checkout'; Apply={param($c) $c.task_acceptance.commit_bound_verification.pull_request_run.checked_out_sha=('0'*40)} },
            [pscustomobject]@{ Name='pr-wrong-object-sha1'; Apply={param($c) $c.task_acceptance.commit_bound_verification.pull_request_run.checked_out_commit_object.sha1=('0'*40)} },
            [pscustomobject]@{ Name='pr-wrong-content-sha256'; Apply={param($c) $c.task_acceptance.commit_bound_verification.pull_request_run.checked_out_commit_object.content_sha256=('0'*64)} },
            [pscustomobject]@{ Name='pr-wrong-parents'; Apply={param($c) $c.task_acceptance.commit_bound_verification.pull_request_run.checked_out_parent_shas=@($c.task_acceptance.reviewed_commit,$c.task_acceptance.reviewed_base)} },
            [pscustomobject]@{ Name='drop-rights-boundary'; Apply={param($c) $c.task_acceptance.boundaries.rights_and_external_distribution='allowed'} },
            [pscustomobject]@{ Name='accepted-task-reverted-to-review'; Apply={param($c) (@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-ARCH-001'})[0]).status='review'} },
            [pscustomobject]@{ Name='done-task-missing-reviewer'; Apply={param($c) (@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-ARCH-001'})[0]).reviewer=$null} },
            [pscustomobject]@{ Name='backlog-bogus-deliverables'; Apply={param($c) (@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-ARCH-001'})[0]).deliverables=@('bogus')} },
            [pscustomobject]@{ Name='backlog-bogus-acceptance'; Apply={param($c) (@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-ARCH-001'})[0]).acceptance=@('bogus')} },
            [pscustomobject]@{ Name='backlog-bogus-evidence'; Apply={param($c) (@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-ARCH-001'})[0]).evidence=@('bogus')} },
            [pscustomobject]@{ Name='truncated-work-package'; Apply={param($c) $c.work_package_text='- \u72b6\u6001\uff1aDone`nR0-ARCH-001-ACCEPTANCE-2026-08-12'} },
            [pscustomobject]@{ Name='truncated-reconciliation-audit'; Apply={param($c) $c.audit_text='logical-only-keep-current keep-current-22-owner-consumer-map accepted'} },
            [pscustomobject]@{ Name='truncated-handoff'; Apply={param($c) $c.handoff_text='R0-ARCH-001 \u5df2\u5b8c\u6210'} },
            [pscustomobject]@{ Name='stale-readiness'; Apply={param($c) $c.readiness_text=$c.readiness_text.Replace('accepted scoped report conformant, 276 terms/20 aliases/27 shared symbols; 15 baseline + 35 review-contract + 76 acceptance mutations','current report conformant, 276 terms/9 mutations')} },
            [pscustomobject]@{ Name='readiness-wrong-g1-x008-status'; Apply={param($c) $c.readiness_text=$c.readiness_text.Replace('| `verified_technical` | `closed` for the accepted source set','| `candidate` | `closed` for the accepted source set')} },
            [pscustomobject]@{ Name='readiness-wrong-planned-classification'; Apply={param($c) $c.readiness_text=[regex]::Replace($c.readiness_text,'(?m)^(R0 .+?)6 (.+dependency-blocked.+)$','$1'+'7 '+'$2')} },
            [pscustomobject]@{ Name='audit-reopens-accepted-decisions'; Apply={param($c) $c.audit_text=$c.audit_text.Replace('ADR-0004/R0-SPEC-001 and ADR-0005/R0-ARCH-001 are accepted','ADR-0004/R0-SPEC-001 is accepted')} },
            [pscustomobject]@{ Name='audit-reopens-dec-006-007'; Apply={param($c) $c.audit_text=$c.audit_text.Replace('`006` and `007` are accepted','`006` and `007` remain open')} },
            [pscustomobject]@{ Name='gate-reopens-adr-0005'; Apply={param($c) $c.gate_work_package_text=$c.gate_work_package_text.Replace('ADR-0005/R0-ARCH-001','ADR-0004/R0-SPEC-001')} },
            [pscustomobject]@{ Name='gate-reverts-arch-001-to-review'; Apply={param($c) $c.gate_work_package_text=$c.gate_work_package_text.Replace('commit-bound acceptance','review pending')} },
            [pscustomobject]@{ Name='arch-002-reopens-dependency'; Apply={param($c) $c.arch002_work_package_text=$c.arch002_work_package_text.Replace('activation pending','dependency blocked')} },
            [pscustomobject]@{ Name='arch-002-wrong-dependency-row'; Apply={param($c) $old=([string][char]0x5747+[char]0x5DF2+[char]0x4E3A);$new=([string][char]0x4EC5+[char]0x540E+[char]0x8005+[char]0x4E3A);$c.arch002_work_package_text=$c.arch002_work_package_text.Replace($old,$new)} },
            [pscustomobject]@{ Name='arch-002-reopens-decisions'; Apply={param($c) $c.arch002_work_package_text=$c.arch002_work_package_text.Replace('Accepted `RECON-DEC-006`','Pending `RECON-DEC-006`')} },
            [pscustomobject]@{ Name='fitness-plan-old-mutation-coverage'; Apply={param($c) $c.fitness_plan_text=$c.fitness_plan_text.Replace('76','6')} },
            [pscustomobject]@{ Name='fitness-plan-wrong-baseline-review-counts'; Apply={param($c) $lines=@($c.fitness_plan_text -split "`r?`n");for($i=0;$i -lt $lines.Count;$i++){if($lines[$i] -match 'architecture baseline' -and $lines[$i] -match 'review contract' -and $lines[$i] -match 'task acceptance'){$lines[$i]=$lines[$i].Replace('15','14').Replace('35','34');break}};$c.fitness_plan_text=$lines -join "`n"} },
            [pscustomobject]@{ Name='fitness-plan-reopens-decisions'; Apply={param($c) $c.fitness_plan_text=$c.fitness_plan_text.Replace('Accepted `RECON-DEC-006` / `RECON-DEC-007`','Pending `RECON-DEC-006` / `RECON-DEC-007`')} }
        )
        foreach ($mutation in $mutations) {
            ++$mutationCount
            $mutated = Get-R0ArchitectureAcceptanceContext -RepoRoot $RepoRoot
            & $mutation.Apply $mutated
            $mutationIssues = @(Get-R0ArchitectureAcceptanceIssues -Context $mutated -ExpectedFileset $expectedFileset)
            if ($mutationIssues.Count -eq 0) {
                $fatalIssues.Add("R0-ARCH-001 acceptance mutation was not rejected: $($mutation.Name)")
            }
        }
    }
    foreach ($issue in $issues) { $fatalIssues.Add([string]$issue) }
    return [pscustomobject][ordered]@{
        Issues = $fatalIssues.ToArray()
        MutationCount = $mutationCount
    }
}

Export-ModuleMember -Function Test-R0ArchitectureAcceptance

[CmdletBinding()]
param(
    [string]$InventoryPath,
    [switch]$RequireExternalReady,
    [switch]$Json,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($InventoryPath)) {
    $InventoryPath = Join-Path $repoRoot 'docs\governance\provenance-inventory.json'
}
elseif (-not [System.IO.Path]::IsPathRooted($InventoryPath)) {
    $InventoryPath = Join-Path $repoRoot $InventoryPath
}

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

function Read-Json([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required JSON is missing: $Path"
    }
    return Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
}

function Copy-JsonObject([object]$Value) {
    return $Value | ConvertTo-Json -Depth 100 | ConvertFrom-Json
}

function Get-Sha256([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString(
                $algorithm.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

function Get-ItemById([object[]]$Items, [string]$Id) {
    return @($Items | Where-Object {
            [string](Get-Field $_ 'id') -ceq $Id
        }) | Select-Object -First 1
}

function Test-PathMatchesScope([string]$Path, [object]$Scope) {
    $included = $false
    foreach ($prefix in @(Get-Field $Scope 'include_prefixes')) {
        $prefixText = [string]$prefix
        if ($prefixText.Length -eq 0 -or
            $Path.StartsWith($prefixText, [System.StringComparison]::Ordinal)) {
            $included = $true
            break
        }
    }
    if (-not $included) { return $false }

    foreach ($prefix in @(Get-Field $Scope 'exclude_prefixes')) {
        $prefixText = [string]$prefix
        if ($prefixText.Length -gt 0 -and
            $Path.StartsWith($prefixText, [System.StringComparison]::Ordinal)) {
            return $false
        }
    }
    return $true
}

function Test-LocalReferences(
    [System.Collections.Generic.List[string]]$Issues,
    [string]$Subject,
    [object[]]$References) {
    if ($References.Count -eq 0) {
        $Issues.Add("$Subject has no source reference.")
        return
    }
    foreach ($reference in $References) {
        $text = [string]$reference
        if ([string]::IsNullOrWhiteSpace($text)) {
            $Issues.Add("$Subject has an empty source reference.")
            continue
        }
        if ($text -match '^https?://') { continue }
        if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $text) -PathType Leaf)) {
            $Issues.Add("$Subject references a missing local file: $text")
        }
    }
}

function Test-InventoryObject(
    [object]$Inventory,
    [string[]]$TrackedFiles,
    [string[]]$CMakeTexts,
    [string[]]$WorkflowTexts,
    [string[]]$RootLicenseFiles) {
    $issues = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $Inventory) {
        $issues.Add('Distribution inventory is null.')
        return $issues.ToArray()
    }

    if ((Get-Field $Inventory 'format_version') -ne 1) {
        $issues.Add('Unsupported distribution inventory format.')
    }
    if ((Get-Field $Inventory 'task_id') -ne 'R0-GOV-002') {
        $issues.Add('Distribution inventory does not belong to R0-GOV-002.')
    }

    $repository = Get-Field $Inventory 'repository'
    foreach ($field in @(
            'owner_decision', 'recommended_g1_scope', 'license_conclusion',
            'external_distribution', 'origin_remote', 'existing_public_exposure')) {
        if (-not (Test-HasField $repository $field)) {
            $issues.Add("Repository distribution state is missing '$field'.")
        }
    }
    $ownerDecision = [string](Get-Field $repository 'owner_decision')
    $repositoryDistribution = [string](Get-Field $repository 'external_distribution')
    if ($ownerDecision -eq 'pending' -and $repositoryDistribution -ne 'blocked') {
        $issues.Add('Repository external distribution cannot be allowed while the owner decision is pending.')
    }
    if ($ownerDecision -eq 'pending' -and $RootLicenseFiles.Count -gt 0) {
        $issues.Add('A root distribution license appeared while the repository owner decision is pending.')
    }
    if ($repositoryDistribution -eq 'allowed' -and $RootLicenseFiles.Count -eq 0) {
        $issues.Add('Repository external distribution is allowed without a root license file.')
    }
    if ($repositoryDistribution -eq 'allowed' -and
        [string](Get-Field $repository 'license_conclusion') -in @('NONE', 'NOASSERTION', '')) {
        $issues.Add('Repository external distribution is allowed without a license conclusion.')
    }

    $scopes = @(Get-Field $Inventory 'tracked_scopes')
    if ($scopes.Count -eq 0) {
        $issues.Add('Distribution inventory has no tracked scopes.')
        return $issues.ToArray()
    }
    $scopeIds = @{}
    $allowedBinaryPaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($scope in $scopes) {
        foreach ($field in @(
                'id', 'include_prefixes', 'exclude_prefixes', 'handling',
                'rights_status', 'license_conclusion', 'external_distribution',
                'source_refs', 'allowed_binary_paths')) {
            if (-not (Test-HasField $scope $field)) {
                $issues.Add("Tracked scope is missing '$field'.")
            }
        }
        $id = [string](Get-Field $scope 'id')
        if ([string]::IsNullOrWhiteSpace($id)) {
            $issues.Add('Tracked scope id is empty.')
        }
        elseif ($scopeIds.ContainsKey($id)) {
            $issues.Add("Duplicate tracked scope id: $id")
        }
        else {
            $scopeIds[$id] = $scope
        }
        if (@(Get-Field $scope 'include_prefixes').Count -eq 0) {
            $issues.Add("Tracked scope '$id' has no include prefix.")
        }
        Test-LocalReferences $issues "Tracked scope '$id'" @(Get-Field $scope 'source_refs')

        $scopeDistribution = [string](Get-Field $scope 'external_distribution')
        $scopeLicense = [string](Get-Field $scope 'license_conclusion')
        $scopeRights = [string](Get-Field $scope 'rights_status')
        if ($scopeDistribution -notin @('blocked', 'allowed')) {
            $issues.Add("Tracked scope '$id' has unsupported external distribution '$scopeDistribution'.")
        }
        if ($scopeDistribution -eq 'allowed' -and
            ($scopeLicense -in @('NONE', 'NOASSERTION', '') -or $scopeRights -ne 'cleared')) {
            $issues.Add("Tracked scope '$id' is externally allowed without cleared rights and a license conclusion.")
        }
        foreach ($path in @(Get-Field $scope 'allowed_binary_paths')) {
            $pathText = [string]$path
            if (-not $allowedBinaryPaths.Add($pathText)) {
                $issues.Add("Tracked binary path is allowed by more than one scope: $pathText")
            }
        }
    }

    if (-not $scopeIds.ContainsKey('legacy-reference')) {
        $issues.Add('Tracked scope legacy-reference is missing.')
    }
    else {
        $legacyScope = $scopeIds['legacy-reference']
        if ((Get-Field $legacyScope 'handling') -ne 'evidence-only' -or
            (Get-Field $legacyScope 'external_distribution') -ne 'blocked') {
            $issues.Add('Legacy reference must remain evidence-only and externally blocked.')
        }
    }

    foreach ($path in $TrackedFiles) {
        $matchingScopes = @($scopes | Where-Object {
                Test-PathMatchesScope $path $_
            })
        if ($matchingScopes.Count -eq 0) {
            $issues.Add("Tracked path matches no distribution scope: $path")
        }
        elseif ($matchingScopes.Count -gt 1) {
            $issues.Add("Tracked path matches multiple distribution scopes: $path")
        }
    }

    foreach ($path in $allowedBinaryPaths) {
        if ($path -notin $TrackedFiles) {
            $issues.Add("Allowed binary path is not tracked: $path")
        }
    }
    $reviewRequiredExtensions = @(
        '.zip', '.7z', '.tar', '.gz', '.bz2', '.xz', '.exe', '.dll', '.lib',
        '.a', '.so', '.dylib', '.jar', '.whl', '.onnx', '.bin', '.pdf',
        '.png', '.jpg', '.jpeg', '.gif', '.mat', '.docx', '.xlsx', '.pptx')
    foreach ($path in $TrackedFiles) {
        $extension = [System.IO.Path]::GetExtension($path).ToLowerInvariant()
        if ($extension -in $reviewRequiredExtensions -and
            -not $allowedBinaryPaths.Contains($path)) {
            $issues.Add("Tracked binary or archive requires an explicit inventory entry: $path")
        }
    }

    $externalInputs = @(Get-Field $Inventory 'external_inputs')
    if ($externalInputs.Count -eq 0) {
        $issues.Add('Distribution inventory has no external inputs.')
    }
    $inputIds = @{}
    $registeredCMakePackages = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    $registeredWorkflowUses = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($input in $externalInputs) {
        foreach ($field in @(
                'id', 'use_scope', 'version', 'bundled', 'source_refs',
                'license_conclusion', 'distribution_effect')) {
            if (-not (Test-HasField $input $field)) {
                $issues.Add("External input is missing '$field'.")
            }
        }
        $id = [string](Get-Field $input 'id')
        if ([string]::IsNullOrWhiteSpace($id)) {
            $issues.Add('External input id is empty.')
        }
        elseif ($inputIds.ContainsKey($id)) {
            $issues.Add("Duplicate external input id: $id")
        }
        else {
            $inputIds[$id] = $input
        }
        Test-LocalReferences $issues "External input '$id'" @(Get-Field $input 'source_refs')
        $bundled = Get-Field $input 'bundled'
        if ($bundled -isnot [bool]) {
            $issues.Add("External input '$id' has a non-boolean bundled field.")
        }
        elseif ($bundled) {
            $license = [string](Get-Field $input 'license_conclusion')
            if ($license -in @('NONE', 'NOASSERTION', '')) {
                $issues.Add("Bundled external input '$id' has no license conclusion.")
            }
            if (@(Get-Field $input 'license_evidence_refs').Count -eq 0) {
                $issues.Add("Bundled external input '$id' has no license evidence.")
            }
        }
        foreach ($package in @(Get-Field $input 'cmake_packages')) {
            $packageText = [string]$package
            if ([string]::IsNullOrWhiteSpace($packageText)) { continue }
            if (-not $registeredCMakePackages.Add($packageText)) {
                $issues.Add("CMake package is registered by more than one external input: $packageText")
            }
        }
        $workflowUse = [string](Get-Field $input 'workflow_uses')
        if (-not [string]::IsNullOrWhiteSpace($workflowUse) -and
            -not $registeredWorkflowUses.Add($workflowUse)) {
            $issues.Add("Workflow use is registered by more than one external input: $workflowUse")
        }
    }

    $dependencyAcquisitionPattern =
        '(?im)\b(FetchContent_(Declare|MakeAvailable)|ExternalProject_Add|CPMAddPackage)\s*\('
    foreach ($text in $CMakeTexts) {
        $activeText = (@($text -split "`r?`n" | Where-Object {
                    $_ -notmatch '^\s*#'
                }) -join "`n")
        if ($activeText -match $dependencyAcquisitionPattern) {
            $issues.Add('Unreviewed CMake dependency acquisition signal is present.')
        }
        foreach ($match in [regex]::Matches(
                $activeText,
                '(?im)\bfind_package\s*\(\s*([A-Za-z0-9_.+-]+)')) {
            $package = [string]$match.Groups[1].Value
            if (-not $registeredCMakePackages.Contains($package)) {
                $issues.Add("CMake package has no external-input record: $package")
            }
        }
    }

    foreach ($text in $WorkflowTexts) {
        foreach ($match in [regex]::Matches($text, '(?im)^\s*uses:\s*([^\s#]+)')) {
            $workflowUse = [string]$match.Groups[1].Value
            if (-not $registeredWorkflowUses.Contains($workflowUse)) {
                $issues.Add("Workflow dependency has no external-input record: $workflowUse")
            }
        }
    }

    return $issues.ToArray()
}

function Get-ExternalBlockers([object]$Inventory) {
    $blockers = [System.Collections.Generic.List[string]]::new()
    $repository = Get-Field $Inventory 'repository'
    if ((Get-Field $repository 'owner_decision') -ne 'accepted') {
        $blockers.Add('repository-owner-distribution-decision-pending')
    }
    if ((Get-Field $repository 'external_distribution') -ne 'allowed') {
        $blockers.Add('repository-distribution-blocked')
    }
    $publicExposure = Get-Field $repository 'existing_public_exposure'
    if ((Get-Field $publicExposure 'owner_disposition') -ne 'resolved') {
        $blockers.Add('existing-public-origin-disposition-pending')
    }
    foreach ($scope in @(Get-Field $Inventory 'tracked_scopes')) {
        if ((Get-Field $scope 'external_distribution') -ne 'allowed') {
            $blockers.Add("scope:$([string](Get-Field $scope 'id')):$([string](Get-Field $scope 'rights_status'))")
        }
    }
    return $blockers.ToArray()
}

function Invoke-NegativeCase(
    [string]$Name,
    [string]$ExpectedDiagnostic,
    [object]$CandidateInventory,
    [string[]]$CandidateTrackedFiles,
    [string[]]$CandidateCMakeTexts,
    [string[]]$CandidateWorkflowTexts,
    [string[]]$CandidateRootLicenseFiles) {
    $issues = @(Test-InventoryObject `
            $CandidateInventory `
            $CandidateTrackedFiles `
            $CandidateCMakeTexts `
            $CandidateWorkflowTexts `
            $CandidateRootLicenseFiles)
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
    $inventory = Read-Json $InventoryPath

    $trackedFiles = @(& git -C $repoRoot -c core.quotepath=false ls-files)
    if ($LASTEXITCODE -ne 0 -or $trackedFiles.Count -eq 0) {
        throw 'Cannot enumerate tracked repository files.'
    }
    $trackedFiles = @($trackedFiles | ForEach-Object {
            ([string]$_).Replace('\', '/')
        })

    $cmakeTexts = [System.Collections.Generic.List[string]]::new()
    foreach ($path in $trackedFiles | Where-Object {
            $_ -notlike 'reference/legacy/*' -and
            ([System.IO.Path]::GetFileName($_) -eq 'CMakeLists.txt' -or
                [System.IO.Path]::GetExtension($_) -eq '.cmake')
        }) {
        $cmakeTexts.Add((Get-Content -LiteralPath (Join-Path $repoRoot $path) -Raw -Encoding utf8))
    }

    $workflowTexts = [System.Collections.Generic.List[string]]::new()
    foreach ($path in $trackedFiles | Where-Object {
            $_ -like '.github/workflows/*.yml' -or $_ -like '.github/workflows/*.yaml'
        }) {
        $workflowTexts.Add((Get-Content -LiteralPath (Join-Path $repoRoot $path) -Raw -Encoding utf8))
    }

    $rootLicenseFiles = @(Get-ChildItem -LiteralPath $repoRoot -File | Where-Object {
            $_.Name -match '^(?i)(LICENSE|LICENCE|COPYING|UNLICENSE)(\.(md|txt|rst))?$'
        } | ForEach-Object { $_.Name })

    foreach ($issue in @(Test-InventoryObject `
            $inventory `
            $trackedFiles `
            $cmakeTexts.ToArray() `
            $workflowTexts.ToArray() `
            $rootLicenseFiles)) {
        $errors.Add($issue)
    }

    $legacyScope = Get-ItemById @(Get-Field $inventory 'tracked_scopes') 'legacy-reference'
    $legacyArchive = Get-Field $legacyScope 'archive'
    $legacyPath = Join-Path $repoRoot ([string](Get-Field $legacyArchive 'path'))
    $legacyAudit = $null
    if (-not (Test-Path -LiteralPath $legacyPath -PathType Leaf)) {
        $errors.Add('Legacy archive is missing.')
    }
    else {
        Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
        $archive = [System.IO.Compression.ZipFile]::OpenRead($legacyPath)
        try {
            $fileEntries = @($archive.Entries | Where-Object {
                    -not [string]::IsNullOrEmpty($_.Name)
                })
            $licenseNamePattern = '(?i)(^|/)(licen[cs]e|copying|notice|copyright)(\.[^/]*)?$'
            $licenseTextPattern = '(?im)SPDX-License-Identifier|Copyright\s*(\(c\)|©)|Licensed under the|Permission is hereby granted, free of charge|GNU (GENERAL|LESSER) PUBLIC LICENSE|Mozilla Public License'
            $licenseNamedEntries = @($fileEntries | Where-Object {
                    $_.FullName -match $licenseNamePattern
                })
            $licenseTextEntries = [System.Collections.Generic.List[string]]::new()
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
                    $licenseTextEntries.Add($entry.FullName)
                }
            }
            $legacyAudit = [pscustomobject][ordered]@{
                sha256 = Get-Sha256 $legacyPath
                bytes = (Get-Item -LiteralPath $legacyPath).Length
                file_entries = $fileEntries.Count
                license_named_entries = $licenseNamedEntries.Count
                license_text_signal_entries = $licenseTextEntries.Count
            }
        }
        finally {
            $archive.Dispose()
        }

        if ($legacyAudit.sha256 -ne (Get-Field $legacyArchive 'sha256') -or
            $legacyAudit.bytes -ne (Get-Field $legacyArchive 'bytes')) {
            $errors.Add('Legacy archive identity differs from the distribution inventory.')
        }
        $legacyManifest = Read-Json (Join-Path $repoRoot 'reference\legacy\source-manifest.json')
        $manifestArchive = Get-Field $legacyManifest 'archive'
        if ($legacyAudit.sha256 -ne (Get-Field $manifestArchive 'sha256') -or
            $legacyAudit.bytes -ne (Get-Field $manifestArchive 'bytes')) {
            $errors.Add('Legacy archive identity differs from its source manifest.')
        }
    }

    $reproductionPointer = Read-Json (
        Join-Path $repoRoot 'reference\legacy\reproduction\current.json')
    $environmentPath = Join-Path $repoRoot (
        ([string](Get-Field $reproductionPointer 'path')).Replace('/', '\') +
        '\environment-manifest.json')
    $environment = Read-Json $environmentPath
    $externalInputs = @(Get-Field $inventory 'external_inputs')
    foreach ($mapping in @(
            @{ item = 'eigen-3.4.0-legacy-reproduction'; environment = 'eigen' },
            @{ item = 'w64devkit-2.9.1-legacy-reproduction'; environment = 'w64devkit' })) {
        $item = Get-ItemById $externalInputs $mapping.item
        $dependency = Get-Field (Get-Field $environment 'dependencies') $mapping.environment
        $integrity = Get-Field $item 'integrity'
        if ($null -eq $item -or $null -eq $dependency -or
            (Get-Field $item 'version') -ne (Get-Field $dependency 'version') -or
            (Get-Field $integrity 'sha256') -ne (Get-Field $dependency 'archive_sha256') -or
            (Get-Field $integrity 'bytes') -ne (Get-Field $dependency 'archive_bytes')) {
            $errors.Add("External input identity differs from Legacy reproduction evidence: $($mapping.item)")
        }
    }

    $candidate = Copy-JsonObject $inventory
    $candidate.repository.external_distribution = 'allowed'
    Invoke-NegativeCase `
        'owner-decision-bypass' `
        'owner decision is pending' `
        $candidate `
        $trackedFiles `
        $cmakeTexts.ToArray() `
        $workflowTexts.ToArray() `
        $rootLicenseFiles

    $candidate = Copy-JsonObject $inventory
    $candidateLegacy = Get-ItemById @($candidate.tracked_scopes) 'legacy-reference'
    $candidateLegacy.external_distribution = 'allowed'
    $candidateLegacy.rights_status = 'cleared'
    $candidateLegacy.license_conclusion = 'MIT'
    Invoke-NegativeCase `
        'legacy-external-distribution' `
        'Legacy reference must remain evidence-only and externally blocked' `
        $candidate `
        $trackedFiles `
        $cmakeTexts.ToArray() `
        $workflowTexts.ToArray() `
        $rootLicenseFiles

    Invoke-NegativeCase `
        'unreviewed-vendored-binary' `
        'Tracked binary or archive requires an explicit inventory entry' `
        (Copy-JsonObject $inventory) `
        @($trackedFiles + 'vendor/unreviewed.dll') `
        $cmakeTexts.ToArray() `
        $workflowTexts.ToArray() `
        $rootLicenseFiles

    Invoke-NegativeCase `
        'unreviewed-cmake-download' `
        'Unreviewed CMake dependency acquisition signal' `
        (Copy-JsonObject $inventory) `
        $trackedFiles `
        @($cmakeTexts.ToArray() + 'FetchContent_Declare(unreviewed URL https://example.invalid/source.zip)') `
        $workflowTexts.ToArray() `
        $rootLicenseFiles

    $externalBlockers = @(Get-ExternalBlockers $inventory)
    $scopeCounts = [System.Collections.Generic.List[object]]::new()
    foreach ($scope in @(Get-Field $inventory 'tracked_scopes')) {
        $scopeCounts.Add([pscustomobject][ordered]@{
                id = [string](Get-Field $scope 'id')
                tracked_files = @($trackedFiles | Where-Object {
                        Test-PathMatchesScope $_ $scope
                    }).Count
            })
    }

    $summary = [pscustomobject][ordered]@{
        task_id = 'R0-GOV-002'
        validation = if ($errors.Count -eq 0) { 'passed' } else { 'failed' }
        internal_workspace = [ordered]@{
            ready = $errors.Count -eq 0
            tracked_files = $trackedFiles.Count
            scope_coverage = @($scopeCounts)
        }
        external_distribution = [ordered]@{
            ready = $errors.Count -eq 0 -and $externalBlockers.Count -eq 0
            blockers = $externalBlockers
        }
        build_inputs = [ordered]@{
            cmake_documents = $cmakeTexts.Count
            workflow_documents = $workflowTexts.Count
            external_inputs = $externalInputs.Count
        }
        legacy_archive = $legacyAudit
        negative_cases = @($negativeResults)
        validation_errors = @($errors)
    }

    if ($errors.Count -gt 0) {
        if ($Json) {
            $summary | ConvertTo-Json -Depth 10
        }
        else {
            Write-Host "License/provenance validation failed with $($errors.Count) issue(s):"
            foreach ($errorMessage in $errors) {
                Write-Host " - $errorMessage"
            }
        }
        exit 1
    }

    if ($RequireExternalReady -and $externalBlockers.Count -gt 0) {
        if ($Json) {
            $summary | ConvertTo-Json -Depth 10
        }
        elseif (-not $Quiet) {
            Write-Host 'External distribution is blocked:'
            foreach ($blocker in $externalBlockers) {
                Write-Host " - $blocker"
            }
        }
        exit 2
    }

    if ($Json) {
        $summary | ConvertTo-Json -Depth 10
    }
    elseif (-not $Quiet) {
        Write-Host 'License/provenance validation passed.'
        Write-Host "Tracked scope coverage: $($trackedFiles.Count)/$($trackedFiles.Count)"
        Write-Host "External distribution: blocked by $($externalBlockers.Count) unresolved item(s)"
        Write-Host "Negative cases rejected: $(@($negativeResults | Where-Object { $_.rejected }).Count)/$($negativeResults.Count)"
    }
}
catch {
    if ($Json) {
        [pscustomobject][ordered]@{
            task_id = 'R0-GOV-002'
            validation = 'failed'
            error = $_.Exception.Message
        } | ConvertTo-Json -Depth 4
    }
    else {
        Write-Host "License/provenance validation failed: $($_.Exception.Message)"
    }
    exit 1
}

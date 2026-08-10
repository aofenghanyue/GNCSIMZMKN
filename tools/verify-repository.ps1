[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$errors = [System.Collections.Generic.List[string]]::new()

function Add-Error([string]$Message) {
    $script:errors.Add($Message)
}

function Read-Json([string]$Path) {
    try {
        return Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
    }
    catch {
        Add-Error "Invalid JSON: $Path :: $($_.Exception.Message)"
        return $null
    }
}

function Test-PathBelow([string]$Candidate, [string]$Root) {
    $separator = [System.IO.Path]::DirectorySeparatorChar
    $prefix = $Root.TrimEnd($separator, [System.IO.Path]::AltDirectorySeparatorChar) + $separator
    return $Candidate.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

$requiredPaths = @(
    'README.md',
    'AGENTS.md',
    'CMakeLists.txt',
    'CMakePresets.json',
    'project-manifest.json',
    'LICENSE-STATUS.md',
    'design-notes/gnczmkn-architecture-roadmap/README.md',
    'docs/handoff/README.md',
    'docs/tasks/backlog.json',
    'docs/team/role-assignments.json',
    'docs/architecture/authority-registry.json',
    'docs/architecture/architecture-baseline.json',
    'docs/quality/terminology-conformance-report.json',
    'docs/adr/0001-greenfield-and-legacy-reference.md',
    'docs/adr/0008-internal-default-license-and-provenance-gate.md',
    'docs/adr/0009-accountable-roles-and-candidate-toolchain.md',
    'docs/governance/license-and-provenance-policy.md',
    'docs/governance/provenance-inventory.json',
    'docs/governance/toolchain-support-matrix.json',
    'docs/quality/provenance-review-checklist.md',
    'docs/quality/license-provenance-conformance-report.json',
    'docs/quality/team-toolchain-readiness-report.json',
    'reference/legacy/source-manifest.json',
    'reference/legacy/legacy-source.zip',
    'reference/legacy/legacy-source.sha256',
    'reference/legacy/reproduction/current.json',
    'reference/legacy/reproduction/compatibility-msvc-19.50.json',
    'fixtures/ref-yyz-001/fixture-manifest.json',
    'fixtures/ref-scientific-conventions/fixture-manifest.json',
    'fixtures/ref-scientific-conventions/conventions.json',
    'fixtures/ref-scientific-conventions/cases.json',
    'docs/quality/scientific-conventions-cross-tool-report.json',
    'oracles/oracle-manifest.json'
)

foreach ($relativePath in $requiredPaths) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $relativePath))) {
        Add-Error "Required path is missing: $relativePath"
    }
}

$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'build'))
$extractedLegacyRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'reference/legacy/extracted'))
$jsonFiles = Get-ChildItem -LiteralPath $repoRoot -Recurse -File -Filter '*.json' |
    Where-Object {
        -not (Test-PathBelow $_.FullName $buildRoot) -and
        -not (Test-PathBelow $_.FullName $extractedLegacyRoot)
    }
foreach ($file in $jsonFiles) {
    [void](Read-Json $file.FullName)
}

$projectManifestPath = Join-Path $repoRoot 'project-manifest.json'
$backlogPath = Join-Path $repoRoot 'docs\tasks\backlog.json'
$rolesPath = Join-Path $repoRoot 'docs\team\role-assignments.json'
$projectManifest = Read-Json $projectManifestPath
$backlog = Read-Json $backlogPath
$roles = Read-Json $rolesPath

if ($null -ne $projectManifest -and $null -ne $backlog) {
    if ($projectManifest.current_gate -ne $backlog.current_gate) {
        Add-Error "project-manifest current_gate differs from backlog current_gate."
    }

    $taskById = @{}
    $allowedStatuses = @($backlog.status_values)
    foreach ($task in $backlog.tasks) {
        if ($taskById.ContainsKey($task.id)) {
            Add-Error "Duplicate task id: $($task.id)"
        }
        else {
            $taskById[$task.id] = $task
        }
        if ($task.status -notin $allowedStatuses) {
            Add-Error "Task $($task.id) has unsupported status $($task.status)."
        }
    }

    $roleById = @{}
    if ($null -ne $roles) {
        foreach ($role in $roles.roles) {
            if ($roleById.ContainsKey($role.id)) {
                Add-Error "Duplicate team role id: $($role.id)"
            }
            else {
                $roleById[$role.id] = $role
            }
        }
    }

    foreach ($task in $backlog.tasks) {
        if ($null -ne $roles -and -not $roleById.ContainsKey($task.owner_role)) {
            Add-Error "Task $($task.id) references undeclared owner role $($task.owner_role)."
        }
        foreach ($dependency in $task.depends_on) {
            if (-not $taskById.ContainsKey($dependency)) {
                Add-Error "Task $($task.id) references missing dependency $dependency."
            }
            elseif ($dependency -eq $task.id) {
                Add-Error "Task $($task.id) depends on itself."
            }
        }

        if ($task.status -eq 'ready') {
            foreach ($dependency in $task.depends_on) {
                if ($taskById.ContainsKey($dependency) -and $taskById[$dependency].status -ne 'done') {
                    Add-Error "Ready task $($task.id) has incomplete dependency $dependency."
                }
            }
        }

        if ($task.status -in @('in_progress', 'review', 'done') -and
            [string]::IsNullOrWhiteSpace([string]$task.assignee)) {
            Add-Error "Active or completed task $($task.id) has no assignee."
        }

        if ($task.status -in @('in_progress', 'review', 'done')) {
            foreach ($dependency in $task.depends_on) {
                if ($taskById.ContainsKey($dependency) -and $taskById[$dependency].status -ne 'done') {
                    Add-Error "Active or completed task $($task.id) has incomplete dependency $dependency."
                }
            }
        }
    }
}

$legacyManifestPath = Join-Path $repoRoot 'reference\legacy\source-manifest.json'
if (Test-Path -LiteralPath $legacyManifestPath) {
    $legacyManifest = Read-Json $legacyManifestPath
    if ($null -ne $legacyManifest) {
        $archivePath = Join-Path $repoRoot $legacyManifest.archive.path
        $checksumPath = Join-Path $repoRoot 'reference\legacy\legacy-source.sha256'
        if ((Test-Path -LiteralPath $archivePath) -and (Test-Path -LiteralPath $checksumPath)) {
            $actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
            $expectedHash = (Get-Content -LiteralPath $checksumPath -Raw -Encoding utf8).Trim().Split(' ')[0].ToLowerInvariant()
            if ($actualHash -ne $expectedHash) {
                Add-Error "Legacy archive hash mismatch."
            }
            if ($legacyManifest.archive.sha256.ToLowerInvariant() -ne $actualHash) {
                Add-Error "Legacy manifest hash differs from the archive hash."
            }
        }
    }
}

$productionRoots = @('apps', 'framework', 'adapters', 'packages', 'user', 'cmake')
$productionFiles = @()
foreach ($relativeRoot in $productionRoots) {
    $absoluteRoot = Join-Path $repoRoot $relativeRoot
    if (Test-Path -LiteralPath $absoluteRoot) {
        $productionFiles += Get-ChildItem -LiteralPath $absoluteRoot -Recurse -File |
            Where-Object { $_.Extension -in @('.h', '.hpp', '.hxx', '.c', '.cc', '.cpp', '.cxx', '.cmake') -or $_.Name -eq 'CMakeLists.txt' }
    }
}
$productionFiles += Get-Item -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt')

$legacyPatterns = @(
    '#include\s*[<\"](?:\.\./)*reference/legacy',
    '#include\s*[<\"][^>\"]*(SimulationNode|NodeFactory|NodeRegistry|AssemblyContext|MissionAssembler|ConfigNode)',
    'target_(include_directories|link_libraries)\([^\)]*reference[/\\]legacy'
)
foreach ($file in $productionFiles) {
    $text = Get-Content -LiteralPath $file.FullName -Raw -Encoding utf8
    foreach ($pattern in $legacyPatterns) {
        if ($text -match $pattern) {
            Add-Error "Forbidden legacy production dependency in $($file.FullName): $pattern"
        }
    }
}

$cmakeText = Get-Content -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') -Raw -Encoding utf8
if ($cmakeText -match 'target_link_libraries\(kernel[^\)]*compiler') {
    Add-Error "Kernel must not link Compiler."
}

$authoredMarkdown = @(
    (Get-Item -LiteralPath (Join-Path $repoRoot 'README.md'))
    (Get-Item -LiteralPath (Join-Path $repoRoot 'AGENTS.md'))
    (Get-Item -LiteralPath (Join-Path $repoRoot 'CONTRIBUTING.md'))
)
$authoredMarkdown += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'docs') -Recurse -File -Filter '*.md'
$authoredMarkdown += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'framework') -Recurse -File -Filter '*.md'
$authoredMarkdown += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'packages') -Recurse -File -Filter '*.md'
$authoredMarkdown += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'adapters') -Recurse -File -Filter '*.md'
$authoredMarkdown += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'user') -Recurse -File -Filter '*.md'
$authoredMarkdown += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'fixtures') -Recurse -File -Filter '*.md'
$authoredMarkdown += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'oracles') -Recurse -File -Filter '*.md'
$authoredMarkdown += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'specs') -Recurse -File -Filter '*.md'

foreach ($file in $authoredMarkdown | Sort-Object FullName -Unique) {
    $text = Get-Content -LiteralPath $file.FullName -Raw -Encoding utf8
    if ($text.Contains([char]0xFFFD)) {
        Add-Error "UTF-8 replacement character found in $($file.FullName)."
    }
    if ($text -match '不是.{0,80}而是') {
        Add-Error "Forbidden Chinese sentence pattern found in $($file.FullName)."
    }
}

$markdownRoots = @('README.md', 'CONTRIBUTING.md', 'docs', 'fixtures', 'oracles', 'specs', 'design-notes/gnczmkn-architecture-roadmap')
$markdownFiles = @()
foreach ($relativeRoot in $markdownRoots) {
    $absoluteRoot = Join-Path $repoRoot $relativeRoot
    if (Test-Path -LiteralPath $absoluteRoot -PathType Leaf) {
        $markdownFiles += Get-Item -LiteralPath $absoluteRoot
    }
    elseif (Test-Path -LiteralPath $absoluteRoot -PathType Container) {
        $markdownFiles += Get-ChildItem -LiteralPath $absoluteRoot -Recurse -File -Filter '*.md'
    }
}

foreach ($file in $markdownFiles | Sort-Object FullName -Unique) {
    $text = Get-Content -LiteralPath $file.FullName -Raw -Encoding utf8
    foreach ($match in [regex]::Matches($text, '\[[^\]]*\]\(([^)]+)\)')) {
        $target = $match.Groups[1].Value.Trim('<', '>')
        if ($target -match '^(https?://|mailto:|#)') { continue }
        $pathPart = ($target -split '#')[0]
        if ([string]::IsNullOrWhiteSpace($pathPart)) { continue }
        $resolved = [System.IO.Path]::GetFullPath((Join-Path $file.DirectoryName $pathPart))
        if (-not (Test-Path -LiteralPath $resolved)) {
            Add-Error "Broken Markdown link in $($file.FullName): $target"
        }
    }
}

$schemaValidatorPath = Join-Path $PSScriptRoot 'validate-r0-specs.ps1'
$powerShellHost = (Get-Process -Id $PID).Path
$schemaValidatorOutput = & $powerShellHost -NoLogo -NoProfile -ExecutionPolicy Bypass -File $schemaValidatorPath -Quiet 2>&1
if ($LASTEXITCODE -ne 0) {
    Add-Error "R0 schema conformance failed: $($schemaValidatorOutput -join [Environment]::NewLine)"
}

$architectureValidatorPath = Join-Path $PSScriptRoot 'validate-architecture-baseline.ps1'
$architectureValidatorOutput = & $powerShellHost -NoLogo -NoProfile -ExecutionPolicy Bypass -File $architectureValidatorPath -Quiet 2>&1
if ($LASTEXITCODE -ne 0) {
    Add-Error "R0 architecture baseline conformance failed: $($architectureValidatorOutput -join [Environment]::NewLine)"
}

$legacyReproductionValidatorPath = Join-Path $PSScriptRoot 'validate-legacy-reproduction.ps1'
$legacyReproductionValidatorOutput = & $powerShellHost -NoLogo -NoProfile -ExecutionPolicy Bypass -File $legacyReproductionValidatorPath -Quiet 2>&1
if ($LASTEXITCODE -ne 0) {
    Add-Error "R0 legacy reproduction evidence failed: $($legacyReproductionValidatorOutput -join [Environment]::NewLine)"
}

$scientificConventionValidatorPath = Join-Path $PSScriptRoot 'validate-scientific-conventions.ps1'
$scientificConventionValidatorOutput = & $powerShellHost -NoLogo -NoProfile -ExecutionPolicy Bypass -File $scientificConventionValidatorPath -StaticOnly -Quiet 2>&1
if ($LASTEXITCODE -ne 0) {
    Add-Error "R0 scientific convention evidence failed: $($scientificConventionValidatorOutput -join [Environment]::NewLine)"
}

$licenseProvenanceValidatorPath = Join-Path $PSScriptRoot 'validate-license-provenance.ps1'
$licenseProvenanceValidatorOutput = & $powerShellHost -NoLogo -NoProfile -ExecutionPolicy Bypass -File $licenseProvenanceValidatorPath -Quiet 2>&1
if ($LASTEXITCODE -ne 0) {
    Add-Error "R0 license/provenance evidence failed: $($licenseProvenanceValidatorOutput -join [Environment]::NewLine)"
}

$teamToolchainValidatorPath = Join-Path $PSScriptRoot 'validate-team-toolchain.ps1'
$teamToolchainValidatorOutput = & $powerShellHost -NoLogo -NoProfile -ExecutionPolicy Bypass -File $teamToolchainValidatorPath -Quiet 2>&1
if ($LASTEXITCODE -ne 0) {
    Add-Error "R0 team/toolchain readiness evidence failed: $($teamToolchainValidatorOutput -join [Environment]::NewLine)"
}

if ($errors.Count -gt 0) {
    Write-Host "Repository verification failed with $($errors.Count) issue(s):"
    foreach ($errorMessage in $errors) {
        Write-Host " - $errorMessage"
    }
    exit 1
}

$taskCount = if ($null -ne $backlog) { $backlog.tasks.Count } else { 0 }
Write-Host "Repository verification passed."
Write-Host "Validated JSON files: $($jsonFiles.Count)"
Write-Host "Validated task entries: $taskCount"
Write-Host "Validated Markdown files: $($markdownFiles.Count)"
Write-Host "Validated R0 schema contracts: 3"
Write-Host "Validated R0 architecture baseline: 1"
Write-Host "Validated R0 legacy reproduction: 1"
Write-Host "Validated R0 scientific convention bundle: 1"
Write-Host "Validated R0 license/provenance bundle: 1"
Write-Host "Validated R0 team/toolchain readiness bundle: 1"

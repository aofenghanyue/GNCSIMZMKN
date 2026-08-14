[CmdletBinding()]
param(
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$reproductionRoot = Join-Path $repoRoot 'reference\legacy\reproduction'
$errors = [System.Collections.Generic.List[string]]::new()

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

function Test-PathBelow([string]$Candidate, [string]$Root) {
    $separator = [System.IO.Path]::DirectorySeparatorChar
    $prefix = $Root.TrimEnd($separator, [System.IO.Path]::AltDirectorySeparatorChar) + $separator
    return $Candidate.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
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

function Test-Value([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        Add-Error $Message
    }
}

$pointerPath = Join-Path $reproductionRoot 'current.json'
$pointer = Read-Json $pointerPath
if ($null -eq $pointer) {
    Write-Host 'Legacy reproduction validation could not load the current pointer.'
    exit 1
}

Test-Value ($pointer.schema_version -eq 'gnczmkn.legacy-reproduction-pointer/1') `
    'Unsupported legacy reproduction pointer schema.'
Test-Value ($pointer.task_id -eq 'R0-LEG-001') `
    'Legacy reproduction pointer does not belong to R0-LEG-001.'
Test-Value ($pointer.status -eq 'passed') `
    'Current legacy reproduction pointer is not passed.'

$runRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $pointer.path))
if (-not (Test-PathBelow $runRoot $reproductionRoot)) {
    Add-Error "Current reproduction path escapes its evidence root: $runRoot"
}
if ((Split-Path $runRoot -Leaf) -ne $pointer.run_id) {
    Add-Error 'Current reproduction directory name differs from run_id.'
}

$environmentPath = Join-Path $runRoot 'environment-manifest.json'
$testReportPath = Join-Path $runRoot 'test-report.json'
$missionReportPath = Join-Path $runRoot 'mission-report.json'
$evidenceIndexPath = Join-Path $runRoot 'evidence-index.json'
$environment = Read-Json $environmentPath
$testReport = Read-Json $testReportPath
$missionReport = Read-Json $missionReportPath
$evidenceIndex = Read-Json $evidenceIndexPath

if ($null -ne $evidenceIndex) {
    Test-Value ($evidenceIndex.schema_version -eq 'gnczmkn.legacy-evidence-index/1') `
        'Unsupported legacy evidence index schema.'
    Test-Value ($evidenceIndex.run_id -eq $pointer.run_id) `
        'Evidence index run_id differs from the current pointer.'
    Test-Value ($evidenceIndex.status -eq 'passed') `
        'Evidence index is not passed.'

    $indexEntries = @($evidenceIndex.evidence_files)
    Test-Value ($indexEntries.Count -eq 32) `
        "Expected 32 indexed evidence files; found $($indexEntries.Count)."
    $uniquePaths = @($indexEntries.path | Sort-Object -Unique)
    Test-Value ($uniquePaths.Count -eq $indexEntries.Count) `
        'Evidence index contains duplicate paths.'

    foreach ($entry in $indexEntries) {
        $candidate = [System.IO.Path]::GetFullPath((Join-Path $runRoot $entry.path))
        if (-not (Test-PathBelow $candidate $runRoot)) {
            Add-Error "Evidence path escapes the run directory: $($entry.path)"
            continue
        }
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            Add-Error "Indexed evidence file is missing: $($entry.path)"
            continue
        }
        $actualBytes = (Get-Item -LiteralPath $candidate).Length
        $actualHash = Get-Sha256 $candidate
        if ($actualBytes -ne $entry.bytes) {
            Add-Error "Indexed byte count differs for $($entry.path)."
        }
        if ($actualHash -ne $entry.sha256) {
            Add-Error "Indexed SHA-256 differs for $($entry.path)."
        }
    }

    foreach ($requiredEntry in @(
            'environment-manifest.json',
            'test-report.json',
            'mission-report.json',
            'logs/legacy-main-configure.log',
            'logs/legacy-main-build.log',
            'logs/legacy-ctest-all.log'
        )) {
        Test-Value ($requiredEntry -in $uniquePaths) `
            "Required evidence is not indexed: $requiredEntry"
    }
}

$sourceManifestPath = Join-Path $repoRoot 'reference\legacy\source-manifest.json'
$sourceManifest = Read-Json $sourceManifestPath
if ($null -ne $environment -and $null -ne $sourceManifest) {
    Test-Value ($environment.schema_version -eq 'gnczmkn.legacy-environment/1') `
        'Unsupported legacy environment manifest schema.'
    Test-Value ($environment.run_id -eq $pointer.run_id) `
        'Environment manifest run_id differs from the current pointer.'
    Test-Value ($environment.source.git_head -eq $sourceManifest.git.head) `
        'Environment source commit differs from the frozen manifest.'
    Test-Value ($environment.source.archive_sha256 -eq $sourceManifest.archive.sha256) `
        'Environment archive hash differs from the frozen manifest.'

    $legacyArchivePath = Join-Path $repoRoot $sourceManifest.archive.path
    if (Test-Path -LiteralPath $legacyArchivePath -PathType Leaf) {
        Test-Value ((Get-Sha256 $legacyArchivePath) -eq $environment.source.archive_sha256) `
            'Checked-in legacy archive differs from the reproduction environment.'
    }

    Test-Value ($environment.source.mutation_check -eq 'passed') `
        'Legacy source mutation check is not passed.'
    Test-Value ($environment.source.fingerprint_before.file_count -eq 390) `
        'Legacy source fingerprint does not contain the expected 390 files.'
    Test-Value ($environment.source.fingerprint_before.file_count -eq
        $environment.source.fingerprint_after.file_count) `
        'Legacy source file count changed during reproduction.'
    Test-Value ($environment.source.fingerprint_before.sha256 -eq
        $environment.source.fingerprint_after.sha256) `
        'Legacy source fingerprint changed during reproduction.'
    Test-Value ($environment.source.fingerprint_before.sha256 -eq
        'ce443a79dc491326de45f4bfcbb9332ba5d2d1fcf7b69bdabaf2bd2df002feb1') `
        'Legacy source aggregate hash differs from the accepted baseline.'
    Test-Value ((@($environment.source.fingerprint_before.excluded_paths) -join ',') -eq
        'user/outputs/**,test_outputs/**') `
        'Legacy source fingerprint exclusions changed.'

    Test-Value ($environment.dependencies.w64devkit.version -eq '2.9.1') `
        'Unexpected w64devkit version.'
    Test-Value ($environment.dependencies.w64devkit.archive_sha256 -eq
        '9208c19755cd4964b7915b9afcf02c66d493a4c870c4b3e83f6c538d9c1237a5') `
        'Unexpected w64devkit archive hash.'
    Test-Value ($environment.dependencies.w64devkit.gcc -match '16\.2\.0') `
        'Unexpected GCC version.'
    Test-Value ($environment.dependencies.w64devkit.cmake -match '4\.4\.2') `
        'Unexpected CMake version.'
    Test-Value ($environment.dependencies.w64devkit.ninja -eq '1.13.2') `
        'Unexpected Ninja version.'
    Test-Value ($environment.dependencies.eigen.version -eq '3.4.0') `
        'Unexpected Eigen version.'
    Test-Value ($environment.dependencies.eigen.archive_sha256 -eq
        'eba3f3d414d2f8cba2919c78ec6daab08fc71ba2ba4ae502b7e5d4d99fc02cda') `
        'Unexpected Eigen archive hash.'

    Test-Value ($environment.build.generator -eq 'Ninja') `
        'Legacy reproduction generator is not Ninja.'
    Test-Value ($environment.build.build_type -eq 'Release') `
        'Legacy reproduction build type is not Release.'
    Test-Value ($environment.build.cxx_standard -eq 17) `
        'Legacy reproduction did not record C++17.'
    Test-Value (@($environment.build.artifacts).Count -eq 3) `
        'Expected three active-project executable artifacts.'
    foreach ($artifact in @($environment.build.artifacts)) {
        Test-Value (-not [string]::IsNullOrWhiteSpace([string]$artifact.sha256)) `
            "Build artifact has no hash: $($artifact.active_project)"
        Test-Value ($artifact.bytes -gt 0) `
            "Build artifact has no bytes: $($artifact.active_project)"
    }
}

if ($null -ne $testReport) {
    Test-Value ($testReport.schema_version -eq 'gnczmkn.legacy-test-report/1') `
        'Unsupported legacy test report schema.'
    Test-Value ($testReport.run_id -eq $pointer.run_id) `
        'Test report run_id differs from the current pointer.'
    Test-Value ($testReport.status -eq 'passed') `
        'Legacy test report is not passed.'
    Test-Value ($testReport.configured -eq 27 -and
        $testReport.passed -eq 27 -and $testReport.failed -eq 0) `
        'Legacy test totals are not 27/27/0.'
    Test-Value ($testReport.labels.core -eq 18) 'Legacy core label count differs from 18.'
    Test-Value ($testReport.labels.example -eq 6) 'Legacy example label count differs from 6.'
    Test-Value ($testReport.labels.project -eq 2) 'Legacy project label count differs from 2.'
    Test-Value ($testReport.labels.'architecture-guard' -eq 1) `
        'Legacy architecture-guard label count differs from 1.'
    Test-Value ($testReport.historical_baseline.configured -eq 25 -and
        $testReport.historical_baseline.passed -eq 23 -and
        $testReport.historical_baseline.failed -eq 2) `
        'Imported historical test baseline changed.'
    Test-Value ($testReport.path_sensitive_comparison.clean_ctest -eq 'passed') `
        'Path-sensitive tests did not pass through clean CTest.'

    $directRuns = @($testReport.path_sensitive_comparison.direct_source_root_runs)
    Test-Value ($directRuns.Count -eq 2) 'Expected two direct path-sensitive reruns.'
    foreach ($directRun in $directRuns) {
        Test-Value ($directRun.exit_code -eq 0) `
            "Direct rerun failed: $($directRun.test)"
        Test-Value ($directRun.working_directory -eq 'legacy-source-root') `
            "Direct rerun used an unexpected working directory: $($directRun.test)"
    }
}

$expectedMissionHashes = @{
    'geographic-3dof' = '8f62c3f9f8c2f06a2d5e9baa9f61b42fad21fd8c683cf44005cb8cc8a655ee37'
    'cartesian-3dof' = '8c42c511804021536650a0f0b9c2211681b0f6ff5688734cea4a55065d3f60a7'
    'cartesian-6dof' = '685cd0c25ce56ad0061de6e740c6e822a09059212214b2bd55b0c9e51ed9f0c0'
    'cavh-geographic-3dof' = 'e3ada3dfdf3ef57eaacb1df59dcc9e75d94d67c0436e9ae9438f8d70d6dba6b1'
    'yyz-cartesian-6dof' = 'fe8b60dffd65635d9a7d330f1d7a20dda2e0666ecc56f39711d5db1e68eec0e2'
}
if ($null -ne $missionReport) {
    Test-Value ($missionReport.schema_version -eq 'gnczmkn.legacy-mission-report/1') `
        'Unsupported legacy mission report schema.'
    Test-Value ($missionReport.run_id -eq $pointer.run_id) `
        'Mission report run_id differs from the current pointer.'
    Test-Value ($missionReport.status -eq 'passed') `
        'Legacy mission report is not passed.'
    $missions = @($missionReport.missions)
    Test-Value ($missionReport.mission_count -eq 5 -and $missions.Count -eq 5) `
        'Expected five representative missions.'
    Test-Value (@($missions.id | Sort-Object -Unique).Count -eq 5) `
        'Representative mission ids are not unique.'

    foreach ($mission in $missions) {
        if (-not $expectedMissionHashes.ContainsKey($mission.id)) {
            Add-Error "Unexpected representative mission id: $($mission.id)"
            continue
        }
        Test-Value ($mission.status -eq 'passed' -and $mission.deterministic -eq $true) `
            "Mission is not passed and deterministic: $($mission.id)"
        Test-Value (@($mission.comparisons).Count -eq 2) `
            "Mission does not contain two artifact comparisons: $($mission.id)"
        foreach ($comparison in @($mission.comparisons)) {
            Test-Value ($comparison.matches -eq $true) `
                "Mission comparison failed: $($mission.id)/$($comparison.artifact)"
        }

        $runs = @($mission.runs)
        Test-Value ($runs.Count -eq 2) "Mission was not run twice: $($mission.id)"
        if ($runs.Count -ne 2) { continue }
        foreach ($run in $runs) {
            Test-Value ($run.exit_code -eq 0) `
                "Mission run returned non-zero: $($mission.id)/$($run.pass)"
            Test-Value (@($run.artifacts).Count -eq 2) `
                "Mission run does not contain two artifacts: $($mission.id)/$($run.pass)"
        }

        $firstCsv = @($runs[0].artifacts | Where-Object { $_.path -match '\.csv$' })
        $secondCsv = @($runs[1].artifacts | Where-Object { $_.path -match '\.csv$' })
        Test-Value ($firstCsv.Count -eq 1 -and $secondCsv.Count -eq 1) `
            "Mission does not contain one CSV per run: $($mission.id)"
        if ($firstCsv.Count -eq 1 -and $secondCsv.Count -eq 1) {
            $expectedHash = $expectedMissionHashes[$mission.id]
            Test-Value ($firstCsv[0].sha256 -eq $expectedHash -and
                $secondCsv[0].sha256 -eq $expectedHash) `
                "Mission CSV hash differs from the accepted baseline: $($mission.id)"
        }

        $firstSummary = @($runs[0].artifacts | Where-Object { $_.path -match '/summary\.txt$' })
        $secondSummary = @($runs[1].artifacts | Where-Object { $_.path -match '/summary\.txt$' })
        Test-Value ($firstSummary.Count -eq 1 -and $secondSummary.Count -eq 1) `
            "Mission does not contain one summary per run: $($mission.id)"
        if ($firstSummary.Count -eq 1 -and $secondSummary.Count -eq 1) {
            Test-Value (-not [string]::IsNullOrWhiteSpace(
                    [string]$firstSummary[0].normalized_sha256)) `
                "Mission summary has no normalized hash: $($mission.id)"
            Test-Value ($firstSummary[0].normalized_sha256 -eq
                $secondSummary[0].normalized_sha256) `
                "Mission normalized summaries differ: $($mission.id)"
        }
    }
}

$compatibilityPath = Join-Path $reproductionRoot 'compatibility-msvc-19.50.json'
$compatibility = Read-Json $compatibilityPath
if ($null -ne $compatibility -and $null -ne $sourceManifest) {
    Test-Value ($compatibility.schema_version -eq 'gnczmkn.legacy-compatibility-gap/1') `
        'Unsupported legacy compatibility gap schema.'
    Test-Value ($compatibility.status -eq 'documented_gap') `
        'MSVC compatibility gap is not documented.'
    Test-Value ($compatibility.source_git_head -eq $sourceManifest.git.head) `
        'MSVC compatibility attempt used a different source commit.'
    Test-Value ($compatibility.results.configure_exit_code -eq 0 -and
        $compatibility.results.build_exit_code -eq 1) `
        'MSVC compatibility result does not record configure-pass/build-fail.'
    Test-Value ($compatibility.source_mutation -eq 'none') `
        'MSVC compatibility attempt records a source mutation.'
    $compatibilityLog = [System.IO.Path]::GetFullPath(
        (Join-Path $repoRoot $compatibility.evidence_log))
    if (-not (Test-PathBelow $compatibilityLog $reproductionRoot) -or
        -not (Test-Path -LiteralPath $compatibilityLog -PathType Leaf)) {
        Add-Error 'MSVC compatibility log is missing or outside the evidence root.'
    }
    else {
        Test-Value ((Get-Sha256 $compatibilityLog) -eq $compatibility.evidence_log_sha256) `
            'MSVC compatibility log hash differs from its report.'
    }
}

if ($errors.Count -gt 0) {
    Write-Host "Legacy reproduction validation failed with $($errors.Count) issue(s):"
    foreach ($errorMessage in $errors) {
        Write-Host " - $errorMessage"
    }
    exit 1
}

if (-not $Quiet) {
    Write-Host "Legacy reproduction evidence passed for run '$($pointer.run_id)'."
    Write-Host 'Validated evidence files: 32'
    Write-Host 'Validated tests: 27/27'
    Write-Host 'Validated representative missions: 5/5 (two runs each)'
}

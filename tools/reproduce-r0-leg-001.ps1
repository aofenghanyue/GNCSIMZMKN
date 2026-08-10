[CmdletBinding()]
param(
    [string]$RunId = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ'),

    [ValidateSet('MinGW Makefiles', 'Ninja')]
    [string]$Generator = 'MinGW Makefiles',

    [ValidateRange(1, 64)]
    [int]$Jobs = 4
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$script:CommandRecords = New-Object System.Collections.ArrayList
$script:EnvironmentGaps = New-Object System.Collections.ArrayList

function Write-Utf8File {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content
    )

    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    [System.IO.File]::WriteAllText($Path, $Content, $script:Utf8NoBom)
}

function Append-Utf8File {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content
    )

    [System.IO.File]::AppendAllText($Path, $Content, $script:Utf8NoBom)
}

function Format-CommandArgument {
    param([AllowEmptyString()][string]$Value)

    if ($Value -eq '') {
        return '""'
    }
    if ($Value -match '[\s"]') {
        return '"' + $Value.Replace('"', '\"') + '"'
    }
    return $Value
}

function Add-CommandRecord {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][int]$ExitCode,
        [Parameter(Mandatory = $true)][string]$Log,
        [Parameter(Mandatory = $true)][datetime]$StartedAtUtc,
        [Parameter(Mandatory = $true)][datetime]$FinishedAtUtc
    )

    [void]$script:CommandRecords.Add([ordered]@{
        name = $Name
        command = $Command
        working_directory = $WorkingDirectory
        exit_code = $ExitCode
        started_at_utc = $StartedAtUtc.ToString('o')
        finished_at_utc = $FinishedAtUtc.ToString('o')
        duration_seconds = [math]::Round(($FinishedAtUtc - $StartedAtUtc).TotalSeconds, 3)
        log = $Log
    })
}

function Add-EnvironmentGap {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [Parameter(Mandatory = $true)][string]$Stage,
        [Parameter(Mandatory = $true)][string]$Classification,
        [Parameter(Mandatory = $true)][string]$Detail,
        [Parameter(Mandatory = $true)][string]$Impact,
        [Parameter(Mandatory = $true)][string]$Disposition
    )

    [void]$script:EnvironmentGaps.Add([ordered]@{
        id = $Id
        stage = $Stage
        classification = $Classification
        detail = $Detail
        impact = $Impact
        disposition = $Disposition
    })
}

function Invoke-LoggedNative {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [switch]$AllowFailure
    )

    $displayArguments = @($Arguments | ForEach-Object { Format-CommandArgument $_ })
    $commandParts = @((Format-CommandArgument $FilePath)) + $displayArguments
    $command = ($commandParts -join ' ').Trim()
    $started = (Get-Date).ToUniversalTime()
    Write-Utf8File -Path $LogPath -Content (
        "name: $Name`n" +
        "started_at_utc: $($started.ToString('o'))`n" +
        "working_directory: $WorkingDirectory`n" +
        "command: $command`n" +
        "--- output ---`n"
    )

    $exitCode = -1
    Push-Location $WorkingDirectory
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        # Native compilers routinely write diagnostics to stderr. Keep the
        # complete stream and classify success from the native exit code.
        $ErrorActionPreference = 'Continue'
        $LASTEXITCODE = 0
        & $FilePath @Arguments 2>&1 | ForEach-Object {
            $line = $_.ToString()
            Append-Utf8File -Path $LogPath -Content ($line + [Environment]::NewLine)
        }
        $exitCode = $LASTEXITCODE
    }
    catch {
        Append-Utf8File -Path $LogPath -Content ("native invocation error: $($_.Exception.Message)`n")
        $exitCode = -1
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
        Pop-Location
    }

    $finished = (Get-Date).ToUniversalTime()
    Append-Utf8File -Path $LogPath -Content (
        "--- result ---`n" +
        "exit_code: $exitCode`n" +
        "finished_at_utc: $($finished.ToString('o'))`n"
    )
    Add-CommandRecord `
        -Name $Name `
        -Command $command `
        -WorkingDirectory $WorkingDirectory `
        -ExitCode $exitCode `
        -Log $LogPath `
        -StartedAtUtc $started `
        -FinishedAtUtc $finished

    $result = [pscustomobject]@{
        Name = $Name
        ExitCode = $exitCode
        LogPath = $LogPath
        StartedAtUtc = $started
        FinishedAtUtc = $finished
    }

    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw "Command '$Name' failed with exit code $exitCode. See $LogPath"
    }
    return $result
}

function Get-ToolIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [string[]]$VersionArguments = @('--version')
    )

    $command = Get-Command $Name -ErrorAction Stop
    $output = & $command.Source @VersionArguments 2>&1
    $exitCode = $LASTEXITCODE
    return [ordered]@{
        path = $command.Source
        exit_code = $exitCode
        version_output = (@($output | ForEach-Object { $_.ToString() }) -join "`n").Trim()
    }
}

function Get-TreeHashRecord {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$HashListPath
    )

    $files = @(Get-ChildItem -LiteralPath $Root -Recurse -File | Sort-Object FullName)
    $lines = New-Object System.Collections.Generic.List[string]
    [long]$bytes = 0
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $lines.Add("$hash  $relative")
        $bytes += $file.Length
    }
    Write-Utf8File -Path $HashListPath -Content (($lines -join "`n") + "`n")
    return [ordered]@{
        root = $Root
        file_count = $files.Count
        total_bytes = $bytes
        hash_list = $HashListPath
        tree_digest_sha256 = (Get-FileHash -LiteralPath $HashListPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Copy-EvidenceTree {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        return @()
    }

    $copied = New-Object System.Collections.Generic.List[string]
    foreach ($file in @(Get-ChildItem -LiteralPath $Source -Recurse -File | Sort-Object FullName)) {
        $relative = $file.FullName.Substring($Source.Length).TrimStart('\')
        $target = Join-Path $Destination $relative
        $targetParent = Split-Path -Parent $target
        if (-not (Test-Path -LiteralPath $targetParent)) {
            New-Item -ItemType Directory -Path $targetParent | Out-Null
        }
        Copy-Item -LiteralPath $file.FullName -Destination $target
        $copied.Add($target)
    }
    return $copied.ToArray()
}

function Convert-ToRepositoryPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$RepositoryRoot
    )

    $absolute = [System.IO.Path]::GetFullPath($Path)
    if ($absolute.StartsWith($RepositoryRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $absolute.Substring($RepositoryRoot.Length).TrimStart('\').Replace('\', '/')
    }
    return $absolute.Replace('\', '/')
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$runRoot = Join-Path $repoRoot ("build\r0-leg-001\runs\$RunId")
$evidenceRoot = Join-Path $repoRoot ("docs\quality\r0-leg-001\runs\$RunId")
$logsRoot = Join-Path $evidenceRoot 'logs'
$artifactsRoot = Join-Path $evidenceRoot 'artifacts'

if (Test-Path -LiteralPath $runRoot) {
    throw "Run directory already exists; choose a new RunId: $runRoot"
}
if (Test-Path -LiteralPath $evidenceRoot) {
    throw "Evidence directory already exists; choose a new RunId: $evidenceRoot"
}

New-Item -ItemType Directory -Path $runRoot | Out-Null
New-Item -ItemType Directory -Path $logsRoot | Out-Null
New-Item -ItemType Directory -Path $artifactsRoot | Out-Null

$manifestPath = Join-Path $repoRoot 'reference\legacy\source-manifest.json'
$checksumPath = Join-Path $repoRoot 'reference\legacy\legacy-source.sha256'
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json
$archivePath = Join-Path $repoRoot $manifest.archive.path
$expectedManifestHash = $manifest.archive.sha256.ToLowerInvariant()
$expectedChecksumHash = ((Get-Content -LiteralPath $checksumPath -Raw -Encoding utf8).Trim() -split '\s+')[0].ToLowerInvariant()
$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()

$archiveLog = Join-Path $logsRoot '01-archive-verification.log'
$archiveStarted = (Get-Date).ToUniversalTime()
$archiveVerified = ($archiveHash -eq $expectedManifestHash -and $archiveHash -eq $expectedChecksumHash)
$archiveFinished = (Get-Date).ToUniversalTime()
Write-Utf8File -Path $archiveLog -Content (
    "archive: $archivePath`n" +
    "manifest_hash: $expectedManifestHash`n" +
    "checksum_file_hash: $expectedChecksumHash`n" +
    "actual_hash: $archiveHash`n" +
    "verified: $archiveVerified`n"
)
Add-CommandRecord `
    -Name 'archive-verification' `
    -Command "Get-FileHash -Algorithm SHA256 $(Format-CommandArgument $archivePath)" `
    -WorkingDirectory $repoRoot `
    -ExitCode $(if ($archiveVerified) { 0 } else { 1 }) `
    -Log $archiveLog `
    -StartedAtUtc $archiveStarted `
    -FinishedAtUtc $archiveFinished

$fatalMessage = $null
$status = 'failed'
$testExtraction = $null
$missionExtraction = $null
$testStats = [ordered]@{
    expected_total = 25
    expected_passed = 23
    expected_failed = 2
    expected_path_sensitive_failures = @(
        'test_ideal_cartesian_3dof_mission',
        'test_ideal_cartesian_6dof_mission'
    )
    observed_total = $null
    observed_passed = $null
    observed_failed = $null
    observed_failed_tests = @()
    ctest_exit_code = $null
    direct_reruns = @()
    baseline_matched = $false
    acceptance_satisfied = $false
}
$representativeRuns = New-Object System.Collections.ArrayList
$eigen3Directory = $null

try {
    if (-not $archiveVerified) {
        Add-EnvironmentGap `
            -Id 'LEG-F01' `
            -Stage 'archive-verification' `
            -Classification 'input-identity-mismatch' `
            -Detail "Archive hash $archiveHash does not match both recorded hashes." `
            -Impact 'Clean extraction is not authorized.' `
            -Disposition 'Replace or recapture the frozen archive through repository governance.'
        throw 'Legacy archive verification failed.'
    }

    $testExtractRoot = Join-Path $runRoot 'test-extraction'
    $missionExtractRoot = Join-Path $runRoot 'mission-extraction'
    $extractionLog = Join-Path $logsRoot '02-extraction.log'
    $extractionStarted = (Get-Date).ToUniversalTime()
    Expand-Archive -LiteralPath $archivePath -DestinationPath $testExtractRoot
    Expand-Archive -LiteralPath $archivePath -DestinationPath $missionExtractRoot
    $prefixDirectory = $manifest.archive.prefix.TrimEnd('/').Replace('/', '\')
    $testSource = Join-Path $testExtractRoot $prefixDirectory
    $missionSource = Join-Path $missionExtractRoot $prefixDirectory
    if (-not (Test-Path -LiteralPath $testSource -PathType Container) -or
        -not (Test-Path -LiteralPath $missionSource -PathType Container)) {
        throw "Expected archive prefix is missing after extraction: $($manifest.archive.prefix)"
    }
    $testTopLevel = @(Get-ChildItem -LiteralPath $testExtractRoot)
    $missionTopLevel = @(Get-ChildItem -LiteralPath $missionExtractRoot)
    if ($testTopLevel.Count -ne 1 -or $missionTopLevel.Count -ne 1) {
        throw 'Clean extraction produced an unexpected top-level inventory.'
    }
    $testExtraction = Get-TreeHashRecord `
        -Root $testSource `
        -HashListPath (Join-Path $evidenceRoot 'test-clean-files.sha256')
    $missionExtraction = Get-TreeHashRecord `
        -Root $missionSource `
        -HashListPath (Join-Path $evidenceRoot 'mission-clean-files.sha256')
    $extractionMatch = ($testExtraction.tree_digest_sha256 -eq $missionExtraction.tree_digest_sha256)
    $extractionFinished = (Get-Date).ToUniversalTime()
    Write-Utf8File -Path $extractionLog -Content (
        "archive_prefix: $($manifest.archive.prefix)`n" +
        "test_source: $testSource`n" +
        "mission_source: $missionSource`n" +
        "test_file_count: $($testExtraction.file_count)`n" +
        "mission_file_count: $($missionExtraction.file_count)`n" +
        "test_tree_digest_sha256: $($testExtraction.tree_digest_sha256)`n" +
        "mission_tree_digest_sha256: $($missionExtraction.tree_digest_sha256)`n" +
        "matching_clean_extractions: $extractionMatch`n"
    )
    Add-CommandRecord `
        -Name 'clean-extraction' `
        -Command "Expand-Archive twice from $(Format-CommandArgument $archivePath)" `
        -WorkingDirectory $repoRoot `
        -ExitCode $(if ($extractionMatch) { 0 } else { 1 }) `
        -Log $extractionLog `
        -StartedAtUtc $extractionStarted `
        -FinishedAtUtc $extractionFinished
    if (-not $extractionMatch) {
        throw 'The two clean extractions have different tree digests.'
    }

    $toolIdentities = [ordered]@{
        cmake = Get-ToolIdentity -Name 'cmake'
        ctest = Get-ToolIdentity -Name 'ctest'
        cxx = Get-ToolIdentity -Name 'g++'
        make = Get-ToolIdentity -Name 'mingw32-make'
        ninja = Get-ToolIdentity -Name 'ninja'
    }
    $toolProbeText = New-Object System.Collections.Generic.List[string]
    foreach ($entry in $toolIdentities.GetEnumerator()) {
        $toolProbeText.Add("[$($entry.Key)]")
        $toolProbeText.Add("path=$($entry.Value.path)")
        $toolProbeText.Add("exit_code=$($entry.Value.exit_code)")
        $toolProbeText.Add($entry.Value.version_output)
        $toolProbeText.Add('')
    }
    Write-Utf8File -Path (Join-Path $logsRoot '00-environment-probes.log') -Content (($toolProbeText -join "`n") + "`n")

    $testBuild = Join-Path $runRoot 'build-tests'
    $configureArgs = @(
        '-S', $testSource,
        '-B', $testBuild,
        '-G', $Generator,
        '-DBUILD_TESTS=ON',
        '-DGNC_BUILD_EXAMPLE_TESTS=ON',
        '-DGNC_BUILD_PROJECT_TESTS=OFF',
        '-DGNC_BUILD_ARCHITECTURE_GUARDS=ON',
        '-DCMAKE_BUILD_TYPE=Release'
    )
    $configure = Invoke-LoggedNative `
        -Name 'legacy-configure-25-tests' `
        -FilePath $toolIdentities.cmake.path `
        -Arguments $configureArgs `
        -WorkingDirectory $repoRoot `
        -LogPath (Join-Path $logsRoot '03-configure.log')

    $cachePath = Join-Path $testBuild 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cachePath) {
        $eigenMatch = Select-String -LiteralPath $cachePath -Pattern '^Eigen3_DIR:PATH=(.*)$'
        if ($eigenMatch) {
            $eigen3Directory = $eigenMatch.Matches[0].Groups[1].Value
        }
    }

    $build = Invoke-LoggedNative `
        -Name 'legacy-build-25-tests' `
        -FilePath $toolIdentities.cmake.path `
        -Arguments @('--build', $testBuild, '--parallel', $Jobs.ToString()) `
        -WorkingDirectory $repoRoot `
        -LogPath (Join-Path $logsRoot '04-build.log')

    $ctest = Invoke-LoggedNative `
        -Name 'legacy-ctest-25-tests' `
        -FilePath $toolIdentities.ctest.path `
        -Arguments @('--test-dir', $testBuild, '--output-on-failure', '--no-tests=error') `
        -WorkingDirectory $repoRoot `
        -LogPath (Join-Path $logsRoot '05-ctest.log') `
        -AllowFailure
    $testStats.ctest_exit_code = $ctest.ExitCode

    $lastTestLog = Join-Path $testBuild 'Testing\Temporary\LastTest.log'
    if (Test-Path -LiteralPath $lastTestLog) {
        Copy-Item -LiteralPath $lastTestLog -Destination (Join-Path $logsRoot '05-ctest-lasttest.log')
    }

    $ctestText = Get-Content -LiteralPath $ctest.LogPath -Raw -Encoding utf8
    $summaryMatch = [regex]::Match(
        $ctestText,
        '(?m)(\d+)% tests passed,\s+(\d+) tests failed out of\s+(\d+)'
    )
    if ($summaryMatch.Success) {
        $testStats.observed_failed = [int]$summaryMatch.Groups[2].Value
        $testStats.observed_total = [int]$summaryMatch.Groups[3].Value
        $testStats.observed_passed = $testStats.observed_total - $testStats.observed_failed
    }
    $failedMatches = [regex]::Matches(
        $ctestText,
        '(?m)^\s*\d+\s+-\s+([^\s]+)\s+\(Failed\)'
    )
    $testStats.observed_failed_tests = @($failedMatches | ForEach-Object { $_.Groups[1].Value })

    $directResults = New-Object System.Collections.ArrayList
    foreach ($testName in $testStats.expected_path_sensitive_failures) {
        $testExecutable = Join-Path $testBuild ("bin\$testName.exe")
        $direct = Invoke-LoggedNative `
            -Name "direct-$testName" `
            -FilePath $testExecutable `
            -Arguments @() `
            -WorkingDirectory $testSource `
            -LogPath (Join-Path $logsRoot ("06-direct-$testName.log")) `
            -AllowFailure
        [void]$directResults.Add([ordered]@{
            test = $testName
            working_directory = $testSource
            exit_code = $direct.ExitCode
            log = $direct.LogPath
        })
    }
    $testStats.direct_reruns = $directResults.ToArray()

    $expectedFailures = @($testStats.expected_path_sensitive_failures | Sort-Object)
    $observedFailures = @($testStats.observed_failed_tests | Sort-Object)
    $failureNamesMatch = (@(Compare-Object $expectedFailures $observedFailures).Count -eq 0)
    $directRerunsPass = (@($testStats.direct_reruns | Where-Object { $_.exit_code -ne 0 }).Count -eq 0)
    $testStats.baseline_matched = (
        $testStats.observed_total -eq $testStats.expected_total -and
        $testStats.observed_passed -eq $testStats.expected_passed -and
        $testStats.observed_failed -eq $testStats.expected_failed -and
        $failureNamesMatch -and
        $directRerunsPass
    )
    $allTestsPassed = (
        $testStats.observed_total -eq $testStats.expected_total -and
        $testStats.observed_passed -eq $testStats.expected_total -and
        $testStats.observed_failed -eq 0 -and
        $ctest.ExitCode -eq 0
    )
    $testStats.acceptance_satisfied = (
        $directRerunsPass -and ($testStats.baseline_matched -or $allTestsPassed)
    )

    if ($testStats.baseline_matched) {
        Add-EnvironmentGap `
            -Id 'LEG-GAP-01' `
            -Stage 'ctest' `
            -Classification 'legacy-working-directory-sensitivity' `
            -Detail 'CTest reproduces the two recorded Cartesian mission failures; direct executable runs from the extracted source root return exit code 0.' `
            -Impact 'The 25-test CTest aggregate remains 23 passed and 2 failed when launched with the legacy CTest working directories.' `
            -Disposition 'Preserve both CTest and direct-rerun evidence. R0-LEG-002 may convert the behavior into path-independent fixtures.'
    }
    elseif ($allTestsPassed) {
        Add-EnvironmentGap `
            -Id 'LEG-GAP-02' `
            -Stage 'ctest' `
            -Classification 'baseline-difference' `
            -Detail 'The clean archive produced 25/25 CTest passes. The imported existing-tree report recorded 23/25 with two Cartesian output-path failures.' `
            -Impact 'The two imported path-sensitive failures do not reproduce on this clean host/toolchain run.' `
            -Disposition 'Preserve both outcomes. Use this clean-run evidence for host reproduction and keep the imported report as historical evidence.'
    }
    else {
        Add-EnvironmentGap `
            -Id 'LEG-GAP-02' `
            -Stage 'ctest' `
            -Classification 'unexpected-baseline-difference' `
            -Detail "Observed total/pass/fail: $($testStats.observed_total)/$($testStats.observed_passed)/$($testStats.observed_failed); failures: $($testStats.observed_failed_tests -join ', ')." `
            -Impact 'The imported baseline was not reproduced and the clean suite is not fully passing.' `
            -Disposition 'Keep the raw logs and investigate before using this host as a legacy oracle producer.'
    }

    $baselineBuild = Join-Path $runRoot 'build-mission-baseline'
    $baselineConfigure = Invoke-LoggedNative `
        -Name 'baseline-mission-configure' `
        -FilePath $toolIdentities.cmake.path `
        -Arguments @(
            '-S', $missionSource,
            '-B', $baselineBuild,
            '-G', $Generator,
            '-DBUILD_TESTS=OFF',
            '-DGNC_ACTIVE_PROJECT=example_05_ideal_3dof_geographic_baseline',
            '-DCMAKE_BUILD_TYPE=Release'
        ) `
        -WorkingDirectory $repoRoot `
        -LogPath (Join-Path $logsRoot '10-baseline-configure.log')
    $baselineBuildResult = Invoke-LoggedNative `
        -Name 'baseline-mission-build' `
        -FilePath $toolIdentities.cmake.path `
        -Arguments @('--build', $baselineBuild, '--target', 'gnc_sim', '--parallel', $Jobs.ToString()) `
        -WorkingDirectory $repoRoot `
        -LogPath (Join-Path $logsRoot '11-baseline-build.log')
    $baselineRun = Invoke-LoggedNative `
        -Name 'baseline-geographic-3dof-run' `
        -FilePath (Join-Path $baselineBuild 'bin\gnc_sim.exe') `
        -Arguments @('--config', 'user/example_05_ideal_3dof_geographic_baseline/config/mission.json') `
        -WorkingDirectory $missionSource `
        -LogPath (Join-Path $logsRoot '12-baseline-run.log') `
        -AllowFailure
    $baselineArtifactSource = Join-Path $missionSource 'user\outputs\example_05_ideal_3dof_geographic_baseline'
    $baselineArtifacts = @(Copy-EvidenceTree `
        -Source $baselineArtifactSource `
        -Destination (Join-Path $artifactsRoot 'example_05_ideal_3dof_geographic_baseline'))
    [void]$representativeRuns.Add([ordered]@{
        id = 'example_05_ideal_3dof_geographic_baseline'
        source_commit = $manifest.git.head
        config = 'user/example_05_ideal_3dof_geographic_baseline/config/mission.json'
        working_directory = $missionSource
        exit_code = $baselineRun.ExitCode
        log = $baselineRun.LogPath
        captured_artifacts = @($baselineArtifacts | ForEach-Object {
            Convert-ToRepositoryPath -Path $_ -RepositoryRoot $repoRoot
        })
    })
    if ($baselineRun.ExitCode -ne 0 -or $baselineArtifacts.Count -eq 0) {
        Add-EnvironmentGap `
            -Id 'LEG-GAP-03' `
            -Stage 'representative-baseline-mission' `
            -Classification 'mission-reproduction-failure' `
            -Detail "Exit code $($baselineRun.ExitCode); captured artifact count $($baselineArtifacts.Count)." `
            -Impact 'The geographic 3DoF representative output is unavailable or incomplete.' `
            -Disposition 'Preserve the log and output inventory; keep the task open.'
    }

    $yyzBuild = Join-Path $runRoot 'build-mission-yyz'
    $yyzConfigure = Invoke-LoggedNative `
        -Name 'yyz-mission-configure' `
        -FilePath $toolIdentities.cmake.path `
        -Arguments @(
            '-S', $missionSource,
            '-B', $yyzBuild,
            '-G', $Generator,
            '-DBUILD_TESTS=OFF',
            '-DGNC_ACTIVE_PROJECT=yyz_cartesian_6dof_framework_9',
            '-DCMAKE_BUILD_TYPE=Release'
        ) `
        -WorkingDirectory $repoRoot `
        -LogPath (Join-Path $logsRoot '20-yyz-configure.log')
    $yyzBuildResult = Invoke-LoggedNative `
        -Name 'yyz-mission-build' `
        -FilePath $toolIdentities.cmake.path `
        -Arguments @('--build', $yyzBuild, '--target', 'gnc_sim', '--parallel', $Jobs.ToString()) `
        -WorkingDirectory $repoRoot `
        -LogPath (Join-Path $logsRoot '21-yyz-build.log')
    $yyzRun = Invoke-LoggedNative `
        -Name 'yyz-cartesian-6dof-run' `
        -FilePath (Join-Path $yyzBuild 'bin\gnc_sim.exe') `
        -Arguments @('--config', 'user/yyz_cartesian_6dof_framework_9/config/mission.json') `
        -WorkingDirectory $missionSource `
        -LogPath (Join-Path $logsRoot '22-yyz-run.log') `
        -AllowFailure
    $yyzArtifactSource = Join-Path $missionSource 'user\outputs\yyz_cartesian_6dof_framework_9'
    $yyzArtifacts = @(Copy-EvidenceTree `
        -Source $yyzArtifactSource `
        -Destination (Join-Path $artifactsRoot 'yyz_cartesian_6dof_framework_9'))
    [void]$representativeRuns.Add([ordered]@{
        id = 'yyz_cartesian_6dof_framework_9'
        source_commit = $manifest.git.head
        config = 'user/yyz_cartesian_6dof_framework_9/config/mission.json'
        working_directory = $missionSource
        exit_code = $yyzRun.ExitCode
        log = $yyzRun.LogPath
        captured_artifacts = @($yyzArtifacts | ForEach-Object {
            Convert-ToRepositoryPath -Path $_ -RepositoryRoot $repoRoot
        })
    })
    if ($yyzRun.ExitCode -ne 0 -or $yyzArtifacts.Count -eq 0) {
        Add-EnvironmentGap `
            -Id 'LEG-GAP-04' `
            -Stage 'representative-yyz-mission' `
            -Classification 'mission-reproduction-failure' `
            -Detail "Exit code $($yyzRun.ExitCode); captured artifact count $($yyzArtifacts.Count)." `
            -Impact 'The YYZ 6DoF representative output is unavailable or incomplete.' `
            -Disposition 'Preserve the log and output inventory; keep the task open.'
    }

    $allRunsPass = (@($representativeRuns | Where-Object {
        $_.exit_code -ne 0 -or $_.captured_artifacts.Count -eq 0
    }).Count -eq 0)
    if ($archiveVerified -and $testExtraction -and $missionExtraction -and
        $testStats.acceptance_satisfied -and $allRunsPass) {
        $status = if ($script:EnvironmentGaps.Count -gt 0) {
            'complete_with_documented_gaps'
        }
        else {
            'complete'
        }
    }
}
catch {
    $fatalMessage = $_.Exception.Message
    if (-not @($script:EnvironmentGaps | Where-Object { $_.id -eq 'LEG-GAP-FATAL' })) {
        Add-EnvironmentGap `
            -Id 'LEG-GAP-FATAL' `
            -Stage 'reproduction-script' `
            -Classification 'fatal-stage-failure' `
            -Detail $fatalMessage `
            -Impact 'One or more dependent reproduction stages did not run.' `
            -Disposition 'Review the last raw log and rerun with a new RunId after the environment is corrected.'
    }
}
finally {
    $osFacts = [ordered]@{
        version_string = [System.Environment]::OSVersion.VersionString
        machine = [System.Environment]::MachineName
        process_architecture = $env:PROCESSOR_ARCHITECTURE
        is_64_bit_operating_system = [System.Environment]::Is64BitOperatingSystem
        is_64_bit_process = [System.Environment]::Is64BitProcess
    }
    $environmentManifest = [ordered]@{
        schema_version = 'gnczmkn.r0-leg-001-environment/1'
        run_id = $RunId
        captured_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        timezone = [System.TimeZoneInfo]::Local.Id
        os = $osFacts
        powershell = [ordered]@{
            edition = $PSVersionTable.PSEdition
            version = $PSVersionTable.PSVersion.ToString()
        }
        generator = $Generator
        parallel_jobs = $Jobs
        tools = $(if (Get-Variable -Name toolIdentities -ErrorAction SilentlyContinue) {
            $toolIdentities
        } else {
            $null
        })
        eigen3_directory = $eigen3Directory
        legacy_source = [ordered]@{
            commit = $manifest.git.head
            archive = Convert-ToRepositoryPath -Path $archivePath -RepositoryRoot $repoRoot
            archive_bytes = (Get-Item -LiteralPath $archivePath).Length
            archive_sha256 = $archiveHash
            archive_prefix = $manifest.archive.prefix
            test_extraction = $testExtraction
            mission_extraction = $missionExtraction
        }
        working_root = Convert-ToRepositoryPath -Path $runRoot -RepositoryRoot $repoRoot
        evidence_root = Convert-ToRepositoryPath -Path $evidenceRoot -RepositoryRoot $repoRoot
    }
    Write-Utf8File `
        -Path (Join-Path $evidenceRoot 'environment-manifest.json') `
        -Content (($environmentManifest | ConvertTo-Json -Depth 12) + "`n")

    $commandJson = ConvertTo-Json -InputObject @($script:CommandRecords) -Depth 8
    if ($script:CommandRecords.Count -eq 0) {
        $commandJson = '[]'
    }
    Write-Utf8File -Path (Join-Path $evidenceRoot 'commands.json') -Content ($commandJson + "`n")
    $commandLines = New-Object System.Collections.Generic.List[string]
    foreach ($record in $script:CommandRecords) {
        $commandLines.Add("[$($record.name)]")
        $commandLines.Add("working_directory=$($record.working_directory)")
        $commandLines.Add("command=$($record.command)")
        $commandLines.Add("exit_code=$($record.exit_code)")
        $commandLines.Add("log=$($record.log)")
        $commandLines.Add('')
    }
    Write-Utf8File -Path (Join-Path $evidenceRoot 'commands.txt') -Content (($commandLines -join "`n") + "`n")

    $gapJson = ConvertTo-Json -InputObject @($script:EnvironmentGaps) -Depth 8
    if ($script:EnvironmentGaps.Count -eq 0) {
        $gapJson = '[]'
    }
    Write-Utf8File -Path (Join-Path $evidenceRoot 'environment-gaps.json') -Content ($gapJson + "`n")

    $hashTargets = New-Object System.Collections.Generic.List[string]
    $hashTargets.Add($archivePath)
    foreach ($candidate in @(
        (Join-Path $evidenceRoot 'test-clean-files.sha256'),
        (Join-Path $evidenceRoot 'mission-clean-files.sha256')
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $hashTargets.Add($candidate)
        }
    }
    foreach ($candidate in @(Get-ChildItem -LiteralPath $artifactsRoot -Recurse -File -ErrorAction SilentlyContinue)) {
        $hashTargets.Add($candidate.FullName)
    }
    foreach ($buildDirectory in @(
        (Join-Path $runRoot 'build-tests'),
        (Join-Path $runRoot 'build-mission-baseline'),
        (Join-Path $runRoot 'build-mission-yyz')
    )) {
        if (Test-Path -LiteralPath $buildDirectory) {
            foreach ($executable in @(Get-ChildItem -LiteralPath $buildDirectory -Recurse -File -Filter '*.exe')) {
                $hashTargets.Add($executable.FullName)
            }
        }
    }
    $hashLines = New-Object System.Collections.Generic.List[string]
    foreach ($target in @($hashTargets | Sort-Object -Unique)) {
        $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
        $relative = Convert-ToRepositoryPath -Path $target -RepositoryRoot $repoRoot
        $hashLines.Add("$hash  $relative")
    }
    Write-Utf8File `
        -Path (Join-Path $evidenceRoot 'artifact-hashes.sha256') `
        -Content (($hashLines -join "`n") + "`n")

    $resultSummary = [ordered]@{
        schema_version = 'gnczmkn.r0-leg-001-result/1'
        task = 'R0-LEG-001'
        run_id = $RunId
        status = $status
        fatal_message = $fatalMessage
        archive_verified = $archiveVerified
        clean_extractions_match = $(
            $null -ne $testExtraction -and
            $null -ne $missionExtraction -and
            $testExtraction.tree_digest_sha256 -eq $missionExtraction.tree_digest_sha256
        )
        tests = $testStats
        representative_runs = $representativeRuns.ToArray()
        environment_gap_ids = @($script:EnvironmentGaps | ForEach-Object { $_.id })
        evidence = [ordered]@{
            environment_manifest = 'environment-manifest.json'
            commands = @('commands.txt', 'commands.json')
            raw_logs = 'logs/'
            environment_gaps = 'environment-gaps.json'
            artifact_hashes = 'artifact-hashes.sha256'
        }
    }
    Write-Utf8File `
        -Path (Join-Path $evidenceRoot 'result-summary.json') `
        -Content (($resultSummary | ConvertTo-Json -Depth 12) + "`n")
}

Write-Host "R0-LEG-001 run status: $status"
Write-Host "Evidence: $evidenceRoot"
Write-Host "Working files: $runRoot"

if ($status -eq 'failed') {
    exit 1
}

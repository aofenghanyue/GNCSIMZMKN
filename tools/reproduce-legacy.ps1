[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$')]
    [string]$RunId,

    [Parameter(Mandatory = $true)]
    [string]$W64DevkitArchive,

    [Parameter(Mandatory = $true)]
    [string]$EigenArchive,

    [ValidateRange(1, 64)]
    [int]$Parallel = 4
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$legacyManifestPath = Join-Path $repoRoot 'reference\legacy\source-manifest.json'
$legacyChecksumPath = Join-Path $repoRoot 'reference\legacy\legacy-source.sha256'
$runRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $repoRoot (Join-Path 'build\legacy-reproduction\runs' $RunId)))
$allowedRunRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $repoRoot 'build\legacy-reproduction\runs'))

$expectedW64DevkitSha256 = '9208c19755cd4964b7915b9afcf02c66d493a4c870c4b3e83f6c538d9c1237a5'
$expectedEigenSha256 = 'eba3f3d414d2f8cba2919c78ec6daab08fc71ba2ba4ae502b7e5d4d99fc02cda'
$expectedVersions = [ordered]@{
    w64devkit = '2.9.1'
    gcc = '16.2.0'
    cmake = '4.4.2'
    ninja = '1.13.2'
    eigen = '3.4.0'
}

function Test-PathBelow([string]$Candidate, [string]$Root) {
    $separator = [System.IO.Path]::DirectorySeparatorChar
    $prefix = $Root.TrimEnd($separator, [System.IO.Path]::AltDirectorySeparatorChar) + $separator
    return $Candidate.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-TextSha256([string]$Text) {
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($Text)
        return ([System.BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Write-Json([string]$Path, [object]$Value) {
    $Value | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $Path -Encoding utf8
}

function ConvertTo-DisplayArgument([string]$Argument) {
    if ($Argument -notmatch '[\s"]') {
        return $Argument
    }
    return '"' + $Argument.Replace('"', '\"') + '"'
}

function Invoke-LoggedCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [switch]$AllowFailure
    )

    $script:currentStage = $Name
    $logPath = Join-Path $script:logsRoot ($Name + '.log')
    $started = [DateTimeOffset]::UtcNow
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $output = @()
    $exitCode = -1

    Push-Location -LiteralPath $WorkingDirectory
    try {
        # GUI-subsystem self-extracting archives may not initialize this automatic
        # variable even though they complete synchronously. Subsequent console
        # tools overwrite it normally; extracted tool presence is validated below.
        $global:LASTEXITCODE = 0
        $savedErrorActionPreference = $ErrorActionPreference
        try {
            # Windows PowerShell 5 wraps native stderr as non-terminating error
            # records. Capture those records and decide solely from the process
            # exit code; repository PowerShell errors remain strict elsewhere.
            $ErrorActionPreference = 'Continue'
            $output = @(& $Executable @Arguments 2>&1)
            $exitCode = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $savedErrorActionPreference
        }
    }
    finally {
        Pop-Location
        $timer.Stop()
    }

    $command = ((@($Executable) + $Arguments) | ForEach-Object {
            ConvertTo-DisplayArgument ([string]$_)
        }) -join ' '
    $logLines = @(
        "# stage: $Name"
        "# command: $command"
        "# working_directory: $WorkingDirectory"
        "# started_utc: $($started.ToString('o'))"
        "# elapsed_seconds: $([Math]::Round($timer.Elapsed.TotalSeconds, 3))"
        "# exit_code: $exitCode"
        ''
    )
    $logLines += @($output | ForEach-Object { $_.ToString() })
    $logLines | Set-Content -LiteralPath $logPath -Encoding utf8

    Write-Host "[$Name] exit=$exitCode elapsed=$([Math]::Round($timer.Elapsed.TotalSeconds, 1))s"
    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw "Stage '$Name' failed with exit code $exitCode. See $logPath"
    }

    return [pscustomobject]@{
        name = $Name
        command = $command
        working_directory = $WorkingDirectory
        exit_code = $exitCode
        elapsed_seconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
        log_path = $logPath
        output = $output
    }
}

function Write-OperationLog {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][scriptblock]$Operation
    )

    $script:currentStage = $Name
    $logPath = Join-Path $script:logsRoot ($Name + '.log')
    $started = [DateTimeOffset]::UtcNow
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $exitCode = 0
    $message = 'completed'
    try {
        & $Operation
    }
    catch {
        $exitCode = 1
        $message = $_.Exception.Message
        throw
    }
    finally {
        $timer.Stop()
        @(
            "# stage: $Name"
            "# command: $Command"
            "# working_directory: $WorkingDirectory"
            "# started_utc: $($started.ToString('o'))"
            "# elapsed_seconds: $([Math]::Round($timer.Elapsed.TotalSeconds, 3))"
            "# exit_code: $exitCode"
            ''
            $message
        ) | Set-Content -LiteralPath $logPath -Encoding utf8
        Write-Host "[$Name] exit=$exitCode elapsed=$([Math]::Round($timer.Elapsed.TotalSeconds, 1))s"
    }
}

function Get-TreeFingerprint([string]$Root) {
    $entries = [System.Collections.Generic.List[string]]::new()
    $files = Get-ChildItem -LiteralPath $Root -Recurse -File | Where-Object {
        $relative = $_.FullName.Substring($Root.Length + 1).Replace(
            [System.IO.Path]::DirectorySeparatorChar, '/')
        -not $relative.StartsWith('user/outputs/', [System.StringComparison]::OrdinalIgnoreCase) -and
        -not $relative.StartsWith('test_outputs/', [System.StringComparison]::OrdinalIgnoreCase)
    } | Sort-Object FullName

    foreach ($file in $files) {
        $relative = $file.FullName.Substring($Root.Length + 1).Replace(
            [System.IO.Path]::DirectorySeparatorChar, '/')
        $entries.Add("$(Get-Sha256 $file.FullName)  $relative")
    }

    $canonical = ($entries -join "`n") + "`n"
    return [pscustomobject]@{
        file_count = $entries.Count
        sha256 = Get-TextSha256 $canonical
        excluded_paths = @('user/outputs/**', 'test_outputs/**')
    }
}

function Get-NormalizedSummarySha256([string]$Path) {
    $stableLines = Get-Content -LiteralPath $Path -Encoding utf8 | Where-Object {
        $_ -notmatch '^(Generated:|  Wall clock time:|  Real-time ratio:|  Output:)'
    }
    return Get-TextSha256 (($stableLines -join "`n") + "`n")
}

function Get-ArtifactRecord([string]$Path, [string]$SourceRoot, [bool]$NormalizeSummary) {
    $relative = $Path.Substring($SourceRoot.Length + 1).Replace(
        [System.IO.Path]::DirectorySeparatorChar, '/')
    $record = [ordered]@{
        path = $relative
        bytes = (Get-Item -LiteralPath $Path).Length
        sha256 = Get-Sha256 $Path
    }
    if ($NormalizeSummary) {
        $record.normalized_sha256 = Get-NormalizedSummarySha256 $Path
        $record.normalization = 'omit Generated, Wall clock time, Real-time ratio, and Output lines'
    }
    return [pscustomobject]$record
}

function Get-NewOutputDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$OutputRoot,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Before,
        [Parameter(Mandatory = $true)][ValidateSet('fixed', 'timestamped')][string]$Mode
    )

    if ($Mode -eq 'fixed') {
        if (-not (Test-Path -LiteralPath $OutputRoot -PathType Container)) {
            throw "Expected fixed output directory was not created: $OutputRoot"
        }
        return $OutputRoot
    }

    $after = @()
    if (Test-Path -LiteralPath $OutputRoot -PathType Container) {
        $after = @(Get-ChildItem -LiteralPath $OutputRoot -Directory | ForEach-Object { $_.FullName })
    }
    $newDirectories = @($after | Where-Object { $_ -notin $Before })
    if ($newDirectories.Count -ne 1) {
        throw "Expected exactly one new output directory below $OutputRoot; found $($newDirectories.Count)."
    }
    return $newDirectories[0]
}

function Invoke-RepresentativeMission {
    param(
        [Parameter(Mandatory = $true)][object]$Case,
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$SourceRoot
    )

    $outputRoot = Join-Path $SourceRoot $Case.output_root
    $runs = @()
    for ($pass = 1; $pass -le 2; ++$pass) {
        if ($pass -eq 2 -and $Case.output_mode -eq 'timestamped') {
            Start-Sleep -Milliseconds 1100
        }

        $before = @()
        if (Test-Path -LiteralPath $outputRoot -PathType Container) {
            $before = @(Get-ChildItem -LiteralPath $outputRoot -Directory | ForEach-Object { $_.FullName })
        }

        $result = Invoke-LoggedCommand `
            -Name ("mission-{0}-run-{1}" -f $Case.id, $pass) `
            -Executable $Executable `
            -Arguments @('--config', $Case.mission) `
            -WorkingDirectory $SourceRoot
        $outputDirectory = Get-NewOutputDirectory `
            -OutputRoot $outputRoot `
            -Before $before `
            -Mode $Case.output_mode

        $artifacts = @()
        foreach ($artifact in $Case.artifacts) {
            $artifactPath = Join-Path $outputDirectory $artifact.name
            if (-not (Test-Path -LiteralPath $artifactPath -PathType Leaf)) {
                throw "Mission $($Case.id) did not create expected artifact: $artifactPath"
            }
            $artifacts += Get-ArtifactRecord `
                -Path $artifactPath `
                -SourceRoot $SourceRoot `
                -NormalizeSummary ($artifact.kind -eq 'summary')
        }

        $runs += [pscustomobject]@{
            pass = $pass
            exit_code = $result.exit_code
            elapsed_seconds = $result.elapsed_seconds
            output_directory = $outputDirectory.Substring($SourceRoot.Length + 1).Replace(
                [System.IO.Path]::DirectorySeparatorChar, '/')
            log = $result.log_path.Substring($script:runRoot.Length + 1).Replace(
                [System.IO.Path]::DirectorySeparatorChar, '/')
            artifacts = $artifacts
        }
    }

    $comparisons = @()
    for ($index = 0; $index -lt $Case.artifacts.Count; ++$index) {
        $kind = $Case.artifacts[$index].kind
        $first = $runs[0].artifacts[$index]
        $second = $runs[1].artifacts[$index]
        if ($kind -eq 'summary') {
            $matches = $first.normalized_sha256 -eq $second.normalized_sha256
            $basis = 'normalized_sha256'
        }
        else {
            $matches = $first.sha256 -eq $second.sha256
            $basis = 'sha256'
        }
        if (-not $matches) {
            throw "Mission $($Case.id) artifact $($Case.artifacts[$index].name) is not deterministic on $basis."
        }
        $comparisons += [pscustomobject]@{
            artifact = $Case.artifacts[$index].name
            basis = $basis
            matches = $matches
        }
    }

    return [pscustomobject]@{
        id = $Case.id
        description = $Case.description
        active_project = $Case.active_project
        mission = $Case.mission
        status = 'passed'
        deterministic = $true
        comparisons = $comparisons
        runs = $runs
    }
}

if (-not (Test-PathBelow $runRoot $allowedRunRoot)) {
    throw "Resolved run directory escapes the allowed root: $runRoot"
}
if (Test-Path -LiteralPath $runRoot) {
    throw "Run directory already exists; choose a new RunId: $runRoot"
}
if (-not (Test-Path -LiteralPath $legacyManifestPath -PathType Leaf)) {
    throw "Legacy source manifest is missing: $legacyManifestPath"
}
if (-not (Test-Path -LiteralPath $legacyChecksumPath -PathType Leaf)) {
    throw "Legacy checksum file is missing: $legacyChecksumPath"
}

$w64ArchivePath = (Resolve-Path -LiteralPath $W64DevkitArchive).Path
$eigenArchivePath = (Resolve-Path -LiteralPath $EigenArchive).Path
if ((Get-Sha256 $w64ArchivePath) -ne $expectedW64DevkitSha256) {
    throw "w64devkit archive hash mismatch; expected $expectedW64DevkitSha256."
}
if ((Get-Sha256 $eigenArchivePath) -ne $expectedEigenSha256) {
    throw "Eigen archive hash mismatch; expected $expectedEigenSha256."
}

$legacyManifest = Get-Content -LiteralPath $legacyManifestPath -Raw -Encoding utf8 | ConvertFrom-Json
$legacyArchivePath = Join-Path $repoRoot $legacyManifest.archive.path
$legacyExpectedSha = (Get-Content -LiteralPath $legacyChecksumPath -Raw -Encoding utf8).Trim().Split(' ')[0].ToLowerInvariant()
$legacyActualSha = Get-Sha256 $legacyArchivePath
if ($legacyActualSha -ne $legacyExpectedSha -or
    $legacyActualSha -ne $legacyManifest.archive.sha256.ToLowerInvariant()) {
    throw 'Legacy archive identity differs from its checksum or source manifest.'
}

New-Item -ItemType Directory -Path $runRoot | Out-Null
$script:runRoot = $runRoot
$script:logsRoot = Join-Path $runRoot 'logs'
New-Item -ItemType Directory -Path $script:logsRoot | Out-Null
$script:currentStage = 'initialization'
$originalPath = $env:PATH

try {
    $depsRoot = Join-Path $runRoot 'deps'
    $toolchainExtractRoot = Join-Path $depsRoot 'w64devkit-extracted'
    New-Item -ItemType Directory -Path $toolchainExtractRoot -Force | Out-Null
    Write-OperationLog `
        -Name 'dependency-w64devkit-extract' `
        -Command "$w64ArchivePath -y -o$toolchainExtractRoot" `
        -WorkingDirectory $runRoot `
        -Operation {
            # The release artifact is a GUI-subsystem 7-Zip SFX. Direct '&'
            # invocation returns before extraction, so wait on the process.
            $extractProcess = Start-Process `
                -FilePath $w64ArchivePath `
                -ArgumentList @('-y', "-o$toolchainExtractRoot") `
                -WorkingDirectory $runRoot `
                -WindowStyle Hidden `
                -Wait `
                -PassThru
            if ($extractProcess.ExitCode -ne 0) {
                throw "w64devkit extraction failed with exit code $($extractProcess.ExitCode)."
            }
        }

    $toolchainRoot = Join-Path $toolchainExtractRoot 'w64devkit'
    $toolchainBin = Join-Path $toolchainRoot 'bin'
    $gcc = Join-Path $toolchainBin 'gcc.exe'
    $gxx = Join-Path $toolchainBin 'g++.exe'
    $cmake = Join-Path $toolchainBin 'cmake.exe'
    $ctest = Join-Path $toolchainBin 'ctest.exe'
    $ninja = Join-Path $toolchainBin 'ninja.exe'
    foreach ($tool in @($gcc, $gxx, $cmake, $ctest, $ninja)) {
        if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
            throw "Expected tool is missing from w64devkit: $tool"
        }
    }
    $kitVersion = (Get-Content -LiteralPath (Join-Path $toolchainRoot 'VERSION.txt') -Raw).Trim()
    if ($kitVersion -ne $expectedVersions.w64devkit) {
        throw "Unexpected w64devkit version: $kitVersion"
    }
    $env:PATH = $toolchainBin + ';' + $originalPath

    $eigenSourceRoot = Join-Path $depsRoot 'eigen-source'
    Write-OperationLog `
        -Name 'dependency-eigen-extract' `
        -Command "Expand-Archive -LiteralPath $eigenArchivePath -DestinationPath $eigenSourceRoot" `
        -WorkingDirectory $runRoot `
        -Operation {
            New-Item -ItemType Directory -Path $eigenSourceRoot -Force | Out-Null
            Expand-Archive -LiteralPath $eigenArchivePath -DestinationPath $eigenSourceRoot
        }
    $eigenProjectRoot = Join-Path $eigenSourceRoot 'eigen-3.4.0'
    if (-not (Test-Path -LiteralPath (Join-Path $eigenProjectRoot 'CMakeLists.txt') -PathType Leaf)) {
        throw "Eigen archive did not contain the expected eigen-3.4.0 root."
    }
    $eigenBuildRoot = Join-Path $depsRoot 'eigen-build'
    $eigenInstallRoot = Join-Path $depsRoot 'eigen-install'
    Invoke-LoggedCommand `
        -Name 'dependency-eigen-configure' `
        -Executable $cmake `
        -Arguments @(
            '-S', $eigenProjectRoot,
            '-B', $eigenBuildRoot,
            '-G', 'Ninja',
            "-DCMAKE_INSTALL_PREFIX=$eigenInstallRoot",
            '-DBUILD_TESTING=OFF'
        ) `
        -WorkingDirectory $runRoot | Out-Null
    Invoke-LoggedCommand `
        -Name 'dependency-eigen-install' `
        -Executable $cmake `
        -Arguments @('--build', $eigenBuildRoot, '--target', 'install') `
        -WorkingDirectory $runRoot | Out-Null
    $eigen3Dir = Join-Path $eigenInstallRoot 'share\eigen3\cmake'
    $eigenVersionFile = Join-Path $eigen3Dir 'Eigen3ConfigVersion.cmake'
    if (-not (Test-Path -LiteralPath $eigenVersionFile -PathType Leaf) -or
        (Get-Content -LiteralPath $eigenVersionFile -Raw) -notmatch '3\.4\.0') {
        throw 'Installed Eigen package does not report version 3.4.0.'
    }

    $sourceExtractRoot = Join-Path $runRoot 'source'
    Write-OperationLog `
        -Name 'legacy-source-extract' `
        -Command "Expand-Archive -LiteralPath $legacyArchivePath -DestinationPath $sourceExtractRoot" `
        -WorkingDirectory $runRoot `
        -Operation {
            New-Item -ItemType Directory -Path $sourceExtractRoot -Force | Out-Null
            Expand-Archive -LiteralPath $legacyArchivePath -DestinationPath $sourceExtractRoot
        }
    $sourceRootName = $legacyManifest.archive.prefix.TrimEnd('/')
    $sourceRoot = Join-Path $sourceExtractRoot $sourceRootName
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot 'CMakeLists.txt') -PathType Leaf)) {
        throw "Legacy archive did not contain expected source root: $sourceRootName"
    }
    $sourceBefore = Get-TreeFingerprint $sourceRoot

    $mainBuildRoot = Join-Path $runRoot 'build-main'
    $commonConfigure = @(
        '-S', $sourceRoot,
        '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release',
        "-DCMAKE_CXX_COMPILER=$gxx",
        "-DEigen3_DIR=$eigen3Dir"
    )
    Invoke-LoggedCommand `
        -Name 'legacy-main-configure' `
        -Executable $cmake `
        -Arguments ($commonConfigure + @(
            '-B', $mainBuildRoot,
            '-DBUILD_TESTS=ON',
            '-DGNC_BUILD_EXAMPLE_TESTS=ON',
            '-DGNC_BUILD_PROJECT_TESTS=ON',
            '-DGNC_BUILD_ARCHITECTURE_GUARDS=ON'
        )) `
        -WorkingDirectory $runRoot | Out-Null
    Invoke-LoggedCommand `
        -Name 'legacy-main-build' `
        -Executable $cmake `
        -Arguments @('--build', $mainBuildRoot, '--parallel', [string]$Parallel) `
        -WorkingDirectory $runRoot | Out-Null

    $testList = Invoke-LoggedCommand `
        -Name 'legacy-ctest-list' `
        -Executable $ctest `
        -Arguments @('--test-dir', $mainBuildRoot, '-N') `
        -WorkingDirectory $runRoot
    if (($testList.output -join "`n") -notmatch 'Total Tests:\s+27') {
        throw 'The clean legacy configuration did not expose exactly 27 tests.'
    }

    Invoke-LoggedCommand `
        -Name 'legacy-ctest-all' `
        -Executable $ctest `
        -Arguments @('--test-dir', $mainBuildRoot, '--output-on-failure') `
        -WorkingDirectory $runRoot | Out-Null
    foreach ($label in @('core', 'example', 'project', 'architecture-guard')) {
        Invoke-LoggedCommand `
            -Name ("legacy-ctest-label-{0}" -f $label) `
            -Executable $ctest `
            -Arguments @('--test-dir', $mainBuildRoot, '-L', $label, '--output-on-failure') `
            -WorkingDirectory $runRoot | Out-Null
    }

    $directTests = @(
        'test_ideal_cartesian_3dof_mission',
        'test_ideal_cartesian_6dof_mission'
    )
    $directResults = @()
    foreach ($testName in $directTests) {
        $direct = Invoke-LoggedCommand `
            -Name ("legacy-direct-{0}" -f $testName) `
            -Executable (Join-Path $mainBuildRoot ("bin\{0}.exe" -f $testName)) `
            -Arguments @() `
            -WorkingDirectory $sourceRoot
        $directResults += [pscustomobject]@{
            test = $testName
            working_directory = 'legacy-source-root'
            exit_code = $direct.exit_code
            log = $direct.log_path.Substring($runRoot.Length + 1).Replace(
                [System.IO.Path]::DirectorySeparatorChar, '/')
        }
    }

    $projectBuilds = [ordered]@{}
    $projectBuilds.example_05_ideal_3dof_geographic_baseline = $mainBuildRoot
    foreach ($project in @(
            'example_08_cavh_geographic_3dof_custom',
            'yyz_cartesian_6dof_framework_9'
        )) {
        $projectBuildRoot = Join-Path $runRoot ("build-project-{0}" -f $project)
        Invoke-LoggedCommand `
            -Name ("project-{0}-configure" -f $project) `
            -Executable $cmake `
            -Arguments ($commonConfigure + @(
                '-B', $projectBuildRoot,
                '-DBUILD_TESTS=OFF',
                "-DGNC_ACTIVE_PROJECT=$project"
            )) `
            -WorkingDirectory $runRoot | Out-Null
        Invoke-LoggedCommand `
            -Name ("project-{0}-build" -f $project) `
            -Executable $cmake `
            -Arguments @('--build', $projectBuildRoot, '--parallel', [string]$Parallel) `
            -WorkingDirectory $runRoot | Out-Null
        $projectBuilds[$project] = $projectBuildRoot
    }

    $missionCases = @(
        [pscustomobject]@{
            id = 'geographic-3dof'
            description = 'Ideal geographic/local-spherical 3DoF baseline'
            active_project = 'example_05_ideal_3dof_geographic_baseline'
            mission = 'user/example_05_ideal_3dof_geographic_baseline/config/mission.json'
            output_root = 'user\outputs\example_05_ideal_3dof_geographic_baseline'
            output_mode = 'fixed'
            artifacts = @(
                [pscustomobject]@{ name = 'ideal_3dof_geographic_baseline.csv'; kind = 'deterministic' },
                [pscustomobject]@{ name = 'summary.txt'; kind = 'summary' }
            )
        },
        [pscustomobject]@{
            id = 'cartesian-3dof'
            description = 'Ideal Cartesian 3DoF baseline'
            active_project = 'example_05_ideal_3dof_geographic_baseline'
            mission = 'user/example_06_ideal_cartesian_3dof_baseline/config/mission.json'
            output_root = 'user\outputs\example_06_ideal_cartesian_3dof_baseline'
            output_mode = 'fixed'
            artifacts = @(
                [pscustomobject]@{ name = 'ideal_cartesian_3dof_baseline.csv'; kind = 'deterministic' },
                [pscustomobject]@{ name = 'summary.txt'; kind = 'summary' }
            )
        },
        [pscustomobject]@{
            id = 'cartesian-6dof'
            description = 'Ideal Cartesian 6DoF baseline'
            active_project = 'example_05_ideal_3dof_geographic_baseline'
            mission = 'user/example_07_ideal_cartesian_6dof_baseline/config/mission.json'
            output_root = 'user\outputs\example_07_ideal_cartesian_6dof_baseline'
            output_mode = 'fixed'
            artifacts = @(
                [pscustomobject]@{ name = 'ideal_cartesian_6dof_baseline.csv'; kind = 'deterministic' },
                [pscustomobject]@{ name = 'summary.txt'; kind = 'summary' }
            )
        },
        [pscustomobject]@{
            id = 'cavh-geographic-3dof'
            description = 'CAV-H custom geographic 3DoF research mission'
            active_project = 'example_08_cavh_geographic_3dof_custom'
            mission = 'user/example_08_cavh_geographic_3dof_custom/config/mission.json'
            output_root = 'user\outputs\example_08_cavh_geographic_3dof_custom'
            output_mode = 'timestamped'
            artifacts = @(
                [pscustomobject]@{ name = 'cavh_geographic_3dof_custom.csv'; kind = 'deterministic' },
                [pscustomobject]@{ name = 'summary.txt'; kind = 'summary' }
            )
        },
        [pscustomobject]@{
            id = 'yyz-cartesian-6dof'
            description = 'YYZ custom Cartesian 6DoF framework-9 mission'
            active_project = 'yyz_cartesian_6dof_framework_9'
            mission = 'user/yyz_cartesian_6dof_framework_9/config/mission.json'
            output_root = 'user\outputs\yyz_cartesian_6dof_framework_9'
            output_mode = 'timestamped'
            artifacts = @(
                [pscustomobject]@{ name = 'trajectory_nominal.csv'; kind = 'deterministic' },
                [pscustomobject]@{ name = 'summary.txt'; kind = 'summary' }
            )
        }
    )

    $missionResults = @()
    foreach ($case in $missionCases) {
        $projectBuildRoot = $projectBuilds[$case.active_project]
        $missionExecutable = Join-Path $projectBuildRoot 'bin\gnc_sim.exe'
        $missionResults += Invoke-RepresentativeMission `
            -Case $case `
            -Executable $missionExecutable `
            -SourceRoot $sourceRoot
    }

    $sourceAfter = Get-TreeFingerprint $sourceRoot
    if ($sourceBefore.file_count -ne $sourceAfter.file_count -or
        $sourceBefore.sha256 -ne $sourceAfter.sha256) {
        throw 'Legacy tracked-source fingerprint changed during reproduction.'
    }

    $osName = [System.Environment]::OSVersion.VersionString
    $cpuName = $env:PROCESSOR_IDENTIFIER
    try {
        $osRecord = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
        $osName = $osRecord.Caption + ' ' + $osRecord.Version
    }
    catch {}
    try {
        $cpuRecord = Get-CimInstance Win32_Processor -ErrorAction Stop | Select-Object -First 1
        $cpuName = $cpuRecord.Name.Trim()
    }
    catch {}

    $gccVersionText = @(& $gxx --version 2>&1)
    $cmakeVersionText = @(& $cmake --version 2>&1)
    $ninjaVersionText = @(& $ninja --version 2>&1)
    if (($gccVersionText[0] -notmatch [regex]::Escape($expectedVersions.gcc)) -or
        ($cmakeVersionText[0] -notmatch [regex]::Escape($expectedVersions.cmake)) -or
        ($ninjaVersionText[0] -ne $expectedVersions.ninja)) {
        throw 'Extracted tool versions differ from the frozen reproduction environment.'
    }

    $buildArtifacts = @()
    foreach ($entry in $projectBuilds.GetEnumerator()) {
        $path = Join-Path ([string]$entry.Value) 'bin\gnc_sim.exe'
        $buildArtifacts += [pscustomobject]@{
            active_project = [string]$entry.Key
            path = $path.Substring($runRoot.Length + 1).Replace(
                [System.IO.Path]::DirectorySeparatorChar, '/')
            bytes = (Get-Item -LiteralPath $path).Length
            sha256 = Get-Sha256 $path
        }
    }

    $environmentManifest = [ordered]@{
        schema_version = 'gnczmkn.legacy-environment/1'
        run_id = $RunId
        recorded_at_utc = [DateTimeOffset]::UtcNow.ToString('o')
        host = [ordered]@{
            os = $osName
            architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
            cpu = $cpuName
            logical_processors = [Environment]::ProcessorCount
            powershell = $PSVersionTable.PSVersion.ToString()
        }
        source = [ordered]@{
            git_head = $legacyManifest.git.head
            archive_sha256 = $legacyActualSha
            archive_bytes = (Get-Item -LiteralPath $legacyArchivePath).Length
            archive_prefix = $legacyManifest.archive.prefix
            fingerprint_before = $sourceBefore
            fingerprint_after = $sourceAfter
            mutation_check = 'passed'
        }
        dependencies = [ordered]@{
            w64devkit = [ordered]@{
                version = $kitVersion
                archive_sha256 = Get-Sha256 $w64ArchivePath
                archive_bytes = (Get-Item -LiteralPath $w64ArchivePath).Length
                gcc = $gccVersionText[0]
                cmake = $cmakeVersionText[0]
                ninja = $ninjaVersionText[0]
            }
            eigen = [ordered]@{
                version = $expectedVersions.eigen
                archive_sha256 = Get-Sha256 $eigenArchivePath
                archive_bytes = (Get-Item -LiteralPath $eigenArchivePath).Length
                config_directory = 'deps/eigen-install/share/eigen3/cmake'
            }
        }
        build = [ordered]@{
            generator = 'Ninja'
            build_type = 'Release'
            cxx_standard = 17
            parallel = $Parallel
            test_options = [ordered]@{
                BUILD_TESTS = 'ON'
                GNC_BUILD_EXAMPLE_TESTS = 'ON'
                GNC_BUILD_PROJECT_TESTS = 'ON'
                GNC_BUILD_ARCHITECTURE_GUARDS = 'ON'
            }
            environment = [ordered]@{
                PATH_prefix = 'deps/w64devkit-extracted/w64devkit/bin'
                CMAKE_GENERATOR = $null
                CC = $null
                CXX = $null
                Eigen3_DIR = 'deps/eigen-install/share/eigen3/cmake'
            }
            artifacts = $buildArtifacts
        }
    }
    $environmentPath = Join-Path $runRoot 'environment-manifest.json'
    Write-Json $environmentPath $environmentManifest

    $testReport = [ordered]@{
        schema_version = 'gnczmkn.legacy-test-report/1'
        run_id = $RunId
        status = 'passed'
        configured = 27
        passed = 27
        failed = 0
        labels = [ordered]@{
            core = 18
            example = 6
            project = 2
            'architecture-guard' = 1
        }
        ctest_working_directory = 'build-main'
        historical_baseline = [ordered]@{
            configured = $legacyManifest.baseline_tests.configured_tests
            passed = $legacyManifest.baseline_tests.ctest_passed
            failed = $legacyManifest.baseline_tests.ctest_failed
            path_sensitive_failures = $legacyManifest.baseline_tests.path_sensitive_failures
        }
        path_sensitive_comparison = [ordered]@{
            clean_ctest = 'passed'
            direct_source_root_runs = $directResults
        }
        logs = [ordered]@{
            list = 'logs/legacy-ctest-list.log'
            all = 'logs/legacy-ctest-all.log'
            core = 'logs/legacy-ctest-label-core.log'
            example = 'logs/legacy-ctest-label-example.log'
            project = 'logs/legacy-ctest-label-project.log'
            architecture_guard = 'logs/legacy-ctest-label-architecture-guard.log'
        }
    }
    $testReportPath = Join-Path $runRoot 'test-report.json'
    Write-Json $testReportPath $testReport

    $missionReport = [ordered]@{
        schema_version = 'gnczmkn.legacy-mission-report/1'
        run_id = $RunId
        status = 'passed'
        mission_count = $missionResults.Count
        summary_normalization = @(
            '^Generated:',
            '^  Wall clock time:',
            '^  Real-time ratio:',
            '^  Output:'
        )
        missions = $missionResults
    }
    $missionReportPath = Join-Path $runRoot 'mission-report.json'
    Write-Json $missionReportPath $missionReport

    $indexedFiles = @($environmentPath, $testReportPath, $missionReportPath)
    $indexedFiles += @(Get-ChildItem -LiteralPath $script:logsRoot -File | ForEach-Object { $_.FullName })
    $evidenceFiles = @()
    foreach ($path in ($indexedFiles | Sort-Object -Unique)) {
        $evidenceFiles += [pscustomobject]@{
            path = $path.Substring($runRoot.Length + 1).Replace(
                [System.IO.Path]::DirectorySeparatorChar, '/')
            bytes = (Get-Item -LiteralPath $path).Length
            sha256 = Get-Sha256 $path
        }
    }
    $evidenceIndex = [ordered]@{
        schema_version = 'gnczmkn.legacy-evidence-index/1'
        run_id = $RunId
        status = 'passed'
        generated_at_utc = [DateTimeOffset]::UtcNow.ToString('o')
        evidence_files = $evidenceFiles
    }
    $evidenceIndexPath = Join-Path $runRoot 'evidence-index.json'
    Write-Json $evidenceIndexPath $evidenceIndex

    Write-Host "Legacy reproduction passed: $runRoot"
    Write-Host 'Tests: 27/27; representative missions: 5/5; source mutation check: passed.'
}
catch {
    $failurePath = Join-Path $runRoot 'failure.json'
    if (Test-Path -LiteralPath $runRoot -PathType Container) {
        Write-Json $failurePath ([ordered]@{
                schema_version = 'gnczmkn.legacy-reproduction-failure/1'
                run_id = $RunId
                failed_stage = $script:currentStage
                recorded_at_utc = [DateTimeOffset]::UtcNow.ToString('o')
                message = $_.Exception.Message
            })
    }
    throw
}
finally {
    $env:PATH = $originalPath
}

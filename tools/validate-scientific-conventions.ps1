[CmdletBinding()]
param(
    [string]$CppExecutable,
    [string]$PythonExecutable = 'python',
    [string]$EvidencePath,
    [switch]$StaticOnly,
    [switch]$UpdateEvidence,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$fixtureRoot = Join-Path $repoRoot 'fixtures\ref-scientific-conventions'
$manifestPath = Join-Path $fixtureRoot 'fixture-manifest.json'
$conventionsPath = Join-Path $fixtureRoot 'conventions.json'
$casesPath = Join-Path $fixtureRoot 'cases.json'
$cppSourcePath = Join-Path $repoRoot 'tests\scientific_conventions.cpp'
$pythonSourcePath = Join-Path $repoRoot 'tools\scientific_conventions_reference.py'
$validatorPath = $MyInvocation.MyCommand.Path
$cmakePath = Join-Path $repoRoot 'CMakeLists.txt'

if ([string]::IsNullOrWhiteSpace($EvidencePath)) {
    $EvidencePath = Join-Path $repoRoot 'docs\quality\scientific-conventions-cross-tool-report.json'
}
elseif (-not [System.IO.Path]::IsPathRooted($EvidencePath)) {
    $EvidencePath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $EvidencePath))
}

$issues = [System.Collections.Generic.List[string]]::new()

function Add-Issue([string]$Message) {
    $script:issues.Add($Message)
}

function Read-Json([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-Issue "Required JSON file is missing: $Path"
        return $null
    }
    try {
        return Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
    }
    catch {
        Add-Issue "Invalid JSON: $Path :: $($_.Exception.Message)"
        return $null
    }
}

function Get-RelativePath([string]$Path) {
    $rootUri = [System.Uri]::new($repoRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar)
    $pathUri = [System.Uri]::new([System.IO.Path]::GetFullPath($Path))
    return [System.Uri]::UnescapeDataString($rootUri.MakeRelativeUri($pathUri).ToString())
}

function Get-NormalizedTextHash([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    if ($text.Length -gt 0 -and $text[0] -eq [char]0xFEFF) {
        $text = $text.Substring(1)
    }
    $text = $text.Replace("`r`n", "`n").Replace("`r", "`n")
    $normalizedBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($text)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($normalizedBytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

function Test-ExactSequence {
    param(
        [object[]]$Actual,
        [object[]]$Expected,
        [string]$Label
    )
    if ($Actual.Count -ne $Expected.Count) {
        Add-Issue "$Label count mismatch: expected $($Expected.Count), got $($Actual.Count)."
        return
    }
    for ($index = 0; $index -lt $Expected.Count; ++$index) {
        if ([string]$Actual[$index] -cne [string]$Expected[$index]) {
            Add-Issue "$Label order/value mismatch at index $index."
        }
    }
}

function Test-UniqueIds {
    param(
        [object[]]$Items,
        [string]$Label
    )
    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($item in $Items) {
        $id = [string]$item.id
        if ([string]::IsNullOrWhiteSpace($id)) {
            Add-Issue "$Label contains an empty id."
        }
        elseif (-not $seen.Add($id)) {
            Add-Issue "$Label contains duplicate id '$id'."
        }
    }
}

function Test-FiniteNumber([object]$Value, [string]$Label) {
    try {
        $number = [double]$Value
        if ([double]::IsNaN($number) -or [double]::IsInfinity($number)) {
            Add-Issue "$Label is not finite."
        }
    }
    catch {
        Add-Issue "$Label is not numeric."
    }
}

$requiredCheckIds = @(
    'quaternion.direction',
    'quaternion.hamilton-product',
    'quaternion.composition',
    'quaternion.inverse',
    'quaternion.matrix-equivalence',
    'quaternion.orthogonality',
    'quaternion.determinant',
    'quaternion.sign-equivalence',
    'quaternion.serialization-wxyz',
    'quaternion.zero-norm-domain-error',
    'quaternion.coefficient-validation',
    'quaternion.normalization-policy',
    'quaternion.body-rate-derivative',
    'quaternion.euler-round-trip',
    'quaternion.euler-singularity',
    'units.si-boundary',
    'units.domain-validation',
    'frames.point-versus-free-vector',
    'time.integer-tick',
    'time.duration-alignment',
    'time.type-separation',
    'time.clock-domain',
    'time.validity-interval'
)

$expectedUnitPairs = @(
    'length_position=m',
    'time_duration=s',
    'mass=kg',
    'angle=rad',
    'velocity=m/s',
    'acceleration=m/s^2',
    'angular_velocity=rad/s',
    'force=N',
    'moment=N*m',
    'pressure=Pa',
    'temperature=K'
)

$expectedInputPaths = @(
    'docs/adr/0006-si-frame-and-simulation-time-conventions.md',
    'docs/adr/0007-passive-hamilton-quaternion-convention.md',
    'fixtures/ref-scientific-conventions/fixture-manifest.json',
    'fixtures/ref-scientific-conventions/conventions.json',
    'fixtures/ref-scientific-conventions/cases.json',
    'tests/scientific_conventions.cpp',
    'tools/scientific_conventions_reference.py',
    'tools/validate-scientific-conventions.ps1'
)

$manifest = Read-Json $manifestPath
$conventions = Read-Json $conventionsPath
$cases = Read-Json $casesPath

if ($null -ne $manifest) {
    if ($manifest.schema_version -cne 'gnczmkn.fixture-manifest/1') { Add-Issue 'Scientific fixture manifest schema_version drifted.' }
    if ($manifest.fixture_id -cne 'REF-SCIENTIFIC-CONVENTIONS-001') { Add-Issue 'Scientific fixture id drifted.' }
    if ($manifest.status -cne 'executable') { Add-Issue 'Scientific fixture must remain executable.' }
    if ($manifest.authority -cne 'scientific_authority') { Add-Issue 'Scientific fixture authority drifted.' }
    $factIds = @($manifest.expected_facts | ForEach-Object { [string]$_.id })
    Test-ExactSequence -Actual $factIds -Expected @(
        'FACT-SCI-QUATERNION', 'FACT-SCI-UNIT', 'FACT-SCI-FRAME',
        'FACT-SCI-TIME', 'FACT-SCI-CROSS-TOOL') -Label 'Scientific fixture expected facts'
}

if ($null -ne $conventions) {
    if ($conventions.schema_version -cne 'gnczmkn.scientific-conventions/1') { Add-Issue 'Scientific convention schema_version drifted.' }
    if ($conventions.convention_id -cne 'SCI-CONVENTIONS-001') { Add-Issue 'Scientific convention id drifted.' }
    if ($conventions.maturity -cne 'fixture') { Add-Issue 'Scientific convention maturity must remain fixture.' }
    if ($conventions.units.runtime_system -cne 'SI' -or $conventions.units.angle_unit -cne 'rad') { Add-Issue 'Runtime units must remain SI with radians.' }
    $unitPairs = @($conventions.units.canonical | ForEach-Object { "$($_.quantity)=$($_.unit)" })
    Test-ExactSequence -Actual $unitPairs -Expected $expectedUnitPairs -Label 'Canonical unit table'
    Test-ExactSequence -Actual @($conventions.units.conversion_boundaries) -Expected @(
        'ConfigAdapter', 'ToolAdapter', 'ArtifactAdapter', 'DomainConverter') -Label 'Unit conversion boundaries'
    if (-not [bool]$conventions.units.offset_units_require_explicit_rule) { Add-Issue 'Offset units must require an explicit conversion rule.' }
    if ([bool]$conventions.units.hot_path_unit_guessing_allowed) { Add-Issue 'Hot-path unit guessing must remain forbidden.' }
    $unitDomain = $conventions.units.domain_policy
    if (-not [bool]$unitDomain.finite_input_required) { Add-Issue 'Unit conversion inputs must remain finite.' }
    if ([double]$unitDomain.absolute_zero_kelvin -ne 0.0) { Add-Issue 'Absolute-zero Kelvin identity drifted.' }
    if ($unitDomain.below_absolute_zero_disposition -cne 'DomainError') { Add-Issue 'Below-absolute-zero disposition drifted.' }
    if ($unitDomain.unknown_unit_disposition -cne 'DomainError') { Add-Issue 'Unknown-unit disposition drifted.' }

    if ($conventions.frames.vector_layout -cne 'column') { Add-Issue 'Vector layout must remain column.' }
    if ($conventions.frames.transform_notation -cne 'R_to_from') { Add-Issue 'Frame transform notation drifted.' }
    if ($conventions.frames.coordinate_equation -cne 'v_to = R_to_from * v_from') { Add-Issue 'Frame coordinate equation drifted.' }
    Test-ExactSequence -Actual @($conventions.frames.frame_id_examples) -Expected @(
        'frame.inertial.j2000@1', 'frame.earth.ecef@1', 'frame.local.nue@1',
        'frame.vehicle.body@1', 'frame.vehicle.wind@1') -Label 'Frame id examples'
    Test-ExactSequence -Actual @($conventions.frames.domain_vector_metadata) -Expected @(
        'geometry_kind', 'expressed_in_frame', 'reference_frame', 'physical_semantic',
        'unit', 'sample_time', 'valid_time') -Label 'Domain vector metadata'
    if ($conventions.frames.point_transform -cne 'rotation_and_translation') { Add-Issue 'Point transform category drifted.' }
    if ($conventions.frames.free_vector_transform -cne 'rotation_only') { Add-Issue 'Free-vector transform category drifted.' }

    if ($conventions.quaternion.notation -cne 'q_to_from') { Add-Issue 'Quaternion notation drifted.' }
    if ($conventions.quaternion.semantic -cne 'passive_coordinate_transform') { Add-Issue 'Quaternion semantic must remain passive.' }
    if ($conventions.quaternion.vector_equation -cne 'v_to = inverse(q_to_from) * pure(v_from) * q_to_from') { Add-Issue 'Quaternion vector equation drifted.' }
    if ($conventions.quaternion.algebra -cne 'Hamilton') { Add-Issue 'Quaternion algebra must remain Hamilton.' }
    Test-ExactSequence -Actual @($conventions.quaternion.storage_order) -Expected @('w', 'x', 'y', 'z') -Label 'Quaternion serialization order'
    if ($conventions.quaternion.quaternion_composition -cne 'q_c_a = q_b_a * q_c_b') { Add-Issue 'Quaternion composition order drifted.' }
    if ($conventions.quaternion.matrix_composition -cne 'R_c_a = R_c_b * R_b_a') { Add-Issue 'Matrix composition order drifted.' }
    if ($conventions.quaternion.zero_norm_disposition -cne 'DomainError') { Add-Issue 'Zero-norm quaternion disposition drifted.' }
    $quaternionInput = $conventions.quaternion.input_policy
    if ([int]$quaternionInput.coefficient_count -ne 4) { Add-Issue 'Quaternion coefficient count drifted.' }
    if (-not [bool]$quaternionInput.finite_coefficients_required) { Add-Issue 'Quaternion coefficients must remain finite.' }
    if (-not [bool]$quaternionInput.non_unit_requires_explicit_policy) { Add-Issue 'Non-unit quaternion input must require an explicit policy.' }
    Test-ExactSequence -Actual @($quaternionInput.supported_normalization_policies) -Expected @(
        'Error', 'NormalizeWithFlag') -Label 'Quaternion normalization policies'
    if ($quaternionInput.normalization_correction_flag -cne 'normalized') { Add-Issue 'Quaternion normalization correction flag drifted.' }
    if (-not [bool]$conventions.quaternion.sign_equivalence) { Add-Issue 'Quaternion sign equivalence must remain enabled.' }
    Test-ExactSequence -Actual @($conventions.quaternion.euler_metadata_required) -Expected @(
        'sequence', 'intrinsic_or_extrinsic', 'angle_unit', 'canonical_range',
        'singular_interval') -Label 'Euler metadata requirements'
    $eulerProfile = $conventions.quaternion.euler_verification_profile
    if ($eulerProfile.name -cne 'intrinsic_ZYX_yaw_pitch_roll') { Add-Issue 'Euler verification profile identity drifted.' }
    if ([bool]$eulerProfile.runtime_default) { Add-Issue 'Euler verification profile must not become a runtime default.' }
    if ($eulerProfile.sequence -cne 'ZYX' -or $eulerProfile.intrinsic_or_extrinsic -cne 'intrinsic') { Add-Issue 'Euler verification sequence/type drifted.' }
    Test-ExactSequence -Actual @($eulerProfile.component_order) -Expected @(
        'yaw_z', 'pitch_y', 'roll_x') -Label 'Euler component order'
    Test-ExactSequence -Actual @($eulerProfile.canonical_ranges) -Expected @(
        'yaw_z:[-pi,pi)', 'pitch_y:[-pi/2,pi/2]', 'roll_x:[-pi,pi)') -Label 'Euler canonical ranges'
    if ($eulerProfile.angle_unit -cne 'rad') { Add-Issue 'Euler verification angle unit drifted.' }
    if ($eulerProfile.singular_when -cne 'abs(cos(pitch_y)) <= policy_tolerance') { Add-Issue 'Euler singularity rule drifted.' }
    if ($eulerProfile.passive_mapping -cne 'q_I_B = inverse(q_z(yaw_z) * q_y(pitch_y) * q_x(roll_x))') { Add-Issue 'Euler passive mapping drifted.' }
    if ($conventions.quaternion.body_rate_derivative -cne 'q_dot_I_B = -0.5 * pure(omega_BI_B) * q_I_B') { Add-Issue 'Body-rate quaternion derivative drifted.' }
    if ($conventions.quaternion.inertial_rate_derivative -cne 'q_dot_I_B = -0.5 * q_I_B * pure(omega_BI_I)') { Add-Issue 'Inertial-rate quaternion derivative drifted.' }

    Test-ExactSequence -Actual @($conventions.time.public_types) -Expected @(
        'SimulationTime', 'Duration', 'SampleTime', 'ValidTime', 'WallTime') -Label 'Public time identities'
    $timeDomain = $conventions.time.domain_policy
    if (-not [bool]$timeDomain.finite_seconds_required) { Add-Issue 'Time values must remain finite.' }
    Test-ExactSequence -Actual @($timeDomain.clock_domain_required_for) -Expected @(
        'SimulationTime', 'SampleTime', 'ValidTime') -Label 'Clock-domain-required time identities'
    if ($timeDomain.cross_clock_arithmetic_disposition -cne 'DomainError') { Add-Issue 'Cross-clock arithmetic disposition drifted.' }
    if ($timeDomain.valid_interval_semantics -cne 'half_open_[valid_from,valid_until)') { Add-Issue 'Validity interval semantics drifted.' }
    if ($timeDomain.reversed_interval_disposition -cne 'DomainError') { Add-Issue 'Reversed validity interval disposition drifted.' }
    if ($conventions.time.fixed_step_authority -cne 'integer_tick') { Add-Issue 'Fixed-step authority must remain integer tick.' }
    if ($conventions.time.canonical_duration_unit -cne 's') { Add-Issue 'Canonical duration unit must remain seconds.' }
    if ($conventions.time.fixed_step_equation -cne 't_k = time_origin + tick * base_dt') { Add-Issue 'Fixed-step time equation drifted.' }
    if ([bool]$conventions.time.repeated_floating_addition_allowed) { Add-Issue 'Repeated floating time addition must remain forbidden.' }
    Test-ExactSequence -Actual @($conventions.time.duration_alignment_supported_v1) -Expected @(
        'ExactGrid', 'StopBefore', 'StopAfter') -Label 'Duration alignment policies'
    if ($conventions.time.duration_alignment_default -cne 'ExactGrid') { Add-Issue 'ExactGrid must remain the default duration alignment.' }
    if ($conventions.time.final_partial_step_v1 -cne 'unsupported') { Add-Issue 'FinalPartialStep must remain unsupported in v1.' }

    Test-ExactSequence -Actual @($conventions.verification.required_check_ids) -Expected $requiredCheckIds -Label 'Required scientific checks'
    if ([int]$conventions.verification.random_rotation_samples -lt 256) { Add-Issue 'Random rotation sample count must be at least 256.' }
    if ($conventions.verification.independent_reference -cne 'CPython standard library only') { Add-Issue 'Independent reference policy drifted.' }
}

if ($null -ne $cases -and $null -ne $conventions) {
    if ($cases.schema_version -cne 'gnczmkn.scientific-convention-cases/1') { Add-Issue 'Scientific case schema_version drifted.' }
    if ($cases.convention_id -cne $conventions.convention_id) { Add-Issue 'Scientific case convention id differs from the profile.' }
    if ([double]$cases.tolerance.absolute -ne [double]$conventions.verification.absolute_tolerance -or
        [double]$cases.tolerance.relative -ne [double]$conventions.verification.relative_tolerance) {
        Add-Issue 'Scientific case tolerance differs from the convention profile.'
    }
    $caseItems = @($cases.observations)
    if ($caseItems.Count -ne 16) { Add-Issue "Expected 16 scientific observations, got $($caseItems.Count)." }
    Test-UniqueIds -Items $caseItems -Label 'Scientific observations'
    foreach ($case in $caseItems) {
        $expectedValues = @($case.expected)
        if ($expectedValues.Count -eq 0) { Add-Issue "Scientific observation '$($case.id)' has no expected values." }
        for ($index = 0; $index -lt $expectedValues.Count; ++$index) {
            Test-FiniteNumber -Value $expectedValues[$index] -Label "Scientific observation '$($case.id)' expected[$index]"
        }
    }
}

if (Test-Path -LiteralPath $pythonSourcePath) {
    $allowedImports = @('argparse', 'json', 'math', 'pathlib', 'platform', 'random', 'sys')
    $pythonLines = Get-Content -LiteralPath $pythonSourcePath -Encoding utf8
    foreach ($line in $pythonLines) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^import\s+([A-Za-z0-9_]+)') {
            if ($Matches[1] -notin $allowedImports) { Add-Issue "Python reference imports non-approved module '$($Matches[1])'." }
        }
        elseif ($trimmed -match '^from\s+([A-Za-z0-9_]+)\s+import') {
            if ($Matches[1] -notin $allowedImports) { Add-Issue "Python reference imports non-approved module '$($Matches[1])'." }
        }
    }
    $pythonText = $pythonLines -join "`n"
    if ($pythonText -match '(?im)\b(subprocess|ctypes|importlib|numpy|scipy|matlab)\b') { Add-Issue 'Python reference contains a forbidden runtime/dependency bridge.' }
}

if (Test-Path -LiteralPath $cppSourcePath) {
    $cppText = Get-Content -LiteralPath $cppSourcePath -Raw -Encoding utf8
    if ($cppText -match '#include\s*"') { Add-Issue 'C++ property spike must use only standard-library includes.' }
    if ($cppText -match '(?i)reference[/\\]legacy|#include\s*[<"][^>"]*gnc[/\\]') { Add-Issue 'C++ property spike references product or Legacy code.' }
}

if (Test-Path -LiteralPath $cmakePath) {
    $cmakeText = Get-Content -LiteralPath $cmakePath -Raw -Encoding utf8
    if ($cmakeText -match 'target_link_libraries\s*\(\s*gnc_scientific_conventions\b') {
        Add-Issue 'Scientific convention spike must not link a product module.'
    }
}

function Test-Report {
    param(
        [object]$Report,
        [string]$ExpectedImplementationId,
        [string]$Label,
        [switch]$ReturnOnly
    )

    $localIssues = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $Report) {
        $localIssues.Add("$Label report is missing.")
        return @($localIssues)
    }
    if ($Report.schema_version -cne 'gnczmkn.scientific-check-output/1') { $localIssues.Add("$Label output schema drifted.") }
    if ($Report.convention_id -cne 'SCI-CONVENTIONS-001') { $localIssues.Add("$Label convention id drifted.") }
    if ($Report.implementation.id -cne $ExpectedImplementationId) { $localIssues.Add("$Label implementation id drifted.") }
    if ($Report.status -cne 'pass') { $localIssues.Add("$Label status is not pass.") }

    $checks = @($Report.checks)
    $checkIds = @($checks | Sort-Object id | ForEach-Object { [string]$_.id })
    $expectedSorted = @($requiredCheckIds | Sort-Object)
    if ($checkIds.Count -ne $expectedSorted.Count) {
        $localIssues.Add("$Label required check count mismatch.")
    }
    else {
        for ($index = 0; $index -lt $expectedSorted.Count; ++$index) {
            if ($checkIds[$index] -cne $expectedSorted[$index]) { $localIssues.Add("$Label required check identity mismatch."); break }
        }
    }
    foreach ($check in $checks) {
        if ($check.status -cne 'pass') { $localIssues.Add("$Label check '$($check.id)' failed.") }
        $assertionValue = [double]$check.assertion_count
        if ([double]::IsNaN($assertionValue) -or [double]::IsInfinity($assertionValue) -or
            $assertionValue -le 0.0 -or [Math]::Floor($assertionValue) -ne $assertionValue) {
            $localIssues.Add("$Label check '$($check.id)' has an invalid assertion_count.")
        }
        $errorValue = [double]$check.max_error
        if ([double]::IsNaN($errorValue) -or [double]::IsInfinity($errorValue) -or $errorValue -lt 0.0) {
            $localIssues.Add("$Label check '$($check.id)' has an invalid max_error.")
        }
    }

    $reportedObservations = @($Report.observations)
    $caseItems = @($cases.observations)
    if ($reportedObservations.Count -ne $caseItems.Count) {
        $localIssues.Add("$Label observation count mismatch.")
    }
    $reportById = @{}
    foreach ($observation in $reportedObservations) {
        $id = [string]$observation.id
        if ($reportById.ContainsKey($id)) { $localIssues.Add("$Label duplicate observation '$id'.") }
        else { $reportById[$id] = $observation }
    }

    $absolute = [double]$cases.tolerance.absolute
    $relative = [double]$cases.tolerance.relative
    foreach ($case in $caseItems) {
        $id = [string]$case.id
        if (-not $reportById.ContainsKey($id)) { $localIssues.Add("$Label missing observation '$id'."); continue }
        $actual = @($reportById[$id].values)
        $expected = @($case.expected)
        if ($actual.Count -ne $expected.Count) { $localIssues.Add("$Label observation '$id' value count mismatch."); continue }
        for ($index = 0; $index -lt $expected.Count; ++$index) {
            $actualValue = [double]$actual[$index]
            $expectedValue = [double]$expected[$index]
            if ([double]::IsNaN($actualValue) -or [double]::IsInfinity($actualValue)) {
                $localIssues.Add("$Label observation '$id' is non-finite at index $index.")
                continue
            }
            $difference = [Math]::Abs($actualValue - $expectedValue)
            $bound = $absolute + $relative * [Math]::Max([Math]::Abs($actualValue), [Math]::Abs($expectedValue))
            if ($id -eq 'quaternion.serialization-wxyz') {
                if ($actualValue -ne $expectedValue) { $localIssues.Add("$Label exact serialization differs at index $index.") }
            }
            elseif ($difference -gt $bound) {
                $localIssues.Add("$Label observation '$id' differs at index $index by $difference (bound $bound).")
            }
        }
    }

    if (-not $ReturnOnly) {
        foreach ($message in $localIssues) { Add-Issue $message }
    }
    return @($localIssues)
}

function Compare-Reports {
    param(
        [object]$CppReport,
        [object]$PythonReport,
        [switch]$ReturnOnly
    )
    $localIssues = [System.Collections.Generic.List[string]]::new()
    $pythonById = @{}
    foreach ($observation in @($PythonReport.observations)) { $pythonById[[string]$observation.id] = $observation }
    $maximum = 0.0
    $valueCount = 0
    $absolute = [double]$cases.tolerance.absolute
    $relative = [double]$cases.tolerance.relative
    foreach ($cppObservation in @($CppReport.observations)) {
        $id = [string]$cppObservation.id
        if (-not $pythonById.ContainsKey($id)) { $localIssues.Add("Python report is missing cross-tool observation '$id'."); continue }
        $cppValues = @($cppObservation.values)
        $pythonValues = @($pythonById[$id].values)
        if ($cppValues.Count -ne $pythonValues.Count) { $localIssues.Add("Cross-tool value count differs for '$id'."); continue }
        for ($index = 0; $index -lt $cppValues.Count; ++$index) {
            $cppValue = [double]$cppValues[$index]
            $pythonValue = [double]$pythonValues[$index]
            if ([double]::IsNaN($cppValue) -or [double]::IsInfinity($cppValue) -or
                [double]::IsNaN($pythonValue) -or [double]::IsInfinity($pythonValue)) {
                $localIssues.Add("Cross-tool observation '$id' is non-finite at index $index.")
                continue
            }
            $difference = [Math]::Abs($cppValue - $pythonValue)
            $maximum = [Math]::Max($maximum, $difference)
            ++$valueCount
            $bound = $absolute + $relative * [Math]::Max([Math]::Abs($cppValue), [Math]::Abs($pythonValue))
            if ($difference -gt $bound) { $localIssues.Add("Cross-tool observation '$id' differs at index $index by $difference (bound $bound).") }
        }
    }
    if (-not $ReturnOnly) { foreach ($message in $localIssues) { Add-Issue $message } }
    return [PSCustomObject]@{ Issues = @($localIssues); Maximum = $maximum; ValueCount = $valueCount }
}

function Invoke-CheckedNative {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$Label
    )
    $output = & $Executable @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode.`n$($output -join [Environment]::NewLine)"
    }
    return @($output)
}

function Test-PathBelow([string]$Candidate, [string]$Root) {
    $candidateFull = [System.IO.Path]::GetFullPath($Candidate)
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $candidateFull.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-MaxCheckError([object]$Report) {
    $maximum = 0.0
    foreach ($check in @($Report.checks)) { $maximum = [Math]::Max($maximum, [double]$check.max_error) }
    return $maximum
}

function Get-TotalAssertions([object]$Report) {
    $total = 0
    foreach ($check in @($Report.checks)) { $total += [int]$check.assertion_count }
    return $total
}

function New-EvidenceReport {
    param(
        [object]$CppReport,
        [object]$PythonReport,
        [object]$Comparison,
        [int]$FailureCaseCount
    )
    $inputEntries = @()
    foreach ($relativePath in $expectedInputPaths) {
        $absolutePath = Join-Path $repoRoot ($relativePath.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
        $inputEntries += [ordered]@{
            path = $relativePath
            sha256_normalized_utf8_lf = Get-NormalizedTextHash $absolutePath
        }
    }
    return [ordered]@{
        schema_version = 'gnczmkn.scientific-conventions-report/1'
        task_id = 'R0-SCI-001'
        convention_id = 'SCI-CONVENTIONS-001'
        status = 'pass'
        generated_at = (Get-Date).ToUniversalTime().ToString('o')
        environment = [ordered]@{
            os = [System.Environment]::OSVersion.VersionString
            architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
            powershell = $PSVersionTable.PSVersion.ToString()
            cpp_runtime = [string]$CppReport.implementation.runtime
            python_runtime = [string]$PythonReport.implementation.runtime
        }
        inputs = $inputEntries
        execution = [ordered]@{
            cpp = [ordered]@{
                implementation_id = [string]$CppReport.implementation.id
                checks = @($CppReport.checks).Count
                observations = @($CppReport.observations).Count
                assertions = Get-TotalAssertions $CppReport
                max_property_error = Get-MaxCheckError $CppReport
            }
            python = [ordered]@{
                implementation_id = [string]$PythonReport.implementation.id
                checks = @($PythonReport.checks).Count
                observations = @($PythonReport.observations).Count
                assertions = Get-TotalAssertions $PythonReport
                max_property_error = Get-MaxCheckError $PythonReport
            }
        }
        comparison = [ordered]@{
            observations_compared = @($CppReport.observations).Count
            numeric_values_compared = [int]$Comparison.ValueCount
            absolute_tolerance = [double]$cases.tolerance.absolute
            relative_tolerance = [double]$cases.tolerance.relative
            max_abs_difference = [double]$Comparison.Maximum
            mismatches = @($Comparison.Issues).Count
        }
        failure_path_tests = [ordered]@{
            cases = $FailureCaseCount
            rejected = $FailureCaseCount
            status = 'pass'
            covered = @('wrong direction', 'wrong serialization order', 'missing required check', 'zero executed assertions', 'wrong convention identity', 'cross-tool numeric drift')
        }
        isolation = [ordered]@{
            cpp = 'standard library only; no product or Legacy link'
            python = 'approved CPython standard-library imports only'
            runtime_consumers = 0
        }
    }
}

function Write-Evidence([object]$Evidence) {
    $directory = Split-Path -Parent $EvidencePath
    if (-not (Test-Path -LiteralPath $directory)) { [void](New-Item -ItemType Directory -Path $directory) }
    $json = $Evidence | ConvertTo-Json -Depth 12
    $json = $json.Replace("`r`n", "`n").Replace("`r", "`n") + "`n"
    [System.IO.File]::WriteAllText($EvidencePath, $json, [System.Text.UTF8Encoding]::new($false))
}

function Test-Evidence {
    $evidence = Read-Json $EvidencePath
    if ($null -eq $evidence) { return }
    if ($evidence.schema_version -cne 'gnczmkn.scientific-conventions-report/1') { Add-Issue 'Scientific evidence schema_version drifted.' }
    if ($evidence.task_id -cne 'R0-SCI-001' -or $evidence.convention_id -cne 'SCI-CONVENTIONS-001') { Add-Issue 'Scientific evidence identity drifted.' }
    if ($evidence.status -cne 'pass') { Add-Issue 'Scientific evidence status is not pass.' }
    if ([int]$evidence.comparison.mismatches -ne 0) { Add-Issue 'Scientific evidence records cross-tool mismatches.' }
    if ($evidence.failure_path_tests.status -cne 'pass') { Add-Issue 'Scientific failure-path evidence status is not pass.' }
    if ([int]$evidence.execution.cpp.checks -ne 23 -or [int]$evidence.execution.python.checks -ne 23) { Add-Issue 'Scientific evidence check counts drifted.' }
    if ([int]$evidence.execution.cpp.observations -ne 16 -or [int]$evidence.execution.python.observations -ne 16) { Add-Issue 'Scientific evidence observation counts drifted.' }
    if ([int]$evidence.execution.cpp.assertions -le 0 -or [int]$evidence.execution.python.assertions -le 0) { Add-Issue 'Scientific evidence assertion counts are incomplete.' }
    if ([int]$evidence.failure_path_tests.cases -lt 6 -or [int]$evidence.failure_path_tests.rejected -ne [int]$evidence.failure_path_tests.cases) { Add-Issue 'Scientific failure-path evidence is incomplete.' }
    Test-ExactSequence -Actual @($evidence.failure_path_tests.covered) -Expected @(
        'wrong direction', 'wrong serialization order', 'missing required check',
        'zero executed assertions', 'wrong convention identity', 'cross-tool numeric drift') -Label 'Scientific failure-path coverage'
    if ([int]$evidence.isolation.runtime_consumers -ne 0) { Add-Issue 'Scientific fixture unexpectedly has a runtime consumer.' }

    $evidenceInputs = @($evidence.inputs)
    $inputPaths = @($evidenceInputs | ForEach-Object { [string]$_.path })
    Test-ExactSequence -Actual $inputPaths -Expected $expectedInputPaths -Label 'Scientific evidence input index'
    foreach ($input in $evidenceInputs) {
        $relativePath = [string]$input.path
        $absolutePath = Join-Path $repoRoot ($relativePath.Replace('/', [System.IO.Path]::DirectorySeparatorChar))
        if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) { Add-Issue "Scientific evidence input is missing: $relativePath"; continue }
        $actualHash = Get-NormalizedTextHash $absolutePath
        if ($actualHash -cne [string]$input.sha256_normalized_utf8_lf) { Add-Issue "Scientific evidence hash drift: $relativePath" }
    }
}

$cppReport = $null
$pythonReport = $null
$comparison = $null
$failureCaseCount = 0

if ($StaticOnly -and $UpdateEvidence) {
    Add-Issue 'StaticOnly and UpdateEvidence cannot be combined.'
}

if (-not $StaticOnly) {
    if ([string]::IsNullOrWhiteSpace($CppExecutable)) {
        Add-Issue 'CppExecutable is required for dynamic scientific validation.'
    }
    elseif (-not (Test-Path -LiteralPath $CppExecutable -PathType Leaf)) {
        Add-Issue "C++ scientific executable is missing: $CppExecutable"
    }

    $pythonCommand = Get-Command $PythonExecutable -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) { Add-Issue "Python executable is unavailable: $PythonExecutable" }

    if ($issues.Count -eq 0) {
        $validationRoot = Join-Path $repoRoot 'build\r0-sci-001-validation'
        if (-not (Test-Path -LiteralPath $validationRoot)) { [void](New-Item -ItemType Directory -Path $validationRoot) }
        $runDirectory = Join-Path $validationRoot ([System.Guid]::NewGuid().ToString('N'))
        [void](New-Item -ItemType Directory -Path $runDirectory)
        try {
            $cppReportPath = Join-Path $runDirectory 'cpp-report.json'
            $pythonReportPath = Join-Path $runDirectory 'python-report.json'
            [void](Invoke-CheckedNative -Executable $CppExecutable -Arguments @('--report', $cppReportPath) -Label 'C++ scientific property spike')
            [void](Invoke-CheckedNative -Executable $pythonCommand.Source -Arguments @(
                '-I', '-B', $pythonSourcePath, '--conventions', $conventionsPath,
                '--cases', $casesPath, '--report', $pythonReportPath) -Label 'Python scientific reference')
            $cppReport = Read-Json $cppReportPath
            $pythonReport = Read-Json $pythonReportPath
            [void](Test-Report -Report $cppReport -ExpectedImplementationId 'cpp17-isolated-property-spike' -Label 'C++')
            [void](Test-Report -Report $pythonReport -ExpectedImplementationId 'cpython-stdlib-reference' -Label 'Python')
            if ($null -ne $cppReport -and $null -ne $pythonReport) {
                $comparison = Compare-Reports -CppReport $cppReport -PythonReport $pythonReport

                $wrongDirection = $cppReport | ConvertTo-Json -Depth 12 | ConvertFrom-Json
                $directionObservation = @($wrongDirection.observations | Where-Object id -eq 'quaternion.rotate-z90-x')[0]
                $directionObservation.values[1] = 1.0
                if (@(Test-Report -Report $wrongDirection -ExpectedImplementationId 'cpp17-isolated-property-spike' -Label 'failure-case direction' -ReturnOnly).Count -gt 0) { ++$failureCaseCount }

                $wrongSerialization = $cppReport | ConvertTo-Json -Depth 12 | ConvertFrom-Json
                $serialization = @($wrongSerialization.observations | Where-Object id -eq 'quaternion.serialization-wxyz')[0]
                $temporary = $serialization.values[0]
                $serialization.values[0] = $serialization.values[1]
                $serialization.values[1] = $temporary
                if (@(Test-Report -Report $wrongSerialization -ExpectedImplementationId 'cpp17-isolated-property-spike' -Label 'failure-case serialization' -ReturnOnly).Count -gt 0) { ++$failureCaseCount }

                $missingCheck = $cppReport | ConvertTo-Json -Depth 12 | ConvertFrom-Json
                $missingCheck.checks = @($missingCheck.checks | Select-Object -Skip 1)
                if (@(Test-Report -Report $missingCheck -ExpectedImplementationId 'cpp17-isolated-property-spike' -Label 'failure-case missing check' -ReturnOnly).Count -gt 0) { ++$failureCaseCount }

                $zeroAssertions = $cppReport | ConvertTo-Json -Depth 12 | ConvertFrom-Json
                $zeroAssertions.checks[0].assertion_count = 0
                if (@(Test-Report -Report $zeroAssertions -ExpectedImplementationId 'cpp17-isolated-property-spike' -Label 'failure-case zero assertions' -ReturnOnly).Count -gt 0) { ++$failureCaseCount }

                $wrongIdentity = $cppReport | ConvertTo-Json -Depth 12 | ConvertFrom-Json
                $wrongIdentity.convention_id = 'SCI-CONVENTIONS-BROKEN'
                if (@(Test-Report -Report $wrongIdentity -ExpectedImplementationId 'cpp17-isolated-property-spike' -Label 'failure-case identity' -ReturnOnly).Count -gt 0) { ++$failureCaseCount }

                $crossDrift = $pythonReport | ConvertTo-Json -Depth 12 | ConvertFrom-Json
                $crossDrift.observations[0].values[0] = 0.1
                $driftComparison = Compare-Reports -CppReport $cppReport -PythonReport $crossDrift -ReturnOnly
                if (@($driftComparison.Issues).Count -gt 0) { ++$failureCaseCount }

                if ($failureCaseCount -ne 6) { Add-Issue "Scientific failure-path suite rejected $failureCaseCount of 6 mutations." }
            }
        }
        finally {
            if ((Test-Path -LiteralPath $runDirectory) -and (Test-PathBelow -Candidate $runDirectory -Root $validationRoot)) {
                Remove-Item -LiteralPath $runDirectory -Recurse -Force
            }
        }
    }
}

if ($UpdateEvidence -and $issues.Count -eq 0) {
    if ($null -eq $comparison) { Add-Issue 'Dynamic comparison is required before evidence can be updated.' }
    else { Write-Evidence (New-EvidenceReport -CppReport $cppReport -PythonReport $pythonReport -Comparison $comparison -FailureCaseCount $failureCaseCount) }
}

Test-Evidence

if ($issues.Count -gt 0) {
    Write-Host "Scientific convention validation failed with $($issues.Count) issue(s):"
    foreach ($message in $issues) { Write-Host " - $message" }
    exit 1
}

if (-not $Quiet) {
    Write-Host 'Scientific convention validation passed.'
    Write-Host "Convention: SCI-CONVENTIONS-001"
    Write-Host "Required checks: $($requiredCheckIds.Count)"
    Write-Host "Fixture observations: $(@($cases.observations).Count)"
    if (-not $StaticOnly) {
        Write-Host "Cross-tool numeric values: $($comparison.ValueCount)"
        Write-Host "Cross-tool max abs difference: $($comparison.Maximum)"
        Write-Host "Rejected failure-path mutations: $failureCaseCount/6"
    }
    Write-Host "Evidence: $(Get-RelativePath $EvidencePath)"
}

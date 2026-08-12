[CmdletBinding()]
param(
    [switch]$Update,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$modulePath = Join-Path $PSScriptRoot 'modules\ArchitectureBaseline.psm1'
$reviewModulePath = Join-Path $PSScriptRoot 'modules\R0ArchitectureReview.psm1'
$strictJsonModulePath = Join-Path $PSScriptRoot 'modules\JsonSchemaSubset.psm1'
$registryRelativePath = 'docs/architecture/authority-registry.json'
$registryPath = Join-Path $repoRoot $registryRelativePath
$reviewContractPath = Join-Path $repoRoot 'docs\architecture\r0-architecture-review-contract.json'
$baselinePath = Join-Path $repoRoot 'docs\architecture\architecture-baseline.json'
$reportPath = Join-Path $repoRoot 'docs\quality\terminology-conformance-report.json'

Import-Module -Name $modulePath -Force
Import-Module -Name $reviewModulePath -Force
Import-Module -Name $strictJsonModulePath -Force

$unicodeSerializerProbe = ([string][char]0x4E2D) + [char]0x6587
$serializerProbe = [PSCustomObject]([ordered]@{
    nested = [PSCustomObject]([ordered]@{ answer = 42; enabled = $true })
    empty_array = [object[]]@()
    null_value = $null
    escaped = "quote`" slash\ newline`n <> " + [char]0x01
    decimal = [decimal]'1.25'
    unicode = $unicodeSerializerProbe
})
$expectedSerializerProbe = '{"nested":{"answer":42,"enabled":true},"empty_array":[],"null_value":null,"escaped":"quote\" slash\\ newline\n <> \u0001","decimal":1.25,"unicode":"' + $unicodeSerializerProbe + '"}' + "`n"
$actualSerializerProbe = ConvertTo-DeterministicJson -Value $serializerProbe
if ($actualSerializerProbe -cne $expectedSerializerProbe) {
    Write-Host 'Architecture deterministic JSON serializer probe failed.'
    exit 1
}

try {
    $registry = Read-StrictJsonFile -Path $registryPath
    $reviewContract = Read-StrictJsonFile -Path $reviewContractPath
    $glossaryPath = Join-Path $repoRoot ([string]$registry.terminology_authority)
    $adrPath = Join-Path $repoRoot ([string]$registry.dependency_authority)
    $cmakePath = Join-Path $repoRoot 'CMakeLists.txt'

    $glossaryText = Get-Content -LiteralPath $glossaryPath -Raw -Encoding utf8
    $glossary = ConvertFrom-GlossaryMarkdown -Text $glossaryText
    $dependency = ConvertFrom-DependencySources `
        -AdrText (Get-Content -LiteralPath $adrPath -Raw -Encoding utf8) `
        -CMakeText (Get-Content -LiteralPath $cmakePath -Raw -Encoding utf8)
}
catch {
    Write-Host "Architecture baseline input parsing failed: $($_.Exception.Message)"
    exit 1
}

$validation = Test-ArchitectureInputs -Glossary $glossary -Dependency $dependency -Registry $registry -RepoRoot $repoRoot
if (-not $validation.IsValid) {
    Write-Host "Architecture baseline validation failed with $($validation.Errors.Count) issue(s):"
    foreach ($message in @($validation.Errors)) { Write-Host " - $message" }
    exit 1
}

$baseline = New-ArchitectureBaseline `
    -Glossary $glossary `
    -Dependency $dependency `
    -Registry $registry `
    -RepoRoot $repoRoot `
    -RegistryRelativePath $registryRelativePath
$baselineJson = ConvertTo-DeterministicJson -Value $baseline

$negativeCases = Invoke-ArchitectureNegativeCases `
    -Glossary $glossary `
    -Dependency $dependency `
    -Registry $registry `
    -RepoRoot $repoRoot `
    -GlossaryText $glossaryText `
    -ExpectedBaselineJson $baselineJson
if (-not $negativeCases.IsValid) {
    Write-Host "Architecture baseline negative cases failed with $($negativeCases.Failures.Count) issue(s):"
    foreach ($message in @($negativeCases.Failures)) { Write-Host " - $message" }
    exit 1
}

$reviewNegativeCaseCount = @($reviewContract.required_mutation_ids).Count
$report = New-TerminologyConformanceReport `
    -Baseline $baseline `
    -BaselineJson $baselineJson `
    -NegativeCaseCount $negativeCases.CaseCount `
    -ReviewNegativeCaseCount $reviewNegativeCaseCount
$reportJson = ConvertTo-DeterministicJson -Value $report

if ($Update) {
    Write-Utf8NoBom -Path $baselinePath -Content $baselineJson
    Write-Utf8NoBom -Path $reportPath -Content $reportJson
}

$reviewValidation = Test-R0ArchitectureReviewContract `
    -Contract $reviewContract `
    -Registry $registry `
    -RepoRoot $repoRoot
if (-not $reviewValidation.IsValid) {
    Write-Host "R0 architecture review contract failed with $($reviewValidation.Errors.Count) issue(s):"
    foreach ($message in @($reviewValidation.Errors)) { Write-Host " - $message" }
    exit 1
}

$reviewMutations = Invoke-R0ArchitectureReviewMutations `
    -Contract $reviewContract `
    -Glossary $glossary `
    -Dependency $dependency `
    -Registry $registry `
    -RepoRoot $repoRoot `
    -GlossaryText $glossaryText `
    -AdrText (Get-Content -LiteralPath $adrPath -Raw -Encoding utf8) `
    -CMakeText (Get-Content -LiteralPath $cmakePath -Raw -Encoding utf8) `
    -ExpectedBaselineJson $baselineJson `
    -ExpectedReportJson $reportJson
if (-not $reviewMutations.IsValid) {
    Write-Host "R0 architecture review mutations failed with $($reviewMutations.Failures.Count) issue(s):"
    foreach ($message in @($reviewMutations.Failures)) { Write-Host " - $message" }
    exit 1
}

$drift = [System.Collections.Generic.List[string]]::new()
if (-not (Test-GeneratedContent -Path $baselinePath -Expected $baselineJson)) {
    [void]$drift.Add('docs/architecture/architecture-baseline.json is missing or stale; run with -Update.')
}
if (-not (Test-GeneratedContent -Path $reportPath -Expected $reportJson)) {
    [void]$drift.Add('docs/quality/terminology-conformance-report.json is missing or stale; run with -Update.')
}
if ($drift.Count -gt 0) {
    Write-Host "Architecture baseline derived artifacts failed with $($drift.Count) issue(s):"
    foreach ($message in $drift) { Write-Host " - $message" }
    exit 1
}

if (-not $Quiet) {
    Write-Host 'Architecture baseline validation passed.'
    Write-Host "Validated terms: $(@($glossary.Terms).Count)"
    Write-Host "Validated aliases: $(@($glossary.Aliases).Count)"
    Write-Host "Validated capabilities: $(@($glossary.Capabilities).Count)"
    Write-Host "Validated shared symbols: $(@($registry.shared_symbols).Count)"
    Write-Host "Validated Legacy ownership mappings: $(@($registry.legacy_ownership).Count)"
    Write-Host "Validated modules: $(@($dependency.ModuleNames).Count)"
    Write-Host "Validated CMake dependency edges: $(@($dependency.CMakeEdges).Count)"
    Write-Host "Validated negative cases: $($negativeCases.CaseCount)"
    Write-Host "Validated logical boundaries: $(@($reviewContract.logical_boundaries).Count)"
    Write-Host "Validated review-contract mutations: $($reviewMutations.CaseCount)"
}

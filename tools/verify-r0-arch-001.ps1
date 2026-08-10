[CmdletBinding()]
param(
    [switch]$UpdateGeneratedArtifacts,
    [switch]$SelfTest,
    [switch]$WriteReport
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$schemaVersion = 'gnczmkn.r0-architecture-baseline/1'
$baselineDate = '2026-08-09'
$termPath = 'specs/architecture/r0/terminology-baseline.json'
$dagPath = 'specs/architecture/r0/module-dependency-map.json'
$legacyPath = 'specs/architecture/r0/legacy-to-target-ownership-map.json'
$reportJsonPath = 'docs/quality/r0-arch-001/terminology-conformance-report.json'
$reportMarkdownPath = 'docs/quality/r0-arch-001/terminology-conformance-report.md'

function Resolve-Repo([string]$RelativePath) {
    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $RelativePath))
}

function Get-Sha([string]$RelativePath) {
    return (Get-FileHash -LiteralPath (Resolve-Repo $RelativePath) -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-AdrStatus([string]$RelativePath) {
    $text = Get-Content -LiteralPath (Resolve-Repo $RelativePath) -Raw -Encoding utf8
    $match = [regex]::Match($text, '(?m)^- Status: (?<status>[A-Za-z][A-Za-z0-9_-]*)\s*$')
    if (-not $match.Success) { throw "ADR status was not found: $RelativePath" }
    return $match.Groups['status'].Value
}

function New-Snapshot([string]$Path, [string]$Locator) {
    return [pscustomobject][ordered]@{ path = $Path; sha256 = Get-Sha $Path; locator = $Locator }
}

function Write-Utf8([string]$Path, [string]$Content) {
    $directory = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $directory)) {
        [void](New-Item -ItemType Directory -Path $directory)
    }
    [System.IO.File]::WriteAllText($Path, $Content, (New-Object System.Text.UTF8Encoding($false)))
}

function Write-Json([string]$RelativePath, $Value) {
    Write-Utf8 (Resolve-Repo $RelativePath) (($Value | ConvertTo-Json -Depth 100) + [Environment]::NewLine)
}

function Read-Json([string]$RelativePath) {
    $path = Resolve-Repo $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing JSON artifact: $RelativePath"
    }
    return Get-Content -LiteralPath $path -Raw -Encoding utf8 | ConvertFrom-Json
}

function Copy-Value($Value) {
    return $Value | ConvertTo-Json -Depth 100 | ConvertFrom-Json
}

function Get-Value($Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Is-ScalarText($Value) {
    return $Value -is [string] -and -not [string]::IsNullOrWhiteSpace($Value)
}

function Add-Issue($Issues, [string]$Message) {
    [void]$Issues.Add($Message)
}

function Split-TableRow([string]$Line) {
    $trimmed = $Line.Trim()
    if (-not $trimmed.StartsWith('|') -or -not $trimmed.EndsWith('|')) { return @() }
    $inner = $trimmed.Substring(1, $trimmed.Length - 2).Trim()
    return @([regex]::Split($inner, '(?<!\\)\s+\|\s+'))
}

function Get-GlossaryTerms {
    $path = Resolve-Repo 'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md'
    $terms = @()
    $seen = @{}
    $tick = [char]96
    foreach ($line in Get-Content -LiteralPath $path -Encoding utf8) {
        $parts = @(Split-TableRow $line)
        if ($parts.Count -notin @(4, 5)) { continue }
        $firstCell = [string]$parts[0]
        if (-not $firstCell.StartsWith([string]$tick)) { continue }
        $closingIndex = $firstCell.IndexOf($tick, 1)
        if ($closingIndex -lt 2) { continue }
        if ($parts[1] -notin @('Stable', 'V1', 'PressureOnly', 'Deferred', 'Legacy')) { continue }
        $name = $firstCell.Substring(1, $closingIndex - 1)
        if ($seen.ContainsKey($name)) { continue }
        $seen[$name] = $true
        $terms += [pscustomobject][ordered]@{
            canonical_name = $name
            status = $parts[1]
            definition = if ($parts.Count -eq 4) { $parts[2] } else { "$($parts[2]) Opening condition: $($parts[3])" }
            detail_authorities = if ($parts.Count -eq 4) { $parts[3] } else { $parts[4] }
            authority_ref = 'terminology-registry'
            registration_origin = 'reference-glossary'
        }
    }
    if ($terms.Count -lt 250) { throw "Glossary extraction returned only $($terms.Count) terms." }
    return $terms
}

function Get-GlossaryAliases {
    $path = Resolve-Repo 'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md'
    $text = Get-Content -LiteralPath $path -Raw -Encoding utf8
    $section = [regex]::Match($text, '(?s)## 2\. .*?(?=\r?\n## 3\.)').Value
    if ([string]::IsNullOrWhiteSpace($section)) { throw 'Glossary alias section was not found.' }
    $aliases = @()
    foreach ($line in $section -split '\r?\n') {
        $parts = @(Split-TableRow $line)
        if ($parts.Count -ne 3 -or $parts[0] -notmatch '[A-Za-z]' -or $parts[0] -match '^---') { continue }
        $aliases += [pscustomobject][ordered]@{
            retired_expression = $parts[0]
            canonical_relation = $parts[1]
            decision = $parts[2]
            authority_ref = 'terminology-registry'
        }
    }
    if ($aliases.Count -lt 15) { throw "Glossary extraction returned only $($aliases.Count) aliases." }
    return $aliases
}

function New-TermDocument {
    $supplementalTerms = @(
        [pscustomobject][ordered]@{ canonical_name = 'UnitId'; status = 'V1'; definition = 'ASCII, case-sensitive identity of a declared unit at a scientific boundary'; detail_authorities = 'ADR-0005'; authority_ref = 'terminology-registry'; registration_origin = 'ADR-0011 scientific overlay' },
        [pscustomobject][ordered]@{ canonical_name = 'FrameId'; status = 'V1'; definition = 'Namespace-qualified, versioned semantic frame identity with instance ownership recorded separately'; detail_authorities = 'ADR-0006'; authority_ref = 'terminology-registry'; registration_origin = 'ADR-0011 scientific overlay' },
        [pscustomobject][ordered]@{ canonical_name = 'SimulationTime'; status = 'V1'; definition = 'Logical time in a declared simulation clock domain with origin or epoch held in run metadata'; detail_authorities = 'ADR-0007'; authority_ref = 'terminology-registry'; registration_origin = 'ADR-0011 scientific overlay' },
        [pscustomobject][ordered]@{ canonical_name = 'Duration'; status = 'V1'; definition = 'Signed time interval expressed in canonical SI seconds'; detail_authorities = 'ADR-0007'; authority_ref = 'terminology-registry'; registration_origin = 'ADR-0011 scientific overlay' },
        [pscustomobject][ordered]@{ canonical_name = 'SampleTime'; status = 'V1'; definition = 'Time at which data was sampled or computed'; detail_authorities = 'ADR-0007'; authority_ref = 'terminology-registry'; registration_origin = 'ADR-0011 scientific overlay' },
        [pscustomobject][ordered]@{ canonical_name = 'ValidTime'; status = 'V1'; definition = 'Time or half-open interval during which data applies'; detail_authorities = 'ADR-0007'; authority_ref = 'terminology-registry'; registration_origin = 'ADR-0011 scientific overlay' },
        [pscustomobject][ordered]@{ canonical_name = 'WallTime'; status = 'V1'; definition = 'Real-world audit time isolated from scientific simulation time'; detail_authorities = 'ADR-0007'; authority_ref = 'terminology-registry'; registration_origin = 'ADR-0011 scientific overlay' },
        [pscustomobject][ordered]@{ canonical_name = 'R_to_from'; status = 'V1'; definition = 'Proper orthogonal matrix mapping coordinates of a free vector from the from frame into the to frame'; detail_authorities = 'ADR-0006'; authority_ref = 'terminology-registry'; registration_origin = 'ADR-0011 scientific overlay' },
        [pscustomobject][ordered]@{ canonical_name = 'q_to_from'; status = 'V1'; definition = 'Hamilton quaternion representing the passive coordinate transformation paired with R_to_from'; detail_authorities = 'ADR-0008'; authority_ref = 'terminology-registry'; registration_origin = 'ADR-0011 scientific overlay' }
    )
    $enums = @(
        [pscustomobject][ordered]@{ name = 'CapabilityStatus'; values = @('V1','Stable','PressureOnly','Deferred','Legacy'); authority_ref = 'terminology-registry'; owner_role = 'architecture_lead'; source_locator = 'reference-glossary section 1' },
        [pscustomobject][ordered]@{ name = 'AuthorityDomain'; values = @('Design/Plan','Model','Operation','Artifact'); authority_ref = 'terminology-registry'; owner_role = 'architecture_lead'; source_locator = '02 section 1.3' },
        [pscustomobject][ordered]@{ name = 'ChangeVectorDimension'; values = @('V','G','S','T','I','R','X'); authority_ref = 'terminology-registry'; owner_role = 'architecture_lead'; source_locator = '02 section 1.2' },
        [pscustomobject][ordered]@{ name = 'ChangeClass'; values = @('A','B','C','D','E','F'); authority_ref = 'terminology-registry'; owner_role = 'architecture_lead'; source_locator = '02 section 9' },
        [pscustomobject][ordered]@{ name = 'EvidenceValidity'; values = @('Valid','ValidWithCaveats','Partial','Invalid','Unknown'); authority_ref = 'terminology-registry'; owner_role = 'evidence_workflow_lead'; source_locator = 'reference-glossary EvidenceValidity' },
        [pscustomobject][ordered]@{ name = 'EvidenceCriticality'; values = @('CriticalEvidence','RequiredMetricInput','OperationalTelemetry','BestEffortDisplay','DebugOnly'); authority_ref = 'terminology-registry'; owner_role = 'evidence_workflow_lead'; source_locator = 'reference-glossary EvidenceCriticality' },
        [pscustomobject][ordered]@{ name = 'ObservationKind'; values = @('InitialAtT0','CycleAtTk','RestoredAtCheckpoint'); authority_ref = 'terminology-registry'; owner_role = 'evidence_workflow_lead'; source_locator = 'reference-glossary ObservationKind' },
        [pscustomobject][ordered]@{ name = 'TemporalRelation'; values = @('CurrentCycle','PreviousCommitted','HeldLatest','IntervalModel','CandidateStateQuery','EventAtOrBefore'); authority_ref = 'terminology-registry'; owner_role = 'architecture_lead'; source_locator = 'reference-glossary TemporalRelation' },
        [pscustomobject][ordered]@{ name = 'ExecutionObligation'; values = @('PublishProjection','BoundaryEvaluation','IntervalEvolution','DerivativeEvaluation','SourceFreeze','PostCommitEffect','ResourceLease'); authority_ref = 'terminology-registry'; owner_role = 'runtime_numerics_lead'; source_locator = 'reference-glossary ExecutionObligation' },
        [pscustomobject][ordered]@{ name = 'ExecutionRegion'; values = @('Publish','Boundary','Integration','Commit','PostCommit'); authority_ref = 'terminology-registry'; owner_role = 'runtime_numerics_lead'; source_locator = 'reference-glossary ExecutionRegionPlan' },
        [pscustomobject][ordered]@{ name = 'ClosureStrategy'; values = @('FrozenInterval','CandidateState','AlgebraicSolve'); authority_ref = 'terminology-registry'; owner_role = 'runtime_numerics_lead'; source_locator = 'reference-glossary ClosureStrategy' },
        [pscustomobject][ordered]@{ name = 'CanonicalSIUnitId'; values = @('m','s','kg','rad','m/s','m/s^2','rad/s','N','N*m','Pa','K'); authority_ref = 'terminology-registry'; owner_role = 'scientific_authority'; source_locator = 'ADR-0005 Decision 3' },
        [pscustomobject][ordered]@{ name = 'ScientificTimeKind'; values = @('SimulationTime','Duration','SampleTime','ValidTime','WallTime'); authority_ref = 'terminology-registry'; owner_role = 'scientific_authority'; source_locator = 'ADR-0007 Decision 1' }
    )
    $keys = @(
        [pscustomobject][ordered]@{ name = 'RuntimeCellIdentity'; components = @('SessionId','RuntimeInstanceId'); derivation = 'ordered tuple'; authority_ref = 'terminology-registry'; owner_role = 'architecture_lead'; source_locator = 'reference-glossary RuntimeCell' },
        [pscustomobject][ordered]@{ name = 'PreparedModelKey'; components = @('definition_id','definition_version','canonical_occurrence_config_hash','asset_set_hash','implementation_id','implementation_version','numerical_policy_hash','preparation_policy_hash'); derivation = 'canonical ordered field set'; authority_ref = 'terminology-registry'; owner_role = 'compiler_lead'; source_locator = 'reference-glossary PreparedModelKey' },
        [pscustomobject][ordered]@{ name = 'CaseIdDerivation'; components = @('ExperimentDefinitionHash','CanonicalParameterValues','ReplicateKey'); derivation = 'deterministic derivation'; authority_ref = 'terminology-registry'; owner_role = 'evidence_workflow_lead'; source_locator = 'reference-glossary CaseId' },
        [pscustomobject][ordered]@{ name = 'QuaternionCoefficientOrder'; components = @('w','x','y','z'); derivation = 'ordered serialized coefficient sequence'; authority_ref = 'terminology-registry'; owner_role = 'scientific_authority'; source_locator = 'ADR-0008 Decision 5' }
    )
    $domains = @(
        [pscustomobject][ordered]@{ name = 'Design/Plan'; authoritative_fact = 'versioned definitions, canonical graphs, bindings, execution choices, and version locks'; typical_owner = 'Package/Catalog and Mission/Workflow Compiler'; commit_result = 'definition release or immutable plan plus proof index'; authority_ref = 'terminology-registry'; owner_role = 'architecture_lead'; source_locator = '02 section 1.3' },
        [pscustomobject][ordered]@{ name = 'Model'; authoritative_fact = 'physical, discrete, control, and entity lifecycle state'; typical_owner = 'Simulation Session transaction'; commit_result = 'ModelCommit plus state or topology revision'; authority_ref = 'terminology-registry'; owner_role = 'runtime_numerics_lead'; source_locator = '02 section 1.3' },
        [pscustomobject][ordered]@{ name = 'Operation'; authoritative_fact = 'command ledger, task or operation lifecycle, approval, and resource occupancy'; typical_owner = 'Application or Workflow operation owner'; commit_result = 'acceptance, application, or task receipt plus Outcome'; authority_ref = 'terminology-registry'; owner_role = 'application_lead'; source_locator = '02 section 1.3' },
        [pscustomobject][ordered]@{ name = 'Artifact'; authoritative_fact = 'immutable payload, dataset, manifest, lineage, and validity'; typical_owner = 'RecordPipeline or Artifact Store'; commit_result = 'EvidenceCommit or ArtifactCommit plus durability receipt'; authority_ref = 'terminology-registry'; owner_role = 'evidence_workflow_lead'; source_locator = '02 section 1.3' }
    )
    return [pscustomobject][ordered]@{
        schema_version = $schemaVersion
        document_kind = 'terminology_baseline'
        task_id = 'R0-ARCH-001'
        baseline_date = $baselineDate
        maturity = 'Accepted R0 governance baseline governed by ADR-0010 and ADR-0011'
        registry = [pscustomobject][ordered]@{
            id = 'terminology-registry'
            authority_path = 'docs/adr/0011-r0-scientific-terminology-overlay.md'
            imported_registry_path = 'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md'
            owner_role = 'architecture_lead'
            authority_rule = 'canonical spelling and registry identity are singular; detail_authorities are informative semantic references'
        }
        source_snapshots = @(
            (New-Snapshot 'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md' 'sections 1 through 9'),
            (New-Snapshot 'design-notes/gnczmkn-architecture-roadmap/02-layered-reference-architecture.md' 'sections 1.2, 1.3, 9, and 13'),
            (New-Snapshot 'docs/adr/0010-r0-architecture-baseline-format.md' 'Decision and verification'),
            (New-Snapshot 'docs/adr/0011-r0-scientific-terminology-overlay.md' 'Decision and verification'),
            (New-Snapshot 'docs/adr/0005-si-unit-and-numeric-conventions.md' 'Decision'),
            (New-Snapshot 'docs/adr/0006-frame-transform-conventions.md' 'Decision'),
            (New-Snapshot 'docs/adr/0007-time-value-conventions.md' 'Decision'),
            (New-Snapshot 'docs/adr/0008-quaternion-conventions.md' 'Decision')
        )
        terms = @(@(Get-GlossaryTerms) + $supplementalTerms)
        retired_aliases = @(Get-GlossaryAliases)
        enums = $enums
        keys = $keys
        authority_domains = $domains
    }
}

function New-Module([string]$Id, [string]$Kind, [string]$OwnerRole, [string[]]$Dependencies, $CMakeTarget, [bool]$InManifest) {
    return [pscustomobject][ordered]@{
        id = $Id
        kind = $Kind
        owner_role = $OwnerRole
        authority_ref = 'module-dependency-registry'
        direct_dependencies = @($Dependencies)
        cmake_target = $CMakeTarget
        declared_in_project_manifest = $InManifest
    }
}

function New-DagDocument {
    $modules = @(
        (New-Module 'foundation' 'cmake_interface' 'runtime_numerics_lead' @() 'foundation' $true),
        (New-Module 'contracts' 'cmake_interface' 'architecture_lead' @('foundation') 'contracts' $true),
        (New-Module 'model_sdk' 'cmake_interface' 'model_sdk_lead' @('foundation','contracts') 'model_sdk' $true),
        (New-Module 'compiler' 'cmake_interface' 'compiler_lead' @('foundation','contracts','model_sdk') 'compiler' $true),
        (New-Module 'kernel' 'cmake_interface' 'runtime_numerics_lead' @('foundation','contracts') 'kernel' $true),
        (New-Module 'evidence' 'cmake_interface' 'evidence_workflow_lead' @('foundation','contracts') 'evidence' $true),
        (New-Module 'workflow' 'cmake_interface' 'evidence_workflow_lead' @('foundation','contracts','evidence') 'workflow' $true),
        (New-Module 'application' 'cmake_interface' 'application_lead' @('foundation','contracts','compiler','kernel','evidence','workflow') 'application' $true),
        (New-Module 'adapters' 'cmake_interface' 'application_lead' @('foundation','contracts','application') 'adapters' $true),
        (New-Module 'packages_user' 'logical_contribution_boundary' 'model_sdk_lead' @('model_sdk') $null $false),
        (New-Module 'composition_root' 'logical_composition_boundary' 'application_lead' @('application','adapters','packages_user') $null $false)
    )
    return [pscustomobject][ordered]@{
        schema_version = $schemaVersion
        document_kind = 'module_dependency_map'
        task_id = 'R0-ARCH-001'
        baseline_date = $baselineDate
        maturity = 'Accepted R0 governance baseline governed by ADR-0010 and ADR-0011'
        registry = [pscustomobject][ordered]@{
            id = 'module-dependency-registry'
            authority_path = 'docs/adr/0003-initial-module-dependency-dag.md'
            owner_role = 'architecture_lead'
            edge_direction = 'consumer_to_dependency'
        }
        source_snapshots = @(
            (New-Snapshot 'design-notes/gnczmkn-architecture-roadmap/02-layered-reference-architecture.md' 'section 13'),
            (New-Snapshot 'docs/adr/0003-initial-module-dependency-dag.md' 'Decision and Verification'),
            (New-Snapshot 'docs/adr/0010-r0-architecture-baseline-format.md' 'Decision and verification'),
            (New-Snapshot 'project-manifest.json' 'modules'),
            (New-Snapshot 'CMakeLists.txt' 'interface module declarations and links')
        )
        modules = $modules
        forbidden_reachability = @(
            [pscustomobject][ordered]@{ from = 'kernel'; to = 'compiler'; reason = 'Kernel cannot depend on Compiler.'; authority_ref = 'module-dependency-registry' },
            [pscustomobject][ordered]@{ from = 'compiler'; to = 'packages_user'; reason = 'Compiler consumes descriptor protocols without package implementations.'; authority_ref = 'module-dependency-registry' },
            [pscustomobject][ordered]@{ from = 'workflow'; to = 'kernel'; reason = 'Workflow cannot consume Session internals.'; authority_ref = 'module-dependency-registry' }
        )
    }
}

function New-LegacyDocument {
    $rows = @(
        'IContinuousGroup|registered|continuous-group-plan|Design/Plan|compiler_lead|compiler|IntegrationScopePlan,ClosurePlan,SolverIslandPlan',
        'SimulationNode|registered|simulation-node-definition|Design/Plan|model_sdk_lead|model_sdk|ModelDefinition,RuntimeComponent,RuntimeCellRecipe',
        'NodeFactory|registered|node-factory-contribution|Design/Plan|model_sdk_lead|packages_user|PackageManifest,ModelDefinition,ModelPrepareFactory,RuntimeCellFactory',
        'NodeRegistry|registered|node-registry-plan-handles|Design/Plan|compiler_lead|compiler|ExecutionPlanImage,BindingPlan',
        'NodeRegistry|registered|node-registry-session-state|Model|runtime_numerics_lead|kernel|SessionRuntimeBindings,CommittedStateStore,CommittedOutputStore',
        'FrameworkCatalog|registered|framework-catalog-view|Design/Plan|compiler_lead|compiler|ModelPackage,PackageManifest,DefinitionRef',
        'AssemblyContext|registered|assembly-context-lowering|Design/Plan|compiler_lead|compiler|BindingPlan,ExecutionPlanImage',
        'ConfigNode|registered|config-node-source|Design/Plan|compiler_lead|compiler|SourceTree,CompiledModelOccurrence',
        'ConfigReader|registered|config-reader-frontend|Design/Plan|compiler_lead|compiler|SourceFrontend,ModelDefinition',
        'ConfigManager|registered|config-manager-compile|Design/Plan|compiler_lead|compiler|SourceFrontend,MissionIR,CompilationOutcome',
        'MissionAssembler|registered|mission-assembler-passes|Design/Plan|compiler_lead|compiler|MissionIR,ExecutionPlanDescriptor,LinkOutcome',
        'SimulationBuilder|registered|simulation-builder-plan|Design/Plan|compiler_lead|compiler|CompilationOutcome,LinkOutcome',
        'SimulationBuilder|registered|simulation-builder-operation|Operation|application_lead|application|ApplicationControlPlane,SessionCreateOutcome,OperationReceipt',
        'ExecutionPhaseManager|registered|execution-phase-plan|Design/Plan|compiler_lead|compiler|ExecutionRegionPlan,BoundaryDagPlan',
        'ExecutionPhaseManager|registered|execution-phase-transaction|Model|runtime_numerics_lead|kernel|StepTransaction,StepOutcome',
        'IDiscreteTask|registered|discrete-task-obligation|Design/Plan|model_sdk_lead|model_sdk|BoundaryEvaluation,RuntimeComponentDescriptor',
        'DiscreteNode|registered|discrete-node-descriptor|Design/Plan|model_sdk_lead|model_sdk|RuntimeComponentDescriptor,BoundaryEvaluation',
        'IObservable|registered|observable-projection-plan|Design/Plan|compiler_lead|compiler|ObservationProjectionPlan,FieldDescriptor',
        'ObservableField|registered|observable-field-contract|Artifact|evidence_workflow_lead|evidence|FieldDescriptor,FieldId',
        'IRecordSink|registered|record-sink-port|Artifact|evidence_workflow_lead|evidence|RecordSink,RecordOutcome',
        'AutoDataLogger|registered|auto-data-recording|Artifact|evidence_workflow_lead|evidence|RecordPipeline,DatasetSink,EncodingPlan',
        'SimFlow|registered|simflow-experiment|Operation|evidence_workflow_lead|workflow|ExperimentDefinition,CaseMaterialization,WorkflowPlan,TaskOutcome',
        'OnboardState|registered|onboard-state-contracts|Design/Plan|architecture_lead|contracts|NavigationEstimate,GuidanceCommand,ActuatorCommand',
        'OnboardStateProcess|registered|onboard-state-input-view|Design/Plan|compiler_lead|compiler|InputBundleView,BindingPlan',
        'GuidanceProcess|registered|guidance-process-recipe|Design/Plan|model_sdk_lead|packages_user|AlgorithmKernel,RuntimeCellRecipe,GuidanceCommand',
        'Simulator|registered|simulator-session|Model|runtime_numerics_lead|kernel|SessionRuntimeBindings,StepTransaction,RunOutcome',
        'Simulator|registered|simulator-control|Operation|application_lead|application|SessionCreateOutcome,SessionHandle',
        'Simulator|registered|simulator-comparison|Operation|evidence_workflow_lead|workflow|WorkflowPlan,TaskOutcome',
        'IIntegrator|audit-only|integrator-plan|Design/Plan|compiler_lead|compiler|IntegrationScopePlan,NumericalPolicy',
        'IIntegrator|audit-only|integrator-outcome|Model|runtime_numerics_lead|foundation|NumericalOutcome',
        'ISummaryObserver|audit-only|summary-observer-metrics|Artifact|evidence_workflow_lead|evidence|MetricResult,Artifact,EvidenceBundle',
        'SimulationSummary|audit-only|simulation-summary-workflow|Artifact|evidence_workflow_lead|workflow|MetricResult,EvidenceBundle',
        'math_types.hpp|audit-only|math-types-contract-boundaries|Design/Plan|architecture_lead|contracts|StateSchema,PortDescriptor,FieldDescriptor'
    )
    $parsed = @($rows | ForEach-Object {
        $parts = $_.Split('|')
        [pscustomobject]@{
            legacy_name = $parts[0]
            registry_status = $parts[1]
            responsibility = [pscustomobject][ordered]@{
                id = $parts[2]
                authority_domain = $parts[3]
                owner_role = $parts[4]
                target_module = $parts[5]
                target_terms = @($parts[6].Split(','))
                authority_ref = 'legacy-ownership-registry'
            }
        }
    })
    $mappings = @($parsed | Group-Object legacy_name | ForEach-Object {
        $status = @($_.Group | Select-Object -ExpandProperty registry_status -Unique)
        if ($status.Count -ne 1) { throw "Conflicting registry status for $($_.Name)." }
        [pscustomobject][ordered]@{
            legacy_name = $_.Name
            registry_status = $status[0]
            source_refs = if ($status[0] -eq 'registered') { @('reference-glossary Legacy registry','01 section 16') } else { @('01 section 16') }
            responsibilities = @($_.Group | ForEach-Object { $_.responsibility })
        }
    } | Sort-Object legacy_name)
    return [pscustomobject][ordered]@{
        schema_version = $schemaVersion
        document_kind = 'legacy_to_target_ownership_map'
        task_id = 'R0-ARCH-001'
        baseline_date = $baselineDate
        maturity = 'Accepted R0 governance baseline governed by ADR-0010 and ADR-0011'
        registry = [pscustomobject][ordered]@{
            id = 'legacy-ownership-registry'
            authority_path = 'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md'
            owner_role = 'architecture_lead'
            ownership_rule = 'each target responsibility has one AuthorityDomain, one accountable role, and one target module'
        }
        source_snapshots = @(
            (New-Snapshot 'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md' 'all Legacy terms and section 9'),
            (New-Snapshot 'design-notes/gnczmkn-architecture-roadmap/01-current-architecture-deep-audit.md' 'section 16'),
            (New-Snapshot 'docs/adr/0010-r0-architecture-baseline-format.md' 'Decision and verification')
        )
        mappings = $mappings
    }
}

function Get-RoleSet {
    $set = @{}
    foreach ($role in @(Get-Value (Read-Json 'docs/team/role-assignments.json') 'roles')) {
        $id = [string](Get-Value $role 'id')
        if (-not [string]::IsNullOrWhiteSpace($id)) { $set[$id] = $true }
    }
    return $set
}

function Test-Unique($Entries, [string]$Property, [string]$Label, $Issues) {
    $seen = @{}
    foreach ($entry in @($Entries)) {
        $value = Get-Value $entry $Property
        if (-not (Is-ScalarText $value)) {
            Add-Issue $Issues "$Label entry has no scalar $Property."
            continue
        }
        $key = ([string]$value).ToLowerInvariant()
        if ($seen.ContainsKey($key)) { Add-Issue $Issues "Duplicate $Label identity: $value" }
        else { $seen[$key] = $true }
    }
}

function Test-Authority($Entry, [string]$Label, $Roles, $Issues) {
    $authority = Get-Value $Entry 'authority_ref'
    if (-not (Is-ScalarText $authority)) { Add-Issue $Issues "$Label must have exactly one scalar authority_ref." }
    $owner = Get-Value $Entry 'owner_role'
    if (-not (Is-ScalarText $owner)) { Add-Issue $Issues "$Label must have exactly one scalar owner_role." }
    elseif (-not $Roles.ContainsKey([string]$owner)) { Add-Issue $Issues "$Label references undeclared role $owner." }
}

function Test-TermDocument($Document, $Roles, $Issues) {
    if ((Get-Value $Document 'schema_version') -ne $schemaVersion) { Add-Issue $Issues 'Terminology schema_version is invalid.' }
    if ((Get-Value $Document 'document_kind') -ne 'terminology_baseline') { Add-Issue $Issues 'Terminology document_kind is invalid.' }
    $registry = Get-Value $Document 'registry'
    $registryId = Get-Value $registry 'id'
    if (-not (Is-ScalarText $registryId)) { Add-Issue $Issues 'Terminology registry id must be scalar.' }
    $registryOwner = Get-Value $registry 'owner_role'
    if (-not (Is-ScalarText $registryOwner) -or -not $Roles.ContainsKey([string]$registryOwner)) {
        Add-Issue $Issues 'Terminology registry owner must be a declared scalar role.'
    }

    $terms = @(Get-Value $Document 'terms')
    Test-Unique $terms 'canonical_name' 'canonical term' $Issues
    foreach ($term in $terms) {
        $name = Get-Value $term 'canonical_name'
        $authority = Get-Value $term 'authority_ref'
        if (-not (Is-ScalarText $authority) -or $authority -ne $registryId) {
            Add-Issue $Issues "Term $name has zero or multiple registry authorities."
        }
    }

    $aliases = @(Get-Value $Document 'retired_aliases')
    Test-Unique $aliases 'retired_expression' 'retired alias' $Issues
    foreach ($alias in $aliases) {
        $name = Get-Value $alias 'retired_expression'
        $authority = Get-Value $alias 'authority_ref'
        if (-not (Is-ScalarText $authority) -or $authority -ne $registryId) {
            Add-Issue $Issues "Alias $name has zero or multiple registry authorities."
        }
    }

    foreach ($collectionName in @('enums','keys','authority_domains')) {
        $entries = @(Get-Value $Document $collectionName)
        Test-Unique $entries 'name' $collectionName $Issues
        foreach ($entry in $entries) {
            $name = Get-Value $entry 'name'
            Test-Authority $entry "$collectionName $name" $Roles $Issues
            if ((Get-Value $entry 'authority_ref') -ne $registryId) {
                Add-Issue $Issues "$collectionName $name does not resolve to the terminology registry."
            }
        }
    }

    foreach ($enum in @(Get-Value $Document 'enums')) {
        $name = Get-Value $enum 'name'
        $values = @(Get-Value $enum 'values')
        if ($values.Count -eq 0) { Add-Issue $Issues "Enum $name has no values."; continue }
        $seen = @{}
        foreach ($value in $values) {
            if (-not (Is-ScalarText $value)) { Add-Issue $Issues "Enum $name has a non-scalar value."; continue }
            $key = ([string]$value).ToLowerInvariant()
            if ($seen.ContainsKey($key)) { Add-Issue $Issues "Enum $name repeats value $value." }
            else { $seen[$key] = $true }
        }
    }
    foreach ($keyEntry in @(Get-Value $Document 'keys')) {
        if (@(Get-Value $keyEntry 'components').Count -eq 0) {
            Add-Issue $Issues "Key $((Get-Value $keyEntry 'name')) has no components."
        }
    }
    $expectedDomains = @('Artifact','Design/Plan','Model','Operation')
    $actualDomains = @(@(Get-Value $Document 'authority_domains') | ForEach-Object { Get-Value $_ 'name' } | Sort-Object)
    if (($expectedDomains -join '|') -ne ($actualDomains -join '|')) {
        Add-Issue $Issues 'AuthorityDomain registry is incomplete.'
    }

    if ((Get-Value $registry 'authority_path') -ne 'docs/adr/0011-r0-scientific-terminology-overlay.md') {
        Add-Issue $Issues 'Terminology registry does not resolve to the R0 overlay authority.'
    }
    $supplementalNames = @('UnitId','FrameId','SimulationTime','Duration','SampleTime','ValidTime','WallTime','R_to_from','q_to_from')
    foreach ($supplementalName in $supplementalNames) {
        $matches = @($terms | Where-Object { (Get-Value $_ 'canonical_name') -eq $supplementalName })
        if ($matches.Count -ne 1 -or (Get-Value $matches[0] 'registration_origin') -ne 'ADR-0011 scientific overlay') {
            Add-Issue $Issues "Scientific overlay term $supplementalName must be registered exactly once."
        }
    }
    $enumByName = @{}
    foreach ($enum in @(Get-Value $Document 'enums')) { $enumByName[[string](Get-Value $enum 'name')] = $enum }
    $expectedUnits = @('m','s','kg','rad','m/s','m/s^2','rad/s','N','N*m','Pa','K')
    $actualUnits = if ($enumByName.ContainsKey('CanonicalSIUnitId')) { @(Get-Value $enumByName['CanonicalSIUnitId'] 'values') } else { @() }
    if (($actualUnits -join '|') -ne ($expectedUnits -join '|')) { Add-Issue $Issues 'CanonicalSIUnitId differs from ADR-0005.' }
    foreach ($unitId in $actualUnits) {
        if ([string]$unitId -notmatch '^[\x20-\x7E]+$') { Add-Issue $Issues "Canonical unit id is not ASCII: $unitId" }
    }
    $expectedTimeKinds = @('SimulationTime','Duration','SampleTime','ValidTime','WallTime')
    $actualTimeKinds = if ($enumByName.ContainsKey('ScientificTimeKind')) { @(Get-Value $enumByName['ScientificTimeKind'] 'values') } else { @() }
    if (($actualTimeKinds -join '|') -ne ($expectedTimeKinds -join '|')) { Add-Issue $Issues 'ScientificTimeKind differs from ADR-0007.' }
    $coefficientKey = @(@(Get-Value $Document 'keys') | Where-Object { (Get-Value $_ 'name') -eq 'QuaternionCoefficientOrder' })
    $actualOrder = if ($coefficientKey.Count -eq 1) { @(Get-Value $coefficientKey[0] 'components') } else { @() }
    if (($actualOrder -join '|') -ne 'w|x|y|z') { Add-Issue $Issues 'QuaternionCoefficientOrder differs from ADR-0008.' }
}

function Find-Path([string]$Start, [string]$Target, $ById) {
    if (-not $ById.ContainsKey($Start) -or -not $ById.ContainsKey($Target)) { return @() }
    $queue = New-Object 'System.Collections.Generic.Queue[string]'
    $seen = @{}
    $parent = @{}
    $queue.Enqueue($Start)
    $seen[$Start] = $true
    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        if ($current -eq $Target) {
            $path = @($current)
            while ($parent.ContainsKey($current)) {
                $current = $parent[$current]
                $path = @($current) + $path
            }
            return $path
        }
        foreach ($dependency in @(Get-Value $ById[$current] 'direct_dependencies')) {
            $id = [string]$dependency
            if ($ById.ContainsKey($id) -and -not $seen.ContainsKey($id)) {
                $seen[$id] = $true
                $parent[$id] = $current
                $queue.Enqueue($id)
            }
        }
    }
    return @()
}

function Get-CMakeDependencies {
    $text = Get-Content -LiteralPath (Resolve-Repo 'CMakeLists.txt') -Raw -Encoding utf8
    $result = @{}
    $pattern = 'target_link_libraries\(\s*(?<target>[A-Za-z0-9_]+)\s+(?<visibility>INTERFACE|PUBLIC|PRIVATE)\s+(?<deps>[^\)]*)\)'
    foreach ($match in [regex]::Matches($text, $pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
        $target = $match.Groups['target'].Value
        $result[$target] = @($match.Groups['deps'].Value -split '\s+' | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_) -and $_ -notin @('INTERFACE','PUBLIC','PRIVATE','gnc_build_options')
        })
    }
    return $result
}

function Test-CMakeMap($Document, $Issues) {
    $text = Get-Content -LiteralPath (Resolve-Repo 'CMakeLists.txt') -Raw -Encoding utf8
    $actualByTarget = Get-CMakeDependencies
    $modules = @(Get-Value $Document 'modules')
    $cmakeIds = @{}
    $manifestIds = @()
    foreach ($module in $modules) {
        $target = Get-Value $module 'cmake_target'
        if (Is-ScalarText $target) { $cmakeIds[[string](Get-Value $module 'id')] = [string]$target }
        if ([bool](Get-Value $module 'declared_in_project_manifest')) { $manifestIds += [string](Get-Value $module 'id') }
    }
    foreach ($module in $modules) {
        $id = [string](Get-Value $module 'id')
        $target = Get-Value $module 'cmake_target'
        if (-not (Is-ScalarText $target)) { continue }
        if ($text -notmatch "gnc_add_interface_module\(\s*$([regex]::Escape([string]$target))\s*\)") {
            Add-Issue $Issues "CMake target declaration is missing for $id."
            continue
        }
        $actual = if ($actualByTarget.ContainsKey([string]$target)) { @($actualByTarget[[string]$target] | Sort-Object) } else { @() }
        $expected = @(@(Get-Value $module 'direct_dependencies') | Where-Object { $cmakeIds.ContainsKey([string]$_) } | Sort-Object)
        if (($actual -join '|') -ne ($expected -join '|')) {
            Add-Issue $Issues "CMake dependencies differ for $id. Expected [$($expected -join ', ')], actual [$($actual -join ', ')]."
        }
    }
    $manifest = Read-Json 'project-manifest.json'
    $projectIds = @(@(Get-Value $manifest 'modules') | Sort-Object)
    $manifestIds = @($manifestIds | Sort-Object)
    if (($projectIds -join '|') -ne ($manifestIds -join '|')) {
        Add-Issue $Issues 'Module map differs from project-manifest module membership.'
    }
}

function Test-DagDocument($Document, $Roles, $Issues, [bool]$CheckCMake) {
    if ((Get-Value $Document 'schema_version') -ne $schemaVersion) { Add-Issue $Issues 'Dependency schema_version is invalid.' }
    if ((Get-Value $Document 'document_kind') -ne 'module_dependency_map') { Add-Issue $Issues 'Dependency document_kind is invalid.' }
    if ((Get-Value (Get-Value $Document 'registry') 'edge_direction') -ne 'consumer_to_dependency') {
        Add-Issue $Issues 'Dependency edge direction must be consumer_to_dependency.'
    }
    $modules = @(Get-Value $Document 'modules')
    Test-Unique $modules 'id' 'module' $Issues
    $byId = @{}
    foreach ($module in $modules) {
        $id = [string](Get-Value $module 'id')
        if (-not [string]::IsNullOrWhiteSpace($id)) { $byId[$id] = $module }
        Test-Authority $module "module $id" $Roles $Issues
    }
    foreach ($module in $modules) {
        $id = [string](Get-Value $module 'id')
        $seen = @{}
        foreach ($dependency in @(Get-Value $module 'direct_dependencies')) {
            $target = [string]$dependency
            if (-not $byId.ContainsKey($target)) { Add-Issue $Issues "Module $id references unknown dependency $target." }
            if ($target -eq $id) { Add-Issue $Issues "Module $id depends on itself." }
            if ($seen.ContainsKey($target)) { Add-Issue $Issues "Module $id repeats dependency $target." }
            else { $seen[$target] = $true }
        }
    }
    foreach ($module in $modules) {
        $id = [string](Get-Value $module 'id')
        foreach ($dependency in @(Get-Value $module 'direct_dependencies')) {
            $cycle = @(Find-Path ([string]$dependency) $id $byId)
            if ($cycle.Count -gt 0) { Add-Issue $Issues "Dependency cycle: $id -> $($cycle -join ' -> ')" }
        }
    }
    foreach ($forbidden in @(Get-Value $Document 'forbidden_reachability')) {
        $from = Get-Value $forbidden 'from'
        $to = Get-Value $forbidden 'to'
        if (-not (Is-ScalarText (Get-Value $forbidden 'authority_ref'))) {
            Add-Issue $Issues "Forbidden edge $from to $to lacks one authority."
        }
        $path = @(Find-Path ([string]$from) ([string]$to) $byId)
        if ($path.Count -gt 0) { Add-Issue $Issues "Forbidden dependency path: $($path -join ' -> ')" }
    }
    if ($CheckCMake) { Test-CMakeMap $Document $Issues }
}

function Test-LegacyDocument($Document, $Terms, $Dag, $Roles, $Issues) {
    if ((Get-Value $Document 'schema_version') -ne $schemaVersion) { Add-Issue $Issues 'Legacy schema_version is invalid.' }
    if ((Get-Value $Document 'document_kind') -ne 'legacy_to_target_ownership_map') { Add-Issue $Issues 'Legacy document_kind is invalid.' }
    $mappings = @(Get-Value $Document 'mappings')
    Test-Unique $mappings 'legacy_name' 'legacy mapping' $Issues
    $termByName = @{}
    $legacyNames = @()
    foreach ($term in @(Get-Value $Terms 'terms')) {
        $name = [string](Get-Value $term 'canonical_name')
        $termByName[$name] = $term
        if ((Get-Value $term 'status') -eq 'Legacy') { $legacyNames += $name }
    }
    $moduleSet = @{}
    foreach ($module in @(Get-Value $Dag 'modules')) { $moduleSet[[string](Get-Value $module 'id')] = $true }
    $domainSet = @{}
    foreach ($domain in @(Get-Value $Terms 'authority_domains')) { $domainSet[[string](Get-Value $domain 'name')] = $true }
    $mappingByName = @{}
    $responsibilityIds = @{}
    foreach ($mapping in $mappings) {
        $legacyName = [string](Get-Value $mapping 'legacy_name')
        $mappingByName[$legacyName] = $mapping
        $responsibilities = @(Get-Value $mapping 'responsibilities')
        if ($responsibilities.Count -eq 0) { Add-Issue $Issues "Legacy mapping $legacyName has no responsibilities." }
        foreach ($responsibility in $responsibilities) {
            $id = Get-Value $responsibility 'id'
            if (-not (Is-ScalarText $id)) { Add-Issue $Issues "Legacy mapping $legacyName has a responsibility without id."; continue }
            $idKey = ([string]$id).ToLowerInvariant()
            if ($responsibilityIds.ContainsKey($idKey)) { Add-Issue $Issues "Duplicate legacy responsibility: $id" }
            else { $responsibilityIds[$idKey] = $true }
            $domain = Get-Value $responsibility 'authority_domain'
            if (-not (Is-ScalarText $domain) -or -not $domainSet.ContainsKey([string]$domain)) {
                Add-Issue $Issues "Responsibility $id must have one declared AuthorityDomain."
            }
            $owner = Get-Value $responsibility 'owner_role'
            if (-not (Is-ScalarText $owner) -or -not $Roles.ContainsKey([string]$owner)) {
                Add-Issue $Issues "Responsibility $id must have one declared owner role."
            }
            $module = Get-Value $responsibility 'target_module'
            if (-not (Is-ScalarText $module) -or -not $moduleSet.ContainsKey([string]$module)) {
                Add-Issue $Issues "Responsibility $id must have one declared target module."
            }
            if (-not (Is-ScalarText (Get-Value $responsibility 'authority_ref'))) {
                Add-Issue $Issues "Responsibility $id must have one scalar authority_ref."
            }
            $targets = @(Get-Value $responsibility 'target_terms')
            if ($targets.Count -eq 0) { Add-Issue $Issues "Responsibility $id has no target term." }
            foreach ($target in $targets) {
                $targetName = [string]$target
                if (-not $termByName.ContainsKey($targetName)) { Add-Issue $Issues "Responsibility $id references unknown term $targetName." }
                elseif ((Get-Value $termByName[$targetName] 'status') -eq 'Legacy') {
                    Add-Issue $Issues "Responsibility $id points to Legacy term $targetName."
                }
            }
        }
    }
    foreach ($legacyName in $legacyNames) {
        if (-not $mappingByName.ContainsKey($legacyName)) { Add-Issue $Issues "Registered Legacy term has no mapping: $legacyName" }
        elseif ((Get-Value $mappingByName[$legacyName] 'registry_status') -ne 'registered') {
            Add-Issue $Issues "Registered Legacy term has wrong registry status: $legacyName"
        }
    }
}

function Compare-Baseline($Expected, $Actual, [string]$Label, $Issues) {
    $expectedJson = $Expected | ConvertTo-Json -Depth 100 -Compress
    $actualJson = $Actual | ConvertTo-Json -Depth 100 -Compress
    if ($expectedJson -ne $actualJson) { Add-Issue $Issues "$Label differs from its authoritative source snapshot." }
}

function Invoke-SelfTests($ExpectedTerms, $ExpectedDag, $ExpectedLegacy, $Roles, $MainIssues) {
    $results = @()

    $mutated = Copy-Value $ExpectedTerms
    $mutated.terms = @($mutated.terms) + @($mutated.terms[0])
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Test-TermDocument $mutated $Roles $issues
    $passed = @($issues | Where-Object { $_ -match 'Duplicate canonical term identity' }).Count -gt 0
    $results += [pscustomobject][ordered]@{ id = 'ARCH-TERM-001'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-TERM-001 did not reject a duplicate term.' }

    $mutated = Copy-Value $ExpectedTerms
    $mutated.enums[0].authority_ref = @('terminology-registry','other-registry')
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Test-TermDocument $mutated $Roles $issues
    $passed = @($issues | Where-Object { $_ -match 'exactly one scalar authority_ref' }).Count -gt 0
    $results += [pscustomobject][ordered]@{ id = 'ARCH-TERM-002'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-TERM-002 did not reject multiple authorities.' }

    $mutated = Copy-Value $ExpectedTerms
    $mutated.terms[0].definition = 'mutated definition'
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Compare-Baseline $ExpectedTerms $mutated 'Terminology baseline' $issues
    $passed = $issues.Count -gt 0
    $results += [pscustomobject][ordered]@{ id = 'ARCH-TERM-003'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-TERM-003 did not reject source drift.' }

    $mutated = Copy-Value $ExpectedTerms
    $mutated.terms = @($mutated.terms | Where-Object { $_.canonical_name -ne 'UnitId' })
    $unitEnum = @($mutated.enums | Where-Object { $_.name -eq 'CanonicalSIUnitId' })[0]
    $unitEnum.values = @($unitEnum.values[0..9]) + @(('m' + [char]0x00B2))
    $timeEnum = @($mutated.enums | Where-Object { $_.name -eq 'ScientificTimeKind' })[0]
    $timeEnum.values = @($timeEnum.values | Where-Object { $_ -ne 'WallTime' })
    $coefficientKey = @($mutated.keys | Where-Object { $_.name -eq 'QuaternionCoefficientOrder' })[0]
    $coefficientKey.components = @('x','y','z','w')
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Test-TermDocument $mutated $Roles $issues
    $hasMissingTerm = @($issues | Where-Object { $_ -match 'Scientific overlay term UnitId' }).Count -gt 0
    $hasUnitMismatch = @($issues | Where-Object { $_ -match 'CanonicalSIUnitId differs|not ASCII' }).Count -ge 2
    $hasTimeMismatch = @($issues | Where-Object { $_ -match 'ScientificTimeKind differs' }).Count -gt 0
    $hasOrderMismatch = @($issues | Where-Object { $_ -match 'QuaternionCoefficientOrder differs' }).Count -gt 0
    $passed = $hasMissingTerm -and $hasUnitMismatch -and $hasTimeMismatch -and $hasOrderMismatch
    $results += [pscustomobject][ordered]@{ id = 'ARCH-TERM-004'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-TERM-004 did not reject scientific terminology overlay drift.' }

    $mutated = Copy-Value $ExpectedDag
    $foundation = @($mutated.modules | Where-Object { $_.id -eq 'foundation' })[0]
    $foundation.direct_dependencies = @('contracts','missing_module','missing_module')
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Test-DagDocument $mutated $Roles $issues $false
    $hasUnknown = @($issues | Where-Object { $_ -match 'references unknown dependency missing_module' }).Count -gt 0
    $hasDuplicate = @($issues | Where-Object { $_ -match 'repeats dependency missing_module' }).Count -gt 0
    $hasCycle = @($issues | Where-Object { $_ -match 'Dependency cycle' }).Count -gt 0
    $passed = $hasUnknown -and $hasDuplicate -and $hasCycle
    $results += [pscustomobject][ordered]@{ id = 'ARCH-DAG-001'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-DAG-001 did not reject unknown, duplicate, and cyclic dependencies.' }

    $mutated = Copy-Value $ExpectedDag
    $kernel = @($mutated.modules | Where-Object { $_.id -eq 'kernel' })[0]
    $kernel.direct_dependencies = @($kernel.direct_dependencies) + @('compiler')
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Test-DagDocument $mutated $Roles $issues $false
    $passed = @($issues | Where-Object { $_ -match 'Forbidden dependency path' }).Count -gt 0
    $results += [pscustomobject][ordered]@{ id = 'ARCH-DAG-002'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-DAG-002 did not reject Kernel to Compiler reachability.' }

    $mutated = Copy-Value $ExpectedDag
    $compiler = @($mutated.modules | Where-Object { $_.id -eq 'compiler' })[0]
    $compiler.direct_dependencies = @($compiler.direct_dependencies | Where-Object { $_ -ne 'model_sdk' })
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Test-DagDocument $mutated $Roles $issues $true
    $passed = @($issues | Where-Object { $_ -match 'CMake dependencies differ for compiler' }).Count -gt 0
    $results += [pscustomobject][ordered]@{ id = 'ARCH-DAG-003'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-DAG-003 did not reject CMake dependency drift.' }

    $mutated = Copy-Value $ExpectedLegacy
    $mutated.mappings[0].responsibilities[0].owner_role = ''
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Test-LegacyDocument $mutated $ExpectedTerms $ExpectedDag $Roles $issues
    $passed = @($issues | Where-Object { $_ -match 'must have one declared owner role' }).Count -gt 0
    $results += [pscustomobject][ordered]@{ id = 'ARCH-LEG-002'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-LEG-002 did not reject incomplete ownership.' }

    $mutated = Copy-Value $ExpectedLegacy
    $mutated.mappings = @($mutated.mappings | Where-Object { $_.legacy_name -ne 'Simulator' })
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Test-LegacyDocument $mutated $ExpectedTerms $ExpectedDag $Roles $issues
    $passed = @($issues | Where-Object { $_ -match 'Registered Legacy term has no mapping: Simulator' }).Count -gt 0
    $results += [pscustomobject][ordered]@{ id = 'ARCH-LEG-001'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-LEG-001 did not reject missing legacy coverage.' }

    $mutated = Copy-Value $ExpectedLegacy
    $mutated.mappings[0].responsibilities[0].target_terms = @('UnknownTargetTerm')
    $issues = New-Object 'System.Collections.Generic.List[string]'
    Test-LegacyDocument $mutated $ExpectedTerms $ExpectedDag $Roles $issues
    $passed = @($issues | Where-Object { $_ -match 'references unknown term UnknownTargetTerm' }).Count -gt 0
    $results += [pscustomobject][ordered]@{ id = 'ARCH-LEG-003'; status = if ($passed) { 'passed' } else { 'failed' }; detected_issues = $issues.Count }
    if (-not $passed) { Add-Issue $MainIssues 'ARCH-LEG-003 did not reject an unknown target term.' }

    return $results
}

function Write-Report($Terms, $Dag, $Legacy, $SelfTestResults) {
    $adrStatus = Get-AdrStatus 'docs/adr/0010-r0-architecture-baseline-format.md'
    $overlayAdrStatus = Get-AdrStatus 'docs/adr/0011-r0-scientific-terminology-overlay.md'
    $sources = @()
    foreach ($document in @($Terms,$Dag,$Legacy)) {
        foreach ($snapshot in @(Get-Value $document 'source_snapshots')) {
            if (@($sources | Where-Object { $_.path -eq $snapshot.path }).Count -eq 0) { $sources += $snapshot }
        }
    }
    $artifacts = @($termPath,$dagPath,$legacyPath,'tools/verify-r0-arch-001.ps1','docs/adr/0010-r0-architecture-baseline-format.md','docs/adr/0011-r0-scientific-terminology-overlay.md','docs/quality/r0-arch-001/acceptance-and-failure-plan.md','docs/quality/r0-arch-001/verification-summary.md' | ForEach-Object {
        [pscustomobject][ordered]@{ path = $_; sha256 = Get-Sha $_ }
    })
    $mappings = @(Get-Value $Legacy 'mappings')
    $responsibilityCount = 0
    foreach ($mapping in $mappings) { $responsibilityCount += @(Get-Value $mapping 'responsibilities').Count }
    $report = [pscustomobject][ordered]@{
        schema_version = 'gnczmkn.r0-architecture-conformance-report/1'
        task_id = 'R0-ARCH-001'
        generated_at_utc = [DateTime]::UtcNow.ToString('o')
        status = 'passed'
        adr = [pscustomobject][ordered]@{ path = 'docs/adr/0010-r0-architecture-baseline-format.md'; status = $adrStatus }
        overlay_adr = [pscustomobject][ordered]@{ path = 'docs/adr/0011-r0-scientific-terminology-overlay.md'; status = $overlayAdrStatus }
        counts = [pscustomobject][ordered]@{
            canonical_terms = @(Get-Value $Terms 'terms').Count
            retired_aliases = @(Get-Value $Terms 'retired_aliases').Count
            enums = @(Get-Value $Terms 'enums').Count
            keys = @(Get-Value $Terms 'keys').Count
            authority_domains = @(Get-Value $Terms 'authority_domains').Count
            modules = @(Get-Value $Dag 'modules').Count
            registered_legacy_mappings = @($mappings | Where-Object { $_.registry_status -eq 'registered' }).Count
            audit_only_legacy_mappings = @($mappings | Where-Object { $_.registry_status -eq 'audit-only' }).Count
            target_responsibilities = $responsibilityCount
        }
        checks = @(
            'authoritative source snapshot conformance',
            'canonical term and alias uniqueness',
            'single authority and owner cardinality',
            'complete enum and key composition',
            'R0 scientific terminology overlay coverage',
            'ASCII canonical SI unit identifiers',
            'scientific time kind and quaternion order conformance',
            'acyclic module graph',
            'root CMake direct dependency conformance',
            'ADR-0003 forbidden reachability',
            'registered legacy coverage',
            'canonical target term resolution'
        )
        negative_self_tests = @($SelfTestResults)
        source_snapshots = $sources
        artifact_hashes = $artifacts
        limits = @(
            "ADR-0010 status is $adrStatus.",
            "ADR-0011 status is $overlayAdrStatus.",
            'These documents are R0 governance metadata and are not production runtime schemas.',
            'Detailed semantics remain governed by the architecture sources in detail_authorities.'
        )
    }
    Write-Json $reportJsonPath $report

    $lines = @(
        '# R0-ARCH-001 terminology conformance report',
        '',
        '- Status: passed',
        "- Generated at UTC: $($report.generated_at_utc)",
        "- Governing decisions: ADR-0010 ($adrStatus) and ADR-0011 ($overlayAdrStatus)",
        '- Scope: R0 governance metadata only',
        '',
        '## Baseline counts',
        '',
        '| Item | Count |',
        '| --- | ---: |',
        "| Canonical terms | $($report.counts.canonical_terms) |",
        "| Retired aliases | $($report.counts.retired_aliases) |",
        "| Complete shared enums | $($report.counts.enums) |",
        "| Shared key compositions | $($report.counts.keys) |",
        "| AuthorityDomains | $($report.counts.authority_domains) |",
        "| Modules and logical boundaries | $($report.counts.modules) |",
        "| Registered legacy mappings | $($report.counts.registered_legacy_mappings) |",
        "| Audit-only legacy mappings | $($report.counts.audit_only_legacy_mappings) |",
        "| Target responsibilities | $($report.counts.target_responsibilities) |",
        '',
        '## Conformance result',
        '',
        'The committed baselines exactly match the authoritative source snapshot. Canonical terms and aliases are unique. Every enum, key, AuthorityDomain, module, and legacy target responsibility resolves to one registry authority and one accountable role. The dependency graph is acyclic, matches the root CMake direct links, and satisfies the forbidden reachability declared by ADR-0003. Every registered Legacy term has a target ownership mapping, and every target term resolves to a non-Legacy canonical term.',
        '',
        '## Negative evidence',
        '',
        '| Failure ID | Result | Detected issues |',
        '| --- | --- | ---: |'
    )
    foreach ($result in $SelfTestResults) { $lines += "| $($result.id) | $($result.status) | $($result.detected_issues) |" }
    $lines += @('', '## Source snapshots', '', '| Source | SHA-256 |', '| --- | --- |')
    foreach ($snapshot in $sources) { $lines += "| $($snapshot.path) | $($snapshot.sha256) |" }
    $lines += @('', '## Artifact hashes', '', '| Artifact | SHA-256 |', '| --- | --- |')
    foreach ($artifact in $artifacts) { $lines += "| $($artifact.path) | $($artifact.sha256) |" }
    $lines += @(
        '',
        '## Limits and review status',
        '',
        "- ADR-0010 status: $adrStatus; Product Owner approval is recorded in the decision.",
        "- ADR-0011 status: $overlayAdrStatus; Scientific Authority and Product Owner approvals are recorded in the decision.",
        '- These files are R0 governance metadata. Production runtime consumption is outside this task.',
        '- Detailed semantic authority remains with the architecture sources listed by each term.'
    )
    Write-Utf8 (Resolve-Repo $reportMarkdownPath) (($lines -join [Environment]::NewLine) + [Environment]::NewLine)
}

$expectedTerms = New-TermDocument
$expectedDag = New-DagDocument
$expectedLegacy = New-LegacyDocument

if ($UpdateGeneratedArtifacts) {
    Write-Json $termPath $expectedTerms
    Write-Json $dagPath $expectedDag
    Write-Json $legacyPath $expectedLegacy
    Write-Host 'Updated R0-ARCH-001 baseline artifacts.'
}

$actualTerms = Read-Json $termPath
$actualDag = Read-Json $dagPath
$actualLegacy = Read-Json $legacyPath
$roles = Get-RoleSet
$issues = New-Object 'System.Collections.Generic.List[string]'

Test-TermDocument $actualTerms $roles $issues
Test-DagDocument $actualDag $roles $issues $true
Test-LegacyDocument $actualLegacy $actualTerms $actualDag $roles $issues
Compare-Baseline $expectedTerms $actualTerms 'Terminology baseline' $issues
Compare-Baseline $expectedDag $actualDag 'Module dependency map' $issues
Compare-Baseline $expectedLegacy $actualLegacy 'Legacy ownership map' $issues

$selfTestResults = @()
if ($SelfTest -or $WriteReport) {
    $selfTestResults = @(Invoke-SelfTests $expectedTerms $expectedDag $expectedLegacy $roles $issues)
}

if ($issues.Count -gt 0) {
    Write-Host "R0-ARCH-001 verification failed with $($issues.Count) issue(s):"
    foreach ($issue in $issues) { Write-Host " - $issue" }
    exit 1
}

if ($WriteReport) {
    Write-Report $actualTerms $actualDag $actualLegacy $selfTestResults
    Write-Host "Wrote $reportJsonPath and $reportMarkdownPath."
}

$registeredLegacyCount = @(@(Get-Value $actualLegacy 'mappings') | Where-Object { $_.registry_status -eq 'registered' }).Count
Write-Host 'R0-ARCH-001 architecture baseline verification passed.'
Write-Host "Canonical terms: $(@(Get-Value $actualTerms 'terms').Count)"
Write-Host "Retired aliases: $(@(Get-Value $actualTerms 'retired_aliases').Count)"
Write-Host "Modules and logical boundaries: $(@(Get-Value $actualDag 'modules').Count)"
Write-Host "Registered legacy mappings: $registeredLegacyCount"
if ($SelfTest -or $WriteReport) { Write-Host "Negative self-tests: $($selfTestResults.Count)" }

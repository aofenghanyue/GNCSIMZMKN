#pragma once

#include "gnc/compiler/canonical_semantic_hash.hpp"
#include "gnc/contracts/execution_plan_image.hpp"
#include "gnc/model_sdk/static_descriptor.hpp"
#include "gnc/model_sdk/static_implementation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace gnc::compiler {

inline constexpr std::string_view kCompleteStaticCompositionSourceVersion =
    "gnc.typed-static-composition-source/3";
inline constexpr std::string_view kCompleteCanonicalSemanticEncodingIdentity =
    "gnc.canonical-mission-ir.semantic-bytes@3";
inline constexpr std::string_view kCompleteExecutionPlanDescriptorIdentity =
    "gnc.execution-plan-descriptor/4";
inline constexpr std::string_view kPlanProofIndexIdentity =
    "gnc.plan-proof-index/1";
inline constexpr std::string_view kExecutionPlanImageFingerprintIdentity =
    "gnc.execution-plan-image.fingerprint@1";
inline constexpr std::string_view kNoWorkspaceLayoutIdentity =
    "gnc.workspace.none@1";

enum class CompleteDiagnosticCode : std::uint8_t {
    InvalidCatalog,
    DuplicateCatalogIdentity,
    InvalidSource,
    MissingSourceReference,
    DuplicateOccurrence,
    UnknownDefinition,
    InvalidConfiguration,
    InvalidAssetBinding,
    UnknownEndpoint,
    PortDirectionMismatch,
    ContractMismatch,
    BindingKindMismatch,
    TemporalMismatch,
    ScopeMismatch,
    MissingProvider,
    MultipleProviders,
    MultipleStateOwners,
    MissingStateWriter,
    MultipleStateWriters,
    UnknownCallsite,
    InvalidRegion,
    DagCycle,
    FuturePhaseDependency,
    MissingInvocationAuthorization,
    MultipleInvocationAuthorizations,
    InvocationKindMismatch,
    MissingInvocationResultFlow,
    AmbiguousInvocationResultFlow,
    InvalidIntegrationScope,
    InvalidTransaction,
    InvalidEvaluatorHistory,
    NonCanonicalIr,
    MissingProofCoverage,
    InvalidProofReference,
    MissingImplementationPackage,
    MultipleImplementationPackages,
    MissingImplementationEntry,
    MultipleImplementationEntries,
    ImplementationMismatch,
    SourceImageConformanceFailure,
};

[[nodiscard]] constexpr std::string_view to_string(
    CompleteDiagnosticCode code) noexcept {
    switch (code) {
    case CompleteDiagnosticCode::InvalidCatalog:
        return "GNC-R2-CAT-INVALID";
    case CompleteDiagnosticCode::DuplicateCatalogIdentity:
        return "GNC-R2-CAT-DUPLICATE";
    case CompleteDiagnosticCode::InvalidSource:
        return "GNC-R2-SRC-INVALID";
    case CompleteDiagnosticCode::MissingSourceReference:
        return "GNC-R2-SRC-MISSING-REFERENCE";
    case CompleteDiagnosticCode::DuplicateOccurrence:
        return "GNC-R2-IR-DUPLICATE-OCCURRENCE";
    case CompleteDiagnosticCode::UnknownDefinition:
        return "GNC-R2-CAT-UNKNOWN-DEFINITION";
    case CompleteDiagnosticCode::InvalidConfiguration:
        return "GNC-R2-IR-INVALID-CONFIGURATION";
    case CompleteDiagnosticCode::InvalidAssetBinding:
        return "GNC-R2-IR-INVALID-ASSET";
    case CompleteDiagnosticCode::UnknownEndpoint:
        return "GNC-R2-BIND-UNKNOWN-ENDPOINT";
    case CompleteDiagnosticCode::PortDirectionMismatch:
        return "GNC-R2-BIND-PORT-DIRECTION";
    case CompleteDiagnosticCode::ContractMismatch:
        return "GNC-R2-BIND-CONTRACT";
    case CompleteDiagnosticCode::BindingKindMismatch:
        return "GNC-R2-BIND-KIND";
    case CompleteDiagnosticCode::TemporalMismatch:
        return "GNC-R2-BIND-TEMPORAL";
    case CompleteDiagnosticCode::ScopeMismatch:
        return "GNC-R2-BIND-SCOPE";
    case CompleteDiagnosticCode::MissingProvider:
        return "GNC-R2-BIND-MISSING-PROVIDER";
    case CompleteDiagnosticCode::MultipleProviders:
        return "GNC-R2-BIND-MULTIPLE-PROVIDERS";
    case CompleteDiagnosticCode::MultipleStateOwners:
        return "GNC-R2-STATE-MULTIPLE-OWNERS";
    case CompleteDiagnosticCode::MissingStateWriter:
        return "GNC-R2-STATE-MISSING-WRITER";
    case CompleteDiagnosticCode::MultipleStateWriters:
        return "GNC-R2-STATE-MULTIPLE-WRITERS";
    case CompleteDiagnosticCode::UnknownCallsite:
        return "GNC-R2-PLAN-UNKNOWN-CALLSITE";
    case CompleteDiagnosticCode::InvalidRegion:
        return "GNC-R2-PLAN-INVALID-REGION";
    case CompleteDiagnosticCode::DagCycle:
        return "GNC-R2-PLAN-DAG-CYCLE";
    case CompleteDiagnosticCode::FuturePhaseDependency:
        return "GNC-R2-PLAN-FUTURE-PHASE";
    case CompleteDiagnosticCode::MissingInvocationAuthorization:
        return "GNC-R2-AUTH-MISSING";
    case CompleteDiagnosticCode::MultipleInvocationAuthorizations:
        return "GNC-R2-AUTH-MULTIPLE";
    case CompleteDiagnosticCode::InvocationKindMismatch:
        return "GNC-R2-AUTH-KIND";
    case CompleteDiagnosticCode::MissingInvocationResultFlow:
        return "GNC-R2-AUTH-MISSING-RESULT-FLOW";
    case CompleteDiagnosticCode::AmbiguousInvocationResultFlow:
        return "GNC-R2-AUTH-AMBIGUOUS-RESULT-FLOW";
    case CompleteDiagnosticCode::InvalidIntegrationScope:
        return "GNC-R2-PLAN-INTEGRATION-SCOPE";
    case CompleteDiagnosticCode::InvalidTransaction:
        return "GNC-R2-PLAN-TRANSACTION";
    case CompleteDiagnosticCode::InvalidEvaluatorHistory:
        return "GNC-R2-PLAN-EVALUATOR-HISTORY";
    case CompleteDiagnosticCode::NonCanonicalIr:
        return "GNC-R2-IR-NONCANONICAL";
    case CompleteDiagnosticCode::MissingProofCoverage:
        return "GNC-R2-PRF-MISSING-COVERAGE";
    case CompleteDiagnosticCode::InvalidProofReference:
        return "GNC-R2-PRF-INVALID-REFERENCE";
    case CompleteDiagnosticCode::MissingImplementationPackage:
        return "GNC-R2-LINK-MISSING-PACKAGE";
    case CompleteDiagnosticCode::MultipleImplementationPackages:
        return "GNC-R2-LINK-MULTIPLE-PACKAGES";
    case CompleteDiagnosticCode::MissingImplementationEntry:
        return "GNC-R2-LINK-MISSING-ENTRY";
    case CompleteDiagnosticCode::MultipleImplementationEntries:
        return "GNC-R2-LINK-MULTIPLE-ENTRIES";
    case CompleteDiagnosticCode::ImplementationMismatch:
        return "GNC-R2-LINK-IMPLEMENTATION-MISMATCH";
    case CompleteDiagnosticCode::SourceImageConformanceFailure:
        return "GNC-R2-LINK-SOURCE-IMAGE-CONFORMANCE";
    }
    return "GNC-R2-UNKNOWN";
}

struct CompleteDiagnostic {
    CompleteDiagnosticCode code = CompleteDiagnosticCode::InvalidSource;
    SourceRef source;
    std::string subject;
    std::string detail;
};

template <typename Value>
struct CompleteOutcome {
    std::optional<Value> value;
    std::vector<CompleteDiagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept {
        return value.has_value() && diagnostics.empty();
    }
};

struct CompleteSourceOccurrence {
    std::string occurrence_id;
    std::string model_id;
    std::string model_version;
    SourceRef source;
    std::string subject_entity_id;
    SourceRef subject_source;
    std::optional<ScopeKey> scope;
    SourceRef scope_source;
    gnc::model_sdk::ModelPlacement placement =
        gnc::model_sdk::ModelPlacement::Unspecified;
    SourceRef placement_source;
    gnc::model_sdk::CanonicalConfigBlock configuration;
    SourceRef configuration_source;
    std::vector<SourceConfigFieldProvenance> configuration_field_sources;
    std::vector<SourceAssetBinding> asset_bindings;
};

struct CompleteSourceBinding {
    std::string binding_id;
    std::string provider_occurrence_id;
    std::string provider_port_id;
    std::string consumer_occurrence_id;
    std::string consumer_port_id;
    SourceRef source;
};

struct CompleteSourceClock {
    std::string clock_id;
    double base_step_seconds = 0.0;
    std::int64_t initial_tick = 0;
    std::int64_t terminal_tick = 0;
    SourceRef source;
};

struct CompleteSourceInitialBinding {
    std::string owner_occurrence_id;
    gnc::model_sdk::CanonicalConfigBlock builder_inputs;
    std::vector<SourceConfigFieldProvenance> field_sources;
    SourceRef source;
};

struct CompleteSourceInvocationBinding {
    std::string invocation_id;
    std::string caller_occurrence_id;
    gnc::contracts::ExecutionObligation caller_obligation =
        gnc::contracts::ExecutionObligation::BoundaryEvaluation;
    std::string requirement_id;
    std::string provider_occurrence_id;
    SourceRef source;
};

struct CompleteSourceIntegrationScope {
    std::string integration_scope_id;
    ScopeKey scope;
    std::string owner_occurrence_id;
    std::string form_occurrence_id;
    std::vector<std::string> closure_invocation_ids;
    SourceRef source;
};

struct CompleteSourceTransaction {
    std::string transaction_id;
    ScopeKey scope;
    std::vector<std::string> owner_occurrence_ids;
    SourceRef source;
};

struct CompleteSourceEvaluatorHistory {
    std::string history_id;
    std::string evaluator_occurrence_id;
    std::uint32_t committed_history_depth = 0U;
    std::vector<std::string> owner_occurrence_ids;
    SourceRef source;
};

// R2 programmatic source. It describes immutable composition and scheduling
// facts only. It contains no prepared model, runtime cell, workspace, store,
// session, candidate value, or executable callback.
struct CompleteStaticCompositionSource {
    std::string source_version;
    std::string mission_id;
    std::string plan_id;
    SourceRef mission_source;
    CompleteSourceClock clock;
    std::vector<SourceEntity> entities;
    std::vector<SourceScope> scopes;
    std::vector<CompleteSourceOccurrence> occurrences;
    std::vector<CompleteSourceInitialBinding> initial_bindings;
    std::vector<CompleteSourceBinding> bindings;
    std::vector<CompleteSourceInvocationBinding> invocation_bindings;
    std::vector<CompleteSourceIntegrationScope> integration_scopes;
    std::vector<CompleteSourceTransaction> transactions;
    std::vector<CompleteSourceEvaluatorHistory> evaluator_histories;
};

struct CompleteCanonicalOccurrence {
    std::string occurrence_id;
    PackageLock package;
    gnc::model_sdk::StaticModelDescriptor descriptor;
    std::string subject_entity_id;
    SourceRef subject_source;
    std::optional<ScopeKey> scope;
    SourceRef scope_source;
    gnc::model_sdk::ModelPlacement placement =
        gnc::model_sdk::ModelPlacement::Unspecified;
    SourceRef placement_source;
    gnc::model_sdk::CanonicalConfigBlock configuration;
    SourceRef configuration_source;
    std::vector<CanonicalConfigFieldProvenance>
        configuration_field_sources;
    std::vector<CanonicalAssetBinding> asset_bindings;
    SourceRef source;
};

// Additive IR. The established CanonicalMissionIr revision 2 and its bytes@2
// encoder are not accepted by, converted into, or modified by this API.
struct CompleteCanonicalMissionIr {
    std::uint32_t revision = 3U;
    std::string mission_id;
    std::string plan_id;
    SourceRef mission_source;
    CompleteSourceClock clock;
    std::vector<CanonicalEntity> entities;
    std::vector<CanonicalScope> scopes;
    std::vector<CompleteCanonicalOccurrence> occurrences;
    std::vector<CompleteSourceInitialBinding> initial_bindings;
    std::vector<CompleteSourceBinding> bindings;
    std::vector<CompleteSourceInvocationBinding> invocation_bindings;
    std::vector<CompleteSourceIntegrationScope> integration_scopes;
    std::vector<CompleteSourceTransaction> transactions;
    std::vector<CompleteSourceEvaluatorHistory> evaluator_histories;
};

struct CompletePortPlan {
    std::string plan_element_id;
    std::string occurrence_id;
    std::string port_id;
    std::string contract_id;
    gnc::model_sdk::StaticPortDirection direction =
        gnc::model_sdk::StaticPortDirection::Input;
    gnc::model_sdk::BindingKind binding_kind =
        gnc::model_sdk::BindingKind::Unspecified;
    gnc::model_sdk::PortCardinality cardinality =
        gnc::model_sdk::PortCardinality::Unspecified;
    gnc::model_sdk::TemporalRelation temporal_relation =
        gnc::model_sdk::TemporalRelation::NotApplicable;
    SourceRef source;
};

enum class CompleteSlotKind : std::uint8_t {
    PortValue,
    CommittedState,
    CandidateState,
    HeldIntervalValue,
};

struct CompleteSlotPlan {
    std::string plan_element_id;
    std::string slot_id;
    CompleteSlotKind kind = CompleteSlotKind::PortValue;
    std::string owner_occurrence_id;
    std::string port_id;
    std::string contract_or_layout_id;
    SourceRef source;
};

struct CompleteOccurrencePlan {
    std::string plan_element_id;
    std::string occurrence_id;
    PackageLock package;
    std::string model_id;
    std::string model_version;
    gnc::model_sdk::ModelExecutionForm execution_form =
        gnc::model_sdk::ModelExecutionForm::Unspecified;
    gnc::model_sdk::ModelPlacement placement =
        gnc::model_sdk::ModelPlacement::Unspecified;
    std::string subject_entity_id;
    SourceRef subject_source;
    std::optional<ScopeKey> scope;
    SourceRef scope_source;
    SourceRef placement_source;
    gnc::model_sdk::CanonicalConfigBlock canonical_configuration;
    SourceRef configuration_source;
    std::vector<CanonicalConfigFieldProvenance>
        configuration_field_sources;
    std::vector<CanonicalAssetBinding> asset_bindings;
    SourceRef source;
};

struct CompleteBindingPlan {
    std::string plan_element_id;
    std::string binding_id;
    std::string provider_occurrence_id;
    std::string provider_port_id;
    std::string provider_slot_id;
    std::string consumer_occurrence_id;
    std::string consumer_port_id;
    std::string contract_id;
    gnc::model_sdk::BindingKind binding_kind =
        gnc::model_sdk::BindingKind::Unspecified;
    gnc::model_sdk::TemporalRelation temporal_relation =
        gnc::model_sdk::TemporalRelation::NotApplicable;
    SourceRef source;
};

struct PreparationInputPlan {
    std::string plan_element_id;
    std::string preparation_input_id;
    std::string occurrence_id;
    PackageLock package;
    std::string model_id;
    std::string model_version;
    std::string preparation_entry_requirement_id;
    gnc::model_sdk::CanonicalConfigBlock canonical_configuration;
    std::vector<CanonicalAssetBinding> asset_bindings;
    SourceRef source;
};

struct QueryPlan {
    std::string plan_element_id;
    std::string query_plan_id;
    std::string occurrence_id;
    std::string preparation_input_ref;
    std::string entry_requirement_id;
    gnc::model_sdk::StaticWorkspaceRequirement workspace_requirement =
        gnc::model_sdk::StaticWorkspaceRequirement::Unspecified;
    std::vector<std::string> authorized_invocation_ids;
    SourceRef source;
};

struct ClosurePlan {
    std::string plan_element_id;
    std::string closure_plan_id;
    std::string occurrence_id;
    std::string preparation_input_ref;
    std::string entry_requirement_id;
    gnc::contracts::ClosureStrategy strategy =
        gnc::contracts::ClosureStrategy::Unspecified;
    gnc::model_sdk::StaticWorkspaceRequirement workspace_requirement =
        gnc::model_sdk::StaticWorkspaceRequirement::Unspecified;
    std::vector<std::string> authorized_invocation_ids;
    SourceRef source;
};

struct StateBlockPlan {
    std::string plan_element_id;
    std::string owner_occurrence_id;
    std::string schema_id;
    std::uint32_t schema_version = 0U;
    std::string layout_id;
    gnc::model_sdk::StaticStateEvolution evolution =
        gnc::model_sdk::StaticStateEvolution::Unspecified;
    std::string committed_slot_id;
    std::string candidate_slot_id;
    SourceRef source;
};

struct InitialStatePlan {
    std::string plan_element_id;
    std::string owner_occurrence_id;
    std::string committed_slot_id;
    std::string builder_entry_requirement_id;
    gnc::model_sdk::CanonicalConfigBlock builder_inputs;
    std::vector<SourceConfigFieldProvenance> field_sources;
    SourceRef source;
};

struct RuntimeCallsitePlan {
    std::string plan_element_id;
    std::string callsite_id;
    std::string occurrence_id;
    gnc::contracts::ExecutionObligation obligation =
        gnc::contracts::ExecutionObligation::BoundaryEvaluation;
    gnc::model_sdk::CoarsePhase phase =
        gnc::model_sdk::CoarsePhase::Unspecified;
    std::string entry_requirement_id;
    std::string request_contract_id;
    std::string result_contract_id;
    std::vector<std::string> input_slot_ids;
    std::vector<std::string> output_slot_ids;
    std::vector<std::string> invocation_binding_ids;
    std::string region_id;
    SourceRef source;
};

struct ProjectionPlan {
    std::string plan_element_id;
    std::string owner_occurrence_id;
    std::string callsite_id;
    std::string committed_slot_id;
    std::vector<std::string> output_slot_ids;
    SourceRef source;
};

struct RuntimeComponentPlan {
    std::string plan_element_id;
    std::string occurrence_id;
    std::string definition_builder_entry_requirement_id;
    std::string recipe_id;
    gnc::model_sdk::RuntimeCellProfile profile =
        gnc::model_sdk::RuntimeCellProfile::Unspecified;
    gnc::model_sdk::StaticRuntimeScheduleDescriptor schedule;
    std::vector<gnc::model_sdk::RuntimeLifecycleCapability>
        lifecycle_capabilities;
    std::vector<std::string> callsite_ids;
    SourceRef source;
};

struct InvocationBindingPlan {
    std::string plan_element_id;
    std::string invocation_id;
    std::string caller_callsite_id;
    std::string requirement_id;
    gnc::model_sdk::StaticInvocationKind kind =
        gnc::model_sdk::StaticInvocationKind::Unspecified;
    std::string contract_id;
    std::string provider_occurrence_id;
    std::string provider_plan_id;
    std::string result_binding_id;
    std::string provider_result_slot_id;
    std::string consumer_port_id;
    SourceRef source;
};

struct RegionPlan {
    std::string plan_element_id;
    std::string region_id;
    std::uint32_t ordinal = 0U;
    gnc::model_sdk::CoarsePhase phase =
        gnc::model_sdk::CoarsePhase::Unspecified;
    std::vector<std::string> callsite_ids;
    SourceRef source;
};

struct BoundaryDagEdgePlan {
    std::string plan_element_id;
    std::string edge_id;
    std::string predecessor_node_id;
    std::string successor_node_id;
    SourceRef source;
};

enum class BoundaryDagNodeKind : std::uint8_t {
    RuntimeCallsite,
    IntegrationScope,
};

struct BoundaryDagNodePlan {
    std::string plan_element_id;
    std::string node_id;
    BoundaryDagNodeKind kind = BoundaryDagNodeKind::RuntimeCallsite;
    std::string target_id;
    gnc::model_sdk::CoarsePhase phase =
        gnc::model_sdk::CoarsePhase::Unspecified;
    SourceRef source;
};

struct IntegrationScopePlan {
    std::string plan_element_id;
    std::string integration_scope_id;
    ScopeKey scope;
    std::string owner_occurrence_id;
    std::string committed_state_slot_id;
    std::string candidate_state_slot_id;
    std::string projection_callsite_id;
    std::string form_callsite_id;
    std::string derivative_callsite_id;
    std::string held_form_slot_id;
    std::string held_form_contract_id;
    std::string derivative_request_contract_id;
    std::string derivative_result_contract_id;
    std::vector<std::string> closure_invocation_ids;
    SourceRef source;
};

enum class CandidateProducerKind : std::uint8_t {
    IntegrationScope,
    RuntimeCallsite,
};

struct CandidateProducerPlan {
    CandidateProducerKind kind = CandidateProducerKind::RuntimeCallsite;
    std::string producer_id;
};

struct TransactionPlan {
    std::string plan_element_id;
    std::string transaction_id;
    ScopeKey scope;
    std::vector<std::string> owner_occurrence_ids;
    std::vector<std::string> candidate_state_slot_ids;
    std::vector<CandidateProducerPlan> candidate_producers;
    SourceRef source;
};

struct ClockPlan {
    std::string plan_element_id;
    std::string clock_id;
    double base_step_seconds = 0.0;
    std::int64_t initial_tick = 0;
    std::int64_t terminal_tick = 0;
    SourceRef source;
};

struct EvaluatorCommittedHistoryMemberPlan {
    std::string member_id;
    std::string owner_occurrence_id;
    std::string state_schema_id;
    std::string state_layout_id;
    std::string committed_state_slot_id;
};

struct EvaluatorCommittedHistoryPlan {
    std::string plan_element_id;
    std::string history_id;
    std::string evaluator_callsite_id;
    std::string request_contract_id;
    std::uint32_t committed_history_depth = 0U;
    std::vector<EvaluatorCommittedHistoryMemberPlan> ordered_members;
    SourceRef source;
};

struct EntryLinkRequirement {
    std::string plan_element_id;
    std::string requirement_id;
    PackageLock package;
    std::string entry_id;
    std::string entry_version;
    gnc::model_sdk::StaticEntryKind kind =
        gnc::model_sdk::StaticEntryKind::Prepare;
    std::string signature_id;
    std::string call_shape_id;
    std::string state_layout_id;
    std::string workspace_layout_id;
    SourceRef source;
};

struct CompleteExecutionPlanDescriptor {
    std::uint32_t revision = 4U;
    std::string descriptor_identity =
        std::string(kCompleteExecutionPlanDescriptorIdentity);
    std::string plan_id;
    std::string mission_id;
    std::string source_semantic_hash;
    std::string descriptor_semantic_hash;
    ClockPlan clock;
    std::vector<PackageLock> dependency_lock;
    std::vector<CompleteOccurrencePlan> occurrences;
    std::vector<CompletePortPlan> ports;
    std::vector<CompleteSlotPlan> slots;
    std::vector<CompleteBindingPlan> bindings;
    std::vector<PreparationInputPlan> preparation_inputs;
    std::vector<QueryPlan> queries;
    std::vector<ClosurePlan> closures;
    std::vector<StateBlockPlan> state_blocks;
    std::vector<InitialStatePlan> initial_states;
    std::vector<ProjectionPlan> projections;
    std::vector<RuntimeComponentPlan> runtime_components;
    std::vector<RuntimeCallsitePlan> runtime_callsites;
    std::vector<InvocationBindingPlan> invocation_bindings;
    std::vector<RegionPlan> regions;
    std::vector<BoundaryDagNodePlan> boundary_dag_nodes;
    std::vector<BoundaryDagEdgePlan> boundary_dag;
    std::vector<IntegrationScopePlan> integration_scopes;
    std::vector<TransactionPlan> transactions;
    std::vector<EvaluatorCommittedHistoryPlan> evaluator_histories;
    std::vector<EntryLinkRequirement> entry_requirements;
};

enum class PlanProofKind : std::uint8_t {
    ExactCatalogResolution,
    CanonicalConfiguration,
    ExactAssetSelection,
    PortAndSlotDerivation,
    RequiredProviderCardinality,
    ScopeCompatibility,
    TemporalCompatibility,
    UniqueStateOwner,
    UniqueCandidateWriter,
    InvocationAuthorization,
    RegionMembership,
    DagAcyclic,
    PhaseOrder,
    IntegrationScopeComplete,
    TransactionComplete,
    EvaluatorCommittedOnly,
    ExactEntryRequirement,
};

struct PlanProofRecord {
    std::string proof_id;
    PlanProofKind kind = PlanProofKind::ExactCatalogResolution;
    std::string subject;
    std::vector<std::string> premises;
    std::vector<std::string> covered_plan_elements;
    std::vector<SourceRef> source_refs;
};

struct PlanElementProofCoverage {
    std::string plan_element_id;
    std::vector<std::string> proof_ids;
};

struct PlanProofIndex {
    std::uint32_t revision = 1U;
    std::string identity = std::string(kPlanProofIndexIdentity);
    std::string plan_id;
    std::string proof_index_hash;
    std::vector<PlanProofRecord> records;
    std::vector<PlanElementProofCoverage> coverage;
};

struct CompleteStaticCompilation {
    CompleteCanonicalMissionIr ir;
    CompleteExecutionPlanDescriptor plan;
    PlanProofIndex proofs;
};

namespace complete_plan_detail {

[[nodiscard]] inline bool valid_source_ref(const SourceRef& source) noexcept {
    return !source.document_uri.empty() && !source.node_path.empty();
}

[[nodiscard]] inline std::string occurrence_element(std::string_view id) {
    return "occurrence/" + std::string(id);
}

[[nodiscard]] inline std::string port_element(std::string_view occurrence,
                                              std::string_view port) {
    return "port/" + std::string(occurrence) + "/" + std::string(port);
}

[[nodiscard]] inline std::string port_slot_id(std::string_view occurrence,
                                             std::string_view port) {
    return "slot/port/" + std::string(occurrence) + "/" +
           std::string(port);
}

[[nodiscard]] inline std::string committed_slot_id(
    std::string_view occurrence) {
    return "slot/state/" + std::string(occurrence) + "/committed";
}

[[nodiscard]] inline std::string candidate_slot_id(
    std::string_view occurrence) {
    return "slot/state/" + std::string(occurrence) + "/candidate";
}

[[nodiscard]] inline std::string callsite_id(
    std::string_view occurrence,
    gnc::contracts::ExecutionObligation obligation,
    std::string_view entry_id) {
    return std::string(occurrence) + "/" +
           std::string(gnc::contracts::to_string(obligation)) + "/" +
           std::string(entry_id);
}

[[nodiscard]] inline std::string entry_requirement_id(
    std::string_view occurrence, std::string_view role,
    std::string_view entry_id) {
    return "entry/" + std::string(occurrence) + "/" + std::string(role) +
           "/" + std::string(entry_id);
}

[[nodiscard]] inline std::string invocation_kind_token(
    gnc::model_sdk::StaticInvocationKind kind) {
    return std::string(gnc::model_sdk::to_string(kind));
}

[[nodiscard]] inline gnc::model_sdk::StaticEntryKind entry_kind(
    gnc::contracts::ExecutionObligation obligation) noexcept {
    switch (obligation) {
    case gnc::contracts::ExecutionObligation::PublishProjection:
        return gnc::model_sdk::StaticEntryKind::PublishProjection;
    case gnc::contracts::ExecutionObligation::BoundaryEvaluation:
        return gnc::model_sdk::StaticEntryKind::BoundaryEvaluation;
    case gnc::contracts::ExecutionObligation::IntervalEvolution:
        return gnc::model_sdk::StaticEntryKind::IntervalEvolution;
    case gnc::contracts::ExecutionObligation::DerivativeEvaluation:
        return gnc::model_sdk::StaticEntryKind::DerivativeEvaluation;
    }
    return gnc::model_sdk::StaticEntryKind::BoundaryEvaluation;
}

[[nodiscard]] inline gnc::contracts::PlanImageEntryKind image_entry_kind(
    gnc::model_sdk::StaticEntryKind kind) noexcept {
    using I = gnc::contracts::PlanImageEntryKind;
    using S = gnc::model_sdk::StaticEntryKind;
    switch (kind) {
    case S::DefinitionBuilder:
        return I::DefinitionBuilder;
    case S::Prepare:
        return I::Prepare;
    case S::PureQuery:
        return I::PureQuery;
    case S::Closure:
        return I::Closure;
    case S::InitialState:
        return I::InitialState;
    case S::PublishProjection:
        return I::PublishProjection;
    case S::BoundaryEvaluation:
        return I::BoundaryEvaluation;
    case S::IntervalEvolution:
        return I::IntervalEvolution;
    case S::DerivativeEvaluation:
        return I::DerivativeEvaluation;
    }
    return I::Prepare;
}

[[nodiscard]] inline const gnc::model_sdk::StaticPortDescriptor* find_port(
    const gnc::model_sdk::StaticModelDescriptor& model,
    std::string_view port_id) {
    const auto found = std::find_if(
        model.ports.begin(), model.ports.end(),
        [&](const gnc::model_sdk::StaticPortDescriptor& port) {
            return port.port_id == port_id;
        });
    return found == model.ports.end() ? nullptr : &*found;
}

struct CatalogModelView {
    PackageLock package;
    const gnc::model_sdk::StaticModelDescriptor* model = nullptr;
};

struct CompleteCatalog {
    std::map<std::string, CatalogModelView> models;
    std::vector<PackageLock> packages;
};

[[nodiscard]] inline std::string exact_key(std::string_view id,
                                           std::string_view version) {
    return std::string(id) + "\x1f" + std::string(version);
}

inline void diagnostic(std::vector<CompleteDiagnostic>& diagnostics,
                       CompleteDiagnosticCode code, SourceRef source,
                       std::string subject, std::string detail) {
    diagnostics.push_back(
        {code, std::move(source), std::move(subject), std::move(detail)});
}

[[nodiscard]] inline SourceRef catalog_source(std::string_view package,
                                              std::string_view subject) {
    return {"catalog://" + std::string(package), "/" +
                                                   std::string(subject)};
}

[[nodiscard]] inline CompleteOutcome<CompleteCatalog> build_catalog(
    const std::vector<gnc::model_sdk::StaticPackageDescriptor>& packages) {
    CompleteOutcome<CompleteCatalog> outcome;

    // Catalog is the single descriptor validator for every compiler path.
    // The complete-plan catalog below is only a non-owning lowering index over
    // the caller's descriptors; it must never accept a graph rejected by the
    // established Catalog boundary.
    const auto validated = Catalog::build(packages);
    if (!validated.succeeded()) {
        for (const auto& item : validated.diagnostics) {
            diagnostic(
                outcome.diagnostics,
                item.code == DiagnosticCode::DuplicateCatalogIdentity
                    ? CompleteDiagnosticCode::DuplicateCatalogIdentity
                    : CompleteDiagnosticCode::InvalidCatalog,
                item.source, item.subject, item.detail);
        }
        return outcome;
    }

    CompleteCatalog catalog;
    std::set<std::string> package_keys;
    for (const auto& package : packages) {
        const auto source = catalog_source(package.package_id, "package");
        if (package.package_id.empty() || package.package_version.empty()) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::InvalidCatalog, source,
                       package.package_id,
                       "package identity and version are required");
            continue;
        }
        const auto package_key =
            exact_key(package.package_id, package.package_version);
        if (!package_keys.insert(package_key).second) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::DuplicateCatalogIdentity,
                       source, package.package_id,
                       "duplicate exact package descriptor");
            continue;
        }
        const PackageLock lock{package.package_id, package.package_version};
        catalog.packages.push_back(lock);
        for (const auto& model : package.models) {
            const auto key = exact_key(model.definition.model_id,
                                       model.definition.model_version);
            if (!catalog.models.emplace(key, CatalogModelView{lock, &model})
                     .second) {
                diagnostic(outcome.diagnostics,
                           CompleteDiagnosticCode::DuplicateCatalogIdentity,
                           catalog_source(package.package_id,
                                          model.definition.model_id),
                           model.definition.model_id,
                           "duplicate exact model definition");
            }
        }
    }
    std::sort(catalog.packages.begin(), catalog.packages.end(),
              [](const PackageLock& lhs, const PackageLock& rhs) {
                  return std::tie(lhs.package_id, lhs.package_version) <
                         std::tie(rhs.package_id, rhs.package_version);
              });
    if (outcome.diagnostics.empty()) {
        outcome.value = std::move(catalog);
    }
    return outcome;
}

[[nodiscard]] inline bool scope_equal(const std::optional<ScopeKey>& lhs,
                                      const std::optional<ScopeKey>& rhs) {
    return lhs.has_value() == rhs.has_value() &&
           (!lhs.has_value() || *lhs == *rhs);
}

[[nodiscard]] inline bool compatible_scopes(
    const CompleteCanonicalOccurrence& provider,
    const CompleteCanonicalOccurrence& consumer) {
    if (scope_equal(provider.scope, consumer.scope)) {
        return true;
    }
    return !provider.scope.has_value() && consumer.scope.has_value() &&
           provider.descriptor.placement ==
               gnc::model_sdk::ModelPlacement::Environment;
}

[[nodiscard]] inline bool config_value_matches(
    const gnc::model_sdk::CanonicalConfigValue& value,
    gnc::model_sdk::CanonicalConfigValueKind kind) noexcept {
    if (gnc::model_sdk::canonical_config_value_kind(value) != kind) {
        return false;
    }
    if (const auto* text = std::get_if<std::string>(&value)) {
        return !text->empty();
    }
    if (const auto* token =
            std::get_if<gnc::model_sdk::CanonicalEnumValue>(&value)) {
        return !token->token.empty();
    }
    if (const auto* number = std::get_if<double>(&value)) {
        return std::isfinite(*number) &&
               !(*number == 0.0 && std::signbit(*number));
    }
    return true;
}

[[nodiscard]] inline bool canonicalize_configuration(
    const CompleteSourceOccurrence& source,
    const gnc::model_sdk::StaticModelDescriptor& descriptor,
    gnc::model_sdk::CanonicalConfigBlock& result,
    std::vector<CompleteDiagnostic>& diagnostics) {
    result = source.configuration;
    if (result.schema_id != descriptor.configuration.schema_id ||
        result.schema_version != descriptor.configuration.schema_version) {
        diagnostic(diagnostics, CompleteDiagnosticCode::InvalidConfiguration,
                   source.configuration_source, source.occurrence_id,
                   "configuration schema identity/version does not match the definition");
        return false;
    }
    std::sort(result.fields.begin(), result.fields.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.field_id < rhs.field_id;
              });
    auto expected = descriptor.configuration.fields;
    std::sort(expected.begin(), expected.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.field_id < rhs.field_id;
              });
    if (result.fields.size() != expected.size()) {
        diagnostic(diagnostics, CompleteDiagnosticCode::InvalidConfiguration,
                   source.configuration_source, source.occurrence_id,
                   "configuration must contain every schema field exactly once");
        return false;
    }
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        if (result.fields[index].field_id != expected[index].field_id ||
            !config_value_matches(result.fields[index].value,
                                  expected[index].value_kind) ||
            (index > 0U && result.fields[index - 1U].field_id ==
                               result.fields[index].field_id)) {
            diagnostic(diagnostics,
                       CompleteDiagnosticCode::InvalidConfiguration,
                       source.configuration_source, source.occurrence_id,
                       "configuration field identity, type, uniqueness, or value is invalid");
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool canonicalize_assets(
    const CompleteSourceOccurrence& source,
    const gnc::model_sdk::StaticModelDescriptor& descriptor,
    std::vector<CanonicalAssetBinding>& result,
    std::vector<CompleteDiagnostic>& diagnostics) {
    auto assets = source.asset_bindings;
    std::sort(assets.begin(), assets.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.role, lhs.asset_id) <
               std::tie(rhs.role, rhs.asset_id);
    });
    for (const auto& slot : descriptor.asset_slots) {
        std::vector<const SourceAssetBinding*> matches;
        for (const auto& asset : assets) {
            if (asset.role == slot.role) {
                matches.push_back(&asset);
            }
        }
        const bool valid_count =
            slot.cardinality == gnc::model_sdk::PortCardinality::ExactlyOne
                ? matches.size() == 1U
                : !matches.empty();
        if (!valid_count) {
            diagnostic(diagnostics,
                       CompleteDiagnosticCode::InvalidAssetBinding,
                       source.source, source.occurrence_id + "." + slot.role,
                       "asset role cardinality is not satisfied");
            continue;
        }
        for (const auto* asset : matches) {
            if (!valid_source_ref(asset->source) || asset->asset_id.empty() ||
                asset->asset_schema_id != slot.asset_schema_id) {
                diagnostic(diagnostics,
                           CompleteDiagnosticCode::InvalidAssetBinding,
                           asset->source,
                           source.occurrence_id + "." + slot.role,
                           "asset identity/schema/source reference is invalid");
                continue;
            }
            result.push_back({asset->role, asset->asset_schema_id,
                              asset->asset_id, slot.cardinality,
                              asset->source});
        }
    }
    for (const auto& asset : assets) {
        const auto found = std::find_if(
            descriptor.asset_slots.begin(), descriptor.asset_slots.end(),
            [&](const auto& slot) { return slot.role == asset.role; });
        if (found == descriptor.asset_slots.end()) {
            diagnostic(diagnostics,
                       CompleteDiagnosticCode::InvalidAssetBinding,
                       asset.source,
                       source.occurrence_id + "." + asset.role,
                       "asset role is not declared by the model definition");
        }
    }
    return diagnostics.empty();
}

inline void encode_config(semantic_hash_detail::Encoder& encoder,
                          const gnc::model_sdk::CanonicalConfigBlock& config) {
    encoder.string(config.schema_id);
    encoder.uint32(config.schema_version);
    encoder.collection(config.fields.size());
    for (const auto& field : config.fields) {
        encoder.string(field.field_id);
        semantic_hash_detail::encode_config_value(encoder, field.value);
    }
}

[[nodiscard]] inline std::string hash_bytes(
    const semantic_hash_detail::Encoder& encoder) {
    return semantic_hash_detail::sha256_hex(encoder.bytes());
}

[[nodiscard]] inline std::string source_semantic_hash(
    const CompleteCanonicalMissionIr& ir) {
    semantic_hash_detail::Encoder encoder;
    encoder.string(kCompleteCanonicalSemanticEncodingIdentity);
    encoder.uint32(ir.revision);
    encoder.string(ir.mission_id);
    encoder.string(ir.clock.clock_id);
    encoder.float64(ir.clock.base_step_seconds);
    encoder.integer(ir.clock.initial_tick);
    encoder.integer(ir.clock.terminal_tick);
    encoder.collection(ir.entities.size());
    for (const auto& entity : ir.entities) {
        encoder.string(entity.entity_id);
        encoder.uint32(static_cast<std::uint32_t>(entity.lifecycle));
    }
    encoder.collection(ir.initial_bindings.size());
    for (const auto& binding : ir.initial_bindings) {
        encoder.string(binding.owner_occurrence_id);
        encode_config(encoder, binding.builder_inputs);
    }
    encoder.collection(ir.scopes.size());
    for (const auto& scope : ir.scopes) {
        encoder.uint32(static_cast<std::uint32_t>(scope.key.kind));
        encoder.string(scope.key.subject_entity_id);
    }
    encoder.collection(ir.occurrences.size());
    for (const auto& occurrence : ir.occurrences) {
        const auto& model = occurrence.descriptor;
        encoder.string(occurrence.occurrence_id);
        encoder.string(model.definition.model_id);
        encoder.string(model.definition.model_version);
        encoder.uint32(
            static_cast<std::uint32_t>(model.definition.execution_form));
        encoder.uint32(static_cast<std::uint32_t>(model.placement));
        encoder.string(occurrence.subject_entity_id);
        encoder.optional(occurrence.scope.has_value());
        if (occurrence.scope.has_value()) {
            encoder.uint32(static_cast<std::uint32_t>(occurrence.scope->kind));
            encoder.string(occurrence.scope->subject_entity_id);
        }
        encode_config(encoder, occurrence.configuration);
        encoder.collection(occurrence.asset_bindings.size());
        for (const auto& asset : occurrence.asset_bindings) {
            encoder.string(asset.role);
            encoder.string(asset.asset_schema_id);
            encoder.string(asset.asset_id);
        }
        auto ports = model.ports;
        std::sort(ports.begin(), ports.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.port_id < rhs.port_id;
        });
        encoder.collection(ports.size());
        for (const auto& port : ports) {
            encoder.string(port.port_id);
            encoder.string(port.contract_id);
            encoder.uint32(static_cast<std::uint32_t>(port.direction));
            encoder.uint32(static_cast<std::uint32_t>(port.binding_kind));
            encoder.uint32(static_cast<std::uint32_t>(port.cardinality));
            encoder.uint32(static_cast<std::uint32_t>(
                port.temporal_relation));
        }
        encoder.optional(model.pure_query.has_value());
        if (model.pure_query.has_value()) {
            encoder.string(model.pure_query->request_contract_id);
        }
        encoder.optional(model.closure.has_value());
        if (model.closure.has_value()) {
            encoder.string(model.closure->request_contract_id);
            encoder.uint32(static_cast<std::uint32_t>(
                model.closure->strategy));
        }
        encoder.optional(model.runtime_component.has_value());
        if (model.runtime_component.has_value()) {
            const auto& runtime = *model.runtime_component;
            encoder.uint32(static_cast<std::uint32_t>(runtime.profile));
            encoder.uint32(static_cast<std::uint32_t>(
                runtime.schedule.trigger));
            encoder.uint32(runtime.schedule.step_interval);
            encoder.uint32(runtime.schedule.offset);
            encoder.uint32(static_cast<std::uint32_t>(
                runtime.schedule.output_hold));
            encoder.uint32(runtime.schedule.max_input_age_steps);
            encoder.collection(runtime.lifecycle_capabilities.size());
            for (const auto capability :
                 runtime.lifecycle_capabilities) {
                encoder.uint32(static_cast<std::uint32_t>(capability));
            }
            encoder.optional(runtime.state_owner.has_value());
            if (runtime.state_owner.has_value()) {
                const auto& owner = *runtime.state_owner;
                encoder.string(owner.schema.schema_id);
                encoder.uint32(owner.schema.schema_version);
                encoder.uint32(static_cast<std::uint32_t>(owner.evolution));
                encoder.string(owner.initial_state_input_schema.schema_id);
                encoder.uint32(
                    owner.initial_state_input_schema.schema_version);
                encoder.collection(
                    owner.initial_state_input_schema.fields.size());
                for (const auto& field :
                     owner.initial_state_input_schema.fields) {
                    encoder.string(field.field_id);
                    encoder.uint32(static_cast<std::uint32_t>(
                        field.value_kind));
                }
                encoder.collection(owner.schema.fields.size());
                for (const auto& field : owner.schema.fields) {
                    encoder.string(field.field_id);
                    encoder.string(field.value_type);
                    encoder.string(field.unit);
                    encoder.string(field.frame_role);
                }
            }
            auto entries = runtime.obligation_entries;
            std::sort(entries.begin(), entries.end(),
                      [](const auto& lhs, const auto& rhs) {
                          return std::tie(lhs.phase, lhs.obligation) <
                                 std::tie(rhs.phase, rhs.obligation);
                      });
            encoder.collection(entries.size());
            for (auto& entry : entries) {
                std::sort(entry.input_port_ids.begin(),
                          entry.input_port_ids.end());
                std::sort(entry.output_port_ids.begin(),
                          entry.output_port_ids.end());
                std::sort(entry.invocation_requirements.begin(),
                          entry.invocation_requirements.end(),
                          [](const auto& lhs, const auto& rhs) {
                              return lhs.requirement_id < rhs.requirement_id;
                          });
                encoder.uint32(static_cast<std::uint32_t>(entry.obligation));
                encoder.uint32(static_cast<std::uint32_t>(entry.phase));
                encoder.string(entry.request_contract_id);
                encoder.string(entry.result_contract_id);
                encoder.uint32(static_cast<std::uint32_t>(entry.state_read));
                encoder.uint32(static_cast<std::uint32_t>(entry.state_write));
                encoder.collection(entry.input_port_ids.size());
                for (const auto& id : entry.input_port_ids) {
                    encoder.string(id);
                }
                encoder.collection(entry.output_port_ids.size());
                for (const auto& id : entry.output_port_ids) {
                    encoder.string(id);
                }
                encoder.collection(entry.invocation_requirements.size());
                for (const auto& requirement :
                     entry.invocation_requirements) {
                    encoder.string(requirement.requirement_id);
                    encoder.uint32(static_cast<std::uint32_t>(
                        requirement.kind));
                    encoder.string(requirement.contract_id);
                    encoder.uint32(static_cast<std::uint32_t>(
                        requirement.cardinality));
                }
            }
        }
    }
    const auto encode_binding = [&](const CompleteSourceBinding& binding) {
        encoder.string(binding.binding_id);
        encoder.string(binding.provider_occurrence_id);
        encoder.string(binding.provider_port_id);
        encoder.string(binding.consumer_occurrence_id);
        encoder.string(binding.consumer_port_id);
    };
    encoder.collection(ir.bindings.size());
    for (const auto& binding : ir.bindings) {
        encode_binding(binding);
    }
    encoder.collection(ir.invocation_bindings.size());
    for (const auto& binding : ir.invocation_bindings) {
        encoder.string(binding.invocation_id);
        encoder.string(binding.caller_occurrence_id);
        encoder.uint32(static_cast<std::uint32_t>(
            binding.caller_obligation));
        encoder.string(binding.requirement_id);
        encoder.string(binding.provider_occurrence_id);
    }
    encoder.collection(ir.integration_scopes.size());
    for (const auto& scope : ir.integration_scopes) {
        encoder.string(scope.integration_scope_id);
        encoder.uint32(static_cast<std::uint32_t>(scope.scope.kind));
        encoder.string(scope.scope.subject_entity_id);
        encoder.string(scope.owner_occurrence_id);
        encoder.string(scope.form_occurrence_id);
        encoder.collection(scope.closure_invocation_ids.size());
        for (const auto& id : scope.closure_invocation_ids) {
            encoder.string(id);
        }
    }
    encoder.collection(ir.transactions.size());
    for (const auto& transaction : ir.transactions) {
        encoder.string(transaction.transaction_id);
        encoder.uint32(static_cast<std::uint32_t>(transaction.scope.kind));
        encoder.string(transaction.scope.subject_entity_id);
        encoder.collection(transaction.owner_occurrence_ids.size());
        for (const auto& id : transaction.owner_occurrence_ids) {
            encoder.string(id);
        }
    }
    encoder.collection(ir.evaluator_histories.size());
    for (const auto& history : ir.evaluator_histories) {
        encoder.string(history.history_id);
        encoder.string(history.evaluator_occurrence_id);
        encoder.uint32(history.committed_history_depth);
        encoder.collection(history.owner_occurrence_ids.size());
        for (const auto& id : history.owner_occurrence_ids) {
            encoder.string(id);
        }
    }
    return hash_bytes(encoder);
}

} // namespace complete_plan_detail

[[nodiscard]] inline CompleteOutcome<CompleteCanonicalMissionIr>
lower_complete_static_source(
    const CompleteStaticCompositionSource& source,
    const std::vector<gnc::model_sdk::StaticPackageDescriptor>& packages) {
    using namespace complete_plan_detail;
    CompleteOutcome<CompleteCanonicalMissionIr> outcome;
    const auto catalog_outcome = build_catalog(packages);
    if (!catalog_outcome.succeeded()) {
        outcome.diagnostics = catalog_outcome.diagnostics;
        return outcome;
    }
    const auto& catalog = *catalog_outcome.value;
    if (source.source_version != kCompleteStaticCompositionSourceVersion ||
        source.mission_id.empty() || source.plan_id.empty() ||
        !valid_source_ref(source.mission_source)) {
        diagnostic(outcome.diagnostics, CompleteDiagnosticCode::InvalidSource,
                   source.mission_source, source.mission_id,
                   "source revision 3, mission/plan identities, and mission provenance are required");
        return outcome;
    }

    CompleteCanonicalMissionIr ir;
    ir.mission_id = source.mission_id;
    ir.plan_id = source.plan_id;
    ir.mission_source = source.mission_source;
    ir.clock = source.clock;
    if (ir.clock.clock_id.empty() ||
        !std::isfinite(ir.clock.base_step_seconds) ||
        ir.clock.base_step_seconds <= 0.0 ||
        ir.clock.terminal_tick < ir.clock.initial_tick ||
        !valid_source_ref(ir.clock.source)) {
        diagnostic(outcome.diagnostics, CompleteDiagnosticCode::InvalidSource,
                   ir.clock.source, ir.clock.clock_id,
                   "clock requires an identity, finite positive base step, ordered ticks, and provenance");
    }

    auto entities = source.entities;
    std::sort(entities.begin(), entities.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.entity_id < rhs.entity_id;
              });
    std::set<std::string> entity_ids;
    for (const auto& entity : entities) {
        if (entity.entity_id.empty() ||
            entity.lifecycle != EntityLifecycle::ActiveAtInitialize ||
            !valid_source_ref(entity.identity_source) ||
            !valid_source_ref(entity.lifecycle_source) ||
            !entity_ids.insert(entity.entity_id).second) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::InvalidSource,
                       entity.identity_source, entity.entity_id,
                       "entity identity/lifecycle/provenance is invalid or duplicated");
            continue;
        }
        ir.entities.push_back({entity.entity_id, entity.lifecycle,
                               entity.identity_source,
                               entity.lifecycle_source});
    }

    auto scopes = source.scopes;
    std::sort(scopes.begin(), scopes.end(), [](const auto& lhs,
                                               const auto& rhs) {
        return lhs.key < rhs.key;
    });
    std::set<ScopeKey> scope_keys;
    for (const auto& scope : scopes) {
        if (scope.key.kind != ScopeKind::Vehicle ||
            entity_ids.find(scope.key.subject_entity_id) ==
                entity_ids.end() ||
            !valid_source_ref(scope.source) ||
            !scope_keys.insert(scope.key).second) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::InvalidSource, scope.source,
                       scope.key.subject_entity_id,
                       "scope is invalid, duplicated, or references an unknown entity");
            continue;
        }
        ir.scopes.push_back({scope.key, scope.source});
    }

    auto occurrences = source.occurrences;
    std::sort(occurrences.begin(), occurrences.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.occurrence_id < rhs.occurrence_id;
              });
    std::set<std::string> occurrence_ids;
    std::set<std::string> scoped_state_schema_owners;
    for (const auto& occurrence : occurrences) {
        if (occurrence.occurrence_id.empty() ||
            !occurrence_ids.insert(occurrence.occurrence_id).second) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::DuplicateOccurrence,
                       occurrence.source, occurrence.occurrence_id,
                       "occurrence identity is empty or duplicated");
            continue;
        }
        if (!valid_source_ref(occurrence.source) ||
            !valid_source_ref(occurrence.subject_source) ||
            !valid_source_ref(occurrence.placement_source) ||
            !valid_source_ref(occurrence.configuration_source) ||
            (occurrence.scope.has_value() &&
             !valid_source_ref(occurrence.scope_source))) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::MissingSourceReference,
                       occurrence.source, occurrence.occurrence_id,
                       "occurrence, subject, configuration, and scope provenance are required");
            continue;
        }
        if (entity_ids.find(occurrence.subject_entity_id) ==
                entity_ids.end() ||
            (occurrence.scope.has_value() &&
             scope_keys.find(*occurrence.scope) == scope_keys.end()) ||
            (occurrence.scope.has_value() &&
             occurrence.scope->subject_entity_id !=
                 occurrence.subject_entity_id)) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::ScopeMismatch,
                       occurrence.scope_source, occurrence.occurrence_id,
                       "occurrence subject and declared scope do not resolve to the same entity");
            continue;
        }
        const auto found = catalog.models.find(
            exact_key(occurrence.model_id, occurrence.model_version));
        if (found == catalog.models.end()) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::UnknownDefinition,
                       occurrence.source, occurrence.occurrence_id,
                       "exact model definition is absent from the Catalog");
            continue;
        }
        gnc::model_sdk::CanonicalConfigBlock config;
        std::vector<CanonicalAssetBinding> assets;
        const auto before = outcome.diagnostics.size();
        static_cast<void>(canonicalize_configuration(
            occurrence, *found->second.model, config,
            outcome.diagnostics));
        static_cast<void>(canonicalize_assets(
            occurrence, *found->second.model, assets,
            outcome.diagnostics));
        if (outcome.diagnostics.size() != before) {
            continue;
        }
        const auto& descriptor = *found->second.model;
        if (occurrence.placement != descriptor.placement) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::InvalidSource,
                       occurrence.placement_source,
                       occurrence.occurrence_id,
                       "source placement must exactly match the selected package definition");
            continue;
        }
        auto configuration_field_sources =
            occurrence.configuration_field_sources;
        std::sort(configuration_field_sources.begin(),
                  configuration_field_sources.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return lhs.field_id < rhs.field_id;
                  });
        auto expected_configuration_fields = descriptor.configuration.fields;
        std::sort(expected_configuration_fields.begin(),
                  expected_configuration_fields.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return lhs.field_id < rhs.field_id;
                  });
        bool configuration_provenance_valid =
            configuration_field_sources.size() ==
            expected_configuration_fields.size();
        for (std::size_t index = 0U;
             index < configuration_field_sources.size(); ++index) {
            configuration_provenance_valid =
                configuration_provenance_valid &&
                index < expected_configuration_fields.size() &&
                configuration_field_sources[index].field_id ==
                    expected_configuration_fields[index].field_id &&
                valid_source_ref(
                    configuration_field_sources[index].source) &&
                (index == 0U ||
                 configuration_field_sources[index - 1U].field_id !=
                     configuration_field_sources[index].field_id);
        }
        if (!configuration_provenance_valid) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::MissingSourceReference,
                       occurrence.configuration_source,
                       occurrence.occurrence_id,
                       "every canonical configuration field requires one exact provenance reference");
            continue;
        }
        if (descriptor.runtime_component.has_value() &&
            descriptor.runtime_component->state_owner.has_value()) {
            const auto& schema =
                descriptor.runtime_component->state_owner->schema;
            std::string scope_token = "global";
            if (occurrence.scope.has_value()) {
                scope_token =
                    std::to_string(static_cast<unsigned int>(
                        occurrence.scope->kind)) +
                    ":" + occurrence.scope->subject_entity_id;
            }
            const auto owner_key = scope_token + "\x1f" + schema.schema_id;
            if (!scoped_state_schema_owners.insert(owner_key).second) {
                diagnostic(outcome.diagnostics,
                           CompleteDiagnosticCode::MultipleStateOwners,
                           occurrence.source, schema.schema_id,
                           "a state schema has multiple owners in the same typed scope");
                continue;
            }
        }
        std::vector<CanonicalConfigFieldProvenance>
            canonical_configuration_sources;
        for (auto& provenance : configuration_field_sources) {
            canonical_configuration_sources.push_back(
                {std::move(provenance.field_id),
                 std::move(provenance.source)});
        }
        ir.occurrences.push_back(
            {occurrence.occurrence_id, found->second.package, descriptor,
             occurrence.subject_entity_id, occurrence.subject_source,
             occurrence.scope, occurrence.scope_source,
             occurrence.placement, occurrence.placement_source,
             std::move(config), occurrence.configuration_source,
             std::move(canonical_configuration_sources),
             std::move(assets), occurrence.source});
    }

    const auto canonicalize_by_id = [](auto values, auto id) {
        std::sort(values.begin(), values.end(), [&](const auto& lhs,
                                                    const auto& rhs) {
            return id(lhs) < id(rhs);
        });
        return values;
    };
    ir.initial_bindings = canonicalize_by_id(
        source.initial_bindings,
        [](const auto& value) -> const std::string& {
            return value.owner_occurrence_id;
        });
    std::set<std::string> initial_owners;
    for (auto& binding : ir.initial_bindings) {
        std::sort(binding.builder_inputs.fields.begin(),
                  binding.builder_inputs.fields.end(),
                   [](const auto& lhs, const auto& rhs) {
                       return lhs.field_id < rhs.field_id;
                   });
        std::sort(binding.field_sources.begin(), binding.field_sources.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return lhs.field_id < rhs.field_id;
                  });
        const auto occurrence = std::find_if(
            ir.occurrences.begin(), ir.occurrences.end(),
            [&](const auto& value) {
                return value.occurrence_id == binding.owner_occurrence_id;
            });
        bool valid = !binding.owner_occurrence_id.empty() &&
                     initial_owners.insert(binding.owner_occurrence_id).second &&
                     valid_source_ref(binding.source) &&
                     occurrence != ir.occurrences.end() &&
                     occurrence->descriptor.runtime_component.has_value() &&
                     occurrence->descriptor.runtime_component->state_owner.has_value();
        std::vector<gnc::model_sdk::StaticConfigFieldDescriptor>
            expected_fields;
        if (valid) {
            const auto& schema = occurrence->descriptor.runtime_component
                                     ->state_owner->initial_state_input_schema;
            expected_fields = schema.fields;
            std::sort(expected_fields.begin(), expected_fields.end(),
                      [](const auto& lhs, const auto& rhs) {
                          return lhs.field_id < rhs.field_id;
                      });
            valid = !schema.schema_id.empty() &&
                    schema.schema_version != 0U &&
                    binding.builder_inputs.schema_id == schema.schema_id &&
                    binding.builder_inputs.schema_version ==
                        schema.schema_version &&
                    expected_fields.size() ==
                        binding.builder_inputs.fields.size() &&
                    binding.field_sources.size() == expected_fields.size();
        }
        for (std::size_t index = 0U;
             index < binding.builder_inputs.fields.size(); ++index) {
            const auto& field = binding.builder_inputs.fields[index];
            valid = valid && index < binding.field_sources.size() &&
                    valid_source_ref(binding.field_sources[index].source) &&
                    !field.field_id.empty() &&
                    index < expected_fields.size() &&
                    field.field_id == expected_fields[index].field_id &&
                    binding.field_sources[index].field_id == field.field_id &&
                    config_value_matches(field.value,
                                         expected_fields[index].value_kind) &&
                    (index == 0U ||
                     binding.builder_inputs.fields[index - 1U].field_id !=
                         field.field_id);
        }
        if (!valid) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::InvalidSource,
                       binding.source, binding.owner_occurrence_id,
                       "initial binding must exactly match one state owner's builder-input schema, values, and provenance");
        }
    }
    ir.bindings = canonicalize_by_id(
        source.bindings,
        [](const auto& value) -> const std::string& {
            return value.binding_id;
        });
    ir.invocation_bindings = canonicalize_by_id(
        source.invocation_bindings,
        [](const auto& value) -> const std::string& {
            return value.invocation_id;
        });
    ir.integration_scopes = canonicalize_by_id(
        source.integration_scopes,
        [](const auto& value) -> const std::string& {
            return value.integration_scope_id;
        });
    ir.transactions = canonicalize_by_id(
        source.transactions,
        [](const auto& value) -> const std::string& {
            return value.transaction_id;
        });
    ir.evaluator_histories = canonicalize_by_id(
        source.evaluator_histories,
        [](const auto& value) -> const std::string& {
            return value.history_id;
        });
    for (auto& scope : ir.integration_scopes) {
        std::sort(scope.closure_invocation_ids.begin(),
                  scope.closure_invocation_ids.end());
    }
    for (auto& transaction : ir.transactions) {
        std::sort(transaction.owner_occurrence_ids.begin(),
                  transaction.owner_occurrence_ids.end());
    }
    // Evaluator owner order is callable shape, not declaration noise. It is
    // validated against the package-authored ordered history members during
    // plan lowering and therefore must not be canonicalized by sorting.
    if (outcome.diagnostics.empty()) {
        outcome.value = std::move(ir);
    }
    return outcome;
}

namespace complete_plan_detail {

struct LoweringContext {
    const CompleteCanonicalMissionIr& ir;
    CompleteExecutionPlanDescriptor plan;
    std::vector<CompleteDiagnostic> diagnostics;
    std::map<std::string, const CompleteCanonicalOccurrence*> occurrences;
    std::map<std::string, const gnc::model_sdk::StaticRuntimeObligationEntryDescriptor*>
        callsite_entries;
    std::map<std::string, std::size_t> callsite_indices;
    std::map<std::string, std::string> output_slots;
    std::map<std::string, std::vector<std::string>> input_slots;
    std::map<std::string, std::size_t> query_indices;
    std::map<std::string, std::size_t> closure_indices;
    std::map<std::string, std::size_t> state_indices;
    std::map<std::string, std::string> state_writer_callsites;
    std::map<std::string, std::string> integration_producers;
    std::map<std::string, std::string> projection_callsites;
    std::map<std::string, std::size_t> invocation_indices;
    std::map<std::string, std::vector<std::string>> output_port_callsites;
    std::map<std::string, std::vector<std::string>> input_port_callsites;
};

inline void add_entry_requirement(
    LoweringContext& context, const CompleteCanonicalOccurrence& occurrence,
    std::string requirement_id, std::string entry_id,
    std::string entry_version, gnc::model_sdk::StaticEntryKind kind,
    std::string signature_id, std::string call_shape_id,
    std::string state_layout_id,
    std::string workspace_layout_id, SourceRef source) {
    const auto element = "link-requirement/" + requirement_id;
    context.plan.entry_requirements.push_back(
        {element, std::move(requirement_id), occurrence.package,
         std::move(entry_id), std::move(entry_version), kind,
         std::move(signature_id), std::move(call_shape_id),
         std::move(state_layout_id),
         std::move(workspace_layout_id), std::move(source)});
}

[[nodiscard]] inline const CompleteCanonicalOccurrence* find_occurrence(
    const LoweringContext& context, std::string_view id) {
    const auto found = context.occurrences.find(std::string(id));
    return found == context.occurrences.end() ? nullptr : found->second;
}

[[nodiscard]] inline const RuntimeCallsitePlan* find_callsite(
    const LoweringContext& context, std::string_view id) {
    const auto found = context.callsite_indices.find(std::string(id));
    return found == context.callsite_indices.end()
               ? nullptr
               : &context.plan.runtime_callsites[found->second];
}

[[nodiscard]] inline RuntimeCallsitePlan* find_callsite(
    LoweringContext& context, std::string_view id) {
    const auto found = context.callsite_indices.find(std::string(id));
    return found == context.callsite_indices.end()
               ? nullptr
               : &context.plan.runtime_callsites[found->second];
}

[[nodiscard]] inline const StateBlockPlan* find_state(
    const LoweringContext& context, std::string_view occurrence) {
    const auto found = context.state_indices.find(std::string(occurrence));
    return found == context.state_indices.end()
               ? nullptr
               : &context.plan.state_blocks[found->second];
}

[[nodiscard]] inline bool same_scope(const ScopeKey& scope,
                                     const CompleteCanonicalOccurrence& occurrence) {
    return occurrence.scope.has_value() && *occurrence.scope == scope;
}

[[nodiscard]] inline std::uint32_t phase_rank(
    gnc::model_sdk::CoarsePhase phase) noexcept {
    switch (phase) {
    case gnc::model_sdk::CoarsePhase::Publish:
        return 0U;
    case gnc::model_sdk::CoarsePhase::Evaluation:
        return 1U;
    case gnc::model_sdk::CoarsePhase::Process:
        return 2U;
    case gnc::model_sdk::CoarsePhase::Output:
        return 3U;
    case gnc::model_sdk::CoarsePhase::Form:
        return 4U;
    case gnc::model_sdk::CoarsePhase::Unspecified:
        return 99U;
    }
    return 99U;
}

[[nodiscard]] inline std::vector<std::string> callsites_for_obligation(
    const LoweringContext& context, std::string_view occurrence,
    gnc::contracts::ExecutionObligation obligation) {
    std::vector<std::string> result;
    for (const auto& callsite : context.plan.runtime_callsites) {
        if (callsite.occurrence_id == occurrence &&
            callsite.obligation == obligation) {
            result.push_back(callsite.callsite_id);
        }
    }
    return result;
}

inline void lower_occurrences(LoweringContext& context) {
    std::set<std::string> used_packages;
    for (const auto& occurrence : context.ir.occurrences) {
        context.occurrences.emplace(occurrence.occurrence_id, &occurrence);
        used_packages.insert(exact_key(occurrence.package.package_id,
                                       occurrence.package.package_version));
        context.plan.occurrences.push_back(
            {occurrence_element(occurrence.occurrence_id),
             occurrence.occurrence_id, occurrence.package,
             occurrence.descriptor.definition.model_id,
             occurrence.descriptor.definition.model_version,
             occurrence.descriptor.definition.execution_form,
             occurrence.descriptor.placement, occurrence.subject_entity_id,
             occurrence.subject_source, occurrence.scope,
             occurrence.scope_source, occurrence.placement_source,
             occurrence.configuration, occurrence.configuration_source,
             occurrence.configuration_field_sources,
             occurrence.asset_bindings, occurrence.source});
        auto ports = occurrence.descriptor.ports;
        std::sort(ports.begin(), ports.end(), [](const auto& lhs,
                                                 const auto& rhs) {
            return lhs.port_id < rhs.port_id;
        });
        for (const auto& port : ports) {
            context.plan.ports.push_back(
                {port_element(occurrence.occurrence_id, port.port_id),
                 occurrence.occurrence_id, port.port_id, port.contract_id,
                 port.direction, port.binding_kind, port.cardinality,
                 port.temporal_relation, occurrence.source});
            if (port.direction ==
                gnc::model_sdk::StaticPortDirection::Output) {
                const auto slot =
                    port_slot_id(occurrence.occurrence_id, port.port_id);
                context.output_slots.emplace(
                    occurrence.occurrence_id + "\x1f" + port.port_id, slot);
                context.plan.slots.push_back(
                    {slot, slot, CompleteSlotKind::PortValue,
                     occurrence.occurrence_id, port.port_id,
                     port.contract_id, occurrence.source});
            }
        }

        const auto& descriptor = occurrence.descriptor;
        if (descriptor.pure_query.has_value() ||
            descriptor.closure.has_value()) {
            const auto prepare_id = entry_requirement_id(
                occurrence.occurrence_id, "prepare",
                descriptor.preparation_algorithm_id);
            add_entry_requirement(
                context, occurrence, prepare_id,
                descriptor.preparation_algorithm_id,
                descriptor.preparation_algorithm_version,
                gnc::model_sdk::StaticEntryKind::Prepare,
                gnc::model_sdk::canonical_prepare_signature(descriptor),
                descriptor.preparation_call_shape_id, {},
                std::string(kNoWorkspaceLayoutIdentity), occurrence.source);
            context.plan.preparation_inputs.push_back(
                {"preparation-input/" + occurrence.occurrence_id,
                 "prepare/" + occurrence.occurrence_id,
                 occurrence.occurrence_id, occurrence.package,
                 descriptor.definition.model_id,
                 descriptor.definition.model_version, prepare_id,
                 occurrence.configuration, occurrence.asset_bindings,
                 occurrence.source});
        }
        if (descriptor.pure_query.has_value()) {
            const auto& query = *descriptor.pure_query;
            const auto requirement_id = entry_requirement_id(
                occurrence.occurrence_id, "query", query.query_entry_id);
            add_entry_requirement(
                context, occurrence, requirement_id, query.query_entry_id,
                query.query_entry_version,
                gnc::model_sdk::StaticEntryKind::PureQuery,
                gnc::model_sdk::canonical_query_signature(descriptor),
                query.query_call_shape_id, {},
                std::string(kNoWorkspaceLayoutIdentity), occurrence.source);
            context.query_indices.emplace(
                occurrence.occurrence_id, context.plan.queries.size());
            context.plan.queries.push_back(
                {"query-plan/" + occurrence.occurrence_id,
                 "query/" + occurrence.occurrence_id,
                 occurrence.occurrence_id,
                 "prepare/" + occurrence.occurrence_id, requirement_id,
                 query.workspace_requirement, {}, occurrence.source});
        }
        if (descriptor.closure.has_value()) {
            const auto& closure = *descriptor.closure;
            const auto requirement_id = entry_requirement_id(
                occurrence.occurrence_id, "closure",
                closure.closure_entry_id);
            add_entry_requirement(
                context, occurrence, requirement_id,
                closure.closure_entry_id, closure.closure_entry_version,
                gnc::model_sdk::StaticEntryKind::Closure,
                gnc::model_sdk::canonical_closure_signature(descriptor),
                closure.closure_call_shape_id, {},
                std::string(kNoWorkspaceLayoutIdentity), occurrence.source);
            context.closure_indices.emplace(
                occurrence.occurrence_id, context.plan.closures.size());
            context.plan.closures.push_back(
                {"closure-plan/" + occurrence.occurrence_id,
                 "closure/" + occurrence.occurrence_id,
                 occurrence.occurrence_id,
                 "prepare/" + occurrence.occurrence_id, requirement_id,
                 closure.strategy, closure.workspace_requirement, {},
                 occurrence.source});
        }
        if (!descriptor.runtime_component.has_value()) {
            continue;
        }
        const auto& runtime = *descriptor.runtime_component;
        const auto definition_builder_requirement = entry_requirement_id(
            occurrence.occurrence_id, "definition-builder",
            runtime.definition_builder_id);
        add_entry_requirement(
            context, occurrence, definition_builder_requirement,
            runtime.definition_builder_id,
            runtime.definition_builder_version,
            gnc::model_sdk::StaticEntryKind::DefinitionBuilder,
            gnc::model_sdk::canonical_definition_builder_signature(
                descriptor),
            runtime.definition_builder_call_shape_id, {},
            std::string(kNoWorkspaceLayoutIdentity), occurrence.source);
        const auto tick_span = static_cast<std::uint64_t>(
            context.ir.clock.terminal_tick -
            context.ir.clock.initial_tick);
        const bool cadence_valid =
            (runtime.schedule.trigger ==
                 gnc::model_sdk::StaticScheduleTrigger::EveryBoundary &&
             runtime.schedule.step_interval > 0U) ||
            (runtime.schedule.trigger ==
                 gnc::model_sdk::StaticScheduleTrigger::TerminalSequenceReady &&
             runtime.schedule.step_interval == 0U);
        if (!cadence_valid ||
            static_cast<std::uint64_t>(runtime.schedule.offset) > tick_span) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidCatalog,
                       occurrence.source, occurrence.occurrence_id,
                       "runtime schedule trigger/interval/offset is incompatible with the selected clock grid");
        }
        RuntimeComponentPlan component{
            "runtime-component/" + occurrence.occurrence_id,
            occurrence.occurrence_id, definition_builder_requirement,
            runtime.recipe_id, runtime.profile,
            runtime.schedule, runtime.lifecycle_capabilities, {},
            occurrence.source};
        std::string state_layout;
        if (runtime.state_owner.has_value()) {
            const auto& owner = *runtime.state_owner;
            state_layout = owner.schema.layout_id;
            const auto committed = committed_slot_id(occurrence.occurrence_id);
            const auto candidate = candidate_slot_id(occurrence.occurrence_id);
            context.plan.slots.push_back(
                {committed, committed, CompleteSlotKind::CommittedState,
                 occurrence.occurrence_id, {}, state_layout,
                 occurrence.source});
            context.plan.slots.push_back(
                {candidate, candidate, CompleteSlotKind::CandidateState,
                 occurrence.occurrence_id, {}, state_layout,
                 occurrence.source});
            const auto builder_requirement = entry_requirement_id(
                occurrence.occurrence_id, "initial-state",
                owner.initial_state_builder_id);
            add_entry_requirement(
                context, occurrence, builder_requirement,
                owner.initial_state_builder_id,
                owner.initial_state_builder_version,
                gnc::model_sdk::StaticEntryKind::InitialState,
                gnc::model_sdk::canonical_initial_state_signature(descriptor),
                owner.initial_state_builder_call_shape_id, state_layout,
                std::string(kNoWorkspaceLayoutIdentity), occurrence.source);
            context.state_indices.emplace(
                occurrence.occurrence_id, context.plan.state_blocks.size());
            context.plan.state_blocks.push_back(
                {"state-block/" + occurrence.occurrence_id,
                 occurrence.occurrence_id, owner.schema.schema_id,
                 owner.schema.schema_version, owner.schema.layout_id,
                 owner.evolution, committed, candidate,
                 occurrence.source});
            const auto initial = std::find_if(
                context.ir.initial_bindings.begin(),
                context.ir.initial_bindings.end(), [&](const auto& value) {
                    return value.owner_occurrence_id == occurrence.occurrence_id;
                });
            if (initial == context.ir.initial_bindings.end()) {
                diagnostic(context.diagnostics,
                           CompleteDiagnosticCode::InvalidSource,
                           occurrence.source, occurrence.occurrence_id,
                           "state owner has no immutable initial binding");
            } else {
                context.plan.initial_states.push_back(
                    {"initial-state/" + occurrence.occurrence_id,
                     occurrence.occurrence_id, committed,
                     builder_requirement, initial->builder_inputs,
                     initial->field_sources,
                     initial->source});
            }
        }
        std::size_t writer_count = 0U;
        std::size_t projection_count = 0U;
        std::vector<const gnc::model_sdk::StaticRuntimeObligationEntryDescriptor*>
            runtime_entries;
        for (const auto& entry : runtime.obligation_entries) {
            runtime_entries.push_back(&entry);
        }
        std::sort(runtime_entries.begin(), runtime_entries.end(),
                  [](const auto* lhs, const auto* rhs) {
                      return std::tie(lhs->phase, lhs->obligation,
                                      lhs->entry_id, lhs->entry_version) <
                             std::tie(rhs->phase, rhs->obligation,
                                      rhs->entry_id, rhs->entry_version);
                  });
        for (const auto* entry_pointer : runtime_entries) {
            const auto& entry = *entry_pointer;
            bool ports_valid = true;
            for (const auto& input : entry.input_port_ids) {
                const auto* port = find_port(descriptor, input);
                if (port == nullptr ||
                    port->direction !=
                        gnc::model_sdk::StaticPortDirection::Input) {
                    diagnostic(context.diagnostics,
                               CompleteDiagnosticCode::InvalidCatalog,
                               occurrence.source, input,
                               "runtime entry input does not name an input port");
                    ports_valid = false;
                }
            }
            for (const auto& output : entry.output_port_ids) {
                const auto* port = find_port(descriptor, output);
                if (port == nullptr ||
                    port->direction !=
                        gnc::model_sdk::StaticPortDirection::Output) {
                    diagnostic(context.diagnostics,
                               CompleteDiagnosticCode::InvalidCatalog,
                               occurrence.source, output,
                               "runtime entry output does not name an output port");
                    ports_valid = false;
                }
            }
            if (!ports_valid) {
                continue;
            }
            if (entry.obligation ==
                    gnc::contracts::ExecutionObligation::PublishProjection &&
                (entry.state_read !=
                     gnc::model_sdk::StaticStateReadKind::Committed ||
                 entry.state_write !=
                     gnc::model_sdk::StaticStateWriteKind::None)) {
                diagnostic(context.diagnostics,
                           CompleteDiagnosticCode::InvalidCatalog,
                           occurrence.source, entry.entry_id,
                           "projection must read committed owner state and cannot write candidate state");
            }
            if (entry.state_write ==
                    gnc::model_sdk::StaticStateWriteKind::IntervalCandidate &&
                entry.state_read !=
                    gnc::model_sdk::StaticStateReadKind::Committed) {
                diagnostic(context.diagnostics,
                           CompleteDiagnosticCode::InvalidCatalog,
                           occurrence.source, entry.entry_id,
                           "interval candidate writer must read committed owner state");
            }
            const auto callsite = callsite_id(
                occurrence.occurrence_id, entry.obligation,
                entry.entry_id);
            const auto link_requirement = entry_requirement_id(
                occurrence.occurrence_id,
                gnc::contracts::to_string(entry.obligation),
                entry.entry_id);
            add_entry_requirement(
                context, occurrence, link_requirement, entry.entry_id,
                entry.entry_version, entry_kind(entry.obligation),
                gnc::model_sdk::canonical_runtime_entry_signature(
                    descriptor, entry),
                entry.call_shape_id,
                entry.state_read !=
                            gnc::model_sdk::StaticStateReadKind::None ||
                        entry.state_write !=
                            gnc::model_sdk::StaticStateWriteKind::None
                    ? state_layout
                    : std::string{},
                std::string(kNoWorkspaceLayoutIdentity), occurrence.source);
            RuntimeCallsitePlan callsite_plan{
                "callsite/" + callsite, callsite, occurrence.occurrence_id,
                entry.obligation, entry.phase, link_requirement,
                entry.request_contract_id, entry.result_contract_id,
                {}, {}, {}, {}, occurrence.source};
            if (entry.state_read !=
                gnc::model_sdk::StaticStateReadKind::None) {
                if (!runtime.state_owner.has_value()) {
                    diagnostic(context.diagnostics,
                               CompleteDiagnosticCode::InvalidCatalog,
                               occurrence.source, callsite,
                               "entry reads owner state but component declares no owner");
                } else {
                    callsite_plan.input_slot_ids.push_back(
                        entry.state_read ==
                                gnc::model_sdk::StaticStateReadKind::Committed
                            ? committed_slot_id(occurrence.occurrence_id)
                            : candidate_slot_id(occurrence.occurrence_id));
                }
            }
            for (const auto& input : entry.input_port_ids) {
                context.input_port_callsites[
                    occurrence.occurrence_id + "\x1f" + input]
                    .push_back(callsite);
            }
            for (const auto& output : entry.output_port_ids) {
                callsite_plan.output_slot_ids.push_back(
                    port_slot_id(occurrence.occurrence_id, output));
                context.output_port_callsites[
                    occurrence.occurrence_id + "\x1f" + output]
                    .push_back(callsite);
            }
            if (entry.state_write !=
                gnc::model_sdk::StaticStateWriteKind::None) {
                ++writer_count;
                callsite_plan.output_slot_ids.push_back(
                    candidate_slot_id(occurrence.occurrence_id));
                context.state_writer_callsites[occurrence.occurrence_id] =
                    callsite;
            }
            if (entry.obligation ==
                gnc::contracts::ExecutionObligation::PublishProjection) {
                ++projection_count;
                context.projection_callsites[occurrence.occurrence_id] =
                    callsite;
                context.plan.projections.push_back(
                    {"projection/" + occurrence.occurrence_id,
                     occurrence.occurrence_id, callsite,
                     committed_slot_id(occurrence.occurrence_id),
                     callsite_plan.output_slot_ids, occurrence.source});
            }
            component.callsite_ids.push_back(callsite);
            context.callsite_entries.emplace(callsite, &entry);
            context.callsite_indices.emplace(
                callsite, context.plan.runtime_callsites.size());
            context.plan.runtime_callsites.push_back(
                std::move(callsite_plan));
        }
        if (runtime.state_owner.has_value()) {
            const auto evolution = runtime.state_owner->evolution;
            if (evolution ==
                    gnc::model_sdk::StaticStateEvolution::IntervalCandidate &&
                writer_count == 0U) {
                diagnostic(context.diagnostics,
                           CompleteDiagnosticCode::MissingStateWriter,
                           occurrence.source, occurrence.occurrence_id,
                           "state owner has no candidate writer obligation");
            } else if (writer_count > 1U ||
                       (evolution == gnc::model_sdk::StaticStateEvolution::
                                         ContinuousCandidate &&
                        writer_count != 0U)) {
                diagnostic(context.diagnostics,
                           CompleteDiagnosticCode::MultipleStateWriters,
                           occurrence.source, occurrence.occurrence_id,
                           "state owner has multiple candidate writer obligations");
            }
            if (projection_count != 1U) {
                diagnostic(context.diagnostics,
                           projection_count == 0U
                               ? CompleteDiagnosticCode::MissingStateWriter
                               : CompleteDiagnosticCode::MultipleStateWriters,
                           occurrence.source, occurrence.occurrence_id,
                           "state owner requires exactly one committed projection obligation");
            }
        }
        std::sort(component.callsite_ids.begin(),
                  component.callsite_ids.end());
        context.plan.runtime_components.push_back(std::move(component));
    }
    for (const auto& package_key : used_packages) {
        const auto separator = package_key.find('\x1f');
        context.plan.dependency_lock.push_back(
            {package_key.substr(0U, separator),
             package_key.substr(separator + 1U)});
    }
}

inline void lower_bindings(LoweringContext& context) {
    std::set<std::string> binding_ids;
    std::map<std::string, std::size_t> provider_counts;
    for (const auto& binding : context.ir.bindings) {
        if (binding.binding_id.empty() ||
            !binding_ids.insert(binding.binding_id).second ||
            !valid_source_ref(binding.source)) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidSource, binding.source,
                       binding.binding_id,
                       "binding identity/source is empty or duplicated");
            continue;
        }
        const auto* provider =
            find_occurrence(context, binding.provider_occurrence_id);
        const auto* consumer =
            find_occurrence(context, binding.consumer_occurrence_id);
        if (provider == nullptr || consumer == nullptr) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::UnknownEndpoint,
                       binding.source, binding.binding_id,
                       "binding occurrence endpoint is unknown");
            continue;
        }
        const auto* provider_port = find_port(
            provider->descriptor, binding.provider_port_id);
        const auto* consumer_port = find_port(
            consumer->descriptor, binding.consumer_port_id);
        if (provider_port == nullptr || consumer_port == nullptr) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::UnknownEndpoint,
                       binding.source, binding.binding_id,
                       "binding port endpoint is unknown");
            continue;
        }
        if (provider_port->direction !=
                gnc::model_sdk::StaticPortDirection::Output ||
            consumer_port->direction !=
                gnc::model_sdk::StaticPortDirection::Input) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::PortDirectionMismatch,
                       binding.source, binding.binding_id,
                       "binding must connect an output to an input");
            continue;
        }
        if (provider_port->contract_id != consumer_port->contract_id) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::ContractMismatch,
                       binding.source, binding.binding_id,
                       "binding endpoint contracts differ");
            continue;
        }
        if (provider_port->binding_kind != consumer_port->binding_kind) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::BindingKindMismatch,
                       binding.source, binding.binding_id,
                       "binding endpoint kinds differ");
            continue;
        }
        if (provider_port->temporal_relation !=
            consumer_port->temporal_relation) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::TemporalMismatch,
                       binding.source, binding.binding_id,
                       "binding endpoint temporal relations differ");
            continue;
        }
        if (!compatible_scopes(*provider, *consumer)) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::ScopeMismatch,
                       binding.source, binding.binding_id,
                       "binding endpoints do not share a typed scope and provider is not global environment");
            continue;
        }
        const auto output_key = binding.provider_occurrence_id + "\x1f" +
                                binding.provider_port_id;
        const auto slot = context.output_slots.find(output_key);
        if (slot == context.output_slots.end()) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::UnknownEndpoint,
                       binding.source, binding.binding_id,
                       "provider output slot was not derived");
            continue;
        }
        const auto input_key = binding.consumer_occurrence_id + "\x1f" +
                               binding.consumer_port_id;
        ++provider_counts[input_key];
        context.input_slots[input_key].push_back(slot->second);
        context.plan.bindings.push_back(
            {"binding/" + binding.binding_id, binding.binding_id,
             binding.provider_occurrence_id, binding.provider_port_id,
             slot->second, binding.consumer_occurrence_id,
             binding.consumer_port_id, provider_port->contract_id,
             provider_port->binding_kind,
             provider_port->temporal_relation, binding.source});
    }
    for (const auto& occurrence : context.ir.occurrences) {
        for (const auto& port : occurrence.descriptor.ports) {
            if (port.direction !=
                    gnc::model_sdk::StaticPortDirection::Input ||
                port.binding_kind ==
                    gnc::model_sdk::BindingKind::AssetBinding) {
                continue;
            }
            const auto key = occurrence.occurrence_id + "\x1f" + port.port_id;
            const auto count = provider_counts[key];
            const bool supplied_by_committed_history =
                std::any_of(
                    context.ir.evaluator_histories.begin(),
                    context.ir.evaluator_histories.end(),
                    [&](const auto& history) {
                        return history.evaluator_occurrence_id ==
                               occurrence.occurrence_id;
                    });
            if (supplied_by_committed_history && count == 0U) {
                // The terminal evaluator consumes the compiler-frozen
                // committed-history slots, not an authored runtime binding.
                continue;
            }
            const bool missing = count == 0U;
            const bool multiple =
                port.cardinality ==
                    gnc::model_sdk::PortCardinality::ExactlyOne &&
                count > 1U;
            if (missing || multiple) {
                diagnostic(context.diagnostics,
                           missing ? CompleteDiagnosticCode::MissingProvider
                                   : CompleteDiagnosticCode::MultipleProviders,
                           occurrence.source,
                           occurrence.occurrence_id + "." + port.port_id,
                           missing
                               ? "required runtime input has no provider"
                               : "exactly-one runtime input has multiple providers");
            }
        }
    }
    for (auto& [key, slots] : context.input_slots) {
        std::sort(slots.begin(), slots.end());
    }
    for (auto& callsite : context.plan.runtime_callsites) {
        const auto* entry = context.callsite_entries.at(callsite.callsite_id);
        for (const auto& port_id : entry->input_port_ids) {
            const auto key = callsite.occurrence_id + "\x1f" + port_id;
            const auto found = context.input_slots.find(key);
            if (found != context.input_slots.end()) {
                callsite.input_slot_ids.insert(callsite.input_slot_ids.end(),
                                               found->second.begin(),
                                               found->second.end());
            }
        }
        std::sort(callsite.input_slot_ids.begin(),
                  callsite.input_slot_ids.end());
        std::sort(callsite.output_slot_ids.begin(),
                  callsite.output_slot_ids.end());
    }
}

inline void lower_regions(LoweringContext& context) {
    std::map<gnc::model_sdk::CoarsePhase, std::vector<std::string>> members;
    for (auto& callsite : context.plan.runtime_callsites) {
        if (callsite.phase == gnc::model_sdk::CoarsePhase::Unspecified) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidRegion,
                       callsite.source, callsite.callsite_id,
                       "package obligation entry has no phase");
            continue;
        }
        if (callsite.obligation ==
            gnc::contracts::ExecutionObligation::DerivativeEvaluation) {
            callsite.region_id = "integration-owned";
            continue;
        }
        callsite.region_id =
            "phase/" + std::string(gnc::model_sdk::to_string(callsite.phase));
        members[callsite.phase].push_back(callsite.callsite_id);
    }
    std::vector<gnc::model_sdk::CoarsePhase> phases;
    for (const auto& [phase, callsites] : members) {
        static_cast<void>(callsites);
        phases.push_back(phase);
    }
    std::sort(phases.begin(), phases.end(), [](const auto lhs,
                                               const auto rhs) {
        return phase_rank(lhs) < phase_rank(rhs);
    });
    for (const auto phase : phases) {
        auto callsites = members[phase];
        std::sort(callsites.begin(), callsites.end());
        const auto id =
            "phase/" + std::string(gnc::model_sdk::to_string(phase));
        context.plan.regions.push_back(
            {"region/" + id, id, phase_rank(phase), phase,
             std::move(callsites), context.ir.mission_source});
    }
}

inline void lower_invocations(LoweringContext& context) {
    std::set<std::string> ids;
    std::map<std::string, std::size_t> counts;
    std::map<std::string, std::size_t> result_flow_counts;
    for (const auto& binding : context.ir.invocation_bindings) {
        if (binding.invocation_id.empty() ||
            !ids.insert(binding.invocation_id).second ||
            !valid_source_ref(binding.source)) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidSource,
                       binding.source, binding.invocation_id,
                       "invocation identity/source is empty or duplicated");
            continue;
        }
        const auto caller_callsites = callsites_for_obligation(
            context, binding.caller_occurrence_id,
            binding.caller_obligation);
        auto* callsite = caller_callsites.size() == 1U
                             ? find_callsite(context, caller_callsites.front())
                             : nullptr;
        const auto* provider =
            find_occurrence(context, binding.provider_occurrence_id);
        if (callsite == nullptr || provider == nullptr) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::UnknownCallsite,
                       binding.source, binding.invocation_id,
                       "invocation caller occurrence/obligation does not resolve exactly once, or provider is unknown");
            continue;
        }
        const auto* caller = find_occurrence(context, callsite->occurrence_id);
        const auto* entry = context.callsite_entries.at(callsite->callsite_id);
        const auto requirement = std::find_if(
            entry->invocation_requirements.begin(),
            entry->invocation_requirements.end(), [&](const auto& item) {
                return item.requirement_id == binding.requirement_id;
            });
        if (requirement == entry->invocation_requirements.end()) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::MissingInvocationAuthorization,
                       binding.source, binding.invocation_id,
                       "caller entry does not declare this invocation requirement");
            continue;
        }
        const auto expected_form =
            requirement->kind ==
                    gnc::model_sdk::StaticInvocationKind::PureQuery
                ? gnc::model_sdk::ModelExecutionForm::PureQuery
                : gnc::model_sdk::ModelExecutionForm::Closure;
        if (provider->descriptor.definition.execution_form != expected_form) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvocationKindMismatch,
                       binding.source, binding.invocation_id,
                       "invocation provider execution form differs from the package requirement");
            continue;
        }
        const auto provider_request_contract =
            expected_form == gnc::model_sdk::ModelExecutionForm::PureQuery
                ? provider->descriptor.pure_query->request_contract_id
                : provider->descriptor.closure->request_contract_id;
        if (provider_request_contract != requirement->contract_id) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::ContractMismatch,
                       binding.source, binding.invocation_id,
                       "invocation provider request contract differs from the caller requirement");
            continue;
        }
        if (!compatible_scopes(*provider, *caller)) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::ScopeMismatch,
                       binding.source, binding.invocation_id,
                       "invocation provider and authorized caller have incompatible scopes");
            continue;
        }
        const auto expected_result_kind =
            requirement->kind ==
                    gnc::model_sdk::StaticInvocationKind::PureQuery
                ? gnc::model_sdk::BindingKind::PureQuery
                : gnc::model_sdk::BindingKind::ContinuousClosureLink;
        std::vector<const CompleteBindingPlan*> result_flows;
        for (const auto& candidate : context.plan.bindings) {
            if (candidate.provider_occurrence_id ==
                    binding.provider_occurrence_id &&
                candidate.consumer_occurrence_id ==
                    callsite->occurrence_id &&
                candidate.binding_kind == expected_result_kind &&
                std::find(entry->input_port_ids.begin(),
                          entry->input_port_ids.end(),
                          candidate.consumer_port_id) !=
                    entry->input_port_ids.end()) {
                result_flows.push_back(&candidate);
            }
        }
        if (result_flows.size() != 1U) {
            diagnostic(
                context.diagnostics,
                result_flows.empty()
                    ? CompleteDiagnosticCode::MissingInvocationResultFlow
                    : CompleteDiagnosticCode::AmbiguousInvocationResultFlow,
                binding.source, binding.invocation_id,
                result_flows.empty()
                    ? "authorized invocation has no exact provider-result to caller-port Binding"
                    : "authorized invocation resolves multiple provider-result to caller-port Bindings");
            continue;
        }
        const auto* result_flow = result_flows.front();
        const auto supplied_slot = std::find(
            callsite->input_slot_ids.begin(),
            callsite->input_slot_ids.end(),
            result_flow->provider_slot_id);
        if (supplied_slot == callsite->input_slot_ids.end()) {
            diagnostic(
                context.diagnostics,
                CompleteDiagnosticCode::MissingInvocationResultFlow,
                binding.source, binding.invocation_id,
                "invocation result Binding is absent from the resolved caller input before lowering");
            continue;
        }
        // A query/closure response is produced by this authorized invocation;
        // it is not an independently scheduled producer or a value that may
        // be read before the caller invokes the provider. Retain the Binding
        // as the exact result-flow fact, but remove its slot from ordinary
        // pre-call inputs and freeze the endpoint on InvocationBindingPlan.
        callsite->input_slot_ids.erase(supplied_slot);
        ++result_flow_counts[result_flow->binding_id];
        const auto count_key = callsite->callsite_id + "\x1f" +
                               binding.requirement_id;
        ++counts[count_key];
        std::string provider_plan;
        if (expected_form == gnc::model_sdk::ModelExecutionForm::PureQuery) {
            provider_plan = "query/" + binding.provider_occurrence_id;
            context.plan.queries[context.query_indices.at(
                                     binding.provider_occurrence_id)]
                .authorized_invocation_ids.push_back(binding.invocation_id);
        } else {
            provider_plan = "closure/" + binding.provider_occurrence_id;
            context.plan.closures[context.closure_indices.at(
                                      binding.provider_occurrence_id)]
                .authorized_invocation_ids.push_back(binding.invocation_id);
        }
        callsite->invocation_binding_ids.push_back(binding.invocation_id);
        context.invocation_indices.emplace(
            binding.invocation_id,
            context.plan.invocation_bindings.size());
        context.plan.invocation_bindings.push_back(
            {"invocation/" + binding.invocation_id, binding.invocation_id,
             callsite->callsite_id, binding.requirement_id,
             requirement->kind, requirement->contract_id,
             binding.provider_occurrence_id, provider_plan,
             result_flow->binding_id, result_flow->provider_slot_id,
             result_flow->consumer_port_id, binding.source});
    }
    for (const auto& [binding_id, count] : result_flow_counts) {
        if (count != 1U) {
            const auto flow = std::find_if(
                context.plan.bindings.begin(), context.plan.bindings.end(),
                [&](const auto& candidate) {
                    return candidate.binding_id == binding_id;
                });
            diagnostic(
                context.diagnostics,
                CompleteDiagnosticCode::AmbiguousInvocationResultFlow,
                flow == context.plan.bindings.end() ? SourceRef{}
                                                    : flow->source,
                binding_id,
                "one result-flow Binding cannot be shared by multiple authorized invocations");
        }
    }
    for (const auto& callsite : context.plan.runtime_callsites) {
        const auto* entry = context.callsite_entries.at(callsite.callsite_id);
        for (const auto& requirement : entry->invocation_requirements) {
            const auto count = counts[callsite.callsite_id + "\x1f" +
                                      requirement.requirement_id];
            const bool missing = count == 0U;
            const bool multiple =
                requirement.cardinality ==
                    gnc::model_sdk::PortCardinality::ExactlyOne &&
                count > 1U;
            if (missing || multiple) {
                diagnostic(
                    context.diagnostics,
                    missing
                        ? CompleteDiagnosticCode::MissingInvocationAuthorization
                        : CompleteDiagnosticCode::MultipleInvocationAuthorizations,
                    callsite.source,
                    callsite.callsite_id + ":" + requirement.requirement_id,
                    missing
                        ? "package invocation requirement has no source authorization"
                        : "exactly-one package invocation requirement has multiple source authorizations");
            }
        }
    }
    for (auto& query : context.plan.queries) {
        std::sort(query.authorized_invocation_ids.begin(),
                  query.authorized_invocation_ids.end());
        if (query.authorized_invocation_ids.empty()) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::MissingInvocationAuthorization,
                       query.source, query.query_plan_id,
                       "query plan has no source-authorized caller");
        }
    }
    for (auto& closure : context.plan.closures) {
        std::sort(closure.authorized_invocation_ids.begin(),
                  closure.authorized_invocation_ids.end());
        if (closure.authorized_invocation_ids.empty()) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::MissingInvocationAuthorization,
                       closure.source, closure.closure_plan_id,
                       "closure plan has no source-authorized caller");
        }
    }
    for (auto& callsite : context.plan.runtime_callsites) {
        std::sort(callsite.invocation_binding_ids.begin(),
                  callsite.invocation_binding_ids.end());
    }
}

inline void lower_dag(LoweringContext& context) {
    std::set<std::pair<std::string, std::string>> pairs;
    std::map<std::pair<std::string, std::string>, SourceRef> pair_sources;
    std::map<std::string, std::vector<std::string>> successors;
    std::map<std::string, std::size_t> indegree;
    std::map<std::string, std::uint32_t> ranks;
    for (const auto& callsite : context.plan.runtime_callsites) {
        if (callsite.obligation ==
            gnc::contracts::ExecutionObligation::DerivativeEvaluation) {
            continue;
        }
        const auto node_id = "callsite:" + callsite.callsite_id;
        context.plan.boundary_dag_nodes.push_back(
            {"dag-node/" + node_id, node_id,
             BoundaryDagNodeKind::RuntimeCallsite, callsite.callsite_id,
             callsite.phase, callsite.source});
        indegree.emplace(node_id, 0U);
        ranks.emplace(node_id, phase_rank(callsite.phase));
    }
    for (const auto& scope : context.plan.integration_scopes) {
        const auto node_id = "integration:" + scope.integration_scope_id;
        context.plan.boundary_dag_nodes.push_back(
            {"dag-node/" + node_id, node_id,
             BoundaryDagNodeKind::IntegrationScope,
             scope.integration_scope_id,
             gnc::model_sdk::CoarsePhase::Form, scope.source});
        indegree.emplace(node_id, 0U);
        ranks.emplace(node_id, 5U);
        const auto pair = std::make_pair(
            "callsite:" + scope.form_callsite_id, node_id);
        pairs.insert(pair);
        pair_sources.emplace(pair, scope.source);
    }
    for (const auto& binding : context.plan.bindings) {
        const auto provider_key = binding.provider_occurrence_id + "\x1f" +
                                  binding.provider_port_id;
        const auto consumer_key = binding.consumer_occurrence_id + "\x1f" +
                                  binding.consumer_port_id;
        const auto providers = context.output_port_callsites[provider_key];
        const auto consumers = context.input_port_callsites[consumer_key];
        if (providers.empty() || consumers.empty()) {
            continue;
        }
        if (providers.size() != 1U) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::MultipleProviders,
                       binding.source, binding.binding_id,
                       "runtime data edge must resolve exactly one producer callsite");
            continue;
        }
        const auto predecessor = "callsite:" + providers.front();
        if (indegree.find(predecessor) == indegree.end()) {
            continue;
        }
        for (const auto& consumer : consumers) {
            const auto successor = "callsite:" + consumer;
            // Derivative callsites are owned by IntegrationScope and are not
            // independently scheduled boundary nodes.
            if (indegree.find(successor) == indegree.end()) {
                continue;
            }
            if (ranks.at(predecessor) > ranks.at(successor)) {
                diagnostic(context.diagnostics,
                           CompleteDiagnosticCode::FuturePhaseDependency,
                           binding.source, binding.binding_id,
                           "typed data dependency reads a future phase");
                continue;
            }
            const auto pair = std::make_pair(predecessor, successor);
            pairs.insert(pair);
            pair_sources.emplace(pair, binding.source);
        }
    }
    std::vector<std::string> nodes;
    for (const auto& [node, rank] : ranks) {
        static_cast<void>(rank);
        nodes.push_back(node);
    }
    for (const auto& predecessor : nodes) {
        for (const auto& successor : nodes) {
            if (ranks.at(predecessor) < ranks.at(successor)) {
                const auto pair = std::make_pair(predecessor, successor);
                pairs.insert(pair);
                pair_sources.emplace(pair, context.ir.mission_source);
            }
        }
    }
    for (const auto& pair : pairs) {
        successors[pair.first].push_back(pair.second);
        ++indegree[pair.second];
        const auto edge_id = pair.first + "->" + pair.second;
        context.plan.boundary_dag.push_back(
            {"dag-edge/" + edge_id, edge_id, pair.first, pair.second,
             pair_sources.at(pair)});
    }
    std::set<std::string> ready;
    for (const auto& [id, degree] : indegree) {
        if (degree == 0U) {
            ready.insert(id);
        }
    }
    std::size_t visited = 0U;
    while (!ready.empty()) {
        const auto id = *ready.begin();
        ready.erase(ready.begin());
        ++visited;
        for (const auto& successor : successors[id]) {
            auto& degree = indegree[successor];
            --degree;
            if (degree == 0U) {
                ready.insert(successor);
            }
        }
    }
    if (visited != indegree.size()) {
        diagnostic(context.diagnostics, CompleteDiagnosticCode::DagCycle,
                   context.ir.mission_source, context.ir.plan_id,
                   "boundary dependency graph contains a cycle");
    }
}

inline void lower_integration_scopes(LoweringContext& context) {
    std::set<std::string> ids;
    std::map<std::string, std::size_t> owner_counts;
    for (const auto& source : context.ir.integration_scopes) {
        if (source.integration_scope_id.empty() ||
            !ids.insert(source.integration_scope_id).second ||
            !valid_source_ref(source.source)) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidIntegrationScope,
                       source.source, source.integration_scope_id,
                       "integration scope identity/source is empty or duplicated");
            continue;
        }
        const auto* owner =
            find_occurrence(context, source.owner_occurrence_id);
        const auto* state = find_state(context, source.owner_occurrence_id);
        const auto projection_ids = callsites_for_obligation(
            context, source.owner_occurrence_id,
            gnc::contracts::ExecutionObligation::PublishProjection);
        const auto form_ids = callsites_for_obligation(
            context, source.form_occurrence_id,
            gnc::contracts::ExecutionObligation::BoundaryEvaluation);
        const auto derivative_ids = callsites_for_obligation(
            context, source.owner_occurrence_id,
            gnc::contracts::ExecutionObligation::DerivativeEvaluation);
        const auto* projection = projection_ids.size() == 1U
                                     ? find_callsite(context,
                                                     projection_ids.front())
                                     : nullptr;
        auto* form = form_ids.size() == 1U
                         ? find_callsite(context, form_ids.front())
                         : nullptr;
        auto* derivative = derivative_ids.size() == 1U
                               ? find_callsite(context,
                                               derivative_ids.front())
                               : nullptr;
        bool valid = owner != nullptr && state != nullptr &&
                     projection != nullptr && form != nullptr &&
                     derivative != nullptr;
        valid = valid && same_scope(source.scope, *owner) &&
                state->evolution ==
                    gnc::model_sdk::StaticStateEvolution::ContinuousCandidate &&
                projection->occurrence_id == source.owner_occurrence_id &&
                projection->obligation ==
                    gnc::contracts::ExecutionObligation::PublishProjection &&
                derivative->occurrence_id == source.owner_occurrence_id &&
                derivative->obligation ==
                    gnc::contracts::ExecutionObligation::DerivativeEvaluation;
        if (form != nullptr) {
            const auto* form_occurrence =
                find_occurrence(context, form->occurrence_id);
            valid = valid && form_occurrence != nullptr &&
                    same_scope(source.scope, *form_occurrence) &&
                    form->obligation ==
                        gnc::contracts::ExecutionObligation::BoundaryEvaluation;
        }
        std::string held_contract;
        std::string derivative_request_contract;
        std::string derivative_result_contract;
        if (form != nullptr && derivative != nullptr) {
            const auto* form_entry =
                context.callsite_entries.at(form->callsite_id);
            const auto* derivative_entry =
                context.callsite_entries.at(derivative->callsite_id);
            held_contract = form_entry->result_contract_id;
            derivative_request_contract =
                derivative_entry->request_contract_id;
            derivative_result_contract =
                derivative_entry->result_contract_id;
            valid = valid && !held_contract.empty() &&
                    !derivative_request_contract.empty() &&
                    !derivative_result_contract.empty() &&
                    derivative_entry->state_read ==
                        gnc::model_sdk::StaticStateReadKind::Candidate &&
                    derivative_entry->state_write ==
                        gnc::model_sdk::StaticStateWriteKind::None;
        }
        std::set<std::string> closure_invocation_ids;
        valid = valid && source.closure_invocation_ids.size() == 1U;
        for (const auto& invocation_id :
             source.closure_invocation_ids) {
            const bool unique_invocation_id =
                closure_invocation_ids.insert(invocation_id).second;
            const auto invocation =
                context.invocation_indices.find(invocation_id);
            const auto* invocation_plan =
                invocation == context.invocation_indices.end()
                    ? nullptr
                    : &context.plan.invocation_bindings[invocation->second];
            const auto* closure_provider =
                invocation_plan == nullptr
                    ? nullptr
                    : find_occurrence(context,
                                      invocation_plan->provider_occurrence_id);
            const auto closure_index =
                invocation_plan == nullptr
                    ? context.closure_indices.end()
                    : context.closure_indices.find(
                          invocation_plan->provider_occurrence_id);
            const auto* closure_plan =
                closure_index == context.closure_indices.end()
                    ? nullptr
                    : &context.plan.closures[closure_index->second];
            const bool closure_output_matches =
                closure_provider != nullptr &&
                std::any_of(
                    closure_provider->descriptor.ports.begin(),
                    closure_provider->descriptor.ports.end(),
                    [&](const auto& port) {
                        return port.direction ==
                                   gnc::model_sdk::StaticPortDirection::Output &&
                               port.binding_kind ==
                                   gnc::model_sdk::BindingKind::ContinuousClosureLink &&
                               port.contract_id == held_contract;
                    });
            if (!unique_invocation_id || invocation_plan == nullptr ||
                form == nullptr ||
                invocation_plan->kind !=
                    gnc::model_sdk::StaticInvocationKind::Closure ||
                invocation_plan->caller_callsite_id != form->callsite_id ||
                closure_provider == nullptr ||
                closure_plan == nullptr ||
                closure_plan->strategy !=
                    gnc::contracts::ClosureStrategy::FrozenInterval ||
                invocation_plan->provider_plan_id !=
                    closure_plan->closure_plan_id ||
                !closure_output_matches) {
                valid = false;
            }
        }
        if (!valid || closure_invocation_ids.size() != 1U) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidIntegrationScope,
                       source.source, source.integration_scope_id,
                       "integration scope does not close one continuous owner, projection, form, derivative, and exactly one unique FrozenInterval closure invocation");
            continue;
        }
        ++owner_counts[source.owner_occurrence_id];
        const auto held_slot =
            "slot/held-form/" + source.integration_scope_id;
        context.plan.slots.push_back(
            {held_slot, held_slot, CompleteSlotKind::HeldIntervalValue,
             source.form_occurrence_id, {}, held_contract, source.source});
        derivative->input_slot_ids.push_back(held_slot);
        std::sort(derivative->input_slot_ids.begin(),
                  derivative->input_slot_ids.end());
        context.integration_producers[source.owner_occurrence_id] =
            source.integration_scope_id;
        context.plan.integration_scopes.push_back(
            {"integration-scope/" + source.integration_scope_id,
             source.integration_scope_id, source.scope,
             source.owner_occurrence_id, state->committed_slot_id,
             state->candidate_slot_id, projection->callsite_id,
             form->callsite_id, derivative->callsite_id, held_slot,
             held_contract, derivative_request_contract,
             derivative_result_contract,
             source.closure_invocation_ids, source.source});
    }
    for (const auto& state : context.plan.state_blocks) {
        if (state.evolution ==
                gnc::model_sdk::StaticStateEvolution::ContinuousCandidate &&
            owner_counts[state.owner_occurrence_id] != 1U) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidIntegrationScope,
                       state.source, state.owner_occurrence_id,
                       "continuous state owner must belong to exactly one integration scope");
        }
    }
}

inline void lower_transactions(LoweringContext& context) {
    std::set<std::string> ids;
    std::map<std::string, std::size_t> owner_counts;
    for (const auto& source : context.ir.transactions) {
        if (source.transaction_id.empty() ||
            !ids.insert(source.transaction_id).second ||
            !valid_source_ref(source.source) ||
            source.owner_occurrence_ids.empty()) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidTransaction,
                       source.source, source.transaction_id,
                       "transaction identity/source and owner/writer cardinality are invalid");
            continue;
        }
        std::set<std::string> owners;
        std::vector<std::string> candidates;
        std::vector<CandidateProducerPlan> producers;
        bool valid = true;
        for (const auto& owner_id : source.owner_occurrence_ids) {
            const auto* owner = find_occurrence(context, owner_id);
            const auto* state = find_state(context, owner_id);
            if (owner == nullptr || state == nullptr ||
                !same_scope(source.scope, *owner) ||
                !owners.insert(owner_id).second) {
                valid = false;
                continue;
            }
            candidates.push_back(state->candidate_slot_id);
            if (state->evolution == gnc::model_sdk::StaticStateEvolution::
                                        ContinuousCandidate) {
                const auto producer = context.integration_producers.find(owner_id);
                if (producer == context.integration_producers.end()) {
                    valid = false;
                } else {
                    producers.push_back(
                        {CandidateProducerKind::IntegrationScope,
                         producer->second});
                }
            } else {
                const auto producer =
                    context.state_writer_callsites.find(owner_id);
                if (producer == context.state_writer_callsites.end()) {
                    valid = false;
                } else {
                    producers.push_back(
                        {CandidateProducerKind::RuntimeCallsite,
                         producer->second});
                }
            }
            ++owner_counts[owner_id];
        }
        if (!valid || owners.size() != producers.size()) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidTransaction,
                       source.source, source.transaction_id,
                       "transaction must bind each scoped state owner to its unique candidate writer exactly once");
            continue;
        }
        std::sort(candidates.begin(), candidates.end());
        context.plan.transactions.push_back(
            {"transaction/" + source.transaction_id,
             source.transaction_id, source.scope,
             source.owner_occurrence_ids, candidates,
             producers, source.source});
    }
    for (const auto& state : context.plan.state_blocks) {
        if (owner_counts[state.owner_occurrence_id] != 1U) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidTransaction,
                       state.source, state.owner_occurrence_id,
                       "every state owner must belong to exactly one atomic transaction");
        }
    }
}

inline void lower_evaluator_histories(LoweringContext& context) {
    std::set<std::string> ids;
    std::map<std::string, std::size_t> evaluator_counts;
    for (const auto& source : context.ir.evaluator_histories) {
        const auto callsite_ids = callsites_for_obligation(
            context, source.evaluator_occurrence_id,
            gnc::contracts::ExecutionObligation::BoundaryEvaluation);
        auto* callsite = callsite_ids.size() == 1U
                             ? find_callsite(context,
                                             callsite_ids.front())
                             : nullptr;
        bool valid = !source.history_id.empty() &&
                     ids.insert(source.history_id).second &&
                     valid_source_ref(source.source) &&
                     callsite != nullptr;
        const CompleteCanonicalOccurrence* evaluator = nullptr;
        const gnc::model_sdk::StaticRuntimeObligationEntryDescriptor*
            evaluator_entry = nullptr;
        const gnc::model_sdk::StaticEvaluatorHistoryShapeDescriptor*
            history_shape = nullptr;
        if (callsite != nullptr) {
            evaluator = find_occurrence(context, callsite->occurrence_id);
            valid = valid && evaluator != nullptr &&
                    evaluator->descriptor.runtime_component.has_value() &&
                    evaluator->descriptor.runtime_component->profile ==
                        gnc::model_sdk::RuntimeCellProfile::Evaluator &&
                    evaluator->descriptor.runtime_component->schedule.trigger ==
                        gnc::model_sdk::StaticScheduleTrigger::TerminalSequenceReady &&
                    callsite->obligation ==
                        gnc::contracts::ExecutionObligation::BoundaryEvaluation;
            evaluator_entry =
                context.callsite_entries.at(callsite->callsite_id);
            if (evaluator != nullptr &&
                evaluator->descriptor.runtime_component.has_value() &&
                evaluator->descriptor.runtime_component
                    ->evaluator_history_shape.has_value()) {
                history_shape = &*evaluator->descriptor.runtime_component
                                      ->evaluator_history_shape;
            }
            const auto* history_input_port =
                evaluator_entry->input_port_ids.size() == 1U &&
                        evaluator != nullptr
                    ? find_port(evaluator->descriptor,
                                evaluator_entry->input_port_ids.front())
                    : nullptr;
            valid = valid &&
                    history_shape != nullptr &&
                    history_shape->depth > 0U &&
                    source.committed_history_depth ==
                        history_shape->depth &&
                    source.owner_occurrence_ids.size() ==
                        history_shape->ordered_members.size() &&
                    !history_shape->ordered_members.empty() &&
                    evaluator_entry->request_contract_id ==
                        history_shape->request_contract_id &&
                    history_input_port != nullptr &&
                    history_input_port->direction ==
                        gnc::model_sdk::StaticPortDirection::Input &&
                    history_input_port->contract_id ==
                        history_shape->request_contract_id &&
                    evaluator_entry->state_read ==
                        gnc::model_sdk::StaticStateReadKind::None &&
                    evaluator_entry->state_write ==
                        gnc::model_sdk::StaticStateWriteKind::None &&
                    callsite->input_slot_ids.empty();
        }
        const auto available_boundaries =
            static_cast<std::uint64_t>(
                context.ir.clock.terminal_tick -
                context.ir.clock.initial_tick) +
            1U;
        valid = valid && source.committed_history_depth <=
                             available_boundaries;
        std::vector<std::string> committed;
        std::vector<EvaluatorCommittedHistoryMemberPlan> members;
        std::set<std::string> owners;
        std::set<std::string> member_ids;
        for (std::size_t index = 0U;
             index < source.owner_occurrence_ids.size(); ++index) {
            const auto& owner_id = source.owner_occurrence_ids[index];
            const auto* state = find_state(context, owner_id);
            const auto* member =
                history_shape != nullptr &&
                        index < history_shape->ordered_members.size()
                    ? &history_shape->ordered_members[index]
                    : nullptr;
            if (state == nullptr || member == nullptr ||
                member->member_id.empty() ||
                !member_ids.insert(member->member_id).second ||
                !owners.insert(owner_id).second ||
                state->schema_id != member->state_schema_id ||
                state->layout_id != member->state_layout_id) {
                valid = false;
            } else {
                committed.push_back(state->committed_slot_id);
                members.push_back(
                    {member->member_id, owner_id, state->schema_id,
                     state->layout_id, state->committed_slot_id});
            }
        }
        if (!valid || committed.empty() || history_shape == nullptr) {
            diagnostic(context.diagnostics,
                       CompleteDiagnosticCode::InvalidEvaluatorHistory,
                       source.source, source.history_id,
                       "evaluator history must exactly match the package callable request, depth, ordered state schema/layout members, and committed-only callsite");
            continue;
        }
        callsite->input_slot_ids.insert(callsite->input_slot_ids.end(),
                                        committed.begin(), committed.end());
        ++evaluator_counts[callsite->callsite_id];
        context.plan.evaluator_histories.push_back(
            {"evaluator-history/" + source.history_id, source.history_id,
             callsite->callsite_id,
             history_shape->request_contract_id,
             source.committed_history_depth, std::move(members),
             source.source});
    }
    for (const auto& component : context.plan.runtime_components) {
        if (component.profile !=
            gnc::model_sdk::RuntimeCellProfile::Evaluator) {
            continue;
        }
        for (const auto& callsite : component.callsite_ids) {
            if (evaluator_counts[callsite] != 1U) {
                diagnostic(context.diagnostics,
                           CompleteDiagnosticCode::InvalidEvaluatorHistory,
                           component.source, callsite,
                           "every evaluator callsite must declare exactly one committed-history plan");
            }
        }
    }
}

[[nodiscard]] inline std::vector<std::pair<std::string, SourceRef>>
all_plan_elements(const CompleteExecutionPlanDescriptor& plan) {
    std::vector<std::pair<std::string, SourceRef>> result;
    result.emplace_back(plan.clock.plan_element_id, plan.clock.source);
    const auto append = [&](const auto& values) {
        for (const auto& value : values) {
            result.emplace_back(value.plan_element_id, value.source);
        }
    };
    append(plan.occurrences);
    append(plan.ports);
    append(plan.slots);
    append(plan.bindings);
    append(plan.preparation_inputs);
    append(plan.queries);
    append(plan.closures);
    append(plan.state_blocks);
    append(plan.initial_states);
    append(plan.projections);
    append(plan.runtime_components);
    append(plan.runtime_callsites);
    append(plan.invocation_bindings);
    append(plan.regions);
    append(plan.boundary_dag_nodes);
    append(plan.boundary_dag);
    append(plan.integration_scopes);
    append(plan.transactions);
    append(plan.evaluator_histories);
    append(plan.entry_requirements);
    std::sort(result.begin(), result.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.first < rhs.first;
              });
    return result;
}

[[nodiscard]] inline PlanProofKind proof_kind_for(
    std::string_view element) noexcept {
    if (element.rfind("occurrence/", 0U) == 0U) {
        return PlanProofKind::ExactCatalogResolution;
    }
    if (element.rfind("port/", 0U) == 0U ||
        element.rfind("slot/", 0U) == 0U) {
        return PlanProofKind::PortAndSlotDerivation;
    }
    if (element.rfind("binding/", 0U) == 0U) {
        return PlanProofKind::RequiredProviderCardinality;
    }
    if (element.rfind("query-plan/", 0U) == 0U ||
        element.rfind("closure-plan/", 0U) == 0U ||
        element.rfind("invocation/", 0U) == 0U) {
        return PlanProofKind::InvocationAuthorization;
    }
    if (element.rfind("state-block/", 0U) == 0U) {
        return PlanProofKind::UniqueStateOwner;
    }
    if (element.rfind("region/", 0U) == 0U ||
        element.rfind("dag-node/", 0U) == 0U ||
        element.rfind("callsite/", 0U) == 0U ||
        element.rfind("runtime-component/", 0U) == 0U) {
        return PlanProofKind::RegionMembership;
    }
    if (element.rfind("dag-edge/", 0U) == 0U) {
        return PlanProofKind::PhaseOrder;
    }
    if (element.rfind("integration-scope/", 0U) == 0U) {
        return PlanProofKind::IntegrationScopeComplete;
    }
    if (element.rfind("transaction/", 0U) == 0U) {
        return PlanProofKind::TransactionComplete;
    }
    if (element.rfind("evaluator-history/", 0U) == 0U) {
        return PlanProofKind::EvaluatorCommittedOnly;
    }
    if (element.rfind("link-requirement/", 0U) == 0U ||
        element.rfind("clock/", 0U) == 0U ||
        element.rfind("preparation-input/", 0U) == 0U ||
        element.rfind("initial-state/", 0U) == 0U ||
        element.rfind("projection/", 0U) == 0U) {
        return PlanProofKind::ExactEntryRequirement;
    }
    return PlanProofKind::ExactCatalogResolution;
}

[[nodiscard]] inline std::string proof_hash(const PlanProofIndex& proofs) {
    semantic_hash_detail::Encoder encoder;
    encoder.string(kPlanProofIndexIdentity);
    encoder.uint32(proofs.revision);
    encoder.string(proofs.plan_id);
    encoder.collection(proofs.records.size());
    for (const auto& record : proofs.records) {
        encoder.string(record.proof_id);
        encoder.uint32(static_cast<std::uint32_t>(record.kind));
        encoder.string(record.subject);
        encoder.collection(record.premises.size());
        for (const auto& premise : record.premises) {
            encoder.string(premise);
        }
        encoder.collection(record.covered_plan_elements.size());
        for (const auto& element : record.covered_plan_elements) {
            encoder.string(element);
        }
    }
    encoder.collection(proofs.coverage.size());
    for (const auto& item : proofs.coverage) {
        encoder.string(item.plan_element_id);
        encoder.collection(item.proof_ids.size());
        for (const auto& proof_id : item.proof_ids) {
            encoder.string(proof_id);
        }
    }
    return hash_bytes(encoder);
}

[[nodiscard]] inline PlanProofIndex derive_proofs(
    const CompleteExecutionPlanDescriptor& plan) {
    PlanProofIndex proofs;
    proofs.plan_id = plan.plan_id;

    std::map<std::string, std::vector<std::string>> coverage;
    const auto enum_value = [](const auto value) {
        return std::to_string(static_cast<std::uint32_t>(value));
    };
    const auto config_hash = [](const auto& config) {
        semantic_hash_detail::Encoder encoder;
        encode_config(encoder, config);
        return hash_bytes(encoder);
    };
    const auto append_ids = [](std::vector<std::string>& premises,
                               std::string_view label,
                               const std::vector<std::string>& ids) {
        premises.push_back(std::string(label) + "-count=" +
                           std::to_string(ids.size()));
        for (std::size_t index = 0U; index < ids.size(); ++index) {
            premises.push_back(std::string(label) + "." +
                               std::to_string(index) + "=" + ids[index]);
        }
    };
    const auto append_scope = [&](std::vector<std::string>& premises,
                                  std::string_view label,
                                  const CompleteOccurrencePlan& occurrence) {
        premises.push_back(std::string(label) + ".placement=" +
                           enum_value(occurrence.placement));
        premises.push_back(std::string(label) + ".scope-present=" +
                           (occurrence.scope.has_value() ? "true" : "false"));
        if (occurrence.scope.has_value()) {
            premises.push_back(std::string(label) + ".scope-kind=" +
                               enum_value(occurrence.scope->kind));
            premises.push_back(std::string(label) + ".scope-subject=" +
                               occurrence.scope->subject_entity_id);
        }
    };
    const auto add = [&](std::string proof_id, PlanProofKind kind,
                         std::string subject,
                         std::vector<std::string> premises,
                         std::vector<std::string> covered,
                         SourceRef source) {
        std::sort(premises.begin(), premises.end());
        std::sort(covered.begin(), covered.end());
        covered.erase(std::unique(covered.begin(), covered.end()),
                      covered.end());
        for (const auto& element : covered) {
            coverage[element].push_back(proof_id);
        }
        proofs.records.push_back(
            {std::move(proof_id), kind, std::move(subject),
             std::move(premises), std::move(covered), {std::move(source)}});
    };
    const auto find_occurrence_plan = [&](std::string_view occurrence_id)
        -> const CompleteOccurrencePlan* {
        const auto found = std::find_if(
            plan.occurrences.begin(), plan.occurrences.end(),
            [&](const auto& occurrence) {
                return occurrence.occurrence_id == occurrence_id;
            });
        return found == plan.occurrences.end() ? nullptr : &*found;
    };
    const auto find_port_plan = [&](std::string_view occurrence_id,
                                    std::string_view port_id)
        -> const CompletePortPlan* {
        const auto found = std::find_if(
            plan.ports.begin(), plan.ports.end(), [&](const auto& port) {
                return port.occurrence_id == occurrence_id &&
                       port.port_id == port_id;
            });
        return found == plan.ports.end() ? nullptr : &*found;
    };

    {
        semantic_hash_detail::Encoder clock_encoder;
        clock_encoder.string(plan.clock.clock_id);
        clock_encoder.float64(plan.clock.base_step_seconds);
        clock_encoder.integer(plan.clock.initial_tick);
        clock_encoder.integer(plan.clock.terminal_tick);
        add("proof/clock/grid", PlanProofKind::PhaseOrder,
            plan.clock.plan_element_id,
            {"clock-id=" + plan.clock.clock_id,
             "clock-grid-hash=" + hash_bytes(clock_encoder),
             "initial-tick=" + std::to_string(plan.clock.initial_tick),
             "terminal-tick=" + std::to_string(plan.clock.terminal_tick)},
            {plan.clock.plan_element_id}, plan.clock.source);
    }

    for (const auto& occurrence : plan.occurrences) {
        std::vector<std::string> catalog_premises{
            "package=" + occurrence.package.package_id,
            "package-version=" + occurrence.package.package_version,
            "model=" + occurrence.model_id,
            "model-version=" + occurrence.model_version,
            "execution-form=" + enum_value(occurrence.execution_form),
            "placement=" + enum_value(occurrence.placement),
            "subject=" + occurrence.subject_entity_id};
        append_scope(catalog_premises, "occurrence", occurrence);
        add("proof/catalog/" + occurrence.occurrence_id,
            PlanProofKind::ExactCatalogResolution,
            occurrence.plan_element_id, std::move(catalog_premises),
            {occurrence.plan_element_id}, occurrence.source);
        add("proof/config/" + occurrence.occurrence_id,
            PlanProofKind::CanonicalConfiguration,
            occurrence.plan_element_id,
            {"schema=" + occurrence.canonical_configuration.schema_id,
             "schema-version=" + std::to_string(
                 occurrence.canonical_configuration.schema_version),
             "field-count=" + std::to_string(
                 occurrence.canonical_configuration.fields.size()),
             "canonical-config-hash=" +
                 config_hash(occurrence.canonical_configuration)},
            {occurrence.plan_element_id}, occurrence.source);
        std::vector<std::string> asset_premises{
            "asset-count=" +
            std::to_string(occurrence.asset_bindings.size())};
        for (const auto& asset : occurrence.asset_bindings) {
            asset_premises.push_back("asset=" + asset.role + "|" +
                                     asset.asset_schema_id + "|" +
                                     asset.asset_id);
        }
        add("proof/assets/" + occurrence.occurrence_id,
            PlanProofKind::ExactAssetSelection,
            occurrence.plan_element_id, std::move(asset_premises),
            {occurrence.plan_element_id}, occurrence.source);
    }

    for (const auto& port : plan.ports) {
        add("proof/port/" + port.occurrence_id + "/" + port.port_id,
            PlanProofKind::PortAndSlotDerivation, port.plan_element_id,
            {"occurrence=" + port.occurrence_id,
             "port=" + port.port_id,
             "contract=" + port.contract_id,
             "direction=" + enum_value(port.direction),
             "binding-kind=" + enum_value(port.binding_kind),
             "cardinality=" + enum_value(port.cardinality),
             "temporal=" + enum_value(port.temporal_relation)},
            {port.plan_element_id}, port.source);
    }
    for (const auto& slot : plan.slots) {
        add("proof/slot/" + slot.slot_id,
            PlanProofKind::PortAndSlotDerivation, slot.plan_element_id,
            {"slot=" + slot.slot_id,
             "slot-kind=" + enum_value(slot.kind),
             "owner=" + slot.owner_occurrence_id,
             "port=" + slot.port_id,
             "contract-or-layout=" + slot.contract_or_layout_id},
            {slot.plan_element_id}, slot.source);
    }

    for (const auto& binding : plan.bindings) {
        const auto* provider_occurrence =
            find_occurrence_plan(binding.provider_occurrence_id);
        const auto* consumer_occurrence =
            find_occurrence_plan(binding.consumer_occurrence_id);
        const auto* provider_port = find_port_plan(
            binding.provider_occurrence_id, binding.provider_port_id);
        const auto* consumer_port = find_port_plan(
            binding.consumer_occurrence_id, binding.consumer_port_id);
        const auto provider_count = static_cast<std::size_t>(std::count_if(
            plan.bindings.begin(), plan.bindings.end(),
            [&](const auto& candidate) {
                return candidate.consumer_occurrence_id ==
                           binding.consumer_occurrence_id &&
                       candidate.consumer_port_id == binding.consumer_port_id;
            }));
        add("proof/provider/" + binding.binding_id,
            PlanProofKind::RequiredProviderCardinality,
            binding.plan_element_id,
            {"provider=" + binding.provider_occurrence_id + "." +
                 binding.provider_port_id,
             "provider-slot=" + binding.provider_slot_id,
             "consumer=" + binding.consumer_occurrence_id + "." +
                 binding.consumer_port_id,
             "contract=" + binding.contract_id,
             "declared-cardinality=" +
                 enum_value(consumer_port == nullptr
                                ? gnc::model_sdk::PortCardinality::Unspecified
                                : consumer_port->cardinality),
             "matching-provider-count=" + std::to_string(provider_count)},
            {binding.plan_element_id}, binding.source);
        std::vector<std::string> scope_premises{
            "compatible=true",
            "provider=" + binding.provider_occurrence_id,
            "consumer=" + binding.consumer_occurrence_id};
        if (provider_occurrence != nullptr) {
            append_scope(scope_premises, "provider", *provider_occurrence);
        }
        if (consumer_occurrence != nullptr) {
            append_scope(scope_premises, "consumer", *consumer_occurrence);
        }
        add("proof/scope/" + binding.binding_id,
            PlanProofKind::ScopeCompatibility, binding.plan_element_id,
            std::move(scope_premises), {binding.plan_element_id},
            binding.source);
        add("proof/temporal/" + binding.binding_id,
            PlanProofKind::TemporalCompatibility,
            binding.plan_element_id,
            {"contract=" + binding.contract_id,
             "binding-kind=" + enum_value(binding.binding_kind),
             "binding-temporal=" + enum_value(binding.temporal_relation),
             "provider-temporal=" +
                 enum_value(provider_port == nullptr
                                ? gnc::model_sdk::TemporalRelation::NotApplicable
                                : provider_port->temporal_relation),
             "consumer-temporal=" +
                 enum_value(consumer_port == nullptr
                                ? gnc::model_sdk::TemporalRelation::NotApplicable
                                : consumer_port->temporal_relation)},
            {binding.plan_element_id}, binding.source);
    }

    for (const auto& preparation : plan.preparation_inputs) {
        std::vector<std::string> premises{
            "occurrence=" + preparation.occurrence_id,
            "model=" + preparation.model_id,
            "model-version=" + preparation.model_version,
            "prepare-entry-requirement=" +
                preparation.preparation_entry_requirement_id,
            "canonical-config-hash=" +
                config_hash(preparation.canonical_configuration),
            "asset-count=" +
                std::to_string(preparation.asset_bindings.size())};
        for (const auto& asset : preparation.asset_bindings) {
            premises.push_back("asset=" + asset.role + "|" +
                               asset.asset_schema_id + "|" + asset.asset_id);
        }
        add("proof/preparation/" + preparation.preparation_input_id,
            PlanProofKind::ExactEntryRequirement,
            preparation.plan_element_id, std::move(premises),
            {preparation.plan_element_id}, preparation.source);
    }
    for (const auto& query : plan.queries) {
        std::vector<std::string> premises{
            "occurrence=" + query.occurrence_id,
            "preparation=" + query.preparation_input_ref,
            "entry-requirement=" + query.entry_requirement_id,
            "workspace=" + enum_value(query.workspace_requirement)};
        append_ids(premises, "authorized-invocation",
                   query.authorized_invocation_ids);
        add("proof/query-auth/" + query.query_plan_id,
            PlanProofKind::InvocationAuthorization, query.plan_element_id,
            std::move(premises), {query.plan_element_id}, query.source);
    }
    for (const auto& closure : plan.closures) {
        std::vector<std::string> premises{
            "occurrence=" + closure.occurrence_id,
            "preparation=" + closure.preparation_input_ref,
            "entry-requirement=" + closure.entry_requirement_id,
            "closure-strategy=" + enum_value(closure.strategy),
            "workspace=" + enum_value(closure.workspace_requirement)};
        append_ids(premises, "authorized-invocation",
                   closure.authorized_invocation_ids);
        add("proof/closure-auth/" + closure.closure_plan_id,
            PlanProofKind::InvocationAuthorization, closure.plan_element_id,
            std::move(premises), {closure.plan_element_id}, closure.source);
    }

    for (const auto& state : plan.state_blocks) {
        const auto* owner = find_occurrence_plan(state.owner_occurrence_id);
        const auto matching_owner_count =
            static_cast<std::size_t>(std::count_if(
                plan.state_blocks.begin(), plan.state_blocks.end(),
                [&](const auto& candidate) {
                    const auto* candidate_owner =
                        find_occurrence_plan(candidate.owner_occurrence_id);
                    return candidate.schema_id == state.schema_id &&
                           owner != nullptr && candidate_owner != nullptr &&
                           scope_equal(owner->scope, candidate_owner->scope);
                }));
        add("proof/state-owner/" + state.owner_occurrence_id,
            PlanProofKind::UniqueStateOwner, state.plan_element_id,
            {"owner=" + state.owner_occurrence_id,
             "schema=" + state.schema_id,
             "schema-version=" + std::to_string(state.schema_version),
             "layout=" + state.layout_id,
             "evolution=" + enum_value(state.evolution),
             "committed-slot=" + state.committed_slot_id,
             "candidate-slot=" + state.candidate_slot_id,
             "matching-scoped-owner-count=" +
                 std::to_string(matching_owner_count)},
            {state.plan_element_id}, state.source);
    }
    for (const auto& initial : plan.initial_states) {
        add("proof/initial/" + initial.owner_occurrence_id,
            PlanProofKind::ExactEntryRequirement,
            initial.plan_element_id,
            {"owner=" + initial.owner_occurrence_id,
             "committed-slot=" + initial.committed_slot_id,
             "builder-entry-requirement=" +
                 initial.builder_entry_requirement_id,
             "builder-schema=" + initial.builder_inputs.schema_id,
             "builder-schema-version=" +
                 std::to_string(initial.builder_inputs.schema_version),
             "builder-input-hash=" + config_hash(initial.builder_inputs)},
            {initial.plan_element_id}, initial.source);
    }
    for (const auto& projection : plan.projections) {
        std::vector<std::string> premises{
            "owner=" + projection.owner_occurrence_id,
            "callsite=" + projection.callsite_id,
            "committed-slot=" + projection.committed_slot_id};
        append_ids(premises, "output-slot", projection.output_slot_ids);
        add("proof/projection/" + projection.owner_occurrence_id,
            PlanProofKind::ExactEntryRequirement,
            projection.plan_element_id, std::move(premises),
            {projection.plan_element_id}, projection.source);
    }

    for (const auto& component : plan.runtime_components) {
        std::vector<std::string> premises{
            "occurrence=" + component.occurrence_id,
            "definition-builder-entry-requirement=" +
                component.definition_builder_entry_requirement_id,
            "recipe=" + component.recipe_id,
            "profile=" + enum_value(component.profile),
            "schedule-trigger=" + enum_value(component.schedule.trigger),
            "schedule-step=" +
                std::to_string(component.schedule.step_interval),
            "schedule-offset=" + std::to_string(component.schedule.offset),
            "schedule-hold=" + enum_value(component.schedule.output_hold),
            "schedule-max-age=" +
                std::to_string(component.schedule.max_input_age_steps)};
        premises.push_back(
            "lifecycle-count=" +
            std::to_string(component.lifecycle_capabilities.size()));
        for (std::size_t index = 0U;
             index < component.lifecycle_capabilities.size(); ++index) {
            premises.push_back(
                "lifecycle." + std::to_string(index) + "=" +
                enum_value(component.lifecycle_capabilities[index]));
        }
        append_ids(premises, "callsite", component.callsite_ids);
        add("proof/runtime-component/" + component.occurrence_id,
            PlanProofKind::RegionMembership,
            component.plan_element_id, std::move(premises),
            {component.plan_element_id}, component.source);
    }
    for (const auto& callsite : plan.runtime_callsites) {
        std::vector<std::string> premises{
            "callsite=" + callsite.callsite_id,
            "occurrence=" + callsite.occurrence_id,
            "obligation=" + enum_value(callsite.obligation),
            "phase=" + enum_value(callsite.phase),
            "phase-rank=" + std::to_string(phase_rank(callsite.phase)),
            "entry-requirement=" + callsite.entry_requirement_id,
            "request-contract=" + callsite.request_contract_id,
            "result-contract=" + callsite.result_contract_id,
            "region=" + callsite.region_id};
        append_ids(premises, "input-slot", callsite.input_slot_ids);
        append_ids(premises, "output-slot", callsite.output_slot_ids);
        append_ids(premises, "authorized-invocation",
                   callsite.invocation_binding_ids);
        add("proof/callsite/" + callsite.callsite_id,
            PlanProofKind::ExactEntryRequirement,
            callsite.plan_element_id, std::move(premises),
            {callsite.plan_element_id}, callsite.source);
    }
    for (const auto& invocation : plan.invocation_bindings) {
        add("proof/invocation/" + invocation.invocation_id,
            PlanProofKind::InvocationAuthorization,
            invocation.plan_element_id,
            {"caller-callsite=" + invocation.caller_callsite_id,
             "requirement=" + invocation.requirement_id,
             "kind=" + enum_value(invocation.kind),
             "request-contract=" + invocation.contract_id,
             "provider-occurrence=" + invocation.provider_occurrence_id,
             "provider-plan=" + invocation.provider_plan_id,
             "result-binding=" + invocation.result_binding_id,
             "provider-result-slot=" +
                 invocation.provider_result_slot_id,
             "consumer-port=" + invocation.consumer_port_id,
             "result-flow-cardinality=1",
             "authorization-cardinality=1"},
            {invocation.plan_element_id}, invocation.source);
    }
    for (const auto& region : plan.regions) {
        std::vector<std::string> premises{
            "region=" + region.region_id,
            "ordinal=" + std::to_string(region.ordinal),
            "phase=" + enum_value(region.phase),
            "phase-rank=" + std::to_string(phase_rank(region.phase))};
        append_ids(premises, "member-callsite", region.callsite_ids);
        add("proof/region/" + region.region_id,
            PlanProofKind::RegionMembership, region.plan_element_id,
            std::move(premises), {region.plan_element_id}, region.source);
    }

    std::map<std::string, std::uint32_t> dag_ranks;
    std::map<std::string, std::size_t> dag_indegree;
    std::map<std::string, std::vector<std::string>> dag_successors;
    std::vector<std::string> dag_covered;
    std::vector<std::string> dag_premises{
        "node-count=" + std::to_string(plan.boundary_dag_nodes.size()),
        "edge-count=" + std::to_string(plan.boundary_dag.size())};
    for (const auto& node : plan.boundary_dag_nodes) {
        dag_ranks.emplace(node.node_id, phase_rank(node.phase));
        dag_indegree.emplace(node.node_id, 0U);
        dag_covered.push_back(node.plan_element_id);
        dag_premises.push_back("node=" + node.node_id + "|" +
                               enum_value(node.kind) + "|" + node.target_id +
                               "|" + enum_value(node.phase));
    }
    for (const auto& edge : plan.boundary_dag) {
        dag_successors[edge.predecessor_node_id].push_back(
            edge.successor_node_id);
        ++dag_indegree[edge.successor_node_id];
        dag_covered.push_back(edge.plan_element_id);
        dag_premises.push_back("edge=" + edge.predecessor_node_id + "->" +
                               edge.successor_node_id);
        add("proof/phase-edge/" + edge.edge_id,
            PlanProofKind::PhaseOrder, edge.plan_element_id,
            {"predecessor=" + edge.predecessor_node_id,
             "successor=" + edge.successor_node_id,
             "predecessor-rank=" +
                 std::to_string(dag_ranks[edge.predecessor_node_id]),
             "successor-rank=" +
                 std::to_string(dag_ranks[edge.successor_node_id]),
             "nonfuture=true"},
            {edge.plan_element_id}, edge.source);
    }
    std::set<std::string> ready;
    for (const auto& [node, indegree] : dag_indegree) {
        if (indegree == 0U) {
            ready.insert(node);
        }
    }
    std::vector<std::string> topological_order;
    while (!ready.empty()) {
        const auto node = *ready.begin();
        ready.erase(ready.begin());
        topological_order.push_back(node);
        for (const auto& successor : dag_successors[node]) {
            auto& indegree = dag_indegree[successor];
            --indegree;
            if (indegree == 0U) {
                ready.insert(successor);
            }
        }
    }
    append_ids(dag_premises, "topological-node", topological_order);
    dag_premises.push_back(
        "acyclic=" + std::string(topological_order.size() ==
                                         plan.boundary_dag_nodes.size()
                                     ? "true"
                                     : "false"));
    if (!dag_covered.empty()) {
        add("proof/dag/acyclic", PlanProofKind::DagAcyclic,
            "boundary-dag", std::move(dag_premises),
            std::move(dag_covered), plan.clock.source);
    }

    for (const auto& scope : plan.integration_scopes) {
        std::vector<std::string> premises{
            "owner=" + scope.owner_occurrence_id,
            "scope-kind=" + enum_value(scope.scope.kind),
            "scope-subject=" + scope.scope.subject_entity_id,
            "committed-slot=" + scope.committed_state_slot_id,
            "candidate-slot=" + scope.candidate_state_slot_id,
            "projection-callsite=" + scope.projection_callsite_id,
            "form-callsite=" + scope.form_callsite_id,
            "derivative-callsite=" + scope.derivative_callsite_id,
            "held-form-slot=" + scope.held_form_slot_id,
            "held-form-contract=" + scope.held_form_contract_id,
            "derivative-request-contract=" +
                scope.derivative_request_contract_id,
            "derivative-result-contract=" +
                scope.derivative_result_contract_id,
            "candidate-producer=IntegrationScope"};
        append_ids(premises, "closure-invocation",
                   scope.closure_invocation_ids);
        add("proof/integration/" + scope.integration_scope_id,
            PlanProofKind::IntegrationScopeComplete,
            scope.plan_element_id, std::move(premises),
            {scope.plan_element_id}, scope.source);
    }
    for (const auto& transaction : plan.transactions) {
        std::vector<std::string> premises{
            "scope-kind=" + enum_value(transaction.scope.kind),
            "scope-subject=" + transaction.scope.subject_entity_id};
        append_ids(premises, "owner", transaction.owner_occurrence_ids);
        append_ids(premises, "candidate-slot",
                   transaction.candidate_state_slot_ids);
        for (std::size_t index = 0U;
             index < transaction.candidate_producers.size(); ++index) {
            premises.push_back(
                "candidate-producer." + std::to_string(index) + "=" +
                enum_value(transaction.candidate_producers[index].kind) +
                "|" + transaction.candidate_producers[index].producer_id);
        }
        premises.push_back(
            "candidate-producer-count=" +
            std::to_string(transaction.candidate_producers.size()));
        add("proof/transaction/" + transaction.transaction_id,
            PlanProofKind::TransactionComplete,
            transaction.plan_element_id, premises,
            {transaction.plan_element_id}, transaction.source);
        const auto writer_count = std::min(
            transaction.owner_occurrence_ids.size(),
            std::min(transaction.candidate_state_slot_ids.size(),
                     transaction.candidate_producers.size()));
        for (std::size_t index = 0U; index < writer_count; ++index) {
            const auto& producer = transaction.candidate_producers[index];
            std::vector<std::string> covered{transaction.plan_element_id};
            const auto state = std::find_if(
                plan.state_blocks.begin(), plan.state_blocks.end(),
                [&](const auto& candidate) {
                    return candidate.owner_occurrence_id ==
                           transaction.owner_occurrence_ids[index];
                });
            if (state != plan.state_blocks.end()) {
                covered.push_back(state->plan_element_id);
            }
            add("proof/candidate-writer/" +
                    transaction.owner_occurrence_ids[index],
                PlanProofKind::UniqueCandidateWriter,
                transaction.owner_occurrence_ids[index],
                {"owner=" + transaction.owner_occurrence_ids[index],
                 "candidate-slot=" +
                     transaction.candidate_state_slot_ids[index],
                 "producer-kind=" + enum_value(producer.kind),
                 "producer=" + producer.producer_id,
                 "writer-cardinality=1",
                 "transaction=" + transaction.transaction_id},
                std::move(covered), transaction.source);
        }
    }
    for (const auto& history : plan.evaluator_histories) {
        std::vector<std::string> premises{
            "evaluator-callsite=" + history.evaluator_callsite_id,
            "request-contract=" + history.request_contract_id,
            "history-depth=" +
                std::to_string(history.committed_history_depth),
            "ordered-member-count=" +
                std::to_string(history.ordered_members.size()),
            "candidate-slot-count=0"};
        for (std::size_t index = 0U;
             index < history.ordered_members.size(); ++index) {
            const auto& member = history.ordered_members[index];
            premises.push_back(
                "ordered-member." + std::to_string(index) + "=" +
                member.member_id + "|" + member.owner_occurrence_id + "|" +
                member.state_schema_id + "|" + member.state_layout_id +
                "|" + member.committed_state_slot_id);
        }
        add("proof/evaluator/" + history.history_id,
            PlanProofKind::EvaluatorCommittedOnly,
            history.plan_element_id, std::move(premises),
            {history.plan_element_id}, history.source);
    }
    for (const auto& requirement : plan.entry_requirements) {
        add("proof/link-requirement/" + requirement.requirement_id,
            PlanProofKind::ExactEntryRequirement,
            requirement.plan_element_id,
            {"package=" + requirement.package.package_id,
             "package-version=" + requirement.package.package_version,
             "entry=" + requirement.entry_id,
             "entry-version=" + requirement.entry_version,
             "entry-kind=" + enum_value(requirement.kind),
             "signature=" + requirement.signature_id,
             "call-shape=" + requirement.call_shape_id,
             "state-layout=" + requirement.state_layout_id,
             "workspace-layout=" + requirement.workspace_layout_id},
            {requirement.plan_element_id}, requirement.source);
    }

    std::map<std::string, std::vector<SourceRef>> additional_sources;
    for (const auto& occurrence : plan.occurrences) {
        auto& sources = additional_sources[occurrence.plan_element_id];
        sources.push_back(occurrence.source);
        sources.push_back(occurrence.subject_source);
        if (occurrence.scope.has_value()) {
            sources.push_back(occurrence.scope_source);
        }
        sources.push_back(occurrence.placement_source);
        sources.push_back(occurrence.configuration_source);
        for (const auto& field : occurrence.configuration_field_sources) {
            sources.push_back(field.source);
        }
        for (const auto& asset : occurrence.asset_bindings) {
            sources.push_back(asset.source);
        }
    }
    for (const auto& initial : plan.initial_states) {
        auto& sources = additional_sources[initial.plan_element_id];
        sources.push_back(initial.source);
        for (const auto& field : initial.field_sources) {
            sources.push_back(field.source);
        }
    }
    for (auto& record : proofs.records) {
        for (const auto& element : record.covered_plan_elements) {
            const auto found = additional_sources.find(element);
            if (found != additional_sources.end()) {
                record.source_refs.insert(record.source_refs.end(),
                                          found->second.begin(),
                                          found->second.end());
            }
        }
        record.source_refs.erase(
            std::remove_if(record.source_refs.begin(),
                           record.source_refs.end(),
                           [](const auto& source) {
                               return !valid_source_ref(source);
                           }),
            record.source_refs.end());
        std::sort(record.source_refs.begin(), record.source_refs.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return std::tie(lhs.document_uri, lhs.node_path) <
                             std::tie(rhs.document_uri, rhs.node_path);
                  });
        record.source_refs.erase(
            std::unique(record.source_refs.begin(),
                        record.source_refs.end(),
                        [](const auto& lhs, const auto& rhs) {
                            return lhs.document_uri == rhs.document_uri &&
                                   lhs.node_path == rhs.node_path;
                        }),
            record.source_refs.end());
    }

    std::sort(proofs.records.begin(), proofs.records.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.proof_id < rhs.proof_id;
              });
    for (auto& [element, proof_ids] : coverage) {
        std::sort(proof_ids.begin(), proof_ids.end());
        proof_ids.erase(std::unique(proof_ids.begin(), proof_ids.end()),
                        proof_ids.end());
        proofs.coverage.push_back({element, std::move(proof_ids)});
    }
    proofs.proof_index_hash = proof_hash(proofs);
    return proofs;
}

[[nodiscard]] inline std::string descriptor_hash(
    const CompleteExecutionPlanDescriptor& plan) {
    semantic_hash_detail::Encoder encoder;
    encoder.string(kCompleteExecutionPlanDescriptorIdentity);
    encoder.uint32(plan.revision);
    encoder.string(plan.mission_id);
    encoder.string(plan.plan_id);
    encoder.string(plan.source_semantic_hash);
    encoder.string(plan.clock.clock_id);
    encoder.float64(plan.clock.base_step_seconds);
    encoder.integer(plan.clock.initial_tick);
    encoder.integer(plan.clock.terminal_tick);
    const auto elements = all_plan_elements(plan);
    encoder.collection(elements.size());
    for (const auto& [element, source] : elements) {
        static_cast<void>(source);
        encoder.string(element);
    }
    encoder.collection(plan.dependency_lock.size());
    for (const auto& package : plan.dependency_lock) {
        encoder.string(package.package_id);
        encoder.string(package.package_version);
    }
    encoder.collection(plan.occurrences.size());
    for (const auto& occurrence : plan.occurrences) {
        encoder.string(occurrence.occurrence_id);
        encoder.string(occurrence.package.package_id);
        encoder.string(occurrence.package.package_version);
        encoder.string(occurrence.model_id);
        encoder.string(occurrence.model_version);
        encode_config(encoder, occurrence.canonical_configuration);
        encoder.collection(occurrence.asset_bindings.size());
        for (const auto& asset : occurrence.asset_bindings) {
            encoder.string(asset.role);
            encoder.string(asset.asset_schema_id);
            encoder.string(asset.asset_id);
        }
    }
    encoder.collection(plan.initial_states.size());
    for (const auto& initial : plan.initial_states) {
        encoder.string(initial.owner_occurrence_id);
        encoder.string(initial.committed_slot_id);
        encoder.string(initial.builder_entry_requirement_id);
        encode_config(encoder, initial.builder_inputs);
    }
    encoder.collection(plan.entry_requirements.size());
    for (const auto& requirement : plan.entry_requirements) {
        encoder.string(requirement.requirement_id);
        encoder.string(requirement.package.package_id);
        encoder.string(requirement.package.package_version);
        encoder.string(requirement.entry_id);
        encoder.string(requirement.entry_version);
        encoder.uint32(static_cast<std::uint32_t>(requirement.kind));
        encoder.string(requirement.signature_id);
        encoder.string(requirement.call_shape_id);
        encoder.string(requirement.state_layout_id);
        encoder.string(requirement.workspace_layout_id);
    }
    encoder.collection(plan.runtime_callsites.size());
    for (const auto& callsite : plan.runtime_callsites) {
        encoder.string(callsite.callsite_id);
        encoder.string(callsite.occurrence_id);
        encoder.uint32(static_cast<std::uint32_t>(callsite.obligation));
        encoder.uint32(static_cast<std::uint32_t>(callsite.phase));
        encoder.string(callsite.entry_requirement_id);
        encoder.string(callsite.request_contract_id);
        encoder.string(callsite.result_contract_id);
        encoder.string(callsite.region_id);
        encoder.collection(callsite.input_slot_ids.size());
        for (const auto& id : callsite.input_slot_ids) {
            encoder.string(id);
        }
        encoder.collection(callsite.output_slot_ids.size());
        for (const auto& id : callsite.output_slot_ids) {
            encoder.string(id);
        }
        encoder.collection(callsite.invocation_binding_ids.size());
        for (const auto& id : callsite.invocation_binding_ids) {
            encoder.string(id);
        }
    }
    encoder.collection(plan.boundary_dag_nodes.size());
    for (const auto& node : plan.boundary_dag_nodes) {
        encoder.string(node.node_id);
        encoder.uint32(static_cast<std::uint32_t>(node.kind));
        encoder.string(node.target_id);
        encoder.uint32(static_cast<std::uint32_t>(node.phase));
    }
    encoder.collection(plan.boundary_dag.size());
    for (const auto& edge : plan.boundary_dag) {
        encoder.string(edge.edge_id);
        encoder.string(edge.predecessor_node_id);
        encoder.string(edge.successor_node_id);
    }
    encoder.collection(plan.transactions.size());
    for (const auto& transaction : plan.transactions) {
        encoder.string(transaction.transaction_id);
        encoder.collection(transaction.owner_occurrence_ids.size());
        for (const auto& id : transaction.owner_occurrence_ids) {
            encoder.string(id);
        }
        encoder.collection(transaction.candidate_producers.size());
        for (const auto& producer : transaction.candidate_producers) {
            encoder.uint32(static_cast<std::uint32_t>(producer.kind));
            encoder.string(producer.producer_id);
        }
    }
    // The proof derivation is a normalized semantic projection of every plan
    // relationship (ports/slots/bindings/auth/regions/DAG/state/transactions
    // and exact entry requirements). Including it closes descriptor hashing
    // over facts not repeated in the compact legacy loops above.
    encoder.string(derive_proofs(plan).proof_index_hash);
    return hash_bytes(encoder);
}

} // namespace complete_plan_detail

[[nodiscard]] inline CompleteOutcome<CompleteStaticCompilation>
compile_complete_execution_plan(
    const CompleteStaticCompositionSource& source,
    const std::vector<gnc::model_sdk::StaticPackageDescriptor>& packages) {
    using namespace complete_plan_detail;
    CompleteOutcome<CompleteStaticCompilation> outcome;
    auto ir_outcome = lower_complete_static_source(source, packages);
    if (!ir_outcome.succeeded()) {
        outcome.diagnostics = std::move(ir_outcome.diagnostics);
        return outcome;
    }
    LoweringContext context{*ir_outcome.value};
    context.plan.plan_id = context.ir.plan_id;
    context.plan.mission_id = context.ir.mission_id;
    context.plan.clock = {"clock/" + context.ir.clock.clock_id,
                          context.ir.clock.clock_id,
                          context.ir.clock.base_step_seconds,
                          context.ir.clock.initial_tick,
                          context.ir.clock.terminal_tick,
                          context.ir.clock.source};
    context.plan.source_semantic_hash = source_semantic_hash(context.ir);
    lower_occurrences(context);
    lower_bindings(context);
    lower_regions(context);
    lower_invocations(context);
    lower_integration_scopes(context);
    lower_dag(context);
    lower_transactions(context);
    lower_evaluator_histories(context);
    if (!context.diagnostics.empty()) {
        outcome.diagnostics = std::move(context.diagnostics);
        return outcome;
    }
    context.plan.descriptor_semantic_hash = descriptor_hash(context.plan);
    auto proofs = derive_proofs(context.plan);
    outcome.value = CompleteStaticCompilation{
        std::move(*ir_outcome.value), std::move(context.plan),
        std::move(proofs)};
    return outcome;
}

namespace complete_plan_detail {

[[nodiscard]] inline bool validate_proofs(
    const CompleteExecutionPlanDescriptor& plan, const PlanProofIndex& proofs,
    std::vector<CompleteDiagnostic>& diagnostics) {
    if (proofs.revision != 1U || proofs.identity != kPlanProofIndexIdentity ||
        proofs.plan_id != plan.plan_id ||
        proofs.proof_index_hash != proof_hash(proofs)) {
        diagnostic(diagnostics, CompleteDiagnosticCode::InvalidProofReference,
                   {}, plan.plan_id,
                   "proof index identity, ownership, or stable hash is invalid");
        return false;
    }
    const auto expected = derive_proofs(plan);
    if (proofs.proof_index_hash != expected.proof_index_hash) {
        diagnostic(
            diagnostics, CompleteDiagnosticCode::InvalidProofReference, {},
            plan.plan_id,
            "proof records/coverage do not match the exact validated plan relationships");
        return false;
    }
    if (proofs.records.size() != expected.records.size()) {
        diagnostic(diagnostics,
                   CompleteDiagnosticCode::InvalidProofReference, {},
                   plan.plan_id,
                   "proof record set differs from the derived index");
        return false;
    }
    for (std::size_t index = 0U; index < proofs.records.size(); ++index) {
        const auto& supplied = proofs.records[index];
        const auto& derived = expected.records[index];
        const bool same_sources =
            supplied.source_refs.size() == derived.source_refs.size() &&
            std::equal(
                supplied.source_refs.begin(), supplied.source_refs.end(),
                derived.source_refs.begin(),
                [](const auto& lhs, const auto& rhs) {
                    return lhs.document_uri == rhs.document_uri &&
                           lhs.node_path == rhs.node_path;
                });
        if (supplied.proof_id != derived.proof_id || !same_sources) {
            diagnostic(
                diagnostics,
                CompleteDiagnosticCode::InvalidProofReference, {},
                supplied.proof_id,
                "proof provenance differs from the exact plan provenance");
            return false;
        }
    }
    std::map<std::string, const PlanProofRecord*> records;
    for (const auto& record : proofs.records) {
        if (record.proof_id.empty() ||
            !records.emplace(record.proof_id, &record).second) {
            diagnostic(diagnostics,
                       CompleteDiagnosticCode::InvalidProofReference, {},
                       record.proof_id,
                       "proof record identity is empty or duplicated");
        }
    }
    std::map<std::string, const PlanElementProofCoverage*> coverage;
    for (const auto& item : proofs.coverage) {
        if (item.plan_element_id.empty() ||
            !coverage.emplace(item.plan_element_id, &item).second) {
            diagnostic(diagnostics,
                       CompleteDiagnosticCode::InvalidProofReference, {},
                       item.plan_element_id,
                       "proof coverage identity is empty or duplicated");
        }
    }
    for (const auto& [element, source] : all_plan_elements(plan)) {
        const auto found = coverage.find(element);
        if (found == coverage.end() || found->second->proof_ids.empty()) {
            diagnostic(diagnostics,
                       CompleteDiagnosticCode::MissingProofCoverage, source,
                       element,
                       "every plan element requires derived proof coverage");
            continue;
        }
        for (const auto& proof_id : found->second->proof_ids) {
            const auto record = records.find(proof_id);
            if (record == records.end() ||
                std::find(record->second->covered_plan_elements.begin(),
                          record->second->covered_plan_elements.end(),
                          element) ==
                    record->second->covered_plan_elements.end()) {
                diagnostic(diagnostics,
                           CompleteDiagnosticCode::InvalidProofReference,
                           source, element,
                           "coverage references no proof that covers this plan element");
            }
        }
    }
    return diagnostics.empty();
}

[[nodiscard]] inline bool validate_invocation_result_flows(
    const CompleteExecutionPlanDescriptor& plan,
    std::vector<CompleteDiagnostic>& diagnostics) {
    std::map<std::string, std::size_t> result_binding_uses;
    for (const auto& invocation : plan.invocation_bindings) {
        const auto callsite = std::find_if(
            plan.runtime_callsites.begin(), plan.runtime_callsites.end(),
            [&](const auto& candidate) {
                return candidate.callsite_id ==
                       invocation.caller_callsite_id;
            });
        const auto binding = std::find_if(
            plan.bindings.begin(), plan.bindings.end(),
            [&](const auto& candidate) {
                return candidate.binding_id ==
                       invocation.result_binding_id;
            });
        const auto slot = std::find_if(
            plan.slots.begin(), plan.slots.end(),
            [&](const auto& candidate) {
                return candidate.slot_id ==
                       invocation.provider_result_slot_id;
            });
        const auto expected_kind =
            invocation.kind ==
                    gnc::model_sdk::StaticInvocationKind::PureQuery
                ? gnc::model_sdk::BindingKind::PureQuery
                : gnc::model_sdk::BindingKind::ContinuousClosureLink;
        bool valid = gnc::model_sdk::valid_static_invocation_kind(
                         invocation.kind) &&
                     callsite != plan.runtime_callsites.end() &&
                     binding != plan.bindings.end() &&
                     slot != plan.slots.end();
        if (valid) {
            valid = binding->provider_occurrence_id ==
                        invocation.provider_occurrence_id &&
                    binding->provider_slot_id ==
                        invocation.provider_result_slot_id &&
                    binding->consumer_occurrence_id ==
                        callsite->occurrence_id &&
                    binding->consumer_port_id ==
                        invocation.consumer_port_id &&
                    binding->binding_kind == expected_kind &&
                    slot->kind == CompleteSlotKind::PortValue &&
                    slot->owner_occurrence_id ==
                        invocation.provider_occurrence_id &&
                    std::count(callsite->invocation_binding_ids.begin(),
                               callsite->invocation_binding_ids.end(),
                               invocation.invocation_id) == 1 &&
                    std::find(callsite->input_slot_ids.begin(),
                              callsite->input_slot_ids.end(),
                              invocation.provider_result_slot_id) ==
                        callsite->input_slot_ids.end();
        }
        if (!valid) {
            diagnostic(
                diagnostics,
                CompleteDiagnosticCode::SourceImageConformanceFailure,
                invocation.source, invocation.invocation_id,
                "invocation result Binding/slot/consumer endpoint is not exact or is duplicated as an ordinary pre-call input");
            continue;
        }
        ++result_binding_uses[invocation.result_binding_id];
    }
    for (const auto& invocation : plan.invocation_bindings) {
        if (result_binding_uses[invocation.result_binding_id] != 1U) {
            diagnostic(
                diagnostics,
                CompleteDiagnosticCode::SourceImageConformanceFailure,
                invocation.source, invocation.invocation_id,
                "each authorized invocation requires an exclusive result-flow Binding");
        }
    }
    return diagnostics.empty();
}

[[nodiscard]] inline bool validate_callsite_image_conformance(
    const CompleteExecutionPlanDescriptor& plan,
    const gnc::contracts::ExecutionPlanImageData& image,
    std::vector<CompleteDiagnostic>& diagnostics) {
    if (plan.runtime_callsites.size() != image.callsites.size()) {
        diagnostic(diagnostics,
                   CompleteDiagnosticCode::SourceImageConformanceFailure,
                   {}, plan.plan_id,
                   "runtime callsite descriptor/image cardinality differs");
        return false;
    }
    for (const auto& expected : plan.runtime_callsites) {
        std::size_t count = 0U;
        const gnc::contracts::PlanImageCallsite* actual = nullptr;
        for (const auto& candidate : image.callsites) {
            if (candidate.callsite_id == expected.callsite_id) {
                actual = &candidate;
                ++count;
            }
        }
        if (count != 1U || actual == nullptr ||
            actual->plan_element_id != expected.plan_element_id ||
            actual->request_contract_id != expected.request_contract_id ||
            actual->result_contract_id != expected.result_contract_id) {
            diagnostic(
                diagnostics,
                CompleteDiagnosticCode::SourceImageConformanceFailure,
                expected.source, expected.callsite_id,
                "runtime callsite request/result contracts are not preserved exactly in the image");
        }
    }
    return diagnostics.empty();
}

[[nodiscard]] inline std::string image_fingerprint(
    const gnc::contracts::ExecutionPlanImageData& image) {
    semantic_hash_detail::Encoder encoder;
    encoder.string(kExecutionPlanImageFingerprintIdentity);
    encoder.uint32(image.revision);
    encoder.string(image.mission_id);
    encoder.string(image.source_semantic_hash);
    encoder.string(image.descriptor_semantic_hash);
    encoder.string(image.proof_index_hash);
    encoder.uint32(image.clock.handle);
    encoder.string(image.clock.clock_id);
    encoder.float64(image.clock.base_step_seconds);
    encoder.integer(image.clock.initial_tick);
    encoder.integer(image.clock.terminal_tick);
    encoder.collection(image.packages.size());
    for (const auto& package : image.packages) {
        encoder.uint32(package.handle);
        encoder.string(package.package_id);
        encoder.string(package.package_version);
        encoder.string(package.build_fingerprint);
    }
    encoder.collection(image.entries.size());
    for (const auto& entry : image.entries) {
        encoder.uint32(entry.handle);
        encoder.uint32(entry.package_handle);
        encoder.string(entry.requirement_id);
        encoder.string(entry.entry_id);
        encoder.string(entry.entry_version);
        encoder.uint32(static_cast<std::uint32_t>(entry.kind));
        encoder.string(entry.signature_id);
        encoder.string(entry.call_shape_id);
        encoder.string(entry.state_layout_id);
        encoder.string(entry.workspace_layout_id);
        // typed_entry/link_anchor are process-local and intentionally
        // excluded. The former is recoverable by an exact package-owned
        // std::any_cast; the latter is inert symbol-retention evidence.
    }
    const auto encode_ids = [&](const auto& values, const auto& encode) {
        encoder.collection(values.size());
        for (const auto& value : values) {
            encode(value);
        }
    };
    encode_ids(image.occurrences, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.uint32(value.package_handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.occurrence_id);
        encoder.string(value.definition_id);
        encoder.string(value.definition_version);
        encoder.string(value.execution_form);
        encoder.string(value.placement);
        encoder.string(value.subject_entity_id);
        encoder.optional(value.has_scope);
        encoder.string(value.scope_kind);
        encoder.string(value.scope_subject_entity_id);
        encoder.string(value.canonical_configuration.schema_id);
        encoder.uint32(value.canonical_configuration.schema_version);
        encoder.collection(value.canonical_configuration.fields.size());
        for (const auto& field :
             value.canonical_configuration.fields) {
            encoder.string(field.field_id);
            encoder.uint32(static_cast<std::uint32_t>(field.kind));
            encoder.string(field.string_value);
            encoder.integer(field.integer_value);
            encoder.float64(field.float64_value);
        }
        encoder.collection(value.asset_bindings.size());
        for (const auto& asset : value.asset_bindings) {
            encoder.string(asset.role);
            encoder.string(asset.asset_schema_id);
            encoder.string(asset.asset_id);
        }
    });
    encode_ids(image.preparations, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.uint32(value.occurrence_handle);
        encoder.uint32(value.prepare_entry_handle);
    });
    encode_ids(image.queries, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.uint32(value.occurrence_handle);
        encoder.uint32(value.preparation_handle);
        encoder.uint32(value.query_entry_handle);
        encoder.string(value.workspace_requirement);
        encoder.collection(value.authorized_invocation_handles.size());
        for (const auto handle : value.authorized_invocation_handles) {
            encoder.uint32(handle);
        }
    });
    encode_ids(image.closures, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.uint32(value.occurrence_handle);
        encoder.uint32(value.preparation_handle);
        encoder.uint32(value.closure_entry_handle);
        encoder.string(value.strategy);
        encoder.string(value.workspace_requirement);
        encoder.collection(value.authorized_invocation_handles.size());
        for (const auto handle : value.authorized_invocation_handles) {
            encoder.uint32(handle);
        }
    });
    encode_ids(image.ports, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.uint32(value.occurrence_handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.port_id);
        encoder.string(value.contract_id);
        encoder.string(value.direction);
        encoder.string(value.binding_kind);
        encoder.string(value.temporal_relation);
    });
    encode_ids(image.slots, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.slot_id);
        encoder.uint32(static_cast<std::uint32_t>(value.kind));
        encoder.uint32(value.owner_occurrence_handle);
        encoder.uint32(value.port_handle);
        encoder.string(value.contract_or_layout_id);
        encoder.integer(static_cast<std::int64_t>(value.size_bytes));
        encoder.integer(static_cast<std::int64_t>(value.alignment_bytes));
    });
    encode_ids(image.state_blocks, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.uint32(value.owner_occurrence_handle);
        encoder.string(value.schema_id);
        encoder.uint32(value.schema_version);
        encoder.string(value.layout_id);
        encoder.integer(static_cast<std::int64_t>(value.size_bytes));
        encoder.integer(static_cast<std::int64_t>(value.alignment_bytes));
        encoder.string(value.evolution);
        encoder.uint32(value.committed_slot_handle);
        encoder.uint32(value.candidate_slot_handle);
    });
    encode_ids(image.initial_bindings, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.uint32(value.owner_occurrence_handle);
        encoder.uint32(value.committed_state_slot_handle);
        encoder.uint32(value.builder_entry_handle);
        encoder.string(value.input_schema_id);
        encoder.uint32(value.input_schema_version);
        encoder.collection(value.values.size());
        for (const auto& field : value.values) {
            encoder.string(field.field_id);
            encoder.uint32(static_cast<std::uint32_t>(field.kind));
            encoder.string(field.string_value);
            encoder.integer(field.integer_value);
            encoder.float64(field.float64_value);
        }
    });
    encode_ids(image.bindings, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.binding_id);
        encoder.uint32(value.provider_slot_handle);
        encoder.uint32(value.consumer_port_handle);
    });
    encode_ids(image.callsites, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.callsite_id);
        encoder.uint32(value.occurrence_handle);
        encoder.uint32(value.entry_handle);
        encoder.string(value.obligation);
        encoder.string(value.request_contract_id);
        encoder.string(value.result_contract_id);
        encoder.string(value.region_id);
        encoder.collection(value.input_slot_handles.size());
        for (const auto handle : value.input_slot_handles) {
            encoder.uint32(handle);
        }
        encoder.collection(value.output_slot_handles.size());
        for (const auto handle : value.output_slot_handles) {
            encoder.uint32(handle);
        }
        encoder.collection(value.authorized_invocation_handles.size());
        for (const auto handle : value.authorized_invocation_handles) {
            encoder.uint32(handle);
        }
    });
    encode_ids(image.runtime_components, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.uint32(value.occurrence_handle);
        encoder.uint32(value.definition_builder_entry_handle);
        encoder.string(value.recipe_id);
        encoder.string(value.profile);
        encoder.string(value.schedule_trigger);
        encoder.uint32(value.step_interval);
        encoder.uint32(value.offset);
        encoder.string(value.output_hold);
        encoder.uint32(value.max_input_age_steps);
        encoder.collection(value.lifecycle_capabilities.size());
        for (const auto& capability : value.lifecycle_capabilities) {
            encoder.string(capability);
        }
        encoder.collection(value.callsite_handles.size());
        for (const auto handle : value.callsite_handles) {
            encoder.uint32(handle);
        }
    });
    encode_ids(image.invocations, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.invocation_id);
        encoder.uint32(value.caller_callsite_handle);
        encoder.uint32(value.provider_occurrence_handle);
        encoder.uint32(value.entry_handle);
        encoder.string(value.requirement_id);
        encoder.string(value.invocation_kind);
        encoder.string(value.contract_id);
        encoder.uint32(value.result_binding_handle);
        encoder.uint32(value.provider_result_slot_handle);
        encoder.uint32(value.consumer_port_handle);
    });
    encode_ids(image.regions, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.region_id);
        encoder.uint32(value.ordinal);
        encoder.string(value.phase);
        encoder.collection(value.callsite_handles.size());
        for (const auto handle : value.callsite_handles) {
            encoder.uint32(handle);
        }
    });
    encode_ids(image.dag_nodes, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.node_id);
        encoder.uint32(static_cast<std::uint32_t>(value.kind));
        encoder.uint32(value.target_handle);
        encoder.string(value.phase);
    });
    encode_ids(image.dag_edges, [&](const auto& value) {
        encoder.string(value.plan_element_id);
        encoder.uint32(value.predecessor_node_handle);
        encoder.uint32(value.successor_node_handle);
    });
    encode_ids(image.integration_scopes, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.integration_scope_id);
        encoder.uint32(value.owner_occurrence_handle);
        encoder.uint32(value.committed_state_slot_handle);
        encoder.uint32(value.candidate_state_slot_handle);
        encoder.uint32(value.projection_callsite_handle);
        encoder.uint32(value.form_callsite_handle);
        encoder.uint32(value.derivative_callsite_handle);
        encoder.uint32(value.held_form_slot_handle);
        encoder.string(value.held_form_contract_id);
        encoder.string(value.derivative_request_contract_id);
        encoder.string(value.derivative_result_contract_id);
        encoder.collection(value.closure_invocation_handles.size());
        for (const auto handle : value.closure_invocation_handles) {
            encoder.uint32(handle);
        }
    });
    encode_ids(image.transactions, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.string(value.transaction_id);
        encoder.collection(value.owner_occurrence_handles.size());
        for (const auto handle : value.owner_occurrence_handles) {
            encoder.uint32(handle);
        }
        encoder.collection(value.candidate_state_slot_handles.size());
        for (const auto handle : value.candidate_state_slot_handles) {
            encoder.uint32(handle);
        }
        encoder.collection(value.candidate_producer_kinds.size());
        for (const auto& kind : value.candidate_producer_kinds) {
            encoder.string(kind);
        }
        encoder.collection(value.candidate_producer_handles.size());
        for (const auto handle : value.candidate_producer_handles) {
            encoder.uint32(handle);
        }
    });
    encode_ids(image.evaluator_histories, [&](const auto& value) {
        encoder.uint32(value.handle);
        encoder.string(value.plan_element_id);
        encoder.uint32(value.evaluator_callsite_handle);
        encoder.string(value.request_contract_id);
        encoder.uint32(value.history_depth);
        encoder.collection(value.ordered_members.size());
        for (const auto& member : value.ordered_members) {
            encoder.string(member.member_id);
            encoder.string(member.state_schema_id);
            encoder.string(member.state_layout_id);
            encoder.uint32(member.committed_state_slot_handle);
        }
    });
    return hash_bytes(encoder);
}

template <typename Value>
[[nodiscard]] inline const Value* find_exactly_one(
    const std::vector<Value>& values,
    const std::function<bool(const Value&)>& predicate,
    std::size_t& count) {
    const Value* result = nullptr;
    count = 0U;
    for (const auto& value : values) {
        if (predicate(value)) {
            ++count;
            result = &value;
        }
    }
    return result;
}

} // namespace complete_plan_detail

[[nodiscard]] inline CompleteOutcome<gnc::contracts::ExecutionPlanImage>
link_complete_execution_plan(
    const CompleteExecutionPlanDescriptor& plan,
    const PlanProofIndex& proofs,
    const std::vector<gnc::model_sdk::StaticPackageImplementation>&
        implementations) {
    using namespace complete_plan_detail;
    CompleteOutcome<gnc::contracts::ExecutionPlanImage> outcome;
    if (plan.revision != 4U ||
        plan.descriptor_identity != kCompleteExecutionPlanDescriptorIdentity ||
        plan.descriptor_semantic_hash != descriptor_hash(plan)) {
        diagnostic(outcome.diagnostics,
                   CompleteDiagnosticCode::SourceImageConformanceFailure, {},
                   plan.plan_id,
                   "plan descriptor identity/revision/stable hash is invalid");
        return outcome;
    }
    static_cast<void>(validate_proofs(plan, proofs, outcome.diagnostics));
    static_cast<void>(validate_invocation_result_flows(
        plan, outcome.diagnostics));
    if (!outcome.diagnostics.empty()) {
        return outcome;
    }

    // Build the descriptor side of the Evaluator callable-history contract
    // from exact plan facts. The package implementation must independently
    // witness the same request, depth, and ordered schema/layout members.
    std::map<std::string,
             gnc::model_sdk::StaticEvaluatorHistoryShapeDescriptor>
        evaluator_history_requirements;
    for (const auto& history : plan.evaluator_histories) {
        const auto callsite = std::find_if(
            plan.runtime_callsites.begin(), plan.runtime_callsites.end(),
            [&](const auto& candidate) {
                return candidate.callsite_id ==
                       history.evaluator_callsite_id;
            });
        if (callsite == plan.runtime_callsites.end() ||
            callsite->request_contract_id != history.request_contract_id) {
            diagnostic(
                outcome.diagnostics,
                CompleteDiagnosticCode::SourceImageConformanceFailure,
                history.source, history.history_id,
                "evaluator history and runtime callsite request contracts differ");
            continue;
        }
        gnc::model_sdk::StaticEvaluatorHistoryShapeDescriptor expected;
        expected.request_contract_id = history.request_contract_id;
        expected.depth = history.committed_history_depth;
        for (const auto& member : history.ordered_members) {
            expected.ordered_members.push_back(
                {member.member_id, member.state_schema_id,
                 member.state_layout_id});
        }
        if (!evaluator_history_requirements
                 .emplace(callsite->entry_requirement_id,
                          std::move(expected))
                 .second) {
            diagnostic(
                outcome.diagnostics,
                CompleteDiagnosticCode::SourceImageConformanceFailure,
                history.source, history.history_id,
                "one evaluator entry requirement has multiple history plans");
        }
    }
    for (const auto& component : plan.runtime_components) {
        if (component.profile !=
            gnc::model_sdk::RuntimeCellProfile::Evaluator) {
            continue;
        }
        for (const auto& callsite_id : component.callsite_ids) {
            const auto callsite = std::find_if(
                plan.runtime_callsites.begin(),
                plan.runtime_callsites.end(), [&](const auto& candidate) {
                    return candidate.callsite_id == callsite_id;
                });
            const auto history_count = static_cast<std::size_t>(std::count_if(
                plan.evaluator_histories.begin(),
                plan.evaluator_histories.end(), [&](const auto& history) {
                    return history.evaluator_callsite_id == callsite_id;
                }));
            if (callsite == plan.runtime_callsites.end() ||
                history_count != 1U ||
                evaluator_history_requirements.count(
                    callsite == plan.runtime_callsites.end()
                        ? std::string{}
                        : callsite->entry_requirement_id) != 1U) {
                diagnostic(
                    outcome.diagnostics,
                    CompleteDiagnosticCode::SourceImageConformanceFailure,
                    component.source, callsite_id,
                    "every evaluator callsite requires exactly one implementation-witnessed history plan");
            }
        }
    }
    if (!outcome.diagnostics.empty()) {
        return outcome;
    }

    gnc::contracts::ExecutionPlanImageData image;
    image.plan_id = plan.plan_id;
    image.mission_id = plan.mission_id;
    image.source_semantic_hash = plan.source_semantic_hash;
    image.descriptor_semantic_hash = plan.descriptor_semantic_hash;
    image.proof_index_hash = proofs.proof_index_hash;
    std::uint32_t next_handle = 1U;
    image.clock = {next_handle++, plan.clock.plan_element_id,
                   plan.clock.clock_id, plan.clock.base_step_seconds,
                   plan.clock.initial_tick, plan.clock.terminal_tick};

    std::map<std::string, std::uint32_t> package_handles;
    std::map<std::string, const gnc::model_sdk::StaticPackageImplementation*>
        package_implementations;
    for (const auto& package : plan.dependency_lock) {
        std::size_t count = 0U;
        const auto* implementation = find_exactly_one<
            gnc::model_sdk::StaticPackageImplementation>(
            implementations,
            [&](const auto& candidate) {
                return candidate.package_id == package.package_id &&
                       candidate.package_version == package.package_version;
            },
            count);
        if (count != 1U || implementation == nullptr ||
            implementation->build_fingerprint.empty()) {
            diagnostic(
                outcome.diagnostics,
                count == 0U
                    ? CompleteDiagnosticCode::MissingImplementationPackage
                    : CompleteDiagnosticCode::MultipleImplementationPackages,
                {}, package.package_id,
                count == 0U
                    ? "exact locked package implementation is missing"
                    : "exact locked package implementation is duplicated or lacks a build fingerprint");
            continue;
        }
        const auto key = exact_key(package.package_id,
                                   package.package_version);
        const auto handle = next_handle++;
        package_handles.emplace(key, handle);
        package_implementations.emplace(key, implementation);
        image.packages.push_back({handle, package.package_id,
                                  package.package_version,
                                  implementation->build_fingerprint});
    }
    if (implementations.size() != plan.dependency_lock.size()) {
        diagnostic(outcome.diagnostics,
                   CompleteDiagnosticCode::MultipleImplementationPackages,
                   {}, plan.plan_id,
                   "implementation package set must equal the exact dependency lock");
    }
    if (!outcome.diagnostics.empty()) {
        return outcome;
    }

    std::map<std::string, std::uint32_t> entry_handles;
    for (const auto& requirement : plan.entry_requirements) {
        const auto package_key = exact_key(requirement.package.package_id,
                                           requirement.package.package_version);
        const auto* package = package_implementations.at(package_key);
        std::size_t count = 0U;
        const auto* entry = find_exactly_one<
            gnc::model_sdk::StaticImplementationEntry>(
            package->entries,
            [&](const auto& candidate) {
                return candidate.entry_id == requirement.entry_id &&
                       candidate.entry_version == requirement.entry_version;
            },
            count);
        if (count != 1U || entry == nullptr) {
            diagnostic(
                outcome.diagnostics,
                count == 0U
                    ? CompleteDiagnosticCode::MissingImplementationEntry
                    : CompleteDiagnosticCode::MultipleImplementationEntries,
                requirement.source, requirement.requirement_id,
                count == 0U ? "required exact entry is missing"
                             : "required exact entry is duplicated");
            continue;
        }
        const auto expected_history =
            evaluator_history_requirements.find(
                requirement.requirement_id);
        const bool history_witness_matches =
            expected_history == evaluator_history_requirements.end()
                ? !entry->evaluator_history_witness.has_value()
                : entry->evaluator_history_witness.has_value() &&
                      gnc::model_sdk::matches_static_evaluator_history_witness(
                          *entry->evaluator_history_witness,
                          expected_history->second);
        if (entry->kind != requirement.kind ||
            entry->signature_id != requirement.signature_id ||
            requirement.call_shape_id.empty() ||
            entry->call_shape_id.empty() ||
            entry->call_shape_id != requirement.call_shape_id ||
            entry->state_layout_id != requirement.state_layout_id ||
            entry->workspace_layout_id !=
                requirement.workspace_layout_id ||
            !entry->typed_entry.has_value() ||
            entry->callable_contract_type == nullptr ||
            entry->typed_entry.type() != *entry->callable_contract_type ||
            entry->link_anchor == nullptr ||
            !history_witness_matches) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::ImplementationMismatch,
                       requirement.source, requirement.requirement_id,
                       "entry kind/signature/call shape/state layout/workspace layout/typed contract/evaluator history witness differs from the plan requirement");
            continue;
        }
        const auto handle = next_handle++;
        entry_handles.emplace(requirement.requirement_id, handle);
        image.entries.push_back(
            {handle, package_handles.at(package_key),
             requirement.requirement_id, entry->entry_id,
             entry->entry_version, image_entry_kind(entry->kind),
             entry->signature_id, entry->call_shape_id,
             entry->state_layout_id,
             entry->workspace_layout_id, entry->typed_entry,
             entry->link_anchor});
    }
    if (!outcome.diagnostics.empty()) {
        return outcome;
    }

    std::map<std::string, std::uint32_t> occurrence_handles;
    std::map<std::string, std::vector<std::uint32_t>> conformance_handles;
    conformance_handles[plan.clock.plan_element_id].push_back(
        image.clock.handle);
    for (const auto& occurrence : plan.occurrences) {
        const auto handle = next_handle++;
        occurrence_handles.emplace(occurrence.occurrence_id, handle);
        const auto package_key = exact_key(occurrence.package.package_id,
                                           occurrence.package.package_version);
        gnc::contracts::PlanImageOccurrence image_occurrence;
        image_occurrence.handle = handle;
        image_occurrence.package_handle = package_handles.at(package_key);
        image_occurrence.plan_element_id = occurrence.plan_element_id;
        image_occurrence.occurrence_id = occurrence.occurrence_id;
        image_occurrence.definition_id = occurrence.model_id;
        image_occurrence.definition_version = occurrence.model_version;
        image_occurrence.execution_form = std::string(
            gnc::model_sdk::to_string(occurrence.execution_form));
        image_occurrence.placement =
            std::string(gnc::model_sdk::to_string(occurrence.placement));
        image_occurrence.subject_entity_id =
            occurrence.subject_entity_id;
        image_occurrence.has_scope = occurrence.scope.has_value();
        if (occurrence.scope.has_value()) {
            image_occurrence.scope_kind =
                std::string(gnc::compiler::to_string(
                    occurrence.scope->kind));
            image_occurrence.scope_subject_entity_id =
                occurrence.scope->subject_entity_id;
        }
        image_occurrence.canonical_configuration.schema_id =
            occurrence.canonical_configuration.schema_id;
        image_occurrence.canonical_configuration.schema_version =
            occurrence.canonical_configuration.schema_version;
        for (const auto& field :
             occurrence.canonical_configuration.fields) {
            gnc::contracts::PlanImageOccurrence::ConfigBlock::Field
                image_field;
            image_field.field_id = field.field_id;
            if (const auto* text =
                    std::get_if<std::string>(&field.value)) {
                image_field.kind =
                    gnc::contracts::PlanImageValueKind::String;
                image_field.string_value = *text;
            } else if (const auto* token = std::get_if<
                           gnc::model_sdk::CanonicalEnumValue>(
                           &field.value)) {
                image_field.kind =
                    gnc::contracts::PlanImageValueKind::Enum;
                image_field.string_value = token->token;
            } else if (const auto* integer =
                           std::get_if<std::int64_t>(&field.value)) {
                image_field.kind =
                    gnc::contracts::PlanImageValueKind::Integer;
                image_field.integer_value = *integer;
            } else {
                image_field.kind =
                    gnc::contracts::PlanImageValueKind::Float64;
                image_field.float64_value = std::get<double>(field.value);
            }
            image_occurrence.canonical_configuration.fields.push_back(
                std::move(image_field));
        }
        for (const auto& asset : occurrence.asset_bindings) {
            image_occurrence.asset_bindings.push_back(
                {asset.role, asset.asset_schema_id, asset.asset_id});
        }
        image.occurrences.push_back(std::move(image_occurrence));
        conformance_handles[occurrence.plan_element_id].push_back(handle);
    }
    std::map<std::string, std::uint32_t> preparation_handles;
    for (const auto& preparation : plan.preparation_inputs) {
        const auto handle = next_handle++;
        preparation_handles.emplace(preparation.preparation_input_id,
                                    handle);
        image.preparations.push_back(
            {handle, preparation.plan_element_id,
             occurrence_handles.at(preparation.occurrence_id),
             entry_handles.at(
                 preparation.preparation_entry_requirement_id)});
        conformance_handles[preparation.plan_element_id].push_back(handle);
    }
    std::map<std::string, std::uint32_t> port_handles;
    for (const auto& port : plan.ports) {
        const auto handle = next_handle++;
        port_handles.emplace(port.occurrence_id + "\x1f" + port.port_id,
                             handle);
        image.ports.push_back(
            {handle, occurrence_handles.at(port.occurrence_id),
             port.plan_element_id, port.port_id, port.contract_id,
             port.direction == gnc::model_sdk::StaticPortDirection::Input
                 ? "Input"
                 : "Output",
             std::string(gnc::model_sdk::to_string(port.binding_kind)),
             std::string(
                 gnc::model_sdk::to_string(port.temporal_relation))});
        conformance_handles[port.plan_element_id].push_back(handle);
    }
    std::map<std::string,
             const gnc::model_sdk::StaticStateLayoutImplementation*>
        state_layouts;
    for (const auto& state : plan.state_blocks) {
        const auto occurrence = std::find_if(
            plan.occurrences.begin(), plan.occurrences.end(),
            [&](const auto& value) {
                return value.occurrence_id == state.owner_occurrence_id;
            });
        if (occurrence == plan.occurrences.end()) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::SourceImageConformanceFailure,
                       state.source, state.plan_element_id,
                       "state owner occurrence is absent from the descriptor");
            continue;
        }
        const auto package_key = exact_key(occurrence->package.package_id,
                                           occurrence->package.package_version);
        const auto* package = package_implementations.at(package_key);
        std::size_t count = 0U;
        const auto* layout = find_exactly_one<
            gnc::model_sdk::StaticStateLayoutImplementation>(
            package->state_layouts,
            [&](const auto& candidate) {
                return candidate.layout_id == state.layout_id;
            },
            count);
        const bool power_of_two =
            layout != nullptr && layout->alignment_bytes != 0U &&
            (layout->alignment_bytes & (layout->alignment_bytes - 1U)) == 0U;
        if (count != 1U || layout == nullptr || layout->size_bytes == 0U ||
            !power_of_two ||
            layout->size_bytes % layout->alignment_bytes != 0U) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::ImplementationMismatch,
                       state.source, state.layout_id,
                       "state layout must resolve exactly once to valid process-local sizeof/alignof facts");
            continue;
        }
        state_layouts.emplace(state.owner_occurrence_id, layout);
    }
    if (!outcome.diagnostics.empty()) {
        return outcome;
    }
    std::map<std::string,
             const gnc::model_sdk::StaticValueLayoutImplementation*>
        value_layouts;
    std::map<std::string, SourceRef> required_value_layouts;
    for (const auto& slot : plan.slots) {
        if (slot.kind == CompleteSlotKind::PortValue ||
            slot.kind == CompleteSlotKind::HeldIntervalValue) {
            required_value_layouts.emplace(slot.contract_or_layout_id,
                                           slot.source);
        }
    }
    for (const auto& [contract_id, source] : required_value_layouts) {
        const gnc::model_sdk::StaticValueLayoutImplementation* layout =
            nullptr;
        std::size_t count = 0U;
        for (const auto& implementation : implementations) {
            for (const auto& candidate : implementation.value_layouts) {
                if (candidate.contract_id == contract_id) {
                    layout = &candidate;
                    ++count;
                }
            }
        }
        const bool power_of_two =
            layout != nullptr && layout->alignment_bytes != 0U &&
            (layout->alignment_bytes & (layout->alignment_bytes - 1U)) == 0U;
        if (count != 1U || layout == nullptr || layout->size_bytes == 0U ||
            !power_of_two ||
            layout->size_bytes % layout->alignment_bytes != 0U) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::ImplementationMismatch,
                       source, contract_id,
                       "runtime value contract must resolve exactly once to valid process-local sizeof/alignof facts");
            continue;
        }
        value_layouts.emplace(contract_id, layout);
    }
    if (!outcome.diagnostics.empty()) {
        return outcome;
    }
    std::map<std::string, std::uint32_t> slot_handles;
    for (const auto& slot : plan.slots) {
        const auto handle = next_handle++;
        slot_handles.emplace(slot.slot_id, handle);
        gnc::contracts::PlanImageSlotKind kind =
            gnc::contracts::PlanImageSlotKind::PortValue;
        if (slot.kind == CompleteSlotKind::CommittedState) {
            kind = gnc::contracts::PlanImageSlotKind::CommittedState;
        } else if (slot.kind == CompleteSlotKind::CandidateState) {
            kind = gnc::contracts::PlanImageSlotKind::CandidateState;
        } else if (slot.kind == CompleteSlotKind::HeldIntervalValue) {
            kind = gnc::contracts::PlanImageSlotKind::HeldIntervalValue;
        }
        std::uint32_t port_handle = 0U;
        if (!slot.port_id.empty()) {
            port_handle = port_handles.at(slot.owner_occurrence_id + "\x1f" +
                                          slot.port_id);
        }
        std::uint64_t size_bytes = 0U;
        std::uint64_t alignment_bytes = 1U;
        if (slot.kind == CompleteSlotKind::CommittedState ||
            slot.kind == CompleteSlotKind::CandidateState) {
            const auto* layout = state_layouts.at(slot.owner_occurrence_id);
            size_bytes = static_cast<std::uint64_t>(layout->size_bytes);
            alignment_bytes =
                static_cast<std::uint64_t>(layout->alignment_bytes);
        } else {
            const auto* layout =
                value_layouts.at(slot.contract_or_layout_id);
            size_bytes = static_cast<std::uint64_t>(layout->size_bytes);
            alignment_bytes =
                static_cast<std::uint64_t>(layout->alignment_bytes);
        }
        image.slots.push_back(
            {handle, slot.plan_element_id, slot.slot_id, kind,
             occurrence_handles.at(slot.owner_occurrence_id), port_handle,
             slot.contract_or_layout_id, size_bytes, alignment_bytes});
        conformance_handles[slot.plan_element_id].push_back(handle);
    }
    for (const auto& state : plan.state_blocks) {
        const auto handle = next_handle++;
        const auto* layout = state_layouts.at(state.owner_occurrence_id);
        image.state_blocks.push_back(
            {handle, state.plan_element_id,
             occurrence_handles.at(state.owner_occurrence_id),
             state.schema_id, state.schema_version, state.layout_id,
             static_cast<std::uint64_t>(layout->size_bytes),
             static_cast<std::uint64_t>(layout->alignment_bytes),
             std::string(gnc::model_sdk::to_string(state.evolution)),
             slot_handles.at(state.committed_slot_id),
             slot_handles.at(state.candidate_slot_id)});
        conformance_handles[state.plan_element_id].push_back(handle);
    }
    for (const auto& initial : plan.initial_states) {
        const auto handle = next_handle++;
        std::vector<gnc::contracts::PlanImageValue> values;
        for (const auto& field : initial.builder_inputs.fields) {
            gnc::contracts::PlanImageValue value;
            value.field_id = field.field_id;
            if (const auto* text =
                    std::get_if<std::string>(&field.value)) {
                value.kind = gnc::contracts::PlanImageValueKind::String;
                value.string_value = *text;
            } else if (const auto* token = std::get_if<
                           gnc::model_sdk::CanonicalEnumValue>(
                           &field.value)) {
                value.kind = gnc::contracts::PlanImageValueKind::Enum;
                value.string_value = token->token;
            } else if (const auto* integer =
                           std::get_if<std::int64_t>(&field.value)) {
                value.kind = gnc::contracts::PlanImageValueKind::Integer;
                value.integer_value = *integer;
            } else if (const auto* scalar =
                           std::get_if<double>(&field.value)) {
                value.kind = gnc::contracts::PlanImageValueKind::Float64;
                value.float64_value = *scalar;
            } else {
                value.kind =
                    gnc::contracts::PlanImageValueKind::Float64;
                value.float64_value = std::get<double>(field.value);
            }
            values.push_back(std::move(value));
        }
        image.initial_bindings.push_back(
            {handle, initial.plan_element_id,
             occurrence_handles.at(initial.owner_occurrence_id),
             slot_handles.at(initial.committed_slot_id),
             entry_handles.at(initial.builder_entry_requirement_id),
             initial.builder_inputs.schema_id,
             initial.builder_inputs.schema_version,
             std::move(values)});
        conformance_handles[initial.plan_element_id].push_back(handle);
    }
    std::map<std::string, std::uint32_t> binding_handles;
    for (const auto& binding : plan.bindings) {
        const auto handle = next_handle++;
        binding_handles.emplace(binding.binding_id, handle);
        image.bindings.push_back(
            {handle, binding.plan_element_id, binding.binding_id,
             slot_handles.at(binding.provider_slot_id),
             port_handles.at(binding.consumer_occurrence_id + "\x1f" +
                             binding.consumer_port_id)});
        conformance_handles[binding.plan_element_id].push_back(handle);
    }

    std::map<std::string, std::uint32_t> invocation_handles;
    std::map<std::string, std::uint32_t> callsite_handles;
    for (const auto& callsite : plan.runtime_callsites) {
        callsite_handles.emplace(callsite.callsite_id, next_handle++);
    }
    for (const auto& invocation : plan.invocation_bindings) {
        const auto handle = next_handle++;
        invocation_handles.emplace(invocation.invocation_id, handle);
        const auto entry_requirement =
            invocation.kind == gnc::model_sdk::StaticInvocationKind::PureQuery
                ? plan.queries.at(
                      std::find_if(
                          plan.queries.begin(), plan.queries.end(),
                          [&](const auto& query) {
                              return query.occurrence_id ==
                                     invocation.provider_occurrence_id;
                          }) -
                      plan.queries.begin())
                      .entry_requirement_id
                : plan.closures.at(
                      std::find_if(
                          plan.closures.begin(), plan.closures.end(),
                          [&](const auto& closure) {
                              return closure.occurrence_id ==
                                     invocation.provider_occurrence_id;
                          }) -
                      plan.closures.begin())
                      .entry_requirement_id;
        const auto caller_callsite = std::find_if(
            plan.runtime_callsites.begin(), plan.runtime_callsites.end(),
            [&](const auto& candidate) {
                return candidate.callsite_id ==
                       invocation.caller_callsite_id;
            });
        const auto consumer_port_key =
            caller_callsite->occurrence_id + "\x1f" +
            invocation.consumer_port_id;
        image.invocations.push_back(
            {handle, invocation.plan_element_id, invocation.invocation_id,
             callsite_handles.at(invocation.caller_callsite_id),
             occurrence_handles.at(invocation.provider_occurrence_id),
             entry_handles.at(entry_requirement), invocation.requirement_id,
             invocation_kind_token(invocation.kind), invocation.contract_id,
             binding_handles.at(invocation.result_binding_id),
             slot_handles.at(invocation.provider_result_slot_id),
             port_handles.at(consumer_port_key)});
        conformance_handles[invocation.plan_element_id].push_back(handle);
    }
    for (const auto& query : plan.queries) {
        const auto handle = next_handle++;
        std::vector<std::uint32_t> authorized;
        authorized.reserve(query.authorized_invocation_ids.size());
        for (const auto& invocation_id :
             query.authorized_invocation_ids) {
            authorized.push_back(invocation_handles.at(invocation_id));
        }
        image.queries.push_back(
            {handle, query.plan_element_id,
             occurrence_handles.at(query.occurrence_id),
             preparation_handles.at(query.preparation_input_ref),
             entry_handles.at(query.entry_requirement_id),
             std::string(gnc::model_sdk::to_string(
                 query.workspace_requirement)),
             std::move(authorized)});
        conformance_handles[query.plan_element_id].push_back(handle);
    }
    for (const auto& closure : plan.closures) {
        const auto handle = next_handle++;
        std::vector<std::uint32_t> authorized;
        authorized.reserve(closure.authorized_invocation_ids.size());
        for (const auto& invocation_id :
             closure.authorized_invocation_ids) {
            authorized.push_back(invocation_handles.at(invocation_id));
        }
        image.closures.push_back(
            {handle, closure.plan_element_id,
             occurrence_handles.at(closure.occurrence_id),
             preparation_handles.at(closure.preparation_input_ref),
             entry_handles.at(closure.entry_requirement_id),
             std::string(gnc::contracts::to_string(closure.strategy)),
             std::string(gnc::model_sdk::to_string(
                 closure.workspace_requirement)),
             std::move(authorized)});
        conformance_handles[closure.plan_element_id].push_back(handle);
    }
    for (const auto& callsite : plan.runtime_callsites) {
        std::vector<std::uint32_t> inputs;
        std::vector<std::uint32_t> outputs;
        std::vector<std::uint32_t> invocations;
        for (const auto& slot : callsite.input_slot_ids) {
            inputs.push_back(slot_handles.at(slot));
        }
        for (const auto& slot : callsite.output_slot_ids) {
            outputs.push_back(slot_handles.at(slot));
        }
        for (const auto& invocation :
             callsite.invocation_binding_ids) {
            invocations.push_back(invocation_handles.at(invocation));
        }
        const auto handle = callsite_handles.at(callsite.callsite_id);
        image.callsites.push_back(
            {handle, callsite.plan_element_id, callsite.callsite_id,
             occurrence_handles.at(callsite.occurrence_id),
             entry_handles.at(callsite.entry_requirement_id),
             std::string(gnc::contracts::to_string(callsite.obligation)),
             callsite.request_contract_id, callsite.result_contract_id,
             callsite.region_id, std::move(inputs), std::move(outputs),
             std::move(invocations)});
        conformance_handles[callsite.plan_element_id].push_back(handle);
    }
    static_cast<void>(validate_callsite_image_conformance(
        plan, image, outcome.diagnostics));
    if (!outcome.diagnostics.empty()) {
        return outcome;
    }
    for (const auto& component : plan.runtime_components) {
        const auto handle = next_handle++;
        std::vector<std::string> lifecycle;
        lifecycle.reserve(component.lifecycle_capabilities.size());
        for (const auto capability : component.lifecycle_capabilities) {
            lifecycle.emplace_back(gnc::model_sdk::to_string(capability));
        }
        std::vector<std::uint32_t> callsites;
        callsites.reserve(component.callsite_ids.size());
        for (const auto& callsite_id : component.callsite_ids) {
            callsites.push_back(callsite_handles.at(callsite_id));
        }
        image.runtime_components.push_back(
            {handle, component.plan_element_id,
             occurrence_handles.at(component.occurrence_id),
             entry_handles.at(
                 component.definition_builder_entry_requirement_id),
             component.recipe_id,
             std::string(gnc::model_sdk::to_string(component.profile)),
             std::string(gnc::model_sdk::to_string(
                 component.schedule.trigger)),
             component.schedule.step_interval,
             component.schedule.offset,
             std::string(gnc::model_sdk::to_string(
                 component.schedule.output_hold)),
             component.schedule.max_input_age_steps,
             std::move(lifecycle), std::move(callsites)});
        conformance_handles[component.plan_element_id].push_back(handle);
    }
    std::map<std::string, std::uint32_t> integration_handles;
    for (const auto& scope : plan.integration_scopes) {
        integration_handles.emplace(scope.integration_scope_id,
                                    next_handle++);
    }
    for (const auto& region : plan.regions) {
        const auto handle = next_handle++;
        std::vector<std::uint32_t> callsites;
        for (const auto& callsite : region.callsite_ids) {
            callsites.push_back(callsite_handles.at(callsite));
        }
        image.regions.push_back(
            {handle, region.plan_element_id, region.region_id,
             region.ordinal,
             std::string(gnc::model_sdk::to_string(region.phase)),
             std::move(callsites)});
        conformance_handles[region.plan_element_id].push_back(handle);
    }
    for (const auto& scope : plan.integration_scopes) {
        const auto handle = integration_handles.at(scope.integration_scope_id);
        std::vector<std::uint32_t> closures;
        for (const auto& invocation : scope.closure_invocation_ids) {
            closures.push_back(invocation_handles.at(invocation));
        }
        image.integration_scopes.push_back(
            {handle, scope.plan_element_id, scope.integration_scope_id,
             occurrence_handles.at(scope.owner_occurrence_id),
             slot_handles.at(scope.committed_state_slot_id),
             slot_handles.at(scope.candidate_state_slot_id),
             callsite_handles.at(scope.projection_callsite_id),
             callsite_handles.at(scope.form_callsite_id),
             callsite_handles.at(scope.derivative_callsite_id),
             slot_handles.at(scope.held_form_slot_id),
             scope.held_form_contract_id,
             scope.derivative_request_contract_id,
             scope.derivative_result_contract_id,
             std::move(closures)});
        conformance_handles[scope.plan_element_id].push_back(handle);
    }
    std::map<std::string, std::uint32_t> dag_node_handles;
    for (const auto& node : plan.boundary_dag_nodes) {
        const auto handle = next_handle++;
        dag_node_handles.emplace(node.node_id, handle);
        const auto target_handle =
            node.kind == BoundaryDagNodeKind::RuntimeCallsite
                ? callsite_handles.at(node.target_id)
                : integration_handles.at(node.target_id);
        image.dag_nodes.push_back(
            {handle, node.plan_element_id, node.node_id,
             node.kind == BoundaryDagNodeKind::RuntimeCallsite
                 ? gnc::contracts::PlanImageDagNodeKind::RuntimeCallsite
                 : gnc::contracts::PlanImageDagNodeKind::IntegrationScope,
             target_handle,
             std::string(gnc::model_sdk::to_string(node.phase))});
        conformance_handles[node.plan_element_id].push_back(handle);
    }
    for (const auto& edge : plan.boundary_dag) {
        image.dag_edges.push_back(
            {edge.plan_element_id,
             dag_node_handles.at(edge.predecessor_node_id),
             dag_node_handles.at(edge.successor_node_id)});
        conformance_handles[edge.plan_element_id].push_back(
            dag_node_handles.at(edge.successor_node_id));
    }
    for (const auto& transaction : plan.transactions) {
        const auto handle = next_handle++;
        std::vector<std::uint32_t> owners;
        std::vector<std::uint32_t> candidates;
        std::vector<std::string> producer_kinds;
        std::vector<std::uint32_t> producers;
        for (const auto& owner : transaction.owner_occurrence_ids) {
            owners.push_back(occurrence_handles.at(owner));
        }
        for (const auto& candidate : transaction.candidate_state_slot_ids) {
            candidates.push_back(slot_handles.at(candidate));
        }
        for (const auto& producer : transaction.candidate_producers) {
            if (producer.kind == CandidateProducerKind::IntegrationScope) {
                producer_kinds.push_back("IntegrationScope");
                producers.push_back(
                    integration_handles.at(producer.producer_id));
            } else {
                producer_kinds.push_back("RuntimeCallsite");
                producers.push_back(callsite_handles.at(producer.producer_id));
            }
        }
        image.transactions.push_back(
            {handle, transaction.plan_element_id, transaction.transaction_id,
             std::move(owners), std::move(candidates),
             std::move(producer_kinds), std::move(producers)});
        conformance_handles[transaction.plan_element_id].push_back(handle);
    }
    for (const auto& history : plan.evaluator_histories) {
        const auto handle = next_handle++;
        std::vector<gnc::contracts::PlanImageEvaluatorHistoryMember>
            members;
        for (const auto& member : history.ordered_members) {
            members.push_back(
                {member.member_id, member.state_schema_id,
                 member.state_layout_id,
                 slot_handles.at(member.committed_state_slot_id)});
        }
        image.evaluator_histories.push_back(
            {handle, history.plan_element_id,
             callsite_handles.at(history.evaluator_callsite_id),
             history.request_contract_id,
             history.committed_history_depth, std::move(members)});
        conformance_handles[history.plan_element_id].push_back(handle);
    }
    for (const auto& requirement : plan.entry_requirements) {
        conformance_handles[requirement.plan_element_id].push_back(
            entry_handles.at(requirement.requirement_id));
    }
    for (const auto& projection : plan.projections) {
        conformance_handles[projection.plan_element_id].push_back(
            callsite_handles.at(projection.callsite_id));
    }

    std::map<std::string, const PlanElementProofCoverage*> proof_coverage;
    std::map<std::string, const PlanProofRecord*> proof_records;
    for (const auto& record : proofs.records) {
        proof_records.emplace(record.proof_id, &record);
    }
    for (const auto& coverage : proofs.coverage) {
        proof_coverage.emplace(coverage.plan_element_id, &coverage);
    }
    for (const auto& [element, source] : all_plan_elements(plan)) {
        auto handles = conformance_handles[element];
        if (handles.empty()) {
            diagnostic(outcome.diagnostics,
                       CompleteDiagnosticCode::SourceImageConformanceFailure,
                       source, element,
                       "plan element has no exact image handle mapping");
            continue;
        }
        std::sort(handles.begin(), handles.end());
        handles.erase(std::unique(handles.begin(), handles.end()),
                      handles.end());
        const auto coverage = proof_coverage.at(element);
        std::vector<gnc::contracts::PlanImageSourceRef> sources{
            {source.document_uri, source.node_path}};
        for (const auto& proof_id : coverage->proof_ids) {
            for (const auto& proof_source :
                 proof_records.at(proof_id)->source_refs) {
                sources.push_back({proof_source.document_uri,
                                   proof_source.node_path});
            }
        }
        std::sort(sources.begin(), sources.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return std::tie(lhs.document_uri, lhs.node_path) <
                             std::tie(rhs.document_uri, rhs.node_path);
                  });
        sources.erase(
            std::unique(sources.begin(), sources.end(),
                        [](const auto& lhs, const auto& rhs) {
                            return lhs.document_uri == rhs.document_uri &&
                                   lhs.node_path == rhs.node_path;
                        }),
            sources.end());
        image.conformance.push_back(
            {element, std::move(sources),
             coverage->proof_ids, std::move(handles)});
    }
    if (!outcome.diagnostics.empty()) {
        return outcome;
    }
    image.image_fingerprint = image_fingerprint(image);
    outcome.value = gnc::contracts::ExecutionPlanImage::freeze(
        std::move(image));
    return outcome;
}

[[nodiscard]] inline CompleteOutcome<gnc::contracts::ExecutionPlanImage>
compile_and_link_complete_execution_plan(
    const CompleteStaticCompositionSource& source,
    const std::vector<gnc::model_sdk::StaticPackageDescriptor>& packages,
    const std::vector<gnc::model_sdk::StaticPackageImplementation>&
        implementations) {
    auto compilation = compile_complete_execution_plan(source, packages);
    if (!compilation.succeeded()) {
        CompleteOutcome<gnc::contracts::ExecutionPlanImage> outcome;
        outcome.diagnostics = std::move(compilation.diagnostics);
        return outcome;
    }
    return link_complete_execution_plan(compilation.value->plan,
                                        compilation.value->proofs,
                                        implementations);
}

} // namespace gnc::compiler

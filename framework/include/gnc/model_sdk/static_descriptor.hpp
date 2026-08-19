#pragma once

#include "gnc/model_sdk/model_metadata.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace gnc::model_sdk {

enum class StaticPortDirection : std::uint8_t {
    Input,
    Output,
};

// Connection semantics owned by the package descriptor. SampledSignal is
// reserved for RuntimeComponent ports; the existing static composition path
// continues to accept only PureQuery and ContinuousClosureLink.
enum class BindingKind : std::uint8_t {
    Unspecified,
    AssetBinding,
    PureQuery,
    ContinuousClosureLink,
    SampledSignal,
};

[[nodiscard]] constexpr std::string_view to_string(
    BindingKind kind) noexcept {
    switch (kind) {
    case BindingKind::Unspecified:
        return "Unspecified";
    case BindingKind::AssetBinding:
        return "AssetBinding";
    case BindingKind::PureQuery:
        return "PureQuery";
    case BindingKind::ContinuousClosureLink:
        return "ContinuousClosureLink";
    case BindingKind::SampledSignal:
        return "SampledSignal";
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_binding_kind(
    BindingKind kind) noexcept {
    return kind == BindingKind::AssetBinding ||
           kind == BindingKind::PureQuery ||
           kind == BindingKind::ContinuousClosureLink ||
           kind == BindingKind::SampledSignal;
}

enum class PortCardinality : std::uint8_t {
    Unspecified,
    ExactlyOne,
    OneOrMore,
};

[[nodiscard]] constexpr std::string_view to_string(
    PortCardinality cardinality) noexcept {
    switch (cardinality) {
    case PortCardinality::Unspecified:
        return "Unspecified";
    case PortCardinality::ExactlyOne:
        return "exactly-one";
    case PortCardinality::OneOrMore:
        return "one-or-more";
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_port_cardinality(
    PortCardinality cardinality) noexcept {
    return cardinality == PortCardinality::ExactlyOne ||
           cardinality == PortCardinality::OneOrMore;
}

// Pure queries have no sampled/closure time relation. Closure relations retain
// their frozen-interval meaning, while CurrentCycle belongs to sampled runtime
// ports that must later participate in a closed runtime graph.
enum class TemporalRelation : std::uint8_t {
    NotApplicable,
    IntervalModel,
    CandidateStateQuery,
    CurrentCycle,
};

[[nodiscard]] constexpr std::string_view to_string(
    TemporalRelation relation) noexcept {
    switch (relation) {
    case TemporalRelation::NotApplicable:
        return "NotApplicable";
    case TemporalRelation::IntervalModel:
        return "IntervalModel";
    case TemporalRelation::CandidateStateQuery:
        return "CandidateStateQuery";
    case TemporalRelation::CurrentCycle:
        return "CurrentCycle";
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_temporal_relation(
    TemporalRelation relation) noexcept {
    return relation == TemporalRelation::NotApplicable ||
           relation == TemporalRelation::IntervalModel ||
           relation == TemporalRelation::CandidateStateQuery ||
           relation == TemporalRelation::CurrentCycle;
}

// Package-owned placement policy. VehicleProcess identifies an independently
// scheduled RuntimeComponent; lifecycle remains in its tagged runtime facts.
enum class ModelPlacement : std::uint8_t {
    Unspecified,
    VehicleOutput,
    InteractionClosure,
    VehicleProcess,
};

[[nodiscard]] constexpr std::string_view to_string(
    ModelPlacement placement) noexcept {
    switch (placement) {
    case ModelPlacement::Unspecified:
        return "Unspecified";
    case ModelPlacement::VehicleOutput:
        return "vehicle.output";
    case ModelPlacement::InteractionClosure:
        return "interaction/closure";
    case ModelPlacement::VehicleProcess:
        return "vehicle.process";
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_model_placement(
    ModelPlacement placement) noexcept {
    return placement == ModelPlacement::VehicleOutput ||
           placement == ModelPlacement::InteractionClosure ||
           placement == ModelPlacement::VehicleProcess;
}

enum class RuntimeCellProfile : std::uint8_t {
    Unspecified,
    SampledTransform,
};

[[nodiscard]] constexpr std::string_view to_string(
    RuntimeCellProfile profile) noexcept {
    switch (profile) {
    case RuntimeCellProfile::Unspecified:
        return "Unspecified";
    case RuntimeCellProfile::SampledTransform:
        return "SampledTransform";
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_runtime_cell_profile(
    RuntimeCellProfile profile) noexcept {
    return profile == RuntimeCellProfile::SampledTransform;
}

enum class RuntimeExecutionObligation : std::uint8_t {
    BoundaryEvaluation,
};

[[nodiscard]] constexpr std::string_view to_string(
    RuntimeExecutionObligation obligation) noexcept {
    switch (obligation) {
    case RuntimeExecutionObligation::BoundaryEvaluation:
        return "BoundaryEvaluation";
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_runtime_execution_obligation(
    RuntimeExecutionObligation obligation) noexcept {
    return obligation == RuntimeExecutionObligation::BoundaryEvaluation;
}

enum class RuntimeLifecycleCapability : std::uint8_t {
    Instantiate,
    Dispose,
};

[[nodiscard]] constexpr std::string_view to_string(
    RuntimeLifecycleCapability capability) noexcept {
    switch (capability) {
    case RuntimeLifecycleCapability::Instantiate:
        return "Instantiate";
    case RuntimeLifecycleCapability::Dispose:
        return "Dispose";
    }
    return "Unknown";
}

enum class CoarsePhase : std::uint8_t {
    Unspecified,
    Process,
};

[[nodiscard]] constexpr std::string_view to_string(
    CoarsePhase phase) noexcept {
    switch (phase) {
    case CoarsePhase::Unspecified:
        return "Unspecified";
    case CoarsePhase::Process:
        return "process";
    }
    return "Unknown";
}

enum class HoldPolicy : std::uint8_t {
    Unspecified,
    ZeroOrderHold,
};

[[nodiscard]] constexpr std::string_view to_string(
    HoldPolicy policy) noexcept {
    switch (policy) {
    case HoldPolicy::Unspecified:
        return "Unspecified";
    case HoldPolicy::ZeroOrderHold:
        return "ZeroOrderHold";
    }
    return "Unknown";
}

enum class CanonicalConfigValueKind : std::uint8_t {
    String,
    Integer,
    Enum,
    Float64,
};

[[nodiscard]] constexpr std::string_view to_string(
    CanonicalConfigValueKind kind) noexcept {
    switch (kind) {
    case CanonicalConfigValueKind::String:
        return "string";
    case CanonicalConfigValueKind::Integer:
        return "integer";
    case CanonicalConfigValueKind::Enum:
        return "enum";
    case CanonicalConfigValueKind::Float64:
        return "float64";
    }
    return "unknown";
}

[[nodiscard]] constexpr bool valid_canonical_config_value_kind(
    CanonicalConfigValueKind kind) noexcept {
    return kind == CanonicalConfigValueKind::String ||
           kind == CanonicalConfigValueKind::Integer ||
           kind == CanonicalConfigValueKind::Enum ||
           kind == CanonicalConfigValueKind::Float64;
}

struct CanonicalEnumValue {
    std::string token;
};

[[nodiscard]] inline bool operator==(const CanonicalEnumValue& lhs,
                                     const CanonicalEnumValue& rhs) {
    return lhs.token == rhs.token;
}

using CanonicalConfigValue =
    std::variant<std::string, std::int64_t, CanonicalEnumValue, double>;

[[nodiscard]] inline CanonicalConfigValueKind canonical_config_value_kind(
    const CanonicalConfigValue& value) noexcept {
    if (std::holds_alternative<std::string>(value)) {
        return CanonicalConfigValueKind::String;
    }
    if (std::holds_alternative<std::int64_t>(value)) {
        return CanonicalConfigValueKind::Integer;
    }
    if (std::holds_alternative<CanonicalEnumValue>(value)) {
        return CanonicalConfigValueKind::Enum;
    }
    return CanonicalConfigValueKind::Float64;
}

struct CanonicalConfigField {
    std::string field_id;
    CanonicalConfigValue value;
};

[[nodiscard]] inline bool operator==(const CanonicalConfigField& lhs,
                                     const CanonicalConfigField& rhs) {
    return lhs.field_id == rhs.field_id && lhs.value == rhs.value;
}

struct CanonicalConfigBlock {
    std::string schema_id;
    std::uint32_t schema_version = 0U;
    std::vector<CanonicalConfigField> fields;
};

[[nodiscard]] inline bool operator==(const CanonicalConfigBlock& lhs,
                                     const CanonicalConfigBlock& rhs) {
    return lhs.schema_id == rhs.schema_id &&
           lhs.schema_version == rhs.schema_version &&
           lhs.fields == rhs.fields;
}

struct StaticConfigFieldDescriptor {
    std::string field_id;
    CanonicalConfigValueKind value_kind =
        CanonicalConfigValueKind::String;
};

struct StaticConfigSchemaDescriptor {
    std::string schema_id;
    std::uint32_t schema_version = 0U;
    std::vector<StaticConfigFieldDescriptor> fields;
};

struct StaticAssetSlotDescriptor {
    std::string role;
    std::string asset_schema_id;
    PortCardinality cardinality = PortCardinality::ExactlyOne;
};

struct StaticPortDescriptor {
    std::string port_id;
    std::string contract_id;
    StaticPortDirection direction = StaticPortDirection::Input;
    BindingKind binding_kind = BindingKind::Unspecified;
    PortCardinality cardinality = PortCardinality::Unspecified;
    TemporalRelation temporal_relation =
        TemporalRelation::NotApplicable;
};

struct StaticRuntimeScheduleDescriptor {
    CoarsePhase phase = CoarsePhase::Unspecified;
    std::uint32_t step_interval = 0U;
    std::uint32_t offset = 0U;
    HoldPolicy output_hold = HoldPolicy::Unspecified;
    std::uint32_t max_input_age_steps = 0U;
};

// Package-owned static facts for one independent runtime boundary. The
// initial R2 consumer is a stateless SampledTransform, so this narrow shape
// deliberately carries no state-schema surface and lifecycle declares only
// instantiate/dispose.
struct StaticRuntimeComponentDescriptor {
    std::string recipe_id;
    RuntimeCellProfile profile = RuntimeCellProfile::Unspecified;
    std::vector<RuntimeExecutionObligation> obligations;
    StaticRuntimeScheduleDescriptor schedule;
    std::vector<RuntimeLifecycleCapability> lifecycle_capabilities;
    std::string algorithm_entry_id;
    std::string algorithm_entry_version;
};

// Package-owned description of a model that can be read without preparing or
// instantiating it. execution_form is the closed tag: only RuntimeComponent
// may carry runtime_component facts.
struct StaticModelDescriptor {
    ModelDefinitionMetadata definition;
    ModelPlacement placement = ModelPlacement::Unspecified;
    std::string preparation_algorithm_id;
    std::string preparation_algorithm_version;
    StaticConfigSchemaDescriptor configuration;
    std::vector<StaticAssetSlotDescriptor> asset_slots;
    std::vector<StaticPortDescriptor> ports;
    std::optional<StaticRuntimeComponentDescriptor> runtime_component;
};

// A stateless AlgorithmKernel composition is a binding consumer. It has no
// execution-form tag, runtime instance, lifecycle, or mutable state.
struct StaticAlgorithmDescriptor {
    std::string algorithm_id;
    std::string algorithm_version;
    std::vector<StaticPortDescriptor> ports;
};

struct StaticPackageDescriptor {
    std::string package_id;
    std::string package_version;
    std::vector<StaticModelDescriptor> models;
    std::vector<StaticAlgorithmDescriptor> algorithms;
};

} // namespace gnc::model_sdk

#pragma once

#include "gnc/model_sdk/model_metadata.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace gnc::model_sdk {

enum class StaticPortDirection : std::uint8_t {
    Input,
    Output,
};

// Connection semantics owned by the package descriptor. The R2 binding
// slice exposes only the three kinds exercised by current YYZ/CAVH product
// code.
enum class BindingKind : std::uint8_t {
    Unspecified,
    AssetBinding,
    PureQuery,
    ContinuousClosureLink,
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
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_binding_kind(
    BindingKind kind) noexcept {
    return kind == BindingKind::AssetBinding ||
           kind == BindingKind::PureQuery ||
           kind == BindingKind::ContinuousClosureLink;
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

// Pure queries have no sampled/closure time relation. The two closure
// relations below are recognized so the Compiler can reject an accidental
// candidate-state declaration on the current frozen-interval YYZ path.
enum class TemporalRelation : std::uint8_t {
    NotApplicable,
    IntervalModel,
    CandidateStateQuery,
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
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_temporal_relation(
    TemporalRelation relation) noexcept {
    return relation == TemporalRelation::NotApplicable ||
           relation == TemporalRelation::IntervalModel ||
           relation == TemporalRelation::CandidateStateQuery;
}

// Package-owned placement policy for the model forms that have real R1
// consumers. Runtime placement and lifecycle remain outside this descriptor.
enum class ModelPlacement : std::uint8_t {
    Unspecified,
    VehicleOutput,
    InteractionClosure,
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
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_model_placement(
    ModelPlacement placement) noexcept {
    return placement == ModelPlacement::VehicleOutput ||
           placement == ModelPlacement::InteractionClosure;
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

// Package-owned description of a model that can be read without preparing or
// instantiating it. This R2 surface deliberately covers only the PureQuery and
// Closure forms already delivered by real package consumers.
struct StaticModelDescriptor {
    ModelDefinitionMetadata definition;
    ModelPlacement placement = ModelPlacement::Unspecified;
    std::string preparation_algorithm_id;
    std::string preparation_algorithm_version;
    StaticConfigSchemaDescriptor configuration;
    std::vector<StaticAssetSlotDescriptor> asset_slots;
    std::vector<StaticPortDescriptor> ports;
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

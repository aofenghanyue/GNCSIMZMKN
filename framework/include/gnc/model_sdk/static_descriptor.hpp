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
};

struct StaticPortDescriptor {
    std::string port_id;
    std::string contract_id;
    StaticPortDirection direction = StaticPortDirection::Input;
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

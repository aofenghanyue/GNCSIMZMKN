#pragma once

#include "gnc/model_sdk/model_metadata.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace gnc::model_sdk {

enum class StaticPortDirection : std::uint8_t {
    Input,
    Output,
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
    std::string preparation_algorithm_id;
    std::string preparation_algorithm_version;
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

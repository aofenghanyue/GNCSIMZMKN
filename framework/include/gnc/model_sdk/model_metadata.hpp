#pragma once

#include "gnc/foundation/numerical_outcome.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace gnc::model_sdk {

enum class ModelExecutionForm : std::uint8_t {
    Unspecified,
    PureQuery,
    Closure,
    RuntimeComponent,
};

[[nodiscard]] constexpr std::string_view to_string(
    ModelExecutionForm form) noexcept {
    switch (form) {
    case ModelExecutionForm::PureQuery:
        return "PureQuery";
    case ModelExecutionForm::Closure:
        return "Closure";
    case ModelExecutionForm::RuntimeComponent:
        return "RuntimeComponent";
    case ModelExecutionForm::Unspecified:
        return "Unspecified";
    }
    return "Unspecified";
}

[[nodiscard]] constexpr bool valid_model_execution_form(
    ModelExecutionForm form) noexcept {
    return form == ModelExecutionForm::PureQuery ||
           form == ModelExecutionForm::Closure ||
           form == ModelExecutionForm::RuntimeComponent;
}

[[nodiscard]] constexpr bool valid_prepared_model_execution_form(
    ModelExecutionForm form) noexcept {
    return form == ModelExecutionForm::PureQuery ||
           form == ModelExecutionForm::Closure;
}

struct ModelDefinitionMetadata {
    std::string model_id;
    std::string model_version;
    ModelExecutionForm execution_form = ModelExecutionForm::Unspecified;
};

struct PreparedModelMetadata {
    ModelDefinitionMetadata definition;
    std::string preparation_algorithm_id;
    std::string preparation_algorithm_version;
};

inline constexpr gnc::foundation::AlgorithmIdentity
    kModelMetadataPreparationIdentity{
        "gnc.model-sdk.model-metadata.prepare@1", "1.0.0"};

[[nodiscard]] inline gnc::foundation::NumericalOutcome<PreparedModelMetadata>
prepare_model_metadata(
    ModelDefinitionMetadata definition,
    gnc::foundation::AlgorithmIdentity preparation_algorithm) {
    gnc::foundation::NumericalEvidence evidence;
    evidence.algorithm = kModelMetadataPreparationIdentity;

    if (definition.model_id.empty() || definition.model_version.empty()) {
        evidence.detail = "model-identity";
        return gnc::foundation::NumericalOutcome<
            PreparedModelMetadata>::failure(
            gnc::foundation::NumericalStatus::DomainError, evidence);
    }
    if (!valid_prepared_model_execution_form(
            definition.execution_form)) {
        evidence.detail = "model-execution-form";
        return gnc::foundation::NumericalOutcome<
            PreparedModelMetadata>::failure(
            gnc::foundation::NumericalStatus::DomainError, evidence);
    }
    if (preparation_algorithm.id.empty() ||
        preparation_algorithm.version.empty()) {
        evidence.detail = "preparation-identity";
        return gnc::foundation::NumericalOutcome<
            PreparedModelMetadata>::failure(
            gnc::foundation::NumericalStatus::DomainError, evidence);
    }

    evidence.detail = "prepared-model-metadata";
    evidence.evaluations = 1U;
    return gnc::foundation::NumericalOutcome<PreparedModelMetadata>::with_value(
        gnc::foundation::NumericalStatus::Success,
        PreparedModelMetadata{
            std::move(definition), std::string(preparation_algorithm.id),
            std::string(preparation_algorithm.version)},
        evidence);
}

} // namespace gnc::model_sdk

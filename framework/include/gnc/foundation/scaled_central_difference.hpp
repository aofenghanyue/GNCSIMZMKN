#pragma once

#include "gnc/foundation/numerical_outcome.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <string_view>

namespace gnc::foundation {

inline constexpr AlgorithmIdentity kScaledCentralDifferenceIdentity{
    "gnc.foundation.differentiation.scaled-central@1", "1.0.0"};

// binary64 epsilon^(1/3): a scale heuristic for a second-order stencil.
inline constexpr double kDefaultCentralDifferenceRelativeStep =
    6.0554544523933429e-6;

struct DifferentiationDomain {
    double lower = 0.0;
    double upper = 0.0;
};

struct ScaledCentralDifferencePolicy {
    double argument_scale = 1.0;
    double relative_step = kDefaultCentralDifferenceRelativeStep;
};

[[nodiscard]] inline bool valid_scaled_central_difference_policy(
    const ScaledCentralDifferencePolicy& policy) noexcept {
    return std::isfinite(policy.argument_scale) &&
           policy.argument_scale > 0.0 &&
           std::isfinite(policy.relative_step) &&
           policy.relative_step > 0.0;
}

struct CentralDifferenceRiskIndicators {
    // Larger normalized steps expose the estimate to more truncation error.
    double normalized_step = 0.0;
    // Smaller output cancellation ratios expose the numerator to rounding.
    double output_cancellation_ratio = 0.0;
    // Non-zero asymmetry records floating-point rounding of x +/- h.
    double step_asymmetry_ratio = 0.0;
};

struct ScaledCentralDifferenceResult {
    double derivative = 0.0;
    double point = 0.0;
    DifferentiationDomain domain;
    double nominal_argument_scale = 0.0;
    double selected_argument_scale = 0.0;
    double relative_step = 0.0;
    double requested_step = 0.0;
    double lower_argument = 0.0;
    double upper_argument = 0.0;
    double lower_value = 0.0;
    double upper_value = 0.0;
    double lower_step = 0.0;
    double upper_step = 0.0;
    double effective_step = 0.0;
    CentralDifferenceRiskIndicators risk;
};

namespace differentiation_detail {

struct Evaluation {
    NumericalStatus status = NumericalStatus::InternalFailure;
    bool has_value = false;
    double value = 0.0;
    std::string_view detail;
};

[[nodiscard]] constexpr NumericalStatus combine_status(
    NumericalStatus aggregate, NumericalStatus evaluation) noexcept {
    if (aggregate == NumericalStatus::Approximate ||
        evaluation == NumericalStatus::Approximate) {
        return NumericalStatus::Approximate;
    }
    if (aggregate == NumericalStatus::Extrapolated ||
        evaluation == NumericalStatus::Extrapolated) {
        return NumericalStatus::Extrapolated;
    }
    return NumericalStatus::Success;
}

} // namespace differentiation_detail

template <typename Function>
[[nodiscard]] NumericalOutcome<ScaledCentralDifferenceResult>
differentiate_scaled_central(
    double point, DifferentiationDomain domain, Function&& function,
    const ScaledCentralDifferencePolicy& policy =
        ScaledCentralDifferencePolicy{}) {
    NumericalFlags accumulated_flags = 0U;
    NumericalStatus aggregate_status = NumericalStatus::Success;
    std::size_t evaluations = 0U;
    double effective_step = 0.0;
    bool effective_step_available = false;

    const auto evidence = [&](std::string_view detail) {
        NumericalEvidence result;
        result.flags = accumulated_flags;
        result.evaluations = evaluations;
        result.algorithm = kScaledCentralDifferenceIdentity;
        result.detail = detail;
        if (effective_step_available && std::isfinite(effective_step)) {
            result.last_step = effective_step;
        }
        return result;
    };

    const auto failure = [&](NumericalStatus status,
                             std::string_view detail) {
        return NumericalOutcome<ScaledCentralDifferenceResult>::failure(
            status, evidence(detail));
    };

    if (!valid_scaled_central_difference_policy(policy)) {
        return failure(NumericalStatus::DomainError, "policy");
    }
    if (!std::isfinite(domain.lower) || !std::isfinite(domain.upper)) {
        return failure(NumericalStatus::NonFiniteInput, "domain");
    }
    if (!(domain.lower < domain.upper)) {
        return failure(NumericalStatus::DomainError, "domain");
    }
    if (!std::isfinite(point)) {
        return failure(NumericalStatus::NonFiniteInput, "point");
    }
    if (point < domain.lower || point > domain.upper) {
        return failure(NumericalStatus::DomainError,
                       "point-outside-domain");
    }

    const double selected_scale =
        std::max(std::abs(point), policy.argument_scale);
    const double requested_step = policy.relative_step * selected_scale;
    if (!std::isfinite(requested_step)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "requested-step");
    }
    if (!(requested_step > 0.0)) {
        return failure(NumericalStatus::StepUnderflow,
                       "requested-step-underflow");
    }

    const double lower_argument = point - requested_step;
    const double upper_argument = point + requested_step;
    if (!std::isfinite(lower_argument) ||
        !std::isfinite(upper_argument)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "sample-arguments");
    }
    if (!(lower_argument < point) || !(point < upper_argument)) {
        return failure(NumericalStatus::StepUnderflow,
                       "unrepresentable-central-step");
    }
    if (lower_argument < domain.lower || upper_argument > domain.upper) {
        return failure(NumericalStatus::DomainError,
                       "central-samples-outside-domain");
    }

    const double lower_step = point - lower_argument;
    const double upper_step = upper_argument - point;
    effective_step = 0.5 * lower_step + 0.5 * upper_step;
    effective_step_available = true;
    if (!std::isfinite(effective_step) || !(effective_step > 0.0)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "effective-step");
    }

    const auto evaluate = [&](double argument) {
        ++evaluations;
        auto outcome = std::invoke(function, argument);
        accumulated_flags |= outcome.evidence().flags;
        if (!outcome.has_value()) {
            return differentiation_detail::Evaluation{
                outcome.status(), false, 0.0,
                outcome.evidence().detail};
        }
        if (!std::isfinite(outcome.value())) {
            return differentiation_detail::Evaluation{
                NumericalStatus::NonFiniteIntermediate, false, 0.0,
                "function-value"};
        }
        aggregate_status = differentiation_detail::combine_status(
            aggregate_status, outcome.status());
        return differentiation_detail::Evaluation{
            outcome.status(), true, outcome.value(),
            outcome.evidence().detail};
    };

    const differentiation_detail::Evaluation lower_evaluation =
        evaluate(lower_argument);
    if (!lower_evaluation.has_value) {
        return failure(lower_evaluation.status,
                       lower_evaluation.detail.empty()
                           ? std::string_view{"function-lower"}
                           : lower_evaluation.detail);
    }
    const differentiation_detail::Evaluation upper_evaluation =
        evaluate(upper_argument);
    if (!upper_evaluation.has_value) {
        return failure(upper_evaluation.status,
                       upper_evaluation.detail.empty()
                           ? std::string_view{"function-upper"}
                           : upper_evaluation.detail);
    }

    const double function_difference =
        upper_evaluation.value - lower_evaluation.value;
    if (!std::isfinite(function_difference)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "function-difference");
    }
    const double derivative =
        (0.5 * function_difference) / effective_step;
    if (!std::isfinite(derivative)) {
        return failure(NumericalStatus::NonFiniteOutput, "derivative");
    }

    const double value_scale =
        std::max(std::abs(lower_evaluation.value),
                 std::abs(upper_evaluation.value));
    const double cancellation_ratio =
        value_scale > 0.0
            ? std::abs(function_difference) / value_scale
            : 0.0;
    const double normalized_step = effective_step / selected_scale;
    const double asymmetry_ratio =
        std::abs(upper_step - lower_step) / effective_step;
    if (!std::isfinite(cancellation_ratio) ||
        !std::isfinite(normalized_step) ||
        !std::isfinite(asymmetry_ratio)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "risk-indicators");
    }

    NumericalEvidence result_evidence = evidence(
        "scaled-central-difference");
    const ScaledCentralDifferenceResult result{
        derivative,
        point,
        domain,
        policy.argument_scale,
        selected_scale,
        policy.relative_step,
        requested_step,
        lower_argument,
        upper_argument,
        lower_evaluation.value,
        upper_evaluation.value,
        lower_step,
        upper_step,
        effective_step,
        CentralDifferenceRiskIndicators{
            normalized_step, cancellation_ratio, asymmetry_ratio}};
    return NumericalOutcome<ScaledCentralDifferenceResult>::with_value(
        aggregate_status, result, result_evidence);
}

} // namespace gnc::foundation

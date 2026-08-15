#pragma once

#include "gnc/foundation/scaled_central_difference.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace gnc::foundation {

inline constexpr AlgorithmIdentity kScaledOneSidedDifferenceIdentity{
    "gnc.foundation.differentiation.scaled-one-sided-second-order@1",
    "1.0.0"};

using ScaledOneSidedDifferencePolicy = ScaledDifferencePolicy;

enum class OneSidedDirection : std::uint8_t {
    Forward,
    Backward,
};

[[nodiscard]] constexpr std::string_view to_string(
    OneSidedDirection direction) noexcept {
    switch (direction) {
    case OneSidedDirection::Forward:
        return "Forward";
    case OneSidedDirection::Backward:
        return "Backward";
    }
    return "Invalid";
}

struct OneSidedDifferenceRiskIndicators {
    double normalized_step = 0.0;
    double nearest_output_cancellation_ratio = 0.0;
    double far_output_cancellation_ratio = 0.0;
    double derivative_combination_ratio = 0.0;
    double spacing_ratio_error = 0.0;
};

struct ScaledOneSidedDifferenceResult {
    double derivative = 0.0;
    OneSidedDirection direction = OneSidedDirection::Forward;
    double point = 0.0;
    DifferentiationDomain domain;
    double nominal_argument_scale = 0.0;
    double selected_argument_scale = 0.0;
    double relative_step = 0.0;
    double requested_step = 0.0;
    double nearest_argument = 0.0;
    double far_argument = 0.0;
    double point_value = 0.0;
    double nearest_value = 0.0;
    double far_value = 0.0;
    double nearest_offset = 0.0;
    double far_offset = 0.0;
    double effective_step = 0.0;
    double spacing_ratio = 0.0;
    OneSidedDifferenceRiskIndicators risk;
};

template <typename Function>
[[nodiscard]] NumericalOutcome<ScaledOneSidedDifferenceResult>
differentiate_scaled_one_sided(
    double point, DifferentiationDomain domain,
    OneSidedDirection direction, Function&& function,
    const ScaledOneSidedDifferencePolicy& policy =
        ScaledOneSidedDifferencePolicy{}) {
    NumericalFlags accumulated_flags = 0U;
    NumericalStatus aggregate_status = NumericalStatus::Success;
    std::size_t evaluations = 0U;
    double effective_step = 0.0;
    bool effective_step_available = false;

    const auto evidence = [&](std::string_view detail) {
        NumericalEvidence result;
        result.flags = accumulated_flags;
        result.evaluations = evaluations;
        result.algorithm = kScaledOneSidedDifferenceIdentity;
        result.detail = detail;
        if (effective_step_available && std::isfinite(effective_step)) {
            result.last_step = effective_step;
        }
        return result;
    };

    const auto failure = [&](NumericalStatus status,
                             std::string_view detail) {
        return NumericalOutcome<ScaledOneSidedDifferenceResult>::failure(
            status, evidence(detail));
    };

    if (!valid_scaled_difference_policy(policy)) {
        return failure(NumericalStatus::DomainError, "policy");
    }
    double direction_sign = 0.0;
    switch (direction) {
    case OneSidedDirection::Forward:
        direction_sign = 1.0;
        break;
    case OneSidedDirection::Backward:
        direction_sign = -1.0;
        break;
    default:
        return failure(NumericalStatus::DomainError, "direction");
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
    const double signed_step = direction_sign * requested_step;
    const double nearest_argument = point + signed_step;
    const double far_argument = nearest_argument + signed_step;
    if (!std::isfinite(nearest_argument) ||
        !std::isfinite(far_argument)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "sample-arguments");
    }
    if (nearest_argument == point) {
        return failure(NumericalStatus::StepUnderflow,
                       "unrepresentable-nearest-step");
    }
    if (far_argument == nearest_argument || far_argument == point) {
        return failure(NumericalStatus::StepUnderflow,
                       "unrepresentable-far-step");
    }
    const bool ordered =
        direction == OneSidedDirection::Forward
            ? point < nearest_argument && nearest_argument < far_argument
            : far_argument < nearest_argument && nearest_argument < point;
    if (!ordered) {
        return failure(NumericalStatus::InternalFailure,
                       "sample-order");
    }
    if (nearest_argument < domain.lower ||
        nearest_argument > domain.upper || far_argument < domain.lower ||
        far_argument > domain.upper) {
        return failure(NumericalStatus::DomainError,
                       "one-sided-samples-outside-domain");
    }

    const double nearest_offset = nearest_argument - point;
    const double far_offset = far_argument - point;
    effective_step = std::abs(nearest_offset);
    effective_step_available = true;
    const double spacing_ratio = far_offset / nearest_offset;
    if (!std::isfinite(effective_step) || !(effective_step > 0.0) ||
        !std::isfinite(spacing_ratio) || !(spacing_ratio > 1.0)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "sample-spacing");
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

    const differentiation_detail::Evaluation point_evaluation =
        evaluate(point);
    if (!point_evaluation.has_value) {
        return failure(point_evaluation.status,
                       point_evaluation.detail.empty()
                           ? std::string_view{"function-point"}
                           : point_evaluation.detail);
    }
    const differentiation_detail::Evaluation nearest_evaluation =
        evaluate(nearest_argument);
    if (!nearest_evaluation.has_value) {
        return failure(nearest_evaluation.status,
                       nearest_evaluation.detail.empty()
                           ? std::string_view{"function-nearest"}
                           : nearest_evaluation.detail);
    }
    const differentiation_detail::Evaluation far_evaluation =
        evaluate(far_argument);
    if (!far_evaluation.has_value) {
        return failure(far_evaluation.status,
                       far_evaluation.detail.empty()
                           ? std::string_view{"function-far"}
                           : far_evaluation.detail);
    }

    const double nearest_difference =
        nearest_evaluation.value - point_evaluation.value;
    const double far_difference =
        far_evaluation.value - point_evaluation.value;
    if (!std::isfinite(nearest_difference) ||
        !std::isfinite(far_difference)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "function-difference");
    }
    const double nearest_slope = nearest_difference / nearest_offset;
    const double far_slope = far_difference / far_offset;
    if (!std::isfinite(nearest_slope) || !std::isfinite(far_slope)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "secant-slope");
    }
    const double scaled_nearest_slope = spacing_ratio * nearest_slope;
    const double derivative_combination =
        scaled_nearest_slope - far_slope;
    if (!std::isfinite(scaled_nearest_slope) ||
        !std::isfinite(derivative_combination)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "derivative-combination");
    }
    const double derivative =
        derivative_combination / (spacing_ratio - 1.0);
    if (!std::isfinite(derivative)) {
        return failure(NumericalStatus::NonFiniteOutput, "derivative");
    }

    const auto cancellation_ratio = [](double difference, double first,
                                       double second) {
        const double scale =
            std::max(std::abs(first), std::abs(second));
        return scale > 0.0 ? std::abs(difference) / scale : 0.0;
    };
    const double nearest_cancellation = cancellation_ratio(
        nearest_difference, point_evaluation.value,
        nearest_evaluation.value);
    const double far_cancellation = cancellation_ratio(
        far_difference, point_evaluation.value, far_evaluation.value);
    const double derivative_scale =
        std::max(std::abs(scaled_nearest_slope),
                 std::abs(far_slope));
    const double derivative_cancellation =
        derivative_scale > 0.0
            ? std::abs(derivative_combination) / derivative_scale
            : 0.0;
    const double normalized_step = effective_step / selected_scale;
    const double spacing_ratio_error = std::abs(spacing_ratio - 2.0);
    if (!std::isfinite(nearest_cancellation) ||
        !std::isfinite(far_cancellation) ||
        !std::isfinite(derivative_cancellation) ||
        !std::isfinite(normalized_step) ||
        !std::isfinite(spacing_ratio_error)) {
        return failure(NumericalStatus::NonFiniteIntermediate,
                       "risk-indicators");
    }

    NumericalEvidence result_evidence = evidence(
        "scaled-one-sided-second-order");
    const ScaledOneSidedDifferenceResult result{
        derivative,
        direction,
        point,
        domain,
        policy.argument_scale,
        selected_scale,
        policy.relative_step,
        requested_step,
        nearest_argument,
        far_argument,
        point_evaluation.value,
        nearest_evaluation.value,
        far_evaluation.value,
        nearest_offset,
        far_offset,
        effective_step,
        spacing_ratio,
        OneSidedDifferenceRiskIndicators{
            normalized_step,
            nearest_cancellation,
            far_cancellation,
            derivative_cancellation,
            spacing_ratio_error}};
    return NumericalOutcome<ScaledOneSidedDifferenceResult>::with_value(
        aggregate_status, result, result_evidence);
}

} // namespace gnc::foundation

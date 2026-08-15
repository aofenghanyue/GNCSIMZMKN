#pragma once

#include "gnc/foundation/numerical_outcome.hpp"

#include <algorithm>
#include <cmath>

namespace gnc::foundation {

enum class FiniteCheck : std::uint8_t {
    Disabled,
    InputAndOutput,
    EveryStage,
};

struct NumericalPolicy {
    double absolute_tolerance = 1.0e-12;
    double relative_tolerance = 1.0e-9;
    FiniteCheck finite_check = FiniteCheck::EveryStage;
    double zero_tolerance = 1.0e-14;
    double condition_limit = 1.0e12;
};

[[nodiscard]] inline bool valid_numerical_policy(
    const NumericalPolicy& policy) noexcept {
    if (!std::isfinite(policy.absolute_tolerance) ||
        !std::isfinite(policy.relative_tolerance) ||
        !std::isfinite(policy.zero_tolerance) ||
        !std::isfinite(policy.condition_limit) ||
        policy.absolute_tolerance < 0.0 || policy.relative_tolerance < 0.0) {
        return false;
    }
    if (policy.zero_tolerance < 0.0 || policy.condition_limit <= 0.0) {
        return false;
    }
    switch (policy.finite_check) {
    case FiniteCheck::Disabled:
    case FiniteCheck::InputAndOutput:
    case FiniteCheck::EveryStage:
        return true;
    }
    return false;
}

struct ToleranceComparison {
    double actual = 0.0;
    double reference = 0.0;
    double absolute_error = 0.0;
    double relative_error = 0.0;
    double limit = 0.0;
    bool accepted = false;
};

inline constexpr AlgorithmIdentity kAbsoluteRelativeToleranceIdentity{
    "gnc.foundation.tolerance.absolute-relative@1", "1.0.0"};

[[nodiscard]] inline NumericalOutcome<ToleranceComparison>
compare_with_tolerance(double actual, double reference,
                       const NumericalPolicy& policy) {
    NumericalEvidence evidence;
    evidence.algorithm = kAbsoluteRelativeToleranceIdentity;

    if (!valid_numerical_policy(policy)) {
        evidence.detail = "policy";
        return NumericalOutcome<ToleranceComparison>::failure(
            NumericalStatus::DomainError, evidence);
    }
    if (!std::isfinite(actual) || !std::isfinite(reference)) {
        evidence.detail = "value";
        return NumericalOutcome<ToleranceComparison>::failure(
            NumericalStatus::NonFiniteInput, evidence);
    }

    const double absolute_error = std::abs(actual - reference);
    const double scale = std::max(std::abs(actual), std::abs(reference));
    const double limit =
        policy.absolute_tolerance + policy.relative_tolerance * scale;
    const double relative_error = scale > 0.0 ? absolute_error / scale : 0.0;
    if (!std::isfinite(absolute_error) || !std::isfinite(limit) ||
        !std::isfinite(relative_error)) {
        evidence.detail = "comparison";
        return NumericalOutcome<ToleranceComparison>::failure(
            NumericalStatus::NonFiniteIntermediate, evidence);
    }

    evidence.evaluations = 1U;
    evidence.estimated_abs_error = absolute_error;
    evidence.estimated_rel_error = relative_error;
    return NumericalOutcome<ToleranceComparison>::with_value(
        NumericalStatus::Success,
        ToleranceComparison{actual, reference, absolute_error, relative_error,
                            limit, absolute_error <= limit},
        evidence);
}

} // namespace gnc::foundation

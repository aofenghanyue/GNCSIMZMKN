#pragma once

#include "gnc/foundation/numerical_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace gnc::foundation {

inline constexpr AlgorithmIdentity kBracketedBisectionIdentity{
    "gnc.foundation.root.bracketed-bisection@1", "1.0.0"};

struct BracketedRootPolicy {
    NumericalPolicy argument_tolerance{1.0e-12, 1.0e-10,
                                       FiniteCheck::EveryStage};
    double residual_absolute_tolerance = 1.0e-12;
    std::size_t max_iterations = 100U;
};

[[nodiscard]] inline bool valid_bracketed_root_policy(
    const BracketedRootPolicy& policy) noexcept {
    return valid_numerical_policy(policy.argument_tolerance) &&
           std::isfinite(policy.residual_absolute_tolerance) &&
           policy.residual_absolute_tolerance >= 0.0 &&
           policy.max_iterations > 0U;
}

enum class RootStopReason : std::uint8_t {
    ExactLowerEndpoint,
    ExactUpperEndpoint,
    ExactEvaluation,
    ResidualTolerance,
    BracketTolerance,
};

[[nodiscard]] constexpr std::string_view to_string(
    RootStopReason reason) noexcept {
    switch (reason) {
    case RootStopReason::ExactLowerEndpoint:
        return "ExactLowerEndpoint";
    case RootStopReason::ExactUpperEndpoint:
        return "ExactUpperEndpoint";
    case RootStopReason::ExactEvaluation:
        return "ExactEvaluation";
    case RootStopReason::ResidualTolerance:
        return "ResidualTolerance";
    case RootStopReason::BracketTolerance:
        return "BracketTolerance";
    }
    return "ResidualTolerance";
}

struct RootBracket {
    double lower = 0.0;
    double upper = 0.0;
    double lower_value = 0.0;
    double upper_value = 0.0;
};

struct BracketedRootResult {
    double root = 0.0;
    double function_value = 0.0;
    RootBracket bracket;
    RootStopReason stop_reason = RootStopReason::ResidualTolerance;
};

namespace detail {

struct ScalarFunctionEvaluation {
    NumericalStatus status = NumericalStatus::InternalFailure;
    bool has_value = false;
    double value = 0.0;
    NumericalFlags flags = 0U;
    std::string_view detail;
};

[[nodiscard]] constexpr NumericalStatus combine_root_evaluation_status(
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

[[nodiscard]] inline bool opposite_nonzero_signs(double lhs,
                                                 double rhs) noexcept {
    return std::signbit(lhs) != std::signbit(rhs);
}

[[nodiscard]] inline double safe_midpoint(double lower,
                                          double upper) noexcept {
    const double width = upper - lower;
    if (std::isfinite(width)) {
        return lower + 0.5 * width;
    }
    return 0.5 * lower + 0.5 * upper;
}

[[nodiscard]] inline double bracket_width(double lower,
                                          double upper) noexcept {
    return upper - lower;
}

[[nodiscard]] inline double argument_tolerance(
    const BracketedRootPolicy& policy, double lower, double upper) noexcept {
    const double scale = std::max(std::abs(lower), std::abs(upper));
    return policy.argument_tolerance.absolute_tolerance +
           policy.argument_tolerance.relative_tolerance * scale;
}

} // namespace detail

template <typename Function>
[[nodiscard]] NumericalOutcome<BracketedRootResult>
solve_bracketed_root_bisection(
    double initial_lower, double initial_upper, Function&& function,
    const BracketedRootPolicy& policy = BracketedRootPolicy{}) {
    NumericalFlags accumulated_flags = 0U;
    NumericalStatus aggregate_status = NumericalStatus::Success;
    std::size_t iterations = 0U;
    std::size_t evaluations = 0U;
    double lower = initial_lower;
    double upper = initial_upper;
    double lower_value = 0.0;
    double upper_value = 0.0;
    bool bracket_available = false;

    const auto best_residual = [&]() -> std::optional<double> {
        if (!bracket_available) {
            return std::nullopt;
        }
        return std::min(std::abs(lower_value), std::abs(upper_value));
    };

    const auto evidence = [&](std::string_view detail_text) {
        NumericalEvidence result;
        result.flags = accumulated_flags;
        result.iterations = iterations;
        result.evaluations = evaluations;
        result.algorithm = kBracketedBisectionIdentity;
        result.detail = detail_text;
        if (bracket_available) {
            result.last_bracket_lower = lower;
            result.last_bracket_upper = upper;
            const double width = detail::bracket_width(lower, upper);
            if (std::isfinite(width)) {
                result.last_step = width;
            }
        }
        const std::optional<double> residual = best_residual();
        if (residual.has_value() && std::isfinite(*residual)) {
            result.residual_norm = *residual;
        }
        return result;
    };

    const auto failure = [&](NumericalStatus status,
                             std::string_view detail_text) {
        NumericalEvidence result_evidence = evidence(detail_text);
        if ((status == NumericalStatus::MaxIterations ||
             status == NumericalStatus::ToleranceUnreachable) &&
            result_evidence.last_step.has_value()) {
            result_evidence.estimated_abs_error =
                *result_evidence.last_step;
        }
        return NumericalOutcome<BracketedRootResult>::failure(
            status, result_evidence);
    };

    const auto evaluate = [&](double argument) {
        ++evaluations;
        auto outcome = std::invoke(function, argument);
        accumulated_flags |= outcome.evidence().flags;
        if (!outcome.has_value()) {
            return detail::ScalarFunctionEvaluation{
                outcome.status(), false, 0.0, outcome.evidence().flags,
                outcome.evidence().detail};
        }
        if (!std::isfinite(outcome.value())) {
            return detail::ScalarFunctionEvaluation{
                NumericalStatus::NonFiniteIntermediate, false, 0.0,
                outcome.evidence().flags, "function-value"};
        }
        aggregate_status = detail::combine_root_evaluation_status(
            aggregate_status, outcome.status());
        return detail::ScalarFunctionEvaluation{
            outcome.status(), true, outcome.value(),
            outcome.evidence().flags, outcome.evidence().detail};
    };

    const auto success = [&](double root, double function_value,
                             const RootBracket& bracket,
                             RootStopReason stop_reason) {
        bracket_available = true;
        lower = bracket.lower;
        upper = bracket.upper;
        lower_value = bracket.lower_value;
        upper_value = bracket.upper_value;
        NumericalEvidence result_evidence = evidence(to_string(stop_reason));
        result_evidence.residual_norm = std::abs(function_value);
        if (stop_reason == RootStopReason::ExactLowerEndpoint ||
            stop_reason == RootStopReason::ExactUpperEndpoint ||
            stop_reason == RootStopReason::ExactEvaluation) {
            result_evidence.estimated_abs_error = 0.0;
        } else if (bracket.lower < bracket.upper) {
            const double width = bracket.upper - bracket.lower;
            if (std::isfinite(width)) {
                result_evidence.estimated_abs_error = width;
            }
        }
        const NumericalStatus result_status =
            aggregate_status == NumericalStatus::Success
            ? NumericalStatus::Converged
            : aggregate_status;
        return NumericalOutcome<BracketedRootResult>::with_value(
            result_status,
            BracketedRootResult{root, function_value, bracket, stop_reason},
            result_evidence);
    };

    if (!valid_bracketed_root_policy(policy)) {
        return failure(NumericalStatus::DomainError, "policy");
    }
    if (!std::isfinite(lower) || !std::isfinite(upper)) {
        return failure(NumericalStatus::NonFiniteInput, "bracket");
    }
    if (!(lower < upper)) {
        return failure(NumericalStatus::DomainError, "bracket");
    }

    const detail::ScalarFunctionEvaluation lower_evaluation = evaluate(lower);
    if (!lower_evaluation.has_value) {
        return failure(lower_evaluation.status,
                       lower_evaluation.detail.empty()
                           ? std::string_view{"function-lower"}
                           : lower_evaluation.detail);
    }
    lower_value = lower_evaluation.value;
    if (lower_value == 0.0) {
        return success(
            lower, lower_value,
            RootBracket{lower, lower, lower_value, lower_value},
            RootStopReason::ExactLowerEndpoint);
    }
    if (std::abs(lower_value) <= policy.residual_absolute_tolerance) {
        return success(
            lower, lower_value,
            RootBracket{lower, lower, lower_value, lower_value},
            RootStopReason::ResidualTolerance);
    }

    const detail::ScalarFunctionEvaluation upper_evaluation = evaluate(upper);
    if (!upper_evaluation.has_value) {
        return failure(upper_evaluation.status,
                       upper_evaluation.detail.empty()
                           ? std::string_view{"function-upper"}
                           : upper_evaluation.detail);
    }
    upper_value = upper_evaluation.value;
    bracket_available = true;
    if (upper_value == 0.0) {
        return success(upper, upper_value,
                       RootBracket{lower, upper, lower_value, upper_value},
                       RootStopReason::ExactUpperEndpoint);
    }
    if (std::abs(upper_value) <= policy.residual_absolute_tolerance) {
        return success(upper, upper_value,
                       RootBracket{lower, upper, lower_value, upper_value},
                       RootStopReason::ResidualTolerance);
    }
    if (!detail::opposite_nonzero_signs(lower_value, upper_value)) {
        return failure(NumericalStatus::NoBracket, "same-sign-endpoints");
    }

    for (iterations = 0U; iterations < policy.max_iterations;) {
        const double width = detail::bracket_width(lower, upper);
        const double tolerance = detail::argument_tolerance(
            policy, lower, upper);
        if (!std::isfinite(tolerance)) {
            return failure(NumericalStatus::NonFiniteIntermediate,
                           "argument-tolerance");
        }
        if (std::isfinite(width) && width <= tolerance) {
            const bool lower_is_best =
                std::abs(lower_value) <= std::abs(upper_value);
            return success(
                lower_is_best ? lower : upper,
                lower_is_best ? lower_value : upper_value,
                RootBracket{lower, upper, lower_value, upper_value},
                RootStopReason::BracketTolerance);
        }

        const double midpoint = detail::safe_midpoint(lower, upper);
        if (!std::isfinite(midpoint)) {
            return failure(NumericalStatus::NonFiniteIntermediate,
                           "midpoint");
        }
        if (!(lower < midpoint && midpoint < upper)) {
            return failure(NumericalStatus::ToleranceUnreachable,
                           "no-representable-midpoint");
        }

        const detail::ScalarFunctionEvaluation midpoint_evaluation =
            evaluate(midpoint);
        ++iterations;
        if (!midpoint_evaluation.has_value) {
            return failure(midpoint_evaluation.status,
                           midpoint_evaluation.detail.empty()
                               ? std::string_view{"function-midpoint"}
                               : midpoint_evaluation.detail);
        }
        const double midpoint_value = midpoint_evaluation.value;
        if (midpoint_value == 0.0) {
            return success(
                midpoint, midpoint_value,
                RootBracket{lower, upper, lower_value, upper_value},
                RootStopReason::ExactEvaluation);
        }
        if (std::abs(midpoint_value) <=
            policy.residual_absolute_tolerance) {
            return success(
                midpoint, midpoint_value,
                RootBracket{lower, upper, lower_value, upper_value},
                RootStopReason::ResidualTolerance);
        }

        if (detail::opposite_nonzero_signs(lower_value, midpoint_value)) {
            upper = midpoint;
            upper_value = midpoint_value;
        } else {
            lower = midpoint;
            lower_value = midpoint_value;
        }
    }

    return failure(NumericalStatus::MaxIterations, "max-iterations");
}

} // namespace gnc::foundation

#pragma once

#include "gnc/foundation/numerical_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace gnc::foundation {

inline constexpr AlgorithmIdentity kLocalNewtonRootIdentity{
    "gnc.foundation.root.local-newton@1", "1.0.0"};

struct LocalRootDomain {
    double lower = 0.0;
    double upper = 0.0;
};

struct LocalRootLinearization {
    double function_value = 0.0;
    double derivative_value = 0.0;
};

struct LocalNewtonRootPolicy {
    NumericalPolicy argument_tolerance{1.0e-12, 1.0e-10,
                                       FiniteCheck::EveryStage};
    double residual_absolute_tolerance = 1.0e-12;
    double derivative_minimum_absolute = 1.0e-14;
    std::size_t max_iterations = 20U;
};

[[nodiscard]] inline bool valid_local_newton_root_policy(
    const LocalNewtonRootPolicy& policy) noexcept {
    return valid_numerical_policy(policy.argument_tolerance) &&
           std::isfinite(policy.residual_absolute_tolerance) &&
           policy.residual_absolute_tolerance >= 0.0 &&
           std::isfinite(policy.derivative_minimum_absolute) &&
           policy.derivative_minimum_absolute >= 0.0 &&
           policy.max_iterations > 0U;
}

enum class LocalRootStopReason : std::uint8_t {
    ExactInitialGuess,
    ExactEvaluation,
    ResidualTolerance,
    StepTolerance,
};

[[nodiscard]] constexpr std::string_view to_string(
    LocalRootStopReason reason) noexcept {
    switch (reason) {
    case LocalRootStopReason::ExactInitialGuess:
        return "ExactInitialGuess";
    case LocalRootStopReason::ExactEvaluation:
        return "ExactEvaluation";
    case LocalRootStopReason::ResidualTolerance:
        return "ResidualTolerance";
    case LocalRootStopReason::StepTolerance:
        return "StepTolerance";
    }
    return "ResidualTolerance";
}

struct LocalNewtonRootResult {
    double initial_guess = 0.0;
    double root = 0.0;
    double function_value = 0.0;
    double derivative_value = 0.0;
    double last_step = 0.0;
    LocalRootDomain domain;
    LocalRootStopReason stop_reason =
        LocalRootStopReason::ResidualTolerance;
};

namespace local_root_detail {

struct Evaluation {
    NumericalStatus status = NumericalStatus::InternalFailure;
    bool has_value = false;
    LocalRootLinearization linearization;
    NumericalFlags flags = 0U;
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

[[nodiscard]] inline double argument_tolerance(
    const LocalNewtonRootPolicy& policy, double current,
    double candidate) noexcept {
    const double scale = std::max(std::abs(current), std::abs(candidate));
    return policy.argument_tolerance.absolute_tolerance +
           policy.argument_tolerance.relative_tolerance * scale;
}

} // namespace local_root_detail

template <typename LinearizationFunction>
[[nodiscard]] NumericalOutcome<LocalNewtonRootResult>
solve_local_root_newton(
    double initial_guess, LocalRootDomain domain,
    LinearizationFunction&& linearize,
    const LocalNewtonRootPolicy& policy = LocalNewtonRootPolicy{}) {
    NumericalFlags accumulated_flags = 0U;
    NumericalStatus aggregate_status = NumericalStatus::Success;
    std::size_t iterations = 0U;
    std::size_t evaluations = 0U;
    double current = initial_guess;
    LocalRootLinearization current_linearization;
    bool linearization_available = false;
    double last_step = 0.0;
    bool last_step_available = false;

    const auto evidence = [&](std::string_view detail_text) {
        NumericalEvidence result;
        result.flags = accumulated_flags;
        result.iterations = iterations;
        result.evaluations = evaluations;
        result.algorithm = kLocalNewtonRootIdentity;
        result.detail = detail_text;
        if (linearization_available &&
            std::isfinite(current_linearization.function_value)) {
            result.residual_norm =
                std::abs(current_linearization.function_value);
        }
        if (last_step_available && std::isfinite(last_step)) {
            result.last_step = last_step;
            result.estimated_abs_error = std::abs(last_step);
        }
        return result;
    };

    const auto failure = [&](NumericalStatus status,
                             std::string_view detail_text) {
        return NumericalOutcome<LocalNewtonRootResult>::failure(
            status, evidence(detail_text));
    };

    const auto evaluate = [&](double argument) {
        ++evaluations;
        auto outcome = std::invoke(linearize, argument);
        accumulated_flags |= outcome.evidence().flags;
        if (!outcome.has_value()) {
            return local_root_detail::Evaluation{
                outcome.status(), false, {}, outcome.evidence().flags,
                outcome.evidence().detail};
        }
        if (!std::isfinite(outcome.value().function_value)) {
            return local_root_detail::Evaluation{
                NumericalStatus::NonFiniteIntermediate, false, {},
                outcome.evidence().flags, "function-value"};
        }
        if (!std::isfinite(outcome.value().derivative_value)) {
            return local_root_detail::Evaluation{
                NumericalStatus::NonFiniteIntermediate, false, {},
                outcome.evidence().flags, "function-derivative"};
        }
        aggregate_status = local_root_detail::combine_status(
            aggregate_status, outcome.status());
        return local_root_detail::Evaluation{
            outcome.status(), true, outcome.value(),
            outcome.evidence().flags, outcome.evidence().detail};
    };

    const auto success = [&](LocalRootStopReason stop_reason) {
        NumericalEvidence result_evidence = evidence(to_string(stop_reason));
        result_evidence.residual_norm =
            std::abs(current_linearization.function_value);
        if (stop_reason == LocalRootStopReason::ExactInitialGuess ||
            stop_reason == LocalRootStopReason::ExactEvaluation) {
            result_evidence.estimated_abs_error = 0.0;
        }
        const NumericalStatus result_status =
            aggregate_status == NumericalStatus::Success
                ? NumericalStatus::Converged
                : aggregate_status;
        return NumericalOutcome<LocalNewtonRootResult>::with_value(
            result_status,
            LocalNewtonRootResult{
                initial_guess,
                current,
                current_linearization.function_value,
                current_linearization.derivative_value,
                last_step_available ? last_step : 0.0,
                domain,
                stop_reason},
            result_evidence);
    };

    if (!valid_local_newton_root_policy(policy)) {
        return failure(NumericalStatus::DomainError, "policy");
    }
    if (!std::isfinite(domain.lower) || !std::isfinite(domain.upper)) {
        return failure(NumericalStatus::NonFiniteInput, "domain");
    }
    if (!(domain.lower < domain.upper)) {
        return failure(NumericalStatus::DomainError, "domain");
    }
    if (!std::isfinite(initial_guess)) {
        return failure(NumericalStatus::NonFiniteInput, "initial-guess");
    }
    if (initial_guess < domain.lower || initial_guess > domain.upper) {
        return failure(NumericalStatus::DomainError,
                       "initial-guess-outside-domain");
    }

    const local_root_detail::Evaluation initial_evaluation =
        evaluate(current);
    if (!initial_evaluation.has_value) {
        return failure(initial_evaluation.status,
                       initial_evaluation.detail.empty()
                           ? std::string_view{"linearization-initial"}
                           : initial_evaluation.detail);
    }
    current_linearization = initial_evaluation.linearization;
    linearization_available = true;
    if (current_linearization.function_value == 0.0) {
        return success(LocalRootStopReason::ExactInitialGuess);
    }
    if (std::abs(current_linearization.function_value) <=
        policy.residual_absolute_tolerance) {
        return success(LocalRootStopReason::ResidualTolerance);
    }

    for (iterations = 0U; iterations < policy.max_iterations;) {
        if (std::abs(current_linearization.derivative_value) <=
            policy.derivative_minimum_absolute) {
            return failure(NumericalStatus::Singular,
                           "derivative-below-threshold");
        }

        const double step = -current_linearization.function_value /
                            current_linearization.derivative_value;
        last_step = step;
        last_step_available = true;
        if (!std::isfinite(step)) {
            return failure(NumericalStatus::NonFiniteIntermediate,
                           "newton-step");
        }
        const double candidate = current + step;
        if (!std::isfinite(candidate)) {
            return failure(NumericalStatus::NonFiniteIntermediate,
                           "newton-candidate");
        }
        if (candidate == current) {
            return failure(NumericalStatus::ToleranceUnreachable,
                           "no-representable-newton-step");
        }
        if (candidate < domain.lower || candidate > domain.upper) {
            return failure(NumericalStatus::DomainError,
                           "newton-step-outside-domain");
        }

        const double tolerance = local_root_detail::argument_tolerance(
            policy, current, candidate);
        if (!std::isfinite(tolerance)) {
            return failure(NumericalStatus::NonFiniteIntermediate,
                           "argument-tolerance");
        }

        const local_root_detail::Evaluation candidate_evaluation =
            evaluate(candidate);
        ++iterations;
        if (!candidate_evaluation.has_value) {
            return failure(candidate_evaluation.status,
                           candidate_evaluation.detail.empty()
                               ? std::string_view{"linearization-candidate"}
                               : candidate_evaluation.detail);
        }
        current = candidate;
        current_linearization = candidate_evaluation.linearization;
        if (current_linearization.function_value == 0.0) {
            return success(LocalRootStopReason::ExactEvaluation);
        }
        if (std::abs(current_linearization.function_value) <=
            policy.residual_absolute_tolerance) {
            return success(LocalRootStopReason::ResidualTolerance);
        }
        if (std::abs(step) <= tolerance) {
            return success(LocalRootStopReason::StepTolerance);
        }
    }

    return failure(NumericalStatus::MaxIterations, "max-iterations");
}

} // namespace gnc::foundation

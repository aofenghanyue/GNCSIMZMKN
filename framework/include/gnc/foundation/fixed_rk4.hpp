#pragma once

#include "gnc/foundation/numerical_policy.hpp"

#include <cmath>
#include <cstddef>
#include <functional>
#include <string_view>
#include <utility>

namespace gnc::foundation {

inline constexpr AlgorithmIdentity kClassicalRk4FixedStepIdentity{
    "gnc.foundation.ode.classical-rk4-fixed-step@1", "1.0.0"};
inline constexpr int kClassicalRk4AccuracyOrder = 4;
inline constexpr std::size_t kClassicalRk4DerivativeEvaluations = 4U;

namespace detail {

template <typename State>
[[nodiscard]] std::size_t state_size(const State& state) {
    return static_cast<std::size_t>(state.size());
}

template <typename State>
[[nodiscard]] bool state_is_finite(const State& state) {
    for (std::size_t index = 0U; index < state_size(state); ++index) {
        if (!std::isfinite(static_cast<double>(state[index]))) {
            return false;
        }
    }
    return true;
}

template <typename State>
[[nodiscard]] State add_scaled(const State& base, const State& increment,
                               double scale) {
    State result = base;
    for (std::size_t index = 0U; index < state_size(base); ++index) {
        result[index] += increment[index] * scale;
    }
    return result;
}

template <typename State>
[[nodiscard]] State rk4_candidate(const State& committed, const State& k1,
                                  const State& k2, const State& k3,
                                  const State& k4, double dt) {
    State result = committed;
    for (std::size_t index = 0U; index < state_size(committed); ++index) {
        result[index] +=
            (k1[index] + 2.0 * k2[index] + 2.0 * k3[index] + k4[index]) *
            (dt / 6.0);
    }
    return result;
}

[[nodiscard]] constexpr NumericalStatus combine_stage_status(
    NumericalStatus aggregate, NumericalStatus stage) noexcept {
    if (aggregate == NumericalStatus::Approximate ||
        stage == NumericalStatus::Approximate) {
        return NumericalStatus::Approximate;
    }
    if (aggregate == NumericalStatus::Extrapolated ||
        stage == NumericalStatus::Extrapolated) {
        return NumericalStatus::Extrapolated;
    }
    return NumericalStatus::Success;
}

} // namespace detail

template <typename State, typename DerivativeFunction>
[[nodiscard]] NumericalOutcome<State>
fixed_rk4_step(const State& committed, double time, double dt,
               DerivativeFunction&& derivative,
               const NumericalPolicy& policy = NumericalPolicy{}) {
    NumericalFlags accumulated_flags = 0U;
    std::size_t derivative_evaluations = 0U;
    NumericalStatus aggregate_status = NumericalStatus::Success;

    const auto failure = [&](NumericalStatus status,
                             std::string_view detail_text) {
        NumericalEvidence evidence;
        evidence.flags = accumulated_flags;
        evidence.evaluations = derivative_evaluations;
        if (std::isfinite(dt)) {
            evidence.last_step = dt;
        }
        evidence.algorithm = kClassicalRk4FixedStepIdentity;
        evidence.detail = detail_text;
        return NumericalOutcome<State>::failure(status, evidence);
    };

    if (!valid_numerical_policy(policy)) {
        return failure(NumericalStatus::DomainError, "policy");
    }
    if (!std::isfinite(time) || !std::isfinite(dt) || dt <= 0.0) {
        return failure(NumericalStatus::DomainError, "time-or-dt");
    }
    if (detail::state_size(committed) == 0U) {
        return failure(NumericalStatus::DomainError, "empty-state");
    }
    if (policy.finite_check != FiniteCheck::Disabled &&
        !detail::state_is_finite(committed)) {
        return failure(NumericalStatus::NonFiniteInput, "state");
    }

    const auto evaluate_stage =
        [&](std::string_view stage, double evaluation_time,
            const State& stage_state) -> NumericalOutcome<State> {
        if (!std::isfinite(evaluation_time)) {
            return failure(NumericalStatus::NonFiniteIntermediate, stage);
        }
        if (policy.finite_check == FiniteCheck::EveryStage &&
            !detail::state_is_finite(stage_state)) {
            return failure(NumericalStatus::NonFiniteIntermediate, stage);
        }

        ++derivative_evaluations;
        auto outcome = std::invoke(derivative, evaluation_time, stage_state);
        accumulated_flags |= outcome.evidence().flags;
        if (!outcome.has_value()) {
            return failure(outcome.status(), stage);
        }
        if (detail::state_size(outcome.value()) !=
            detail::state_size(committed)) {
            return failure(NumericalStatus::InternalFailure, stage);
        }
        if (policy.finite_check == FiniteCheck::EveryStage &&
            !detail::state_is_finite(outcome.value())) {
            return failure(NumericalStatus::NonFiniteIntermediate, stage);
        }
        aggregate_status =
            detail::combine_stage_status(aggregate_status, outcome.status());
        return outcome;
    };

    auto k1_outcome = evaluate_stage("k1", time, committed);
    if (!k1_outcome.has_value()) {
        return k1_outcome;
    }
    const State k1 = k1_outcome.value();

    const double half_time = time + 0.5 * dt;
    const State k2_state = detail::add_scaled(committed, k1, 0.5 * dt);
    auto k2_outcome = evaluate_stage("k2", half_time, k2_state);
    if (!k2_outcome.has_value()) {
        return k2_outcome;
    }
    const State k2 = k2_outcome.value();

    const State k3_state = detail::add_scaled(committed, k2, 0.5 * dt);
    auto k3_outcome = evaluate_stage("k3", half_time, k3_state);
    if (!k3_outcome.has_value()) {
        return k3_outcome;
    }
    const State k3 = k3_outcome.value();

    const double full_time = time + dt;
    const State k4_state = detail::add_scaled(committed, k3, dt);
    auto k4_outcome = evaluate_stage("k4", full_time, k4_state);
    if (!k4_outcome.has_value()) {
        return k4_outcome;
    }
    const State k4 = k4_outcome.value();

    State candidate = detail::rk4_candidate(committed, k1, k2, k3, k4, dt);
    if (policy.finite_check != FiniteCheck::Disabled &&
        !detail::state_is_finite(candidate)) {
        return failure(NumericalStatus::NonFiniteOutput, "candidate");
    }

    NumericalEvidence evidence;
    evidence.flags = accumulated_flags;
    evidence.evaluations = derivative_evaluations;
    evidence.last_step = dt;
    evidence.algorithm = kClassicalRk4FixedStepIdentity;
    return NumericalOutcome<State>::with_value(
        aggregate_status, std::move(candidate), evidence);
}

} // namespace gnc::foundation

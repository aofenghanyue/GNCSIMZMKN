#include "gnc/foundation/fixed_rk4.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using gnc::foundation::FiniteCheck;
using gnc::foundation::NumericalEvidence;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;

constexpr std::string_view kSchema = "gnczmkn.foundation-numerics-probe/1";
constexpr std::string_view kFixtureId = "REF-MINIMAL-3DOF-001";
constexpr std::string_view kModelId =
    "MODEL-MINIMAL-3DOF-LINEAR-TRANSLATION-001";
constexpr std::string_view kComponentId = "GNC-FOUNDATION-NUMERICS-001";
constexpr double kGridTolerance = 1.0e-12;

using State = std::array<double, 6>;

enum StateIndex : std::size_t {
    PositionX = 0U,
    PositionY = 1U,
    PositionZ = 2U,
    VelocityX = 3U,
    VelocityY = 4U,
    VelocityZ = 5U,
};

struct Model {
    std::array<double, 3> acceleration;
    double drag_rate = 0.0;
};

struct Record {
    std::int64_t tick = 0;
    double time_s = 0.0;
    State state{};
};

struct Run {
    std::vector<Record> trajectory;
};

struct ConvergenceRun {
    double dt_s = 0.0;
    std::int64_t ticks = 0;
    State final_state{};
    std::size_t derivative_evaluations = 0U;
};

struct TerminationRun {
    std::vector<Record> trajectory;
    std::int64_t first_satisfied_tick = -1;
    double terminal_time_s = 0.0;
};

struct FailureRun {
    std::vector<Record> committed_trajectory;
    NumericalStatus status = NumericalStatus::InternalFailure;
    std::string detail;
    std::size_t derivative_evaluations = 0U;
    double evaluation_time_s = 0.0;
    std::int64_t failed_step_start_tick = -1;
};

struct FailureCase {
    std::string id;
    NumericalStatus status = NumericalStatus::InternalFailure;
    std::string detail;
    std::size_t evaluations = 0U;
    bool has_value = false;
};

struct ToleranceCase {
    std::string id;
    NumericalStatus status = NumericalStatus::InternalFailure;
    bool has_value = false;
    bool accepted = false;
    double absolute_error = 0.0;
    double limit = 0.0;
};

struct Bundle {
    Run constant_acceleration;
    std::vector<ConvergenceRun> convergence;
    TerminationRun termination;
    FailureRun stage_failure;
    std::vector<FailureCase> failure_cases;
    std::vector<ToleranceCase> tolerance_cases;
};

State makeState(const std::array<double, 3>& position,
                const std::array<double, 3>& velocity) {
    return {position[0], position[1], position[2], velocity[0], velocity[1],
            velocity[2]};
}

bool finite(const State& state) {
    return std::all_of(state.begin(), state.end(),
                       [](double value) { return std::isfinite(value); });
}

NumericalEvidence rhsEvidence() {
    NumericalEvidence evidence;
    evidence.algorithm = {
        "fixture.minimal-3dof.linear-translation-rhs@1", "1.0.0"};
    return evidence;
}

template <typename DomainPredicate>
auto makeDerivative(const Model& model, DomainPredicate domain_predicate) {
    return [model, domain_predicate](double time_s,
                                     const State& state) {
        if (!domain_predicate(time_s, state)) {
            NumericalEvidence evidence = rhsEvidence();
            evidence.detail = "evaluation-domain";
            return NumericalOutcome<State>::failure(
                NumericalStatus::DomainError, evidence);
        }

        State derivative{};
        derivative[PositionX] = state[VelocityX];
        derivative[PositionY] = state[VelocityY];
        derivative[PositionZ] = state[VelocityZ];
        derivative[VelocityX] =
            model.acceleration[0] - model.drag_rate * state[VelocityX];
        derivative[VelocityY] =
            model.acceleration[1] - model.drag_rate * state[VelocityY];
        derivative[VelocityZ] =
            model.acceleration[2] - model.drag_rate * state[VelocityZ];
        return NumericalOutcome<State>::with_value(
            NumericalStatus::Success, derivative, rhsEvidence());
    };
}

auto makeDerivative(const Model& model) {
    return makeDerivative(model,
                          [](double, const State&) { return true; });
}

std::int64_t exactGridTicks(double duration_s, double dt_s) {
    const double ratio = duration_s / dt_s;
    const double nearest = std::round(ratio);
    if (!std::isfinite(ratio) || !std::isfinite(nearest) || duration_s < 0.0 ||
        dt_s <= 0.0 || std::abs(ratio - nearest) > kGridTolerance ||
        nearest < 0.0 ||
        nearest >=
            static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        throw std::domain_error("duration-grid");
    }
    return static_cast<std::int64_t>(nearest);
}

Run runDuration(const State& initial, const Model& model, double dt_s,
                double duration_s, const NumericalPolicy& policy) {
    const std::int64_t ticks = exactGridTicks(duration_s, dt_s);
    Run run;
    run.trajectory.reserve(static_cast<std::size_t>(ticks + 1));
    State committed = initial;
    run.trajectory.push_back({0, 0.0, committed});
    const auto derivative = makeDerivative(model);
    for (std::int64_t tick = 0; tick < ticks; ++tick) {
        const double time_s = static_cast<double>(tick) * dt_s;
        auto outcome = gnc::foundation::fixed_rk4_step(
            committed, time_s, dt_s, derivative, policy);
        if (!outcome.succeeded() || !outcome.has_value() ||
            outcome.evidence().evaluations !=
                gnc::foundation::kClassicalRk4DerivativeEvaluations) {
            throw std::runtime_error("unexpected fixed RK4 failure");
        }
        committed = outcome.value();
        const std::int64_t committed_tick = tick + 1;
        run.trajectory.push_back(
            {committed_tick, static_cast<double>(committed_tick) * dt_s,
             committed});
    }
    return run;
}

State constantInitial() {
    return makeState({100.0, -50.0, 1000.0}, {20.0, 5.0, 10.0});
}

Model constantModel() { return {{0.5, -1.0, -9.80665}, 0.0}; }

State convergenceInitial() {
    return makeState({0.0, 0.0, 100.0}, {10.0, -5.0, 20.0});
}

Model convergenceModel() { return {{1.0, 0.0, -9.80665}, 0.2}; }

State analyticState(const State& initial, const Model& model, double time_s) {
    State result{};
    if (model.drag_rate == 0.0) {
        for (std::size_t axis = 0U; axis < 3U; ++axis) {
            const std::size_t velocity_index = VelocityX + axis;
            result[axis] = initial[axis] + initial[velocity_index] * time_s +
                           0.5 * model.acceleration[axis] * time_s * time_s;
            result[velocity_index] =
                initial[velocity_index] + model.acceleration[axis] * time_s;
        }
        return result;
    }

    const double decay = std::exp(-model.drag_rate * time_s);
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        const std::size_t velocity_index = VelocityX + axis;
        const double velocity_infinity =
            model.acceleration[axis] / model.drag_rate;
        const double velocity_delta =
            initial[velocity_index] - velocity_infinity;
        result[velocity_index] = velocity_infinity + velocity_delta * decay;
        result[axis] = initial[axis] + velocity_infinity * time_s +
                       velocity_delta * (1.0 - decay) / model.drag_rate;
    }
    return result;
}

std::vector<ConvergenceRun> runConvergence(const NumericalPolicy& policy) {
    constexpr std::array<double, 5> kSteps{0.8, 0.4, 0.2, 0.1, 0.05};
    std::vector<ConvergenceRun> runs;
    runs.reserve(kSteps.size());
    for (double dt_s : kSteps) {
        const Run run = runDuration(convergenceInitial(), convergenceModel(),
                                    dt_s, 4.0, policy);
        const std::int64_t ticks = exactGridTicks(4.0, dt_s);
        runs.push_back({dt_s, ticks, run.trajectory.back().state,
                        static_cast<std::size_t>(ticks) *
                            gnc::foundation::
                                kClassicalRk4DerivativeEvaluations});
    }
    return runs;
}

TerminationRun runTermination(const NumericalPolicy& policy) {
    constexpr double kDt = 0.5;
    constexpr double kMaximumDuration = 10.0;
    const State initial = makeState({0.0, 0.0, 10.0}, {1.0, 0.0, -2.0});
    const Model model{{0.0, 0.0, 0.0}, 0.0};
    const auto derivative = makeDerivative(model);
    const std::int64_t maximum_ticks =
        exactGridTicks(kMaximumDuration, kDt);

    TerminationRun run;
    State committed = initial;
    run.trajectory.push_back({0, 0.0, committed});
    for (std::int64_t tick = 0; tick < maximum_ticks; ++tick) {
        const double time_s = static_cast<double>(tick) * kDt;
        auto outcome = gnc::foundation::fixed_rk4_step(
            committed, time_s, kDt, derivative, policy);
        if (!outcome.succeeded()) {
            throw std::runtime_error("termination integration failed");
        }
        committed = outcome.value();
        const std::int64_t committed_tick = tick + 1;
        const double committed_time_s =
            static_cast<double>(committed_tick) * kDt;
        run.trajectory.push_back(
            {committed_tick, committed_time_s, committed});
        if (committed[PositionZ] <= 0.0) {
            run.first_satisfied_tick = committed_tick;
            run.terminal_time_s = committed_time_s;
            return run;
        }
    }
    throw std::runtime_error("termination predicate was not satisfied");
}

FailureRun runStageFailure(const NumericalPolicy& policy) {
    constexpr double kDt = 0.5;
    constexpr double kEvaluationLimit = 0.75;
    const State initial = makeState({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0});
    const Model model{{0.0, 0.0, 0.0}, 0.0};
    const auto derivative = makeDerivative(
        model, [](double time_s, const State&) {
            return time_s < kEvaluationLimit;
        });

    FailureRun run;
    State committed = initial;
    run.committed_trajectory.push_back({0, 0.0, committed});
    for (std::int64_t tick = 0; tick < 3; ++tick) {
        const double time_s = static_cast<double>(tick) * kDt;
        auto outcome = gnc::foundation::fixed_rk4_step(
            committed, time_s, kDt, derivative, policy);
        if (!outcome.has_value()) {
            run.status = outcome.status();
            run.detail = std::string{outcome.evidence().detail};
            run.derivative_evaluations = outcome.evidence().evaluations;
            run.failed_step_start_tick = tick;
            run.evaluation_time_s = time_s + 0.5 * kDt;
            return run;
        }
        committed = outcome.value();
        const std::int64_t committed_tick = tick + 1;
        run.committed_trajectory.push_back(
            {committed_tick, static_cast<double>(committed_tick) * kDt,
             committed});
    }
    throw std::runtime_error("stage failure unexpectedly completed");
}

FailureCase toFailureCase(std::string id,
                          const NumericalOutcome<State>& outcome) {
    return {std::move(id), outcome.status(),
            std::string{outcome.evidence().detail},
            outcome.evidence().evaluations, outcome.has_value()};
}

std::vector<FailureCase> runFailureCases(const NumericalPolicy& policy) {
    const State initial = constantInitial();
    const auto derivative = makeDerivative(constantModel());
    std::vector<FailureCase> cases;
    cases.push_back(toFailureCase(
        "NEGATIVE-DT",
        gnc::foundation::fixed_rk4_step(initial, 0.0, -0.5, derivative,
                                        policy)));

    State nonfinite = initial;
    nonfinite[PositionX] = std::numeric_limits<double>::quiet_NaN();
    cases.push_back(toFailureCase(
        "NONFINITE-INPUT",
        gnc::foundation::fixed_rk4_step(nonfinite, 0.0, 0.5, derivative,
                                        policy)));

    const auto nonfinite_derivative = [](double, const State&) {
        State value{};
        value[0] = std::numeric_limits<double>::infinity();
        return NumericalOutcome<State>::with_value(
            NumericalStatus::Success, value, rhsEvidence());
    };
    cases.push_back(toFailureCase(
        "NONFINITE-DERIVATIVE",
        gnc::foundation::fixed_rk4_step(initial, 0.0, 0.5,
                                        nonfinite_derivative, policy)));

    NumericalPolicy invalid_policy = policy;
    invalid_policy.absolute_tolerance = -1.0;
    cases.push_back(toFailureCase(
        "INVALID-POLICY",
        gnc::foundation::fixed_rk4_step(initial, 0.0, 0.5, derivative,
                                        invalid_policy)));
    return cases;
}

ToleranceCase toToleranceCase(
    std::string id,
    const NumericalOutcome<gnc::foundation::ToleranceComparison>& outcome) {
    ToleranceCase result;
    result.id = std::move(id);
    result.status = outcome.status();
    result.has_value = outcome.has_value();
    if (outcome.has_value()) {
        result.accepted = outcome.value().accepted;
        result.absolute_error = outcome.value().absolute_error;
        result.limit = outcome.value().limit;
    }
    return result;
}

std::vector<ToleranceCase> runToleranceCases(const NumericalPolicy& policy) {
    return {
        toToleranceCase(
            "WITHIN-ABS-REL",
            gnc::foundation::compare_with_tolerance(1000.000000001,
                                                    1000.0, policy)),
        toToleranceCase(
            "OUTSIDE-ABS-REL",
            gnc::foundation::compare_with_tolerance(1000.00001, 1000.0,
                                                    policy)),
        toToleranceCase(
            "NONFINITE-COMPARISON",
            gnc::foundation::compare_with_tolerance(
                std::numeric_limits<double>::infinity(), 1000.0, policy)),
    };
}

Bundle runBundle() {
    const NumericalPolicy policy{1.0e-12, 1.0e-12,
                                 FiniteCheck::EveryStage};
    return {
        runDuration(constantInitial(), constantModel(), 0.5, 2.0, policy),
        runConvergence(policy),
        runTermination(policy),
        runStageFailure(policy),
        runFailureCases(policy),
        runToleranceCases(policy),
    };
}

double maxStateError(const State& actual, const State& expected,
                     std::size_t begin, std::size_t end) {
    double error = 0.0;
    for (std::size_t index = begin; index < end; ++index) {
        error = std::max(error, std::abs(actual[index] - expected[index]));
    }
    return error;
}

bool close(double actual, double expected, double tolerance = 1.0e-11) {
    return std::isfinite(actual) &&
           std::abs(actual - expected) <=
               tolerance *
                   std::max({1.0, std::abs(actual), std::abs(expected)});
}

bool factoriesRejectInconsistentStatus() {
    bool value_rejected = false;
    bool failure_rejected = false;
    try {
        static_cast<void>(NumericalOutcome<double>::with_value(
            NumericalStatus::DomainError, 1.0));
    } catch (const std::invalid_argument&) {
        value_rejected = true;
    }
    try {
        static_cast<void>(NumericalOutcome<double>::failure(
            NumericalStatus::Success));
    } catch (const std::invalid_argument&) {
        failure_rejected = true;
    }
    return value_rejected && failure_rejected;
}

bool selfCheck(const Bundle& bundle) {
    if (gnc::foundation::kClassicalRk4AccuracyOrder != 4 ||
        gnc::foundation::kClassicalRk4DerivativeEvaluations != 4U ||
        gnc::foundation::to_string(NumericalStatus::NonFiniteIntermediate) !=
            "NonFiniteIntermediate" ||
        !factoriesRejectInconsistentStatus()) {
        return false;
    }

    if (bundle.constant_acceleration.trajectory.size() != 5U) {
        return false;
    }
    const State& constant_final =
        bundle.constant_acceleration.trajectory.back().state;
    if (!close(constant_final[PositionX], 141.0) ||
        !close(constant_final[PositionY], -42.0) ||
        !close(constant_final[PositionZ], 1000.3867) ||
        !close(constant_final[VelocityX], 21.0) ||
        !close(constant_final[VelocityY], 3.0) ||
        !close(constant_final[VelocityZ], -9.6133)) {
        return false;
    }

    if (bundle.convergence.size() != 5U) {
        return false;
    }
    const State analytic =
        analyticState(convergenceInitial(), convergenceModel(), 4.0);
    std::vector<double> position_errors;
    std::vector<double> velocity_errors;
    for (const ConvergenceRun& run : bundle.convergence) {
        if (!finite(run.final_state) || run.derivative_evaluations !=
                                            static_cast<std::size_t>(run.ticks) *
                                                4U) {
            return false;
        }
        position_errors.push_back(
            maxStateError(run.final_state, analytic, PositionX, VelocityX));
        velocity_errors.push_back(maxStateError(
            run.final_state, analytic, VelocityX, bundle.convergence[0]
                                                     .final_state.size()));
    }
    for (std::size_t index = 0U; index + 1U < position_errors.size(); ++index) {
        const double position_order =
            std::log2(position_errors[index] / position_errors[index + 1U]);
        const double velocity_order =
            std::log2(velocity_errors[index] / velocity_errors[index + 1U]);
        if (!(position_errors[index + 1U] < position_errors[index]) ||
            !(velocity_errors[index + 1U] < velocity_errors[index]) ||
            position_order < 3.8 || velocity_order < 3.8) {
            return false;
        }
    }

    if (bundle.termination.first_satisfied_tick != 10 ||
        bundle.termination.trajectory.size() != 11U ||
        !close(bundle.termination.terminal_time_s, 5.0) ||
        !close(bundle.termination.trajectory.back().state[PositionZ], 0.0)) {
        return false;
    }

    if (bundle.stage_failure.status != NumericalStatus::DomainError ||
        bundle.stage_failure.detail != "k2" ||
        bundle.stage_failure.derivative_evaluations != 2U ||
        bundle.stage_failure.failed_step_start_tick != 1 ||
        bundle.stage_failure.committed_trajectory.size() != 2U ||
        !close(bundle.stage_failure.evaluation_time_s, 0.75) ||
        !close(bundle.stage_failure.committed_trajectory.back()
                   .state[PositionX],
               0.5)) {
        return false;
    }

    const std::array<NumericalStatus, 4> expected_failures{
        NumericalStatus::DomainError, NumericalStatus::NonFiniteInput,
        NumericalStatus::NonFiniteIntermediate, NumericalStatus::DomainError};
    if (bundle.failure_cases.size() != expected_failures.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < expected_failures.size(); ++index) {
        if (bundle.failure_cases[index].status != expected_failures[index] ||
            bundle.failure_cases[index].has_value) {
            return false;
        }
    }

    return bundle.tolerance_cases.size() == 3U &&
           bundle.tolerance_cases[0].status == NumericalStatus::Success &&
           bundle.tolerance_cases[0].accepted &&
           bundle.tolerance_cases[1].status == NumericalStatus::Success &&
           !bundle.tolerance_cases[1].accepted &&
           bundle.tolerance_cases[2].status ==
               NumericalStatus::NonFiniteInput &&
           !bundle.tolerance_cases[2].has_value;
}

void writeJsonString(std::ostream& stream, std::string_view value) {
    stream << '"';
    for (char character : value) {
        if (character == '"' || character == '\\') {
            stream << '\\';
        }
        stream << character;
    }
    stream << '"';
}

void writeState(std::ostream& stream, const State& state) {
    stream << "{\"position_m\": [" << state[PositionX] << ", "
           << state[PositionY] << ", " << state[PositionZ]
           << "], \"velocity_mps\": [" << state[VelocityX] << ", "
           << state[VelocityY] << ", " << state[VelocityZ] << "]}";
}

void writeTrajectory(std::ostream& stream,
                     const std::vector<Record>& trajectory) {
    stream << '[';
    for (std::size_t index = 0U; index < trajectory.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        const Record& record = trajectory[index];
        stream << "\n        {\"tick\": " << record.tick
               << ", \"time_s\": " << record.time_s << ", \"state\": ";
        writeState(stream, record.state);
        stream << '}';
    }
    if (!trajectory.empty()) {
        stream << '\n';
    }
    stream << "      ]";
}

void writeReport(std::ostream& stream, const Bundle& bundle) {
    stream << std::setprecision(17);
    stream << "{\n  \"schema_version\": ";
    writeJsonString(stream, kSchema);
    stream << ",\n  \"component_id\": ";
    writeJsonString(stream, kComponentId);
    stream << ",\n  \"fixture_id\": ";
    writeJsonString(stream, kFixtureId);
    stream << ",\n  \"model_id\": ";
    writeJsonString(stream, kModelId);
    stream << ",\n  \"algorithm\": {\"id\": ";
    writeJsonString(stream,
                    gnc::foundation::kClassicalRk4FixedStepIdentity.id);
    stream << ", \"version\": ";
    writeJsonString(stream,
                    gnc::foundation::kClassicalRk4FixedStepIdentity.version);
    stream << ", \"accuracy_order\": "
           << gnc::foundation::kClassicalRk4AccuracyOrder
           << ", \"derivative_evaluations_per_step\": "
           << gnc::foundation::kClassicalRk4DerivativeEvaluations << "},\n"
              "  \"policy\": {\"absolute_tolerance\": 1e-12, "
              "\"relative_tolerance\": 1e-12, "
              "\"finite_check\": \"EveryStage\"},\n"
              "  \"cases\": {\n"
              "    \"CASE-MIN3D-CONSTANT-ACCELERATION\": {\n"
              "      \"trajectory\": ";
    writeTrajectory(stream, bundle.constant_acceleration.trajectory);
    stream << "\n    },\n"
              "    \"CASE-MIN3D-LINEAR-DRAG-CONVERGENCE\": {\n"
              "      \"runs\": [";
    for (std::size_t index = 0U; index < bundle.convergence.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        const ConvergenceRun& run = bundle.convergence[index];
        stream << "\n        {\"dt_s\": " << run.dt_s
               << ", \"ticks\": " << run.ticks
               << ", \"derivative_evaluations\": "
               << run.derivative_evaluations << ", \"final_state\": ";
        writeState(stream, run.final_state);
        stream << '}';
    }
    stream << "\n      ]\n    },\n"
              "    \"CASE-MIN3D-EXACT-GRID-TERMINATION\": {\n"
              "      \"trajectory\": ";
    writeTrajectory(stream, bundle.termination.trajectory);
    stream << ",\n      \"terminal\": {\"predicate\": "
              "\"position.z <= 0 m\", \"first_satisfied_tick\": "
           << bundle.termination.first_satisfied_tick
           << ", \"time_s\": " << bundle.termination.terminal_time_s
           << "}\n    },\n"
              "    \"CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE\": {\n"
              "      \"committed_trajectory\": ";
    writeTrajectory(stream, bundle.stage_failure.committed_trajectory);
    stream << ",\n      \"failure\": {\"status\": ";
    writeJsonString(stream,
                    gnc::foundation::to_string(bundle.stage_failure.status));
    stream << ", \"detail\": ";
    writeJsonString(stream, bundle.stage_failure.detail);
    stream << ", \"derivative_evaluations\": "
           << bundle.stage_failure.derivative_evaluations
           << ", \"evaluation_time_s\": "
           << bundle.stage_failure.evaluation_time_s
           << ", \"failed_step_start_tick\": "
           << bundle.stage_failure.failed_step_start_tick
           << ", \"candidate_disposition\": \"discarded\", "
              "\"last_committed_tick\": "
           << bundle.stage_failure.committed_trajectory.back().tick
           << "}\n    }\n  },\n  \"failure_cases\": [";
    for (std::size_t index = 0U; index < bundle.failure_cases.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        const FailureCase& failure_case = bundle.failure_cases[index];
        stream << "\n    {\"id\": ";
        writeJsonString(stream, failure_case.id);
        stream << ", \"status\": ";
        writeJsonString(stream,
                        gnc::foundation::to_string(failure_case.status));
        stream << ", \"detail\": ";
        writeJsonString(stream, failure_case.detail);
        stream << ", \"evaluations\": " << failure_case.evaluations
               << ", \"has_value\": "
               << (failure_case.has_value ? "true" : "false") << '}';
    }
    stream << "\n  ],\n  \"tolerance_cases\": [";
    for (std::size_t index = 0U; index < bundle.tolerance_cases.size();
         ++index) {
        if (index != 0U) {
            stream << ',';
        }
        const ToleranceCase& tolerance_case = bundle.tolerance_cases[index];
        stream << "\n    {\"id\": ";
        writeJsonString(stream, tolerance_case.id);
        stream << ", \"status\": ";
        writeJsonString(stream,
                        gnc::foundation::to_string(tolerance_case.status));
        stream << ", \"has_value\": "
               << (tolerance_case.has_value ? "true" : "false")
               << ", \"accepted\": "
               << (tolerance_case.accepted ? "true" : "false")
               << ", \"absolute_error\": " << tolerance_case.absolute_error
               << ", \"limit\": " << tolerance_case.limit << '}';
    }
    stream << "\n  ]\n}\n";
}

struct Options {
    bool self_check = false;
    std::string report_path;
};

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--self-check") {
            options.self_check = true;
        } else if (argument == "--report" && index + 1 < argc) {
            options.report_path = argv[++index];
        } else {
            throw std::invalid_argument("usage: --self-check | --report <path>");
        }
    }
    if (!options.self_check && options.report_path.empty()) {
        throw std::invalid_argument("usage: --self-check | --report <path>");
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        const Bundle bundle = runBundle();
        const bool passed = selfCheck(bundle);
        if (!options.report_path.empty()) {
            std::ofstream stream{options.report_path,
                                 std::ios::out | std::ios::binary};
            if (!stream) {
                throw std::runtime_error("unable to open report path");
            }
            writeReport(stream, bundle);
            if (!stream) {
                throw std::runtime_error("unable to write report");
            }
        }

        std::cout << "foundation numerics RK4 cases=4 failures="
                  << bundle.failure_cases.size()
                  << " tolerance_cases=" << bundle.tolerance_cases.size()
                  << " status=" << (passed ? "pass" : "fail") << '\n';
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "foundation numerics probe error: " << error.what()
                  << '\n';
        return EXIT_FAILURE;
    }
}

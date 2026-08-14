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

constexpr std::string_view kFixtureId = "REF-MINIMAL-3DOF-001";
constexpr std::string_view kModelId =
    "MODEL-MINIMAL-3DOF-LINEAR-TRANSLATION-001";
constexpr std::string_view kOutputSchema = "gnczmkn.minimal-3dof-probe/1";
constexpr double kGridTolerance = 1.0e-12;

struct Vec3 {
    double x;
    double y;
    double z;
};

struct State {
    Vec3 position;
    Vec3 velocity;
};

struct Model {
    Vec3 acceleration;
    double drag_rate;
};

struct Derivative {
    Vec3 position_rate;
    Vec3 velocity_rate;
};

struct Record {
    std::int64_t tick;
    double time_s;
    State state;
};

struct FixedCase {
    std::string_view id;
    State initial;
    Model model;
    double dt_s;
    double duration_s;
};

struct StepResult {
    bool ok = false;
    State candidate{};
    std::string code;
    std::string stage;
    double evaluation_time_s = 0.0;
};

struct DurationRun {
    std::vector<Record> trajectory;
};

struct ConvergenceRun {
    double dt_s;
    std::int64_t ticks;
    State final_state;
};

struct TerminationRun {
    std::vector<Record> trajectory;
    std::int64_t first_satisfied_tick = -1;
    double terminal_time_s = 0.0;
};

struct FailureRun {
    std::vector<Record> committed_trajectory;
    std::string code;
    std::string stage;
    double evaluation_time_s = 0.0;
    std::int64_t failed_step_start_tick = -1;
};

struct InvalidInputResult {
    std::string id;
    std::string status;
};

struct BundleResult {
    DurationRun constant_acceleration;
    std::vector<ConvergenceRun> convergence;
    TerminationRun termination;
    FailureRun stage_failure;
    std::vector<InvalidInputResult> invalid_inputs;
};

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 scale(const Vec3& value, double factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

Vec3 addScaled(const Vec3& base, const Vec3& increment, double factor) {
    return add(base, scale(increment, factor));
}

bool finite(const Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool finite(const State& state) {
    return finite(state.position) && finite(state.velocity);
}

bool finite(const Model& model) {
    return finite(model.acceleration) && std::isfinite(model.drag_rate);
}

void validateInputs(const State& initial, const Model& model, double dt_s,
                    double duration_s) {
    if (!finite(initial) || !finite(model) || !std::isfinite(dt_s) ||
        !std::isfinite(duration_s) || dt_s <= 0.0 || duration_s < 0.0 ||
        model.drag_rate < 0.0) {
        throw std::domain_error("input-domain-error");
    }
}

std::int64_t exactGridTicks(double duration_s, double dt_s) {
    if (!std::isfinite(duration_s) || !std::isfinite(dt_s) ||
        duration_s < 0.0 || dt_s <= 0.0) {
        throw std::domain_error("input-domain-error");
    }
    const double ratio = duration_s / dt_s;
    const double nearest = std::round(ratio);
    if (!std::isfinite(ratio) || !std::isfinite(nearest) ||
        std::abs(ratio - nearest) > kGridTolerance || nearest < 0.0 ||
        nearest >=
            static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        throw std::domain_error("input-domain-error");
    }
    return static_cast<std::int64_t>(nearest);
}

Derivative evaluate(const State& state, const Model& model, double time_s,
                    std::string_view stage,
                    const std::optional<double>& time_limit_s) {
    if (!finite(state) || !finite(model) || !std::isfinite(time_s)) {
        throw std::domain_error("non-finite-stage-input");
    }
    if (time_limit_s.has_value() && !(time_s < *time_limit_s)) {
        throw std::domain_error(std::string{stage});
    }

    const Derivative derivative{
        state.velocity,
        add(model.acceleration, scale(state.velocity, -model.drag_rate)),
    };
    if (!finite(derivative.position_rate) ||
        !finite(derivative.velocity_rate)) {
        throw std::domain_error("non-finite-stage-output");
    }
    return derivative;
}

State addScaled(const State& state, const Derivative& derivative,
                double factor) {
    return {
        addScaled(state.position, derivative.position_rate, factor),
        addScaled(state.velocity, derivative.velocity_rate, factor),
    };
}

Vec3 weightedIncrement(const Vec3& k1, const Vec3& k2, const Vec3& k3,
                       const Vec3& k4, double dt_s) {
    return scale(
        add(add(k1, scale(k2, 2.0)), add(scale(k3, 2.0), k4)),
        dt_s / 6.0);
}

StepResult rk4Candidate(const State& committed, const Model& model,
                        double time_s, double dt_s,
                        const std::optional<double>& time_limit_s =
                            std::nullopt) {
    try {
        const Derivative k1 =
            evaluate(committed, model, time_s, "k1", time_limit_s);
        const Derivative k2 = evaluate(
            addScaled(committed, k1, 0.5 * dt_s), model,
            time_s + 0.5 * dt_s, "k2", time_limit_s);
        const Derivative k3 = evaluate(
            addScaled(committed, k2, 0.5 * dt_s), model,
            time_s + 0.5 * dt_s, "k3", time_limit_s);
        const Derivative k4 = evaluate(addScaled(committed, k3, dt_s), model,
                                       time_s + dt_s, "k4", time_limit_s);

        const State candidate{
            add(committed.position,
                weightedIncrement(k1.position_rate, k2.position_rate,
                                  k3.position_rate, k4.position_rate, dt_s)),
            add(committed.velocity,
                weightedIncrement(k1.velocity_rate, k2.velocity_rate,
                                  k3.velocity_rate, k4.velocity_rate, dt_s)),
        };
        if (!finite(candidate)) {
            return {false, {}, "reference-domain-error", "candidate",
                    time_s + dt_s};
        }
        return {true, candidate, {}, {}, 0.0};
    } catch (const std::domain_error& error) {
        const std::string stage = error.what();
        double evaluation_time_s = time_s;
        if (stage == "k2" || stage == "k3") {
            evaluation_time_s += 0.5 * dt_s;
        } else if (stage == "k4") {
            evaluation_time_s += dt_s;
        }
        return {false, {}, "reference-domain-error", stage,
                evaluation_time_s};
    }
}

DurationRun runDuration(const FixedCase& simulation_case) {
    validateInputs(simulation_case.initial, simulation_case.model,
                   simulation_case.dt_s, simulation_case.duration_s);
    const std::int64_t ticks =
        exactGridTicks(simulation_case.duration_s, simulation_case.dt_s);

    DurationRun result;
    result.trajectory.reserve(static_cast<std::size_t>(ticks + 1));
    State committed = simulation_case.initial;
    result.trajectory.push_back({0, 0.0, committed});
    for (std::int64_t tick = 0; tick < ticks; ++tick) {
        const double time_s = static_cast<double>(tick) * simulation_case.dt_s;
        const StepResult step = rk4Candidate(
            committed, simulation_case.model, time_s, simulation_case.dt_s);
        if (!step.ok) {
            throw std::runtime_error("unexpected RK4 failure");
        }
        committed = step.candidate;
        const std::int64_t committed_tick = tick + 1;
        result.trajectory.push_back(
            {committed_tick,
             static_cast<double>(committed_tick) * simulation_case.dt_s,
             committed});
    }
    return result;
}

FixedCase constantAccelerationCase() {
    return {
        "CASE-MIN3D-CONSTANT-ACCELERATION",
        {{100.0, -50.0, 1000.0}, {20.0, 5.0, 10.0}},
        {{0.5, -1.0, -9.80665}, 0.0},
        0.5,
        2.0,
    };
}

FixedCase convergenceCase(double dt_s) {
    return {
        "CASE-MIN3D-LINEAR-DRAG-CONVERGENCE",
        {{0.0, 0.0, 100.0}, {10.0, -5.0, 20.0}},
        {{1.0, 0.0, -9.80665}, 0.2},
        dt_s,
        4.0,
    };
}

FixedCase terminationCase() {
    return {
        "CASE-MIN3D-EXACT-GRID-TERMINATION",
        {{0.0, 0.0, 10.0}, {1.0, 0.0, -2.0}},
        {{0.0, 0.0, 0.0}, 0.0},
        0.5,
        10.0,
    };
}

FixedCase stageFailureCase() {
    return {
        "CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE",
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
        {{0.0, 0.0, 0.0}, 0.0},
        0.5,
        1.5,
    };
}

std::vector<ConvergenceRun> runConvergence() {
    constexpr std::array<double, 5> kDtLadder{0.8, 0.4, 0.2, 0.1, 0.05};
    std::vector<ConvergenceRun> result;
    result.reserve(kDtLadder.size());
    for (const double dt_s : kDtLadder) {
        const FixedCase simulation_case = convergenceCase(dt_s);
        const DurationRun run = runDuration(simulation_case);
        result.push_back(
            {dt_s, exactGridTicks(simulation_case.duration_s, dt_s),
             run.trajectory.back().state});
    }
    return result;
}

TerminationRun runTermination() {
    const FixedCase simulation_case = terminationCase();
    validateInputs(simulation_case.initial, simulation_case.model,
                   simulation_case.dt_s, simulation_case.duration_s);
    const std::int64_t maximum_ticks =
        exactGridTicks(simulation_case.duration_s, simulation_case.dt_s);

    TerminationRun result;
    State committed = simulation_case.initial;
    result.trajectory.push_back({0, 0.0, committed});
    if (committed.position.z <= 0.0) {
        result.first_satisfied_tick = 0;
        return result;
    }

    for (std::int64_t tick = 0; tick < maximum_ticks; ++tick) {
        const double time_s = static_cast<double>(tick) * simulation_case.dt_s;
        const StepResult step = rk4Candidate(
            committed, simulation_case.model, time_s, simulation_case.dt_s);
        if (!step.ok) {
            throw std::runtime_error("unexpected termination-case RK4 failure");
        }
        committed = step.candidate;
        const std::int64_t committed_tick = tick + 1;
        const double committed_time_s =
            static_cast<double>(committed_tick) * simulation_case.dt_s;
        result.trajectory.push_back(
            {committed_tick, committed_time_s, committed});
        if (committed.position.z <= 0.0) {
            result.first_satisfied_tick = committed_tick;
            result.terminal_time_s = committed_time_s;
            return result;
        }
    }
    throw std::runtime_error("termination predicate was not satisfied");
}

FailureRun runStageFailure() {
    const FixedCase simulation_case = stageFailureCase();
    validateInputs(simulation_case.initial, simulation_case.model,
                   simulation_case.dt_s, simulation_case.duration_s);
    const std::int64_t maximum_ticks =
        exactGridTicks(simulation_case.duration_s, simulation_case.dt_s);
    constexpr double kEvaluationTimeLimit = 0.75;

    FailureRun result;
    State committed = simulation_case.initial;
    result.committed_trajectory.push_back({0, 0.0, committed});
    for (std::int64_t tick = 0; tick < maximum_ticks; ++tick) {
        const double time_s = static_cast<double>(tick) * simulation_case.dt_s;
        const StepResult step = rk4Candidate(
            committed, simulation_case.model, time_s, simulation_case.dt_s,
            kEvaluationTimeLimit);
        if (!step.ok) {
            result.code = step.code;
            result.stage = step.stage;
            result.evaluation_time_s = step.evaluation_time_s;
            result.failed_step_start_tick = tick;
            return result;
        }
        committed = step.candidate;
        const std::int64_t committed_tick = tick + 1;
        result.committed_trajectory.push_back(
            {committed_tick,
             static_cast<double>(committed_tick) * simulation_case.dt_s,
             committed});
    }
    throw std::runtime_error("stage failure case unexpectedly completed");
}

template <typename Operation>
InvalidInputResult expectInputDomainError(std::string id,
                                          Operation&& operation) {
    try {
        operation();
    } catch (const std::domain_error&) {
        return {std::move(id), "input-domain-error"};
    }
    return {std::move(id), "unexpected-success"};
}

std::vector<InvalidInputResult> runInvalidInputs() {
    std::vector<InvalidInputResult> result;
    const FixedCase valid = constantAccelerationCase();
    result.push_back(expectInputDomainError(
        "INVALID-MIN3D-ZERO-DT", [&valid] {
            validateInputs(valid.initial, valid.model, 0.0, valid.duration_s);
        }));
    result.push_back(expectInputDomainError(
        "INVALID-MIN3D-NEGATIVE-DURATION", [&valid] {
            validateInputs(valid.initial, valid.model, valid.dt_s, -0.5);
        }));
    result.push_back(expectInputDomainError(
        "INVALID-MIN3D-NON-GRID-DURATION", [&valid] {
            FixedCase invalid = valid;
            invalid.duration_s = 1.1;
            static_cast<void>(runDuration(invalid));
        }));
    result.push_back(expectInputDomainError(
        "INVALID-MIN3D-NEGATIVE-DRAG", [&valid] {
            Model invalid = valid.model;
            invalid.drag_rate = -0.1;
            validateInputs(valid.initial, invalid, valid.dt_s,
                           valid.duration_s);
        }));
    result.push_back(expectInputDomainError(
        "INVALID-MIN3D-NONFINITE-STATE", [&valid] {
            State invalid = valid.initial;
            invalid.position.x = std::numeric_limits<double>::quiet_NaN();
            validateInputs(invalid, valid.model, valid.dt_s,
                           valid.duration_s);
        }));
    return result;
}

BundleResult runBundle() {
    return {
        runDuration(constantAccelerationCase()),
        runConvergence(),
        runTermination(),
        runStageFailure(),
        runInvalidInputs(),
    };
}

bool close(double actual, double expected, double tolerance = 1.0e-11) {
    return std::isfinite(actual) &&
           std::abs(actual - expected) <=
               tolerance * std::max({1.0, std::abs(actual),
                                     std::abs(expected)});
}

bool selfCheck(const BundleResult& bundle) {
    if (bundle.constant_acceleration.trajectory.size() != 5U) {
        return false;
    }
    const State& constant_final =
        bundle.constant_acceleration.trajectory.back().state;
    if (!close(constant_final.position.x, 141.0) ||
        !close(constant_final.position.y, -42.0) ||
        !close(constant_final.position.z, 1000.3867) ||
        !close(constant_final.velocity.x, 21.0) ||
        !close(constant_final.velocity.y, 3.0) ||
        !close(constant_final.velocity.z, -9.6133)) {
        return false;
    }

    if (bundle.convergence.size() != 5U ||
        !std::all_of(bundle.convergence.begin(), bundle.convergence.end(),
                     [](const ConvergenceRun& run) {
                         return run.ticks > 0 && finite(run.final_state);
                     })) {
        return false;
    }

    if (bundle.termination.first_satisfied_tick != 10 ||
        bundle.termination.trajectory.size() != 11U ||
        !close(bundle.termination.terminal_time_s, 5.0) ||
        !close(bundle.termination.trajectory.back().state.position.z, 0.0)) {
        return false;
    }

    if (bundle.stage_failure.code != "reference-domain-error" ||
        bundle.stage_failure.stage != "k2" ||
        bundle.stage_failure.failed_step_start_tick != 1 ||
        bundle.stage_failure.committed_trajectory.size() != 2U ||
        !close(bundle.stage_failure.evaluation_time_s, 0.75) ||
        !close(bundle.stage_failure.committed_trajectory.back().state.position.x,
               0.5)) {
        return false;
    }

    return bundle.invalid_inputs.size() == 5U &&
           std::all_of(bundle.invalid_inputs.begin(),
                       bundle.invalid_inputs.end(),
                       [](const InvalidInputResult& result) {
                           return result.status == "input-domain-error";
                       });
}

void writeJsonString(std::ostream& stream, std::string_view value) {
    stream << '"';
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            stream << '\\';
        }
        stream << character;
    }
    stream << '"';
}

void writeVec3(std::ostream& stream, const Vec3& value) {
    stream << '[' << value.x << ", " << value.y << ", " << value.z << ']';
}

void writeState(std::ostream& stream, const State& state) {
    stream << "{\"position_m\": ";
    writeVec3(stream, state.position);
    stream << ", \"velocity_mps\": ";
    writeVec3(stream, state.velocity);
    stream << '}';
}

void writeTrajectory(std::ostream& stream,
                     const std::vector<Record>& trajectory) {
    stream << '[';
    for (std::size_t index = 0; index < trajectory.size(); ++index) {
        const Record& record = trajectory[index];
        if (index != 0U) {
            stream << ',';
        }
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

void writeFixedInput(std::ostream& stream, const FixedCase& simulation_case,
                     std::string_view duration_key = "duration_s") {
    stream << "{\"initial_position_m\": ";
    writeVec3(stream, simulation_case.initial.position);
    stream << ", \"initial_velocity_mps\": ";
    writeVec3(stream, simulation_case.initial.velocity);
    stream << ", \"acceleration_mps2\": ";
    writeVec3(stream, simulation_case.model.acceleration);
    stream << ", \"drag_rate_per_s\": " << simulation_case.model.drag_rate
           << ", \"dt_s\": " << simulation_case.dt_s << ", \""
           << duration_key << "\": " << simulation_case.duration_s << '}';
}

void writeReport(std::ostream& stream, const BundleResult& bundle) {
    stream << std::setprecision(17);
    stream << "{\n  \"schema_version\": ";
    writeJsonString(stream, kOutputSchema);
    stream << ",\n  \"fixture_id\": ";
    writeJsonString(stream, kFixtureId);
    stream << ",\n  \"model_id\": ";
    writeJsonString(stream, kModelId);
    stream << ",\n  \"algorithm\": \"classical-rk4-fixed-step\",\n"
              "  \"cases\": {\n";

    const FixedCase constant_case = constantAccelerationCase();
    stream << "    \"CASE-MIN3D-CONSTANT-ACCELERATION\": {\n"
              "      \"input\": ";
    writeFixedInput(stream, constant_case);
    stream << ",\n      \"trajectory\": ";
    writeTrajectory(stream, bundle.constant_acceleration.trajectory);
    stream << "\n    },\n";

    const FixedCase convergence_case = convergenceCase(0.8);
    stream << "    \"CASE-MIN3D-LINEAR-DRAG-CONVERGENCE\": {\n"
              "      \"input\": {\"initial_position_m\": ";
    writeVec3(stream, convergence_case.initial.position);
    stream << ", \"initial_velocity_mps\": ";
    writeVec3(stream, convergence_case.initial.velocity);
    stream << ", \"acceleration_mps2\": ";
    writeVec3(stream, convergence_case.model.acceleration);
    stream << ", \"drag_rate_per_s\": "
           << convergence_case.model.drag_rate
           << ", \"duration_s\": " << convergence_case.duration_s
           << ", \"dt_ladder_s\": [0.8, 0.4, 0.2, 0.1, 0.05]},\n"
              "      \"runs\": [";
    for (std::size_t index = 0; index < bundle.convergence.size(); ++index) {
        const ConvergenceRun& run = bundle.convergence[index];
        if (index != 0U) {
            stream << ',';
        }
        stream << "\n        {\"dt_s\": " << run.dt_s
               << ", \"ticks\": " << run.ticks << ", \"final_state\": ";
        writeState(stream, run.final_state);
        stream << '}';
    }
    stream << "\n      ]\n    },\n";

    const FixedCase termination_case = terminationCase();
    stream << "    \"CASE-MIN3D-EXACT-GRID-TERMINATION\": {\n"
              "      \"input\": ";
    writeFixedInput(stream, termination_case, "maximum_duration_s");
    stream << ",\n      \"trajectory\": ";
    writeTrajectory(stream, bundle.termination.trajectory);
    stream << ",\n      \"terminal\": {\"predicate\": "
              "\"position.z <= 0 m\", \"first_satisfied_tick\": "
           << bundle.termination.first_satisfied_tick
           << ", \"time_s\": " << bundle.termination.terminal_time_s
           << "}\n    },\n";

    const FixedCase failure_case = stageFailureCase();
    stream << "    \"CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE\": {\n"
              "      \"input\": ";
    writeFixedInput(stream, failure_case, "maximum_duration_s");
    stream << ",\n      \"evaluation_time_strictly_less_than_s\": 0.75,\n"
              "      \"committed_trajectory\": ";
    writeTrajectory(stream, bundle.stage_failure.committed_trajectory);
    stream << ",\n      \"failure\": {\"code\": ";
    writeJsonString(stream, bundle.stage_failure.code);
    stream << ", \"stage\": ";
    writeJsonString(stream, bundle.stage_failure.stage);
    stream << ", \"evaluation_time_s\": "
           << bundle.stage_failure.evaluation_time_s
           << ", \"failed_step_start_tick\": "
           << bundle.stage_failure.failed_step_start_tick
           << ", \"candidate_disposition\": \"discarded\", "
              "\"last_committed_tick\": "
           << bundle.stage_failure.committed_trajectory.back().tick
           << "}\n    }\n  },\n";

    stream << "  \"invalid_input_cases\": [";
    for (std::size_t index = 0; index < bundle.invalid_inputs.size(); ++index) {
        const InvalidInputResult& result = bundle.invalid_inputs[index];
        if (index != 0U) {
            stream << ',';
        }
        stream << "\n    {\"id\": ";
        writeJsonString(stream, result.id);
        stream << ", \"status\": ";
        writeJsonString(stream, result.status);
        stream << '}';
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
        const BundleResult bundle = runBundle();
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

        std::cout << "minimal 3DoF probe cases=4 convergence_runs="
                  << bundle.convergence.size()
                  << " invalid_inputs=" << bundle.invalid_inputs.size()
                  << " status=" << (passed ? "pass" : "fail") << '\n';
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "minimal 3DoF probe error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

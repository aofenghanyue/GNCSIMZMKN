#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kOracleId =
    "ORACLE-YYZ-TWO-INTERVAL-MASS-COMMIT-001";
constexpr const char* kModelId =
    "MODEL-YYZ-TWO-INTERVAL-MASS-COMMIT-001";
constexpr const char* kMassModelId =
    "MODEL-YYZ-SCALAR-BURN-CONSTANT-GEOMETRY-001";
constexpr const char* kMassStateId = "mass.fixture.yyz.vehicle@1";
constexpr const char* kInertialFrameId =
    "frame.fixture.yyz.inertial-cartesian@1";
constexpr const char* kBodyFrameId = "frame.fixture.yyz.body@1";
constexpr const char* kClockDomain = "clock.fixture.yyz.simulation@1";
constexpr std::int64_t kConfigurationRevision = 11;
constexpr double kAbsoluteTolerance = 2.0e-12;
constexpr double kRelativeTolerance = 2.0e-12;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Quaternion {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Matrix3 {
    std::array<std::array<double, 3>, 3> values{};
};

struct RigidState {
    Vec3 position_i_m;
    Vec3 velocity_i_mps;
    Quaternion q_i_b;
    Vec3 omega_bi_b_radps;
};

struct MassInput {
    std::int64_t sample_tick = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    std::string mass_state_id;
    std::string body_frame_id;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    double mass_kg = 0.0;
    Vec3 r_body_origin_to_com_b_m;
    Matrix3 inertia_about_com_b_kgm2;
};

struct IntervalInput {
    std::int64_t sample_tick = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    std::string inertial_frame_id;
    std::string body_frame_id;
    std::string clock_domain;
    std::string mass_state_id;
    std::int64_t configuration_revision = 0;
    Vec3 force_total_b_n;
    Vec3 moment_total_about_com_b_nm;
    Vec3 gravity_i_mps2;
    double fuel_consumption_rate_kgps = 0.0;
};

struct Input {
    std::string id;
    double dt_s = 0.0;
    std::int64_t initial_sample_tick = 0;
    RigidState initial_state;
    MassInput committed_mass;
    std::array<IntervalInput, 2> intervals;
    std::string terminal_kind;
    std::int64_t terminal_tick = 0;
};

struct ClosingCommit {
    std::int64_t tick = 0;
    std::string kind;
    double mass_kg = 0.0;
    Vec3 r_body_origin_to_com_b_m;
    Matrix3 inertia_about_com_b_kgm2;
    RigidState rigid_state;
};

struct IntervalResult {
    std::int64_t sample_tick = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    double current_committed_mass_kg = 0.0;
    double integration_mass_kg = 0.0;
    double fuel_consumption_rate_kgps = 0.0;
    double consumed_mass_kg = 0.0;
    double pending_mass_candidate_kg = 0.0;
    std::string pending_visibility_before_commit;
    Vec3 held_force_total_b_n;
    Vec3 held_moment_total_about_com_b_nm;
    Vec3 gravity_i_mps2;
    Vec3 acceleration_i_mps2;
    RigidState initial_rigid_state;
    RigidState rigid_candidate;
    ClosingCommit closing_commit;
};

struct Terminal {
    std::int64_t tick = 0;
    double time_s = 0.0;
    std::string termination_kind;
    double committed_mass_kg = 0.0;
    Vec3 r_body_origin_to_com_b_m;
    Matrix3 inertia_about_com_b_kgm2;
    RigidState rigid_state;
};

struct Evaluation {
    std::string id;
    std::string model_id;
    std::string mass_evolution_model_id;
    double dt_s = 0.0;
    std::array<IntervalResult, 2> intervals;
    Terminal terminal;
};

struct Options {
    bool early_visibility = false;
    bool stale_next_mass = false;
    bool stale_rigid_state = false;
    int substeps = 1;
};

struct EquivalenceResult {
    std::string id;
    std::string status;
    RigidState full_step_terminal;
    RigidState two_substep_terminal;
    double full_step_terminal_mass_kg = 0.0;
    double two_substep_terminal_mass_kg = 0.0;
    double max_abs_physical_difference = 0.0;
};

enum class MutationKind { EarlyVisibility, StaleNextMass, StaleRigidState };

struct MutationResult {
    std::string id;
    std::string status;
    MutationKind kind = MutationKind::EarlyVisibility;
    Evaluation observed;
    double max_abs_physical_difference = 0.0;
};

struct ProbeResult {
    Evaluation accepted;
    EquivalenceResult equivalence;
    std::vector<std::string> invalid_input_rejections;
    std::vector<MutationResult> mutations;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireDomain(bool condition, const std::string& message) {
    if (!condition) {
        throw std::domain_error(message);
    }
}

bool finite(double value) {
    return std::isfinite(value);
}

bool finite(const Vec3& value) {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool finite(const Quaternion& value) {
    return finite(value.w) && finite(value.x) && finite(value.y) &&
           finite(value.z);
}

bool finite(const Matrix3& value) {
    return std::all_of(
        value.values.begin(), value.values.end(), [](const auto& row) {
            return std::all_of(row.begin(), row.end(),
                               [](double item) { return finite(item); });
        });
}

double canonicalZero(double value) {
    return value == 0.0 ? 0.0 : value;
}

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return {canonicalZero(lhs.x + rhs.x), canonicalZero(lhs.y + rhs.y),
            canonicalZero(lhs.z + rhs.z)};
}

Vec3 scale(const Vec3& value, double factor) {
    return {canonicalZero(value.x * factor),
            canonicalZero(value.y * factor),
            canonicalZero(value.z * factor)};
}

Quaternion scale(const Quaternion& value, double factor) {
    return {canonicalZero(value.w * factor), canonicalZero(value.x * factor),
            canonicalZero(value.y * factor), canonicalZero(value.z * factor)};
}

double dot(const Quaternion& lhs, const Quaternion& rhs) {
    return lhs.w * rhs.w + lhs.x * rhs.x + lhs.y * rhs.y +
           lhs.z * rhs.z;
}

Quaternion normalize(const Quaternion& value) {
    requireDomain(finite(value), "q_I_B contains a non-finite value");
    const double magnitude = std::sqrt(dot(value, value));
    requireDomain(finite(magnitude) && magnitude > 0.0,
                  "q_I_B must have nonzero finite norm");
    return scale(value, 1.0 / magnitude);
}

bool near(double actual, double expected) {
    const double difference = std::abs(actual - expected);
    const double bound = kAbsoluteTolerance + kRelativeTolerance *
        std::max({1.0, std::abs(actual), std::abs(expected)});
    return difference <= bound;
}

double maxDifference(const Vec3& lhs, const Vec3& rhs) {
    return std::max({std::abs(lhs.x - rhs.x), std::abs(lhs.y - rhs.y),
                     std::abs(lhs.z - rhs.z)});
}

void validateInertia(const Matrix3& inertia) {
    requireDomain(finite(inertia), "inertia contains a non-finite value");
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            requireDomain(inertia.values[row][column] ==
                              inertia.values[column][row],
                          "inertia must be symmetric");
        }
    }
    std::array<std::array<double, 3>, 3> lower{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column <= row; ++column) {
            double residual = inertia.values[row][column];
            for (std::size_t index = 0; index < column; ++index) {
                residual -= lower[row][index] * lower[column][index];
            }
            if (row == column) {
                requireDomain(finite(residual) && residual > 0.0,
                              "inertia must be positive definite");
                lower[row][column] = std::sqrt(residual);
            } else {
                lower[row][column] = residual / lower[column][column];
            }
        }
    }
}

void validateMassIdentity(const MassInput& mass) {
    requireDomain(mass.sample_tick == 0 && mass.valid_from_tick == 0 &&
                      mass.valid_until_tick == 1,
                  "committed mass sample identity differs");
    requireDomain(mass.mass_state_id == kMassStateId &&
                      mass.body_frame_id == kBodyFrameId &&
                      mass.clock_domain == kClockDomain &&
                      mass.configuration_revision == kConfigurationRevision,
                  "committed mass contract identity differs");
}

void validateInterval(const IntervalInput& interval, std::size_t index) {
    const auto tick = static_cast<std::int64_t>(index);
    requireDomain(interval.sample_tick == tick &&
                      interval.valid_from_tick == tick &&
                      interval.valid_until_tick == tick + 1,
                  "sample or interval identity differs");
    requireDomain(interval.inertial_frame_id == kInertialFrameId &&
                      interval.body_frame_id == kBodyFrameId &&
                      interval.clock_domain == kClockDomain &&
                      interval.mass_state_id == kMassStateId &&
                      interval.configuration_revision ==
                          kConfigurationRevision,
                  "interval contract identity differs");
    requireDomain(finite(interval.force_total_b_n) &&
                      finite(interval.moment_total_about_com_b_nm) &&
                      finite(interval.gravity_i_mps2) &&
                      finite(interval.fuel_consumption_rate_kgps) &&
                      interval.fuel_consumption_rate_kgps >= 0.0,
                  "interval physical input is outside its domain");
    requireDomain(maxDifference(interval.moment_total_about_com_b_nm,
                                {0.0, 0.0, 0.0}) == 0.0,
                  "analytic profile requires zero held moment");
}

void validateInput(const Input& input) {
    requireDomain(finite(input.dt_s) && input.dt_s > 0.0,
                  "base dt must be positive and finite");
    requireDomain(input.initial_sample_tick == 0 &&
                      finite(input.initial_state.position_i_m) &&
                      finite(input.initial_state.velocity_i_mps) &&
                      finite(input.initial_state.omega_bi_b_radps),
                  "initial rigid state is outside its domain");
    const Quaternion attitude = normalize(input.initial_state.q_i_b);
    requireDomain(std::abs(attitude.w) == 1.0 && attitude.x == 0.0 &&
                      attitude.y == 0.0 && attitude.z == 0.0,
                  "analytic profile requires identity attitude up to sign");
    requireDomain(maxDifference(input.initial_state.omega_bi_b_radps,
                                {0.0, 0.0, 0.0}) == 0.0,
                  "analytic profile requires zero angular rate");
    validateMassIdentity(input.committed_mass);
    requireDomain(finite(input.committed_mass.mass_kg) &&
                      input.committed_mass.mass_kg > 0.0 &&
                      finite(input.committed_mass.r_body_origin_to_com_b_m),
                  "committed mass state is outside its domain");
    validateInertia(input.committed_mass.inertia_about_com_b_kgm2);
    double current_mass = input.committed_mass.mass_kg;
    for (std::size_t index = 0; index < input.intervals.size(); ++index) {
        validateInterval(input.intervals[index], index);
        current_mass -= input.intervals[index].fuel_consumption_rate_kgps *
                        input.dt_s;
        requireDomain(finite(current_mass) && current_mass > 0.0,
                      "mass candidate must remain positive");
    }
    requireDomain(input.terminal_kind == "duration_exact_grid" &&
                      input.terminal_tick == 2,
                  "terminal identity differs");
}

Input acceptedInput() {
    Input input;
    input.id = "CASE-YYZ-TWO-INTERVAL-MASS-COMMIT-TRAJECTORY";
    input.dt_s = 0.1;
    input.initial_sample_tick = 0;
    input.initial_state = {
        {0.0, 0.0, 0.0},
        {10.0, 0.0, 0.0},
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
    };
    input.committed_mass = {
        0, 0, 1, kMassStateId, kBodyFrameId, kClockDomain,
        kConfigurationRevision, 120.0, {0.2, -0.1, 0.05}, {}};
    input.committed_mass.inertia_about_com_b_kgm2.values = {{
        {{12.0, 1.0, 0.5}},
        {{1.0, 20.0, 2.0}},
        {{0.5, 2.0, 30.0}},
    }};
    for (std::size_t index = 0; index < input.intervals.size(); ++index) {
        const auto tick = static_cast<std::int64_t>(index);
        input.intervals[index] = {
            tick,
            tick,
            tick + 1,
            kInertialFrameId,
            kBodyFrameId,
            kClockDomain,
            kMassStateId,
            kConfigurationRevision,
            {240.0, 0.0, 0.0},
            {0.0, 0.0, 0.0},
            {0.0, 0.0, 0.0},
            0.5,
        };
    }
    input.terminal_kind = "duration_exact_grid";
    input.terminal_tick = 2;
    return input;
}

struct Derivative {
    Vec3 position;
    Vec3 velocity;
};

RigidState addScaled(const RigidState& state, const Derivative& derivative,
                     double factor) {
    RigidState result = state;
    result.position_i_m = add(state.position_i_m,
                              scale(derivative.position, factor));
    result.velocity_i_mps = add(state.velocity_i_mps,
                                scale(derivative.velocity, factor));
    return result;
}

Derivative derivative(const RigidState& state, const Vec3& acceleration) {
    return {state.velocity_i_mps, acceleration};
}

Vec3 weighted(const Vec3& first, const Vec3& second,
              const Vec3& third, const Vec3& fourth) {
    return scale(add(add(first, scale(second, 2.0)),
                     add(scale(third, 2.0), fourth)), 1.0 / 6.0);
}

RigidState rk4Step(const RigidState& committed, const Vec3& acceleration,
                   double dt_s) {
    const Derivative k1 = derivative(committed, acceleration);
    const Derivative k2 = derivative(
        addScaled(committed, k1, 0.5 * dt_s), acceleration);
    const Derivative k3 = derivative(
        addScaled(committed, k2, 0.5 * dt_s), acceleration);
    const Derivative k4 = derivative(
        addScaled(committed, k3, dt_s), acceleration);
    const Derivative combined{
        weighted(k1.position, k2.position, k3.position, k4.position),
        weighted(k1.velocity, k2.velocity, k3.velocity, k4.velocity),
    };
    RigidState candidate = addScaled(committed, combined, dt_s);
    candidate.q_i_b = normalize(candidate.q_i_b);
    return candidate;
}

RigidState advance(const RigidState& state, const Vec3& acceleration,
                   double dt_s, int substeps) {
    requireDomain(substeps > 0, "substep count must be positive");
    RigidState result = state;
    const double substep_dt = dt_s / static_cast<double>(substeps);
    for (int index = 0; index < substeps; ++index) {
        result = rk4Step(result, acceleration, substep_dt);
    }
    return result;
}

Evaluation evaluate(const Input& input, const Options& options = {}) {
    validateInput(input);
    const double opening_mass = input.committed_mass.mass_kg;
    double committed_mass = opening_mass;
    RigidState committed_state = input.initial_state;
    std::array<IntervalResult, 2> intervals{};
    for (std::size_t index = 0; index < input.intervals.size(); ++index) {
        const IntervalInput& interval = input.intervals[index];
        const double consumed = interval.fuel_consumption_rate_kgps *
                                input.dt_s;
        const double candidate_mass = committed_mass - consumed;
        double integration_mass = committed_mass;
        if (index == 0 && options.early_visibility) {
            integration_mass = candidate_mass;
        }
        if (index == 1 && options.stale_next_mass) {
            integration_mass = opening_mass;
        }
        RigidState interval_initial = committed_state;
        if (index == 1 && options.stale_rigid_state) {
            interval_initial = input.initial_state;
        }
        const Vec3 acceleration = add(
            scale(interval.force_total_b_n, 1.0 / integration_mass),
            interval.gravity_i_mps2);
        const RigidState rigid_candidate = advance(
            interval_initial, acceleration, input.dt_s, options.substeps);
        const ClosingCommit commit{
            interval.valid_until_tick,
            "atomic-rigid-and-mass",
            candidate_mass,
            input.committed_mass.r_body_origin_to_com_b_m,
            input.committed_mass.inertia_about_com_b_kgm2,
            rigid_candidate,
        };
        intervals[index] = {
            interval.sample_tick,
            interval.valid_from_tick,
            interval.valid_until_tick,
            committed_mass,
            integration_mass,
            interval.fuel_consumption_rate_kgps,
            consumed,
            candidate_mass,
            "candidate-only",
            interval.force_total_b_n,
            interval.moment_total_about_com_b_nm,
            interval.gravity_i_mps2,
            acceleration,
            interval_initial,
            rigid_candidate,
            commit,
        };
        committed_mass = candidate_mass;
        committed_state = rigid_candidate;
    }
    return {
        input.id,
        kModelId,
        kMassModelId,
        input.dt_s,
        intervals,
        {input.terminal_tick,
         static_cast<double>(input.terminal_tick) * input.dt_s,
         input.terminal_kind,
         committed_mass,
         input.committed_mass.r_body_origin_to_com_b_m,
         input.committed_mass.inertia_about_com_b_kgm2,
         committed_state},
    };
}

void append(std::vector<double>& destination, const Vec3& value) {
    destination.push_back(value.x);
    destination.push_back(value.y);
    destination.push_back(value.z);
}

void append(std::vector<double>& destination, const Quaternion& value) {
    destination.push_back(value.w);
    destination.push_back(value.x);
    destination.push_back(value.y);
    destination.push_back(value.z);
}

void append(std::vector<double>& destination, const RigidState& value) {
    append(destination, value.position_i_m);
    append(destination, value.velocity_i_mps);
    append(destination, value.q_i_b);
    append(destination, value.omega_bi_b_radps);
}

std::vector<double> physicalVector(const Evaluation& value) {
    std::vector<double> result;
    for (const IntervalResult& interval : value.intervals) {
        result.push_back(interval.current_committed_mass_kg);
        result.push_back(interval.integration_mass_kg);
        result.push_back(interval.consumed_mass_kg);
        result.push_back(interval.pending_mass_candidate_kg);
        append(result, interval.acceleration_i_mps2);
        append(result, interval.initial_rigid_state);
        append(result, interval.rigid_candidate);
    }
    result.push_back(value.terminal.committed_mass_kg);
    append(result, value.terminal.rigid_state);
    return result;
}

double maxDifference(const std::vector<double>& lhs,
                     const std::vector<double>& rhs) {
    require(lhs.size() == rhs.size(), "physical vector shape differs");
    double maximum = 0.0;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        maximum = std::max(maximum, std::abs(lhs[index] - rhs[index]));
    }
    return maximum;
}

void expectDomainRejection(
    std::vector<std::string>& rejected, const std::string& id,
    const std::function<void(Input&)>& mutate) {
    Input input = acceptedInput();
    mutate(input);
    try {
        static_cast<void>(evaluate(input));
    } catch (const std::domain_error&) {
        rejected.push_back(id);
        return;
    }
    throw std::runtime_error("invalid two-interval input survived: " + id);
}

ProbeResult runProbe() {
    const Input input = acceptedInput();
    const Evaluation accepted = evaluate(input);
    require(near(accepted.intervals[0].integration_mass_kg, 120.0) &&
                near(accepted.intervals[0].pending_mass_candidate_kg,
                     119.95) &&
                near(accepted.intervals[0].rigid_candidate.position_i_m.x,
                     1.01) &&
                near(accepted.intervals[0].rigid_candidate.velocity_i_mps.x,
                     10.2) &&
                near(accepted.intervals[1].integration_mass_kg, 119.95) &&
                near(accepted.intervals[1].acceleration_i_mps2.x,
                     4800.0 / 2399.0) &&
                near(accepted.terminal.committed_mass_kg, 119.9) &&
                near(accepted.terminal.rigid_state.position_i_m.x,
                     2.0400041684035015) &&
                near(accepted.terminal.rigid_state.velocity_i_mps.x,
                     10.40008336807003),
            "accepted two-interval anchors differ");

    Options substep_options;
    substep_options.substeps = 2;
    const Evaluation substeps = evaluate(input, substep_options);
    const double substep_difference = maxDifference(
        physicalVector(accepted), physicalVector(substeps));
    require(substep_difference <= kAbsoluteTolerance,
            "RK4 substep partition changed the terminal result");
    const EquivalenceResult equivalence{
        "EQUIV-YYZ-TWO-INTERVAL-MASS-COMMIT-RK4-SUBSTEPS",
        "passed",
        accepted.terminal.rigid_state,
        substeps.terminal.rigid_state,
        accepted.terminal.committed_mass_kg,
        substeps.terminal.committed_mass_kg,
        substep_difference,
    };

    std::vector<std::string> invalid;
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-STATE-ID",
        [](Input& value) {
            value.intervals[0].mass_state_id = "mass.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-FRAME",
        [](Input& value) {
            value.intervals[1].body_frame_id = "frame.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-CLOCK",
        [](Input& value) {
            value.committed_mass.clock_domain = "clock.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-REVISION",
        [](Input& value) {
            value.intervals[1].configuration_revision = 12;
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-SAMPLE-TICK",
        [](Input& value) { value.intervals[0].sample_tick = 1; });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-INTERVAL-SEQUENCE",
        [](Input& value) {
            value.intervals[1].sample_tick = 2;
            value.intervals[1].valid_from_tick = 2;
            value.intervals[1].valid_until_tick = 3;
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NONPOSITIVE-DT",
        [](Input& value) { value.dt_s = 0.0; });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NONPOSITIVE-MASS",
        [](Input& value) { value.committed_mass.mass_kg = 0.0; });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NEGATIVE-FLOW",
        [](Input& value) {
            value.intervals[0].fuel_consumption_rate_kgps = -1.0;
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-DEPLETION",
        [](Input& value) {
            value.intervals[0].fuel_consumption_rate_kgps = 1200.0;
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NONZERO-MOMENT",
        [](Input& value) {
            value.intervals[0].moment_total_about_com_b_nm = {0.0, 1.0, 0.0};
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NONZERO-RATE",
        [](Input& value) {
            value.initial_state.omega_bi_b_radps = {0.0, 0.0, 1.0};
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-ZERO-QUATERNION",
        [](Input& value) {
            value.initial_state.q_i_b = {0.0, 0.0, 0.0, 0.0};
        });

    Options early_options;
    early_options.early_visibility = true;
    const Evaluation early = evaluate(input, early_options);
    Options stale_mass_options;
    stale_mass_options.stale_next_mass = true;
    const Evaluation stale_mass = evaluate(input, stale_mass_options);
    Options stale_rigid_options;
    stale_rigid_options.stale_rigid_state = true;
    const Evaluation stale_rigid = evaluate(input, stale_rigid_options);
    std::vector<MutationResult> mutations{
        {"MUTATION-YYZ-TWO-INTERVAL-MASS-COMMIT-EARLY-VISIBILITY",
         "rejected", MutationKind::EarlyVisibility, early,
         maxDifference(physicalVector(accepted), physicalVector(early))},
        {"MUTATION-YYZ-TWO-INTERVAL-MASS-COMMIT-STALE-NEXT-MASS",
         "rejected", MutationKind::StaleNextMass, stale_mass,
         maxDifference(physicalVector(accepted), physicalVector(stale_mass))},
        {"MUTATION-YYZ-TWO-INTERVAL-MASS-COMMIT-STALE-RIGID-STATE",
         "rejected", MutationKind::StaleRigidState, stale_rigid,
         maxDifference(physicalVector(accepted), physicalVector(stale_rigid))},
    };
    require(near(mutations[0].max_abs_physical_difference, 0.05) &&
                near(mutations[1].max_abs_physical_difference, 0.05) &&
                near(mutations[2].max_abs_physical_difference, 1.03),
            "a two-interval temporal mutation matched the accepted result");
    return {accepted, equivalence, invalid, mutations};
}

void writeNumber(double value) {
    std::cout << canonicalZero(value);
}

void writeVec3(const Vec3& value) {
    std::cout << '[';
    writeNumber(value.x);
    std::cout << ',';
    writeNumber(value.y);
    std::cout << ',';
    writeNumber(value.z);
    std::cout << ']';
}

void writeQuaternion(const Quaternion& value) {
    std::cout << '[';
    writeNumber(value.w);
    std::cout << ',';
    writeNumber(value.x);
    std::cout << ',';
    writeNumber(value.y);
    std::cout << ',';
    writeNumber(value.z);
    std::cout << ']';
}

void writeMatrix(const Matrix3& value) {
    std::cout << '[';
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            if (row != 0 || column != 0) {
                std::cout << ',';
            }
            writeNumber(value.values[row][column]);
        }
    }
    std::cout << ']';
}

void writeRigidState(const RigidState& value) {
    std::cout << "{\"position_I_m\":";
    writeVec3(value.position_i_m);
    std::cout << ",\"velocity_I_mps\":";
    writeVec3(value.velocity_i_mps);
    std::cout << ",\"q_I_B_wxyz\":";
    writeQuaternion(value.q_i_b);
    std::cout << ",\"omega_BI_B_radps\":";
    writeVec3(value.omega_bi_b_radps);
    std::cout << '}';
}

void writeClosingCommit(const ClosingCommit& value) {
    std::cout << "{\"tick\":" << value.tick
              << ",\"kind\":\"" << value.kind << "\",\"mass_kg\":";
    writeNumber(value.mass_kg);
    std::cout << ",\"r_body_origin_to_CoM_B_m\":";
    writeVec3(value.r_body_origin_to_com_b_m);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    writeMatrix(value.inertia_about_com_b_kgm2);
    std::cout << ",\"rigid_state\":";
    writeRigidState(value.rigid_state);
    std::cout << '}';
}

void writeInterval(const IntervalResult& value) {
    std::cout << "{\"sample_tick\":" << value.sample_tick
              << ",\"valid_from_tick\":" << value.valid_from_tick
              << ",\"valid_until_tick\":" << value.valid_until_tick
              << ",\"current_committed_mass_kg\":";
    writeNumber(value.current_committed_mass_kg);
    std::cout << ",\"integration_mass_kg\":";
    writeNumber(value.integration_mass_kg);
    std::cout << ",\"fuel_consumption_rate_kgps\":";
    writeNumber(value.fuel_consumption_rate_kgps);
    std::cout << ",\"consumed_mass_kg\":";
    writeNumber(value.consumed_mass_kg);
    std::cout << ",\"pending_mass_candidate_kg\":";
    writeNumber(value.pending_mass_candidate_kg);
    std::cout << ",\"pending_visibility_before_commit\":\""
              << value.pending_visibility_before_commit
              << "\",\"held_force_total_B_N\":";
    writeVec3(value.held_force_total_b_n);
    std::cout << ",\"held_moment_total_about_CoM_B_Nm\":";
    writeVec3(value.held_moment_total_about_com_b_nm);
    std::cout << ",\"gravity_I_mps2\":";
    writeVec3(value.gravity_i_mps2);
    std::cout << ",\"acceleration_I_mps2\":";
    writeVec3(value.acceleration_i_mps2);
    std::cout << ",\"initial_rigid_state\":";
    writeRigidState(value.initial_rigid_state);
    std::cout << ",\"rigid_candidate\":";
    writeRigidState(value.rigid_candidate);
    std::cout << ",\"closing_commit\":";
    writeClosingCommit(value.closing_commit);
    std::cout << '}';
}

void writeEvaluation(const Evaluation& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"model_id\":\"" << value.model_id
              << "\",\"mass_evolution_model_id\":\""
              << value.mass_evolution_model_id << "\",\"base_dt_s\":";
    writeNumber(value.dt_s);
    std::cout << ",\"intervals\":[";
    writeInterval(value.intervals[0]);
    std::cout << ',';
    writeInterval(value.intervals[1]);
    std::cout << "],\"terminal\":{\"tick\":" << value.terminal.tick
              << ",\"time_s\":";
    writeNumber(value.terminal.time_s);
    std::cout << ",\"termination_kind\":\""
              << value.terminal.termination_kind
              << "\",\"committed_mass_kg\":";
    writeNumber(value.terminal.committed_mass_kg);
    std::cout << ",\"r_body_origin_to_CoM_B_m\":";
    writeVec3(value.terminal.r_body_origin_to_com_b_m);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    writeMatrix(value.terminal.inertia_about_com_b_kgm2);
    std::cout << ",\"rigid_state\":";
    writeRigidState(value.terminal.rigid_state);
    std::cout << "}}";
}

void writeStringList(const std::vector<std::string>& values) {
    std::cout << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << '\"' << values[index] << '\"';
    }
    std::cout << ']';
}

void writeMutation(const MutationResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"status\":\"" << value.status << '\"';
    if (value.kind == MutationKind::EarlyVisibility) {
        std::cout << ",\"observed_interval0_integration_mass_kg\":";
        writeNumber(value.observed.intervals[0].integration_mass_kg);
        std::cout << ",\"observed_tick1_rigid_state\":";
        writeRigidState(value.observed.intervals[0].rigid_candidate);
        std::cout << ",\"observed_terminal_rigid_state\":";
        writeRigidState(value.observed.terminal.rigid_state);
    } else if (value.kind == MutationKind::StaleNextMass) {
        std::cout << ",\"observed_interval1_committed_mass_kg\":";
        writeNumber(value.observed.intervals[1].current_committed_mass_kg);
        std::cout << ",\"observed_interval1_integration_mass_kg\":";
        writeNumber(value.observed.intervals[1].integration_mass_kg);
        std::cout << ",\"observed_interval1_acceleration_I_mps2\":";
        writeVec3(value.observed.intervals[1].acceleration_i_mps2);
        std::cout << ",\"observed_terminal_rigid_state\":";
        writeRigidState(value.observed.terminal.rigid_state);
    } else {
        std::cout << ",\"observed_interval1_initial_rigid_state\":";
        writeRigidState(value.observed.intervals[1].initial_rigid_state);
        std::cout << ",\"observed_interval1_committed_mass_kg\":";
        writeNumber(value.observed.intervals[1].current_committed_mass_kg);
        std::cout << ",\"observed_terminal_rigid_state\":";
        writeRigidState(value.observed.terminal.rigid_state);
    }
    std::cout << ",\"max_abs_physical_difference\":";
    writeNumber(value.max_abs_physical_difference);
    std::cout << '}';
}

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"model_id\":\"" << kModelId
              << "\",\"status\":\"passed\",\"cases\":[";
    writeEvaluation(result.accepted);
    std::cout << "],\"equivalence_results\":[{\"id\":\""
              << result.equivalence.id << "\",\"status\":\""
              << result.equivalence.status
              << "\",\"full_step_terminal_rigid_state\":";
    writeRigidState(result.equivalence.full_step_terminal);
    std::cout << ",\"two_substep_terminal_rigid_state\":";
    writeRigidState(result.equivalence.two_substep_terminal);
    std::cout << ",\"full_step_terminal_mass_kg\":";
    writeNumber(result.equivalence.full_step_terminal_mass_kg);
    std::cout << ",\"two_substep_terminal_mass_kg\":";
    writeNumber(result.equivalence.two_substep_terminal_mass_kg);
    std::cout << ",\"max_abs_physical_difference\":";
    writeNumber(result.equivalence.max_abs_physical_difference);
    std::cout << "}],\"invalid_input_rejections\":";
    writeStringList(result.invalid_input_rejections);
    std::cout << ",\"mutation_results\":[";
    for (std::size_t index = 0; index < result.mutations.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeMutation(result.mutations[index]);
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr <<
            "usage: gnc_yyz_two_interval_mass_commit_probe --self-check\n";
        return 2;
    }
    try {
        writeJson(runProbe());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId =
    "ORACLE-YYZ-MISSION-COMPOSITION-001";
constexpr const char* kModelId =
    "MODEL-YYZ-FIXTURE-MISSION-COMPOSITION-002";
constexpr const char* kMissionSourceId =
    "mission.fixture.yyz.lookup-open-loop@1";
constexpr const char* kExecutionId =
    "execution.fixture.yyz.lookup-open-loop.0001";
constexpr const char* kSubject = "vehicle.fixture.yyz@1";
constexpr const char* kInertialFrameId =
    "frame.fixture.yyz.inertial-cartesian@1";
constexpr const char* kBodyFrameId = "frame.fixture.yyz.body@1";
constexpr const char* kClockDomain = "clock.fixture.yyz.simulation@1";
constexpr const char* kMassStateId = "mass.fixture.yyz.vehicle@1";
constexpr const char* kConfigurationId =
    "configuration.fixture.yyz.clean@1";
constexpr std::int64_t kConfigurationRevision = 11;
constexpr double kDt = 0.1;
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

struct State {
    Vec3 position_i_m;
    Vec3 velocity_i_mps;
    Quaternion q_i_b;
    Vec3 omega_bi_b_radps;
};

struct Derivative {
    Vec3 position;
    Vec3 velocity;
    Quaternion attitude;
    Vec3 angular_rate;
};

struct StepResult {
    State state;
    double pre_normalization_quaternion_norm_residual = 0.0;
};

struct Binding {
    std::string role;
    std::string fixture_id;
    std::string oracle_id;
    std::string model_id;
};

struct Predicate {
    std::string predicate_id;
    std::string metric_id;
    std::string relation;
    double threshold = 0.0;
    std::string action;
    std::string reason_code;
    std::int64_t priority = 0;
};

struct Input {
    std::string mission_source_id;
    std::string execution_id;
    std::string composition_model_id;
    std::string subject;
    std::string inertial_frame_id;
    std::string body_frame_id;
    std::string clock_domain;
    std::string mass_state_id;
    std::string configuration_id;
    std::int64_t configuration_revision = 0;
    double dt_s = 0.0;
    std::int64_t initial_tick = 0;
    std::int64_t terminal_tick = 0;
    std::string integration_strategy;
    std::string commit_policy;
    std::string evaluation_mode;
    std::vector<Binding> bindings;
    std::vector<Predicate> predicates;
};

struct Environment {
    Vec3 gravity_i_mps2;
    Vec3 velocity_airmass_i_mps;
    double density_kgpm3 = 0.0;
    double speed_of_sound_mps = 0.0;
};

struct AirData {
    Vec3 velocity_relative_i_mps;
    Vec3 velocity_relative_b_mps;
    double airspeed_mps = 0.0;
    double alpha_rad = 0.0;
    double beta_rad = 0.0;
    double dynamic_pressure_pa = 0.0;
    double mach = 0.0;
};

struct AeroLookup {
    std::string model_id;
    std::string table_id;
    std::string configuration_id;
    std::string domain_status;
    std::array<std::int64_t, 3> cell_indices{};
    std::array<double, 3> weights{};
    std::array<double, 6> coefficients{};
};

struct Closure {
    Vec3 force_total_b_n;
    Vec3 moment_total_about_com_b_nm;
};

struct RigidDerivative {
    Vec3 force_total_i_n;
    Vec3 acceleration_i_mps2;
    Vec3 angular_momentum_b_kgm2ps;
    Vec3 gyroscopic_moment_b_nm;
    Vec3 net_moment_b_nm;
    Vec3 angular_acceleration_b_radps2;
    Quaternion q_derivative_i_b_per_s;
};

struct MassTransition {
    std::string mass_state_id;
    double opening_committed_mass_kg = 0.0;
    double consumed_mass_kg = 0.0;
    double pending_mass_candidate_kg = 0.0;
    std::string pending_visibility_before_commit;
    double closing_committed_mass_kg = 0.0;
    std::string closing_commit_kind;
};

struct IntervalExecution {
    std::int64_t sample_tick = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    std::int64_t configuration_revision = 0;
    double dt_s = 0.0;
    std::string strategy;
    std::int64_t integration_substeps = 1;
    Environment environment;
    AirData air_data;
    AeroLookup aero_lookup;
    Closure closure;
    RigidDerivative rigid_derivative;
    MassTransition mass_transition;
};

struct ConvergenceEntry {
    std::int64_t substeps = 0;
    State terminal_state;
    double maximum_pre_normalization_quaternion_norm_residual = 0.0;
};

struct ConvergenceEvidence {
    std::vector<std::int64_t> substep_counts;
    std::vector<ConvergenceEntry> terminal_states;
    std::vector<double> successive_max_abs_differences;
    std::vector<double> error_reduction_ratios;
    double minimum_required_error_reduction_ratio = 0.0;
};

struct CommittedSample {
    std::int64_t sample_tick = 0;
    double time_s = 0.0;
    std::string commit_id;
    std::string quality;
    State state;
    double committed_mass_kg = 0.0;
};

struct Metrics {
    double duration_s = 0.0;
    double downrange_m = 0.0;
    double vertical_displacement_m = 0.0;
    double remaining_mass_kg = 0.0;
    double consumed_mass_kg = 0.0;
    double speed_mps = 0.0;
};

struct PredicateResult {
    Predicate predicate;
    double observed = 0.0;
    bool met = false;
};

struct Decision {
    std::string action;
    std::string reason_code;
    double trigger_time_s = 0.0;
    std::string subject;
    std::int64_t priority = 0;
    Metrics metrics;
    std::string message_key;
};

struct EvaluationBoundary {
    std::int64_t sample_tick = 0;
    double time_s = 0.0;
    std::string commit_id;
    Metrics metrics;
    std::vector<PredicateResult> predicate_results;
    Decision decision;
};

struct MetricSummary {
    std::int64_t evaluated_sample_count = 0;
    double duration_s = 0.0;
    double downrange_m = 0.0;
    double vertical_displacement_m = 0.0;
    double remaining_mass_kg = 0.0;
    double consumed_mass_kg = 0.0;
    double terminal_speed_mps = 0.0;
    double peak_speed_mps = 0.0;
    std::int64_t peak_speed_tick = 0;
    double maximum_downrange_m = 0.0;
    std::int64_t maximum_downrange_tick = 0;
    double minimum_remaining_mass_kg = 0.0;
    std::int64_t minimum_remaining_mass_tick = 0;
};

struct TerminalObservation {
    std::int64_t sample_tick = 0;
    double time_s = 0.0;
    std::string commit_id;
    std::string quality;
    State state;
    double committed_mass_kg = 0.0;
    Metrics metrics;
    Decision decision;
    std::vector<std::string> event_order;
    bool sealed = false;
};

struct MissionResult {
    std::string final_status;
    std::string evidence_validity;
    std::int64_t initial_tick = 0;
    std::int64_t final_tick = 0;
    double final_time_s = 0.0;
    Decision termination;
    MetricSummary metrics;
    std::string terminal_observation_commit_id;
    bool terminal_observation_sealed = false;
    bool frozen = false;
};

struct Composition {
    std::string id;
    std::string mission_source_id;
    std::string execution_id;
    std::string composition_model_id;
    std::vector<Binding> resolved_components;
    std::vector<CommittedSample> committed_samples;
    std::vector<IntervalExecution> interval_executions;
    ConvergenceEvidence second_interval_convergence;
    std::vector<EvaluationBoundary> evaluation_trace;
    MetricSummary metric_summary;
    TerminalObservation terminal_observation;
    MissionResult mission_result;
};

struct Options {
    bool early_mass_visibility = false;
    bool nonatomic_mass_commit = false;
    bool stale_boundary_closure = false;
    bool low_priority_wins = false;
    bool result_before_observation = false;
};

struct ProbeResult {
    Composition accepted;
    std::vector<std::string> invalid_input_rejections;
    Composition early_mass;
    Composition nonatomic_mass;
    Composition stale_boundary_closure;
    Composition low_priority;
    Composition result_before_observation;
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

Vec3 canonicalZero(const Vec3& value) {
    return {canonicalZero(value.x), canonicalZero(value.y),
            canonicalZero(value.z)};
}

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero({lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z});
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero({lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z});
}

Vec3 scale(const Vec3& value, double factor) {
    return canonicalZero(
        {value.x * factor, value.y * factor, value.z * factor});
}

double dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero({
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    });
}

Quaternion add(const Quaternion& lhs, const Quaternion& rhs) {
    return {canonicalZero(lhs.w + rhs.w), canonicalZero(lhs.x + rhs.x),
            canonicalZero(lhs.y + rhs.y), canonicalZero(lhs.z + rhs.z)};
}

Quaternion scale(const Quaternion& value, double factor) {
    return {canonicalZero(value.w * factor),
            canonicalZero(value.x * factor),
            canonicalZero(value.y * factor),
            canonicalZero(value.z * factor)};
}

double dot(const Quaternion& lhs, const Quaternion& rhs) {
    return lhs.w * rhs.w + lhs.x * rhs.x + lhs.y * rhs.y +
           lhs.z * rhs.z;
}

double norm(const Quaternion& value) {
    return std::sqrt(dot(value, value));
}

Quaternion hamilton(const Quaternion& lhs, const Quaternion& rhs) {
    return {
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
    };
}

Quaternion conjugate(const Quaternion& value) {
    return {value.w, -value.x, -value.y, -value.z};
}

Quaternion normalize(const Quaternion& value) {
    requireDomain(finite(value), "q_I_B contains a non-finite component");
    const double magnitude = norm(value);
    requireDomain(finite(magnitude) && magnitude > 0.0,
                  "q_I_B must have nonzero finite norm");
    return scale(value, 1.0 / magnitude);
}

Vec3 inertialToBody(const Quaternion& q_i_b, const Vec3& value_i) {
    const Quaternion unit = normalize(q_i_b);
    const Quaternion pure{0.0, value_i.x, value_i.y, value_i.z};
    const Quaternion rotated = hamilton(hamilton(unit, pure), conjugate(unit));
    return canonicalZero({rotated.x, rotated.y, rotated.z});
}

Vec3 bodyToInertial(const Quaternion& q_i_b, const Vec3& value_b) {
    const Quaternion unit = normalize(q_i_b);
    const Quaternion pure{0.0, value_b.x, value_b.y, value_b.z};
    const Quaternion rotated = hamilton(hamilton(conjugate(unit), pure), unit);
    return canonicalZero({rotated.x, rotated.y, rotated.z});
}

Vec3 multiply(const Matrix3& matrix, const Vec3& value) {
    return {
        matrix.values[0][0] * value.x + matrix.values[0][1] * value.y +
            matrix.values[0][2] * value.z,
        matrix.values[1][0] * value.x + matrix.values[1][1] * value.y +
            matrix.values[1][2] * value.z,
        matrix.values[2][0] * value.x + matrix.values[2][1] * value.y +
            matrix.values[2][2] * value.z,
    };
}

std::array<std::array<double, 3>, 3> cholesky(const Matrix3& inertia) {
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
    return lower;
}

Vec3 solveSpd(const Matrix3& inertia, const Vec3& rhs) {
    const auto lower = cholesky(inertia);
    const std::array<double, 3> source{rhs.x, rhs.y, rhs.z};
    std::array<double, 3> forward{};
    for (std::size_t row = 0; row < 3; ++row) {
        double residual = source[row];
        for (std::size_t column = 0; column < row; ++column) {
            residual -= lower[row][column] * forward[column];
        }
        forward[row] = residual / lower[row][row];
    }
    std::array<double, 3> result{};
    for (std::size_t row = 3; row-- > 0;) {
        double residual = forward[row];
        for (std::size_t column = row + 1; column < 3; ++column) {
            residual -= lower[column][row] * result[column];
        }
        result[row] = residual / lower[row][row];
    }
    return canonicalZero({result[0], result[1], result[2]});
}

bool near(double actual, double expected) {
    const double difference = std::abs(actual - expected);
    const double bound = kAbsoluteTolerance + kRelativeTolerance *
        std::max({1.0, std::abs(actual), std::abs(expected)});
    return difference <= bound;
}

std::vector<std::string> eventOrder() {
    return {
        "resolve-components",
        "publish-opening-commit",
        "evaluate-opening-boundary",
        "evaluate-interval-0",
        "stage-commit-1",
        "commit-rigid-and-mass-1",
        "evaluate-intermediate-boundary",
        "evaluate-interval-1",
        "stage-commit-2",
        "commit-rigid-and-mass-2",
        "evaluate-terminal-boundary",
        "seal-terminal-observation",
        "freeze-mission-result",
    };
}

std::vector<Binding> expectedBindings() {
    return {
        {"rigid_body", "REF-YYZ-6DOF-CORE-001",
         "ORACLE-YYZ-6DOF-CORE-001",
         "MODEL-YYZ-6DOF-RIGID-CORE-001"},
        {"force_moment_closure", "REF-YYZ-FORCE-MOMENT-CLOSURE-001",
         "ORACLE-YYZ-FORCE-MOMENT-CLOSURE-001",
         "MODEL-YYZ-FORCE-MOMENT-CLOSURE-001"},
        {"air_data", "REF-YYZ-AIR-DATA-KINEMATICS-001",
         "ORACLE-YYZ-AIR-DATA-KINEMATICS-001",
         "MODEL-YYZ-AIR-DATA-KINEMATICS-001"},
        {"aero_dimensionalization", "REF-YYZ-AERO-DIMENSIONALIZATION-001",
         "ORACLE-YYZ-AERO-DIMENSIONALIZATION-001",
         "MODEL-YYZ-AERO-DIMENSIONALIZATION-001"},
        {"aero_lookup", "REF-YYZ-AERO-LOOKUP-001",
         "ORACLE-YYZ-AERO-LOOKUP-001",
         "MODEL-YYZ-AERO-TRILINEAR-LOOKUP-001"},
        {"environment", "REF-YYZ-UNIFORM-ENVIRONMENT-001",
         "ORACLE-YYZ-UNIFORM-ENVIRONMENT-001",
         "MODEL-YYZ-UNIFORM-ENVIRONMENT-001"},
        {"propulsion", "REF-YYZ-PROPULSION-RESPONSE-001",
         "ORACLE-YYZ-PROPULSION-RESPONSE-001",
         "MODEL-YYZ-PROPULSION-RESPONSE-001"},
        {"mass_properties", "REF-YYZ-MASS-PROPERTIES-001",
         "ORACLE-YYZ-MASS-PROPERTIES-001",
         "MODEL-YYZ-MASS-PROPERTIES-PROJECTION-001"},
        {"mass_evolution", "REF-YYZ-SCALAR-BURN-MASS-001",
         "ORACLE-YYZ-SCALAR-BURN-MASS-001",
         "MODEL-YYZ-SCALAR-BURN-CONSTANT-GEOMETRY-001"},
        {"frozen_interval", "REF-YYZ-FROZEN-INTERVAL-001",
         "ORACLE-YYZ-FROZEN-INTERVAL-001",
         "MODEL-YYZ-LOOKUP-FROZEN-INTERVAL-001"},
        {"atomic_mass_commit", "REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001",
         "ORACLE-YYZ-TWO-INTERVAL-MASS-COMMIT-001",
         "MODEL-YYZ-TWO-INTERVAL-MASS-COMMIT-001"},
        {"run_evaluation", "REF-YYZ-RUN-EVALUATION-001",
         "ORACLE-YYZ-RUN-EVALUATION-001",
         "MODEL-YYZ-RUN-EVALUATION-001"},
    };
}

bool supportedMetric(const std::string& value) {
    return value == "duration_s" || value == "downrange_m" ||
           value == "remaining_mass_kg";
}

void validateInput(const Input& input) {
    requireDomain(input.mission_source_id == kMissionSourceId &&
                      input.execution_id == kExecutionId &&
                      input.composition_model_id == kModelId &&
                      input.subject == kSubject,
                  "mission source or execution identity differs");
    requireDomain(input.inertial_frame_id == kInertialFrameId &&
                      input.body_frame_id == kBodyFrameId &&
                      input.clock_domain == kClockDomain &&
                      input.mass_state_id == kMassStateId &&
                      input.configuration_id == kConfigurationId,
                  "mission shared identity differs");
    requireDomain(input.configuration_revision == kConfigurationRevision &&
                      finite(input.dt_s) && near(input.dt_s, kDt) &&
                      input.initial_tick == 0 && input.terminal_tick == 2,
                  "mission revision, tick or dt differs");
    requireDomain(input.integration_strategy == "FrozenInterval" &&
                      input.commit_policy == "atomic-rigid-and-mass" &&
                      input.evaluation_mode == "AtGrid",
                  "mission execution policy differs");

    const std::vector<Binding> expected = expectedBindings();
    std::set<std::string> roles;
    for (const Binding& binding : input.bindings) {
        requireDomain(!binding.role.empty() &&
                          roles.insert(binding.role).second,
                      "component roles must be nonempty and unique");
        const auto found = std::find_if(
            expected.begin(), expected.end(), [&](const Binding& value) {
                return value.role == binding.role;
            });
        requireDomain(found != expected.end(),
                      "component role is unsupported");
        requireDomain(binding.fixture_id == found->fixture_id &&
                          binding.oracle_id == found->oracle_id &&
                          binding.model_id == found->model_id,
                      "component binding identity differs");
    }
    requireDomain(roles.size() == expected.size(),
                  "component roles differ from the required mission set");
    for (const Binding& binding : expected) {
        requireDomain(roles.count(binding.role) == 1,
                      "a required component role is missing");
    }

    requireDomain(!input.predicates.empty(),
                  "termination plan must contain predicates");
    std::set<std::string> predicate_ids;
    for (const Predicate& predicate : input.predicates) {
        requireDomain(!predicate.predicate_id.empty() &&
                          predicate_ids.insert(predicate.predicate_id).second,
                      "predicate ids must be nonempty and unique");
        requireDomain(supportedMetric(predicate.metric_id),
                      "unsupported predicate metric");
        requireDomain(predicate.relation == ">=" ||
                          predicate.relation == "<=",
                      "unsupported predicate relation");
        requireDomain(finite(predicate.threshold) &&
                          (predicate.action == "Complete" ||
                           predicate.action == "Abort") &&
                          !predicate.reason_code.empty() &&
                          predicate.priority >= 0,
                      "predicate definition is outside its domain");
    }
}

Input acceptedInput() {
    Input input;
    input.mission_source_id = kMissionSourceId;
    input.execution_id = kExecutionId;
    input.composition_model_id = kModelId;
    input.subject = kSubject;
    input.inertial_frame_id = kInertialFrameId;
    input.body_frame_id = kBodyFrameId;
    input.clock_domain = kClockDomain;
    input.mass_state_id = kMassStateId;
    input.configuration_id = kConfigurationId;
    input.configuration_revision = kConfigurationRevision;
    input.dt_s = kDt;
    input.initial_tick = 0;
    input.terminal_tick = 2;
    input.integration_strategy = "FrozenInterval";
    input.commit_policy = "atomic-rigid-and-mass";
    input.evaluation_mode = "AtGrid";
    input.bindings = expectedBindings();
    input.predicates = {
        {"remaining-mass-floor", "remaining_mass_kg", "<=", 99.85,
         "Abort", "remaining-mass-floor", 300},
        {"duration-limit", "duration_s", ">=", 0.2,
         "Complete", "duration-complete", 100},
        {"downrange-goal", "downrange_m", ">=", 20.0,
         "Complete", "downrange-goal", 200},
    };
    return input;
}

std::array<std::array<double, 6>, 8> aeroTable() {
    return {{
        {{0.006, 0.0245, -0.0795, 0.005, 0.014, -0.00755}},
        {{0.006, -0.0245, -0.0805, -0.005, 0.014, 0.00755}},
        {{0.05, 0.0245, 0.0795, 0.005, -0.106, -0.00785}},
        {{0.05, -0.0245, 0.0805, -0.005, -0.106, 0.00785}},
        {{0.018, 0.0235, -0.0795, 0.005, 0.022, -0.00795}},
        {{0.018, -0.0235, -0.0805, -0.005, 0.022, 0.00795}},
        {{0.07, 0.0235, 0.0795, 0.005, -0.098, -0.00825}},
        {{0.07, -0.0235, 0.0805, -0.005, -0.098, 0.00825}},
    }};
}

AeroLookup lookupAero(double mach, double alpha, double beta) {
    const std::array<double, 2> mach_axis{0.2, 0.6};
    const std::array<double, 2> alpha_axis{-0.1, 0.1};
    const std::array<double, 2> beta_axis{-0.05, 0.05};
    requireDomain(mach_axis[0] <= mach && mach <= mach_axis[1] &&
                      alpha_axis[0] <= alpha && alpha <= alpha_axis[1] &&
                      beta_axis[0] <= beta && beta <= beta_axis[1],
                  "aero query is outside the inclusive prepared table");
    const std::array<double, 3> weights{
        (mach - mach_axis[0]) / (mach_axis[1] - mach_axis[0]),
        (alpha - alpha_axis[0]) / (alpha_axis[1] - alpha_axis[0]),
        (beta - beta_axis[0]) / (beta_axis[1] - beta_axis[0]),
    };
    const auto table = aeroTable();
    std::array<double, 6> coefficients{};
    const auto rowIndex = [](std::size_t mach_index,
                             std::size_t alpha_index,
                             std::size_t beta_index) {
        return (mach_index * 2 + alpha_index) * 2 + beta_index;
    };
    for (std::size_t m = 0; m < 2; ++m) {
        const double m_factor = m == 0 ? 1.0 - weights[0] : weights[0];
        for (std::size_t a = 0; a < 2; ++a) {
            const double a_factor = a == 0 ? 1.0 - weights[1] : weights[1];
            for (std::size_t b = 0; b < 2; ++b) {
                const double b_factor =
                    b == 0 ? 1.0 - weights[2] : weights[2];
                const double factor = m_factor * a_factor * b_factor;
                const auto& row = table[rowIndex(m, a, b)];
                for (std::size_t index = 0;
                     index < coefficients.size(); ++index) {
                    coefficients[index] += factor * row[index];
                }
            }
        }
    }
    for (double& value : coefficients) {
        value = canonicalZero(value);
    }
    return {
        "MODEL-YYZ-AERO-TRILINEAR-LOOKUP-001",
        "aero-table.fixture.yyz.multiaffine@1",
        kConfigurationId,
        "Interior",
        {0, 0, 0},
        weights,
        coefficients,
    };
}

Matrix3 inertiaMatrix() {
    Matrix3 result;
    result.values = {{{10.0, 0.0, 0.0},
                      {0.0, 20.0, 0.0},
                      {0.0, 0.0, 30.0}}};
    return result;
}

RigidDerivative evaluateRigidDerivative(
    const State& state, double mass_kg, const Closure& closure,
    const Environment& environment) {
    requireDomain(finite(mass_kg) && mass_kg > 0.0,
                  "integration mass must be positive and finite");
    const Matrix3 inertia = inertiaMatrix();
    const Quaternion attitude = normalize(state.q_i_b);
    const Vec3 force_i = bodyToInertial(
        attitude, closure.force_total_b_n);
    const Vec3 acceleration = add(
        scale(force_i, 1.0 / mass_kg), environment.gravity_i_mps2);
    const Vec3 angular_momentum = multiply(
        inertia, state.omega_bi_b_radps);
    const Vec3 gyroscopic = cross(
        state.omega_bi_b_radps, angular_momentum);
    const Vec3 net_moment = subtract(
        closure.moment_total_about_com_b_nm, gyroscopic);
    const Vec3 angular_acceleration = solveSpd(inertia, net_moment);
    const Quaternion pure_omega{
        0.0, state.omega_bi_b_radps.x, state.omega_bi_b_radps.y,
        state.omega_bi_b_radps.z};
    const Quaternion q_derivative = scale(
        hamilton(pure_omega, attitude), -0.5);
    return {force_i, acceleration, angular_momentum, gyroscopic,
            net_moment, angular_acceleration, q_derivative};
}

Derivative stateDerivative(const State& state, double mass_kg,
                           const Closure& closure,
                           const Environment& environment) {
    const RigidDerivative rigid = evaluateRigidDerivative(
        state, mass_kg, closure, environment);
    return {state.velocity_i_mps, rigid.acceleration_i_mps2,
            rigid.q_derivative_i_b_per_s,
            rigid.angular_acceleration_b_radps2};
}

State addScaled(const State& state, const Derivative& change,
                double factor) {
    return {
        add(state.position_i_m, scale(change.position, factor)),
        add(state.velocity_i_mps, scale(change.velocity, factor)),
        add(state.q_i_b, scale(change.attitude, factor)),
        add(state.omega_bi_b_radps,
            scale(change.angular_rate, factor)),
    };
}

Vec3 weighted(const Vec3& first, const Vec3& second,
              const Vec3& third, const Vec3& fourth) {
    return scale(add(add(first, scale(second, 2.0)),
                     add(scale(third, 2.0), fourth)), 1.0 / 6.0);
}

Quaternion weighted(const Quaternion& first, const Quaternion& second,
                    const Quaternion& third, const Quaternion& fourth) {
    return scale(add(add(first, scale(second, 2.0)),
                     add(scale(third, 2.0), fourth)), 1.0 / 6.0);
}

StepResult rk4Step(const State& committed, double dt_s, double mass_kg,
                   const Closure& closure,
                   const Environment& environment) {
    requireDomain(finite(dt_s) && dt_s > 0.0,
                  "RK4 dt must be positive and finite");
    const Derivative k1 = stateDerivative(
        committed, mass_kg, closure, environment);
    const Derivative k2 = stateDerivative(
        addScaled(committed, k1, 0.5 * dt_s), mass_kg, closure,
        environment);
    const Derivative k3 = stateDerivative(
        addScaled(committed, k2, 0.5 * dt_s), mass_kg, closure,
        environment);
    const Derivative k4 = stateDerivative(
        addScaled(committed, k3, dt_s), mass_kg, closure, environment);
    const Derivative combined{
        weighted(k1.position, k2.position, k3.position, k4.position),
        weighted(k1.velocity, k2.velocity, k3.velocity, k4.velocity),
        weighted(k1.attitude, k2.attitude, k3.attitude, k4.attitude),
        weighted(k1.angular_rate, k2.angular_rate, k3.angular_rate,
                 k4.angular_rate),
    };
    State candidate = addScaled(committed, combined, dt_s);
    const double norm_residual = std::abs(norm(candidate.q_i_b) - 1.0);
    candidate.q_i_b = normalize(candidate.q_i_b);
    return {candidate, norm_residual};
}

ConvergenceEntry integrateSubsteps(
    const State& opening, double mass_kg, const Closure& closure,
    const Environment& environment, std::int64_t substeps) {
    requireDomain(substeps > 0, "RK4 substep count must be positive");
    State state = opening;
    double maximum_residual = 0.0;
    const double step_s = kDt / static_cast<double>(substeps);
    for (std::int64_t index = 0; index < substeps; ++index) {
        const StepResult step = rk4Step(
            state, step_s, mass_kg, closure, environment);
        state = step.state;
        maximum_residual = std::max(
            maximum_residual,
            step.pre_normalization_quaternion_norm_residual);
    }
    return {substeps, state, maximum_residual};
}

double stateMaxDifference(const State& lhs, const State& rhs) {
    return std::max({
        std::abs(lhs.position_i_m.x - rhs.position_i_m.x),
        std::abs(lhs.position_i_m.y - rhs.position_i_m.y),
        std::abs(lhs.position_i_m.z - rhs.position_i_m.z),
        std::abs(lhs.velocity_i_mps.x - rhs.velocity_i_mps.x),
        std::abs(lhs.velocity_i_mps.y - rhs.velocity_i_mps.y),
        std::abs(lhs.velocity_i_mps.z - rhs.velocity_i_mps.z),
        std::abs(lhs.q_i_b.w - rhs.q_i_b.w),
        std::abs(lhs.q_i_b.x - rhs.q_i_b.x),
        std::abs(lhs.q_i_b.y - rhs.q_i_b.y),
        std::abs(lhs.q_i_b.z - rhs.q_i_b.z),
        std::abs(lhs.omega_bi_b_radps.x - rhs.omega_bi_b_radps.x),
        std::abs(lhs.omega_bi_b_radps.y - rhs.omega_bi_b_radps.y),
        std::abs(lhs.omega_bi_b_radps.z - rhs.omega_bi_b_radps.z),
    });
}

ConvergenceEvidence convergenceEvidence(
    const State& opening, double mass_kg, const IntervalExecution& interval,
    bool require_fourth_order) {
    ConvergenceEvidence result;
    result.substep_counts = {1, 2, 4, 8};
    for (std::int64_t substeps : result.substep_counts) {
        result.terminal_states.push_back(integrateSubsteps(
            opening, mass_kg, interval.closure, interval.environment,
            substeps));
    }
    for (std::size_t index = 0;
         index + 1 < result.terminal_states.size(); ++index) {
        result.successive_max_abs_differences.push_back(
            stateMaxDifference(result.terminal_states[index].terminal_state,
                               result.terminal_states[index + 1]
                                   .terminal_state));
    }
    for (std::size_t index = 0;
         index + 1 < result.successive_max_abs_differences.size(); ++index) {
        const double denominator =
            result.successive_max_abs_differences[index + 1];
        if (denominator > 0.0) {
            result.error_reduction_ratios.push_back(
                result.successive_max_abs_differences[index] / denominator);
        } else {
            requireDomain(!require_fourth_order,
                          "convergence difference must be positive");
            result.error_reduction_ratios.push_back(0.0);
        }
    }
    result.minimum_required_error_reduction_ratio = 12.0;
    if (require_fourth_order) {
        requireDomain(std::all_of(
                          result.error_reduction_ratios.begin(),
                          result.error_reduction_ratios.end(),
                          [&](double value) {
                              return value >= result
                                  .minimum_required_error_reduction_ratio;
                          }),
                      "second interval did not demonstrate fourth-order "
                      "convergence");
    }
    return result;
}

double formulaZero(double value) {
    return std::abs(value) <= kAbsoluteTolerance ? 0.0 : value;
}

Vec3 formulaZero(const Vec3& value) {
    return {formulaZero(value.x), formulaZero(value.y),
            formulaZero(value.z)};
}

IntervalExecution executeInterval(
    std::int64_t sample_tick, const State& opening_state,
    double opening_mass_kg, double closing_mass_kg,
    const IntervalExecution* stale_source = nullptr) {
    const Environment environment{
        {0.0, 0.0, -9.80665},
        {10.0, 0.0, 0.0},
        1.225,
        340.0,
    };
    requireDomain(finite(opening_state.position_i_m) &&
                      finite(opening_state.velocity_i_mps) &&
                      finite(environment.gravity_i_mps2) &&
                      finite(environment.velocity_airmass_i_mps),
                  "mission state or environment is non-finite");
    const Vec3 velocity_relative_i = subtract(
        opening_state.velocity_i_mps, environment.velocity_airmass_i_mps);
    const Vec3 velocity_relative_b = inertialToBody(
        opening_state.q_i_b, velocity_relative_i);
    const double airspeed = std::sqrt(dot(velocity_relative_b,
                                          velocity_relative_b));
    const double horizontal = std::sqrt(
        velocity_relative_b.x * velocity_relative_b.x +
        velocity_relative_b.z * velocity_relative_b.z);
    AirData air_data{
        velocity_relative_i,
        velocity_relative_b,
        airspeed,
        std::atan2(velocity_relative_b.z, velocity_relative_b.x),
        std::atan2(velocity_relative_b.y, horizontal),
        0.5 * environment.density_kgpm3 * airspeed * airspeed,
        airspeed / environment.speed_of_sound_mps,
    };
    AeroLookup lookup = lookupAero(
        air_data.mach, air_data.alpha_rad, air_data.beta_rad);
    const double pressure_area = air_data.dynamic_pressure_pa;
    const Vec3 aero_force{
        -pressure_area * lookup.coefficients[0],
        pressure_area * lookup.coefficients[1],
        -pressure_area * lookup.coefficients[2],
    };
    const Vec3 aero_application_moment{
        pressure_area * lookup.coefficients[3],
        pressure_area * lookup.coefficients[4],
        pressure_area * lookup.coefficients[5],
    };
    const Vec3 aero_lever{0.0, 0.0, -25.0 / 18.0};
    const Vec3 aero_moment = add(
        aero_application_moment, cross(aero_lever, aero_force));
    const Vec3 propulsion_force{100.0, 0.0, 0.0};
    const Vec3 propulsion_lever{0.0, 0.2, 0.0};
    const Vec3 propulsion_moment = add(
        {0.0, 0.0, 20.0}, cross(propulsion_lever, propulsion_force));
    Closure closure{
        formulaZero(add(aero_force, propulsion_force)),
        formulaZero(add(aero_moment, propulsion_moment)),
    };
    if (stale_source != nullptr) {
        air_data = stale_source->air_data;
        lookup = stale_source->aero_lookup;
        closure = stale_source->closure;
    }
    const RigidDerivative derivative = evaluateRigidDerivative(
        opening_state, opening_mass_kg, closure, environment);
    const double candidate_mass_kg = opening_mass_kg - 0.05;
    requireDomain(candidate_mass_kg > 0.0,
                  "pending mass candidate must be positive");
    const MassTransition mass{
        kMassStateId,
        opening_mass_kg,
        0.05,
        candidate_mass_kg,
        "candidate-only",
        closing_mass_kg,
        "atomic-rigid-and-mass",
    };
    return {
        sample_tick,
        sample_tick,
        sample_tick + 1,
        kConfigurationRevision,
        kDt,
        "FrozenInterval",
        1,
        environment,
        air_data,
        lookup,
        closure,
        derivative,
        mass,
    };
}

CommittedSample openingSample(double mass_kg) {
    return {
        0,
        0.0,
        "commit.fixture.yyz.mission.0",
        "Valid",
        {{0.0, 0.0, 1000.0},
         {110.0, 0.0, 0.0},
         {1.0, 0.0, 0.0, 0.0},
         {0.0, 0.0, 0.0}},
        mass_kg,
    };
}

CommittedSample committedSample(std::int64_t tick,
                                const std::string& commit_id,
                                const State& state, double mass_kg) {
    return {tick, static_cast<double>(tick) * kDt, commit_id, "Valid",
            state, mass_kg};
}

Metrics metricsFor(const CommittedSample& sample,
                   const State& initial_state) {
    const double speed = std::sqrt(dot(sample.state.velocity_i_mps,
                                       sample.state.velocity_i_mps));
    return {
        sample.time_s,
        sample.state.position_i_m.x - initial_state.position_i_m.x,
        sample.state.position_i_m.z - initial_state.position_i_m.z,
        sample.committed_mass_kg,
        100.0 - sample.committed_mass_kg,
        speed,
    };
}

double metricValue(const Metrics& metrics, const std::string& metric_id) {
    if (metric_id == "duration_s") {
        return metrics.duration_s;
    }
    if (metric_id == "downrange_m") {
        return metrics.downrange_m;
    }
    if (metric_id == "remaining_mass_kg") {
        return metrics.remaining_mass_kg;
    }
    throw std::domain_error("unsupported predicate metric");
}

std::pair<std::vector<PredicateResult>, Decision> evaluatePredicates(
    const Input& input, const Metrics& metrics, bool low_priority_wins) {
    std::vector<PredicateResult> results;
    for (const Predicate& predicate : input.predicates) {
        const double observed = metricValue(metrics, predicate.metric_id);
        const bool met = predicate.relation == ">="
            ? observed >= predicate.threshold
            : observed <= predicate.threshold;
        results.push_back({predicate, observed, met});
    }
    std::sort(results.begin(), results.end(),
              [](const PredicateResult& lhs, const PredicateResult& rhs) {
                  return lhs.predicate.predicate_id <
                         rhs.predicate.predicate_id;
              });
    const PredicateResult* selected = nullptr;
    for (const PredicateResult& result : results) {
        if (!result.met) {
            continue;
        }
        if (selected == nullptr) {
            selected = &result;
            continue;
        }
        bool better = false;
        if (result.predicate.priority != selected->predicate.priority) {
            better = low_priority_wins
                ? result.predicate.priority < selected->predicate.priority
                : result.predicate.priority > selected->predicate.priority;
        } else {
            better = result.predicate.predicate_id <
                     selected->predicate.predicate_id;
        }
        if (better) {
            selected = &result;
        }
    }
    if (selected == nullptr) {
        return {
            results,
            {"Continue", "none", metrics.duration_s, kSubject, 0,
             metrics, "yyz.termination.continue"},
        };
    }
    return {
        results,
        {selected->predicate.action,
         selected->predicate.reason_code,
         metrics.duration_s,
         kSubject,
         selected->predicate.priority,
         metrics,
         "yyz.termination." + selected->predicate.reason_code},
    };
}

MetricSummary summarize(const std::vector<EvaluationBoundary>& trace) {
    requireDomain(!trace.empty(), "evaluation trace must not be empty");
    const EvaluationBoundary& terminal = trace.back();
    const EvaluationBoundary* peak = &trace.front();
    const EvaluationBoundary* maximum_downrange = &trace.front();
    const EvaluationBoundary* minimum_mass = &trace.front();
    for (const EvaluationBoundary& boundary : trace) {
        if (boundary.metrics.speed_mps > peak->metrics.speed_mps) {
            peak = &boundary;
        }
        if (boundary.metrics.downrange_m >
            maximum_downrange->metrics.downrange_m) {
            maximum_downrange = &boundary;
        }
        if (boundary.metrics.remaining_mass_kg <
            minimum_mass->metrics.remaining_mass_kg) {
            minimum_mass = &boundary;
        }
    }
    return {
        static_cast<std::int64_t>(trace.size()),
        terminal.metrics.duration_s,
        terminal.metrics.downrange_m,
        terminal.metrics.vertical_displacement_m,
        terminal.metrics.remaining_mass_kg,
        terminal.metrics.consumed_mass_kg,
        terminal.metrics.speed_mps,
        peak->metrics.speed_mps,
        peak->sample_tick,
        maximum_downrange->metrics.downrange_m,
        maximum_downrange->sample_tick,
        minimum_mass->metrics.remaining_mass_kg,
        minimum_mass->sample_tick,
    };
}

Composition compose(const Input& input, const Options& options = {}) {
    validateInput(input);
    std::vector<Binding> resolved = input.bindings;
    std::sort(resolved.begin(), resolved.end(),
              [](const Binding& lhs, const Binding& rhs) {
                  return lhs.role < rhs.role;
              });
    const double opening_mass = options.early_mass_visibility
        ? 99.95 : 100.0;
    const double intermediate_mass = options.nonatomic_mass_commit
        ? 100.0 : 99.95;
    const CommittedSample opening = openingSample(opening_mass);
    const IntervalExecution first_interval = executeInterval(
        0, opening.state, 100.0, intermediate_mass);
    const State intermediate_state = integrateSubsteps(
        opening.state, 100.0, first_interval.closure,
        first_interval.environment, 1).terminal_state;
    const CommittedSample intermediate = committedSample(
        1, "commit.fixture.yyz.mission.1", intermediate_state,
        intermediate_mass);
    const double closing_mass = intermediate_mass - 0.05;
    const IntervalExecution second_interval = executeInterval(
        1, intermediate.state, intermediate_mass, closing_mass,
        options.stale_boundary_closure ? &first_interval : nullptr);
    const ConvergenceEvidence convergence = convergenceEvidence(
        intermediate.state, intermediate_mass, second_interval,
        !options.stale_boundary_closure);
    const CommittedSample closing = committedSample(
        2, "commit.fixture.yyz.mission.2",
        convergence.terminal_states.front().terminal_state, closing_mass);
    const std::vector<IntervalExecution> intervals{
        first_interval, second_interval};
    std::vector<CommittedSample> committed{opening, intermediate, closing};
    std::vector<EvaluationBoundary> trace;
    bool terminal_found = false;
    for (const CommittedSample& sample : committed) {
        const Metrics metrics = metricsFor(sample, committed.front().state);
        auto evaluated = evaluatePredicates(
            input, metrics, options.low_priority_wins);
        EvaluationBoundary boundary{
            sample.sample_tick,
            sample.time_s,
            sample.commit_id,
            metrics,
            std::move(evaluated.first),
            std::move(evaluated.second),
        };
        trace.push_back(boundary);
        if (boundary.decision.action != "Continue") {
            terminal_found = true;
            break;
        }
    }
    requireDomain(terminal_found,
                  "mission composition did not reach a terminal boundary");
    requireDomain(trace.back().sample_tick == input.terminal_tick,
                  "mission composition terminated on the wrong boundary");
    const MetricSummary summary = summarize(trace);
    std::vector<std::string> order = eventOrder();
    if (options.result_before_observation) {
        std::swap(order[order.size() - 2], order.back());
    }
    const EvaluationBoundary& terminal = trace.back();
    const TerminalObservation observation{
        closing.sample_tick,
        closing.time_s,
        closing.commit_id,
        "Valid",
        closing.state,
        closing.committed_mass_kg,
        terminal.metrics,
        terminal.decision,
        order,
        !options.result_before_observation,
    };
    const std::string final_status =
        terminal.decision.action == "Complete" ? "Completed" : "Terminated";
    const MissionResult mission_result{
        final_status,
        "Valid",
        input.initial_tick,
        terminal.sample_tick,
        terminal.time_s,
        terminal.decision,
        summary,
        observation.commit_id,
        observation.sealed,
        true,
    };
    return {
        "CASE-YYZ-MISSION-COMPOSITION-BASELINE",
        input.mission_source_id,
        input.execution_id,
        input.composition_model_id,
        resolved,
        committed,
        intervals,
        convergence,
        trace,
        summary,
        observation,
        mission_result,
    };
}

template <typename Mutation>
void expectDomainRejection(std::vector<std::string>& rejected,
                           const std::string& identifier,
                           const Input& accepted, Mutation mutation) {
    Input value = accepted;
    mutation(value);
    try {
        static_cast<void>(compose(value));
    } catch (const std::domain_error&) {
        rejected.push_back(identifier);
        return;
    }
    throw std::runtime_error("invalid input was accepted: " + identifier);
}

Binding& bindingByRole(Input& input, const std::string& role) {
    const auto found = std::find_if(
        input.bindings.begin(), input.bindings.end(),
        [&](const Binding& value) { return value.role == role; });
    require(found != input.bindings.end(), "mutation binding is missing");
    return *found;
}

bool equivalentCore(const Composition& lhs, const Composition& rhs) {
    return lhs.resolved_components.size() == rhs.resolved_components.size() &&
           lhs.committed_samples.size() == rhs.committed_samples.size() &&
           lhs.mission_result.final_status == rhs.mission_result.final_status &&
           lhs.mission_result.final_tick == rhs.mission_result.final_tick &&
           lhs.mission_result.termination.reason_code ==
               rhs.mission_result.termination.reason_code &&
           near(lhs.metric_summary.downrange_m,
                rhs.metric_summary.downrange_m) &&
           near(lhs.committed_samples.back().committed_mass_kg,
                rhs.committed_samples.back().committed_mass_kg);
}

ProbeResult runProbe() {
    const Input input = acceptedInput();
    ProbeResult result;
    result.accepted = compose(input);
    require(result.accepted.resolved_components.size() == 12 &&
                result.accepted.committed_samples.size() == 3 &&
                result.accepted.interval_executions.size() == 2 &&
                near(result.accepted.committed_samples[0]
                          .committed_mass_kg, 100.0) &&
                near(result.accepted.committed_samples[1]
                          .committed_mass_kg, 99.95) &&
                near(result.accepted.committed_samples[2]
                         .committed_mass_kg, 99.9) &&
                result.accepted.mission_result.final_tick == 2 &&
                result.accepted.mission_result.final_status == "Completed" &&
                result.accepted.mission_result.termination.reason_code ==
                    "downrange-goal" &&
                result.accepted.terminal_observation.sealed,
            "accepted mission composition differs");
    require(near(result.accepted.interval_executions[0].air_data.mach,
                  100.0 / 340.0) &&
                near(result.accepted.interval_executions[0].aero_lookup
                          .coefficients[0], 27.0 / 850.0) &&
                near(result.accepted.interval_executions[0].closure
                         .force_total_b_n.x, -3215.0 / 34.0) &&
                near(result.accepted.interval_executions[1].closure
                         .moment_total_about_com_b_nm.y,
                     16.766216427351054) &&
                std::all_of(
                    result.accepted.second_interval_convergence
                        .error_reduction_ratios.begin(),
                    result.accepted.second_interval_convergence
                        .error_reduction_ratios.end(),
                    [](double value) { return value >= 12.0; }),
            "lookup-composed interval facts differ");

    Input reversed = input;
    std::reverse(reversed.bindings.begin(), reversed.bindings.end());
    require(equivalentCore(result.accepted, compose(reversed)),
            "component declaration order changed mission composition");

    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-DUPLICATE-ROLE", input,
        [](Input& value) { value.bindings.push_back(value.bindings.front()); });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-MISSING-ROLE", input,
        [](Input& value) {
            value.bindings.erase(
                std::remove_if(
                    value.bindings.begin(), value.bindings.end(),
                    [](const Binding& binding) {
                        return binding.role == "frozen_interval";
                    }),
                value.bindings.end());
        });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-FIXTURE-ID", input,
        [](Input& value) {
            bindingByRole(value, "aero_lookup").fixture_id =
                "REF-YYZ-OTHER-001";
        });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-ORACLE-ID", input,
        [](Input& value) {
            bindingByRole(value, "propulsion").oracle_id =
                "ORACLE-YYZ-OTHER-001";
        });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-MODEL-ID", input,
        [](Input& value) {
            bindingByRole(value, "rigid_body").model_id =
                "MODEL-YYZ-OTHER-001";
        });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-CLOCK", input,
        [](Input& value) {
            value.clock_domain = "clock.fixture.yyz.other@1";
        });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-REVISION", input,
        [](Input& value) { value.configuration_revision = 12; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-DT", input,
        [](Input& value) { value.dt_s = 0.2; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-RELATION", input,
        [](Input& value) { value.predicates[0].relation = "<"; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-MISSION-COMPOSITION-NO-TERMINAL", input,
        [](Input& value) {
            for (Predicate& predicate : value.predicates) {
                predicate.metric_id = "duration_s";
                predicate.relation = ">=";
                predicate.threshold = 10.0;
            }
        });

    Options early;
    early.early_mass_visibility = true;
    result.early_mass = compose(input, early);
    Options nonatomic;
    nonatomic.nonatomic_mass_commit = true;
    result.nonatomic_mass = compose(input, nonatomic);
    Options stale;
    stale.stale_boundary_closure = true;
    result.stale_boundary_closure = compose(input, stale);
    Options low;
    low.low_priority_wins = true;
    result.low_priority = compose(input, low);
    Options wrong_order;
    wrong_order.result_before_observation = true;
    result.result_before_observation = compose(input, wrong_order);
    require(near(result.early_mass.committed_samples[0]
                     .committed_mass_kg, 99.95) &&
                near(result.nonatomic_mass.committed_samples[1]
                      .committed_mass_kg, 100.0) &&
                near(result.nonatomic_mass.committed_samples[2]
                         .committed_mass_kg, 99.95) &&
                stateMaxDifference(
                    result.accepted.committed_samples[2].state,
                    result.stale_boundary_closure.committed_samples[2]
                        .state) > 0.08 &&
                result.low_priority.mission_result.termination.reason_code ==
                    "duration-complete" &&
                !result.result_before_observation.terminal_observation.sealed,
            "a mission-composition mutation matched the accepted result");
    return result;
}

void writeNumber(double value) {
    std::cout << canonicalZero(value);
}

void writeBoolean(bool value) {
    std::cout << (value ? "true" : "false");
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

void writeStringList(const std::vector<std::string>& values) {
    std::cout << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << '"' << values[index] << '"';
    }
    std::cout << ']';
}

void writeBindings(const std::vector<Binding>& values) {
    std::cout << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        const Binding& value = values[index];
        std::cout << "{\"role\":\"" << value.role
                  << "\",\"fixture_id\":\"" << value.fixture_id
                  << "\",\"oracle_id\":\"" << value.oracle_id
                  << "\",\"model_id\":\"" << value.model_id
                  << "\",\"status\":\"passed\"}";
    }
    std::cout << ']';
}

void writeExecutionTrace(const Composition& value) {
    const std::string& opening_id = value.committed_samples[0].commit_id;
    const std::string& intermediate_id = value.committed_samples[1].commit_id;
    const std::string& closing_id = value.committed_samples[2].commit_id;
    const std::string& opening_action =
        value.evaluation_trace[0].decision.action;
    const std::string& intermediate_action =
        value.evaluation_trace[1].decision.action;
    const std::string& terminal_action =
        value.evaluation_trace.back().decision.action;
    std::cout << "[{\"order\":0,\"event\":\"resolve-components\","
                 "\"sample_tick\":0,\"component_count\":"
              << value.resolved_components.size()
              << "},{\"order\":1,\"event\":\"publish-opening-commit\","
                 "\"sample_tick\":0,\"commit_id\":\""
              << opening_id
              << "\"},{\"order\":2,\"event\":\""
                 "evaluate-opening-boundary\",\"sample_tick\":0,"
                 "\"action\":\""
              << opening_action
              << "\"},{\"order\":3,\"event\":\""
                 "evaluate-interval-0\",\"sample_tick\":0,"
                 "\"valid_until_tick\":1},{\"order\":4,\"event\":\""
                 "stage-commit-1\",\"sample_tick\":0,"
                 "\"candidate_tick\":1},{\"order\":5,\"event\":\""
                 "commit-rigid-and-mass-1\",\"sample_tick\":1,"
                 "\"commit_id\":\""
              << intermediate_id
              << "\"},{\"order\":6,\"event\":\""
                 "evaluate-intermediate-boundary\",\"sample_tick\":1,"
                 "\"action\":\""
              << intermediate_action
              << "\"},{\"order\":7,\"event\":\""
                 "evaluate-interval-1\",\"sample_tick\":1,"
                 "\"valid_until_tick\":2},{\"order\":8,\"event\":\""
                 "stage-commit-2\",\"sample_tick\":1,"
                 "\"candidate_tick\":2},{\"order\":9,\"event\":\""
                 "commit-rigid-and-mass-2\",\"sample_tick\":2,"
                 "\"commit_id\":\""
              << closing_id
              << "\"},{\"order\":10,\"event\":\""
                 "evaluate-terminal-boundary\",\"sample_tick\":2,"
                 "\"action\":\""
              << terminal_action
              << "\"},{\"order\":11,\"event\":\""
                 "seal-terminal-observation\",\"sample_tick\":2,"
                 "\"commit_id\":\""
              << closing_id
              << "\"},{\"order\":12,\"event\":\""
                 "freeze-mission-result\",\"sample_tick\":2,"
                 "\"status\":\""
              << value.mission_result.final_status << "\"}]";
}

void writeCommittedSample(const CommittedSample& value) {
    std::cout << "{\"sample_tick\":" << value.sample_tick
              << ",\"time_s\":";
    writeNumber(value.time_s);
    std::cout << ",\"commit_id\":\"" << value.commit_id
              << "\",\"quality\":\"" << value.quality
              << "\",\"position_I_m\":";
    writeVec3(value.state.position_i_m);
    std::cout << ",\"velocity_I_mps\":";
    writeVec3(value.state.velocity_i_mps);
    std::cout << ",\"q_I_B_wxyz\":";
    writeQuaternion(value.state.q_i_b);
    std::cout << ",\"omega_BI_B_radps\":";
    writeVec3(value.state.omega_bi_b_radps);
    std::cout << ",\"committed_mass_kg\":";
    writeNumber(value.committed_mass_kg);
    std::cout << '}';
}

void writeState(const State& value) {
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

void writeEnvironment(const Environment& value) {
    std::cout << "{\"gravity_I_mps2\":";
    writeVec3(value.gravity_i_mps2);
    std::cout << ",\"velocity_airmass_I_mps\":";
    writeVec3(value.velocity_airmass_i_mps);
    std::cout << ",\"density_kgpm3\":";
    writeNumber(value.density_kgpm3);
    std::cout << ",\"speed_of_sound_mps\":";
    writeNumber(value.speed_of_sound_mps);
    std::cout << '}';
}

void writeAirData(const AirData& value) {
    std::cout << "{\"velocity_relative_I_mps\":";
    writeVec3(value.velocity_relative_i_mps);
    std::cout << ",\"velocity_relative_B_mps\":";
    writeVec3(value.velocity_relative_b_mps);
    std::cout << ",\"airspeed_mps\":";
    writeNumber(value.airspeed_mps);
    std::cout << ",\"alpha_rad\":";
    writeNumber(value.alpha_rad);
    std::cout << ",\"beta_rad\":";
    writeNumber(value.beta_rad);
    std::cout << ",\"dynamic_pressure_Pa\":";
    writeNumber(value.dynamic_pressure_pa);
    std::cout << ",\"mach\":";
    writeNumber(value.mach);
    std::cout << '}';
}

void writeAeroLookup(const AeroLookup& value) {
    std::cout << "{\"model_id\":\"" << value.model_id
              << "\",\"table_id\":\"" << value.table_id
              << "\",\"configuration_id\":\""
              << value.configuration_id
              << "\",\"domain_status\":\"" << value.domain_status
              << "\",\"cell_indices_M_alpha_beta\":["
              << value.cell_indices[0] << ',' << value.cell_indices[1]
              << ',' << value.cell_indices[2]
              << "],\"weights_M_alpha_beta\":[";
    for (std::size_t index = 0; index < value.weights.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeNumber(value.weights[index]);
    }
    std::cout << "],\"coefficients_CA_CY_CN_Cl_Cm_Cn\":[";
    for (std::size_t index = 0; index < value.coefficients.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeNumber(value.coefficients[index]);
    }
    std::cout << "]}";
}

void writeRigidDerivative(const RigidDerivative& value) {
    std::cout << "{\"force_total_I_N\":";
    writeVec3(value.force_total_i_n);
    std::cout << ",\"acceleration_I_mps2\":";
    writeVec3(value.acceleration_i_mps2);
    std::cout << ",\"angular_momentum_B_kgm2ps\":";
    writeVec3(value.angular_momentum_b_kgm2ps);
    std::cout << ",\"gyroscopic_moment_B_Nm\":";
    writeVec3(value.gyroscopic_moment_b_nm);
    std::cout << ",\"net_moment_B_Nm\":";
    writeVec3(value.net_moment_b_nm);
    std::cout << ",\"angular_acceleration_B_radps2\":";
    writeVec3(value.angular_acceleration_b_radps2);
    std::cout << ",\"q_derivative_I_B_per_s\":";
    writeQuaternion(value.q_derivative_i_b_per_s);
    std::cout << '}';
}

void writeInterval(const IntervalExecution& value) {
    std::cout << "{\"sample_tick\":" << value.sample_tick
              << ",\"valid_from_tick\":" << value.valid_from_tick
              << ",\"valid_until_tick\":" << value.valid_until_tick
              << ",\"configuration_revision\":"
              << value.configuration_revision << ",\"base_dt_s\":";
    writeNumber(value.dt_s);
    std::cout << ",\"strategy\":\"" << value.strategy
              << "\",\"integration_substeps\":"
              << value.integration_substeps
              << ",\"environment_sample\":";
    writeEnvironment(value.environment);
    std::cout << ",\"air_data\":";
    writeAirData(value.air_data);
    std::cout << ",\"aero_lookup\":";
    writeAeroLookup(value.aero_lookup);
    std::cout << ",\"closure\":{\"force_total_B_N\":";
    writeVec3(value.closure.force_total_b_n);
    std::cout << ",\"moment_total_about_CoM_B_Nm\":";
    writeVec3(value.closure.moment_total_about_com_b_nm);
    std::cout << "},\"rigid_derivative_at_opening\":";
    writeRigidDerivative(value.rigid_derivative);
    const MassTransition& mass = value.mass_transition;
    std::cout << ",\"mass_transition\":{\"mass_state_id\":\""
              << mass.mass_state_id
              << "\",\"opening_committed_mass_kg\":";
    writeNumber(mass.opening_committed_mass_kg);
    std::cout << ",\"consumed_mass_kg\":";
    writeNumber(mass.consumed_mass_kg);
    std::cout << ",\"pending_mass_candidate_kg\":";
    writeNumber(mass.pending_mass_candidate_kg);
    std::cout << ",\"pending_visibility_before_commit\":\""
              << mass.pending_visibility_before_commit
              << "\",\"closing_committed_mass_kg\":";
    writeNumber(mass.closing_committed_mass_kg);
    std::cout << ",\"closing_commit_kind\":\""
              << mass.closing_commit_kind << "\"}}";
}

void writeConvergence(const ConvergenceEvidence& value) {
    std::cout << "{\"substep_counts\":[";
    for (std::size_t index = 0; index < value.substep_counts.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << value.substep_counts[index];
    }
    std::cout << "],\"terminal_states\":[";
    for (std::size_t index = 0; index < value.terminal_states.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        const ConvergenceEntry& entry = value.terminal_states[index];
        std::cout << "{\"substeps\":" << entry.substeps
                  << ",\"terminal_state\":";
        writeState(entry.terminal_state);
        std::cout << ",\"maximum_pre_normalization_quaternion_norm_residual\":";
        writeNumber(
            entry.maximum_pre_normalization_quaternion_norm_residual);
        std::cout << '}';
    }
    std::cout << "],\"successive_max_abs_differences\":[";
    for (std::size_t index = 0;
         index < value.successive_max_abs_differences.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeNumber(value.successive_max_abs_differences[index]);
    }
    std::cout << "],\"error_reduction_ratios\":[";
    for (std::size_t index = 0;
         index < value.error_reduction_ratios.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeNumber(value.error_reduction_ratios[index]);
    }
    std::cout << "],\"minimum_required_error_reduction_ratio\":";
    writeNumber(value.minimum_required_error_reduction_ratio);
    std::cout << '}';
}

void writeMetrics(const Metrics& value) {
    std::cout << "{\"duration_s\":";
    writeNumber(value.duration_s);
    std::cout << ",\"downrange_m\":";
    writeNumber(value.downrange_m);
    std::cout << ",\"vertical_displacement_m\":";
    writeNumber(value.vertical_displacement_m);
    std::cout << ",\"remaining_mass_kg\":";
    writeNumber(value.remaining_mass_kg);
    std::cout << ",\"consumed_mass_kg\":";
    writeNumber(value.consumed_mass_kg);
    std::cout << ",\"speed_mps\":";
    writeNumber(value.speed_mps);
    std::cout << '}';
}

void writeDecisionMetrics(const Metrics& value) {
    std::cout << "{\"duration_s\":";
    writeNumber(value.duration_s);
    std::cout << ",\"downrange_m\":";
    writeNumber(value.downrange_m);
    std::cout << ",\"remaining_mass_kg\":";
    writeNumber(value.remaining_mass_kg);
    std::cout << '}';
}

void writePredicateResult(const PredicateResult& value) {
    const Predicate& predicate = value.predicate;
    std::cout << "{\"predicate_id\":\"" << predicate.predicate_id
              << "\",\"metric_id\":\"" << predicate.metric_id
              << "\",\"relation\":\"" << predicate.relation
              << "\",\"threshold\":";
    writeNumber(predicate.threshold);
    std::cout << ",\"observed\":";
    writeNumber(value.observed);
    std::cout << ",\"met\":";
    writeBoolean(value.met);
    std::cout << ",\"action\":\"" << predicate.action
              << "\",\"reason_code\":\"" << predicate.reason_code
              << "\",\"priority\":" << predicate.priority << '}';
}

void writeDecision(const Decision& value) {
    std::cout << "{\"action\":\"" << value.action
              << "\",\"reason_code\":\"" << value.reason_code
              << "\",\"trigger_time_s\":";
    writeNumber(value.trigger_time_s);
    std::cout << ",\"subject\":\"" << value.subject
              << "\",\"priority\":" << value.priority
              << ",\"metrics\":";
    writeDecisionMetrics(value.metrics);
    std::cout << ",\"message_key\":\"" << value.message_key
              << "\",\"params\":{}}";
}

void writeEvaluationBoundary(const EvaluationBoundary& value) {
    std::cout << "{\"sample_tick\":" << value.sample_tick
              << ",\"time_s\":";
    writeNumber(value.time_s);
    std::cout << ",\"commit_id\":\"" << value.commit_id
              << "\",\"metrics\":";
    writeMetrics(value.metrics);
    std::cout << ",\"predicate_results\":[";
    for (std::size_t index = 0; index < value.predicate_results.size();
         ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writePredicateResult(value.predicate_results[index]);
    }
    std::cout << "],\"decision\":";
    writeDecision(value.decision);
    std::cout << '}';
}

void writeMetricSummary(const MetricSummary& value) {
    std::cout << "{\"evaluated_sample_count\":"
              << value.evaluated_sample_count << ",\"duration_s\":";
    writeNumber(value.duration_s);
    std::cout << ",\"downrange_m\":";
    writeNumber(value.downrange_m);
    std::cout << ",\"vertical_displacement_m\":";
    writeNumber(value.vertical_displacement_m);
    std::cout << ",\"remaining_mass_kg\":";
    writeNumber(value.remaining_mass_kg);
    std::cout << ",\"consumed_mass_kg\":";
    writeNumber(value.consumed_mass_kg);
    std::cout << ",\"terminal_speed_mps\":";
    writeNumber(value.terminal_speed_mps);
    std::cout << ",\"peak_speed_mps\":";
    writeNumber(value.peak_speed_mps);
    std::cout << ",\"peak_speed_tick\":" << value.peak_speed_tick
              << ",\"maximum_downrange_m\":";
    writeNumber(value.maximum_downrange_m);
    std::cout << ",\"maximum_downrange_tick\":"
              << value.maximum_downrange_tick
              << ",\"minimum_remaining_mass_kg\":";
    writeNumber(value.minimum_remaining_mass_kg);
    std::cout << ",\"minimum_remaining_mass_tick\":"
              << value.minimum_remaining_mass_tick << '}';
}

void writeTerminalObservation(const TerminalObservation& value) {
    std::cout << "{\"sample_tick\":" << value.sample_tick
              << ",\"time_s\":";
    writeNumber(value.time_s);
    std::cout << ",\"commit_id\":\"" << value.commit_id
              << "\",\"quality\":\"" << value.quality
              << "\",\"fields\":{\"position_I_m\":";
    writeVec3(value.state.position_i_m);
    std::cout << ",\"velocity_I_mps\":";
    writeVec3(value.state.velocity_i_mps);
    std::cout << ",\"q_I_B_wxyz\":";
    writeQuaternion(value.state.q_i_b);
    std::cout << ",\"omega_BI_B_radps\":";
    writeVec3(value.state.omega_bi_b_radps);
    std::cout << ",\"committed_mass_kg\":";
    writeNumber(value.committed_mass_kg);
    std::cout << ",\"metrics\":";
    writeMetrics(value.metrics);
    std::cout << ",\"decision\":";
    writeDecision(value.decision);
    std::cout << "},\"event_order\":";
    writeStringList(value.event_order);
    std::cout << ",\"sealed\":";
    writeBoolean(value.sealed);
    std::cout << '}';
}

void writeMissionResult(const MissionResult& value) {
    std::cout << "{\"final_status\":\"" << value.final_status
              << "\",\"evidence_validity\":\""
              << value.evidence_validity << "\",\"initial_tick\":"
              << value.initial_tick << ",\"final_tick\":"
              << value.final_tick << ",\"final_time_s\":";
    writeNumber(value.final_time_s);
    std::cout << ",\"termination\":";
    writeDecision(value.termination);
    std::cout << ",\"metrics\":";
    writeMetricSummary(value.metrics);
    std::cout << ",\"terminal_observation_commit_id\":\""
              << value.terminal_observation_commit_id
              << "\",\"terminal_observation_sealed\":";
    writeBoolean(value.terminal_observation_sealed);
    std::cout << ",\"frozen\":";
    writeBoolean(value.frozen);
    std::cout << '}';
}

void writeComposition(const Composition& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"mission_source_id\":\""
              << value.mission_source_id << "\",\"execution_id\":\""
              << value.execution_id << "\",\"composition_model_id\":\""
              << value.composition_model_id
              << "\",\"resolved_components\":";
    writeBindings(value.resolved_components);
    std::cout << ",\"execution_trace\":";
    writeExecutionTrace(value);
    std::cout << ",\"committed_samples\":[";
    for (std::size_t index = 0; index < value.committed_samples.size();
         ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeCommittedSample(value.committed_samples[index]);
    }
    std::cout << "],\"interval_executions\":[";
    for (std::size_t index = 0; index < value.interval_executions.size();
         ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeInterval(value.interval_executions[index]);
    }
    std::cout << "],\"second_interval_convergence\":";
    writeConvergence(value.second_interval_convergence);
    std::cout << ",\"evaluation_trace\":[";
    for (std::size_t index = 0; index < value.evaluation_trace.size();
         ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeEvaluationBoundary(value.evaluation_trace[index]);
    }
    std::cout << "],\"metric_summary\":";
    writeMetricSummary(value.metric_summary);
    std::cout << ",\"terminal_observation\":";
    writeTerminalObservation(value.terminal_observation);
    std::cout << ",\"mission_result\":";
    writeMissionResult(value.mission_result);
    std::cout << '}';
}

void writeMutations(const ProbeResult& value) {
    std::cout << "[{\"id\":\"MUTATION-YYZ-MISSION-COMPOSITION-"
                 "EARLY-MASS-VISIBILITY\",\"status\":\"rejected\","
                 "\"expected_opening_committed_mass_kg\":";
    writeNumber(value.accepted.committed_samples[0].committed_mass_kg);
    std::cout << ",\"observed_opening_committed_mass_kg\":";
    writeNumber(value.early_mass.committed_samples[0].committed_mass_kg);
    std::cout << ",\"observed_opening_consumed_mass_kg\":";
    writeNumber(value.early_mass.evaluation_trace[0]
                    .metrics.consumed_mass_kg);
    std::cout << ",\"max_abs_result_difference\":0.05},"
                 "{\"id\":\"MUTATION-YYZ-MISSION-COMPOSITION-"
                 "NONATOMIC-MASS-COMMIT\",\"status\":\"rejected\","
                 "\"expected_intermediate_committed_mass_kg\":";
    writeNumber(value.accepted.committed_samples[1].committed_mass_kg);
    std::cout << ",\"observed_intermediate_committed_mass_kg\":";
    writeNumber(value.nonatomic_mass.committed_samples[1]
                    .committed_mass_kg);
    std::cout << ",\"expected_closing_committed_mass_kg\":";
    writeNumber(value.accepted.committed_samples[2].committed_mass_kg);
    std::cout << ",\"observed_closing_committed_mass_kg\":";
    writeNumber(value.nonatomic_mass.committed_samples[2]
                    .committed_mass_kg);
    std::cout << ",\"observed_terminal_consumed_mass_kg\":";
    writeNumber(value.nonatomic_mass.metric_summary.consumed_mass_kg);
    std::cout << ",\"max_abs_result_difference\":0.05},"
                 "{\"id\":\"MUTATION-YYZ-MISSION-COMPOSITION-"
                 "STALE-BOUNDARY-CLOSURE\",\"status\":\"rejected\","
                 "\"expected_force_total_B_N\":";
    writeVec3(value.accepted.interval_executions[1]
                  .closure.force_total_b_n);
    std::cout << ",\"observed_force_total_B_N\":";
    writeVec3(value.stale_boundary_closure.interval_executions[1]
                  .closure.force_total_b_n);
    std::cout << ",\"expected_terminal_position_I_m\":";
    writeVec3(value.accepted.committed_samples[2].state.position_i_m);
    std::cout << ",\"observed_terminal_position_I_m\":";
    writeVec3(value.stale_boundary_closure.committed_samples[2]
                  .state.position_i_m);
    std::cout << ",\"max_abs_result_difference\":";
    writeNumber(stateMaxDifference(
        value.accepted.committed_samples[2].state,
        value.stale_boundary_closure.committed_samples[2].state));
    std::cout << "},{\"id\":\"MUTATION-YYZ-MISSION-COMPOSITION-"
                 "LOW-PRIORITY-WINS\",\"status\":\"rejected\","
                 "\"expected_reason_code\":\""
              << value.accepted.mission_result.termination.reason_code
              << "\",\"observed_reason_code\":\""
              << value.low_priority.mission_result.termination.reason_code
              << "\",\"expected_priority\":"
              << value.accepted.mission_result.termination.priority
              << ",\"observed_priority\":"
              << value.low_priority.mission_result.termination.priority
              << ",\"max_abs_result_difference\":100},"
                 "{\"id\":\"MUTATION-YYZ-MISSION-COMPOSITION-"
                 "RESULT-BEFORE-OBSERVATION\",\"status\":\"rejected\","
                 "\"expected_event_order\":";
    writeStringList(eventOrder());
    std::cout << ",\"observed_event_order\":";
    writeStringList(value.result_before_observation.terminal_observation
                        .event_order);
    std::cout << ",\"observed_terminal_observation_sealed\":";
    writeBoolean(value.result_before_observation.mission_result
                     .terminal_observation_sealed);
    std::cout << ",\"max_abs_result_difference\":1}]";
}

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"model_id\":\"" << kModelId
              << "\",\"status\":\"passed\",\"cases\":[";
    writeComposition(result.accepted);
    std::cout << "],\"equivalence_results\":[{\"id\":\""
                 "EQUIV-YYZ-MISSION-COMPOSITION-BINDING-ORDER\","
                 "\"status\":\"passed\",\"resolved_role_count\":"
              << result.accepted.resolved_components.size()
              << ",\"terminal_tick\":"
              << result.accepted.mission_result.final_tick
              << ",\"reason_code\":\""
              << result.accepted.mission_result.termination.reason_code
              << "\",\"max_abs_result_difference\":0}],"
                 "\"invalid_input_rejections\":";
    writeStringList(result.invalid_input_rejections);
    std::cout << ",\"mutation_results\":";
    writeMutations(result);
    std::cout << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr <<
            "usage: gnc_yyz_mission_composition_probe --self-check\n";
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

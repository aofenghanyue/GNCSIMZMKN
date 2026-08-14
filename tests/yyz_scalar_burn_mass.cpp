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

constexpr const char* kOracleId = "ORACLE-YYZ-SCALAR-BURN-MASS-001";
constexpr const char* kModelId =
    "MODEL-YYZ-SCALAR-BURN-CONSTANT-GEOMETRY-001";
constexpr const char* kModelChoiceStatus = "accepted";
constexpr const char* kMassStateId = "mass.fixture.yyz.vehicle@1";
constexpr const char* kBodyFrameId = "frame.fixture.yyz.body@1";
constexpr const char* kBodyOriginPointId =
    "point.fixture.yyz.body-origin@1";
constexpr const char* kComPointId =
    "point.fixture.yyz.center-of-mass@1";
constexpr const char* kClockDomain = "clock.fixture.yyz.simulation@1";
constexpr const char* kQuality = "Valid";
constexpr double kFormulaAbsolute = 2.0e-12;
constexpr double kFormulaRelative = 2.0e-12;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Matrix3 {
    std::array<std::array<double, 3>, 3> values{};
};

struct Context {
    std::string mass_state_id;
    std::string body_frame_id;
    std::string clock_domain;
    std::int64_t sample_tick = 0;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    double base_dt_s = 0.0;
    std::int64_t next_valid_until_tick = 0;
};

struct MassState {
    double mass_kg = 0.0;
    Vec3 r_body_origin_to_com_b_m;
    Matrix3 inertia_about_com_b_kgm2;
};

struct MassFlowInterval {
    std::string source_id;
    std::string mass_state_id;
    std::string body_frame_id;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    double fuel_consumption_rate_kgps = 0.0;
};

struct ConsumerProbe {
    std::string application_point_id;
    Vec3 r_body_origin_to_application_b_m;
    Vec3 force_b_n;
    Vec3 intrinsic_moment_at_application_b_nm;
    Vec3 omega_bi_b_radps;
};

struct Input {
    std::string id;
    Context context;
    MassState committed;
    MassFlowInterval flow;
    ConsumerProbe consumer;
};

enum class MassSign { Subtract, Add };
enum class Visibility { CandidateOnly, CommittedEarly };
enum class ComMode { Constant, Drift };
enum class InertiaMode { Constant, MassRatio };

struct Options {
    MassSign mass_sign = MassSign::Subtract;
    Visibility visibility = Visibility::CandidateOnly;
    ComMode com_mode = ComMode::Constant;
    InertiaMode inertia_mode = InertiaMode::Constant;
};

struct IdentityResult {
    std::string model_id;
    std::string mass_state_id;
    std::string body_frame_id;
    std::string body_origin_point_id;
    std::string center_of_mass_point_id;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
};

struct SampleResult {
    std::string quality;
    std::int64_t sample_tick = 0;
    double mass_kg = 0.0;
    Vec3 r_body_origin_to_com_b_m;
    Matrix3 inertia_about_com_b_kgm2;
    std::string visibility;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    bool has_validity = false;
};

struct CandidateResult {
    std::string source_id;
    double interval_duration_s = 0.0;
    double fuel_consumption_rate_kgps = 0.0;
    double consumed_mass_kg = 0.0;
    double mass_delta_kg = 0.0;
    double mass_kg = 0.0;
    Vec3 r_body_origin_to_com_b_m;
    Matrix3 inertia_about_com_b_kgm2;
    std::string visibility_before_commit;
    std::int64_t next_commit_tick = 0;
};

struct ConsumerResult {
    std::string application_point_id;
    double mass_kg = 0.0;
    Vec3 r_com_to_application_b_m;
    Vec3 force_b_n;
    Vec3 moment_about_com_b_nm;
    Vec3 specific_force_b_mps2;
    Matrix3 inertia_about_com_b_kgm2;
    Vec3 angular_momentum_b_kgm2ps;
    Vec3 gyroscopic_moment_b_nm;
    Vec3 angular_acceleration_b_radps2;
};

struct Evaluation {
    std::string id;
    IdentityResult identity;
    SampleResult current_sample;
    CandidateResult candidate;
    ConsumerResult interval_consumer;
    SampleResult next_sample;
    ConsumerResult next_consumer;
};

struct PartitionResult {
    std::string id;
    std::string status;
    double first_interval_duration_s = 0.0;
    double first_consumed_mass_kg = 0.0;
    double first_committed_mass_kg = 0.0;
    double second_interval_duration_s = 0.0;
    double second_consumed_mass_kg = 0.0;
    double summed_consumed_mass_kg = 0.0;
    double partitioned_final_mass_kg = 0.0;
    double unsplit_final_mass_kg = 0.0;
    double max_abs_geometry_difference = 0.0;
};

enum class MutationKind { MassGain, EarlyVisibility, ComDrift, InertiaScale };

struct MutationResult {
    std::string id;
    std::string status;
    MutationKind kind = MutationKind::MassGain;
    Evaluation observed;
    double max_abs_physical_difference = 0.0;
};

struct ProbeResult {
    std::vector<Evaluation> cases;
    PartitionResult partition;
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

bool finite(const Matrix3& value) {
    for (const auto& row : value.values) {
        for (double entry : row) {
            if (!finite(entry)) {
                return false;
            }
        }
    }
    return true;
}

double canonicalZero(double value) {
    return value == 0.0 ? 0.0 : value;
}

Vec3 canonicalZero(const Vec3& value) {
    return {canonicalZero(value.x), canonicalZero(value.y),
            canonicalZero(value.z)};
}

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero(
        {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z});
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero(
        {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z});
}

Vec3 scale(const Vec3& value, double factor) {
    return canonicalZero(
        {value.x * factor, value.y * factor, value.z * factor});
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero({
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    });
}

Vec3 multiply(const Matrix3& matrix, const Vec3& value) {
    return canonicalZero({
        matrix.values[0][0] * value.x +
            matrix.values[0][1] * value.y +
            matrix.values[0][2] * value.z,
        matrix.values[1][0] * value.x +
            matrix.values[1][1] * value.y +
            matrix.values[1][2] * value.z,
        matrix.values[2][0] * value.x +
            matrix.values[2][1] * value.y +
            matrix.values[2][2] * value.z,
    });
}

Matrix3 scale(const Matrix3& matrix, double factor) {
    Matrix3 result;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            result.values[row][column] = canonicalZero(
                matrix.values[row][column] * factor);
        }
    }
    return result;
}

std::array<std::array<double, 3>, 3> cholesky(const Matrix3& inertia) {
    requireDomain(finite(inertia), "MassState inertia is non-finite");
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            requireDomain(inertia.values[row][column] ==
                              inertia.values[column][row],
                          "MassState inertia must be symmetric");
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
                              "MassState inertia must be positive definite");
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
    const double bound = kFormulaAbsolute + kFormulaRelative *
        std::max({1.0, std::abs(actual), std::abs(expected)});
    return difference <= bound;
}

double maxDifference(const Vec3& lhs, const Vec3& rhs) {
    return std::max({std::abs(lhs.x - rhs.x), std::abs(lhs.y - rhs.y),
                     std::abs(lhs.z - rhs.z)});
}

double maxDifference(const Matrix3& lhs, const Matrix3& rhs) {
    double maximum = 0.0;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            maximum = std::max(maximum, std::abs(
                lhs.values[row][column] - rhs.values[row][column]));
        }
    }
    return maximum;
}

void validateInput(const Input& input) {
    const Context& context = input.context;
    requireDomain(context.mass_state_id == kMassStateId &&
                      context.body_frame_id == kBodyFrameId &&
                      context.clock_domain == kClockDomain,
                  "scalar-burn context identity differs");
    requireDomain(context.sample_tick >= 0 &&
                      context.configuration_revision >= 0 &&
                      context.valid_from_tick >= 0 &&
                      context.valid_until_tick >= 0 &&
                      context.next_valid_until_tick >= 0,
                  "scalar-burn tick or revision is invalid");
    requireDomain(context.sample_tick == context.valid_from_tick &&
                      context.valid_until_tick > context.valid_from_tick &&
                      context.next_valid_until_tick > context.valid_until_tick,
                  "scalar-burn interval identity is invalid");
    requireDomain(finite(context.base_dt_s) && context.base_dt_s > 0.0,
                  "base dt must be positive and finite");
    requireDomain(finite(input.committed.mass_kg) &&
                      input.committed.mass_kg > 0.0 &&
                      finite(input.committed.r_body_origin_to_com_b_m),
                  "committed MassState is outside its domain");
    static_cast<void>(cholesky(input.committed.inertia_about_com_b_kgm2));
    requireDomain(!input.flow.source_id.empty(),
                  "MassFlowInterval source identity is empty");
    requireDomain(input.flow.mass_state_id == context.mass_state_id,
                  "MassFlowInterval mass state identity differs");
    requireDomain(input.flow.body_frame_id == context.body_frame_id,
                  "MassFlowInterval body frame differs");
    requireDomain(input.flow.clock_domain == context.clock_domain,
                  "MassFlowInterval clock domain differs");
    requireDomain(input.flow.configuration_revision ==
                      context.configuration_revision,
                  "MassFlowInterval revision differs");
    requireDomain(input.flow.valid_from_tick == context.valid_from_tick &&
                      input.flow.valid_until_tick == context.valid_until_tick,
                  "MassFlowInterval validity differs");
    requireDomain(finite(input.flow.fuel_consumption_rate_kgps) &&
                      input.flow.fuel_consumption_rate_kgps >= 0.0,
                  "fuel consumption rate must be nonnegative and finite");
    const double duration = static_cast<double>(
        context.valid_until_tick - context.valid_from_tick) *
        context.base_dt_s;
    requireDomain(input.committed.mass_kg -
                      input.flow.fuel_consumption_rate_kgps * duration > 0.0,
                  "scalar-burn candidate mass must be positive");
    requireDomain(!input.consumer.application_point_id.empty() &&
                      finite(input.consumer.r_body_origin_to_application_b_m) &&
                      finite(input.consumer.force_b_n) &&
                      finite(input.consumer.intrinsic_moment_at_application_b_nm) &&
                      finite(input.consumer.omega_bi_b_radps),
                  "consumer probe is outside its domain");
}

Matrix3 matrix(double i00, double i01, double i02,
               double i10, double i11, double i12,
               double i20, double i21, double i22) {
    Matrix3 result;
    result.values = std::array<std::array<double, 3>, 3>{{
        {{i00, i01, i02}},
        {{i10, i11, i12}},
        {{i20, i21, i22}},
    }};
    return result;
}

std::vector<Input> acceptedInputs() {
    Input burn;
    burn.id = "CASE-YYZ-SCALAR-BURN-MASS-FULL-INERTIA";
    burn.context = {kMassStateId, kBodyFrameId, kClockDomain,
                    20, 8, 20, 25, 0.1, 30};
    burn.committed = {
        120.0,
        {0.2, -0.1, 0.05},
        matrix(12.0, 1.0, 0.5,
               1.0, 20.0, 2.0,
               0.5, 2.0, 30.0),
    };
    burn.flow = {"propulsion.main", kMassStateId, kBodyFrameId,
                 kClockDomain, 8, 20, 25, 0.5};
    burn.consumer = {
        "point.fixture.yyz.propulsion-main@1",
        {1.0, 0.4, -0.2},
        {300.0, 400.0, 0.0},
        {1.0, -2.0, 3.0},
        {1.0, 2.0, 3.0},
    };

    Input zero_flow;
    zero_flow.id = "CASE-YYZ-SCALAR-BURN-MASS-ZERO-FLOW";
    zero_flow.context = {kMassStateId, kBodyFrameId, kClockDomain,
                         3, 9, 3, 7, 0.25, 8};
    zero_flow.committed = {
        10.0,
        {-0.5, 0.25, 0.1},
        matrix(4.0, 0.0, 0.0,
               0.0, 5.0, 0.0,
               0.0, 0.0, 6.0),
    };
    zero_flow.flow = {"propulsion.main", kMassStateId, kBodyFrameId,
                      kClockDomain, 9, 3, 7, 0.0};
    zero_flow.consumer = {
        "point.fixture.yyz.propulsion-main@1",
        {0.2, -0.1, 0.3},
        {40.0, 0.0, 30.0},
        {-4.0, 5.0, -6.0},
        {0.2, -0.1, 0.3},
    };
    return {burn, zero_flow};
}

ConsumerResult consumerValues(double mass_kg, const Vec3& com,
                              const Matrix3& inertia,
                              const ConsumerProbe& probe) {
    const Vec3 lever = subtract(
        probe.r_body_origin_to_application_b_m, com);
    const Vec3 moment_com = add(
        probe.intrinsic_moment_at_application_b_nm,
        cross(lever, probe.force_b_n));
    const Vec3 angular_momentum = multiply(inertia, probe.omega_bi_b_radps);
    const Vec3 gyroscopic = cross(probe.omega_bi_b_radps,
                                  angular_momentum);
    const Vec3 net_moment = subtract(moment_com, gyroscopic);
    return {
        probe.application_point_id,
        mass_kg,
        lever,
        probe.force_b_n,
        moment_com,
        scale(probe.force_b_n, 1.0 / mass_kg),
        inertia,
        angular_momentum,
        gyroscopic,
        solveSpd(inertia, net_moment),
    };
}

Evaluation evaluate(const Input& input, const Options& options = {}) {
    validateInput(input);
    const double duration = static_cast<double>(
        input.context.valid_until_tick - input.context.valid_from_tick) *
        input.context.base_dt_s;
    const double consumed = input.flow.fuel_consumption_rate_kgps * duration;
    const double candidate_mass = options.mass_sign == MassSign::Subtract
        ? input.committed.mass_kg - consumed
        : input.committed.mass_kg + consumed;
    requireDomain(finite(candidate_mass) && candidate_mass > 0.0,
                  "mutated candidate mass is invalid");
    const Vec3 candidate_com = options.com_mode == ComMode::Constant
        ? input.committed.r_body_origin_to_com_b_m
        : add(input.committed.r_body_origin_to_com_b_m,
              {consumed, 0.0, 0.0});
    const Matrix3 candidate_inertia =
        options.inertia_mode == InertiaMode::Constant
            ? input.committed.inertia_about_com_b_kgm2
            : scale(input.committed.inertia_about_com_b_kgm2,
                    candidate_mass / input.committed.mass_kg);
    static_cast<void>(cholesky(candidate_inertia));
    const double interval_visible_mass =
        options.visibility == Visibility::CandidateOnly
            ? input.committed.mass_kg : candidate_mass;
    const std::string visibility =
        options.visibility == Visibility::CandidateOnly
            ? "candidate-only" : "committed-early";
    return {
        input.id,
        {kModelId, input.context.mass_state_id, input.context.body_frame_id,
         kBodyOriginPointId, kComPointId, input.context.clock_domain,
         input.context.configuration_revision,
         input.context.valid_from_tick, input.context.valid_until_tick},
        {kQuality, input.context.sample_tick, input.committed.mass_kg,
         input.committed.r_body_origin_to_com_b_m,
         input.committed.inertia_about_com_b_kgm2,
         "committed-through-interval", 0, 0, false},
        {input.flow.source_id, duration,
         input.flow.fuel_consumption_rate_kgps, consumed,
         candidate_mass - input.committed.mass_kg, candidate_mass,
         candidate_com, candidate_inertia, visibility,
         input.context.valid_until_tick},
        consumerValues(interval_visible_mass,
                       input.committed.r_body_origin_to_com_b_m,
                       input.committed.inertia_about_com_b_kgm2,
                       input.consumer),
        {kQuality, input.context.valid_until_tick, candidate_mass,
         candidate_com, candidate_inertia,
         "committed-at-next-boundary",
         input.context.valid_until_tick,
         input.context.next_valid_until_tick, true},
        consumerValues(candidate_mass, candidate_com, candidate_inertia,
                       input.consumer),
    };
}

void append(std::vector<double>& destination, const Vec3& value) {
    destination.push_back(value.x);
    destination.push_back(value.y);
    destination.push_back(value.z);
}

void append(std::vector<double>& destination, const Matrix3& value) {
    for (const auto& row : value.values) {
        destination.insert(destination.end(), row.begin(), row.end());
    }
}

void appendConsumer(std::vector<double>& result,
                    const ConsumerResult& value) {
    result.push_back(value.mass_kg);
    append(result, value.r_com_to_application_b_m);
    append(result, value.moment_about_com_b_nm);
    append(result, value.specific_force_b_mps2);
    append(result, value.angular_momentum_b_kgm2ps);
    append(result, value.gyroscopic_moment_b_nm);
    append(result, value.angular_acceleration_b_radps2);
}

std::vector<double> physicalVector(const Evaluation& value) {
    std::vector<double> result;
    result.push_back(value.current_sample.mass_kg);
    append(result, value.current_sample.r_body_origin_to_com_b_m);
    append(result, value.current_sample.inertia_about_com_b_kgm2);
    result.push_back(value.candidate.interval_duration_s);
    result.push_back(value.candidate.consumed_mass_kg);
    result.push_back(value.candidate.mass_delta_kg);
    result.push_back(value.candidate.mass_kg);
    append(result, value.candidate.r_body_origin_to_com_b_m);
    append(result, value.candidate.inertia_about_com_b_kgm2);
    appendConsumer(result, value.interval_consumer);
    result.push_back(value.next_sample.mass_kg);
    append(result, value.next_sample.r_body_origin_to_com_b_m);
    append(result, value.next_sample.inertia_about_com_b_kgm2);
    appendConsumer(result, value.next_consumer);
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

PartitionResult partitionResult(const Input& input,
                                const Evaluation& accepted) {
    const double first_duration = 2.0 * input.context.base_dt_s;
    const double second_duration = 3.0 * input.context.base_dt_s;
    const double first_consumed =
        input.flow.fuel_consumption_rate_kgps * first_duration;
    const double second_consumed =
        input.flow.fuel_consumption_rate_kgps * second_duration;
    const double first_candidate = input.committed.mass_kg - first_consumed;
    const double final_candidate = first_candidate - second_consumed;
    const double geometry_difference = std::max(
        maxDifference(input.committed.r_body_origin_to_com_b_m,
                      accepted.candidate.r_body_origin_to_com_b_m),
        maxDifference(input.committed.inertia_about_com_b_kgm2,
                      accepted.candidate.inertia_about_com_b_kgm2));
    require(near(first_consumed + second_consumed,
                 accepted.candidate.consumed_mass_kg) &&
                near(final_candidate, accepted.candidate.mass_kg) &&
                geometry_difference <= kFormulaAbsolute,
            "partition changed the scalar-burn final state");
    return {
        "EQUIV-YYZ-SCALAR-BURN-MASS-INTERVAL-PARTITION",
        "passed",
        first_duration,
        first_consumed,
        first_candidate,
        second_duration,
        second_consumed,
        first_consumed + second_consumed,
        final_candidate,
        accepted.candidate.mass_kg,
        geometry_difference,
    };
}

void expectDomainRejection(
    std::vector<std::string>& rejected, const Input& accepted,
    const std::string& id, const std::function<void(Input&)>& mutate) {
    Input input = accepted;
    mutate(input);
    try {
        static_cast<void>(evaluate(input));
    } catch (const std::domain_error&) {
        rejected.push_back(id);
        return;
    }
    throw std::runtime_error("invalid scalar-burn input survived: " + id);
}

ProbeResult runProbe() {
    const std::vector<Input> inputs = acceptedInputs();
    std::vector<Evaluation> cases;
    for (const Input& input : inputs) {
        cases.push_back(evaluate(input));
    }
    const Evaluation& accepted = cases[0];
    require(near(accepted.candidate.interval_duration_s, 0.5) &&
                near(accepted.candidate.consumed_mass_kg, 0.25) &&
                near(accepted.candidate.mass_kg, 119.75) &&
                maxDifference(accepted.current_sample.r_body_origin_to_com_b_m,
                              accepted.candidate.r_body_origin_to_com_b_m) <=
                    kFormulaAbsolute &&
                maxDifference(accepted.current_sample.inertia_about_com_b_kgm2,
                              accepted.candidate.inertia_about_com_b_kgm2) <=
                    kFormulaAbsolute &&
                near(accepted.interval_consumer.mass_kg, 120.0) &&
                near(accepted.next_consumer.mass_kg, 119.75) &&
                near(cases[1].candidate.consumed_mass_kg, 0.0) &&
                near(cases[1].candidate.mass_kg, 10.0),
            "accepted scalar-burn anchors differ");

    std::vector<std::string> invalid;
    const Input& first = inputs[0];
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-STATE-IDENTITY",
        [](Input& value) {
            value.flow.mass_state_id = "mass.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-FRAME-MISMATCH",
        [](Input& value) {
            value.flow.body_frame_id = "frame.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-CLOCK-MISMATCH",
        [](Input& value) {
            value.flow.clock_domain = "clock.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-REVISION-MISMATCH",
        [](Input& value) { value.flow.configuration_revision = 9; });
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-INTERVAL-MISMATCH",
        [](Input& value) { value.flow.valid_until_tick = 24; });
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-NONPOSITIVE-DT",
        [](Input& value) { value.context.base_dt_s = 0.0; });
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-NONPOSITIVE-CURRENT",
        [](Input& value) { value.committed.mass_kg = 0.0; });
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-NEGATIVE-CONSUMPTION",
        [](Input& value) { value.flow.fuel_consumption_rate_kgps = -0.5; });
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-DEPLETED-CANDIDATE",
        [](Input& value) { value.flow.fuel_consumption_rate_kgps = 240.0; });
    expectDomainRejection(
        invalid, first, "INVALID-YYZ-SCALAR-BURN-MASS-NON-SPD-INERTIA",
        [](Input& value) {
            value.committed.inertia_about_com_b_kgm2 =
                matrix(-1.0, 0.0, 0.0,
                       0.0, 20.0, 0.0,
                       0.0, 0.0, 30.0);
        });

    Options mass_gain_options;
    mass_gain_options.mass_sign = MassSign::Add;
    const Evaluation mass_gain = evaluate(first, mass_gain_options);
    Options early_options;
    early_options.visibility = Visibility::CommittedEarly;
    const Evaluation early = evaluate(first, early_options);
    Options com_options;
    com_options.com_mode = ComMode::Drift;
    const Evaluation com_drift = evaluate(first, com_options);
    Options inertia_options;
    inertia_options.inertia_mode = InertiaMode::MassRatio;
    const Evaluation inertia_scale = evaluate(first, inertia_options);
    std::vector<MutationResult> mutations{
        {"MUTATION-YYZ-SCALAR-BURN-MASS-GAIN", "rejected",
         MutationKind::MassGain, mass_gain,
         maxDifference(physicalVector(accepted), physicalVector(mass_gain))},
        {"MUTATION-YYZ-SCALAR-BURN-MASS-EARLY-VISIBILITY", "rejected",
         MutationKind::EarlyVisibility, early,
         maxDifference(physicalVector(accepted), physicalVector(early))},
        {"MUTATION-YYZ-SCALAR-BURN-MASS-COM-DRIFT", "rejected",
         MutationKind::ComDrift, com_drift,
         maxDifference(physicalVector(accepted), physicalVector(com_drift))},
        {"MUTATION-YYZ-SCALAR-BURN-MASS-INERTIA-SCALE", "rejected",
         MutationKind::InertiaScale, inertia_scale,
         maxDifference(physicalVector(accepted), physicalVector(inertia_scale))},
    };
    require(near(mutations[0].max_abs_physical_difference, 0.5) &&
                near(mutations[1].max_abs_physical_difference, 0.25) &&
                near(mutations[2].max_abs_physical_difference, 100.0) &&
                near(mutations[3].max_abs_physical_difference, 0.196875),
            "a scalar-burn mutation matched the accepted model");
    return {cases, partitionResult(first, accepted), invalid, mutations};
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

void writeMatrix(const Matrix3& value) {
    std::cout << '[';
    bool first = true;
    for (const auto& row : value.values) {
        for (double entry : row) {
            if (!first) {
                std::cout << ',';
            }
            writeNumber(entry);
            first = false;
        }
    }
    std::cout << ']';
}

void writeConsumer(const ConsumerResult& value) {
    std::cout << "{\"application_point_id\":\""
              << value.application_point_id << "\",\"mass_kg\":";
    writeNumber(value.mass_kg);
    std::cout << ",\"r_CoM_to_application_B_m\":";
    writeVec3(value.r_com_to_application_b_m);
    std::cout << ",\"force_B_N\":";
    writeVec3(value.force_b_n);
    std::cout << ",\"moment_about_CoM_B_Nm\":";
    writeVec3(value.moment_about_com_b_nm);
    std::cout << ",\"specific_force_B_mps2\":";
    writeVec3(value.specific_force_b_mps2);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    writeMatrix(value.inertia_about_com_b_kgm2);
    std::cout << ",\"angular_momentum_B_kgm2ps\":";
    writeVec3(value.angular_momentum_b_kgm2ps);
    std::cout << ",\"gyroscopic_moment_B_Nm\":";
    writeVec3(value.gyroscopic_moment_b_nm);
    std::cout << ",\"angular_acceleration_B_radps2\":";
    writeVec3(value.angular_acceleration_b_radps2);
    std::cout << '}';
}

void writeEvaluation(const Evaluation& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"identity\":{\"model_id\":\""
              << value.identity.model_id
              << "\",\"mass_state_id\":\""
              << value.identity.mass_state_id
              << "\",\"body_frame_id\":\""
              << value.identity.body_frame_id
              << "\",\"body_origin_point_id\":\""
              << value.identity.body_origin_point_id
              << "\",\"center_of_mass_point_id\":\""
              << value.identity.center_of_mass_point_id
              << "\",\"clock_domain\":\""
              << value.identity.clock_domain
              << "\",\"configuration_revision\":"
              << value.identity.configuration_revision
              << ",\"valid_from_tick\":"
              << value.identity.valid_from_tick
              << ",\"valid_until_tick\":"
              << value.identity.valid_until_tick
              << "},\"current_committed_sample\":{\"quality\":\""
              << value.current_sample.quality
              << "\",\"sample_tick\":"
              << value.current_sample.sample_tick << ",\"mass_kg\":";
    writeNumber(value.current_sample.mass_kg);
    std::cout << ",\"r_body_origin_to_CoM_B_m\":";
    writeVec3(value.current_sample.r_body_origin_to_com_b_m);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    writeMatrix(value.current_sample.inertia_about_com_b_kgm2);
    std::cout << ",\"visibility\":\""
              << value.current_sample.visibility
              << "\"},\"interval_candidate\":{\"source_id\":\""
              << value.candidate.source_id
              << "\",\"interval_duration_s\":";
    writeNumber(value.candidate.interval_duration_s);
    std::cout << ",\"fuel_consumption_rate_kgps\":";
    writeNumber(value.candidate.fuel_consumption_rate_kgps);
    std::cout << ",\"consumed_mass_kg\":";
    writeNumber(value.candidate.consumed_mass_kg);
    std::cout << ",\"mass_delta_kg\":";
    writeNumber(value.candidate.mass_delta_kg);
    std::cout << ",\"mass_kg\":";
    writeNumber(value.candidate.mass_kg);
    std::cout << ",\"r_body_origin_to_CoM_B_m\":";
    writeVec3(value.candidate.r_body_origin_to_com_b_m);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    writeMatrix(value.candidate.inertia_about_com_b_kgm2);
    std::cout << ",\"visibility_before_commit\":\""
              << value.candidate.visibility_before_commit
              << "\",\"next_commit_tick\":"
              << value.candidate.next_commit_tick
              << "},\"interval_consumer\":";
    writeConsumer(value.interval_consumer);
    std::cout << ",\"next_committed_sample\":{\"quality\":\""
              << value.next_sample.quality
              << "\",\"sample_tick\":" << value.next_sample.sample_tick
              << ",\"valid_from_tick\":"
              << value.next_sample.valid_from_tick
              << ",\"valid_until_tick\":"
              << value.next_sample.valid_until_tick
              << ",\"mass_kg\":";
    writeNumber(value.next_sample.mass_kg);
    std::cout << ",\"r_body_origin_to_CoM_B_m\":";
    writeVec3(value.next_sample.r_body_origin_to_com_b_m);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    writeMatrix(value.next_sample.inertia_about_com_b_kgm2);
    std::cout << ",\"visibility\":\"" << value.next_sample.visibility
              << "\"},\"next_consumer\":";
    writeConsumer(value.next_consumer);
    std::cout << '}';
}

void writePartition(const PartitionResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"status\":\"" << value.status
              << "\",\"first_interval_duration_s\":";
    writeNumber(value.first_interval_duration_s);
    std::cout << ",\"first_consumed_mass_kg\":";
    writeNumber(value.first_consumed_mass_kg);
    std::cout << ",\"first_committed_mass_kg\":";
    writeNumber(value.first_committed_mass_kg);
    std::cout << ",\"second_interval_duration_s\":";
    writeNumber(value.second_interval_duration_s);
    std::cout << ",\"second_consumed_mass_kg\":";
    writeNumber(value.second_consumed_mass_kg);
    std::cout << ",\"summed_consumed_mass_kg\":";
    writeNumber(value.summed_consumed_mass_kg);
    std::cout << ",\"partitioned_final_mass_kg\":";
    writeNumber(value.partitioned_final_mass_kg);
    std::cout << ",\"unsplit_final_mass_kg\":";
    writeNumber(value.unsplit_final_mass_kg);
    std::cout << ",\"max_abs_geometry_difference\":";
    writeNumber(value.max_abs_geometry_difference);
    std::cout << '}';
}

void writeMutation(const MutationResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"status\":\"" << value.status << '\"';
    if (value.kind == MutationKind::MassGain) {
        std::cout << ",\"observed_mass_delta_kg\":";
        writeNumber(value.observed.candidate.mass_delta_kg);
        std::cout << ",\"observed_candidate_mass_kg\":";
        writeNumber(value.observed.candidate.mass_kg);
    } else if (value.kind == MutationKind::EarlyVisibility) {
        std::cout << ",\"observed_visibility_before_commit\":\""
                  << value.observed.candidate.visibility_before_commit
                  << "\",\"observed_interval_visible_mass_kg\":";
        writeNumber(value.observed.interval_consumer.mass_kg);
        std::cout << ",\"observed_interval_specific_force_B_mps2\":";
        writeVec3(value.observed.interval_consumer.specific_force_b_mps2);
    } else if (value.kind == MutationKind::ComDrift) {
        std::cout <<
            ",\"observed_candidate_r_body_origin_to_CoM_B_m\":";
        writeVec3(value.observed.candidate.r_body_origin_to_com_b_m);
        std::cout << ",\"observed_next_r_CoM_to_application_B_m\":";
        writeVec3(value.observed.next_consumer.r_com_to_application_b_m);
        std::cout << ",\"observed_next_moment_about_CoM_B_Nm\":";
        writeVec3(value.observed.next_consumer.moment_about_com_b_nm);
    } else {
        std::cout <<
            ",\"observed_candidate_inertia_about_CoM_B_kgm2_row_major\":";
        writeMatrix(value.observed.candidate.inertia_about_com_b_kgm2);
        std::cout <<
            ",\"observed_next_angular_momentum_B_kgm2ps\":";
        writeVec3(value.observed.next_consumer.angular_momentum_b_kgm2ps);
        std::cout <<
            ",\"observed_next_angular_acceleration_B_radps2\":";
        writeVec3(value.observed.next_consumer.angular_acceleration_b_radps2);
    }
    std::cout << ",\"max_abs_physical_difference\":";
    writeNumber(value.max_abs_physical_difference);
    std::cout << '}';
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

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"model_id\":\"" << kModelId
              << "\",\"status\":\"passed\""
              << ",\"model_choice_status\":\""
              << kModelChoiceStatus << "\",\"cases\":[";
    for (std::size_t index = 0; index < result.cases.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeEvaluation(result.cases[index]);
    }
    std::cout << "],\"equivalence_results\":[";
    writePartition(result.partition);
    std::cout << "],\"invalid_input_rejections\":";
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
            "usage: gnc_yyz_scalar_burn_mass_probe --self-check\n";
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

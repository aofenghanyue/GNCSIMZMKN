#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-MASS-PROPERTIES-001";
constexpr const char* kModelId =
    "MODEL-YYZ-MASS-PROPERTIES-PROJECTION-001";
constexpr const char* kProfileStatus =
    "implemented-from-accepted-invariants";
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
};

struct MassState {
    double mass_kg = 0.0;
    Vec3 r_body_origin_to_com_b_m;
    Matrix3 inertia_about_com_b_kgm2;
};

struct PendingMassCandidate {
    std::string source_interval_id;
    std::int64_t next_commit_tick = 0;
    double mass_candidate_kg = 0.0;
    std::string visibility_before_commit;
};

struct ClosureProbe {
    std::string application_point_id;
    Vec3 r_body_origin_to_application_b_m;
    Vec3 force_b_n;
    Vec3 intrinsic_moment_at_application_b_nm;
};

struct RigidCoreProbe {
    Vec3 omega_bi_b_radps;
};

struct MassPropertiesInput {
    std::string id;
    Context current_context;
    MassState current_state;
    PendingMassCandidate pending_candidate;
    Context next_context;
    MassState next_state;
    ClosureProbe closure_probe;
    RigidCoreProbe rigid_core_probe;
};

struct FormulaOptions {
    bool early_candidate_visibility = false;
    bool omit_com_offset = false;
    bool diagonalize_inertia = false;
};

struct MassPropertiesSample {
    std::string model_id;
    std::string quality;
    std::string mass_state_id;
    std::string body_frame_id;
    std::string body_origin_point_id;
    std::string center_of_mass_point_id;
    std::int64_t sample_tick = 0;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    double mass_kg = 0.0;
    Vec3 r_body_origin_to_com_b_m;
    Matrix3 inertia_about_com_b_kgm2;
};

struct ClosureConsumer {
    std::string body_frame_id;
    std::int64_t sample_tick = 0;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    std::string application_point_id;
    Vec3 r_body_origin_to_application_b_m;
    Vec3 r_com_to_application_b_m;
    Vec3 force_b_n;
    Vec3 moment_at_application_b_nm;
    Vec3 lever_arm_moment_b_nm;
    Vec3 moment_about_com_b_nm;
};

struct RigidCoreConsumer {
    std::string body_frame_id;
    std::int64_t sample_tick = 0;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    double mass_kg = 0.0;
    double mass_reciprocal_per_kg = 0.0;
    Vec3 force_b_n;
    Vec3 specific_force_b_mps2;
    Matrix3 inertia_about_com_b_kgm2;
    Vec3 omega_bi_b_radps;
    Vec3 angular_momentum_b_kgm2ps;
    Vec3 gyroscopic_moment_b_nm;
    Vec3 net_moment_b_nm;
    Vec3 angular_acceleration_b_radps2;
};

struct PublicationSequence {
    std::int64_t current_sample_tick = 0;
    double current_visible_mass_kg = 0.0;
    std::string pending_source_interval_id;
    std::string pending_visibility_before_commit;
    double pending_mass_candidate_kg = 0.0;
    std::int64_t next_commit_tick = 0;
    double next_visible_mass_kg = 0.0;
    MassPropertiesSample next_sample;
};

struct MassPropertiesResult {
    std::string id;
    MassPropertiesSample current_sample;
    ClosureConsumer closure_consumer;
    RigidCoreConsumer rigid_core_consumer;
    PublicationSequence publication_sequence;
};

struct EquivalenceResult {
    std::string id;
    std::string status;
    Vec3 translation_b_m;
    Vec3 shifted_r_body_origin_to_com_b_m;
    double max_abs_consumer_difference = 0.0;
};

enum class MutationKind {
    EarlyCandidate,
    OmitComOffset,
    DiagonalizeInertia,
};

struct MutationResult {
    std::string id;
    std::string status;
    double max_abs_physical_difference = 0.0;
    MutationKind kind = MutationKind::EarlyCandidate;
    double observed_current_visible_mass_kg = 0.0;
    Vec3 observed_first_vector;
    Vec3 observed_second_vector;
    Vec3 observed_third_vector;
};

struct ProbeResult {
    std::vector<MassPropertiesResult> cases;
    std::vector<EquivalenceResult> equivalence_results;
    std::vector<std::string> invalid_input_rejections;
    std::vector<MutationResult> mutation_results;
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

bool near(double actual, double expected) {
    const double bound = kFormulaAbsolute + kFormulaRelative *
        std::max(std::abs(actual), std::abs(expected));
    return finite(actual) && finite(expected) &&
        std::abs(actual - expected) <= bound;
}

bool near(const Vec3& actual, const Vec3& expected) {
    return near(actual.x, expected.x) && near(actual.y, expected.y) &&
        near(actual.z, expected.z);
}

std::array<std::array<double, 3>, 3> cholesky(
    const Matrix3& inertia) {
    requireDomain(finite(inertia),
                  "mass-properties inertia must be finite");
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            requireDomain(inertia.values[row][column] ==
                              inertia.values[column][row],
                          "mass-properties inertia must be symmetric");
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
                              "mass-properties inertia must be positive definite");
                lower[row][column] = std::sqrt(residual);
            } else {
                lower[row][column] = residual / lower[column][column];
            }
        }
    }
    return lower;
}

Vec3 solveSpd(const Matrix3& inertia, const Vec3& rhs) {
    requireDomain(finite(rhs), "mass-properties solve rhs must be finite");
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
    const Vec3 answer = canonicalZero(
        {result[0], result[1], result[2]});
    requireDomain(finite(answer),
                  "mass-properties solve result must be finite");
    return answer;
}

void validateContext(const Context& context, const std::string& label) {
    requireDomain(context.mass_state_id == kMassStateId,
                  label + " mass state identity differs");
    requireDomain(context.body_frame_id == kBodyFrameId,
                  label + " body frame differs");
    requireDomain(context.clock_domain == kClockDomain,
                  label + " clock domain differs");
    requireDomain(context.sample_tick >= 0,
                  label + " sample tick must be nonnegative");
    requireDomain(context.configuration_revision >= 0,
                  label + " revision must be nonnegative");
    requireDomain(context.valid_from_tick >= 0 &&
                      context.valid_until_tick >= 0,
                  label + " interval ticks must be nonnegative");
    requireDomain(context.sample_tick == context.valid_from_tick,
                  label + " sample tick must equal interval start");
    requireDomain(context.valid_until_tick > context.valid_from_tick,
                  label + " interval must be nonempty");
}

void validateState(const MassState& state, const std::string& label) {
    requireDomain(finite(state.mass_kg) && state.mass_kg > 0.0,
                  label + " mass must be positive");
    requireDomain(finite(state.r_body_origin_to_com_b_m),
                  label + " CoM must be finite");
    static_cast<void>(cholesky(state.inertia_about_com_b_kgm2));
}

void validateInput(const MassPropertiesInput& input) {
    validateContext(input.current_context, "current MassProperties");
    validateContext(input.next_context, "next MassProperties");
    validateState(input.current_state, "current MassState");
    validateState(input.next_state, "next MassState");
    requireDomain(input.next_context.configuration_revision ==
                      input.current_context.configuration_revision,
                  "projection-only case changes configuration revision");
    requireDomain(!input.pending_candidate.source_interval_id.empty(),
                  "pending candidate source identity is empty");
    requireDomain(input.pending_candidate.visibility_before_commit ==
                      "candidate-only",
                  "pending candidate visibility differs");
    requireDomain(input.pending_candidate.next_commit_tick >= 0 &&
                      input.pending_candidate.next_commit_tick ==
                          input.current_context.valid_until_tick &&
                      input.pending_candidate.next_commit_tick ==
                          input.next_context.sample_tick,
                  "pending candidate commit tick differs");
    requireDomain(finite(input.pending_candidate.mass_candidate_kg) &&
                      input.pending_candidate.mass_candidate_kg > 0.0,
                  "pending candidate mass must be positive");
    requireDomain(input.pending_candidate.mass_candidate_kg ==
                      input.next_state.mass_kg,
                  "pending candidate and next committed mass differ");
    requireDomain(!input.closure_probe.application_point_id.empty() &&
                      finite(input.closure_probe.
                                 r_body_origin_to_application_b_m) &&
                      finite(input.closure_probe.force_b_n) &&
                      finite(input.closure_probe.
                                 intrinsic_moment_at_application_b_nm),
                  "Closure probe is invalid");
    requireDomain(finite(input.rigid_core_probe.omega_bi_b_radps),
                  "rigid-core omega must be finite");
}

MassPropertiesSample projectState(const Context& context,
                                  const MassState& state) {
    validateContext(context, "projected MassProperties");
    validateState(state, "projected MassState");
    return {
        kModelId,
        kQuality,
        kMassStateId,
        kBodyFrameId,
        kBodyOriginPointId,
        kComPointId,
        context.sample_tick,
        kClockDomain,
        context.configuration_revision,
        context.valid_from_tick,
        context.valid_until_tick,
        state.mass_kg,
        state.r_body_origin_to_com_b_m,
        state.inertia_about_com_b_kgm2,
    };
}

MassPropertiesResult evaluate(const MassPropertiesInput& input,
                              const FormulaOptions& options = {}) {
    validateInput(input);
    MassState visible_state = input.current_state;
    if (options.early_candidate_visibility) {
        visible_state.mass_kg = input.pending_candidate.mass_candidate_kg;
    }
    const MassPropertiesSample current_sample =
        projectState(input.current_context, visible_state);
    const MassPropertiesSample next_sample =
        projectState(input.next_context, input.next_state);

    const Vec3 radius = options.omit_com_offset ?
        input.closure_probe.r_body_origin_to_application_b_m :
        subtract(input.closure_probe.r_body_origin_to_application_b_m,
                 current_sample.r_body_origin_to_com_b_m);
    const Vec3 lever = cross(radius, input.closure_probe.force_b_n);
    const Vec3 moment_about_com = add(
        input.closure_probe.intrinsic_moment_at_application_b_nm, lever);
    const ClosureConsumer closure{
        kBodyFrameId,
        input.current_context.sample_tick,
        kClockDomain,
        input.current_context.configuration_revision,
        input.closure_probe.application_point_id,
        input.closure_probe.r_body_origin_to_application_b_m,
        radius,
        input.closure_probe.force_b_n,
        input.closure_probe.intrinsic_moment_at_application_b_nm,
        lever,
        moment_about_com,
    };

    Matrix3 consumer_inertia = current_sample.inertia_about_com_b_kgm2;
    if (options.diagonalize_inertia) {
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t column = 0; column < 3; ++column) {
                if (row != column) {
                    consumer_inertia.values[row][column] = 0.0;
                }
            }
        }
    }
    const double mass_reciprocal = 1.0 / current_sample.mass_kg;
    const Vec3 specific_force = scale(
        input.closure_probe.force_b_n, mass_reciprocal);
    const Vec3 angular_momentum = multiply(
        consumer_inertia, input.rigid_core_probe.omega_bi_b_radps);
    const Vec3 gyroscopic = cross(
        input.rigid_core_probe.omega_bi_b_radps, angular_momentum);
    const Vec3 net_moment = subtract(moment_about_com, gyroscopic);
    const Vec3 angular_acceleration = solveSpd(
        consumer_inertia, net_moment);
    const RigidCoreConsumer rigid{
        kBodyFrameId,
        input.current_context.sample_tick,
        kClockDomain,
        input.current_context.configuration_revision,
        current_sample.mass_kg,
        mass_reciprocal,
        input.closure_probe.force_b_n,
        specific_force,
        consumer_inertia,
        input.rigid_core_probe.omega_bi_b_radps,
        angular_momentum,
        gyroscopic,
        net_moment,
        angular_acceleration,
    };
    requireDomain(finite(specific_force) && finite(angular_momentum) &&
                      finite(gyroscopic) && finite(net_moment) &&
                      finite(angular_acceleration),
                  "MassProperties consumer produced a non-finite value");

    const PublicationSequence publication{
        input.current_context.sample_tick,
        current_sample.mass_kg,
        input.pending_candidate.source_interval_id,
        input.pending_candidate.visibility_before_commit,
        input.pending_candidate.mass_candidate_kg,
        input.pending_candidate.next_commit_tick,
        next_sample.mass_kg,
        next_sample,
    };
    return {input.id, current_sample, closure, rigid, publication};
}

Matrix3 makeMatrix(double xx, double xy, double xz,
                   double yx, double yy, double yz,
                   double zx, double zy, double zz) {
    Matrix3 result;
    result.values = {{{xx, xy, xz},
                      {yx, yy, yz},
                      {zx, zy, zz}}};
    return result;
}

Context makeContext(std::int64_t tick, std::int64_t until_tick,
                    std::int64_t revision) {
    return {kMassStateId, kBodyFrameId, kClockDomain, tick, revision,
            tick, until_tick};
}

std::vector<MassPropertiesInput> caseInputs() {
    return {
        {
            "CASE-YYZ-MASS-PROPERTIES-CURRENT-CANDIDATE-PUBLICATION",
            makeContext(20, 25, 8),
            {120.0, {0.2, -0.1, 0.05},
             makeMatrix(12.0, 1.0, 0.5,
                        1.0, 20.0, 2.0,
                        0.5, 2.0, 30.0)},
            {"mass-flow.fixture.propulsion.main.[20,25)",
             25, 119.75, "candidate-only"},
            makeContext(25, 30, 8),
            {119.75, {0.18, -0.08, 0.04},
             makeMatrix(11.5, 0.8, 0.4,
                        0.8, 19.2, 1.6,
                        0.4, 1.6, 29.0)},
            {"point.fixture.yyz.propulsion-main@1",
             {1.0, 0.4, -0.2}, {300.0, 400.0, 0.0},
             {1.0, -2.0, 3.0}},
            {{1.0, 2.0, 3.0}},
        },
        {
            "CASE-YYZ-MASS-PROPERTIES-DIAGONAL-ZERO-FLOW",
            makeContext(3, 7, 9),
            {10.0, {-0.5, 0.25, 0.1},
             makeMatrix(4.0, 0.0, 0.0,
                        0.0, 5.0, 0.0,
                        0.0, 0.0, 6.0)},
            {"mass-flow.fixture.propulsion.main.[3,7)",
             7, 10.0, "candidate-only"},
            makeContext(7, 8, 9),
            {10.0, {-0.5, 0.25, 0.1},
             makeMatrix(4.0, 0.0, 0.0,
                        0.0, 5.0, 0.0,
                        0.0, 0.0, 6.0)},
            {"point.fixture.yyz.propulsion-main@1",
             {0.2, -0.1, 0.3}, {40.0, 0.0, 30.0},
             {-4.0, 5.0, -6.0}},
            {{0.2, -0.1, 0.3}},
        },
    };
}

void append(std::vector<double>& values, const Vec3& value) {
    values.push_back(value.x);
    values.push_back(value.y);
    values.push_back(value.z);
}

void append(std::vector<double>& values, const Matrix3& value) {
    for (const auto& row : value.values) {
        values.insert(values.end(), row.begin(), row.end());
    }
}

std::vector<double> physicalValues(const MassPropertiesResult& value) {
    std::vector<double> values;
    values.push_back(value.current_sample.mass_kg);
    append(values, value.current_sample.r_body_origin_to_com_b_m);
    append(values, value.current_sample.inertia_about_com_b_kgm2);
    const ClosureConsumer& closure = value.closure_consumer;
    append(values, closure.r_body_origin_to_application_b_m);
    append(values, closure.r_com_to_application_b_m);
    append(values, closure.force_b_n);
    append(values, closure.moment_at_application_b_nm);
    append(values, closure.lever_arm_moment_b_nm);
    append(values, closure.moment_about_com_b_nm);
    const RigidCoreConsumer& rigid = value.rigid_core_consumer;
    values.push_back(rigid.mass_kg);
    values.push_back(rigid.mass_reciprocal_per_kg);
    append(values, rigid.force_b_n);
    append(values, rigid.specific_force_b_mps2);
    append(values, rigid.inertia_about_com_b_kgm2);
    append(values, rigid.omega_bi_b_radps);
    append(values, rigid.angular_momentum_b_kgm2ps);
    append(values, rigid.gyroscopic_moment_b_nm);
    append(values, rigid.net_moment_b_nm);
    append(values, rigid.angular_acceleration_b_radps2);
    values.push_back(value.publication_sequence.current_visible_mass_kg);
    values.push_back(value.publication_sequence.pending_mass_candidate_kg);
    values.push_back(value.publication_sequence.next_visible_mass_kg);
    return values;
}

std::vector<double> consumerInvariantValues(
    const MassPropertiesResult& value) {
    std::vector<double> values;
    values.push_back(value.current_sample.mass_kg);
    append(values, value.current_sample.inertia_about_com_b_kgm2);
    const ClosureConsumer& closure = value.closure_consumer;
    append(values, closure.r_com_to_application_b_m);
    append(values, closure.force_b_n);
    append(values, closure.moment_at_application_b_nm);
    append(values, closure.lever_arm_moment_b_nm);
    append(values, closure.moment_about_com_b_nm);
    const RigidCoreConsumer& rigid = value.rigid_core_consumer;
    values.push_back(rigid.mass_kg);
    values.push_back(rigid.mass_reciprocal_per_kg);
    append(values, rigid.force_b_n);
    append(values, rigid.specific_force_b_mps2);
    append(values, rigid.inertia_about_com_b_kgm2);
    append(values, rigid.omega_bi_b_radps);
    append(values, rigid.angular_momentum_b_kgm2ps);
    append(values, rigid.gyroscopic_moment_b_nm);
    append(values, rigid.net_moment_b_nm);
    append(values, rigid.angular_acceleration_b_radps2);
    return values;
}

double maxDifference(const std::vector<double>& lhs,
                     const std::vector<double>& rhs) {
    require(lhs.size() == rhs.size(),
            "MassProperties physical vectors differ in length");
    double maximum = 0.0;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        maximum = std::max(maximum,
                           std::abs(lhs[index] - rhs[index]));
    }
    return maximum;
}

EquivalenceResult originTranslation(const MassPropertiesInput& base) {
    const MassPropertiesResult accepted = evaluate(base);
    MassPropertiesInput shifted = base;
    const Vec3 offset{10.0, -5.0, 2.0};
    shifted.current_state.r_body_origin_to_com_b_m = add(
        shifted.current_state.r_body_origin_to_com_b_m, offset);
    shifted.closure_probe.r_body_origin_to_application_b_m = add(
        shifted.closure_probe.r_body_origin_to_application_b_m, offset);
    const MassPropertiesResult shifted_result = evaluate(shifted);
    const double difference = maxDifference(
        consumerInvariantValues(accepted),
        consumerInvariantValues(shifted_result));
    return {
        "EQUIV-YYZ-MASS-PROPERTIES-BODY-ORIGIN-TRANSLATION",
        difference <= kFormulaAbsolute ? "passed" : "failed",
        offset,
        shifted_result.current_sample.r_body_origin_to_com_b_m,
        difference,
    };
}

template <typename Operation>
bool rejected(Operation&& operation) {
    try {
        operation();
    } catch (const std::domain_error&) {
        return true;
    }
    return false;
}

void recordInvalid(ProbeResult& result, const std::string& identifier,
                   const MassPropertiesInput& input) {
    if (rejected([&] { static_cast<void>(evaluate(input)); })) {
        result.invalid_input_rejections.push_back(identifier);
    }
}

MutationResult makeMutation(const std::string& id, MutationKind kind,
                            const MassPropertiesResult& accepted,
                            const MassPropertiesResult& mutated) {
    MutationResult result;
    result.id = id;
    result.kind = kind;
    result.max_abs_physical_difference = maxDifference(
        physicalValues(accepted), physicalValues(mutated));
    result.status = result.max_abs_physical_difference >
        kFormulaAbsolute ? "rejected" : "matched";
    if (kind == MutationKind::EarlyCandidate) {
        result.observed_current_visible_mass_kg =
            mutated.publication_sequence.current_visible_mass_kg;
        result.observed_first_vector =
            mutated.rigid_core_consumer.specific_force_b_mps2;
    } else if (kind == MutationKind::OmitComOffset) {
        result.observed_first_vector =
            mutated.closure_consumer.r_com_to_application_b_m;
        result.observed_second_vector =
            mutated.closure_consumer.moment_about_com_b_nm;
    } else {
        result.observed_first_vector =
            mutated.rigid_core_consumer.angular_momentum_b_kgm2ps;
        result.observed_second_vector =
            mutated.rigid_core_consumer.gyroscopic_moment_b_nm;
        result.observed_third_vector =
            mutated.rigid_core_consumer.angular_acceleration_b_radps2;
    }
    return result;
}

ProbeResult runProbe() {
    ProbeResult result;
    const std::vector<MassPropertiesInput> inputs = caseInputs();
    for (const MassPropertiesInput& input : inputs) {
        result.cases.push_back(evaluate(input));
    }
    require(result.cases.size() == 2,
            "MassProperties case coverage is incomplete");
    require(near(result.cases[0].current_sample.mass_kg, 120.0) &&
                near(result.cases[0].closure_consumer.
                         r_com_to_application_b_m,
                     {0.8, 0.5, -0.25}) &&
                near(result.cases[0].closure_consumer.
                         lever_arm_moment_b_nm,
                     {100.0, -75.0, 170.0}) &&
                near(result.cases[0].closure_consumer.
                         moment_about_com_b_nm,
                     {101.0, -77.0, 173.0}) &&
                near(result.cases[0].rigid_core_consumer.
                         angular_momentum_b_kgm2ps,
                     {15.5, 47.0, 94.5}) &&
                near(result.cases[0].rigid_core_consumer.
                         gyroscopic_moment_b_nm,
                     {48.0, -48.0, 16.0}) &&
                near(result.cases[0].rigid_core_consumer.net_moment_b_nm,
                     {53.0, -29.0, 157.0}) &&
                near(result.cases[0].publication_sequence.
                         next_visible_mass_kg,
                     119.75),
            "MassProperties current/candidate anchor differs");
    require(near(result.cases[1].closure_consumer.
                     r_com_to_application_b_m,
                 {0.7, -0.35, 0.2}) &&
                near(result.cases[1].closure_consumer.
                         moment_about_com_b_nm,
                     {-14.5, -8.0, 8.0}) &&
                near(result.cases[1].rigid_core_consumer.
                         gyroscopic_moment_b_nm,
                     {-0.03, -0.12, -0.02}),
            "diagonal MassProperties anchor differs");

    result.equivalence_results.push_back(originTranslation(inputs[0]));
    require(result.equivalence_results.size() == 1 &&
                result.equivalence_results[0].status == "passed" &&
                near(result.equivalence_results[0].
                         shifted_r_body_origin_to_com_b_m,
                     {10.2, -5.1, 2.05}),
            "MassProperties body-origin translation failed");

    MassPropertiesInput invalid = inputs[0];
    invalid.current_context.mass_state_id = "mass.other@1";
    recordInvalid(result,
                  "INVALID-YYZ-MASS-PROPERTIES-STATE-IDENTITY", invalid);
    invalid = inputs[0];
    invalid.current_context.body_frame_id = "frame.other@1";
    recordInvalid(result,
                  "INVALID-YYZ-MASS-PROPERTIES-FRAME-MISMATCH", invalid);
    invalid = inputs[0];
    invalid.current_context.clock_domain = "clock.other@1";
    recordInvalid(result,
                  "INVALID-YYZ-MASS-PROPERTIES-CLOCK-MISMATCH", invalid);
    invalid = inputs[0];
    invalid.current_context.configuration_revision = -1;
    recordInvalid(result,
                  "INVALID-YYZ-MASS-PROPERTIES-NEGATIVE-REVISION", invalid);
    invalid = inputs[0];
    invalid.current_context.sample_tick = 21;
    recordInvalid(
        result,
        "INVALID-YYZ-MASS-PROPERTIES-SAMPLE-INTERVAL-MISMATCH",
        invalid);
    invalid = inputs[0];
    invalid.current_state.mass_kg = 0.0;
    recordInvalid(result,
                  "INVALID-YYZ-MASS-PROPERTIES-NONPOSITIVE-MASS", invalid);
    invalid = inputs[0];
    invalid.current_state.r_body_origin_to_com_b_m.x =
        std::numeric_limits<double>::infinity();
    recordInvalid(result,
                  "INVALID-YYZ-MASS-PROPERTIES-NONFINITE-COM", invalid);
    invalid = inputs[0];
    invalid.current_state.inertia_about_com_b_kgm2.values[0][1] = 2.0;
    recordInvalid(result,
                  "INVALID-YYZ-MASS-PROPERTIES-ASYMMETRIC-INERTIA",
                  invalid);
    invalid = inputs[0];
    invalid.current_state.inertia_about_com_b_kgm2.values[0][0] = -1.0;
    recordInvalid(result,
                  "INVALID-YYZ-MASS-PROPERTIES-NON-SPD-INERTIA", invalid);
    invalid = inputs[0];
    invalid.pending_candidate.mass_candidate_kg = 119.5;
    recordInvalid(
        result,
        "INVALID-YYZ-MASS-PROPERTIES-CANDIDATE-COMMIT-MISMATCH",
        invalid);
    require(result.invalid_input_rejections.size() == 10,
            "an invalid MassProperties input was accepted");

    const MassPropertiesResult& accepted = result.cases[0];
    FormulaOptions options;
    options.early_candidate_visibility = true;
    result.mutation_results.push_back(makeMutation(
        "MUTATION-YYZ-MASS-PROPERTIES-EARLY-CANDIDATE-VISIBILITY",
        MutationKind::EarlyCandidate, accepted, evaluate(inputs[0], options)));
    options = {};
    options.omit_com_offset = true;
    result.mutation_results.push_back(makeMutation(
        "MUTATION-YYZ-MASS-PROPERTIES-OMIT-COM-OFFSET",
        MutationKind::OmitComOffset, accepted, evaluate(inputs[0], options)));
    options = {};
    options.diagonalize_inertia = true;
    result.mutation_results.push_back(makeMutation(
        "MUTATION-YYZ-MASS-PROPERTIES-DIAGONALIZE-INERTIA",
        MutationKind::DiagonalizeInertia, accepted,
        evaluate(inputs[0], options)));
    require(result.mutation_results.size() == 3 &&
                std::all_of(result.mutation_results.begin(),
                            result.mutation_results.end(),
                            [](const MutationResult& mutation) {
                                return mutation.status == "rejected";
                            }) &&
                near(result.mutation_results[0].
                         max_abs_physical_difference,
                     0.25) &&
                near(result.mutation_results[1].
                         max_abs_physical_difference,
                     110.0) &&
                near(result.mutation_results[2].
                         max_abs_physical_difference,
                     12.0),
            "a MassProperties mutation matched the accepted result");
    return result;
}

void writeVec3(const Vec3& value) {
    std::cout << '[' << value.x << ',' << value.y << ',' << value.z << ']';
}

void writeMatrix(const Matrix3& value) {
    std::cout << '[';
    bool first = true;
    for (const auto& row : value.values) {
        for (double entry : row) {
            if (!first) {
                std::cout << ',';
            }
            std::cout << entry;
            first = false;
        }
    }
    std::cout << ']';
}

void writeSample(const MassPropertiesSample& value) {
    std::cout << "{\"model_id\":\"" << value.model_id
              << "\",\"quality\":\"" << value.quality
              << "\",\"mass_state_id\":\"" << value.mass_state_id
              << "\",\"body_frame_id\":\"" << value.body_frame_id
              << "\",\"body_origin_point_id\":\""
              << value.body_origin_point_id
              << "\",\"center_of_mass_point_id\":\""
              << value.center_of_mass_point_id
              << "\",\"sample_tick\":" << value.sample_tick
              << ",\"clock_domain\":\"" << value.clock_domain
              << "\",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"valid_from_tick\":" << value.valid_from_tick
              << ",\"valid_until_tick\":" << value.valid_until_tick
              << ",\"mass_kg\":" << value.mass_kg
              << ",\"r_body_origin_to_CoM_B_m\":";
    writeVec3(value.r_body_origin_to_com_b_m);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    writeMatrix(value.inertia_about_com_b_kgm2);
    std::cout << '}';
}

void writeClosure(const ClosureConsumer& value) {
    std::cout << "{\"body_frame_id\":\"" << value.body_frame_id
              << "\",\"sample_tick\":" << value.sample_tick
              << ",\"clock_domain\":\"" << value.clock_domain
              << "\",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"application_point_id\":\""
              << value.application_point_id
              << "\",\"r_body_origin_to_application_B_m\":";
    writeVec3(value.r_body_origin_to_application_b_m);
    std::cout << ",\"r_CoM_to_application_B_m\":";
    writeVec3(value.r_com_to_application_b_m);
    std::cout << ",\"force_B_N\":";
    writeVec3(value.force_b_n);
    std::cout << ",\"moment_at_application_B_Nm\":";
    writeVec3(value.moment_at_application_b_nm);
    std::cout << ",\"lever_arm_moment_B_Nm\":";
    writeVec3(value.lever_arm_moment_b_nm);
    std::cout << ",\"moment_about_CoM_B_Nm\":";
    writeVec3(value.moment_about_com_b_nm);
    std::cout << '}';
}

void writeRigid(const RigidCoreConsumer& value) {
    std::cout << "{\"body_frame_id\":\"" << value.body_frame_id
              << "\",\"sample_tick\":" << value.sample_tick
              << ",\"clock_domain\":\"" << value.clock_domain
              << "\",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"mass_kg\":" << value.mass_kg
              << ",\"mass_reciprocal_per_kg\":"
              << value.mass_reciprocal_per_kg
              << ",\"force_B_N\":";
    writeVec3(value.force_b_n);
    std::cout << ",\"specific_force_B_mps2\":";
    writeVec3(value.specific_force_b_mps2);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    writeMatrix(value.inertia_about_com_b_kgm2);
    std::cout << ",\"omega_BI_B_radps\":";
    writeVec3(value.omega_bi_b_radps);
    std::cout << ",\"angular_momentum_B_kgm2ps\":";
    writeVec3(value.angular_momentum_b_kgm2ps);
    std::cout << ",\"gyroscopic_moment_B_Nm\":";
    writeVec3(value.gyroscopic_moment_b_nm);
    std::cout << ",\"net_moment_B_Nm\":";
    writeVec3(value.net_moment_b_nm);
    std::cout << ",\"angular_acceleration_B_radps2\":";
    writeVec3(value.angular_acceleration_b_radps2);
    std::cout << '}';
}

void writePublication(const PublicationSequence& value) {
    std::cout << "{\"current_sample_tick\":"
              << value.current_sample_tick
              << ",\"current_visible_mass_kg\":"
              << value.current_visible_mass_kg
              << ",\"pending_source_interval_id\":\""
              << value.pending_source_interval_id
              << "\",\"pending_visibility_before_commit\":\""
              << value.pending_visibility_before_commit
              << "\",\"pending_mass_candidate_kg\":"
              << value.pending_mass_candidate_kg
              << ",\"next_commit_tick\":" << value.next_commit_tick
              << ",\"next_visible_mass_kg\":"
              << value.next_visible_mass_kg
              << ",\"next_sample\":";
    writeSample(value.next_sample);
    std::cout << '}';
}

void writeCase(const MassPropertiesResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"current_sample\":";
    writeSample(value.current_sample);
    std::cout << ",\"closure_consumer\":";
    writeClosure(value.closure_consumer);
    std::cout << ",\"rigid_core_consumer\":";
    writeRigid(value.rigid_core_consumer);
    std::cout << ",\"publication_sequence\":";
    writePublication(value.publication_sequence);
    std::cout << '}';
}

void writeEquivalence(const EquivalenceResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"status\":\"" << value.status
              << "\",\"translation_B_m\":";
    writeVec3(value.translation_b_m);
    std::cout << ",\"shifted_r_body_origin_to_CoM_B_m\":";
    writeVec3(value.shifted_r_body_origin_to_com_b_m);
    std::cout << ",\"max_abs_consumer_difference\":"
              << value.max_abs_consumer_difference << '}';
}

void writeMutation(const MutationResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"status\":\"" << value.status
              << "\",\"max_abs_physical_difference\":"
              << value.max_abs_physical_difference;
    if (value.kind == MutationKind::EarlyCandidate) {
        std::cout << ",\"observed_current_visible_mass_kg\":"
                  << value.observed_current_visible_mass_kg
                  << ",\"observed_specific_force_B_mps2\":";
        writeVec3(value.observed_first_vector);
    } else if (value.kind == MutationKind::OmitComOffset) {
        std::cout << ",\"observed_r_CoM_to_application_B_m\":";
        writeVec3(value.observed_first_vector);
        std::cout << ",\"observed_moment_about_CoM_B_Nm\":";
        writeVec3(value.observed_second_vector);
    } else {
        std::cout << ",\"observed_angular_momentum_B_kgm2ps\":";
        writeVec3(value.observed_first_vector);
        std::cout << ",\"observed_gyroscopic_moment_B_Nm\":";
        writeVec3(value.observed_second_vector);
        std::cout << ",\"observed_angular_acceleration_B_radps2\":";
        writeVec3(value.observed_third_vector);
    }
    std::cout << '}';
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

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"model_id\":\"" << kModelId
              << "\",\"status\":\"passed\""
              << ",\"semantic_profile_status\":\""
              << kProfileStatus << "\",\"cases\":[";
    for (std::size_t index = 0; index < result.cases.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeCase(result.cases[index]);
    }
    std::cout << "],\"equivalence_results\":[";
    for (std::size_t index = 0;
         index < result.equivalence_results.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeEquivalence(result.equivalence_results[index]);
    }
    std::cout << "],\"invalid_input_rejections\":";
    writeStringList(result.invalid_input_rejections);
    std::cout << ",\"mutation_results\":[";
    for (std::size_t index = 0;
         index < result.mutation_results.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeMutation(result.mutation_results[index]);
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr <<
            "usage: gnc_yyz_mass_properties_probe --self-check\n";
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

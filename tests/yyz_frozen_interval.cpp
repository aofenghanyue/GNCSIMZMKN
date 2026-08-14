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

constexpr const char* kOracleId = "ORACLE-YYZ-FROZEN-INTERVAL-001";
constexpr const char* kModelId =
    "MODEL-YYZ-LOOKUP-FROZEN-INTERVAL-001";
constexpr const char* kProfileStatus =
    "implemented-from-accepted-profiles";
constexpr const char* kInertialFrameId =
    "frame.fixture.yyz.inertial-cartesian@1";
constexpr const char* kBodyFrameId = "frame.fixture.yyz.body@1";
constexpr const char* kClockDomain = "clock.fixture.yyz.simulation@1";
constexpr const char* kMassStateId = "mass.fixture.yyz.vehicle@1";
constexpr const char* kAeroLookupModelId =
    "MODEL-YYZ-AERO-TRILINEAR-LOOKUP-001";
constexpr const char* kAeroTableId =
    "aero-table.fixture.yyz.multiaffine@1";
constexpr const char* kAeroConfigurationId =
    "configuration.fixture.yyz.clean@1";
constexpr double kFormulaAbsolute = 2.0e-12;
constexpr double kFormulaRelative = 2.0e-12;

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

struct Identity {
    std::string frame_id;
    std::string clock_domain;
    std::int64_t sample_tick = 0;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
};

struct Input {
    std::string id;
    std::string inertial_frame_id;
    std::string body_frame_id;
    std::string clock_domain;
    std::int64_t sample_tick = 0;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    double dt_s = 0.0;
    State initial_state;

    Identity environment_identity;
    Vec3 gravity_i_mps2;
    Vec3 velocity_airmass_i_mps;
    double density_kgpm3 = 0.0;
    double speed_of_sound_mps = 0.0;

    Identity mass_identity;
    std::string mass_state_id;
    double mass_kg = 0.0;
    Vec3 r_body_origin_to_com_b_m;
    Matrix3 inertia_about_com_b_kgm2;

    Identity aero_identity;
    std::string aero_source_id;
    std::string aero_lookup_model_id;
    std::string aero_table_id;
    std::string aero_configuration_id;
    double reference_area_m2 = 0.0;
    double reference_span_m = 0.0;
    double reference_chord_m = 0.0;
    Vec3 aero_query_rates_b_radps;
    std::array<double, 4> aero_surface_state_rad{};
    std::vector<std::string> aero_required_derivatives;
    std::array<double, 2> mach_axis{};
    std::array<double, 2> alpha_axis_rad{};
    std::array<double, 2> beta_axis_rad{};
    std::array<std::array<double, 6>, 8> aero_coefficient_rows{};
    Vec3 r_body_origin_to_aero_application_b_m;

    Identity propulsion_identity;
    std::string propulsion_source_id;
    double thrust_magnitude_n = 0.0;
    Vec3 thrust_direction_b_unit;
    Vec3 r_body_origin_to_propulsion_application_b_m;
    Vec3 intrinsic_propulsion_moment_at_application_b_nm;
    double fuel_consumption_rate_kgps = 0.0;

    std::string terminal_kind;
    std::int64_t terminal_tick = 0;
};

enum class WindMode { Subtract, Add };

struct Options {
    WindMode wind_mode = WindMode::Subtract;
    bool early_mass_candidate = false;
    bool pretransport_propulsion_moment = false;
    bool nearest_aero_lookup = false;
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
    std::array<std::size_t, 3> cell_indices{};
    Vec3 weights;
    std::array<double, 6> coefficients{};
};

struct Contribution {
    std::string source_id;
    Vec3 force_b_n;
    Vec3 r_com_to_application_b_m;
    Vec3 moment_at_application_b_nm;
    Vec3 transport_moment_b_nm;
    Vec3 moment_about_com_b_nm;
};

struct MassVisibility {
    std::string mass_state_id;
    double current_visible_mass_kg = 0.0;
    double consumed_mass_kg = 0.0;
    double pending_mass_candidate_kg = 0.0;
    std::string pending_visibility_before_commit;
    double integration_mass_kg = 0.0;
    std::int64_t next_commit_tick = 0;
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

struct Terminal {
    std::int64_t tick = 0;
    double time_s = 0.0;
    State state;
    std::string termination_kind;
};

struct Composition {
    std::string id;
    std::int64_t sample_tick = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    std::int64_t configuration_revision = 0;
    double dt_s = 0.0;
    Vec3 gravity_i_mps2;
    Vec3 velocity_airmass_i_mps;
    double density_kgpm3 = 0.0;
    double speed_of_sound_mps = 0.0;
    AirData air_data;
    AeroLookup aero_lookup;
    Contribution aero;
    Contribution propulsion;
    MassVisibility mass;
    Vec3 force_total_b_n;
    Vec3 moment_total_about_com_b_nm;
    RigidDerivative derivative;
    Terminal terminal;
};

struct Derivative {
    Vec3 position;
    Vec3 velocity;
    Quaternion attitude;
    Vec3 angular_rate;
};

struct EquivalenceResult {
    std::string id;
    std::string status;
    Quaternion alternate_q_i_b;
    double orientation_error_rad = 0.0;
    double max_abs_physical_difference = 0.0;
};

enum class MutationKind {
    AddWind,
    EarlyMass,
    PretransportedMoment,
    NearestAeroLookup,
};

struct MutationResult {
    std::string id;
    std::string status;
    MutationKind kind = MutationKind::AddWind;
    Composition observed;
    double max_abs_physical_difference = 0.0;
};

struct ProbeResult {
    Composition accepted;
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

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return {canonicalZero(lhs.x - rhs.x), canonicalZero(lhs.y - rhs.y),
            canonicalZero(lhs.z - rhs.z)};
}

Vec3 scale(const Vec3& value, double factor) {
    return {canonicalZero(value.x * factor),
            canonicalZero(value.y * factor),
            canonicalZero(value.z * factor)};
}

Vec3 canonicalZero(const Vec3& value) {
    return {canonicalZero(value.x), canonicalZero(value.y),
            canonicalZero(value.z)};
}

double dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero(Vec3{
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
    const double bound = kFormulaAbsolute + kFormulaRelative *
        std::max({1.0, std::abs(actual), std::abs(expected)});
    return difference <= bound;
}

double maxDifference(const Vec3& lhs, const Vec3& rhs) {
    return std::max({std::abs(lhs.x - rhs.x), std::abs(lhs.y - rhs.y),
                     std::abs(lhs.z - rhs.z)});
}

void validateIdentity(const Identity& identity, const Input& input,
                      const std::string& expected_frame,
                      bool interval, const std::string& label) {
    requireDomain(identity.frame_id == expected_frame,
                  label + " frame identity differs");
    requireDomain(identity.clock_domain == input.clock_domain,
                  label + " clock identity differs");
    requireDomain(identity.sample_tick == input.sample_tick,
                  label + " sample tick differs");
    requireDomain(identity.configuration_revision ==
                      input.configuration_revision,
                  label + " configuration revision differs");
    if (interval) {
        requireDomain(identity.valid_from_tick == input.valid_from_tick &&
                          identity.valid_until_tick == input.valid_until_tick,
                      label + " validity interval differs");
    }
}

void validateInput(const Input& input) {
    requireDomain(input.inertial_frame_id == kInertialFrameId &&
                      input.body_frame_id == kBodyFrameId &&
                      input.clock_domain == kClockDomain,
                  "FrozenInterval context identity differs");
    requireDomain(input.sample_tick >= 0 &&
                      input.configuration_revision >= 0 &&
                      input.valid_from_tick >= 0 &&
                      input.valid_until_tick >= 0,
                  "FrozenInterval tick or revision is invalid");
    requireDomain(input.sample_tick == input.valid_from_tick &&
                      input.valid_until_tick == input.sample_tick + 1,
                  "FrozenInterval must span one tick interval");
    requireDomain(finite(input.dt_s) && input.dt_s > 0.0,
                  "base dt must be positive and finite");
    validateIdentity(input.environment_identity, input,
                     input.inertial_frame_id, false, "environment");
    validateIdentity(input.mass_identity, input,
                     input.body_frame_id, true, "MassProperties");
    validateIdentity(input.aero_identity, input,
                     input.body_frame_id, true, "aero");
    validateIdentity(input.propulsion_identity, input,
                     input.body_frame_id, true, "propulsion");
    requireDomain(input.mass_state_id == kMassStateId,
                  "mass state identity differs");
    requireDomain(!input.aero_source_id.empty() &&
                      !input.propulsion_source_id.empty() &&
                      input.aero_source_id != input.propulsion_source_id,
                  "Closure source identities must be distinct");
    requireDomain(input.aero_lookup_model_id == kAeroLookupModelId &&
                      input.aero_table_id == kAeroTableId &&
                      input.aero_configuration_id == kAeroConfigurationId,
                  "aero lookup identity differs");
    requireDomain(finite(input.initial_state.position_i_m) &&
                      finite(input.initial_state.velocity_i_mps) &&
                      finite(input.initial_state.omega_bi_b_radps),
                  "initial rigid state contains a non-finite value");
    static_cast<void>(normalize(input.initial_state.q_i_b));
    requireDomain(finite(input.gravity_i_mps2) &&
                      finite(input.velocity_airmass_i_mps) &&
                      finite(input.density_kgpm3) &&
                      input.density_kgpm3 >= 0.0 &&
                      finite(input.speed_of_sound_mps) &&
                      input.speed_of_sound_mps > 0.0,
                  "environment sample is outside its domain");
    requireDomain(finite(input.mass_kg) && input.mass_kg > 0.0 &&
                      finite(input.r_body_origin_to_com_b_m),
                  "MassProperties sample is outside its domain");
    static_cast<void>(cholesky(input.inertia_about_com_b_kgm2));
    requireDomain(finite(input.reference_area_m2) &&
                      input.reference_area_m2 > 0.0 &&
                      finite(input.reference_span_m) &&
                      input.reference_span_m > 0.0 &&
                      finite(input.reference_chord_m) &&
                      input.reference_chord_m > 0.0 &&
                      finite(input.r_body_origin_to_aero_application_b_m) &&
                      finite(input.aero_query_rates_b_radps) &&
                      maxDifference(input.aero_query_rates_b_radps,
                                    input.initial_state.omega_bi_b_radps) == 0.0 &&
                      dot(input.aero_query_rates_b_radps,
                          input.aero_query_rates_b_radps) == 0.0 &&
                      std::all_of(input.aero_surface_state_rad.begin(),
                                  input.aero_surface_state_rad.end(),
                                  [](double value) {
                                      return finite(value) && value == 0.0;
                                  }) &&
                      input.aero_required_derivatives.empty(),
                  "aero lookup query scope is outside its domain");
    requireDomain(input.mach_axis[0] < input.mach_axis[1] &&
                      input.alpha_axis_rad[0] < input.alpha_axis_rad[1] &&
                      input.beta_axis_rad[0] < input.beta_axis_rad[1] &&
                      std::all_of(input.mach_axis.begin(),
                                  input.mach_axis.end(),
                                  [](double value) { return finite(value); }) &&
                      std::all_of(input.alpha_axis_rad.begin(),
                                  input.alpha_axis_rad.end(),
                                  [](double value) { return finite(value); }) &&
                      std::all_of(input.beta_axis_rad.begin(),
                                  input.beta_axis_rad.end(),
                                  [](double value) { return finite(value); }) &&
                      std::all_of(
                          input.aero_coefficient_rows.begin(),
                          input.aero_coefficient_rows.end(),
                          [](const auto& row) {
                              return std::all_of(
                                  row.begin(), row.end(),
                                  [](double value) { return finite(value); });
                          }),
                  "prepared aero table is outside its domain");
    requireDomain(finite(input.thrust_magnitude_n) &&
                      input.thrust_magnitude_n >= 0.0 &&
                      finite(input.thrust_direction_b_unit) &&
                      near(std::sqrt(dot(input.thrust_direction_b_unit,
                                         input.thrust_direction_b_unit)),
                           1.0) &&
                      finite(input.r_body_origin_to_propulsion_application_b_m) &&
                      finite(input.intrinsic_propulsion_moment_at_application_b_nm) &&
                      finite(input.fuel_consumption_rate_kgps) &&
                      input.fuel_consumption_rate_kgps >= 0.0,
                  "propulsion response is outside its domain");
    requireDomain(input.mass_kg - input.fuel_consumption_rate_kgps *
                      input.dt_s > 0.0,
                  "scalar mass candidate must be positive");
    requireDomain(input.terminal_kind == "duration_exact_grid" &&
                      input.terminal_tick == input.valid_until_tick,
                  "terminal identity differs");
}

Input acceptedInput() {
    Input input;
    input.id = "CASE-YYZ-FROZEN-INTERVAL-COMPOSED-TRAJECTORY";
    input.inertial_frame_id = kInertialFrameId;
    input.body_frame_id = kBodyFrameId;
    input.clock_domain = kClockDomain;
    input.sample_tick = 0;
    input.configuration_revision = 11;
    input.valid_from_tick = 0;
    input.valid_until_tick = 1;
    input.dt_s = 0.1;
    input.initial_state = {
        {0.0, 0.0, 1000.0},
        {110.0, 0.0, 0.0},
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
    };
    input.environment_identity = {
        kInertialFrameId, kClockDomain, 0, 11, 0, 0};
    input.gravity_i_mps2 = {0.0, 0.0, -9.80665};
    input.velocity_airmass_i_mps = {10.0, 0.0, 0.0};
    input.density_kgpm3 = 1.225;
    input.speed_of_sound_mps = 340.0;
    input.mass_identity = {kBodyFrameId, kClockDomain, 0, 11, 0, 1};
    input.mass_state_id = kMassStateId;
    input.mass_kg = 100.0;
    input.r_body_origin_to_com_b_m = {0.2, 0.0, 0.0};
    input.inertia_about_com_b_kgm2.values =
        std::array<std::array<double, 3>, 3>{{
            {{10.0, 0.0, 0.0}},
            {{0.0, 20.0, 0.0}},
            {{0.0, 0.0, 30.0}},
        }};
    input.aero_identity = {kBodyFrameId, kClockDomain, 0, 11, 0, 1};
    input.aero_source_id = "aero.body";
    input.aero_lookup_model_id = kAeroLookupModelId;
    input.aero_table_id = kAeroTableId;
    input.aero_configuration_id = kAeroConfigurationId;
    input.reference_area_m2 = 1.0;
    input.reference_span_m = 1.0;
    input.reference_chord_m = 1.0;
    input.aero_query_rates_b_radps = {0.0, 0.0, 0.0};
    input.aero_surface_state_rad = {0.0, 0.0, 0.0, 0.0};
    input.mach_axis = {0.2, 0.6};
    input.alpha_axis_rad = {-0.1, 0.1};
    input.beta_axis_rad = {-0.05, 0.05};
    input.aero_coefficient_rows = {{
        {{0.006, 0.0245, -0.0795, 0.005, 0.014, -0.00755}},
        {{0.006, -0.0245, -0.0805, -0.005, 0.014, 0.00755}},
        {{0.05, 0.0245, 0.0795, 0.005, -0.106, -0.00785}},
        {{0.05, -0.0245, 0.0805, -0.005, -0.106, 0.00785}},
        {{0.018, 0.0235, -0.0795, 0.005, 0.022, -0.00795}},
        {{0.018, -0.0235, -0.0805, -0.005, 0.022, 0.00795}},
        {{0.07, 0.0235, 0.0795, 0.005, -0.098, -0.00825}},
        {{0.07, -0.0235, 0.0805, -0.005, -0.098, 0.00825}},
    }};
    input.r_body_origin_to_aero_application_b_m =
        {0.2, 0.0, -25.0 / 18.0};
    input.propulsion_identity = {
        kBodyFrameId, kClockDomain, 0, 11, 0, 1};
    input.propulsion_source_id = "propulsion.main";
    input.thrust_magnitude_n = 100.0;
    input.thrust_direction_b_unit = {1.0, 0.0, 0.0};
    input.r_body_origin_to_propulsion_application_b_m = {0.2, 0.2, 0.0};
    input.intrinsic_propulsion_moment_at_application_b_nm =
        {0.0, 0.0, 20.0};
    input.fuel_consumption_rate_kgps = 0.5;
    input.terminal_kind = "duration_exact_grid";
    input.terminal_tick = 1;
    return input;
}

struct AxisLocation {
    std::size_t index = 0;
    double weight = 0.0;
    bool boundary = false;
};

AxisLocation locateAxis(const std::array<double, 2>& axis,
                        double query, const std::string& label) {
    requireDomain(finite(query) && axis.front() <= query &&
                      query <= axis.back(),
                  label + " query is outside the inclusive table domain");
    if (query == axis.back()) {
        return {0, 1.0, true};
    }
    const double weight = (query - axis.front()) /
                          (axis.back() - axis.front());
    return {0, weight, query == axis.front()};
}

AeroLookup evaluateAeroLookup(const Input& input, double mach,
                              double alpha_rad, double beta_rad,
                              bool nearest) {
    const AxisLocation mach_location = locateAxis(
        input.mach_axis, mach, "Mach");
    const AxisLocation alpha_location = locateAxis(
        input.alpha_axis_rad, alpha_rad, "alpha");
    const AxisLocation beta_location = locateAxis(
        input.beta_axis_rad, beta_rad, "beta");
    const std::array<AxisLocation, 3> locations{
        mach_location, alpha_location, beta_location};
    std::array<double, 6> coefficients{};
    const auto rowIndex = [](std::size_t mach_index,
                             std::size_t alpha_index,
                             std::size_t beta_index) {
        return (mach_index * 2 + alpha_index) * 2 + beta_index;
    };
    if (nearest) {
        const std::size_t mach_index =
            mach_location.weight >= 0.5 ? 1 : 0;
        const std::size_t alpha_index =
            alpha_location.weight >= 0.5 ? 1 : 0;
        const std::size_t beta_index =
            beta_location.weight >= 0.5 ? 1 : 0;
        coefficients = input.aero_coefficient_rows[
            rowIndex(mach_index, alpha_index, beta_index)];
    } else {
        for (std::size_t mach_offset = 0; mach_offset < 2; ++mach_offset) {
            const double mach_factor = mach_offset == 0
                ? 1.0 - mach_location.weight : mach_location.weight;
            for (std::size_t alpha_offset = 0;
                 alpha_offset < 2; ++alpha_offset) {
                const double alpha_factor = alpha_offset == 0
                    ? 1.0 - alpha_location.weight : alpha_location.weight;
                for (std::size_t beta_offset = 0;
                     beta_offset < 2; ++beta_offset) {
                    const double beta_factor = beta_offset == 0
                        ? 1.0 - beta_location.weight : beta_location.weight;
                    const double factor =
                        mach_factor * alpha_factor * beta_factor;
                    const auto& row = input.aero_coefficient_rows[
                        rowIndex(mach_offset, alpha_offset, beta_offset)];
                    for (std::size_t coefficient = 0;
                         coefficient < coefficients.size(); ++coefficient) {
                        coefficients[coefficient] += factor * row[coefficient];
                    }
                }
            }
        }
    }
    for (double& coefficient : coefficients) {
        coefficient = canonicalZero(coefficient);
    }
    const bool boundary = std::any_of(
        locations.begin(), locations.end(),
        [](const AxisLocation& value) { return value.boundary; });
    return {
        input.aero_lookup_model_id,
        input.aero_table_id,
        input.aero_configuration_id,
        boundary ? "Boundary" : "Interior",
        {mach_location.index, alpha_location.index, beta_location.index},
        {mach_location.weight, alpha_location.weight, beta_location.weight},
        coefficients,
    };
}

RigidDerivative evaluateRigidDerivative(
    const State& state, double mass_kg, const Matrix3& inertia,
    const Vec3& force_b_n, const Vec3& moment_b_nm,
    const Vec3& gravity_i_mps2) {
    const Quaternion attitude = normalize(state.q_i_b);
    const Vec3 force_i = bodyToInertial(attitude, force_b_n);
    const Vec3 acceleration = add(scale(force_i, 1.0 / mass_kg),
                                  gravity_i_mps2);
    const Vec3 angular_momentum = multiply(inertia, state.omega_bi_b_radps);
    const Vec3 gyroscopic = cross(state.omega_bi_b_radps, angular_momentum);
    const Vec3 net_moment = subtract(moment_b_nm, gyroscopic);
    const Vec3 angular_acceleration = solveSpd(inertia, net_moment);
    const Quaternion pure_omega{
        0.0, state.omega_bi_b_radps.x, state.omega_bi_b_radps.y,
        state.omega_bi_b_radps.z};
    const Quaternion q_derivative =
        scale(hamilton(pure_omega, attitude), -0.5);
    return {force_i, acceleration, angular_momentum, gyroscopic,
            net_moment, angular_acceleration, q_derivative};
}

Derivative derivative(const State& state, double mass_kg,
                      const Matrix3& inertia, const Vec3& force_b_n,
                      const Vec3& moment_b_nm,
                      const Vec3& gravity_i_mps2) {
    const RigidDerivative rigid = evaluateRigidDerivative(
        state, mass_kg, inertia, force_b_n, moment_b_nm, gravity_i_mps2);
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

State rk4Step(const State& committed, double dt_s, double mass_kg,
              const Matrix3& inertia, const Vec3& force_b_n,
              const Vec3& moment_b_nm, const Vec3& gravity_i_mps2) {
    requireDomain(finite(dt_s) && dt_s > 0.0,
                  "RK4 dt must be positive and finite");
    const Derivative k1 = derivative(
        committed, mass_kg, inertia, force_b_n, moment_b_nm,
        gravity_i_mps2);
    const Derivative k2 = derivative(
        addScaled(committed, k1, 0.5 * dt_s), mass_kg, inertia,
        force_b_n, moment_b_nm, gravity_i_mps2);
    const Derivative k3 = derivative(
        addScaled(committed, k2, 0.5 * dt_s), mass_kg, inertia,
        force_b_n, moment_b_nm, gravity_i_mps2);
    const Derivative k4 = derivative(
        addScaled(committed, k3, dt_s), mass_kg, inertia,
        force_b_n, moment_b_nm, gravity_i_mps2);
    const Derivative combined{
        weighted(k1.position, k2.position, k3.position, k4.position),
        weighted(k1.velocity, k2.velocity, k3.velocity, k4.velocity),
        weighted(k1.attitude, k2.attitude, k3.attitude, k4.attitude),
        weighted(k1.angular_rate, k2.angular_rate, k3.angular_rate,
                 k4.angular_rate),
    };
    State candidate = addScaled(committed, combined, dt_s);
    candidate.q_i_b = normalize(candidate.q_i_b);
    return candidate;
}

Composition compose(const Input& input, const Options& options = {}) {
    validateInput(input);
    const Quaternion attitude = normalize(input.initial_state.q_i_b);
    const Vec3 velocity_relative_i =
        options.wind_mode == WindMode::Subtract
            ? subtract(input.initial_state.velocity_i_mps,
                       input.velocity_airmass_i_mps)
            : add(input.initial_state.velocity_i_mps,
                  input.velocity_airmass_i_mps);
    const Vec3 velocity_relative_b =
        inertialToBody(attitude, velocity_relative_i);
    const double airspeed = std::sqrt(dot(velocity_relative_b,
                                         velocity_relative_b));
    requireDomain(finite(airspeed) && airspeed > 0.0,
                  "composed fixture requires positive airspeed");
    const double horizontal = std::sqrt(
        velocity_relative_b.x * velocity_relative_b.x +
        velocity_relative_b.z * velocity_relative_b.z);
    const AirData air_data{
        velocity_relative_i,
        velocity_relative_b,
        airspeed,
        std::atan2(velocity_relative_b.z, velocity_relative_b.x),
        std::atan2(velocity_relative_b.y, horizontal),
        0.5 * input.density_kgpm3 * airspeed * airspeed,
        airspeed / input.speed_of_sound_mps,
    };
    const AeroLookup lookup = evaluateAeroLookup(
        input, air_data.mach, air_data.alpha_rad, air_data.beta_rad,
        options.nearest_aero_lookup);

    const double pressure_area =
        air_data.dynamic_pressure_pa * input.reference_area_m2;
    const Vec3 aero_force{
        -pressure_area * lookup.coefficients[0],
        pressure_area * lookup.coefficients[1],
        -pressure_area * lookup.coefficients[2],
    };
    const Vec3 aero_application_moment{
        pressure_area * input.reference_span_m * lookup.coefficients[3],
        pressure_area * input.reference_chord_m * lookup.coefficients[4],
        pressure_area * input.reference_span_m * lookup.coefficients[5],
    };
    const Vec3 aero_lever = subtract(
        input.r_body_origin_to_aero_application_b_m,
        input.r_body_origin_to_com_b_m);
    const Vec3 aero_transport = cross(aero_lever, aero_force);
    const Contribution aero{
        input.aero_source_id, aero_force, aero_lever,
        aero_application_moment, aero_transport,
        add(aero_application_moment, aero_transport),
    };

    const Vec3 propulsion_force = scale(
        input.thrust_direction_b_unit, input.thrust_magnitude_n);
    const Vec3 propulsion_lever = subtract(
        input.r_body_origin_to_propulsion_application_b_m,
        input.r_body_origin_to_com_b_m);
    const Vec3 propulsion_transport = cross(
        propulsion_lever, propulsion_force);
    const Vec3 propulsion_application_moment =
        options.pretransport_propulsion_moment
            ? add(input.intrinsic_propulsion_moment_at_application_b_nm,
                  propulsion_transport)
            : input.intrinsic_propulsion_moment_at_application_b_nm;
    const Contribution propulsion{
        input.propulsion_source_id, propulsion_force, propulsion_lever,
        propulsion_application_moment, propulsion_transport,
        add(propulsion_application_moment, propulsion_transport),
    };
    const Vec3 force_total = add(aero.force_b_n, propulsion.force_b_n);
    const Vec3 moment_total = add(
        aero.moment_about_com_b_nm, propulsion.moment_about_com_b_nm);
    const double consumed_mass = input.fuel_consumption_rate_kgps * input.dt_s;
    const double candidate_mass = input.mass_kg - consumed_mass;
    const double integration_mass = options.early_mass_candidate
        ? candidate_mass : input.mass_kg;
    const MassVisibility mass{
        input.mass_state_id, input.mass_kg, consumed_mass, candidate_mass,
        "candidate-only", integration_mass, input.valid_until_tick,
    };
    const RigidDerivative initial_derivative = evaluateRigidDerivative(
        input.initial_state, integration_mass,
        input.inertia_about_com_b_kgm2, force_total, moment_total,
        input.gravity_i_mps2);
    const State terminal_state = rk4Step(
        input.initial_state, input.dt_s, integration_mass,
        input.inertia_about_com_b_kgm2, force_total, moment_total,
        input.gravity_i_mps2);
    return {
        input.id,
        input.sample_tick,
        input.valid_from_tick,
        input.valid_until_tick,
        input.configuration_revision,
        input.dt_s,
        input.gravity_i_mps2,
        input.velocity_airmass_i_mps,
        input.density_kgpm3,
        input.speed_of_sound_mps,
        air_data,
        lookup,
        aero,
        propulsion,
        mass,
        force_total,
        moment_total,
        initial_derivative,
        {input.terminal_tick,
         static_cast<double>(input.terminal_tick) * input.dt_s,
         terminal_state,
         input.terminal_kind},
    };
}

void append(std::vector<double>& destination, const Vec3& value) {
    destination.push_back(value.x);
    destination.push_back(value.y);
    destination.push_back(value.z);
}

void append(std::vector<double>& destination,
            const std::array<double, 6>& value) {
    destination.insert(destination.end(), value.begin(), value.end());
}

std::vector<double> instantaneousVector(const Composition& value) {
    std::vector<double> result;
    append(result, value.air_data.velocity_relative_i_mps);
    append(result, value.air_data.velocity_relative_b_mps);
    result.push_back(value.air_data.airspeed_mps);
    result.push_back(value.air_data.alpha_rad);
    result.push_back(value.air_data.beta_rad);
    result.push_back(value.air_data.dynamic_pressure_pa);
    result.push_back(value.air_data.mach);
    append(result, value.aero_lookup.weights);
    append(result, value.aero_lookup.coefficients);
    append(result, value.aero.force_b_n);
    append(result, value.aero.moment_about_com_b_nm);
    append(result, value.propulsion.force_b_n);
    append(result, value.propulsion.moment_about_com_b_nm);
    result.push_back(value.mass.current_visible_mass_kg);
    result.push_back(value.mass.pending_mass_candidate_kg);
    result.push_back(value.mass.integration_mass_kg);
    append(result, value.force_total_b_n);
    append(result, value.moment_total_about_com_b_nm);
    append(result, value.derivative.acceleration_i_mps2);
    append(result, value.derivative.angular_acceleration_b_radps2);
    return result;
}

std::vector<double> physicalVector(const Composition& value) {
    std::vector<double> result = instantaneousVector(value);
    append(result, value.terminal.state.position_i_m);
    append(result, value.terminal.state.velocity_i_mps);
    append(result, value.terminal.state.omega_bi_b_radps);
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
        static_cast<void>(compose(input));
    } catch (const std::domain_error&) {
        rejected.push_back(id);
        return;
    }
    throw std::runtime_error("invalid FrozenInterval input survived: " + id);
}

ProbeResult runProbe() {
    const Input accepted_input = acceptedInput();
    const Composition accepted = compose(accepted_input);
    require(near(accepted.air_data.dynamic_pressure_pa, 6125.0) &&
                accepted.aero_lookup.domain_status == "Interior" &&
                near(accepted.aero_lookup.weights.x, 4.0 / 17.0) &&
                near(accepted.aero_lookup.weights.y, 0.5) &&
                near(accepted.aero_lookup.weights.z, 0.5) &&
                near(accepted.aero_lookup.coefficients[0], 27.0 / 850.0) &&
                near(accepted.aero_lookup.coefficients[4], -3.0 / 68.0) &&
                near(accepted.force_total_b_n.x, -3215.0 / 34.0) &&
                maxDifference(accepted.moment_total_about_com_b_nm,
                              {0.0, 0.0, 0.0}) <= kFormulaAbsolute &&
                near(accepted.mass.current_visible_mass_kg, 100.0) &&
                near(accepted.mass.pending_mass_candidate_kg, 99.95) &&
                near(accepted.terminal.state.position_i_m.x,
                     10.995272058823529) &&
                near(accepted.terminal.state.position_i_m.z, 999.95096675) &&
                near(accepted.terminal.state.velocity_i_mps.x,
                     109.90544117647059) &&
                near(accepted.terminal.state.velocity_i_mps.z, -0.980665),
            "accepted FrozenInterval result differs from analytic anchors");

    Input alternate_input = accepted_input;
    alternate_input.initial_state.q_i_b =
        scale(alternate_input.initial_state.q_i_b, -1.0);
    const Composition alternate = compose(alternate_input);
    const double sign_difference = maxDifference(
        physicalVector(accepted), physicalVector(alternate));
    require(sign_difference <= kFormulaAbsolute,
            "quaternion sign changed the physical result");
    const EquivalenceResult equivalence{
        "EQUIV-YYZ-FROZEN-INTERVAL-QUATERNION-SIGN",
        "passed",
        normalize(alternate_input.initial_state.q_i_b),
        0.0,
        sign_difference,
    };

    std::vector<std::string> invalid;
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-BODY-FRAME-MISMATCH",
        [](Input& value) {
            value.aero_identity.frame_id = "frame.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-CLOCK-MISMATCH",
        [](Input& value) {
            value.environment_identity.clock_domain =
                "clock.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-SAMPLE-TICK-MISMATCH",
        [](Input& value) { value.mass_identity.sample_tick = 1; });
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-INTERVAL-MISMATCH",
        [](Input& value) { value.propulsion_identity.valid_until_tick = 2; });
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-REVISION-MISMATCH",
        [](Input& value) {
            value.aero_identity.configuration_revision = 12;
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-LOOKUP-TABLE-ID",
        [](Input& value) {
            value.aero_table_id = "aero-table.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-LOOKUP-MACH-HIGH",
        [](Input& value) {
            value.initial_state.velocity_i_mps = {300.0, 0.0, 0.0};
        });
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-NONPOSITIVE-DT",
        [](Input& value) { value.dt_s = 0.0; });
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-NONPOSITIVE-MASS",
        [](Input& value) { value.mass_kg = 0.0; });
    expectDomainRejection(
        invalid, "INVALID-YYZ-FROZEN-INTERVAL-ZERO-QUATERNION",
        [](Input& value) {
            value.initial_state.q_i_b = {0.0, 0.0, 0.0, 0.0};
        });

    Options add_wind_options;
    add_wind_options.wind_mode = WindMode::Add;
    const Composition add_wind = compose(accepted_input, add_wind_options);
    Options early_mass_options;
    early_mass_options.early_mass_candidate = true;
    const Composition early_mass = compose(accepted_input, early_mass_options);
    Options pretransport_options;
    pretransport_options.pretransport_propulsion_moment = true;
    const Composition pretransported = compose(
        accepted_input, pretransport_options);
    Options nearest_lookup_options;
    nearest_lookup_options.nearest_aero_lookup = true;
    const Composition nearest_lookup = compose(
        accepted_input, nearest_lookup_options);
    std::vector<MutationResult> mutations{
        {
            "MUTATION-YYZ-FROZEN-INTERVAL-ADD-WIND",
            "rejected",
            MutationKind::AddWind,
            add_wind,
            maxDifference(instantaneousVector(accepted),
                          instantaneousVector(add_wind)),
        },
        {
            "MUTATION-YYZ-FROZEN-INTERVAL-EARLY-MASS-CANDIDATE",
            "rejected",
            MutationKind::EarlyMass,
            early_mass,
            maxDifference(physicalVector(accepted), physicalVector(early_mass)),
        },
        {
            "MUTATION-YYZ-FROZEN-INTERVAL-PRETRANSPORTED-PROPULSION-MOMENT",
            "rejected",
            MutationKind::PretransportedMoment,
            pretransported,
            maxDifference(instantaneousVector(accepted),
                          instantaneousVector(pretransported)),
        },
        {
            "MUTATION-YYZ-FROZEN-INTERVAL-NEAREST-AERO-LOOKUP",
            "rejected",
            MutationKind::NearestAeroLookup,
            nearest_lookup,
            maxDifference(instantaneousVector(accepted),
                          instantaneousVector(nearest_lookup)),
        },
    };
    require(near(mutations[0].max_abs_physical_difference, 2695.0) &&
                near(mutations[1].max_abs_physical_difference, 0.05) &&
                near(mutations[2].max_abs_physical_difference, 20.0) &&
                near(mutations[3].max_abs_physical_difference, 493.0625),
            "a FrozenInterval mutation matched the accepted result");
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

void writeCoefficients(const std::array<double, 6>& values) {
    std::cout << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeNumber(values[index]);
    }
    std::cout << ']';
}

void writeAeroLookup(const AeroLookup& value) {
    std::cout << "{\"model_id\":\"" << value.model_id
              << "\",\"table_id\":\"" << value.table_id
              << "\",\"configuration_id\":\"" << value.configuration_id
              << "\",\"domain_status\":\"" << value.domain_status
              << "\",\"cell_indices_M_alpha_beta\":["
              << value.cell_indices[0] << ',' << value.cell_indices[1]
              << ',' << value.cell_indices[2]
              << "],\"weights_M_alpha_beta\":";
    writeVec3(value.weights);
    std::cout << ",\"coefficients_CA_CY_CN_Cl_Cm_Cn\":";
    writeCoefficients(value.coefficients);
    std::cout << '}';
}

void writeContribution(const Contribution& value) {
    std::cout << "{\"source_id\":\"" << value.source_id
              << "\",\"force_B_N\":";
    writeVec3(value.force_b_n);
    std::cout << ",\"r_CoM_to_application_B_m\":";
    writeVec3(value.r_com_to_application_b_m);
    std::cout << ",\"moment_at_application_B_Nm\":";
    writeVec3(value.moment_at_application_b_nm);
    std::cout << ",\"transport_moment_B_Nm\":";
    writeVec3(value.transport_moment_b_nm);
    std::cout << ",\"moment_about_CoM_B_Nm\":";
    writeVec3(value.moment_about_com_b_nm);
    std::cout << '}';
}

void writeComposition(const Composition& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"context\":{\"sample_tick\":"
              << value.sample_tick
              << ",\"valid_from_tick\":" << value.valid_from_tick
              << ",\"valid_until_tick\":" << value.valid_until_tick
              << ",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"base_dt_s\":";
    writeNumber(value.dt_s);
    std::cout << "},\"environment_sample\":{\"gravity_I_mps2\":";
    writeVec3(value.gravity_i_mps2);
    std::cout << ",\"velocity_airmass_I_mps\":";
    writeVec3(value.velocity_airmass_i_mps);
    std::cout << ",\"density_kgpm3\":";
    writeNumber(value.density_kgpm3);
    std::cout << ",\"speed_of_sound_mps\":";
    writeNumber(value.speed_of_sound_mps);
    std::cout << "},\"air_data\":{\"velocity_relative_I_mps\":";
    writeVec3(value.air_data.velocity_relative_i_mps);
    std::cout << ",\"velocity_relative_B_mps\":";
    writeVec3(value.air_data.velocity_relative_b_mps);
    std::cout << ",\"airspeed_mps\":";
    writeNumber(value.air_data.airspeed_mps);
    std::cout << ",\"alpha_rad\":";
    writeNumber(value.air_data.alpha_rad);
    std::cout << ",\"beta_rad\":";
    writeNumber(value.air_data.beta_rad);
    std::cout << ",\"dynamic_pressure_Pa\":";
    writeNumber(value.air_data.dynamic_pressure_pa);
    std::cout << ",\"mach\":";
    writeNumber(value.air_data.mach);
    std::cout << "},\"aero_lookup\":";
    writeAeroLookup(value.aero_lookup);
    std::cout << ",\"aero_contribution\":";
    writeContribution(value.aero);
    std::cout << ",\"propulsion_contribution\":";
    writeContribution(value.propulsion);
    std::cout << ",\"mass_visibility\":{\"mass_state_id\":\""
              << value.mass.mass_state_id
              << "\",\"current_visible_mass_kg\":";
    writeNumber(value.mass.current_visible_mass_kg);
    std::cout << ",\"consumed_mass_kg\":";
    writeNumber(value.mass.consumed_mass_kg);
    std::cout << ",\"pending_mass_candidate_kg\":";
    writeNumber(value.mass.pending_mass_candidate_kg);
    std::cout << ",\"pending_visibility_before_commit\":\""
              << value.mass.pending_visibility_before_commit
              << "\",\"integration_mass_kg\":";
    writeNumber(value.mass.integration_mass_kg);
    std::cout << ",\"next_commit_tick\":"
              << value.mass.next_commit_tick
              << "},\"closure\":{\"force_total_B_N\":";
    writeVec3(value.force_total_b_n);
    std::cout << ",\"moment_total_about_CoM_B_Nm\":";
    writeVec3(value.moment_total_about_com_b_nm);
    std::cout << "},\"rigid_derivative_at_tick0\":{\"force_total_I_N\":";
    writeVec3(value.derivative.force_total_i_n);
    std::cout << ",\"acceleration_I_mps2\":";
    writeVec3(value.derivative.acceleration_i_mps2);
    std::cout << ",\"angular_momentum_B_kgm2ps\":";
    writeVec3(value.derivative.angular_momentum_b_kgm2ps);
    std::cout << ",\"gyroscopic_moment_B_Nm\":";
    writeVec3(value.derivative.gyroscopic_moment_b_nm);
    std::cout << ",\"net_moment_B_Nm\":";
    writeVec3(value.derivative.net_moment_b_nm);
    std::cout << ",\"angular_acceleration_B_radps2\":";
    writeVec3(value.derivative.angular_acceleration_b_radps2);
    std::cout << ",\"q_derivative_I_B_per_s\":";
    writeQuaternion(value.derivative.q_derivative_i_b_per_s);
    std::cout << "},\"analytic_terminal\":{\"tick\":"
              << value.terminal.tick << ",\"time_s\":";
    writeNumber(value.terminal.time_s);
    std::cout << ",\"position_I_m\":";
    writeVec3(value.terminal.state.position_i_m);
    std::cout << ",\"velocity_I_mps\":";
    writeVec3(value.terminal.state.velocity_i_mps);
    std::cout << ",\"q_I_B_wxyz\":";
    writeQuaternion(value.terminal.state.q_i_b);
    std::cout << ",\"omega_BI_B_radps\":";
    writeVec3(value.terminal.state.omega_bi_b_radps);
    std::cout << ",\"termination_kind\":\""
              << value.terminal.termination_kind << "\"}}";
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
    if (value.kind == MutationKind::AddWind) {
        std::cout << ",\"observed_velocity_relative_I_mps\":";
        writeVec3(value.observed.air_data.velocity_relative_i_mps);
        std::cout << ",\"observed_dynamic_pressure_Pa\":";
        writeNumber(value.observed.air_data.dynamic_pressure_pa);
        std::cout << ",\"observed_force_total_B_N\":";
        writeVec3(value.observed.force_total_b_n);
    } else if (value.kind == MutationKind::EarlyMass) {
        std::cout << ",\"observed_integration_mass_kg\":";
        writeNumber(value.observed.mass.integration_mass_kg);
        std::cout << ",\"observed_acceleration_I_mps2\":";
        writeVec3(value.observed.derivative.acceleration_i_mps2);
        std::cout << ",\"observed_terminal_position_I_m\":";
        writeVec3(value.observed.terminal.state.position_i_m);
        std::cout << ",\"observed_terminal_velocity_I_mps\":";
        writeVec3(value.observed.terminal.state.velocity_i_mps);
    } else if (value.kind == MutationKind::PretransportedMoment) {
        std::cout <<
            ",\"observed_propulsion_moment_at_application_B_Nm\":";
        writeVec3(value.observed.propulsion.moment_at_application_b_nm);
        std::cout << ",\"observed_moment_total_about_CoM_B_Nm\":";
        writeVec3(value.observed.moment_total_about_com_b_nm);
        std::cout << ",\"observed_angular_acceleration_B_radps2\":";
        writeVec3(value.observed.derivative.angular_acceleration_b_radps2);
    } else {
        std::cout <<
            ",\"observed_coefficients_CA_CY_CN_Cl_Cm_Cn\":";
        writeCoefficients(value.observed.aero_lookup.coefficients);
        std::cout << ",\"observed_force_total_B_N\":";
        writeVec3(value.observed.force_total_b_n);
        std::cout << ",\"observed_moment_total_about_CoM_B_Nm\":";
        writeVec3(value.observed.moment_total_about_com_b_nm);
    }
    std::cout << ",\"max_abs_physical_difference\":";
    writeNumber(value.max_abs_physical_difference);
    std::cout << '}';
}

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"model_id\":\"" << kModelId
              << "\",\"status\":\"passed\""
              << ",\"semantic_profile_status\":\""
              << kProfileStatus << "\",\"cases\":[";
    writeComposition(result.accepted);
    std::cout << "],\"equivalence_results\":[{\"id\":\""
              << result.equivalence.id << "\",\"status\":\""
              << result.equivalence.status
              << "\",\"alternate_q_I_B_wxyz\":";
    writeQuaternion(result.equivalence.alternate_q_i_b);
    std::cout << ",\"orientation_error_rad\":";
    writeNumber(result.equivalence.orientation_error_rad);
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
            "usage: gnc_yyz_frozen_interval_probe --self-check\n";
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

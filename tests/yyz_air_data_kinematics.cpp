#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId =
    "ORACLE-YYZ-AIR-DATA-KINEMATICS-001";
constexpr const char* kModelId =
    "MODEL-YYZ-AIR-DATA-KINEMATICS-001";
constexpr const char* kModelChoiceStatus = "accepted";
constexpr const char* kInertialFrameId =
    "frame.fixture.yyz.inertial-cartesian@1";
constexpr const char* kBodyFrameId = "frame.fixture.yyz.body@1";
constexpr const char* kBodyAxes = "x-forward_y-right_z-down";
constexpr const char* kClockDomain = "clock.fixture.yyz.simulation@1";
constexpr double kFormulaAbsolute = 2.0e-12;
constexpr double kFormulaRelative = 2.0e-12;
constexpr double kQuaternionNormAbsolute = 1.0e-12;
constexpr double kQuaternionNormRelative = 1.0e-12;

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

struct Context {
    std::string inertial_frame_id;
    std::string body_frame_id;
    std::string body_axes;
    std::uint64_t sample_tick = 0;
    std::string clock_domain;
};

struct VelocitySample {
    Vec3 value_i_mps;
    std::string frame_id;
    std::uint64_t sample_tick = 0;
    std::string clock_domain;
};

struct TruthSample {
    VelocitySample velocity;
    Quaternion q_i_b;
    std::string attitude_to_frame_id;
    std::string attitude_from_frame_id;
    std::uint64_t attitude_sample_tick = 0;
    std::string attitude_clock_domain;
};

struct AtmosphereSample {
    double density_kgpm3 = 0.0;
    double speed_of_sound_mps = 0.0;
    std::uint64_t sample_tick = 0;
    std::string clock_domain;
};

struct AirDataInput {
    std::string id;
    Context context;
    TruthSample truth;
    VelocitySample wind;
    AtmosphereSample atmosphere;
};

enum class WindMode {
    Subtract,
    AddMutation,
};

enum class RotationMode {
    QPureInverse,
    InversePureQMutation,
};

enum class AngleMode {
    Accepted,
    LegacyClampMutation,
};

enum class SoundSpeedMode {
    Accepted,
    FloorOneMutation,
};

struct FormulaOptions {
    WindMode wind = WindMode::Subtract;
    RotationMode rotation = RotationMode::QPureInverse;
    AngleMode angles = AngleMode::Accepted;
    SoundSpeedMode sound_speed = SoundSpeedMode::Accepted;
};

struct AirDataResult {
    std::string id;
    Context context;
    Vec3 vehicle_velocity_i_mps;
    Vec3 airmass_velocity_i_mps;
    Vec3 relative_velocity_i_mps;
    Quaternion q_i_b;
    double quaternion_norm = 0.0;
    Quaternion q_b_i;
    Quaternion q_times_relative_pure;
    Quaternion rotated_relative_pure;
    Vec3 relative_velocity_b_mps;
    double u_mps = 0.0;
    double v_mps = 0.0;
    double w_mps = 0.0;
    double speed_mps = 0.0;
    double horizontal_speed_uw_mps = 0.0;
    double alpha_rad = 0.0;
    double beta_rad = 0.0;
    double dynamic_pressure_pa = 0.0;
    double mach = 0.0;
};

struct ProbeResult {
    std::vector<AirDataResult> cases;
    std::vector<std::string> equivalence_checks;
    std::vector<std::string> invalid_input_rejections;
    std::vector<std::string> mutation_rejections;
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
    return finite(value.w) && finite(value.x) &&
        finite(value.y) && finite(value.z);
}

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Quaternion scale(const Quaternion& value, double factor) {
    return {
        value.w * factor,
        value.x * factor,
        value.y * factor,
        value.z * factor,
    };
}

double dot(const Quaternion& lhs, const Quaternion& rhs) {
    return lhs.w * rhs.w + lhs.x * rhs.x +
        lhs.y * rhs.y + lhs.z * rhs.z;
}

Quaternion hamilton(const Quaternion& lhs, const Quaternion& rhs) {
    return {
        lhs.w * rhs.w - lhs.x * rhs.x -
            lhs.y * rhs.y - lhs.z * rhs.z,
        lhs.w * rhs.x + lhs.x * rhs.w +
            lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z +
            lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y -
            lhs.y * rhs.x + lhs.z * rhs.w,
    };
}

Quaternion inverse(const Quaternion& value) {
    const double norm_squared = dot(value, value);
    requireDomain(finite(norm_squared) && norm_squared > 0.0,
                  "q_I_B must have nonzero finite norm");
    return {
        value.w / norm_squared,
        -value.x / norm_squared,
        -value.y / norm_squared,
        -value.z / norm_squared,
    };
}

Quaternion pure(const Vec3& value) {
    return {0.0, value.x, value.y, value.z};
}

Vec3 vectorPart(const Quaternion& value) {
    return {value.x, value.y, value.z};
}

double norm(const Quaternion& value) {
    return std::sqrt(dot(value, value));
}

bool near(double actual, double expected) {
    const double bound = kFormulaAbsolute + kFormulaRelative *
        std::max(std::abs(actual), std::abs(expected));
    return finite(actual) && finite(expected) &&
        std::abs(actual - expected) <= bound;
}

bool near(const Vec3& actual, const Vec3& expected) {
    return near(actual.x, expected.x) &&
        near(actual.y, expected.y) && near(actual.z, expected.z);
}

void validateIdentity(const AirDataInput& input) {
    const Context& context = input.context;
    requireDomain(context.inertial_frame_id == kInertialFrameId,
                  "air-data inertial frame differs");
    requireDomain(context.body_frame_id == kBodyFrameId,
                  "air-data body frame differs");
    requireDomain(context.body_axes == kBodyAxes,
                  "air-data body axes differ");
    requireDomain(context.clock_domain == kClockDomain,
                  "air-data clock domain differs");
    requireDomain(
        input.truth.velocity.frame_id == context.inertial_frame_id &&
            input.truth.velocity.sample_tick == context.sample_tick &&
            input.truth.velocity.clock_domain == context.clock_domain,
        "truth velocity identity differs from air-data context");
    requireDomain(
        input.truth.attitude_to_frame_id == context.inertial_frame_id &&
            input.truth.attitude_from_frame_id == context.body_frame_id &&
            input.truth.attitude_sample_tick == context.sample_tick &&
            input.truth.attitude_clock_domain == context.clock_domain,
        "truth attitude identity differs from air-data context");
    requireDomain(
        input.wind.frame_id == context.inertial_frame_id &&
            input.wind.sample_tick == context.sample_tick &&
            input.wind.clock_domain == context.clock_domain,
        "wind identity differs from air-data context");
    requireDomain(
        input.atmosphere.sample_tick == context.sample_tick &&
            input.atmosphere.clock_domain == context.clock_domain,
        "atmosphere identity differs from air-data context");
}

AirDataResult calculate(const AirDataInput& input,
                        const FormulaOptions& options = {}) {
    validateIdentity(input);
    requireDomain(finite(input.truth.velocity.value_i_mps),
                  "vehicle velocity contains a non-finite component");
    requireDomain(finite(input.wind.value_i_mps),
                  "air-mass velocity contains a non-finite component");
    requireDomain(finite(input.truth.q_i_b),
                  "q_I_B contains a non-finite coefficient");
    requireDomain(finite(input.atmosphere.density_kgpm3) &&
                      input.atmosphere.density_kgpm3 >= 0.0,
                  "air density must be finite and nonnegative");
    requireDomain(finite(input.atmosphere.speed_of_sound_mps) &&
                      input.atmosphere.speed_of_sound_mps > 0.0,
                  "speed of sound must be finite and positive");

    const double quaternion_norm = norm(input.truth.q_i_b);
    const double unit_bound = kQuaternionNormAbsolute +
        kQuaternionNormRelative * std::max(std::abs(quaternion_norm), 1.0);
    requireDomain(finite(quaternion_norm) && quaternion_norm > 0.0 &&
                      std::abs(quaternion_norm - 1.0) <= unit_bound,
                  "q_I_B is outside the Error normalization policy");
    const Quaternion q_b_i = inverse(input.truth.q_i_b);

    const Vec3 relative_i = options.wind == WindMode::Subtract
        ? subtract(input.truth.velocity.value_i_mps, input.wind.value_i_mps)
        : add(input.truth.velocity.value_i_mps, input.wind.value_i_mps);
    requireDomain(finite(relative_i),
                  "relative inertial velocity is non-finite");
    const Quaternion relative_pure = pure(relative_i);
    Quaternion left;
    Quaternion rotated;
    if (options.rotation == RotationMode::QPureInverse) {
        left = hamilton(input.truth.q_i_b, relative_pure);
        rotated = hamilton(left, q_b_i);
    } else {
        left = hamilton(q_b_i, relative_pure);
        rotated = hamilton(left, input.truth.q_i_b);
    }
    requireDomain(finite(left) && finite(rotated),
                  "air-data rotation produced a non-finite value");
    const Vec3 relative_b = vectorPart(rotated);

    const double horizontal = std::hypot(relative_b.x, relative_b.z);
    const double speed = std::hypot(horizontal, relative_b.y);
    requireDomain(finite(speed) && speed > 0.0,
                  "relative speed must be finite and positive");
    requireDomain(finite(horizontal) && horizontal > 0.0,
                  "u/w horizontal speed must be finite and positive");

    double alpha = 0.0;
    double beta = 0.0;
    if (options.angles == AngleMode::Accepted) {
        alpha = std::atan2(relative_b.z, relative_b.x);
        beta = std::atan2(relative_b.y, horizontal);
    } else {
        alpha = std::atan2(relative_b.z,
                           std::max(1.0e-9, relative_b.x));
        beta = std::atan2(relative_b.y,
                          std::max(1.0e-9, std::abs(relative_b.x)));
    }
    const double sound_speed =
        options.sound_speed == SoundSpeedMode::Accepted
        ? input.atmosphere.speed_of_sound_mps
        : std::max(1.0, input.atmosphere.speed_of_sound_mps);
    const double dynamic_pressure = 0.5 * input.atmosphere.density_kgpm3 *
        speed * speed;
    const double mach = speed / sound_speed;
    requireDomain(finite(alpha) && finite(beta) &&
                      finite(dynamic_pressure) && finite(mach),
                  "air-data formula produced a non-finite value");

    return {
        input.id,
        input.context,
        input.truth.velocity.value_i_mps,
        input.wind.value_i_mps,
        relative_i,
        input.truth.q_i_b,
        quaternion_norm,
        q_b_i,
        left,
        rotated,
        relative_b,
        relative_b.x,
        relative_b.y,
        relative_b.z,
        speed,
        horizontal,
        alpha,
        beta,
        dynamic_pressure,
        mach,
    };
}

Context makeContext(std::uint64_t tick) {
    return {kInertialFrameId, kBodyFrameId, kBodyAxes, tick, kClockDomain};
}

AirDataInput makeInput(const std::string& id, std::uint64_t tick,
                       const Vec3& vehicle, const Vec3& wind,
                       const Quaternion& q_i_b, double density,
                       double sound_speed) {
    const Context context = makeContext(tick);
    return {
        id,
        context,
        {
            {vehicle, kInertialFrameId, tick, kClockDomain},
            q_i_b,
            kInertialFrameId,
            kBodyFrameId,
            tick,
            kClockDomain,
        },
        {wind, kInertialFrameId, tick, kClockDomain},
        {density, sound_speed, tick, kClockDomain},
    };
}

std::vector<AirDataInput> caseInputs() {
    constexpr double kSqrtHalf = 0.70710678118654752440;
    return {
        makeInput("CASE-YYZ-AIR-DATA-WIND-SUBTRACTION", 12,
                  {210.0, 30.0, -10.0}, {10.0, -5.0, -10.0},
                  {1.0, 0.0, 0.0, 0.0}, 1.225, 340.0),
        makeInput("CASE-YYZ-AIR-DATA-PASSIVE-ROTATION", 21,
                  {25.0, -115.0, 35.0}, {5.0, 5.0, 5.0},
                  {kSqrtHalf, 0.0, 0.0, kSqrtHalf}, 1.0, 320.0),
        makeInput("CASE-YYZ-AIR-DATA-REARWARD-FLOW", 34,
                  {-100.0, 20.0, 10.0}, {0.0, 0.0, 0.0},
                  {1.0, 0.0, 0.0, 0.0}, 0.9, 300.0),
        makeInput("CASE-YYZ-AIR-DATA-TINY-POSITIVE-SPEED", 55,
                  {1.0e-12, 2.0e-12, -3.0e-12}, {0.0, 0.0, 0.0},
                  {1.0, 0.0, 0.0, 0.0}, 0.0, 1.0),
        makeInput("CASE-YYZ-AIR-DATA-SUB-ONE-SOUND-SPEED", 89,
                  {100.0, 0.0, 0.0}, {0.0, 0.0, 0.0},
                  {1.0, 0.0, 0.0, 0.0}, 1.0, 0.5),
    };
}

bool samePhysical(const AirDataResult& lhs, const AirDataResult& rhs) {
    return near(lhs.relative_velocity_i_mps, rhs.relative_velocity_i_mps) &&
        near(lhs.relative_velocity_b_mps, rhs.relative_velocity_b_mps) &&
        near(lhs.u_mps, rhs.u_mps) && near(lhs.v_mps, rhs.v_mps) &&
        near(lhs.w_mps, rhs.w_mps) && near(lhs.speed_mps, rhs.speed_mps) &&
        near(lhs.horizontal_speed_uw_mps, rhs.horizontal_speed_uw_mps) &&
        near(lhs.alpha_rad, rhs.alpha_rad) &&
        near(lhs.beta_rad, rhs.beta_rad) &&
        near(lhs.dynamic_pressure_pa, rhs.dynamic_pressure_pa) &&
        near(lhs.mach, rhs.mach);
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
                   const AirDataInput& input) {
    if (rejected([&] { static_cast<void>(calculate(input)); })) {
        result.invalid_input_rejections.push_back(identifier);
    }
}

ProbeResult runProbe() {
    ProbeResult result;
    const std::vector<AirDataInput> inputs = caseInputs();
    for (const AirDataInput& input : inputs) {
        result.cases.push_back(calculate(input));
    }
    require(result.cases.size() == 5,
            "air-data executable case coverage is incomplete");
    require(near(result.cases[0].relative_velocity_i_mps,
                 {200.0, 35.0, 0.0}) &&
                near(result.cases[0].dynamic_pressure_pa, 25250.3125),
            "wind-subtraction anchor differs");
    require(near(result.cases[1].relative_velocity_b_mps,
                 {120.0, 20.0, 30.0}) &&
                near(result.cases[1].speed_mps, std::sqrt(15700.0)),
            "passive-rotation anchor differs");
    require(result.cases[2].alpha_rad > 3.0 &&
                result.cases[2].beta_rad > 0.19,
            "rearward-flow quadrant anchor differs");
    require(result.cases[3].speed_mps > 0.0 &&
                result.cases[3].dynamic_pressure_pa == 0.0,
            "tiny-positive-speed anchor differs");
    require(near(result.cases[4].mach, 200.0),
            "sub-one sound-speed anchor differs");

    AirDataInput sign_input = inputs[1];
    sign_input.truth.q_i_b = scale(sign_input.truth.q_i_b, -1.0);
    if (samePhysical(result.cases[1], calculate(sign_input))) {
        result.equivalence_checks.push_back(
            "EQUIV-YYZ-AIR-DATA-QUATERNION-SIGN");
    }
    AirDataInput offset_input = inputs[0];
    const Vec3 offset{17.0, -23.0, 5.0};
    offset_input.truth.velocity.value_i_mps = add(
        offset_input.truth.velocity.value_i_mps, offset);
    offset_input.wind.value_i_mps = add(
        offset_input.wind.value_i_mps, offset);
    if (samePhysical(result.cases[0], calculate(offset_input))) {
        result.equivalence_checks.push_back(
            "EQUIV-YYZ-AIR-DATA-COMMON-INERTIAL-VELOCITY");
    }
    require(result.equivalence_checks.size() == 2,
            "an air-data equivalence check failed");

    AirDataInput invalid = inputs[0];
    invalid.wind.frame_id = "frame.other@1";
    recordInvalid(result, "INVALID-YYZ-AIR-DATA-FRAME-MISMATCH", invalid);

    invalid = inputs[0];
    ++invalid.atmosphere.sample_tick;
    recordInvalid(result,
                  "INVALID-YYZ-AIR-DATA-SAMPLE-TICK-MISMATCH", invalid);

    invalid = inputs[0];
    invalid.truth.velocity.value_i_mps.x =
        std::numeric_limits<double>::infinity();
    recordInvalid(result, "INVALID-YYZ-AIR-DATA-NONFINITE-VELOCITY",
                  invalid);

    invalid = inputs[0];
    invalid.truth.q_i_b = {0.0, 0.0, 0.0, 0.0};
    recordInvalid(result, "INVALID-YYZ-AIR-DATA-ZERO-QUATERNION", invalid);

    invalid = inputs[0];
    invalid.truth.q_i_b = {0.8, 0.0, 0.0, 0.0};
    recordInvalid(result, "INVALID-YYZ-AIR-DATA-NONUNIT-QUATERNION", invalid);

    invalid = inputs[0];
    invalid.atmosphere.density_kgpm3 = -0.1;
    recordInvalid(result, "INVALID-YYZ-AIR-DATA-NEGATIVE-DENSITY", invalid);

    invalid = inputs[0];
    invalid.atmosphere.speed_of_sound_mps = 0.0;
    recordInvalid(result,
                  "INVALID-YYZ-AIR-DATA-NONPOSITIVE-SOUND-SPEED", invalid);

    invalid = inputs[0];
    invalid.truth.velocity.value_i_mps = invalid.wind.value_i_mps;
    recordInvalid(result,
                  "INVALID-YYZ-AIR-DATA-ZERO-RELATIVE-VELOCITY", invalid);

    invalid = inputs[0];
    invalid.truth.velocity.value_i_mps = {0.0, 10.0, 0.0};
    invalid.wind.value_i_mps = {0.0, 0.0, 0.0};
    recordInvalid(result,
                  "INVALID-YYZ-AIR-DATA-PURE-LATERAL-VELOCITY", invalid);
    require(result.invalid_input_rejections.size() == 9,
            "an invalid air-data input was accepted");

    FormulaOptions mutation;
    mutation.wind = WindMode::AddMutation;
    if (!samePhysical(result.cases[0], calculate(inputs[0], mutation))) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-AIR-DATA-ADD-WIND");
    }
    mutation = {};
    mutation.rotation = RotationMode::InversePureQMutation;
    if (!samePhysical(result.cases[1], calculate(inputs[1], mutation))) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-AIR-DATA-REVERSE-QUATERNION-DIRECTION");
    }
    mutation = {};
    mutation.angles = AngleMode::LegacyClampMutation;
    if (!samePhysical(result.cases[2], calculate(inputs[2], mutation))) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-AIR-DATA-LEGACY-ANGLE-CLAMPS");
    }
    mutation = {};
    mutation.sound_speed = SoundSpeedMode::FloorOneMutation;
    if (!samePhysical(result.cases[4], calculate(inputs[4], mutation))) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-AIR-DATA-SOUND-SPEED-FLOOR");
    }
    require(result.mutation_rejections.size() == 4,
            "an air-data physical mutation matched the accepted result");
    return result;
}

void writeVec3(const Vec3& value) {
    std::cout << '[' << value.x << ',' << value.y << ',' << value.z << ']';
}

void writeQuaternion(const Quaternion& value) {
    std::cout << '[' << value.w << ',' << value.x << ','
              << value.y << ',' << value.z << ']';
}

void writeContext(const Context& context) {
    std::cout << "{\"inertial_frame_id\":\"" << context.inertial_frame_id
              << "\",\"body_frame_id\":\"" << context.body_frame_id
              << "\",\"body_axes\":\"" << context.body_axes
              << "\",\"sample_tick\":" << context.sample_tick
              << ",\"clock_domain\":\"" << context.clock_domain << "\"}";
}

void writeCase(const AirDataResult& value) {
    std::cout << "{\"id\":\"" << value.id << "\",\"context\":";
    writeContext(value.context);
    std::cout << ",\"vehicle_velocity_I_mps\":";
    writeVec3(value.vehicle_velocity_i_mps);
    std::cout << ",\"airmass_velocity_I_mps\":";
    writeVec3(value.airmass_velocity_i_mps);
    std::cout << ",\"relative_velocity_I_mps\":";
    writeVec3(value.relative_velocity_i_mps);
    std::cout << ",\"q_I_B_wxyz\":";
    writeQuaternion(value.q_i_b);
    std::cout << ",\"quaternion_norm\":" << value.quaternion_norm
              << ",\"q_B_I_wxyz\":";
    writeQuaternion(value.q_b_i);
    std::cout << ",\"q_times_relative_pure_wxyz\":";
    writeQuaternion(value.q_times_relative_pure);
    std::cout << ",\"rotated_relative_pure_wxyz\":";
    writeQuaternion(value.rotated_relative_pure);
    std::cout << ",\"relative_velocity_B_mps\":";
    writeVec3(value.relative_velocity_b_mps);
    std::cout << ",\"u_mps\":" << value.u_mps
              << ",\"v_mps\":" << value.v_mps
              << ",\"w_mps\":" << value.w_mps
              << ",\"speed_mps\":" << value.speed_mps
              << ",\"horizontal_speed_uw_mps\":"
              << value.horizontal_speed_uw_mps
              << ",\"alpha_rad\":" << value.alpha_rad
              << ",\"beta_rad\":" << value.beta_rad
              << ",\"dynamic_pressure_Pa\":"
              << value.dynamic_pressure_pa
              << ",\"mach\":" << value.mach << '}';
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
              << ",\"model_choice_status\":\""
              << kModelChoiceStatus << "\",\"cases\":[";
    for (std::size_t index = 0; index < result.cases.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeCase(result.cases[index]);
    }
    std::cout << "],\"equivalence_checks\":";
    writeStringList(result.equivalence_checks);
    std::cout << ",\"invalid_input_rejections\":";
    writeStringList(result.invalid_input_rejections);
    std::cout << ",\"mutation_rejections\":";
    writeStringList(result.mutation_rejections);
    std::cout << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr <<
            "usage: gnc_yyz_air_data_kinematics_probe --self-check\n";
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

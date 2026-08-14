#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kOracleId =
    "ORACLE-YYZ-UNIFORM-ENVIRONMENT-001";
constexpr const char* kModelId =
    "MODEL-YYZ-UNIFORM-ENVIRONMENT-001";
constexpr const char* kModelChoiceStatus = "accepted";
constexpr const char* kInertialFrameId =
    "frame.fixture.yyz.inertial-cartesian@1";
constexpr const char* kClockDomain =
    "clock.fixture.yyz.simulation@1";
constexpr const char* kQuality = "Valid";
constexpr double kFormulaAbsolute = 2.0e-12;
constexpr double kFormulaRelative = 2.0e-12;
constexpr double kLegacyDensityScaleHeightM = 7200.0;
constexpr double kLegacyReferenceRadiusM = 6378137.0;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct EnvironmentDefinition {
    std::string inertial_frame_id;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    Vec3 gravity_i_mps2;
    Vec3 airmass_velocity_i_mps;
    double density_kgpm3 = 0.0;
    double speed_of_sound_mps = 0.0;
};

struct EnvironmentQuery {
    Vec3 position_i_m;
    std::string position_frame_id;
    std::int64_t sample_tick = 0;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
};

struct ConsumerProbe {
    Vec3 vehicle_velocity_i_mps;
    Vec3 force_i_n;
    double mass_kg = 0.0;
    std::string inertial_frame_id;
    std::int64_t sample_tick = 0;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
};

struct EnvironmentInput {
    std::string id;
    EnvironmentDefinition definition;
    EnvironmentQuery query;
    ConsumerProbe consumer;
};

enum class DensityMode {
    Uniform,
    LegacyAltitudeDecayMutation,
};

enum class GravityMode {
    Uniform,
    LegacyInverseSquareMutation,
};

struct FormulaOptions {
    DensityMode density = DensityMode::Uniform;
    GravityMode gravity = GravityMode::Uniform;
};

struct EnvironmentResponse {
    std::string model_id;
    std::string quality;
    std::string inertial_frame_id;
    std::int64_t sample_tick = 0;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    Vec3 gravity_i_mps2;
    Vec3 airmass_velocity_i_mps;
    double density_kgpm3 = 0.0;
    double speed_of_sound_mps = 0.0;
};

struct ConsumerLink {
    std::string inertial_frame_id;
    std::int64_t sample_tick = 0;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    Vec3 vehicle_velocity_i_mps;
    Vec3 relative_velocity_i_mps;
    double speed_squared_m2ps2 = 0.0;
    double speed_mps = 0.0;
    double dynamic_pressure_pa = 0.0;
    double mach = 0.0;
    Vec3 force_i_n;
    double mass_kg = 0.0;
    double mass_reciprocal_per_kg = 0.0;
    Vec3 force_acceleration_i_mps2;
    Vec3 total_acceleration_i_mps2;
};

struct EnvironmentResult {
    std::string id;
    EnvironmentQuery query;
    EnvironmentResponse response;
    ConsumerLink consumer_link;
};

struct ProbeResult {
    std::vector<EnvironmentResult> cases;
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

double canonicalZero(double value) {
    return value == 0.0 ? 0.0 : value;
}

Vec3 canonicalZero(const Vec3& value) {
    return {
        canonicalZero(value.x),
        canonicalZero(value.y),
        canonicalZero(value.z),
    };
}

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero({
        lhs.x + rhs.x,
        lhs.y + rhs.y,
        lhs.z + rhs.z,
    });
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero({
        lhs.x - rhs.x,
        lhs.y - rhs.y,
        lhs.z - rhs.z,
    });
}

Vec3 scale(const Vec3& value, double factor) {
    return canonicalZero({
        value.x * factor,
        value.y * factor,
        value.z * factor,
    });
}

double dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
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

void validateInput(const EnvironmentInput& input) {
    const EnvironmentDefinition& definition = input.definition;
    const EnvironmentQuery& query = input.query;
    const ConsumerProbe& consumer = input.consumer;
    requireDomain(definition.inertial_frame_id == kInertialFrameId,
                  "environment definition inertial frame differs");
    requireDomain(definition.clock_domain == kClockDomain,
                  "environment definition clock domain differs");
    requireDomain(definition.configuration_revision >= 0,
                  "environment configuration revision must be nonnegative");
    requireDomain(finite(definition.gravity_i_mps2),
                  "environment gravity must be finite");
    requireDomain(finite(definition.airmass_velocity_i_mps),
                  "environment air-mass velocity must be finite");
    requireDomain(finite(definition.density_kgpm3) &&
                      definition.density_kgpm3 >= 0.0,
                  "environment density must be finite and nonnegative");
    requireDomain(finite(definition.speed_of_sound_mps) &&
                      definition.speed_of_sound_mps > 0.0,
                  "environment speed of sound must be finite and positive");

    requireDomain(finite(query.position_i_m),
                  "environment query position must be finite");
    requireDomain(query.position_frame_id == kInertialFrameId,
                  "environment query position frame differs");
    requireDomain(query.sample_tick >= 0,
                  "environment query sample tick must be nonnegative");
    requireDomain(query.clock_domain == definition.clock_domain,
                  "environment query clock domain differs");
    requireDomain(query.configuration_revision ==
                      definition.configuration_revision,
                  "environment query configuration revision differs");

    requireDomain(finite(consumer.vehicle_velocity_i_mps) &&
                      finite(consumer.force_i_n),
                  "environment consumer vectors must be finite");
    requireDomain(finite(consumer.mass_kg) && consumer.mass_kg > 0.0,
                  "environment consumer mass must be finite and positive");
    requireDomain(consumer.inertial_frame_id == kInertialFrameId,
                  "environment consumer inertial frame differs");
    requireDomain(consumer.sample_tick == query.sample_tick &&
                      consumer.clock_domain == query.clock_domain &&
                      consumer.configuration_revision ==
                          query.configuration_revision,
                  "environment consumer sample identity differs");
}

EnvironmentResult queryEnvironment(
    const EnvironmentInput& input,
    const FormulaOptions& options = {}) {
    validateInput(input);
    const double altitude_seed = std::max(0.0, input.query.position_i_m.z);
    double density = input.definition.density_kgpm3;
    if (options.density == DensityMode::LegacyAltitudeDecayMutation) {
        density *= std::exp(
            -altitude_seed / kLegacyDensityScaleHeightM);
    }
    Vec3 gravity = input.definition.gravity_i_mps2;
    if (options.gravity == GravityMode::LegacyInverseSquareMutation) {
        const double ratio = kLegacyReferenceRadiusM /
            (kLegacyReferenceRadiusM + altitude_seed);
        gravity = scale(gravity, ratio * ratio);
    }
    requireDomain(finite(density) && density >= 0.0 && finite(gravity),
                  "environment query produced an invalid response");

    const EnvironmentResponse response{
        kModelId,
        kQuality,
        kInertialFrameId,
        input.query.sample_tick,
        input.query.clock_domain,
        input.query.configuration_revision,
        gravity,
        input.definition.airmass_velocity_i_mps,
        canonicalZero(density),
        input.definition.speed_of_sound_mps,
    };

    const Vec3 relative_velocity = subtract(
        input.consumer.vehicle_velocity_i_mps,
        response.airmass_velocity_i_mps);
    const double speed_squared = canonicalZero(
        dot(relative_velocity, relative_velocity));
    const double speed = std::sqrt(speed_squared);
    requireDomain(finite(speed) && speed > 0.0,
                  "environment consumer link requires positive speed");
    const double dynamic_pressure = canonicalZero(
        0.5 * response.density_kgpm3 * speed_squared);
    const double mach = speed / response.speed_of_sound_mps;
    const double mass_reciprocal = 1.0 / input.consumer.mass_kg;
    const Vec3 force_acceleration = scale(
        input.consumer.force_i_n, mass_reciprocal);
    const Vec3 total_acceleration = add(force_acceleration, gravity);
    requireDomain(finite(dynamic_pressure) && finite(mach) &&
                      finite(mass_reciprocal) &&
                      finite(force_acceleration) &&
                      finite(total_acceleration),
                  "environment consumer link produced a non-finite value");
    const ConsumerLink consumer_link{
        kInertialFrameId,
        input.query.sample_tick,
        input.query.clock_domain,
        input.query.configuration_revision,
        input.consumer.vehicle_velocity_i_mps,
        relative_velocity,
        speed_squared,
        speed,
        dynamic_pressure,
        mach,
        input.consumer.force_i_n,
        input.consumer.mass_kg,
        mass_reciprocal,
        force_acceleration,
        total_acceleration,
    };
    return {input.id, input.query, response, consumer_link};
}

EnvironmentInput makeInput(
    const std::string& id,
    std::int64_t tick,
    std::int64_t revision,
    const Vec3& position_i_m,
    const Vec3& gravity_i_mps2,
    const Vec3& airmass_velocity_i_mps,
    double density_kgpm3,
    double speed_of_sound_mps,
    const Vec3& vehicle_velocity_i_mps,
    const Vec3& force_i_n,
    double mass_kg) {
    return {
        id,
        {
            kInertialFrameId,
            kClockDomain,
            revision,
            gravity_i_mps2,
            airmass_velocity_i_mps,
            density_kgpm3,
            speed_of_sound_mps,
        },
        {position_i_m, kInertialFrameId, tick, kClockDomain, revision},
        {
            vehicle_velocity_i_mps,
            force_i_n,
            mass_kg,
            kInertialFrameId,
            tick,
            kClockDomain,
            revision,
        },
    };
}

std::vector<EnvironmentInput> caseInputs() {
    return {
        makeInput(
            "CASE-YYZ-UNIFORM-ENVIRONMENT-CONSUMER-LINK",
            12, 3, {100.0, -20.0, 1000.0},
            {0.0, 0.0, -9.80665}, {5.0, 0.0, 0.0},
            1.225, 340.294, {205.0, 35.0, -10.0},
            {100.0, 0.0, 0.0}, 50.0),
        makeInput(
            "CASE-YYZ-UNIFORM-ENVIRONMENT-ZERO-DENSITY",
            33, 4, {-1000000.0, 2000000.0, -3000000.0},
            {0.1, -0.2, -9.7}, {-3.0, 4.0, 5.0},
            0.0, 0.5, {97.0, 4.0, 5.0},
            {10.0, -20.0, 30.0}, 20.0),
    };
}

EnvironmentInput highPositionInput(const EnvironmentInput& base) {
    EnvironmentInput moved = base;
    moved.query.position_i_m = {101.0, -22.0, 10000.0};
    moved.query.sample_tick = 112;
    moved.consumer.sample_tick = 112;
    return moved;
}

bool samePhysical(const EnvironmentResult& lhs,
                  const EnvironmentResult& rhs) {
    return near(lhs.response.gravity_i_mps2,
                rhs.response.gravity_i_mps2) &&
        near(lhs.response.airmass_velocity_i_mps,
             rhs.response.airmass_velocity_i_mps) &&
        near(lhs.response.density_kgpm3, rhs.response.density_kgpm3) &&
        near(lhs.response.speed_of_sound_mps,
             rhs.response.speed_of_sound_mps) &&
        near(lhs.consumer_link.vehicle_velocity_i_mps,
             rhs.consumer_link.vehicle_velocity_i_mps) &&
        near(lhs.consumer_link.relative_velocity_i_mps,
             rhs.consumer_link.relative_velocity_i_mps) &&
        near(lhs.consumer_link.speed_squared_m2ps2,
             rhs.consumer_link.speed_squared_m2ps2) &&
        near(lhs.consumer_link.speed_mps, rhs.consumer_link.speed_mps) &&
        near(lhs.consumer_link.dynamic_pressure_pa,
             rhs.consumer_link.dynamic_pressure_pa) &&
        near(lhs.consumer_link.mach, rhs.consumer_link.mach) &&
        near(lhs.consumer_link.force_i_n, rhs.consumer_link.force_i_n) &&
        near(lhs.consumer_link.mass_kg, rhs.consumer_link.mass_kg) &&
        near(lhs.consumer_link.mass_reciprocal_per_kg,
             rhs.consumer_link.mass_reciprocal_per_kg) &&
        near(lhs.consumer_link.force_acceleration_i_mps2,
             rhs.consumer_link.force_acceleration_i_mps2) &&
        near(lhs.consumer_link.total_acceleration_i_mps2,
             rhs.consumer_link.total_acceleration_i_mps2);
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
                   const EnvironmentInput& input) {
    if (rejected([&] { static_cast<void>(queryEnvironment(input)); })) {
        result.invalid_input_rejections.push_back(identifier);
    }
}

ProbeResult runProbe() {
    ProbeResult result;
    const std::vector<EnvironmentInput> inputs = caseInputs();
    for (const EnvironmentInput& input : inputs) {
        result.cases.push_back(queryEnvironment(input));
    }
    require(result.cases.size() == 2,
            "uniform-environment case coverage is incomplete");
    require(near(result.cases[0].response.gravity_i_mps2,
                 {0.0, 0.0, -9.80665}) &&
                near(result.cases[0].consumer_link.relative_velocity_i_mps,
                     {200.0, 35.0, -10.0}) &&
                near(result.cases[0].consumer_link.speed_squared_m2ps2,
                     41325.0) &&
                near(result.cases[0].consumer_link.dynamic_pressure_pa,
                     25311.5625) &&
                near(result.cases[0].consumer_link.total_acceleration_i_mps2,
                     {2.0, 0.0, -9.80665}),
            "uniform-environment consumer-link anchor differs");
    require(near(result.cases[1].consumer_link.relative_velocity_i_mps,
                 {100.0, 0.0, 0.0}) &&
                near(result.cases[1].consumer_link.dynamic_pressure_pa,
                     0.0) &&
                near(result.cases[1].consumer_link.mach, 200.0) &&
                near(result.cases[1].consumer_link.total_acceleration_i_mps2,
                     {0.6, -1.2, -8.2}),
            "zero-density environment anchor differs");

    const EnvironmentInput high = highPositionInput(inputs[0]);
    const EnvironmentResult high_result = queryEnvironment(high);
    if (samePhysical(result.cases[0], high_result)) {
        result.equivalence_checks.push_back(
            "EQUIV-YYZ-UNIFORM-ENVIRONMENT-POSITION-TICK");
    }
    require(result.equivalence_checks.size() == 1,
            "uniform-environment invariance failed");

    EnvironmentInput invalid = inputs[0];
    invalid.query.position_frame_id = "frame.other@1";
    recordInvalid(result,
                  "INVALID-YYZ-UNIFORM-ENVIRONMENT-FRAME-MISMATCH",
                  invalid);

    invalid = inputs[0];
    invalid.query.clock_domain = "clock.other@1";
    recordInvalid(result,
                  "INVALID-YYZ-UNIFORM-ENVIRONMENT-CLOCK-MISMATCH",
                  invalid);

    invalid = inputs[0];
    ++invalid.query.configuration_revision;
    recordInvalid(result,
                  "INVALID-YYZ-UNIFORM-ENVIRONMENT-REVISION-MISMATCH",
                  invalid);

    invalid = inputs[0];
    invalid.query.sample_tick = -1;
    recordInvalid(result,
                  "INVALID-YYZ-UNIFORM-ENVIRONMENT-NEGATIVE-TICK",
                  invalid);

    invalid = inputs[0];
    invalid.query.position_i_m.x =
        std::numeric_limits<double>::infinity();
    recordInvalid(result,
                  "INVALID-YYZ-UNIFORM-ENVIRONMENT-NONFINITE-POSITION",
                  invalid);

    invalid = inputs[0];
    invalid.definition.gravity_i_mps2.x =
        std::numeric_limits<double>::infinity();
    recordInvalid(result,
                  "INVALID-YYZ-UNIFORM-ENVIRONMENT-NONFINITE-GRAVITY",
                  invalid);

    invalid = inputs[0];
    invalid.definition.airmass_velocity_i_mps.x =
        std::numeric_limits<double>::infinity();
    recordInvalid(result,
                  "INVALID-YYZ-UNIFORM-ENVIRONMENT-NONFINITE-WIND",
                  invalid);

    invalid = inputs[0];
    invalid.definition.density_kgpm3 = -0.1;
    recordInvalid(result,
                  "INVALID-YYZ-UNIFORM-ENVIRONMENT-NEGATIVE-DENSITY",
                  invalid);

    invalid = inputs[0];
    invalid.definition.speed_of_sound_mps = 0.0;
    recordInvalid(
        result,
        "INVALID-YYZ-UNIFORM-ENVIRONMENT-NONPOSITIVE-SOUND-SPEED",
        invalid);
    require(result.invalid_input_rejections.size() == 9,
            "an invalid uniform-environment input was accepted");

    FormulaOptions mutation;
    mutation.density = DensityMode::LegacyAltitudeDecayMutation;
    if (!samePhysical(high_result, queryEnvironment(high, mutation))) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-UNIFORM-ENVIRONMENT-LEGACY-DENSITY-DECAY");
    }
    mutation = {};
    mutation.gravity = GravityMode::LegacyInverseSquareMutation;
    if (!samePhysical(high_result, queryEnvironment(high, mutation))) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-UNIFORM-ENVIRONMENT-LEGACY-GRAVITY-DECAY");
    }
    require(result.mutation_rejections.size() == 2,
            "an environment physical mutation matched the accepted result");
    return result;
}

void writeVec3(const Vec3& value) {
    std::cout << '[' << value.x << ',' << value.y << ',' << value.z << ']';
}

void writeQuery(const EnvironmentQuery& value) {
    std::cout << "{\"position_I_m\":";
    writeVec3(value.position_i_m);
    std::cout << ",\"position_frame_id\":\""
              << value.position_frame_id
              << "\",\"sample_tick\":" << value.sample_tick
              << ",\"clock_domain\":\"" << value.clock_domain
              << "\",\"configuration_revision\":"
              << value.configuration_revision << '}';
}

void writeResponse(const EnvironmentResponse& value) {
    std::cout << "{\"model_id\":\"" << value.model_id
              << "\",\"quality\":\"" << value.quality
              << "\",\"inertial_frame_id\":\""
              << value.inertial_frame_id
              << "\",\"sample_tick\":" << value.sample_tick
              << ",\"clock_domain\":\"" << value.clock_domain
              << "\",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"gravity_I_mps2\":";
    writeVec3(value.gravity_i_mps2);
    std::cout << ",\"airmass_velocity_I_mps\":";
    writeVec3(value.airmass_velocity_i_mps);
    std::cout << ",\"density_kgpm3\":" << value.density_kgpm3
              << ",\"speed_of_sound_mps\":"
              << value.speed_of_sound_mps << '}';
}

void writeConsumerLink(const ConsumerLink& value) {
    std::cout << "{\"inertial_frame_id\":\""
              << value.inertial_frame_id
              << "\",\"sample_tick\":" << value.sample_tick
              << ",\"clock_domain\":\"" << value.clock_domain
              << "\",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"vehicle_velocity_I_mps\":";
    writeVec3(value.vehicle_velocity_i_mps);
    std::cout << ",\"relative_velocity_I_mps\":";
    writeVec3(value.relative_velocity_i_mps);
    std::cout << ",\"speed_squared_m2ps2\":"
              << value.speed_squared_m2ps2
              << ",\"speed_mps\":" << value.speed_mps
              << ",\"dynamic_pressure_Pa\":"
              << value.dynamic_pressure_pa
              << ",\"mach\":" << value.mach
              << ",\"force_I_N\":";
    writeVec3(value.force_i_n);
    std::cout << ",\"mass_kg\":" << value.mass_kg
              << ",\"mass_reciprocal_per_kg\":"
              << value.mass_reciprocal_per_kg
              << ",\"force_acceleration_I_mps2\":";
    writeVec3(value.force_acceleration_i_mps2);
    std::cout << ",\"total_acceleration_I_mps2\":";
    writeVec3(value.total_acceleration_i_mps2);
    std::cout << '}';
}

void writeCase(const EnvironmentResult& value) {
    std::cout << "{\"id\":\"" << value.id << "\",\"query\":";
    writeQuery(value.query);
    std::cout << ",\"response\":";
    writeResponse(value.response);
    std::cout << ",\"consumer_link\":";
    writeConsumerLink(value.consumer_link);
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
            "usage: gnc_yyz_uniform_environment_probe --self-check\n";
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

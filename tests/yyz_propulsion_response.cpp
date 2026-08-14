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
    "ORACLE-YYZ-PROPULSION-RESPONSE-001";
constexpr const char* kModelId =
    "MODEL-YYZ-PROPULSION-RESPONSE-001";
constexpr const char* kModelChoiceStatus = "accepted";
constexpr const char* kSourceId = "propulsion.main";
constexpr const char* kBodyFrameId = "frame.fixture.yyz.body@1";
constexpr const char* kClockDomain = "clock.fixture.yyz.simulation@1";
constexpr const char* kMassStateId = "mass.fixture.yyz.vehicle@1";
constexpr const char* kQuality = "Valid";
constexpr double kFormulaAbsolute = 2.0e-12;
constexpr double kFormulaRelative = 2.0e-12;
constexpr double kDirectionAbsolute = 1.0e-12;
constexpr double kDirectionRelative = 1.0e-12;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Context {
    std::string source_id;
    std::string body_frame_id;
    std::string clock_domain;
    std::int64_t sample_tick = 0;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    double base_dt_s = 0.0;
};

struct SuppliedResponse {
    double thrust_magnitude_n = 0.0;
    Vec3 thrust_direction_b_unit;
    Vec3 r_com_to_application_b_m;
    Vec3 intrinsic_moment_at_application_b_nm;
    double fuel_consumption_rate_kgps = 0.0;
};

struct MassConsumerInput {
    std::string mass_state_id;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    double committed_mass_kg = 0.0;
};

struct PropulsionInput {
    std::string id;
    Context context;
    SuppliedResponse supplied;
    MassConsumerInput mass_consumer;
};

struct FormulaOptions {
    bool reverse_thrust = false;
    bool pretransport_moment = false;
    bool mass_gain = false;
};

struct PropulsionResponse {
    std::string model_id;
    std::string source_id;
    std::string quality;
    std::string body_frame_id;
    std::int64_t sample_tick = 0;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    Vec3 force_b_n;
    Vec3 r_com_to_application_b_m;
    Vec3 moment_at_application_b_nm;
    double fuel_consumption_rate_kgps = 0.0;
};

struct ClosureConsumer {
    std::string source_id;
    std::string body_frame_id;
    std::int64_t sample_tick = 0;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    Vec3 force_b_n;
    Vec3 moment_at_application_b_nm;
    Vec3 lever_arm_moment_b_nm;
    Vec3 moment_about_com_b_nm;
};

struct MassConsumerResult {
    std::string mass_state_id;
    std::string clock_domain;
    std::int64_t configuration_revision = 0;
    std::int64_t valid_from_tick = 0;
    std::int64_t valid_until_tick = 0;
    double interval_duration_s = 0.0;
    double fuel_consumption_rate_kgps = 0.0;
    double consumed_fuel_mass_kg = 0.0;
    double mass_delta_kg = 0.0;
    double committed_mass_kg = 0.0;
    double mass_candidate_kg = 0.0;
};

struct PropulsionResult {
    std::string id;
    PropulsionResponse response;
    ClosureConsumer closure_consumer;
    MassConsumerResult mass_consumer;
};

struct EquivalenceResult {
    std::string id;
    std::string status;
    double force_and_response_max_abs_difference = 0.0;
    double application_wrench_max_abs_difference = 0.0;
    double summed_consumed_fuel_mass_kg = 0.0;
    double consumed_fuel_mass_difference_kg = 0.0;
    double sequential_final_mass_candidate_kg = 0.0;
    double final_mass_candidate_difference_kg = 0.0;
};

enum class MutationKind {
    ReversedThrust,
    PretransportedMoment,
    MassGain,
};

struct MutationResult {
    std::string id;
    std::string status;
    double max_abs_physical_difference = 0.0;
    MutationKind kind = MutationKind::ReversedThrust;
    Vec3 first_observed_vector;
    Vec3 second_observed_vector;
    double observed_mass_delta_kg = 0.0;
    double observed_mass_candidate_kg = 0.0;
};

struct ProbeResult {
    std::vector<PropulsionResult> cases;
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

Vec3 scale(const Vec3& value, double factor) {
    return canonicalZero({
        value.x * factor,
        value.y * factor,
        value.z * factor,
    });
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero({
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
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

double maxAbsDifference(const Vec3& lhs, const Vec3& rhs) {
    return std::max({std::abs(lhs.x - rhs.x),
                     std::abs(lhs.y - rhs.y),
                     std::abs(lhs.z - rhs.z)});
}

void validateInput(const PropulsionInput& input) {
    const Context& context = input.context;
    const SuppliedResponse& supplied = input.supplied;
    const MassConsumerInput& mass = input.mass_consumer;
    requireDomain(context.source_id == kSourceId,
                  "propulsion source identity differs");
    requireDomain(context.body_frame_id == kBodyFrameId,
                  "propulsion body frame differs");
    requireDomain(context.clock_domain == kClockDomain,
                  "propulsion clock domain differs");
    requireDomain(context.sample_tick >= 0,
                  "propulsion sample tick must be nonnegative");
    requireDomain(context.configuration_revision >= 0,
                  "propulsion configuration revision must be nonnegative");
    requireDomain(context.valid_from_tick >= 0 &&
                      context.valid_until_tick >= 0,
                  "propulsion interval ticks must be nonnegative");
    requireDomain(context.sample_tick == context.valid_from_tick,
                  "propulsion sample tick must equal interval start");
    requireDomain(context.valid_until_tick > context.valid_from_tick,
                  "propulsion interval must be nonempty");
    requireDomain(finite(context.base_dt_s) && context.base_dt_s > 0.0,
                  "propulsion base dt must be finite and positive");

    requireDomain(finite(supplied.thrust_magnitude_n) &&
                      supplied.thrust_magnitude_n >= 0.0,
                  "propulsion thrust magnitude must be nonnegative");
    requireDomain(finite(supplied.thrust_direction_b_unit) &&
                      finite(supplied.r_com_to_application_b_m) &&
                      finite(supplied.intrinsic_moment_at_application_b_nm),
                  "propulsion supplied vectors must be finite");
    const double direction_norm =
        std::sqrt(dot(supplied.thrust_direction_b_unit,
                      supplied.thrust_direction_b_unit));
    const double direction_bound = kDirectionAbsolute +
        kDirectionRelative * std::max(std::abs(direction_norm), 1.0);
    requireDomain(finite(direction_norm) &&
                      std::abs(direction_norm - 1.0) <= direction_bound,
                  "propulsion thrust direction must be unit length");
    requireDomain(finite(supplied.fuel_consumption_rate_kgps) &&
                      supplied.fuel_consumption_rate_kgps >= 0.0,
                  "propulsion fuel consumption must be nonnegative");

    requireDomain(mass.mass_state_id == kMassStateId,
                  "propulsion mass state identity differs");
    requireDomain(mass.clock_domain == context.clock_domain &&
                      mass.configuration_revision ==
                          context.configuration_revision &&
                      mass.valid_from_tick == context.valid_from_tick &&
                      mass.valid_until_tick == context.valid_until_tick,
                  "propulsion mass consumer identity differs");
    requireDomain(finite(mass.committed_mass_kg) &&
                      mass.committed_mass_kg > 0.0,
                  "propulsion committed mass must be positive");
}

PropulsionResult evaluate(const PropulsionInput& input,
                          const FormulaOptions& options = {}) {
    validateInput(input);
    const Context& context = input.context;
    const SuppliedResponse& supplied = input.supplied;
    const MassConsumerInput& mass = input.mass_consumer;

    const double thrust_factor = options.reverse_thrust ?
        -supplied.thrust_magnitude_n : supplied.thrust_magnitude_n;
    const Vec3 force = scale(supplied.thrust_direction_b_unit,
                             thrust_factor);
    const Vec3 lever_arm_moment =
        cross(supplied.r_com_to_application_b_m, force);
    const Vec3 closure_input_moment = options.pretransport_moment ?
        add(supplied.intrinsic_moment_at_application_b_nm,
            lever_arm_moment) :
        supplied.intrinsic_moment_at_application_b_nm;
    const Vec3 moment_about_com =
        add(closure_input_moment, lever_arm_moment);

    const double duration = canonicalZero(
        static_cast<double>(context.valid_until_tick -
                            context.valid_from_tick) * context.base_dt_s);
    const double consumed_mass = canonicalZero(
        supplied.fuel_consumption_rate_kgps * duration);
    const double mass_delta = canonicalZero(
        options.mass_gain ? consumed_mass : -consumed_mass);
    const double candidate_mass = canonicalZero(
        mass.committed_mass_kg + mass_delta);
    requireDomain(finite(force) && finite(lever_arm_moment) &&
                      finite(closure_input_moment) &&
                      finite(moment_about_com) && finite(duration) &&
                      finite(consumed_mass) && finite(mass_delta) &&
                      finite(candidate_mass),
                  "propulsion response produced a non-finite value");
    requireDomain(candidate_mass > 0.0,
                  "propulsion mass candidate must be positive");

    return {
        input.id,
        {
            kModelId,
            kSourceId,
            kQuality,
            kBodyFrameId,
            context.sample_tick,
            kClockDomain,
            context.configuration_revision,
            context.valid_from_tick,
            context.valid_until_tick,
            force,
            supplied.r_com_to_application_b_m,
            supplied.intrinsic_moment_at_application_b_nm,
            supplied.fuel_consumption_rate_kgps,
        },
        {
            kSourceId,
            kBodyFrameId,
            context.sample_tick,
            kClockDomain,
            context.configuration_revision,
            force,
            closure_input_moment,
            lever_arm_moment,
            moment_about_com,
        },
        {
            kMassStateId,
            kClockDomain,
            context.configuration_revision,
            context.valid_from_tick,
            context.valid_until_tick,
            duration,
            supplied.fuel_consumption_rate_kgps,
            consumed_mass,
            mass_delta,
            mass.committed_mass_kg,
            candidate_mass,
        },
    };
}

PropulsionInput makeInput(const std::string& id, std::int64_t tick,
                          std::int64_t until_tick,
                          std::int64_t revision, double base_dt_s,
                          double thrust_magnitude_n,
                          const Vec3& thrust_direction_b_unit,
                          const Vec3& radius_b_m,
                          const Vec3& intrinsic_moment_b_nm,
                          double consumption_rate_kgps,
                          double committed_mass_kg) {
    return {
        id,
        {
            kSourceId,
            kBodyFrameId,
            kClockDomain,
            tick,
            revision,
            tick,
            until_tick,
            base_dt_s,
        },
        {
            thrust_magnitude_n,
            thrust_direction_b_unit,
            radius_b_m,
            intrinsic_moment_b_nm,
            consumption_rate_kgps,
        },
        {
            kMassStateId,
            kClockDomain,
            revision,
            tick,
            until_tick,
            committed_mass_kg,
        },
    };
}

std::vector<PropulsionInput> caseInputs() {
    return {
        makeInput(
            "CASE-YYZ-PROPULSION-OFF-AXIS-CONSUMERS",
            20, 25, 8, 0.1, 500.0, {0.6, 0.8, 0.0},
            {-0.5, 0.25, 0.1}, {1.0, -2.0, 3.0}, 0.5, 120.0),
        makeInput(
            "CASE-YYZ-PROPULSION-THREE-DIMENSIONAL-WRENCH",
            3, 7, 9, 0.25, 50.0, {0.8, 0.0, 0.6},
            {0.2, -0.1, 0.3}, {-4.0, 5.0, -6.0}, 0.0, 10.0),
        makeInput(
            "CASE-YYZ-PROPULSION-ZERO-RESPONSE",
            100, 101, 10, 0.02, 0.0, {1.0, 0.0, 0.0},
            {-2.0, 3.0, -4.0}, {0.0, 0.0, 0.0}, 0.0, 5.0),
    };
}

std::vector<double> physicalValues(const PropulsionResult& value) {
    std::vector<double> values;
    const auto append_vector = [&values](const Vec3& vector) {
        values.push_back(vector.x);
        values.push_back(vector.y);
        values.push_back(vector.z);
    };
    append_vector(value.response.force_b_n);
    append_vector(value.response.r_com_to_application_b_m);
    append_vector(value.response.moment_at_application_b_nm);
    values.push_back(value.response.fuel_consumption_rate_kgps);
    append_vector(value.closure_consumer.force_b_n);
    append_vector(value.closure_consumer.moment_at_application_b_nm);
    append_vector(value.closure_consumer.lever_arm_moment_b_nm);
    append_vector(value.closure_consumer.moment_about_com_b_nm);
    values.push_back(value.mass_consumer.interval_duration_s);
    values.push_back(value.mass_consumer.fuel_consumption_rate_kgps);
    values.push_back(value.mass_consumer.consumed_fuel_mass_kg);
    values.push_back(value.mass_consumer.mass_delta_kg);
    values.push_back(value.mass_consumer.committed_mass_kg);
    values.push_back(value.mass_consumer.mass_candidate_kg);
    return values;
}

double maxPhysicalDifference(const PropulsionResult& lhs,
                             const PropulsionResult& rhs) {
    const std::vector<double> lhs_values = physicalValues(lhs);
    const std::vector<double> rhs_values = physicalValues(rhs);
    require(lhs_values.size() == rhs_values.size(),
            "propulsion physical vectors differ in length");
    double maximum = 0.0;
    for (std::size_t index = 0; index < lhs_values.size(); ++index) {
        maximum = std::max(maximum,
                           std::abs(lhs_values[index] - rhs_values[index]));
    }
    return maximum;
}

EquivalenceResult intervalPartition(const PropulsionInput& base) {
    const PropulsionResult whole = evaluate(base);
    PropulsionInput first_input = base;
    first_input.context.valid_until_tick = 22;
    first_input.mass_consumer.valid_until_tick = 22;
    const PropulsionResult first = evaluate(first_input);

    PropulsionInput second_input = base;
    second_input.context.sample_tick = 22;
    second_input.context.valid_from_tick = 22;
    second_input.mass_consumer.valid_from_tick = 22;
    second_input.mass_consumer.committed_mass_kg =
        first.mass_consumer.mass_candidate_kg;
    const PropulsionResult second = evaluate(second_input);

    double response_difference = 0.0;
    double wrench_difference = 0.0;
    for (const PropulsionResult* part : {&first, &second}) {
        response_difference = std::max({
            response_difference,
            maxAbsDifference(whole.response.force_b_n,
                             part->response.force_b_n),
            maxAbsDifference(whole.response.r_com_to_application_b_m,
                             part->response.r_com_to_application_b_m),
            maxAbsDifference(whole.response.moment_at_application_b_nm,
                             part->response.moment_at_application_b_nm),
        });
        wrench_difference = std::max({
            wrench_difference,
            maxAbsDifference(whole.closure_consumer.force_b_n,
                             part->closure_consumer.force_b_n),
            maxAbsDifference(
                whole.closure_consumer.moment_at_application_b_nm,
                part->closure_consumer.moment_at_application_b_nm),
            maxAbsDifference(whole.closure_consumer.lever_arm_moment_b_nm,
                             part->closure_consumer.lever_arm_moment_b_nm),
            maxAbsDifference(whole.closure_consumer.moment_about_com_b_nm,
                             part->closure_consumer.moment_about_com_b_nm),
        });
    }
    const double summed_consumption = canonicalZero(
        first.mass_consumer.consumed_fuel_mass_kg +
        second.mass_consumer.consumed_fuel_mass_kg);
    const double consumption_difference = std::abs(
        summed_consumption -
        whole.mass_consumer.consumed_fuel_mass_kg);
    const double final_mass_difference = std::abs(
        second.mass_consumer.mass_candidate_kg -
        whole.mass_consumer.mass_candidate_kg);
    const double maximum = std::max({response_difference,
                                     wrench_difference,
                                     consumption_difference,
                                     final_mass_difference});
    return {
        "EQUIV-YYZ-PROPULSION-MASS-INTERVAL-PARTITION",
        maximum <= kFormulaAbsolute ? "passed" : "failed",
        response_difference,
        wrench_difference,
        summed_consumption,
        consumption_difference,
        second.mass_consumer.mass_candidate_kg,
        final_mass_difference,
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
                   const PropulsionInput& input) {
    if (rejected([&] { static_cast<void>(evaluate(input)); })) {
        result.invalid_input_rejections.push_back(identifier);
    }
}

MutationResult makeMutation(const std::string& id, MutationKind kind,
                            const PropulsionResult& accepted,
                            const PropulsionResult& mutated) {
    const double difference = maxPhysicalDifference(accepted, mutated);
    MutationResult result;
    result.id = id;
    result.status = difference > kFormulaAbsolute ? "rejected" : "matched";
    result.max_abs_physical_difference = difference;
    result.kind = kind;
    if (kind == MutationKind::ReversedThrust) {
        result.first_observed_vector = mutated.response.force_b_n;
    } else if (kind == MutationKind::PretransportedMoment) {
        result.first_observed_vector =
            mutated.closure_consumer.moment_at_application_b_nm;
        result.second_observed_vector =
            mutated.closure_consumer.moment_about_com_b_nm;
    } else {
        result.observed_mass_delta_kg =
            mutated.mass_consumer.mass_delta_kg;
        result.observed_mass_candidate_kg =
            mutated.mass_consumer.mass_candidate_kg;
    }
    return result;
}

ProbeResult runProbe() {
    ProbeResult result;
    const std::vector<PropulsionInput> inputs = caseInputs();
    for (const PropulsionInput& input : inputs) {
        result.cases.push_back(evaluate(input));
    }
    require(result.cases.size() == 3,
            "propulsion-response case coverage is incomplete");
    require(near(result.cases[0].response.force_b_n,
                 {300.0, 400.0, 0.0}) &&
                near(result.cases[0].closure_consumer.lever_arm_moment_b_nm,
                     {-40.0, 30.0, -275.0}) &&
                near(result.cases[0].closure_consumer.moment_about_com_b_nm,
                     {-39.0, 28.0, -272.0}) &&
                near(result.cases[0].mass_consumer.consumed_fuel_mass_kg,
                     0.25) &&
                near(result.cases[0].mass_consumer.mass_candidate_kg,
                     119.75),
            "off-axis propulsion consumer anchor differs");
    require(near(result.cases[1].response.force_b_n,
                 {40.0, 0.0, 30.0}) &&
                near(result.cases[1].closure_consumer.lever_arm_moment_b_nm,
                     {-3.0, 6.0, 4.0}) &&
                near(result.cases[1].closure_consumer.moment_about_com_b_nm,
                     {-7.0, 11.0, -2.0}),
            "three-dimensional propulsion wrench anchor differs");
    require(near(result.cases[2].response.force_b_n,
                 {0.0, 0.0, 0.0}) &&
                near(result.cases[2].closure_consumer.moment_about_com_b_nm,
                     {0.0, 0.0, 0.0}) &&
                near(result.cases[2].mass_consumer.mass_candidate_kg, 5.0),
            "zero propulsion response anchor differs");

    result.equivalence_results.push_back(intervalPartition(inputs[0]));
    require(result.equivalence_results.size() == 1 &&
                result.equivalence_results[0].status == "passed" &&
                near(result.equivalence_results[0].summed_consumed_fuel_mass_kg,
                     0.25) &&
                near(result.equivalence_results[0].
                         sequential_final_mass_candidate_kg,
                     119.75),
            "propulsion interval partition failed");

    PropulsionInput invalid = inputs[0];
    invalid.context.body_frame_id = "frame.other@1";
    recordInvalid(result, "INVALID-YYZ-PROPULSION-FRAME-MISMATCH",
                  invalid);

    invalid = inputs[0];
    invalid.context.clock_domain = "clock.other@1";
    recordInvalid(result, "INVALID-YYZ-PROPULSION-CLOCK-MISMATCH",
                  invalid);

    invalid = inputs[0];
    invalid.context.sample_tick = 21;
    recordInvalid(
        result, "INVALID-YYZ-PROPULSION-SAMPLE-INTERVAL-MISMATCH",
        invalid);

    invalid = inputs[0];
    invalid.context.configuration_revision = -1;
    recordInvalid(result, "INVALID-YYZ-PROPULSION-REVISION", invalid);

    invalid = inputs[0];
    invalid.context.base_dt_s = 0.0;
    recordInvalid(result, "INVALID-YYZ-PROPULSION-NONPOSITIVE-DT",
                  invalid);

    invalid = inputs[0];
    invalid.supplied.thrust_magnitude_n = -1.0;
    recordInvalid(result, "INVALID-YYZ-PROPULSION-NEGATIVE-THRUST",
                  invalid);

    invalid = inputs[0];
    invalid.supplied.thrust_direction_b_unit = {2.0, 0.0, 0.0};
    recordInvalid(result, "INVALID-YYZ-PROPULSION-NONUNIT-DIRECTION",
                  invalid);

    invalid = inputs[0];
    invalid.supplied.intrinsic_moment_at_application_b_nm.x =
        std::numeric_limits<double>::infinity();
    recordInvalid(result, "INVALID-YYZ-PROPULSION-NONFINITE-MOMENT",
                  invalid);

    invalid = inputs[0];
    invalid.supplied.fuel_consumption_rate_kgps = -1.0;
    recordInvalid(result, "INVALID-YYZ-PROPULSION-NEGATIVE-CONSUMPTION",
                  invalid);

    invalid = inputs[0];
    invalid.mass_consumer.committed_mass_kg = 0.1;
    recordInvalid(result, "INVALID-YYZ-PROPULSION-DEPLETED-MASS",
                  invalid);
    require(result.invalid_input_rejections.size() == 10,
            "an invalid propulsion-response input was accepted");

    const PropulsionResult& accepted = result.cases[0];
    FormulaOptions options;
    options.reverse_thrust = true;
    result.mutation_results.push_back(makeMutation(
        "MUTATION-YYZ-PROPULSION-REVERSED-THRUST-DIRECTION",
        MutationKind::ReversedThrust, accepted, evaluate(inputs[0], options)));
    options = {};
    options.pretransport_moment = true;
    result.mutation_results.push_back(makeMutation(
        "MUTATION-YYZ-PROPULSION-PRETRANSPORTED-MOMENT",
        MutationKind::PretransportedMoment, accepted,
        evaluate(inputs[0], options)));
    options = {};
    options.mass_gain = true;
    result.mutation_results.push_back(makeMutation(
        "MUTATION-YYZ-PROPULSION-MASS-GAIN",
        MutationKind::MassGain, accepted, evaluate(inputs[0], options)));
    require(result.mutation_results.size() == 3 &&
                std::all_of(result.mutation_results.begin(),
                            result.mutation_results.end(),
                            [](const MutationResult& mutation) {
                                return mutation.status == "rejected";
                            }) &&
                near(result.mutation_results[0].
                         max_abs_physical_difference,
                     800.0) &&
                near(result.mutation_results[1].
                         max_abs_physical_difference,
                     275.0) &&
                near(result.mutation_results[2].
                         max_abs_physical_difference,
                     0.5),
            "a propulsion physical mutation matched the accepted result");
    return result;
}

void writeVec3(const Vec3& value) {
    std::cout << '[' << value.x << ',' << value.y << ',' << value.z << ']';
}

void writeResponse(const PropulsionResponse& value) {
    std::cout << "{\"model_id\":\"" << value.model_id
              << "\",\"source_id\":\"" << value.source_id
              << "\",\"quality\":\"" << value.quality
              << "\",\"body_frame_id\":\"" << value.body_frame_id
              << "\",\"sample_tick\":" << value.sample_tick
              << ",\"clock_domain\":\"" << value.clock_domain
              << "\",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"valid_from_tick\":" << value.valid_from_tick
              << ",\"valid_until_tick\":" << value.valid_until_tick
              << ",\"force_B_N\":";
    writeVec3(value.force_b_n);
    std::cout << ",\"r_CoM_to_application_B_m\":";
    writeVec3(value.r_com_to_application_b_m);
    std::cout << ",\"moment_at_application_B_Nm\":";
    writeVec3(value.moment_at_application_b_nm);
    std::cout << ",\"fuel_consumption_rate_kgps\":"
              << value.fuel_consumption_rate_kgps << '}';
}

void writeClosureConsumer(const ClosureConsumer& value) {
    std::cout << "{\"source_id\":\"" << value.source_id
              << "\",\"body_frame_id\":\"" << value.body_frame_id
              << "\",\"sample_tick\":" << value.sample_tick
              << ",\"clock_domain\":\"" << value.clock_domain
              << "\",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"force_B_N\":";
    writeVec3(value.force_b_n);
    std::cout << ",\"moment_at_application_B_Nm\":";
    writeVec3(value.moment_at_application_b_nm);
    std::cout << ",\"lever_arm_moment_B_Nm\":";
    writeVec3(value.lever_arm_moment_b_nm);
    std::cout << ",\"moment_about_CoM_B_Nm\":";
    writeVec3(value.moment_about_com_b_nm);
    std::cout << '}';
}

void writeMassConsumer(const MassConsumerResult& value) {
    std::cout << "{\"mass_state_id\":\"" << value.mass_state_id
              << "\",\"clock_domain\":\"" << value.clock_domain
              << "\",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"valid_from_tick\":" << value.valid_from_tick
              << ",\"valid_until_tick\":" << value.valid_until_tick
              << ",\"interval_duration_s\":"
              << value.interval_duration_s
              << ",\"fuel_consumption_rate_kgps\":"
              << value.fuel_consumption_rate_kgps
              << ",\"consumed_fuel_mass_kg\":"
              << value.consumed_fuel_mass_kg
              << ",\"mass_delta_kg\":" << value.mass_delta_kg
              << ",\"committed_mass_kg\":" << value.committed_mass_kg
              << ",\"mass_candidate_kg\":" << value.mass_candidate_kg
              << '}';
}

void writeCase(const PropulsionResult& value) {
    std::cout << "{\"id\":\"" << value.id << "\",\"response\":";
    writeResponse(value.response);
    std::cout << ",\"closure_consumer\":";
    writeClosureConsumer(value.closure_consumer);
    std::cout << ",\"mass_consumer\":";
    writeMassConsumer(value.mass_consumer);
    std::cout << '}';
}

void writeEquivalence(const EquivalenceResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"status\":\"" << value.status
              << "\",\"force_and_response_max_abs_difference\":"
              << value.force_and_response_max_abs_difference
              << ",\"application_wrench_max_abs_difference\":"
              << value.application_wrench_max_abs_difference
              << ",\"summed_consumed_fuel_mass_kg\":"
              << value.summed_consumed_fuel_mass_kg
              << ",\"consumed_fuel_mass_difference_kg\":"
              << value.consumed_fuel_mass_difference_kg
              << ",\"sequential_final_mass_candidate_kg\":"
              << value.sequential_final_mass_candidate_kg
              << ",\"final_mass_candidate_difference_kg\":"
              << value.final_mass_candidate_difference_kg << '}';
}

void writeMutation(const MutationResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"status\":\"" << value.status
              << "\",\"max_abs_physical_difference\":"
              << value.max_abs_physical_difference;
    if (value.kind == MutationKind::ReversedThrust) {
        std::cout << ",\"observed_force_B_N\":";
        writeVec3(value.first_observed_vector);
    } else if (value.kind == MutationKind::PretransportedMoment) {
        std::cout << ",\"observed_moment_at_application_B_Nm\":";
        writeVec3(value.first_observed_vector);
        std::cout << ",\"observed_moment_about_CoM_B_Nm\":";
        writeVec3(value.second_observed_vector);
    } else {
        std::cout << ",\"observed_mass_delta_kg\":"
                  << value.observed_mass_delta_kg
                  << ",\"observed_mass_candidate_kg\":"
                  << value.observed_mass_candidate_kg;
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
              << ",\"model_choice_status\":\""
              << kModelChoiceStatus << "\",\"cases\":[";
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
            "usage: gnc_yyz_propulsion_response_probe --self-check\n";
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

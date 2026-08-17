#include <yyz/mass_commit.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using gnc::contracts::ClockDomainIdentity;
using gnc::contracts::DataQuality;
using gnc::contracts::FrameIdentity;
using gnc::contracts::HalfOpenValidityInterval;
using gnc::contracts::IntervalSampleContext;
using gnc::contracts::SampleContext;
using gnc::contracts::SimulationInstant;
using gnc::foundation::FiniteCheck;
using gnc::foundation::Mat3;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;
using gnc::foundation::QuaternionNormalizationPolicy;
using gnc::foundation::QuaternionPolicy;
using gnc::foundation::Vec3;
using namespace gnc::packages::yyz;

constexpr std::string_view kFixtureId =
    "REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001";
constexpr std::string_view kOracleId =
    "ORACLE-YYZ-TWO-INTERVAL-MASS-COMMIT-001";
constexpr std::string_view kReferenceModelId =
    "MODEL-YYZ-TWO-INTERVAL-MASS-COMMIT-001";
constexpr std::string_view kPropulsionFixtureId =
    "REF-YYZ-PROPULSION-RESPONSE-001";
constexpr std::string_view kPropulsionOracleId =
    "ORACLE-YYZ-PROPULSION-RESPONSE-001";
constexpr std::string_view kPropulsionReferenceModelId =
    "MODEL-YYZ-PROPULSION-RESPONSE-001";
constexpr std::string_view kPropulsionSourceId = "propulsion.main";
constexpr std::string_view kMassStateId =
    "mass.fixture.yyz.vehicle@1";
constexpr std::string_view kInertialFrame =
    "frame.fixture.yyz.inertial-cartesian@1";
constexpr std::string_view kBodyFrame =
    "frame.fixture.yyz.body@1";
constexpr std::string_view kClock =
    "clock.fixture.yyz.simulation@1";
constexpr double kAbsolute = 2.0e-12;
constexpr double kRelative = 2.0e-12;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

[[nodiscard]] bool near(double actual, double expected,
                        double absolute = kAbsolute,
                        double relative = kRelative) {
    const double scale =
        std::max({1.0, std::abs(actual), std::abs(expected)});
    return std::isfinite(actual) && std::isfinite(expected) &&
           std::abs(actual - expected) <= absolute + relative * scale;
}

[[nodiscard]] bool near(const Vec3& actual, const Vec3& expected) {
    return near(actual(0), expected(0)) &&
           near(actual(1), expected(1)) &&
           near(actual(2), expected(2));
}

template <typename Value>
const Value& require_value(const NumericalOutcome<Value>& outcome,
                           std::string_view message) {
    require(outcome.has_value(), message);
    return outcome.value();
}

template <typename Value>
void expect_failure(const NumericalOutcome<Value>& outcome,
                    NumericalStatus expected,
                    std::string_view message) {
    require(!outcome.has_value() && outcome.status() == expected, message);
}

NumericalPolicy fixture_numerical_policy() {
    NumericalPolicy policy;
    policy.absolute_tolerance = kAbsolute;
    policy.relative_tolerance = kRelative;
    policy.finite_check = FiniteCheck::EveryStage;
    policy.zero_tolerance = 1.0e-14;
    policy.condition_limit = 1.0e12;
    return policy;
}

QuaternionPolicy fixture_quaternion_policy() {
    QuaternionPolicy policy;
    policy.numerical = fixture_numerical_policy();
    policy.normalization =
        QuaternionNormalizationPolicy::NormalizeWithFlag;
    return policy;
}

NumericalPolicy propulsion_numerical_policy() {
    NumericalPolicy policy = fixture_numerical_policy();
    policy.absolute_tolerance = 1.0e-12;
    policy.relative_tolerance = 1.0e-12;
    return policy;
}

RigidStepModelDefinition fixture_rigid_definition() {
    RigidStepModelDefinition definition;
    definition.model_id = std::string(kRigidStepModelIdentity);
    definition.model_version = "0.2.0";
    definition.inertial_frame = FrameIdentity{std::string(kInertialFrame)};
    definition.body_frame = FrameIdentity{std::string(kBodyFrame)};
    definition.clock_domain = ClockDomainIdentity{std::string(kClock)};
    definition.configuration_revision = 11;
    definition.algorithm.fixed_step_seconds = 0.1;
    definition.algorithm.numerical_policy = fixture_numerical_policy();
    definition.algorithm.attitude_evaluation_policy =
        fixture_quaternion_policy();
    definition.algorithm.candidate_attitude_policy =
        fixture_quaternion_policy();

    auto& aero = definition.aerodynamics;
    aero.source_id = "aero.zero-force";
    aero.table_id = "aero-table.fixture.yyz.zero@1";
    aero.configuration_id = "configuration.fixture.yyz.clean@1";
    aero.reference_area_square_meters = 1.0;
    aero.reference_span_meters = 1.0;
    aero.reference_chord_meters = 1.0;
    aero.body_origin_to_application.value = Vec3::Zero();
    aero.mach_axis = {0.05, 0.2};
    aero.alpha_axis_radians = {-0.1, 0.1};
    aero.beta_axis_radians = {-0.1, 0.1};
    aero.coefficient_rows_ca_cy_cn_cl_cm_cn.assign(8U, {});
    return definition;
}

ScalarBurnMassDefinition fixture_mass_definition() {
    return {
        std::string(kScalarBurnMassModelIdentity),
        "0.1.0",
        std::string(kMassStateId),
    };
}

SuppliedPropulsionDefinition fixture_propulsion_definition() {
    SuppliedPropulsionDefinition definition;
    definition.model_id = std::string(kSuppliedPropulsionModelIdentity);
    definition.model_version = "0.1.0";
    definition.source_id = std::string(kPropulsionSourceId);
    definition.body_frame = FrameIdentity{std::string(kBodyFrame)};
    definition.clock_domain = ClockDomainIdentity{std::string(kClock)};
    definition.mass_state_id = std::string(kMassStateId);
    definition.numerical_policy = propulsion_numerical_policy();
    return definition;
}

[[nodiscard]] SimulationInstant instant_at(std::int64_t tick,
                                           double base_dt_seconds) {
    return {tick, base_dt_seconds * static_cast<double>(tick)};
}

[[nodiscard]] SampleContext sample_context_at(
    std::string_view frame, std::int64_t tick, double base_dt_seconds,
    std::int64_t configuration_revision) {
    return {
        FrameIdentity{std::string(frame)},
        ClockDomainIdentity{std::string(kClock)},
        instant_at(tick, base_dt_seconds),
        configuration_revision,
        DataQuality::Valid,
    };
}

[[nodiscard]] IntervalSampleContext interval_context_at(
    std::string_view frame, std::int64_t start_tick,
    std::int64_t end_tick, double base_dt_seconds,
    std::int64_t configuration_revision) {
    return {
        sample_context_at(frame, start_tick, base_dt_seconds,
                          configuration_revision),
        HalfOpenValidityInterval{
            instant_at(start_tick, base_dt_seconds),
            instant_at(end_tick, base_dt_seconds)},
    };
}

[[nodiscard]] SimulationInstant instant(std::int64_t tick) {
    return instant_at(tick, 0.1);
}

[[nodiscard]] SampleContext sample_context(
    std::string_view frame, std::int64_t tick) {
    return sample_context_at(frame, tick, 0.1, 11);
}

[[nodiscard]] IntervalSampleContext interval_context(
    std::string_view frame, std::int64_t tick) {
    return {
        sample_context(frame, tick),
        HalfOpenValidityInterval{instant(tick), instant(tick + 1)},
    };
}

[[nodiscard]] RigidMassIntervalInput interval_input(std::int64_t tick) {
    RigidMassIntervalInput interval;
    interval.context = {
        FrameIdentity{std::string(kInertialFrame)},
        FrameIdentity{std::string(kBodyFrame)},
        ClockDomainIdentity{std::string(kClock)},
        instant(tick),
        instant(tick + 1),
        11,
        DataQuality::Valid,
    };
    interval.environment.context = sample_context(kInertialFrame, tick);
    interval.environment.gravity.value = Vec3::Zero();
    interval.environment.velocity_airmass.value = Vec3::Zero();
    interval.environment.density_kilograms_per_cubic_meter = 1.0;
    interval.environment.speed_of_sound_meters_per_second = 100.0;
    interval.supplied_wrench.context = interval_context(kBodyFrame, tick);
    interval.supplied_wrench.source_id = "propulsion.supplied-force";
    interval.supplied_wrench.force.value = Vec3{240.0, 0.0, 0.0};
    interval.supplied_wrench.body_origin_to_application.value =
        Vec3{0.2, -0.1, 0.05};
    interval.supplied_wrench.intrinsic_moment_at_application.value =
        Vec3::Zero();
    interval.mass_flow.context = interval_context(kBodyFrame, tick);
    interval.mass_flow.mass_state_id = std::string(kMassStateId);
    interval.mass_flow.fuel_consumption_rate_kilograms_per_second = 0.5;
    return interval;
}

[[nodiscard]] TwoIntervalMassCommitInput fixture_input() {
    TwoIntervalMassCommitInput input;
    input.opening_boundary.rigid_context =
        sample_context(kInertialFrame, 0);
    input.opening_boundary.rigid_state.position.value = Vec3::Zero();
    input.opening_boundary.rigid_state.velocity.value =
        Vec3{10.0, 0.0, 0.0};
    input.opening_boundary.rigid_state.attitude.value =
        gnc::foundation::quaternion_from_wxyz(1.0, 0.0, 0.0, 0.0);
    input.opening_boundary.rigid_state.angular_rate.value = Vec3::Zero();

    auto& mass = input.opening_boundary.mass_state;
    mass.context = sample_context(kBodyFrame, 0);
    mass.mass_state_id = std::string(kMassStateId);
    mass.mass_kilograms = 120.0;
    mass.body_origin_to_center_of_mass.value = Vec3{0.2, -0.1, 0.05};
    mass.inertia_about_center_of_mass.value <<
        12.0, 1.0, 0.5,
        1.0, 20.0, 2.0,
        0.5, 2.0, 30.0;
    input.intervals = {interval_input(0), interval_input(1)};
    return input;
}

struct PropulsionCaseInput {
    std::string_view id;
    std::int64_t start_tick = 0;
    std::int64_t end_tick = 0;
    double base_dt_seconds = 0.0;
    std::int64_t configuration_revision = 0;
    double thrust_magnitude_newtons = 0.0;
    Vec3 thrust_direction = Vec3::UnitX();
    Vec3 center_of_mass_to_application = Vec3::Zero();
    Vec3 intrinsic_moment_at_application = Vec3::Zero();
    double fuel_consumption_rate_kilograms_per_second = 0.0;
    double committed_mass_kilograms = 0.0;
};

[[nodiscard]] std::array<PropulsionCaseInput, 3U>
fixture_propulsion_cases() {
    return {{
        {
            "CASE-YYZ-PROPULSION-OFF-AXIS-CONSUMERS",
            20,
            25,
            0.1,
            8,
            500.0,
            Vec3{0.6, 0.8, 0.0},
            Vec3{-0.5, 0.25, 0.1},
            Vec3{1.0, -2.0, 3.0},
            0.5,
            120.0,
        },
        {
            "CASE-YYZ-PROPULSION-THREE-DIMENSIONAL-WRENCH",
            3,
            7,
            0.25,
            9,
            50.0,
            Vec3{0.8, 0.0, 0.6},
            Vec3{0.2, -0.1, 0.3},
            Vec3{-4.0, 5.0, -6.0},
            0.0,
            10.0,
        },
        {
            "CASE-YYZ-PROPULSION-ZERO-RESPONSE",
            100,
            101,
            0.02,
            10,
            0.0,
            Vec3{1.0, 0.0, 0.0},
            Vec3{-2.0, 3.0, -4.0},
            Vec3::Zero(),
            0.0,
            5.0,
        },
    }};
}

[[nodiscard]] SuppliedPropulsionInput propulsion_input(
    const PropulsionCaseInput& fixture) {
    SuppliedPropulsionInput input;
    input.context = interval_context_at(
        kBodyFrame, fixture.start_tick, fixture.end_tick,
        fixture.base_dt_seconds, fixture.configuration_revision);
    input.thrust_magnitude_newtons = fixture.thrust_magnitude_newtons;
    input.thrust_direction.value = fixture.thrust_direction;
    input.center_of_mass_to_application.value =
        fixture.center_of_mass_to_application;
    input.intrinsic_moment_at_application.value =
        fixture.intrinsic_moment_at_application;
    input.fuel_consumption_rate_kilograms_per_second =
        fixture.fuel_consumption_rate_kilograms_per_second;
    return input;
}

[[nodiscard]] MassState propulsion_mass_state(
    const PropulsionCaseInput& fixture) {
    MassState state;
    state.context = sample_context_at(
        kBodyFrame, fixture.start_tick, fixture.base_dt_seconds,
        fixture.configuration_revision);
    state.mass_state_id = std::string(kMassStateId);
    state.mass_kilograms = fixture.committed_mass_kilograms;
    state.body_origin_to_center_of_mass.value = Vec3::Zero();
    state.inertia_about_center_of_mass.value = Mat3::Identity();
    return state;
}

struct PropulsionCaseResult {
    std::string id;
    SuppliedPropulsionInput input;
    SuppliedPropulsionOutput response;
    ScalarBurnMassOutput mass;
    double committed_mass_kilograms = 0.0;
};

[[nodiscard]] PropulsionCaseResult evaluate_propulsion_case(
    const SuppliedPropulsionDefinition& propulsion_definition,
    const ScalarBurnMassDefinition& mass_definition,
    const PropulsionCaseInput& fixture) {
    const SuppliedPropulsionInput input = propulsion_input(fixture);
    const auto response_outcome = SuppliedPropulsionKernel::evaluate(
        propulsion_definition, input);
    const auto& response = require_value(
        response_outcome, "supplied propulsion response failed");
    const auto mass_outcome = ScalarBurnMassKernel::evaluate(
        mass_definition, propulsion_mass_state(fixture),
        response.mass_flow, fixture_numerical_policy());
    const auto& mass = require_value(
        mass_outcome, "propulsion mass consumer failed");
    return {
        std::string(fixture.id),
        input,
        response,
        mass,
        fixture.committed_mass_kilograms,
    };
}

[[nodiscard]] double max_abs_difference(const Vec3& lhs,
                                        const Vec3& rhs) {
    return (lhs - rhs).cwiseAbs().maxCoeff();
}

struct PropulsionEquivalenceResult {
    double force_and_response_max_abs_difference = 0.0;
    double application_wrench_max_abs_difference = 0.0;
    double summed_consumed_fuel_mass_kilograms = 0.0;
    double consumed_fuel_mass_difference_kilograms = 0.0;
    double sequential_final_mass_candidate_kilograms = 0.0;
    double final_mass_candidate_difference_kilograms = 0.0;
};

[[nodiscard]] PropulsionEquivalenceResult propulsion_partition_equivalence(
    const SuppliedPropulsionDefinition& propulsion_definition,
    const ScalarBurnMassDefinition& mass_definition,
    const PropulsionCaseInput& whole_fixture) {
    const PropulsionCaseResult whole = evaluate_propulsion_case(
        propulsion_definition, mass_definition, whole_fixture);
    PropulsionCaseInput first_fixture = whole_fixture;
    first_fixture.end_tick = 22;
    const PropulsionCaseResult first = evaluate_propulsion_case(
        propulsion_definition, mass_definition, first_fixture);
    PropulsionCaseInput second_fixture = whole_fixture;
    second_fixture.start_tick = 22;
    second_fixture.committed_mass_kilograms =
        first.mass.candidate.state.mass_kilograms;
    const PropulsionCaseResult second = evaluate_propulsion_case(
        propulsion_definition, mass_definition, second_fixture);

    const auto response_difference = [&](const PropulsionCaseResult& part) {
        return std::max({
            max_abs_difference(
                whole.response.supplied_body_wrench.force.value,
                part.response.supplied_body_wrench.force.value),
            max_abs_difference(
                whole.response.supplied_body_wrench
                    .center_of_mass_to_application.value,
                part.response.supplied_body_wrench
                    .center_of_mass_to_application.value),
            max_abs_difference(
                whole.response.supplied_body_wrench
                    .intrinsic_moment_at_application.value,
                part.response.supplied_body_wrench
                    .intrinsic_moment_at_application.value),
        });
    };
    const auto wrench_difference = [&](const PropulsionCaseResult& part) {
        return std::max({
            max_abs_difference(
                whole.response.supplied_body_wrench.force.value,
                part.response.supplied_body_wrench.force.value),
            max_abs_difference(
                whole.response.supplied_body_wrench
                    .intrinsic_moment_at_application.value,
                part.response.supplied_body_wrench
                    .intrinsic_moment_at_application.value),
            max_abs_difference(whole.response.lever_arm_moment.value,
                               part.response.lever_arm_moment.value),
            max_abs_difference(
                whole.response.moment_about_center_of_mass.value,
                part.response.moment_about_center_of_mass.value),
        });
    };
    const double summed_consumed =
        first.mass.consumed_mass_kilograms +
        second.mass.consumed_mass_kilograms;
    PropulsionEquivalenceResult result;
    result.force_and_response_max_abs_difference =
        std::max(response_difference(first), response_difference(second));
    result.application_wrench_max_abs_difference =
        std::max(wrench_difference(first), wrench_difference(second));
    result.summed_consumed_fuel_mass_kilograms = summed_consumed;
    result.consumed_fuel_mass_difference_kilograms =
        std::abs(summed_consumed - whole.mass.consumed_mass_kilograms);
    result.sequential_final_mass_candidate_kilograms =
        second.mass.candidate.state.mass_kilograms;
    result.final_mass_candidate_difference_kilograms = std::abs(
        second.mass.candidate.state.mass_kilograms -
        whole.mass.candidate.state.mass_kilograms);
    require(near(result.force_and_response_max_abs_difference, 0.0) &&
                near(result.application_wrench_max_abs_difference, 0.0) &&
                near(result.consumed_fuel_mass_difference_kilograms, 0.0) &&
                near(result.final_mass_candidate_difference_kilograms, 0.0),
            "propulsion interval partition equivalence failed");
    return result;
}

struct PropulsionProbeBundle {
    std::vector<PropulsionCaseResult> cases;
    PropulsionEquivalenceResult equivalence;
    std::vector<std::string> invalid_input_rejections;
    PropelledFrozenRigidMassStepOutput consumer;
    std::vector<std::string> direct_checks;
};

[[nodiscard]] PropulsionProbeBundle run_propulsion_probe(
    const PreparedRigidStepModel& prepared,
    const ScalarBurnMassDefinition& mass_definition,
    const CommittedRigidMassBoundary& opening_boundary) {
    const SuppliedPropulsionDefinition propulsion_definition =
        fixture_propulsion_definition();
    const auto fixtures = fixture_propulsion_cases();
    std::vector<PropulsionCaseResult> cases;
    cases.reserve(fixtures.size());
    for (const auto& fixture : fixtures) {
        cases.emplace_back(evaluate_propulsion_case(
            propulsion_definition, mass_definition, fixture));
    }

    const auto& off_axis = cases[0];
    require(near(off_axis.response.supplied_body_wrench.force.value,
                 Vec3{300.0, 400.0, 0.0}) &&
                near(off_axis.response.lever_arm_moment.value,
                     Vec3{-40.0, 30.0, -275.0}) &&
                near(off_axis.response.moment_about_center_of_mass.value,
                     Vec3{-39.0, 28.0, -272.0}) &&
                near(off_axis.mass.consumed_mass_kilograms, 0.25) &&
                near(off_axis.mass.candidate.state.mass_kilograms, 119.75),
            "off-axis propulsion anchors differ");
    require(near(cases[1].response.supplied_body_wrench.force.value,
                 Vec3{40.0, 0.0, 30.0}) &&
                near(cases[1].response.lever_arm_moment.value,
                     Vec3{-3.0, 6.0, 4.0}) &&
                near(cases[1].response.moment_about_center_of_mass.value,
                     Vec3{-7.0, 11.0, -2.0}) &&
                near(cases[1].mass.candidate.state.mass_kilograms, 10.0),
            "three-dimensional propulsion anchors differ");
    require(near(cases[2].response.supplied_body_wrench.force.value,
                 Vec3::Zero()) &&
                near(cases[2].response.moment_about_center_of_mass.value,
                     Vec3::Zero()) &&
                near(cases[2].mass.candidate.state.mass_kilograms, 5.0),
            "zero propulsion anchors differ");
    std::vector<std::string> checks{
        "propulsion-three-oracle-cases"};

    const PropulsionEquivalenceResult equivalence =
        propulsion_partition_equivalence(
            propulsion_definition, mass_definition, fixtures[0]);
    checks.emplace_back("propulsion-interval-partition-equivalence");

    std::vector<std::string> invalid;
    const auto reject_response = [&](std::string_view id,
                                     const SuppliedPropulsionInput& input,
                                     NumericalStatus status) {
        expect_failure(SuppliedPropulsionKernel::evaluate(
                           propulsion_definition, input),
                       status, id);
        invalid.emplace_back(id);
    };
    SuppliedPropulsionInput mutated = propulsion_input(fixtures[0]);
    mutated.context.sample.frame.id = "frame.other@1";
    reject_response("INVALID-YYZ-PROPULSION-FRAME-MISMATCH", mutated,
                    NumericalStatus::DomainError);
    mutated = propulsion_input(fixtures[0]);
    mutated.context.sample.clock_domain.id = "clock.other@1";
    reject_response("INVALID-YYZ-PROPULSION-CLOCK-MISMATCH", mutated,
                    NumericalStatus::DomainError);
    mutated = propulsion_input(fixtures[0]);
    mutated.context.sample.sample_time = instant_at(21, 0.1);
    reject_response(
        "INVALID-YYZ-PROPULSION-SAMPLE-INTERVAL-MISMATCH", mutated,
        NumericalStatus::DomainError);
    mutated = propulsion_input(fixtures[0]);
    mutated.context.sample.configuration_revision = -1;
    reject_response("INVALID-YYZ-PROPULSION-REVISION", mutated,
                    NumericalStatus::DomainError);
    mutated = propulsion_input(fixtures[0]);
    mutated.context.validity.effective_until.seconds =
        mutated.context.validity.effective_from.seconds;
    reject_response("INVALID-YYZ-PROPULSION-NONPOSITIVE-DT", mutated,
                    NumericalStatus::DomainError);
    mutated = propulsion_input(fixtures[0]);
    mutated.thrust_magnitude_newtons = -1.0;
    reject_response("INVALID-YYZ-PROPULSION-NEGATIVE-THRUST", mutated,
                    NumericalStatus::DomainError);
    mutated = propulsion_input(fixtures[0]);
    mutated.thrust_direction.value = Vec3{2.0, 0.0, 0.0};
    reject_response("INVALID-YYZ-PROPULSION-NONUNIT-DIRECTION", mutated,
                    NumericalStatus::DomainError);
    mutated = propulsion_input(fixtures[0]);
    mutated.intrinsic_moment_at_application.value(0) =
        std::numeric_limits<double>::infinity();
    reject_response("INVALID-YYZ-PROPULSION-NONFINITE-MOMENT", mutated,
                    NumericalStatus::NonFiniteInput);
    mutated = propulsion_input(fixtures[0]);
    mutated.fuel_consumption_rate_kilograms_per_second = -1.0;
    reject_response("INVALID-YYZ-PROPULSION-NEGATIVE-CONSUMPTION", mutated,
                    NumericalStatus::DomainError);

    const auto depleted_response = SuppliedPropulsionKernel::evaluate(
        propulsion_definition, propulsion_input(fixtures[0]));
    const auto& depleted_response_value = require_value(
        depleted_response, "depleted-mass response setup failed");
    PropulsionCaseInput depleted_fixture = fixtures[0];
    depleted_fixture.committed_mass_kilograms = 0.1;
    expect_failure(ScalarBurnMassKernel::evaluate(
                       mass_definition,
                       propulsion_mass_state(depleted_fixture),
                       depleted_response_value.mass_flow,
                       fixture_numerical_policy()),
                   NumericalStatus::DomainError,
                   "INVALID-YYZ-PROPULSION-DEPLETED-MASS");
    invalid.emplace_back("INVALID-YYZ-PROPULSION-DEPLETED-MASS");
    require(invalid.size() == 10U,
            "propulsion invalid-input coverage differs");
    checks.emplace_back("propulsion-ten-invalid-input-rejections");

    PropulsionCaseInput consumer_fixture = fixtures[0];
    consumer_fixture.start_tick = 0;
    consumer_fixture.end_tick = 1;
    consumer_fixture.configuration_revision = 11;
    consumer_fixture.committed_mass_kilograms =
        opening_boundary.mass_state.mass_kilograms;
    PropelledRigidMassIntervalInput consumer_input;
    const RigidMassIntervalInput established_interval = interval_input(0);
    consumer_input.context = established_interval.context;
    consumer_input.environment = established_interval.environment;
    consumer_input.propulsion = propulsion_input(consumer_fixture);
    const auto consumer_outcome =
        PropelledFrozenRigidMassStepKernel::evaluate(
            prepared, mass_definition, propulsion_definition,
            opening_boundary, consumer_input);
    const auto& consumer = require_value(
        consumer_outcome,
        "propulsion-to-atomic-boundary consumer failed");
    require(near(consumer.propulsion.supplied_body_wrench.force.value,
                 Vec3{300.0, 400.0, 0.0}) &&
                near(consumer.propulsion.moment_about_center_of_mass.value,
                     Vec3{-39.0, 28.0, -272.0}) &&
                near(consumer.atomic_boundary.rigid_step
                         .supplied_contribution.force.value,
                     Vec3{300.0, 400.0, 0.0}) &&
                near(consumer.atomic_boundary.rigid_step
                         .supplied_contribution
                         .moment_about_center_of_mass.value,
                     Vec3{-39.0, 28.0, -272.0}) &&
                near(consumer.atomic_boundary.mass_evolution
                         .consumed_mass_kilograms,
                     0.05) &&
                near(consumer.atomic_boundary.mass_evolution
                         .candidate.state.mass_kilograms,
                     119.95) &&
                consumer.atomic_boundary.candidate.effective_at.tick == 1,
            "propulsion atomic consumer closure differs");
    checks.emplace_back("propulsion-to-atomic-boundary-single-transport");

    return {
        std::move(cases),
        equivalence,
        std::move(invalid),
        consumer,
        std::move(checks),
    };
}

struct ProbeBundle {
    TwoIntervalMassCommitOutput accepted;
    std::vector<std::string> direct_checks;
    PropulsionProbeBundle propulsion;
};

ProbeBundle run_probe() {
    const auto prepared_outcome =
        prepare_rigid_step_model(fixture_rigid_definition());
    const PreparedRigidStepModel& prepared = require_value(
        prepared_outcome, "two-interval rigid model preparation failed");
    const ScalarBurnMassDefinition mass_definition =
        fixture_mass_definition();
    const TwoIntervalMassCommitInput accepted_input = fixture_input();
    const auto accepted_outcome = TwoIntervalMassCommitKernel::evaluate(
        prepared, mass_definition, accepted_input);
    const auto& accepted = require_value(
        accepted_outcome, "two-interval product evaluation failed");
    const auto& first = accepted.intervals[0];
    const auto& second = accepted.intervals[1];

    require(near(first.staged.projected_committed_mass.mass_kilograms,
                 120.0) &&
                near(first.staged.mass_evolution
                         .integration_mass_kilograms,
                     120.0) &&
                near(first.staged.mass_evolution
                         .candidate.state.mass_kilograms,
                     119.95) &&
                near(first.staged.rigid_step.derivative_at_interval_start
                         .acceleration.value,
                     Vec3{2.0, 0.0, 0.0}) &&
                near(first.staged.rigid_step.candidate.state.position.value,
                     Vec3{1.01, 0.0, 0.0}) &&
                near(first.staged.rigid_step.candidate.state.velocity.value,
                     Vec3{10.2, 0.0, 0.0}),
            "interval zero did not use committed mass");
    std::vector<std::string> checks{
        "interval-zero-committed-mass-and-hidden-candidate"};

    require(first.closing_commit.rigid_context.sample_time.tick == 1 &&
                first.closing_commit.mass_state.context.sample_time.tick == 1 &&
                near(first.closing_commit.mass_state.mass_kilograms,
                     119.95) &&
                near(first.closing_commit.rigid_state.position.value,
                     Vec3{1.01, 0.0, 0.0}),
            "first atomic boundary differs");
    checks.emplace_back("first-atomic-boundary");

    require(second.staged.opening_boundary.rigid_context.sample_time.tick ==
                first.closing_commit.rigid_context.sample_time.tick &&
                second.staged.opening_boundary.mass_state.context.sample_time
                        .tick ==
                    first.closing_commit.mass_state.context.sample_time.tick &&
                near(second.staged.opening_boundary.rigid_state.position.value,
                     first.closing_commit.rigid_state.position.value) &&
                near(second.staged.opening_boundary.rigid_state.velocity.value,
                     first.closing_commit.rigid_state.velocity.value) &&
                near(second.staged.projected_committed_mass.mass_kilograms,
                     first.closing_commit.mass_state.mass_kilograms) &&
                near(second.staged.rigid_step.derivative_at_interval_start
                         .acceleration.value,
                     Vec3{240.0 / 119.95, 0.0, 0.0}),
            "interval one did not consume the complete committed pair");
    checks.emplace_back("next-interval-consumes-committed-pair");

    require(accepted.terminal_boundary.rigid_context.sample_time.tick == 2 &&
                accepted.terminal_boundary.mass_state.context.sample_time.tick ==
                    2 &&
                near(accepted.terminal_boundary.mass_state.mass_kilograms,
                     119.9) &&
                near(accepted.terminal_boundary.rigid_state.position.value,
                     Vec3{2.0400041684035015, 0.0, 0.0}) &&
                near(accepted.terminal_boundary.rigid_state.velocity.value,
                     Vec3{10.40008336807003, 0.0, 0.0}),
            "terminal committed boundary differs from oracle anchors");
    checks.emplace_back("accepted-terminal-oracle-anchors");

    require(near(accepted.terminal_boundary.mass_state
                     .body_origin_to_center_of_mass.value,
                 Vec3{0.2, -0.1, 0.05}) &&
                near(accepted.terminal_boundary.mass_state
                         .inertia_about_center_of_mass.value(0, 1),
                     1.0) &&
                near(accepted.terminal_boundary.mass_state
                         .inertia_about_center_of_mass.value(1, 2),
                     2.0),
            "constant-geometry mass state changed geometry");
    checks.emplace_back("constant-geometry-preserved");

    MassFlowIntervalInput zero_flow = accepted_input.intervals[0].mass_flow;
    zero_flow.fuel_consumption_rate_kilograms_per_second = 0.0;
    const auto zero_flow_outcome = ScalarBurnMassKernel::evaluate(
        mass_definition, accepted_input.opening_boundary.mass_state,
        zero_flow, fixture_numerical_policy());
    const auto& zero_flow_output = require_value(
        zero_flow_outcome, "zero-flow mass evolution failed");
    require(near(zero_flow_output.candidate.state.mass_kilograms, 120.0) &&
                near(zero_flow_output.consumed_mass_kilograms, 0.0),
            "zero-flow mass candidate changed mass");
    checks.emplace_back("zero-flow-mass-candidate");

    MassFlowIntervalInput negative_flow =
        accepted_input.intervals[0].mass_flow;
    negative_flow.fuel_consumption_rate_kilograms_per_second = -0.5;
    expect_failure(ScalarBurnMassKernel::evaluate(
                       mass_definition,
                       accepted_input.opening_boundary.mass_state,
                       negative_flow, fixture_numerical_policy()),
                   NumericalStatus::DomainError,
                   "negative fuel-consumption rate survived");
    checks.emplace_back("negative-flow-rejection");

    MassState indefinite_mass =
        accepted_input.opening_boundary.mass_state;
    indefinite_mass.inertia_about_center_of_mass.value(2, 2) = -1.0;
    expect_failure(ScalarBurnMassKernel::evaluate(
                       mass_definition, indefinite_mass,
                       accepted_input.intervals[0].mass_flow,
                       fixture_numerical_policy()),
                   NumericalStatus::DomainError,
                   "indefinite mass inertia survived");
    checks.emplace_back("mass-invariant-rejection");

    TwoIntervalMassCommitInput depleted = accepted_input;
    depleted.intervals[0].mass_flow
        .fuel_consumption_rate_kilograms_per_second = 1200.0;
    expect_failure(TwoIntervalMassCommitKernel::evaluate(
                       prepared, mass_definition, depleted),
                   NumericalStatus::DomainError,
                   "mass failure exposed a partial rigid boundary");
    checks.emplace_back("atomic-discard-on-mass-failure");

    TwoIntervalMassCommitInput rigid_failure = accepted_input;
    rigid_failure.intervals[1].environment
        .speed_of_sound_meters_per_second = 1.0;
    expect_failure(TwoIntervalMassCommitKernel::evaluate(
                       prepared, mass_definition, rigid_failure),
                   NumericalStatus::OutOfRange,
                   "rigid failure exposed a partial mass boundary");
    checks.emplace_back("atomic-discard-on-rigid-failure");

    TwoIntervalMassCommitInput gap = accepted_input;
    gap.intervals[1] = interval_input(2);
    expect_failure(TwoIntervalMassCommitKernel::evaluate(
                       prepared, mass_definition, gap),
                   NumericalStatus::DomainError,
                   "non-contiguous interval survived");
    checks.emplace_back("contiguous-boundary-rejection");

    PropulsionProbeBundle propulsion = run_propulsion_probe(
        prepared, mass_definition, accepted_input.opening_boundary);
    return {accepted, std::move(checks), std::move(propulsion)};
}

void write_number(double value) {
    std::cout << (value == 0.0 ? 0.0 : value);
}

void write_vec3(const Vec3& value) {
    std::cout << '[';
    for (Eigen::Index index = 0; index < 3; ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        write_number(value(index));
    }
    std::cout << ']';
}

void write_quaternion(const gnc::foundation::QuaternionStorage& value) {
    const auto coefficients = gnc::foundation::quaternion_to_wxyz(value);
    std::cout << '[';
    for (std::size_t index = 0U; index < coefficients.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        write_number(coefficients[index]);
    }
    std::cout << ']';
}

void write_matrix(const Mat3& value) {
    std::cout << '[';
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = 0; column < 3; ++column) {
            if (row != 0 || column != 0) {
                std::cout << ',';
            }
            write_number(value(row, column));
        }
    }
    std::cout << ']';
}

void write_rigid_state(const RigidState& state) {
    std::cout << "{\"position_I_m\":";
    write_vec3(state.position.value);
    std::cout << ",\"velocity_I_mps\":";
    write_vec3(state.velocity.value);
    std::cout << ",\"q_I_B_wxyz\":";
    write_quaternion(state.attitude.value);
    std::cout << ",\"omega_BI_B_radps\":";
    write_vec3(state.angular_rate.value);
    std::cout << '}';
}

void write_interval(const TwoIntervalMassCommitIntervalOutput& interval) {
    const auto& staged = interval.staged;
    const auto& mass = staged.mass_evolution;
    std::cout << "{\"sample_tick\":"
              << staged.opening_boundary.rigid_context.sample_time.tick
              << ",\"valid_from_tick\":"
              << staged.opening_boundary.rigid_context.sample_time.tick
              << ",\"valid_until_tick\":"
              << staged.candidate.effective_at.tick
              << ",\"current_committed_mass_kg\":";
    write_number(mass.current_committed_mass_kilograms);
    std::cout << ",\"integration_mass_kg\":";
    write_number(mass.integration_mass_kilograms);
    std::cout << ",\"consumed_mass_kg\":";
    write_number(mass.consumed_mass_kilograms);
    std::cout << ",\"pending_mass_candidate_kg\":";
    write_number(mass.candidate.state.mass_kilograms);
    std::cout << ",\"pending_visibility_before_commit\":\"candidate-only\""
              << ",\"acceleration_I_mps2\":";
    write_vec3(staged.rigid_step.derivative_at_interval_start
                   .acceleration.value);
    std::cout << ",\"initial_rigid_state\":";
    write_rigid_state(staged.opening_boundary.rigid_state);
    std::cout << ",\"rigid_candidate\":";
    write_rigid_state(staged.candidate.rigid.state);
    std::cout << ",\"closing_commit\":{\"tick\":"
              << interval.closing_commit.rigid_context.sample_time.tick
              << ",\"kind\":\"atomic-rigid-and-mass\",\"mass_kg\":";
    write_number(interval.closing_commit.mass_state.mass_kilograms);
    std::cout << ",\"r_body_origin_to_CoM_B_m\":";
    write_vec3(interval.closing_commit.mass_state
                   .body_origin_to_center_of_mass.value);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    write_matrix(interval.closing_commit.mass_state
                     .inertia_about_center_of_mass.value);
    std::cout << ",\"rigid_state\":";
    write_rigid_state(interval.closing_commit.rigid_state);
    std::cout << "}}";
}

void write_propulsion_case(const PropulsionCaseResult& result) {
    const auto& response = result.response;
    const auto& wrench = response.supplied_body_wrench;
    const auto& context = wrench.context;
    const auto& mass = result.mass;
    const double duration_seconds =
        context.validity.effective_until.seconds -
        context.validity.effective_from.seconds;
    std::cout << "{\"id\":\"" << result.id
              << "\",\"response\":{\"model_id\":\""
              << kSuppliedPropulsionModelIdentity
              << "\",\"source_id\":\"" << wrench.source_id
              << "\",\"quality\":\"Valid\",\"body_frame_id\":\""
              << context.sample.frame.id
              << "\",\"sample_tick\":"
              << context.sample.sample_time.tick
              << ",\"clock_domain\":\""
              << context.sample.clock_domain.id
              << "\",\"configuration_revision\":"
              << context.sample.configuration_revision
              << ",\"valid_from_tick\":"
              << context.validity.effective_from.tick
              << ",\"valid_until_tick\":"
              << context.validity.effective_until.tick
              << ",\"force_B_N\":";
    write_vec3(wrench.force.value);
    std::cout << ",\"r_CoM_to_application_B_m\":";
    write_vec3(wrench.center_of_mass_to_application.value);
    std::cout << ",\"moment_at_application_B_Nm\":";
    write_vec3(wrench.intrinsic_moment_at_application.value);
    std::cout << ",\"fuel_consumption_rate_kgps\":";
    write_number(
        response.mass_flow
            .fuel_consumption_rate_kilograms_per_second);
    std::cout << "},\"closure_consumer\":{\"source_id\":\""
              << wrench.source_id << "\",\"body_frame_id\":\""
              << context.sample.frame.id
              << "\",\"sample_tick\":"
              << context.sample.sample_time.tick
              << ",\"clock_domain\":\""
              << context.sample.clock_domain.id
              << "\",\"configuration_revision\":"
              << context.sample.configuration_revision
              << ",\"force_B_N\":";
    write_vec3(wrench.force.value);
    std::cout << ",\"moment_at_application_B_Nm\":";
    write_vec3(wrench.intrinsic_moment_at_application.value);
    std::cout << ",\"lever_arm_moment_B_Nm\":";
    write_vec3(response.lever_arm_moment.value);
    std::cout << ",\"moment_about_CoM_B_Nm\":";
    write_vec3(response.moment_about_center_of_mass.value);
    std::cout << "},\"mass_consumer\":{\"mass_state_id\":\""
              << response.mass_flow.mass_state_id
              << "\",\"clock_domain\":\""
              << context.sample.clock_domain.id
              << "\",\"configuration_revision\":"
              << context.sample.configuration_revision
              << ",\"valid_from_tick\":"
              << context.validity.effective_from.tick
              << ",\"valid_until_tick\":"
              << context.validity.effective_until.tick
              << ",\"interval_duration_s\":";
    write_number(duration_seconds);
    std::cout << ",\"fuel_consumption_rate_kgps\":";
    write_number(
        response.mass_flow
            .fuel_consumption_rate_kilograms_per_second);
    std::cout << ",\"consumed_fuel_mass_kg\":";
    write_number(mass.consumed_mass_kilograms);
    std::cout << ",\"mass_delta_kg\":";
    write_number(mass.candidate.state.mass_kilograms -
                 result.committed_mass_kilograms);
    std::cout << ",\"committed_mass_kg\":";
    write_number(result.committed_mass_kilograms);
    std::cout << ",\"mass_candidate_kg\":";
    write_number(mass.candidate.state.mass_kilograms);
    std::cout << "}}";
}

void write_propulsion(const PropulsionProbeBundle& propulsion) {
    std::cout << "{\"product_model_id\":\""
              << kSuppliedPropulsionModelIdentity
              << "\",\"contract_id\":\""
              << kSuppliedPropulsionContractIdentity
              << "\",\"source_fixture_id\":\""
              << kPropulsionFixtureId
              << "\",\"source_oracle_id\":\""
              << kPropulsionOracleId
              << "\",\"reference_model_id\":\""
              << kPropulsionReferenceModelId
              << "\",\"status\":\"passed\",\"cases\":[";
    for (std::size_t index = 0U; index < propulsion.cases.size();
         ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        write_propulsion_case(propulsion.cases[index]);
    }
    const auto& equivalence = propulsion.equivalence;
    std::cout << "],\"equivalence_results\":[{\"id\":\"EQUIV-YYZ-PROPULSION-MASS-INTERVAL-PARTITION\",\"status\":\"passed\",\"force_and_response_max_abs_difference\":";
    write_number(equivalence.force_and_response_max_abs_difference);
    std::cout << ",\"application_wrench_max_abs_difference\":";
    write_number(equivalence.application_wrench_max_abs_difference);
    std::cout << ",\"summed_consumed_fuel_mass_kg\":";
    write_number(equivalence.summed_consumed_fuel_mass_kilograms);
    std::cout << ",\"consumed_fuel_mass_difference_kg\":";
    write_number(
        equivalence.consumed_fuel_mass_difference_kilograms);
    std::cout << ",\"sequential_final_mass_candidate_kg\":";
    write_number(
        equivalence.sequential_final_mass_candidate_kilograms);
    std::cout << ",\"final_mass_candidate_difference_kg\":";
    write_number(
        equivalence.final_mass_candidate_difference_kilograms);
    std::cout << "}],\"invalid_input_rejections\":[";
    for (std::size_t index = 0U;
         index < propulsion.invalid_input_rejections.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << '\"' << propulsion.invalid_input_rejections[index]
                  << '\"';
    }
    const auto& consumer = propulsion.consumer;
    std::cout << "],\"atomic_boundary_consumer\":{\"force_B_N\":";
    write_vec3(consumer.atomic_boundary.rigid_step
                   .supplied_contribution.force.value);
    std::cout << ",\"moment_about_CoM_B_Nm\":";
    write_vec3(consumer.atomic_boundary.rigid_step
                   .supplied_contribution
                   .moment_about_center_of_mass.value);
    std::cout << ",\"consumed_fuel_mass_kg\":";
    write_number(consumer.atomic_boundary.mass_evolution
                     .consumed_mass_kilograms);
    std::cout << ",\"mass_candidate_kg\":";
    write_number(consumer.atomic_boundary.mass_evolution
                     .candidate.state.mass_kilograms);
    std::cout << ",\"candidate_tick\":"
              << consumer.atomic_boundary.candidate.effective_at.tick
              << "},\"direct_checks\":[";
    for (std::size_t index = 0U;
         index < propulsion.direct_checks.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << '\"' << propulsion.direct_checks[index] << '\"';
    }
    std::cout << "]}";
}

void write_json(const ProbeBundle& bundle) {
    const auto& terminal = bundle.accepted.terminal_boundary;
    std::cout << std::setprecision(17)
              << "{\"schema_version\":\"gnczmkn.yyz-two-interval-mass-commit-product-probe/2\""
              << ",\"product_model_id\":\""
              << kTwoIntervalMassCommitModelIdentity
              << "\",\"mass_model_id\":\""
              << kScalarBurnMassModelIdentity
              << "\",\"contract_id\":\""
              << kTwoIntervalMassCommitContractIdentity
              << "\",\"source_fixture_id\":\"" << kFixtureId
              << "\",\"source_oracle_id\":\"" << kOracleId
              << "\",\"reference_model_id\":\"" << kReferenceModelId
              << "\",\"status\":\"passed\",\"accepted\":{\"intervals\":[";
    write_interval(bundle.accepted.intervals[0]);
    std::cout << ',';
    write_interval(bundle.accepted.intervals[1]);
    std::cout << "],\"terminal\":{\"tick\":"
              << terminal.rigid_context.sample_time.tick
              << ",\"time_s\":";
    write_number(terminal.rigid_context.sample_time.seconds);
    std::cout << ",\"termination_kind\":\"duration_exact_grid\""
              << ",\"committed_mass_kg\":";
    write_number(terminal.mass_state.mass_kilograms);
    std::cout << ",\"r_body_origin_to_CoM_B_m\":";
    write_vec3(terminal.mass_state.body_origin_to_center_of_mass.value);
    std::cout << ",\"inertia_about_CoM_B_kgm2_row_major\":";
    write_matrix(terminal.mass_state.inertia_about_center_of_mass.value);
    std::cout << ",\"rigid_state\":";
    write_rigid_state(terminal.rigid_state);
    std::cout << "}},\"direct_checks\":[";
    for (std::size_t index = 0U; index < bundle.direct_checks.size();
         ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << '\"' << bundle.direct_checks[index] << '\"';
    }
    std::cout << "],\"propulsion\":";
    write_propulsion(bundle.propulsion);
    std::cout << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr <<
            "usage: gnc_yyz_two_interval_mass_commit_product_probe --self-check\n";
        return 2;
    }
    try {
        write_json(run_probe());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

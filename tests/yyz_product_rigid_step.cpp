#include <yyz/rigid_step.hpp>

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
using gnc::foundation::InterpolationDomainStatus;
using gnc::foundation::Mat3;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;
using gnc::foundation::QuaternionNormalizationPolicy;
using gnc::foundation::QuaternionPolicy;
using gnc::foundation::Vec3;
using namespace gnc::packages::yyz;

constexpr std::string_view kFixtureId =
    "REF-YYZ-FROZEN-INTERVAL-001";
constexpr std::string_view kOracleId =
    "ORACLE-YYZ-FROZEN-INTERVAL-001";
constexpr std::string_view kInertialFrame =
    "frame.fixture.yyz.inertial-cartesian@1";
constexpr std::string_view kBodyFrame = "frame.fixture.yyz.body@1";
constexpr std::string_view kClock = "clock.fixture.yyz.simulation@1";
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

[[nodiscard]] bool near(const Vec3& actual, const Vec3& expected,
                        double absolute = kAbsolute,
                        double relative = kRelative) {
    return near(actual(0), expected(0), absolute, relative) &&
           near(actual(1), expected(1), absolute, relative) &&
           near(actual(2), expected(2), absolute, relative);
}

template <typename Value>
const Value& require_value(const NumericalOutcome<Value>& outcome,
                           std::string_view message) {
    require(outcome.has_value(), message);
    return outcome.value();
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

RigidStepModelDefinition fixture_definition() {
    RigidStepModelDefinition definition;
    definition.model_id = std::string(kRigidStepModelIdentity);
    definition.model_version = "0.1.0";
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
    aero.source_id = "aero.body";
    aero.table_id = "aero-table.fixture.yyz.multiaffine@1";
    aero.configuration_id = "configuration.fixture.yyz.clean@1";
    aero.reference_area_square_meters = 1.0;
    aero.reference_span_meters = 1.0;
    aero.reference_chord_meters = 1.0;
    aero.body_origin_to_application.value = Vec3{0.2, 0.0, -25.0 / 18.0};
    aero.mach_axis = {0.2, 0.6};
    aero.alpha_axis_radians = {-0.1, 0.1};
    aero.beta_axis_radians = {-0.05, 0.05};
    aero.coefficient_rows_ca_cy_cn_cl_cm_cn = {
        {0.006, 0.0245, -0.0795, 0.005, 0.014, -0.00755},
        {0.006, -0.0245, -0.0805, -0.005, 0.014, 0.00755},
        {0.05, 0.0245, 0.0795, 0.005, -0.106, -0.00785},
        {0.05, -0.0245, 0.0805, -0.005, -0.106, 0.00785},
        {0.018, 0.0235, -0.0795, 0.005, 0.022, -0.00795},
        {0.018, -0.0235, -0.0805, -0.005, 0.022, 0.00795},
        {0.07, 0.0235, 0.0795, 0.005, -0.098, -0.00825},
        {0.07, -0.0235, 0.0805, -0.005, -0.098, 0.00825},
    };
    return definition;
}

SampleContext sample_context(std::string_view frame) {
    return {
        FrameIdentity{std::string(frame)},
        ClockDomainIdentity{std::string(kClock)},
        SimulationInstant{0, 0.0},
        11,
        DataQuality::Valid,
    };
}

IntervalSampleContext interval_context(std::string_view frame) {
    return {
        sample_context(frame),
        HalfOpenValidityInterval{
            SimulationInstant{0, 0.0},
            SimulationInstant{1, 0.1},
        },
    };
}

RigidStepInput fixture_input() {
    RigidStepInput input;
    input.context = {
        FrameIdentity{std::string(kInertialFrame)},
        FrameIdentity{std::string(kBodyFrame)},
        ClockDomainIdentity{std::string(kClock)},
        SimulationInstant{0, 0.0},
        SimulationInstant{1, 0.1},
        11,
        DataQuality::Valid,
    };
    input.committed_state.position.value = Vec3{0.0, 0.0, 1000.0};
    input.committed_state.velocity.value = Vec3{110.0, 0.0, 0.0};
    input.committed_state.attitude.value =
        gnc::foundation::quaternion_from_wxyz(1.0, 0.0, 0.0, 0.0);
    input.committed_state.angular_rate.value = Vec3::Zero();

    input.environment.context = sample_context(kInertialFrame);
    input.environment.gravity.value = Vec3{0.0, 0.0, -9.80665};
    input.environment.velocity_airmass.value = Vec3{10.0, 0.0, 0.0};
    input.environment.density_kilograms_per_cubic_meter = 1.225;
    input.environment.speed_of_sound_meters_per_second = 340.0;

    input.mass_properties.context = interval_context(kBodyFrame);
    input.mass_properties.mass_state_id = "mass.fixture.yyz.vehicle@1";
    input.mass_properties.mass_kilograms = 100.0;
    input.mass_properties.body_origin_to_center_of_mass.value =
        Vec3{0.2, 0.0, 0.0};
    input.mass_properties.inertia_about_center_of_mass.value = Mat3::Zero();
    input.mass_properties.inertia_about_center_of_mass.value.diagonal() =
        Vec3{10.0, 20.0, 30.0};

    input.supplied_wrench.context = interval_context(kBodyFrame);
    input.supplied_wrench.source_id = "propulsion.main";
    input.supplied_wrench.force.value = Vec3{100.0, 0.0, 0.0};
    input.supplied_wrench.body_origin_to_application.value =
        Vec3{0.2, 0.2, 0.0};
    input.supplied_wrench.intrinsic_moment_at_application.value =
        Vec3{0.0, 0.0, 20.0};
    return input;
}

template <typename Value>
void expect_failure(const NumericalOutcome<Value>& outcome,
                    NumericalStatus expected,
                    std::string_view message) {
    require(!outcome.has_value() && outcome.status() == expected, message);
}

struct ProbeBundle {
    RigidStepOutput accepted;
    std::vector<std::string> direct_checks;
};

ProbeBundle run_probe() {
    const auto prepared_outcome =
        prepare_rigid_step_model(fixture_definition());
    const PreparedRigidStepModel& prepared = require_value(
        prepared_outcome, "YYZ product model preparation failed");
    const RigidStepInput accepted_input = fixture_input();
    const auto accepted_outcome =
        RigidStepKernel::evaluate(prepared, accepted_input);
    const RigidStepOutput& accepted = require_value(
        accepted_outcome, "YYZ product accepted step failed");
    require(accepted.aerodynamic_lookup.domain_status ==
                InterpolationDomainStatus::Inside &&
                near(accepted.air_data.dynamic_pressure_pascals, 6125.0) &&
                near(accepted.air_data.mach, 5.0 / 17.0) &&
                near(accepted.aerodynamic_lookup.weights[0], 4.0 / 17.0) &&
                near(accepted.aerodynamic_lookup.weights[1], 0.5) &&
                near(accepted.aerodynamic_lookup.weights[2], 0.5) &&
                near(accepted.aerodynamic_lookup
                         .coefficients_ca_cy_cn_cl_cm_cn[0],
                     27.0 / 850.0) &&
                near(accepted.aerodynamic_lookup
                         .coefficients_ca_cy_cn_cl_cm_cn[4],
                     -3.0 / 68.0) &&
                near(accepted.force_total.value,
                     Vec3{-3215.0 / 34.0, 0.0, 0.0}) &&
                near(accepted.moment_total_about_center_of_mass.value,
                     Vec3::Zero()) &&
                near(accepted.candidate.state.position.value,
                     Vec3{10.995272058823529, 0.0, 999.95096675}) &&
                near(accepted.candidate.state.velocity.value,
                     Vec3{109.90544117647059, 0.0, -0.980665}),
            "YYZ product accepted result differs from oracle anchors");

    std::vector<std::string> checks{"accepted-oracle-anchors"};

    RigidStepInput rotated = accepted_input;
    const double root_half = std::sqrt(0.5);
    rotated.committed_state.attitude.value =
        gnc::foundation::quaternion_from_wxyz(
            root_half, 0.0, 0.0, root_half);
    rotated.committed_state.velocity.value = Vec3{0.0, -100.0, 0.0};
    rotated.environment.velocity_airmass.value = Vec3::Zero();
    const auto rotated_outcome = RigidStepKernel::evaluate(prepared, rotated);
    const auto& rotated_output = require_value(
        rotated_outcome, "passive frame-direction case failed");
    require(near(rotated_output.air_data.velocity_relative_body.value,
                 Vec3{100.0, 0.0, 0.0}) &&
                near(rotated_output.derivative_at_interval_start
                         .force_total_inertial.value,
                     Vec3{0.0, 3215.0 / 34.0, 0.0}),
            "passive frame direction differs");
    checks.emplace_back("passive-frame-direction");

    RigidStepInput boundary = accepted_input;
    boundary.committed_state.velocity.value = Vec3{68.0, 0.0, 0.0};
    boundary.environment.velocity_airmass.value = Vec3::Zero();
    const auto boundary_outcome =
        RigidStepKernel::evaluate(prepared, boundary);
    const auto& boundary_output = require_value(
        boundary_outcome, "inclusive aero boundary case failed");
    require(boundary_output.aerodynamic_lookup.domain_status ==
                InterpolationDomainStatus::Boundary &&
                near(boundary_output.aerodynamic_lookup.weights[0], 0.0),
            "inclusive aero boundary semantics differ");
    checks.emplace_back("inclusive-table-boundary");

    RigidStepInput out_of_range = accepted_input;
    out_of_range.committed_state.velocity.value = Vec3{300.0, 0.0, 0.0};
    expect_failure(RigidStepKernel::evaluate(prepared, out_of_range),
                   NumericalStatus::OutOfRange,
                   "out-of-range aero query survived");
    checks.emplace_back("strict-table-domain");

    RigidStepInput frame_mismatch = accepted_input;
    frame_mismatch.environment.context.frame.id =
        "frame.fixture.yyz.other@1";
    expect_failure(RigidStepKernel::evaluate(prepared, frame_mismatch),
                   NumericalStatus::DomainError,
                   "environment frame mismatch survived");
    checks.emplace_back("frame-context-rejection");

    RigidStepInput time_mismatch = accepted_input;
    time_mismatch.mass_properties.context.validity.effective_until.tick = 2;
    expect_failure(RigidStepKernel::evaluate(prepared, time_mismatch),
                   NumericalStatus::DomainError,
                   "mass validity mismatch survived");
    checks.emplace_back("time-context-rejection");

    RigidStepInput invalid_quality = accepted_input;
    invalid_quality.supplied_wrench.context.sample.quality =
        DataQuality::Invalid;
    expect_failure(RigidStepKernel::evaluate(prepared, invalid_quality),
                   NumericalStatus::DomainError,
                   "invalid wrench quality survived");
    checks.emplace_back("quality-rejection");

    RigidStepInput invalid_mass = accepted_input;
    invalid_mass.mass_properties.mass_kilograms = 0.0;
    expect_failure(RigidStepKernel::evaluate(prepared, invalid_mass),
                   NumericalStatus::DomainError,
                   "nonpositive mass survived");
    checks.emplace_back("mass-rejection");

    RigidStepInput invalid_inertia = accepted_input;
    invalid_inertia.mass_properties.inertia_about_center_of_mass.value(2, 2) =
        -1.0;
    expect_failure(RigidStepKernel::evaluate(prepared, invalid_inertia),
                   NumericalStatus::DomainError,
                   "indefinite inertia survived");
    checks.emplace_back("inertia-rejection");

    RigidStepInput zero_quaternion = accepted_input;
    zero_quaternion.committed_state.attitude.value =
        gnc::foundation::quaternion_from_wxyz(0.0, 0.0, 0.0, 0.0);
    expect_failure(RigidStepKernel::evaluate(prepared, zero_quaternion),
                   NumericalStatus::DomainError,
                   "zero quaternion survived");
    checks.emplace_back("quaternion-rejection");

    RigidStepInput nonfinite = accepted_input;
    nonfinite.environment.density_kilograms_per_cubic_meter =
        std::numeric_limits<double>::quiet_NaN();
    expect_failure(RigidStepKernel::evaluate(prepared, nonfinite),
                   NumericalStatus::NonFiniteInput,
                   "non-finite environment input survived");
    checks.emplace_back("nonfinite-rejection");

    RigidStepModelDefinition invalid_table = fixture_definition();
    invalid_table.aerodynamics.mach_axis = {0.2, 0.2};
    expect_failure(prepare_rigid_step_model(std::move(invalid_table)),
                   NumericalStatus::DomainError,
                   "invalid prepared table survived");
    checks.emplace_back("table-preparation-rejection");

    RigidStepInput full_inertia = accepted_input;
    Mat3 coupled_inertia;
    coupled_inertia << 4.0, 1.0, 0.0,
                       1.0, 3.0, 0.0,
                       0.0, 0.0, 2.0;
    full_inertia.mass_properties.inertia_about_center_of_mass.value =
        coupled_inertia;
    full_inertia.supplied_wrench.intrinsic_moment_at_application.value =
        Vec3{9.0, -12.0, 25.0};
    const auto full_inertia_outcome =
        RigidStepKernel::evaluate(prepared, full_inertia);
    const auto& full_inertia_output = require_value(
        full_inertia_outcome, "full-inertia derivative case failed");
    require(near(full_inertia_output.derivative_at_interval_start
                     .angular_acceleration.value,
                 Vec3{39.0 / 11.0, -57.0 / 11.0, 2.5}),
            "full-inertia angular acceleration differs");
    checks.emplace_back("full-inertia-derivative");

    RigidStepInput stage_failure = accepted_input;
    stage_failure.mass_properties.inertia_about_center_of_mass.value =
        Mat3::Zero();
    stage_failure.mass_properties.inertia_about_center_of_mass.value.diagonal() =
        Vec3{1.0, 2.0, 3.0};
    stage_failure.committed_state.angular_rate.value =
        Vec3{1.0e154, 1.0e154, 0.0};
    const auto failed_stage =
        RigidStepKernel::evaluate(prepared, stage_failure);
    expect_failure(failed_stage, NumericalStatus::NonFiniteIntermediate,
                   "RK4 stage failure retained a candidate");
    checks.emplace_back("rk4-stage-discards-candidate");

    return {accepted, std::move(checks)};
}

void write_number(double value) {
    std::cout << (value == 0.0 ? 0.0 : value);
}

void write_vec3(const Vec3& value) {
    std::cout << '[';
    write_number(value(0));
    std::cout << ',';
    write_number(value(1));
    std::cout << ',';
    write_number(value(2));
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

template <std::size_t Size>
void write_array(const std::array<double, Size>& values) {
    std::cout << '[';
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        write_number(values[index]);
    }
    std::cout << ']';
}

void write_json(const ProbeBundle& bundle) {
    const auto& output = bundle.accepted;
    std::cout << std::setprecision(17)
              << "{\"schema_version\":\"gnczmkn.yyz-rigid-step-product-probe/1\""
              << ",\"product_model_id\":\"" << kRigidStepModelIdentity
              << "\",\"contract_id\":\"" << kRigidStepContractIdentity
              << "\",\"source_fixture_id\":\"" << kFixtureId
              << "\",\"source_oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\",\"accepted\":{";
    std::cout << "\"air_data\":{\"velocity_relative_I_mps\":";
    write_vec3(output.air_data.velocity_relative_inertial.value);
    std::cout << ",\"velocity_relative_B_mps\":";
    write_vec3(output.air_data.velocity_relative_body.value);
    std::cout << ",\"airspeed_mps\":";
    write_number(output.air_data.airspeed_meters_per_second);
    std::cout << ",\"alpha_rad\":";
    write_number(output.air_data.alpha_radians);
    std::cout << ",\"beta_rad\":";
    write_number(output.air_data.beta_radians);
    std::cout << ",\"dynamic_pressure_Pa\":";
    write_number(output.air_data.dynamic_pressure_pascals);
    std::cout << ",\"mach\":";
    write_number(output.air_data.mach);
    std::cout << "},\"aero_lookup\":{\"domain_status\":\""
              << gnc::foundation::to_string(
                     output.aerodynamic_lookup.domain_status)
              << "\",\"weights_M_alpha_beta\":";
    write_array(output.aerodynamic_lookup.weights);
    std::cout << ",\"coefficients_CA_CY_CN_Cl_Cm_Cn\":";
    write_array(output.aerodynamic_lookup
                    .coefficients_ca_cy_cn_cl_cm_cn);
    std::cout << "},\"aero_contribution\":{\"source_id\":\""
              << output.aerodynamic_contribution.source_id
              << "\",\"force_B_N\":";
    write_vec3(output.aerodynamic_contribution.force.value);
    std::cout << ",\"moment_about_CoM_B_Nm\":";
    write_vec3(output.aerodynamic_contribution
                   .moment_about_center_of_mass.value);
    std::cout << "},\"supplied_contribution\":{\"source_id\":\""
              << output.supplied_contribution.source_id
              << "\",\"force_B_N\":";
    write_vec3(output.supplied_contribution.force.value);
    std::cout << ",\"moment_about_CoM_B_Nm\":";
    write_vec3(output.supplied_contribution
                   .moment_about_center_of_mass.value);
    std::cout << "},\"closure\":{\"force_total_B_N\":";
    write_vec3(output.force_total.value);
    std::cout << ",\"moment_total_about_CoM_B_Nm\":";
    write_vec3(output.moment_total_about_center_of_mass.value);
    std::cout << "},\"rigid_derivative_at_tick0\":{\"force_total_I_N\":";
    write_vec3(output.derivative_at_interval_start
                   .force_total_inertial.value);
    std::cout << ",\"acceleration_I_mps2\":";
    write_vec3(output.derivative_at_interval_start.acceleration.value);
    std::cout << ",\"angular_momentum_B_kgm2ps\":";
    write_vec3(output.derivative_at_interval_start.angular_momentum.value);
    std::cout << ",\"gyroscopic_moment_B_Nm\":";
    write_vec3(output.derivative_at_interval_start
                   .gyroscopic_moment.value);
    std::cout << ",\"net_moment_B_Nm\":";
    write_vec3(output.derivative_at_interval_start.net_moment.value);
    std::cout << ",\"angular_acceleration_B_radps2\":";
    write_vec3(output.derivative_at_interval_start
                   .angular_acceleration.value);
    std::cout << ",\"q_derivative_I_B_per_s\":";
    write_quaternion(output.derivative_at_interval_start
                         .attitude_derivative.value);
    std::cout << "},\"candidate\":{\"tick\":"
              << output.candidate.effective_at.tick << ",\"time_s\":";
    write_number(output.candidate.effective_at.seconds);
    std::cout << ",\"position_I_m\":";
    write_vec3(output.candidate.state.position.value);
    std::cout << ",\"velocity_I_mps\":";
    write_vec3(output.candidate.state.velocity.value);
    std::cout << ",\"q_I_B_wxyz\":";
    write_quaternion(output.candidate.state.attitude.value);
    std::cout << ",\"omega_BI_B_radps\":";
    write_vec3(output.candidate.state.angular_rate.value);
    std::cout << "}},\"direct_checks\":[";
    for (std::size_t index = 0U; index < bundle.direct_checks.size();
         ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << '\"' << bundle.direct_checks[index] << '\"';
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_yyz_rigid_step_product_probe --self-check\n";
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

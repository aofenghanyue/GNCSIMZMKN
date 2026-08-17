#include <yyz/mass_commit.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
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

[[nodiscard]] SimulationInstant instant(std::int64_t tick) {
    return {tick, 0.1 * static_cast<double>(tick)};
}

[[nodiscard]] SampleContext sample_context(
    std::string_view frame, std::int64_t tick) {
    return {
        FrameIdentity{std::string(frame)},
        ClockDomainIdentity{std::string(kClock)},
        instant(tick),
        11,
        DataQuality::Valid,
    };
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

struct ProbeBundle {
    TwoIntervalMassCommitOutput accepted;
    std::vector<std::string> direct_checks;
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

    return {accepted, std::move(checks)};
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

void write_json(const ProbeBundle& bundle) {
    const auto& terminal = bundle.accepted.terminal_boundary;
    std::cout << std::setprecision(17)
              << "{\"schema_version\":\"gnczmkn.yyz-two-interval-mass-commit-product-probe/1\""
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
    std::cout << "]}\n";
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

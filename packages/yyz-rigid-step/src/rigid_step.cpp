#include "../include/yyz/rigid_step.hpp"

#include "gnc/foundation/fixed_rk4.hpp"
#include "gnc/foundation/spd_cholesky_3x3.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace gnc::packages::yyz {
namespace {

using gnc::contracts::DataQuality;
using gnc::contracts::IntervalSampleContext;
using gnc::contracts::SampleContext;
using gnc::contracts::SimulationInstant;
using gnc::foundation::FiniteCheck;
using gnc::foundation::Mat3;
using gnc::foundation::NumericalEvidence;
using gnc::foundation::NumericalFlags;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;
using gnc::foundation::PreparedTrilinearTableView;
using gnc::foundation::QuaternionStorage;
using gnc::foundation::Vec3;

using StateVector = std::array<double, 13U>;

struct ValidationFailure {
    NumericalStatus status = NumericalStatus::DomainError;
    std::string_view detail;
};

struct AirLookupComputation {
    AirDataOutput air_data;
    AerodynamicLookupOutput lookup;
};

[[nodiscard]] NumericalEvidence product_evidence(
    gnc::foundation::AlgorithmIdentity algorithm, std::string_view detail,
    NumericalFlags flags = 0U) {
    NumericalEvidence evidence;
    evidence.algorithm = algorithm;
    evidence.detail = detail;
    evidence.flags = flags;
    return evidence;
}

template <typename Value>
[[nodiscard]] NumericalOutcome<Value> product_failure(
    NumericalStatus status, std::string_view detail,
    NumericalFlags flags = 0U) {
    return NumericalOutcome<Value>::failure(
        status, product_evidence(kRigidStepKernelIdentity, detail, flags));
}

[[nodiscard]] bool finite(const Vec3& value) noexcept {
    return std::isfinite(value(0)) && std::isfinite(value(1)) &&
           std::isfinite(value(2));
}

[[nodiscard]] bool finite(const Mat3& value) noexcept {
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = 0; column < 3; ++column) {
            if (!std::isfinite(value(row, column))) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool finite(const QuaternionStorage& value) noexcept {
    const auto coefficients = gnc::foundation::quaternion_to_wxyz(value);
    return std::all_of(coefficients.begin(), coefficients.end(),
                       [](double coefficient) {
                           return std::isfinite(coefficient);
                       });
}

[[nodiscard]] bool finite(const RigidState& state) noexcept {
    return finite(state.position.value) && finite(state.velocity.value) &&
           finite(state.attitude.value) && finite(state.angular_rate.value);
}

[[nodiscard]] bool near(double lhs, double rhs,
                        const NumericalPolicy& policy) noexcept {
    if (!std::isfinite(lhs) || !std::isfinite(rhs)) {
        return false;
    }
    const double scale = std::max(std::abs(lhs), std::abs(rhs));
    const double limit = policy.absolute_tolerance +
                         policy.relative_tolerance * scale;
    return std::isfinite(limit) && std::abs(lhs - rhs) <= limit;
}

[[nodiscard]] bool same_instant(const SimulationInstant& lhs,
                                const SimulationInstant& rhs,
                                const NumericalPolicy& policy) noexcept {
    return lhs.tick == rhs.tick && near(lhs.seconds, rhs.seconds, policy);
}

[[nodiscard]] bool valid_sample_context(
    const SampleContext& context,
    const gnc::contracts::FrameIdentity& expected_frame,
    const gnc::contracts::ClockDomainIdentity& expected_clock,
    const SimulationInstant& expected_time,
    std::int64_t expected_revision,
    const NumericalPolicy& policy) noexcept {
    return context.frame == expected_frame &&
           context.clock_domain == expected_clock &&
           same_instant(context.sample_time, expected_time, policy) &&
           context.configuration_revision == expected_revision &&
           context.quality == DataQuality::Valid;
}

[[nodiscard]] bool valid_interval_context(
    const IntervalSampleContext& context,
    const gnc::contracts::FrameIdentity& expected_frame,
    const RigidStepContext& step,
    const NumericalPolicy& policy) noexcept {
    return valid_sample_context(
               context.sample, expected_frame, step.clock_domain,
               step.interval_start, step.configuration_revision, policy) &&
           same_instant(context.validity.effective_from,
                        step.interval_start, policy) &&
           same_instant(context.validity.effective_until,
                        step.interval_end, policy);
}

[[nodiscard]] std::optional<ValidationFailure> validate_contexts(
    const RigidStepModelDefinition& definition,
    const RigidStepInput& input) {
    const auto& policy = definition.algorithm.numerical_policy;
    const auto& step = input.context;
    if (step.inertial_frame != definition.inertial_frame ||
        step.body_frame != definition.body_frame ||
        step.clock_domain != definition.metadata.clock_domain) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "step-frame-or-clock"};
    }
    if (step.configuration_revision !=
            definition.metadata.configuration_revision ||
        step.quality != DataQuality::Valid) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "step-revision-or-quality"};
    }
    if (!std::isfinite(step.interval_start.seconds) ||
        !std::isfinite(step.interval_end.seconds)) {
        return ValidationFailure{NumericalStatus::NonFiniteInput,
                                 "step-time"};
    }
    if (step.interval_start.tick < 0 ||
        step.interval_start.tick ==
            std::numeric_limits<std::int64_t>::max() ||
        step.interval_end.tick != step.interval_start.tick + 1) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "step-ticks"};
    }
    const double duration =
        step.interval_end.seconds - step.interval_start.seconds;
    if (!std::isfinite(duration) || duration <= 0.0 ||
        !near(duration, definition.algorithm.fixed_step_seconds, policy)) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "step-duration"};
    }
    if (!valid_sample_context(
            input.environment.context, definition.inertial_frame,
            definition.metadata.clock_domain, step.interval_start,
            definition.metadata.configuration_revision, policy)) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "environment-context"};
    }
    if (!valid_interval_context(input.mass_properties.context,
                                definition.body_frame, step, policy)) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "mass-context"};
    }
    if (!valid_interval_context(input.supplied_wrench.context,
                                definition.body_frame, step, policy)) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "wrench-context"};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<ValidationFailure> validate_values(
    const RigidStepModelDefinition& definition,
    const RigidStepInput& input) {
    if (!finite(input.committed_state) ||
        !finite(input.environment.gravity.value) ||
        !finite(input.environment.velocity_airmass.value) ||
        !std::isfinite(input.environment.density_kilograms_per_cubic_meter) ||
        !std::isfinite(
            input.environment.speed_of_sound_meters_per_second) ||
        !std::isfinite(input.mass_properties.mass_kilograms) ||
        !finite(input.mass_properties.body_origin_to_center_of_mass.value) ||
        !finite(input.mass_properties.inertia_about_center_of_mass.value) ||
        !finite(input.supplied_wrench.force.value) ||
        !finite(input.supplied_wrench.body_origin_to_application.value) ||
        !finite(input.supplied_wrench.intrinsic_moment_at_application.value)) {
        return ValidationFailure{NumericalStatus::NonFiniteInput,
                                 "physical-input"};
    }
    if (input.environment.density_kilograms_per_cubic_meter < 0.0 ||
        input.environment.speed_of_sound_meters_per_second <= 0.0 ||
        input.mass_properties.mass_kilograms <= 0.0 ||
        input.mass_properties.mass_state_id.empty() ||
        input.supplied_wrench.source_id.empty() ||
        input.supplied_wrench.source_id ==
            definition.aerodynamics.source_id) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "physical-domain"};
    }
    return std::nullopt;
}

[[nodiscard]] bool approximate_status(NumericalStatus status) noexcept {
    return status == NumericalStatus::Approximate ||
           status == NumericalStatus::Extrapolated;
}

[[nodiscard]] NumericalOutcome<AirLookupComputation> compute_air_lookup(
    const RigidStepModelDefinition& definition,
    const PreparedTrilinearTableView<6U>& table,
    const RigidState& state,
    const EnvironmentInput& environment) {
    NumericalFlags flags = 0U;
    bool approximate = false;
    const auto& quaternion_policy =
        definition.algorithm.attitude_evaluation_policy;

    const auto prepared_attitude = gnc::foundation::prepare_passive_quaternion(
        state.attitude.value, quaternion_policy);
    if (!prepared_attitude.has_value()) {
        return product_failure<AirLookupComputation>(
            prepared_attitude.status(), "air-attitude",
            prepared_attitude.evidence().flags);
    }
    flags |= prepared_attitude.evidence().flags;
    approximate = approximate || approximate_status(prepared_attitude.status());

    const Vec3 relative_inertial =
        state.velocity.value - environment.velocity_airmass.value;
    if (!finite(relative_inertial)) {
        return product_failure<AirLookupComputation>(
            NumericalStatus::NonFiniteIntermediate, "relative-air", flags);
    }
    const auto inverse_attitude =
        gnc::foundation::inverse_passive_quaternion(
            prepared_attitude.value(), quaternion_policy);
    if (!inverse_attitude.has_value()) {
        return product_failure<AirLookupComputation>(
            inverse_attitude.status(), "air-attitude-inverse",
            flags | inverse_attitude.evidence().flags);
    }
    flags |= inverse_attitude.evidence().flags;
    approximate = approximate || approximate_status(inverse_attitude.status());
    const auto relative_body = gnc::foundation::rotate_passive(
        inverse_attitude.value(), relative_inertial, quaternion_policy);
    if (!relative_body.has_value()) {
        return product_failure<AirLookupComputation>(
            relative_body.status(), "air-frame-transform",
            flags | relative_body.evidence().flags);
    }
    flags |= relative_body.evidence().flags;
    approximate = approximate || approximate_status(relative_body.status());

    const Vec3 velocity_body = relative_body.value();
    const double airspeed = std::hypot(
        std::hypot(velocity_body(0), velocity_body(1)), velocity_body(2));
    if (!std::isfinite(airspeed)) {
        return product_failure<AirLookupComputation>(
            NumericalStatus::NonFiniteIntermediate, "airspeed", flags);
    }
    if (airspeed <= 0.0) {
        return product_failure<AirLookupComputation>(
            NumericalStatus::DomainError, "zero-airspeed", flags);
    }
    const double horizontal =
        std::hypot(velocity_body(0), velocity_body(2));
    const double alpha = std::atan2(velocity_body(2), velocity_body(0));
    const double beta = std::atan2(velocity_body(1), horizontal);
    const double dynamic_pressure =
        0.5 * environment.density_kilograms_per_cubic_meter *
        airspeed * airspeed;
    const double mach =
        airspeed / environment.speed_of_sound_meters_per_second;
    if (!std::isfinite(alpha) || !std::isfinite(beta) ||
        !std::isfinite(dynamic_pressure) || !std::isfinite(mach)) {
        return product_failure<AirLookupComputation>(
            NumericalStatus::NonFiniteIntermediate, "air-data", flags);
    }

    const auto query = gnc::foundation::query_trilinear_strict(
        table, mach, alpha, beta);
    if (!query.has_value()) {
        return product_failure<AirLookupComputation>(
            query.status(), "aero-query", flags | query.evidence().flags);
    }
    flags |= query.evidence().flags;
    approximate = approximate || approximate_status(query.status());
    const auto& result = query.value();

    AirLookupComputation output;
    output.air_data.velocity_relative_inertial.value = relative_inertial;
    output.air_data.velocity_relative_body.value = velocity_body;
    output.air_data.airspeed_meters_per_second = airspeed;
    output.air_data.alpha_radians = alpha;
    output.air_data.beta_radians = beta;
    output.air_data.dynamic_pressure_pascals = dynamic_pressure;
    output.air_data.mach = mach;
    output.lookup.domain_status = result.domain_status;
    output.lookup.weights = {
        result.x_bracket.weight,
        result.y_bracket.weight,
        result.z_bracket.weight,
    };
    output.lookup.coefficients_ca_cy_cn_cl_cm_cn = result.values;
    NumericalEvidence evidence = product_evidence(
        kRigidStepKernelIdentity, "air-and-aero-query", flags);
    evidence.evaluations = query.evidence().evaluations;
    return NumericalOutcome<AirLookupComputation>::with_value(
        approximate ? NumericalStatus::Approximate
                    : NumericalStatus::Success,
        std::move(output), evidence);
}

[[nodiscard]] BodyWrenchContribution make_contribution(
    std::string source_id, const Vec3& force,
    const Vec3& body_origin_to_application,
    const Vec3& body_origin_to_center_of_mass,
    const Vec3& moment_at_application) {
    const Vec3 lever =
        body_origin_to_application - body_origin_to_center_of_mass;
    const Vec3 transport = lever.cross(force);
    return {
        std::move(source_id),
        BodyForceNewtons{force},
        BodyMomentNewtonMeters{moment_at_application + transport},
    };
}

[[nodiscard]] NumericalOutcome<RigidDerivativeOutput> evaluate_derivative(
    const RigidStepAlgorithmDefinition& algorithm,
    const RigidState& state,
    double mass_kilograms,
    const Mat3& inertia_about_center_of_mass,
    const Vec3& force_total_body,
    const Vec3& moment_total_body,
    const Vec3& gravity_inertial) {
    NumericalFlags flags = 0U;
    bool approximate = false;
    const auto prepared_attitude = gnc::foundation::prepare_passive_quaternion(
        state.attitude.value, algorithm.attitude_evaluation_policy);
    if (!prepared_attitude.has_value()) {
        return product_failure<RigidDerivativeOutput>(
            prepared_attitude.status(), "derivative-attitude",
            prepared_attitude.evidence().flags);
    }
    flags |= prepared_attitude.evidence().flags;
    approximate = approximate || approximate_status(prepared_attitude.status());

    const auto force_inertial = gnc::foundation::rotate_passive(
        prepared_attitude.value(), force_total_body,
        algorithm.attitude_evaluation_policy);
    if (!force_inertial.has_value()) {
        return product_failure<RigidDerivativeOutput>(
            force_inertial.status(), "derivative-force-frame",
            flags | force_inertial.evidence().flags);
    }
    flags |= force_inertial.evidence().flags;
    approximate = approximate || approximate_status(force_inertial.status());
    const Vec3 acceleration =
        force_inertial.value() / mass_kilograms + gravity_inertial;
    const Vec3 angular_momentum =
        inertia_about_center_of_mass * state.angular_rate.value;
    const Vec3 gyroscopic = state.angular_rate.value.cross(angular_momentum);
    const Vec3 net_moment = moment_total_body - gyroscopic;
    if (!finite(acceleration) || !finite(angular_momentum) ||
        !finite(gyroscopic) || !finite(net_moment)) {
        return product_failure<RigidDerivativeOutput>(
            NumericalStatus::NonFiniteIntermediate,
            "derivative-balance", flags);
    }

    const auto angular_acceleration = gnc::foundation::solve_spd_3x3(
        inertia_about_center_of_mass, net_moment,
        algorithm.numerical_policy);
    if (!angular_acceleration.has_value()) {
        return product_failure<RigidDerivativeOutput>(
            angular_acceleration.status(), "derivative-inertia",
            flags | angular_acceleration.evidence().flags);
    }
    flags |= angular_acceleration.evidence().flags;
    approximate = approximate ||
                  approximate_status(angular_acceleration.status());

    const auto attitude_derivative =
        gnc::foundation::passive_quaternion_body_rate_derivative(
            prepared_attitude.value(), state.angular_rate.value,
            algorithm.attitude_evaluation_policy);
    if (!attitude_derivative.has_value()) {
        return product_failure<RigidDerivativeOutput>(
            attitude_derivative.status(), "derivative-attitude-rate",
            flags | attitude_derivative.evidence().flags);
    }
    flags |= attitude_derivative.evidence().flags;
    approximate = approximate || approximate_status(
        attitude_derivative.status());

    RigidDerivativeOutput output;
    output.force_total_inertial.value = force_inertial.value();
    output.acceleration.value = acceleration;
    output.angular_momentum.value = angular_momentum;
    output.gyroscopic_moment.value = gyroscopic;
    output.net_moment.value = net_moment;
    output.angular_acceleration.value = angular_acceleration.value().solution;
    output.attitude_derivative.value = attitude_derivative.value();
    NumericalEvidence evidence = product_evidence(
        kRigidStepKernelIdentity, "rigid-derivative", flags);
    return NumericalOutcome<RigidDerivativeOutput>::with_value(
        approximate ? NumericalStatus::Approximate
                    : NumericalStatus::Success,
        std::move(output), evidence);
}

[[nodiscard]] StateVector pack_state(const RigidState& state) {
    const auto quaternion =
        gnc::foundation::quaternion_to_wxyz(state.attitude.value);
    return {
        state.position.value(0),
        state.position.value(1),
        state.position.value(2),
        state.velocity.value(0),
        state.velocity.value(1),
        state.velocity.value(2),
        quaternion[0],
        quaternion[1],
        quaternion[2],
        quaternion[3],
        state.angular_rate.value(0),
        state.angular_rate.value(1),
        state.angular_rate.value(2),
    };
}

[[nodiscard]] RigidState unpack_state(const StateVector& value) {
    RigidState state;
    state.position.value = Vec3{value[0], value[1], value[2]};
    state.velocity.value = Vec3{value[3], value[4], value[5]};
    state.attitude.value = gnc::foundation::quaternion_from_wxyz(
        value[6], value[7], value[8], value[9]);
    state.angular_rate.value = Vec3{value[10], value[11], value[12]};
    return state;
}

[[nodiscard]] StateVector pack_derivative(
    const RigidState& state,
    const RigidDerivativeOutput& derivative) {
    const auto quaternion_derivative =
        gnc::foundation::quaternion_to_wxyz(
            derivative.attitude_derivative.value);
    return {
        state.velocity.value(0),
        state.velocity.value(1),
        state.velocity.value(2),
        derivative.acceleration.value(0),
        derivative.acceleration.value(1),
        derivative.acceleration.value(2),
        quaternion_derivative[0],
        quaternion_derivative[1],
        quaternion_derivative[2],
        quaternion_derivative[3],
        derivative.angular_acceleration.value(0),
        derivative.angular_acceleration.value(1),
        derivative.angular_acceleration.value(2),
    };
}

} // namespace

PreparedRigidStepModel::PreparedRigidStepModel(
    std::shared_ptr<const RigidStepModelDefinition> definition,
    PreparedTrilinearTableView<6U> table,
    gnc::model_sdk::PreparedModelMetadata metadata) noexcept
    : definition_(std::move(definition)), table_(std::move(table)),
      metadata_(std::move(metadata)) {}

const RigidStepModelDefinition& PreparedRigidStepModel::definition()
    const noexcept {
    return *definition_;
}

const gnc::model_sdk::PreparedModelMetadata&
PreparedRigidStepModel::metadata() const noexcept {
    return metadata_;
}

NumericalOutcome<PreparedRigidStepModel> prepare_rigid_step_model(
    RigidStepModelDefinition definition) {
    const auto failure = [](NumericalStatus status, std::string_view detail) {
        return NumericalOutcome<PreparedRigidStepModel>::failure(
            status,
            product_evidence(kRigidStepPreparationIdentity, detail));
    };

    auto metadata = gnc::model_sdk::prepare_model_metadata(
        definition.metadata, kRigidStepPreparationIdentity);
    if (!metadata.has_value()) {
        return NumericalOutcome<PreparedRigidStepModel>::failure(
            metadata.status(), metadata.evidence());
    }

    if (definition.metadata.model_id != kRigidStepModelIdentity ||
        definition.metadata.execution_form !=
            gnc::model_sdk::ModelExecutionForm::Closure ||
        definition.inertial_frame.id.empty() ||
        definition.body_frame.id.empty() ||
        definition.inertial_frame == definition.body_frame ||
        definition.aerodynamics.source_id.empty() ||
        definition.aerodynamics.table_id.empty() ||
        definition.aerodynamics.configuration_id.empty()) {
        return failure(NumericalStatus::DomainError, "definition-identity");
    }
    if (!std::isfinite(definition.algorithm.fixed_step_seconds) ||
        definition.algorithm.fixed_step_seconds <= 0.0 ||
        !gnc::foundation::valid_numerical_policy(
            definition.algorithm.numerical_policy) ||
        !gnc::foundation::valid_quaternion_policy(
            definition.algorithm.attitude_evaluation_policy) ||
        !gnc::foundation::valid_quaternion_policy(
            definition.algorithm.candidate_attitude_policy)) {
        return failure(NumericalStatus::DomainError,
                       "definition-numerical-policy");
    }
    const auto& aero = definition.aerodynamics;
    if (!std::isfinite(aero.reference_area_square_meters) ||
        !std::isfinite(aero.reference_span_meters) ||
        !std::isfinite(aero.reference_chord_meters) ||
        aero.reference_area_square_meters <= 0.0 ||
        aero.reference_span_meters <= 0.0 ||
        aero.reference_chord_meters <= 0.0 ||
        !finite(aero.body_origin_to_application.value)) {
        return failure(NumericalStatus::DomainError,
                       "definition-aero-geometry");
    }

    auto owned_definition =
        std::make_shared<const RigidStepModelDefinition>(
            std::move(definition));
    const auto& owned_aero = owned_definition->aerodynamics;
    gnc::foundation::TrilinearTableView<6U> view;
    view.x_axis = {owned_aero.mach_axis.data(),
                   owned_aero.mach_axis.size()};
    view.y_axis = {owned_aero.alpha_axis_radians.data(),
                   owned_aero.alpha_axis_radians.size()};
    view.z_axis = {owned_aero.beta_axis_radians.data(),
                   owned_aero.beta_axis_radians.size()};
    view.rows = owned_aero.coefficient_rows_ca_cy_cn_cl_cm_cn.data();
    view.row_count =
        owned_aero.coefficient_rows_ca_cy_cn_cl_cm_cn.size();
    auto table = gnc::foundation::prepare_trilinear_table(view);
    if (!table.has_value()) {
        NumericalEvidence evidence = table.evidence();
        evidence.algorithm = kRigidStepPreparationIdentity;
        evidence.detail = "aero-table";
        return NumericalOutcome<PreparedRigidStepModel>::failure(
            table.status(), evidence);
    }

    NumericalEvidence evidence = table.evidence();
    evidence.algorithm = kRigidStepPreparationIdentity;
    evidence.detail = "prepared-aero-table";
    return NumericalOutcome<PreparedRigidStepModel>::with_value(
        table.status(),
        PreparedRigidStepModel{std::move(owned_definition), table.value(),
                               std::move(metadata.value())},
        evidence);
}

NumericalOutcome<RigidStepEvaluation> RigidStepKernel::evaluate(
    const PreparedRigidStepModel& model,
    const RigidStepInput& input) {
    const auto& definition = model.definition();
    if (const auto failure = validate_contexts(definition, input)) {
        return product_failure<RigidStepEvaluation>(failure->status,
                                                    failure->detail);
    }
    if (const auto failure = validate_values(definition, input)) {
        return product_failure<RigidStepEvaluation>(failure->status,
                                                    failure->detail);
    }

    NumericalFlags flags = 0U;
    bool approximate = false;
    const auto inertia_check = gnc::foundation::solve_spd_3x3(
        input.mass_properties.inertia_about_center_of_mass.value,
        Vec3::Zero(), definition.algorithm.numerical_policy);
    if (!inertia_check.has_value()) {
        return product_failure<RigidStepEvaluation>(
            inertia_check.status(), "mass-inertia",
            inertia_check.evidence().flags);
    }
    flags |= inertia_check.evidence().flags;
    approximate = approximate || approximate_status(inertia_check.status());

    const auto air_lookup = compute_air_lookup(
        definition, model.table_, input.committed_state,
        input.environment);
    if (!air_lookup.has_value()) {
        return product_failure<RigidStepEvaluation>(
            air_lookup.status(), air_lookup.evidence().detail,
            flags | air_lookup.evidence().flags);
    }
    flags |= air_lookup.evidence().flags;
    approximate = approximate || approximate_status(air_lookup.status());
    const auto& air = air_lookup.value().air_data;
    const auto& lookup = air_lookup.value().lookup;

    const auto& coefficients = lookup.coefficients_ca_cy_cn_cl_cm_cn;
    const double pressure_area =
        air.dynamic_pressure_pascals *
        definition.aerodynamics.reference_area_square_meters;
    const Vec3 aerodynamic_force{
        -pressure_area * coefficients[0],
        pressure_area * coefficients[1],
        -pressure_area * coefficients[2],
    };
    const Vec3 aerodynamic_moment_at_application{
        pressure_area * definition.aerodynamics.reference_span_meters *
            coefficients[3],
        pressure_area * definition.aerodynamics.reference_chord_meters *
            coefficients[4],
        pressure_area * definition.aerodynamics.reference_span_meters *
            coefficients[5],
    };
    if (!finite(aerodynamic_force) ||
        !finite(aerodynamic_moment_at_application)) {
        return product_failure<RigidStepEvaluation>(
            NumericalStatus::NonFiniteIntermediate,
            "aero-dimensionalization", flags);
    }

    BodyWrenchContribution aerodynamic = make_contribution(
        definition.aerodynamics.source_id, aerodynamic_force,
        definition.aerodynamics.body_origin_to_application.value,
        input.mass_properties.body_origin_to_center_of_mass.value,
        aerodynamic_moment_at_application);
    BodyWrenchContribution supplied = make_contribution(
        input.supplied_wrench.source_id,
        input.supplied_wrench.force.value,
        input.supplied_wrench.body_origin_to_application.value,
        input.mass_properties.body_origin_to_center_of_mass.value,
        input.supplied_wrench.intrinsic_moment_at_application.value);
    const Vec3 force_total =
        aerodynamic.force.value + supplied.force.value;
    const Vec3 moment_total =
        aerodynamic.moment_about_center_of_mass.value +
        supplied.moment_about_center_of_mass.value;
    if (!finite(force_total) || !finite(moment_total)) {
        return product_failure<RigidStepEvaluation>(
            NumericalStatus::NonFiniteIntermediate,
            "force-moment-closure", flags);
    }

    const auto initial_derivative = evaluate_derivative(
        definition.algorithm, input.committed_state,
        input.mass_properties.mass_kilograms,
        input.mass_properties.inertia_about_center_of_mass.value,
        force_total, moment_total, input.environment.gravity.value);
    if (!initial_derivative.has_value()) {
        return product_failure<RigidStepEvaluation>(
            initial_derivative.status(), initial_derivative.evidence().detail,
            flags | initial_derivative.evidence().flags);
    }
    flags |= initial_derivative.evidence().flags;
    approximate = approximate || approximate_status(
        initial_derivative.status());

    const StateVector committed = pack_state(input.committed_state);
    const auto derivative = [&](double, const StateVector& stage_state) {
        const RigidState typed_state = unpack_state(stage_state);
        const auto stage_derivative = evaluate_derivative(
            definition.algorithm, typed_state,
            input.mass_properties.mass_kilograms,
            input.mass_properties.inertia_about_center_of_mass.value,
            force_total, moment_total, input.environment.gravity.value);
        if (!stage_derivative.has_value()) {
            return NumericalOutcome<StateVector>::failure(
                stage_derivative.status(), stage_derivative.evidence());
        }
        return NumericalOutcome<StateVector>::with_value(
            stage_derivative.status(),
            pack_derivative(typed_state, stage_derivative.value()),
            stage_derivative.evidence());
    };
    const auto integrated = gnc::foundation::fixed_rk4_step(
        committed, input.context.interval_start.seconds,
        definition.algorithm.fixed_step_seconds, derivative,
        definition.algorithm.numerical_policy);
    if (!integrated.has_value()) {
        return product_failure<RigidStepEvaluation>(
            integrated.status(), "rk4", flags | integrated.evidence().flags);
    }
    flags |= integrated.evidence().flags;
    approximate = approximate || approximate_status(integrated.status());

    RigidState candidate_state = unpack_state(integrated.value());
    const auto candidate_attitude =
        gnc::foundation::prepare_passive_quaternion(
            candidate_state.attitude.value,
            definition.algorithm.candidate_attitude_policy);
    if (!candidate_attitude.has_value()) {
        return product_failure<RigidStepEvaluation>(
            candidate_attitude.status(), "candidate-attitude",
            flags | candidate_attitude.evidence().flags);
    }
    flags |= candidate_attitude.evidence().flags;
    approximate = approximate || approximate_status(candidate_attitude.status());
    candidate_state.attitude.value = candidate_attitude.value();
    if (!finite(candidate_state)) {
        return product_failure<RigidStepEvaluation>(
            NumericalStatus::NonFiniteOutput, "candidate", flags);
    }

    RigidStepTelemetry telemetry;
    telemetry.air_data = air;
    telemetry.aerodynamic_lookup = lookup;
    telemetry.aerodynamic_contribution = std::move(aerodynamic);
    telemetry.supplied_contribution = std::move(supplied);
    telemetry.force_total.value = force_total;
    telemetry.moment_total_about_center_of_mass.value = moment_total;
    telemetry.derivative_at_interval_start = initial_derivative.value();
    RigidStepOutput output;
    output.candidate.effective_at = input.context.interval_end;
    output.candidate.state = std::move(candidate_state);
    NumericalEvidence evidence = product_evidence(
        kRigidStepKernelIdentity, "one-step-candidate", flags);
    evidence.evaluations = integrated.evidence().evaluations +
                           air_lookup.evidence().evaluations;
    evidence.last_step = definition.algorithm.fixed_step_seconds;
    return NumericalOutcome<RigidStepEvaluation>::with_value(
        approximate ? NumericalStatus::Approximate
                    : NumericalStatus::Success,
        RigidStepEvaluation{std::move(output), std::move(telemetry)},
        evidence);
}

} // namespace gnc::packages::yyz

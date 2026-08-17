#include "../include/yyz/mass_commit.hpp"

#include "gnc/foundation/spd_cholesky_3x3.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>

namespace gnc::packages::yyz {
namespace {

using gnc::contracts::DataQuality;
using gnc::contracts::IntervalSampleContext;
using gnc::contracts::SampleContext;
using gnc::contracts::SimulationInstant;
using gnc::foundation::Mat3;
using gnc::foundation::NumericalEvidence;
using gnc::foundation::NumericalFlags;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;
using gnc::foundation::Vec3;

[[nodiscard]] NumericalEvidence mass_commit_evidence(
    gnc::foundation::AlgorithmIdentity algorithm, std::string_view detail,
    NumericalFlags flags = 0U) {
    NumericalEvidence evidence;
    evidence.algorithm = algorithm;
    evidence.detail = detail;
    evidence.flags = flags;
    return evidence;
}

template <typename Value>
[[nodiscard]] NumericalOutcome<Value> mass_commit_failure(
    gnc::foundation::AlgorithmIdentity algorithm,
    NumericalStatus status, std::string_view detail,
    NumericalFlags flags = 0U) {
    return NumericalOutcome<Value>::failure(
        status, mass_commit_evidence(algorithm, detail, flags));
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

[[nodiscard]] bool finite(
    const gnc::foundation::QuaternionStorage& value) noexcept {
    return std::isfinite(value.w()) && std::isfinite(value.x()) &&
           std::isfinite(value.y()) && std::isfinite(value.z());
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

[[nodiscard]] bool valid_sample_at(
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

[[nodiscard]] bool valid_interval_at(
    const IntervalSampleContext& context,
    const gnc::contracts::FrameIdentity& expected_frame,
    const gnc::contracts::ClockDomainIdentity& expected_clock,
    const SimulationInstant& expected_start,
    const SimulationInstant& expected_end,
    std::int64_t expected_revision,
    const NumericalPolicy& policy) noexcept {
    return valid_sample_at(context.sample, expected_frame, expected_clock,
                           expected_start, expected_revision, policy) &&
           same_instant(context.validity.effective_from, expected_start,
                        policy) &&
           same_instant(context.validity.effective_until, expected_end,
                        policy);
}

[[nodiscard]] bool valid_propulsion_interval(
    const IntervalSampleContext& context,
    const SuppliedPropulsionDefinition& definition) noexcept {
    const NumericalPolicy& policy = definition.numerical_policy;
    const SimulationInstant& start = context.validity.effective_from;
    const SimulationInstant& end = context.validity.effective_until;
    return context.sample.frame == definition.body_frame &&
           context.sample.clock_domain == definition.clock_domain &&
           context.sample.configuration_revision >= 0 &&
           context.sample.quality == DataQuality::Valid &&
           same_instant(context.sample.sample_time, start, policy) &&
           start.tick >= 0 && end.tick > start.tick &&
           std::isfinite(start.seconds) && std::isfinite(end.seconds) &&
           end.seconds > start.seconds;
}

[[nodiscard]] bool approximate_status(NumericalStatus status) noexcept {
    return status == NumericalStatus::Approximate ||
           status == NumericalStatus::Extrapolated;
}

[[nodiscard]] CommittedRigidMassBoundary promote_candidate(
    const RigidStepContext& context,
    const AtomicRigidMassCandidate& candidate) {
    CommittedRigidMassBoundary committed;
    committed.rigid_context = {
        context.inertial_frame,
        context.clock_domain,
        candidate.effective_at,
        context.configuration_revision,
        DataQuality::Valid,
    };
    committed.rigid_state = candidate.rigid.state;
    committed.mass_state = candidate.mass.state;
    return committed;
}

} // namespace

NumericalOutcome<SuppliedPropulsionOutput>
SuppliedPropulsionKernel::evaluate(
    const SuppliedPropulsionDefinition& definition,
    const SuppliedPropulsionInput& input) {
    if (definition.model_id != kSuppliedPropulsionModelIdentity ||
        definition.model_version.empty() || definition.source_id.empty() ||
        definition.body_frame.id.empty() ||
        definition.clock_domain.id.empty() ||
        definition.mass_state_id.empty() ||
        !gnc::foundation::valid_numerical_policy(
            definition.numerical_policy)) {
        return mass_commit_failure<SuppliedPropulsionOutput>(
            kSuppliedPropulsionKernelIdentity,
            NumericalStatus::DomainError, "definition-or-policy");
    }
    if (!valid_propulsion_interval(input.context, definition)) {
        return mass_commit_failure<SuppliedPropulsionOutput>(
            kSuppliedPropulsionKernelIdentity,
            NumericalStatus::DomainError, "response-context");
    }
    const Vec3& direction = input.thrust_direction.value;
    const Vec3& radius =
        input.center_of_mass_to_application.value;
    const Vec3& intrinsic_moment =
        input.intrinsic_moment_at_application.value;
    if (!std::isfinite(input.thrust_magnitude_newtons) ||
        !std::isfinite(
            input.fuel_consumption_rate_kilograms_per_second) ||
        !finite(direction) || !finite(radius) ||
        !finite(intrinsic_moment)) {
        return mass_commit_failure<SuppliedPropulsionOutput>(
            kSuppliedPropulsionKernelIdentity,
            NumericalStatus::NonFiniteInput, "physical-input");
    }
    if (input.thrust_magnitude_newtons < 0.0 ||
        input.fuel_consumption_rate_kilograms_per_second < 0.0) {
        return mass_commit_failure<SuppliedPropulsionOutput>(
            kSuppliedPropulsionKernelIdentity,
            NumericalStatus::DomainError, "physical-domain");
    }
    const double direction_norm = direction.norm();
    if (!std::isfinite(direction_norm) ||
        !near(direction_norm, 1.0, definition.numerical_policy)) {
        return mass_commit_failure<SuppliedPropulsionOutput>(
            kSuppliedPropulsionKernelIdentity,
            NumericalStatus::DomainError, "thrust-direction-unit");
    }

    const Vec3 force =
        input.thrust_magnitude_newtons * direction;
    const Vec3 lever_arm_moment = radius.cross(force);
    const Vec3 moment_about_center_of_mass =
        intrinsic_moment + lever_arm_moment;
    if (!finite(force) || !finite(lever_arm_moment) ||
        !finite(moment_about_center_of_mass)) {
        return mass_commit_failure<SuppliedPropulsionOutput>(
            kSuppliedPropulsionKernelIdentity,
            NumericalStatus::NonFiniteIntermediate,
            "response-closure-preview");
    }

    SuppliedPropulsionOutput output;
    output.supplied_body_wrench.context = input.context;
    output.supplied_body_wrench.source_id = definition.source_id;
    output.supplied_body_wrench.force.value = force;
    output.supplied_body_wrench.center_of_mass_to_application =
        input.center_of_mass_to_application;
    output.supplied_body_wrench.intrinsic_moment_at_application =
        input.intrinsic_moment_at_application;
    output.lever_arm_moment.value = lever_arm_moment;
    output.moment_about_center_of_mass.value =
        moment_about_center_of_mass;
    output.mass_flow.context = input.context;
    output.mass_flow.mass_state_id = definition.mass_state_id;
    output.mass_flow.fuel_consumption_rate_kilograms_per_second =
        input.fuel_consumption_rate_kilograms_per_second;

    NumericalEvidence evidence = mass_commit_evidence(
        kSuppliedPropulsionKernelIdentity, "supplied-response");
    evidence.evaluations = 1U;
    evidence.last_step = input.context.validity.effective_until.seconds -
                         input.context.validity.effective_from.seconds;
    return NumericalOutcome<SuppliedPropulsionOutput>::with_value(
        NumericalStatus::Success, std::move(output), evidence);
}

NumericalOutcome<ScalarBurnMassOutput> ScalarBurnMassKernel::evaluate(
    const ScalarBurnMassDefinition& definition,
    const MassState& committed_state,
    const MassFlowIntervalInput& flow,
    const NumericalPolicy& policy) {
    if (definition.model_id != kScalarBurnMassModelIdentity ||
        definition.model_version.empty() || definition.mass_state_id.empty() ||
        !gnc::foundation::valid_numerical_policy(policy)) {
        return mass_commit_failure<ScalarBurnMassOutput>(
            kScalarBurnMassKernelIdentity, NumericalStatus::DomainError,
            "definition-or-policy");
    }
    if (committed_state.mass_state_id != definition.mass_state_id ||
        flow.mass_state_id != definition.mass_state_id ||
        committed_state.context.frame.id.empty() ||
        committed_state.context.clock_domain.id.empty() ||
        committed_state.context.configuration_revision < 0 ||
        committed_state.context.frame != flow.context.sample.frame ||
        committed_state.context.clock_domain !=
            flow.context.sample.clock_domain ||
        committed_state.context.configuration_revision !=
            flow.context.sample.configuration_revision ||
        committed_state.context.quality != DataQuality::Valid ||
        flow.context.sample.quality != DataQuality::Valid ||
        !same_instant(committed_state.context.sample_time,
                      flow.context.sample.sample_time, policy) ||
        !same_instant(flow.context.sample.sample_time,
                      flow.context.validity.effective_from, policy)) {
        return mass_commit_failure<ScalarBurnMassOutput>(
            kScalarBurnMassKernelIdentity, NumericalStatus::DomainError,
            "state-or-flow-identity");
    }
    const SimulationInstant start = flow.context.validity.effective_from;
    const SimulationInstant end = flow.context.validity.effective_until;
    if (start.tick < 0 || end.tick <= start.tick ||
        !std::isfinite(start.seconds) || !std::isfinite(end.seconds)) {
        return mass_commit_failure<ScalarBurnMassOutput>(
            kScalarBurnMassKernelIdentity, NumericalStatus::DomainError,
            "flow-time");
    }
    const double duration_seconds = end.seconds - start.seconds;
    if (!std::isfinite(committed_state.mass_kilograms) ||
        !std::isfinite(
            flow.fuel_consumption_rate_kilograms_per_second) ||
        !finite(committed_state.body_origin_to_center_of_mass.value) ||
        !finite(committed_state.inertia_about_center_of_mass.value)) {
        return mass_commit_failure<ScalarBurnMassOutput>(
            kScalarBurnMassKernelIdentity, NumericalStatus::NonFiniteInput,
            "mass-input");
    }
    if (duration_seconds <= 0.0 || committed_state.mass_kilograms <= 0.0 ||
        flow.fuel_consumption_rate_kilograms_per_second < 0.0) {
        return mass_commit_failure<ScalarBurnMassOutput>(
            kScalarBurnMassKernelIdentity, NumericalStatus::DomainError,
            "mass-domain");
    }
    const auto inertia_check = gnc::foundation::solve_spd_3x3(
        committed_state.inertia_about_center_of_mass.value,
        Vec3::Zero(), policy);
    if (!inertia_check.has_value()) {
        return mass_commit_failure<ScalarBurnMassOutput>(
            kScalarBurnMassKernelIdentity, inertia_check.status(),
            "mass-inertia", inertia_check.evidence().flags);
    }
    const double consumed =
        flow.fuel_consumption_rate_kilograms_per_second * duration_seconds;
    const double candidate_mass =
        committed_state.mass_kilograms - consumed;
    if (!std::isfinite(consumed) || !std::isfinite(candidate_mass)) {
        return mass_commit_failure<ScalarBurnMassOutput>(
            kScalarBurnMassKernelIdentity,
            NumericalStatus::NonFiniteIntermediate, "mass-evolution");
    }
    if (candidate_mass <= 0.0) {
        return mass_commit_failure<ScalarBurnMassOutput>(
            kScalarBurnMassKernelIdentity, NumericalStatus::DomainError,
            "mass-depletion");
    }

    MassState candidate_state = committed_state;
    candidate_state.context.sample_time = end;
    candidate_state.mass_kilograms = candidate_mass;
    ScalarBurnMassOutput output;
    output.current_committed_mass_kilograms =
        committed_state.mass_kilograms;
    output.integration_mass_kilograms = committed_state.mass_kilograms;
    output.consumed_mass_kilograms = consumed;
    output.candidate.effective_at = end;
    output.candidate.state = std::move(candidate_state);
    NumericalEvidence evidence = mass_commit_evidence(
        kScalarBurnMassKernelIdentity, "mass-candidate",
        inertia_check.evidence().flags);
    evidence.evaluations = 1U + inertia_check.evidence().evaluations;
    evidence.last_step = duration_seconds;
    return NumericalOutcome<ScalarBurnMassOutput>::with_value(
        approximate_status(inertia_check.status())
            ? NumericalStatus::Approximate
            : NumericalStatus::Success,
        std::move(output), evidence);
}

NumericalOutcome<AltitudePitchGuidanceOutput>
AltitudePitchGuidanceKernel::evaluate(
    const AltitudePitchGuidanceDefinition& definition,
    const CommittedRigidObservation& observation) {
    const NumericalPolicy& policy = definition.attitude_policy.numerical;
    if (definition.model_id != kAltitudePitchGuidanceModelIdentity ||
        definition.model_version.empty() ||
        definition.inertial_frame.id.empty() ||
        definition.clock_domain.id.empty() ||
        definition.configuration_revision < 0 ||
        !gnc::foundation::valid_quaternion_policy(
            definition.attitude_policy) ||
        !std::isfinite(definition.target_altitude_meters) ||
        !std::isfinite(
            definition.altitude_error_gain_radians_per_meter) ||
        !std::isfinite(
            definition.vertical_speed_gain_radian_seconds_per_meter) ||
        !std::isfinite(definition.pitch_command_limit_radians) ||
        definition.altitude_error_gain_radians_per_meter < 0.0 ||
        definition.vertical_speed_gain_radian_seconds_per_meter < 0.0 ||
        definition.pitch_command_limit_radians <= 0.0) {
        return mass_commit_failure<AltitudePitchGuidanceOutput>(
            kAltitudePitchGuidanceKernelIdentity,
            NumericalStatus::DomainError, "definition-or-policy");
    }
    if (!valid_sample_at(
            observation.context, definition.inertial_frame,
            definition.clock_domain, observation.context.sample_time,
            definition.configuration_revision, policy) ||
        observation.context.sample_time.tick < 0 ||
        !std::isfinite(observation.context.sample_time.seconds)) {
        return mass_commit_failure<AltitudePitchGuidanceOutput>(
            kAltitudePitchGuidanceKernelIdentity,
            NumericalStatus::DomainError, "committed-observation-context");
    }
    if (!finite(observation.state.position.value) ||
        !finite(observation.state.velocity.value) ||
        !finite(observation.state.angular_rate.value)) {
        return mass_commit_failure<AltitudePitchGuidanceOutput>(
            kAltitudePitchGuidanceKernelIdentity,
            NumericalStatus::NonFiniteInput, "committed-observation-state");
    }
    const auto attitude = gnc::foundation::prepare_passive_quaternion(
        observation.state.attitude.value, definition.attitude_policy);
    if (!attitude.has_value()) {
        return mass_commit_failure<AltitudePitchGuidanceOutput>(
            kAltitudePitchGuidanceKernelIdentity, attitude.status(),
            "committed-observation-attitude", attitude.evidence().flags);
    }
    const auto& q = attitude.value();
    if (!near(q.x(), 0.0, policy) || !near(q.z(), 0.0, policy) ||
        q.w() <= 0.0) {
        return mass_commit_failure<AltitudePitchGuidanceOutput>(
            kAltitudePitchGuidanceKernelIdentity,
            NumericalStatus::DomainError, "pure-pitch-projection",
            attitude.evidence().flags);
    }

    const double pitch = -2.0 * std::atan2(q.y(), q.w());
    const double pitch_rate = observation.state.angular_rate.value(1);
    const double altitude_error = definition.target_altitude_meters -
                                  observation.state.position.value(2);
    const double altitude_feedback =
        definition.altitude_error_gain_radians_per_meter * altitude_error;
    const double vertical_speed_feedback =
        -definition.vertical_speed_gain_radian_seconds_per_meter *
        observation.state.velocity.value(2);
    const double raw_pitch_command =
        altitude_feedback + vertical_speed_feedback;
    const double pitch_command = std::clamp(
        raw_pitch_command, -definition.pitch_command_limit_radians,
        definition.pitch_command_limit_radians);
    if (!std::isfinite(pitch) || !std::isfinite(pitch_rate) ||
        !std::isfinite(altitude_error) ||
        !std::isfinite(altitude_feedback) ||
        !std::isfinite(vertical_speed_feedback) ||
        !std::isfinite(raw_pitch_command) ||
        !std::isfinite(pitch_command)) {
        return mass_commit_failure<AltitudePitchGuidanceOutput>(
            kAltitudePitchGuidanceKernelIdentity,
            NumericalStatus::NonFiniteIntermediate, "guidance-formula",
            attitude.evidence().flags);
    }

    AltitudePitchGuidanceOutput output;
    output.source_observation = observation;
    output.measured_pitch_radians = pitch;
    output.measured_pitch_rate_radians_per_second = pitch_rate;
    output.altitude_error_meters = altitude_error;
    output.altitude_feedback_radians = altitude_feedback;
    output.vertical_speed_feedback_radians = vertical_speed_feedback;
    output.raw_pitch_command_radians = raw_pitch_command;
    output.pitch_command_radians = pitch_command;
    output.saturated = pitch_command != raw_pitch_command;
    NumericalEvidence evidence = mass_commit_evidence(
        kAltitudePitchGuidanceKernelIdentity, "committed-altitude-pitch",
        attitude.evidence().flags);
    evidence.evaluations = attitude.evidence().evaluations + 1U;
    evidence.residual_norm = attitude.evidence().residual_norm;
    return NumericalOutcome<AltitudePitchGuidanceOutput>::with_value(
        approximate_status(attitude.status())
            ? NumericalStatus::Approximate
            : NumericalStatus::Success,
        std::move(output), evidence);
}

NumericalOutcome<PitchMomentControllerOutput>
PitchMomentControllerKernel::evaluate(
    const PitchMomentControllerDefinition& definition,
    const AltitudePitchGuidanceOutput& guidance) {
    if (definition.model_id != kPitchMomentControllerModelIdentity ||
        definition.model_version.empty() || definition.body_frame.id.empty() ||
        definition.clock_domain.id.empty() ||
        definition.configuration_revision < 0 ||
        !gnc::foundation::valid_numerical_policy(
            definition.numerical_policy) ||
        !std::isfinite(
            definition.pitch_error_gain_newton_meters_per_radian) ||
        !std::isfinite(
            definition.pitch_rate_gain_newton_meter_seconds_per_radian) ||
        !std::isfinite(
            definition.moment_command_limit_newton_meters) ||
        definition.pitch_error_gain_newton_meters_per_radian < 0.0 ||
        definition.pitch_rate_gain_newton_meter_seconds_per_radian < 0.0 ||
        definition.moment_command_limit_newton_meters <= 0.0) {
        return mass_commit_failure<PitchMomentControllerOutput>(
            kPitchMomentControllerKernelIdentity,
            NumericalStatus::DomainError, "definition-or-policy");
    }
    const auto& source = guidance.source_observation.context;
    if (source.clock_domain != definition.clock_domain ||
        source.configuration_revision !=
            definition.configuration_revision ||
        source.quality != DataQuality::Valid ||
        source.sample_time.tick < 0 ||
        !std::isfinite(source.sample_time.seconds)) {
        return mass_commit_failure<PitchMomentControllerOutput>(
            kPitchMomentControllerKernelIdentity,
            NumericalStatus::DomainError, "guidance-context");
    }
    if (!std::isfinite(guidance.measured_pitch_radians) ||
        !std::isfinite(guidance.measured_pitch_rate_radians_per_second) ||
        !std::isfinite(guidance.pitch_command_radians)) {
        return mass_commit_failure<PitchMomentControllerOutput>(
            kPitchMomentControllerKernelIdentity,
            NumericalStatus::NonFiniteInput, "guidance-value");
    }

    const double pitch_error = guidance.pitch_command_radians -
                               guidance.measured_pitch_radians;
    const double proportional =
        definition.pitch_error_gain_newton_meters_per_radian * pitch_error;
    const double damping =
        -definition.pitch_rate_gain_newton_meter_seconds_per_radian *
        guidance.measured_pitch_rate_radians_per_second;
    const double raw_command = proportional + damping;
    const double command = std::clamp(
        raw_command, -definition.moment_command_limit_newton_meters,
        definition.moment_command_limit_newton_meters);
    if (!std::isfinite(pitch_error) || !std::isfinite(proportional) ||
        !std::isfinite(damping) || !std::isfinite(raw_command) ||
        !std::isfinite(command)) {
        return mass_commit_failure<PitchMomentControllerOutput>(
            kPitchMomentControllerKernelIdentity,
            NumericalStatus::NonFiniteIntermediate, "controller-formula");
    }

    PitchMomentControllerOutput output;
    output.context = source;
    output.context.frame = definition.body_frame;
    output.pitch_error_radians = pitch_error;
    output.proportional_moment_newton_meters = proportional;
    output.rate_damping_moment_newton_meters = damping;
    output.raw_moment_command_newton_meters = raw_command;
    output.moment_command_newton_meters = command;
    output.saturated = command != raw_command;
    NumericalEvidence evidence = mass_commit_evidence(
        kPitchMomentControllerKernelIdentity, "pitch-moment-command");
    evidence.evaluations = 1U;
    return NumericalOutcome<PitchMomentControllerOutput>::with_value(
        NumericalStatus::Success, std::move(output), evidence);
}

NumericalOutcome<IdealBodyMomentActuatorOutput>
IdealBodyMomentActuatorKernel::evaluate(
    const IdealBodyMomentActuatorDefinition& definition,
    const IntervalSampleContext& context,
    const PitchMomentControllerOutput& controller) {
    if (definition.model_id != kIdealBodyMomentActuatorModelIdentity ||
        definition.model_version.empty() || definition.source_id.empty() ||
        definition.body_frame.id.empty() ||
        definition.clock_domain.id.empty() ||
        definition.configuration_revision < 0 ||
        !gnc::foundation::valid_numerical_policy(
            definition.numerical_policy) ||
        !std::isfinite(definition.realization_gain) ||
        !near(definition.realization_gain, 1.0,
              definition.numerical_policy)) {
        return mass_commit_failure<IdealBodyMomentActuatorOutput>(
            kIdealBodyMomentActuatorKernelIdentity,
            NumericalStatus::DomainError, "definition-or-policy");
    }
    if (!valid_interval_at(
            context, definition.body_frame, definition.clock_domain,
            context.validity.effective_from,
            context.validity.effective_until,
            definition.configuration_revision,
            definition.numerical_policy) ||
        context.validity.effective_from.tick < 0 ||
        context.validity.effective_until.tick <=
            context.validity.effective_from.tick ||
        !std::isfinite(context.validity.effective_from.seconds) ||
        !std::isfinite(context.validity.effective_until.seconds) ||
        context.validity.effective_until.seconds <=
            context.validity.effective_from.seconds ||
        controller.context.frame != definition.body_frame ||
        controller.context.clock_domain != definition.clock_domain ||
        controller.context.configuration_revision !=
            definition.configuration_revision ||
        controller.context.quality != DataQuality::Valid ||
        !same_instant(controller.context.sample_time,
                      context.validity.effective_from,
                      definition.numerical_policy)) {
        return mass_commit_failure<IdealBodyMomentActuatorOutput>(
            kIdealBodyMomentActuatorKernelIdentity,
            NumericalStatus::DomainError, "controller-or-interval-context");
    }
    if (!std::isfinite(controller.moment_command_newton_meters)) {
        return mass_commit_failure<IdealBodyMomentActuatorOutput>(
            kIdealBodyMomentActuatorKernelIdentity,
            NumericalStatus::NonFiniteInput, "moment-command");
    }
    const double realized = definition.realization_gain *
                            controller.moment_command_newton_meters;
    if (!std::isfinite(realized)) {
        return mass_commit_failure<IdealBodyMomentActuatorOutput>(
            kIdealBodyMomentActuatorKernelIdentity,
            NumericalStatus::NonFiniteIntermediate,
            "moment-realization");
    }

    IdealBodyMomentActuatorOutput output;
    output.context = context;
    output.source_id = definition.source_id;
    output.moment_about_center_of_mass.value = Vec3{0.0, realized, 0.0};
    NumericalEvidence evidence = mass_commit_evidence(
        kIdealBodyMomentActuatorKernelIdentity,
        "current-cycle-ideal-moment");
    evidence.evaluations = 1U;
    evidence.last_step = context.validity.effective_until.seconds -
                         context.validity.effective_from.seconds;
    return NumericalOutcome<IdealBodyMomentActuatorOutput>::with_value(
        NumericalStatus::Success, std::move(output), evidence);
}

NumericalOutcome<FrozenRigidMassStepOutput>
FrozenRigidMassStepKernel::evaluate(
    const PreparedRigidStepModel& rigid_model,
    const ScalarBurnMassDefinition& mass_definition,
    const CommittedRigidMassBoundary& opening_boundary,
    const RigidMassIntervalInput& interval) {
    const auto& rigid_definition = rigid_model.definition();
    const auto& policy = rigid_definition.algorithm.numerical_policy;
    if (!valid_sample_at(
            opening_boundary.rigid_context,
            rigid_definition.inertial_frame,
            rigid_definition.metadata.clock_domain,
            interval.context.interval_start,
            rigid_definition.metadata.configuration_revision, policy) ||
        !valid_sample_at(
            opening_boundary.mass_state.context,
            rigid_definition.body_frame,
            rigid_definition.metadata.clock_domain,
            interval.context.interval_start,
            rigid_definition.metadata.configuration_revision, policy) ||
        !valid_interval_at(
            interval.mass_flow.context, rigid_definition.body_frame,
            rigid_definition.metadata.clock_domain,
            interval.context.interval_start,
            interval.context.interval_end,
            rigid_definition.metadata.configuration_revision, policy) ||
        opening_boundary.mass_state.mass_state_id !=
            mass_definition.mass_state_id ||
        interval.mass_flow.mass_state_id != mass_definition.mass_state_id) {
        return mass_commit_failure<FrozenRigidMassStepOutput>(
            kFrozenRigidMassStepKernelIdentity,
            NumericalStatus::DomainError, "opening-boundary-or-flow");
    }

    MassPropertiesInput projected_mass;
    projected_mass.context = interval.mass_flow.context;
    projected_mass.mass_state_id =
        opening_boundary.mass_state.mass_state_id;
    projected_mass.mass_kilograms =
        opening_boundary.mass_state.mass_kilograms;
    projected_mass.body_origin_to_center_of_mass =
        opening_boundary.mass_state.body_origin_to_center_of_mass;
    projected_mass.inertia_about_center_of_mass =
        opening_boundary.mass_state.inertia_about_center_of_mass;

    RigidStepInput rigid_input;
    rigid_input.context = interval.context;
    rigid_input.committed_state = opening_boundary.rigid_state;
    rigid_input.environment = interval.environment;
    rigid_input.mass_properties = projected_mass;
    rigid_input.supplied_wrench = interval.supplied_wrench;
    const auto rigid = RigidStepKernel::evaluate(rigid_model, rigid_input);
    if (!rigid.has_value()) {
        return mass_commit_failure<FrozenRigidMassStepOutput>(
            kFrozenRigidMassStepKernelIdentity, rigid.status(),
            "rigid-candidate", rigid.evidence().flags);
    }

    const auto mass = ScalarBurnMassKernel::evaluate(
        mass_definition, opening_boundary.mass_state,
        interval.mass_flow, policy);
    if (!mass.has_value()) {
        return mass_commit_failure<FrozenRigidMassStepOutput>(
            kFrozenRigidMassStepKernelIdentity, mass.status(),
            "mass-candidate", rigid.evidence().flags |
                                  mass.evidence().flags);
    }
    if (!same_instant(rigid.value().candidate.effective_at,
                      mass.value().candidate.effective_at, policy)) {
        return mass_commit_failure<FrozenRigidMassStepOutput>(
            kFrozenRigidMassStepKernelIdentity,
            NumericalStatus::InternalFailure,
            "candidate-closing-time", rigid.evidence().flags |
                                          mass.evidence().flags);
    }

    FrozenRigidMassStepOutput output;
    output.opening_boundary = opening_boundary;
    output.projected_committed_mass = projected_mass;
    output.rigid_step = rigid.value();
    output.mass_evolution = mass.value();
    output.candidate.effective_at = rigid.value().candidate.effective_at;
    output.candidate.rigid = rigid.value().candidate;
    output.candidate.mass = mass.value().candidate;
    const NumericalFlags flags =
        rigid.evidence().flags | mass.evidence().flags;
    NumericalEvidence evidence = mass_commit_evidence(
        kFrozenRigidMassStepKernelIdentity, "atomic-candidate", flags);
    evidence.evaluations = rigid.evidence().evaluations +
                           mass.evidence().evaluations;
    evidence.last_step = rigid_definition.algorithm.fixed_step_seconds;
    return NumericalOutcome<FrozenRigidMassStepOutput>::with_value(
        approximate_status(rigid.status()) || approximate_status(mass.status())
            ? NumericalStatus::Approximate
            : NumericalStatus::Success,
        std::move(output), evidence);
}

NumericalOutcome<PropelledFrozenRigidMassStepOutput>
PropelledFrozenRigidMassStepKernel::evaluate(
    const PreparedRigidStepModel& rigid_model,
    const ScalarBurnMassDefinition& mass_definition,
    const SuppliedPropulsionDefinition& propulsion_definition,
    const CommittedRigidMassBoundary& opening_boundary,
    const PropelledRigidMassIntervalInput& interval) {
    const auto propulsion = SuppliedPropulsionKernel::evaluate(
        propulsion_definition, interval.propulsion);
    if (!propulsion.has_value()) {
        return mass_commit_failure<
            PropelledFrozenRigidMassStepOutput>(
                kPropelledFrozenRigidMassStepKernelIdentity,
                propulsion.status(), "propulsion-response",
                propulsion.evidence().flags);
    }

    RigidMassIntervalInput atomic_input;
    atomic_input.context = interval.context;
    atomic_input.environment = interval.environment;
    const auto& response = propulsion.value();
    atomic_input.supplied_wrench.context =
        response.supplied_body_wrench.context;
    atomic_input.supplied_wrench.source_id =
        response.supplied_body_wrench.source_id;
    atomic_input.supplied_wrench.force =
        response.supplied_body_wrench.force;
    atomic_input.supplied_wrench.body_origin_to_application.value =
        opening_boundary.mass_state.body_origin_to_center_of_mass.value +
        response.supplied_body_wrench
            .center_of_mass_to_application.value;
    atomic_input.supplied_wrench.intrinsic_moment_at_application =
        response.supplied_body_wrench
            .intrinsic_moment_at_application;
    atomic_input.mass_flow = response.mass_flow;
    if (!finite(atomic_input.supplied_wrench
                    .body_origin_to_application.value)) {
        return mass_commit_failure<
            PropelledFrozenRigidMassStepOutput>(
                kPropelledFrozenRigidMassStepKernelIdentity,
                NumericalStatus::NonFiniteIntermediate,
                "application-point-adapter",
                propulsion.evidence().flags);
    }

    const auto boundary = FrozenRigidMassStepKernel::evaluate(
        rigid_model, mass_definition, opening_boundary, atomic_input);
    if (!boundary.has_value()) {
        return mass_commit_failure<
            PropelledFrozenRigidMassStepOutput>(
                kPropelledFrozenRigidMassStepKernelIdentity,
                boundary.status(), "atomic-boundary",
                propulsion.evidence().flags |
                    boundary.evidence().flags);
    }

    PropelledFrozenRigidMassStepOutput output;
    output.propulsion = response;
    output.atomic_boundary = boundary.value();
    const NumericalFlags flags =
        propulsion.evidence().flags | boundary.evidence().flags;
    NumericalEvidence evidence = mass_commit_evidence(
        kPropelledFrozenRigidMassStepKernelIdentity,
        "propulsion-to-atomic-boundary", flags);
    evidence.evaluations = propulsion.evidence().evaluations +
                           boundary.evidence().evaluations;
    evidence.last_step =
        rigid_model.definition().algorithm.fixed_step_seconds;
    return NumericalOutcome<
        PropelledFrozenRigidMassStepOutput>::with_value(
            approximate_status(propulsion.status()) ||
                    approximate_status(boundary.status())
                ? NumericalStatus::Approximate
                : NumericalStatus::Success,
            std::move(output), evidence);
}

NumericalOutcome<ControlledPropelledRigidMassStepOutput>
ControlledPropelledRigidMassStepKernel::evaluate(
    const PreparedRigidStepModel& rigid_model,
    const ScalarBurnMassDefinition& mass_definition,
    const SuppliedPropulsionDefinition& propulsion_definition,
    const ControlledPropelledRigidMassStepDefinition& definition,
    const CommittedRigidMassBoundary& opening_boundary,
    const PropelledRigidMassIntervalInput& interval) {
    const auto& rigid_definition = rigid_model.definition();
    if (definition.model_id !=
            kControlledPropelledRigidMassStepModelIdentity ||
        definition.model_version.empty() ||
        definition.combined_wrench_source_id.empty() ||
        definition.guidance.inertial_frame !=
            rigid_definition.inertial_frame ||
        definition.controller.body_frame != rigid_definition.body_frame ||
        definition.actuator.body_frame != rigid_definition.body_frame ||
        definition.guidance.clock_domain !=
            rigid_definition.metadata.clock_domain ||
        definition.controller.clock_domain !=
            rigid_definition.metadata.clock_domain ||
        definition.actuator.clock_domain !=
            rigid_definition.metadata.clock_domain ||
        definition.guidance.configuration_revision !=
            rigid_definition.metadata.configuration_revision ||
        definition.controller.configuration_revision !=
            rigid_definition.metadata.configuration_revision ||
        definition.actuator.configuration_revision !=
            rigid_definition.metadata.configuration_revision ||
        propulsion_definition.body_frame != rigid_definition.body_frame ||
        propulsion_definition.clock_domain !=
            rigid_definition.metadata.clock_domain) {
        return mass_commit_failure<
            ControlledPropelledRigidMassStepOutput>(
                kControlledPropelledRigidMassStepKernelIdentity,
                NumericalStatus::DomainError,
                "definition-identity-closure");
    }

    CommittedRigidObservation observation;
    observation.context = opening_boundary.rigid_context;
    observation.state = opening_boundary.rigid_state;
    const auto guidance = AltitudePitchGuidanceKernel::evaluate(
        definition.guidance, observation);
    if (!guidance.has_value()) {
        return mass_commit_failure<
            ControlledPropelledRigidMassStepOutput>(
                kControlledPropelledRigidMassStepKernelIdentity,
                guidance.status(), "guidance",
                guidance.evidence().flags);
    }
    const auto controller = PitchMomentControllerKernel::evaluate(
        definition.controller, guidance.value());
    if (!controller.has_value()) {
        return mass_commit_failure<
            ControlledPropelledRigidMassStepOutput>(
                kControlledPropelledRigidMassStepKernelIdentity,
                controller.status(), "controller",
                guidance.evidence().flags |
                    controller.evidence().flags);
    }
    const auto actuator = IdealBodyMomentActuatorKernel::evaluate(
        definition.actuator, interval.propulsion.context,
        controller.value());
    if (!actuator.has_value()) {
        return mass_commit_failure<
            ControlledPropelledRigidMassStepOutput>(
                kControlledPropelledRigidMassStepKernelIdentity,
                actuator.status(), "actuator",
                guidance.evidence().flags |
                    controller.evidence().flags |
                    actuator.evidence().flags);
    }
    const auto propulsion = SuppliedPropulsionKernel::evaluate(
        propulsion_definition, interval.propulsion);
    if (!propulsion.has_value()) {
        return mass_commit_failure<
            ControlledPropelledRigidMassStepOutput>(
                kControlledPropelledRigidMassStepKernelIdentity,
                propulsion.status(), "propulsion-response",
                guidance.evidence().flags |
                    controller.evidence().flags |
                    actuator.evidence().flags |
                    propulsion.evidence().flags);
    }

    RigidMassIntervalInput atomic_input;
    atomic_input.context = interval.context;
    atomic_input.environment = interval.environment;
    const auto& response = propulsion.value();
    atomic_input.supplied_wrench.context =
        response.supplied_body_wrench.context;
    atomic_input.supplied_wrench.source_id =
        definition.combined_wrench_source_id;
    atomic_input.supplied_wrench.force =
        response.supplied_body_wrench.force;
    atomic_input.supplied_wrench.body_origin_to_application.value =
        opening_boundary.mass_state.body_origin_to_center_of_mass.value +
        response.supplied_body_wrench
            .center_of_mass_to_application.value;
    atomic_input.supplied_wrench.intrinsic_moment_at_application.value =
        response.supplied_body_wrench
            .intrinsic_moment_at_application.value +
        actuator.value().moment_about_center_of_mass.value;
    atomic_input.mass_flow = response.mass_flow;
    if (!finite(atomic_input.supplied_wrench
                    .body_origin_to_application.value) ||
        !finite(atomic_input.supplied_wrench
                    .intrinsic_moment_at_application.value)) {
        return mass_commit_failure<
            ControlledPropelledRigidMassStepOutput>(
                kControlledPropelledRigidMassStepKernelIdentity,
                NumericalStatus::NonFiniteIntermediate,
                "controlled-wrench-adapter");
    }
    const auto boundary = FrozenRigidMassStepKernel::evaluate(
        rigid_model, mass_definition, opening_boundary, atomic_input);
    if (!boundary.has_value()) {
        return mass_commit_failure<
            ControlledPropelledRigidMassStepOutput>(
                kControlledPropelledRigidMassStepKernelIdentity,
                boundary.status(), "atomic-boundary",
                guidance.evidence().flags |
                    controller.evidence().flags |
                    actuator.evidence().flags |
                    propulsion.evidence().flags |
                    boundary.evidence().flags);
    }

    ControlledPropelledRigidMassStepOutput output;
    output.observation = std::move(observation);
    output.guidance = guidance.value();
    output.controller = controller.value();
    output.actuator = actuator.value();
    output.propulsion = propulsion.value();
    output.atomic_boundary = boundary.value();
    const NumericalFlags flags =
        guidance.evidence().flags | controller.evidence().flags |
        actuator.evidence().flags | propulsion.evidence().flags |
        boundary.evidence().flags;
    NumericalEvidence evidence = mass_commit_evidence(
        kControlledPropelledRigidMassStepKernelIdentity,
        "committed-control-to-atomic-boundary", flags);
    evidence.evaluations = guidance.evidence().evaluations +
                           controller.evidence().evaluations +
                           actuator.evidence().evaluations +
                           propulsion.evidence().evaluations +
                           boundary.evidence().evaluations;
    evidence.last_step =
        rigid_definition.algorithm.fixed_step_seconds;
    return NumericalOutcome<
        ControlledPropelledRigidMassStepOutput>::with_value(
            approximate_status(guidance.status()) ||
                    approximate_status(controller.status()) ||
                    approximate_status(actuator.status()) ||
                    approximate_status(propulsion.status()) ||
                    approximate_status(boundary.status())
                ? NumericalStatus::Approximate
                : NumericalStatus::Success,
            std::move(output), evidence);
}

NumericalOutcome<TwoIntervalControlledPropelledCommitOutput>
TwoIntervalControlledPropelledCommitKernel::evaluate(
    const PreparedRigidStepModel& rigid_model,
    const ScalarBurnMassDefinition& mass_definition,
    const SuppliedPropulsionDefinition& propulsion_definition,
    const ControlledPropelledRigidMassStepDefinition& definition,
    const TwoIntervalControlledPropelledCommitInput& input) {
    const auto first = ControlledPropelledRigidMassStepKernel::evaluate(
        rigid_model, mass_definition, propulsion_definition, definition,
        input.opening_boundary, input.intervals[0]);
    if (!first.has_value()) {
        return mass_commit_failure<
            TwoIntervalControlledPropelledCommitOutput>(
                kTwoIntervalControlledPropelledCommitKernelIdentity,
                first.status(), "interval-0", first.evidence().flags);
    }
    CommittedRigidMassBoundary first_commit = promote_candidate(
        input.intervals[0].context,
        first.value().atomic_boundary.candidate);

    const auto second = ControlledPropelledRigidMassStepKernel::evaluate(
        rigid_model, mass_definition, propulsion_definition, definition,
        first_commit, input.intervals[1]);
    if (!second.has_value()) {
        return mass_commit_failure<
            TwoIntervalControlledPropelledCommitOutput>(
                kTwoIntervalControlledPropelledCommitKernelIdentity,
                second.status(), "interval-1",
                first.evidence().flags | second.evidence().flags);
    }
    CommittedRigidMassBoundary second_commit = promote_candidate(
        input.intervals[1].context,
        second.value().atomic_boundary.candidate);

    TwoIntervalControlledPropelledCommitOutput output;
    output.intervals[0].staged = first.value();
    output.intervals[0].closing_commit = first_commit;
    output.intervals[1].staged = second.value();
    output.intervals[1].closing_commit = second_commit;
    output.terminal_boundary = second_commit;
    const NumericalFlags flags =
        first.evidence().flags | second.evidence().flags;
    NumericalEvidence evidence = mass_commit_evidence(
        kTwoIntervalControlledPropelledCommitKernelIdentity,
        "two-interval-committed-control-feedback", flags);
    evidence.evaluations = first.evidence().evaluations +
                           second.evidence().evaluations;
    evidence.last_step =
        rigid_model.definition().algorithm.fixed_step_seconds;
    return NumericalOutcome<
        TwoIntervalControlledPropelledCommitOutput>::with_value(
            approximate_status(first.status()) ||
                    approximate_status(second.status())
                ? NumericalStatus::Approximate
                : NumericalStatus::Success,
            std::move(output), evidence);
}

NumericalOutcome<CommittedMissionResultOutput>
CommittedMissionResultKernel::evaluate(
    const CommittedMissionResultDefinition& definition,
    const CommittedMissionResultInput& input) {
    const auto valid_metric = [](MissionMetric metric) {
        switch (metric) {
        case MissionMetric::DurationSeconds:
        case MissionMetric::DownrangeMeters:
        case MissionMetric::RemainingMassKilograms:
            return true;
        }
        return false;
    };
    const auto valid_relation = [](MissionRelation relation) {
        switch (relation) {
        case MissionRelation::LessThanOrEqual:
        case MissionRelation::GreaterThanOrEqual:
            return true;
        }
        return false;
    };
    const auto valid_action = [](MissionAction action) {
        switch (action) {
        case MissionAction::Complete:
        case MissionAction::Abort:
            return true;
        }
        return false;
    };
    if (definition.model_id != kCommittedMissionResultModelIdentity ||
        definition.model_version.empty() || definition.subject.empty() ||
        definition.inertial_frame.id.empty() ||
        definition.body_frame.id.empty() ||
        definition.clock_domain.id.empty() ||
        definition.mass_state_id.empty() ||
        definition.configuration_revision < 0 ||
        !gnc::foundation::valid_numerical_policy(
            definition.numerical_policy)) {
        return mass_commit_failure<CommittedMissionResultOutput>(
            kCommittedMissionResultKernelIdentity,
            NumericalStatus::DomainError, "definition-or-policy");
    }
    for (std::size_t index = 0U;
         index < definition.predicates.size(); ++index) {
        const auto& predicate = definition.predicates[index];
        if (predicate.predicate_id.empty() ||
            predicate.reason_code.empty() ||
            !valid_metric(predicate.metric) ||
            !valid_relation(predicate.relation) ||
            !valid_action(predicate.action) ||
            !std::isfinite(predicate.threshold) ||
            predicate.priority < 0) {
            return mass_commit_failure<CommittedMissionResultOutput>(
                kCommittedMissionResultKernelIdentity,
                NumericalStatus::DomainError,
                "termination-predicate");
        }
        for (std::size_t previous = 0U; previous < index; ++previous) {
            if (predicate.predicate_id ==
                definition.predicates[previous].predicate_id) {
                return mass_commit_failure<
                    CommittedMissionResultOutput>(
                        kCommittedMissionResultKernelIdentity,
                        NumericalStatus::DomainError,
                        "duplicate-predicate-id");
            }
        }
    }

    const NumericalPolicy& policy = definition.numerical_policy;
    NumericalFlags validation_flags = 0U;
    std::uint64_t validation_evaluations = 0U;
    for (std::size_t index = 0U;
         index < input.committed_samples.size(); ++index) {
        const auto& sample = input.committed_samples[index];
        const auto& rigid_context = sample.rigid_context;
        const auto& mass = sample.mass_state;
        if (!valid_sample_at(
                rigid_context, definition.inertial_frame,
                definition.clock_domain, rigid_context.sample_time,
                definition.configuration_revision, policy) ||
            !valid_sample_at(
                mass.context, definition.body_frame,
                definition.clock_domain, rigid_context.sample_time,
                definition.configuration_revision, policy) ||
            mass.mass_state_id != definition.mass_state_id ||
            rigid_context.sample_time.tick < 0 ||
            !std::isfinite(rigid_context.sample_time.seconds)) {
            return mass_commit_failure<CommittedMissionResultOutput>(
                kCommittedMissionResultKernelIdentity,
                NumericalStatus::DomainError,
                "committed-sample-context", validation_flags);
        }
        if (!finite(sample.rigid_state.position.value) ||
            !finite(sample.rigid_state.velocity.value) ||
            !finite(sample.rigid_state.attitude.value) ||
            !finite(sample.rigid_state.angular_rate.value) ||
            !std::isfinite(mass.mass_kilograms) ||
            !finite(mass.body_origin_to_center_of_mass.value) ||
            !finite(mass.inertia_about_center_of_mass.value)) {
            return mass_commit_failure<CommittedMissionResultOutput>(
                kCommittedMissionResultKernelIdentity,
                NumericalStatus::NonFiniteInput,
                "committed-sample-state", validation_flags);
        }
        const double attitude_norm =
            sample.rigid_state.attitude.value.norm();
        if (mass.mass_kilograms <= 0.0 ||
            !std::isfinite(attitude_norm) ||
            !near(attitude_norm, 1.0, policy)) {
            return mass_commit_failure<CommittedMissionResultOutput>(
                kCommittedMissionResultKernelIdentity,
                NumericalStatus::DomainError,
                "committed-sample-domain", validation_flags);
        }
        const auto inertia = gnc::foundation::solve_spd_3x3(
            mass.inertia_about_center_of_mass.value,
            Vec3::Zero(), policy);
        validation_flags |= inertia.evidence().flags;
        validation_evaluations += inertia.evidence().evaluations;
        if (!inertia.has_value()) {
            return mass_commit_failure<CommittedMissionResultOutput>(
                kCommittedMissionResultKernelIdentity,
                inertia.status(), "committed-sample-inertia",
                validation_flags);
        }
        if (index > 0U) {
            const auto& previous = input.committed_samples[index - 1U];
            if (rigid_context.sample_time.tick !=
                    previous.rigid_context.sample_time.tick + 1 ||
                rigid_context.sample_time.seconds <=
                    previous.rigid_context.sample_time.seconds ||
                (mass.mass_kilograms >
                     previous.mass_state.mass_kilograms &&
                 !near(mass.mass_kilograms,
                       previous.mass_state.mass_kilograms, policy))) {
                return mass_commit_failure<CommittedMissionResultOutput>(
                    kCommittedMissionResultKernelIdentity,
                    NumericalStatus::DomainError,
                    "committed-sample-sequence", validation_flags);
            }
        }
    }

    const auto& initial = input.committed_samples[0];
    const double initial_time = initial.rigid_context.sample_time.seconds;
    const double initial_downrange =
        initial.rigid_state.position.value(0);
    const double initial_altitude =
        initial.rigid_state.position.value(2);
    const double initial_mass = initial.mass_state.mass_kilograms;
    MissionMetricSummary summary;
    bool summary_initialized = false;
    for (std::size_t sample_index = 0U;
         sample_index < input.committed_samples.size(); ++sample_index) {
        const auto& sample = input.committed_samples[sample_index];
        MissionMetrics metrics;
        metrics.duration_seconds =
            sample.rigid_context.sample_time.seconds - initial_time;
        metrics.downrange_meters =
            sample.rigid_state.position.value(0) - initial_downrange;
        metrics.vertical_displacement_meters =
            sample.rigid_state.position.value(2) - initial_altitude;
        metrics.remaining_mass_kilograms =
            sample.mass_state.mass_kilograms;
        metrics.consumed_mass_kilograms =
            initial_mass - sample.mass_state.mass_kilograms;
        metrics.speed_meters_per_second =
            sample.rigid_state.velocity.value.norm();
        if (!std::isfinite(metrics.duration_seconds) ||
            !std::isfinite(metrics.downrange_meters) ||
            !std::isfinite(metrics.vertical_displacement_meters) ||
            !std::isfinite(metrics.remaining_mass_kilograms) ||
            !std::isfinite(metrics.consumed_mass_kilograms) ||
            !std::isfinite(metrics.speed_meters_per_second) ||
            metrics.duration_seconds < 0.0 ||
            (metrics.consumed_mass_kilograms < 0.0 &&
             !near(metrics.consumed_mass_kilograms, 0.0, policy))) {
            return mass_commit_failure<CommittedMissionResultOutput>(
                kCommittedMissionResultKernelIdentity,
                NumericalStatus::NonFiniteIntermediate,
                "mission-metrics", validation_flags);
        }

        const std::int64_t tick =
            sample.rigid_context.sample_time.tick;
        summary.evaluated_sample_count = sample_index + 1U;
        summary.terminal = metrics;
        if (!summary_initialized ||
            metrics.speed_meters_per_second >
                summary.peak_speed_meters_per_second) {
            summary.peak_speed_meters_per_second =
                metrics.speed_meters_per_second;
            summary.peak_speed_tick = tick;
        }
        if (!summary_initialized ||
            metrics.downrange_meters >
                summary.maximum_downrange_meters) {
            summary.maximum_downrange_meters =
                metrics.downrange_meters;
            summary.maximum_downrange_tick = tick;
        }
        if (!summary_initialized ||
            metrics.remaining_mass_kilograms <
                summary.minimum_remaining_mass_kilograms) {
            summary.minimum_remaining_mass_kilograms =
                metrics.remaining_mass_kilograms;
            summary.minimum_remaining_mass_tick = tick;
        }
        summary_initialized = true;

        std::array<MissionPredicateEvaluation, 3U> evaluations;
        const MissionTerminationPredicate* selected = nullptr;
        for (std::size_t predicate_index = 0U;
             predicate_index < definition.predicates.size();
             ++predicate_index) {
            const auto& predicate =
                definition.predicates[predicate_index];
            double observed = 0.0;
            switch (predicate.metric) {
            case MissionMetric::DurationSeconds:
                observed = metrics.duration_seconds;
                break;
            case MissionMetric::DownrangeMeters:
                observed = metrics.downrange_meters;
                break;
            case MissionMetric::RemainingMassKilograms:
                observed = metrics.remaining_mass_kilograms;
                break;
            }
            const bool met =
                predicate.relation == MissionRelation::LessThanOrEqual
                    ? observed <= predicate.threshold
                    : observed >= predicate.threshold;
            evaluations[predicate_index] = {
                predicate.predicate_id,
                observed,
                met,
                predicate.action,
                predicate.reason_code,
                predicate.priority,
            };
            if (met &&
                (selected == nullptr ||
                 predicate.priority > selected->priority ||
                 (predicate.priority == selected->priority &&
                  predicate.predicate_id < selected->predicate_id))) {
                selected = &predicate;
            }
        }
        if (selected != nullptr) {
            CommittedMissionResultOutput output;
            output.status = selected->action == MissionAction::Complete
                                ? MissionResultStatus::Completed
                                : MissionResultStatus::Aborted;
            output.initial_tick =
                initial.rigid_context.sample_time.tick;
            output.final_tick = tick;
            output.final_time_seconds =
                sample.rigid_context.sample_time.seconds;
            output.termination = {
                selected->action,
                selected->reason_code,
                output.final_time_seconds,
                selected->priority,
            };
            output.metrics = summary;
            output.terminal_predicates = std::move(evaluations);
            output.terminal_boundary = sample;
            NumericalEvidence evidence = mass_commit_evidence(
                kCommittedMissionResultKernelIdentity,
                "first-terminal-committed-sample", validation_flags);
            evidence.evaluations = validation_evaluations +
                                   summary.evaluated_sample_count *
                                       definition.predicates.size();
            evidence.last_step = metrics.duration_seconds;
            return NumericalOutcome<
                CommittedMissionResultOutput>::with_value(
                    validation_flags == 0U
                        ? NumericalStatus::Success
                        : NumericalStatus::Approximate,
                    std::move(output), evidence);
        }
    }
    return mass_commit_failure<CommittedMissionResultOutput>(
        kCommittedMissionResultKernelIdentity,
        NumericalStatus::DomainError, "no-terminal-committed-sample",
        validation_flags);
}

NumericalOutcome<TwoIntervalMassCommitOutput>
TwoIntervalMassCommitKernel::evaluate(
    const PreparedRigidStepModel& rigid_model,
    const ScalarBurnMassDefinition& mass_definition,
    const TwoIntervalMassCommitInput& input) {
    const auto first = FrozenRigidMassStepKernel::evaluate(
        rigid_model, mass_definition, input.opening_boundary,
        input.intervals[0]);
    if (!first.has_value()) {
        return mass_commit_failure<TwoIntervalMassCommitOutput>(
            kTwoIntervalMassCommitKernelIdentity, first.status(),
            "interval-0", first.evidence().flags);
    }
    CommittedRigidMassBoundary first_commit = promote_candidate(
        input.intervals[0].context, first.value().candidate);

    const auto second = FrozenRigidMassStepKernel::evaluate(
        rigid_model, mass_definition, first_commit, input.intervals[1]);
    if (!second.has_value()) {
        return mass_commit_failure<TwoIntervalMassCommitOutput>(
            kTwoIntervalMassCommitKernelIdentity, second.status(),
            "interval-1", first.evidence().flags |
                              second.evidence().flags);
    }
    CommittedRigidMassBoundary second_commit = promote_candidate(
        input.intervals[1].context, second.value().candidate);

    TwoIntervalMassCommitOutput output;
    output.intervals[0].staged = first.value();
    output.intervals[0].closing_commit = first_commit;
    output.intervals[1].staged = second.value();
    output.intervals[1].closing_commit = second_commit;
    output.terminal_boundary = second_commit;
    const NumericalFlags flags =
        first.evidence().flags | second.evidence().flags;
    NumericalEvidence evidence = mass_commit_evidence(
        kTwoIntervalMassCommitKernelIdentity,
        "two-interval-committed-boundaries", flags);
    evidence.evaluations = first.evidence().evaluations +
                           second.evidence().evaluations;
    evidence.last_step =
        rigid_model.definition().algorithm.fixed_step_seconds;
    return NumericalOutcome<TwoIntervalMassCommitOutput>::with_value(
        approximate_status(first.status()) ||
                approximate_status(second.status())
            ? NumericalStatus::Approximate
            : NumericalStatus::Success,
        std::move(output), evidence);
}

} // namespace gnc::packages::yyz

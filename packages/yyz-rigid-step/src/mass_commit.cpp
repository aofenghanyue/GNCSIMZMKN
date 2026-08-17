#include "../include/yyz/mass_commit.hpp"

#include "gnc/foundation/spd_cholesky_3x3.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
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

[[nodiscard]] bool approximate_status(NumericalStatus status) noexcept {
    return status == NumericalStatus::Approximate ||
           status == NumericalStatus::Extrapolated;
}

[[nodiscard]] CommittedRigidMassBoundary promote_candidate(
    const RigidMassIntervalInput& interval,
    const AtomicRigidMassCandidate& candidate) {
    CommittedRigidMassBoundary committed;
    committed.rigid_context = {
        interval.context.inertial_frame,
        interval.context.clock_domain,
        candidate.effective_at,
        interval.context.configuration_revision,
        DataQuality::Valid,
    };
    committed.rigid_state = candidate.rigid.state;
    committed.mass_state = candidate.mass.state;
    return committed;
}

} // namespace

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
    if (start.tick < 0 ||
        start.tick == std::numeric_limits<std::int64_t>::max() ||
        end.tick != start.tick + 1 || !std::isfinite(start.seconds) ||
        !std::isfinite(end.seconds)) {
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
            rigid_definition.clock_domain,
            interval.context.interval_start,
            rigid_definition.configuration_revision, policy) ||
        !valid_sample_at(
            opening_boundary.mass_state.context,
            rigid_definition.body_frame,
            rigid_definition.clock_domain,
            interval.context.interval_start,
            rigid_definition.configuration_revision, policy) ||
        !valid_interval_at(
            interval.mass_flow.context, rigid_definition.body_frame,
            rigid_definition.clock_domain,
            interval.context.interval_start,
            interval.context.interval_end,
            rigid_definition.configuration_revision, policy) ||
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
        input.intervals[0], first.value().candidate);

    const auto second = FrozenRigidMassStepKernel::evaluate(
        rigid_model, mass_definition, first_commit, input.intervals[1]);
    if (!second.has_value()) {
        return mass_commit_failure<TwoIntervalMassCommitOutput>(
            kTwoIntervalMassCommitKernelIdentity, second.status(),
            "interval-1", first.evidence().flags |
                              second.evidence().flags);
    }
    CommittedRigidMassBoundary second_commit = promote_candidate(
        input.intervals[1], second.value().candidate);

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

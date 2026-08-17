#pragma once

#include <yyz/rigid_step.hpp>

#include <array>
#include <string>
#include <string_view>

namespace gnc::packages::yyz {

inline constexpr std::string_view kScalarBurnMassContractIdentity =
    "gnc.package.yyz.mass.scalar-burn.contract.experimental@1";
inline constexpr std::string_view kScalarBurnMassModelIdentity =
    "gnc.package.yyz.mass.scalar-burn-constant-geometry.experimental@1";
inline constexpr std::string_view kTwoIntervalMassCommitContractIdentity =
    "gnc.package.yyz.rigid-mass-boundary.contract.experimental@1";
inline constexpr std::string_view kTwoIntervalMassCommitModelIdentity =
    "gnc.package.yyz.two-interval-mass-commit.experimental@1";
inline constexpr std::string_view kSuppliedPropulsionContractIdentity =
    "gnc.package.yyz.propulsion-response.contract.experimental@1";
inline constexpr std::string_view kSuppliedPropulsionModelIdentity =
    "gnc.package.yyz.propulsion-response.supplied.experimental@1";

inline constexpr gnc::foundation::AlgorithmIdentity
    kScalarBurnMassKernelIdentity{
        "gnc.package.yyz.mass.scalar-burn.kernel@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kFrozenRigidMassStepKernelIdentity{
        "gnc.package.yyz.rigid-mass.frozen-step.kernel@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kTwoIntervalMassCommitKernelIdentity{
        "gnc.package.yyz.rigid-mass.two-interval.kernel@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kSuppliedPropulsionKernelIdentity{
        "gnc.package.yyz.propulsion-response.supplied.kernel@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kPropelledFrozenRigidMassStepKernelIdentity{
        "gnc.package.yyz.rigid-mass.propelled-frozen-step.kernel@1",
        "0.1.0"};

struct ScalarBurnMassDefinition {
    std::string model_id;
    std::string model_version;
    std::string mass_state_id;
};

struct MassState {
    gnc::contracts::SampleContext context;
    std::string mass_state_id;
    double mass_kilograms = 0.0;
    BodyPointMeters body_origin_to_center_of_mass;
    BodyInertiaKilogramMetersSquared inertia_about_center_of_mass;
};

struct MassFlowIntervalInput {
    gnc::contracts::IntervalSampleContext context;
    std::string mass_state_id;
    double fuel_consumption_rate_kilograms_per_second = 0.0;
};

struct BodyThrustDirectionUnit {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::UnitX();
};

struct BodyCenterOfMassToApplicationMeters {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct SuppliedPropulsionDefinition {
    std::string model_id;
    std::string model_version;
    std::string source_id;
    gnc::contracts::FrameIdentity body_frame;
    gnc::contracts::ClockDomainIdentity clock_domain;
    std::string mass_state_id;
    gnc::foundation::NumericalPolicy numerical_policy;
};

struct SuppliedPropulsionInput {
    gnc::contracts::IntervalSampleContext context;
    double thrust_magnitude_newtons = 0.0;
    BodyThrustDirectionUnit thrust_direction;
    BodyCenterOfMassToApplicationMeters
        center_of_mass_to_application;
    BodyMomentNewtonMeters intrinsic_moment_at_application;
    double fuel_consumption_rate_kilograms_per_second = 0.0;
};

// This response intentionally keeps its application geometry relative to the
// committed center of mass. The rigid closure remains the sole wrench-
// transport owner after the atomic-boundary adapter supplies body-origin
// geometry from the committed MassState.
struct SuppliedPropulsionBodyWrench {
    gnc::contracts::IntervalSampleContext context;
    std::string source_id;
    BodyForceNewtons force;
    BodyCenterOfMassToApplicationMeters
        center_of_mass_to_application;
    BodyMomentNewtonMeters intrinsic_moment_at_application;
};

struct SuppliedPropulsionOutput {
    SuppliedPropulsionBodyWrench supplied_body_wrench;
    BodyMomentNewtonMeters lever_arm_moment;
    BodyMomentNewtonMeters moment_about_center_of_mass;
    MassFlowIntervalInput mass_flow;
};

class SuppliedPropulsionKernel {
  public:
    [[nodiscard]] static
        gnc::foundation::NumericalOutcome<SuppliedPropulsionOutput>
        evaluate(const SuppliedPropulsionDefinition& definition,
                 const SuppliedPropulsionInput& input);
};

struct MassStateCandidate {
    gnc::contracts::SimulationInstant effective_at;
    MassState state;
};

struct ScalarBurnMassOutput {
    double current_committed_mass_kilograms = 0.0;
    double integration_mass_kilograms = 0.0;
    double consumed_mass_kilograms = 0.0;
    MassStateCandidate candidate;
};

class ScalarBurnMassKernel {
  public:
    [[nodiscard]] static
        gnc::foundation::NumericalOutcome<ScalarBurnMassOutput>
        evaluate(const ScalarBurnMassDefinition& definition,
                 const MassState& committed_state,
                 const MassFlowIntervalInput& flow,
                 const gnc::foundation::NumericalPolicy& policy);
};

struct CommittedRigidMassBoundary {
    gnc::contracts::SampleContext rigid_context;
    RigidState rigid_state;
    MassState mass_state;
};

struct RigidMassIntervalInput {
    RigidStepContext context;
    EnvironmentInput environment;
    AppliedBodyWrenchInput supplied_wrench;
    MassFlowIntervalInput mass_flow;
};

struct AtomicRigidMassCandidate {
    gnc::contracts::SimulationInstant effective_at;
    RigidStateCandidate rigid;
    MassStateCandidate mass;
};

struct FrozenRigidMassStepOutput {
    CommittedRigidMassBoundary opening_boundary;
    MassPropertiesInput projected_committed_mass;
    RigidStepOutput rigid_step;
    ScalarBurnMassOutput mass_evolution;
    AtomicRigidMassCandidate candidate;
};

class FrozenRigidMassStepKernel {
  public:
    [[nodiscard]] static
        gnc::foundation::NumericalOutcome<FrozenRigidMassStepOutput>
        evaluate(const PreparedRigidStepModel& rigid_model,
                 const ScalarBurnMassDefinition& mass_definition,
                 const CommittedRigidMassBoundary& opening_boundary,
                 const RigidMassIntervalInput& interval);
};

struct PropelledRigidMassIntervalInput {
    RigidStepContext context;
    EnvironmentInput environment;
    SuppliedPropulsionInput propulsion;
};

struct PropelledFrozenRigidMassStepOutput {
    SuppliedPropulsionOutput propulsion;
    FrozenRigidMassStepOutput atomic_boundary;
};

class PropelledFrozenRigidMassStepKernel {
  public:
    [[nodiscard]] static gnc::foundation::NumericalOutcome<
        PropelledFrozenRigidMassStepOutput>
        evaluate(const PreparedRigidStepModel& rigid_model,
                 const ScalarBurnMassDefinition& mass_definition,
                 const SuppliedPropulsionDefinition& propulsion_definition,
                 const CommittedRigidMassBoundary& opening_boundary,
                 const PropelledRigidMassIntervalInput& interval);
};

struct TwoIntervalMassCommitInput {
    CommittedRigidMassBoundary opening_boundary;
    std::array<RigidMassIntervalInput, 2U> intervals;
};

struct TwoIntervalMassCommitIntervalOutput {
    FrozenRigidMassStepOutput staged;
    CommittedRigidMassBoundary closing_commit;
};

struct TwoIntervalMassCommitOutput {
    std::array<TwoIntervalMassCommitIntervalOutput, 2U> intervals;
    CommittedRigidMassBoundary terminal_boundary;
};

class TwoIntervalMassCommitKernel {
  public:
    [[nodiscard]] static
        gnc::foundation::NumericalOutcome<TwoIntervalMassCommitOutput>
        evaluate(const PreparedRigidStepModel& rigid_model,
                 const ScalarBurnMassDefinition& mass_definition,
                 const TwoIntervalMassCommitInput& input);
};

} // namespace gnc::packages::yyz

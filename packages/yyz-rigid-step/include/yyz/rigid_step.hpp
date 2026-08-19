#pragma once

#include "gnc/contracts/sample_context.hpp"
#include "gnc/foundation/linear_algebra.hpp"
#include "gnc/foundation/numerical_outcome.hpp"
#include "gnc/foundation/numerical_policy.hpp"
#include "gnc/foundation/passive_quaternion.hpp"
#include "gnc/foundation/trilinear_table.hpp"
#include "gnc/model_sdk/algorithm_evaluation.hpp"
#include "gnc/model_sdk/model_metadata.hpp"
#include "gnc/model_sdk/static_descriptor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gnc::packages::yyz {

inline constexpr std::string_view kRigidStepContractIdentity =
    "gnc.package.yyz.rigid-step.contract.experimental@1";
inline constexpr std::string_view kYyzRigidStepPackageIdentity =
    "gnc.package.yyz-rigid-step.experimental@1";
inline constexpr std::string_view kYyzRigidStepPackageVersion = "0.1.0";
inline constexpr std::string_view kRigidStepModelIdentity =
    "gnc.package.yyz.rigid-step.frozen-interval.experimental@1";
inline constexpr std::string_view kForceMomentClosureModelIdentity =
    "gnc.package.yyz.force-moment-closure.frozen-interval.experimental@1";
inline constexpr std::string_view kForceMomentClosureModelVersion = "0.1.0";
inline constexpr std::string_view kAerodynamicTableModelIdentity =
    "gnc.package.yyz.aerodynamic-table.multiaffine.experimental@1";
inline constexpr std::string_view kAerodynamicTableModelVersion = "0.1.0";
inline constexpr std::string_view kForceMomentClosureConfigSchemaIdentity =
    "gnc.package.yyz.force-moment-closure.config@1";
inline constexpr std::uint32_t kForceMomentClosureConfigSchemaVersion = 1U;
inline constexpr std::string_view kAerodynamicTableConfigSchemaIdentity =
    "gnc.package.yyz.aerodynamic-table.config@1";
inline constexpr std::uint32_t kAerodynamicTableConfigSchemaVersion = 1U;
inline constexpr std::string_view kAerodynamicTableAssetSchemaIdentity =
    "gnc.asset.yyz.aerodynamic-table.multiaffine@1";
inline constexpr std::string_view kAerodynamicCoefficientsContractIdentity =
    "gnc.contract.yyz.aerodynamic-coefficients@1";
inline constexpr std::string_view kRigidFormInputContractIdentity =
    "gnc.contract.yyz.rigid-form-input@1";
inline constexpr gnc::foundation::AlgorithmIdentity
    kRigidStepPreparationIdentity{
        "gnc.package.yyz.rigid-step.prepare@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kForceMomentClosurePreparationIdentity{
        "gnc.package.yyz.force-moment-closure.prepare@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kForceMomentClosureKernelIdentity{
        "gnc.package.yyz.force-moment-closure.kernel@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kAerodynamicTablePreparationIdentity{
        "gnc.package.yyz.aerodynamic-table.prepare@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kAerodynamicTableQueryIdentity{
        "gnc.package.yyz.aerodynamic-table.query@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity kRigidStepKernelIdentity{
    "gnc.package.yyz.rigid-step.kernel@1", "0.1.0"};

struct InertialPositionMeters {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct InertialVelocityMetersPerSecond {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct BodyVelocityMetersPerSecond {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct InertialAccelerationMetersPerSecondSquared {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct PassiveAttitudeIFromB {
    gnc::foundation::QuaternionStorage value =
        gnc::foundation::quaternion_from_wxyz(1.0, 0.0, 0.0, 0.0);
};

struct BodyAngularRateRadiansPerSecond {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct BodyAngularAccelerationRadiansPerSecondSquared {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct BodyPointMeters {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct BodyForceNewtons {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct InertialForceNewtons {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct BodyMomentNewtonMeters {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct BodyInertiaKilogramMetersSquared {
    gnc::foundation::Mat3 value = gnc::foundation::Mat3::Zero();
};

struct BodyAngularMomentumKilogramMetersSquaredPerSecond {
    gnc::foundation::Vec3 value = gnc::foundation::Vec3::Zero();
};

struct PassiveAttitudeDerivativeIFromBPerSecond {
    gnc::foundation::QuaternionStorage value =
        gnc::foundation::quaternion_from_wxyz(0.0, 0.0, 0.0, 0.0);
};

struct RigidState {
    InertialPositionMeters position;
    InertialVelocityMetersPerSecond velocity;
    PassiveAttitudeIFromB attitude;
    BodyAngularRateRadiansPerSecond angular_rate;
};

struct RigidStepContext {
    gnc::contracts::FrameIdentity inertial_frame;
    gnc::contracts::FrameIdentity body_frame;
    gnc::contracts::ClockDomainIdentity clock_domain;
    gnc::contracts::SimulationInstant interval_start;
    gnc::contracts::SimulationInstant interval_end;
    std::int64_t configuration_revision = 0;
    gnc::contracts::DataQuality quality =
        gnc::contracts::DataQuality::Invalid;
};

struct EnvironmentInput {
    gnc::contracts::SampleContext context;
    InertialAccelerationMetersPerSecondSquared gravity;
    InertialVelocityMetersPerSecond velocity_airmass;
    double density_kilograms_per_cubic_meter = 0.0;
    double speed_of_sound_meters_per_second = 0.0;
};

struct MassPropertiesInput {
    gnc::contracts::IntervalSampleContext context;
    std::string mass_state_id;
    double mass_kilograms = 0.0;
    BodyPointMeters body_origin_to_center_of_mass;
    BodyInertiaKilogramMetersSquared inertia_about_center_of_mass;
};

// This input is an already evaluated source contribution. The package does
// not model propulsion, actuation, or source state in this slice.
struct AppliedBodyWrenchInput {
    gnc::contracts::IntervalSampleContext context;
    std::string source_id;
    BodyForceNewtons force;
    BodyPointMeters body_origin_to_application;
    BodyMomentNewtonMeters intrinsic_moment_at_application;
};

struct RigidStepAlgorithmDefinition {
    double fixed_step_seconds = 0.0;
    gnc::foundation::NumericalPolicy numerical_policy;
    gnc::foundation::QuaternionPolicy attitude_evaluation_policy;
    gnc::foundation::QuaternionPolicy candidate_attitude_policy;
};

struct AerodynamicTableAsset {
    std::string asset_schema_id;
    std::string asset_id;
    std::vector<double> mach_axis;
    std::vector<double> alpha_axis_radians;
    std::vector<double> beta_axis_radians;
    std::vector<std::array<double, 6U>>
        coefficient_rows_ca_cy_cn_cl_cm_cn;
};

struct AerodynamicTableDefinition {
    gnc::model_sdk::ModelDefinitionMetadata metadata;
    std::string source_id;
    std::string configuration_id;
    double reference_area_square_meters = 0.0;
    double reference_span_meters = 0.0;
    double reference_chord_meters = 0.0;
    BodyPointMeters body_origin_to_application;
    std::string table_asset_id;
};

[[nodiscard]] gnc::model_sdk::CanonicalConfigBlock
canonical_aerodynamic_table_config(
    const AerodynamicTableDefinition& definition);

[[nodiscard]] gnc::foundation::NumericalOutcome<
    AerodynamicTableDefinition>
build_aerodynamic_table_definition(
    const gnc::model_sdk::CanonicalConfigBlock& configuration,
    std::string table_asset_id);

class PreparedAerodynamicTableModel {
  public:
    PreparedAerodynamicTableModel(
        const PreparedAerodynamicTableModel&) = default;
    PreparedAerodynamicTableModel(
        PreparedAerodynamicTableModel&&) noexcept = default;
    PreparedAerodynamicTableModel& operator=(
        const PreparedAerodynamicTableModel&) = default;
    PreparedAerodynamicTableModel& operator=(
        PreparedAerodynamicTableModel&&) noexcept = default;

    [[nodiscard]] const AerodynamicTableDefinition& definition()
        const noexcept;
    [[nodiscard]] const AerodynamicTableAsset& asset() const noexcept;
    [[nodiscard]] const gnc::model_sdk::PreparedModelMetadata& metadata()
        const noexcept;

  private:
    PreparedAerodynamicTableModel(
        std::shared_ptr<const AerodynamicTableDefinition> definition,
        std::shared_ptr<const AerodynamicTableAsset> asset,
        gnc::foundation::PreparedTrilinearTableView<6U> table,
        gnc::model_sdk::PreparedModelMetadata metadata) noexcept;

    std::shared_ptr<const AerodynamicTableDefinition> definition_;
    std::shared_ptr<const AerodynamicTableAsset> asset_;
    gnc::foundation::PreparedTrilinearTableView<6U> table_;
    gnc::model_sdk::PreparedModelMetadata metadata_;

    friend gnc::foundation::NumericalOutcome<
        PreparedAerodynamicTableModel>
    prepare_aerodynamic_table_model(
        AerodynamicTableDefinition definition,
        AerodynamicTableAsset asset);
    friend class AerodynamicTableQueryKernel;
};

[[nodiscard]] gnc::foundation::NumericalOutcome<
    PreparedAerodynamicTableModel>
prepare_aerodynamic_table_model(
    AerodynamicTableDefinition definition,
    AerodynamicTableAsset asset);

struct AerodynamicTableQueryInput {
    double mach = 0.0;
    double alpha_radians = 0.0;
    double beta_radians = 0.0;
};

struct AerodynamicTableQueryOutput {
    std::array<double, 6U> coefficients_ca_cy_cn_cl_cm_cn{};
};

struct AerodynamicTableQueryTelemetry {
    gnc::foundation::InterpolationDomainStatus domain_status =
        gnc::foundation::InterpolationDomainStatus::Inside;
    std::array<double, 3U> weights{};
};

using AerodynamicTableQueryEvaluation =
    gnc::model_sdk::AlgorithmEvaluation<AerodynamicTableQueryOutput,
                                        AerodynamicTableQueryTelemetry>;

class AerodynamicTableQueryKernel {
  public:
    [[nodiscard]] static gnc::foundation::NumericalOutcome<
        AerodynamicTableQueryEvaluation>
    evaluate(const PreparedAerodynamicTableModel& model,
             const AerodynamicTableQueryInput& input);
};

struct ForceMomentClosureDefinition {
    gnc::model_sdk::ModelDefinitionMetadata metadata;
    gnc::contracts::FrameIdentity body_frame;
    gnc::contracts::ClockDomainIdentity clock_domain;
    std::int64_t configuration_revision = -1;
    gnc::foundation::NumericalPolicy numerical_policy;
};

[[nodiscard]] gnc::model_sdk::CanonicalConfigBlock
canonical_force_moment_closure_config(
    const ForceMomentClosureDefinition& definition);

[[nodiscard]] gnc::foundation::NumericalOutcome<
    ForceMomentClosureDefinition>
build_force_moment_closure_definition(
    const gnc::model_sdk::CanonicalConfigBlock& configuration);

class PreparedForceMomentClosureModel {
  public:
    PreparedForceMomentClosureModel(
        const PreparedForceMomentClosureModel&) = default;
    PreparedForceMomentClosureModel(
        PreparedForceMomentClosureModel&&) noexcept = default;
    PreparedForceMomentClosureModel& operator=(
        const PreparedForceMomentClosureModel&) = default;
    PreparedForceMomentClosureModel& operator=(
        PreparedForceMomentClosureModel&&) noexcept = default;

    [[nodiscard]] const ForceMomentClosureDefinition& definition()
        const noexcept;
    [[nodiscard]] const gnc::model_sdk::PreparedModelMetadata& metadata()
        const noexcept;

  private:
    PreparedForceMomentClosureModel(
        std::shared_ptr<const ForceMomentClosureDefinition> definition,
        gnc::model_sdk::PreparedModelMetadata metadata) noexcept;

    std::shared_ptr<const ForceMomentClosureDefinition> definition_;
    gnc::model_sdk::PreparedModelMetadata metadata_;

    friend gnc::foundation::NumericalOutcome<
        PreparedForceMomentClosureModel>
    prepare_force_moment_closure_model(
        ForceMomentClosureDefinition definition);
};

[[nodiscard]] gnc::foundation::NumericalOutcome<
    PreparedForceMomentClosureModel>
prepare_force_moment_closure_model(
    ForceMomentClosureDefinition definition);

struct RigidStepModelDefinition {
    gnc::contracts::FrameIdentity inertial_frame;
    ForceMomentClosureDefinition force_moment_closure;
    RigidStepAlgorithmDefinition algorithm;
    AerodynamicTableDefinition aerodynamics;
    AerodynamicTableAsset aerodynamic_table;
};

[[nodiscard]] inline gnc::model_sdk::StaticPackageDescriptor
describe_yyz_rigid_step_package() {
    gnc::model_sdk::StaticModelDescriptor closure;
    closure.definition = {
        std::string(kForceMomentClosureModelIdentity),
        std::string(kForceMomentClosureModelVersion),
        gnc::model_sdk::ModelExecutionForm::Closure};
    closure.placement =
        gnc::model_sdk::ModelPlacement::InteractionClosure;
    closure.preparation_algorithm_id =
        std::string(kForceMomentClosurePreparationIdentity.id);
    closure.preparation_algorithm_version =
        std::string(kForceMomentClosurePreparationIdentity.version);
    closure.configuration.schema_id =
        std::string(kForceMomentClosureConfigSchemaIdentity);
    closure.configuration.schema_version =
        kForceMomentClosureConfigSchemaVersion;
    closure.configuration.fields = {
        {"body_frame_id",
         gnc::model_sdk::CanonicalConfigValueKind::String},
        {"clock_domain_id",
         gnc::model_sdk::CanonicalConfigValueKind::String},
        {"configuration_revision",
         gnc::model_sdk::CanonicalConfigValueKind::Integer},
        {"numerical.absolute_tolerance",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
        {"numerical.condition_limit",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
        {"numerical.finite_check",
         gnc::model_sdk::CanonicalConfigValueKind::Enum},
        {"numerical.relative_tolerance",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
        {"numerical.zero_tolerance",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
    };
    closure.ports.push_back(
        {"form-input", std::string(kRigidFormInputContractIdentity),
         gnc::model_sdk::StaticPortDirection::Output,
         gnc::model_sdk::BindingKind::ContinuousClosureLink,
         gnc::model_sdk::PortCardinality::OneOrMore,
         gnc::model_sdk::TemporalRelation::IntervalModel});

    gnc::model_sdk::StaticModelDescriptor aerodynamics;
    aerodynamics.definition = {
        std::string(kAerodynamicTableModelIdentity),
        std::string(kAerodynamicTableModelVersion),
        gnc::model_sdk::ModelExecutionForm::PureQuery};
    aerodynamics.placement =
        gnc::model_sdk::ModelPlacement::VehicleOutput;
    aerodynamics.preparation_algorithm_id =
        std::string(kAerodynamicTablePreparationIdentity.id);
    aerodynamics.preparation_algorithm_version =
        std::string(kAerodynamicTablePreparationIdentity.version);
    aerodynamics.configuration.schema_id =
        std::string(kAerodynamicTableConfigSchemaIdentity);
    aerodynamics.configuration.schema_version =
        kAerodynamicTableConfigSchemaVersion;
    aerodynamics.configuration.fields = {
        {"body_origin_to_application.x_m",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
        {"body_origin_to_application.y_m",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
        {"body_origin_to_application.z_m",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
        {"configuration_id",
         gnc::model_sdk::CanonicalConfigValueKind::String},
        {"reference_area_square_meters",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
        {"reference_chord_meters",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
        {"reference_span_meters",
         gnc::model_sdk::CanonicalConfigValueKind::Float64},
        {"source_id",
         gnc::model_sdk::CanonicalConfigValueKind::String},
    };
    aerodynamics.asset_slots.push_back(
        {"aerodynamics",
         std::string(kAerodynamicTableAssetSchemaIdentity),
         gnc::model_sdk::PortCardinality::ExactlyOne});
    aerodynamics.ports.push_back(
        {"coefficients",
         std::string(kAerodynamicCoefficientsContractIdentity),
         gnc::model_sdk::StaticPortDirection::Output,
         gnc::model_sdk::BindingKind::PureQuery,
         gnc::model_sdk::PortCardinality::OneOrMore,
         gnc::model_sdk::TemporalRelation::NotApplicable});

    gnc::model_sdk::StaticAlgorithmDescriptor rigid_step;
    rigid_step.algorithm_id = std::string(kRigidStepKernelIdentity.id);
    rigid_step.algorithm_version =
        std::string(kRigidStepKernelIdentity.version);
    rigid_step.ports.push_back(
        {"aerodynamic-coefficients",
         std::string(kAerodynamicCoefficientsContractIdentity),
         gnc::model_sdk::StaticPortDirection::Input,
         gnc::model_sdk::BindingKind::PureQuery,
         gnc::model_sdk::PortCardinality::ExactlyOne,
         gnc::model_sdk::TemporalRelation::NotApplicable});
    rigid_step.ports.push_back(
        {"form-input", std::string(kRigidFormInputContractIdentity),
         gnc::model_sdk::StaticPortDirection::Input,
         gnc::model_sdk::BindingKind::ContinuousClosureLink,
         gnc::model_sdk::PortCardinality::ExactlyOne,
         gnc::model_sdk::TemporalRelation::IntervalModel});

    gnc::model_sdk::StaticPackageDescriptor package;
    package.package_id = std::string(kYyzRigidStepPackageIdentity);
    package.package_version = std::string(kYyzRigidStepPackageVersion);
    package.models.push_back(std::move(closure));
    package.models.push_back(std::move(aerodynamics));
    package.algorithms.push_back(std::move(rigid_step));
    return package;
}

class PreparedRigidStepModel {
  public:
    PreparedRigidStepModel(const PreparedRigidStepModel&) = default;
    PreparedRigidStepModel(PreparedRigidStepModel&&) noexcept = default;
    PreparedRigidStepModel& operator=(const PreparedRigidStepModel&) = default;
    PreparedRigidStepModel& operator=(PreparedRigidStepModel&&) noexcept =
        default;

    [[nodiscard]] const RigidStepModelDefinition& definition() const noexcept;
    [[nodiscard]] const PreparedForceMomentClosureModel&
    force_moment_closure_model()
        const noexcept;
    [[nodiscard]] const PreparedAerodynamicTableModel&
    aerodynamic_table_model() const noexcept;

  private:
    PreparedRigidStepModel(
        std::shared_ptr<const RigidStepModelDefinition> definition,
        PreparedAerodynamicTableModel aerodynamic_table_model,
        PreparedForceMomentClosureModel force_moment_closure_model) noexcept;

    std::shared_ptr<const RigidStepModelDefinition> definition_;
    PreparedAerodynamicTableModel aerodynamic_table_model_;
    PreparedForceMomentClosureModel force_moment_closure_model_;

    friend gnc::foundation::NumericalOutcome<PreparedRigidStepModel>
    prepare_rigid_step_model(RigidStepModelDefinition definition);
    friend class RigidStepKernel;
};

[[nodiscard]] gnc::foundation::NumericalOutcome<PreparedRigidStepModel>
prepare_rigid_step_model(RigidStepModelDefinition definition);

struct RigidStepInput {
    RigidStepContext context;
    RigidState committed_state;
    EnvironmentInput environment;
    MassPropertiesInput mass_properties;
    AppliedBodyWrenchInput supplied_wrench;
};

struct AirDataOutput {
    InertialVelocityMetersPerSecond velocity_relative_inertial;
    BodyVelocityMetersPerSecond velocity_relative_body;
    double airspeed_meters_per_second = 0.0;
    double alpha_radians = 0.0;
    double beta_radians = 0.0;
    double dynamic_pressure_pascals = 0.0;
    double mach = 0.0;
};

struct AerodynamicLookupOutput {
    gnc::foundation::InterpolationDomainStatus domain_status =
        gnc::foundation::InterpolationDomainStatus::Inside;
    std::array<double, 3U> weights{};
    std::array<double, 6U> coefficients_ca_cy_cn_cl_cm_cn{};
};

struct BodyWrenchContribution {
    std::string source_id;
    BodyForceNewtons force;
    BodyMomentNewtonMeters moment_about_center_of_mass;
};

struct RigidFormInput {
    BodyForceNewtons force_total;
    BodyMomentNewtonMeters moment_total_about_center_of_mass;
};

struct ForceMomentClosureInput {
    BodyPointMeters body_origin_to_center_of_mass;
    std::vector<AppliedBodyWrenchInput> contributions;
};

struct ForceMomentClosureOutput {
    BodyForceNewtons force_total;
    BodyMomentNewtonMeters moment_total_about_center_of_mass;

    [[nodiscard]] RigidFormInput form_input() const noexcept {
        return {force_total, moment_total_about_center_of_mass};
    }
};

struct ForceMomentClosureTelemetry {
    std::vector<BodyWrenchContribution> contributions;
};

using ForceMomentClosureEvaluation =
    gnc::model_sdk::AlgorithmEvaluation<ForceMomentClosureOutput,
                                        ForceMomentClosureTelemetry>;

class ForceMomentClosureKernel {
  public:
    [[nodiscard]] static gnc::foundation::NumericalOutcome<
        ForceMomentClosureEvaluation>
    evaluate(const PreparedForceMomentClosureModel& model,
             const ForceMomentClosureInput& input);
};

struct RigidDerivativeOutput {
    InertialForceNewtons force_total_inertial;
    InertialAccelerationMetersPerSecondSquared acceleration;
    BodyAngularMomentumKilogramMetersSquaredPerSecond angular_momentum;
    BodyMomentNewtonMeters gyroscopic_moment;
    BodyMomentNewtonMeters net_moment;
    BodyAngularAccelerationRadiansPerSecondSquared angular_acceleration;
    PassiveAttitudeDerivativeIFromBPerSecond attitude_derivative;
};

struct RigidStateCandidate {
    gnc::contracts::SimulationInstant effective_at;
    RigidState state;
};

struct RigidStepOutput {
    RigidStateCandidate candidate;
};

struct RigidStepTelemetry {
    AirDataOutput air_data;
    AerodynamicLookupOutput aerodynamic_lookup;
    ForceMomentClosureEvaluation force_moment_closure;
    RigidDerivativeOutput derivative_at_interval_start;
};

using RigidStepEvaluation =
    gnc::model_sdk::AlgorithmEvaluation<RigidStepOutput,
                                        RigidStepTelemetry>;

class RigidStepKernel {
  public:
    [[nodiscard]] static
        gnc::foundation::NumericalOutcome<RigidStepEvaluation>
    evaluate(const PreparedRigidStepModel& model,
             const RigidStepInput& input);
};

} // namespace gnc::packages::yyz

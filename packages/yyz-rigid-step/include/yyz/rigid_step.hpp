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

struct AerodynamicTableDefinition {
    std::string source_id;
    std::string table_id;
    std::string configuration_id;
    double reference_area_square_meters = 0.0;
    double reference_span_meters = 0.0;
    double reference_chord_meters = 0.0;
    BodyPointMeters body_origin_to_application;
    std::vector<double> mach_axis;
    std::vector<double> alpha_axis_radians;
    std::vector<double> beta_axis_radians;
    std::vector<std::array<double, 6U>>
        coefficient_rows_ca_cy_cn_cl_cm_cn;
};

struct ForceMomentClosureDefinition {
    gnc::model_sdk::ModelDefinitionMetadata metadata;
    gnc::contracts::FrameIdentity body_frame;
    gnc::contracts::ClockDomainIdentity clock_domain;
    std::int64_t configuration_revision = -1;
    gnc::foundation::NumericalPolicy numerical_policy;
};

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
};

[[nodiscard]] inline gnc::model_sdk::StaticPackageDescriptor
describe_yyz_rigid_step_package() {
    gnc::model_sdk::StaticModelDescriptor closure;
    closure.definition = {
        std::string(kForceMomentClosureModelIdentity),
        std::string(kForceMomentClosureModelVersion),
        gnc::model_sdk::ModelExecutionForm::Closure};
    closure.preparation_algorithm_id =
        std::string(kForceMomentClosurePreparationIdentity.id);
    closure.preparation_algorithm_version =
        std::string(kForceMomentClosurePreparationIdentity.version);
    closure.ports.push_back(
        {"form-input", std::string(kRigidFormInputContractIdentity),
         gnc::model_sdk::StaticPortDirection::Output, true});

    gnc::model_sdk::StaticAlgorithmDescriptor rigid_step;
    rigid_step.algorithm_id = std::string(kRigidStepKernelIdentity.id);
    rigid_step.algorithm_version =
        std::string(kRigidStepKernelIdentity.version);
    rigid_step.composition_model_id = std::string(kRigidStepModelIdentity);
    rigid_step.ports.push_back(
        {"form-input", std::string(kRigidFormInputContractIdentity),
         gnc::model_sdk::StaticPortDirection::Input, true});

    gnc::model_sdk::StaticPackageDescriptor package;
    package.package_id = std::string(kYyzRigidStepPackageIdentity);
    package.package_version = std::string(kYyzRigidStepPackageVersion);
    package.models.push_back(std::move(closure));
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

  private:
    PreparedRigidStepModel(
        std::shared_ptr<const RigidStepModelDefinition> definition,
        gnc::foundation::PreparedTrilinearTableView<6U> table,
        PreparedForceMomentClosureModel force_moment_closure_model) noexcept;

    std::shared_ptr<const RigidStepModelDefinition> definition_;
    gnc::foundation::PreparedTrilinearTableView<6U> table_;
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

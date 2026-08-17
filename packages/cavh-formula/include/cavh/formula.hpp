#pragma once

#include "gnc/contracts/sample_context.hpp"
#include "gnc/foundation/numerical_outcome.hpp"
#include "gnc/foundation/numerical_policy.hpp"
#include "gnc/model_sdk/model_metadata.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace gnc::packages::cavh {

inline constexpr std::string_view kCavhFormulaContractIdentity =
    "gnc.package.cavh.formula.contract.experimental@1";
inline constexpr std::string_view kCavhFormulaModelIdentity =
    "gnc.package.cavh.formula.legacy-transcribed.experimental@1";
inline constexpr gnc::foundation::AlgorithmIdentity
    kCavhFormulaPreparationIdentity{
        "gnc.package.cavh.formula.prepare@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kCavhGammaReferenceIdentity{
        "gnc.package.cavh.formula.gamma-reference@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity kCavhTdctIdentity{
    "gnc.package.cavh.formula.tdct@1", "0.1.0"};
inline constexpr gnc::foundation::AlgorithmIdentity
    kCavhFormulaKernelIdentity{
        "gnc.package.cavh.formula.composite@1", "0.1.0"};

enum class GammaReferenceEquation : std::uint8_t {
    Eq17MachDependent,
    Eq18MachIndependent,
};

[[nodiscard]] constexpr std::string_view to_string(
    GammaReferenceEquation equation) noexcept {
    switch (equation) {
    case GammaReferenceEquation::Eq17MachDependent:
        return "eq17";
    case GammaReferenceEquation::Eq18MachIndependent:
        return "eq18";
    }
    return "unknown";
}

enum class TdctSaturation : std::uint8_t {
    None,
    Lower,
    Upper,
};

[[nodiscard]] constexpr std::string_view to_string(
    TdctSaturation saturation) noexcept {
    switch (saturation) {
    case TdctSaturation::None:
        return "none";
    case TdctSaturation::Lower:
        return "lower";
    case TdctSaturation::Upper:
        return "upper";
    }
    return "unknown";
}

struct ParabolicEnvelopeDefinition {
    double cl_intercept = 0.0;
    double cl_slope_per_radian = 0.0;
    double cd0_base = 0.0;
    double cd0_slope_per_mach = 0.0;
    double induced_drag_factor = 0.0;
    double alpha_min_radians = 0.0;
    double alpha_max_radians = 0.0;
};

struct CavhFormulaAlgorithmDefinition {
    GammaReferenceEquation equation =
        GammaReferenceEquation::Eq18MachIndependent;
    double denominator_minimum_absolute = 0.0;
    double derivative_minimum_absolute_seconds_per_meter = 0.0;
    gnc::foundation::NumericalPolicy numerical_policy;
};

struct TdctFormulaDefinition {
    double gain = 0.0;
    double alpha_min_radians = 0.0;
    double alpha_max_radians = 0.0;
};

struct CavhFormulaDefinition {
    gnc::model_sdk::ModelDefinitionMetadata metadata;
    gnc::contracts::FrameIdentity navigation_frame;
    ParabolicEnvelopeDefinition envelope;
    CavhFormulaAlgorithmDefinition algorithm;
    TdctFormulaDefinition tdct;
};

class PreparedCavhFormulaModel {
  public:
    PreparedCavhFormulaModel(const PreparedCavhFormulaModel&) = default;
    PreparedCavhFormulaModel(PreparedCavhFormulaModel&&) noexcept = default;
    PreparedCavhFormulaModel& operator=(
        const PreparedCavhFormulaModel&) = default;
    PreparedCavhFormulaModel& operator=(
        PreparedCavhFormulaModel&&) noexcept = default;

    [[nodiscard]] const CavhFormulaDefinition& definition() const noexcept;
    [[nodiscard]] const gnc::model_sdk::PreparedModelMetadata& metadata()
        const noexcept;

  private:
    explicit PreparedCavhFormulaModel(
        std::shared_ptr<const CavhFormulaDefinition> definition,
        gnc::model_sdk::PreparedModelMetadata metadata) noexcept;

    std::shared_ptr<const CavhFormulaDefinition> definition_;
    gnc::model_sdk::PreparedModelMetadata metadata_;

    friend gnc::foundation::NumericalOutcome<PreparedCavhFormulaModel>
    prepare_cavh_formula_model(CavhFormulaDefinition definition);
};

[[nodiscard]] gnc::foundation::NumericalOutcome<PreparedCavhFormulaModel>
prepare_cavh_formula_model(CavhFormulaDefinition definition);

// Atmosphere and Mach derivatives are supplied products. This package owns
// the accepted CAVH envelope and formula evaluation only.
struct CavhOperatingPointInput {
    double density_kilograms_per_cubic_meter = 0.0;
    double density_altitude_gradient_kilograms_per_cubic_meter_per_meter =
        0.0;
    double altitude_meters = 0.0;
    double reference_radius_meters = 0.0;
    double speed_meters_per_second = 0.0;
    double gravity_meters_per_second_squared = 0.0;
    double mass_kilograms = 0.0;
    double reference_area_square_meters = 0.0;
    double mach = 0.0;
    double mach_speed_partial_seconds_per_meter = 0.0;
    double mach_altitude_partial_per_meter = 0.0;
    double bank_angle_radians = 0.0;
};

struct CavhFormulaInput {
    gnc::contracts::SampleContext context;
    CavhOperatingPointInput operating_point;
    double gamma_measured_radians = 0.0;
};

struct CavhEnvelopeOutput {
    double cd0 = 0.0;
    double cl_star = 0.0;
    double cd_star = 0.0;
    double lift_to_drag_maximum = 0.0;
    double alpha_star_radians = 0.0;
    double dcl_star_dmach = 0.0;
};

struct GammaReferenceIntermediates {
    double density_kilograms_per_cubic_meter = 0.0;
    double density_altitude_gradient_kilograms_per_cubic_meter_per_meter =
        0.0;
    double radius_meters = 0.0;
    double dynamic_pressure_pascals = 0.0;
    double drag_force_newtons = 0.0;
    double cl_vertical = 0.0;
    double dcl_vertical_dmach = 0.0;
    double mach_speed_partial_seconds_per_meter = 0.0;
    double mach_altitude_partial_per_meter = 0.0;
    double dcl_vertical_dspeed_seconds_per_meter = 0.0;
    std::optional<double> b1;
    double b2 = 0.0;
    double b3 = 0.0;
};

struct GammaReferenceOutput {
    gnc::contracts::SampleContext context;
    GammaReferenceEquation equation =
        GammaReferenceEquation::Eq18MachIndependent;
    CavhEnvelopeOutput envelope;
    GammaReferenceIntermediates intermediates;
    double gamma_reference_radians = 0.0;
};

struct TdctFormulaInput {
    gnc::contracts::SampleContext context;
    double alpha_star_radians = 0.0;
    double gamma_reference_radians = 0.0;
    double gamma_measured_radians = 0.0;
};

struct TdctFormulaOutput {
    gnc::contracts::SampleContext context;
    double error_radians = 0.0;
    double correction_radians = 0.0;
    double alpha_raw_radians = 0.0;
    double alpha_limited_radians = 0.0;
    TdctSaturation saturation = TdctSaturation::None;
};

struct CavhFormulaOutput {
    GammaReferenceOutput gamma_reference;
    TdctFormulaOutput tdct;
};

class CavhFormulaKernel {
  public:
    [[nodiscard]] static gnc::foundation::NumericalOutcome<
        GammaReferenceOutput>
    evaluate_gamma_reference(const PreparedCavhFormulaModel& model,
                             const CavhFormulaInput& input);

    [[nodiscard]] static gnc::foundation::NumericalOutcome<
        TdctFormulaOutput>
    evaluate_tdct(const PreparedCavhFormulaModel& model,
                  const TdctFormulaInput& input);

    [[nodiscard]] static gnc::foundation::NumericalOutcome<CavhFormulaOutput>
    evaluate(const PreparedCavhFormulaModel& model,
             const CavhFormulaInput& input);
};

} // namespace gnc::packages::cavh

#include "../include/cavh/formula.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace gnc::packages::cavh {
namespace {

using gnc::contracts::DataQuality;
using gnc::contracts::SampleContext;
using gnc::foundation::NumericalEvidence;
using gnc::foundation::NumericalFlag;
using gnc::foundation::NumericalFlags;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalStatus;

struct ValidationFailure {
    NumericalStatus status = NumericalStatus::DomainError;
    std::string_view detail;
};

[[nodiscard]] NumericalEvidence product_evidence(
    gnc::foundation::AlgorithmIdentity algorithm, std::string_view detail,
    NumericalFlags flags = 0U, std::size_t evaluations = 0U) {
    NumericalEvidence evidence;
    evidence.algorithm = algorithm;
    evidence.detail = detail;
    evidence.flags = flags;
    evidence.evaluations = evaluations;
    return evidence;
}

template <typename Value>
[[nodiscard]] NumericalOutcome<Value> product_failure(
    gnc::foundation::AlgorithmIdentity algorithm, NumericalStatus status,
    std::string_view detail, NumericalFlags flags = 0U) {
    return NumericalOutcome<Value>::failure(
        status, product_evidence(algorithm, detail, flags));
}

[[nodiscard]] bool finite(std::initializer_list<double> values) noexcept {
    return std::all_of(values.begin(), values.end(), [](double value) {
        return std::isfinite(value);
    });
}

[[nodiscard]] bool valid_equation(
    GammaReferenceEquation equation) noexcept {
    switch (equation) {
    case GammaReferenceEquation::Eq17MachDependent:
    case GammaReferenceEquation::Eq18MachIndependent:
        return true;
    }
    return false;
}

[[nodiscard]] std::optional<ValidationFailure> validate_context(
    const CavhFormulaDefinition& definition,
    const SampleContext& context) noexcept {
    if (context.frame != definition.navigation_frame ||
        context.clock_domain != definition.clock_domain) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "sample-frame-or-clock"};
    }
    if (context.configuration_revision !=
            definition.configuration_revision ||
        context.quality != DataQuality::Valid) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "sample-revision-or-quality"};
    }
    if (!std::isfinite(context.sample_time.seconds)) {
        return ValidationFailure{NumericalStatus::NonFiniteInput,
                                 "sample-time"};
    }
    if (context.sample_time.tick < 0) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "sample-tick"};
    }
    return std::nullopt;
}

[[nodiscard]] NumericalOutcome<GlideEnvelopeQueryEvaluation>
solve_envelope(const GlideEnvelopeDefinition& definition,
               double mach) {
    const auto& polar = definition.polar;
    if (!std::isfinite(mach)) {
        return product_failure<GlideEnvelopeQueryEvaluation>(
            kGlideEnvelopeQueryIdentity,
            NumericalStatus::NonFiniteInput, "envelope-mach");
    }
    const double cd0 =
        polar.cd0_base + polar.cd0_slope_per_mach * mach;
    if (!std::isfinite(cd0)) {
        return product_failure<GlideEnvelopeQueryEvaluation>(
            kGlideEnvelopeQueryIdentity,
            NumericalStatus::NonFiniteIntermediate, "envelope-cd0");
    }
    if (cd0 <= 0.0) {
        return product_failure<GlideEnvelopeQueryEvaluation>(
            kGlideEnvelopeQueryIdentity, NumericalStatus::DomainError,
            "envelope-domain");
    }

    const double cl_star =
        std::sqrt(cd0 / polar.induced_drag_factor);
    const double alpha_star =
        (cl_star - polar.cl_intercept) /
        polar.cl_slope_per_radian;
    if (!finite({cl_star, alpha_star})) {
        return product_failure<GlideEnvelopeQueryEvaluation>(
            kGlideEnvelopeQueryIdentity,
            NumericalStatus::NonFiniteIntermediate,
            "envelope-optimum");
    }
    if (alpha_star < polar.alpha_min_radians ||
        alpha_star > polar.alpha_max_radians) {
        return product_failure<GlideEnvelopeQueryEvaluation>(
            kGlideEnvelopeQueryIdentity, NumericalStatus::OutOfRange,
            "envelope-alpha-domain");
    }

    const double cd_star =
        cd0 + polar.induced_drag_factor * cl_star * cl_star;
    const double lift_to_drag = cl_star / cd_star;
    const double derivative = polar.cd0_slope_per_mach /
                              (2.0 * std::sqrt(
                                  polar.induced_drag_factor * cd0));
    if (!finite({cd_star, lift_to_drag, derivative})) {
        return product_failure<GlideEnvelopeQueryEvaluation>(
            kGlideEnvelopeQueryIdentity,
            NumericalStatus::NonFiniteOutput, "envelope-output");
    }

    return NumericalOutcome<GlideEnvelopeQueryEvaluation>::with_value(
        NumericalStatus::Success,
        GlideEnvelopeQueryEvaluation{
            GlideEnvelopeQueryOutput{cd0, cl_star, cd_star,
                                     lift_to_drag, alpha_star,
                                     derivative},
            GlideEnvelopeQueryTelemetry{}},
        product_evidence(kGlideEnvelopeQueryIdentity, "envelope", 0U,
                         1U));
}

[[nodiscard]] std::optional<ValidationFailure>
validate_operating_point_common(
    const CavhOperatingPointInput& point) noexcept {
    if (!finite({
            point.density_kilograms_per_cubic_meter,
            point.density_altitude_gradient_kilograms_per_cubic_meter_per_meter,
            point.altitude_meters,
            point.reference_radius_meters,
            point.speed_meters_per_second,
            point.gravity_meters_per_second_squared,
            point.mass_kilograms,
            point.reference_area_square_meters,
            point.mach,
            point.bank_angle_radians,
        })) {
        return ValidationFailure{NumericalStatus::NonFiniteInput,
                                 "formula-input"};
    }
    const double radius =
        point.reference_radius_meters + point.altitude_meters;
    if (!std::isfinite(radius)) {
        return ValidationFailure{NumericalStatus::NonFiniteIntermediate,
                                 "formula-radius"};
    }
    if (point.density_kilograms_per_cubic_meter <= 0.0 ||
        point.speed_meters_per_second <= 0.0 ||
        point.gravity_meters_per_second_squared <= 0.0 ||
        point.mass_kilograms <= 0.0 ||
        point.reference_area_square_meters <= 0.0 || radius <= 0.0) {
        return ValidationFailure{NumericalStatus::DomainError,
                                 "formula-domain"};
    }
    return std::nullopt;
}

[[nodiscard]] bool small_denominator(double value,
                                     double minimum) noexcept {
    return !std::isfinite(value) || std::abs(value) <= minimum;
}

[[nodiscard]] bool finite(const GammaReferenceIntermediates& value) noexcept {
    return finite({
               value.density_kilograms_per_cubic_meter,
               value.density_altitude_gradient_kilograms_per_cubic_meter_per_meter,
               value.radius_meters,
               value.dynamic_pressure_pascals,
               value.drag_force_newtons,
               value.cl_vertical,
               value.dcl_vertical_dmach,
               value.mach_speed_partial_seconds_per_meter,
               value.mach_altitude_partial_per_meter,
               value.dcl_vertical_dspeed_seconds_per_meter,
               value.b2,
               value.b3,
           }) &&
           (!value.b1.has_value() || std::isfinite(*value.b1));
}

} // namespace

GlideEnvelopePreparedModel::GlideEnvelopePreparedModel(
    std::shared_ptr<const GlideEnvelopeDefinition> definition,
    gnc::model_sdk::PreparedModelMetadata metadata) noexcept
    : definition_(std::move(definition)), metadata_(std::move(metadata)) {}

const GlideEnvelopeDefinition& GlideEnvelopePreparedModel::definition()
    const noexcept {
    return *definition_;
}

const gnc::model_sdk::PreparedModelMetadata&
GlideEnvelopePreparedModel::metadata() const noexcept {
    return metadata_;
}

NumericalOutcome<GlideEnvelopePreparedModel>
prepare_glide_envelope_model(GlideEnvelopeDefinition definition) {
    const auto failure = [](NumericalStatus status,
                            std::string_view detail) {
        return NumericalOutcome<GlideEnvelopePreparedModel>::failure(
            status,
            product_evidence(kGlideEnvelopePreparationIdentity, detail));
    };

    auto metadata = gnc::model_sdk::prepare_model_metadata(
        definition.metadata, kGlideEnvelopePreparationIdentity);
    if (!metadata.has_value()) {
        return NumericalOutcome<GlideEnvelopePreparedModel>::failure(
            metadata.status(), metadata.evidence());
    }
    if (definition.metadata.model_id != kGlideEnvelopeModelIdentity ||
        definition.metadata.execution_form !=
            gnc::model_sdk::ModelExecutionForm::PureQuery) {
        return failure(NumericalStatus::DomainError,
                       "definition-identity");
    }

    const auto& polar = definition.polar;
    if (!finite({polar.cl_intercept,
                 polar.cl_slope_per_radian,
                 polar.cd0_base,
                 polar.cd0_slope_per_mach,
                 polar.induced_drag_factor,
                 polar.alpha_min_radians,
                 polar.alpha_max_radians}) ||
        polar.cl_slope_per_radian <= 0.0 ||
        polar.induced_drag_factor <= 0.0 ||
        polar.alpha_max_radians <= polar.alpha_min_radians) {
        return failure(NumericalStatus::DomainError,
                       "envelope-definition");
    }

    return NumericalOutcome<GlideEnvelopePreparedModel>::with_value(
        NumericalStatus::Success,
        GlideEnvelopePreparedModel{
            std::make_shared<const GlideEnvelopeDefinition>(
                std::move(definition)),
            std::move(metadata.value())},
        product_evidence(kGlideEnvelopePreparationIdentity, "prepared", 0U,
                         1U));
}

NumericalOutcome<GlideEnvelopeQueryEvaluation>
GlideEnvelopeQueryKernel::evaluate(
    const GlideEnvelopePreparedModel& model,
    const GlideEnvelopeQueryInput& input) {
    return solve_envelope(model.definition(), input.mach);
}

PreparedCavhFormulaModel::PreparedCavhFormulaModel(
    std::shared_ptr<const CavhFormulaDefinition> definition,
    GlideEnvelopePreparedModel glide_envelope_model) noexcept
    : definition_(std::move(definition)),
      glide_envelope_model_(std::move(glide_envelope_model)) {}

const CavhFormulaDefinition& PreparedCavhFormulaModel::definition()
    const noexcept {
    return *definition_;
}

const GlideEnvelopePreparedModel&
PreparedCavhFormulaModel::glide_envelope_model() const noexcept {
    return glide_envelope_model_;
}

NumericalOutcome<PreparedCavhFormulaModel> prepare_cavh_formula_model(
    CavhFormulaDefinition definition) {
    const auto failure = [](NumericalStatus status,
                            std::string_view detail) {
        return NumericalOutcome<PreparedCavhFormulaModel>::failure(
            status,
            product_evidence(kCavhFormulaPreparationIdentity, detail));
    };

    auto envelope = prepare_glide_envelope_model(definition.envelope);
    if (!envelope.has_value()) {
        return NumericalOutcome<PreparedCavhFormulaModel>::failure(
            envelope.status(), envelope.evidence());
    }

    if (definition.navigation_frame.id.empty() ||
        definition.clock_domain.id.empty() ||
        definition.configuration_revision < 0) {
        return failure(NumericalStatus::DomainError,
                       "definition-context-policy");
    }
    if (!valid_equation(definition.algorithm.equation) ||
        !gnc::foundation::valid_numerical_policy(
            definition.algorithm.numerical_policy) ||
        !finite({definition.algorithm.denominator_minimum_absolute,
                 definition.algorithm
                     .derivative_minimum_absolute_seconds_per_meter}) ||
        definition.algorithm.denominator_minimum_absolute <= 0.0 ||
        definition.algorithm
                .derivative_minimum_absolute_seconds_per_meter <=
            0.0) {
        return failure(NumericalStatus::DomainError,
                       "definition-algorithm");
    }

    const auto& tdct = definition.tdct;
    if (!finite({tdct.gain, tdct.alpha_min_radians,
                 tdct.alpha_max_radians}) ||
        tdct.gain < 0.0 ||
        tdct.alpha_max_radians <= tdct.alpha_min_radians) {
        return failure(NumericalStatus::DomainError,
                       "tdct-definition");
    }

    return NumericalOutcome<PreparedCavhFormulaModel>::with_value(
        NumericalStatus::Success,
        PreparedCavhFormulaModel{
            std::make_shared<const CavhFormulaDefinition>(
                std::move(definition)),
            std::move(envelope.value())},
        product_evidence(kCavhFormulaPreparationIdentity, "prepared", 0U,
                         1U));
}

NumericalOutcome<GammaReferenceEvaluation>
CavhFormulaKernel::evaluate_gamma_reference(
    const PreparedCavhFormulaModel& model,
    const GammaReferenceInput& input) {
    const auto& definition = model.definition();
    const auto& formula = input.formula;
    const auto& envelope = input.envelope;
    if (const auto failure = validate_context(definition, formula.context)) {
        return product_failure<GammaReferenceEvaluation>(
            kCavhGammaReferenceIdentity, failure->status,
            failure->detail);
    }
    if (const auto failure =
            validate_operating_point_common(formula.operating_point)) {
        return product_failure<GammaReferenceEvaluation>(
            kCavhGammaReferenceIdentity, failure->status,
            failure->detail);
    }
    if (!finite({envelope.cd0, envelope.cl_star, envelope.cd_star,
                 envelope.lift_to_drag_maximum,
                 envelope.alpha_star_radians,
                 envelope.dcl_star_dmach})) {
        return product_failure<GammaReferenceEvaluation>(
            kCavhGammaReferenceIdentity, NumericalStatus::NonFiniteInput,
            "envelope-query-output");
    }
    const auto& point = formula.operating_point;
    const double bank_cosine = std::cos(point.bank_angle_radians);
    const double cl_vertical = envelope.cl_star * bank_cosine;
    const double dcl_vertical_dmach =
        envelope.dcl_star_dmach * bank_cosine;
    if (!finite({bank_cosine, cl_vertical, dcl_vertical_dmach})) {
        return product_failure<GammaReferenceEvaluation>(
            kCavhGammaReferenceIdentity,
            NumericalStatus::NonFiniteIntermediate,
            "formula-bank-projection");
    }
    if (cl_vertical <= 0.0 || envelope.cd_star <= 0.0) {
        return product_failure<GammaReferenceEvaluation>(
            kCavhGammaReferenceIdentity, NumericalStatus::DomainError,
            "formula-domain");
    }

    GammaReferenceIntermediates values;
    values.density_kilograms_per_cubic_meter =
        point.density_kilograms_per_cubic_meter;
    values.density_altitude_gradient_kilograms_per_cubic_meter_per_meter =
        point.density_altitude_gradient_kilograms_per_cubic_meter_per_meter;
    values.radius_meters =
        point.reference_radius_meters + point.altitude_meters;
    values.dynamic_pressure_pascals =
        0.5 * point.density_kilograms_per_cubic_meter *
        point.speed_meters_per_second * point.speed_meters_per_second;
    values.drag_force_newtons =
        values.dynamic_pressure_pascals *
        point.reference_area_square_meters * envelope.cd_star;
    values.cl_vertical = cl_vertical;

    const double rho = point.density_kilograms_per_cubic_meter;
    const double rho_h =
        point.density_altitude_gradient_kilograms_per_cubic_meter_per_meter;
    const double speed = point.speed_meters_per_second;
    const double gravity = point.gravity_meters_per_second_squared;
    const double mass = point.mass_kilograms;
    const double area = point.reference_area_square_meters;
    const double radius = values.radius_meters;
    const double threshold =
        definition.algorithm.denominator_minimum_absolute;
    double gamma_reference = 0.0;

    if (definition.algorithm.equation ==
        GammaReferenceEquation::Eq18MachIndependent) {
        const double a21 = rho_h * speed * speed /
                           (2.0 * rho * gravity);
        const double a24 = 2.0 * mass /
                           (cl_vertical * rho * area * radius);
        const double a25 = mass * speed * speed /
                           (cl_vertical * rho * gravity * area * radius *
                            radius);
        const double a31 = rho_h * cl_vertical * speed * speed * area *
                           radius / (4.0 * mass * gravity);
        const double a34 = 1.0 / a24;
        const double a35 = speed * speed /
                           (2.0 * gravity * radius);
        values.b2 = 1.0 - a21 + a24 + a25;
        values.b3 = 1.0 - a31 + a34 + a35;
        if (small_denominator(values.b2, threshold) ||
            small_denominator(values.b3, threshold)) {
            return product_failure<GammaReferenceEvaluation>(
                kCavhGammaReferenceIdentity,
                NumericalStatus::Singular, "formula-denominator");
        }
        gamma_reference =
            -values.drag_force_newtons / (mass * gravity) *
            (1.0 / values.b2 + 1.0 / values.b3);
    } else {
        if (!finite({point.mach_speed_partial_seconds_per_meter,
                     point.mach_altitude_partial_per_meter})) {
            return product_failure<GammaReferenceEvaluation>(
                kCavhGammaReferenceIdentity,
                NumericalStatus::NonFiniteInput,
                "eq17-mach-partials");
        }
        if (point.mach_speed_partial_seconds_per_meter <= 0.0) {
            return product_failure<GammaReferenceEvaluation>(
                kCavhGammaReferenceIdentity,
                NumericalStatus::DomainError,
                "eq17-mach-speed-partial");
        }
        values.dcl_vertical_dmach = dcl_vertical_dmach;
        values.mach_speed_partial_seconds_per_meter =
            point.mach_speed_partial_seconds_per_meter;
        values.mach_altitude_partial_per_meter =
            point.mach_altitude_partial_per_meter;
        values.dcl_vertical_dspeed_seconds_per_meter =
            dcl_vertical_dmach *
            point.mach_speed_partial_seconds_per_meter;
        if (std::abs(values.dcl_vertical_dspeed_seconds_per_meter) <=
            definition.algorithm
                .derivative_minimum_absolute_seconds_per_meter) {
            return product_failure<GammaReferenceEvaluation>(
                kCavhGammaReferenceIdentity,
                NumericalStatus::IllConditioned,
                "eq17-derivative-degenerate-fallback-forbidden");
        }

        const double dcl_dspeed =
            values.dcl_vertical_dspeed_seconds_per_meter;
        const double mach_v =
            point.mach_speed_partial_seconds_per_meter;
        const double mach_h = point.mach_altitude_partial_per_meter;
        const double a11 = rho_h * cl_vertical * speed /
                           (dcl_dspeed * rho * gravity);
        const double a12 = mach_h * speed / (mach_v * gravity);
        const double a13 =
            2.0 * cl_vertical / (dcl_dspeed * speed);
        const double a14 = 4.0 * mass /
                           (dcl_dspeed * rho * speed * area * radius);
        const double a15 = 2.0 * speed * mass /
                           (dcl_dspeed * rho * area * gravity * radius *
                            radius);
        const double a21 = rho_h * speed * speed /
                           (2.0 * rho * gravity);
        const double a22 = dcl_vertical_dmach * mach_h * speed * speed /
                           (2.0 * cl_vertical * gravity);
        const double a23 = 1.0 / a13;
        const double a24 = 2.0 * mass /
                           (cl_vertical * rho * area * radius);
        const double a25 = mass * speed * speed /
                           (cl_vertical * rho * gravity * area * radius *
                            radius);
        const double a31 = rho_h * cl_vertical * speed * speed * area *
                           radius / (4.0 * mass * gravity);
        const double a32 = dcl_vertical_dmach * mach_h * rho * speed *
                           speed * area * radius /
                           (4.0 * mass * gravity);
        const double a33 = 1.0 / a14;
        const double a34 = 1.0 / a24;
        const double a35 = speed * speed /
                           (2.0 * gravity * radius);
        values.b1 = 1.0 - a11 - a12 + a13 + a14 + a15;
        values.b2 = 1.0 - a21 - a22 + a23 + a24 + a25;
        values.b3 = 1.0 - a31 - a32 + a33 + a34 + a35;
        if (small_denominator(*values.b1, threshold) ||
            small_denominator(values.b2, threshold) ||
            small_denominator(values.b3, threshold)) {
            return product_failure<GammaReferenceEvaluation>(
                kCavhGammaReferenceIdentity,
                NumericalStatus::Singular, "formula-denominator");
        }
        gamma_reference =
            -values.drag_force_newtons / (mass * gravity) *
            (1.0 / *values.b1 + 1.0 / values.b2 +
             1.0 / values.b3);
    }

    if (!finite(values) || !std::isfinite(gamma_reference)) {
        return product_failure<GammaReferenceEvaluation>(
            kCavhGammaReferenceIdentity,
            NumericalStatus::NonFiniteOutput, "formula-output");
    }

    return NumericalOutcome<GammaReferenceEvaluation>::with_value(
        NumericalStatus::Success,
        GammaReferenceEvaluation{
            GammaReferenceOutput{formula.context,
                                 definition.algorithm.equation,
                                 envelope.alpha_star_radians,
                                 gamma_reference},
            GammaReferenceTelemetry{envelope, values}},
        product_evidence(
            kCavhGammaReferenceIdentity,
            definition.algorithm.equation ==
                    GammaReferenceEquation::Eq17MachDependent
                ? "eq17"
                : "eq18",
            0U, 1U));
}

NumericalOutcome<TdctFormulaEvaluation> CavhFormulaKernel::evaluate_tdct(
    const PreparedCavhFormulaModel& model,
    const TdctFormulaInput& input) {
    const auto& definition = model.definition();
    if (const auto failure = validate_context(definition, input.context)) {
        return product_failure<TdctFormulaEvaluation>(
            kCavhTdctIdentity, failure->status, failure->detail);
    }
    if (!finite({input.alpha_star_radians,
                 input.gamma_reference_radians,
                 input.gamma_measured_radians})) {
        return product_failure<TdctFormulaEvaluation>(
            kCavhTdctIdentity, NumericalStatus::NonFiniteInput,
            "tdct-input");
    }

    const double error = input.gamma_reference_radians -
                         input.gamma_measured_radians;
    const double correction = definition.tdct.gain * error;
    const double raw = input.alpha_star_radians + correction;
    const double limited = std::clamp(
        raw, definition.tdct.alpha_min_radians,
        definition.tdct.alpha_max_radians);
    if (!finite({error, correction, raw, limited})) {
        return product_failure<TdctFormulaEvaluation>(
            kCavhTdctIdentity, NumericalStatus::NonFiniteOutput,
            "tdct-output");
    }

    const TdctSaturation saturation =
        raw < definition.tdct.alpha_min_radians
            ? TdctSaturation::Lower
            : raw > definition.tdct.alpha_max_radians
                  ? TdctSaturation::Upper
                  : TdctSaturation::None;
    const NumericalFlags flags =
        saturation == TdctSaturation::None
            ? 0U
            : gnc::foundation::numerical_flag(NumericalFlag::Clamped);
    return NumericalOutcome<TdctFormulaEvaluation>::with_value(
        NumericalStatus::Success,
        TdctFormulaEvaluation{
            TdctFormulaOutput{input.context, limited},
            TdctFormulaTelemetry{error, correction, raw, saturation}},
        product_evidence(kCavhTdctIdentity, "tdct", flags, 1U));
}

NumericalOutcome<CavhFormulaEvaluation> CavhFormulaKernel::evaluate(
    const PreparedCavhFormulaModel& model,
    const CavhFormulaInput& input) {
    const auto envelope = GlideEnvelopeQueryKernel::evaluate(
        model.glide_envelope_model(),
        GlideEnvelopeQueryInput{input.operating_point.mach});
    if (!envelope.has_value()) {
        return NumericalOutcome<CavhFormulaEvaluation>::failure(
            envelope.status(), envelope.evidence());
    }
    const auto reference = evaluate_gamma_reference(
        model, GammaReferenceInput{input, envelope.value().output});
    if (!reference.has_value()) {
        return NumericalOutcome<CavhFormulaEvaluation>::failure(
            reference.status(), reference.evidence());
    }
    const auto tdct = evaluate_tdct(
        model,
        TdctFormulaInput{input.context,
                         reference.value().output.alpha_star_radians,
                         reference.value().output.gamma_reference_radians,
                         input.gamma_measured_radians});
    if (!tdct.has_value()) {
        return NumericalOutcome<CavhFormulaEvaluation>::failure(
            tdct.status(), tdct.evidence());
    }

    const NumericalFlags flags =
        reference.evidence().flags | tdct.evidence().flags;
    return NumericalOutcome<CavhFormulaEvaluation>::with_value(
        NumericalStatus::Success,
        CavhFormulaEvaluation{
            CavhFormulaOutput{reference.value().output,
                              tdct.value().output},
            CavhFormulaTelemetry{reference.value().telemetry,
                                 tdct.value().telemetry}},
        product_evidence(kCavhFormulaKernelIdentity, "formula-tdct",
                          flags,
                          envelope.evidence().evaluations +
                              reference.evidence().evaluations +
                              tdct.evidence().evaluations));
}

} // namespace gnc::packages::cavh

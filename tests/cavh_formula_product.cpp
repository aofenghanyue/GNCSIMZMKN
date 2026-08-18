#include <cavh/formula.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using gnc::contracts::ClockDomainIdentity;
using gnc::contracts::DataQuality;
using gnc::contracts::FrameIdentity;
using gnc::contracts::SampleContext;
using gnc::contracts::SimulationInstant;
using gnc::foundation::FiniteCheck;
using gnc::foundation::NumericalFlag;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;
using namespace gnc::packages::cavh;

constexpr std::string_view kFixtureId = "REF-CAVH-FORMULA-001";
constexpr std::string_view kOracleId = "ORACLE-CAVH-FORMULA-001";
constexpr std::string_view kReferenceModelId =
    "MODEL-CAVH-LEGACY-TRANSCRIBED-FORMULA-001";
constexpr std::string_view kNavigationFrame =
    "frame.fixture.cavh.navigation@1";
constexpr std::string_view kClock =
    "clock.fixture.cavh.simulation@1";
constexpr double kAbsolute = 2.0e-11;
constexpr double kRelative = 2.0e-11;
constexpr double kPi =
    3.141592653589793238462643383279502884;

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

template <typename Value>
const Value& require_value(const NumericalOutcome<Value>& outcome,
                           std::string_view message) {
    require(outcome.has_value(), message);
    return outcome.value();
}

template <typename Value>
void expect_failure(const NumericalOutcome<Value>& outcome,
                    NumericalStatus status, std::string_view detail,
                    std::string_view message) {
    require(!outcome.has_value() && outcome.status() == status &&
                outcome.evidence().detail == detail,
            message);
}

NumericalPolicy fixture_policy() {
    NumericalPolicy policy;
    policy.absolute_tolerance = kAbsolute;
    policy.relative_tolerance = kRelative;
    policy.finite_check = FiniteCheck::EveryStage;
    policy.zero_tolerance = 1.0e-14;
    policy.condition_limit = 1.0e12;
    return policy;
}

SampleContext fixture_context() {
    return {
        FrameIdentity{std::string(kNavigationFrame)},
        ClockDomainIdentity{std::string(kClock)},
        SimulationInstant{42, 30.0},
        4,
        DataQuality::Valid,
    };
}

ParabolicEnvelopeDefinition constant_polar() {
    return {0.0, 2.0, 0.02, 0.0, 0.08, 0.0, 0.5};
}

ParabolicEnvelopeDefinition mach_polar() {
    return {0.0, 2.0, 0.02, 0.001, 0.08, 0.0, 0.5};
}

GlideEnvelopeDefinition envelope_definition(
    ParabolicEnvelopeDefinition polar) {
    return {
        {std::string(kGlideEnvelopeModelIdentity), "0.1.0",
         gnc::model_sdk::ModelExecutionForm::PureQuery},
        polar,
    };
}

CavhFormulaDefinition fixture_definition(
    GammaReferenceEquation equation,
    ParabolicEnvelopeDefinition envelope = constant_polar(),
    double tdct_gain = 3.0) {
    CavhFormulaDefinition definition;
    definition.navigation_frame =
        FrameIdentity{std::string(kNavigationFrame)};
    definition.clock_domain = ClockDomainIdentity{std::string(kClock)};
    definition.configuration_revision = 4;
    definition.envelope = envelope_definition(envelope);
    definition.algorithm.equation = equation;
    definition.algorithm.denominator_minimum_absolute = 1.0e-12;
    definition.algorithm
        .derivative_minimum_absolute_seconds_per_meter = 1.0e-12;
    definition.algorithm.numerical_policy = fixture_policy();
    definition.tdct = {tdct_gain, 0.1, 0.4};
    return definition;
}

CavhOperatingPointInput fixture_operating_point(double bank_radians) {
    constexpr double altitude = 30000.0;
    constexpr double rho0 = 1.225;
    constexpr double scale_height = 7200.0;
    constexpr double speed = 3000.0;
    constexpr double sound = 300.0;
    constexpr double sound_gradient = -0.00075;
    const double density = rho0 * std::exp(-altitude / scale_height);

    CavhOperatingPointInput point;
    point.density_kilograms_per_cubic_meter = density;
    point.density_altitude_gradient_kilograms_per_cubic_meter_per_meter =
        -density / scale_height;
    point.altitude_meters = altitude;
    point.reference_radius_meters = 6371000.0;
    point.speed_meters_per_second = speed;
    point.gravity_meters_per_second_squared = 9.81;
    point.mass_kilograms = 50000.0;
    point.reference_area_square_meters = 100.0;
    point.mach = speed / sound;
    point.mach_speed_partial_seconds_per_meter = 1.0 / sound;
    point.mach_altitude_partial_per_meter =
        -speed * sound_gradient / (sound * sound);
    point.bank_angle_radians = bank_radians;
    return point;
}

CavhFormulaInput fixture_input(double bank_radians,
                               double gamma_measured = -0.03) {
    return {fixture_context(), fixture_operating_point(bank_radians),
            gamma_measured};
}

PreparedCavhFormulaModel prepared(CavhFormulaDefinition definition) {
    const auto outcome =
        prepare_cavh_formula_model(std::move(definition));
    require(outcome.has_value(), "CAVH product definition did not prepare");
    return outcome.value();
}

NumericalOutcome<GammaReferenceEvaluation> evaluate_reference(
    const PreparedCavhFormulaModel& model,
    const CavhFormulaInput& input) {
    const auto envelope = GlideEnvelopeQueryKernel::evaluate(
        model.glide_envelope_model(),
        GlideEnvelopeQueryInput{input.operating_point.mach});
    if (!envelope.has_value()) {
        return NumericalOutcome<GammaReferenceEvaluation>::failure(
            envelope.status(), envelope.evidence());
    }
    return CavhFormulaKernel::evaluate_gamma_reference(
        model, GammaReferenceInput{input, envelope.value().output});
}

template <typename Value, typename = void>
struct has_intermediates_member : std::false_type {};

template <typename Value>
struct has_intermediates_member<
    Value, std::void_t<decltype(std::declval<Value>().intermediates)>>
    : std::true_type {};

template <typename Value, typename = void>
struct has_saturation_member : std::false_type {};

template <typename Value>
struct has_saturation_member<
    Value, std::void_t<decltype(std::declval<Value>().saturation)>>
    : std::true_type {};

static_assert(!has_intermediates_member<GammaReferenceOutput>::value,
              "formula telemetry must stay outside GammaReferenceOutput");
static_assert(!has_saturation_member<TdctFormulaOutput>::value,
              "TDCT telemetry must stay outside TdctFormulaOutput");

struct EquationCase {
    std::string id;
    GammaReferenceEvaluation evaluation;
};

struct TdctCase {
    std::string id;
    TdctFormulaEvaluation evaluation;
};

struct ProbeBundle {
    gnc::model_sdk::PreparedModelMetadata metadata;
    ClockDomainIdentity clock_domain;
    std::int64_t configuration_revision = -1;
    CavhEnvelopeOutput constant_envelope;
    CavhEnvelopeOutput mach_envelope;
    std::vector<EquationCase> equations;
    std::vector<TdctCase> tdct;
    CavhFormulaEvaluation typed_consumer;
    double typed_consumer_gamma_measured = 0.0;
    double typed_consumer_gain = 0.0;
    std::vector<std::string> direct_checks;
};

ProbeBundle run_probe() {
    std::vector<std::string> checks;
    require(kCavhFormulaModelIdentity != kReferenceModelId &&
                kGlideEnvelopeModelIdentity != kReferenceModelId,
            "CAVH product identity aliases the R0 reference identity");
    checks.emplace_back("product-reference-identity");

    const auto eq18_model = prepared(fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent));
    const auto& prepared_metadata =
        eq18_model.glide_envelope_model().metadata();
    require(prepared_metadata.definition.model_id ==
                kGlideEnvelopeModelIdentity &&
                prepared_metadata.definition.model_version == "0.1.0" &&
                prepared_metadata.definition.execution_form ==
                    gnc::model_sdk::ModelExecutionForm::PureQuery &&
                prepared_metadata.preparation_algorithm_id ==
                    kGlideEnvelopePreparationIdentity.id &&
                prepared_metadata.preparation_algorithm_version ==
                    kGlideEnvelopePreparationIdentity.version &&
                eq18_model.definition().clock_domain.id == kClock &&
                eq18_model.definition().configuration_revision == 4,
            "CAVH prepared-model metadata differs");
    checks.emplace_back("prepared-model-metadata");
    const CavhFormulaInput eq18_unbanked_input = fixture_input(0.0);
    const auto envelope_query = GlideEnvelopeQueryKernel::evaluate(
        eq18_model.glide_envelope_model(),
        GlideEnvelopeQueryInput{
            eq18_unbanked_input.operating_point.mach});
    const auto& envelope_query_value = require_value(
        envelope_query, "glide-envelope pure query failed");
    const auto eq18_unbanked =
        CavhFormulaKernel::evaluate_gamma_reference(
            eq18_model,
            GammaReferenceInput{eq18_unbanked_input,
                                envelope_query_value.output});
    const auto& eq18_unbanked_value = require_value(
        eq18_unbanked, "Eq18 unbanked product evaluation failed");
    require(near(eq18_unbanked_value.telemetry.envelope.cd0, 0.02) &&
                near(eq18_unbanked_value.telemetry.envelope.cl_star, 0.5) &&
                near(eq18_unbanked_value.telemetry.envelope.cd_star, 0.04) &&
                near(eq18_unbanked_value.telemetry.envelope
                         .lift_to_drag_maximum,
                     12.5) &&
                near(eq18_unbanked_value.output.alpha_star_radians,
                     0.25) &&
                near(eq18_unbanked_value.output.alpha_star_radians,
                     eq18_unbanked_value.telemetry.envelope
                         .alpha_star_radians) &&
                near(eq18_unbanked_value.telemetry.envelope.dcl_star_dmach,
                     0.0),
            "constant-polar envelope differs from the accepted oracle");
    checks.emplace_back("formal-output-telemetry-separation");
    checks.emplace_back("formula-consumes-glide-envelope-query-output");
    checks.emplace_back("envelope-accepted");

    const auto repeated_eq18 =
        evaluate_reference(
            eq18_model, fixture_input(0.0));
    require(repeated_eq18.has_value() &&
                near(repeated_eq18.value().output.gamma_reference_radians,
                     eq18_unbanked_value.output.gamma_reference_radians),
            "CAVH prepared model evaluation is not deterministic");
    checks.emplace_back("deterministic-independent-evaluation");

    const auto eq18_banked = evaluate_reference(
        eq18_model, fixture_input(kPi / 3.0));
    const auto& eq18_banked_value = require_value(
        eq18_banked, "Eq18 banked product evaluation failed");
    require(near(eq18_unbanked_value.output.gamma_reference_radians,
                 -0.0109446746632963695) &&
                near(eq18_banked_value.output.gamma_reference_radians,
                     -0.0111187885840221742) &&
                near(eq18_unbanked_value.telemetry.intermediates
                         .cl_vertical,
                     0.5) &&
                near(eq18_banked_value.telemetry.intermediates.cl_vertical,
                     0.25) &&
                !eq18_unbanked_value.telemetry.intermediates.b1.has_value(),
            "Eq18 product results differ from the accepted oracle");
    checks.emplace_back("eq18-accepted");

    CavhFormulaInput eq18_without_derivatives = fixture_input(0.0);
    eq18_without_derivatives.operating_point
        .mach_speed_partial_seconds_per_meter =
        std::numeric_limits<double>::quiet_NaN();
    eq18_without_derivatives.operating_point
        .mach_altitude_partial_per_meter =
        std::numeric_limits<double>::quiet_NaN();
    const auto eq18_immutable =
        evaluate_reference(
            eq18_model, eq18_without_derivatives);
    require(eq18_immutable.has_value() &&
                near(eq18_immutable.value().output.gamma_reference_radians,
                     eq18_unbanked_value.output.gamma_reference_radians),
            "Eq18 consumed derivative inputs excluded by its identity");
    checks.emplace_back("eq18-ignores-unused-derivatives");

    const auto eq17_model = prepared(fixture_definition(
        GammaReferenceEquation::Eq17MachDependent, mach_polar()));
    CavhFormulaInput eq17_input = fixture_input(kPi / 6.0);
    const auto eq17 = evaluate_reference(
        eq17_model, eq17_input);
    const auto& eq17_value = require_value(
        eq17, "Eq17 product evaluation failed");
    require(near(eq17_value.telemetry.envelope.cl_star,
                 0.61237243569579452455) &&
                near(eq17_value.telemetry.envelope.dcl_star_dmach,
                     0.010206207261596575409) &&
                near(eq17_value.telemetry.intermediates.cl_vertical,
                     0.53033008588991064330) &&
                eq17_value.telemetry.intermediates.b1.has_value() &&
                near(eq17_value.output.gamma_reference_radians,
                     -0.017777640903762576252),
            "Eq17 product results differ from the accepted oracle");
    checks.emplace_back("eq17-accepted");

    const std::vector<std::pair<std::string, TdctFormulaInput>>
        tdct_inputs{
            {"CASE-CAVH-TDCT-UNSATURATED",
             {fixture_context(), 0.25, -0.01, -0.03}},
            {"CASE-CAVH-TDCT-UPPER-SATURATION",
             {fixture_context(), 0.25, -0.01, -0.2}},
            {"CASE-CAVH-TDCT-LOWER-SATURATION",
             {fixture_context(), 0.25, -0.01, 0.1}},
        };
    std::vector<TdctCase> tdct_cases;
    for (const auto& [id, input] : tdct_inputs) {
        const auto outcome =
            CavhFormulaKernel::evaluate_tdct(eq18_model, input);
        tdct_cases.push_back({id, require_value(
                                     outcome,
                                     "TDCT product evaluation failed")});
        if (id != "CASE-CAVH-TDCT-UNSATURATED") {
            require(gnc::foundation::has_numerical_flag(
                        outcome.evidence().flags,
                        NumericalFlag::Clamped),
                    "TDCT saturation omitted the clamped flag");
        }
    }
    const auto zero_gain_model = prepared(fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent, constant_polar(),
        0.0));
    const auto zero_gain = CavhFormulaKernel::evaluate_tdct(
        zero_gain_model,
        {fixture_context(), 0.25, -0.01, 0.3});
    tdct_cases.push_back(
        {"CASE-CAVH-TDCT-ZERO-GAIN",
         require_value(zero_gain, "zero-gain TDCT evaluation failed")});
    require(near(tdct_cases[0].evaluation.output.alpha_limited_radians,
                 0.31) &&
                tdct_cases[0].evaluation.telemetry.saturation ==
                    TdctSaturation::None &&
                near(tdct_cases[1].evaluation.output.alpha_limited_radians,
                     0.4) &&
                tdct_cases[1].evaluation.telemetry.saturation ==
                    TdctSaturation::Upper &&
                near(tdct_cases[2].evaluation.output.alpha_limited_radians,
                     0.1) &&
                tdct_cases[2].evaluation.telemetry.saturation ==
                    TdctSaturation::Lower &&
                near(tdct_cases[3].evaluation.output.alpha_limited_radians,
                     0.25) &&
                tdct_cases[3].evaluation.telemetry.saturation ==
                    TdctSaturation::None,
            "TDCT product results differ from the accepted oracle");
    checks.emplace_back("tdct-accepted");
    checks.emplace_back("tdct-clamp-evidence");

    const auto composite =
        CavhFormulaKernel::evaluate(eq17_model, eq17_input);
    const auto& composite_value = require_value(
        composite, "typed formula-to-TDCT evaluation failed");
    const double expected_composite_raw =
        composite_value.output.gamma_reference.alpha_star_radians +
        3.0 *
            (composite_value.output.gamma_reference
                 .gamma_reference_radians -
             eq17_input.gamma_measured_radians);
    require(near(composite_value.telemetry.tdct.alpha_raw_radians,
                 expected_composite_raw) &&
                near(composite_value.output.tdct.alpha_limited_radians,
                     expected_composite_raw) &&
                composite_value.output.tdct.context.sample_time.tick ==
                    eq17_input.context.sample_time.tick &&
                composite.evidence().algorithm.id ==
                    kCavhFormulaKernelIdentity.id,
            "typed gamma output was not consumed by TDCT");
    checks.emplace_back("typed-formula-tdct-consumer");

    CavhFormulaDefinition invalid_definition = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent);
    invalid_definition.envelope.metadata.model_id =
        std::string(kReferenceModelId);
    expect_failure(prepare_cavh_formula_model(invalid_definition),
                   NumericalStatus::DomainError, "definition-identity",
                   "reference model identity prepared as a product model");
    checks.emplace_back("definition-identity-rejection");

    CavhFormulaDefinition invalid_metadata = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent);
    invalid_metadata.envelope.metadata.execution_form =
        gnc::model_sdk::ModelExecutionForm::Unspecified;
    const auto invalid_metadata_outcome =
        prepare_cavh_formula_model(std::move(invalid_metadata));
    require(!invalid_metadata_outcome.has_value() &&
                invalid_metadata_outcome.status() ==
                    NumericalStatus::DomainError &&
                invalid_metadata_outcome.evidence().algorithm.id ==
                    gnc::model_sdk::kModelMetadataPreparationIdentity.id &&
                invalid_metadata_outcome.evidence().detail ==
                    "model-execution-form",
            "CAVH invalid framework model metadata prepared");
    checks.emplace_back("model-metadata-rejection");

    CavhFormulaDefinition invalid_context_policy = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent);
    invalid_context_policy.clock_domain.id.clear();
    expect_failure(prepare_cavh_formula_model(invalid_context_policy),
                   NumericalStatus::DomainError,
                   "definition-context-policy",
                   "empty CAVH context policy prepared");
    checks.emplace_back("package-context-policy-rejection");

    CavhFormulaInput invalid_context = fixture_input(0.0);
    invalid_context.context.frame.id = "frame.other";
    expect_failure(evaluate_reference(
                       eq18_model, invalid_context),
                   NumericalStatus::DomainError,
                   "sample-frame-or-clock",
                   "frame mismatch produced a formula value");
    invalid_context = fixture_input(0.0);
    invalid_context.context.quality = DataQuality::Degraded;
    expect_failure(evaluate_reference(
                       eq18_model, invalid_context),
                   NumericalStatus::DomainError,
                   "sample-revision-or-quality",
                   "degraded sample produced a formula value");
    invalid_context = fixture_input(0.0);
    invalid_context.context.sample_time.seconds =
        std::numeric_limits<double>::quiet_NaN();
    expect_failure(evaluate_reference(
                       eq18_model, invalid_context),
                   NumericalStatus::NonFiniteInput, "sample-time",
                   "non-finite sample time produced a formula value");
    checks.emplace_back("sample-context-rejection");

    CavhFormulaDefinition invalid_envelope = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent);
    invalid_envelope.envelope.polar.cd0_base = 0.0;
    const auto invalid_cd0_model = prepared(invalid_envelope);
    expect_failure(evaluate_reference(
                       invalid_cd0_model, fixture_input(0.0)),
                   NumericalStatus::DomainError, "envelope-domain",
                   "nonpositive CD0 produced an envelope");
    invalid_envelope = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent);
    invalid_envelope.envelope.polar.induced_drag_factor = 0.0;
    expect_failure(prepare_cavh_formula_model(invalid_envelope),
                   NumericalStatus::DomainError,
                   "envelope-definition",
                   "nonpositive induced-drag factor prepared");
    invalid_envelope = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent);
    invalid_envelope.envelope.polar.cl_slope_per_radian = 0.0;
    expect_failure(prepare_cavh_formula_model(invalid_envelope),
                   NumericalStatus::DomainError,
                   "envelope-definition",
                   "nonpositive lift slope prepared");
    checks.emplace_back("envelope-domain-rejection");

    invalid_envelope = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent);
    invalid_envelope.envelope.polar.alpha_max_radians = 0.1;
    const auto outside_model = prepared(invalid_envelope);
    expect_failure(evaluate_reference(
                       outside_model, fixture_input(0.0)),
                   NumericalStatus::OutOfRange,
                   "envelope-alpha-domain",
                   "out-of-domain envelope optimum produced a value");
    checks.emplace_back("envelope-alpha-domain-rejection");

    CavhFormulaInput vertical_lift = fixture_input(kPi);
    expect_failure(evaluate_reference(
                       eq18_model, vertical_lift),
                   NumericalStatus::DomainError, "formula-domain",
                   "nonpositive vertical lift produced Eq18 output");
    checks.emplace_back("formula-domain-rejection");

    CavhFormulaDefinition singular_definition = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent,
        {0.0, 2.0, 0.05, 0.0, 0.05, 0.0, 0.6});
    const auto singular_model = prepared(singular_definition);
    CavhFormulaInput singular_input;
    singular_input.context = fixture_context();
    singular_input.operating_point = {
        2.0, 2.0, 0.0, 2.0, 2.0, 1.0,
        1.0, 1.0, 0.0, 0.01, 0.0, 0.0};
    expect_failure(evaluate_reference(
                       singular_model, singular_input),
                   NumericalStatus::Singular,
                   "formula-denominator",
                   "singular Eq18 denominator produced output");
    checks.emplace_back("formula-singularity-rejection");

    const auto zero_derivative_model = prepared(fixture_definition(
        GammaReferenceEquation::Eq17MachDependent));
    const auto zero_derivative =
        evaluate_reference(
            zero_derivative_model, fixture_input(0.0));
    expect_failure(zero_derivative, NumericalStatus::IllConditioned,
                   "eq17-derivative-degenerate-fallback-forbidden",
                   "degenerate Eq17 derivative produced output");
    require(!gnc::foundation::has_numerical_flag(
                zero_derivative.evidence().flags,
                NumericalFlag::FallbackUsed),
            "degenerate Eq17 derivative enabled fallback");
    checks.emplace_back("eq17-derivative-fallback-forbidden");

    CavhFormulaInput invalid_mach_partial = fixture_input(kPi / 6.0);
    invalid_mach_partial.operating_point
        .mach_speed_partial_seconds_per_meter = 0.0;
    expect_failure(evaluate_reference(
                       eq17_model, invalid_mach_partial),
                   NumericalStatus::DomainError,
                   "eq17-mach-speed-partial",
                   "nonpositive Mach speed partial produced Eq17 output");
    checks.emplace_back("eq17-mach-partial-rejection");

    CavhFormulaDefinition invalid_tdct = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent);
    invalid_tdct.tdct.gain = -1.0;
    expect_failure(prepare_cavh_formula_model(invalid_tdct),
                   NumericalStatus::DomainError, "tdct-definition",
                   "negative TDCT gain prepared");
    invalid_tdct = fixture_definition(
        GammaReferenceEquation::Eq18MachIndependent);
    invalid_tdct.tdct.alpha_min_radians = 1.0;
    invalid_tdct.tdct.alpha_max_radians = 1.0;
    expect_failure(prepare_cavh_formula_model(invalid_tdct),
                   NumericalStatus::DomainError, "tdct-definition",
                   "invalid TDCT bounds prepared");
    checks.emplace_back("tdct-definition-rejection");

    TdctFormulaInput nonfinite_tdct{
        fixture_context(), std::numeric_limits<double>::quiet_NaN(),
        0.0, 0.0};
    expect_failure(CavhFormulaKernel::evaluate_tdct(
                       eq18_model, nonfinite_tdct),
                   NumericalStatus::NonFiniteInput, "tdct-input",
                   "non-finite TDCT input produced output");
    checks.emplace_back("tdct-nonfinite-rejection");

    const auto failed_composite = CavhFormulaKernel::evaluate(
        zero_derivative_model, fixture_input(0.0));
    expect_failure(failed_composite, NumericalStatus::IllConditioned,
                   "eq17-derivative-degenerate-fallback-forbidden",
                   "composite retained output after Eq17 failure");
    checks.emplace_back("composite-discards-failed-reference");

    return {
        prepared_metadata,
        eq18_model.definition().clock_domain,
        eq18_model.definition().configuration_revision,
        eq18_unbanked_value.telemetry.envelope,
        eq17_value.telemetry.envelope,
        {{"CASE-CAVH-EQ18-UNBANKED", eq18_unbanked_value},
         {"CASE-CAVH-EQ18-BANKED", eq18_banked_value},
         {"CASE-CAVH-EQ17-MACH-ALTITUDE-COUPLED", eq17_value}},
        std::move(tdct_cases),
        composite_value,
        eq17_input.gamma_measured_radians,
        eq17_model.definition().tdct.gain,
        std::move(checks),
    };
}

void write_number(double value) {
    std::cout << (value == 0.0 ? 0.0 : value);
}

void write_envelope(const CavhEnvelopeOutput& output) {
    std::cout << "{\"cd0\":";
    write_number(output.cd0);
    std::cout << ",\"cl_star\":";
    write_number(output.cl_star);
    std::cout << ",\"cd_star\":";
    write_number(output.cd_star);
    std::cout << ",\"lift_to_drag_max\":";
    write_number(output.lift_to_drag_maximum);
    std::cout << ",\"alpha_star_rad\":";
    write_number(output.alpha_star_radians);
    std::cout << ",\"dcl_star_dmach\":";
    write_number(output.dcl_star_dmach);
    std::cout << '}';
}

void write_equation(const GammaReferenceEvaluation& evaluation) {
    const auto& output = evaluation.output;
    const auto& values = evaluation.telemetry.intermediates;
    std::cout << "{\"equation\":\"" << to_string(output.equation)
              << "\",\"density_kg_per_m3\":";
    write_number(values.density_kilograms_per_cubic_meter);
    std::cout << ",\"density_gradient_kg_per_m4\":";
    write_number(
        values.density_altitude_gradient_kilograms_per_cubic_meter_per_meter);
    std::cout << ",\"radius_m\":";
    write_number(values.radius_meters);
    std::cout << ",\"dynamic_pressure_Pa\":";
    write_number(values.dynamic_pressure_pascals);
    std::cout << ",\"drag_force_N\":";
    write_number(values.drag_force_newtons);
    std::cout << ",\"cl_vertical\":";
    write_number(values.cl_vertical);
    std::cout << ",\"dcl_vertical_dmach\":";
    write_number(values.dcl_vertical_dmach);
    std::cout << ",\"partial_mach_partial_speed_s_per_m\":";
    write_number(values.mach_speed_partial_seconds_per_meter);
    std::cout << ",\"partial_mach_partial_altitude_per_m\":";
    write_number(values.mach_altitude_partial_per_meter);
    std::cout << ",\"dcl_vertical_dspeed_s_per_m\":";
    write_number(values.dcl_vertical_dspeed_seconds_per_meter);
    std::cout << ",\"B1\":";
    if (values.b1.has_value()) {
        write_number(*values.b1);
    } else {
        std::cout << "null";
    }
    std::cout << ",\"B2\":";
    write_number(values.b2);
    std::cout << ",\"B3\":";
    write_number(values.b3);
    std::cout << ",\"gamma_reference_rad\":";
    write_number(output.gamma_reference_radians);
    std::cout << '}';
}

void write_tdct(const TdctFormulaEvaluation& evaluation) {
    const auto& output = evaluation.output;
    const auto& telemetry = evaluation.telemetry;
    std::cout << "{\"error_rad\":";
    write_number(telemetry.error_radians);
    std::cout << ",\"correction_rad\":";
    write_number(telemetry.correction_radians);
    std::cout << ",\"alpha_raw_rad\":";
    write_number(telemetry.alpha_raw_radians);
    std::cout << ",\"alpha_command_rad\":";
    write_number(output.alpha_limited_radians);
    std::cout << ",\"saturation\":\"" << to_string(telemetry.saturation)
              << "\"}";
}

void write_json(const ProbeBundle& bundle) {
    std::cout << std::setprecision(17)
              << "{\"schema_version\":\"gnczmkn.cavh-formula-product-probe/1\""
              << ",\"product_model_id\":\"" << kCavhFormulaModelIdentity
              << "\",\"contract_id\":\"" << kCavhFormulaContractIdentity
              << "\",\"source_fixture_id\":\"" << kFixtureId
              << "\",\"source_oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\",\"prepared_model\":{"
              << "\"model_id\":\""
              << bundle.metadata.definition.model_id
              << "\",\"model_version\":\""
              << bundle.metadata.definition.model_version
              << "\",\"execution_form\":\""
              << gnc::model_sdk::to_string(
                     bundle.metadata.definition.execution_form)
              << "\",\"preparation_algorithm_id\":\""
              << bundle.metadata.preparation_algorithm_id
              << "\",\"preparation_algorithm_version\":\""
              << bundle.metadata.preparation_algorithm_version
              << "\"},\"context_policy\":{\"clock_domain_id\":\""
              << bundle.clock_domain.id
              << "\",\"configuration_revision\":"
              << bundle.configuration_revision
              << "},\"envelope_cases\":{";
    std::cout << "\"CASE-CAVH-ENVELOPE-CONSTANT-POLAR\":";
    write_envelope(bundle.constant_envelope);
    std::cout << ",\"CASE-CAVH-ENVELOPE-MACH-DEPENDENT\":";
    write_envelope(bundle.mach_envelope);
    std::cout << "},\"equation_cases\":{";
    for (std::size_t index = 0U; index < bundle.equations.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << '\"' << bundle.equations[index].id << "\":";
        write_equation(bundle.equations[index].evaluation);
    }
    std::cout << "},\"tdct_cases\":{";
    for (std::size_t index = 0U; index < bundle.tdct.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << '\"' << bundle.tdct[index].id << "\":";
        write_tdct(bundle.tdct[index].evaluation);
    }
    const auto& consumer = bundle.typed_consumer;
    std::cout << "},\"typed_consumer\":{\"equation\":\""
              << to_string(consumer.output.gamma_reference.equation)
              << "\",\"alpha_star_rad\":";
    write_number(consumer.output.gamma_reference.alpha_star_radians);
    std::cout << ",\"gamma_reference_rad\":";
    write_number(consumer.output.gamma_reference.gamma_reference_radians);
    std::cout << ",\"gamma_measured_rad\":";
    write_number(bundle.typed_consumer_gamma_measured);
    std::cout << ",\"gain\":";
    write_number(bundle.typed_consumer_gain);
    std::cout << ",\"alpha_raw_rad\":";
    write_number(consumer.telemetry.tdct.alpha_raw_radians);
    std::cout << ",\"alpha_limited_rad\":";
    write_number(consumer.output.tdct.alpha_limited_radians);
    std::cout << ",\"saturation\":\""
              << to_string(consumer.telemetry.tdct.saturation)
              << "\",\"sample_tick\":"
              << consumer.output.tdct.context.sample_time.tick
              << ",\"configuration_revision\":"
              << consumer.output.tdct.context.configuration_revision
              << "},\"direct_checks\":[";
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
        std::cerr << "usage: gnc_cavh_formula_product_probe --self-check\n";
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

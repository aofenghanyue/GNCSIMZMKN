#include "gnc/foundation/scaled_central_difference.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using gnc::foundation::DifferentiationDomain;
using gnc::foundation::NumericalEvidence;
using gnc::foundation::NumericalFlag;
using gnc::foundation::NumericalFlags;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalStatus;
using gnc::foundation::ScaledCentralDifferencePolicy;
using gnc::foundation::ScaledCentralDifferenceResult;

constexpr std::string_view kSchema =
    "gnczmkn.foundation-differentiation-probe/1";
constexpr std::string_view kComponentId =
    "GNC-FOUNDATION-DIFFERENTIATION-001";
constexpr std::string_view kFixtureId = "REF-CAVH-FORMULA-001";

constexpr double kAltitudeM = 30000.0;
constexpr double kSpeedMps = 3000.0;
constexpr double kSeaLevelDensity = 1.225;
constexpr double kDensityScaleHeightM = 7200.0;
constexpr double kSoundSpeedMps = 300.0;
constexpr double kSoundGradientPerM = -0.00075;
constexpr double kMachPoint = 10.0;

struct OutcomeRecord {
    std::string id;
    NumericalStatus status = NumericalStatus::InternalFailure;
    NumericalFlags flags = 0U;
    bool has_value = false;
    std::size_t evaluations = 0U;
    std::string detail;
    std::optional<double> last_step;
    std::optional<ScaledCentralDifferenceResult> result;
};

struct LadderSample {
    double requested_step = 0.0;
    OutcomeRecord outcome;
};

struct LadderObservation {
    std::string id;
    double point = 0.0;
    DifferentiationDomain domain;
    double argument_scale = 0.0;
    double analytic_derivative = 0.0;
    std::vector<LadderSample> samples;
    std::vector<double> error_reduction_ratios;
};

struct RoundoffTransition {
    std::string id;
    double analytic_derivative = 0.0;
    LadderSample well_scaled;
    LadderSample tiny_step;
};

struct Bundle {
    ScaledCentralDifferencePolicy default_policy;
    std::vector<LadderObservation> convergence_ladders;
    std::vector<RoundoffTransition> roundoff_transitions;
    std::vector<OutcomeRecord> scale_selection;
    std::vector<OutcomeRecord> success_semantics;
    std::vector<OutcomeRecord> failure_cases;
};

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

bool near(double actual, double expected, double absolute = 1.0e-14,
          double relative = 1.0e-13) {
    return std::isfinite(actual) && std::isfinite(expected) &&
           std::abs(actual - expected) <=
               absolute + relative *
                              std::max(std::abs(actual), std::abs(expected));
}

NumericalOutcome<double> scalar_value(
    double value, NumericalStatus status = NumericalStatus::Success,
    NumericalFlags flags = 0U) {
    NumericalEvidence evidence;
    evidence.algorithm = {"fixture.cavh.scalar-function@1", "1.0.0"};
    evidence.flags = flags;
    return NumericalOutcome<double>::with_value(status, value, evidence);
}

NumericalOutcome<double> scalar_failure(
    NumericalStatus status, std::string_view detail) {
    NumericalEvidence evidence;
    evidence.algorithm = {"fixture.cavh.scalar-function@1", "1.0.0"};
    evidence.detail = detail;
    return NumericalOutcome<double>::failure(status, evidence);
}

OutcomeRecord record(
    std::string id,
    const NumericalOutcome<ScaledCentralDifferenceResult>& outcome) {
    OutcomeRecord result;
    result.id = std::move(id);
    result.status = outcome.status();
    result.flags = outcome.evidence().flags;
    result.has_value = outcome.has_value();
    result.evaluations = outcome.evidence().evaluations;
    result.detail = std::string(outcome.evidence().detail);
    result.last_step = outcome.evidence().last_step;
    if (outcome.has_value()) {
        result.result = outcome.value();
    }
    return result;
}

ScaledCentralDifferencePolicy policy_for_step(
    double point, double argument_scale, double requested_step) {
    const double selected_scale =
        std::max(std::abs(point), argument_scale);
    return {argument_scale, requested_step / selected_scale};
}

template <typename Function>
OutcomeRecord solve(std::string id, double point,
                    DifferentiationDomain domain, double argument_scale,
                    double requested_step, Function&& function) {
    return record(
        std::move(id),
        gnc::foundation::differentiate_scaled_central(
            point, domain, std::forward<Function>(function),
            policy_for_step(point, argument_scale, requested_step)));
}

double density(double altitude_m) {
    return kSeaLevelDensity *
           std::exp(-altitude_m / kDensityScaleHeightM);
}

double mach_at_altitude(double altitude_m) {
    const double local_sound =
        kSoundSpeedMps +
        kSoundGradientPerM * (altitude_m - kAltitudeM);
    return kSpeedMps / local_sound;
}

double cl_star(double mach) {
    return std::sqrt((0.02 + 0.001 * mach) / 0.08);
}

double analytic_density_gradient() {
    return -density(kAltitudeM) / kDensityScaleHeightM;
}

constexpr double analytic_mach_altitude_gradient() {
    return -kSpeedMps * kSoundGradientPerM /
           (kSoundSpeedMps * kSoundSpeedMps);
}

double analytic_cl_star_gradient() {
    return 0.001 /
           (2.0 * std::sqrt(0.08 * (0.02 + 0.001 * kMachPoint)));
}

NumericalOutcome<double> density_function(double altitude_m) {
    if (!std::isfinite(altitude_m) || altitude_m < 0.0 ||
        altitude_m > 60000.0) {
        return scalar_failure(NumericalStatus::DomainError,
                              "density-domain");
    }
    return scalar_value(density(altitude_m));
}

NumericalOutcome<double> mach_altitude_function(double altitude_m) {
    if (!std::isfinite(altitude_m) || altitude_m < 0.0 ||
        altitude_m > 60000.0) {
        return scalar_failure(NumericalStatus::DomainError,
                              "mach-altitude-domain");
    }
    const double local_sound =
        kSoundSpeedMps +
        kSoundGradientPerM * (altitude_m - kAltitudeM);
    if (!(local_sound > 0.0)) {
        return scalar_failure(NumericalStatus::DomainError,
                              "sound-speed-domain");
    }
    return scalar_value(mach_at_altitude(altitude_m));
}

NumericalOutcome<double> cl_star_function(double mach) {
    if (!std::isfinite(mach) || mach < 0.0 || mach > 20.0) {
        return scalar_failure(NumericalStatus::DomainError,
                              "cl-star-domain");
    }
    return scalar_value(cl_star(mach));
}

template <typename Function>
LadderObservation make_ladder(
    std::string id, double point, DifferentiationDomain domain,
    double argument_scale, double analytic_derivative,
    const std::vector<double>& steps, Function&& function) {
    LadderObservation ladder;
    ladder.id = std::move(id);
    ladder.point = point;
    ladder.domain = domain;
    ladder.argument_scale = argument_scale;
    ladder.analytic_derivative = analytic_derivative;
    for (double step : steps) {
        ladder.samples.push_back(
            {step, solve(ladder.id, point, domain, argument_scale, step,
                         function)});
    }
    for (std::size_t index = 0U;
         index + 1U < ladder.samples.size(); ++index) {
        const double error = std::abs(
            ladder.samples[index].outcome.result->derivative -
            analytic_derivative);
        const double next_error = std::abs(
            ladder.samples[index + 1U].outcome.result->derivative -
            analytic_derivative);
        ladder.error_reduction_ratios.push_back(error / next_error);
    }
    return ladder;
}

template <typename Function>
RoundoffTransition make_transition(
    std::string id, double point, DifferentiationDomain domain,
    double argument_scale, double analytic_derivative,
    double well_step, double tiny_step, Function&& function) {
    RoundoffTransition result;
    result.id = std::move(id);
    result.analytic_derivative = analytic_derivative;
    result.well_scaled = {
        well_step,
        solve(result.id + "-WELL", point, domain, argument_scale,
              well_step, function)};
    result.tiny_step = {
        tiny_step,
        solve(result.id + "-TINY", point, domain, argument_scale,
              tiny_step, function)};
    return result;
}

std::vector<OutcomeRecord> scale_selection_cases() {
    const auto linear = [](double value) {
        return scalar_value(3.0 * value + 2.0);
    };
    return {
        solve("ZERO-POINT-NOMINAL-SCALE", 0.0,
              DifferentiationDomain{-1000.0, 1000.0}, 20.0, 2.5,
              linear),
        solve("POINT-MAGNITUDE-SCALE", 100.0,
              DifferentiationDomain{-1000.0, 1000.0}, 20.0, 12.5,
              linear),
        record(
            "DEFAULT-POLICY-CL-STAR",
            gnc::foundation::differentiate_scaled_central(
                kMachPoint, DifferentiationDomain{0.0, 20.0},
                cl_star_function, ScaledCentralDifferencePolicy{})),
    };
}

std::vector<OutcomeRecord> success_semantics() {
    const auto policy = policy_for_step(1.0, 1.0, 0.25);
    std::vector<OutcomeRecord> results;
    results.push_back(record(
        "APPROXIMATE-FLAG-PROPAGATION",
        gnc::foundation::differentiate_scaled_central(
            1.0, DifferentiationDomain{-2.0, 2.0},
            [](double value) {
                return scalar_value(
                    2.0 * value, NumericalStatus::Approximate,
                    gnc::foundation::numerical_flag(
                        NumericalFlag::Clamped));
            },
            policy)));
    results.push_back(record(
        "EXTRAPOLATED-STATUS-PROPAGATION",
        gnc::foundation::differentiate_scaled_central(
            1.0, DifferentiationDomain{-2.0, 2.0},
            [](double value) {
                return scalar_value(value * value,
                                    NumericalStatus::Extrapolated);
            },
            policy)));
    results.push_back(record(
        "ZERO-DERIVATIVE-CANCELLATION-EVIDENCE",
        gnc::foundation::differentiate_scaled_central(
            1.0, DifferentiationDomain{-2.0, 2.0},
            [](double) { return scalar_value(4.0); }, policy)));
    return results;
}

std::vector<OutcomeRecord> failure_cases() {
    std::vector<OutcomeRecord> failures;
    const auto function = [](double value) { return scalar_value(value); };
    const DifferentiationDomain unit_domain{-1.0, 1.0};

    ScaledCentralDifferencePolicy invalid_scale =
        policy_for_step(0.0, 1.0, 0.25);
    invalid_scale.argument_scale = 0.0;
    failures.push_back(record(
        "INVALID-ARGUMENT-SCALE",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain, function, invalid_scale)));
    ScaledCentralDifferencePolicy nonfinite_scale =
        policy_for_step(0.0, 1.0, 0.25);
    nonfinite_scale.argument_scale =
        std::numeric_limits<double>::infinity();
    failures.push_back(record(
        "NONFINITE-ARGUMENT-SCALE",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain, function, nonfinite_scale)));

    ScaledCentralDifferencePolicy invalid_relative =
        policy_for_step(0.0, 1.0, 0.25);
    invalid_relative.relative_step = 0.0;
    failures.push_back(record(
        "INVALID-RELATIVE-STEP",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain, function, invalid_relative)));
    ScaledCentralDifferencePolicy nonfinite_relative =
        policy_for_step(0.0, 1.0, 0.25);
    nonfinite_relative.relative_step =
        std::numeric_limits<double>::quiet_NaN();
    failures.push_back(record(
        "NONFINITE-RELATIVE-STEP",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain, function, nonfinite_relative)));

    failures.push_back(record(
        "NONFINITE-DOMAIN",
        gnc::foundation::differentiate_scaled_central(
            0.0,
            DifferentiationDomain{
                -1.0, std::numeric_limits<double>::infinity()},
            function, policy_for_step(0.0, 1.0, 0.25))));
    failures.push_back(record(
        "INVALID-DOMAIN",
        gnc::foundation::differentiate_scaled_central(
            0.0, DifferentiationDomain{1.0, -1.0}, function,
            policy_for_step(0.0, 1.0, 0.25))));
    failures.push_back(record(
        "NONFINITE-POINT",
        gnc::foundation::differentiate_scaled_central(
            std::numeric_limits<double>::quiet_NaN(), unit_domain,
            function, policy_for_step(0.0, 1.0, 0.25))));
    failures.push_back(record(
        "POINT-OUTSIDE-DOMAIN",
        gnc::foundation::differentiate_scaled_central(
            2.0, unit_domain, function,
            policy_for_step(2.0, 1.0, 0.25))));

    const double maximum = std::numeric_limits<double>::max();
    failures.push_back(record(
        "REQUESTED-STEP-OVERFLOW",
        gnc::foundation::differentiate_scaled_central(
            0.5 * maximum, DifferentiationDomain{-maximum, maximum},
            function,
            ScaledCentralDifferencePolicy{maximum, 2.0})));
    failures.push_back(record(
        "REQUESTED-STEP-UNDERFLOW",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain, function,
            ScaledCentralDifferencePolicy{
                std::numeric_limits<double>::denorm_min(), 0.25})));
    failures.push_back(record(
        "UNREPRESENTABLE-CENTRAL-STEP",
        gnc::foundation::differentiate_scaled_central(
            1.0e16, DifferentiationDomain{0.0, 2.0e16}, function,
            ScaledCentralDifferencePolicy{1.0, 1.0e-20})));
    failures.push_back(record(
        "CENTRAL-SAMPLES-OUTSIDE-DOMAIN",
        gnc::foundation::differentiate_scaled_central(
            0.9, DifferentiationDomain{0.0, 1.0}, function,
            policy_for_step(0.9, 1.0, 0.2))));
    failures.push_back(record(
        "NONFINITE-SAMPLE-ARGUMENTS",
        gnc::foundation::differentiate_scaled_central(
            0.75 * maximum,
            DifferentiationDomain{-maximum, maximum}, function,
            ScaledCentralDifferencePolicy{0.75 * maximum, 0.5})));
    failures.push_back(record(
        "LOWER-CALLBACK-DOMAIN",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain,
            [](double value) {
                return value < 0.0
                           ? scalar_failure(NumericalStatus::DomainError,
                                            "lower-domain")
                           : scalar_value(value);
            },
            policy_for_step(0.0, 1.0, 0.5))));
    failures.push_back(record(
        "UPPER-CALLBACK-DOMAIN",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain,
            [](double value) {
                return value > 0.0
                           ? scalar_failure(NumericalStatus::DomainError,
                                            "upper-domain")
                           : scalar_value(value);
            },
            policy_for_step(0.0, 1.0, 0.5))));
    failures.push_back(record(
        "NONFINITE-FUNCTION-VALUE",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain,
            [](double) {
                return scalar_value(
                    std::numeric_limits<double>::infinity());
            },
            policy_for_step(0.0, 1.0, 0.5))));
    failures.push_back(record(
        "NONFINITE-FUNCTION-DIFFERENCE",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain,
            [maximum](double value) {
                return scalar_value(value < 0.0 ? -maximum : maximum);
            },
            policy_for_step(0.0, 1.0, 0.5))));
    failures.push_back(record(
        "NONFINITE-DERIVATIVE",
        gnc::foundation::differentiate_scaled_central(
            0.0, unit_domain,
            [maximum](double value) {
                return scalar_value(value < 0.0 ? 0.0 : maximum);
            },
            ScaledCentralDifferencePolicy{1.0e-300, 1.0})));
    return failures;
}

Bundle run_bundle() {
    Bundle bundle;
    bundle.default_policy = ScaledCentralDifferencePolicy{};
    const std::vector<double> altitude_steps{
        800.0, 400.0, 200.0, 100.0, 50.0};
    const std::vector<double> mach_steps{
        0.4, 0.2, 0.1, 0.05, 0.025};
    bundle.convergence_ladders.push_back(make_ladder(
        "DENSITY-ALTITUDE", kAltitudeM,
        DifferentiationDomain{0.0, 60000.0}, kAltitudeM,
        analytic_density_gradient(), altitude_steps, density_function));
    bundle.convergence_ladders.push_back(make_ladder(
        "MACH-ALTITUDE", kAltitudeM,
        DifferentiationDomain{0.0, 60000.0}, kAltitudeM,
        analytic_mach_altitude_gradient(), altitude_steps,
        mach_altitude_function));
    bundle.convergence_ladders.push_back(make_ladder(
        "CL-STAR-MACH", kMachPoint,
        DifferentiationDomain{0.0, 20.0}, kMachPoint,
        analytic_cl_star_gradient(), mach_steps, cl_star_function));

    bundle.roundoff_transitions.push_back(make_transition(
        "DENSITY-ALTITUDE", kAltitudeM,
        DifferentiationDomain{0.0, 60000.0}, kAltitudeM,
        analytic_density_gradient(), 0.1, 1.0e-9,
        density_function));
    bundle.roundoff_transitions.push_back(make_transition(
        "MACH-ALTITUDE", kAltitudeM,
        DifferentiationDomain{0.0, 60000.0}, kAltitudeM,
        analytic_mach_altitude_gradient(), 1.0, 1.0e-9,
        mach_altitude_function));
    bundle.roundoff_transitions.push_back(make_transition(
        "CL-STAR-MACH", kMachPoint,
        DifferentiationDomain{0.0, 20.0}, kMachPoint,
        analytic_cl_star_gradient(), 1.0e-4, 1.0e-12,
        cl_star_function));

    bundle.scale_selection = scale_selection_cases();
    bundle.success_semantics = success_semantics();
    bundle.failure_cases = failure_cases();
    return bundle;
}

void self_check(const Bundle& bundle) {
    require(gnc::foundation::kScaledCentralDifferenceIdentity.id ==
                "gnc.foundation.differentiation.scaled-central@1" &&
                near(bundle.default_policy.relative_step,
                     gnc::foundation::kDefaultCentralDifferenceRelativeStep,
                     0.0, 0.0),
            "scaled central difference identity differs");
    require(bundle.convergence_ladders.size() == 3U,
            "CAVH differentiation ladder count differs");
    for (const LadderObservation& ladder :
         bundle.convergence_ladders) {
        require(ladder.samples.size() == 5U &&
                    ladder.error_reduction_ratios.size() == 4U,
                "CAVH differentiation ladder size differs");
        double previous_error = std::numeric_limits<double>::infinity();
        for (const LadderSample& sample : ladder.samples) {
            require(sample.outcome.status == NumericalStatus::Success &&
                        sample.outcome.has_value &&
                        sample.outcome.evaluations == 2U &&
                        sample.outcome.result.has_value() &&
                        near(sample.outcome.result->requested_step,
                             sample.requested_step, 2.0e-12, 2.0e-15) &&
                        near(sample.outcome.result->risk.normalized_step,
                             sample.outcome.result->effective_step /
                                 sample.outcome.result
                                     ->selected_argument_scale,
                             1.0e-18, 1.0e-14),
                    "CAVH differentiation outcome differs");
            const double error = std::abs(
                sample.outcome.result->derivative -
                ladder.analytic_derivative);
            require(error < previous_error,
                    "CAVH differentiation error is not monotone");
            previous_error = error;
        }
        for (double ratio : ladder.error_reduction_ratios) {
            require(ratio > 3.8 && ratio < 4.2,
                    "CAVH central difference order differs");
        }
    }

    require(bundle.roundoff_transitions.size() == 3U,
            "roundoff transition count differs");
    for (const RoundoffTransition& transition :
         bundle.roundoff_transitions) {
        require(transition.well_scaled.outcome.has_value &&
                    transition.tiny_step.outcome.has_value,
                "roundoff transition outcome is missing");
        const auto& well = *transition.well_scaled.outcome.result;
        const auto& tiny = *transition.tiny_step.outcome.result;
        const double well_error =
            std::abs(well.derivative - transition.analytic_derivative);
        const double tiny_error =
            std::abs(tiny.derivative - transition.analytic_derivative);
        require(tiny_error > 1000.0 * std::max(well_error, 1.0e-20) &&
                    tiny.risk.output_cancellation_ratio <
                        well.risk.output_cancellation_ratio,
                "double precision roundoff transition differs");
    }

    require(bundle.scale_selection.size() == 3U &&
                near(bundle.scale_selection[0U].result->requested_step,
                     2.5) &&
                near(bundle.scale_selection[0U].result
                         ->selected_argument_scale,
                     20.0) &&
                near(bundle.scale_selection[1U].result->requested_step,
                     12.5) &&
                near(bundle.scale_selection[1U].result
                         ->selected_argument_scale,
                     100.0) &&
                near(bundle.scale_selection[0U].result->derivative, 3.0) &&
                near(bundle.scale_selection[1U].result->derivative, 3.0) &&
                bundle.scale_selection[2U].status ==
                    NumericalStatus::Success &&
                near(bundle.scale_selection[2U].result
                         ->selected_argument_scale,
                     10.0) &&
                near(bundle.scale_selection[2U].result->requested_step,
                     10.0 *
                         gnc::foundation::
                             kDefaultCentralDifferenceRelativeStep) &&
                std::abs(bundle.scale_selection[2U].result->derivative -
                         analytic_cl_star_gradient()) < 2.0e-10,
            "scaled step selection differs");

    require(bundle.success_semantics.size() == 3U &&
                bundle.success_semantics[0U].status ==
                    NumericalStatus::Approximate &&
                gnc::foundation::has_numerical_flag(
                    bundle.success_semantics[0U].flags,
                    NumericalFlag::Clamped) &&
                bundle.success_semantics[1U].status ==
                    NumericalStatus::Extrapolated &&
                bundle.success_semantics[2U].status ==
                    NumericalStatus::Success &&
                bundle.success_semantics[2U].result->derivative == 0.0 &&
                bundle.success_semantics[2U].result->risk
                        .output_cancellation_ratio == 0.0,
            "central difference success semantics differ");

    const std::array<
        std::tuple<NumericalStatus, std::string_view, std::size_t>, 18U>
        expected{{
            {NumericalStatus::DomainError, "policy", 0U},
            {NumericalStatus::DomainError, "policy", 0U},
            {NumericalStatus::DomainError, "policy", 0U},
            {NumericalStatus::DomainError, "policy", 0U},
            {NumericalStatus::NonFiniteInput, "domain", 0U},
            {NumericalStatus::DomainError, "domain", 0U},
            {NumericalStatus::NonFiniteInput, "point", 0U},
            {NumericalStatus::DomainError, "point-outside-domain", 0U},
            {NumericalStatus::NonFiniteIntermediate, "requested-step", 0U},
            {NumericalStatus::StepUnderflow,
             "requested-step-underflow", 0U},
            {NumericalStatus::StepUnderflow,
             "unrepresentable-central-step", 0U},
            {NumericalStatus::DomainError,
             "central-samples-outside-domain", 0U},
            {NumericalStatus::NonFiniteIntermediate,
             "sample-arguments", 0U},
            {NumericalStatus::DomainError, "lower-domain", 1U},
            {NumericalStatus::DomainError, "upper-domain", 2U},
            {NumericalStatus::NonFiniteIntermediate,
             "function-value", 1U},
            {NumericalStatus::NonFiniteIntermediate,
             "function-difference", 2U},
            {NumericalStatus::NonFiniteOutput, "derivative", 2U},
        }};
    require(bundle.failure_cases.size() == expected.size(),
            "central difference failure count differs");
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        require(bundle.failure_cases[index].status ==
                    std::get<0>(expected[index]) &&
                    bundle.failure_cases[index].detail ==
                        std::get<1>(expected[index]) &&
                    bundle.failure_cases[index].evaluations ==
                        std::get<2>(expected[index]) &&
                    !bundle.failure_cases[index].has_value,
                "central difference failure semantics differ");
    }
}

void write_string(std::string_view value) {
    std::cout << '"' << value << '"';
}

void write_result(const ScaledCentralDifferenceResult& result) {
    std::cout << "{\"derivative\":" << result.derivative
              << ",\"point\":" << result.point
              << ",\"domain\":{\"lower\":" << result.domain.lower
              << ",\"upper\":" << result.domain.upper
              << "},\"nominal_argument_scale\":"
              << result.nominal_argument_scale
              << ",\"selected_argument_scale\":"
              << result.selected_argument_scale
              << ",\"relative_step\":" << result.relative_step
              << ",\"requested_step\":" << result.requested_step
              << ",\"lower_argument\":" << result.lower_argument
              << ",\"upper_argument\":" << result.upper_argument
              << ",\"lower_value\":" << result.lower_value
              << ",\"upper_value\":" << result.upper_value
              << ",\"lower_step\":" << result.lower_step
              << ",\"upper_step\":" << result.upper_step
              << ",\"effective_step\":" << result.effective_step
              << ",\"risk\":{\"normalized_step\":"
              << result.risk.normalized_step
              << ",\"output_cancellation_ratio\":"
              << result.risk.output_cancellation_ratio
              << ",\"step_asymmetry_ratio\":"
              << result.risk.step_asymmetry_ratio << "}}";
}

void write_outcome(const OutcomeRecord& outcome) {
    std::cout << "{\"id\":";
    write_string(outcome.id);
    std::cout << ",\"status\":";
    write_string(gnc::foundation::to_string(outcome.status));
    std::cout << ",\"flags\":" << outcome.flags
              << ",\"has_value\":"
              << (outcome.has_value ? "true" : "false")
              << ",\"evaluations\":" << outcome.evaluations
              << ",\"detail\":";
    write_string(outcome.detail);
    std::cout << ",\"last_step\":";
    if (outcome.last_step.has_value()) {
        std::cout << *outcome.last_step;
    } else {
        std::cout << "null";
    }
    std::cout << ",\"result\":";
    if (outcome.result.has_value()) {
        write_result(*outcome.result);
    } else {
        std::cout << "null";
    }
    std::cout << '}';
}

void write_sample(const LadderSample& sample) {
    std::cout << "{\"requested_step\":" << sample.requested_step
              << ",\"outcome\":";
    write_outcome(sample.outcome);
    std::cout << '}';
}

void write_bundle(const Bundle& bundle) {
    std::cout << std::setprecision(17)
              << "{\"schema_version\":";
    write_string(kSchema);
    std::cout << ",\"component_id\":";
    write_string(kComponentId);
    std::cout << ",\"fixture_id\":";
    write_string(kFixtureId);
    std::cout << ",\"algorithm\":{\"id\":";
    write_string(gnc::foundation::kScaledCentralDifferenceIdentity.id);
    std::cout << ",\"version\":";
    write_string(gnc::foundation::kScaledCentralDifferenceIdentity.version);
    std::cout << "},\"default_policy\":{\"argument_scale\":"
              << bundle.default_policy.argument_scale
              << ",\"relative_step\":"
              << bundle.default_policy.relative_step
              << "},\"convergence_ladders\":[";
    for (std::size_t index = 0U;
         index < bundle.convergence_ladders.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        const LadderObservation& ladder =
            bundle.convergence_ladders[index];
        std::cout << "{\"id\":";
        write_string(ladder.id);
        std::cout << ",\"point\":" << ladder.point
                  << ",\"domain\":{\"lower\":"
                  << ladder.domain.lower << ",\"upper\":"
                  << ladder.domain.upper
                  << "},\"argument_scale\":"
                  << ladder.argument_scale
                  << ",\"analytic_derivative\":"
                  << ladder.analytic_derivative << ",\"samples\":[";
        for (std::size_t sample = 0U; sample < ladder.samples.size();
             ++sample) {
            if (sample != 0U) {
                std::cout << ',';
            }
            write_sample(ladder.samples[sample]);
        }
        std::cout << "],\"error_reduction_ratios\":[";
        for (std::size_t ratio = 0U;
             ratio < ladder.error_reduction_ratios.size(); ++ratio) {
            if (ratio != 0U) {
                std::cout << ',';
            }
            std::cout << ladder.error_reduction_ratios[ratio];
        }
        std::cout << "]}";
    }
    std::cout << "],\"roundoff_transitions\":[";
    for (std::size_t index = 0U;
         index < bundle.roundoff_transitions.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        const RoundoffTransition& transition =
            bundle.roundoff_transitions[index];
        std::cout << "{\"id\":";
        write_string(transition.id);
        std::cout << ",\"analytic_derivative\":"
                  << transition.analytic_derivative
                  << ",\"well_scaled\":";
        write_sample(transition.well_scaled);
        std::cout << ",\"tiny_step\":";
        write_sample(transition.tiny_step);
        std::cout << '}';
    }
    std::cout << ']';

    const auto write_outcome_array = [](const char* name,
                                        const std::vector<OutcomeRecord>&
                                            outcomes) {
        std::cout << ",\"" << name << "\":[";
        for (std::size_t index = 0U; index < outcomes.size(); ++index) {
            if (index != 0U) {
                std::cout << ',';
            }
            write_outcome(outcomes[index]);
        }
        std::cout << ']';
    };
    write_outcome_array("scale_selection", bundle.scale_selection);
    write_outcome_array("success_semantics", bundle.success_semantics);
    write_outcome_array("failure_cases", bundle.failure_cases);
    std::cout << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2 || std::string_view(argv[1]) != "--self-check") {
            std::cerr << "usage: gnc_foundation_differentiation_probe "
                         "--self-check\n";
            return EXIT_FAILURE;
        }
        const Bundle bundle = run_bundle();
        self_check(bundle);
        write_bundle(bundle);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "foundation differentiation self-check failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

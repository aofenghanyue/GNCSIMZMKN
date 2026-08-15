#include "gnc/foundation/scaled_one_sided_difference.hpp"

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
using gnc::foundation::OneSidedDirection;
using gnc::foundation::ScaledOneSidedDifferencePolicy;
using gnc::foundation::ScaledOneSidedDifferenceResult;

constexpr std::string_view kSchema =
    "gnczmkn.foundation-one-sided-differentiation-probe/1";
constexpr std::string_view kComponentId =
    "GNC-FOUNDATION-ONE-SIDED-DIFFERENTIATION-001";
constexpr std::string_view kFixtureId = "REF-CAVH-FORMULA-001";

constexpr double kReferenceAltitudeM = 30000.0;
constexpr double kSpeedMps = 3000.0;
constexpr double kSeaLevelDensity = 1.225;
constexpr double kDensityScaleHeightM = 7200.0;
constexpr double kSoundSpeedMps = 300.0;
constexpr double kSoundGradientPerM = -0.00075;

struct OutcomeRecord {
    std::string id;
    NumericalStatus status = NumericalStatus::InternalFailure;
    NumericalFlags flags = 0U;
    bool has_value = false;
    std::size_t evaluations = 0U;
    std::string detail;
    std::optional<double> last_step;
    std::optional<ScaledOneSidedDifferenceResult> result;
};

struct LadderSample {
    double requested_step = 0.0;
    OutcomeRecord outcome;
};

struct LadderObservation {
    std::string id;
    OneSidedDirection direction = OneSidedDirection::Forward;
    double point = 0.0;
    DifferentiationDomain domain;
    double argument_scale = 0.0;
    double analytic_derivative = 0.0;
    std::vector<LadderSample> samples;
    std::vector<double> error_reduction_ratios;
};

struct RoundoffTransition {
    std::string id;
    OneSidedDirection direction = OneSidedDirection::Forward;
    double analytic_derivative = 0.0;
    LadderSample well_scaled;
    LadderSample tiny_step;
};

struct Bundle {
    ScaledOneSidedDifferencePolicy default_policy;
    std::vector<LadderObservation> convergence_ladders;
    std::vector<RoundoffTransition> roundoff_transitions;
    std::vector<OutcomeRecord> direction_semantics;
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
    const NumericalOutcome<ScaledOneSidedDifferenceResult>& outcome) {
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

ScaledOneSidedDifferencePolicy policy_for_step(
    double point, double argument_scale, double requested_step) {
    const double selected_scale =
        std::max(std::abs(point), argument_scale);
    return {argument_scale, requested_step / selected_scale};
}

template <typename Function>
OutcomeRecord solve(std::string id, double point,
                    DifferentiationDomain domain,
                    OneSidedDirection direction, double argument_scale,
                    double requested_step, Function&& function) {
    return record(
        std::move(id),
        gnc::foundation::differentiate_scaled_one_sided(
            point, domain, direction, std::forward<Function>(function),
            policy_for_step(point, argument_scale, requested_step)));
}

double density(double altitude_m) {
    return kSeaLevelDensity *
           std::exp(-altitude_m / kDensityScaleHeightM);
}

double local_sound_speed(double altitude_m) {
    return kSoundSpeedMps +
           kSoundGradientPerM * (altitude_m - kReferenceAltitudeM);
}

double mach_at_altitude(double altitude_m) {
    return kSpeedMps / local_sound_speed(altitude_m);
}

double cl_star(double mach) {
    return std::sqrt((0.02 + 0.001 * mach) / 0.08);
}

double analytic_density_gradient(double altitude_m) {
    return -density(altitude_m) / kDensityScaleHeightM;
}

double analytic_mach_altitude_gradient(double altitude_m) {
    const double sound = local_sound_speed(altitude_m);
    return -kSpeedMps * kSoundGradientPerM / (sound * sound);
}

double analytic_cl_star_gradient(double mach) {
    return 0.001 /
           (2.0 * std::sqrt(0.08 * (0.02 + 0.001 * mach)));
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
    if (!(local_sound_speed(altitude_m) > 0.0)) {
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
    OneSidedDirection direction, double argument_scale,
    double analytic_derivative, const std::vector<double>& steps,
    Function&& function) {
    LadderObservation ladder;
    ladder.id = std::move(id);
    ladder.direction = direction;
    ladder.point = point;
    ladder.domain = domain;
    ladder.argument_scale = argument_scale;
    ladder.analytic_derivative = analytic_derivative;
    for (double step : steps) {
        ladder.samples.push_back(
            {step, solve(ladder.id, point, domain, direction,
                         argument_scale, step, function)});
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
    OneSidedDirection direction, double argument_scale,
    double analytic_derivative, double well_step, double tiny_step,
    Function&& function) {
    RoundoffTransition result;
    result.id = std::move(id);
    result.direction = direction;
    result.analytic_derivative = analytic_derivative;
    result.well_scaled = {
        well_step,
        solve(result.id + "-WELL", point, domain, direction,
              argument_scale, well_step, function)};
    result.tiny_step = {
        tiny_step,
        solve(result.id + "-TINY", point, domain, direction,
              argument_scale, tiny_step, function)};
    return result;
}

std::vector<OutcomeRecord> direction_semantics() {
    const auto quadratic = [](double value) {
        return scalar_value(value * value);
    };
    std::vector<OutcomeRecord> outcomes;
    outcomes.push_back(solve(
        "FORWARD-QUADRATIC", 1.0, DifferentiationDomain{-2.0, 4.0},
        OneSidedDirection::Forward, 1.0, 0.25, quadratic));
    outcomes.push_back(solve(
        "BACKWARD-QUADRATIC", 1.0, DifferentiationDomain{-2.0, 4.0},
        OneSidedDirection::Backward, 1.0, 0.25, quadratic));

    std::vector<double> arguments;
    auto ordered = gnc::foundation::differentiate_scaled_one_sided(
        0.0, DifferentiationDomain{0.0, 2.0},
        OneSidedDirection::Forward,
        [&arguments](double value) {
            arguments.push_back(value);
            return scalar_value(value);
        },
        policy_for_step(0.0, 1.0, 0.5));
    require(ordered.has_value() && arguments.size() == 3U &&
                arguments[0U] == ordered.value().point &&
                arguments[1U] == ordered.value().nearest_argument &&
                arguments[2U] == ordered.value().far_argument,
            "one-sided callback evaluation order differs");
    outcomes.push_back(record("POINT-NEAREST-FAR-CALLBACK-ORDER", ordered));
    return outcomes;
}

std::vector<OutcomeRecord> scale_selection_cases() {
    const auto linear = [](double value) {
        return scalar_value(3.0 * value + 2.0);
    };
    return {
        solve("ZERO-POINT-NOMINAL-SCALE", 0.0,
              DifferentiationDomain{0.0, 1000.0},
              OneSidedDirection::Forward, 20.0, 2.5, linear),
        solve("POINT-MAGNITUDE-SCALE", 100.0,
              DifferentiationDomain{-1000.0, 1000.0},
              OneSidedDirection::Backward, 20.0, 12.5, linear),
        record(
            "DEFAULT-POLICY-DENSITY-LOWER",
            gnc::foundation::differentiate_scaled_one_sided(
                0.0, DifferentiationDomain{0.0, 60000.0},
                OneSidedDirection::Forward, density_function,
                ScaledOneSidedDifferencePolicy{})),
        record(
            "DEFAULT-POLICY-CL-STAR-UPPER",
            gnc::foundation::differentiate_scaled_one_sided(
                20.0, DifferentiationDomain{0.0, 20.0},
                OneSidedDirection::Backward, cl_star_function,
                ScaledOneSidedDifferencePolicy{})),
    };
}

std::vector<OutcomeRecord> success_semantics() {
    const auto policy = policy_for_step(0.0, 1.0, 0.25);
    std::vector<OutcomeRecord> results;
    results.push_back(record(
        "APPROXIMATE-FLAG-PROPAGATION",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, DifferentiationDomain{0.0, 1.0},
            OneSidedDirection::Forward,
            [](double value) {
                return scalar_value(
                    2.0 * value, NumericalStatus::Approximate,
                    gnc::foundation::numerical_flag(
                        NumericalFlag::Clamped));
            },
            policy)));
    results.push_back(record(
        "EXTRAPOLATED-STATUS-PROPAGATION",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, DifferentiationDomain{0.0, 1.0},
            OneSidedDirection::Forward,
            [](double value) {
                return scalar_value(value * value,
                                    NumericalStatus::Extrapolated);
            },
            policy)));
    results.push_back(record(
        "ZERO-DERIVATIVE-CANCELLATION-EVIDENCE",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, DifferentiationDomain{0.0, 1.0},
            OneSidedDirection::Forward,
            [](double) { return scalar_value(4.0); }, policy)));
    return results;
}

std::vector<OutcomeRecord> failure_cases() {
    std::vector<OutcomeRecord> failures;
    const auto function = [](double value) { return scalar_value(value); };
    const DifferentiationDomain unit_domain{-1.0, 1.0};
    const auto unit_policy = policy_for_step(0.0, 1.0, 0.25);

    ScaledOneSidedDifferencePolicy invalid_scale = unit_policy;
    invalid_scale.argument_scale = 0.0;
    failures.push_back(record(
        "INVALID-ARGUMENT-SCALE",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward, function,
            invalid_scale)));
    ScaledOneSidedDifferencePolicy nonfinite_scale = unit_policy;
    nonfinite_scale.argument_scale =
        std::numeric_limits<double>::infinity();
    failures.push_back(record(
        "NONFINITE-ARGUMENT-SCALE",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward, function,
            nonfinite_scale)));
    ScaledOneSidedDifferencePolicy invalid_relative = unit_policy;
    invalid_relative.relative_step = 0.0;
    failures.push_back(record(
        "INVALID-RELATIVE-STEP",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward, function,
            invalid_relative)));
    ScaledOneSidedDifferencePolicy nonfinite_relative = unit_policy;
    nonfinite_relative.relative_step =
        std::numeric_limits<double>::quiet_NaN();
    failures.push_back(record(
        "NONFINITE-RELATIVE-STEP",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward, function,
            nonfinite_relative)));
    failures.push_back(record(
        "INVALID-DIRECTION",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain,
            static_cast<OneSidedDirection>(255U), function,
            unit_policy)));
    failures.push_back(record(
        "NONFINITE-DOMAIN",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0,
            DifferentiationDomain{
                -1.0, std::numeric_limits<double>::infinity()},
            OneSidedDirection::Forward, function, unit_policy)));
    failures.push_back(record(
        "INVALID-DOMAIN",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, DifferentiationDomain{1.0, -1.0},
            OneSidedDirection::Forward, function, unit_policy)));
    failures.push_back(record(
        "NONFINITE-POINT",
        gnc::foundation::differentiate_scaled_one_sided(
            std::numeric_limits<double>::quiet_NaN(), unit_domain,
            OneSidedDirection::Forward, function, unit_policy)));
    failures.push_back(record(
        "POINT-OUTSIDE-DOMAIN",
        gnc::foundation::differentiate_scaled_one_sided(
            2.0, unit_domain, OneSidedDirection::Forward, function,
            policy_for_step(2.0, 1.0, 0.25))));

    const double maximum = std::numeric_limits<double>::max();
    failures.push_back(record(
        "REQUESTED-STEP-OVERFLOW",
        gnc::foundation::differentiate_scaled_one_sided(
            0.5 * maximum, DifferentiationDomain{-maximum, maximum},
            OneSidedDirection::Forward, function,
            ScaledOneSidedDifferencePolicy{maximum, 2.0})));
    failures.push_back(record(
        "REQUESTED-STEP-UNDERFLOW",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward, function,
            ScaledOneSidedDifferencePolicy{
                std::numeric_limits<double>::denorm_min(), 0.25})));
    failures.push_back(record(
        "NONFINITE-SAMPLE-ARGUMENTS",
        gnc::foundation::differentiate_scaled_one_sided(
            0.75 * maximum,
            DifferentiationDomain{-maximum, maximum},
            OneSidedDirection::Forward, function,
            ScaledOneSidedDifferencePolicy{0.75 * maximum, 0.5})));
    failures.push_back(record(
        "UNREPRESENTABLE-NEAREST-STEP",
        gnc::foundation::differentiate_scaled_one_sided(
            1.0e16, DifferentiationDomain{0.0, 2.0e16},
            OneSidedDirection::Forward, function,
            policy_for_step(1.0e16, 1.0, 1.0))));
    const double binade_point = std::nextafter(2.0, 0.0);
    const double binade_step = 2.0 - binade_point;
    failures.push_back(record(
        "UNREPRESENTABLE-FAR-STEP",
        gnc::foundation::differentiate_scaled_one_sided(
            binade_point, DifferentiationDomain{0.0, 4.0},
            OneSidedDirection::Forward, function,
            policy_for_step(binade_point, 1.0, binade_step))));
    failures.push_back(record(
        "FORWARD-SAMPLES-OUTSIDE-DOMAIN",
        gnc::foundation::differentiate_scaled_one_sided(
            0.9, DifferentiationDomain{0.0, 1.0},
            OneSidedDirection::Forward, function,
            policy_for_step(0.9, 1.0, 0.2))));
    failures.push_back(record(
        "BACKWARD-SAMPLES-OUTSIDE-DOMAIN",
        gnc::foundation::differentiate_scaled_one_sided(
            0.1, DifferentiationDomain{0.0, 1.0},
            OneSidedDirection::Backward, function,
            policy_for_step(0.1, 1.0, 0.2))));
    failures.push_back(record(
        "POINT-CALLBACK-DOMAIN",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward,
            [](double value) {
                return value == 0.0
                           ? scalar_failure(NumericalStatus::DomainError,
                                            "point-domain")
                           : scalar_value(value);
            },
            unit_policy)));
    failures.push_back(record(
        "NEAREST-CALLBACK-DOMAIN",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward,
            [](double value) {
                return value > 0.0 && value < 0.5
                           ? scalar_failure(NumericalStatus::DomainError,
                                            "nearest-domain")
                           : scalar_value(value);
            },
            unit_policy)));
    failures.push_back(record(
        "FAR-CALLBACK-DOMAIN",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward,
            [](double value) {
                return value >= 0.5
                           ? scalar_failure(NumericalStatus::DomainError,
                                            "far-domain")
                           : scalar_value(value);
            },
            unit_policy)));
    failures.push_back(record(
        "NONFINITE-FUNCTION-VALUE",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward,
            [](double) {
                return scalar_value(
                    std::numeric_limits<double>::infinity());
            },
            unit_policy)));
    failures.push_back(record(
        "NONFINITE-FUNCTION-DIFFERENCE",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward,
            [maximum](double value) {
                return scalar_value(value == 0.0 ? -maximum : maximum);
            },
            unit_policy)));
    failures.push_back(record(
        "NONFINITE-SECANT-SLOPE",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, unit_domain, OneSidedDirection::Forward,
            [maximum](double value) {
                return scalar_value(value == 0.0 ? 0.0 : maximum);
            },
            ScaledOneSidedDifferencePolicy{1.0e-300, 1.0})));
    failures.push_back(record(
        "NONFINITE-DERIVATIVE-COMBINATION",
        gnc::foundation::differentiate_scaled_one_sided(
            0.0, DifferentiationDomain{0.0, 2.0},
            OneSidedDirection::Forward,
            [maximum](double value) {
                if (value == 0.0) {
                    return scalar_value(0.0);
                }
                return scalar_value(value == 1.0 ? 0.5 * maximum
                                                  : -maximum);
            },
            ScaledOneSidedDifferencePolicy{1.0, 1.0})));
    return failures;
}

Bundle run_bundle() {
    Bundle bundle;
    bundle.default_policy = ScaledOneSidedDifferencePolicy{};
    const std::vector<double> altitude_steps{
        800.0, 400.0, 200.0, 100.0, 50.0};
    const std::vector<double> mach_steps{
        0.4, 0.2, 0.1, 0.05, 0.025};
    const DifferentiationDomain altitude_domain{0.0, 60000.0};
    const DifferentiationDomain mach_domain{0.0, 20.0};

    bundle.convergence_ladders.push_back(make_ladder(
        "DENSITY-LOWER-FORWARD", 0.0, altitude_domain,
        OneSidedDirection::Forward, kReferenceAltitudeM,
        analytic_density_gradient(0.0), altitude_steps,
        density_function));
    bundle.convergence_ladders.push_back(make_ladder(
        "DENSITY-UPPER-BACKWARD", 60000.0, altitude_domain,
        OneSidedDirection::Backward, kReferenceAltitudeM,
        analytic_density_gradient(60000.0), altitude_steps,
        density_function));
    bundle.convergence_ladders.push_back(make_ladder(
        "MACH-LOWER-FORWARD", 0.0, altitude_domain,
        OneSidedDirection::Forward, kReferenceAltitudeM,
        analytic_mach_altitude_gradient(0.0), altitude_steps,
        mach_altitude_function));
    bundle.convergence_ladders.push_back(make_ladder(
        "MACH-UPPER-BACKWARD", 60000.0, altitude_domain,
        OneSidedDirection::Backward, kReferenceAltitudeM,
        analytic_mach_altitude_gradient(60000.0), altitude_steps,
        mach_altitude_function));
    bundle.convergence_ladders.push_back(make_ladder(
        "CL-STAR-LOWER-FORWARD", 0.0, mach_domain,
        OneSidedDirection::Forward, 10.0,
        analytic_cl_star_gradient(0.0), mach_steps, cl_star_function));
    bundle.convergence_ladders.push_back(make_ladder(
        "CL-STAR-UPPER-BACKWARD", 20.0, mach_domain,
        OneSidedDirection::Backward, 10.0,
        analytic_cl_star_gradient(20.0), mach_steps, cl_star_function));

    bundle.roundoff_transitions.push_back(make_transition(
        "DENSITY-LOWER-FORWARD", 0.0, altitude_domain,
        OneSidedDirection::Forward, kReferenceAltitudeM,
        analytic_density_gradient(0.0), 0.01, 1.0e-9,
        density_function));
    bundle.roundoff_transitions.push_back(make_transition(
        "DENSITY-UPPER-BACKWARD", 60000.0, altitude_domain,
        OneSidedDirection::Backward, kReferenceAltitudeM,
        analytic_density_gradient(60000.0), 0.1, 1.0e-9,
        density_function));
    bundle.roundoff_transitions.push_back(make_transition(
        "MACH-LOWER-FORWARD", 0.0, altitude_domain,
        OneSidedDirection::Forward, kReferenceAltitudeM,
        analytic_mach_altitude_gradient(0.0), 1.0, 1.0e-8,
        mach_altitude_function));
    bundle.roundoff_transitions.push_back(make_transition(
        "MACH-UPPER-BACKWARD", 60000.0, altitude_domain,
        OneSidedDirection::Backward, kReferenceAltitudeM,
        analytic_mach_altitude_gradient(60000.0), 1.0, 1.0e-8,
        mach_altitude_function));
    bundle.roundoff_transitions.push_back(make_transition(
        "CL-STAR-LOWER-FORWARD", 0.0, mach_domain,
        OneSidedDirection::Forward, 10.0,
        analytic_cl_star_gradient(0.0), 1.0e-4, 1.0e-12,
        cl_star_function));
    bundle.roundoff_transitions.push_back(make_transition(
        "CL-STAR-UPPER-BACKWARD", 20.0, mach_domain,
        OneSidedDirection::Backward, 10.0,
        analytic_cl_star_gradient(20.0), 1.0e-3, 1.0e-12,
        cl_star_function));

    bundle.direction_semantics = direction_semantics();
    bundle.scale_selection = scale_selection_cases();
    bundle.success_semantics = success_semantics();
    bundle.failure_cases = failure_cases();
    return bundle;
}

void self_check(const Bundle& bundle) {
    require(
        gnc::foundation::kScaledOneSidedDifferenceIdentity.id ==
                "gnc.foundation.differentiation.scaled-one-sided-second-order@1" &&
            gnc::foundation::to_string(OneSidedDirection::Forward) ==
                "Forward" &&
            gnc::foundation::to_string(OneSidedDirection::Backward) ==
                "Backward" &&
            gnc::foundation::to_string(
                static_cast<OneSidedDirection>(255U)) == "Invalid" &&
            near(bundle.default_policy.relative_step,
                 gnc::foundation::
                     kDefaultSecondOrderDifferenceRelativeStep,
                 0.0, 0.0),
        "scaled one-sided difference identity differs");

    require(bundle.convergence_ladders.size() == 6U,
            "CAVH boundary ladder count differs");
    for (const LadderObservation& ladder :
         bundle.convergence_ladders) {
        require(ladder.samples.size() == 5U &&
                    ladder.error_reduction_ratios.size() == 4U,
                "CAVH boundary ladder size differs");
        double previous_error = std::numeric_limits<double>::infinity();
        for (const LadderSample& sample : ladder.samples) {
            require(sample.outcome.status == NumericalStatus::Success &&
                        sample.outcome.has_value &&
                        sample.outcome.evaluations == 3U &&
                        sample.outcome.result.has_value(),
                    "CAVH boundary outcome metadata differs");
            const ScaledOneSidedDifferenceResult& result =
                *sample.outcome.result;
            const bool ordered =
                ladder.direction == OneSidedDirection::Forward
                    ? result.point < result.nearest_argument &&
                          result.nearest_argument < result.far_argument
                    : result.far_argument < result.nearest_argument &&
                          result.nearest_argument < result.point;
            require(result.direction == ladder.direction && ordered &&
                        result.nearest_argument >= result.domain.lower &&
                        result.far_argument >= result.domain.lower &&
                        result.nearest_argument <= result.domain.upper &&
                        result.far_argument <= result.domain.upper &&
                        near(result.requested_step, sample.requested_step,
                             2.0e-12, 2.0e-15) &&
                        result.spacing_ratio > 1.5 &&
                        result.spacing_ratio < 2.5,
                    "CAVH boundary sample geometry differs");
            const double error =
                std::abs(result.derivative - ladder.analytic_derivative);
            require(error < previous_error,
                    "CAVH boundary error is not monotone");
            previous_error = error;
        }
        for (double ratio : ladder.error_reduction_ratios) {
            require(ratio > 3.75 && ratio < 4.25,
                    "CAVH one-sided difference order differs");
        }
    }

    require(bundle.roundoff_transitions.size() == 6U,
            "one-sided roundoff transition count differs");
    for (const RoundoffTransition& transition :
         bundle.roundoff_transitions) {
        require(transition.well_scaled.outcome.has_value &&
                    transition.tiny_step.outcome.has_value,
                "one-sided roundoff outcome is missing");
        const auto& well = *transition.well_scaled.outcome.result;
        const auto& tiny = *transition.tiny_step.outcome.result;
        const double well_error =
            std::abs(well.derivative - transition.analytic_derivative);
        const double tiny_error =
            std::abs(tiny.derivative - transition.analytic_derivative);
        require(tiny_error > 1000.0 *
                                 std::max(well_error, 1.0e-30) &&
                    tiny.risk.nearest_output_cancellation_ratio <
                        well.risk.nearest_output_cancellation_ratio,
                "one-sided double roundoff transition differs");
    }

    require(bundle.direction_semantics.size() == 3U &&
                near(bundle.direction_semantics[0U].result->derivative,
                     2.0) &&
                near(bundle.direction_semantics[1U].result->derivative,
                     2.0) &&
                bundle.direction_semantics[0U].result->direction ==
                    OneSidedDirection::Forward &&
                bundle.direction_semantics[1U].result->direction ==
                    OneSidedDirection::Backward &&
                bundle.direction_semantics[2U].evaluations == 3U,
            "explicit one-sided direction semantics differ");

    require(bundle.scale_selection.size() == 4U &&
                near(bundle.scale_selection[0U].result
                         ->selected_argument_scale,
                     20.0) &&
                near(bundle.scale_selection[0U].result->requested_step,
                     2.5) &&
                near(bundle.scale_selection[1U].result
                         ->selected_argument_scale,
                     100.0) &&
                near(bundle.scale_selection[1U].result->requested_step,
                     12.5) &&
                near(bundle.scale_selection[0U].result->derivative, 3.0) &&
                near(bundle.scale_selection[1U].result->derivative, 3.0) &&
                bundle.scale_selection[2U].status ==
                    NumericalStatus::Success &&
                bundle.scale_selection[3U].status ==
                    NumericalStatus::Success &&
                near(bundle.scale_selection[2U].result->requested_step,
                     gnc::foundation::
                         kDefaultSecondOrderDifferenceRelativeStep,
                     1.0e-22, 0.0) &&
                near(bundle.scale_selection[3U].result->requested_step,
                     20.0 * gnc::foundation::
                                kDefaultSecondOrderDifferenceRelativeStep,
                     1.0e-20, 0.0),
            "one-sided scaled step selection differs");

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
                        .nearest_output_cancellation_ratio == 0.0 &&
                bundle.success_semantics[2U].result->risk
                        .far_output_cancellation_ratio == 0.0 &&
                bundle.success_semantics[2U].result->risk
                        .derivative_combination_ratio == 0.0,
            "one-sided success semantics differ");

    const std::array<
        std::tuple<NumericalStatus, std::string_view, std::size_t>, 23U>
        expected{{
            {NumericalStatus::DomainError, "policy", 0U},
            {NumericalStatus::DomainError, "policy", 0U},
            {NumericalStatus::DomainError, "policy", 0U},
            {NumericalStatus::DomainError, "policy", 0U},
            {NumericalStatus::DomainError, "direction", 0U},
            {NumericalStatus::NonFiniteInput, "domain", 0U},
            {NumericalStatus::DomainError, "domain", 0U},
            {NumericalStatus::NonFiniteInput, "point", 0U},
            {NumericalStatus::DomainError, "point-outside-domain", 0U},
            {NumericalStatus::NonFiniteIntermediate, "requested-step", 0U},
            {NumericalStatus::StepUnderflow,
             "requested-step-underflow", 0U},
            {NumericalStatus::NonFiniteIntermediate,
             "sample-arguments", 0U},
            {NumericalStatus::StepUnderflow,
             "unrepresentable-nearest-step", 0U},
            {NumericalStatus::StepUnderflow,
             "unrepresentable-far-step", 0U},
            {NumericalStatus::DomainError,
             "one-sided-samples-outside-domain", 0U},
            {NumericalStatus::DomainError,
             "one-sided-samples-outside-domain", 0U},
            {NumericalStatus::DomainError, "point-domain", 1U},
            {NumericalStatus::DomainError, "nearest-domain", 2U},
            {NumericalStatus::DomainError, "far-domain", 3U},
            {NumericalStatus::NonFiniteIntermediate,
             "function-value", 1U},
            {NumericalStatus::NonFiniteIntermediate,
             "function-difference", 3U},
            {NumericalStatus::NonFiniteIntermediate,
             "secant-slope", 3U},
            {NumericalStatus::NonFiniteIntermediate,
             "derivative-combination", 3U},
        }};
    require(bundle.failure_cases.size() == expected.size(),
            "one-sided difference failure count differs");
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        require(bundle.failure_cases[index].status ==
                    std::get<0>(expected[index]) &&
                    bundle.failure_cases[index].detail ==
                        std::get<1>(expected[index]) &&
                    bundle.failure_cases[index].evaluations ==
                        std::get<2>(expected[index]) &&
                    !bundle.failure_cases[index].has_value,
                "one-sided difference failure semantics differ");
    }
}

void write_string(std::string_view value) {
    std::cout << '"' << value << '"';
}

void write_result(const ScaledOneSidedDifferenceResult& result) {
    std::cout << "{\"derivative\":" << result.derivative
              << ",\"direction\":";
    write_string(gnc::foundation::to_string(result.direction));
    std::cout << ",\"point\":" << result.point
              << ",\"domain\":{\"lower\":" << result.domain.lower
              << ",\"upper\":" << result.domain.upper
              << "},\"nominal_argument_scale\":"
              << result.nominal_argument_scale
              << ",\"selected_argument_scale\":"
              << result.selected_argument_scale
              << ",\"relative_step\":" << result.relative_step
              << ",\"requested_step\":" << result.requested_step
              << ",\"nearest_argument\":" << result.nearest_argument
              << ",\"far_argument\":" << result.far_argument
              << ",\"point_value\":" << result.point_value
              << ",\"nearest_value\":" << result.nearest_value
              << ",\"far_value\":" << result.far_value
              << ",\"nearest_offset\":" << result.nearest_offset
              << ",\"far_offset\":" << result.far_offset
              << ",\"effective_step\":" << result.effective_step
              << ",\"spacing_ratio\":" << result.spacing_ratio
              << ",\"risk\":{\"normalized_step\":"
              << result.risk.normalized_step
              << ",\"nearest_output_cancellation_ratio\":"
              << result.risk.nearest_output_cancellation_ratio
              << ",\"far_output_cancellation_ratio\":"
              << result.risk.far_output_cancellation_ratio
              << ",\"derivative_combination_ratio\":"
              << result.risk.derivative_combination_ratio
              << ",\"spacing_ratio_error\":"
              << result.risk.spacing_ratio_error << "}}";
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

void write_outcome_array(
    const char* name, const std::vector<OutcomeRecord>& outcomes) {
    std::cout << ",\"" << name << "\":[";
    for (std::size_t index = 0U; index < outcomes.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        write_outcome(outcomes[index]);
    }
    std::cout << ']';
}

void write_bundle(const Bundle& bundle) {
    std::cout << std::setprecision(17) << "{\"schema_version\":";
    write_string(kSchema);
    std::cout << ",\"component_id\":";
    write_string(kComponentId);
    std::cout << ",\"fixture_id\":";
    write_string(kFixtureId);
    std::cout << ",\"algorithm\":{\"id\":";
    write_string(gnc::foundation::kScaledOneSidedDifferenceIdentity.id);
    std::cout << ",\"version\":";
    write_string(
        gnc::foundation::kScaledOneSidedDifferenceIdentity.version);
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
        std::cout << ",\"direction\":";
        write_string(gnc::foundation::to_string(ladder.direction));
        std::cout << ",\"point\":" << ladder.point
                  << ",\"domain\":{\"lower\":"
                  << ladder.domain.lower << ",\"upper\":"
                  << ladder.domain.upper
                  << "},\"argument_scale\":" << ladder.argument_scale
                  << ",\"analytic_derivative\":"
                  << ladder.analytic_derivative << ",\"samples\":[";
        for (std::size_t sample = 0U;
             sample < ladder.samples.size(); ++sample) {
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
        std::cout << ",\"direction\":";
        write_string(gnc::foundation::to_string(transition.direction));
        std::cout << ",\"analytic_derivative\":"
                  << transition.analytic_derivative
                  << ",\"well_scaled\":";
        write_sample(transition.well_scaled);
        std::cout << ",\"tiny_step\":";
        write_sample(transition.tiny_step);
        std::cout << '}';
    }
    std::cout << ']';
    write_outcome_array("direction_semantics", bundle.direction_semantics);
    write_outcome_array("scale_selection", bundle.scale_selection);
    write_outcome_array("success_semantics", bundle.success_semantics);
    write_outcome_array("failure_cases", bundle.failure_cases);
    std::cout << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2 || std::string_view(argv[1]) != "--self-check") {
            std::cerr
                << "usage: gnc_foundation_one_sided_differentiation_probe "
                   "--self-check\n";
            return EXIT_FAILURE;
        }
        const Bundle bundle = run_bundle();
        self_check(bundle);
        write_bundle(bundle);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "foundation one-sided differentiation failure: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

#include "gnc/foundation/local_newton_root.hpp"

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
#include <utility>
#include <vector>

namespace {

using gnc::foundation::FiniteCheck;
using gnc::foundation::LocalNewtonRootPolicy;
using gnc::foundation::LocalNewtonRootResult;
using gnc::foundation::LocalRootDomain;
using gnc::foundation::LocalRootLinearization;
using gnc::foundation::LocalRootStopReason;
using gnc::foundation::NumericalEvidence;
using gnc::foundation::NumericalFlag;
using gnc::foundation::NumericalFlags;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;

constexpr std::string_view kSchema =
    "gnczmkn.foundation-local-root-probe/1";
constexpr std::string_view kComponentId =
    "GNC-FOUNDATION-LOCAL-ROOT-001";
constexpr std::string_view kFixtureId = "REF-CAVH-FORMULA-001";
constexpr double kFormulaTolerance = 2.0e-11;

struct Polar {
    std::string_view id;
    double cl_intercept = 0.0;
    double cl_slope_per_rad = 0.0;
    double cd0_base = 0.0;
    double cd0_slope_per_mach = 0.0;
    double induced_drag_factor = 0.0;
    double mach = 0.0;
    double alpha_min_rad = 0.0;
    double alpha_max_rad = 0.0;
};

struct OutcomeRecord {
    std::string id;
    NumericalStatus status = NumericalStatus::InternalFailure;
    NumericalFlags flags = 0U;
    bool has_value = false;
    std::size_t iterations = 0U;
    std::size_t evaluations = 0U;
    std::string detail;
    std::optional<double> residual_norm;
    std::optional<double> last_step;
    std::optional<LocalNewtonRootResult> result;
};

struct BasinObservation {
    std::string polar_case_id;
    double initial_guess = 0.0;
    OutcomeRecord outcome;
};

struct TraceSample {
    double argument = 0.0;
    double function_value = 0.0;
    double derivative_value = 0.0;
    double absolute_root_error = 0.0;
};

struct TraceObservation {
    std::string polar_case_id;
    double initial_guess = 0.0;
    double minimum_asymptotic_order = 0.0;
    std::vector<TraceSample> samples;
    OutcomeRecord outcome;
};

struct Bundle {
    LocalNewtonRootPolicy policy;
    std::vector<BasinObservation> basin;
    std::vector<TraceObservation> traces;
    std::vector<OutcomeRecord> success_semantics;
    std::vector<OutcomeRecord> failures;
};

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

Polar constant_polar() {
    return {"CASE-CAVH-ENVELOPE-CONSTANT-POLAR", 0.0, 2.0, 0.02,
            0.0, 0.08, 10.0, 0.0, 0.5};
}

Polar mach_dependent_polar() {
    return {"CASE-CAVH-ENVELOPE-MACH-DEPENDENT", 0.0, 2.0, 0.02,
            0.001, 0.08, 10.0, 0.0, 0.5};
}

bool finite_polar(const Polar& polar) {
    return std::isfinite(polar.cl_intercept) &&
           std::isfinite(polar.cl_slope_per_rad) &&
           std::isfinite(polar.cd0_base) &&
           std::isfinite(polar.cd0_slope_per_mach) &&
           std::isfinite(polar.induced_drag_factor) &&
           std::isfinite(polar.mach) &&
           std::isfinite(polar.alpha_min_rad) &&
           std::isfinite(polar.alpha_max_rad);
}

double analytic_alpha_star(const Polar& polar) {
    const double cd0 =
        polar.cd0_base + polar.cd0_slope_per_mach * polar.mach;
    return (std::sqrt(cd0 / polar.induced_drag_factor) -
            polar.cl_intercept) /
           polar.cl_slope_per_rad;
}

NumericalEvidence polar_evidence() {
    NumericalEvidence evidence;
    evidence.algorithm = {
        "fixture.cavh.parabolic-polar-ld-derivative-linearization@1",
        "1.0.0"};
    return evidence;
}

NumericalOutcome<LocalRootLinearization> polar_linearization(
    const Polar& polar, double alpha_rad) {
    NumericalEvidence evidence = polar_evidence();
    if (!finite_polar(polar) || !std::isfinite(alpha_rad)) {
        evidence.detail = "polar-nonfinite";
        return NumericalOutcome<LocalRootLinearization>::failure(
            NumericalStatus::NonFiniteInput, evidence);
    }
    const double cd0 =
        polar.cd0_base + polar.cd0_slope_per_mach * polar.mach;
    if (cd0 <= 0.0 || polar.induced_drag_factor <= 0.0 ||
        polar.cl_slope_per_rad <= 0.0 ||
        !(polar.alpha_min_rad < polar.alpha_max_rad) ||
        alpha_rad < polar.alpha_min_rad ||
        alpha_rad > polar.alpha_max_rad) {
        evidence.detail = "polar-domain";
        return NumericalOutcome<LocalRootLinearization>::failure(
            NumericalStatus::DomainError, evidence);
    }

    const double lift = polar.cl_intercept +
                        polar.cl_slope_per_rad * alpha_rad;
    const double lift_squared = lift * lift;
    const double drag = cd0 + polar.induced_drag_factor * lift_squared;
    const double function_value =
        polar.cl_slope_per_rad *
        (cd0 - polar.induced_drag_factor * lift_squared) /
        (drag * drag);
    const double derivative_value =
        -2.0 * polar.cl_slope_per_rad * polar.cl_slope_per_rad *
        polar.induced_drag_factor * lift *
        (3.0 * cd0 - polar.induced_drag_factor * lift_squared) /
        (drag * drag * drag);
    if (!std::isfinite(function_value) ||
        !std::isfinite(derivative_value)) {
        evidence.detail = "polar-linearization";
        return NumericalOutcome<LocalRootLinearization>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    return NumericalOutcome<LocalRootLinearization>::with_value(
        NumericalStatus::Success,
        LocalRootLinearization{function_value, derivative_value},
        evidence);
}

LocalNewtonRootPolicy accepted_policy() {
    return {
        NumericalPolicy{1.0e-14, 1.0e-13, FiniteCheck::EveryStage},
        1.0e-12,
        1.0e-14,
        20U};
}

OutcomeRecord record(
    std::string id,
    const NumericalOutcome<LocalNewtonRootResult>& outcome) {
    OutcomeRecord result;
    result.id = std::move(id);
    result.status = outcome.status();
    result.flags = outcome.evidence().flags;
    result.has_value = outcome.has_value();
    result.iterations = outcome.evidence().iterations;
    result.evaluations = outcome.evidence().evaluations;
    result.detail = std::string(outcome.evidence().detail);
    result.residual_norm = outcome.evidence().residual_norm;
    result.last_step = outcome.evidence().last_step;
    if (outcome.has_value()) {
        result.result = outcome.value();
    }
    return result;
}

OutcomeRecord solve_polar(const Polar& polar, double initial_guess,
                          const LocalNewtonRootPolicy& policy) {
    const LocalRootDomain domain{
        polar.alpha_min_rad, polar.alpha_max_rad};
    return record(
        std::string(polar.id) + "@" + std::to_string(initial_guess),
        gnc::foundation::solve_local_root_newton(
            initial_guess, domain,
            [polar](double alpha_rad) {
                return polar_linearization(polar, alpha_rad);
            },
            policy));
}

NumericalOutcome<LocalRootLinearization> scalar_linearization(
    double function_value, double derivative_value,
    NumericalStatus status = NumericalStatus::Success,
    NumericalFlags flags = 0U) {
    NumericalEvidence evidence;
    evidence.flags = flags;
    evidence.algorithm = {"fixture.scalar-linearization@1", "1.0.0"};
    return NumericalOutcome<LocalRootLinearization>::with_value(
        status, LocalRootLinearization{function_value, derivative_value},
        evidence);
}

TraceObservation trace_polar(const Polar& polar, double initial_guess,
                             const LocalNewtonRootPolicy& policy) {
    TraceObservation trace;
    trace.polar_case_id = std::string(polar.id);
    trace.initial_guess = initial_guess;
    const double analytic = analytic_alpha_star(polar);
    const auto outcome = gnc::foundation::solve_local_root_newton(
        initial_guess,
        LocalRootDomain{polar.alpha_min_rad, polar.alpha_max_rad},
        [&](double alpha_rad) {
            const auto linearization = polar_linearization(polar, alpha_rad);
            if (linearization.has_value()) {
                trace.samples.push_back(
                    {alpha_rad,
                     linearization.value().function_value,
                     linearization.value().derivative_value,
                     std::abs(alpha_rad - analytic)});
            }
            return linearization;
        },
        policy);
    trace.outcome = record(std::string(polar.id) + "@trace", outcome);

    double minimum_order = std::numeric_limits<double>::infinity();
    for (std::size_t index = 3U; index < trace.samples.size(); ++index) {
        const double previous =
            trace.samples[index - 2U].absolute_root_error;
        const double current =
            trace.samples[index - 1U].absolute_root_error;
        const double next = trace.samples[index].absolute_root_error;
        if (previous > 0.0 && current > 0.0 && next > 1.0e-14 &&
            current < previous && next < current) {
            const double order = std::log(next / current) /
                                 std::log(current / previous);
            if (std::isfinite(order)) {
                minimum_order = std::min(minimum_order, order);
            }
        }
    }
    require(std::isfinite(minimum_order),
            "CAVH trace has no asymptotic order sample");
    trace.minimum_asymptotic_order = minimum_order;
    return trace;
}

std::vector<OutcomeRecord> success_semantics(
    const LocalNewtonRootPolicy& base_policy) {
    std::vector<OutcomeRecord> results;
    results.push_back(record(
        "EXACT-INITIAL",
        gnc::foundation::solve_local_root_newton(
            2.0, LocalRootDomain{0.0, 4.0},
            [](double value) {
                return scalar_linearization(value - 2.0, 1.0);
            },
            base_policy)));

    LocalNewtonRootPolicy step_policy = base_policy;
    step_policy.argument_tolerance.absolute_tolerance = 1.0e-3;
    step_policy.argument_tolerance.relative_tolerance = 0.0;
    step_policy.residual_absolute_tolerance = 0.0;
    results.push_back(record(
        "STEP-TOLERANCE",
        gnc::foundation::solve_local_root_newton(
            1.4, LocalRootDomain{0.0, 2.0},
            [](double value) {
                return scalar_linearization(value * value - 2.0,
                                            2.0 * value);
            },
            step_policy)));

    results.push_back(record(
        "APPROXIMATE-FLAG-PROPAGATION",
        gnc::foundation::solve_local_root_newton(
            1.0, LocalRootDomain{0.0, 4.0},
            [](double value) {
                return scalar_linearization(
                    value - 2.0, 1.0, NumericalStatus::Approximate,
                    gnc::foundation::numerical_flag(
                        NumericalFlag::Clamped));
            },
            base_policy)));
    return results;
}

std::vector<OutcomeRecord> failure_cases(
    const LocalNewtonRootPolicy& base_policy) {
    std::vector<OutcomeRecord> failures;

    LocalNewtonRootPolicy invalid_policy = base_policy;
    invalid_policy.max_iterations = 0U;
    failures.push_back(record(
        "INVALID-POLICY",
        gnc::foundation::solve_local_root_newton(
            0.0, LocalRootDomain{-1.0, 1.0},
            [](double value) {
                return scalar_linearization(value, 1.0);
            },
            invalid_policy)));
    failures.push_back(record(
        "NONFINITE-DOMAIN",
        gnc::foundation::solve_local_root_newton(
            0.0,
            LocalRootDomain{-1.0,
                            std::numeric_limits<double>::infinity()},
            [](double value) {
                return scalar_linearization(value, 1.0);
            },
            base_policy)));
    failures.push_back(record(
        "INVALID-DOMAIN",
        gnc::foundation::solve_local_root_newton(
            0.0, LocalRootDomain{1.0, -1.0},
            [](double value) {
                return scalar_linearization(value, 1.0);
            },
            base_policy)));
    failures.push_back(record(
        "NONFINITE-INITIAL",
        gnc::foundation::solve_local_root_newton(
            std::numeric_limits<double>::quiet_NaN(),
            LocalRootDomain{-1.0, 1.0},
            [](double value) {
                return scalar_linearization(value, 1.0);
            },
            base_policy)));
    failures.push_back(record(
        "INITIAL-OUTSIDE-DOMAIN",
        gnc::foundation::solve_local_root_newton(
            2.0, LocalRootDomain{-1.0, 1.0},
            [](double value) {
                return scalar_linearization(value, 1.0);
            },
            base_policy)));
    failures.push_back(record(
        "DERIVATIVE-DEGENERATE",
        gnc::foundation::solve_local_root_newton(
            0.0, LocalRootDomain{-1.0, 1.0},
            [](double) { return scalar_linearization(1.0, 0.0); },
            base_policy)));
    failures.push_back(record(
        "NONFINITE-FUNCTION-VALUE",
        gnc::foundation::solve_local_root_newton(
            0.0, LocalRootDomain{-1.0, 1.0},
            [](double) {
                return scalar_linearization(
                    std::numeric_limits<double>::infinity(), 1.0);
            },
            base_policy)));
    failures.push_back(record(
        "NONFINITE-DERIVATIVE",
        gnc::foundation::solve_local_root_newton(
            0.0, LocalRootDomain{-1.0, 1.0},
            [](double) {
                return scalar_linearization(
                    1.0, std::numeric_limits<double>::quiet_NaN());
            },
            base_policy)));
    failures.push_back(record(
        "CALLBACK-DOMAIN",
        gnc::foundation::solve_local_root_newton(
            0.0, LocalRootDomain{-1.0, 1.0},
            [](double) {
                NumericalEvidence evidence;
                evidence.algorithm = {
                    "fixture.scalar-linearization@1", "1.0.0"};
                evidence.detail = "polar-domain";
                return NumericalOutcome<LocalRootLinearization>::failure(
                    NumericalStatus::DomainError, evidence);
            },
            base_policy)));

    failures.push_back(record(
        "CAVH-STEP-OUTSIDE-DOMAIN",
        gnc::foundation::solve_local_root_newton(
            0.4, LocalRootDomain{0.0, 0.5},
            [](double alpha_rad) {
                return polar_linearization(constant_polar(), alpha_rad);
            },
            base_policy)));

    LocalNewtonRootPolicy exact_policy = base_policy;
    exact_policy.argument_tolerance.absolute_tolerance = 0.0;
    exact_policy.argument_tolerance.relative_tolerance = 0.0;
    exact_policy.residual_absolute_tolerance = 0.0;
    exact_policy.derivative_minimum_absolute = 0.0;
    failures.push_back(record(
        "TOLERANCE-UNREACHABLE",
        gnc::foundation::solve_local_root_newton(
            1.0, LocalRootDomain{0.0, 2.0},
            [](double) {
                return scalar_linearization(
                    1.0, std::numeric_limits<double>::max());
            },
            exact_policy)));

    failures.push_back(record(
        "CALLBACK-CANDIDATE-DOMAIN",
        gnc::foundation::solve_local_root_newton(
            0.0, LocalRootDomain{-1.0, 1.0},
            [](double value) {
                if (value == 0.5) {
                    NumericalEvidence evidence;
                    evidence.algorithm = {
                        "fixture.scalar-linearization@1", "1.0.0"};
                    evidence.detail = "candidate-domain";
                    return NumericalOutcome<LocalRootLinearization>::failure(
                        NumericalStatus::DomainError, evidence);
                }
                return scalar_linearization(value - 0.5, 1.0);
            },
            base_policy)));

    LocalNewtonRootPolicy iteration_policy = exact_policy;
    iteration_policy.max_iterations = 2U;
    failures.push_back(record(
        "MAX-ITERATIONS",
        gnc::foundation::solve_local_root_newton(
            0.05, LocalRootDomain{0.0, 0.5},
            [](double alpha_rad) {
                return polar_linearization(constant_polar(), alpha_rad);
            },
            iteration_policy)));

    failures.push_back(record(
        "NONFINITE-NEWTON-STEP",
        gnc::foundation::solve_local_root_newton(
            0.0, LocalRootDomain{-1.0, 1.0},
            [](double) {
                return scalar_linearization(
                    std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::min());
            },
            exact_policy)));

    failures.push_back(record(
        "NONFINITE-CANDIDATE",
        gnc::foundation::solve_local_root_newton(
            0.5 * std::numeric_limits<double>::max(),
            LocalRootDomain{-std::numeric_limits<double>::max(),
                            std::numeric_limits<double>::max()},
            [](double) {
                return scalar_linearization(
                    -std::numeric_limits<double>::max(), 1.0);
            },
            exact_policy)));
    return failures;
}

bool near(double actual, double expected,
          double tolerance = kFormulaTolerance) {
    return std::isfinite(actual) && std::isfinite(expected) &&
           std::abs(actual - expected) <=
               tolerance *
                   std::max({1.0, std::abs(actual), std::abs(expected)});
}

Bundle run_bundle() {
    Bundle bundle;
    bundle.policy = accepted_policy();
    const std::array<Polar, 2U> polars{
        constant_polar(), mach_dependent_polar()};
    for (const Polar& polar : polars) {
        for (int index = 1; index < 20; ++index) {
            const double initial_guess = 0.025 * index;
            bundle.basin.push_back(
                {std::string(polar.id), initial_guess,
                 solve_polar(polar, initial_guess, bundle.policy)});
        }
        bundle.traces.push_back(trace_polar(polar, 0.1, bundle.policy));
    }
    bundle.success_semantics = success_semantics(bundle.policy);
    bundle.failures = failure_cases(bundle.policy);
    return bundle;
}

void self_check(const Bundle& bundle) {
    require(gnc::foundation::kLocalNewtonRootIdentity.id ==
                "gnc.foundation.root.local-newton@1",
            "local Newton algorithm identity differs");
    require(bundle.basin.size() == 38U,
            "CAVH initial basin sample count differs");
    std::size_t converged = 0U;
    std::size_t outside_domain = 0U;
    std::size_t maximum_iterations = 0U;
    for (const BasinObservation& observation : bundle.basin) {
        const Polar polar = observation.polar_case_id ==
                                    constant_polar().id
                                ? constant_polar()
                                : mach_dependent_polar();
        const double analytic = analytic_alpha_star(polar);
        const OutcomeRecord& outcome = observation.outcome;
        if (outcome.has_value) {
            ++converged;
            maximum_iterations =
                std::max(maximum_iterations, outcome.iterations);
            require(outcome.status == NumericalStatus::Converged &&
                        outcome.result.has_value() &&
                        near(outcome.result->root, analytic) &&
                        outcome.evaluations == outcome.iterations + 1U &&
                        outcome.result->domain.lower ==
                            polar.alpha_min_rad &&
                        outcome.result->domain.upper ==
                            polar.alpha_max_rad,
                    "CAVH local Newton success differs");
        } else {
            ++outside_domain;
            require(outcome.status == NumericalStatus::DomainError &&
                        outcome.detail ==
                            "newton-step-outside-domain" &&
                        outcome.residual_norm.has_value() &&
                        outcome.last_step.has_value(),
                    "CAVH local Newton basin failure differs");
        }
    }
    require(converged == 29U && outside_domain == 9U &&
                maximum_iterations == 7U,
            "CAVH local Newton basin summary differs");

    const auto constant_sensitive = std::find_if(
        bundle.basin.begin(), bundle.basin.end(),
        [](const BasinObservation& value) {
            return value.polar_case_id == constant_polar().id &&
                   near(value.initial_guess, 0.4, 1.0e-15);
        });
    const auto mach_sensitive = std::find_if(
        bundle.basin.begin(), bundle.basin.end(),
        [](const BasinObservation& value) {
            return value.polar_case_id == mach_dependent_polar().id &&
                   near(value.initial_guess, 0.4, 1.0e-15);
        });
    require(constant_sensitive != bundle.basin.end() &&
                mach_sensitive != bundle.basin.end() &&
                !constant_sensitive->outcome.has_value &&
                mach_sensitive->outcome.has_value,
            "CAVH model-dependent initial sensitivity is missing");

    require(bundle.traces.size() == 2U,
            "CAVH Newton trace count differs");
    for (const TraceObservation& trace : bundle.traces) {
        require(trace.outcome.has_value &&
                    trace.outcome.status == NumericalStatus::Converged &&
                    trace.samples.size() == trace.outcome.evaluations &&
                    trace.minimum_asymptotic_order >= 1.8,
                "CAVH Newton convergence trace differs");
        for (std::size_t index = 1U; index < trace.samples.size(); ++index) {
            require(trace.samples[index].absolute_root_error <
                        trace.samples[index - 1U].absolute_root_error,
                    "CAVH Newton error is not monotone");
        }
    }

    require(bundle.success_semantics.size() == 3U,
            "local Newton success semantics count differs");
    require(bundle.success_semantics[0U].status ==
                NumericalStatus::Converged &&
                bundle.success_semantics[0U].result.has_value() &&
                bundle.success_semantics[0U].result->stop_reason ==
                    LocalRootStopReason::ExactInitialGuess &&
                bundle.success_semantics[0U].iterations == 0U &&
                bundle.success_semantics[0U].evaluations == 1U,
            "exact initial local root differs");
    require(bundle.success_semantics[1U].status ==
                NumericalStatus::Converged &&
                bundle.success_semantics[1U].result.has_value() &&
                bundle.success_semantics[1U].result->stop_reason ==
                    LocalRootStopReason::StepTolerance,
            "local Newton step tolerance differs");
    require(bundle.success_semantics[2U].status ==
                NumericalStatus::Approximate &&
                bundle.success_semantics[2U].result.has_value() &&
                gnc::foundation::has_numerical_flag(
                    bundle.success_semantics[2U].flags,
                    NumericalFlag::Clamped),
            "local Newton callback flags did not propagate");

    const std::array<std::pair<NumericalStatus, std::string_view>, 15U>
        expected{{
            {NumericalStatus::DomainError, "policy"},
            {NumericalStatus::NonFiniteInput, "domain"},
            {NumericalStatus::DomainError, "domain"},
            {NumericalStatus::NonFiniteInput, "initial-guess"},
            {NumericalStatus::DomainError,
             "initial-guess-outside-domain"},
            {NumericalStatus::Singular, "derivative-below-threshold"},
            {NumericalStatus::NonFiniteIntermediate, "function-value"},
            {NumericalStatus::NonFiniteIntermediate,
             "function-derivative"},
            {NumericalStatus::DomainError, "polar-domain"},
            {NumericalStatus::DomainError,
             "newton-step-outside-domain"},
            {NumericalStatus::ToleranceUnreachable,
             "no-representable-newton-step"},
            {NumericalStatus::DomainError, "candidate-domain"},
            {NumericalStatus::MaxIterations, "max-iterations"},
            {NumericalStatus::NonFiniteIntermediate, "newton-step"},
            {NumericalStatus::NonFiniteIntermediate,
             "newton-candidate"},
        }};
    require(bundle.failures.size() == expected.size(),
            "local Newton failure count differs");
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        require(bundle.failures[index].status == expected[index].first &&
                    bundle.failures[index].detail == expected[index].second &&
                    !bundle.failures[index].has_value,
                "local Newton failure semantics differ");
    }
    require(bundle.failures[5U].evaluations == 1U &&
                bundle.failures[5U].iterations == 0U &&
                bundle.failures[11U].evaluations == 2U &&
                bundle.failures[11U].iterations == 1U &&
                bundle.failures[12U].evaluations == 3U &&
                bundle.failures[12U].iterations == 2U,
            "local Newton failure evidence differs");
}

void write_string(std::string_view value) {
    std::cout << '"' << value << '"';
}

void write_optional(std::optional<double> value) {
    if (value.has_value()) {
        std::cout << *value;
    } else {
        std::cout << "null";
    }
}

void write_outcome(const OutcomeRecord& outcome) {
    std::cout << "{\"id\":";
    write_string(outcome.id);
    std::cout << ",\"status\":";
    write_string(gnc::foundation::to_string(outcome.status));
    std::cout << ",\"flags\":" << outcome.flags
              << ",\"has_value\":"
              << (outcome.has_value ? "true" : "false")
              << ",\"iterations\":" << outcome.iterations
              << ",\"evaluations\":" << outcome.evaluations
              << ",\"detail\":";
    write_string(outcome.detail);
    std::cout << ",\"residual_norm\":";
    write_optional(outcome.residual_norm);
    std::cout << ",\"last_step\":";
    write_optional(outcome.last_step);
    std::cout << ",\"result\":";
    if (outcome.result.has_value()) {
        const LocalNewtonRootResult& result = *outcome.result;
        std::cout << "{\"initial_guess\":" << result.initial_guess
                  << ",\"root\":" << result.root
                  << ",\"function_value\":" << result.function_value
                  << ",\"derivative_value\":"
                  << result.derivative_value
                  << ",\"last_step\":" << result.last_step
                  << ",\"domain\":{\"lower\":"
                  << result.domain.lower << ",\"upper\":"
                  << result.domain.upper << "},\"stop_reason\":";
        write_string(gnc::foundation::to_string(result.stop_reason));
        std::cout << '}';
    } else {
        std::cout << "null";
    }
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
    write_string(gnc::foundation::kLocalNewtonRootIdentity.id);
    std::cout << ",\"version\":";
    write_string(gnc::foundation::kLocalNewtonRootIdentity.version);
    std::cout << "},\"policy\":{"
              << "\"argument_absolute_tolerance\":"
              << bundle.policy.argument_tolerance.absolute_tolerance
              << ",\"argument_relative_tolerance\":"
              << bundle.policy.argument_tolerance.relative_tolerance
              << ",\"residual_absolute_tolerance\":"
              << bundle.policy.residual_absolute_tolerance
              << ",\"derivative_minimum_absolute\":"
              << bundle.policy.derivative_minimum_absolute
              << ",\"max_iterations\":"
              << bundle.policy.max_iterations << "},\"basin_samples\":[";
    for (std::size_t index = 0U; index < bundle.basin.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        const BasinObservation& observation = bundle.basin[index];
        std::cout << "{\"polar_case_id\":";
        write_string(observation.polar_case_id);
        std::cout << ",\"initial_guess\":"
                  << observation.initial_guess << ",\"outcome\":";
        write_outcome(observation.outcome);
        std::cout << '}';
    }
    std::cout << "],\"convergence_traces\":[";
    for (std::size_t index = 0U; index < bundle.traces.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        const TraceObservation& trace = bundle.traces[index];
        std::cout << "{\"polar_case_id\":";
        write_string(trace.polar_case_id);
        std::cout << ",\"initial_guess\":" << trace.initial_guess
                  << ",\"minimum_asymptotic_order\":"
                  << trace.minimum_asymptotic_order
                  << ",\"samples\":[";
        for (std::size_t sample = 0U; sample < trace.samples.size();
             ++sample) {
            if (sample != 0U) {
                std::cout << ',';
            }
            const TraceSample& value = trace.samples[sample];
            std::cout << "{\"argument\":" << value.argument
                      << ",\"function_value\":"
                      << value.function_value
                      << ",\"derivative_value\":"
                      << value.derivative_value
                      << ",\"absolute_root_error\":"
                      << value.absolute_root_error << '}';
        }
        std::cout << "],\"outcome\":";
        write_outcome(trace.outcome);
        std::cout << '}';
    }
    std::cout << "],\"success_semantics\":[";
    for (std::size_t index = 0U;
         index < bundle.success_semantics.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        write_outcome(bundle.success_semantics[index]);
    }
    std::cout << "],\"failure_cases\":[";
    for (std::size_t index = 0U; index < bundle.failures.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        write_outcome(bundle.failures[index]);
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2 || std::string_view(argv[1]) != "--self-check") {
            std::cerr << "usage: gnc_foundation_local_root_probe --self-check\n";
            return EXIT_FAILURE;
        }
        const Bundle bundle = run_bundle();
        self_check(bundle);
        write_bundle(bundle);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "foundation local Newton self-check failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

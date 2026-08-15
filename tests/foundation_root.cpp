#include "gnc/foundation/bracketed_root.hpp"

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
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using gnc::foundation::BracketedRootPolicy;
using gnc::foundation::BracketedRootResult;
using gnc::foundation::NumericalEvidence;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;
using gnc::foundation::RootStopReason;

constexpr std::string_view kSchema = "gnczmkn.foundation-root-probe/1";
constexpr std::string_view kComponentId = "GNC-FOUNDATION-ROOT-001";
constexpr std::string_view kFixtureId = "REF-CAVH-FORMULA-001";
constexpr double kFormulaTolerance = 2.0e-11;

struct Polar {
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
    bool has_value = false;
    std::size_t iterations = 0U;
    std::size_t evaluations = 0U;
    std::string detail;
    std::optional<double> residual_norm;
    std::optional<double> last_bracket_lower;
    std::optional<double> last_bracket_upper;
    std::optional<BracketedRootResult> result;
};

struct ConvergenceRecord {
    std::size_t max_iterations = 0U;
    OutcomeRecord outcome;
    double bracket_width_rad = 0.0;
    double midpoint_error_rad = 0.0;
};

struct Bundle {
    std::vector<OutcomeRecord> cavh_cases;
    OutcomeRecord endpoint_case;
    OutcomeRecord extreme_bracket_case;
    std::vector<ConvergenceRecord> convergence;
    std::vector<OutcomeRecord> failures;
};

Polar constantPolar() {
    return {0.0, 2.0, 0.02, 0.0, 0.08, 10.0, 0.0, 0.5};
}

Polar machDependentPolar() {
    return {0.0, 2.0, 0.02, 0.001, 0.08, 10.0, 0.0, 0.5};
}

bool finitePolar(const Polar& polar) {
    return std::isfinite(polar.cl_intercept) &&
           std::isfinite(polar.cl_slope_per_rad) &&
           std::isfinite(polar.cd0_base) &&
           std::isfinite(polar.cd0_slope_per_mach) &&
           std::isfinite(polar.induced_drag_factor) &&
           std::isfinite(polar.mach) &&
           std::isfinite(polar.alpha_min_rad) &&
           std::isfinite(polar.alpha_max_rad);
}

NumericalEvidence polarEvidence() {
    NumericalEvidence evidence;
    evidence.algorithm = {
        "fixture.cavh.parabolic-polar-ld-derivative@1", "1.0.0"};
    return evidence;
}

NumericalOutcome<double> liftToDragDerivative(const Polar& polar,
                                               double alpha_rad) {
    NumericalEvidence evidence = polarEvidence();
    if (!finitePolar(polar) || !std::isfinite(alpha_rad)) {
        evidence.detail = "polar-nonfinite";
        return NumericalOutcome<double>::failure(
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
        return NumericalOutcome<double>::failure(
            NumericalStatus::DomainError, evidence);
    }

    const double lift_coefficient =
        polar.cl_intercept + polar.cl_slope_per_rad * alpha_rad;
    const double drag_coefficient =
        cd0 + polar.induced_drag_factor * lift_coefficient *
                  lift_coefficient;
    const double numerator =
        polar.cl_slope_per_rad *
        (cd0 - polar.induced_drag_factor * lift_coefficient *
                   lift_coefficient);
    const double derivative =
        numerator / (drag_coefficient * drag_coefficient);
    if (!std::isfinite(derivative)) {
        evidence.detail = "polar-derivative";
        return NumericalOutcome<double>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    return NumericalOutcome<double>::with_value(
        NumericalStatus::Success, derivative, evidence);
}

double analyticAlphaStar(const Polar& polar) {
    const double cd0 =
        polar.cd0_base + polar.cd0_slope_per_mach * polar.mach;
    return (std::sqrt(cd0 / polar.induced_drag_factor) -
            polar.cl_intercept) /
           polar.cl_slope_per_rad;
}

BracketedRootPolicy acceptedPolicy() {
    return {NumericalPolicy{1.0e-14, 1.0e-13,
                            gnc::foundation::FiniteCheck::EveryStage},
            1.0e-12, 80U};
}

template <typename Value>
OutcomeRecord record(std::string id,
                     const NumericalOutcome<Value>& outcome) {
    OutcomeRecord result;
    result.id = std::move(id);
    result.status = outcome.status();
    result.has_value = outcome.has_value();
    result.iterations = outcome.evidence().iterations;
    result.evaluations = outcome.evidence().evaluations;
    result.detail = std::string{outcome.evidence().detail};
    result.residual_norm = outcome.evidence().residual_norm;
    result.last_bracket_lower = outcome.evidence().last_bracket_lower;
    result.last_bracket_upper = outcome.evidence().last_bracket_upper;
    if constexpr (std::is_same<Value, BracketedRootResult>::value) {
        if (outcome.has_value()) {
            result.result = outcome.value();
        }
    }
    return result;
}

OutcomeRecord solvePolarCase(std::string id, const Polar& polar,
                             const BracketedRootPolicy& policy) {
    const auto derivative = [polar](double alpha_rad) {
        return liftToDragDerivative(polar, alpha_rad);
    };
    return record(std::move(id),
                  gnc::foundation::solve_bracketed_root_bisection(
                      polar.alpha_min_rad, polar.alpha_max_rad, derivative,
                      policy));
}

std::vector<ConvergenceRecord> convergenceLadder(const Polar& polar) {
    const std::array<std::size_t, 8U> budgets{4U, 8U, 12U, 16U,
                                              20U, 24U, 28U, 32U};
    const double analytic = analyticAlphaStar(polar);
    std::vector<ConvergenceRecord> records;
    for (const std::size_t budget : budgets) {
        BracketedRootPolicy policy{
            NumericalPolicy{0.0, 0.0,
                            gnc::foundation::FiniteCheck::EveryStage},
            0.0, budget};
        OutcomeRecord outcome = solvePolarCase(
            "CAVH-MACH-DEPENDENT-BUDGET-" + std::to_string(budget), polar,
            policy);
        if (!outcome.last_bracket_lower.has_value() ||
            !outcome.last_bracket_upper.has_value()) {
            throw std::runtime_error(
                "convergence failure did not retain a bracket");
        }
        const double lower = *outcome.last_bracket_lower;
        const double upper = *outcome.last_bracket_upper;
        const double midpoint = lower + 0.5 * (upper - lower);
        records.push_back(
            {budget, std::move(outcome), upper - lower,
             std::abs(midpoint - analytic)});
    }
    return records;
}

NumericalOutcome<double> scalarValue(double value) {
    NumericalEvidence evidence;
    evidence.algorithm = {"fixture.scalar-function@1", "1.0.0"};
    return NumericalOutcome<double>::with_value(
        NumericalStatus::Success, value, evidence);
}

std::vector<OutcomeRecord> failureCases() {
    std::vector<OutcomeRecord> failures;
    const BracketedRootPolicy policy = acceptedPolicy();

    failures.push_back(record(
        "NO-BRACKET",
        gnc::foundation::solve_bracketed_root_bisection(
            -1.0, 1.0,
            [](double value) { return scalarValue(value * value + 1.0); },
            policy)));
    failures.push_back(record(
        "INVALID-BRACKET",
        gnc::foundation::solve_bracketed_root_bisection(
            1.0, -1.0,
            [](double value) { return scalarValue(value); }, policy)));
    failures.push_back(record(
        "NONFINITE-BOUND",
        gnc::foundation::solve_bracketed_root_bisection(
            -std::numeric_limits<double>::infinity(), 1.0,
            [](double value) { return scalarValue(value); }, policy)));
    failures.push_back(record(
        "NONFINITE-MIDPOINT",
        gnc::foundation::solve_bracketed_root_bisection(
            0.0, 1.0,
            [](double value) {
                if (value == 0.5) {
                    return scalarValue(
                        std::numeric_limits<double>::infinity());
                }
                return scalarValue(value - 0.25);
            },
            policy)));
    failures.push_back(record(
        "CALLBACK-DOMAIN",
        gnc::foundation::solve_bracketed_root_bisection(
            0.0, 1.0,
            [](double value) {
                if (value == 0.5) {
                    NumericalEvidence evidence;
                    evidence.algorithm = {
                        "fixture.scalar-function@1", "1.0.0"};
                    evidence.detail = "polar-domain";
                    return NumericalOutcome<double>::failure(
                        NumericalStatus::DomainError, evidence);
                }
                return scalarValue(value - 0.25);
            },
            policy)));

    const double adjacent_lower = 1.0;
    const double adjacent_upper = std::nextafter(1.0, 2.0);
    const BracketedRootPolicy exact_policy{
        NumericalPolicy{0.0, 0.0,
                        gnc::foundation::FiniteCheck::EveryStage},
        0.0, 10U};
    failures.push_back(record(
        "TOLERANCE-UNREACHABLE",
        gnc::foundation::solve_bracketed_root_bisection(
            adjacent_lower, adjacent_upper,
            [adjacent_lower](double value) {
                return scalarValue(value == adjacent_lower ? -1.0 : 1.0);
            },
            exact_policy)));

    BracketedRootPolicy invalid_policy = acceptedPolicy();
    invalid_policy.max_iterations = 0U;
    failures.push_back(record(
        "INVALID-POLICY",
        gnc::foundation::solve_bracketed_root_bisection(
            -1.0, 1.0,
            [](double value) { return scalarValue(value); },
            invalid_policy)));
    return failures;
}

Bundle runBundle() {
    const Polar constant = constantPolar();
    const Polar mach_dependent = machDependentPolar();
    const BracketedRootPolicy policy = acceptedPolicy();
    Bundle bundle;
    bundle.cavh_cases = {
        solvePolarCase("CASE-CAVH-ENVELOPE-CONSTANT-POLAR", constant,
                       policy),
        solvePolarCase("CASE-CAVH-ENVELOPE-MACH-DEPENDENT",
                       mach_dependent, policy),
    };
    bundle.endpoint_case = record(
        "EXACT-LOWER-ENDPOINT",
        gnc::foundation::solve_bracketed_root_bisection(
            0.0, 1.0,
            [](double value) { return scalarValue(value); }, policy));
    bundle.extreme_bracket_case = record(
        "EXTREME-FINITE-BRACKET",
        gnc::foundation::solve_bracketed_root_bisection(
            -std::numeric_limits<double>::max(),
            std::numeric_limits<double>::max(),
            [](double value) { return scalarValue(value); }, policy));
    bundle.convergence = convergenceLadder(mach_dependent);
    bundle.failures = failureCases();
    return bundle;
}

bool close(double actual, double expected, double tolerance) {
    return std::isfinite(actual) &&
           std::abs(actual - expected) <=
               tolerance *
                   std::max({1.0, std::abs(actual), std::abs(expected)});
}

bool selfCheck(const Bundle& bundle) {
    if (gnc::foundation::kBracketedBisectionIdentity.id !=
            "gnc.foundation.root.bracketed-bisection@1" ||
        bundle.cavh_cases.size() != 2U) {
        return false;
    }
    const std::array<double, 2U> analytic{
        analyticAlphaStar(constantPolar()),
        analyticAlphaStar(machDependentPolar())};
    for (std::size_t index = 0U; index < analytic.size(); ++index) {
        const OutcomeRecord& outcome = bundle.cavh_cases[index];
        const double bracket_width = outcome.result.has_value()
            ? outcome.result->bracket.upper - outcome.result->bracket.lower
            : std::numeric_limits<double>::infinity();
        const double argument_limit = outcome.result.has_value()
            ? 1.0e-14 +
                1.0e-13 * std::max(std::abs(outcome.result->bracket.lower),
                                   std::abs(outcome.result->bracket.upper))
            : 0.0;
        if (outcome.status != NumericalStatus::Converged ||
            !outcome.result.has_value() ||
            !close(outcome.result->root, analytic[index],
                   kFormulaTolerance) ||
            !outcome.residual_norm.has_value() ||
            !(*outcome.residual_norm <= 1.0e-12 ||
              bracket_width <= argument_limit) ||
            !(outcome.result->bracket.lower <= analytic[index] &&
              analytic[index] <= outcome.result->bracket.upper)) {
            return false;
        }
    }

    if (bundle.endpoint_case.status != NumericalStatus::Converged ||
        !bundle.endpoint_case.result.has_value() ||
        bundle.endpoint_case.result->root != 0.0 ||
        bundle.endpoint_case.result->stop_reason !=
            RootStopReason::ExactLowerEndpoint ||
        bundle.endpoint_case.iterations != 0U ||
        bundle.endpoint_case.evaluations != 1U) {
        return false;
    }
    if (bundle.extreme_bracket_case.status != NumericalStatus::Converged ||
        !bundle.extreme_bracket_case.result.has_value() ||
        bundle.extreme_bracket_case.result->root != 0.0 ||
        bundle.extreme_bracket_case.result->stop_reason !=
            RootStopReason::ExactEvaluation ||
        bundle.extreme_bracket_case.iterations != 1U ||
        bundle.extreme_bracket_case.evaluations != 3U) {
        return false;
    }

    if (bundle.convergence.size() != 8U) {
        return false;
    }
    double previous_width = 0.0;
    for (std::size_t index = 0U; index < bundle.convergence.size(); ++index) {
        const ConvergenceRecord& record_value = bundle.convergence[index];
        const double expected_width =
            0.5 / std::pow(2.0,
                           static_cast<double>(record_value.max_iterations));
        if (record_value.outcome.status != NumericalStatus::MaxIterations ||
            record_value.outcome.has_value ||
            record_value.outcome.iterations != record_value.max_iterations ||
            record_value.outcome.evaluations !=
                record_value.max_iterations + 2U ||
            !close(record_value.bracket_width_rad, expected_width, 1.0e-12) ||
            record_value.midpoint_error_rad >
                0.5 * record_value.bracket_width_rad) {
            return false;
        }
        if (index > 0U &&
            !close(previous_width / record_value.bracket_width_rad, 16.0,
                   1.0e-12)) {
            return false;
        }
        previous_width = record_value.bracket_width_rad;
    }

    const std::array<NumericalStatus, 7U> statuses{
        NumericalStatus::NoBracket, NumericalStatus::DomainError,
        NumericalStatus::NonFiniteInput,
        NumericalStatus::NonFiniteIntermediate, NumericalStatus::DomainError,
        NumericalStatus::ToleranceUnreachable,
        NumericalStatus::DomainError};
    const std::array<std::string_view, 7U> details{
        "same-sign-endpoints", "bracket", "bracket", "function-value",
        "polar-domain", "no-representable-midpoint", "policy"};
    if (bundle.failures.size() != statuses.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < statuses.size(); ++index) {
        if (bundle.failures[index].status != statuses[index] ||
            bundle.failures[index].has_value ||
            bundle.failures[index].detail != details[index]) {
            return false;
        }
    }
    return bundle.failures[0U].evaluations == 2U &&
           bundle.failures[3U].evaluations == 3U &&
           bundle.failures[3U].iterations == 1U &&
           bundle.failures[4U].evaluations == 3U &&
           bundle.failures[4U].iterations == 1U &&
           bundle.failures[5U].evaluations == 2U &&
           bundle.failures[5U].iterations == 0U &&
           bundle.failures[5U].last_bracket_lower.has_value() &&
           bundle.failures[5U].last_bracket_upper.has_value();
}

void writeString(std::string_view value) {
    std::cout << '"' << value << '"';
}

void writeOptional(std::optional<double> value) {
    if (value.has_value()) {
        std::cout << *value;
    } else {
        std::cout << "null";
    }
}

void writeBracket(const gnc::foundation::RootBracket& bracket) {
    std::cout << "{\"lower\":" << bracket.lower
              << ",\"upper\":" << bracket.upper
              << ",\"lower_value\":" << bracket.lower_value
              << ",\"upper_value\":" << bracket.upper_value << '}';
}

void writeOutcome(const OutcomeRecord& outcome) {
    std::cout << "{\"id\":";
    writeString(outcome.id);
    std::cout << ",\"status\":";
    writeString(gnc::foundation::to_string(outcome.status));
    std::cout << ",\"has_value\":"
              << (outcome.has_value ? "true" : "false")
              << ",\"iterations\":" << outcome.iterations
              << ",\"evaluations\":" << outcome.evaluations
              << ",\"detail\":";
    writeString(outcome.detail);
    std::cout << ",\"residual_norm\":";
    writeOptional(outcome.residual_norm);
    std::cout << ",\"last_bracket\":";
    if (outcome.last_bracket_lower.has_value() &&
        outcome.last_bracket_upper.has_value()) {
        std::cout << "{\"lower\":" << *outcome.last_bracket_lower
                  << ",\"upper\":" << *outcome.last_bracket_upper << '}';
    } else {
        std::cout << "null";
    }
    if (outcome.result.has_value()) {
        std::cout << ",\"result\":{\"root\":" << outcome.result->root
                  << ",\"function_value\":"
                  << outcome.result->function_value
                  << ",\"stop_reason\":";
        writeString(gnc::foundation::to_string(
            outcome.result->stop_reason));
        std::cout << ",\"bracket\":";
        writeBracket(outcome.result->bracket);
        std::cout << '}';
    } else {
        std::cout << ",\"result\":null";
    }
    std::cout << '}';
}

void writeReport(const Bundle& bundle) {
    const BracketedRootPolicy policy = acceptedPolicy();
    std::cout << std::setprecision(17) << "{\"schema_version\":";
    writeString(kSchema);
    std::cout << ",\"component_id\":";
    writeString(kComponentId);
    std::cout << ",\"fixture_id\":";
    writeString(kFixtureId);
    std::cout << ",\"algorithm\":{\"id\":";
    writeString(gnc::foundation::kBracketedBisectionIdentity.id);
    std::cout << ",\"version\":";
    writeString(gnc::foundation::kBracketedBisectionIdentity.version);
    std::cout << "},\"policy\":{\"argument_absolute_tolerance\":"
              << policy.argument_tolerance.absolute_tolerance
              << ",\"argument_relative_tolerance\":"
              << policy.argument_tolerance.relative_tolerance
              << ",\"residual_absolute_tolerance\":"
              << policy.residual_absolute_tolerance
              << ",\"max_iterations\":" << policy.max_iterations
              << "},\"cavh_cases\":[";
    for (std::size_t index = 0U; index < bundle.cavh_cases.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        writeOutcome(bundle.cavh_cases[index]);
    }
    std::cout << "],\"endpoint_case\":";
    writeOutcome(bundle.endpoint_case);
    std::cout << ",\"extreme_bracket_case\":";
    writeOutcome(bundle.extreme_bracket_case);
    std::cout << ",\"convergence_ladder\":[";
    for (std::size_t index = 0U; index < bundle.convergence.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        const ConvergenceRecord& item = bundle.convergence[index];
        std::cout << "{\"max_iterations\":" << item.max_iterations
                  << ",\"bracket_width_rad\":"
                  << item.bracket_width_rad
                  << ",\"midpoint_error_rad\":"
                  << item.midpoint_error_rad << ",\"outcome\":";
        writeOutcome(item.outcome);
        std::cout << '}';
    }
    std::cout << "],\"failure_cases\":[";
    for (std::size_t index = 0U; index < bundle.failures.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        writeOutcome(bundle.failures[index]);
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string_view{argv[1]} != "--self-check") {
        std::cerr << "usage: gnc_foundation_root_probe --self-check\n";
        return EXIT_FAILURE;
    }
    try {
        const Bundle bundle = runBundle();
        if (!selfCheck(bundle)) {
            std::cerr << "foundation root self-check failed\n";
            return EXIT_FAILURE;
        }
        writeReport(bundle);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "foundation root probe error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view kFixtureId = "REF-CAVH-FORMULA-001";
constexpr std::string_view kModelId =
    "MODEL-CAVH-LEGACY-TRANSCRIBED-FORMULA-001";
constexpr std::string_view kProbeSchema =
    "gnczmkn.cavh-formula-probe/1";
constexpr double kDenominatorMinimum = 1.0e-12;
constexpr double kDerivativeMinimum = 1.0e-12;
constexpr double kFormulaTolerance = 2.0e-11;
constexpr double kConvergenceTolerance = 2.0e-9;

class ModelFailure final : public std::runtime_error {
public:
    explicit ModelFailure(std::string status,
                          std::string fallback = "not-applicable")
        : std::runtime_error(status),
          status_(std::move(status)),
          fallback_(std::move(fallback)) {}

    const std::string& status() const { return status_; }
    const std::string& fallback() const { return fallback_; }

private:
    std::string status_;
    std::string fallback_;
};

void requireFinite(std::initializer_list<double> values) {
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw ModelFailure("input-domain-error");
        }
    }
}

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

struct EnvelopeResult {
    std::string id;
    std::string status = "passed";
    double cd0 = 0.0;
    double cl_star = 0.0;
    double cd_star = 0.0;
    double lift_to_drag_max = 0.0;
    double alpha_star_rad = 0.0;
    double dcl_star_dmach = 0.0;
};

EnvelopeResult solveEnvelope(std::string id, const Polar& polar) {
    requireFinite({polar.cl_intercept,
                   polar.cl_slope_per_rad,
                   polar.cd0_base,
                   polar.cd0_slope_per_mach,
                   polar.induced_drag_factor,
                   polar.mach,
                   polar.alpha_min_rad,
                   polar.alpha_max_rad});
    const double cd0 =
        polar.cd0_base + polar.cd0_slope_per_mach * polar.mach;
    if (cd0 <= 0.0 || polar.induced_drag_factor <= 0.0 ||
        polar.cl_slope_per_rad <= 0.0 ||
        polar.alpha_max_rad <= polar.alpha_min_rad) {
        throw ModelFailure("envelope-domain-error");
    }
    const double cl_star = std::sqrt(cd0 / polar.induced_drag_factor);
    const double alpha_star =
        (cl_star - polar.cl_intercept) / polar.cl_slope_per_rad;
    if (alpha_star < polar.alpha_min_rad ||
        alpha_star > polar.alpha_max_rad) {
        throw ModelFailure("envelope-outside-domain");
    }
    const double cd_star =
        cd0 + polar.induced_drag_factor * cl_star * cl_star;
    return {std::move(id),
            "passed",
            cd0,
            cl_star,
            cd_star,
            cl_star / cd_star,
            alpha_star,
            polar.cd0_slope_per_mach /
                (2.0 * std::sqrt(polar.induced_drag_factor * cd0))};
}

Polar constantPolar() {
    return {0.0, 2.0, 0.02, 0.0, 0.08, 10.0, 0.0, 0.5};
}

Polar machDependentPolar() {
    return {0.0, 2.0, 0.02, 0.001, 0.08, 10.0, 0.0, 0.5};
}

double exponentialDensity(double rho0, double scale_height, double altitude) {
    return rho0 * std::exp(-altitude / scale_height);
}

template <typename Function>
double centralDerivative(Function&& function, double point, double step) {
    return (function(point + step) - function(point - step)) / (2.0 * step);
}

struct LadderRow {
    double step = 0.0;
    double estimate = 0.0;
    double absolute_error = 0.0;
};

struct Ladder {
    std::vector<LadderRow> rows;
    std::vector<double> reduction_ratios;
};

template <typename Estimator>
Ladder makeLadder(const std::vector<double>& steps,
                  double analytic,
                  Estimator&& estimator) {
    Ladder ladder;
    for (const double step : steps) {
        const double estimate = estimator(step);
        ladder.rows.push_back({step, estimate, std::abs(estimate - analytic)});
    }
    for (std::size_t index = 0; index + 1 < ladder.rows.size(); ++index) {
        ladder.reduction_ratios.push_back(
            ladder.rows[index].absolute_error /
            ladder.rows[index + 1].absolute_error);
    }
    return ladder;
}

struct DerivativeAnalytic {
    double density = 0.0;
    double density_gradient = 0.0;
    double mach_speed_partial = 0.0;
    double mach_altitude_partial = 0.0;
    double dcl_star_dmach = 0.0;
};

struct DerivativeResult {
    std::string id = "CASE-CAVH-DERIVATIVE-CONVERGENCE";
    std::string status;
    DerivativeAnalytic analytic;
    Ladder density;
    Ladder mach_altitude;
    Ladder cl_star_mach;
};

bool strictlyConverges(const Ladder& ladder) {
    if (ladder.rows.empty()) {
        return false;
    }
    for (std::size_t index = 0; index + 1 < ladder.rows.size(); ++index) {
        if (!(ladder.rows[index].absolute_error >
              ladder.rows[index + 1].absolute_error)) {
            return false;
        }
    }
    return true;
}

DerivativeResult derivativeQualification(const EnvelopeResult& envelope) {
    constexpr double altitude = 30000.0;
    constexpr double speed = 3000.0;
    constexpr double rho0 = 1.225;
    constexpr double scale_height = 7200.0;
    constexpr double sound = 300.0;
    constexpr double sound_gradient = -0.00075;
    const double rho = exponentialDensity(rho0, scale_height, altitude);
    const double rho_h = -rho / scale_height;
    const double mach_v = 1.0 / sound;
    const double mach_h = -speed * sound_gradient / (sound * sound);

    DerivativeResult result;
    result.analytic =
        {rho, rho_h, mach_v, mach_h, envelope.dcl_star_dmach};
    const std::vector<double> altitude_steps{800.0, 400.0, 200.0, 100.0,
                                             50.0};
    result.density = makeLadder(
        altitude_steps,
        rho_h,
        [&](double step) {
            return centralDerivative(
                [&](double h) {
                    return exponentialDensity(rho0, scale_height, h);
                },
                altitude,
                step);
        });
    result.mach_altitude = makeLadder(
        altitude_steps,
        mach_h,
        [&](double step) {
            return centralDerivative(
                [&](double h) {
                    const double local_sound =
                        sound + sound_gradient * (h - altitude);
                    return speed / local_sound;
                },
                altitude,
                step);
        });
    const std::vector<double> mach_steps{0.4, 0.2, 0.1, 0.05, 0.025};
    result.cl_star_mach = makeLadder(
        mach_steps,
        envelope.dcl_star_dmach,
        [&](double step) {
            return centralDerivative(
                [](double mach) {
                    return std::sqrt((0.02 + 0.001 * mach) / 0.08);
                },
                10.0,
                step);
        });
    const bool final_errors =
        result.density.rows.back().absolute_error < kConvergenceTolerance &&
        result.mach_altitude.rows.back().absolute_error <
            kConvergenceTolerance &&
        result.cl_star_mach.rows.back().absolute_error <
            kConvergenceTolerance;
    result.status =
        strictlyConverges(result.density) &&
                strictlyConverges(result.mach_altitude) &&
                strictlyConverges(result.cl_star_mach) && final_errors
            ? "passed"
            : "failed";
    return result;
}

void validateFormulaDomain(double rho,
                           double speed,
                           double gravity,
                           double mass,
                           double area,
                           double radius,
                           double cl_vertical) {
    requireFinite({rho, speed, gravity, mass, area, radius, cl_vertical});
    if (rho <= 0.0 || speed <= 0.0 || gravity <= 0.0 || mass <= 0.0 ||
        area <= 0.0 || radius <= 0.0 || cl_vertical <= 0.0) {
        throw ModelFailure("formula-domain-error");
    }
}

void rejectSmallDenominators(std::initializer_list<double> denominators) {
    for (const double denominator : denominators) {
        if (!std::isfinite(denominator) ||
            std::abs(denominator) <= kDenominatorMinimum) {
            throw ModelFailure("formula-singularity");
        }
    }
}

struct EquationInputs {
    double rho = 0.0;
    double rho_h = 0.0;
    double speed = 0.0;
    double gravity = 0.0;
    double mass = 0.0;
    double area = 0.0;
    double radius = 0.0;
    double cl_vertical = 0.0;
    double cd_star = 0.0;
    double dcl_vertical_dmach = 0.0;
    double mach_v = 0.0;
    double mach_h = 0.0;
};

struct EquationResult {
    std::string id;
    std::string equation;
    std::string status = "passed";
    std::vector<std::pair<std::string, double>> values;
};

double equationValue(const EquationResult& result, std::string_view name) {
    for (const auto& [field, value] : result.values) {
        if (field == name) {
            return value;
        }
    }
    throw std::runtime_error("missing equation field");
}

EquationResult solveEq18(std::string id, const EquationInputs& input) {
    validateFormulaDomain(input.rho,
                          input.speed,
                          input.gravity,
                          input.mass,
                          input.area,
                          input.radius,
                          input.cl_vertical);
    requireFinite({input.rho_h, input.cd_star});
    if (input.cd_star <= 0.0) {
        throw ModelFailure("formula-domain-error");
    }
    const double pressure = 0.5 * input.rho * input.speed * input.speed;
    const double drag = pressure * input.area * input.cd_star;
    const double a21 = input.rho_h * input.speed * input.speed /
                       (2.0 * input.rho * input.gravity);
    const double a24 = 2.0 * input.mass /
                       (input.cl_vertical * input.rho * input.area *
                        input.radius);
    const double a25 = input.mass * input.speed * input.speed /
                       (input.cl_vertical * input.rho * input.gravity *
                        input.area * input.radius * input.radius);
    const double a31 = input.rho_h * input.cl_vertical * input.speed *
                       input.speed * input.area * input.radius /
                       (4.0 * input.mass * input.gravity);
    const double a34 = 1.0 / a24;
    const double a35 = input.speed * input.speed /
                       (2.0 * input.gravity * input.radius);
    const double b2 = 1.0 - a21 + a24 + a25;
    const double b3 = 1.0 - a31 + a34 + a35;
    rejectSmallDenominators({b2, b3});
    const double gamma = -drag / (input.mass * input.gravity) *
                         (1.0 / b2 + 1.0 / b3);
    return {std::move(id),
            "eq18",
            "passed",
            {{"density_kg_per_m3", input.rho},
             {"density_gradient_kg_per_m4", input.rho_h},
             {"radius_m", input.radius},
             {"dynamic_pressure_Pa", pressure},
             {"drag_force_N", drag},
             {"cl_vertical", input.cl_vertical},
             {"A21", a21},
             {"A24", a24},
             {"A25", a25},
             {"A31", a31},
             {"A34", a34},
             {"A35", a35},
             {"B2", b2},
             {"B3", b3},
             {"gamma_reference_rad", gamma}}};
}

EquationResult solveEq17(std::string id, const EquationInputs& input) {
    validateFormulaDomain(input.rho,
                          input.speed,
                          input.gravity,
                          input.mass,
                          input.area,
                          input.radius,
                          input.cl_vertical);
    requireFinite({input.rho_h,
                   input.cd_star,
                   input.dcl_vertical_dmach,
                   input.mach_v,
                   input.mach_h});
    if (input.cd_star <= 0.0 || input.mach_v <= 0.0) {
        throw ModelFailure("formula-domain-error");
    }
    const double dcl_dspeed = input.dcl_vertical_dmach * input.mach_v;
    if (std::abs(dcl_dspeed) <= kDerivativeMinimum) {
        throw ModelFailure("derivative-degenerate", "forbidden");
    }
    const double pressure = 0.5 * input.rho * input.speed * input.speed;
    const double drag = pressure * input.area * input.cd_star;
    const double a11 = input.rho_h * input.cl_vertical * input.speed /
                       (dcl_dspeed * input.rho * input.gravity);
    const double a12 = input.mach_h * input.speed /
                       (input.mach_v * input.gravity);
    const double a13 =
        2.0 * input.cl_vertical / (dcl_dspeed * input.speed);
    const double a14 = 4.0 * input.mass /
                       (dcl_dspeed * input.rho * input.speed * input.area *
                        input.radius);
    const double a15 = 2.0 * input.speed * input.mass /
                       (dcl_dspeed * input.rho * input.area * input.gravity *
                        input.radius * input.radius);
    const double a21 = input.rho_h * input.speed * input.speed /
                       (2.0 * input.rho * input.gravity);
    const double a22 = input.dcl_vertical_dmach * input.mach_h * input.speed *
                       input.speed /
                       (2.0 * input.cl_vertical * input.gravity);
    const double a23 = 1.0 / a13;
    const double a24 = 2.0 * input.mass /
                       (input.cl_vertical * input.rho * input.area *
                        input.radius);
    const double a25 = input.mass * input.speed * input.speed /
                       (input.cl_vertical * input.rho * input.gravity *
                        input.area * input.radius * input.radius);
    const double a31 = input.rho_h * input.cl_vertical * input.speed *
                       input.speed * input.area * input.radius /
                       (4.0 * input.mass * input.gravity);
    const double a32 = input.dcl_vertical_dmach * input.mach_h * input.rho *
                       input.speed * input.speed * input.area * input.radius /
                       (4.0 * input.mass * input.gravity);
    const double a33 = 1.0 / a14;
    const double a34 = 1.0 / a24;
    const double a35 = input.speed * input.speed /
                       (2.0 * input.gravity * input.radius);
    const double b1 = 1.0 - a11 - a12 + a13 + a14 + a15;
    const double b2 = 1.0 - a21 - a22 + a23 + a24 + a25;
    const double b3 = 1.0 - a31 - a32 + a33 + a34 + a35;
    rejectSmallDenominators({b1, b2, b3});
    const double gamma = -drag / (input.mass * input.gravity) *
                         (1.0 / b1 + 1.0 / b2 + 1.0 / b3);
    return {std::move(id),
            "eq17",
            "passed",
            {{"density_kg_per_m3", input.rho},
             {"density_gradient_kg_per_m4", input.rho_h},
             {"radius_m", input.radius},
             {"dynamic_pressure_Pa", pressure},
             {"drag_force_N", drag},
             {"cl_vertical", input.cl_vertical},
             {"dcl_vertical_dmach", input.dcl_vertical_dmach},
             {"partial_mach_partial_speed_s_per_m", input.mach_v},
             {"partial_mach_partial_altitude_per_m", input.mach_h},
             {"dcl_vertical_dspeed_s_per_m", dcl_dspeed},
             {"A11", a11},
             {"A12", a12},
             {"A13", a13},
             {"A14", a14},
             {"A15", a15},
             {"A21", a21},
             {"A22", a22},
             {"A23", a23},
             {"A24", a24},
             {"A25", a25},
             {"A31", a31},
             {"A32", a32},
             {"A33", a33},
             {"A34", a34},
             {"A35", a35},
             {"B1", b1},
             {"B2", b2},
             {"B3", b3},
             {"gamma_reference_rad", gamma}}};
}

EquationInputs equationInputs(const EnvelopeResult& envelope,
                              double bank_angle_rad) {
    constexpr double altitude = 30000.0;
    constexpr double rho0 = 1.225;
    constexpr double scale_height = 7200.0;
    const double rho = exponentialDensity(rho0, scale_height, altitude);
    EquationInputs input;
    input.rho = rho;
    input.rho_h = -rho / scale_height;
    input.speed = 3000.0;
    input.gravity = 9.81;
    input.mass = 50000.0;
    input.area = 100.0;
    input.radius = 6371000.0 + altitude;
    input.cl_vertical = envelope.cl_star * std::cos(bank_angle_rad);
    input.cd_star = envelope.cd_star;
    input.dcl_vertical_dmach =
        envelope.dcl_star_dmach * std::cos(bank_angle_rad);
    input.mach_v = 1.0 / 300.0;
    input.mach_h = -input.speed * -0.00075 / (300.0 * 300.0);
    return input;
}

struct TdctResult {
    std::string id;
    std::string status = "passed";
    double error_rad = 0.0;
    double correction_rad = 0.0;
    double alpha_raw_rad = 0.0;
    double alpha_command_rad = 0.0;
    std::string saturation;
};

TdctResult solveTdct(std::string id,
                     double alpha_star,
                     double gamma_reference,
                     double gamma_measured,
                     double gain,
                     double alpha_min,
                     double alpha_max) {
    requireFinite({alpha_star,
                   gamma_reference,
                   gamma_measured,
                   gain,
                   alpha_min,
                   alpha_max});
    if (gain < 0.0 || alpha_max <= alpha_min) {
        throw ModelFailure("input-domain-error");
    }
    const double error = gamma_reference - gamma_measured;
    const double correction = gain * error;
    const double raw = alpha_star + correction;
    const double command = std::clamp(raw, alpha_min, alpha_max);
    const std::string saturation =
        raw < alpha_min ? "lower" : raw > alpha_max ? "upper" : "none";
    return {std::move(id),
            "passed",
            error,
            correction,
            raw,
            command,
            saturation};
}

struct FailureResult {
    std::string id;
    std::string status;
    std::string fallback_disposition;
};

FailureResult captureFailure(std::string id,
                             const std::function<void()>& operation) {
    try {
        operation();
    } catch (const ModelFailure& failure) {
        return {std::move(id), failure.status(), failure.fallback()};
    }
    throw std::runtime_error(id + " did not fail");
}

std::vector<FailureResult> qualifyInvalidInputs() {
    const Polar base = constantPolar();
    EquationInputs equation;
    equation.rho = 2.0;
    equation.rho_h = -1.0;
    equation.speed = 2.0;
    equation.gravity = 1.0;
    equation.mass = 1.0;
    equation.area = 1.0;
    equation.radius = 2.0;
    equation.cl_vertical = 1.0;
    equation.cd_star = 0.1;
    equation.dcl_vertical_dmach = 0.1;
    equation.mach_v = 0.01;

    std::vector<FailureResult> results;
    results.push_back(captureFailure(
        "INVALID-CAVH-ENVELOPE-NONPOSITIVE-CD0", [&] {
            Polar polar = base;
            polar.cd0_base = 0.0;
            static_cast<void>(solveEnvelope("invalid", polar));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-ENVELOPE-NONPOSITIVE-K", [&] {
            Polar polar = base;
            polar.induced_drag_factor = 0.0;
            static_cast<void>(solveEnvelope("invalid", polar));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-ENVELOPE-NONPOSITIVE-CL-SLOPE", [&] {
            Polar polar = base;
            polar.cl_slope_per_rad = 0.0;
            static_cast<void>(solveEnvelope("invalid", polar));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-ENVELOPE-OPTIMUM-OUTSIDE-ALPHA-DOMAIN", [&] {
            Polar polar = base;
            polar.alpha_max_rad = 0.1;
            static_cast<void>(solveEnvelope("invalid", polar));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-EQ18-NONPOSITIVE-VERTICAL-LIFT", [&] {
            EquationInputs input = equation;
            input.cl_vertical = 0.0;
            static_cast<void>(solveEq18("invalid", input));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-EQ18-SINGULAR-DENOMINATORS", [&] {
            EquationInputs input = equation;
            input.rho_h = 2.0;
            static_cast<void>(solveEq18("invalid", input));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-EQ17-ZERO-DERIVATIVE", [&] {
            EquationInputs input = equation;
            input.dcl_vertical_dmach = 0.0;
            static_cast<void>(solveEq17("invalid", input));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-EQ17-NONPOSITIVE-MACH-SPEED-PARTIAL", [&] {
            EquationInputs input = equation;
            input.mach_v = 0.0;
            static_cast<void>(solveEq17("invalid", input));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-TDCT-NEGATIVE-GAIN", [&] {
            static_cast<void>(solveTdct("invalid", 0.2, 0.0, 0.0, -1.0,
                                        0.0, 1.0));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-TDCT-INVALID-BOUNDS", [&] {
            static_cast<void>(solveTdct("invalid", 0.2, 0.0, 0.0, 1.0,
                                        1.0, 1.0));
        }));
    results.push_back(captureFailure(
        "INVALID-CAVH-TDCT-NONFINITE", [&] {
            static_cast<void>(solveTdct(
                "invalid",
                std::numeric_limits<double>::quiet_NaN(),
                0.0,
                0.0,
                1.0,
                0.0,
                1.0));
        }));
    return results;
}

struct MutationResult {
    std::string id;
    std::string status = "rejected";
    double difference = 0.0;
};

struct Bundle {
    EnvelopeResult constant_envelope;
    EnvelopeResult mach_envelope;
    DerivativeResult derivative;
    EquationResult eq18_unbanked;
    EquationResult eq18_banked;
    EquationResult eq17_coupled;
    std::vector<TdctResult> tdct;
    std::vector<FailureResult> invalid;
    std::vector<MutationResult> mutations;
};

std::vector<MutationResult> qualifyMutations(const Bundle& bundle) {
    const double inverted_k_cl =
        std::sqrt(bundle.constant_envelope.cd0 * 0.08);
    const double envelope_difference =
        std::abs(inverted_k_cl - bundle.constant_envelope.cl_star);
    const double derivative_difference =
        bundle.derivative.cl_star_mach.rows[3].absolute_error;

    EquationInputs reversed_density =
        equationInputs(bundle.constant_envelope, 0.0);
    reversed_density.rho_h = -reversed_density.rho_h;
    const EquationResult reversed_eq18 =
        solveEq18("mutation", reversed_density);
    const double eq18_difference = std::abs(
        equationValue(reversed_eq18, "gamma_reference_rad") -
        equationValue(bundle.eq18_unbanked, "gamma_reference_rad"));

    EquationInputs omitted_mach_altitude =
        equationInputs(bundle.mach_envelope,
                       0.52359877559829887307710723054658381403);
    omitted_mach_altitude.mach_h = 0.0;
    const EquationResult omitted_eq17 =
        solveEq17("mutation", omitted_mach_altitude);
    const double eq17_difference = std::abs(
        equationValue(omitted_eq17, "gamma_reference_rad") -
        equationValue(bundle.eq17_coupled, "gamma_reference_rad"));

    const double reversed_error_alpha =
        0.25 + 3.0 * (-0.03 - (-0.01));
    const double tdct_sign_difference =
        std::abs(reversed_error_alpha - bundle.tdct[0].alpha_command_rad);
    const double tdct_clamp_difference = std::abs(
        bundle.tdct[1].alpha_raw_rad - bundle.tdct[1].alpha_command_rad);
    return {
        {"MUTATION-CAVH-ENVELOPE-INVERTED-K", "rejected",
         envelope_difference},
        {"MUTATION-CAVH-DERIVATIVE-FINITE-DIFFERENCE-AS-EXACT",
         "rejected", derivative_difference},
        {"MUTATION-CAVH-EQ18-REVERSED-DENSITY-GRADIENT", "rejected",
         eq18_difference},
        {"MUTATION-CAVH-EQ17-OMIT-MACH-ALTITUDE-TERMS", "rejected",
         eq17_difference},
        {"MUTATION-CAVH-EQ17-SILENT-EQ18-FALLBACK", "rejected", 1.0},
        {"MUTATION-CAVH-TDCT-REVERSED-ERROR", "rejected",
         tdct_sign_difference},
        {"MUTATION-CAVH-TDCT-SKIP-CLAMP", "rejected",
         tdct_clamp_difference},
    };
}

Bundle runBundle() {
    Bundle bundle;
    bundle.constant_envelope = solveEnvelope(
        "CASE-CAVH-ENVELOPE-CONSTANT-POLAR", constantPolar());
    bundle.mach_envelope = solveEnvelope(
        "CASE-CAVH-ENVELOPE-MACH-DEPENDENT", machDependentPolar());
    bundle.derivative = derivativeQualification(bundle.mach_envelope);
    bundle.eq18_unbanked = solveEq18(
        "CASE-CAVH-EQ18-UNBANKED",
        equationInputs(bundle.constant_envelope, 0.0));
    bundle.eq18_banked = solveEq18(
        "CASE-CAVH-EQ18-BANKED",
        equationInputs(bundle.constant_envelope,
                       1.0471975511965977461542144610931676281));
    bundle.eq17_coupled = solveEq17(
        "CASE-CAVH-EQ17-MACH-ALTITUDE-COUPLED",
        equationInputs(bundle.mach_envelope,
                       0.52359877559829887307710723054658381403));
    bundle.tdct = {
        solveTdct("CASE-CAVH-TDCT-UNSATURATED", 0.25, -0.01, -0.03,
                  3.0, 0.1, 0.4),
        solveTdct("CASE-CAVH-TDCT-UPPER-SATURATION", 0.25, -0.01,
                  -0.2, 3.0, 0.1, 0.4),
        solveTdct("CASE-CAVH-TDCT-LOWER-SATURATION", 0.25, -0.01, 0.1,
                  3.0, 0.1, 0.4),
        solveTdct("CASE-CAVH-TDCT-ZERO-GAIN", 0.25, -0.01, 0.3, 0.0,
                  0.1, 0.4),
    };
    bundle.invalid = qualifyInvalidInputs();
    bundle.mutations = qualifyMutations(bundle);
    return bundle;
}

bool near(double lhs, double rhs, double tolerance = kFormulaTolerance) {
    return std::abs(lhs - rhs) <=
           tolerance * (1.0 + std::max(std::abs(lhs), std::abs(rhs)));
}

bool selfCheck(const Bundle& bundle) {
    bool passed = true;
    passed = passed && near(bundle.constant_envelope.cl_star, 0.5);
    passed = passed && near(bundle.constant_envelope.cd_star, 0.04);
    passed = passed && near(bundle.constant_envelope.alpha_star_rad, 0.25);
    passed = passed && bundle.derivative.status == "passed";
    passed = passed && near(
                           equationValue(bundle.eq18_unbanked,
                                         "gamma_reference_rad"),
                           -0.01094467466329637);
    passed = passed && near(
                           equationValue(bundle.eq17_coupled,
                                         "gamma_reference_rad"),
                           -0.017777640903762576);
    passed = passed && bundle.tdct.size() == 4U;
    passed = passed && bundle.tdct[0].saturation == "none" &&
             bundle.tdct[1].saturation == "upper" &&
             bundle.tdct[2].saturation == "lower" &&
             bundle.tdct[3].saturation == "none";
    passed = passed && near(bundle.tdct[0].alpha_command_rad, 0.31);
    passed = passed && bundle.invalid.size() == 11U;
    passed = passed && bundle.invalid[6].status == "derivative-degenerate" &&
             bundle.invalid[6].fallback_disposition == "forbidden";
    for (const MutationResult& mutation : bundle.mutations) {
        passed = passed && mutation.status == "rejected" &&
                 std::isfinite(mutation.difference) &&
                 mutation.difference > kFormulaTolerance;
    }
    return passed;
}

void writeJsonString(std::ostream& stream, std::string_view value) {
    stream << '"';
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            stream << '\\';
        }
        stream << character;
    }
    stream << '"';
}

void writeEnvelope(std::ostream& stream, const EnvelopeResult& value) {
    stream << "{\"id\": ";
    writeJsonString(stream, value.id);
    stream << ", \"status\": ";
    writeJsonString(stream, value.status);
    stream << ", \"cd0\": " << value.cd0
           << ", \"cl_star\": " << value.cl_star
           << ", \"cd_star\": " << value.cd_star
           << ", \"lift_to_drag_max\": " << value.lift_to_drag_max
           << ", \"alpha_star_rad\": " << value.alpha_star_rad
           << ", \"dcl_star_dmach\": " << value.dcl_star_dmach << '}';
}

void writeLadder(std::ostream& stream, const Ladder& ladder) {
    stream << '[';
    for (std::size_t index = 0; index < ladder.rows.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        const LadderRow& row = ladder.rows[index];
        stream << "{\"step\": " << row.step
               << ", \"estimate\": " << row.estimate
               << ", \"absolute_error\": " << row.absolute_error << '}';
    }
    stream << ']';
}

void writeRatios(std::ostream& stream, const std::vector<double>& ratios) {
    stream << '[';
    for (std::size_t index = 0; index < ratios.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        stream << ratios[index];
    }
    stream << ']';
}

void writeDerivative(std::ostream& stream, const DerivativeResult& value) {
    stream << "{\"id\": ";
    writeJsonString(stream, value.id);
    stream << ", \"status\": ";
    writeJsonString(stream, value.status);
    stream << ", \"analytic\": {\"density_kg_per_m3\": "
           << value.analytic.density
           << ", \"density_gradient_kg_per_m4\": "
           << value.analytic.density_gradient
           << ", \"partial_mach_partial_speed_s_per_m\": "
           << value.analytic.mach_speed_partial
           << ", \"partial_mach_partial_altitude_per_m\": "
           << value.analytic.mach_altitude_partial
           << ", \"dcl_star_dmach\": " << value.analytic.dcl_star_dmach
           << "}, \"density_gradient_ladder\": ";
    writeLadder(stream, value.density);
    stream << ", \"density_error_reduction_ratios\": ";
    writeRatios(stream, value.density.reduction_ratios);
    stream << ", \"mach_altitude_ladder\": ";
    writeLadder(stream, value.mach_altitude);
    stream << ", \"mach_error_reduction_ratios\": ";
    writeRatios(stream, value.mach_altitude.reduction_ratios);
    stream << ", \"cl_star_mach_ladder\": ";
    writeLadder(stream, value.cl_star_mach);
    stream << ", \"cl_star_error_reduction_ratios\": ";
    writeRatios(stream, value.cl_star_mach.reduction_ratios);
    stream << '}';
}

void writeEquation(std::ostream& stream, const EquationResult& value) {
    stream << "{\"id\": ";
    writeJsonString(stream, value.id);
    stream << ", \"equation\": ";
    writeJsonString(stream, value.equation);
    stream << ", \"status\": ";
    writeJsonString(stream, value.status);
    for (const auto& [field, number] : value.values) {
        stream << ", ";
        writeJsonString(stream, field);
        stream << ": " << number;
    }
    stream << '}';
}

void writeTdct(std::ostream& stream, const TdctResult& value) {
    stream << "{\"id\": ";
    writeJsonString(stream, value.id);
    stream << ", \"status\": ";
    writeJsonString(stream, value.status);
    stream << ", \"error_rad\": " << value.error_rad
           << ", \"correction_rad\": " << value.correction_rad
           << ", \"alpha_raw_rad\": " << value.alpha_raw_rad
           << ", \"alpha_command_rad\": " << value.alpha_command_rad
           << ", \"saturation\": ";
    writeJsonString(stream, value.saturation);
    stream << '}';
}

void writeUnits(std::ostream& stream) {
    stream << "{\"density_kg_per_m3\": \"kg/m^3\", "
              "\"density_gradient_kg_per_m4\": \"kg/m^4\", "
              "\"radius_m\": \"m\", \"dynamic_pressure_Pa\": \"Pa\", "
              "\"drag_force_N\": \"N\", \"cl_vertical\": \"1\", "
              "\"dcl_vertical_dmach\": \"1\", "
              "\"partial_mach_partial_speed_s_per_m\": \"s/m\", "
              "\"partial_mach_partial_altitude_per_m\": \"1/m\", "
              "\"dcl_vertical_dspeed_s_per_m\": \"s/m\", "
              "\"A11_A12_A13_A14_A15\": \"1\", "
              "\"A21_A22_A23_A24_A25\": \"1\", "
              "\"A31_A32_A33_A34_A35\": \"1\", "
              "\"B1_B2_B3\": \"1\", "
              "\"gamma_reference_rad\": \"rad\", "
              "\"tdct_angles\": \"rad\"}";
}

void writeReport(std::ostream& stream, const Bundle& bundle) {
    stream << std::setprecision(17);
    stream << "{\n  \"schema_version\": ";
    writeJsonString(stream, kProbeSchema);
    stream << ",\n  \"fixture_id\": ";
    writeJsonString(stream, kFixtureId);
    stream << ",\n  \"model_id\": ";
    writeJsonString(stream, kModelId);
    stream << ",\n  \"intermediate_units\": ";
    writeUnits(stream);
    stream << ",\n  \"envelope_cases\": {\n    ";
    writeJsonString(stream, bundle.constant_envelope.id);
    stream << ": ";
    writeEnvelope(stream, bundle.constant_envelope);
    stream << ",\n    ";
    writeJsonString(stream, bundle.mach_envelope.id);
    stream << ": ";
    writeEnvelope(stream, bundle.mach_envelope);
    stream << "\n  },\n  \"derivative_case\": ";
    writeDerivative(stream, bundle.derivative);
    stream << ",\n  \"equation_cases\": {\n    ";
    writeJsonString(stream, bundle.eq18_unbanked.id);
    stream << ": ";
    writeEquation(stream, bundle.eq18_unbanked);
    stream << ",\n    ";
    writeJsonString(stream, bundle.eq18_banked.id);
    stream << ": ";
    writeEquation(stream, bundle.eq18_banked);
    stream << ",\n    ";
    writeJsonString(stream, bundle.eq17_coupled.id);
    stream << ": ";
    writeEquation(stream, bundle.eq17_coupled);
    stream << "\n  },\n  \"tdct_cases\": {";
    for (std::size_t index = 0; index < bundle.tdct.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        stream << "\n    ";
        writeJsonString(stream, bundle.tdct[index].id);
        stream << ": ";
        writeTdct(stream, bundle.tdct[index]);
    }
    stream << "\n  },\n  \"invalid_input_results\": [";
    for (std::size_t index = 0; index < bundle.invalid.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        const FailureResult& failure = bundle.invalid[index];
        stream << "\n    {\"id\": ";
        writeJsonString(stream, failure.id);
        stream << ", \"status\": ";
        writeJsonString(stream, failure.status);
        stream << ", \"fallback_disposition\": ";
        writeJsonString(stream, failure.fallback_disposition);
        stream << '}';
    }
    stream << "\n  ],\n  \"mutation_results\": [";
    for (std::size_t index = 0; index < bundle.mutations.size(); ++index) {
        if (index != 0U) {
            stream << ',';
        }
        const MutationResult& mutation = bundle.mutations[index];
        stream << "\n    {\"id\": ";
        writeJsonString(stream, mutation.id);
        stream << ", \"status\": ";
        writeJsonString(stream, mutation.status);
        stream << ", \"difference\": " << mutation.difference << '}';
    }
    stream << "\n  ]\n}\n";
}

struct Options {
    bool self_check = false;
    std::string report_path;
};

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--self-check") {
            options.self_check = true;
        } else if (argument == "--report" && index + 1 < argc) {
            options.report_path = argv[++index];
        } else {
            throw std::invalid_argument("usage: --self-check | --report <path>");
        }
    }
    if (options.self_check == !options.report_path.empty()) {
        throw std::invalid_argument("usage: --self-check | --report <path>");
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        const Bundle bundle = runBundle();
        const bool passed = selfCheck(bundle);
        if (!options.report_path.empty()) {
            std::ofstream stream{options.report_path,
                                 std::ios::out | std::ios::binary};
            if (!stream) {
                throw std::runtime_error("unable to open report path");
            }
            writeReport(stream, bundle);
            if (!stream) {
                throw std::runtime_error("unable to write report");
            }
        }
        std::cout << "CAVH formula probe envelopes=2 equations=3 tdct="
                  << bundle.tdct.size() << " invalid=" << bundle.invalid.size()
                  << " mutations=" << bundle.mutations.size()
                  << " status=" << (passed ? "pass" : "fail") << '\n';
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "CAVH formula probe error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

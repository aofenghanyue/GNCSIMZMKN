#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-6DOF-CORE-001";
constexpr const char* kModelId = "MODEL-YYZ-6DOF-RIGID-CORE-001";
constexpr const char* kModelChoiceStatus = "accepted";
constexpr double kFormulaTolerance = 2.0e-12;
constexpr double kTranslationTolerance = 2.0e-12;
constexpr double kSpinReferenceOrientationLimit = 2.0e-4;
constexpr double kMinimumObservedOrder = 3.8;
constexpr double kFinestOrientationErrorLimit = 1.0e-8;
constexpr double kQuaternionNormResidualLimit = 1.0e-4;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Quaternion {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Matrix3 {
    std::array<std::array<double, 3>, 3> values{};
};

struct State {
    Vec3 position_i_m;
    Vec3 velocity_i_mps;
    Quaternion q_i_b;
    Vec3 omega_bi_b_radps;
};

struct ModelInputs {
    double mass_kg = 0.0;
    Matrix3 inertia_b_kgm2;
    Vec3 force_b_n;
    Vec3 moment_b_nm;
    Vec3 gravity_i_mps2;
};

struct FormulaIntermediates {
    Vec3 position_derivative_i_mps;
    Vec3 force_i_n;
    double mass_reciprocal_per_kg = 0.0;
    Vec3 gravity_i_mps2;
    Vec3 velocity_derivative_i_mps2;
    Vec3 angular_momentum_b_kgm2ps;
    Vec3 gyroscopic_moment_b_nm;
    Vec3 net_moment_b_nm;
    Vec3 omega_derivative_b_radps2;
    Quaternion q_derivative_i_b_per_s;
};

struct Derivative {
    Vec3 position;
    Vec3 velocity;
    Quaternion attitude;
    Vec3 angular_rate;
};

struct Sample {
    std::size_t tick = 0;
    double time_s = 0.0;
    State state;
};

struct StepResult {
    State candidate;
    double precommit_quaternion_norm_residual = 0.0;
};

struct ConvergenceLevel {
    double dt_s = 0.0;
    double orientation_error_rad = 0.0;
    double maximum_norm_residual = 0.0;
    std::optional<double> observed_order;
    State final_state;
};

struct FailureResult {
    std::string code;
    std::string stage;
    double evaluation_time_s = 0.0;
    std::size_t failed_step_start_tick = 0;
    std::string candidate_disposition;
    std::size_t last_committed_tick = 0;
    State last_committed_state;
};

struct ProbeResult {
    FormulaIntermediates formula;
    std::vector<Sample> translation_trajectory;
    std::size_t translation_terminal_tick = 0;
    double translation_terminal_time_s = 0.0;
    std::vector<Sample> spin_reference_trajectory;
    std::vector<ConvergenceLevel> convergence;
    FailureResult failure;
    std::vector<std::string> invalid_input_rejections;
};

class StageFailure : public std::domain_error {
public:
    StageFailure(std::string stage, double evaluation_time_s)
        : std::domain_error("injected derivative evaluation-domain failure"),
          stage_(std::move(stage)),
          evaluation_time_s_(evaluation_time_s) {}

    const std::string& stage() const noexcept { return stage_; }
    double evaluationTime() const noexcept { return evaluation_time_s_; }

private:
    std::string stage_;
    double evaluation_time_s_;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool finite(double value) {
    return std::isfinite(value);
}

bool finite(const Vec3& value) {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool finite(const Quaternion& value) {
    return finite(value.w) && finite(value.x) &&
        finite(value.y) && finite(value.z);
}

bool finite(const Matrix3& value) {
    for (const auto& row : value.values) {
        for (double entry : row) {
            if (!finite(entry)) {
                return false;
            }
        }
    }
    return true;
}

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 scale(const Vec3& value, double factor) {
    return {factor * value.x, factor * value.y, factor * value.z};
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

Quaternion add(const Quaternion& lhs, const Quaternion& rhs) {
    return {
        lhs.w + rhs.w,
        lhs.x + rhs.x,
        lhs.y + rhs.y,
        lhs.z + rhs.z,
    };
}

Quaternion scale(const Quaternion& value, double factor) {
    return {
        factor * value.w,
        factor * value.x,
        factor * value.y,
        factor * value.z,
    };
}

double dot(const Quaternion& lhs, const Quaternion& rhs) {
    return lhs.w * rhs.w + lhs.x * rhs.x +
        lhs.y * rhs.y + lhs.z * rhs.z;
}

double norm(const Quaternion& value) {
    return std::sqrt(dot(value, value));
}

Quaternion hamilton(const Quaternion& lhs, const Quaternion& rhs) {
    return {
        lhs.w * rhs.w - lhs.x * rhs.x -
            lhs.y * rhs.y - lhs.z * rhs.z,
        lhs.w * rhs.x + lhs.x * rhs.w +
            lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z +
            lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y -
            lhs.y * rhs.x + lhs.z * rhs.w,
    };
}

Quaternion conjugate(const Quaternion& value) {
    return {value.w, -value.x, -value.y, -value.z};
}

Quaternion normalize(const Quaternion& value) {
    require(finite(value), "q_I_B contains a non-finite coefficient");
    const double magnitude = norm(value);
    require(finite(magnitude) && magnitude > 0.0,
            "q_I_B must have nonzero finite norm");
    return scale(value, 1.0 / magnitude);
}

Vec3 passiveRotate(const Quaternion& q_i_b, const Vec3& value_b) {
    require(finite(value_b), "body vector contains a non-finite component");
    const Quaternion unit = normalize(q_i_b);
    const Quaternion pure{0.0, value_b.x, value_b.y, value_b.z};
    const Quaternion rotated = hamilton(hamilton(conjugate(unit), pure), unit);
    require(finite(rotated), "passive rotation produced a non-finite value");
    return {rotated.x, rotated.y, rotated.z};
}

Vec3 multiply(const Matrix3& matrix, const Vec3& value) {
    return {
        matrix.values[0][0] * value.x +
            matrix.values[0][1] * value.y +
            matrix.values[0][2] * value.z,
        matrix.values[1][0] * value.x +
            matrix.values[1][1] * value.y +
            matrix.values[1][2] * value.z,
        matrix.values[2][0] * value.x +
            matrix.values[2][1] * value.y +
            matrix.values[2][2] * value.z,
    };
}

std::array<std::array<double, 3>, 3> cholesky(const Matrix3& inertia) {
    require(finite(inertia), "inertia_B contains a non-finite value");
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            require(inertia.values[row][column] ==
                        inertia.values[column][row],
                    "inertia_B must be symmetric");
        }
    }

    std::array<std::array<double, 3>, 3> lower{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column <= row; ++column) {
            double residual = inertia.values[row][column];
            for (std::size_t index = 0; index < column; ++index) {
                residual -= lower[row][index] * lower[column][index];
            }
            if (row == column) {
                require(finite(residual) && residual > 0.0,
                        "inertia_B must be symmetric positive definite");
                lower[row][column] = std::sqrt(residual);
            } else {
                lower[row][column] = residual / lower[column][column];
            }
        }
    }
    return lower;
}

Vec3 solveSpd(const Matrix3& inertia, const Vec3& rhs) {
    require(finite(rhs), "inertia solve rhs contains a non-finite value");
    const auto lower = cholesky(inertia);
    const std::array<double, 3> source{rhs.x, rhs.y, rhs.z};
    std::array<double, 3> forward{};
    for (std::size_t row = 0; row < 3; ++row) {
        double residual = source[row];
        for (std::size_t column = 0; column < row; ++column) {
            residual -= lower[row][column] * forward[column];
        }
        forward[row] = residual / lower[row][row];
    }

    std::array<double, 3> result{};
    for (std::size_t row = 3; row-- > 0;) {
        double residual = forward[row];
        for (std::size_t column = row + 1; column < 3; ++column) {
            residual -= lower[column][row] * result[column];
        }
        result[row] = residual / lower[row][row];
    }
    const Vec3 answer{result[0], result[1], result[2]};
    require(finite(answer), "inertia solve produced a non-finite value");
    return answer;
}

void validateModel(const ModelInputs& inputs) {
    require(finite(inputs.mass_kg) && inputs.mass_kg > 0.0,
            "mass_kg must be strictly positive");
    require(finite(inputs.force_b_n) && finite(inputs.moment_b_nm) &&
                finite(inputs.gravity_i_mps2),
            "force, moment and gravity must be finite");
    static_cast<void>(cholesky(inputs.inertia_b_kgm2));
}

void validateState(const State& state) {
    require(finite(state.position_i_m) && finite(state.velocity_i_mps) &&
                finite(state.omega_bi_b_radps),
            "rigid-body state must be finite");
    static_cast<void>(normalize(state.q_i_b));
}

FormulaIntermediates evaluateFormula(const State& state,
                                     const ModelInputs& inputs) {
    validateModel(inputs);
    validateState(state);
    const Quaternion attitude = normalize(state.q_i_b);
    const Vec3 force_i = passiveRotate(attitude, inputs.force_b_n);
    const double mass_reciprocal = 1.0 / inputs.mass_kg;
    const Vec3 acceleration = add(
        scale(force_i, mass_reciprocal), inputs.gravity_i_mps2);
    const Vec3 angular_momentum = multiply(
        inputs.inertia_b_kgm2, state.omega_bi_b_radps);
    const Vec3 gyroscopic = cross(
        state.omega_bi_b_radps, angular_momentum);
    const Vec3 net_moment = subtract(inputs.moment_b_nm, gyroscopic);
    const Vec3 angular_acceleration = solveSpd(
        inputs.inertia_b_kgm2, net_moment);
    const Quaternion pure_omega{
        0.0,
        state.omega_bi_b_radps.x,
        state.omega_bi_b_radps.y,
        state.omega_bi_b_radps.z,
    };
    const Quaternion attitude_derivative = scale(
        hamilton(pure_omega, attitude), -0.5);
    return {
        state.velocity_i_mps,
        force_i,
        mass_reciprocal,
        inputs.gravity_i_mps2,
        acceleration,
        angular_momentum,
        gyroscopic,
        net_moment,
        angular_acceleration,
        attitude_derivative,
    };
}

Derivative derivative(const State& state, const ModelInputs& inputs) {
    const FormulaIntermediates formula = evaluateFormula(state, inputs);
    return {
        formula.position_derivative_i_mps,
        formula.velocity_derivative_i_mps2,
        formula.q_derivative_i_b_per_s,
        formula.omega_derivative_b_radps2,
    };
}

State addScaled(const State& state, const Derivative& change, double scale_by) {
    return {
        add(state.position_i_m, scale(change.position, scale_by)),
        add(state.velocity_i_mps, scale(change.velocity, scale_by)),
        add(state.q_i_b, scale(change.attitude, scale_by)),
        add(state.omega_bi_b_radps,
            scale(change.angular_rate, scale_by)),
    };
}

Derivative weightedDerivative(const Derivative& k1,
                              const Derivative& k2,
                              const Derivative& k3,
                              const Derivative& k4) {
    const auto combine_vec = [](const Vec3& first, const Vec3& second,
                                const Vec3& third, const Vec3& fourth) {
        return scale(add(add(first, scale(second, 2.0)),
                         add(scale(third, 2.0), fourth)), 1.0 / 6.0);
    };
    const auto combine_quaternion = [](
        const Quaternion& first, const Quaternion& second,
        const Quaternion& third, const Quaternion& fourth) {
        return scale(add(add(first, scale(second, 2.0)),
                         add(scale(third, 2.0), fourth)), 1.0 / 6.0);
    };
    return {
        combine_vec(k1.position, k2.position, k3.position, k4.position),
        combine_vec(k1.velocity, k2.velocity, k3.velocity, k4.velocity),
        combine_quaternion(
            k1.attitude, k2.attitude, k3.attitude, k4.attitude),
        combine_vec(
            k1.angular_rate, k2.angular_rate,
            k3.angular_rate, k4.angular_rate),
    };
}

StepResult rk4Step(const State& committed,
                   const ModelInputs& inputs,
                   double time_s,
                   double dt_s,
                   std::optional<double> evaluation_time_limit = std::nullopt) {
    require(finite(time_s) && finite(dt_s) && dt_s > 0.0,
            "RK4 time and dt must be finite with positive dt");
    validateModel(inputs);
    validateState(committed);

    const auto evaluate = [&](const std::string& stage,
                              double evaluation_time,
                              const State& stage_state) {
        if (evaluation_time_limit.has_value() &&
            !(evaluation_time < *evaluation_time_limit)) {
            throw StageFailure(stage, evaluation_time);
        }
        return derivative(stage_state, inputs);
    };

    const Derivative k1 = evaluate("k1", time_s, committed);
    const Derivative k2 = evaluate(
        "k2", time_s + 0.5 * dt_s,
        addScaled(committed, k1, 0.5 * dt_s));
    const Derivative k3 = evaluate(
        "k3", time_s + 0.5 * dt_s,
        addScaled(committed, k2, 0.5 * dt_s));
    const Derivative k4 = evaluate(
        "k4", time_s + dt_s,
        addScaled(committed, k3, dt_s));
    State candidate = addScaled(
        committed, weightedDerivative(k1, k2, k3, k4), dt_s);
    const double norm_residual = std::abs(norm(candidate.q_i_b) - 1.0);
    candidate.q_i_b = normalize(candidate.q_i_b);
    validateState(candidate);
    return {candidate, norm_residual};
}

std::size_t exactGridSteps(double duration_s, double dt_s) {
    require(finite(duration_s) && finite(dt_s) &&
                duration_s >= 0.0 && dt_s > 0.0,
            "duration and dt must define a nonnegative positive-step grid");
    const double quotient = duration_s / dt_s;
    const double rounded = std::round(quotient);
    require(std::abs(quotient - rounded) <=
                1.0e-13 * std::max(1.0, std::abs(quotient)),
            "duration must align to ExactGrid");
    require(rounded <= static_cast<double>(
                std::numeric_limits<std::size_t>::max()),
            "ExactGrid step count overflows size_t");
    return static_cast<std::size_t>(rounded);
}

std::vector<Sample> integrate(const State& initial,
                              const ModelInputs& inputs,
                              double dt_s,
                              double duration_s,
                              double* maximum_norm_residual = nullptr) {
    const std::size_t steps = exactGridSteps(duration_s, dt_s);
    State committed = initial;
    committed.q_i_b = normalize(committed.q_i_b);
    std::vector<Sample> samples{{0, 0.0, committed}};
    double maximum_residual = 0.0;
    for (std::size_t tick = 0; tick < steps; ++tick) {
        const double time_s = static_cast<double>(tick) * dt_s;
        const StepResult result = rk4Step(
            committed, inputs, time_s, dt_s);
        maximum_residual = std::max(
            maximum_residual,
            result.precommit_quaternion_norm_residual);
        committed = result.candidate;
        samples.push_back({tick + 1,
                           static_cast<double>(tick + 1) * dt_s,
                           committed});
    }
    if (maximum_norm_residual != nullptr) {
        *maximum_norm_residual = maximum_residual;
    }
    return samples;
}

Matrix3 matrix(std::array<double, 9> values) {
    return {{{
        {values[0], values[1], values[2]},
        {values[3], values[4], values[5]},
        {values[6], values[7], values[8]},
    }}};
}

ModelInputs coupledFormulaInputs() {
    return {
        50.0,
        matrix({12.0, 1.0, 0.5,
                1.0, 20.0, 2.0,
                0.5, 2.0, 30.0}),
        {100.0, 200.0, 300.0},
        {4.0, 5.0, 6.0},
        {0.0, 0.0, -9.80665},
    };
}

State coupledFormulaState() {
    constexpr double half_sqrt_two = 0.70710678118654752440;
    return {
        {10.0, -20.0, 30.0},
        {50.0, -4.0, 2.0},
        {half_sqrt_two, 0.0, 0.0, half_sqrt_two},
        {1.0, 2.0, 3.0},
    };
}

ModelInputs translationInputs() {
    return {
        100.0,
        matrix({40.0, 0.0, 0.0,
                0.0, 50.0, 0.0,
                0.0, 0.0, 60.0}),
        {200.0, -100.0, 50.0},
        {0.0, 0.0, 0.0},
        {0.0, 0.0, -9.80665},
    };
}

State translationInitialState() {
    return {
        {100.0, -20.0, 1000.0},
        {50.0, 5.0, 0.0},
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
    };
}

State analyticTranslation(const State& initial,
                          const ModelInputs& inputs,
                          double time_s) {
    const Vec3 acceleration = evaluateFormula(
        initial, inputs).velocity_derivative_i_mps2;
    return {
        add(add(initial.position_i_m,
                scale(initial.velocity_i_mps, time_s)),
            scale(acceleration, 0.5 * time_s * time_s)),
        add(initial.velocity_i_mps, scale(acceleration, time_s)),
        normalize(initial.q_i_b),
        initial.omega_bi_b_radps,
    };
}

ModelInputs spinInputs() {
    return {
        120.0,
        matrix({40.0, 0.0, 0.0,
                0.0, 50.0, 0.0,
                0.0, 0.0, 60.0}),
        {0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
    };
}

State spinInitialState() {
    return {
        {10.0, -5.0, 2.0},
        {3.0, -2.0, 1.0},
        {0.86602540378443864676, 0.0, 0.5, 0.0},
        {0.0, 0.0, 1.3},
    };
}

State analyticSpin(const State& initial, double time_s) {
    const double half_angle = 0.5 * initial.omega_bi_b_radps.z * time_s;
    const Quaternion rotation{
        std::cos(half_angle), 0.0, 0.0, -std::sin(half_angle)};
    return {
        add(initial.position_i_m,
            scale(initial.velocity_i_mps, time_s)),
        initial.velocity_i_mps,
        normalize(hamilton(rotation, normalize(initial.q_i_b))),
        initial.omega_bi_b_radps,
    };
}

double orientationError(const Quaternion& actual_value,
                        const Quaternion& expected_value) {
    const Quaternion actual = normalize(actual_value);
    Quaternion expected = normalize(expected_value);
    if (dot(actual, expected) < 0.0) {
        expected = scale(expected, -1.0);
    }
    const Quaternion difference = add(actual, scale(expected, -1.0));
    const double chord = norm(difference);
    return 4.0 * std::asin(std::min(1.0, 0.5 * chord));
}

double maxAbsDifference(const Vec3& lhs, const Vec3& rhs) {
    return std::max({
        std::abs(lhs.x - rhs.x),
        std::abs(lhs.y - rhs.y),
        std::abs(lhs.z - rhs.z),
    });
}

bool sameState(const State& lhs, const State& rhs,
               double scalar_tolerance, double orientation_tolerance) {
    return maxAbsDifference(lhs.position_i_m, rhs.position_i_m) <=
            scalar_tolerance &&
        maxAbsDifference(lhs.velocity_i_mps, rhs.velocity_i_mps) <=
            scalar_tolerance &&
        maxAbsDifference(lhs.omega_bi_b_radps, rhs.omega_bi_b_radps) <=
            scalar_tolerance &&
        orientationError(lhs.q_i_b, rhs.q_i_b) <= orientation_tolerance;
}

template <typename Function>
bool rejected(Function&& function) {
    try {
        function();
    } catch (const std::domain_error&) {
        return true;
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

ProbeResult runProbe() {
    ProbeResult result;
    result.formula = evaluateFormula(
        coupledFormulaState(), coupledFormulaInputs());

    const State translation_initial = translationInitialState();
    const ModelInputs translation_inputs = translationInputs();
    result.translation_trajectory = integrate(
        translation_initial, translation_inputs, 0.25, 1.0);
    result.translation_terminal_tick = 4;
    result.translation_terminal_time_s = 1.0;
    for (const auto& sample : result.translation_trajectory) {
        require(sameState(
                    sample.state,
                    analyticTranslation(
                        translation_initial, translation_inputs, sample.time_s),
                    kTranslationTolerance, kTranslationTolerance),
                "constant-translation RK4 trajectory differs from analytic truth");
    }

    const State spin_initial = spinInitialState();
    const ModelInputs spin_inputs = spinInputs();
    result.spin_reference_trajectory = integrate(
        spin_initial, spin_inputs, 0.4, 2.0);
    for (const auto& sample : result.spin_reference_trajectory) {
        require(sameState(
                    sample.state, analyticSpin(spin_initial, sample.time_s),
                    kTranslationTolerance,
                    kSpinReferenceOrientationLimit),
                "principal-spin reference trajectory exceeds its error limit");
    }

    const std::array<double, 5> dt_ladder{
        0.4, 0.2, 0.1, 0.05, 0.025};
    std::optional<double> previous_error;
    for (double dt_s : dt_ladder) {
        double maximum_residual = 0.0;
        const auto trajectory = integrate(
            spin_initial, spin_inputs, dt_s, 2.0, &maximum_residual);
        const double error = orientationError(
            trajectory.back().state.q_i_b,
            analyticSpin(spin_initial, 2.0).q_i_b);
        ConvergenceLevel level;
        level.dt_s = dt_s;
        level.orientation_error_rad = error;
        level.maximum_norm_residual = maximum_residual;
        level.final_state = trajectory.back().state;
        if (previous_error.has_value()) {
            level.observed_order = std::log(*previous_error / error) /
                std::log(2.0);
            require(error < *previous_error,
                    "principal-spin orientation error did not decrease");
            require(*level.observed_order >= kMinimumObservedOrder,
                    "principal-spin observed order is below the limit");
        }
        require(maximum_residual <= kQuaternionNormResidualLimit,
                "principal-spin quaternion norm residual exceeds the limit");
        result.convergence.push_back(level);
        previous_error = error;
    }
    require(result.convergence.back().orientation_error_rad <=
                kFinestOrientationErrorLimit,
            "principal-spin finest orientation error exceeds the limit");

    const State failure_initial{
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
    };
    ModelInputs failure_inputs = translationInputs();
    failure_inputs.force_b_n = {0.0, 0.0, 0.0};
    failure_inputs.gravity_i_mps2 = {0.0, 0.0, 0.0};
    const StepResult committed_tick_one = rk4Step(
        failure_initial, failure_inputs, 0.0, 0.5);
    try {
        static_cast<void>(rk4Step(
            committed_tick_one.candidate, failure_inputs,
            0.5, 0.5, 0.75));
        throw std::runtime_error("injected stage failure was accepted");
    } catch (const StageFailure& failure) {
        result.failure = {
            "reference-domain-error",
            failure.stage(),
            failure.evaluationTime(),
            1,
            "discarded",
            1,
            committed_tick_one.candidate,
        };
    }

    ModelInputs invalid_model = translationInputs();
    invalid_model.mass_kg = 0.0;
    if (rejected([&] { validateModel(invalid_model); })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ6-NONPOSITIVE-MASS");
    }

    invalid_model = translationInputs();
    invalid_model.inertia_b_kgm2.values[0][1] = 1.0;
    if (rejected([&] { validateModel(invalid_model); })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ6-ASYMMETRIC-INERTIA");
    }

    invalid_model = translationInputs();
    invalid_model.inertia_b_kgm2.values[1][1] = -1.0;
    if (rejected([&] { validateModel(invalid_model); })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ6-NON-SPD-INERTIA");
    }

    State invalid_state = translationInitialState();
    invalid_state.q_i_b = {0.0, 0.0, 0.0, 0.0};
    if (rejected([&] { validateState(invalid_state); })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ6-ZERO-QUATERNION");
    }

    invalid_model = translationInputs();
    invalid_model.force_b_n.x = std::numeric_limits<double>::infinity();
    if (rejected([&] { validateModel(invalid_model); })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ6-NONFINITE-INPUT");
    }

    if (rejected([] { static_cast<void>(exactGridSteps(1.0, 0.0)); })) {
        result.invalid_input_rejections.push_back("INVALID-YYZ6-ZERO-DT");
    }
    if (rejected([] { static_cast<void>(exactGridSteps(1.0, 0.3)); })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ6-NON-GRID-DURATION");
    }
    require(result.invalid_input_rejections.size() == 7,
            "an invalid YYZ core input was accepted");

    const Vec3 inertia_residual = subtract(
        multiply(coupledFormulaInputs().inertia_b_kgm2,
                 result.formula.omega_derivative_b_radps2),
        result.formula.net_moment_b_nm);
    require(maxAbsDifference(inertia_residual, {0.0, 0.0, 0.0}) <=
                kFormulaTolerance,
            "C++ inertia solve residual exceeds the formula tolerance");
    require(std::abs(dot(coupledFormulaState().q_i_b,
                         result.formula.q_derivative_i_b_per_s)) <=
                kFormulaTolerance,
            "C++ quaternion derivative is not tangent to the unit sphere");
    return result;
}

void writeVec3(const Vec3& value) {
    std::cout << '[' << value.x << ',' << value.y << ',' << value.z << ']';
}

void writeQuaternion(const Quaternion& value) {
    std::cout << '[' << value.w << ',' << value.x << ','
              << value.y << ',' << value.z << ']';
}

void writeState(const State& state) {
    std::cout << "{\"position_I_m\":";
    writeVec3(state.position_i_m);
    std::cout << ",\"velocity_I_mps\":";
    writeVec3(state.velocity_i_mps);
    std::cout << ",\"q_I_B_wxyz\":";
    writeQuaternion(state.q_i_b);
    std::cout << ",\"omega_BI_B_radps\":";
    writeVec3(state.omega_bi_b_radps);
    std::cout << '}';
}

void writeSample(const Sample& sample) {
    std::cout << "{\"tick\":" << sample.tick
              << ",\"time_s\":" << sample.time_s << ',';
    const State& state = sample.state;
    std::cout << "\"position_I_m\":";
    writeVec3(state.position_i_m);
    std::cout << ",\"velocity_I_mps\":";
    writeVec3(state.velocity_i_mps);
    std::cout << ",\"q_I_B_wxyz\":";
    writeQuaternion(state.q_i_b);
    std::cout << ",\"omega_BI_B_radps\":";
    writeVec3(state.omega_bi_b_radps);
    std::cout << '}';
}

void writeTrajectory(const std::vector<Sample>& trajectory) {
    std::cout << '[';
    for (std::size_t index = 0; index < trajectory.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeSample(trajectory[index]);
    }
    std::cout << ']';
}

void writeFormula(const FormulaIntermediates& formula) {
    std::cout << "{\"position_derivative_I_mps\":";
    writeVec3(formula.position_derivative_i_mps);
    std::cout << ",\"force_I_N\":";
    writeVec3(formula.force_i_n);
    std::cout << ",\"mass_reciprocal_per_kg\":"
              << formula.mass_reciprocal_per_kg;
    std::cout << ",\"gravity_I_mps2\":";
    writeVec3(formula.gravity_i_mps2);
    std::cout << ",\"velocity_derivative_I_mps2\":";
    writeVec3(formula.velocity_derivative_i_mps2);
    std::cout << ",\"angular_momentum_B_kgm2ps\":";
    writeVec3(formula.angular_momentum_b_kgm2ps);
    std::cout << ",\"gyroscopic_moment_B_Nm\":";
    writeVec3(formula.gyroscopic_moment_b_nm);
    std::cout << ",\"net_moment_B_Nm\":";
    writeVec3(formula.net_moment_b_nm);
    std::cout << ",\"omega_derivative_B_radps2\":";
    writeVec3(formula.omega_derivative_b_radps2);
    std::cout << ",\"q_derivative_I_B_per_s\":";
    writeQuaternion(formula.q_derivative_i_b_per_s);
    std::cout << '}';
}

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\""
              << ",\"model_id\":\"" << kModelId << "\""
              << ",\"model_choice_status\":\""
              << kModelChoiceStatus << "\""
              << ",\"formula_intermediates\":";
    writeFormula(result.formula);
    std::cout << ",\"translation_trajectory\":";
    writeTrajectory(result.translation_trajectory);
    std::cout << ",\"translation_terminal\":{"
              << "\"kind\":\"duration_exact_grid\""
              << ",\"tick\":" << result.translation_terminal_tick
              << ",\"time_s\":" << result.translation_terminal_time_s
              << '}';
    std::cout << ",\"spin_reference_trajectory\":";
    writeTrajectory(result.spin_reference_trajectory);
    std::cout << ",\"orientation_convergence\":[";
    for (std::size_t index = 0; index < result.convergence.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        const auto& level = result.convergence[index];
        std::cout << "{\"dt_s\":" << level.dt_s
                  << ",\"orientation_error_rad\":"
                  << level.orientation_error_rad
                  << ",\"max_precommit_quaternion_norm_residual\":"
                  << level.maximum_norm_residual
                  << ",\"observed_order\":";
        if (level.observed_order.has_value()) {
            std::cout << *level.observed_order;
        } else {
            std::cout << "null";
        }
        std::cout << ",\"final_state\":";
        writeState(level.final_state);
        std::cout << '}';
    }
    std::cout << "]";
    std::cout << ",\"stage_failure\":{"
              << "\"code\":\"" << result.failure.code << "\""
              << ",\"stage\":\"" << result.failure.stage << "\""
              << ",\"evaluation_time_s\":"
              << result.failure.evaluation_time_s
              << ",\"failed_step_start_tick\":"
              << result.failure.failed_step_start_tick
              << ",\"candidate_disposition\":\""
              << result.failure.candidate_disposition << "\""
              << ",\"last_committed_tick\":"
              << result.failure.last_committed_tick
              << ",\"last_committed_state\":";
    writeState(result.failure.last_committed_state);
    std::cout << '}';
    std::cout << ",\"invalid_input_rejections\":[";
    for (std::size_t index = 0;
         index < result.invalid_input_rejections.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << '\"' << result.invalid_input_rejections[index] << '\"';
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_yyz_6dof_core_probe --self-check\n";
        return 2;
    }
    try {
        writeJson(runProbe());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

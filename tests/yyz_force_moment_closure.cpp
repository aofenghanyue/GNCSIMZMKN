#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId =
    "ORACLE-YYZ-FORCE-MOMENT-CLOSURE-001";
constexpr const char* kModelId =
    "MODEL-YYZ-FORCE-MOMENT-CLOSURE-001";
constexpr const char* kModelChoiceStatus = "accepted";
constexpr const char* kStrategy = "FrozenInterval";
constexpr double kTolerance = 2.0e-12;

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

struct Derivative {
    Vec3 position;
    Vec3 velocity;
    Quaternion attitude;
    Vec3 angular_rate;
};

struct ClosureContext {
    std::string body_frame_id;
    std::int64_t configuration_revision = 0;
    std::size_t valid_from_tick = 0;
    std::size_t valid_until_tick = 0;
};

struct Contribution {
    std::string source_id;
    std::string body_frame_id;
    std::int64_t configuration_revision = 0;
    std::size_t valid_from_tick = 0;
    std::size_t valid_until_tick = 0;
    Vec3 force_b_n;
    Vec3 r_com_to_application_b_m;
    Vec3 moment_at_application_b_nm;
};

struct ClosedContribution {
    std::string source_id;
    Vec3 force_b_n;
    Vec3 r_com_to_application_b_m;
    Vec3 moment_at_application_b_nm;
    Vec3 lever_arm_moment_b_nm;
    Vec3 moment_about_com_b_nm;
};

struct ClosureResult {
    ClosureContext context;
    std::vector<ClosedContribution> contributions;
    Vec3 total_force_b_n;
    Vec3 total_moment_about_com_b_nm;
};

struct RigidInputs {
    double mass_kg = 0.0;
    Matrix3 inertia_b_kgm2;
    Vec3 gravity_i_mps2;
};

struct Sample {
    std::size_t tick = 0;
    double time_s = 0.0;
    State state;
};

struct TrajectoryResult {
    ClosureResult held_closure;
    bool held_through_rk_stages = false;
    Vec3 gravity_i_mps2;
    Vec3 body_force_acceleration_i_mps2;
    Vec3 total_acceleration_i_mps2;
    std::vector<Sample> trajectory;
    std::string terminal_kind;
    std::size_t terminal_tick = 0;
    double terminal_time_s = 0.0;
};

struct ProbeResult {
    ClosureResult formula_closure;
    ClosureResult reversed_order_closure;
    TrajectoryResult rigid_core_trajectory;
    std::vector<std::string> invalid_input_rejections;
    std::vector<std::string> mutation_rejections;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireDomain(bool condition, const std::string& message) {
    if (!condition) {
        throw std::domain_error(message);
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
    requireDomain(finite(value),
                  "q_I_B contains a non-finite coefficient");
    const double magnitude = std::sqrt(dot(value, value));
    requireDomain(finite(magnitude) && magnitude > 0.0,
                  "q_I_B must have nonzero finite norm");
    return scale(value, 1.0 / magnitude);
}

Vec3 passiveRotate(const Quaternion& q_i_b, const Vec3& value_b) {
    requireDomain(finite(value_b),
                  "body vector contains a non-finite component");
    const Quaternion unit = normalize(q_i_b);
    const Quaternion pure{0.0, value_b.x, value_b.y, value_b.z};
    const Quaternion rotated = hamilton(
        hamilton(conjugate(unit), pure), unit);
    requireDomain(finite(rotated),
                  "passive rotation produced a non-finite value");
    return {rotated.x, rotated.y, rotated.z};
}

bool near(double actual, double expected, double tolerance = kTolerance) {
    return std::abs(actual - expected) <=
        tolerance * (1.0 + std::max(std::abs(actual), std::abs(expected)));
}

bool near(const Vec3& actual, const Vec3& expected,
          double tolerance = kTolerance) {
    return near(actual.x, expected.x, tolerance) &&
        near(actual.y, expected.y, tolerance) &&
        near(actual.z, expected.z, tolerance);
}

bool sameContext(const ClosureContext& lhs, const ClosureContext& rhs) {
    return lhs.body_frame_id == rhs.body_frame_id &&
        lhs.configuration_revision == rhs.configuration_revision &&
        lhs.valid_from_tick == rhs.valid_from_tick &&
        lhs.valid_until_tick == rhs.valid_until_tick;
}

void validateContext(const ClosureContext& context) {
    requireDomain(!context.body_frame_id.empty(),
                  "closure body frame identity must be nonempty");
    requireDomain(context.configuration_revision >= 0,
                  "closure configuration revision must be nonnegative");
    requireDomain(context.valid_until_tick > context.valid_from_tick,
                  "closure validity must be a forward half-open interval");
}

ClosureResult closeContributions(
        const ClosureContext& context,
        std::vector<Contribution> contributions,
        bool reverse_application_vector = false) {
    validateContext(context);
    std::sort(
        contributions.begin(), contributions.end(),
        [](const Contribution& lhs, const Contribution& rhs) {
            return lhs.source_id < rhs.source_id;
        });

    ClosureResult result;
    result.context = context;
    for (std::size_t index = 0; index < contributions.size(); ++index) {
        const Contribution& contribution = contributions[index];
        requireDomain(!contribution.source_id.empty(),
                      "closure source identity must be nonempty");
        if (index != 0) {
            requireDomain(
                contributions[index - 1].source_id != contribution.source_id,
                "closure source identity must be unique");
        }
        requireDomain(contribution.body_frame_id == context.body_frame_id,
                      "closure body frame identity differs");
        requireDomain(contribution.configuration_revision ==
                          context.configuration_revision,
                      "closure configuration revision differs");
        requireDomain(contribution.valid_from_tick ==
                          context.valid_from_tick &&
                          contribution.valid_until_tick ==
                          context.valid_until_tick,
                      "closure validity interval differs");
        requireDomain(finite(contribution.force_b_n) &&
                          finite(contribution.r_com_to_application_b_m) &&
                          finite(contribution.moment_at_application_b_nm),
                      "closure contribution contains a non-finite value");

        const Vec3 transported_application = reverse_application_vector
            ? scale(contribution.r_com_to_application_b_m, -1.0)
            : contribution.r_com_to_application_b_m;
        const Vec3 lever_arm_moment = cross(
            transported_application, contribution.force_b_n);
        const Vec3 moment_about_com = add(
            contribution.moment_at_application_b_nm, lever_arm_moment);
        requireDomain(finite(lever_arm_moment) && finite(moment_about_com),
                      "closure transport produced a non-finite value");
        result.contributions.push_back({
            contribution.source_id,
            contribution.force_b_n,
            contribution.r_com_to_application_b_m,
            contribution.moment_at_application_b_nm,
            lever_arm_moment,
            moment_about_com,
        });
        result.total_force_b_n = add(
            result.total_force_b_n, contribution.force_b_n);
        result.total_moment_about_com_b_nm = add(
            result.total_moment_about_com_b_nm, moment_about_com);
    }
    requireDomain(finite(result.total_force_b_n) &&
                      finite(result.total_moment_about_com_b_nm),
                  "closure total contains a non-finite value");
    return result;
}

bool sameClosure(const ClosureResult& lhs, const ClosureResult& rhs) {
    if (!sameContext(lhs.context, rhs.context) ||
        lhs.contributions.size() != rhs.contributions.size() ||
        !near(lhs.total_force_b_n, rhs.total_force_b_n, 0.0) ||
        !near(lhs.total_moment_about_com_b_nm,
              rhs.total_moment_about_com_b_nm, 0.0)) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.contributions.size(); ++index) {
        const ClosedContribution& left = lhs.contributions[index];
        const ClosedContribution& right = rhs.contributions[index];
        if (left.source_id != right.source_id ||
            !near(left.force_b_n, right.force_b_n, 0.0) ||
            !near(left.r_com_to_application_b_m,
                  right.r_com_to_application_b_m, 0.0) ||
            !near(left.moment_at_application_b_nm,
                  right.moment_at_application_b_nm, 0.0) ||
            !near(left.lever_arm_moment_b_nm,
                  right.lever_arm_moment_b_nm, 0.0) ||
            !near(left.moment_about_com_b_nm,
                  right.moment_about_com_b_nm, 0.0)) {
            return false;
        }
    }
    return true;
}

Matrix3 matrix(std::array<double, 9> values) {
    return {{{
        {values[0], values[1], values[2]},
        {values[3], values[4], values[5]},
        {values[6], values[7], values[8]},
    }}};
}

Vec3 multiply(const Matrix3& matrix_value, const Vec3& vector_value) {
    return {
        matrix_value.values[0][0] * vector_value.x +
            matrix_value.values[0][1] * vector_value.y +
            matrix_value.values[0][2] * vector_value.z,
        matrix_value.values[1][0] * vector_value.x +
            matrix_value.values[1][1] * vector_value.y +
            matrix_value.values[1][2] * vector_value.z,
        matrix_value.values[2][0] * vector_value.x +
            matrix_value.values[2][1] * vector_value.y +
            matrix_value.values[2][2] * vector_value.z,
    };
}

std::array<std::array<double, 3>, 3> cholesky(
        const Matrix3& inertia) {
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            requireDomain(finite(inertia.values[row][column]),
                          "inertia contains a non-finite value");
            requireDomain(inertia.values[row][column] ==
                              inertia.values[column][row],
                          "inertia must be symmetric");
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
                requireDomain(finite(residual) && residual > 0.0,
                              "inertia must be positive definite");
                lower[row][column] = std::sqrt(residual);
            } else {
                lower[row][column] = residual / lower[column][column];
            }
        }
    }
    return lower;
}

Vec3 solveSpd(const Matrix3& inertia, const Vec3& rhs) {
    requireDomain(finite(rhs), "inertia solve rhs must be finite");
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
    requireDomain(finite(answer), "inertia solve result must be finite");
    return answer;
}

void validateState(const State& state) {
    requireDomain(finite(state.position_i_m) &&
                      finite(state.velocity_i_mps) &&
                      finite(state.omega_bi_b_radps),
                  "rigid state must be finite");
    static_cast<void>(normalize(state.q_i_b));
}

void validateRigidInputs(const RigidInputs& inputs) {
    requireDomain(finite(inputs.mass_kg) && inputs.mass_kg > 0.0,
                  "mass must be strictly positive");
    requireDomain(finite(inputs.gravity_i_mps2),
                  "gravity must be finite");
    static_cast<void>(cholesky(inputs.inertia_b_kgm2));
}

Derivative derivative(const State& state,
                      const RigidInputs& inputs,
                      const ClosureResult& held_closure) {
    validateState(state);
    validateRigidInputs(inputs);
    const Quaternion attitude = normalize(state.q_i_b);
    const Vec3 force_i = passiveRotate(
        attitude, held_closure.total_force_b_n);
    const Vec3 acceleration = add(
        scale(force_i, 1.0 / inputs.mass_kg),
        inputs.gravity_i_mps2);
    const Vec3 angular_momentum = multiply(
        inputs.inertia_b_kgm2, state.omega_bi_b_radps);
    const Vec3 gyroscopic = cross(
        state.omega_bi_b_radps, angular_momentum);
    const Vec3 angular_acceleration = solveSpd(
        inputs.inertia_b_kgm2,
        subtract(held_closure.total_moment_about_com_b_nm, gyroscopic));
    const Quaternion pure_omega{
        0.0,
        state.omega_bi_b_radps.x,
        state.omega_bi_b_radps.y,
        state.omega_bi_b_radps.z,
    };
    return {
        state.velocity_i_mps,
        acceleration,
        scale(hamilton(pure_omega, attitude), -0.5),
        angular_acceleration,
    };
}

State addScaled(const State& state,
                const Derivative& change,
                double factor) {
    return {
        add(state.position_i_m, scale(change.position, factor)),
        add(state.velocity_i_mps, scale(change.velocity, factor)),
        add(state.q_i_b, scale(change.attitude, factor)),
        add(state.omega_bi_b_radps,
            scale(change.angular_rate, factor)),
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

State rk4Step(const State& committed,
              const RigidInputs& inputs,
              const ClosureResult& held_closure,
              double dt_s) {
    requireDomain(finite(dt_s) && dt_s > 0.0,
                  "RK4 step must be finite and positive");
    const Derivative k1 = derivative(committed, inputs, held_closure);
    const Derivative k2 = derivative(
        addScaled(committed, k1, 0.5 * dt_s), inputs, held_closure);
    const Derivative k3 = derivative(
        addScaled(committed, k2, 0.5 * dt_s), inputs, held_closure);
    const Derivative k4 = derivative(
        addScaled(committed, k3, dt_s), inputs, held_closure);
    State candidate = addScaled(
        committed, weightedDerivative(k1, k2, k3, k4), dt_s);
    candidate.q_i_b = normalize(candidate.q_i_b);
    validateState(candidate);
    return candidate;
}

std::size_t exactGridSteps(double duration_s, double dt_s) {
    requireDomain(finite(duration_s) && finite(dt_s) &&
                      duration_s >= 0.0 && dt_s > 0.0,
                  "duration and step must form a nonnegative positive grid");
    const double quotient = duration_s / dt_s;
    const double rounded = std::round(quotient);
    requireDomain(std::abs(quotient - rounded) <=
                      1.0e-13 * std::max(1.0, std::abs(quotient)),
                  "duration must align to ExactGrid");
    return static_cast<std::size_t>(rounded);
}

ClosureContext formulaContext() {
    return {"frame.fixture.yyz.body@1", 7, 10, 11};
}

std::vector<Contribution> formulaContributions() {
    return {
        {
            "aero.body",
            "frame.fixture.yyz.body@1",
            7,
            10,
            11,
            {10.0, -20.0, 30.0},
            {2.0, -1.0, 0.5},
            {1.0, 2.0, 3.0},
        },
        {
            "propulsion.main",
            "frame.fixture.yyz.body@1",
            7,
            10,
            11,
            {-5.0, 4.0, 2.0},
            {-1.0, 0.25, 3.0},
            {-2.0, 1.0, 0.5},
        },
    };
}

ClosureContext trajectoryContext() {
    return {"frame.fixture.yyz.body@1", 3, 0, 4};
}

std::vector<Contribution> trajectoryContributions() {
    return {
        {
            "propulsion.main",
            "frame.fixture.yyz.body@1",
            3,
            0,
            4,
            {150.0, 0.0, 0.0},
            {0.0, 0.4, 0.0},
            {0.0, 0.0, 0.0},
        },
        {
            "aero.body",
            "frame.fixture.yyz.body@1",
            3,
            0,
            4,
            {-30.0, 0.0, 0.0},
            {0.0, -1.0, 0.0},
            {0.0, 0.0, 90.0},
        },
    };
}

RigidInputs trajectoryRigidInputs() {
    return {
        60.0,
        matrix({
            40.0, 0.0, 0.0,
            0.0, 50.0, 0.0,
            0.0, 0.0, 60.0,
        }),
        {0.0, 0.0, -9.80665},
    };
}

State trajectoryInitialState() {
    return {
        {100.0, -20.0, 1000.0},
        {50.0, 5.0, 0.0},
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
    };
}

TrajectoryResult integrateTrajectory(const ClosureResult& held_closure) {
    constexpr double dt_s = 0.25;
    constexpr double duration_s = 1.0;
    const std::size_t steps = exactGridSteps(duration_s, dt_s);
    require(held_closure.context.valid_from_tick == 0 &&
                held_closure.context.valid_until_tick == steps,
            "held closure interval does not cover the integration steps");
    const RigidInputs inputs = trajectoryRigidInputs();
    const State initial = trajectoryInitialState();
    State committed = initial;
    committed.q_i_b = normalize(committed.q_i_b);

    TrajectoryResult result;
    result.held_closure = held_closure;
    result.held_through_rk_stages = true;
    result.gravity_i_mps2 = inputs.gravity_i_mps2;
    result.body_force_acceleration_i_mps2 = scale(
        passiveRotate(committed.q_i_b, held_closure.total_force_b_n),
        1.0 / inputs.mass_kg);
    result.total_acceleration_i_mps2 = add(
        result.body_force_acceleration_i_mps2, inputs.gravity_i_mps2);
    result.trajectory.push_back({0, 0.0, committed});
    for (std::size_t tick = 0; tick < steps; ++tick) {
        committed = rk4Step(committed, inputs, held_closure, dt_s);
        result.trajectory.push_back({
            tick + 1,
            static_cast<double>(tick + 1) * dt_s,
            committed,
        });
    }
    result.terminal_kind = "duration_exact_grid";
    result.terminal_tick = steps;
    result.terminal_time_s = duration_s;
    return result;
}

State analyticTrajectoryState(double time_s) {
    const State initial = trajectoryInitialState();
    const Vec3 acceleration{2.0, 0.0, -9.80665};
    return {
        add(add(initial.position_i_m,
                scale(initial.velocity_i_mps, time_s)),
            scale(acceleration, 0.5 * time_s * time_s)),
        add(initial.velocity_i_mps, scale(acceleration, time_s)),
        initial.q_i_b,
        initial.omega_bi_b_radps,
    };
}

bool sameState(const State& lhs, const State& rhs) {
    return near(lhs.position_i_m, rhs.position_i_m) &&
        near(lhs.velocity_i_mps, rhs.velocity_i_mps) &&
        near(lhs.q_i_b.w, rhs.q_i_b.w) &&
        near(lhs.q_i_b.x, rhs.q_i_b.x) &&
        near(lhs.q_i_b.y, rhs.q_i_b.y) &&
        near(lhs.q_i_b.z, rhs.q_i_b.z) &&
        near(lhs.omega_bi_b_radps, rhs.omega_bi_b_radps);
}

template <typename Function>
bool rejected(Function&& function) {
    try {
        function();
    } catch (const std::domain_error&) {
        return true;
    }
    return false;
}

ProbeResult runProbe() {
    ProbeResult result;
    const ClosureContext formula_context = formulaContext();
    const std::vector<Contribution> formula_contributions =
        formulaContributions();
    result.formula_closure = closeContributions(
        formula_context, formula_contributions);
    std::vector<Contribution> reversed_contributions = formula_contributions;
    std::reverse(reversed_contributions.begin(), reversed_contributions.end());
    result.reversed_order_closure = closeContributions(
        formula_context, reversed_contributions);
    require(sameClosure(
                result.formula_closure, result.reversed_order_closure),
            "contribution order changed the canonical closure");
    require(near(result.formula_closure.total_force_b_n,
                 {5.0, -16.0, 32.0}) &&
                near(result.formula_closure.total_moment_about_com_b_nm,
                     {-32.5, -65.0, -29.25}),
            "formula closure differs from the fixture truth");
    require(result.formula_closure.contributions.size() == 2 &&
                result.formula_closure.contributions[0].source_id ==
                    "aero.body" &&
                near(result.formula_closure.contributions[0].
                         lever_arm_moment_b_nm,
                     {-20.0, -55.0, -30.0}) &&
                near(result.formula_closure.contributions[1].
                         lever_arm_moment_b_nm,
                     {-11.5, -13.0, -2.75}),
            "lever-arm transport differs from the fixture truth");

    const ClosureResult trajectory_closure = closeContributions(
        trajectoryContext(), trajectoryContributions());
    require(near(trajectory_closure.total_force_b_n, {120.0, 0.0, 0.0}) &&
                near(trajectory_closure.total_moment_about_com_b_nm,
                     {0.0, 0.0, 0.0}),
            "trajectory closure differs from the fixture truth");
    result.rigid_core_trajectory = integrateTrajectory(trajectory_closure);
    for (const Sample& sample : result.rigid_core_trajectory.trajectory) {
        require(sameState(sample.state, analyticTrajectoryState(sample.time_s)),
                "closure-fed RK4 trajectory differs from analytic truth");
    }
    require(result.rigid_core_trajectory.terminal_tick == 4 &&
                near(result.rigid_core_trajectory.terminal_time_s, 1.0),
            "closure-fed trajectory termination differs");

    std::vector<Contribution> invalid = formula_contributions;
    invalid[1].source_id = invalid[0].source_id;
    if (rejected([&] {
            static_cast<void>(closeContributions(formula_context, invalid));
        })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ-CLOSURE-DUPLICATE-SOURCE");
    }

    invalid = formula_contributions;
    invalid[0].body_frame_id = "frame.other@1";
    if (rejected([&] {
            static_cast<void>(closeContributions(formula_context, invalid));
        })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ-CLOSURE-FRAME-MISMATCH");
    }

    invalid = formula_contributions;
    ++invalid[0].configuration_revision;
    if (rejected([&] {
            static_cast<void>(closeContributions(formula_context, invalid));
        })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ-CLOSURE-REVISION-MISMATCH");
    }

    invalid = formula_contributions;
    ++invalid[0].valid_until_tick;
    if (rejected([&] {
            static_cast<void>(closeContributions(formula_context, invalid));
        })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ-CLOSURE-INTERVAL-MISMATCH");
    }

    invalid = formula_contributions;
    invalid[0].force_b_n.x = std::numeric_limits<double>::infinity();
    if (rejected([&] {
            static_cast<void>(closeContributions(formula_context, invalid));
        })) {
        result.invalid_input_rejections.push_back(
            "INVALID-YYZ-CLOSURE-NONFINITE-FORCE");
    }
    require(result.invalid_input_rejections.size() == 5,
            "an invalid closure input was accepted");

    const ClosureResult reversed_vector = closeContributions(
        formula_context, formula_contributions, true);
    if (!near(reversed_vector.total_moment_about_com_b_nm,
              result.formula_closure.total_moment_about_com_b_nm)) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-CLOSURE-REVERSED-APPLICATION-VECTOR");
    }

    ClosureResult gravity_double_count = trajectory_closure;
    gravity_double_count.total_force_b_n = add(
        gravity_double_count.total_force_b_n,
        scale(trajectoryRigidInputs().gravity_i_mps2,
              trajectoryRigidInputs().mass_kg));
    const TrajectoryResult gravity_mutation =
        integrateTrajectory(gravity_double_count);
    if (!sameState(gravity_mutation.trajectory.back().state,
                   result.rigid_core_trajectory.trajectory.back().state)) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-CLOSURE-GRAVITY-DOUBLE-COUNT");
    }
    require(result.mutation_rejections.size() == 2,
            "a closure physical mutation matched the accepted result");
    return result;
}

void writeVec3(const Vec3& value) {
    std::cout << '[' << value.x << ',' << value.y << ',' << value.z << ']';
}

void writeQuaternion(const Quaternion& value) {
    std::cout << '[' << value.w << ',' << value.x << ','
              << value.y << ',' << value.z << ']';
}

void writeContext(const ClosureContext& context) {
    std::cout << "{\"body_frame_id\":\"" << context.body_frame_id
              << "\",\"configuration_revision\":"
              << context.configuration_revision
              << ",\"valid_from_tick\":" << context.valid_from_tick
              << ",\"valid_until_tick\":" << context.valid_until_tick
              << '}';
}

void writeClosure(const ClosureResult& closure) {
    std::cout << "{\"strategy\":\"" << kStrategy << "\",\"context\":";
    writeContext(closure.context);
    std::cout << ",\"contributions\":[";
    for (std::size_t index = 0;
         index < closure.contributions.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        const ClosedContribution& contribution =
            closure.contributions[index];
        std::cout << "{\"source_id\":\"" << contribution.source_id
                  << "\",\"force_B_N\":";
        writeVec3(contribution.force_b_n);
        std::cout << ",\"r_CoM_to_application_B_m\":";
        writeVec3(contribution.r_com_to_application_b_m);
        std::cout << ",\"moment_at_application_B_Nm\":";
        writeVec3(contribution.moment_at_application_b_nm);
        std::cout << ",\"lever_arm_moment_B_Nm\":";
        writeVec3(contribution.lever_arm_moment_b_nm);
        std::cout << ",\"moment_about_CoM_B_Nm\":";
        writeVec3(contribution.moment_about_com_b_nm);
        std::cout << '}';
    }
    std::cout << "],\"total_force_B_N\":";
    writeVec3(closure.total_force_b_n);
    std::cout << ",\"total_moment_about_CoM_B_Nm\":";
    writeVec3(closure.total_moment_about_com_b_nm);
    std::cout << '}';
}

void writeSample(const Sample& sample) {
    std::cout << "{\"tick\":" << sample.tick
              << ",\"time_s\":" << sample.time_s
              << ",\"position_I_m\":";
    writeVec3(sample.state.position_i_m);
    std::cout << ",\"velocity_I_mps\":";
    writeVec3(sample.state.velocity_i_mps);
    std::cout << ",\"q_I_B_wxyz\":";
    writeQuaternion(sample.state.q_i_b);
    std::cout << ",\"omega_BI_B_radps\":";
    writeVec3(sample.state.omega_bi_b_radps);
    std::cout << '}';
}

void writeTrajectory(const TrajectoryResult& trajectory) {
    std::cout << "{\"strategy\":\"" << kStrategy
              << "\",\"held_through_rk_stages\":"
              << (trajectory.held_through_rk_stages ? "true" : "false")
              << ",\"held_closure\":";
    writeClosure(trajectory.held_closure);
    std::cout << ",\"gravity_I_mps2\":";
    writeVec3(trajectory.gravity_i_mps2);
    std::cout << ",\"body_force_acceleration_I_mps2\":";
    writeVec3(trajectory.body_force_acceleration_i_mps2);
    std::cout << ",\"total_acceleration_I_mps2\":";
    writeVec3(trajectory.total_acceleration_i_mps2);
    std::cout << ",\"trajectory\":[";
    for (std::size_t index = 0;
         index < trajectory.trajectory.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeSample(trajectory.trajectory[index]);
    }
    std::cout << "],\"terminal\":{\"kind\":\""
              << trajectory.terminal_kind << "\",\"tick\":"
              << trajectory.terminal_tick << ",\"time_s\":"
              << trajectory.terminal_time_s << "}}";
}

void writeStringList(const std::vector<std::string>& values) {
    std::cout << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << '"' << values[index] << '"';
    }
    std::cout << ']';
}

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\""
              << ",\"model_id\":\"" << kModelId << "\""
              << ",\"model_choice_status\":\""
              << kModelChoiceStatus << "\""
              << ",\"formula_closure\":";
    writeClosure(result.formula_closure);
    std::cout << ",\"reversed_order_closure\":";
    writeClosure(result.reversed_order_closure);
    std::cout << ",\"rigid_core_trajectory\":";
    writeTrajectory(result.rigid_core_trajectory);
    std::cout << ",\"invalid_input_rejections\":";
    writeStringList(result.invalid_input_rejections);
    std::cout << ",\"mutation_rejections\":";
    writeStringList(result.mutation_rejections);
    std::cout << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr <<
            "usage: gnc_yyz_force_moment_closure_probe --self-check\n";
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

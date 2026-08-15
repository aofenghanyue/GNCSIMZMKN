#include "gnc/foundation/fixed_rk4.hpp"
#include "gnc/foundation/passive_quaternion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
using gnc::foundation::Mat3;
using gnc::foundation::NumericalFlag;
using gnc::foundation::NumericalFlags;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;
using gnc::foundation::QuaternionNormalizationPolicy;
using gnc::foundation::QuaternionPolicy;
using gnc::foundation::QuaternionStorage;
using gnc::foundation::Vec3;

constexpr std::string_view kSchema =
    "gnczmkn.foundation-passive-quaternion-probe/1";
constexpr std::string_view kComponentId =
    "GNC-FOUNDATION-PASSIVE-QUATERNION-001";
constexpr std::size_t kPropertySamples = 256U;

struct FixedObservation {
    std::string id;
    std::vector<double> result;
};

struct PropertyMetrics {
    double max_matrix_vector_error = 0.0;
    double max_orthogonality_error = 0.0;
    double max_determinant_error = 0.0;
    double max_sign_equivalence_error = 0.0;
    double max_composition_error = 0.0;
    double max_inverse_round_trip_error = 0.0;
    double max_derivative_equivalence_error = 0.0;
    double max_orientation_sign_error_rad = 0.0;
};

struct SpinLevel {
    double dt_s = 0.0;
    double orientation_error_rad = 0.0;
    double max_precommit_norm_residual = 0.0;
    std::optional<double> observed_order;
    QuaternionStorage final_quaternion;
};

struct FailureObservation {
    std::string id;
    NumericalStatus status = NumericalStatus::InternalFailure;
    std::string detail;
    bool has_value = false;
};

struct NormalizationObservation {
    NumericalStatus status = NumericalStatus::InternalFailure;
    NumericalFlags flags = 0U;
    std::string detail;
    QuaternionStorage quaternion;
};

struct Bundle {
    QuaternionPolicy policy;
    std::vector<FixedObservation> fixed_cases;
    PropertyMetrics properties;
    Vec3 yyz_force_i = Vec3::Zero();
    QuaternionStorage yyz_q_derivative;
    std::vector<SpinLevel> spin_convergence;
    NormalizationObservation normalization;
    std::vector<FailureObservation> failures;
};

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

QuaternionStorage quaternion(double w, double x, double y, double z) {
    return gnc::foundation::quaternion_from_wxyz(w, x, y, z);
}

Vec3 vector(double x, double y, double z) {
    return Vec3{x, y, z};
}

QuaternionPolicy product_policy(
    QuaternionNormalizationPolicy normalization =
        QuaternionNormalizationPolicy::Error) {
    QuaternionPolicy policy;
    policy.numerical.absolute_tolerance = 2.0e-12;
    policy.numerical.relative_tolerance = 2.0e-12;
    policy.numerical.finite_check = FiniteCheck::EveryStage;
    policy.numerical.zero_tolerance = 1.0e-14;
    policy.numerical.condition_limit = 1.0e12;
    policy.normalization = normalization;
    return policy;
}

double max_difference(const Vec3& lhs, const Vec3& rhs) {
    return std::max({std::abs(lhs(0) - rhs(0)),
                     std::abs(lhs(1) - rhs(1)),
                     std::abs(lhs(2) - rhs(2))});
}

double max_difference(const QuaternionStorage& lhs,
                      const QuaternionStorage& rhs) {
    const auto left = gnc::foundation::quaternion_to_wxyz(lhs);
    const auto right = gnc::foundation::quaternion_to_wxyz(rhs);
    double result = 0.0;
    for (std::size_t index = 0U; index < left.size(); ++index) {
        result = std::max(result, std::abs(left[index] - right[index]));
    }
    return result;
}

double max_abs(const Mat3& matrix) {
    double result = 0.0;
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = 0; column < 3; ++column) {
            result = std::max(result, std::abs(matrix(row, column)));
        }
    }
    return result;
}

bool near(double actual, double expected, double absolute = 2.0e-12,
          double relative = 2.0e-12) {
    const double scale =
        std::max({1.0, std::abs(actual), std::abs(expected)});
    return std::isfinite(actual) && std::isfinite(expected) &&
           std::abs(actual - expected) <= absolute + relative * scale;
}

bool near(const Vec3& actual, const Vec3& expected,
          double absolute = 2.0e-12, double relative = 2.0e-12) {
    return near(actual(0), expected(0), absolute, relative) &&
           near(actual(1), expected(1), absolute, relative) &&
           near(actual(2), expected(2), absolute, relative);
}

bool near(const QuaternionStorage& actual,
          const QuaternionStorage& expected,
          double absolute = 2.0e-12, double relative = 2.0e-12) {
    const auto actual_values =
        gnc::foundation::quaternion_to_wxyz(actual);
    const auto expected_values =
        gnc::foundation::quaternion_to_wxyz(expected);
    for (std::size_t index = 0U; index < actual_values.size(); ++index) {
        if (!near(actual_values[index], expected_values[index], absolute,
                  relative)) {
            return false;
        }
    }
    return true;
}

template <typename Value>
const Value& require_value(const NumericalOutcome<Value>& outcome,
                           std::string_view message) {
    require(outcome.has_value(), message);
    return outcome.value();
}

std::vector<double> values(const Vec3& value) {
    return {value(0), value(1), value(2)};
}

std::vector<double> values(const QuaternionStorage& value) {
    const auto coefficients =
        gnc::foundation::quaternion_to_wxyz(value);
    return {coefficients.begin(), coefficients.end()};
}

std::vector<double> row_major(const Mat3& value) {
    std::vector<double> result;
    result.reserve(9U);
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = 0; column < 3; ++column) {
            result.push_back(value(row, column));
        }
    }
    return result;
}

class DeterministicGenerator {
  public:
    [[nodiscard]] double symmetric() {
        state_ = state_ * UINT64_C(6364136223846793005) +
                 UINT64_C(1442695040888963407);
        const double unit = static_cast<double>(state_ >> 11U) *
                            0x1.0p-53;
        return 2.0 * unit - 1.0;
    }

    [[nodiscard]] QuaternionStorage unit_quaternion() {
        QuaternionStorage result = quaternion(
            symmetric(), symmetric(), symmetric(), symmetric());
        const auto prepared = gnc::foundation::prepare_passive_quaternion(
            result, product_policy(
                        QuaternionNormalizationPolicy::NormalizeWithFlag));
        require(prepared.has_value(), "random quaternion preparation failed");
        return prepared.value();
    }

    [[nodiscard]] Vec3 vector3() {
        return vector(4.0 * symmetric(), 4.0 * symmetric(),
                      4.0 * symmetric());
    }

  private:
    std::uint64_t state_ = UINT64_C(0x3243f6a8885a308d);
};

void add_fixed_cases(Bundle& bundle) {
    const double root_half = std::sqrt(0.5);
    const QuaternionStorage z90 = quaternion(root_half, 0.0, 0.0,
                                             root_half);
    const QuaternionStorage x90 = quaternion(root_half, root_half, 0.0,
                                             0.0);
    const QuaternionStorage x180 = quaternion(0.0, 1.0, 0.0, 0.0);

    const auto z90_rotate = gnc::foundation::rotate_passive(
        z90, vector(1.0, 0.0, 0.0), bundle.policy);
    require(near(require_value(z90_rotate, "z90 rotation failed"),
                 vector(0.0, -1.0, 0.0)),
            "z90 passive rotation differs");
    bundle.fixed_cases.push_back(
        {"quaternion.rotate-z90-x", values(z90_rotate.value())});

    const auto x180_rotate = gnc::foundation::rotate_passive(
        x180, vector(0.0, 1.0, 0.0), bundle.policy);
    require(near(require_value(x180_rotate, "x180 rotation failed"),
                 vector(0.0, -1.0, 0.0)),
            "x180 passive rotation differs");
    bundle.fixed_cases.push_back(
        {"quaternion.rotate-x180-y", values(x180_rotate.value())});

    const auto composed =
        gnc::foundation::compose_passive(x90, z90, bundle.policy);
    const auto composition_rotate = gnc::foundation::rotate_passive(
        require_value(composed, "composition failed"),
        vector(1.0, 2.0, 3.0), bundle.policy);
    require(near(require_value(composition_rotate,
                               "composition rotation failed"),
                 vector(2.0, 3.0, 1.0)),
            "passive composition differs");
    bundle.fixed_cases.push_back(
        {"quaternion.compose-z90-then-x90",
         values(composition_rotate.value())});

    const auto product = gnc::foundation::hamilton_product(
        z90, x90, bundle.policy.numerical);
    require(near(require_value(product, "Hamilton product failed"),
                 quaternion(0.5, 0.5, 0.5, 0.5)),
            "Hamilton coefficient order differs");
    bundle.fixed_cases.push_back(
        {"quaternion.hamilton-composition-coefficients",
         values(product.value())});

    const Vec3 round_trip_input = vector(2.0, -3.0, 5.0);
    const auto rotated = gnc::foundation::rotate_passive(
        z90, round_trip_input, bundle.policy);
    const auto inverse = gnc::foundation::inverse_passive_quaternion(
        z90, bundle.policy);
    const auto round_trip = gnc::foundation::rotate_passive(
        require_value(inverse, "inverse failed"),
        require_value(rotated, "round-trip forward rotation failed"),
        bundle.policy);
    require(near(require_value(round_trip, "round-trip inverse failed"),
                 round_trip_input),
            "inverse round trip differs");
    bundle.fixed_cases.push_back(
        {"quaternion.inverse-round-trip", values(round_trip.value())});

    const auto matrix =
        gnc::foundation::passive_rotation_matrix(z90, bundle.policy);
    require(matrix.has_value(), "passive matrix failed");
    const std::vector<double> expected_matrix{
        0.0, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0};
    const std::vector<double> actual_matrix = row_major(matrix.value());
    require(actual_matrix.size() == expected_matrix.size(),
            "passive matrix size differs");
    for (std::size_t index = 0U; index < actual_matrix.size(); ++index) {
        require(near(actual_matrix[index], expected_matrix[index]),
                "passive matrix coefficient differs");
    }
    bundle.fixed_cases.push_back(
        {"quaternion.matrix-row-major-z90", actual_matrix});

    const QuaternionStorage serialized =
        quaternion(0.5, -0.5, 0.5, -0.5);
    const auto explicit_wxyz =
        gnc::foundation::quaternion_to_wxyz(serialized);
    require(explicit_wxyz == std::array<double, 4>{0.5, -0.5, 0.5, -0.5},
            "explicit wxyz mapping differs");
    require(serialized.coeffs()(0) == -0.5 &&
                serialized.coeffs()(1) == 0.5 &&
                serialized.coeffs()(2) == -0.5 &&
                serialized.coeffs()(3) == 0.5,
            "Eigen coefficient layout precondition differs");
    bundle.fixed_cases.push_back(
        {"quaternion.serialization-wxyz", values(serialized)});

    const QuaternionStorage derivative_q =
        quaternion(0.5, 0.5, 0.5, 0.5);
    const auto body_derivative =
        gnc::foundation::passive_quaternion_body_rate_derivative(
            derivative_q, vector(1.0, 2.0, 3.0), bundle.policy);
    require(near(require_value(body_derivative,
                               "body-rate derivative failed"),
                 quaternion(1.5, 0.0, -1.0, -0.5)),
            "body-rate derivative differs");
    bundle.fixed_cases.push_back(
        {"quaternion.body-rate-derivative",
         values(body_derivative.value())});

    const auto inertial_derivative =
        gnc::foundation::passive_quaternion_inertial_rate_derivative(
            derivative_q, vector(2.0, 3.0, 1.0), bundle.policy);
    require(near(require_value(inertial_derivative,
                               "inertial-rate derivative failed"),
                 quaternion(1.5, 0.0, -1.0, -0.5)),
            "inertial-rate derivative differs");
    bundle.fixed_cases.push_back(
        {"quaternion.inertial-rate-derivative",
         values(inertial_derivative.value())});
}

PropertyMetrics run_properties(const QuaternionPolicy& policy) {
    DeterministicGenerator generator;
    PropertyMetrics metrics;
    for (std::size_t sample = 0U; sample < kPropertySamples; ++sample) {
        const QuaternionStorage q_to_from = generator.unit_quaternion();
        const Vec3 vector_from = generator.vector3();
        const auto rotated = gnc::foundation::rotate_passive(
            q_to_from, vector_from, policy);
        const auto matrix = gnc::foundation::passive_rotation_matrix(
            q_to_from, policy);
        require(rotated.has_value() && matrix.has_value(),
                "random passive rotation failed");
        metrics.max_matrix_vector_error = std::max(
            metrics.max_matrix_vector_error,
            max_difference(rotated.value(), matrix.value() * vector_from));
        metrics.max_orthogonality_error = std::max(
            metrics.max_orthogonality_error,
            max_abs(matrix.value() * matrix.value().transpose() -
                    Mat3::Identity()));
        metrics.max_determinant_error = std::max(
            metrics.max_determinant_error,
            std::abs(matrix.value().determinant() - 1.0));

        QuaternionStorage negative = q_to_from;
        negative.coeffs() *= -1.0;
        const auto negative_rotate = gnc::foundation::rotate_passive(
            negative, vector_from, policy);
        const auto sign_angle =
            gnc::foundation::passive_quaternion_orientation_error(
                q_to_from, negative, policy);
        require(negative_rotate.has_value() && sign_angle.has_value(),
                "quaternion sign property failed");
        metrics.max_sign_equivalence_error = std::max(
            metrics.max_sign_equivalence_error,
            max_difference(rotated.value(), negative_rotate.value()));
        metrics.max_orientation_sign_error_rad = std::max(
            metrics.max_orientation_sign_error_rad, sign_angle.value());

        const QuaternionStorage q_b_from_a = generator.unit_quaternion();
        const QuaternionStorage q_c_from_b = generator.unit_quaternion();
        const Vec3 vector_a = generator.vector3();
        const auto vector_b = gnc::foundation::rotate_passive(
            q_b_from_a, vector_a, policy);
        const auto vector_c_sequential = gnc::foundation::rotate_passive(
            q_c_from_b,
            require_value(vector_b, "random first rotation failed"),
            policy);
        const auto q_c_from_a = gnc::foundation::compose_passive(
            q_c_from_b, q_b_from_a, policy);
        const auto vector_c_composed = gnc::foundation::rotate_passive(
            require_value(q_c_from_a, "random composition failed"),
            vector_a, policy);
        require(vector_c_sequential.has_value() &&
                    vector_c_composed.has_value(),
                "random composed rotation failed");
        metrics.max_composition_error = std::max(
            metrics.max_composition_error,
            max_difference(vector_c_sequential.value(),
                           vector_c_composed.value()));

        const auto inverse = gnc::foundation::inverse_passive_quaternion(
            q_to_from, policy);
        const auto inverse_round_trip = gnc::foundation::rotate_passive(
            require_value(inverse, "random inverse failed"),
            rotated.value(), policy);
        require(inverse_round_trip.has_value(),
                "random inverse round trip failed");
        metrics.max_inverse_round_trip_error = std::max(
            metrics.max_inverse_round_trip_error,
            max_difference(vector_from, inverse_round_trip.value()));

        const Vec3 omega_b = generator.vector3();
        const auto omega_i = gnc::foundation::rotate_passive(
            q_to_from, omega_b, policy);
        const auto body_derivative =
            gnc::foundation::passive_quaternion_body_rate_derivative(
                q_to_from, omega_b, policy);
        const auto inertial_derivative =
            gnc::foundation::passive_quaternion_inertial_rate_derivative(
                q_to_from,
                require_value(omega_i, "angular-rate rotation failed"),
                policy);
        require(body_derivative.has_value() &&
                    inertial_derivative.has_value(),
                "random derivative equivalence failed");
        metrics.max_derivative_equivalence_error = std::max(
            metrics.max_derivative_equivalence_error,
            max_difference(body_derivative.value(),
                           inertial_derivative.value()));
    }

    require(metrics.max_matrix_vector_error <= 8.0e-15,
            "matrix/vector property exceeds tolerance");
    require(metrics.max_orthogonality_error <= 2.0e-15,
            "orthogonality property exceeds tolerance");
    require(metrics.max_determinant_error <= 3.0e-15,
            "determinant property exceeds tolerance");
    require(metrics.max_sign_equivalence_error <= 1.0e-15,
            "sign property exceeds tolerance");
    require(metrics.max_composition_error <= 1.0e-14,
            "composition property exceeds tolerance");
    require(metrics.max_inverse_round_trip_error <= 8.0e-15,
            "inverse property exceeds tolerance");
    require(metrics.max_derivative_equivalence_error <= 4.0e-15,
            "derivative equivalence exceeds tolerance");
    require(metrics.max_orientation_sign_error_rad <= 1.0e-15,
            "orientation sign property exceeds tolerance");
    return metrics;
}

std::array<double, 4> state_from_quaternion(
    const QuaternionStorage& quaternion_value) {
    return gnc::foundation::quaternion_to_wxyz(quaternion_value);
}

QuaternionStorage quaternion_from_state(
    const std::array<double, 4>& state) {
    return gnc::foundation::quaternion_from_wxyz(state);
}

std::vector<SpinLevel> run_principal_spin(
    const QuaternionPolicy& policy) {
    const QuaternionStorage initial = quaternion(
        0.8660254037844386, 0.0, 0.5, 0.0);
    const Vec3 omega_b = vector(0.0, 0.0, 1.3);
    const double duration_s = 2.0;
    const QuaternionStorage rotation = quaternion(
        std::cos(0.5 * omega_b(2) * duration_s), 0.0, 0.0,
        -std::sin(0.5 * omega_b(2) * duration_s));
    const auto analytic_product = gnc::foundation::hamilton_product(
        rotation, initial, policy.numerical);
    require(analytic_product.has_value(),
            "principal-spin analytic product failed");
    const auto analytic = gnc::foundation::prepare_passive_quaternion(
        analytic_product.value(),
        product_policy(QuaternionNormalizationPolicy::NormalizeWithFlag));
    require(analytic.has_value(), "principal-spin analytic prepare failed");

    std::vector<SpinLevel> levels;
    std::optional<double> previous_error;
    for (double dt_s : {0.4, 0.2, 0.1, 0.05, 0.025}) {
        std::array<double, 4> state = state_from_quaternion(initial);
        double maximum_norm_residual = 0.0;
        const auto derivative =
            [&](double, const std::array<double, 4>& stage_state) {
                const auto q_dot =
                    gnc::foundation::passive_quaternion_body_rate_derivative(
                        quaternion_from_state(stage_state), omega_b,
                        product_policy(
                            QuaternionNormalizationPolicy::NormalizeWithFlag));
                if (!q_dot.has_value()) {
                    return NumericalOutcome<std::array<double, 4>>::failure(
                        q_dot.status(), q_dot.evidence());
                }
                return NumericalOutcome<std::array<double, 4>>::with_value(
                    q_dot.status(), state_from_quaternion(q_dot.value()),
                    q_dot.evidence());
            };
        const std::size_t steps = static_cast<std::size_t>(
            std::llround(duration_s / dt_s));
        double time_s = 0.0;
        for (std::size_t step = 0U; step < steps; ++step) {
            const auto candidate = gnc::foundation::fixed_rk4_step(
                state, time_s, dt_s, derivative, policy.numerical);
            require(candidate.has_value(), "principal-spin RK4 step failed");
            const QuaternionStorage candidate_q =
                quaternion_from_state(candidate.value());
            maximum_norm_residual = std::max(
                maximum_norm_residual,
                std::abs(std::sqrt(candidate_q.squaredNorm()) - 1.0));
            const auto committed =
                gnc::foundation::prepare_passive_quaternion(
                    candidate_q,
                    product_policy(
                        QuaternionNormalizationPolicy::NormalizeWithFlag));
            require(committed.has_value(),
                    "principal-spin candidate normalization failed");
            state = state_from_quaternion(committed.value());
            time_s += dt_s;
        }

        const QuaternionStorage final_quaternion =
            quaternion_from_state(state);
        const auto error =
            gnc::foundation::passive_quaternion_orientation_error(
                final_quaternion, analytic.value(), policy);
        require(error.has_value(), "principal-spin orientation error failed");
        SpinLevel level;
        level.dt_s = dt_s;
        level.orientation_error_rad = error.value();
        level.max_precommit_norm_residual = maximum_norm_residual;
        level.final_quaternion = final_quaternion;
        if (previous_error.has_value()) {
            require(error.value() > 0.0 &&
                        error.value() < *previous_error,
                    "principal-spin error did not decrease");
            level.observed_order =
                std::log(*previous_error / error.value()) / std::log(2.0);
            require(*level.observed_order >= 3.8,
                    "principal-spin observed order is below 3.8");
        }
        require(maximum_norm_residual <= 1.0e-4,
                "principal-spin norm residual exceeds fixture limit");
        previous_error = error.value();
        levels.push_back(level);
    }
    require(levels.back().orientation_error_rad <= 1.0e-8,
            "principal-spin finest error exceeds fixture limit");
    return levels;
}

FailureObservation failure(std::string id, NumericalStatus status,
                           std::string_view detail, bool has_value) {
    return {std::move(id), status, std::string(detail), has_value};
}

Bundle run_bundle() {
    Bundle bundle;
    bundle.policy = product_policy();
    add_fixed_cases(bundle);
    bundle.properties = run_properties(bundle.policy);

    const QuaternionStorage yyz_q = quaternion(
        0.7071067811865476, 0.0, 0.0, 0.7071067811865476);
    const auto yyz_force = gnc::foundation::rotate_passive(
        yyz_q, vector(100.0, 200.0, 300.0), bundle.policy);
    const auto yyz_q_dot =
        gnc::foundation::passive_quaternion_body_rate_derivative(
            yyz_q, vector(1.0, 2.0, 3.0), bundle.policy);
    require(near(require_value(yyz_force, "YYZ force rotation failed"),
                 vector(200.0, -100.0, 300.0)),
            "YYZ force rotation differs");
    require(near(require_value(yyz_q_dot, "YYZ q derivative failed"),
                 quaternion(1.0606601717798214,
                            -1.0606601717798214,
                            -0.3535533905932738,
                            -1.0606601717798214)),
            "YYZ q derivative differs");
    bundle.yyz_force_i = yyz_force.value();
    bundle.yyz_q_derivative = yyz_q_dot.value();
    bundle.spin_convergence = run_principal_spin(bundle.policy);

    const QuaternionPolicy normalize_policy = product_policy(
        QuaternionNormalizationPolicy::NormalizeWithFlag);
    const auto normalized = gnc::foundation::prepare_passive_quaternion(
        quaternion(2.0, 0.0, 0.0, 0.0), normalize_policy);
    require(normalized.status() == NumericalStatus::Approximate &&
                normalized.has_value() &&
                gnc::foundation::has_numerical_flag(
                    normalized.evidence().flags,
                    NumericalFlag::Normalized) &&
                normalized.evidence().detail == "normalized-input" &&
                near(normalized.value(), quaternion(1.0, 0.0, 0.0, 0.0)),
            "NormalizeWithFlag semantics differ");
    bundle.normalization = {
        normalized.status(), normalized.evidence().flags,
        std::string(normalized.evidence().detail), normalized.value()};

    QuaternionPolicy invalid_policy = bundle.policy;
    invalid_policy.normalization =
        static_cast<QuaternionNormalizationPolicy>(255);
    auto outcome = gnc::foundation::prepare_passive_quaternion(
        quaternion(1.0, 0.0, 0.0, 0.0), invalid_policy);
    bundle.failures.push_back(failure(
        "INVALID-POLICY", outcome.status(), outcome.evidence().detail,
        outcome.has_value()));

    outcome = gnc::foundation::prepare_passive_quaternion(
        quaternion(0.0, 0.0, 0.0, 0.0), bundle.policy);
    bundle.failures.push_back(failure(
        "ZERO-NORM", outcome.status(), outcome.evidence().detail,
        outcome.has_value()));

    outcome = gnc::foundation::prepare_passive_quaternion(
        quaternion(2.0, 0.0, 0.0, 0.0), bundle.policy);
    bundle.failures.push_back(failure(
        "NONUNIT-ERROR", outcome.status(), outcome.evidence().detail,
        outcome.has_value()));

    outcome = gnc::foundation::prepare_passive_quaternion(
        quaternion(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0,
                   0.0),
        bundle.policy);
    bundle.failures.push_back(failure(
        "NONFINITE-QUATERNION", outcome.status(),
        outcome.evidence().detail, outcome.has_value()));

    const auto vector_failure = gnc::foundation::rotate_passive(
        quaternion(1.0, 0.0, 0.0, 0.0),
        vector(0.0, std::numeric_limits<double>::infinity(), 0.0),
        bundle.policy);
    bundle.failures.push_back(failure(
        "NONFINITE-VECTOR", vector_failure.status(),
        vector_failure.evidence().detail, vector_failure.has_value()));

    const auto rate_failure =
        gnc::foundation::passive_quaternion_body_rate_derivative(
            quaternion(1.0, 0.0, 0.0, 0.0),
            vector(0.0, 0.0,
                   std::numeric_limits<double>::quiet_NaN()),
            bundle.policy);
    bundle.failures.push_back(failure(
        "NONFINITE-ANGULAR-RATE", rate_failure.status(),
        rate_failure.evidence().detail, rate_failure.has_value()));

    const double largest = std::numeric_limits<double>::max();
    const auto overflow = gnc::foundation::hamilton_product(
        quaternion(largest, largest, largest, largest),
        quaternion(largest, largest, largest, largest),
        bundle.policy.numerical);
    bundle.failures.push_back(failure(
        "OVERFLOW-PRODUCT", overflow.status(), overflow.evidence().detail,
        overflow.has_value()));

    const std::array<std::pair<NumericalStatus, std::string_view>, 7>
        expected{{
            {NumericalStatus::DomainError, "policy"},
            {NumericalStatus::DomainError, "zero-norm-quaternion"},
            {NumericalStatus::DomainError, "non-unit-quaternion"},
            {NumericalStatus::NonFiniteInput, "quaternion"},
            {NumericalStatus::NonFiniteInput, "vector"},
            {NumericalStatus::NonFiniteInput, "angular-rate"},
            {NumericalStatus::NonFiniteOutput, "product"},
        }};
    require(bundle.failures.size() == expected.size(),
            "failure case count differs");
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        require(bundle.failures[index].status == expected[index].first &&
                    bundle.failures[index].detail == expected[index].second &&
                    !bundle.failures[index].has_value,
                "quaternion failure semantics differ");
    }
    return bundle;
}

void write_values(const std::vector<double>& values_to_write) {
    std::cout << '[';
    for (std::size_t index = 0U; index < values_to_write.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << values_to_write[index];
    }
    std::cout << ']';
}

void write_quaternion(const QuaternionStorage& value) {
    write_values(values(value));
}

void write_vec3(const Vec3& value) {
    write_values(values(value));
}

void write_bundle(const Bundle& bundle) {
    std::cout << std::setprecision(17);
    std::cout << "{\"schema_version\":\"" << kSchema
              << "\",\"component_id\":\"" << kComponentId
              << "\",\"fixture_ids\":["
              << "\"REF-SCIENTIFIC-CONVENTIONS-001\","
              << "\"REF-YYZ-6DOF-CORE-001\"]"
              << ",\"convention\":{"
              << "\"semantic\":\"passive\","
              << "\"algebra\":\"Hamilton\","
              << "\"coefficient_order\":\"wxyz\","
              << "\"composition\":\"q_c_a=q_b_a*q_c_b\"}"
              << ",\"storage\":{"
              << "\"type\":\"Eigen::Quaterniond\","
              << "\"eigen_version\":\"3.4.0\","
              << "\"scalar\":\"binary64\","
              << "\"wire_order\":\"wxyz\","
              << "\"eigen_coeffs_order\":\"xyzw\"}"
              << ",\"algorithms\":{"
              << "\"prepare\":\""
              << gnc::foundation::kPassiveQuaternionPrepareIdentity.id
              << "\",\"product\":\""
              << gnc::foundation::kHamiltonProductIdentity.id
              << "\",\"rotate\":\""
              << gnc::foundation::kPassiveQuaternionRotateIdentity.id
              << "\",\"compose\":\""
              << gnc::foundation::kPassiveQuaternionComposeIdentity.id
              << "\",\"body_rate\":\""
              << gnc::foundation::kPassiveQuaternionBodyRateIdentity.id
              << "\"}"
              << ",\"policy\":{"
              << "\"absolute_tolerance\":"
              << bundle.policy.numerical.absolute_tolerance
              << ",\"relative_tolerance\":"
              << bundle.policy.numerical.relative_tolerance
              << ",\"zero_tolerance\":"
              << bundle.policy.numerical.zero_tolerance
              << ",\"normalization\":\"Error\"}"
              << ",\"fixed_cases\":[";
    for (std::size_t index = 0U; index < bundle.fixed_cases.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << "{\"id\":\"" << bundle.fixed_cases[index].id
                  << "\",\"result\":";
        write_values(bundle.fixed_cases[index].result);
        std::cout << '}';
    }
    const PropertyMetrics& properties = bundle.properties;
    std::cout << "]"
              << ",\"properties\":{\"samples\":" << kPropertySamples
              << ",\"max_matrix_vector_error\":"
              << properties.max_matrix_vector_error
              << ",\"max_orthogonality_error\":"
              << properties.max_orthogonality_error
              << ",\"max_determinant_error\":"
              << properties.max_determinant_error
              << ",\"max_sign_equivalence_error\":"
              << properties.max_sign_equivalence_error
              << ",\"max_composition_error\":"
              << properties.max_composition_error
              << ",\"max_inverse_round_trip_error\":"
              << properties.max_inverse_round_trip_error
              << ",\"max_derivative_equivalence_error\":"
              << properties.max_derivative_equivalence_error
              << ",\"max_orientation_sign_error_rad\":"
              << properties.max_orientation_sign_error_rad << '}';
    std::cout << ",\"yyz\":{\"coupled_derivative\":{"
              << "\"force_I_N\":";
    write_vec3(bundle.yyz_force_i);
    std::cout << ",\"q_derivative_I_B_per_s\":";
    write_quaternion(bundle.yyz_q_derivative);
    std::cout << "},\"principal_spin_convergence\":[";
    for (std::size_t index = 0U;
         index < bundle.spin_convergence.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        const SpinLevel& level = bundle.spin_convergence[index];
        std::cout << "{\"dt_s\":" << level.dt_s
                  << ",\"orientation_error_rad\":"
                  << level.orientation_error_rad
                  << ",\"max_precommit_norm_residual\":"
                  << level.max_precommit_norm_residual
                  << ",\"observed_order\":";
        if (level.observed_order.has_value()) {
            std::cout << *level.observed_order;
        } else {
            std::cout << "null";
        }
        std::cout << ",\"final_q_I_B_wxyz\":";
        write_quaternion(level.final_quaternion);
        std::cout << '}';
    }
    std::cout << "]}"
              << ",\"normalization\":{\"status\":\""
              << gnc::foundation::to_string(bundle.normalization.status)
              << "\",\"flags\":" << bundle.normalization.flags
              << ",\"detail\":\"" << bundle.normalization.detail
              << "\",\"quaternion_wxyz\":";
    write_quaternion(bundle.normalization.quaternion);
    std::cout << "},\"failure_cases\":[";
    for (std::size_t index = 0U; index < bundle.failures.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        const FailureObservation& failure_case = bundle.failures[index];
        std::cout << "{\"id\":\"" << failure_case.id
                  << "\",\"status\":\""
                  << gnc::foundation::to_string(failure_case.status)
                  << "\",\"detail\":\"" << failure_case.detail
                  << "\",\"has_value\":"
                  << (failure_case.has_value ? "true" : "false") << '}';
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2 || std::string_view(argv[1]) != "--self-check") {
            std::cerr << "usage: gnc_foundation_quaternion_probe --self-check\n";
            return EXIT_FAILURE;
        }
        const Bundle bundle = run_bundle();
        write_bundle(bundle);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "foundation passive quaternion self-check failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

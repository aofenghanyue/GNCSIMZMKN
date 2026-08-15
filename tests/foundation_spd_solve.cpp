#include "gnc/foundation/spd_cholesky_3x3.hpp"

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
using gnc::foundation::LinearSolveMethod;
using gnc::foundation::Mat3;
using gnc::foundation::NumericalFlag;
using gnc::foundation::NumericalFlags;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalPolicy;
using gnc::foundation::NumericalStatus;
using gnc::foundation::SpdSolve3Result;
using gnc::foundation::Vec3;

constexpr std::string_view kSchema = "gnczmkn.foundation-spd-solve-probe/1";
constexpr std::string_view kComponentId = "GNC-FOUNDATION-SPD-SOLVE-001";
constexpr std::string_view kFixtureId = "REF-YYZ-MASS-PROPERTIES-001";

struct RigidConsumerCase {
    std::string id;
    Mat3 inertia;
    Vec3 omega;
    Vec3 moment_about_com;
    Vec3 expected_acceleration;
};

struct SolveObservation {
    std::string id;
    Mat3 matrix = Mat3::Zero();
    Vec3 rhs = Vec3::Zero();
    NumericalStatus status = NumericalStatus::InternalFailure;
    NumericalFlags flags = 0U;
    std::string detail;
    std::size_t evaluations = 0U;
    bool has_value = false;
    Vec3 solution = Vec3::Zero();
    std::size_t rank = 0U;
    LinearSolveMethod method = LinearSolveMethod::Cholesky;
    std::optional<double> residual_norm;
    std::optional<double> relative_residual;
    std::optional<double> condition_estimate;
};

struct ScaleObservation {
    double scale = 0.0;
    SolveObservation solve;
};

struct MutationObservation {
    SolveObservation solve;
    Vec3 angular_momentum = Vec3::Zero();
    Vec3 gyroscopic_moment = Vec3::Zero();
    double max_acceleration_difference = 0.0;
};

struct Bundle {
    NumericalPolicy policy;
    std::vector<SolveObservation> yyz_cases;
    std::vector<ScaleObservation> scale_cases;
    MutationObservation diagonalized_mutation;
    SolveObservation near_symmetric;
    std::vector<SolveObservation> failure_cases;
};

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

Mat3 matrix(std::array<double, 9> values) {
    Mat3 result;
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = 0; column < 3; ++column) {
            result(row, column) = values[static_cast<std::size_t>(
                row * 3 + column)];
        }
    }
    return result;
}

Vec3 vector(double x, double y, double z) {
    Vec3 result;
    result << x, y, z;
    return result;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return vector(lhs(1) * rhs(2) - lhs(2) * rhs(1),
                  lhs(2) * rhs(0) - lhs(0) * rhs(2),
                  lhs(0) * rhs(1) - lhs(1) * rhs(0));
}

double max_difference(const Vec3& lhs, const Vec3& rhs) {
    double result = 0.0;
    for (Eigen::Index index = 0; index < 3; ++index) {
        result = std::max(result, std::abs(lhs(index) - rhs(index)));
    }
    return result;
}

bool near(double actual, double expected, double absolute = 2.0e-12,
          double relative = 2.0e-12) {
    const double scale = std::max({1.0, std::abs(actual),
                                   std::abs(expected)});
    return std::isfinite(actual) && std::isfinite(expected) &&
           std::abs(actual - expected) <= absolute + relative * scale;
}

bool near(const Vec3& actual, const Vec3& expected,
          double absolute = 2.0e-12, double relative = 2.0e-12) {
    for (Eigen::Index index = 0; index < 3; ++index) {
        if (!near(actual(index), expected(index), absolute, relative)) {
            return false;
        }
    }
    return true;
}

NumericalPolicy product_policy() {
    NumericalPolicy policy;
    policy.absolute_tolerance = 2.0e-12;
    policy.relative_tolerance = 2.0e-12;
    policy.finite_check = FiniteCheck::EveryStage;
    policy.zero_tolerance = 1.0e-14;
    policy.condition_limit = 1.0e12;
    return policy;
}

std::vector<RigidConsumerCase> consumer_cases() {
    return {
        {
            "CASE-YYZ-MASS-PROPERTIES-CURRENT-CANDIDATE-PUBLICATION",
            matrix({12.0, 1.0, 0.5,
                    1.0, 20.0, 2.0,
                    0.5, 2.0, 30.0}),
            vector(1.0, 2.0, 3.0),
            vector(101.0, -77.0, 173.0),
            vector(4.3788453434471134, -2.1996418036241045,
                   5.3069953645174884),
        },
        {
            "CASE-YYZ-MASS-PROPERTIES-DIAGONAL-ZERO-FLOW",
            matrix({4.0, 0.0, 0.0,
                    0.0, 5.0, 0.0,
                    0.0, 0.0, 6.0}),
            vector(0.2, -0.1, 0.3),
            vector(-14.5, -8.0, 8.0),
            vector(-3.6175, -1.576, 1.3366666666666667),
        },
    };
}

Vec3 rigid_rhs(const Mat3& inertia, const Vec3& omega,
               const Vec3& moment_about_com) {
    const Vec3 angular_momentum = inertia * omega;
    return moment_about_com - cross(omega, angular_momentum);
}

SolveObservation observe(std::string id, const Mat3& matrix_value,
                         const Vec3& rhs, const NumericalPolicy& policy) {
    const NumericalOutcome<SpdSolve3Result> outcome =
        gnc::foundation::solve_spd_3x3(matrix_value, rhs, policy);
    SolveObservation result;
    result.id = std::move(id);
    result.matrix = matrix_value;
    result.rhs = rhs;
    result.status = outcome.status();
    result.flags = outcome.evidence().flags;
    result.detail = std::string(outcome.evidence().detail);
    result.evaluations = outcome.evidence().evaluations;
    result.has_value = outcome.has_value();
    result.residual_norm = outcome.evidence().residual_norm;
    result.relative_residual = outcome.evidence().estimated_rel_error;
    result.condition_estimate = outcome.evidence().condition_estimate;
    if (outcome.has_value()) {
        result.solution = outcome.value().solution;
        result.rank = outcome.value().rank;
        result.method = outcome.value().method;
    }
    return result;
}

void require_success(const SolveObservation& value,
                     const Vec3& expected) {
    require(value.status == NumericalStatus::Success,
            "SPD solve status differs");
    require(value.has_value && value.rank == 3U &&
                value.method == LinearSolveMethod::Cholesky,
            "SPD solve result metadata differs");
    require(value.detail == "cholesky" && value.flags == 0U &&
                value.evaluations == 4U,
            "SPD solve evidence metadata differs");
    require(value.residual_norm.has_value() &&
                value.relative_residual.has_value() &&
                value.condition_estimate.has_value(),
            "SPD solve numerical evidence is incomplete");
    require(near(value.solution, expected),
            "SPD solve solution differs");
    require(*value.relative_residual <= 2.0e-15,
            "SPD solve normalized residual is too large");
    require(*value.condition_estimate >= 1.0 &&
                *value.condition_estimate <= 1.0e12,
            "SPD solve condition estimate is outside policy");
}

Bundle run_bundle() {
    Bundle result;
    result.policy = product_policy();
    const auto cases = consumer_cases();

    for (const auto& value : cases) {
        const Vec3 rhs = rigid_rhs(value.inertia, value.omega,
                                   value.moment_about_com);
        auto observation = observe(value.id, value.inertia, rhs,
                                   result.policy);
        require_success(observation, value.expected_acceleration);
        result.yyz_cases.push_back(std::move(observation));
    }

    const RigidConsumerCase& scale_source = cases.front();
    const Vec3 scale_rhs = rigid_rhs(
        scale_source.inertia, scale_source.omega,
        scale_source.moment_about_com);
    for (double scale : {1.0e-9, 1.0, 1.0e9}) {
        auto observation = observe(
            "SCALE-YYZ-FULL-INERTIA", scale * scale_source.inertia,
            scale * scale_rhs, result.policy);
        require_success(observation, scale_source.expected_acceleration);
        require(near(*observation.condition_estimate,
                     *result.yyz_cases.front().condition_estimate,
                     5.0e-13, 5.0e-13),
                "SPD condition estimate is not scale invariant");
        result.scale_cases.push_back({scale, std::move(observation)});
    }

    Mat3 diagonalized = scale_source.inertia;
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = 0; column < 3; ++column) {
            if (row != column) {
                diagonalized(row, column) = 0.0;
            }
        }
    }
    const Vec3 diagonal_angular_momentum =
        diagonalized * scale_source.omega;
    const Vec3 diagonal_gyroscopic =
        cross(scale_source.omega, diagonal_angular_momentum);
    const Vec3 diagonal_rhs =
        scale_source.moment_about_com - diagonal_gyroscopic;
    auto diagonal_solve = observe(
        "MUTATION-YYZ-MASS-PROPERTIES-DIAGONALIZE-INERTIA",
        diagonalized, diagonal_rhs, result.policy);
    require_success(diagonal_solve,
                    vector(3.4166666666666665, -1.15,
                           5.2333333333333334));
    const double mutation_difference = max_difference(
        diagonal_solve.solution,
        result.yyz_cases.front().solution);
    require(mutation_difference > 0.9,
            "diagonalized inertia mutation was not exposed");
    result.diagonalized_mutation = {
        std::move(diagonal_solve), diagonal_angular_momentum,
        diagonal_gyroscopic, mutation_difference};

    Mat3 near_symmetric_matrix = scale_source.inertia;
    near_symmetric_matrix(0, 1) += 1.0e-13;
    Mat3 projected = near_symmetric_matrix;
    const double average = 0.5 * near_symmetric_matrix(0, 1) +
                           0.5 * near_symmetric_matrix(1, 0);
    projected(0, 1) = average;
    projected(1, 0) = average;
    const Vec3 projected_solution = vector(1.0, -2.0, 0.5);
    result.near_symmetric = observe(
        "NEAR-SYMMETRIC-PROJECTION", near_symmetric_matrix,
        projected * projected_solution, result.policy);
    require(result.near_symmetric.status == NumericalStatus::Approximate &&
                result.near_symmetric.has_value &&
                gnc::foundation::has_numerical_flag(
                    result.near_symmetric.flags,
                    NumericalFlag::Symmetrized) &&
                result.near_symmetric.detail == "symmetrized-input" &&
                near(result.near_symmetric.solution, projected_solution),
            "near-symmetric projection semantics differ");

    NumericalPolicy invalid_policy = result.policy;
    invalid_policy.zero_tolerance = -1.0;
    result.failure_cases.push_back(observe(
        "INVALID-POLICY", Mat3::Identity(), Vec3::Ones(), invalid_policy));

    Mat3 nonfinite_matrix = Mat3::Identity();
    nonfinite_matrix(0, 0) = std::numeric_limits<double>::quiet_NaN();
    result.failure_cases.push_back(observe(
        "NONFINITE-MATRIX", nonfinite_matrix, Vec3::Ones(), result.policy));

    Vec3 nonfinite_rhs = Vec3::Ones();
    nonfinite_rhs(2) = std::numeric_limits<double>::infinity();
    result.failure_cases.push_back(observe(
        "NONFINITE-RHS", Mat3::Identity(), nonfinite_rhs, result.policy));

    Mat3 asymmetric = scale_source.inertia;
    asymmetric(0, 1) += 1.0e-3;
    result.failure_cases.push_back(observe(
        "ASYMMETRIC", asymmetric, Vec3::Ones(), result.policy));

    Mat3 indefinite = Mat3::Identity();
    indefinite(2, 2) = -1.0;
    result.failure_cases.push_back(observe(
        "NON-SPD", indefinite, Vec3::Ones(), result.policy));

    Mat3 singular = Mat3::Identity();
    singular(2, 2) = 0.0;
    result.failure_cases.push_back(observe(
        "ZERO-PIVOT", singular, Vec3::Ones(), result.policy));

    result.failure_cases.push_back(observe(
        "ZERO-MATRIX", Mat3::Zero(), Vec3::Ones(), result.policy));

    Mat3 near_singular = Mat3::Identity();
    near_singular(2, 2) = 1.0e-16;
    result.failure_cases.push_back(observe(
        "PIVOT-BELOW-THRESHOLD", near_singular, Vec3::Ones(),
        result.policy));

    NumericalPolicy condition_policy = result.policy;
    condition_policy.zero_tolerance = 0.0;
    condition_policy.condition_limit = 1.0e12;
    result.failure_cases.push_back(observe(
        "CONDITION-LIMIT", near_singular, Vec3::Ones(),
        condition_policy));

    const std::array<std::pair<NumericalStatus, std::string_view>, 9>
        expected_failures{{
            {NumericalStatus::DomainError, "policy"},
            {NumericalStatus::NonFiniteInput, "matrix"},
            {NumericalStatus::NonFiniteInput, "rhs"},
            {NumericalStatus::DomainError, "matrix-not-symmetric"},
            {NumericalStatus::DomainError, "matrix-not-positive-definite"},
            {NumericalStatus::Singular, "zero-pivot"},
            {NumericalStatus::Singular, "zero-matrix"},
            {NumericalStatus::Singular, "pivot-below-threshold"},
            {NumericalStatus::IllConditioned, "condition-limit"},
        }};
    require(result.failure_cases.size() == expected_failures.size(),
            "SPD failure case count differs");
    for (std::size_t index = 0; index < expected_failures.size(); ++index) {
        const auto& actual = result.failure_cases[index];
        const auto& expected = expected_failures[index];
        require(actual.status == expected.first &&
                    actual.detail == expected.second && !actual.has_value,
                "SPD failure outcome differs");
    }
    require(result.failure_cases.back().condition_estimate.has_value() &&
                *result.failure_cases.back().condition_estimate > 1.0e12 &&
                result.failure_cases.back().residual_norm.has_value(),
            "ill-conditioned solve lost numerical evidence");
    return result;
}

void write_string(std::string_view value) {
    std::cout << '"' << value << '"';
}

void write_bool(bool value) {
    std::cout << (value ? "true" : "false");
}

void write_number(double value) {
    if (!std::isfinite(value)) {
        std::cout << "null";
        return;
    }
    std::cout << std::setprecision(17) << value;
}

void write_optional(const std::optional<double>& value) {
    if (!value.has_value()) {
        std::cout << "null";
        return;
    }
    write_number(*value);
}

void write_vector(const Vec3& value) {
    std::cout << '[';
    for (Eigen::Index index = 0; index < 3; ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        write_number(value(index));
    }
    std::cout << ']';
}

void write_matrix(const Mat3& value) {
    std::cout << '[';
    bool first = true;
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = 0; column < 3; ++column) {
            if (!first) {
                std::cout << ',';
            }
            first = false;
            write_number(value(row, column));
        }
    }
    std::cout << ']';
}

void write_solve(const SolveObservation& value) {
    std::cout << "{\"id\":";
    write_string(value.id);
    std::cout << ",\"matrix_row_major\":";
    write_matrix(value.matrix);
    std::cout << ",\"rhs\":";
    write_vector(value.rhs);
    std::cout << ",\"status\":";
    write_string(gnc::foundation::to_string(value.status));
    std::cout << ",\"has_value\":";
    write_bool(value.has_value);
    std::cout << ",\"flags\":" << value.flags << ",\"detail\":";
    write_string(value.detail);
    std::cout << ",\"evaluations\":" << value.evaluations;
    std::cout << ",\"rank\":";
    if (value.has_value) {
        std::cout << value.rank;
    } else {
        std::cout << "null";
    }
    std::cout << ",\"method\":";
    if (value.has_value) {
        write_string(gnc::foundation::to_string(value.method));
    } else {
        std::cout << "null";
    }
    std::cout << ",\"solution\":";
    if (value.has_value) {
        write_vector(value.solution);
    } else {
        std::cout << "null";
    }
    std::cout << ",\"residual_norm\":";
    write_optional(value.residual_norm);
    std::cout << ",\"relative_residual\":";
    write_optional(value.relative_residual);
    std::cout << ",\"condition_estimate\":";
    write_optional(value.condition_estimate);
    std::cout << '}';
}

void write_bundle(const Bundle& value) {
    std::cout << "{\"schema_version\":";
    write_string(kSchema);
    std::cout << ",\"component_id\":";
    write_string(kComponentId);
    std::cout << ",\"fixture_id\":";
    write_string(kFixtureId);
    std::cout << ",\"algorithm\":{\"id\":";
    write_string(gnc::foundation::kSpdCholesky3Identity.id);
    std::cout << ",\"version\":";
    write_string(gnc::foundation::kSpdCholesky3Identity.version);
    std::cout << "},\"storage\":{\"eigen_version\":\"3.4.0\","
                 "\"scalar\":\"binary64\",\"vector_convention\":"
                 "\"column\",\"matrix_storage_order\":\"column-major\","
                 "\"vec3_shape\":[3,1],\"mat3_shape\":[3,3]},"
                 "\"policy\":{\"absolute_tolerance\":";
    write_number(value.policy.absolute_tolerance);
    std::cout << ",\"relative_tolerance\":";
    write_number(value.policy.relative_tolerance);
    std::cout << ",\"zero_tolerance\":";
    write_number(value.policy.zero_tolerance);
    std::cout << ",\"condition_limit\":";
    write_number(value.policy.condition_limit);
    std::cout << "},\"yyz_cases\":[";
    for (std::size_t index = 0; index < value.yyz_cases.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        write_solve(value.yyz_cases[index]);
    }
    std::cout << "],\"scale_cases\":[";
    for (std::size_t index = 0; index < value.scale_cases.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << "{\"scale\":";
        write_number(value.scale_cases[index].scale);
        std::cout << ",\"solve\":";
        write_solve(value.scale_cases[index].solve);
        std::cout << '}';
    }
    std::cout << "],\"diagonalized_mutation\":{\"solve\":";
    write_solve(value.diagonalized_mutation.solve);
    std::cout << ",\"angular_momentum\":";
    write_vector(value.diagonalized_mutation.angular_momentum);
    std::cout << ",\"gyroscopic_moment\":";
    write_vector(value.diagonalized_mutation.gyroscopic_moment);
    std::cout << ",\"max_acceleration_difference\":";
    write_number(value.diagonalized_mutation.max_acceleration_difference);
    std::cout << "},\"near_symmetric\":";
    write_solve(value.near_symmetric);
    std::cout << ",\"failure_cases\":[";
    for (std::size_t index = 0; index < value.failure_cases.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        write_solve(value.failure_cases[index]);
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string_view(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_foundation_spd_solve_probe --self-check\n";
        return EXIT_FAILURE;
    }
    try {
        static_assert(Mat3::IsRowMajor == 0,
                      "canonical Mat3 storage must remain column-major");
        const Bundle result = run_bundle();
        write_bundle(result);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "foundation SPD solve self-check failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

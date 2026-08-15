#pragma once

#include "gnc/foundation/linear_algebra.hpp"
#include "gnc/foundation/numerical_policy.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace gnc::foundation {

enum class LinearSolveMethod : std::uint8_t {
    Cholesky,
};

[[nodiscard]] constexpr std::string_view to_string(
    LinearSolveMethod method) noexcept {
    switch (method) {
    case LinearSolveMethod::Cholesky:
        return "Cholesky";
    }
    return "Cholesky";
}

struct SpdSolve3Result {
    Vec3 solution = Vec3::Zero();
    std::size_t rank = 0U;
    LinearSolveMethod method = LinearSolveMethod::Cholesky;
};

inline constexpr AlgorithmIdentity kSpdCholesky3Identity{
    "gnc.foundation.linear.spd-cholesky-3x3@1", "1.0.0"};

namespace detail {

[[nodiscard]] inline NumericalEvidence spd_evidence() {
    NumericalEvidence evidence;
    evidence.algorithm = kSpdCholesky3Identity;
    return evidence;
}

[[nodiscard]] inline bool all_finite(const Mat3& value) noexcept {
    for (Eigen::Index row = 0; row < value.rows(); ++row) {
        for (Eigen::Index column = 0; column < value.cols(); ++column) {
            if (!std::isfinite(value(row, column))) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] inline bool all_finite(const Vec3& value) noexcept {
    for (Eigen::Index row = 0; row < value.rows(); ++row) {
        if (!std::isfinite(value(row))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline double matrix_one_norm(const Mat3& value) noexcept {
    double result = 0.0;
    for (Eigen::Index column = 0; column < value.cols(); ++column) {
        double sum = 0.0;
        for (Eigen::Index row = 0; row < value.rows(); ++row) {
            sum += std::abs(value(row, column));
        }
        result = std::max(result, sum);
    }
    return result;
}

[[nodiscard]] inline double vector_one_norm(const Vec3& value) noexcept {
    double result = 0.0;
    for (Eigen::Index row = 0; row < value.rows(); ++row) {
        result += std::abs(value(row));
    }
    return result;
}

[[nodiscard]] inline double vector_infinity_norm(
    const Vec3& value) noexcept {
    double result = 0.0;
    for (Eigen::Index row = 0; row < value.rows(); ++row) {
        result = std::max(result, std::abs(value(row)));
    }
    return result;
}

[[nodiscard]] inline Vec3 solve_cholesky_factor(const Mat3& lower,
                                                 const Vec3& rhs) {
    Vec3 forward = Vec3::Zero();
    for (Eigen::Index row = 0; row < 3; ++row) {
        double residual = rhs(row);
        for (Eigen::Index column = 0; column < row; ++column) {
            residual -= lower(row, column) * forward(column);
        }
        forward(row) = residual / lower(row, row);
    }

    Vec3 result = Vec3::Zero();
    for (Eigen::Index row = 3; row-- > 0;) {
        double residual = forward(row);
        for (Eigen::Index column = row + 1; column < 3; ++column) {
            residual -= lower(column, row) * result(column);
        }
        result(row) = residual / lower(row, row);
    }
    return result;
}

} // namespace detail

[[nodiscard]] inline NumericalOutcome<SpdSolve3Result> solve_spd_3x3(
    const Mat3& matrix, const Vec3& rhs,
    const NumericalPolicy& policy = NumericalPolicy{}) {
    NumericalEvidence evidence = detail::spd_evidence();
    if (!valid_numerical_policy(policy)) {
        evidence.detail = "policy";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::DomainError, evidence);
    }

    if (policy.finite_check != FiniteCheck::Disabled &&
        !detail::all_finite(matrix)) {
        evidence.detail = "matrix";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::NonFiniteInput, evidence);
    }
    if (policy.finite_check != FiniteCheck::Disabled &&
        !detail::all_finite(rhs)) {
        evidence.detail = "rhs";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::NonFiniteInput, evidence);
    }

    Mat3 symmetric = matrix;
    bool symmetrized = false;
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = row + 1; column < 3; ++column) {
            const double upper = matrix(row, column);
            const double lower = matrix(column, row);
            const double difference = std::abs(upper - lower);
            const double scale = std::max(std::abs(upper), std::abs(lower));
            const double limit = policy.absolute_tolerance +
                                 policy.relative_tolerance * scale;
            if (!std::isfinite(difference) || !std::isfinite(limit)) {
                evidence.detail = "symmetry-check";
                return NumericalOutcome<SpdSolve3Result>::failure(
                    NumericalStatus::NonFiniteIntermediate, evidence);
            }
            if (difference > limit) {
                evidence.detail = "matrix-not-symmetric";
                return NumericalOutcome<SpdSolve3Result>::failure(
                    NumericalStatus::DomainError, evidence);
            }
            if (upper != lower) {
                const double average = 0.5 * upper + 0.5 * lower;
                if (policy.finite_check == FiniteCheck::EveryStage &&
                    !std::isfinite(average)) {
                    evidence.detail = "symmetry-average";
                    return NumericalOutcome<SpdSolve3Result>::failure(
                        NumericalStatus::NonFiniteIntermediate, evidence);
                }
                symmetric(row, column) = average;
                symmetric(column, row) = average;
                symmetrized = true;
            }
        }
    }
    if (symmetrized) {
        evidence.flags |= numerical_flag(NumericalFlag::Symmetrized);
    }

    const double matrix_norm = detail::matrix_one_norm(symmetric);
    if (policy.finite_check == FiniteCheck::EveryStage &&
        !std::isfinite(matrix_norm)) {
        evidence.detail = "matrix-norm";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::NonFiniteIntermediate, evidence);
    }
    if (matrix_norm == 0.0) {
        evidence.detail = "zero-matrix";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::Singular, evidence);
    }
    const double pivot_threshold = policy.zero_tolerance * matrix_norm;
    if (policy.finite_check == FiniteCheck::EveryStage &&
        !std::isfinite(pivot_threshold)) {
        evidence.detail = "pivot-threshold";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::NonFiniteIntermediate, evidence);
    }

    Mat3 lower = Mat3::Zero();
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = 0; column <= row; ++column) {
            double residual = symmetric(row, column);
            for (Eigen::Index index = 0; index < column; ++index) {
                residual -= lower(row, index) * lower(column, index);
            }
            if (policy.finite_check == FiniteCheck::EveryStage &&
                !std::isfinite(residual)) {
                evidence.detail = "factorization";
                return NumericalOutcome<SpdSolve3Result>::failure(
                    NumericalStatus::NonFiniteIntermediate, evidence);
            }
            if (row == column) {
                if (residual < 0.0) {
                    evidence.detail = "matrix-not-positive-definite";
                    return NumericalOutcome<SpdSolve3Result>::failure(
                        NumericalStatus::DomainError, evidence);
                }
                if (residual <= pivot_threshold) {
                    evidence.detail = residual == 0.0 ?
                        "zero-pivot" : "pivot-below-threshold";
                    return NumericalOutcome<SpdSolve3Result>::failure(
                        NumericalStatus::Singular, evidence);
                }
                lower(row, column) = std::sqrt(residual);
            } else {
                lower(row, column) = residual / lower(column, column);
            }
            if (policy.finite_check == FiniteCheck::EveryStage &&
                !std::isfinite(lower(row, column))) {
                evidence.detail = "factorization";
                return NumericalOutcome<SpdSolve3Result>::failure(
                    NumericalStatus::NonFiniteIntermediate, evidence);
            }
        }
    }

    const Vec3 solution = detail::solve_cholesky_factor(lower, rhs);
    if (policy.finite_check != FiniteCheck::Disabled &&
        !detail::all_finite(solution)) {
        evidence.detail = "solution";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }

    Mat3 inverse = Mat3::Zero();
    for (Eigen::Index column = 0; column < 3; ++column) {
        Vec3 basis = Vec3::Zero();
        basis(column) = 1.0;
        inverse.col(column) = detail::solve_cholesky_factor(lower, basis);
    }
    const double inverse_norm = detail::matrix_one_norm(inverse);
    const double condition_estimate = matrix_norm * inverse_norm;
    if (policy.finite_check != FiniteCheck::Disabled &&
        (!detail::all_finite(inverse) || !std::isfinite(inverse_norm) ||
         !std::isfinite(condition_estimate))) {
        evidence.detail = "condition-estimate";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::NonFiniteIntermediate, evidence);
    }

    const Vec3 residual = symmetric * solution - rhs;
    const double residual_norm = detail::vector_infinity_norm(residual);
    const double residual_scale =
        matrix_norm * detail::vector_one_norm(solution) +
        detail::vector_one_norm(rhs);
    const double relative_residual = residual_scale > 0.0 ?
        residual_norm / residual_scale : residual_norm;
    if (policy.finite_check != FiniteCheck::Disabled &&
        (!detail::all_finite(residual) || !std::isfinite(residual_norm) ||
         !std::isfinite(residual_scale) ||
         !std::isfinite(relative_residual))) {
        evidence.detail = "residual";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }

    evidence.evaluations = 4U;
    evidence.estimated_abs_error = residual_norm;
    evidence.estimated_rel_error = relative_residual;
    evidence.residual_norm = residual_norm;
    evidence.condition_estimate = condition_estimate;
    if (condition_estimate > policy.condition_limit) {
        evidence.detail = "condition-limit";
        return NumericalOutcome<SpdSolve3Result>::failure(
            NumericalStatus::IllConditioned, evidence);
    }

    evidence.detail = symmetrized ? "symmetrized-input" : "cholesky";
    return NumericalOutcome<SpdSolve3Result>::with_value(
        symmetrized ? NumericalStatus::Approximate : NumericalStatus::Success,
        SpdSolve3Result{solution, 3U, LinearSolveMethod::Cholesky}, evidence);
}

} // namespace gnc::foundation

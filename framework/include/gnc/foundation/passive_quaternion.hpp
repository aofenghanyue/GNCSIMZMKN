#pragma once

#include "gnc/foundation/linear_algebra.hpp"
#include "gnc/foundation/numerical_policy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace gnc::foundation {

enum class QuaternionNormalizationPolicy : std::uint8_t {
    Error,
    NormalizeWithFlag,
};

struct QuaternionPolicy {
    NumericalPolicy numerical{};
    QuaternionNormalizationPolicy normalization =
        QuaternionNormalizationPolicy::Error;
};

[[nodiscard]] inline bool valid_quaternion_policy(
    const QuaternionPolicy& policy) noexcept {
    if (!valid_numerical_policy(policy.numerical)) {
        return false;
    }
    switch (policy.normalization) {
    case QuaternionNormalizationPolicy::Error:
    case QuaternionNormalizationPolicy::NormalizeWithFlag:
        return true;
    }
    return false;
}

inline constexpr AlgorithmIdentity kPassiveQuaternionPrepareIdentity{
    "gnc.foundation.quaternion.prepare-passive-hamilton@1", "1.0.0"};
inline constexpr AlgorithmIdentity kHamiltonProductIdentity{
    "gnc.foundation.quaternion.hamilton-product@1", "1.0.0"};
inline constexpr AlgorithmIdentity kPassiveQuaternionInverseIdentity{
    "gnc.foundation.quaternion.inverse@1", "1.0.0"};
inline constexpr AlgorithmIdentity kPassiveQuaternionRotateIdentity{
    "gnc.foundation.quaternion.passive-rotate@1", "1.0.0"};
inline constexpr AlgorithmIdentity kPassiveQuaternionMatrixIdentity{
    "gnc.foundation.quaternion.passive-matrix@1", "1.0.0"};
inline constexpr AlgorithmIdentity kPassiveQuaternionComposeIdentity{
    "gnc.foundation.quaternion.passive-compose@1", "1.0.0"};
inline constexpr AlgorithmIdentity kPassiveQuaternionBodyRateIdentity{
    "gnc.foundation.quaternion.body-rate-derivative@1", "1.0.0"};
inline constexpr AlgorithmIdentity kPassiveQuaternionInertialRateIdentity{
    "gnc.foundation.quaternion.inertial-rate-derivative@1", "1.0.0"};
inline constexpr AlgorithmIdentity kPassiveQuaternionOrientationErrorIdentity{
    "gnc.foundation.quaternion.orientation-error@1", "1.0.0"};

[[nodiscard]] inline QuaternionStorage quaternion_from_wxyz(
    double w, double x, double y, double z) noexcept {
    return QuaternionStorage{w, x, y, z};
}

[[nodiscard]] inline QuaternionStorage quaternion_from_wxyz(
    const std::array<double, 4>& coefficients) noexcept {
    return quaternion_from_wxyz(coefficients[0], coefficients[1],
                                coefficients[2], coefficients[3]);
}

[[nodiscard]] inline std::array<double, 4> quaternion_to_wxyz(
    const QuaternionStorage& quaternion) noexcept {
    return {quaternion.w(), quaternion.x(), quaternion.y(), quaternion.z()};
}

namespace quaternion_detail {

[[nodiscard]] inline bool is_finite(
    const QuaternionStorage& quaternion) noexcept {
    return std::isfinite(quaternion.w()) &&
           std::isfinite(quaternion.x()) &&
           std::isfinite(quaternion.y()) &&
           std::isfinite(quaternion.z());
}

[[nodiscard]] inline bool is_finite(const Vec3& vector) noexcept {
    return std::isfinite(vector(0)) && std::isfinite(vector(1)) &&
           std::isfinite(vector(2));
}

[[nodiscard]] inline bool is_finite(const Mat3& matrix) noexcept {
    for (Eigen::Index row = 0; row < 3; ++row) {
        for (Eigen::Index column = 0; column < 3; ++column) {
            if (!std::isfinite(matrix(row, column))) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] inline double norm(
    const QuaternionStorage& quaternion) noexcept {
    return std::hypot(std::hypot(quaternion.w(), quaternion.x()),
                      std::hypot(quaternion.y(), quaternion.z()));
}

[[nodiscard]] inline QuaternionStorage product(
    const QuaternionStorage& lhs,
    const QuaternionStorage& rhs) noexcept {
    return quaternion_from_wxyz(
        lhs.w() * rhs.w() - lhs.x() * rhs.x() -
            lhs.y() * rhs.y() - lhs.z() * rhs.z(),
        lhs.w() * rhs.x() + lhs.x() * rhs.w() +
            lhs.y() * rhs.z() - lhs.z() * rhs.y(),
        lhs.w() * rhs.y() - lhs.x() * rhs.z() +
            lhs.y() * rhs.w() + lhs.z() * rhs.x(),
        lhs.w() * rhs.z() + lhs.x() * rhs.y() -
            lhs.y() * rhs.x() + lhs.z() * rhs.w());
}

[[nodiscard]] inline NumericalEvidence evidence(
    AlgorithmIdentity algorithm, NumericalFlags flags = 0U,
    std::string_view detail = {}) noexcept {
    NumericalEvidence result;
    result.flags = flags;
    result.algorithm = algorithm;
    result.detail = detail;
    return result;
}

[[nodiscard]] inline NumericalStatus value_status(
    NumericalFlags flags) noexcept {
    return flags == 0U ? NumericalStatus::Success
                       : NumericalStatus::Approximate;
}

[[nodiscard]] inline std::string_view value_detail(
    NumericalFlags flags, std::string_view exact_detail) noexcept {
    return has_numerical_flag(flags, NumericalFlag::Normalized)
               ? std::string_view{"normalized-input"}
               : exact_detail;
}

} // namespace quaternion_detail

[[nodiscard]] inline NumericalOutcome<QuaternionStorage>
prepare_passive_quaternion(
    const QuaternionStorage& quaternion,
    const QuaternionPolicy& policy = QuaternionPolicy{}) {
    NumericalEvidence evidence = quaternion_detail::evidence(
        kPassiveQuaternionPrepareIdentity);
    if (!valid_quaternion_policy(policy)) {
        evidence.detail = "policy";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::DomainError, evidence);
    }
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(quaternion)) {
        evidence.detail = "quaternion";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteInput, evidence);
    }

    const double quaternion_norm = quaternion_detail::norm(quaternion);
    if (!std::isfinite(quaternion_norm)) {
        evidence.detail = "quaternion-norm";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteIntermediate, evidence);
    }
    evidence.evaluations = 1U;
    evidence.residual_norm = std::abs(quaternion_norm - 1.0);
    if (quaternion_norm <= policy.numerical.zero_tolerance) {
        evidence.detail = "zero-norm-quaternion";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::DomainError, evidence);
    }

    const double unit_limit =
        policy.numerical.absolute_tolerance +
        policy.numerical.relative_tolerance *
            std::max(1.0, quaternion_norm);
    if (*evidence.residual_norm <= unit_limit) {
        evidence.detail = "unit-input";
        return NumericalOutcome<QuaternionStorage>::with_value(
            NumericalStatus::Success, quaternion, evidence);
    }
    if (policy.normalization == QuaternionNormalizationPolicy::Error) {
        evidence.detail = "non-unit-quaternion";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::DomainError, evidence);
    }

    QuaternionStorage normalized = quaternion;
    normalized.coeffs() /= quaternion_norm;
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(normalized)) {
        evidence.detail = "normalized-quaternion";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    evidence.flags |= numerical_flag(NumericalFlag::Normalized);
    evidence.detail = "normalized-input";
    return NumericalOutcome<QuaternionStorage>::with_value(
        NumericalStatus::Approximate, normalized, evidence);
}

[[nodiscard]] inline NumericalOutcome<QuaternionStorage>
hamilton_product(
    const QuaternionStorage& lhs, const QuaternionStorage& rhs,
    const NumericalPolicy& policy = NumericalPolicy{}) {
    NumericalEvidence evidence =
        quaternion_detail::evidence(kHamiltonProductIdentity);
    if (!valid_numerical_policy(policy)) {
        evidence.detail = "policy";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::DomainError, evidence);
    }
    if (policy.finite_check != FiniteCheck::Disabled &&
        (!quaternion_detail::is_finite(lhs) ||
         !quaternion_detail::is_finite(rhs))) {
        evidence.detail = "quaternion";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteInput, evidence);
    }
    QuaternionStorage result = quaternion_detail::product(lhs, rhs);
    if (policy.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(result)) {
        evidence.detail = "product";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    evidence.evaluations = 1U;
    evidence.detail = "hamilton-wxyz";
    return NumericalOutcome<QuaternionStorage>::with_value(
        NumericalStatus::Success, result, evidence);
}

[[nodiscard]] inline NumericalOutcome<QuaternionStorage>
inverse_passive_quaternion(
    const QuaternionStorage& quaternion,
    const QuaternionPolicy& policy = QuaternionPolicy{}) {
    const auto prepared = prepare_passive_quaternion(quaternion, policy);
    if (!prepared.has_value()) {
        NumericalEvidence evidence = prepared.evidence();
        evidence.algorithm = kPassiveQuaternionInverseIdentity;
        return NumericalOutcome<QuaternionStorage>::failure(
            prepared.status(), evidence);
    }

    const QuaternionStorage& value = prepared.value();
    const double squared_norm = value.squaredNorm();
    QuaternionStorage result = value.conjugate();
    result.coeffs() /= squared_norm;
    NumericalEvidence evidence = quaternion_detail::evidence(
        kPassiveQuaternionInverseIdentity, prepared.evidence().flags,
        quaternion_detail::value_detail(prepared.evidence().flags,
                                        "inverse"));
    evidence.evaluations = prepared.evidence().evaluations + 1U;
    evidence.residual_norm = prepared.evidence().residual_norm;
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(result)) {
        evidence.detail = "inverse";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    return NumericalOutcome<QuaternionStorage>::with_value(
        quaternion_detail::value_status(evidence.flags), result, evidence);
}

[[nodiscard]] inline NumericalOutcome<Vec3> rotate_passive(
    const QuaternionStorage& q_to_from, const Vec3& vector_from,
    const QuaternionPolicy& policy = QuaternionPolicy{}) {
    if (!valid_quaternion_policy(policy)) {
        NumericalEvidence evidence = quaternion_detail::evidence(
            kPassiveQuaternionRotateIdentity, 0U, "policy");
        return NumericalOutcome<Vec3>::failure(
            NumericalStatus::DomainError, evidence);
    }
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(vector_from)) {
        NumericalEvidence evidence = quaternion_detail::evidence(
            kPassiveQuaternionRotateIdentity, 0U, "vector");
        return NumericalOutcome<Vec3>::failure(
            NumericalStatus::NonFiniteInput, evidence);
    }
    const auto prepared = prepare_passive_quaternion(q_to_from, policy);
    if (!prepared.has_value()) {
        NumericalEvidence evidence = prepared.evidence();
        evidence.algorithm = kPassiveQuaternionRotateIdentity;
        return NumericalOutcome<Vec3>::failure(prepared.status(), evidence);
    }

    const QuaternionStorage& quaternion = prepared.value();
    QuaternionStorage inverse = quaternion.conjugate();
    inverse.coeffs() /= quaternion.squaredNorm();
    const QuaternionStorage pure =
        quaternion_from_wxyz(0.0, vector_from(0), vector_from(1),
                             vector_from(2));
    const QuaternionStorage rotated = quaternion_detail::product(
        quaternion_detail::product(inverse, pure), quaternion);
    const Vec3 result{rotated.x(), rotated.y(), rotated.z()};
    NumericalEvidence evidence = quaternion_detail::evidence(
        kPassiveQuaternionRotateIdentity, prepared.evidence().flags,
        quaternion_detail::value_detail(prepared.evidence().flags,
                                        "inverse-pure-product"));
    evidence.evaluations = prepared.evidence().evaluations + 2U;
    evidence.residual_norm = prepared.evidence().residual_norm;
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(result)) {
        evidence.detail = "rotated-vector";
        return NumericalOutcome<Vec3>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    return NumericalOutcome<Vec3>::with_value(
        quaternion_detail::value_status(evidence.flags), result, evidence);
}

[[nodiscard]] inline NumericalOutcome<Mat3> passive_rotation_matrix(
    const QuaternionStorage& q_to_from,
    const QuaternionPolicy& policy = QuaternionPolicy{}) {
    const auto prepared = prepare_passive_quaternion(q_to_from, policy);
    if (!prepared.has_value()) {
        NumericalEvidence evidence = prepared.evidence();
        evidence.algorithm = kPassiveQuaternionMatrixIdentity;
        return NumericalOutcome<Mat3>::failure(prepared.status(), evidence);
    }

    const QuaternionStorage& quaternion = prepared.value();
    const double w = quaternion.w();
    const double x = quaternion.x();
    const double y = quaternion.y();
    const double z = quaternion.z();
    const double inverse_squared_norm = 1.0 / quaternion.squaredNorm();
    Mat3 result;
    result <<
        w * w + x * x - y * y - z * z,
        2.0 * (x * y + w * z),
        2.0 * (x * z - w * y),
        2.0 * (x * y - w * z),
        w * w - x * x + y * y - z * z,
        2.0 * (y * z + w * x),
        2.0 * (x * z + w * y),
        2.0 * (y * z - w * x),
        w * w - x * x - y * y + z * z;
    result *= inverse_squared_norm;

    NumericalEvidence evidence = quaternion_detail::evidence(
        kPassiveQuaternionMatrixIdentity, prepared.evidence().flags,
        quaternion_detail::value_detail(prepared.evidence().flags,
                                        "passive-matrix"));
    evidence.evaluations = prepared.evidence().evaluations + 1U;
    evidence.residual_norm = prepared.evidence().residual_norm;
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(result)) {
        evidence.detail = "rotation-matrix";
        return NumericalOutcome<Mat3>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    return NumericalOutcome<Mat3>::with_value(
        quaternion_detail::value_status(evidence.flags), result, evidence);
}

// q_c_from_a = q_b_from_a Hamilton-product q_c_from_b.
[[nodiscard]] inline NumericalOutcome<QuaternionStorage> compose_passive(
    const QuaternionStorage& q_c_from_b,
    const QuaternionStorage& q_b_from_a,
    const QuaternionPolicy& policy = QuaternionPolicy{}) {
    const auto c_from_b = prepare_passive_quaternion(q_c_from_b, policy);
    if (!c_from_b.has_value()) {
        NumericalEvidence evidence = c_from_b.evidence();
        evidence.algorithm = kPassiveQuaternionComposeIdentity;
        evidence.detail = "q-c-from-b";
        return NumericalOutcome<QuaternionStorage>::failure(
            c_from_b.status(), evidence);
    }
    const auto b_from_a = prepare_passive_quaternion(q_b_from_a, policy);
    if (!b_from_a.has_value()) {
        NumericalEvidence evidence = b_from_a.evidence();
        evidence.algorithm = kPassiveQuaternionComposeIdentity;
        evidence.detail = "q-b-from-a";
        return NumericalOutcome<QuaternionStorage>::failure(
            b_from_a.status(), evidence);
    }

    const QuaternionStorage candidate = quaternion_detail::product(
        b_from_a.value(), c_from_b.value());
    const auto prepared_candidate =
        prepare_passive_quaternion(candidate, policy);
    if (!prepared_candidate.has_value()) {
        NumericalEvidence evidence = prepared_candidate.evidence();
        evidence.flags |= c_from_b.evidence().flags |
                          b_from_a.evidence().flags;
        evidence.algorithm = kPassiveQuaternionComposeIdentity;
        evidence.detail = "composed-quaternion";
        return NumericalOutcome<QuaternionStorage>::failure(
            prepared_candidate.status(), evidence);
    }

    const NumericalFlags flags = c_from_b.evidence().flags |
                                 b_from_a.evidence().flags |
                                 prepared_candidate.evidence().flags;
    NumericalEvidence evidence = quaternion_detail::evidence(
        kPassiveQuaternionComposeIdentity, flags,
        quaternion_detail::value_detail(flags, "passive-composition"));
    evidence.evaluations = c_from_b.evidence().evaluations +
                           b_from_a.evidence().evaluations +
                           prepared_candidate.evidence().evaluations + 1U;
    evidence.residual_norm = std::max(
        {c_from_b.evidence().residual_norm.value_or(0.0),
         b_from_a.evidence().residual_norm.value_or(0.0),
         prepared_candidate.evidence().residual_norm.value_or(0.0)});
    return NumericalOutcome<QuaternionStorage>::with_value(
        quaternion_detail::value_status(flags),
        prepared_candidate.value(), evidence);
}

[[nodiscard]] inline NumericalOutcome<QuaternionStorage>
passive_quaternion_body_rate_derivative(
    const QuaternionStorage& q_i_from_b, const Vec3& omega_bi_b,
    const QuaternionPolicy& policy = QuaternionPolicy{}) {
    if (!valid_quaternion_policy(policy)) {
        NumericalEvidence evidence = quaternion_detail::evidence(
            kPassiveQuaternionBodyRateIdentity, 0U, "policy");
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::DomainError, evidence);
    }
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(omega_bi_b)) {
        NumericalEvidence evidence = quaternion_detail::evidence(
            kPassiveQuaternionBodyRateIdentity, 0U, "angular-rate");
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteInput, evidence);
    }
    const auto prepared = prepare_passive_quaternion(q_i_from_b, policy);
    if (!prepared.has_value()) {
        NumericalEvidence evidence = prepared.evidence();
        evidence.algorithm = kPassiveQuaternionBodyRateIdentity;
        return NumericalOutcome<QuaternionStorage>::failure(
            prepared.status(), evidence);
    }
    const QuaternionStorage pure = quaternion_from_wxyz(
        0.0, omega_bi_b(0), omega_bi_b(1), omega_bi_b(2));
    QuaternionStorage result =
        quaternion_detail::product(pure, prepared.value());
    result.coeffs() *= -0.5;
    NumericalEvidence evidence = quaternion_detail::evidence(
        kPassiveQuaternionBodyRateIdentity, prepared.evidence().flags,
        quaternion_detail::value_detail(prepared.evidence().flags,
                                        "body-rate"));
    evidence.evaluations = prepared.evidence().evaluations + 1U;
    evidence.residual_norm = prepared.evidence().residual_norm;
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(result)) {
        evidence.detail = "quaternion-derivative";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    return NumericalOutcome<QuaternionStorage>::with_value(
        quaternion_detail::value_status(evidence.flags), result, evidence);
}

[[nodiscard]] inline NumericalOutcome<QuaternionStorage>
passive_quaternion_inertial_rate_derivative(
    const QuaternionStorage& q_i_from_b, const Vec3& omega_bi_i,
    const QuaternionPolicy& policy = QuaternionPolicy{}) {
    if (!valid_quaternion_policy(policy)) {
        NumericalEvidence evidence = quaternion_detail::evidence(
            kPassiveQuaternionInertialRateIdentity, 0U, "policy");
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::DomainError, evidence);
    }
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(omega_bi_i)) {
        NumericalEvidence evidence = quaternion_detail::evidence(
            kPassiveQuaternionInertialRateIdentity, 0U, "angular-rate");
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteInput, evidence);
    }
    const auto prepared = prepare_passive_quaternion(q_i_from_b, policy);
    if (!prepared.has_value()) {
        NumericalEvidence evidence = prepared.evidence();
        evidence.algorithm = kPassiveQuaternionInertialRateIdentity;
        return NumericalOutcome<QuaternionStorage>::failure(
            prepared.status(), evidence);
    }
    const QuaternionStorage pure = quaternion_from_wxyz(
        0.0, omega_bi_i(0), omega_bi_i(1), omega_bi_i(2));
    QuaternionStorage result =
        quaternion_detail::product(prepared.value(), pure);
    result.coeffs() *= -0.5;
    NumericalEvidence evidence = quaternion_detail::evidence(
        kPassiveQuaternionInertialRateIdentity, prepared.evidence().flags,
        quaternion_detail::value_detail(prepared.evidence().flags,
                                        "inertial-rate"));
    evidence.evaluations = prepared.evidence().evaluations + 1U;
    evidence.residual_norm = prepared.evidence().residual_norm;
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !quaternion_detail::is_finite(result)) {
        evidence.detail = "quaternion-derivative";
        return NumericalOutcome<QuaternionStorage>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    return NumericalOutcome<QuaternionStorage>::with_value(
        quaternion_detail::value_status(evidence.flags), result, evidence);
}

[[nodiscard]] inline NumericalOutcome<double>
passive_quaternion_orientation_error(
    const QuaternionStorage& lhs, const QuaternionStorage& rhs,
    const QuaternionPolicy& policy = QuaternionPolicy{}) {
    const auto prepared_lhs = prepare_passive_quaternion(lhs, policy);
    if (!prepared_lhs.has_value()) {
        NumericalEvidence evidence = prepared_lhs.evidence();
        evidence.algorithm = kPassiveQuaternionOrientationErrorIdentity;
        evidence.detail = "lhs";
        return NumericalOutcome<double>::failure(prepared_lhs.status(),
                                                 evidence);
    }
    const auto prepared_rhs = prepare_passive_quaternion(rhs, policy);
    if (!prepared_rhs.has_value()) {
        NumericalEvidence evidence = prepared_rhs.evidence();
        evidence.algorithm = kPassiveQuaternionOrientationErrorIdentity;
        evidence.detail = "rhs";
        return NumericalOutcome<double>::failure(prepared_rhs.status(),
                                                 evidence);
    }

    QuaternionStorage unit_lhs = prepared_lhs.value();
    QuaternionStorage unit_rhs = prepared_rhs.value();
    unit_lhs.coeffs() /= quaternion_detail::norm(unit_lhs);
    unit_rhs.coeffs() /= quaternion_detail::norm(unit_rhs);
    if (unit_lhs.dot(unit_rhs) < 0.0) {
        unit_rhs.coeffs() *= -1.0;
    }
    const auto lhs_wxyz = quaternion_to_wxyz(unit_lhs);
    const auto rhs_wxyz = quaternion_to_wxyz(unit_rhs);
    double chord = 0.0;
    for (std::size_t index = 0U; index < lhs_wxyz.size(); ++index) {
        chord = std::hypot(chord, lhs_wxyz[index] - rhs_wxyz[index]);
    }
    const double half_chord = 0.5 * chord;
    const double clamped_half_chord = std::clamp(half_chord, 0.0, 1.0);
    const double result = 4.0 * std::asin(clamped_half_chord);
    NumericalFlags flags = prepared_lhs.evidence().flags |
                           prepared_rhs.evidence().flags;
    if (clamped_half_chord != half_chord) {
        flags |= numerical_flag(NumericalFlag::Clamped);
    }
    NumericalEvidence evidence = quaternion_detail::evidence(
        kPassiveQuaternionOrientationErrorIdentity, flags,
        quaternion_detail::value_detail(flags, "sign-invariant-angle"));
    evidence.evaluations = prepared_lhs.evidence().evaluations +
                           prepared_rhs.evidence().evaluations + 1U;
    evidence.estimated_abs_error = result;
    if (policy.numerical.finite_check != FiniteCheck::Disabled &&
        !std::isfinite(result)) {
        evidence.detail = "orientation-error";
        return NumericalOutcome<double>::failure(
            NumericalStatus::NonFiniteOutput, evidence);
    }
    return NumericalOutcome<double>::with_value(
        quaternion_detail::value_status(flags), result, evidence);
}

} // namespace gnc::foundation

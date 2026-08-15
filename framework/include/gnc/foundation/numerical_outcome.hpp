#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace gnc::foundation {

enum class NumericalStatus : std::uint8_t {
    Success,
    Converged,
    Approximate,
    OutOfRange,
    Extrapolated,
    NoBracket,
    MaxIterations,
    Singular,
    IllConditioned,
    DomainError,
    NonFiniteInput,
    NonFiniteIntermediate,
    NonFiniteOutput,
    StepUnderflow,
    ToleranceUnreachable,
    Cancelled,
    InternalFailure,
};

[[nodiscard]] constexpr std::string_view to_string(
    NumericalStatus status) noexcept {
    switch (status) {
    case NumericalStatus::Success:
        return "Success";
    case NumericalStatus::Converged:
        return "Converged";
    case NumericalStatus::Approximate:
        return "Approximate";
    case NumericalStatus::OutOfRange:
        return "OutOfRange";
    case NumericalStatus::Extrapolated:
        return "Extrapolated";
    case NumericalStatus::NoBracket:
        return "NoBracket";
    case NumericalStatus::MaxIterations:
        return "MaxIterations";
    case NumericalStatus::Singular:
        return "Singular";
    case NumericalStatus::IllConditioned:
        return "IllConditioned";
    case NumericalStatus::DomainError:
        return "DomainError";
    case NumericalStatus::NonFiniteInput:
        return "NonFiniteInput";
    case NumericalStatus::NonFiniteIntermediate:
        return "NonFiniteIntermediate";
    case NumericalStatus::NonFiniteOutput:
        return "NonFiniteOutput";
    case NumericalStatus::StepUnderflow:
        return "StepUnderflow";
    case NumericalStatus::ToleranceUnreachable:
        return "ToleranceUnreachable";
    case NumericalStatus::Cancelled:
        return "Cancelled";
    case NumericalStatus::InternalFailure:
        return "InternalFailure";
    }
    return "InternalFailure";
}

[[nodiscard]] constexpr bool numerical_status_has_value(
    NumericalStatus status) noexcept {
    return status == NumericalStatus::Success ||
           status == NumericalStatus::Converged ||
           status == NumericalStatus::Approximate ||
           status == NumericalStatus::Extrapolated;
}

[[nodiscard]] constexpr bool numerical_status_is_success(
    NumericalStatus status) noexcept {
    return status == NumericalStatus::Success ||
           status == NumericalStatus::Converged;
}

enum class NumericalFlag : std::uint32_t {
    Normalized = 1U << 0U,
    Clamped = 1U << 1U,
    FallbackUsed = 1U << 2U,
    Symmetrized = 1U << 3U,
};

using NumericalFlags = std::uint32_t;

[[nodiscard]] constexpr NumericalFlags numerical_flag(
    NumericalFlag flag) noexcept {
    return static_cast<NumericalFlags>(flag);
}

[[nodiscard]] constexpr NumericalFlags operator|(NumericalFlag lhs,
                                                  NumericalFlag rhs) noexcept {
    return numerical_flag(lhs) | numerical_flag(rhs);
}

[[nodiscard]] constexpr bool has_numerical_flag(
    NumericalFlags flags, NumericalFlag flag) noexcept {
    return (flags & numerical_flag(flag)) != 0U;
}

struct AlgorithmIdentity {
    std::string_view id;
    std::string_view version;
};

struct NumericalEvidence {
    NumericalFlags flags = 0U;
    std::size_t iterations = 0U;
    std::size_t evaluations = 0U;
    std::optional<double> estimated_abs_error;
    std::optional<double> estimated_rel_error;
    std::optional<double> residual_norm;
    std::optional<double> condition_estimate;
    std::optional<double> last_step;
    std::optional<double> last_bracket_lower;
    std::optional<double> last_bracket_upper;
    AlgorithmIdentity algorithm{};
    std::string_view detail;
};

template <typename Value>
class NumericalOutcome {
    static_assert(!std::is_reference<Value>::value,
                  "NumericalOutcome must own its value");

  public:
    [[nodiscard]] static NumericalOutcome with_value(
        NumericalStatus status, Value value,
        NumericalEvidence evidence = NumericalEvidence{}) {
        if (!numerical_status_has_value(status)) {
            throw std::invalid_argument(
                "NumericalOutcome value is incompatible with status");
        }
        return NumericalOutcome{status, std::move(value),
                                std::move(evidence)};
    }

    [[nodiscard]] static NumericalOutcome failure(
        NumericalStatus status,
        NumericalEvidence evidence = NumericalEvidence{}) {
        if (numerical_status_has_value(status)) {
            throw std::invalid_argument(
                "NumericalOutcome failure requires a value-less status");
        }
        return NumericalOutcome{status, std::nullopt, std::move(evidence)};
    }

    [[nodiscard]] NumericalStatus status() const noexcept { return status_; }

    [[nodiscard]] bool has_value() const noexcept { return value_.has_value(); }

    [[nodiscard]] bool succeeded() const noexcept {
        return numerical_status_is_success(status_);
    }

    [[nodiscard]] const Value& value() const {
        if (!value_.has_value()) {
            throw std::logic_error("NumericalOutcome has no value");
        }
        return *value_;
    }

    [[nodiscard]] Value& value() {
        if (!value_.has_value()) {
            throw std::logic_error("NumericalOutcome has no value");
        }
        return *value_;
    }

    [[nodiscard]] const NumericalEvidence& evidence() const noexcept {
        return evidence_;
    }

  private:
    NumericalOutcome(NumericalStatus status, std::optional<Value> value,
                     NumericalEvidence evidence)
        : status_(status), value_(std::move(value)),
          evidence_(std::move(evidence)) {}

    NumericalStatus status_;
    std::optional<Value> value_;
    NumericalEvidence evidence_;
};

} // namespace gnc::foundation

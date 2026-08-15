#pragma once

#include "gnc/foundation/numerical_outcome.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace gnc::foundation {

inline constexpr AlgorithmIdentity kTrilinearTablePreparationIdentity{
    "gnc.foundation.interpolation.trilinear-prepare@1", "1.0.0"};
inline constexpr AlgorithmIdentity kStrictTrilinearInterpolationIdentity{
    "gnc.foundation.interpolation.trilinear-strict@1", "1.0.0"};
inline constexpr std::string_view kTrilinearTableLayout =
    "x-major-y-middle-z-fastest";
inline constexpr std::size_t kTrilinearCornerEvaluations = 8U;

struct NumericAxisView {
    const double* values = nullptr;
    std::size_t size = 0U;
};

template <std::size_t OutputCount>
struct TrilinearTableView {
    static_assert(OutputCount > 0U,
                  "trilinear tables require at least one output");

    NumericAxisView x_axis;
    NumericAxisView y_axis;
    NumericAxisView z_axis;
    const std::array<double, OutputCount>* rows = nullptr;
    std::size_t row_count = 0U;
};

struct AxisBracket {
    std::size_t lower_index = 0U;
    std::size_t upper_index = 0U;
    double lower_value = 0.0;
    double upper_value = 0.0;
    double weight = 0.0;
};

enum class InterpolationDomainStatus : std::uint8_t {
    Inside,
    Boundary,
};

[[nodiscard]] constexpr std::string_view to_string(
    InterpolationDomainStatus status) noexcept {
    switch (status) {
    case InterpolationDomainStatus::Inside:
        return "Inside";
    case InterpolationDomainStatus::Boundary:
        return "Boundary";
    }
    return "Inside";
}

template <std::size_t OutputCount>
struct TrilinearInterpolationResult {
    std::array<double, OutputCount> values{};
    AxisBracket x_bracket;
    AxisBracket y_bracket;
    AxisBracket z_bracket;
    InterpolationDomainStatus domain_status =
        InterpolationDomainStatus::Inside;
};

namespace detail {

struct AxisValidation {
    NumericalStatus status = NumericalStatus::Success;
    std::string_view detail;
};

[[nodiscard]] inline AxisValidation validate_axis_view(
    const NumericAxisView& axis, std::string_view detail) {
    if (axis.values == nullptr || axis.size < 2U) {
        return {NumericalStatus::DomainError, detail};
    }
    for (std::size_t index = 0U; index < axis.size; ++index) {
        if (!std::isfinite(axis.values[index])) {
            return {NumericalStatus::NonFiniteInput, detail};
        }
        if (index + 1U < axis.size &&
            !(axis.values[index] < axis.values[index + 1U])) {
            return {NumericalStatus::DomainError, detail};
        }
    }
    return {};
}

struct BracketComputation {
    bool in_range = false;
    bool finite = false;
    AxisBracket bracket;
};

[[nodiscard]] inline BracketComputation strict_axis_bracket(
    const NumericAxisView& axis, double query) {
    if (query < axis.values[0U] || query > axis.values[axis.size - 1U]) {
        return {};
    }

    AxisBracket result;
    if (query == axis.values[axis.size - 1U]) {
        result.lower_index = axis.size - 2U;
        result.upper_index = axis.size - 1U;
        result.weight = 1.0;
    } else {
        const double* upper = std::upper_bound(
            axis.values, axis.values + axis.size, query);
        result.upper_index = static_cast<std::size_t>(upper - axis.values);
        result.lower_index = result.upper_index - 1U;
        result.weight =
            (query - axis.values[result.lower_index]) /
            (axis.values[result.upper_index] -
             axis.values[result.lower_index]);
    }
    result.lower_value = axis.values[result.lower_index];
    result.upper_value = axis.values[result.upper_index];
    return {true, std::isfinite(result.weight), result};
}

[[nodiscard]] inline bool is_axis_boundary(const NumericAxisView& axis,
                                           double query) noexcept {
    return query == axis.values[0U] ||
           query == axis.values[axis.size - 1U];
}

} // namespace detail

template <std::size_t OutputCount>
class PreparedTrilinearTableView {
  public:
    PreparedTrilinearTableView(const PreparedTrilinearTableView&) = default;
    PreparedTrilinearTableView(PreparedTrilinearTableView&&) noexcept =
        default;
    PreparedTrilinearTableView& operator=(
        const PreparedTrilinearTableView&) = default;
    PreparedTrilinearTableView& operator=(
        PreparedTrilinearTableView&&) noexcept = default;

    [[nodiscard]] static NumericalOutcome<PreparedTrilinearTableView>
    prepare(TrilinearTableView<OutputCount> view) {
        NumericalEvidence evidence;
        evidence.algorithm = kTrilinearTablePreparationIdentity;

        const auto failure = [&](NumericalStatus status,
                                 std::string_view detail) {
            evidence.detail = detail;
            return NumericalOutcome<PreparedTrilinearTableView>::failure(
                status, evidence);
        };

        for (const auto validation : {
                 detail::validate_axis_view(view.x_axis, "x-axis"),
                 detail::validate_axis_view(view.y_axis, "y-axis"),
                 detail::validate_axis_view(view.z_axis, "z-axis")}) {
            if (validation.status != NumericalStatus::Success) {
                return failure(validation.status, validation.detail);
            }
        }

        const std::size_t maximum =
            std::numeric_limits<std::size_t>::max();
        if (view.x_axis.size > maximum / view.y_axis.size) {
            return failure(NumericalStatus::DomainError, "grid-shape");
        }
        const std::size_t xy_count = view.x_axis.size * view.y_axis.size;
        if (xy_count > maximum / view.z_axis.size) {
            return failure(NumericalStatus::DomainError, "grid-shape");
        }
        const std::size_t expected_rows = xy_count * view.z_axis.size;
        if (view.rows == nullptr || view.row_count != expected_rows) {
            return failure(NumericalStatus::DomainError, "row-count");
        }
        for (std::size_t row = 0U; row < view.row_count; ++row) {
            for (double value : view.rows[row]) {
                if (!std::isfinite(value)) {
                    return failure(NumericalStatus::NonFiniteInput,
                                   "coefficient");
                }
            }
        }

        evidence.evaluations = view.row_count;
        return NumericalOutcome<PreparedTrilinearTableView>::with_value(
            NumericalStatus::Success,
            PreparedTrilinearTableView{std::move(view)}, evidence);
    }

    [[nodiscard]] const NumericAxisView& x_axis() const noexcept {
        return view_.x_axis;
    }

    [[nodiscard]] const NumericAxisView& y_axis() const noexcept {
        return view_.y_axis;
    }

    [[nodiscard]] const NumericAxisView& z_axis() const noexcept {
        return view_.z_axis;
    }

    [[nodiscard]] const std::array<double, OutputCount>* rows()
        const noexcept {
        return view_.rows;
    }

    [[nodiscard]] std::size_t row_count() const noexcept {
        return view_.row_count;
    }

  private:
    explicit PreparedTrilinearTableView(
        TrilinearTableView<OutputCount> view) noexcept
        : view_(std::move(view)) {}

    TrilinearTableView<OutputCount> view_;
};

template <std::size_t OutputCount>
[[nodiscard]] NumericalOutcome<PreparedTrilinearTableView<OutputCount>>
prepare_trilinear_table(TrilinearTableView<OutputCount> view) {
    return PreparedTrilinearTableView<OutputCount>::prepare(std::move(view));
}

template <std::size_t OutputCount>
[[nodiscard]] NumericalOutcome<TrilinearInterpolationResult<OutputCount>>
query_trilinear_strict(
    const PreparedTrilinearTableView<OutputCount>& table, double x_query,
    double y_query, double z_query) {
    std::size_t corner_evaluations = 0U;
    const auto failure = [&](NumericalStatus status,
                             std::string_view detail) {
        NumericalEvidence evidence;
        evidence.evaluations = corner_evaluations;
        evidence.algorithm = kStrictTrilinearInterpolationIdentity;
        evidence.detail = detail;
        return NumericalOutcome<
            TrilinearInterpolationResult<OutputCount>>::failure(status,
                                                                 evidence);
    };

    if (!std::isfinite(x_query) || !std::isfinite(y_query) ||
        !std::isfinite(z_query)) {
        return failure(NumericalStatus::NonFiniteInput, "query");
    }

    const detail::BracketComputation x =
        detail::strict_axis_bracket(table.x_axis(), x_query);
    if (!x.in_range) {
        return failure(NumericalStatus::OutOfRange, "x-query");
    }
    const detail::BracketComputation y =
        detail::strict_axis_bracket(table.y_axis(), y_query);
    if (!y.in_range) {
        return failure(NumericalStatus::OutOfRange, "y-query");
    }
    const detail::BracketComputation z =
        detail::strict_axis_bracket(table.z_axis(), z_query);
    if (!z.in_range) {
        return failure(NumericalStatus::OutOfRange, "z-query");
    }
    if (!x.finite || !y.finite || !z.finite) {
        return failure(NumericalStatus::NonFiniteIntermediate, "weight");
    }

    TrilinearInterpolationResult<OutputCount> result;
    result.x_bracket = x.bracket;
    result.y_bracket = y.bracket;
    result.z_bracket = z.bracket;
    result.domain_status =
        detail::is_axis_boundary(table.x_axis(), x_query) ||
                detail::is_axis_boundary(table.y_axis(), y_query) ||
                detail::is_axis_boundary(table.z_axis(), z_query)
            ? InterpolationDomainStatus::Boundary
            : InterpolationDomainStatus::Inside;

    const std::array<double, 3U> weights{
        x.bracket.weight, y.bracket.weight, z.bracket.weight};
    for (std::size_t x_corner = 0U; x_corner < 2U; ++x_corner) {
        const double x_weight =
            x_corner == 0U ? 1.0 - weights[0U] : weights[0U];
        const std::size_t x_index =
            x_corner == 0U ? x.bracket.lower_index : x.bracket.upper_index;
        for (std::size_t y_corner = 0U; y_corner < 2U; ++y_corner) {
            const double y_weight =
                y_corner == 0U ? 1.0 - weights[1U] : weights[1U];
            const std::size_t y_index = y_corner == 0U
                ? y.bracket.lower_index
                : y.bracket.upper_index;
            for (std::size_t z_corner = 0U; z_corner < 2U; ++z_corner) {
                const double z_weight = z_corner == 0U
                    ? 1.0 - weights[2U]
                    : weights[2U];
                const std::size_t z_index = z_corner == 0U
                    ? z.bracket.lower_index
                    : z.bracket.upper_index;
                const std::size_t row_index =
                    (x_index * table.y_axis().size + y_index) *
                        table.z_axis().size +
                    z_index;
                const auto& row = table.rows()[row_index];
                const double corner_weight =
                    x_weight * y_weight * z_weight;
                for (std::size_t output = 0U; output < OutputCount;
                     ++output) {
                    result.values[output] += row[output] * corner_weight;
                }
                ++corner_evaluations;
            }
        }
    }

    for (double& value : result.values) {
        if (!std::isfinite(value)) {
            return failure(NumericalStatus::NonFiniteOutput, "result");
        }
        if (value == 0.0) {
            value = 0.0;
        }
    }

    NumericalEvidence evidence;
    evidence.evaluations = corner_evaluations;
    evidence.algorithm = kStrictTrilinearInterpolationIdentity;
    return NumericalOutcome<TrilinearInterpolationResult<OutputCount>>::
        with_value(NumericalStatus::Success, std::move(result), evidence);
}

} // namespace gnc::foundation

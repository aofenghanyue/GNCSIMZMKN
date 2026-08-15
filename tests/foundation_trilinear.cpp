#include "gnc/foundation/trilinear_table.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Coefficients = std::array<double, 6U>;
using PreparedTable = gnc::foundation::PreparedTrilinearTableView<6U>;
using QueryResult = gnc::foundation::TrilinearInterpolationResult<6U>;
using gnc::foundation::NumericalOutcome;
using gnc::foundation::NumericalStatus;

constexpr std::string_view kSchema =
    "gnczmkn.foundation-trilinear-probe/1";
constexpr std::string_view kComponentId =
    "GNC-FOUNDATION-TRILINEAR-001";
constexpr std::string_view kFixtureId = "REF-YYZ-AERO-LOOKUP-001";

const std::array<double, 2U> kMachAxis{0.2, 0.6};
const std::array<double, 2U> kAlphaAxis{-0.1, 0.1};
const std::array<double, 2U> kBetaAxis{-0.05, 0.05};
const std::array<Coefficients, 8U> kRows{{
    {0.006, 0.0245, -0.0795, 0.005, 0.014, -0.00755},
    {0.006, -0.0245, -0.0805, -0.005, 0.014, 0.00755},
    {0.05, 0.0245, 0.0795, 0.005, -0.106, -0.00785},
    {0.05, -0.0245, 0.0805, -0.005, -0.106, 0.00785},
    {0.018, 0.0235, -0.0795, 0.005, 0.022, -0.00795},
    {0.018, -0.0235, -0.0805, -0.005, 0.022, 0.00795},
    {0.07, 0.0235, 0.0795, 0.005, -0.098, -0.00825},
    {0.07, -0.0235, 0.0805, -0.005, -0.098, 0.00825},
}};

struct QueryRecord {
    std::string id;
    NumericalStatus status = NumericalStatus::InternalFailure;
    bool has_value = false;
    std::size_t evaluations = 0U;
    QueryResult result;
};

struct FailureRecord {
    std::string id;
    NumericalStatus status = NumericalStatus::InternalFailure;
    bool has_value = false;
    std::string detail;
};

struct Bundle {
    std::size_t preparation_evaluations = 0U;
    std::vector<QueryRecord> cases;
    std::vector<FailureRecord> query_failures;
    std::vector<FailureRecord> preparation_failures;
};

gnc::foundation::TrilinearTableView<6U> tableView(
    const double* mach = kMachAxis.data(), std::size_t mach_count = 2U,
    const Coefficients* rows = kRows.data(), std::size_t row_count = 8U) {
    return {
        {mach, mach_count},
        {kAlphaAxis.data(), kAlphaAxis.size()},
        {kBetaAxis.data(), kBetaAxis.size()},
        rows,
        row_count,
    };
}

QueryRecord query(std::string id, const PreparedTable& table, double mach,
                  double alpha, double beta) {
    const auto outcome = gnc::foundation::query_trilinear_strict(
        table, mach, alpha, beta);
    QueryRecord record;
    record.id = std::move(id);
    record.status = outcome.status();
    record.has_value = outcome.has_value();
    record.evaluations = outcome.evidence().evaluations;
    if (outcome.has_value()) {
        record.result = outcome.value();
    }
    return record;
}

template <typename Value>
FailureRecord failureRecord(std::string id,
                            const NumericalOutcome<Value>& outcome) {
    return {std::move(id), outcome.status(), outcome.has_value(),
            std::string{outcome.evidence().detail}};
}

Bundle runBundle() {
    const auto prepared = gnc::foundation::prepare_trilinear_table(
        tableView());
    if (!prepared.has_value()) {
        throw std::runtime_error("accepted trilinear table did not prepare");
    }

    Bundle bundle;
    bundle.preparation_evaluations = prepared.evidence().evaluations;
    bundle.cases = {
        query("CASE-YYZ-AERO-LOOKUP-INTERIOR", prepared.value(), 0.35,
              0.025, -0.0125),
        query("CASE-YYZ-AERO-LOOKUP-EXACT-KNOT", prepared.value(), 0.2,
              -0.1, 0.05),
        query("CASE-YYZ-AERO-LOOKUP-UPPER-BOUNDARY", prepared.value(), 0.6,
              0.1, 0.05),
    };

    bundle.query_failures = {
        failureRecord(
            "INVALID-YYZ-AERO-LOOKUP-MACH-LOW",
            gnc::foundation::query_trilinear_strict(
                prepared.value(), 0.1, 0.025, -0.0125)),
        failureRecord(
            "INVALID-YYZ-AERO-LOOKUP-ALPHA-HIGH",
            gnc::foundation::query_trilinear_strict(
                prepared.value(), 0.35, 0.2, -0.0125)),
        failureRecord(
            "INVALID-YYZ-AERO-LOOKUP-BETA-LOW",
            gnc::foundation::query_trilinear_strict(
                prepared.value(), 0.35, 0.025, -0.1)),
        failureRecord(
            "NONFINITE-QUERY",
            gnc::foundation::query_trilinear_strict(
                prepared.value(),
                std::numeric_limits<double>::quiet_NaN(), 0.025,
                -0.0125)),
    };

    const std::array<double, 2U> duplicate_mach{0.2, 0.2};
    auto nonfinite_rows = kRows;
    nonfinite_rows[0U][0U] =
        std::numeric_limits<double>::quiet_NaN();
    bundle.preparation_failures = {
        failureRecord(
            "INVALID-YYZ-AERO-LOOKUP-DUPLICATE-AXIS",
            gnc::foundation::prepare_trilinear_table(
                tableView(duplicate_mach.data(), duplicate_mach.size()))),
        failureRecord(
            "INVALID-YYZ-AERO-LOOKUP-ROW-COUNT",
            gnc::foundation::prepare_trilinear_table(
                tableView(kMachAxis.data(), kMachAxis.size(), kRows.data(),
                          kRows.size() - 1U))),
        failureRecord(
            "INVALID-YYZ-AERO-LOOKUP-NONFINITE-COEFFICIENT",
            gnc::foundation::prepare_trilinear_table(tableView(
                kMachAxis.data(), kMachAxis.size(), nonfinite_rows.data(),
                nonfinite_rows.size()))),
    };
    return bundle;
}

bool close(double actual, double expected, double tolerance = 2.0e-12) {
    return std::isfinite(actual) &&
           std::abs(actual - expected) <=
               tolerance *
                   std::max({1.0, std::abs(actual), std::abs(expected)});
}

bool close(const Coefficients& actual, const Coefficients& expected) {
    for (std::size_t index = 0U; index < actual.size(); ++index) {
        if (!close(actual[index], expected[index])) {
            return false;
        }
    }
    return true;
}

bool bracketEquals(const gnc::foundation::AxisBracket& bracket,
                   std::size_t lower, std::size_t upper, double weight) {
    return bracket.lower_index == lower && bracket.upper_index == upper &&
           close(bracket.weight, weight);
}

bool selfCheck(const Bundle& bundle) {
    if (gnc::foundation::kTrilinearTableLayout !=
            "x-major-y-middle-z-fastest" ||
        gnc::foundation::kTrilinearCornerEvaluations != 8U ||
        bundle.preparation_evaluations != 8U || bundle.cases.size() != 3U) {
        return false;
    }

    const std::array<Coefficients, 3U> expected{{
        {0.039875, 0.00603125, 0.01996875, 0.00125, -0.058,
         -0.001971875},
        {0.006, -0.0245, -0.0805, -0.005, 0.014, 0.00755},
        {0.07, -0.0235, 0.0805, -0.005, -0.098, 0.00825},
    }};
    for (std::size_t index = 0U; index < bundle.cases.size(); ++index) {
        const QueryRecord& record = bundle.cases[index];
        if (record.status != NumericalStatus::Success || !record.has_value ||
            record.evaluations != 8U ||
            !close(record.result.values, expected[index])) {
            return false;
        }
    }

    const QueryResult& interior = bundle.cases[0U].result;
    if (!bracketEquals(interior.x_bracket, 0U, 1U, 0.375) ||
        !bracketEquals(interior.y_bracket, 0U, 1U, 0.625) ||
        !bracketEquals(interior.z_bracket, 0U, 1U, 0.375) ||
        interior.domain_status !=
            gnc::foundation::InterpolationDomainStatus::Inside) {
        return false;
    }
    const QueryResult& exact = bundle.cases[1U].result;
    const QueryResult& upper = bundle.cases[2U].result;
    if (!bracketEquals(exact.x_bracket, 0U, 1U, 0.0) ||
        !bracketEquals(exact.y_bracket, 0U, 1U, 0.0) ||
        !bracketEquals(exact.z_bracket, 0U, 1U, 1.0) ||
        !bracketEquals(upper.x_bracket, 0U, 1U, 1.0) ||
        !bracketEquals(upper.y_bracket, 0U, 1U, 1.0) ||
        !bracketEquals(upper.z_bracket, 0U, 1U, 1.0) ||
        exact.domain_status !=
            gnc::foundation::InterpolationDomainStatus::Boundary ||
        upper.domain_status !=
            gnc::foundation::InterpolationDomainStatus::Boundary) {
        return false;
    }

    const std::array<NumericalStatus, 4U> query_statuses{
        NumericalStatus::OutOfRange, NumericalStatus::OutOfRange,
        NumericalStatus::OutOfRange, NumericalStatus::NonFiniteInput};
    const std::array<std::string_view, 4U> query_details{
        "x-query", "y-query", "z-query", "query"};
    if (bundle.query_failures.size() != query_statuses.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < query_statuses.size(); ++index) {
        if (bundle.query_failures[index].status != query_statuses[index] ||
            bundle.query_failures[index].has_value ||
            bundle.query_failures[index].detail != query_details[index]) {
            return false;
        }
    }

    const std::array<NumericalStatus, 3U> preparation_statuses{
        NumericalStatus::DomainError, NumericalStatus::DomainError,
        NumericalStatus::NonFiniteInput};
    const std::array<std::string_view, 3U> preparation_details{
        "x-axis", "row-count", "coefficient"};
    if (bundle.preparation_failures.size() != preparation_statuses.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < preparation_statuses.size();
         ++index) {
        if (bundle.preparation_failures[index].status !=
                preparation_statuses[index] ||
            bundle.preparation_failures[index].has_value ||
            bundle.preparation_failures[index].detail !=
                preparation_details[index]) {
            return false;
        }
    }
    return true;
}

void writeString(std::string_view value) {
    std::cout << '"' << value << '"';
}

void writeBracket(const gnc::foundation::AxisBracket& bracket) {
    std::cout << "{\"lower_index\":" << bracket.lower_index
              << ",\"upper_index\":" << bracket.upper_index
              << ",\"lower_value\":" << bracket.lower_value
              << ",\"upper_value\":" << bracket.upper_value
              << ",\"weight\":" << bracket.weight << '}';
}

void writeCoefficients(const Coefficients& values) {
    std::cout << '[';
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << values[index];
    }
    std::cout << ']';
}

void writeFailure(const FailureRecord& failure) {
    std::cout << "{\"id\":";
    writeString(failure.id);
    std::cout << ",\"status\":";
    writeString(gnc::foundation::to_string(failure.status));
    std::cout << ",\"has_value\":"
              << (failure.has_value ? "true" : "false")
              << ",\"detail\":";
    writeString(failure.detail);
    std::cout << '}';
}

void writeReport(const Bundle& bundle) {
    std::cout << std::setprecision(17) << "{\"schema_version\":";
    writeString(kSchema);
    std::cout << ",\"component_id\":";
    writeString(kComponentId);
    std::cout << ",\"fixture_id\":";
    writeString(kFixtureId);
    std::cout << ",\"algorithm\":{\"prepare_id\":";
    writeString(gnc::foundation::kTrilinearTablePreparationIdentity.id);
    std::cout << ",\"query_id\":";
    writeString(gnc::foundation::kStrictTrilinearInterpolationIdentity.id);
    std::cout << ",\"version\":";
    writeString(gnc::foundation::kStrictTrilinearInterpolationIdentity.version);
    std::cout << ",\"layout\":";
    writeString(gnc::foundation::kTrilinearTableLayout);
    std::cout << "},\"preparation_evaluations\":"
              << bundle.preparation_evaluations << ",\"cases\":[";
    for (std::size_t index = 0U; index < bundle.cases.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        const QueryRecord& record = bundle.cases[index];
        std::cout << "{\"id\":";
        writeString(record.id);
        std::cout << ",\"status\":";
        writeString(gnc::foundation::to_string(record.status));
        std::cout << ",\"has_value\":"
                  << (record.has_value ? "true" : "false")
                  << ",\"evaluations\":" << record.evaluations
                  << ",\"domain_status\":";
        writeString(gnc::foundation::to_string(record.result.domain_status));
        std::cout << ",\"brackets\":{\"mach\":";
        writeBracket(record.result.x_bracket);
        std::cout << ",\"alpha\":";
        writeBracket(record.result.y_bracket);
        std::cout << ",\"beta\":";
        writeBracket(record.result.z_bracket);
        std::cout << "},\"coefficients\":";
        writeCoefficients(record.result.values);
        std::cout << '}';
    }
    std::cout << "],\"query_failures\":[";
    for (std::size_t index = 0U; index < bundle.query_failures.size();
         ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        writeFailure(bundle.query_failures[index]);
    }
    std::cout << "],\"preparation_failures\":[";
    for (std::size_t index = 0U;
         index < bundle.preparation_failures.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        writeFailure(bundle.preparation_failures[index]);
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string_view{argv[1]} != "--self-check") {
        std::cerr << "usage: gnc_foundation_trilinear_probe --self-check\n";
        return EXIT_FAILURE;
    }
    try {
        const Bundle bundle = runBundle();
        if (!selfCheck(bundle)) {
            std::cerr << "foundation trilinear self-check failed\n";
            return EXIT_FAILURE;
        }
        writeReport(bundle);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "foundation trilinear probe error: " << error.what()
                  << '\n';
        return EXIT_FAILURE;
    }
}

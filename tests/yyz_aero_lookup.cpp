#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-AERO-LOOKUP-001";
constexpr const char* kModelId = "MODEL-YYZ-AERO-TRILINEAR-LOOKUP-001";
constexpr const char* kTableId = "aero-table.fixture.yyz.multiaffine@1";
constexpr const char* kConfigurationId =
    "configuration.fixture.yyz.clean@1";
constexpr const char* kBodyFrameId = "frame.fixture.yyz.body@1";
constexpr const char* kClockDomain = "clock.fixture.yyz.simulation@1";
constexpr const char* kQuality = "Valid";
constexpr const char* kModelChoiceStatus = "accepted";
constexpr double kFormulaAbsolute = 2.0e-12;
constexpr double kFormulaRelative = 2.0e-12;

using Coefficients = std::array<double, 6>;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct PreparedTable {
    std::string layout;
    std::vector<double> mach_axis;
    std::vector<double> alpha_axis_rad;
    std::vector<double> beta_axis_rad;
    std::vector<Coefficients> rows;
};

struct Context {
    std::string model_id;
    std::string table_id;
    std::string configuration_id;
    std::string body_frame_id;
    std::string clock_domain;
    std::int64_t sample_tick = 0;
    std::int64_t configuration_revision = 0;
};

struct OperatingPoint {
    double mach = 0.0;
    double alpha_rad = 0.0;
    double beta_rad = 0.0;
    Vec3 omega_bi_b_radps;
    std::array<double, 4> surface_state_rad{};
    std::vector<std::string> required_derivative_set;
};

struct DimensionalizationProbe {
    double dynamic_pressure_pa = 0.0;
    double reference_area_m2 = 0.0;
    double reference_span_m = 0.0;
    double reference_chord_m = 0.0;
};

struct Input {
    std::string id;
    Context context;
    OperatingPoint operating;
    DimensionalizationProbe probe;
};

struct Bracket {
    std::size_t lower_index = 0;
    std::size_t upper_index = 0;
    double lower_value = 0.0;
    double upper_value = 0.0;
    double weight = 0.0;
};

struct IdentityResult {
    std::string model_id;
    std::string table_id;
    std::string configuration_id;
    std::string body_frame_id;
    std::string clock_domain;
    std::int64_t sample_tick = 0;
    std::int64_t configuration_revision = 0;
};

struct OperatingResult {
    double mach = 0.0;
    double alpha_rad = 0.0;
    double beta_rad = 0.0;
    Vec3 omega_bi_b_radps;
    std::array<double, 4> surface_state_rad{};
    std::size_t required_derivative_count = 0;
};

struct Response {
    std::string quality;
    std::string domain_status;
    Coefficients coefficients{};
};

struct DimensionalizationResult {
    double dynamic_pressure_pa = 0.0;
    double reference_area_m2 = 0.0;
    double reference_span_m = 0.0;
    double reference_chord_m = 0.0;
    Vec3 force_b_n;
    Vec3 moment_at_aero_reference_b_nm;
};

struct LookupResult {
    std::string id;
    IdentityResult identity;
    OperatingResult operating;
    Bracket mach_bracket;
    Bracket alpha_bracket;
    Bracket beta_bracket;
    Response response;
    DimensionalizationResult dimensionalization;
};

struct EquivalenceResult {
    std::string id;
    std::string status;
    Coefficients alternate_coefficients{};
    Vec3 alternate_force_b_n;
    Vec3 alternate_moment_b_nm;
    double max_abs_physical_difference = 0.0;
};

enum class LookupMode { Trilinear, Nearest, SwapAlphaBeta, LowerBetaFace };
enum class MutationKind { Nearest, ClampMach, SwapAlphaBeta, LowerBetaFace };

struct MutationResult {
    std::string id;
    std::string status;
    MutationKind kind = MutationKind::Nearest;
    LookupResult observed;
    double max_abs_physical_difference = 0.0;
    double out_of_domain_query_mach = 0.0;
    double observed_clamped_mach = 0.0;
};

struct ProbeResult {
    std::vector<LookupResult> cases;
    EquivalenceResult equivalence;
    std::vector<std::string> invalid_input_rejections;
    std::vector<MutationResult> mutations;
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

double canonicalZero(double value) {
    return value == 0.0 ? 0.0 : value;
}

Vec3 canonicalZero(const Vec3& value) {
    return {canonicalZero(value.x), canonicalZero(value.y),
            canonicalZero(value.z)};
}

Coefficients add(const Coefficients& lhs, const Coefficients& rhs) {
    Coefficients result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = canonicalZero(lhs[index] + rhs[index]);
    }
    return result;
}

Coefficients scale(const Coefficients& value, double factor) {
    Coefficients result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = canonicalZero(value[index] * factor);
    }
    return result;
}

Coefficients lerp(const Coefficients& lhs, const Coefficients& rhs,
                  double weight) {
    Coefficients result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = canonicalZero(
            lhs[index] + weight * (rhs[index] - lhs[index]));
    }
    return result;
}

bool near(double actual, double expected) {
    const double difference = std::abs(actual - expected);
    const double bound = kFormulaAbsolute + kFormulaRelative *
        std::max({1.0, std::abs(actual), std::abs(expected)});
    return difference <= bound;
}

double maxDifference(const Vec3& lhs, const Vec3& rhs) {
    return std::max({std::abs(lhs.x - rhs.x), std::abs(lhs.y - rhs.y),
                     std::abs(lhs.z - rhs.z)});
}

double maxDifference(const Coefficients& lhs, const Coefficients& rhs) {
    double maximum = 0.0;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        maximum = std::max(maximum, std::abs(lhs[index] - rhs[index]));
    }
    return maximum;
}

void validateAxis(const std::vector<double>& axis,
                  const std::string& label) {
    requireDomain(axis.size() >= 2, label + " must contain two knots");
    requireDomain(std::all_of(axis.begin(), axis.end(),
                              [](double value) { return finite(value); }),
                  label + " contains a non-finite knot");
    for (std::size_t index = 0; index + 1 < axis.size(); ++index) {
        requireDomain(axis[index] < axis[index + 1],
                      label + " must be strictly increasing");
    }
}

void validateTable(const PreparedTable& table) {
    requireDomain(table.layout == "mach-major-alpha-middle-beta-fastest",
                  "aero table layout differs");
    validateAxis(table.mach_axis, "Mach axis");
    validateAxis(table.alpha_axis_rad, "alpha axis");
    validateAxis(table.beta_axis_rad, "beta axis");
    const std::size_t expected_rows = table.mach_axis.size() *
        table.alpha_axis_rad.size() * table.beta_axis_rad.size();
    requireDomain(table.rows.size() == expected_rows,
                  "aero coefficient row count differs from grid shape");
    for (const Coefficients& row : table.rows) {
        requireDomain(std::all_of(
            row.begin(), row.end(),
            [](double value) { return finite(value); }),
            "aero table contains a non-finite coefficient");
    }
}

void validateInput(const Input& input, const PreparedTable& table) {
    validateTable(table);
    const Context& context = input.context;
    requireDomain(context.model_id == kModelId &&
                      context.table_id == kTableId &&
                      context.configuration_id == kConfigurationId &&
                      context.body_frame_id == kBodyFrameId &&
                      context.clock_domain == kClockDomain,
                  "aero lookup query identity differs");
    requireDomain(context.sample_tick >= 0 &&
                      context.configuration_revision >= 0,
                  "aero lookup tick or revision is invalid");
    const OperatingPoint& operating = input.operating;
    requireDomain(finite(operating.mach) &&
                      table.mach_axis.front() <= operating.mach &&
                      operating.mach <= table.mach_axis.back(),
                  "Mach is outside the validated aero table");
    requireDomain(finite(operating.alpha_rad) &&
                      table.alpha_axis_rad.front() <= operating.alpha_rad &&
                      operating.alpha_rad <= table.alpha_axis_rad.back(),
                  "alpha is outside the validated aero table");
    requireDomain(finite(operating.beta_rad) &&
                      table.beta_axis_rad.front() <= operating.beta_rad &&
                      operating.beta_rad <= table.beta_axis_rad.back(),
                  "beta is outside the validated aero table");
    requireDomain(finite(operating.omega_bi_b_radps) &&
                      operating.omega_bi_b_radps.x == 0.0 &&
                      operating.omega_bi_b_radps.y == 0.0 &&
                      operating.omega_bi_b_radps.z == 0.0,
                  "fixture-local lookup supports only zero body rates");
    requireDomain(std::all_of(
        operating.surface_state_rad.begin(),
        operating.surface_state_rad.end(),
        [](double value) { return finite(value) && value == 0.0; }),
        "fixture-local lookup supports only zero surfaces");
    requireDomain(operating.required_derivative_set.empty(),
                  "fixture-local lookup supports no derivative request");
    requireDomain(finite(input.probe.dynamic_pressure_pa) &&
                      input.probe.dynamic_pressure_pa >= 0.0 &&
                      finite(input.probe.reference_area_m2) &&
                      input.probe.reference_area_m2 > 0.0 &&
                      finite(input.probe.reference_span_m) &&
                      input.probe.reference_span_m > 0.0 &&
                      finite(input.probe.reference_chord_m) &&
                      input.probe.reference_chord_m > 0.0,
                  "dimensionalization probe is outside its domain");
}

PreparedTable acceptedTable() {
    return {
        "mach-major-alpha-middle-beta-fastest",
        {0.2, 0.6},
        {-0.1, 0.1},
        {-0.05, 0.05},
        {
            {0.006, 0.0245, -0.0795, 0.005, 0.014, -0.00755},
            {0.006, -0.0245, -0.0805, -0.005, 0.014, 0.00755},
            {0.05, 0.0245, 0.0795, 0.005, -0.106, -0.00785},
            {0.05, -0.0245, 0.0805, -0.005, -0.106, 0.00785},
            {0.018, 0.0235, -0.0795, 0.005, 0.022, -0.00795},
            {0.018, -0.0235, -0.0805, -0.005, 0.022, 0.00795},
            {0.07, 0.0235, 0.0795, 0.005, -0.098, -0.00825},
            {0.07, -0.0235, 0.0805, -0.005, -0.098, 0.00825},
        },
    };
}

std::vector<Input> acceptedInputs() {
    const Context first_context{kModelId, kTableId, kConfigurationId,
                                kBodyFrameId, kClockDomain, 0, 11};
    const Context second_context{kModelId, kTableId, kConfigurationId,
                                 kBodyFrameId, kClockDomain, 4, 11};
    const Context third_context{kModelId, kTableId, kConfigurationId,
                                kBodyFrameId, kClockDomain, 8, 12};
    return {
        {
            "CASE-YYZ-AERO-LOOKUP-INTERIOR",
            first_context,
            {0.35, 0.025, -0.0125, {0.0, 0.0, 0.0},
             {0.0, 0.0, 0.0, 0.0}, {}},
            {6125.0, 1.0, 1.0, 1.0},
        },
        {
            "CASE-YYZ-AERO-LOOKUP-EXACT-KNOT",
            second_context,
            {0.2, -0.1, 0.05, {0.0, 0.0, 0.0},
             {0.0, 0.0, 0.0, 0.0}, {}},
            {1000.0, 2.0, 3.0, 1.5},
        },
        {
            "CASE-YYZ-AERO-LOOKUP-UPPER-BOUNDARY",
            third_context,
            {0.6, 0.1, 0.05, {0.0, 0.0, 0.0},
             {0.0, 0.0, 0.0, 0.0}, {}},
            {500.0, 4.0, 2.0, 0.5},
        },
    };
}

Bracket bracket(const std::vector<double>& axis, double query,
                const std::string& label) {
    requireDomain(axis.front() <= query && query <= axis.back(),
                  label + " query is outside the prepared axis");
    std::size_t lower_index = 0;
    std::size_t upper_index = 1;
    double weight = 0.0;
    if (query == axis.back()) {
        lower_index = axis.size() - 2;
        upper_index = axis.size() - 1;
        weight = 1.0;
    } else {
        bool found = false;
        for (std::size_t index = 0; index + 1 < axis.size(); ++index) {
            if (axis[index] <= query && query < axis[index + 1]) {
                lower_index = index;
                upper_index = index + 1;
                found = true;
                break;
            }
        }
        requireDomain(found, label + " bracket was not found");
        weight = (query - axis[lower_index]) /
            (axis[upper_index] - axis[lower_index]);
    }
    return {lower_index, upper_index, axis[lower_index],
            axis[upper_index], weight};
}

std::size_t rowIndex(std::size_t mach_index, std::size_t alpha_index,
                     std::size_t beta_index, std::size_t alpha_count,
                     std::size_t beta_count) {
    return ((mach_index * alpha_count + alpha_index) * beta_count +
            beta_index);
}

Coefficients trilinear(const PreparedTable& table,
                       const Bracket& mach_bracket,
                       const Bracket& alpha_bracket,
                       const Bracket& beta_bracket,
                       double beta_weight) {
    Coefficients result{};
    const std::array<double, 3> weights{
        mach_bracket.weight, alpha_bracket.weight, beta_weight};
    for (std::size_t mach_corner = 0; mach_corner < 2; ++mach_corner) {
        const double mach_weight = mach_corner == 0
            ? 1.0 - weights[0] : weights[0];
        const std::size_t mach_index = mach_corner == 0
            ? mach_bracket.lower_index : mach_bracket.upper_index;
        for (std::size_t alpha_corner = 0; alpha_corner < 2;
             ++alpha_corner) {
            const double alpha_weight = alpha_corner == 0
                ? 1.0 - weights[1] : weights[1];
            const std::size_t alpha_index = alpha_corner == 0
                ? alpha_bracket.lower_index : alpha_bracket.upper_index;
            for (std::size_t beta_corner = 0; beta_corner < 2;
                 ++beta_corner) {
                const double corner_beta_weight = beta_corner == 0
                    ? 1.0 - weights[2] : weights[2];
                const std::size_t beta_index = beta_corner == 0
                    ? beta_bracket.lower_index : beta_bracket.upper_index;
                const Coefficients& corner = table.rows[rowIndex(
                    mach_index, alpha_index, beta_index,
                    table.alpha_axis_rad.size(), table.beta_axis_rad.size())];
                result = add(result, scale(
                    corner, mach_weight * alpha_weight * corner_beta_weight));
            }
        }
    }
    return result;
}

Coefficients alternateNested(const PreparedTable& table,
                             const Bracket& mach_bracket,
                             const Bracket& alpha_bracket,
                             const Bracket& beta_bracket) {
    std::array<Coefficients, 2> beta_faces{};
    for (std::size_t beta_corner = 0; beta_corner < 2; ++beta_corner) {
        const std::size_t beta_index = beta_corner == 0
            ? beta_bracket.lower_index : beta_bracket.upper_index;
        std::array<Coefficients, 2> alpha_edges{};
        for (std::size_t alpha_corner = 0; alpha_corner < 2;
             ++alpha_corner) {
            const std::size_t alpha_index = alpha_corner == 0
                ? alpha_bracket.lower_index : alpha_bracket.upper_index;
            const Coefficients& lower = table.rows[rowIndex(
                mach_bracket.lower_index, alpha_index, beta_index,
                table.alpha_axis_rad.size(), table.beta_axis_rad.size())];
            const Coefficients& upper = table.rows[rowIndex(
                mach_bracket.upper_index, alpha_index, beta_index,
                table.alpha_axis_rad.size(), table.beta_axis_rad.size())];
            alpha_edges[alpha_corner] = lerp(
                lower, upper, mach_bracket.weight);
        }
        beta_faces[beta_corner] = lerp(
            alpha_edges[0], alpha_edges[1], alpha_bracket.weight);
    }
    return lerp(beta_faces[0], beta_faces[1], beta_bracket.weight);
}

DimensionalizationResult dimensionalize(
    const Coefficients& coefficients,
    const DimensionalizationProbe& probe) {
    const double pressure_area =
        probe.dynamic_pressure_pa * probe.reference_area_m2;
    return {
        probe.dynamic_pressure_pa,
        probe.reference_area_m2,
        probe.reference_span_m,
        probe.reference_chord_m,
        canonicalZero({
            -pressure_area * coefficients[0],
            pressure_area * coefficients[1],
            -pressure_area * coefficients[2],
        }),
        canonicalZero({
            pressure_area * probe.reference_span_m * coefficients[3],
            pressure_area * probe.reference_chord_m * coefficients[4],
            pressure_area * probe.reference_span_m * coefficients[5],
        }),
    };
}

LookupResult lookup(const Input& input, const PreparedTable& table,
                    LookupMode mode = LookupMode::Trilinear) {
    validateInput(input, table);
    double alpha_query = input.operating.alpha_rad;
    double beta_query = input.operating.beta_rad;
    if (mode == LookupMode::SwapAlphaBeta) {
        std::swap(alpha_query, beta_query);
    }
    const Bracket mach_bracket = bracket(
        table.mach_axis, input.operating.mach, "Mach");
    const Bracket alpha_bracket = bracket(
        table.alpha_axis_rad, alpha_query, "alpha");
    const Bracket beta_bracket = bracket(
        table.beta_axis_rad, beta_query, "beta");
    Coefficients coefficients{};
    if (mode == LookupMode::Nearest) {
        const std::size_t mach_index = mach_bracket.weight > 0.5
            ? mach_bracket.upper_index : mach_bracket.lower_index;
        const std::size_t alpha_index = alpha_bracket.weight > 0.5
            ? alpha_bracket.upper_index : alpha_bracket.lower_index;
        const std::size_t beta_index = beta_bracket.weight > 0.5
            ? beta_bracket.upper_index : beta_bracket.lower_index;
        coefficients = table.rows[rowIndex(
            mach_index, alpha_index, beta_index,
            table.alpha_axis_rad.size(), table.beta_axis_rad.size())];
    } else {
        coefficients = trilinear(
            table, mach_bracket, alpha_bracket, beta_bracket,
            mode == LookupMode::LowerBetaFace ? 0.0 : beta_bracket.weight);
    }
    const bool boundary =
        input.operating.mach == table.mach_axis.front() ||
        input.operating.mach == table.mach_axis.back() ||
        alpha_query == table.alpha_axis_rad.front() ||
        alpha_query == table.alpha_axis_rad.back() ||
        beta_query == table.beta_axis_rad.front() ||
        beta_query == table.beta_axis_rad.back();
    return {
        input.id,
        {input.context.model_id, input.context.table_id,
         input.context.configuration_id, input.context.body_frame_id,
         input.context.clock_domain, input.context.sample_tick,
         input.context.configuration_revision},
        {input.operating.mach, alpha_query, beta_query,
         input.operating.omega_bi_b_radps,
         input.operating.surface_state_rad,
         input.operating.required_derivative_set.size()},
        mach_bracket,
        alpha_bracket,
        beta_bracket,
        {kQuality, boundary ? "Boundary" : "Inside", coefficients},
        dimensionalize(coefficients, input.probe),
    };
}

void append(std::vector<double>& destination,
            const Coefficients& coefficients) {
    destination.insert(destination.end(),
                       coefficients.begin(), coefficients.end());
}

void append(std::vector<double>& destination, const Vec3& value) {
    destination.push_back(value.x);
    destination.push_back(value.y);
    destination.push_back(value.z);
}

std::vector<double> physicalVector(const LookupResult& result) {
    std::vector<double> values;
    append(values, result.response.coefficients);
    append(values, result.dimensionalization.force_b_n);
    append(values, result.dimensionalization.moment_at_aero_reference_b_nm);
    return values;
}

double maxDifference(const std::vector<double>& lhs,
                     const std::vector<double>& rhs) {
    require(lhs.size() == rhs.size(), "physical vector shape differs");
    double maximum = 0.0;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        maximum = std::max(maximum, std::abs(lhs[index] - rhs[index]));
    }
    return maximum;
}

EquivalenceResult interpolationEquivalence(
    const Input& input, const PreparedTable& table,
    const LookupResult& accepted) {
    const Bracket mach_bracket = bracket(
        table.mach_axis, input.operating.mach, "Mach");
    const Bracket alpha_bracket = bracket(
        table.alpha_axis_rad, input.operating.alpha_rad, "alpha");
    const Bracket beta_bracket = bracket(
        table.beta_axis_rad, input.operating.beta_rad, "beta");
    const Coefficients alternate = alternateNested(
        table, mach_bracket, alpha_bracket, beta_bracket);
    const DimensionalizationResult consumer = dimensionalize(
        alternate, input.probe);
    const double difference = std::max({
        maxDifference(accepted.response.coefficients, alternate),
        maxDifference(accepted.dimensionalization.force_b_n,
                      consumer.force_b_n),
        maxDifference(accepted.dimensionalization.moment_at_aero_reference_b_nm,
                      consumer.moment_at_aero_reference_b_nm),
    });
    require(difference <= kFormulaAbsolute,
            "interpolation nesting order changed the aero response");
    return {
        "EQUIV-YYZ-AERO-LOOKUP-INTERPOLATION-ORDER",
        "passed",
        alternate,
        consumer.force_b_n,
        consumer.moment_at_aero_reference_b_nm,
        difference,
    };
}

void expectDomainRejection(
    std::vector<std::string>& rejected, const Input& accepted_input,
    const PreparedTable& accepted_table, const std::string& id,
    const std::function<void(Input&, PreparedTable&)>& mutate) {
    Input input = accepted_input;
    PreparedTable table = accepted_table;
    mutate(input, table);
    try {
        static_cast<void>(lookup(input, table));
    } catch (const std::domain_error&) {
        rejected.push_back(id);
        return;
    }
    throw std::runtime_error("invalid aero lookup input survived: " + id);
}

ProbeResult runProbe() {
    const PreparedTable table = acceptedTable();
    const std::vector<Input> inputs = acceptedInputs();
    std::vector<LookupResult> cases;
    for (const Input& input : inputs) {
        cases.push_back(lookup(input, table));
    }
    const LookupResult& accepted = cases[0];
    require(near(accepted.mach_bracket.weight, 0.375) &&
                near(accepted.alpha_bracket.weight, 0.625) &&
                near(accepted.beta_bracket.weight, 0.375) &&
                near(accepted.response.coefficients[0], 0.039875) &&
                near(accepted.response.coefficients[4], -0.058) &&
                near(accepted.dimensionalization.force_b_n.x, -244.234375) &&
                cases[1].response.domain_status == "Boundary" &&
                cases[2].response.domain_status == "Boundary" &&
                near(cases[2].response.coefficients[5], 0.00825),
            "accepted aero lookup anchors differ");

    std::vector<std::string> invalid;
    const Input& first = inputs[0];
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-CONFIGURATION",
        [](Input& value, PreparedTable&) {
            value.context.configuration_id =
                "configuration.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-CLOCK",
        [](Input& value, PreparedTable&) {
            value.context.clock_domain = "clock.fixture.yyz.other@1";
        });
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-NEGATIVE-TICK",
        [](Input& value, PreparedTable&) { value.context.sample_tick = -1; });
    expectDomainRejection(
        invalid, first, table,
        "INVALID-YYZ-AERO-LOOKUP-DERIVATIVE-REQUEST",
        [](Input& value, PreparedTable&) {
            value.operating.required_derivative_set = {"dC_m/dalpha"};
        });
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-NONZERO-RATE",
        [](Input& value, PreparedTable&) {
            value.operating.omega_bi_b_radps.y = 0.1;
        });
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-NONZERO-SURFACE",
        [](Input& value, PreparedTable&) {
            value.operating.surface_state_rad[2] = 0.05;
        });
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-MACH-LOW",
        [](Input& value, PreparedTable&) { value.operating.mach = 0.1; });
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-ALPHA-HIGH",
        [](Input& value, PreparedTable&) { value.operating.alpha_rad = 0.2; });
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-BETA-LOW",
        [](Input& value, PreparedTable&) { value.operating.beta_rad = -0.1; });
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-DUPLICATE-AXIS",
        [](Input&, PreparedTable& value) { value.mach_axis = {0.2, 0.2}; });
    expectDomainRejection(
        invalid, first, table, "INVALID-YYZ-AERO-LOOKUP-ROW-COUNT",
        [](Input&, PreparedTable& value) { value.rows.pop_back(); });
    expectDomainRejection(
        invalid, first, table,
        "INVALID-YYZ-AERO-LOOKUP-NONFINITE-COEFFICIENT",
        [](Input&, PreparedTable& value) {
            value.rows[0][0] = std::numeric_limits<double>::quiet_NaN();
        });

    const LookupResult nearest = lookup(first, table, LookupMode::Nearest);
    Input clamped_input = first;
    clamped_input.operating.mach = table.mach_axis.front();
    const LookupResult clamped = lookup(clamped_input, table);
    const LookupResult swapped = lookup(
        first, table, LookupMode::SwapAlphaBeta);
    const LookupResult lower_beta = lookup(
        first, table, LookupMode::LowerBetaFace);
    std::vector<MutationResult> mutations{
        {"MUTATION-YYZ-AERO-LOOKUP-NEAREST-NEIGHBOR", "rejected",
         MutationKind::Nearest, nearest,
         maxDifference(physicalVector(accepted), physicalVector(nearest)),
         0.0, 0.0},
        {"MUTATION-YYZ-AERO-LOOKUP-CLAMP-MACH", "rejected",
         MutationKind::ClampMach, clamped,
         maxDifference(physicalVector(accepted), physicalVector(clamped)),
         0.1, table.mach_axis.front()},
        {"MUTATION-YYZ-AERO-LOOKUP-SWAP-ALPHA-BETA", "rejected",
         MutationKind::SwapAlphaBeta, swapped,
         maxDifference(physicalVector(accepted), physicalVector(swapped)),
         0.0, 0.0},
        {"MUTATION-YYZ-AERO-LOOKUP-LOWER-BETA-FACE", "rejected",
         MutationKind::LowerBetaFace, lower_beta,
         maxDifference(physicalVector(accepted), physicalVector(lower_beta)),
         0.0, 0.0},
    };
    require(near(mutations[0].max_abs_physical_difference, 364.62890625) &&
                near(mutations[1].max_abs_physical_difference, 39.046875) &&
                near(mutations[2].max_abs_physical_difference, 183.75) &&
                near(mutations[3].max_abs_physical_difference, 110.82421875),
            "an aero lookup mutation matched the accepted response");
    return {cases, interpolationEquivalence(first, table, accepted),
            invalid, mutations};
}

void writeNumber(double value) {
    std::cout << canonicalZero(value);
}

void writeVec3(const Vec3& value) {
    std::cout << '[';
    writeNumber(value.x);
    std::cout << ',';
    writeNumber(value.y);
    std::cout << ',';
    writeNumber(value.z);
    std::cout << ']';
}

void writeCoefficients(const Coefficients& value) {
    std::cout << '[';
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeNumber(value[index]);
    }
    std::cout << ']';
}

void writeSurface(const std::array<double, 4>& value) {
    std::cout << '[';
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeNumber(value[index]);
    }
    std::cout << ']';
}

void writeBracket(const Bracket& value) {
    std::cout << "{\"lower_index\":" << value.lower_index
              << ",\"upper_index\":" << value.upper_index
              << ",\"lower_value\":";
    writeNumber(value.lower_value);
    std::cout << ",\"upper_value\":";
    writeNumber(value.upper_value);
    std::cout << ",\"weight\":";
    writeNumber(value.weight);
    std::cout << '}';
}

void writeLookup(const LookupResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"query_identity\":{\"model_id\":\""
              << value.identity.model_id
              << "\",\"table_id\":\"" << value.identity.table_id
              << "\",\"configuration_id\":\""
              << value.identity.configuration_id
              << "\",\"body_frame_id\":\""
              << value.identity.body_frame_id
              << "\",\"clock_domain\":\""
              << value.identity.clock_domain
              << "\",\"sample_tick\":" << value.identity.sample_tick
              << ",\"configuration_revision\":"
              << value.identity.configuration_revision
              << "},\"operating_point\":{\"mach\":";
    writeNumber(value.operating.mach);
    std::cout << ",\"alpha_rad\":";
    writeNumber(value.operating.alpha_rad);
    std::cout << ",\"beta_rad\":";
    writeNumber(value.operating.beta_rad);
    std::cout << ",\"omega_BI_B_radps\":";
    writeVec3(value.operating.omega_bi_b_radps);
    std::cout << ",\"surface_state_rad\":";
    writeSurface(value.operating.surface_state_rad);
    std::cout << ",\"required_derivative_count\":"
              << value.operating.required_derivative_count
              << "},\"brackets\":{\"mach\":";
    writeBracket(value.mach_bracket);
    std::cout << ",\"alpha\":";
    writeBracket(value.alpha_bracket);
    std::cout << ",\"beta\":";
    writeBracket(value.beta_bracket);
    std::cout << "},\"response\":{\"quality\":\""
              << value.response.quality
              << "\",\"domain_status\":\""
              << value.response.domain_status
              << "\",\"coefficient_order\":[\"C_A\",\"C_Y\",\"C_N\","
                 "\"C_l\",\"C_m\",\"C_n\"],\"coefficients\":";
    writeCoefficients(value.response.coefficients);
    std::cout << "},\"dimensionalization_consumer\":{"
                 "\"dynamic_pressure_Pa\":";
    writeNumber(value.dimensionalization.dynamic_pressure_pa);
    std::cout << ",\"reference_area_m2\":";
    writeNumber(value.dimensionalization.reference_area_m2);
    std::cout << ",\"reference_span_m\":";
    writeNumber(value.dimensionalization.reference_span_m);
    std::cout << ",\"reference_chord_m\":";
    writeNumber(value.dimensionalization.reference_chord_m);
    std::cout << ",\"force_B_N\":";
    writeVec3(value.dimensionalization.force_b_n);
    std::cout << ",\"moment_at_aero_reference_B_Nm\":";
    writeVec3(value.dimensionalization.moment_at_aero_reference_b_nm);
    std::cout << "}}";
}

void writeEquivalence(const EquivalenceResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"status\":\"" << value.status
              << "\",\"alternate_coefficients\":";
    writeCoefficients(value.alternate_coefficients);
    std::cout << ",\"alternate_force_B_N\":";
    writeVec3(value.alternate_force_b_n);
    std::cout << ",\"alternate_moment_at_aero_reference_B_Nm\":";
    writeVec3(value.alternate_moment_b_nm);
    std::cout << ",\"max_abs_physical_difference\":";
    writeNumber(value.max_abs_physical_difference);
    std::cout << '}';
}

void writeMutation(const MutationResult& value) {
    std::cout << "{\"id\":\"" << value.id
              << "\",\"status\":\"" << value.status
              << "\",\"observed_coefficients\":";
    writeCoefficients(value.observed.response.coefficients);
    std::cout << ",\"observed_force_B_N\":";
    writeVec3(value.observed.dimensionalization.force_b_n);
    std::cout << ",\"observed_moment_at_aero_reference_B_Nm\":";
    writeVec3(value.observed.dimensionalization.moment_at_aero_reference_b_nm);
    std::cout << ",\"max_abs_physical_difference\":";
    writeNumber(value.max_abs_physical_difference);
    if (value.kind == MutationKind::ClampMach) {
        std::cout << ",\"out_of_domain_query_mach\":";
        writeNumber(value.out_of_domain_query_mach);
        std::cout << ",\"observed_clamped_mach\":";
        writeNumber(value.observed_clamped_mach);
    } else if (value.kind == MutationKind::SwapAlphaBeta) {
        std::cout << ",\"observed_alpha_rad\":";
        writeNumber(value.observed.operating.alpha_rad);
        std::cout << ",\"observed_beta_rad\":";
        writeNumber(value.observed.operating.beta_rad);
    }
    std::cout << '}';
}

void writeStringList(const std::vector<std::string>& values) {
    std::cout << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << '\"' << values[index] << '\"';
    }
    std::cout << ']';
}

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"model_id\":\"" << kModelId
              << "\",\"status\":\"passed\""
              << ",\"model_choice_status\":\""
              << kModelChoiceStatus << "\",\"cases\":[";
    for (std::size_t index = 0; index < result.cases.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeLookup(result.cases[index]);
    }
    std::cout << "],\"equivalence_results\":[";
    writeEquivalence(result.equivalence);
    std::cout << "],\"invalid_input_rejections\":";
    writeStringList(result.invalid_input_rejections);
    std::cout << ",\"mutation_results\":[";
    for (std::size_t index = 0; index < result.mutations.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeMutation(result.mutations[index]);
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_yyz_aero_lookup_probe --self-check\n";
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

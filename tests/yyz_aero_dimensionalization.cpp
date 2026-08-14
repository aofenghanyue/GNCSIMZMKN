#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kOracleId =
    "ORACLE-YYZ-AERO-DIMENSIONALIZATION-001";
constexpr const char* kModelId =
    "MODEL-YYZ-AERO-DIMENSIONALIZATION-001";
constexpr const char* kModelChoiceStatus = "accepted";
constexpr const char* kBodyFrameId = "frame.fixture.yyz.body@1";
constexpr const char* kBodyAxes = "x-forward_y-right_z-down";
constexpr const char* kClockDomain =
    "clock.fixture.yyz.simulation@1";
constexpr const char* kCoefficientConventionId =
    "convention.fixture.yyz.aero-body-axes@1";
constexpr const char* kSourceId = "aero.body";
constexpr double kFormulaAbsolute = 2.0e-12;
constexpr double kFormulaRelative = 2.0e-12;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct CoefficientVector {
    double c_a = 0.0;
    double c_y = 0.0;
    double c_n_normal = 0.0;
    double c_l = 0.0;
    double c_m = 0.0;
    double c_n_yaw = 0.0;
};

struct Context {
    std::string body_frame_id;
    std::string body_axes;
    std::uint64_t sample_tick = 0;
    std::string clock_domain;
    std::uint64_t configuration_revision = 0;
};

struct AirDataSample {
    double dynamic_pressure_pa = 0.0;
    std::uint64_t sample_tick = 0;
    std::string clock_domain;
    std::uint64_t configuration_revision = 0;
};

struct GeometrySample {
    double reference_area_m2 = 0.0;
    double reference_span_m = 0.0;
    double reference_chord_m = 0.0;
    Vec3 r_com_to_aero_ref_b_m;
    std::string body_frame_id;
    std::uint64_t sample_tick = 0;
    std::string clock_domain;
    std::uint64_t configuration_revision = 0;
};

struct CoefficientSample {
    std::string convention_id;
    CoefficientVector value;
    std::string body_frame_id;
    std::uint64_t sample_tick = 0;
    std::string clock_domain;
    std::uint64_t configuration_revision = 0;
};

struct AeroInput {
    std::string id;
    Context context;
    AirDataSample air_data;
    GeometrySample geometry;
    CoefficientSample coefficients;
};

enum class ForceMode {
    Accepted,
    DirectBodySignsMutation,
};

enum class MomentMode {
    Accepted,
    SingleSpanMutation,
};

enum class TransportMode {
    Accepted,
    ReversedReferenceVectorMutation,
};

struct FormulaOptions {
    ForceMode force = ForceMode::Accepted;
    MomentMode moment = MomentMode::Accepted;
    TransportMode transport = TransportMode::Accepted;
};

struct ClosureContribution {
    std::string source_id;
    std::string body_frame_id;
    std::uint64_t configuration_revision = 0;
    std::uint64_t valid_from_tick = 0;
    std::uint64_t valid_until_tick = 0;
    Vec3 force_b_n;
    Vec3 r_com_to_application_b_m;
    Vec3 moment_at_application_b_nm;
};

struct AeroResult {
    std::string id;
    Context context;
    std::string coefficient_convention_id;
    double dynamic_pressure_pa = 0.0;
    double reference_area_m2 = 0.0;
    double reference_span_m = 0.0;
    double reference_chord_m = 0.0;
    CoefficientVector coefficient_vector;
    Vec3 r_com_to_aero_ref_b_m;
    double pressure_area_n = 0.0;
    Vec3 force_coefficient_vector_b;
    Vec3 force_at_aero_ref_b_n;
    Vec3 moment_length_coefficient_vector_b_m;
    Vec3 moment_at_aero_ref_b_nm;
    Vec3 transport_moment_b_nm;
    Vec3 moment_about_com_b_nm;
    ClosureContribution closure_contribution;
};

struct ProbeResult {
    std::vector<AeroResult> cases;
    std::vector<std::string> equivalence_checks;
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

bool finite(const CoefficientVector& value) {
    return finite(value.c_a) && finite(value.c_y) &&
        finite(value.c_n_normal) && finite(value.c_l) &&
        finite(value.c_m) && finite(value.c_n_yaw);
}

double canonicalZero(double value) {
    return value == 0.0 ? 0.0 : value;
}

Vec3 canonicalZero(const Vec3& value) {
    return {
        canonicalZero(value.x),
        canonicalZero(value.y),
        canonicalZero(value.z),
    };
}

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero({
        lhs.x + rhs.x,
        lhs.y + rhs.y,
        lhs.z + rhs.z,
    });
}

Vec3 scale(const Vec3& value, double factor) {
    return canonicalZero({
        value.x * factor,
        value.y * factor,
        value.z * factor,
    });
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return canonicalZero({
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    });
}

bool near(double actual, double expected) {
    const double bound = kFormulaAbsolute + kFormulaRelative *
        std::max(std::abs(actual), std::abs(expected));
    return finite(actual) && finite(expected) &&
        std::abs(actual - expected) <= bound;
}

bool near(const Vec3& actual, const Vec3& expected) {
    return near(actual.x, expected.x) &&
        near(actual.y, expected.y) && near(actual.z, expected.z);
}

void validateIdentity(const AeroInput& input) {
    const Context& context = input.context;
    requireDomain(context.body_frame_id == kBodyFrameId,
                  "aero body frame differs");
    requireDomain(context.body_axes == kBodyAxes,
                  "aero body axes differ");
    requireDomain(context.clock_domain == kClockDomain,
                  "aero clock domain differs");
    requireDomain(
        input.air_data.sample_tick == context.sample_tick &&
            input.air_data.clock_domain == context.clock_domain &&
            input.air_data.configuration_revision ==
                context.configuration_revision,
        "air-data identity differs from aero context");
    requireDomain(
        input.geometry.body_frame_id == context.body_frame_id &&
            input.geometry.sample_tick == context.sample_tick &&
            input.geometry.clock_domain == context.clock_domain &&
            input.geometry.configuration_revision ==
                context.configuration_revision,
        "reference-geometry identity differs from aero context");
    requireDomain(input.coefficients.convention_id ==
                      kCoefficientConventionId,
                  "aero coefficient convention differs");
    requireDomain(
        input.coefficients.body_frame_id == context.body_frame_id &&
            input.coefficients.sample_tick == context.sample_tick &&
            input.coefficients.clock_domain == context.clock_domain &&
            input.coefficients.configuration_revision ==
                context.configuration_revision,
        "aero coefficient identity differs from context");
}

AeroResult dimensionalize(const AeroInput& input,
                          const FormulaOptions& options = {}) {
    validateIdentity(input);
    requireDomain(finite(input.air_data.dynamic_pressure_pa) &&
                      input.air_data.dynamic_pressure_pa >= 0.0,
                  "dynamic pressure must be finite and nonnegative");
    requireDomain(finite(input.geometry.reference_area_m2) &&
                      input.geometry.reference_area_m2 > 0.0,
                  "reference area must be finite and positive");
    requireDomain(finite(input.geometry.reference_span_m) &&
                      input.geometry.reference_span_m > 0.0,
                  "reference span must be finite and positive");
    requireDomain(finite(input.geometry.reference_chord_m) &&
                      input.geometry.reference_chord_m > 0.0,
                  "reference chord must be finite and positive");
    requireDomain(finite(input.geometry.r_com_to_aero_ref_b_m),
                  "aero reference point must be finite");
    requireDomain(finite(input.coefficients.value),
                  "aero coefficients must be finite");

    const CoefficientVector& coefficient = input.coefficients.value;
    const Vec3 force_coefficients =
        options.force == ForceMode::Accepted
        ? Vec3{-coefficient.c_a, coefficient.c_y,
               -coefficient.c_n_normal}
        : Vec3{coefficient.c_a, coefficient.c_y,
               coefficient.c_n_normal};
    const double pitch_length =
        options.moment == MomentMode::Accepted
        ? input.geometry.reference_chord_m
        : input.geometry.reference_span_m;
    const Vec3 moment_length_coefficients{
        input.geometry.reference_span_m * coefficient.c_l,
        pitch_length * coefficient.c_m,
        input.geometry.reference_span_m * coefficient.c_n_yaw,
    };
    const double pressure_area = canonicalZero(
        input.air_data.dynamic_pressure_pa *
        input.geometry.reference_area_m2);
    const Vec3 force = scale(force_coefficients, pressure_area);
    const Vec3 moment_at_reference = scale(
        moment_length_coefficients, pressure_area);
    const Vec3 transport_vector =
        options.transport == TransportMode::Accepted
        ? input.geometry.r_com_to_aero_ref_b_m
        : scale(input.geometry.r_com_to_aero_ref_b_m, -1.0);
    const Vec3 transport_moment = cross(transport_vector, force);
    const Vec3 moment_about_com = add(
        moment_at_reference, transport_moment);
    requireDomain(finite(force_coefficients) &&
                      finite(moment_length_coefficients) && finite(force) &&
                      finite(moment_at_reference) &&
                      finite(transport_moment) && finite(moment_about_com),
                  "aero dimensionalization produced a non-finite value");

    const ClosureContribution closure{
        kSourceId,
        input.context.body_frame_id,
        input.context.configuration_revision,
        input.context.sample_tick,
        input.context.sample_tick + 1,
        force,
        input.geometry.r_com_to_aero_ref_b_m,
        moment_at_reference,
    };
    return {
        input.id,
        input.context,
        kCoefficientConventionId,
        input.air_data.dynamic_pressure_pa,
        input.geometry.reference_area_m2,
        input.geometry.reference_span_m,
        input.geometry.reference_chord_m,
        coefficient,
        input.geometry.r_com_to_aero_ref_b_m,
        pressure_area,
        canonicalZero(force_coefficients),
        force,
        canonicalZero(moment_length_coefficients),
        moment_at_reference,
        transport_moment,
        moment_about_com,
        closure,
    };
}

Context makeContext(std::uint64_t tick, std::uint64_t revision) {
    return {kBodyFrameId, kBodyAxes, tick, kClockDomain, revision};
}

AeroInput makeInput(const std::string& id, std::uint64_t tick,
                    std::uint64_t revision, double dynamic_pressure_pa,
                    double area_m2, double span_m, double chord_m,
                    const Vec3& reference_point,
                    const CoefficientVector& coefficients) {
    const Context context = makeContext(tick, revision);
    return {
        id,
        context,
        {dynamic_pressure_pa, tick, kClockDomain, revision},
        {
            area_m2,
            span_m,
            chord_m,
            reference_point,
            kBodyFrameId,
            tick,
            kClockDomain,
            revision,
        },
        {
            kCoefficientConventionId,
            coefficients,
            kBodyFrameId,
            tick,
            kClockDomain,
            revision,
        },
    };
}

std::vector<AeroInput> caseInputs() {
    return {
        makeInput(
            "CASE-YYZ-AERO-DIMENSIONALIZATION-ASYMMETRIC",
            17, 9, 25000.0, 0.12, 1.8, 0.45, {0.2, 0.0, -0.1},
            {0.10, -0.02, 0.30, 0.01, -0.04, 0.03}),
        makeInput(
            "CASE-YYZ-AERO-DIMENSIONALIZATION-SEPARATE-LENGTHS",
            18, 9, 800.0, 2.5, 12.0, 3.0, {0.0, 0.0, 0.0},
            {0.0, 0.0, 0.0, 0.02, -0.03, 0.04}),
        makeInput(
            "CASE-YYZ-AERO-DIMENSIONALIZATION-ZERO-DYNAMIC-PRESSURE",
            19, 10, 0.0, 4.0, 7.0, 2.0, {-3.0, 4.0, 0.5},
            {-0.5, 0.25, 0.75, 0.1, -0.2, 0.3}),
    };
}

bool sameWrench(const AeroResult& lhs, const AeroResult& rhs) {
    return near(lhs.force_at_aero_ref_b_n,
                rhs.force_at_aero_ref_b_n) &&
        near(lhs.moment_at_aero_ref_b_nm,
             rhs.moment_at_aero_ref_b_nm) &&
        near(lhs.transport_moment_b_nm, rhs.transport_moment_b_nm) &&
        near(lhs.moment_about_com_b_nm, rhs.moment_about_com_b_nm);
}

template <typename Operation>
bool rejected(Operation&& operation) {
    try {
        operation();
    } catch (const std::domain_error&) {
        return true;
    }
    return false;
}

void recordInvalid(ProbeResult& result, const std::string& identifier,
                   const AeroInput& input) {
    if (rejected([&] { static_cast<void>(dimensionalize(input)); })) {
        result.invalid_input_rejections.push_back(identifier);
    }
}

ProbeResult runProbe() {
    ProbeResult result;
    const std::vector<AeroInput> inputs = caseInputs();
    for (const AeroInput& input : inputs) {
        result.cases.push_back(dimensionalize(input));
    }
    require(result.cases.size() == 3,
            "aero executable case coverage is incomplete");
    require(near(result.cases[0].pressure_area_n, 3000.0) &&
                near(result.cases[0].force_at_aero_ref_b_n,
                     {-300.0, -60.0, -900.0}) &&
                near(result.cases[0].moment_at_aero_ref_b_nm,
                     {54.0, -54.0, 162.0}) &&
                near(result.cases[0].transport_moment_b_nm,
                     {-6.0, 210.0, -12.0}) &&
                near(result.cases[0].moment_about_com_b_nm,
                     {48.0, 156.0, 150.0}),
            "asymmetric aero anchor differs");
    require(near(result.cases[1].moment_at_aero_ref_b_nm,
                 {480.0, -180.0, 960.0}),
            "separate reference-length anchor differs");
    require(near(result.cases[2].force_at_aero_ref_b_n,
                 {0.0, 0.0, 0.0}) &&
                near(result.cases[2].moment_about_com_b_nm,
                     {0.0, 0.0, 0.0}),
            "zero-dynamic-pressure anchor differs");

    AeroInput equivalent = inputs[0];
    equivalent.air_data.dynamic_pressure_pa *= 5.0;
    equivalent.geometry.reference_area_m2 /= 5.0;
    if (sameWrench(result.cases[0], dimensionalize(equivalent))) {
        result.equivalence_checks.push_back(
            "EQUIV-YYZ-AERO-PRESSURE-AREA-FACTORIZATION");
    }
    equivalent = inputs[0];
    equivalent.geometry.reference_span_m *= 2.0;
    equivalent.geometry.reference_chord_m *= 2.0;
    equivalent.coefficients.value.c_l /= 2.0;
    equivalent.coefficients.value.c_m /= 2.0;
    equivalent.coefficients.value.c_n_yaw /= 2.0;
    if (sameWrench(result.cases[0], dimensionalize(equivalent))) {
        result.equivalence_checks.push_back(
            "EQUIV-YYZ-AERO-LENGTH-COEFFICIENT-FACTORIZATION");
    }
    require(result.equivalence_checks.size() == 2,
            "an aero equivalence check failed");

    AeroInput invalid = inputs[0];
    invalid.coefficients.body_frame_id = "frame.other@1";
    recordInvalid(result, "INVALID-YYZ-AERO-FRAME-MISMATCH", invalid);

    invalid = inputs[0];
    invalid.air_data.clock_domain = "clock.other@1";
    recordInvalid(result, "INVALID-YYZ-AERO-CLOCK-MISMATCH", invalid);

    invalid = inputs[0];
    ++invalid.geometry.sample_tick;
    recordInvalid(result, "INVALID-YYZ-AERO-SAMPLE-TICK-MISMATCH",
                  invalid);

    invalid = inputs[0];
    ++invalid.coefficients.configuration_revision;
    recordInvalid(result, "INVALID-YYZ-AERO-REVISION-MISMATCH", invalid);

    invalid = inputs[0];
    invalid.coefficients.value.c_a =
        std::numeric_limits<double>::infinity();
    recordInvalid(result, "INVALID-YYZ-AERO-NONFINITE-COEFFICIENT",
                  invalid);

    invalid = inputs[0];
    invalid.air_data.dynamic_pressure_pa = -1.0;
    recordInvalid(result, "INVALID-YYZ-AERO-NEGATIVE-DYNAMIC-PRESSURE",
                  invalid);

    invalid = inputs[0];
    invalid.geometry.reference_area_m2 = 0.0;
    recordInvalid(result, "INVALID-YYZ-AERO-NONPOSITIVE-AREA", invalid);

    invalid = inputs[0];
    invalid.geometry.reference_span_m = 0.0;
    recordInvalid(result, "INVALID-YYZ-AERO-NONPOSITIVE-SPAN", invalid);

    invalid = inputs[0];
    invalid.geometry.reference_chord_m = 0.0;
    recordInvalid(result, "INVALID-YYZ-AERO-NONPOSITIVE-CHORD", invalid);

    invalid = inputs[0];
    invalid.geometry.r_com_to_aero_ref_b_m.x =
        std::numeric_limits<double>::infinity();
    recordInvalid(result,
                  "INVALID-YYZ-AERO-NONFINITE-REFERENCE-POINT", invalid);
    require(result.invalid_input_rejections.size() == 10,
            "an invalid aero input was accepted");

    FormulaOptions mutation;
    mutation.force = ForceMode::DirectBodySignsMutation;
    if (!sameWrench(result.cases[0], dimensionalize(inputs[0], mutation))) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-AERO-DIRECT-BODY-SIGNS");
    }
    mutation = {};
    mutation.moment = MomentMode::SingleSpanMutation;
    if (!sameWrench(result.cases[0], dimensionalize(inputs[0], mutation))) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-AERO-SINGLE-REFERENCE-LENGTH");
    }
    mutation = {};
    mutation.transport = TransportMode::ReversedReferenceVectorMutation;
    if (!sameWrench(result.cases[0], dimensionalize(inputs[0], mutation))) {
        result.mutation_rejections.push_back(
            "MUTATION-YYZ-AERO-REVERSED-REFERENCE-VECTOR");
    }
    require(result.mutation_rejections.size() == 3,
            "an aero physical mutation matched the accepted result");
    return result;
}

void writeVec3(const Vec3& value) {
    std::cout << '[' << value.x << ',' << value.y << ',' << value.z << ']';
}

void writeCoefficientVector(const CoefficientVector& value) {
    std::cout << '[' << value.c_a << ',' << value.c_y << ','
              << value.c_n_normal << ',' << value.c_l << ','
              << value.c_m << ',' << value.c_n_yaw << ']';
}

void writeContext(const Context& context) {
    std::cout << "{\"body_frame_id\":\"" << context.body_frame_id
              << "\",\"body_axes\":\"" << context.body_axes
              << "\",\"sample_tick\":" << context.sample_tick
              << ",\"clock_domain\":\"" << context.clock_domain
              << "\",\"configuration_revision\":"
              << context.configuration_revision << '}';
}

void writeClosureContribution(const ClosureContribution& value) {
    std::cout << "{\"source_id\":\"" << value.source_id
              << "\",\"body_frame_id\":\"" << value.body_frame_id
              << "\",\"configuration_revision\":"
              << value.configuration_revision
              << ",\"valid_from_tick\":" << value.valid_from_tick
              << ",\"valid_until_tick\":" << value.valid_until_tick
              << ",\"force_B_N\":";
    writeVec3(value.force_b_n);
    std::cout << ",\"r_CoM_to_application_B_m\":";
    writeVec3(value.r_com_to_application_b_m);
    std::cout << ",\"moment_at_application_B_Nm\":";
    writeVec3(value.moment_at_application_b_nm);
    std::cout << '}';
}

void writeCase(const AeroResult& value) {
    std::cout << "{\"id\":\"" << value.id << "\",\"context\":";
    writeContext(value.context);
    std::cout << ",\"coefficient_convention_id\":\""
              << value.coefficient_convention_id
              << "\",\"dynamic_pressure_Pa\":"
              << value.dynamic_pressure_pa
              << ",\"reference_area_m2\":" << value.reference_area_m2
              << ",\"reference_span_m\":" << value.reference_span_m
              << ",\"reference_chord_m\":" << value.reference_chord_m
              << ",\"coefficient_vector_CA_CY_CN_Cl_Cm_Cn\":";
    writeCoefficientVector(value.coefficient_vector);
    std::cout << ",\"r_CoM_to_aero_ref_B_m\":";
    writeVec3(value.r_com_to_aero_ref_b_m);
    std::cout << ",\"pressure_area_N\":" << value.pressure_area_n
              << ",\"force_coefficient_vector_B\":";
    writeVec3(value.force_coefficient_vector_b);
    std::cout << ",\"force_at_aero_ref_B_N\":";
    writeVec3(value.force_at_aero_ref_b_n);
    std::cout << ",\"moment_length_coefficient_vector_B_m\":";
    writeVec3(value.moment_length_coefficient_vector_b_m);
    std::cout << ",\"moment_at_aero_ref_B_Nm\":";
    writeVec3(value.moment_at_aero_ref_b_nm);
    std::cout << ",\"transport_moment_B_Nm\":";
    writeVec3(value.transport_moment_b_nm);
    std::cout << ",\"moment_about_CoM_B_Nm\":";
    writeVec3(value.moment_about_com_b_nm);
    std::cout << ",\"closure_contribution\":";
    writeClosureContribution(value.closure_contribution);
    std::cout << '}';
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
              << "\",\"model_id\":\"" << kModelId
              << "\",\"status\":\"passed\""
              << ",\"model_choice_status\":\""
              << kModelChoiceStatus << "\",\"cases\":[";
    for (std::size_t index = 0; index < result.cases.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeCase(result.cases[index]);
    }
    std::cout << "],\"equivalence_checks\":";
    writeStringList(result.equivalence_checks);
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
            "usage: gnc_yyz_aero_dimensionalization_probe --self-check\n";
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

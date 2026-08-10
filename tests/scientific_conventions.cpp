#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr double kAbsoluteTolerance = 1.0e-12;
constexpr double kRelativeTolerance = 1.0e-12;
constexpr std::size_t kRandomRotationSamples = 256;
constexpr std::string_view kConventionId = "SCI-CONVENTIONS-001";
constexpr std::string_view kOutputSchema =
    "gnczmkn.scientific-check-output/1";

struct Vec3 {
    double x;
    double y;
    double z;
};

struct Quaternion {
    double w;
    double x;
    double y;
    double z;
};

using Matrix3 = std::array<double, 9>;

struct Observation {
    std::string id;
    std::vector<double> values;
};

struct Check {
    std::string id;
    bool passed = true;
    double max_error = 0.0;
    std::size_t assertion_count = 0;
};

const std::array<std::string_view, 18> kRequiredCheckIds = {
    "quaternion.direction",
    "quaternion.hamilton-product",
    "quaternion.composition",
    "quaternion.inverse",
    "quaternion.matrix-equivalence",
    "quaternion.orthogonality",
    "quaternion.determinant",
    "quaternion.sign-equivalence",
    "quaternion.serialization-wxyz",
    "quaternion.zero-norm-domain-error",
    "quaternion.body-rate-derivative",
    "quaternion.euler-round-trip",
    "quaternion.euler-singularity",
    "units.si-boundary",
    "frames.point-versus-free-vector",
    "time.integer-tick",
    "time.duration-alignment",
    "time.type-separation",
};

class CheckBook {
public:
    CheckBook() {
        checks_.reserve(kRequiredCheckIds.size());
        for (const auto id : kRequiredCheckIds) {
            checks_.push_back(Check{std::string{id}});
        }
    }

    void observe(std::string_view id, bool passed, double error = 0.0) {
        auto iterator = std::find_if(
            checks_.begin(), checks_.end(),
            [id](const Check& check) { return check.id == id; });
        if (iterator == checks_.end()) {
            throw std::logic_error("unknown scientific check id");
        }
        iterator->passed = iterator->passed && passed;
        iterator->max_error = std::max(iterator->max_error, error);
        ++iterator->assertion_count;
    }

    [[nodiscard]] bool allPassed() const {
        return std::all_of(checks_.begin(), checks_.end(), [](const Check& check) {
            return check.passed && check.assertion_count > 0 &&
                   std::isfinite(check.max_error);
        });
    }

    [[nodiscard]] const std::vector<Check>& checks() const { return checks_; }

private:
    std::vector<Check> checks_;
};

Vec3 add(const Vec3& lhs, const Vec3& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 scale(const Vec3& value, double factor) {
    return {factor * value.x, factor * value.y, factor * value.z};
}

Quaternion scale(const Quaternion& value, double factor) {
    return {factor * value.w, factor * value.x, factor * value.y,
            factor * value.z};
}

double dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

Quaternion hamilton(const Quaternion& lhs, const Quaternion& rhs) {
    const Vec3 lhs_vector{lhs.x, lhs.y, lhs.z};
    const Vec3 rhs_vector{rhs.x, rhs.y, rhs.z};
    const Vec3 vector = add(
        add(scale(rhs_vector, lhs.w), scale(lhs_vector, rhs.w)),
        cross(lhs_vector, rhs_vector));
    return {
        lhs.w * rhs.w - dot(lhs_vector, rhs_vector),
        vector.x,
        vector.y,
        vector.z,
    };
}

Quaternion conjugate(const Quaternion& value) {
    return {value.w, -value.x, -value.y, -value.z};
}

double normSquared(const Quaternion& value) {
    return value.w * value.w + value.x * value.x + value.y * value.y +
           value.z * value.z;
}

Quaternion normalize(const Quaternion& value) {
    const double squared = normSquared(value);
    if (squared == 0.0) {
        throw std::domain_error("zero-norm quaternion");
    }
    return scale(value, 1.0 / std::sqrt(squared));
}

Quaternion inverse(const Quaternion& value) {
    const double squared = normSquared(value);
    if (squared == 0.0) {
        throw std::domain_error("zero-norm quaternion");
    }
    return scale(conjugate(value), 1.0 / squared);
}

Vec3 passiveRotate(const Quaternion& quaternion, const Vec3& vector) {
    const Quaternion unit = normalize(quaternion);
    const Quaternion pure{0.0, vector.x, vector.y, vector.z};
    const Quaternion result = hamilton(hamilton(conjugate(unit), pure), unit);
    return {result.x, result.y, result.z};
}

Matrix3 passiveMatrix(const Quaternion& quaternion) {
    const Quaternion unit = normalize(quaternion);
    const double w = unit.w;
    const double x = unit.x;
    const double y = unit.y;
    const double z = unit.z;
    return {
        1.0 - 2.0 * (y * y + z * z),
        2.0 * (x * y + w * z),
        2.0 * (x * z - w * y),
        2.0 * (x * y - w * z),
        1.0 - 2.0 * (x * x + z * z),
        2.0 * (y * z + w * x),
        2.0 * (x * z + w * y),
        2.0 * (y * z - w * x),
        1.0 - 2.0 * (x * x + y * y),
    };
}

Quaternion activeAxisAngle(char axis, double angle) {
    const double half = 0.5 * angle;
    const double cosine = std::cos(half);
    const double sine = std::sin(half);
    if (axis == 'x') {
        return {cosine, sine, 0.0, 0.0};
    }
    if (axis == 'y') {
        return {cosine, 0.0, sine, 0.0};
    }
    if (axis == 'z') {
        return {cosine, 0.0, 0.0, sine};
    }
    throw std::invalid_argument("unsupported Euler axis");
}

Quaternion passiveQuaternionFromIntrinsicZyx(double yaw, double pitch,
                                              double roll) {
    const Quaternion active_body_to_inertial = hamilton(
        hamilton(activeAxisAngle('z', yaw), activeAxisAngle('y', pitch)),
        activeAxisAngle('x', roll));
    return inverse(active_body_to_inertial);
}

Vec3 intrinsicZyxFromPassiveQuaternion(const Quaternion& quaternion) {
    const Matrix3 matrix = passiveMatrix(quaternion);
    const double cosine_pitch = std::hypot(matrix[0], matrix[3]);
    if (cosine_pitch <= kAbsoluteTolerance) {
        throw std::domain_error("intrinsic ZYX pitch singularity");
    }
    const double pitch = std::asin(std::clamp(-matrix[6], -1.0, 1.0));
    const double yaw = std::atan2(matrix[3], matrix[0]);
    const double roll = std::atan2(matrix[7], matrix[8]);
    return {yaw, pitch, roll};
}

Vec3 matrixVector(const Matrix3& matrix, const Vec3& vector) {
    return {
        matrix[0] * vector.x + matrix[1] * vector.y + matrix[2] * vector.z,
        matrix[3] * vector.x + matrix[4] * vector.y + matrix[5] * vector.z,
        matrix[6] * vector.x + matrix[7] * vector.y + matrix[8] * vector.z,
    };
}

Matrix3 transpose(const Matrix3& matrix) {
    return {
        matrix[0], matrix[3], matrix[6],
        matrix[1], matrix[4], matrix[7],
        matrix[2], matrix[5], matrix[8],
    };
}

Matrix3 multiply(const Matrix3& lhs, const Matrix3& rhs) {
    Matrix3 result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            double value = 0.0;
            for (std::size_t inner = 0; inner < 3; ++inner) {
                value += lhs[row * 3 + inner] * rhs[inner * 3 + column];
            }
            result[row * 3 + column] = value;
        }
    }
    return result;
}

double determinant(const Matrix3& matrix) {
    return matrix[0] * (matrix[4] * matrix[8] - matrix[5] * matrix[7]) -
           matrix[1] * (matrix[3] * matrix[8] - matrix[5] * matrix[6]) +
           matrix[2] * (matrix[3] * matrix[7] - matrix[4] * matrix[6]);
}

std::vector<double> values(const Vec3& value) {
    return {value.x, value.y, value.z};
}

std::vector<double> values(const Quaternion& value) {
    return {value.w, value.x, value.y, value.z};
}

std::vector<double> values(const Matrix3& value) {
    return {value.begin(), value.end()};
}

double maxAbsDifference(const std::vector<double>& lhs,
                        const std::vector<double>& rhs) {
    if (lhs.size() != rhs.size()) {
        return std::numeric_limits<double>::max();
    }
    double maximum = 0.0;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        maximum = std::max(maximum, std::abs(lhs[index] - rhs[index]));
    }
    return maximum;
}

bool withinTolerance(const std::vector<double>& actual,
                     const std::vector<double>& expected) {
    if (actual.size() != expected.size()) {
        return false;
    }
    for (std::size_t index = 0; index < actual.size(); ++index) {
        const double bound =
            kAbsoluteTolerance +
            kRelativeTolerance *
                std::max(std::abs(actual[index]), std::abs(expected[index]));
        if (std::abs(actual[index] - expected[index]) > bound) {
            return false;
        }
    }
    return true;
}

void addObservation(std::vector<Observation>& observations, CheckBook& checks,
                    std::string id, std::string_view check_id,
                    std::vector<double> actual,
                    const std::vector<double>& expected, bool exact = false) {
    const double error = maxAbsDifference(actual, expected);
    const bool passed = exact ? actual == expected
                              : withinTolerance(actual, expected);
    checks.observe(check_id, passed, error);
    observations.push_back(Observation{std::move(id), std::move(actual)});
}

class DeterministicGenerator {
public:
    explicit DeterministicGenerator(std::uint64_t seed) : state_(seed) {}

    double uniform(double lower, double upper) {
        state_ = state_ * UINT64_C(6364136223846793005) +
                 UINT64_C(1442695040888963407);
        const auto bits = state_ >> 11U;
        const double unit = static_cast<double>(bits) /
                            9007199254740992.0;
        return lower + (upper - lower) * unit;
    }

private:
    std::uint64_t state_;
};

Quaternion randomUnitQuaternion(DeterministicGenerator& generator) {
    for (;;) {
        const Quaternion candidate{
            generator.uniform(-1.0, 1.0),
            generator.uniform(-1.0, 1.0),
            generator.uniform(-1.0, 1.0),
            generator.uniform(-1.0, 1.0),
        };
        if (normSquared(candidate) > 0.01) {
            return normalize(candidate);
        }
    }
}

std::int64_t exactGridTicks(double duration, double base_dt) {
    const double ratio = duration / base_dt;
    const double nearest = std::round(ratio);
    if (std::abs(ratio - nearest) > kAbsoluteTolerance) {
        throw std::domain_error("duration is not an integer multiple of base_dt");
    }
    return static_cast<std::int64_t>(nearest);
}

std::array<double, 2> durationAlignment(double duration, double base_dt) {
    const double ratio = duration / base_dt;
    return {std::floor(ratio) * base_dt, std::ceil(ratio) * base_dt};
}

void runFixtureObservations(std::vector<Observation>& observations,
                            CheckBook& checks) {
    constexpr double kSqrtHalf = 0.7071067811865476;
    const Quaternion z90{kSqrtHalf, 0.0, 0.0, kSqrtHalf};
    const Quaternion x90{kSqrtHalf, kSqrtHalf, 0.0, 0.0};

    addObservation(observations, checks, "quaternion.rotate-z90-x",
                   "quaternion.direction",
                   values(passiveRotate(z90, Vec3{1.0, 0.0, 0.0})),
                   {0.0, -1.0, 0.0});

    addObservation(observations, checks, "quaternion.rotate-x180-y",
                   "quaternion.direction",
                   values(passiveRotate(Quaternion{0.0, 1.0, 0.0, 0.0},
                                        Vec3{0.0, 1.0, 0.0})),
                   {0.0, -1.0, 0.0});

    const Quaternion combined = hamilton(z90, x90);
    addObservation(observations, checks,
                   "quaternion.compose-z90-then-x90",
                   "quaternion.composition",
                   values(passiveRotate(combined, Vec3{1.0, 2.0, 3.0})),
                   {2.0, 3.0, 1.0});

    addObservation(observations, checks,
                   "quaternion.hamilton-composition-coefficients",
                   "quaternion.hamilton-product", values(combined),
                   {0.5, 0.5, 0.5, 0.5});

    const Vec3 original{2.0, -3.0, 5.0};
    const Vec3 transformed = passiveRotate(z90, original);
    addObservation(observations, checks,
                   "quaternion.inverse-round-trip", "quaternion.inverse",
                   values(passiveRotate(inverse(z90), transformed)),
                   values(original));

    addObservation(observations, checks,
                   "quaternion.matrix-row-major-z90",
                   "quaternion.matrix-equivalence", values(passiveMatrix(z90)),
                   {0.0, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0});

    const Quaternion serialized{0.5, -0.5, 0.5, -0.5};
    addObservation(observations, checks, "quaternion.serialization-wxyz",
                   "quaternion.serialization-wxyz", values(serialized),
                   {0.5, -0.5, 0.5, -0.5}, true);

    const Quaternion pure_omega{0.0, 0.0, 0.0, 2.0};
    const Quaternion derivative =
        scale(hamilton(pure_omega, Quaternion{1.0, 0.0, 0.0, 0.0}),
              -0.5);
    addObservation(observations, checks,
                   "quaternion.body-rate-derivative",
                   "quaternion.body-rate-derivative", values(derivative),
                   {0.0, 0.0, 0.0, -1.0});

    const Vec3 euler = intrinsicZyxFromPassiveQuaternion(
        passiveQuaternionFromIntrinsicZyx(0.7, -0.4, 0.2));
    addObservation(observations, checks,
                   "quaternion.euler-intrinsic-zyx-round-trip",
                   "quaternion.euler-round-trip", values(euler),
                   {0.7, -0.4, 0.2});

    constexpr double kPi = 3.14159265358979323846;
    addObservation(observations, checks, "units.degree-180-to-radian",
                   "units.si-boundary", {180.0 * (kPi / 180.0)}, {kPi});
    addObservation(observations, checks, "units.kmh-72-to-mps",
                   "units.si-boundary", {72.0 * (1.0 / 3.6)}, {20.0});
    addObservation(observations, checks, "units.celsius-25-to-kelvin",
                   "units.si-boundary", {25.0 + 273.15}, {298.15});

    const Matrix3 rotation{
        0.0, 1.0, 0.0,
        -1.0, 0.0, 0.0,
        0.0, 0.0, 1.0,
    };
    const Vec3 coordinates{1.0, 2.0, 3.0};
    addObservation(observations, checks,
                   "frames.point-z90-with-translation",
                   "frames.point-versus-free-vector",
                   values(add(matrixVector(rotation, coordinates),
                              Vec3{10.0, 20.0, 30.0})),
                   {12.0, 19.0, 33.0});
    addObservation(observations, checks, "frames.free-vector-z90",
                   "frames.point-versus-free-vector",
                   values(matrixVector(rotation, coordinates)),
                   {2.0, -1.0, 3.0});

    constexpr std::int64_t kTick = 10000003;
    addObservation(observations, checks, "time.large-integer-tick",
                   "time.integer-tick",
                   {12.5 + static_cast<double>(kTick) * 0.125},
                   {1250012.875});

    const auto aligned = durationAlignment(1.01, 0.125);
    addObservation(observations, checks,
                   "time.non-grid-stop-before-after",
                   "time.duration-alignment", {aligned[0], aligned[1]},
                   {1.0, 1.125});
}

void runRandomProperties(CheckBook& checks) {
    DeterministicGenerator generator{UINT64_C(0x534349303031)};
    const Matrix3 identity{
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
    };

    for (std::size_t sample = 0; sample < kRandomRotationSamples; ++sample) {
        const Quaternion first = randomUnitQuaternion(generator);
        const Quaternion second = randomUnitQuaternion(generator);
        const Vec3 vector{
            generator.uniform(-10.0, 10.0),
            generator.uniform(-10.0, 10.0),
            generator.uniform(-10.0, 10.0),
        };

        const Matrix3 matrix = passiveMatrix(first);
        const auto matrix_result = values(matrixVector(matrix, vector));
        const auto quaternion_result = values(passiveRotate(first, vector));
        double error = maxAbsDifference(matrix_result, quaternion_result);
        checks.observe("quaternion.matrix-equivalence",
                       withinTolerance(matrix_result, quaternion_result), error);

        const auto orthogonality = values(multiply(matrix, transpose(matrix)));
        const auto expected_identity = values(identity);
        error = maxAbsDifference(orthogonality, expected_identity);
        checks.observe("quaternion.orthogonality",
                       withinTolerance(orthogonality, expected_identity), error);

        const double determinant_error = std::abs(determinant(matrix) - 1.0);
        checks.observe("quaternion.determinant",
                       determinant_error <=
                           kAbsoluteTolerance + kRelativeTolerance,
                       determinant_error);

        const auto negative_result =
            values(passiveRotate(scale(first, -1.0), vector));
        error = maxAbsDifference(negative_result, quaternion_result);
        checks.observe("quaternion.sign-equivalence",
                       withinTolerance(negative_result, quaternion_result), error);

        const auto combined_result =
            values(passiveRotate(hamilton(first, second), vector));
        const auto sequential_result =
            values(passiveRotate(second, passiveRotate(first, vector)));
        error = maxAbsDifference(combined_result, sequential_result);
        checks.observe("quaternion.composition",
                       withinTolerance(combined_result, sequential_result), error);

        const auto round_trip =
            values(passiveRotate(inverse(first), passiveRotate(first, vector)));
        const auto expected_vector = values(vector);
        error = maxAbsDifference(round_trip, expected_vector);
        checks.observe("quaternion.inverse",
                       withinTolerance(round_trip, expected_vector), error);

        const Vec3 expected_euler{
            generator.uniform(-2.8, 2.8),
            generator.uniform(-1.4, 1.4),
            generator.uniform(-2.8, 2.8),
        };
        const auto euler_round_trip = values(intrinsicZyxFromPassiveQuaternion(
            passiveQuaternionFromIntrinsicZyx(
                expected_euler.x, expected_euler.y, expected_euler.z)));
        const auto expected_euler_values = values(expected_euler);
        error = maxAbsDifference(euler_round_trip, expected_euler_values);
        checks.observe("quaternion.euler-round-trip",
                       withinTolerance(euler_round_trip,
                                       expected_euler_values),
                       error);
    }

    bool zero_rejected = false;
    try {
        static_cast<void>(inverse(Quaternion{0.0, 0.0, 0.0, 0.0}));
    } catch (const std::domain_error&) {
        zero_rejected = true;
    }
    checks.observe("quaternion.zero-norm-domain-error", zero_rejected);

    bool singularity_rejected = false;
    try {
        constexpr double kPi = 3.14159265358979323846;
        const Quaternion singular =
            passiveQuaternionFromIntrinsicZyx(0.3, 0.5 * kPi, -0.2);
        static_cast<void>(intrinsicZyxFromPassiveQuaternion(singular));
    } catch (const std::domain_error&) {
        singularity_rejected = true;
    }
    checks.observe("quaternion.euler-singularity", singularity_rejected);

    constexpr double kPi = 3.14159265358979323846;
    const auto euler_direction = values(passiveRotate(
        passiveQuaternionFromIntrinsicZyx(0.5 * kPi, 0.0, 0.0),
        Vec3{1.0, 0.0, 0.0}));
    const std::vector<double> expected_euler_direction{0.0, 1.0, 0.0};
    const double euler_direction_error =
        maxAbsDifference(euler_direction, expected_euler_direction);
    checks.observe("quaternion.euler-round-trip",
                   withinTolerance(euler_direction,
                                   expected_euler_direction),
                   euler_direction_error);

    checks.observe("time.duration-alignment",
                   exactGridTicks(2.0, 0.125) == 16);
    bool non_grid_rejected = false;
    try {
        static_cast<void>(exactGridTicks(1.01, 0.125));
    } catch (const std::domain_error&) {
        non_grid_rejected = true;
    }
    checks.observe("time.duration-alignment", non_grid_rejected);

    const std::array<std::string_view, 5> time_types = {
        "SimulationTime", "Duration", "SampleTime", "ValidTime", "WallTime"};
    bool distinct = true;
    for (std::size_t first = 0; first < time_types.size(); ++first) {
        for (std::size_t second = first + 1; second < time_types.size();
             ++second) {
            distinct = distinct && time_types[first] != time_types[second];
        }
    }
    checks.observe("time.type-separation", distinct);
}

std::string compilerDescription() {
#if defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return std::string{"Clang "} + __clang_version__;
#elif defined(__GNUC__)
    return std::string{"GCC "} + __VERSION__;
#else
    return "unknown C++ compiler";
#endif
}

void writeJsonString(std::ostream& stream, std::string_view value) {
    stream << '"';
    for (const char character : value) {
        switch (character) {
        case '"': stream << "\\\""; break;
        case '\\': stream << "\\\\"; break;
        case '\n': stream << "\\n"; break;
        case '\r': stream << "\\r"; break;
        case '\t': stream << "\\t"; break;
        default: stream << character; break;
        }
    }
    stream << '"';
}

void writeReport(std::ostream& stream, const CheckBook& checks,
                 const std::vector<Observation>& observations) {
    stream << std::setprecision(17);
    stream << "{\n  \"schema_version\": ";
    writeJsonString(stream, kOutputSchema);
    stream << ",\n  \"convention_id\": ";
    writeJsonString(stream, kConventionId);
    stream << ",\n  \"implementation\": {\n    \"id\": "
              "\"cpp17-isolated-property-spike\",\n    \"language\": "
              "\"C++17\",\n    \"runtime\": ";
    writeJsonString(stream, compilerDescription());
    stream << ",\n    \"dependency_policy\": "
              "\"standard library only; no product or Legacy link\"\n  },\n"
              "  \"status\": \""
           << (checks.allPassed() ? "pass" : "fail") << "\",\n"
              "  \"checks\": [\n";

    std::vector<Check> sorted_checks = checks.checks();
    std::sort(sorted_checks.begin(), sorted_checks.end(),
              [](const Check& lhs, const Check& rhs) { return lhs.id < rhs.id; });
    for (std::size_t index = 0; index < sorted_checks.size(); ++index) {
        const Check& check = sorted_checks[index];
        stream << "    {\"id\": ";
        writeJsonString(stream, check.id);
        stream << ", \"status\": \"" << (check.passed ? "pass" : "fail")
               << "\", \"max_error\": " << check.max_error << '}';
        stream << (index + 1 == sorted_checks.size() ? "\n" : ",\n");
    }

    stream << "  ],\n  \"observations\": [\n";
    for (std::size_t index = 0; index < observations.size(); ++index) {
        const Observation& observation = observations[index];
        stream << "    {\"id\": ";
        writeJsonString(stream, observation.id);
        stream << ", \"values\": [";
        for (std::size_t value_index = 0;
             value_index < observation.values.size(); ++value_index) {
            stream << observation.values[value_index];
            if (value_index + 1 != observation.values.size()) {
                stream << ", ";
            }
        }
        stream << "]}";
        stream << (index + 1 == observations.size() ? "\n" : ",\n");
    }
    stream << "  ]\n}\n";
}

struct Options {
    bool self_check = false;
    std::string report_path;
};

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--self-check") {
            options.self_check = true;
        } else if (argument == "--report" && index + 1 < argc) {
            ++index;
            options.report_path = argv[index];
        } else {
            throw std::invalid_argument("usage: --self-check | --report <path>");
        }
    }
    if (!options.self_check && options.report_path.empty()) {
        throw std::invalid_argument("usage: --self-check | --report <path>");
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        CheckBook checks;
        std::vector<Observation> observations;
        observations.reserve(16);
        runFixtureObservations(observations, checks);
        runRandomProperties(checks);

        if (!options.report_path.empty()) {
            std::ofstream stream{options.report_path,
                                 std::ios::out | std::ios::binary};
            if (!stream) {
                throw std::runtime_error("unable to open report path");
            }
            writeReport(stream, checks, observations);
            if (!stream) {
                throw std::runtime_error("unable to write report");
            }
        }

        std::cout << "scientific convention checks=" << checks.checks().size()
                  << " observations=" << observations.size()
                  << " random_rotations=" << kRandomRotationSamples
                  << " status=" << (checks.allPassed() ? "pass" : "fail")
                  << '\n';
        return checks.allPassed() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& exception) {
        std::cerr << "scientific convention test error: " << exception.what()
                  << '\n';
        return EXIT_FAILURE;
    }
}

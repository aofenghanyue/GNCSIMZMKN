#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kAbsTolerance = 1.0e-12;
constexpr double kRelTolerance = 1.0e-12;
constexpr double kPropertyTolerance = 2.0e-12;

bool is_close(const double left,
              const double right,
              const double absolute_tolerance = kAbsTolerance,
              const double relative_tolerance = kRelTolerance) {
    const double scale = std::max(std::abs(left), std::abs(right));
    return std::abs(left - right) <= absolute_tolerance + relative_tolerance * scale;
}

struct TestRunner {
    int checks{0};
    std::vector<std::string> failures;

    void check(const bool condition, std::string label) {
        ++checks;
        if (!condition) {
            failures.push_back(std::move(label));
        }
    }
};

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

Vec3 add(const Vec3& left, const Vec3& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 scale(const double factor, const Vec3& value) {
    return {factor * value.x, factor * value.y, factor * value.z};
}

double dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

double norm(const Vec3& value) {
    return std::sqrt(dot(value, value));
}

bool normalize(const Vec3& input, Vec3& output) {
    const double magnitude = norm(input);
    if (!std::isfinite(magnitude) || magnitude == 0.0) {
        return false;
    }
    output = scale(1.0 / magnitude, input);
    return true;
}

bool vec_close(const Vec3& left, const Vec3& right, const double tolerance = kPropertyTolerance) {
    return std::max({std::abs(left.x - right.x), std::abs(left.y - right.y), std::abs(left.z - right.z)}) <=
           tolerance;
}

struct Mat3 {
    std::array<double, 9> values{};

    double& at(const std::size_t row, const std::size_t column) {
        return values[row * 3U + column];
    }

    double at(const std::size_t row, const std::size_t column) const {
        return values[row * 3U + column];
    }
};

Mat3 identity_matrix() {
    Mat3 result;
    result.at(0U, 0U) = 1.0;
    result.at(1U, 1U) = 1.0;
    result.at(2U, 2U) = 1.0;
    return result;
}

Vec3 multiply(const Mat3& matrix, const Vec3& vector) {
    return {
        matrix.at(0U, 0U) * vector.x + matrix.at(0U, 1U) * vector.y + matrix.at(0U, 2U) * vector.z,
        matrix.at(1U, 0U) * vector.x + matrix.at(1U, 1U) * vector.y + matrix.at(1U, 2U) * vector.z,
        matrix.at(2U, 0U) * vector.x + matrix.at(2U, 1U) * vector.y + matrix.at(2U, 2U) * vector.z,
    };
}

Mat3 multiply(const Mat3& left, const Mat3& right) {
    Mat3 result;
    for (std::size_t row = 0U; row < 3U; ++row) {
        for (std::size_t column = 0U; column < 3U; ++column) {
            double value = 0.0;
            for (std::size_t inner = 0U; inner < 3U; ++inner) {
                value += left.at(row, inner) * right.at(inner, column);
            }
            result.at(row, column) = value;
        }
    }
    return result;
}

Mat3 transpose(const Mat3& matrix) {
    Mat3 result;
    for (std::size_t row = 0U; row < 3U; ++row) {
        for (std::size_t column = 0U; column < 3U; ++column) {
            result.at(row, column) = matrix.at(column, row);
        }
    }
    return result;
}

double determinant(const Mat3& matrix) {
    const double a = matrix.at(0U, 0U);
    const double b = matrix.at(0U, 1U);
    const double c = matrix.at(0U, 2U);
    const double d = matrix.at(1U, 0U);
    const double e = matrix.at(1U, 1U);
    const double f = matrix.at(1U, 2U);
    const double g = matrix.at(2U, 0U);
    const double h = matrix.at(2U, 1U);
    const double i = matrix.at(2U, 2U);
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

double identity_error(const Mat3& matrix) {
    double error = 0.0;
    for (std::size_t row = 0U; row < 3U; ++row) {
        for (std::size_t column = 0U; column < 3U; ++column) {
            const double expected = row == column ? 1.0 : 0.0;
            error = std::max(error, std::abs(matrix.at(row, column) - expected));
        }
    }
    return error;
}

double matrix_difference(const Mat3& left, const Mat3& right) {
    double error = 0.0;
    for (std::size_t index = 0U; index < left.values.size(); ++index) {
        error = std::max(error, std::abs(left.values[index] - right.values[index]));
    }
    return error;
}

double orthogonality_error(const Mat3& matrix) {
    return identity_error(multiply(transpose(matrix), matrix));
}

bool is_proper_rotation(const Mat3& matrix, const double tolerance = kPropertyTolerance) {
    for (const double value : matrix.values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return orthogonality_error(matrix) <= tolerance && std::abs(determinant(matrix) - 1.0) <= tolerance;
}

Mat3 rodrigues(const Vec3& axis, const double angle) {
    Vec3 unit_axis;
    if (!normalize(axis, unit_axis) || !std::isfinite(angle)) {
        throw std::invalid_argument("axis-angle input is invalid");
    }
    const double x = unit_axis.x;
    const double y = unit_axis.y;
    const double z = unit_axis.z;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const double one_minus = 1.0 - cosine;
    Mat3 result;
    result.at(0U, 0U) = cosine + x * x * one_minus;
    result.at(0U, 1U) = x * y * one_minus - z * sine;
    result.at(0U, 2U) = x * z * one_minus + y * sine;
    result.at(1U, 0U) = y * x * one_minus + z * sine;
    result.at(1U, 1U) = cosine + y * y * one_minus;
    result.at(1U, 2U) = y * z * one_minus - x * sine;
    result.at(2U, 0U) = z * x * one_minus - y * sine;
    result.at(2U, 1U) = z * y * one_minus + x * sine;
    result.at(2U, 2U) = cosine + z * z * one_minus;
    return result;
}

struct Quaternion {
    double w{1.0};
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

Quaternion hamilton(const Quaternion& left, const Quaternion& right) {
    return {
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
    };
}

Quaternion conjugate(const Quaternion& quaternion) {
    return {quaternion.w, -quaternion.x, -quaternion.y, -quaternion.z};
}

double squared_norm(const Quaternion& quaternion) {
    return quaternion.w * quaternion.w + quaternion.x * quaternion.x + quaternion.y * quaternion.y +
           quaternion.z * quaternion.z;
}

double quaternion_norm(const Quaternion& quaternion) {
    return std::sqrt(squared_norm(quaternion));
}

bool finite(const Quaternion& quaternion) {
    return std::isfinite(quaternion.w) && std::isfinite(quaternion.x) && std::isfinite(quaternion.y) &&
           std::isfinite(quaternion.z);
}

bool inverse(const Quaternion& quaternion, Quaternion& result) {
    const double norm_squared = squared_norm(quaternion);
    if (!finite(quaternion) || !std::isfinite(norm_squared) || norm_squared == 0.0) {
        return false;
    }
    const Quaternion conjugated = conjugate(quaternion);
    result = {
        conjugated.w / norm_squared,
        conjugated.x / norm_squared,
        conjugated.y / norm_squared,
        conjugated.z / norm_squared,
    };
    return true;
}

enum class NormalizationPolicy { Reject, NormalizeWithFlag };

struct PreparedQuaternion {
    bool ok{false};
    Quaternion value{};
    bool normalized{false};
};

PreparedQuaternion deserialize_quaternion(const std::vector<double>& coefficients,
                                          const NormalizationPolicy policy) {
    if (coefficients.size() != 4U) {
        return {};
    }
    const Quaternion value{coefficients[0U], coefficients[1U], coefficients[2U], coefficients[3U]};
    if (!finite(value)) {
        return {};
    }
    const double magnitude = quaternion_norm(value);
    if (!std::isfinite(magnitude) || magnitude == 0.0) {
        return {};
    }
    if (is_close(magnitude, 1.0)) {
        return {true, value, false};
    }
    if (policy == NormalizationPolicy::Reject) {
        return {};
    }
    return {
        true,
        {value.w / magnitude, value.x / magnitude, value.y / magnitude, value.z / magnitude},
        true,
    };
}

Quaternion passive_axis_angle(const Vec3& axis, const double angle) {
    Vec3 unit_axis;
    if (!normalize(axis, unit_axis) || !std::isfinite(angle)) {
        throw std::invalid_argument("axis-angle input is invalid");
    }
    const double half = 0.5 * angle;
    const double sine = std::sin(half);
    return {
        std::cos(half),
        -unit_axis.x * sine,
        -unit_axis.y * sine,
        -unit_axis.z * sine,
    };
}

bool passive_rotate(const Quaternion& quaternion, const Vec3& input, Vec3& output) {
    const PreparedQuaternion prepared = deserialize_quaternion(
        {quaternion.w, quaternion.x, quaternion.y, quaternion.z}, NormalizationPolicy::Reject);
    Quaternion inverted;
    if (!prepared.ok || !inverse(prepared.value, inverted)) {
        return false;
    }
    const Quaternion pure{0.0, input.x, input.y, input.z};
    const Quaternion rotated = hamilton(hamilton(inverted, pure), prepared.value);
    if (!finite(rotated) || std::abs(rotated.w) > 5.0e-12) {
        return false;
    }
    output = {rotated.x, rotated.y, rotated.z};
    return true;
}

bool matrix_from_quaternion(const Quaternion& quaternion, Mat3& result) {
    const std::array<Vec3, 3> basis{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    std::array<Vec3, 3> columns{};
    for (std::size_t column = 0U; column < columns.size(); ++column) {
        if (!passive_rotate(quaternion, basis[column], columns[column])) {
            return false;
        }
    }
    for (std::size_t row = 0U; row < 3U; ++row) {
        result.at(row, 0U) = row == 0U ? columns[0U].x : (row == 1U ? columns[0U].y : columns[0U].z);
        result.at(row, 1U) = row == 0U ? columns[1U].x : (row == 1U ? columns[1U].y : columns[1U].z);
        result.at(row, 2U) = row == 0U ? columns[2U].x : (row == 1U ? columns[2U].y : columns[2U].z);
    }
    return true;
}

struct UnitConversion {
    bool ok{false};
    double value{0.0};
    std::string_view canonical_unit{};
};

UnitConversion scaled_unit(const double value,
                           const double factor,
                           const std::string_view canonical_unit) {
    const double converted = value * factor;
    if (!std::isfinite(value) || !std::isfinite(converted)) {
        return {};
    }
    return {true, converted, canonical_unit};
}

UnitConversion to_si(const double value, const std::string_view unit_id) {
    if (unit_id == "m") {
        return scaled_unit(value, 1.0, "m");
    }
    if (unit_id == "km") {
        return scaled_unit(value, 1000.0, "m");
    }
    if (unit_id == "s") {
        return scaled_unit(value, 1.0, "s");
    }
    if (unit_id == "kg") {
        return scaled_unit(value, 1.0, "kg");
    }
    if (unit_id == "rad") {
        return scaled_unit(value, 1.0, "rad");
    }
    if (unit_id == "deg") {
        return scaled_unit(value, kPi / 180.0, "rad");
    }
    if (unit_id == "m/s") {
        return scaled_unit(value, 1.0, "m/s");
    }
    if (unit_id == "m/s^2") {
        return scaled_unit(value, 1.0, "m/s^2");
    }
    if (unit_id == "rad/s") {
        return scaled_unit(value, 1.0, "rad/s");
    }
    if (unit_id == "N") {
        return scaled_unit(value, 1.0, "N");
    }
    if (unit_id == "N*m") {
        return scaled_unit(value, 1.0, "N*m");
    }
    if (unit_id == "Pa") {
        return scaled_unit(value, 1.0, "Pa");
    }
    if (unit_id == "K") {
        const UnitConversion result = scaled_unit(value, 1.0, "K");
        return result.ok && result.value >= 0.0 ? result : UnitConversion{};
    }
    if (unit_id == "degC") {
        const UnitConversion result = scaled_unit(value + 273.15, 1.0, "K");
        return result.ok && result.value >= 0.0 ? result : UnitConversion{};
    }
    return {};
}

struct Duration {
    double seconds{0.0};
};

struct SimulationTime {
    double seconds{0.0};
    std::string clock_domain;
};

struct SampleTime {
    double seconds{0.0};
    std::string clock_domain;
};

struct ValidTime {
    double seconds{0.0};
    std::string clock_domain;
};

struct WallTime {
    std::string representation;
    std::string clock_source;
};

static_assert(!std::is_same_v<Duration, SimulationTime>);
static_assert(!std::is_same_v<SimulationTime, SampleTime>);
static_assert(!std::is_same_v<SampleTime, ValidTime>);
static_assert(!std::is_same_v<ValidTime, WallTime>);

bool valid(const Duration& duration) {
    return std::isfinite(duration.seconds);
}

bool valid(const SimulationTime& time) {
    return std::isfinite(time.seconds) && !time.clock_domain.empty();
}

bool valid(const SampleTime& time) {
    return std::isfinite(time.seconds) && !time.clock_domain.empty();
}

bool valid(const ValidTime& time) {
    return std::isfinite(time.seconds) && !time.clock_domain.empty();
}

bool add_duration(const SimulationTime& start, const Duration& duration, SimulationTime& result) {
    if (!valid(start) || !valid(duration)) {
        return false;
    }
    result = {start.seconds + duration.seconds, start.clock_domain};
    return valid(result);
}

bool subtract_time(const SimulationTime& left, const SimulationTime& right, Duration& result) {
    if (!valid(left) || !valid(right) || left.clock_domain != right.clock_domain) {
        return false;
    }
    result = {left.seconds - right.seconds};
    return valid(result);
}

struct ValidInterval {
    ValidTime from;
    ValidTime until;
};

bool valid(const ValidInterval& interval) {
    return valid(interval.from) && valid(interval.until) && interval.from.clock_domain == interval.until.clock_domain &&
           interval.from.seconds <= interval.until.seconds;
}

struct ContainsOutcome {
    bool ok{false};
    bool contains{false};
};

ContainsOutcome contains(const ValidInterval& interval, const ValidTime& instant) {
    if (!valid(interval) || !valid(instant) || interval.from.clock_domain != instant.clock_domain) {
        return {};
    }
    return {true, interval.from.seconds <= instant.seconds && instant.seconds < interval.until.seconds};
}

bool valid_euler_metadata(const std::string_view sequence,
                          const std::string_view mode,
                          const std::string_view unit_id) {
    if (sequence.size() != 3U) {
        return false;
    }
    for (const char axis : sequence) {
        if (axis != 'X' && axis != 'Y' && axis != 'Z') {
            return false;
        }
    }
    return (mode == "intrinsic" || mode == "extrinsic") && (unit_id == "rad" || unit_id == "deg");
}

int run_properties() {
    TestRunner test;
    test.check(std::numeric_limits<double>::is_iec559, "binary64 uses IEC 559 semantics");
    test.check(std::numeric_limits<double>::digits == 53, "binary64 has 53 significand bits");
    test.check(sizeof(double) == 8U, "binary64 storage is eight bytes");

    const UnitConversion kilometres = to_si(2.5, "km");
    test.check(kilometres.ok && kilometres.value == 2500.0 && kilometres.canonical_unit == "m", "km to m");
    const UnitConversion degrees = to_si(180.0, "deg");
    test.check(degrees.ok && is_close(degrees.value, kPi) && degrees.canonical_unit == "rad", "deg to rad");
    const UnitConversion celsius = to_si(-273.15, "degC");
    test.check(celsius.ok && is_close(celsius.value, 0.0) && celsius.canonical_unit == "K", "degC to K");
    const std::array<std::string_view, 11> canonical_units{
        "m", "s", "kg", "rad", "m/s", "m/s^2", "rad/s", "N", "N*m", "Pa", "K",
    };
    for (const std::string_view canonical_unit : canonical_units) {
        const UnitConversion identity = to_si(1.0, canonical_unit);
        test.check(identity.ok && identity.value == 1.0 && identity.canonical_unit == canonical_unit,
                   "canonical unit id " + std::string(canonical_unit));
    }
    test.check(!to_si(-273.1501, "degC").ok, "temperature below zero Kelvin fails");
    test.check(!to_si(-0.001, "K").ok, "negative Kelvin fails");
    test.check(!to_si(1.0, "unknown").ok, "unknown unit fails");
    test.check(!to_si(std::numeric_limits<double>::infinity(), "m").ok, "non-finite unit input fails");

    const SimulationTime start{5.0, "sim/main"};
    SimulationTime end;
    test.check(add_duration(start, Duration{0.25}, end) && is_close(end.seconds, 5.25) &&
                   end.clock_domain == start.clock_domain,
               "simulation time plus duration");
    Duration elapsed;
    test.check(subtract_time(end, start, elapsed) && is_close(elapsed.seconds, 0.25), "same-clock time difference");
    test.check(!subtract_time(end, SimulationTime{5.0, "sim/other"}, elapsed), "mixed simulation clocks fail");
    const ValidInterval interval{{1.0, "sim/main"}, {2.0, "sim/main"}};
    const ContainsOutcome at_start = contains(interval, ValidTime{1.0, "sim/main"});
    const ContainsOutcome at_end = contains(interval, ValidTime{2.0, "sim/main"});
    test.check(at_start.ok && at_start.contains, "half-open validity includes start");
    test.check(at_end.ok && !at_end.contains, "half-open validity excludes end");
    test.check(!valid(ValidInterval{{2.0, "sim/main"}, {1.0, "sim/main"}}), "reversed validity interval fails");
    test.check(!contains(interval, ValidTime{1.5, "sim/other"}).ok, "mixed validity clocks fail");
    test.check(!valid(SimulationTime{std::numeric_limits<double>::quiet_NaN(), "sim/main"}), "non-finite time fails");
    test.check(valid(SampleTime{1.0, "sensor/main"}), "sample time carries clock domain");
    test.check(valid(ValidTime{1.0, "sim/main"}), "valid time carries clock domain");

    const Quaternion z_quarter = passive_axis_angle({0.0, 0.0, 1.0}, kPi / 2.0);
    const double coefficient = std::sqrt(0.5);
    test.check(is_close(z_quarter.w, coefficient) && is_close(z_quarter.x, 0.0) && is_close(z_quarter.y, 0.0) &&
                   is_close(z_quarter.z, -coefficient),
               "passive quaternion serializes wxyz");
    Vec3 rotated;
    test.check(passive_rotate(z_quarter, {1.0, 0.0, 0.0}, rotated) && vec_close(rotated, {0.0, 1.0, 0.0}),
               "passive z rotation maps x to y");
    const Quaternion negated{-z_quarter.w, -z_quarter.x, -z_quarter.y, -z_quarter.z};
    Vec3 positive_result;
    Vec3 negative_result;
    const Vec3 equivalence_probe{0.25, -0.5, 1.0};
    test.check(passive_rotate(z_quarter, equivalence_probe, positive_result) &&
                   passive_rotate(negated, equivalence_probe, negative_result) &&
                   vec_close(positive_result, negative_result),
               "q and negative q are equivalent");

    const Quaternion q_b_a = z_quarter;
    const Quaternion q_c_b = passive_axis_angle({1.0, 0.0, 0.0}, kPi / 3.0);
    const Quaternion q_c_a = hamilton(q_b_a, q_c_b);
    const Vec3 composition_probe{0.25, -0.75, 2.0};
    Vec3 b_coordinates;
    Vec3 sequential_coordinates;
    Vec3 composed_coordinates;
    test.check(passive_rotate(q_b_a, composition_probe, b_coordinates) &&
                   passive_rotate(q_c_b, b_coordinates, sequential_coordinates) &&
                   passive_rotate(q_c_a, composition_probe, composed_coordinates) &&
                   vec_close(sequential_coordinates, composed_coordinates),
               "Hamilton passive composition uses q_b_a times q_c_b");
    Mat3 r_b_a;
    Mat3 r_c_b;
    Mat3 r_c_a;
    test.check(matrix_from_quaternion(q_b_a, r_b_a) && matrix_from_quaternion(q_c_b, r_c_b) &&
                   matrix_from_quaternion(q_c_a, r_c_a) &&
                   matrix_difference(r_c_a, multiply(r_c_b, r_b_a)) <= kPropertyTolerance,
               "matrix composition follows R_c_b times R_b_a");

    test.check(!deserialize_quaternion({1.0, 0.0, 0.0}, NormalizationPolicy::Reject).ok,
               "short quaternion storage fails");
    test.check(!deserialize_quaternion({0.0, 0.0, 0.0, 0.0}, NormalizationPolicy::Reject).ok,
               "zero quaternion fails");
    test.check(!deserialize_quaternion({2.0, 0.0, 0.0, 0.0}, NormalizationPolicy::Reject).ok,
               "non-unit quaternion reject policy fails");
    const PreparedQuaternion normalized =
        deserialize_quaternion({2.0, 0.0, 0.0, 0.0}, NormalizationPolicy::NormalizeWithFlag);
    test.check(normalized.ok && normalized.normalized && is_close(normalized.value.w, 1.0),
               "normalization policy records flag");
    test.check(!deserialize_quaternion({std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 1.0},
                                       NormalizationPolicy::Reject)
                    .ok,
               "non-finite quaternion fails");
    test.check(valid_euler_metadata("ZYX", "intrinsic", "rad"), "qualified Euler metadata passes");
    test.check(!valid_euler_metadata("", "intrinsic", "rad"), "missing Euler sequence fails");
    test.check(!valid_euler_metadata("ZYX", "", "rad"), "missing Euler mode fails");

    Mat3 reflection = identity_matrix();
    reflection.at(0U, 0U) = -1.0;
    test.check(!is_proper_rotation(reflection), "reflection is not a proper attitude rotation");
    Mat3 distorted = identity_matrix();
    distorted.at(0U, 1U) = 0.1;
    test.check(!is_proper_rotation(distorted), "non-orthogonal matrix fails");
    const Mat3 z_matrix = rodrigues({0.0, 0.0, 1.0}, kPi / 2.0);
    const Vec3 free_vector = multiply(z_matrix, Vec3{1.0, 0.0, 0.0});
    const Vec3 point = add(free_vector, {10.0, -2.0, 0.5});
    test.check(!vec_close(free_vector, point), "point translation remains separate from free-vector rotation");

    std::mt19937_64 generator(20260809ULL);
    std::uniform_real_distribution<double> component_distribution(-1.0, 1.0);
    std::uniform_real_distribution<double> angle_distribution(-kPi, kPi);
    std::uniform_real_distribution<double> vector_distribution(-5.0, 5.0);
    for (std::size_t index = 0U; index < 128U; ++index) {
        Vec3 axis{component_distribution(generator), component_distribution(generator), component_distribution(generator)};
        if (norm(axis) < 1.0e-9) {
            axis = {1.0, 0.0, 0.0};
        }
        const double angle = angle_distribution(generator);
        const Quaternion quaternion = passive_axis_angle(axis, angle);
        Mat3 quaternion_matrix;
        const bool matrix_ok = matrix_from_quaternion(quaternion, quaternion_matrix);
        const Mat3 analytic_matrix = rodrigues(axis, angle);
        test.check(matrix_ok && matrix_difference(quaternion_matrix, analytic_matrix) <= kPropertyTolerance,
                   "random quaternion/matrix agreement " + std::to_string(index));
        test.check(matrix_ok && orthogonality_error(quaternion_matrix) <= kPropertyTolerance,
                   "random rotation orthogonality " + std::to_string(index));
        test.check(matrix_ok && std::abs(determinant(quaternion_matrix) - 1.0) <= kPropertyTolerance,
                   "random rotation determinant " + std::to_string(index));
        const Vec3 vector{vector_distribution(generator), vector_distribution(generator), vector_distribution(generator)};
        Vec3 transformed;
        Vec3 roundtrip;
        test.check(passive_rotate(quaternion, vector, transformed) &&
                       passive_rotate(conjugate(quaternion), transformed, roundtrip) &&
                       vec_close(roundtrip, vector, 5.0e-12),
                   "random inverse round trip " + std::to_string(index));
    }

    if (!test.failures.empty()) {
        std::cerr << "scientific convention properties failed with " << test.failures.size() << " issue(s):\n";
        for (const std::string& failure : test.failures) {
            std::cerr << " - " << failure << '\n';
        }
        return EXIT_FAILURE;
    }
    std::cout << "properties_passed=" << test.checks << '\n';
    return EXIT_SUCCESS;
}

struct CrossToolCase {
    std::string case_id;
    Vec3 axis;
    double passive_angle{0.0};
    Vec3 vector;
    Vec3 translation;
};

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        if (!field.empty() && field.back() == '\r') {
            field.pop_back();
        }
        fields.push_back(field);
    }
    return fields;
}

bool parse_finite_double(const std::string& text, double& value) {
    try {
        std::size_t consumed = 0U;
        value = std::stod(text, &consumed);
        return consumed == text.size() && std::isfinite(value);
    } catch (const std::exception&) {
        return false;
    }
}

bool load_cases(const std::string& path, std::vector<CrossToolCase>& cases, std::string& error) {
    std::ifstream stream(path);
    if (!stream) {
        error = "cannot open case file: " + path;
        return false;
    }
    std::string line;
    if (!std::getline(stream, line)) {
        error = "case file is empty";
        return false;
    }
    const std::vector<std::string> expected_header{
        "case_id",       "axis_x",       "axis_y",       "axis_z",       "passive_angle_rad", "vector_x",
        "vector_y",      "vector_z",     "translation_x", "translation_y", "translation_z",
    };
    if (split_csv_line(line) != expected_header) {
        error = "case file header does not match version 1";
        return false;
    }
    std::vector<std::string> seen_ids;
    std::size_t line_number = 1U;
    while (std::getline(stream, line)) {
        ++line_number;
        if (line.empty() || line == "\r") {
            continue;
        }
        const std::vector<std::string> fields = split_csv_line(line);
        if (fields.size() != expected_header.size() || fields[0U].empty()) {
            error = "invalid case row at line " + std::to_string(line_number);
            return false;
        }
        if (std::find(seen_ids.begin(), seen_ids.end(), fields[0U]) != seen_ids.end()) {
            error = "duplicate case id: " + fields[0U];
            return false;
        }
        std::array<double, 10> values{};
        for (std::size_t index = 0U; index < values.size(); ++index) {
            if (!parse_finite_double(fields[index + 1U], values[index])) {
                error = "invalid numeric value at line " + std::to_string(line_number);
                return false;
            }
        }
        const Vec3 axis{values[0U], values[1U], values[2U]};
        Vec3 unit_axis;
        if (!normalize(axis, unit_axis)) {
            error = "zero or non-finite axis at line " + std::to_string(line_number);
            return false;
        }
        seen_ids.push_back(fields[0U]);
        cases.push_back({
            fields[0U],
            axis,
            values[3U],
            {values[4U], values[5U], values[6U]},
            {values[7U], values[8U], values[9U]},
        });
    }
    if (cases.empty()) {
        error = "case file contains no cases";
        return false;
    }
    return true;
}

int emit_cross_tool(const std::string& path) {
    std::vector<CrossToolCase> cases;
    std::string error;
    if (!load_cases(path, cases, error)) {
        std::cerr << error << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "case_id,q_w,q_x,q_y,q_z,vector_to_x,vector_to_y,vector_to_z,point_to_x,point_to_y,point_to_z,"
                 "roundtrip_x,roundtrip_y,roundtrip_z,q_norm,det_r,orthogonality_error,matrix_agreement_error\n";
    std::cout << std::setprecision(17);
    for (const CrossToolCase& item : cases) {
        const Quaternion quaternion = passive_axis_angle(item.axis, item.passive_angle);
        Mat3 rotation;
        Vec3 vector_to;
        Vec3 roundtrip;
        if (!matrix_from_quaternion(quaternion, rotation) || !passive_rotate(quaternion, item.vector, vector_to) ||
            !passive_rotate(conjugate(quaternion), vector_to, roundtrip)) {
            std::cerr << "failed to evaluate case " << item.case_id << '\n';
            return EXIT_FAILURE;
        }
        const Vec3 point_to = add(vector_to, item.translation);
        const Mat3 analytic = rodrigues(item.axis, item.passive_angle);
        std::cout << item.case_id << ',' << quaternion.w << ',' << quaternion.x << ',' << quaternion.y << ','
                  << quaternion.z << ',' << vector_to.x << ',' << vector_to.y << ',' << vector_to.z << ','
                  << point_to.x << ',' << point_to.y << ',' << point_to.z << ',' << roundtrip.x << ','
                  << roundtrip.y << ',' << roundtrip.z << ',' << quaternion_norm(quaternion) << ','
                  << determinant(rotation) << ',' << orthogonality_error(rotation) << ','
                  << matrix_difference(rotation, analytic) << '\n';
    }
    return EXIT_SUCCESS;
}

std::string compiler_identity() {
#if defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
    return std::string("GCC ") + __VERSION__;
#else
    return "unknown";
#endif
}

std::string json_escape(const std::string& value) {
    std::string escaped;
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

int emit_metadata() {
#if defined(NDEBUG)
    constexpr bool ndebug_defined = true;
#else
    constexpr bool ndebug_defined = false;
#endif
#if defined(__FAST_MATH__)
    constexpr bool fast_math_defined = true;
#else
    constexpr bool fast_math_defined = false;
#endif
    std::cout << "{\"binary64_iec559\":" << (std::numeric_limits<double>::is_iec559 ? "true" : "false")
              << ",\"compiler\":\"" << json_escape(compiler_identity())
              << "\",\"fast_math_defined\":" << (fast_math_defined ? "true" : "false")
              << ",\"implementation_id\":\"gnczmkn.scientific-conventions.cpp-test/1\","
                 "\"language_standard\":\"C++17\",\"ndebug_defined\":"
              << (ndebug_defined ? "true" : "false") << "}\n";
    return EXIT_SUCCESS;
}

}  // namespace

int main(const int argc, char** argv) {
    try {
        if (argc == 1) {
            return run_properties();
        }
        const std::string action = argv[1];
        if (action == "--metadata" && argc == 2) {
            return emit_metadata();
        }
        if (action == "--emit-cross-tool" && argc == 3) {
            return emit_cross_tool(argv[2]);
        }
        std::cerr << "usage: scientific_conventions_properties [--metadata | --emit-cross-tool CASES]\n";
        return EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "scientific convention property tool failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

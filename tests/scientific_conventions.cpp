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
#include <type_traits>
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

const std::array<std::string_view, 23> kRequiredCheckIds = {
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
    "quaternion.coefficient-validation",
    "quaternion.normalization-policy",
    "quaternion.body-rate-derivative",
    "quaternion.euler-round-trip",
    "quaternion.euler-singularity",
    "units.si-boundary",
    "units.domain-validation",
    "frames.point-versus-free-vector",
    "time.integer-tick",
    "time.duration-alignment",
    "time.type-separation",
    "time.clock-domain",
    "time.validity-interval",
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
        const bool finite_error = std::isfinite(error) && error >= 0.0;
        iterator->passed = iterator->passed && passed && finite_error;
        iterator->max_error = finite_error
                                  ? std::max(iterator->max_error, error)
                                  : std::numeric_limits<double>::max();
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

bool finite(const Quaternion& value) {
    return std::isfinite(value.w) && std::isfinite(value.x) &&
           std::isfinite(value.y) && std::isfinite(value.z);
}

bool finite(const Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

enum class NormalizationPolicy {
    Error,
    NormalizeWithFlag,
};

struct PreparedQuaternion {
    bool ok = false;
    Quaternion value{1.0, 0.0, 0.0, 0.0};
    bool normalized = false;
};

PreparedQuaternion prepareQuaternion(const Quaternion& value,
                                     NormalizationPolicy policy) {
    const double squared = normSquared(value);
    if (!finite(value) || !std::isfinite(squared) || squared == 0.0) {
        return {};
    }

    const double magnitude = std::sqrt(squared);
    if (!std::isfinite(magnitude)) {
        return {};
    }
    const double unit_error = std::abs(magnitude - 1.0);
    const double unit_bound =
        kAbsoluteTolerance +
        kRelativeTolerance * std::max(std::abs(magnitude), 1.0);
    if (unit_error <= unit_bound) {
        return {true, value, false};
    }
    if (policy == NormalizationPolicy::Error) {
        return {};
    }

    const Quaternion normalized = scale(value, 1.0 / magnitude);
    if (!finite(normalized)) {
        return {};
    }
    return {true, normalized, true};
}

PreparedQuaternion deserializeQuaternion(
    const std::vector<double>& coefficients, NormalizationPolicy policy) {
    if (coefficients.size() != 4U) {
        return {};
    }
    return prepareQuaternion(
        Quaternion{coefficients[0], coefficients[1], coefficients[2],
                   coefficients[3]},
        policy);
}

Quaternion requireUnitQuaternion(const Quaternion& value) {
    const PreparedQuaternion prepared =
        prepareQuaternion(value, NormalizationPolicy::Error);
    if (!prepared.ok) {
        throw std::domain_error("quaternion is not a finite unit quaternion");
    }
    return prepared.value;
}

Quaternion inverse(const Quaternion& value) {
    const double squared = normSquared(value);
    if (!finite(value) || !std::isfinite(squared) || squared == 0.0) {
        throw std::domain_error("invalid quaternion inverse input");
    }
    const Quaternion result = scale(conjugate(value), 1.0 / squared);
    if (!finite(result)) {
        throw std::domain_error("non-finite quaternion inverse output");
    }
    return result;
}

Vec3 passiveRotate(const Quaternion& quaternion, const Vec3& vector) {
    if (!finite(vector)) {
        throw std::domain_error("non-finite vector input");
    }
    const Quaternion unit = requireUnitQuaternion(quaternion);
    const Quaternion pure{0.0, vector.x, vector.y, vector.z};
    const Quaternion result = hamilton(hamilton(conjugate(unit), pure), unit);
    if (!finite(result)) {
        throw std::domain_error("non-finite quaternion rotation output");
    }
    return {result.x, result.y, result.z};
}

Matrix3 passiveMatrix(const Quaternion& quaternion) {
    const Quaternion unit = requireUnitQuaternion(quaternion);
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
        if (!std::isfinite(lhs[index]) || !std::isfinite(rhs[index])) {
            return std::numeric_limits<double>::max();
        }
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
        if (!std::isfinite(actual[index]) ||
            !std::isfinite(expected[index])) {
            return false;
        }
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
            const PreparedQuaternion prepared = prepareQuaternion(
                candidate, NormalizationPolicy::NormalizeWithFlag);
            if (prepared.ok) {
                return prepared.value;
            }
        }
    }
}

struct UnitConversionResult {
    bool ok = false;
    double value = 0.0;
};

UnitConversionResult convertToSi(double input, std::string_view unit) {
    if (!std::isfinite(input)) {
        return {};
    }

    constexpr double kPi = 3.14159265358979323846;
    double converted = input;
    bool temperature = false;
    if (unit == "deg") {
        converted = input * (kPi / 180.0);
    } else if (unit == "km/h") {
        converted = input / 3.6;
    } else if (unit == "degC") {
        converted = input + 273.15;
        temperature = true;
    } else if (unit == "km") {
        converted = input * 1000.0;
    } else if (unit == "K") {
        temperature = true;
    } else if (unit != "m" && unit != "s" && unit != "kg" &&
               unit != "rad" && unit != "m/s" && unit != "m/s^2" &&
               unit != "rad/s" && unit != "N" && unit != "N*m" &&
               unit != "Pa") {
        return {};
    }

    if (!std::isfinite(converted) || (temperature && converted < 0.0)) {
        return {};
    }
    return {true, converted};
}

double requireSiConversion(double input, std::string_view unit) {
    const UnitConversionResult result = convertToSi(input, unit);
    if (!result.ok) {
        throw std::domain_error("invalid unit conversion input");
    }
    return result.value;
}

struct Duration {
    double seconds = 0.0;
};

struct SimulationTime {
    double seconds = 0.0;
    std::string clock_domain;
};

struct SampleTime {
    double seconds = 0.0;
    std::string clock_domain;
};

struct ValidTime {
    double seconds = 0.0;
    std::string clock_domain;
};

struct WallTime {
    double seconds = 0.0;
};

static_assert(!std::is_same_v<SimulationTime, Duration>);
static_assert(!std::is_same_v<SimulationTime, SampleTime>);
static_assert(!std::is_same_v<SimulationTime, ValidTime>);
static_assert(!std::is_same_v<SimulationTime, WallTime>);
static_assert(!std::is_same_v<Duration, SampleTime>);
static_assert(!std::is_same_v<Duration, ValidTime>);
static_assert(!std::is_same_v<Duration, WallTime>);
static_assert(!std::is_same_v<SampleTime, ValidTime>);
static_assert(!std::is_same_v<SampleTime, WallTime>);
static_assert(!std::is_same_v<ValidTime, WallTime>);

bool validDuration(const Duration& duration) {
    return std::isfinite(duration.seconds);
}

bool validWallTime(const WallTime& wall_time) {
    return std::isfinite(wall_time.seconds);
}

template <typename TimePoint>
bool validClockPoint(const TimePoint& point) {
    return std::isfinite(point.seconds) && !point.clock_domain.empty();
}

template <typename TimePoint>
struct TimePointResult {
    bool ok = false;
    TimePoint value{};
};

template <typename TimePoint>
TimePointResult<TimePoint> addDuration(const TimePoint& point,
                                       const Duration& duration) {
    if (!validClockPoint(point) || !validDuration(duration)) {
        return {};
    }
    const double seconds = point.seconds + duration.seconds;
    if (!std::isfinite(seconds)) {
        return {};
    }
    return {true, TimePoint{seconds, point.clock_domain}};
}

struct DurationResult {
    bool ok = false;
    Duration value{};
};

template <typename TimePoint>
DurationResult subtractSameClock(const TimePoint& lhs,
                                 const TimePoint& rhs) {
    if (!validClockPoint(lhs) || !validClockPoint(rhs) ||
        lhs.clock_domain != rhs.clock_domain) {
        return {};
    }
    const double seconds = lhs.seconds - rhs.seconds;
    if (!std::isfinite(seconds)) {
        return {};
    }
    return {true, Duration{seconds}};
}

struct ValidInterval {
    ValidTime valid_from;
    ValidTime valid_until;
};

bool valid(const ValidInterval& interval) {
    return validClockPoint(interval.valid_from) &&
           validClockPoint(interval.valid_until) &&
           interval.valid_from.clock_domain ==
               interval.valid_until.clock_domain &&
           interval.valid_from.seconds <= interval.valid_until.seconds;
}

struct BooleanResult {
    bool ok = false;
    bool value = false;
};

BooleanResult contains(const ValidInterval& interval, const ValidTime& point) {
    if (!valid(interval) || !validClockPoint(point) ||
        interval.valid_from.clock_domain != point.clock_domain) {
        return {};
    }
    return {true, interval.valid_from.seconds <= point.seconds &&
                      point.seconds < interval.valid_until.seconds};
}

std::int64_t exactGridTicks(double duration, double base_dt) {
    if (!std::isfinite(duration) || !std::isfinite(base_dt) ||
        base_dt <= 0.0) {
        throw std::domain_error("invalid duration alignment input");
    }
    const double ratio = duration / base_dt;
    const double nearest = std::round(ratio);
    if (!std::isfinite(ratio) || !std::isfinite(nearest) ||
        std::abs(ratio - nearest) > kAbsoluteTolerance ||
        nearest < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
        nearest >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        throw std::domain_error("duration is not an integer multiple of base_dt");
    }
    return static_cast<std::int64_t>(nearest);
}

std::array<double, 2> durationAlignment(double duration, double base_dt) {
    if (!std::isfinite(duration) || !std::isfinite(base_dt) ||
        base_dt <= 0.0) {
        throw std::domain_error("invalid duration alignment input");
    }
    const double ratio = duration / base_dt;
    const std::array<double, 2> result{
        std::floor(ratio) * base_dt, std::ceil(ratio) * base_dt};
    if (!std::isfinite(ratio) || !std::isfinite(result[0]) ||
        !std::isfinite(result[1])) {
        throw std::domain_error("non-finite duration alignment output");
    }
    return result;
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
                   "units.si-boundary", {requireSiConversion(180.0, "deg")},
                   {kPi});
    addObservation(observations, checks, "units.kmh-72-to-mps",
                   "units.si-boundary", {requireSiConversion(72.0, "km/h")},
                   {20.0});
    addObservation(observations, checks, "units.celsius-25-to-kelvin",
                   "units.si-boundary", {requireSiConversion(25.0, "degC")},
                   {298.15});

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

    const PreparedQuaternion valid_coefficients = deserializeQuaternion(
        {1.0, 0.0, 0.0, 0.0}, NormalizationPolicy::Error);
    const PreparedQuaternion short_coefficients = deserializeQuaternion(
        {1.0, 0.0, 0.0}, NormalizationPolicy::Error);
    const PreparedQuaternion long_coefficients = deserializeQuaternion(
        {1.0, 0.0, 0.0, 0.0, 0.0}, NormalizationPolicy::Error);
    const PreparedQuaternion nan_coefficients = deserializeQuaternion(
        {1.0, std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0},
        NormalizationPolicy::Error);
    const PreparedQuaternion infinite_coefficients = deserializeQuaternion(
        {1.0, 0.0, std::numeric_limits<double>::infinity(), 0.0},
        NormalizationPolicy::Error);
    checks.observe("quaternion.coefficient-validation",
                   valid_coefficients.ok && !short_coefficients.ok &&
                       !long_coefficients.ok && !nan_coefficients.ok &&
                       !infinite_coefficients.ok);

    const PreparedQuaternion rejected_non_unit =
        prepareQuaternion(Quaternion{2.0, 0.0, 0.0, 0.0},
                          NormalizationPolicy::Error);
    const PreparedQuaternion corrected_non_unit =
        prepareQuaternion(Quaternion{2.0, 0.0, 0.0, 0.0},
                          NormalizationPolicy::NormalizeWithFlag);
    const PreparedQuaternion accepted_unit =
        prepareQuaternion(Quaternion{1.0, 0.0, 0.0, 0.0},
                          NormalizationPolicy::Error);
    bool implicit_normalization_rejected = false;
    try {
        static_cast<void>(passiveRotate(Quaternion{2.0, 0.0, 0.0, 0.0},
                                        Vec3{1.0, 0.0, 0.0}));
    } catch (const std::domain_error&) {
        implicit_normalization_rejected = true;
    }
    const std::vector<double> identity_quaternion{1.0, 0.0, 0.0, 0.0};
    const double normalization_error = maxAbsDifference(
        values(corrected_non_unit.value), identity_quaternion);
    checks.observe(
        "quaternion.normalization-policy",
        !rejected_non_unit.ok && corrected_non_unit.ok &&
            corrected_non_unit.normalized && accepted_unit.ok &&
            !accepted_unit.normalized && implicit_normalization_rejected &&
            withinTolerance(values(corrected_non_unit.value),
                            identity_quaternion),
        normalization_error);

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

    const UnitConversionResult absolute_zero =
        convertToSi(-273.15, "degC");
    const UnitConversionResult zero_kelvin = convertToSi(0.0, "K");
    const UnitConversionResult below_celsius =
        convertToSi(-273.1500000001, "degC");
    const UnitConversionResult below_kelvin = convertToSi(-1.0e-12, "K");
    const UnitConversionResult unknown_unit = convertToSi(1.0, "furlong");
    const UnitConversionResult non_finite_unit = convertToSi(
        std::numeric_limits<double>::quiet_NaN(), "m");
    const UnitConversionResult overflowing_unit = convertToSi(
        std::numeric_limits<double>::max(), "km");
    const double absolute_zero_error = absolute_zero.ok
                                           ? std::abs(absolute_zero.value)
                                           : std::numeric_limits<double>::max();
    checks.observe(
        "units.domain-validation",
        absolute_zero.ok && zero_kelvin.ok &&
            absolute_zero_error <= kAbsoluteTolerance &&
            zero_kelvin.value == 0.0 && !below_celsius.ok &&
            !below_kelvin.ok && !unknown_unit.ok && !non_finite_unit.ok &&
            !overflowing_unit.ok,
        absolute_zero_error);

    checks.observe("time.duration-alignment",
                   exactGridTicks(2.0, 0.125) == 16);
    bool non_grid_rejected = false;
    try {
        static_cast<void>(exactGridTicks(1.01, 0.125));
    } catch (const std::domain_error&) {
        non_grid_rejected = true;
    }
    checks.observe("time.duration-alignment", non_grid_rejected);

    constexpr bool distinct =
        !std::is_same_v<SimulationTime, Duration> &&
        !std::is_same_v<SimulationTime, SampleTime> &&
        !std::is_same_v<SimulationTime, ValidTime> &&
        !std::is_same_v<SimulationTime, WallTime> &&
        !std::is_same_v<Duration, SampleTime> &&
        !std::is_same_v<Duration, ValidTime> &&
        !std::is_same_v<Duration, WallTime> &&
        !std::is_same_v<SampleTime, ValidTime> &&
        !std::is_same_v<SampleTime, WallTime> &&
        !std::is_same_v<ValidTime, WallTime>;
    checks.observe("time.type-separation", distinct);

    const SimulationTime simulation_start{10.0, "simulation.primary"};
    const TimePointResult<SimulationTime> simulation_advanced =
        addDuration(simulation_start, Duration{2.5});
    const DurationResult simulation_elapsed = subtractSameClock(
        simulation_advanced.value, simulation_start);
    const DurationResult mixed_clock = subtractSameClock(
        SimulationTime{11.0, "simulation.primary"},
        SimulationTime{10.0, "simulation.secondary"});
    const TimePointResult<SampleTime> missing_clock =
        addDuration(SampleTime{1.0, ""}, Duration{1.0});
    const TimePointResult<ValidTime> non_finite_clock = addDuration(
        ValidTime{std::numeric_limits<double>::infinity(),
                  "simulation.primary"},
        Duration{1.0});
    const TimePointResult<SimulationTime> non_finite_duration = addDuration(
        simulation_start,
        Duration{std::numeric_limits<double>::quiet_NaN()});
    const bool wall_time_finite =
        validWallTime(WallTime{0.0}) &&
        !validWallTime(WallTime{std::numeric_limits<double>::infinity()});
    const double clock_error =
        simulation_elapsed.ok
            ? std::abs(simulation_elapsed.value.seconds - 2.5)
            : std::numeric_limits<double>::max();
    checks.observe(
        "time.clock-domain",
        simulation_advanced.ok && simulation_elapsed.ok &&
            clock_error <= kAbsoluteTolerance && !mixed_clock.ok &&
            !missing_clock.ok && !non_finite_clock.ok &&
            !non_finite_duration.ok && wall_time_finite,
        clock_error);

    const ValidInterval interval{
        ValidTime{5.0, "simulation.primary"},
        ValidTime{7.0, "simulation.primary"}};
    const BooleanResult includes_start = contains(
        interval, ValidTime{5.0, "simulation.primary"});
    const BooleanResult excludes_end = contains(
        interval, ValidTime{7.0, "simulation.primary"});
    const BooleanResult reversed_interval = contains(
        ValidInterval{ValidTime{7.0, "simulation.primary"},
                      ValidTime{5.0, "simulation.primary"}},
        ValidTime{6.0, "simulation.primary"});
    const BooleanResult mixed_interval = contains(
        ValidInterval{ValidTime{5.0, "simulation.primary"},
                      ValidTime{7.0, "simulation.secondary"}},
        ValidTime{6.0, "simulation.primary"});
    const BooleanResult non_finite_interval = contains(
        ValidInterval{ValidTime{5.0, "simulation.primary"},
                      ValidTime{std::numeric_limits<double>::quiet_NaN(),
                                "simulation.primary"}},
        ValidTime{6.0, "simulation.primary"});
    checks.observe(
        "time.validity-interval",
        includes_start.ok && includes_start.value && excludes_end.ok &&
            !excludes_end.value && !reversed_interval.ok &&
            !mixed_interval.ok && !non_finite_interval.ok);
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
        const bool check_passed =
            check.passed && check.assertion_count > 0 &&
            std::isfinite(check.max_error);
        stream << "    {\"id\": ";
        writeJsonString(stream, check.id);
        stream << ", \"status\": \"" << (check_passed ? "pass" : "fail")
               << "\", \"max_error\": " << check.max_error
               << ", \"assertion_count\": " << check.assertion_count << '}';
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

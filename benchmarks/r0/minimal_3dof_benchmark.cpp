#include <array>
#include <chrono>
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

namespace {

constexpr std::string_view kSchemaVersion =
    "gnczmkn.r0-minimal-3dof-benchmark-result/1";
constexpr std::string_view kWorkloadId = "PERF-R0-M3DOF-BATCH-001";
constexpr std::string_view kModelId =
    "MODEL-MINIMAL-3DOF-LINEAR-TRANSLATION-001";
constexpr std::uint64_t kStepsPerEpisode = 80U;
constexpr std::uint64_t kDerivativeEvaluationsPerStep = 4U;
constexpr std::uint64_t kMaximumHarnessEpisodes = 1'000'000U;
constexpr double kStepSeconds = 0.05;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct State {
    Vec3 position;
    Vec3 velocity;
};

struct Derivative {
    Vec3 position_rate;
    Vec3 velocity_rate;
};

struct WorkloadResult {
    std::uint64_t episodes = 0U;
    std::uint64_t completed_steps = 0U;
    std::uint64_t derivative_evaluations = 0U;
    std::uint64_t weight_sum = 0U;
    std::int64_t elapsed_ns = 0;
    Vec3 mean_final_position;
    Vec3 mean_final_velocity;
    double weighted_observable = 0.0;
    bool skipped_integrator = false;
};

struct Options {
    bool self_check = false;
    std::uint64_t episodes = 0U;
    std::string report_path;
    bool skip_integrator = false;
};

Vec3 add(const Vec3& left, const Vec3& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 scaled(const Vec3& value, double factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

bool finite(const Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool finite(const State& state) {
    return finite(state.position) && finite(state.velocity);
}

Derivative evaluate(const State& state) {
    constexpr Vec3 acceleration{1.0, 0.0, -9.80665};
    constexpr double drag_rate = 0.2;
    return {
        state.velocity,
        {
            acceleration.x - drag_rate * state.velocity.x,
            acceleration.y - drag_rate * state.velocity.y,
            acceleration.z - drag_rate * state.velocity.z,
        },
    };
}

State offset(const State& state, const Derivative& derivative, double factor) {
    return {
        add(state.position, scaled(derivative.position_rate, factor)),
        add(state.velocity, scaled(derivative.velocity_rate, factor)),
    };
}

State rk4Step(const State& state) {
    const Derivative k1 = evaluate(state);
    const Derivative k2 = evaluate(offset(state, k1, 0.5 * kStepSeconds));
    const Derivative k3 = evaluate(offset(state, k2, 0.5 * kStepSeconds));
    const Derivative k4 = evaluate(offset(state, k3, kStepSeconds));

    const auto combine = [](double initial, double first, double second,
                            double third, double fourth) {
        return initial +
               (kStepSeconds / 6.0) *
                   (first + 2.0 * second + 2.0 * third + fourth);
    };

    return {
        {
            combine(state.position.x, k1.position_rate.x,
                    k2.position_rate.x, k3.position_rate.x,
                    k4.position_rate.x),
            combine(state.position.y, k1.position_rate.y,
                    k2.position_rate.y, k3.position_rate.y,
                    k4.position_rate.y),
            combine(state.position.z, k1.position_rate.z,
                    k2.position_rate.z, k3.position_rate.z,
                    k4.position_rate.z),
        },
        {
            combine(state.velocity.x, k1.velocity_rate.x,
                    k2.velocity_rate.x, k3.velocity_rate.x,
                    k4.velocity_rate.x),
            combine(state.velocity.y, k1.velocity_rate.y,
                    k2.velocity_rate.y, k3.velocity_rate.y,
                    k4.velocity_rate.y),
            combine(state.velocity.z, k1.velocity_rate.z,
                    k2.velocity_rate.z, k3.velocity_rate.z,
                    k4.velocity_rate.z),
        },
    };
}

double centered(std::uint64_t episode, std::uint64_t multiplier,
                std::uint64_t modulus, std::int64_t center) {
    const std::uint64_t residue = (episode * multiplier) % modulus;
    return static_cast<double>(static_cast<std::int64_t>(residue) - center);
}

State initialState(std::uint64_t episode) {
    return {
        {
            0.0 + 0.001 * centered(episode, 1U, 23U, 11),
            0.0 + 0.001 * centered(episode, 7U, 29U, 14),
            100.0 + 0.001 * centered(episode, 11U, 31U, 15),
        },
        {
            10.0 + 0.0001 * centered(episode, 13U, 37U, 18),
            -5.0 + 0.0001 * centered(episode, 17U, 41U, 20),
            20.0 + 0.0001 * centered(episode, 19U, 43U, 21),
        },
    };
}

double observable(const State& state) {
    return state.position.x + 3.0 * state.position.y +
           5.0 * state.position.z + 7.0 * state.velocity.x +
           11.0 * state.velocity.y + 13.0 * state.velocity.z;
}

WorkloadResult runWorkload(std::uint64_t episodes, bool skip_integrator) {
    if (episodes == 0U || episodes > kMaximumHarnessEpisodes) {
        throw std::invalid_argument(
            "episodes must be in the inclusive harness range [1, 1000000]");
    }
    if (episodes >
        std::numeric_limits<std::uint64_t>::max() /
            (kStepsPerEpisode * kDerivativeEvaluationsPerStep)) {
        throw std::overflow_error("workload counter overflow");
    }

    Vec3 position_sum{};
    Vec3 velocity_sum{};
    double weighted_sum = 0.0;
    std::uint64_t weight_sum = 0U;

    const auto started = std::chrono::steady_clock::now();
    for (std::uint64_t episode = 0U; episode < episodes; ++episode) {
        State state = initialState(episode);
        if (!skip_integrator) {
            for (std::uint64_t step = 0U; step < kStepsPerEpisode; ++step) {
                state = rk4Step(state);
            }
        }
        if (!finite(state)) {
            throw std::runtime_error("benchmark produced a non-finite state");
        }

        position_sum = add(position_sum, state.position);
        velocity_sum = add(velocity_sum, state.velocity);
        const std::uint64_t weight = (episode % 31U) + 1U;
        weight_sum += weight;
        weighted_sum += static_cast<double>(weight) * observable(state);
    }
    const auto finished = std::chrono::steady_clock::now();

    const double inverse_episodes = 1.0 / static_cast<double>(episodes);
    WorkloadResult result;
    result.episodes = episodes;
    result.completed_steps = episodes * kStepsPerEpisode;
    result.derivative_evaluations =
        result.completed_steps * kDerivativeEvaluationsPerStep;
    result.weight_sum = weight_sum;
    result.elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started)
            .count();
    result.mean_final_position = scaled(position_sum, inverse_episodes);
    result.mean_final_velocity = scaled(velocity_sum, inverse_episodes);
    result.weighted_observable = weighted_sum / static_cast<double>(weight_sum);
    result.skipped_integrator = skip_integrator;
    return result;
}

std::string compilerIdentity() {
#if defined(_MSC_VER)
    return "msvc-" + std::to_string(_MSC_FULL_VER);
#elif defined(__clang__)
    return "clang-" + std::string(__clang_version__);
#elif defined(__GNUC__)
    return "gcc-" + std::string(__VERSION__);
#else
    return "unknown";
#endif
}

std::string buildConfiguration() {
#if defined(NDEBUG)
    return "release";
#else
    return "debug";
#endif
}

void writeVec3(std::ostream& stream, const Vec3& value) {
    stream << '[' << value.x << ", " << value.y << ", " << value.z << ']';
}

void writeReport(std::ostream& stream, const WorkloadResult& result) {
    using Clock = std::chrono::steady_clock;
    stream << std::setprecision(17);
    stream << "{\n"
              "  \"schema_version\": \""
           << kSchemaVersion << "\",\n"
           << "  \"workload_id\": \"" << kWorkloadId << "\",\n"
           << "  \"model_id\": \"" << kModelId << "\",\n"
           << "  \"algorithm\": \"classical-rk4-fixed-step\",\n"
           << "  \"compiler_identity\": \"" << compilerIdentity()
           << "\",\n"
           << "  \"build_configuration\": \"" << buildConfiguration()
           << "\",\n"
           << "  \"timer\": {\"name\": \"std::chrono::steady_clock\", "
              "\"is_steady\": "
           << (Clock::is_steady ? "true" : "false")
           << ", \"period_numerator\": " << Clock::period::num
           << ", \"period_denominator\": " << Clock::period::den
           << "},\n"
           << "  \"timed_stage\": \"rk4-batch-only\",\n"
           << "  \"elapsed_ns\": " << result.elapsed_ns << ",\n"
           << "  \"mutation\": "
           << (result.skipped_integrator ? "\"skip-integrator\"" : "null")
           << ",\n"
           << "  \"semantic_result\": {\n"
           << "    \"completed_episodes\": " << result.episodes << ",\n"
           << "    \"steps_per_episode\": " << kStepsPerEpisode << ",\n"
           << "    \"completed_steps\": " << result.completed_steps << ",\n"
           << "    \"derivative_evaluations\": "
           << result.derivative_evaluations << ",\n"
           << "    \"weight_sum\": " << result.weight_sum << ",\n"
           << "    \"mean_final_position_m\": ";
    writeVec3(stream, result.mean_final_position);
    stream << ",\n    \"mean_final_velocity_mps\": ";
    writeVec3(stream, result.mean_final_velocity);
    stream << ",\n    \"weighted_observable\": "
           << result.weighted_observable << "\n"
           << "  }\n"
           << "}\n";
}

std::uint64_t parsePositiveInteger(std::string_view text) {
    if (text.empty() || text.front() == '-') {
        throw std::invalid_argument("episodes must be an unsigned integer");
    }
    std::size_t parsed = 0U;
    const std::uint64_t value = std::stoull(std::string{text}, &parsed, 10);
    if (parsed != text.size()) {
        throw std::invalid_argument("episodes must be an unsigned integer");
    }
    return value;
}

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--self-check") {
            options.self_check = true;
        } else if (argument == "--episodes" && index + 1 < argc) {
            options.episodes = parsePositiveInteger(argv[++index]);
        } else if (argument == "--report" && index + 1 < argc) {
            options.report_path = argv[++index];
        } else if (argument == "--mutation" && index + 1 < argc) {
            const std::string_view mutation{argv[++index]};
            if (mutation != "skip-integrator") {
                throw std::invalid_argument("unknown benchmark mutation");
            }
            options.skip_integrator = true;
        } else {
            throw std::invalid_argument(
                "usage: --self-check | --episodes <count> --report <path> "
                "[--mutation skip-integrator]");
        }
    }

    if (options.self_check) {
        if (argc != 2) {
            throw std::invalid_argument("--self-check cannot be combined");
        }
        return options;
    }
    if (options.episodes == 0U || options.report_path.empty()) {
        throw std::invalid_argument(
            "--episodes and --report are required for a benchmark run");
    }
    return options;
}

bool selfCheck() {
    const WorkloadResult normal = runWorkload(4U, false);
    const WorkloadResult skipped = runWorkload(4U, true);
    return normal.episodes == 4U && normal.completed_steps == 320U &&
           normal.derivative_evaluations == 1280U && normal.weight_sum == 10U &&
           finite(normal.mean_final_position) &&
           finite(normal.mean_final_velocity) &&
           std::abs(normal.mean_final_position.z -
                    skipped.mean_final_position.z) > 1.0 &&
           normal.weighted_observable != skipped.weighted_observable;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        if (options.self_check) {
            const bool passed = selfCheck();
            std::cout << "minimal 3DoF benchmark self-check status="
                      << (passed ? "pass" : "fail") << '\n';
            return passed ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        const WorkloadResult result =
            runWorkload(options.episodes, options.skip_integrator);
        std::ofstream stream{options.report_path,
                             std::ios::out | std::ios::binary};
        if (!stream) {
            throw std::runtime_error("unable to open benchmark report path");
        }
        writeReport(stream, result);
        if (!stream) {
            throw std::runtime_error("unable to write benchmark report");
        }
        std::cout << "minimal 3DoF benchmark episodes=" << result.episodes
                  << " steps=" << result.completed_steps
                  << " elapsed_ns=" << result.elapsed_ns
                  << " status=pass\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "minimal 3DoF benchmark error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

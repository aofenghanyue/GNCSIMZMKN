#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-PUBLISH-01";
constexpr double kTolerance = 1.0e-12;

struct State {
    double altitude_m = 1000.0;
    double vertical_velocity_mps = 10.0;
};

struct Derivative {
    double altitude_rate_mps = 0.0;
    double vertical_acceleration_mps2 = -2.0;
};

struct Truth {
    State state;
    double sample_time_s = 0.0;
};

struct ProbeResult {
    State before_publish_t0;
    State after_publish_t0;
    Truth truth_t0;
    State after_commit_t1;
    Truth truth_before_publish_t1;
    State after_publish_t1;
    Truth truth_t1;
    bool state_unchanged_t0 = false;
    bool state_unchanged_t1 = false;
    bool truth_stale_between_boundaries = false;
    bool mutation_rejected = false;
    std::vector<std::string> events;
};

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kTolerance;
}

bool sameStateExact(const State& lhs, const State& rhs) {
    return lhs.altitude_m == rhs.altitude_m &&
           lhs.vertical_velocity_mps == rhs.vertical_velocity_mps;
}

State addScaled(const State& state,
                const Derivative& derivative,
                double scale) {
    return {
        state.altitude_m + derivative.altitude_rate_mps * scale,
        state.vertical_velocity_mps +
            derivative.vertical_acceleration_mps2 * scale,
    };
}

Derivative evaluate(const State& state) {
    return {state.vertical_velocity_mps, -2.0};
}

State rk4Step(const State& initial, double dt_s) {
    const Derivative k1 = evaluate(initial);
    const Derivative k2 = evaluate(addScaled(initial, k1, 0.5 * dt_s));
    const Derivative k3 = evaluate(addScaled(initial, k2, 0.5 * dt_s));
    const Derivative k4 = evaluate(addScaled(initial, k3, dt_s));
    return {
        initial.altitude_m +
            dt_s * (k1.altitude_rate_mps + 2.0 * k2.altitude_rate_mps +
                    2.0 * k3.altitude_rate_mps + k4.altitude_rate_mps) /
                6.0,
        initial.vertical_velocity_mps +
            dt_s * (k1.vertical_acceleration_mps2 +
                    2.0 * k2.vertical_acceleration_mps2 +
                    2.0 * k3.vertical_acceleration_mps2 +
                    k4.vertical_acceleration_mps2) /
                6.0,
    };
}

Truth publish(const State& committed, double boundary_time_s) {
    return {committed, boundary_time_s};
}

bool validPublish(const State& before,
                  const State& after,
                  const Truth& truth,
                  double boundary_time_s) {
    return sameStateExact(before, after) &&
           sameStateExact(before, truth.state) &&
           truth.sample_time_s == boundary_time_s;
}

ProbeResult runProbe() {
    constexpr double dt_s = 0.5;
    ProbeResult result;

    result.before_publish_t0 = State{};
    State committed = result.before_publish_t0;
    result.truth_t0 = publish(committed, 0.0);
    result.after_publish_t0 = committed;
    result.state_unchanged_t0 =
        sameStateExact(result.before_publish_t0, result.after_publish_t0);
    result.events.push_back("publish:t0");

    committed = rk4Step(committed, dt_s);
    result.after_commit_t1 = committed;
    result.truth_before_publish_t1 = result.truth_t0;
    result.events.push_back("commit:t1");

    result.truth_t1 = publish(committed, dt_s);
    result.after_publish_t1 = committed;
    result.state_unchanged_t1 =
        sameStateExact(result.after_commit_t1, result.after_publish_t1);
    result.truth_stale_between_boundaries =
        sameStateExact(result.truth_before_publish_t1.state,
                       result.truth_t0.state) &&
        result.truth_before_publish_t1.sample_time_s ==
            result.truth_t0.sample_time_s;
    result.events.push_back("publish:t1");

    State mutated = result.before_publish_t0;
    mutated.altitude_m += 1.0;
    result.mutation_rejected =
        !validPublish(result.before_publish_t0,
                      mutated,
                      publish(result.before_publish_t0, 0.0),
                      0.0);
    return result;
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\""
              << ",\"altitude_before_publish_t0_m\":"
              << result.before_publish_t0.altitude_m
              << ",\"velocity_before_publish_t0_mps\":"
              << result.before_publish_t0.vertical_velocity_mps
              << ",\"altitude_after_publish_t0_m\":"
              << result.after_publish_t0.altitude_m
              << ",\"velocity_after_publish_t0_mps\":"
              << result.after_publish_t0.vertical_velocity_mps
              << ",\"truth_altitude_t0_m\":"
              << result.truth_t0.state.altitude_m
              << ",\"truth_velocity_t0_mps\":"
              << result.truth_t0.state.vertical_velocity_mps
              << ",\"truth_sample_time_t0_s\":"
              << result.truth_t0.sample_time_s
              << ",\"altitude_after_commit_t1_m\":"
              << result.after_commit_t1.altitude_m
              << ",\"velocity_after_commit_t1_mps\":"
              << result.after_commit_t1.vertical_velocity_mps
              << ",\"truth_altitude_before_publish_t1_m\":"
              << result.truth_before_publish_t1.state.altitude_m
              << ",\"truth_sample_time_before_publish_t1_s\":"
              << result.truth_before_publish_t1.sample_time_s
              << ",\"altitude_after_publish_t1_m\":"
              << result.after_publish_t1.altitude_m
              << ",\"velocity_after_publish_t1_mps\":"
              << result.after_publish_t1.vertical_velocity_mps
              << ",\"truth_altitude_t1_m\":"
              << result.truth_t1.state.altitude_m
              << ",\"truth_velocity_t1_mps\":"
              << result.truth_t1.state.vertical_velocity_mps
              << ",\"truth_sample_time_t1_s\":"
              << result.truth_t1.sample_time_s
              << ",\"state_unchanged_t0\":"
              << (result.state_unchanged_t0 ? "true" : "false")
              << ",\"state_unchanged_t1\":"
              << (result.state_unchanged_t1 ? "true" : "false")
              << ",\"truth_stale_between_boundaries\":"
              << (result.truth_stale_between_boundaries ? "true" : "false")
              << ",\"mutation_rejected\":"
              << (result.mutation_rejected ? "true" : "false")
              << ",\"events\":[";
    for (std::size_t index = 0; index < result.events.size(); ++index) {
        if (index > 0) {
            std::cout << ',';
        }
        std::cout << '\"' << result.events[index] << '\"';
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_legacy_publish_probe --self-check\n";
        return 2;
    }

    try {
        const ProbeResult result = runProbe();
        require(validPublish(result.before_publish_t0,
                             result.after_publish_t0,
                             result.truth_t0,
                             0.0),
                "t0 publish mutated committed state or used the wrong time");
        require(validPublish(result.after_commit_t1,
                             result.after_publish_t1,
                             result.truth_t1,
                             0.5),
                "t1 publish mutated committed state or used the wrong time");
        require(nearlyEqual(result.after_commit_t1.altitude_m, 1004.75) &&
                    nearlyEqual(result.after_commit_t1.vertical_velocity_mps,
                                9.0),
                "RK4 committed state differs from the analytic trajectory");
        require(nearlyEqual(
                    result.truth_before_publish_t1.state.altitude_m, 1000.0) &&
                    result.truth_before_publish_t1.sample_time_s == 0.0,
                "truth advanced before the next publish boundary");
        require(result.state_unchanged_t0 && result.state_unchanged_t1 &&
                    result.truth_stale_between_boundaries,
                "publish boundary invariants were not observed");
        require(result.events == std::vector<std::string>{
                                     "publish:t0", "commit:t1", "publish:t1"},
                "publish/commit boundary order differs");
        require(result.mutation_rejected,
                "a publish-time committed-state mutation was accepted");
        writeJson(result);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

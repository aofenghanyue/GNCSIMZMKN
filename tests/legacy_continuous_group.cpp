#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-GROUP-04";
constexpr const char* kScopeId = "scope:mass-position";
constexpr double kTolerance = 1.0e-12;

struct State {
    double mass_kg = 10.0;
    double position_m = 0.0;
};

struct Derivative {
    double mass_rate_kg_per_s = -2.0;
    double position_rate_mps = 0.0;
};

struct StageEvent {
    std::size_t sequence = 0;
    int rk_stage = 0;
    double time_s = 0.0;
    State candidate;
    Derivative derivative;
};

struct ProbeResult {
    std::vector<StageEvent> stages;
    State committed;
    int commit_count = 0;
    double split_snapshot_position_m = 0.0;
    bool split_closure_rejected = false;
    bool valid_membership_accepted = false;
    bool unregistered_member_rejected = false;
    bool duplicate_ownership_rejected = false;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kTolerance;
}

State addScaled(const State& state,
                const Derivative& derivative,
                double scale) {
    return {
        state.mass_kg + derivative.mass_rate_kg_per_s * scale,
        state.position_m + derivative.position_rate_mps * scale,
    };
}

Derivative evaluate(const State& state,
                    int rk_stage,
                    double time_s,
                    std::vector<StageEvent>& stages) {
    const Derivative derivative{-2.0, state.mass_kg};
    stages.push_back(StageEvent{
        stages.size(), rk_stage, time_s, state, derivative});
    return derivative;
}

State jointRk4Step(const State& initial,
                   double initial_time_s,
                   double dt_s,
                   std::vector<StageEvent>& stages) {
    const Derivative k1 = evaluate(initial, 1, initial_time_s, stages);
    const Derivative k2 = evaluate(
        addScaled(initial, k1, 0.5 * dt_s),
        2,
        initial_time_s + 0.5 * dt_s,
        stages);
    const Derivative k3 = evaluate(
        addScaled(initial, k2, 0.5 * dt_s),
        3,
        initial_time_s + 0.5 * dt_s,
        stages);
    const Derivative k4 = evaluate(
        addScaled(initial, k3, dt_s),
        4,
        initial_time_s + dt_s,
        stages);
    return {
        initial.mass_kg +
            dt_s * (k1.mass_rate_kg_per_s +
                    2.0 * k2.mass_rate_kg_per_s +
                    2.0 * k3.mass_rate_kg_per_s +
                    k4.mass_rate_kg_per_s) /
                6.0,
        initial.position_m +
            dt_s * (k1.position_rate_mps +
                    2.0 * k2.position_rate_mps +
                    2.0 * k3.position_rate_mps +
                    k4.position_rate_mps) /
                6.0,
    };
}

bool validMembership(const std::set<std::string>& registered,
                     const std::vector<std::vector<std::string>>& scopes) {
    std::set<std::string> claimed;
    for (const std::vector<std::string>& members : scopes) {
        if (members.empty()) {
            return false;
        }
        for (const std::string& member : members) {
            if (registered.count(member) == 0 ||
                !claimed.insert(member).second) {
                return false;
            }
        }
    }
    return true;
}

bool validTrace(const ProbeResult& result) {
    const std::array<StageEvent, 4> expected{{
        {0, 1, 0.0, {10.0, 0.0}, {-2.0, 10.0}},
        {1, 2, 0.5, {9.0, 5.0}, {-2.0, 9.0}},
        {2, 3, 0.5, {9.0, 4.5}, {-2.0, 9.0}},
        {3, 4, 1.0, {8.0, 9.0}, {-2.0, 8.0}},
    }};
    if (result.stages.size() != expected.size() || result.commit_count != 1) {
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const StageEvent& actual = result.stages[index];
        const StageEvent& target = expected[index];
        if (actual.sequence != target.sequence ||
            actual.rk_stage != target.rk_stage ||
            !nearlyEqual(actual.time_s, target.time_s) ||
            !nearlyEqual(actual.candidate.mass_kg,
                         target.candidate.mass_kg) ||
            !nearlyEqual(actual.candidate.position_m,
                         target.candidate.position_m) ||
            !nearlyEqual(actual.derivative.mass_rate_kg_per_s,
                         target.derivative.mass_rate_kg_per_s) ||
            !nearlyEqual(actual.derivative.position_rate_mps,
                         target.derivative.position_rate_mps)) {
            return false;
        }
    }
    return nearlyEqual(result.committed.mass_kg, 8.0) &&
           nearlyEqual(result.committed.position_m, 9.0);
}

ProbeResult runProbe() {
    ProbeResult result;
    const State initial{};
    result.committed = jointRk4Step(initial, 0.0, 1.0, result.stages);
    result.commit_count = 1;

    result.split_snapshot_position_m =
        initial.position_m + initial.mass_kg * 1.0;
    result.split_closure_rejected =
        !nearlyEqual(result.committed.position_m,
                     result.split_snapshot_position_m);

    const std::set<std::string> registered{"mass", "position"};
    result.valid_membership_accepted = validMembership(
        registered, {{"mass", "position"}});
    result.unregistered_member_rejected = !validMembership(
        registered, {{"mass", "orphan"}});
    result.duplicate_ownership_rejected = !validMembership(
        registered, {{"mass"}, {"mass"}});
    return result;
}

void writeJson(const ProbeResult& result) {
    std::cout << "{\"oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\""
              << ",\"scope_id\":\"" << kScopeId << "\""
              << ",\"member_ids\":[\"mass\",\"position\"]"
              << ",\"events\":[";
    for (std::size_t index = 0; index < result.stages.size(); ++index) {
        if (index > 0) {
            std::cout << ',';
        }
        const StageEvent& stage = result.stages[index];
        std::cout << "{\"sequence\":" << stage.sequence
                  << ",\"event_kind\":\"rk-stage\""
                  << ",\"rk_stage\":" << stage.rk_stage
                  << ",\"time_s\":" << stage.time_s
                  << ",\"candidate_mass_kg\":"
                  << stage.candidate.mass_kg
                  << ",\"candidate_position_m\":"
                  << stage.candidate.position_m
                  << ",\"mass_rate_kg_per_s\":"
                  << stage.derivative.mass_rate_kg_per_s
                  << ",\"position_rate_mps\":"
                  << stage.derivative.position_rate_mps << '}';
    }
    std::cout << ",{";
    std::cout << "\"sequence\":4,\"event_kind\":\"group-commit\""
              << ",\"effective_time_s\":1"
              << ",\"committed_mass_kg\":" << result.committed.mass_kg
              << ",\"committed_position_m\":"
              << result.committed.position_m << "}]"
              << ",\"member_final\":{\"mass_kg\":"
              << result.committed.mass_kg << ",\"position_m\":"
              << result.committed.position_m << '}'
              << ",\"commit_count\":" << result.commit_count
              << ",\"split_snapshot_position_m\":"
              << result.split_snapshot_position_m
              << ",\"split_closure_rejected\":"
              << (result.split_closure_rejected ? "true" : "false")
              << ",\"valid_membership_accepted\":"
              << (result.valid_membership_accepted ? "true" : "false")
              << ",\"unregistered_member_rejected\":"
              << (result.unregistered_member_rejected ? "true" : "false")
              << ",\"duplicate_ownership_rejected\":"
              << (result.duplicate_ownership_rejected ? "true" : "false")
              << "}\n";
}

} // namespace


int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_legacy_continuous_group_probe --self-check\n";
        return 2;
    }

    try {
        const ProbeResult result = runProbe();
        require(validTrace(result),
                "joint RK4 stage or commit trace differs");
        require(result.split_closure_rejected &&
                    nearlyEqual(result.split_snapshot_position_m, 10.0),
                "split snapshot closure was not distinguished");
        require(result.valid_membership_accepted,
                "valid scope membership was rejected");
        require(result.unregistered_member_rejected,
                "unregistered scope member was accepted");
        require(result.duplicate_ownership_rejected,
                "duplicate scope ownership was accepted");
        writeJson(result);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

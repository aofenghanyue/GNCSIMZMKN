#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-SYNC-03";
constexpr double kTolerance = 1.0e-12;

struct State {
    double mass_kg = 10.0;
    double position = 0.0;
};

struct ProbeResult {
    State committed;
    double premature_position = 0.0;
    std::vector<std::string> journal;
};

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kTolerance;
}

template <typename Derivative>
double rk4Step(double initial, double dt_s, Derivative derivative) {
    const double k1 = derivative(initial);
    const double k2 = derivative(initial + 0.5 * dt_s * k1);
    const double k3 = derivative(initial + 0.5 * dt_s * k2);
    const double k4 = derivative(initial + dt_s * k3);
    return initial + dt_s * (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
}

std::size_t eventIndex(const std::vector<std::string>& journal,
                       const std::string& event) {
    const auto found = std::find(journal.begin(), journal.end(), event);
    return found == journal.end()
               ? journal.size()
               : static_cast<std::size_t>(std::distance(journal.begin(), found));
}

bool hasCandidateBarrier(const std::vector<std::string>& journal) {
    const std::set<std::string> expected{
        "candidate-complete:mass",
        "candidate-complete:position",
        "commit:mass",
        "commit:position",
    };
    if (journal.size() != expected.size() ||
        std::set<std::string>(journal.begin(), journal.end()) != expected) {
        return false;
    }

    for (const std::string& candidate : {
             std::string("candidate-complete:mass"),
             std::string("candidate-complete:position")}) {
        for (const std::string& commit : {
                 std::string("commit:mass"),
                 std::string("commit:position")}) {
            if (eventIndex(journal, candidate) >= eventIndex(journal, commit)) {
                return false;
            }
        }
    }
    return true;
}

ProbeResult runProbe() {
    constexpr double dt_s = 1.0;
    constexpr double mass_rate_kg_per_s = -2.0;

    const State snapshot{};
    const double mass_candidate = rk4Step(
        snapshot.mass_kg,
        dt_s,
        [mass_rate_kg_per_s](double) { return mass_rate_kg_per_s; });
    const double position_candidate = rk4Step(
        snapshot.position,
        dt_s,
        [&snapshot](double) { return snapshot.mass_kg; });

    ProbeResult result;
    result.journal.push_back("candidate-complete:mass");
    result.journal.push_back("candidate-complete:position");

    // Commit in the reverse order to demonstrate that the preserved fact is
    // the candidate barrier, independent of a container's commit iteration.
    result.committed.position = position_candidate;
    result.journal.push_back("commit:position");
    result.committed.mass_kg = mass_candidate;
    result.journal.push_back("commit:mass");

    result.premature_position = rk4Step(
        snapshot.position,
        dt_s,
        [mass_candidate](double) { return mass_candidate; });
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
              << "\",\"status\":\"passed\",\"mass_final_kg\":"
              << result.committed.mass_kg
              << ",\"position_final\":" << result.committed.position
              << ",\"premature_position_final\":"
              << result.premature_position
              << ",\"candidate_barrier\":true"
              << ",\"early_commit_rejected\":true,\"journal\":[";
    for (std::size_t index = 0; index < result.journal.size(); ++index) {
        if (index > 0) {
            std::cout << ',';
        }
        std::cout << '\"' << result.journal[index] << '\"';
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_legacy_sync_commit_probe --self-check\n";
        return 2;
    }

    try {
        const ProbeResult result = runProbe();
        require(hasCandidateBarrier(result.journal),
                "candidate barrier was not preserved");
        require(nearlyEqual(result.committed.mass_kg, 8.0),
                "mass candidate differs from the analytic result");
        require(nearlyEqual(result.committed.position, 10.0),
                "position did not read the committed t_k mass");
        require(nearlyEqual(result.premature_position, 8.0),
                "premature-commit discriminator differs from its control");
        require(!nearlyEqual(result.committed.position,
                             result.premature_position),
                "the case does not distinguish synchronized and early commit");

        const std::vector<std::string> early_commit{
            "candidate-complete:mass",
            "commit:mass",
            "candidate-complete:position",
            "commit:position",
        };
        require(!hasCandidateBarrier(early_commit),
                "an early commit journal was accepted");

        writeJson(result);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

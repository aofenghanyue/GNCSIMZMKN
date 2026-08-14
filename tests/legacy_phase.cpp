#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-PHASE-02";

const std::array<std::string, 7> kMacroPhases{
    "environment",
    "perturbation",
    "input",
    "process",
    "output",
    "interaction",
    "evaluation",
};

struct Registration {
    std::string phase;
    std::string probe_id;
    int priority = 0;
    std::size_t registration_order = 0;
};

struct Event {
    std::size_t sequence = 0;
    std::string event_kind;
    std::string phase;
    std::string probe_id;
    std::uint64_t step = 0;
    double time_s = 0.0;
};

struct ProbeResult {
    std::vector<Event> events;
    bool cross_phase_priority_independent = false;
    bool cross_phase_registration_independent = false;
    bool swap_rejected = false;
    bool duplicate_rejected = false;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::size_t phaseRank(const std::string& phase) {
    const auto found = std::find(kMacroPhases.begin(), kMacroPhases.end(), phase);
    return found == kMacroPhases.end()
               ? kMacroPhases.size()
               : static_cast<std::size_t>(
                     std::distance(kMacroPhases.begin(), found));
}

std::vector<Event> schedule(std::vector<Registration> registrations) {
    for (const Registration& registration : registrations) {
        require(phaseRank(registration.phase) < kMacroPhases.size(),
                "registration uses an unknown macro phase");
    }

    std::stable_sort(
        registrations.begin(),
        registrations.end(),
        [](const Registration& lhs, const Registration& rhs) {
            const std::size_t lhs_rank = phaseRank(lhs.phase);
            const std::size_t rhs_rank = phaseRank(rhs.phase);
            if (lhs_rank != rhs_rank) {
                return lhs_rank < rhs_rank;
            }
            if (lhs.priority != rhs.priority) {
                return lhs.priority < rhs.priority;
            }
            return lhs.registration_order < rhs.registration_order;
        });

    std::vector<Event> events;
    events.reserve(registrations.size());
    for (const Registration& registration : registrations) {
        events.push_back(Event{
            events.size(),
            "phase-invoke",
            registration.phase,
            registration.probe_id,
            0,
            0.0,
        });
    }
    return events;
}

bool validTrace(const std::vector<Event>& events) {
    if (events.size() != kMacroPhases.size()) {
        return false;
    }
    for (std::size_t index = 0; index < events.size(); ++index) {
        const Event& event = events[index];
        const std::string& phase = kMacroPhases[index];
        if (event.sequence != index || event.event_kind != "phase-invoke" ||
            event.phase != phase || event.probe_id != "probe:" + phase ||
            event.step != 0 || event.time_s != 0.0) {
            return false;
        }
    }
    return true;
}

std::vector<Event> eventsForPhases(const std::vector<std::string>& phases) {
    std::vector<Event> events;
    events.reserve(phases.size());
    for (const std::string& phase : phases) {
        events.push_back(Event{
            events.size(), "phase-invoke", phase, "probe:" + phase, 0, 0.0});
    }
    return events;
}

std::vector<Registration> scrambledRegistrations() {
    return {
        {"evaluation", "probe:evaluation", -100, 0},
        {"output", "probe:output", 50, 1},
        {"environment", "probe:environment", 100, 2},
        {"interaction", "probe:interaction", -20, 3},
        {"input", "probe:input", 0, 4},
        {"perturbation", "probe:perturbation", 30, 5},
        {"process", "probe:process", -40, 6},
    };
}

ProbeResult runProbe() {
    ProbeResult result;
    const std::vector<Registration> original = scrambledRegistrations();
    result.events = schedule(original);

    std::vector<Registration> priority_mutation = original;
    for (std::size_t index = 0; index < priority_mutation.size(); ++index) {
        priority_mutation[index].priority =
            static_cast<int>((priority_mutation.size() - index) * 1000);
    }
    result.cross_phase_priority_independent =
        validTrace(schedule(priority_mutation));

    std::vector<Registration> registration_mutation = original;
    std::reverse(registration_mutation.begin(), registration_mutation.end());
    for (std::size_t index = 0; index < registration_mutation.size(); ++index) {
        registration_mutation[index].registration_order = index;
    }
    result.cross_phase_registration_independent =
        validTrace(schedule(registration_mutation));

    const std::vector<std::string> swapped{
        "environment",
        "perturbation",
        "input",
        "output",
        "process",
        "interaction",
        "evaluation",
    };
    result.swap_rejected = !validTrace(eventsForPhases(swapped));

    const std::vector<std::string> duplicated{
        "environment",
        "perturbation",
        "input",
        "input",
        "output",
        "interaction",
        "evaluation",
    };
    result.duplicate_rejected = !validTrace(eventsForPhases(duplicated));
    return result;
}

void writeJson(const ProbeResult& result) {
    std::cout << "{\"oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\",\"events\":[";
    for (std::size_t index = 0; index < result.events.size(); ++index) {
        if (index > 0) {
            std::cout << ',';
        }
        const Event& event = result.events[index];
        std::cout << "{\"sequence\":" << event.sequence
                  << ",\"event_kind\":\"" << event.event_kind << "\""
                  << ",\"phase\":\"" << event.phase << "\""
                  << ",\"probe_id\":\"" << event.probe_id << "\""
                  << ",\"step\":" << event.step
                  << ",\"time_s\":" << event.time_s << '}';
    }
    std::cout << "]"
              << ",\"cross_phase_priority_independent\":"
              << (result.cross_phase_priority_independent ? "true" : "false")
              << ",\"cross_phase_registration_independent\":"
              << (result.cross_phase_registration_independent ? "true" : "false")
              << ",\"swap_rejected\":"
              << (result.swap_rejected ? "true" : "false")
              << ",\"duplicate_rejected\":"
              << (result.duplicate_rejected ? "true" : "false")
              << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_legacy_phase_probe --self-check\n";
        return 2;
    }

    try {
        const ProbeResult result = runProbe();
        require(validTrace(result.events),
                "macro phase trace differs from the expected sequence");
        require(result.cross_phase_priority_independent,
                "cross-phase priority changed the macro sequence");
        require(result.cross_phase_registration_independent,
                "cross-phase registration changed the macro sequence");
        require(result.swap_rejected,
                "a process/output phase swap was accepted");
        require(result.duplicate_rejected,
                "a duplicate input phase was accepted");
        writeJson(result);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

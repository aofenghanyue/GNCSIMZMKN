#include "gnc/core/simulator.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-PHASE-02";

struct Event {
    std::string phase;
    std::string probe_id;
    std::uint64_t step = 0;
    double time_s = 0.0;
};

class PhaseProbe final : public gnc::core::DiscreteNode {
public:
    PhaseProbe(std::string phase,
               std::string probe_id,
               std::vector<Event>* events)
        : DiscreteNode("PhaseProbe"),
          phase_(std::move(phase)),
          probe_id_(std::move(probe_id)),
          events_(events) {}

    void update(const gnc::core::StepContext& context) override {
        if (events_) {
            events_->push_back(
                Event{phase_, probe_id_, context.step, context.time_s});
        }
    }

private:
    std::string phase_;
    std::string probe_id_;
    std::vector<Event>* events_ = nullptr;
};

struct Registration {
    const char* phase_name;
    gnc::core::DiscretePhase phase;
    int priority;
};

const std::vector<Registration> kScrambledRegistrations{
    {"evaluation", gnc::core::DiscretePhase::Evaluation, -100},
    {"output", gnc::core::DiscretePhase::Output, 50},
    {"environment", gnc::core::DiscretePhase::Environment, 100},
    {"interaction", gnc::core::DiscretePhase::Interaction, -20},
    {"input", gnc::core::DiscretePhase::Input, 0},
    {"perturbation", gnc::core::DiscretePhase::Perturbation, 30},
    {"process", gnc::core::DiscretePhase::Process, -40},
};

const std::vector<std::string> kExpectedPhases{
    "environment",
    "perturbation",
    "input",
    "process",
    "output",
    "interaction",
    "evaluation",
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int parseRerunIndex(const std::string& value) {
    std::size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    require(consumed == value.size() && parsed > 0,
            "rerun index must be a positive integer");
    return parsed;
}

std::vector<Event> captureTrace() {
    std::vector<Event> events;
    gnc::core::Simulator simulator;
    simulator.configure(gnc::core::SimulatorConfig{1.0, 0.0});

    for (const Registration& registration : kScrambledRegistrations) {
        const std::string probe_id =
            std::string("probe:") + registration.phase_name;
        auto probe = std::make_unique<PhaseProbe>(
            registration.phase_name, probe_id, &events);
        auto* probe_ptr = probe.get();
        simulator.getRegistry().add<PhaseProbe>(probe_id, std::move(probe));
        simulator.addNodeExecution(
            probe_ptr,
            {registration.phase, registration.priority, 0.0});
    }

    simulator.run();
    require(events.size() == kExpectedPhases.size(),
            "Legacy phase trace does not contain exactly seven events");
    for (std::size_t index = 0; index < events.size(); ++index) {
        require(events[index].phase == kExpectedPhases[index],
                "Legacy macro phase order differs from the expected sequence");
        require(events[index].step == 0 && events[index].time_s == 0.0,
                "Legacy phase event used an unexpected step or time");
    }
    return events;
}

void writeTrace(const std::string& path,
                int rerun_index,
                const std::vector<Event>& events) {
    std::ofstream output(path, std::ios::binary);
    require(output.is_open(), "could not open the requested trace path");
    output << std::setprecision(17)
           << "{\"schema_version\":\"gnczmkn.legacy-phase-trace/1\""
           << ",\"oracle_id\":\"" << kOracleId << "\""
           << ",\"rerun_index\":" << rerun_index
           << ",\"events\":[";
    for (std::size_t index = 0; index < events.size(); ++index) {
        if (index > 0) {
            output << ',';
        }
        const Event& event = events[index];
        output << "{\"sequence\":" << index
               << ",\"event_kind\":\"phase-invoke\""
               << ",\"phase\":\"" << event.phase << "\""
               << ",\"probe_id\":\"" << event.probe_id << "\""
               << ",\"step\":" << event.step
               << ",\"time_s\":" << event.time_s << '}';
    }
    output << "]}\n";
    require(output.good(), "could not write the Legacy phase trace");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5 || std::string(argv[1]) != "--output" ||
        std::string(argv[3]) != "--rerun-index") {
        std::cerr << "usage: legacy_phase_capture --output <path> "
                     "--rerun-index <positive integer>\n";
        return 2;
    }

    try {
        const int rerun_index = parseRerunIndex(argv[4]);
        const std::vector<Event> events = captureTrace();
        writeTrace(argv[2], rerun_index, events);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

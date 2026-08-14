#include "gnc/core/simulator.hpp"
#include "gnc/interfaces/i_observable.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-STOP-06";
constexpr double kTolerance = 1.0e-12;

struct Event {
    std::string kind;
    std::string field_id;
    std::uint64_t step = 0;
    double time_s = 0.0;
    double value = 0.0;
    bool predicate_met = false;
    bool recorded_row_visible = false;
    double recorded_time_s = 0.0;
    double recorded_altitude_m = 0.0;
    double recorded_velocity_mps = 0.0;
    std::string termination_reason_text;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kTolerance;
}

std::vector<std::string> splitCsvLine(std::string line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

bool readVisibleDatasetRow(const std::filesystem::path& path,
                           double& time_s,
                           double& altitude_m,
                           double& velocity_mps) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    std::string header_line;
    std::string row_line;
    if (!std::getline(input, header_line) || !std::getline(input, row_line)) {
        return false;
    }
    const auto header = splitCsvLine(header_line);
    const auto row = splitCsvLine(row_line);
    if (header != std::vector<std::string>{
                      "time",
                      "vehicle.state.altitude_m",
                      "vehicle.state.vertical_velocity_mps"} ||
        row.size() != header.size()) {
        return false;
    }

    try {
        time_s = std::stod(row[0]);
        altitude_m = std::stod(row[1]);
        velocity_mps = std::stod(row[2]);
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

class StoppingState final
    : public gnc::core::SimulationNode,
      public gnc::core::IPublishTask,
      public gnc::interfaces::IObservable {
public:
    explicit StoppingState(std::vector<Event>* events)
        : SimulationNode("LegacyStopState"), events_(events) {}

    gnc::core::PublishPhase publishPhase() const override {
        return gnc::core::PublishPhase::StateOwner;
    }

    void publish(const gnc::core::PublishContext& context) override {
        published_altitude_m_ = committed_altitude_m_;
        published_velocity_mps_ = committed_velocity_mps_;
        published_time_s_ = context.time_s;
        published_step_ = context.step;
        events_->push_back(Event{
            "publish",
            {},
            context.step,
            context.time_s,
            0.0,
            false,
            false,
            0.0,
            published_altitude_m_,
            published_velocity_mps_,
            {}});
    }

    std::vector<gnc::interfaces::ObservableField>
    getObservableFields() const override {
        return {
            {"altitude_m",
             [this]() {
                 return recordField("altitude_m", published_altitude_m_);
             }},
            {"vertical_velocity_mps",
             [this]() {
                 return recordField(
                     "vertical_velocity_mps", published_velocity_mps_);
             }},
        };
    }

    double publishedAltitudeM() const { return published_altitude_m_; }
    double publishedVelocityMps() const { return published_velocity_mps_; }
    double publishedTimeS() const { return published_time_s_; }
    std::uint64_t publishedStep() const { return published_step_; }

private:
    double recordField(const std::string& field_id, double value) const {
        events_->push_back(Event{
            "record-field-read",
            field_id,
            published_step_,
            published_time_s_,
            value,
            false,
            false,
            0.0,
            0.0,
            0.0,
            {}});
        return value;
    }

    std::vector<Event>* events_ = nullptr;
    double committed_altitude_m_ = 1000.0;
    double committed_velocity_mps_ = 10.0;
    double published_altitude_m_ = 1000.0;
    double published_velocity_mps_ = 10.0;
    double published_time_s_ = 0.0;
    std::uint64_t published_step_ = 0;
};

class StopAtInitialAltitude final
    : public gnc::core::SimulationNode,
      public gnc::interfaces::ITerminationEvaluator {
public:
    StopAtInitialAltitude(const StoppingState* state,
                          std::filesystem::path dataset_path,
                          std::vector<Event>* events)
        : SimulationNode("LegacyStopEvaluator"),
          state_(state),
          dataset_path_(std::move(dataset_path)),
          events_(events) {}

    bool shouldTerminate() const override {
        double row_time_s = 0.0;
        double row_altitude_m = 0.0;
        double row_velocity_mps = 0.0;
        const bool row_visible = readVisibleDatasetRow(
            dataset_path_, row_time_s, row_altitude_m, row_velocity_mps);
        const bool predicate_met = state_->publishedAltitudeM() >= 1000.0;
        events_->push_back(Event{
            "termination-evaluate",
            {},
            state_->publishedStep(),
            state_->publishedTimeS(),
            0.0,
            predicate_met,
            row_visible,
            row_time_s,
            row_altitude_m,
            row_velocity_mps,
            {}});
        return predicate_met;
    }

    std::string reason() const override { return "stop at t0"; }

private:
    const StoppingState* state_ = nullptr;
    std::filesystem::path dataset_path_;
    std::vector<Event>* events_ = nullptr;
};

gnc::core::ConfigNode loggerConfig(const std::filesystem::path& output) {
    auto record = gnc::core::ConfigNode::makeObject();
    record.set("vehicle.state", gnc::core::ConfigNode::makeString("all"));

    auto config = gnc::core::ConfigNode::makeObject();
    config.set(
        "directory",
        gnc::core::ConfigNode::makeString(output.parent_path().string()));
    config.set(
        "session_name",
        gnc::core::ConfigNode::makeString(output.stem().string()));
    config.set("format", gnc::core::ConfigNode::makeString("csv"));
    config.set("precision", gnc::core::ConfigNode::makeNumber(12));
    config.set("flush_every_step", gnc::core::ConfigNode::makeBool(true));
    config.set("record_initial_state", gnc::core::ConfigNode::makeBool(true));
    config.set("record", record);
    return config;
}

void validateTrace(const std::vector<Event>& events) {
    require(events.size() == 4,
            "Legacy STOP trace must contain publish, two field reads and evaluation");
    require(events[0].kind == "publish" &&
                events[1].kind == "record-field-read" &&
                events[1].field_id == "altitude_m" &&
                events[2].kind == "record-field-read" &&
                events[2].field_id == "vertical_velocity_mps" &&
                events[3].kind == "termination-evaluate",
            "Legacy STOP event order differs from the record-before-termination boundary");
    require(events[3].predicate_met && events[3].recorded_row_visible,
            "Termination evaluation could not observe the flushed t0 dataset row");
    require(nearlyEqual(events[3].recorded_time_s, 0.0) &&
                nearlyEqual(events[3].recorded_altitude_m, 1000.0) &&
                nearlyEqual(events[3].recorded_velocity_mps, 10.0),
            "Visible t0 dataset row contains an unexpected stopping state");
}

void writeTrace(const std::filesystem::path& path,
                int rerun_index,
                const std::string& dataset_filename,
                const std::vector<Event>& events) {
    std::ofstream output(path, std::ios::binary);
    require(output.is_open(), "could not open the requested trace path");
    output << std::setprecision(17)
           << "{\"schema_version\":\"gnczmkn.legacy-stop-trace/1\""
           << ",\"oracle_id\":\"" << kOracleId << "\""
           << ",\"rerun_index\":" << rerun_index
           << ",\"dataset_filename\":\"" << dataset_filename << "\""
           << ",\"events\":[";
    for (std::size_t index = 0; index < events.size(); ++index) {
        if (index > 0) {
            output << ',';
        }
        const Event& event = events[index];
        output << "{\"sequence\":" << index
               << ",\"event_kind\":\"" << event.kind << "\"";
        if (event.kind == "publish") {
            output << ",\"step\":" << event.step
                   << ",\"time_s\":" << event.time_s
                   << ",\"altitude_m\":" << event.recorded_altitude_m
                   << ",\"vertical_velocity_mps\":"
                   << event.recorded_velocity_mps;
        } else if (event.kind == "record-field-read") {
            output << ",\"field_id\":\"" << event.field_id << "\""
                   << ",\"step\":" << event.step
                   << ",\"sample_time_s\":" << event.time_s
                   << ",\"value\":" << event.value;
        } else if (event.kind == "termination-evaluate") {
            output << ",\"predicate_id\":\"altitude-at-or-above-1000m\""
                   << ",\"step\":" << event.step
                   << ",\"time_s\":" << event.time_s
                   << ",\"predicate_met\":"
                   << (event.predicate_met ? "true" : "false")
                   << ",\"recorded_row_visible\":"
                   << (event.recorded_row_visible ? "true" : "false")
                   << ",\"recorded_time_s\":" << event.recorded_time_s
                   << ",\"recorded_altitude_m\":"
                   << event.recorded_altitude_m
                   << ",\"recorded_vertical_velocity_mps\":"
                   << event.recorded_velocity_mps;
        } else if (event.kind == "legacy-run-complete") {
            output << ",\"final_time_s\":" << event.time_s
                   << ",\"termination_reason_text\":\""
                   << event.termination_reason_text << "\"";
        }
        output << '}';
    }
    output << "]}\n";
    require(output.good(), "could not write the Legacy STOP trace");
}

int parseRerunIndex(const std::string& value) {
    std::size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    require(consumed == value.size() && parsed > 0,
            "rerun index must be a positive integer");
    return parsed;
}

void runCapture(const std::filesystem::path& dataset_path,
                const std::filesystem::path& trace_path,
                int rerun_index) {
    require(dataset_path.extension() == ".csv",
            "dataset path must use a .csv extension");
    std::filesystem::create_directories(dataset_path.parent_path());
    std::filesystem::create_directories(trace_path.parent_path());

    std::vector<Event> events;
    gnc::core::Simulator simulator;
    simulator.configure({1.0, 2.0});

    auto state = std::make_unique<StoppingState>(&events);
    auto* state_ptr = state.get();
    simulator.getRegistry().add<StoppingState>(
        "vehicle.state", std::move(state));
    simulator.addNodeExecution(state_ptr);

    auto evaluator = std::make_unique<StopAtInitialAltitude>(
        state_ptr, dataset_path, &events);
    auto* evaluator_ptr = evaluator.get();
    simulator.getRegistry().add<StopAtInitialAltitude>(
        "mission.stop", std::move(evaluator));
    simulator.addNodeExecution(evaluator_ptr);

    require(simulator.initializeAutoDataLogger(loggerConfig(dataset_path)),
            "Legacy AutoDataLogger initialization failed");
    simulator.run();

    validateTrace(events);
    require(nearlyEqual(simulator.getCurrentTime(), 0.0),
            "Legacy STOP run advanced beyond t0");
    require(simulator.getTerminationReason() == "stop at t0",
            "Legacy STOP run returned an unexpected reason text");

    events.push_back(Event{
        "legacy-run-complete",
        {},
        0,
        simulator.getCurrentTime(),
        0.0,
        false,
        false,
        0.0,
        0.0,
        0.0,
        simulator.getTerminationReason()});
    writeTrace(
        trace_path, rerun_index, dataset_path.filename().string(), events);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 7 || std::string(argv[1]) != "--dataset" ||
        std::string(argv[3]) != "--trace" ||
        std::string(argv[5]) != "--rerun-index") {
        std::cerr << "usage: legacy_stop_capture --dataset <path.csv> "
                     "--trace <path.json> --rerun-index <positive integer>\n";
        return 2;
    }

    try {
        runCapture(
            std::filesystem::path(argv[2]),
            std::filesystem::path(argv[4]),
            parseRerunIndex(argv[6]));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
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
    double time_s = 0.0;
    bool predicate_met = false;
    bool recorded_row_visible = false;
};

struct Row {
    double sample_time_s = 0.0;
    double altitude_m = 0.0;
    double vertical_velocity_mps = 0.0;
};

struct Timeline {
    std::vector<Event> events;
    std::vector<Row> rows;
    double final_time_s = 0.0;
    std::string reason_text;
};

struct ProbeResult {
    Timeline canonical;
    bool termination_before_record_rejected = false;
    bool missing_terminal_row_rejected = false;
    bool post_stop_advance_rejected = false;
    bool reason_text_change_accepted = false;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kTolerance;
}

Timeline canonicalTimeline() {
    return {
        {
            {"publish", {}, 0.0, false, false},
            {"record-field-read", "altitude_m", 0.0, false, false},
            {"record-field-read", "vertical_velocity_mps", 0.0, false, false},
            {"termination-evaluate", {}, 0.0, true, true},
            {"legacy-run-complete", {}, 0.0, false, false},
        },
        {{0.0, 1000.0, 10.0}},
        0.0,
        "display text is outside semantic comparison",
    };
}

bool validTimeline(const Timeline& timeline) {
    static const std::vector<std::string> expected_kinds{
        "publish",
        "record-field-read",
        "record-field-read",
        "termination-evaluate",
        "legacy-run-complete",
    };
    if (timeline.events.size() != expected_kinds.size()) {
        return false;
    }
    for (std::size_t index = 0; index < expected_kinds.size(); ++index) {
        if (timeline.events[index].kind != expected_kinds[index] ||
            !nearlyEqual(timeline.events[index].time_s, 0.0)) {
            return false;
        }
    }
    if (timeline.events[1].field_id != "altitude_m" ||
        timeline.events[2].field_id != "vertical_velocity_mps") {
        return false;
    }
    const Event& evaluation = timeline.events[3];
    if (!evaluation.predicate_met || !evaluation.recorded_row_visible) {
        return false;
    }
    if (timeline.rows.size() != 1 ||
        !nearlyEqual(timeline.rows[0].sample_time_s, 0.0) ||
        !nearlyEqual(timeline.rows[0].altitude_m, 1000.0) ||
        !nearlyEqual(timeline.rows[0].vertical_velocity_mps, 10.0) ||
        !nearlyEqual(timeline.final_time_s, 0.0)) {
        return false;
    }
    return true;
}

ProbeResult runProbe() {
    ProbeResult result;
    result.canonical = canonicalTimeline();

    Timeline termination_before_record = result.canonical;
    std::rotate(
        termination_before_record.events.begin() + 1,
        termination_before_record.events.begin() + 3,
        termination_before_record.events.begin() + 4);
    result.termination_before_record_rejected =
        !validTimeline(termination_before_record);

    Timeline missing_row = result.canonical;
    missing_row.rows.clear();
    missing_row.events[3].recorded_row_visible = false;
    result.missing_terminal_row_rejected = !validTimeline(missing_row);

    Timeline post_stop_advance = result.canonical;
    post_stop_advance.rows.push_back({1.0, 1010.0, 10.0});
    post_stop_advance.final_time_s = 1.0;
    result.post_stop_advance_rejected = !validTimeline(post_stop_advance);

    Timeline changed_reason = result.canonical;
    changed_reason.reason_text = "different display text";
    result.reason_text_change_accepted = validTimeline(changed_reason);
    return result;
}

void writeJson(const ProbeResult& result) {
    const Timeline& timeline = result.canonical;
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\",\"semantic_event_kinds\":[";
    for (std::size_t index = 0; index < timeline.events.size(); ++index) {
        if (index > 0) {
            std::cout << ',';
        }
        std::cout << '\"' << timeline.events[index].kind << '\"';
    }
    const Row& row = timeline.rows.front();
    std::cout << "]"
              << ",\"record_field_ids\":[\"altitude_m\","
                 "\"vertical_velocity_mps\"]"
              << ",\"terminal_row\":{\"sample_time_s\":"
              << row.sample_time_s
              << ",\"altitude_m\":" << row.altitude_m
              << ",\"vertical_velocity_mps\":"
              << row.vertical_velocity_mps << "}"
              << ",\"final_time_s\":" << timeline.final_time_s
              << ",\"termination_before_record_rejected\":"
              << (result.termination_before_record_rejected ? "true" : "false")
              << ",\"missing_terminal_row_rejected\":"
              << (result.missing_terminal_row_rejected ? "true" : "false")
              << ",\"post_stop_advance_rejected\":"
              << (result.post_stop_advance_rejected ? "true" : "false")
              << ",\"reason_text_change_accepted\":"
              << (result.reason_text_change_accepted ? "true" : "false")
              << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_legacy_stop_probe --self-check\n";
        return 2;
    }

    try {
        const ProbeResult result = runProbe();
        require(validTimeline(result.canonical),
                "canonical stop timeline differs");
        require(result.termination_before_record_rejected,
                "termination-before-record mutation was accepted");
        require(result.missing_terminal_row_rejected,
                "missing terminal row mutation was accepted");
        require(result.post_stop_advance_rejected,
                "post-stop advance mutation was accepted");
        require(result.reason_text_change_accepted,
                "free-text reason changed semantic comparison");
        writeJson(result);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

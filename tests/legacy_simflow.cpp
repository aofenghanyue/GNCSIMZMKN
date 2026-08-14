#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-SIMFLOW-07";
constexpr double kTolerance = 1.0e-12;

struct Mission {
    std::string vehicle_id;
    std::map<std::string, double> perturbation_inputs;
    std::string output_directory;
};

struct CaseRow {
    std::string source_case_id;
    std::map<std::string, double> values;
};

struct Command {
    std::string mode;
    std::string input_role;
    int exit_code = 1;
};

struct SemanticRow {
    double sample_time_s = 0.0;
    double altitude_m = 0.0;
    double vertical_velocity_mps = 0.0;
    double mass_kg = 0.0;
};

struct Replay {
    Mission mission;
    Command command;
    std::vector<SemanticRow> dataset;
    bool working_root_started_absent = false;
    bool effective_mission_standalone = false;
};

struct ProbeResult {
    std::string source_case_id;
    Mission effective_mission;
    Replay replay;
    bool missing_injected_input_rejected = false;
    bool hidden_replay_context_rejected = false;
    bool reused_batch_root_rejected = false;
    bool case_local_replay_input_rejected = false;
    bool replay_result_mismatch_rejected = false;
    bool input_declaration_order_accepted = false;
    bool case_source_column_order_accepted = false;
    bool dataset_column_order_accepted = false;
    bool legacy_case_directory_change_accepted = false;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kTolerance;
}

Mission materialize(const Mission& base,
                    const CaseRow& selected,
                    const std::vector<std::string>& requested_inputs,
                    const std::string& output_directory) {
    Mission result = base;
    for (const auto& input : requested_inputs) {
        const auto found = selected.values.find(input);
        require(found != selected.values.end(),
                "selected case is missing a requested numeric input");
        result.perturbation_inputs[input] = found->second;
    }
    result.output_directory = output_directory;
    return result;
}

bool sameSemanticMission(const Mission& lhs, const Mission& rhs) {
    if (lhs.vehicle_id != rhs.vehicle_id ||
        lhs.perturbation_inputs.size() != rhs.perturbation_inputs.size()) {
        return false;
    }
    for (const auto& [field, value] : lhs.perturbation_inputs) {
        const auto found = rhs.perturbation_inputs.find(field);
        if (found == rhs.perturbation_inputs.end() ||
            !nearlyEqual(value, found->second)) {
            return false;
        }
    }
    return true;
}

bool sameRows(const std::vector<SemanticRow>& lhs,
              const std::vector<SemanticRow>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!nearlyEqual(lhs[index].sample_time_s,
                         rhs[index].sample_time_s) ||
            !nearlyEqual(lhs[index].altitude_m, rhs[index].altitude_m) ||
            !nearlyEqual(lhs[index].vertical_velocity_mps,
                         rhs[index].vertical_velocity_mps) ||
            !nearlyEqual(lhs[index].mass_kg, rhs[index].mass_kg)) {
            return false;
        }
    }
    return true;
}

SemanticRow decodeSemanticRow(
    const std::vector<std::pair<std::string, double>>& encoded_fields) {
    std::map<std::string, double> by_field;
    for (const auto& [field, value] : encoded_fields) {
        require(by_field.emplace(field, value).second,
                "encoded dataset contains a duplicate semantic field");
    }
    require(by_field.size() == 4 &&
            by_field.count("sample_time_s") == 1 &&
            by_field.count("altitude_m") == 1 &&
            by_field.count("vertical_velocity_mps") == 1 &&
            by_field.count("mass_kg") == 1,
            "encoded dataset semantic fields differ");
    return {
        by_field.at("sample_time_s"),
        by_field.at("altitude_m"),
        by_field.at("vertical_velocity_mps"),
        by_field.at("mass_kg"),
    };
}

bool validOrdinaryReplay(const Mission& expected_mission,
                         const std::vector<SemanticRow>& expected_rows,
                         const Replay& replay) {
    return replay.command.mode == "--config" &&
        replay.command.input_role == "effective-mission" &&
        replay.command.exit_code == 0 &&
        replay.working_root_started_absent &&
        replay.effective_mission_standalone &&
        sameSemanticMission(expected_mission, replay.mission) &&
        sameRows(expected_rows, replay.dataset);
}

ProbeResult runProbe() {
    const Mission base{"vehicle", {}, "base-output"};
    const std::vector<CaseRow> matrix{
        {"hot", {{"engine.temp_level", 2.0},
                  {"aero.drag_bias", -0.03}}},
        {"cold", {{"engine.temp_level", 0.0},
                   {"aero.drag_bias", 0.02}}},
    };
    const std::vector<std::string> requested_inputs{
        "engine.temp_level", "aero.drag_bias"};
    const std::vector<SemanticRow> expected_rows{
        {0.0, 1000.0, 0.0, 100.0}};

    ProbeResult result;
    result.source_case_id = matrix.front().source_case_id;
    result.effective_mission = materialize(
        base, matrix.front(), requested_inputs, "case_000001");
    result.replay = {
        result.effective_mission,
        {"--config", "effective-mission", 0},
        expected_rows,
        true,
        true,
    };

    auto reordered_inputs = requested_inputs;
    std::reverse(reordered_inputs.begin(), reordered_inputs.end());
    const Mission reordered_mission = materialize(
        base, matrix.front(), reordered_inputs, "case_000001");
    result.input_declaration_order_accepted = sameSemanticMission(
        result.effective_mission, reordered_mission);

    CaseRow reordered_case{matrix.front().source_case_id, {}};
    const std::vector<std::pair<std::string, double>> reordered_fields{
        {"aero.drag_bias", -0.03},
        {"engine.temp_level", 2.0},
    };
    for (const auto& [field, value] : reordered_fields) {
        reordered_case.values[field] = value;
    }
    result.case_source_column_order_accepted = sameSemanticMission(
        result.effective_mission,
        materialize(base, reordered_case, requested_inputs, "case_000001"));

    const std::vector<std::pair<std::string, double>> reordered_dataset{
        {"mass_kg", 100.0},
        {"vertical_velocity_mps", 0.0},
        {"altitude_m", 1000.0},
        {"sample_time_s", 0.0},
    };
    result.dataset_column_order_accepted = sameRows(
        expected_rows, {decodeSemanticRow(reordered_dataset)});

    Mission missing_input = result.effective_mission;
    missing_input.perturbation_inputs.erase("aero.drag_bias");
    result.missing_injected_input_rejected =
        !sameSemanticMission(result.effective_mission, missing_input);

    Replay hidden_context = result.replay;
    hidden_context.command.mode = "--simflow";
    result.hidden_replay_context_rejected = !validOrdinaryReplay(
        result.effective_mission, expected_rows, hidden_context);

    Replay reused_batch_root = result.replay;
    reused_batch_root.working_root_started_absent = false;
    result.reused_batch_root_rejected = !validOrdinaryReplay(
        result.effective_mission, expected_rows, reused_batch_root);

    Replay case_local_input = result.replay;
    case_local_input.effective_mission_standalone = false;
    result.case_local_replay_input_rejected = !validOrdinaryReplay(
        result.effective_mission, expected_rows, case_local_input);

    Replay mismatched_result = result.replay;
    mismatched_result.dataset.front().altitude_m = 999.0;
    result.replay_result_mismatch_rejected = !validOrdinaryReplay(
        result.effective_mission, expected_rows, mismatched_result);

    Replay renamed_directory = result.replay;
    renamed_directory.mission.output_directory = "renamed-case-directory";
    result.legacy_case_directory_change_accepted = validOrdinaryReplay(
        result.effective_mission, expected_rows, renamed_directory);
    return result;
}

void writeJson(const ProbeResult& result) {
    const auto& inputs = result.effective_mission.perturbation_inputs;
    const auto& row = result.replay.dataset.front();
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\""
              << ",\"source_case_id\":\"" << result.source_case_id << "\""
              << ",\"injected_inputs\":{"
              << "\"aero.drag_bias\":" << inputs.at("aero.drag_bias")
              << ",\"engine.temp_level\":"
              << inputs.at("engine.temp_level") << "}"
              << ",\"ordinary_replay_mode\":\""
              << result.replay.command.mode << "\""
              << ",\"semantic_dataset_rows\":[{"
              << "\"sample_time_s\":" << row.sample_time_s
              << ",\"altitude_m\":" << row.altitude_m
              << ",\"vertical_velocity_mps\":"
              << row.vertical_velocity_mps
              << ",\"mass_kg\":" << row.mass_kg << "}]"
              << ",\"missing_injected_input_rejected\":"
              << (result.missing_injected_input_rejected ? "true" : "false")
              << ",\"hidden_replay_context_rejected\":"
              << (result.hidden_replay_context_rejected ? "true" : "false")
              << ",\"reused_batch_root_rejected\":"
              << (result.reused_batch_root_rejected ? "true" : "false")
              << ",\"case_local_replay_input_rejected\":"
              << (result.case_local_replay_input_rejected ? "true" : "false")
              << ",\"replay_result_mismatch_rejected\":"
              << (result.replay_result_mismatch_rejected ? "true" : "false")
              << ",\"input_declaration_order_accepted\":"
              << (result.input_declaration_order_accepted ?
                  "true" : "false")
              << ",\"case_source_column_order_accepted\":"
              << (result.case_source_column_order_accepted ?
                  "true" : "false")
              << ",\"dataset_column_order_accepted\":"
              << (result.dataset_column_order_accepted ?
                  "true" : "false")
              << ",\"legacy_case_directory_change_accepted\":"
              << (result.legacy_case_directory_change_accepted
                      ? "true" : "false")
              << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_legacy_simflow_probe --self-check\n";
        return 2;
    }

    try {
        const ProbeResult result = runProbe();
        require(result.source_case_id == "hot",
                "matrix row zero did not select hot");
        require(nearlyEqual(
                    result.effective_mission.perturbation_inputs.at(
                        "engine.temp_level"),
                    2.0) &&
                nearlyEqual(
                    result.effective_mission.perturbation_inputs.at(
                        "aero.drag_bias"),
                    -0.03),
                "effective mission inputs differ");
        require(result.missing_injected_input_rejected,
                "missing injected input was accepted");
        require(result.hidden_replay_context_rejected,
                "SimFlow-only replay context was accepted");
        require(result.reused_batch_root_rejected,
                "ordinary replay reused the batch working root");
        require(result.case_local_replay_input_rejected,
                "ordinary replay input remained case-directory local");
        require(result.replay_result_mismatch_rejected,
                "ordinary replay result mismatch was accepted");
        require(result.input_declaration_order_accepted,
                "input declaration order changed effective mission semantics");
        require(result.case_source_column_order_accepted,
                "case-source column order changed effective mission semantics");
        require(result.dataset_column_order_accepted,
                "dataset column order changed replay semantics");
        require(result.legacy_case_directory_change_accepted,
                "Legacy case directory changed semantic identity");
        writeJson(result);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

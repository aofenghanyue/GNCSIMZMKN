#!/usr/bin/env python3
"""Semantic materialization/replay reference for ORACLE-SIMFLOW-07."""

from __future__ import annotations

import argparse
import copy
import csv
from decimal import Decimal, getcontext
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import zipfile


ORACLE_ID = "ORACLE-SIMFLOW-07"
TRACE_SCHEMA = "gnczmkn.legacy-simflow-trace/1"
DATASET_COLUMNS = {
    "sample_time_s": "time",
    "altitude_m": "vehicle.dynamics.position.z",
    "vertical_velocity_mps": "vehicle.dynamics.velocity.z",
    "mass_kg": "vehicle.mass.mass_kg",
}


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = Decimal(str(value))
    require(result.is_finite(), f"Non-finite decimal value: {value}")
    return result


def verify_file_identity(record: dict, repo_root: Path, label: str) -> bytes:
    value = (repo_root / record["path"]).read_bytes()
    require(len(value) == record["bytes"], f"{label} byte count differs")
    require(sha256_bytes(value) == record["sha256"],
            f"{label} SHA-256 differs")
    return value


def verify_source(case: dict, repo_root: Path) -> int:
    source = case["legacy_source"]
    archive_record = source["archive"]
    archive_path = repo_root / archive_record["path"]
    archive_bytes = archive_path.read_bytes()
    require(len(archive_bytes) == archive_record["bytes"],
            "Legacy archive byte count differs")
    require(sha256_bytes(archive_bytes) == archive_record["sha256"],
            "Legacy archive SHA-256 differs")
    checks = 2

    with zipfile.ZipFile(archive_path, "r") as archive:
        for entry in source["entries"]:
            raw = archive.read(archive_record["prefix"] + entry["path"])
            require(len(raw) == entry["bytes"],
                    f"Legacy entry byte count differs: {entry['path']}")
            require(sha256_bytes(raw) == entry["sha256"],
                    f"Legacy entry SHA-256 differs: {entry['path']}")
            checks += 2

    evidence = verify_file_identity(
        source["runtime_test_evidence"], repo_root,
        "Legacy SimFlow CTest evidence")
    evidence_text = evidence.decode("utf-8-sig")
    for test in source["runtime_tests"]:
        marker = f"Test #{test['test_number']}: {test['id']}"
        require(any(marker in line and "Passed" in line
                    for line in evidence_text.splitlines()),
                f"Legacy CTest evidence is missing {test['id']}")
        checks += 2
    checks += 2
    return checks


def verify_capture(case: dict, repo_root: Path) -> tuple[
        int, list[dict], list[bytes], list[list[dict]], list[dict], list[dict]]:
    capture = case["legacy_capture"]
    require(capture["compiler"] == "w64devkit GCC 16.2.0" and
            "clean R0-LEG-001" in capture["executable_role"],
            "Legacy SimFlow executable provenance differs")
    require(capture["commands"] == [
        "gnc_sim.exe --simflow <generated-simflow.json>",
        "gnc_sim.exe --config <case-directory>/effective_mission.json",
    ], "Legacy SimFlow capture commands differ")
    checks = 3

    verify_file_identity(capture["environment_evidence"], repo_root,
                         "Legacy SimFlow environment evidence")
    verify_file_identity(capture["harness"], repo_root,
                         "Legacy SimFlow capture harness")
    checks += 4

    effective_missions = []
    effective_raw = []
    for index, record in enumerate(capture["effective_missions"], start=1):
        raw = verify_file_identity(
            record, repo_root, f"Legacy effective mission {index}")
        effective_raw.append(raw)
        effective_missions.append(json.loads(raw.decode("utf-8")))
        checks += 3
    require(effective_raw[0] == effective_raw[1],
            "Legacy effective mission reruns differ")
    checks += 1

    dataset_raw = []
    datasets = []
    for record in capture["datasets"]:
        stored = verify_file_identity(
            record, repo_root, f"Legacy dataset {record['role']}")
        require(record["stored_line_endings"] == "LF" and
                record["captured_line_endings"] == "CRLF" and
                b"\r" not in stored,
                "Legacy SimFlow dataset storage normalization differs")
        captured = stored.replace(b"\n", b"\r\n")
        require(len(captured) == record["captured_bytes"] and
                sha256_bytes(captured) == record["captured_sha256"],
                f"Raw dataset identity differs: {record['role']}")
        dataset_raw.append(stored)
        datasets.append(parse_dataset(stored))
        checks += 6
    require(all(raw == dataset_raw[0] for raw in dataset_raw[1:]),
            "SimFlow or ordinary replay dataset bytes differ")
    checks += 3

    summaries = []
    summary_raw = []
    for index, record in enumerate(capture["summaries"], start=1):
        raw = verify_file_identity(
            record, repo_root, f"Legacy SimFlow summary {index}")
        summary_raw.append(raw)
        summaries.append(parse_summary(raw))
        checks += 3
    require(summary_raw[0] == summary_raw[1],
            "Legacy SimFlow summary reruns differ")
    checks += 1

    traces = []
    for index, record in enumerate(capture["traces"], start=1):
        raw = verify_file_identity(
            record, repo_root, f"Legacy SimFlow trace {index}")
        trace = json.loads(raw.decode("utf-8"))
        require(trace["rerun_index"] == index,
                f"Legacy SimFlow trace {index} rerun identity differs")
        traces.append(trace)
        checks += 4
    return checks, effective_missions, dataset_raw, datasets, summaries, traces


def parse_case_source(raw: bytes) -> list[dict]:
    with io.StringIO(raw.decode("utf-8"), newline="") as stream:
        reader = csv.DictReader(stream)
        require(reader.fieldnames == [
            "case_id", "engine.temp_level", "aero.drag_bias"],
            "SimFlow matrix header differs")
        rows = list(reader)
    require(len(rows) == 2 and
            [row["case_id"] for row in rows] == ["hot", "cold"],
            "SimFlow matrix rows differ")
    return [{
        "source_case_id": row["case_id"],
        "values": {
            "engine.temp_level": decimal(row["engine.temp_level"]),
            "aero.drag_bias": decimal(row["aero.drag_bias"]),
        },
    } for row in rows]


def parse_dataset(raw: bytes) -> list[dict]:
    with io.StringIO(raw.decode("utf-8"), newline="") as stream:
        encoded = list(csv.reader(stream))
    require(len(encoded) >= 2, "SimFlow dataset has no data row")
    header = encoded[0]
    require(len(header) == len(set(header)),
            "SimFlow dataset contains duplicate headers")
    by_name = {name: index for index, name in enumerate(header)}
    require(all(column in by_name for column in DATASET_COLUMNS.values()),
            "SimFlow dataset is missing a semantic field")
    rows = []
    for encoded_row in encoded[1:]:
        require(len(encoded_row) == len(header),
                "SimFlow dataset row width differs")
        rows.append({
            field: decimal(encoded_row[by_name[column]])
            for field, column in DATASET_COLUMNS.items()
        })
    return rows


def parse_summary(raw: bytes) -> dict:
    with io.StringIO(raw.decode("utf-8"), newline="") as stream:
        rows = list(csv.DictReader(stream))
    require(len(rows) == 1, "SimFlow summary must contain one row")
    row = rows[0]
    require(row["case_id"] == "hot" and
            row["status"] == "succeeded" and
            row["exit_code"] == "0" and
            bool(row["case_directory"]),
            "SimFlow summary semantic result differs")
    return row


def normalized_mission(mission: dict) -> dict:
    result = copy.deepcopy(mission)
    require(isinstance(result.get("outputs"), dict),
            "Effective mission outputs are missing")
    result["outputs"].pop("directory", None)
    return result


def materialize_independently(base: dict, selected: dict,
                              vehicle_id: str,
                              requested_inputs: list[str]) -> dict:
    mission = copy.deepcopy(base)
    vehicles = [vehicle for vehicle in mission["vehicles"]
                if vehicle.get("id") == vehicle_id]
    require(len(vehicles) == 1,
            "Base mission does not contain exactly one selected vehicle")
    values = selected["values"]
    require(all(name in values for name in requested_inputs),
            "Selected matrix row is missing a requested input")
    injected = vehicles[0]["perturbation"]["config"]["inputs"]
    require(isinstance(injected, dict) and len(injected) == 0,
            "Base mission perturbation inputs must start empty")
    for name in requested_inputs:
        value = values[name]
        injected[name] = int(value) if value == value.to_integral() else float(value)
    return mission


def validate_mission(actual: dict, expected: dict, label: str) -> int:
    require(normalized_mission(actual) == normalized_mission(expected),
            f"{label} semantic effective mission differs")
    return 1


def reference_rows(oracle: dict) -> list[dict]:
    return [{field: decimal(row[field]) for field in DATASET_COLUMNS}
            for row in oracle["expected"]["semantic_dataset_rows"]]


def validate_rows(actual: list[dict], expected: list[dict],
                  tolerance: Decimal, label: str) -> int:
    require(len(actual) == len(expected), f"{label} row count differs")
    checks = 1
    for row_index, target in enumerate(expected):
        row = actual[row_index]
        require(set(row) == set(DATASET_COLUMNS),
                f"{label} row fields differ")
        for field in DATASET_COLUMNS:
            error = abs(decimal(row[field]) - target[field])
            require(error <= tolerance,
                    f"{label} {field} error {error} exceeds {tolerance}")
            checks += 1
    return checks


def validate_trace(trace: dict, expected_commands: list[dict],
                   expected_isolation: dict, label: str) -> int:
    require(trace["schema_version"] == TRACE_SCHEMA and
            trace["oracle_id"] == ORACLE_ID,
            f"{label} identity differs")
    commands = trace["commands"]
    require(len(commands) == len(expected_commands),
            f"{label} command count differs")
    for sequence, expected in enumerate(expected_commands):
        actual = commands[sequence]
        require(actual["sequence"] == sequence and
                actual["entrypoint"] == "gnc_sim" and
                actual["mode"] == expected["mode"] and
                actual["input_role"] == expected["input_role"] and
                actual["exit_code"] == expected["exit_code"],
                f"{label} command {sequence} differs")
    materialization = trace["materialization"]
    require(materialization["source_case_id"] == "hot" and
            materialization["case_manifest_present"] is False,
            f"{label} materialization result differs")
    lineage = trace["lineage_checks"]
    require(lineage["simflow_output_root_started_absent"] is True,
            f"{label} SimFlow output root was reused")
    observed_isolation = {
        "working_root_started_absent":
            lineage["ordinary_replay_root_started_absent"],
        "effective_mission_copied_outside_case_directory":
            lineage["effective_mission_copied_outside_case_directory"],
        "dataset_path_started_absent":
            lineage["ordinary_replay_dataset_path_started_absent"],
    }
    require(observed_isolation == expected_isolation,
            f"{label} ordinary replay isolation differs")
    return 12


def rejected(function) -> bool:
    try:
        function()
    except (KeyError, ValueError):
        return True
    return False


def run_probe(path: Path) -> tuple[str, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    require(completed.returncode == 0,
            f"C++ SimFlow probe failed: {completed.stderr.strip()}")
    return completed.stdout, json.loads(completed.stdout)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--oracle", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--repo-root", required=True, type=Path)
    arguments = parser.parse_args()

    getcontext().prec = 50
    input_bytes = arguments.input.read_bytes()
    case = json.loads(input_bytes.decode("utf-8"))
    oracle = json.loads(arguments.oracle.read_text(encoding="utf-8"))
    require(case["oracle_id"] == ORACLE_ID and
            oracle["oracle_id"] == ORACLE_ID,
            "Input or reference belongs to a different oracle")
    require(case["fixture_id"] == oracle["fixture_id"] and
            case["case_id"] == oracle["case_id"],
            "Input and reference identities differ")
    require(len(input_bytes) == oracle["input_identity"]["bytes"] and
            sha256_bytes(input_bytes) == oracle["input_identity"]["sha256"],
            "Input byte identity differs from the reference")
    checks = 4
    repo_root = arguments.repo_root.resolve()

    checks += verify_source(case, repo_root)
    input_records = case["inputs"]
    base_raw = verify_file_identity(
        input_records["base_mission"], repo_root, "SimFlow base mission")
    case_raw = verify_file_identity(
        input_records["case_source"], repo_root, "SimFlow case source")
    template_raw = verify_file_identity(
        input_records["simflow_template"], repo_root, "SimFlow template")
    checks += 6

    base = json.loads(base_raw.decode("utf-8"))
    matrix = parse_case_source(case_raw)
    template = json.loads(template_raw.decode("utf-8"))
    selected_record = input_records["selected_case"]
    selected = matrix[selected_record["source_row"]]
    require(selected["source_case_id"] == selected_record["source_case_id"] ==
            oracle["expected"]["source_case_id"],
            "Selected SimFlow source case differs")
    requested_inputs = template["materializer"]["config"]["vehicles"][
        selected_record["vehicle_id"]]["inputs"]
    require(template["materializer"]["config"]["case_source"]["rows"] == [0] and
            requested_inputs == ["engine.temp_level", "aero.drag_bias"],
            "SimFlow template selection differs")
    for field, value in selected_record["numeric_inputs"].items():
        require(selected["values"][field] == decimal(value),
                f"Selected SimFlow input differs: {field}")
    checks += 8

    expected_mission = materialize_independently(
        base, selected, selected_record["vehicle_id"], requested_inputs)
    capture_checks, effective_missions, _, datasets, summaries, traces = (
        verify_capture(case, repo_root))
    checks += capture_checks
    for index, mission in enumerate(effective_missions, start=1):
        checks += validate_mission(
            mission, expected_mission, f"Legacy effective mission {index}")

    expected_rows = reference_rows(oracle)
    tolerance = decimal(oracle["tolerances"]["legacy_seed_scalar_absolute"])
    for index, rows in enumerate(datasets, start=1):
        checks += validate_rows(
            rows, expected_rows, tolerance, f"Legacy dataset {index}")
    require(summaries[0]["case_id"] == summaries[1]["case_id"] == "hot",
            "Legacy SimFlow summary source case differs between reruns")
    checks += 2

    expected_commands = oracle["expected"]["command_sequence"]
    expected_isolation = oracle["expected"]["ordinary_replay_isolation"]
    for index, trace in enumerate(traces, start=1):
        checks += validate_trace(
            trace, expected_commands, expected_isolation,
            f"Legacy SimFlow trace {index}")

    failure_by_id = {entry["id"]: entry
                     for entry in oracle["failure_cases"]}
    require(set(failure_by_id) == {
        "FAIL-SIMFLOW-MISSING-INJECTED-INPUT",
        "FAIL-SIMFLOW-HIDDEN-REPLAY-CONTEXT",
        "FAIL-SIMFLOW-REPLAY-REUSES-BATCH-ROOT",
        "FAIL-SIMFLOW-CASE-LOCAL-REPLAY-INPUT",
        "FAIL-SIMFLOW-REPLAY-RESULT-MISMATCH",
    } and all(entry["expected_status"] == "rejected"
              for entry in failure_by_id.values()),
            "SimFlow failure-case definitions differ")

    missing_input = copy.deepcopy(effective_missions[0])
    missing_input["vehicles"][0]["perturbation"]["config"]["inputs"].pop(
        "aero.drag_bias")
    hidden_context = copy.deepcopy(traces[0])
    hidden_context["commands"][1]["mode"] = "--simflow"
    reused_batch_root = copy.deepcopy(traces[0])
    reused_batch_root["lineage_checks"][
        "ordinary_replay_root_started_absent"] = False
    case_local_replay_input = copy.deepcopy(traces[0])
    case_local_replay_input["lineage_checks"][
        "effective_mission_copied_outside_case_directory"] = False
    mismatched_rows = copy.deepcopy(datasets[1])
    mismatched_rows[0]["altitude_m"] = Decimal("999")
    require(rejected(lambda: validate_mission(
                missing_input, expected_mission,
                "Missing-input mutation")) and
            rejected(lambda: validate_trace(
                hidden_context, expected_commands, expected_isolation,
                "Hidden-context mutation")) and
            rejected(lambda: validate_trace(
                reused_batch_root, expected_commands, expected_isolation,
                "Reused-batch-root mutation")) and
            rejected(lambda: validate_trace(
                case_local_replay_input, expected_commands,
                expected_isolation,
                "Case-local-replay-input mutation")) and
            rejected(lambda: validate_rows(
                mismatched_rows, expected_rows, tolerance,
                "Replay-result mutation")),
            "A SimFlow semantic failure mutation was accepted")
    checks += 6

    equivalence = oracle["equivalence_cases"]
    require(len(equivalence) == 1 and
            equivalence[0]["id"] ==
            "PASS-SIMFLOW-LEGACY-CASE-DIRECTORY-RENAMED" and
            equivalence[0]["expected_status"] == "accepted",
            "SimFlow equivalence-case definition differs")
    renamed_trace = copy.deepcopy(traces[0])
    renamed_trace["materialization"]["legacy_case_index"] = 99
    renamed_trace["materialization"]["legacy_case_directory_name"] = "renamed"
    renamed_mission = copy.deepcopy(effective_missions[0])
    renamed_mission["outputs"]["directory"] = "renamed-output"
    validate_trace(renamed_trace, expected_commands, expected_isolation,
                   "Legacy identity equivalence")
    validate_mission(renamed_mission, expected_mission,
                     "Output-directory equivalence")
    checks += 4

    first_stdout, probe = run_probe(arguments.probe)
    second_stdout, second_probe = run_probe(arguments.probe)
    require(first_stdout == second_stdout and probe == second_probe,
            "C++ SimFlow probe reruns differ")
    require(probe["oracle_id"] == ORACLE_ID and
            probe["status"] == "passed" and
            probe["source_case_id"] == "hot" and
            probe["ordinary_replay_mode"] == "--config",
            "C++ SimFlow probe identity or replay mode differs")
    for field, expected in selected["values"].items():
        error = abs(decimal(probe["injected_inputs"][field]) - expected)
        require(error <= tolerance,
                f"C++ SimFlow injected input differs: {field}")
    probe_rows = [{field: decimal(row[field]) for field in DATASET_COLUMNS}
                  for row in probe["semantic_dataset_rows"]]
    checks += validate_rows(
        probe_rows, expected_rows, tolerance, "C++ SimFlow probe")
    for flag in (
            "missing_injected_input_rejected",
            "hidden_replay_context_rejected",
            "reused_batch_root_rejected",
            "case_local_replay_input_rejected",
            "replay_result_mismatch_rejected",
            "legacy_case_directory_change_accepted"):
        require(probe[flag] is True,
                f"C++ SimFlow probe did not enforce {flag}")
    checks += 12

    decision = oracle["disposition_decision"]
    require(decision["status"] in {"needs_owner_decision", "accepted"},
            "Disposition decision has an unsupported status")
    disposition_field = ("disposition" if decision["status"] == "accepted"
                         else "recommended_disposition")
    dispositions = {entry[disposition_field] for entry in decision["facts"]}
    require(dispositions == {"Preserve", "Retire"},
            "Disposition must separate Preserve and Retire")
    checks += 2

    print(json.dumps({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks,
        "input_sha256": sha256_bytes(input_bytes),
        "source_case_id": selected["source_case_id"],
        "injected_inputs": {
            field: str(value) for field, value in selected["values"].items()
        },
        "legacy_reruns_byte_identical": True,
        "ordinary_replay_dataset_equal": True,
        "ordinary_replay_mode": "--config",
        "ordinary_replay_fresh_root": True,
        "effective_mission_standalone": True,
        "deterministic_target_case_id": "pending",
        "disposition_status": decision["status"],
    }, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (IndexError, KeyError, OSError, ValueError, zipfile.BadZipFile,
            json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"legacy SimFlow reference failed: {error}", file=sys.stderr)
        raise SystemExit(1)

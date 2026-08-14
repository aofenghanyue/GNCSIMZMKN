#!/usr/bin/env python3
"""Record-before-termination reference for ORACLE-YYZ-STOP-06."""

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


ORACLE_ID = "ORACLE-YYZ-STOP-06"
TRACE_SCHEMA = "gnczmkn.legacy-stop-trace/1"
EXPECTED_EVENT_KINDS = [
    "publish",
    "record-field-read",
    "record-field-read",
    "termination-evaluate",
    "legacy-run-complete",
]
EXPECTED_HEADERS = [
    "time",
    "vehicle.state.altitude_m",
    "vehicle.state.vertical_velocity_mps",
]


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


def verify_capture_identity(case: dict, repo_root: Path) -> tuple[int, list[bytes], list[dict]]:
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
            value = archive.read(archive_record["prefix"] + entry["path"])
            require(len(value) == entry["bytes"],
                    f"Legacy entry byte count differs: {entry['path']}")
            require(sha256_bytes(value) == entry["sha256"],
                    f"Legacy entry SHA-256 differs: {entry['path']}")
            checks += 2

    test_record = source["runtime_test"]
    evidence = verify_file_identity({
        "path": test_record["evidence_path"],
        "bytes": test_record["evidence_bytes"],
        "sha256": test_record["evidence_sha256"],
    }, repo_root, "Legacy CTest evidence")
    evidence_text = evidence.decode("utf-8-sig")
    require(any("Test #11: test_publish_semantics" in line and
                "Passed" in line for line in evidence_text.splitlines()),
            "Legacy CTest evidence does not contain the passing stop test")
    checks += 3

    capture = case["legacy_capture"]
    require(capture["compiler"] == "w64devkit GCC 16.2.0" and
            capture["language_standard"] == "C++17",
            "Legacy STOP capture compiler identity differs")
    require("-std=c++17" in capture["compile_command"] and
            "-O2" in capture["compile_command"] and
            "legacy_stop_capture.exe" in capture["compile_command"],
            "Legacy STOP capture compile command differs")
    require(len(capture["run_commands"]) == 2 and
            "--rerun-index 1" in capture["run_commands"][0] and
            "--rerun-index 2" in capture["run_commands"][1],
            "Legacy STOP capture run commands differ")
    checks += 3

    verify_file_identity(capture["environment_evidence"], repo_root,
                         "Legacy capture environment evidence")
    verify_file_identity(capture["harness"], repo_root,
                         "Legacy STOP capture harness")
    checks += 4

    datasets = []
    for index, dataset_record in enumerate(capture["datasets"], start=1):
        stored = verify_file_identity(dataset_record, repo_root,
                                      f"Legacy STOP dataset {index}")
        require(dataset_record["stored_line_endings"] == "LF" and
                dataset_record["captured_line_endings"] == "CRLF" and
                b"\r" not in stored,
                "Legacy STOP dataset storage normalization differs")
        captured = stored.replace(b"\n", b"\r\n")
        require(len(captured) == dataset_record["captured_bytes"] and
                sha256_bytes(captured) == dataset_record["captured_sha256"],
                "Reconstructed raw Legacy STOP dataset identity differs")
        datasets.append(stored)
        checks += 5

    traces = []
    for index, trace_record in enumerate(capture["traces"], start=1):
        raw = verify_file_identity(trace_record, repo_root,
                                   f"Legacy STOP trace {index}")
        trace = json.loads(raw.decode("utf-8"))
        require(trace["rerun_index"] == index and
                trace["dataset_filename"] == f"legacy-run-{index}.csv",
                f"Legacy STOP trace {index} rerun identity differs")
        traces.append(trace)
        checks += 4
    return checks, datasets, traces


def parse_dataset(raw: bytes) -> list[dict]:
    reader = csv.reader(io.StringIO(raw.decode("utf-8"), newline=""))
    encoded = list(reader)
    require(len(encoded) >= 1, "Legacy STOP dataset is empty")
    require(encoded[0] == EXPECTED_HEADERS,
            "Legacy STOP dataset header differs")
    result = []
    for row in encoded[1:]:
        require(len(row) == 3, "Legacy STOP dataset row width differs")
        result.append({
            "sample_time_s": decimal(row[0]),
            "altitude_m": decimal(row[1]),
            "vertical_velocity_mps": decimal(row[2]),
        })
    return result


def normalized_trace(trace: dict, normalization: dict) -> dict:
    result = copy.deepcopy(trace)
    for field in normalization["excluded_top_level_fields"]:
        result.pop(field, None)
    for event in result["events"]:
        for field in normalization["excluded_event_fields"]:
            event.pop(field, None)
    return result


def compare_decimal(actual: object, expected: Decimal,
                    tolerance: Decimal, label: str) -> None:
    error = abs(decimal(actual) - expected)
    require(error <= tolerance,
            f"{label} error {error} exceeds tolerance {tolerance}")


def expected_values(case: dict, oracle: dict) -> dict:
    run = case["run"]
    initial = case["initial_state"]
    termination = case["termination"]
    expected = oracle["expected"]
    expected_row = expected["terminal_dataset"]
    result = {
        "sample_time_s": decimal(run["initial_time_s"]),
        "altitude_m": decimal(initial["altitude_m"]),
        "vertical_velocity_mps": decimal(initial["vertical_velocity_mps"]),
        "threshold_m": decimal(termination["threshold_m"]),
        "final_time_s": decimal(run["initial_time_s"]),
    }
    require(run["record_initial_state"] is True and
            run["flush_every_step"] is True and
            decimal(run["step_s"]) > 0 and
            decimal(run["duration_s"]) >= decimal(run["step_s"]),
            "STOP run definition differs")
    require(termination["relation"] == ">=" and
            result["altitude_m"] >= result["threshold_m"],
            "STOP predicate is not satisfied at t0")
    require(expected["event_kinds"] == EXPECTED_EVENT_KINDS and
            expected["record_field_ids"] ==
            ["altitude_m", "vertical_velocity_mps"] and
            expected_row["row_count"] == 1 and
            decimal(expected_row["sample_time_s"]) ==
            result["sample_time_s"] and
            decimal(expected_row["altitude_m"]) == result["altitude_m"] and
            decimal(expected_row["vertical_velocity_mps"]) ==
            result["vertical_velocity_mps"],
            "STOP reference values differ from the independent initial state")
    return result


def validate_semantics(trace: dict, rows: list[dict], values: dict,
                       time_tolerance: Decimal,
                       state_tolerance: Decimal,
                       label: str) -> int:
    require(trace["schema_version"] == TRACE_SCHEMA and
            trace["oracle_id"] == ORACLE_ID,
            f"{label} trace identity differs")
    events = trace["events"]
    require([event["event_kind"] for event in events] ==
            EXPECTED_EVENT_KINDS,
            f"{label} event order differs")
    require([event["sequence"] for event in events] == list(range(5)),
            f"{label} event sequence differs")

    publish, altitude_read, velocity_read, evaluation, completion = events
    require(publish["step"] == 0 and
            altitude_read["step"] == 0 and
            velocity_read["step"] == 0 and
            evaluation["step"] == 0,
            f"{label} step differs")
    compare_decimal(publish["time_s"], values["sample_time_s"],
                    time_tolerance, f"{label} publish time")
    compare_decimal(publish["altitude_m"], values["altitude_m"],
                    state_tolerance, f"{label} published altitude")
    compare_decimal(publish["vertical_velocity_mps"],
                    values["vertical_velocity_mps"], state_tolerance,
                    f"{label} published velocity")
    require(altitude_read["field_id"] == "altitude_m" and
            velocity_read["field_id"] == "vertical_velocity_mps",
            f"{label} record field identity differs")
    compare_decimal(altitude_read["sample_time_s"],
                    values["sample_time_s"], time_tolerance,
                    f"{label} altitude read time")
    compare_decimal(altitude_read["value"], values["altitude_m"],
                    state_tolerance, f"{label} recorded altitude")
    compare_decimal(velocity_read["sample_time_s"],
                    values["sample_time_s"], time_tolerance,
                    f"{label} velocity read time")
    compare_decimal(velocity_read["value"],
                    values["vertical_velocity_mps"], state_tolerance,
                    f"{label} recorded velocity")

    require(evaluation["predicate_id"] ==
            "altitude-at-or-above-1000m" and
            evaluation["predicate_met"] is True and
            evaluation["recorded_row_visible"] is True,
            f"{label} termination evaluation differs")
    compare_decimal(evaluation["time_s"], values["sample_time_s"],
                    time_tolerance, f"{label} evaluation time")
    compare_decimal(evaluation["recorded_time_s"],
                    values["sample_time_s"], time_tolerance,
                    f"{label} visible row time")
    compare_decimal(evaluation["recorded_altitude_m"],
                    values["altitude_m"], state_tolerance,
                    f"{label} visible row altitude")
    compare_decimal(evaluation["recorded_vertical_velocity_mps"],
                    values["vertical_velocity_mps"], state_tolerance,
                    f"{label} visible row velocity")

    require(len(rows) == 1, f"{label} terminal dataset row count differs")
    row = rows[0]
    compare_decimal(row["sample_time_s"], values["sample_time_s"],
                    time_tolerance, f"{label} dataset time")
    compare_decimal(row["altitude_m"], values["altitude_m"],
                    state_tolerance, f"{label} dataset altitude")
    compare_decimal(row["vertical_velocity_mps"],
                    values["vertical_velocity_mps"], state_tolerance,
                    f"{label} dataset velocity")
    require(decimal(evaluation["recorded_time_s"]) ==
            row["sample_time_s"] and
            decimal(evaluation["recorded_altitude_m"]) ==
            row["altitude_m"] and
            decimal(evaluation["recorded_vertical_velocity_mps"]) ==
            row["vertical_velocity_mps"],
            f"{label} evaluator-visible row differs from stored row")

    compare_decimal(completion["final_time_s"], values["final_time_s"],
                    time_tolerance, f"{label} final time")
    require(isinstance(completion.get("termination_reason_text"), str),
            f"{label} Legacy reason text shape differs")
    return 25


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
            f"C++ STOP probe failed: {completed.stderr.strip()}")
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
            sha256_bytes(input_bytes) ==
            oracle["input_identity"]["sha256"],
            "Input byte identity differs from the reference")
    checks = 4

    repo_root = arguments.repo_root.resolve()
    capture_checks, dataset_bytes, traces = verify_capture_identity(
        case, repo_root)
    checks += capture_checks
    require(len(dataset_bytes) == 2 and
            dataset_bytes[0] == dataset_bytes[1],
            "Legacy STOP dataset reruns differ")
    normalization = case["legacy_capture"]["semantic_normalization"]
    normalized = [normalized_trace(trace, normalization) for trace in traces]
    require(normalized[0] == normalized[1],
            "Legacy STOP semantic trace reruns differ")
    checks += 3

    values = expected_values(case, oracle)
    time_tolerance = decimal(
        oracle["tolerances"]["legacy_seed_time_absolute"])
    state_tolerance = decimal(
        oracle["tolerances"]["legacy_seed_state_absolute"])
    rows = [parse_dataset(raw) for raw in dataset_bytes]
    for index, (trace, dataset) in enumerate(zip(traces, rows), start=1):
        checks += validate_semantics(
            trace, dataset, values, time_tolerance, state_tolerance,
            f"Legacy STOP run {index}")

    failure_by_id = {entry["id"]: entry
                     for entry in oracle["failure_cases"]}
    require(set(failure_by_id) == {
        "FAIL-STOP-TERMINATION-BEFORE-RECORD",
        "FAIL-STOP-MISSING-TERMINAL-ROW",
        "FAIL-STOP-POST-STOP-ADVANCE",
    } and all(entry["expected_status"] == "rejected"
              for entry in failure_by_id.values()),
            "STOP failure-case definitions differ")

    termination_before_record = copy.deepcopy(traces[0])
    events = termination_before_record["events"]
    termination_before_record["events"] = [
        events[0], events[3], events[1], events[2], events[4],
    ]
    for sequence, event in enumerate(termination_before_record["events"]):
        event["sequence"] = sequence

    missing_row = copy.deepcopy(traces[0])
    missing_row["events"][3]["recorded_row_visible"] = False

    post_stop_advance = copy.deepcopy(traces[0])
    post_stop_advance["events"][4]["final_time_s"] = 1
    extra_rows = rows[0] + [{
        "sample_time_s": Decimal("1"),
        "altitude_m": Decimal("1010"),
        "vertical_velocity_mps": Decimal("10"),
    }]
    require(rejected(lambda: validate_semantics(
                termination_before_record, rows[0], values,
                time_tolerance, state_tolerance,
                "Termination-before-record mutation")) and
            rejected(lambda: validate_semantics(
                missing_row, [], values, time_tolerance, state_tolerance,
                "Missing-terminal-row mutation")) and
            rejected(lambda: validate_semantics(
                post_stop_advance, extra_rows, values,
                time_tolerance, state_tolerance,
                "Post-stop-advance mutation")),
            "A STOP semantic failure mutation was accepted")
    checks += 4

    equivalence = oracle["equivalence_cases"]
    require(equivalence == [{
        "id": "PASS-STOP-REASON-TEXT-CHANGED",
        "mutation": "Replace the Legacy free-text reason while keeping the semantic timeline unchanged",
        "expected_status": "accepted",
    }], "STOP equivalence-case definition differs")
    changed_reason = copy.deepcopy(traces[0])
    changed_reason["events"][4]["termination_reason_text"] = (
        "different display text")
    validate_semantics(changed_reason, rows[0], values,
                       time_tolerance, state_tolerance,
                       "Reason-text equivalence")
    require(normalized_trace(changed_reason, normalization) == normalized[0],
            "Legacy free-text reason changed semantic normalization")
    checks += 3

    first_stdout, probe = run_probe(arguments.probe)
    second_stdout, second_probe = run_probe(arguments.probe)
    require(first_stdout == second_stdout and probe == second_probe,
            "C++ STOP probe reruns differ")
    require(probe["oracle_id"] == ORACLE_ID and
            probe["status"] == "passed" and
            probe["semantic_event_kinds"] == EXPECTED_EVENT_KINDS and
            probe["record_field_ids"] ==
            ["altitude_m", "vertical_velocity_mps"],
            "C++ STOP probe returned an unexpected identity or timeline")
    probe_row = probe["terminal_row"]
    compare_decimal(probe_row["sample_time_s"], values["sample_time_s"],
                    time_tolerance, "C++ terminal row time")
    compare_decimal(probe_row["altitude_m"], values["altitude_m"],
                    state_tolerance, "C++ terminal row altitude")
    compare_decimal(probe_row["vertical_velocity_mps"],
                    values["vertical_velocity_mps"], state_tolerance,
                    "C++ terminal row velocity")
    compare_decimal(probe["final_time_s"], values["final_time_s"],
                    time_tolerance, "C++ final time")
    for flag in (
            "termination_before_record_rejected",
            "missing_terminal_row_rejected",
            "post_stop_advance_rejected",
            "reason_text_change_accepted"):
        require(probe[flag] is True, f"C++ STOP probe did not enforce {flag}")
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
        "legacy_dataset_reruns_byte_identical": True,
        "legacy_trace_reruns_semantically_identical": True,
        "terminal_row": {
            "sample_time_s": str(values["sample_time_s"]),
            "altitude_m": str(values["altitude_m"]),
            "vertical_velocity_mps": str(
                values["vertical_velocity_mps"]),
        },
        "final_time_s": str(values["final_time_s"]),
        "reason_text_ignored": True,
        "disposition_status": decision["status"],
    }, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError, zipfile.BadZipFile,
            json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"legacy STOP reference failed: {error}", file=sys.stderr)
        raise SystemExit(1)

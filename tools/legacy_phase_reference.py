#!/usr/bin/env python3
"""Legacy capture and independent comparator for ORACLE-YYZ-PHASE-02."""

from __future__ import annotations

import argparse
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile


ORACLE_ID = "ORACLE-YYZ-PHASE-02"
TRACE_SCHEMA = "gnczmkn.legacy-phase-trace/1"


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def verify_file_identity(record: dict, repo_root: Path, label: str) -> bytes:
    value = (repo_root / record["path"]).read_bytes()
    require(len(value) == record["bytes"], f"{label} byte count differs")
    require(sha256_bytes(value) == record["sha256"],
            f"{label} SHA-256 differs")
    return value


def verify_capture_identity(case: dict, repo_root: Path) -> tuple[int, list[dict]]:
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
            "Legacy CTest evidence does not contain the passing runtime test")
    checks += 3

    capture = case["legacy_capture"]
    require(capture["compiler"] == "w64devkit GCC 16.2.0" and
            capture["language_standard"] == "C++17",
            "Legacy capture compiler identity differs")
    require("-std=c++17" in capture["compile_command"] and
            "-O2" in capture["compile_command"],
            "Legacy capture compile command differs")
    require(len(capture["run_commands"]) == 2 and
            "--rerun-index 1" in capture["run_commands"][0] and
            "--rerun-index 2" in capture["run_commands"][1],
            "Legacy capture run commands differ")
    checks += 3
    verify_file_identity(capture["environment_evidence"], repo_root,
                         "Legacy capture environment evidence")
    verify_file_identity(capture["harness"], repo_root,
                         "Legacy capture harness")
    checks += 4

    traces = []
    for index, trace_record in enumerate(capture["traces"], start=1):
        raw = verify_file_identity(trace_record, repo_root,
                                   f"Legacy trace {index}")
        traces.append(json.loads(raw.decode("utf-8")))
        checks += 2
    return checks, traces


def normalize_trace(trace: dict, expected: dict, rerun_index: int) -> dict:
    require(trace["schema_version"] == TRACE_SCHEMA,
            "Legacy trace schema differs")
    require(trace["oracle_id"] == ORACLE_ID,
            "Legacy trace belongs to a different oracle")
    require(trace["rerun_index"] == rerun_index,
            "Legacy trace rerun index differs")

    phases = expected["phases"]
    events = trace["events"]
    require(len(events) == expected["event_count"] == len(phases),
            "Legacy trace event count differs")
    for index, (event, phase) in enumerate(zip(events, phases)):
        expected_event = {
            "sequence": index,
            "event_kind": expected["event_kind"],
            "phase": phase,
            "probe_id": expected["probe_id_template"].format(phase=phase),
            "step": expected["step"],
        }
        require({key: event[key] for key in expected_event} == expected_event,
                f"Legacy trace event differs at sequence {index}")
        require(Decimal(str(event["time_s"])) ==
                Decimal(expected["time_s"]),
                f"Legacy trace time differs at sequence {index}")

    normalization = expected["normalization"]
    require(normalization["excluded_top_level_fields"] == ["rerun_index"] and
            normalization["excluded_event_fields"] == [],
            "Trace normalization excludes unsupported fields")
    return {
        "schema_version": trace["schema_version"],
        "oracle_id": trace["oracle_id"],
        "events": events,
    }


def validate_phase_list(phases: list[str], expected: list[str]) -> bool:
    return phases == expected


def run_probe(path: Path) -> tuple[str, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    require(completed.returncode == 0,
            f"C++ phase probe failed: {completed.stderr.strip()}")
    return completed.stdout, json.loads(completed.stdout)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--oracle", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--repo-root", required=True, type=Path)
    arguments = parser.parse_args()

    input_bytes = arguments.input.read_bytes()
    case = json.loads(input_bytes.decode("utf-8"))
    oracle = read_json(arguments.oracle)
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

    capture_checks, traces = verify_capture_identity(
        case, arguments.repo_root.resolve())
    checks += capture_checks
    require(len(traces) == 2, "Phase capture requires exactly two raw runs")

    expected = oracle["expected"]
    normalized_first = normalize_trace(traces[0], expected, 1)
    normalized_second = normalize_trace(traces[1], expected, 2)
    require(normalized_first == normalized_second,
            "Normalized Legacy phase trace reruns differ")
    checks += 4 + 2 * expected["event_count"]

    registrations = case["registrations"]
    require(len(registrations) == expected["event_count"],
            "Phase registration count differs")
    require({entry["phase"] for entry in registrations} ==
            set(expected["phases"]),
            "Phase registration set differs")
    require({entry["probe_id"] for entry in registrations} ==
            {expected["probe_id_template"].format(phase=phase)
             for phase in expected["phases"]},
            "Phase probe identity set differs")
    require(sorted(entry["registration_order"] for entry in registrations) ==
            list(range(expected["event_count"])),
            "Registration order is not contiguous")

    registration_order = [entry["phase"] for entry in sorted(
        registrations, key=lambda item: item["registration_order"])]
    priority_order = [entry["phase"] for entry in sorted(
        registrations,
        key=lambda item: (item["priority"], item["registration_order"]))]
    observation = oracle["legacy_observation"]
    require(registration_order == observation["registration_order"] and
            priority_order == observation["priority_order"],
            "Reference registration or priority order differs from input")
    require(observation["observed_order"] == expected["phases"],
            "Reference observed order differs from expected phases")
    require(registration_order != expected["phases"] and
            priority_order != expected["phases"],
            "Capture input does not distinguish macro phase ordering")
    checks += 7

    for failure in oracle["failure_cases"]:
        require(failure["expected_status"] == "rejected" and
                not validate_phase_list(
                    failure["mutated_phases"], expected["phases"]),
                f"Failure case was accepted: {failure['id']}")
        checks += 1

    first_stdout, probe = run_probe(arguments.probe)
    second_stdout, second_probe = run_probe(arguments.probe)
    require(first_stdout == second_stdout and probe == second_probe,
            "C++ phase probe reruns differ")
    require(probe["oracle_id"] == ORACLE_ID and
            probe["status"] == "passed",
            "C++ phase probe returned an unexpected identity or status")
    require(probe["events"] == normalized_first["events"],
            "C++ phase events differ from the Legacy capture")
    for flag in ("cross_phase_priority_independent",
                 "cross_phase_registration_independent",
                 "swap_rejected", "duplicate_rejected"):
        require(probe[flag] is True,
                f"C++ phase probe did not enforce {flag}")
    checks += 7

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
        "legacy_reruns_identical": True,
        "cpp_reruns_identical": True,
        "phases": expected["phases"],
        "disposition_status": decision["status"],
    }, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError, zipfile.BadZipFile,
            json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"legacy phase reference failed: {error}", file=sys.stderr)
        raise SystemExit(1)

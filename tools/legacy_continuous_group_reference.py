#!/usr/bin/env python3
"""Independent reference and comparator for ORACLE-YYZ-GROUP-04."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile


ORACLE_ID = "ORACLE-YYZ-GROUP-04"
TRACE_SCHEMA = "gnczmkn.legacy-group-trace/1"


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    return Decimal(str(value))


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
    require(any("Test #10: test_continuous_group" in line and
                "Passed" in line for line in evidence_text.splitlines()),
            "Legacy CTest evidence does not contain the passing group test")
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
                         "Legacy group capture harness")
    checks += 4

    traces = []
    for index, trace_record in enumerate(capture["traces"], start=1):
        raw = verify_file_identity(trace_record, repo_root,
                                   f"Legacy group trace {index}")
        traces.append(json.loads(raw.decode("utf-8")))
        checks += 2
    return checks, traces


def add_scaled(state: tuple[Decimal, Decimal],
               derivative: tuple[Decimal, Decimal],
               scale: Decimal) -> tuple[Decimal, Decimal]:
    return (state[0] + derivative[0] * scale,
            state[1] + derivative[1] * scale)


def evaluate(state: tuple[Decimal, Decimal],
             mass_rate: Decimal) -> tuple[Decimal, Decimal]:
    return mass_rate, state[0]


def analytic_result(case: dict) -> dict:
    run = case["run"]
    initial = case["initial_state"]
    dynamics = case["dynamics"]
    t0 = decimal(run["initial_time_s"])
    dt = decimal(run["step_s"])
    half = Decimal("0.5")
    state0 = (decimal(initial["mass_kg"]),
              decimal(initial["position_m"]))
    mass_rate = decimal(dynamics["mass_rate_kg_per_s"])

    stage_states = [state0]
    derivatives = [evaluate(state0, mass_rate)]
    stage_states.append(add_scaled(state0, derivatives[0], half * dt))
    derivatives.append(evaluate(stage_states[1], mass_rate))
    stage_states.append(add_scaled(state0, derivatives[1], half * dt))
    derivatives.append(evaluate(stage_states[2], mass_rate))
    stage_states.append(add_scaled(state0, derivatives[2], dt))
    derivatives.append(evaluate(stage_states[3], mass_rate))

    stage_times = [t0, t0 + half * dt, t0 + half * dt, t0 + dt]
    stages = []
    for index, (time_s, state, derivative) in enumerate(
            zip(stage_times, stage_states, derivatives), start=1):
        stages.append({
            "sequence": index - 1,
            "event_kind": "rk-stage",
            "rk_stage": index,
            "time_s": time_s,
            "candidate_mass_kg": state[0],
            "candidate_position_m": state[1],
            "mass_rate_kg_per_s": derivative[0],
            "position_rate_mps": derivative[1],
        })

    final = (
        state0[0] + dt *
        (derivatives[0][0] + 2 * derivatives[1][0] +
         2 * derivatives[2][0] + derivatives[3][0]) / 6,
        state0[1] + dt *
        (derivatives[0][1] + 2 * derivatives[1][1] +
         2 * derivatives[2][1] + derivatives[3][1]) / 6,
    )
    split_snapshot_position = state0[1] + state0[0] * dt
    return {
        "stages": stages,
        "final_mass_kg": final[0],
        "final_position_m": final[1],
        "split_snapshot_position_m": split_snapshot_position,
        "effective_time_s": t0 + dt,
    }


def compare_decimal(actual: object, expected: Decimal,
                    tolerance: Decimal, label: str) -> None:
    error = abs(decimal(actual) - expected)
    require(error <= tolerance,
            f"{label} error {error} exceeds tolerance {tolerance}")


def compare_stage(actual: dict, expected: dict,
                  tolerance: Decimal, label: str) -> None:
    require(set(actual) == set(expected), f"{label} field set differs")
    for key in ("sequence", "event_kind", "rk_stage"):
        require(actual[key] == expected[key], f"{label} {key} differs")
    require(decimal(actual["time_s"]) == expected["time_s"],
            f"{label} time differs")
    for key in ("candidate_mass_kg", "candidate_position_m",
                "mass_rate_kg_per_s", "position_rate_mps"):
        compare_decimal(actual[key], expected[key], tolerance,
                        f"{label} {key}")


def validate_trace(trace: dict, expected: dict, analytic: dict,
                   tolerance: Decimal, rerun_index: int) -> dict:
    require(set(trace) == {
        "schema_version", "oracle_id", "rerun_index", "scope_id",
        "member_ids", "events", "member_final",
        "unregistered_member_rejected", "duplicate_ownership_rejected",
    }, "Legacy group trace field set differs")
    require(trace["schema_version"] == TRACE_SCHEMA,
            "Legacy group trace schema differs")
    require(trace["oracle_id"] == ORACLE_ID and
            trace["rerun_index"] == rerun_index,
            "Legacy group trace identity differs")
    require(trace["scope_id"] == expected["scope_id"] and
            trace["member_ids"] == expected["member_ids"],
            "Legacy scope or member identity differs")

    events = trace["events"]
    require(len(events) == len(analytic["stages"]) + 1,
            "Legacy group event count differs")
    for index, stage in enumerate(analytic["stages"]):
        compare_stage(events[index], stage, tolerance,
                      f"Legacy stage {index + 1}")

    commit = events[-1]
    require(set(commit) == {
        "sequence", "event_kind", "effective_time_s",
        "committed_mass_kg", "committed_position_m",
    }, "Legacy group commit field set differs")
    require(commit["sequence"] == expected["commit"]["sequence"] and
            commit["event_kind"] == expected["commit"]["event_kind"],
            "Legacy group commit identity differs")
    require(decimal(commit["effective_time_s"]) ==
            analytic["effective_time_s"],
            "Legacy group commit time differs")
    compare_decimal(commit["committed_mass_kg"],
                    analytic["final_mass_kg"], tolerance,
                    "Legacy committed mass")
    compare_decimal(commit["committed_position_m"],
                    analytic["final_position_m"], tolerance,
                    "Legacy committed position")
    require(set(trace["member_final"]) == {"mass_kg", "position_m"},
            "Legacy member-final field set differs")
    compare_decimal(trace["member_final"]["mass_kg"],
                    analytic["final_mass_kg"], tolerance,
                    "Legacy member mass")
    compare_decimal(trace["member_final"]["position_m"],
                    analytic["final_position_m"], tolerance,
                    "Legacy member position")
    require(trace["unregistered_member_rejected"] is True and
            trace["duplicate_ownership_rejected"] is True,
            "Legacy membership failure result differs")

    normalization = expected["normalization"]
    require(normalization["excluded_top_level_fields"] == ["rerun_index"] and
            normalization["excluded_event_fields"] == [],
            "Trace normalization excludes unsupported fields")
    return {key: value for key, value in trace.items()
            if key != "rerun_index"}


def compare_reference(expected: dict, analytic: dict,
                      tolerance: Decimal) -> int:
    require(len(expected["stages"]) == len(analytic["stages"]),
            "Reference stage count differs")
    checks = 1
    for index, stage in enumerate(analytic["stages"]):
        reference_stage = expected["stages"][index]
        for key in ("sequence", "event_kind", "rk_stage"):
            require(reference_stage[key] == stage[key],
                    f"Reference stage {index + 1} {key} differs")
        require(decimal(reference_stage["time_s"]) == stage["time_s"],
                f"Reference stage {index + 1} time differs")
        for key in ("candidate_mass_kg", "candidate_position_m",
                    "mass_rate_kg_per_s", "position_rate_mps"):
            compare_decimal(reference_stage[key], stage[key], tolerance,
                            f"Reference stage {index + 1} {key}")
        checks += 8

    commit = expected["commit"]
    require(commit["sequence"] == 4 and
            commit["event_kind"] == "group-commit" and
            decimal(commit["effective_time_s"]) ==
            analytic["effective_time_s"],
            "Reference commit identity or time differs")
    compare_decimal(commit["committed_mass_kg"],
                    analytic["final_mass_kg"], tolerance,
                    "Reference committed mass")
    compare_decimal(commit["committed_position_m"],
                    analytic["final_position_m"], tolerance,
                    "Reference committed position")
    compare_decimal(expected["member_final"]["mass_kg"],
                    analytic["final_mass_kg"], tolerance,
                    "Reference member mass")
    compare_decimal(expected["member_final"]["position_m"],
                    analytic["final_position_m"], tolerance,
                    "Reference member position")
    require(expected["commit_count"] == 1 and
            expected["unregistered_member_rejected"] is True and
            expected["duplicate_ownership_rejected"] is True,
            "Reference commit or membership result differs")
    return checks + 8


def run_probe(path: Path) -> tuple[str, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    require(completed.returncode == 0,
            f"C++ group probe failed: {completed.stderr.strip()}")
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
    require(len(traces) == 2,
            "Continuous-group capture requires exactly two raw runs")

    analytic = analytic_result(case)
    expected = oracle["expected"]
    tolerance = decimal(oracle["tolerances"]["synthetic_scalar_absolute"])
    checks += compare_reference(expected, analytic, tolerance)

    normalized_first = validate_trace(
        traces[0], expected, analytic, tolerance, 1)
    normalized_second = validate_trace(
        traces[1], expected, analytic, tolerance, 2)
    require(normalized_first == normalized_second,
            "Normalized Legacy group trace reruns differ")
    checks += 3 + 8 * len(analytic["stages"])

    require(analytic["final_position_m"] !=
            analytic["split_snapshot_position_m"],
            "Group case does not distinguish joint and split closure")
    failure_by_id = {entry["id"]: entry
                     for entry in oracle["failure_cases"]}
    split_failure = failure_by_id["FAIL-GROUP-SPLIT-SNAPSHOT-CLOSURE"]
    require(split_failure["expected_status"] == "rejected" and
            decimal(split_failure["mutated_position_final_m"]) ==
            analytic["split_snapshot_position_m"],
            "Split-closure failure case differs")
    require(failure_by_id["FAIL-GROUP-UNREGISTERED-MEMBER"]
            ["expected_status"] == "rejected" and
            failure_by_id["FAIL-GROUP-DUPLICATE-OWNERSHIP"]
            ["expected_status"] == "rejected",
            "Membership failure case differs")
    checks += 4

    first_stdout, probe = run_probe(arguments.probe)
    second_stdout, second_probe = run_probe(arguments.probe)
    require(first_stdout == second_stdout and probe == second_probe,
            "C++ group probe reruns differ")
    require(probe["oracle_id"] == ORACLE_ID and
            probe["status"] == "passed",
            "C++ group probe returned an unexpected identity or status")
    require(probe["scope_id"] == expected["scope_id"] and
            probe["member_ids"] == expected["member_ids"],
            "C++ scope or member identity differs")
    require(probe["events"] == normalized_first["events"] and
            probe["member_final"] == normalized_first["member_final"],
            "C++ group trace differs from the Legacy capture")
    require(probe["commit_count"] == expected["commit_count"] and
            decimal(probe["split_snapshot_position_m"]) ==
            analytic["split_snapshot_position_m"],
            "C++ commit count or split control differs")
    for flag in ("split_closure_rejected", "valid_membership_accepted",
                 "unregistered_member_rejected",
                 "duplicate_ownership_rejected"):
        require(probe[flag] is True, f"C++ probe did not enforce {flag}")
    checks += 9

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
        "final_mass_kg": str(analytic["final_mass_kg"]),
        "final_position_m": str(analytic["final_position_m"]),
        "split_snapshot_position_m": str(
            analytic["split_snapshot_position_m"]),
        "disposition_status": decision["status"],
    }, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError, zipfile.BadZipFile,
            json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"legacy continuous-group reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

#!/usr/bin/env python3
"""Independent Decimal reference and comparator for ORACLE-YYZ-PUBLISH-01."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile


ORACLE_ID = "ORACLE-YYZ-PUBLISH-01"


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


def verify_legacy_identity(case: dict, repo_root: Path) -> int:
    source = case["legacy_source"]
    archive_record = source["archive"]
    archive_path = repo_root / archive_record["path"]
    archive_bytes = archive_path.read_bytes()
    require(len(archive_bytes) == archive_record["bytes"],
            "Legacy archive byte count differs")
    require(sha256_bytes(archive_bytes) == archive_record["sha256"],
            "Legacy archive SHA-256 differs")

    with zipfile.ZipFile(archive_path, "r") as archive:
        for entry in source["entries"]:
            value = archive.read(archive_record["prefix"] + entry["path"])
            require(len(value) == entry["bytes"],
                    f"Legacy entry byte count differs: {entry['path']}")
            require(sha256_bytes(value) == entry["sha256"],
                    f"Legacy entry SHA-256 differs: {entry['path']}")

    test_record = source["runtime_test"]
    evidence = (repo_root / test_record["evidence_path"]).read_bytes()
    require(len(evidence) == test_record["evidence_bytes"],
            "Legacy CTest evidence byte count differs")
    require(sha256_bytes(evidence) == test_record["evidence_sha256"],
            "Legacy CTest evidence SHA-256 differs")
    evidence_text = evidence.decode("utf-8-sig")
    require(any("Test #11: test_publish_semantics" in line and
                "Passed" in line for line in evidence_text.splitlines()),
            "Legacy CTest evidence does not contain the passing runtime test")
    return 5


def analytic_result(case: dict) -> dict[str, Decimal]:
    time = case["time"]
    state = case["committed_state"]
    dynamics = case["dynamics"]
    t0 = decimal(time["initial_s"])
    dt = decimal(time["step_s"])
    altitude0 = decimal(state["altitude_m"])
    velocity0 = decimal(state["vertical_velocity_mps"])
    acceleration = decimal(dynamics["vertical_acceleration_mps2"])
    altitude1 = altitude0 + velocity0 * dt + acceleration * dt * dt / 2
    velocity1 = velocity0 + acceleration * dt
    return {
        "altitude_before_publish_t0_m": altitude0,
        "velocity_before_publish_t0_mps": velocity0,
        "altitude_after_publish_t0_m": altitude0,
        "velocity_after_publish_t0_mps": velocity0,
        "truth_altitude_t0_m": altitude0,
        "truth_velocity_t0_mps": velocity0,
        "truth_sample_time_t0_s": t0,
        "altitude_after_commit_t1_m": altitude1,
        "velocity_after_commit_t1_mps": velocity1,
        "truth_altitude_before_publish_t1_m": altitude0,
        "truth_sample_time_before_publish_t1_s": t0,
        "altitude_after_publish_t1_m": altitude1,
        "velocity_after_publish_t1_mps": velocity1,
        "truth_altitude_t1_m": altitude1,
        "truth_velocity_t1_mps": velocity1,
        "truth_sample_time_t1_s": t0 + dt,
    }


def compare_decimal(actual: object, expected: Decimal,
                    tolerance: Decimal, label: str) -> None:
    error = abs(decimal(actual) - expected)
    require(error <= tolerance,
            f"{label} error {error} exceeds tolerance {tolerance}")


def run_probe(path: Path) -> tuple[str, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    require(completed.returncode == 0,
            f"C++ probe failed: {completed.stderr.strip()}")
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
    require(len(input_bytes) == oracle["input_identity"]["bytes"] and
            sha256_bytes(input_bytes) ==
            oracle["input_identity"]["sha256"],
            "Input byte identity differs from the reference")

    checks = verify_legacy_identity(case, arguments.repo_root.resolve())
    analytic = analytic_result(case)
    expected = oracle["expected"]
    tolerance = decimal(oracle["tolerances"]["scalar_absolute"])
    for field, value in analytic.items():
        compare_decimal(value, decimal(expected[field]), tolerance,
                        f"reference {field}")
        checks += 1

    first_stdout, probe = run_probe(arguments.probe)
    second_stdout, second_probe = run_probe(arguments.probe)
    require(first_stdout == second_stdout and probe == second_probe,
            "C++ probe reruns differ")
    require(probe["oracle_id"] == ORACLE_ID and
            probe["status"] == "passed",
            "C++ probe returned an unexpected identity or status")
    checks += 2

    for field, value in analytic.items():
        compare_decimal(probe[field], value, tolerance, f"C++ {field}")
        checks += 1

    for flag in ("state_unchanged_t0", "state_unchanged_t1",
                 "truth_stale_between_boundaries", "mutation_rejected"):
        require(probe[flag] is True, f"C++ probe did not enforce {flag}")
        checks += 1
    require(probe["events"] == expected["events"],
            "C++ publish/commit event order differs")
    checks += 1

    failure = oracle["failure_case"]
    require(failure["expected_status"] == "rejected" and
            failure["mutated_field"] == "committed.altitude_m" and
            probe["mutation_rejected"] is True,
            "Publish mutation failure case is incomplete")
    checks += 1

    decision = oracle["disposition_decision"]
    require(decision["status"] in {"needs_owner_decision", "accepted"},
            "Disposition decision has an unsupported status")
    require({entry["recommended_disposition"]
             for entry in decision["facts"]} == {"Preserve", "Retire"},
            "Disposition recommendation must separate Preserve and Retire")
    checks += 1

    print(json.dumps({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks,
        "input_sha256": sha256_bytes(input_bytes),
        "altitude_after_commit_t1_m": str(
            analytic["altitude_after_commit_t1_m"]),
        "velocity_after_commit_t1_mps": str(
            analytic["velocity_after_commit_t1_mps"]),
        "reruns_identical": True,
        "disposition_status": decision["status"],
    }, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError, zipfile.BadZipFile,
            json.JSONDecodeError) as error:
        print(f"legacy publish reference failed: {error}", file=sys.stderr)
        raise SystemExit(1)

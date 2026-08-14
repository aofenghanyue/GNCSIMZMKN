#!/usr/bin/env python3
"""Independent Decimal reference and comparator for ORACLE-YYZ-SYNC-03."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile


ORACLE_ID = "ORACLE-YYZ-SYNC-03"


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
            logical_path = archive_record["prefix"] + entry["path"]
            value = archive.read(logical_path)
            require(len(value) == entry["bytes"],
                    f"Legacy entry byte count differs: {entry['path']}")
            require(sha256_bytes(value) == entry["sha256"],
                    f"Legacy entry SHA-256 differs: {entry['path']}")

    test_record = source["runtime_test"]
    evidence_path = repo_root / test_record["evidence_path"]
    evidence = evidence_path.read_bytes()
    require(len(evidence) == test_record["evidence_bytes"],
            "Legacy CTest evidence byte count differs")
    require(sha256_bytes(evidence) == test_record["evidence_sha256"],
            "Legacy CTest evidence SHA-256 differs")
    evidence_text = evidence.decode("utf-8-sig")
    require(any("Test #11: test_publish_semantics" in line and
                "Passed" in line for line in evidence_text.splitlines()),
            "Legacy CTest evidence does not contain the passing runtime test")
    return 4


def analytic_result(case: dict) -> dict[str, Decimal]:
    time = case["time"]
    state = case["committed_state"]
    dynamics = case["dynamics"]
    dt = decimal(time["step_s"])
    mass_initial = decimal(state["mass_kg"])
    position_initial = decimal(state["position"])
    mass_rate = decimal(dynamics["mass_rate_kg_per_s"])

    mass_candidate = mass_initial + mass_rate * dt
    position_candidate = position_initial + mass_initial * dt
    premature_position = position_initial + mass_candidate * dt
    return {
        "mass_final_kg": mass_candidate,
        "position_final": position_candidate,
        "premature_position_final": premature_position,
    }


def validate_journal(journal: list[str]) -> bool:
    candidates = ["candidate-complete:mass", "candidate-complete:position"]
    commits = ["commit:mass", "commit:position"]
    expected = set(candidates + commits)
    if len(journal) != len(expected) or set(journal) != expected:
        return False
    return all(journal.index(candidate) < journal.index(commit)
               for candidate in candidates for commit in commits)


def compare_decimal(actual: object, expected: Decimal,
                    tolerance: Decimal, label: str) -> None:
    error = abs(decimal(actual) - expected)
    require(error <= tolerance,
            f"{label} error {error} exceeds tolerance {tolerance}")


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

    require(case["oracle_id"] == ORACLE_ID,
            "Input belongs to a different oracle")
    require(oracle["oracle_id"] == ORACLE_ID,
            "Reference belongs to a different oracle")
    require(sha256_bytes(input_bytes) ==
            oracle["input_identity"]["sha256"],
            "Input SHA-256 differs from the reference")
    require(len(input_bytes) == oracle["input_identity"]["bytes"],
            "Input byte count differs from the reference")

    checks = verify_legacy_identity(case, arguments.repo_root.resolve())
    analytic = analytic_result(case)
    expected = oracle["expected"]
    tolerance = decimal(oracle["tolerances"]["scalar_absolute"])
    for field, value in analytic.items():
        compare_decimal(value, decimal(expected[field]), tolerance,
                        f"reference {field}")
        checks += 1

    require(analytic["position_final"] !=
            analytic["premature_position_final"],
            "Case does not distinguish the early-commit mutation")
    require(not validate_journal(
        ["candidate-complete:mass", "commit:mass",
         "candidate-complete:position", "commit:position"]),
        "Independent evaluator accepted an early commit")
    checks += 2

    completed = subprocess.run(
        [str(arguments.probe), "--self-check"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    require(completed.returncode == 0,
            f"C++ probe failed: {completed.stderr.strip()}")
    probe = json.loads(completed.stdout)
    require(probe["oracle_id"] == ORACLE_ID and
            probe["status"] == "passed",
            "C++ probe returned an unexpected identity or status")
    require(probe["candidate_barrier"] is True and
            probe["early_commit_rejected"] is True,
            "C++ probe did not enforce the barrier failure case")
    require(validate_journal(probe["journal"]),
            "C++ probe journal violates the candidate barrier")
    checks += 3

    for field, value in analytic.items():
        compare_decimal(probe[field], value, tolerance,
                        f"C++ {field}")
        checks += 1

    decision = oracle["disposition_decision"]
    require(decision["status"] in {"needs_owner_decision", "accepted"},
            "Disposition decision has an unsupported status")
    dispositions = {entry["recommended_disposition"]
                    for entry in decision["facts"]}
    require(dispositions == {"Preserve", "Retire"},
            "Disposition recommendation must separate Preserve and Retire")
    checks += 1

    print(json.dumps({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks,
        "input_sha256": sha256_bytes(input_bytes),
        "mass_final_kg": str(analytic["mass_final_kg"]),
        "position_final": str(analytic["position_final"]),
        "premature_position_final": str(
            analytic["premature_position_final"]),
        "disposition_status": decision["status"],
    }, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError, zipfile.BadZipFile,
            json.JSONDecodeError) as error:
        print(f"legacy sync-commit reference failed: {error}", file=sys.stderr)
        raise SystemExit(1)

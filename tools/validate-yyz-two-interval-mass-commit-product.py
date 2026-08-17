#!/usr/bin/env python3
"""Compare the R1 typed rigid/mass product with the accepted R0 oracle."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001"
ORACLE_ID = "ORACLE-YYZ-TWO-INTERVAL-MASS-COMMIT-001"
REFERENCE_MODEL_ID = "MODEL-YYZ-TWO-INTERVAL-MASS-COMMIT-001"
REFERENCE_MASS_MODEL_ID = \
    "MODEL-YYZ-SCALAR-BURN-CONSTANT-GEOMETRY-001"
PRODUCT_MODEL_ID = \
    "gnc.package.yyz.two-interval-mass-commit.experimental@1"
PRODUCT_MASS_MODEL_ID = \
    "gnc.package.yyz.mass.scalar-burn-constant-geometry.experimental@1"
CONTRACT_ID = \
    "gnc.package.yyz.rigid-mass-boundary.contract.experimental@1"
CASE_ID = "CASE-YYZ-TWO-INTERVAL-MASS-COMMIT-TRAJECTORY"
DIRECT_CHECKS = {
    "interval-zero-committed-mass-and-hidden-candidate",
    "first-atomic-boundary",
    "next-interval-consumes-committed-pair",
    "accepted-terminal-oracle-anchors",
    "constant-geometry-preserved",
    "zero-flow-mass-candidate",
    "negative-flow-rejection",
    "mass-invariant-rejection",
    "atomic-discard-on-mass-failure",
    "atomic-discard-on-rigid-failure",
    "contiguous-boundary-rejection",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite numeric value: {value}")
    return result


def compare_number(actual: object, expected: object,
                   absolute: Decimal, relative: Decimal,
                   label: str) -> Decimal:
    left = decimal(actual)
    right = decimal(expected)
    difference = abs(left - right)
    limit = absolute + relative * max(Decimal(1), abs(left), abs(right))
    require(difference <= limit, f"{label} differs: {left} vs {right}")
    return difference


def compare_vector(actual: object, expected: object,
                   absolute: Decimal, relative: Decimal,
                   label: str) -> Decimal:
    require(isinstance(actual, list) and isinstance(expected, list) and
            len(actual) == len(expected), f"{label} shape differs")
    return max((compare_number(left, right, absolute, relative,
                               f"{label}[{index}]")
                for index, (left, right) in enumerate(zip(actual, expected))),
               default=Decimal(0))


def orientation_error(actual: object, expected: object) -> Decimal:
    require(isinstance(actual, list) and isinstance(expected, list) and
            len(actual) == 4 and len(expected) == 4,
            "quaternion shape differs")
    left = [decimal(value) for value in actual]
    right = [decimal(value) for value in expected]
    left_norm = sum((value * value for value in left), Decimal(0)).sqrt()
    right_norm = sum((value * value for value in right), Decimal(0)).sqrt()
    require(left_norm > 0 and right_norm > 0,
            "quaternion has zero norm")
    left = [value / left_norm for value in left]
    right = [value / right_norm for value in right]
    if sum((a * b for a, b in zip(left, right)), Decimal(0)) < 0:
        right = [-value for value in right]
    chord = sum(((a - b) * (a - b)
                 for a, b in zip(left, right)), Decimal(0)).sqrt()
    half_chord = min(Decimal(1), chord / Decimal(2))
    return Decimal(str(4.0 * math.asin(float(half_chord))))


def compare_state(actual: dict, expected: dict,
                  absolute: Decimal, relative: Decimal,
                  label: str) -> tuple[Decimal, Decimal]:
    maximum = max(
        compare_vector(actual["position_I_m"], expected["position_I_m"],
                       absolute, relative, f"{label}.position"),
        compare_vector(actual["velocity_I_mps"],
                       expected["velocity_I_mps"], absolute, relative,
                       f"{label}.velocity"),
        compare_vector(actual["omega_BI_B_radps"],
                       expected["omega_BI_B_radps"], absolute, relative,
                       f"{label}.angular_rate"),
    )
    attitude_error = orientation_error(
        actual["q_I_B_wxyz"], expected["q_I_B_wxyz"])
    require(attitude_error <= absolute,
            f"{label}.attitude differs by {attitude_error} rad")
    return maximum, attitude_error


def run_probe(probe: Path) -> tuple[bytes, dict]:
    completed = subprocess.run(
        [str(probe), "--self-check"], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return completed.stdout, json.loads(
        completed.stdout.decode("utf-8"), parse_float=Decimal)


def verify(cases_path: Path, oracle_path: Path, probe_path: Path) -> dict:
    getcontext().prec = 80
    raw_cases = cases_path.read_bytes()
    cases = json.loads(raw_cases.decode("utf-8"), parse_float=Decimal)
    oracle = json.loads(oracle_path.read_text(encoding="utf-8"),
                        parse_float=Decimal)
    require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
            "source fixture identity differs")
    require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
            "source oracle identity differs")
    require(cases["model"]["model_id"] ==
            oracle["model_id"] == REFERENCE_MODEL_ID,
            "reference model identity differs")
    require(cases["model"]["mass_evolution_model_id"] ==
            REFERENCE_MASS_MODEL_ID,
            "reference mass model identity differs")
    raw_hash = hashlib.sha256(raw_cases).hexdigest()
    require(oracle["input_identity"]["bytes"] == len(raw_cases) and
            oracle["input_identity"]["sha256"] == raw_hash,
            "source input byte identity differs")

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    require(first_stdout == second_stdout and probe == second_probe,
            "product probe reruns differ")
    require(probe["schema_version"] ==
            "gnczmkn.yyz-two-interval-mass-commit-product-probe/1" and
            probe["product_model_id"] == PRODUCT_MODEL_ID and
            probe["mass_model_id"] == PRODUCT_MASS_MODEL_ID and
            probe["contract_id"] == CONTRACT_ID and
            probe["source_fixture_id"] == FIXTURE_ID and
            probe["source_oracle_id"] == ORACLE_ID and
            probe["reference_model_id"] == REFERENCE_MODEL_ID and
            probe["status"] == "passed",
            "product probe identity differs")
    require(PRODUCT_MODEL_ID != REFERENCE_MODEL_ID and
            PRODUCT_MASS_MODEL_ID != REFERENCE_MASS_MODEL_ID,
            "product and reference model identities must remain distinct")
    require(set(probe["direct_checks"]) == DIRECT_CHECKS and
            len(probe["direct_checks"]) == len(DIRECT_CHECKS),
            "product direct-check coverage differs")

    expected = oracle["cases"][CASE_ID]
    actual = probe["accepted"]
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    maximum_numeric_error = Decimal(0)
    maximum_orientation_error = Decimal(0)
    require(len(actual["intervals"]) == len(expected["intervals"]) == 2,
            "interval count differs")
    for index, (observed, reference) in enumerate(
            zip(actual["intervals"], expected["intervals"])):
        label = f"intervals[{index}]"
        for field in ("sample_tick", "valid_from_tick", "valid_until_tick",
                      "pending_visibility_before_commit"):
            require(observed[field] == reference[field],
                    f"{label}.{field} differs")
        for field in ("current_committed_mass_kg", "integration_mass_kg",
                      "consumed_mass_kg", "pending_mass_candidate_kg"):
            maximum_numeric_error = max(
                maximum_numeric_error,
                compare_number(observed[field], reference[field],
                               absolute, relative, f"{label}.{field}"))
        maximum_numeric_error = max(
            maximum_numeric_error,
            compare_vector(observed["acceleration_I_mps2"],
                           reference["acceleration_I_mps2"],
                           absolute, relative, f"{label}.acceleration"))
        for state_field in ("initial_rigid_state", "rigid_candidate"):
            state_error, attitude_error = compare_state(
                observed[state_field], reference[state_field],
                absolute, relative, f"{label}.{state_field}")
            maximum_numeric_error = max(maximum_numeric_error, state_error)
            maximum_orientation_error = max(
                maximum_orientation_error, attitude_error)
        observed_commit = observed["closing_commit"]
        reference_commit = reference["closing_commit"]
        require(observed_commit["tick"] == reference_commit["tick"] and
                observed_commit["kind"] == reference_commit["kind"],
                f"{label}.closing_commit identity differs")
        for field in ("mass_kg",):
            maximum_numeric_error = max(
                maximum_numeric_error,
                compare_number(observed_commit[field],
                               reference_commit[field], absolute, relative,
                               f"{label}.closing_commit.{field}"))
        for field in ("r_body_origin_to_CoM_B_m",
                      "inertia_about_CoM_B_kgm2_row_major"):
            maximum_numeric_error = max(
                maximum_numeric_error,
                compare_vector(observed_commit[field],
                               reference_commit[field], absolute, relative,
                               f"{label}.closing_commit.{field}"))
        state_error, attitude_error = compare_state(
            observed_commit["rigid_state"],
            reference_commit["rigid_state"], absolute, relative,
            f"{label}.closing_commit.rigid_state")
        maximum_numeric_error = max(maximum_numeric_error, state_error)
        maximum_orientation_error = max(
            maximum_orientation_error, attitude_error)

    observed_terminal = actual["terminal"]
    reference_terminal = expected["terminal"]
    require(observed_terminal["tick"] == reference_terminal["tick"] and
            observed_terminal["termination_kind"] ==
            reference_terminal["termination_kind"],
            "terminal identity differs")
    for field in ("time_s", "committed_mass_kg"):
        maximum_numeric_error = max(
            maximum_numeric_error,
            compare_number(observed_terminal[field],
                           reference_terminal[field], absolute, relative,
                           f"terminal.{field}"))
    for field in ("r_body_origin_to_CoM_B_m",
                  "inertia_about_CoM_B_kgm2_row_major"):
        maximum_numeric_error = max(
            maximum_numeric_error,
            compare_vector(observed_terminal[field],
                           reference_terminal[field], absolute, relative,
                           f"terminal.{field}"))
    state_error, attitude_error = compare_state(
        observed_terminal["rigid_state"],
        reference_terminal["rigid_state"], absolute, relative,
        "terminal.rigid_state")
    maximum_numeric_error = max(maximum_numeric_error, state_error)
    maximum_orientation_error = max(
        maximum_orientation_error, attitude_error)

    return {
        "source_oracle_id": ORACLE_ID,
        "product_model_id": PRODUCT_MODEL_ID,
        "status": "passed",
        "fixture_sha256": raw_hash,
        "maximum_numeric_error": str(maximum_numeric_error),
        "maximum_orientation_error_rad":
            str(maximum_orientation_error),
        "interval0_integration_mass_kg":
            str(actual["intervals"][0]["integration_mass_kg"]),
        "tick1_committed_mass_kg":
            str(actual["intervals"][0]["closing_commit"]["mass_kg"]),
        "interval1_integration_mass_kg":
            str(actual["intervals"][1]["integration_mass_kg"]),
        "terminal_mass_kg": str(actual["terminal"]["committed_mass_kg"]),
        "terminal_position_I_m": [
            str(value) for value in
            actual["terminal"]["rigid_state"]["position_I_m"]],
        "terminal_velocity_I_mps": [
            str(value) for value in
            actual["terminal"]["rigid_state"]["velocity_I_mps"]],
        "direct_checks_passed": len(probe["direct_checks"]),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", required=True, type=Path)
    parser.add_argument("--oracle", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    arguments = parser.parse_args()
    print(json.dumps(
        verify(arguments.cases, arguments.oracle, arguments.probe),
        separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ArithmeticError, KeyError, OSError, TypeError, ValueError,
            json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(
            f"YYZ two-interval product validation failed: {error}",
            file=sys.stderr)
        raise SystemExit(1)

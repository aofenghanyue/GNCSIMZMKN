#!/usr/bin/env python3
"""Compare the R1 YYZ product step with the accepted R0 Decimal oracle."""

from __future__ import annotations

import argparse
from decimal import Decimal
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-FROZEN-INTERVAL-001"
ORACLE_ID = "ORACLE-YYZ-FROZEN-INTERVAL-001"
R0_MODEL_ID = "MODEL-YYZ-LOOKUP-FROZEN-INTERVAL-001"
PRODUCT_MODEL_ID = "gnc.package.yyz.rigid-step.frozen-interval.experimental@1"
CONTRACT_ID = "gnc.package.yyz.rigid-step.contract.experimental@1"
CASE_ID = "CASE-YYZ-FROZEN-INTERVAL-COMPOSED-TRAJECTORY"
DIRECT_CHECKS = {
    "prepared-model-metadata",
    "formal-output-telemetry-separation",
    "accepted-oracle-anchors",
    "rigid-step-consumes-force-moment-closure-output",
    "rigid-step-consumes-aerodynamic-query-output",
    "canonical-model-config-roundtrip",
    "force-moment-closure-r0-oracle-anchors",
    "force-moment-closure-order-equivalence",
    "force-moment-closure-five-invalid-rejections",
    "deterministic-independent-evaluation",
    "passive-frame-direction",
    "inclusive-table-boundary",
    "strict-table-domain",
    "frame-context-rejection",
    "time-context-rejection",
    "quality-rejection",
    "mass-rejection",
    "inertia-rejection",
    "quaternion-rejection",
    "nonfinite-rejection",
    "table-preparation-rejection",
    "model-metadata-rejection",
    "definition-version-rejection",
    "full-inertia-derivative",
    "rk4-stage-discards-candidate",
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
    actual_value = decimal(actual)
    expected_value = decimal(expected)
    difference = abs(actual_value - expected_value)
    limit = absolute + relative * max(
        Decimal(1), abs(actual_value), abs(expected_value))
    require(difference <= limit,
            f"{label} differs: {actual_value} vs {expected_value}")
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
            "candidate quaternion shape differs")
    left = [decimal(value) for value in actual]
    right = [decimal(value) for value in expected]
    left_norm = sum((value * value for value in left), Decimal(0)).sqrt()
    right_norm = sum((value * value for value in right), Decimal(0)).sqrt()
    require(left_norm > 0 and right_norm > 0,
            "candidate quaternion has zero norm")
    left = [value / left_norm for value in left]
    right = [value / right_norm for value in right]
    if sum((a * b for a, b in zip(left, right)), Decimal(0)) < 0:
        right = [-value for value in right]
    chord = sum(((a - b) * (a - b)
                 for a, b in zip(left, right)), Decimal(0)).sqrt()
    half_chord = min(Decimal(1), chord / Decimal(2))
    return Decimal(str(4.0 * math.asin(float(half_chord))))


def run_probe(probe: Path) -> tuple[bytes, dict]:
    completed = subprocess.run(
        [str(probe), "--self-check"], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return completed.stdout, json.loads(
        completed.stdout.decode("utf-8"), parse_float=Decimal)


def verify(cases_path: Path, oracle_path: Path, probe_path: Path) -> dict:
    raw_cases = cases_path.read_bytes()
    cases = json.loads(raw_cases.decode("utf-8"), parse_float=Decimal)
    oracle = json.loads(oracle_path.read_text(encoding="utf-8"),
                        parse_float=Decimal)
    require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
            "YYZ source fixture identity differs")
    require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
            "YYZ source oracle identity differs")
    require(cases["model"]["model_id"] ==
            oracle["model_id"] == R0_MODEL_ID,
            "YYZ R0 model identity differs")
    raw_hash = hashlib.sha256(raw_cases).hexdigest()
    require(oracle["input_identity"]["bytes"] == len(raw_cases) and
            oracle["input_identity"]["sha256"] == raw_hash,
            "YYZ R0 oracle input identity differs")

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    require(first_stdout == second_stdout and probe == second_probe,
            "YYZ product probe reruns differ")
    require(probe["schema_version"] ==
            "gnczmkn.yyz-rigid-step-product-probe/1" and
            probe["product_model_id"] == PRODUCT_MODEL_ID and
            probe["contract_id"] == CONTRACT_ID and
            probe["source_fixture_id"] == FIXTURE_ID and
            probe["source_oracle_id"] == ORACLE_ID and
            probe["status"] == "passed",
            "YYZ product probe identity differs")
    require(PRODUCT_MODEL_ID != R0_MODEL_ID,
            "product and reference model identities must remain distinct")
    require(set(probe["direct_checks"]) == DIRECT_CHECKS and
            len(probe["direct_checks"]) == len(DIRECT_CHECKS),
            "YYZ product direct-check coverage differs")

    expected = oracle["cases"][CASE_ID]
    actual = probe["accepted"]
    tolerances = cases["tolerances"]
    formula_absolute = decimal(tolerances["formula_absolute"])
    formula_relative = decimal(tolerances["formula_relative"])
    maximum_formula_error = Decimal(0)

    require(actual["aero_contribution"]["source_id"] ==
            expected["aero_contribution"]["source_id"] and
            actual["supplied_contribution"]["source_id"] ==
            expected["propulsion_contribution"]["source_id"],
            "YYZ product contribution identities differ")
    for actual_section, expected_section, fields in (
        ("air_data", "air_data", (
            "velocity_relative_I_mps", "velocity_relative_B_mps",
            "airspeed_mps", "alpha_rad", "beta_rad",
            "dynamic_pressure_Pa", "mach")),
        ("aero_lookup", "aero_lookup", (
            "weights_M_alpha_beta",
            "coefficients_CA_CY_CN_Cl_Cm_Cn")),
        ("aero_contribution", "aero_contribution", (
            "force_B_N", "moment_about_CoM_B_Nm")),
        ("supplied_contribution", "propulsion_contribution", (
            "force_B_N", "moment_about_CoM_B_Nm")),
        ("closure", "closure", (
            "force_total_B_N", "moment_total_about_CoM_B_Nm")),
        ("rigid_derivative_at_tick0", "rigid_derivative_at_tick0", (
            "force_total_I_N", "acceleration_I_mps2",
            "angular_momentum_B_kgm2ps", "gyroscopic_moment_B_Nm",
            "net_moment_B_Nm",
            "angular_acceleration_B_radps2",
            "q_derivative_I_B_per_s")),
    ):
        for field in fields:
            left = actual[actual_section][field]
            right = expected[expected_section][field]
            difference = (compare_vector(
                left, right, formula_absolute, formula_relative,
                f"{actual_section}.{field}") if isinstance(right, list)
                else compare_number(
                    left, right, formula_absolute, formula_relative,
                    f"{actual_section}.{field}"))
            maximum_formula_error = max(maximum_formula_error, difference)

    require(actual["aero_lookup"]["domain_status"] == "Inside" and
            expected["aero_lookup"]["domain_status"] == "Interior",
            "YYZ product/R0 interior-domain semantic mapping differs")
    candidate = actual["candidate"]
    terminal = expected["analytic_terminal"]
    require(candidate["tick"] == terminal["tick"],
            "YYZ candidate tick differs")
    compare_number(candidate["time_s"], terminal["time_s"],
                   formula_absolute, formula_relative,
                   "candidate.time_s")
    maximum_position_error = compare_vector(
        candidate["position_I_m"], terminal["position_I_m"],
        decimal(tolerances["position_absolute_m"]), Decimal(0),
        "candidate.position_I_m")
    maximum_velocity_error = compare_vector(
        candidate["velocity_I_mps"], terminal["velocity_I_mps"],
        decimal(tolerances["velocity_absolute_mps"]), Decimal(0),
        "candidate.velocity_I_mps")
    maximum_angular_rate_error = compare_vector(
        candidate["omega_BI_B_radps"], terminal["omega_BI_B_radps"],
        decimal(tolerances["angular_rate_absolute_radps"]), Decimal(0),
        "candidate.omega_BI_B_radps")
    candidate_orientation_error = orientation_error(
        candidate["q_I_B_wxyz"], terminal["q_I_B_wxyz"])
    require(candidate_orientation_error <=
            decimal(tolerances["orientation_error_max_rad"]),
            "YYZ candidate orientation differs")

    return {
        "source_oracle_id": ORACLE_ID,
        "product_model_id": PRODUCT_MODEL_ID,
        "status": "passed",
        "fixture_sha256": raw_hash,
        "maximum_formula_error": str(maximum_formula_error),
        "maximum_position_error_m": str(maximum_position_error),
        "maximum_velocity_error_mps": str(maximum_velocity_error),
        "maximum_angular_rate_error_radps":
            str(maximum_angular_rate_error),
        "orientation_error_rad": str(candidate_orientation_error),
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
        print(f"YYZ rigid-step product validation failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

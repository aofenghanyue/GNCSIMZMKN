#!/usr/bin/env python3
"""Compare the R1 CAVH formula product with the accepted R0 oracle."""

from __future__ import annotations

import argparse
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-CAVH-FORMULA-001"
ORACLE_ID = "ORACLE-CAVH-FORMULA-001"
R0_MODEL_ID = "MODEL-CAVH-LEGACY-TRANSCRIBED-FORMULA-001"
PRODUCT_MODEL_ID = (
    "gnc.package.cavh.formula.legacy-transcribed.experimental@1")
CONTRACT_ID = "gnc.package.cavh.formula.contract.experimental@1"
DIRECT_CHECKS = {
    "product-reference-identity",
    "prepared-model-metadata",
    "formal-output-telemetry-separation",
    "formula-consumes-glide-envelope-query-output",
    "envelope-accepted",
    "deterministic-independent-evaluation",
    "eq18-accepted",
    "eq18-ignores-unused-derivatives",
    "eq17-accepted",
    "tdct-accepted",
    "tdct-clamp-evidence",
    "typed-formula-tdct-consumer",
    "definition-identity-rejection",
    "model-metadata-rejection",
    "package-context-policy-rejection",
    "sample-context-rejection",
    "envelope-domain-rejection",
    "envelope-alpha-domain-rejection",
    "formula-domain-rejection",
    "formula-singularity-rejection",
    "eq17-derivative-fallback-forbidden",
    "eq17-mach-partial-rejection",
    "tdct-definition-rejection",
    "tdct-nonfinite-rejection",
    "composite-discards-failed-reference",
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
            "CAVH source fixture identity differs")
    require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
            "CAVH source oracle identity differs")
    require(cases["model_choice"]["model_id"] ==
            oracle["model_id"] == R0_MODEL_ID,
            "CAVH R0 model identity differs")
    raw_hash = hashlib.sha256(raw_cases).hexdigest()
    require(oracle["input_identity"]["bytes"] == len(raw_cases) and
            oracle["input_identity"]["sha256"] == raw_hash,
            "CAVH R0 oracle input identity differs")

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    require(first_stdout == second_stdout and probe == second_probe,
            "CAVH product probe reruns differ")
    require(probe["schema_version"] ==
            "gnczmkn.cavh-formula-product-probe/1" and
            probe["product_model_id"] == PRODUCT_MODEL_ID and
            probe["contract_id"] == CONTRACT_ID and
            probe["source_fixture_id"] == FIXTURE_ID and
            probe["source_oracle_id"] == ORACLE_ID and
            probe["status"] == "passed",
            "CAVH product probe identity differs")
    require(PRODUCT_MODEL_ID != R0_MODEL_ID,
            "product and reference model identities must remain distinct")
    require(set(probe["direct_checks"]) == DIRECT_CHECKS and
            len(probe["direct_checks"]) == len(DIRECT_CHECKS),
            "CAVH product direct-check coverage differs")

    tolerances = cases["tolerances"]
    absolute = decimal(tolerances["formula_absolute"])
    relative = decimal(tolerances["formula_relative"])
    maximum_formula_error = Decimal(0)
    maximum_gamma_error = Decimal(0)
    maximum_tdct_error = Decimal(0)

    envelope_fields = (
        "cd0",
        "cl_star",
        "cd_star",
        "lift_to_drag_max",
        "alpha_star_rad",
        "dcl_star_dmach",
    )
    require(set(probe["envelope_cases"]) ==
            set(oracle["envelope_cases"]),
            "CAVH product envelope case set differs")
    for case_id, actual in probe["envelope_cases"].items():
        expected = oracle["envelope_cases"][case_id]
        for field in envelope_fields:
            maximum_formula_error = max(
                maximum_formula_error,
                compare_number(actual[field], expected[field],
                               absolute, relative,
                               f"{case_id}.{field}"))

    common_equation_fields = (
        "density_kg_per_m3",
        "density_gradient_kg_per_m4",
        "radius_m",
        "dynamic_pressure_Pa",
        "drag_force_N",
        "cl_vertical",
        "B2",
        "B3",
        "gamma_reference_rad",
    )
    eq17_fields = (
        "dcl_vertical_dmach",
        "partial_mach_partial_speed_s_per_m",
        "partial_mach_partial_altitude_per_m",
        "dcl_vertical_dspeed_s_per_m",
        "B1",
    )
    require(set(probe["equation_cases"]) ==
            set(oracle["equation_cases"]),
            "CAVH product equation case set differs")
    for case_id, actual in probe["equation_cases"].items():
        expected = oracle["equation_cases"][case_id]
        require(actual["equation"] == expected["equation"],
                f"{case_id} equation identity differs")
        fields = common_equation_fields
        if expected["equation"] == "eq17":
            fields += eq17_fields
        else:
            require(actual["B1"] is None,
                    f"{case_id} unexpectedly exposed an Eq17 denominator")
        for field in fields:
            difference = compare_number(
                actual[field], expected[field], absolute, relative,
                f"{case_id}.{field}")
            maximum_formula_error = max(maximum_formula_error, difference)
            if field == "gamma_reference_rad":
                maximum_gamma_error = max(maximum_gamma_error, difference)

    tdct_numeric_fields = (
        "error_rad",
        "correction_rad",
        "alpha_raw_rad",
        "alpha_command_rad",
    )
    require(set(probe["tdct_cases"]) == set(oracle["tdct_cases"]),
            "CAVH product TDCT case set differs")
    for case_id, actual in probe["tdct_cases"].items():
        expected = oracle["tdct_cases"][case_id]
        require(actual["saturation"] == expected["saturation"],
                f"{case_id} saturation differs")
        for field in tdct_numeric_fields:
            difference = compare_number(
                actual[field], expected[field], absolute, relative,
                f"{case_id}.{field}")
            maximum_formula_error = max(maximum_formula_error, difference)
            maximum_tdct_error = max(maximum_tdct_error, difference)

    consumer = probe["typed_consumer"]
    require(consumer["equation"] == "eq17" and
            consumer["saturation"] == "none" and
            consumer["sample_tick"] == 42 and
            consumer["configuration_revision"] == 4,
            "typed CAVH formula consumer metadata differs")
    expected_raw = (
        decimal(consumer["alpha_star_rad"]) +
        decimal(consumer["gain"]) *
        (decimal(consumer["gamma_reference_rad"]) -
         decimal(consumer["gamma_measured_rad"])))
    maximum_formula_error = max(
        maximum_formula_error,
        compare_number(consumer["alpha_raw_rad"], expected_raw,
                       absolute, relative,
                       "typed_consumer.alpha_raw_rad"),
        compare_number(consumer["alpha_limited_rad"], expected_raw,
                       absolute, relative,
                       "typed_consumer.alpha_limited_rad"))

    return {
        "source_oracle_id": ORACLE_ID,
        "product_model_id": PRODUCT_MODEL_ID,
        "status": "passed",
        "fixture_sha256": raw_hash,
        "maximum_formula_error": str(maximum_formula_error),
        "maximum_gamma_error_rad": str(maximum_gamma_error),
        "maximum_tdct_error_rad": str(maximum_tdct_error),
        "direct_check_count": len(DIRECT_CHECKS),
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
        print(f"CAVH formula product validation failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

#!/usr/bin/env python3
"""Independent Decimal reference for YYZ aero dimensionalization."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from decimal import Decimal, getcontext
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-AERO-DIMENSIONALIZATION-001"
ORACLE_ID = "ORACLE-YYZ-AERO-DIMENSIONALIZATION-001"
MODEL_ID = "MODEL-YYZ-AERO-DIMENSIONALIZATION-001"
CASES_SCHEMA = "gnczmkn.yyz-aero-dimensionalization-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-aero-dimensionalization-reference/1"
BODY_FRAME_ID = "frame.fixture.yyz.body@1"
BODY_AXES = "x-forward_y-right_z-down"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
COEFFICIENT_CONVENTION_ID = "convention.fixture.yyz.aero-body-axes@1"
SOURCE_ID = "aero.body"

SCALAR_FIELDS = (
    "dynamic_pressure_Pa",
    "reference_area_m2",
    "reference_span_m",
    "reference_chord_m",
    "pressure_area_N",
)
VECTOR_FIELDS = (
    "coefficient_vector_CA_CY_CN_Cl_Cm_Cn",
    "r_CoM_to_aero_ref_B_m",
    "force_coefficient_vector_B",
    "force_at_aero_ref_B_N",
    "moment_length_coefficient_vector_B_m",
    "moment_at_aero_ref_B_Nm",
    "transport_moment_B_Nm",
    "moment_about_CoM_B_Nm",
)
WRENCH_FIELDS = (
    "force_at_aero_ref_B_N",
    "moment_at_aero_ref_B_Nm",
    "transport_moment_B_Nm",
    "moment_about_CoM_B_Nm",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite decimal value: {value}")
    return result


def vector(values: list[object], size: int, label: str) -> list[Decimal]:
    require(isinstance(values, list) and len(values) == size,
            f"{label} must have {size} components")
    return [decimal(value) for value in values]


def scale(values: list[Decimal], factor: Decimal) -> list[Decimal]:
    return [factor * value for value in values]


def add(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left + right for left, right in zip(lhs, rhs)]


def cross(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [
        lhs[1] * rhs[2] - lhs[2] * rhs[1],
        lhs[2] * rhs[0] - lhs[0] * rhs[2],
        lhs[0] * rhs[1] - lhs[1] * rhs[0],
    ]


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def stringify(value):
    if isinstance(value, Decimal):
        if value.is_zero():
            return "0"
        encoded = format(value, "f")
        if "." in encoded:
            encoded = encoded.rstrip("0").rstrip(".")
        return encoded
    if isinstance(value, list):
        return [stringify(item) for item in value]
    if isinstance(value, dict):
        return {key: stringify(item) for key, item in value.items()}
    return value


def cases_by_id(cases: dict) -> dict[str, dict]:
    result = {case["id"]: case for case in cases["cases"]}
    require(len(result) == len(cases["cases"]),
            "duplicate aero dimensionalization case id")
    return result


def valid_nonnegative_integer(value: object) -> bool:
    return (isinstance(value, int) and not isinstance(value, bool) and
            value >= 0)


def validate_identity(case: dict) -> None:
    context = case["context"]
    require(context["body_frame_id"] == BODY_FRAME_ID,
            "aero body frame differs")
    require(context["body_axes"] == BODY_AXES,
            "aero body axes differ")
    require(context["clock_domain"] == CLOCK_DOMAIN,
            "aero clock domain differs")
    require(valid_nonnegative_integer(context["sample_tick"]),
            "aero sample tick must be a nonnegative integer")
    require(valid_nonnegative_integer(context["configuration_revision"]),
            "aero configuration revision must be a nonnegative integer")

    tick = context["sample_tick"]
    clock = context["clock_domain"]
    revision = context["configuration_revision"]
    air_data = case["air_data"]
    require(air_data["sample_tick"] == tick and
            air_data["clock_domain"] == clock and
            air_data["configuration_revision"] == revision,
            "air-data identity differs from aero context")
    geometry = case["geometry"]
    require(geometry["body_frame_id"] == context["body_frame_id"] and
            geometry["sample_tick"] == tick and
            geometry["clock_domain"] == clock and
            geometry["configuration_revision"] == revision,
            "reference-geometry identity differs from aero context")
    coefficients = case["coefficients"]
    require(coefficients["convention_id"] == COEFFICIENT_CONVENTION_ID,
            "aero coefficient convention differs")
    require(coefficients["body_frame_id"] == context["body_frame_id"] and
            coefficients["sample_tick"] == tick and
            coefficients["clock_domain"] == clock and
            coefficients["configuration_revision"] == revision,
            "aero coefficient identity differs from context")


def dimensionalize(case: dict, force_mode: str = "accepted",
                   moment_mode: str = "accepted",
                   transport_mode: str = "accepted") -> dict:
    validate_identity(case)
    air_data = case["air_data"]
    geometry = case["geometry"]
    coefficients = case["coefficients"]

    dynamic_pressure = decimal(air_data["dynamic_pressure_Pa"])
    area = decimal(geometry["reference_area_m2"])
    span = decimal(geometry["reference_span_m"])
    chord = decimal(geometry["reference_chord_m"])
    reference_point = vector(
        geometry["r_CoM_to_aero_ref_B_m"], 3,
        "r_CoM_to_aero_ref_B")
    coefficient_vector = vector(
        coefficients["coefficient_vector_CA_CY_CN_Cl_Cm_Cn"], 6,
        "aerodynamic coefficient vector")
    require(dynamic_pressure >= 0,
            "dynamic pressure must be nonnegative")
    require(area > 0, "reference area must be positive")
    require(span > 0, "reference span must be positive")
    require(chord > 0, "reference chord must be positive")

    c_a, c_y, c_n_normal, c_l, c_m, c_n_yaw = coefficient_vector
    require(force_mode in ("accepted", "direct_body_signs"),
            "unsupported aero force mode")
    force_coefficients = (
        [-c_a, c_y, -c_n_normal]
        if force_mode == "accepted" else [c_a, c_y, c_n_normal])
    require(moment_mode in ("accepted", "single_span"),
            "unsupported aero moment mode")
    pitch_length = chord if moment_mode == "accepted" else span
    moment_length_coefficients = [
        span * c_l,
        pitch_length * c_m,
        span * c_n_yaw,
    ]
    pressure_area = dynamic_pressure * area
    force = scale(force_coefficients, pressure_area)
    moment_at_reference = scale(moment_length_coefficients, pressure_area)
    require(transport_mode in ("accepted", "reversed_vector"),
            "unsupported aero transport mode")
    transported_reference = (
        reference_point if transport_mode == "accepted"
        else scale(reference_point, Decimal(-1)))
    transport_moment = cross(transported_reference, force)
    moment_about_com = add(moment_at_reference, transport_moment)
    derived = (force_coefficients + moment_length_coefficients + force +
               moment_at_reference + transport_moment + moment_about_com)
    require(all(value.is_finite() for value in derived),
            "aero dimensionalization produced a non-finite value")

    context = copy.deepcopy(case["context"])
    closure = {
        "source_id": SOURCE_ID,
        "body_frame_id": context["body_frame_id"],
        "configuration_revision": context["configuration_revision"],
        "valid_from_tick": context["sample_tick"],
        "valid_until_tick": context["sample_tick"] + 1,
        "force_B_N": force,
        "r_CoM_to_application_B_m": reference_point,
        "moment_at_application_B_Nm": moment_at_reference,
    }
    return {
        "id": case["id"],
        "context": context,
        "coefficient_convention_id": COEFFICIENT_CONVENTION_ID,
        "dynamic_pressure_Pa": dynamic_pressure,
        "reference_area_m2": area,
        "reference_span_m": span,
        "reference_chord_m": chord,
        "coefficient_vector_CA_CY_CN_Cl_Cm_Cn": coefficient_vector,
        "r_CoM_to_aero_ref_B_m": reference_point,
        "pressure_area_N": pressure_area,
        "force_coefficient_vector_B": force_coefficients,
        "force_at_aero_ref_B_N": force,
        "moment_length_coefficient_vector_B_m":
            moment_length_coefficients,
        "moment_at_aero_ref_B_Nm": moment_at_reference,
        "transport_moment_B_Nm": transport_moment,
        "moment_about_CoM_B_Nm": moment_about_com,
        "closure_contribution": closure,
    }


def numeric_values(result: dict, fields: tuple[str, ...]) -> list[Decimal]:
    values: list[Decimal] = []
    for field in fields:
        value = result[field]
        if isinstance(value, list):
            values.extend(decimal(item) for item in value)
        else:
            values.append(decimal(value))
    return values


def max_wrench_difference(lhs: dict, rhs: dict) -> Decimal:
    left = numeric_values(lhs, WRENCH_FIELDS)
    right = numeric_values(rhs, WRENCH_FIELDS)
    return max((abs(a - b) for a, b in zip(left, right)),
               default=Decimal(0))


def reference_equivalence_results(cases: dict) -> list[dict]:
    indexed = cases_by_id(cases)
    base = indexed["CASE-YYZ-AERO-DIMENSIONALIZATION-ASYMMETRIC"]
    accepted = dimensionalize(base)

    pressure_area_case = copy.deepcopy(base)
    pressure_area_case["air_data"]["dynamic_pressure_Pa"] *= Decimal(5)
    pressure_area_case["geometry"]["reference_area_m2"] /= Decimal(5)
    pressure_area = dimensionalize(pressure_area_case)

    length_case = copy.deepcopy(base)
    length_case["geometry"]["reference_span_m"] *= Decimal(2)
    length_case["geometry"]["reference_chord_m"] *= Decimal(2)
    for index in (3, 4, 5):
        length_case["coefficients"][
            "coefficient_vector_CA_CY_CN_Cl_Cm_Cn"][index] /= Decimal(2)
    length = dimensionalize(length_case)

    results = [
        {
            "id": "EQUIV-YYZ-AERO-PRESSURE-AREA-FACTORIZATION",
            "status": "passed" if max_wrench_difference(
                accepted, pressure_area) <= Decimal("1e-68") else "failed",
            "max_abs_wrench_difference": max_wrench_difference(
                accepted, pressure_area),
        },
        {
            "id": "EQUIV-YYZ-AERO-LENGTH-COEFFICIENT-FACTORIZATION",
            "status": "passed" if max_wrench_difference(
                accepted, length) <= Decimal("1e-68") else "failed",
            "max_abs_wrench_difference": max_wrench_difference(
                accepted, length),
        },
    ]
    require(all(result["status"] == "passed" for result in results),
            "Python aero equivalence check failed")
    return results


def rejects(operation) -> bool:
    try:
        operation()
    except (KeyError, TypeError, ValueError):
        return True
    return False


def reference_invalid_rejections(cases: dict) -> list[str]:
    base = cases_by_id(cases)[
        "CASE-YYZ-AERO-DIMENSIONALIZATION-ASYMMETRIC"]
    results = []

    def add_mutation(identifier: str, mutate) -> None:
        value = copy.deepcopy(base)
        mutate(value)
        if rejects(lambda: dimensionalize(value)):
            results.append(identifier)

    add_mutation("INVALID-YYZ-AERO-FRAME-MISMATCH",
                 lambda value: value["coefficients"].__setitem__(
                     "body_frame_id", "frame.other@1"))
    add_mutation("INVALID-YYZ-AERO-CLOCK-MISMATCH",
                 lambda value: value["air_data"].__setitem__(
                     "clock_domain", "clock.other@1"))
    add_mutation("INVALID-YYZ-AERO-SAMPLE-TICK-MISMATCH",
                 lambda value: value["geometry"].__setitem__(
                     "sample_tick", value["context"]["sample_tick"] + 1))
    add_mutation("INVALID-YYZ-AERO-REVISION-MISMATCH",
                 lambda value: value["coefficients"].__setitem__(
                     "configuration_revision",
                     value["context"]["configuration_revision"] + 1))
    add_mutation("INVALID-YYZ-AERO-NONFINITE-COEFFICIENT",
                 lambda value: value["coefficients"][
                     "coefficient_vector_CA_CY_CN_Cl_Cm_Cn"].__setitem__(
                         0, Decimal("Infinity")))
    add_mutation("INVALID-YYZ-AERO-NEGATIVE-DYNAMIC-PRESSURE",
                 lambda value: value["air_data"].__setitem__(
                     "dynamic_pressure_Pa", Decimal("-1")))
    add_mutation("INVALID-YYZ-AERO-NONPOSITIVE-AREA",
                 lambda value: value["geometry"].__setitem__(
                     "reference_area_m2", 0))
    add_mutation("INVALID-YYZ-AERO-NONPOSITIVE-SPAN",
                 lambda value: value["geometry"].__setitem__(
                     "reference_span_m", 0))
    add_mutation("INVALID-YYZ-AERO-NONPOSITIVE-CHORD",
                 lambda value: value["geometry"].__setitem__(
                     "reference_chord_m", 0))
    add_mutation("INVALID-YYZ-AERO-NONFINITE-REFERENCE-POINT",
                 lambda value: value["geometry"][
                     "r_CoM_to_aero_ref_B_m"].__setitem__(
                         0, Decimal("Infinity")))
    return results


def reference_mutation_results(cases: dict) -> list[dict]:
    case = cases_by_id(cases)[
        "CASE-YYZ-AERO-DIMENSIONALIZATION-ASYMMETRIC"]
    accepted = dimensionalize(case)
    profiles = [
        ("MUTATION-YYZ-AERO-DIRECT-BODY-SIGNS",
         {"force_mode": "direct_body_signs"}),
        ("MUTATION-YYZ-AERO-SINGLE-REFERENCE-LENGTH",
         {"moment_mode": "single_span"}),
        ("MUTATION-YYZ-AERO-REVERSED-REFERENCE-VECTOR",
         {"transport_mode": "reversed_vector"}),
    ]
    results = []
    for identifier, options in profiles:
        mutated = dimensionalize(case, **options)
        difference = max_wrench_difference(accepted, mutated)
        results.append({
            "id": identifier,
            "status": "rejected" if difference > Decimal("1e-30")
            else "matched",
            "max_abs_wrench_difference": difference,
        })
    require(all(result["status"] == "rejected" for result in results),
            "Python aero reference accepted a physical mutation")
    return results


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    invalid = reference_invalid_rejections(cases)
    expected_invalid = [entry["id"] for entry in cases[
        "invalid_input_cases"]]
    require(invalid == expected_invalid,
            "Python aero invalid-input coverage differs")
    return stringify({
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "status": "executable",
        "precision": {"decimal_digits": getcontext().prec},
        "input_identity": {
            "path": "fixtures/ref-yyz-aero-dimensionalization/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": {
            case["id"]: dimensionalize(case) for case in cases["cases"]
        },
        "equivalence_results": reference_equivalence_results(cases),
        "invalid_input_rejections": invalid,
        "mutation_results": reference_mutation_results(cases),
    })


class Checks:
    def __init__(self) -> None:
        self.count = 0

    def require(self, condition: bool, message: str,
                observations: int = 1) -> None:
        require(condition, message)
        self.count += observations


def within_tolerance(actual: object, expected: object,
                     absolute: Decimal, relative: Decimal) -> bool:
    actual_value = decimal(actual)
    expected_value = decimal(expected)
    difference = abs(actual_value - expected_value)
    bound = absolute + relative * max(abs(actual_value), abs(expected_value))
    return difference <= bound


def compare_scalar(checks: Checks, actual: object, expected: object,
                   absolute: Decimal, relative: Decimal, label: str) -> None:
    checks.require(within_tolerance(actual, expected, absolute, relative),
                   f"{label} differs")


def compare_vector(checks: Checks, actual: list, expected: list,
                   absolute: Decimal, relative: Decimal, label: str) -> None:
    checks.require(len(actual) == len(expected), f"{label} length differs")
    for index, (actual_value, expected_value) in enumerate(
            zip(actual, expected)):
        compare_scalar(checks, actual_value, expected_value,
                       absolute, relative, f"{label}[{index}]")


def run_probe(path: Path) -> tuple[str, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    require(completed.returncode == 0,
            f"C++ aero probe failed: {completed.stderr.strip()}")
    return completed.stdout, json.loads(
        completed.stdout, parse_float=Decimal)


def compare_case(checks: Checks, actual: dict, expected: dict,
                 absolute: Decimal, relative: Decimal) -> None:
    case_id = expected["id"]
    checks.require(actual["id"] == case_id,
                   f"C++ aero case identity differs for {case_id}")
    checks.require(actual["context"] == expected["context"],
                   f"C++ aero context differs for {case_id}", 5)
    checks.require(actual["coefficient_convention_id"] ==
                   expected["coefficient_convention_id"],
                   f"C++ coefficient convention differs for {case_id}")
    for field in SCALAR_FIELDS:
        compare_scalar(checks, actual[field], expected[field],
                       absolute, relative, f"{case_id}.{field}")
    for field in VECTOR_FIELDS:
        compare_vector(checks, actual[field], expected[field],
                       absolute, relative, f"{case_id}.{field}")

    actual_closure = actual["closure_contribution"]
    expected_closure = expected["closure_contribution"]
    for field in ("source_id", "body_frame_id", "configuration_revision",
                  "valid_from_tick", "valid_until_tick"):
        checks.require(actual_closure[field] == expected_closure[field],
                       f"{case_id}.closure_contribution.{field} differs")
    for field in ("force_B_N", "r_CoM_to_application_B_m",
                  "moment_at_application_B_Nm"):
        compare_vector(checks, actual_closure[field],
                       expected_closure[field], absolute, relative,
                       f"{case_id}.closure_contribution.{field}")


def verify_reference(cases: dict, raw_cases: bytes, oracle: dict,
                     probe_path: Path) -> dict:
    checks = Checks()
    checks.require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
                   "aero fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "aero oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "aero model identity differs", 2)
    checks.require(oracle["precision"]["decimal_digits"] >= 70,
                   "aero oracle precision is below 70 digits")
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "aero cases byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-aero-dimensionalization/cases.json",
                   "aero input path differs")

    recomputed = build_reference(cases, raw_cases)
    checks.require(oracle == recomputed,
                   "stored aero oracle differs from its Decimal producer",
                   len(oracle["cases"]) + 4)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ aero probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed" and
                   probe["model_choice_status"] ==
                   cases["model_choice"]["status"],
                   "C++ aero probe identity differs", 4)

    probe_cases = {entry["id"]: entry for entry in probe["cases"]}
    checks.require(len(probe_cases) == len(probe["cases"]) ==
                   len(oracle["cases"]),
                   "C++ aero case identities are incomplete", 2)
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    for case_id, expected in oracle["cases"].items():
        checks.require(case_id in probe_cases,
                       f"C++ aero case is missing: {case_id}")
        compare_case(checks, probe_cases[case_id], expected,
                     absolute, relative)

    expected_equivalence = {
        entry["id"] for entry in cases["equivalence_cases"]
    }
    checks.require(set(probe["equivalence_checks"]) == expected_equivalence,
                   "C++ aero equivalence identities differ")
    expected_invalid = {
        entry["id"] for entry in cases["invalid_input_cases"]
    }
    checks.require(set(probe["invalid_input_rejections"]) == expected_invalid,
                   "C++ aero invalid-input identities differ")
    expected_mutations = {
        entry["id"] for entry in cases["mutation_cases"]
    }
    checks.require(set(probe["mutation_rejections"]) == expected_mutations,
                   "C++ aero mutation identities differ")

    asymmetric = probe_cases[
        "CASE-YYZ-AERO-DIMENSIONALIZATION-ASYMMETRIC"]
    separate = probe_cases[
        "CASE-YYZ-AERO-DIMENSIONALIZATION-SEPARATE-LENGTHS"]
    zero_pressure = probe_cases[
        "CASE-YYZ-AERO-DIMENSIONALIZATION-ZERO-DYNAMIC-PRESSURE"]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "case_count": len(probe_cases),
        "asymmetric_force_B_N": asymmetric["force_at_aero_ref_B_N"],
        "asymmetric_moment_at_aero_ref_B_Nm":
            asymmetric["moment_at_aero_ref_B_Nm"],
        "asymmetric_transport_moment_B_Nm":
            asymmetric["transport_moment_B_Nm"],
        "asymmetric_moment_about_CoM_B_Nm":
            asymmetric["moment_about_CoM_B_Nm"],
        "separate_lengths_moment_B_Nm":
            separate["moment_at_aero_ref_B_Nm"],
        "zero_dynamic_pressure_wrench_is_zero":
            all(decimal(value).is_zero() for field in WRENCH_FIELDS
                for value in zero_pressure[field]),
        "equivalence_cases_passed": len(expected_equivalence),
        "invalid_input_cases_rejected": len(expected_invalid),
        "mutation_cases_rejected": len(expected_mutations),
    })


def validate_cases_identity(cases: dict) -> None:
    require(cases["schema_version"] == CASES_SCHEMA and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model"]["model_id"] == MODEL_ID,
            "aero cases identity differs")
    require(cases["model_choice"]["status"] == "accepted",
            "aero model choice is not accepted")
    require(cases["model"]["body_frame_id"] == BODY_FRAME_ID and
            cases["model"]["body_axes"] == BODY_AXES and
            cases["model"]["clock_domain"] == CLOCK_DOMAIN and
            cases["model"]["coefficient_convention_id"] ==
            COEFFICIENT_CONVENTION_ID and
            cases["model"]["source_id"] == SOURCE_ID,
            "aero model convention differs")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", required=True, type=Path)
    parser.add_argument("--oracle", type=Path)
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--generate-reference", action="store_true")
    arguments = parser.parse_args()

    getcontext().prec = 80
    raw_cases = arguments.cases.read_bytes()
    cases = json.loads(raw_cases.decode("utf-8"), parse_float=Decimal)
    validate_cases_identity(cases)

    if arguments.generate_reference:
        require(arguments.oracle is not None and arguments.probe is None,
                "reference generation requires --oracle and rejects --probe")
        output = json.dumps(build_reference(cases, raw_cases), indent=2,
                            ensure_ascii=False) + "\n"
        arguments.oracle.parent.mkdir(parents=True, exist_ok=True)
        with arguments.oracle.open(
                "w", encoding="utf-8", newline="\n") as stream:
            stream.write(output)
        print(json.dumps({
            "oracle_id": ORACLE_ID,
            "status": "generated",
            "path": arguments.oracle.as_posix(),
            "bytes": len(output.encode("utf-8")),
            "sha256": sha256_bytes(output.encode("utf-8")),
        }, separators=(",", ":")))
        return 0

    require(arguments.oracle is not None and arguments.probe is not None,
            "verification requires --oracle and --probe")
    oracle = json.loads(arguments.oracle.read_text(encoding="utf-8"),
                        parse_float=Decimal)
    result = verify_reference(cases, raw_cases, oracle, arguments.probe)
    print(json.dumps(result, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ArithmeticError, IndexError, KeyError, OSError, TypeError,
            ValueError, json.JSONDecodeError,
            subprocess.SubprocessError) as error:
        print(f"YYZ aero dimensionalization reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

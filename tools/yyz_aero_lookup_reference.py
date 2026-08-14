#!/usr/bin/env python3
"""Independent Decimal reference for fixture-local YYZ aero lookup."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal, getcontext
import hashlib
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-AERO-LOOKUP-001"
ORACLE_ID = "ORACLE-YYZ-AERO-LOOKUP-001"
MODEL_ID = "MODEL-YYZ-AERO-TRILINEAR-LOOKUP-001"
TABLE_ID = "aero-table.fixture.yyz.multiaffine@1"
CONFIGURATION_ID = "configuration.fixture.yyz.clean@1"
BODY_FRAME_ID = "frame.fixture.yyz.body@1"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
QUALITY = "Valid"
CASES_SCHEMA = "gnczmkn.yyz-aero-lookup-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-aero-lookup-reference/1"
PROFILE_STATUS = "accepted"
COEFFICIENT_ORDER = ["C_A", "C_Y", "C_N", "C_l", "C_m", "C_n"]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite decimal value: {value}")
    return result


def vector(values: object, size: int, label: str) -> list[Decimal]:
    require(isinstance(values, list) and len(values) == size,
            f"{label} must have {size} components")
    return [decimal(value) for value in values]


def scale(values: list[Decimal], factor: Decimal) -> list[Decimal]:
    return [value * factor for value in values]


def add(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left + right for left, right in zip(lhs, rhs)]


def lerp(lhs: list[Decimal], rhs: list[Decimal],
         weight: Decimal) -> list[Decimal]:
    return [left + weight * (right - left)
            for left, right in zip(lhs, rhs)]


def valid_nonnegative_integer(value: object) -> bool:
    return (isinstance(value, int) and not isinstance(value, bool) and
            value >= 0)


def validate_axis(axis: object, label: str) -> list[Decimal]:
    require(isinstance(axis, list) and len(axis) >= 2,
            f"{label} must have at least two knots")
    parsed = [decimal(value) for value in axis]
    require(all(parsed[index] < parsed[index + 1]
                for index in range(len(parsed) - 1)),
            f"{label} must be strictly increasing")
    return parsed


def validate_table(table: dict) -> tuple[list[Decimal], list[Decimal],
                                         list[Decimal], list[list[Decimal]]]:
    require(table["layout"] == "mach-major-alpha-middle-beta-fastest",
            "aero table layout differs")
    mach_axis = validate_axis(table["mach_axis"], "Mach axis")
    alpha_axis = validate_axis(table["alpha_axis_rad"], "alpha axis")
    beta_axis = validate_axis(table["beta_axis_rad"], "beta axis")
    raw_rows = table["coefficient_rows_CA_CY_CN_Cl_Cm_Cn"]
    require(isinstance(raw_rows, list) and
            len(raw_rows) == len(mach_axis) * len(alpha_axis) * len(beta_axis),
            "aero coefficient row count differs from grid shape")
    rows = [vector(row, 6, "aero coefficient row") for row in raw_rows]
    return mach_axis, alpha_axis, beta_axis, rows


def validate_case(case: dict, table: dict) -> None:
    mach_axis, alpha_axis, beta_axis, _ = validate_table(table)
    context = case["context"]
    require(context["model_id"] == MODEL_ID and
            context["table_id"] == TABLE_ID and
            context["configuration_id"] == CONFIGURATION_ID and
            context["body_frame_id"] == BODY_FRAME_ID and
            context["clock_domain"] == CLOCK_DOMAIN,
            "aero lookup query identity differs")
    require(valid_nonnegative_integer(context["sample_tick"]) and
            valid_nonnegative_integer(context["configuration_revision"]),
            "aero lookup tick or revision is invalid")
    operating = case["operating_point"]
    mach = decimal(operating["mach"])
    alpha = decimal(operating["alpha_rad"])
    beta = decimal(operating["beta_rad"])
    require(mach_axis[0] <= mach <= mach_axis[-1],
            "Mach is outside the validated aero table")
    require(alpha_axis[0] <= alpha <= alpha_axis[-1],
            "alpha is outside the validated aero table")
    require(beta_axis[0] <= beta <= beta_axis[-1],
            "beta is outside the validated aero table")
    rates = vector(operating["omega_BI_B_radps"], 3, "body rate")
    surfaces = vector(operating["surface_state_rad"], 4, "surface state")
    require(all(value.is_zero() for value in rates),
            "fixture-local aero lookup supports only zero body rates")
    require(all(value.is_zero() for value in surfaces),
            "fixture-local aero lookup supports only zero surfaces")
    require(operating["required_derivative_set"] == [],
            "fixture-local aero lookup supports no derivative request")
    probe = case["dimensionalization_probe"]
    require(decimal(probe["dynamic_pressure_Pa"]) >= 0 and
            decimal(probe["reference_area_m2"]) > 0 and
            decimal(probe["reference_span_m"]) > 0 and
            decimal(probe["reference_chord_m"]) > 0,
            "aero dimensionalization probe is outside its domain")


def bracket(axis: list[Decimal], query: Decimal,
            label: str) -> dict:
    require(axis[0] <= query <= axis[-1],
            f"{label} query is outside the prepared axis")
    if query == axis[-1]:
        lower_index = len(axis) - 2
        upper_index = len(axis) - 1
        weight = Decimal(1)
    else:
        lower_index = next(index for index in range(len(axis) - 1)
                           if axis[index] <= query < axis[index + 1])
        upper_index = lower_index + 1
        weight = ((query - axis[lower_index]) /
                  (axis[upper_index] - axis[lower_index]))
    return {
        "lower_index": lower_index,
        "upper_index": upper_index,
        "lower_value": axis[lower_index],
        "upper_value": axis[upper_index],
        "weight": weight,
    }


def row_index(mach_index: int, alpha_index: int, beta_index: int,
              alpha_count: int, beta_count: int) -> int:
    return ((mach_index * alpha_count + alpha_index) * beta_count +
            beta_index)


def trilinear(rows: list[list[Decimal]], mach_bracket: dict,
              alpha_bracket: dict, beta_bracket: dict,
              alpha_count: int, beta_count: int,
              beta_weight_override: Decimal | None = None) -> list[Decimal]:
    weights = [mach_bracket["weight"], alpha_bracket["weight"],
               (beta_bracket["weight"] if beta_weight_override is None
                else beta_weight_override)]
    result = [Decimal(0)] * 6
    for mach_corner in range(2):
        mach_weight = weights[0] if mach_corner else Decimal(1) - weights[0]
        mach_index = mach_bracket[
            "upper_index" if mach_corner else "lower_index"]
        for alpha_corner in range(2):
            alpha_weight = (weights[1] if alpha_corner else
                            Decimal(1) - weights[1])
            alpha_index = alpha_bracket[
                "upper_index" if alpha_corner else "lower_index"]
            for beta_corner in range(2):
                beta_weight = (weights[2] if beta_corner else
                               Decimal(1) - weights[2])
                beta_index = beta_bracket[
                    "upper_index" if beta_corner else "lower_index"]
                corner = rows[row_index(
                    mach_index, alpha_index, beta_index,
                    alpha_count, beta_count)]
                result = add(result, scale(
                    corner, mach_weight * alpha_weight * beta_weight))
    return result


def alternate_nested(rows: list[list[Decimal]], mach_bracket: dict,
                     alpha_bracket: dict, beta_bracket: dict,
                     alpha_count: int, beta_count: int) -> list[Decimal]:
    """Interpolate Mach, then alpha, then beta to change nesting order."""
    beta_faces: list[list[Decimal]] = []
    for beta_key in ("lower_index", "upper_index"):
        alpha_edges: list[list[Decimal]] = []
        for alpha_key in ("lower_index", "upper_index"):
            lower = rows[row_index(
                mach_bracket["lower_index"], alpha_bracket[alpha_key],
                beta_bracket[beta_key], alpha_count, beta_count)]
            upper = rows[row_index(
                mach_bracket["upper_index"], alpha_bracket[alpha_key],
                beta_bracket[beta_key], alpha_count, beta_count)]
            alpha_edges.append(lerp(lower, upper, mach_bracket["weight"]))
        beta_faces.append(lerp(
            alpha_edges[0], alpha_edges[1], alpha_bracket["weight"]))
    return lerp(beta_faces[0], beta_faces[1], beta_bracket["weight"])


def dimensionalize(coefficients: list[Decimal], probe: dict) -> dict:
    ca_value, cy_value, cn_value, cl_value, cm_value, cn_moment = coefficients
    dynamic_pressure = decimal(probe["dynamic_pressure_Pa"])
    area = decimal(probe["reference_area_m2"])
    span = decimal(probe["reference_span_m"])
    chord = decimal(probe["reference_chord_m"])
    pressure_area = dynamic_pressure * area
    return {
        "dynamic_pressure_Pa": dynamic_pressure,
        "reference_area_m2": area,
        "reference_span_m": span,
        "reference_chord_m": chord,
        "force_B_N": scale([-ca_value, cy_value, -cn_value], pressure_area),
        "moment_at_aero_reference_B_Nm": [
            pressure_area * span * cl_value,
            pressure_area * chord * cm_value,
            pressure_area * span * cn_moment,
        ],
    }


def lookup(case: dict, table: dict, *, mode: str = "trilinear") -> dict:
    validate_case(case, table)
    mach_axis, alpha_axis, beta_axis, rows = validate_table(table)
    operating = case["operating_point"]
    mach_query = decimal(operating["mach"])
    alpha_query = decimal(operating["alpha_rad"])
    beta_query = decimal(operating["beta_rad"])
    if mode == "swap-alpha-beta":
        alpha_query, beta_query = beta_query, alpha_query
    mach_bracket = bracket(mach_axis, mach_query, "Mach")
    alpha_bracket = bracket(alpha_axis, alpha_query, "alpha")
    beta_bracket = bracket(beta_axis, beta_query, "beta")
    if mode == "nearest":
        indices = [
            mach_bracket["upper_index"] if mach_bracket["weight"] >
            Decimal("0.5") else mach_bracket["lower_index"],
            alpha_bracket["upper_index"] if alpha_bracket["weight"] >
            Decimal("0.5") else alpha_bracket["lower_index"],
            beta_bracket["upper_index"] if beta_bracket["weight"] >
            Decimal("0.5") else beta_bracket["lower_index"],
        ]
        coefficients = rows[row_index(
            indices[0], indices[1], indices[2],
            len(alpha_axis), len(beta_axis))]
    else:
        coefficients = trilinear(
            rows, mach_bracket, alpha_bracket, beta_bracket,
            len(alpha_axis), len(beta_axis),
            Decimal(0) if mode == "lower-beta-face" else None)
    is_boundary = any(query in (axis[0], axis[-1]) for query, axis in (
        (mach_query, mach_axis), (alpha_query, alpha_axis),
        (beta_query, beta_axis)))
    context = case["context"]
    return {
        "id": case["id"],
        "query_identity": {
            "model_id": context["model_id"],
            "table_id": context["table_id"],
            "configuration_id": context["configuration_id"],
            "body_frame_id": context["body_frame_id"],
            "clock_domain": context["clock_domain"],
            "sample_tick": context["sample_tick"],
            "configuration_revision": context["configuration_revision"],
        },
        "operating_point": {
            "mach": mach_query,
            "alpha_rad": alpha_query,
            "beta_rad": beta_query,
            "omega_BI_B_radps": vector(
                operating["omega_BI_B_radps"], 3, "body rate"),
            "surface_state_rad": vector(
                operating["surface_state_rad"], 4, "surface state"),
            "required_derivative_count":
                len(operating["required_derivative_set"]),
        },
        "brackets": {
            "mach": mach_bracket,
            "alpha": alpha_bracket,
            "beta": beta_bracket,
        },
        "response": {
            "quality": QUALITY,
            "domain_status": "Boundary" if is_boundary else "Inside",
            "coefficient_order": COEFFICIENT_ORDER,
            "coefficients": coefficients,
        },
        "dimensionalization_consumer": dimensionalize(
            coefficients, case["dimensionalization_probe"]),
    }


def max_difference(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    require(len(lhs) == len(rhs), "numeric vectors have different lengths")
    return max((abs(left - right) for left, right in zip(lhs, rhs)),
               default=Decimal(0))


def physical_vector(result: dict) -> list[Decimal]:
    return (list(result["response"]["coefficients"]) +
            list(result["dimensionalization_consumer"]["force_B_N"]) +
            list(result["dimensionalization_consumer"][
                "moment_at_aero_reference_B_Nm"]))


def interpolation_equivalence(case: dict, table: dict,
                              accepted: dict) -> dict:
    mach_axis, alpha_axis, beta_axis, rows = validate_table(table)
    operating = case["operating_point"]
    mach_bracket = bracket(mach_axis, decimal(operating["mach"]), "Mach")
    alpha_bracket = bracket(
        alpha_axis, decimal(operating["alpha_rad"]), "alpha")
    beta_bracket = bracket(
        beta_axis, decimal(operating["beta_rad"]), "beta")
    coefficients = alternate_nested(
        rows, mach_bracket, alpha_bracket, beta_bracket,
        len(alpha_axis), len(beta_axis))
    consumer = dimensionalize(
        coefficients, case["dimensionalization_probe"])
    alternate_vector = (coefficients + consumer["force_B_N"] +
                        consumer["moment_at_aero_reference_B_Nm"])
    difference = max_difference(physical_vector(accepted), alternate_vector)
    require(difference == 0,
            "aero interpolation nesting order changed the response")
    return {
        "id": "EQUIV-YYZ-AERO-LOOKUP-INTERPOLATION-ORDER",
        "status": "passed",
        "alternate_coefficients": coefficients,
        "alternate_force_B_N": consumer["force_B_N"],
        "alternate_moment_at_aero_reference_B_Nm":
            consumer["moment_at_aero_reference_B_Nm"],
        "max_abs_physical_difference": difference,
    }


def invalid_rejections(cases: dict, accepted_case: dict,
                       prepared_table: dict) -> list[str]:
    def case_action(function):
        return lambda case, table: function(case)

    def table_action(function):
        return lambda case, table: function(table)

    actions = {
        "INVALID-YYZ-AERO-LOOKUP-CONFIGURATION": case_action(
            lambda item: item["context"].__setitem__(
                "configuration_id", "configuration.fixture.yyz.other@1")),
        "INVALID-YYZ-AERO-LOOKUP-CLOCK": case_action(
            lambda item: item["context"].__setitem__(
                "clock_domain", "clock.fixture.yyz.other@1")),
        "INVALID-YYZ-AERO-LOOKUP-NEGATIVE-TICK": case_action(
            lambda item: item["context"].__setitem__("sample_tick", -1)),
        "INVALID-YYZ-AERO-LOOKUP-DERIVATIVE-REQUEST": case_action(
            lambda item: item["operating_point"].__setitem__(
                "required_derivative_set", ["dC_m/dalpha"])),
        "INVALID-YYZ-AERO-LOOKUP-NONZERO-RATE": case_action(
            lambda item: item["operating_point"].__setitem__(
                "omega_BI_B_radps", [0, 0.1, 0])),
        "INVALID-YYZ-AERO-LOOKUP-NONZERO-SURFACE": case_action(
            lambda item: item["operating_point"].__setitem__(
                "surface_state_rad", [0, 0, 0.05, 0])),
        "INVALID-YYZ-AERO-LOOKUP-MACH-LOW": case_action(
            lambda item: item["operating_point"].__setitem__("mach", 0.1)),
        "INVALID-YYZ-AERO-LOOKUP-ALPHA-HIGH": case_action(
            lambda item: item["operating_point"].__setitem__(
                "alpha_rad", 0.2)),
        "INVALID-YYZ-AERO-LOOKUP-BETA-LOW": case_action(
            lambda item: item["operating_point"].__setitem__(
                "beta_rad", -0.1)),
        "INVALID-YYZ-AERO-LOOKUP-DUPLICATE-AXIS": table_action(
            lambda item: item.__setitem__("mach_axis", [0.2, 0.2])),
        "INVALID-YYZ-AERO-LOOKUP-ROW-COUNT": table_action(
            lambda item: item[
                "coefficient_rows_CA_CY_CN_Cl_Cm_Cn"].pop()),
        "INVALID-YYZ-AERO-LOOKUP-NONFINITE-COEFFICIENT": table_action(
            lambda item: item[
                "coefficient_rows_CA_CY_CN_Cl_Cm_Cn"][0].__setitem__(
                    0, "NaN")),
    }
    rejected: list[str] = []
    for specification in cases["invalid_input_cases"]:
        identifier = specification["id"]
        require(identifier in actions,
                f"unsupported aero lookup invalid case: {identifier}")
        mutated_case = copy.deepcopy(accepted_case)
        mutated_table = copy.deepcopy(prepared_table)
        actions[identifier](mutated_case, mutated_table)
        try:
            lookup(mutated_case, mutated_table)
        except (ArithmeticError, IndexError, KeyError, StopIteration,
                TypeError, ValueError):
            rejected.append(identifier)
        else:
            raise ValueError(f"invalid aero lookup input survived: {identifier}")
    return rejected


def mutation_results(cases: dict, accepted_case: dict, table: dict,
                     accepted: dict) -> list[dict]:
    nearest = lookup(accepted_case, table, mode="nearest")
    swapped = lookup(accepted_case, table, mode="swap-alpha-beta")
    lower_beta = lookup(accepted_case, table, mode="lower-beta-face")
    clamped_case = copy.deepcopy(accepted_case)
    clamped_case["operating_point"]["mach"] = table["mach_axis"][0]
    clamped = lookup(clamped_case, table)
    identifiers = [entry["id"] for entry in cases["mutation_cases"]]

    def result(identifier: str, observed: dict,
               extra: dict | None = None) -> dict:
        output = {
            "id": identifier,
            "status": "rejected",
            "observed_coefficients": observed["response"]["coefficients"],
            "observed_force_B_N":
                observed["dimensionalization_consumer"]["force_B_N"],
            "observed_moment_at_aero_reference_B_Nm":
                observed["dimensionalization_consumer"][
                    "moment_at_aero_reference_B_Nm"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(observed)),
        }
        if extra:
            output.update(extra)
        return output

    results = [
        result(identifiers[0], nearest),
        result(identifiers[1], clamped, {
            "out_of_domain_query_mach": Decimal("0.1"),
            "observed_clamped_mach": decimal(table["mach_axis"][0]),
        }),
        result(identifiers[2], swapped, {
            "observed_alpha_rad":
                swapped["operating_point"]["alpha_rad"],
            "observed_beta_rad": swapped["operating_point"]["beta_rad"],
        }),
        result(identifiers[3], lower_beta),
    ]
    require(all(entry["max_abs_physical_difference"] > 0
                for entry in results),
            "an aero lookup mutation matched the accepted response")
    return results


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


def validate_cases_identity(cases: dict) -> None:
    require(cases["schema_version"] == CASES_SCHEMA and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model_choice"]["status"] == PROFILE_STATUS and
            cases["model"]["model_id"] == MODEL_ID,
            "aero lookup cases identity differs")
    model = cases["model"]
    require(model["table_id"] == TABLE_ID and
            model["configuration_id"] == CONFIGURATION_ID and
            model["body_frame_id"] == BODY_FRAME_ID and
            model["clock_domain"] == CLOCK_DOMAIN and
            model["quality"] == QUALITY and
            model["execution_form"] == "fixture-local-pure-query" and
            cases["model_choice"]["coefficient_order"] == COEFFICIENT_ORDER,
            "aero lookup model profile differs")
    require(len(cases["cases"]) == 3,
            "aero lookup bundle must contain three query cases")


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    validate_cases_identity(cases)
    table = cases["prepared_table"]
    validate_table(table)
    computed = {case["id"]: lookup(case, table) for case in cases["cases"]}
    first_case = cases["cases"][0]
    first_result = computed[first_case["id"]]
    return stringify({
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "precision": {
            "implementation": "Python standard-library Decimal trilinear lookup",
            "decimal_digits": getcontext().prec,
        },
        "input_identity": {
            "path": "fixtures/ref-yyz-aero-lookup/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": computed,
        "equivalence_results": [interpolation_equivalence(
            first_case, table, first_result)],
        "invalid_input_rejections": invalid_rejections(
            cases, first_case, table),
        "mutation_results": mutation_results(
            cases, first_case, table, first_result),
    })


class Checks:
    def __init__(self) -> None:
        self.count = 0

    def require(self, condition: bool, message: str,
                increment: int = 1) -> None:
        self.count += increment
        require(condition, message)


def numeric_string(value: object) -> bool:
    if not isinstance(value, str):
        return False
    try:
        return Decimal(value).is_finite()
    except ArithmeticError:
        return False


def compare_numeric(checks: Checks, actual: object, expected: object,
                    absolute: Decimal, relative: Decimal,
                    label: str) -> None:
    actual_value = decimal(actual)
    expected_value = decimal(expected)
    difference = abs(actual_value - expected_value)
    bound = absolute + relative * max(
        abs(actual_value), abs(expected_value), Decimal(1))
    checks.require(difference <= bound,
                   f"{label} differs: {actual_value} vs {expected_value}")


def compare_tree(checks: Checks, actual, expected,
                 absolute: Decimal, relative: Decimal,
                 label: str) -> None:
    if isinstance(expected, dict):
        checks.require(isinstance(actual, dict), f"{label} is not an object")
        checks.require(set(actual) == set(expected),
                       f"{label} fields differ")
        for key, expected_value in expected.items():
            compare_tree(checks, actual[key], expected_value,
                         absolute, relative, f"{label}.{key}")
        return
    if isinstance(expected, list):
        checks.require(isinstance(actual, list) and
                       len(actual) == len(expected),
                       f"{label} list shape differs")
        for index, expected_value in enumerate(expected):
            compare_tree(checks, actual[index], expected_value,
                         absolute, relative, f"{label}[{index}]")
        return
    if numeric_string(expected):
        compare_numeric(checks, actual, expected, absolute, relative, label)
        return
    checks.require(actual == expected, f"{label} differs")


def run_probe(probe_path: Path) -> tuple[bytes, dict]:
    completed = subprocess.run(
        [str(probe_path), "--self-check"], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return completed.stdout, json.loads(
        completed.stdout.decode("utf-8"), parse_float=Decimal)


def verify_reference(cases: dict, raw_cases: bytes, oracle: dict,
                     probe_path: Path) -> dict:
    checks = Checks()
    checks.require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
                   "aero lookup fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "aero lookup oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "aero lookup model identity differs", 2)
    checks.require(oracle["precision"]["decimal_digits"] >= 70,
                   "aero lookup reference precision is below 70 digits")
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "aero lookup input byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-aero-lookup/cases.json",
                   "aero lookup input path differs")
    recomputed = build_reference(cases, raw_cases)
    checks.require(oracle == recomputed,
                   "stored aero lookup oracle differs from its producer",
                   len(oracle["cases"]) + 4)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ aero lookup probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed" and
                   probe["model_choice_status"] == PROFILE_STATUS,
                   "C++ aero lookup probe identity differs", 4)
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    compare_tree(checks, probe["cases"], list(oracle["cases"].values()),
                 absolute, relative, "cases")
    compare_tree(checks, probe["equivalence_results"],
                 oracle["equivalence_results"], absolute, relative,
                 "equivalence_results")
    checks.require(probe["invalid_input_rejections"] ==
                   oracle["invalid_input_rejections"],
                   "aero lookup invalid-input identities differ",
                   len(oracle["invalid_input_rejections"]))
    compare_tree(checks, probe["mutation_results"],
                 oracle["mutation_results"], absolute, relative,
                 "mutation_results")
    first = probe["cases"][0]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "case_count": len(probe["cases"]),
        "interior_weights": [
            first["brackets"]["mach"]["weight"],
            first["brackets"]["alpha"]["weight"],
            first["brackets"]["beta"]["weight"],
        ],
        "interior_coefficients": first["response"]["coefficients"],
        "interior_force_B_N":
            first["dimensionalization_consumer"]["force_B_N"],
        "equivalence_cases_passed": len(oracle["equivalence_results"]),
        "invalid_input_cases_rejected":
            len(oracle["invalid_input_rejections"]),
        "mutation_cases_rejected": len(oracle["mutation_results"]),
    })


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
    print(json.dumps(
        verify_reference(cases, raw_cases, oracle, arguments.probe),
        separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ArithmeticError, IndexError, KeyError, OSError, StopIteration,
            TypeError, ValueError, json.JSONDecodeError,
            subprocess.SubprocessError) as error:
        print(f"YYZ aero lookup reference failed: {error}", file=sys.stderr)
        raise SystemExit(1)

#!/usr/bin/env python3
"""Independent Decimal comparator for the R1 strict trilinear product path."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-AERO-LOOKUP-001"
ORACLE_ID = "ORACLE-YYZ-AERO-LOOKUP-001"
PROBE_SCHEMA = "gnczmkn.foundation-trilinear-probe/1"
COMPONENT_ID = "GNC-FOUNDATION-TRILINEAR-001"
PREPARE_ALGORITHM = "gnc.foundation.interpolation.trilinear-prepare@1"
QUERY_ALGORITHM = "gnc.foundation.interpolation.trilinear-strict@1"
LAYOUT = "x-major-y-middle-z-fastest"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite decimal value: {value}")
    return result


def axis(values: object, label: str) -> list[Decimal]:
    require(isinstance(values, list) and len(values) >= 2,
            f"{label} requires at least two knots")
    result = [decimal(value) for value in values]
    require(all(result[index] < result[index + 1]
                for index in range(len(result) - 1)),
            f"{label} is not strictly increasing")
    return result


def bracket(values: list[Decimal], query: Decimal) -> dict[str, object]:
    require(values[0] <= query <= values[-1], "query is outside axis")
    if query == values[-1]:
        lower = len(values) - 2
        upper = len(values) - 1
        weight = Decimal(1)
    else:
        lower = next(index for index in range(len(values) - 1)
                     if values[index] <= query < values[index + 1])
        upper = lower + 1
        weight = ((query - values[lower]) /
                  (values[upper] - values[lower]))
    return {
        "lower_index": lower,
        "upper_index": upper,
        "lower_value": values[lower],
        "upper_value": values[upper],
        "weight": weight,
    }


def row_index(x_index: int, y_index: int, z_index: int,
              y_count: int, z_count: int) -> int:
    return (x_index * y_count + y_index) * z_count + z_index


def reference_query(table: dict, query: tuple[Decimal, Decimal, Decimal]
                    ) -> dict[str, object]:
    require(table["layout"] == "mach-major-alpha-middle-beta-fastest",
            "fixture table layout differs")
    axes = [
        axis(table["mach_axis"], "Mach axis"),
        axis(table["alpha_axis_rad"], "alpha axis"),
        axis(table["beta_axis_rad"], "beta axis"),
    ]
    rows = [[decimal(value) for value in row]
            for row in table["coefficient_rows_CA_CY_CN_Cl_Cm_Cn"]]
    require(len(rows) == len(axes[0]) * len(axes[1]) * len(axes[2]),
            "fixture row count differs")
    require(all(len(row) == 6 for row in rows),
            "fixture coefficient width differs")
    brackets = [bracket(values, value)
                for values, value in zip(axes, query)]

    result = [Decimal(0)] * 6
    for x_corner in range(2):
        x_weight = (brackets[0]["weight"] if x_corner else
                    Decimal(1) - brackets[0]["weight"])
        x_index = brackets[0][
            "upper_index" if x_corner else "lower_index"]
        for y_corner in range(2):
            y_weight = (brackets[1]["weight"] if y_corner else
                        Decimal(1) - brackets[1]["weight"])
            y_index = brackets[1][
                "upper_index" if y_corner else "lower_index"]
            for z_corner in range(2):
                z_weight = (brackets[2]["weight"] if z_corner else
                            Decimal(1) - brackets[2]["weight"])
                z_index = brackets[2][
                    "upper_index" if z_corner else "lower_index"]
                row = rows[row_index(
                    x_index, y_index, z_index, len(axes[1]), len(axes[2]))]
                corner_weight = x_weight * y_weight * z_weight
                result = [current + value * corner_weight
                          for current, value in zip(result, row)]

    boundary = any(value in (values[0], values[-1])
                   for values, value in zip(axes, query))
    return {
        "brackets": {
            "mach": brackets[0],
            "alpha": brackets[1],
            "beta": brackets[2],
        },
        "domain_status": "Boundary" if boundary else "Inside",
        "coefficients": result,
    }


def compare_number(actual: object, expected: object, absolute: Decimal,
                   relative: Decimal, label: str) -> Decimal:
    actual_value = decimal(actual)
    expected_value = decimal(expected)
    difference = abs(actual_value - expected_value)
    limit = absolute + relative * max(
        Decimal(1), abs(actual_value), abs(expected_value))
    require(difference <= limit,
            f"{label} differs: {actual_value} vs {expected_value}")
    return difference


def compare_bracket(actual: dict, expected: dict, absolute: Decimal,
                    relative: Decimal, label: str) -> Decimal:
    require(actual["lower_index"] == expected["lower_index"] and
            actual["upper_index"] == expected["upper_index"],
            f"{label} indices differ")
    return max(compare_number(actual[field], expected[field], absolute,
                              relative, f"{label}.{field}")
               for field in ("lower_value", "upper_value", "weight"))


def run_probe(path: Path) -> tuple[bytes, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"], check=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    return completed.stdout, json.loads(
        completed.stdout.decode("utf-8"), parse_float=Decimal)


def verify(cases: dict, oracle: dict, probe_path: Path) -> dict:
    require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
            "fixture identity differs")
    require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
            "oracle identity differs")
    require(oracle["precision"]["decimal_digits"] >= 70,
            "stored oracle precision is below 70 digits")
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])

    expected_by_id: dict[str, dict] = {}
    for case in cases["cases"]:
        point = case["operating_point"]
        expected = reference_query(cases["prepared_table"], (
            decimal(point["mach"]), decimal(point["alpha_rad"]),
            decimal(point["beta_rad"])))
        oracle_case = oracle["cases"][case["id"]]
        require(expected["domain_status"] ==
                oracle_case["response"]["domain_status"],
                f"stored domain status differs for {case['id']}")
        for axis_name in ("mach", "alpha", "beta"):
            compare_bracket(oracle_case["brackets"][axis_name],
                            expected["brackets"][axis_name], Decimal(0),
                            Decimal(0),
                            f"stored.{case['id']}.{axis_name}")
        for index, value in enumerate(expected["coefficients"]):
            compare_number(
                oracle_case["response"]["coefficients"][index], value,
                Decimal(0), Decimal(0),
                f"stored.{case['id']}.coefficients[{index}]")
        expected_by_id[case["id"]] = expected

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    require(first_stdout == second_stdout and probe == second_probe,
            "product probe reruns differ")
    require(probe["schema_version"] == PROBE_SCHEMA and
            probe["component_id"] == COMPONENT_ID and
            probe["fixture_id"] == FIXTURE_ID,
            "product probe identity differs")
    algorithm = probe["algorithm"]
    require(algorithm == {
        "prepare_id": PREPARE_ALGORITHM,
        "query_id": QUERY_ALGORITHM,
        "version": "1.0.0",
        "layout": LAYOUT,
    }, "product algorithm identity differs")
    require(probe["preparation_evaluations"] == 8,
            "product preparation row count differs")

    product_cases = {entry["id"]: entry for entry in probe["cases"]}
    require(set(product_cases) == set(expected_by_id),
            "product query case identities differ")
    maximum_difference = Decimal(0)
    for identifier, expected in expected_by_id.items():
        actual = product_cases[identifier]
        require(actual["status"] == "Success" and actual["has_value"] and
                actual["evaluations"] == 8,
                f"product query outcome differs for {identifier}")
        require(actual["domain_status"] == expected["domain_status"],
                f"product domain status differs for {identifier}")
        for axis_name in ("mach", "alpha", "beta"):
            maximum_difference = max(
                maximum_difference,
                compare_bracket(actual["brackets"][axis_name],
                                expected["brackets"][axis_name], absolute,
                                relative,
                                f"product.{identifier}.{axis_name}"))
        for index, value in enumerate(expected["coefficients"]):
            maximum_difference = max(
                maximum_difference,
                compare_number(actual["coefficients"][index], value,
                               absolute, relative,
                               f"product.{identifier}.coefficients[{index}]"))

    expected_query_failures = {
        "INVALID-YYZ-AERO-LOOKUP-MACH-LOW": ("OutOfRange", "x-query"),
        "INVALID-YYZ-AERO-LOOKUP-ALPHA-HIGH": ("OutOfRange", "y-query"),
        "INVALID-YYZ-AERO-LOOKUP-BETA-LOW": ("OutOfRange", "z-query"),
        "NONFINITE-QUERY": ("NonFiniteInput", "query"),
    }
    expected_preparation_failures = {
        "INVALID-YYZ-AERO-LOOKUP-DUPLICATE-AXIS":
            ("DomainError", "x-axis"),
        "INVALID-YYZ-AERO-LOOKUP-ROW-COUNT":
            ("DomainError", "row-count"),
        "INVALID-YYZ-AERO-LOOKUP-NONFINITE-COEFFICIENT":
            ("NonFiniteInput", "coefficient"),
    }

    def check_failures(entries: list[dict], expected: dict,
                       label: str) -> None:
        actual = {entry["id"]: entry for entry in entries}
        require(set(actual) == set(expected), f"{label} identities differ")
        for identifier, (status, detail) in expected.items():
            entry = actual[identifier]
            require(entry["status"] == status and
                    entry["detail"] == detail and
                    entry["has_value"] is False,
                    f"{label} outcome differs for {identifier}")

    check_failures(probe["query_failures"], expected_query_failures,
                   "query failures")
    check_failures(probe["preparation_failures"],
                   expected_preparation_failures,
                   "preparation failures")
    interior = product_cases["CASE-YYZ-AERO-LOOKUP-INTERIOR"]
    return {
        "component_id": COMPONENT_ID,
        "status": "passed",
        "case_count": len(product_cases),
        "strict_out_of_range_cases": 3,
        "preparation_failures": len(expected_preparation_failures),
        "interior_weights": [
            str(interior["brackets"][name]["weight"])
            for name in ("mach", "alpha", "beta")
        ],
        "max_abs_numeric_difference": str(maximum_difference),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", required=True, type=Path)
    parser.add_argument("--oracle", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    arguments = parser.parse_args()

    getcontext().prec = 80
    cases = json.loads(arguments.cases.read_text(encoding="utf-8"),
                       parse_float=Decimal)
    oracle = json.loads(arguments.oracle.read_text(encoding="utf-8"),
                        parse_float=Decimal)
    print(json.dumps(verify(cases, oracle, arguments.probe),
                     separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ArithmeticError, IndexError, KeyError, OSError, StopIteration,
            TypeError, ValueError, json.JSONDecodeError,
            subprocess.SubprocessError) as error:
        print(f"foundation trilinear validation failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

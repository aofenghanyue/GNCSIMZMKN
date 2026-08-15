#!/usr/bin/env python3
"""Independent Decimal comparator for the R1 fixed 3x3 SPD solve path."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-MASS-PROPERTIES-001"
ORACLE_ID = "ORACLE-YYZ-MASS-PROPERTIES-001"
PROBE_SCHEMA = "gnczmkn.foundation-spd-solve-probe/1"
COMPONENT_ID = "GNC-FOUNDATION-SPD-SOLVE-001"
ALGORITHM_ID = "gnc.foundation.linear.spd-cholesky-3x3@1"
SYMMETRIZED_FLAG = 1 << 3


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite Decimal value: {value}")
    return result


def vector(values: object, label: str) -> list[Decimal]:
    require(isinstance(values, list) and len(values) == 3,
            f"{label} must have three entries")
    return [decimal(value) for value in values]


def matrix(values: object, label: str) -> list[list[Decimal]]:
    require(isinstance(values, list) and len(values) == 9,
            f"{label} must have nine entries")
    parsed = [decimal(value) for value in values]
    return [parsed[0:3], parsed[3:6], parsed[6:9]]


def flatten(value: list[list[Decimal]]) -> list[Decimal]:
    return [entry for row in value for entry in row]


def add(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left + right for left, right in zip(lhs, rhs)]


def subtract(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left - right for left, right in zip(lhs, rhs)]


def dot(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    return sum((left * right for left, right in zip(lhs, rhs)), Decimal(0))


def cross(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [
        lhs[1] * rhs[2] - lhs[2] * rhs[1],
        lhs[2] * rhs[0] - lhs[0] * rhs[2],
        lhs[0] * rhs[1] - lhs[1] * rhs[0],
    ]


def matrix_vector(product: list[list[Decimal]],
                  value: list[Decimal]) -> list[Decimal]:
    return [dot(row, value) for row in product]


def cholesky(spd: list[list[Decimal]]) -> list[list[Decimal]]:
    require(all(spd[row][column] == spd[column][row]
                for row in range(3) for column in range(3)),
            "independent SPD matrix is asymmetric")
    lower = [[Decimal(0) for _ in range(3)] for _ in range(3)]
    for row in range(3):
        for column in range(row + 1):
            residual = spd[row][column] - sum(
                (lower[row][index] * lower[column][index]
                 for index in range(column)), Decimal(0))
            if row == column:
                require(residual > 0,
                        "independent SPD matrix is not positive definite")
                lower[row][column] = residual.sqrt()
            else:
                lower[row][column] = residual / lower[column][column]
    return lower


def solve_factor(lower: list[list[Decimal]],
                 rhs: list[Decimal]) -> list[Decimal]:
    forward = [Decimal(0)] * 3
    for row in range(3):
        forward[row] = (
            rhs[row] - sum((lower[row][column] * forward[column]
                            for column in range(row)), Decimal(0))
        ) / lower[row][row]
    result = [Decimal(0)] * 3
    for row in reversed(range(3)):
        result[row] = (
            forward[row] - sum((lower[column][row] * result[column]
                                for column in range(row + 1, 3)), Decimal(0))
        ) / lower[row][row]
    return result


def solve_spd(spd: list[list[Decimal]],
              rhs: list[Decimal]) -> list[Decimal]:
    return solve_factor(cholesky(spd), rhs)


def matrix_one_norm(value: list[list[Decimal]]) -> Decimal:
    return max(sum((abs(value[row][column]) for row in range(3)),
                   Decimal(0)) for column in range(3))


def condition_one(spd: list[list[Decimal]]) -> Decimal:
    lower = cholesky(spd)
    inverse_columns = [
        solve_factor(lower, [Decimal(index == column)
                             for index in range(3)])
        for column in range(3)
    ]
    inverse = [[inverse_columns[column][row] for column in range(3)]
               for row in range(3)]
    return matrix_one_norm(spd) * matrix_one_norm(inverse)


def close(actual: object, expected: object, absolute: Decimal,
          relative: Decimal, label: str) -> Decimal:
    actual_value = decimal(actual)
    expected_value = decimal(expected)
    difference = abs(actual_value - expected_value)
    limit = absolute + relative * max(
        Decimal(1), abs(actual_value), abs(expected_value))
    require(difference <= limit,
            f"{label} differs: {actual_value} vs {expected_value}")
    return difference


def close_vector(actual: object, expected: list[Decimal],
                 absolute: Decimal, relative: Decimal,
                 label: str) -> Decimal:
    require(isinstance(actual, list) and len(actual) == len(expected),
            f"{label} length differs")
    actual_values = [decimal(value) for value in actual]
    return max(close(actual_value, expected_value, absolute, relative,
                     f"{label}[{index}]")
               for index, (actual_value, expected_value) in enumerate(
                   zip(actual_values, expected)))


def consumer_values(case: dict) -> dict[str, object]:
    state = case["current_committed_state"]
    closure = case["closure_probe"]
    inertia = matrix(
        state["inertia_about_CoM_B_kgm2_row_major"], "fixture inertia")
    com = vector(state["r_body_origin_to_CoM_B_m"], "fixture CoM")
    application = vector(
        closure["r_body_origin_to_application_B_m"], "fixture application")
    force = vector(closure["force_B_N"], "fixture force")
    intrinsic = vector(
        closure["intrinsic_moment_at_application_B_Nm"],
        "fixture intrinsic moment")
    moment = add(intrinsic, cross(subtract(application, com), force))
    omega = vector(case["rigid_core_probe"]["omega_BI_B_radps"],
                   "fixture omega")
    angular_momentum = matrix_vector(inertia, omega)
    gyroscopic = cross(omega, angular_momentum)
    rhs = subtract(moment, gyroscopic)
    return {
        "inertia": inertia,
        "omega": omega,
        "moment": moment,
        "angular_momentum": angular_momentum,
        "gyroscopic": gyroscopic,
        "rhs": rhs,
        "solution": solve_spd(inertia, rhs),
        "condition": condition_one(inertia),
    }


def run_probe(path: Path) -> tuple[bytes, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"], check=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    return completed.stdout, json.loads(
        completed.stdout.decode("utf-8"), parse_float=Decimal)


def verify_success(observation: dict, expected_matrix: list[list[Decimal]],
                   expected_rhs: list[Decimal], expected_solution: list[Decimal],
                   expected_condition: Decimal, absolute: Decimal,
                   relative: Decimal, label: str) -> tuple[Decimal, Decimal]:
    require(observation["status"] == "Success" and
            observation["has_value"] is True and
            observation["rank"] == 3 and
            observation["method"] == "Cholesky" and
            observation["flags"] == 0 and
            observation["detail"] == "cholesky" and
            observation["evaluations"] == 4,
            f"{label} success metadata differs")
    close_vector(observation["matrix_row_major"], flatten(expected_matrix),
                 absolute, relative, f"{label}.matrix")
    close_vector(observation["rhs"], expected_rhs, absolute, relative,
                 f"{label}.rhs")
    solution_error = close_vector(
        observation["solution"], expected_solution, absolute, relative,
        f"{label}.solution")
    condition_error = close(
        observation["condition_estimate"], expected_condition,
        Decimal("2e-12"), Decimal("2e-12"), f"{label}.condition")

    actual_solution = vector(observation["solution"], f"{label}.solution")
    independent_residual = subtract(
        matrix_vector(expected_matrix, actual_solution), expected_rhs)
    independent_residual_norm = max(abs(value)
                                    for value in independent_residual)
    residual_limit = absolute + relative * max(
        Decimal(1), *(abs(value) for value in expected_rhs))
    require(decimal(observation["residual_norm"]) <= residual_limit,
            f"{label} reported residual is too large")
    require(independent_residual_norm <= residual_limit,
            f"{label} independent residual is too large")
    require(decimal(observation["relative_residual"]) <= Decimal("2e-15"),
            f"{label} normalized residual is too large")
    return solution_error, condition_error


def verify(cases: dict, oracle: dict, probe_path: Path) -> dict:
    require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
            "YYZ mass-properties fixture identity differs")
    require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
            "YYZ mass-properties oracle identity differs")
    require(oracle["precision"]["decimal_digits"] >= 70,
            "YYZ mass-properties precision is below 70 digits")
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    stored_absolute = decimal(
        cases["tolerances"]["stored_decimal_absolute"])

    independent: dict[str, dict[str, object]] = {}
    for case in cases["cases"]:
        identifier = case["id"]
        values = consumer_values(case)
        stored = oracle["cases"][identifier]["rigid_core_consumer"]
        close_vector(stored["net_moment_B_Nm"], values["rhs"],
                     stored_absolute, Decimal(0),
                     f"stored.{identifier}.rhs")
        close_vector(stored["angular_acceleration_B_radps2"],
                     values["solution"], stored_absolute, Decimal(0),
                     f"stored.{identifier}.solution")
        independent[identifier] = values

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    require(first_stdout == second_stdout and probe == second_probe,
            "SPD solve probe reruns differ")
    require(probe["schema_version"] == PROBE_SCHEMA and
            probe["component_id"] == COMPONENT_ID and
            probe["fixture_id"] == FIXTURE_ID,
            "SPD solve probe identity differs")
    require(probe["algorithm"] == {
        "id": ALGORITHM_ID,
        "version": "1.0.0",
    }, "SPD solve algorithm identity differs")
    require(probe["storage"] == {
        "eigen_version": "3.4.0",
        "scalar": "binary64",
        "vector_convention": "column",
        "matrix_storage_order": "column-major",
        "vec3_shape": [3, 1],
        "mat3_shape": [3, 3],
    }, "canonical linear algebra storage differs")
    policy = probe["policy"]
    require(decimal(policy["absolute_tolerance"]) == Decimal("2e-12") and
            decimal(policy["relative_tolerance"]) == Decimal("2e-12") and
            decimal(policy["zero_tolerance"]) == Decimal("1e-14") and
            decimal(policy["condition_limit"]) == Decimal("1e12"),
            "SPD solve policy differs")

    product_cases = {entry["id"]: entry for entry in probe["yyz_cases"]}
    require(set(product_cases) == set(independent),
            "SPD solve YYZ case identities differ")
    maximum_solution_error = Decimal(0)
    maximum_condition_error = Decimal(0)
    condition_values: list[Decimal] = []
    for identifier, expected in independent.items():
        solution_error, condition_error = verify_success(
            product_cases[identifier], expected["inertia"], expected["rhs"],
            expected["solution"], expected["condition"], absolute, relative,
            f"product.{identifier}")
        maximum_solution_error = max(maximum_solution_error, solution_error)
        maximum_condition_error = max(maximum_condition_error,
                                      condition_error)
        condition_values.append(decimal(
            product_cases[identifier]["condition_estimate"]))

    scale_cases = probe["scale_cases"]
    expected_scales = [Decimal("1e-9"), Decimal(1), Decimal("1e9")]
    require(len(scale_cases) == len(expected_scales),
            "SPD solve scale ladder length differs")
    for entry, expected_scale in zip(scale_cases, expected_scales):
        close(entry["scale"], expected_scale, Decimal("2e-24"),
              Decimal("2e-16"), "SPD solve scale ladder")
    first_id = cases["cases"][0]["id"]
    first = independent[first_id]
    maximum_scaled_relative_residual = Decimal(0)
    for entry in scale_cases:
        scale = decimal(entry["scale"])
        scaled_matrix = [[scale * value for value in row]
                         for row in first["inertia"]]
        scaled_rhs = [scale * value for value in first["rhs"]]
        solve = entry["solve"]
        verify_success(solve, scaled_matrix, scaled_rhs, first["solution"],
                       first["condition"], Decimal("5e-12"),
                       Decimal("5e-12"), f"scale.{scale}")
        maximum_scaled_relative_residual = max(
            maximum_scaled_relative_residual,
            decimal(solve["relative_residual"]))

    mutation = probe["diagonalized_mutation"]
    diagonal = [[first["inertia"][row][column]
                 if row == column else Decimal(0)
                 for column in range(3)] for row in range(3)]
    diagonal_momentum = matrix_vector(diagonal, first["omega"])
    diagonal_gyro = cross(first["omega"], diagonal_momentum)
    diagonal_rhs = subtract(first["moment"], diagonal_gyro)
    diagonal_solution = solve_spd(diagonal, diagonal_rhs)
    verify_success(mutation["solve"], diagonal, diagonal_rhs,
                   diagonal_solution, condition_one(diagonal), absolute,
                   relative, "diagonalized-mutation")
    close_vector(mutation["angular_momentum"], diagonal_momentum,
                 absolute, relative, "diagonalized angular momentum")
    close_vector(mutation["gyroscopic_moment"], diagonal_gyro,
                 absolute, relative, "diagonalized gyroscopic moment")
    stored_mutation = next(
        value for value in oracle["mutation_results"]
        if value["id"] ==
        "MUTATION-YYZ-MASS-PROPERTIES-DIAGONALIZE-INERTIA")
    close_vector(mutation["solve"]["solution"],
                 vector(stored_mutation[
                     "observed_angular_acceleration_B_radps2"],
                        "stored mutation acceleration"),
                 absolute, relative, "stored mutation acceleration")
    diagonal_difference = max(abs(left - right) for left, right in zip(
        diagonal_solution, first["solution"]))
    close(mutation["max_acceleration_difference"], diagonal_difference,
          absolute, relative, "diagonalized acceleration difference")
    require(diagonal_difference > Decimal("0.9"),
            "diagonalized inertia mutation is not observable")

    near_symmetric = probe["near_symmetric"]
    require(near_symmetric["status"] == "Approximate" and
            near_symmetric["has_value"] is True and
            near_symmetric["flags"] == SYMMETRIZED_FLAG and
            near_symmetric["detail"] == "symmetrized-input",
            "near-symmetric solve semantics differ")
    near_matrix = matrix(near_symmetric["matrix_row_major"],
                         "near-symmetric matrix")
    projected = [row[:] for row in near_matrix]
    average = (near_matrix[0][1] + near_matrix[1][0]) / Decimal(2)
    projected[0][1] = average
    projected[1][0] = average
    projected_solution = solve_spd(
        projected, vector(near_symmetric["rhs"], "near-symmetric rhs"))
    close_vector(near_symmetric["solution"], projected_solution,
                 absolute, relative, "near-symmetric solution")

    expected_failures = {
        "INVALID-POLICY": ("DomainError", "policy"),
        "NONFINITE-MATRIX": ("NonFiniteInput", "matrix"),
        "NONFINITE-RHS": ("NonFiniteInput", "rhs"),
        "ASYMMETRIC": ("DomainError", "matrix-not-symmetric"),
        "NON-SPD": ("DomainError", "matrix-not-positive-definite"),
        "ZERO-PIVOT": ("Singular", "zero-pivot"),
        "ZERO-MATRIX": ("Singular", "zero-matrix"),
        "PIVOT-BELOW-THRESHOLD":
            ("Singular", "pivot-below-threshold"),
        "CONDITION-LIMIT": ("IllConditioned", "condition-limit"),
    }
    failures = {entry["id"]: entry for entry in probe["failure_cases"]}
    require(set(failures) == set(expected_failures),
            "SPD solve failure identities differ")
    for identifier, expected in expected_failures.items():
        status, detail = expected
        actual = failures[identifier]
        require(actual["status"] == status and
                actual["detail"] == detail and
                actual["has_value"] is False and
                actual["solution"] is None and actual["rank"] is None and
                actual["method"] is None,
                f"SPD solve failure differs for {identifier}")
    condition_failure = failures["CONDITION-LIMIT"]
    require(decimal(condition_failure["condition_estimate"]) >
            decimal(policy["condition_limit"]) and
            condition_failure["residual_norm"] is not None,
            "ill-conditioned failure lost evidence")

    return {
        "component_id": COMPONENT_ID,
        "status": "passed",
        "yyz_consumer_cases": len(product_cases),
        "scale_cases": len(scale_cases),
        "failure_cases": len(failures),
        "max_solution_error": str(maximum_solution_error),
        "max_condition_estimate_error": str(maximum_condition_error),
        "max_scaled_relative_residual":
            str(maximum_scaled_relative_residual),
        "condition_estimate_range": [
            str(min(condition_values)), str(max(condition_values))],
        "diagonalization_acceleration_difference":
            str(diagonal_difference),
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
        print(f"foundation SPD solve validation failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

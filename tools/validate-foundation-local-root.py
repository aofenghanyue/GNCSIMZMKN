#!/usr/bin/env python3
"""Independent Decimal comparator for the R1 local Newton root path."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import json
import math
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-CAVH-FORMULA-001"
ORACLE_ID = "ORACLE-CAVH-FORMULA-001"
PROBE_SCHEMA = "gnczmkn.foundation-local-root-probe/1"
COMPONENT_ID = "GNC-FOUNDATION-LOCAL-ROOT-001"
ALGORITHM_ID = "gnc.foundation.root.local-newton@1"
CLAMPED_FLAG = 1 << 1


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite Decimal value: {value}")
    return result


def polar(case: dict) -> dict[str, Decimal]:
    raw = case["polar"]
    return {key: decimal(raw[key]) for key in (
        "cl_intercept", "cl_slope_per_rad", "cd0_base",
        "cd0_slope_per_mach", "induced_drag_factor", "mach",
        "alpha_min_rad", "alpha_max_rad")}


def analytic_alpha_star(values: dict[str, Decimal]) -> Decimal:
    cd0 = (values["cd0_base"] +
           values["cd0_slope_per_mach"] * values["mach"])
    require(cd0 > 0 and values["induced_drag_factor"] > 0 and
            values["cl_slope_per_rad"] > 0,
            "polar is outside the analytic envelope domain")
    return ((cd0 / values["induced_drag_factor"]).sqrt() -
            values["cl_intercept"]) / values["cl_slope_per_rad"]


def derivative_linearization(
        values: dict[str, Decimal],
        alpha: Decimal) -> tuple[Decimal, Decimal]:
    cd0 = (values["cd0_base"] +
           values["cd0_slope_per_mach"] * values["mach"])
    slope = values["cl_slope_per_rad"]
    induced = values["induced_drag_factor"]
    lift = values["cl_intercept"] + slope * alpha
    lift_squared = lift * lift
    drag = cd0 + induced * lift_squared
    function_value = slope * (cd0 - induced * lift_squared) / (drag * drag)
    derivative_value = (
        -Decimal(2) * slope * slope * induced * lift *
        (Decimal(3) * cd0 - induced * lift_squared) /
        (drag * drag * drag))
    return function_value, derivative_value


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


def run_probe(path: Path) -> tuple[bytes, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"], check=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    return completed.stdout, json.loads(
        completed.stdout.decode("utf-8"), parse_float=Decimal)


def decimal_newton(values: dict[str, Decimal], initial: Decimal,
                   residual_tolerance: Decimal,
                   derivative_minimum: Decimal,
                   max_iterations: int) -> dict[str, object]:
    lower = values["alpha_min_rad"]
    upper = values["alpha_max_rad"]
    current = initial
    function_value, derivative_value = derivative_linearization(
        values, current)
    evaluations = 1
    if function_value == 0:
        return {
            "status": "Converged", "stop_reason": "ExactInitialGuess",
            "root": current, "function_value": function_value,
            "derivative_value": derivative_value, "iterations": 0,
            "evaluations": evaluations, "last_step": None,
        }
    if abs(function_value) <= residual_tolerance:
        return {
            "status": "Converged", "stop_reason": "ResidualTolerance",
            "root": current, "function_value": function_value,
            "derivative_value": derivative_value, "iterations": 0,
            "evaluations": evaluations, "last_step": None,
        }
    last_step: Decimal | None = None
    for iteration in range(max_iterations):
        require(abs(derivative_value) > derivative_minimum,
                "Decimal basin encountered a degenerate derivative")
        last_step = -function_value / derivative_value
        candidate = current + last_step
        if candidate < lower or candidate > upper:
            return {
                "status": "DomainError",
                "detail": "newton-step-outside-domain",
                "root": current, "function_value": function_value,
                "derivative_value": derivative_value,
                "iterations": iteration, "evaluations": evaluations,
                "last_step": last_step,
            }
        current = candidate
        function_value, derivative_value = derivative_linearization(
            values, current)
        evaluations += 1
        if function_value == 0:
            return {
                "status": "Converged", "stop_reason": "ExactEvaluation",
                "root": current, "function_value": function_value,
                "derivative_value": derivative_value,
                "iterations": iteration + 1,
                "evaluations": evaluations, "last_step": last_step,
            }
        if abs(function_value) <= residual_tolerance:
            return {
                "status": "Converged",
                "stop_reason": "ResidualTolerance",
                "root": current, "function_value": function_value,
                "derivative_value": derivative_value,
                "iterations": iteration + 1,
                "evaluations": evaluations, "last_step": last_step,
            }
    return {
        "status": "MaxIterations", "detail": "max-iterations",
        "root": current, "function_value": function_value,
        "derivative_value": derivative_value,
        "iterations": max_iterations, "evaluations": evaluations,
        "last_step": last_step,
    }


def verify(cases: dict, oracle: dict, probe_path: Path) -> dict:
    require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
            "CAVH fixture identity differs")
    require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
            "CAVH oracle identity differs")
    require(oracle["precision"]["decimal_digits"] >= 70,
            "CAVH stored oracle precision is below 70 digits")
    formula_absolute = decimal(cases["tolerances"]["formula_absolute"])
    formula_relative = decimal(cases["tolerances"]["formula_relative"])
    stored_absolute = decimal(
        cases["tolerances"]["stored_decimal_absolute"])

    polars: dict[str, dict[str, Decimal]] = {}
    analytic: dict[str, Decimal] = {}
    for case in cases["envelope_cases"]:
        identifier = case["id"]
        values = polar(case)
        root = analytic_alpha_star(values)
        stored_root = decimal(
            oracle["envelope_cases"][identifier]["alpha_star_rad"])
        close(root, stored_root, stored_absolute, Decimal(0),
              f"stored.{identifier}.alpha_star_rad")
        polars[identifier] = values
        analytic[identifier] = root

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    require(first_stdout == second_stdout and probe == second_probe,
            "local Newton probe reruns differ")
    require(probe["schema_version"] == PROBE_SCHEMA and
            probe["component_id"] == COMPONENT_ID and
            probe["fixture_id"] == FIXTURE_ID,
            "local Newton probe identity differs")
    require(probe["algorithm"] == {
        "id": ALGORITHM_ID,
        "version": "1.0.0",
    }, "local Newton algorithm identity differs")

    policy = probe["policy"]
    argument_absolute = decimal(policy["argument_absolute_tolerance"])
    argument_relative = decimal(policy["argument_relative_tolerance"])
    residual_absolute = decimal(policy["residual_absolute_tolerance"])
    derivative_minimum = decimal(policy["derivative_minimum_absolute"])
    close(argument_absolute, Decimal("1e-14"), Decimal("1e-29"),
          Decimal(0), "local Newton argument absolute tolerance")
    close(argument_relative, Decimal("1e-13"), Decimal("1e-28"),
          Decimal(0), "local Newton argument relative tolerance")
    close(residual_absolute, Decimal("1e-12"), Decimal("1e-27"),
          Decimal(0), "local Newton residual tolerance")
    close(derivative_minimum, Decimal("1e-14"), Decimal("1e-29"),
          Decimal(0), "local Newton derivative minimum")
    require(policy["max_iterations"] == 20,
            "local Newton iteration limit differs")

    samples = probe["basin_samples"]
    require(len(samples) == 38,
            "local Newton basin sample count differs")
    expected_grid = [Decimal(index) * Decimal("0.025")
                     for index in range(1, 20)]
    grouped: dict[str, list[dict]] = {identifier: []
                                     for identifier in polars}
    for sample in samples:
        require(sample["polar_case_id"] in grouped,
                "local Newton basin has an unknown polar")
        grouped[sample["polar_case_id"]].append(sample)
    require(all(len(group) == len(expected_grid)
                for group in grouped.values()),
            "local Newton initial grid length differs")
    for identifier, group in grouped.items():
        for index, (entry, expected_initial) in enumerate(
                zip(group, expected_grid)):
            close(entry["initial_guess"], expected_initial,
                  Decimal("2e-16"), Decimal("2e-16"),
                  f"local Newton initial grid {identifier}[{index}]")

    converged = 0
    outside_domain = 0
    maximum_iterations = 0
    maximum_root_error = Decimal(0)
    maximum_reference_state_error = Decimal(0)
    maximum_function_difference = Decimal(0)
    maximum_derivative_difference = Decimal(0)
    for identifier, group in grouped.items():
        values = polars[identifier]
        for sample in group:
            initial = decimal(sample["initial_guess"])
            expected = decimal_newton(
                values, initial, residual_absolute,
                derivative_minimum, policy["max_iterations"])
            outcome = sample["outcome"]
            require(outcome["status"] == expected["status"] and
                    outcome["iterations"] == expected["iterations"] and
                    outcome["evaluations"] == expected["evaluations"],
                    f"local Newton basin disposition differs for "
                    f"{identifier}@{initial}")
            if outcome["has_value"]:
                converged += 1
                maximum_iterations = max(maximum_iterations,
                                         outcome["iterations"])
                result = outcome["result"]
                require(outcome["status"] == "Converged" and
                        result is not None and outcome["flags"] == 0 and
                        result["domain"] == {
                            "lower": values["alpha_min_rad"],
                            "upper": values["alpha_max_rad"],
                        }, f"local Newton result metadata differs for "
                           f"{identifier}@{initial}")
                actual_root = decimal(result["root"])
                maximum_root_error = max(
                    maximum_root_error,
                    close(actual_root, analytic[identifier],
                          formula_absolute, formula_relative,
                          f"local Newton root {identifier}@{initial}"))
                maximum_reference_state_error = max(
                    maximum_reference_state_error,
                    close(actual_root, expected["root"],
                          Decimal("3e-14"), Decimal("3e-14"),
                          f"Decimal Newton root {identifier}@{initial}"))
                function_value, derivative_value = derivative_linearization(
                    values, actual_root)
                maximum_function_difference = max(
                    maximum_function_difference,
                    close(result["function_value"], function_value,
                          formula_absolute, formula_relative,
                          f"local Newton function {identifier}@{initial}"))
                maximum_derivative_difference = max(
                    maximum_derivative_difference,
                    close(result["derivative_value"], derivative_value,
                          formula_absolute, formula_relative,
                          f"local Newton derivative {identifier}@{initial}"))
                close(outcome["residual_norm"], abs(function_value),
                      formula_absolute, formula_relative,
                      f"local Newton residual {identifier}@{initial}")
                require(result["stop_reason"] in (
                    "ExactInitialGuess", "ExactEvaluation",
                    "ResidualTolerance", "StepTolerance"),
                    "local Newton stop reason differs")
            else:
                outside_domain += 1
                require(outcome["status"] == "DomainError" and
                        outcome["detail"] ==
                        "newton-step-outside-domain" and
                        outcome["result"] is None,
                        f"local Newton basin failure differs for "
                        f"{identifier}@{initial}")
                close(outcome["residual_norm"],
                      abs(expected["function_value"]),
                      formula_absolute, formula_relative,
                      f"local Newton failed residual {identifier}@{initial}")
                close(outcome["last_step"], expected["last_step"],
                      Decimal("3e-12"), Decimal("3e-12"),
                      f"local Newton failed step {identifier}@{initial}")
    require(converged == 29 and outside_domain == 9 and
            maximum_iterations == 7,
            "local Newton basin summary differs")

    constant_at_04 = next(
        sample for sample in grouped[
            "CASE-CAVH-ENVELOPE-CONSTANT-POLAR"]
        if abs(decimal(sample["initial_guess"]) - Decimal("0.4")) <=
        Decimal("2e-16"))
    mach_at_04 = next(
        sample for sample in grouped[
            "CASE-CAVH-ENVELOPE-MACH-DEPENDENT"]
        if abs(decimal(sample["initial_guess"]) - Decimal("0.4")) <=
        Decimal("2e-16"))
    require(constant_at_04["outcome"]["status"] == "DomainError" and
            mach_at_04["outcome"]["status"] == "Converged",
            "model-dependent CAVH initial sensitivity differs")

    traces = probe["convergence_traces"]
    require(len(traces) == 2 and
            {trace["polar_case_id"] for trace in traces} == set(polars),
            "local Newton convergence traces differ")
    maximum_recurrence_error = Decimal(0)
    minimum_asymptotic_order: Decimal | None = None
    for trace in traces:
        identifier = trace["polar_case_id"]
        values = polars[identifier]
        trace_samples = trace["samples"]
        outcome = trace["outcome"]
        require(abs(decimal(trace["initial_guess"]) - Decimal("0.1")) <=
                Decimal("2e-16") and outcome["status"] == "Converged" and
                outcome["evaluations"] == len(trace_samples),
                f"local Newton trace metadata differs for {identifier}")
        errors: list[Decimal] = []
        for index, sample in enumerate(trace_samples):
            argument = decimal(sample["argument"])
            function_value, derivative_value = derivative_linearization(
                values, argument)
            maximum_function_difference = max(
                maximum_function_difference,
                close(sample["function_value"], function_value,
                      formula_absolute, formula_relative,
                      f"trace function {identifier}[{index}]"))
            maximum_derivative_difference = max(
                maximum_derivative_difference,
                close(sample["derivative_value"], derivative_value,
                      formula_absolute, formula_relative,
                      f"trace derivative {identifier}[{index}]"))
            error = abs(argument - analytic[identifier])
            close(sample["absolute_root_error"], error,
                  Decimal("2e-16"), Decimal("2e-16"),
                  f"trace root error {identifier}[{index}]")
            errors.append(error)
            if index + 1 < len(trace_samples):
                expected_next = argument - function_value / derivative_value
                maximum_recurrence_error = max(
                    maximum_recurrence_error,
                    close(trace_samples[index + 1]["argument"],
                          expected_next, Decimal("3e-14"), Decimal("3e-14"),
                          f"Newton recurrence {identifier}[{index}]"))
        require(all(errors[index] < errors[index - 1]
                    for index in range(1, len(errors))),
                f"local Newton trace error is not monotone for {identifier}")
        orders: list[Decimal] = []
        for index in range(3, len(errors)):
            previous, current, following = errors[index-2:index+1]
            if (previous > 0 and current > 0 and
                    following > Decimal("1e-14") and
                    following < current < previous):
                order = Decimal(str(
                    math.log(float(following / current)) /
                    math.log(float(current / previous))))
                orders.append(order)
        require(orders and min(orders) >= Decimal("1.8"),
                f"local Newton asymptotic order differs for {identifier}")
        reported_order = decimal(trace["minimum_asymptotic_order"])
        close(reported_order, min(orders), Decimal("2e-12"),
              Decimal("2e-12"),
              f"reported local Newton order for {identifier}")
        minimum_asymptotic_order = (
            reported_order if minimum_asymptotic_order is None else
            min(minimum_asymptotic_order, reported_order))

    successes = {entry["id"]: entry
                 for entry in probe["success_semantics"]}
    require(set(successes) == {
        "EXACT-INITIAL", "STEP-TOLERANCE",
        "APPROXIMATE-FLAG-PROPAGATION"},
        "local Newton success identities differ")
    require(successes["EXACT-INITIAL"]["status"] == "Converged" and
            successes["EXACT-INITIAL"]["iterations"] == 0 and
            successes["EXACT-INITIAL"]["evaluations"] == 1 and
            successes["EXACT-INITIAL"]["result"]["stop_reason"] ==
            "ExactInitialGuess",
            "local Newton exact-initial semantics differ")
    require(successes["STEP-TOLERANCE"]["status"] == "Converged" and
            successes["STEP-TOLERANCE"]["result"]["stop_reason"] ==
            "StepTolerance" and
            abs(decimal(successes["STEP-TOLERANCE"]
                        ["result"]["function_value"])) > 0,
            "local Newton step-tolerance semantics differ")
    require(successes["APPROXIMATE-FLAG-PROPAGATION"]["status"] ==
            "Approximate" and
            successes["APPROXIMATE-FLAG-PROPAGATION"]["flags"] ==
            CLAMPED_FLAG and
            successes["APPROXIMATE-FLAG-PROPAGATION"]["has_value"] is True,
            "local Newton callback flag propagation differs")

    expected_failures = {
        "INVALID-POLICY": ("DomainError", "policy", 0, 0),
        "NONFINITE-DOMAIN": ("NonFiniteInput", "domain", 0, 0),
        "INVALID-DOMAIN": ("DomainError", "domain", 0, 0),
        "NONFINITE-INITIAL":
            ("NonFiniteInput", "initial-guess", 0, 0),
        "INITIAL-OUTSIDE-DOMAIN":
            ("DomainError", "initial-guess-outside-domain", 0, 0),
        "DERIVATIVE-DEGENERATE":
            ("Singular", "derivative-below-threshold", 1, 0),
        "NONFINITE-FUNCTION-VALUE":
            ("NonFiniteIntermediate", "function-value", 1, 0),
        "NONFINITE-DERIVATIVE":
            ("NonFiniteIntermediate", "function-derivative", 1, 0),
        "CALLBACK-DOMAIN": ("DomainError", "polar-domain", 1, 0),
        "CAVH-STEP-OUTSIDE-DOMAIN":
            ("DomainError", "newton-step-outside-domain", 1, 0),
        "TOLERANCE-UNREACHABLE":
            ("ToleranceUnreachable", "no-representable-newton-step", 1, 0),
        "CALLBACK-CANDIDATE-DOMAIN":
            ("DomainError", "candidate-domain", 2, 1),
        "MAX-ITERATIONS": ("MaxIterations", "max-iterations", 3, 2),
        "NONFINITE-NEWTON-STEP":
            ("NonFiniteIntermediate", "newton-step", 1, 0),
        "NONFINITE-CANDIDATE":
            ("NonFiniteIntermediate", "newton-candidate", 1, 0),
    }
    failures = {entry["id"]: entry for entry in probe["failure_cases"]}
    require(set(failures) == set(expected_failures),
            "local Newton failure identities differ")
    for identifier, expected in expected_failures.items():
        status, detail, evaluations, iterations = expected
        outcome = failures[identifier]
        require(outcome["status"] == status and
                outcome["detail"] == detail and
                outcome["evaluations"] == evaluations and
                outcome["iterations"] == iterations and
                outcome["has_value"] is False and
                outcome["result"] is None,
                f"local Newton failure differs for {identifier}")

    return {
        "component_id": COMPONENT_ID,
        "status": "passed",
        "basin_samples": len(samples),
        "converged_initials": converged,
        "domain_rejected_initials": outside_domain,
        "maximum_successful_iterations": maximum_iterations,
        "minimum_asymptotic_order": str(minimum_asymptotic_order),
        "max_root_error_rad": str(maximum_root_error),
        "max_decimal_newton_state_error_rad":
            str(maximum_reference_state_error),
        "max_function_difference": str(maximum_function_difference),
        "max_derivative_difference": str(maximum_derivative_difference),
        "max_recurrence_error_rad": str(maximum_recurrence_error),
        "failure_cases": len(failures),
        "initial_0_4_disposition": {
            "constant_polar": constant_at_04["outcome"]["status"],
            "mach_dependent_polar": mach_at_04["outcome"]["status"],
        },
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
        print(f"foundation local Newton validation failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

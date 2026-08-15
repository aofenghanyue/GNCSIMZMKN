#!/usr/bin/env python3
"""Independent Decimal comparator for the R1 bracketed scalar root path."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-CAVH-FORMULA-001"
ORACLE_ID = "ORACLE-CAVH-FORMULA-001"
PROBE_SCHEMA = "gnczmkn.foundation-root-probe/1"
COMPONENT_ID = "GNC-FOUNDATION-ROOT-001"
ALGORITHM_ID = "gnc.foundation.root.bracketed-bisection@1"


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


def lift_to_drag_derivative(values: dict[str, Decimal],
                            alpha: Decimal) -> Decimal:
    cd0 = (values["cd0_base"] +
           values["cd0_slope_per_mach"] * values["mach"])
    lift = values["cl_intercept"] + values["cl_slope_per_rad"] * alpha
    drag = cd0 + values["induced_drag_factor"] * lift * lift
    return (values["cl_slope_per_rad"] *
            (cd0 - values["induced_drag_factor"] * lift * lift) /
            (drag * drag))


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
        require(values["alpha_min_rad"] <= root <=
                values["alpha_max_rad"],
                f"analytic root is outside {identifier} domain")
        polars[identifier] = values
        analytic[identifier] = root

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    require(first_stdout == second_stdout and probe == second_probe,
            "root probe reruns differ")
    require(probe["schema_version"] == PROBE_SCHEMA and
            probe["component_id"] == COMPONENT_ID and
            probe["fixture_id"] == FIXTURE_ID,
            "root probe identity differs")
    require(probe["algorithm"] == {
        "id": ALGORITHM_ID,
        "version": "1.0.0",
    }, "root algorithm identity differs")

    policy = probe["policy"]
    argument_absolute = decimal(policy["argument_absolute_tolerance"])
    argument_relative = decimal(policy["argument_relative_tolerance"])
    residual_absolute = decimal(policy["residual_absolute_tolerance"])
    close(argument_absolute, Decimal("1e-14"), Decimal("1e-29"),
          Decimal(0), "root policy argument absolute tolerance")
    close(argument_relative, Decimal("1e-13"), Decimal("1e-28"),
          Decimal(0), "root policy argument relative tolerance")
    close(residual_absolute, Decimal("1e-12"), Decimal("1e-27"),
          Decimal(0), "root policy residual absolute tolerance")
    require(policy["max_iterations"] == 80,
            "root policy iteration limit differs")

    product_cases = {entry["id"]: entry
                     for entry in probe["cavh_cases"]}
    require(set(product_cases) == set(analytic),
            "root product case identities differ")
    maximum_root_error = Decimal(0)
    maximum_derivative_difference = Decimal(0)
    for identifier, expected_root in analytic.items():
        outcome = product_cases[identifier]
        require(outcome["status"] == "Converged" and
                outcome["has_value"] is True and
                outcome["result"] is not None,
                f"root outcome differs for {identifier}")
        result = outcome["result"]
        actual_root = decimal(result["root"])
        maximum_root_error = max(
            maximum_root_error,
            close(actual_root, expected_root, formula_absolute,
                  formula_relative, f"product.{identifier}.root"))
        bracket = result["bracket"]
        lower = decimal(bracket["lower"])
        upper = decimal(bracket["upper"])
        require(lower <= expected_root <= upper,
                f"product bracket lost the root for {identifier}")
        independent_residual = lift_to_drag_derivative(
            polars[identifier], actual_root)
        maximum_derivative_difference = max(
            maximum_derivative_difference,
            close(result["function_value"], independent_residual,
                  formula_absolute, formula_relative,
                  f"product.{identifier}.function_value"))
        close(outcome["residual_norm"], abs(independent_residual),
              formula_absolute, formula_relative,
              f"product.{identifier}.residual_norm")

        stop_reason = result["stop_reason"]
        if stop_reason in ("ExactLowerEndpoint", "ExactUpperEndpoint",
                           "ExactEvaluation"):
            require(decimal(result["function_value"]) == 0,
                    f"exact stop has nonzero residual for {identifier}")
        elif stop_reason == "ResidualTolerance":
            require(abs(decimal(result["function_value"])) <=
                    residual_absolute,
                    f"residual stop exceeds tolerance for {identifier}")
        elif stop_reason == "BracketTolerance":
            x_limit = argument_absolute + argument_relative * max(
                abs(lower), abs(upper))
            require(upper - lower <= x_limit,
                    f"bracket stop exceeds tolerance for {identifier}")
        else:
            raise ValueError(
                f"unsupported root stop reason for {identifier}: "
                f"{stop_reason}")

    endpoint = probe["endpoint_case"]
    require(endpoint["status"] == "Converged" and
            endpoint["result"]["root"] == 0 and
            endpoint["result"]["stop_reason"] == "ExactLowerEndpoint" and
            endpoint["iterations"] == 0 and endpoint["evaluations"] == 1,
            "exact endpoint root semantics differ")
    extreme = probe["extreme_bracket_case"]
    require(extreme["status"] == "Converged" and
            extreme["result"]["root"] == 0 and
            extreme["result"]["stop_reason"] == "ExactEvaluation" and
            extreme["iterations"] == 1 and extreme["evaluations"] == 3,
            "extreme finite bracket midpoint semantics differ")

    mach_id = "CASE-CAVH-ENVELOPE-MACH-DEPENDENT"
    mach_root = analytic[mach_id]
    ladder = probe["convergence_ladder"]
    expected_budgets = [4, 8, 12, 16, 20, 24, 28, 32]
    require([entry["max_iterations"] for entry in ladder] ==
            expected_budgets,
            "root convergence budgets differ")
    previous_width: Decimal | None = None
    final_midpoint_error = Decimal(0)
    for entry in ladder:
        budget = entry["max_iterations"]
        outcome = entry["outcome"]
        require(outcome["status"] == "MaxIterations" and
                outcome["has_value"] is False and
                outcome["result"] is None and
                outcome["detail"] == "max-iterations" and
                outcome["iterations"] == budget and
                outcome["evaluations"] == budget + 2,
                f"root convergence outcome differs at budget {budget}")
        bracket = outcome["last_bracket"]
        lower = decimal(bracket["lower"])
        upper = decimal(bracket["upper"])
        require(lower <= mach_root <= upper,
                f"root convergence bracket lost the root at {budget}")
        width = upper - lower
        expected_width = Decimal("0.5") / (Decimal(2) ** budget)
        close(width, expected_width, Decimal("2e-16"), Decimal(0),
              f"root convergence width at {budget}")
        reported_width = decimal(entry["bracket_width_rad"])
        close(reported_width, width, Decimal("2e-16"),
              Decimal(0), f"reported root width at {budget}")
        midpoint_error = abs((lower + upper) / Decimal(2) - mach_root)
        close(entry["midpoint_error_rad"], midpoint_error,
              Decimal("2e-16"), Decimal(0),
              f"reported midpoint error at {budget}")
        require(midpoint_error <= width / Decimal(2),
                f"midpoint error exceeds bracket bound at {budget}")
        endpoint_residual = min(
            abs(lift_to_drag_derivative(polars[mach_id], lower)),
            abs(lift_to_drag_derivative(polars[mach_id], upper)))
        close(outcome["residual_norm"], endpoint_residual,
              formula_absolute, formula_relative,
              f"root convergence residual at {budget}")
        if previous_width is not None:
            close(previous_width / reported_width, Decimal(16),
                  Decimal("1e-12"),
                  Decimal(0), f"root width reduction at {budget}")
        previous_width = reported_width
        final_midpoint_error = midpoint_error

    expected_failures = {
        "NO-BRACKET": ("NoBracket", "same-sign-endpoints", 2, 0),
        "INVALID-BRACKET": ("DomainError", "bracket", 0, 0),
        "NONFINITE-BOUND": ("NonFiniteInput", "bracket", 0, 0),
        "NONFINITE-MIDPOINT":
            ("NonFiniteIntermediate", "function-value", 3, 1),
        "CALLBACK-DOMAIN": ("DomainError", "polar-domain", 3, 1),
        "TOLERANCE-UNREACHABLE":
            ("ToleranceUnreachable", "no-representable-midpoint", 2, 0),
        "INVALID-POLICY": ("DomainError", "policy", 0, 0),
    }
    failures = {entry["id"]: entry for entry in probe["failure_cases"]}
    require(set(failures) == set(expected_failures),
            "root failure identities differ")
    for identifier, expected in expected_failures.items():
        status, detail, evaluations, iterations = expected
        actual = failures[identifier]
        require(actual["status"] == status and
                actual["detail"] == detail and
                actual["evaluations"] == evaluations and
                actual["iterations"] == iterations and
                actual["has_value"] is False and
                actual["result"] is None,
                f"root failure outcome differs for {identifier}")
    unreachable = failures["TOLERANCE-UNREACHABLE"]["last_bracket"]
    require(decimal(unreachable["lower"]) < decimal(unreachable["upper"]),
            "unreachable tolerance did not retain adjacent bracket")

    return {
        "component_id": COMPONENT_ID,
        "status": "passed",
        "cavh_root_cases": len(product_cases),
        "mach_iterations": product_cases[mach_id]["iterations"],
        "width_reduction_per_four_iterations": "16",
        "final_ladder_width_rad": str(previous_width),
        "final_ladder_midpoint_error_rad": str(final_midpoint_error),
        "max_root_error_rad": str(maximum_root_error),
        "max_derivative_difference": str(maximum_derivative_difference),
        "failure_cases": len(failures),
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
        print(f"foundation root validation failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

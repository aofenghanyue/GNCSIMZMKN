#!/usr/bin/env python3
"""Independent Decimal comparator for scaled central differentiation."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import json
from pathlib import Path
import subprocess
import sys
from typing import Callable


FIXTURE_ID = "REF-CAVH-FORMULA-001"
ORACLE_ID = "ORACLE-CAVH-FORMULA-001"
PROBE_SCHEMA = "gnczmkn.foundation-differentiation-probe/1"
COMPONENT_ID = "GNC-FOUNDATION-DIFFERENTIATION-001"
ALGORITHM_ID = "gnc.foundation.differentiation.scaled-central@1"
CLAMPED_FLAG = 1 << 1


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite Decimal value: {value}")
    return result


def close(
    actual: object,
    expected: object,
    absolute: Decimal,
    relative: Decimal,
    label: str,
) -> Decimal:
    actual_value = decimal(actual)
    expected_value = decimal(expected)
    difference = abs(actual_value - expected_value)
    limit = absolute + relative * max(
        Decimal(1), abs(actual_value), abs(expected_value)
    )
    require(
        difference <= limit,
        f"{label} differs: {actual_value} vs {expected_value}",
    )
    return difference


def run_probe(path: Path) -> tuple[bytes, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return completed.stdout, json.loads(
        completed.stdout.decode("utf-8"), parse_float=Decimal
    )


def central_difference(
    function: Callable[[Decimal], Decimal],
    point: Decimal,
    step: Decimal,
) -> Decimal:
    require(step > 0, "Decimal central step must be positive")
    return (function(point + step) - function(point - step)) / (
        Decimal(2) * step
    )


def verify_outcome_shape(outcome: dict, label: str) -> dict:
    require(
        outcome["status"] == "Success"
        and outcome["flags"] == 0
        and outcome["has_value"] is True
        and outcome["evaluations"] == 2
        and outcome["detail"] == "scaled-central-difference"
        and outcome["result"] is not None,
        f"{label} outcome metadata differs",
    )
    result = outcome["result"]
    close(
        outcome["last_step"],
        result["effective_step"],
        Decimal("1e-30"),
        Decimal("1e-16"),
        f"{label}.last_step",
    )
    selected = max(
        abs(decimal(result["point"])),
        decimal(result["nominal_argument_scale"]),
    )
    close(
        result["selected_argument_scale"],
        selected,
        Decimal("1e-30"),
        Decimal("1e-16"),
        f"{label}.selected_argument_scale",
    )
    close(
        result["requested_step"],
        decimal(result["relative_step"]) * selected,
        Decimal("2e-15"),
        Decimal("2e-16"),
        f"{label}.requested_step_selection",
    )
    endpoint_lower_step = (
        decimal(result["point"]) - decimal(result["lower_argument"])
    )
    endpoint_upper_step = (
        decimal(result["upper_argument"]) - decimal(result["point"])
    )
    lower_step = decimal(result["lower_step"])
    upper_step = decimal(result["upper_step"])
    effective = (lower_step + upper_step) / Decimal(2)
    close(
        lower_step,
        endpoint_lower_step,
        Decimal("5e-12"),
        Decimal("2e-16"),
        f"{label}.lower_step",
    )
    close(
        upper_step,
        endpoint_upper_step,
        Decimal("5e-12"),
        Decimal("2e-16"),
        f"{label}.upper_step",
    )
    close(
        result["effective_step"],
        effective,
        Decimal("2e-15"),
        Decimal("2e-16"),
        f"{label}.effective_step",
    )
    risk = result["risk"]
    close(
        risk["normalized_step"],
        effective / selected,
        Decimal("2e-18"),
        Decimal("2e-16"),
        f"{label}.normalized_step",
    )
    lower_value = decimal(result["lower_value"])
    upper_value = decimal(result["upper_value"])
    value_scale = max(abs(lower_value), abs(upper_value))
    cancellation = (
        abs(upper_value - lower_value) / value_scale
        if value_scale > 0
        else Decimal(0)
    )
    close(
        risk["output_cancellation_ratio"],
        cancellation,
        Decimal("3e-17"),
        Decimal("3e-16"),
        f"{label}.output_cancellation_ratio",
    )
    asymmetry = abs(upper_step - lower_step) / effective
    close(
        risk["step_asymmetry_ratio"],
        asymmetry,
        Decimal("3e-14"),
        Decimal("2e-16"),
        f"{label}.step_asymmetry_ratio",
    )
    return result


def verify(cases: dict, oracle: dict, probe_path: Path) -> dict:
    require(
        cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
        "CAVH fixture identity differs",
    )
    require(
        cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
        "CAVH oracle identity differs",
    )
    require(
        oracle["precision"]["decimal_digits"] >= 70,
        "CAVH stored oracle precision is below 70 digits",
    )

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    require(
        first_stdout == second_stdout and probe == second_probe,
        "differentiation probe reruns differ",
    )
    require(
        probe["schema_version"] == PROBE_SCHEMA
        and probe["component_id"] == COMPONENT_ID
        and probe["fixture_id"] == FIXTURE_ID,
        "differentiation probe identity differs",
    )
    require(
        probe["algorithm"]
        == {"id": ALGORITHM_ID, "version": "1.0.0"},
        "differentiation algorithm identity differs",
    )
    close(
        probe["default_policy"]["argument_scale"],
        Decimal(1),
        Decimal(0),
        Decimal(0),
        "default argument scale",
    )
    close(
        probe["default_policy"]["relative_step"],
        Decimal("6.0554544523933429e-6"),
        Decimal("1e-22"),
        Decimal(0),
        "default relative step",
    )

    derivative_case = cases["derivative_case"]
    derivative_oracle = oracle["derivative_case"]
    atmosphere = derivative_case["atmosphere"]
    altitude = decimal(derivative_case["altitude_m"])
    speed = decimal(derivative_case["speed_mps"])
    rho0 = decimal(atmosphere["sea_level_density_kg_per_m3"])
    scale_height = decimal(atmosphere["density_scale_height_m"])
    sound = decimal(atmosphere["speed_of_sound_mps"])
    sound_gradient = decimal(
        atmosphere["speed_of_sound_gradient_per_m"]
    )

    def density_function(argument: Decimal) -> Decimal:
        return rho0 * (-argument / scale_height).exp()

    def mach_altitude_function(argument: Decimal) -> Decimal:
        local_sound = sound + sound_gradient * (argument - altitude)
        require(local_sound > 0, "Decimal sound speed is outside domain")
        return speed / local_sound

    def cl_star_function(argument: Decimal) -> Decimal:
        radicand = (Decimal("0.02") + Decimal("0.001") * argument) / Decimal(
            "0.08"
        )
        require(radicand >= 0, "Decimal CL-star radicand is outside domain")
        return radicand.sqrt()

    analytic = derivative_oracle["analytic"]
    ladder_specs = {
        "DENSITY-ALTITUDE": {
            "point": altitude,
            "domain": {"lower": Decimal(0), "upper": Decimal("60000")},
            "scale": altitude,
            "steps": [
                decimal(value)
                for value in derivative_case[
                    "density_altitude_step_ladder_m"
                ]
            ],
            "analytic": decimal(analytic["density_gradient_kg_per_m4"]),
            "function": density_function,
            "stored": derivative_oracle["density_gradient_ladder"],
            "stored_ratios": derivative_oracle[
                "density_error_reduction_ratios"
            ],
        },
        "MACH-ALTITUDE": {
            "point": altitude,
            "domain": {"lower": Decimal(0), "upper": Decimal("60000")},
            "scale": altitude,
            "steps": [
                decimal(value)
                for value in derivative_case[
                    "mach_altitude_step_ladder_m"
                ]
            ],
            "analytic": decimal(
                analytic["partial_mach_partial_altitude_per_m"]
            ),
            "function": mach_altitude_function,
            "stored": derivative_oracle["mach_altitude_ladder"],
            "stored_ratios": derivative_oracle[
                "mach_error_reduction_ratios"
            ],
        },
        "CL-STAR-MACH": {
            "point": Decimal(10),
            "domain": {"lower": Decimal(0), "upper": Decimal(20)},
            "scale": Decimal(10),
            "steps": [
                decimal(value)
                for value in derivative_case["cl_star_mach_step_ladder"]
            ],
            "analytic": decimal(analytic["dcl_star_dmach"]),
            "function": cl_star_function,
            "stored": derivative_oracle["cl_star_mach_ladder"],
            "stored_ratios": derivative_oracle[
                "cl_star_error_reduction_ratios"
            ],
        },
    }

    ladders = {entry["id"]: entry for entry in probe["convergence_ladders"]}
    require(
        set(ladders) == set(ladder_specs),
        "differentiation ladder identities differ",
    )
    maximum_stored_difference = Decimal(0)
    maximum_decimal_formula_difference = Decimal(0)
    minimum_reduction_ratio: Decimal | None = None
    convergence_samples = 0
    for identifier, spec in ladder_specs.items():
        ladder = ladders[identifier]
        close(
            ladder["point"],
            spec["point"],
            Decimal("1e-24"),
            Decimal(0),
            f"{identifier}.point",
        )
        require(
            ladder["domain"] == spec["domain"],
            f"{identifier} differentiation domain differs",
        )
        close(
            ladder["argument_scale"],
            spec["scale"],
            Decimal("1e-24"),
            Decimal(0),
            f"{identifier}.argument_scale",
        )
        close(
            ladder["analytic_derivative"],
            spec["analytic"],
            Decimal("2e-18"),
            Decimal("2e-16"),
            f"{identifier}.analytic_derivative",
        )
        samples = ladder["samples"]
        require(
            len(samples) == len(spec["steps"]) == len(spec["stored"]) == 5,
            f"{identifier} differentiation sample count differs",
        )
        errors: list[Decimal] = []
        for index, (sample, step, stored) in enumerate(
            zip(samples, spec["steps"], spec["stored"])
        ):
            convergence_samples += 1
            close(
                sample["requested_step"],
                step,
                Decimal("2e-15"),
                Decimal("2e-16"),
                f"{identifier}[{index}].requested_step",
            )
            result = verify_outcome_shape(
                sample["outcome"], f"{identifier}[{index}]"
            )
            close(
                result["nominal_argument_scale"],
                spec["scale"],
                Decimal("1e-24"),
                Decimal(0),
                f"{identifier}[{index}].nominal_scale",
            )
            lower_argument = decimal(result["lower_argument"])
            upper_argument = decimal(result["upper_argument"])
            decimal_lower = spec["function"](lower_argument)
            decimal_upper = spec["function"](upper_argument)
            maximum_decimal_formula_difference = max(
                maximum_decimal_formula_difference,
                close(
                    result["lower_value"],
                    decimal_lower,
                    Decimal("3e-15"),
                    Decimal("3e-15"),
                    f"{identifier}[{index}].lower_value",
                ),
                close(
                    result["upper_value"],
                    decimal_upper,
                    Decimal("3e-15"),
                    Decimal("3e-15"),
                    f"{identifier}[{index}].upper_value",
                ),
            )
            decimal_estimate = (decimal_upper - decimal_lower) / (
                upper_argument - lower_argument
            )
            maximum_decimal_formula_difference = max(
                maximum_decimal_formula_difference,
                close(
                    result["derivative"],
                    decimal_estimate,
                    Decimal("5e-14"),
                    Decimal("5e-14"),
                    f"{identifier}[{index}].decimal_derivative",
                ),
            )
            maximum_stored_difference = max(
                maximum_stored_difference,
                close(
                    result["derivative"],
                    stored["estimate"],
                    Decimal("5e-14"),
                    Decimal("5e-14"),
                    f"{identifier}[{index}].stored_estimate",
                ),
            )
            errors.append(abs(decimal(result["derivative"]) - spec["analytic"]))
        require(
            all(errors[index] > errors[index + 1] for index in range(4)),
            f"{identifier} central difference errors are not monotone",
        )
        ratios = [errors[index] / errors[index + 1] for index in range(4)]
        require(
            all(Decimal("3.8") < ratio < Decimal("4.2") for ratio in ratios),
            f"{identifier} central difference order differs",
        )
        for index, ratio in enumerate(ratios):
            close(
                ladder["error_reduction_ratios"][index],
                ratio,
                Decimal("5e-8"),
                Decimal("5e-8"),
                f"{identifier}.reported_ratio[{index}]",
            )
            close(
                ratio,
                spec["stored_ratios"][index],
                Decimal("4e-4"),
                Decimal("4e-4"),
                f"{identifier}.stored_ratio[{index}]",
            )
            minimum_reduction_ratio = (
                ratio
                if minimum_reduction_ratio is None
                else min(minimum_reduction_ratio, ratio)
            )

    transitions = {
        entry["id"]: entry for entry in probe["roundoff_transitions"]
    }
    require(
        set(transitions) == set(ladder_specs),
        "roundoff transition identities differ",
    )
    expected_transition_steps = {
        "DENSITY-ALTITUDE": (Decimal("0.1"), Decimal("1e-9")),
        "MACH-ALTITUDE": (Decimal("1"), Decimal("1e-9")),
        "CL-STAR-MACH": (Decimal("1e-4"), Decimal("1e-12")),
    }
    minimum_roundoff_amplification: Decimal | None = None
    smallest_cancellation_ratio: Decimal | None = None
    for identifier, transition in transitions.items():
        spec = ladder_specs[identifier]
        well_step, tiny_step = expected_transition_steps[identifier]
        close(
            transition["analytic_derivative"],
            spec["analytic"],
            Decimal("2e-18"),
            Decimal("2e-16"),
            f"{identifier}.transition_analytic",
        )
        well_sample = transition["well_scaled"]
        tiny_sample = transition["tiny_step"]
        close(
            well_sample["requested_step"],
            well_step,
            Decimal("2e-15"),
            Decimal("2e-16"),
            f"{identifier}.well_step",
        )
        close(
            tiny_sample["requested_step"],
            tiny_step,
            Decimal("2e-24"),
            Decimal("2e-16"),
            f"{identifier}.tiny_step",
        )
        well_result = verify_outcome_shape(
            well_sample["outcome"], f"{identifier}.well"
        )
        tiny_result = verify_outcome_shape(
            tiny_sample["outcome"], f"{identifier}.tiny"
        )
        actual_well_error = abs(
            decimal(well_result["derivative"]) - spec["analytic"]
        )
        actual_tiny_error = abs(
            decimal(tiny_result["derivative"]) - spec["analytic"]
        )
        require(
            actual_tiny_error
            > Decimal(1000) * max(actual_well_error, Decimal("1e-30")),
            f"{identifier} double roundoff transition is missing",
        )
        ideal_well = central_difference(
            spec["function"], spec["point"], well_step
        )
        ideal_tiny = central_difference(
            spec["function"], spec["point"], tiny_step
        )
        ideal_well_error = abs(ideal_well - spec["analytic"])
        ideal_tiny_error = abs(ideal_tiny - spec["analytic"])
        require(
            Decimal(0) < ideal_tiny_error < ideal_well_error,
            f"{identifier} Decimal truncation trend differs",
        )
        amplification = actual_tiny_error / ideal_tiny_error
        require(
            amplification > Decimal("1e12"),
            f"{identifier} roundoff amplification is too small",
        )
        minimum_roundoff_amplification = (
            amplification
            if minimum_roundoff_amplification is None
            else min(minimum_roundoff_amplification, amplification)
        )
        well_cancellation = decimal(
            well_result["risk"]["output_cancellation_ratio"]
        )
        tiny_cancellation = decimal(
            tiny_result["risk"]["output_cancellation_ratio"]
        )
        require(
            Decimal(0) < tiny_cancellation < well_cancellation,
            f"{identifier} cancellation risk trend differs",
        )
        smallest_cancellation_ratio = (
            tiny_cancellation
            if smallest_cancellation_ratio is None
            else min(smallest_cancellation_ratio, tiny_cancellation)
        )

    scale_cases = {entry["id"]: entry for entry in probe["scale_selection"]}
    require(
        set(scale_cases)
        == {
            "ZERO-POINT-NOMINAL-SCALE",
            "POINT-MAGNITUDE-SCALE",
            "DEFAULT-POLICY-CL-STAR",
        },
        "scale selection identities differ",
    )
    zero_scale = verify_outcome_shape(
        scale_cases["ZERO-POINT-NOMINAL-SCALE"], "zero-scale"
    )
    point_scale = verify_outcome_shape(
        scale_cases["POINT-MAGNITUDE-SCALE"], "point-scale"
    )
    default_scale = verify_outcome_shape(
        scale_cases["DEFAULT-POLICY-CL-STAR"], "default-scale"
    )
    require(
        decimal(zero_scale["selected_argument_scale"]) == Decimal(20)
        and decimal(zero_scale["requested_step"]) == Decimal("2.5")
        and decimal(zero_scale["derivative"]) == Decimal(3)
        and decimal(point_scale["selected_argument_scale"]) == Decimal(100)
        and decimal(point_scale["requested_step"]) == Decimal("12.5")
        and decimal(point_scale["derivative"]) == Decimal(3)
        and decimal(default_scale["selected_argument_scale"])
        == Decimal(10)
        and abs(
            decimal(default_scale["derivative"])
            - ladder_specs["CL-STAR-MACH"]["analytic"]
        )
        < Decimal("2e-10"),
        "scaled step selection differs",
    )
    close(
        default_scale["requested_step"],
        Decimal(10) * decimal(probe["default_policy"]["relative_step"]),
        Decimal("2e-20"),
        Decimal("2e-16"),
        "default scale requested step",
    )

    successes = {
        entry["id"]: entry for entry in probe["success_semantics"]
    }
    require(
        set(successes)
        == {
            "APPROXIMATE-FLAG-PROPAGATION",
            "EXTRAPOLATED-STATUS-PROPAGATION",
            "ZERO-DERIVATIVE-CANCELLATION-EVIDENCE",
        },
        "differentiation success identities differ",
    )
    require(
        successes["APPROXIMATE-FLAG-PROPAGATION"]["status"]
        == "Approximate"
        and successes["APPROXIMATE-FLAG-PROPAGATION"]["flags"]
        == CLAMPED_FLAG
        and successes["EXTRAPOLATED-STATUS-PROPAGATION"]["status"]
        == "Extrapolated"
        and successes["ZERO-DERIVATIVE-CANCELLATION-EVIDENCE"]["status"]
        == "Success"
        and decimal(
            successes["ZERO-DERIVATIVE-CANCELLATION-EVIDENCE"]["result"][
                "derivative"
            ]
        )
        == 0
        and decimal(
            successes["ZERO-DERIVATIVE-CANCELLATION-EVIDENCE"]["result"][
                "risk"
            ]["output_cancellation_ratio"]
        )
        == 0,
        "differentiation success semantics differ",
    )

    expected_failures = {
        "INVALID-ARGUMENT-SCALE": ("DomainError", "policy", 0),
        "NONFINITE-ARGUMENT-SCALE": ("DomainError", "policy", 0),
        "INVALID-RELATIVE-STEP": ("DomainError", "policy", 0),
        "NONFINITE-RELATIVE-STEP": ("DomainError", "policy", 0),
        "NONFINITE-DOMAIN": ("NonFiniteInput", "domain", 0),
        "INVALID-DOMAIN": ("DomainError", "domain", 0),
        "NONFINITE-POINT": ("NonFiniteInput", "point", 0),
        "POINT-OUTSIDE-DOMAIN": (
            "DomainError",
            "point-outside-domain",
            0,
        ),
        "REQUESTED-STEP-OVERFLOW": (
            "NonFiniteIntermediate",
            "requested-step",
            0,
        ),
        "REQUESTED-STEP-UNDERFLOW": (
            "StepUnderflow",
            "requested-step-underflow",
            0,
        ),
        "UNREPRESENTABLE-CENTRAL-STEP": (
            "StepUnderflow",
            "unrepresentable-central-step",
            0,
        ),
        "CENTRAL-SAMPLES-OUTSIDE-DOMAIN": (
            "DomainError",
            "central-samples-outside-domain",
            0,
        ),
        "NONFINITE-SAMPLE-ARGUMENTS": (
            "NonFiniteIntermediate",
            "sample-arguments",
            0,
        ),
        "LOWER-CALLBACK-DOMAIN": ("DomainError", "lower-domain", 1),
        "UPPER-CALLBACK-DOMAIN": ("DomainError", "upper-domain", 2),
        "NONFINITE-FUNCTION-VALUE": (
            "NonFiniteIntermediate",
            "function-value",
            1,
        ),
        "NONFINITE-FUNCTION-DIFFERENCE": (
            "NonFiniteIntermediate",
            "function-difference",
            2,
        ),
        "NONFINITE-DERIVATIVE": ("NonFiniteOutput", "derivative", 2),
    }
    failures = {entry["id"]: entry for entry in probe["failure_cases"]}
    require(
        set(failures) == set(expected_failures),
        "differentiation failure identities differ",
    )
    for identifier, expected in expected_failures.items():
        status, detail, evaluations = expected
        outcome = failures[identifier]
        require(
            outcome["status"] == status
            and outcome["detail"] == detail
            and outcome["evaluations"] == evaluations
            and outcome["has_value"] is False
            and outcome["result"] is None,
            f"differentiation failure differs for {identifier}",
        )

    return {
        "component_id": COMPONENT_ID,
        "status": "passed",
        "convergence_samples": convergence_samples,
        "minimum_error_reduction_ratio": str(minimum_reduction_ratio),
        "maximum_stored_ladder_difference": str(maximum_stored_difference),
        "maximum_decimal_formula_difference": str(
            maximum_decimal_formula_difference
        ),
        "roundoff_transitions": len(transitions),
        "minimum_roundoff_amplification": str(
            minimum_roundoff_amplification
        ),
        "smallest_output_cancellation_ratio": str(
            smallest_cancellation_ratio
        ),
        "failure_cases": len(failures),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", required=True, type=Path)
    parser.add_argument("--oracle", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    arguments = parser.parse_args()

    getcontext().prec = 80
    cases = json.loads(
        arguments.cases.read_text(encoding="utf-8"), parse_float=Decimal
    )
    oracle = json.loads(
        arguments.oracle.read_text(encoding="utf-8"), parse_float=Decimal
    )
    print(
        json.dumps(
            verify(cases, oracle, arguments.probe), separators=(",", ":")
        )
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (
        ArithmeticError,
        IndexError,
        KeyError,
        OSError,
        TypeError,
        ValueError,
        json.JSONDecodeError,
        subprocess.SubprocessError,
    ) as error:
        print(
            f"foundation differentiation validation failed: {error}",
            file=sys.stderr,
        )
        raise SystemExit(1)

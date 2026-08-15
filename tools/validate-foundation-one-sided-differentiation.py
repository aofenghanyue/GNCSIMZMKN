#!/usr/bin/env python3
"""Independent Decimal comparator for one-sided boundary differentiation."""

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
PROBE_SCHEMA = "gnczmkn.foundation-one-sided-differentiation-probe/1"
COMPONENT_ID = "GNC-FOUNDATION-ONE-SIDED-DIFFERENTIATION-001"
ALGORITHM_ID = (
    "gnc.foundation.differentiation.scaled-one-sided-second-order@1"
)
CLAMPED_FLAG = 1 << 1


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite Decimal value: {value}")
    return result


def binary_decimal(value: object) -> Decimal:
    """Recover the exact binary64 value represented by probe JSON."""
    return Decimal.from_float(float(value))


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


def one_sided_difference(
    function: Callable[[Decimal], Decimal],
    point: Decimal,
    nearest: Decimal,
    far: Decimal,
) -> Decimal:
    nearest_offset = nearest - point
    far_offset = far - point
    require(
        nearest_offset != 0
        and far_offset != 0
        and nearest_offset * far_offset > 0,
        "Decimal one-sided offsets have invalid signs",
    )
    spacing_ratio = far_offset / nearest_offset
    require(spacing_ratio > 1, "Decimal one-sided spacing is invalid")
    nearest_slope = (function(nearest) - function(point)) / nearest_offset
    far_slope = (function(far) - function(point)) / far_offset
    return (spacing_ratio * nearest_slope - far_slope) / (
        spacing_ratio - Decimal(1)
    )


def verify_outcome_shape(
    outcome: dict,
    label: str,
    function: Callable[[Decimal], Decimal] | None = None,
) -> dict:
    require(
        outcome["status"] == "Success"
        and outcome["flags"] == 0
        and outcome["has_value"] is True
        and outcome["evaluations"] == 3
        and outcome["detail"] == "scaled-one-sided-second-order"
        and outcome["result"] is not None,
        f"{label} outcome metadata differs",
    )
    result = outcome["result"]
    point = binary_decimal(result["point"])
    nearest = binary_decimal(result["nearest_argument"])
    far = binary_decimal(result["far_argument"])
    nearest_offset = nearest - point
    far_offset = far - point
    direction = result["direction"]
    require(
        (direction == "Forward" and point < nearest < far)
        or (direction == "Backward" and far < nearest < point),
        f"{label} direction or sample order differs",
    )
    require(
        decimal(result["domain"]["lower"]) <= min(point, nearest, far)
        and max(point, nearest, far)
        <= decimal(result["domain"]["upper"]),
        f"{label} samples exceed the declared domain",
    )
    selected = max(
        abs(point), decimal(result["nominal_argument_scale"])
    )
    close(
        result["selected_argument_scale"],
        selected,
        Decimal("1e-30"),
        Decimal("2e-16"),
        f"{label}.selected_argument_scale",
    )
    close(
        result["requested_step"],
        decimal(result["relative_step"]) * selected,
        Decimal("2e-15"),
        Decimal("2e-16"),
        f"{label}.requested_step_selection",
    )
    close(
        result["nearest_offset"],
        nearest_offset,
        Decimal("5e-12"),
        Decimal("2e-16"),
        f"{label}.nearest_offset",
    )
    close(
        result["far_offset"],
        far_offset,
        Decimal("5e-12"),
        Decimal("2e-16"),
        f"{label}.far_offset",
    )
    effective = abs(nearest_offset)
    spacing_ratio = far_offset / nearest_offset
    close(
        result["effective_step"],
        effective,
        Decimal("5e-12"),
        Decimal("2e-16"),
        f"{label}.effective_step",
    )
    close(
        outcome["last_step"],
        effective,
        Decimal("5e-12"),
        Decimal("2e-16"),
        f"{label}.last_step",
    )
    close(
        result["spacing_ratio"],
        spacing_ratio,
        Decimal("5e-14"),
        Decimal("1e-15"),
        f"{label}.spacing_ratio",
    )

    point_value = decimal(result["point_value"])
    nearest_value = decimal(result["nearest_value"])
    far_value = decimal(result["far_value"])

    # JSON uses round-trip decimal spellings for binary64 values. Convert them
    # back to binary64 before checking the implementation arithmetic; a direct
    # Decimal reconstruction would amplify the final printed digit at tiny h.
    point_value_float = float(result["point_value"])
    nearest_value_float = float(result["nearest_value"])
    far_value_float = float(result["far_value"])
    nearest_offset_float = float(result["nearest_offset"])
    far_offset_float = float(result["far_offset"])
    spacing_ratio_float = float(result["spacing_ratio"])
    nearest_difference_float = nearest_value_float - point_value_float
    far_difference_float = far_value_float - point_value_float
    nearest_slope_float = (
        nearest_difference_float / nearest_offset_float
    )
    far_slope_float = far_difference_float / far_offset_float
    scaled_nearest_slope_float = (
        spacing_ratio_float * nearest_slope_float
    )
    derivative_combination_float = (
        scaled_nearest_slope_float - far_slope_float
    )
    reported_formula = derivative_combination_float / (
        spacing_ratio_float - 1.0
    )
    close(
        result["derivative"],
        reported_formula,
        Decimal("1e-30"),
        Decimal("2e-16"),
        f"{label}.reported_value_formula",
    )

    risk = result["risk"]
    close(
        risk["normalized_step"],
        effective / selected,
        Decimal("2e-18"),
        Decimal("3e-16"),
        f"{label}.normalized_step",
    )

    def cancellation(difference: float, first: float, second: float) -> float:
        scale = max(abs(first), abs(second))
        return abs(difference) / scale if scale > 0 else 0.0

    close(
        risk["nearest_output_cancellation_ratio"],
        cancellation(
            nearest_difference_float,
            point_value_float,
            nearest_value_float,
        ),
        Decimal("1e-30"),
        Decimal("2e-16"),
        f"{label}.nearest_output_cancellation_ratio",
    )
    close(
        risk["far_output_cancellation_ratio"],
        cancellation(
            far_difference_float, point_value_float, far_value_float
        ),
        Decimal("1e-30"),
        Decimal("2e-16"),
        f"{label}.far_output_cancellation_ratio",
    )
    derivative_scale = max(
        abs(scaled_nearest_slope_float), abs(far_slope_float)
    )
    combination_ratio = (
        abs(derivative_combination_float) / derivative_scale
        if derivative_scale > 0
        else 0.0
    )
    close(
        risk["derivative_combination_ratio"],
        combination_ratio,
        Decimal("1e-30"),
        Decimal("2e-16"),
        f"{label}.derivative_combination_ratio",
    )
    close(
        risk["spacing_ratio_error"],
        abs(spacing_ratio - Decimal(2)),
        Decimal("5e-14"),
        Decimal("1e-15"),
        f"{label}.spacing_ratio_error",
    )

    if function is not None:
        close(
            point_value,
            function(point),
            Decimal("2e-15"),
            Decimal("2e-15"),
            f"{label}.point_value",
        )
        close(
            nearest_value,
            function(nearest),
            Decimal("2e-15"),
            Decimal("2e-15"),
            f"{label}.nearest_value",
        )
        close(
            far_value,
            function(far),
            Decimal("2e-15"),
            Decimal("2e-15"),
            f"{label}.far_value",
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
        "one-sided differentiation probe reruns differ",
    )
    require(
        probe["schema_version"] == PROBE_SCHEMA
        and probe["component_id"] == COMPONENT_ID
        and probe["fixture_id"] == FIXTURE_ID,
        "one-sided differentiation probe identity differs",
    )
    require(
        probe["algorithm"]
        == {"id": ALGORITHM_ID, "version": "1.0.0"},
        "one-sided differentiation algorithm identity differs",
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
    reference_altitude = decimal(derivative_case["altitude_m"])
    speed = decimal(derivative_case["speed_mps"])
    rho0 = decimal(atmosphere["sea_level_density_kg_per_m3"])
    scale_height = decimal(atmosphere["density_scale_height_m"])
    sound = decimal(atmosphere["speed_of_sound_mps"])
    sound_gradient = decimal(
        atmosphere["speed_of_sound_gradient_per_m"]
    )

    def density_function(argument: Decimal) -> Decimal:
        return rho0 * (-argument / scale_height).exp()

    def local_sound(argument: Decimal) -> Decimal:
        return sound + sound_gradient * (
            argument - reference_altitude
        )

    def mach_altitude_function(argument: Decimal) -> Decimal:
        local = local_sound(argument)
        require(local > 0, "Decimal sound speed is outside domain")
        return speed / local

    def cl_star_function(argument: Decimal) -> Decimal:
        radicand = (
            Decimal("0.02") + Decimal("0.001") * argument
        ) / Decimal("0.08")
        require(radicand >= 0, "Decimal CL-star radicand is outside domain")
        return radicand.sqrt()

    def density_derivative(argument: Decimal) -> Decimal:
        return -density_function(argument) / scale_height

    def mach_derivative(argument: Decimal) -> Decimal:
        return -speed * sound_gradient / (local_sound(argument) ** 2)

    def cl_star_derivative(argument: Decimal) -> Decimal:
        return Decimal("0.001") / (
            Decimal(2)
            * (
                Decimal("0.08")
                * (Decimal("0.02") + Decimal("0.001") * argument)
            ).sqrt()
        )

    stored_analytic = derivative_oracle["analytic"]
    close(
        density_function(reference_altitude),
        stored_analytic["density_kg_per_m3"],
        Decimal("2e-78"),
        Decimal(0),
        "stored CAVH density anchor",
    )
    close(
        density_derivative(reference_altitude),
        stored_analytic["density_gradient_kg_per_m4"],
        Decimal("2e-78"),
        Decimal(0),
        "stored CAVH density derivative anchor",
    )
    close(
        mach_derivative(reference_altitude),
        stored_analytic["partial_mach_partial_altitude_per_m"],
        Decimal("1e-79"),
        Decimal(0),
        "stored CAVH Mach derivative anchor",
    )
    close(
        cl_star_derivative(Decimal(10)),
        stored_analytic["dcl_star_dmach"],
        Decimal("2e-78"),
        Decimal(0),
        "stored CAVH CL-star derivative anchor",
    )
    close(
        cl_star_function(Decimal(10)),
        oracle["envelope_cases"][
            "CASE-CAVH-ENVELOPE-MACH-DEPENDENT"
        ]["cl_star"],
        Decimal("2e-78"),
        Decimal(0),
        "stored CAVH CL-star value anchor",
    )

    altitude_steps = [
        decimal(value)
        for value in derivative_case["density_altitude_step_ladder_m"]
    ]
    mach_steps = [
        decimal(value)
        for value in derivative_case["cl_star_mach_step_ladder"]
    ]
    altitude_domain = {"lower": Decimal(0), "upper": Decimal("60000")}
    mach_domain = {"lower": Decimal(0), "upper": Decimal(20)}
    ladder_specs = {
        "DENSITY-LOWER-FORWARD": {
            "direction": "Forward",
            "point": Decimal(0),
            "domain": altitude_domain,
            "scale": reference_altitude,
            "steps": altitude_steps,
            "analytic": density_derivative(Decimal(0)),
            "function": density_function,
        },
        "DENSITY-UPPER-BACKWARD": {
            "direction": "Backward",
            "point": Decimal("60000"),
            "domain": altitude_domain,
            "scale": reference_altitude,
            "steps": altitude_steps,
            "analytic": density_derivative(Decimal("60000")),
            "function": density_function,
        },
        "MACH-LOWER-FORWARD": {
            "direction": "Forward",
            "point": Decimal(0),
            "domain": altitude_domain,
            "scale": reference_altitude,
            "steps": altitude_steps,
            "analytic": mach_derivative(Decimal(0)),
            "function": mach_altitude_function,
        },
        "MACH-UPPER-BACKWARD": {
            "direction": "Backward",
            "point": Decimal("60000"),
            "domain": altitude_domain,
            "scale": reference_altitude,
            "steps": altitude_steps,
            "analytic": mach_derivative(Decimal("60000")),
            "function": mach_altitude_function,
        },
        "CL-STAR-LOWER-FORWARD": {
            "direction": "Forward",
            "point": Decimal(0),
            "domain": mach_domain,
            "scale": Decimal(10),
            "steps": mach_steps,
            "analytic": cl_star_derivative(Decimal(0)),
            "function": cl_star_function,
        },
        "CL-STAR-UPPER-BACKWARD": {
            "direction": "Backward",
            "point": Decimal(20),
            "domain": mach_domain,
            "scale": Decimal(10),
            "steps": mach_steps,
            "analytic": cl_star_derivative(Decimal(20)),
            "function": cl_star_function,
        },
    }

    ladders = {entry["id"]: entry for entry in probe["convergence_ladders"]}
    require(
        set(ladders) == set(ladder_specs),
        "one-sided boundary ladder identities differ",
    )
    minimum_reduction_ratio: Decimal | None = None
    maximum_reduction_ratio = Decimal(0)
    maximum_decimal_formula_difference = Decimal(0)
    boundary_samples = 0
    for identifier, spec in ladder_specs.items():
        ladder = ladders[identifier]
        require(
            ladder["direction"] == spec["direction"]
            and ladder["domain"] == spec["domain"],
            f"{identifier} boundary direction or domain differs",
        )
        close(
            ladder["point"],
            spec["point"],
            Decimal("1e-24"),
            Decimal(0),
            f"{identifier}.point",
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
            Decimal("3e-20"),
            Decimal("3e-16"),
            f"{identifier}.analytic_derivative",
        )
        reported_analytic = decimal(ladder["analytic_derivative"])
        require(
            len(ladder["samples"]) == len(spec["steps"]) == 5,
            f"{identifier} boundary sample count differs",
        )
        ideal_errors: list[Decimal] = []
        double_errors: list[Decimal] = []
        for index, (sample, step) in enumerate(
            zip(ladder["samples"], spec["steps"])
        ):
            close(
                sample["requested_step"],
                step,
                Decimal("3e-16"),
                Decimal("2e-16"),
                f"{identifier}.step[{index}]",
            )
            result = verify_outcome_shape(
                sample["outcome"],
                f"{identifier}.sample[{index}]",
                spec["function"],
            )
            require(
                result["direction"] == spec["direction"],
                f"{identifier}.sample[{index}] direction differs",
            )
            ideal = one_sided_difference(
                spec["function"],
                binary_decimal(result["point"]),
                binary_decimal(result["nearest_argument"]),
                binary_decimal(result["far_argument"]),
            )
            formula_difference = close(
                result["derivative"],
                ideal,
                Decimal("4e-14"),
                Decimal("4e-15"),
                f"{identifier}.sample[{index}].Decimal_formula",
            )
            maximum_decimal_formula_difference = max(
                maximum_decimal_formula_difference, formula_difference
            )
            ideal_errors.append(abs(ideal - spec["analytic"]))
            double_errors.append(
                abs(decimal(result["derivative"]) - reported_analytic)
            )
            boundary_samples += 1
        ideal_ratios = [
            ideal_errors[index] / ideal_errors[index + 1]
            for index in range(4)
        ]
        require(
            all(
                Decimal("3.75") < ratio < Decimal("4.25")
                for ratio in ideal_ratios
            ),
            f"{identifier} independent second-order trend differs",
        )
        double_ratios = [
            double_errors[index] / double_errors[index + 1]
            for index in range(4)
        ]
        for index, ratio in enumerate(ladder["error_reduction_ratios"]):
            close(
                ratio,
                double_ratios[index],
                Decimal("2e-8"),
                Decimal("2e-9"),
                f"{identifier}.reported_ratio[{index}]",
            )
        local_minimum = min(ideal_ratios)
        minimum_reduction_ratio = (
            local_minimum
            if minimum_reduction_ratio is None
            else min(minimum_reduction_ratio, local_minimum)
        )
        maximum_reduction_ratio = max(
            maximum_reduction_ratio, max(ideal_ratios)
        )

    transition_steps = {
        "DENSITY-LOWER-FORWARD": (Decimal("0.01"), Decimal("1e-9")),
        "DENSITY-UPPER-BACKWARD": (Decimal("0.1"), Decimal("1e-9")),
        "MACH-LOWER-FORWARD": (Decimal(1), Decimal("1e-8")),
        "MACH-UPPER-BACKWARD": (Decimal(1), Decimal("1e-8")),
        "CL-STAR-LOWER-FORWARD": (Decimal("1e-4"), Decimal("1e-12")),
        "CL-STAR-UPPER-BACKWARD": (Decimal("1e-3"), Decimal("1e-12")),
    }
    transitions = {
        entry["id"]: entry for entry in probe["roundoff_transitions"]
    }
    require(
        set(transitions) == set(transition_steps),
        "one-sided roundoff transition identities differ",
    )
    minimum_roundoff_amplification: Decimal | None = None
    smallest_cancellation_ratio: Decimal | None = None
    for identifier, (well_step, tiny_step) in transition_steps.items():
        transition = transitions[identifier]
        spec = ladder_specs[identifier]
        require(
            transition["direction"] == spec["direction"],
            f"{identifier} transition direction differs",
        )
        results: list[dict] = []
        for key, expected_step in (
            ("well_scaled", well_step),
            ("tiny_step", tiny_step),
        ):
            sample = transition[key]
            close(
                sample["requested_step"],
                expected_step,
                Decimal("3e-16"),
                Decimal("2e-16"),
                f"{identifier}.{key}.requested_step",
            )
            results.append(
                verify_outcome_shape(
                    sample["outcome"],
                    f"{identifier}.{key}",
                    spec["function"],
                )
            )
        actual_errors = [
            abs(decimal(result["derivative"]) - spec["analytic"])
            for result in results
        ]
        ideal_estimates = [
            one_sided_difference(
                spec["function"],
                binary_decimal(result["point"]),
                binary_decimal(result["nearest_argument"]),
                binary_decimal(result["far_argument"]),
            )
            for result in results
        ]
        ideal_errors = [
            abs(estimate - spec["analytic"])
            for estimate in ideal_estimates
        ]
        require(
            actual_errors[1]
            > Decimal(1000) * max(actual_errors[0], Decimal("1e-40")),
            f"{identifier} binary64 roundoff transition is missing",
        )
        require(
            Decimal(0) < ideal_errors[1] < ideal_errors[0],
            f"{identifier} Decimal truncation trend differs",
        )
        amplification = actual_errors[1] / ideal_errors[1]
        require(
            amplification > Decimal("1e10"),
            f"{identifier} roundoff amplification is too small",
        )
        minimum_roundoff_amplification = (
            amplification
            if minimum_roundoff_amplification is None
            else min(minimum_roundoff_amplification, amplification)
        )
        tiny_cancellation = decimal(
            results[1]["risk"]["nearest_output_cancellation_ratio"]
        )
        well_cancellation = decimal(
            results[0]["risk"]["nearest_output_cancellation_ratio"]
        )
        require(
            Decimal(0) < tiny_cancellation < well_cancellation,
            f"{identifier} cancellation evidence differs",
        )
        smallest_cancellation_ratio = (
            tiny_cancellation
            if smallest_cancellation_ratio is None
            else min(smallest_cancellation_ratio, tiny_cancellation)
        )

    direction_cases = {
        entry["id"]: entry for entry in probe["direction_semantics"]
    }
    require(
        set(direction_cases)
        == {
            "FORWARD-QUADRATIC",
            "BACKWARD-QUADRATIC",
            "POINT-NEAREST-FAR-CALLBACK-ORDER",
        },
        "one-sided direction case identities differ",
    )
    forward = verify_outcome_shape(
        direction_cases["FORWARD-QUADRATIC"], "forward quadratic"
    )
    backward = verify_outcome_shape(
        direction_cases["BACKWARD-QUADRATIC"], "backward quadratic"
    )
    ordered = verify_outcome_shape(
        direction_cases["POINT-NEAREST-FAR-CALLBACK-ORDER"],
        "callback order",
    )
    require(
        forward["direction"] == "Forward"
        and backward["direction"] == "Backward"
        and decimal(forward["derivative"]) == Decimal(2)
        and decimal(backward["derivative"]) == Decimal(2)
        and decimal(ordered["derivative"]) == Decimal(1),
        "one-sided direction results differ",
    )

    scale_cases = {entry["id"]: entry for entry in probe["scale_selection"]}
    require(
        set(scale_cases)
        == {
            "ZERO-POINT-NOMINAL-SCALE",
            "POINT-MAGNITUDE-SCALE",
            "DEFAULT-POLICY-DENSITY-LOWER",
            "DEFAULT-POLICY-CL-STAR-UPPER",
        },
        "one-sided scale selection identities differ",
    )
    zero_scale = verify_outcome_shape(
        scale_cases["ZERO-POINT-NOMINAL-SCALE"], "zero-point scale"
    )
    point_scale = verify_outcome_shape(
        scale_cases["POINT-MAGNITUDE-SCALE"], "point-magnitude scale"
    )
    default_density = verify_outcome_shape(
        scale_cases["DEFAULT-POLICY-DENSITY-LOWER"],
        "default density",
        density_function,
    )
    default_cl = verify_outcome_shape(
        scale_cases["DEFAULT-POLICY-CL-STAR-UPPER"],
        "default CL-star",
        cl_star_function,
    )
    require(
        decimal(zero_scale["selected_argument_scale"]) == Decimal(20)
        and decimal(zero_scale["requested_step"]) == Decimal("2.5")
        and decimal(zero_scale["derivative"]) == Decimal(3)
        and decimal(point_scale["selected_argument_scale"]) == Decimal(100)
        and decimal(point_scale["requested_step"]) == Decimal("12.5")
        and decimal(point_scale["derivative"]) == Decimal(3)
        and decimal(default_density["selected_argument_scale"])
        == Decimal(1)
        and decimal(default_cl["selected_argument_scale"]) == Decimal(20),
        "one-sided scale selection values differ",
    )

    success_cases = {
        entry["id"]: entry for entry in probe["success_semantics"]
    }
    require(
        set(success_cases)
        == {
            "APPROXIMATE-FLAG-PROPAGATION",
            "EXTRAPOLATED-STATUS-PROPAGATION",
            "ZERO-DERIVATIVE-CANCELLATION-EVIDENCE",
        },
        "one-sided success case identities differ",
    )
    approximate = success_cases["APPROXIMATE-FLAG-PROPAGATION"]
    extrapolated = success_cases["EXTRAPOLATED-STATUS-PROPAGATION"]
    zero = success_cases["ZERO-DERIVATIVE-CANCELLATION-EVIDENCE"]
    require(
        approximate["status"] == "Approximate"
        and approximate["flags"] & CLAMPED_FLAG
        and approximate["has_value"] is True
        and approximate["evaluations"] == 3
        and extrapolated["status"] == "Extrapolated"
        and extrapolated["has_value"] is True
        and extrapolated["evaluations"] == 3
        and zero["status"] == "Success"
        and decimal(zero["result"]["derivative"]) == Decimal(0)
        and decimal(
            zero["result"]["risk"][
                "nearest_output_cancellation_ratio"
            ]
        )
        == Decimal(0)
        and decimal(
            zero["result"]["risk"]["far_output_cancellation_ratio"]
        )
        == Decimal(0)
        and decimal(
            zero["result"]["risk"]["derivative_combination_ratio"]
        )
        == Decimal(0),
        "one-sided success status or risk propagation differs",
    )

    expected_failures = {
        "INVALID-ARGUMENT-SCALE": ("DomainError", "policy", 0),
        "NONFINITE-ARGUMENT-SCALE": ("DomainError", "policy", 0),
        "INVALID-RELATIVE-STEP": ("DomainError", "policy", 0),
        "NONFINITE-RELATIVE-STEP": ("DomainError", "policy", 0),
        "INVALID-DIRECTION": ("DomainError", "direction", 0),
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
        "NONFINITE-SAMPLE-ARGUMENTS": (
            "NonFiniteIntermediate",
            "sample-arguments",
            0,
        ),
        "UNREPRESENTABLE-NEAREST-STEP": (
            "StepUnderflow",
            "unrepresentable-nearest-step",
            0,
        ),
        "UNREPRESENTABLE-FAR-STEP": (
            "StepUnderflow",
            "unrepresentable-far-step",
            0,
        ),
        "FORWARD-SAMPLES-OUTSIDE-DOMAIN": (
            "DomainError",
            "one-sided-samples-outside-domain",
            0,
        ),
        "BACKWARD-SAMPLES-OUTSIDE-DOMAIN": (
            "DomainError",
            "one-sided-samples-outside-domain",
            0,
        ),
        "POINT-CALLBACK-DOMAIN": ("DomainError", "point-domain", 1),
        "NEAREST-CALLBACK-DOMAIN": (
            "DomainError",
            "nearest-domain",
            2,
        ),
        "FAR-CALLBACK-DOMAIN": ("DomainError", "far-domain", 3),
        "NONFINITE-FUNCTION-VALUE": (
            "NonFiniteIntermediate",
            "function-value",
            1,
        ),
        "NONFINITE-FUNCTION-DIFFERENCE": (
            "NonFiniteIntermediate",
            "function-difference",
            3,
        ),
        "NONFINITE-SECANT-SLOPE": (
            "NonFiniteIntermediate",
            "secant-slope",
            3,
        ),
        "NONFINITE-DERIVATIVE-COMBINATION": (
            "NonFiniteIntermediate",
            "derivative-combination",
            3,
        ),
    }
    failures = {entry["id"]: entry for entry in probe["failure_cases"]}
    require(
        set(failures) == set(expected_failures),
        "one-sided failure identities differ",
    )
    for identifier, expected in expected_failures.items():
        outcome = failures[identifier]
        require(
            (
                outcome["status"],
                outcome["detail"],
                outcome["evaluations"],
            )
            == expected
            and outcome["flags"] == 0
            and outcome["has_value"] is False
            and outcome["result"] is None,
            f"one-sided failure differs for {identifier}",
        )

    require(
        minimum_reduction_ratio is not None
        and minimum_roundoff_amplification is not None
        and smallest_cancellation_ratio is not None,
        "one-sided metrics are incomplete",
    )
    return {
        "status": "passed",
        "component_id": COMPONENT_ID,
        "algorithm_id": ALGORITHM_ID,
        "boundary_samples": boundary_samples,
        "minimum_error_reduction_ratio": str(minimum_reduction_ratio),
        "maximum_error_reduction_ratio": str(maximum_reduction_ratio),
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
    getcontext().prec = 80
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", required=True, type=Path)
    parser.add_argument("--oracle", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    arguments = parser.parse_args()
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
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(
            f"foundation one-sided differentiation validation failed: {error}",
            file=sys.stderr,
        )
        raise SystemExit(1)

#!/usr/bin/env python3
"""Independent high-precision oracle for REF-CAVH-FORMULA-001."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
import tempfile
from decimal import Decimal, localcontext
from pathlib import Path
from typing import Any, Callable


DECIMAL_DIGITS = 80
WORK_DIGITS = 100
FIXTURE_ID = "REF-CAVH-FORMULA-001"
ORACLE_ID = "ORACLE-CAVH-FORMULA-001"
MODEL_ID = "MODEL-CAVH-LEGACY-TRANSCRIBED-FORMULA-001"
REFERENCE_SCHEMA = "gnczmkn.cavh-formula-reference/1"
PROBE_SCHEMA = "gnczmkn.cavh-formula-probe/1"


class ModelFailure(RuntimeError):
    def __init__(self, status: str, fallback: str = "not-applicable") -> None:
        super().__init__(status)
        self.status = status
        self.fallback = fallback


def load_decimal_json(path: Path) -> Any:
    return json.loads(
        path.read_text(encoding="utf-8"),
        parse_float=Decimal,
        parse_int=Decimal,
    )


def d(value: Any) -> Decimal:
    if isinstance(value, Decimal):
        return value
    return Decimal(str(value))


def require_finite(*values: Decimal) -> None:
    if not all(value.is_finite() for value in values):
        raise ModelFailure("input-domain-error")


def decimal_cos(value: Decimal) -> Decimal:
    with localcontext() as context:
        context.prec = WORK_DIGITS
        square = value * value
        term = Decimal(1)
        result = Decimal(1)
        index = 0
        threshold = Decimal(1).scaleb(-(WORK_DIGITS - 5))
        while True:
            index += 1
            term *= -square / d((2 * index - 1) * (2 * index))
            result += term
            if abs(term) < threshold:
                return +result


def envelope(polar: dict[str, Decimal]) -> dict[str, Decimal | str]:
    cl0 = d(polar["cl_intercept"])
    cl_alpha = d(polar["cl_slope_per_rad"])
    cd0_base = d(polar["cd0_base"])
    cd0_mach = d(polar["cd0_slope_per_mach"])
    induced = d(polar["induced_drag_factor"])
    mach = d(polar["mach"])
    alpha_min = d(polar["alpha_min_rad"])
    alpha_max = d(polar["alpha_max_rad"])
    require_finite(
        cl0,
        cl_alpha,
        cd0_base,
        cd0_mach,
        induced,
        mach,
        alpha_min,
        alpha_max,
    )
    cd0 = cd0_base + cd0_mach * mach
    if cd0 <= 0 or induced <= 0 or cl_alpha <= 0 or alpha_max <= alpha_min:
        raise ModelFailure("envelope-domain-error")
    cl_star = (cd0 / induced).sqrt()
    alpha_star = (cl_star - cl0) / cl_alpha
    if alpha_star < alpha_min or alpha_star > alpha_max:
        raise ModelFailure("envelope-outside-domain")
    cd_star = cd0 + induced * cl_star * cl_star
    return {
        "status": "passed",
        "cd0": cd0,
        "cl_star": cl_star,
        "cd_star": cd_star,
        "lift_to_drag_max": cl_star / cd_star,
        "alpha_star_rad": alpha_star,
        "dcl_star_dmach": cd0_mach / (d(2) * (induced * cd0).sqrt()),
    }


def density(rho0: Decimal, scale_height: Decimal, altitude: Decimal) -> Decimal:
    return rho0 * (-altitude / scale_height).exp()


def speed_of_sound(
    reference_value: Decimal,
    gradient: Decimal,
    altitude: Decimal,
    reference_altitude: Decimal,
) -> Decimal:
    return reference_value + gradient * (altitude - reference_altitude)


def central_derivative(
    function: Callable[[Decimal], Decimal], point: Decimal, step: Decimal
) -> Decimal:
    return (function(point + step) - function(point - step)) / (d(2) * step)


def convergence_ladder(
    steps: list[Decimal], analytic: Decimal, estimator: Callable[[Decimal], Decimal]
) -> tuple[list[dict[str, Decimal]], list[Decimal]]:
    rows: list[dict[str, Decimal]] = []
    errors: list[Decimal] = []
    for step in steps:
        estimate = estimator(step)
        error = abs(estimate - analytic)
        rows.append({"step": step, "estimate": estimate, "absolute_error": error})
        errors.append(error)
    ratios = [errors[index] / errors[index + 1] for index in range(len(errors) - 1)]
    return rows, ratios


def derivative_report(
    case: dict[str, Any], envelope_result: dict[str, Any], final_tolerance: Decimal
) -> dict[str, Any]:
    altitude = d(case["altitude_m"])
    speed = d(case["speed_mps"])
    atmosphere = case["atmosphere"]
    rho0 = d(atmosphere["sea_level_density_kg_per_m3"])
    scale_height = d(atmosphere["density_scale_height_m"])
    sound = d(atmosphere["speed_of_sound_mps"])
    sound_gradient = d(atmosphere["speed_of_sound_gradient_per_m"])
    require_finite(altitude, speed, rho0, scale_height, sound, sound_gradient)
    if speed <= 0 or rho0 <= 0 or scale_height <= 0 or sound <= 0:
        raise ModelFailure("input-domain-error")

    rho = density(rho0, scale_height, altitude)
    rho_h = -rho / scale_height
    mach_v = d(1) / sound
    mach_h = -speed * sound_gradient / (sound * sound)
    dcl_dmach = d(envelope_result["dcl_star_dmach"])

    density_steps = [d(value) for value in case["density_altitude_step_ladder_m"]]
    density_rows, density_ratios = convergence_ladder(
        density_steps,
        rho_h,
        lambda step: central_derivative(
            lambda h: density(rho0, scale_height, h), altitude, step
        ),
    )

    mach_steps = [d(value) for value in case["mach_altitude_step_ladder_m"]]
    mach_rows, mach_ratios = convergence_ladder(
        mach_steps,
        mach_h,
        lambda step: central_derivative(
            lambda h: speed
            / speed_of_sound(sound, sound_gradient, h, altitude),
            altitude,
            step,
        ),
    )

    polar = {
        "cd0_base": d("0.02"),
        "cd0_slope_per_mach": d("0.001"),
        "induced_drag_factor": d("0.08"),
    }
    cl_function = lambda mach: (
        (polar["cd0_base"] + polar["cd0_slope_per_mach"] * mach)
        / polar["induced_drag_factor"]
    ).sqrt()
    cl_steps = [d(value) for value in case["cl_star_mach_step_ladder"]]
    cl_rows, cl_ratios = convergence_ladder(
        cl_steps,
        dcl_dmach,
        lambda step: central_derivative(cl_function, d("10"), step),
    )

    all_errors = [
        density_rows[-1]["absolute_error"],
        mach_rows[-1]["absolute_error"],
        cl_rows[-1]["absolute_error"],
    ]
    monotone = all(
        rows[index]["absolute_error"] > rows[index + 1]["absolute_error"]
        for rows in (density_rows, mach_rows, cl_rows)
        for index in range(len(rows) - 1)
    )
    status = "passed" if monotone and max(all_errors) < final_tolerance else "failed"
    return {
        "id": case["id"],
        "status": status,
        "analytic": {
            "density_kg_per_m3": rho,
            "density_gradient_kg_per_m4": rho_h,
            "partial_mach_partial_speed_s_per_m": mach_v,
            "partial_mach_partial_altitude_per_m": mach_h,
            "dcl_star_dmach": dcl_dmach,
        },
        "density_gradient_ladder": density_rows,
        "density_error_reduction_ratios": density_ratios,
        "mach_altitude_ladder": mach_rows,
        "mach_error_reduction_ratios": mach_ratios,
        "cl_star_mach_ladder": cl_rows,
        "cl_star_error_reduction_ratios": cl_ratios,
    }


def validate_formula_domain(
    rho: Decimal,
    speed: Decimal,
    gravity: Decimal,
    mass: Decimal,
    area: Decimal,
    radius: Decimal,
    cl_vertical: Decimal,
) -> None:
    require_finite(rho, speed, gravity, mass, area, radius, cl_vertical)
    if min(rho, speed, gravity, mass, area, radius, cl_vertical) <= 0:
        raise ModelFailure("formula-domain-error")


def reject_small_denominators(
    denominators: list[Decimal], minimum: Decimal
) -> None:
    if any(not value.is_finite() or abs(value) <= minimum for value in denominators):
        raise ModelFailure("formula-singularity")


def eq18_raw(
    *,
    rho: Decimal,
    rho_h: Decimal,
    speed: Decimal,
    gravity: Decimal,
    mass: Decimal,
    area: Decimal,
    radius: Decimal,
    cl_vertical: Decimal,
    cd_star: Decimal,
    denominator_minimum: Decimal,
) -> dict[str, Decimal | str]:
    validate_formula_domain(rho, speed, gravity, mass, area, radius, cl_vertical)
    require_finite(rho_h, cd_star)
    if cd_star <= 0:
        raise ModelFailure("formula-domain-error")
    dynamic_pressure = rho * speed * speed / d(2)
    drag = dynamic_pressure * area * cd_star
    a21 = rho_h * speed * speed / (d(2) * rho * gravity)
    a24 = d(2) * mass / (cl_vertical * rho * area * radius)
    a25 = mass * speed * speed / (
        cl_vertical * rho * gravity * area * radius * radius
    )
    a31 = rho_h * cl_vertical * speed * speed * area * radius / (
        d(4) * mass * gravity
    )
    a34 = d(1) / a24
    a35 = speed * speed / (d(2) * gravity * radius)
    b2 = d(1) - a21 + a24 + a25
    b3 = d(1) - a31 + a34 + a35
    reject_small_denominators([b2, b3], denominator_minimum)
    gamma = -drag / (mass * gravity) * (d(1) / b2 + d(1) / b3)
    return {
        "equation": "eq18",
        "status": "passed",
        "density_kg_per_m3": rho,
        "density_gradient_kg_per_m4": rho_h,
        "radius_m": radius,
        "dynamic_pressure_Pa": dynamic_pressure,
        "drag_force_N": drag,
        "cl_vertical": cl_vertical,
        "A21": a21,
        "A24": a24,
        "A25": a25,
        "A31": a31,
        "A34": a34,
        "A35": a35,
        "B2": b2,
        "B3": b3,
        "gamma_reference_rad": gamma,
    }


def eq17_raw(
    *,
    rho: Decimal,
    rho_h: Decimal,
    speed: Decimal,
    gravity: Decimal,
    mass: Decimal,
    area: Decimal,
    radius: Decimal,
    cl_vertical: Decimal,
    cd_star: Decimal,
    dcl_vertical_dmach: Decimal,
    mach_v: Decimal,
    mach_h: Decimal,
    denominator_minimum: Decimal,
    derivative_minimum: Decimal,
) -> dict[str, Decimal | str]:
    validate_formula_domain(rho, speed, gravity, mass, area, radius, cl_vertical)
    require_finite(rho_h, cd_star, dcl_vertical_dmach, mach_v, mach_h)
    if cd_star <= 0 or mach_v <= 0:
        raise ModelFailure("formula-domain-error")
    dcl_vertical_dspeed = dcl_vertical_dmach * mach_v
    if abs(dcl_vertical_dspeed) <= derivative_minimum:
        raise ModelFailure("derivative-degenerate", "forbidden")

    dynamic_pressure = rho * speed * speed / d(2)
    drag = dynamic_pressure * area * cd_star
    a11 = rho_h * cl_vertical * speed / (
        dcl_vertical_dspeed * rho * gravity
    )
    a12 = mach_h * speed / (mach_v * gravity)
    a13 = d(2) * cl_vertical / (dcl_vertical_dspeed * speed)
    a14 = d(4) * mass / (
        dcl_vertical_dspeed * rho * speed * area * radius
    )
    a15 = d(2) * speed * mass / (
        dcl_vertical_dspeed * rho * area * gravity * radius * radius
    )
    a21 = rho_h * speed * speed / (d(2) * rho * gravity)
    a22 = dcl_vertical_dmach * mach_h * speed * speed / (
        d(2) * cl_vertical * gravity
    )
    a23 = d(1) / a13
    a24 = d(2) * mass / (cl_vertical * rho * area * radius)
    a25 = mass * speed * speed / (
        cl_vertical * rho * gravity * area * radius * radius
    )
    a31 = rho_h * cl_vertical * speed * speed * area * radius / (
        d(4) * mass * gravity
    )
    a32 = dcl_vertical_dmach * mach_h * rho * speed * speed * area * radius / (
        d(4) * mass * gravity
    )
    a33 = d(1) / a14
    a34 = d(1) / a24
    a35 = speed * speed / (d(2) * gravity * radius)
    b1 = d(1) - a11 - a12 + a13 + a14 + a15
    b2 = d(1) - a21 - a22 + a23 + a24 + a25
    b3 = d(1) - a31 - a32 + a33 + a34 + a35
    reject_small_denominators([b1, b2, b3], denominator_minimum)
    gamma = -drag / (mass * gravity) * (
        d(1) / b1 + d(1) / b2 + d(1) / b3
    )
    return {
        "equation": "eq17",
        "status": "passed",
        "density_kg_per_m3": rho,
        "density_gradient_kg_per_m4": rho_h,
        "radius_m": radius,
        "dynamic_pressure_Pa": dynamic_pressure,
        "drag_force_N": drag,
        "cl_vertical": cl_vertical,
        "dcl_vertical_dmach": dcl_vertical_dmach,
        "partial_mach_partial_speed_s_per_m": mach_v,
        "partial_mach_partial_altitude_per_m": mach_h,
        "dcl_vertical_dspeed_s_per_m": dcl_vertical_dspeed,
        "A11": a11,
        "A12": a12,
        "A13": a13,
        "A14": a14,
        "A15": a15,
        "A21": a21,
        "A22": a22,
        "A23": a23,
        "A24": a24,
        "A25": a25,
        "A31": a31,
        "A32": a32,
        "A33": a33,
        "A34": a34,
        "A35": a35,
        "B1": b1,
        "B2": b2,
        "B3": b3,
        "gamma_reference_rad": gamma,
    }


def equation_report(
    case: dict[str, Any],
    envelope_result: dict[str, Any],
    denominator_minimum: Decimal,
    derivative_minimum: Decimal,
) -> dict[str, Any]:
    altitude = d(case["altitude_m"])
    speed = d(case["speed_mps"])
    gravity = d(case["gravity_mps2"])
    mass = d(case["mass_kg"])
    area = d(case["reference_area_m2"])
    radius = d(case["reference_radius_m"]) + altitude
    rho0 = d(case["sea_level_density_kg_per_m3"])
    scale_height = d(case["density_scale_height_m"])
    rho = density(rho0, scale_height, altitude)
    rho_h = -rho / scale_height
    bank_cos = decimal_cos(d(case["bank_angle_rad"]))
    cl_vertical = d(envelope_result["cl_star"]) * bank_cos
    common = {
        "rho": rho,
        "rho_h": rho_h,
        "speed": speed,
        "gravity": gravity,
        "mass": mass,
        "area": area,
        "radius": radius,
        "cl_vertical": cl_vertical,
        "cd_star": d(envelope_result["cd_star"]),
        "denominator_minimum": denominator_minimum,
    }
    if case["equation"] == "eq18":
        result = eq18_raw(**common)
    elif case["equation"] == "eq17":
        sound = d(case["speed_of_sound_mps"])
        sound_gradient = d(case["speed_of_sound_gradient_per_m"])
        mach_v = d(1) / sound
        mach_h = -speed * sound_gradient / (sound * sound)
        result = eq17_raw(
            **common,
            dcl_vertical_dmach=d(envelope_result["dcl_star_dmach"]) * bank_cos,
            mach_v=mach_v,
            mach_h=mach_h,
            derivative_minimum=derivative_minimum,
        )
    else:
        raise ValueError(f"unknown equation {case['equation']}")
    return {"id": case["id"], **result}


def tdct_raw(
    alpha_star: Decimal,
    gamma_reference: Decimal,
    gamma_measured: Decimal,
    gain: Decimal,
    alpha_min: Decimal,
    alpha_max: Decimal,
) -> dict[str, Decimal | str]:
    require_finite(
        alpha_star, gamma_reference, gamma_measured, gain, alpha_min, alpha_max
    )
    if gain < 0 or alpha_max <= alpha_min:
        raise ModelFailure("input-domain-error")
    error = gamma_reference - gamma_measured
    correction = gain * error
    raw = alpha_star + correction
    command = max(alpha_min, min(alpha_max, raw))
    saturation = "lower" if raw < alpha_min else "upper" if raw > alpha_max else "none"
    return {
        "status": "passed",
        "error_rad": error,
        "correction_rad": correction,
        "alpha_raw_rad": raw,
        "alpha_command_rad": command,
        "saturation": saturation,
    }


def tdct_report(case: dict[str, Any]) -> dict[str, Any]:
    return {
        "id": case["id"],
        **tdct_raw(
            d(case["alpha_star_rad"]),
            d(case["gamma_reference_rad"]),
            d(case["gamma_measured_rad"]),
            d(case["gain"]),
            d(case["alpha_min_rad"]),
            d(case["alpha_max_rad"]),
        ),
    }


def capture_failure(identifier: str, operation: Callable[[], Any]) -> dict[str, str]:
    try:
        operation()
    except ModelFailure as failure:
        return {
            "id": identifier,
            "status": failure.status,
            "fallback_disposition": failure.fallback,
        }
    raise AssertionError(f"{identifier} did not fail")


def invalid_results(
    envelopes: dict[str, dict[str, Any]],
    denominator_minimum: Decimal,
    derivative_minimum: Decimal,
) -> list[dict[str, str]]:
    base_polar = {
        "cl_intercept": d(0),
        "cl_slope_per_rad": d(2),
        "cd0_base": d("0.02"),
        "cd0_slope_per_mach": d(0),
        "induced_drag_factor": d("0.08"),
        "mach": d(10),
        "alpha_min_rad": d(0),
        "alpha_max_rad": d("0.5"),
    }

    def mutated_polar(**updates: Decimal) -> dict[str, Decimal]:
        result = dict(base_polar)
        result.update(updates)
        return result

    eq_common = {
        "rho": d(2),
        "rho_h": d(-1),
        "speed": d(2),
        "gravity": d(1),
        "mass": d(1),
        "area": d(1),
        "radius": d(2),
        "cl_vertical": d(1),
        "cd_star": d("0.1"),
        "denominator_minimum": denominator_minimum,
    }
    results = [
        capture_failure(
            "INVALID-CAVH-ENVELOPE-NONPOSITIVE-CD0",
            lambda: envelope(mutated_polar(cd0_base=d(0))),
        ),
        capture_failure(
            "INVALID-CAVH-ENVELOPE-NONPOSITIVE-K",
            lambda: envelope(mutated_polar(induced_drag_factor=d(0))),
        ),
        capture_failure(
            "INVALID-CAVH-ENVELOPE-NONPOSITIVE-CL-SLOPE",
            lambda: envelope(mutated_polar(cl_slope_per_rad=d(0))),
        ),
        capture_failure(
            "INVALID-CAVH-ENVELOPE-OPTIMUM-OUTSIDE-ALPHA-DOMAIN",
            lambda: envelope(mutated_polar(alpha_max_rad=d("0.1"))),
        ),
        capture_failure(
            "INVALID-CAVH-EQ18-NONPOSITIVE-VERTICAL-LIFT",
            lambda: eq18_raw(**{**eq_common, "cl_vertical": d(0)}),
        ),
        capture_failure(
            "INVALID-CAVH-EQ18-SINGULAR-DENOMINATORS",
            lambda: eq18_raw(**{**eq_common, "rho_h": d(2)}),
        ),
        capture_failure(
            "INVALID-CAVH-EQ17-ZERO-DERIVATIVE",
            lambda: eq17_raw(
                **eq_common,
                dcl_vertical_dmach=d(0),
                mach_v=d("0.01"),
                mach_h=d(0),
                derivative_minimum=derivative_minimum,
            ),
        ),
        capture_failure(
            "INVALID-CAVH-EQ17-NONPOSITIVE-MACH-SPEED-PARTIAL",
            lambda: eq17_raw(
                **eq_common,
                dcl_vertical_dmach=d("0.1"),
                mach_v=d(0),
                mach_h=d(0),
                derivative_minimum=derivative_minimum,
            ),
        ),
        capture_failure(
            "INVALID-CAVH-TDCT-NEGATIVE-GAIN",
            lambda: tdct_raw(d("0.2"), d(0), d(0), d(-1), d(0), d(1)),
        ),
        capture_failure(
            "INVALID-CAVH-TDCT-INVALID-BOUNDS",
            lambda: tdct_raw(d("0.2"), d(0), d(0), d(1), d(1), d(1)),
        ),
        capture_failure(
            "INVALID-CAVH-TDCT-NONFINITE",
            lambda: tdct_raw(
                Decimal("NaN"), d(0), d(0), d(1), d(0), d(1)
            ),
        ),
    ]
    return results


def mutation_results(
    envelopes: dict[str, dict[str, Any]],
    derivatives: dict[str, Any],
    equations: dict[str, dict[str, Any]],
    tdct_cases: dict[str, dict[str, Any]],
    denominator_minimum: Decimal,
    derivative_minimum: Decimal,
) -> list[dict[str, Any]]:
    constant = envelopes["CASE-CAVH-ENVELOPE-CONSTANT-POLAR"]
    mach_dependent = envelopes["CASE-CAVH-ENVELOPE-MACH-DEPENDENT"]
    inverted_k_cl = (d(constant["cd0"]) * d("0.08")).sqrt()
    envelope_difference = abs(inverted_k_cl - d(constant["cl_star"]))

    cl_row = derivatives["cl_star_mach_ladder"][3]
    derivative_difference = d(cl_row["absolute_error"])

    eq18 = equations["CASE-CAVH-EQ18-UNBANKED"]
    reversed_eq18 = eq18_raw(
        rho=d(eq18["density_kg_per_m3"]),
        rho_h=-d(eq18["density_gradient_kg_per_m4"]),
        speed=d(3000),
        gravity=d("9.81"),
        mass=d(50000),
        area=d(100),
        radius=d(6401000),
        cl_vertical=d(constant["cl_star"]),
        cd_star=d(constant["cd_star"]),
        denominator_minimum=denominator_minimum,
    )
    eq18_difference = abs(
        d(reversed_eq18["gamma_reference_rad"]) - d(eq18["gamma_reference_rad"])
    )

    eq17 = equations["CASE-CAVH-EQ17-MACH-ALTITUDE-COUPLED"]
    bank_cos = decimal_cos(
        d("0.5235987755982988730771072305465838140328615665625")
    )
    eq17_without_mach_h = eq17_raw(
        rho=d(eq17["density_kg_per_m3"]),
        rho_h=d(eq17["density_gradient_kg_per_m4"]),
        speed=d(3000),
        gravity=d("9.81"),
        mass=d(50000),
        area=d(100),
        radius=d(6401000),
        cl_vertical=d(mach_dependent["cl_star"]) * bank_cos,
        cd_star=d(mach_dependent["cd_star"]),
        dcl_vertical_dmach=d(mach_dependent["dcl_star_dmach"]) * bank_cos,
        mach_v=d(1) / d(300),
        mach_h=d(0),
        denominator_minimum=denominator_minimum,
        derivative_minimum=derivative_minimum,
    )
    eq17_difference = abs(
        d(eq17_without_mach_h["gamma_reference_rad"])
        - d(eq17["gamma_reference_rad"])
    )

    unsaturated = tdct_cases["CASE-CAVH-TDCT-UNSATURATED"]
    reversed_error_alpha = d("0.25") + d(3) * (d("-0.03") - d("-0.01"))
    tdct_sign_difference = abs(
        reversed_error_alpha - d(unsaturated["alpha_command_rad"])
    )
    upper = tdct_cases["CASE-CAVH-TDCT-UPPER-SATURATION"]
    tdct_clamp_difference = abs(
        d(upper["alpha_raw_rad"]) - d(upper["alpha_command_rad"])
    )

    values = [
        ("MUTATION-CAVH-ENVELOPE-INVERTED-K", envelope_difference),
        (
            "MUTATION-CAVH-DERIVATIVE-FINITE-DIFFERENCE-AS-EXACT",
            derivative_difference,
        ),
        ("MUTATION-CAVH-EQ18-REVERSED-DENSITY-GRADIENT", eq18_difference),
        ("MUTATION-CAVH-EQ17-OMIT-MACH-ALTITUDE-TERMS", eq17_difference),
        ("MUTATION-CAVH-EQ17-SILENT-EQ18-FALLBACK", d(1)),
        ("MUTATION-CAVH-TDCT-REVERSED-ERROR", tdct_sign_difference),
        ("MUTATION-CAVH-TDCT-SKIP-CLAMP", tdct_clamp_difference),
    ]
    return [
        {"id": identifier, "status": "rejected", "difference": difference}
        for identifier, difference in values
    ]


INTERMEDIATE_UNITS = {
    "density_kg_per_m3": "kg/m^3",
    "density_gradient_kg_per_m4": "kg/m^4",
    "radius_m": "m",
    "dynamic_pressure_Pa": "Pa",
    "drag_force_N": "N",
    "cl_vertical": "1",
    "dcl_vertical_dmach": "1",
    "partial_mach_partial_speed_s_per_m": "s/m",
    "partial_mach_partial_altitude_per_m": "1/m",
    "dcl_vertical_dspeed_s_per_m": "s/m",
    "A11_A12_A13_A14_A15": "1",
    "A21_A22_A23_A24_A25": "1",
    "A31_A32_A33_A34_A35": "1",
    "B1_B2_B3": "1",
    "gamma_reference_rad": "rad",
    "tdct_angles": "rad",
}


def decimal_text(value: Decimal) -> str:
    if value == 0:
        return "0"
    return str(value.normalize()).lower()


def encode_decimals(value: Any) -> Any:
    if isinstance(value, Decimal):
        return decimal_text(value)
    if isinstance(value, dict):
        return {key: encode_decimals(item) for key, item in value.items()}
    if isinstance(value, list):
        return [encode_decimals(item) for item in value]
    return value


def build_reference(cases_path: Path, cases: dict[str, Any]) -> dict[str, Any]:
    if cases["fixture_id"] != FIXTURE_ID or cases["oracle_id"] != ORACLE_ID:
        raise ValueError("fixture or oracle identity mismatch")
    tolerances = cases["tolerances"]
    denominator_minimum = d(tolerances["denominator_minimum_absolute"])
    derivative_minimum = d(tolerances["derivative_minimum_absolute_per_mps"])
    convergence_tolerance = d(tolerances["convergence_final_absolute"])

    envelope_results: dict[str, dict[str, Any]] = {}
    for case in cases["envelope_cases"]:
        envelope_results[case["id"]] = {"id": case["id"], **envelope(case["polar"])}

    derivative_case = cases["derivative_case"]
    derivative_result = derivative_report(
        derivative_case,
        envelope_results[derivative_case["envelope_case_id"]],
        convergence_tolerance,
    )

    equation_results: dict[str, dict[str, Any]] = {}
    for case in cases["equation_cases"]:
        equation_results[case["id"]] = equation_report(
            case,
            envelope_results[case["envelope_case_id"]],
            denominator_minimum,
            derivative_minimum,
        )

    tdct_results = {case["id"]: tdct_report(case) for case in cases["tdct_cases"]}
    invalid = invalid_results(
        envelope_results, denominator_minimum, derivative_minimum
    )
    mutations = mutation_results(
        envelope_results,
        derivative_result,
        equation_results,
        tdct_results,
        denominator_minimum,
        derivative_minimum,
    )

    declared_invalid = [item["id"] for item in cases["invalid_input_cases"]]
    declared_mutations = [item["id"] for item in cases["mutation_cases"]]
    if declared_invalid != [item["id"] for item in invalid]:
        raise AssertionError("invalid case declaration mismatch")
    if declared_mutations != [item["id"] for item in mutations]:
        raise AssertionError("mutation case declaration mismatch")
    if derivative_result["status"] != "passed":
        raise AssertionError("derivative ladder did not converge")
    if any(d(item["difference"]) <= d(tolerances["formula_absolute"]) for item in mutations):
        raise AssertionError("a declared mutation is below the formula tolerance")

    raw = cases_path.read_bytes()
    reference = {
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "status": "executable",
        "precision": {"decimal_digits": DECIMAL_DIGITS},
        "input_identity": {
            "path": "fixtures/ref-cavh-formula/cases.json",
            "bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest(),
        },
        "intermediate_units": INTERMEDIATE_UNITS,
        "envelope_cases": envelope_results,
        "derivative_case": derivative_result,
        "equation_cases": equation_results,
        "tdct_cases": tdct_results,
        "invalid_input_results": invalid,
        "mutation_results": mutations,
    }
    return encode_decimals(reference)


def numeric_string(value: Any) -> Decimal | None:
    if not isinstance(value, str):
        return None
    try:
        result = Decimal(value)
    except Exception:
        return None
    return result if result.is_finite() else None


def compare_reports(
    expected: Any,
    actual: Any,
    absolute: Decimal,
    relative: Decimal,
    path: str = "$",
    convergence_ratio_absolute: Decimal = Decimal(0),
) -> tuple[int, Decimal]:
    comparisons = 0
    maximum_difference = Decimal(0)
    if isinstance(expected, dict):
        if not isinstance(actual, dict) or set(expected) != set(actual):
            raise AssertionError(f"{path}: object keys differ")
        for key in expected:
            count, difference = compare_reports(
                expected[key],
                actual[key],
                absolute,
                relative,
                f"{path}.{key}",
                convergence_ratio_absolute,
            )
            comparisons += count
            maximum_difference = max(maximum_difference, difference)
        return comparisons, maximum_difference
    if isinstance(expected, list):
        if not isinstance(actual, list) or len(expected) != len(actual):
            raise AssertionError(f"{path}: list shape differs")
        for index, (expected_item, actual_item) in enumerate(zip(expected, actual)):
            count, difference = compare_reports(
                expected_item,
                actual_item,
                absolute,
                relative,
                f"{path}[{index}]",
                convergence_ratio_absolute,
            )
            comparisons += count
            maximum_difference = max(maximum_difference, difference)
        return comparisons, maximum_difference

    expected_number = numeric_string(expected)
    if expected_number is not None and isinstance(actual, (int, float, Decimal)):
        actual_number = d(actual)
        if not actual_number.is_finite():
            raise AssertionError(f"{path}: non-finite probe value")
        difference = abs(actual_number - expected_number)
        allowed = absolute + relative * max(abs(expected_number), abs(actual_number))
        if "error_reduction_ratios" in path:
            allowed = max(allowed, convergence_ratio_absolute)
        if difference > allowed:
            raise AssertionError(
                f"{path}: {actual_number} differs from {expected_number} by "
                f"{difference}, allowed {allowed}"
            )
        return 1, difference
    if expected != actual:
        raise AssertionError(f"{path}: expected {expected!r}, got {actual!r}")
    return 1, Decimal(0)


def stored_reference_check(expected: dict[str, Any], oracle_path: Path) -> None:
    actual = json.loads(oracle_path.read_text(encoding="utf-8"))
    compare_reports(expected, actual, Decimal("1e-60"), Decimal(0))


def probe_projection(reference: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema_version": PROBE_SCHEMA,
        "fixture_id": reference["fixture_id"],
        "model_id": reference["model_id"],
        "intermediate_units": reference["intermediate_units"],
        "envelope_cases": reference["envelope_cases"],
        "derivative_case": reference["derivative_case"],
        "equation_cases": reference["equation_cases"],
        "tdct_cases": reference["tdct_cases"],
        "invalid_input_results": reference["invalid_input_results"],
        "mutation_results": reference["mutation_results"],
    }


def run_probe(
    probe: Path,
    expected: dict[str, Any],
    absolute: Decimal,
    relative: Decimal,
    convergence_ratio_absolute: Decimal,
) -> tuple[int, Decimal, str]:
    with tempfile.TemporaryDirectory(prefix="gnczmkn-cavh-") as directory:
        report_path = Path(directory) / "probe.json"
        completed = subprocess.run(
            [str(probe), "--report", str(report_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if completed.returncode != 0:
            raise AssertionError(
                f"probe failed with {completed.returncode}: "
                f"{completed.stdout.strip()} {completed.stderr.strip()}"
            )
        actual = json.loads(report_path.read_text(encoding="utf-8"))
        comparisons, maximum = compare_reports(
            probe_projection(expected),
            actual,
            absolute,
            relative,
            convergence_ratio_absolute=convergence_ratio_absolute,
        )
        return comparisons, maximum, completed.stdout.strip()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=Path, required=True)
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--write-oracle", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    with localcontext() as context:
        context.prec = DECIMAL_DIGITS
        cases = load_decimal_json(args.cases)
        expected = build_reference(args.cases, cases)
    if args.write_oracle:
        args.oracle.parent.mkdir(parents=True, exist_ok=True)
        args.oracle.write_text(
            json.dumps(expected, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
    stored_reference_check(expected, args.oracle)

    comparisons = 0
    maximum = Decimal(0)
    probe_output = ""
    if args.probe:
        comparisons, maximum, probe_output = run_probe(
            args.probe,
            expected,
            d(cases["tolerances"]["formula_absolute"]),
            d(cases["tolerances"]["formula_relative"]),
            d(cases["tolerances"]["convergence_ratio_absolute"]),
        )
    suffix = f" probe=({probe_output})" if probe_output else ""
    print(
        "CAVH formula oracle "
        f"envelopes={len(expected['envelope_cases'])} "
        f"equations={len(expected['equation_cases'])} "
        f"tdct={len(expected['tdct_cases'])} "
        f"invalid={len(expected['invalid_input_results'])} "
        f"mutations={len(expected['mutation_results'])} "
        f"comparisons={comparisons} max_abs_diff={maximum}{suffix}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

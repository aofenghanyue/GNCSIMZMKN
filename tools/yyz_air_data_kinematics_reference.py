#!/usr/bin/env python3
"""Independent Decimal reference for YYZ supplied air-data kinematics."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from decimal import Decimal, getcontext
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-AIR-DATA-KINEMATICS-001"
ORACLE_ID = "ORACLE-YYZ-AIR-DATA-KINEMATICS-001"
MODEL_ID = "MODEL-YYZ-AIR-DATA-KINEMATICS-001"
CASES_SCHEMA = "gnczmkn.yyz-air-data-kinematics-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-air-data-kinematics-reference/1"
INERTIAL_FRAME_ID = "frame.fixture.yyz.inertial-cartesian@1"
BODY_FRAME_ID = "frame.fixture.yyz.body@1"
BODY_AXES = "x-forward_y-right_z-down"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"

VECTOR3_FIELDS = (
    "vehicle_velocity_I_mps",
    "airmass_velocity_I_mps",
    "relative_velocity_I_mps",
    "relative_velocity_B_mps",
)
VECTOR4_FIELDS = (
    "q_I_B_wxyz",
    "q_B_I_wxyz",
    "q_times_relative_pure_wxyz",
    "rotated_relative_pure_wxyz",
)
SCALAR_FIELDS = (
    "quaternion_norm",
    "u_mps",
    "v_mps",
    "w_mps",
    "speed_mps",
    "horizontal_speed_uw_mps",
    "alpha_rad",
    "beta_rad",
    "dynamic_pressure_Pa",
    "mach",
)
PHYSICAL_FIELDS = (
    "relative_velocity_I_mps",
    "relative_velocity_B_mps",
    "u_mps",
    "v_mps",
    "w_mps",
    "speed_mps",
    "horizontal_speed_uw_mps",
    "alpha_rad",
    "beta_rad",
    "dynamic_pressure_Pa",
    "mach",
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


def add(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left + right for left, right in zip(lhs, rhs)]


def subtract(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left - right for left, right in zip(lhs, rhs)]


def scale(values: list[Decimal], factor: Decimal) -> list[Decimal]:
    return [factor * value for value in values]


def dot(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    return sum((left * right for left, right in zip(lhs, rhs)), Decimal(0))


def hamilton(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    lw, lx, ly, lz = lhs
    rw, rx, ry, rz = rhs
    return [
        lw * rw - lx * rx - ly * ry - lz * rz,
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
    ]


def quaternion_inverse(values: list[Decimal]) -> list[Decimal]:
    norm_squared = dot(values, values)
    require(norm_squared > 0, "q_I_B has zero norm")
    w, x, y, z = values
    return [w / norm_squared, -x / norm_squared,
            -y / norm_squared, -z / norm_squared]


def atan_series(value: Decimal) -> Decimal:
    """Evaluate atan(value) for abs(value) <= 0.5."""
    require(abs(value) <= Decimal("0.5"),
            "atan series input exceeds its convergence profile")
    if value.is_zero():
        return Decimal(0)
    squared = value * value
    power = value
    total = value
    index = 1
    threshold = Decimal(1).scaleb(-(getcontext().prec + 4))
    while True:
        power *= squared
        term = power / Decimal(2 * index + 1)
        if index % 2:
            total -= term
        else:
            total += term
        if abs(term) < threshold:
            return total
        index += 1


def pi_decimal() -> Decimal:
    """Compute pi with the Machin identity at the active Decimal precision."""
    return (Decimal(16) * atan_series(Decimal(1) / Decimal(5)) -
            Decimal(4) * atan_series(Decimal(1) / Decimal(239)))


def atan_decimal(value: Decimal) -> Decimal:
    if value < 0:
        return -atan_decimal(-value)
    if value > 1:
        return pi_decimal() / Decimal(2) - atan_decimal(Decimal(1) / value)
    if value > Decimal("0.5"):
        transformed = (value - Decimal(1)) / (value + Decimal(1))
        return pi_decimal() / Decimal(4) + atan_series(transformed)
    return atan_series(value)


def atan2_decimal(y_value: Decimal, x_value: Decimal) -> Decimal:
    require(y_value.is_finite() and x_value.is_finite(),
            "atan2 inputs must be finite")
    if x_value > 0:
        return atan_decimal(y_value / x_value)
    if x_value < 0:
        base = atan_decimal(y_value / x_value)
        return (base + pi_decimal()) if y_value >= 0 else (
            base - pi_decimal())
    if y_value > 0:
        return pi_decimal() / Decimal(2)
    if y_value < 0:
        return -pi_decimal() / Decimal(2)
    raise ValueError("atan2 is undefined for two zero arguments")


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


def validate_context(case: dict) -> None:
    context = case["context"]
    require(context["inertial_frame_id"] == INERTIAL_FRAME_ID,
            "air-data inertial frame differs")
    require(context["body_frame_id"] == BODY_FRAME_ID,
            "air-data body frame differs")
    require(context["body_axes"] == BODY_AXES,
            "air-data body axes differ")
    require(isinstance(context["sample_tick"], int) and
            not isinstance(context["sample_tick"], bool) and
            context["sample_tick"] >= 0,
            "air-data sample tick must be a nonnegative integer")
    require(context["clock_domain"] == CLOCK_DOMAIN,
            "air-data clock domain differs")

    truth = case["truth"]
    wind = case["wind"]
    atmosphere = case["atmosphere"]
    tick = context["sample_tick"]
    clock = context["clock_domain"]
    require(truth["velocity_frame_id"] == context["inertial_frame_id"] and
            truth["velocity_sample_tick"] == tick and
            truth["velocity_clock_domain"] == clock,
            "truth velocity identity differs from air-data context")
    require(truth["attitude_to_frame_id"] == context["inertial_frame_id"] and
            truth["attitude_from_frame_id"] == context["body_frame_id"] and
            truth["attitude_sample_tick"] == tick and
            truth["attitude_clock_domain"] == clock,
            "truth attitude identity differs from air-data context")
    require(wind["velocity_frame_id"] == context["inertial_frame_id"] and
            wind["sample_tick"] == tick and
            wind["clock_domain"] == clock,
            "wind identity differs from air-data context")
    require(atmosphere["sample_tick"] == tick and
            atmosphere["clock_domain"] == clock,
            "atmosphere identity differs from air-data context")


def air_data_reference(case: dict, tolerances: dict,
                       wind_mode: str = "subtract",
                       rotation_mode: str = "q_p_inverse",
                       angle_mode: str = "accepted",
                       sound_speed_mode: str = "accepted") -> dict:
    validate_context(case)
    vehicle = vector(case["truth"]["velocity_I_mps"], 3,
                     "vehicle velocity")
    airmass = vector(case["wind"]["velocity_airmass_I_mps"], 3,
                     "air-mass velocity")
    quaternion = vector(case["truth"]["q_I_B_wxyz"], 4, "q_I_B")
    density = decimal(case["atmosphere"]["density_kgpm3"])
    sound_speed = decimal(case["atmosphere"]["speed_of_sound_mps"])

    quaternion_norm = dot(quaternion, quaternion).sqrt()
    unit_error = abs(quaternion_norm - Decimal(1))
    norm_absolute = decimal(tolerances["quaternion_norm_absolute"])
    norm_relative = decimal(tolerances["quaternion_norm_relative"])
    unit_bound = norm_absolute + norm_relative * max(
        abs(quaternion_norm), Decimal(1))
    require(quaternion_norm > 0 and unit_error <= unit_bound,
            "q_I_B is outside the Error normalization policy")
    require(density >= 0, "air density must be nonnegative")
    require(sound_speed > 0, "speed of sound must be positive")

    require(wind_mode in ("subtract", "add"), "unsupported wind mode")
    relative_i = (subtract(vehicle, airmass)
                  if wind_mode == "subtract" else add(vehicle, airmass))
    inverse = quaternion_inverse(quaternion)
    pure_relative = [Decimal(0), *relative_i]
    require(rotation_mode in ("q_p_inverse", "inverse_p_q"),
            "unsupported rotation mode")
    if rotation_mode == "q_p_inverse":
        left_intermediate = hamilton(quaternion, pure_relative)
        rotated = hamilton(left_intermediate, inverse)
    else:
        left_intermediate = hamilton(inverse, pure_relative)
        rotated = hamilton(left_intermediate, quaternion)
    relative_b = rotated[1:]
    require(all(value.is_finite() for value in relative_i + inverse +
                left_intermediate + rotated),
            "air-data rotation produced a non-finite value")

    u_value, v_value, w_value = relative_b
    speed = dot(relative_b, relative_b).sqrt()
    horizontal = (u_value * u_value + w_value * w_value).sqrt()
    require(speed.is_finite() and speed > 0,
            "relative speed must be finite and positive")
    require(horizontal.is_finite() and horizontal > 0,
            "u/w horizontal speed must be finite and positive")

    require(angle_mode in ("accepted", "legacy_clamps"),
            "unsupported angle mode")
    if angle_mode == "accepted":
        alpha = atan2_decimal(w_value, u_value)
        beta = atan2_decimal(v_value, horizontal)
    else:
        epsilon = Decimal("1e-9")
        alpha = atan2_decimal(w_value, max(epsilon, u_value))
        beta = atan2_decimal(v_value, max(epsilon, abs(u_value)))

    require(sound_speed_mode in ("accepted", "floor_one"),
            "unsupported sound-speed mode")
    denominator = (sound_speed if sound_speed_mode == "accepted"
                   else max(Decimal(1), sound_speed))
    dynamic_pressure = Decimal("0.5") * density * speed * speed
    mach = speed / denominator
    derived = [quaternion_norm, u_value, v_value, w_value, speed,
               horizontal, alpha, beta, dynamic_pressure, mach]
    require(all(value.is_finite() for value in derived),
            "air-data formula produced a non-finite value")
    pi_value = pi_decimal()
    require(-pi_value < alpha <= pi_value,
            "alpha is outside (-pi,pi]")
    require(-pi_value / Decimal(2) < beta < pi_value / Decimal(2),
            "beta is outside (-pi/2,pi/2)")

    return {
        "id": case["id"],
        "context": copy.deepcopy(case["context"]),
        "vehicle_velocity_I_mps": vehicle,
        "airmass_velocity_I_mps": airmass,
        "relative_velocity_I_mps": relative_i,
        "q_I_B_wxyz": quaternion,
        "quaternion_norm": quaternion_norm,
        "q_B_I_wxyz": inverse,
        "q_times_relative_pure_wxyz": left_intermediate,
        "rotated_relative_pure_wxyz": rotated,
        "relative_velocity_B_mps": relative_b,
        "u_mps": u_value,
        "v_mps": v_value,
        "w_mps": w_value,
        "speed_mps": speed,
        "horizontal_speed_uw_mps": horizontal,
        "alpha_rad": alpha,
        "beta_rad": beta,
        "dynamic_pressure_Pa": dynamic_pressure,
        "mach": mach,
    }


def cases_by_id(cases: dict) -> dict[str, dict]:
    result = {case["id"]: case for case in cases["cases"]}
    require(len(result) == len(cases["cases"]),
            "duplicate air-data case id")
    return result


def numeric_values(result: dict, fields: tuple[str, ...]) -> list[Decimal]:
    values: list[Decimal] = []
    for field in fields:
        value = result[field]
        if isinstance(value, list):
            values.extend(decimal(item) for item in value)
        else:
            values.append(decimal(value))
    return values


def max_physical_difference(lhs: dict, rhs: dict) -> Decimal:
    left_values = numeric_values(lhs, PHYSICAL_FIELDS)
    right_values = numeric_values(rhs, PHYSICAL_FIELDS)
    return max((abs(left - right)
                for left, right in zip(left_values, right_values)),
               default=Decimal(0))


def equivalent(lhs: dict, rhs: dict, tolerance: Decimal) -> bool:
    return max_physical_difference(lhs, rhs) <= tolerance


def reference_equivalence_results(cases: dict) -> list[dict]:
    indexed = cases_by_id(cases)
    tolerances = cases["tolerances"]
    strict = Decimal("1e-68")

    rotation_case = copy.deepcopy(
        indexed["CASE-YYZ-AIR-DATA-PASSIVE-ROTATION"])
    accepted_rotation = air_data_reference(rotation_case, tolerances)
    rotation_case["truth"]["q_I_B_wxyz"] = [
        -decimal(value) for value in rotation_case["truth"]["q_I_B_wxyz"]]
    sign_equivalent = air_data_reference(rotation_case, tolerances)

    wind_case = copy.deepcopy(
        indexed["CASE-YYZ-AIR-DATA-WIND-SUBTRACTION"])
    accepted_wind = air_data_reference(wind_case, tolerances)
    offset = [Decimal(17), Decimal(-23), Decimal(5)]
    wind_case["truth"]["velocity_I_mps"] = add(
        vector(wind_case["truth"]["velocity_I_mps"], 3, "vehicle"),
        offset)
    wind_case["wind"]["velocity_airmass_I_mps"] = add(
        vector(wind_case["wind"]["velocity_airmass_I_mps"], 3, "wind"),
        offset)
    offset_equivalent = air_data_reference(wind_case, tolerances)

    results = [
        {
            "id": "EQUIV-YYZ-AIR-DATA-QUATERNION-SIGN",
            "status": "passed" if equivalent(
                accepted_rotation, sign_equivalent, strict) else "failed",
            "max_abs_physical_difference": max_physical_difference(
                accepted_rotation, sign_equivalent),
        },
        {
            "id": "EQUIV-YYZ-AIR-DATA-COMMON-INERTIAL-VELOCITY",
            "status": "passed" if equivalent(
                accepted_wind, offset_equivalent, strict) else "failed",
            "max_abs_physical_difference": max_physical_difference(
                accepted_wind, offset_equivalent),
        },
    ]
    require(all(result["status"] == "passed" for result in results),
            "Python air-data equivalence check failed")
    return results


def rejects(operation) -> bool:
    try:
        operation()
    except (KeyError, TypeError, ValueError):
        return True
    return False


def reference_invalid_rejections(cases: dict) -> list[str]:
    base = cases_by_id(cases)["CASE-YYZ-AIR-DATA-WIND-SUBTRACTION"]
    tolerances = cases["tolerances"]
    mutations = []

    def add_mutation(identifier: str, mutate) -> None:
        value = copy.deepcopy(base)
        mutate(value)
        if rejects(lambda: air_data_reference(value, tolerances)):
            mutations.append(identifier)

    add_mutation("INVALID-YYZ-AIR-DATA-FRAME-MISMATCH",
                 lambda value: value["wind"].__setitem__(
                     "velocity_frame_id", "frame.other@1"))
    add_mutation("INVALID-YYZ-AIR-DATA-SAMPLE-TICK-MISMATCH",
                 lambda value: value["atmosphere"].__setitem__(
                     "sample_tick", value["context"]["sample_tick"] + 1))
    add_mutation("INVALID-YYZ-AIR-DATA-NONFINITE-VELOCITY",
                 lambda value: value["truth"]["velocity_I_mps"].__setitem__(
                     0, Decimal("Infinity")))
    add_mutation("INVALID-YYZ-AIR-DATA-ZERO-QUATERNION",
                 lambda value: value["truth"].__setitem__(
                     "q_I_B_wxyz", [0, 0, 0, 0]))
    add_mutation("INVALID-YYZ-AIR-DATA-NONUNIT-QUATERNION",
                 lambda value: value["truth"].__setitem__(
                     "q_I_B_wxyz", [Decimal("0.8"), 0, 0, 0]))
    add_mutation("INVALID-YYZ-AIR-DATA-NEGATIVE-DENSITY",
                 lambda value: value["atmosphere"].__setitem__(
                     "density_kgpm3", Decimal("-0.1")))
    add_mutation("INVALID-YYZ-AIR-DATA-NONPOSITIVE-SOUND-SPEED",
                 lambda value: value["atmosphere"].__setitem__(
                     "speed_of_sound_mps", 0))
    add_mutation("INVALID-YYZ-AIR-DATA-ZERO-RELATIVE-VELOCITY",
                 lambda value: value["truth"].__setitem__(
                     "velocity_I_mps",
                     copy.deepcopy(value["wind"]["velocity_airmass_I_mps"])))

    def pure_lateral(value: dict) -> None:
        value["truth"]["velocity_I_mps"] = [0, 10, 0]
        value["wind"]["velocity_airmass_I_mps"] = [0, 0, 0]

    add_mutation("INVALID-YYZ-AIR-DATA-PURE-LATERAL-VELOCITY",
                 pure_lateral)
    return mutations


def reference_mutation_results(cases: dict) -> list[dict]:
    indexed = cases_by_id(cases)
    tolerances = cases["tolerances"]
    profiles = [
        ("MUTATION-YYZ-AIR-DATA-ADD-WIND",
         "CASE-YYZ-AIR-DATA-WIND-SUBTRACTION",
         {"wind_mode": "add"}),
        ("MUTATION-YYZ-AIR-DATA-REVERSE-QUATERNION-DIRECTION",
         "CASE-YYZ-AIR-DATA-PASSIVE-ROTATION",
         {"rotation_mode": "inverse_p_q"}),
        ("MUTATION-YYZ-AIR-DATA-LEGACY-ANGLE-CLAMPS",
         "CASE-YYZ-AIR-DATA-REARWARD-FLOW",
         {"angle_mode": "legacy_clamps"}),
        ("MUTATION-YYZ-AIR-DATA-SOUND-SPEED-FLOOR",
         "CASE-YYZ-AIR-DATA-SUB-ONE-SOUND-SPEED",
         {"sound_speed_mode": "floor_one"}),
    ]
    results = []
    for identifier, case_id, options in profiles:
        accepted = air_data_reference(indexed[case_id], tolerances)
        mutated = air_data_reference(indexed[case_id], tolerances, **options)
        difference = max_physical_difference(accepted, mutated)
        results.append({
            "id": identifier,
            "status": "rejected" if difference > Decimal("1e-30")
            else "matched",
            "max_abs_physical_difference": difference,
        })
    require(all(result["status"] == "rejected" for result in results),
            "Python air-data reference accepted a physical mutation")
    return results


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    indexed_results = {}
    for case in cases["cases"]:
        indexed_results[case["id"]] = air_data_reference(
            case, cases["tolerances"])
    invalid = reference_invalid_rejections(cases)
    expected_invalid = [entry["id"] for entry in cases["invalid_input_cases"]]
    require(invalid == expected_invalid,
            "Python air-data invalid-input coverage differs")
    return stringify({
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "status": "executable",
        "precision": {
            "decimal_digits": getcontext().prec,
            "angle_method": "Machin pi plus transformed alternating atan series",
        },
        "input_identity": {
            "path": "fixtures/ref-yyz-air-data-kinematics/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": indexed_results,
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
            f"C++ air-data probe failed: {completed.stderr.strip()}")
    return completed.stdout, json.loads(
        completed.stdout, parse_float=Decimal)


def compare_case(checks: Checks, actual: dict, expected: dict,
                 absolute: Decimal, relative: Decimal) -> None:
    case_id = expected["id"]
    checks.require(actual["id"] == case_id,
                   f"C++ air-data case identity differs for {case_id}")
    checks.require(actual["context"] == expected["context"],
                   f"C++ air-data context differs for {case_id}", 5)
    for field in VECTOR3_FIELDS:
        compare_vector(checks, actual[field], expected[field],
                       absolute, relative, f"{case_id}.{field}")
    for field in VECTOR4_FIELDS:
        compare_vector(checks, actual[field], expected[field],
                       absolute, relative, f"{case_id}.{field}")
    for field in SCALAR_FIELDS:
        compare_scalar(checks, actual[field], expected[field],
                       absolute, relative, f"{case_id}.{field}")


def verify_reference(cases: dict, raw_cases: bytes, oracle: dict,
                     probe_path: Path) -> dict:
    checks = Checks()
    checks.require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
                   "air-data fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "air-data oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "air-data model identity differs", 2)
    checks.require(oracle["precision"]["decimal_digits"] >= 70,
                   "air-data oracle precision is below 70 digits")
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "air-data cases byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-air-data-kinematics/cases.json",
                   "air-data input path differs")

    recomputed = build_reference(cases, raw_cases)
    checks.require(oracle == recomputed,
                   "stored air-data oracle differs from its Decimal producer",
                   len(oracle["cases"]) + 4)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ air-data probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed" and
                   probe["model_choice_status"] ==
                   cases["model_choice"]["status"],
                   "C++ air-data probe identity differs", 4)

    probe_cases = {entry["id"]: entry for entry in probe["cases"]}
    checks.require(len(probe_cases) == len(probe["cases"]) ==
                   len(oracle["cases"]),
                   "C++ air-data case identities are incomplete", 2)
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    for case_id, expected in oracle["cases"].items():
        checks.require(case_id in probe_cases,
                       f"C++ air-data case is missing: {case_id}")
        compare_case(checks, probe_cases[case_id], expected,
                     absolute, relative)

    expected_equivalence = {
        entry["id"] for entry in cases["equivalence_cases"]
    }
    checks.require(set(probe["equivalence_checks"]) == expected_equivalence,
                   "C++ air-data equivalence identities differ")
    expected_invalid = {
        entry["id"] for entry in cases["invalid_input_cases"]
    }
    checks.require(set(probe["invalid_input_rejections"]) == expected_invalid,
                   "C++ air-data invalid-input identities differ")
    expected_mutations = {
        entry["id"] for entry in cases["mutation_cases"]
    }
    checks.require(set(probe["mutation_rejections"]) == expected_mutations,
                   "C++ air-data mutation identities differ")

    rearward = probe_cases["CASE-YYZ-AIR-DATA-REARWARD-FLOW"]
    passive = probe_cases["CASE-YYZ-AIR-DATA-PASSIVE-ROTATION"]
    sub_one = probe_cases["CASE-YYZ-AIR-DATA-SUB-ONE-SOUND-SPEED"]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "case_count": len(probe_cases),
        "passive_rotation_relative_velocity_B_mps":
            passive["relative_velocity_B_mps"],
        "rearward_alpha_rad": rearward["alpha_rad"],
        "rearward_beta_rad": rearward["beta_rad"],
        "sub_one_sound_speed_mach": sub_one["mach"],
        "equivalence_cases_passed": len(expected_equivalence),
        "invalid_input_cases_rejected": len(expected_invalid),
        "mutation_cases_rejected": len(expected_mutations),
    })


def validate_cases_identity(cases: dict) -> None:
    require(cases["schema_version"] == CASES_SCHEMA and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model"]["model_id"] == MODEL_ID,
            "air-data cases identity differs")
    require(cases["model_choice"]["status"] == "accepted",
            "air-data model choice is not accepted")
    require(cases["model"]["quaternion_normalization_policy"] == "Error",
            "air-data quaternion policy differs")


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
        print(f"YYZ air-data kinematics reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

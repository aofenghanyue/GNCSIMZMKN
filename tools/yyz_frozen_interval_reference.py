#!/usr/bin/env python3
"""Independent Decimal reference for the composed YYZ FrozenInterval slice."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal, getcontext
import hashlib
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-FROZEN-INTERVAL-001"
ORACLE_ID = "ORACLE-YYZ-FROZEN-INTERVAL-001"
MODEL_ID = "MODEL-YYZ-FROZEN-INTERVAL-001"
CASES_SCHEMA = "gnczmkn.yyz-frozen-interval-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-frozen-interval-reference/1"
PROFILE_STATUS = "implemented-from-accepted-profiles"
INERTIAL_FRAME_ID = "frame.fixture.yyz.inertial-cartesian@1"
BODY_FRAME_ID = "frame.fixture.yyz.body@1"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
MASS_STATE_ID = "mass.fixture.yyz.vehicle@1"


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


def add(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left + right for left, right in zip(lhs, rhs)]


def subtract(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left - right for left, right in zip(lhs, rhs)]


def scale(values: list[Decimal], factor: Decimal) -> list[Decimal]:
    return [value * factor for value in values]


def dot(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    return sum((left * right for left, right in zip(lhs, rhs)), Decimal(0))


def cross(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [
        lhs[1] * rhs[2] - lhs[2] * rhs[1],
        lhs[2] * rhs[0] - lhs[0] * rhs[2],
        lhs[0] * rhs[1] - lhs[1] * rhs[0],
    ]


def hamilton(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    lw, lx, ly, lz = lhs
    rw, rx, ry, rz = rhs
    return [
        lw * rw - lx * rx - ly * ry - lz * rz,
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
    ]


def normalize_quaternion(value: list[Decimal]) -> list[Decimal]:
    magnitude_squared = dot(value, value)
    require(magnitude_squared > 0, "q_I_B must have nonzero norm")
    magnitude = magnitude_squared.sqrt()
    return scale(value, Decimal(1) / magnitude)


def quaternion_inverse(value: list[Decimal]) -> list[Decimal]:
    magnitude_squared = dot(value, value)
    require(magnitude_squared > 0, "q_I_B must have nonzero norm")
    w_value, x_value, y_value, z_value = value
    return [w_value / magnitude_squared, -x_value / magnitude_squared,
            -y_value / magnitude_squared, -z_value / magnitude_squared]


def inertial_to_body(q_i_b: list[Decimal], value_i: list[Decimal]) -> list[Decimal]:
    unit = normalize_quaternion(q_i_b)
    rotated = hamilton(hamilton(unit, [Decimal(0), *value_i]),
                       quaternion_inverse(unit))
    return rotated[1:]


def body_to_inertial(q_i_b: list[Decimal], value_b: list[Decimal]) -> list[Decimal]:
    unit = normalize_quaternion(q_i_b)
    rotated = hamilton(
        hamilton(quaternion_inverse(unit), [Decimal(0), *value_b]), unit)
    return rotated[1:]


def atan_series(value: Decimal) -> Decimal:
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
        total = total - term if index % 2 else total + term
        if abs(term) < threshold:
            return total
        index += 1


def pi_decimal() -> Decimal:
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
    if x_value > 0:
        return atan_decimal(y_value / x_value)
    if x_value < 0:
        base = atan_decimal(y_value / x_value)
        return base + pi_decimal() if y_value >= 0 else base - pi_decimal()
    if y_value > 0:
        return pi_decimal() / Decimal(2)
    if y_value < 0:
        return -pi_decimal() / Decimal(2)
    raise ValueError("atan2 is undefined for two zero arguments")


def matrix(values: object, label: str) -> list[list[Decimal]]:
    parsed = vector(values, 9, label)
    return [parsed[0:3], parsed[3:6], parsed[6:9]]


def flatten(value: list[list[Decimal]]) -> list[Decimal]:
    return [entry for row in value for entry in row]


def matrix_vector(product: list[list[Decimal]],
                  value: list[Decimal]) -> list[Decimal]:
    return [dot(row, value) for row in product]


def cholesky(inertia: list[list[Decimal]]) -> list[list[Decimal]]:
    require(all(inertia[row][column] == inertia[column][row]
                for row in range(3) for column in range(3)),
            "inertia must be symmetric")
    lower = [[Decimal(0) for _ in range(3)] for _ in range(3)]
    for row in range(3):
        for column in range(row + 1):
            residual = inertia[row][column] - sum(
                (lower[row][index] * lower[column][index]
                 for index in range(column)), Decimal(0))
            if row == column:
                require(residual > 0, "inertia must be positive definite")
                lower[row][column] = residual.sqrt()
            else:
                lower[row][column] = residual / lower[column][column]
    return lower


def solve_spd(inertia: list[list[Decimal]],
              rhs: list[Decimal]) -> list[Decimal]:
    lower = cholesky(inertia)
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


def valid_nonnegative_integer(value: object) -> bool:
    return (isinstance(value, int) and not isinstance(value, bool) and
            value >= 0)


def validate_shared_identity(section: dict, context: dict,
                             label: str, body: bool = False,
                             interval: bool = False) -> None:
    if body:
        require(section["body_frame_id"] == context["body_frame_id"],
                f"{label} body frame differs")
    else:
        require(section["inertial_frame_id"] == context["inertial_frame_id"],
                f"{label} inertial frame differs")
    require(section["clock_domain"] == context["clock_domain"],
            f"{label} clock domain differs")
    require(section["sample_tick"] == context["sample_tick"],
            f"{label} sample tick differs")
    require(section["configuration_revision"] ==
            context["configuration_revision"],
            f"{label} configuration revision differs")
    if interval:
        require(section["valid_from_tick"] == context["valid_from_tick"] and
                section["valid_until_tick"] == context["valid_until_tick"],
                f"{label} validity interval differs")


def validate_case(case: dict) -> None:
    context = case["context"]
    require(context["inertial_frame_id"] == INERTIAL_FRAME_ID and
            context["body_frame_id"] == BODY_FRAME_ID and
            context["clock_domain"] == CLOCK_DOMAIN,
            "FrozenInterval context identity differs")
    require(valid_nonnegative_integer(context["sample_tick"]) and
            valid_nonnegative_integer(context["configuration_revision"]) and
            valid_nonnegative_integer(context["valid_from_tick"]) and
            valid_nonnegative_integer(context["valid_until_tick"]),
            "FrozenInterval tick or revision is invalid")
    require(context["sample_tick"] == context["valid_from_tick"] and
            context["valid_until_tick"] == context["sample_tick"] + 1,
            "FrozenInterval must span one half-open tick interval")
    dt_s = decimal(context["base_dt_s"])
    require(dt_s > 0, "base_dt_s must be positive")

    environment = case["environment_sample"]
    mass = case["mass_properties_sample"]
    aero = case["aero_response"]
    propulsion = case["propulsion_response"]
    validate_shared_identity(environment, context, "environment")
    validate_shared_identity(mass, context, "MassProperties", True, True)
    validate_shared_identity(aero, context, "aero", True, True)
    validate_shared_identity(propulsion, context, "propulsion", True, True)
    require(mass["mass_state_id"] == MASS_STATE_ID,
            "MassProperties state identity differs")
    require(aero["source_id"] and propulsion["source_id"] and
            aero["source_id"] != propulsion["source_id"],
            "Closure contribution source identities must be distinct")

    state = case["initial_state"]
    vector(state["position_I_m"], 3, "initial position")
    vector(state["velocity_I_mps"], 3, "initial velocity")
    normalize_quaternion(vector(state["q_I_B_wxyz"], 4, "initial q_I_B"))
    vector(state["omega_BI_B_radps"], 3, "initial angular rate")

    vector(environment["gravity_I_mps2"], 3, "gravity")
    vector(environment["velocity_airmass_I_mps"], 3, "air-mass velocity")
    require(decimal(environment["density_kgpm3"]) >= 0 and
            decimal(environment["speed_of_sound_mps"]) > 0,
            "environment density or sound speed is invalid")

    mass_value = decimal(mass["mass_kg"])
    require(mass_value > 0, "current mass must be positive")
    vector(mass["r_body_origin_to_CoM_B_m"], 3, "center of mass")
    cholesky(matrix(mass["inertia_about_CoM_B_kgm2_row_major"], "inertia"))

    require(decimal(aero["reference_area_m2"]) > 0 and
            decimal(aero["reference_span_m"]) > 0 and
            decimal(aero["reference_chord_m"]) > 0,
            "aero reference geometry must be positive")
    vector(aero["coefficients_CA_CY_CN_Cl_Cm_Cn"], 6,
           "aero coefficients")
    vector(aero["r_body_origin_to_application_B_m"], 3,
           "aero application point")

    require(decimal(propulsion["thrust_magnitude_N"]) >= 0 and
            decimal(propulsion["fuel_consumption_rate_kgps"]) >= 0,
            "propulsion thrust or consumption is negative")
    thrust_direction = vector(
        propulsion["thrust_direction_B_unit"], 3, "thrust direction")
    require(abs(dot(thrust_direction, thrust_direction).sqrt() - Decimal(1))
            <= Decimal("2e-12"), "thrust direction must be unit length")
    vector(propulsion["r_body_origin_to_application_B_m"], 3,
           "propulsion application point")
    vector(propulsion["intrinsic_moment_at_application_B_Nm"], 3,
           "propulsion intrinsic moment")
    candidate = mass_value - decimal(
        propulsion["fuel_consumption_rate_kgps"]) * dt_s
    require(candidate > 0, "scalar mass candidate must be positive")
    require(case["terminal"]["kind"] == "duration_exact_grid" and
            case["terminal"]["expected_tick"] ==
            context["valid_until_tick"],
            "FrozenInterval terminal identity differs")


def compose(case: dict, *, wind_mode: str = "subtract",
            mass_mode: str = "current",
            propulsion_moment_mode: str = "application",
            include_trajectory: bool = True) -> dict:
    validate_case(case)
    context = case["context"]
    state = case["initial_state"]
    environment = case["environment_sample"]
    mass = case["mass_properties_sample"]
    aero = case["aero_response"]
    propulsion = case["propulsion_response"]
    dt_s = decimal(context["base_dt_s"])

    position_i = vector(state["position_I_m"], 3, "position")
    velocity_i = vector(state["velocity_I_mps"], 3, "velocity")
    q_i_b = normalize_quaternion(
        vector(state["q_I_B_wxyz"], 4, "q_I_B"))
    omega_b = vector(state["omega_BI_B_radps"], 3, "angular rate")
    gravity_i = vector(environment["gravity_I_mps2"], 3, "gravity")
    airmass_i = vector(
        environment["velocity_airmass_I_mps"], 3, "air-mass velocity")
    density = decimal(environment["density_kgpm3"])
    sound_speed = decimal(environment["speed_of_sound_mps"])
    velocity_relative_i = (subtract(velocity_i, airmass_i)
                           if wind_mode == "subtract"
                           else add(velocity_i, airmass_i))
    velocity_relative_b = inertial_to_body(q_i_b, velocity_relative_i)
    u_value, v_value, w_value = velocity_relative_b
    speed = dot(velocity_relative_b, velocity_relative_b).sqrt()
    require(speed > 0, "this composed fixture requires positive airspeed")
    horizontal = (u_value * u_value + w_value * w_value).sqrt()
    alpha = atan2_decimal(w_value, u_value)
    beta = atan2_decimal(v_value, horizontal)
    dynamic_pressure = density * speed * speed / Decimal(2)
    mach = speed / sound_speed

    center_of_mass = vector(
        mass["r_body_origin_to_CoM_B_m"], 3, "center of mass")
    inertia = matrix(mass["inertia_about_CoM_B_kgm2_row_major"], "inertia")
    coefficients = vector(
        aero["coefficients_CA_CY_CN_Cl_Cm_Cn"], 6, "coefficients")
    ca_value, cy_value, cn_value, cl_value, cm_value, cn_moment = coefficients
    area = decimal(aero["reference_area_m2"])
    span = decimal(aero["reference_span_m"])
    chord = decimal(aero["reference_chord_m"])
    pressure_area = dynamic_pressure * area
    aero_force = scale([-ca_value, cy_value, -cn_value], pressure_area)
    aero_moment_application = [
        pressure_area * span * cl_value,
        pressure_area * chord * cm_value,
        pressure_area * span * cn_moment,
    ]
    aero_application = vector(
        aero["r_body_origin_to_application_B_m"], 3,
        "aero application")
    aero_lever = subtract(aero_application, center_of_mass)
    aero_transport = cross(aero_lever, aero_force)
    aero_moment_com = add(aero_moment_application, aero_transport)

    thrust = decimal(propulsion["thrust_magnitude_N"])
    thrust_direction = vector(
        propulsion["thrust_direction_B_unit"], 3, "thrust direction")
    propulsion_force = scale(thrust_direction, thrust)
    propulsion_application = vector(
        propulsion["r_body_origin_to_application_B_m"], 3,
        "propulsion application")
    propulsion_lever = subtract(propulsion_application, center_of_mass)
    propulsion_transport = cross(propulsion_lever, propulsion_force)
    propulsion_intrinsic = vector(
        propulsion["intrinsic_moment_at_application_B_Nm"], 3,
        "propulsion intrinsic moment")
    propulsion_moment_application = (
        propulsion_intrinsic if propulsion_moment_mode == "application"
        else add(propulsion_intrinsic, propulsion_transport))
    propulsion_moment_com = add(
        propulsion_moment_application, propulsion_transport)

    force_total_b = add(aero_force, propulsion_force)
    moment_total_b = add(aero_moment_com, propulsion_moment_com)
    current_mass = decimal(mass["mass_kg"])
    consumed_mass = decimal(
        propulsion["fuel_consumption_rate_kgps"]) * dt_s
    mass_candidate = current_mass - consumed_mass
    integration_mass = (current_mass if mass_mode == "current"
                        else mass_candidate)
    force_total_i = body_to_inertial(q_i_b, force_total_b)
    acceleration_i = add(
        scale(force_total_i, Decimal(1) / integration_mass), gravity_i)
    angular_momentum_b = matrix_vector(inertia, omega_b)
    gyroscopic_b = cross(omega_b, angular_momentum_b)
    net_moment_b = subtract(moment_total_b, gyroscopic_b)
    angular_acceleration_b = solve_spd(inertia, net_moment_b)
    pure_omega = [Decimal(0), *omega_b]
    q_derivative = scale(hamilton(pure_omega, q_i_b), Decimal("-0.5"))

    result = {
        "id": case["id"],
        "context": {
            "sample_tick": context["sample_tick"],
            "valid_from_tick": context["valid_from_tick"],
            "valid_until_tick": context["valid_until_tick"],
            "configuration_revision": context["configuration_revision"],
            "base_dt_s": dt_s,
        },
        "environment_sample": {
            "gravity_I_mps2": gravity_i,
            "velocity_airmass_I_mps": airmass_i,
            "density_kgpm3": density,
            "speed_of_sound_mps": sound_speed,
        },
        "air_data": {
            "velocity_relative_I_mps": velocity_relative_i,
            "velocity_relative_B_mps": velocity_relative_b,
            "airspeed_mps": speed,
            "alpha_rad": alpha,
            "beta_rad": beta,
            "dynamic_pressure_Pa": dynamic_pressure,
            "mach": mach,
        },
        "aero_contribution": {
            "source_id": aero["source_id"],
            "force_B_N": aero_force,
            "r_CoM_to_application_B_m": aero_lever,
            "moment_at_application_B_Nm": aero_moment_application,
            "transport_moment_B_Nm": aero_transport,
            "moment_about_CoM_B_Nm": aero_moment_com,
        },
        "propulsion_contribution": {
            "source_id": propulsion["source_id"],
            "force_B_N": propulsion_force,
            "r_CoM_to_application_B_m": propulsion_lever,
            "moment_at_application_B_Nm": propulsion_moment_application,
            "transport_moment_B_Nm": propulsion_transport,
            "moment_about_CoM_B_Nm": propulsion_moment_com,
        },
        "mass_visibility": {
            "mass_state_id": mass["mass_state_id"],
            "current_visible_mass_kg": current_mass,
            "consumed_mass_kg": consumed_mass,
            "pending_mass_candidate_kg": mass_candidate,
            "pending_visibility_before_commit": "candidate-only",
            "integration_mass_kg": integration_mass,
            "next_commit_tick": context["valid_until_tick"],
        },
        "closure": {
            "force_total_B_N": force_total_b,
            "moment_total_about_CoM_B_Nm": moment_total_b,
        },
        "rigid_derivative_at_tick0": {
            "force_total_I_N": force_total_i,
            "acceleration_I_mps2": acceleration_i,
            "angular_momentum_B_kgm2ps": angular_momentum_b,
            "gyroscopic_moment_B_Nm": gyroscopic_b,
            "net_moment_B_Nm": net_moment_b,
            "angular_acceleration_B_radps2": angular_acceleration_b,
            "q_derivative_I_B_per_s": q_derivative,
        },
    }
    if include_trajectory:
        require(all(value.is_zero() for value in omega_b) and
                all(value.is_zero() for value in moment_total_b),
                "analytic trajectory requires zero rotation")
        half_dt_squared = dt_s * dt_s / Decimal(2)
        final_position = add(
            add(position_i, scale(velocity_i, dt_s)),
            scale(acceleration_i, half_dt_squared))
        final_velocity = add(velocity_i, scale(acceleration_i, dt_s))
        result["analytic_terminal"] = {
            "tick": case["terminal"]["expected_tick"],
            "time_s": Decimal(case["terminal"]["expected_tick"]) * dt_s,
            "position_I_m": final_position,
            "velocity_I_mps": final_velocity,
            "q_I_B_wxyz": q_i_b,
            "omega_BI_B_radps": omega_b,
            "termination_kind": case["terminal"]["kind"],
        }
    return result


def max_difference(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    require(len(lhs) == len(rhs), "numeric vectors have different lengths")
    return max((abs(left - right) for left, right in zip(lhs, rhs)),
               default=Decimal(0))


def physical_vector(result: dict) -> list[Decimal]:
    values: list[Decimal] = []
    for section, fields in (
        ("air_data", ("velocity_relative_I_mps", "velocity_relative_B_mps",
                      "airspeed_mps", "alpha_rad", "beta_rad",
                      "dynamic_pressure_Pa", "mach")),
        ("aero_contribution", ("force_B_N", "moment_about_CoM_B_Nm")),
        ("propulsion_contribution", ("force_B_N", "moment_about_CoM_B_Nm")),
        ("mass_visibility", ("current_visible_mass_kg",
                             "pending_mass_candidate_kg",
                             "integration_mass_kg")),
        ("closure", ("force_total_B_N",
                     "moment_total_about_CoM_B_Nm")),
        ("rigid_derivative_at_tick0", ("acceleration_I_mps2",
                                       "angular_acceleration_B_radps2")),
        ("analytic_terminal", ("position_I_m", "velocity_I_mps",
                               "omega_BI_B_radps")),
    ):
        for field in fields:
            value = result[section][field]
            values.extend(value if isinstance(value, list) else [value])
    return values


def invalid_rejections(cases: dict, accepted_case: dict) -> list[str]:
    actions = {
        "INVALID-YYZ-FROZEN-INTERVAL-BODY-FRAME-MISMATCH":
            lambda item: item["aero_response"].__setitem__(
                "body_frame_id", "frame.fixture.yyz.other@1"),
        "INVALID-YYZ-FROZEN-INTERVAL-CLOCK-MISMATCH":
            lambda item: item["environment_sample"].__setitem__(
                "clock_domain", "clock.fixture.yyz.other@1"),
        "INVALID-YYZ-FROZEN-INTERVAL-SAMPLE-TICK-MISMATCH":
            lambda item: item["mass_properties_sample"].__setitem__(
                "sample_tick", 1),
        "INVALID-YYZ-FROZEN-INTERVAL-INTERVAL-MISMATCH":
            lambda item: item["propulsion_response"].__setitem__(
                "valid_until_tick", 2),
        "INVALID-YYZ-FROZEN-INTERVAL-REVISION-MISMATCH":
            lambda item: item["aero_response"].__setitem__(
                "configuration_revision", 12),
        "INVALID-YYZ-FROZEN-INTERVAL-NONPOSITIVE-DT":
            lambda item: item["context"].__setitem__("base_dt_s", 0),
        "INVALID-YYZ-FROZEN-INTERVAL-NONPOSITIVE-MASS":
            lambda item: item["mass_properties_sample"].__setitem__(
                "mass_kg", 0),
        "INVALID-YYZ-FROZEN-INTERVAL-ZERO-QUATERNION":
            lambda item: item["initial_state"].__setitem__(
                "q_I_B_wxyz", [0, 0, 0, 0]),
    }
    rejected: list[str] = []
    for specification in cases["invalid_input_cases"]:
        identifier = specification["id"]
        require(identifier in actions,
                f"unsupported invalid-input case: {identifier}")
        mutated = copy.deepcopy(accepted_case)
        actions[identifier](mutated)
        try:
            compose(mutated)
        except (ArithmeticError, IndexError, KeyError, TypeError, ValueError):
            rejected.append(identifier)
        else:
            raise ValueError(f"invalid input was accepted: {identifier}")
    return rejected


def mutation_results(cases: dict, accepted_case: dict,
                     accepted: dict) -> list[dict]:
    add_wind = compose(accepted_case, wind_mode="add")
    early_mass = compose(accepted_case, mass_mode="candidate")
    pretransported = compose(
        accepted_case, propulsion_moment_mode="pretransported",
        include_trajectory=False)
    results = [
        {
            "id": "MUTATION-YYZ-FROZEN-INTERVAL-ADD-WIND",
            "status": "rejected",
            "observed_velocity_relative_I_mps":
                add_wind["air_data"]["velocity_relative_I_mps"],
            "observed_dynamic_pressure_Pa":
                add_wind["air_data"]["dynamic_pressure_Pa"],
            "observed_force_total_B_N":
                add_wind["closure"]["force_total_B_N"],
            "observed_terminal_position_I_m":
                add_wind["analytic_terminal"]["position_I_m"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(add_wind)),
        },
        {
            "id": "MUTATION-YYZ-FROZEN-INTERVAL-EARLY-MASS-CANDIDATE",
            "status": "rejected",
            "observed_integration_mass_kg":
                early_mass["mass_visibility"]["integration_mass_kg"],
            "observed_acceleration_I_mps2":
                early_mass["rigid_derivative_at_tick0"][
                    "acceleration_I_mps2"],
            "observed_terminal_position_I_m":
                early_mass["analytic_terminal"]["position_I_m"],
            "observed_terminal_velocity_I_mps":
                early_mass["analytic_terminal"]["velocity_I_mps"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(early_mass)),
        },
        {
            "id": "MUTATION-YYZ-FROZEN-INTERVAL-PRETRANSPORTED-PROPULSION-MOMENT",
            "status": "rejected",
            "observed_propulsion_moment_at_application_B_Nm":
                pretransported["propulsion_contribution"][
                    "moment_at_application_B_Nm"],
            "observed_moment_total_about_CoM_B_Nm":
                pretransported["closure"][
                    "moment_total_about_CoM_B_Nm"],
            "observed_angular_acceleration_B_radps2":
                pretransported["rigid_derivative_at_tick0"][
                    "angular_acceleration_B_radps2"],
            "max_abs_physical_difference": max_difference(
                accepted["closure"]["moment_total_about_CoM_B_Nm"],
                pretransported["closure"][
                    "moment_total_about_CoM_B_Nm"]),
        },
    ]
    require([entry["id"] for entry in results] ==
            [entry["id"] for entry in cases["mutation_cases"]],
            "mutation case identities differ")
    require(all(entry["max_abs_physical_difference"] > 0
                for entry in results), "a composition mutation survived")
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
            cases["semantic_profile"]["status"] == PROFILE_STATUS and
            cases["model"]["model_id"] == MODEL_ID,
            "FrozenInterval cases identity differs")
    model = cases["model"]
    require(model["inertial_frame_id"] == INERTIAL_FRAME_ID and
            model["body_frame_id"] == BODY_FRAME_ID and
            model["clock_domain"] == CLOCK_DOMAIN and
            model["mass_state_id"] == MASS_STATE_ID and
            model["strategy"] == "FrozenInterval",
            "FrozenInterval model profile differs")
    require(len(cases["cases"]) == 1,
            "FrozenInterval bundle must contain one analytic case")


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    validate_cases_identity(cases)
    accepted_case = cases["cases"][0]
    accepted = compose(accepted_case)
    sign_case = copy.deepcopy(accepted_case)
    sign_case["initial_state"]["q_I_B_wxyz"] = [
        -decimal(value) for value in
        sign_case["initial_state"]["q_I_B_wxyz"]]
    sign_result = compose(sign_case)
    sign_difference = max_difference(
        physical_vector(accepted), physical_vector(sign_result))
    require(sign_difference == 0,
            "quaternion sign changed the physical FrozenInterval result")
    equivalence = [{
        "id": "EQUIV-YYZ-FROZEN-INTERVAL-QUATERNION-SIGN",
        "status": "passed",
        "alternate_q_I_B_wxyz": normalize_quaternion(vector(
            sign_case["initial_state"]["q_I_B_wxyz"], 4, "alternate q")),
        "orientation_error_rad": Decimal(0),
        "max_abs_physical_difference": sign_difference,
    }]
    reference = {
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "precision": {
            "implementation": "Python standard-library Decimal analytic composition",
            "decimal_digits": getcontext().prec,
        },
        "input_identity": {
            "path": "fixtures/ref-yyz-frozen-interval/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": {accepted_case["id"]: accepted},
        "equivalence_results": equivalence,
        "invalid_input_rejections": invalid_rejections(
            cases, accepted_case),
        "mutation_results": mutation_results(cases, accepted_case, accepted),
    }
    return stringify(reference)


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
                   "FrozenInterval fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "FrozenInterval oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "FrozenInterval model identity differs", 2)
    checks.require(oracle["precision"]["decimal_digits"] >= 70,
                   "FrozenInterval reference precision is below 70 digits")
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "FrozenInterval input byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-frozen-interval/cases.json",
                   "FrozenInterval input path differs")
    recomputed = build_reference(cases, raw_cases)
    checks.require(oracle == recomputed,
                   "stored FrozenInterval oracle differs from its producer",
                   len(oracle["cases"]) + 4)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ FrozenInterval probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed" and
                   probe["semantic_profile_status"] == PROFILE_STATUS,
                   "C++ FrozenInterval probe identity differs", 4)
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    expected_cases = list(oracle["cases"].values())
    compare_tree(checks, probe["cases"], expected_cases,
                 absolute, relative, "cases")
    compare_tree(checks, probe["equivalence_results"],
                 oracle["equivalence_results"], absolute, relative,
                 "equivalence_results")
    checks.require(probe["invalid_input_rejections"] ==
                   oracle["invalid_input_rejections"],
                   "FrozenInterval invalid-input identities differ",
                   len(oracle["invalid_input_rejections"]))
    compare_tree(checks, probe["mutation_results"],
                 oracle["mutation_results"], absolute, relative,
                 "mutation_results")

    accepted = probe["cases"][0]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "case_count": len(probe["cases"]),
        "dynamic_pressure_Pa":
            accepted["air_data"]["dynamic_pressure_Pa"],
        "force_total_B_N": accepted["closure"]["force_total_B_N"],
        "moment_total_about_CoM_B_Nm":
            accepted["closure"]["moment_total_about_CoM_B_Nm"],
        "current_visible_mass_kg":
            accepted["mass_visibility"]["current_visible_mass_kg"],
        "pending_mass_candidate_kg":
            accepted["mass_visibility"]["pending_mass_candidate_kg"],
        "terminal_position_I_m":
            accepted["analytic_terminal"]["position_I_m"],
        "terminal_velocity_I_mps":
            accepted["analytic_terminal"]["velocity_I_mps"],
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
    except (ArithmeticError, IndexError, KeyError, OSError, TypeError,
            ValueError, json.JSONDecodeError,
            subprocess.SubprocessError) as error:
        print(f"YYZ FrozenInterval reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

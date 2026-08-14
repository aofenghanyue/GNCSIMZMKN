#!/usr/bin/env python3
"""Independent Decimal reference for ORACLE-YYZ-6DOF-CORE-001."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from decimal import Decimal, getcontext, localcontext
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-6DOF-CORE-001"
ORACLE_ID = "ORACLE-YYZ-6DOF-CORE-001"
MODEL_ID = "MODEL-YYZ-6DOF-RIGID-CORE-001"
PI = Decimal(
    "3.141592653589793238462643383279502884197169399375105820974944592"
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite decimal value: {value}")
    return result


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def vector(values: list[object], label: str) -> list[Decimal]:
    require(len(values) == 3, f"{label} must have three components")
    return [decimal(value) for value in values]


def quaternion(values: list[object], label: str) -> list[Decimal]:
    require(len(values) == 4, f"{label} must have four components")
    return [decimal(value) for value in values]


def add(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left + right for left, right in zip(lhs, rhs)]


def subtract(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left - right for left, right in zip(lhs, rhs)]


def scale(values: list[Decimal], factor: Decimal) -> list[Decimal]:
    return [factor * value for value in values]


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


def conjugate(value: list[Decimal]) -> list[Decimal]:
    return [value[0], -value[1], -value[2], -value[3]]


def normalize_quaternion(value: list[Decimal], label: str) -> list[Decimal]:
    norm_squared = dot(value, value)
    require(norm_squared > 0, f"{label} has zero norm")
    norm = norm_squared.sqrt()
    return [coefficient / norm for coefficient in value]


def passive_rotate(q_i_b: list[Decimal], value_b: list[Decimal]) -> list[Decimal]:
    unit = normalize_quaternion(q_i_b, "q_I_B")
    pure = [Decimal(0)] + value_b
    return hamilton(hamilton(conjugate(unit), pure), unit)[1:]


def matrix(values: list[object], label: str) -> list[list[Decimal]]:
    require(len(values) == 9, f"{label} must contain nine row-major values")
    parsed = [decimal(value) for value in values]
    return [parsed[0:3], parsed[3:6], parsed[6:9]]


def matrix_vector(product: list[list[Decimal]], value: list[Decimal]) -> list[Decimal]:
    return [dot(row, value) for row in product]


def cholesky(inertia: list[list[Decimal]]) -> list[list[Decimal]]:
    require(all(inertia[row][column] == inertia[column][row]
                for row in range(3) for column in range(3)),
            "inertia_B must be symmetric")
    lower = [[Decimal(0) for _ in range(3)] for _ in range(3)]
    for row in range(3):
        for column in range(row + 1):
            residual = inertia[row][column] - sum(
                (lower[row][index] * lower[column][index]
                 for index in range(column)), Decimal(0))
            if row == column:
                require(residual > 0,
                        "inertia_B must be symmetric positive definite")
                lower[row][column] = residual.sqrt()
            else:
                lower[row][column] = residual / lower[column][column]
    return lower


def solve_spd(inertia: list[list[Decimal]], rhs: list[Decimal]) -> list[Decimal]:
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


def validate_inputs(state: dict, inputs: dict) -> None:
    mass = decimal(inputs["mass_kg"])
    require(mass > 0, "mass_kg must be strictly positive")
    cholesky(matrix(inputs["inertia_B_kgm2_row_major"], "inertia_B"))
    vector(inputs["force_B_N"], "force_B")
    vector(inputs["moment_B_Nm"], "moment_B")
    vector(inputs["gravity_I_mps2"], "gravity_I")
    vector(state["position_I_m"], "position_I")
    vector(state["velocity_I_mps"], "velocity_I")
    normalize_quaternion(
        quaternion(state["q_I_B_wxyz"], "q_I_B"), "q_I_B")
    vector(state["omega_BI_B_radps"], "omega_BI_B")


def formula_reference(case: dict) -> dict:
    state = case["state"]
    inputs = case["inputs"]
    validate_inputs(state, inputs)
    velocity = vector(state["velocity_I_mps"], "velocity_I")
    q_i_b = normalize_quaternion(
        quaternion(state["q_I_B_wxyz"], "q_I_B"), "q_I_B")
    omega = vector(state["omega_BI_B_radps"], "omega_BI_B")
    mass = decimal(inputs["mass_kg"])
    inertia = matrix(inputs["inertia_B_kgm2_row_major"], "inertia_B")
    force_body = vector(inputs["force_B_N"], "force_B")
    moment_body = vector(inputs["moment_B_Nm"], "moment_B")
    gravity = vector(inputs["gravity_I_mps2"], "gravity_I")

    force_inertial = passive_rotate(q_i_b, force_body)
    mass_reciprocal = Decimal(1) / mass
    acceleration = add(scale(force_inertial, mass_reciprocal), gravity)
    angular_momentum = matrix_vector(inertia, omega)
    gyroscopic = cross(omega, angular_momentum)
    net_moment = subtract(moment_body, gyroscopic)
    angular_acceleration = solve_spd(inertia, net_moment)
    attitude_derivative = scale(
        hamilton([Decimal(0)] + omega, q_i_b), Decimal("-0.5"))
    return {
        "position_derivative_I_mps": velocity,
        "force_I_N": force_inertial,
        "mass_reciprocal_per_kg": mass_reciprocal,
        "gravity_I_mps2": gravity,
        "velocity_derivative_I_mps2": acceleration,
        "angular_momentum_B_kgm2ps": angular_momentum,
        "gyroscopic_moment_B_Nm": gyroscopic,
        "net_moment_B_Nm": net_moment,
        "omega_derivative_B_radps2": angular_acceleration,
        "q_derivative_I_B_per_s": attitude_derivative,
    }


def decimal_sin_cos(value: Decimal) -> tuple[Decimal, Decimal]:
    with localcontext() as context:
        context.prec = getcontext().prec + 12
        two_pi = Decimal(2) * PI
        reduced = value % two_pi
        if reduced > PI:
            reduced -= two_pi
        if reduced < -PI:
            reduced += two_pi
        epsilon = Decimal(10) ** (-(getcontext().prec + 6))

        sine = reduced
        sine_term = reduced
        index = 1
        while True:
            sine_term *= -reduced * reduced / Decimal((2 * index) * (2 * index + 1))
            sine += sine_term
            if abs(sine_term) < epsilon:
                break
            index += 1

        cosine = Decimal(1)
        cosine_term = Decimal(1)
        index = 1
        while True:
            cosine_term *= -reduced * reduced / Decimal((2 * index - 1) * (2 * index))
            cosine += cosine_term
            if abs(cosine_term) < epsilon:
                break
            index += 1
        return +sine, +cosine


def state_from_case(case: dict) -> dict:
    source = case["initial_state"]
    return {
        "position_I_m": vector(source["position_I_m"], "position_I"),
        "velocity_I_mps": vector(source["velocity_I_mps"], "velocity_I"),
        "q_I_B_wxyz": normalize_quaternion(
            quaternion(source["q_I_B_wxyz"], "q_I_B"), "q_I_B"),
        "omega_BI_B_radps": vector(
            source["omega_BI_B_radps"], "omega_BI_B"),
    }


def analytic_translation_state(case: dict, time_s: Decimal) -> dict:
    initial = state_from_case(case)
    formula_case = {
        "state": case["initial_state"],
        "inputs": case["inputs"],
    }
    acceleration = formula_reference(formula_case)[
        "velocity_derivative_I_mps2"]
    position = add(
        add(initial["position_I_m"],
            scale(initial["velocity_I_mps"], time_s)),
        scale(acceleration, Decimal("0.5") * time_s * time_s),
    )
    velocity = add(initial["velocity_I_mps"], scale(acceleration, time_s))
    return {
        "position_I_m": position,
        "velocity_I_mps": velocity,
        "q_I_B_wxyz": initial["q_I_B_wxyz"],
        "omega_BI_B_radps": initial["omega_BI_B_radps"],
    }


def analytic_spin_state(case: dict, time_s: Decimal) -> dict:
    initial = state_from_case(case)
    omega = initial["omega_BI_B_radps"]
    require(omega[0] == 0 and omega[1] == 0,
            "analytic principal-spin case must use the body z principal axis")
    sine, cosine = decimal_sin_cos(Decimal("0.5") * omega[2] * time_s)
    rotation = [cosine, Decimal(0), Decimal(0), -sine]
    attitude = normalize_quaternion(
        hamilton(rotation, initial["q_I_B_wxyz"]), "analytic q_I_B")
    return {
        "position_I_m": add(
            initial["position_I_m"],
            scale(initial["velocity_I_mps"], time_s)),
        "velocity_I_mps": initial["velocity_I_mps"],
        "q_I_B_wxyz": attitude,
        "omega_BI_B_radps": omega,
    }


STATE_FIELDS = (
    "position_I_m",
    "velocity_I_mps",
    "q_I_B_wxyz",
    "omega_BI_B_radps",
)


def state_derivative_reference(state: dict, inputs: dict) -> dict:
    formula = formula_reference({"state": state, "inputs": inputs})
    return {
        "position_I_m": formula["position_derivative_I_mps"],
        "velocity_I_mps": formula["velocity_derivative_I_mps2"],
        "q_I_B_wxyz": formula["q_derivative_I_B_per_s"],
        "omega_BI_B_radps": formula["omega_derivative_B_radps2"],
    }


def add_scaled_state(state: dict, derivative: dict,
                     factor: Decimal) -> dict:
    return {
        field: add(state[field], scale(derivative[field], factor))
        for field in STATE_FIELDS
    }


def weighted_state_derivative(k1: dict, k2: dict,
                              k3: dict, k4: dict) -> dict:
    one_sixth = Decimal(1) / Decimal(6)
    return {
        field: scale(add(add(k1[field], scale(k2[field], Decimal(2))),
                         add(scale(k3[field], Decimal(2)), k4[field])),
                     one_sixth)
        for field in STATE_FIELDS
    }


def decimal_rk4_step(state: dict, inputs: dict,
                     dt_s: Decimal) -> tuple[dict, Decimal]:
    require(dt_s > 0, "Decimal RK4 dt must be positive")
    k1 = state_derivative_reference(state, inputs)
    k2 = state_derivative_reference(
        add_scaled_state(state, k1, dt_s / Decimal(2)), inputs)
    k3 = state_derivative_reference(
        add_scaled_state(state, k2, dt_s / Decimal(2)), inputs)
    k4 = state_derivative_reference(
        add_scaled_state(state, k3, dt_s), inputs)
    candidate = add_scaled_state(
        state, weighted_state_derivative(k1, k2, k3, k4), dt_s)
    candidate_norm = dot(
        candidate["q_I_B_wxyz"], candidate["q_I_B_wxyz"]).sqrt()
    norm_residual = abs(candidate_norm - Decimal(1))
    candidate["q_I_B_wxyz"] = normalize_quaternion(
        candidate["q_I_B_wxyz"], "Decimal RK4 candidate q_I_B")
    validate_inputs(candidate, inputs)
    return candidate, norm_residual


def rotational_invariants(state: dict, inputs: dict) -> dict:
    inertia = matrix(inputs["inertia_B_kgm2_row_major"], "inertia_B")
    omega = vector(state["omega_BI_B_radps"], "omega_BI_B")
    angular_momentum = matrix_vector(inertia, omega)
    return {
        "rotational_kinetic_energy_J":
            Decimal("0.5") * dot(omega, angular_momentum),
        "angular_momentum_norm_kgm2ps":
            dot(angular_momentum, angular_momentum).sqrt(),
    }


def decimal_integrate(case: dict, dt_value: object,
                      sample_dt_value: object | None = None) -> dict:
    dt_s = decimal(dt_value)
    steps = exact_grid_steps(case["duration_s"], dt_s)
    sample_dt_s = dt_s if sample_dt_value is None else decimal(sample_dt_value)
    sample_interval = exact_grid_steps(sample_dt_s, dt_s)
    require(sample_interval > 0 and steps % sample_interval == 0,
            "stored trajectory sample grid must align to reference dt")

    state = state_from_case(case)
    initial_invariants = rotational_invariants(state, case["inputs"])
    maximum_norm_residual = Decimal(0)
    maximum_energy_drift = Decimal(0)
    maximum_momentum_drift = Decimal(0)
    samples = [{"tick": 0, "time_s": Decimal(0), **state}]
    for step in range(steps):
        state, norm_residual = decimal_rk4_step(
            state, case["inputs"], dt_s)
        maximum_norm_residual = max(maximum_norm_residual, norm_residual)
        invariants = rotational_invariants(state, case["inputs"])
        maximum_energy_drift = max(
            maximum_energy_drift,
            abs(invariants["rotational_kinetic_energy_J"] -
                initial_invariants["rotational_kinetic_energy_J"]))
        maximum_momentum_drift = max(
            maximum_momentum_drift,
            abs(invariants["angular_momentum_norm_kgm2ps"] -
                initial_invariants["angular_momentum_norm_kgm2ps"]))
        committed_step = step + 1
        if committed_step % sample_interval == 0:
            samples.append({
                "tick": committed_step // sample_interval,
                "time_s": Decimal(committed_step) * dt_s,
                **state,
            })
    return {
        "trajectory": samples,
        "final_state": state,
        "maximum_precommit_quaternion_norm_residual":
            maximum_norm_residual,
        "initial_invariants": initial_invariants,
        "maximum_invariant_drift": {
            "rotational_kinetic_energy_J": maximum_energy_drift,
            "angular_momentum_norm_kgm2ps": maximum_momentum_drift,
        },
    }


def quaternion_chord(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    left = normalize_quaternion(lhs, "left q_I_B")
    right = normalize_quaternion(rhs, "right q_I_B")
    if dot(left, right) < 0:
        right = scale(right, Decimal(-1))
    difference = subtract(left, right)
    return dot(difference, difference).sqrt()


def vector_l2_difference(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    difference = subtract(lhs, rhs)
    return dot(difference, difference).sqrt()


def exact_grid_steps(duration: object, dt: object) -> int:
    duration_decimal = decimal(duration)
    dt_decimal = decimal(dt)
    require(dt_decimal > 0 and duration_decimal >= 0,
            "duration and dt must define a nonnegative positive-step grid")
    steps = duration_decimal / dt_decimal
    integral = steps.to_integral_value()
    require(steps == integral, "duration must align to ExactGrid")
    return int(integral)


def trajectory(case: dict, dt_key: str, analytic) -> list[dict]:
    dt = decimal(case[dt_key])
    steps = exact_grid_steps(case["duration_s"], dt)
    return [{
        "tick": tick,
        "time_s": Decimal(tick) * dt,
        **analytic(case, Decimal(tick) * dt),
    } for tick in range(steps + 1)]


def failure_reference(case: dict) -> dict:
    dt = decimal(case["dt_s"])
    last_state = state_from_case(case)
    last_state["position_I_m"] = add(
        last_state["position_I_m"],
        scale(last_state["velocity_I_mps"], dt),
    )
    return {
        "code": "reference-domain-error",
        "stage": case["expected_failure_stage"],
        "evaluation_time_s": decimal(
            case["evaluation_time_strictly_less_than_s"]),
        "failed_step_start_tick": 1,
        "candidate_disposition": "discarded",
        "last_committed_tick": 1,
        "last_committed_state": last_state,
    }


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


def cases_by_id(cases: dict) -> dict[str, dict]:
    result = {case["id"]: case for case in cases["cases"]}
    require(len(result) == len(cases["cases"]), "duplicate YYZ core case id")
    return result


def call_rejected(function) -> bool:
    try:
        function()
    except ValueError:
        return True
    return False


def independent_invalid_rejections(cases: dict) -> set[str]:
    by_id = cases_by_id(cases)
    base = by_id["CASE-YYZ6-COUPLED-DERIVATIVE"]
    rejected_ids: set[str] = set()

    candidate = copy.deepcopy(base)
    candidate["inputs"]["mass_kg"] = Decimal(0)
    if call_rejected(lambda: formula_reference(candidate)):
        rejected_ids.add("INVALID-YYZ6-NONPOSITIVE-MASS")

    candidate = copy.deepcopy(base)
    candidate["inputs"]["inertia_B_kgm2_row_major"][1] = Decimal(2)
    if call_rejected(lambda: formula_reference(candidate)):
        rejected_ids.add("INVALID-YYZ6-ASYMMETRIC-INERTIA")

    candidate = copy.deepcopy(base)
    candidate["inputs"]["inertia_B_kgm2_row_major"][4] = Decimal(-1)
    if call_rejected(lambda: formula_reference(candidate)):
        rejected_ids.add("INVALID-YYZ6-NON-SPD-INERTIA")

    candidate = copy.deepcopy(base)
    candidate["state"]["q_I_B_wxyz"] = [
        Decimal(0), Decimal(0), Decimal(0), Decimal(0)]
    if call_rejected(lambda: formula_reference(candidate)):
        rejected_ids.add("INVALID-YYZ6-ZERO-QUATERNION")

    candidate = copy.deepcopy(base)
    candidate["inputs"]["force_B_N"][0] = Decimal("Infinity")
    if call_rejected(lambda: formula_reference(candidate)):
        rejected_ids.add("INVALID-YYZ6-NONFINITE-INPUT")

    if call_rejected(lambda: exact_grid_steps(Decimal(1), Decimal(0))):
        rejected_ids.add("INVALID-YYZ6-ZERO-DT")
    if call_rejected(lambda: exact_grid_steps(Decimal(1), Decimal("0.3"))):
        rejected_ids.add("INVALID-YYZ6-NON-GRID-DURATION")
    return rejected_ids


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    by_id = cases_by_id(cases)
    formula_case = by_id["CASE-YYZ6-COUPLED-DERIVATIVE"]
    translation_case = by_id["CASE-YYZ6-CONSTANT-TRANSLATION"]
    spin_case = by_id["CASE-YYZ6-PRINCIPAL-SPIN-CONVERGENCE"]
    coupled_case = by_id[
        "CASE-YYZ6-TORQUE-FREE-COUPLED-CONVERGENCE"]
    failure_case = by_id["CASE-YYZ6-RK-STAGE-DOMAIN-FAILURE"]
    coupled_reference = decimal_integrate(
        coupled_case, coupled_case["reference_integration_dt_s"],
        coupled_case["stored_trajectory_dt_s"])
    coupled_confirmation = decimal_integrate(
        coupled_case, coupled_case["reference_confirmation_dt_s"],
        coupled_case["duration_s"])
    coupled_final = coupled_reference["final_state"]
    confirmation_final = coupled_confirmation["final_state"]
    return stringify({
        "schema_version": "gnczmkn.yyz-6dof-core-reference/1",
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "input_identity": {
            "path": "fixtures/ref-yyz-6dof-core/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "reference_method": {
            "implementation": "CPython standard-library decimal",
            "precision_digits": getcontext().prec,
            "solution": "direct formula evaluation, closed-form constant translation and principal-axis spin, plus fine-step Decimal RK4 for coupled torque-free rotation",
        },
        "model_choice_status": cases["model_choice"]["status"],
        "cases": {
            formula_case["id"]: {
                "formula_intermediates": formula_reference(formula_case),
            },
            translation_case["id"]: {
                "analytic_trajectory": trajectory(
                    translation_case, "dt_s", analytic_translation_state),
                "terminal": {
                    "kind": "duration_exact_grid",
                    "tick": exact_grid_steps(
                        translation_case["duration_s"],
                        translation_case["dt_s"]),
                    "time_s": decimal(translation_case["duration_s"]),
                },
            },
            spin_case["id"]: {
                "analytic_trajectory": trajectory(
                    spin_case, "reference_trajectory_dt_s",
                    analytic_spin_state),
                "analytic_final": analytic_spin_state(
                    spin_case, decimal(spin_case["duration_s"])),
            },
            coupled_case["id"]: {
                "high_precision_trajectory":
                    coupled_reference["trajectory"],
                "high_precision_final": coupled_final,
                "reference_convergence": {
                    "reference_integration_dt_s": decimal(
                        coupled_case["reference_integration_dt_s"]),
                    "reference_confirmation_dt_s": decimal(
                        coupled_case["reference_confirmation_dt_s"]),
                    "final_quaternion_chord": quaternion_chord(
                        coupled_final["q_I_B_wxyz"],
                        confirmation_final["q_I_B_wxyz"]),
                    "final_angular_rate_l2_radps": vector_l2_difference(
                        coupled_final["omega_BI_B_radps"],
                        confirmation_final["omega_BI_B_radps"]),
                },
                "maximum_precommit_quaternion_norm_residual":
                    coupled_reference[
                        "maximum_precommit_quaternion_norm_residual"],
                "initial_invariants":
                    coupled_reference["initial_invariants"],
                "maximum_invariant_drift":
                    coupled_reference["maximum_invariant_drift"],
            },
            failure_case["id"]: {
                "failure": failure_reference(failure_case),
            },
        },
        "invalid_input_cases": cases["invalid_input_cases"],
    })


def near(actual: Decimal, expected: Decimal,
         absolute: Decimal, relative: Decimal) -> bool:
    error = abs(actual - expected)
    return error <= absolute + relative * max(abs(actual), abs(expected))


class Checks:
    def __init__(self) -> None:
        self.count = 0

    def require(self, condition: bool, message: str, count: int = 1) -> None:
        require(condition, message)
        self.count += count


def compare_scalar(checks: Checks, actual: object, expected: object,
                   absolute: Decimal, relative: Decimal, label: str) -> None:
    actual_decimal = decimal(actual)
    expected_decimal = decimal(expected)
    checks.require(near(actual_decimal, expected_decimal, absolute, relative),
                   f"{label} differs: {actual_decimal} vs {expected_decimal}")


def compare_vector(checks: Checks, actual: list[object], expected: list[object],
                   absolute: Decimal, relative: Decimal, label: str) -> None:
    checks.require(len(actual) == len(expected), f"{label} width differs")
    for index, (actual_value, expected_value) in enumerate(zip(actual, expected)):
        compare_scalar(checks, actual_value, expected_value,
                       absolute, relative, f"{label}[{index}]")


def orientation_error(actual: list[object], expected: list[object]) -> float:
    actual_q = normalize_quaternion(
        [decimal(value) for value in actual], "actual q_I_B")
    expected_q = normalize_quaternion(
        [decimal(value) for value in expected], "expected q_I_B")
    if dot(actual_q, expected_q) < 0:
        expected_q = scale(expected_q, Decimal(-1))
    chord = subtract(actual_q, expected_q)
    chord_norm = dot(chord, chord).sqrt()
    return 4.0 * math.asin(min(1.0, 0.5 * float(chord_norm)))


def compare_state(checks: Checks, actual: dict, expected: dict,
                  tolerances: dict, orientation_limit: Decimal,
                  label: str,
                  angular_rate_absolute: Decimal | None = None,
                  angular_rate_relative: Decimal | None = None) -> None:
    compare_vector(
        checks, actual["position_I_m"], expected["position_I_m"],
        decimal(tolerances["position_absolute_m"]),
        decimal(tolerances["position_relative"]), f"{label}.position")
    compare_vector(
        checks, actual["velocity_I_mps"], expected["velocity_I_mps"],
        decimal(tolerances["velocity_absolute_mps"]),
        decimal(tolerances["velocity_relative"]), f"{label}.velocity")
    compare_vector(
        checks, actual["omega_BI_B_radps"], expected["omega_BI_B_radps"],
        (decimal(tolerances["angular_rate_absolute_radps"])
         if angular_rate_absolute is None else angular_rate_absolute),
        (decimal(tolerances["angular_rate_relative"])
         if angular_rate_relative is None else angular_rate_relative),
        f"{label}.omega")
    error = Decimal(str(orientation_error(
        actual["q_I_B_wxyz"], expected["q_I_B_wxyz"])))
    checks.require(error <= orientation_limit,
                   f"{label}.orientation error {error} exceeds {orientation_limit}")


def run_probe(path: Path) -> tuple[str, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    require(completed.returncode == 0,
            f"C++ YYZ core probe failed: {completed.stderr.strip()}")
    return completed.stdout, json.loads(
        completed.stdout, parse_float=Decimal)


def verify_reference(cases: dict, raw_cases: bytes, oracle: dict,
                     probe_path: Path) -> dict:
    checks = Checks()
    checks.require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
                   "YYZ core fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "YYZ core oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] == oracle["model_id"] == MODEL_ID,
                   "YYZ core model identity differs", 2)
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "YYZ core cases byte identity differs", 2)
    checks.require(identity["path"] == "fixtures/ref-yyz-6dof-core/cases.json",
                   "YYZ core input path differs")
    decision_status = cases["model_choice"]["status"]
    checks.require(decision_status in {"needs_owner_decision", "accepted"} and
                   oracle["model_choice_status"] == decision_status,
                   "YYZ core model-choice status differs", 2)

    recomputed = build_reference(cases, raw_cases)
    stored_tolerance = decimal(cases["tolerances"]["stored_decimal_absolute"])
    by_id = cases_by_id(cases)
    formula_id = "CASE-YYZ6-COUPLED-DERIVATIVE"
    stored_formula = oracle["cases"][formula_id]["formula_intermediates"]
    recomputed_formula = recomputed["cases"][formula_id]["formula_intermediates"]
    checks.require(set(stored_formula) == set(recomputed_formula),
                   "stored formula intermediate identities differ")
    for field in stored_formula:
        stored = stored_formula[field]
        expected = recomputed_formula[field]
        if isinstance(stored, list):
            compare_vector(checks, stored, expected,
                           stored_tolerance, Decimal(0),
                           f"stored formula {field}")
        else:
            compare_scalar(checks, stored, expected,
                           stored_tolerance, Decimal(0),
                           f"stored formula {field}")

    for case_id in (
            "CASE-YYZ6-CONSTANT-TRANSLATION",
            "CASE-YYZ6-PRINCIPAL-SPIN-CONVERGENCE"):
        stored_trajectory = oracle["cases"][case_id]["analytic_trajectory"]
        recomputed_trajectory = recomputed["cases"][case_id]["analytic_trajectory"]
        checks.require(len(stored_trajectory) == len(recomputed_trajectory),
                       f"stored {case_id} trajectory length differs")
        for index, (stored_sample, expected_sample) in enumerate(
                zip(stored_trajectory, recomputed_trajectory)):
            checks.require(stored_sample["tick"] == expected_sample["tick"],
                           f"stored {case_id} tick {index} differs")
            compare_scalar(checks, stored_sample["time_s"],
                           expected_sample["time_s"], stored_tolerance,
                           Decimal(0), f"stored {case_id} time {index}")
            for field in ("position_I_m", "velocity_I_mps",
                          "q_I_B_wxyz", "omega_BI_B_radps"):
                compare_vector(checks, stored_sample[field],
                               expected_sample[field], stored_tolerance,
                               Decimal(0), f"stored {case_id} {field} {index}")

    spin_id = "CASE-YYZ6-PRINCIPAL-SPIN-CONVERGENCE"
    stored_spin_final = oracle["cases"][spin_id]["analytic_final"]
    recomputed_spin_final = recomputed["cases"][spin_id]["analytic_final"]
    checks.require(set(stored_spin_final) == set(recomputed_spin_final),
                   "stored spin final-state identities differ")
    for field in ("position_I_m", "velocity_I_mps",
                  "q_I_B_wxyz", "omega_BI_B_radps"):
        compare_vector(checks, stored_spin_final[field],
                       recomputed_spin_final[field], stored_tolerance,
                       Decimal(0), f"stored spin final {field}")

    coupled_id = "CASE-YYZ6-TORQUE-FREE-COUPLED-CONVERGENCE"
    stored_coupled = oracle["cases"][coupled_id]
    recomputed_coupled = recomputed["cases"][coupled_id]
    checks.require(stored_coupled == recomputed_coupled,
                   "stored torque-free coupled reference differs")
    reference_confirmation_limit = decimal(
        cases["tolerances"]["reference_confirmation_state_error_max"])
    checks.require(
        decimal(stored_coupled["reference_convergence"][
            "final_quaternion_chord"]) <= reference_confirmation_limit,
        "Decimal torque-free quaternion reference did not converge")
    checks.require(
        decimal(stored_coupled["reference_convergence"][
            "final_angular_rate_l2_radps"]) <=
        reference_confirmation_limit,
        "Decimal torque-free angular-rate reference did not converge")
    checks.require(
        decimal(stored_coupled[
            "maximum_precommit_quaternion_norm_residual"]) <=
        decimal(cases["tolerances"][
            "maximum_precommit_quaternion_norm_residual"]),
        "Decimal torque-free quaternion norm residual exceeds the limit")
    for invariant, drift in stored_coupled[
            "maximum_invariant_drift"].items():
        checks.require(decimal(drift) <= reference_confirmation_limit,
                       f"Decimal torque-free {invariant} drift exceeds the limit")

    failure_id = "CASE-YYZ6-RK-STAGE-DOMAIN-FAILURE"
    checks.require(oracle["cases"][failure_id]["failure"] ==
                   recomputed["cases"][failure_id]["failure"],
                   "stored YYZ core failure reference differs")
    checks.require(oracle["invalid_input_cases"] == cases["invalid_input_cases"],
                   "stored YYZ core invalid-input definitions differ")
    expected_invalid = {entry["id"] for entry in cases["invalid_input_cases"]}
    reference_invalid = independent_invalid_rejections(cases)
    checks.require(reference_invalid == expected_invalid,
                   "Python reference accepted an invalid YYZ core input",
                   len(reference_invalid) + 1)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ YYZ core probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["status"] == "passed" and
                   probe["model_id"] == MODEL_ID and
                   probe["model_choice_status"] == decision_status,
                   "C++ YYZ core probe identity differs", 4)

    formula_absolute = decimal(cases["tolerances"]["formula_absolute"])
    formula_relative = decimal(cases["tolerances"]["formula_relative"])
    probe_formula = probe["formula_intermediates"]
    checks.require(set(probe_formula) == set(stored_formula),
                   "C++ formula intermediate identities differ")
    for field, expected in stored_formula.items():
        actual = probe_formula[field]
        if isinstance(expected, list):
            compare_vector(checks, actual, expected,
                           formula_absolute, formula_relative,
                           f"C++ formula {field}")
        else:
            compare_scalar(checks, actual, expected,
                           formula_absolute, formula_relative,
                           f"C++ formula {field}")

    tolerances = cases["tolerances"]
    translation_id = "CASE-YYZ6-CONSTANT-TRANSLATION"
    expected_translation = oracle["cases"][translation_id]["analytic_trajectory"]
    actual_translation = probe["translation_trajectory"]
    checks.require(len(actual_translation) == len(expected_translation),
                   "C++ translation trajectory length differs")
    for index, (actual, expected) in enumerate(
            zip(actual_translation, expected_translation)):
        checks.require(actual["tick"] == expected["tick"],
                       f"C++ translation tick {index} differs")
        compare_scalar(checks, actual["time_s"], expected["time_s"],
                       decimal(tolerances["time_absolute_s"]), Decimal(0),
                       f"C++ translation time {index}")
        compare_state(
            checks, actual, expected, tolerances,
            decimal(tolerances["translation_orientation_error_max_rad"]),
            f"C++ translation sample {index}")
    actual_terminal = probe["translation_terminal"]
    expected_terminal = oracle["cases"][translation_id]["terminal"]
    checks.require(actual_terminal["kind"] == expected_terminal["kind"] and
                   actual_terminal["tick"] == expected_terminal["tick"],
                   "C++ translation terminal identity differs", 2)
    compare_scalar(checks, actual_terminal["time_s"],
                   expected_terminal["time_s"],
                   decimal(tolerances["time_absolute_s"]), Decimal(0),
                   "C++ translation terminal time")

    expected_spin = oracle["cases"][spin_id]["analytic_trajectory"]
    actual_spin = probe["spin_reference_trajectory"]
    checks.require(len(actual_spin) == len(expected_spin),
                   "C++ spin reference trajectory length differs")
    spin_limit = decimal(
        tolerances["spin_reference_orientation_error_max_rad"])
    for index, (actual, expected) in enumerate(zip(actual_spin, expected_spin)):
        checks.require(actual["tick"] == expected["tick"],
                       f"C++ spin tick {index} differs")
        compare_scalar(checks, actual["time_s"], expected["time_s"],
                       decimal(tolerances["time_absolute_s"]), Decimal(0),
                       f"C++ spin time {index}")
        compare_state(checks, actual, expected, tolerances, spin_limit,
                      f"C++ spin sample {index}")

    convergence = probe["orientation_convergence"]
    ladder = by_id[spin_id]["dt_ladder_s"]
    checks.require(len(convergence) == len(ladder),
                   "C++ orientation convergence ladder length differs")
    previous_error = None
    minimum_order = decimal(
        tolerances["minimum_observed_orientation_order"])
    for index, (entry, expected_dt) in enumerate(zip(convergence, ladder)):
        compare_scalar(checks, entry["dt_s"], expected_dt,
                       decimal(tolerances["time_absolute_s"]), Decimal(0),
                       f"convergence dt {index}")
        compare_state(checks, entry["final_state"], stored_spin_final,
                      tolerances, spin_limit,
                      f"C++ convergence final state {index}")
        independent_error = Decimal(str(orientation_error(
            entry["final_state"]["q_I_B_wxyz"],
            stored_spin_final["q_I_B_wxyz"])))
        compare_scalar(checks, entry["orientation_error_rad"],
                       independent_error, formula_absolute,
                       formula_relative,
                       f"convergence independent error {index}")
        checks.require(independent_error > 0,
                       f"convergence error {index} is not positive")
        checks.require(decimal(entry["max_precommit_quaternion_norm_residual"]) <=
                       decimal(tolerances[
                           "maximum_precommit_quaternion_norm_residual"]),
                       f"convergence norm residual {index} exceeds limit")
        if previous_error is None:
            checks.require(entry["observed_order"] is None,
                           "first convergence order must be null")
        else:
            checks.require(independent_error < previous_error,
                           f"convergence error {index} did not decrease")
            independent_order = Decimal(str(math.log(
                float(previous_error / independent_error), 2.0)))
            compare_scalar(checks, entry["observed_order"],
                           independent_order, Decimal("1e-5"),
                           Decimal("1e-5"),
                           f"convergence independent order {index}")
            checks.require(independent_order >= minimum_order,
                           f"convergence order {index} is below {minimum_order}")
        previous_error = independent_error
    checks.require(previous_error is not None and previous_error <=
                   decimal(tolerances["finest_orientation_error_max_rad"]),
                   "finest orientation error exceeds limit")

    coupled_case = by_id[coupled_id]
    expected_coupled_trajectory = stored_coupled[
        "high_precision_trajectory"]
    actual_coupled_trajectory = probe["coupled_reference_trajectory"]
    checks.require(
        len(actual_coupled_trajectory) == len(expected_coupled_trajectory),
        "C++ torque-free coupled trajectory length differs")
    coupled_orientation_limit = decimal(
        tolerances["coupled_reference_orientation_error_max_rad"])
    coupled_angular_rate_limit = decimal(
        tolerances["coupled_reference_angular_rate_error_max_radps"])
    for index, (actual, expected) in enumerate(zip(
            actual_coupled_trajectory, expected_coupled_trajectory)):
        checks.require(actual["tick"] == expected["tick"],
                       f"C++ torque-free coupled tick {index} differs")
        compare_scalar(checks, actual["time_s"], expected["time_s"],
                       decimal(tolerances["time_absolute_s"]), Decimal(0),
                       f"C++ torque-free coupled time {index}")
        compare_state(
            checks, actual, expected, tolerances,
            coupled_orientation_limit,
            f"C++ torque-free coupled sample {index}",
            coupled_angular_rate_limit, Decimal(0))

    coupled_convergence = probe["coupled_convergence"]
    coupled_ladder = coupled_case["dt_ladder_s"]
    checks.require(len(coupled_convergence) == len(coupled_ladder),
                   "C++ torque-free convergence ladder length differs")
    coupled_final = stored_coupled["high_precision_final"]
    initial_invariants = stored_coupled["initial_invariants"]
    previous_orientation_error = None
    previous_angular_rate_error = None
    previous_energy_drift = None
    previous_momentum_drift = None
    for index, (entry, expected_dt) in enumerate(zip(
            coupled_convergence, coupled_ladder)):
        compare_scalar(checks, entry["dt_s"], expected_dt,
                       decimal(tolerances["time_absolute_s"]), Decimal(0),
                       f"torque-free convergence dt {index}")
        compare_state(
            checks, entry["final_state"], coupled_final, tolerances,
            coupled_orientation_limit,
            f"C++ torque-free convergence final state {index}",
            coupled_angular_rate_limit, Decimal(0))
        orientation = Decimal(str(orientation_error(
            entry["final_state"]["q_I_B_wxyz"],
            coupled_final["q_I_B_wxyz"])))
        angular_rate = vector_l2_difference(
            [decimal(value) for value in
             entry["final_state"]["omega_BI_B_radps"]],
            [decimal(value) for value in
             coupled_final["omega_BI_B_radps"]])
        checks.require(orientation > 0,
                       f"torque-free orientation error {index} is not positive")
        checks.require(angular_rate > 0,
                       f"torque-free angular-rate error {index} is not positive")

        computed_invariants = rotational_invariants(
            entry["final_state"], coupled_case["inputs"])
        compare_scalar(
            checks, entry["rotational_kinetic_energy_J"],
            computed_invariants["rotational_kinetic_energy_J"],
            formula_absolute, formula_relative,
            f"torque-free energy {index}")
        compare_scalar(
            checks, entry["angular_momentum_norm_kgm2ps"],
            computed_invariants["angular_momentum_norm_kgm2ps"],
            formula_absolute, formula_relative,
            f"torque-free angular-momentum norm {index}")
        energy_drift = abs(
            computed_invariants["rotational_kinetic_energy_J"] -
            decimal(initial_invariants["rotational_kinetic_energy_J"]))
        momentum_drift = abs(
            computed_invariants["angular_momentum_norm_kgm2ps"] -
            decimal(initial_invariants[
                "angular_momentum_norm_kgm2ps"]))
        compare_scalar(checks, entry["energy_absolute_drift_J"],
                       energy_drift, formula_absolute, formula_relative,
                       f"torque-free energy drift {index}")
        compare_scalar(
            checks,
            entry[
                "angular_momentum_norm_absolute_drift_kgm2ps"],
            momentum_drift, formula_absolute, formula_relative,
            f"torque-free angular-momentum drift {index}")
        checks.require(
            decimal(entry[
                "max_precommit_quaternion_norm_residual"]) <=
            decimal(tolerances[
                "maximum_precommit_quaternion_norm_residual"]),
            f"torque-free quaternion norm residual {index} exceeds limit")

        if previous_orientation_error is not None:
            checks.require(orientation < previous_orientation_error,
                           f"torque-free orientation error {index} did not decrease")
            checks.require(angular_rate < previous_angular_rate_error,
                           f"torque-free angular-rate error {index} did not decrease")
            orientation_order = Decimal(str(math.log(
                float(previous_orientation_error / orientation), 2.0)))
            angular_rate_order = Decimal(str(math.log(
                float(previous_angular_rate_error / angular_rate), 2.0)))
            checks.require(
                orientation_order >= decimal(tolerances[
                    "minimum_observed_orientation_order"]),
                f"torque-free orientation order {index} is below the limit")
            checks.require(
                angular_rate_order >= decimal(tolerances[
                    "minimum_observed_angular_rate_order"]),
                f"torque-free angular-rate order {index} is below the limit")
            checks.require(energy_drift < previous_energy_drift,
                           f"torque-free energy drift {index} did not decrease")
            checks.require(momentum_drift < previous_momentum_drift,
                           f"torque-free momentum drift {index} did not decrease")
        previous_orientation_error = orientation
        previous_angular_rate_error = angular_rate
        previous_energy_drift = energy_drift
        previous_momentum_drift = momentum_drift

    checks.require(
        previous_orientation_error is not None and
        previous_orientation_error <= decimal(
            tolerances["coupled_finest_orientation_error_max_rad"]),
        "torque-free finest orientation error exceeds the limit")
    checks.require(
        previous_angular_rate_error is not None and
        previous_angular_rate_error <= decimal(
            tolerances["coupled_finest_angular_rate_error_max_radps"]),
        "torque-free finest angular-rate error exceeds the limit")
    checks.require(
        previous_energy_drift is not None and
        previous_energy_drift <= decimal(
            tolerances["coupled_finest_energy_drift_max_J"]),
        "torque-free finest energy drift exceeds the limit")
    checks.require(
        previous_momentum_drift is not None and
        previous_momentum_drift <= decimal(tolerances[
            "coupled_finest_angular_momentum_norm_drift_max_kgm2ps"]),
        "torque-free finest angular-momentum drift exceeds the limit")
    checks.require(
        decimal(coupled_convergence[0]["energy_absolute_drift_J"]) <=
        decimal(tolerances["coupled_coarsest_energy_drift_max_J"]),
        "torque-free coarsest energy drift exceeds the limit")
    checks.require(
        decimal(coupled_convergence[0][
            "angular_momentum_norm_absolute_drift_kgm2ps"]) <=
        decimal(tolerances[
            "coupled_coarsest_angular_momentum_norm_drift_max_kgm2ps"]),
        "torque-free coarsest angular-momentum drift exceeds the limit")

    expected_failure = oracle["cases"][failure_id]["failure"]
    actual_failure = probe["stage_failure"]
    for field in ("code", "stage", "failed_step_start_tick",
                  "candidate_disposition", "last_committed_tick"):
        checks.require(actual_failure[field] == expected_failure[field],
                       f"C++ stage failure {field} differs")
    compare_scalar(checks, actual_failure["evaluation_time_s"],
                   expected_failure["evaluation_time_s"],
                   decimal(tolerances["time_absolute_s"]), Decimal(0),
                   "C++ stage failure evaluation time")
    compare_state(
        checks, actual_failure["last_committed_state"],
        expected_failure["last_committed_state"], tolerances,
        decimal(tolerances["translation_orientation_error_max_rad"]),
        "C++ last committed failure state")

    actual_invalid = set(probe["invalid_input_rejections"])
    checks.require(actual_invalid == expected_invalid,
                   "C++ invalid-input rejection identities differ")

    return {
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "model_choice_status": decision_status,
        "formula_case": formula_id,
        "translation_terminal_tick":
            probe["translation_terminal"]["tick"],
        "orientation_convergence_levels": len(convergence),
        "finest_orientation_error_rad":
            str(convergence[-1]["orientation_error_rad"]),
        "coupled_convergence_levels": len(coupled_convergence),
        "coupled_finest_orientation_error_rad":
            str(previous_orientation_error),
        "coupled_finest_angular_rate_error_radps":
            str(previous_angular_rate_error),
        "stage_failure_rejected": True,
        "invalid_input_cases_rejected": len(actual_invalid),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", required=True, type=Path)
    parser.add_argument("--oracle", type=Path)
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--generate-reference", action="store_true")
    arguments = parser.parse_args()

    getcontext().prec = 60
    raw_cases = arguments.cases.read_bytes()
    cases = json.loads(raw_cases.decode("utf-8"), parse_float=Decimal)
    require(cases["schema_version"] == "gnczmkn.yyz-6dof-core-cases/1" and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model"]["model_id"] == MODEL_ID,
            "YYZ core cases identity differs")

    if arguments.generate_reference:
        require(arguments.oracle is None and arguments.probe is None,
                "reference generation does not accept --oracle or --probe")
        print(json.dumps(build_reference(cases, raw_cases), indent=2,
                         ensure_ascii=False))
        return 0

    require(arguments.oracle is not None and arguments.probe is not None,
            "verification requires --oracle and --probe")
    oracle = json.loads(
        arguments.oracle.read_text(encoding="utf-8"),
        parse_float=Decimal)
    result = verify_reference(cases, raw_cases, oracle, arguments.probe)
    print(json.dumps(result, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (IndexError, KeyError, OSError, ValueError,
            json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"YYZ 6DoF core reference failed: {error}", file=sys.stderr)
        raise SystemExit(1)

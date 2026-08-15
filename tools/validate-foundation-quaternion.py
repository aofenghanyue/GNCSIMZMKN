#!/usr/bin/env python3
"""Independent Decimal reference for the passive Hamilton product path."""

from __future__ import annotations

import argparse
from decimal import Decimal, getcontext
import json
import math
from pathlib import Path
import subprocess
import sys


CONVENTION_ID = "SCI-CONVENTIONS-001"
YYZ_FIXTURE_ID = "REF-YYZ-6DOF-CORE-001"
YYZ_ORACLE_ID = "ORACLE-YYZ-6DOF-CORE-001"
PROBE_SCHEMA = "gnczmkn.foundation-passive-quaternion-probe/1"
COMPONENT_ID = "GNC-FOUNDATION-PASSIVE-QUATERNION-001"
NORMALIZED_FLAG = 1 << 0


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite Decimal value: {value}")
    return result


def vector(values: object, size: int, label: str) -> list[Decimal]:
    require(isinstance(values, list) and len(values) == size,
            f"{label} must have {size} entries")
    return [decimal(value) for value in values]


def quaternion(values: object, label: str) -> list[Decimal]:
    return vector(values, 4, label)


def q_norm(value: list[Decimal]) -> Decimal:
    return sum((entry * entry for entry in value), Decimal(0)).sqrt()


def q_normalize(value: list[Decimal]) -> list[Decimal]:
    norm = q_norm(value)
    require(norm > 0, "independent quaternion has zero norm")
    return [entry / norm for entry in value]


def q_prepare(value: list[Decimal], absolute: Decimal,
              relative: Decimal) -> list[Decimal]:
    norm = q_norm(value)
    require(norm > 0, "independent quaternion has zero norm")
    limit = absolute + relative * max(Decimal(1), norm)
    if abs(norm - Decimal(1)) <= limit:
        return value[:]
    return [entry / norm for entry in value]


def q_conjugate(value: list[Decimal]) -> list[Decimal]:
    return [value[0], -value[1], -value[2], -value[3]]


def q_inverse(value: list[Decimal]) -> list[Decimal]:
    norm_squared = sum((entry * entry for entry in value), Decimal(0))
    require(norm_squared > 0, "independent quaternion has zero norm")
    return [entry / norm_squared for entry in q_conjugate(value)]


def hamilton(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    lw, lx, ly, lz = lhs
    rw, rx, ry, rz = rhs
    return [
        lw * rw - lx * rx - ly * ry - lz * rz,
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
    ]


def passive_rotate(q_to_from: list[Decimal],
                   vector_from: list[Decimal]) -> list[Decimal]:
    pure = [Decimal(0), *vector_from]
    result = hamilton(hamilton(q_inverse(q_to_from), pure), q_to_from)
    require(abs(result[0]) <= Decimal("1e-60"),
            "independent passive rotation has a scalar residue")
    return result[1:]


def passive_matrix(value: list[Decimal]) -> list[Decimal]:
    w, x, y, z = value
    norm_squared = sum((entry * entry for entry in value), Decimal(0))
    return [
        (w*w + x*x - y*y - z*z) / norm_squared,
        Decimal(2) * (x*y + w*z) / norm_squared,
        Decimal(2) * (x*z - w*y) / norm_squared,
        Decimal(2) * (x*y - w*z) / norm_squared,
        (w*w - x*x + y*y - z*z) / norm_squared,
        Decimal(2) * (y*z + w*x) / norm_squared,
        Decimal(2) * (x*z + w*y) / norm_squared,
        Decimal(2) * (y*z - w*x) / norm_squared,
        (w*w - x*x - y*y + z*z) / norm_squared,
    ]


def q_body_derivative(value: list[Decimal],
                      omega_body: list[Decimal]) -> list[Decimal]:
    return [Decimal("-0.5") * entry for entry in hamilton(
        [Decimal(0), *omega_body], value)]


def q_inertial_derivative(value: list[Decimal],
                          omega_inertial: list[Decimal]) -> list[Decimal]:
    return [Decimal("-0.5") * entry for entry in hamilton(
        value, [Decimal(0), *omega_inertial])]


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
    return max((close(actual_value, expected_value, absolute, relative,
                      f"{label}[{index}]")
                for index, (actual_value, expected_value) in enumerate(
                    zip(actual, expected))), default=Decimal(0))


def independent_observation(observation: dict) -> list[Decimal] | None:
    operation = observation["operation"]
    inputs = observation["input"]
    if operation == "passive_rotate":
        return passive_rotate(
            quaternion(inputs["quaternion_wxyz"], "rotation quaternion"),
            vector(inputs["vector"], 3, "rotation vector"))
    if operation == "composition_rotate":
        q_b_a = quaternion(inputs["q_b_a_wxyz"], "q_b_a")
        q_c_b = quaternion(inputs["q_c_b_wxyz"], "q_c_b")
        q_c_a = hamilton(q_b_a, q_c_b)
        return passive_rotate(
            q_c_a, vector(inputs["vector_a"], 3, "composition vector"))
    if operation == "hamilton_product":
        return hamilton(
            quaternion(inputs["lhs_wxyz"], "Hamilton lhs"),
            quaternion(inputs["rhs_wxyz"], "Hamilton rhs"))
    if operation == "inverse_round_trip":
        value = quaternion(inputs["quaternion_wxyz"],
                           "round-trip quaternion")
        original = vector(inputs["vector"], 3, "round-trip vector")
        return passive_rotate(q_inverse(value),
                              passive_rotate(value, original))
    if operation == "passive_matrix_row_major":
        return passive_matrix(quaternion(
            inputs["quaternion_wxyz"], "matrix quaternion"))
    if operation == "serialize_wxyz":
        return quaternion(inputs["quaternion_wxyz"],
                          "serialization quaternion")
    if operation == "body_rate_derivative":
        return q_body_derivative(
            quaternion(inputs["q_i_b_wxyz"], "body derivative q"),
            vector(inputs["omega_bi_b_radps"], 3,
                   "body derivative omega"))
    if operation == "inertial_rate_derivative":
        return q_inertial_derivative(
            quaternion(inputs["q_i_b_wxyz"], "inertial derivative q"),
            vector(inputs["omega_bi_i_radps"], 3,
                   "inertial derivative omega"))
    if operation == "euler_intrinsic_zyx_round_trip":
        return None
    return None


def run_probe(path: Path) -> tuple[bytes, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"], check=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    return completed.stdout, json.loads(
        completed.stdout.decode("utf-8"), parse_float=Decimal)


def q_add_scaled(base: list[Decimal], increment: list[Decimal],
                 scale: Decimal) -> list[Decimal]:
    return [left + scale * right for left, right in zip(base, increment)]


def rk4_quaternion_step(committed: list[Decimal], dt: Decimal,
                        omega: list[Decimal], absolute: Decimal,
                        relative: Decimal) -> tuple[list[Decimal], Decimal]:
    def derivative(stage: list[Decimal]) -> list[Decimal]:
        return q_body_derivative(q_prepare(stage, absolute, relative), omega)

    half = dt / Decimal(2)
    k1 = derivative(committed)
    k2 = derivative(q_add_scaled(committed, k1, half))
    k3 = derivative(q_add_scaled(committed, k2, half))
    k4 = derivative(q_add_scaled(committed, k3, dt))
    candidate = [
        value + dt * (first + Decimal(2) * second +
                      Decimal(2) * third + fourth) / Decimal(6)
        for value, first, second, third, fourth in zip(
            committed, k1, k2, k3, k4)
    ]
    residual = abs(q_norm(candidate) - Decimal(1))
    return q_prepare(candidate, absolute, relative), residual


def sign_aligned_chord(actual: list[Decimal],
                       expected: list[Decimal]) -> Decimal:
    left = q_normalize(actual)
    right = q_normalize(expected)
    dot = sum((lhs * rhs for lhs, rhs in zip(left, right)), Decimal(0))
    if dot < 0:
        right = [-entry for entry in right]
    return sum(((lhs - rhs) ** 2
                for lhs, rhs in zip(left, right)), Decimal(0)).sqrt()


def orientation_error_from_chord(chord: Decimal) -> Decimal:
    return Decimal(str(4.0 * math.asin(min(1.0, float(chord / 2)))))


def verify(convention_cases: dict, convention_profile: dict,
           yyz_cases: dict, yyz_oracle: dict,
           probe_path: Path) -> dict:
    require(convention_cases["convention_id"] ==
            convention_profile["convention_id"] == CONVENTION_ID,
            "scientific convention identity differs")
    quaternion_profile = convention_profile["quaternion"]
    require(quaternion_profile["semantic"] ==
            "passive_coordinate_transform" and
            quaternion_profile["algebra"] == "Hamilton" and
            quaternion_profile["storage_order"] == ["w", "x", "y", "z"] and
            quaternion_profile["quaternion_composition"] ==
            "q_c_a = q_b_a * q_c_b" and
            quaternion_profile["euler_verification_profile"]
            ["runtime_default"] is False,
            "accepted quaternion convention differs")
    require(yyz_cases["fixture_id"] == yyz_oracle["fixture_id"] ==
            YYZ_FIXTURE_ID and
            yyz_cases["oracle_id"] == yyz_oracle["oracle_id"] ==
            YYZ_ORACLE_ID,
            "YYZ rigid-core identity differs")
    require(yyz_oracle["reference_method"]["precision_digits"] >= 60,
            "YYZ reference precision is below 60 digits")

    absolute = decimal(convention_cases["tolerance"]["absolute"])
    relative = decimal(convention_cases["tolerance"]["relative"])
    independent: dict[str, list[Decimal]] = {}
    for observation in convention_cases["observations"]:
        expected = independent_observation(observation)
        if expected is None:
            continue
        identifier = observation["id"]
        close_vector(observation["expected"], expected, absolute, relative,
                     f"fixture.{identifier}")
        independent[identifier] = expected
    require(len(independent) == 9,
            "quaternion fixed observation coverage differs")

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    require(first_stdout == second_stdout and probe == second_probe,
            "quaternion probe reruns differ")
    require(probe["schema_version"] == PROBE_SCHEMA and
            probe["component_id"] == COMPONENT_ID and
            probe["fixture_ids"] ==
            ["REF-SCIENTIFIC-CONVENTIONS-001", YYZ_FIXTURE_ID],
            "quaternion probe identity differs")
    require(probe["convention"] == {
        "semantic": "passive",
        "algebra": "Hamilton",
        "coefficient_order": "wxyz",
        "composition": "q_c_a=q_b_a*q_c_b",
    }, "product quaternion convention differs")
    require(probe["storage"] == {
        "type": "Eigen::Quaterniond",
        "eigen_version": "3.4.0",
        "scalar": "binary64",
        "wire_order": "wxyz",
        "eigen_coeffs_order": "xyzw",
    }, "canonical QuaternionStorage differs")
    require(probe["algorithms"] == {
        "prepare":
            "gnc.foundation.quaternion.prepare-passive-hamilton@1",
        "product": "gnc.foundation.quaternion.hamilton-product@1",
        "rotate": "gnc.foundation.quaternion.passive-rotate@1",
        "compose": "gnc.foundation.quaternion.passive-compose@1",
        "body_rate":
            "gnc.foundation.quaternion.body-rate-derivative@1",
    }, "quaternion algorithm identities differ")
    require(decimal(probe["policy"]["absolute_tolerance"]) ==
            Decimal("2e-12") and
            decimal(probe["policy"]["relative_tolerance"]) ==
            Decimal("2e-12") and
            decimal(probe["policy"]["zero_tolerance"]) ==
            Decimal("1e-14") and
            probe["policy"]["normalization"] == "Error",
            "quaternion product policy differs")

    product_fixed = {entry["id"]: entry["result"]
                     for entry in probe["fixed_cases"]}
    require(set(product_fixed) == set(independent),
            "quaternion fixed case identities differ")
    maximum_fixed_error = Decimal(0)
    for identifier, expected in independent.items():
        maximum_fixed_error = max(
            maximum_fixed_error,
            close_vector(product_fixed[identifier], expected,
                         Decimal("2e-12"), Decimal("2e-12"),
                         f"product.{identifier}"))

    properties = probe["properties"]
    property_limits = {
        "max_matrix_vector_error": Decimal("8e-15"),
        "max_orthogonality_error": Decimal("2e-15"),
        "max_determinant_error": Decimal("3e-15"),
        "max_sign_equivalence_error": Decimal("1e-15"),
        "max_composition_error": Decimal("1e-14"),
        "max_inverse_round_trip_error": Decimal("8e-15"),
        "max_derivative_equivalence_error": Decimal("4e-15"),
        "max_orientation_sign_error_rad": Decimal("1e-15"),
    }
    require(properties["samples"] == 256,
            "quaternion property sample count differs")
    for field, limit in property_limits.items():
        require(decimal(properties[field]) <= limit,
                f"quaternion property exceeds limit: {field}")

    yyz_case_map = {case["id"]: case for case in yyz_cases["cases"]}
    coupled = yyz_case_map["CASE-YYZ6-COUPLED-DERIVATIVE"]
    coupled_q = quaternion(coupled["state"]["q_I_B_wxyz"],
                           "YYZ coupled q")
    expected_force = passive_rotate(
        coupled_q,
        vector(coupled["inputs"]["force_B_N"], 3, "YYZ coupled force"))
    expected_q_dot = q_body_derivative(
        coupled_q,
        vector(coupled["state"]["omega_BI_B_radps"], 3,
               "YYZ coupled omega"))
    stored_coupled = yyz_oracle["cases"][
        "CASE-YYZ6-COUPLED-DERIVATIVE"]["formula_intermediates"]
    stored_tolerance = decimal(
        yyz_cases["tolerances"]["stored_decimal_absolute"])
    close_vector(stored_coupled["force_I_N"], expected_force,
                 stored_tolerance, Decimal(0), "stored YYZ force")
    close_vector(stored_coupled["q_derivative_I_B_per_s"], expected_q_dot,
                 stored_tolerance, Decimal(0), "stored YYZ q derivative")
    product_coupled = probe["yyz"]["coupled_derivative"]
    maximum_yyz_formula_error = max(
        close_vector(product_coupled["force_I_N"], expected_force,
                     Decimal("2e-12"), Decimal("2e-12"),
                     "product YYZ force"),
        close_vector(product_coupled["q_derivative_I_B_per_s"],
                     expected_q_dot, Decimal("2e-12"), Decimal("2e-12"),
                     "product YYZ q derivative"))

    spin_case = yyz_case_map["CASE-YYZ6-PRINCIPAL-SPIN-CONVERGENCE"]
    initial = quaternion(spin_case["initial_state"]["q_I_B_wxyz"],
                         "YYZ spin initial q")
    omega = vector(spin_case["initial_state"]["omega_BI_B_radps"], 3,
                   "YYZ spin omega")
    duration = decimal(spin_case["duration_s"])
    analytic = quaternion(
        yyz_oracle["cases"]["CASE-YYZ6-PRINCIPAL-SPIN-CONVERGENCE"]
        ["analytic_final"]["q_I_B_wxyz"], "YYZ analytic final q")
    product_levels = probe["yyz"]["principal_spin_convergence"]
    dt_ladder = [decimal(value) for value in spin_case["dt_ladder_s"]]
    require(len(product_levels) == len(dt_ladder),
            "principal-spin ladder length differs")
    maximum_spin_state_error = Decimal(0)
    maximum_norm_residual = Decimal(0)
    minimum_observed_order: Decimal | None = None
    previous_error: Decimal | None = None
    for index, (level, dt) in enumerate(zip(product_levels, dt_ladder)):
        close(level["dt_s"], dt, Decimal("1e-17"), Decimal("1e-16"),
              f"principal-spin dt[{index}]")
        state = initial[:]
        independent_max_residual = Decimal(0)
        steps = int(duration / dt)
        require(Decimal(steps) * dt == duration,
                "principal-spin duration is not on the dt grid")
        for _ in range(steps):
            state, residual = rk4_quaternion_step(
                state, dt, omega, Decimal("2e-12"), Decimal("2e-12"))
            independent_max_residual = max(independent_max_residual,
                                           residual)
        maximum_spin_state_error = max(
            maximum_spin_state_error,
            close_vector(level["final_q_I_B_wxyz"], state,
                         Decimal("3e-11"), Decimal("3e-11"),
                         f"principal-spin final q[{index}]"))
        close(level["max_precommit_norm_residual"],
              independent_max_residual, Decimal("5e-12"),
              Decimal("5e-8"), f"principal-spin norm residual[{index}]")
        maximum_norm_residual = max(
            maximum_norm_residual,
            decimal(level["max_precommit_norm_residual"]))
        chord = sign_aligned_chord(state, analytic)
        independent_error = orientation_error_from_chord(chord)
        close(level["orientation_error_rad"], independent_error,
              Decimal("3e-10"), Decimal("3e-8"),
              f"principal-spin orientation error[{index}]")
        if previous_error is None:
            require(level["observed_order"] is None,
                    "coarsest principal-spin order must be null")
        else:
            independent_order = Decimal(str(math.log(
                float(previous_error / independent_error), 2.0)))
            close(level["observed_order"], independent_order,
                  Decimal("2e-3"), Decimal("2e-3"),
                  f"principal-spin observed order[{index}]")
            reported_order = decimal(level["observed_order"])
            minimum_observed_order = (
                reported_order if minimum_observed_order is None else
                min(minimum_observed_order, reported_order))
        previous_error = independent_error

    minimum_limit = decimal(
        yyz_cases["tolerances"]["minimum_observed_orientation_order"])
    finest_limit = decimal(
        yyz_cases["tolerances"]["finest_orientation_error_max_rad"])
    norm_limit = decimal(
        yyz_cases["tolerances"]
        ["maximum_precommit_quaternion_norm_residual"])
    require(minimum_observed_order is not None and
            minimum_observed_order >= minimum_limit,
            "principal-spin product order is below fixture limit")
    finest_error = decimal(product_levels[-1]["orientation_error_rad"])
    require(finest_error <= finest_limit and
            maximum_norm_residual <= norm_limit,
            "principal-spin product limits differ")

    normalization = probe["normalization"]
    require(normalization == {
        "status": "Approximate",
        "flags": NORMALIZED_FLAG,
        "detail": "normalized-input",
        "quaternion_wxyz": [Decimal(1), Decimal(0),
                             Decimal(0), Decimal(0)],
    }, "NormalizeWithFlag product result differs")
    expected_failures = {
        "INVALID-POLICY": ("DomainError", "policy"),
        "ZERO-NORM": ("DomainError", "zero-norm-quaternion"),
        "NONUNIT-ERROR": ("DomainError", "non-unit-quaternion"),
        "NONFINITE-QUATERNION": ("NonFiniteInput", "quaternion"),
        "NONFINITE-VECTOR": ("NonFiniteInput", "vector"),
        "NONFINITE-ANGULAR-RATE": ("NonFiniteInput", "angular-rate"),
        "OVERFLOW-PRODUCT": ("NonFiniteOutput", "product"),
    }
    failures = {entry["id"]: entry for entry in probe["failure_cases"]}
    require(set(failures) == set(expected_failures),
            "quaternion failure identities differ")
    for identifier, (status, detail) in expected_failures.items():
        require(failures[identifier] == {
            "id": identifier,
            "status": status,
            "detail": detail,
            "has_value": False,
        }, f"quaternion failure differs for {identifier}")

    return {
        "component_id": COMPONENT_ID,
        "status": "passed",
        "fixed_cases": len(product_fixed),
        "property_samples": properties["samples"],
        "failure_cases": len(failures),
        "max_fixed_case_error": str(maximum_fixed_error),
        "max_yyz_formula_error": str(maximum_yyz_formula_error),
        "max_spin_decimal_state_error": str(maximum_spin_state_error),
        "minimum_observed_orientation_order":
            str(minimum_observed_order),
        "finest_orientation_error_rad": str(finest_error),
        "max_precommit_quaternion_norm_residual":
            str(maximum_norm_residual),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--convention-cases", required=True, type=Path)
    parser.add_argument("--convention-profile", required=True, type=Path)
    parser.add_argument("--yyz-cases", required=True, type=Path)
    parser.add_argument("--yyz-oracle", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    arguments = parser.parse_args()

    getcontext().prec = 80
    load = lambda path: json.loads(path.read_text(encoding="utf-8"),
                                   parse_float=Decimal)
    print(json.dumps(verify(
        load(arguments.convention_cases),
        load(arguments.convention_profile),
        load(arguments.yyz_cases),
        load(arguments.yyz_oracle),
        arguments.probe), separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ArithmeticError, IndexError, KeyError, OSError, TypeError,
            ValueError, json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"foundation quaternion validation failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

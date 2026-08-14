#!/usr/bin/env python3
"""Independent Decimal reference for ORACLE-YYZ-FORCE-MOMENT-CLOSURE-001."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from decimal import Decimal, getcontext
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-FORCE-MOMENT-CLOSURE-001"
ORACLE_ID = "ORACLE-YYZ-FORCE-MOMENT-CLOSURE-001"
MODEL_ID = "MODEL-YYZ-FORCE-MOMENT-CLOSURE-001"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite decimal value: {value}")
    return result


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


def normalize_quaternion(values: list[Decimal], label: str) -> list[Decimal]:
    norm_squared = dot(values, values)
    require(norm_squared > 0, f"{label} has zero norm")
    norm = norm_squared.sqrt()
    return [value / norm for value in values]


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


def cases_by_id(cases: dict) -> dict[str, dict]:
    result = {case["id"]: case for case in cases["cases"]}
    require(len(result) == len(cases["cases"]),
            "duplicate YYZ closure case id")
    return result


def validate_context(context: dict) -> None:
    require(isinstance(context["body_frame_id"], str) and
            bool(context["body_frame_id"]),
            "closure body_frame_id must be nonempty")
    require(isinstance(context["configuration_revision"], int) and
            context["configuration_revision"] >= 0,
            "configuration_revision must be a nonnegative integer")
    require(isinstance(context["valid_from_tick"], int) and
            isinstance(context["valid_until_tick"], int) and
            context["valid_from_tick"] >= 0 and
            context["valid_until_tick"] > context["valid_from_tick"],
            "closure validity must be a forward half-open tick interval")


def closure_reference(case: dict,
                      reverse_application_vector: bool = False) -> dict:
    context = case["context"]
    validate_context(context)
    source_ids: set[str] = set()
    canonical_contributions = []
    for contribution in case["contributions"]:
        source_id = contribution["source_id"]
        require(isinstance(source_id, str) and bool(source_id),
                "closure source_id must be nonempty")
        require(source_id not in source_ids,
                f"duplicate closure source_id: {source_id}")
        source_ids.add(source_id)
        require(contribution["body_frame_id"] == context["body_frame_id"],
                f"closure frame mismatch for {source_id}")
        require(contribution["configuration_revision"] ==
                context["configuration_revision"],
                f"closure configuration revision mismatch for {source_id}")
        require(contribution["valid_from_tick"] ==
                context["valid_from_tick"] and
                contribution["valid_until_tick"] ==
                context["valid_until_tick"],
                f"closure validity interval mismatch for {source_id}")
        force = vector(contribution["force_B_N"], f"{source_id}.force_B")
        application = vector(
            contribution["r_CoM_to_application_B_m"],
            f"{source_id}.r_CoM_to_application_B")
        moment_at_application = vector(
            contribution["moment_at_application_B_Nm"],
            f"{source_id}.moment_at_application_B")
        transported_application = (
            scale(application, Decimal(-1))
            if reverse_application_vector else application)
        lever_arm_moment = cross(transported_application, force)
        moment_about_com = add(moment_at_application, lever_arm_moment)
        canonical_contributions.append({
            "source_id": source_id,
            "force_B_N": force,
            "r_CoM_to_application_B_m": application,
            "moment_at_application_B_Nm": moment_at_application,
            "lever_arm_moment_B_Nm": lever_arm_moment,
            "moment_about_CoM_B_Nm": moment_about_com,
        })

    canonical_contributions.sort(key=lambda entry: entry["source_id"])
    total_force = [Decimal(0), Decimal(0), Decimal(0)]
    total_moment = [Decimal(0), Decimal(0), Decimal(0)]
    for contribution in canonical_contributions:
        total_force = add(total_force, contribution["force_B_N"])
        total_moment = add(
            total_moment, contribution["moment_about_CoM_B_Nm"])
    return {
        "strategy": "FrozenInterval",
        "context": copy.deepcopy(context),
        "contributions": canonical_contributions,
        "total_force_B_N": total_force,
        "total_moment_about_CoM_B_Nm": total_moment,
    }


def exact_grid_steps(duration: object, dt: object) -> int:
    duration_value = decimal(duration)
    dt_value = decimal(dt)
    require(duration_value >= 0 and dt_value > 0,
            "duration and dt must define a nonnegative positive-step grid")
    steps = duration_value / dt_value
    integral = steps.to_integral_value()
    require(steps == integral, "duration must align to ExactGrid")
    return int(integral)


def analytic_trajectory(case: dict,
                        gravity_double_count: bool = False) -> dict:
    closure = closure_reference(case)
    rigid = case["rigid_core"]
    mass = decimal(rigid["mass_kg"])
    require(mass > 0, "rigid-core mass must be strictly positive")
    gravity = vector(rigid["gravity_I_mps2"], "gravity_I")
    force = closure["total_force_B_N"]
    moment = closure["total_moment_about_CoM_B_Nm"]
    require(moment == [Decimal(0), Decimal(0), Decimal(0)],
            "analytic closure trajectory requires zero total moment")
    initial = rigid["initial_state"]
    q_i_b = normalize_quaternion(
        quaternion(initial["q_I_B_wxyz"], "q_I_B"), "q_I_B")
    omega = vector(initial["omega_BI_B_radps"], "omega_BI_B")
    require(q_i_b == [Decimal(1), Decimal(0), Decimal(0), Decimal(0)] and
            omega == [Decimal(0), Decimal(0), Decimal(0)],
            "analytic closure trajectory requires identity attitude and zero rate")
    body_force_acceleration = scale(force, Decimal(1) / mass)
    acceleration = add(body_force_acceleration, gravity)
    if gravity_double_count:
        acceleration = add(acceleration, gravity)
    position_initial = vector(initial["position_I_m"], "position_I")
    velocity_initial = vector(initial["velocity_I_mps"], "velocity_I")
    dt_s = decimal(rigid["dt_s"])
    steps = exact_grid_steps(rigid["duration_s"], dt_s)
    samples = []
    for tick in range(steps + 1):
        time_s = Decimal(tick) * dt_s
        position = add(
            add(position_initial, scale(velocity_initial, time_s)),
            scale(acceleration, Decimal("0.5") * time_s * time_s))
        velocity = add(velocity_initial, scale(acceleration, time_s))
        samples.append({
            "tick": tick,
            "time_s": time_s,
            "position_I_m": position,
            "velocity_I_mps": velocity,
            "q_I_B_wxyz": q_i_b,
            "omega_BI_B_radps": omega,
        })
    return {
        "strategy": "FrozenInterval",
        "held_closure": closure,
        "gravity_I_mps2": gravity,
        "body_force_acceleration_I_mps2": body_force_acceleration,
        "total_acceleration_I_mps2": acceleration,
        "trajectory": samples,
        "terminal": {
            "kind": rigid["terminal"]["kind"],
            "tick": steps,
            "time_s": decimal(rigid["duration_s"]),
        },
    }


def call_rejected(function) -> bool:
    try:
        function()
    except ValueError:
        return True
    return False


def independent_invalid_rejections(cases: dict) -> set[str]:
    formula_case = cases_by_id(cases)["CASE-YYZ-CLOSURE-3D-WRENCH"]
    rejected: set[str] = set()

    candidate = copy.deepcopy(formula_case)
    candidate["contributions"][1]["source_id"] = \
        candidate["contributions"][0]["source_id"]
    if call_rejected(lambda: closure_reference(candidate)):
        rejected.add("INVALID-YYZ-CLOSURE-DUPLICATE-SOURCE")

    candidate = copy.deepcopy(formula_case)
    candidate["contributions"][0]["body_frame_id"] = "frame.other@1"
    if call_rejected(lambda: closure_reference(candidate)):
        rejected.add("INVALID-YYZ-CLOSURE-FRAME-MISMATCH")

    candidate = copy.deepcopy(formula_case)
    candidate["contributions"][0]["configuration_revision"] += 1
    if call_rejected(lambda: closure_reference(candidate)):
        rejected.add("INVALID-YYZ-CLOSURE-REVISION-MISMATCH")

    candidate = copy.deepcopy(formula_case)
    candidate["contributions"][0]["valid_until_tick"] += 1
    if call_rejected(lambda: closure_reference(candidate)):
        rejected.add("INVALID-YYZ-CLOSURE-INTERVAL-MISMATCH")

    candidate = copy.deepcopy(formula_case)
    candidate["contributions"][0]["force_B_N"][0] = Decimal("Infinity")
    if call_rejected(lambda: closure_reference(candidate)):
        rejected.add("INVALID-YYZ-CLOSURE-NONFINITE-FORCE")
    return rejected


def independent_mutation_rejections(cases: dict) -> set[str]:
    by_id = cases_by_id(cases)
    formula_case = by_id["CASE-YYZ-CLOSURE-3D-WRENCH"]
    trajectory_case = by_id["CASE-YYZ-CLOSURE-RIGID-CORE-TRAJECTORY"]
    expected_formula = closure_reference(formula_case)
    reversed_application = closure_reference(
        formula_case, reverse_application_vector=True)
    pretransported_case = copy.deepcopy(formula_case)
    propulsion = next(
        contribution for contribution in pretransported_case["contributions"]
        if contribution["source_id"] == "propulsion.main")
    propulsion["moment_at_application_B_Nm"] = add(
        vector(propulsion["moment_at_application_B_Nm"],
               "propulsion.main.moment_at_application_B"),
        cross(vector(propulsion["r_CoM_to_application_B_m"],
                     "propulsion.main.r_CoM_to_application_B"),
              vector(propulsion["force_B_N"],
                     "propulsion.main.force_B")))
    pretransported_application = closure_reference(pretransported_case)
    require(pretransported_application[
                "total_moment_about_CoM_B_Nm"] == [
                    Decimal("-44"), Decimal("-78"), Decimal("-32")],
            "pre-transported application-moment mutation shape differs")
    expected_trajectory = analytic_trajectory(trajectory_case)
    doubled_gravity = analytic_trajectory(
        trajectory_case, gravity_double_count=True)
    rejected: set[str] = set()
    if reversed_application["total_moment_about_CoM_B_Nm"] != \
            expected_formula["total_moment_about_CoM_B_Nm"]:
        rejected.add("MUTATION-YYZ-CLOSURE-REVERSED-APPLICATION-VECTOR")
    if pretransported_application["total_moment_about_CoM_B_Nm"] != \
            expected_formula["total_moment_about_CoM_B_Nm"]:
        rejected.add(
            "MUTATION-YYZ-CLOSURE-PRETRANSPORTED-APPLICATION-MOMENT")
    if doubled_gravity["trajectory"][-1] != \
            expected_trajectory["trajectory"][-1]:
        rejected.add("MUTATION-YYZ-CLOSURE-GRAVITY-DOUBLE-COUNT")
    return rejected


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    by_id = cases_by_id(cases)
    formula_case = by_id["CASE-YYZ-CLOSURE-3D-WRENCH"]
    trajectory_case = by_id["CASE-YYZ-CLOSURE-RIGID-CORE-TRAJECTORY"]
    formula = closure_reference(formula_case)
    reversed_case = copy.deepcopy(formula_case)
    reversed_case["contributions"].reverse()
    reversed_formula = closure_reference(reversed_case)
    require(reversed_formula == formula,
            "Decimal closure contribution order changed semantic result")
    return stringify({
        "schema_version": "gnczmkn.yyz-force-moment-closure-reference/1",
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "input_identity": {
            "path": "fixtures/ref-yyz-force-moment-closure/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "reference_method": {
            "implementation": "CPython standard-library decimal",
            "precision_digits": getcontext().prec,
            "solution": "direct wrench transport and closed-form constant-acceleration trajectory",
        },
        "model_choice_status": cases["model_choice"]["status"],
        "cases": {
            formula_case["id"]: {"closure": formula},
            trajectory_case["id"]: analytic_trajectory(trajectory_case),
        },
        "equivalence_cases": cases["equivalence_cases"],
        "invalid_input_cases": cases["invalid_input_cases"],
        "mutation_cases": cases["mutation_cases"],
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
    actual_value = decimal(actual)
    expected_value = decimal(expected)
    checks.require(near(actual_value, expected_value, absolute, relative),
                   f"{label} differs: {actual_value} vs {expected_value}")


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
    difference = subtract(actual_q, expected_q)
    chord = dot(difference, difference).sqrt()
    return 4.0 * math.asin(min(1.0, 0.5 * float(chord)))


def compare_closure(checks: Checks, actual: dict, expected: dict,
                    absolute: Decimal, relative: Decimal, label: str) -> None:
    checks.require(actual["strategy"] == expected["strategy"],
                   f"{label}.strategy differs")
    checks.require(actual["context"] == expected["context"],
                   f"{label}.context differs")
    actual_contributions = actual["contributions"]
    expected_contributions = expected["contributions"]
    checks.require(len(actual_contributions) == len(expected_contributions),
                   f"{label}.contribution count differs")
    vector_fields = (
        "force_B_N",
        "r_CoM_to_application_B_m",
        "moment_at_application_B_Nm",
        "lever_arm_moment_B_Nm",
        "moment_about_CoM_B_Nm",
    )
    for index, (actual_entry, expected_entry) in enumerate(zip(
            actual_contributions, expected_contributions)):
        checks.require(actual_entry["source_id"] == expected_entry["source_id"],
                       f"{label}.source {index} differs")
        for field in vector_fields:
            compare_vector(checks, actual_entry[field], expected_entry[field],
                           absolute, relative,
                           f"{label}.{actual_entry['source_id']}.{field}")
    compare_vector(checks, actual["total_force_B_N"],
                   expected["total_force_B_N"], absolute, relative,
                   f"{label}.total_force")
    compare_vector(checks, actual["total_moment_about_CoM_B_Nm"],
                   expected["total_moment_about_CoM_B_Nm"],
                   absolute, relative, f"{label}.total_moment")


def run_probe(path: Path) -> tuple[str, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    require(completed.returncode == 0,
            f"C++ YYZ closure probe failed: {completed.stderr.strip()}")
    return completed.stdout, json.loads(
        completed.stdout, parse_float=Decimal)


def verify_reference(cases: dict, raw_cases: bytes, oracle: dict,
                     probe_path: Path) -> dict:
    checks = Checks()
    checks.require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
                   "YYZ closure fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "YYZ closure oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "YYZ closure model identity differs", 2)
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "YYZ closure cases byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-force-moment-closure/cases.json",
                   "YYZ closure input path differs")

    recomputed = build_reference(cases, raw_cases)
    formula_id = "CASE-YYZ-CLOSURE-3D-WRENCH"
    trajectory_id = "CASE-YYZ-CLOSURE-RIGID-CORE-TRAJECTORY"
    checks.require(oracle["cases"][formula_id] ==
                   recomputed["cases"][formula_id],
                   "stored YYZ closure formula differs")
    checks.require(oracle["cases"][trajectory_id] ==
                   recomputed["cases"][trajectory_id],
                   "stored YYZ closure trajectory differs")
    checks.require(oracle["equivalence_cases"] == cases["equivalence_cases"],
                   "stored YYZ closure equivalence definitions differ")
    checks.require(oracle["invalid_input_cases"] == cases["invalid_input_cases"],
                   "stored YYZ closure invalid-input definitions differ")
    checks.require(oracle["mutation_cases"] == cases["mutation_cases"],
                   "stored YYZ closure mutation definitions differ")

    expected_invalid = {entry["id"] for entry in cases["invalid_input_cases"]}
    reference_invalid = independent_invalid_rejections(cases)
    checks.require(reference_invalid == expected_invalid,
                   "Python closure reference accepted an invalid input",
                   len(reference_invalid) + 1)
    expected_mutations = {entry["id"] for entry in cases["mutation_cases"]}
    reference_mutations = independent_mutation_rejections(cases)
    checks.require(reference_mutations == expected_mutations,
                   "Python closure reference accepted a physical mutation",
                   len(reference_mutations) + 1)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ YYZ closure probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed" and
                   probe["model_choice_status"] ==
                   cases["model_choice"]["status"],
                   "C++ YYZ closure probe identity differs", 4)

    tolerances = cases["tolerances"]
    formula_absolute = decimal(tolerances["formula_absolute"])
    formula_relative = decimal(tolerances["formula_relative"])
    expected_formula = oracle["cases"][formula_id]["closure"]
    compare_closure(checks, probe["formula_closure"], expected_formula,
                    formula_absolute, formula_relative, "C++ formula closure")
    compare_closure(checks, probe["reversed_order_closure"], expected_formula,
                    formula_absolute, formula_relative,
                    "C++ reversed-order closure")

    expected_trajectory = oracle["cases"][trajectory_id]
    actual_trajectory = probe["rigid_core_trajectory"]
    checks.require(actual_trajectory["strategy"] == "FrozenInterval" and
                   actual_trajectory["held_through_rk_stages"] is True,
                   "C++ closure was not held through RK stages", 2)
    compare_closure(checks, actual_trajectory["held_closure"],
                    expected_trajectory["held_closure"],
                    formula_absolute, formula_relative,
                    "C++ held closure")
    for field in ("gravity_I_mps2",
                  "body_force_acceleration_I_mps2",
                  "total_acceleration_I_mps2"):
        compare_vector(checks, actual_trajectory[field],
                       expected_trajectory[field],
                       formula_absolute, formula_relative,
                       f"C++ trajectory {field}")

    actual_samples = actual_trajectory["trajectory"]
    expected_samples = expected_trajectory["trajectory"]
    checks.require(len(actual_samples) == len(expected_samples),
                   "C++ closure trajectory length differs")
    for index, (actual, expected) in enumerate(zip(
            actual_samples, expected_samples)):
        checks.require(actual["tick"] == expected["tick"],
                       f"C++ closure trajectory tick {index} differs")
        compare_scalar(checks, actual["time_s"], expected["time_s"],
                       decimal(tolerances["time_absolute_s"]), Decimal(0),
                       f"C++ closure trajectory time {index}")
        compare_vector(checks, actual["position_I_m"],
                       expected["position_I_m"],
                       decimal(tolerances["position_absolute_m"]),
                       decimal(tolerances["position_relative"]),
                       f"C++ closure trajectory position {index}")
        compare_vector(checks, actual["velocity_I_mps"],
                       expected["velocity_I_mps"],
                       decimal(tolerances["velocity_absolute_mps"]),
                       decimal(tolerances["velocity_relative"]),
                       f"C++ closure trajectory velocity {index}")
        compare_vector(checks, actual["omega_BI_B_radps"],
                       expected["omega_BI_B_radps"],
                       decimal(tolerances["angular_rate_absolute_radps"]),
                       Decimal(0),
                       f"C++ closure trajectory omega {index}")
        checks.require(
            Decimal(str(orientation_error(
                actual["q_I_B_wxyz"], expected["q_I_B_wxyz"]))) <=
            decimal(tolerances["orientation_error_max_rad"]),
            f"C++ closure trajectory orientation {index} differs")

    checks.require(actual_trajectory["terminal"]["kind"] ==
                   expected_trajectory["terminal"]["kind"] and
                   actual_trajectory["terminal"]["tick"] ==
                   expected_trajectory["terminal"]["tick"],
                   "C++ closure terminal identity differs", 2)
    compare_scalar(checks, actual_trajectory["terminal"]["time_s"],
                   expected_trajectory["terminal"]["time_s"],
                   decimal(tolerances["time_absolute_s"]), Decimal(0),
                   "C++ closure terminal time")

    actual_invalid = set(probe["invalid_input_rejections"])
    checks.require(actual_invalid == expected_invalid,
                   "C++ closure invalid-input identities differ")
    actual_mutations = set(probe["mutation_rejections"])
    checks.require(actual_mutations == expected_mutations,
                   "C++ closure mutation identities differ")

    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "formula_total_force_B_N":
            probe["formula_closure"]["total_force_B_N"],
        "formula_total_moment_about_CoM_B_Nm":
            probe["formula_closure"]["total_moment_about_CoM_B_Nm"],
        "trajectory_terminal_tick":
            actual_trajectory["terminal"]["tick"],
        "invalid_input_cases_rejected": len(actual_invalid),
        "mutation_cases_rejected": len(actual_mutations),
    })


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
    require(cases["schema_version"] ==
            "gnczmkn.yyz-force-moment-closure-cases/1" and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model"]["model_id"] == MODEL_ID,
            "YYZ closure cases identity differs")

    if arguments.generate_reference:
        require(arguments.oracle is None and arguments.probe is None,
                "reference generation does not accept --oracle or --probe")
        print(json.dumps(build_reference(cases, raw_cases), indent=2,
                         ensure_ascii=False))
        return 0

    require(arguments.oracle is not None and arguments.probe is not None,
            "verification requires --oracle and --probe")
    oracle = json.loads(
        arguments.oracle.read_text(encoding="utf-8"), parse_float=Decimal)
    result = verify_reference(cases, raw_cases, oracle, arguments.probe)
    print(json.dumps(result, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (IndexError, KeyError, OSError, ValueError,
            json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"YYZ force/moment closure reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

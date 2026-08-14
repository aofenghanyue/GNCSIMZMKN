#!/usr/bin/env python3
"""Independent Decimal reference for two committed FrozenIntervals."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal, getcontext
import hashlib
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001"
ORACLE_ID = "ORACLE-YYZ-TWO-INTERVAL-MASS-COMMIT-001"
MODEL_ID = "MODEL-YYZ-TWO-INTERVAL-MASS-COMMIT-001"
MASS_MODEL_ID = "MODEL-YYZ-SCALAR-BURN-CONSTANT-GEOMETRY-001"
CASES_SCHEMA = "gnczmkn.yyz-two-interval-mass-commit-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-two-interval-mass-commit-reference/1"
MASS_STATE_ID = "mass.fixture.yyz.vehicle@1"
INERTIAL_FRAME_ID = "frame.fixture.yyz.inertial-cartesian@1"
BODY_FRAME_ID = "frame.fixture.yyz.body@1"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
CONFIGURATION_REVISION = 11


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


def scale(values: list[Decimal], factor: Decimal) -> list[Decimal]:
    return [value * factor for value in values]


def dot(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    return sum((left * right for left, right in zip(lhs, rhs)), Decimal(0))


def matrix(values: object, label: str) -> list[list[Decimal]]:
    parsed = vector(values, 9, label)
    return [parsed[0:3], parsed[3:6], parsed[6:9]]


def cholesky(value: list[list[Decimal]]) -> None:
    require(all(value[row][column] == value[column][row]
                for row in range(3) for column in range(3)),
            "inertia must be symmetric")
    lower = [[Decimal(0) for _ in range(3)] for _ in range(3)]
    for row in range(3):
        for column in range(row + 1):
            residual = value[row][column] - sum(
                (lower[row][index] * lower[column][index]
                 for index in range(column)), Decimal(0))
            if row == column:
                require(residual > 0, "inertia must be positive definite")
                lower[row][column] = residual.sqrt()
            else:
                lower[row][column] = residual / lower[column][column]


def valid_tick(value: object) -> bool:
    return (isinstance(value, int) and not isinstance(value, bool) and
            value >= 0)


def validate_identity(section: dict, sample_tick: int) -> None:
    require(section["mass_state_id"] == MASS_STATE_ID and
            section["body_frame_id"] == BODY_FRAME_ID and
            section["clock_domain"] == CLOCK_DOMAIN and
            section["configuration_revision"] == CONFIGURATION_REVISION,
            "mass or interval identity differs")
    require(section["sample_tick"] == sample_tick and
            section["valid_from_tick"] == sample_tick and
            section["valid_until_tick"] == sample_tick + 1,
            "sample or interval identity differs")


def validate_case(case: dict) -> None:
    dt_s = decimal(case["base_dt_s"])
    require(dt_s > 0, "base_dt_s must be positive")
    state = case["initial_state"]
    require(state["sample_tick"] == 0, "initial sample tick differs")
    vector(state["position_I_m"], 3, "initial position")
    vector(state["velocity_I_mps"], 3, "initial velocity")
    quaternion = vector(state["q_I_B_wxyz"], 4, "initial q_I_B")
    require(abs(quaternion[0]) == 1 and
            all(value.is_zero() for value in quaternion[1:]),
            "analytic profile requires identity q_I_B up to sign")
    rates = vector(state["omega_BI_B_radps"], 3, "initial angular rate")
    require(all(value.is_zero() for value in rates),
            "analytic profile requires zero angular rate")

    committed = case["committed_mass_state"]
    validate_identity(committed, 0)
    require(decimal(committed["mass_kg"]) > 0,
            "committed mass must be positive")
    vector(committed["r_body_origin_to_CoM_B_m"], 3, "center of mass")
    cholesky(matrix(committed["inertia_about_CoM_B_kgm2_row_major"],
                    "inertia"))

    intervals = case["intervals"]
    require(isinstance(intervals, list) and len(intervals) == 2,
            "exactly two intervals are required")
    current_mass = decimal(committed["mass_kg"])
    for index, interval in enumerate(intervals):
        require(interval["inertial_frame_id"] == INERTIAL_FRAME_ID,
                "interval inertial frame differs")
        validate_identity(interval, index)
        vector(interval["force_total_B_N"], 3, "held force")
        moment = vector(interval["moment_total_about_CoM_B_Nm"], 3,
                        "held moment")
        require(all(value.is_zero() for value in moment),
                "analytic profile requires zero held moment")
        vector(interval["gravity_I_mps2"], 3, "gravity")
        rate = decimal(interval["fuel_consumption_rate_kgps"])
        require(rate >= 0, "fuel-consumption rate must be nonnegative")
        current_mass -= rate * dt_s
        require(current_mass > 0, "mass candidate must be positive")
    require(case["terminal"]["kind"] == "duration_exact_grid" and
            case["terminal"]["expected_tick"] == 2,
            "terminal identity differs")


def state_from_fixture(value: dict) -> dict:
    return {
        "position_I_m": vector(value["position_I_m"], 3, "position"),
        "velocity_I_mps": vector(value["velocity_I_mps"], 3, "velocity"),
        "q_I_B_wxyz": vector(value["q_I_B_wxyz"], 4, "q_I_B"),
        "omega_BI_B_radps": vector(value["omega_BI_B_radps"], 3,
                                    "angular rate"),
    }


def analytic_step(state: dict, acceleration: list[Decimal],
                  dt_s: Decimal) -> dict:
    return {
        "position_I_m": add(
            add(state["position_I_m"], scale(state["velocity_I_mps"], dt_s)),
            scale(acceleration, dt_s * dt_s / Decimal(2))),
        "velocity_I_mps": add(
            state["velocity_I_mps"], scale(acceleration, dt_s)),
        "q_I_B_wxyz": list(state["q_I_B_wxyz"]),
        "omega_BI_B_radps": list(state["omega_BI_B_radps"]),
    }


def advance(state: dict, acceleration: list[Decimal], dt_s: Decimal,
            substeps: int) -> dict:
    require(substeps > 0, "substep count must be positive")
    result = copy.deepcopy(state)
    substep_dt = dt_s / Decimal(substeps)
    for _ in range(substeps):
        result = analytic_step(result, acceleration, substep_dt)
    return result


def state_vector(value: dict) -> list[Decimal]:
    return [*value["position_I_m"], *value["velocity_I_mps"],
            *value["q_I_B_wxyz"], *value["omega_BI_B_radps"]]


def evaluate(case: dict, *, early_visibility: bool = False,
             stale_next_mass: bool = False,
             stale_rigid_state: bool = False,
             substeps: int = 1) -> dict:
    validate_case(case)
    dt_s = decimal(case["base_dt_s"])
    committed_mass_input = case["committed_mass_state"]
    center_of_mass = vector(
        committed_mass_input["r_body_origin_to_CoM_B_m"], 3,
        "center of mass")
    inertia = vector(
        committed_mass_input["inertia_about_CoM_B_kgm2_row_major"], 9,
        "inertia")
    initial_state = state_from_fixture(case["initial_state"])
    committed_state = copy.deepcopy(initial_state)
    committed_mass = decimal(committed_mass_input["mass_kg"])
    opening_mass = committed_mass
    interval_results = []

    for index, interval in enumerate(case["intervals"]):
        rate = decimal(interval["fuel_consumption_rate_kgps"])
        consumed_mass = rate * dt_s
        candidate_mass = committed_mass - consumed_mass
        integration_mass = committed_mass
        if index == 0 and early_visibility:
            integration_mass = candidate_mass
        if index == 1 and stale_next_mass:
            integration_mass = opening_mass
        interval_initial = committed_state
        if index == 1 and stale_rigid_state:
            interval_initial = initial_state
        force = vector(interval["force_total_B_N"], 3, "held force")
        gravity = vector(interval["gravity_I_mps2"], 3, "gravity")
        acceleration = add(scale(force, Decimal(1) / integration_mass),
                           gravity)
        rigid_candidate = advance(
            interval_initial, acceleration, dt_s, substeps)
        interval_results.append({
            "sample_tick": interval["sample_tick"],
            "valid_from_tick": interval["valid_from_tick"],
            "valid_until_tick": interval["valid_until_tick"],
            "current_committed_mass_kg": committed_mass,
            "integration_mass_kg": integration_mass,
            "fuel_consumption_rate_kgps": rate,
            "consumed_mass_kg": consumed_mass,
            "pending_mass_candidate_kg": candidate_mass,
            "pending_visibility_before_commit": "candidate-only",
            "held_force_total_B_N": force,
            "held_moment_total_about_CoM_B_Nm": vector(
                interval["moment_total_about_CoM_B_Nm"], 3, "held moment"),
            "gravity_I_mps2": gravity,
            "acceleration_I_mps2": acceleration,
            "initial_rigid_state": interval_initial,
            "rigid_candidate": rigid_candidate,
            "closing_commit": {
                "tick": interval["valid_until_tick"],
                "kind": "atomic-rigid-and-mass",
                "mass_kg": candidate_mass,
                "r_body_origin_to_CoM_B_m": center_of_mass,
                "inertia_about_CoM_B_kgm2_row_major": inertia,
                "rigid_state": rigid_candidate,
            },
        })
        committed_mass = candidate_mass
        committed_state = rigid_candidate

    return {
        "id": case["id"],
        "model_id": MODEL_ID,
        "mass_evolution_model_id": MASS_MODEL_ID,
        "base_dt_s": dt_s,
        "intervals": interval_results,
        "terminal": {
            "tick": case["terminal"]["expected_tick"],
            "time_s": Decimal(case["terminal"]["expected_tick"]) * dt_s,
            "termination_kind": case["terminal"]["kind"],
            "committed_mass_kg": committed_mass,
            "r_body_origin_to_CoM_B_m": center_of_mass,
            "inertia_about_CoM_B_kgm2_row_major": inertia,
            "rigid_state": committed_state,
        },
    }


def physical_vector(value: dict) -> list[Decimal]:
    result: list[Decimal] = []
    for interval in value["intervals"]:
        result.extend([
            interval["current_committed_mass_kg"],
            interval["integration_mass_kg"],
            interval["consumed_mass_kg"],
            interval["pending_mass_candidate_kg"],
        ])
        result.extend(interval["acceleration_I_mps2"])
        result.extend(state_vector(interval["initial_rigid_state"]))
        result.extend(state_vector(interval["rigid_candidate"]))
    result.append(value["terminal"]["committed_mass_kg"])
    result.extend(state_vector(value["terminal"]["rigid_state"]))
    return result


def max_difference(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    require(len(lhs) == len(rhs), "physical vector shape differs")
    return max((abs(left - right) for left, right in zip(lhs, rhs)),
               default=Decimal(0))


def invalid_rejections(cases: dict, accepted_case: dict) -> list[str]:
    actions = {
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-STATE-ID":
            lambda item: item["intervals"][0].__setitem__(
                "mass_state_id", "mass.fixture.yyz.other@1"),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-FRAME":
            lambda item: item["intervals"][1].__setitem__(
                "body_frame_id", "frame.fixture.yyz.other@1"),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-CLOCK":
            lambda item: item["committed_mass_state"].__setitem__(
                "clock_domain", "clock.fixture.yyz.other@1"),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-REVISION":
            lambda item: item["intervals"][1].__setitem__(
                "configuration_revision", 12),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-SAMPLE-TICK":
            lambda item: item["intervals"][0].__setitem__("sample_tick", 1),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-INTERVAL-SEQUENCE":
            lambda item: item["intervals"][1].update(
                {"sample_tick": 2, "valid_from_tick": 2,
                 "valid_until_tick": 3}),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NONPOSITIVE-DT":
            lambda item: item.__setitem__("base_dt_s", 0),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NONPOSITIVE-MASS":
            lambda item: item["committed_mass_state"].__setitem__(
                "mass_kg", 0),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NEGATIVE-FLOW":
            lambda item: item["intervals"][0].__setitem__(
                "fuel_consumption_rate_kgps", -1),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-DEPLETION":
            lambda item: item["intervals"][0].__setitem__(
                "fuel_consumption_rate_kgps", 1200),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NONZERO-MOMENT":
            lambda item: item["intervals"][0].__setitem__(
                "moment_total_about_CoM_B_Nm", [0, 1, 0]),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-NONZERO-RATE":
            lambda item: item["initial_state"].__setitem__(
                "omega_BI_B_radps", [0, 0, 1]),
        "INVALID-YYZ-TWO-INTERVAL-MASS-COMMIT-ZERO-QUATERNION":
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
            evaluate(mutated)
        except (ArithmeticError, IndexError, KeyError, TypeError, ValueError):
            rejected.append(identifier)
        else:
            raise ValueError(f"invalid input was accepted: {identifier}")
    return rejected


def mutation_results(cases: dict, accepted_case: dict,
                     accepted: dict) -> list[dict]:
    early = evaluate(accepted_case, early_visibility=True)
    stale_mass = evaluate(accepted_case, stale_next_mass=True)
    stale_rigid = evaluate(accepted_case, stale_rigid_state=True)
    results = [
        {
            "id": "MUTATION-YYZ-TWO-INTERVAL-MASS-COMMIT-EARLY-VISIBILITY",
            "status": "rejected",
            "observed_interval0_integration_mass_kg":
                early["intervals"][0]["integration_mass_kg"],
            "observed_tick1_rigid_state":
                early["intervals"][0]["rigid_candidate"],
            "observed_terminal_rigid_state": early["terminal"]["rigid_state"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(early)),
        },
        {
            "id": "MUTATION-YYZ-TWO-INTERVAL-MASS-COMMIT-STALE-NEXT-MASS",
            "status": "rejected",
            "observed_interval1_committed_mass_kg":
                stale_mass["intervals"][1]["current_committed_mass_kg"],
            "observed_interval1_integration_mass_kg":
                stale_mass["intervals"][1]["integration_mass_kg"],
            "observed_interval1_acceleration_I_mps2":
                stale_mass["intervals"][1]["acceleration_I_mps2"],
            "observed_terminal_rigid_state":
                stale_mass["terminal"]["rigid_state"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(stale_mass)),
        },
        {
            "id": "MUTATION-YYZ-TWO-INTERVAL-MASS-COMMIT-STALE-RIGID-STATE",
            "status": "rejected",
            "observed_interval1_initial_rigid_state":
                stale_rigid["intervals"][1]["initial_rigid_state"],
            "observed_interval1_committed_mass_kg":
                stale_rigid["intervals"][1]["current_committed_mass_kg"],
            "observed_terminal_rigid_state":
                stale_rigid["terminal"]["rigid_state"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(stale_rigid)),
        },
    ]
    require([entry["id"] for entry in results] ==
            [entry["id"] for entry in cases["mutation_cases"]],
            "mutation identities differ")
    require(all(entry["max_abs_physical_difference"] > 0
                for entry in results), "a temporal mutation survived")
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
            cases["model_choice"]["status"] == "accepted",
            "two-interval cases identity differs")
    model = cases["model"]
    require(model["model_id"] == MODEL_ID and
            model["mass_evolution_model_id"] == MASS_MODEL_ID and
            model["mass_state_id"] == MASS_STATE_ID and
            model["inertial_frame_id"] == INERTIAL_FRAME_ID and
            model["body_frame_id"] == BODY_FRAME_ID and
            model["clock_domain"] == CLOCK_DOMAIN and
            model["configuration_revision"] == CONFIGURATION_REVISION and
            model["closure_strategy"] == "FrozenInterval" and
            model["commit_policy"] == "atomic-rigid-and-mass",
            "two-interval model profile differs")
    require(len(cases["cases"]) == 1,
            "two-interval bundle must contain one trajectory case")


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    validate_cases_identity(cases)
    accepted_case = cases["cases"][0]
    accepted = evaluate(accepted_case)
    substeps = evaluate(accepted_case, substeps=2)
    substep_difference = max_difference(
        physical_vector(accepted), physical_vector(substeps))
    require(substep_difference <= Decimal("1e-70"),
            "analytic substep partition changed the accepted result")
    equivalence = [{
        "id": "EQUIV-YYZ-TWO-INTERVAL-MASS-COMMIT-RK4-SUBSTEPS",
        "status": "passed",
        "full_step_terminal_rigid_state": accepted["terminal"]["rigid_state"],
        "two_substep_terminal_rigid_state":
            substeps["terminal"]["rigid_state"],
        "full_step_terminal_mass_kg": accepted["terminal"]["committed_mass_kg"],
        "two_substep_terminal_mass_kg":
            substeps["terminal"]["committed_mass_kg"],
        "max_abs_physical_difference": substep_difference,
    }]
    return stringify({
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "precision": {
            "implementation":
                "Python standard-library Decimal piecewise analytic trajectory",
            "decimal_digits": getcontext().prec,
        },
        "input_identity": {
            "path":
                "fixtures/ref-yyz-two-interval-mass-commit/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": {accepted_case["id"]: accepted},
        "equivalence_results": equivalence,
        "invalid_input_rejections": invalid_rejections(
            cases, accepted_case),
        "mutation_results": mutation_results(
            cases, accepted_case, accepted),
    })


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


def compare_tree(checks: Checks, actual, expected,
                 absolute: Decimal, relative: Decimal, label: str) -> None:
    if isinstance(expected, dict):
        checks.require(isinstance(actual, dict), f"{label} is not an object")
        checks.require(set(actual) == set(expected), f"{label} fields differ")
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
        actual_value = decimal(actual)
        expected_value = decimal(expected)
        difference = abs(actual_value - expected_value)
        bound = absolute + relative * max(
            abs(actual_value), abs(expected_value), Decimal(1))
        checks.require(difference <= bound,
                       f"{label} differs: {actual_value} vs {expected_value}")
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
                   "fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "model identity differs", 2)
    checks.require(oracle["precision"]["decimal_digits"] >= 70,
                   "reference precision is below 70 digits")
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "input byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-two-interval-mass-commit/cases.json",
                   "input path differs")
    recomputed = build_reference(cases, raw_cases)
    checks.require(oracle == recomputed,
                   "stored oracle differs from its producer",
                   len(oracle["cases"]) + 4)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed",
                   "C++ probe identity differs", 3)
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    compare_tree(checks, probe["cases"], list(oracle["cases"].values()),
                 absolute, relative, "cases")
    compare_tree(checks, probe["equivalence_results"],
                 oracle["equivalence_results"], absolute, relative,
                 "equivalence_results")
    checks.require(probe["invalid_input_rejections"] ==
                   oracle["invalid_input_rejections"],
                   "invalid-input identities differ",
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
        "interval0_integration_mass_kg":
            accepted["intervals"][0]["integration_mass_kg"],
        "tick1_committed_mass_kg":
            accepted["intervals"][0]["closing_commit"]["mass_kg"],
        "interval1_integration_mass_kg":
            accepted["intervals"][1]["integration_mass_kg"],
        "terminal_mass_kg": accepted["terminal"]["committed_mass_kg"],
        "terminal_position_I_m":
            accepted["terminal"]["rigid_state"]["position_I_m"],
        "terminal_velocity_I_mps":
            accepted["terminal"]["rigid_state"]["velocity_I_mps"],
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
        print(f"YYZ two-interval mass commit reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

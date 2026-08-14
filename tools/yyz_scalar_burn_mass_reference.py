#!/usr/bin/env python3
"""Independent Decimal reference for sampled scalar-burn MassState evolution."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal, getcontext
import hashlib
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-SCALAR-BURN-MASS-001"
ORACLE_ID = "ORACLE-YYZ-SCALAR-BURN-MASS-001"
MODEL_ID = "MODEL-YYZ-SCALAR-BURN-CONSTANT-GEOMETRY-001"
CASES_SCHEMA = "gnczmkn.yyz-scalar-burn-mass-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-scalar-burn-mass-reference/1"
PROFILE_STATUS = "accepted"
MASS_STATE_ID = "mass.fixture.yyz.vehicle@1"
BODY_FRAME_ID = "frame.fixture.yyz.body@1"
BODY_ORIGIN_POINT_ID = "point.fixture.yyz.body-origin@1"
COM_POINT_ID = "point.fixture.yyz.center-of-mass@1"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
QUALITY = "Valid"


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
            "MassState inertia must be symmetric")
    lower = [[Decimal(0) for _ in range(3)] for _ in range(3)]
    for row in range(3):
        for column in range(row + 1):
            residual = inertia[row][column] - sum(
                (lower[row][index] * lower[column][index]
                 for index in range(column)), Decimal(0))
            if row == column:
                require(residual > 0,
                        "MassState inertia must be positive definite")
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


def validate_case(case: dict) -> None:
    context = case["context"]
    require(context["mass_state_id"] == MASS_STATE_ID and
            context["body_frame_id"] == BODY_FRAME_ID and
            context["clock_domain"] == CLOCK_DOMAIN,
            "scalar-burn context identity differs")
    for field in ("sample_tick", "configuration_revision",
                  "valid_from_tick", "valid_until_tick",
                  "next_valid_until_tick"):
        require(valid_nonnegative_integer(context[field]),
                f"{field} must be a nonnegative integer")
    require(context["sample_tick"] == context["valid_from_tick"] and
            context["valid_until_tick"] > context["valid_from_tick"] and
            context["next_valid_until_tick"] > context["valid_until_tick"],
            "scalar-burn interval identity is invalid")
    dt_s = decimal(context["base_dt_s"])
    require(dt_s > 0, "base_dt_s must be positive")

    state = case["committed_mass_state"]
    require(decimal(state["mass_kg"]) > 0,
            "committed mass must be positive")
    vector(state["r_body_origin_to_CoM_B_m"], 3, "committed CoM")
    cholesky(matrix(state["inertia_about_CoM_B_kgm2_row_major"],
                    "committed inertia"))

    flow = case["mass_flow_interval"]
    require(flow["source_id"], "MassFlowInterval source_id is empty")
    require(flow["mass_state_id"] == context["mass_state_id"],
            "MassFlowInterval mass state identity differs")
    require(flow["body_frame_id"] == context["body_frame_id"],
            "MassFlowInterval body frame differs")
    require(flow["clock_domain"] == context["clock_domain"],
            "MassFlowInterval clock domain differs")
    require(flow["configuration_revision"] ==
            context["configuration_revision"],
            "MassFlowInterval configuration revision differs")
    require(flow["valid_from_tick"] == context["valid_from_tick"] and
            flow["valid_until_tick"] == context["valid_until_tick"],
            "MassFlowInterval validity differs")
    rate = decimal(flow["fuel_consumption_rate_kgps"])
    require(rate >= 0, "fuel consumption rate must be nonnegative")
    duration = Decimal(context["valid_until_tick"] -
                       context["valid_from_tick"]) * dt_s
    require(decimal(state["mass_kg"]) - rate * duration > 0,
            "scalar-burn candidate mass must be positive")

    probe = case["consumer_probe"]
    require(probe["application_point_id"],
            "consumer application point identity is empty")
    vector(probe["r_body_origin_to_application_B_m"], 3,
           "consumer application point")
    vector(probe["force_B_N"], 3, "consumer force")
    vector(probe["intrinsic_moment_at_application_B_Nm"], 3,
           "consumer intrinsic moment")
    vector(probe["omega_BI_B_radps"], 3, "consumer angular rate")


def consumer_values(mass_kg: Decimal, com: list[Decimal],
                    inertia: list[list[Decimal]], probe: dict) -> dict:
    application = vector(
        probe["r_body_origin_to_application_B_m"], 3,
        "application point")
    force = vector(probe["force_B_N"], 3, "force")
    intrinsic = vector(
        probe["intrinsic_moment_at_application_B_Nm"], 3,
        "intrinsic moment")
    omega = vector(probe["omega_BI_B_radps"], 3, "angular rate")
    lever = subtract(application, com)
    transport = cross(lever, force)
    moment_com = add(intrinsic, transport)
    angular_momentum = matrix_vector(inertia, omega)
    gyroscopic = cross(omega, angular_momentum)
    net_moment = subtract(moment_com, gyroscopic)
    angular_acceleration = solve_spd(inertia, net_moment)
    return {
        "application_point_id": probe["application_point_id"],
        "mass_kg": mass_kg,
        "r_CoM_to_application_B_m": lever,
        "force_B_N": force,
        "moment_about_CoM_B_Nm": moment_com,
        "specific_force_B_mps2": scale(force, Decimal(1) / mass_kg),
        "inertia_about_CoM_B_kgm2_row_major": flatten(inertia),
        "angular_momentum_B_kgm2ps": angular_momentum,
        "gyroscopic_moment_B_Nm": gyroscopic,
        "angular_acceleration_B_radps2": angular_acceleration,
    }


def evaluate(case: dict, *, mass_sign: str = "subtract",
             visibility: str = "candidate-only",
             com_mode: str = "constant",
             inertia_mode: str = "constant") -> dict:
    validate_case(case)
    context = case["context"]
    state = case["committed_mass_state"]
    flow = case["mass_flow_interval"]
    dt_s = decimal(context["base_dt_s"])
    duration = Decimal(context["valid_until_tick"] -
                       context["valid_from_tick"]) * dt_s
    rate = decimal(flow["fuel_consumption_rate_kgps"])
    consumed = rate * duration
    current_mass = decimal(state["mass_kg"])
    candidate_mass = (current_mass - consumed if mass_sign == "subtract"
                      else current_mass + consumed)
    require(candidate_mass > 0, "mutated candidate mass is nonpositive")
    current_com = vector(
        state["r_body_origin_to_CoM_B_m"], 3, "current CoM")
    current_inertia = matrix(
        state["inertia_about_CoM_B_kgm2_row_major"], "current inertia")
    candidate_com = (list(current_com) if com_mode == "constant" else
                     add(current_com, [consumed, Decimal(0), Decimal(0)]))
    candidate_inertia = (
        [list(row) for row in current_inertia]
        if inertia_mode == "constant" else
        [scale(row, candidate_mass / current_mass)
         for row in current_inertia])
    cholesky(candidate_inertia)
    interval_visible_mass = (
        current_mass if visibility == "candidate-only" else candidate_mass)
    interval_consumer = consumer_values(
        interval_visible_mass, current_com, current_inertia,
        case["consumer_probe"])
    next_consumer = consumer_values(
        candidate_mass, candidate_com, candidate_inertia,
        case["consumer_probe"])
    return {
        "id": case["id"],
        "identity": {
            "model_id": MODEL_ID,
            "mass_state_id": context["mass_state_id"],
            "body_frame_id": context["body_frame_id"],
            "body_origin_point_id": BODY_ORIGIN_POINT_ID,
            "center_of_mass_point_id": COM_POINT_ID,
            "clock_domain": context["clock_domain"],
            "configuration_revision": context["configuration_revision"],
            "valid_from_tick": context["valid_from_tick"],
            "valid_until_tick": context["valid_until_tick"],
        },
        "current_committed_sample": {
            "quality": QUALITY,
            "sample_tick": context["sample_tick"],
            "mass_kg": current_mass,
            "r_body_origin_to_CoM_B_m": current_com,
            "inertia_about_CoM_B_kgm2_row_major": flatten(current_inertia),
            "visibility": "committed-through-interval",
        },
        "interval_candidate": {
            "source_id": flow["source_id"],
            "interval_duration_s": duration,
            "fuel_consumption_rate_kgps": rate,
            "consumed_mass_kg": consumed,
            "mass_delta_kg": (candidate_mass - current_mass),
            "mass_kg": candidate_mass,
            "r_body_origin_to_CoM_B_m": candidate_com,
            "inertia_about_CoM_B_kgm2_row_major":
                flatten(candidate_inertia),
            "visibility_before_commit": visibility,
            "next_commit_tick": context["valid_until_tick"],
        },
        "interval_consumer": interval_consumer,
        "next_committed_sample": {
            "quality": QUALITY,
            "sample_tick": context["valid_until_tick"],
            "valid_from_tick": context["valid_until_tick"],
            "valid_until_tick": context["next_valid_until_tick"],
            "mass_kg": candidate_mass,
            "r_body_origin_to_CoM_B_m": candidate_com,
            "inertia_about_CoM_B_kgm2_row_major":
                flatten(candidate_inertia),
            "visibility": "committed-at-next-boundary",
        },
        "next_consumer": next_consumer,
    }


def max_difference(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    require(len(lhs) == len(rhs), "numeric vectors have different lengths")
    return max((abs(left - right) for left, right in zip(lhs, rhs)),
               default=Decimal(0))


def physical_vector(result: dict) -> list[Decimal]:
    values: list[Decimal] = []
    for section, fields in (
        ("current_committed_sample", (
            "mass_kg", "r_body_origin_to_CoM_B_m",
            "inertia_about_CoM_B_kgm2_row_major")),
        ("interval_candidate", (
            "interval_duration_s", "consumed_mass_kg", "mass_delta_kg",
            "mass_kg", "r_body_origin_to_CoM_B_m",
            "inertia_about_CoM_B_kgm2_row_major")),
        ("interval_consumer", (
            "mass_kg", "r_CoM_to_application_B_m",
            "moment_about_CoM_B_Nm", "specific_force_B_mps2",
            "angular_momentum_B_kgm2ps",
            "gyroscopic_moment_B_Nm",
            "angular_acceleration_B_radps2")),
        ("next_committed_sample", (
            "mass_kg", "r_body_origin_to_CoM_B_m",
            "inertia_about_CoM_B_kgm2_row_major")),
        ("next_consumer", (
            "mass_kg", "r_CoM_to_application_B_m",
            "moment_about_CoM_B_Nm", "specific_force_B_mps2",
            "angular_momentum_B_kgm2ps",
            "gyroscopic_moment_B_Nm",
            "angular_acceleration_B_radps2")),
    ):
        for field in fields:
            value = result[section][field]
            values.extend(value if isinstance(value, list) else [value])
    return values


def partition_equivalence(case: dict, accepted: dict) -> dict:
    context = case["context"]
    state = case["committed_mass_state"]
    flow = case["mass_flow_interval"]
    dt_s = decimal(context["base_dt_s"])
    rate = decimal(flow["fuel_consumption_rate_kgps"])
    current_mass = decimal(state["mass_kg"])
    first_duration = Decimal(22 - 20) * dt_s
    second_duration = Decimal(25 - 22) * dt_s
    first_consumed = rate * first_duration
    second_consumed = rate * second_duration
    first_candidate = current_mass - first_consumed
    final_candidate = first_candidate - second_consumed
    geometry_difference = max(
        max_difference(
            vector(state["r_body_origin_to_CoM_B_m"], 3, "partition CoM"),
            accepted["interval_candidate"]["r_body_origin_to_CoM_B_m"]),
        max_difference(
            vector(state["inertia_about_CoM_B_kgm2_row_major"], 9,
                   "partition inertia"),
            accepted["interval_candidate"][
                "inertia_about_CoM_B_kgm2_row_major"]),
    )
    require(first_consumed + second_consumed ==
            accepted["interval_candidate"]["consumed_mass_kg"] and
            final_candidate == accepted["interval_candidate"]["mass_kg"] and
            geometry_difference == 0,
            "scalar-burn interval partition changed the final state")
    return {
        "id": "EQUIV-YYZ-SCALAR-BURN-MASS-INTERVAL-PARTITION",
        "status": "passed",
        "first_interval_duration_s": first_duration,
        "first_consumed_mass_kg": first_consumed,
        "first_committed_mass_kg": first_candidate,
        "second_interval_duration_s": second_duration,
        "second_consumed_mass_kg": second_consumed,
        "summed_consumed_mass_kg": first_consumed + second_consumed,
        "partitioned_final_mass_kg": final_candidate,
        "unsplit_final_mass_kg":
            accepted["interval_candidate"]["mass_kg"],
        "max_abs_geometry_difference": geometry_difference,
    }


def invalid_rejections(cases: dict, accepted_case: dict) -> list[str]:
    actions = {
        "INVALID-YYZ-SCALAR-BURN-MASS-STATE-IDENTITY":
            lambda item: item["mass_flow_interval"].__setitem__(
                "mass_state_id", "mass.fixture.yyz.other@1"),
        "INVALID-YYZ-SCALAR-BURN-MASS-FRAME-MISMATCH":
            lambda item: item["mass_flow_interval"].__setitem__(
                "body_frame_id", "frame.fixture.yyz.other@1"),
        "INVALID-YYZ-SCALAR-BURN-MASS-CLOCK-MISMATCH":
            lambda item: item["mass_flow_interval"].__setitem__(
                "clock_domain", "clock.fixture.yyz.other@1"),
        "INVALID-YYZ-SCALAR-BURN-MASS-REVISION-MISMATCH":
            lambda item: item["mass_flow_interval"].__setitem__(
                "configuration_revision", 9),
        "INVALID-YYZ-SCALAR-BURN-MASS-INTERVAL-MISMATCH":
            lambda item: item["mass_flow_interval"].__setitem__(
                "valid_until_tick", 24),
        "INVALID-YYZ-SCALAR-BURN-MASS-NONPOSITIVE-DT":
            lambda item: item["context"].__setitem__("base_dt_s", 0),
        "INVALID-YYZ-SCALAR-BURN-MASS-NONPOSITIVE-CURRENT":
            lambda item: item["committed_mass_state"].__setitem__(
                "mass_kg", 0),
        "INVALID-YYZ-SCALAR-BURN-MASS-NEGATIVE-CONSUMPTION":
            lambda item: item["mass_flow_interval"].__setitem__(
                "fuel_consumption_rate_kgps", -0.5),
        "INVALID-YYZ-SCALAR-BURN-MASS-DEPLETED-CANDIDATE":
            lambda item: item["mass_flow_interval"].__setitem__(
                "fuel_consumption_rate_kgps", 240.0),
        "INVALID-YYZ-SCALAR-BURN-MASS-NON-SPD-INERTIA":
            lambda item: item["committed_mass_state"].__setitem__(
                "inertia_about_CoM_B_kgm2_row_major",
                [-1, 0, 0, 0, 20, 0, 0, 0, 30]),
    }
    rejected: list[str] = []
    for specification in cases["invalid_input_cases"]:
        identifier = specification["id"]
        require(identifier in actions,
                f"unsupported scalar-burn invalid case: {identifier}")
        mutated = copy.deepcopy(accepted_case)
        actions[identifier](mutated)
        try:
            evaluate(mutated)
        except (ArithmeticError, IndexError, KeyError, TypeError, ValueError):
            rejected.append(identifier)
        else:
            raise ValueError(f"invalid scalar-burn input survived: {identifier}")
    return rejected


def mutation_results(cases: dict, accepted_case: dict,
                     accepted: dict) -> list[dict]:
    mass_gain = evaluate(accepted_case, mass_sign="add")
    early = evaluate(accepted_case, visibility="committed-early")
    com_drift = evaluate(accepted_case, com_mode="drift")
    inertia_scale = evaluate(accepted_case, inertia_mode="mass-ratio")
    evaluated = [mass_gain, early, com_drift, inertia_scale]
    identifiers = [entry["id"] for entry in cases["mutation_cases"]]
    results = [
        {
            "id": identifiers[0],
            "status": "rejected",
            "observed_mass_delta_kg":
                mass_gain["interval_candidate"]["mass_delta_kg"],
            "observed_candidate_mass_kg":
                mass_gain["interval_candidate"]["mass_kg"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(mass_gain)),
        },
        {
            "id": identifiers[1],
            "status": "rejected",
            "observed_visibility_before_commit":
                early["interval_candidate"]["visibility_before_commit"],
            "observed_interval_visible_mass_kg":
                early["interval_consumer"]["mass_kg"],
            "observed_interval_specific_force_B_mps2":
                early["interval_consumer"]["specific_force_B_mps2"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(early)),
        },
        {
            "id": identifiers[2],
            "status": "rejected",
            "observed_candidate_r_body_origin_to_CoM_B_m":
                com_drift["interval_candidate"][
                    "r_body_origin_to_CoM_B_m"],
            "observed_next_r_CoM_to_application_B_m":
                com_drift["next_consumer"]["r_CoM_to_application_B_m"],
            "observed_next_moment_about_CoM_B_Nm":
                com_drift["next_consumer"]["moment_about_CoM_B_Nm"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(com_drift)),
        },
        {
            "id": identifiers[3],
            "status": "rejected",
            "observed_candidate_inertia_about_CoM_B_kgm2_row_major":
                inertia_scale["interval_candidate"][
                    "inertia_about_CoM_B_kgm2_row_major"],
            "observed_next_angular_momentum_B_kgm2ps":
                inertia_scale["next_consumer"][
                    "angular_momentum_B_kgm2ps"],
            "observed_next_angular_acceleration_B_radps2":
                inertia_scale["next_consumer"][
                    "angular_acceleration_B_radps2"],
            "max_abs_physical_difference": max_difference(
                physical_vector(accepted), physical_vector(inertia_scale)),
        },
    ]
    require(len(evaluated) == len(results) and
            all(entry["max_abs_physical_difference"] > 0
                for entry in results),
            "a scalar-burn mutation matched the accepted model")
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
            cases["model_choice"]["status"] == PROFILE_STATUS and
            cases["model"]["model_id"] == MODEL_ID,
            "scalar-burn cases identity differs")
    model = cases["model"]
    require(model["mass_state_id"] == MASS_STATE_ID and
            model["body_frame_id"] == BODY_FRAME_ID and
            model["body_origin_point_id"] == BODY_ORIGIN_POINT_ID and
            model["center_of_mass_point_id"] == COM_POINT_ID and
            model["clock_domain"] == CLOCK_DOMAIN and
            model["quality"] == QUALITY and
            model["strategy"] == "SampledIntervalCandidate",
            "scalar-burn model profile differs")
    require(len(cases["cases"]) == 2,
            "scalar-burn bundle must contain burn and zero-flow cases")


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    validate_cases_identity(cases)
    computed = {case["id"]: evaluate(case) for case in cases["cases"]}
    first_case = cases["cases"][0]
    first_result = computed[first_case["id"]]
    reference = {
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "precision": {
            "implementation": "Python standard-library Decimal scalar-burn algebra",
            "decimal_digits": getcontext().prec,
        },
        "input_identity": {
            "path": "fixtures/ref-yyz-scalar-burn-mass/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": computed,
        "equivalence_results": [partition_equivalence(
            first_case, first_result)],
        "invalid_input_rejections": invalid_rejections(cases, first_case),
        "mutation_results": mutation_results(
            cases, first_case, first_result),
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
                   "scalar-burn fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "scalar-burn oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "scalar-burn model identity differs", 2)
    checks.require(oracle["precision"]["decimal_digits"] >= 70,
                   "scalar-burn reference precision is below 70 digits")
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "scalar-burn input byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-scalar-burn-mass/cases.json",
                   "scalar-burn input path differs")
    recomputed = build_reference(cases, raw_cases)
    checks.require(oracle == recomputed,
                   "stored scalar-burn oracle differs from its producer",
                   len(oracle["cases"]) + 4)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ scalar-burn probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed" and
                   probe["model_choice_status"] == PROFILE_STATUS,
                   "C++ scalar-burn probe identity differs", 4)
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    compare_tree(checks, probe["cases"], list(oracle["cases"].values()),
                 absolute, relative, "cases")
    compare_tree(checks, probe["equivalence_results"],
                 oracle["equivalence_results"], absolute, relative,
                 "equivalence_results")
    checks.require(probe["invalid_input_rejections"] ==
                   oracle["invalid_input_rejections"],
                   "scalar-burn invalid-input identities differ",
                   len(oracle["invalid_input_rejections"]))
    compare_tree(checks, probe["mutation_results"],
                 oracle["mutation_results"], absolute, relative,
                 "mutation_results")

    first = probe["cases"][0]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "case_count": len(probe["cases"]),
        "current_mass_kg":
            first["current_committed_sample"]["mass_kg"],
        "consumed_mass_kg":
            first["interval_candidate"]["consumed_mass_kg"],
        "candidate_mass_kg": first["interval_candidate"]["mass_kg"],
        "candidate_r_body_origin_to_CoM_B_m":
            first["interval_candidate"]["r_body_origin_to_CoM_B_m"],
        "next_sample_tick": first["next_committed_sample"]["sample_tick"],
        "partition_equivalence_cases_passed":
            len(oracle["equivalence_results"]),
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
        print(f"YYZ scalar-burn mass reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

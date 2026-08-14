#!/usr/bin/env python3
"""Independent Decimal reference for the accepted YYZ propulsion response."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from decimal import Decimal, getcontext
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-PROPULSION-RESPONSE-001"
ORACLE_ID = "ORACLE-YYZ-PROPULSION-RESPONSE-001"
MODEL_ID = "MODEL-YYZ-PROPULSION-RESPONSE-001"
CASES_SCHEMA = "gnczmkn.yyz-propulsion-response-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-propulsion-response-reference/1"
SOURCE_ID = "propulsion.main"
BODY_FRAME_ID = "frame.fixture.yyz.body@1"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
MASS_STATE_ID = "mass.fixture.yyz.vehicle@1"
QUALITY = "Valid"

RESPONSE_VECTOR_FIELDS = (
    "force_B_N",
    "r_CoM_to_application_B_m",
    "moment_at_application_B_Nm",
)
RESPONSE_SCALAR_FIELDS = (
    "fuel_consumption_rate_kgps",
)
CLOSURE_VECTOR_FIELDS = (
    "force_B_N",
    "moment_at_application_B_Nm",
    "lever_arm_moment_B_Nm",
    "moment_about_CoM_B_Nm",
)
MASS_SCALAR_FIELDS = (
    "interval_duration_s",
    "fuel_consumption_rate_kgps",
    "consumed_fuel_mass_kg",
    "mass_delta_kg",
    "committed_mass_kg",
    "mass_candidate_kg",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite decimal value: {value}")
    return result


def vector(values: list[object], label: str) -> list[Decimal]:
    require(isinstance(values, list) and len(values) == 3,
            f"{label} must have three components")
    return [decimal(value) for value in values]


def add(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left + right for left, right in zip(lhs, rhs)]


def scale(values: list[Decimal], factor: Decimal) -> list[Decimal]:
    return [value * factor for value in values]


def cross(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [
        lhs[1] * rhs[2] - lhs[2] * rhs[1],
        lhs[2] * rhs[0] - lhs[0] * rhs[2],
        lhs[0] * rhs[1] - lhs[1] * rhs[0],
    ]


def dot(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    return sum((left * right for left, right in zip(lhs, rhs)), Decimal(0))


def max_difference(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    require(len(lhs) == len(rhs), "physical vectors have different lengths")
    return max((abs(left - right) for left, right in zip(lhs, rhs)),
               default=Decimal(0))


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


def valid_nonnegative_integer(value: object) -> bool:
    return (isinstance(value, int) and not isinstance(value, bool) and
            value >= 0)


def cases_by_id(cases: dict) -> dict[str, dict]:
    result = {case["id"]: case for case in cases["cases"]}
    require(len(result) == len(cases["cases"]),
            "duplicate propulsion-response case id")
    return result


def direction_is_unit(direction: list[Decimal], cases: dict) -> bool:
    norm = dot(direction, direction).sqrt()
    absolute = decimal(cases["tolerances"]["direction_norm_absolute"])
    relative = decimal(cases["tolerances"]["direction_norm_relative"])
    return abs(norm - Decimal(1)) <= (
        absolute + relative * max(abs(norm), Decimal(1)))


def validate_input(case: dict, cases: dict) -> None:
    context = case["context"]
    supplied = case["supplied_response"]
    mass = case["mass_consumer"]

    require(context["source_id"] == SOURCE_ID,
            "propulsion source identity differs")
    require(context["body_frame_id"] == BODY_FRAME_ID,
            "propulsion body frame differs")
    require(context["clock_domain"] == CLOCK_DOMAIN,
            "propulsion clock domain differs")
    require(valid_nonnegative_integer(context["sample_tick"]),
            "propulsion sample tick must be nonnegative")
    require(valid_nonnegative_integer(context["configuration_revision"]),
            "propulsion configuration revision must be nonnegative")
    require(valid_nonnegative_integer(context["valid_from_tick"]) and
            valid_nonnegative_integer(context["valid_until_tick"]),
            "propulsion interval ticks must be nonnegative")
    require(context["sample_tick"] == context["valid_from_tick"],
            "propulsion sample tick must equal interval start")
    require(context["valid_until_tick"] > context["valid_from_tick"],
            "propulsion interval must be nonempty")
    base_dt = decimal(context["base_dt_s"])
    require(base_dt > 0, "propulsion base dt must be positive")

    thrust = decimal(supplied["thrust_magnitude_N"])
    direction = vector(supplied["thrust_direction_B_unit"],
                       "thrust direction_B")
    vector(supplied["r_CoM_to_application_B_m"],
           "r_CoM_to_application_B")
    vector(supplied["intrinsic_moment_at_application_B_Nm"],
           "intrinsic moment_at_application_B")
    consumption_rate = decimal(supplied["fuel_consumption_rate_kgps"])
    require(thrust >= 0, "propulsion thrust magnitude must be nonnegative")
    require(direction_is_unit(direction, cases),
            "propulsion thrust direction must be unit length")
    require(consumption_rate >= 0,
            "propulsion fuel consumption rate must be nonnegative")

    require(mass["mass_state_id"] == MASS_STATE_ID,
            "propulsion mass state identity differs")
    for field in ("clock_domain", "configuration_revision",
                  "valid_from_tick", "valid_until_tick"):
        require(mass[field] == context[field],
                f"propulsion mass consumer {field} differs")
    committed_mass = decimal(mass["committed_mass_kg"])
    require(committed_mass > 0,
            "propulsion committed mass must be positive")


def evaluate_case(case: dict, cases: dict, *, reverse_thrust: bool = False,
                  pretransport_moment: bool = False,
                  mass_gain: bool = False) -> dict:
    validate_input(case, cases)
    context = case["context"]
    supplied = case["supplied_response"]
    mass_input = case["mass_consumer"]

    thrust = decimal(supplied["thrust_magnitude_N"])
    direction = vector(supplied["thrust_direction_B_unit"],
                       "thrust direction_B")
    force = scale(direction, -thrust if reverse_thrust else thrust)
    radius = vector(supplied["r_CoM_to_application_B_m"],
                    "r_CoM_to_application_B")
    intrinsic_moment = vector(
        supplied["intrinsic_moment_at_application_B_Nm"],
        "intrinsic moment_at_application_B")
    lever_arm_moment = cross(radius, force)
    closure_input_moment = (add(intrinsic_moment, lever_arm_moment)
                            if pretransport_moment else intrinsic_moment)
    moment_about_com = add(closure_input_moment, lever_arm_moment)

    base_dt = decimal(context["base_dt_s"])
    duration = (Decimal(context["valid_until_tick"] -
                        context["valid_from_tick"]) * base_dt)
    consumption_rate = decimal(supplied["fuel_consumption_rate_kgps"])
    consumed_mass = consumption_rate * duration
    mass_delta = consumed_mass if mass_gain else -consumed_mass
    committed_mass = decimal(mass_input["committed_mass_kg"])
    candidate_mass = committed_mass + mass_delta
    derived = (force + radius + intrinsic_moment + closure_input_moment +
               lever_arm_moment + moment_about_com +
               [duration, consumed_mass, mass_delta, candidate_mass])
    require(all(value.is_finite() for value in derived),
            "propulsion response produced a non-finite value")
    require(candidate_mass > 0,
            "propulsion mass candidate must be positive")

    response = {
        "model_id": MODEL_ID,
        "source_id": SOURCE_ID,
        "quality": QUALITY,
        "body_frame_id": BODY_FRAME_ID,
        "sample_tick": context["sample_tick"],
        "clock_domain": CLOCK_DOMAIN,
        "configuration_revision": context["configuration_revision"],
        "valid_from_tick": context["valid_from_tick"],
        "valid_until_tick": context["valid_until_tick"],
        "force_B_N": force,
        "r_CoM_to_application_B_m": radius,
        "moment_at_application_B_Nm": intrinsic_moment,
        "fuel_consumption_rate_kgps": consumption_rate,
    }
    closure_consumer = {
        "source_id": SOURCE_ID,
        "body_frame_id": BODY_FRAME_ID,
        "sample_tick": context["sample_tick"],
        "clock_domain": CLOCK_DOMAIN,
        "configuration_revision": context["configuration_revision"],
        "force_B_N": force,
        "moment_at_application_B_Nm": closure_input_moment,
        "lever_arm_moment_B_Nm": lever_arm_moment,
        "moment_about_CoM_B_Nm": moment_about_com,
    }
    mass_consumer = {
        "mass_state_id": MASS_STATE_ID,
        "clock_domain": CLOCK_DOMAIN,
        "configuration_revision": context["configuration_revision"],
        "valid_from_tick": context["valid_from_tick"],
        "valid_until_tick": context["valid_until_tick"],
        "interval_duration_s": duration,
        "fuel_consumption_rate_kgps": consumption_rate,
        "consumed_fuel_mass_kg": consumed_mass,
        "mass_delta_kg": mass_delta,
        "committed_mass_kg": committed_mass,
        "mass_candidate_kg": candidate_mass,
    }
    return {
        "id": case["id"],
        "response": response,
        "closure_consumer": closure_consumer,
        "mass_consumer": mass_consumer,
    }


def physical_values(result: dict) -> list[Decimal]:
    values: list[Decimal] = []
    response = result["response"]
    closure = result["closure_consumer"]
    mass = result["mass_consumer"]
    for field in RESPONSE_VECTOR_FIELDS:
        values.extend(decimal(item) for item in response[field])
    for field in RESPONSE_SCALAR_FIELDS:
        values.append(decimal(response[field]))
    for field in CLOSURE_VECTOR_FIELDS:
        values.extend(decimal(item) for item in closure[field])
    for field in MASS_SCALAR_FIELDS:
        values.append(decimal(mass[field]))
    return values


def split_interval_cases(cases: dict) -> tuple[dict, dict]:
    base = cases_by_id(cases)[
        "CASE-YYZ-PROPULSION-OFF-AXIS-CONSUMERS"]
    first = copy.deepcopy(base)
    first["context"]["valid_until_tick"] = 22
    first["mass_consumer"]["valid_until_tick"] = 22
    first_result = evaluate_case(first, cases)

    second = copy.deepcopy(base)
    second["context"]["sample_tick"] = 22
    second["context"]["valid_from_tick"] = 22
    second["mass_consumer"]["valid_from_tick"] = 22
    second["mass_consumer"]["committed_mass_kg"] = (
        first_result["mass_consumer"]["mass_candidate_kg"])
    return first, second


def reference_equivalence_results(cases: dict) -> list[dict]:
    base = cases_by_id(cases)[
        "CASE-YYZ-PROPULSION-OFF-AXIS-CONSUMERS"]
    whole = evaluate_case(base, cases)
    first_input, second_input = split_interval_cases(cases)
    first = evaluate_case(first_input, cases)
    second = evaluate_case(second_input, cases)

    response_fields = ("force_B_N", "r_CoM_to_application_B_m",
                       "moment_at_application_B_Nm")
    wrench_fields = ("force_B_N", "moment_at_application_B_Nm",
                     "lever_arm_moment_B_Nm", "moment_about_CoM_B_Nm")
    force_and_response_difference = max(
        (max_difference(
            [decimal(value) for value in whole["response"][field]],
            [decimal(value) for value in part["response"][field]])
         for part in (first, second) for field in response_fields),
        default=Decimal(0))
    wrench_difference = max(
        (max_difference(
            [decimal(value) for value in whole["closure_consumer"][field]],
            [decimal(value) for value in part["closure_consumer"][field]])
         for part in (first, second) for field in wrench_fields),
        default=Decimal(0))
    summed_consumption = (
        first["mass_consumer"]["consumed_fuel_mass_kg"] +
        second["mass_consumer"]["consumed_fuel_mass_kg"])
    consumption_difference = abs(
        summed_consumption -
        whole["mass_consumer"]["consumed_fuel_mass_kg"])
    final_mass_difference = abs(
        second["mass_consumer"]["mass_candidate_kg"] -
        whole["mass_consumer"]["mass_candidate_kg"])
    maximum = max(force_and_response_difference, wrench_difference,
                  consumption_difference, final_mass_difference)
    result = {
        "id": "EQUIV-YYZ-PROPULSION-MASS-INTERVAL-PARTITION",
        "status": "passed" if maximum <= Decimal("1e-68") else "failed",
        "force_and_response_max_abs_difference":
            force_and_response_difference,
        "application_wrench_max_abs_difference": wrench_difference,
        "summed_consumed_fuel_mass_kg": summed_consumption,
        "consumed_fuel_mass_difference_kg": consumption_difference,
        "sequential_final_mass_candidate_kg":
            second["mass_consumer"]["mass_candidate_kg"],
        "final_mass_candidate_difference_kg": final_mass_difference,
    }
    require(result["status"] == "passed",
            "Python propulsion interval partition failed")
    return [result]


def rejects(operation) -> bool:
    try:
        operation()
    except (ArithmeticError, KeyError, TypeError, ValueError):
        return True
    return False


def reference_invalid_rejections(cases: dict) -> list[str]:
    base = cases_by_id(cases)[
        "CASE-YYZ-PROPULSION-OFF-AXIS-CONSUMERS"]
    results: list[str] = []

    def add_mutation(identifier: str, mutate) -> None:
        value = copy.deepcopy(base)
        mutate(value)
        if rejects(lambda: evaluate_case(value, cases)):
            results.append(identifier)

    add_mutation(
        "INVALID-YYZ-PROPULSION-FRAME-MISMATCH",
        lambda value: value["context"].__setitem__(
            "body_frame_id", "frame.other@1"))
    add_mutation(
        "INVALID-YYZ-PROPULSION-CLOCK-MISMATCH",
        lambda value: value["context"].__setitem__(
            "clock_domain", "clock.other@1"))
    add_mutation(
        "INVALID-YYZ-PROPULSION-SAMPLE-INTERVAL-MISMATCH",
        lambda value: value["context"].__setitem__("sample_tick", 21))
    add_mutation(
        "INVALID-YYZ-PROPULSION-REVISION",
        lambda value: value["context"].__setitem__(
            "configuration_revision", -1))
    add_mutation(
        "INVALID-YYZ-PROPULSION-NONPOSITIVE-DT",
        lambda value: value["context"].__setitem__("base_dt_s", 0))
    add_mutation(
        "INVALID-YYZ-PROPULSION-NEGATIVE-THRUST",
        lambda value: value["supplied_response"].__setitem__(
            "thrust_magnitude_N", -1))
    add_mutation(
        "INVALID-YYZ-PROPULSION-NONUNIT-DIRECTION",
        lambda value: value["supplied_response"].__setitem__(
            "thrust_direction_B_unit", [2, 0, 0]))
    add_mutation(
        "INVALID-YYZ-PROPULSION-NONFINITE-MOMENT",
        lambda value: value["supplied_response"][
            "intrinsic_moment_at_application_B_Nm"].__setitem__(
                0, Decimal("Infinity")))
    add_mutation(
        "INVALID-YYZ-PROPULSION-NEGATIVE-CONSUMPTION",
        lambda value: value["supplied_response"].__setitem__(
            "fuel_consumption_rate_kgps", -1))
    add_mutation(
        "INVALID-YYZ-PROPULSION-DEPLETED-MASS",
        lambda value: value["mass_consumer"].__setitem__(
            "committed_mass_kg", Decimal("0.1")))
    return results


def reference_mutation_results(cases: dict) -> list[dict]:
    base = cases_by_id(cases)[
        "CASE-YYZ-PROPULSION-OFF-AXIS-CONSUMERS"]
    accepted = evaluate_case(base, cases)
    reverse = evaluate_case(base, cases, reverse_thrust=True)
    pretransport = evaluate_case(base, cases, pretransport_moment=True)
    mass_gain = evaluate_case(base, cases, mass_gain=True)
    profiles = [
        {
            "id": "MUTATION-YYZ-PROPULSION-REVERSED-THRUST-DIRECTION",
            "mutated": reverse,
            "observed_force_B_N": reverse["response"]["force_B_N"],
        },
        {
            "id": "MUTATION-YYZ-PROPULSION-PRETRANSPORTED-MOMENT",
            "mutated": pretransport,
            "observed_moment_at_application_B_Nm":
                pretransport["closure_consumer"][
                    "moment_at_application_B_Nm"],
            "observed_moment_about_CoM_B_Nm":
                pretransport["closure_consumer"]["moment_about_CoM_B_Nm"],
        },
        {
            "id": "MUTATION-YYZ-PROPULSION-MASS-GAIN",
            "mutated": mass_gain,
            "observed_mass_delta_kg":
                mass_gain["mass_consumer"]["mass_delta_kg"],
            "observed_mass_candidate_kg":
                mass_gain["mass_consumer"]["mass_candidate_kg"],
        },
    ]
    results = []
    for profile in profiles:
        mutated = profile.pop("mutated")
        difference = max_difference(physical_values(accepted),
                                    physical_values(mutated))
        results.append({
            "id": profile["id"],
            "status": "rejected" if difference > Decimal("1e-68")
            else "matched",
            "max_abs_physical_difference": difference,
            **{key: value for key, value in profile.items() if key != "id"},
        })
    require(all(result["status"] == "rejected" for result in results),
            "Python propulsion reference accepted a mutation")
    return results


def validate_exact_anchors(cases: dict) -> None:
    results = {
        case["id"]: evaluate_case(case, cases) for case in cases["cases"]
    }
    off_axis = results["CASE-YYZ-PROPULSION-OFF-AXIS-CONSUMERS"]
    require(off_axis["response"]["force_B_N"] ==
            [Decimal(300), Decimal(400), Decimal(0)],
            "off-axis propulsion force anchor differs")
    require(off_axis["closure_consumer"]["lever_arm_moment_B_Nm"] ==
            [Decimal(-40), Decimal(30), Decimal(-275)],
            "off-axis propulsion lever-arm anchor differs")
    require(off_axis["closure_consumer"]["moment_about_CoM_B_Nm"] ==
            [Decimal(-39), Decimal(28), Decimal(-272)],
            "off-axis propulsion total-moment anchor differs")
    require(off_axis["mass_consumer"]["consumed_fuel_mass_kg"] ==
            Decimal("0.25") and
            off_axis["mass_consumer"]["mass_candidate_kg"] ==
            Decimal("119.75"),
            "off-axis propulsion mass anchor differs")

    three_dimensional = results[
        "CASE-YYZ-PROPULSION-THREE-DIMENSIONAL-WRENCH"]
    require(three_dimensional["response"]["force_B_N"] ==
            [Decimal(40), Decimal(0), Decimal(30)] and
            three_dimensional["closure_consumer"][
                "lever_arm_moment_B_Nm"] ==
            [Decimal(-3), Decimal(6), Decimal(4)] and
            three_dimensional["closure_consumer"][
                "moment_about_CoM_B_Nm"] ==
            [Decimal(-7), Decimal(11), Decimal(-2)],
            "three-dimensional propulsion wrench anchor differs")

    zero = results["CASE-YYZ-PROPULSION-ZERO-RESPONSE"]
    require(all(value == 0 for value in
                zero["closure_consumer"]["moment_about_CoM_B_Nm"]) and
            zero["mass_consumer"]["mass_candidate_kg"] == Decimal(5),
            "zero propulsion response anchor differs")


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    validate_exact_anchors(cases)
    invalid = reference_invalid_rejections(cases)
    expected_invalid = [entry["id"] for entry in
                        cases["invalid_input_cases"]]
    require(invalid == expected_invalid,
            "Python propulsion invalid coverage differs")
    equivalence = reference_equivalence_results(cases)
    require([entry["id"] for entry in equivalence] ==
            [entry["id"] for entry in cases["equivalence_cases"]],
            "Python propulsion equivalence coverage differs")
    mutations = reference_mutation_results(cases)
    require([entry["id"] for entry in mutations] ==
            [entry["id"] for entry in cases["mutation_cases"]],
            "Python propulsion mutation coverage differs")
    return stringify({
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "status": "executable",
        "precision": {"decimal_digits": getcontext().prec},
        "input_identity": {
            "path": "fixtures/ref-yyz-propulsion-response/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": {
            case["id"]: evaluate_case(case, cases)
            for case in cases["cases"]
        },
        "equivalence_results": equivalence,
        "invalid_input_rejections": invalid,
        "mutation_results": mutations,
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
            f"C++ propulsion-response probe failed: "
            f"{completed.stderr.strip()}")
    return completed.stdout, json.loads(
        completed.stdout, parse_float=Decimal)


def compare_case(checks: Checks, actual: dict, expected: dict,
                 absolute: Decimal, relative: Decimal) -> None:
    case_id = expected["id"]
    checks.require(actual["id"] == case_id,
                   f"C++ propulsion case id differs for {case_id}")
    for section, exact_fields, vector_fields, scalar_fields in (
        ("response",
         ("model_id", "source_id", "quality", "body_frame_id",
          "sample_tick", "clock_domain", "configuration_revision",
          "valid_from_tick", "valid_until_tick"),
         RESPONSE_VECTOR_FIELDS, RESPONSE_SCALAR_FIELDS),
        ("closure_consumer",
         ("source_id", "body_frame_id", "sample_tick", "clock_domain",
          "configuration_revision"),
         CLOSURE_VECTOR_FIELDS, ()),
        ("mass_consumer",
         ("mass_state_id", "clock_domain", "configuration_revision",
          "valid_from_tick", "valid_until_tick"),
         (), MASS_SCALAR_FIELDS),
    ):
        actual_section = actual[section]
        expected_section = expected[section]
        for field in exact_fields:
            checks.require(actual_section[field] == expected_section[field],
                           f"{case_id}.{section}.{field} differs")
        for field in vector_fields:
            compare_vector(checks, actual_section[field],
                           expected_section[field], absolute, relative,
                           f"{case_id}.{section}.{field}")
        for field in scalar_fields:
            compare_scalar(checks, actual_section[field],
                           expected_section[field], absolute, relative,
                           f"{case_id}.{section}.{field}")


def compare_equivalence(checks: Checks, actual: list, expected: list,
                        absolute: Decimal, relative: Decimal) -> None:
    checks.require(len(actual) == len(expected),
                   "C++ propulsion equivalence count differs")
    scalar_fields = (
        "force_and_response_max_abs_difference",
        "application_wrench_max_abs_difference",
        "summed_consumed_fuel_mass_kg",
        "consumed_fuel_mass_difference_kg",
        "sequential_final_mass_candidate_kg",
        "final_mass_candidate_difference_kg",
    )
    for actual_entry, expected_entry in zip(actual, expected):
        checks.require(actual_entry["id"] == expected_entry["id"] and
                       actual_entry["status"] == expected_entry["status"],
                       "C++ propulsion equivalence identity differs", 2)
        for field in scalar_fields:
            compare_scalar(checks, actual_entry[field],
                           expected_entry[field], absolute, relative,
                           f"{expected_entry['id']}.{field}")


def compare_mutations(checks: Checks, actual: list, expected: list,
                      absolute: Decimal, relative: Decimal) -> None:
    checks.require(len(actual) == len(expected),
                   "C++ propulsion mutation count differs")
    actual_by_id = {entry["id"]: entry for entry in actual}
    checks.require(len(actual_by_id) == len(actual),
                   "C++ propulsion mutation ids repeat")
    for expected_entry in expected:
        identifier = expected_entry["id"]
        checks.require(identifier in actual_by_id,
                       f"C++ propulsion mutation is missing: {identifier}")
        actual_entry = actual_by_id[identifier]
        checks.require(actual_entry["status"] == expected_entry["status"],
                       f"{identifier}.status differs")
        compare_scalar(checks,
                       actual_entry["max_abs_physical_difference"],
                       expected_entry["max_abs_physical_difference"],
                       absolute, relative,
                       f"{identifier}.max_abs_physical_difference")
        for field in expected_entry:
            if field in ("id", "status", "max_abs_physical_difference"):
                continue
            if isinstance(expected_entry[field], list):
                compare_vector(checks, actual_entry[field],
                               expected_entry[field], absolute, relative,
                               f"{identifier}.{field}")
            else:
                compare_scalar(checks, actual_entry[field],
                               expected_entry[field], absolute, relative,
                               f"{identifier}.{field}")


def verify_reference(cases: dict, raw_cases: bytes, oracle: dict,
                     probe_path: Path) -> dict:
    checks = Checks()
    checks.require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
                   "propulsion fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "propulsion oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "propulsion model identity differs", 2)
    checks.require(oracle["precision"]["decimal_digits"] >= 70,
                   "propulsion reference precision is below 70 digits")
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "propulsion input byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-propulsion-response/cases.json",
                   "propulsion input path differs")

    recomputed = build_reference(cases, raw_cases)
    checks.require(oracle == recomputed,
                   "stored propulsion oracle differs from its producer",
                   len(oracle["cases"]) + 4)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ propulsion probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed" and
                   probe["model_choice_status"] ==
                   cases["model_choice"]["status"],
                   "C++ propulsion probe identity differs", 4)

    probe_cases = {entry["id"]: entry for entry in probe["cases"]}
    checks.require(len(probe_cases) == len(probe["cases"]) ==
                   len(oracle["cases"]),
                   "C++ propulsion cases are incomplete", 2)
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    for case_id, expected in oracle["cases"].items():
        checks.require(case_id in probe_cases,
                       f"C++ propulsion case is missing: {case_id}")
        compare_case(checks, probe_cases[case_id], expected,
                     absolute, relative)

    compare_equivalence(checks, probe["equivalence_results"],
                        oracle["equivalence_results"], absolute, relative)
    expected_invalid = {
        entry["id"] for entry in cases["invalid_input_cases"]
    }
    checks.require(set(probe["invalid_input_rejections"]) == expected_invalid,
                   "C++ propulsion invalid identities differ")
    compare_mutations(checks, probe["mutation_results"],
                      oracle["mutation_results"], absolute, relative)

    linked = probe_cases["CASE-YYZ-PROPULSION-OFF-AXIS-CONSUMERS"]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "case_count": len(probe_cases),
        "off_axis_force_B_N": linked["response"]["force_B_N"],
        "off_axis_lever_arm_moment_B_Nm":
            linked["closure_consumer"]["lever_arm_moment_B_Nm"],
        "off_axis_moment_about_CoM_B_Nm":
            linked["closure_consumer"]["moment_about_CoM_B_Nm"],
        "off_axis_consumed_fuel_mass_kg":
            linked["mass_consumer"]["consumed_fuel_mass_kg"],
        "off_axis_mass_candidate_kg":
            linked["mass_consumer"]["mass_candidate_kg"],
        "equivalence_cases_passed": len(oracle["equivalence_results"]),
        "invalid_input_cases_rejected": len(expected_invalid),
        "mutation_cases_rejected": len(oracle["mutation_results"]),
    })


def validate_cases_identity(cases: dict) -> None:
    require(cases["schema_version"] == CASES_SCHEMA and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model"]["model_id"] == MODEL_ID,
            "propulsion cases identity differs")
    require(cases["model_choice"]["status"] == "accepted",
            "propulsion model choice is not accepted")
    model = cases["model"]
    require(model["source_id"] == SOURCE_ID and
            model["body_frame_id"] == BODY_FRAME_ID and
            model["clock_domain"] == CLOCK_DOMAIN and
            model["mass_state_id"] == MASS_STATE_ID and
            model["quality"] == QUALITY,
            "propulsion model profile differs")


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
        print(f"YYZ propulsion response reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

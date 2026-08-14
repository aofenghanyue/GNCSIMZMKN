#!/usr/bin/env python3
"""Independent Decimal reference for YYZ MassProperties projection."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from decimal import Decimal, getcontext
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-MASS-PROPERTIES-001"
ORACLE_ID = "ORACLE-YYZ-MASS-PROPERTIES-001"
MODEL_ID = "MODEL-YYZ-MASS-PROPERTIES-PROJECTION-001"
CASES_SCHEMA = "gnczmkn.yyz-mass-properties-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-mass-properties-reference/1"
PROFILE_STATUS = "implemented-from-accepted-invariants"
MASS_STATE_ID = "mass.fixture.yyz.vehicle@1"
BODY_FRAME_ID = "frame.fixture.yyz.body@1"
BODY_ORIGIN_POINT_ID = "point.fixture.yyz.body-origin@1"
COM_POINT_ID = "point.fixture.yyz.center-of-mass@1"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
QUALITY = "Valid"

SAMPLE_EXACT_FIELDS = (
    "model_id",
    "quality",
    "mass_state_id",
    "body_frame_id",
    "body_origin_point_id",
    "center_of_mass_point_id",
    "sample_tick",
    "clock_domain",
    "configuration_revision",
    "valid_from_tick",
    "valid_until_tick",
)
SAMPLE_SCALAR_FIELDS = ("mass_kg",)
SAMPLE_VECTOR_FIELDS = (
    "r_body_origin_to_CoM_B_m",
    "inertia_about_CoM_B_kgm2_row_major",
)
CLOSURE_EXACT_FIELDS = (
    "body_frame_id",
    "sample_tick",
    "clock_domain",
    "configuration_revision",
    "application_point_id",
)
CLOSURE_VECTOR_FIELDS = (
    "r_body_origin_to_application_B_m",
    "r_CoM_to_application_B_m",
    "force_B_N",
    "moment_at_application_B_Nm",
    "lever_arm_moment_B_Nm",
    "moment_about_CoM_B_Nm",
)
RIGID_EXACT_FIELDS = (
    "body_frame_id",
    "sample_tick",
    "clock_domain",
    "configuration_revision",
)
RIGID_SCALAR_FIELDS = ("mass_kg", "mass_reciprocal_per_kg")
RIGID_VECTOR_FIELDS = (
    "force_B_N",
    "specific_force_B_mps2",
    "inertia_about_CoM_B_kgm2_row_major",
    "omega_BI_B_radps",
    "angular_momentum_B_kgm2ps",
    "gyroscopic_moment_B_Nm",
    "net_moment_B_Nm",
    "angular_acceleration_B_radps2",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), f"non-finite decimal value: {value}")
    return result


def vector(values: list[object], label: str,
           length: int = 3) -> list[Decimal]:
    require(isinstance(values, list) and len(values) == length,
            f"{label} must have {length} components")
    return [decimal(value) for value in values]


def add(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left + right for left, right in zip(lhs, rhs)]


def subtract(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left - right for left, right in zip(lhs, rhs)]


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


def matrix(values: list[object], label: str) -> list[list[Decimal]]:
    parsed = vector(values, label, 9)
    return [parsed[0:3], parsed[3:6], parsed[6:9]]


def flatten(value: list[list[Decimal]]) -> list[Decimal]:
    return [entry for row in value for entry in row]


def matrix_vector(product: list[list[Decimal]],
                  value: list[Decimal]) -> list[Decimal]:
    return [dot(row, value) for row in product]


def cholesky(inertia: list[list[Decimal]]) -> list[list[Decimal]]:
    require(all(inertia[row][column] == inertia[column][row]
                for row in range(3) for column in range(3)),
            "mass-properties inertia must be symmetric")
    lower = [[Decimal(0) for _ in range(3)] for _ in range(3)]
    for row in range(3):
        for column in range(row + 1):
            residual = inertia[row][column] - sum(
                (lower[row][index] * lower[column][index]
                 for index in range(column)), Decimal(0))
            if row == column:
                require(residual > 0,
                        "mass-properties inertia must be positive definite")
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
            "duplicate mass-properties case id")
    return result


def validate_context(context: dict, label: str) -> None:
    require(context["mass_state_id"] == MASS_STATE_ID,
            f"{label} mass state identity differs")
    require(context["body_frame_id"] == BODY_FRAME_ID,
            f"{label} body frame differs")
    require(context["clock_domain"] == CLOCK_DOMAIN,
            f"{label} clock domain differs")
    require(valid_nonnegative_integer(context["sample_tick"]),
            f"{label} sample tick must be nonnegative")
    require(valid_nonnegative_integer(context["configuration_revision"]),
            f"{label} configuration revision must be nonnegative")
    require(valid_nonnegative_integer(context["valid_from_tick"]) and
            valid_nonnegative_integer(context["valid_until_tick"]),
            f"{label} interval ticks must be nonnegative")
    require(context["sample_tick"] == context["valid_from_tick"],
            f"{label} sample tick must equal interval start")
    require(context["valid_until_tick"] > context["valid_from_tick"],
            f"{label} interval must be nonempty")


def validate_state(state: dict, label: str) -> None:
    mass = decimal(state["mass_kg"])
    require(mass > 0, f"{label} mass must be positive")
    vector(state["r_body_origin_to_CoM_B_m"], f"{label} CoM")
    inertia = matrix(state["inertia_about_CoM_B_kgm2_row_major"],
                     f"{label} inertia")
    cholesky(inertia)


def validate_input(case: dict) -> None:
    current_context = case["current_context"]
    next_context = case["next_context"]
    validate_context(current_context, "current MassProperties")
    validate_context(next_context, "next MassProperties")
    validate_state(case["current_committed_state"], "current MassState")
    validate_state(case["explicit_next_committed_state"],
                   "next MassState")
    require(next_context["configuration_revision"] ==
            current_context["configuration_revision"],
            "projection-only case changes configuration revision")

    pending = case["pending_mass_candidate"]
    require(isinstance(pending["source_interval_id"], str) and
            bool(pending["source_interval_id"]),
            "pending mass candidate source identity is empty")
    require(pending["visibility_before_commit"] == "candidate-only",
            "pending mass candidate visibility differs")
    require(valid_nonnegative_integer(pending["next_commit_tick"]) and
            pending["next_commit_tick"] ==
            current_context["valid_until_tick"] ==
            next_context["sample_tick"],
            "pending mass candidate commit tick differs")
    candidate_mass = decimal(pending["mass_candidate_kg"])
    require(candidate_mass > 0,
            "pending mass candidate must be positive")
    require(candidate_mass == decimal(
        case["explicit_next_committed_state"]["mass_kg"]),
        "pending candidate and next committed mass differ")

    closure = case["closure_probe"]
    require(isinstance(closure["application_point_id"], str) and
            bool(closure["application_point_id"]),
            "Closure application point identity is empty")
    vector(closure["r_body_origin_to_application_B_m"],
           "Closure application point")
    vector(closure["force_B_N"], "Closure force_B")
    vector(closure["intrinsic_moment_at_application_B_Nm"],
           "Closure moment_at_application_B")
    vector(case["rigid_core_probe"]["omega_BI_B_radps"],
           "rigid-core omega_BI_B")


def project_state(context: dict, state: dict) -> dict:
    validate_context(context, "projected MassProperties")
    validate_state(state, "projected MassState")
    return {
        "model_id": MODEL_ID,
        "quality": QUALITY,
        "mass_state_id": MASS_STATE_ID,
        "body_frame_id": BODY_FRAME_ID,
        "body_origin_point_id": BODY_ORIGIN_POINT_ID,
        "center_of_mass_point_id": COM_POINT_ID,
        "sample_tick": context["sample_tick"],
        "clock_domain": CLOCK_DOMAIN,
        "configuration_revision": context["configuration_revision"],
        "valid_from_tick": context["valid_from_tick"],
        "valid_until_tick": context["valid_until_tick"],
        "mass_kg": decimal(state["mass_kg"]),
        "r_body_origin_to_CoM_B_m": vector(
            state["r_body_origin_to_CoM_B_m"], "projected CoM"),
        "inertia_about_CoM_B_kgm2_row_major": vector(
            state["inertia_about_CoM_B_kgm2_row_major"],
            "projected inertia", 9),
    }


def evaluate_case(case: dict, *, early_candidate_visibility: bool = False,
                  omit_com_offset: bool = False,
                  diagonalize_inertia: bool = False) -> dict:
    validate_input(case)
    current_context = case["current_context"]
    current_state = copy.deepcopy(case["current_committed_state"])
    pending = case["pending_mass_candidate"]
    if early_candidate_visibility:
        current_state["mass_kg"] = pending["mass_candidate_kg"]
    current_sample = project_state(current_context, current_state)
    next_sample = project_state(case["next_context"],
                                case["explicit_next_committed_state"])

    closure_input = case["closure_probe"]
    application = vector(
        closure_input["r_body_origin_to_application_B_m"],
        "Closure application point")
    com = [decimal(value) for value in
           current_sample["r_body_origin_to_CoM_B_m"]]
    radius = application if omit_com_offset else subtract(application, com)
    force = vector(closure_input["force_B_N"], "Closure force_B")
    intrinsic_moment = vector(
        closure_input["intrinsic_moment_at_application_B_Nm"],
        "Closure moment_at_application_B")
    lever_arm_moment = cross(radius, force)
    moment_about_com = add(intrinsic_moment, lever_arm_moment)
    closure_consumer = {
        "body_frame_id": BODY_FRAME_ID,
        "sample_tick": current_context["sample_tick"],
        "clock_domain": CLOCK_DOMAIN,
        "configuration_revision": current_context[
            "configuration_revision"],
        "application_point_id": closure_input["application_point_id"],
        "r_body_origin_to_application_B_m": application,
        "r_CoM_to_application_B_m": radius,
        "force_B_N": force,
        "moment_at_application_B_Nm": intrinsic_moment,
        "lever_arm_moment_B_Nm": lever_arm_moment,
        "moment_about_CoM_B_Nm": moment_about_com,
    }

    mass = decimal(current_sample["mass_kg"])
    inertia = matrix(
        current_sample["inertia_about_CoM_B_kgm2_row_major"],
        "rigid-core inertia")
    consumer_inertia = copy.deepcopy(inertia)
    if diagonalize_inertia:
        for row in range(3):
            for column in range(3):
                if row != column:
                    consumer_inertia[row][column] = Decimal(0)
    omega = vector(case["rigid_core_probe"]["omega_BI_B_radps"],
                   "rigid-core omega_BI_B")
    mass_reciprocal = Decimal(1) / mass
    specific_force = scale(force, mass_reciprocal)
    angular_momentum = matrix_vector(consumer_inertia, omega)
    gyroscopic_moment = cross(omega, angular_momentum)
    net_moment = subtract(moment_about_com, gyroscopic_moment)
    angular_acceleration = solve_spd(consumer_inertia, net_moment)
    derived = (radius + lever_arm_moment + moment_about_com +
               specific_force + angular_momentum + gyroscopic_moment +
               net_moment + angular_acceleration)
    require(all(value.is_finite() for value in derived),
            "MassProperties consumer produced a non-finite value")
    rigid_core_consumer = {
        "body_frame_id": BODY_FRAME_ID,
        "sample_tick": current_context["sample_tick"],
        "clock_domain": CLOCK_DOMAIN,
        "configuration_revision": current_context[
            "configuration_revision"],
        "mass_kg": mass,
        "mass_reciprocal_per_kg": mass_reciprocal,
        "force_B_N": force,
        "specific_force_B_mps2": specific_force,
        "inertia_about_CoM_B_kgm2_row_major": flatten(consumer_inertia),
        "omega_BI_B_radps": omega,
        "angular_momentum_B_kgm2ps": angular_momentum,
        "gyroscopic_moment_B_Nm": gyroscopic_moment,
        "net_moment_B_Nm": net_moment,
        "angular_acceleration_B_radps2": angular_acceleration,
    }
    publication_sequence = {
        "current_sample_tick": current_context["sample_tick"],
        "current_visible_mass_kg": mass,
        "pending_source_interval_id": pending["source_interval_id"],
        "pending_visibility_before_commit":
            pending["visibility_before_commit"],
        "pending_mass_candidate_kg": decimal(
            pending["mass_candidate_kg"]),
        "next_commit_tick": pending["next_commit_tick"],
        "next_visible_mass_kg": next_sample["mass_kg"],
        "next_sample": next_sample,
    }
    return {
        "id": case["id"],
        "current_sample": current_sample,
        "closure_consumer": closure_consumer,
        "rigid_core_consumer": rigid_core_consumer,
        "publication_sequence": publication_sequence,
    }


def physical_values(result: dict) -> list[Decimal]:
    values: list[Decimal] = []
    sample = result["current_sample"]
    values.append(decimal(sample["mass_kg"]))
    values.extend(decimal(value) for value in
                  sample["r_body_origin_to_CoM_B_m"])
    values.extend(decimal(value) for value in
                  sample["inertia_about_CoM_B_kgm2_row_major"])
    closure = result["closure_consumer"]
    for field in CLOSURE_VECTOR_FIELDS:
        values.extend(decimal(value) for value in closure[field])
    rigid = result["rigid_core_consumer"]
    for field in RIGID_SCALAR_FIELDS:
        values.append(decimal(rigid[field]))
    for field in RIGID_VECTOR_FIELDS:
        values.extend(decimal(value) for value in rigid[field])
    publication = result["publication_sequence"]
    for field in ("current_visible_mass_kg", "pending_mass_candidate_kg",
                  "next_visible_mass_kg"):
        values.append(decimal(publication[field]))
    return values


def consumer_invariant_values(result: dict) -> list[Decimal]:
    values: list[Decimal] = []
    sample = result["current_sample"]
    values.append(decimal(sample["mass_kg"]))
    values.extend(decimal(value) for value in
                  sample["inertia_about_CoM_B_kgm2_row_major"])
    closure = result["closure_consumer"]
    for field in ("r_CoM_to_application_B_m", "force_B_N",
                  "moment_at_application_B_Nm", "lever_arm_moment_B_Nm",
                  "moment_about_CoM_B_Nm"):
        values.extend(decimal(value) for value in closure[field])
    rigid = result["rigid_core_consumer"]
    for field in RIGID_SCALAR_FIELDS:
        values.append(decimal(rigid[field]))
    for field in RIGID_VECTOR_FIELDS:
        values.extend(decimal(value) for value in rigid[field])
    return values


def reference_equivalence_results(cases: dict) -> list[dict]:
    base = cases_by_id(cases)[
        "CASE-YYZ-MASS-PROPERTIES-CURRENT-CANDIDATE-PUBLICATION"]
    accepted = evaluate_case(base)
    shifted_input = copy.deepcopy(base)
    offset = [Decimal(10), Decimal(-5), Decimal(2)]
    shifted_input["current_committed_state"][
        "r_body_origin_to_CoM_B_m"] = add(
            vector(base["current_committed_state"][
                "r_body_origin_to_CoM_B_m"], "base CoM"), offset)
    shifted_input["closure_probe"][
        "r_body_origin_to_application_B_m"] = add(
            vector(base["closure_probe"][
                "r_body_origin_to_application_B_m"],
                "base application point"), offset)
    shifted = evaluate_case(shifted_input)
    difference = max_difference(consumer_invariant_values(accepted),
                                consumer_invariant_values(shifted))
    result = {
        "id": "EQUIV-YYZ-MASS-PROPERTIES-BODY-ORIGIN-TRANSLATION",
        "status": "passed" if difference <= Decimal("1e-68")
        else "failed",
        "translation_B_m": offset,
        "shifted_r_body_origin_to_CoM_B_m":
            shifted["current_sample"]["r_body_origin_to_CoM_B_m"],
        "max_abs_consumer_difference": difference,
    }
    require(result["status"] == "passed",
            "Python MassProperties origin translation failed")
    return [result]


def rejects(operation) -> bool:
    try:
        operation()
    except (ArithmeticError, KeyError, TypeError, ValueError):
        return True
    return False


def reference_invalid_rejections(cases: dict) -> list[str]:
    base = cases_by_id(cases)[
        "CASE-YYZ-MASS-PROPERTIES-CURRENT-CANDIDATE-PUBLICATION"]
    results: list[str] = []

    def add_mutation(identifier: str, mutate) -> None:
        value = copy.deepcopy(base)
        mutate(value)
        if rejects(lambda: evaluate_case(value)):
            results.append(identifier)

    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-STATE-IDENTITY",
        lambda value: value["current_context"].__setitem__(
            "mass_state_id", "mass.other@1"))
    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-FRAME-MISMATCH",
        lambda value: value["current_context"].__setitem__(
            "body_frame_id", "frame.other@1"))
    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-CLOCK-MISMATCH",
        lambda value: value["current_context"].__setitem__(
            "clock_domain", "clock.other@1"))
    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-NEGATIVE-REVISION",
        lambda value: value["current_context"].__setitem__(
            "configuration_revision", -1))
    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-SAMPLE-INTERVAL-MISMATCH",
        lambda value: value["current_context"].__setitem__(
            "sample_tick", 21))
    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-NONPOSITIVE-MASS",
        lambda value: value["current_committed_state"].__setitem__(
            "mass_kg", 0))
    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-NONFINITE-COM",
        lambda value: value["current_committed_state"][
            "r_body_origin_to_CoM_B_m"].__setitem__(
                0, Decimal("Infinity")))
    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-ASYMMETRIC-INERTIA",
        lambda value: value["current_committed_state"][
            "inertia_about_CoM_B_kgm2_row_major"].__setitem__(1, 2))
    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-NON-SPD-INERTIA",
        lambda value: value["current_committed_state"][
            "inertia_about_CoM_B_kgm2_row_major"].__setitem__(0, -1))
    add_mutation(
        "INVALID-YYZ-MASS-PROPERTIES-CANDIDATE-COMMIT-MISMATCH",
        lambda value: value["pending_mass_candidate"].__setitem__(
            "mass_candidate_kg", Decimal("119.5")))
    return results


def reference_mutation_results(cases: dict) -> list[dict]:
    base = cases_by_id(cases)[
        "CASE-YYZ-MASS-PROPERTIES-CURRENT-CANDIDATE-PUBLICATION"]
    accepted = evaluate_case(base)
    early = evaluate_case(base, early_candidate_visibility=True)
    omitted = evaluate_case(base, omit_com_offset=True)
    diagonalized = evaluate_case(base, diagonalize_inertia=True)
    profiles = [
        {
            "id": "MUTATION-YYZ-MASS-PROPERTIES-EARLY-CANDIDATE-VISIBILITY",
            "mutated": early,
            "observed_current_visible_mass_kg":
                early["publication_sequence"]["current_visible_mass_kg"],
            "observed_specific_force_B_mps2":
                early["rigid_core_consumer"]["specific_force_B_mps2"],
        },
        {
            "id": "MUTATION-YYZ-MASS-PROPERTIES-OMIT-COM-OFFSET",
            "mutated": omitted,
            "observed_r_CoM_to_application_B_m":
                omitted["closure_consumer"][
                    "r_CoM_to_application_B_m"],
            "observed_moment_about_CoM_B_Nm":
                omitted["closure_consumer"]["moment_about_CoM_B_Nm"],
        },
        {
            "id": "MUTATION-YYZ-MASS-PROPERTIES-DIAGONALIZE-INERTIA",
            "mutated": diagonalized,
            "observed_angular_momentum_B_kgm2ps":
                diagonalized["rigid_core_consumer"][
                    "angular_momentum_B_kgm2ps"],
            "observed_gyroscopic_moment_B_Nm":
                diagonalized["rigid_core_consumer"][
                    "gyroscopic_moment_B_Nm"],
            "observed_angular_acceleration_B_radps2":
                diagonalized["rigid_core_consumer"][
                    "angular_acceleration_B_radps2"],
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
            "Python MassProperties reference accepted a mutation")
    return results


def validate_exact_anchors(cases: dict) -> None:
    results = {
        case["id"]: evaluate_case(case) for case in cases["cases"]
    }
    current = results[
        "CASE-YYZ-MASS-PROPERTIES-CURRENT-CANDIDATE-PUBLICATION"]
    require(current["current_sample"]["mass_kg"] == Decimal(120) and
            current["publication_sequence"]["next_visible_mass_kg"] ==
            Decimal("119.75"),
            "MassProperties publication anchor differs")
    require(current["closure_consumer"]["r_CoM_to_application_B_m"] ==
            [Decimal("0.8"), Decimal("0.5"), Decimal("-0.25")] and
            current["closure_consumer"]["lever_arm_moment_B_Nm"] ==
            [Decimal(100), Decimal(-75), Decimal(170)] and
            current["closure_consumer"]["moment_about_CoM_B_Nm"] ==
            [Decimal(101), Decimal(-77), Decimal(173)],
            "MassProperties Closure anchor differs")
    require(current["rigid_core_consumer"][
                "angular_momentum_B_kgm2ps"] ==
            [Decimal("15.5"), Decimal(47), Decimal("94.5")] and
            current["rigid_core_consumer"][
                "gyroscopic_moment_B_Nm"] ==
            [Decimal(48), Decimal(-48), Decimal(16)] and
            current["rigid_core_consumer"]["net_moment_B_Nm"] ==
            [Decimal(53), Decimal(-29), Decimal(157)],
            "MassProperties full-inertia anchor differs")

    diagonal = results[
        "CASE-YYZ-MASS-PROPERTIES-DIAGONAL-ZERO-FLOW"]
    require(diagonal["closure_consumer"]["r_CoM_to_application_B_m"] ==
            [Decimal("0.7"), Decimal("-0.35"), Decimal("0.2")] and
            diagonal["closure_consumer"]["moment_about_CoM_B_Nm"] ==
            [Decimal("-14.5"), Decimal(-8), Decimal(8)] and
            diagonal["rigid_core_consumer"][
                "gyroscopic_moment_B_Nm"] ==
            [Decimal("-0.03"), Decimal("-0.12"), Decimal("-0.02")],
            "diagonal MassProperties anchor differs")


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    validate_exact_anchors(cases)
    invalid = reference_invalid_rejections(cases)
    expected_invalid = [entry["id"] for entry in
                        cases["invalid_input_cases"]]
    require(invalid == expected_invalid,
            "Python MassProperties invalid coverage differs")
    equivalence = reference_equivalence_results(cases)
    require([entry["id"] for entry in equivalence] ==
            [entry["id"] for entry in cases["equivalence_cases"]],
            "Python MassProperties equivalence coverage differs")
    mutations = reference_mutation_results(cases)
    require([entry["id"] for entry in mutations] ==
            [entry["id"] for entry in cases["mutation_cases"]],
            "Python MassProperties mutation coverage differs")
    return stringify({
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "status": "executable",
        "precision": {"decimal_digits": getcontext().prec},
        "input_identity": {
            "path": "fixtures/ref-yyz-mass-properties/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": {
            case["id"]: evaluate_case(case) for case in cases["cases"]
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
        [str(path), "--self-check"], check=False, capture_output=True,
        text=True, encoding="utf-8")
    require(completed.returncode == 0,
            f"C++ MassProperties probe failed: {completed.stderr.strip()}")
    return completed.stdout, json.loads(
        completed.stdout, parse_float=Decimal)


def compare_sample(checks: Checks, actual: dict, expected: dict,
                   absolute: Decimal, relative: Decimal,
                   label: str) -> None:
    for field in SAMPLE_EXACT_FIELDS:
        checks.require(actual[field] == expected[field],
                       f"{label}.{field} differs")
    for field in SAMPLE_SCALAR_FIELDS:
        compare_scalar(checks, actual[field], expected[field],
                       absolute, relative, f"{label}.{field}")
    for field in SAMPLE_VECTOR_FIELDS:
        compare_vector(checks, actual[field], expected[field],
                       absolute, relative, f"{label}.{field}")


def compare_case(checks: Checks, actual: dict, expected: dict,
                 absolute: Decimal, relative: Decimal) -> None:
    case_id = expected["id"]
    checks.require(actual["id"] == case_id,
                   f"C++ MassProperties case id differs for {case_id}")
    compare_sample(checks, actual["current_sample"],
                   expected["current_sample"], absolute, relative,
                   f"{case_id}.current_sample")
    for section, exact_fields, scalar_fields, vector_fields in (
        ("closure_consumer", CLOSURE_EXACT_FIELDS, (),
         CLOSURE_VECTOR_FIELDS),
        ("rigid_core_consumer", RIGID_EXACT_FIELDS,
         RIGID_SCALAR_FIELDS, RIGID_VECTOR_FIELDS),
    ):
        actual_section = actual[section]
        expected_section = expected[section]
        for field in exact_fields:
            checks.require(actual_section[field] == expected_section[field],
                           f"{case_id}.{section}.{field} differs")
        for field in scalar_fields:
            compare_scalar(checks, actual_section[field],
                           expected_section[field], absolute, relative,
                           f"{case_id}.{section}.{field}")
        for field in vector_fields:
            compare_vector(checks, actual_section[field],
                           expected_section[field], absolute, relative,
                           f"{case_id}.{section}.{field}")

    actual_publication = actual["publication_sequence"]
    expected_publication = expected["publication_sequence"]
    for field in ("current_sample_tick", "pending_source_interval_id",
                  "pending_visibility_before_commit", "next_commit_tick"):
        checks.require(actual_publication[field] ==
                       expected_publication[field],
                       f"{case_id}.publication_sequence.{field} differs")
    for field in ("current_visible_mass_kg", "pending_mass_candidate_kg",
                  "next_visible_mass_kg"):
        compare_scalar(checks, actual_publication[field],
                       expected_publication[field], absolute, relative,
                       f"{case_id}.publication_sequence.{field}")
    compare_sample(checks, actual_publication["next_sample"],
                   expected_publication["next_sample"], absolute, relative,
                   f"{case_id}.publication_sequence.next_sample")


def compare_generic_results(checks: Checks, actual: list, expected: list,
                            absolute: Decimal, relative: Decimal,
                            label: str) -> None:
    checks.require(len(actual) == len(expected), f"{label} count differs")
    actual_by_id = {entry["id"]: entry for entry in actual}
    checks.require(len(actual_by_id) == len(actual),
                   f"{label} ids repeat")
    for expected_entry in expected:
        identifier = expected_entry["id"]
        checks.require(identifier in actual_by_id,
                       f"{label} is missing: {identifier}")
        actual_entry = actual_by_id[identifier]
        for field, expected_value in expected_entry.items():
            if field == "id" or isinstance(expected_value, str) and field == "status":
                checks.require(actual_entry[field] == expected_value,
                               f"{identifier}.{field} differs")
            elif isinstance(expected_value, list):
                compare_vector(checks, actual_entry[field], expected_value,
                               absolute, relative,
                               f"{identifier}.{field}")
            elif isinstance(expected_value, str) and field.startswith(
                    "observed_"):
                compare_scalar(checks, actual_entry[field], expected_value,
                               absolute, relative,
                               f"{identifier}.{field}")
            elif field == "status":
                checks.require(actual_entry[field] == expected_value,
                               f"{identifier}.{field} differs")
            else:
                compare_scalar(checks, actual_entry[field], expected_value,
                               absolute, relative,
                               f"{identifier}.{field}")


def verify_reference(cases: dict, raw_cases: bytes, oracle: dict,
                     probe_path: Path) -> dict:
    checks = Checks()
    checks.require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
                   "MassProperties fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "MassProperties oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "MassProperties model identity differs", 2)
    checks.require(oracle["precision"]["decimal_digits"] >= 70,
                   "MassProperties reference precision is below 70 digits")
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "MassProperties input byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-mass-properties/cases.json",
                   "MassProperties input path differs")
    recomputed = build_reference(cases, raw_cases)
    checks.require(oracle == recomputed,
                   "stored MassProperties oracle differs from its producer",
                   len(oracle["cases"]) + 4)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ MassProperties probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed" and
                   probe["semantic_profile_status"] == PROFILE_STATUS,
                   "C++ MassProperties probe identity differs", 4)

    probe_cases = {entry["id"]: entry for entry in probe["cases"]}
    checks.require(len(probe_cases) == len(probe["cases"]) ==
                   len(oracle["cases"]),
                   "C++ MassProperties cases are incomplete", 2)
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    for case_id, expected in oracle["cases"].items():
        checks.require(case_id in probe_cases,
                       f"C++ MassProperties case is missing: {case_id}")
        compare_case(checks, probe_cases[case_id], expected,
                     absolute, relative)

    compare_generic_results(checks, probe["equivalence_results"],
                            oracle["equivalence_results"],
                            absolute, relative, "MassProperties equivalence")
    expected_invalid = {
        entry["id"] for entry in cases["invalid_input_cases"]
    }
    checks.require(set(probe["invalid_input_rejections"]) == expected_invalid,
                   "C++ MassProperties invalid identities differ")
    compare_generic_results(checks, probe["mutation_results"],
                            oracle["mutation_results"],
                            absolute, relative, "MassProperties mutation")

    linked = probe_cases[
        "CASE-YYZ-MASS-PROPERTIES-CURRENT-CANDIDATE-PUBLICATION"]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "case_count": len(probe_cases),
        "current_visible_mass_kg":
            linked["publication_sequence"]["current_visible_mass_kg"],
        "pending_mass_candidate_kg":
            linked["publication_sequence"]["pending_mass_candidate_kg"],
        "next_visible_mass_kg":
            linked["publication_sequence"]["next_visible_mass_kg"],
        "r_CoM_to_application_B_m":
            linked["closure_consumer"]["r_CoM_to_application_B_m"],
        "moment_about_CoM_B_Nm":
            linked["closure_consumer"]["moment_about_CoM_B_Nm"],
        "equivalence_cases_passed": len(oracle["equivalence_results"]),
        "invalid_input_cases_rejected": len(expected_invalid),
        "mutation_cases_rejected": len(oracle["mutation_results"]),
    })


def validate_cases_identity(cases: dict) -> None:
    require(cases["schema_version"] == CASES_SCHEMA and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model"]["model_id"] == MODEL_ID,
            "MassProperties cases identity differs")
    require(cases["semantic_profile"]["status"] == PROFILE_STATUS,
            "MassProperties semantic profile status differs")
    model = cases["model"]
    require(model["mass_state_id"] == MASS_STATE_ID and
            model["body_frame_id"] == BODY_FRAME_ID and
            model["body_origin_point_id"] == BODY_ORIGIN_POINT_ID and
            model["center_of_mass_point_id"] == COM_POINT_ID and
            model["clock_domain"] == CLOCK_DOMAIN and
            model["quality"] == QUALITY and
            model["strategy"] == "FrozenInterval",
            "MassProperties model profile differs")


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
        print(f"YYZ MassProperties reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

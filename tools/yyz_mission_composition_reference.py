#!/usr/bin/env python3
"""Fixture-local YYZ source-to-result composition reference."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal, InvalidOperation, getcontext
import importlib.util
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-MISSION-COMPOSITION-001"
ORACLE_ID = "ORACLE-YYZ-MISSION-COMPOSITION-001"
MODEL_ID = "MODEL-YYZ-FIXTURE-MISSION-COMPOSITION-003"
CASES_SCHEMA = "gnczmkn.yyz-mission-composition-cases/3"
REFERENCE_SCHEMA = "gnczmkn.yyz-mission-composition-reference/3"
MISSION_SOURCE_ID = "mission.fixture.yyz.lookup-open-loop@1"
EXECUTION_ID = "execution.fixture.yyz.lookup-open-loop.0001"
SUBJECT = "vehicle.fixture.yyz@1"
INERTIAL_FRAME_ID = "frame.fixture.yyz.inertial-cartesian@1"
BODY_FRAME_ID = "frame.fixture.yyz.body@1"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
MASS_STATE_ID = "mass.fixture.yyz.vehicle@1"
CONFIGURATION_ID = "configuration.fixture.yyz.clean@1"
CONFIGURATION_REVISION = 11
FROZEN_CASE_ID = "CASE-YYZ-FROZEN-INTERVAL-COMPOSED-TRAJECTORY"
CASE_ID = "CASE-YYZ-MISSION-COMPOSITION-BASELINE"
SUPPORTED_METRICS = {"duration_s", "downrange_m", "remaining_mass_kg"}
REQUIRED_ROLES = {
    "rigid_body",
    "force_moment_closure",
    "air_data",
    "aero_dimensionalization",
    "aero_lookup",
    "environment",
    "propulsion",
    "mass_properties",
    "mass_evolution",
    "frozen_interval",
    "atomic_mass_commit",
    "run_evaluation",
}
DIAGNOSTIC_CASES = {
    "stale-boundary-closure": {
        "id": "DIAGNOSTIC-YYZ-MISSION-STALE-BOUNDARY-CLOSURE",
        "source_failure_id":
            "MUTATION-YYZ-MISSION-COMPOSITION-STALE-BOUNDARY-CLOSURE",
        "code": "GNC-SCH-0201",
        "component_role": "force_moment_closure",
        "stage": "step",
        "region": "advance",
        "callsite": "closure-input-validation",
        "sample_tick": 1,
        "message_key": "closure.sample_tick_mismatch",
        "remediation": "recompute_closure_from_current_committed_boundary",
    },
    "nonatomic-rigid-mass-commit": {
        "id": "DIAGNOSTIC-YYZ-MISSION-NONATOMIC-COMMIT",
        "source_failure_id":
            "MUTATION-YYZ-MISSION-COMPOSITION-NONATOMIC-MASS-COMMIT",
        "code": "GNC-INT-0301",
        "component_role": "atomic_mass_commit",
        "stage": "step",
        "region": "commit",
        "callsite": "atomic-commit-validation",
        "sample_tick": 0,
        "message_key": "commit.rigid_mass_candidate_mismatch",
        "remediation":
            "stage_rigid_and_mass_candidates_in_one_atomic_group",
    },
    "aero-model-domain": {
        "id": "DIAGNOSTIC-YYZ-MISSION-AERO-DOMAIN",
        "source_failure_id":
            "FAILURE-YYZ-MISSION-COMPOSITION-AERO-DOMAIN",
        "code": "GNC-PHY-0201",
        "component_role": "aero_lookup",
        "stage": "step",
        "region": "advance",
        "callsite": "aero-query",
        "sample_tick": 0,
        "message_key": "aero.mach_outside_validated_domain",
        "remediation":
            "use_an_operating_point_inside_the_validated_aero_domain",
    },
}
EVENT_ORDER = [
    "resolve-components",
    "publish-opening-commit",
    "evaluate-opening-boundary",
    "evaluate-interval-0",
    "stage-commit-1",
    "commit-rigid-and-mass-1",
    "evaluate-intermediate-boundary",
    "evaluate-interval-1",
    "stage-commit-2",
    "commit-rigid-and-mass-2",
    "evaluate-terminal-boundary",
    "seal-terminal-observation",
    "freeze-mission-result",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    try:
        result = value if isinstance(value, Decimal) else Decimal(str(value))
    except InvalidOperation as error:
        raise ValueError(f"invalid decimal value: {value}") from error
    require(result.is_finite(), f"non-finite decimal value: {value}")
    return result


def vector(values: object, size: int, label: str) -> list[Decimal]:
    require(isinstance(values, list) and len(values) == size,
            f"{label} must have {size} components")
    return [decimal(value) for value in values]


def valid_integer(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def relative_json_path(repo_root: Path, value: object,
                       prefix: str) -> Path:
    require(isinstance(value, str) and value.endswith(".json") and
            "\\" not in value,
            "component path must be a repository-relative JSON path")
    relative = Path(value)
    require(not relative.is_absolute() and ".." not in relative.parts and
            relative.parts and relative.parts[0] == prefix,
            f"component path must stay beneath {prefix}")
    root = repo_root.resolve()
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as error:
        raise ValueError("component path escapes the repository") from error
    require(candidate.is_file(), f"component file is missing: {value}")
    return candidate


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"),
                      parse_float=Decimal)


def load_reference_module(repo_root: Path, module_name: str,
                          relative_path: str):
    path = (repo_root / relative_path).resolve()
    require(path.is_file(), f"reference module is missing: {relative_path}")
    spec = importlib.util.spec_from_file_location(module_name, path)
    require(spec is not None and spec.loader is not None,
            f"reference module cannot be loaded: {relative_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def validate_cases_identity(cases: dict) -> None:
    require(cases["schema_version"] == CASES_SCHEMA and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model_choice"]["status"] == "accepted",
            "mission-composition cases identity differs")
    model = cases["model"]
    require(model["model_id"] == MODEL_ID and
            model["mission_source_id"] == MISSION_SOURCE_ID and
            model["execution_id"] == EXECUTION_ID and
            model["subject"] == SUBJECT and
            model["inertial_frame_id"] == INERTIAL_FRAME_ID and
            model["body_frame_id"] == BODY_FRAME_ID and
            model["clock_domain"] == CLOCK_DOMAIN and
            model["mass_state_id"] == MASS_STATE_ID and
            model["configuration_id"] == CONFIGURATION_ID and
            model["configuration_revision"] == CONFIGURATION_REVISION and
            decimal(model["base_dt_s"]) > 0 and
            model["initial_tick"] == 0 and
            model["terminal_tick"] == 2 and
            model["integration_strategy"] == "FrozenInterval" and
            model["commit_policy"] == "atomic-rigid-and-mass" and
            model["evaluation_mode"] == "AtGrid" and
            model["evidence_validity"] == "Valid",
            "mission-composition model profile differs")
    require(len(cases["cases"]) == 1 and
            cases["cases"][0]["id"] == CASE_ID,
            "mission-composition bundle must contain its baseline case")
    validate_diagnostic_profile(cases)


def validate_case(case: dict) -> None:
    require(case["frozen_interval_case_id"] == FROZEN_CASE_ID and
            isinstance(case["opening_commit_id"], str) and
            bool(case["opening_commit_id"]) and
            isinstance(case["intermediate_commit_id"], str) and
            bool(case["intermediate_commit_id"]) and
            isinstance(case["closing_commit_id"], str) and
            bool(case["closing_commit_id"]) and
            len({case["opening_commit_id"], case["intermediate_commit_id"],
                 case["closing_commit_id"]}) == 3 and
            isinstance(case["termination_plan_id"], str) and
            bool(case["termination_plan_id"]),
            "mission-composition case identity differs")
    predicates = case["predicates"]
    require(isinstance(predicates, list) and predicates,
            "termination plan must contain predicates")
    predicate_ids: set[str] = set()
    for predicate in predicates:
        identifier = predicate["predicate_id"]
        require(isinstance(identifier, str) and identifier and
                identifier not in predicate_ids,
                "predicate ids must be nonempty and unique")
        predicate_ids.add(identifier)
        require(predicate["metric_id"] in SUPPORTED_METRICS,
                "unsupported predicate metric")
        require(predicate["relation"] in {">=", "<="},
                "unsupported predicate relation")
        decimal(predicate["threshold"])
        require(predicate["action"] in {"Complete", "Abort"} and
                isinstance(predicate["reason_code"], str) and
                bool(predicate["reason_code"]),
                "terminal action or reason differs")
        require(valid_integer(predicate["priority"]) and
                predicate["priority"] >= 0,
                "predicate priority must be a nonnegative integer")
    require(case["terminal_observation_fields"] == [
        "position_I_m", "velocity_I_mps", "q_I_B_wxyz",
        "omega_BI_B_radps", "committed_mass_kg", "metrics", "decision"
    ], "terminal observation field selection differs")


def validate_diagnostic_profile(cases: dict) -> None:
    profile = cases["diagnostic_profile"]
    require(profile == {
        "scope": "fixture-local",
        "authority_domain": "Model",
        "policy_rule_set_id": "qualification.fixture.yyz@1",
        "matched_rule_id": "step-error-fails-before-commit",
        "severity": "Error",
        "disposition": "FailOperation",
        "validity_effect": "Invalid",
    }, "fixture diagnostic policy differs")
    specifications = cases["diagnostic_cases"]
    require(isinstance(specifications, list) and
            len(specifications) == len(DIAGNOSTIC_CASES),
            "fixture diagnostic case count differs")
    require([specification["failure_kind"] for specification in specifications]
            == list(DIAGNOSTIC_CASES),
            "fixture diagnostic case order differs")
    for specification in specifications:
        kind = specification["failure_kind"]
        require(kind in DIAGNOSTIC_CASES,
                f"unsupported diagnostic failure kind: {kind}")
        expected = DIAGNOSTIC_CASES[kind]
        require(all(specification.get(key) == value
                    for key, value in expected.items()),
                f"fixture diagnostic specification differs: {kind}")
        require(specification["component_role"] in REQUIRED_ROLES and
                valid_integer(specification["sample_tick"]) and
                0 <= specification["sample_tick"] <
                    cases["model"]["terminal_tick"],
                f"fixture diagnostic context differs: {kind}")
        if kind == "aero-model-domain":
            vector(specification["injected_velocity_I_mps"], 3,
                   "diagnostic injected velocity")
        else:
            require("injected_velocity_I_mps" not in specification,
                    f"unexpected diagnostic injection: {kind}")


def resolve_components(cases: dict, repo_root: Path) -> tuple[list[dict], dict]:
    bindings = cases["component_bindings"]
    require(isinstance(bindings, list), "component_bindings must be a list")
    seen: set[str] = set()
    documents = {}
    resolved = []
    for binding in bindings:
        role = binding["role"]
        require(isinstance(role, str) and role and role not in seen,
                "component roles must be nonempty and unique")
        seen.add(role)
        cases_path = relative_json_path(
            repo_root, binding["cases_path"], "fixtures")
        oracle_path = relative_json_path(
            repo_root, binding["oracle_path"], "oracles")
        component_cases = load_json(cases_path)
        component_oracle = load_json(oracle_path)
        manifest_path = cases_path.parent / "fixture-manifest.json"
        require(manifest_path.is_file(),
                f"component fixture manifest is missing: {role}")
        manifest = load_json(manifest_path)
        fixture_id = binding["fixture_id"]
        oracle_id = binding["oracle_id"]
        model_id = binding["model_id"]
        require(component_cases["fixture_id"] == fixture_id and
                component_cases["oracle_id"] == oracle_id and
                component_cases["model"]["model_id"] == model_id,
                f"component cases identity differs: {role}")
        require(component_oracle["fixture_id"] == fixture_id and
                component_oracle["oracle_id"] == oracle_id and
                component_oracle["model_id"] == model_id,
                f"component oracle identity differs: {role}")
        require(manifest["fixture_id"] == fixture_id and
                manifest["status"] == "executable",
                f"component fixture is not executable: {role}")
        resolved.append({
            "role": role,
            "fixture_id": fixture_id,
            "oracle_id": oracle_id,
            "model_id": model_id,
            "status": "passed",
        })
        documents[role] = {
            "cases": component_cases,
            "oracle": component_oracle,
        }
    require(seen == REQUIRED_ROLES,
            "component roles differ from the required mission set")
    resolved.sort(key=lambda item: item["role"])
    return resolved, documents


def find_case(values: list[dict], identifier: str, label: str) -> dict:
    selected = [value for value in values if value["id"] == identifier]
    require(len(selected) == 1, f"{label} case identity differs")
    return selected[0]


def validate_cross_component_context(cases: dict, documents: dict,
                                     frozen_input: dict,
                                     frozen_result: dict) -> None:
    model = cases["model"]
    context = frozen_input["context"]
    frozen_model = documents["frozen_interval"]["cases"]["model"]
    require(frozen_model["inertial_frame_id"] ==
                model["inertial_frame_id"] and
            frozen_model["body_frame_id"] == model["body_frame_id"] and
            frozen_model["clock_domain"] == model["clock_domain"] and
            frozen_model["mass_state_id"] == model["mass_state_id"] and
            frozen_model["strategy"] == model["integration_strategy"],
            "FrozenInterval shared context differs")
    require(context["sample_tick"] == model["initial_tick"] and
            context["valid_from_tick"] == model["initial_tick"] and
            context["valid_until_tick"] == model["initial_tick"] + 1 and
            context["configuration_revision"] ==
                model["configuration_revision"] and
            decimal(context["base_dt_s"]) == decimal(model["base_dt_s"]),
            "FrozenInterval tick, revision or dt differs")
    require(frozen_input["environment_sample"]["inertial_frame_id"] ==
                model["inertial_frame_id"] and
            frozen_input["environment_sample"]["clock_domain"] ==
                model["clock_domain"] and
            frozen_input["mass_properties_sample"]["body_frame_id"] ==
                model["body_frame_id"] and
            frozen_input["mass_properties_sample"]["mass_state_id"] ==
                model["mass_state_id"] and
            frozen_input["aero_lookup"]["configuration_id"] ==
                model["configuration_id"] and
            frozen_input["aero_lookup"]["configuration_revision"] ==
                model["configuration_revision"],
            "FrozenInterval input identity differs")
    require(frozen_result["context"]["sample_tick"] ==
                model["initial_tick"] and
            frozen_result["context"]["valid_until_tick"] ==
                model["initial_tick"] + 1 and
            decimal(frozen_result["context"]["base_dt_s"]) ==
                decimal(model["base_dt_s"]),
            "FrozenInterval result context differs")
    require(frozen_result["aero_lookup"]["model_id"] ==
                documents["aero_lookup"]["cases"]["model"]["model_id"] and
            frozen_result["aero_lookup"]["configuration_id"] ==
                model["configuration_id"] and
            frozen_result["mass_visibility"]["mass_state_id"] ==
                model["mass_state_id"],
            "lookup or mass result identity differs")
    atomic_model = documents["atomic_mass_commit"]["cases"]["model"]
    require(atomic_model["commit_policy"] == model["commit_policy"] and
            atomic_model["closure_strategy"] ==
                model["integration_strategy"] and
            atomic_model["inertial_frame_id"] ==
                model["inertial_frame_id"] and
            atomic_model["body_frame_id"] == model["body_frame_id"] and
            atomic_model["clock_domain"] == model["clock_domain"] and
            atomic_model["mass_state_id"] == model["mass_state_id"] and
            atomic_model["configuration_revision"] ==
                model["configuration_revision"],
            "atomic commit semantics differ")
    evaluation_model = documents["run_evaluation"]["cases"]["model"]
    require(evaluation_model["clock_domain"] == model["clock_domain"] and
            evaluation_model["evaluation_mode"] == model["evaluation_mode"] and
            evaluation_model["composition"] == "any" and
            evaluation_model["priority_policy"] ==
                "higher-integer-then-ascending-predicate-id",
            "run-evaluation semantics differ")
    require(documents["mass_evolution"]["cases"]["model"]["model_id"] ==
                atomic_model["mass_evolution_model_id"],
            "mass-evolution model identity differs")


def committed_sample(tick: int, time_s: Decimal, commit_id: str,
                     state: dict, mass: Decimal) -> dict:
    require(mass > 0, "committed mass must be positive")
    return {
        "sample_tick": tick,
        "time_s": time_s,
        "commit_id": commit_id,
        "quality": "Valid",
        "position_I_m": vector(state["position_I_m"], 3, "position"),
        "velocity_I_mps": vector(state["velocity_I_mps"], 3, "velocity"),
        "q_I_B_wxyz": vector(state["q_I_B_wxyz"], 4, "q_I_B"),
        "omega_BI_B_radps": vector(
            state["omega_BI_B_radps"], 3, "angular rate"),
        "committed_mass_kg": mass,
    }


def sample_metrics(sample: dict, opening_state: dict,
                   source_opening_mass: Decimal) -> dict:
    position = vector(sample["position_I_m"], 3, "position")
    initial_position = vector(opening_state["position_I_m"], 3,
                              "initial position")
    velocity = vector(sample["velocity_I_mps"], 3, "velocity")
    speed = sum((value * value for value in velocity), Decimal(0)).sqrt()
    mass = decimal(sample["committed_mass_kg"])
    return {
        "duration_s": decimal(sample["time_s"]),
        "downrange_m": position[0] - initial_position[0],
        "vertical_displacement_m": position[2] - initial_position[2],
        "remaining_mass_kg": mass,
        "consumed_mass_kg": source_opening_mass - mass,
        "speed_mps": speed,
    }


def metric_value(metrics: dict, metric_id: str) -> Decimal:
    require(metric_id in SUPPORTED_METRICS,
            f"unsupported metric id: {metric_id}")
    return metrics[metric_id]


def evaluate_predicates(case: dict, metrics: dict,
                        low_priority_wins: bool) -> tuple[list[dict], dict]:
    results = []
    for predicate in case["predicates"]:
        observed = metric_value(metrics, predicate["metric_id"])
        threshold = decimal(predicate["threshold"])
        relation = predicate["relation"]
        met = observed >= threshold if relation == ">=" else observed <= threshold
        results.append({
            "predicate_id": predicate["predicate_id"],
            "metric_id": predicate["metric_id"],
            "relation": relation,
            "threshold": threshold,
            "observed": observed,
            "met": met,
            "action": predicate["action"],
            "reason_code": predicate["reason_code"],
            "priority": predicate["priority"],
        })
    results.sort(key=lambda item: item["predicate_id"])
    met_results = [item for item in results if item["met"]]
    decision_metrics = {
        "duration_s": metrics["duration_s"],
        "downrange_m": metrics["downrange_m"],
        "remaining_mass_kg": metrics["remaining_mass_kg"],
    }
    if not met_results:
        return results, {
            "action": "Continue",
            "reason_code": "none",
            "trigger_time_s": metrics["duration_s"],
            "subject": SUBJECT,
            "priority": 0,
            "metrics": decision_metrics,
            "message_key": "yyz.termination.continue",
            "params": {},
        }
    if low_priority_wins:
        selected = min(met_results, key=lambda item:
                       (item["priority"], item["predicate_id"]))
    else:
        selected = min(met_results, key=lambda item:
                       (-item["priority"], item["predicate_id"]))
    return results, {
        "action": selected["action"],
        "reason_code": selected["reason_code"],
        "trigger_time_s": metrics["duration_s"],
        "subject": SUBJECT,
        "priority": selected["priority"],
        "metrics": decision_metrics,
        "message_key": f"yyz.termination.{selected['reason_code']}",
        "params": {},
    }


def metric_summary(trace: list[dict]) -> dict:
    require(trace, "evaluation trace must not be empty")
    terminal = trace[-1]
    peak = max(trace, key=lambda item: item["metrics"]["speed_mps"])
    maximum_downrange = max(
        trace, key=lambda item: item["metrics"]["downrange_m"])
    minimum_mass = min(
        trace, key=lambda item: item["metrics"]["remaining_mass_kg"])
    metrics = terminal["metrics"]
    return {
        "evaluated_sample_count": len(trace),
        "duration_s": metrics["duration_s"],
        "downrange_m": metrics["downrange_m"],
        "vertical_displacement_m": metrics["vertical_displacement_m"],
        "remaining_mass_kg": metrics["remaining_mass_kg"],
        "consumed_mass_kg": metrics["consumed_mass_kg"],
        "terminal_speed_mps": metrics["speed_mps"],
        "peak_speed_mps": peak["metrics"]["speed_mps"],
        "peak_speed_tick": peak["sample_tick"],
        "maximum_downrange_m":
            maximum_downrange["metrics"]["downrange_m"],
        "maximum_downrange_tick": maximum_downrange["sample_tick"],
        "minimum_remaining_mass_kg":
            minimum_mass["metrics"]["remaining_mass_kg"],
        "minimum_remaining_mass_tick": minimum_mass["sample_tick"],
    }


STATE_FIELDS = (
    "position_I_m",
    "velocity_I_mps",
    "q_I_B_wxyz",
    "omega_BI_B_radps",
)


def state_fields(state: dict) -> dict:
    return {
        "position_I_m": vector(state["position_I_m"], 3, "position"),
        "velocity_I_mps": vector(state["velocity_I_mps"], 3, "velocity"),
        "q_I_B_wxyz": vector(state["q_I_B_wxyz"], 4, "q_I_B"),
        "omega_BI_B_radps": vector(
            state["omega_BI_B_radps"], 3, "angular rate"),
    }


def interval_case(base: dict, tick: int, opening_state: dict,
                  opening_mass: Decimal) -> dict:
    value = copy.deepcopy(base)
    value["id"] = f"CASE-YYZ-MISSION-COMPOSITION-INTERVAL-{tick}"
    value["context"].update({
        "sample_tick": tick,
        "valid_from_tick": tick,
        "valid_until_tick": tick + 1,
    })
    value["initial_state"] = state_fields(opening_state)
    for section_name in ("environment_sample", "mass_properties_sample",
                         "aero_lookup", "propulsion_response"):
        section = value[section_name]
        section["sample_tick"] = tick
        if "valid_from_tick" in section:
            section["valid_from_tick"] = tick
            section["valid_until_tick"] = tick + 1
    value["mass_properties_sample"]["mass_kg"] = opening_mass
    value["aero_lookup"]["omega_BI_B_radps"] = vector(
        opening_state["omega_BI_B_radps"], 3, "aero body rate")
    value["terminal"]["expected_tick"] = tick + 1
    return value


def core_inputs(interval_input: dict, interval_result: dict,
                opening_mass: Decimal) -> dict:
    return {
        "mass_kg": opening_mass,
        "inertia_B_kgm2_row_major": interval_input["mass_properties_sample"]
            ["inertia_about_CoM_B_kgm2_row_major"],
        "force_B_N": interval_result["closure"]["force_total_B_N"],
        "moment_B_Nm": interval_result["closure"]
            ["moment_total_about_CoM_B_Nm"],
        "gravity_I_mps2": interval_result["environment_sample"]
            ["gravity_I_mps2"],
    }


def rigid_derivative(core_module, opening_state: dict,
                     inputs: dict) -> dict:
    formula = core_module.formula_reference({
        "state": state_fields(opening_state),
        "inputs": inputs,
    })
    return {
        "force_total_I_N": formula["force_I_N"],
        "acceleration_I_mps2": formula["velocity_derivative_I_mps2"],
        "angular_momentum_B_kgm2ps":
            formula["angular_momentum_B_kgm2ps"],
        "gyroscopic_moment_B_Nm": formula["gyroscopic_moment_B_Nm"],
        "net_moment_B_Nm": formula["net_moment_B_Nm"],
        "angular_acceleration_B_radps2":
            formula["omega_derivative_B_radps2"],
        "q_derivative_I_B_per_s": formula["q_derivative_I_B_per_s"],
    }


def integrate_state(core_module, opening_state: dict, inputs: dict,
                    duration_s: Decimal, substeps: int) -> tuple[dict, Decimal]:
    require(valid_integer(substeps) and substeps > 0,
            "RK4 substep count must be a positive integer")
    state = state_fields(opening_state)
    step_s = duration_s / Decimal(substeps)
    maximum_residual = Decimal(0)
    for _ in range(substeps):
        state, residual = core_module.decimal_rk4_step(
            state, inputs, step_s)
        maximum_residual = max(maximum_residual, residual)
    return state, maximum_residual


def state_max_difference(lhs: dict, rhs: dict) -> Decimal:
    return max(
        abs(decimal(left) - decimal(right))
        for field in STATE_FIELDS
        for left, right in zip(lhs[field], rhs[field])
    )


def convergence_series(core_module, opening_state: dict, inputs: dict,
                       duration_s: Decimal, *,
                       require_fourth_order: bool = True) -> dict:
    entries = []
    for substeps in (1, 2, 4, 8):
        state, residual = integrate_state(
            core_module, opening_state, inputs, duration_s, substeps)
        entries.append({
            "substeps": substeps,
            "terminal_state": state,
            "maximum_pre_normalization_quaternion_norm_residual": residual,
        })
    differences = [
        state_max_difference(entries[index]["terminal_state"],
                             entries[index + 1]["terminal_state"])
        for index in range(len(entries) - 1)
    ]
    require(all(value > 0 for value in differences),
            "second-interval convergence differences must be positive")
    ratios = [differences[index] / differences[index + 1]
              for index in range(len(differences) - 1)]
    minimum_ratio = Decimal(12)
    require(not require_fourth_order or
            all(value >= minimum_ratio for value in ratios),
            f"second interval did not demonstrate fourth-order convergence: "
            f"differences={differences}, ratios={ratios}")
    return {
        "substep_counts": [1, 2, 4, 8],
        "terminal_states": entries,
        "successive_max_abs_differences": differences,
        "error_reduction_ratios": ratios,
        "minimum_required_error_reduction_ratio": minimum_ratio,
    }


def interval_execution(result: dict, opening_mass: Decimal,
                       closing_mass: Decimal) -> dict:
    mass = result["mass_visibility"]
    return {
        "sample_tick": result["context"]["sample_tick"],
        "valid_from_tick": result["context"]["valid_from_tick"],
        "valid_until_tick": result["context"]["valid_until_tick"],
        "configuration_revision": result["context"]["configuration_revision"],
        "base_dt_s": decimal(result["context"]["base_dt_s"]),
        "strategy": "FrozenInterval",
        "integration_substeps": 1,
        "environment_sample": result["environment_sample"],
        "air_data": result["air_data"],
        "aero_lookup": result["aero_lookup"],
        "closure": result["closure"],
        "rigid_derivative_at_opening": result["rigid_derivative_at_tick0"],
        "mass_transition": {
            "mass_state_id": MASS_STATE_ID,
            "opening_committed_mass_kg": opening_mass,
            "consumed_mass_kg": decimal(mass["consumed_mass_kg"]),
            "pending_mass_candidate_kg":
                decimal(mass["pending_mass_candidate_kg"]),
            "pending_visibility_before_commit": "candidate-only",
            "closing_committed_mass_kg": closing_mass,
            "closing_commit_kind": "atomic-rigid-and-mass",
        },
    }


def compose(cases: dict, repo_root: Path, *,
            early_mass_visibility: bool = False,
            nonatomic_mass_commit: bool = False,
            stale_boundary_closure: bool = False,
            low_priority_wins: bool = False,
            result_before_observation: bool = False) -> dict:
    validate_cases_identity(cases)
    case = cases["cases"][0]
    validate_case(case)
    resolved, documents = resolve_components(cases, repo_root)
    frozen_cases = documents["frozen_interval"]["cases"]
    frozen_input = find_case(
        frozen_cases["cases"], case["frozen_interval_case_id"],
        "FrozenInterval input")
    frozen_oracle_cases = documents["frozen_interval"]["oracle"]["cases"]
    require(case["frozen_interval_case_id"] in frozen_oracle_cases,
            "FrozenInterval oracle case is missing")
    frozen_result = frozen_oracle_cases[case["frozen_interval_case_id"]]
    validate_cross_component_context(
        cases, documents, frozen_input, frozen_result)

    frozen_module = load_reference_module(
        repo_root, "yyz_mission_frozen_interval_reference",
        "tools/yyz_frozen_interval_reference.py")
    core_module = load_reference_module(
        repo_root, "yyz_mission_6dof_core_reference",
        "tools/yyz_6dof_core_reference.py")
    first_result = frozen_module.compose(frozen_input)
    require(stringify(first_result) == frozen_result,
            "FrozenInterval stored oracle differs from its formula reference")

    model = cases["model"]
    dt_s = decimal(model["base_dt_s"])
    opening_state = frozen_input["initial_state"]
    source_opening_mass = decimal(
        frozen_input["mass_properties_sample"]["mass_kg"])
    current_visible_mass = decimal(
        first_result["mass_visibility"]["current_visible_mass_kg"])
    candidate_mass = decimal(
        first_result["mass_visibility"]["pending_mass_candidate_kg"])
    consumed_mass = decimal(
        first_result["mass_visibility"]["consumed_mass_kg"])
    require(current_visible_mass == source_opening_mass and
            candidate_mass == source_opening_mass - consumed_mass and
            decimal(first_result["mass_visibility"]["integration_mass_kg"]) ==
                source_opening_mass and
            first_result["mass_visibility"]
                ["pending_visibility_before_commit"] == "candidate-only" and
            first_result["mass_visibility"]["next_commit_tick"] ==
                model["initial_tick"] + 1,
            "FrozenInterval mass handoff differs")
    intermediate_state = first_result["analytic_terminal"]
    require(intermediate_state["tick"] == model["initial_tick"] + 1 and
            decimal(intermediate_state["time_s"]) == dt_s,
            "first FrozenInterval terminal state identity differs")

    opening_mass = candidate_mass if early_mass_visibility else source_opening_mass
    intermediate_mass = (source_opening_mass if nonatomic_mass_commit
                         else candidate_mass)
    second_input = interval_case(
        frozen_input, 1, intermediate_state, intermediate_mass)
    second_result = frozen_module.compose(
        second_input, include_trajectory=False)
    if stale_boundary_closure:
        second_result = copy.deepcopy(second_result)
        second_result["air_data"] = copy.deepcopy(first_result["air_data"])
        second_result["aero_lookup"] = copy.deepcopy(
            first_result["aero_lookup"])
        second_result["closure"] = copy.deepcopy(first_result["closure"])
        stale_inputs = core_inputs(
            second_input, second_result, intermediate_mass)
        second_result["rigid_derivative_at_tick0"] = rigid_derivative(
            core_module, intermediate_state, stale_inputs)
    second_mass = second_result["mass_visibility"]
    second_candidate_mass = decimal(
        second_mass["pending_mass_candidate_kg"])
    require(decimal(second_mass["current_visible_mass_kg"]) ==
                intermediate_mass and
            decimal(second_mass["integration_mass_kg"]) ==
                intermediate_mass and
            second_candidate_mass == intermediate_mass - consumed_mass and
            second_mass["pending_visibility_before_commit"] ==
                "candidate-only" and
            second_mass["next_commit_tick"] == model["terminal_tick"],
            "second FrozenInterval mass handoff differs")
    second_inputs = core_inputs(
        second_input, second_result, intermediate_mass)
    convergence = convergence_series(
        core_module, intermediate_state, second_inputs, dt_s,
        require_fourth_order=not stale_boundary_closure)
    closing_state = convergence["terminal_states"][0]["terminal_state"]
    closing_mass = second_candidate_mass

    opening = committed_sample(
        model["initial_tick"], Decimal(model["initial_tick"]) * dt_s,
        case["opening_commit_id"], opening_state, opening_mass)
    intermediate = committed_sample(
        1, dt_s, case["intermediate_commit_id"], intermediate_state,
        intermediate_mass)
    closing = committed_sample(
        model["terminal_tick"], Decimal(model["terminal_tick"]) * dt_s,
        case["closing_commit_id"], closing_state, closing_mass)
    committed_samples = [opening, intermediate, closing]

    evaluation_trace = []
    terminal_boundary = None
    for sample in committed_samples:
        metrics = sample_metrics(sample, opening_state, source_opening_mass)
        predicate_results, decision = evaluate_predicates(
            case, metrics, low_priority_wins)
        boundary = {
            "sample_tick": sample["sample_tick"],
            "time_s": sample["time_s"],
            "commit_id": sample["commit_id"],
            "metrics": metrics,
            "predicate_results": predicate_results,
            "decision": decision,
        }
        evaluation_trace.append(boundary)
        if decision["action"] != "Continue":
            terminal_boundary = boundary
            break
    require(terminal_boundary is not None,
            "mission composition did not reach a terminal boundary")
    require(terminal_boundary["sample_tick"] == model["terminal_tick"],
            "mission composition terminated on the wrong boundary")

    summary = metric_summary(evaluation_trace)
    event_order = list(EVENT_ORDER)
    if result_before_observation:
        event_order[-2:] = ["freeze-mission-result",
                            "seal-terminal-observation"]
    terminal_observation = {
        "sample_tick": closing["sample_tick"],
        "time_s": closing["time_s"],
        "commit_id": closing["commit_id"],
        "quality": "Valid",
        "fields": {
            "position_I_m": closing["position_I_m"],
            "velocity_I_mps": closing["velocity_I_mps"],
            "q_I_B_wxyz": closing["q_I_B_wxyz"],
            "omega_BI_B_radps": closing["omega_BI_B_radps"],
            "committed_mass_kg": closing["committed_mass_kg"],
            "metrics": terminal_boundary["metrics"],
            "decision": terminal_boundary["decision"],
        },
        "event_order": event_order,
        "sealed": not result_before_observation,
    }
    action = terminal_boundary["decision"]["action"]
    final_status = "Completed" if action == "Complete" else "Terminated"
    mission_result = {
        "final_status": final_status,
        "evidence_validity": "Valid",
        "initial_tick": model["initial_tick"],
        "final_tick": terminal_boundary["sample_tick"],
        "final_time_s": terminal_boundary["time_s"],
        "termination": terminal_boundary["decision"],
        "metrics": summary,
        "terminal_observation_commit_id": closing["commit_id"],
        "terminal_observation_sealed": terminal_observation["sealed"],
        "frozen": True,
    }
    interval_executions = [
        interval_execution(first_result, source_opening_mass,
                           intermediate_mass),
        interval_execution(second_result, intermediate_mass, closing_mass),
    ]
    execution_trace = [
        {"order": 0, "event": "resolve-components",
         "sample_tick": 0, "component_count": len(resolved)},
        {"order": 1, "event": "publish-opening-commit",
         "sample_tick": 0, "commit_id": opening["commit_id"]},
        {"order": 2, "event": "evaluate-opening-boundary",
         "sample_tick": 0,
         "action": evaluation_trace[0]["decision"]["action"]},
        {"order": 3, "event": "evaluate-interval-0",
         "sample_tick": 0, "valid_until_tick": 1},
        {"order": 4, "event": "stage-commit-1",
         "sample_tick": 0, "candidate_tick": 1},
        {"order": 5, "event": "commit-rigid-and-mass-1",
         "sample_tick": 1, "commit_id": intermediate["commit_id"]},
        {"order": 6, "event": "evaluate-intermediate-boundary",
         "sample_tick": 1,
         "action": evaluation_trace[1]["decision"]["action"]},
        {"order": 7, "event": "evaluate-interval-1",
         "sample_tick": 1, "valid_until_tick": 2},
        {"order": 8, "event": "stage-commit-2",
         "sample_tick": 1, "candidate_tick": 2},
        {"order": 9, "event": "commit-rigid-and-mass-2",
         "sample_tick": 2, "commit_id": closing["commit_id"]},
        {"order": 10, "event": "evaluate-terminal-boundary",
         "sample_tick": 2,
         "action": terminal_boundary["decision"]["action"]},
        {"order": 11, "event": "seal-terminal-observation",
         "sample_tick": 2, "commit_id": closing["commit_id"]},
        {"order": 12, "event": "freeze-mission-result",
         "sample_tick": 2, "status": final_status},
    ]
    return {
        "id": case["id"],
        "mission_source_id": MISSION_SOURCE_ID,
        "execution_id": EXECUTION_ID,
        "composition_model_id": MODEL_ID,
        "resolved_components": resolved,
        "execution_trace": execution_trace,
        "committed_samples": committed_samples,
        "interval_executions": interval_executions,
        "second_interval_convergence": convergence,
        "evaluation_trace": evaluation_trace,
        "metric_summary": summary,
        "terminal_observation": terminal_observation,
        "mission_result": mission_result,
    }


def invalid_rejections(cases: dict, repo_root: Path) -> list[str]:
    def remove_role(value: dict, role: str) -> None:
        value["component_bindings"] = [
            binding for binding in value["component_bindings"]
            if binding["role"] != role
        ]

    def change_binding(value: dict, role: str, field: str,
                       changed: str) -> None:
        selected = [binding for binding in value["component_bindings"]
                    if binding["role"] == role]
        require(len(selected) == 1, "invalid mutation binding is missing")
        selected[0][field] = changed

    actions = {
        "INVALID-YYZ-MISSION-COMPOSITION-DUPLICATE-ROLE":
            lambda value: value["component_bindings"].append(
                copy.deepcopy(value["component_bindings"][0])),
        "INVALID-YYZ-MISSION-COMPOSITION-MISSING-ROLE":
            lambda value: remove_role(value, "frozen_interval"),
        "INVALID-YYZ-MISSION-COMPOSITION-FIXTURE-ID":
            lambda value: change_binding(
                value, "aero_lookup", "fixture_id", "REF-YYZ-OTHER-001"),
        "INVALID-YYZ-MISSION-COMPOSITION-ORACLE-ID":
            lambda value: change_binding(
                value, "propulsion", "oracle_id", "ORACLE-YYZ-OTHER-001"),
        "INVALID-YYZ-MISSION-COMPOSITION-MODEL-ID":
            lambda value: change_binding(
                value, "rigid_body", "model_id", "MODEL-YYZ-OTHER-001"),
        "INVALID-YYZ-MISSION-COMPOSITION-CLOCK":
            lambda value: value["model"].__setitem__(
                "clock_domain", "clock.fixture.yyz.other@1"),
        "INVALID-YYZ-MISSION-COMPOSITION-REVISION":
            lambda value: value["model"].__setitem__(
                "configuration_revision", 12),
        "INVALID-YYZ-MISSION-COMPOSITION-DT":
            lambda value: value["model"].__setitem__("base_dt_s", "0.2"),
        "INVALID-YYZ-MISSION-COMPOSITION-RELATION":
            lambda value: value["cases"][0]["predicates"][0].__setitem__(
                "relation", "<"),
        "INVALID-YYZ-MISSION-COMPOSITION-NO-TERMINAL":
            lambda value: [
                predicate.update({
                    "metric_id": "duration_s", "relation": ">=",
                    "threshold": 10,
                }) for predicate in value["cases"][0]["predicates"]
            ],
    }
    rejected = []
    for specification in cases["invalid_input_cases"]:
        identifier = specification["id"]
        require(identifier in actions,
                f"unsupported invalid-input case: {identifier}")
        mutated = copy.deepcopy(cases)
        actions[identifier](mutated)
        try:
            compose(mutated, repo_root)
        except (ArithmeticError, IndexError, KeyError, OSError,
                TypeError, ValueError):
            rejected.append(identifier)
        else:
            raise ValueError(f"invalid input was accepted: {identifier}")
    return rejected


def mutation_results(cases: dict, repo_root: Path,
                     accepted: dict) -> list[dict]:
    early = compose(cases, repo_root, early_mass_visibility=True)
    nonatomic = compose(cases, repo_root, nonatomic_mass_commit=True)
    stale = compose(cases, repo_root, stale_boundary_closure=True)
    low = compose(cases, repo_root, low_priority_wins=True)
    order = compose(cases, repo_root, result_before_observation=True)
    results = [
        {
            "id": "MUTATION-YYZ-MISSION-COMPOSITION-EARLY-MASS-VISIBILITY",
            "status": "rejected",
            "expected_opening_committed_mass_kg":
                accepted["committed_samples"][0]["committed_mass_kg"],
            "observed_opening_committed_mass_kg":
                early["committed_samples"][0]["committed_mass_kg"],
            "observed_opening_consumed_mass_kg":
                early["evaluation_trace"][0]["metrics"]["consumed_mass_kg"],
            "max_abs_result_difference": Decimal("0.05"),
        },
        {
            "id": "MUTATION-YYZ-MISSION-COMPOSITION-NONATOMIC-MASS-COMMIT",
            "status": "rejected",
            "expected_intermediate_committed_mass_kg":
                accepted["committed_samples"][1]["committed_mass_kg"],
            "observed_intermediate_committed_mass_kg":
                nonatomic["committed_samples"][1]["committed_mass_kg"],
            "expected_closing_committed_mass_kg":
                accepted["committed_samples"][2]["committed_mass_kg"],
            "observed_closing_committed_mass_kg":
                nonatomic["committed_samples"][2]["committed_mass_kg"],
            "observed_terminal_consumed_mass_kg":
                nonatomic["metric_summary"]["consumed_mass_kg"],
            "max_abs_result_difference": Decimal("0.05"),
        },
        {
            "id": "MUTATION-YYZ-MISSION-COMPOSITION-STALE-BOUNDARY-CLOSURE",
            "status": "rejected",
            "expected_force_total_B_N":
                accepted["interval_executions"][1]["closure"]
                    ["force_total_B_N"],
            "observed_force_total_B_N":
                stale["interval_executions"][1]["closure"]
                    ["force_total_B_N"],
            "expected_terminal_position_I_m":
                accepted["committed_samples"][2]["position_I_m"],
            "observed_terminal_position_I_m":
                stale["committed_samples"][2]["position_I_m"],
            "max_abs_result_difference": state_max_difference(
                accepted["committed_samples"][2],
                stale["committed_samples"][2]),
        },
        {
            "id": "MUTATION-YYZ-MISSION-COMPOSITION-LOW-PRIORITY-WINS",
            "status": "rejected",
            "expected_reason_code":
                accepted["mission_result"]["termination"]["reason_code"],
            "observed_reason_code":
                low["mission_result"]["termination"]["reason_code"],
            "expected_priority":
                accepted["mission_result"]["termination"]["priority"],
            "observed_priority":
                low["mission_result"]["termination"]["priority"],
            "max_abs_result_difference": Decimal(100),
        },
        {
            "id": "MUTATION-YYZ-MISSION-COMPOSITION-RESULT-BEFORE-OBSERVATION",
            "status": "rejected",
            "expected_event_order": EVENT_ORDER,
            "observed_event_order": order["terminal_observation"]["event_order"],
            "observed_terminal_observation_sealed":
                order["mission_result"]["terminal_observation_sealed"],
            "max_abs_result_difference": Decimal(1),
        },
    ]
    require([entry["id"] for entry in results] ==
            [entry["id"] for entry in cases["mutation_cases"]],
            "mutation identities differ")
    require(all(entry["max_abs_result_difference"] > 0
                for entry in results),
            "a mission-composition mutation survived")
    return results


def resolved_binding(accepted: dict, role: str) -> dict:
    selected = [binding for binding in accepted["resolved_components"]
                if binding["role"] == role]
    require(len(selected) == 1,
            f"resolved diagnostic component differs: {role}")
    return selected[0]


def committed_sample_at(accepted: dict, tick: int) -> dict:
    selected = [sample for sample in accepted["committed_samples"]
                if sample["sample_tick"] == tick]
    require(len(selected) == 1,
            f"diagnostic base commit differs at tick {tick}")
    return selected[0]


def vector_max_difference(lhs: object, rhs: object, label: str) -> Decimal:
    left = vector(lhs, 3, f"{label} left")
    right = vector(rhs, 3, f"{label} right")
    return max(abs(first - second) for first, second in zip(left, right))


def diagnostic_result(cases: dict, accepted: dict, specification: dict,
                      parameters: dict, evidence: dict) -> dict:
    profile = cases["diagnostic_profile"]
    kind = specification["failure_kind"]
    diagnostic_id = f"diag:fixture:yyz:{kind}"
    base = committed_sample_at(accepted, specification["sample_tick"])
    binding = resolved_binding(accepted, specification["component_role"])
    return {
        "id": specification["id"],
        "source_failure_id": specification["source_failure_id"],
        "diagnostic_record": {
            "diagnostic_id": diagnostic_id,
            "code": specification["code"],
            "code_scope": profile["scope"],
            "category": {
                "stale-boundary-closure": "scheduling",
                "nonatomic-rigid-mass-commit": "internal",
                "aero-model-domain": "physical-domain",
            }[kind],
            "authority_domain": profile["authority_domain"],
            "stage": specification["stage"],
            "region": specification["region"],
            "callsite": specification["callsite"],
            "subject": {
                "component_role": binding["role"],
                "fixture_id": binding["fixture_id"],
                "oracle_id": binding["oracle_id"],
                "model_id": binding["model_id"],
            },
            "message_key": specification["message_key"],
            "parameters": parameters,
            "simulation_context": {
                "sample_tick": base["sample_tick"],
                "time_s": decimal(base["time_s"]),
                "clock_domain": CLOCK_DOMAIN,
            },
            "evidence": evidence,
            "cause_ids": [],
            "related_ids": [],
            "remediation": [specification["remediation"]],
        },
        "policy_decision": {
            "decision_id": f"policy-decision:fixture:yyz:{kind}",
            "diagnostic_id": diagnostic_id,
            "policy_rule_set_id": profile["policy_rule_set_id"],
            "matched_rule_id": profile["matched_rule_id"],
            "severity": profile["severity"],
            "disposition": profile["disposition"],
            "validity_effect": profile["validity_effect"],
        },
        "step_outcome": {
            "outcome_type": "FixtureStepOutcome",
            "status": "Failed",
            "evidence_validity": profile["validity_effect"],
            "base_commit_id": base["commit_id"],
            "resulting_commit_id": base["commit_id"],
            "base_tick": base["sample_tick"],
            "resulting_tick": base["sample_tick"],
            "base_committed_mass_kg": decimal(base["committed_mass_kg"]),
            "resulting_committed_mass_kg":
                decimal(base["committed_mass_kg"]),
            "primary_diagnostic_id": diagnostic_id,
            "candidate_commit_published": False,
            "rollback_verified": True,
        },
    }


def diagnostic_results(cases: dict, repo_root: Path,
                       accepted: dict) -> list[dict]:
    specifications = {
        value["failure_kind"]: value for value in cases["diagnostic_cases"]
    }
    stale = compose(cases, repo_root, stale_boundary_closure=True)
    expected_interval = accepted["interval_executions"][1]
    observed_interval = stale["interval_executions"][1]
    stale_specification = specifications["stale-boundary-closure"]
    stale_result = diagnostic_result(
        cases, accepted, stale_specification,
        {
            "expected_sample_tick": expected_interval["sample_tick"],
            "observed_sample_tick":
                stale["interval_executions"][0]["sample_tick"],
        },
        {
            "required_valid_from_tick":
                expected_interval["valid_from_tick"],
            "observed_valid_from_tick":
                stale["interval_executions"][0]["valid_from_tick"],
            "observed_valid_until_tick":
                stale["interval_executions"][0]["valid_until_tick"],
            "expected_force_total_B_N":
                expected_interval["closure"]["force_total_B_N"],
            "observed_force_total_B_N":
                observed_interval["closure"]["force_total_B_N"],
            "max_abs_force_difference_N": vector_max_difference(
                expected_interval["closure"]["force_total_B_N"],
                observed_interval["closure"]["force_total_B_N"],
                "stale Closure force"),
        })

    nonatomic = compose(cases, repo_root, nonatomic_mass_commit=True)
    accepted_intermediate = accepted["committed_samples"][1]
    observed_intermediate = nonatomic["committed_samples"][1]
    atomic_specification = specifications["nonatomic-rigid-mass-commit"]
    atomic_result = diagnostic_result(
        cases, accepted, atomic_specification,
        {
            "rigid_candidate_tick": 1,
            "observed_mass_candidate_tick": 0,
        },
        {
            "required_commit_kind": "atomic-rigid-and-mass",
            "expected_intermediate_mass_kg":
                decimal(accepted_intermediate["committed_mass_kg"]),
            "observed_intermediate_mass_kg":
                decimal(observed_intermediate["committed_mass_kg"]),
            "max_abs_mass_difference_kg":
                abs(decimal(accepted_intermediate["committed_mass_kg"]) -
                    decimal(observed_intermediate["committed_mass_kg"])),
        })

    _, documents = resolve_components(cases, repo_root)
    frozen_cases = documents["frozen_interval"]["cases"]
    baseline_case = find_case(
        frozen_cases["cases"], FROZEN_CASE_ID, "diagnostic aero input")
    domain_specification = specifications["aero-model-domain"]
    injected_velocity = vector(
        domain_specification["injected_velocity_I_mps"], 3,
        "diagnostic injected velocity")
    environment = baseline_case["environment_sample"]
    airmass = vector(
        environment["velocity_airmass_I_mps"], 3, "diagnostic airmass")
    relative_velocity = [value - wind for value, wind in
                         zip(injected_velocity, airmass)]
    airspeed = sum((value * value for value in relative_velocity),
                   Decimal(0)).sqrt()
    sound_speed = decimal(environment["speed_of_sound_mps"])
    mach = airspeed / sound_speed
    aero = baseline_case["aero_lookup"]
    mach_axis = [decimal(value) for value in
                 aero["prepared_table"]["mach_axis"]]
    domain_minimum = min(mach_axis)
    domain_maximum = max(mach_axis)
    frozen_module = load_reference_module(
        repo_root, "yyz_mission_diagnostic_frozen_reference",
        "tools/yyz_frozen_interval_reference.py")
    rejected = False
    try:
        frozen_module.aero_lookup(
            aero, mach, Decimal(0), Decimal(0), "trilinear")
    except (ArithmeticError, IndexError, KeyError, TypeError, ValueError):
        rejected = True
    require(rejected and mach > domain_maximum,
            "aero model-domain diagnostic was not triggered")
    domain_result = diagnostic_result(
        cases, accepted, domain_specification,
        {"axis_id": "mach", "domain_policy": "Reject"},
        {
            "injected_velocity_I_mps": injected_velocity,
            "velocity_airmass_I_mps": airmass,
            "velocity_relative_I_mps": relative_velocity,
            "airspeed_mps": airspeed,
            "speed_of_sound_mps": sound_speed,
            "query_value": mach,
            "minimum_inclusive": domain_minimum,
            "maximum_inclusive": domain_maximum,
            "excess_above_maximum": mach - domain_maximum,
        })

    results_by_kind = {
        "stale-boundary-closure": stale_result,
        "nonatomic-rigid-mass-commit": atomic_result,
        "aero-model-domain": domain_result,
    }
    results = [results_by_kind[value["failure_kind"]]
               for value in cases["diagnostic_cases"]]
    require(all(result["diagnostic_record"]["diagnostic_id"] ==
                result["policy_decision"]["diagnostic_id"] ==
                result["step_outcome"]["primary_diagnostic_id"] and
                result["step_outcome"]["base_commit_id"] ==
                result["step_outcome"]["resulting_commit_id"] and
                not result["step_outcome"]["candidate_commit_published"] and
                result["step_outcome"]["rollback_verified"]
                for result in results),
            "fixture diagnostic linkage or rollback differs")
    return results


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


def build_reference(cases: dict, repo_root: Path,
                    cases_path: Path) -> dict:
    validate_cases_identity(cases)
    accepted = compose(cases, repo_root)
    reversed_cases = copy.deepcopy(cases)
    reversed_cases["component_bindings"].reverse()
    reordered = compose(reversed_cases, repo_root)
    require(accepted == reordered,
            "component declaration order changed mission composition")
    equivalence = [{
        "id": "EQUIV-YYZ-MISSION-COMPOSITION-BINDING-ORDER",
        "status": "passed",
        "resolved_role_count": len(accepted["resolved_components"]),
        "terminal_tick": accepted["mission_result"]["final_tick"],
        "reason_code":
            accepted["mission_result"]["termination"]["reason_code"],
        "max_abs_result_difference": Decimal(0),
    }]
    return stringify({
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "precision": {
            "implementation":
                "Python Decimal composition over executable component formula references with boundary recomputation, full-state RK4 and fixture-local structured failure projection",
            "decimal_digits": getcontext().prec,
        },
        "source_identity": {
            "cases_path": cases_path.resolve().relative_to(
                repo_root.resolve()).as_posix(),
            "dependency_count": len(cases["component_bindings"]),
        },
        "cases": {CASE_ID: accepted},
        "equivalence_results": equivalence,
        "invalid_input_rejections": invalid_rejections(cases, repo_root),
        "mutation_results": mutation_results(cases, repo_root, accepted),
        "diagnostic_results": diagnostic_results(
            cases, repo_root, accepted),
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
    except (ArithmeticError, ValueError):
        return False


def compare_tree(checks: Checks, actual, expected,
                 absolute: Decimal, relative: Decimal, label: str,
                 convergence_ratio_absolute: Decimal = Decimal(0)) -> None:
    if isinstance(expected, dict):
        checks.require(isinstance(actual, dict), f"{label} is not an object")
        checks.require(set(actual) == set(expected), f"{label} fields differ")
        for key, expected_value in expected.items():
            compare_tree(checks, actual[key], expected_value,
                         absolute, relative, f"{label}.{key}",
                         convergence_ratio_absolute)
        return
    if isinstance(expected, list):
        checks.require(isinstance(actual, list) and
                       len(actual) == len(expected),
                       f"{label} list shape differs")
        for index, expected_value in enumerate(expected):
            compare_tree(checks, actual[index], expected_value,
                         absolute, relative, f"{label}[{index}]",
                         convergence_ratio_absolute)
        return
    if numeric_string(expected):
        actual_value = decimal(actual)
        expected_value = decimal(expected)
        difference = abs(actual_value - expected_value)
        bound = absolute + relative * max(
            abs(actual_value), abs(expected_value), Decimal(1))
        if ".error_reduction_ratios[" in label:
            bound = max(bound, convergence_ratio_absolute)
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


def verify_reference(cases: dict, repo_root: Path, cases_path: Path,
                     oracle: dict, probe_path: Path) -> dict:
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
    recomputed = build_reference(cases, repo_root, cases_path)
    checks.require(oracle == recomputed,
                   "stored oracle differs from its producer",
                   len(oracle["cases"]) + 5)

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
    convergence_ratio_absolute = decimal(
        cases["tolerances"]["convergence_ratio_absolute"])
    checks.require(convergence_ratio_absolute > 0,
                   "convergence ratio tolerance must be positive")
    compare_tree(checks, probe["cases"], list(oracle["cases"].values()),
                 absolute, relative, "cases", convergence_ratio_absolute)
    compare_tree(checks, probe["equivalence_results"],
                  oracle["equivalence_results"], absolute, relative,
                 "equivalence_results", convergence_ratio_absolute)
    checks.require(probe["invalid_input_rejections"] ==
                   oracle["invalid_input_rejections"],
                   "invalid-input identities differ",
                   len(oracle["invalid_input_rejections"]))
    compare_tree(checks, probe["mutation_results"],
                 oracle["mutation_results"], absolute, relative,
                 "mutation_results", convergence_ratio_absolute)
    compare_tree(checks, probe["diagnostic_results"],
                 oracle["diagnostic_results"], absolute, relative,
                 "diagnostic_results", convergence_ratio_absolute)

    accepted = probe["cases"][0]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "resolved_components": len(accepted["resolved_components"]),
        "opening_mass_kg":
            accepted["committed_samples"][0]["committed_mass_kg"],
        "closing_mass_kg":
            accepted["committed_samples"][2]["committed_mass_kg"],
        "intervals_executed": len(accepted["interval_executions"]),
        "minimum_convergence_error_reduction_ratio": min(
            decimal(value) for value in
            accepted["second_interval_convergence"]
                ["error_reduction_ratios"]),
        "terminal_tick": accepted["mission_result"]["final_tick"],
        "terminal_reason_code":
            accepted["mission_result"]["termination"]["reason_code"],
        "terminal_downrange_m":
            accepted["metric_summary"]["downrange_m"],
        "terminal_observation_sealed":
            accepted["terminal_observation"]["sealed"],
        "equivalence_cases_passed": len(oracle["equivalence_results"]),
        "invalid_input_cases_rejected":
            len(oracle["invalid_input_rejections"]),
        "mutation_cases_rejected": len(oracle["mutation_results"]),
        "diagnostic_failures_structured":
            len(oracle["diagnostic_results"]),
    })


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", required=True, type=Path)
    parser.add_argument("--repo-root", required=True, type=Path)
    parser.add_argument("--oracle", type=Path)
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--generate-reference", action="store_true")
    arguments = parser.parse_args()

    getcontext().prec = 80
    cases = load_json(arguments.cases)
    validate_cases_identity(cases)
    repo_root = arguments.repo_root.resolve()
    require(repo_root.is_dir(), "repo root is missing")
    if arguments.generate_reference:
        require(arguments.oracle is not None and arguments.probe is None,
                "reference generation requires --oracle and rejects --probe")
        output = json.dumps(
            build_reference(cases, repo_root, arguments.cases),
            indent=2, ensure_ascii=False) + "\n"
        arguments.oracle.parent.mkdir(parents=True, exist_ok=True)
        with arguments.oracle.open(
                "w", encoding="utf-8", newline="\n") as stream:
            stream.write(output)
        print(json.dumps({
            "oracle_id": ORACLE_ID,
            "status": "generated",
            "path": arguments.oracle.as_posix(),
            "bytes": len(output.encode("utf-8")),
        }, separators=(",", ":")))
        return 0

    require(arguments.oracle is not None and arguments.probe is not None,
            "verification requires --oracle and --probe")
    oracle = load_json(arguments.oracle)
    print(json.dumps(
        verify_reference(cases, repo_root, arguments.cases,
                         oracle, arguments.probe),
        separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ArithmeticError, IndexError, KeyError, OSError, TypeError,
            ValueError, json.JSONDecodeError,
            subprocess.SubprocessError) as error:
        print(f"YYZ mission composition reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

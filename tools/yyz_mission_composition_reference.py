#!/usr/bin/env python3
"""Fixture-local YYZ source-to-result composition reference."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal, InvalidOperation, getcontext
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-MISSION-COMPOSITION-001"
ORACLE_ID = "ORACLE-YYZ-MISSION-COMPOSITION-001"
MODEL_ID = "MODEL-YYZ-FIXTURE-MISSION-COMPOSITION-001"
CASES_SCHEMA = "gnczmkn.yyz-mission-composition-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-mission-composition-reference/1"
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
EVENT_ORDER = [
    "resolve-components",
    "publish-opening-commit",
    "evaluate-opening-boundary",
    "evaluate-frozen-interval",
    "stage-rigid-and-mass-candidates",
    "commit-rigid-and-mass",
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
            model["terminal_tick"] == 1 and
            model["integration_strategy"] == "FrozenInterval" and
            model["commit_policy"] == "atomic-rigid-and-mass" and
            model["evaluation_mode"] == "AtGrid" and
            model["evidence_validity"] == "Valid",
            "mission-composition model profile differs")
    require(len(cases["cases"]) == 1 and
            cases["cases"][0]["id"] == CASE_ID,
            "mission-composition bundle must contain its baseline case")


def validate_case(case: dict) -> None:
    require(case["frozen_interval_case_id"] == FROZEN_CASE_ID and
            isinstance(case["opening_commit_id"], str) and
            bool(case["opening_commit_id"]) and
            isinstance(case["closing_commit_id"], str) and
            bool(case["closing_commit_id"]) and
            case["opening_commit_id"] != case["closing_commit_id"] and
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
            context["valid_until_tick"] == model["terminal_tick"] and
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
                model["terminal_tick"] and
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


def compose(cases: dict, repo_root: Path, *,
            early_mass_visibility: bool = False,
            nonatomic_mass_commit: bool = False,
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

    model = cases["model"]
    dt_s = decimal(model["base_dt_s"])
    opening_state = frozen_input["initial_state"]
    source_opening_mass = decimal(
        frozen_input["mass_properties_sample"]["mass_kg"])
    current_visible_mass = decimal(
        frozen_result["mass_visibility"]["current_visible_mass_kg"])
    candidate_mass = decimal(
        frozen_result["mass_visibility"]["pending_mass_candidate_kg"])
    consumed_mass = decimal(
        frozen_result["mass_visibility"]["consumed_mass_kg"])
    require(current_visible_mass == source_opening_mass and
            candidate_mass == source_opening_mass - consumed_mass and
            decimal(frozen_result["mass_visibility"]["integration_mass_kg"]) ==
                source_opening_mass and
            frozen_result["mass_visibility"]
                ["pending_visibility_before_commit"] == "candidate-only" and
            frozen_result["mass_visibility"]["next_commit_tick"] ==
                model["terminal_tick"],
            "FrozenInterval mass handoff differs")
    terminal_state = frozen_result["analytic_terminal"]
    require(terminal_state["tick"] == model["terminal_tick"] and
            decimal(terminal_state["time_s"]) ==
                Decimal(model["terminal_tick"]) * dt_s,
            "FrozenInterval terminal state identity differs")

    opening_mass = candidate_mass if early_mass_visibility else source_opening_mass
    closing_mass = source_opening_mass if nonatomic_mass_commit else candidate_mass
    opening = committed_sample(
        model["initial_tick"], Decimal(model["initial_tick"]) * dt_s,
        case["opening_commit_id"], opening_state, opening_mass)
    closing = committed_sample(
        model["terminal_tick"], Decimal(model["terminal_tick"]) * dt_s,
        case["closing_commit_id"], terminal_state, closing_mass)
    committed_samples = [opening, closing]

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
    interval_execution = {
        "sample_tick": frozen_result["context"]["sample_tick"],
        "valid_from_tick": frozen_result["context"]["valid_from_tick"],
        "valid_until_tick": frozen_result["context"]["valid_until_tick"],
        "configuration_revision":
            frozen_result["context"]["configuration_revision"],
        "base_dt_s": decimal(frozen_result["context"]["base_dt_s"]),
        "strategy": "FrozenInterval",
        "environment_sample": frozen_result["environment_sample"],
        "air_data": frozen_result["air_data"],
        "aero_lookup": frozen_result["aero_lookup"],
        "closure": frozen_result["closure"],
        "rigid_derivative_at_opening":
            frozen_result["rigid_derivative_at_tick0"],
        "mass_transition": {
            "mass_state_id": MASS_STATE_ID,
            "opening_committed_mass_kg": source_opening_mass,
            "consumed_mass_kg": consumed_mass,
            "pending_mass_candidate_kg": candidate_mass,
            "pending_visibility_before_commit": "candidate-only",
            "closing_committed_mass_kg": closing_mass,
            "closing_commit_kind": "atomic-rigid-and-mass",
        },
    }
    execution_trace = [
        {"order": 0, "event": "resolve-components",
         "sample_tick": 0, "component_count": len(resolved)},
        {"order": 1, "event": "publish-opening-commit",
         "sample_tick": 0, "commit_id": opening["commit_id"]},
        {"order": 2, "event": "evaluate-opening-boundary",
         "sample_tick": 0,
         "action": evaluation_trace[0]["decision"]["action"]},
        {"order": 3, "event": "evaluate-frozen-interval",
         "sample_tick": 0, "valid_until_tick": 1},
        {"order": 4, "event": "stage-rigid-and-mass-candidates",
         "sample_tick": 0, "candidate_tick": 1},
        {"order": 5, "event": "commit-rigid-and-mass",
         "sample_tick": 1, "commit_id": closing["commit_id"]},
        {"order": 6, "event": "evaluate-terminal-boundary",
         "sample_tick": 1,
         "action": terminal_boundary["decision"]["action"]},
        {"order": 7, "event": "seal-terminal-observation",
         "sample_tick": 1, "commit_id": closing["commit_id"]},
        {"order": 8, "event": "freeze-mission-result",
         "sample_tick": 1, "status": final_status},
    ]
    return {
        "id": case["id"],
        "mission_source_id": MISSION_SOURCE_ID,
        "execution_id": EXECUTION_ID,
        "composition_model_id": MODEL_ID,
        "resolved_components": resolved,
        "execution_trace": execution_trace,
        "committed_samples": committed_samples,
        "interval_execution": interval_execution,
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
            "expected_closing_committed_mass_kg":
                accepted["committed_samples"][1]["committed_mass_kg"],
            "observed_closing_committed_mass_kg":
                nonatomic["committed_samples"][1]["committed_mass_kg"],
            "observed_terminal_consumed_mass_kg":
                nonatomic["metric_summary"]["consumed_mass_kg"],
            "max_abs_result_difference": Decimal("0.05"),
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
                "Python Decimal semantic resolver over executable component oracles",
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
        "resolved_components": len(accepted["resolved_components"]),
        "opening_mass_kg":
            accepted["committed_samples"][0]["committed_mass_kg"],
        "closing_mass_kg":
            accepted["committed_samples"][1]["committed_mass_kg"],
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

#!/usr/bin/env python3
"""Independent Decimal reference for committed YYZ run evaluation."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal, InvalidOperation, getcontext
import hashlib
import json
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-RUN-EVALUATION-001"
ORACLE_ID = "ORACLE-YYZ-RUN-EVALUATION-001"
MODEL_ID = "MODEL-YYZ-RUN-EVALUATION-001"
TRAJECTORY_FIXTURE_ID = "REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001"
CASES_SCHEMA = "gnczmkn.yyz-run-evaluation-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-run-evaluation-reference/1"
SUBJECT = "vehicle.fixture.yyz@1"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
INERTIAL_FRAME_ID = "frame.fixture.yyz.inertial-cartesian@1"
SUPPORTED_METRICS = {
    "duration_s", "downrange_m", "remaining_mass_kg"
}
EVENT_ORDER = [
    "publish-committed-sample",
    "evaluate-metrics",
    "evaluate-termination",
    "seal-terminal-observation",
    "freeze-run-outcome",
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


def vector(values: object, label: str) -> list[Decimal]:
    require(isinstance(values, list) and len(values) == 3,
            f"{label} must have three components")
    return [decimal(value) for value in values]


def valid_integer(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def speed(velocity: list[Decimal]) -> Decimal:
    return sum((value * value for value in velocity), Decimal(0)).sqrt()


def validate_cases_identity(cases: dict) -> None:
    require(cases["schema_version"] == CASES_SCHEMA and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model_choice"]["status"] == "accepted",
            "run-evaluation cases identity differs")
    model = cases["model"]
    require(model["model_id"] == MODEL_ID and
            model["trajectory_source_fixture_id"] == TRAJECTORY_FIXTURE_ID and
            model["subject"] == SUBJECT and
            model["clock_domain"] == CLOCK_DOMAIN and
            model["evaluation_mode"] == "AtGrid" and
            model["composition"] == "any" and
            model["priority_policy"] ==
            "higher-integer-then-ascending-predicate-id" and
            model["evidence_validity"] == "Valid",
            "run-evaluation model profile differs")
    require(len(cases["cases"]) == 2,
            "run-evaluation bundle must contain two result cases")


def validate_trajectory(cases: dict) -> None:
    trajectory = cases["trajectory"]
    dt_s = decimal(trajectory["base_dt_s"])
    require(dt_s > 0, "base_dt_s must be positive")
    require(trajectory["inertial_frame_id"] == INERTIAL_FRAME_ID,
            "trajectory inertial frame differs")
    samples = trajectory["samples"]
    require(isinstance(samples, list) and len(samples) >= 2,
            "at least two committed samples are required")
    commit_ids: set[str] = set()
    previous_mass: Decimal | None = None
    for index, sample in enumerate(samples):
        require(valid_integer(sample["sample_tick"]) and
                sample["sample_tick"] == index,
                "sample ticks must be contiguous from zero")
        require(decimal(sample["time_s"]) == Decimal(index) * dt_s,
                "sample time does not match tick * base_dt_s")
        commit_id = sample["commit_id"]
        require(isinstance(commit_id, str) and commit_id and
                commit_id not in commit_ids,
                "commit ids must be nonempty and unique")
        commit_ids.add(commit_id)
        require(sample["quality"] == "Valid",
                "committed sample quality must be Valid")
        vector(sample["position_I_m"], "position")
        vector(sample["velocity_I_mps"], "velocity")
        mass = decimal(sample["committed_mass_kg"])
        require(mass > 0, "committed mass must be positive")
        if previous_mass is not None:
            require(mass <= previous_mass,
                    "committed mass must be nonincreasing")
        previous_mass = mass


def validate_plan(case: dict) -> None:
    require(isinstance(case["id"], str) and case["id"] and
            isinstance(case["plan_id"], str) and case["plan_id"],
            "case and plan identities must be nonempty")
    require(decimal(case["requested_duration_s"]) > 0,
            "requested duration must be positive")
    predicates = case["predicates"]
    require(isinstance(predicates, list) and predicates,
            "termination plan must contain predicates")
    predicate_ids: set[str] = set()
    for predicate in predicates:
        predicate_id = predicate["predicate_id"]
        require(isinstance(predicate_id, str) and predicate_id and
                predicate_id not in predicate_ids,
                "predicate ids must be nonempty and unique")
        predicate_ids.add(predicate_id)
        require(predicate["metric_id"] in SUPPORTED_METRICS,
                "unsupported predicate metric")
        require(predicate["relation"] in {">=", "<="},
                "unsupported predicate relation")
        decimal(predicate["threshold"])
        require(predicate["action"] in {"Complete", "Abort"},
                "unsupported terminal action")
        require(isinstance(predicate["reason_code"], str) and
                predicate["reason_code"],
                "reason_code must be nonempty")
        require(valid_integer(predicate["priority"]) and
                predicate["priority"] >= 0,
                "predicate priority must be a nonnegative integer")


def validate_input(cases: dict) -> None:
    validate_cases_identity(cases)
    validate_trajectory(cases)
    case_ids: set[str] = set()
    for case in cases["cases"]:
        validate_plan(case)
        require(case["id"] not in case_ids, "case ids must be unique")
        case_ids.add(case["id"])


def sample_metrics(sample: dict, initial_sample: dict,
                   visible_mass: Decimal | None = None) -> dict:
    position = vector(sample["position_I_m"], "position")
    initial_position = vector(initial_sample["position_I_m"],
                              "initial position")
    velocity = vector(sample["velocity_I_mps"], "velocity")
    initial_mass = decimal(initial_sample["committed_mass_kg"])
    mass = (decimal(sample["committed_mass_kg"])
            if visible_mass is None else visible_mass)
    return {
        "duration_s": decimal(sample["time_s"]),
        "downrange_m": position[0] - initial_position[0],
        "remaining_mass_kg": mass,
        "consumed_mass_kg": initial_mass - mass,
        "speed_mps": speed(velocity),
    }


def metric_value(metrics: dict, metric_id: str) -> Decimal:
    require(metric_id in SUPPORTED_METRICS,
            f"unsupported metric id: {metric_id}")
    return metrics[metric_id]


def predicate_met(observed: Decimal, threshold: Decimal, relation: str,
                  strict_thresholds: bool) -> bool:
    if relation == ">=":
        return observed > threshold if strict_thresholds else observed >= threshold
    if relation == "<=":
        return observed < threshold if strict_thresholds else observed <= threshold
    raise ValueError(f"unsupported predicate relation: {relation}")


def decision_metrics(metrics: dict) -> dict:
    return {
        "duration_s": metrics["duration_s"],
        "downrange_m": metrics["downrange_m"],
        "remaining_mass_kg": metrics["remaining_mass_kg"],
    }


def continue_decision(metrics: dict) -> dict:
    return {
        "action": "Continue",
        "reason_code": "none",
        "trigger_time_s": metrics["duration_s"],
        "subject": SUBJECT,
        "priority": 0,
        "metrics": decision_metrics(metrics),
        "message_key": "yyz.termination.continue",
        "params": {},
    }


def evaluate_predicates(case: dict, metrics: dict, *,
                        strict_thresholds: bool,
                        low_priority_wins: bool) -> tuple[list[dict], dict]:
    results = []
    for predicate in case["predicates"]:
        observed = metric_value(metrics, predicate["metric_id"])
        threshold = decimal(predicate["threshold"])
        results.append({
            "predicate_id": predicate["predicate_id"],
            "metric_id": predicate["metric_id"],
            "relation": predicate["relation"],
            "threshold": threshold,
            "observed": observed,
            "met": predicate_met(observed, threshold,
                                 predicate["relation"],
                                 strict_thresholds),
            "action": predicate["action"],
            "reason_code": predicate["reason_code"],
            "priority": predicate["priority"],
        })
    results.sort(key=lambda result: result["predicate_id"])
    met = [result for result in results if result["met"]]
    if not met:
        return results, continue_decision(metrics)
    if low_priority_wins:
        selected = min(met, key=lambda result:
                       (result["priority"], result["predicate_id"]))
    else:
        selected = min(met, key=lambda result:
                       (-result["priority"], result["predicate_id"]))
    decision = {
        "action": selected["action"],
        "reason_code": selected["reason_code"],
        "trigger_time_s": metrics["duration_s"],
        "subject": SUBJECT,
        "priority": selected["priority"],
        "metrics": decision_metrics(metrics),
        "message_key": f"yyz.termination.{selected['reason_code']}",
        "params": {},
    }
    return results, decision


def update_summary(summary: dict | None, sample: dict,
                   metrics: dict, count: int) -> dict:
    tick = sample["sample_tick"]
    if summary is None:
        peak_speed = metrics["speed_mps"]
        peak_speed_tick = tick
        maximum_downrange = metrics["downrange_m"]
        maximum_downrange_tick = tick
        minimum_mass = metrics["remaining_mass_kg"]
        minimum_mass_tick = tick
    else:
        peak_speed = summary["peak_speed_mps"]
        peak_speed_tick = summary["peak_speed_tick"]
        if metrics["speed_mps"] > peak_speed:
            peak_speed = metrics["speed_mps"]
            peak_speed_tick = tick
        maximum_downrange = summary["maximum_downrange_m"]
        maximum_downrange_tick = summary["maximum_downrange_tick"]
        if metrics["downrange_m"] > maximum_downrange:
            maximum_downrange = metrics["downrange_m"]
            maximum_downrange_tick = tick
        minimum_mass = summary["minimum_remaining_mass_kg"]
        minimum_mass_tick = summary["minimum_remaining_mass_tick"]
        if metrics["remaining_mass_kg"] < minimum_mass:
            minimum_mass = metrics["remaining_mass_kg"]
            minimum_mass_tick = tick
    return {
        "evaluated_sample_count": count,
        "duration_s": metrics["duration_s"],
        "downrange_m": metrics["downrange_m"],
        "remaining_mass_kg": metrics["remaining_mass_kg"],
        "consumed_mass_kg": metrics["consumed_mass_kg"],
        "terminal_speed_mps": metrics["speed_mps"],
        "peak_speed_mps": peak_speed,
        "peak_speed_tick": peak_speed_tick,
        "maximum_downrange_m": maximum_downrange,
        "maximum_downrange_tick": maximum_downrange_tick,
        "minimum_remaining_mass_kg": minimum_mass,
        "minimum_remaining_mass_tick": minimum_mass_tick,
    }


def evaluate(cases: dict, case: dict, *, early_mass_candidate: bool = False,
             strict_thresholds: bool = False,
             low_priority_wins: bool = False,
             outcome_before_observation: bool = False,
             post_terminal_sample: bool = False,
             require_terminal: bool = True) -> dict:
    validate_trajectory(cases)
    validate_plan(case)
    samples = cases["trajectory"]["samples"]
    initial_sample = samples[0]
    boundaries = []
    summary = None
    terminal_boundary: dict | None = None
    terminal_index: int | None = None

    for index, sample in enumerate(samples):
        visible_mass = None
        if early_mass_candidate and index + 1 < len(samples):
            visible_mass = decimal(samples[index + 1]["committed_mass_kg"])
        metrics = sample_metrics(sample, initial_sample, visible_mass)
        predicate_results, decision = evaluate_predicates(
            case, metrics, strict_thresholds=strict_thresholds,
            low_priority_wins=low_priority_wins)
        boundary = {
            "sample_tick": sample["sample_tick"],
            "time_s": decimal(sample["time_s"]),
            "commit_id": sample["commit_id"],
            "metrics": metrics,
            "predicate_results": predicate_results,
            "decision": decision,
        }
        boundaries.append(boundary)
        summary = update_summary(summary, sample, metrics, len(boundaries))
        if decision["action"] != "Continue" and terminal_boundary is None:
            terminal_boundary = boundary
            terminal_index = index
            if not post_terminal_sample:
                break

    if terminal_boundary is None:
        require(not require_terminal,
                "termination plan did not reach a terminal boundary")
        terminal_boundary = boundaries[-1]
        terminal_index = len(samples)

    event_order = list(EVENT_ORDER)
    if outcome_before_observation:
        event_order[-2:] = ["freeze-run-outcome",
                            "seal-terminal-observation"]
    post_count = max(0, len(boundaries) - int(terminal_index) - 1)
    terminal_observation = {
        "sample_tick": terminal_boundary["sample_tick"],
        "time_s": terminal_boundary["time_s"],
        "commit_id": terminal_boundary["commit_id"],
        "metrics": terminal_boundary["metrics"],
        "decision": terminal_boundary["decision"],
        "event_order": event_order,
        "sealed": not outcome_before_observation,
        "post_terminal_sample_count": post_count,
    }
    action = terminal_boundary["decision"]["action"]
    final_status = {
        "Complete": "Completed",
        "Abort": "Terminated",
        "Continue": "Running",
    }[action]
    outcome = {
        "final_status": final_status,
        "evidence_validity": "Valid",
        "initial_tick": 0,
        "final_tick": terminal_boundary["sample_tick"],
        "requested_duration_s": decimal(case["requested_duration_s"]),
        "final_time_s": terminal_boundary["time_s"],
        "termination": terminal_boundary["decision"],
        "metrics": summary,
        "terminal_observation_sealed": terminal_observation["sealed"],
        "frozen": True,
    }
    return {
        "id": case["id"],
        "plan_id": case["plan_id"],
        "model_id": MODEL_ID,
        "evaluated_boundaries": boundaries,
        "metric_summary": summary,
        "terminal_observation": terminal_observation,
        "run_outcome": outcome,
    }


def invalid_rejections(cases: dict) -> list[str]:
    actions = {
        "INVALID-YYZ-RUN-EVALUATION-NONPOSITIVE-DT":
            lambda value: value["trajectory"].__setitem__("base_dt_s", 0),
        "INVALID-YYZ-RUN-EVALUATION-TICK-GAP":
            lambda value: value["trajectory"]["samples"][2].__setitem__(
                "sample_tick", 3),
        "INVALID-YYZ-RUN-EVALUATION-TIME-MISMATCH":
            lambda value: value["trajectory"]["samples"][1].__setitem__(
                "time_s", "0.11"),
        "INVALID-YYZ-RUN-EVALUATION-DUPLICATE-COMMIT":
            lambda value: value["trajectory"]["samples"][1].__setitem__(
                "commit_id", value["trajectory"]["samples"][0]["commit_id"]),
        "INVALID-YYZ-RUN-EVALUATION-QUALITY":
            lambda value: value["trajectory"]["samples"][1].__setitem__(
                "quality", "Invalid"),
        "INVALID-YYZ-RUN-EVALUATION-MASS-INCREASE":
            lambda value: value["trajectory"]["samples"][1].__setitem__(
                "committed_mass_kg", "120.01"),
        "INVALID-YYZ-RUN-EVALUATION-NONFINITE-STATE":
            lambda value: value["trajectory"]["samples"][1].__setitem__(
                "velocity_I_mps", ["NaN", 0, 0]),
        "INVALID-YYZ-RUN-EVALUATION-DUPLICATE-PREDICATE":
            lambda value: value["cases"][0]["predicates"][1].__setitem__(
                "predicate_id",
                value["cases"][0]["predicates"][0]["predicate_id"]),
        "INVALID-YYZ-RUN-EVALUATION-RELATION":
            lambda value: value["cases"][0]["predicates"][0].__setitem__(
                "relation", ">"),
        "INVALID-YYZ-RUN-EVALUATION-NEGATIVE-PRIORITY":
            lambda value: value["cases"][0]["predicates"][0].__setitem__(
                "priority", -1),
        "INVALID-YYZ-RUN-EVALUATION-METRIC-ID":
            lambda value: value["cases"][0]["predicates"][0].__setitem__(
                "metric_id", "altitude_m"),
        "INVALID-YYZ-RUN-EVALUATION-NO-TERMINAL":
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
            validate_input(mutated)
            for case in mutated["cases"]:
                evaluate(mutated, case)
        except (ArithmeticError, IndexError, KeyError, TypeError, ValueError):
            rejected.append(identifier)
        else:
            raise ValueError(f"invalid input was accepted: {identifier}")
    return rejected


def mutation_results(cases: dict, accepted: dict[str, dict]) -> list[dict]:
    complete_case, abort_case = cases["cases"]
    early = evaluate(cases, abort_case, early_mass_candidate=True)
    strict = evaluate(cases, complete_case, strict_thresholds=True,
                      require_terminal=False)
    low = evaluate(cases, complete_case, low_priority_wins=True)
    order = evaluate(cases, complete_case,
                     outcome_before_observation=True)
    post = evaluate(cases, abort_case, post_terminal_sample=True)
    results = [
        {
            "id": "MUTATION-YYZ-RUN-EVALUATION-EARLY-MASS-CANDIDATE",
            "status": "rejected",
            "expected_terminal_tick":
                accepted[abort_case["id"]]["run_outcome"]["final_tick"],
            "observed_terminal_tick": early["run_outcome"]["final_tick"],
            "observed_remaining_mass_kg":
                early["terminal_observation"]["metrics"]
                ["remaining_mass_kg"],
            "max_abs_result_difference": Decimal(1),
        },
        {
            "id": "MUTATION-YYZ-RUN-EVALUATION-STRICT-THRESHOLDS",
            "status": "rejected",
            "expected_terminal_tick":
                accepted[complete_case["id"]]["run_outcome"]["final_tick"],
            "observed_terminal_tick": len(cases["trajectory"]["samples"]),
            "observed_final_action":
                strict["run_outcome"]["termination"]["action"],
            "max_abs_result_difference": Decimal(1),
        },
        {
            "id": "MUTATION-YYZ-RUN-EVALUATION-LOW-PRIORITY-WINS",
            "status": "rejected",
            "expected_reason_code":
                accepted[complete_case["id"]]["run_outcome"]
                ["termination"]["reason_code"],
            "observed_reason_code":
                low["run_outcome"]["termination"]["reason_code"],
            "expected_priority": 200,
            "observed_priority":
                low["run_outcome"]["termination"]["priority"],
            "max_abs_result_difference": Decimal(100),
        },
        {
            "id": "MUTATION-YYZ-RUN-EVALUATION-OUTCOME-BEFORE-OBSERVATION",
            "status": "rejected",
            "expected_event_order": EVENT_ORDER,
            "observed_event_order":
                order["terminal_observation"]["event_order"],
            "observed_terminal_observation_sealed":
                order["run_outcome"]["terminal_observation_sealed"],
            "max_abs_result_difference": Decimal(1),
        },
        {
            "id": "MUTATION-YYZ-RUN-EVALUATION-POST-TERMINAL-SAMPLE",
            "status": "rejected",
            "expected_evaluated_ticks": [0, 1],
            "observed_evaluated_ticks": [
                boundary["sample_tick"]
                for boundary in post["evaluated_boundaries"]
            ],
            "observed_post_terminal_sample_count":
                post["terminal_observation"]["post_terminal_sample_count"],
            "max_abs_result_difference": Decimal(1),
        },
    ]
    require([entry["id"] for entry in results] ==
            [entry["id"] for entry in cases["mutation_cases"]],
            "mutation identities differ")
    require(all(entry["max_abs_result_difference"] > 0
                for entry in results),
            "a run-evaluation mutation survived")
    return results


def summary_vector(value: dict) -> list[Decimal]:
    summary = value["metric_summary"]
    return [
        decimal(summary["duration_s"]),
        decimal(summary["downrange_m"]),
        decimal(summary["remaining_mass_kg"]),
        decimal(summary["consumed_mass_kg"]),
        decimal(summary["terminal_speed_mps"]),
        decimal(summary["peak_speed_mps"]),
        decimal(summary["maximum_downrange_m"]),
        decimal(summary["minimum_remaining_mass_kg"]),
    ]


def max_difference(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    require(len(lhs) == len(rhs), "metric vector shape differs")
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


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    validate_input(cases)
    accepted = {
        case["id"]: evaluate(cases, case) for case in cases["cases"]
    }
    reversed_cases = copy.deepcopy(cases)
    for case in reversed_cases["cases"]:
        case["predicates"].reverse()
    reversed_results = {
        case["id"]: evaluate(reversed_cases, case)
        for case in reversed_cases["cases"]
    }
    maximum = max(
        (max_difference(summary_vector(accepted[identifier]),
                        summary_vector(reversed_results[identifier]))
         for identifier in accepted), default=Decimal(0))
    require(accepted == reversed_results,
            "predicate declaration order changed evaluation results")
    equivalence = [{
        "id": "EQUIV-YYZ-RUN-EVALUATION-PREDICATE-ORDER",
        "status": "passed",
        "case_ids": list(accepted),
        "terminal_ticks": [
            accepted[identifier]["run_outcome"]["final_tick"]
            for identifier in accepted
        ],
        "reason_codes": [
            accepted[identifier]["run_outcome"]["termination"]["reason_code"]
            for identifier in accepted
        ],
        "max_abs_metric_difference": maximum,
    }]
    return stringify({
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "precision": {
            "implementation":
                "Python standard-library Decimal committed-sample evaluator",
            "decimal_digits": getcontext().prec,
        },
        "input_identity": {
            "path": "fixtures/ref-yyz-run-evaluation/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": accepted,
        "equivalence_results": equivalence,
        "invalid_input_rejections": invalid_rejections(cases),
        "mutation_results": mutation_results(cases, accepted),
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
                   "fixtures/ref-yyz-run-evaluation/cases.json",
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

    complete = probe["cases"][0]
    aborted = probe["cases"][1]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "case_count": len(probe["cases"]),
        "complete_terminal_tick": complete["run_outcome"]["final_tick"],
        "complete_reason_code":
            complete["run_outcome"]["termination"]["reason_code"],
        "complete_downrange_m":
            complete["metric_summary"]["downrange_m"],
        "abort_terminal_tick": aborted["run_outcome"]["final_tick"],
        "abort_reason_code":
            aborted["run_outcome"]["termination"]["reason_code"],
        "terminal_observations_sealed": sum(
            int(case["terminal_observation"]["sealed"])
            for case in probe["cases"]),
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
    validate_input(cases)
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
        print(f"YYZ run evaluation reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

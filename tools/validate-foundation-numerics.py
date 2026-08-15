#!/usr/bin/env python3
"""Validate the R1 foundation RK4 slice against REF-MINIMAL-3DOF-001."""

import argparse
import json
import math
import pathlib
import subprocess
import sys
import tempfile


SCHEMA = "gnczmkn.foundation-numerics-probe/1"
COMPONENT_ID = "GNC-FOUNDATION-NUMERICS-001"
FIXTURE_ID = "REF-MINIMAL-3DOF-001"
MODEL_ID = "MODEL-MINIMAL-3DOF-LINEAR-TRANSLATION-001"
ALGORITHM_ID = "gnc.foundation.ode.classical-rk4-fixed-step@1"
CASE_IDS = {
    "CASE-MIN3D-CONSTANT-ACCELERATION",
    "CASE-MIN3D-LINEAR-DRAG-CONVERGENCE",
    "CASE-MIN3D-EXACT-GRID-TERMINATION",
    "CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE",
}


def reject_duplicate_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key: " + key)
        result[key] = value
    return result


def read_json(path):
    with pathlib.Path(path).open("r", encoding="utf-8") as stream:
        return json.load(stream, object_pairs_hook=reject_duplicate_pairs)


def run_probe(executable):
    with tempfile.TemporaryDirectory(prefix="gnczmkn-foundation-numerics-") as directory:
        report_path = pathlib.Path(directory) / "probe.json"
        completed = subprocess.run(
            [str(executable), "--report", str(report_path)],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        if completed.returncode != 0:
            raise ValueError(
                "foundation numerics probe failed: "
                + completed.stdout
                + completed.stderr
            )
        return read_json(report_path)


def close(actual, expected, absolute, relative=0.0):
    actual_value = float(actual)
    expected_value = float(expected)
    if not math.isfinite(actual_value) or not math.isfinite(expected_value):
        return False
    return abs(actual_value - expected_value) <= absolute + relative * max(
        abs(actual_value), abs(expected_value)
    )


def compare_vector(actual, expected, absolute, relative, label):
    if not isinstance(actual, list) or len(actual) != 3:
        raise ValueError(label + " must contain three values")
    errors = []
    for index in range(3):
        if not close(actual[index], expected[index], absolute, relative):
            raise ValueError(
                f"{label}[{index}] differs: actual={actual[index]} "
                f"expected={expected[index]}"
            )
        errors.append(abs(float(actual[index]) - float(expected[index])))
    return max(errors)


def compare_state(actual, expected_position, expected_velocity, tolerances, label):
    if not isinstance(actual, dict):
        raise ValueError(label + " state is missing")
    position_error = compare_vector(
        actual.get("position_m"),
        expected_position,
        tolerances["position_absolute_m"],
        tolerances["position_relative"],
        label + " position",
    )
    velocity_error = compare_vector(
        actual.get("velocity_mps"),
        expected_velocity,
        tolerances["velocity_absolute_mps"],
        tolerances["velocity_relative"],
        label + " velocity",
    )
    return position_error, velocity_error


def compare_trajectory(actual, expected, tolerances, label):
    if not isinstance(actual, list) or len(actual) != len(expected):
        raise ValueError(label + " trajectory length differs")
    max_position_error = 0.0
    max_velocity_error = 0.0
    for index, expected_record in enumerate(expected):
        actual_record = actual[index]
        if actual_record.get("tick") != expected_record["tick"]:
            raise ValueError(label + f" tick differs at record {index}")
        if not close(
            actual_record.get("time_s"),
            expected_record["time_s"],
            tolerances["time_absolute_s"],
        ):
            raise ValueError(label + f" time differs at record {index}")
        position_error, velocity_error = compare_state(
            actual_record.get("state"),
            expected_record["position_m"],
            expected_record["velocity_mps"],
            tolerances,
            label + f" tick {expected_record['tick']}",
        )
        max_position_error = max(max_position_error, position_error)
        max_velocity_error = max(max_velocity_error, velocity_error)
    return max_position_error, max_velocity_error


def state_errors(actual, expected):
    position = actual.get("position_m")
    velocity = actual.get("velocity_mps")
    if not isinstance(position, list) or len(position) != 3:
        raise ValueError("convergence position is malformed")
    if not isinstance(velocity, list) or len(velocity) != 3:
        raise ValueError("convergence velocity is malformed")
    position_error = max(
        abs(float(position[index]) - float(expected["position_m"][index]))
        for index in range(3)
    )
    velocity_error = max(
        abs(float(velocity[index]) - float(expected["velocity_mps"][index]))
        for index in range(3)
    )
    if not math.isfinite(position_error) or not math.isfinite(velocity_error):
        raise ValueError("convergence error is non-finite")
    return position_error, velocity_error


def case_by_id(cases, case_id):
    matches = [item for item in cases.get("cases", []) if item.get("id") == case_id]
    if len(matches) != 1:
        raise ValueError("fixture case is missing or duplicated: " + case_id)
    return matches[0]


def validate_identity(cases, reference, probe):
    if cases.get("fixture_id") != FIXTURE_ID:
        raise ValueError("fixture identity drifted")
    if cases.get("model", {}).get("model_id") != MODEL_ID:
        raise ValueError("fixture model identity drifted")
    if reference.get("fixture_id") != FIXTURE_ID or reference.get("model_id") != MODEL_ID:
        raise ValueError("oracle identity drifted")
    if probe.get("schema_version") != SCHEMA:
        raise ValueError("foundation probe schema drifted")
    if probe.get("component_id") != COMPONENT_ID:
        raise ValueError("foundation component identity drifted")
    if probe.get("fixture_id") != FIXTURE_ID or probe.get("model_id") != MODEL_ID:
        raise ValueError("foundation fixture/model identity drifted")

    algorithm = probe.get("algorithm", {})
    if algorithm != {
        "id": ALGORITHM_ID,
        "version": "1.0.0",
        "accuracy_order": 4,
        "derivative_evaluations_per_step": 4,
    }:
        raise ValueError("foundation RK4 descriptor drifted")
    if probe.get("policy") != {
        "absolute_tolerance": 1e-12,
        "relative_tolerance": 1e-12,
        "finite_check": "EveryStage",
    }:
        raise ValueError("foundation numerical policy drifted")
    if set(probe.get("cases", {})) != CASE_IDS:
        raise ValueError("foundation case inventory drifted")


def validate_constant(cases, reference, probe):
    tolerances = cases["tolerances"]
    case_id = "CASE-MIN3D-CONSTANT-ACCELERATION"
    return compare_trajectory(
        probe["cases"][case_id].get("trajectory"),
        reference["cases"][case_id]["trajectory"],
        tolerances,
        case_id,
    )


def validate_convergence(cases, reference, probe):
    tolerances = cases["tolerances"]
    case_id = "CASE-MIN3D-LINEAR-DRAG-CONVERGENCE"
    case = case_by_id(cases, case_id)
    runs = probe["cases"][case_id].get("runs")
    if not isinstance(runs, list):
        raise ValueError("foundation convergence runs are missing")
    by_dt = {}
    for run in runs:
        key = str(run.get("dt_s"))
        if key in by_dt:
            raise ValueError("duplicate convergence step: " + key)
        by_dt[key] = run

    analytic = reference["cases"][case_id]["analytic_final"]
    position_errors = []
    velocity_errors = []
    for dt_s in case["dt_ladder_s"]:
        key = str(dt_s)
        if key not in by_dt:
            raise ValueError("missing convergence step: " + key)
        run = by_dt[key]
        expected_ticks = int(round(case["duration_s"] / dt_s))
        if run.get("ticks") != expected_ticks:
            raise ValueError("convergence tick count differs for " + key)
        if run.get("derivative_evaluations") != 4 * expected_ticks:
            raise ValueError("RK4 derivative count differs for " + key)
        position_error, velocity_error = state_errors(
            run.get("final_state", {}), analytic
        )
        position_errors.append(position_error)
        velocity_errors.append(velocity_error)
    if len(by_dt) != len(case["dt_ladder_s"]):
        raise ValueError("unexpected convergence run")

    orders = []
    for index in range(len(position_errors) - 1):
        if not position_errors[index + 1] < position_errors[index]:
            raise ValueError("position error did not strictly decrease")
        if not velocity_errors[index + 1] < velocity_errors[index]:
            raise ValueError("velocity error did not strictly decrease")
        orders.append(math.log2(position_errors[index] / position_errors[index + 1]))
        orders.append(math.log2(velocity_errors[index] / velocity_errors[index + 1]))
    minimum_order = min(orders)
    if minimum_order < tolerances["minimum_observed_order"]:
        raise ValueError(f"observed RK4 order is too low: {minimum_order:.6f}")
    if position_errors[-1] > tolerances["finest_position_error_max_m"]:
        raise ValueError("finest position error exceeds fixture limit")
    if velocity_errors[-1] > tolerances["finest_velocity_error_max_mps"]:
        raise ValueError("finest velocity error exceeds fixture limit")
    return minimum_order, position_errors[-1], velocity_errors[-1]


def validate_termination(cases, reference, probe):
    case_id = "CASE-MIN3D-EXACT-GRID-TERMINATION"
    actual = probe["cases"][case_id]
    compare_trajectory(
        actual.get("trajectory"),
        reference["cases"][case_id]["trajectory"],
        cases["tolerances"],
        case_id,
    )
    if actual.get("terminal") != {
        "predicate": "position.z <= 0 m",
        "first_satisfied_tick": 10,
        "time_s": 5,
    }:
        raise ValueError("foundation termination outcome differs")


def validate_stage_failure(cases, reference, probe):
    case_id = "CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE"
    case = case_by_id(cases, case_id)
    actual = probe["cases"][case_id]
    expected_failure = reference["cases"][case_id]["failure"]
    expected_committed = [
        {
            "tick": 0,
            "time_s": 0,
            "position_m": case["initial_position_m"],
            "velocity_mps": case["initial_velocity_mps"],
        },
        {
            "tick": 1,
            "time_s": case["dt_s"],
            "position_m": expected_failure["last_committed_state"]["position_m"],
            "velocity_mps": expected_failure["last_committed_state"]["velocity_mps"],
        },
    ]
    compare_trajectory(
        actual.get("committed_trajectory"),
        expected_committed,
        cases["tolerances"],
        case_id + " committed",
    )
    failure = actual.get("failure", {})
    expected = {
        "status": "DomainError",
        "detail": "k2",
        "derivative_evaluations": 2,
        "evaluation_time_s": 0.75,
        "failed_step_start_tick": 1,
        "candidate_disposition": "discarded",
        "last_committed_tick": 1,
    }
    if failure != expected:
        raise ValueError("typed RK4 stage failure differs")


def validate_failures(probe):
    expected = {
        "NEGATIVE-DT": ("DomainError", "time-or-dt", 0),
        "NONFINITE-INPUT": ("NonFiniteInput", "state", 0),
        "NONFINITE-DERIVATIVE": ("NonFiniteIntermediate", "k1", 1),
        "INVALID-POLICY": ("DomainError", "policy", 0),
    }
    actual = {}
    for item in probe.get("failure_cases", []):
        identifier = item.get("id")
        if identifier in actual:
            raise ValueError("duplicate foundation failure case")
        actual[identifier] = item
    if set(actual) != set(expected):
        raise ValueError("foundation failure inventory differs")
    for identifier, (status, detail, evaluations) in expected.items():
        item = actual[identifier]
        if item.get("status") != status or item.get("detail") != detail:
            raise ValueError("foundation failure status differs: " + identifier)
        if item.get("evaluations") != evaluations or item.get("has_value") is not False:
            raise ValueError("foundation failure evidence differs: " + identifier)


def validate_tolerances(probe):
    expected = {
        "WITHIN-ABS-REL": ("Success", True, True),
        "OUTSIDE-ABS-REL": ("Success", True, False),
        "NONFINITE-COMPARISON": ("NonFiniteInput", False, False),
    }
    actual = {}
    for item in probe.get("tolerance_cases", []):
        identifier = item.get("id")
        if identifier in actual:
            raise ValueError("duplicate tolerance case")
        actual[identifier] = item
    if set(actual) != set(expected):
        raise ValueError("tolerance case inventory differs")
    for identifier, (status, has_value, accepted) in expected.items():
        item = actual[identifier]
        if item.get("status") != status or item.get("has_value") is not has_value:
            raise ValueError("tolerance status differs: " + identifier)
        if item.get("accepted") is not accepted:
            raise ValueError("tolerance verdict differs: " + identifier)
        if has_value and (
            not math.isfinite(float(item.get("absolute_error")))
            or not math.isfinite(float(item.get("limit")))
        ):
            raise ValueError("tolerance evidence is non-finite: " + identifier)


def validate(cases, reference, probe):
    validate_identity(cases, reference, probe)
    constant_errors = validate_constant(cases, reference, probe)
    convergence = validate_convergence(cases, reference, probe)
    validate_termination(cases, reference, probe)
    validate_stage_failure(cases, reference, probe)
    validate_failures(probe)
    validate_tolerances(probe)
    return {
        "constant_position_error": constant_errors[0],
        "constant_velocity_error": constant_errors[1],
        "minimum_order": convergence[0],
        "finest_position_error": convergence[1],
        "finest_velocity_error": convergence[2],
    }


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cases", required=True, type=pathlib.Path)
    parser.add_argument("--oracle", required=True, type=pathlib.Path)
    parser.add_argument("--probe", required=True, type=pathlib.Path)
    return parser.parse_args()


def main():
    args = parse_args()
    summary = validate(
        read_json(args.cases), read_json(args.oracle), run_probe(args.probe)
    )
    print(
        "foundation numerics oracle validation passed: "
        f"min_order={summary['minimum_order']:.6f} "
        f"finest_position_error_m={summary['finest_position_error']:.3e} "
        f"finest_velocity_error_mps={summary['finest_velocity_error']:.3e}"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("foundation numerics validation error: " + str(error), file=sys.stderr)
        sys.exit(1)

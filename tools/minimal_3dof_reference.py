#!/usr/bin/env python3
"""Independent analytic reference and comparator for REF-MINIMAL-3DOF-001."""

import argparse
import decimal
import json
import math
import pathlib
import subprocess
import sys
import tempfile


FIXTURE_ID = "REF-MINIMAL-3DOF-001"
MODEL_ID = "MODEL-MINIMAL-3DOF-LINEAR-TRANSLATION-001"
CASE_IDS = {
    "CASE-MIN3D-CONSTANT-ACCELERATION",
    "CASE-MIN3D-LINEAR-DRAG-CONVERGENCE",
    "CASE-MIN3D-EXACT-GRID-TERMINATION",
    "CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE",
}
REFERENCE_SCHEMA = "gnczmkn.minimal-3dof-reference/1"
PROBE_SCHEMA = "gnczmkn.minimal-3dof-probe/1"
DECIMAL_PRECISION = 50


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


def write_json(path, value):
    content = json.dumps(value, indent=2, allow_nan=False) + "\n"
    with pathlib.Path(path).open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(content)


def as_decimal(value):
    if isinstance(value, decimal.Decimal):
        if not value.is_finite():
            raise ValueError("numeric value must be finite")
        return value
    if isinstance(value, bool) or not isinstance(value, (int, float, str)):
        raise ValueError("numeric value has an unsupported type")
    result = decimal.Decimal(str(value))
    if not result.is_finite():
        raise ValueError("numeric value must be finite")
    return result


def decimal_vector(values):
    if not isinstance(values, list) or len(values) != 3:
        raise ValueError("3DoF vector must contain exactly three values")
    return [as_decimal(value) for value in values]


def decimal_text(value):
    if value.is_zero():
        return "0"
    return format(value.normalize(), "f")


def vector_add(lhs, rhs):
    return [lhs[index] + rhs[index] for index in range(3)]


def vector_subtract(lhs, rhs):
    return [lhs[index] - rhs[index] for index in range(3)]


def vector_scale(values, factor):
    return [value * factor for value in values]


def analytic_state(case, time_s):
    position0 = decimal_vector(case["initial_position_m"])
    velocity0 = decimal_vector(case["initial_velocity_mps"])
    acceleration = decimal_vector(case["acceleration_mps2"])
    drag_rate = as_decimal(case["drag_rate_per_s"])
    tau = as_decimal(time_s)
    if tau < 0 or drag_rate < 0:
        raise ValueError("analytic model domain requires nonnegative time and drag")

    if drag_rate == 0:
        velocity = vector_add(velocity0, vector_scale(acceleration, tau))
        position = vector_add(
            vector_add(position0, vector_scale(velocity0, tau)),
            vector_scale(acceleration, tau * tau / decimal.Decimal(2)),
        )
        return position, velocity

    decay = (-drag_rate * tau).exp()
    velocity_infinity = vector_scale(acceleration, decimal.Decimal(1) / drag_rate)
    velocity_delta = vector_subtract(velocity0, velocity_infinity)
    velocity = vector_add(velocity_infinity, vector_scale(velocity_delta, decay))
    position = vector_add(
        vector_add(position0, vector_scale(velocity_infinity, tau)),
        vector_scale(
            velocity_delta, (decimal.Decimal(1) - decay) / drag_rate
        ),
    )
    return position, velocity


def exact_ticks(duration_s, dt_s):
    duration = as_decimal(duration_s)
    dt = as_decimal(dt_s)
    if duration < 0 or dt <= 0:
        raise ValueError("duration and dt must define a nonnegative exact grid")
    ratio = duration / dt
    integral = ratio.to_integral_value()
    if ratio != integral:
        raise ValueError("duration is not an exact multiple of dt")
    return int(integral)


def reference_record(tick, dt_s, state):
    time_s = as_decimal(dt_s) * tick
    position, velocity = state
    return {
        "tick": tick,
        "time_s": decimal_text(time_s),
        "position_m": [decimal_text(value) for value in position],
        "velocity_mps": [decimal_text(value) for value in velocity],
    }


def analytic_trajectory(case, dt_s, duration_s):
    ticks = exact_ticks(duration_s, dt_s)
    return [
        reference_record(
            tick,
            dt_s,
            analytic_state(case, as_decimal(dt_s) * tick),
        )
        for tick in range(ticks + 1)
    ]


def validate_case_payload(cases):
    if cases.get("schema_version") != "gnczmkn.minimal-3dof-cases/1":
        raise ValueError("minimal 3DoF case schema identity drifted")
    if cases.get("fixture_id") != FIXTURE_ID:
        raise ValueError("minimal 3DoF fixture identity drifted")
    model = cases.get("model", {})
    if model.get("model_id") != MODEL_ID:
        raise ValueError("minimal 3DoF model identity drifted")
    if model.get("frame_id") != "frame.fixture.minimal3dof.inertial@1":
        raise ValueError("minimal 3DoF frame identity drifted")
    if model.get("time_origin_s") != 0.0:
        raise ValueError("minimal 3DoF time origin must remain zero")

    items = cases.get("cases")
    if not isinstance(items, list):
        raise ValueError("minimal 3DoF cases must be an array")
    by_id = {}
    for case in items:
        case_id = case.get("id")
        if case_id in by_id:
            raise ValueError("duplicate minimal 3DoF case id: " + str(case_id))
        by_id[case_id] = case
    if set(by_id) != CASE_IDS:
        raise ValueError("minimal 3DoF case inventory drifted")

    for case in items:
        decimal_vector(case["initial_position_m"])
        decimal_vector(case["initial_velocity_mps"])
        decimal_vector(case["acceleration_mps2"])
        if as_decimal(case["drag_rate_per_s"]) < 0:
            raise ValueError("drag rate must be nonnegative")

    constant = by_id["CASE-MIN3D-CONSTANT-ACCELERATION"]
    exact_ticks(constant["duration_s"], constant["dt_s"])
    convergence = by_id["CASE-MIN3D-LINEAR-DRAG-CONVERGENCE"]
    ladder = [as_decimal(value) for value in convergence["dt_ladder_s"]]
    if len(ladder) < 3 or any(ladder[index + 1] * 2 != ladder[index] for index in range(len(ladder) - 1)):
        raise ValueError("convergence dt ladder must halve at each entry")
    for dt_s in ladder:
        exact_ticks(convergence["duration_s"], dt_s)
    termination = by_id["CASE-MIN3D-EXACT-GRID-TERMINATION"]
    exact_ticks(termination["maximum_duration_s"], termination["dt_s"])
    if termination["terminal_predicate"] != {
        "field": "position.z",
        "operator": "<=",
        "value_m": 0.0,
    }:
        raise ValueError("termination predicate drifted")
    failure = by_id["CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE"]
    exact_ticks(failure["maximum_duration_s"], failure["dt_s"])
    if failure["expected_failure_stage"] != "k2":
        raise ValueError("stage-domain failure must occur at k2")
    return by_id


def generate_reference(cases):
    by_id = validate_case_payload(cases)
    constant = by_id["CASE-MIN3D-CONSTANT-ACCELERATION"]
    convergence = by_id["CASE-MIN3D-LINEAR-DRAG-CONVERGENCE"]
    termination = by_id["CASE-MIN3D-EXACT-GRID-TERMINATION"]
    failure = by_id["CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE"]

    convergence_final = analytic_state(convergence, convergence["duration_s"])
    convergence_reference_trajectory = analytic_trajectory(
        convergence,
        convergence["reference_trajectory_dt_s"],
        convergence["duration_s"],
    )

    termination_trajectory = []
    maximum_ticks = exact_ticks(
        termination["maximum_duration_s"], termination["dt_s"]
    )
    terminal_tick = None
    for tick in range(maximum_ticks + 1):
        state = analytic_state(
            termination, as_decimal(termination["dt_s"]) * tick
        )
        termination_trajectory.append(
            reference_record(tick, termination["dt_s"], state)
        )
        if state[0][2] <= as_decimal(
            termination["terminal_predicate"]["value_m"]
        ):
            terminal_tick = tick
            break
    if terminal_tick is None:
        raise ValueError("analytic termination predicate was not satisfied")

    failure_last_tick = 1
    failure_last_state = analytic_state(
        failure, as_decimal(failure["dt_s"]) * failure_last_tick
    )

    return {
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "model_id": MODEL_ID,
        "reference_method": {
            "implementation": "CPython standard library decimal",
            "precision_digits": DECIMAL_PRECISION,
            "solution": "closed form for constant acceleration and isotropic linear drag",
        },
        "cases": {
            constant["id"]: {
                "trajectory": analytic_trajectory(
                    constant, constant["dt_s"], constant["duration_s"]
                ),
                "terminal": {
                    "kind": "duration",
                    "tick": exact_ticks(constant["duration_s"], constant["dt_s"]),
                    "time_s": decimal_text(as_decimal(constant["duration_s"])),
                },
            },
            convergence["id"]: {
                "reference_trajectory_dt_s": decimal_text(
                    as_decimal(convergence["reference_trajectory_dt_s"])
                ),
                "analytic_trajectory": convergence_reference_trajectory,
                "analytic_final": {
                    "time_s": decimal_text(as_decimal(convergence["duration_s"])),
                    "position_m": [decimal_text(value) for value in convergence_final[0]],
                    "velocity_mps": [decimal_text(value) for value in convergence_final[1]],
                },
            },
            termination["id"]: {
                "trajectory": termination_trajectory,
                "terminal": {
                    "predicate": "position.z <= 0 m",
                    "first_satisfied_tick": terminal_tick,
                    "time_s": decimal_text(
                        as_decimal(termination["dt_s"]) * terminal_tick
                    ),
                },
            },
            failure["id"]: {
                "failure": {
                    "code": "reference-domain-error",
                    "stage": failure["expected_failure_stage"],
                    "evaluation_time_s": decimal_text(
                        as_decimal(failure["evaluation_time_strictly_less_than_s"])
                    ),
                    "failed_step_start_tick": 1,
                    "candidate_disposition": "discarded",
                    "last_committed_tick": failure_last_tick,
                    "last_committed_state": {
                        "position_m": [decimal_text(value) for value in failure_last_state[0]],
                        "velocity_mps": [decimal_text(value) for value in failure_last_state[1]],
                    },
                }
            },
        },
        "invalid_input_cases": cases["invalid_input_cases"],
    }


def close(actual, expected, absolute, relative):
    if not math.isfinite(actual):
        return False
    expected_float = float(as_decimal(expected))
    return abs(actual - expected_float) <= absolute + relative * max(
        abs(actual), abs(expected_float)
    )


def compare_vector(actual, expected, absolute, relative, label):
    if not isinstance(actual, list) or len(actual) != 3:
        raise ValueError(label + " must contain three numeric values")
    errors = []
    for index in range(3):
        actual_value = float(actual[index])
        expected_value = float(as_decimal(expected[index]))
        if not close(actual_value, expected[index], absolute, relative):
            raise ValueError(
                f"{label}[{index}] differs: actual={actual_value:.17g} "
                f"expected={expected_value:.17g}"
            )
        errors.append(abs(actual_value - expected_value))
    return max(errors)


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
            float(actual_record.get("time_s")),
            expected_record["time_s"],
            tolerances["time_absolute_s"],
            0.0,
        ):
            raise ValueError(label + f" time differs at tick {expected_record['tick']}")
        state = actual_record.get("state", {})
        max_position_error = max(
            max_position_error,
            compare_vector(
                state.get("position_m"),
                expected_record["position_m"],
                tolerances["position_absolute_m"],
                tolerances["position_relative"],
                label + f" position at tick {expected_record['tick']}",
            ),
        )
        max_velocity_error = max(
            max_velocity_error,
            compare_vector(
                state.get("velocity_mps"),
                expected_record["velocity_mps"],
                tolerances["velocity_absolute_mps"],
                tolerances["velocity_relative"],
                label + f" velocity at tick {expected_record['tick']}",
            ),
        )
    return max_position_error, max_velocity_error


def compare_exact_input(actual, expected, fields, label):
    if not isinstance(actual, dict):
        raise ValueError(label + " input echo is missing")
    for field in fields:
        if actual.get(field) != expected.get(field):
            raise ValueError(label + " input echo differs for " + field)


def state_errors(actual_state, expected_state):
    position = actual_state.get("position_m")
    velocity = actual_state.get("velocity_mps")
    if not isinstance(position, list) or len(position) != 3:
        raise ValueError("convergence final position is malformed")
    if not isinstance(velocity, list) or len(velocity) != 3:
        raise ValueError("convergence final velocity is malformed")
    position_error = max(
        abs(float(position[index]) - float(as_decimal(expected_state["position_m"][index])))
        for index in range(3)
    )
    velocity_error = max(
        abs(float(velocity[index]) - float(as_decimal(expected_state["velocity_mps"][index])))
        for index in range(3)
    )
    if not math.isfinite(position_error) or not math.isfinite(velocity_error):
        raise ValueError("convergence error is non-finite")
    return position_error, velocity_error


def validate_probe(cases, reference, probe):
    if probe.get("schema_version") != PROBE_SCHEMA:
        raise ValueError("C++ minimal 3DoF probe schema identity drifted")
    if probe.get("fixture_id") != FIXTURE_ID or probe.get("model_id") != MODEL_ID:
        raise ValueError("C++ minimal 3DoF probe identity drifted")
    if probe.get("algorithm") != "classical-rk4-fixed-step":
        raise ValueError("C++ minimal 3DoF algorithm identity drifted")

    by_id = validate_case_payload(cases)
    probe_cases = probe.get("cases")
    if not isinstance(probe_cases, dict) or set(probe_cases) != CASE_IDS:
        raise ValueError("C++ minimal 3DoF case inventory drifted")
    tolerances = cases["tolerances"]

    fixed_input_fields = [
        "initial_position_m",
        "initial_velocity_mps",
        "acceleration_mps2",
        "drag_rate_per_s",
        "dt_s",
    ]

    constant_id = "CASE-MIN3D-CONSTANT-ACCELERATION"
    constant_probe = probe_cases[constant_id]
    constant_case = by_id[constant_id]
    compare_exact_input(
        constant_probe.get("input"),
        constant_case,
        fixed_input_fields + ["duration_s"],
        constant_id,
    )
    trajectory_errors = compare_trajectory(
        constant_probe.get("trajectory"),
        reference["cases"][constant_id]["trajectory"],
        tolerances,
        constant_id,
    )

    convergence_id = "CASE-MIN3D-LINEAR-DRAG-CONVERGENCE"
    convergence_probe = probe_cases[convergence_id]
    convergence_case = by_id[convergence_id]
    compare_exact_input(
        convergence_probe.get("input"),
        convergence_case,
        [
            "initial_position_m",
            "initial_velocity_mps",
            "acceleration_mps2",
            "drag_rate_per_s",
            "duration_s",
            "dt_ladder_s",
        ],
        convergence_id,
    )
    runs = convergence_probe.get("runs")
    if not isinstance(runs, list):
        raise ValueError("convergence runs are missing")
    runs_by_dt = {}
    for run in runs:
        dt_key = str(run.get("dt_s"))
        if dt_key in runs_by_dt:
            raise ValueError("duplicate convergence dt: " + dt_key)
        runs_by_dt[dt_key] = run

    analytic_final = reference["cases"][convergence_id]["analytic_final"]
    position_errors = []
    velocity_errors = []
    for dt_value in convergence_case["dt_ladder_s"]:
        dt_key = str(dt_value)
        if dt_key not in runs_by_dt:
            raise ValueError("missing convergence dt: " + dt_key)
        run = runs_by_dt[dt_key]
        expected_ticks = exact_ticks(convergence_case["duration_s"], dt_value)
        if run.get("ticks") != expected_ticks:
            raise ValueError("convergence tick count differs for dt " + dt_key)
        position_error, velocity_error = state_errors(
            run.get("final_state", {}), analytic_final
        )
        position_errors.append(position_error)
        velocity_errors.append(velocity_error)
    if len(runs_by_dt) != len(convergence_case["dt_ladder_s"]):
        raise ValueError("unexpected convergence dt run")

    position_orders = []
    velocity_orders = []
    for index in range(len(position_errors) - 1):
        if not position_errors[index + 1] < position_errors[index]:
            raise ValueError("position convergence error did not strictly decrease")
        if not velocity_errors[index + 1] < velocity_errors[index]:
            raise ValueError("velocity convergence error did not strictly decrease")
        position_orders.append(math.log2(position_errors[index] / position_errors[index + 1]))
        velocity_orders.append(math.log2(velocity_errors[index] / velocity_errors[index + 1]))
    minimum_order = min(position_orders + velocity_orders)
    if minimum_order < tolerances["minimum_observed_order"]:
        raise ValueError(
            f"observed RK4 order {minimum_order:.6f} is below "
            f"{tolerances['minimum_observed_order']:.6f}"
        )
    if position_errors[-1] > tolerances["finest_position_error_max_m"]:
        raise ValueError("finest-grid position error exceeds the fixture limit")
    if velocity_errors[-1] > tolerances["finest_velocity_error_max_mps"]:
        raise ValueError("finest-grid velocity error exceeds the fixture limit")

    termination_id = "CASE-MIN3D-EXACT-GRID-TERMINATION"
    termination_probe = probe_cases[termination_id]
    termination_case = by_id[termination_id]
    compare_exact_input(
        termination_probe.get("input"),
        termination_case,
        fixed_input_fields + ["maximum_duration_s"],
        termination_id,
    )
    compare_trajectory(
        termination_probe.get("trajectory"),
        reference["cases"][termination_id]["trajectory"],
        tolerances,
        termination_id,
    )
    if termination_probe.get("terminal") != {
        "predicate": "position.z <= 0 m",
        "first_satisfied_tick": 10,
        "time_s": 5,
    }:
        raise ValueError("termination outcome differs")

    failure_id = "CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE"
    failure_probe = probe_cases[failure_id]
    failure_case = by_id[failure_id]
    compare_exact_input(
        failure_probe.get("input"),
        failure_case,
        fixed_input_fields + ["maximum_duration_s"],
        failure_id,
    )
    if failure_probe.get("evaluation_time_strictly_less_than_s") != failure_case[
        "evaluation_time_strictly_less_than_s"
    ]:
        raise ValueError("stage failure evaluation domain differs")
    committed = failure_probe.get("committed_trajectory")
    failure_reference = reference["cases"][failure_id]["failure"]
    expected_committed = [
        reference_record(0, failure_case["dt_s"], analytic_state(failure_case, 0)),
        reference_record(
            1,
            failure_case["dt_s"],
            analytic_state(failure_case, failure_case["dt_s"]),
        ),
    ]
    compare_trajectory(
        committed, expected_committed, tolerances, failure_id + " committed"
    )
    actual_failure = failure_probe.get("failure", {})
    for field in (
        "code",
        "stage",
        "failed_step_start_tick",
        "candidate_disposition",
        "last_committed_tick",
    ):
        if actual_failure.get(field) != failure_reference[field]:
            raise ValueError("stage failure fact differs for " + field)
    if not close(
        float(actual_failure.get("evaluation_time_s")),
        failure_reference["evaluation_time_s"],
        tolerances["time_absolute_s"],
        0.0,
    ):
        raise ValueError("stage failure evaluation time differs")

    expected_invalid = {
        item["id"]: item["expected_status"]
        for item in cases["invalid_input_cases"]
    }
    actual_invalid = {}
    for item in probe.get("invalid_input_cases", []):
        if item.get("id") in actual_invalid:
            raise ValueError("duplicate invalid-input result")
        actual_invalid[item.get("id")] = item.get("status")
    if actual_invalid != expected_invalid:
        raise ValueError("invalid-input results differ")

    return {
        "constant_max_position_error_m": trajectory_errors[0],
        "constant_max_velocity_error_mps": trajectory_errors[1],
        "convergence_minimum_order": minimum_order,
        "finest_position_error_m": position_errors[-1],
        "finest_velocity_error_mps": velocity_errors[-1],
        "terminal_tick": termination_probe["terminal"]["first_satisfied_tick"],
        "failure_stage": actual_failure["stage"],
    }


def run_probe(executable):
    with tempfile.TemporaryDirectory(prefix="gnczmkn-minimal-3dof-") as directory:
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
                "C++ minimal 3DoF probe failed: "
                + completed.stdout
                + completed.stderr
            )
        return read_json(report_path)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cases", required=True, type=pathlib.Path)
    parser.add_argument("--oracle", required=True, type=pathlib.Path)
    parser.add_argument("--probe", type=pathlib.Path)
    parser.add_argument("--write-oracle", action="store_true")
    return parser.parse_args()


def main():
    args = parse_args()
    decimal.getcontext().prec = DECIMAL_PRECISION
    cases = read_json(args.cases)
    generated = generate_reference(cases)
    if args.write_oracle:
        write_json(args.oracle, generated)
    else:
        stored = read_json(args.oracle)
        if stored != generated:
            raise ValueError(
                "stored minimal 3DoF oracle differs from the independent reference"
            )

    if args.probe is None:
        print("minimal 3DoF analytic oracle check passed")
        return 0

    summary = validate_probe(cases, generated, run_probe(args.probe))
    print(
        "minimal 3DoF oracle validation passed: "
        f"min_order={summary['convergence_minimum_order']:.6f} "
        f"finest_position_error_m={summary['finest_position_error_m']:.3e} "
        f"finest_velocity_error_mps={summary['finest_velocity_error_mps']:.3e} "
        f"terminal_tick={summary['terminal_tick']} "
        f"failure_stage={summary['failure_stage']}"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, decimal.DecimalException) as error:
        print("minimal 3DoF reference error: " + str(error), file=sys.stderr)
        sys.exit(1)

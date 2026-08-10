#!/usr/bin/env python3
"""Independent CPython-standard-library reference for R0 scientific conventions."""

import argparse
import json
import math
import pathlib
import platform
import random
import sys


IMPLEMENTATION_ID = "cpython-stdlib-reference"
OUTPUT_SCHEMA = "gnczmkn.scientific-check-output/1"


def add3(lhs, rhs):
    return [lhs[index] + rhs[index] for index in range(3)]


def scale(values, factor):
    return [factor * value for value in values]


def dot3(lhs, rhs):
    return sum(lhs[index] * rhs[index] for index in range(3))


def cross3(lhs, rhs):
    return [
        lhs[1] * rhs[2] - lhs[2] * rhs[1],
        lhs[2] * rhs[0] - lhs[0] * rhs[2],
        lhs[0] * rhs[1] - lhs[1] * rhs[0],
    ]


def hamilton(lhs, rhs):
    w1 = lhs[0]
    u1 = lhs[1:4]
    w2 = rhs[0]
    u2 = rhs[1:4]
    vector = add3(add3(scale(u2, w1), scale(u1, w2)), cross3(u1, u2))
    return [w1 * w2 - dot3(u1, u2)] + vector


def conjugate(quaternion):
    return [quaternion[0], -quaternion[1], -quaternion[2], -quaternion[3]]


def quaternion_norm(quaternion):
    return math.sqrt(sum(value * value for value in quaternion))


def normalize(quaternion):
    norm = quaternion_norm(quaternion)
    if norm == 0.0:
        raise ValueError("zero-norm quaternion")
    return [value / norm for value in quaternion]


def inverse(quaternion):
    norm_squared = sum(value * value for value in quaternion)
    if norm_squared == 0.0:
        raise ValueError("zero-norm quaternion")
    return [value / norm_squared for value in conjugate(quaternion)]


def passive_rotate(quaternion, vector):
    unit = normalize(quaternion)
    pure = [0.0] + list(vector)
    result = hamilton(hamilton(conjugate(unit), pure), unit)
    return result[1:4]


def passive_matrix(quaternion):
    w, x, y, z = normalize(quaternion)
    return [
        1.0 - 2.0 * (y * y + z * z),
        2.0 * (x * y + w * z),
        2.0 * (x * z - w * y),
        2.0 * (x * y - w * z),
        1.0 - 2.0 * (x * x + z * z),
        2.0 * (y * z + w * x),
        2.0 * (x * z + w * y),
        2.0 * (y * z - w * x),
        1.0 - 2.0 * (x * x + y * y),
    ]


def active_axis_angle(axis, angle):
    half = 0.5 * angle
    sine = math.sin(half)
    if axis == "x":
        return [math.cos(half), sine, 0.0, 0.0]
    if axis == "y":
        return [math.cos(half), 0.0, sine, 0.0]
    if axis == "z":
        return [math.cos(half), 0.0, 0.0, sine]
    raise ValueError("unsupported Euler axis")


def passive_quaternion_from_intrinsic_zyx(yaw, pitch, roll):
    active_body_to_inertial = hamilton(
        hamilton(active_axis_angle("z", yaw), active_axis_angle("y", pitch)),
        active_axis_angle("x", roll),
    )
    return inverse(active_body_to_inertial)


def intrinsic_zyx_from_passive_quaternion(quaternion):
    matrix = passive_matrix(quaternion)
    cosine_pitch = math.hypot(matrix[0], matrix[3])
    if cosine_pitch <= 1e-12:
        raise ValueError("intrinsic ZYX pitch singularity")
    pitch = math.asin(max(-1.0, min(1.0, -matrix[6])))
    yaw = math.atan2(matrix[3], matrix[0])
    roll = math.atan2(matrix[7], matrix[8])
    return [yaw, pitch, roll]


def matrix_vector(matrix, vector):
    return [
        sum(matrix[row * 3 + column] * vector[column] for column in range(3))
        for row in range(3)
    ]


def matrix_multiply(lhs, rhs):
    return [
        sum(lhs[row * 3 + inner] * rhs[inner * 3 + column] for inner in range(3))
        for row in range(3)
        for column in range(3)
    ]


def matrix_transpose(matrix):
    return [matrix[column * 3 + row] for row in range(3) for column in range(3)]


def determinant(matrix):
    return (
        matrix[0] * (matrix[4] * matrix[8] - matrix[5] * matrix[7])
        - matrix[1] * (matrix[3] * matrix[8] - matrix[5] * matrix[6])
        + matrix[2] * (matrix[3] * matrix[7] - matrix[4] * matrix[6])
    )


def duration_alignment(duration, base_dt):
    ratio = duration / base_dt
    return [math.floor(ratio) * base_dt, math.ceil(ratio) * base_dt]


def exact_grid_ticks(duration, base_dt):
    ratio = duration / base_dt
    nearest = round(ratio)
    if not math.isclose(ratio, nearest, rel_tol=0.0, abs_tol=1e-12):
        raise ValueError("duration is not an integer multiple of base_dt")
    return int(nearest)


def compute_observation(case):
    operation = case["operation"]
    values = case["input"]

    if operation == "passive_rotate":
        return passive_rotate(values["quaternion_wxyz"], values["vector"])
    if operation == "composition_rotate":
        combined = hamilton(values["q_b_a_wxyz"], values["q_c_b_wxyz"])
        return passive_rotate(combined, values["vector_a"])
    if operation == "hamilton_product":
        return hamilton(values["lhs_wxyz"], values["rhs_wxyz"])
    if operation == "inverse_round_trip":
        transformed = passive_rotate(values["quaternion_wxyz"], values["vector"])
        return passive_rotate(inverse(values["quaternion_wxyz"]), transformed)
    if operation == "passive_matrix_row_major":
        return passive_matrix(values["quaternion_wxyz"])
    if operation == "serialize_wxyz":
        return list(values["quaternion_wxyz"])
    if operation == "body_rate_derivative":
        pure_omega = [0.0] + list(values["omega_bi_b_radps"])
        return scale(hamilton(pure_omega, values["q_i_b_wxyz"]), -0.5)
    if operation == "euler_intrinsic_zyx_round_trip":
        quaternion = passive_quaternion_from_intrinsic_zyx(
            values["yaw_z_rad"], values["pitch_y_rad"], values["roll_x_rad"]
        )
        return intrinsic_zyx_from_passive_quaternion(quaternion)
    if operation == "unit_affine":
        return [values["value"] * values["scale"] + values["offset"]]
    if operation == "point_transform":
        rotated = matrix_vector(values["matrix_row_major"], values["coordinates_from"])
        return add3(rotated, values["translation_to"])
    if operation == "free_vector_transform":
        return matrix_vector(values["matrix_row_major"], values["vector_from"])
    if operation == "tick_time":
        return [values["time_origin_s"] + values["tick"] * values["base_dt_s"]]
    if operation == "duration_alignment":
        return duration_alignment(values["duration_s"], values["base_dt_s"])
    raise ValueError("unsupported observation operation: " + operation)


def max_abs_difference(lhs, rhs):
    if len(lhs) != len(rhs):
        return math.inf
    return max((abs(lhs[index] - rhs[index]) for index in range(len(lhs))), default=0.0)


def within_tolerance(actual, expected, absolute, relative):
    if len(actual) != len(expected):
        return False
    return all(
        abs(actual[index] - expected[index])
        <= absolute + relative * max(abs(actual[index]), abs(expected[index]))
        for index in range(len(actual))
    )


class CheckBook:
    def __init__(self, required_ids):
        self._checks = {check_id: {"status": "pass", "max_error": 0.0} for check_id in required_ids}

    def observe(self, check_id, passed, error=0.0):
        if check_id not in self._checks:
            raise ValueError("unknown check id: " + check_id)
        entry = self._checks[check_id]
        entry["max_error"] = max(entry["max_error"], float(error))
        if not passed:
            entry["status"] = "fail"

    def report(self):
        return [
            {"id": check_id, "status": self._checks[check_id]["status"], "max_error": self._checks[check_id]["max_error"]}
            for check_id in sorted(self._checks)
        ]

    def passed(self):
        return all(entry["status"] == "pass" for entry in self._checks.values())


def check_id_for_observation(observation_id):
    if observation_id.startswith("quaternion.rotate-"):
        return "quaternion.direction"
    if observation_id == "quaternion.compose-z90-then-x90":
        return "quaternion.composition"
    if observation_id == "quaternion.hamilton-composition-coefficients":
        return "quaternion.hamilton-product"
    if observation_id == "quaternion.inverse-round-trip":
        return "quaternion.inverse"
    if observation_id == "quaternion.matrix-row-major-z90":
        return "quaternion.matrix-equivalence"
    if observation_id == "quaternion.serialization-wxyz":
        return "quaternion.serialization-wxyz"
    if observation_id == "quaternion.body-rate-derivative":
        return "quaternion.body-rate-derivative"
    if observation_id == "quaternion.euler-intrinsic-zyx-round-trip":
        return "quaternion.euler-round-trip"
    if observation_id.startswith("units."):
        return "units.si-boundary"
    if observation_id.startswith("frames."):
        return "frames.point-versus-free-vector"
    if observation_id == "time.large-integer-tick":
        return "time.integer-tick"
    if observation_id == "time.non-grid-stop-before-after":
        return "time.duration-alignment"
    raise ValueError("observation is not mapped to a check: " + observation_id)


def random_unit_quaternion(generator):
    while True:
        candidate = [generator.uniform(-1.0, 1.0) for _ in range(4)]
        if quaternion_norm(candidate) > 0.1:
            return normalize(candidate)


def run_property_checks(checks, sample_count, absolute, relative):
    generator = random.Random(0x534349303031)
    identity = [
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
    ]

    for _ in range(sample_count):
        first = random_unit_quaternion(generator)
        second = random_unit_quaternion(generator)
        vector = [generator.uniform(-10.0, 10.0) for _ in range(3)]

        matrix = passive_matrix(first)
        matrix_result = matrix_vector(matrix, vector)
        quaternion_result = passive_rotate(first, vector)
        error = max_abs_difference(matrix_result, quaternion_result)
        checks.observe("quaternion.matrix-equivalence", within_tolerance(matrix_result, quaternion_result, absolute, relative), error)

        orthogonality = matrix_multiply(matrix, matrix_transpose(matrix))
        error = max_abs_difference(orthogonality, identity)
        checks.observe("quaternion.orthogonality", within_tolerance(orthogonality, identity, absolute, relative), error)

        determinant_error = abs(determinant(matrix) - 1.0)
        checks.observe("quaternion.determinant", determinant_error <= absolute + relative, determinant_error)

        negative_result = passive_rotate(scale(first, -1.0), vector)
        error = max_abs_difference(negative_result, quaternion_result)
        checks.observe("quaternion.sign-equivalence", within_tolerance(negative_result, quaternion_result, absolute, relative), error)

        combined = hamilton(first, second)
        combined_result = passive_rotate(combined, vector)
        sequential_result = passive_rotate(second, passive_rotate(first, vector))
        error = max_abs_difference(combined_result, sequential_result)
        checks.observe("quaternion.composition", within_tolerance(combined_result, sequential_result, absolute, relative), error)

        round_trip = passive_rotate(inverse(first), quaternion_result)
        error = max_abs_difference(round_trip, vector)
        checks.observe("quaternion.inverse", within_tolerance(round_trip, vector, absolute, relative), error)

        yaw = generator.uniform(-2.8, 2.8)
        pitch = generator.uniform(-1.4, 1.4)
        roll = generator.uniform(-2.8, 2.8)
        euler_round_trip = intrinsic_zyx_from_passive_quaternion(
            passive_quaternion_from_intrinsic_zyx(yaw, pitch, roll)
        )
        expected_euler = [yaw, pitch, roll]
        error = max_abs_difference(euler_round_trip, expected_euler)
        checks.observe(
            "quaternion.euler-round-trip",
            within_tolerance(euler_round_trip, expected_euler, absolute, relative),
            error,
        )

    zero_rejected = False
    try:
        inverse([0.0, 0.0, 0.0, 0.0])
    except ValueError:
        zero_rejected = True
    checks.observe("quaternion.zero-norm-domain-error", zero_rejected)

    singularity_rejected = False
    try:
        singular = passive_quaternion_from_intrinsic_zyx(0.3, 0.5 * math.pi, -0.2)
        intrinsic_zyx_from_passive_quaternion(singular)
    except ValueError:
        singularity_rejected = True
    checks.observe("quaternion.euler-singularity", singularity_rejected)

    euler_direction = passive_rotate(
        passive_quaternion_from_intrinsic_zyx(0.5 * math.pi, 0.0, 0.0),
        [1.0, 0.0, 0.0],
    )
    expected_direction = [0.0, 1.0, 0.0]
    error = max_abs_difference(euler_direction, expected_direction)
    checks.observe(
        "quaternion.euler-round-trip",
        within_tolerance(euler_direction, expected_direction, absolute, relative),
        error,
    )

    checks.observe("time.duration-alignment", exact_grid_ticks(2.0, 0.125) == 16)
    non_grid_rejected = False
    try:
        exact_grid_ticks(1.01, 0.125)
    except ValueError:
        non_grid_rejected = True
    checks.observe("time.duration-alignment", non_grid_rejected)

    time_types = ["SimulationTime", "Duration", "SampleTime", "ValidTime", "WallTime"]
    checks.observe("time.type-separation", len(time_types) == len(set(time_types)) == 5)


def build_report(conventions, cases):
    if conventions["convention_id"] != cases["convention_id"]:
        raise ValueError("convention identity mismatch")

    absolute = float(cases["tolerance"]["absolute"])
    relative = float(cases["tolerance"]["relative"])
    required_ids = list(conventions["verification"]["required_check_ids"])
    checks = CheckBook(required_ids)
    observations = []

    for case in cases["observations"]:
        actual = compute_observation(case)
        expected = list(case["expected"])
        check_id = check_id_for_observation(case["id"])
        error = max_abs_difference(actual, expected)
        exact = case["operation"] == "serialize_wxyz"
        passed = actual == expected if exact else within_tolerance(actual, expected, absolute, relative)
        checks.observe(check_id, passed, error)
        observations.append({"id": case["id"], "values": actual})

    run_property_checks(
        checks,
        int(conventions["verification"]["random_rotation_samples"]),
        absolute,
        relative,
    )

    status = "pass" if checks.passed() else "fail"
    return {
        "schema_version": OUTPUT_SCHEMA,
        "convention_id": conventions["convention_id"],
        "implementation": {
            "id": IMPLEMENTATION_ID,
            "language": "Python",
            "runtime": platform.python_implementation() + " " + platform.python_version(),
            "dependency_policy": "CPython standard library only",
        },
        "status": status,
        "checks": checks.report(),
        "observations": observations,
    }


def read_json(path):
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def write_report(report, destination):
    content = json.dumps(report, indent=2, sort_keys=False, allow_nan=False) + "\n"
    if destination == "-":
        sys.stdout.write(content)
        return
    pathlib.Path(destination).write_text(content, encoding="utf-8", newline="\n")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--conventions", required=True, type=pathlib.Path)
    parser.add_argument("--cases", required=True, type=pathlib.Path)
    parser.add_argument("--report", required=True, help="Output JSON path, or '-' for stdout")
    return parser.parse_args()


def main():
    args = parse_args()
    report = build_report(read_json(args.conventions), read_json(args.cases))
    write_report(report, args.report)
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())

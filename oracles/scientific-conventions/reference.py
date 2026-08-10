#!/usr/bin/env python3
"""Independent standard-library reference for R0 scientific conventions."""

from __future__ import annotations

import argparse
import csv
import json
import math
import random
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, Mapping, Sequence, TextIO


ORACLE_ID = "ORACLE-R0-SCI-CONVENTIONS-001"
REFERENCE_ID = "gnczmkn.scientific-conventions.python/1"
ABS_TOL = 1.0e-12
REL_TOL = 1.0e-12

Vec3 = tuple[float, float, float]
Mat3 = tuple[tuple[float, float, float], tuple[float, float, float], tuple[float, float, float]]
Quaternion = tuple[float, float, float, float]


class ConventionError(ValueError):
    """A value violates a frozen convention."""


def _require_finite(value: float, label: str) -> float:
    result = float(value)
    if not math.isfinite(result):
        raise ConventionError(f"{label} must be finite")
    return result


def close(a: float, b: float, abs_tol: float = ABS_TOL, rel_tol: float = REL_TOL) -> bool:
    return abs(a - b) <= abs_tol + rel_tol * max(abs(a), abs(b))


def vec_close(a: Vec3, b: Vec3, tolerance: float = 2.0e-12) -> bool:
    return max(abs(a[index] - b[index]) for index in range(3)) <= tolerance


def to_si(value: float, unit_id: str) -> tuple[float, str]:
    """Convert the deliberately small R0 boundary-unit set to canonical SI."""

    source = _require_finite(value, "unit value")
    scale_units = {
        "m": (1.0, "m"),
        "km": (1000.0, "m"),
        "s": (1.0, "s"),
        "kg": (1.0, "kg"),
        "rad": (1.0, "rad"),
        "deg": (math.pi / 180.0, "rad"),
        "m/s": (1.0, "m/s"),
        "m/s^2": (1.0, "m/s^2"),
        "rad/s": (1.0, "rad/s"),
        "N": (1.0, "N"),
        "N*m": (1.0, "N*m"),
        "Pa": (1.0, "Pa"),
        "K": (1.0, "K"),
    }
    if unit_id == "degC":
        kelvin = source + 273.15
        if kelvin < 0.0:
            raise ConventionError("temperature is below absolute zero")
        return kelvin, "K"
    try:
        scale, canonical = scale_units[unit_id]
    except KeyError as exc:
        raise ConventionError(f"unknown unit id: {unit_id}") from exc
    result = source * scale
    if not math.isfinite(result):
        raise ConventionError("unit conversion produced a non-finite value")
    if canonical == "K" and result < 0.0:
        raise ConventionError("temperature is below absolute zero")
    return result, canonical


@dataclass(frozen=True)
class Duration:
    seconds: float

    def __post_init__(self) -> None:
        object.__setattr__(self, "seconds", _require_finite(self.seconds, "duration"))


@dataclass(frozen=True)
class SimulationTime:
    seconds: float
    clock_domain: str

    def __post_init__(self) -> None:
        object.__setattr__(self, "seconds", _require_finite(self.seconds, "simulation time"))
        if not self.clock_domain:
            raise ConventionError("simulation time requires a clock domain")

    def plus(self, duration: Duration) -> "SimulationTime":
        return SimulationTime(self.seconds + duration.seconds, self.clock_domain)

    def minus(self, other: "SimulationTime") -> Duration:
        if self.clock_domain != other.clock_domain:
            raise ConventionError("simulation clock domains differ")
        return Duration(self.seconds - other.seconds)


@dataclass(frozen=True)
class SampleTime:
    seconds: float
    clock_domain: str

    def __post_init__(self) -> None:
        object.__setattr__(self, "seconds", _require_finite(self.seconds, "sample time"))
        if not self.clock_domain:
            raise ConventionError("sample time requires a clock domain")


@dataclass(frozen=True)
class ValidTime:
    seconds: float
    clock_domain: str

    def __post_init__(self) -> None:
        object.__setattr__(self, "seconds", _require_finite(self.seconds, "valid time"))
        if not self.clock_domain:
            raise ConventionError("valid time requires a clock domain")


@dataclass(frozen=True)
class WallTime:
    representation: str
    clock_source: str

    def __post_init__(self) -> None:
        if not self.representation or not self.clock_source:
            raise ConventionError("wall time requires an explicit representation and clock source")


@dataclass(frozen=True)
class ValidInterval:
    valid_from: float
    valid_until: float
    clock_domain: str

    def __post_init__(self) -> None:
        start = _require_finite(self.valid_from, "valid_from")
        end = _require_finite(self.valid_until, "valid_until")
        object.__setattr__(self, "valid_from", start)
        object.__setattr__(self, "valid_until", end)
        if not self.clock_domain:
            raise ConventionError("validity interval requires a clock domain")
        if end < start:
            raise ConventionError("validity interval is reversed")

    def contains(self, instant: ValidTime) -> bool:
        if instant.clock_domain != self.clock_domain:
            raise ConventionError("validity clock domains differ")
        value = _require_finite(instant.seconds, "valid time")
        return self.valid_from <= value < self.valid_until


def add_vec(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def scale_vec(scale: float, vector: Vec3) -> Vec3:
    return (scale * vector[0], scale * vector[1], scale * vector[2])


def dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: Vec3, b: Vec3) -> Vec3:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def norm(vector: Vec3) -> float:
    return math.sqrt(dot(vector, vector))


def unit(vector: Vec3) -> Vec3:
    magnitude = norm(vector)
    if not math.isfinite(magnitude) or magnitude == 0.0:
        raise ConventionError("axis must have finite nonzero norm")
    return scale_vec(1.0 / magnitude, vector)


def mat_vec(matrix: Mat3, vector: Vec3) -> Vec3:
    return tuple(dot(row, vector) for row in matrix)  # type: ignore[return-value]


def transpose(matrix: Mat3) -> Mat3:
    return tuple(tuple(matrix[row][column] for row in range(3)) for column in range(3))  # type: ignore[return-value]


def mat_mul(left: Mat3, right: Mat3) -> Mat3:
    right_t = transpose(right)
    return tuple(tuple(dot(row, column) for column in right_t) for row in left)  # type: ignore[return-value]


def determinant(matrix: Mat3) -> float:
    a, b, c = matrix[0]
    d, e, f = matrix[1]
    g, h, i = matrix[2]
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)


def identity_error(matrix: Mat3) -> float:
    return max(abs(matrix[row][column] - (1.0 if row == column else 0.0)) for row in range(3) for column in range(3))


def proper_rotation_error(matrix: Mat3) -> tuple[float, float]:
    return identity_error(mat_mul(transpose(matrix), matrix)), abs(determinant(matrix) - 1.0)


def validate_proper_rotation(matrix: Mat3, tolerance: float = 2.0e-12) -> None:
    finite = all(math.isfinite(value) for row in matrix for value in row)
    orthogonality_error, determinant_error = proper_rotation_error(matrix)
    if not finite or orthogonality_error > tolerance or determinant_error > tolerance:
        raise ConventionError("matrix is not a proper rotation")


def rodrigues(axis: Vec3, angle: float) -> Mat3:
    x, y, z = unit(axis)
    theta = _require_finite(angle, "passive rotation angle")
    cosine = math.cos(theta)
    sine = math.sin(theta)
    one_minus = 1.0 - cosine
    return (
        (cosine + x * x * one_minus, x * y * one_minus - z * sine, x * z * one_minus + y * sine),
        (y * x * one_minus + z * sine, cosine + y * y * one_minus, y * z * one_minus - x * sine),
        (z * x * one_minus - y * sine, z * y * one_minus + x * sine, cosine + z * z * one_minus),
    )


def hamilton(left: Quaternion, right: Quaternion) -> Quaternion:
    lw, lx, ly, lz = left
    rw, rx, ry, rz = right
    return (
        lw * rw - lx * rx - ly * ry - lz * rz,
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
    )


def qnorm(quaternion: Quaternion) -> float:
    return math.sqrt(sum(value * value for value in quaternion))


def qconjugate(quaternion: Quaternion) -> Quaternion:
    return (quaternion[0], -quaternion[1], -quaternion[2], -quaternion[3])


def qinverse(quaternion: Quaternion) -> Quaternion:
    squared_norm = sum(value * value for value in quaternion)
    if not math.isfinite(squared_norm) or squared_norm == 0.0:
        raise ConventionError("quaternion has zero or non-finite norm")
    return tuple(value / squared_norm for value in qconjugate(quaternion))  # type: ignore[return-value]


def deserialize_quaternion(values: Sequence[float], policy: str = "Reject") -> tuple[Quaternion, bool]:
    if len(values) != 4:
        raise ConventionError("quaternion storage must contain exactly four coefficients")
    quaternion = tuple(_require_finite(value, "quaternion coefficient") for value in values)
    magnitude = qnorm(quaternion)  # type: ignore[arg-type]
    if magnitude == 0.0:
        raise ConventionError("quaternion has zero norm")
    if close(magnitude, 1.0):
        return quaternion, False  # type: ignore[return-value]
    if policy == "NormalizeWithFlag":
        return tuple(value / magnitude for value in quaternion), True  # type: ignore[return-value]
    if policy != "Reject":
        raise ConventionError(f"unknown normalization policy: {policy}")
    raise ConventionError("non-unit quaternion rejected by policy")


def passive_axis_angle(axis: Vec3, angle: float) -> Quaternion:
    ux, uy, uz = unit(axis)
    half = 0.5 * _require_finite(angle, "passive rotation angle")
    sine = math.sin(half)
    return (math.cos(half), -ux * sine, -uy * sine, -uz * sine)


def passive_rotate(quaternion: Quaternion, vector: Vec3) -> Vec3:
    checked, _ = deserialize_quaternion(quaternion, "Reject")
    pure = (0.0, vector[0], vector[1], vector[2])
    rotated = hamilton(hamilton(qinverse(checked), pure), checked)
    if abs(rotated[0]) > 5.0e-12:
        raise ConventionError("passive rotation produced a nonzero scalar component")
    return (rotated[1], rotated[2], rotated[3])


def matrix_from_quaternion(quaternion: Quaternion) -> Mat3:
    columns = (
        passive_rotate(quaternion, (1.0, 0.0, 0.0)),
        passive_rotate(quaternion, (0.0, 1.0, 0.0)),
        passive_rotate(quaternion, (0.0, 0.0, 1.0)),
    )
    return tuple(tuple(columns[column][row] for column in range(3)) for row in range(3))  # type: ignore[return-value]


def max_matrix_difference(left: Mat3, right: Mat3) -> float:
    return max(abs(left[row][column] - right[row][column]) for row in range(3) for column in range(3))


def validate_euler_metadata(sequence: str, mode: str, unit_id: str) -> None:
    if len(sequence) != 3 or any(axis not in "XYZ" for axis in sequence):
        raise ConventionError("Euler sequence must declare three X/Y/Z axes")
    if mode not in {"intrinsic", "extrinsic"}:
        raise ConventionError("Euler mode must be intrinsic or extrinsic")
    if unit_id not in {"rad", "deg"}:
        raise ConventionError("Euler angle unit must be rad or deg")


class _Verifier:
    def __init__(self) -> None:
        self.checks = 0
        self.failures: list[str] = []

    def check(self, condition: bool, label: str) -> None:
        self.checks += 1
        if not condition:
            self.failures.append(label)

    def expect_failure(self, action: Callable[[], object], label: str) -> None:
        self.checks += 1
        try:
            action()
        except ConventionError:
            return
        self.failures.append(label)


def self_test() -> dict[str, object]:
    verify = _Verifier()

    verify.check(to_si(2.5, "km") == (2500.0, "m"), "km converts to m")
    angle, angle_unit = to_si(180.0, "deg")
    verify.check(close(angle, math.pi) and angle_unit == "rad", "degrees convert to radians")
    temperature, temperature_unit = to_si(-273.15, "degC")
    verify.check(close(temperature, 0.0) and temperature_unit == "K", "Celsius offset reaches zero Kelvin")
    for canonical_unit in ("m", "s", "kg", "rad", "m/s", "m/s^2", "rad/s", "N", "N*m", "Pa", "K"):
        converted, returned_unit = to_si(1.0, canonical_unit)
        verify.check(converted == 1.0 and returned_unit == canonical_unit, f"canonical unit id {canonical_unit}")
    verify.expect_failure(lambda: to_si(1.0, "unknown"), "unknown unit must fail")
    verify.expect_failure(lambda: to_si(math.inf, "m"), "non-finite unit value must fail")
    verify.expect_failure(lambda: to_si(-273.1501, "degC"), "below-zero Kelvin must fail")
    verify.expect_failure(lambda: to_si(-0.001, "K"), "negative Kelvin must fail")

    start = SimulationTime(5.0, "sim/main")
    end = start.plus(Duration(0.25))
    verify.check(end == SimulationTime(5.25, "sim/main"), "simulation time plus duration")
    verify.check(end.minus(start) == Duration(0.25), "same-domain simulation time difference")
    verify.expect_failure(lambda: end.minus(SimulationTime(5.0, "sim/other")), "mixed simulation clocks must fail")
    interval = ValidInterval(1.0, 2.0, "sim/main")
    verify.check(interval.contains(ValidTime(1.0, "sim/main")), "half-open interval includes start")
    verify.check(not interval.contains(ValidTime(2.0, "sim/main")), "half-open interval excludes end")
    verify.expect_failure(lambda: ValidInterval(2.0, 1.0, "sim/main"), "reversed interval must fail")
    verify.expect_failure(lambda: SimulationTime(math.nan, "sim/main"), "non-finite time must fail")
    verify.expect_failure(lambda: SampleTime(math.inf, "sensor/main"), "non-finite sample time must fail")
    verify.expect_failure(lambda: ValidTime(1.0, ""), "valid time without clock domain must fail")
    verify.expect_failure(lambda: WallTime("", "host/utc"), "wall time without representation must fail")

    z_quarter = passive_axis_angle((0.0, 0.0, 1.0), math.pi / 2.0)
    expected_coefficient = math.sqrt(0.5)
    verify.check(
        vec_close(passive_rotate(z_quarter, (1.0, 0.0, 0.0)), (0.0, 1.0, 0.0)),
        "passive z rotation maps x to y",
    )
    verify.check(
        all(close(actual, expected) for actual, expected in zip(z_quarter, (expected_coefficient, 0.0, 0.0, -expected_coefficient))),
        "wxyz passive quarter-turn storage",
    )
    negated = tuple(-value for value in z_quarter)
    verify.check(
        vec_close(passive_rotate(negated, (0.25, -0.5, 1.0)), passive_rotate(z_quarter, (0.25, -0.5, 1.0))),
        "q and negative q are rotation-equivalent",
    )
    q_b_a = z_quarter
    q_c_b = passive_axis_angle((1.0, 0.0, 0.0), math.pi / 3.0)
    q_c_a = hamilton(q_b_a, q_c_b)
    probe = (0.25, -0.75, 2.0)
    sequential = passive_rotate(q_c_b, passive_rotate(q_b_a, probe))
    verify.check(vec_close(passive_rotate(q_c_a, probe), sequential), "passive quaternion composition order")

    verify.expect_failure(lambda: deserialize_quaternion((1.0, 0.0, 0.0)), "short quaternion storage must fail")
    verify.expect_failure(lambda: deserialize_quaternion((0.0, 0.0, 0.0, 0.0)), "zero quaternion must fail")
    verify.expect_failure(lambda: deserialize_quaternion((2.0, 0.0, 0.0, 0.0)), "non-unit quaternion reject policy")
    normalized, flagged = deserialize_quaternion((2.0, 0.0, 0.0, 0.0), "NormalizeWithFlag")
    verify.check(normalized == (1.0, 0.0, 0.0, 0.0) and flagged, "normalization records a flag")
    verify.expect_failure(lambda: deserialize_quaternion((math.nan, 0.0, 0.0, 1.0)), "non-finite quaternion must fail")
    validate_euler_metadata("ZYX", "intrinsic", "rad")
    verify.check(True, "qualified Euler metadata passes")
    verify.expect_failure(lambda: validate_euler_metadata("", "intrinsic", "rad"), "missing Euler sequence must fail")
    verify.expect_failure(lambda: validate_euler_metadata("ZYX", "", "rad"), "missing Euler mode must fail")

    reflection: Mat3 = ((-1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
    verify.expect_failure(lambda: validate_proper_rotation(reflection), "reflection matrix must fail")
    free_vector = mat_vec(rodrigues((0.0, 0.0, 1.0), math.pi / 2.0), (1.0, 0.0, 0.0))
    point = add_vec(free_vector, (10.0, -2.0, 0.5))
    verify.check(not vec_close(free_vector, point), "point translation remains separate")

    generator = random.Random(20260809)
    for index in range(128):
        axis = (generator.uniform(-1.0, 1.0), generator.uniform(-1.0, 1.0), generator.uniform(-1.0, 1.0))
        if norm(axis) < 1.0e-9:
            axis = (1.0, 0.0, 0.0)
        angle_value = generator.uniform(-math.pi, math.pi)
        quaternion = passive_axis_angle(axis, angle_value)
        from_quaternion = matrix_from_quaternion(quaternion)
        analytic = rodrigues(axis, angle_value)
        orthogonality_error, determinant_error = proper_rotation_error(from_quaternion)
        verify.check(max_matrix_difference(from_quaternion, analytic) <= 2.0e-12, f"random matrix/quaternion agreement {index}")
        verify.check(orthogonality_error <= 2.0e-12, f"random orthogonality {index}")
        verify.check(determinant_error <= 2.0e-12, f"random determinant {index}")
        vector = (generator.uniform(-5.0, 5.0), generator.uniform(-5.0, 5.0), generator.uniform(-5.0, 5.0))
        round_trip = passive_rotate(qconjugate(quaternion), passive_rotate(quaternion, vector))
        verify.check(vec_close(round_trip, vector, 5.0e-12), f"random inverse round trip {index}")

    status = "passed" if not verify.failures else "failed"
    return {
        "oracle_id": ORACLE_ID,
        "reference_id": REFERENCE_ID,
        "status": status,
        "property_checks": verify.checks,
        "failures": verify.failures,
    }


CASE_FIELDS = (
    "case_id",
    "axis_x",
    "axis_y",
    "axis_z",
    "passive_angle_rad",
    "vector_x",
    "vector_y",
    "vector_z",
    "translation_x",
    "translation_y",
    "translation_z",
)

RESULT_FIELDS = (
    "case_id",
    "q_w",
    "q_x",
    "q_y",
    "q_z",
    "vector_to_x",
    "vector_to_y",
    "vector_to_z",
    "point_to_x",
    "point_to_y",
    "point_to_z",
    "roundtrip_x",
    "roundtrip_y",
    "roundtrip_z",
    "q_norm",
    "det_r",
    "orthogonality_error",
    "matrix_agreement_error",
)


def load_cases(path: Path) -> list[dict[str, object]]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if tuple(reader.fieldnames or ()) != CASE_FIELDS:
            raise ConventionError("cross-tool case header does not match version 1")
        cases: list[dict[str, object]] = []
        seen: set[str] = set()
        for row in reader:
            case_id = row["case_id"]
            if not case_id or case_id in seen:
                raise ConventionError("case ids must be nonempty and unique")
            seen.add(case_id)
            parsed: dict[str, object] = {"case_id": case_id}
            for field in CASE_FIELDS[1:]:
                parsed[field] = _require_finite(float(row[field]), field)
            cases.append(parsed)
    if not cases:
        raise ConventionError("cross-tool case set is empty")
    return cases


def evaluate_case(case: Mapping[str, object]) -> dict[str, object]:
    axis = (float(case["axis_x"]), float(case["axis_y"]), float(case["axis_z"]))
    angle = float(case["passive_angle_rad"])
    vector = (float(case["vector_x"]), float(case["vector_y"]), float(case["vector_z"]))
    translation = (
        float(case["translation_x"]),
        float(case["translation_y"]),
        float(case["translation_z"]),
    )
    quaternion = passive_axis_angle(axis, angle)
    rotation = matrix_from_quaternion(quaternion)
    analytic = rodrigues(axis, angle)
    validate_proper_rotation(rotation)
    vector_to = passive_rotate(quaternion, vector)
    point_to = add_vec(vector_to, translation)
    roundtrip = passive_rotate(qconjugate(quaternion), vector_to)
    orthogonality_error, _ = proper_rotation_error(rotation)
    result_values = (
        quaternion[0], quaternion[1], quaternion[2], quaternion[3],
        vector_to[0], vector_to[1], vector_to[2],
        point_to[0], point_to[1], point_to[2],
        roundtrip[0], roundtrip[1], roundtrip[2],
        qnorm(quaternion), determinant(rotation), orthogonality_error,
        max_matrix_difference(rotation, analytic),
    )
    return {"case_id": case["case_id"], **dict(zip(RESULT_FIELDS[1:], result_values))}


def evaluate_cases(path: Path) -> list[dict[str, object]]:
    return [evaluate_case(case) for case in load_cases(path)]


def emit_cases(path: Path, stream: TextIO) -> None:
    writer = csv.DictWriter(stream, fieldnames=RESULT_FIELDS, lineterminator="\n")
    writer.writeheader()
    for result in evaluate_cases(path):
        formatted = {"case_id": result["case_id"]}
        formatted.update({field: format(float(result[field]), ".17g") for field in RESULT_FIELDS[1:]})
        writer.writerow(formatted)


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--self-test", action="store_true", help="run analytic and failure-path checks")
    action.add_argument("--metadata", action="store_true", help="emit reference implementation metadata")
    action.add_argument("--emit-cross-tool", type=Path, metavar="CASES", help="emit evaluated cases as CSV")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)
    try:
        if args.self_test:
            result = self_test()
            print(json.dumps(result, sort_keys=True))
            return 0 if result["status"] == "passed" else 1
        if args.metadata:
            print(
                json.dumps(
                    {
                        "oracle_id": ORACLE_ID,
                        "reference_id": REFERENCE_ID,
                        "language": "Python",
                        "standard_library_only": True,
                        "python_version": sys.version.split()[0],
                    },
                    sort_keys=True,
                )
            )
            return 0
        emit_cases(args.emit_cross_tool, sys.stdout)
        return 0
    except (ConventionError, OSError, ValueError) as exc:
        print(f"scientific convention reference failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

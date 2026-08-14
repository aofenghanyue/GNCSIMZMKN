#!/usr/bin/env python3
"""Independent Decimal reference for YYZ supplied uniform environment."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from decimal import Decimal, getcontext
from pathlib import Path
import subprocess
import sys


FIXTURE_ID = "REF-YYZ-UNIFORM-ENVIRONMENT-001"
ORACLE_ID = "ORACLE-YYZ-UNIFORM-ENVIRONMENT-001"
MODEL_ID = "MODEL-YYZ-UNIFORM-ENVIRONMENT-001"
CASES_SCHEMA = "gnczmkn.yyz-uniform-environment-cases/1"
REFERENCE_SCHEMA = "gnczmkn.yyz-uniform-environment-reference/1"
INERTIAL_FRAME_ID = "frame.fixture.yyz.inertial-cartesian@1"
CLOCK_DOMAIN = "clock.fixture.yyz.simulation@1"
QUALITY = "Valid"
LEGACY_DENSITY_SCALE_HEIGHT_M = Decimal(7200)
LEGACY_REFERENCE_RADIUS_M = Decimal(6378137)

RESPONSE_VECTOR_FIELDS = (
    "gravity_I_mps2",
    "airmass_velocity_I_mps",
)
RESPONSE_SCALAR_FIELDS = (
    "density_kgpm3",
    "speed_of_sound_mps",
)
CONSUMER_VECTOR_FIELDS = (
    "vehicle_velocity_I_mps",
    "relative_velocity_I_mps",
    "force_I_N",
    "force_acceleration_I_mps2",
    "total_acceleration_I_mps2",
)
CONSUMER_SCALAR_FIELDS = (
    "mass_kg",
    "mass_reciprocal_per_kg",
    "speed_squared_m2ps2",
    "speed_mps",
    "dynamic_pressure_Pa",
    "mach",
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


def subtract(lhs: list[Decimal], rhs: list[Decimal]) -> list[Decimal]:
    return [left - right for left, right in zip(lhs, rhs)]


def scale(values: list[Decimal], factor: Decimal) -> list[Decimal]:
    return [value * factor for value in values]


def dot(lhs: list[Decimal], rhs: list[Decimal]) -> Decimal:
    return sum((left * right for left, right in zip(lhs, rhs)), Decimal(0))


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
            "duplicate uniform-environment case id")
    return result


def validate_input(case: dict) -> None:
    definition = case["definition"]
    require(definition["inertial_frame_id"] == INERTIAL_FRAME_ID,
            "environment definition inertial frame differs")
    require(definition["clock_domain"] == CLOCK_DOMAIN,
            "environment definition clock domain differs")
    require(valid_nonnegative_integer(
                definition["configuration_revision"]),
            "environment configuration revision must be nonnegative")
    gravity = vector(definition["gravity_I_mps2"], "gravity_I")
    wind = vector(definition["airmass_velocity_I_mps"],
                  "airmass_velocity_I")
    density = decimal(definition["density_kgpm3"])
    sound_speed = decimal(definition["speed_of_sound_mps"])
    require(all(value.is_finite() for value in gravity + wind),
            "environment free vectors must be finite")
    require(density >= 0, "environment density must be nonnegative")
    require(sound_speed > 0,
            "environment speed of sound must be positive")

    query = case["query"]
    vector(query["position_I_m"], "query position_I")
    require(query["position_frame_id"] == INERTIAL_FRAME_ID,
            "environment query position frame differs")
    require(valid_nonnegative_integer(query["sample_tick"]),
            "environment query sample tick must be nonnegative")
    require(query["clock_domain"] == definition["clock_domain"],
            "environment query clock domain differs")
    require(query["configuration_revision"] ==
            definition["configuration_revision"],
            "environment query configuration revision differs")

    consumer = case["consumer_probe"]
    vector(consumer["vehicle_velocity_I_mps"], "vehicle velocity_I")
    vector(consumer["force_I_N"], "force_I")
    mass = decimal(consumer["mass_kg"])
    require(mass > 0, "environment consumer mass must be positive")
    require(consumer["inertial_frame_id"] == INERTIAL_FRAME_ID,
            "environment consumer inertial frame differs")
    require(consumer["sample_tick"] == query["sample_tick"] and
            consumer["clock_domain"] == query["clock_domain"] and
            consumer["configuration_revision"] ==
            query["configuration_revision"],
            "environment consumer sample identity differs")


def query_environment(case: dict, density_mode: str = "uniform",
                      gravity_mode: str = "uniform") -> dict:
    validate_input(case)
    definition = case["definition"]
    query = case["query"]
    position = vector(query["position_I_m"], "query position_I")
    gravity = vector(definition["gravity_I_mps2"], "gravity_I")
    wind = vector(definition["airmass_velocity_I_mps"],
                  "airmass_velocity_I")
    density = decimal(definition["density_kgpm3"])
    sound_speed = decimal(definition["speed_of_sound_mps"])
    altitude_seed = max(Decimal(0), position[2])

    require(density_mode in ("uniform", "legacy_altitude_decay"),
            "unsupported environment density mode")
    if density_mode == "legacy_altitude_decay":
        density *= (-altitude_seed /
                    LEGACY_DENSITY_SCALE_HEIGHT_M).exp()

    require(gravity_mode in ("uniform", "legacy_inverse_square"),
            "unsupported environment gravity mode")
    if gravity_mode == "legacy_inverse_square":
        ratio = (LEGACY_REFERENCE_RADIUS_M /
                 (LEGACY_REFERENCE_RADIUS_M + altitude_seed))
        gravity = scale(gravity, ratio * ratio)

    require(density.is_finite() and density >= 0 and
            all(value.is_finite() for value in gravity),
            "environment query produced an invalid response")
    response = {
        "model_id": MODEL_ID,
        "quality": QUALITY,
        "inertial_frame_id": INERTIAL_FRAME_ID,
        "sample_tick": query["sample_tick"],
        "clock_domain": query["clock_domain"],
        "configuration_revision": query["configuration_revision"],
        "gravity_I_mps2": gravity,
        "airmass_velocity_I_mps": wind,
        "density_kgpm3": density,
        "speed_of_sound_mps": sound_speed,
    }

    consumer = case["consumer_probe"]
    vehicle_velocity = vector(
        consumer["vehicle_velocity_I_mps"], "vehicle velocity_I")
    force = vector(consumer["force_I_N"], "force_I")
    mass = decimal(consumer["mass_kg"])
    relative_velocity = subtract(vehicle_velocity, wind)
    speed_squared = dot(relative_velocity, relative_velocity)
    speed = speed_squared.sqrt()
    require(speed > 0,
            "environment consumer link requires positive relative speed")
    dynamic_pressure = Decimal("0.5") * density * speed_squared
    mach = speed / sound_speed
    mass_reciprocal = Decimal(1) / mass
    force_acceleration = scale(force, mass_reciprocal)
    total_acceleration = add(force_acceleration, gravity)
    derived = (relative_velocity + force_acceleration +
               total_acceleration + [speed_squared, speed,
                                     dynamic_pressure, mach,
                                     mass_reciprocal])
    require(all(value.is_finite() for value in derived),
            "environment consumer link produced a non-finite value")
    consumer_link = {
        "inertial_frame_id": INERTIAL_FRAME_ID,
        "sample_tick": query["sample_tick"],
        "clock_domain": query["clock_domain"],
        "configuration_revision": query["configuration_revision"],
        "vehicle_velocity_I_mps": vehicle_velocity,
        "relative_velocity_I_mps": relative_velocity,
        "speed_squared_m2ps2": speed_squared,
        "speed_mps": speed,
        "dynamic_pressure_Pa": dynamic_pressure,
        "mach": mach,
        "force_I_N": force,
        "mass_kg": mass,
        "mass_reciprocal_per_kg": mass_reciprocal,
        "force_acceleration_I_mps2": force_acceleration,
        "total_acceleration_I_mps2": total_acceleration,
    }
    return {
        "id": case["id"],
        "query": {
            "position_I_m": position,
            "position_frame_id": query["position_frame_id"],
            "sample_tick": query["sample_tick"],
            "clock_domain": query["clock_domain"],
            "configuration_revision": query["configuration_revision"],
        },
        "response": response,
        "consumer_link": consumer_link,
    }


def physical_values(result: dict) -> list[Decimal]:
    response = result["response"]
    consumer = result["consumer_link"]
    values: list[Decimal] = []
    for field in RESPONSE_VECTOR_FIELDS:
        values.extend(decimal(item) for item in response[field])
    for field in RESPONSE_SCALAR_FIELDS:
        values.append(decimal(response[field]))
    for field in CONSUMER_VECTOR_FIELDS:
        values.extend(decimal(item) for item in consumer[field])
    for field in CONSUMER_SCALAR_FIELDS:
        values.append(decimal(consumer[field]))
    return values


def max_physical_difference(lhs: dict, rhs: dict) -> Decimal:
    return max((abs(left - right) for left, right in zip(
        physical_values(lhs), physical_values(rhs))), default=Decimal(0))


def high_position_case(cases: dict) -> dict:
    base = copy.deepcopy(cases_by_id(cases)[
        "CASE-YYZ-UNIFORM-ENVIRONMENT-CONSUMER-LINK"])
    base["query"]["position_I_m"] = [101, -22, 10000]
    base["query"]["sample_tick"] = 112
    base["consumer_probe"]["sample_tick"] = 112
    return base


def reference_equivalence_results(cases: dict) -> list[dict]:
    base = cases_by_id(cases)[
        "CASE-YYZ-UNIFORM-ENVIRONMENT-CONSUMER-LINK"]
    accepted = query_environment(base)
    moved = query_environment(high_position_case(cases))
    difference = max_physical_difference(accepted, moved)
    result = {
        "id": "EQUIV-YYZ-UNIFORM-ENVIRONMENT-POSITION-TICK",
        "status": "passed" if difference <= Decimal("1e-68")
        else "failed",
        "max_abs_physical_difference": difference,
    }
    require(result["status"] == "passed",
            "Python uniform-environment invariance check failed")
    return [result]


def rejects(operation) -> bool:
    try:
        operation()
    except (KeyError, TypeError, ValueError):
        return True
    return False


def reference_invalid_rejections(cases: dict) -> list[str]:
    base = cases_by_id(cases)[
        "CASE-YYZ-UNIFORM-ENVIRONMENT-CONSUMER-LINK"]
    results = []

    def add_mutation(identifier: str, mutate) -> None:
        value = copy.deepcopy(base)
        mutate(value)
        if rejects(lambda: query_environment(value)):
            results.append(identifier)

    add_mutation("INVALID-YYZ-UNIFORM-ENVIRONMENT-FRAME-MISMATCH",
                 lambda value: value["query"].__setitem__(
                     "position_frame_id", "frame.other@1"))
    add_mutation("INVALID-YYZ-UNIFORM-ENVIRONMENT-CLOCK-MISMATCH",
                 lambda value: value["query"].__setitem__(
                     "clock_domain", "clock.other@1"))
    add_mutation("INVALID-YYZ-UNIFORM-ENVIRONMENT-REVISION-MISMATCH",
                 lambda value: value["query"].__setitem__(
                     "configuration_revision",
                     value["definition"]["configuration_revision"] + 1))
    add_mutation("INVALID-YYZ-UNIFORM-ENVIRONMENT-NEGATIVE-TICK",
                 lambda value: value["query"].__setitem__(
                     "sample_tick", -1))
    add_mutation("INVALID-YYZ-UNIFORM-ENVIRONMENT-NONFINITE-POSITION",
                 lambda value: value["query"]["position_I_m"].__setitem__(
                     0, Decimal("Infinity")))
    add_mutation("INVALID-YYZ-UNIFORM-ENVIRONMENT-NONFINITE-GRAVITY",
                 lambda value: value["definition"][
                     "gravity_I_mps2"].__setitem__(
                         0, Decimal("Infinity")))
    add_mutation("INVALID-YYZ-UNIFORM-ENVIRONMENT-NONFINITE-WIND",
                 lambda value: value["definition"][
                     "airmass_velocity_I_mps"].__setitem__(
                         0, Decimal("Infinity")))
    add_mutation("INVALID-YYZ-UNIFORM-ENVIRONMENT-NEGATIVE-DENSITY",
                 lambda value: value["definition"].__setitem__(
                     "density_kgpm3", Decimal("-0.1")))
    add_mutation(
        "INVALID-YYZ-UNIFORM-ENVIRONMENT-NONPOSITIVE-SOUND-SPEED",
        lambda value: value["definition"].__setitem__(
            "speed_of_sound_mps", 0))
    return results


def reference_mutation_results(cases: dict) -> list[dict]:
    high = high_position_case(cases)
    accepted = query_environment(high)
    profiles = [
        ("MUTATION-YYZ-UNIFORM-ENVIRONMENT-LEGACY-DENSITY-DECAY",
         {"density_mode": "legacy_altitude_decay"}),
        ("MUTATION-YYZ-UNIFORM-ENVIRONMENT-LEGACY-GRAVITY-DECAY",
         {"gravity_mode": "legacy_inverse_square"}),
    ]
    results = []
    for identifier, options in profiles:
        mutated = query_environment(high, **options)
        difference = max_physical_difference(accepted, mutated)
        results.append({
            "id": identifier,
            "status": "rejected" if difference > Decimal("1e-30")
            else "matched",
            "max_abs_physical_difference": difference,
        })
    require(all(result["status"] == "rejected" for result in results),
            "Python uniform-environment reference accepted a mutation")
    return results


def build_reference(cases: dict, raw_cases: bytes) -> dict:
    invalid = reference_invalid_rejections(cases)
    expected_invalid = [entry["id"] for entry in
                        cases["invalid_input_cases"]]
    require(invalid == expected_invalid,
            "Python uniform-environment invalid coverage differs")
    return stringify({
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "status": "executable",
        "precision": {"decimal_digits": getcontext().prec},
        "input_identity": {
            "path": "fixtures/ref-yyz-uniform-environment/cases.json",
            "bytes": len(raw_cases),
            "sha256": sha256_bytes(raw_cases),
        },
        "cases": {
            case["id"]: query_environment(case) for case in cases["cases"]
        },
        "equivalence_results": reference_equivalence_results(cases),
        "invalid_input_rejections": invalid,
        "mutation_results": reference_mutation_results(cases),
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
            f"C++ uniform-environment probe failed: "
            f"{completed.stderr.strip()}")
    return completed.stdout, json.loads(
        completed.stdout, parse_float=Decimal)


def compare_case(checks: Checks, actual: dict, expected: dict,
                 absolute: Decimal, relative: Decimal) -> None:
    case_id = expected["id"]
    checks.require(actual["id"] == case_id,
                   f"C++ environment case id differs for {case_id}")
    actual_query = actual["query"]
    expected_query = expected["query"]
    compare_vector(checks, actual_query["position_I_m"],
                   expected_query["position_I_m"], absolute, relative,
                   f"{case_id}.query.position_I_m")
    for field in ("position_frame_id", "sample_tick", "clock_domain",
                  "configuration_revision"):
        checks.require(actual_query[field] == expected_query[field],
                       f"{case_id}.query.{field} differs")

    actual_response = actual["response"]
    expected_response = expected["response"]
    for field in ("model_id", "quality", "inertial_frame_id",
                  "sample_tick", "clock_domain", "configuration_revision"):
        checks.require(actual_response[field] == expected_response[field],
                       f"{case_id}.response.{field} differs")
    for field in RESPONSE_VECTOR_FIELDS:
        compare_vector(checks, actual_response[field],
                       expected_response[field], absolute, relative,
                       f"{case_id}.response.{field}")
    for field in RESPONSE_SCALAR_FIELDS:
        compare_scalar(checks, actual_response[field],
                       expected_response[field], absolute, relative,
                       f"{case_id}.response.{field}")

    actual_consumer = actual["consumer_link"]
    expected_consumer = expected["consumer_link"]
    for field in ("inertial_frame_id", "sample_tick", "clock_domain",
                  "configuration_revision"):
        checks.require(actual_consumer[field] == expected_consumer[field],
                       f"{case_id}.consumer_link.{field} differs")
    for field in CONSUMER_VECTOR_FIELDS:
        compare_vector(checks, actual_consumer[field],
                       expected_consumer[field], absolute, relative,
                       f"{case_id}.consumer_link.{field}")
    for field in CONSUMER_SCALAR_FIELDS:
        compare_scalar(checks, actual_consumer[field],
                       expected_consumer[field], absolute, relative,
                       f"{case_id}.consumer_link.{field}")


def verify_reference(cases: dict, raw_cases: bytes, oracle: dict,
                     probe_path: Path) -> dict:
    checks = Checks()
    checks.require(cases["fixture_id"] == oracle["fixture_id"] == FIXTURE_ID,
                   "uniform-environment fixture identity differs", 2)
    checks.require(cases["oracle_id"] == oracle["oracle_id"] == ORACLE_ID,
                   "uniform-environment oracle identity differs", 2)
    checks.require(cases["model"]["model_id"] ==
                   oracle["model_id"] == MODEL_ID,
                   "uniform-environment model identity differs", 2)
    checks.require(oracle["precision"]["decimal_digits"] >= 70,
                   "uniform-environment precision is below 70 digits")
    identity = oracle["input_identity"]
    checks.require(identity["bytes"] == len(raw_cases) and
                   identity["sha256"] == sha256_bytes(raw_cases),
                   "uniform-environment input byte identity differs", 2)
    checks.require(identity["path"] ==
                   "fixtures/ref-yyz-uniform-environment/cases.json",
                   "uniform-environment input path differs")

    recomputed = build_reference(cases, raw_cases)
    checks.require(oracle == recomputed,
                   "stored environment oracle differs from its producer",
                   len(oracle["cases"]) + 3)

    first_stdout, probe = run_probe(probe_path)
    second_stdout, second_probe = run_probe(probe_path)
    checks.require(first_stdout == second_stdout and probe == second_probe,
                   "C++ uniform-environment probe reruns differ", 2)
    checks.require(probe["oracle_id"] == ORACLE_ID and
                   probe["model_id"] == MODEL_ID and
                   probe["status"] == "passed" and
                   probe["model_choice_status"] ==
                   cases["model_choice"]["status"],
                   "C++ uniform-environment identity differs", 4)

    probe_cases = {entry["id"]: entry for entry in probe["cases"]}
    checks.require(len(probe_cases) == len(probe["cases"]) ==
                   len(oracle["cases"]),
                   "C++ uniform-environment cases are incomplete", 2)
    absolute = decimal(cases["tolerances"]["formula_absolute"])
    relative = decimal(cases["tolerances"]["formula_relative"])
    for case_id, expected in oracle["cases"].items():
        checks.require(case_id in probe_cases,
                       f"C++ environment case is missing: {case_id}")
        compare_case(checks, probe_cases[case_id], expected,
                     absolute, relative)

    expected_equivalence = {
        entry["id"] for entry in cases["equivalence_cases"]
    }
    checks.require(set(probe["equivalence_checks"]) == expected_equivalence,
                   "C++ environment equivalence identities differ")
    expected_invalid = {
        entry["id"] for entry in cases["invalid_input_cases"]
    }
    checks.require(set(probe["invalid_input_rejections"]) == expected_invalid,
                   "C++ environment invalid identities differ")
    expected_mutations = {
        entry["id"] for entry in cases["mutation_cases"]
    }
    checks.require(set(probe["mutation_rejections"]) == expected_mutations,
                   "C++ environment mutation identities differ")

    linked = probe_cases[
        "CASE-YYZ-UNIFORM-ENVIRONMENT-CONSUMER-LINK"]
    zero_density = probe_cases[
        "CASE-YYZ-UNIFORM-ENVIRONMENT-ZERO-DENSITY"]
    return stringify({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks.count,
        "input_sha256": sha256_bytes(raw_cases),
        "case_count": len(probe_cases),
        "consumer_link_relative_velocity_I_mps":
            linked["consumer_link"]["relative_velocity_I_mps"],
        "consumer_link_dynamic_pressure_Pa":
            linked["consumer_link"]["dynamic_pressure_Pa"],
        "consumer_link_mach": linked["consumer_link"]["mach"],
        "consumer_link_total_acceleration_I_mps2":
            linked["consumer_link"]["total_acceleration_I_mps2"],
        "zero_density_dynamic_pressure_Pa":
            zero_density["consumer_link"]["dynamic_pressure_Pa"],
        "zero_density_sub_one_sound_speed_mach":
            zero_density["consumer_link"]["mach"],
        "equivalence_cases_passed": len(expected_equivalence),
        "invalid_input_cases_rejected": len(expected_invalid),
        "mutation_cases_rejected": len(expected_mutations),
    })


def validate_cases_identity(cases: dict) -> None:
    require(cases["schema_version"] == CASES_SCHEMA and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model"]["model_id"] == MODEL_ID,
            "uniform-environment cases identity differs")
    require(cases["model_choice"]["status"] == "accepted",
            "uniform-environment model choice is not accepted")
    require(cases["model"]["inertial_frame_id"] == INERTIAL_FRAME_ID and
            cases["model"]["clock_domain"] == CLOCK_DOMAIN and
            cases["model"]["quality"] == QUALITY,
            "uniform-environment model profile differs")


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
        print(f"YYZ uniform environment reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

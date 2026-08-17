#!/usr/bin/env python3
"""Two-consumer conformance for call-local AlgorithmEvaluation results."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys


FORBIDDEN_RUNTIME_KEYS = {
    "session_id",
    "runtime_instance_id",
    "run_id",
    "state_epoch",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def run_probe(path: Path) -> tuple[bytes, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return completed.stdout, json.loads(completed.stdout.decode("utf-8"))


def collect_keys(value: object) -> set[str]:
    if isinstance(value, dict):
        result = set(value)
        for nested in value.values():
            result.update(collect_keys(nested))
        return result
    if isinstance(value, list):
        result: set[str] = set()
        for nested in value:
            result.update(collect_keys(nested))
        return result
    return set()


def deterministic_probe(name: str, path: Path) -> dict:
    first_bytes, first = run_probe(path)
    second_bytes, second = run_probe(path)
    require(first_bytes == second_bytes and first == second,
            f"{name} AlgorithmEvaluation is not deterministic")
    require(first.get("status") == "passed",
            f"{name} product evaluation did not pass")
    checks = first.get("direct_checks")
    require(isinstance(checks, list) and
            "formal-output-telemetry-separation" in checks,
            f"{name} did not prove output/telemetry separation")
    runtime_keys = collect_keys(first) & FORBIDDEN_RUNTIME_KEYS
    require(not runtime_keys,
            f"{name} independent evaluation exposed runtime keys: "
            f"{sorted(runtime_keys)}")
    return first


def verify(yyz_probe: Path, cavh_probe: Path) -> dict:
    yyz = deterministic_probe("yyz", yyz_probe)
    cavh = deterministic_probe("cavh", cavh_probe)

    yyz_accepted = yyz.get("accepted")
    require(isinstance(yyz_accepted, dict) and
            isinstance(yyz_accepted.get("candidate"), dict) and
            isinstance(yyz_accepted.get("air_data"), dict) and
            isinstance(yyz_accepted.get("rigid_derivative_at_tick0"), dict),
            "YYZ did not expose candidate output plus evaluation telemetry")
    require(yyz_accepted["candidate"].get("tick") == 1,
            "YYZ formal candidate output has the wrong effective tick")

    cavh_consumer = cavh.get("typed_consumer")
    require(isinstance(cavh_consumer, dict) and
            "gamma_reference_rad" in cavh_consumer and
            "alpha_limited_rad" in cavh_consumer and
            "alpha_raw_rad" in cavh_consumer and
            "saturation" in cavh_consumer,
            "CAVH did not expose formal outputs plus TDCT telemetry")
    require(cavh_consumer["sample_tick"] == 42,
            "CAVH formal output has the wrong sample tick")

    consumers = [yyz.get("product_model_id"),
                 cavh.get("product_model_id")]
    require(all(isinstance(value, str) and value for value in consumers) and
            consumers[0] != consumers[1],
            "AlgorithmEvaluation consumers do not have distinct identities")
    return {
        "schema_version":
            "gnczmkn.model-sdk-algorithm-evaluation-conformance/1",
        "status": "passed",
        "consumer_count": 2,
        "consumers": consumers,
        "result_parts": ["output", "telemetry"],
        "state": "omitted-for-stateless-kernels",
        "session_runtime_keys": "absent",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--yyz-probe", required=True, type=Path)
    parser.add_argument("--cavh-probe", required=True, type=Path)
    arguments = parser.parse_args()
    print(json.dumps(
        verify(arguments.yyz_probe, arguments.cavh_probe),
        separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, TypeError, ValueError,
            json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"Model SDK AlgorithmEvaluation conformance failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

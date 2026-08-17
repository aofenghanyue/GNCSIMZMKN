#!/usr/bin/env python3
"""Conformance for independently evaluable YYZ and CAVH prepared models."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys


EXPECTED = {
    "yyz": {
        "model_id":
            "gnc.package.yyz.rigid-step.frozen-interval.experimental@1",
        "model_version": "0.1.0",
        "execution_form": "Closure",
        "clock_domain_id": "clock.fixture.yyz.simulation@1",
        "configuration_revision": 11,
        "preparation_algorithm_id":
            "gnc.package.yyz.rigid-step.prepare@1",
        "preparation_algorithm_version": "0.1.0",
    },
    "cavh": {
        "model_id":
            "gnc.package.cavh.formula.legacy-transcribed.experimental@1",
        "model_version": "0.1.0",
        "execution_form": "PureQuery",
        "clock_domain_id": "clock.fixture.cavh.simulation@1",
        "configuration_revision": 4,
        "preparation_algorithm_id":
            "gnc.package.cavh.formula.prepare@1",
        "preparation_algorithm_version": "0.1.0",
    },
}
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


def verify_probe(name: str, path: Path) -> dict:
    first_bytes, first = run_probe(path)
    second_bytes, second = run_probe(path)
    require(first_bytes == second_bytes and first == second,
            f"{name} prepared-model evaluation is not deterministic")
    require(first.get("status") == "passed",
            f"{name} product evaluation did not pass")
    metadata = first.get("prepared_model")
    require(isinstance(metadata, dict),
            f"{name} prepared-model metadata is missing")
    require(metadata == EXPECTED[name],
            f"{name} prepared-model metadata differs")
    require(first.get("product_model_id") == metadata["model_id"],
            f"{name} root/model metadata identity differs")
    checks = first.get("direct_checks")
    require(isinstance(checks, list) and
            "prepared-model-metadata" in checks and
            "deterministic-independent-evaluation" in checks and
            "model-metadata-rejection" in checks,
            f"{name} model conformance checks are incomplete")
    runtime_keys = collect_keys(first) & FORBIDDEN_RUNTIME_KEYS
    require(not runtime_keys,
            f"{name} independent evaluation exposed runtime keys: "
            f"{sorted(runtime_keys)}")
    return metadata


def verify(yyz_probe: Path, cavh_probe: Path) -> dict:
    yyz = verify_probe("yyz", yyz_probe)
    cavh = verify_probe("cavh", cavh_probe)
    require(yyz["model_id"] != cavh["model_id"],
            "YYZ and CAVH model identities alias")
    require(yyz["execution_form"] != cavh["execution_form"],
            "model conformance did not exercise distinct execution forms")
    return {
        "schema_version": "gnczmkn.model-sdk-prepared-conformance/1",
        "status": "passed",
        "consumer_count": 2,
        "consumers": [yyz["model_id"], cavh["model_id"]],
        "execution_forms": [
            yyz["execution_form"], cavh["execution_form"]],
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
        print(f"Model SDK prepared-model conformance failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

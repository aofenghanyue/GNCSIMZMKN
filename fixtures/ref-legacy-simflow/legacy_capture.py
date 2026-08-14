#!/usr/bin/env python3
"""Run the frozen Legacy SimFlow and plain replay entrypoints once."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import shutil
import subprocess
import sys


ORACLE_ID = "ORACLE-SIMFLOW-07"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def write_json(path: Path, value: dict) -> None:
    path.write_text(
        json.dumps(value, ensure_ascii=False, separators=(",", ":")) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def run_legacy(executable: Path, mode: str, input_path: Path,
               run_root: Path) -> int:
    completed = subprocess.run(
        [str(executable), mode, str(input_path)],
        cwd=run_root,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    require(
        completed.returncode == 0,
        f"Legacy {mode} failed with {completed.returncode}: "
        f"{completed.stderr.strip()}",
    )
    return completed.returncode


def parse_summary(path: Path) -> dict:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    require(len(rows) == 1, "Legacy SimFlow summary must contain one case")
    row = rows[0]
    require(
        row["case_id"] == "hot" and
        row["status"] == "succeeded" and
        row["exit_code"] == "0",
        "Legacy SimFlow hot case did not succeed",
    )
    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--legacy-executable", required=True, type=Path)
    parser.add_argument("--run-root", required=True, type=Path)
    parser.add_argument("--fixture-root", required=True, type=Path)
    parser.add_argument("--work-root", required=True, type=Path)
    parser.add_argument("--capture-root", required=True, type=Path)
    parser.add_argument("--configured-output", required=True, type=Path)
    parser.add_argument("--rerun-index", required=True, type=int)
    arguments = parser.parse_args()

    run_root = arguments.run_root.resolve()
    fixture_root = arguments.fixture_root.resolve()
    work_root = arguments.work_root.resolve()
    capture_root = arguments.capture_root.resolve()
    executable = arguments.legacy_executable.resolve()
    configured_output = arguments.configured_output
    require(executable.is_file(), "Legacy executable does not exist")
    require(arguments.rerun_index > 0, "rerun index must be positive")
    require(not configured_output.is_absolute() and
            ".." not in configured_output.parts,
             "configured output must be a repository-relative child path")
    require(not run_root.exists(), "run root must be a fresh path")
    require(not work_root.exists(), "work root must be a fresh path")
    run_root.mkdir(parents=True)
    work_root.mkdir(parents=True)
    capture_root.mkdir(parents=True, exist_ok=True)

    base_mission = fixture_root / "base-mission.json"
    case_source = fixture_root / "cases.csv"
    template = read_json(fixture_root / "simflow-template.json")
    require(base_mission.is_file() and case_source.is_file(),
            "SimFlow fixture inputs are incomplete")

    output_directory = run_root / configured_output
    require(not output_directory.exists(),
            "configured output must be fresh for every run")
    template["base_mission"] = base_mission.as_posix()
    template["materializer"]["config"]["case_source"]["file"] = (
        case_source.as_posix())
    template["outputs"]["directory"] = configured_output.as_posix()
    generated_simflow = work_root / "simflow.json"
    generated_simflow.write_text(
        json.dumps(template, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )

    simflow_exit = run_legacy(
        executable, "--simflow", generated_simflow, run_root)
    case_directory = output_directory / "case_000001"
    effective_mission = case_directory / "effective_mission.json"
    simflow_dataset = case_directory / "simflow_case.csv"
    summary = output_directory / "simflow_summary.csv"
    require(effective_mission.is_file() and
            simflow_dataset.is_file() and summary.is_file(),
            "Legacy SimFlow did not produce its replay artifacts")
    require(not (case_directory / "case_manifest.json").exists(),
            "Legacy SimFlow unexpectedly produced case_manifest.json")
    summary_row = parse_summary(summary)

    suffix = str(arguments.rerun_index)
    effective_capture = capture_root / f"legacy-effective-run-{suffix}.json"
    simflow_capture = capture_root / f"legacy-simflow-run-{suffix}.csv"
    replay_capture = capture_root / f"legacy-replay-run-{suffix}.csv"
    summary_capture = capture_root / f"legacy-summary-run-{suffix}.csv"
    trace_capture = capture_root / f"legacy-trace-run-{suffix}.json"
    for target in (
            effective_capture, simflow_capture, replay_capture,
            summary_capture, trace_capture):
        require(not target.exists(), f"capture target already exists: {target}")

    shutil.copyfile(effective_mission, effective_capture)
    shutil.copyfile(simflow_dataset, simflow_capture)
    shutil.copyfile(summary, summary_capture)

    pre_replay_dataset = work_root / "simflow-dataset-before-replay.csv"
    require(not pre_replay_dataset.exists(),
            "pre-replay holding path must be fresh")
    simflow_dataset.replace(pre_replay_dataset)
    require(not simflow_dataset.exists(),
            "ordinary replay dataset path must start absent")
    replay_exit = run_legacy(
        executable, "--config", effective_mission, run_root)
    require(simflow_dataset.is_file(),
            "Ordinary replay did not produce its normal dataset")
    shutil.copyfile(simflow_dataset, replay_capture)
    require(simflow_capture.read_bytes() == replay_capture.read_bytes(),
            "SimFlow and ordinary replay datasets differ")

    write_json(trace_capture, {
        "schema_version": "gnczmkn.legacy-simflow-trace/1",
        "oracle_id": ORACLE_ID,
        "rerun_index": arguments.rerun_index,
        "commands": [
            {
                "sequence": 0,
                "entrypoint": "gnc_sim",
                "mode": "--simflow",
                "input_role": "generated-simflow",
                "exit_code": simflow_exit,
            },
            {
                "sequence": 1,
                "entrypoint": "gnc_sim",
                "mode": "--config",
                "input_role": "effective-mission",
                "exit_code": replay_exit,
            },
        ],
        "materialization": {
            "source_case_id": summary_row["case_id"],
            "legacy_case_index": int(summary_row["case_index"]),
            "legacy_case_directory_name": case_directory.name,
            "case_manifest_present": False,
        },
        "lineage_checks": {
            "simflow_output_root_started_absent": True,
            "ordinary_replay_dataset_path_started_absent": True,
        },
        "artifacts": {
            "effective_mission": effective_capture.name,
            "simflow_dataset": simflow_capture.name,
            "ordinary_replay_dataset": replay_capture.name,
            "simflow_summary": summary_capture.name,
        },
    })
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError, json.JSONDecodeError,
            subprocess.SubprocessError) as error:
        print(f"legacy SimFlow capture failed: {error}", file=sys.stderr)
        raise SystemExit(1)

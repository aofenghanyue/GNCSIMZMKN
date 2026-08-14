#!/usr/bin/env python3
"""Semantic CSV reference and comparator for ORACLE-YYZ-CSV-05."""

from __future__ import annotations

import argparse
import csv
from decimal import Decimal, getcontext
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import zipfile


ORACLE_ID = "ORACLE-YYZ-CSV-05"
TIME_FIELD_ID = "fixture:legacy-csv.sample_time_s"
ALTITUDE_FIELD_ID = "vehicle:fixture.truth.altitude_m"
VELOCITY_FIELD_ID = "vehicle:fixture.truth.vertical_velocity_mps"


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def decimal(value: object) -> Decimal:
    result = Decimal(str(value))
    require(result.is_finite(), f"Non-finite decimal value: {value}")
    return result


def verify_file_identity(record: dict, repo_root: Path, label: str) -> bytes:
    value = (repo_root / record["path"]).read_bytes()
    require(len(value) == record["bytes"], f"{label} byte count differs")
    require(sha256_bytes(value) == record["sha256"],
            f"{label} SHA-256 differs")
    return value


def verify_capture_identity(case: dict, repo_root: Path) -> tuple[int, list[bytes]]:
    source = case["legacy_source"]
    archive_record = source["archive"]
    archive_path = repo_root / archive_record["path"]
    archive_bytes = archive_path.read_bytes()
    require(len(archive_bytes) == archive_record["bytes"],
            "Legacy archive byte count differs")
    require(sha256_bytes(archive_bytes) == archive_record["sha256"],
            "Legacy archive SHA-256 differs")
    checks = 2

    with zipfile.ZipFile(archive_path, "r") as archive:
        for entry in source["entries"]:
            value = archive.read(archive_record["prefix"] + entry["path"])
            require(len(value) == entry["bytes"],
                    f"Legacy entry byte count differs: {entry['path']}")
            require(sha256_bytes(value) == entry["sha256"],
                    f"Legacy entry SHA-256 differs: {entry['path']}")
            checks += 2

    test_record = source["runtime_test"]
    evidence = verify_file_identity({
        "path": test_record["evidence_path"],
        "bytes": test_record["evidence_bytes"],
        "sha256": test_record["evidence_sha256"],
    }, repo_root, "Legacy CTest evidence")
    evidence_text = evidence.decode("utf-8-sig")
    require(any("Test #11: test_publish_semantics" in line and
                "Passed" in line for line in evidence_text.splitlines()),
            "Legacy CTest evidence does not contain the passing publish test")
    checks += 3

    capture = case["legacy_capture"]
    require(capture["compiler"] == "w64devkit GCC 16.2.0" and
            capture["language_standard"] == "C++17",
            "Legacy CSV capture compiler identity differs")
    require("-std=c++17" in capture["compile_command"] and
            "-O2" in capture["compile_command"],
            "Legacy CSV capture compile command differs")
    require(len(capture["run_commands"]) == 2 and
            "run-1/legacy-run-1.csv" in capture["run_commands"][0] and
            "run-2/legacy-run-2.csv" in capture["run_commands"][1],
            "Legacy CSV capture run commands differ")
    checks += 3

    verify_file_identity(capture["environment_evidence"], repo_root,
                         "Legacy capture environment evidence")
    verify_file_identity(capture["harness"], repo_root,
                         "Legacy CSV capture harness")
    checks += 4

    datasets = []
    for index, dataset_record in enumerate(capture["datasets"], start=1):
        stored = verify_file_identity(dataset_record, repo_root,
                                      f"Legacy CSV dataset {index}")
        require(dataset_record["stored_line_endings"] == "LF" and
                dataset_record["captured_line_endings"] == "CRLF" and
                b"\r" not in stored,
                "Legacy CSV storage normalization differs")
        captured = stored.replace(b"\n", b"\r\n")
        require(len(captured) == dataset_record["captured_bytes"] and
                sha256_bytes(captured) == dataset_record["captured_sha256"],
                "Reconstructed raw Legacy CSV identity differs")
        datasets.append(stored)
        checks += 5
    return checks, datasets


def load_semantic_fields(case: dict, repo_root: Path) -> tuple[int, dict]:
    raw = verify_file_identity(case["semantic_fields"], repo_root,
                               "CSV semantic field map")
    sidecar = json.loads(raw.decode("utf-8"))
    require(set(sidecar) == {
        "schema_version", "oracle_id", "case_id", "fields",
        "encoding_policy",
    }, "CSV semantic field map top-level fields differ")
    require(sidecar["schema_version"] ==
            "gnczmkn.legacy-csv-semantic-fields/1" and
            sidecar["oracle_id"] == ORACLE_ID and
            sidecar["case_id"] == case["case_id"],
            "CSV semantic field map identity differs")

    fields = sidecar["fields"]
    require(len(fields) == 3, "CSV semantic field map must contain three fields")
    by_id = {field["field_id"]: field for field in fields}
    require(len(by_id) == len(fields) and set(by_id) == {
        TIME_FIELD_ID, ALTITUDE_FIELD_ID, VELOCITY_FIELD_ID,
    }, "CSV semantic field ids differ")
    require(by_id[TIME_FIELD_ID]["legacy_column"] == "time" and
            by_id[TIME_FIELD_ID]["unit"] == "s" and
            by_id[ALTITUDE_FIELD_ID]["legacy_column"] ==
            "vehicle.dynamics.position.z" and
            by_id[ALTITUDE_FIELD_ID]["unit"] == "m" and
            by_id[VELOCITY_FIELD_ID]["legacy_column"] ==
            "vehicle.dynamics.velocity.z" and
            by_id[VELOCITY_FIELD_ID]["unit"] == "m/s",
            "CSV semantic field mapping differs")
    policy = sidecar["encoding_policy"]
    require(policy["column_order"] == "ignored after semantic mapping" and
            policy["unmapped_columns"] ==
            "allowed and excluded from semantic comparison" and
            policy["numeric_text_format"] ==
            "finite Decimal-equivalent forms accepted after parsing" and
            policy["duplicate_headers"] == "rejected",
            "CSV encoding policy differs")
    return 12, by_id


def parse_csv_rows(raw: bytes) -> tuple[list[str], list[list[str]]]:
    text = raw.decode("utf-8")
    reader = csv.reader(io.StringIO(text, newline=""))
    rows = list(reader)
    require(len(rows) >= 2, "CSV dataset has no data rows")
    header = rows[0]
    require(all(header), "CSV dataset contains an empty header")
    require(len(set(header)) == len(header),
            "CSV dataset contains duplicate headers")
    require(all(len(row) == len(header) for row in rows[1:]),
            "CSV data row width differs from its header")
    return header, rows[1:]


def normalize_dataset(raw: bytes, fields_by_id: dict) -> list[dict]:
    header, encoded_rows = parse_csv_rows(raw)
    index_by_name = {name: index for index, name in enumerate(header)}
    required_columns = {
        field_id: fields_by_id[field_id]["legacy_column"]
        for field_id in (TIME_FIELD_ID, ALTITUDE_FIELD_ID, VELOCITY_FIELD_ID)
    }
    require(all(column in index_by_name
                for column in required_columns.values()),
            "CSV dataset is missing a required semantic column")

    result = []
    for sample_index, encoded in enumerate(encoded_rows):
        result.append({
            "sample_index": sample_index,
            "sample_time_s": decimal(encoded[index_by_name[
                required_columns[TIME_FIELD_ID]]]),
            "altitude_m": decimal(encoded[index_by_name[
                required_columns[ALTITUDE_FIELD_ID]]]),
            "vertical_velocity_mps": decimal(encoded[index_by_name[
                required_columns[VELOCITY_FIELD_ID]]]),
        })
    return result


def analytic_rows(case: dict) -> list[dict]:
    run = case["run"]
    t0 = decimal(run["initial_time_s"])
    dt = decimal(run["step_s"])
    duration = decimal(run["duration_s"])
    require(dt > 0 and duration >= 0 and duration % dt == 0,
            "CSV fixture duration must contain an integral step count")
    altitude0 = decimal(case["initial_state"]["altitude_m"])
    velocity0 = decimal(case["initial_state"]["vertical_velocity_mps"])
    acceleration = decimal(case["dynamics"]["vertical_acceleration_mps2"])
    count = int(duration / dt) + 1

    result = []
    for sample_index in range(count):
        time_s = t0 + dt * sample_index
        elapsed = time_s - t0
        result.append({
            "sample_index": sample_index,
            "sample_time_s": time_s,
            "altitude_m": altitude0 + velocity0 * elapsed +
            Decimal("0.5") * acceleration * elapsed * elapsed,
            "vertical_velocity_mps": velocity0 + acceleration * elapsed,
        })
    return result


def compare_decimal(actual: object, expected: Decimal,
                    tolerance: Decimal, label: str) -> None:
    error = abs(decimal(actual) - expected)
    require(error <= tolerance,
            f"{label} error {error} exceeds tolerance {tolerance}")


def validate_semantic_rows(actual: list[dict], expected: list[dict],
                           time_tolerance: Decimal,
                           state_tolerance: Decimal,
                           label: str) -> int:
    require(len(actual) == len(expected), f"{label} row count differs")
    checks = 1
    for index, target in enumerate(expected):
        row = actual[index]
        require(set(row) == {
            "sample_index", "sample_time_s", "altitude_m",
            "vertical_velocity_mps",
        }, f"{label} row {index} field set differs")
        require(row["sample_index"] == target["sample_index"],
                f"{label} row {index} sample index differs")
        compare_decimal(row["sample_time_s"], target["sample_time_s"],
                        time_tolerance, f"{label} row {index} sample time")
        compare_decimal(row["altitude_m"], target["altitude_m"],
                        state_tolerance, f"{label} row {index} altitude")
        compare_decimal(row["vertical_velocity_mps"],
                        target["vertical_velocity_mps"],
                        state_tolerance,
                        f"{label} row {index} vertical velocity")
        checks += 5
    return checks


def reference_rows(oracle: dict) -> list[dict]:
    return [{
        "sample_index": row["sample_index"],
        "sample_time_s": decimal(row["sample_time_s"]),
        "altitude_m": decimal(row["altitude_m"]),
        "vertical_velocity_mps": decimal(row["vertical_velocity_mps"]),
    } for row in oracle["expected"]["rows"]]


def permute_dataset(raw: bytes) -> bytes:
    header, rows = parse_csv_rows(raw)
    order = list(reversed(range(len(header))))
    output = io.StringIO(newline="")
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow([header[index] for index in order])
    for row in rows:
        writer.writerow([row[index] for index in order])
    return output.getvalue().encode("utf-8")


def reformat_required_numeric_text(raw: bytes,
                                   required_columns: set[str]) -> bytes:
    header, rows = parse_csv_rows(raw)
    required_indices = [index for index, name in enumerate(header)
                        if name in required_columns]
    output = io.StringIO(newline="")
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow(header)
    for row in rows:
        transformed = list(row)
        for index in required_indices:
            transformed[index] = format(decimal(transformed[index]), "E")
        writer.writerow(transformed)
    return output.getvalue().encode("utf-8")


def change_unmapped_column_values(raw: bytes,
                                  required_columns: set[str]) -> bytes:
    header, rows = parse_csv_rows(raw)
    unmapped_indices = [index for index, name in enumerate(header)
                        if name not in required_columns]
    require(unmapped_indices, "CSV mutation requires an unmapped column")
    output = io.StringIO(newline="")
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow(header)
    for row_index, row in enumerate(rows):
        transformed = list(row)
        for column_index in unmapped_indices:
            transformed[column_index] = (
                f"ignored-{row_index}-{column_index}")
        writer.writerow(transformed)
    return output.getvalue().encode("utf-8")


def duplicate_required_header(raw: bytes, required_column: str) -> bytes:
    header, rows = parse_csv_rows(raw)
    replacement_index = next(index for index, name in enumerate(header)
                             if name not in {"time", required_column,
                                             "vehicle.dynamics.velocity.z"})
    header[replacement_index] = required_column
    output = io.StringIO(newline="")
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow(header)
    writer.writerows(rows)
    return output.getvalue().encode("utf-8")


def rejected(function) -> bool:
    try:
        function()
    except (KeyError, ValueError):
        return True
    return False


def run_probe(path: Path) -> tuple[str, dict]:
    completed = subprocess.run(
        [str(path), "--self-check"],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    require(completed.returncode == 0,
            f"C++ CSV probe failed: {completed.stderr.strip()}")
    return completed.stdout, json.loads(completed.stdout)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--oracle", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--repo-root", required=True, type=Path)
    arguments = parser.parse_args()

    getcontext().prec = 50
    input_bytes = arguments.input.read_bytes()
    case = json.loads(input_bytes.decode("utf-8"))
    oracle = read_json(arguments.oracle)
    require(case["oracle_id"] == ORACLE_ID and
            oracle["oracle_id"] == ORACLE_ID,
            "Input or reference belongs to a different oracle")
    require(case["fixture_id"] == oracle["fixture_id"] and
            case["case_id"] == oracle["case_id"],
            "Input and reference identities differ")
    require(len(input_bytes) == oracle["input_identity"]["bytes"] and
            sha256_bytes(input_bytes) ==
            oracle["input_identity"]["sha256"],
            "Input byte identity differs from the reference")
    checks = 4

    repo_root = arguments.repo_root.resolve()
    capture_checks, datasets = verify_capture_identity(case, repo_root)
    checks += capture_checks
    require(len(datasets) == 2 and datasets[0] == datasets[1],
            "Legacy CSV capture reruns differ")
    checks += 2

    field_checks, fields_by_id = load_semantic_fields(case, repo_root)
    checks += field_checks
    expected_field_ids = oracle["expected"]["field_ids"]
    require(expected_field_ids ==
            [TIME_FIELD_ID, ALTITUDE_FIELD_ID, VELOCITY_FIELD_ID],
            "Reference semantic field ids differ")
    checks += 1

    analytic = analytic_rows(case)
    expected = reference_rows(oracle)
    time_tolerance = decimal(
        oracle["tolerances"]["legacy_seed_sample_time_absolute"])
    state_tolerance = decimal(
        oracle["tolerances"]["legacy_seed_state_absolute"])
    checks += validate_semantic_rows(
        expected, analytic, time_tolerance, state_tolerance, "Reference")
    require(oracle["expected"]["row_count"] == len(analytic) and
            oracle["expected"]["record_initial_state"] is True,
            "Reference row count or initial-record fact differs")
    checks += 2

    normalized = []
    for index, raw in enumerate(datasets, start=1):
        semantic = normalize_dataset(raw, fields_by_id)
        checks += validate_semantic_rows(
            semantic, analytic, time_tolerance, state_tolerance,
            f"Legacy dataset {index}")
        normalized.append(semantic)
    require(normalized[0] == normalized[1],
            "Normalized Legacy CSV reruns differ")
    checks += 1

    equivalence_by_id = {entry["id"]: entry
                         for entry in oracle["equivalence_cases"]}
    require(set(equivalence_by_id) == {
        "PASS-CSV-COLUMN-PERMUTED",
        "PASS-CSV-NUMERIC-TEXT-REFORMATTED",
        "PASS-CSV-UNMAPPED-COLUMNS-CHANGED",
    } and all(entry["expected_status"] == "accepted"
              for entry in equivalence_by_id.values()),
            "CSV equivalence-case definitions differ")
    required_columns = {
        fields_by_id[field_id]["legacy_column"]
        for field_id in expected_field_ids
    }
    permuted = normalize_dataset(permute_dataset(datasets[0]), fields_by_id)
    require(permuted == normalized[0],
            "CSV column permutation changed semantic data")
    reformatted = normalize_dataset(
        reformat_required_numeric_text(datasets[0], required_columns),
        fields_by_id)
    require(reformatted == normalized[0],
            "Equivalent CSV numeric text changed semantic data")
    unmapped_changed = normalize_dataset(
        change_unmapped_column_values(datasets[0], required_columns),
        fields_by_id)
    require(unmapped_changed == normalized[0],
            "Unmapped CSV column data changed semantic data")
    checks += 7

    failure_by_id = {entry["id"]: entry
                     for entry in oracle["failure_cases"]}
    require(set(failure_by_id) == {
        "FAIL-CSV-MISSING-T0", "FAIL-CSV-SHIFTED-TK",
        "FAIL-CSV-STALE-PUBLISHED-STATE",
        "FAIL-CSV-DUPLICATE-REQUIRED-HEADER",
    } and all(entry["expected_status"] == "rejected"
              for entry in failure_by_id.values()),
            "CSV failure-case definitions differ")

    missing_t0 = normalized[0][1:]
    shifted_tk = [dict(row) for row in normalized[0]]
    shifted_tk[1]["sample_time_s"] = Decimal("0.75")
    stale_state = [dict(row) for row in normalized[0]]
    stale_state[1]["altitude_m"] = Decimal("1000")
    require(rejected(lambda: validate_semantic_rows(
                missing_t0, analytic, time_tolerance, state_tolerance,
                "Missing-t0 mutation")) and
            rejected(lambda: validate_semantic_rows(
                shifted_tk, analytic, time_tolerance, state_tolerance,
                "Shifted-tk mutation")) and
            rejected(lambda: validate_semantic_rows(
                stale_state, analytic, time_tolerance, state_tolerance,
                "Stale-state mutation")) and
            rejected(lambda: normalize_dataset(
                duplicate_required_header(
                    datasets[0], "vehicle.dynamics.position.z"),
                fields_by_id)),
            "A CSV semantic failure mutation was accepted")
    checks += 5

    first_stdout, probe = run_probe(arguments.probe)
    second_stdout, second_probe = run_probe(arguments.probe)
    require(first_stdout == second_stdout and probe == second_probe,
            "C++ CSV probe reruns differ")
    require(probe["oracle_id"] == ORACLE_ID and
            probe["status"] == "passed",
            "C++ CSV probe returned an unexpected identity or status")
    probe_rows = [{
        "sample_index": row["sample_index"],
        "sample_time_s": decimal(row["sample_time_s"]),
        "altitude_m": decimal(row["altitude_m"]),
        "vertical_velocity_mps": decimal(row["vertical_velocity_mps"]),
    } for row in probe["semantic_rows"]]
    checks += validate_semantic_rows(
        probe_rows, analytic, time_tolerance, state_tolerance, "C++ probe")
    for flag in (
            "column_permutation_accepted", "numeric_text_format_accepted",
            "unmapped_column_change_accepted", "missing_t0_rejected",
            "shifted_tk_rejected", "stale_published_state_rejected",
            "duplicate_required_header_rejected"):
        require(probe[flag] is True, f"C++ CSV probe did not enforce {flag}")
    checks += 10

    decision = oracle["disposition_decision"]
    require(decision["status"] in {"needs_owner_decision", "accepted"},
            "Disposition decision has an unsupported status")
    disposition_field = ("disposition" if decision["status"] == "accepted"
                         else "recommended_disposition")
    dispositions = {entry[disposition_field] for entry in decision["facts"]}
    require(dispositions == {"Preserve", "Retire"},
            "Disposition must separate Preserve and Retire")
    checks += 2

    print(json.dumps({
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "checks": checks,
        "input_sha256": sha256_bytes(input_bytes),
        "legacy_reruns_byte_identical": True,
        "semantic_column_permutation_equivalent": True,
        "semantic_numeric_text_equivalent": True,
        "semantic_unmapped_column_change_equivalent": True,
        "row_count": len(analytic),
        "final_altitude_m": str(analytic[-1]["altitude_m"]),
        "final_vertical_velocity_mps": str(
            analytic[-1]["vertical_velocity_mps"]),
        "disposition_status": decision["status"],
    }, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError, zipfile.BadZipFile,
            json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"legacy CSV reference failed: {error}", file=sys.stderr)
        raise SystemExit(1)

#!/usr/bin/env python3
"""Compare the independent Python reference with the C++ R0 property tool."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import math
import platform
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Mapping, Sequence

sys.dont_write_bytecode = True

import reference


REPORT_SCHEMA = "gnczmkn.scientific-cross-tool-report/1"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run_tool(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, check=False, capture_output=True, text=True, encoding="utf-8")


def read_cmake_cache(path: Path) -> dict[str, str]:
    selected_keys = {
        "CMAKE_BUILD_TYPE",
        "CMAKE_CXX_COMPILER",
        "CMAKE_CXX_FLAGS",
        "CMAKE_CXX_FLAGS_DEBUG",
        "CMAKE_CXX_FLAGS_RELEASE",
        "CMAKE_GENERATOR",
        "GNC_WARNINGS_AS_ERRORS",
    }
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="strict").splitlines():
        if not line or line.startswith("//") or line.startswith("#") or "=" not in line or ":" not in line:
            continue
        declaration, value = line.split("=", 1)
        key, _value_type = declaration.split(":", 1)
        if key in selected_keys:
            values[key] = value
    return values


def parse_cpp_rows(output: str) -> list[dict[str, object]]:
    reader = csv.DictReader(io.StringIO(output))
    if tuple(reader.fieldnames or ()) != reference.RESULT_FIELDS:
        raise reference.ConventionError("C++ cross-tool output header does not match version 1")
    rows: list[dict[str, object]] = []
    seen: set[str] = set()
    for row in reader:
        case_id = row["case_id"]
        if not case_id or case_id in seen:
            raise reference.ConventionError("C++ result case ids must be nonempty and unique")
        seen.add(case_id)
        parsed: dict[str, object] = {"case_id": case_id}
        for field in reference.RESULT_FIELDS[1:]:
            value = float(row[field])
            if not math.isfinite(value):
                raise reference.ConventionError(f"C++ result {case_id}.{field} is non-finite")
            parsed[field] = value
        rows.append(parsed)
    return rows


def compare_rows(
    python_rows: list[dict[str, object]], cpp_rows: list[dict[str, object]]
) -> tuple[list[dict[str, object]], list[str], float, float]:
    errors: list[str] = []
    cpp_by_id = {str(row["case_id"]): row for row in cpp_rows}
    python_ids = [str(row["case_id"]) for row in python_rows]
    cpp_ids = [str(row["case_id"]) for row in cpp_rows]
    if python_ids != cpp_ids:
        errors.append(f"case order or identity differs: python={python_ids}, cpp={cpp_ids}")

    per_case: list[dict[str, object]] = []
    overall_max_abs = 0.0
    overall_max_ratio = 0.0
    for expected in python_rows:
        case_id = str(expected["case_id"])
        candidate = cpp_by_id.get(case_id)
        if candidate is None:
            errors.append(f"C++ output is missing case {case_id}")
            continue
        max_abs = 0.0
        max_ratio = 0.0
        worst_field = ""
        failed_fields: list[str] = []
        for field in reference.RESULT_FIELDS[1:]:
            reference_value = float(expected[field])
            candidate_value = float(candidate[field])
            difference = abs(candidate_value - reference_value)
            limit = reference.ABS_TOL + reference.REL_TOL * max(abs(reference_value), abs(candidate_value))
            ratio = difference / limit if limit > 0.0 else 0.0
            if difference > max_abs:
                max_abs = difference
                worst_field = field
            max_ratio = max(max_ratio, ratio)
            if difference > limit:
                failed_fields.append(field)
        overall_max_abs = max(overall_max_abs, max_abs)
        overall_max_ratio = max(overall_max_ratio, max_ratio)
        case_status = "passed" if not failed_fields else "failed"
        if failed_fields:
            errors.append(f"case {case_id} exceeded tolerance in {failed_fields}")
        per_case.append(
            {
                "case_id": case_id,
                "status": case_status,
                "compared_values": len(reference.RESULT_FIELDS) - 1,
                "max_abs_error": max_abs,
                "max_tolerance_ratio": max_ratio,
                "worst_field": worst_field,
                "failed_fields": failed_fields,
            }
        )
    for extra in sorted(set(cpp_ids) - set(python_ids)):
        errors.append(f"C++ output contains unexpected case {extra}")
    return per_case, errors, overall_max_abs, overall_max_ratio


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", required=True, type=Path, help="C++ property executable")
    parser.add_argument("--cases", required=True, type=Path, help="checked-in CSV input cases")
    parser.add_argument("--cpp-source", required=True, type=Path, help="C++ property source for evidence hashing")
    parser.add_argument("--cmake-cache", required=True, type=Path, help="CMake cache used to build the C++ tool")
    parser.add_argument("--output", type=Path, help="write JSON report; stdout is used when omitted")
    parser.add_argument(
        "--approval-status",
        choices=("pending_architecture_review", "approved"),
        default="pending_architecture_review",
    )
    parser.add_argument("--reviewer-assignee", default="", help="reviewer identity for an approved retained report")
    parser.add_argument("--reviewed-at", default="", help="review date for an approved retained report")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.approval_status == "approved" and (not args.reviewer_assignee or not args.reviewed_at):
        print("approved evidence requires --reviewer-assignee and --reviewed-at", file=sys.stderr)
        return 1
    cpp_path = args.cpp.resolve()
    cases_path = args.cases.resolve()
    cpp_source_path = args.cpp_source.resolve()
    cmake_cache_path = args.cmake_cache.resolve()
    script_path = Path(__file__).resolve()
    reference_path = Path(reference.__file__).resolve()
    errors: list[str] = []

    for label, path in (
        ("C++ executable", cpp_path),
        ("case set", cases_path),
        ("C++ source", cpp_source_path),
        ("CMake cache", cmake_cache_path),
    ):
        if not path.is_file():
            errors.append(f"{label} does not exist: {path}")

    python_self_test = reference.self_test()
    if python_self_test["status"] != "passed":
        errors.append("Python reference self-test failed")

    cpp_metadata: Mapping[str, object] = {}
    cpp_self_test_output = ""
    cpp_property_checks = 0
    cpp_rows: list[dict[str, object]] = []
    if not errors:
        metadata_run = run_tool([str(cpp_path), "--metadata"])
        if metadata_run.returncode != 0:
            errors.append(f"C++ metadata command failed: {metadata_run.stderr.strip()}")
        else:
            try:
                cpp_metadata = json.loads(metadata_run.stdout)
            except json.JSONDecodeError as exc:
                errors.append(f"C++ metadata is invalid JSON: {exc}")

        self_test_run = run_tool([str(cpp_path)])
        cpp_self_test_output = self_test_run.stdout.strip()
        if self_test_run.returncode != 0:
            errors.append(f"C++ property self-test failed: {self_test_run.stderr.strip()}")
        match = re.search(r"properties_passed=(\d+)", cpp_self_test_output)
        if match is None:
            errors.append("C++ property output did not report properties_passed")
        else:
            cpp_property_checks = int(match.group(1))

        emit_run = run_tool([str(cpp_path), "--emit-cross-tool", str(cases_path)])
        if emit_run.returncode != 0:
            errors.append(f"C++ case emission failed: {emit_run.stderr.strip()}")
        else:
            try:
                cpp_rows = parse_cpp_rows(emit_run.stdout)
            except (reference.ConventionError, ValueError) as exc:
                errors.append(str(exc))

    python_rows: list[dict[str, object]] = []
    try:
        python_rows = reference.evaluate_cases(cases_path)
    except (reference.ConventionError, OSError, ValueError) as exc:
        errors.append(f"Python case evaluation failed: {exc}")

    per_case: list[dict[str, object]] = []
    max_abs_error = 0.0
    max_tolerance_ratio = 0.0
    if python_rows and cpp_rows:
        comparisons, comparison_errors, max_abs_error, max_tolerance_ratio = compare_rows(python_rows, cpp_rows)
        per_case.extend(comparisons)
        errors.extend(comparison_errors)

    status = "passed" if not errors else "failed"
    implementation_hashes: dict[str, str] = {}
    for label, path in (
        ("case_set_sha256", cases_path),
        ("python_reference_sha256", reference_path),
        ("cross_tool_check_sha256", script_path),
        ("cpp_source_sha256", cpp_source_path),
        ("cpp_binary_sha256", cpp_path),
        ("cmake_cache_sha256", cmake_cache_path),
    ):
        if path.is_file():
            implementation_hashes[label] = sha256_file(path)

    report = {
        "schema_version": REPORT_SCHEMA,
        "report_id": "EVIDENCE-R0-SCI-001-CROSS-TOOL-001",
        "oracle_set_id": reference.ORACLE_ID,
        "generated_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "scope": ["binary64", "SI", "frame transforms", "scientific time", "Hamilton passive quaternion"],
        "authority_refs": [
            "docs/adr/0005-si-unit-and-numeric-conventions.md",
            "docs/adr/0006-frame-transform-conventions.md",
            "docs/adr/0007-time-value-conventions.md",
            "docs/adr/0008-quaternion-conventions.md",
            "design-notes/gnczmkn-architecture-roadmap/03-mathematics-and-numerical-foundation.md §6-§9, §22",
        ],
        "input": {
            "identity": "gnczmkn.scientific-conventions.cases/1",
            "repository_ref": "oracles/scientific-conventions/cases.csv",
            "applicable_domain": "finite proper 3D axis-angle rotations with finite vectors and translations",
            "case_count": len(python_rows),
        },
        "implementations": {
            "python": {
                "identity": reference.REFERENCE_ID,
                "standard_library_only": True,
                "self_test": python_self_test,
            },
            "cpp": {
                "identity": cpp_metadata.get("implementation_id", "gnczmkn.scientific-conventions.cpp-test/1"),
                "metadata": cpp_metadata,
                "property_checks": cpp_property_checks,
                "self_test_output": cpp_self_test_output,
            },
        },
        "environment": {
            "platform": platform.platform(),
            "python_implementation": platform.python_implementation(),
            "python_version": platform.python_version(),
            "cmake_cache": read_cmake_cache(cmake_cache_path) if cmake_cache_path.is_file() else {},
            "target_compile_options_source": "CMakeLists.txt + cmake/GncWarnings.cmake",
            "deterministic_property_seed": 20260809,
        },
        "hashes": implementation_hashes,
        "tolerance_policy": {
            "formula": "abs(cpp-python) <= atol + rtol * max(abs(cpp), abs(python))",
            "absolute_tolerance": reference.ABS_TOL,
            "relative_tolerance": reference.REL_TOL,
        },
        "verification_coverage": {
            "numeric_and_si": [
                "IEEE-754 binary64 assumptions",
                "km to m",
                "degree to radian",
                "degree Celsius to kelvin",
                "unknown and non-finite unit rejection",
                "below-zero kelvin rejection",
            ],
            "frame": [
                "column-vector R_to_from",
                "proper rotation",
                "inverse and composition",
                "point versus free-vector translation",
            ],
            "time": [
                "five distinct semantic kinds",
                "finite SI seconds",
                "typed arithmetic",
                "half-open validity interval",
                "clock-domain mismatch rejection",
            ],
            "quaternion": [
                "Hamilton multiplication",
                "passive inverse(q) times v times q",
                "q_b_a times q_c_b composition",
                "wxyz serialization",
                "q and negative q equivalence",
                "malformed, zero, non-finite, and rejected non-unit failure paths",
                "qualified Euler metadata",
            ],
        },
        "comparison": {
            "status": status,
            "case_count": len(per_case),
            "values_per_case": len(reference.RESULT_FIELDS) - 1,
            "max_abs_error": max_abs_error,
            "max_tolerance_ratio": max_tolerance_ratio,
            "cases": per_case,
            "errors": errors,
        },
        "approval": {
            "status": args.approval_status,
            "owner_role": "scientific_authority",
            "reviewer_role": "architecture_lead",
            "reviewer_assignee": args.reviewer_assignee or None,
            "reviewed_at": args.reviewed_at or None,
        },
        "result": status,
    }

    serialized = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output is None:
        sys.stdout.write(serialized)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(serialized, encoding="utf-8")
        print(f"cross-tool report {status}: {args.output}")
    if status != "passed":
        for error in errors:
            print(f" - {error}", file=sys.stderr)
    return 0 if status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())

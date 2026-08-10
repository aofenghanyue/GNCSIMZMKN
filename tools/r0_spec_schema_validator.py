#!/usr/bin/env python3
"""Task-local R0 schema and reference-closure validator.

This tool intentionally implements only the JSON Schema keywords used by the
three R0 evidence schemas. Unsupported schema keywords fail closed. It is test
tooling and is not a production framework API.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Sequence, Tuple


class ValidationFailure(Exception):
    pass


class SchemaDefinitionFailure(Exception):
    pass


def _object_without_duplicate_keys(pairs: Sequence[Tuple[str, Any]]) -> Dict[str, Any]:
    result: Dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValidationFailure(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def _reject_non_finite(token: str) -> None:
    raise ValidationFailure(f"non-finite JSON number: {token}")


def load_json(path: Path) -> Any:
    try:
        text = path.read_text(encoding="utf-8-sig")
    except OSError as error:
        raise ValidationFailure(f"cannot read {path}: {error}") from error

    try:
        return json.loads(
            text,
            object_pairs_hook=_object_without_duplicate_keys,
            parse_constant=_reject_non_finite,
        )
    except ValidationFailure:
        raise
    except (json.JSONDecodeError, UnicodeError) as error:
        raise ValidationFailure(f"invalid UTF-8 JSON in {path}: {error}") from error


SUPPORTED_SCHEMA_KEYWORDS = {
    "$schema",
    "$id",
    "$ref",
    "$defs",
    "title",
    "description",
    "x-stage",
    "x-maturity",
    "type",
    "required",
    "properties",
    "additionalProperties",
    "items",
    "minItems",
    "maxItems",
    "uniqueItems",
    "minLength",
    "maxLength",
    "pattern",
    "enum",
    "const",
    "minProperties",
    "maxProperties",
    "minimum",
    "maximum",
    "exclusiveMinimum",
    "exclusiveMaximum",
    "allOf",
    "anyOf",
    "oneOf",
    "not",
    "if",
    "then",
    "else",
}


def assert_supported_schema(schema: Any, path: str = "schema") -> None:
    if isinstance(schema, bool):
        return
    if not isinstance(schema, Mapping):
        raise SchemaDefinitionFailure(f"{path}: schema must be an object or boolean")

    for keyword, value in schema.items():
        if keyword not in SUPPORTED_SCHEMA_KEYWORDS:
            raise SchemaDefinitionFailure(f"{path}: unsupported schema keyword '{keyword}'")

        if keyword in ("properties", "$defs"):
            if not isinstance(value, Mapping):
                raise SchemaDefinitionFailure(f"{path}.{keyword}: expected object")
            for name, child in value.items():
                assert_supported_schema(child, f"{path}.{keyword}.{name}")
        elif keyword in ("items", "additionalProperties", "not", "if", "then", "else"):
            if keyword == "additionalProperties" and not isinstance(value, (bool, Mapping)):
                raise SchemaDefinitionFailure(f"{path}.{keyword}: expected schema or boolean")
            assert_supported_schema(value, f"{path}.{keyword}")
        elif keyword in ("allOf", "anyOf", "oneOf"):
            if not isinstance(value, list):
                raise SchemaDefinitionFailure(f"{path}.{keyword}: expected array")
            for index, child in enumerate(value):
                assert_supported_schema(child, f"{path}.{keyword}[{index}]")


def _json_equal(left: Any, right: Any) -> bool:
    if isinstance(left, bool) or isinstance(right, bool):
        return type(left) is type(right) and left == right
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        return float(left) == float(right)
    return type(left) is type(right) and left == right


def _json_fingerprint(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _type_matches(instance: Any, expected: str) -> bool:
    if expected == "object":
        return isinstance(instance, dict)
    if expected == "array":
        return isinstance(instance, list)
    if expected == "string":
        return isinstance(instance, str)
    if expected == "boolean":
        return isinstance(instance, bool)
    if expected == "null":
        return instance is None
    if expected == "integer":
        return isinstance(instance, int) and not isinstance(instance, bool)
    if expected == "number":
        return isinstance(instance, (int, float)) and not isinstance(instance, bool)
    raise SchemaDefinitionFailure(f"unsupported JSON Schema type: {expected}")


class JsonSchemaSubsetValidator:
    def __init__(self, root_schema: Mapping[str, Any]):
        self.root_schema = root_schema
        assert_supported_schema(root_schema)

    def validate(self, instance: Any) -> None:
        self._validate(instance, self.root_schema, "$")

    def _resolve_ref(self, reference: str) -> Any:
        if not reference.startswith("#/"):
            raise SchemaDefinitionFailure(f"only local JSON Pointer $ref values are supported: {reference}")
        current: Any = self.root_schema
        for token in reference[2:].split("/"):
            token = token.replace("~1", "/").replace("~0", "~")
            if not isinstance(current, Mapping) or token not in current:
                raise SchemaDefinitionFailure(f"unresolved schema reference: {reference}")
            current = current[token]
        return current

    def _matches(self, instance: Any, schema: Any, path: str) -> bool:
        try:
            self._validate(instance, schema, path)
            return True
        except ValidationFailure:
            return False

    def _validate(self, instance: Any, schema: Any, path: str) -> None:
        if schema is True:
            return
        if schema is False:
            raise ValidationFailure(f"{path}: rejected by false schema")
        if not isinstance(schema, Mapping):
            raise SchemaDefinitionFailure(f"{path}: schema must be object or boolean")

        if "$ref" in schema:
            self._validate(instance, self._resolve_ref(schema["$ref"]), path)

        if "allOf" in schema:
            for child in schema["allOf"]:
                self._validate(instance, child, path)
        if "anyOf" in schema:
            if not any(self._matches(instance, child, path) for child in schema["anyOf"]):
                raise ValidationFailure(f"{path}: does not match any allowed schema")
        if "oneOf" in schema:
            matches = sum(1 for child in schema["oneOf"] if self._matches(instance, child, path))
            if matches != 1:
                raise ValidationFailure(f"{path}: expected exactly one schema match, found {matches}")
        if "not" in schema and self._matches(instance, schema["not"], path):
            raise ValidationFailure(f"{path}: matches a forbidden schema")
        if "if" in schema:
            branch = schema.get("then") if self._matches(instance, schema["if"], path) else schema.get("else")
            if branch is not None:
                self._validate(instance, branch, path)

        if "const" in schema and not _json_equal(instance, schema["const"]):
            raise ValidationFailure(f"{path}: expected constant {schema['const']!r}")
        if "enum" in schema and not any(_json_equal(instance, value) for value in schema["enum"]):
            raise ValidationFailure(f"{path}: value {instance!r} is outside the closed enum")

        if "type" in schema:
            expected_types = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
            if not any(_type_matches(instance, expected) for expected in expected_types):
                raise ValidationFailure(f"{path}: expected type {expected_types}, got {type(instance).__name__}")

        if isinstance(instance, dict):
            required = schema.get("required", [])
            for name in required:
                if name not in instance:
                    raise ValidationFailure(f"{path}: missing required property '{name}'")

            if "minProperties" in schema and len(instance) < schema["minProperties"]:
                raise ValidationFailure(f"{path}: expected at least {schema['minProperties']} properties")
            if "maxProperties" in schema and len(instance) > schema["maxProperties"]:
                raise ValidationFailure(f"{path}: expected at most {schema['maxProperties']} properties")

            properties = schema.get("properties", {})
            for name, child in properties.items():
                if name in instance:
                    self._validate(instance[name], child, f"{path}.{name}")

            if "additionalProperties" in schema:
                unknown = [name for name in instance if name not in properties]
                policy = schema["additionalProperties"]
                if policy is False and unknown:
                    raise ValidationFailure(f"{path}: unknown property '{unknown[0]}'")
                if isinstance(policy, Mapping):
                    for name in unknown:
                        self._validate(instance[name], policy, f"{path}.{name}")

        if isinstance(instance, list):
            if "minItems" in schema and len(instance) < schema["minItems"]:
                raise ValidationFailure(f"{path}: expected at least {schema['minItems']} items")
            if "maxItems" in schema and len(instance) > schema["maxItems"]:
                raise ValidationFailure(f"{path}: expected at most {schema['maxItems']} items")
            if schema.get("uniqueItems"):
                fingerprints = [_json_fingerprint(value) for value in instance]
                if len(fingerprints) != len(set(fingerprints)):
                    raise ValidationFailure(f"{path}: array items must be unique")
            if "items" in schema:
                for index, value in enumerate(instance):
                    self._validate(value, schema["items"], f"{path}[{index}]")

        if isinstance(instance, str):
            if "minLength" in schema and len(instance) < schema["minLength"]:
                raise ValidationFailure(f"{path}: string is shorter than {schema['minLength']}")
            if "maxLength" in schema and len(instance) > schema["maxLength"]:
                raise ValidationFailure(f"{path}: string is longer than {schema['maxLength']}")
            if "pattern" in schema and re.search(schema["pattern"], instance) is None:
                raise ValidationFailure(f"{path}: value {instance!r} does not match required pattern")

        if isinstance(instance, (int, float)) and not isinstance(instance, bool):
            value = float(instance)
            if not math.isfinite(value):
                raise ValidationFailure(f"{path}: numeric value must be finite")
            if "minimum" in schema and value < schema["minimum"]:
                raise ValidationFailure(f"{path}: value is below minimum {schema['minimum']}")
            if "maximum" in schema and value > schema["maximum"]:
                raise ValidationFailure(f"{path}: value is above maximum {schema['maximum']}")
            if "exclusiveMinimum" in schema and value <= schema["exclusiveMinimum"]:
                raise ValidationFailure(f"{path}: value must exceed {schema['exclusiveMinimum']}")
            if "exclusiveMaximum" in schema and value >= schema["exclusiveMaximum"]:
                raise ValidationFailure(f"{path}: value must be below {schema['exclusiveMaximum']}")


def _path_has_uri_scheme(reference: str) -> bool:
    return re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", reference) is not None


def _reject_forbidden_local_reference_syntax(value: str, original_reference: str) -> None:
    if re.match(r"^[A-Za-z]:", value) is not None:
        raise ValidationFailure(
            f"Windows drive source reference is forbidden: {original_reference}"
        )
    if value.lower().startswith("file:"):
        raise ValidationFailure(f"file URI source reference is forbidden: {original_reference}")
    if value.startswith(("/", "\\")):
        raise ValidationFailure(f"absolute source path is forbidden: {original_reference}")


def _check_local_source_ref(root: Path, reference: str) -> None:
    _reject_forbidden_local_reference_syntax(reference, reference)

    if reference.lower().startswith("repo://"):
        relative = reference[len("repo://") :]
    elif reference.lower().startswith("repo:"):
        raise ValidationFailure(f"malformed repository source scheme: {reference}")
    elif _path_has_uri_scheme(reference):
        return
    else:
        relative = reference

    relative = relative.split("#", 1)[0]
    _reject_forbidden_local_reference_syntax(relative, reference)
    if not relative:
        raise ValidationFailure(f"source reference does not resolve: {reference}")
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError as error:
        raise ValidationFailure(f"source reference escapes repository: {reference}") from error
    if not candidate.is_file():
        raise ValidationFailure(f"source reference does not resolve to a file: {reference}")


def _iter_expected_fact_ids(document: Mapping[str, Any]) -> Iterable[str]:
    for fact in document.get("expected_facts", []):
        if isinstance(fact, Mapping) and isinstance(fact.get("fact_id"), str):
            yield fact["fact_id"]
    for oracle in document.get("oracles", []):
        if isinstance(oracle, Mapping):
            for fact in oracle.get("expected_facts", []):
                if isinstance(fact, Mapping) and isinstance(fact.get("fact_id"), str):
                    yield fact["fact_id"]


def _identity_entries(document: Mapping[str, Any]) -> Iterable[Tuple[str, str]]:
    for field in ("fixture_id", "oracle_set_id", "proof_id"):
        value = document.get(field)
        if isinstance(value, str):
            yield field, value
    for oracle in document.get("oracles", []):
        if isinstance(oracle, Mapping) and isinstance(oracle.get("id"), str):
            yield "oracle_id", oracle["id"]
    for fact_id in _iter_expected_fact_ids(document):
        yield "fact_id", fact_id


def _collect_documents(
    root: Path, reference_paths: Sequence[str], candidate_path: Path, candidate: Mapping[str, Any]
) -> List[Tuple[Path, Mapping[str, Any]]]:
    documents: List[Tuple[Path, Mapping[str, Any]]] = []
    candidate_resolved = candidate_path.resolve()
    for relative in reference_paths:
        path = (root / relative).resolve()
        try:
            path.relative_to(root.resolve())
        except ValueError as error:
            raise ValidationFailure(f"validation-set reference escapes repository: {relative}") from error
        if path == candidate_resolved:
            continue
        value = load_json(path)
        if not isinstance(value, Mapping):
            raise ValidationFailure(f"reference document must be an object: {relative}")
        documents.append((path, value))
    documents.append((candidate_resolved, candidate))
    return documents


def _detect_proof_cycles(proofs: Mapping[str, Mapping[str, Any]]) -> None:
    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(proof_id: str) -> None:
        if proof_id in visiting:
            raise ValidationFailure(f"proof prerequisite cycle includes: {proof_id}")
        if proof_id in visited:
            return
        visiting.add(proof_id)
        for premise in proofs[proof_id].get("premises", []):
            if not isinstance(premise, Mapping) or premise.get("kind") != "ProofReference":
                continue
            dependency = premise.get("proof_ref")
            if dependency in proofs:
                visit(dependency)
        visiting.remove(proof_id)
        visited.add(proof_id)

    for proof_id in proofs:
        visit(proof_id)


def validate_reference_closure(
    root: Path,
    provenance_ids: set[str],
    role_ids: set[str],
    task_ids: set[str],
    reference_paths: Sequence[str],
    candidate_path: Path,
    candidate: Mapping[str, Any],
) -> None:
    documents = _collect_documents(root, reference_paths, candidate_path, candidate)

    seen: Dict[Tuple[str, str], Path] = {}
    oracle_ids: set[str] = set()
    proofs: Dict[str, Mapping[str, Any]] = {}
    for path, document in documents:
        for kind, identity in _identity_entries(document):
            key = (kind, identity)
            if key in seen:
                raise ValidationFailure(f"duplicate {kind} identity: {identity}")
            seen[key] = path
            if kind == "oracle_id":
                oracle_ids.add(identity)
            elif kind == "proof_id":
                proofs[identity] = document

    for _, document in documents:
        authority = document.get("authority")
        if isinstance(authority, str) and authority not in role_ids:
            raise ValidationFailure(f"fixture authority does not resolve to a repository role: {authority}")

        for task_id in document.get("open_tasks", []):
            if task_id not in task_ids:
                raise ValidationFailure(
                    f"fixture open task does not resolve to a non-done backlog task: {task_id}"
                )

        for provenance_ref in document.get("provenance_refs", []):
            if provenance_ref not in provenance_ids:
                raise ValidationFailure(f"provenance reference does not resolve: {provenance_ref}")

        for source_ref in document.get("source_refs", []):
            _check_local_source_ref(root, source_ref)
        for oracle in document.get("oracles", []):
            if isinstance(oracle, Mapping):
                for source_ref in oracle.get("source_refs", []):
                    _check_local_source_ref(root, source_ref)
                for artifact_ref in oracle.get("artifact_refs", []):
                    _check_local_source_ref(root, artifact_ref)

        for oracle_ref in document.get("oracle_refs", []):
            if oracle_ref not in oracle_ids:
                raise ValidationFailure(f"oracle reference does not resolve: {oracle_ref}")

        proof_id = document.get("proof_id")
        premise_ids: List[str] = []
        for premise in document.get("premises", []):
            if not isinstance(premise, Mapping):
                continue
            premise_id = premise.get("premise_id")
            if isinstance(premise_id, str):
                premise_ids.append(premise_id)
            if premise.get("kind") != "ProofReference":
                continue
            proof_ref = premise.get("proof_ref")
            if proof_ref == proof_id:
                raise ValidationFailure(f"proof cannot reference itself: {proof_ref}")
            if proof_ref not in proofs:
                raise ValidationFailure(f"proof prerequisite does not resolve: {proof_ref}")
        if len(premise_ids) != len(set(premise_ids)):
            raise ValidationFailure(f"duplicate premise identity in proof: {proof_id}")

    _detect_proof_cycles(proofs)


EXPECTED_SCHEMAS = {
    "specs/fixture-manifest.schema.json": (
        "urn:gnczmkn:schema:r0:fixture-manifest:1",
        "gnczmkn.fixture-manifest/1",
    ),
    "specs/oracle-manifest.schema.json": (
        "urn:gnczmkn:schema:r0:oracle-manifest:1",
        "gnczmkn.oracle-manifest/1",
    ),
    "specs/plan-proof-record.schema.json": (
        "urn:gnczmkn:schema:r0:plan-proof-record:1",
        "gnczmkn.plan-proof-record/1",
    ),
}

PRODUCTION_FORBIDDEN_TOKENS = (
    "specs/fixture-manifest.schema.json",
    "specs/oracle-manifest.schema.json",
    "specs/plan-proof-record.schema.json",
    "urn:gnczmkn:schema:r0:fixture-manifest:1",
    "urn:gnczmkn:schema:r0:oracle-manifest:1",
    "urn:gnczmkn:schema:r0:plan-proof-record:1",
    "gnczmkn.fixture-manifest/1",
    "gnczmkn.oracle-manifest/1",
    "gnczmkn.plan-proof-record/1",
    "gnczmkn.fixture-manifest.placeholder/0",
    "gnczmkn.oracle-manifest.placeholder/0",
    "r0_spec_schema_validator.py",
    "verify-r0-spec-001.ps1",
)

PRODUCTION_SOURCE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".ipp",
    ".inl",
    ".cmake",
}


def _forbidden_tokens_in_text(text: str) -> List[str]:
    normalized = text.replace("\\", "/").lower()
    return [token for token in PRODUCTION_FORBIDDEN_TOKENS if token.lower() in normalized]


def validate_production_consumer_boundary(root: Path) -> int:
    for token in PRODUCTION_FORBIDDEN_TOKENS:
        sentinel = f"target-consumer={token}"
        matches = _forbidden_tokens_in_text(sentinel)
        if token not in matches:
            raise ValidationFailure(f"production guard sentinel did not detect token: {token}")
    print(
        f"[PASS] production guard sentinels: {len(PRODUCTION_FORBIDDEN_TOKENS)} forbidden tokens detected"
    )

    candidates: set[Path] = set()
    root_cmake = root / "CMakeLists.txt"
    if root_cmake.exists():
        candidates.add(root_cmake)

    for relative_root in ("apps", "framework", "adapters", "packages", "user", "cmake"):
        directory = root / relative_root
        if not directory.exists():
            continue
        for path in directory.rglob("*"):
            if not path.is_file():
                continue
            if path.name == "CMakeLists.txt" or path.suffix.lower() in PRODUCTION_SOURCE_SUFFIXES:
                candidates.add(path)

    violations: List[str] = []
    for path in sorted(candidates):
        try:
            text = path.read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as error:
            raise ValidationFailure(f"cannot scan production source {path}: {error}") from error
        for token in _forbidden_tokens_in_text(text):
            violations.append(f"{path.relative_to(root).as_posix()}: {token}")

    if violations:
        raise ValidationFailure(
            "production source consumes R0 evidence schema tooling: " + "; ".join(violations)
        )
    print(f"[PASS] production consumer scan: 0 forbidden references across {len(candidates)} files")
    return len(candidates)


def validate_schema_identity(relative_path: str, schema: Mapping[str, Any]) -> None:
    if relative_path not in EXPECTED_SCHEMAS:
        raise SchemaDefinitionFailure(f"unexpected schema path in case manifest: {relative_path}")
    expected_id, expected_version = EXPECTED_SCHEMAS[relative_path]
    if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        raise SchemaDefinitionFailure(f"{relative_path}: wrong JSON Schema dialect")
    if schema.get("$id") != expected_id:
        raise SchemaDefinitionFailure(f"{relative_path}: wrong stable $id")
    if schema.get("x-stage") != "R0" or schema.get("x-maturity") != "Fixture":
        raise SchemaDefinitionFailure(f"{relative_path}: wrong stage or maturity")
    version_schema = schema.get("properties", {}).get("schema_version", {})
    if version_schema.get("const") != expected_version:
        raise SchemaDefinitionFailure(f"{relative_path}: wrong instance version constant")


def _load_provenance_ids(path: Path) -> set[str]:
    register = load_json(path)
    if not isinstance(register, Mapping) or not isinstance(register.get("records"), list):
        raise ValidationFailure("provenance register has no records array")
    ids: List[str] = []
    for record in register["records"]:
        if not isinstance(record, Mapping) or not isinstance(record.get("record_id"), str):
            raise ValidationFailure("provenance register contains a record without record_id")
        ids.append(record["record_id"])
    if len(ids) != len(set(ids)):
        raise ValidationFailure("provenance register contains duplicate record_id values")
    return set(ids)


def _load_role_ids(path: Path) -> set[str]:
    assignments = load_json(path)
    if not isinstance(assignments, Mapping) or not isinstance(assignments.get("roles"), list):
        raise ValidationFailure("role assignments have no roles array")
    ids: List[str] = []
    for role in assignments["roles"]:
        if not isinstance(role, Mapping) or not isinstance(role.get("id"), str):
            raise ValidationFailure("role assignments contain a role without id")
        ids.append(role["id"])
    if len(ids) != len(set(ids)):
        raise ValidationFailure("role assignments contain duplicate role ids")
    return set(ids)


VALID_TASK_STATUSES = {"planned", "ready", "in_progress", "blocked", "review", "done"}
OPEN_TASK_STATUSES = VALID_TASK_STATUSES - {"done"}


def _extract_open_task_ids(tasks: Any) -> set[str]:
    if not isinstance(tasks, list):
        raise ValidationFailure("task backlog has no tasks array")
    ids: List[str] = []
    open_ids: List[str] = []
    for task in tasks:
        if not isinstance(task, Mapping) or not isinstance(task.get("id"), str):
            raise ValidationFailure("task backlog contains a task without id")
        status = task.get("status")
        if status not in VALID_TASK_STATUSES:
            raise ValidationFailure(f"task backlog contains unknown status: {status}")
        ids.append(task["id"])
        if status in OPEN_TASK_STATUSES:
            open_ids.append(task["id"])
    if len(ids) != len(set(ids)):
        raise ValidationFailure("task backlog contains duplicate task ids")
    return set(open_ids)


def _load_task_ids(path: Path) -> set[str]:
    backlog = load_json(path)
    if not isinstance(backlog, Mapping):
        raise ValidationFailure("task backlog must be an object")
    return _extract_open_task_ids(backlog.get("tasks"))


def _validate_case_manifest(root: Path, manifest: Mapping[str, Any]) -> None:
    if manifest.get("schema_version") != "gnczmkn.r0-spec-conformance-cases/1":
        raise ValidationFailure("case manifest has unknown schema_version")
    if not isinstance(manifest.get("reference_documents"), list):
        raise ValidationFailure("case manifest is missing reference_documents")
    if not isinstance(manifest.get("cases"), list) or not manifest["cases"]:
        raise ValidationFailure("case manifest is missing cases")
    if not isinstance(manifest.get("schema_negative_cases"), list):
        raise ValidationFailure("case manifest is missing schema_negative_cases")

    listed = {case.get("document") for case in manifest["cases"] if isinstance(case, Mapping)}
    discovered: set[str] = set()
    for directory in (root / "tests/r0-spec-001/valid", root / "tests/r0-spec-001/invalid"):
        for path in directory.glob("*.json"):
            discovered.add(path.relative_to(root).as_posix())
    if listed != discovered:
        missing = sorted(discovered - listed)
        extra = sorted(listed - discovered)
        raise ValidationFailure(f"case manifest coverage mismatch; missing={missing}, extra={extra}")

    listed_schema_negatives = {
        case.get("schema")
        for case in manifest["schema_negative_cases"]
        if isinstance(case, Mapping)
    }
    discovered_schema_negatives = {
        path.relative_to(root).as_posix()
        for path in (root / "tests/r0-spec-001/invalid-schema").glob("*.json")
    }
    if listed_schema_negatives != discovered_schema_negatives:
        raise ValidationFailure(
            "schema negative coverage mismatch; "
            f"listed={sorted(listed_schema_negatives)}, "
            f"discovered={sorted(discovered_schema_negatives)}"
        )


def validate_existing_manifest_inventory(
    root: Path,
    classification_path: Path,
    validators: Mapping[str, JsonSchemaSubsetValidator],
    provenance_ids: set[str],
    role_ids: set[str],
    task_ids: set[str],
) -> Tuple[int, int]:
    classification = load_json(classification_path)
    if not isinstance(classification, Mapping):
        raise ValidationFailure("existing-manifest classification must be an object")
    if classification.get("schema_version") != "gnczmkn.r0-spec-existing-manifest-classification/1":
        raise ValidationFailure("existing-manifest classification has unknown schema_version")
    entries = classification.get("manifests")
    if not isinstance(entries, list):
        raise ValidationFailure("existing-manifest classification is missing manifests")

    discovered_paths = {
        path.relative_to(root).as_posix()
        for base, pattern in ((root / "fixtures", "fixture-manifest.json"), (root / "oracles", "oracle-manifest.json"))
        for path in base.rglob(pattern)
    }
    classified_paths = {
        entry.get("path") for entry in entries if isinstance(entry, Mapping)
    }
    if discovered_paths != classified_paths or len(classified_paths) != len(entries):
        raise ValidationFailure(
            "existing-manifest inventory mismatch; "
            f"discovered={sorted(discovered_paths)}, classified={sorted(classified_paths)}"
        )

    conforming_paths = [
        entry["path"]
        for entry in entries
        if isinstance(entry, Mapping) and entry.get("disposition") == "conforming_v1"
    ]
    conforming_count = 0
    placeholder_count = 0

    for entry in entries:
        if not isinstance(entry, Mapping):
            raise ValidationFailure("existing-manifest classification entry must be an object")
        relative_path = entry.get("path")
        document_kind = entry.get("document_kind")
        disposition = entry.get("disposition")
        expected_version = entry.get("declared_schema_version")
        retained_identity = entry.get("retained_identity")
        owner_task = entry.get("owner_task")
        blockers = entry.get("blockers")

        if document_kind not in ("fixture", "oracle"):
            raise ValidationFailure(f"classification has invalid document_kind: {relative_path}")
        if not isinstance(owner_task, str) or not owner_task:
            raise ValidationFailure(f"classification has no owner_task: {relative_path}")
        if not isinstance(blockers, list):
            raise ValidationFailure(f"classification has invalid blockers: {relative_path}")

        path = root / relative_path
        document = load_json(path)
        if not isinstance(document, Mapping):
            raise ValidationFailure(f"existing manifest must be an object: {relative_path}")
        if document.get("schema_version") != expected_version:
            raise ValidationFailure(f"classified schema_version drift: {relative_path}")
        identity_field = "fixture_id" if document_kind == "fixture" else "oracle_set_id"
        if document.get(identity_field) != retained_identity:
            raise ValidationFailure(f"classified identity drift: {relative_path}")

        if disposition == "placeholder_gate_blocked":
            required_placeholder = (
                "gnczmkn.fixture-manifest.placeholder/0"
                if document_kind == "fixture"
                else "gnczmkn.oracle-manifest.placeholder/0"
            )
            if expected_version != required_placeholder:
                raise ValidationFailure(f"placeholder has non-placeholder version: {relative_path}")
            if not blockers or not all(isinstance(value, str) and value for value in blockers):
                raise ValidationFailure(f"placeholder has no factual gate blocker: {relative_path}")
            placeholder_count += 1
        elif disposition == "conforming_v1":
            required_version = (
                "gnczmkn.fixture-manifest/1"
                if document_kind == "fixture"
                else "gnczmkn.oracle-manifest/1"
            )
            schema_path = (
                "specs/fixture-manifest.schema.json"
                if document_kind == "fixture"
                else "specs/oracle-manifest.schema.json"
            )
            if expected_version != required_version or blockers:
                raise ValidationFailure(f"conforming v1 classification is inconsistent: {relative_path}")
            validators[schema_path].validate(document)
            validate_reference_closure(
                root,
                provenance_ids,
                role_ids,
                task_ids,
                conforming_paths,
                path,
                document,
            )
            conforming_count += 1
        else:
            raise ValidationFailure(f"unknown manifest disposition: {relative_path}")

    return conforming_count, placeholder_count


def run_cases(
    root: Path,
    case_manifest_path: Path,
    provenance_path: Path,
    classification_path: Path,
    role_assignments_path: Path,
    backlog_path: Path,
) -> int:
    manifest_value = load_json(case_manifest_path)
    if not isinstance(manifest_value, Mapping):
        raise ValidationFailure("case manifest must be a JSON object")
    _validate_case_manifest(root, manifest_value)
    provenance_ids = _load_provenance_ids(provenance_path)
    role_ids = _load_role_ids(role_assignments_path)
    task_ids = _load_task_ids(backlog_path)

    try:
        _extract_open_task_ids([{"id": "R0-SENTINEL-001", "status": "unknown"}])
    except ValidationFailure as error:
        if "unknown status" not in str(error):
            raise
        print("[PASS] task status fail-closed sentinel")
    else:
        raise ValidationFailure("unknown task status sentinel unexpectedly passed")

    validators: Dict[str, JsonSchemaSubsetValidator] = {}
    for relative_path in EXPECTED_SCHEMAS:
        schema_value = load_json(root / relative_path)
        if not isinstance(schema_value, Mapping):
            raise SchemaDefinitionFailure(f"{relative_path}: schema must be an object")
        validate_schema_identity(relative_path, schema_value)
        validators[relative_path] = JsonSchemaSubsetValidator(schema_value)

    production_file_count = validate_production_consumer_boundary(root)

    schema_negative_count = 0
    for schema_case in manifest_value["schema_negative_cases"]:
        name = schema_case.get("name", "unnamed schema negative")
        relative_path = schema_case.get("schema")
        expected_fragment = schema_case.get("expected_error_contains")
        try:
            schema_value = load_json(root / relative_path)
            assert_supported_schema(schema_value)
            error_text = None
        except (ValidationFailure, SchemaDefinitionFailure) as error:
            error_text = str(error)
        if error_text is None:
            raise ValidationFailure(f"{name}: invalid schema unexpectedly passed")
        if not isinstance(expected_fragment, str) or expected_fragment.lower() not in error_text.lower():
            raise ValidationFailure(
                f"{name}: expected error containing {expected_fragment!r}, got: {error_text}"
            )
        schema_negative_count += 1
        print(f"[PASS] {name}: rejected ({error_text})")

    conforming_existing, placeholder_existing = validate_existing_manifest_inventory(
        root,
        classification_path,
        validators,
        provenance_ids,
        role_ids,
        task_ids,
    )
    print(
        "[PASS] existing manifest inventory: "
        f"{conforming_existing} conforming v1, {placeholder_existing} gate-blocked placeholder v0"
    )

    failures: List[str] = []
    valid_count = 0
    invalid_count = 0
    references = manifest_value["reference_documents"]

    for case in manifest_value["cases"]:
        name = case.get("name", "unnamed")
        schema_path = case.get("schema")
        document_path = case.get("document")
        expected_valid = case.get("expected_valid")
        expected_fragment = case.get("expected_error_contains")

        try:
            if schema_path not in validators:
                raise ValidationFailure(f"case names unknown schema: {schema_path}")
            document_file = root / document_path
            document = load_json(document_file)
            validators[schema_path].validate(document)
            if not isinstance(document, Mapping):
                raise ValidationFailure("validated document must be an object")
            case_references = list(references) + list(case.get("additional_reference_documents", []))
            validate_reference_closure(
                root,
                provenance_ids,
                role_ids,
                task_ids,
                case_references,
                document_file,
                document,
            )
            error_text = None
        except (ValidationFailure, SchemaDefinitionFailure) as error:
            error_text = str(error)

        if expected_valid is True:
            valid_count += 1
            if error_text is None:
                print(f"[PASS] {name}: valid")
            else:
                failures.append(f"{name}: expected valid, got: {error_text}")
                print(f"[FAIL] {name}: {error_text}")
        elif expected_valid is False:
            invalid_count += 1
            if error_text is None:
                failures.append(f"{name}: invalid case unexpectedly passed")
                print(f"[FAIL] {name}: invalid case unexpectedly passed")
            elif not isinstance(expected_fragment, str) or expected_fragment.lower() not in error_text.lower():
                failures.append(
                    f"{name}: expected error containing {expected_fragment!r}, got: {error_text}"
                )
                print(f"[FAIL] {name}: wrong diagnostic: {error_text}")
            else:
                print(f"[PASS] {name}: rejected ({error_text})")
        else:
            failures.append(f"{name}: expected_valid must be boolean")

    print(
        f"R0-SPEC-001 conformance summary: {valid_count} valid cases, "
        f"{invalid_count} required rejection cases, {schema_negative_count} invalid schema case, "
        f"{conforming_existing} conforming existing manifest, "
        f"{placeholder_existing} gate-blocked placeholders, "
        f"0 forbidden production references across {production_file_count} files, "
        f"{len(failures)} failures"
    )
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}", file=sys.stderr)
        return 1
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument(
        "--case-manifest",
        default="tests/r0-spec-001/cases.json",
        type=Path,
    )
    parser.add_argument(
        "--provenance-register",
        default="docs/quality/provenance-register.json",
        type=Path,
    )
    parser.add_argument(
        "--manifest-classification",
        default="docs/quality/r0-spec-001/existing-manifest-classification.json",
        type=Path,
    )
    parser.add_argument(
        "--role-assignments",
        default="docs/team/role-assignments.json",
        type=Path,
    )
    parser.add_argument(
        "--task-backlog",
        default="docs/tasks/backlog.json",
        type=Path,
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    case_manifest = args.case_manifest
    provenance_register = args.provenance_register
    manifest_classification = args.manifest_classification
    role_assignments = args.role_assignments
    task_backlog = args.task_backlog
    if not case_manifest.is_absolute():
        case_manifest = root / case_manifest
    if not provenance_register.is_absolute():
        provenance_register = root / provenance_register
    if not manifest_classification.is_absolute():
        manifest_classification = root / manifest_classification
    if not role_assignments.is_absolute():
        role_assignments = root / role_assignments
    if not task_backlog.is_absolute():
        task_backlog = root / task_backlog
    try:
        return run_cases(
            root,
            case_manifest,
            provenance_register,
            manifest_classification,
            role_assignments,
            task_backlog,
        )
    except (ValidationFailure, SchemaDefinitionFailure) as error:
        print(f"R0-SPEC-001 validation error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Independent canonical-byte and SHA-256 reference for the R2 YYZ IR."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
from typing import Any, Dict, List


ENCODING_ID = "gnc.canonical-mission-ir.semantic-bytes@1"
ASSET_SCHEMA_ID = "gnc.asset.yyz.aerodynamic-table.multiaffine@1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


class Encoder:
    def __init__(self) -> None:
        self.data = bytearray()

    def record(self, tag: int) -> None:
        self.data.append(0x08)
        self.data.extend(struct.pack(">I", tag))

    def string(self, value: str) -> None:
        payload = value.encode("utf-8")
        self.data.append(0x01)
        self.data.extend(struct.pack(">I", len(payload)))
        self.data.extend(payload)

    def uint32(self, value: int) -> None:
        self.data.append(0x02)
        self.data.extend(struct.pack(">I", value))

    def integer(self, value: int) -> None:
        self.data.append(0x03)
        self.data.extend(struct.pack(">q", value))

    def enumeration(self, domain: int, value: int) -> None:
        self.data.extend((0x04, 0x01))
        self.data.extend(struct.pack(">II", domain, value))

    def enum_token(self, value: str) -> None:
        payload = value.encode("utf-8")
        self.data.extend((0x04, 0x02))
        self.data.extend(struct.pack(">I", len(payload)))
        self.data.extend(payload)

    def float64(self, value: float) -> None:
        self.data.append(0x05)
        self.data.extend(struct.pack(">d", value))

    def collection(self, count: int) -> None:
        self.data.append(0x06)
        self.data.extend(struct.pack(">I", count))

    def optional(self, present: bool) -> None:
        self.data.extend((0x07, 1 if present else 0))


def normalize_ir(value: Dict[str, Any]) -> Dict[str, Any]:
    ir = copy.deepcopy(value)
    ir["entities"].sort(key=lambda item: item["entity_id"])
    ir["scopes"].sort(
        key=lambda item: (item["kind"], item["subject_entity_id"]))
    ir["models"].sort(key=lambda item: item["occurrence_id"])
    for model in ir["models"]:
        model["output_ports"].sort(key=lambda item: item["port_id"])
        model["configuration"]["fields"].sort(
            key=lambda item: item["field_id"])
        model["assets"].sort(key=lambda item: item["role"])
    ir["algorithms"].sort(key=lambda item: item["consumer_id"])
    for algorithm in ir["algorithms"]:
        algorithm["input_ports"].sort(key=lambda item: item["port_id"])
    ir["bindings"].sort(key=lambda item: item["binding_id"])
    return ir


def require_unique_order(values: List[Dict[str, Any]], key: str,
                         label: str) -> None:
    actual = [item[key] for item in values]
    require(actual == sorted(actual) and len(actual) == len(set(actual)),
            f"{label} is not in unique canonical order")


def validate_canonical(ir: Dict[str, Any]) -> None:
    require(ir["revision"] == 1 and ir["mission_id"],
            "mission semantics are invalid")
    require_unique_order(ir["entities"], "entity_id", "entities")
    entity_ids = {item["entity_id"] for item in ir["entities"]}
    require(all(item["lifecycle"] == 1 for item in ir["entities"]),
            "entity lifecycle is invalid")
    scope_keys = [(item["kind"], item["subject_entity_id"])
                  for item in ir["scopes"]]
    require(scope_keys == sorted(scope_keys) and
            len(scope_keys) == len(set(scope_keys)),
            "scopes are not in unique canonical order")
    require(all(kind == 1 and subject in entity_ids
                for kind, subject in scope_keys),
            "Vehicle scope is invalid or unanchored")
    require_unique_order(ir["models"], "occurrence_id", "models")
    for model in ir["models"]:
        require(model["package_id"] and model["package_version"] and
                model["model_id"] and model["model_version"] and
                model["execution_form"] in (1, 2) and
                model["placement"] in (1, 2) and
                model["preparation_algorithm_id"] and
                model["preparation_algorithm_version"],
                "model identity or package policy is invalid")
        subject = model.get("subject_entity_id")
        require(not subject or subject in entity_ids,
                "model subject is absent")
        scope = model.get("scope")
        require(not scope or
                ((scope["kind"], scope["subject_entity_id"]) in scope_keys and
                 scope["subject_entity_id"] == subject),
                "model scope and subject are inconsistent")
        require_unique_order(model["output_ports"], "port_id",
                             "model ports")
        configuration = model["configuration"]
        require(configuration["schema_id"] and
                configuration["schema_version"] == 1,
                "configuration schema is invalid")
        require_unique_order(configuration["fields"], "field_id",
                             "configuration fields")
        for field in configuration["fields"]:
            kind = field["kind"]
            value = field["value"]
            require(kind in ("string", "integer", "enum", "float64"),
                    "configuration value kind is invalid")
            if kind in ("string", "enum"):
                require(isinstance(value, str) and bool(value),
                        "canonical text value is empty")
            elif kind == "integer":
                require(isinstance(value, int) and not isinstance(value, bool),
                        "canonical integer type differs")
            else:
                require(isinstance(value, (int, float)) and
                        not isinstance(value, bool) and
                        math.isfinite(float(value)) and
                        not (float(value) == 0.0 and
                             math.copysign(1.0, float(value)) < 0.0),
                        "canonical float64 is invalid")
        require_unique_order(model["assets"], "role", "asset bindings")
        require(all(item["asset_schema_id"] and item["asset_id"]
                    for item in model["assets"]),
                "asset identity is invalid")
    require_unique_order(ir["algorithms"], "consumer_id", "algorithms")
    require_unique_order(ir["bindings"], "binding_id", "bindings")


def encode_config_value(encoder: Encoder, field: Dict[str, Any]) -> None:
    kind = field["kind"]
    value = field["value"]
    if kind == "string":
        encoder.string(value)
    elif kind == "integer":
        encoder.integer(value)
    elif kind == "enum":
        encoder.enum_token(value)
    elif kind == "float64":
        encoder.float64(float(value))
    else:
        raise ValueError(f"unsupported config kind: {kind}")


def encode_canonical(ir: Dict[str, Any]) -> bytes:
    validate_canonical(ir)
    encoder = Encoder()
    encoder.record(1)
    encoder.string(ENCODING_ID)
    encoder.uint32(ir["revision"])
    encoder.string(ir["mission_id"])

    encoder.collection(len(ir["entities"]))
    for entity in ir["entities"]:
        encoder.record(2)
        encoder.string(entity["entity_id"])
        encoder.enumeration(1, entity["lifecycle"])

    encoder.collection(len(ir["scopes"]))
    for scope in ir["scopes"]:
        encoder.record(3)
        encoder.enumeration(2, scope["kind"])
        encoder.string(scope["subject_entity_id"])

    encoder.collection(len(ir["models"]))
    for model in ir["models"]:
        encoder.record(4)
        for key in ("occurrence_id", "package_id", "package_version",
                    "model_id", "model_version"):
            encoder.string(model[key])
        encoder.enumeration(3, model["execution_form"])
        encoder.enumeration(4, model["placement"])
        encoder.string(model["preparation_algorithm_id"])
        encoder.string(model["preparation_algorithm_version"])
        subject = model.get("subject_entity_id", "")
        encoder.optional(bool(subject))
        if subject:
            encoder.string(subject)
        scope = model.get("scope")
        encoder.optional(scope is not None)
        if scope is not None:
            encoder.enumeration(2, scope["kind"])
            encoder.string(scope["subject_entity_id"])
        encoder.collection(len(model["output_ports"]))
        for port in model["output_ports"]:
            encoder.record(5)
            encoder.string(port["port_id"])
            encoder.string(port["contract_id"])
        configuration = model["configuration"]
        encoder.record(6)
        encoder.string(configuration["schema_id"])
        encoder.uint32(configuration["schema_version"])
        encoder.collection(len(configuration["fields"]))
        for field in configuration["fields"]:
            encoder.record(7)
            encoder.string(field["field_id"])
            encode_config_value(encoder, field)
        encoder.collection(len(model["assets"]))
        for asset in model["assets"]:
            encoder.record(8)
            encoder.string(asset["role"])
            encoder.string(asset["asset_schema_id"])
            encoder.string(asset["asset_id"])

    encoder.collection(len(ir["algorithms"]))
    for algorithm in ir["algorithms"]:
        encoder.record(9)
        for key in ("consumer_id", "package_id", "package_version",
                    "algorithm_id", "algorithm_version"):
            encoder.string(algorithm[key])
        encoder.collection(len(algorithm["input_ports"]))
        for port in algorithm["input_ports"]:
            encoder.record(5)
            encoder.string(port["port_id"])
            encoder.string(port["contract_id"])

    encoder.collection(len(ir["bindings"]))
    for binding in ir["bindings"]:
        encoder.record(10)
        for key in ("binding_id", "provider_occurrence_id",
                    "provider_port_id", "consumer_id", "consumer_port_id"):
            encoder.string(binding[key])
    return bytes(encoder.data)


def digest(ir: Dict[str, Any]) -> str:
    return hashlib.sha256(encode_canonical(ir)).hexdigest()


def verify_provenance(source: Dict[str, Any],
                      asset_index: Dict[str, Any],
                      ir: Dict[str, Any]) -> None:
    qualification = source["profiles"]["qualification"]
    vehicle = qualification["vehicle"]
    aero_asset = next(item for item in asset_index["selected_assets"]
                      if item["role"] == "aerodynamics")
    models = {item["occurrence_id"]: item for item in ir["models"]}
    closure_fields = {
        item["field_id"]: item["value"]
        for item in models["force_moment_closure"]
        ["configuration"]["fields"]}
    aero_fields = {
        item["field_id"]: item["value"]
        for item in models["aero_lookup"]["configuration"]["fields"]}
    require(ir["mission_id"] == source["source_id"] and
            ir["entities"][0]["entity_id"] == vehicle["subject"] and
            closure_fields["body_frame_id"] == vehicle["body_frame_id"] and
            closure_fields["clock_domain_id"] ==
            qualification["clock"]["clock_domain"] and
            closure_fields["configuration_revision"] ==
            vehicle["configuration_revision"],
            "canonical mission/scope/closure facts diverge from source.json")
    payload = aero_asset["payload"]
    asset_binding = models["aero_lookup"]["assets"][0]
    require(aero_asset["asset_schema_id"] == ASSET_SCHEMA_ID and
            asset_binding["asset_schema_id"] == ASSET_SCHEMA_ID and
            asset_binding["asset_id"] == aero_asset["asset_id"] and
            aero_fields["configuration_id"] == payload["configuration_id"] and
            aero_fields["reference_area_square_meters"] ==
            payload["reference_area_m2"] and
            aero_fields["reference_chord_meters"] ==
            payload["reference_chord_m"] and
            aero_fields["reference_span_meters"] ==
            payload["reference_span_m"] and
            [aero_fields[f"body_origin_to_application.{axis}_m"]
             for axis in ("x", "y", "z")] ==
            payload["r_body_origin_to_application_B_m"],
            "canonical aero config/asset diverges from asset-index.json")


def verify_mutations(base: Dict[str, Any], expected: str) -> int:
    equivalent = copy.deepcopy(base)
    equivalent["source_ref"] = {
        "uri": "repo://relocated/qualification.json", "path": "/mission"}
    equivalent["entities"].reverse()
    equivalent["scopes"].reverse()
    equivalent["models"].reverse()
    for model in equivalent["models"]:
        model["output_ports"].reverse()
        model["configuration"]["fields"].reverse()
        model["assets"].reverse()
    require(digest(normalize_ir(equivalent)) == expected,
            "representation order or source location changed reference hash")

    mutations: List[Dict[str, Any]] = []
    entity = copy.deepcopy(base)
    entity["entities"][0]["entity_id"] = "vehicle.fixture.yyz-renamed@1"
    entity["scopes"][0]["subject_entity_id"] = \
        entity["entities"][0]["entity_id"]
    for model in entity["models"]:
        model["subject_entity_id"] = entity["entities"][0]["entity_id"]
        model["scope"]["subject_entity_id"] = \
            entity["entities"][0]["entity_id"]
    mutations.append(entity)

    placement = copy.deepcopy(base)
    placement["models"][0]["placement"] = 2
    mutations.append(placement)
    model_identity = copy.deepcopy(base)
    model_identity["models"][0]["model_version"] = "0.1.1"
    mutations.append(model_identity)
    config = copy.deepcopy(base)
    config["models"][1]["configuration"]["fields"][3]["value"] = 3e-12
    mutations.append(config)
    asset = copy.deepcopy(base)
    asset["models"][0]["assets"][0]["asset_id"] = \
        "aero-table.fixture.yyz.alternate@1"
    mutations.append(asset)
    require(all(digest(normalize_ir(item)) != expected for item in mutations),
            "a semantic mutation retained the reference hash")

    scope_base = copy.deepcopy(base)
    scope_base["entities"].append({
        "entity_id": "vehicle.fixture.yyz.alternate@1", "lifecycle": 1})
    scope_base["scopes"].append({
        "kind": 1,
        "subject_entity_id": "vehicle.fixture.yyz.alternate@1"})
    scope_base = normalize_ir(scope_base)
    scope_changed = copy.deepcopy(scope_base)
    scope_changed["models"][0]["subject_entity_id"] = \
        "vehicle.fixture.yyz.alternate@1"
    scope_changed["models"][0]["scope"]["subject_entity_id"] = \
        "vehicle.fixture.yyz.alternate@1"
    require(digest(scope_base) != digest(scope_changed),
            "scope semantic mutation retained the reference hash")

    noncanonical = copy.deepcopy(base)
    noncanonical["models"].reverse()
    try:
        encode_canonical(noncanonical)
    except ValueError:
        pass
    else:
        raise ValueError("noncanonical model order reached reference SHA-256")
    negative_zero = copy.deepcopy(base)
    negative_zero["models"][0]["configuration"]["fields"][0]["value"] = -0.0
    try:
        encode_canonical(negative_zero)
    except ValueError:
        pass
    else:
        raise ValueError("negative zero reached reference SHA-256")
    return len(mutations) + 3


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--asset-index", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    arguments = parser.parse_args()

    fixture = json.loads(arguments.cases.read_text(encoding="utf-8"))
    source = json.loads(arguments.source.read_text(encoding="utf-8"))
    asset_index = json.loads(
        arguments.asset_index.read_text(encoding="utf-8"))
    require(fixture["encoding_id"] == ENCODING_ID and
            fixture["hash_algorithm"] == "SHA-256",
            "hash fixture identity differs")
    canonical = normalize_ir(fixture["ir"])
    verify_provenance(source, asset_index, canonical)
    reference_hash = digest(canonical)
    require(reference_hash == fixture["expected_sha256"],
            "independent Python hash differs from the stored vector")
    mutation_checks = verify_mutations(canonical, reference_hash)

    completed = subprocess.run(
        [str(arguments.probe), "--semantic-hash"], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    product_hash = completed.stdout.strip()
    require(product_hash == reference_hash,
            "C++ canonical semantic hash differs from Python reference")
    print(json.dumps({
        "case_id": fixture["case_id"],
        "encoding_id": ENCODING_ID,
        "sha256": reference_hash,
        "representation_and_mutation_checks": mutation_checks,
        "status": "passed"
    }, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit(
            f"R2 canonical IR hash reference failed: {error}") from error

#!/usr/bin/env python3
"""Build and verify the fixture-local canonical REF-YYZ-001 R0 bundle."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal, InvalidOperation, getcontext
import importlib.util
import json
from pathlib import Path, PurePosixPath
import sys


FIXTURE_ID = "REF-YYZ-001"
ORACLE_ID = "ORACLE-YYZ-001"
MODEL_ID = "MODEL-YYZ-R0-QUALIFICATION-BUNDLE-001"
SOURCE_ID = "mission.fixture.yyz.lookup-altitude-hold@1"
ASSET_INDEX_ID = "asset-index.fixture.yyz.r0-qualification@1"
CASE_ID = "CASE-YYZ-001-R0-QUALIFICATION"
REFERENCE_SCHEMA = "gnczmkn.yyz-r0-qualification-reference/1"
SOURCE_SCHEMA = "gnczmkn.yyz-r0-qualification-source/1"
ASSET_SCHEMA = "gnczmkn.yyz-r0-asset-index/1"
CASES_SCHEMA = "gnczmkn.yyz-r0-qualification-cases/1"
MISSION_CASE_ID = "CASE-YYZ-MISSION-COMPOSITION-BASELINE"
FROZEN_CASE_ID = "CASE-YYZ-FROZEN-INTERVAL-COMPOSED-TRAJECTORY"

INVALID_IDS = [
    "INVALID-YYZ-001-TARGET-PROFILE-AS-EXECUTABLE",
    "INVALID-YYZ-001-TARGET-VERDICT-CONTRIBUTION",
    "INVALID-YYZ-001-SOURCE-INITIAL-MASS",
    "INVALID-YYZ-001-ASSET-BINDING-MISSING",
    "INVALID-YYZ-001-AERO-TABLE-VALUE",
    "INVALID-YYZ-001-TOLERANCE-WIDENING",
]

ASSET_ROLES = [
    "environment",
    "mass_properties",
    "aerodynamics",
    "propulsion",
    "guidance_control",
    "numerical_policy",
    "termination_observation",
]

ASSET_METADATA = {
    "environment": (
        "environment.fixture.yyz.uniform@1",
        "fixtures/ref-yyz-frozen-interval/cases.json#/cases/CASE-YYZ-FROZEN-INTERVAL-COMPOSED-TRAJECTORY/environment_sample",
    ),
    "mass_properties": (
        "mass-properties.fixture.yyz.constant-geometry@1",
        "fixtures/ref-yyz-frozen-interval/cases.json#/cases/CASE-YYZ-FROZEN-INTERVAL-COMPOSED-TRAJECTORY/mass_properties_sample",
    ),
    "aerodynamics": (
        "aero-table.fixture.yyz.multiaffine@1",
        "fixtures/ref-yyz-frozen-interval/cases.json#/cases/CASE-YYZ-FROZEN-INTERVAL-COMPOSED-TRAJECTORY/aero_lookup",
    ),
    "propulsion": (
        "propulsion-response.fixture.yyz.main@1",
        "fixtures/ref-yyz-frozen-interval/cases.json#/cases/CASE-YYZ-FROZEN-INTERVAL-COMPOSED-TRAJECTORY/propulsion_response",
    ),
    "guidance_control": (
        "guidance-control.fixture.yyz.altitude-pitch@1",
        "fixtures/ref-yyz-mission-composition/cases.json#/cases/CASE-YYZ-MISSION-COMPOSITION-BASELINE/guidance_control",
    ),
    "numerical_policy": (
        "numerical.fixture.yyz.rk4-frozen-interval@1",
        "fixtures/ref-yyz-mission-composition/cases.json#/model",
    ),
    "termination_observation": (
        "termination-observation.fixture.yyz.two-interval@1",
        "fixtures/ref-yyz-mission-composition/cases.json#/cases/CASE-YYZ-MISSION-COMPOSITION-BASELINE",
    ),
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream, parse_float=Decimal)
    require(isinstance(value, dict), f"JSON root is not an object: {path}")
    return value


def decimal(value: object) -> Decimal:
    if isinstance(value, bool):
        raise ValueError("boolean is not numeric")
    result = value if isinstance(value, Decimal) else Decimal(str(value))
    require(result.is_finite(), "numeric value must be finite")
    return result


def numeric_string(value: object) -> bool:
    if not isinstance(value, str):
        return False
    try:
        return Decimal(value).is_finite()
    except (InvalidOperation, ValueError):
        return False


def encode_decimal(value: Decimal) -> str:
    if value.is_zero():
        return "0"
    encoded = format(value, "f")
    if "." in encoded:
        encoded = encoded.rstrip("0").rstrip(".")
    return encoded


def json_ready(value):
    if isinstance(value, Decimal):
        return encode_decimal(value)
    if isinstance(value, list):
        return [json_ready(item) for item in value]
    if isinstance(value, dict):
        return {key: json_ready(item) for key, item in value.items()}
    return value


def write_json(path: Path, value: dict) -> int:
    output = json.dumps(json_ready(value), indent=2, ensure_ascii=False) + "\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(output)
    return len(output.encode("utf-8"))


def relative_json_path(repo_root: Path, value: object,
                       required_prefix: str | None = None) -> Path:
    require(isinstance(value, str) and value and "\\" not in value,
            "repository JSON path must be a nonempty POSIX path")
    logical = PurePosixPath(value)
    require(not logical.is_absolute() and ".." not in logical.parts and
            logical.suffix == ".json",
            f"invalid repository JSON path: {value}")
    if required_prefix is not None:
        require(logical.parts and logical.parts[0] == required_prefix,
                f"repository path must remain under {required_prefix}: {value}")
    path = (repo_root / Path(*logical.parts)).resolve()
    try:
        path.relative_to(repo_root.resolve())
    except ValueError as error:
        raise ValueError(f"repository path escapes root: {value}") from error
    require(path.is_file(), f"repository JSON path is missing: {value}")
    return path


def repo_relative(repo_root: Path, path: Path) -> str:
    return path.resolve().relative_to(repo_root.resolve()).as_posix()


def load_mission_module(repo_root: Path):
    module_path = repo_root / "tools" / "yyz_mission_composition_reference.py"
    spec = importlib.util.spec_from_file_location(
        "yyz_canonical_mission_reference", module_path)
    require(spec is not None and spec.loader is not None,
            "mission reference module cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def find_case(values: list[dict], identifier: str, label: str) -> dict:
    selected = [value for value in values if value.get("id") == identifier]
    require(len(selected) == 1, f"{label} identity differs")
    return selected[0]


def find_asset(assets: dict, role: str) -> dict:
    selected = [value for value in assets["selected_assets"]
                if value.get("role") == role]
    require(len(selected) == 1, f"selected asset role differs: {role}")
    return selected[0]


def load_documents(cases_path: Path, repo_root: Path) -> tuple[dict, ...]:
    cases = load_json(cases_path)
    source_path = relative_json_path(
        repo_root, cases.get("source_path"), "fixtures")
    asset_path = relative_json_path(
        repo_root, cases.get("asset_index_path"), "fixtures")
    mission_cases_path = relative_json_path(
        repo_root, cases.get("mission_cases_path"), "fixtures")
    mission_oracle_path = relative_json_path(
        repo_root, cases.get("mission_oracle_path"), "oracles")
    return (
        cases,
        load_json(source_path),
        load_json(asset_path),
        load_json(mission_cases_path),
        load_json(mission_oracle_path),
        source_path,
        asset_path,
        mission_cases_path,
        mission_oracle_path,
    )


def expected_asset_payloads(frozen: dict, mission_case: dict,
                            mission_model: dict) -> dict[str, dict]:
    environment = frozen["environment_sample"]
    mass = frozen["mass_properties_sample"]
    aero = frozen["aero_lookup"]
    table = aero["prepared_table"]
    propulsion = frozen["propulsion_response"]
    control = mission_case["guidance_control"]
    return {
        "environment": {
            "gravity_I_mps2": environment["gravity_I_mps2"],
            "velocity_airmass_I_mps": environment["velocity_airmass_I_mps"],
            "density_kgpm3": environment["density_kgpm3"],
            "speed_of_sound_mps": environment["speed_of_sound_mps"],
        },
        "mass_properties": {
            "initial_mass_kg": mass["mass_kg"],
            "r_body_origin_to_CoM_B_m":
                mass["r_body_origin_to_CoM_B_m"],
            "inertia_about_CoM_B_kgm2_row_major":
                mass["inertia_about_CoM_B_kgm2_row_major"],
        },
        "aerodynamics": {
            "model_id": aero["model_id"],
            "configuration_id": aero["configuration_id"],
            "reference_area_m2": aero["reference_area_m2"],
            "reference_span_m": aero["reference_span_m"],
            "reference_chord_m": aero["reference_chord_m"],
            "r_body_origin_to_application_B_m":
                aero["r_body_origin_to_application_B_m"],
            "layout": table["layout"],
            "mach_axis": table["mach_axis"],
            "alpha_axis_rad": table["alpha_axis_rad"],
            "beta_axis_rad": table["beta_axis_rad"],
            "coefficient_rows_CA_CY_CN_Cl_Cm_Cn":
                table["coefficient_rows_CA_CY_CN_Cl_Cm_Cn"],
        },
        "propulsion": {
            "thrust_magnitude_N": propulsion["thrust_magnitude_N"],
            "thrust_direction_B_unit":
                propulsion["thrust_direction_B_unit"],
            "r_body_origin_to_application_B_m":
                propulsion["r_body_origin_to_application_B_m"],
            "intrinsic_moment_at_application_B_Nm":
                propulsion["intrinsic_moment_at_application_B_Nm"],
            "fuel_consumption_rate_kgps":
                propulsion["fuel_consumption_rate_kgps"],
        },
        "guidance_control": {
            key: control[key] for key in (
                "altitude_error_gain_rad_per_m",
                "vertical_speed_gain_rad_s_per_m",
                "pitch_command_limit_rad",
                "pitch_error_gain_Nm_per_rad",
                "pitch_rate_gain_Nm_s_per_rad",
                "moment_command_limit_Nm",
                "realization_gain",
            )
        },
        "numerical_policy": {
            "integration_strategy": mission_model["integration_strategy"],
            "integrator": "classical-RK4",
            "base_dt_s": mission_model["base_dt_s"],
            "convergence_substeps": [1, 2, 4, 8],
            "minimum_error_reduction_ratio": Decimal("12"),
        },
        "termination_observation": {
            "termination_plan_id": mission_case["termination_plan_id"],
            "terminal_tick": mission_model["terminal_tick"],
            "expected_reason_code": "downrange-goal",
            "terminal_observation_fields":
                mission_case["terminal_observation_fields"],
        },
    }


def validate_cases(cases: dict) -> None:
    required = {
        "schema_version", "fixture_id", "oracle_id", "model_id",
        "source_id", "asset_index_id", "source_path", "asset_index_path",
        "mission_cases_path", "mission_oracle_path", "accepted_case",
        "difference_policy", "invalid_input_cases",
    }
    require(set(cases) == required, "canonical cases fields differ")
    require(cases["schema_version"] == CASES_SCHEMA and
            cases["fixture_id"] == FIXTURE_ID and
            cases["oracle_id"] == ORACLE_ID and
            cases["model_id"] == MODEL_ID and
            cases["source_id"] == SOURCE_ID and
            cases["asset_index_id"] == ASSET_INDEX_ID,
            "canonical cases identity differs")
    accepted = cases["accepted_case"]
    require(accepted == {
        "id": CASE_ID,
        "qualification_profile_id":
            "profile.fixture.yyz.r0-qualification@1",
        "target_profile_id": "profile.target.yyz.00a-altitude-hold@1",
        "composition_case_id": MISSION_CASE_ID,
        "expected_component_count": 12,
        "expected_asset_count": 7,
        "expected_committed_sample_count": 3,
        "expected_interval_count": 2,
        "expected_terminal_tick": 2,
        "expected_terminal_reason_code": "downrange-goal",
        "expected_invalid_input_rejections": 14,
        "expected_mutation_rejections": 10,
        "expected_structured_diagnostics": 3,
    }, "canonical accepted case differs")
    policy = cases["difference_policy"]
    require(decimal(policy["formula_absolute"]) == Decimal("2e-12") and
            decimal(policy["formula_relative"]) == Decimal("2e-12") and
            decimal(policy["convergence_ratio_absolute"]) ==
                Decimal("5e-4") and
            decimal(policy["minimum_convergence_error_reduction_ratio"]) ==
                Decimal("12") and
            decimal(policy["stored_decimal_absolute"]) == Decimal("1e-68") and
            policy["report_granularity"] ==
                "one entry for every C++ probe leaf compared with the independent Python oracle",
            "canonical difference policy differs")
    invalid = cases["invalid_input_cases"]
    require(isinstance(invalid, list) and
            [item.get("id") for item in invalid] == INVALID_IDS and
            all(item.get("expected_status") == "input-domain-error"
                for item in invalid),
            "canonical invalid input cases differ")


def validate_source(source: dict, cases: dict, mission_cases: dict,
                    frozen: dict, mission_case: dict) -> None:
    require(source["schema_version"] == SOURCE_SCHEMA and
            source["fixture_id"] == FIXTURE_ID and
            source["source_id"] == SOURCE_ID,
            "canonical source identity differs")
    require(source["qualification_choice"]["status"] == "accepted" and
            source["qualification_choice"]["scope"] ==
                "R0 fixture-local scientific qualification" and
            len(source["qualification_choice"]["locked_boundaries"]) == 3,
            "qualification choice differs")
    qualification = source["profiles"]["qualification"]
    target = source["profiles"]["target_architecture"]
    require(qualification["profile_id"] ==
                cases["accepted_case"]["qualification_profile_id"] and
            qualification["status"] == "executable" and
            target["profile_id"] ==
                cases["accepted_case"]["target_profile_id"] and
            target["status"] == "target_pending" and
            target["scientific_verdict_contribution"] == "none",
            "qualification and target profile boundary differs")
    require(target["base_rate_hz"] == 100 and
            decimal(target["duration_s"]) == Decimal("30") and
            target["component_rates_hz"] == {
                "navigation": 100,
                "guidance": 20,
                "controller": 50,
                "plant_output": 100,
                "observation": 25,
            } and
            all(100 % value == 0
                for value in target["component_rates_hz"].values()),
            "target architecture rate profile differs")
    target_initial = target["author_initial_values"]
    require(target_initial["latitude_deg"] == Decimal("31.2304") and
            target_initial["longitude_deg"] == Decimal("121.4737") and
            target_initial["altitude_m"] == Decimal("1000") and
            target_initial["speed_mps"] == Decimal("220") and
            target_initial["heading_deg"] == Decimal("90") and
            target_initial["flight_path_deg"] == Decimal("0") and
            target_initial["bank_deg"] == Decimal("0") and
            target_initial["mass_kg"] == Decimal("680") and
            len(target["pending_capabilities"]) == 6,
            "target architecture author values differ")

    model = mission_cases["model"]
    clock = qualification["clock"]
    require(clock["clock_domain"] == model["clock_domain"] and
            decimal(clock["base_dt_s"]) == decimal(model["base_dt_s"]) and
            decimal(clock["base_rate_hz"]) * decimal(clock["base_dt_s"]) ==
                Decimal(1) and
            clock["initial_tick"] == model["initial_tick"] and
            clock["terminal_tick"] == model["terminal_tick"] and
            decimal(clock["duration_s"]) ==
                decimal(model["base_dt_s"]) *
                (model["terminal_tick"] - model["initial_tick"]) and
            clock["evaluation_mode"] == model["evaluation_mode"],
            "qualification clock differs from mission composition")
    require(set(qualification["schedule"].values()) == {
                1, "current-cycle-held-over-[t_k,t_k+1)"},
            "qualification schedule differs")
    vehicle = qualification["vehicle"]
    require(vehicle["subject"] == model["subject"] and
            vehicle["lifecycle"] == "active_at_initialize" and
            vehicle["inertial_frame_id"] == model["inertial_frame_id"] and
            vehicle["body_frame_id"] == model["body_frame_id"] and
            vehicle["configuration_id"] == model["configuration_id"] and
            vehicle["configuration_revision"] ==
                model["configuration_revision"] and
            vehicle["mass_state_id"] == model["mass_state_id"],
            "qualification vehicle identity differs")
    initial = vehicle["initial_state"]
    require(initial["position_I_m"] == frozen["initial_state"]["position_I_m"] and
            initial["velocity_I_mps"] == frozen["initial_state"]["velocity_I_mps"] and
            initial["q_I_B_wxyz"] == frozen["initial_state"]["q_I_B_wxyz"] and
            initial["omega_BI_B_radps"] ==
                frozen["initial_state"]["omega_BI_B_radps"] and
            initial["committed_mass_kg"] ==
                frozen["mass_properties_sample"]["mass_kg"],
            "qualification initial condition differs")
    require(initial["velocity_I_mps"][0] != target_initial["speed_mps"] and
            initial["committed_mass_kg"] != target_initial["mass_kg"],
            "qualification and target profile values were conflated")
    control = qualification["guidance_control"]
    selected_control = mission_case["guidance_control"]
    for key in (
            "guidance_law_id", "controller_law_id", "actuation_model_id",
            "observation_source", "target_altitude_m", "altitude_axis",
            "attitude_projection", "realized_axis"):
        require(control[key] == selected_control[key],
                f"qualification guidance/control identity differs: {key}")
    termination = qualification["termination"]
    require(termination["plan_id"] == mission_case["termination_plan_id"] and
            termination["expected_terminal_tick"] == model["terminal_tick"] and
            termination["expected_action"] == "Complete" and
            termination["expected_reason_code"] == "downrange-goal" and
            termination["terminal_observation_must_precede_result"] is True,
            "qualification termination contract differs")
    composition = source["composition"]
    require(composition["fixture_id"] == mission_cases["fixture_id"] and
            composition["oracle_id"] == mission_cases["oracle_id"] and
            composition["model_id"] == mission_cases["model"]["model_id"] and
            composition["case_id"] == MISSION_CASE_ID and
            composition["cases_path"] == cases["mission_cases_path"] and
            composition["oracle_path"] == cases["mission_oracle_path"] and
            source["asset_index_ref"] == cases["asset_index_path"] and
            set(source["semantic_coverage"]) == {
                "source", "step", "observation", "diagnostic",
                "trajectory_and_terminal", "tolerance_and_difference"},
            "canonical source composition mapping differs")


def validate_assets(assets: dict, cases: dict, mission_cases: dict,
                    frozen: dict, mission_case: dict) -> None:
    require(assets["schema_version"] == ASSET_SCHEMA and
            assets["fixture_id"] == FIXTURE_ID and
            assets["asset_index_id"] == ASSET_INDEX_ID and
            assets["source_id"] == SOURCE_ID,
            "canonical asset index identity differs")
    require(assets["component_bindings"] ==
                mission_cases["component_bindings"] and
            len(assets["component_bindings"]) ==
                cases["accepted_case"]["expected_component_count"],
            "canonical component bindings differ")
    selected = assets["selected_assets"]
    require(isinstance(selected, list) and
            [item.get("role") for item in selected] == ASSET_ROLES and
            len(selected) == cases["accepted_case"]["expected_asset_count"],
            "canonical selected asset roles differ")
    expected = expected_asset_payloads(
        frozen, mission_case, mission_cases["model"])
    for role in ASSET_ROLES:
        asset = find_asset(assets, role)
        require(asset["payload"] == expected[role],
                f"canonical selected asset payload differs: {role}")
        expected_id, expected_path = ASSET_METADATA[role]
        require(asset["asset_id"] == expected_id and
                asset["source_case_path"] == expected_path,
                f"canonical selected asset identity differs: {role}")
    require(len(assets["applicability"]) == 5,
            "canonical asset applicability differs")


def validate_documents(cases: dict, source: dict, assets: dict,
                       mission_cases: dict, mission_oracle: dict,
                       repo_root: Path, mission_module) -> tuple[dict, dict]:
    validate_cases(cases)
    mission_module.validate_cases_identity(mission_cases)
    require(mission_oracle["fixture_id"] == mission_cases["fixture_id"] and
            mission_oracle["oracle_id"] == mission_cases["oracle_id"] and
            mission_oracle["model_id"] == mission_cases["model"]["model_id"],
            "mission composition oracle identity differs")
    frozen_binding = [item for item in mission_cases["component_bindings"]
                      if item["role"] == "frozen_interval"]
    require(len(frozen_binding) == 1,
            "mission composition FrozenInterval binding differs")
    frozen_path = relative_json_path(
        repo_root, frozen_binding[0]["cases_path"], "fixtures")
    frozen_cases = load_json(frozen_path)
    frozen = find_case(frozen_cases["cases"], FROZEN_CASE_ID,
                       "canonical FrozenInterval source")
    mission_case = find_case(
        mission_cases["cases"], MISSION_CASE_ID,
        "canonical mission composition source")
    validate_source(source, cases, mission_cases, frozen, mission_case)
    validate_assets(assets, cases, mission_cases, frozen, mission_case)
    return frozen, mission_case


def invalid_rejections(cases: dict, source: dict, assets: dict,
                       mission_cases: dict, mission_oracle: dict,
                       repo_root: Path, mission_module) -> list[str]:
    def rejected(mutator) -> bool:
        candidate_cases = copy.deepcopy(cases)
        candidate_source = copy.deepcopy(source)
        candidate_assets = copy.deepcopy(assets)
        mutator(candidate_cases, candidate_source, candidate_assets)
        try:
            validate_documents(
                candidate_cases, candidate_source, candidate_assets,
                mission_cases, mission_oracle, repo_root, mission_module)
        except (IndexError, KeyError, TypeError, ValueError):
            return True
        return False

    mutations = [
        lambda _c, source_value, _a:
            source_value["profiles"]["target_architecture"].update(
                {"status": "executable"}),
        lambda _c, source_value, _a:
            source_value["profiles"]["target_architecture"].update(
                {"scientific_verdict_contribution": "pass"}),
        lambda _c, source_value, _a:
            source_value["profiles"]["qualification"]["vehicle"]
                ["initial_state"].update({"committed_mass_kg": Decimal("101")}),
        lambda _c, _s, asset_value:
            asset_value["component_bindings"].pop(),
        lambda _c, _s, asset_value:
            find_asset(asset_value, "aerodynamics")["payload"]
                ["coefficient_rows_CA_CY_CN_Cl_Cm_Cn"][0]
                .__setitem__(0, Decimal("0.007")),
        lambda cases_value, _s, _a:
            cases_value["difference_policy"].update(
                {"formula_absolute": "1e-6"}),
    ]
    require(len(mutations) == len(INVALID_IDS),
            "canonical invalid mutation implementation differs")
    require(all(rejected(mutation) for mutation in mutations),
            "canonical invalid input was accepted")
    return list(INVALID_IDS)


def expected_probe_tree(mission_oracle: dict) -> dict:
    return {
        "oracle_id": mission_oracle["oracle_id"],
        "model_id": mission_oracle["model_id"],
        "status": "passed",
        "cases": list(mission_oracle["cases"].values()),
        "equivalence_results": mission_oracle["equivalence_results"],
        "invalid_input_rejections":
            mission_oracle["invalid_input_rejections"],
        "mutation_results": mission_oracle["mutation_results"],
        "diagnostic_results": mission_oracle["diagnostic_results"],
    }


def build_difference_report(actual, expected, policy: dict) -> dict:
    absolute = decimal(policy["formula_absolute"])
    relative = decimal(policy["formula_relative"])
    convergence = decimal(policy["convergence_ratio_absolute"])
    entries: list[dict] = []
    object_count = 0
    list_count = 0
    numeric_differences: list[tuple[Decimal, str]] = []

    def compare(actual_value, expected_value, path: str) -> None:
        nonlocal object_count, list_count
        if isinstance(expected_value, dict):
            object_count += 1
            require(isinstance(actual_value, dict), f"{path} is not an object")
            require(set(actual_value) == set(expected_value),
                    f"{path} object fields differ")
            for key, child in expected_value.items():
                compare(actual_value[key], child, f"{path}.{key}")
            return
        if isinstance(expected_value, list):
            list_count += 1
            require(isinstance(actual_value, list) and
                    len(actual_value) == len(expected_value),
                    f"{path} list shape differs")
            for index, child in enumerate(expected_value):
                compare(actual_value[index], child, f"{path}[{index}]")
            return
        if numeric_string(expected_value):
            observed = decimal(actual_value)
            expected_numeric = decimal(expected_value)
            difference = abs(observed - expected_numeric)
            bound = absolute + relative * max(
                abs(observed), abs(expected_numeric), Decimal(1))
            policy_id = "formula-state-abs-rel"
            if ".error_reduction_ratios[" in path:
                bound = max(bound, convergence)
                policy_id = "convergence-ratio-absolute"
            status = "passed" if difference <= bound else "failed"
            entries.append({
                "path": path,
                "comparison": "numeric",
                "policy_id": policy_id,
                "expected": encode_decimal(expected_numeric),
                "observed": encode_decimal(observed),
                "absolute_difference": encode_decimal(difference),
                "allowed_bound": encode_decimal(bound),
                "status": status,
            })
            numeric_differences.append((difference, path))
            return
        status = "passed" if actual_value == expected_value else "failed"
        entries.append({
            "path": path,
            "comparison": "exact",
            "policy_id": "exact",
            "expected": actual_value if status == "passed" else expected_value,
            "observed": actual_value,
            "status": status,
        })

    compare(actual, expected, "$probe")
    failed = [entry for entry in entries if entry["status"] != "passed"]
    maximum = max(numeric_differences, default=(Decimal(0), ""))
    summaries = []
    for policy_id in (
            "exact", "formula-state-abs-rel",
            "convergence-ratio-absolute"):
        selected = [entry for entry in entries
                    if entry["policy_id"] == policy_id]
        selected_differences = [
            decimal(entry["absolute_difference"]) for entry in selected
            if entry["comparison"] == "numeric"]
        summaries.append({
            "policy_id": policy_id,
            "field_count": len(selected),
            "failed_field_count": sum(
                entry["status"] != "passed" for entry in selected),
            "maximum_absolute_difference": encode_decimal(
                max(selected_differences, default=Decimal(0))),
        })
    return {
        "report_kind": "python-oracle-to-cpp17-probe-leaf-comparison",
        "object_shapes_checked": object_count,
        "list_shapes_checked": list_count,
        "leaf_field_count": len(entries),
        "exact_field_count": sum(
            entry["comparison"] == "exact" for entry in entries),
        "numeric_field_count": sum(
            entry["comparison"] == "numeric" for entry in entries),
        "failed_field_count": len(failed),
        "maximum_absolute_difference": encode_decimal(maximum[0]),
        "maximum_absolute_difference_path": maximum[1],
        "policy_summaries": summaries,
        "fields": entries,
    }


def validate_stored_difference_report(stored: dict, expected: dict,
                                      policy: dict) -> None:
    skeleton = build_difference_report(expected, expected, policy)
    require(stored["report_kind"] == skeleton["report_kind"] and
            stored["object_shapes_checked"] ==
                skeleton["object_shapes_checked"] and
            stored["list_shapes_checked"] ==
                skeleton["list_shapes_checked"] and
            stored["leaf_field_count"] == skeleton["leaf_field_count"] and
            stored["exact_field_count"] == skeleton["exact_field_count"] and
            stored["numeric_field_count"] ==
                skeleton["numeric_field_count"],
            "stored difference report coverage differs")
    require(len(stored["fields"]) == len(skeleton["fields"]),
            "stored difference report field count differs")
    absolute = decimal(policy["formula_absolute"])
    relative = decimal(policy["formula_relative"])
    convergence = decimal(policy["convergence_ratio_absolute"])
    numeric_differences: list[tuple[Decimal, str]] = []
    policy_counts = {key: [0, 0, Decimal(0)] for key in (
        "exact", "formula-state-abs-rel", "convergence-ratio-absolute")}
    failed_count = 0
    for baseline, expected_entry in zip(stored["fields"], skeleton["fields"]):
        require(baseline["path"] == expected_entry["path"] and
                baseline["comparison"] == expected_entry["comparison"] and
                baseline["policy_id"] == expected_entry["policy_id"] and
                baseline["expected"] == expected_entry["expected"],
                "stored difference report expected field differs")
        policy_state = policy_counts[baseline["policy_id"]]
        policy_state[0] += 1
        if baseline["comparison"] == "exact":
            require(baseline["observed"] == baseline["expected"] and
                    baseline["status"] == "passed" and
                    set(baseline) == {
                        "path", "comparison", "policy_id", "expected",
                        "observed", "status"},
                    f"stored exact difference differs: {baseline['path']}")
            continue
        observed = decimal(baseline["observed"])
        expected_numeric = decimal(baseline["expected"])
        difference = abs(observed - expected_numeric)
        bound = absolute + relative * max(
            abs(observed), abs(expected_numeric), Decimal(1))
        if baseline["policy_id"] == "convergence-ratio-absolute":
            bound = max(bound, convergence)
        passed = difference <= bound
        require(baseline["absolute_difference"] ==
                    encode_decimal(difference) and
                baseline["allowed_bound"] == encode_decimal(bound) and
                baseline["status"] == ("passed" if passed else "failed") and
                set(baseline) == {
                    "path", "comparison", "policy_id", "expected",
                    "observed", "absolute_difference", "allowed_bound",
                    "status"},
                f"stored numeric difference differs: {baseline['path']}")
        numeric_differences.append((difference, baseline["path"]))
        policy_state[2] = max(policy_state[2], difference)
        if not passed:
            failed_count += 1
            policy_state[1] += 1
    maximum = max(numeric_differences, default=(Decimal(0), ""))
    require(stored["failed_field_count"] == failed_count == 0 and
            stored["maximum_absolute_difference"] ==
                encode_decimal(maximum[0]) and
            stored["maximum_absolute_difference_path"] == maximum[1],
            "stored difference report summary differs")
    expected_summaries = [{
        "policy_id": key,
        "field_count": values[0],
        "failed_field_count": values[1],
        "maximum_absolute_difference": encode_decimal(values[2]),
    } for key, values in policy_counts.items()]
    require(stored["policy_summaries"] == expected_summaries,
            "stored difference report policy summary differs")


def build_header(cases: dict, source: dict, assets: dict,
                 mission_oracle: dict, mission_checks: int,
                 invalid: list[str], repo_root: Path, source_path: Path,
                 asset_path: Path, mission_cases_path: Path,
                 mission_oracle_path: Path) -> dict:
    accepted = mission_oracle["cases"][MISSION_CASE_ID]
    convergence = accepted["second_interval_convergence"]
    minimum_ratio = min(decimal(value)
                        for value in convergence["error_reduction_ratios"])
    require(minimum_ratio >= decimal(
        cases["difference_policy"]["minimum_convergence_error_reduction_ratio"]),
        "mission convergence ratio is below qualification threshold")
    opening = accepted["committed_samples"][0]
    intermediate = accepted["committed_samples"][1]
    closing = accepted["committed_samples"][2]
    control = accepted["interval_executions"][1]["guidance_control"]
    return {
        "schema_version": REFERENCE_SCHEMA,
        "fixture_id": FIXTURE_ID,
        "oracle_id": ORACLE_ID,
        "model_id": MODEL_ID,
        "case_id": CASE_ID,
        "precision": {
            "independent_python_decimal_digits":
                mission_oracle["precision"]["decimal_digits"],
            "stored_reference_policy":
                "exact regeneration from the independent Python producer",
            "cross_implementation_policy":
                "field-specific exact or numeric comparison against the independent Python oracle",
        },
        "source_identity": {
            "source_id": source["source_id"],
            "source_path": repo_relative(repo_root, source_path),
            "asset_index_id": assets["asset_index_id"],
            "asset_index_path": repo_relative(repo_root, asset_path),
            "mission_cases_path":
                repo_relative(repo_root, mission_cases_path),
            "mission_oracle_path":
                repo_relative(repo_root, mission_oracle_path),
            "qualification_profile_id":
                source["profiles"]["qualification"]["profile_id"],
            "qualification_status":
                source["profiles"]["qualification"]["status"],
            "target_profile_id":
                source["profiles"]["target_architecture"]["profile_id"],
            "target_status":
                source["profiles"]["target_architecture"]["status"],
            "target_scientific_verdict_contribution":
                source["profiles"]["target_architecture"]
                    ["scientific_verdict_contribution"],
        },
        "asset_resolution": {
            "component_count": len(assets["component_bindings"]),
            "selected_asset_count": len(assets["selected_assets"]),
            "components": accepted["resolved_components"],
            "selected_assets": [{
                "role": item["role"],
                "asset_id": item["asset_id"],
                "status": "passed",
            } for item in assets["selected_assets"]],
        },
        "semantic_coverage": {
            "source": {
                "status": "passed",
                "initial_condition_fields": 5,
            },
            "step": {
                "status": "passed",
                "execution_events": len(accepted["execution_trace"]),
                "committed_samples": len(accepted["committed_samples"]),
                "intervals": len(accepted["interval_executions"]),
            },
            "observation": {
                "status": "passed",
                "sample_tick":
                    accepted["terminal_observation"]["sample_tick"],
                "commit_id":
                    accepted["terminal_observation"]["commit_id"],
                "sealed": accepted["terminal_observation"]["sealed"],
            },
            "diagnostic": {
                "status": "passed",
                "structured_failures":
                    len(mission_oracle["diagnostic_results"]),
                "codes": [value["diagnostic_record"]["code"]
                          for value in mission_oracle["diagnostic_results"]],
            },
            "target_runtime": {
                "status": "target_pending",
                "passing_scientific_fields": 0,
            },
        },
        "trajectory_terminal_summary": {
            "opening_tick": opening["sample_tick"],
            "opening_mass_kg": opening["committed_mass_kg"],
            "intermediate_tick": intermediate["sample_tick"],
            "intermediate_altitude_I_z_m": intermediate["position_I_m"][2],
            "intermediate_vertical_speed_I_z_mps":
                intermediate["velocity_I_mps"][2],
            "tick1_pitch_command_rad":
                control["guidance"]["pitch_command_rad"],
            "tick1_moment_command_Nm":
                control["controller"]["moment_command_Nm"],
            "closing_tick": closing["sample_tick"],
            "closing_time_s": closing["time_s"],
            "closing_mass_kg": closing["committed_mass_kg"],
            "closing_downrange_m":
                accepted["metric_summary"]["downrange_m"],
            "closing_pitch_rate_B_y_radps":
                closing["omega_BI_B_radps"][1],
            "minimum_convergence_error_reduction_ratio":
                encode_decimal(minimum_ratio),
            "terminal_reason_code":
                accepted["mission_result"]["termination"]["reason_code"],
            "terminal_observation_sealed":
                accepted["terminal_observation"]["sealed"],
        },
        "inherited_verification": {
            "mission_oracle_id": mission_oracle["oracle_id"],
            "mission_reference_recomputed": True,
            "mission_python_cpp_checks": mission_checks,
            "mission_invalid_input_rejections":
                len(mission_oracle["invalid_input_rejections"]),
            "mission_mutation_rejections":
                len(mission_oracle["mutation_results"]),
            "mission_structured_diagnostics":
                len(mission_oracle["diagnostic_results"]),
        },
        "canonical_invalid_input_rejections": invalid,
        "difference_policy": cases["difference_policy"],
    }


def execute_current(cases: dict, source: dict, assets: dict,
                    mission_cases: dict, mission_oracle: dict,
                    repo_root: Path, mission_module, probe_path: Path,
                    source_path: Path, asset_path: Path,
                    mission_cases_path: Path,
                    mission_oracle_path: Path) -> tuple[dict, dict]:
    validate_documents(cases, source, assets, mission_cases, mission_oracle,
                       repo_root, mission_module)
    recomputed = mission_module.build_reference(
        mission_cases, repo_root, mission_cases_path)
    require(recomputed == mission_oracle,
            "stored mission composition oracle differs from its producer")
    mission_verification = mission_module.verify_reference(
        mission_cases, repo_root, mission_cases_path,
        mission_oracle, probe_path)
    require(mission_verification["status"] == "passed",
            "mission composition verification failed")
    invalid = invalid_rejections(
        cases, source, assets, mission_cases, mission_oracle,
        repo_root, mission_module)
    _stdout, probe = mission_module.run_probe(probe_path)
    expected = expected_probe_tree(mission_oracle)
    report = build_difference_report(
        probe, expected, cases["difference_policy"])
    require(report["failed_field_count"] == 0,
            "current C++ probe has failed difference fields")
    header = build_header(
        cases, source, assets, mission_oracle,
        int(mission_verification["checks"]), invalid, repo_root,
        source_path, asset_path, mission_cases_path, mission_oracle_path)
    return header, report


def verify_oracle(stored: dict, header: dict, current_report: dict,
                  mission_oracle: dict, policy: dict) -> dict:
    require(set(stored) == set(header) | {"difference_report"},
            "canonical stored oracle fields differ")
    for key, value in header.items():
        require(stored[key] == value,
                f"canonical stored oracle header differs: {key}")
    expected = expected_probe_tree(mission_oracle)
    validate_stored_difference_report(
        stored["difference_report"], expected, policy)
    require(current_report["failed_field_count"] == 0 and
            current_report["leaf_field_count"] ==
                stored["difference_report"]["leaf_field_count"] and
            current_report["exact_field_count"] ==
                stored["difference_report"]["exact_field_count"] and
            current_report["numeric_field_count"] ==
                stored["difference_report"]["numeric_field_count"],
            "current difference report coverage differs")
    return {
        "oracle_id": ORACLE_ID,
        "status": "passed",
        "qualification_profile": "executable",
        "target_profile": "target_pending",
        "components_resolved": header["asset_resolution"]["component_count"],
        "assets_resolved": header["asset_resolution"]["selected_asset_count"],
        "committed_samples":
            header["semantic_coverage"]["step"]["committed_samples"],
        "intervals_executed":
            header["semantic_coverage"]["step"]["intervals"],
        "terminal_tick":
            header["trajectory_terminal_summary"]["closing_tick"],
        "terminal_reason_code":
            header["trajectory_terminal_summary"]["terminal_reason_code"],
        "mission_python_cpp_checks":
            header["inherited_verification"]["mission_python_cpp_checks"],
        "leaf_fields_compared": current_report["leaf_field_count"],
        "exact_fields_compared": current_report["exact_field_count"],
        "numeric_fields_compared": current_report["numeric_field_count"],
        "failed_fields": current_report["failed_field_count"],
        "maximum_absolute_difference":
            current_report["maximum_absolute_difference"],
        "maximum_absolute_difference_path":
            current_report["maximum_absolute_difference_path"],
        "canonical_invalid_inputs_rejected":
            len(header["canonical_invalid_input_rejections"]),
        "inherited_invalid_inputs_rejected":
            header["inherited_verification"]
                ["mission_invalid_input_rejections"],
        "inherited_mutations_rejected":
            header["inherited_verification"]["mission_mutation_rejections"],
        "structured_diagnostics":
            header["inherited_verification"]
                ["mission_structured_diagnostics"],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", required=True, type=Path)
    parser.add_argument("--repo-root", required=True, type=Path)
    parser.add_argument("--oracle", required=True, type=Path)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--generate-reference", action="store_true")
    parser.add_argument("--difference-report", type=Path)
    arguments = parser.parse_args()

    getcontext().prec = 80
    repo_root = arguments.repo_root.resolve()
    require(repo_root.is_dir(), "repository root is missing")
    require(arguments.cases.resolve().is_file(), "canonical cases are missing")
    require(arguments.probe.resolve().is_file(), "C++ probe is missing")
    documents = load_documents(arguments.cases.resolve(), repo_root)
    (cases, source, assets, mission_cases, mission_oracle,
     source_path, asset_path, mission_cases_path,
     mission_oracle_path) = documents
    require(repo_relative(repo_root, arguments.cases.resolve()) ==
                "fixtures/ref-yyz-001/cases.json",
            "canonical cases path differs")
    mission_module = load_mission_module(repo_root)
    header, current_report = execute_current(
        cases, source, assets, mission_cases, mission_oracle,
        repo_root, mission_module, arguments.probe.resolve(),
        source_path, asset_path, mission_cases_path, mission_oracle_path)

    if arguments.difference_report is not None:
        write_json(arguments.difference_report, current_report)

    if arguments.generate_reference:
        oracle = dict(header)
        oracle["difference_report"] = current_report
        byte_count = write_json(arguments.oracle, oracle)
        print(json.dumps({
            "oracle_id": ORACLE_ID,
            "status": "generated",
            "path": arguments.oracle.as_posix(),
            "bytes": byte_count,
            "leaf_fields": current_report["leaf_field_count"],
        }, separators=(",", ":")))
        return 0

    stored = load_json(arguments.oracle)
    summary = verify_oracle(
        stored, header, current_report, mission_oracle,
        cases["difference_policy"])
    print(json.dumps(json_ready(summary), separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ArithmeticError, IndexError, KeyError, OSError, TypeError,
            ValueError, json.JSONDecodeError) as error:
        print(f"YYZ canonical bundle reference failed: {error}",
              file=sys.stderr)
        raise SystemExit(1)

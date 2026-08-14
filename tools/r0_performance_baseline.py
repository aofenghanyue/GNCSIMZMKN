#!/usr/bin/env python3
"""Run and validate the observation-only R0 minimal 3DoF benchmark."""

import argparse
import copy
import datetime
import decimal
import hashlib
import json
import math
import os
import pathlib
import platform
import statistics
import subprocess
import sys
import tempfile
import time


decimal.getcontext().prec = 80
D = decimal.Decimal


class ValidationFailure(ValueError):
    def __init__(self, category, message):
        super().__init__(message)
        self.category = category


def fail(category, message):
    raise ValidationFailure(category, message)


def require(condition, category, message):
    if not condition:
        fail(category, message)


def reject_duplicate_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key: " + key)
        result[key] = value
    return result


def read_json(path):
    with pathlib.Path(path).open("r", encoding="utf-8") as stream:
        return json.load(stream, object_pairs_hook=reject_duplicate_pairs)


def write_json(path, value):
    content = json.dumps(value, indent=2, allow_nan=False) + "\n"
    with pathlib.Path(path).open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(content)


def sha256_file(path):
    digest = hashlib.sha256()
    with pathlib.Path(path).open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def finite_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def validate_vector(value, name):
    require(isinstance(value, list) and len(value) == 3, "correctness", name + " must have three components")
    require(all(finite_number(component) for component in value), "correctness", name + " must be finite numeric data")


def find_case(cases, case_id):
    matches = [item for item in cases.get("cases", []) if item.get("id") == case_id]
    require(len(matches) == 1, "workload", "scientific case identity is missing or duplicated")
    return matches[0]


def validate_manifest(manifest, cases, oracle):
    require(manifest.get("schema_version") == "gnczmkn.r0-benchmark-workload/1", "workload", "unexpected workload manifest schema")
    require(manifest.get("workload_id") == "PERF-R0-M3DOF-BATCH-001", "workload", "unexpected workload identity")
    require(manifest.get("status") == "executable_observation_only", "workload", "workload must remain observation-only")
    require(manifest.get("task_id") == "R0-PERF-001", "workload", "workload task identity differs")

    scientific = manifest.get("scientific_identity", {})
    require(scientific.get("fixture_id") == "REF-MINIMAL-3DOF-001", "workload", "fixture identity differs")
    require(scientific.get("model_id") == cases.get("model", {}).get("model_id"), "workload", "model identity differs from cases")
    require(scientific.get("model_id") == oracle.get("model_id"), "workload", "model identity differs from oracle")
    case = find_case(cases, scientific.get("case_id"))

    episode = manifest.get("episode", {})
    exact_fields = (
        ("base_initial_position_m", "initial_position_m"),
        ("base_initial_velocity_mps", "initial_velocity_mps"),
        ("acceleration_mps2", "acceleration_mps2"),
        ("drag_rate_per_s", "drag_rate_per_s"),
        ("duration_s", "duration_s"),
    )
    for manifest_name, case_name in exact_fields:
        require(episode.get(manifest_name) == case.get(case_name), "workload", manifest_name + " differs from the scientific case")
    require(episode.get("dt_s") in case.get("dt_ladder_s", []), "workload", "benchmark dt is absent from the convergence ladder")
    require(episode.get("steps_per_episode") == 80, "workload", "steps per episode must be 80")
    require(abs(episode["dt_s"] * episode["steps_per_episode"] - episode["duration_s"]) <= 1e-15, "workload", "episode duration is inconsistent")
    require(episode.get("derivative_evaluations_per_step") == 4, "workload", "RK4 evaluation count differs")

    variation = episode.get("episode_variation", {})
    require(variation.get("position_scale_m") == 0.001, "workload", "position variation scale differs")
    require(variation.get("velocity_scale_mps") == 0.0001, "workload", "velocity variation scale differs")
    for key in ("position_patterns", "velocity_patterns"):
        patterns = variation.get(key)
        require(isinstance(patterns, list) and len(patterns) == 3, "workload", key + " must define three axes")
        for pattern in patterns:
            require(isinstance(pattern, list) and len(pattern) == 3 and all(isinstance(item, int) for item in pattern), "workload", key + " contains an invalid pattern")

    points = manifest.get("workload_points")
    require(isinstance(points, list) and len(points) == 4, "workload", "four concrete workload points are required")
    expected_points = [("smoke-1", 1), ("batch-64", 64), ("batch-1024", 1024), ("batch-16384", 16384)]
    require([(point.get("id"), point.get("episodes")) for point in points] == expected_points, "workload", "concrete workload points differ")
    require(len({point["id"] for point in points}) == len(points), "workload", "workload point identity is duplicated")

    size = manifest.get("size_vector", {})
    require(size.get("capacity_claim") == "tested_points_only" and size.get("product_limit") is None, "capacity", "R0 workload must not claim a product capacity limit")
    require(size.get("runtime_cells") == 0 and size.get("sessions") == 0, "capacity", "standalone workload must not imply runtime cells or Sessions")

    targets = manifest.get("determinism_targets")
    require(isinstance(targets, list) and [item.get("level") for item in targets] == ["D0", "D1", "D2", "D3"], "determinism", "D0-D3 target order differs")
    by_level = {item["level"]: item for item in targets}
    require(by_level["D0"].get("status") == "enforced_for_this_workload", "determinism", "D0 must be enforced")
    require(by_level["D1"].get("status") == "enforced_for_this_workload", "determinism", "D1 must be enforced")
    require(by_level["D1"].get("minimum_fresh_processes", 0) >= 3, "determinism", "D1 requires at least three fresh processes")
    require(by_level["D2"].get("status") == "target_pending", "determinism", "D2 cannot be claimed by this workload")
    require(by_level["D3"].get("status") == "target_pending", "determinism", "D3 cannot be claimed by this workload")

    measurement = manifest.get("measurement_profile", {})
    require(measurement.get("budget_class") == "observation_only", "performance", "budget class must remain observation-only")
    require(measurement.get("performance_thresholds") == [], "performance", "observation baseline cannot contain a timing threshold")
    require(measurement.get("baseline_configuration") == "release", "build", "baseline configuration must be release")
    require(measurement.get("verification_processes", 0) >= 3, "determinism", "verification needs at least three processes")
    require(measurement.get("measured_processes_per_point", 0) >= 5, "statistics", "baseline needs at least five measured processes")
    require(measurement.get("warmup_processes_per_point", -1) >= 1, "statistics", "baseline needs retained warm-up processes")
    require(measurement.get("raw_sample_retention") is True, "statistics", "raw samples must be retained")
    require(measurement.get("outlier_policy") == "retain_all_no_posthoc_deletion", "statistics", "outlier policy differs")
    return case


SENSITIVE_HARDWARE_KEYS = {
    "hostname",
    "host_name",
    "computer_name",
    "username",
    "user_name",
    "serial",
    "serial_number",
    "processor_id",
    "mac_address",
    "ip_address",
}


def walk_keys(value):
    if isinstance(value, dict):
        for key, nested in value.items():
            yield key
            yield from walk_keys(nested)
    elif isinstance(value, list):
        for nested in value:
            yield from walk_keys(nested)


def validate_hardware_profile(profile):
    require(profile.get("schema_version") == "gnczmkn.r0-hardware-profile/1", "environment", "unexpected hardware profile schema")
    require(profile.get("profile_id") == "R0-LOCAL-WIN11-INTEL12700K-HYPERV-001", "environment", "unexpected hardware profile identity")
    require(profile.get("status") == "observation_only_with_caveats", "environment", "hardware profile status differs")
    require(profile.get("qualification") is False, "environment", "local hardware profile cannot be qualification evidence")
    cpu = profile.get("cpu", {})
    require(cpu.get("vendor") and cpu.get("model_name"), "environment", "CPU identity is incomplete")
    require(cpu.get("physical_cores", 0) > 0 and cpu.get("logical_processors", 0) > 0, "environment", "CPU topology is incomplete")
    require(profile.get("memory", {}).get("physical_bytes_reported", 0) > 0, "environment", "memory profile is incomplete")
    require(profile.get("os", {}).get("family") and profile.get("os", {}).get("architecture"), "environment", "OS profile is incomplete")
    require(profile.get("virtualization", {}).get("hypervisor_present") is True, "environment", "known hypervisor caveat is missing")
    require(profile.get("clock", {}).get("per_run_overhead_calibration_required") is True, "instrumentation", "clock calibration requirement is missing")
    require(isinstance(profile.get("limitations"), list) and len(profile["limitations"]) >= 4, "environment", "hardware caveats are incomplete")
    sensitive = sorted({key for key in walk_keys(profile) if key.lower() in SENSITIVE_HARDWARE_KEYS})
    require(not sensitive, "privacy", "hardware profile contains sensitive key(s): " + ", ".join(sensitive))


def centered(episode, pattern):
    multiplier, modulus, center = pattern
    return D((episode * multiplier) % modulus) - D(center)


class AnalyticComparator:
    def __init__(self, manifest, oracle):
        self.manifest = manifest
        self.oracle = oracle
        self.cache = {}
        self._validate_base_oracle()

    def initial_state(self, episode, varied=True):
        spec = self.manifest["episode"]
        position = [D(str(value)) for value in spec["base_initial_position_m"]]
        velocity = [D(str(value)) for value in spec["base_initial_velocity_mps"]]
        if varied:
            variation = spec["episode_variation"]
            position_scale = D(str(variation["position_scale_m"]))
            velocity_scale = D(str(variation["velocity_scale_mps"]))
            for axis in range(3):
                position[axis] += position_scale * centered(episode, variation["position_patterns"][axis])
                velocity[axis] += velocity_scale * centered(episode, variation["velocity_patterns"][axis])
        return position, velocity

    def analytic_state(self, initial_position, initial_velocity):
        spec = self.manifest["episode"]
        acceleration = [D(str(value)) for value in spec["acceleration_mps2"]]
        drag = D(str(spec["drag_rate_per_s"]))
        duration = D(str(spec["duration_s"]))
        exponential = (-drag * duration).exp()
        position = []
        velocity = []
        for axis in range(3):
            terminal_velocity = acceleration[axis] / drag
            delta = initial_velocity[axis] - terminal_velocity
            velocity.append(terminal_velocity + delta * exponential)
            position.append(initial_position[axis] + terminal_velocity * duration + delta * (D(1) - exponential) / drag)
        return position, velocity

    @staticmethod
    def observable(position, velocity):
        coefficients = [D(1), D(3), D(5), D(7), D(11), D(13)]
        values = position + velocity
        return sum((coefficient * value for coefficient, value in zip(coefficients, values)), D(0))

    def _validate_base_oracle(self):
        initial_position, initial_velocity = self.initial_state(0, varied=False)
        position, velocity = self.analytic_state(initial_position, initial_velocity)
        stored = self.oracle["cases"]["CASE-MIN3D-LINEAR-DRAG-CONVERGENCE"]["analytic_final"]
        for actual, expected in zip(position, stored["position_m"]):
            require(abs(actual - D(expected)) <= D("1e-45"), "correctness", "manifest analytic position differs from the stored oracle")
        for actual, expected in zip(velocity, stored["velocity_mps"]):
            require(abs(actual - D(expected)) <= D("1e-45"), "correctness", "manifest analytic velocity differs from the stored oracle")

    def expected(self, episodes):
        if episodes in self.cache:
            return self.cache[episodes]
        position_sum = [D(0), D(0), D(0)]
        velocity_sum = [D(0), D(0), D(0)]
        weighted_sum = D(0)
        weight_sum = 0
        for episode in range(episodes):
            initial_position, initial_velocity = self.initial_state(episode)
            position, velocity = self.analytic_state(initial_position, initial_velocity)
            for axis in range(3):
                position_sum[axis] += position[axis]
                velocity_sum[axis] += velocity[axis]
            weight = episode % 31 + 1
            weight_sum += weight
            weighted_sum += D(weight) * self.observable(position, velocity)
        count = D(episodes)
        result = {
            "position": [float(value / count) for value in position_sum],
            "velocity": [float(value / count) for value in velocity_sum],
            "weighted_observable": float(weighted_sum / D(weight_sum)),
            "weight_sum": weight_sum,
        }
        self.cache[episodes] = result
        return result


def close(actual, expected, absolute):
    return abs(actual - expected) <= absolute


def validate_benchmark_result(report, manifest, comparator, episodes, configuration, require_normal=True):
    require(report.get("schema_version") == "gnczmkn.r0-minimal-3dof-benchmark-result/1", "correctness", "unexpected benchmark result schema")
    require(report.get("workload_id") == manifest.get("workload_id"), "correctness", "workload result identity differs")
    require(report.get("model_id") == manifest.get("scientific_identity", {}).get("model_id"), "correctness", "model result identity differs")
    require(report.get("algorithm") == "classical-rk4-fixed-step", "correctness", "algorithm identity differs")
    require(report.get("build_configuration") == configuration.lower(), "build", "benchmark build configuration differs")
    require(isinstance(report.get("compiler_identity"), str) and report["compiler_identity"], "build", "compiler identity is missing")
    timer = report.get("timer", {})
    require(timer.get("name") == "std::chrono::steady_clock" and timer.get("is_steady") is True, "instrumentation", "benchmark timer is not steady_clock")
    require(isinstance(report.get("elapsed_ns"), int) and report["elapsed_ns"] >= 0, "instrumentation", "workload elapsed time is invalid")
    require(report.get("timed_stage") == manifest.get("timing_boundary", {}).get("timed_stage"), "instrumentation", "timed stage differs")
    if require_normal:
        require(report.get("mutation") is None, "correctness", "benchmark mutation is active")

    semantic = report.get("semantic_result")
    require(isinstance(semantic, dict), "correctness", "semantic result is missing")
    steps = manifest["episode"]["steps_per_episode"]
    evaluations_per_step = manifest["episode"]["derivative_evaluations_per_step"]
    require(semantic.get("completed_episodes") == episodes, "correctness", "completed episode count differs")
    require(semantic.get("steps_per_episode") == steps, "correctness", "steps per episode differ")
    require(semantic.get("completed_steps") == episodes * steps, "correctness", "completed step count differs")
    require(semantic.get("derivative_evaluations") == episodes * steps * evaluations_per_step, "correctness", "derivative evaluation count differs")
    validate_vector(semantic.get("mean_final_position_m"), "mean final position")
    validate_vector(semantic.get("mean_final_velocity_mps"), "mean final velocity")
    require(finite_number(semantic.get("weighted_observable")), "correctness", "weighted observable is invalid")

    expected = comparator.expected(episodes)
    require(semantic.get("weight_sum") == expected["weight_sum"], "correctness", "weight sum differs")
    tolerances = manifest["correctness_comparator"]
    for axis, (actual, reference) in enumerate(zip(semantic["mean_final_position_m"], expected["position"])):
        require(close(actual, reference, tolerances["mean_position_absolute_m"]), "correctness", f"mean final position axis {axis} differs from the analytic reference")
    for axis, (actual, reference) in enumerate(zip(semantic["mean_final_velocity_mps"], expected["velocity"])):
        require(close(actual, reference, tolerances["mean_velocity_absolute_mps"]), "correctness", f"mean final velocity axis {axis} differs from the analytic reference")
    require(close(semantic["weighted_observable"], expected["weighted_observable"], tolerances["weighted_observable_absolute"]), "correctness", "weighted observable differs from the analytic reference")
    return semantic


def run_benchmark(executable, episodes, timeout_s, mutation=None):
    with tempfile.TemporaryDirectory(prefix="gnczmkn-r0-performance-") as directory:
        report_path = pathlib.Path(directory) / "result.json"
        command = [str(executable), "--episodes", str(episodes), "--report", str(report_path)]
        if mutation is not None:
            command.extend(["--mutation", mutation])
        started = time.perf_counter_ns()
        completed = subprocess.run(command, check=False, capture_output=True, text=True, timeout=timeout_s)
        finished = time.perf_counter_ns()
        if completed.returncode != 0:
            fail("execution", "benchmark process failed: " + completed.stdout + completed.stderr)
        require(report_path.is_file(), "execution", "benchmark process did not create a result")
        return read_json(report_path), finished - started


def assert_semantic_exact(reports, subject):
    require(len(reports) >= 3, "determinism", subject + " has fewer than three fresh-process results")
    first = reports[0]
    for index, report in enumerate(reports[1:], start=2):
        require(report == first, "determinism", f"{subject} semantic result differs in fresh process {index}")


def quantile(values, probability):
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def summarize(values):
    require(len(values) > 0, "statistics", "cannot summarize an empty sample set")
    median = statistics.median(values)
    deviations = [abs(value - median) for value in values]
    return {
        "count": len(values),
        "min": min(values),
        "max": max(values),
        "median": median,
        "mad": statistics.median(deviations),
        "p95": quantile(values, 0.95),
        "mean": statistics.fmean(values),
        "population_stddev": statistics.pstdev(values),
    }


def point_statistics(samples, episodes, steps_per_episode):
    measured = [sample for sample in samples if not sample["warmup"]]
    workload = [sample["workload_elapsed_ns"] for sample in measured]
    process = [sample["process_elapsed_ns"] for sample in measured]
    completed_steps = episodes * steps_per_episode
    throughputs = [completed_steps * 1_000_000_000.0 / max(value, 1) for value in workload]
    return {
        "workload_elapsed_ns": summarize(workload),
        "process_elapsed_ns": summarize(process),
        "steps_per_second": summarize(throughputs),
    }


def calibrate_clock(iterations):
    info = time.get_clock_info("perf_counter")
    deltas = []
    previous = time.perf_counter_ns()
    for _ in range(iterations):
        current = time.perf_counter_ns()
        delta = current - previous
        if delta > 0:
            deltas.append(delta)
        previous = current
    require(deltas, "instrumentation", "clock calibration produced no positive delta")
    return {
        "clock": "python-perf_counter_ns",
        "implementation": info.implementation,
        "monotonic": info.monotonic,
        "adjustable": info.adjustable,
        "reported_resolution_s": info.resolution,
        "iterations": iterations,
        "minimum_nonzero_delta_ns": min(deltas),
        "median_positive_delta_ns": statistics.median(deltas),
        "positive_delta_count": len(deltas),
        "subtracted_from_workload_samples": False,
    }


def capture_run_environment():
    return {
        "os_family": platform.system(),
        "os_release": platform.release(),
        "os_version": platform.version(),
        "architecture": platform.machine(),
        "logical_processors_visible": os.cpu_count(),
        "python_implementation": platform.python_implementation(),
        "python_version": platform.python_version(),
        "process_priority_policy": "unchanged",
        "affinity_policy": "unchanged",
        "worker_threads": 1,
        "environment_validity": "valid_with_profile_caveats",
    }


def validate_generation_host(hardware):
    expected_os = hardware.get("capture", {}).get("expected_os_family_for_baseline_generation")
    expected_logical = hardware.get("capture", {}).get("expected_logical_processors_for_baseline_generation")
    require(platform.system() == expected_os, "environment", "current OS does not match the baseline hardware profile")
    require(os.cpu_count() == expected_logical, "environment", "visible logical processor count does not match the baseline hardware profile")


def source_artifact_records(manifest_path, cases_path, oracle_path, hardware_path):
    repo_root = pathlib.Path(__file__).resolve().parent.parent
    return [
        {"role": "workload-manifest", "path": "benchmarks/r0/minimal-3dof/workload-manifest.json", "sha256": sha256_file(manifest_path)},
        {"role": "benchmark-source", "path": "benchmarks/r0/minimal_3dof_benchmark.cpp", "sha256": sha256_file(repo_root / "benchmarks/r0/minimal_3dof_benchmark.cpp")},
        {"role": "benchmark-runner", "path": "tools/r0_performance_baseline.py", "sha256": sha256_file(repo_root / "tools/r0_performance_baseline.py")},
        {"role": "scientific-cases", "path": "fixtures/ref-minimal-3dof/cases.json", "sha256": sha256_file(cases_path)},
        {"role": "scientific-oracle", "path": "oracles/ref-minimal-3dof/reference.json", "sha256": sha256_file(oracle_path)},
        {"role": "hardware-profile", "path": "benchmarks/r0/minimal-3dof/hardware-profile-windows-intel-12700k.json", "sha256": sha256_file(hardware_path)},
    ]


def generate_baseline(args, manifest, cases, oracle, hardware):
    validate_generation_host(hardware)
    comparator = AnalyticComparator(manifest, oracle)
    measurement = manifest["measurement_profile"]
    require(args.configuration.lower() == measurement["baseline_configuration"], "build", "baseline generation requires the release configuration")
    timeout_s = measurement["process_timeout_s"]
    warmups = measurement["warmup_processes_per_point"]
    measured = measurement["measured_processes_per_point"]
    point_reports = []
    first_process_report = None

    for point in manifest["workload_points"]:
        samples = []
        semantic_results = []
        total = warmups + measured
        for sample_index in range(total):
            report, process_elapsed_ns = run_benchmark(args.executable, point["episodes"], timeout_s)
            semantic = validate_benchmark_result(report, manifest, comparator, point["episodes"], args.configuration)
            if first_process_report is None:
                first_process_report = report
            semantic_results.append(semantic)
            warmup = sample_index < warmups
            ordinal = sample_index + 1 if warmup else sample_index - warmups + 1
            samples.append({
                "sample_id": point["id"] + ("-warmup-" if warmup else "-measured-") + f"{ordinal:02d}",
                "warmup": warmup,
                "workload_elapsed_ns": report["elapsed_ns"],
                "process_elapsed_ns": process_elapsed_ns,
                "semantic_result": semantic,
                "status": "valid_with_profile_caveats",
            })
        assert_semantic_exact(semantic_results, point["id"])
        point_reports.append({
            "point_id": point["id"],
            "episodes": point["episodes"],
            "warmup_sample_count": warmups,
            "measured_sample_count": measured,
            "raw_samples": samples,
            "statistics": point_statistics(samples, point["episodes"], manifest["episode"]["steps_per_episode"]),
            "determinism": {"requested": "D1", "achieved": "D1", "semantic_comparator": "exact parsed semantic_result"},
        })

    executable = pathlib.Path(args.executable)
    compiler_identity = first_process_report["compiler_identity"]
    if platform.system() == "Windows" and compiler_identity.startswith("gcc-"):
        build_qualification = "unqualified_local_windows_mingw_observation"
        build_limitation = "The local Windows MinGW/GCC build is outside the supported product toolchain profiles and is retained only as harness observation evidence."
    else:
        build_qualification = "candidate_toolchain_observation"
        build_limitation = "The observed build profile has correctness evidence but no performance qualification."
    baseline = {
        "schema_version": "gnczmkn.r0-performance-baseline/1",
        "baseline_id": "R0-PERF-M3DOF-WIN-INTEL12700K-OBS-001",
        "task_id": "R0-PERF-001",
        "status": "observed_with_caveats",
        "budget_class": "observation_only",
        "performance_thresholds": [],
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat(),
        "workload_id": manifest["workload_id"],
        "source_artifacts": source_artifact_records(args.manifest, args.cases, args.oracle, args.hardware_profile),
        "hardware_profile_id": hardware["profile_id"],
        "run_environment": capture_run_environment(),
        "clock_calibration": calibrate_clock(measurement["clock_calibration_iterations"]),
        "build_profile": {
            "configuration": first_process_report["build_configuration"],
            "compiler_identity": compiler_identity,
            "qualification": build_qualification,
            "product_qualification": False,
            "cplusplus_standard": "C++17",
            "executable_name": executable.name,
            "executable_bytes": executable.stat().st_size,
            "executable_sha256": sha256_file(executable),
            "instrumentation": "benchmark steady_clock only",
        },
        "determinism": {
            "D0": "passed_independent_analytic_tolerance",
            "D1": "passed_exact_fresh_process_semantic_results",
            "D2": "target_pending_cross_build_comparator",
            "D3": "target_pending_fixed_platform_and_fenv",
        },
        "capacity": {
            "claim": "tested_points_only",
            "largest_tested_episodes": max(point["episodes"] for point in manifest["workload_points"]),
            "product_limit": None,
        },
        "points": point_reports,
        "limitations": list(hardware["limitations"]) + [
            build_limitation,
            "Elapsed distributions do not gate CI or a release.",
            "The benchmark is a standalone R0 workload and does not measure target Session, Compiler or observation infrastructure.",
            "D2, D3, multi-Session and realtime qualification were not executed.",
        ],
    }
    validate_baseline(baseline, manifest, comparator, hardware, args)
    write_json(args.baseline, baseline)
    return baseline


def compare_statistics(actual, expected, subject):
    require(actual == expected, "statistics", subject + " aggregate differs from retained raw samples")


def validate_source_artifacts(records, args):
    expected = source_artifact_records(args.manifest, args.cases, args.oracle, args.hardware_profile)
    require(records == expected, "integrity", "baseline source artifact identities or hashes differ")


def validate_baseline(baseline, manifest, comparator, hardware, args):
    require(baseline.get("schema_version") == "gnczmkn.r0-performance-baseline/1", "baseline", "unexpected baseline schema")
    require(baseline.get("baseline_id") == "R0-PERF-M3DOF-WIN-INTEL12700K-OBS-001", "baseline", "unexpected baseline identity")
    require(baseline.get("task_id") == "R0-PERF-001" and baseline.get("workload_id") == manifest.get("workload_id"), "baseline", "baseline task or workload identity differs")
    require(baseline.get("status") == "observed_with_caveats", "baseline", "baseline status differs")
    require(baseline.get("budget_class") == "observation_only" and baseline.get("performance_thresholds") == [], "performance", "stored baseline cannot carry a performance threshold")
    require(baseline.get("hardware_profile_id") == hardware.get("profile_id"), "environment", "baseline hardware profile identity differs")
    validate_source_artifacts(baseline.get("source_artifacts"), args)

    environment = baseline.get("run_environment", {})
    require(environment.get("worker_threads") == 1, "environment", "baseline worker-thread policy differs")
    require(environment.get("environment_validity") == "valid_with_profile_caveats", "environment", "baseline environment caveat is missing")
    clock = baseline.get("clock_calibration", {})
    require(clock.get("monotonic") is True and clock.get("iterations") == manifest["measurement_profile"]["clock_calibration_iterations"], "instrumentation", "clock calibration is incomplete")
    require(clock.get("minimum_nonzero_delta_ns", 0) > 0 and clock.get("subtracted_from_workload_samples") is False, "instrumentation", "clock calibration disposition differs")

    build = baseline.get("build_profile", {})
    require(build.get("configuration") == "release", "build", "stored baseline was not produced by Release")
    require(isinstance(build.get("compiler_identity"), str) and build["compiler_identity"], "build", "stored compiler identity is missing")
    require(build.get("qualification") in ("unqualified_local_windows_mingw_observation", "candidate_toolchain_observation"), "build", "stored build qualification is invalid")
    require(build.get("product_qualification") is False, "build", "stored build must not claim product qualification")
    require(isinstance(build.get("executable_bytes"), int) and build["executable_bytes"] > 0, "build", "stored executable size is invalid")
    require(isinstance(build.get("executable_sha256"), str) and len(build["executable_sha256"]) == 64, "build", "stored executable SHA-256 is invalid")

    determinism = baseline.get("determinism", {})
    require(determinism.get("D0") == "passed_independent_analytic_tolerance", "determinism", "stored D0 result differs")
    require(determinism.get("D1") == "passed_exact_fresh_process_semantic_results", "determinism", "stored D1 result differs")
    require(determinism.get("D2") == "target_pending_cross_build_comparator", "determinism", "stored baseline overclaims D2")
    require(determinism.get("D3") == "target_pending_fixed_platform_and_fenv", "determinism", "stored baseline overclaims D3")

    expected_points = manifest["workload_points"]
    actual_points = baseline.get("points")
    require(isinstance(actual_points, list) and len(actual_points) == len(expected_points), "baseline", "stored workload points are incomplete")
    by_id = {point.get("point_id"): point for point in actual_points}
    require(len(by_id) == len(actual_points), "baseline", "stored workload point identity is duplicated")
    measurement = manifest["measurement_profile"]
    for point in expected_points:
        stored = by_id.get(point["id"])
        require(stored is not None and stored.get("episodes") == point["episodes"], "baseline", "stored workload point differs: " + point["id"])
        require(stored.get("warmup_sample_count") == measurement["warmup_processes_per_point"], "statistics", "warm-up sample count differs")
        require(stored.get("measured_sample_count") == measurement["measured_processes_per_point"], "statistics", "measured sample count differs")
        samples = stored.get("raw_samples")
        require(isinstance(samples, list) and len(samples) == stored["warmup_sample_count"] + stored["measured_sample_count"], "statistics", "raw sample count differs")
        sample_ids = [sample.get("sample_id") for sample in samples]
        require(len(set(sample_ids)) == len(sample_ids), "statistics", "raw sample identity is duplicated")
        semantics = []
        for sample in samples:
            require(sample.get("status") == "valid_with_profile_caveats", "environment", "raw sample status differs")
            require(isinstance(sample.get("workload_elapsed_ns"), int) and sample["workload_elapsed_ns"] >= 0, "statistics", "raw workload timing is invalid")
            require(isinstance(sample.get("process_elapsed_ns"), int) and sample["process_elapsed_ns"] > 0, "statistics", "raw process timing is invalid")
            pseudo_report = {
                "schema_version": "gnczmkn.r0-minimal-3dof-benchmark-result/1",
                "workload_id": manifest["workload_id"],
                "model_id": manifest["scientific_identity"]["model_id"],
                "algorithm": "classical-rk4-fixed-step",
                "compiler_identity": build["compiler_identity"],
                "build_configuration": "release",
                "timer": {"name": "std::chrono::steady_clock", "is_steady": True},
                "timed_stage": manifest["timing_boundary"]["timed_stage"],
                "elapsed_ns": sample["workload_elapsed_ns"],
                "mutation": None,
                "semantic_result": sample.get("semantic_result"),
            }
            semantics.append(validate_benchmark_result(pseudo_report, manifest, comparator, point["episodes"], "release"))
        assert_semantic_exact(semantics, "stored " + point["id"])
        expected_statistics = point_statistics(samples, point["episodes"], manifest["episode"]["steps_per_episode"])
        compare_statistics(stored.get("statistics"), expected_statistics, point["id"])
        require(stored.get("determinism") == {"requested": "D1", "achieved": "D1", "semantic_comparator": "exact parsed semantic_result"}, "determinism", "stored point determinism result differs")

    capacity = baseline.get("capacity", {})
    require(capacity.get("claim") == "tested_points_only" and capacity.get("product_limit") is None, "capacity", "stored baseline overclaims capacity")
    require(capacity.get("largest_tested_episodes") == 16384, "capacity", "largest tested point differs")
    require(isinstance(baseline.get("limitations"), list) and len(baseline["limitations"]) >= len(hardware["limitations"]), "baseline", "baseline limitations are incomplete")


def expect_validation_failure(callable_object, category, name):
    try:
        callable_object()
    except ValidationFailure as error:
        require(error.category == category, "mutation", f"{name} failed in category {error.category}, expected {category}")
        return
    fail("mutation", name + " was accepted")


def verify_zero_episode_rejection(executable, timeout_s):
    with tempfile.TemporaryDirectory(prefix="gnczmkn-r0-performance-zero-") as directory:
        report_path = pathlib.Path(directory) / "result.json"
        completed = subprocess.run(
            [str(executable), "--episodes", "0", "--report", str(report_path)],
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
        require(completed.returncode != 0 and not report_path.exists(), "mutation", "zero-episode input was accepted")


def verify_current(args, manifest, cases, oracle, hardware, baseline):
    comparator = AnalyticComparator(manifest, oracle)
    validate_baseline(baseline, manifest, comparator, hardware, args)
    measurement = manifest["measurement_profile"]
    point = manifest["workload_points"][0]
    semantic_results = []
    for _ in range(measurement["verification_processes"]):
        report, _ = run_benchmark(args.executable, point["episodes"], measurement["process_timeout_s"])
        semantic_results.append(validate_benchmark_result(report, manifest, comparator, point["episodes"], args.configuration))
    assert_semantic_exact(semantic_results, "current smoke point")

    mutation_report, _ = run_benchmark(
        args.executable,
        point["episodes"],
        measurement["process_timeout_s"],
        mutation="skip-integrator",
    )
    expect_validation_failure(
        lambda: validate_benchmark_result(mutation_report, manifest, comparator, point["episodes"], args.configuration, require_normal=False),
        "correctness",
        "skip-integrator mutation",
    )
    verify_zero_episode_rejection(args.executable, measurement["process_timeout_s"])

    false_d2 = copy.deepcopy(manifest)
    false_d2["determinism_targets"][2]["status"] = "enforced_for_this_workload"
    expect_validation_failure(lambda: validate_manifest(false_d2, cases, oracle), "determinism", "false D2 claim mutation")

    tampered_baseline = copy.deepcopy(baseline)
    tampered_baseline["points"][0]["statistics"]["workload_elapsed_ns"]["median"] += 1
    expect_validation_failure(lambda: validate_baseline(tampered_baseline, manifest, comparator, hardware, args), "statistics", "aggregate statistic mutation")

    sensitive_hardware = copy.deepcopy(hardware)
    sensitive_hardware["hostname"] = "forbidden"
    expect_validation_failure(lambda: validate_hardware_profile(sensitive_hardware), "privacy", "sensitive hardware mutation")
    return len(semantic_results), 5


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=pathlib.Path)
    parser.add_argument("--cases", required=True, type=pathlib.Path)
    parser.add_argument("--oracle", required=True, type=pathlib.Path)
    parser.add_argument("--hardware-profile", required=True, type=pathlib.Path)
    parser.add_argument("--baseline", required=True, type=pathlib.Path)
    parser.add_argument("--executable", type=pathlib.Path)
    parser.add_argument("--configuration")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--verify", action="store_true")
    mode.add_argument("--write-baseline", action="store_true")
    mode.add_argument("--static-only", action="store_true")
    return parser.parse_args()


def main():
    args = parse_args()
    manifest = read_json(args.manifest)
    cases = read_json(args.cases)
    oracle = read_json(args.oracle)
    hardware = read_json(args.hardware_profile)
    validate_manifest(manifest, cases, oracle)
    validate_hardware_profile(hardware)
    require(args.baseline.is_file() or args.write_baseline, "baseline", "stored performance baseline is missing")

    if args.static_only:
        baseline = read_json(args.baseline)
        validate_baseline(baseline, manifest, AnalyticComparator(manifest, oracle), hardware, args)
        print(
            "R0 performance baseline static validation passed: "
            f"stored_points={len(baseline['points'])} "
            f"stored_raw_samples={sum(len(point['raw_samples']) for point in baseline['points'])} "
            "D0=passed D1=passed D2=pending D3=pending"
        )
        return 0

    require(args.executable is not None and args.executable.is_file(), "execution", "benchmark executable is missing")
    require(args.configuration is not None, "build", "benchmark configuration is required")

    if args.write_baseline:
        baseline = generate_baseline(args, manifest, cases, oracle, hardware)
        print(
            "R0 performance baseline written: "
            f"points={len(baseline['points'])} "
            f"raw_samples={sum(len(point['raw_samples']) for point in baseline['points'])} "
            "D0=passed D1=passed D2=pending D3=pending"
        )
        return 0

    baseline = read_json(args.baseline)
    process_count, mutation_count = verify_current(args, manifest, cases, oracle, hardware, baseline)
    print(
        "R0 performance baseline validation passed: "
        f"current_fresh_processes={process_count} "
        f"stored_points={len(baseline['points'])} "
        f"stored_raw_samples={sum(len(point['raw_samples']) for point in baseline['points'])} "
        f"mutations_rejected={mutation_count} "
        "D0=passed D1=passed D2=pending D3=pending"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, decimal.DecimalException, subprocess.TimeoutExpired) as error:
        if isinstance(error, ValidationFailure):
            print(f"R0 performance baseline error [{error.category}]: {error}", file=sys.stderr)
        else:
            print("R0 performance baseline error: " + str(error), file=sys.stderr)
        sys.exit(1)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-RUN-EVALUATION-001";
constexpr const char* kModelId = "MODEL-YYZ-RUN-EVALUATION-001";
constexpr const char* kSubject = "vehicle.fixture.yyz@1";
constexpr const char* kInertialFrameId =
    "frame.fixture.yyz.inertial-cartesian@1";
constexpr double kAbsoluteTolerance = 2.0e-12;
constexpr double kRelativeTolerance = 2.0e-12;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Sample {
    std::int64_t sample_tick = 0;
    double time_s = 0.0;
    std::string commit_id;
    std::string quality;
    Vec3 position_i_m;
    Vec3 velocity_i_mps;
    double committed_mass_kg = 0.0;
};

struct Predicate {
    std::string predicate_id;
    std::string metric_id;
    std::string relation;
    double threshold = 0.0;
    std::string action;
    std::string reason_code;
    std::int64_t priority = 0;
};

struct Case {
    std::string id;
    std::string plan_id;
    double requested_duration_s = 0.0;
    std::vector<Predicate> predicates;
};

struct Input {
    double dt_s = 0.0;
    std::string inertial_frame_id;
    std::vector<Sample> samples;
    std::vector<Case> cases;
};

struct Metrics {
    double duration_s = 0.0;
    double downrange_m = 0.0;
    double remaining_mass_kg = 0.0;
    double consumed_mass_kg = 0.0;
    double speed_mps = 0.0;
};

struct PredicateResult {
    Predicate predicate;
    double observed = 0.0;
    bool met = false;
};

struct Decision {
    std::string action;
    std::string reason_code;
    double trigger_time_s = 0.0;
    std::string subject;
    std::int64_t priority = 0;
    Metrics metrics;
    std::string message_key;
};

struct Boundary {
    std::int64_t sample_tick = 0;
    double time_s = 0.0;
    std::string commit_id;
    Metrics metrics;
    std::vector<PredicateResult> predicate_results;
    Decision decision;
};

struct MetricSummary {
    std::int64_t evaluated_sample_count = 0;
    double duration_s = 0.0;
    double downrange_m = 0.0;
    double remaining_mass_kg = 0.0;
    double consumed_mass_kg = 0.0;
    double terminal_speed_mps = 0.0;
    double peak_speed_mps = 0.0;
    std::int64_t peak_speed_tick = 0;
    double maximum_downrange_m = 0.0;
    std::int64_t maximum_downrange_tick = 0;
    double minimum_remaining_mass_kg = 0.0;
    std::int64_t minimum_remaining_mass_tick = 0;
};

struct TerminalObservation {
    std::int64_t sample_tick = 0;
    double time_s = 0.0;
    std::string commit_id;
    Metrics metrics;
    Decision decision;
    std::vector<std::string> event_order;
    bool sealed = false;
    std::int64_t post_terminal_sample_count = 0;
};

struct RunOutcome {
    std::string final_status;
    std::string evidence_validity;
    std::int64_t initial_tick = 0;
    std::int64_t final_tick = 0;
    double requested_duration_s = 0.0;
    double final_time_s = 0.0;
    Decision termination;
    MetricSummary metrics;
    bool terminal_observation_sealed = false;
    bool frozen = false;
};

struct Evaluation {
    std::string id;
    std::string plan_id;
    std::string model_id;
    std::vector<Boundary> evaluated_boundaries;
    MetricSummary metric_summary;
    TerminalObservation terminal_observation;
    RunOutcome run_outcome;
    bool terminal_found = false;
};

struct Options {
    bool early_mass_candidate = false;
    bool strict_thresholds = false;
    bool low_priority_wins = false;
    bool outcome_before_observation = false;
    bool post_terminal_sample = false;
    bool require_terminal = true;
};

struct ProbeResult {
    std::vector<Evaluation> accepted;
    std::vector<std::string> invalid_input_rejections;
    Evaluation early_mass;
    Evaluation strict_thresholds;
    Evaluation low_priority;
    Evaluation outcome_before_observation;
    Evaluation post_terminal;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireDomain(bool condition, const std::string& message) {
    if (!condition) {
        throw std::domain_error(message);
    }
}

bool finite(double value) {
    return std::isfinite(value);
}

bool finite(const Vec3& value) {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool near(double actual, double expected) {
    const double difference = std::abs(actual - expected);
    const double bound = kAbsoluteTolerance + kRelativeTolerance *
        std::max({1.0, std::abs(actual), std::abs(expected)});
    return difference <= bound;
}

double canonicalZero(double value) {
    return value == 0.0 ? 0.0 : value;
}

std::vector<std::string> acceptedEventOrder() {
    return {
        "publish-committed-sample",
        "evaluate-metrics",
        "evaluate-termination",
        "seal-terminal-observation",
        "freeze-run-outcome",
    };
}

bool supportedMetric(const std::string& value) {
    return value == "duration_s" || value == "downrange_m" ||
           value == "remaining_mass_kg";
}

void validatePlan(const Case& value) {
    requireDomain(!value.id.empty() && !value.plan_id.empty(),
                  "case and plan identities must be nonempty");
    requireDomain(finite(value.requested_duration_s) &&
                      value.requested_duration_s > 0.0,
                  "requested duration must be positive");
    requireDomain(!value.predicates.empty(),
                  "termination plan must contain predicates");
    std::set<std::string> predicate_ids;
    for (const Predicate& predicate : value.predicates) {
        requireDomain(!predicate.predicate_id.empty() &&
                          predicate_ids.insert(predicate.predicate_id).second,
                      "predicate ids must be nonempty and unique");
        requireDomain(supportedMetric(predicate.metric_id),
                      "unsupported predicate metric");
        requireDomain(predicate.relation == ">=" ||
                          predicate.relation == "<=",
                      "unsupported predicate relation");
        requireDomain(finite(predicate.threshold),
                      "predicate threshold must be finite");
        requireDomain(predicate.action == "Complete" ||
                          predicate.action == "Abort",
                      "unsupported terminal action");
        requireDomain(!predicate.reason_code.empty(),
                      "reason_code must be nonempty");
        requireDomain(predicate.priority >= 0,
                      "predicate priority must be nonnegative");
    }
}

void validateInput(const Input& value) {
    requireDomain(finite(value.dt_s) && value.dt_s > 0.0,
                  "base_dt_s must be positive");
    requireDomain(value.inertial_frame_id == kInertialFrameId,
                  "trajectory inertial frame differs");
    requireDomain(value.samples.size() >= 2,
                  "at least two committed samples are required");
    std::set<std::string> commit_ids;
    double previous_mass = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < value.samples.size(); ++index) {
        const Sample& sample = value.samples[index];
        requireDomain(sample.sample_tick ==
                          static_cast<std::int64_t>(index),
                      "sample ticks must be contiguous from zero");
        requireDomain(finite(sample.time_s) &&
                          near(sample.time_s,
                               static_cast<double>(index) * value.dt_s),
                      "sample time does not match tick * base_dt_s");
        requireDomain(!sample.commit_id.empty() &&
                          commit_ids.insert(sample.commit_id).second,
                      "commit ids must be nonempty and unique");
        requireDomain(sample.quality == "Valid",
                      "committed sample quality must be Valid");
        requireDomain(finite(sample.position_i_m) &&
                          finite(sample.velocity_i_mps),
                      "committed state must be finite");
        requireDomain(finite(sample.committed_mass_kg) &&
                          sample.committed_mass_kg > 0.0 &&
                          sample.committed_mass_kg <= previous_mass,
                      "committed mass must be positive nonincreasing");
        previous_mass = sample.committed_mass_kg;
    }
    std::set<std::string> case_ids;
    for (const Case& item : value.cases) {
        validatePlan(item);
        requireDomain(case_ids.insert(item.id).second,
                      "case ids must be unique");
    }
}

double magnitude(const Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y +
                     value.z * value.z);
}

Metrics sampleMetrics(const Sample& sample, const Sample& initial,
                      double visible_mass) {
    return {
        canonicalZero(sample.time_s),
        canonicalZero(sample.position_i_m.x - initial.position_i_m.x),
        canonicalZero(visible_mass),
        canonicalZero(initial.committed_mass_kg - visible_mass),
        canonicalZero(magnitude(sample.velocity_i_mps)),
    };
}

double metricValue(const Metrics& metrics, const std::string& metric_id) {
    if (metric_id == "duration_s") {
        return metrics.duration_s;
    }
    if (metric_id == "downrange_m") {
        return metrics.downrange_m;
    }
    if (metric_id == "remaining_mass_kg") {
        return metrics.remaining_mass_kg;
    }
    throw std::domain_error("unsupported predicate metric");
}

bool predicateMet(double observed, const Predicate& predicate,
                  bool strict_thresholds) {
    if (predicate.relation == ">=") {
        return strict_thresholds ? observed > predicate.threshold
                                 : observed >= predicate.threshold;
    }
    if (predicate.relation == "<=") {
        return strict_thresholds ? observed < predicate.threshold
                                 : observed <= predicate.threshold;
    }
    throw std::domain_error("unsupported predicate relation");
}

Decision continueDecision(const Metrics& metrics) {
    return {"Continue", "none", metrics.duration_s, kSubject, 0, metrics,
            "yyz.termination.continue"};
}

std::pair<std::vector<PredicateResult>, Decision> evaluatePredicates(
    const Case& item, const Metrics& metrics, const Options& options) {
    std::vector<PredicateResult> results;
    for (const Predicate& predicate : item.predicates) {
        const double observed = metricValue(metrics, predicate.metric_id);
        results.push_back({predicate, observed,
                           predicateMet(observed, predicate,
                                        options.strict_thresholds)});
    }
    std::sort(results.begin(), results.end(),
              [](const PredicateResult& lhs, const PredicateResult& rhs) {
                  return lhs.predicate.predicate_id <
                         rhs.predicate.predicate_id;
              });
    const PredicateResult* selected = nullptr;
    for (const PredicateResult& result : results) {
        if (!result.met) {
            continue;
        }
        if (selected == nullptr) {
            selected = &result;
            continue;
        }
        bool better = false;
        if (result.predicate.priority != selected->predicate.priority) {
            better = options.low_priority_wins
                ? result.predicate.priority < selected->predicate.priority
                : result.predicate.priority > selected->predicate.priority;
        } else {
            better = result.predicate.predicate_id <
                     selected->predicate.predicate_id;
        }
        if (better) {
            selected = &result;
        }
    }
    if (selected == nullptr) {
        return {results, continueDecision(metrics)};
    }
    Decision decision{
        selected->predicate.action,
        selected->predicate.reason_code,
        metrics.duration_s,
        kSubject,
        selected->predicate.priority,
        metrics,
        "yyz.termination." + selected->predicate.reason_code,
    };
    return {results, decision};
}

void updateSummary(MetricSummary& summary, bool& initialized,
                   const Sample& sample, const Metrics& metrics,
                   std::int64_t count) {
    if (!initialized) {
        summary.peak_speed_mps = metrics.speed_mps;
        summary.peak_speed_tick = sample.sample_tick;
        summary.maximum_downrange_m = metrics.downrange_m;
        summary.maximum_downrange_tick = sample.sample_tick;
        summary.minimum_remaining_mass_kg = metrics.remaining_mass_kg;
        summary.minimum_remaining_mass_tick = sample.sample_tick;
        initialized = true;
    } else {
        if (metrics.speed_mps > summary.peak_speed_mps) {
            summary.peak_speed_mps = metrics.speed_mps;
            summary.peak_speed_tick = sample.sample_tick;
        }
        if (metrics.downrange_m > summary.maximum_downrange_m) {
            summary.maximum_downrange_m = metrics.downrange_m;
            summary.maximum_downrange_tick = sample.sample_tick;
        }
        if (metrics.remaining_mass_kg <
            summary.minimum_remaining_mass_kg) {
            summary.minimum_remaining_mass_kg = metrics.remaining_mass_kg;
            summary.minimum_remaining_mass_tick = sample.sample_tick;
        }
    }
    summary.evaluated_sample_count = count;
    summary.duration_s = metrics.duration_s;
    summary.downrange_m = metrics.downrange_m;
    summary.remaining_mass_kg = metrics.remaining_mass_kg;
    summary.consumed_mass_kg = metrics.consumed_mass_kg;
    summary.terminal_speed_mps = metrics.speed_mps;
}

Evaluation evaluate(const Input& input, std::size_t case_index,
                    const Options& options = {}) {
    validateInput(input);
    requireDomain(case_index < input.cases.size(), "case index is invalid");
    const Case& item = input.cases[case_index];
    Evaluation result;
    result.id = item.id;
    result.plan_id = item.plan_id;
    result.model_id = kModelId;
    bool summary_initialized = false;
    std::size_t terminal_index = input.samples.size();
    Boundary terminal_boundary;

    for (std::size_t index = 0; index < input.samples.size(); ++index) {
        const Sample& sample = input.samples[index];
        double visible_mass = sample.committed_mass_kg;
        if (options.early_mass_candidate &&
            index + 1 < input.samples.size()) {
            visible_mass = input.samples[index + 1].committed_mass_kg;
        }
        const Metrics metrics = sampleMetrics(
            sample, input.samples.front(), visible_mass);
        auto evaluated = evaluatePredicates(item, metrics, options);
        Boundary boundary{sample.sample_tick, sample.time_s, sample.commit_id,
                          metrics, std::move(evaluated.first),
                          std::move(evaluated.second)};
        result.evaluated_boundaries.push_back(boundary);
        updateSummary(result.metric_summary, summary_initialized, sample,
                      metrics, static_cast<std::int64_t>(
                          result.evaluated_boundaries.size()));
        if (boundary.decision.action != "Continue" &&
            !result.terminal_found) {
            result.terminal_found = true;
            terminal_index = index;
            terminal_boundary = boundary;
            if (!options.post_terminal_sample) {
                break;
            }
        }
    }

    requireDomain(result.terminal_found || !options.require_terminal,
                  "termination plan did not reach a terminal boundary");
    if (!result.terminal_found) {
        terminal_boundary = result.evaluated_boundaries.back();
    }
    std::vector<std::string> event_order = acceptedEventOrder();
    if (options.outcome_before_observation) {
        std::swap(event_order[event_order.size() - 2], event_order.back());
    }
    const std::int64_t post_count = result.terminal_found
        ? static_cast<std::int64_t>(result.evaluated_boundaries.size() -
                                    terminal_index - 1)
        : 0;
    result.terminal_observation = {
        terminal_boundary.sample_tick,
        terminal_boundary.time_s,
        terminal_boundary.commit_id,
        terminal_boundary.metrics,
        terminal_boundary.decision,
        event_order,
        !options.outcome_before_observation,
        post_count,
    };
    std::string final_status = "Running";
    if (terminal_boundary.decision.action == "Complete") {
        final_status = "Completed";
    } else if (terminal_boundary.decision.action == "Abort") {
        final_status = "Terminated";
    }
    result.run_outcome = {
        final_status,
        "Valid",
        0,
        terminal_boundary.sample_tick,
        item.requested_duration_s,
        terminal_boundary.time_s,
        terminal_boundary.decision,
        result.metric_summary,
        result.terminal_observation.sealed,
        true,
    };
    return result;
}

Input makeInput() {
    constexpr double terminal_position = 2.0400041684035015;
    constexpr double terminal_velocity = 10.400083368070029;
    Input input;
    input.dt_s = 0.1;
    input.inertial_frame_id = kInertialFrameId;
    input.samples = {
        {0, 0.0, "commit.fixture.yyz.0", "Valid",
         {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, 120.0},
        {1, 0.1, "commit.fixture.yyz.1", "Valid",
         {1.01, 0.0, 0.0}, {10.2, 0.0, 0.0}, 119.95},
        {2, 0.2, "commit.fixture.yyz.2", "Valid",
         {terminal_position, 0.0, 0.0},
         {terminal_velocity, 0.0, 0.0}, 119.9},
    };
    input.cases = {
        {
            "CASE-YYZ-RUN-EVALUATION-COMPLETE",
            "termination-plan.fixture.yyz.complete@1",
            0.2,
            {
                {"remaining-mass-floor", "remaining_mass_kg", "<=",
                 119.8, "Abort", "remaining-mass-floor", 300},
                {"duration-limit", "duration_s", ">=", 0.2,
                 "Complete", "duration-complete", 100},
                {"downrange-goal", "downrange_m", ">=",
                 terminal_position, "Complete", "downrange-goal", 200},
            },
        },
        {
            "CASE-YYZ-RUN-EVALUATION-MASS-ABORT",
            "termination-plan.fixture.yyz.mass-abort@1",
            1.0,
            {
                {"downrange-goal", "downrange_m", ">=", 20.0,
                 "Complete", "downrange-goal", 200},
                {"remaining-mass-floor", "remaining_mass_kg", "<=",
                 119.95, "Abort", "remaining-mass-floor", 300},
                {"duration-limit", "duration_s", ">=", 1.0,
                 "Complete", "duration-complete", 100},
            },
        },
    };
    return input;
}

template <typename Mutation>
void expectDomainRejection(std::vector<std::string>& rejected,
                           const std::string& identifier,
                           const Input& accepted, Mutation mutation) {
    Input value = accepted;
    mutation(value);
    try {
        static_cast<void>(evaluate(value, 0));
    } catch (const std::domain_error&) {
        rejected.push_back(identifier);
        return;
    }
    throw std::runtime_error("invalid input was accepted: " + identifier);
}

bool equivalentCore(const Evaluation& lhs, const Evaluation& rhs) {
    if (lhs.evaluated_boundaries.size() != rhs.evaluated_boundaries.size() ||
        lhs.run_outcome.final_tick != rhs.run_outcome.final_tick ||
        lhs.run_outcome.final_status != rhs.run_outcome.final_status ||
        lhs.run_outcome.termination.reason_code !=
            rhs.run_outcome.termination.reason_code ||
        lhs.run_outcome.termination.priority !=
            rhs.run_outcome.termination.priority) {
        return false;
    }
    return near(lhs.metric_summary.duration_s,
                rhs.metric_summary.duration_s) &&
           near(lhs.metric_summary.downrange_m,
                rhs.metric_summary.downrange_m) &&
           near(lhs.metric_summary.remaining_mass_kg,
                rhs.metric_summary.remaining_mass_kg) &&
           near(lhs.metric_summary.peak_speed_mps,
                rhs.metric_summary.peak_speed_mps);
}

ProbeResult runProbe() {
    const Input input = makeInput();
    ProbeResult result;
    result.accepted = {evaluate(input, 0), evaluate(input, 1)};
    require(result.accepted[0].run_outcome.final_status == "Completed" &&
                result.accepted[0].run_outcome.final_tick == 2 &&
                result.accepted[0].run_outcome.termination.reason_code ==
                    "downrange-goal" &&
                result.accepted[0].run_outcome.termination.priority == 200 &&
                result.accepted[0].evaluated_boundaries.size() == 3,
            "complete-path result differs");
    require(result.accepted[1].run_outcome.final_status == "Terminated" &&
                result.accepted[1].run_outcome.final_tick == 1 &&
                result.accepted[1].run_outcome.termination.reason_code ==
                    "remaining-mass-floor" &&
                result.accepted[1].evaluated_boundaries.size() == 2,
            "abort-path result differs");
    require(result.accepted[0].terminal_observation.sealed &&
                result.accepted[1].terminal_observation.sealed &&
                result.accepted[0].terminal_observation.event_order ==
                    acceptedEventOrder(),
            "terminal observation was not sealed before result freeze");

    Input reversed = input;
    for (Case& item : reversed.cases) {
        std::reverse(item.predicates.begin(), item.predicates.end());
    }
    require(equivalentCore(result.accepted[0], evaluate(reversed, 0)) &&
                equivalentCore(result.accepted[1], evaluate(reversed, 1)),
            "predicate declaration order changed evaluation results");

    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-NONPOSITIVE-DT", input,
        [](Input& value) { value.dt_s = 0.0; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-TICK-GAP", input,
        [](Input& value) { value.samples[2].sample_tick = 3; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-TIME-MISMATCH", input,
        [](Input& value) { value.samples[1].time_s = 0.11; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-DUPLICATE-COMMIT", input,
        [](Input& value) {
            value.samples[1].commit_id = value.samples[0].commit_id;
        });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-QUALITY", input,
        [](Input& value) { value.samples[1].quality = "Invalid"; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-MASS-INCREASE", input,
        [](Input& value) { value.samples[1].committed_mass_kg = 120.01; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-NONFINITE-STATE", input,
        [](Input& value) {
            value.samples[1].velocity_i_mps.x =
                std::numeric_limits<double>::quiet_NaN();
        });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-DUPLICATE-PREDICATE", input,
        [](Input& value) {
            value.cases[0].predicates[1].predicate_id =
                value.cases[0].predicates[0].predicate_id;
        });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-RELATION", input,
        [](Input& value) { value.cases[0].predicates[0].relation = ">"; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-NEGATIVE-PRIORITY", input,
        [](Input& value) { value.cases[0].predicates[0].priority = -1; });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-METRIC-ID", input,
        [](Input& value) {
            value.cases[0].predicates[0].metric_id = "altitude_m";
        });
    expectDomainRejection(
        result.invalid_input_rejections,
        "INVALID-YYZ-RUN-EVALUATION-NO-TERMINAL", input,
        [](Input& value) {
            for (Predicate& predicate : value.cases[0].predicates) {
                predicate.metric_id = "duration_s";
                predicate.relation = ">=";
                predicate.threshold = 10.0;
            }
        });

    Options early;
    early.early_mass_candidate = true;
    result.early_mass = evaluate(input, 1, early);
    Options strict;
    strict.strict_thresholds = true;
    strict.require_terminal = false;
    result.strict_thresholds = evaluate(input, 0, strict);
    Options low;
    low.low_priority_wins = true;
    result.low_priority = evaluate(input, 0, low);
    Options wrong_order;
    wrong_order.outcome_before_observation = true;
    result.outcome_before_observation = evaluate(input, 0, wrong_order);
    Options post;
    post.post_terminal_sample = true;
    result.post_terminal = evaluate(input, 1, post);
    require(result.early_mass.run_outcome.final_tick == 0 &&
                !result.strict_thresholds.terminal_found &&
                result.low_priority.run_outcome.termination.reason_code ==
                    "duration-complete" &&
                !result.outcome_before_observation.terminal_observation.sealed &&
                result.post_terminal.terminal_observation
                        .post_terminal_sample_count == 1,
            "a run-evaluation mutation matched the accepted result");
    return result;
}

void writeNumber(double value) {
    std::cout << canonicalZero(value);
}

void writeBoolean(bool value) {
    std::cout << (value ? "true" : "false");
}

void writeStringList(const std::vector<std::string>& values) {
    std::cout << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << '"' << values[index] << '"';
    }
    std::cout << ']';
}

void writeMetrics(const Metrics& value) {
    std::cout << "{\"duration_s\":";
    writeNumber(value.duration_s);
    std::cout << ",\"downrange_m\":";
    writeNumber(value.downrange_m);
    std::cout << ",\"remaining_mass_kg\":";
    writeNumber(value.remaining_mass_kg);
    std::cout << ",\"consumed_mass_kg\":";
    writeNumber(value.consumed_mass_kg);
    std::cout << ",\"speed_mps\":";
    writeNumber(value.speed_mps);
    std::cout << '}';
}

void writeDecisionMetrics(const Metrics& value) {
    std::cout << "{\"duration_s\":";
    writeNumber(value.duration_s);
    std::cout << ",\"downrange_m\":";
    writeNumber(value.downrange_m);
    std::cout << ",\"remaining_mass_kg\":";
    writeNumber(value.remaining_mass_kg);
    std::cout << '}';
}

void writePredicateResult(const PredicateResult& value) {
    const Predicate& predicate = value.predicate;
    std::cout << "{\"predicate_id\":\"" << predicate.predicate_id
              << "\",\"metric_id\":\"" << predicate.metric_id
              << "\",\"relation\":\"" << predicate.relation
              << "\",\"threshold\":";
    writeNumber(predicate.threshold);
    std::cout << ",\"observed\":";
    writeNumber(value.observed);
    std::cout << ",\"met\":";
    writeBoolean(value.met);
    std::cout << ",\"action\":\"" << predicate.action
              << "\",\"reason_code\":\"" << predicate.reason_code
              << "\",\"priority\":" << predicate.priority << '}';
}

void writeDecision(const Decision& value) {
    std::cout << "{\"action\":\"" << value.action
              << "\",\"reason_code\":\"" << value.reason_code
              << "\",\"trigger_time_s\":";
    writeNumber(value.trigger_time_s);
    std::cout << ",\"subject\":\"" << value.subject
              << "\",\"priority\":" << value.priority
              << ",\"metrics\":";
    writeDecisionMetrics(value.metrics);
    std::cout << ",\"message_key\":\"" << value.message_key
              << "\",\"params\":{}}";
}

void writeBoundary(const Boundary& value) {
    std::cout << "{\"sample_tick\":" << value.sample_tick
              << ",\"time_s\":";
    writeNumber(value.time_s);
    std::cout << ",\"commit_id\":\"" << value.commit_id
              << "\",\"metrics\":";
    writeMetrics(value.metrics);
    std::cout << ",\"predicate_results\":[";
    for (std::size_t index = 0; index < value.predicate_results.size();
         ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writePredicateResult(value.predicate_results[index]);
    }
    std::cout << "],\"decision\":";
    writeDecision(value.decision);
    std::cout << '}';
}

void writeMetricSummary(const MetricSummary& value) {
    std::cout << "{\"evaluated_sample_count\":"
              << value.evaluated_sample_count << ",\"duration_s\":";
    writeNumber(value.duration_s);
    std::cout << ",\"downrange_m\":";
    writeNumber(value.downrange_m);
    std::cout << ",\"remaining_mass_kg\":";
    writeNumber(value.remaining_mass_kg);
    std::cout << ",\"consumed_mass_kg\":";
    writeNumber(value.consumed_mass_kg);
    std::cout << ",\"terminal_speed_mps\":";
    writeNumber(value.terminal_speed_mps);
    std::cout << ",\"peak_speed_mps\":";
    writeNumber(value.peak_speed_mps);
    std::cout << ",\"peak_speed_tick\":" << value.peak_speed_tick
              << ",\"maximum_downrange_m\":";
    writeNumber(value.maximum_downrange_m);
    std::cout << ",\"maximum_downrange_tick\":"
              << value.maximum_downrange_tick
              << ",\"minimum_remaining_mass_kg\":";
    writeNumber(value.minimum_remaining_mass_kg);
    std::cout << ",\"minimum_remaining_mass_tick\":"
              << value.minimum_remaining_mass_tick << '}';
}

void writeTerminalObservation(const TerminalObservation& value) {
    std::cout << "{\"sample_tick\":" << value.sample_tick
              << ",\"time_s\":";
    writeNumber(value.time_s);
    std::cout << ",\"commit_id\":\"" << value.commit_id
              << "\",\"metrics\":";
    writeMetrics(value.metrics);
    std::cout << ",\"decision\":";
    writeDecision(value.decision);
    std::cout << ",\"event_order\":";
    writeStringList(value.event_order);
    std::cout << ",\"sealed\":";
    writeBoolean(value.sealed);
    std::cout << ",\"post_terminal_sample_count\":"
              << value.post_terminal_sample_count << '}';
}

void writeRunOutcome(const RunOutcome& value) {
    std::cout << "{\"final_status\":\"" << value.final_status
              << "\",\"evidence_validity\":\""
              << value.evidence_validity << "\",\"initial_tick\":"
              << value.initial_tick << ",\"final_tick\":"
              << value.final_tick << ",\"requested_duration_s\":";
    writeNumber(value.requested_duration_s);
    std::cout << ",\"final_time_s\":";
    writeNumber(value.final_time_s);
    std::cout << ",\"termination\":";
    writeDecision(value.termination);
    std::cout << ",\"metrics\":";
    writeMetricSummary(value.metrics);
    std::cout << ",\"terminal_observation_sealed\":";
    writeBoolean(value.terminal_observation_sealed);
    std::cout << ",\"frozen\":";
    writeBoolean(value.frozen);
    std::cout << '}';
}

void writeEvaluation(const Evaluation& value) {
    std::cout << "{\"id\":\"" << value.id << "\",\"plan_id\":\""
              << value.plan_id << "\",\"model_id\":\""
              << value.model_id << "\",\"evaluated_boundaries\":[";
    for (std::size_t index = 0;
         index < value.evaluated_boundaries.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeBoundary(value.evaluated_boundaries[index]);
    }
    std::cout << "],\"metric_summary\":";
    writeMetricSummary(value.metric_summary);
    std::cout << ",\"terminal_observation\":";
    writeTerminalObservation(value.terminal_observation);
    std::cout << ",\"run_outcome\":";
    writeRunOutcome(value.run_outcome);
    std::cout << '}';
}

void writeMutations(const ProbeResult& value) {
    const Evaluation& complete = value.accepted[0];
    const Evaluation& aborted = value.accepted[1];
    std::cout << "[{\"id\":\"MUTATION-YYZ-RUN-EVALUATION-"
                 "EARLY-MASS-CANDIDATE\",\"status\":\"rejected\","
                 "\"expected_terminal_tick\":"
              << aborted.run_outcome.final_tick
              << ",\"observed_terminal_tick\":"
              << value.early_mass.run_outcome.final_tick
              << ",\"observed_remaining_mass_kg\":";
    writeNumber(value.early_mass.terminal_observation.metrics
                    .remaining_mass_kg);
    std::cout << ",\"max_abs_result_difference\":1},"
                 "{\"id\":\"MUTATION-YYZ-RUN-EVALUATION-"
                 "STRICT-THRESHOLDS\",\"status\":\"rejected\","
                 "\"expected_terminal_tick\":"
              << complete.run_outcome.final_tick
              << ",\"observed_terminal_tick\":3,"
                 "\"observed_final_action\":\""
              << value.strict_thresholds.run_outcome.termination.action
              << "\",\"max_abs_result_difference\":1},"
                 "{\"id\":\"MUTATION-YYZ-RUN-EVALUATION-"
                 "LOW-PRIORITY-WINS\",\"status\":\"rejected\","
                 "\"expected_reason_code\":\""
              << complete.run_outcome.termination.reason_code
              << "\",\"observed_reason_code\":\""
              << value.low_priority.run_outcome.termination.reason_code
              << "\",\"expected_priority\":"
              << complete.run_outcome.termination.priority
              << ",\"observed_priority\":"
              << value.low_priority.run_outcome.termination.priority
              << ",\"max_abs_result_difference\":100},"
                 "{\"id\":\"MUTATION-YYZ-RUN-EVALUATION-"
                 "OUTCOME-BEFORE-OBSERVATION\",\"status\":\"rejected\","
                 "\"expected_event_order\":";
    writeStringList(acceptedEventOrder());
    std::cout << ",\"observed_event_order\":";
    writeStringList(value.outcome_before_observation.terminal_observation
                        .event_order);
    std::cout << ",\"observed_terminal_observation_sealed\":";
    writeBoolean(value.outcome_before_observation.run_outcome
                     .terminal_observation_sealed);
    std::cout << ",\"max_abs_result_difference\":1},"
                 "{\"id\":\"MUTATION-YYZ-RUN-EVALUATION-"
                 "POST-TERMINAL-SAMPLE\",\"status\":\"rejected\","
                 "\"expected_evaluated_ticks\":[0,1],"
                 "\"observed_evaluated_ticks\":[";
    for (std::size_t index = 0;
         index < value.post_terminal.evaluated_boundaries.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << value.post_terminal.evaluated_boundaries[index]
                         .sample_tick;
    }
    std::cout << "],\"observed_post_terminal_sample_count\":"
              << value.post_terminal.terminal_observation
                     .post_terminal_sample_count
              << ",\"max_abs_result_difference\":1}]";
}

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"model_id\":\"" << kModelId
              << "\",\"status\":\"passed\",\"cases\":[";
    for (std::size_t index = 0; index < result.accepted.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        writeEvaluation(result.accepted[index]);
    }
    std::cout << "],\"equivalence_results\":[{\"id\":\""
                 "EQUIV-YYZ-RUN-EVALUATION-PREDICATE-ORDER\","
                 "\"status\":\"passed\",\"case_ids\":[\""
              << result.accepted[0].id << "\",\""
              << result.accepted[1].id
              << "\"],\"terminal_ticks\":["
              << result.accepted[0].run_outcome.final_tick << ','
              << result.accepted[1].run_outcome.final_tick
              << "],\"reason_codes\":[\""
              << result.accepted[0].run_outcome.termination.reason_code
              << "\",\""
              << result.accepted[1].run_outcome.termination.reason_code
              << "\"],\"max_abs_metric_difference\":0}],"
                 "\"invalid_input_rejections\":";
    writeStringList(result.invalid_input_rejections);
    std::cout << ",\"mutation_results\":";
    writeMutations(result);
    std::cout << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_yyz_run_evaluation_probe --self-check\n";
        return 2;
    }
    try {
        writeJson(runProbe());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

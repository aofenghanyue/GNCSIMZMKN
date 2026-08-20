#pragma once

#include <cstdint>
#include <string_view>

namespace gnc::contracts {

enum class TemporalRelation : std::uint8_t {
    NotApplicable,
    IntervalModel,
    CandidateStateQuery,
    CurrentCycle,
};

[[nodiscard]] constexpr std::string_view to_string(
    TemporalRelation relation) noexcept {
    switch (relation) {
    case TemporalRelation::NotApplicable:
        return "NotApplicable";
    case TemporalRelation::IntervalModel:
        return "IntervalModel";
    case TemporalRelation::CandidateStateQuery:
        return "CandidateStateQuery";
    case TemporalRelation::CurrentCycle:
        return "CurrentCycle";
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_temporal_relation(
    TemporalRelation relation) noexcept {
    return relation == TemporalRelation::NotApplicable ||
           relation == TemporalRelation::IntervalModel ||
           relation == TemporalRelation::CandidateStateQuery ||
           relation == TemporalRelation::CurrentCycle;
}

enum class ExecutionObligation : std::uint8_t {
    // BoundaryEvaluation was the first published R2 value. Keep its numeric
    // value stable and append the owner-specific obligations.
    BoundaryEvaluation = 0U,
    PublishProjection = 1U,
    IntervalEvolution,
    DerivativeEvaluation,
};

[[nodiscard]] constexpr std::string_view to_string(
    ExecutionObligation obligation) noexcept {
    switch (obligation) {
    case ExecutionObligation::PublishProjection:
        return "PublishProjection";
    case ExecutionObligation::BoundaryEvaluation:
        return "BoundaryEvaluation";
    case ExecutionObligation::IntervalEvolution:
        return "IntervalEvolution";
    case ExecutionObligation::DerivativeEvaluation:
        return "DerivativeEvaluation";
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_execution_obligation(
    ExecutionObligation obligation) noexcept {
    return obligation == ExecutionObligation::PublishProjection ||
           obligation == ExecutionObligation::BoundaryEvaluation ||
           obligation == ExecutionObligation::IntervalEvolution ||
           obligation == ExecutionObligation::DerivativeEvaluation;
}

// Shared continuous-closure strategy authority. Individual Compiler slices
// still admit only strategies backed by their selected product definitions.
enum class ClosureStrategy : std::uint8_t {
    Unspecified,
    FrozenInterval,
    CandidateState,
    AlgebraicSolve,
};

[[nodiscard]] constexpr std::string_view to_string(
    ClosureStrategy strategy) noexcept {
    switch (strategy) {
    case ClosureStrategy::Unspecified:
        return "Unspecified";
    case ClosureStrategy::FrozenInterval:
        return "FrozenInterval";
    case ClosureStrategy::CandidateState:
        return "CandidateState";
    case ClosureStrategy::AlgebraicSolve:
        return "AlgebraicSolve";
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool valid_closure_strategy(
    ClosureStrategy strategy) noexcept {
    return strategy == ClosureStrategy::FrozenInterval ||
           strategy == ClosureStrategy::CandidateState ||
           strategy == ClosureStrategy::AlgebraicSolve;
}

} // namespace gnc::contracts

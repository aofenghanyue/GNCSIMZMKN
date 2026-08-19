#pragma once

#include <cstdint>
#include <string_view>

namespace gnc::contracts {

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

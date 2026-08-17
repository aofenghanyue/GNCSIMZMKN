#pragma once

namespace gnc::model_sdk {

// Call-local result of a stateless AlgorithmKernel evaluation. Output is the
// typed value available to downstream model consumers. Telemetry only
// explains the completed evaluation and has no physical decision authority.
template <typename Output, typename Telemetry>
struct AlgorithmEvaluation {
    Output output;
    Telemetry telemetry;
};

} // namespace gnc::model_sdk

#include "gnc/core/integrators/rk4_integrator.hpp"
#include "gnc/core/simulator.hpp"
#include "gnc/interfaces/i_continuous_group.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-GROUP-04";
constexpr const char* kScopeId = "scope:mass-position";
constexpr double kTolerance = 1.0e-12;

struct StageEvent {
    int rk_stage = 0;
    double time_s = 0.0;
    double candidate_mass_kg = 0.0;
    double candidate_position_m = 0.0;
    double mass_rate_kg_per_s = 0.0;
    double position_rate_mps = 0.0;
};

struct CaptureResult {
    std::vector<StageEvent> stages;
    int commit_count = 0;
    double committed_mass_kg = 0.0;
    double committed_position_m = 0.0;
    double member_mass_kg = 0.0;
    double member_position_m = 0.0;
    bool unregistered_member_rejected = false;
    bool duplicate_ownership_rejected = false;
};

class ScalarMember final : public gnc::core::SimulationNode,
                           public gnc::interfaces::IContinuousSystem {
public:
    ScalarMember(const char* type_name,
                 const char* state_name,
                 double initial_value)
        : SimulationNode(type_name) {
        state_index_ = layout_.addVariable(state_name);
        state_ = Eigen::VectorXd::Constant(1, initial_value);
        initial_state_ = state_;
    }

    const gnc::core::StateLayout& getStateLayout() const override {
        return layout_;
    }

    void computeDerivatives(double,
                            const Eigen::VectorXd&,
                            Eigen::VectorXd& derivative) const override {
        derivative = Eigen::VectorXd::Zero(1);
    }

    const Eigen::VectorXd& getState() const override { return state_; }
    void setState(const Eigen::VectorXd& state) override { state_ = state; }
    Eigen::VectorXd getInitialState() const override { return initial_state_; }
    double value() const { return state_[state_index_]; }

private:
    gnc::core::StateLayout layout_;
    Eigen::VectorXd state_;
    Eigen::VectorXd initial_state_;
    int state_index_ = -1;
};

class JointCandidateScope final : public gnc::core::SimulationNode,
                                  public gnc::interfaces::IContinuousGroup {
public:
    JointCandidateScope(ScalarMember* mass,
                        ScalarMember* position,
                        CaptureResult* capture)
        : SimulationNode("JointCandidateScope"),
          mass_(mass),
          position_(position),
          capture_(capture) {
        mass_index_ = layout_.addVariable("mass_kg");
        position_index_ = layout_.addVariable("position_m");
        state_ = Eigen::VectorXd::Zero(2);
        state_[mass_index_] = mass_->value();
        state_[position_index_] = position_->value();
        initial_state_ = state_;
    }

    std::vector<gnc::interfaces::IContinuousSystem*> members() const override {
        return {mass_, position_};
    }

    const gnc::core::StateLayout& getStateLayout() const override {
        return layout_;
    }

    void computeDerivatives(double time_s,
                            const Eigen::VectorXd& state,
                            Eigen::VectorXd& derivative) const override {
        derivative = Eigen::VectorXd::Zero(2);
        derivative[mass_index_] = -2.0;
        derivative[position_index_] = state[mass_index_];
        if (capture_) {
            capture_->stages.push_back(StageEvent{
                static_cast<int>(capture_->stages.size() + 1),
                time_s,
                state[mass_index_],
                state[position_index_],
                derivative[mass_index_],
                derivative[position_index_],
            });
        }
    }

    const Eigen::VectorXd& getState() const override { return state_; }

    void setState(const Eigen::VectorXd& state) override {
        state_ = state;
        mass_->setState(Eigen::VectorXd::Constant(1, state_[mass_index_]));
        position_->setState(
            Eigen::VectorXd::Constant(1, state_[position_index_]));
        if (capture_) {
            ++capture_->commit_count;
            capture_->committed_mass_kg = state_[mass_index_];
            capture_->committed_position_m = state_[position_index_];
        }
    }

    Eigen::VectorXd getInitialState() const override { return initial_state_; }

private:
    ScalarMember* mass_ = nullptr;
    ScalarMember* position_ = nullptr;
    CaptureResult* capture_ = nullptr;
    gnc::core::StateLayout layout_;
    Eigen::VectorXd state_;
    Eigen::VectorXd initial_state_;
    int mass_index_ = -1;
    int position_index_ = -1;
};

class MembershipScope final : public gnc::core::SimulationNode,
                              public gnc::interfaces::IContinuousGroup {
public:
    explicit MembershipScope(
        std::vector<gnc::interfaces::IContinuousSystem*> members)
        : SimulationNode("MembershipScope"), members_(std::move(members)) {
        layout_.addVariable("scope_state");
        state_ = Eigen::VectorXd::Zero(1);
    }

    std::vector<gnc::interfaces::IContinuousSystem*> members() const override {
        return members_;
    }
    const gnc::core::StateLayout& getStateLayout() const override {
        return layout_;
    }
    void computeDerivatives(double,
                            const Eigen::VectorXd&,
                            Eigen::VectorXd& derivative) const override {
        derivative = Eigen::VectorXd::Zero(1);
    }
    const Eigen::VectorXd& getState() const override { return state_; }
    void setState(const Eigen::VectorXd& state) override { state_ = state; }
    Eigen::VectorXd getInitialState() const override { return state_; }

private:
    std::vector<gnc::interfaces::IContinuousSystem*> members_;
    gnc::core::StateLayout layout_;
    Eigen::VectorXd state_;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kTolerance;
}

bool captureUnregisteredMemberFailure() {
    gnc::core::Simulator simulator;
    simulator.configure({1.0, 1.0});
    simulator.setIntegrator(std::make_unique<gnc::core::RK4Integrator>());

    auto orphan = std::make_unique<ScalarMember>("Orphan", "orphan", 1.0);
    auto scope = std::make_unique<MembershipScope>(
        std::vector<gnc::interfaces::IContinuousSystem*>{orphan.get()});
    auto* scope_ptr = scope.get();
    simulator.getRegistry().add<MembershipScope,
                                gnc::interfaces::IContinuousGroup>(
        "scope.invalid", std::move(scope));
    simulator.addNodeExecution(scope_ptr);
    try {
        simulator.initialize();
    } catch (const std::exception& error) {
        return std::string(error.what()).find("not registered as a node") !=
               std::string::npos;
    }
    return false;
}

bool captureDuplicateOwnershipFailure() {
    gnc::core::Simulator simulator;
    simulator.configure({1.0, 1.0});
    simulator.setIntegrator(std::make_unique<gnc::core::RK4Integrator>());

    auto shared = std::make_unique<ScalarMember>("Shared", "shared", 1.0);
    auto* shared_ptr = shared.get();
    simulator.getRegistry().add<ScalarMember,
                                gnc::interfaces::IContinuousSystem>(
        "member.shared", std::move(shared));
    simulator.addNodeExecution(shared_ptr);

    for (const std::string& name : {std::string("scope.first"),
                                    std::string("scope.second")}) {
        auto scope = std::make_unique<MembershipScope>(
            std::vector<gnc::interfaces::IContinuousSystem*>{shared_ptr});
        auto* scope_ptr = scope.get();
        simulator.getRegistry().add<MembershipScope,
                                    gnc::interfaces::IContinuousGroup>(
            name, std::move(scope));
        simulator.addNodeExecution(scope_ptr);
    }

    try {
        simulator.initialize();
    } catch (const std::exception& error) {
        return std::string(error.what()).find(
                   "more than one continuous group") != std::string::npos;
    }
    return false;
}

CaptureResult captureLegacyBehavior() {
    CaptureResult result;
    gnc::core::Simulator simulator;
    simulator.configure({1.0, 1.0});
    simulator.setIntegrator(std::make_unique<gnc::core::RK4Integrator>());

    auto mass = std::make_unique<ScalarMember>("Mass", "mass_kg", 10.0);
    auto* mass_ptr = mass.get();
    simulator.getRegistry().add<ScalarMember,
                                gnc::interfaces::IContinuousSystem>(
        "member.mass", std::move(mass));
    simulator.addNodeExecution(mass_ptr);

    auto position =
        std::make_unique<ScalarMember>("Position", "position_m", 0.0);
    auto* position_ptr = position.get();
    simulator.getRegistry().add<ScalarMember,
                                gnc::interfaces::IContinuousSystem>(
        "member.position", std::move(position));
    simulator.addNodeExecution(position_ptr);

    auto scope = std::make_unique<JointCandidateScope>(
        mass_ptr, position_ptr, &result);
    auto* scope_ptr = scope.get();
    simulator.getRegistry().add<JointCandidateScope,
                                gnc::interfaces::IContinuousGroup>(
        kScopeId, std::move(scope));
    simulator.addNodeExecution(scope_ptr);

    simulator.run();
    result.member_mass_kg = mass_ptr->value();
    result.member_position_m = position_ptr->value();
    result.unregistered_member_rejected = captureUnregisteredMemberFailure();
    result.duplicate_ownership_rejected = captureDuplicateOwnershipFailure();
    return result;
}

int parseRerunIndex(const std::string& value) {
    std::size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    require(consumed == value.size() && parsed > 0,
            "rerun index must be a positive integer");
    return parsed;
}

void validateCapture(const CaptureResult& result) {
    const std::vector<StageEvent> expected{
        {1, 0.0, 10.0, 0.0, -2.0, 10.0},
        {2, 0.5, 9.0, 5.0, -2.0, 9.0},
        {3, 0.5, 9.0, 4.5, -2.0, 9.0},
        {4, 1.0, 8.0, 9.0, -2.0, 8.0},
    };
    require(result.stages.size() == expected.size(),
            "Legacy group did not produce four RK stages");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const StageEvent& actual = result.stages[index];
        const StageEvent& target = expected[index];
        require(actual.rk_stage == target.rk_stage &&
                    nearlyEqual(actual.time_s, target.time_s) &&
                    nearlyEqual(actual.candidate_mass_kg,
                                target.candidate_mass_kg) &&
                    nearlyEqual(actual.candidate_position_m,
                                target.candidate_position_m) &&
                    nearlyEqual(actual.mass_rate_kg_per_s,
                                target.mass_rate_kg_per_s) &&
                    nearlyEqual(actual.position_rate_mps,
                                target.position_rate_mps),
                "Legacy group RK stage differs from the analytic trace");
    }
    require(result.commit_count == 1 &&
                nearlyEqual(result.committed_mass_kg, 8.0) &&
                nearlyEqual(result.committed_position_m, 9.0) &&
                nearlyEqual(result.member_mass_kg, 8.0) &&
                nearlyEqual(result.member_position_m, 9.0),
            "Legacy group commit differs from the analytic result");
    require(result.unregistered_member_rejected,
            "Legacy group accepted an unregistered member");
    require(result.duplicate_ownership_rejected,
            "Legacy group accepted duplicate member ownership");
}

void writeTrace(const std::string& path,
                int rerun_index,
                const CaptureResult& result) {
    std::ofstream output(path, std::ios::binary);
    require(output.is_open(), "could not open the requested trace path");
    output << std::setprecision(17)
           << "{\"schema_version\":\"gnczmkn.legacy-group-trace/1\""
           << ",\"oracle_id\":\"" << kOracleId << "\""
           << ",\"rerun_index\":" << rerun_index
           << ",\"scope_id\":\"" << kScopeId << "\""
           << ",\"member_ids\":[\"mass\",\"position\"]"
           << ",\"events\":[";
    for (std::size_t index = 0; index < result.stages.size(); ++index) {
        if (index > 0) {
            output << ',';
        }
        const StageEvent& stage = result.stages[index];
        output << "{\"sequence\":" << index
               << ",\"event_kind\":\"rk-stage\""
               << ",\"rk_stage\":" << stage.rk_stage
               << ",\"time_s\":" << stage.time_s
               << ",\"candidate_mass_kg\":" << stage.candidate_mass_kg
               << ",\"candidate_position_m\":"
               << stage.candidate_position_m
               << ",\"mass_rate_kg_per_s\":"
               << stage.mass_rate_kg_per_s
               << ",\"position_rate_mps\":" << stage.position_rate_mps
               << '}';
    }
    output << ",{";
    output << "\"sequence\":4,\"event_kind\":\"group-commit\""
           << ",\"effective_time_s\":1"
           << ",\"committed_mass_kg\":" << result.committed_mass_kg
           << ",\"committed_position_m\":"
           << result.committed_position_m << "}]"
           << ",\"member_final\":{\"mass_kg\":" << result.member_mass_kg
           << ",\"position_m\":" << result.member_position_m << '}'
           << ",\"unregistered_member_rejected\":"
           << (result.unregistered_member_rejected ? "true" : "false")
           << ",\"duplicate_ownership_rejected\":"
           << (result.duplicate_ownership_rejected ? "true" : "false")
           << "}\n";
    require(output.good(), "could not write the Legacy group trace");
}

} // namespace


int main(int argc, char** argv) {
    if (argc != 5 || std::string(argv[1]) != "--output" ||
        std::string(argv[3]) != "--rerun-index") {
        std::cerr << "usage: legacy_group_capture --output <path> "
                     "--rerun-index <positive integer>\n";
        return 2;
    }

    try {
        const int rerun_index = parseRerunIndex(argv[4]);
        const CaptureResult result = captureLegacyBehavior();
        validateCapture(result);
        writeTrace(argv[2], rerun_index, result);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

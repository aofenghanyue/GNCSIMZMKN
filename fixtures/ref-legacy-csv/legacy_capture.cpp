#include "gnc/core/integrators/rk4_integrator.hpp"
#include "gnc/core/simulator.hpp"
#include "gnc/infrastructure/observable_helpers.hpp"
#include "gnc/interfaces/i_continuous_system.hpp"
#include "gnc/interfaces/i_observable.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

constexpr double kTolerance = 1.0e-12;

class ConstantAccelerationState final
    : public gnc::core::SimulationNode,
      public gnc::core::IPublishTask,
      public gnc::interfaces::IContinuousSystem,
      public gnc::interfaces::IObservable {
public:
    ConstantAccelerationState()
        : SimulationNode("LegacyCsvConstantAcceleration") {
        altitude_index_ = layout_.addVariable("altitude_m");
        velocity_index_ = layout_.addVariable("vertical_velocity_mps");
        state_ = Eigen::VectorXd::Zero(layout_.dimension());
        state_[altitude_index_] = 1000.0;
        state_[velocity_index_] = 10.0;
        initial_state_ = state_;
    }

    void initialize() override { publish({0.0, 0}); }

    gnc::core::PublishPhase publishPhase() const override {
        return gnc::core::PublishPhase::StateOwner;
    }

    void publish(const gnc::core::PublishContext& context) override {
        published_position_m_ = {0.0, 0.0, state_[altitude_index_]};
        published_velocity_mps_ = {0.0, 0.0, state_[velocity_index_]};
        published_acceleration_mps2_ = {0.0, 0.0, -2.0};
        published_time_s_ = context.time_s;
    }

    const gnc::core::StateLayout& getStateLayout() const override {
        return layout_;
    }

    void computeDerivatives(double,
                            const Eigen::VectorXd& state,
                            Eigen::VectorXd& derivative) const override {
        derivative = Eigen::VectorXd::Zero(layout_.dimension());
        derivative[altitude_index_] = state[velocity_index_];
        derivative[velocity_index_] = -2.0;
    }

    const Eigen::VectorXd& getState() const override { return state_; }
    void setState(const Eigen::VectorXd& state) override { state_ = state; }
    Eigen::VectorXd getInitialState() const override { return initial_state_; }

    std::vector<gnc::interfaces::ObservableField>
    getObservableFields() const override {
        gnc::core::ObservableFieldBuilder builder;
        builder.addVector3(
            "position",
            [this]() -> const gnc::math::Vector3& {
                return published_position_m_;
            });
        builder.addVector3(
            "velocity",
            [this]() -> const gnc::math::Vector3& {
                return published_velocity_mps_;
            });
        builder.addVector3(
            "acceleration",
            [this]() -> const gnc::math::Vector3& {
                return published_acceleration_mps2_;
            });
        builder.addScalar(
            "speed", [this]() { return published_velocity_mps_.norm(); });
        builder.addScalar(
            "altitude", [this]() { return published_position_m_.z(); });
        return builder.build();
    }

    double altitudeM() const { return state_[altitude_index_]; }
    double verticalVelocityMps() const { return state_[velocity_index_]; }
    double publishedTimeS() const { return published_time_s_; }

private:
    gnc::core::StateLayout layout_;
    Eigen::VectorXd state_;
    Eigen::VectorXd initial_state_;
    gnc::math::Vector3 published_position_m_ = gnc::math::Vector3::Zero();
    gnc::math::Vector3 published_velocity_mps_ = gnc::math::Vector3::Zero();
    gnc::math::Vector3 published_acceleration_mps2_ =
        gnc::math::Vector3::Zero();
    double published_time_s_ = 0.0;
    int altitude_index_ = -1;
    int velocity_index_ = -1;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kTolerance;
}

gnc::core::ConfigNode loggerConfig(const std::filesystem::path& output) {
    auto record = gnc::core::ConfigNode::makeObject();
    record.set("vehicle.dynamics", gnc::core::ConfigNode::makeString("all"));

    auto config = gnc::core::ConfigNode::makeObject();
    config.set(
        "directory",
        gnc::core::ConfigNode::makeString(output.parent_path().string()));
    config.set(
        "session_name",
        gnc::core::ConfigNode::makeString(output.stem().string()));
    config.set("format", gnc::core::ConfigNode::makeString("csv"));
    config.set("precision", gnc::core::ConfigNode::makeNumber(12));
    config.set("flush_every_step", gnc::core::ConfigNode::makeBool(true));
    config.set("record_initial_state", gnc::core::ConfigNode::makeBool(true));
    config.set("record", record);
    return config;
}

void runCapture(const std::filesystem::path& output) {
    require(output.extension() == ".csv",
            "output path must use a .csv extension");

    gnc::core::Simulator simulator;
    simulator.configure({0.5, 1.0});
    simulator.setIntegrator(std::make_unique<gnc::core::RK4Integrator>());

    auto state = std::make_unique<ConstantAccelerationState>();
    auto* state_ptr = state.get();
    simulator.getRegistry().add<
        ConstantAccelerationState,
        gnc::interfaces::IContinuousSystem,
        gnc::interfaces::IObservable>("vehicle.dynamics", std::move(state));
    simulator.addNodeExecution(state_ptr);

    require(simulator.initializeAutoDataLogger(loggerConfig(output)),
            "Legacy AutoDataLogger initialization failed");
    simulator.run();

    require(std::filesystem::is_regular_file(output),
            "Legacy CSV output was not created");
    require(nearlyEqual(state_ptr->altitudeM(), 1009.0) &&
                nearlyEqual(state_ptr->verticalVelocityMps(), 8.0) &&
                nearlyEqual(state_ptr->publishedTimeS(), 1.0),
            "Legacy final committed or published state differs");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3 || std::string(argv[1]) != "--output") {
        std::cerr << "usage: legacy_csv_capture --output <path.csv>\n";
        return 2;
    }

    try {
        runCapture(std::filesystem::path(argv[2]));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

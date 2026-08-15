#pragma once

#include <cstdint>
#include <string>

namespace gnc::contracts {

struct FrameIdentity {
    std::string id;

    friend bool operator==(const FrameIdentity& lhs,
                           const FrameIdentity& rhs) noexcept {
        return lhs.id == rhs.id;
    }

    friend bool operator!=(const FrameIdentity& lhs,
                           const FrameIdentity& rhs) noexcept {
        return !(lhs == rhs);
    }
};

struct ClockDomainIdentity {
    std::string id;

    friend bool operator==(const ClockDomainIdentity& lhs,
                           const ClockDomainIdentity& rhs) noexcept {
        return lhs.id == rhs.id;
    }

    friend bool operator!=(const ClockDomainIdentity& lhs,
                           const ClockDomainIdentity& rhs) noexcept {
        return !(lhs == rhs);
    }
};

struct SimulationInstant {
    std::int64_t tick = 0;
    double seconds = 0.0;
};

enum class DataQuality : std::uint8_t {
    Valid,
    Degraded,
    Invalid,
};

struct SampleContext {
    FrameIdentity frame;
    ClockDomainIdentity clock_domain;
    SimulationInstant sample_time;
    std::int64_t configuration_revision = 0;
    DataQuality quality = DataQuality::Invalid;
};

struct HalfOpenValidityInterval {
    SimulationInstant effective_from;
    SimulationInstant effective_until;
};

struct IntervalSampleContext {
    SampleContext sample;
    HalfOpenValidityInterval validity;
};

} // namespace gnc::contracts

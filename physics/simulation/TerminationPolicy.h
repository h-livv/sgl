#pragma once
#include <core/GeodesicState.h>

namespace Simulation {

class TerminationPolicy {
public:
    virtual ~TerminationPolicy() = default;
    virtual bool should_terminate(const State& state) const = 0;
};

class HorizonTermination : public TerminationPolicy {
public:
    explicit HorizonTermination(double event_horizon_radius, double safety_factor = 1.001);

    bool should_terminate(const State& state) const override;

private:
    double r_horizon_;
};

class RadiusBoundTermination : public TerminationPolicy {
public:
    explicit RadiusBoundTermination(double r_min, double r_max);

    bool should_terminate(const State& state) const override;

private:
    double r_min_;
    double r_max_;
};

} // namespace Simulation

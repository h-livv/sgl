#pragma once

#include <core/GeodesicState.h>

namespace Propagation {

// Stop condition polled *before* each RK4 step. True means do not take another
// step. Observer-plane crossing lives in Arrivals::PlaneCrossingTermination
// (typically wrapping RadiusBoundTermination as fallback), not in this module.
class TerminationPolicy {
public:
    virtual ~TerminationPolicy() = default;
    virtual bool should_terminate(const State& state) const = 0;
};

// Horizon/escape fallback: stop when spherical radius r = X[1] is at or inside
// r_min or at or beyond r_max. Imaging uses this inside PlaneCrossingTermination;
// without a plane it is the sole stop condition.
class RadiusBoundTermination : public TerminationPolicy {
public:
    RadiusBoundTermination(double r_min, double r_max);

    bool should_terminate(const State& state) const override;

private:
    double r_min_;
    double r_max_;
};

} // namespace Propagation

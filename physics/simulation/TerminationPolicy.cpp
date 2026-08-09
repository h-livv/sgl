#include "TerminationPolicy.h"

namespace Simulation {

HorizonTermination::HorizonTermination(double event_horizon_radius, double safety_factor)
    : r_horizon_(event_horizon_radius * safety_factor) {}

bool HorizonTermination::should_terminate(const State& state) const {
    return state.X[1] <= r_horizon_;
}

RadiusBoundTermination::RadiusBoundTermination(double r_min, double r_max)
    : r_min_(r_min), r_max_(r_max) {}

bool RadiusBoundTermination::should_terminate(const State& state) const {
    const double r = state.X[1];
    return r <= r_min_ || r >= r_max_;
}

} // namespace Simulation

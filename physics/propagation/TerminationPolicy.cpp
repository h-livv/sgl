#include "TerminationPolicy.h"

namespace Propagation {

RadiusBoundTermination::RadiusBoundTermination(double r_min, double r_max)
    : r_min_(r_min), r_max_(r_max) {}

bool RadiusBoundTermination::should_terminate(const State& state) const {
    const double r = state.X[1];
    return r <= r_min_ || r >= r_max_;
}

} // namespace Propagation

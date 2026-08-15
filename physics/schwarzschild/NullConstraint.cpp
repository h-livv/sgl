#include "NullConstraint.h"

#include <cmath>

namespace Schwarzschild {

void project_onto_null_cone(State& state, double rs) {
    const double r = state.X[1];
    const double f = 1.0 - rs / r;
    const double vr = state.U[1];
    const double vth = state.U[2];
    const double vph = state.U[3];
    const double spatial = vr * vr / f + r * r * vth * vth +
                           r * r * std::sin(state.X[2]) * std::sin(state.X[2]) * vph * vph;
    // Future-directed: U^t = sqrt(spatial/f). Spatial components unchanged.
    state.U[0] = std::sqrt(spatial / f);
}

} // namespace Schwarzschild

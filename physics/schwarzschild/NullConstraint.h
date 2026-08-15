#pragma once

#include <core/GeodesicState.h>

namespace Schwarzschild {

// Reset U^t from the current spatial velocity so g(U,U) = 0 (null cone).
// Does not modify U^r, U^θ, U^φ. Optional during propagation (every N steps).
void project_onto_null_cone(State& state, double rs);

} // namespace Schwarzschild

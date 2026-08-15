#pragma once

#include <core/GeodesicState.h>

#include <cstddef>

namespace Rays {

// Minimal unit of propagation input. initial_state is expressed in the Schwarzschild
// spherical chart (t, r, theta, phi) that the propagation kernel integrates in.
// id is assigned by RayEnsemble and equals the ray's index in its ensemble.
// Samplers emit Rays; EnsemblePropagator is the first consumer that may call the GR kernel.
struct Ray {
    State initial_state{};
    std::size_t id = 0;
};

} // namespace Rays

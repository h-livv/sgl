#pragma once

#include "InitialConditions.h"

#include <core/GeodesicState.h>
#include <core/SchwarzschildParameters.h>

namespace Schwarzschild {

// Timelike bound-orbit test state. Fills U^t from g(U,U) = −1.
State build_bound_orbit(const Spacetime::SchwarzschildParameters& parameters,
                        const BoundOrbitInitialConditions& initial);

// Timelike radial-fall test state (E = 1, L = 0). Not used by imaging.
State build_radial_freefall(const Spacetime::SchwarzschildParameters& parameters,
                            const RadialFreefallInitialConditions& initial);

// 1D point-source photon: E = 1, L = b E, U^θ = 0, inward U^r.
State build_null_scatter(const Spacetime::SchwarzschildParameters& parameters,
                         const NullScatterInitialConditions& initial);

// Parallel / 2D launches. If vt == 0, fills U^t from the chosen constraint
// (callers typically push a Cartesian direction through the chart Jacobian
// then pass vt = 0). Non-zero vt is kept as given.
State build_custom(const Spacetime::SchwarzschildParameters& parameters,
                   const CustomInitialConditions& initial, GeodesicKind geodesic);

} // namespace Schwarzschild

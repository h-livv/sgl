#pragma once

#include "InitialConditions.h"

#include <core/GeodesicState.h>
#include <core/SchwarzschildParameters.h>

namespace Schwarzschild {

State build_bound_orbit(const Spacetime::SchwarzschildParameters& parameters,
                        const BoundOrbitInitialConditions& initial);

State build_radial_freefall(const Spacetime::SchwarzschildParameters& parameters,
                            const RadialFreefallInitialConditions& initial);

State build_null_scatter(const Spacetime::SchwarzschildParameters& parameters,
                         const NullScatterInitialConditions& initial);

State build_custom(const Spacetime::SchwarzschildParameters& parameters,
                   const CustomInitialConditions& initial, GeodesicKind geodesic);

} // namespace Schwarzschild

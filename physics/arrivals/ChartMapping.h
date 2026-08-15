#pragma once

#include <core/GeodesicState.h>
#include <geometry/Lens.h>

#include <Eigen/Dense>

namespace Arrivals {

// Chart (t, r, θ, φ) → world Cartesian. Used by plane-crossing tests and
// arrival localization. Does not own the metric.

// Position only: spherical (r, θ, φ) → chart Cartesian → world via
// from_chart_frame (includes lens translation). No Jacobian. Safe every step.
Eigen::Vector3d world_position(const Geometry::Lens& lens, const State& chart_state);

// Unit outgoing world direction of the geodesic.
// sph_to_cart Jacobian maps U, then chart_to_world rotates the spatial
// Cartesian velocity and normalizes. `lens` is unused ((void)lens): a
// direction is not translated. Returns Vector3d::Zero() if that spatial
// velocity has norm 0. Call once per arrival, not every integrator step.
Eigen::Vector3d world_direction(const Geometry::Lens& lens, const State& chart_state);

} // namespace Arrivals

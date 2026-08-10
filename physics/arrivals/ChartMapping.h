#pragma once

#include <core/GeodesicState.h>
#include <geometry/Lens.h>

#include <Eigen/Dense>

namespace Arrivals {

// Schwarzschild chart (t, r, theta, phi) -> Phase 2 world frame.
// Position-only; no Jacobian. Safe to call on every integration step.
Eigen::Vector3d world_position(const Geometry::Lens& lens, const State& chart_state);

// Unit world-space propagation direction. Returns Vector3d::Zero() if the mapped
// velocity has zero norm. Uses CoordinateChart::sph_to_cart; call once per arrival.
Eigen::Vector3d world_direction(const Geometry::Lens& lens, const State& chart_state);

} // namespace Arrivals

#pragma once

#include <core/GeodesicState.h>

#include <Eigen/Dense>

#include <cstddef>

namespace Arrivals {

// Observation after geodesic propagation. Downstream of rays/propagation; does
// not own the metric.
//
// Pipeline: physical problem → ray sampling → Schwarzschild geodesic
// propagation → this module (plane stop, localize, angles, optional 2D Newton)
// → ImageFormation.
// Crossing the observer plane is not an observer hit. The hit condition is
// plane residual (0, 0). 1D bisection lives in the experiment; 2D Newton is
// ObserverLaunchRefiner.

enum class ArrivalStatus {
    Arrived,     // the ray reached the observer plane; all fields are meaningful
    NoCrossing,  // ended without a plane crossing (horizon, escape, or step budget)
};

// One result per propagated ray, index-aligned with the originating RayEnsemble.
// When status == NoCrossing, only ray_id and status are meaningful; every other
// field is default-initialised.
// world_position is the first-order plane-crossing estimate.
// world_direction is the unit outgoing spatial velocity there; the sky vector
// used for imaging is −world_direction (see observer_angular_coordinates).
struct RayArrival {
    std::size_t ray_id = 0;
    Eigen::Vector3d world_position = Eigen::Vector3d::Zero();
    Eigen::Vector3d world_direction = Eigen::Vector3d::Zero();
    State chart_state{};
    ArrivalStatus status = ArrivalStatus::NoCrossing;
};

} // namespace Arrivals

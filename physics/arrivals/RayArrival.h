#pragma once

#include <core/GeodesicState.h>

#include <Eigen/Dense>

#include <cstddef>

namespace Arrivals {

enum class ArrivalStatus {
    Arrived,     // the ray reached the observer plane; all fields are meaningful
    NoCrossing,  // propagation ended without reaching the plane
};

// One result per propagated ray, index-aligned with the originating RayEnsemble.
// When status == NoCrossing, only ray_id and status are meaningful; every other
// field is default-initialised.
struct RayArrival {
    std::size_t ray_id = 0;
    Eigen::Vector3d world_position = Eigen::Vector3d::Zero();
    Eigen::Vector3d world_direction = Eigen::Vector3d::Zero();
    State chart_state{};
    ArrivalStatus status = ArrivalStatus::NoCrossing;
};

} // namespace Arrivals

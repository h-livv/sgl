#pragma once

#include <Eigen/Dense>

namespace Geometry {

// Point source: world position only (no extent or spectrum).
// Aligned setup: (0, 0, −S). RaySampler launches from this event;
// RayGrid2DSampler offsets a launch plane through this point.
struct Source {
    Eigen::Vector3d position = Eigen::Vector3d::Zero();
};

} // namespace Geometry

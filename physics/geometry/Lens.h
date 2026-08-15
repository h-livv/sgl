#pragma once

#include "WorldFrame.h"

#include <core/SchwarzschildParameters.h>

#include <Eigen/Dense>

namespace Geometry {

// Compact-object lens: world position plus Schwarzschild radius rs.
// rs lives here so sgl_geometry can keep source/observer outside the horizon
// without linking the geodesic kernel (sgl_physics).
struct Lens {
    Eigen::Vector3d position = Eigen::Vector3d::Zero();
    Spacetime::SchwarzschildParameters parameters{};
};

inline double horizon_radius(const Lens& lens) { return lens.parameters.rs; }

// World → lens-centered world (translation only; axes unchanged).
inline Eigen::Vector3d to_lens_frame(const Lens& lens, const Eigen::Vector3d& world_point) {
    return world_point - lens.position;
}

// Lens-centered world → chart Cartesian (Z,X,Y). Used by samplers and ChartMapping.
inline Eigen::Vector3d to_chart_frame(const Lens& lens, const Eigen::Vector3d& world_point) {
    return WorldFrame::world_to_chart(to_lens_frame(lens, world_point));
}

// Inverse of to_chart_frame: chart Cartesian → world.
inline Eigen::Vector3d from_chart_frame(const Lens& lens, const Eigen::Vector3d& chart_point) {
    return lens.position + WorldFrame::chart_to_world(chart_point);
}

} // namespace Geometry

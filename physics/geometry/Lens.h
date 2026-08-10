#pragma once

#include "WorldFrame.h"

#include <core/SchwarzschildParameters.h>

#include <Eigen/Dense>

namespace Geometry {

struct Lens {
    Eigen::Vector3d position = Eigen::Vector3d::Zero();
    Spacetime::SchwarzschildParameters parameters{};
};

inline double horizon_radius(const Lens& lens) { return lens.parameters.rs; }

inline Eigen::Vector3d to_lens_frame(const Lens& lens, const Eigen::Vector3d& world_point) {
    return world_point - lens.position;
}

inline Eigen::Vector3d to_chart_frame(const Lens& lens, const Eigen::Vector3d& world_point) {
    return WorldFrame::world_to_chart(to_lens_frame(lens, world_point));
}

inline Eigen::Vector3d from_chart_frame(const Lens& lens, const Eigen::Vector3d& chart_point) {
    return lens.position + WorldFrame::chart_to_world(chart_point);
}

} // namespace Geometry

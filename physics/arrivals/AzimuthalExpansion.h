#pragma once

#include "RayArrival.h"

#include <geometry/ImagePlane.h>

#include <Eigen/Dense>

#include <cstddef>
#include <vector>

namespace Arrivals {

// One image-plane arrival position produced by the azimuthal symmetry expansion.
// ray_id is copied from the originating RayArrival, so every rotational copy of a
// given ray shares that ray's id.
struct PlaneArrival {
    std::size_t ray_id = 0;
    Eigen::Vector2d plane_position = Eigen::Vector2d::Zero();
};

// Expands the equatorial arrival family into a rotationally symmetric 2D set by
// exploiting the exact axisymmetry of the aligned Schwarzschild problem about the
// optical axis. Performs no propagation.
//
// Precondition: arrivals come from the chart-equatorial ray family, so their plane
// v coordinate is zero to machine precision; v is discarded and the radial coordinate
// is carried by the signed u.
//
// Entries with status != ArrivalStatus::Arrived are skipped, so the result is NOT
// index-aligned with the input; association is preserved through ray_id.
//
// Throws std::invalid_argument if azimuth_count < 1.
std::vector<PlaneArrival> expand_azimuthally(const std::vector<RayArrival>& arrivals,
                                             const Geometry::ImagePlane& plane,
                                             int azimuth_count);

} // namespace Arrivals

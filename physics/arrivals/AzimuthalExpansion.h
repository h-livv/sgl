#pragma once

#include "RayArrival.h"

#include <geometry/ImagePlane.h>

#include <Eigen/Dense>

#include <cstddef>
#include <vector>

namespace Arrivals {

// One image-plane arrival position produced by the azimuthal symmetry expansion.
// ray_id is copied from the originating RayArrival, so every rotational copy of a
// given ray shares that ray's id. Not used by the current angular imaging path.
struct PlaneArrival {
    std::size_t ray_id = 0;
    Eigen::Vector2d plane_position = Eigen::Vector2d::Zero();
};

// Plane-coordinate azimuthal copy of equatorial arrivals. Performs no propagation.
// Imaging experiments currently expand ANGULAR coordinates instead
// (expand_angular_azimuthally / fill_aligned_observer_ring); this path remains
// for plane-(u, v) pictures.
//
// Precondition: arrivals come from the chart-equatorial ray family, so their plane
// v coordinate is zero to machine precision; v is discarded and the radial coordinate
// is carried by the signed u. Rotation is (u cos ψ, u sin ψ).
//
// Entries with status != ArrivalStatus::Arrived are skipped, so the result is NOT
// index-aligned with the input; association is preserved through ray_id.
// If u == 0, one origin point is emitted (not azimuth_count copies).
//
// Throws std::invalid_argument if azimuth_count < 1.
std::vector<PlaneArrival> expand_azimuthally(const std::vector<RayArrival>& arrivals,
                                             const Geometry::ImagePlane& plane,
                                             int azimuth_count);

} // namespace Arrivals

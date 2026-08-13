#pragma once

#include "RayArrival.h"

#include <geometry/Observer.h>

#include <Eigen/Dense>

#include <optional>
#include <vector>

namespace Arrivals {

// Maps an arrived ray to observer-centered gnomonic tangent-plane angular coordinates:
//   (u_ang, v_ang) = (dot(s,r)/den, dot(s,u)/den)
// where s = -normalize(world_direction) is the sky direction and den = dot(s, forward).
// Returns nullopt for non-arrivals, zero direction, or behind-observer (den <= 0).
std::optional<Eigen::Vector2d> observer_angular_coordinates(const RayArrival& arrival,
                                                            const Geometry::Observer& observer);

// Expands a signed equatorial angular coordinate into a rotationally symmetric set.
// Preserves signed u_ang at k=0. Throws std::invalid_argument if azimuth_count < 1.
std::vector<Eigen::Vector2d> expand_angular_azimuthally(double signed_u_ang, int azimuth_count);

} // namespace Arrivals

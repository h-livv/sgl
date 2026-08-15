#pragma once

#include "RayArrival.h"

#include <geometry/Observer.h>

#include <Eigen/Dense>

#include <optional>
#include <vector>

namespace Arrivals {

// Observer-centered gnomonic tangent-plane coordinates, the imaging observable:
//   (u_ang, v_ang) = (tan θ_right, tan θ_up)
//                = (s·right / den, s·up / den)
// s = −normalize(world_direction) is the incoming sky direction;
// den = s·forward. Reject den <= 0 (behind the camera), non-arrivals, and
// zero/non-finite direction.
std::optional<Eigen::Vector2d> observer_angular_coordinates(const RayArrival& arrival,
                                                            const Geometry::Observer& observer);

// 1D path: rotate one signed equatorial u_ang about the optical axis.
// (ρ cos ψ, ρ sin ψ) with ρ = signed_u_ang. Preserves sign at k = 0.
// If ρ == 0, a single origin point (not azimuth_count copies).
// Throws std::invalid_argument if azimuth_count < 1.
std::vector<Eigen::Vector2d> expand_angular_azimuthally(double signed_u_ang, int azimuth_count);

// 2D on-axis: isolated Newton hits lie on a circle that a coarse grid cannot
// fill. Take the median Euclidean norm of refined_angular, then
// expand_angular_azimuthally(median, azimuth_count).
// 2D off-axis: return refined hits unchanged (no azimuthal copy).
//
// `observer_distance` is the CLI transverse offset, compared with exact != 0.0.
// It is not PropagationProblem::observer_distance() (lens–observer length).
// Empty input is returned unchanged.
std::vector<Eigen::Vector2d>
fill_aligned_observer_ring(const std::vector<Eigen::Vector2d>& refined_angular,
                           double observer_distance, int azimuth_count);

} // namespace Arrivals

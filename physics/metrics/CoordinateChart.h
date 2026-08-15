#pragma once
#include <core/GeodesicState.h>
#include <Eigen/Dense>

namespace CoordinateChart {

// Spherical ↔ chart-Cartesian maps for State (position X and four-velocity U).
// Schwarzschild spherical: X = (t, r, θ, φ). Chart Cartesian: X = (t, x, y, z).
// This is chart-Cartesian, not the world frame. WorldFrame permutes axes so the
// aligned optical axis sits on the equator θ=π/2, away from the polar pole of Γ^φ_θφ.
// Velocity is mapped with Jacobians ∂x_cart/∂x_sph (and the inverse), not by converting
// U as if it were a Euclidean 3-vector. General GR infrastructure, not SGL geometry.
Eigen::Matrix4d sph_to_cart_Jacobian(double r, double theta, double phi);
Eigen::Matrix4d cart_to_sph_Jacobian(double r, double theta, double phi);
State cart_to_sphere(const State& cartState);
// Non-const ref is historical: this function does not modify sphState.
State sph_to_cart(State& sphState);

} // namespace CoordinateChart

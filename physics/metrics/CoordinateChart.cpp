#include "CoordinateChart.h"
#include <algorithm>
#include <cmath>

namespace CoordinateChart {

Eigen::Matrix4d sph_to_cart_Jacobian(double r, double theta, double phi) {
    // ∂(t,x,y,z)/∂(t,r,θ,φ). Maps spherical U to chart-Cartesian U: U_cart = J U_sph.
    // t is invariant. Spatial block is the usual spherical-coordinate differential.
    Eigen::Matrix4d J = Eigen::Matrix4d::Zero();
    J(0, 0) = 1.0;

    J(1, 1) = std::sin(theta) * std::cos(phi);
    J(1, 2) = r * std::cos(theta) * std::cos(phi);
    J(1, 3) = -r * std::sin(theta) * std::sin(phi);

    J(2, 1) = std::sin(theta) * std::sin(phi);
    J(2, 2) = r * std::cos(theta) * std::sin(phi);
    J(2, 3) = r * std::sin(theta) * std::cos(phi);

    J(3, 1) = std::cos(theta);
    J(3, 2) = -r * std::sin(theta);
    J(3, 3) = 0.0;
    return J;
}

Eigen::Matrix4d cart_to_sph_Jacobian(double r, double theta, double phi) {
    // Inverse map U_sph = J^{-1} U_cart. Singular at r = 0 and on the polar axis (sinθ = 0).
    return sph_to_cart_Jacobian(r, theta, phi).inverse();
}

State cart_to_sphere(const State& cartState) {
    double t = cartState.X[0];
    double x = cartState.X[1];
    double y = cartState.X[2];
    double z = cartState.X[3];

    double r = std::sqrt(x * x + y * y + z * z);
    double phi = std::atan2(y, x); // full-range azimuth, including the negative-x half-plane
    // 1e-8 floor keeps z/r defined at the origin; clamp keeps acos in [-1, 1].
    double theta = std::acos(std::clamp(z / (r + 1e-8), -1.0, 1.0));

    return State(Eigen::Vector4d(t, r, theta, phi), cart_to_sph_Jacobian(r, theta, phi) * cartState.U);
}

State sph_to_cart(State& sphState) {
    // Reads sphState; does not write it (non-const signature is unused).
    double t = sphState.X[0];
    double r = sphState.X[1];
    double theta = sphState.X[2];
    double phi = sphState.X[3];

    double x = r * std::cos(phi) * std::sin(theta);
    double y = r * std::sin(phi) * std::sin(theta);
    double z = r * std::cos(theta);

    return State(Eigen::Vector4d(t, x, y, z), sph_to_cart_Jacobian(r, theta, phi) * sphState.U);
}

} // namespace CoordinateChart

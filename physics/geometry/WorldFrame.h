#pragma once

#include <Eigen/Dense>

namespace Geometry {

inline constexpr double kOrthonormalityTolerance = 1e-12;

// Shared SGL world/chart convention used by Lens, Observer, ImagePlane,
// RaySampler, RayGrid2DSampler, and ChartMapping. Geometry-only: no geodesic types.
//
// World frame: right-handed Cartesian (X, Y, Z) with X x Y = Z.
// Optical axis = +Z. Lens at origin, source at (0, 0, -S).
// Aligned observer at (0, 0, +D); off-axis observer at D·Z + d·X
// (d = 0 Einstein ring; d ≠ 0 arcs). Axes do not change with d.
// Observer looks toward -Z; image-plane basis (u, v, normal) is right-handed with
// normal = u x v = +Z (along incoming light). Camera triple (right, up, -forward) is
// right-handed.
//
// Chart frame (for Phase 3 handoff to CoordinateChart): x_chart = +Z_world (optical axis),
// y_chart = +X_world (image-plane u), z_chart = +Y_world (chart polar axis). This places
// the canonical aligned configuration on the chart equator (theta = pi/2), avoiding the
// polar singularity at theta in {0, pi}.
//
// Residual constraint (Phase 3): a ray whose orbital plane contains world +Y (e.g. impact
// parameter purely along the image-plane v axis) still passes through theta in {0, pi}.
// Phase 3 must rotate each ray's orbital plane into the chart equator (Schwarzschild
// spherical symmetry permits this).

namespace WorldFrame {

inline Eigen::Vector3d optical_axis() { return Eigen::Vector3d::UnitZ(); }
inline Eigen::Vector3d plane_u_axis() { return Eigen::Vector3d::UnitX(); }
inline Eigen::Vector3d plane_v_axis() { return Eigen::Vector3d::UnitY(); }

// Axis permutation (x_c, y_c, z_c) = (Z, X, Y); not a Lorentz transform.
inline Eigen::Matrix3d world_to_chart_rotation() {
    Eigen::Matrix3d r;
    r << 0.0, 0.0, 1.0,
         1.0, 0.0, 0.0,
         0.0, 1.0, 0.0;
    return r;
}

inline Eigen::Matrix3d chart_to_world_rotation() { return world_to_chart_rotation().transpose(); }

inline Eigen::Vector3d world_to_chart(const Eigen::Vector3d& v) {
    return world_to_chart_rotation() * v;
}

inline Eigen::Vector3d chart_to_world(const Eigen::Vector3d& v) {
    return chart_to_world_rotation() * v;
}

} // namespace WorldFrame
} // namespace Geometry

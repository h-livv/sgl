#pragma once

#include "Observer.h"

#include <Eigen/Dense>

namespace Geometry {

// Detector plane attached to the observer. Origin = observer position;
// u = right, v = up, normal = u × v = +Z when aligned (incoming-light / +Z).
// signed_distance >= 0 means a world event has reached or passed the plane
// (PlaneCrossingTermination). (u, v) = (0, 0) at the observer is the
// observer-hit residual, not an aperture.
class ImagePlane {
public:
    ImagePlane(const Eigen::Vector3d& origin, const Eigen::Vector3d& u, const Eigen::Vector3d& v,
               double half_width, double half_height);

    // origin = observer.position, u = observer.right(), v = observer.up().
    // Canonical aligned: u = +X, v = +Y, normal = +Z.
    static ImagePlane attached_to(const Observer& observer, double half_width, double half_height);

    const Eigen::Vector3d& origin() const { return origin_; }
    const Eigen::Vector3d& u_axis() const { return u_; }
    const Eigen::Vector3d& v_axis() const { return v_; }
    const Eigen::Vector3d& normal() const { return normal_; }
    double half_width() const { return half_width_; }
    double half_height() const { return half_height_; }

    // In-plane residual (Δ·u, Δ·v). Zero at the observer — the F=0 hit condition.
    Eigen::Vector2d to_plane_coordinates(const Eigen::Vector3d& world_point) const;
    Eigen::Vector3d to_world(const Eigen::Vector2d& plane_point) const;
    // (world − origin) · normal. Positive on the +normal side (past the plane along +Z).
    double signed_distance(const Eigen::Vector3d& world_point) const;
    // Rectangle |u| <= half_width AND |v| <= half_height. Imaging does not use
    // this as an observer-hit acceptance test.
    bool contains(const Eigen::Vector2d& plane_point) const;

private:
    Eigen::Vector3d origin_;
    Eigen::Vector3d u_;
    Eigen::Vector3d v_;
    Eigen::Vector3d normal_;
    double half_width_;
    double half_height_;
};

} // namespace Geometry

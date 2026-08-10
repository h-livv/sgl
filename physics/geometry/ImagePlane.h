#pragma once

#include "Observer.h"

#include <Eigen/Dense>

namespace Geometry {

class ImagePlane {
public:
    ImagePlane(const Eigen::Vector3d& origin, const Eigen::Vector3d& u, const Eigen::Vector3d& v,
               double half_width, double half_height);

    static ImagePlane attached_to(const Observer& observer, double half_width, double half_height);

    const Eigen::Vector3d& origin() const { return origin_; }
    const Eigen::Vector3d& u_axis() const { return u_; }
    const Eigen::Vector3d& v_axis() const { return v_; }
    const Eigen::Vector3d& normal() const { return normal_; }
    double half_width() const { return half_width_; }
    double half_height() const { return half_height_; }

    Eigen::Vector2d to_plane_coordinates(const Eigen::Vector3d& world_point) const;
    Eigen::Vector3d to_world(const Eigen::Vector2d& plane_point) const;
    double signed_distance(const Eigen::Vector3d& world_point) const;
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

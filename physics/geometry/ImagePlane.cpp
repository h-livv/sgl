#include "ImagePlane.h"

#include <cmath>
#include <stdexcept>

namespace Geometry {
namespace {

bool is_finite(const Eigen::Vector3d& v) {
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}

} // namespace

ImagePlane::ImagePlane(const Eigen::Vector3d& origin, const Eigen::Vector3d& u,
                       const Eigen::Vector3d& v, double half_width, double half_height)
    : origin_(origin), u_(u), v_(v), normal_(u.cross(v)), half_width_(half_width),
      half_height_(half_height) {
    if (!is_finite(origin_) || !is_finite(u_) || !is_finite(v_)) {
        throw std::invalid_argument("ImagePlane: origin, u, and v must be finite");
    }
    if (!std::isfinite(half_width_) || !std::isfinite(half_height_)) {
        throw std::invalid_argument("ImagePlane: half_width and half_height must be finite");
    }
    if (std::abs(u_.norm() - 1.0) > kOrthonormalityTolerance) {
        throw std::invalid_argument("ImagePlane: u must be a unit vector");
    }
    if (std::abs(v_.norm() - 1.0) > kOrthonormalityTolerance) {
        throw std::invalid_argument("ImagePlane: v must be a unit vector");
    }
    if (std::abs(u_.dot(v_)) > kOrthonormalityTolerance) {
        throw std::invalid_argument("ImagePlane: u and v must be orthogonal");
    }
    if (half_width_ <= 0.0) {
        throw std::invalid_argument("ImagePlane: half_width must be positive");
    }
    if (half_height_ <= 0.0) {
        throw std::invalid_argument("ImagePlane: half_height must be positive");
    }
}

ImagePlane ImagePlane::attached_to(const Observer& observer, double half_width,
                                   double half_height) {
    return ImagePlane(observer.position(), observer.right(), observer.up(), half_width,
                      half_height);
}

Eigen::Vector2d ImagePlane::to_plane_coordinates(const Eigen::Vector3d& world_point) const {
    const Eigen::Vector3d delta = world_point - origin_;
    return Eigen::Vector2d(delta.dot(u_), delta.dot(v_));
}

Eigen::Vector3d ImagePlane::to_world(const Eigen::Vector2d& plane_point) const {
    return origin_ + plane_point.x() * u_ + plane_point.y() * v_;
}

double ImagePlane::signed_distance(const Eigen::Vector3d& world_point) const {
    return (world_point - origin_).dot(normal_);
}

bool ImagePlane::contains(const Eigen::Vector2d& plane_point) const {
    return std::abs(plane_point.x()) <= half_width_ && std::abs(plane_point.y()) <= half_height_;
}

} // namespace Geometry

#include "Observer.h"

#include <cmath>
#include <stdexcept>

namespace Geometry {
namespace {

bool is_finite(const Eigen::Vector3d& v) {
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}

} // namespace

Observer::Observer(const Eigen::Vector3d& position, const Eigen::Vector3d& forward,
                   const Eigen::Vector3d& up)
    : position_(position), forward_(forward), up_(up) {
    if (!is_finite(position_) || !is_finite(forward_) || !is_finite(up_)) {
        throw std::invalid_argument("Observer: position, forward, and up must be finite");
    }
    if (std::abs(forward_.norm() - 1.0) > kOrthonormalityTolerance) {
        throw std::invalid_argument("Observer: forward must be a unit vector");
    }
    if (std::abs(up_.norm() - 1.0) > kOrthonormalityTolerance) {
        throw std::invalid_argument("Observer: up must be a unit vector");
    }
    if (std::abs(forward_.dot(up_)) > kOrthonormalityTolerance) {
        throw std::invalid_argument("Observer: forward and up must be orthogonal");
    }
}

Observer Observer::looking_at(const Eigen::Vector3d& position, const Eigen::Vector3d& target,
                              const Eigen::Vector3d& up_hint) {
    if (!is_finite(position) || !is_finite(target) || !is_finite(up_hint)) {
        throw std::invalid_argument("Observer::looking_at: inputs must be finite");
    }
    const Eigen::Vector3d to_target = target - position;
    if (to_target.norm() <= kOrthonormalityTolerance) {
        throw std::invalid_argument("Observer::looking_at: target must differ from position");
    }
    const Eigen::Vector3d forward = to_target.normalized();
    const Eigen::Vector3d up_residual = up_hint - up_hint.dot(forward) * forward;
    if (up_residual.norm() <= kOrthonormalityTolerance) {
        throw std::invalid_argument(
            "Observer::looking_at: up_hint must not be parallel to view direction");
    }
    const Eigen::Vector3d up = up_residual.normalized();
    return Observer(position, forward, up);
}

} // namespace Geometry

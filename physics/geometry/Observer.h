#pragma once

#include "WorldFrame.h"

#include <Eigen/Dense>

namespace Geometry {

// Spacecraft/camera pose in the world frame. Canonical SGL attitude looks at
// the lens (on-axis: forward = −Z). ImagePlane::attached_to takes u = right(),
// v = up(), so this basis is the detector frame.
class Observer {
public:
    // Requires already-unit, already-orthogonal forward and up. Does not repair.
    Observer(const Eigen::Vector3d& position, const Eigen::Vector3d& forward,
             const Eigen::Vector3d& up);

    // forward = normalize(target − position); up = Gram-Schmidt of up_hint
    // against forward. Canonical: target = origin, up_hint = +Y
    // → forward = −Z, up = +Y, right = +X.
    static Observer looking_at(const Eigen::Vector3d& position, const Eigen::Vector3d& target,
                               const Eigen::Vector3d& up_hint);

    const Eigen::Vector3d& position() const { return position_; }
    const Eigen::Vector3d& forward() const { return forward_; }
    const Eigen::Vector3d& up() const { return up_; }
    // Camera +X / image-plane u. Not stored: right = forward × up.
    Eigen::Vector3d right() const { return forward_.cross(up_); }

private:
    Eigen::Vector3d position_;
    Eigen::Vector3d forward_;
    Eigen::Vector3d up_;
};

} // namespace Geometry

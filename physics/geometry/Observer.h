#pragma once

#include "WorldFrame.h"

#include <Eigen/Dense>

namespace Geometry {

class Observer {
public:
    Observer(const Eigen::Vector3d& position, const Eigen::Vector3d& forward,
             const Eigen::Vector3d& up);

    static Observer looking_at(const Eigen::Vector3d& position, const Eigen::Vector3d& target,
                               const Eigen::Vector3d& up_hint);

    const Eigen::Vector3d& position() const { return position_; }
    const Eigen::Vector3d& forward() const { return forward_; }
    const Eigen::Vector3d& up() const { return up_; }
    Eigen::Vector3d right() const { return forward_.cross(up_); }

private:
    Eigen::Vector3d position_;
    Eigen::Vector3d forward_;
    Eigen::Vector3d up_;
};

} // namespace Geometry

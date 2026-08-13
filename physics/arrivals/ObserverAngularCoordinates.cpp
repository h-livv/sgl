#include "ObserverAngularCoordinates.h"

#include <cmath>
#include <stdexcept>

namespace Arrivals {

std::optional<Eigen::Vector2d>
observer_angular_coordinates(const RayArrival& arrival, const Geometry::Observer& observer) {
    if (arrival.status != ArrivalStatus::Arrived) {
        return std::nullopt;
    }

    const double direction_norm = arrival.world_direction.norm();
    if (direction_norm == 0.0 || !std::isfinite(direction_norm)) {
        return std::nullopt;
    }

    const Eigen::Vector3d view = -arrival.world_direction / direction_norm;
    const double den = view.dot(observer.forward());
    if (den <= 0.0) {
        return std::nullopt;
    }

    const Eigen::Vector2d angular(view.dot(observer.right()) / den,
                                  view.dot(observer.up()) / den);
    if (!std::isfinite(angular.x()) || !std::isfinite(angular.y())) {
        return std::nullopt;
    }
    return angular;
}

std::vector<Eigen::Vector2d> expand_angular_azimuthally(double signed_u_ang, int azimuth_count) {
    if (azimuth_count < 1) {
        throw std::invalid_argument("expand_angular_azimuthally: azimuth_count must be >= 1");
    }

    std::vector<Eigen::Vector2d> expanded;
    expanded.reserve(static_cast<std::size_t>(azimuth_count));

    if (signed_u_ang == 0.0) {
        expanded.push_back(Eigen::Vector2d::Zero());
        return expanded;
    }

    for (int k = 0; k < azimuth_count; ++k) {
        const double psi =
            2.0 * M_PI * static_cast<double>(k) / static_cast<double>(azimuth_count);
        expanded.push_back(
            Eigen::Vector2d(signed_u_ang * std::cos(psi), signed_u_ang * std::sin(psi)));
    }
    return expanded;
}

} // namespace Arrivals

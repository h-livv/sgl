#include "AzimuthalExpansion.h"

#include <cmath>
#include <stdexcept>

namespace Arrivals {

std::vector<PlaneArrival> expand_azimuthally(const std::vector<RayArrival>& arrivals,
                                             const Geometry::ImagePlane& plane,
                                             int azimuth_count) {
    if (azimuth_count < 1) {
        throw std::invalid_argument("expand_azimuthally: azimuth_count must be >= 1");
    }

    std::vector<PlaneArrival> expanded;
    expanded.reserve(arrivals.size() * static_cast<std::size_t>(azimuth_count));

    for (const RayArrival& arrival : arrivals) {
        if (arrival.status != ArrivalStatus::Arrived) {
            continue;  // not index-aligned: misses are dropped, not padded
        }
        const double u = plane.to_plane_coordinates(arrival.world_position).x();
        if (u == 0.0) {
            expanded.push_back(PlaneArrival{arrival.ray_id, Eigen::Vector2d::Zero()});
            continue;  // on-axis foot: one sample, v discarded
        }
        for (int k = 0; k < azimuth_count; ++k) {
            const double psi = 2.0 * M_PI * static_cast<double>(k) /
                               static_cast<double>(azimuth_count);
            expanded.push_back(PlaneArrival{
                arrival.ray_id, Eigen::Vector2d(u * std::cos(psi), u * std::sin(psi))});
        }
    }
    return expanded;
}

} // namespace Arrivals

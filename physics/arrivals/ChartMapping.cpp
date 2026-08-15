#include "ChartMapping.h"

#include <geometry/WorldFrame.h>
#include <metrics/CoordinateChart.h>

#include <cmath>

namespace Arrivals {

Eigen::Vector3d world_position(const Geometry::Lens& lens, const State& chart_state) {
    const double r = chart_state.X[1];
    const double theta = chart_state.X[2];
    const double phi = chart_state.X[3];
    // Standard spherical embedding; translation/rotation is from_chart_frame.
    const Eigen::Vector3d chart_point(r * std::cos(phi) * std::sin(theta),
                                      r * std::sin(phi) * std::sin(theta),
                                      r * std::cos(theta));
    return Geometry::from_chart_frame(lens, chart_point);
}

Eigen::Vector3d world_direction(const Geometry::Lens& lens, const State& chart_state) {
    (void)lens;  // API mirrors world_position; translation does not act on U.
    State spherical = chart_state;
    const State cartesian = CoordinateChart::sph_to_cart(spherical);  // Jacobian on U
    const Eigen::Vector3d chart_velocity(cartesian.U[1], cartesian.U[2], cartesian.U[3]);
    const Eigen::Vector3d world_velocity = Geometry::WorldFrame::chart_to_world(chart_velocity);
    const double norm = world_velocity.norm();
    if (norm == 0.0) {
        return Eigen::Vector3d::Zero();
    }
    return world_velocity / norm;
}

} // namespace Arrivals

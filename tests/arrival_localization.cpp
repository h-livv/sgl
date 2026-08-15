#include "support/Check.h"

#include <arrivals/ArrivalCollector.h>
#include <arrivals/ChartMapping.h>
#include <geometry/Lens.h>
#include <geometry/WorldFrame.h>
#include <metrics/CoordinateChart.h>
#include <problem/PropagationProblem.h>

#include <cmath>

// Arrivals: localize_arrival interpolates the last integrator segment onto the plane.
// Contract: a sign-changing axial/off-axis step lands on z = D; receding or
//           short-of-plane segments are NoCrossing; ray_id is preserved either way.
// Pipeline: arrivals. collect_arrivals wraps this after plane-crossing termination.
// Caveat: if previous == final already past the plane, status is Arrived at that
//         state (t = 0); there is no back-projection onto the plane.

namespace {

State chart_state_at(const Geometry::Lens& lens, const Eigen::Vector3d& world_point,
                     const Eigen::Vector3d& world_velocity) {
    const Eigen::Vector3d c = Geometry::to_chart_frame(lens, world_point);
    const Eigen::Vector3d v = Geometry::WorldFrame::world_to_chart(world_velocity);
    const State cartesian(Eigen::Vector4d(0.0, c.x(), c.y(), c.z()),
                          Eigen::Vector4d(1.0, v.x(), v.y(), v.z()));
    return CoordinateChart::cart_to_sphere(cartesian);
}

Propagation::PropagationOutcome make_outcome(const State& previous, const State& final_state,
                                             int steps_taken) {
    Propagation::PropagationOutcome outcome;
    outcome.previous_state = previous;
    outcome.final_state = final_state;
    outcome.steps_taken = steps_taken;
    outcome.status = Propagation::PropagationStatus::Terminated;
    return outcome;
}

bool vectors_close(const Eigen::Vector3d& actual, const Eigen::Vector3d& expected,
                   double rel_tol) {
    return TestSupport::close_rel(actual.x(), expected.x(), rel_tol) &&
           TestSupport::close_rel(actual.y(), expected.y(), rel_tol) &&
           TestSupport::close_rel(actual.z(), expected.z(), rel_tol);
}

} // namespace

int main() {
    const Spacetime::SchwarzschildParameters params{.rs = 1.0};
    const Problem::PropagationProblem problem =
        Problem::make_aligned_problem(params, 30.0, 30.0, 10.0, 10.0);
    const Geometry::Lens& lens = problem.lens();
    const Geometry::ImagePlane& plane = problem.image_plane();

    const Eigen::Vector3d forward(0.0, 0.0, 1.0);
    // Last step straddles z = 30: linear interpolate in world coordinates, not a geodesic root.
    const State axial_prev =
        chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 29.5), forward);
    const State axial_curr =
        chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 30.5), forward);
    const Arrivals::RayArrival axial = Arrivals::localize_arrival(
        0, lens, plane, make_outcome(axial_prev, axial_curr, 1));

    CHECK(axial.status == Arrivals::ArrivalStatus::Arrived, "axial arrival status");
    CHECK(vectors_close(axial.world_position, Eigen::Vector3d(0.0, 0.0, 30.0), 1e-9),
          "axial localized position");
    CHECK(std::abs(plane.signed_distance(axial.world_position)) <=
              1e-9 * std::max(1.0, axial.world_position.norm()),
          "axial position lies on the plane");

    // Endpoints (1,0,29.5) → (3,0,30.5): hit must be the interpolated (2,0,30), not either state.
    const State off_prev = chart_state_at(lens, Eigen::Vector3d(1.0, 0.0, 29.5), forward);
    const State off_curr = chart_state_at(lens, Eigen::Vector3d(3.0, 0.0, 30.5), forward);
    const Arrivals::RayArrival off_axis = Arrivals::localize_arrival(
        1, lens, plane, make_outcome(off_prev, off_curr, 1));

    CHECK(off_axis.status == Arrivals::ArrivalStatus::Arrived, "off-axis arrival status");
    CHECK(vectors_close(off_axis.world_position, Eigen::Vector3d(2.0, 0.0, 30.0), 1e-9),
          "off-axis localized position");
    CHECK(std::abs(plane.signed_distance(off_axis.world_position)) <= 1e-9,
          "off-axis position lies on the plane");
    CHECK(!vectors_close(off_axis.world_position,
                         Arrivals::world_position(lens, off_curr), 1e-3),
          "off-axis result is not the final integration state");
    CHECK(!vectors_close(off_axis.world_position,
                         Arrivals::world_position(lens, off_prev), 1e-3),
          "off-axis result is not the previous integration state");

    CHECK(vectors_close(axial.world_direction, forward, 1e-9), "axial arrival direction");
    CHECK_CLOSE(axial.world_direction.norm(), 1.0, 1e-12, "axial direction is unit");
    CHECK(axial.world_direction.dot(plane.normal()) > 0.0,
          "axial direction crosses in the physically relevant direction");
    CHECK_CLOSE(axial.chart_state.X[2], M_PI / 2.0, 1e-12, "axial chart theta");
    CHECK_CLOSE(axial.chart_state.X[1], 30.0, 1e-9, "axial chart radius");

    const State short_prev = chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 9.0), forward);
    const State short_curr = chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 10.0), forward);
    const Arrivals::RayArrival short_of_plane = Arrivals::localize_arrival(
        2, lens, plane, make_outcome(short_prev, short_curr, 1));
    CHECK(short_of_plane.status == Arrivals::ArrivalStatus::NoCrossing,
          "short-of-plane status");
    CHECK(vectors_close(short_of_plane.world_position, Eigen::Vector3d::Zero(), 1e-15),
          "short-of-plane position zeroed");
    CHECK(vectors_close(short_of_plane.world_direction, Eigen::Vector3d::Zero(), 1e-15),
          "short-of-plane direction zeroed");
    CHECK(short_of_plane.ray_id == 2, "short-of-plane ray id preserved");

    const State recede_prev = chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 10.0), forward);
    const State recede_curr = chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 9.0), forward);
    // Moving away from the plane (both samples still on the lens side) is not a crossing.
    const Arrivals::RayArrival receding = Arrivals::localize_arrival(
        3, lens, plane, make_outcome(recede_prev, recede_curr, 1));
    CHECK(receding.status == Arrivals::ArrivalStatus::NoCrossing, "receding status");

    const State on_plane = chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 30.0), forward);
    const Arrivals::RayArrival on_plane_arrival = Arrivals::localize_arrival(
        4, lens, plane, make_outcome(on_plane, on_plane, 0));
    CHECK(on_plane_arrival.status == Arrivals::ArrivalStatus::Arrived, "on-plane status");
    CHECK(vectors_close(on_plane_arrival.world_position, Eigen::Vector3d(0.0, 0.0, 30.0), 1e-9),
          "on-plane position");
    CHECK(std::abs(plane.signed_distance(on_plane_arrival.world_position)) <= 1e-12,
          "on-plane signed distance");

    const State beyond_plane = chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 31.0), forward);
    // previous == final already on the observer side: Arrived at z = 31, not snapped to the plane.
    const Arrivals::RayArrival beyond = Arrivals::localize_arrival(
        5, lens, plane, make_outcome(beyond_plane, beyond_plane, 0));
    CHECK(beyond.status == Arrivals::ArrivalStatus::Arrived, "beyond-plane status");
    CHECK(vectors_close(beyond.world_position, Eigen::Vector3d(0.0, 0.0, 31.0), 1e-9),
          "beyond-plane position");

    const State boundary_prev = chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 29.5), forward);
    const State boundary_curr = chart_state_at(lens, Eigen::Vector3d(0.0, 0.0, 30.0), forward);
    const Arrivals::RayArrival boundary = Arrivals::localize_arrival(
        6, lens, plane, make_outcome(boundary_prev, boundary_curr, 1));
    CHECK(vectors_close(boundary.world_position, Eigen::Vector3d(0.0, 0.0, 30.0), 1e-9),
          "boundary t=1 localization");

    const Arrivals::RayArrival id_arrived = Arrivals::localize_arrival(
        7, lens, plane, make_outcome(axial_prev, axial_curr, 1));
    CHECK(id_arrived.ray_id == 7, "arrived ray id");
    const Arrivals::RayArrival id_missing = Arrivals::localize_arrival(
        7, lens, plane, make_outcome(short_prev, short_curr, 1));
    CHECK(id_missing.ray_id == 7, "non-arrived ray id");

    return TestSupport::report();
}

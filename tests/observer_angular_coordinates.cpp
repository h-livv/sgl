#include "support/Check.h"

#include <arrivals/ObserverAngularCoordinates.h>
#include <arrivals/RayArrival.h>
#include <geometry/Observer.h>
#include <geometry/WorldFrame.h>

#include <cmath>
#include <stdexcept>

namespace {

Arrivals::RayArrival make_arrival(const Eigen::Vector3d& world_direction) {
    Arrivals::RayArrival arrival;
    arrival.status = Arrivals::ArrivalStatus::Arrived;
    arrival.world_direction = world_direction;
    return arrival;
}

} // namespace

int main() {
    const Geometry::Observer observer = Geometry::Observer::looking_at(
        Eigen::Vector3d(0.0, 0.0, 30.0), Eigen::Vector3d::Zero(),
        Geometry::WorldFrame::plane_v_axis());

    const auto central =
        Arrivals::observer_angular_coordinates(make_arrival(Eigen::Vector3d(0.0, 0.0, 1.0)),
                                               observer);
    CHECK(central.has_value(), "central incoming photon maps");
    CHECK_CLOSE(central->x(), 0.0, 1e-15, "central u");
    CHECK_CLOSE(central->y(), 0.0, 1e-15, "central v");

    constexpr double rho = 0.1;
    const Eigen::Vector3d view_right =
        (observer.forward() + rho * observer.right()).normalized();
    const auto right =
        Arrivals::observer_angular_coordinates(make_arrival(-view_right), observer);
    CHECK(right.has_value(), "right-side photon maps");
    CHECK_CLOSE(right->x(), rho, 1e-12, "right u");
    CHECK_CLOSE(right->y(), 0.0, 1e-12, "right v");

    const Eigen::Vector3d view_up = (observer.forward() + rho * observer.up()).normalized();
    const auto up = Arrivals::observer_angular_coordinates(make_arrival(-view_up), observer);
    CHECK(up.has_value(), "up-side photon maps");
    CHECK_CLOSE(up->x(), 0.0, 1e-12, "up u");
    CHECK_CLOSE(up->y(), rho, 1e-12, "up v");

    const auto behind =
        Arrivals::observer_angular_coordinates(make_arrival(Eigen::Vector3d(0.0, 0.0, -1.0)),
                                               observer);
    CHECK(!behind.has_value(), "behind observer rejected");

    Arrivals::RayArrival no_crossing;
    no_crossing.status = Arrivals::ArrivalStatus::NoCrossing;
    no_crossing.world_direction = Eigen::Vector3d(0.0, 0.0, 1.0);
    CHECK(!Arrivals::observer_angular_coordinates(no_crossing, observer).has_value(),
          "non-arrival rejected");

    const std::vector<Eigen::Vector2d> expanded =
        Arrivals::expand_angular_azimuthally(0.2, 4);
    CHECK(expanded.size() == 4, "four angular expansion samples");
    CHECK_CLOSE(expanded[0].x(), 0.2, 1e-15, "expansion k=0 u");
    CHECK_CLOSE(expanded[0].y(), 0.0, 1e-15, "expansion k=0 v");
    CHECK_CLOSE(expanded[1].x(), 0.0, 1e-15, "expansion k=1 u");
    CHECK_CLOSE(expanded[1].y(), 0.2, 1e-15, "expansion k=1 v");
    CHECK_CLOSE(expanded[2].x(), -0.2, 1e-15, "expansion k=2 u");
    CHECK_CLOSE(expanded[2].y(), 0.0, 1e-15, "expansion k=2 v");
    CHECK_CLOSE(expanded[3].x(), 0.0, 1e-15, "expansion k=3 u");
    CHECK_CLOSE(expanded[3].y(), -0.2, 1e-15, "expansion k=3 v");

    try {
        (void)Arrivals::expand_angular_azimuthally(0.2, 0);
        CHECK(false, "expected invalid_argument for azimuth_count < 1");
    } catch (const std::invalid_argument&) {
    }

    const std::vector<Eigen::Vector2d> two_hits = {Eigen::Vector2d(0.3, 0.0),
                                                   Eigen::Vector2d(-0.3, 0.0)};
    const std::vector<Eigen::Vector2d> on_axis =
        Arrivals::fill_aligned_observer_ring(two_hits, 0.0, 8);
    CHECK(on_axis.size() == 8, "on-axis fill uses azimuth_count");
    CHECK_CLOSE(on_axis[0].x(), 0.3, 1e-15, "on-axis fill k=0 u");
    CHECK_CLOSE(on_axis[0].y(), 0.0, 1e-15, "on-axis fill k=0 v");
    CHECK_CLOSE(on_axis[2].x(), 0.0, 1e-15, "on-axis fill k=2 u");
    CHECK_CLOSE(on_axis[2].y(), 0.3, 1e-15, "on-axis fill k=2 v");

    const std::vector<Eigen::Vector2d> off_axis =
        Arrivals::fill_aligned_observer_ring(two_hits, 1.0, 8);
    CHECK(off_axis.size() == 2, "off-axis keeps isolated hits");
    CHECK_CLOSE(off_axis[0].x(), 0.3, 1e-15, "off-axis first u");
    CHECK_CLOSE(off_axis[1].x(), -0.3, 1e-15, "off-axis second u");

    return TestSupport::report();
}

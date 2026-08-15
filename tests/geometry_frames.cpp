#include "support/Check.h"

#include <geometry/Observer.h>
#include <geometry/WorldFrame.h>

#include <cmath>
#include <limits>

// Geometry: WorldFrame chart permutation and Observer orthonormal frames.
// Contract: world (X,Y,Z) → chart (Z,X,Y) is a rotation (det +1); aligned
//           world X-Z points sit on the chart equator (θ = π/2); looking_at
//           builds a right-handed (right, up, -forward) camera triple.
// Pipeline: geometry (sgl_geometry), before rays/arrivals.
// Caveat: the permutation is not a Lorentz transform. It exists so the
//         optical axis is chart-x and canonical rays avoid the polar singularity.

namespace {

bool vectors_close(const Eigen::Vector3d& actual, const Eigen::Vector3d& expected,
                   double rel_tol) {
    return TestSupport::close_rel(actual.x(), expected.x(), rel_tol) &&
           TestSupport::close_rel(actual.y(), expected.y(), rel_tol) &&
           TestSupport::close_rel(actual.z(), expected.z(), rel_tol);
}

bool matrix_close(const Eigen::Matrix3d& actual, const Eigen::Matrix3d& expected,
                  double rel_tol) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (!TestSupport::close_rel(actual(i, j), expected(i, j), rel_tol)) {
                return false;
            }
        }
    }
    return true;
}

void expect_invalid_observer(const Eigen::Vector3d& position, const Eigen::Vector3d& forward,
                             const Eigen::Vector3d& up) {
    try {
        (void)Geometry::Observer(position, forward, up);
        CHECK(false, "expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    }
}

void expect_invalid_looking_at(const Eigen::Vector3d& position, const Eigen::Vector3d& target,
                               const Eigen::Vector3d& up_hint) {
    try {
        (void)Geometry::Observer::looking_at(position, target, up_hint);
        CHECK(false, "expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    }
}

} // namespace

int main() {
    CHECK(vectors_close(Geometry::WorldFrame::optical_axis(), Eigen::Vector3d(0, 0, 1), 1e-15),
          "optical axis");
    CHECK(vectors_close(Geometry::WorldFrame::plane_u_axis(), Eigen::Vector3d(1, 0, 0), 1e-15),
          "plane u axis");
    CHECK(vectors_close(Geometry::WorldFrame::plane_v_axis(), Eigen::Vector3d(0, 1, 0), 1e-15),
          "plane v axis");
    CHECK(vectors_close(Geometry::WorldFrame::plane_u_axis().cross(Geometry::WorldFrame::plane_v_axis()),
                        Geometry::WorldFrame::optical_axis(), 1e-15),
          "world right-handedness");

    const Eigen::Matrix3d rotation = Geometry::WorldFrame::world_to_chart_rotation();
    const Eigen::Matrix3d identity = Eigen::Matrix3d::Identity();
    CHECK(matrix_close(rotation * rotation.transpose(), identity, 1e-15), "rotation orthogonal");
    CHECK_CLOSE(rotation.determinant(), 1.0, 1e-12, "rotation determinant");

    // Optical axis +Z becomes chart +X (Schwarzschild radial in the aligned problem).
    CHECK(vectors_close(Geometry::WorldFrame::world_to_chart(Eigen::Vector3d(0, 0, 1)),
                        Eigen::Vector3d(1, 0, 0), 1e-15),
          "world +Z to chart");
    CHECK(vectors_close(Geometry::WorldFrame::world_to_chart(Eigen::Vector3d(1, 0, 0)),
                        Eigen::Vector3d(0, 1, 0), 1e-15),
          "world +X to chart");
    CHECK(vectors_close(Geometry::WorldFrame::world_to_chart(Eigen::Vector3d(0, 1, 0)),
                        Eigen::Vector3d(0, 0, 1), 1e-15),
          "world +Y to chart");

    const Eigen::Vector3d samples[] = {
        Eigen::Vector3d(1.0, 2.0, 3.0),
        Eigen::Vector3d(-4.0, 0.5, 7.0),
        Eigen::Vector3d(0.0, -2.0, 5.0),
    };
    for (const auto& p : samples) {
        CHECK(vectors_close(Geometry::WorldFrame::chart_to_world(Geometry::WorldFrame::world_to_chart(p)), p, 1e-15),
              "chart round trip");
    }

    const double D = 50.0;
    const double S = 100.0;
    // World X-Z (the aligned source–lens–observer plane) must map to θ = π/2.
    const Eigen::Vector3d equatorial_points[] = {
        Eigen::Vector3d(0, 0, D),
        Eigen::Vector3d(0, 0, -S),
        Eigen::Vector3d(3.0, 0.0, -7.0),
        Eigen::Vector3d(-2.0, 0.0, 11.0),
    };
    for (const auto& p : equatorial_points) {
        const Eigen::Vector3d chart = Geometry::WorldFrame::world_to_chart(p);
        CHECK_CLOSE(chart.z(), 0.0, 1e-15, "world X-Z plane maps to chart equator");
        const double r = chart.norm();
        if (r > 0.0) {
            CHECK_CLOSE(std::acos(chart.z() / r), M_PI / 2.0, 1e-12, "chart theta is pi/2");
        }
    }

    const Geometry::Observer observer =
        Geometry::Observer::looking_at(Eigen::Vector3d(0, 0, 50), Eigen::Vector3d::Zero(),
                                       Eigen::Vector3d(0, 1, 0));
    CHECK(vectors_close(observer.forward(), Eigen::Vector3d(0, 0, -1), 1e-12), "canonical forward");
    CHECK(vectors_close(observer.up(), Eigen::Vector3d(0, 1, 0), 1e-12), "canonical up");
    CHECK(vectors_close(observer.right(), Eigen::Vector3d(1, 0, 0), 1e-12), "canonical right");
    CHECK(vectors_close(observer.right(), observer.forward().cross(observer.up()), 1e-12),
          "right equals forward cross up");
    CHECK(vectors_close(observer.right().cross(observer.up()), -observer.forward(), 1e-12),
          "camera triple handedness");

    expect_invalid_observer(Eigen::Vector3d::Zero(), Eigen::Vector3d(2, 0, 0),
                            Eigen::Vector3d(0, 1, 0));
    expect_invalid_observer(Eigen::Vector3d::Zero(), Eigen::Vector3d(0, 0, -1),
                            Eigen::Vector3d(0, 2, 0));
    expect_invalid_observer(Eigen::Vector3d::Zero(), Eigen::Vector3d(0, 0, -1),
                            Eigen::Vector3d(0, 0, 1));
    expect_invalid_observer(
        Eigen::Vector3d::Constant(std::numeric_limits<double>::quiet_NaN()),
        Eigen::Vector3d(0, 0, -1), Eigen::Vector3d(0, 1, 0));

    expect_invalid_looking_at(Eigen::Vector3d(0, 0, 1), Eigen::Vector3d(0, 0, 1),
                            Eigen::Vector3d(0, 1, 0));
    expect_invalid_looking_at(Eigen::Vector3d(0, 0, 0), Eigen::Vector3d(0, 0, 1),
                            Eigen::Vector3d(0, 0, 1));

    return TestSupport::report();
}

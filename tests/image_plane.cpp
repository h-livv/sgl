#include "support/Check.h"

#include <geometry/ImagePlane.h>
#include <geometry/Observer.h>

#include <limits>

namespace {

bool vectors_close(const Eigen::Vector3d& actual, const Eigen::Vector3d& expected,
                   double rel_tol) {
    return TestSupport::close_rel(actual.x(), expected.x(), rel_tol) &&
           TestSupport::close_rel(actual.y(), expected.y(), rel_tol) &&
           TestSupport::close_rel(actual.z(), expected.z(), rel_tol);
}

bool plane_coords_close(const Eigen::Vector2d& actual, const Eigen::Vector2d& expected,
                        double rel_tol) {
    return TestSupport::close_rel(actual.x(), expected.x(), rel_tol) &&
           TestSupport::close_rel(actual.y(), expected.y(), rel_tol);
}

void expect_invalid_image_plane(const Eigen::Vector3d& origin, const Eigen::Vector3d& u,
                                const Eigen::Vector3d& v, double half_width,
                                double half_height) {
    try {
        (void)Geometry::ImagePlane(origin, u, v, half_width, half_height);
        CHECK(false, "expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    }
}

} // namespace

int main() {
    const Geometry::Observer observer =
        Geometry::Observer::looking_at(Eigen::Vector3d(0, 0, 50), Eigen::Vector3d::Zero(),
                                       Eigen::Vector3d(0, 1, 0));
    const Geometry::ImagePlane plane = Geometry::ImagePlane::attached_to(observer, 2.0, 1.5);

    CHECK(vectors_close(plane.origin(), Eigen::Vector3d(0, 0, 50), 1e-15), "attached origin");
    CHECK(vectors_close(plane.u_axis(), Eigen::Vector3d(1, 0, 0), 1e-15), "attached u");
    CHECK(vectors_close(plane.v_axis(), Eigen::Vector3d(0, 1, 0), 1e-15), "attached v");
    CHECK(vectors_close(plane.normal(), Eigen::Vector3d(0, 0, 1), 1e-15), "attached normal");
    CHECK(vectors_close(plane.normal(), -observer.forward(), 1e-12), "normal antiparallel forward");
    CHECK_CLOSE(plane.normal().norm(), 1.0, 1e-12, "normal unit length");
    CHECK(vectors_close(plane.normal(), plane.u_axis().cross(plane.v_axis()), 1e-12),
          "normal equals u cross v");

    const double a = 0.7;
    const double b = -1.3;
    const Eigen::Vector3d in_plane_point = plane.origin() + a * plane.u_axis() + b * plane.v_axis();
    CHECK(plane_coords_close(plane.to_plane_coordinates(in_plane_point), Eigen::Vector2d(a, b),
                             1e-15),
          "forward mapping");

    CHECK(vectors_close(plane.to_world(plane.to_plane_coordinates(in_plane_point)), in_plane_point,
                        1e-15),
          "in-plane round trip");

    const Eigen::Vector3d off_plane_point(1.0, 2.0, 40.0);
    const Eigen::Vector2d projected = plane.to_plane_coordinates(off_plane_point);
    const double distance = plane.signed_distance(off_plane_point);
    CHECK(vectors_close(plane.to_world(projected),
                        off_plane_point - distance * plane.normal(), 1e-15),
          "off-plane projection round trip");

    CHECK_CLOSE(plane.signed_distance(plane.origin()), 0.0, 1e-15, "origin distance");
    const double d = 3.5;
    CHECK_CLOSE(plane.signed_distance(plane.origin() + d * plane.normal()), d, 1e-15,
                "positive signed distance");
    CHECK_CLOSE(plane.signed_distance(plane.origin() - d * plane.normal()), -d, 1e-15,
                "negative signed distance");
    CHECK(plane.signed_distance(Eigen::Vector3d(0, 0, 10)) < 0.0,
          "lens-side point has negative distance");

    CHECK(plane.contains(Eigen::Vector2d(0, 0)), "centre contained");
    CHECK(plane.contains(Eigen::Vector2d(plane.half_width(), plane.half_height())),
          "corner ++ contained");
    CHECK(plane.contains(Eigen::Vector2d(-plane.half_width(), plane.half_height())),
          "corner -+ contained");
    CHECK(plane.contains(Eigen::Vector2d(plane.half_width(), -plane.half_height())),
          "corner +- contained");
    CHECK(plane.contains(Eigen::Vector2d(-plane.half_width(), -plane.half_height())),
          "corner -- contained");
    CHECK(!plane.contains(Eigen::Vector2d(plane.half_width() * 1.000001, 0.0)),
          "outside width rejected");
    CHECK(!plane.contains(Eigen::Vector2d(0.0, plane.half_height() * 1.1)),
          "outside height rejected");

    const Geometry::Observer off_axis_observer = Geometry::Observer::looking_at(
        Eigen::Vector3d(1.0, 0.0, 30.0), Eigen::Vector3d::Zero(), Eigen::Vector3d(0, 1, 0));
    const Geometry::ImagePlane off_plane =
        Geometry::ImagePlane::attached_to(off_axis_observer, 2.0, 1.5);
    CHECK(vectors_close(off_plane.origin(), off_axis_observer.position(), 1e-15),
          "off-axis plane origin is the observer");
    CHECK(plane_coords_close(off_plane.to_plane_coordinates(off_axis_observer.position()),
                             Eigen::Vector2d::Zero(), 1e-15),
          "observer-hit residual origin is the observer");
    CHECK_CLOSE(off_plane.signed_distance(off_axis_observer.position()), 0.0, 1e-15,
                "observer lies on its image plane");
    CHECK(off_plane.to_plane_coordinates(Eigen::Vector3d(0.0, 0.0, 30.0)).norm() > 0.1,
          "optical-axis foot is not the off-axis residual origin");
    CHECK(vectors_close(off_plane.normal(), -off_axis_observer.forward(), 1e-12),
          "off-axis plane faces the observer look direction");

    expect_invalid_image_plane(plane.origin(), Eigen::Vector3d(2, 0, 0), plane.v_axis(),
                               plane.half_width(), plane.half_height());
    expect_invalid_image_plane(plane.origin(), plane.u_axis(), Eigen::Vector3d(0, 2, 0),
                               plane.half_width(), plane.half_height());
    expect_invalid_image_plane(plane.origin(), plane.u_axis(), plane.u_axis(),
                               plane.half_width(), plane.half_height());
    expect_invalid_image_plane(plane.origin(), plane.u_axis(), plane.v_axis(), 0.0,
                               plane.half_height());
    expect_invalid_image_plane(plane.origin(), plane.u_axis(), plane.v_axis(),
                               plane.half_width(), -1.0);
    expect_invalid_image_plane(
        Eigen::Vector3d::Constant(std::numeric_limits<double>::quiet_NaN()), plane.u_axis(),
        plane.v_axis(), plane.half_width(), plane.half_height());

    return TestSupport::report();
}

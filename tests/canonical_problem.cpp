#include "support/Check.h"

#include <geometry/Lens.h>
#include <geometry/WorldFrame.h>
#include <problem/PropagationProblem.h>

#include <limits>

namespace {

bool vectors_close(const Eigen::Vector3d& actual, const Eigen::Vector3d& expected,
                   double rel_tol) {
    return TestSupport::close_rel(actual.x(), expected.x(), rel_tol) &&
           TestSupport::close_rel(actual.y(), expected.y(), rel_tol) &&
           TestSupport::close_rel(actual.z(), expected.z(), rel_tol);
}

void expect_invalid_problem(const Geometry::Lens& lens, const Geometry::Source& source,
                            const Geometry::Observer& observer,
                            const Geometry::ImagePlane& image_plane) {
    try {
        (void)Problem::PropagationProblem(lens, source, observer, image_plane);
        CHECK(false, "expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    }
}

Problem::PropagationProblem make_problem(const Geometry::Lens& lens,
                                         const Geometry::Source& source,
                                         const Geometry::Observer& observer,
                                         const Geometry::ImagePlane& image_plane) {
    return Problem::PropagationProblem(lens, source, observer, image_plane);
}

} // namespace

int main() {
    const Problem::PropagationProblem problem =
        Problem::make_aligned_problem(Spacetime::SchwarzschildParameters{.rs = 1.0}, 100.0, 50.0,
                                      2.0, 2.0);

    CHECK(vectors_close(problem.lens().position, Eigen::Vector3d::Zero(), 1e-15),
          "aligned lens position");
    CHECK_CLOSE(problem.lens().parameters.rs, 1.0, 1e-15, "aligned lens rs");
    CHECK(vectors_close(problem.source().position, Eigen::Vector3d(0, 0, -100), 1e-15),
          "aligned source position");
    CHECK(vectors_close(problem.observer().position(), Eigen::Vector3d(0, 0, 50), 1e-15),
          "aligned observer position");
    CHECK(vectors_close(problem.observer().forward(), Eigen::Vector3d(0, 0, -1), 1e-12),
          "aligned observer forward");
    CHECK(vectors_close(problem.image_plane().origin(), Eigen::Vector3d(0, 0, 50), 1e-15),
          "aligned image plane origin");
    CHECK(vectors_close(problem.image_plane().normal(), Eigen::Vector3d(0, 0, 1), 1e-12),
          "aligned image plane normal");
    CHECK_CLOSE(problem.source_distance(), 100.0, 1e-15, "source distance");
    CHECK_CLOSE(problem.observer_distance(), 50.0, 1e-15, "observer distance");

    const Eigen::Vector3d source_offset = problem.source().position - problem.lens().position;
    const Eigen::Vector3d observer_offset =
        problem.observer().position() - problem.lens().position;
    CHECK(source_offset.dot(Geometry::WorldFrame::optical_axis()) < 0.0,
          "source behind lens along optical axis");
    CHECK(observer_offset.dot(Geometry::WorldFrame::optical_axis()) > 0.0,
          "observer ahead of lens along optical axis");

    CHECK(vectors_close(Geometry::to_chart_frame(problem.lens(), problem.observer().position()),
                        Eigen::Vector3d(50, 0, 0), 1e-15),
          "observer chart frame");
    CHECK(vectors_close(Geometry::to_chart_frame(problem.lens(), problem.source().position),
                        Eigen::Vector3d(-100, 0, 0), 1e-15),
          "source chart frame");

    const Eigen::Vector3d world_point(4.0, -2.0, 7.0);
    CHECK(vectors_close(
              Geometry::from_chart_frame(problem.lens(),
                                         Geometry::to_chart_frame(problem.lens(), world_point)),
              world_point, 1e-15),
          "chart frame round trip");

    Geometry::Lens translated_lens;
    translated_lens.position = Eigen::Vector3d(3, -4, 5);
    translated_lens.parameters.rs = 1.0;
    const Eigen::Vector3d translated_point(8.0, 1.0, -2.0);
    CHECK(vectors_close(Geometry::to_lens_frame(translated_lens, translated_point),
                        translated_point - translated_lens.position, 1e-15),
          "translated lens frame");
    CHECK(vectors_close(
              Geometry::from_chart_frame(translated_lens,
                                         Geometry::to_chart_frame(translated_lens, translated_point)),
              translated_point, 1e-15),
          "translated chart round trip");

    Geometry::Lens off_axis_lens;
    off_axis_lens.parameters.rs = 1.0;
    Geometry::Source off_axis_source;
    off_axis_source.position = Eigen::Vector3d(0, 0, -100);
    Geometry::Observer off_axis_observer = Geometry::Observer::looking_at(
        Eigen::Vector3d(10, 0, 50), off_axis_lens.position, Geometry::WorldFrame::plane_v_axis());
    Geometry::ImagePlane off_axis_plane =
        Geometry::ImagePlane::attached_to(off_axis_observer, 2.0, 2.0);
    const Problem::PropagationProblem off_axis_problem =
        make_problem(off_axis_lens, off_axis_source, off_axis_observer, off_axis_plane);
    CHECK(off_axis_problem.observer().position().x() > 0.0, "off-axis observer constructible");

    Geometry::Lens bad_lens = problem.lens();
    bad_lens.parameters.rs = 0.0;
    expect_invalid_problem(bad_lens, problem.source(), problem.observer(), problem.image_plane());

    bad_lens = problem.lens();
    bad_lens.parameters.rs = std::numeric_limits<double>::quiet_NaN();
    expect_invalid_problem(bad_lens, problem.source(), problem.observer(), problem.image_plane());

    Geometry::Observer inside_observer = Geometry::Observer::looking_at(
        Eigen::Vector3d(0, 0, 0.5), problem.lens().position,
        Geometry::WorldFrame::plane_v_axis());
    expect_invalid_problem(problem.lens(), problem.source(), inside_observer,
                           problem.image_plane());

    Geometry::Source inside_source;
    inside_source.position = Eigen::Vector3d(0, 0, 0.5);
    expect_invalid_problem(problem.lens(), inside_source, problem.observer(),
                           problem.image_plane());

    Geometry::Source non_finite_source;
    non_finite_source.position =
        Eigen::Vector3d(0, 0, std::numeric_limits<double>::quiet_NaN());
    expect_invalid_problem(problem.lens(), non_finite_source, problem.observer(),
                           problem.image_plane());

    Geometry::ImagePlane flipped_plane(
        problem.image_plane().origin(), problem.image_plane().u_axis(),
        -problem.image_plane().v_axis(), problem.image_plane().half_width(),
        problem.image_plane().half_height());
    expect_invalid_problem(problem.lens(), problem.source(), problem.observer(), flipped_plane);

    Geometry::ImagePlane lateral_plane(
        problem.image_plane().origin() + Eigen::Vector3d(1, 0, 0),
        problem.image_plane().u_axis(), problem.image_plane().v_axis(),
        problem.image_plane().half_width(), problem.image_plane().half_height());
    expect_invalid_problem(problem.lens(), problem.source(), problem.observer(), lateral_plane);

    try {
        (void)Problem::make_aligned_problem(Spacetime::SchwarzschildParameters{.rs = 1.0}, 100.0,
                                            50.0, 0.0, 2.0);
        CHECK(false, "expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    }

    return TestSupport::report();
}

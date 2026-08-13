#include "support/Check.h"
#include "support/True2DPipelineTestHelpers.h"

#include <imaging/ImageFormation.h>
#include <integrators/RK4Integrator.h>
#include <schwarzschild/PropagationContext.h>

#include <cmath>
#include <limits>

int main() {
    constexpr double angular_extent = 0.8;
    constexpr double source_distance = 30.0;
    constexpr double observer_axial_distance = 30.0;
    constexpr double observer_transverse_u = 1.0;
    constexpr double b_max = 20.0;
    constexpr int samples_per_axis = 5;
    constexpr double step_size = 0.01;
    constexpr int max_steps = 300000;
    constexpr std::size_t resolution = 64;
    constexpr double hit_tolerance = 1e-6;

    const Problem::PropagationProblem problem = True2DTest::make_problem(
        source_distance, observer_axial_distance, observer_transverse_u, angular_extent / 2.0);

    Schwarzschild::PropagationOptions options;
    options.horizon_safety_factor = 1.0001;
    options.escape_radius = std::numeric_limits<double>::infinity();
    options.null_constraint_projection = true;
    options.null_projection_interval = 1000;
    Schwarzschild::PropagationContext context(Spacetime::SchwarzschildParameters{.rs = 1.0},
                                              options);

    const Propagation::RadiusBoundTermination fallback(
        1.0001, std::numeric_limits<double>::infinity());
    Integration::RK4Integrator integrator;
    const Propagation::IntegrationSettings settings{.step_size = step_size,
                                                    .max_steps = max_steps};

    const True2DTest::True2DPipelineResult result = True2DTest::collect_observer_angular_samples(
        problem, Rays::RayGrid2DSamplingConfig{.samples_per_axis = samples_per_axis,
                                               .max_impact_parameter = b_max},
        Arrivals::ObserverLaunchRefinementConfig{.hit_tolerance = hit_tolerance,
                                                 .max_iterations = 12},
        context, fallback, settings, integrator);

    CHECK(!result.accepted.empty(), "off-axis 5x5 finds at least one observer-reaching ray");
    CHECK(static_cast<int>(result.accepted.size()) >= 2,
          "off-axis 5x5 finds the two Schwarzschild images");

    const Eigen::Vector3d observer_position = problem.observer().position();
    const Geometry::ImagePlane& plane = problem.image_plane();
    for (const True2DTest::AcceptedAngularSample& sample : result.accepted) {
        CHECK(sample.arrival.status == Arrivals::ArrivalStatus::Arrived, "refined arrival status");
        CHECK(sample.plane_residual.norm() <= hit_tolerance, "plane residual at observer origin");
        CHECK(std::abs(plane.signed_distance(sample.arrival.world_position)) <= 1e-6,
              "arrival lies on the observer plane");
        CHECK((sample.arrival.world_position - observer_position).norm() <= 1e-5,
              "arrival is at the displaced observer, not elsewhere on the plane");
        CHECK(std::isfinite(sample.angular_coordinate.x()) &&
                  std::isfinite(sample.angular_coordinate.y()),
              "observer-centered angular coordinate is finite");
        CHECK(std::isfinite(sample.b_u) && std::isfinite(sample.b_v), "launch parameters finite");
    }

    const std::vector<Eigen::Vector2d> image_samples = Arrivals::fill_aligned_observer_ring(
        result.angular_coordinates, observer_transverse_u, 720);
    CHECK(image_samples.size() == result.angular_coordinates.size(),
          "off-axis path does not azimuthally expand");

    const Imaging::Image image = Imaging::ImageFormation::form_image(
        image_samples, resolution, resolution, angular_extent);
    CHECK(image.max_intensity() > 0.0, "off-axis image is nonempty");

    return TestSupport::report();
}

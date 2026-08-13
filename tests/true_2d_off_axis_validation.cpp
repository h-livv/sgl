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
    constexpr int samples_per_axis = 21;
    constexpr double step_size = 0.01;
    constexpr int max_steps = 300000;
    constexpr std::size_t resolution = 128;

    const Problem::PropagationProblem problem =
        True2DTest::make_problem(source_distance, observer_axial_distance, observer_transverse_u,
                                 angular_extent / 2.0);

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
        Arrivals::ObserverLaunchRefinementConfig{.hit_tolerance = 1e-6, .max_iterations = 12},
        context, fallback, settings, integrator);

    CHECK(static_cast<int>(result.accepted.size()) >= 8, "off-axis accepted enough rays");

    const Eigen::Vector2d mean = True2DTest::centroid(result.angular_coordinates);
    const double anisotropy = True2DTest::second_moment_anisotropy(result.angular_coordinates);

    CHECK(mean.norm() >= 0.01 || anisotropy >= 0.15,
          "off-axis image is not constrained to circular symmetry");

    const Imaging::Image image = Imaging::ImageFormation::form_image(
        result.angular_coordinates, resolution, resolution, angular_extent);
    CHECK(image.max_intensity() > 0.0, "off-axis 2D image has positive max");

    return TestSupport::report();
}

#include "support/AngularPipelineTestHelpers.h"
#include "support/Check.h"
#include "support/True2DPipelineTestHelpers.h"

#include <imaging/ImageFormation.h>
#include <integrators/RK4Integrator.h>
#include <schwarzschild/PropagationContext.h>

#include <cmath>
#include <limits>

namespace {

bool has_nonzero_in_quadrant(const Imaging::Image& image, bool positive_u, bool positive_v) {
    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            if (image.at(x, y) <= 0.0) {
                continue;
            }
            const Eigen::Vector2d center = image.pixel_center(x, y);
            const bool u_positive = center.x() >= 0.0;
            const bool v_positive = center.y() >= 0.0;
            if (u_positive == positive_u && v_positive == positive_v) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

int main() {
    constexpr double angular_extent = 0.8;
    constexpr double source_distance = 30.0;
    constexpr double observer_axial_distance = 30.0;
    constexpr double observer_transverse_u = 0.0;
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

    const AngularPipelineTest::SelectedObserverHit method_a =
        AngularPipelineTest::run_angular_pipeline(
            problem, context, fallback, settings, integrator,
            AngularPipelineTest::RayModel::Parallel, source_distance, 81, 0.0, b_max, 1e-6, 60);

    CHECK(method_a.selected_bracket_index >= 0, "Method A selected a primary observer hit");
    CHECK(std::isfinite(method_a.angular_radius), "Method A angular radius finite");

    const True2DTest::True2DPipelineResult method_b = True2DTest::collect_observer_angular_samples(
        problem, Rays::RayGrid2DSamplingConfig{.samples_per_axis = samples_per_axis,
                                               .max_impact_parameter = b_max},
        Arrivals::ObserverLaunchRefinementConfig{.hit_tolerance = 1e-6, .max_iterations = 12},
        context, fallback, settings, integrator);

    CHECK(static_cast<int>(method_b.accepted.size()) >= 24, "Method B accepted enough rays");

    const double median_radius =
        True2DTest::characteristic_radius(method_b.angular_coordinates);
    const double scatter = True2DTest::radial_stddev(method_b.angular_coordinates);

    CHECK(std::isfinite(median_radius), "Method B median radius finite");
    CHECK(scatter <= 0.03, "Method B radial scatter within tolerance");
    CHECK(std::abs(median_radius - method_a.angular_radius) <= 0.02,
          "Method B median radius agrees with Method A");

    const Imaging::Image image = Imaging::ImageFormation::form_image(
        method_b.angular_coordinates, resolution, resolution, angular_extent);
    CHECK(image.max_intensity() > 0.0, "2D image has positive max");

    const std::size_t center_x = image.width() / 2;
    const std::size_t center_y = image.height() / 2;
    CHECK_CLOSE(image.at(center_x, center_y), 0.0, 1e-15, "center pixel intensity zero");

    CHECK(has_nonzero_in_quadrant(image, true, true), "nonzero in quadrant ++");
    CHECK(has_nonzero_in_quadrant(image, true, false), "nonzero in quadrant +-");
    CHECK(has_nonzero_in_quadrant(image, false, true), "nonzero in quadrant -+");
    CHECK(has_nonzero_in_quadrant(image, false, false), "nonzero in quadrant --");

    return TestSupport::report();
}

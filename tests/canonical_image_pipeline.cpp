#include "support/AngularPipelineTestHelpers.h"
#include "support/Check.h"

#include <arrivals/ObserverAngularCoordinates.h>
#include <imaging/ImageFormation.h>
#include <integrators/RK4Integrator.h>
#include <problem/PropagationProblem.h>
#include <propagation/TerminationPolicy.h>
#include <schwarzschild/PropagationContext.h>

#include <cmath>
#include <limits>
#include <vector>

// Experiment-level: canonical 1D imaging path (mirrors sgl_canonical_sgl_image).
// Contract: observer-hit residual within 1e-6; azimuthal gnomonic ring in all
//           quadrants; hollow center; r_max/r_min < 1.25; form_image is bitwise
//           deterministic.
// Pipeline: kernel + geometry + rays + arrivals + imaging at S = D = 30, rs = 1.
// Caveat: ray_count = 9 is coarser than angular_image_pipeline.cpp (41 rays);
//         still required to find a primary hit.

namespace {

bool images_equal(const Imaging::Image& a, const Imaging::Image& b) {
    if (a.width() != b.width() || a.height() != b.height()) {
        return false;
    }
    const std::vector<double>& data_a = a.data();
    const std::vector<double>& data_b = b.data();
    if (data_a.size() != data_b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < data_a.size(); ++i) {
        if (data_a[i] != data_b[i]) {
            return false;
        }
    }
    return true;
}

int count_nonzero_pixels(const Imaging::Image& image) {
    int count = 0;
    for (double value : image.data()) {
        if (value > 0.0) {
            ++count;
        }
    }
    return count;
}

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

double min_nonzero_radius(const Imaging::Image& image) {
    double r_min = std::numeric_limits<double>::infinity();
    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            if (image.at(x, y) <= 0.0) {
                continue;
            }
            r_min = std::min(r_min, image.pixel_center(x, y).norm());
        }
    }
    return r_min;
}

double max_nonzero_radius(const Imaging::Image& image) {
    double r_max = 0.0;
    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            if (image.at(x, y) <= 0.0) {
                continue;
            }
            r_max = std::max(r_max, image.pixel_center(x, y).norm());
        }
    }
    return r_max;
}

} // namespace

int main() {
    constexpr double angular_extent = 0.8;
    constexpr int ray_count = 9;
    constexpr int azimuth_count = 96;
    constexpr std::size_t resolution = 128;
    constexpr double b_min = 2.0;
    constexpr double b_max = 20.0;
    constexpr double step_size = 0.01;
    constexpr int max_steps = 300000;
    constexpr double observer_hit_tolerance = 1e-6;
    constexpr int max_root_iterations = 60;

    // Canonical toy geometry: rs = 1, source and observer both 30 rs from the lens.
    const Problem::PropagationProblem problem = Problem::make_aligned_problem(
        Spacetime::SchwarzschildParameters{.rs = 1.0}, 30.0, 30.0, angular_extent / 2.0,
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

    const AngularPipelineTest::SelectedObserverHit selection =
        AngularPipelineTest::run_angular_pipeline(
            problem, context, fallback, settings, integrator,
            AngularPipelineTest::RayModel::Point, 30.0, ray_count, b_min, b_max,
            observer_hit_tolerance, max_root_iterations);

    CHECK(selection.candidate_count >= 1, "at least one observer-hit bracket");
    CHECK(selection.selected_bracket_index >= 0, "primary observer hit selected");
    CHECK(std::abs(selection.hit.residual_u) <= observer_hit_tolerance,
          "refined observer-hit residual within tolerance");

    const std::vector<Eigen::Vector2d> angular_samples =
        Arrivals::expand_angular_azimuthally(selection.angular_coordinate.x(), azimuth_count);
    CHECK(angular_samples.size() > 0, "nonzero angular samples");

    const Imaging::Image image = Imaging::ImageFormation::form_image(
        angular_samples, resolution, resolution, angular_extent);
    CHECK(image.max_intensity() > 0.0, "formed image has positive max");

    const Imaging::Image normalized = image.normalized_to_max();
    CHECK_CLOSE(normalized.max_intensity(), 1.0, 1e-15, "normalized max is one");

    const std::size_t center_x = image.width() / 2;
    const std::size_t center_y = image.height() / 2;
    CHECK_CLOSE(image.at(center_x, center_y), 0.0, 1e-15, "center pixel intensity zero");

    CHECK(count_nonzero_pixels(image) > 0, "nonzero pixels exist");
    CHECK(has_nonzero_in_quadrant(image, true, true), "nonzero in quadrant ++");
    CHECK(has_nonzero_in_quadrant(image, true, false), "nonzero in quadrant +-");
    CHECK(has_nonzero_in_quadrant(image, false, true), "nonzero in quadrant -+");
    CHECK(has_nonzero_in_quadrant(image, false, false), "nonzero in quadrant --");

    const double r_min = min_nonzero_radius(image);
    const double r_max = max_nonzero_radius(image);
    CHECK(r_min > 0.0, "localized angular ring has positive inner radius");
    CHECK(r_max / r_min < 1.25, "localized angular ring thickness bounded");

    const Imaging::Image image_repeat = Imaging::ImageFormation::form_image(
        angular_samples, resolution, resolution, angular_extent);
    CHECK(images_equal(image, image_repeat), "deterministic image formation");

    return TestSupport::report();
}

#include "support/AngularPipelineTestHelpers.h"
#include "support/Check.h"

#include <arrivals/ObserverAngularCoordinates.h>
#include <imaging/ImageFormation.h>

#include <cmath>
#include <limits>
#include <vector>

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
    constexpr int ray_count = 41;
    constexpr int azimuth_count = 96;
    constexpr std::size_t resolution = 128;
    constexpr double b_min = 2.0;
    constexpr double b_max = 20.0;
    constexpr double step_size = 0.01;
    constexpr int max_steps = 300000;
    constexpr double observer_hit_tolerance = 1e-6;
    constexpr int max_root_iterations = 60;

    const Problem::PropagationProblem problem = Problem::make_aligned_problem(
        Spacetime::SchwarzschildParameters{.rs = 1.0}, 100.0, 30.0, angular_extent / 2.0,
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
            AngularPipelineTest::RayModel::Point, 100.0, ray_count, b_min, b_max,
            observer_hit_tolerance, max_root_iterations);

    CHECK(selection.candidate_count >= 1, "at least one observer-hit bracket");
    CHECK(selection.selected_bracket_index >= 0, "primary observer hit selected");

    int selected_count = 0;
    for (const AngularPipelineTest::ObserverHitCandidate& candidate : selection.candidates) {
        if (candidate.selected) {
            ++selected_count;
        }
    }
    CHECK(selected_count == 1, "exactly one selected candidate");

  double smallest_positive_rho = std::numeric_limits<double>::infinity();
  for (const AngularPipelineTest::ObserverHitCandidate& candidate : selection.candidates) {
    if (!std::isfinite(candidate.angular_radius) || candidate.angular_radius <= 0.0) {
      continue;
    }
    smallest_positive_rho = std::min(smallest_positive_rho, candidate.angular_radius);
  }
  if (std::isfinite(smallest_positive_rho)) {
    CHECK_CLOSE(selection.angular_radius, smallest_positive_rho, 1e-8,
                "selected candidate has smallest positive angular radius");
  }

    CHECK(std::abs(selection.hit.residual_u) <= observer_hit_tolerance,
          "refined observer-hit residual within tolerance");
    CHECK(std::isfinite(selection.angular_coordinate.x()) &&
              std::isfinite(selection.angular_coordinate.y()),
          "angular coordinate finite");

    const std::vector<Eigen::Vector2d> angular_samples =
        Arrivals::expand_angular_azimuthally(selection.angular_coordinate.x(), azimuth_count);
    CHECK(angular_samples.size() > 0, "angular samples produced");

    const Imaging::Image image = Imaging::ImageFormation::form_image(
        angular_samples, resolution, resolution, angular_extent);
    CHECK(image.max_intensity() > 0.0, "formed image has positive max");

    const Imaging::Image normalized = image.normalized_to_max();
    CHECK_CLOSE(normalized.max_intensity(), 1.0, 1e-15, "normalized max is one");

    const std::size_t center_x = image.width() / 2;
    const std::size_t center_y = image.height() / 2;
    CHECK_CLOSE(image.at(center_x, center_y), 0.0, 1e-15, "center pixel intensity zero");

    CHECK(has_nonzero_in_quadrant(image, true, true), "nonzero in quadrant ++");
    CHECK(has_nonzero_in_quadrant(image, true, false), "nonzero in quadrant +-");
    CHECK(has_nonzero_in_quadrant(image, false, true), "nonzero in quadrant -+");
    CHECK(has_nonzero_in_quadrant(image, false, false), "nonzero in quadrant --");

    const double r_min = min_nonzero_radius(image);
    const double r_max = max_nonzero_radius(image);
    CHECK(r_min > 0.0, "localized ring has positive inner radius");
    CHECK(r_max / r_min < 1.25, "localized ring thickness bounded");

    return TestSupport::report();
}

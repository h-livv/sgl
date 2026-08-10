#include "support/Check.h"

#include <arrivals/ArrivalCollector.h>
#include <arrivals/AzimuthalExpansion.h>
#include <imaging/ImageFormation.h>
#include <integrators/RK4Integrator.h>
#include <problem/PropagationProblem.h>
#include <propagation/TerminationPolicy.h>
#include <rays/RaySampler.h>
#include <schwarzschild/PropagationContext.h>

#include <Eigen/Dense>

#include <cmath>
#include <limits>
#include <vector>

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

bool has_pixel_in_radius_band(const Imaging::Image& image, double r_min, double r_max) {
    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            if (image.at(x, y) <= 0.0) {
                continue;
            }
            const Eigen::Vector2d center = image.pixel_center(x, y);
            const double radius = center.norm();
            if (radius >= r_min && radius <= r_max) {
                return true;
            }
        }
    }
    return false;
}

bool has_pixel_inside_radius(const Imaging::Image& image, double radius_limit) {
    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            if (image.at(x, y) <= 0.0) {
                continue;
            }
            const Eigen::Vector2d center = image.pixel_center(x, y);
            if (center.norm() < radius_limit) {
                return true;
            }
        }
    }
    return false;
}

bool has_pixel_outside_radius(const Imaging::Image& image, double radius_limit) {
    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            if (image.at(x, y) <= 0.0) {
                continue;
            }
            const Eigen::Vector2d center = image.pixel_center(x, y);
            if (center.norm() > radius_limit) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

int main() {
    constexpr double extent = 40.0;
    constexpr int ray_count = 9;
    constexpr int azimuth_count = 96;
    constexpr std::size_t resolution = 128;
    constexpr double b_min = 10.2;
    constexpr double b_max = 11.6;
    constexpr double step_size = 0.01;
    constexpr int max_steps = 300000;

    const Problem::PropagationProblem problem = Problem::make_aligned_problem(
        Spacetime::SchwarzschildParameters{.rs = 1.0}, 30.0, 30.0, extent / 2.0, extent / 2.0);

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
    const Rays::RaySampler sampler(Rays::RaySamplingConfig{
        .ray_count = ray_count, .min_impact_parameter = b_min, .max_impact_parameter = b_max});
    const Rays::RayEnsemble ensemble = sampler.sample(problem);
    const std::vector<Arrivals::RayArrival> arrivals = Arrivals::collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());

    int arrived_count = 0;
    for (const Arrivals::RayArrival& arrival : arrivals) {
        if (arrival.status == Arrivals::ArrivalStatus::Arrived) {
            ++arrived_count;
        }
    }
    CHECK(arrived_count >= 1, "at least one arrived ray");

    const std::vector<Arrivals::PlaneArrival> plane_arrivals =
        Arrivals::expand_azimuthally(arrivals, problem.image_plane(), azimuth_count);
    CHECK(plane_arrivals.size() > 0, "nonzero plane arrivals");

    const Imaging::Image image =
        Imaging::ImageFormation::form_image(plane_arrivals, resolution, resolution, extent);
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

    CHECK(has_pixel_in_radius_band(image, 10.0, 20.0), "localized in radius band");
    CHECK(!has_pixel_inside_radius(image, 5.0), "none inside radius 5");
    CHECK(!has_pixel_outside_radius(image, 25.0), "none outside radius 25");

    const Imaging::Image image_repeat =
        Imaging::ImageFormation::form_image(plane_arrivals, resolution, resolution, extent);
    CHECK(images_equal(image, image_repeat), "deterministic image formation");

    return TestSupport::report();
}

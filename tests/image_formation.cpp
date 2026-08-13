#include "support/Check.h"

#include <arrivals/AzimuthalExpansion.h>
#include <imaging/Image.h>
#include <imaging/ImageFormation.h>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

void expect_invalid_image(std::size_t width, std::size_t height, double u_min, double u_max,
                          double v_min, double v_max) {
    try {
        (void)Imaging::Image(width, height, u_min, u_max, v_min, v_max);
        CHECK(false, "expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    }
}

void expect_out_of_range(const Imaging::Image& image, std::size_t x, std::size_t y) {
    try {
        (void)image.at(x, y);
        CHECK(false, "expected std::out_of_range");
    } catch (const std::out_of_range&) {
    }
}

} // namespace

int main() {
    const Imaging::Image image(4, 3, -2.0, 2.0, -1.5, 1.5);
    CHECK(image.width() == 4, "width");
    CHECK(image.height() == 3, "height");
    CHECK_CLOSE(image.du(), 1.0, 1e-15, "du");
    CHECK_CLOSE(image.dv(), 1.0, 1e-15, "dv");

    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            CHECK_CLOSE(image.at(x, y), 0.0, 1e-15, "initial pixel zero");
        }
    }

    expect_invalid_image(0, 3, -1.0, 1.0, -1.0, 1.0);
    expect_invalid_image(4, 0, -1.0, 1.0, -1.0, 1.0);
    expect_invalid_image(4, 3, 1.0, 1.0, -1.0, 1.0);
    expect_invalid_image(4, 3, -1.0, 1.0, 1.0, 1.0);

    const Eigen::Vector2d first_center = image.pixel_center(0, 0);
    CHECK_CLOSE(first_center.x(), -1.5, 1e-15, "pixel center (0,0) u");
    CHECK_CLOSE(first_center.y(), -1.0, 1e-15, "pixel center (0,0) v");

    const Eigen::Vector2d last_center = image.pixel_center(3, 2);
    CHECK_CLOSE(last_center.x(), 1.5, 1e-15, "pixel center last u");
    CHECK_CLOSE(last_center.y(), 1.0, 1e-15, "pixel center last v");

    const auto origin_pixel =
        Imaging::ImageFormation::pixel_for(image, Eigen::Vector2d(-2.0, -1.5));
    CHECK(origin_pixel.has_value(), "(u_min, v_min) maps");
    CHECK(origin_pixel->first == 0, "(u_min, v_min) x");
    CHECK(origin_pixel->second == 0, "(u_min, v_min) y");

    CHECK(!Imaging::ImageFormation::pixel_for(image, Eigen::Vector2d(2.0, 0.0)).has_value(),
          "u_max out of bounds");
    CHECK(!Imaging::ImageFormation::pixel_for(image, Eigen::Vector2d(0.0, 1.5)).has_value(),
          "v_max out of bounds");
    CHECK(!Imaging::ImageFormation::pixel_for(image, Eigen::Vector2d(-3.0, 0.0)).has_value(),
          "below u_min out of bounds");

    Imaging::Image mutable_image = image;
    const Arrivals::PlaneArrival out_of_bounds{
        0, Eigen::Vector2d(10.0, 10.0)};
    Imaging::ImageFormation::accumulate(mutable_image, out_of_bounds);
    for (std::size_t y = 0; y < mutable_image.height(); ++y) {
        for (std::size_t x = 0; x < mutable_image.width(); ++x) {
            CHECK_CLOSE(mutable_image.at(x, y), 0.0, 1e-15, "oob arrival ignored");
        }
    }

    const Arrivals::PlaneArrival in_bounds{1, Eigen::Vector2d(-1.5, -1.0)};
    Imaging::ImageFormation::accumulate(mutable_image, in_bounds);
    CHECK_CLOSE(mutable_image.at(0, 0), 1.0, 1e-15, "single plane accumulation");

    const Eigen::Vector2d vector_in_bounds(-1.5, -1.0);
    Imaging::ImageFormation::accumulate(mutable_image, vector_in_bounds);
    CHECK_CLOSE(mutable_image.at(0, 0), 2.0, 1e-15, "single vector accumulation");

    const std::vector<Eigen::Vector2d> repeated_vectors{
        Eigen::Vector2d(-1.5, -1.0),
        Eigen::Vector2d(-1.5, -1.0),
        Eigen::Vector2d(-1.5, -1.0),
    };
    Imaging::ImageFormation::accumulate(mutable_image, repeated_vectors);
    CHECK_CLOSE(mutable_image.at(0, 0), 5.0, 1e-15, "multiple vector accumulation");

    const std::vector<Arrivals::PlaneArrival> repeated{
        Arrivals::PlaneArrival{2, Eigen::Vector2d(-1.5, -1.0)},
        Arrivals::PlaneArrival{3, Eigen::Vector2d(-1.5, -1.0)},
        Arrivals::PlaneArrival{4, Eigen::Vector2d(-1.5, -1.0)},
    };
    Imaging::ImageFormation::accumulate(mutable_image, repeated);
    CHECK_CLOSE(mutable_image.at(0, 0), 8.0, 1e-15, "multiple plane accumulation");

    const Imaging::Image normalized = mutable_image.normalized_to_max();
    CHECK_CLOSE(normalized.max_intensity(), 1.0, 1e-15, "normalized max");
    CHECK_CLOSE(normalized.at(0, 0), 1.0, 1e-15, "normalized peak");

    const Imaging::Image zero_image(2, 2, -1.0, 1.0, -1.0, 1.0);
    const Imaging::Image zero_normalized = zero_image.normalized_to_max();
    CHECK_CLOSE(zero_normalized.max_intensity(), 0.0, 1e-15, "zero max");
    CHECK_CLOSE(zero_normalized.at(0, 0), 0.0, 1e-15, "zero normalized unchanged");

    const std::vector<Arrivals::PlaneArrival> arrivals{
        Arrivals::PlaneArrival{0, Eigen::Vector2d(0.5, 0.5)},
        Arrivals::PlaneArrival{1, Eigen::Vector2d(-0.5, -0.5)},
    };
    const Imaging::Image formed =
        Imaging::ImageFormation::form_image(arrivals, 4, 4, 4.0);
    CHECK(formed.width() == 4, "formed width");
    CHECK(formed.height() == 4, "formed height");
    CHECK_CLOSE(formed.u_min(), -2.0, 1e-15, "formed u_min");
    CHECK_CLOSE(formed.u_max(), 2.0, 1e-15, "formed u_max");

    const std::vector<Eigen::Vector2d> vector_arrivals{
        Eigen::Vector2d(0.5, 0.5),
        Eigen::Vector2d(-0.5, -0.5),
    };
    const Imaging::Image vector_formed =
        Imaging::ImageFormation::form_image(vector_arrivals, 4, 4, 4.0);
    CHECK(vector_formed.data() == formed.data() ||
              (vector_formed.data().size() == formed.data().size() &&
               std::equal(vector_formed.data().begin(), vector_formed.data().end(),
                          formed.data().begin())),
          "vector overload matches plane delegation");

    const Imaging::Image formed_again =
        Imaging::ImageFormation::form_image(arrivals, 4, 4, 4.0);
    CHECK(formed.data() == formed_again.data() ||
              (formed.data().size() == formed_again.data().size() &&
               std::equal(formed.data().begin(), formed.data().end(),
                          formed_again.data().begin())),
          "deterministic formation");

    expect_out_of_range(image, 4, 0);
    expect_out_of_range(image, 0, 3);

    return TestSupport::report();
}

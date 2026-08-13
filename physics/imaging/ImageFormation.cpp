#include "ImageFormation.h"

#include <algorithm>
#include <cmath>

namespace Imaging::ImageFormation {

std::optional<std::pair<std::size_t, std::size_t>> pixel_for(const Image& image,
                                                             const Eigen::Vector2d& plane_position) {
    const double u = plane_position.x();
    const double v = plane_position.y();

    if (u < image.u_min() || u >= image.u_max()) {
        return std::nullopt;
    }
    if (v < image.v_min() || v >= image.v_max()) {
        return std::nullopt;
    }

    const std::size_t x =
        static_cast<std::size_t>(std::floor((u - image.u_min()) / image.du()));
    const std::size_t y =
        static_cast<std::size_t>(std::floor((v - image.v_min()) / image.dv()));
    return std::make_pair(x, y);
}

void accumulate(Image& image, const Eigen::Vector2d& image_position) {
    const std::optional<std::pair<std::size_t, std::size_t>> pixel = pixel_for(image, image_position);
    if (!pixel.has_value()) {
        return;
    }
    image.at(pixel->first, pixel->second) += 1.0;
}

void accumulate(Image& image, const std::vector<Eigen::Vector2d>& image_positions) {
    for (const Eigen::Vector2d& position : image_positions) {
        accumulate(image, position);
    }
}

void accumulate(Image& image, const Arrivals::PlaneArrival& arrival) {
    accumulate(image, arrival.plane_position);
}

void accumulate(Image& image, const std::vector<Arrivals::PlaneArrival>& arrivals) {
    for (const Arrivals::PlaneArrival& arrival : arrivals) {
        accumulate(image, arrival);
    }
}

Image form_image(const std::vector<Eigen::Vector2d>& image_positions, std::size_t width,
                 std::size_t height, double coordinate_extent) {
    const double half_extent = 0.5 * coordinate_extent;
    Image image(width, height, -half_extent, half_extent, -half_extent, half_extent);
    accumulate(image, image_positions);
    return image;
}

Image form_image(const std::vector<Arrivals::PlaneArrival>& arrivals, std::size_t width,
                 std::size_t height, double coordinate_extent) {
    std::vector<Eigen::Vector2d> positions;
    positions.reserve(arrivals.size());
    for (const Arrivals::PlaneArrival& arrival : arrivals) {
        positions.push_back(arrival.plane_position);
    }
    return form_image(positions, width, height, coordinate_extent);
}

double covering_extent(const std::vector<Eigen::Vector2d>& image_positions,
                       double requested_extent) {
    if (!(requested_extent > 0.0) || !std::isfinite(requested_extent)) {
        return requested_extent;
    }

    double max_abs = 0.0;
    for (const Eigen::Vector2d& position : image_positions) {
        if (!std::isfinite(position.x()) || !std::isfinite(position.y())) {
            continue;
        }
        max_abs = std::max(max_abs, std::max(std::abs(position.x()), std::abs(position.y())));
    }
    if (!(max_abs > 0.0)) {
        return requested_extent;
    }

    const double half = 0.5 * requested_extent;
    if (max_abs < half) {
        return requested_extent;
    }

    constexpr double kMargin = 1.1;
    return 2.0 * max_abs * kMargin;
}

} // namespace Imaging::ImageFormation

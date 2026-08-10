#include "ImageFormation.h"

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

void accumulate(Image& image, const Arrivals::PlaneArrival& arrival) {
    const std::optional<std::pair<std::size_t, std::size_t>> pixel =
        pixel_for(image, arrival.plane_position);
    if (!pixel.has_value()) {
        return;
    }
    image.at(pixel->first, pixel->second) += 1.0;
}

void accumulate(Image& image, const std::vector<Arrivals::PlaneArrival>& arrivals) {
    for (const Arrivals::PlaneArrival& arrival : arrivals) {
        accumulate(image, arrival);
    }
}

Image form_image(const std::vector<Arrivals::PlaneArrival>& arrivals, std::size_t width,
                 std::size_t height, double physical_extent) {
    const double half_extent = 0.5 * physical_extent;
    Image image(width, height, -half_extent, half_extent, -half_extent, half_extent);
    accumulate(image, arrivals);
    return image;
}

} // namespace Imaging::ImageFormation

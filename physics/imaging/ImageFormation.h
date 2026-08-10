#pragma once

#include "Image.h"

#include <arrivals/AzimuthalExpansion.h>

#include <Eigen/Dense>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace Imaging::ImageFormation {

std::optional<std::pair<std::size_t, std::size_t>> pixel_for(const Image& image,
                                                             const Eigen::Vector2d& plane_position);

void accumulate(Image& image, const Arrivals::PlaneArrival& arrival);

void accumulate(Image& image, const std::vector<Arrivals::PlaneArrival>& arrivals);

Image form_image(const std::vector<Arrivals::PlaneArrival>& arrivals, std::size_t width,
                 std::size_t height, double physical_extent);

} // namespace Imaging::ImageFormation

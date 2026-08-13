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

void accumulate(Image& image, const Eigen::Vector2d& image_position);

void accumulate(Image& image, const std::vector<Eigen::Vector2d>& image_positions);

void accumulate(Image& image, const Arrivals::PlaneArrival& arrival);

void accumulate(Image& image, const std::vector<Arrivals::PlaneArrival>& arrivals);

Image form_image(const std::vector<Eigen::Vector2d>& image_positions, std::size_t width,
                 std::size_t height, double coordinate_extent);

Image form_image(const std::vector<Arrivals::PlaneArrival>& arrivals, std::size_t width,
                 std::size_t height, double coordinate_extent);

// Smallest even-sided extent that keeps every finite sample strictly inside the
// half-open imaging window, or `requested_extent` if it already does. Pixel
// mapping treats u_max / v_max as out of bounds, so a sample on the boundary
// is not inside.
double covering_extent(const std::vector<Eigen::Vector2d>& image_positions,
                       double requested_extent);

} // namespace Imaging::ImageFormation

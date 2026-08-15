#pragma once

#include "Image.h"

#include <arrivals/AzimuthalExpansion.h>

#include <Eigen/Dense>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace Imaging::ImageFormation {

// Bin observer-plane or gnomonic (u, v) samples into an Image.
// Each sample adds +1 to one pixel (unit counts, not flux). form_image
// returns raw counts; callers typically then Image::normalized_to_max.
// PGM I/O is in the experiments, which write rows from high v to low v.

// Half-open [u_min, u_max) × [v_min, v_max). A sample on the max boundary
// is out of bounds (nullopt).
std::optional<std::pair<std::size_t, std::size_t>> pixel_for(const Image& image,
                                                             const Eigen::Vector2d& plane_position);

void accumulate(Image& image, const Eigen::Vector2d& image_position);

void accumulate(Image& image, const std::vector<Eigen::Vector2d>& image_positions);

void accumulate(Image& image, const Arrivals::PlaneArrival& arrival);

void accumulate(Image& image, const std::vector<Arrivals::PlaneArrival>& arrivals);

// Square window ±coordinate_extent/2 in both axes, then accumulate.
Image form_image(const std::vector<Eigen::Vector2d>& image_positions, std::size_t width,
                 std::size_t height, double coordinate_extent);

Image form_image(const std::vector<Arrivals::PlaneArrival>& arrivals, std::size_t width,
                 std::size_t height, double coordinate_extent);

// Square extent that keeps every finite sample strictly inside the half-open
// imaging window, or `requested_extent` if it already does. Pixel mapping
// treats u_max / v_max as out of bounds, so a sample on the boundary is not
// inside. If any finite sample has max(|u|,|v|) >= requested_extent/2, grow
// to 2.2 × that max (1.1× margin on the half-width).
double covering_extent(const std::vector<Eigen::Vector2d>& image_positions,
                       double requested_extent);

} // namespace Imaging::ImageFormation

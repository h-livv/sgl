#include "Image.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Imaging {

namespace {

void validate_bounds(std::size_t width, std::size_t height, double u_min, double u_max,
                     double v_min, double v_max) {
    if (width == 0) {
        throw std::invalid_argument("Image: width must be > 0");
    }
    if (height == 0) {
        throw std::invalid_argument("Image: height must be > 0");
    }
    if (!std::isfinite(u_min) || !std::isfinite(u_max) || !std::isfinite(v_min) ||
        !std::isfinite(v_max)) {
        throw std::invalid_argument("Image: bounds must be finite");
    }
    if (!(u_min < u_max)) {
        throw std::invalid_argument("Image: u_min must be < u_max");
    }
    if (!(v_min < v_max)) {
        throw std::invalid_argument("Image: v_min must be < v_max");
    }
}

} // namespace

Image::Image(std::size_t width, std::size_t height, double u_min, double u_max, double v_min,
             double v_max)
    : width_(width), height_(height), u_min_(u_min), u_max_(u_max), v_min_(v_min),
      v_max_(v_max), intensity_(width * height, 0.0) {
    validate_bounds(width, height, u_min, u_max, v_min, v_max);
}

double Image::du() const { return (u_max_ - u_min_) / static_cast<double>(width_); }

double Image::dv() const { return (v_max_ - v_min_) / static_cast<double>(height_); }

Eigen::Vector2d Image::pixel_center(std::size_t x, std::size_t y) const {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("Image: pixel index out of range");
    }
    return Eigen::Vector2d(u_min_ + (static_cast<double>(x) + 0.5) * du(),
                           v_min_ + (static_cast<double>(y) + 0.5) * dv());
}

double& Image::at(std::size_t x, std::size_t y) {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("Image: pixel index out of range");
    }
    return intensity_[y * width_ + x];
}

double Image::at(std::size_t x, std::size_t y) const {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("Image: pixel index out of range");
    }
    return intensity_[y * width_ + x];
}

double Image::max_intensity() const {
    if (intensity_.empty()) {
        return 0.0;
    }
    return *std::max_element(intensity_.begin(), intensity_.end());
}

Image Image::normalized_to_max() const {
    Image normalized = *this;
    const double max_value = max_intensity();
    if (max_value > 0.0) {
        for (double& value : normalized.intensity_) {
            value /= max_value;
        }
    }
    return normalized;
}

} // namespace Imaging

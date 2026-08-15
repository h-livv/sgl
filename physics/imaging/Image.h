#pragma once

#include <Eigen/Dense>

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace Imaging {

// Histogram of observer-plane (or gnomonic angular) samples.
// intensity_[y * width + x], row-major. Pixel centers are cell-centered:
//   (u_min + (x + 1/2) du, v_min + (y + 1/2) dv).
// Values are unit counts, not flux. normalized_to_max scales by the peak
// count for display — not physical magnification.
// PGM writing is in the experiments (rows from high v to low v), not here.
class Image {
public:
    Image(std::size_t width, std::size_t height, double u_min, double u_max, double v_min,
          double v_max);

    std::size_t width() const { return width_; }
    std::size_t height() const { return height_; }

    double u_min() const { return u_min_; }
    double u_max() const { return u_max_; }
    double v_min() const { return v_min_; }
    double v_max() const { return v_max_; }

    double du() const;
    double dv() const;

    Eigen::Vector2d pixel_center(std::size_t x, std::size_t y) const;

    double& at(std::size_t x, std::size_t y);
    double at(std::size_t x, std::size_t y) const;

    const std::vector<double>& data() const { return intensity_; }
    std::vector<double>& data() { return intensity_; }

    double max_intensity() const;
    Image normalized_to_max() const;  // peak → 1; all-zero image is unchanged

private:
    std::size_t width_;
    std::size_t height_;
    double u_min_;
    double u_max_;
    double v_min_;
    double v_max_;
    std::vector<double> intensity_;
};

} // namespace Imaging

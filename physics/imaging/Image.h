#pragma once

#include <Eigen/Dense>

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace Imaging {

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
    Image normalized_to_max() const;

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

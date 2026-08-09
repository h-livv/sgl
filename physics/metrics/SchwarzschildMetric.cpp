#include "SchwarzschildMetric.h"
#include <cmath>

namespace Spacetime {

SchwarzschildMetric::SchwarzschildMetric(double rs) : rs_(rs) {}

double SchwarzschildMetric::christoffel(int mu, int alpha, int beta, const Eigen::Vector4d& X) const {
    double r = X[1];
    double theta = X[2];

    double sinth = std::sin(theta);
    double costh = std::cos(theta);

    if (alpha > beta) {
        std::swap(alpha, beta);
    }

    if (mu == 0) {
        if (alpha == 0 && beta == 1) return rs_ / (2.0 * r * (r - rs_));
    } else if (mu == 1) {
        if (alpha == 0 && beta == 0) return rs_ * (r - rs_) / (2.0 * r * r * r);
        if (alpha == 1 && beta == 1) return -rs_ / (2.0 * r * (r - rs_));
        if (alpha == 2 && beta == 2) return -(r - rs_);
        if (alpha == 3 && beta == 3) return -(r - rs_) * sinth * sinth;
    } else if (mu == 2) {
        if (alpha == 1 && beta == 2) return 1.0 / r;
        if (alpha == 3 && beta == 3) return -sinth * costh;
    } else if (mu == 3) {
        if (alpha == 1 && beta == 3) return 1.0 / r;
        if (alpha == 2 && beta == 3) return costh / (sinth + 1e-8);
    }

    return 0.0;
}

} // namespace Spacetime

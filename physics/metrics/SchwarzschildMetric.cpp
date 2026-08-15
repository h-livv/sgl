#include "SchwarzschildMetric.h"
#include <cmath>

namespace Spacetime {

SchwarzschildMetric::SchwarzschildMetric(double rs) : rs_(rs) {}

double SchwarzschildMetric::christoffel(int mu, int alpha, int beta, const Eigen::Vector4d& X) const {
    // Event X = (t, r, θ, φ). Stationary and axisymmetric: Γ depends only on r, θ.
    double r = X[1];
    double theta = X[2];

    double sinth = std::sin(theta);
    double costh = std::cos(theta);

    if (alpha > beta) {
        std::swap(alpha, beta);
    }

    // Nonzero Levi-Civita symbols of vacuum Schwarzschild (geometrized rs = rs_):
    //   Γ^t_tr       — redshift / time-dilation gradient (Killing time coupled to r)
    //   Γ^r_tt       — inward acceleration of a static observer
    //   Γ^r_rr       — radial curvature; denominators vanish at the horizon r = rs
    //   Γ^r_θθ, Γ^r_φφ — centrifugal terms from angular motion
    //   Γ^θ_rθ, Γ^φ_rφ — 1/r transport of polar / azimuthal velocity
    //   Γ^θ_φφ       — geodesic curvature on the coordinate 2-sphere
    //   Γ^φ_θφ       — cot θ; polar-axis singularity of spherical coordinates
    // α > β was swapped above so only the α ≤ β slot is tabulated (Γ^μ_αβ = Γ^μ_βα).
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
        // 1e-8 keeps Γ^φ_θφ finite on the polar axis (sinθ = 0).
        if (alpha == 2 && beta == 3) return costh / (sinth + 1e-8);
    }

    return 0.0;
}

} // namespace Spacetime

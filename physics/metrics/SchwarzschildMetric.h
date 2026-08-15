#pragma once
#include <core/Metric.h>

namespace Spacetime {

// Analytic vacuum-Schwarzschild connection in spherical coordinates (G = c = 1).
// Exterior of a non-rotating spherical mass — not Kerr, not a plasma.
// State: rs_ is the geometrized Schwarzschild radius (typically Parameters.rs = 1).
// Usage: PropagationContext owns the metric and hands it to GeodesicDynamics.
// Coordinate order at X: 0=t, 1=r, 2=θ, 3=φ. g_μν itself is not stored.
class SchwarzschildMetric : public Metric {
public:
    explicit SchwarzschildMetric(double rs);

    double christoffel(int mu, int alpha, int beta, const Eigen::Vector4d& X) const override;

    double get_rs() const { return rs_; }

private:
    double rs_;
};

} // namespace Spacetime

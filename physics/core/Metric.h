#pragma once
#include <Eigen/Dense>

namespace Spacetime {

// Abstract spacetime connection for geodesic integration (sgl_physics GR infrastructure).
// Architectural role: the only metric API the ODE sees. Implementations supply Γ^μ_αβ;
// the metric tensor g_μν is never stored or returned (null-cone checks live elsewhere).
// Usage: GeodesicDynamics holds a const Metric& and evaluates christoffel at each RK4 stage.
// Relationship: SchwarzschildMetric is the production implementation (vacuum, spherical).
class Metric {
public:
    virtual ~Metric() = default;

    // Connection coefficient Γ^μ_αβ at event X. Index order is coordinate-chart order
    // (Schwarzschild spherical: 0=t, 1=r, 2=θ, 3=φ). Symmetric in (α, β) for Levi-Civita Γ.
    virtual double christoffel(int mu, int alpha, int beta, const Eigen::Vector4d& X) const = 0;
};

} // namespace Spacetime

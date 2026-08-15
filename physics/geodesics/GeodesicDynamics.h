#pragma once
#include "DynamicsModel.h"
#include <core/Metric.h>

namespace Dynamics {

// Geodesic equation as eight first-order ODEs, using Metric::christoffel.
// d²x^μ/dλ² + Γ^μ_αβ (dx^α/dλ)(dx^β/dλ) = 0, with U = dx/dλ.
// Holds a const Metric& (lifetime must outlive this object; PropagationContext owns both).
class GeodesicDynamics : public DynamicsModel {
public:
    explicit GeodesicDynamics(const Spacetime::Metric& metric);

    State compute_derivative(const State& state) const override;

private:
    const Spacetime::Metric& metric_;
};

} // namespace Dynamics

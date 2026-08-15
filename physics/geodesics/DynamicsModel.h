#pragma once
#include <core/GeodesicState.h>

namespace Dynamics {

// First-order ODE right-hand side for a State: returns dState/dλ = (dX/dλ, dU/dλ).
// Architectural role: integrator-facing GR infrastructure (sgl_physics), not SGL geometry.
// RK4 calls this four times per step; IntegrationSettings.step_size is a fixed Δλ,
// independent of impact parameter. GeodesicDynamics is the production implementation;
// tests supply a free-motion stand-in. No explicit λ argument — the ODE is autonomous.
class DynamicsModel {
public:
    virtual ~DynamicsModel() = default;
    virtual State compute_derivative(const State& state) const = 0;
};

} // namespace Dynamics

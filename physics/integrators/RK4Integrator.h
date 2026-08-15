#pragma once

#include "Integrator.h"

namespace Integration {

// Classic 4-stage Runge–Kutta on State. Stages:
//   k1 = f(y),  k2 = f(y + dt/2 k1),  k3 = f(y + dt/2 k2),  k4 = f(y + dt k3)
//   y ← y + (dt/6) (k1 + 2 k2 + 2 k3 + k4)
// Metric-agnostic. Used by the imaging path and by Schwarzschild validation.
class RK4Integrator final : public Integrator {
public:
    State step(const State& state, double dt, const DerivativeFunc& derivative) const override;
};

} // namespace Integration

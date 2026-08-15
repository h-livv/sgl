#pragma once

#include <core/GeodesicState.h>

#include <functional>

namespace Integration {

// dState/dλ from geodesic dynamics: X-dot = U, U-dot = −Γ(U,U).
using DerivativeFunc = std::function<State(const State&)>;

// One affine-parameter step of the first-order system on State (X, U).
// Metric-agnostic: the metric enters only through `derivative`. Does not
// terminate, project onto the null cone, or record a path — those live in
// Propagation. This is the general GR kernel, not SGL geometry.
class Integrator {
public:
    virtual ~Integrator() = default;
    // Advance `state` by affine-parameter increment `dt` (independent of impact
    // parameter). `dt` may be negative. Returns a new State; does not mutate `state`.
    virtual State step(const State& state, double dt, const DerivativeFunc& derivative) const = 0;
};

} // namespace Integration

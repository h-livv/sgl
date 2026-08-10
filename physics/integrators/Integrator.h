#pragma once

#include <core/GeodesicState.h>

#include <functional>

namespace Integration {

using DerivativeFunc = std::function<State(const State&)>;

class Integrator {
public:
    virtual ~Integrator() = default;
    virtual State step(const State& state, double dt, const DerivativeFunc& derivative) const = 0;
};

} // namespace Integration

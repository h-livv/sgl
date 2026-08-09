#pragma once
#include <core/GeodesicState.h>

namespace Dynamics {

class DynamicsModel {
public:
    virtual ~DynamicsModel() = default;
    virtual State compute_derivative(const State& state) const = 0;
};

} // namespace Dynamics

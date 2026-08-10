#pragma once

#include "RayEnsemble.h"

#include <problem/PropagationProblem.h>

namespace Rays {

// Deterministic linear sweep in impact parameter. Geometrized units (G = c = 1).
struct RaySamplingConfig {
    int ray_count = 1;
    double min_impact_parameter = 0.0;
    double max_impact_parameter = 0.0;
};

class RaySampler {
public:
    explicit RaySampler(const RaySamplingConfig& config);

    RayEnsemble sample(const Problem::PropagationProblem& problem) const;

    const RaySamplingConfig& config() const { return config_; }

    // b_i for index i in [0, ray_count). ray_count == 1 yields min_impact_parameter.
    double impact_parameter_at(int index) const;

private:
    RaySamplingConfig config_;
};

} // namespace Rays

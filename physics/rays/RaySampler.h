#pragma once

#include "RayEnsemble.h"

#include <problem/PropagationProblem.h>

namespace Rays {

// Deterministic linear sweep in impact parameter b. Geometrized units (G = c = 1).
// Point-source equatorial fan: every ray shares the same start event (source
// spherical coordinates); only b changes. Uses build_null_scatter (vθ = 0).
// Observer pose is unused at launch — it is the later F=0 target.
struct RaySamplingConfig {
    int ray_count = 1;
    double min_impact_parameter = 0.0;
    double max_impact_parameter = 0.0;
};

class RaySampler {
public:
    // min_impact_parameter must be > 0. Inclusive endpoints in b.
    explicit RaySampler(const RaySamplingConfig& config);

    RayEnsemble sample(const Problem::PropagationProblem& problem) const;

    const RaySamplingConfig& config() const { return config_; }

    // b_i for index i in [0, ray_count). Inclusive endpoints.
    // ray_count == 1 yields min_impact_parameter (max is unused).
    double impact_parameter_at(int index) const;

private:
    RaySamplingConfig config_;
};

} // namespace Rays

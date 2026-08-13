#pragma once

#include "RayEnsemble.h"

#include <problem/PropagationProblem.h>

#include <cstddef>
#include <vector>

namespace Rays {

// Deterministic cell-centered square grid on the source launch plane.
// Geometrized units (G = c = 1).
//
// (b_u, b_v) are launch-plane impact-parameter offsets from the source along
// WorldFrame (X, Y), with every ray aimed source → lens. They are not observer
// coordinates. Moving the observer changes the F=0 target, not this grid.
struct RayGrid2DSamplingConfig {
    int samples_per_axis = 5;
    double max_impact_parameter = 20.0;
};

struct RayGrid2DSample {
    double b_u = 0.0;
    double b_v = 0.0;
    std::size_t ray_id = 0;
};

class RayGrid2DSampler {
public:
    explicit RayGrid2DSampler(const RayGrid2DSamplingConfig& config);

    RayEnsemble sample(const Problem::PropagationProblem& problem);

    const std::vector<RayGrid2DSample>& samples() const { return samples_; }

    const RayGrid2DSamplingConfig& config() const { return config_; }

    // Cell-centered value for index i in [0, samples_per_axis).
    double grid_value_at(int index) const;

    // Null initial state for an arbitrary launch-plane sample.
    State state_for(const Problem::PropagationProblem& problem, double b_u, double b_v) const;

private:
    RayGrid2DSamplingConfig config_;
    std::vector<RayGrid2DSample> samples_;
};

} // namespace Rays

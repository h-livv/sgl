#pragma once

namespace Propagation {

// Fixed-step RK4 controls in geometrized units (G = c = 1). `step_size` is a
// step in affine parameter λ, not a spatial step and not scaled by impact
// parameter — every ray uses the same Δλ.
struct IntegrationSettings {
    // Affine-parameter increment per RK4 step. Default 0.01.
    // Must be finite and non-zero; negative Δλ (backward in λ) is allowed.
    double step_size = 0.01;
    // Hard cap on RK4 steps. Default 100000; imaging experiments often use 300000.
    int max_steps = 100000;
};

} // namespace Propagation

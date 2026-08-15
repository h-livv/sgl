#pragma once

// Frozen numerical reference States for Schwarzschild-scenario regression
// (null_scatter_regression, schwarzschild_scenarios). Not imaging outputs.
// X,U are Schwarzschild spherical (t, r, θ, φ) and dX/dλ at the final sample.
// steps = outcome.steps_taken. terminated means PropagationStatus::Terminated
// (horizon/radius bound); false means StepBudgetExhausted.

namespace Baseline {

struct Case {
    double X[4];
    double U[4];
    int steps;
    bool terminated;
};

// Photon scatter via build_null_scatter; 50000-step budget, not captured.
inline constexpr Case null_scatter{
    {58.593962978084697, 18.36823089414451, 1.5707963267948966, 4.4172275679240416},
    {1.0575763879519215, 0.98645876629658802, 2.4836366909126005e-18, 0.0091824259995582514},
    50000,
    false,
};

// Timelike bound orbit via build_bound_orbit; 100000-step budget.
inline constexpr Case bound_orbit{
    {1113.5537006396846, 9.6464997321665891, 1.5707963267948966, 36.49814686611316},
    {1.0824332965380101, 0.0075702674185969881, 5.1875931542533878e-17, 0.023212089583585779},
    100000,
    false,
};

// Timelike radial fall (E=1, L=0) to r ≈ rs·1.0001; terminated.
inline constexpr Case radial_freefall{
    {-197.82290000419644, 0.94015888766518252, 1.5707963267948966, 0.0},
    {-7291381.8716144413, -1970556.850955083, 0.0, 0.0},
    20416,
    true,
};

// Custom null geodesic (inward vr, small vφ) to the horizon bound; terminated.
inline constexpr Case custom_null{
    {22.885580841379429, 0.99992089469926393, 1.5707963267948966, 2.2656101549741168},
    {10559.523850416614, -0.18333197170179871, 2.7750106159151423e-16, 2.0003160065055634},
    9903,
    true,
};

} // namespace Baseline

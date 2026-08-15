#pragma once

#include "NullConstraint.h"

#include <core/SchwarzschildParameters.h>
#include <geodesics/GeodesicDynamics.h>
#include <metrics/SchwarzschildMetric.h>
#include <propagation/Propagator.h>
#include <propagation/TerminationPolicy.h>

#include <limits>

namespace Schwarzschild {

// Schwarzschild kernel knobs. Imaging typically sets horizon_safety_factor =
// 1.0001, escape_radius = +∞, null_constraint_projection = true, interval = 1000.
struct PropagationOptions {
    double horizon_safety_factor = 1.0;  // r_min = rs * this; imaging uses 1.0001
    double escape_radius = std::numeric_limits<double>::infinity();
    bool null_constraint_projection = false;
    int null_projection_interval = 1000;  // 0 or negative is clamped to 1 (every step)
};

// Owns the Schwarzschild metric, geodesic RHS, radius-bound termination
// (r_min = rs * horizon_safety_factor, r_max = escape_radius), and optional
// null-projection correction. Non-copyable: GeodesicDynamics holds a reference
// to the metric member. This is the general GR kernel, not SGL geometry.
class PropagationContext {
public:
    explicit PropagationContext(const Spacetime::SchwarzschildParameters& parameters,
                                const PropagationOptions& options = {});

    PropagationContext(const PropagationContext&) = delete;
    PropagationContext& operator=(const PropagationContext&) = delete;

    const Dynamics::DynamicsModel& dynamics() const { return dynamics_; }
    const Propagation::TerminationPolicy& termination() const { return termination_; }
    const Propagation::StepCorrection& correction() const { return correction_; }

private:
    Spacetime::SchwarzschildMetric metric_;
    Dynamics::GeodesicDynamics dynamics_;
    Propagation::RadiusBoundTermination termination_;
    Propagation::StepCorrection correction_;
};

} // namespace Schwarzschild

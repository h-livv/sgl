#pragma once

#include "RayEnsemble.h"

#include <propagation/Propagator.h>

#include <vector>

namespace Rays {

// Index-aligned with the ensemble: outcomes[i] corresponds to ensemble.at(i), whose id is i.
// First sgl_rays entry that may invoke the GR kernel. Geodesics are independent.
using RayOutcomes = std::vector<Propagation::PropagationOutcome>;

RayOutcomes propagate_ensemble(const RayEnsemble& ensemble,
                               const Dynamics::DynamicsModel& dynamics,
                               const Propagation::TerminationPolicy& termination,
                               const Propagation::IntegrationSettings& settings,
                               const Integration::Integrator& integrator,
                               const Propagation::StepCorrection& correction = {});

} // namespace Rays

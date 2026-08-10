#pragma once

#include "IntegrationSettings.h"
#include "PropagationOutcome.h"
#include "TerminationPolicy.h"

#include <geodesics/DynamicsModel.h>
#include <integrators/Integrator.h>

#include <functional>
#include <vector>

namespace Propagation {

using StepCorrection = std::function<void(State& state, int step_index)>;

PropagationOutcome propagate(const State& initial_state, const Dynamics::DynamicsModel& dynamics,
                             const TerminationPolicy& termination,
                             const IntegrationSettings& settings,
                             const Integration::Integrator& integrator,
                             const StepCorrection& correction = {});

PropagationOutcome propagate_recorded(const State& initial_state,
                                      const Dynamics::DynamicsModel& dynamics,
                                      const TerminationPolicy& termination,
                                      const IntegrationSettings& settings,
                                      const Integration::Integrator& integrator,
                                      std::vector<State>& path,
                                      const StepCorrection& correction = {});

} // namespace Propagation

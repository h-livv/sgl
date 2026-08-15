#pragma once

#include "IntegrationSettings.h"
#include "PropagationOutcome.h"
#include "TerminationPolicy.h"

#include <geodesics/DynamicsModel.h>
#include <integrators/Integrator.h>

#include <functional>
#include <vector>

namespace Propagation {

// Post-step hook (e.g. null-cone projection). Called after each successful RK4
// step with the 0-based index of that step. Not called if the initial state is
// already terminated. Default-empty function is skipped (no-op).
using StepCorrection = std::function<void(State& state, int step_index)>;

// Integrate until termination or the step budget. Does not store the path;
// imaging uses this (previous_state / final_state only).
PropagationOutcome propagate(const State& initial_state, const Dynamics::DynamicsModel& dynamics,
                             const TerminationPolicy& termination,
                             const IntegrationSettings& settings,
                             const Integration::Integrator& integrator,
                             const StepCorrection& correction = {});

// Same loop, appending states onto `path` (does not clear it). After a fresh
// vector, path.front() is the initial state. Diagnostics, not the imaging path.
PropagationOutcome propagate_recorded(const State& initial_state,
                                      const Dynamics::DynamicsModel& dynamics,
                                      const TerminationPolicy& termination,
                                      const IntegrationSettings& settings,
                                      const Integration::Integrator& integrator,
                                      std::vector<State>& path,
                                      const StepCorrection& correction = {});

} // namespace Propagation

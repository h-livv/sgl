#pragma once

#include "RayArrival.h"

#include <geometry/ImagePlane.h>
#include <geometry/Lens.h>
#include <problem/PropagationProblem.h>
#include <propagation/PropagationOutcome.h>
#include <rays/EnsemblePropagator.h>
#include <rays/RayEnsemble.h>

#include <vector>

namespace Arrivals {

// Pure function: turns one propagation outcome into one arrival. No propagation.
RayArrival localize_arrival(std::size_t ray_id, const Geometry::Lens& lens,
                            const Geometry::ImagePlane& plane,
                            const Propagation::PropagationOutcome& outcome);

// Index-aligned with the ensemble: result[i] corresponds to ensemble.at(i).
// Builds a PlaneCrossingTermination from the problem, reuses Rays::propagate_ensemble,
// then localizes each outcome. fallback_termination bounds capture and escape.
std::vector<RayArrival> collect_arrivals(const Rays::RayEnsemble& ensemble,
                                         const Problem::PropagationProblem& problem,
                                         const Dynamics::DynamicsModel& dynamics,
                                         const Propagation::TerminationPolicy& fallback_termination,
                                         const Propagation::IntegrationSettings& settings,
                                         const Integration::Integrator& integrator,
                                         const Propagation::StepCorrection& correction = {});

} // namespace Arrivals

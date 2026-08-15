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

// First-order plane crossing from a finished geodesic. No further propagation.
// If the final sample is still behind the plane (signed_distance < 0) → NoCrossing.
// Else linear t = d_prev / (d_prev − d_curr), clamped to [0, 1], applied to
// world position and to chart State (X and U). Direction comes from the
// interpolated chart_state. This is not a root on the geodesic.
RayArrival localize_arrival(std::size_t ray_id, const Geometry::Lens& lens,
                            const Geometry::ImagePlane& plane,
                            const Propagation::PropagationOutcome& outcome);

// Index-aligned with the ensemble: result[i] corresponds to ensemble.at(i).
// Builds a PlaneCrossingTermination, propagate_ensemble, then localize each.
// Crossing the plane ≠ hitting the observer; 1D residual_u(b) bisection lives
// in the experiment, not here. fallback_termination bounds capture and escape.
std::vector<RayArrival> collect_arrivals(const Rays::RayEnsemble& ensemble,
                                         const Problem::PropagationProblem& problem,
                                         const Dynamics::DynamicsModel& dynamics,
                                         const Propagation::TerminationPolicy& fallback_termination,
                                         const Propagation::IntegrationSettings& settings,
                                         const Integration::Integrator& integrator,
                                         const Propagation::StepCorrection& correction = {});

} // namespace Arrivals

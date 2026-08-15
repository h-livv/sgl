#include "ArrivalCollector.h"

#include "ChartMapping.h"
#include "PlaneCrossingTermination.h"

#include <algorithm>

namespace Arrivals {

RayArrival localize_arrival(std::size_t ray_id, const Geometry::Lens& lens,
                            const Geometry::ImagePlane& plane,
                            const Propagation::PropagationOutcome& outcome) {
    RayArrival arrival;
    arrival.ray_id = ray_id;

    const Eigen::Vector3d p_curr = world_position(lens, outcome.final_state);
    const double d_curr = plane.signed_distance(p_curr);
    if (d_curr < 0.0) {
        arrival.status = ArrivalStatus::NoCrossing;
        return arrival;
    }

    const Eigen::Vector3d p_prev = world_position(lens, outcome.previous_state);
    const double d_prev = plane.signed_distance(p_prev);

    // Linear zero of signed_distance along the last segment. Equal distances
    // leave t = 0 (previous sample). Not a geodesic root finder.
    double t = 0.0;
    if (d_prev != d_curr) {
        t = d_prev / (d_prev - d_curr);
        t = std::clamp(t, 0.0, 1.0);
    }

    arrival.world_position = p_prev + t * (p_curr - p_prev);
    // Same t on spherical X and U; first-order, not a geodesic interpolation.
    arrival.chart_state =
        State(outcome.previous_state.X + t * (outcome.final_state.X - outcome.previous_state.X),
              outcome.previous_state.U + t * (outcome.final_state.U - outcome.previous_state.U));
    arrival.world_direction = world_direction(lens, arrival.chart_state);
    arrival.status = ArrivalStatus::Arrived;
    return arrival;
}

std::vector<RayArrival> collect_arrivals(const Rays::RayEnsemble& ensemble,
                                         const Problem::PropagationProblem& problem,
                                         const Dynamics::DynamicsModel& dynamics,
                                         const Propagation::TerminationPolicy& fallback_termination,
                                         const Propagation::IntegrationSettings& settings,
                                         const Integration::Integrator& integrator,
                                         const Propagation::StepCorrection& correction) {
    const PlaneCrossingTermination termination(problem.lens(), problem.image_plane(),
                                               fallback_termination);
    const Rays::RayOutcomes outcomes = Rays::propagate_ensemble(
        ensemble, dynamics, termination, settings, integrator, correction);

    std::vector<RayArrival> arrivals;
    arrivals.reserve(outcomes.size());
    for (std::size_t i = 0; i < outcomes.size(); ++i) {  // index-aligned with ensemble
        arrivals.push_back(localize_arrival(ensemble.at(i).id, problem.lens(),
                                          problem.image_plane(), outcomes[i]));
    }
    return arrivals;
}

} // namespace Arrivals

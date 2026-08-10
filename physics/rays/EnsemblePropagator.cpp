#include "EnsemblePropagator.h"

namespace Rays {

RayOutcomes propagate_ensemble(const RayEnsemble& ensemble,
                               const Dynamics::DynamicsModel& dynamics,
                               const Propagation::TerminationPolicy& termination,
                               const Propagation::IntegrationSettings& settings,
                               const Integration::Integrator& integrator,
                               const Propagation::StepCorrection& correction) {
    RayOutcomes outcomes;
    outcomes.reserve(ensemble.size());
    for (const Ray& ray : ensemble) {
        outcomes.push_back(Propagation::propagate(ray.initial_state, dynamics, termination,
                                                  settings, integrator, correction));
    }
    return outcomes;
}

} // namespace Rays

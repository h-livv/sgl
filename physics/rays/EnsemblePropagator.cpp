#include "EnsemblePropagator.h"

#include <cstddef>

namespace Rays {

RayOutcomes propagate_ensemble(const RayEnsemble& ensemble,
                               const Dynamics::DynamicsModel& dynamics,
                               const Propagation::TerminationPolicy& termination,
                               const Propagation::IntegrationSettings& settings,
                               const Integration::Integrator& integrator,
                               const Propagation::StepCorrection& correction) {
    const std::size_t n = ensemble.size();
    RayOutcomes outcomes(n);
#if defined(_OPENMP)
    // Independent geodesics; dynamic schedule because step counts vary. Inner
    // propagate() is serial — no nested parallelism.
#pragma omp parallel for schedule(dynamic) if (n > 1)
#endif
    for (std::size_t i = 0; i < n; ++i) {
        outcomes[i] = Propagation::propagate(ensemble.at(i).initial_state, dynamics, termination,
                                             settings, integrator, correction);
    }
    return outcomes;
}

} // namespace Rays

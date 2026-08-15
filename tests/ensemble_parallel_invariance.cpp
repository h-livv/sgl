#include "support/Check.h"

#include <integrators/RK4Integrator.h>
#include <rays/EnsemblePropagator.h>
#include <rays/RaySampler.h>
#include <schwarzschild/PropagationContext.h>
#include <validation/observables/SchwarzschildObservables.h>

#include <cstddef>

#if defined(_OPENMP)
#include <omp.h>
#endif

// Rays: OpenMP invariance of EnsemblePropagator.
// Contract: two serial runs match bit-for-bit, including previous_state;
//           1 thread vs 4 threads match (no shared writable integrator state).
// Pipeline: rays. previous_state is compared because arrivals interpolate with it.
// Caveat: the 1-vs-4 check is compiled out when OpenMP is absent; a serial
//         build can still pass without testing parallel invariance.

namespace {

bool equal_state_bits(const State& a, const State& b) {
    return a.X[0] == b.X[0] && a.X[1] == b.X[1] && a.X[2] == b.X[2] && a.X[3] == b.X[3] &&
           a.U[0] == b.U[0] && a.U[1] == b.U[1] && a.U[2] == b.U[2] && a.U[3] == b.U[3];
}

bool equal_outcome_bits(const Propagation::PropagationOutcome& a,
                        const Propagation::PropagationOutcome& b) {
    return equal_state_bits(a.final_state, b.final_state) && a.steps_taken == b.steps_taken &&
           a.status == b.status && equal_state_bits(a.previous_state, b.previous_state);
}

Schwarzschild::PropagationOptions make_options() {
    Schwarzschild::PropagationOptions options;
    options.horizon_safety_factor = 1.0001;
    options.escape_radius = 100.0;
    options.null_constraint_projection = true;
    options.null_projection_interval = 1000;
    return options;
}

} // namespace

int main() {
    const Spacetime::SchwarzschildParameters params{.rs = 1.0};
    const Problem::PropagationProblem problem =
        Problem::make_aligned_problem(params, 30.0, 30.0, 5.0, 5.0);

    const double b_crit = Physics::Observables::critical_impact_parameter(params.rs);
    const Rays::RaySampler sampler(Rays::RaySamplingConfig{
        .ray_count = 5, .min_impact_parameter = b_crit + 0.1, .max_impact_parameter = b_crit + 2.0});
    const Rays::RayEnsemble ensemble = sampler.sample(problem);

    const Schwarzschild::PropagationOptions options = make_options();
    Schwarzschild::PropagationContext context(params, options);
    Integration::RK4Integrator integrator;
    const Propagation::IntegrationSettings settings{.step_size = 0.01, .max_steps = 200000};

    const Rays::RayOutcomes first = Rays::propagate_ensemble(
        ensemble, context.dynamics(), context.termination(), settings, integrator,
        context.correction());
    CHECK(first.size() == ensemble.size(), "outcome count matches ray count");
    for (std::size_t i = 0; i < ensemble.size(); ++i) {
        CHECK(ensemble.at(i).id == i, "outcome index aligns with ray id");
    }

    const Rays::RayOutcomes second = Rays::propagate_ensemble(
        ensemble, context.dynamics(), context.termination(), settings, integrator,
        context.correction());
    CHECK(second.size() == first.size(), "repeated run size matches");
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(equal_outcome_bits(second[i], first[i]), "repeated run is bitwise identical");
    }

#if defined(_OPENMP)
    omp_set_num_threads(1);
    const Rays::RayOutcomes serial = Rays::propagate_ensemble(
        ensemble, context.dynamics(), context.termination(), settings, integrator,
        context.correction());
    // Same ensemble, different thread count: results must not depend on scheduling.
    omp_set_num_threads(4);
    const Rays::RayOutcomes parallel = Rays::propagate_ensemble(
        ensemble, context.dynamics(), context.termination(), settings, integrator,
        context.correction());
    CHECK(parallel.size() == serial.size(), "1-thread and 4-thread sizes match");
    for (std::size_t i = 0; i < serial.size(); ++i) {
        CHECK(equal_outcome_bits(parallel[i], serial[i]),
              "1-thread and 4-thread outcomes are bitwise identical");
    }
#endif

    return TestSupport::report();
}

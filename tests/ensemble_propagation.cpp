#include "support/Check.h"

#include <integrators/RK4Integrator.h>
#include <rays/EnsemblePropagator.h>
#include <rays/RaySampler.h>
#include <schwarzschild/PropagationContext.h>
#include <validation/observables/SchwarzschildObservables.h>

#include <cstddef>

// Rays: EnsemblePropagator vs the single-ray kernel.
// Contract: one-ray ensemble is bitwise equal to propagate(); outcome i belongs
//           to ray id i; repeated / fresh-context runs match; mutating ray 2
//           changes only outcome 2; empty ensemble yields no outcomes.
// Pipeline: rays. Does not cover OpenMP (see ensemble_parallel_invariance.cpp).
// Caveat: sampled b > b_crit so every ray hits the escape radius.

namespace {

bool equal_state_bits(const State& a, const State& b) {
    return a.X[0] == b.X[0] && a.X[1] == b.X[1] && a.X[2] == b.X[2] && a.X[3] == b.X[3] &&
           a.U[0] == b.U[0] && a.U[1] == b.U[1] && a.U[2] == b.U[2] && a.U[3] == b.U[3];
}

bool equal_outcome_bits(const Propagation::PropagationOutcome& a,
                        const Propagation::PropagationOutcome& b) {
    return equal_state_bits(a.final_state, b.final_state) && a.steps_taken == b.steps_taken &&
           a.status == b.status;
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

    // Single-ray ensemble reproduces the low-level kernel bitwise.
    Rays::RayEnsemble single;
    single.add(ensemble.at(0).initial_state);
    const Rays::RayOutcomes single_outcomes = Rays::propagate_ensemble(
        single, context.dynamics(), context.termination(), settings, integrator,
        context.correction());
    const Propagation::PropagationOutcome direct =
        Propagation::propagate(ensemble.at(0).initial_state, context.dynamics(),
                               context.termination(), settings, integrator, context.correction());
    CHECK(single_outcomes.size() == 1, "single-ray ensemble yields one outcome");
    CHECK(equal_outcome_bits(single_outcomes[0], direct),
          "single-ray ensemble matches kernel bitwise");

    // Multiple rays: every ray is propagated.
    const Rays::RayOutcomes outcomes = Rays::propagate_ensemble(
        ensemble, context.dynamics(), context.termination(), settings, integrator,
        context.correction());
    CHECK(outcomes.size() == ensemble.size(), "outcome count matches ray count");
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        CHECK(outcomes[i].status == Propagation::PropagationStatus::Terminated,
              "every ray terminated");
        CHECK(outcomes[i].steps_taken > 0, "every ray advanced");
        CHECK(outcomes[i].final_state.X[1] >= options.escape_radius,
              "every sampled ray escaped");
        CHECK(ensemble.at(i).id == i, "outcome index aligns with ray id");
    }

    // Determinism across repeated runs.
    const Rays::RayOutcomes repeated = Rays::propagate_ensemble(
        ensemble, context.dynamics(), context.termination(), settings, integrator,
        context.correction());
    CHECK(repeated.size() == outcomes.size(), "repeated run size matches");
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        CHECK(equal_outcome_bits(repeated[i], outcomes[i]), "repeated run is bitwise identical");
    }

    // Determinism with a freshly constructed context.
    Schwarzschild::PropagationContext fresh_context(params, options);
    const Rays::RayOutcomes fresh_outcomes = Rays::propagate_ensemble(
        ensemble, fresh_context.dynamics(), fresh_context.termination(), settings, integrator,
        fresh_context.correction());
    CHECK(fresh_outcomes.size() == outcomes.size(), "fresh context size matches");
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        CHECK(equal_outcome_bits(fresh_outcomes[i], outcomes[i]),
              "fresh context is bitwise identical");
    }

    // Independence: replacing one ray changes only that ray's outcome.
    Rays::RayEnsemble modified = ensemble;
    modified.set_initial_state(2, ensemble.at(0).initial_state);
    const Rays::RayOutcomes modified_outcomes = Rays::propagate_ensemble(
        modified, context.dynamics(), context.termination(), settings, integrator,
        context.correction());
    CHECK(modified_outcomes.size() == outcomes.size(), "modified ensemble size unchanged");
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        if (i == 2) {
            continue;
        }
        CHECK(equal_outcome_bits(modified_outcomes[i], outcomes[i]),
              "untouched rays produce identical outcomes");
    }
    CHECK(!equal_outcome_bits(modified_outcomes[2], outcomes[2]),
          "modified ray produces a different outcome");
    CHECK(equal_outcome_bits(modified_outcomes[2], outcomes[0]),
          "modified ray reproduces the outcome of the state it was given");

    // Empty ensemble.
    const Rays::RayEnsemble empty_ensemble;
    const Rays::RayOutcomes empty_outcomes = Rays::propagate_ensemble(
        empty_ensemble, context.dynamics(), context.termination(), settings, integrator,
        context.correction());
    CHECK(empty_outcomes.empty(), "empty ensemble yields no outcomes");

    return TestSupport::report();
}

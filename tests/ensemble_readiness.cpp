#include "integrators/RK4Integrator.h"
#include "propagation/Propagator.h"
#include "schwarzschild/InitialStates.h"
#include "schwarzschild/PropagationContext.h"
#include "support/Check.h"
#include "validation/observables/SchwarzschildObservables.h"

#include <array>
#include <vector>

// Kernel: PropagationOutcome as a per-ray value type for later ensembles.
// Contract: b < b_crit captures (horizon), b > b_crit escapes; a second run and
//           a fresh PropagationContext are bitwise identical (no hidden state).
// Pipeline: kernel, immediately before EnsemblePropagator. Still serial
//           propagate() calls, not the ensemble wrapper.
// Caveat: equality is bit-exact, not a relative tolerance.

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

std::vector<Propagation::PropagationOutcome> run_ensemble(const Spacetime::SchwarzschildParameters& params) {
    Schwarzschild::PropagationOptions options;
    options.horizon_safety_factor = 1.0001;
    options.escape_radius = 100.0;
    options.null_constraint_projection = true;
    options.null_projection_interval = 1000;
    Schwarzschild::PropagationContext context(params, options);

    const double b_crit = Physics::Observables::critical_impact_parameter(params.rs);
    // Sign of (b - b_crit) splits capture from escape; ±0.1 is a near-critical pair.
    const std::array<double, 5> offsets{-0.5, -0.1, 0.1, 0.5, 2.0};

    Integration::RK4Integrator integrator;
    Propagation::IntegrationSettings settings{.step_size = 0.01, .max_steps = 200000};

    std::vector<Propagation::PropagationOutcome> outcomes;
    outcomes.reserve(offsets.size());

    for (double offset : offsets) {
        Schwarzschild::NullScatterInitialConditions initial;
        initial.r0 = 30.0;
        initial.impact_parameter = b_crit + offset;
        const State initial_state = Schwarzschild::build_null_scatter(params, initial);
        const Propagation::PropagationOutcome outcome =
            Propagation::propagate(initial_state, context.dynamics(), context.termination(), settings,
                                   integrator, context.correction());
        outcomes.push_back(outcome);

        CHECK(outcome.status == Propagation::PropagationStatus::Terminated, "ensemble ray did not terminate");
        if (offset < 0.0) {
            CHECK(outcome.final_state.X[1] <= params.rs * options.horizon_safety_factor,
                  "captured ray did not hit horizon");
        } else {
            CHECK(outcome.final_state.X[1] >= options.escape_radius,
                  "escaping ray did not reach escape radius");
        }
    }

    return outcomes;
}

} // namespace

int main() {
    Spacetime::SchwarzschildParameters params{.rs = 1.0};

    const std::vector<Propagation::PropagationOutcome> first = run_ensemble(params);
    const std::vector<Propagation::PropagationOutcome> second = run_ensemble(params);

    CHECK(first.size() == second.size(), "ensemble size mismatch");
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(equal_outcome_bits(first[i], second[i]), "repeated run mismatch");
    }

    Schwarzschild::PropagationOptions options;
    options.horizon_safety_factor = 1.0001;
    options.escape_radius = 100.0;
    options.null_constraint_projection = true;
    options.null_projection_interval = 1000;
    const double b_crit = Physics::Observables::critical_impact_parameter(params.rs);
    const std::array<double, 5> offsets{-0.5, -0.1, 0.1, 0.5, 2.0};
    Integration::RK4Integrator integrator;
    Propagation::IntegrationSettings settings{.step_size = 0.01, .max_steps = 200000};

    std::vector<Propagation::PropagationOutcome> fresh_context_results;
    fresh_context_results.reserve(offsets.size());
    for (double offset : offsets) {
        Schwarzschild::PropagationContext fresh_context(params, options);
        Schwarzschild::NullScatterInitialConditions initial;
        initial.r0 = 30.0;
        initial.impact_parameter = b_crit + offset;
        const State initial_state = Schwarzschild::build_null_scatter(params, initial);
        fresh_context_results.push_back(Propagation::propagate(initial_state, fresh_context.dynamics(),
                                                               fresh_context.termination(), settings,
                                                               integrator, fresh_context.correction()));
    }

    CHECK(first.size() == fresh_context_results.size(), "fresh context size mismatch");
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(equal_outcome_bits(first[i], fresh_context_results[i]), "fresh context mismatch");
    }

    return TestSupport::report();
}

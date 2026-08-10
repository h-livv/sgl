#include "integrators/RK4Integrator.h"
#include "propagation/Propagator.h"
#include "reference/Baseline.h"
#include "schwarzschild/InitialStates.h"
#include "schwarzschild/PropagationContext.h"
#include "support/Check.h"
#include "validation/observables/SchwarzschildObservables.h"

#include <cmath>
#include <vector>

namespace {

void check_state(const State& actual, const Baseline::Case& expected, double rel_tol) {
    CHECK_CLOSE(actual.X[0], expected.X[0], rel_tol, "X0 mismatch");
    CHECK_CLOSE(actual.X[1], expected.X[1], rel_tol, "X1 mismatch");
    CHECK_CLOSE(actual.X[2], expected.X[2], rel_tol, "X2 mismatch");
    CHECK_CLOSE(actual.X[3], expected.X[3], rel_tol, "X3 mismatch");
    CHECK_CLOSE(actual.U[0], expected.U[0], rel_tol, "U0 mismatch");
    CHECK_CLOSE(actual.U[1], expected.U[1], rel_tol, "U1 mismatch");
    CHECK_CLOSE(actual.U[2], expected.U[2], rel_tol, "U2 mismatch");
    CHECK_CLOSE(actual.U[3], expected.U[3], rel_tol, "U3 mismatch");
}

} // namespace

int main() {
    Spacetime::SchwarzschildParameters params{.rs = 1.0};
    Schwarzschild::PropagationOptions options;
    options.horizon_safety_factor = 1.0001;
    options.null_constraint_projection = true;
    options.null_projection_interval = 1000;
    Schwarzschild::PropagationContext context(params, options);

    Schwarzschild::NullScatterInitialConditions initial;
    initial.r0 = 30.0;
    initial.impact_parameter = Physics::Observables::critical_impact_parameter(params.rs) + 0.5;
    State initial_state = Schwarzschild::build_null_scatter(params, initial);

    Propagation::IntegrationSettings settings{.step_size = 0.001, .max_steps = 50000};
    Integration::RK4Integrator integrator;
    std::vector<State> path;
    const Propagation::PropagationOutcome outcome =
        Propagation::propagate_recorded(initial_state, context.dynamics(), context.termination(),
                                        settings, integrator, path, context.correction());

    CHECK(static_cast<int>(path.size()) == Baseline::null_scatter.steps + 1, "path size mismatch");
    CHECK(outcome.steps_taken == Baseline::null_scatter.steps, "steps mismatch");
    CHECK(outcome.status == Propagation::PropagationStatus::StepBudgetExhausted, "status mismatch");
    check_state(outcome.final_state, Baseline::null_scatter, 1e-12);

    const double E0 = Physics::Observables::conserved_energy(path.front(), params.rs);
    const double L0 = Physics::Observables::conserved_angular_momentum(path.front());
    const double Ef = Physics::Observables::conserved_energy(path.back(), params.rs);
    const double Lf = Physics::Observables::conserved_angular_momentum(path.back());
    CHECK(std::abs((Ef - E0) / E0) < 1e-12, "energy drift too high");
    CHECK(std::abs((Lf - L0) / L0) < 1e-12, "angular momentum drift too high");

    CHECK(Physics::Observables::null_hamiltonian_error(path.back(), params.rs) < 1e-10,
          "null Hamiltonian error too high");

    return TestSupport::report();
}

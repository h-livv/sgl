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

void check_state(const State& actual, const Baseline::Case& expected, double rel_tol, const char* label) {
    CHECK_CLOSE(actual.X[0], expected.X[0], rel_tol, label);
    CHECK_CLOSE(actual.X[1], expected.X[1], rel_tol, label);
    CHECK_CLOSE(actual.X[2], expected.X[2], rel_tol, label);
    CHECK_CLOSE(actual.X[3], expected.X[3], rel_tol, label);
    CHECK_CLOSE(actual.U[0], expected.U[0], rel_tol, label);
    CHECK_CLOSE(actual.U[1], expected.U[1], rel_tol, label);
    CHECK_CLOSE(actual.U[2], expected.U[2], rel_tol, label);
    CHECK_CLOSE(actual.U[3], expected.U[3], rel_tol, label);
}

} // namespace

int main() {
    Spacetime::SchwarzschildParameters params{.rs = 1.0};
    Integration::RK4Integrator integrator;

    {
        Schwarzschild::PropagationContext context(params, {});
        Schwarzschild::BoundOrbitInitialConditions initial;
        const State initial_state = Schwarzschild::build_bound_orbit(params, initial);
        Propagation::IntegrationSettings settings{.step_size = 0.01, .max_steps = 100000};
        std::vector<State> path;
        const Propagation::PropagationOutcome outcome =
            Propagation::propagate_recorded(initial_state, context.dynamics(), context.termination(),
                                            settings, integrator, path, context.correction());
        CHECK(outcome.steps_taken == Baseline::bound_orbit.steps, "bound steps mismatch");
        CHECK(outcome.status == Propagation::PropagationStatus::StepBudgetExhausted, "bound status mismatch");
        check_state(outcome.final_state, Baseline::bound_orbit, 1e-12, "bound final mismatch");

        const double E0 = Physics::Observables::conserved_energy(path.front(), params.rs);
        const double L0 = Physics::Observables::conserved_angular_momentum(path.front());
        const double Ef = Physics::Observables::conserved_energy(path.back(), params.rs);
        const double Lf = Physics::Observables::conserved_angular_momentum(path.back());
        CHECK(std::abs((Ef - E0) / E0) < 1e-9, "bound energy drift too high");
        CHECK(std::abs((Lf - L0) / L0) < 1e-9, "bound momentum drift too high");
        for (const State& state : path) {
            CHECK(state.X[1] > params.rs, "bound orbit crossed horizon");
            CHECK(state.X[1] < 20.0, "bound orbit exceeded radius bound");
        }
    }

    {
        Schwarzschild::PropagationOptions options;
        options.horizon_safety_factor = 1.0001;
        Schwarzschild::PropagationContext context(params, options);
        Schwarzschild::RadialFreefallInitialConditions initial;
        initial.r0 = 10.0;
        const State initial_state = Schwarzschild::build_radial_freefall(params, initial);
        Propagation::IntegrationSettings settings{.step_size = 0.001, .max_steps = 100000};
        const Propagation::PropagationOutcome outcome =
            Propagation::propagate(initial_state, context.dynamics(), context.termination(), settings,
                                   integrator, context.correction());
        CHECK(outcome.steps_taken == Baseline::radial_freefall.steps, "radial steps mismatch");
        CHECK(outcome.status == Propagation::PropagationStatus::Terminated, "radial status mismatch");
        check_state(outcome.final_state, Baseline::radial_freefall, 1e-12, "radial final mismatch");
        CHECK(outcome.final_state.X[1] <= params.rs * options.horizon_safety_factor,
              "radial final radius above horizon threshold");
    }

    {
        Schwarzschild::PropagationOptions options;
        options.horizon_safety_factor = 1.0001;
        Schwarzschild::PropagationContext context(params, options);
        Schwarzschild::CustomInitialConditions initial;
        initial.r0 = 10.0;
        initial.vr = -1.0;
        initial.vphi = 0.02;
        initial.vt = 0.0;
        const State initial_state = Schwarzschild::build_custom(params, initial, Schwarzschild::GeodesicKind::Null);
        const double H0 = Physics::Observables::null_hamiltonian(initial_state, params.rs);
        const double scale = std::abs(initial_state.U[0] * initial_state.U[0]) +
                             std::abs(initial_state.U[1] * initial_state.U[1]) +
                             std::abs(initial_state.X[1] * initial_state.X[1] * initial_state.U[3] *
                                      initial_state.U[3]) +
                             1e-12;
        CHECK(std::abs(H0) / scale < 1e-12, "custom null initial normalization mismatch");

        Propagation::IntegrationSettings settings{.step_size = 0.001, .max_steps = 20000};
        const Propagation::PropagationOutcome outcome =
            Propagation::propagate(initial_state, context.dynamics(), context.termination(), settings,
                                   integrator, context.correction());
        CHECK(outcome.steps_taken == Baseline::custom_null.steps, "custom steps mismatch");
        CHECK(outcome.status == Propagation::PropagationStatus::Terminated, "custom status mismatch");
        check_state(outcome.final_state, Baseline::custom_null, 1e-12, "custom final mismatch");
    }

    return TestSupport::report();
}

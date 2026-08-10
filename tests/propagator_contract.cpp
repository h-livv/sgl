#include "core/GeodesicState.h"
#include "propagation/Propagator.h"
#include "support/Check.h"

#include <Eigen/Dense>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {

class FreeMotionDynamics final : public Dynamics::DynamicsModel {
public:
    State compute_derivative(const State& state) const override {
        return State(state.U, Eigen::Vector4d::Zero());
    }
};

class NeverTerminate final : public Propagation::TerminationPolicy {
public:
    bool should_terminate(const State&) const override { return false; }
};

class RadiusTerminate final : public Propagation::TerminationPolicy {
public:
    bool should_terminate(const State& state) const override { return state.X[1] > 1.0; }
};

class ImmediateTerminate final : public Propagation::TerminationPolicy {
public:
    bool should_terminate(const State&) const override { return true; }
};

class EulerIntegrator final : public Integration::Integrator {
public:
    State step(const State& state, double dt, const Integration::DerivativeFunc& derivative) const override {
        return state + derivative(state) * dt;
    }
};

void check_state_equal(const State& a, const State& b, double rel_tol, const char* message) {
    CHECK_CLOSE(a.X[0], b.X[0], rel_tol, message);
    CHECK_CLOSE(a.X[1], b.X[1], rel_tol, message);
    CHECK_CLOSE(a.X[2], b.X[2], rel_tol, message);
    CHECK_CLOSE(a.X[3], b.X[3], rel_tol, message);
    CHECK_CLOSE(a.U[0], b.U[0], rel_tol, message);
    CHECK_CLOSE(a.U[1], b.U[1], rel_tol, message);
    CHECK_CLOSE(a.U[2], b.U[2], rel_tol, message);
    CHECK_CLOSE(a.U[3], b.U[3], rel_tol, message);
}

} // namespace

int main() {
    static_assert(std::is_copy_constructible_v<Propagation::PropagationOutcome>);
    static_assert(std::is_trivially_copyable_v<Propagation::IntegrationSettings>);

    FreeMotionDynamics dynamics;
    NeverTerminate never_terminate;
    RadiusTerminate radius_terminate;
    ImmediateTerminate immediate_terminate;
    EulerIntegrator integrator;

    const State initial(Eigen::Vector4d::Zero(), Eigen::Vector4d(1.0, 0.5, 0.0, 0.0));
    const Propagation::IntegrationSettings settings{.step_size = 0.01, .max_steps = 1000};
    auto [step_size, max_steps] = settings;
    CHECK_CLOSE(step_size, 0.01, 0.0, "structured binding step_size mismatch");
    CHECK(max_steps == 1000, "structured binding max_steps mismatch");

    const Propagation::PropagationOutcome basic =
        Propagation::propagate(initial, dynamics, never_terminate, settings, integrator);
    auto [final_state, steps_taken, status] = basic;
    CHECK(steps_taken == 1000, "step count mismatch");
    CHECK(status == Propagation::PropagationStatus::StepBudgetExhausted, "status mismatch");
    CHECK_CLOSE(final_state.X[0], initial.X[0] + 10.0, 1e-12, "x0 propagation mismatch");
    CHECK_CLOSE(final_state.X[1], initial.X[1] + 5.0, 1e-12, "x1 propagation mismatch");
    CHECK_CLOSE(final_state.U[0], initial.U[0], 1e-15, "u0 changed");
    CHECK_CLOSE(final_state.U[1], initial.U[1], 1e-15, "u1 changed");

    std::vector<State> path;
    const Propagation::PropagationOutcome recorded =
        Propagation::propagate_recorded(initial, dynamics, never_terminate, settings, integrator, path);
    CHECK(static_cast<int>(path.size()) == recorded.steps_taken + 1, "recorded path size mismatch");
    CHECK(path.back().X == recorded.final_state.X, "path/final X mismatch");
    CHECK(path.back().U == recorded.final_state.U, "path/final U mismatch");
    check_state_equal(recorded.final_state, basic.final_state, 1e-15, "recorded final mismatch");
    CHECK(recorded.steps_taken == basic.steps_taken, "recorded step count mismatch");
    CHECK(recorded.status == basic.status, "recorded status mismatch");

    std::vector<State> prefilled(2, initial);
    const std::size_t prefilled_size = prefilled.size();
    const Propagation::PropagationOutcome appended = Propagation::propagate_recorded(
        initial, dynamics, never_terminate, settings, integrator, prefilled);
    CHECK(prefilled.size() == prefilled_size + static_cast<std::size_t>(appended.steps_taken + 1),
          "append semantics mismatch");

    const Propagation::PropagationOutcome terminated =
        Propagation::propagate(initial, dynamics, radius_terminate, settings, integrator);
    CHECK(terminated.status == Propagation::PropagationStatus::Terminated, "termination status mismatch");
    CHECK(terminated.steps_taken < settings.max_steps, "termination did not stop early");

    std::vector<State> immediate_path;
    const Propagation::PropagationOutcome immediate =
        Propagation::propagate_recorded(initial, dynamics, immediate_terminate, settings, integrator,
                                        immediate_path);
    CHECK(immediate.status == Propagation::PropagationStatus::Terminated, "immediate status mismatch");
    CHECK(immediate.steps_taken == 0, "immediate steps mismatch");
    CHECK(immediate_path.size() == 1, "immediate path size mismatch");

    const Propagation::IntegrationSettings zero_budget{.step_size = 0.01, .max_steps = 0};
    const Propagation::PropagationOutcome zero =
        Propagation::propagate(initial, dynamics, never_terminate, zero_budget, integrator);
    CHECK(zero.steps_taken == 0, "zero budget steps mismatch");
    CHECK(zero.status == Propagation::PropagationStatus::StepBudgetExhausted, "zero budget status mismatch");

    bool threw = false;
    try {
        const Propagation::IntegrationSettings bad{.step_size = 0.0, .max_steps = 10};
        (void)Propagation::propagate(initial, dynamics, never_terminate, bad, integrator);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw, "expected throw for step_size=0");

    threw = false;
    try {
        const Propagation::IntegrationSettings bad{.step_size = std::numeric_limits<double>::quiet_NaN(),
                                                   .max_steps = 10};
        (void)Propagation::propagate(initial, dynamics, never_terminate, bad, integrator);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw, "expected throw for step_size=nan");

    threw = false;
    try {
        const Propagation::IntegrationSettings bad{.step_size = std::numeric_limits<double>::infinity(),
                                                   .max_steps = 10};
        (void)Propagation::propagate(initial, dynamics, never_terminate, bad, integrator);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw, "expected throw for step_size=inf");

    threw = false;
    try {
        const Propagation::IntegrationSettings bad{.step_size = 0.01, .max_steps = -1};
        (void)Propagation::propagate(initial, dynamics, never_terminate, bad, integrator);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw, "expected throw for max_steps<0");

    const Propagation::IntegrationSettings reverse_settings{.step_size = -0.01, .max_steps = 1000};
    const Propagation::PropagationOutcome reverse =
        Propagation::propagate(initial, dynamics, never_terminate, reverse_settings, integrator);
    CHECK(reverse.status == Propagation::PropagationStatus::StepBudgetExhausted, "reverse status mismatch");
    CHECK_CLOSE(reverse.final_state.X[0], initial.X[0] - 10.0, 1e-12, "reverse x0 mismatch");
    CHECK_CLOSE(reverse.final_state.X[1], initial.X[1] - 5.0, 1e-12, "reverse x1 mismatch");

    int correction_calls = 0;
    Propagation::StepCorrection correction = [&correction_calls](State& state, int step_index) {
        if (step_index % 2 == 0) {
            state.U[2] = 0.0;
            ++correction_calls;
        }
    };
    std::vector<State> correction_path;
    (void)Propagation::propagate_recorded(initial, dynamics, never_terminate,
                                          Propagation::IntegrationSettings{.step_size = 0.1, .max_steps = 10},
                                          integrator, correction_path, correction);
    CHECK(correction_calls == 5, "correction call count mismatch");
    for (const State& state : correction_path) {
        CHECK_CLOSE(state.U[2], 0.0, 0.0, "correction did not set U2");
    }

    return TestSupport::report();
}

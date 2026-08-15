#include "Propagator.h"

#include <cmath>
#include <stdexcept>

namespace Propagation {
namespace {

void validate_settings(const IntegrationSettings& settings) {
    // Negative step_size (backward in λ) is allowed; only 0 / NaN / Inf are rejected.
    if (!std::isfinite(settings.step_size) || settings.step_size == 0.0) {
        throw std::invalid_argument("propagate: step_size must be finite and non-zero");
    }
    if (settings.max_steps < 0) {
        throw std::invalid_argument("propagate: max_steps must be >= 0");
    }
}

PropagationOutcome run(const State& initial_state, const Dynamics::DynamicsModel& dynamics,
                       const TerminationPolicy& termination, const IntegrationSettings& settings,
                       const Integration::Integrator& integrator, std::vector<State>* path,
                       const StepCorrection& correction) {
    validate_settings(settings);

    State current = initial_state;
    State previous = initial_state;
    if (path != nullptr) {
        path->reserve(path->size() + static_cast<std::size_t>(settings.max_steps) + 1);
        path->push_back(current);
    }

    int steps_taken = 0;
    PropagationStatus status = PropagationStatus::StepBudgetExhausted;

    const Integration::DerivativeFunc derivative = [&dynamics](const State& state) {
        return dynamics.compute_derivative(state);
    };

    // Termination is tested before stepping. Already-terminated start →
    // steps_taken = 0 and previous_state == final_state. max_steps == 0 skips
    // the loop (Exhausted, no termination test). After each accepted step,
    // optional StepCorrection runs. The first state that satisfies termination
    // is kept as final_state; the last budgeted step is not re-tested.
    for (int i = 0; i < settings.max_steps; ++i) {
        if (termination.should_terminate(current)) {
            status = PropagationStatus::Terminated;
            break;
        }

        previous = current;
        current = integrator.step(current, settings.step_size, derivative);
        if (correction) {
            correction(current, i);
        }
        if (path != nullptr) {
            path->push_back(current);
        }
        steps_taken = i + 1;
    }

    return PropagationOutcome{.final_state = current,
                              .steps_taken = steps_taken,
                              .status = status,
                              .previous_state = previous};
}

} // namespace

PropagationOutcome propagate(const State& initial_state, const Dynamics::DynamicsModel& dynamics,
                             const TerminationPolicy& termination,
                             const IntegrationSettings& settings,
                             const Integration::Integrator& integrator,
                             const StepCorrection& correction) {
    return run(initial_state, dynamics, termination, settings, integrator, nullptr, correction);
}

PropagationOutcome propagate_recorded(const State& initial_state,
                                      const Dynamics::DynamicsModel& dynamics,
                                      const TerminationPolicy& termination,
                                      const IntegrationSettings& settings,
                                      const Integration::Integrator& integrator,
                                      std::vector<State>& path,
                                      const StepCorrection& correction) {
    return run(initial_state, dynamics, termination, settings, integrator, &path, correction);
}

} // namespace Propagation

#pragma once

#include <core/GeodesicState.h>

namespace Propagation {

// Why the integrator stopped. Terminated: should_terminate was true at the
// start of an iteration (including before any step). StepBudgetExhausted:
// max_steps were taken without that check succeeding. The last budgeted step
// is not re-tested, so a bound crossed on that step still reports Exhausted.
enum class PropagationStatus {
    Terminated,
    StepBudgetExhausted,
};

// Endpoint of a geodesic integration. The imaging path calls propagate() and
// keeps only these two states — the full trajectory is not stored. Arrivals
// interpolates between previous_state and final_state to locate a plane hit.
struct PropagationOutcome {
    State final_state{};
    int steps_taken = 0;
    PropagationStatus status = PropagationStatus::StepBudgetExhausted;
    // State one step before final_state. Together with final_state it brackets the
    // condition that stopped propagation. Equals final_state when steps_taken == 0.
    State previous_state{};
};

} // namespace Propagation

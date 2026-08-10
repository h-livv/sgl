#pragma once

#include <core/GeodesicState.h>

namespace Propagation {

enum class PropagationStatus {
    Terminated,
    StepBudgetExhausted,
};

struct PropagationOutcome {
    State final_state{};
    int steps_taken = 0;
    PropagationStatus status = PropagationStatus::StepBudgetExhausted;
    // State one step before final_state. Together with final_state it brackets the
    // condition that stopped propagation. Equals final_state when steps_taken == 0.
    State previous_state{};
};

} // namespace Propagation

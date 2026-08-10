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
};

} // namespace Propagation

#include "PropagationContext.h"

#include <algorithm>
#include <cmath>

namespace Schwarzschild {

PropagationContext::PropagationContext(const Spacetime::SchwarzschildParameters& parameters,
                                       const PropagationOptions& options)
    : metric_(parameters.rs),
      dynamics_(metric_),
      termination_(parameters.rs * options.horizon_safety_factor, options.escape_radius) {
    if (!options.null_constraint_projection) {
        return;
    }

    const int interval = std::max(1, options.null_projection_interval);
    const double rs = parameters.rs;
    correction_ = [rs, interval](State& state, int step_index) {
        if (state.X[1] <= rs) {
            return;
        }
        if (step_index % interval == 0) {
            project_onto_null_cone(state, rs);
        }
        if (state.U[0] < 0.0) {
            state.U[0] = std::abs(state.U[0]);
        }
    };
}

} // namespace Schwarzschild

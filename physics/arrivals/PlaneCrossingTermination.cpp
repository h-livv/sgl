#include "PlaneCrossingTermination.h"

#include "ChartMapping.h"

namespace Arrivals {

PlaneCrossingTermination::PlaneCrossingTermination(const Geometry::Lens& lens,
                                                   const Geometry::ImagePlane& plane,
                                                   const Propagation::TerminationPolicy& fallback)
    : lens_(lens), plane_(plane), fallback_(fallback) {}

bool PlaneCrossingTermination::should_terminate(const State& state) const {
    // Fallback first so capture/escape still win on the same sample as a
    // plane crossing. Position-only map: cheap enough for every RK4 step.
    if (fallback_.should_terminate(state)) {
        return true;
    }
    return plane_.signed_distance(world_position(lens_, state)) >= 0.0;
}

} // namespace Arrivals

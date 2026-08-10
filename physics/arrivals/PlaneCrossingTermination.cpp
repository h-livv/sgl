#include "PlaneCrossingTermination.h"

#include "ChartMapping.h"

namespace Arrivals {

PlaneCrossingTermination::PlaneCrossingTermination(const Geometry::Lens& lens,
                                                   const Geometry::ImagePlane& plane,
                                                   const Propagation::TerminationPolicy& fallback)
    : lens_(lens), plane_(plane), fallback_(fallback) {}

bool PlaneCrossingTermination::should_terminate(const State& state) const {
    if (fallback_.should_terminate(state)) {
        return true;
    }
    return plane_.signed_distance(world_position(lens_, state)) >= 0.0;
}

} // namespace Arrivals

#pragma once

#include <geometry/ImagePlane.h>
#include <geometry/Lens.h>
#include <propagation/TerminationPolicy.h>

namespace Arrivals {

// Stops geodesic integration at the observer plane (or fallback).
// Fallback (horizon / escape) is tested first; then signed_distance(world_pos) >= 0.
// The plane is infinite: this is a crossing test, not an observer-hit test.
// Stateless and const: one instance is safely shared by every ray in an ensemble.
class PlaneCrossingTermination : public Propagation::TerminationPolicy {
public:
    // fallback is stored by reference and must outlive this object.
    PlaneCrossingTermination(const Geometry::Lens& lens, const Geometry::ImagePlane& plane,
                             const Propagation::TerminationPolicy& fallback);

    bool should_terminate(const State& state) const override;

private:
    Geometry::Lens lens_;
    Geometry::ImagePlane plane_;
    const Propagation::TerminationPolicy& fallback_;
};

} // namespace Arrivals

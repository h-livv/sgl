#pragma once

#include <geometry/ImagePlane.h>
#include <geometry/Lens.h>
#include <propagation/TerminationPolicy.h>

namespace Arrivals {

// Terminates when the ray reaches or passes the observer plane, or when the
// fallback policy fires (horizon capture, escape radius, and so on).
// Stateless and const: one instance is safely shared by every ray in an ensemble.
class PlaneCrossingTermination : public Propagation::TerminationPolicy {
public:
    // fallback must outlive this object.
    PlaneCrossingTermination(const Geometry::Lens& lens, const Geometry::ImagePlane& plane,
                             const Propagation::TerminationPolicy& fallback);

    bool should_terminate(const State& state) const override;

private:
    Geometry::Lens lens_;
    Geometry::ImagePlane plane_;
    const Propagation::TerminationPolicy& fallback_;
};

} // namespace Arrivals

#pragma once

#include <core/SchwarzschildParameters.h>
#include <geometry/ImagePlane.h>
#include <geometry/Lens.h>
#include <geometry/Observer.h>
#include <geometry/Source.h>

namespace Problem {

class PropagationProblem {
public:
    PropagationProblem(const Geometry::Lens& lens, const Geometry::Source& source,
                       const Geometry::Observer& observer,
                       const Geometry::ImagePlane& image_plane);

    const Geometry::Lens& lens() const { return lens_; }
    const Geometry::Source& source() const { return source_; }
    const Geometry::Observer& observer() const { return observer_; }
    const Geometry::ImagePlane& image_plane() const { return image_plane_; }

    double source_distance() const;
    double observer_distance() const;

private:
    Geometry::Lens lens_;
    Geometry::Source source_;
    Geometry::Observer observer_;
    Geometry::ImagePlane image_plane_;
};

PropagationProblem make_aligned_problem(const Spacetime::SchwarzschildParameters& parameters,
                                        double source_distance, double observer_distance,
                                        double half_width, double half_height);

} // namespace Problem

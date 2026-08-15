#pragma once

#include <core/SchwarzschildParameters.h>
#include <geometry/ImagePlane.h>
#include <geometry/Lens.h>
#include <geometry/Observer.h>
#include <geometry/Source.h>

namespace Problem {

// SGL problem geometry only (sgl_geometry): lens, source, observer, image plane.
// No geodesic state and no link to sgl_physics. Samplers consume this; the
// kernel first appears in sgl_rays.
class PropagationProblem {
public:
    // Rejects non-finite positions, source/observer inside the horizon, image-plane
    // normal not antiparallel to observer.forward, and plane origin off the view axis
    // (perpendicular offset). Does not require origin == observer.position.
    PropagationProblem(const Geometry::Lens& lens, const Geometry::Source& source,
                       const Geometry::Observer& observer,
                       const Geometry::ImagePlane& image_plane);

    const Geometry::Lens& lens() const { return lens_; }
    const Geometry::Source& source() const { return source_; }
    const Geometry::Observer& observer() const { return observer_; }
    const Geometry::ImagePlane& image_plane() const { return image_plane_; }

    // |source − lens|. Axial S in the aligned setup.
    double source_distance() const;
    // |observer − lens| (3D Euclidean). NOT CLI --observer-distance, which is the
    // transverse +X offset d. On axis this equals axial D (--observer-axial-distance).
    // Off axis it is hypot(D, d), neither D nor d.
    double observer_distance() const;

private:
    Geometry::Lens lens_;
    Geometry::Source source_;
    Geometry::Observer observer_;
    Geometry::ImagePlane image_plane_;
};

// On-axis factory: lens at origin, source at −S Z, observer at +D Z looking at
// the origin with up = +Y, plane attached_to. No transverse d — off-axis
// callers place the observer at D·Z + d·X themselves.
// The observer_distance argument is axial D, matching observer_distance() on axis,
// not CLI --observer-distance.
PropagationProblem make_aligned_problem(const Spacetime::SchwarzschildParameters& parameters,
                                        double source_distance, double observer_distance,
                                        double half_width, double half_height);

} // namespace Problem

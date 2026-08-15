#include "PropagationProblem.h"

#include <cmath>
#include <stdexcept>

namespace Problem {
namespace {

bool is_finite(const Eigen::Vector3d& v) {
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}

} // namespace

PropagationProblem::PropagationProblem(const Geometry::Lens& lens,
                                       const Geometry::Source& source,
                                       const Geometry::Observer& observer,
                                       const Geometry::ImagePlane& image_plane)
    : lens_(lens), source_(source), observer_(observer), image_plane_(image_plane) {
    if (!is_finite(lens_.position)) {
        throw std::invalid_argument("PropagationProblem: lens position must be finite");
    }
    if (!std::isfinite(lens_.parameters.rs) || lens_.parameters.rs <= 0.0) {
        throw std::invalid_argument("PropagationProblem: lens rs must be finite and positive");
    }

    if (!is_finite(source_.position)) {
        throw std::invalid_argument("PropagationProblem: source position must be finite");
    }
    const double source_radius = (source_.position - lens_.position).norm();
    if (source_radius <= lens_.parameters.rs) {
        throw std::invalid_argument("PropagationProblem: source must be outside the lens horizon");
    }

    const double observer_radius = (observer_.position() - lens_.position).norm();
    if (observer_radius <= lens_.parameters.rs) {
        throw std::invalid_argument(
            "PropagationProblem: observer must be outside the lens horizon");
    }

    // Light travels +Z into a plane whose normal is +Z; the camera looks −Z, so
    // normal · forward must be −1. attached_to satisfies this by construction.
    const double normal_dot_forward = image_plane_.normal().dot(observer_.forward());
    if (std::abs(normal_dot_forward + 1.0) > Geometry::kOrthonormalityTolerance) {
        throw std::invalid_argument(
            "PropagationProblem: image plane normal must be antiparallel to observer forward");
    }

    // Allow a shift of the plane along the view axis; reject a perpendicular offset.
    const Eigen::Vector3d origin_offset = image_plane_.origin() - observer_.position();
    const Eigen::Vector3d perp =
        origin_offset - origin_offset.dot(observer_.forward()) * observer_.forward();
    const double scale = std::max(1.0, origin_offset.norm());
    if (perp.norm() > Geometry::kOrthonormalityTolerance * scale) {
        throw std::invalid_argument(
            "PropagationProblem: image plane origin must lie on the observer optical axis");
    }
}

double PropagationProblem::source_distance() const {
    return (source_.position - lens_.position).norm();
}

double PropagationProblem::observer_distance() const {
    return (observer_.position() - lens_.position).norm();
}

// On-axis only: observer at +D Z, looking at the origin. No transverse d.
PropagationProblem make_aligned_problem(const Spacetime::SchwarzschildParameters& parameters,
                                        double source_distance, double observer_distance,
                                        double half_width, double half_height) {
    Geometry::Lens lens;
    lens.parameters = parameters;

    Geometry::Source source;
    source.position = -source_distance * Geometry::WorldFrame::optical_axis();

    const Eigen::Vector3d observer_position =
        observer_distance * Geometry::WorldFrame::optical_axis();
    Geometry::Observer observer = Geometry::Observer::looking_at(
        observer_position, Eigen::Vector3d::Zero(), Geometry::WorldFrame::plane_v_axis());

    Geometry::ImagePlane image_plane =
        Geometry::ImagePlane::attached_to(observer, half_width, half_height);

    return PropagationProblem(lens, source, observer, image_plane);
}

} // namespace Problem

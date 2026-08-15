#pragma once

// Test-side copy of the 1D observer-hit kernel in experiments/canonical_sgl_image.cpp.
// Not the production API — keep in sync with the experiment when that kernel changes.
// run_angular_pipeline is the 1D test entry: sample → collect_arrivals → scan
// residual_u(b) → bisection → select_primary_observer_hit.

#include <arrivals/ArrivalCollector.h>
#include <arrivals/ObserverAngularCoordinates.h>
#include <geometry/WorldFrame.h>
#include <integrators/RK4Integrator.h>
#include <metrics/CoordinateChart.h>
#include <problem/PropagationProblem.h>
#include <propagation/TerminationPolicy.h>
#include <rays/RayEnsemble.h>
#include <rays/RaySampler.h>
#include <schwarzschild/InitialConditions.h>
#include <schwarzschild/InitialStates.h>
#include <schwarzschild/PropagationContext.h>

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace AngularPipelineTest {

enum class RayModel {
    Point,
    Parallel,
};

// Scan sample: geodesic at impact parameter b; residual_u = plane u of intercept.
struct ObserverHit {
    double b = 0.0;
    Arrivals::RayArrival arrival;
    double residual_u = 0.0;
};

struct ObserverHitBracket {
    ObserverHit left;
    ObserverHit right;
    int index = 0;
};

struct ObserverHitCandidate {
    ObserverHit hit;
    Eigen::Vector2d angular_coordinate = Eigen::Vector2d::Zero();
    double angular_radius = 0.0;
    int bracket_index = 0;
    bool selected = false;
};

// Primary Einstein-ring root: smallest positive ρ, then smaller b.
struct SelectedObserverHit {
    ObserverHit hit;
    Eigen::Vector2d angular_coordinate = Eigen::Vector2d::Zero();
    double angular_radius = 0.0;
    int selected_bracket_index = -1;
    int candidate_count = 0;
    std::vector<ObserverHitCandidate> candidates;
};

inline double impact_parameter_at(int index, int ray_count, double b_min, double b_max) {
    if (ray_count == 1) {
        return b_min;
    }
    const double t = static_cast<double>(index) / static_cast<double>(ray_count - 1);
    return b_min + t * (b_max - b_min);
}

// Parallel bundle on z = −S, offset b along +X, aimed +Z (build_custom). Mirrors
// the experiment helper of the same name.
inline State make_parallel_null_state(const Geometry::Lens& lens, double source_distance, double b) {
    const Eigen::Vector3d world_position =
        -source_distance * Geometry::WorldFrame::optical_axis() +
        b * Geometry::WorldFrame::plane_u_axis();
    const Eigen::Vector3d chart_position = Geometry::to_chart_frame(lens, world_position);
    const Eigen::Vector3d chart_direction =
        Geometry::WorldFrame::world_to_chart(Geometry::WorldFrame::optical_axis());

    const State chart_cartesian(
        Eigen::Vector4d(0.0, chart_position.x(), chart_position.y(), chart_position.z()),
        Eigen::Vector4d(0.0, chart_direction.x(), chart_direction.y(), chart_direction.z()));
    const State spherical = CoordinateChart::cart_to_sphere(chart_cartesian);

    Schwarzschild::CustomInitialConditions initial;
    initial.t0 = 0.0;
    initial.r0 = spherical.X[1];
    initial.theta0 = spherical.X[2];
    initial.phi0 = spherical.X[3];
    initial.vt = 0.0;
    initial.vr = spherical.U[1];
    initial.vtheta = spherical.U[2];
    initial.vphi = spherical.U[3];
    return Schwarzschild::build_custom(lens.parameters, initial, Schwarzschild::GeodesicKind::Null);
}

inline Rays::RayEnsemble make_single_ray_ensemble(const Problem::PropagationProblem& problem,
                                                  RayModel ray_model, double source_distance,
                                                  double b) {
    if (ray_model == RayModel::Point) {
        const Rays::RaySampler sampler(Rays::RaySamplingConfig{
            .ray_count = 1, .min_impact_parameter = b, .max_impact_parameter = b});
        return sampler.sample(problem);
    }
    Rays::RayEnsemble ensemble;
    ensemble.add(make_parallel_null_state(problem.lens(), source_distance, b));
    return ensemble;
}

inline double residual_u_for_arrival(const Geometry::ImagePlane& plane,
                                     const Arrivals::RayArrival& arrival) {
    if (arrival.status != Arrivals::ArrivalStatus::Arrived) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return plane.to_plane_coordinates(arrival.world_position).x();
}

inline ObserverHit propagate_one_for_b(const Problem::PropagationProblem& problem,
                                       Schwarzschild::PropagationContext& context,
                                       const Propagation::RadiusBoundTermination& fallback,
                                       const Propagation::IntegrationSettings& settings,
                                       Integration::RK4Integrator& integrator, RayModel ray_model,
                                       double source_distance, double b) {
    const Rays::RayEnsemble ensemble =
        make_single_ray_ensemble(problem, ray_model, source_distance, b);
    const std::vector<Arrivals::RayArrival> arrivals = Arrivals::collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());
    ObserverHit hit;
    hit.b = b;
    hit.arrival = arrivals.empty() ? Arrivals::RayArrival{} : arrivals.front();
    hit.residual_u = residual_u_for_arrival(problem.image_plane(), hit.arrival);
    return hit;
}

inline bool is_bracketing_hit(const ObserverHit& left, const ObserverHit& right,
                              double observer_hit_tolerance) {
    if (!std::isfinite(left.residual_u) || !std::isfinite(right.residual_u)) {
        return false;
    }
    if (std::abs(left.residual_u) <= observer_hit_tolerance ||
        std::abs(right.residual_u) <= observer_hit_tolerance) {
        return true;
    }
    return (left.residual_u > 0.0 && right.residual_u < 0.0) ||
           (left.residual_u < 0.0 && right.residual_u > 0.0);
}

// Mirrors canonical_sgl_image: consecutive residual_u sign changes / near-zero.
inline std::vector<ObserverHitBracket> scan_observer_hit_brackets(
    const std::vector<ObserverHit>& hits, double observer_hit_tolerance) {
    std::vector<ObserverHitBracket> brackets;
    if (hits.empty()) {
        return brackets;
    }
    for (std::size_t i = 0; i + 1 < hits.size(); ++i) {
        if (!is_bracketing_hit(hits[i], hits[i + 1], observer_hit_tolerance)) {
            continue;
        }
        brackets.push_back(ObserverHitBracket{hits[i], hits[i + 1], static_cast<int>(i)});
    }
    if (hits.size() == 1 &&
        hits.front().arrival.status == Arrivals::ArrivalStatus::Arrived &&
        std::abs(hits.front().residual_u) <= observer_hit_tolerance) {
        brackets.push_back(ObserverHitBracket{hits.front(), hits.front(), 0});
    }
    return brackets;
}

// Mirrors canonical_sgl_image: bisection on residual_u(b).
inline ObserverHit refine_observer_hit_bisection(
    const ObserverHitBracket& bracket, const Problem::PropagationProblem& problem,
    Schwarzschild::PropagationContext& context,
    const Propagation::RadiusBoundTermination& fallback,
    const Propagation::IntegrationSettings& settings, Integration::RK4Integrator& integrator,
    RayModel ray_model, double source_distance, double observer_hit_tolerance,
    int max_root_iterations) {
    ObserverHit left = bracket.left;
    ObserverHit right = bracket.right;
    if (left.b == right.b) {
        return left;
    }
    for (int iteration = 0; iteration < max_root_iterations; ++iteration) {
        if (std::abs(left.residual_u) <= observer_hit_tolerance) {
            return left;
        }
        if (std::abs(right.residual_u) <= observer_hit_tolerance) {
            return right;
        }
        const double mid_b = 0.5 * (left.b + right.b);
        const ObserverHit mid =
            propagate_one_for_b(problem, context, fallback, settings, integrator, ray_model,
                                source_distance, mid_b);
        if (mid.arrival.status != Arrivals::ArrivalStatus::Arrived ||
            !std::isfinite(mid.residual_u)) {
            break;
        }
        if (std::abs(mid.residual_u) <= observer_hit_tolerance) {
            return mid;
        }
        if (is_bracketing_hit(left, mid, observer_hit_tolerance)) {
            right = mid;
        } else if (is_bracketing_hit(mid, right, observer_hit_tolerance)) {
            left = mid;
        } else {
            break;
        }
    }
    if (std::abs(left.residual_u) <= std::abs(right.residual_u)) {
        return left;
    }
    return right;
}

// Mirrors canonical_sgl_image: smallest positive ρ, tie-break smaller b.
inline SelectedObserverHit select_primary_observer_hit(
    const std::vector<ObserverHitBracket>& brackets, const Problem::PropagationProblem& problem,
    const Geometry::Observer& observer, Schwarzschild::PropagationContext& context,
    const Propagation::RadiusBoundTermination& fallback,
    const Propagation::IntegrationSettings& settings, Integration::RK4Integrator& integrator,
    RayModel ray_model, double source_distance, double observer_hit_tolerance,
    int max_root_iterations) {
    SelectedObserverHit selection;
    selection.candidate_count = static_cast<int>(brackets.size());

    constexpr double angular_radius_tie_tolerance = 1e-8;
    int best_index = -1;
    double best_radius = std::numeric_limits<double>::infinity();
    double best_b = std::numeric_limits<double>::infinity();

    for (const ObserverHitBracket& bracket : brackets) {
        ObserverHitCandidate candidate;
        candidate.bracket_index = bracket.index;
        candidate.hit = refine_observer_hit_bisection(
            bracket, problem, context, fallback, settings, integrator, ray_model, source_distance,
            observer_hit_tolerance, max_root_iterations);

        const std::optional<Eigen::Vector2d> angular =
            Arrivals::observer_angular_coordinates(candidate.hit.arrival, observer);
        if (!angular.has_value()) {
            candidate.angular_radius = std::numeric_limits<double>::quiet_NaN();
            selection.candidates.push_back(candidate);
            continue;
        }

        candidate.angular_coordinate = *angular;
        candidate.angular_radius = angular->norm();
        if (!std::isfinite(candidate.angular_radius) || candidate.angular_radius <= 0.0) {
            selection.candidates.push_back(candidate);
            continue;
        }

        const bool better_radius = candidate.angular_radius + angular_radius_tie_tolerance < best_radius;
        const bool tie_break =
            std::abs(candidate.angular_radius - best_radius) <= angular_radius_tie_tolerance &&
            candidate.hit.b < best_b;
        if (best_index < 0 || better_radius || tie_break) {
            best_index = static_cast<int>(selection.candidates.size());
            best_radius = candidate.angular_radius;
            best_b = candidate.hit.b;
        }
        selection.candidates.push_back(candidate);
    }

    if (best_index < 0) {
        return selection;
    }

    selection.candidates[static_cast<std::size_t>(best_index)].selected = true;
    const ObserverHitCandidate& chosen = selection.candidates[static_cast<std::size_t>(best_index)];
    selection.hit = chosen.hit;
    selection.angular_coordinate = chosen.angular_coordinate;
    selection.angular_radius = chosen.angular_radius;
    selection.selected_bracket_index = chosen.bracket_index;
    return selection;
}

// 1D test entry: point or parallel fan → plane arrivals → residual_u(b) brackets
// → bisection → primary observer hit. Does not form an image or reject off-axis
// geometry (those live in the experiment main).
inline SelectedObserverHit run_angular_pipeline(
    const Problem::PropagationProblem& problem, Schwarzschild::PropagationContext& context,
    const Propagation::RadiusBoundTermination& fallback,
    const Propagation::IntegrationSettings& settings, Integration::RK4Integrator& integrator,
    RayModel ray_model, double source_distance, int ray_count, double b_min, double b_max,
    double observer_hit_tolerance, int max_root_iterations) {
    Rays::RayEnsemble ensemble;
    if (ray_model == RayModel::Point) {
        const Rays::RaySampler sampler(Rays::RaySamplingConfig{
            .ray_count = ray_count, .min_impact_parameter = b_min, .max_impact_parameter = b_max});
        ensemble = sampler.sample(problem);
    } else {
        for (int i = 0; i < ray_count; ++i) {
            const double b = impact_parameter_at(i, ray_count, b_min, b_max);
            ensemble.add(make_parallel_null_state(problem.lens(), source_distance, b));
        }
    }

    const std::vector<Arrivals::RayArrival> arrivals = Arrivals::collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator, context.correction());

    std::vector<ObserverHit> scan_hits;
    scan_hits.reserve(arrivals.size());
    for (std::size_t i = 0; i < arrivals.size(); ++i) {
        const double b = impact_parameter_at(static_cast<int>(i), ray_count, b_min, b_max);
        scan_hits.push_back(ObserverHit{
            b, arrivals[i], residual_u_for_arrival(problem.image_plane(), arrivals[i])});
    }

    const std::vector<ObserverHitBracket> brackets =
        scan_observer_hit_brackets(scan_hits, observer_hit_tolerance);
    return select_primary_observer_hit(
        brackets, problem, problem.observer(), context, fallback, settings, integrator, ray_model,
        source_distance, observer_hit_tolerance, max_root_iterations);
}

} // namespace AngularPipelineTest

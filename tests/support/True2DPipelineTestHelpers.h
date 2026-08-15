#pragma once

// Test-side copy of the 2D launch-plane + Newton pipeline in
// experiments/true_2d_sgl_image.cpp. Not the production API.
// collect_observer_angular_samples is the 2D test entry. Unlike the 1D helpers,
// make_problem allows a transverse observer offset (CLI --observer-distance d).

#include <arrivals/ArrivalCollector.h>
#include <arrivals/ObserverAngularCoordinates.h>
#include <geometry/ImagePlane.h>
#include <geometry/Observer.h>
#include <geometry/Source.h>
#include <geometry/WorldFrame.h>
#include <integrators/RK4Integrator.h>
#include <problem/PropagationProblem.h>
#include <propagation/TerminationPolicy.h>
#include <rays/RayGrid2DSampler.h>
#include <schwarzschild/PropagationContext.h>
#include <arrivals/ObserverLaunchRefiner.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace True2DTest {

// One Gauss-Newton hit that reached the observer (launch b_u,b_v + gnomonic).
struct AcceptedAngularSample {
    Arrivals::RayArrival arrival;
    Eigen::Vector2d angular_coordinate = Eigen::Vector2d::Zero();
    Eigen::Vector2d plane_residual = Eigen::Vector2d::Zero();
    double b_u = 0.0;
    double b_v = 0.0;
    int iterations = 0;
};

struct True2DPipelineResult {
    Rays::RayEnsemble ensemble;
    std::vector<Arrivals::RayArrival> all_arrivals;
    std::vector<AcceptedAngularSample> accepted;
    std::vector<Eigen::Vector2d> angular_coordinates;
    std::size_t seed_count = 0;
};

// Lens at origin (rs=1), source at −S Z, observer at D·Z + d·X looking at the
// origin, plane attached. observer_transverse_u is CLI d, not |observer−lens|.
inline Problem::PropagationProblem make_problem(double source_distance, double observer_axial_distance,
                                                double observer_transverse_u, double half_extent) {
    Geometry::Lens lens;
    lens.parameters = Spacetime::SchwarzschildParameters{.rs = 1.0};

    Geometry::Source source;
    source.position = -source_distance * Geometry::WorldFrame::optical_axis();

    const Eigen::Vector3d observer_position =
        observer_axial_distance * Geometry::WorldFrame::optical_axis() +
        observer_transverse_u * Geometry::WorldFrame::plane_u_axis();
    const Geometry::Observer observer = Geometry::Observer::looking_at(
        observer_position, Eigen::Vector3d::Zero(), Geometry::WorldFrame::plane_v_axis());
    const Geometry::ImagePlane image_plane =
        Geometry::ImagePlane::attached_to(observer, half_extent, half_extent);

    return Problem::PropagationProblem(lens, source, observer, image_plane);
}

inline double plane_residual_norm(const Geometry::ImagePlane& plane,
                                  const Arrivals::RayArrival& arrival) {
    if (arrival.status != Arrivals::ArrivalStatus::Arrived) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return plane.to_plane_coordinates(arrival.world_position).norm();
}

// 2D test entry: RayGrid2DSampler search grid → collect_arrivals →
// observer_hit_seeds + refine_observer_launches. Search geodesics are not
// binned; accepted[] holds Newton hits. Does not form an image.
inline True2DPipelineResult collect_observer_angular_samples(
    const Problem::PropagationProblem& problem, const Rays::RayGrid2DSamplingConfig& sampling_config,
    const Arrivals::ObserverLaunchRefinementConfig& refinement,
    Schwarzschild::PropagationContext& context,
    const Propagation::RadiusBoundTermination& fallback,
    const Propagation::IntegrationSettings& settings, Integration::RK4Integrator& integrator) {
    True2DPipelineResult result;

    Rays::RayGrid2DSampler sampler(sampling_config);
    result.ensemble = sampler.sample(problem);
    result.all_arrivals = Arrivals::collect_arrivals(
        result.ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());

    result.seed_count = Arrivals::observer_hit_seeds(sampler.samples(), result.all_arrivals,
                                                     problem.image_plane(),
                                                     sampling_config.samples_per_axis)
                            .size();

    const std::vector<Arrivals::RefinedObserverHit> refined = Arrivals::refine_observer_launches(
        problem, sampler, result.all_arrivals, refinement, context, fallback, settings,
        integrator);

    for (const Arrivals::RefinedObserverHit& hit : refined) {
        AcceptedAngularSample sample;
        sample.arrival = hit.hit.arrival;
        sample.angular_coordinate = hit.angular_coordinate;
        sample.plane_residual = hit.hit.plane_residual;
        sample.b_u = hit.hit.b_u;
        sample.b_v = hit.hit.b_v;
        sample.iterations = hit.iterations;
        result.accepted.push_back(sample);
        result.angular_coordinates.push_back(hit.angular_coordinate);
    }

    return result;
}

// Analysis of refined observer gnomonic samples (not the search grid).
inline double median_radius(const std::vector<Eigen::Vector2d>& coordinates) {
    if (coordinates.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::vector<double> radii;
    radii.reserve(coordinates.size());
    for (const Eigen::Vector2d& coordinate : coordinates) {
        radii.push_back(coordinate.norm());
    }
    std::sort(radii.begin(), radii.end());
    const std::size_t mid = radii.size() / 2;
    if (radii.size() % 2 == 1) {
        return radii[mid];
    }
    return 0.5 * (radii[mid - 1] + radii[mid]);
}

// Alias of median_radius — ring size used by 2D tests.
inline double characteristic_radius(const std::vector<Eigen::Vector2d>& coordinates) {
    return median_radius(coordinates);
}

// Population stddev of ρ = ||(u_ang, v_ang)||.
inline double radial_stddev(const std::vector<Eigen::Vector2d>& coordinates) {
    if (coordinates.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::vector<double> radii;
    radii.reserve(coordinates.size());
    for (const Eigen::Vector2d& coordinate : coordinates) {
        radii.push_back(coordinate.norm());
    }
    const double mean =
        std::accumulate(radii.begin(), radii.end(), 0.0) / static_cast<double>(radii.size());
    double variance = 0.0;
    for (double radius : radii) {
        const double delta = radius - mean;
        variance += delta * delta;
    }
    variance /= static_cast<double>(radii.size());
    return std::sqrt(variance);
}

// Mean of refined gnomonic samples.
inline Eigen::Vector2d centroid(const std::vector<Eigen::Vector2d>& coordinates) {
    if (coordinates.empty()) {
        return Eigen::Vector2d::Zero();
    }
    Eigen::Vector2d sum = Eigen::Vector2d::Zero();
    for (const Eigen::Vector2d& coordinate : coordinates) {
        sum += coordinate;
    }
    return sum / static_cast<double>(coordinates.size());
}

// (λ_max − λ_min) / (λ_max + λ_min) of the 2×2 covariance of the samples.
// 0 is circular; approaching 1 is a line. Population (÷ n) moments.
inline double second_moment_anisotropy(const std::vector<Eigen::Vector2d>& coordinates) {
    if (coordinates.size() < 2) {
        return 0.0;
    }
    const Eigen::Vector2d mean = centroid(coordinates);
    double cxx = 0.0;
    double cyy = 0.0;
    double cxy = 0.0;
    for (const Eigen::Vector2d& coordinate : coordinates) {
        const Eigen::Vector2d delta = coordinate - mean;
        cxx += delta.x() * delta.x();
        cyy += delta.y() * delta.y();
        cxy += delta.x() * delta.y();
    }
    const double inv_n = 1.0 / static_cast<double>(coordinates.size());
    cxx *= inv_n;
    cyy *= inv_n;
    cxy *= inv_n;

    const double trace = cxx + cyy;
    const double det = cxx * cyy - cxy * cxy;
    const double discriminant = std::max(0.0, 0.25 * trace * trace - det);
    const double root = std::sqrt(discriminant);
    const double lambda_max = 0.5 * trace + root;
    const double lambda_min = 0.5 * trace - root;
    const double denom = lambda_max + lambda_min;
    if (denom <= 0.0) {
        return 0.0;
    }
    return (lambda_max - lambda_min) / denom;
}

} // namespace True2DTest

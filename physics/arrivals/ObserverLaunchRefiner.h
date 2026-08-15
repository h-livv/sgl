#pragma once

#include "RayArrival.h"

#include <integrators/Integrator.h>
#include <problem/PropagationProblem.h>
#include <propagation/IntegrationSettings.h>
#include <propagation/TerminationPolicy.h>
#include <rays/RayGrid2DSampler.h>
#include <schwarzschild/PropagationContext.h>

#include <optional>
#include <limits>
#include <vector>

namespace Arrivals {

// 2D observer-hit search. F(b_u, b_v) is the observer-plane residual of one
// geodesic; F = (0, 0) is an observer hit (not merely a plane crossing).
// Inner 1-ray propagates are serial; OpenMP is over seeds only.

struct LaunchHit {
    double b_u = 0.0;
    double b_v = 0.0;
    RayArrival arrival;
    Eigen::Vector2d plane_residual = Eigen::Vector2d::Constant(
        std::numeric_limits<double>::quiet_NaN());
};

struct RefinedObserverHit {
    LaunchHit hit;
    Eigen::Vector2d angular_coordinate = Eigen::Vector2d::Zero();
    int seed_index = -1;
    int iterations = 0;
};

struct ObserverLaunchRefinementConfig {
    double hit_tolerance = 1e-6;          // |plane residual| for an accepted hit
    int max_iterations = 12;
    double finite_difference_step = 1e-3; // launch-plane db for the Jacobian
};

// One (b_u, b_v) → 1-ray ensemble → collect_arrivals → plane residual.
// Residual is ImagePlane::to_plane_coordinates(arrival): (0, 0) is the
// observer, not the optical-axis foot. NaN residual if the ray never
// reached the plane.
LaunchHit evaluate_launch(const Problem::PropagationProblem& problem,
                          const Rays::RayGrid2DSampler& sampler, double b_u, double b_v,
                          Schwarzschild::PropagationContext& context,
                          const Propagation::RadiusBoundTermination& fallback,
                          const Propagation::IntegrationSettings& settings,
                          const Integration::Integrator& integrator);

// Seeds in launch (b_u, b_v), not an image-acceptance aperture:
// global best residual, 8-neighbour local minima, and edge interpolations
// where adjacent residuals point toward a closer miss. Unique (b_u, b_v).
// Empty if the grid is not a square samples_per_axis^2 aligned with arrivals.
std::vector<Eigen::Vector2d> observer_hit_seeds(const std::vector<Rays::RayGrid2DSample>& samples,
                                                const std::vector<RayArrival>& arrivals,
                                                const Geometry::ImagePlane& plane,
                                                int samples_per_axis);

// Damped Gauss-Newton on F(b_u, b_v) = plane residual. Finite-difference
// Jacobian (step finite_difference_step). Line search halves the step up to
// 6 times; Broyden update when a trial is accepted. Launches clamped to
// [−b_max, b_max]. Returns nullopt on failure (including a hit whose
// incoming direction is behind the camera). Inner 1-ray propagate is serial.
std::optional<RefinedObserverHit>
refine_launch_to_observer(const Problem::PropagationProblem& problem,
                          const Rays::RayGrid2DSampler& sampler, double b_u0, double b_v0,
                          const ObserverLaunchRefinementConfig& config,
                          Schwarzschild::PropagationContext& context,
                          const Propagation::RadiusBoundTermination& fallback,
                          const Propagation::IntegrationSettings& settings,
                          const Integration::Integrator& integrator, int seed_index);

// observer_hit_seeds, then OpenMP over seeds, sort by residual, drop
// duplicates within 0.25 × cell_width in launch space.
std::vector<RefinedObserverHit>
refine_observer_launches(const Problem::PropagationProblem& problem,
                         const Rays::RayGrid2DSampler& sampler,
                         const std::vector<RayArrival>& search_arrivals,
                         const ObserverLaunchRefinementConfig& config,
                         Schwarzschild::PropagationContext& context,
                         const Propagation::RadiusBoundTermination& fallback,
                         const Propagation::IntegrationSettings& settings,
                         const Integration::Integrator& integrator);

} // namespace Arrivals

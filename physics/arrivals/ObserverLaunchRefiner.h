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
    double hit_tolerance = 1e-6;
    int max_iterations = 12;
    double finite_difference_step = 1e-3;
};

// Propagate one launch-plane sample and return its observer-plane residual.
LaunchHit evaluate_launch(const Problem::PropagationProblem& problem,
                          const Rays::RayGrid2DSampler& sampler, double b_u, double b_v,
                          Schwarzschild::PropagationContext& context,
                          const Propagation::RadiusBoundTermination& fallback,
                          const Propagation::IntegrationSettings& settings,
                          const Integration::Integrator& integrator);

// Grid-search seeds: residual local minima and adjacent samples whose residual
// vectors lie in opposite half-planes. Not an image-acceptance aperture.
std::vector<Eigen::Vector2d> observer_hit_seeds(const std::vector<Rays::RayGrid2DSample>& samples,
                                                const std::vector<RayArrival>& arrivals,
                                                const Geometry::ImagePlane& plane,
                                                int samples_per_axis);

// Drive one seed to the observer origin by damped Gauss-Newton on (b_u, b_v).
std::optional<RefinedObserverHit>
refine_launch_to_observer(const Problem::PropagationProblem& problem,
                          const Rays::RayGrid2DSampler& sampler, double b_u0, double b_v0,
                          const ObserverLaunchRefinementConfig& config,
                          Schwarzschild::PropagationContext& context,
                          const Propagation::RadiusBoundTermination& fallback,
                          const Propagation::IntegrationSettings& settings,
                          const Integration::Integrator& integrator, int seed_index);

// Search on the existing 2D grid, refine each seed, drop duplicates.
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

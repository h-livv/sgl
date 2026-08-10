#include "RaySampler.h"

#include <geometry/Lens.h>
#include <metrics/CoordinateChart.h>
#include <schwarzschild/InitialConditions.h>
#include <schwarzschild/InitialStates.h>

#include <cmath>
#include <stdexcept>

namespace Rays {

RaySampler::RaySampler(const RaySamplingConfig& config) : config_(config) {
    if (config_.ray_count < 1) {
        throw std::invalid_argument("RaySampler: ray_count must be >= 1");
    }
    if (!std::isfinite(config_.min_impact_parameter) ||
        !std::isfinite(config_.max_impact_parameter)) {
        throw std::invalid_argument("RaySampler: impact parameter bounds must be finite");
    }
    if (config_.min_impact_parameter <= 0.0) {
        throw std::invalid_argument("RaySampler: min_impact_parameter must be positive");
    }
    if (config_.max_impact_parameter < config_.min_impact_parameter) {
        throw std::invalid_argument(
            "RaySampler: max_impact_parameter must be >= min_impact_parameter");
    }
    if (config_.ray_count > 1 && config_.max_impact_parameter == config_.min_impact_parameter) {
        throw std::invalid_argument(
            "RaySampler: max_impact_parameter must exceed min_impact_parameter for ray_count > 1");
    }
}

double RaySampler::impact_parameter_at(int index) const {
    if (index < 0 || index >= config_.ray_count) {
        throw std::out_of_range("RaySampler: ray index out of range");
    }
    if (config_.ray_count == 1) {
        return config_.min_impact_parameter;
    }
    const double t = static_cast<double>(index) / static_cast<double>(config_.ray_count - 1);
    return config_.min_impact_parameter +
           t * (config_.max_impact_parameter - config_.min_impact_parameter);
}

RayEnsemble RaySampler::sample(const Problem::PropagationProblem& problem) const {
    const Eigen::Vector3d source_chart =
        Geometry::to_chart_frame(problem.lens(), problem.source().position);
    const State chart_state(
        Eigen::Vector4d(0.0, source_chart.x(), source_chart.y(), source_chart.z()),
        Eigen::Vector4d::Zero());
    const State source_spherical = CoordinateChart::cart_to_sphere(chart_state);

    RayEnsemble ensemble;
    for (int i = 0; i < config_.ray_count; ++i) {
        Schwarzschild::NullScatterInitialConditions initial;
        initial.t0 = 0.0;
        initial.r0 = source_spherical.X[1];
        initial.theta0 = source_spherical.X[2];
        initial.phi0 = source_spherical.X[3];
        initial.impact_parameter = impact_parameter_at(i);
        ensemble.add(Schwarzschild::build_null_scatter(problem.lens().parameters, initial));
    }
    return ensemble;
}

} // namespace Rays

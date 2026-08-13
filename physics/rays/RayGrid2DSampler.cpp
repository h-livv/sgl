#include "RayGrid2DSampler.h"

#include <geometry/WorldFrame.h>
#include <metrics/CoordinateChart.h>
#include <schwarzschild/InitialConditions.h>
#include <schwarzschild/InitialStates.h>

#include <cmath>
#include <stdexcept>

namespace Rays {

RayGrid2DSampler::RayGrid2DSampler(const RayGrid2DSamplingConfig& config) : config_(config) {
    if (config_.samples_per_axis < 2) {
        throw std::invalid_argument("RayGrid2DSampler: samples_per_axis must be >= 2");
    }
    if (!std::isfinite(config_.max_impact_parameter) || config_.max_impact_parameter <= 0.0) {
        throw std::invalid_argument(
            "RayGrid2DSampler: max_impact_parameter must be finite and positive");
    }
}

double RayGrid2DSampler::grid_value_at(int index) const {
    if (index < 0 || index >= config_.samples_per_axis) {
        throw std::out_of_range("RayGrid2DSampler: grid index out of range");
    }
    const double cell_width =
        (2.0 * config_.max_impact_parameter) / static_cast<double>(config_.samples_per_axis);
    return -config_.max_impact_parameter + (static_cast<double>(index) + 0.5) * cell_width;
}

State RayGrid2DSampler::state_for(const Problem::PropagationProblem& problem, double b_u,
                                 double b_v) const {
    const Eigen::Vector3d world_position =
        problem.source().position + b_u * Geometry::WorldFrame::plane_u_axis() +
        b_v * Geometry::WorldFrame::plane_v_axis();
    const Eigen::Vector3d to_lens = problem.lens().position - problem.source().position;
    const double to_lens_norm = to_lens.norm();
    if (to_lens_norm <= Geometry::kOrthonormalityTolerance) {
        throw std::runtime_error("RayGrid2DSampler: source and lens positions must differ");
    }
    const Eigen::Vector3d world_direction = to_lens / to_lens_norm;

    const Eigen::Vector3d chart_position = Geometry::to_chart_frame(problem.lens(), world_position);
    const Eigen::Vector3d chart_direction =
        Geometry::WorldFrame::world_to_chart(world_direction);

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
    return Schwarzschild::build_custom(problem.lens().parameters, initial,
                                       Schwarzschild::GeodesicKind::Null);
}

RayEnsemble RayGrid2DSampler::sample(const Problem::PropagationProblem& problem) {
    samples_.clear();
    RayEnsemble ensemble;

    for (int j = 0; j < config_.samples_per_axis; ++j) {
        const double b_v = grid_value_at(j);
        for (int i = 0; i < config_.samples_per_axis; ++i) {
            const double b_u = grid_value_at(i);
            const State initial_state = state_for(problem, b_u, b_v);
            const std::size_t ray_id = ensemble.add(initial_state);
            samples_.push_back(RayGrid2DSample{b_u, b_v, ray_id});
        }
    }

    return ensemble;
}

} // namespace Rays

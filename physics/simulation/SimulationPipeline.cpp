#include "SimulationConfig.h"
#include "TerminationPolicy.h"
#include "TrajectorySolver.h"

#include "../geodesics/GeodesicDynamics.h"
#include "../metrics/SchwarzschildMetric.h"
#include "initial_conditions/SchwarzschildInitialStateBuilders.h"

#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>

namespace Simulation {
namespace {

void require_spacetime(const SimulationConfig& config, Scenario scenario) {
    if (config.spacetime != SpacetimeKind::Schwarzschild) {
        throw std::runtime_error("run_simulation: SGL physics currently supports Schwarzschild only");
    }
    if (config.scenario != scenario) {
        throw std::runtime_error(
            "run_simulation: initial-condition type does not match SimulationConfig::scenario");
    }
}

std::unique_ptr<Spacetime::Metric> make_schwarzschild_metric(
    const Spacetime::SchwarzschildParameters& params) {
    return std::make_unique<Spacetime::SchwarzschildMetric>(params.mass);
}

SimulationMetadata schwarzschild_metadata(const Spacetime::SchwarzschildParameters& metric) {
    SimulationMetadata metadata;
    metadata.metric = Spacetime::MetricKind::Schwarzschild;
    metadata.coordinate_chart = Spacetime::CoordinateChartKind::SchwarzschildSpherical;
    metadata.horizon_radius = metric.mass;
    metadata.photon_sphere_radius = 1.5 * metric.mass;
    return metadata;
}

std::function<void(State&, int)> make_schwarzschild_post_step(const SimulationConfig& config,
                                                              double rs) {
    if (!config.solver.null_constraint_projection) {
        return nullptr;
    }

    const int interval = std::max(1, config.solver.null_projection_interval);
    return [rs, interval](State& state, int step) {
        const double r = state.X[1];
        if (r <= rs) {
            return;
        }
        const double f = 1.0 - rs / r;
        const double vr = state.U[1];
        const double vth = state.U[2];
        const double vph = state.U[3];
        if (step % interval == 0) {
            const double spatial = vr * vr / f + r * r * vth * vth + r * r * std::sin(state.X[2]) *
                                                                         std::sin(state.X[2]) * vph * vph;
            state.U[0] = std::sqrt(spatial / f);
        }
        if (state.U[0] < 0.0) {
            state.U[0] = std::abs(state.U[0]);
        }
    };
}

SimulationResult integrate_schwarzschild(const SimulationConfig& config,
                                         std::unique_ptr<Spacetime::Metric> metric_impl,
                                         const Spacetime::SchwarzschildParameters& metric_params,
                                         const State& initial) {
    Dynamics::GeodesicDynamics dynamics(*metric_impl);
    HorizonTermination policy(metric_params.mass, config.horizon_safety_factor);

    SimulationResult result;
    result.history = TrajectorySolver::solve(
        initial, dynamics, policy, config.dt, config.max_steps, Integration::default_integrator(),
        make_schwarzschild_post_step(config, metric_params.mass));
    result.characteristic_radius = metric_params.mass;
    result.name = config.name;
    result.spacetime = config.spacetime;
    result.metadata = schwarzschild_metadata(metric_params);
    return result;
}

} // namespace

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const BoundOrbitInitialConditions& initial) {
    require_spacetime(config, Scenario::BoundOrbit);
    return integrate_schwarzschild(config, make_schwarzschild_metric(metric), metric,
                                   InitialStateBuilders::build_bound_orbit(metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const RadialFreefallInitialConditions& initial) {
    require_spacetime(config, Scenario::RadialFreefall);
    return integrate_schwarzschild(config, make_schwarzschild_metric(metric), metric,
                                   InitialStateBuilders::build_radial_freefall(metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const NullScatterInitialConditions& initial) {
    require_spacetime(config, Scenario::NullScatter);
    return integrate_schwarzschild(config, make_schwarzschild_metric(metric), metric,
                                   InitialStateBuilders::build_null_scatter(metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const CustomInitialConditions& initial) {
    require_spacetime(config, Scenario::Custom);
    return integrate_schwarzschild(config, make_schwarzschild_metric(metric), metric,
                                   InitialStateBuilders::build_custom(config, metric, initial));
}

} // namespace Simulation

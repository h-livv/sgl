/**
 * Minimal smoke test for the extracted Schwarzschild GR engine.
 * Not an SGL science experiment — only verifies the library builds and runs.
 */

#include "simulation/SimulationConfig.h"
#include "simulation/initial_conditions/InitialConditions.h"
#include "validation/observables/SchwarzschildObservables.h"

#include <cmath>
#include <iostream>

int main() {
    Simulation::SimulationConfig config;
    config.spacetime = Simulation::SpacetimeKind::Schwarzschild;
    config.scenario = Simulation::Scenario::NullScatter;
    config.geodesic = Simulation::GeodesicKind::Null;
    config.dt = 0.001;
    config.max_steps = 50000;
    config.horizon_safety_factor = 1.0001;
    config.solver.null_constraint_projection = true;
    config.name = "null_smoke";

    Spacetime::SchwarzschildParameters metric{.mass = 1.0};

    Simulation::NullScatterInitialConditions initial;
    initial.r0 = 30.0;
    initial.impact_parameter = Physics::Observables::critical_impact_parameter(metric.mass) + 0.5;

    const Simulation::SimulationResult result = Simulation::run_simulation(config, metric, initial);
    if (result.history.size() < 2) {
        std::cerr << "smoke test failed: empty trajectory\n";
        return 1;
    }

    const State& first = result.history.front();
    const State& last = result.history.back();
    const double E0 = Physics::Observables::conserved_energy(first, metric.mass);
    const double L0 = Physics::Observables::conserved_angular_momentum(first);
    const double Ef = Physics::Observables::conserved_energy(last, metric.mass);
    const double Lf = Physics::Observables::conserved_angular_momentum(last);

    std::cout << "sgl_null_smoke: steps=" << result.history.size()
              << " |dE/E|=" << std::abs((Ef - E0) / E0)
              << " |dL/L|=" << std::abs((Lf - L0) / L0) << "\n";

    if (std::abs((Ef - E0) / E0) > 1e-3 || std::abs((Lf - L0) / L0) > 1e-3) {
        std::cerr << "smoke test failed: conservation\n";
        return 2;
    }

    std::cout << "OK\n";
    return 0;
}

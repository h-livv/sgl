#include "support/Check.h"

#include <geometry/WorldFrame.h>
#include <problem/PropagationProblem.h>
#include <rays/RayGrid2DSampler.h>
#include <validation/observables/SchwarzschildObservables.h>

#include <cmath>
#include <limits>

namespace {

bool state_is_finite(const State& state) {
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(state.X[i]) || !std::isfinite(state.U[i])) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    constexpr double b_max = 20.0;
    constexpr int samples_per_axis = 5;
    constexpr double source_distance = 30.0;
    constexpr double observer_distance = 30.0;
    constexpr double half_extent = 0.4;

    const Problem::PropagationProblem problem = Problem::make_aligned_problem(
        Spacetime::SchwarzschildParameters{.rs = 1.0}, source_distance, observer_distance,
        half_extent, half_extent);

    const Rays::RayGrid2DSamplingConfig config{
        .samples_per_axis = samples_per_axis, .max_impact_parameter = b_max};
    Rays::RayGrid2DSampler sampler(config);

    CHECK_CLOSE(sampler.grid_value_at(0), -b_max + 0.5 * (2.0 * b_max / samples_per_axis), 1e-15,
                "first cell center");
    CHECK_CLOSE(sampler.grid_value_at(samples_per_axis - 1),
                b_max - 0.5 * (2.0 * b_max / samples_per_axis), 1e-15, "last cell center");

    const Rays::RayEnsemble ensemble = sampler.sample(problem);
    const std::vector<Rays::RayGrid2DSample>& samples = sampler.samples();

    CHECK(static_cast<int>(ensemble.size()) == samples_per_axis * samples_per_axis,
          "ensemble size is N^2");
    CHECK(static_cast<int>(samples.size()) == samples_per_axis * samples_per_axis,
          "sample metadata count matches ensemble size");

    for (std::size_t i = 0; i < ensemble.size(); ++i) {
        CHECK(ensemble.at(i).id == i, "ray id equals index");
        CHECK(samples[i].ray_id == i, "sample metadata ray id equals index");
    }

    CHECK_CLOSE(samples.front().b_u, sampler.grid_value_at(0), 1e-15, "first sample b_u");
    CHECK_CLOSE(samples.front().b_v, sampler.grid_value_at(0), 1e-15, "first sample b_v");

    const std::size_t last = samples.size() - 1;
    CHECK_CLOSE(samples[last].b_u, sampler.grid_value_at(samples_per_axis - 1), 1e-15,
                "last sample b_u");
    CHECK_CLOSE(samples[last].b_v, sampler.grid_value_at(samples_per_axis - 1), 1e-15,
                "last sample b_v");

    for (const Rays::Ray& ray : ensemble.rays()) {
        CHECK(state_is_finite(ray.initial_state), "initial state is finite");
        CHECK(Physics::Observables::null_hamiltonian_error(ray.initial_state, 1.0) <= 1e-10,
              "null Hamiltonian error within tolerance");
    }

    const Rays::RayEnsemble ensemble_repeat = sampler.sample(problem);
    CHECK(ensemble_repeat.size() == ensemble.size(), "deterministic resample size");
    for (std::size_t i = 0; i < ensemble.size(); ++i) {
        CHECK(ensemble.at(i).initial_state.X == ensemble_repeat.at(i).initial_state.X,
              "deterministic resample position");
        CHECK(ensemble.at(i).initial_state.U == ensemble_repeat.at(i).initial_state.U,
              "deterministic resample velocity");
    }

    return TestSupport::report();
}

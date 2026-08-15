#include "support/Check.h"

#include <rays/RaySampler.h>
#include <validation/observables/SchwarzschildObservables.h>

#include <cmath>
#include <limits>
#include <stdexcept>

// Rays: 1D RaySampler (equatorial impact-parameter sweep).
// Contract: ray_count states, id == index, linearly spaced b in (b_min, b_max],
//           shared start event at the source, L = b, U^θ = 0, inward U^r.
// Pipeline: rays, 1D imaging path. True 2D uses RayGrid2DSampler instead.
// Caveat: b_min must be > 0 and strictly less than b_max. Here b > b_crit so
//         later observer-plane tests see escaping photons.

namespace {

bool equal_state_bits(const State& a, const State& b) {
    return a.X[0] == b.X[0] && a.X[1] == b.X[1] && a.X[2] == b.X[2] && a.X[3] == b.X[3] &&
           a.U[0] == b.U[0] && a.U[1] == b.U[1] && a.U[2] == b.U[2] && a.U[3] == b.U[3];
}

void expect_invalid_config(const Rays::RaySamplingConfig& config) {
    try {
        (void)Rays::RaySampler(config);
        CHECK(false, "expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    }
}

} // namespace

int main() {
    const Spacetime::SchwarzschildParameters params{.rs = 1.0};
    const Problem::PropagationProblem problem =
        Problem::make_aligned_problem(params, 30.0, 30.0, 5.0, 5.0);

    const double b_crit = Physics::Observables::critical_impact_parameter(params.rs);
    // Slightly above b_crit: scatter/escape, not capture. All rays share (t,r,θ,φ).
    const Rays::RaySamplingConfig config{.ray_count = 5,
                                         .min_impact_parameter = b_crit + 0.1,
                                         .max_impact_parameter = b_crit + 2.0};
    const Rays::RaySampler sampler(config);
    const Rays::RayEnsemble ensemble = sampler.sample(problem);

    CHECK(ensemble.size() == 5, "sampled ensemble size matches ray_count");
    for (std::size_t i = 0; i < ensemble.size(); ++i) {
        CHECK(ensemble.at(i).id == i, "sampled ray id equals index");
    }

    for (std::size_t i = 0; i < ensemble.size(); ++i) {
        const State& state = ensemble.at(i).initial_state;
        CHECK_CLOSE(state.X[0], 0.0, 1e-15, "sampled t0");
        CHECK_CLOSE(state.X[1], problem.source_distance(), 1e-15, "sampled r0 equals source radius");
        CHECK_CLOSE(state.X[2], M_PI / 2.0, 1e-15, "sampled theta0 is chart equator");
        // The source sits at azimuth pi (mod 2 pi); atan2 may report it as -pi because
        // -source_distance * UnitZ() carries signed zeros into the chart x/y components.
        CHECK_CLOSE(std::cos(state.X[3]), -1.0, 1e-15, "sampled phi0 points along -x in the chart");
        CHECK_CLOSE(std::sin(state.X[3]), 0.0, 1e-15, "sampled phi0 lies on the chart x axis");
        CHECK(state.U[2] == 0.0, "sampled ray has no polar velocity");
        CHECK(state.U[1] < 0.0, "sampled ray is inward directed");

        const double L = Physics::Observables::conserved_angular_momentum(state);
        CHECK_CLOSE(L, sampler.impact_parameter_at(static_cast<int>(i)), 1e-12,
                    "angular momentum equals impact parameter");
    }

    CHECK_CLOSE(sampler.impact_parameter_at(0), config.min_impact_parameter, 1e-15,
                "first impact parameter is the minimum");
    CHECK_CLOSE(sampler.impact_parameter_at(4), config.max_impact_parameter, 1e-15,
                "last impact parameter is the maximum");
    const double spacing = sampler.impact_parameter_at(1) - sampler.impact_parameter_at(0);
    for (int i = 1; i < config.ray_count; ++i) {
        CHECK_CLOSE(sampler.impact_parameter_at(i) - sampler.impact_parameter_at(i - 1), spacing,
                    1e-12, "impact parameters are linearly spaced");
    }

    const Rays::RaySampler single_sampler(Rays::RaySamplingConfig{
        .ray_count = 1, .min_impact_parameter = b_crit + 0.5, .max_impact_parameter = b_crit + 0.5});
    const Rays::RayEnsemble single = single_sampler.sample(problem);
    CHECK(single.size() == 1, "single-ray sampling size");
    CHECK_CLOSE(single_sampler.impact_parameter_at(0), b_crit + 0.5, 1e-15,
                "single-ray impact parameter is the minimum");

    const Rays::RayEnsemble repeated = sampler.sample(problem);
    CHECK(repeated.size() == ensemble.size(), "repeated sampling size matches");
    for (std::size_t i = 0; i < ensemble.size(); ++i) {
        CHECK(equal_state_bits(repeated.at(i).initial_state, ensemble.at(i).initial_state),
              "repeated sampling is bitwise deterministic");
        CHECK(repeated.at(i).id == ensemble.at(i).id, "repeated sampling preserves ids");
    }

    expect_invalid_config(Rays::RaySamplingConfig{
        .ray_count = 0, .min_impact_parameter = 3.0, .max_impact_parameter = 4.0});
    expect_invalid_config(Rays::RaySamplingConfig{
        .ray_count = -1, .min_impact_parameter = 3.0, .max_impact_parameter = 4.0});
    expect_invalid_config(Rays::RaySamplingConfig{
        .ray_count = 5, .min_impact_parameter = 0.0, .max_impact_parameter = 4.0});
    expect_invalid_config(Rays::RaySamplingConfig{
        .ray_count = 5, .min_impact_parameter = -1.0, .max_impact_parameter = 4.0});
    expect_invalid_config(Rays::RaySamplingConfig{
        .ray_count = 5, .min_impact_parameter = 4.0, .max_impact_parameter = 3.0});
    expect_invalid_config(Rays::RaySamplingConfig{
        .ray_count = 5, .min_impact_parameter = 3.0, .max_impact_parameter = 3.0});
    expect_invalid_config(Rays::RaySamplingConfig{
        .ray_count = 5,
        .min_impact_parameter = 3.0,
        .max_impact_parameter = std::numeric_limits<double>::infinity()});

    try {
        (void)sampler.impact_parameter_at(-1);
        CHECK(false, "expected std::out_of_range for negative index");
    } catch (const std::out_of_range&) {
    }
    try {
        (void)sampler.impact_parameter_at(config.ray_count);
        CHECK(false, "expected std::out_of_range for index past the end");
    } catch (const std::out_of_range&) {
    }

    return TestSupport::report();
}

#include "support/Check.h"

#include <arrivals/ArrivalCollector.h>
#include <integrators/RK4Integrator.h>
#include <propagation/TerminationPolicy.h>
#include <rays/RaySampler.h>
#include <schwarzschild/PropagationContext.h>
#include <validation/observables/SchwarzschildObservables.h>

#include <limits>

namespace {

bool vectors_close(const Eigen::Vector3d& actual, const Eigen::Vector3d& expected,
                   double rel_tol) {
    return TestSupport::close_rel(actual.x(), expected.x(), rel_tol) &&
           TestSupport::close_rel(actual.y(), expected.y(), rel_tol) &&
           TestSupport::close_rel(actual.z(), expected.z(), rel_tol);
}

bool arrivals_equal(const Arrivals::RayArrival& a, const Arrivals::RayArrival& b) {
    return a.ray_id == b.ray_id && a.status == b.status &&
           vectors_close(a.world_position, b.world_position, 1e-15) &&
           vectors_close(a.world_direction, b.world_direction, 1e-15) &&
           a.chart_state.X == b.chart_state.X && a.chart_state.U == b.chart_state.U;
}

} // namespace

int main() {
    const Spacetime::SchwarzschildParameters params{.rs = 1.0};
    const Problem::PropagationProblem problem =
        Problem::make_aligned_problem(params, 30.0, 30.0, 10.0, 10.0);
    const Geometry::ImagePlane& plane = problem.image_plane();

    Schwarzschild::PropagationOptions options;
    options.horizon_safety_factor = 1.0001;
    // Unbounded escape: a glancing ray can reach large Schwarzschild r before its world
    // trajectory crosses the observer plane; a finite escape radius would terminate it early.
    options.escape_radius = std::numeric_limits<double>::infinity();
    options.null_constraint_projection = true;
    options.null_projection_interval = 1000;
    Schwarzschild::PropagationContext context(params, options);

    const Propagation::RadiusBoundTermination fallback(
        params.rs * 1.0001, std::numeric_limits<double>::infinity());
    Integration::RK4Integrator integrator;
    const Propagation::IntegrationSettings settings{.step_size = 0.01, .max_steps = 200000};

    const double b_crit = Physics::Observables::critical_impact_parameter(params.rs);
    const Rays::RaySampler sampler(Rays::RaySamplingConfig{
        .ray_count = 3,
        .min_impact_parameter = b_crit + 0.5,
        .max_impact_parameter = b_crit + 2.0});
    const Rays::RayEnsemble ensemble = sampler.sample(problem);

    const std::vector<Arrivals::RayArrival> arrivals = Arrivals::collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());

    CHECK(arrivals.size() == 3, "arrival count matches ensemble");
    for (std::size_t i = 0; i < arrivals.size(); ++i) {
        CHECK(arrivals[i].status == Arrivals::ArrivalStatus::Arrived, "ray arrived");
        CHECK(arrivals[i].ray_id == i, "arrival ray id matches ensemble index");
        CHECK(std::abs(plane.signed_distance(arrivals[i].world_position)) <=
                  1e-9 * std::max(1.0, arrivals[i].world_position.norm()),
              "arrival lies on the plane");
        CHECK_CLOSE(arrivals[i].world_position.z(), 30.0, 1e-9, "arrival at observer plane z");
        CHECK_CLOSE(arrivals[i].world_direction.norm(), 1.0, 1e-12, "arrival direction unit");
        CHECK(arrivals[i].world_direction.dot(plane.normal()) > 0.0,
              "arrival direction crosses toward the plane");
    }

    const bool increasing_x = arrivals[0].world_position.x() < arrivals[1].world_position.x() &&
                              arrivals[1].world_position.x() < arrivals[2].world_position.x();
    const bool decreasing_x = arrivals[0].world_position.x() > arrivals[1].world_position.x() &&
                              arrivals[1].world_position.x() > arrivals[2].world_position.x();
    CHECK(increasing_x || decreasing_x, "arrival x ordering is monotonic with impact parameter");
    CHECK(!vectors_close(arrivals[0].world_position, arrivals[1].world_position, 1e-6),
          "distinct arrival positions 0 and 1");
    CHECK(!vectors_close(arrivals[1].world_position, arrivals[2].world_position, 1e-6),
          "distinct arrival positions 1 and 2");

    const std::vector<Arrivals::RayArrival> repeated = Arrivals::collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());
    CHECK(repeated.size() == arrivals.size(), "repeated arrival count");
    for (std::size_t i = 0; i < arrivals.size(); ++i) {
        CHECK(arrivals_equal(arrivals[i], repeated[i]), "repeated arrivals are bitwise identical");
    }

    const Rays::RaySampler captured_sampler(Rays::RaySamplingConfig{
        .ray_count = 1,
        .min_impact_parameter = b_crit - 0.5,
        .max_impact_parameter = b_crit - 0.5});
    const Rays::RayEnsemble captured_ensemble = captured_sampler.sample(problem);
    const std::vector<Arrivals::RayArrival> captured = Arrivals::collect_arrivals(
        captured_ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());
    CHECK(captured.size() == 1, "captured ensemble size");
    CHECK(captured[0].status == Arrivals::ArrivalStatus::NoCrossing, "captured ray no crossing");
    CHECK(vectors_close(captured[0].world_position, Eigen::Vector3d::Zero(), 1e-15),
          "captured position zeroed");
    CHECK(vectors_close(captured[0].world_direction, Eigen::Vector3d::Zero(), 1e-15),
          "captured direction zeroed");

    CHECK(arrivals.size() == ensemble.size(), "index alignment size");
    for (std::size_t i = 0; i < ensemble.size(); ++i) {
        CHECK(arrivals[i].ray_id == ensemble.at(i).id, "index alignment id");
    }

    return TestSupport::report();
}

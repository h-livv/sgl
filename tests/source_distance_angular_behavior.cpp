#include "support/AngularPipelineTestHelpers.h"
#include "support/Check.h"

#include <cmath>
#include <limits>
#include <vector>

namespace {

double equivalent_ring_radius(const AngularPipelineTest::SelectedObserverHit& selection,
                              double observer_axial_distance) {
    return observer_axial_distance * selection.angular_radius;
}

AngularPipelineTest::SelectedObserverHit run_case(double source_distance,
                                                  AngularPipelineTest::RayModel ray_model) {
    constexpr double observer_axial_distance = 30.0;
    constexpr double angular_extent = 0.8;
    constexpr int ray_count = 41;
    constexpr double b_min = 2.0;
    constexpr double b_max = 20.0;
    constexpr double step_size = 0.01;
    constexpr int max_steps = 300000;
    constexpr double observer_hit_tolerance = 1e-6;
    constexpr int max_root_iterations = 60;

    const Problem::PropagationProblem problem = Problem::make_aligned_problem(
        Spacetime::SchwarzschildParameters{.rs = 1.0}, source_distance, observer_axial_distance,
        angular_extent / 2.0, angular_extent / 2.0);

    Schwarzschild::PropagationOptions options;
    options.horizon_safety_factor = 1.0001;
    options.escape_radius = std::numeric_limits<double>::infinity();
    options.null_constraint_projection = true;
    options.null_projection_interval = 1000;
    Schwarzschild::PropagationContext context(Spacetime::SchwarzschildParameters{.rs = 1.0},
                                              options);

    const Propagation::RadiusBoundTermination fallback(
        1.0001, std::numeric_limits<double>::infinity());
    Integration::RK4Integrator integrator;
    const Propagation::IntegrationSettings settings{.step_size = step_size,
                                                    .max_steps = max_steps};

    return AngularPipelineTest::run_angular_pipeline(
        problem, context, fallback, settings, integrator, ray_model, source_distance, ray_count,
        b_min, b_max, observer_hit_tolerance, max_root_iterations);
}

} // namespace

int main() {
    constexpr double observer_axial_distance = 30.0;
    constexpr double ordering_margin = 1e-4;

    const AngularPipelineTest::SelectedObserverHit s50 =
        run_case(50.0, AngularPipelineTest::RayModel::Point);
    const AngularPipelineTest::SelectedObserverHit s100 =
        run_case(100.0, AngularPipelineTest::RayModel::Point);
    const AngularPipelineTest::SelectedObserverHit s200 =
        run_case(200.0, AngularPipelineTest::RayModel::Point);
    const AngularPipelineTest::SelectedObserverHit parallel =
        run_case(30.0, AngularPipelineTest::RayModel::Parallel);

    CHECK(s50.selected_bracket_index >= 0, "S=50 selected");
    CHECK(s100.selected_bracket_index >= 0, "S=100 selected");
    CHECK(s200.selected_bracket_index >= 0, "S=200 selected");
    CHECK(parallel.selected_bracket_index >= 0, "parallel selected");

    const double r50 = equivalent_ring_radius(s50, observer_axial_distance);
    const double r100 = equivalent_ring_radius(s100, observer_axial_distance);
    const double r200 = equivalent_ring_radius(s200, observer_axial_distance);
    const double r_parallel = equivalent_ring_radius(parallel, observer_axial_distance);

    CHECK(r50 > 0.0 && r100 > 0.0 && r200 > 0.0 && r_parallel > 0.0, "finite nonzero radii");
    CHECK(r100 > r50 + ordering_margin, "R_equiv increases from S=50 to S=100");
    CHECK(r200 > r100 + ordering_margin, "R_equiv increases from S=100 to S=200");

    const double diff_100 = std::abs(r_parallel - r100);
    const double diff_200 = std::abs(r_parallel - r200);
    CHECK(diff_200 <= diff_100 + ordering_margin,
          "finite S=200 approaches parallel limit monotonically");

    return TestSupport::report();
}

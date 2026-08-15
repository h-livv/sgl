#include "support/Check.h"

#include <arrivals/ArrivalCollector.h>
#include <arrivals/AzimuthalExpansion.h>
#include <integrators/RK4Integrator.h>
#include <problem/PropagationProblem.h>
#include <propagation/TerminationPolicy.h>
#include <rays/RaySampler.h>
#include <schwarzschild/PropagationContext.h>

#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

// Arrivals: expand_azimuthally — rotate spatial plane-u, not gnomonic u_ang.
// Contract: (u,0) → (u cos ψ, u sin ψ); NoCrossing dropped; |u| is the radius
//           (negative u kept at k = 0); residual v discarded; u = 0 emits one
//           origin point; bit-exact reruns.
// Pipeline: arrivals. The 1D imaging experiments use expand_angular_azimuthally
//           instead (observer_angular_coordinates.cpp / angular pipelines).
// Caveat: this is a spatial image-plane fill. Do not treat it as the Einstein-ring
//         picture path.

namespace {

Arrivals::RayArrival make_arrived(const Geometry::ImagePlane& plane, std::size_t ray_id,
                                  double u) {
    Arrivals::RayArrival arrival;
    arrival.ray_id = ray_id;
    arrival.world_position = plane.to_world(Eigen::Vector2d(u, 0.0));
    arrival.status = Arrivals::ArrivalStatus::Arrived;
    return arrival;
}

Arrivals::RayArrival make_no_crossing(std::size_t ray_id) {
    Arrivals::RayArrival arrival;
    arrival.ray_id = ray_id;
    arrival.status = Arrivals::ArrivalStatus::NoCrossing;
    return arrival;
}

bool plane_coords_close(const Eigen::Vector2d& actual, const Eigen::Vector2d& expected,
                        double rel_tol) {
    return TestSupport::close_rel(actual.x(), expected.x(), rel_tol) &&
           TestSupport::close_rel(actual.y(), expected.y(), rel_tol);
}

void expect_invalid_azimuth_count(const std::vector<Arrivals::RayArrival>& arrivals,
                                  const Geometry::ImagePlane& plane, int azimuth_count) {
    try {
        (void)Arrivals::expand_azimuthally(arrivals, plane, azimuth_count);
        CHECK(false, "expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    }
}

std::map<std::size_t, int> count_by_ray_id(const std::vector<Arrivals::PlaneArrival>& expanded) {
    std::map<std::size_t, int> counts;
    for (const Arrivals::PlaneArrival& entry : expanded) {
        ++counts[entry.ray_id];
    }
    return counts;
}

} // namespace

int main() {
    const Problem::PropagationProblem problem =
        Problem::make_aligned_problem(Spacetime::SchwarzschildParameters{.rs = 1.0}, 30.0, 30.0,
                                      100.0, 100.0);
    const Geometry::ImagePlane& plane = problem.image_plane();

    expect_invalid_azimuth_count({}, plane, 0);
    expect_invalid_azimuth_count({}, plane, -1);

    // Arbitrary in-plane radius; the formula is (u cos ψ, u sin ψ), independent of this value.
    const double u = 5.393;
    const std::vector<Arrivals::RayArrival> single_arrival{make_arrived(plane, 0, u)};
    const std::vector<Arrivals::PlaneArrival> identity =
        Arrivals::expand_azimuthally(single_arrival, plane, 1);
    CHECK(identity.size() == 1, "identity expansion size");
    CHECK(plane_coords_close(identity[0].plane_position, Eigen::Vector2d(u, 0.0), 1e-15),
          "identity expansion position");

    const std::vector<Arrivals::PlaneArrival> sixteen =
        Arrivals::expand_azimuthally(single_arrival, plane, 16);
    CHECK(sixteen.size() == 16, "sixteen expansion size");
    CHECK(plane_coords_close(sixteen[0].plane_position, Eigen::Vector2d(u, 0.0), 1e-15),
          "k=0 copy is exact");
    for (int k = 0; k < 16; ++k) {
        const double psi = 2.0 * M_PI * static_cast<double>(k) / 16.0;
        const Eigen::Vector2d expected(u * std::cos(psi), u * std::sin(psi));
        CHECK(plane_coords_close(sixteen[static_cast<std::size_t>(k)].plane_position, expected,
                                 1e-12),
              "explicit formula");
        CHECK_CLOSE(sixteen[static_cast<std::size_t>(k)].plane_position.norm(), std::abs(u),
                    1e-12, "radius preservation");
    }

    const std::vector<Arrivals::PlaneArrival> circle =
        Arrivals::expand_azimuthally(single_arrival, plane, 64);
    CHECK(circle.size() == 64, "circle expansion size");
    for (std::size_t i = 0; i < circle.size(); ++i) {
        for (std::size_t j = i + 1; j < circle.size(); ++j) {
            CHECK((circle[i].plane_position - circle[j].plane_position).norm() > 1e-9,
                  "circle points are pairwise distinct");
        }
    }
    Eigen::Vector2d mean = Eigen::Vector2d::Zero();
    for (const Arrivals::PlaneArrival& entry : circle) {
        mean += entry.plane_position;
    }
    mean /= static_cast<double>(circle.size());
    CHECK(mean.norm() <= 1e-12 * std::max(1.0, std::abs(u)), "circle mean near origin");
    for (std::size_t k = 0; k < circle.size(); ++k) {
        const std::size_t next = (k + 1) % circle.size();
        const double angle_k = std::atan2(circle[k].plane_position.y(), circle[k].plane_position.x());
        const double angle_next =
            std::atan2(circle[next].plane_position.y(), circle[next].plane_position.x());
        double delta = angle_next - angle_k;
        if (delta <= 0.0) {
            delta += 2.0 * M_PI;
        }
        CHECK_CLOSE(delta, 2.0 * M_PI / 64.0, 1e-12, "uniform angular spacing");
    }

    const std::vector<Arrivals::RayArrival> mixed{
        make_arrived(plane, 0, 1.0),
        make_arrived(plane, 1, 2.0),
        make_arrived(plane, 2, 3.0),
        make_no_crossing(3),
        make_no_crossing(4),
    };
    const std::vector<Arrivals::PlaneArrival> filtered =
        Arrivals::expand_azimuthally(mixed, plane, 8);
    CHECK(filtered.size() == 24, "filtered expansion size");
    const std::map<std::size_t, int> filtered_counts = count_by_ray_id(filtered);
    CHECK(filtered_counts.count(3) == 0, "no crossing id 3 absent");
    CHECK(filtered_counts.count(4) == 0, "no crossing id 4 absent");

    const std::vector<Arrivals::RayArrival> three_rays{
        make_arrived(plane, 0, 1.0),
        make_arrived(plane, 1, 2.0),
        make_arrived(plane, 2, 3.0),
    };
    const std::vector<Arrivals::PlaneArrival> associated =
        Arrivals::expand_azimuthally(three_rays, plane, 8);
    const std::map<std::size_t, int> associated_counts = count_by_ray_id(associated);
    CHECK(associated_counts.at(0) == 8, "ray 0 association count");
    CHECK(associated_counts.at(1) == 8, "ray 1 association count");
    CHECK(associated_counts.at(2) == 8, "ray 2 association count");

    const std::vector<Arrivals::RayArrival> two_radii{
        make_arrived(plane, 0, 5.393),
        make_arrived(plane, 1, -2.316),
    };
    const std::vector<Arrivals::PlaneArrival> radii_expanded =
        Arrivals::expand_azimuthally(two_radii, plane, 8);
    CHECK(radii_expanded.size() == 16, "two radii expansion size");
    for (std::size_t k = 0; k < 8; ++k) {
        CHECK_CLOSE(radii_expanded[k].plane_position.norm(), 5.393, 1e-12, "positive radius group");
        CHECK_CLOSE(radii_expanded[k + 8].plane_position.norm(), 2.316, 1e-12,
                    "negative radius group");
    }

    // Signed u: k = 0 stays on the negative-u axis; the circle radius is |u|.
    const std::vector<Arrivals::RayArrival> negative_u{make_arrived(plane, 0, -2.316)};
    const std::vector<Arrivals::PlaneArrival> negative_expanded =
        Arrivals::expand_azimuthally(negative_u, plane, 8);
    CHECK(negative_expanded[0].plane_position.x() < 0.0, "negative u preserved at k=0");
    for (const Arrivals::PlaneArrival& entry : negative_expanded) {
        CHECK_CLOSE(entry.plane_position.norm(), 2.316, 1e-12, "negative u radius preserved");
    }

    // Degenerate ring: one origin sample, not azimuth_count copies of (0,0).
    const std::vector<Arrivals::RayArrival> zero_radius{make_arrived(plane, 0, 0.0)};
    const std::vector<Arrivals::PlaneArrival> zero_expanded =
        Arrivals::expand_azimuthally(zero_radius, plane, 16);
    CHECK(zero_expanded.size() == 1, "zero radius emits one copy");
    CHECK(plane_coords_close(zero_expanded[0].plane_position, Eigen::Vector2d::Zero(), 1e-15),
          "zero radius position");

    // Tiny v is ignored: expansion uses signed u only (equatorial 1D residue).
    Arrivals::RayArrival residual_v = make_arrived(plane, 0, u);
    residual_v.world_position = plane.to_world(Eigen::Vector2d(u, 1e-15));
    const std::vector<Arrivals::PlaneArrival> residue_expanded =
        Arrivals::expand_azimuthally({residual_v}, plane, 16);
    for (const Arrivals::PlaneArrival& entry : residue_expanded) {
        CHECK_CLOSE(entry.plane_position.norm(), std::abs(u), 1e-12, "residual v discarded");
    }

    const std::vector<Arrivals::PlaneArrival> first = sixteen;
    const std::vector<Arrivals::PlaneArrival> second =
        Arrivals::expand_azimuthally(single_arrival, plane, 16);
    CHECK(first.size() == second.size(), "determinism size");
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(first[i].ray_id == second[i].ray_id, "determinism ray id");
        CHECK(first[i].plane_position.x() == second[i].plane_position.x(),
              "determinism x bit-exact");
        CHECK(first[i].plane_position.y() == second[i].plane_position.y(),
              "determinism y bit-exact");
    }

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
    const Propagation::IntegrationSettings settings{.step_size = 0.01, .max_steps = 300000};
    const Rays::RaySampler sampler(Rays::RaySamplingConfig{
        .ray_count = 5, .min_impact_parameter = 5.0, .max_impact_parameter = 8.0});
    const Rays::RayEnsemble ensemble = sampler.sample(problem);
    const std::vector<Arrivals::RayArrival> arrivals = Arrivals::collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());

    for (const Arrivals::RayArrival& arrival : arrivals) {
        CHECK(arrival.status == Arrivals::ArrivalStatus::Arrived, "e2e ray arrived");
        const Eigen::Vector2d coords = plane.to_plane_coordinates(arrival.world_position);
        CHECK(std::abs(coords.y()) <= 1e-12, "e2e equatorial v residue");
    }

    const std::vector<Arrivals::PlaneArrival> e2e_expanded =
        Arrivals::expand_azimuthally(arrivals, plane, 32);
    CHECK(e2e_expanded.size() == 160, "e2e expanded size");

    std::map<std::size_t, std::vector<double>> radii_by_ray;
    for (const Arrivals::PlaneArrival& entry : e2e_expanded) {
        radii_by_ray[entry.ray_id].push_back(entry.plane_position.norm());
    }
    CHECK(radii_by_ray.size() == 5, "e2e five radius groups");
    double min_radius = std::numeric_limits<double>::infinity();
    double max_radius = 0.0;
    bool has_nonzero_v = false;
    for (const auto& [ray_id, radii] : radii_by_ray) {
        (void)ray_id;
        CHECK(radii.size() == 32, "e2e group size");
        for (double radius : radii) {
            CHECK_CLOSE(radius, radii.front(), 1e-12, "e2e group radius consistency");
            min_radius = std::min(min_radius, radius);
            max_radius = std::max(max_radius, radius);
        }
    }
    CHECK(max_radius - min_radius > 0.5, "e2e radial structure preserved");
    for (const Arrivals::PlaneArrival& entry : e2e_expanded) {
        if (std::abs(entry.plane_position.y()) > 0.1) {
            has_nonzero_v = true;
            break;
        }
    }
    CHECK(has_nonzero_v, "e2e distribution leaves v=0 line");

    return TestSupport::report();
}

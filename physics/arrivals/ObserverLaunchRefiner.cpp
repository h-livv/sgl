#include "ObserverLaunchRefiner.h"

#include "ArrivalCollector.h"
#include "ObserverAngularCoordinates.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace Arrivals {
namespace {

constexpr double kResidualEps = 1e-18;

bool is_finite_vec(const Eigen::Vector2d& v) {
    return std::isfinite(v.x()) && std::isfinite(v.y());
}

Eigen::Vector2d plane_residual(const Geometry::ImagePlane& plane, const RayArrival& arrival) {
    if (arrival.status != ArrivalStatus::Arrived) {
        return Eigen::Vector2d::Constant(std::numeric_limits<double>::quiet_NaN());
    }
    // Observer-centered: ImagePlane origin is the observer, so (0, 0) is a
    // true observer hit. Off-axis observers do not need a residual offset.
    return plane.to_plane_coordinates(arrival.world_position);
}

int grid_index(int i, int j, int n) { return j * n + i; }

Eigen::Vector2d solve_gauss_newton(const Eigen::Matrix2d& jacobian, const Eigen::Vector2d& residual) {
    const Eigen::Matrix2d jtj = jacobian.transpose() * jacobian;
    const double lambda = 1e-12 * (1.0 + jtj.trace());
    Eigen::Matrix2d damped = jtj;
    damped(0, 0) += lambda;
    damped(1, 1) += lambda;
    return damped.ldlt().solve(-jacobian.transpose() * residual);
}

double launch_distance2(const Eigen::Vector2d& a, const Eigen::Vector2d& b) {
    return (a - b).squaredNorm();
}

} // namespace

LaunchHit evaluate_launch(const Problem::PropagationProblem& problem,
                          const Rays::RayGrid2DSampler& sampler, double b_u, double b_v,
                          Schwarzschild::PropagationContext& context,
                          const Propagation::RadiusBoundTermination& fallback,
                          const Propagation::IntegrationSettings& settings,
                          const Integration::Integrator& integrator) {
    LaunchHit hit;
    hit.b_u = b_u;
    hit.b_v = b_v;

    Rays::RayEnsemble ensemble;
    ensemble.add(sampler.state_for(problem, b_u, b_v));
    const std::vector<RayArrival> arrivals = collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());
    if (!arrivals.empty()) {
        hit.arrival = arrivals.front();
        hit.plane_residual = plane_residual(problem.image_plane(), hit.arrival);
    }
    return hit;
}

std::vector<Eigen::Vector2d> observer_hit_seeds(const std::vector<Rays::RayGrid2DSample>& samples,
                                                const std::vector<RayArrival>& arrivals,
                                                const Geometry::ImagePlane& plane,
                                                int samples_per_axis) {
    std::vector<Eigen::Vector2d> seeds;
    if (samples_per_axis < 2 ||
        samples.size() != static_cast<std::size_t>(samples_per_axis * samples_per_axis) ||
        arrivals.size() != samples.size()) {
        return seeds;
    }

    std::vector<Eigen::Vector2d> residuals(samples.size());
    std::vector<double> norms(samples.size(), std::numeric_limits<double>::infinity());
    for (std::size_t k = 0; k < samples.size(); ++k) {
        residuals[k] = plane_residual(plane, arrivals[k]);
        if (is_finite_vec(residuals[k])) {
            norms[k] = residuals[k].norm();
        }
    }

    auto add_seed = [&](double b_u, double b_v) {
        const Eigen::Vector2d candidate(b_u, b_v);
        for (const Eigen::Vector2d& existing : seeds) {
            if (launch_distance2(existing, candidate) <= kResidualEps) {
                return;
            }
        }
        seeds.push_back(candidate);
    };

    int best_index = -1;
    double best_norm = std::numeric_limits<double>::infinity();
    for (std::size_t k = 0; k < samples.size(); ++k) {
        if (norms[k] < best_norm) {
            best_norm = norms[k];
            best_index = static_cast<int>(k);
        }
    }
    if (best_index >= 0) {
        add_seed(samples[static_cast<std::size_t>(best_index)].b_u,
                 samples[static_cast<std::size_t>(best_index)].b_v);
    }

    for (int j = 0; j < samples_per_axis; ++j) {
        for (int i = 0; i < samples_per_axis; ++i) {
            const int k = grid_index(i, j, samples_per_axis);
            if (!std::isfinite(norms[static_cast<std::size_t>(k)])) {
                continue;
            }
            bool is_minimum = true;
            for (int dj = -1; dj <= 1 && is_minimum; ++dj) {
                for (int di = -1; di <= 1; ++di) {
                    if (di == 0 && dj == 0) {
                        continue;
                    }
                    const int ni = i + di;
                    const int nj = j + dj;
                    if (ni < 0 || nj < 0 || ni >= samples_per_axis || nj >= samples_per_axis) {
                        continue;
                    }
                    const int nk = grid_index(ni, nj, samples_per_axis);
                    if (norms[static_cast<std::size_t>(nk)] + kResidualEps <
                        norms[static_cast<std::size_t>(k)]) {
                        is_minimum = false;
                        break;
                    }
                }
            }
            if (is_minimum) {
                add_seed(samples[static_cast<std::size_t>(k)].b_u,
                         samples[static_cast<std::size_t>(k)].b_v);
            }
        }
    }

    auto maybe_edge_seed = [&](std::size_t a, std::size_t b) {
        if (!is_finite_vec(residuals[a]) || !is_finite_vec(residuals[b])) {
            return;
        }
        const Eigen::Vector2d r0 = residuals[a];
        const Eigen::Vector2d r1 = residuals[b];
        const Eigen::Vector2d dr = r1 - r0;
        const double denom = dr.squaredNorm();
        if (denom <= kResidualEps) {
            return;
        }
        double t = -r0.dot(dr) / denom;
        t = std::clamp(t, 0.0, 1.0);
        const Eigen::Vector2d estimated = (1.0 - t) * r0 + t * r1;
        if (estimated.norm() + kResidualEps >= std::min(r0.norm(), r1.norm())) {
            return;
        }
        add_seed((1.0 - t) * samples[a].b_u + t * samples[b].b_u,
                 (1.0 - t) * samples[a].b_v + t * samples[b].b_v);
    };

    for (int j = 0; j < samples_per_axis; ++j) {
        for (int i = 0; i + 1 < samples_per_axis; ++i) {
            maybe_edge_seed(static_cast<std::size_t>(grid_index(i, j, samples_per_axis)),
                            static_cast<std::size_t>(grid_index(i + 1, j, samples_per_axis)));
        }
    }
    for (int j = 0; j + 1 < samples_per_axis; ++j) {
        for (int i = 0; i < samples_per_axis; ++i) {
            maybe_edge_seed(static_cast<std::size_t>(grid_index(i, j, samples_per_axis)),
                            static_cast<std::size_t>(grid_index(i, j + 1, samples_per_axis)));
        }
    }

    return seeds;
}

std::optional<RefinedObserverHit>
refine_launch_to_observer(const Problem::PropagationProblem& problem,
                          const Rays::RayGrid2DSampler& sampler, double b_u0, double b_v0,
                          const ObserverLaunchRefinementConfig& config,
                          Schwarzschild::PropagationContext& context,
                          const Propagation::RadiusBoundTermination& fallback,
                          const Propagation::IntegrationSettings& settings,
                          const Integration::Integrator& integrator, int seed_index) {
    const double b_max = sampler.config().max_impact_parameter;
    auto clamp_b = [b_max](double value) { return std::clamp(value, -b_max, b_max); };

    LaunchHit current =
        evaluate_launch(problem, sampler, clamp_b(b_u0), clamp_b(b_v0), context, fallback,
                        settings, integrator);
    if (!is_finite_vec(current.plane_residual)) {
        return std::nullopt;
    }

    Eigen::Matrix2d jacobian = Eigen::Matrix2d::Zero();
    bool have_jacobian = false;

    for (int iteration = 0; iteration <= config.max_iterations; ++iteration) {
        const double residual_norm = current.plane_residual.norm();
        if (residual_norm <= config.hit_tolerance) {
            const std::optional<Eigen::Vector2d> angular =
                observer_angular_coordinates(current.arrival, problem.observer());
            if (!angular.has_value()) {
                return std::nullopt;
            }
            RefinedObserverHit refined;
            refined.hit = current;
            refined.angular_coordinate = *angular;
            refined.seed_index = seed_index;
            refined.iterations = iteration;
            return refined;
        }
        if (iteration == config.max_iterations) {
            break;
        }

        if (!have_jacobian) {
            const LaunchHit plus_u = evaluate_launch(
                problem, sampler, clamp_b(current.b_u + config.finite_difference_step), current.b_v,
                context, fallback, settings, integrator);
            const LaunchHit plus_v = evaluate_launch(
                problem, sampler, current.b_u, clamp_b(current.b_v + config.finite_difference_step),
                context, fallback, settings, integrator);
            if (!is_finite_vec(plus_u.plane_residual) || !is_finite_vec(plus_v.plane_residual)) {
                return std::nullopt;
            }
            jacobian.col(0) =
                (plus_u.plane_residual - current.plane_residual) / config.finite_difference_step;
            jacobian.col(1) =
                (plus_v.plane_residual - current.plane_residual) / config.finite_difference_step;
            have_jacobian = true;
        }

        Eigen::Vector2d step = solve_gauss_newton(jacobian, current.plane_residual);
        bool accepted = false;
        for (int attempt = 0; attempt < 6; ++attempt) {
            const double next_u = clamp_b(current.b_u + step.x());
            const double next_v = clamp_b(current.b_v + step.y());
            if (next_u == current.b_u && next_v == current.b_v) {
                step *= 0.5;
                continue;
            }
            LaunchHit trial = evaluate_launch(problem, sampler, next_u, next_v, context, fallback,
                                              settings, integrator);
            if (!is_finite_vec(trial.plane_residual) ||
                trial.plane_residual.norm() > residual_norm) {
                step *= 0.5;
                continue;
            }

            const Eigen::Vector2d db(trial.b_u - current.b_u, trial.b_v - current.b_v);
            const double db2 = db.squaredNorm();
            if (db2 > kResidualEps) {
                const Eigen::Vector2d dR = trial.plane_residual - current.plane_residual;
                jacobian += (dR - jacobian * db) * db.transpose() / db2;
            }
            current = trial;
            accepted = true;
            break;
        }
        if (!accepted) {
            have_jacobian = false;
        }
    }

    return std::nullopt;
}

std::vector<RefinedObserverHit>
refine_observer_launches(const Problem::PropagationProblem& problem,
                         const Rays::RayGrid2DSampler& sampler,
                         const std::vector<RayArrival>& search_arrivals,
                         const ObserverLaunchRefinementConfig& config,
                         Schwarzschild::PropagationContext& context,
                         const Propagation::RadiusBoundTermination& fallback,
                         const Propagation::IntegrationSettings& settings,
                         const Integration::Integrator& integrator) {
    const std::vector<Eigen::Vector2d> seeds = observer_hit_seeds(
        sampler.samples(), search_arrivals, problem.image_plane(),
        sampler.config().samples_per_axis);

    std::vector<std::optional<RefinedObserverHit>> per_seed(seeds.size());
#if defined(_OPENMP)
#pragma omp parallel for schedule(dynamic) if (seeds.size() > 1)
#endif
    for (std::size_t i = 0; i < seeds.size(); ++i) {
        per_seed[i] = refine_launch_to_observer(problem, sampler, seeds[i].x(), seeds[i].y(),
                                                config, context, fallback, settings, integrator,
                                                static_cast<int>(i));
    }

    std::vector<RefinedObserverHit> refined;
    refined.reserve(seeds.size());
    for (std::size_t i = 0; i < per_seed.size(); ++i) {
        if (per_seed[i].has_value()) {
            refined.push_back(*per_seed[i]);
        }
    }

    const double cell_width =
        (2.0 * sampler.config().max_impact_parameter) /
        static_cast<double>(sampler.config().samples_per_axis);
    const double duplicate2 = 0.25 * cell_width * 0.25 * cell_width;

    std::sort(refined.begin(), refined.end(), [](const RefinedObserverHit& a,
                                                 const RefinedObserverHit& b) {
        const double na = a.hit.plane_residual.norm();
        const double nb = b.hit.plane_residual.norm();
        if (na < nb) {
            return true;
        }
        if (nb < na) {
            return false;
        }
        return a.seed_index < b.seed_index;
    });

    std::vector<RefinedObserverHit> unique;
    unique.reserve(refined.size());
    for (const RefinedObserverHit& candidate : refined) {
        const Eigen::Vector2d launch(candidate.hit.b_u, candidate.hit.b_v);
        bool duplicate = false;
        for (const RefinedObserverHit& kept : unique) {
            const Eigen::Vector2d existing(kept.hit.b_u, kept.hit.b_v);
            if (launch_distance2(launch, existing) <= duplicate2) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            unique.push_back(candidate);
        }
    }
    return unique;
}

} // namespace Arrivals

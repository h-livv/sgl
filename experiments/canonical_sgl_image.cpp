#include <arrivals/ArrivalCollector.h>
#include <arrivals/ObserverAngularCoordinates.h>
#include <geometry/ImagePlane.h>
#include <geometry/Lens.h>
#include <geometry/Observer.h>
#include <geometry/Source.h>
#include <geometry/WorldFrame.h>
#include <imaging/Image.h>
#include <imaging/ImageFormation.h>
#include <integrators/RK4Integrator.h>
#include <metrics/CoordinateChart.h>
#include <problem/PropagationProblem.h>
#include <propagation/TerminationPolicy.h>
#include <rays/RayEnsemble.h>
#include <rays/RaySampler.h>
#include <schwarzschild/InitialConditions.h>
#include <schwarzschild/InitialStates.h>
#include <schwarzschild/PropagationContext.h>

#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class RayModel {
    Point,
    Parallel,
};

struct CliOptions {
    std::string output_dir = "outputs/sgl_forward";
    int ray_count = 801;
    int azimuth_count = 720;
    int resolution = 1024;
    // Tangent-plane angular extent (dimensionless gnomonic coordinates).
    double extent = 0.8;
    double b_min = 2.0;
    double b_max = 20.0;
    double step_size = 0.01;
    int max_steps = 300000;
    double source_distance = 30.0;
    // Distance along the optical axis (+Z) from the lens to the observer.
    double observer_axial_distance = 30.0;
    // Perpendicular distance from the focal line / optical axis (along +X).
    // Zero is on-axis (Einstein ring); nonzero is an off-axis observer.
    double observer_distance = 0.0;
    double observer_hit_tolerance = 1e-6;
    int max_root_iterations = 60;
    RayModel ray_model = RayModel::Point;
};

struct ObserverHit {
    double b = 0.0;
    Arrivals::RayArrival arrival;
    double residual_u = 0.0;
};

struct ObserverHitBracket {
    ObserverHit left;
    ObserverHit right;
    int index = 0;
};

struct ObserverHitCandidate {
    ObserverHit hit;
    Eigen::Vector2d angular_coordinate = Eigen::Vector2d::Zero();
    double angular_radius = 0.0;
    int bracket_index = 0;
    bool selected = false;
};

struct SelectedObserverHit {
    ObserverHit hit;
    Eigen::Vector2d angular_coordinate = Eigen::Vector2d::Zero();
    double angular_radius = 0.0;
    int selected_bracket_index = -1;
    int candidate_count = 0;
    std::vector<ObserverHitCandidate> candidates;
};

const char* ray_model_name(RayModel model) {
    switch (model) {
    case RayModel::Point:
        return "point";
    case RayModel::Parallel:
        return "parallel";
    }
    return "unknown";
}

void print_usage(std::ostream& out) {
    out << "Usage: sgl_canonical_sgl_image [options]\n"
        << "  --output-dir <dir>              Output directory (default: outputs/sgl_forward)\n"
        << "  --ray-count <int>               Number of impact-parameter samples (default: 41)\n"
        << "  --azimuth-count <int>           Azimuthal expansion count (default: 720)\n"
        << "  --resolution <int>              Square image resolution (default: 512)\n"
        << "  --extent <double>               Angular tangent-plane extent (default: 0.8)\n"
        << "  --b-min <double>                Minimum impact parameter (default: 2.0)\n"
        << "  --b-max <double>                Maximum impact parameter (default: 20.0)\n"
        << "  --step-size <double>            Integration step size (default: 0.01)\n"
        << "  --max-steps <int>               Maximum integration steps (default: 300000)\n"
        << "  --source-distance <double>      Source/launch-plane distance from lens along -Z (default: 30.0)\n"
        << "  --observer-axial-distance <double> Observer distance from lens along +Z (default: 30.0)\n"
        << "  --observer-distance <double>    Perpendicular distance from focal line (default: 0.0)\n"
        << "  --ray-model <point|parallel>    Ray launch model (default: point)\n"
        << "  --observer-hit-tolerance <double> Observer-hit residual tolerance (default: 1e-6)\n"
        << "  --max-root-iterations <int>     Max bisection iterations per root (default: 60)\n"
        << "  --help                          Show this help message\n";
}

bool parse_int(const std::string& value, const std::string& flag, int& out) {
    try {
        std::size_t consumed = 0;
        const int parsed = std::stoi(value, &consumed);
        if (consumed != value.size()) {
            std::cerr << "Invalid integer for " << flag << ": " << value << '\n';
            return false;
        }
        out = parsed;
        return true;
    } catch (const std::exception&) {
        std::cerr << "Invalid integer for " << flag << ": " << value << '\n';
        return false;
    }
}

bool parse_double(const std::string& value, const std::string& flag, double& out) {
    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size()) {
            std::cerr << "Invalid double for " << flag << ": " << value << '\n';
            return false;
        }
        out = parsed;
        return true;
    } catch (const std::exception&) {
        std::cerr << "Invalid double for " << flag << ": " << value << '\n';
        return false;
    }
}

bool parse_args(int argc, char** argv, CliOptions& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(std::cout);
            return false;
        }
        if (arg == "--output-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --output-dir\n";
                return false;
            }
            options.output_dir = argv[++i];
            continue;
        }
        if (arg == "--ray-count") {
            if (i + 1 >= argc || !parse_int(argv[++i], arg, options.ray_count)) {
                return false;
            }
            continue;
        }
        if (arg == "--azimuth-count") {
            if (i + 1 >= argc || !parse_int(argv[++i], arg, options.azimuth_count)) {
                return false;
            }
            continue;
        }
        if (arg == "--resolution") {
            if (i + 1 >= argc || !parse_int(argv[++i], arg, options.resolution)) {
                return false;
            }
            continue;
        }
        if (arg == "--extent") {
            if (i + 1 >= argc || !parse_double(argv[++i], arg, options.extent)) {
                return false;
            }
            continue;
        }
        if (arg == "--b-min") {
            if (i + 1 >= argc || !parse_double(argv[++i], arg, options.b_min)) {
                return false;
            }
            continue;
        }
        if (arg == "--b-max") {
            if (i + 1 >= argc || !parse_double(argv[++i], arg, options.b_max)) {
                return false;
            }
            continue;
        }
        if (arg == "--step-size") {
            if (i + 1 >= argc || !parse_double(argv[++i], arg, options.step_size)) {
                return false;
            }
            continue;
        }
        if (arg == "--max-steps") {
            if (i + 1 >= argc || !parse_int(argv[++i], arg, options.max_steps)) {
                return false;
            }
            continue;
        }
        if (arg == "--source-distance") {
            if (i + 1 >= argc || !parse_double(argv[++i], arg, options.source_distance)) {
                return false;
            }
            continue;
        }
        if (arg == "--observer-axial-distance") {
            if (i + 1 >= argc ||
                !parse_double(argv[++i], arg, options.observer_axial_distance)) {
                return false;
            }
            continue;
        }
        if (arg == "--observer-distance") {
            if (i + 1 >= argc || !parse_double(argv[++i], arg, options.observer_distance)) {
                return false;
            }
            continue;
        }
        if (arg == "--observer-hit-tolerance") {
            if (i + 1 >= argc ||
                !parse_double(argv[++i], arg, options.observer_hit_tolerance)) {
                return false;
            }
            continue;
        }
        if (arg == "--max-root-iterations") {
            if (i + 1 >= argc || !parse_int(argv[++i], arg, options.max_root_iterations)) {
                return false;
            }
            continue;
        }
        if (arg == "--ray-model") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --ray-model\n";
                return false;
            }
            const std::string value = argv[++i];
            if (value == "point") {
                options.ray_model = RayModel::Point;
            } else if (value == "parallel") {
                options.ray_model = RayModel::Parallel;
            } else {
                std::cerr << "Invalid --ray-model (expected point|parallel): " << value << '\n';
                return false;
            }
            continue;
        }
        std::cerr << "Unknown argument: " << arg << '\n';
        print_usage(std::cerr);
        return false;
    }
    return true;
}

double impact_parameter_at(int index, int ray_count, double b_min, double b_max) {
    if (ray_count == 1) {
        return b_min;
    }
    const double t = static_cast<double>(index) / static_cast<double>(ray_count - 1);
    return b_min + t * (b_max - b_min);
}

// Parallel null rays on the launch plane z = -source_distance, offset by impact
// parameter b along +X, all aimed along +Z (toward the lens / observer).
State make_parallel_null_state(const Geometry::Lens& lens, double source_distance, double b) {
    const Eigen::Vector3d world_position =
        -source_distance * Geometry::WorldFrame::optical_axis() +
        b * Geometry::WorldFrame::plane_u_axis();
    const Eigen::Vector3d chart_position = Geometry::to_chart_frame(lens, world_position);
    const Eigen::Vector3d chart_direction =
        Geometry::WorldFrame::world_to_chart(Geometry::WorldFrame::optical_axis());

    const State chart_cartesian(
        Eigen::Vector4d(0.0, chart_position.x(), chart_position.y(), chart_position.z()),
        Eigen::Vector4d(0.0, chart_direction.x(), chart_direction.y(), chart_direction.z()));
    const State spherical = CoordinateChart::cart_to_sphere(chart_cartesian);

    Schwarzschild::CustomInitialConditions initial;
    initial.t0 = 0.0;
    initial.r0 = spherical.X[1];
    initial.theta0 = spherical.X[2];
    initial.phi0 = spherical.X[3];
    initial.vt = 0.0; // filled by null constraint in build_custom
    initial.vr = spherical.U[1];
    initial.vtheta = spherical.U[2];
    initial.vphi = spherical.U[3];
    return Schwarzschild::build_custom(lens.parameters, initial, Schwarzschild::GeodesicKind::Null);
}

Rays::RayEnsemble sample_parallel_rays(const Geometry::Lens& lens, double source_distance,
                                       int ray_count, double b_min, double b_max) {
    Rays::RayEnsemble ensemble;
    for (int i = 0; i < ray_count; ++i) {
        const double b = impact_parameter_at(i, ray_count, b_min, b_max);
        ensemble.add(make_parallel_null_state(lens, source_distance, b));
    }
    return ensemble;
}

Rays::RayEnsemble make_single_ray_ensemble(const Problem::PropagationProblem& problem,
                                           RayModel ray_model, double source_distance,
                                           double b) {
    Rays::RayEnsemble ensemble;
    if (ray_model == RayModel::Point) {
        const Rays::RaySampler sampler(Rays::RaySamplingConfig{
            .ray_count = 1, .min_impact_parameter = b, .max_impact_parameter = b});
        return sampler.sample(problem);
    }
    ensemble.add(make_parallel_null_state(problem.lens(), source_distance, b));
    return ensemble;
}

double residual_u_for_arrival(const Geometry::ImagePlane& plane,
                              const Arrivals::RayArrival& arrival) {
    if (arrival.status != Arrivals::ArrivalStatus::Arrived) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return plane.to_plane_coordinates(arrival.world_position).x();
}

ObserverHit propagate_one_for_b(const Problem::PropagationProblem& problem,
                                Schwarzschild::PropagationContext& context,
                                const Propagation::RadiusBoundTermination& fallback,
                                const Propagation::IntegrationSettings& settings,
                                Integration::RK4Integrator& integrator, RayModel ray_model,
                                double source_distance, double b) {
    const Rays::RayEnsemble ensemble =
        make_single_ray_ensemble(problem, ray_model, source_distance, b);
    const std::vector<Arrivals::RayArrival> arrivals = Arrivals::collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());
    ObserverHit hit;
    hit.b = b;
    hit.arrival = arrivals.empty() ? Arrivals::RayArrival{} : arrivals.front();
    hit.residual_u = residual_u_for_arrival(problem.image_plane(), hit.arrival);
    return hit;
}

bool is_bracketing_hit(const ObserverHit& left, const ObserverHit& right,
                       double observer_hit_tolerance) {
    if (!std::isfinite(left.residual_u) || !std::isfinite(right.residual_u)) {
        return false;
    }
    if (std::abs(left.residual_u) <= observer_hit_tolerance ||
        std::abs(right.residual_u) <= observer_hit_tolerance) {
        return true;
    }
    return (left.residual_u > 0.0 && right.residual_u < 0.0) ||
           (left.residual_u < 0.0 && right.residual_u > 0.0);
}

std::vector<ObserverHitBracket> scan_observer_hit_brackets(
    const std::vector<ObserverHit>& hits, double observer_hit_tolerance) {
    std::vector<ObserverHitBracket> brackets;
    if (hits.empty()) {
        return brackets;
    }

    for (std::size_t i = 0; i + 1 < hits.size(); ++i) {
        if (!is_bracketing_hit(hits[i], hits[i + 1], observer_hit_tolerance)) {
            continue;
        }
        brackets.push_back(ObserverHitBracket{hits[i], hits[i + 1], static_cast<int>(i)});
    }

    if (hits.size() == 1 &&
        hits.front().arrival.status == Arrivals::ArrivalStatus::Arrived &&
        std::abs(hits.front().residual_u) <= observer_hit_tolerance) {
        brackets.push_back(ObserverHitBracket{hits.front(), hits.front(), 0});
    }
    return brackets;
}

ObserverHit refine_observer_hit_bisection(const ObserverHitBracket& bracket,
                                          const Problem::PropagationProblem& problem,
                                          Schwarzschild::PropagationContext& context,
                                          const Propagation::RadiusBoundTermination& fallback,
                                          const Propagation::IntegrationSettings& settings,
                                          Integration::RK4Integrator& integrator,
                                          RayModel ray_model, double source_distance,
                                          double observer_hit_tolerance, int max_root_iterations) {
    ObserverHit left = bracket.left;
    ObserverHit right = bracket.right;

    if (left.b == right.b) {
        return left;
    }

    for (int iteration = 0; iteration < max_root_iterations; ++iteration) {
        if (std::abs(left.residual_u) <= observer_hit_tolerance) {
            return left;
        }
        if (std::abs(right.residual_u) <= observer_hit_tolerance) {
            return right;
        }

        const double mid_b = 0.5 * (left.b + right.b);
        const ObserverHit mid =
            propagate_one_for_b(problem, context, fallback, settings, integrator, ray_model,
                                source_distance, mid_b);
        if (mid.arrival.status != Arrivals::ArrivalStatus::Arrived ||
            !std::isfinite(mid.residual_u)) {
            break;
        }

        if (std::abs(mid.residual_u) <= observer_hit_tolerance) {
            return mid;
        }

        if (is_bracketing_hit(left, mid, observer_hit_tolerance)) {
            right = mid;
        } else if (is_bracketing_hit(mid, right, observer_hit_tolerance)) {
            left = mid;
        } else {
            break;
        }
    }

    if (std::abs(left.residual_u) <= std::abs(right.residual_u)) {
        return left;
    }
    return right;
}

SelectedObserverHit select_primary_observer_hit(
    const std::vector<ObserverHitBracket>& brackets, const Problem::PropagationProblem& problem,
    const Geometry::Observer& observer, Schwarzschild::PropagationContext& context,
    const Propagation::RadiusBoundTermination& fallback,
    const Propagation::IntegrationSettings& settings, Integration::RK4Integrator& integrator,
    RayModel ray_model, double source_distance, double observer_hit_tolerance,
    int max_root_iterations) {
    SelectedObserverHit selection;
    selection.candidate_count = static_cast<int>(brackets.size());

    constexpr double angular_radius_tie_tolerance = 1e-8;
    int best_index = -1;
    double best_radius = std::numeric_limits<double>::infinity();
    double best_b = std::numeric_limits<double>::infinity();

    for (const ObserverHitBracket& bracket : brackets) {
        ObserverHitCandidate candidate;
        candidate.bracket_index = bracket.index;
        candidate.hit = refine_observer_hit_bisection(
            bracket, problem, context, fallback, settings, integrator, ray_model, source_distance,
            observer_hit_tolerance, max_root_iterations);

        const std::optional<Eigen::Vector2d> angular =
            Arrivals::observer_angular_coordinates(candidate.hit.arrival, observer);
        if (!angular.has_value()) {
            candidate.angular_radius = std::numeric_limits<double>::quiet_NaN();
            selection.candidates.push_back(candidate);
            continue;
        }

        candidate.angular_coordinate = *angular;
        candidate.angular_radius = angular->norm();
        if (!std::isfinite(candidate.angular_radius) || candidate.angular_radius <= 0.0) {
            selection.candidates.push_back(candidate);
            continue;
        }

        const bool better_radius = candidate.angular_radius + angular_radius_tie_tolerance < best_radius;
        const bool tie_break =
            std::abs(candidate.angular_radius - best_radius) <= angular_radius_tie_tolerance &&
            candidate.hit.b < best_b;
        if (best_index < 0 || better_radius || tie_break) {
            best_index = static_cast<int>(selection.candidates.size());
            best_radius = candidate.angular_radius;
            best_b = candidate.hit.b;
        }
        selection.candidates.push_back(candidate);
    }

    if (best_index < 0) {
        return selection;
    }

    selection.candidates[static_cast<std::size_t>(best_index)].selected = true;
    const ObserverHitCandidate& chosen = selection.candidates[static_cast<std::size_t>(best_index)];
    selection.hit = chosen.hit;
    selection.angular_coordinate = chosen.angular_coordinate;
    selection.angular_radius = chosen.angular_radius;
    selection.selected_bracket_index = chosen.bracket_index;
    return selection;
}

void write_csv(const std::filesystem::path& path, const Imaging::Image& image) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open CSV output: " + path.string());
    }

    out << "# width," << image.width() << '\n';
    out << "# height," << image.height() << '\n';
    out << "# u_min," << image.u_min() << '\n';
    out << "# u_max," << image.u_max() << '\n';
    out << "# v_min," << image.v_min() << '\n';
    out << "# v_max," << image.v_max() << '\n';
    out << "# normalized,true\n";

    out << std::setprecision(17);
    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            if (x > 0) {
                out << ',';
            }
            out << image.at(x, y);
        }
        out << '\n';
    }
}

void write_pgm(const std::filesystem::path& path, const Imaging::Image& image) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open PGM output: " + path.string());
    }

    out << "P2\n" << image.width() << ' ' << image.height() << "\n65535\n";
    for (std::size_t y = image.height(); y-- > 0;) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            if (x > 0) {
                out << ' ';
            }
            const double value = image.at(x, y);
            const int scaled =
                static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 65535.0));
            out << scaled;
        }
        out << '\n';
    }
}

void write_summary(const std::filesystem::path& path, const CliOptions& options,
                   std::size_t ray_count, std::size_t raw_arrivals, std::size_t arrived_count,
                   const SelectedObserverHit& selection, std::size_t angular_sample_count,
                   double raw_max, const std::filesystem::path& csv_path,
                   const std::filesystem::path& pgm_path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open summary output: " + path.string());
    }

    out << "output_dir=" << options.output_dir << '\n';
    out << "image_observable=observer_angular_gnomonic\n";
    out << "ray_count=" << options.ray_count << '\n';
    out << "azimuth_count=" << options.azimuth_count << '\n';
    out << "resolution=" << options.resolution << '\n';
    out << "extent=" << options.extent << '\n';
    out << "angular_extent=" << options.extent << '\n';
    out << "b_min=" << options.b_min << '\n';
    out << "b_max=" << options.b_max << '\n';
    out << "step_size=" << options.step_size << '\n';
    out << "max_steps=" << options.max_steps << '\n';
    out << "source_distance=" << options.source_distance << '\n';
    out << "observer_axial_distance=" << options.observer_axial_distance << '\n';
    out << "observer_distance=" << options.observer_distance << '\n';
    out << "observer_hit_tolerance=" << options.observer_hit_tolerance << '\n';
    out << "max_root_iterations=" << options.max_root_iterations << '\n';
    out << "ray_model=" << ray_model_name(options.ray_model) << '\n';
    out << "rays_sampled=" << ray_count << '\n';
    out << "raw_arrivals=" << raw_arrivals << '\n';
    out << "arrived_count=" << arrived_count << '\n';
    out << "observer_hit_candidate_count=" << selection.candidate_count << '\n';
    out << "observer_hit_count="
        << (selection.selected_bracket_index >= 0 ? 1 : 0) << '\n';
    out << "selected_observer_hit_bracket_index=" << selection.selected_bracket_index << '\n';
    out << "selected_observer_hit_b=" << selection.hit.b << '\n';
    out << "selected_observer_hit_residual_u=" << selection.hit.residual_u << '\n';
    out << "selected_angular_radius=" << selection.angular_radius << '\n';
    out << "selected_angular_u=" << selection.angular_coordinate.x() << '\n';
    out << "selected_angular_v=" << selection.angular_coordinate.y() << '\n';
    // Primary Einstein-ring radius: true angular radius from the observer-hit root.
    // theta_E = atan(rho) with rho the gnomonic tangent-plane radius. Not derived from
    // image pixels or radial histograms.
    const double theta_E = std::atan(selection.angular_radius);
    const double R_equiv = options.observer_axial_distance * selection.angular_radius;
    out << "selected_angular_theta=" << theta_E << '\n';
    out << "theta_E=" << theta_E << '\n';
    out << "R_equiv=" << R_equiv << '\n';
    out << "ring_radius_source=observer_hit_root\n";
    for (std::size_t i = 0; i < selection.candidates.size(); ++i) {
        const ObserverHitCandidate& candidate = selection.candidates[i];
        const double candidate_theta = std::atan(candidate.angular_radius);
        out << "observer_hit_candidate_" << i << "=b:" << candidate.hit.b
            << ",residual:" << candidate.hit.residual_u << ",rho:" << candidate.angular_radius
            << ",theta:" << candidate_theta
            << ",selected:" << (candidate.selected ? "true" : "false") << '\n';
    }
    out << "angular_samples=" << angular_sample_count << '\n';
    out << "raw_image_max=" << raw_max << '\n';
    out << "csv_path=" << csv_path.string() << '\n';
    out << "pgm_path=" << pgm_path.string() << '\n';
}

} // namespace

int main(int argc, char** argv) {
    CliOptions options;
    if (!parse_args(argc, argv, options)) {
        return 1;
    }

    if (options.ray_count < 1 || options.azimuth_count < 1 || options.resolution < 1 ||
        options.max_steps < 1 || options.extent <= 0.0 || options.step_size <= 0.0 ||
        options.b_max < options.b_min || options.source_distance <= 0.0 ||
        options.observer_axial_distance <= 0.0 || !std::isfinite(options.observer_distance)) {
        std::cerr << "Invalid parameter values.\n";
        return 1;
    }
    if (options.ray_model == RayModel::Point && options.b_min <= 0.0) {
        std::cerr << "Invalid parameter values: point-source model requires b-min > 0.\n";
        return 1;
    }
    if (options.ray_model == RayModel::Parallel && options.b_min < 0.0) {
        std::cerr << "Invalid parameter values: parallel model requires b-min >= 0.\n";
        return 1;
    }
    if (options.ray_count > 1 && options.b_max == options.b_min) {
        std::cerr << "Invalid parameter values: b-max must exceed b-min when ray-count > 1.\n";
        return 1;
    }
    if (options.observer_hit_tolerance <= 0.0 || options.max_root_iterations < 1) {
        std::cerr << "Invalid parameter values: observer-hit-tolerance > 0 and "
                     "max-root-iterations >= 1 required.\n";
        return 1;
    }
    if (options.observer_distance != 0.0) {
        std::cerr << "Angular image formation (Fix A) requires on-axis observer "
                     "(observer-distance == 0).\n";
        return 1;
    }

    const double half_extent = 0.5 * options.extent;
    Geometry::Lens lens;
    lens.parameters = Spacetime::SchwarzschildParameters{.rs = 1.0};

    Geometry::Source source;
    source.position = -options.source_distance * Geometry::WorldFrame::optical_axis();

    // Observer sits at axial distance D along +Z, then displaced perpendicular to the
    // focal line (optical axis) along +X by observer_distance.
    const Eigen::Vector3d observer_position =
        options.observer_axial_distance * Geometry::WorldFrame::optical_axis() +
        options.observer_distance * Geometry::WorldFrame::plane_u_axis();
    const Geometry::Observer observer = Geometry::Observer::looking_at(
        observer_position, Eigen::Vector3d::Zero(), Geometry::WorldFrame::plane_v_axis());
    const Geometry::ImagePlane image_plane =
        Geometry::ImagePlane::attached_to(observer, half_extent, half_extent);
    const Problem::PropagationProblem problem(lens, source, observer, image_plane);

    Schwarzschild::PropagationOptions propagation_options;
    propagation_options.horizon_safety_factor = 1.0001;
    propagation_options.escape_radius = std::numeric_limits<double>::infinity();
    propagation_options.null_constraint_projection = true;
    propagation_options.null_projection_interval = 1000;
    Schwarzschild::PropagationContext context(Spacetime::SchwarzschildParameters{.rs = 1.0},
                                              propagation_options);

    const Propagation::RadiusBoundTermination fallback(
        1.0001, std::numeric_limits<double>::infinity());
    Integration::RK4Integrator integrator;
    const Propagation::IntegrationSettings settings{.step_size = options.step_size,
                                                    .max_steps = options.max_steps};

    Rays::RayEnsemble ensemble;
    if (options.ray_model == RayModel::Point) {
        const Rays::RaySampler sampler(Rays::RaySamplingConfig{
            .ray_count = options.ray_count,
            .min_impact_parameter = options.b_min,
            .max_impact_parameter = options.b_max});
        ensemble = sampler.sample(problem);
    } else {
        ensemble = sample_parallel_rays(lens, options.source_distance, options.ray_count,
                                        options.b_min, options.b_max);
    }
    const std::vector<Arrivals::RayArrival> arrivals = Arrivals::collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());

    std::size_t arrived_count = 0;
    std::vector<ObserverHit> scan_hits;
    scan_hits.reserve(arrivals.size());
    for (std::size_t i = 0; i < arrivals.size(); ++i) {
        const Arrivals::RayArrival& arrival = arrivals[i];
        if (arrival.status == Arrivals::ArrivalStatus::Arrived) {
            ++arrived_count;
        }
        const double b = impact_parameter_at(static_cast<int>(i), options.ray_count, options.b_min,
                                           options.b_max);
        scan_hits.push_back(ObserverHit{
            b, arrival, residual_u_for_arrival(problem.image_plane(), arrival)});
    }

    const std::vector<ObserverHitBracket> brackets =
        scan_observer_hit_brackets(scan_hits, options.observer_hit_tolerance);
    if (brackets.empty()) {
        std::cerr << "No observer-hit root bracketed in [b_min, b_max]. "
                     "Widen --b-min/--b-max and retry.\n";
        return 1;
    }

    const SelectedObserverHit selection = select_primary_observer_hit(
        brackets, problem, observer, context, fallback, settings, integrator, options.ray_model,
        options.source_distance, options.observer_hit_tolerance, options.max_root_iterations);
    if (selection.selected_bracket_index < 0) {
        std::cerr << "No valid primary observer-hit root selected after refinement.\n";
        return 1;
    }
    if (std::abs(selection.hit.residual_u) > options.observer_hit_tolerance) {
        std::cerr << "Selected observer-hit residual exceeds tolerance: residual_u="
                  << selection.hit.residual_u << '\n';
        return 1;
    }

    const std::vector<Eigen::Vector2d> angular_samples = Arrivals::expand_angular_azimuthally(
        selection.angular_coordinate.x(), options.azimuth_count);

    const Imaging::Image image = Imaging::ImageFormation::form_image(
        angular_samples, static_cast<std::size_t>(options.resolution),
        static_cast<std::size_t>(options.resolution), options.extent);
    const double raw_max = image.max_intensity();
    const Imaging::Image normalized = image.normalized_to_max();

    const std::filesystem::path output_dir(options.output_dir);
    std::filesystem::create_directories(output_dir);

    const std::filesystem::path csv_path = output_dir / "einstein_ring.csv";
    const std::filesystem::path pgm_path = output_dir / "einstein_ring.pgm";
    const std::filesystem::path summary_path = output_dir / "run_summary.txt";

    write_csv(csv_path, normalized);
    write_pgm(pgm_path, normalized);
    write_summary(summary_path, options, ensemble.size(), arrivals.size(), arrived_count,
                  selection, angular_samples.size(), raw_max, csv_path, pgm_path);

    std::cout << "Wrote " << csv_path << '\n';
    std::cout << "Wrote " << pgm_path << '\n';
    std::cout << "Wrote " << summary_path << '\n';
    return 0;
}

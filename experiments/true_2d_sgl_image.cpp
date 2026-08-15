// 2D launch-plane search + Gauss-Newton refine experiment (sgl_true_2d_sgl_image).
//
// Same geometry as the 1D executable, but CLI --observer-distance (transverse d
// along +X) IS allowed. --observer-axial-distance is D along +Z. Distinct from
// PropagationProblem::observer_distance() (|observer−lens|).
//
// Data flow: ALWAYS a parallel RayGrid2DSampler search grid (not the image;
// those geodesics are never binned) → collect_arrivals → observer_hit_seeds +
// refine_observer_launches (damped Gauss-Newton on plane residual) → observer
// gnomonic coords of refined hits → fill_aligned_observer_ring (on-axis: median
// ρ then azimuthal copy; off-axis: refined hits only) → covering_extent may
// enlarge the window → form_image → CSV/PGM/hits CSV/summary.
//
// Kernel knobs: horizon_safety_factor 1.0001, null projection every 1000 steps.

#include <arrivals/ArrivalCollector.h>
#include <arrivals/ObserverAngularCoordinates.h>
#include <arrivals/ObserverLaunchRefiner.h>
#include <geometry/ImagePlane.h>
#include <geometry/Lens.h>
#include <geometry/Observer.h>
#include <geometry/Source.h>
#include <geometry/WorldFrame.h>
#include <imaging/Image.h>
#include <imaging/ImageFormation.h>
#include <integrators/RK4Integrator.h>
#include <problem/PropagationProblem.h>
#include <propagation/TerminationPolicy.h>
#include <rays/RayGrid2DSampler.h>
#include <schwarzschild/PropagationContext.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct CliOptions {
    std::string output_dir = "outputs/sgl_true_2d";
    int samples_per_axis = 5;  // launch-plane search grid (not image pixels)
    int resolution = 64;
    double extent = 0.8;       // requested imaging window; covering_extent may grow it
    double b_max = 20.0;       // launch-plane half-width
    double step_size = 0.01;
    int max_steps = 300000;
    double source_distance = 30.0;           // S along −Z
    double observer_axial_distance = 30.0;   // D along +Z; not |observer−lens|
    double observer_distance = 0.0;          // transverse d along +X; allowed nonzero
    double observer_hit_tolerance = 1e-6;    // |plane residual| after Newton
    int max_root_iterations = 12;            // Gauss-Newton cap per seed (not 1D bisection)
    int azimuth_count = 720;                 // on-axis ring fill; unused off-axis
};

void print_usage(std::ostream& out) {
    out << "Usage: sgl_true_2d_sgl_image [options]\n"
        << "  --output-dir <dir>                Output directory (default: outputs/sgl_true_2d)\n"
        << "  --samples-per-axis <int>          2D grid samples per axis (default: 5)\n"
        << "  --resolution <int>                Square image resolution (default: 64)\n"
        << "  --extent <double>                 Angular tangent-plane extent (default: 0.8)\n"
        << "  --b-max <double>                  Launch-plane half-width (default: 20.0)\n"
        << "  --step-size <double>              Integration step size (default: 0.01)\n"
        << "  --max-steps <int>                 Maximum integration steps (default: 300000)\n"
        << "  --source-distance <double>        Source distance from lens along -Z (default: 30.0)\n"
        << "  --observer-axial-distance <double> Observer distance from lens along +Z (default: 30.0)\n"
        << "  --observer-distance <double>      Perpendicular observer displacement along +X (default: 0.0)\n"
        << "  --observer-hit-tolerance <double> Observer-plane residual tolerance after refinement (default: 1e-6)\n"
        << "  --max-root-iterations <int>       Max Gauss-Newton iterations per seed (default: 12)\n"
        << "  --azimuth-count <int>             On-axis ring fill count (default: 720; unused off-axis)\n"
        << "  --help                            Show this help message\n";
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
        if (arg == "--samples-per-axis") {
            if (i + 1 >= argc || !parse_int(argv[++i], arg, options.samples_per_axis)) {
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
        if (arg == "--azimuth-count") {
            if (i + 1 >= argc || !parse_int(argv[++i], arg, options.azimuth_count)) {
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

// Scientific dump of normalized counts, row-major, low-v first (not flipped).
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

// Visualization. Rows written high-v to low-v so +v is visually up.
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

// Refined Newton hits (launch b_u,b_v + observer gnomonic), not the search grid.
void write_hits(const std::filesystem::path& path, const Geometry::Observer& observer,
                const Geometry::ImagePlane& plane,
                const std::vector<Arrivals::RefinedObserverHit>& hits) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open hits output: " + path.string());
    }

    out << std::setprecision(17);
    out << "b_u,b_v,residual_u,residual_v,residual_norm,signed_distance,world_miss,"
           "arrival_x,arrival_y,arrival_z,u_ang,v_ang,iterations,seed_index\n";
    for (const Arrivals::RefinedObserverHit& hit : hits) {
        const Eigen::Vector3d miss = hit.hit.arrival.world_position - observer.position();
        out << hit.hit.b_u << ',' << hit.hit.b_v << ',' << hit.hit.plane_residual.x() << ','
            << hit.hit.plane_residual.y() << ',' << hit.hit.plane_residual.norm() << ','
            << plane.signed_distance(hit.hit.arrival.world_position) << ',' << miss.norm() << ','
            << hit.hit.arrival.world_position.x() << ',' << hit.hit.arrival.world_position.y()
            << ',' << hit.hit.arrival.world_position.z() << ',' << hit.angular_coordinate.x()
            << ',' << hit.angular_coordinate.y() << ',' << hit.iterations << ','
            << hit.seed_index << '\n';
    }
}

// Run metadata. Median ρ / radial stddev are of refined angular samples, not pixels.
void write_summary(const std::filesystem::path& path, const CliOptions& options,
                   const Geometry::Observer& observer, std::size_t rays_sampled,
                   std::size_t raw_arrivals, std::size_t arrived_count, std::size_t seed_count,
                   std::size_t refined_count, std::size_t angular_samples,
                   double max_refined_residual, double median_radius, double radial_stddev,
                   double image_extent, double raw_max, const std::filesystem::path& csv_path,
                   const std::filesystem::path& pgm_path, const std::filesystem::path& hits_path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open summary output: " + path.string());
    }

    out << "output_dir=" << options.output_dir << '\n';
    out << "image_observable=observer_angular_gnomonic\n";
    out << "sampling_method=true_2d_launch_plane_refined\n";
    out << "samples_per_axis=" << options.samples_per_axis << '\n';
    out << "resolution=" << options.resolution << '\n';
    out << "extent=" << options.extent << '\n';
    out << "image_extent=" << image_extent << '\n';
    out << "b_max=" << options.b_max << '\n';
    out << "step_size=" << options.step_size << '\n';
    out << "max_steps=" << options.max_steps << '\n';
    out << "source_distance=" << options.source_distance << '\n';
    out << "observer_axial_distance=" << options.observer_axial_distance << '\n';
    out << "observer_distance=" << options.observer_distance << '\n';
    out << "observer_position_x=" << observer.position().x() << '\n';
    out << "observer_position_y=" << observer.position().y() << '\n';
    out << "observer_position_z=" << observer.position().z() << '\n';
    out << "observer_hit_tolerance=" << options.observer_hit_tolerance << '\n';
    out << "max_root_iterations=" << options.max_root_iterations << '\n';
    out << "azimuth_count=" << options.azimuth_count << '\n';
    out << "on_axis_azimuthal_expansion=" << (options.observer_distance == 0.0 ? 1 : 0) << '\n';
    out << "rays_sampled=" << rays_sampled << '\n';
    out << "raw_arrivals=" << raw_arrivals << '\n';
    out << "arrived_count=" << arrived_count << '\n';
    out << "seed_count=" << seed_count << '\n';
    out << "refined_observer_hits=" << refined_count << '\n';
    out << "angular_samples=" << angular_samples << '\n';
    out << "max_refined_residual=" << max_refined_residual << '\n';
    out << "median_angular_radius=" << median_radius << '\n';
    out << "radial_stddev=" << radial_stddev << '\n';
    out << "raw_image_max=" << raw_max << '\n';
    out << "csv_path=" << csv_path.string() << '\n';
    out << "pgm_path=" << pgm_path.string() << '\n';
    out << "hits_path=" << hits_path.string() << '\n';
}

// Median / population stddev of ||(u_ang, v_ang)|| over refined hits (not the search grid).
double median_radius(const std::vector<Eigen::Vector2d>& coordinates) {
    if (coordinates.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::vector<double> radii;
    radii.reserve(coordinates.size());
    for (const Eigen::Vector2d& coordinate : coordinates) {
        radii.push_back(coordinate.norm());
    }
    std::sort(radii.begin(), radii.end());
    const std::size_t mid = radii.size() / 2;
    if (radii.size() % 2 == 1) {
        return radii[mid];
    }
    return 0.5 * (radii[mid - 1] + radii[mid]);
}

double radial_stddev(const std::vector<Eigen::Vector2d>& coordinates) {
    if (coordinates.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::vector<double> radii;
    radii.reserve(coordinates.size());
    for (const Eigen::Vector2d& coordinate : coordinates) {
        radii.push_back(coordinate.norm());
    }
    const double mean =
        std::accumulate(radii.begin(), radii.end(), 0.0) / static_cast<double>(radii.size());
    double variance = 0.0;
    for (double radius : radii) {
        const double delta = radius - mean;
        variance += delta * delta;
    }
    variance /= static_cast<double>(radii.size());
    return std::sqrt(variance);
}

} // namespace

int main(int argc, char** argv) {
    // Stages: parse/validate → geometry (d may be nonzero) → kernel → 2D search
    // grid → collect_arrivals → Gauss-Newton refine → angular fill → image → I/O.
    CliOptions options;
    if (!parse_args(argc, argv, options)) {
        return 1;
    }

    if (options.samples_per_axis < 2 || options.resolution < 1 || options.max_steps < 1 ||
        options.extent <= 0.0 || options.step_size <= 0.0 || options.b_max <= 0.0 ||
        options.source_distance <= 0.0 || options.observer_axial_distance <= 0.0 ||
        options.observer_hit_tolerance <= 0.0 || options.max_root_iterations < 1 ||
        options.azimuth_count < 1 || !std::isfinite(options.observer_distance)) {
        std::cerr << "Invalid parameter values.\n";
        return 1;
    }

    // Geometry: lens origin rs=1, source (0,0,−S), observer D·Z + d·X (d allowed).
    const double half_extent = 0.5 * options.extent;
    Geometry::Lens lens;
    lens.parameters = Spacetime::SchwarzschildParameters{.rs = 1.0};

    Geometry::Source source;
    source.position = -options.source_distance * Geometry::WorldFrame::optical_axis();

    // CLI --observer-distance is d along +X; --observer-axial-distance is D along +Z.
    const Eigen::Vector3d observer_position =
        options.observer_axial_distance * Geometry::WorldFrame::optical_axis() +
        options.observer_distance * Geometry::WorldFrame::plane_u_axis();
    const Geometry::Observer observer = Geometry::Observer::looking_at(
        observer_position, Eigen::Vector3d::Zero(), Geometry::WorldFrame::plane_v_axis());
    const Geometry::ImagePlane image_plane =
        Geometry::ImagePlane::attached_to(observer, half_extent, half_extent);
    const Problem::PropagationProblem problem(lens, source, observer, image_plane);

    // Kernel: capture at r = 1.0001 rs; reproject the null constraint every 1000 steps.
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

    // Sampling: parallel RayGrid2DSampler search grid — never binned into the image.
    Rays::RayGrid2DSampler sampler(Rays::RayGrid2DSamplingConfig{
        .samples_per_axis = options.samples_per_axis,
        .max_impact_parameter = options.b_max});
    const Rays::RayEnsemble ensemble = sampler.sample(problem);
    // Arrivals: PlaneCrossingTermination + RK4 + localize. Crossing ≠ observer hit.
    const std::vector<Arrivals::RayArrival> arrivals = Arrivals::collect_arrivals(
        ensemble, problem, context.dynamics(), fallback, settings, integrator,
        context.correction());

    std::size_t arrived_count = 0;
    for (const Arrivals::RayArrival& arrival : arrivals) {
        if (arrival.status == Arrivals::ArrivalStatus::Arrived) {
            ++arrived_count;
        }
    }

    // Newton: seeds from the search grid, then damped Gauss-Newton on plane residual.
    const Arrivals::ObserverLaunchRefinementConfig refinement{
        .hit_tolerance = options.observer_hit_tolerance,
        .max_iterations = options.max_root_iterations};

    const std::vector<Eigen::Vector2d> seeds = Arrivals::observer_hit_seeds(
        sampler.samples(), arrivals, problem.image_plane(), options.samples_per_axis);
    const std::vector<Arrivals::RefinedObserverHit> refined = Arrivals::refine_observer_launches(
        problem, sampler, arrivals, refinement, context, fallback, settings, integrator);

    // Refined hits already carry observer gnomonic (u_ang, v_ang).
    std::vector<Eigen::Vector2d> angular_coordinates;
    angular_coordinates.reserve(refined.size());
    double max_refined_residual = 0.0;
    for (const Arrivals::RefinedObserverHit& hit : refined) {
        angular_coordinates.push_back(hit.angular_coordinate);
        max_refined_residual = std::max(max_refined_residual, hit.hit.plane_residual.norm());
    }

    if (angular_coordinates.empty()) {
        std::cerr << "No launch parameters refined to the observer.\n";
        return 1;
    }

    // Angular: on-axis median ρ + azimuthal copy; off-axis refined hits only.
    const std::vector<Eigen::Vector2d> image_samples = Arrivals::fill_aligned_observer_ring(
        angular_coordinates, options.observer_distance, options.azimuth_count);

    // covering_extent may enlarge the window so hits on/beyond the max boundary are kept.
    const double image_extent =
        Imaging::ImageFormation::covering_extent(image_samples, options.extent);
    if (image_extent > options.extent) {
        std::cerr << "Warning: observer-hit angular samples fall outside extent="
                  << options.extent << "; imaging with extent=" << image_extent
                  << " so they are not discarded.\n";
    }

    // Image: +1 per filled sample (not the search geodesics), then scale peak to 1.
    const Imaging::Image image = Imaging::ImageFormation::form_image(
        image_samples, static_cast<std::size_t>(options.resolution),
        static_cast<std::size_t>(options.resolution), image_extent);
    const double raw_max = image.max_intensity();
    const Imaging::Image normalized = image.normalized_to_max();

    // I/O: CSV (scientific, low-v first), PGM (high-v first so +v is up), hits CSV.
    const std::filesystem::path output_dir(options.output_dir);
    std::filesystem::create_directories(output_dir);

    const std::filesystem::path csv_path = output_dir / "true_2d_image.csv";
    const std::filesystem::path pgm_path = output_dir / "true_2d_image.pgm";
    const std::filesystem::path hits_path = output_dir / "refined_observer_hits.csv";
    const std::filesystem::path summary_path = output_dir / "run_summary.txt";

    write_csv(csv_path, normalized);
    write_pgm(pgm_path, normalized);
    write_hits(hits_path, observer, problem.image_plane(), refined);
    write_summary(summary_path, options, observer, ensemble.size(), arrivals.size(),
                  arrived_count, seeds.size(), angular_coordinates.size(), image_samples.size(),
                  max_refined_residual, median_radius(angular_coordinates),
                  radial_stddev(angular_coordinates), image_extent, raw_max, csv_path, pgm_path,
                  hits_path);

    std::cout << "Wrote " << csv_path << '\n';
    std::cout << "Wrote " << pgm_path << '\n';
    std::cout << "Wrote " << hits_path << '\n';
    std::cout << "Wrote " << summary_path << '\n';
    return 0;
}

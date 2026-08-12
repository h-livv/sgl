#include <arrivals/ArrivalCollector.h>
#include <arrivals/AzimuthalExpansion.h>
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
    int ray_count = 41;
    int azimuth_count = 720;
    int resolution = 512;
    double extent = 40.0;
    double b_min = 10.2;
    double b_max = 11.6;
    double step_size = 0.01;
    int max_steps = 300000;
    double source_distance = 30.0;
    // Distance along the optical axis (+Z) from the lens to the observer.
    double observer_axial_distance = 30.0;
    // Perpendicular distance from the focal line / optical axis (along +X).
    // Zero is on-axis (Einstein ring); nonzero is an off-axis observer.
    double observer_distance = 0.0;
    RayModel ray_model = RayModel::Point;
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
        << "  --extent <double>               Physical image extent (default: 40.0)\n"
        << "  --b-min <double>                Minimum impact parameter (default: 10.2)\n"
        << "  --b-max <double>                Maximum impact parameter (default: 11.6)\n"
        << "  --step-size <double>            Integration step size (default: 0.01)\n"
        << "  --max-steps <int>               Maximum integration steps (default: 300000)\n"
        << "  --source-distance <double>      Source/launch-plane distance from lens along -Z (default: 30.0)\n"
        << "  --observer-axial-distance <double> Observer distance from lens along +Z (default: 30.0)\n"
        << "  --observer-distance <double>    Perpendicular distance from focal line (default: 0.0)\n"
        << "  --ray-model <point|parallel>    Ray launch model (default: point)\n"
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
                   std::size_t plane_arrival_count, double raw_max,
                   const std::filesystem::path& csv_path,
                   const std::filesystem::path& pgm_path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open summary output: " + path.string());
    }

    out << "output_dir=" << options.output_dir << '\n';
    out << "ray_count=" << options.ray_count << '\n';
    out << "azimuth_count=" << options.azimuth_count << '\n';
    out << "resolution=" << options.resolution << '\n';
    out << "extent=" << options.extent << '\n';
    out << "b_min=" << options.b_min << '\n';
    out << "b_max=" << options.b_max << '\n';
    out << "step_size=" << options.step_size << '\n';
    out << "max_steps=" << options.max_steps << '\n';
    out << "source_distance=" << options.source_distance << '\n';
    out << "observer_axial_distance=" << options.observer_axial_distance << '\n';
    out << "observer_distance=" << options.observer_distance << '\n';
    out << "ray_model=" << ray_model_name(options.ray_model) << '\n';
    out << "rays_sampled=" << ray_count << '\n';
    out << "raw_arrivals=" << raw_arrivals << '\n';
    out << "arrived_count=" << arrived_count << '\n';
    out << "plane_arrivals=" << plane_arrival_count << '\n';
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
    for (const Arrivals::RayArrival& arrival : arrivals) {
        if (arrival.status == Arrivals::ArrivalStatus::Arrived) {
            ++arrived_count;
        }
    }

    // Azimuthal expansion is valid only on axis (spherical symmetry about the optical
    // axis). Off-axis observers use the actual plane arrivals only.
    std::vector<Arrivals::PlaneArrival> plane_arrivals;
    if (options.observer_distance == 0.0) {
        plane_arrivals = Arrivals::expand_azimuthally(arrivals, problem.image_plane(),
                                                      options.azimuth_count);
    } else {
        plane_arrivals.reserve(arrived_count);
        for (const Arrivals::RayArrival& arrival : arrivals) {
            if (arrival.status != Arrivals::ArrivalStatus::Arrived) {
                continue;
            }
            plane_arrivals.push_back(Arrivals::PlaneArrival{
                arrival.ray_id, problem.image_plane().to_plane_coordinates(arrival.world_position)});
        }
    }

    const Imaging::Image image = Imaging::ImageFormation::form_image(
        plane_arrivals, static_cast<std::size_t>(options.resolution),
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
                  plane_arrivals.size(), raw_max, csv_path, pgm_path);

    std::cout << "Wrote " << csv_path << '\n';
    std::cout << "Wrote " << pgm_path << '\n';
    std::cout << "Wrote " << summary_path << '\n';
    return 0;
}

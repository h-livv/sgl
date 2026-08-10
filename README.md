# SGL — scientific computing foundation

Standalone extract of reusable General Relativity / geodesic infrastructure from
[Penrose](https://github.com/), intended as a **foundation** for Solar
Gravitational Lens (SGL) research.

This directory is self-contained: copy `SGL/` out of the Penrose tree and build
it independently. It does **not** implement an SGL optical pipeline.

## What this is

A Schwarzschild-focused CPU science library:

- spacetime / metric interface
- Schwarzschild geometry (Christoffel symbols)
- geodesic equation + RK4 integration
- low-level propagation API (`propagate`, `propagate_recorded`)
- termination policies and integration settings split from model setup
- Schwarzschild composition context and null/timelike initial-state builders
- conserved-quantity observables
- physical constants / geometrized-unit conventions

## What this is not

SGL-specific work is intentionally absent. Empty placeholders exist for you to
fill in:

| Directory | Intended use (not implemented here) |
|-----------|--------------------------------------|
| `optics/` | source / lens / observer / image formation |
| `experiments/` | SGL experiment drivers |
| `analysis/` | offline analysis / plots |
| `visualization/` | offline presentation helpers |

Realtime GPU rendering, Kerr, GLFW/OpenGL viewers, and Penrose trajectory-viz
were excluded on purpose.

## Layout

```text
SGL/
├── physics/           # GR engine (from Penrose, Schwarzschild path)
│   ├── core/          # State, Metric, parameters, constants (was shared/)
│   ├── metrics/
│   ├── geodesics/
│   ├── integrators/
│   ├── propagation/   # generic single-ray propagation kernel
│   ├── schwarzschild/ # Schwarzschild-specific composition + ICs
│   ├── geometry/      # SGL physical entities (Lens, Source, Observer, ImagePlane)
│   ├── problem/       # PropagationProblem composition
│   ├── rays/          # Ray, RayEnsemble, RaySampler, ensemble propagation
│   ├── arrivals/      # observer-plane crossing detection and RayArrival
│   └── validation/
├── optics/            # placeholder
├── experiments/       # placeholder
├── analysis/          # placeholder
├── visualization/     # placeholder
├── tests/             # ctest-registered regression/contract tests
├── CMakeLists.txt
├── vcpkg.json
└── README.md
```

## Penrose → SGL mapping

| Penrose | SGL |
|---------|-----|
| `shared/state`, `shared/spacetime`, `shared/metrics`, `shared/constants` | `physics/core/` |
| `physics/metrics/SchwarzschildMetric`, `CoordinateChart` | `physics/metrics/` |
| `physics/geodesics/*` | `physics/geodesics/` |
| `physics/integrators/*` | `physics/integrators/` |
| `physics/simulation/TrajectorySolver`, `TerminationPolicy` | `physics/propagation/` |
| `physics/simulation/SimulationPipeline`, Schwarzschild initial builders | `physics/schwarzschild/` |
| `physics/validation/observables/SchwarzschildObservables` | `physics/validation/observables/` |

Kerr, `realtime/`, and the visualization/viewer stack were not extracted.

## Dependencies

- C++20, CMake ≥ 3.22
- Eigen3 (system install or vcpkg)

## Build

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```

## Minimal usage

```cpp
Spacetime::SchwarzschildParameters params{.rs = 1.0};
Schwarzschild::PropagationContext context(params, {});
State initial = Schwarzschild::build_null_scatter(params, Schwarzschild::NullScatterInitialConditions{});
Propagation::IntegrationSettings settings{.step_size = 0.001, .max_steps = 50000};
Propagation::PropagationOutcome out = Propagation::propagate(
    initial, context.dynamics(), context.termination(), settings, Integration::RK4Integrator{},
    context.correction());
```

Disable tests with `-DSGL_BUILD_TESTS=OFF`.

## Problem geometry

Phase 2 adds `sgl_geometry`, a library independent of the numerical kernel. It
describes the physical SGL setup in geometrized units (`G = c = 1`):

- optical axis = `+Z`
- lens at the origin
- observer at `(0, 0, +D)`, looking toward `-Z`
- point source at `(0, 0, -S)`
- image-plane normal = `+Z` (along incoming light)

```cpp
Problem::PropagationProblem problem = Problem::make_aligned_problem(
    Spacetime::SchwarzschildParameters{.rs = 1.0},
    /* source_distance */ 100.0,
    /* observer_distance */ 50.0,
    /* half_width */ 2.0,
    /* half_height */ 2.0);
```

## Ray ensembles

`sgl_rays` makes the ensemble the normal propagation path. A `RaySampler` turns a
`PropagationProblem` into a `RayEnsemble`; `propagate_ensemble` runs every ray
through the Phase 1 kernel with one shared set of physics objects. Outcomes are
index-aligned with the ensemble, so `outcomes[i]` belongs to the ray whose `id` is
`i`. A single ray is an ensemble of size one.

```cpp
Rays::RaySampler sampler(Rays::RaySamplingConfig{
    .ray_count = 5, .min_impact_parameter = 2.7, .max_impact_parameter = 4.6});
Rays::RayEnsemble ensemble = sampler.sample(problem);

Schwarzschild::PropagationContext context(problem.lens().parameters, options);
Rays::RayOutcomes outcomes = Rays::propagate_ensemble(
    ensemble, context.dynamics(), context.termination(), settings,
    Integration::RK4Integrator{}, context.correction());
```

## Observer-plane arrivals

`sgl_arrivals` detects the first crossing of the Phase 2 `ImagePlane` using
`signed_distance >= 0` (normal `+Z`, along incoming light). The plane is treated
as unbounded for detection; linear interpolation between the two bracketing
integration states localizes the intersection in world space.

```cpp
Propagation::RadiusBoundTermination fallback(params.rs * 1.0001,
    std::numeric_limits<double>::infinity());
std::vector<Arrivals::RayArrival> arrivals = Arrivals::collect_arrivals(
    ensemble, problem, context.dynamics(), fallback, settings,
    Integration::RK4Integrator{}, context.correction());
```

The equatorial ray family lands on `v = 0` because `RaySampler` varies only impact
parameter. `Arrivals::expand_azimuthally` turns that 1D locus into a rotationally
symmetric 2D arrival set by exact spherical symmetry about the optical axis: each
arrival at plane coordinate `(u, 0)` is copied to `(u cos ψ, u sin ψ)` for
`ψ_k = 2π k / N`. No additional geodesics are integrated.

```cpp
std::vector<Arrivals::PlaneArrival> expanded =
    Arrivals::expand_azimuthally(arrivals, problem.image_plane(), /* azimuth_count */ 32);
```

## Next steps

Phase 5 consumes `RayArrival[]`, maps `world_position` through
`ImagePlane::to_plane_coordinates`, and accumulates intensity into an image.

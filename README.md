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

## Next steps

Implement the SGL pipeline on top of `sgl_physics` — source models, ray
generation, propagation wrappers, focal/image plane, and Einstein-ring
experiments — under the placeholder directories above.

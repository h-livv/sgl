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
- trajectory solving and termination policies
- null / timelike initial-condition builders
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
│   ├── simulation/
│   └── validation/
├── optics/            # placeholder
├── experiments/       # placeholder
├── analysis/          # placeholder
├── visualization/     # placeholder
├── tests/             # optional library smoke test
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
| `physics/simulation/*` (Schwarzschild) | `physics/simulation/` |
| `physics/validation/observables/SchwarzschildObservables` | `physics/validation/observables/` |

Kerr, `realtime/`, and the visualization/viewer stack were not extracted.

## Dependencies

- C++20, CMake ≥ 3.22
- Eigen3 (system install or vcpkg)

## Build

```bash
cmake -B build -S .
cmake --build build
```

Optional smoke test (library link + null geodesic conservation check):

```bash
./build/sgl_null_smoke
```

Disable with `-DSGL_BUILD_SMOKE_TEST=OFF`.

## Next steps (for you)

Implement the SGL pipeline on top of `sgl_physics` — source models, ray
generation, propagation wrappers, focal/image plane, and Einstein-ring
experiments — under the placeholder directories above.

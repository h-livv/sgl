# Extraction inventory

What was copied from the sibling Penrose project, and how those pieces are named
in this tree. This is not a description of Penrose itself.

Living docs for the current SGL pipeline:
[`README.md`](../README.md),
[`docs/HOW_THE_EINSTEIN_RING_IS_FORMED.md`](../docs/HOW_THE_EINSTEIN_RING_IS_FORMED.md),
[`docs/SGL_FORWARD_PIPELINE.md`](../docs/SGL_FORWARD_PIPELINE.md),
[`docs/TECHNICAL_BRIEFING.md`](../docs/TECHNICAL_BRIEFING.md).

## Included (GR kernel → `sgl_physics`)

- Geodesic state, metric interface, Schwarzschild parameters
- Schwarzschild Christoffel symbols
- Coordinate chart Cartesian ↔ spherical
- Geodesic dynamics + RK4 + `Propagation::propagate`
- Termination policies
- Schwarzschild initial-state builders + `PropagationContext`
- Schwarzschild observables (E, L, null Hamiltonian, b_crit)
- Physical constants / units placeholders (`PhysicalConstants.h` is unused by the imaging path)

## Excluded from Penrose

- Kerr metric, Kerr ICs, Kerr observables
- GPU ray march / shaders / realtime viewers
- Trajectory visualization / GLFW
- Penrose benchmark harness
- Any SGL optical / image-formation code (that layer was written in this repo)

## Restructuring vs Penrose

- `shared/` folded into `physics/core/`
- Multi-metric request API reduced to Schwarzschild
- Standalone CMake + Eigen-only `vcpkg.json`
- Orchestration is `Propagator` / `PropagationContext`, not Penrose’s `TrajectorySolver` / `SimulationPipeline`

## Added in this repository (not from Penrose)

Geometry, ray ensembles, observer-plane arrivals, 1D/2D image executables,
optional OpenMP, and the validation tests/scripts.

# Extraction inventory

## Included (reusable GR foundation)

- Geodesic state, metric interface, Schwarzschild parameters
- Schwarzschild Christoffel symbols
- Coordinate chart Cartesian ↔ spherical
- Geodesic dynamics + RK4 + TrajectorySolver
- Termination policies
- Schwarzschild IC builders + SimulationPipeline (Schwarzschild-only)
- Schwarzschild observables (E, L, null Hamiltonian, b_crit)
- Physical constants / units placeholders

## Excluded

- Kerr metric, Kerr ICs, Kerr observables
- `realtime/` GPU ray march / shaders
- Trajectory visualization viewer / export / GLFW / glad
- Penrose benchmark harness and analysis scripts
- Any SGL optical / image-formation code

## Restructuring vs Penrose

- `shared/` folded into `physics/core/`
- Multi-metric request API reduced to Schwarzschild
- Standalone CMake + Eigen-only `vcpkg.json`

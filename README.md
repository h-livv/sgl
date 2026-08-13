# SGL — Scientific Computing Foundation

A standalone C++ foundation for Solar Gravitational Lens research.

The current implementation provides a Schwarzschild geometric-optics
forward pipeline with two image paths:

```text
1D:  null geodesics (one plane) → observer-hit root → azimuthal expansion → ring
2D:  launch-plane search grid → Newton to observer → refined hits → ring
```

Both paths share the same Schwarzschild/RK4 kernel. The 2D path does **not**
image the search geodesics; only launches that actually hit the observer are
binned. See [docs/HOW_THE_EINSTEIN_RING_IS_FORMED.md](docs/HOW_THE_EINSTEIN_RING_IS_FORMED.md).

## Status

The aligned Schwarzschild configuration can produce an Einstein-ring intensity
distribution from numerically integrated null geodesics. Off-axis observers
(`--observer-distance ≠ 0`) require the 2D executable.

The current implementation is intentionally minimal. It is a computational
baseline for future SGL experiments, not a complete physical model of the
Solar Gravitational Lens.

![ring](assets/ring.png)

## Scope

- Schwarzschild GR / null geodesics
- RK4 propagation
- Ray ensembles (1D impact-parameter sweep and 2D launch-plane grid)
- Observer-plane arrivals and observer-hit refinement
- Geometric-optics image formation
- Optional OpenMP over independent geodesics and Newton seeds
- Numerical validation

Not yet implemented: Kerr, plasma, wave optics, detector physics, or
mission-level SGL modelling.

## Build

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```

OpenMP is optional: if CMake finds it, `sgl_rays` and `sgl_arrivals` link it;
otherwise the same source builds serially. Thread count is `OMP_NUM_THREADS`
(default: all logical cores). There is no C++ `--threads` flag.

Heavy 2D validation binaries (`sgl_true_2d_canonical_validation`,
`sgl_true_2d_off_axis_validation`) are built but **not** registered in default
CTest.

## Run

On-axis 1D (symmetry-reduced) ring:

```bash
./build/sgl_canonical_sgl_image --output-dir outputs/sgl_forward
```

True 2D launch-plane sampling (required for off-axis observers). Example 5×5
search grid; raise `--samples-per-axis` for a denser ring:

```bash
OMP_NUM_THREADS=8 ./build/sgl_true_2d_sgl_image \
  --output-dir outputs/sgl_true_2d \
  --samples-per-axis 5 --resolution 64 --b-max 20 --extent 0.8
```

Parameter sweeps (Python orchestrator; picks the 2D executable automatically
when sweeping `observer-distance`):

```bash
python3 experiments/parameter_sweep.py
python3 experiments/parameter_sweep.py --threads 8
```

Start with [`docs/HOW_THE_EINSTEIN_RING_IS_FORMED.md`](docs/HOW_THE_EINSTEIN_RING_IS_FORMED.md)
for the ray model, search-vs-ring distinction, and image-formation intuition.
See [`docs/SGL_FORWARD_PIPELINE.md`](docs/SGL_FORWARD_PIPELINE.md) for the full
implementation, CLI, OpenMP, and parameter-sweep documentation.
[`docs/TECHNICAL_BRIEFING.md`](docs/TECHNICAL_BRIEFING.md) is the physics /
numerics study guide (units, validation, limitations).

Validation scripts (1D observer-hit `theta_E`):

```bash
python3 scripts/source_distance_test.py
python3 scripts/ray_convergence_test.py
python3 scripts/step_convergence_test.py
python3 scripts/weak_field_validation.py
```

# SGL Forward Pipeline — Developer Guide

This document describes the **implemented** Solar Gravitational Lensing (SGL) forward
pipeline in this repository. It is reverse-engineered from the current source. It does
not propose architecture changes.

A new developer should be able to read this once, then navigate the relevant source
without relying on prior project history.

Conceptual questions (search geodesics vs ring rays, 1D vs 2D) are answered in
[HOW_THE_EINSTEIN_RING_IS_FORMED.md](HOW_THE_EINSTEIN_RING_IS_FORMED.md).

---

# 1. Overview

The SGL forward pipeline computes a **geometric-optics intensity image** of an Einstein
ring (on-axis) or lensed arcs (off-axis) from numerically integrated Schwarzschild null
geodesics.

There are **two image paths**. They share geometry, the RK4 geodesic kernel, observer-plane
crossing, gnomonic angular coordinates, and pixel binning. They differ in launch sampling
and in how a 2D sky image is filled.

| Path | Executable | Sampling | Ring fill | Off-axis |
|---|---|---|---|---|
| 1D symmetry-reduced | `sgl_canonical_sgl_image` | 1D `b` sweep, one equatorial plane | `expand_angular_azimuthally` | Rejected |
| True 2D | `sgl_true_2d_sgl_image` | Cartesian `(b_u, b_v)` launch-plane grid | Refined observer hits only | Supported |

The 2D **search grid is not the image**. Those geodesics survey launch-parameter space.
Newton then moves promising `(b_u, b_v)` until the geodesic hits the observer. Only those
refined hits are binned. Details: [HOW_THE_EINSTEIN_RING_IS_FORMED.md](HOW_THE_EINSTEIN_RING_IS_FORMED.md)
section 4.

## 1D pipeline diagram

```text
Physical problem
      ↓  Lens + Source + Observer::looking_at + ImagePlane::attached_to
Ray sampling
      ↓  Rays::RaySampler  or  sample_parallel_rays  (experiments/canonical_sgl_image.cpp)
Initial null states
      ↓  Schwarzschild::build_null_scatter  or  build_custom
Schwarzschild geodesic integration
      ↓  Arrivals::collect_arrivals → Rays::propagate_ensemble → Propagation::propagate
Observer-plane crossing
      ↓  Arrivals::PlaneCrossingTermination + Arrivals::localize_arrival
Observer-hit root search
      ↓  residual_u(b) scan + bisection (canonical executable)
Angular coordinates
      ↓  Arrivals::observer_angular_coordinates
Azimuthal symmetry expansion
      ↓  Arrivals::expand_angular_azimuthally
Image formation
      ↓  Imaging::ImageFormation::form_image  (vector of Eigen::Vector2d)
      ↓  write_csv / write_pgm          (experiments/canonical_sgl_image.cpp)
```

## 2D pipeline diagram

```text
Physical problem (observer may be offset along +X)
      ↓
Search-grid sampling
      ↓  Rays::RayGrid2DSampler::sample     (physics/rays/RayGrid2DSampler.cpp)
         N×N cell-centered (b_u, b_v), parallel incident rays, build_custom
Search geodesics
      ↓  Arrivals::collect_arrivals → OpenMP propagate_ensemble
         outcomes[i] corresponds to ensemble.at(i) with id == i
Plane residuals
      ↓  image_plane.to_plane_coordinates(hit)  (miss distance, not an aperture)
Seeds
      ↓  Arrivals::observer_hit_seeds  (global best, local minima, edge interpolations)
Newton (independent seeds, OpenMP; inner 1-ray propagate stays serial)
      ↓  Arrivals::refine_observer_launches → refine_launch_to_observer
Refined observer hits  ← ONLY these are imaged
      ↓  Arrivals::observer_angular_coordinates  (no azimuthal expansion)
Image formation
      ↓  Imaging::ImageFormation::form_image
      ↓  write_csv / write_pgm          (experiments/true_2d_sgl_image.cpp)
```

## Libraries involved

| CMake target | Role |
|---|---|
| `sgl_geometry` | `Lens`, `Source`, `Observer`, `ImagePlane`, `PropagationProblem` |
| `sgl_physics` | metric, dynamics, RK4, propagator, Schwarzschild helpers |
| `sgl_rays` | `Ray`, `RayEnsemble`, `RaySampler`, `RayGrid2DSampler`, `propagate_ensemble` (OpenMP) |
| `sgl_arrivals` | plane crossing, `RayArrival`, angular coordinates, azimuthal expansion, `ObserverLaunchRefiner` (OpenMP) |
| `sgl_imaging` | `Image`, `ImageFormation` |
| `sgl_canonical_sgl_image` | 1D executable |
| `sgl_true_2d_sgl_image` | 2D executable |

Dependency direction in CMake: `sgl_imaging` → `sgl_arrivals` → `sgl_rays` →
(`sgl_geometry`, `sgl_physics`). Geometry does **not** link the numerical kernel.

OpenMP is optional (`find_package(OpenMP)` with no `FATAL_ERROR`). If found, it is
`PUBLIC`-linked on `sgl_rays` and `sgl_arrivals` only. `sgl_physics` has no pragmas.

---

# 2. How to Run It

Verified against the repository build layout (`build/` out of source).

## Configure and build

```bash
cmake -B build -S .
cmake --build build
```

Prerequisites: C++20, CMake ≥ 3.22, Eigen3 (`find_package(Eigen3 CONFIG REQUIRED)` in
`CMakeLists.txt`). OpenMP is optional (`find_package(OpenMP)`); configure succeeds if it
is missing.

## Run tests (optional, recommended)

```bash
ctest --test-dir build --output-on-failure
```

Default CTest covers the 1D angular image path (`canonical_image_pipeline`), 2D sampler
unit tests (`ray_grid_2d_sampler`), and OpenMP bitwise invariance
(`ensemble_parallel_invariance`). Heavy 2D image binaries
`sgl_true_2d_canonical_validation` and `sgl_true_2d_off_axis_validation` are **built but
not registered** in CTest.

## Run the 1D canonical SGL experiment

```bash
./build/sgl_canonical_sgl_image --output-dir outputs/sgl_forward
```

Thread count for both executables is `OMP_NUM_THREADS` (unset = OpenMP default, all
logical cores). Serial: `OMP_NUM_THREADS=1`. There is no C++ `--threads` flag.

## Run the true 2D experiment

```bash
OMP_NUM_THREADS=8 ./build/sgl_true_2d_sgl_image \
  --output-dir outputs/sgl_true_2d \
  --samples-per-axis 5 --resolution 64 --b-max 20 --extent 0.8 \
  --source-distance 30 --observer-axial-distance 30 --observer-distance 0
```

`--samples-per-axis 5` is a cheap search grid (25 geodesics). `11` is a practical
on-axis density. Larger `N` is `N²` search geodesics plus Newton seeds.

## Locate outputs

### 1D (`sgl_canonical_sgl_image`, default `outputs/sgl_forward`)

| File | Role |
|---|---|
| `einstein_ring.csv` | Normalized scalar intensity grid (scientific data) |
| `einstein_ring.pgm` | ASCII PGM visualization (`P2`, 0…65535) |
| `run_summary.txt` | CLI settings, arrival counts, raw max, paths |

Typical default-run summary fields: `image_observable=observer_angular_gnomonic`,
`rays_sampled=801`, `observer_hit_count=1`, `angular_samples=720`, `raw_image_max=1`.

### 2D (`sgl_true_2d_sgl_image`, default `outputs/sgl_true_2d`)

| File | Role |
|---|---|
| `true_2d_image.csv` | Normalized scalar intensity grid |
| `true_2d_image.pgm` | ASCII PGM visualization |
| `run_summary.txt` | Settings plus search/seed/refined counts |

Read `rays_sampled` (search grid), `arrived_count` (plane crossings), `seed_count`,
and `refined_observer_hits` (the image samples) as distinct. See
[HOW_THE_EINSTEIN_RING_IS_FORMED.md](HOW_THE_EINSTEIN_RING_IS_FORMED.md) section 4.

## View the image

```bash
xdg-open outputs/sgl_forward/einstein_ring.pgm
```

Any PGM-capable viewer works. PGM rows are written from high `v` to low `v`, so
positive `v` appears visually upward in common viewers.

## CLI parameters

Defined in `experiments/canonical_sgl_image.cpp` (`CliOptions`):

| Flag | Default | Meaning |
|---|---|---|
| `--output-dir` | `outputs/sgl_forward` | Output directory (created if needed) |
| `--ray-count` | `801` | Number of impact-parameter samples for residual scan |
| `--azimuth-count` | `720` | Azimuthal copies of the selected angular coordinate |
| `--resolution` | `1024` | Square image width and height |
| `--extent` | `0.8` | Angular tangent-plane extent (dimensionless gnomonic coordinates) |
| `--b-min` | `2.0` | Minimum impact parameter for root bracketing |
| `--b-max` | `20.0` | Maximum impact parameter for root bracketing |
| `--step-size` | `0.01` | Affine-parameter RK4 step |
| `--max-steps` | `300000` | Per-ray step budget |
| `--source-distance` | `30.0` | Source distance from the lens along −Z |
| `--observer-axial-distance` | `30.0` | Observer distance from the lens along the optical axis (+Z) |
| `--observer-distance` | `0.0` | Perpendicular distance from the focal line / optical axis (along +X; 0 = on-axis) |
| `--ray-model` | `point` | `point` = fan from a point source; `parallel` = parallel beam on the launch plane at `z = -source-distance` |
| `--observer-hit-tolerance` | `1e-6` | Bisection stop tolerance on plane-coordinate residual `residual_u` |
| `--max-root-iterations` | `60` | Maximum bisection iterations per observer-hit candidate |
| `--help` | — | Print usage |

### Fixed physics (not on CLI)

Hard-coded in `main()` of `experiments/canonical_sgl_image.cpp`:

- `rs = 1.0`
- image-plane half-width/height = `extent / 2`
- `horizon_safety_factor = 1.0001`
- `escape_radius = infinity`
- null-constraint projection enabled every `1000` steps
- azimuthal angular expansion is applied only when `--observer-distance` is exactly `0`
  (on-axis). Off-axis runs are **rejected** (`observer-distance != 0`). Use
  `sgl_true_2d_sgl_image` for off-axis geometry.

Units are geometrized (`G = c = 1`), as stated on `Spacetime::SchwarzschildParameters`
and `Rays::RaySamplingConfig`.

## 2D CLI parameters

Defined in `experiments/true_2d_sgl_image.cpp` (`CliOptions`). There is **no**
`--ray-model`, `--ray-count`, `--azimuth-count`, or `--b-min`. Incident rays are always
parallel. Thread count is not a flag.

| Flag | Default | Meaning |
|---|---|---|
| `--output-dir` | `outputs/sgl_true_2d` | Output directory |
| `--samples-per-axis` | `5` | Search-grid samples per axis (`N×N` geodesics) |
| `--resolution` | `64` | Square image width and height |
| `--extent` | `0.8` | Gnomonic tangent-plane extent |
| `--b-max` | `20.0` | Launch-plane half-width; grid covers `[−b_max, +b_max]²` |
| `--step-size` | `0.01` | Affine-parameter RK4 step |
| `--max-steps` | `300000` | Per-ray step budget |
| `--source-distance` | `30.0` | Launch-plane distance from the lens along −Z |
| `--observer-axial-distance` | `30.0` | Observer distance along +Z |
| `--observer-distance` | `0.0` | Perpendicular offset along +X (off-axis allowed) |
| `--observer-hit-tolerance` | `1e-6` | Newton stop on `‖plane_residual‖` |
| `--max-root-iterations` | `12` | Max Gauss–Newton iterations per seed |

Fixed physics matches the 1D executable (`rs = 1`, null projection every 1000 steps,
unbounded plane for crossing).

## Parameter sweeps

`experiments/parameter_sweep.py` is a thin Python orchestrator. It does not compute
physics; it builds CLI arguments, sets `OMP_NUM_THREADS`, and records metadata.

On-axis 1D sweeps use `sgl_canonical_sgl_image` / `BASE_PARAMS_1D`. Sweeps of
`observer-distance`, or any run whose 1D base `observer-distance ≠ 0`, automatically
select `sgl_true_2d_sgl_image` / `BASE_PARAMS_2D`, because the 1D executable rejects
off-axis geometry.

1. Edit the configuration block:
   - `BASE_PARAMS_1D` / `BASE_PARAMS_2D` — fixed CLI values (must match that executable)
   - `SWEEP_NAME`, `SWEEP_PARAMETER`, `SWEEP_VALUES`
   - `NUM_THREADS` — integer ≥ 1, or `None` to leave `OMP_NUM_THREADS` unset
2. Run from the repository root:

```bash
python3 experiments/parameter_sweep.py
python3 experiments/parameter_sweep.py --threads 8
```

`--threads N` overrides `NUM_THREADS`. It is **not** forwarded as a C++ flag.

3. Outputs land in:

```text
outputs/sweeps/<SWEEP_NAME>/<value>/
    einstein_ring.csv          # 1D
    true_2d_image.csv          # 2D
    *.pgm, run_summary.txt, run_metadata.json
    executable_stdout.txt, executable_stderr.txt
outputs/sweeps/<SWEEP_NAME>/summary.csv
```

`run_metadata.json` and `summary.csv` record `omp_num_threads` and `method`
(`symmetry_reduced_1d` or `true_2d`).

For a 1D `source-distance` sweep, include `"inf"` in `SWEEP_VALUES` to switch that run
to `--ray-model parallel`. Plot with `python3 experiments/plot_source_distance_sweep.py`.
The 2D path is already a parallel beam; `"inf"` is not a 2D CLI mode.

Meaning of the two observer-related knobs:

- `--observer-axial-distance`: how far the spacecraft is from the Sun/lens along the
  focal line.
- `--observer-distance`: how far the spacecraft is **off** the focal line
  (perpendicular offset). Use the 2D executable for off-axis / arc experiments.

---

# 3. Entry Point

## 1D executable

- **Binary:** `sgl_canonical_sgl_image`
- **Source:** `experiments/canonical_sgl_image.cpp`
- **CMake:** `add_executable(sgl_canonical_sgl_image …)` linking `sgl_imaging`

Order of construction:

1. Parse CLI into `CliOptions`; reject `observer-distance != 0`.
2. Build `Problem::PropagationProblem` (lens, source at `−S Ẑ`, observer at `D Ẑ + d X̂`).
3. Build `Schwarzschild::PropagationContext`, fallback termination, RK4, settings.
4. Sample rays: `RaySampler` (`point`) or `sample_parallel_rays` (`parallel`).
5. `Arrivals::collect_arrivals` → scan `residual_u(b)` → bisection → primary observer hit.
6. `expand_angular_azimuthally(signed_u_ang, azimuth_count)`.
7. `form_image(angular_samples, …)` → normalize → CSV/PGM/summary.

## 1D call-flow (actual symbols)

```text
main()
  ↓
parse_args() → CliOptions
  ↓
PropagationProblem(lens, source, observer, image_plane)
  ↓
RaySampler::sample  or  sample_parallel_rays
  ↓
Arrivals::collect_arrivals(...)
  │   PlaneCrossingTermination
  │   Rays::propagate_ensemble  (OpenMP if n > 1)
  │     outcomes[i] = Propagation::propagate(ensemble.at(i).initial_state, ...)
  │   localize_arrival → RayArrival
  ↓
scan residual_u(b) → brackets → bisection → select_primary_observer_hit
  ↓
observer_angular_coordinates → expand_angular_azimuthally
  ↓
form_image(vector<Vector2d>, resolution, resolution, extent)
  ↓
normalized_to_max → write_csv / write_pgm / write_summary
```

## 2D executable

- **Binary:** `sgl_true_2d_sgl_image`
- **Source:** `experiments/true_2d_sgl_image.cpp`
- **CMake:** `add_executable(sgl_true_2d_sgl_image …)` linking `sgl_imaging`

Order of construction:

1. Parse CLI; off-axis `observer-distance` is allowed.
2. Same `PropagationProblem` construction as 1D (observer may be offset).
3. `RayGrid2DSampler::sample` → `N²` search geodesics.
4. `collect_arrivals` on the search ensemble.
5. `observer_hit_seeds` then `refine_observer_launches` (OpenMP over seeds).
6. `observer_angular_coordinates` per refined hit — **no** azimuthal expansion.
7. `form_image` → `true_2d_image.csv` / `.pgm`.

If Newton produces zero refined hits, the executable exits with an error
(`No launch parameters refined to the observer.`). A 3×3 search grid often fails
this way; 5×5 is the smallest grid that typically yields on-axis hits.

## 2D call-flow (actual symbols)

```text
main()
  ↓
RayGrid2DSampler::sample(problem)
  │   for j, i: state_for(problem, b_u, b_v) → ensemble.add
  ↓
collect_arrivals → propagate_ensemble (OpenMP, n = N²)
  ↓
observer_hit_seeds(samples, arrivals, plane, N)
  ↓
refine_observer_launches(...)
  │   per_seed[i] = refine_launch_to_observer(..., seed_index=i)   # OpenMP
  │   compact in increasing i; sort by (residual.norm, seed_index); dedup
  ↓
observer_angular_coordinates(hit.arrival, observer)  for each refined hit
  ↓
form_image(angular_coordinates, ...)
```

Scientific computation starts at sampler `sample(problem)` and at
`Arrivals::collect_arrivals`. 2D Newton additionally calls `evaluate_launch` →
1-ray `propagate_ensemble` (`if(n > 1)` keeps that inner loop serial).

---

# 4. Physical Problem Construction

## Composition type

`Problem::PropagationProblem` (`physics/problem/PropagationProblem.h`) owns:

- `Geometry::Lens lens_`
- `Geometry::Source source_`
- `Geometry::Observer observer_`
- `Geometry::ImagePlane image_plane_`

It stores **geometry only**. It does not store integration settings, ray ensembles, or
images.

Canonical construction:

```cpp
Problem::make_aligned_problem(
    Spacetime::SchwarzschildParameters{.rs = 1.0},
    /* source_distance */ 30.0,
    /* observer_distance */ 30.0,
    /* half_width */ extent/2,
    /* half_height */ extent/2);
```

Implemented in `physics/problem/PropagationProblem.cpp`.

## Coordinate convention

Documented and implemented in `physics/geometry/WorldFrame.h`:

- World frame: right-handed Cartesian `(X, Y, Z)`, `X × Y = Z`.
- Optical axis = `+Z`.
- Lens at `(0, 0, 0)`.
- Observer at `(0, 0, +D)`.
- Source at `(0, 0, -S)`.
- Observer looks toward `-Z`.
- Image-plane axes: `u = +X`, `v = +Y`, `normal = u × v = +Z` (along incoming light).

Chart frame used for Schwarzschild spherical coordinates:

| Chart axis | World axis |
|---|---|
| `x_chart` | `+Z` (optical axis) |
| `y_chart` | `+X` (image-plane `u`) |
| `z_chart` | `+Y` (chart polar axis) |

This places the aligned source–lens–observer line on the chart equator
(`θ = π/2`), away from the polar singularity.

## Lens

- **Type:** `struct Geometry::Lens` (`physics/geometry/Lens.h`)
- **Fields:**
  - `Eigen::Vector3d position` — world position (canonical: origin)
  - `Spacetime::SchwarzschildParameters parameters` — currently `{ double rs; }`
- **Horizon:** `horizon_radius(lens) == parameters.rs`
- **Frame helpers:**
  - `to_lens_frame` = `world - lens.position`
  - `to_chart_frame` = `WorldFrame::world_to_chart(to_lens_frame(...))`
  - `from_chart_frame` = `lens.position + WorldFrame::chart_to_world(...)`

How the metric receives lens mass: `Schwarzschild::PropagationContext` is constructed
with `Spacetime::SchwarzschildParameters{.rs = 1.0}` and builds
`Spacetime::SchwarzschildMetric(parameters.rs)`. The executable passes the same `rs`
into both `make_aligned_problem` and `PropagationContext`. The metric does **not**
read `Lens` directly; only `rs` is used.

## Source

- **Type:** `struct Geometry::Source` (`physics/geometry/Source.h`)
- **Field:** `Eigen::Vector3d position` only (no spectrum, luminosity, etc.)
- **Canonical:** `(0, 0, -30)` for default run

How it affects ray generation: on the **1D point** path, `RaySampler::sample` converts
the source world position into chart spherical coordinates and uses that as the common
launch **point**; only `b` varies. On the **1D parallel** and **2D** paths, `source.position`
is the launch-plane origin; rays are offset in the plane and share one direction.

## Observer

- **Type:** `class Geometry::Observer` (`physics/geometry/Observer.h`)
- **Stored:** `position_`, `forward_`, `up_` (orthonormal; `right() = forward × up`)
- **Canonical construction:** `Observer::looking_at(observer_position, origin, +Y)`
  yields `forward = -Z`, `up = +Y`, `right = +X`

Relationship to image plane: `ImagePlane::attached_to(observer, half_w, half_h)` places
the plane origin at the observer position with axes `(right, up)`.

`PropagationProblem` validates that the image-plane normal is antiparallel to observer
forward (`normal · forward ≈ -1`) and that the plane origin lies on the optical axis
through the observer.

## ImagePlane

- **Type:** `class Geometry::ImagePlane` (`physics/geometry/ImagePlane.h`)
- **Stored:** `origin_`, `u_`, `v_`, `normal_ = u × v`, `half_width_`, `half_height_`
- **Canonical:** origin `(0,0,D)`, `u=+X`, `v=+Y`, `normal=+Z`, half-extent = `extent/2`

Operations:

| Method | Behavior |
|---|---|
| `to_plane_coordinates(world)` | `(Δ·u, Δ·v)` |
| `to_world(plane)` | `origin + u*u_axis + v*v_axis` |
| `signed_distance(world)` | `(world - origin) · normal` |
| `contains(plane)` | `|u| ≤ half_width` and `|v| ≤ half_height` |

**Important implementation fact:** observer-plane **crossing detection uses
`signed_distance` only**. The plane is treated as unbounded for termination.
`contains()` is available but not used by `PlaneCrossingTermination` or the
canonical executable. Image clipping happens later in `ImageFormation` via image
bounds, not via `ImagePlane::contains`.

---

# 5. Ray Generation

## `Ray`

`struct Rays::Ray` (`physics/rays/Ray.h`):

- `State initial_state` — Schwarzschild spherical chart `(t,r,θ,φ)` and tangent `U`
- `std::size_t id` — assigned by `RayEnsemble`, equals index in the ensemble

## `RayEnsemble`

`class Rays::RayEnsemble` (`physics/rays/RayEnsemble.h/.cpp`):

- Owns a `std::vector<Ray>`
- `add(State)` appends `Ray{state, id=size}` and returns the id
- Preserves insertion order; iteration yields rays in id order

## `RaySampler`

`class Rays::RaySampler` (`physics/rays/RaySampler.h/.cpp`) with
`struct Rays::RaySamplingConfig { ray_count, min_impact_parameter, max_impact_parameter }`.

`sample(problem)`:

1. Maps source world position → chart Cartesian via `Geometry::to_chart_frame`.
2. Converts to spherical via `CoordinateChart::cart_to_sphere`.
3. For each index `i ∈ [0, ray_count)`:
   - Builds `Schwarzschild::NullScatterInitialConditions` with shared `(t0,r0,θ0,φ0)`
     and `impact_parameter = impact_parameter_at(i)`.
   - Calls `Schwarzschild::build_null_scatter(lens.parameters, initial)`.
   - Adds the resulting `State` to the ensemble.

### Impact-parameter selection

`impact_parameter_at(index)`:

- `ray_count == 1` → `min_impact_parameter`
- otherwise linear sweep:
  `b_i = b_min + (i/(N-1)) * (b_max - b_min)`

Defaults in the 1D executable: `N=801`, `b ∈ [2.0, 20.0]`.

### What is fixed vs what varies

| Quantity | Behavior in current sampler |
|---|---|
| Source launch coordinates `(t0,r0,θ0,φ0)` | Fixed from problem source |
| Impact parameter `b` | Varies linearly |
| Orbital plane / `θ` motion | Fixed: `vθ = 0` in `build_null_scatter` |
| Azimuthal launch plane | Chart equator (see below) |

### How equatorial integration is enforced

Not by a runtime “force θ=π/2” constraint during RK4. It is enforced by **initial
conditions**:

1. Canonical source lies on the optical axis → after chart rotation,
   `θ0 = π/2` (equator).
2. `build_null_scatter` always sets `U_θ = vtheta = 0`.
3. With Schwarzschild spherical symmetry and equatorial initial data, the geodesic
   remains in the equatorial plane (`θ = π/2`, `U_θ = 0`).

Consequently, observer-plane arrivals from this family lie on the image-plane `u`
axis (`v ≈ 0`). That is why azimuthal expansion is required for a 2D ring **on the
1D path**.

## `RayGrid2DSampler`

`class Rays::RayGrid2DSampler` (`physics/rays/RayGrid2DSampler.h/.cpp`) with
`struct Rays::RayGrid2DSamplingConfig { samples_per_axis, max_impact_parameter }`.

This is the 2D search-grid sampler. It is **not** a point-source fan.

Cell-centered grid on `[−b_max, +b_max]²`:

```text
cell = 2 * max_impact_parameter / samples_per_axis
b_i = −b_max + (i + 0.5) * cell     for i = 0 … N−1
```

Row-major: `b_v` outer, `b_u` inner. `sample(problem)` assigns `id == index` and
stores `RayGrid2DSample{b_u, b_v, ray_id}` in `samples()`.

`state_for(problem, b_u, b_v)` (public, also used by Newton):

1. World position = `source.position + b_u·X̂ + b_v·Ŷ`.
2. World direction = `normalize(lens.position − source.position)` — **the same for every
   sample** (parallel incident rays).
3. Chart Cartesian → spherical → `Schwarzschild::build_custom(..., Null)` with `vt = 0`.

`--source-distance` is the launch-plane location. There is no 2D `--ray-model`.

Invariant: `outcomes[i]` from `propagate_ensemble` corresponds to `ensemble.at(i)`
with `id == i`. Indexed writes (not `push_back`) keep that identity under OpenMP.

---

# 6. Initial Conditions

## Path

```text
RaySampler::sample
  ↓
NullScatterInitialConditions { t0, r0, theta0, phi0, impact_parameter }
  ↓
Schwarzschild::build_null_scatter(parameters, initial)
  ↓
State { X=(t,r,θ,φ), U=(vt,vr,vθ,vφ) }
  ↓
RayEnsemble::add → Ray
```

Implementation: `physics/schwarzschild/InitialStates.cpp`,
`physics/schwarzschild/InitialConditions.h`.

## `build_null_scatter` equations (as coded)

With `f = 1 - rs/r0`:

- `E = 1`
- `L = b * E` where `b = impact_parameter` if `> 0`, else
  `b_crit + impact_parameter_offset` with `b_crit = (3√3/2) rs`
- `vt = E / f`
- `vφ = L / (r0² sin θ0)` (0 if `sin θ0 == 0`)
- `vr = -√(E² - f L²/r0²)` (incoming)
- `vθ = 0`

Returns:

```text
X = (t0, r0, θ0, φ0)
U = (vt, vr, 0, vφ)
```

## Physical vs numerical meaning

| Component | Meaning |
|---|---|
| `r0, θ0, φ0` | Launch location in Schwarzschild spherical coordinates |
| `t0` | Coordinate time at launch (canonical: `0`) |
| `b` / `L` | Impact parameter / angular momentum scale |
| `E` | Energy-like normalization fixed to `1` |
| `U` | Tangent with respect to the affine parameter used by RK4 |
| `vθ = 0` | Numerical choice confining motion to the equatorial plane |

Affine parameter is not stored as a separate field; it advances implicitly as the
integrator’s independent variable with step `settings.step_size`.

---

# 7. Schwarzschild Physics

## Mass parameter

`Spacetime::SchwarzschildParameters::rs` — Schwarzschild radius in geometrized units.
Canonical value: `1.0`.

## Metric interface

`Spacetime::Metric` (`physics/core/Metric.h`) exposes **only**:

```cpp
virtual double christoffel(int mu, int alpha, int beta, const Eigen::Vector4d& X) const = 0;
```

There is **no** implemented metric-tensor or inverse-metric evaluation API in this
repository. Null-cone projection uses an explicit Schwarzschild formula instead
(`project_onto_null_cone`).

## Christoffel symbols

`Spacetime::SchwarzschildMetric::christoffel` (`physics/metrics/SchwarzschildMetric.cpp`)
returns the non-zero Schwarzschild spherical Christoffel components used by the code
(with `α ≤ β` after an internal swap). Nonzero cases include:

- `Γ^t_{tr} = rs / (2 r (r - rs))`
- `Γ^r_{tt} = rs (r - rs) / (2 r³)`
- `Γ^r_{rr} = -rs / (2 r (r - rs))`
- `Γ^r_{θθ} = -(r - rs)`
- `Γ^r_{φφ} = -(r - rs) sin²θ`
- `Γ^θ_{rθ} = 1/r`
- `Γ^θ_{φφ} = -sinθ cosθ`
- `Γ^φ_{rφ} = 1/r`
- `Γ^φ_{θφ} = cosθ / (sinθ + 1e-8)`

All other combinations return `0`.

## Geodesic equations / RHS

`Dynamics::GeodesicDynamics::compute_derivative` (`physics/geodesics/GeodesicDynamics.cpp`):

```text
dX^μ/dλ = U^μ
dU^μ/dλ = - Γ^μ_{αβ}(X) U^α U^β
```

Implemented as nested loops over `μ,α,β ∈ {0,1,2,3}`:

```cpp
a[mu] -= Gamma * state.U[alpha] * state.U[beta];
return State(state.U, a);  // Xdot = U, Udot = a
```

## Null condition

Not enforced as a hard algebraic constraint inside `compute_derivative`.

In the canonical run, `PropagationContext` installs a `StepCorrection` that, every
`null_projection_interval` steps (default 1000), calls
`Schwarzschild::project_onto_null_cone(state, rs)`:

```text
f = 1 - rs/r
spatial = vr²/f + r² vθ² + r² sin²θ vφ²
vt = √(spatial / f)
```

and then forces `U^t = |U^t|` if negative.

## Conserved quantities

`build_null_scatter` chooses `E` and `L` when constructing the initial state.
The forward pipeline does **not** continuously monitor conserved energy/angular
momentum during integration. (Separate validation utilities exist under
`physics/validation/`, but they are not on the canonical image path.)

## Code path for one derivative evaluation

```text
State
  ↓
Integration::DerivativeFunc  (= dynamics.compute_derivative)
  ↓
GeodesicDynamics::compute_derivative
  ↓
metric_.christoffel(mu, alpha, beta, state.X)   // SchwarzschildMetric
  ↓
State{Xdot=U, Udot=a}
```

---

# 8. Numerical Integration

## State representation

`struct State` (`physics/core/GeodesicState.h`):

- `Eigen::Vector4d X` — coordinates
- `Eigen::Vector4d U` — tangent / four-velocity components
- Supports `+` and scalar `*` for RK4 arithmetic

## Derivative function

`Integration::DerivativeFunc = std::function<State(const State&)>`
(`physics/integrators/Integrator.h`).

In `Propagation::propagate`, it is a lambda closing over `dynamics.compute_derivative`.

## Integrator interface

```cpp
class Integration::Integrator {
  virtual State step(const State& state, double dt, const DerivativeFunc& derivative) const = 0;
};
```

Concrete type: `Integration::RK4Integrator` (`physics/integrators/RK4Integrator.cpp`).

## RK4 step (as coded)

```text
k1 = f(y)
k2 = f(y + k1 * dt/2)
k3 = f(y + k2 * dt/2)
k4 = f(y + k3 * dt)
y_next = y + (dt/6) * (k1 + 2 k2 + 2 k3 + k4)
```

where `f` returns a full `State` of derivatives and arithmetic acts componentwise on
`(X,U)`.

## Propagation loop ownership

`Propagation::propagate` (`physics/propagation/Propagator.cpp`):

```text
current = initial_state
previous = initial_state
for i in [0, max_steps):
    if termination.should_terminate(current):
        status = Terminated; break
    previous = current
    current = integrator.step(current, step_size, derivative)
    if correction: correction(current, i)
    steps_taken = i+1
return PropagationOutcome{final_state=current, steps_taken, status, previous_state=previous}
```

| Responsibility | Owner |
|---|---|
| Physics RHS | `DynamicsModel` / `GeodesicDynamics` |
| One step | `Integrator` / `RK4Integrator` |
| Step loop, budget, correction hook | `Propagation::propagate` |
| Stop predicate | `TerminationPolicy` |
| Ensemble loop | `Rays::propagate_ensemble`: preallocate `RayOutcomes(n)`; indexed write `outcomes[i]`; `#pragma omp parallel for schedule(dynamic) if(n > 1)` when `_OPENMP` is defined |

Canonical settings: `step_size=0.01`, `max_steps=300000`. Nested OpenMP is not enabled.
A 1-ray ensemble (Newton `evaluate_launch`) does not spawn a team.

Status enum (`Propagation::PropagationStatus`):

- `Terminated` — stop predicate fired
- `StepBudgetExhausted` — loop finished without termination

---

# 9. Termination and Observer-Plane Crossing

## Policies stacked in the canonical path

`Arrivals::collect_arrivals` builds:

```cpp
PlaneCrossingTermination(problem.lens(), problem.image_plane(), fallback_termination)
```

where the executable’s fallback is:

```cpp
RadiusBoundTermination(/* r_min */ 1.0001, /* r_max */ infinity)
```

(`rs * 1.0001` with `rs=1`).

`PlaneCrossingTermination::should_terminate` (`physics/arrivals/PlaneCrossingTermination.cpp`):

1. If `fallback_.should_terminate(state)` → true (horizon capture if `r ≤ r_min`;
   escape if `r ≥ r_max`, unused when `r_max = ∞`).
2. Else true when `plane.signed_distance(world_position(lens, state)) >= 0`.

World position mapping: `Arrivals::world_position` converts spherical chart state to
chart Cartesian, then `Geometry::from_chart_frame`.

## Crossing localization

After propagation, `localize_arrival` (`physics/arrivals/ArrivalCollector.cpp`):

1. Compute `p_curr = world_position(final_state)`, `d_curr = signed_distance(p_curr)`.
2. If `d_curr < 0` → `ArrivalStatus::NoCrossing` (did not finish on/beyond the plane).
3. Otherwise linearly interpolate between `previous_state` and `final_state` using
   signed distances:
   `t = clamp(d_prev / (d_prev - d_curr), 0, 1)`
4. Set:
   - `world_position = p_prev + t (p_curr - p_prev)`
   - `chart_state` = linearly interpolated `(X,U)`
   - `world_direction = normalize(chart_to_world spatial velocity)` via
     `Arrivals::world_direction`
   - `status = Arrived`

Only the **first** termination crossing is retained: propagation stops as soon as
`signed_distance >= 0`, so later crossings are never integrated.

## What happens in each case

| Situation | Propagation status | Arrival status | Recorded fields |
|---|---|---|---|
| Crosses observer plane | `Terminated` (plane predicate) | `Arrived` | full `RayArrival` |
| Hits near-horizon `r_min` before plane | `Terminated` (fallback) | usually `NoCrossing` if still `d<0` | id + status |
| Exhausts `max_steps` before plane | `StepBudgetExhausted` | `NoCrossing` if `d_curr < 0` | id + status |
| Starts already with `d≥0` | may terminate immediately | localization may yield `Arrived` with `t≈0` | depends on states |

`PropagationOutcome::previous_state` exists specifically so localization can bracket the
crossing. It equals `final_state` when `steps_taken == 0`.

Note: `Schwarzschild::PropagationContext` also builds an internal
`RadiusBoundTermination`, but the canonical executable does **not** call
`context.termination()`. It only uses `context.dynamics()` and `context.correction()`.

---

# 10. RayArrival

Defined in `physics/arrivals/RayArrival.h`.

```cpp
enum class ArrivalStatus { Arrived, NoCrossing };

struct RayArrival {
    std::size_t ray_id = 0;
    Eigen::Vector3d world_position = Zero;
    Eigen::Vector3d world_direction = Zero;
    State chart_state{};
    ArrivalStatus status = NoCrossing;
};
```

| Field | Meaning | Coordinate / units |
|---|---|---|
| `ray_id` | Same as originating `Ray::id` | dimensionless index |
| `world_position` | Localized intersection point | world Cartesian |
| `world_direction` | Unit spatial direction at arrival | world Cartesian |
| `chart_state` | Interpolated Schwarzschild state | spherical chart |
| `status` | Whether the plane was reached | enum |

Ownership: value types in a `std::vector` owned by the caller (executable or test).
`collect_arrivals` returns a new vector **index-aligned** with the ensemble:
`result[i]` corresponds to `ensemble.at(i)`.

Retained vs discarded:

- Retained: id, localized world hit, direction, interpolated chart state, status.
- Discarded: full trajectory history (canonical path uses non-recording
  `Propagation::propagate`, not `propagate_recorded`).
- On `NoCrossing`, only `ray_id` and `status` are meaningful; other fields are defaults.

---

# 11. Azimuthal Expansion (1D path only)

The current **1D image** does not rotate plane-hit coordinates. It maps the refined
observer-hit arrival to gnomonic angles and calls
`Arrivals::expand_angular_azimuthally(signed_u_ang, azimuth_count)`
(`physics/arrivals/ObserverAngularCoordinates.cpp`).

`Arrivals::expand_azimuthally` (`physics/arrivals/AzimuthalExpansion.cpp`) still exists:
it rotates **plane** `(u, 0)` arrivals into `PlaneArrival`s. That helper is tested and
available to `ImageFormation`, but the canonical executable’s imaging path uses the
**angular** expander. Do not confuse the two.

The 2D executable never calls either expander.

## Why 1D integrated arrivals are `(u, 0)`

Because the 1D sampler integrates only the chart-equatorial family (`vθ = 0`, source on
axis), each arrived world point maps through `ImagePlane::to_plane_coordinates` to
approximately `(u, 0)`. The radial structure `u(b)` is physically meaningful; the
missing azimuthal coordinate is not sampled by geodesic integration.

## Why that is enough for the aligned Schwarzschild problem

The aligned configuration is axisymmetric about the optical axis. For spherical
Schwarzschild, rotating an equatorial solution about that axis yields another valid
solution of the same physical problem. The 1D implementation therefore **does not
re-integrate** rotated geodesics; it rotates the equatorial **angular** coordinate.

That argument **fails off-axis**. The 1D executable rejects `--observer-distance != 0`.
The 2D path integrates independent launch-plane azimuths instead.

## Implemented transformation (what the 1D image uses)

```text
signed_u_ang = observer_angular_coordinates(refined_arrival).x()
for k = 0 .. N-1:
    ψ = 2π k / N
    emit (signed_u_ang * cos ψ, signed_u_ang * sin ψ)
```

`N` is `azimuth_count` (canonical default `720`). Signed `u_ang` is preserved at `k=0`.

## `PlaneArrival` (legacy / optional image input)

```cpp
struct Arrivals::PlaneArrival {
    std::size_t ray_id;
    Eigen::Vector2d plane_position;  // (u, v) in ImagePlane coordinates
};
```

`form_image` still accepts `vector<PlaneArrival>` as well as `vector<Vector2d>`.
Current executables pass gnomonic `Vector2d`s.

## Physical validity scope

Valid for **spherical Schwarzschild + aligned axisymmetric** 1D imaging. It is not a
substitute for 2D ray sampling in asymmetric (off-axis) problems.

---

# 11A. True 2D search geodesics and observer-hit refinement

This is the 2D counterpart of the 1D `b` scan + bisection. Files:
`physics/arrivals/ObserverLaunchRefiner.h/.cpp`.

## Search geodesics vs imaged rays

| Stage | What is integrated | Role |
|---|---|---|
| Search grid | `N×N` parallel rays at cell centers | Survey `‖r(b_u, b_v)‖` on the observer plane |
| Seeds | No extra integration | Starting guesses from the residual field |
| Newton trials | One geodesic per `evaluate_launch` | Move `(b_u, b_v)` toward `‖r‖ = 0` |
| Refined hits | The successful Newton geodesic | **Only these** go to `form_image` |

`r = image_plane.to_plane_coordinates(world_hit)`. Crossing the unbounded observer
**plane** (`ArrivalStatus::Arrived`) is not an observer hit. The image is the set of
launches with `‖r‖ ≤ hit_tolerance` (default `1e-6`).

Search rays are never an aperture: a near-miss is a different photon path and is not
binned.

## Seeds (`observer_hit_seeds`)

Built from the search arrivals (no extra geodesics):

1. Global minimum of residual norm.
2. Every 8-neighbor local minimum on the `N×N` residual grid.
3. Edge interpolations: for neighboring cells `a,b`, if the residual segment
   `r(t) = (1-t)r_a + t r_b` has a point closer to the origin than either endpoint,
   emit that interpolated `(b_u, b_v)`.

Duplicates at machine epsilon are skipped. A 3×3 grid often yields no refinable
seeds; 5×5 typically yields a few on-axis hits; denser grids find more distinct
launch-plane azimuths around the ring.

## Newton (`refine_launch_to_observer`)

Per seed, serial:

1. `evaluate_launch` → 1-ray ensemble → `collect_arrivals`.
2. If residual already ≤ tolerance, accept.
3. Else finite-difference Jacobian (`finite_difference_step`, default `1e-3`) with two
   extra launches; damped Gauss–Newton; up to 6 halved line-search trials per
   iteration; Broyden Jacobian update on accepted steps.
4. At most `max_iterations` (2D CLI default 12). Failure → `nullopt`.

`refine_observer_launches` runs seeds concurrently (OpenMP, `schedule(dynamic)`,
`if(seeds.size() > 1)`), then **serially** compacts in increasing `i`, sorts by
`(plane_residual.norm(), seed_index)`, and drops pairs closer than `0.25` of a search
cell. Compact/sort/dedup are not parallelized.

## Summary fields (2D `run_summary.txt`)

`rays_sampled` = `N²` search geodesics. `arrived_count` = plane crossings.
`seed_count` = Newton guesses. `refined_observer_hits` = image samples.
`median_angular_radius` / `radial_stddev` are computed from refined gnomonic radii.

---

# 11B. OpenMP parallelization

Independent geodesics and independent Newton seeds are the only parallel regions.
RK4 internals, Christoffel evaluation, image formation, and I/O stay serial.

| Site | File | Schedule |
|---|---|---|
| `propagate_ensemble` | `physics/rays/EnsemblePropagator.cpp` | `parallel for schedule(dynamic) if(n > 1)` |
| `refine_observer_launches` | `physics/arrivals/ObserverLaunchRefiner.cpp` | same, over seeds |

Guarded by `#if defined(_OPENMP)`. Without OpenMP, pragmas are omitted and the loops
are serial. CMake does not add OpenMP to `vcpkg.json`.

Thread count: environment variable `OMP_NUM_THREADS`, or `parameter_sweep.py --threads`
/ config `NUM_THREADS`. Nested parallelism is not enabled; Newton’s inner 1-ray
`propagate_ensemble` cannot spawn a nested team.

Result identity: indexed writes only (`outcomes[i]`, `per_seed[i]`). Test
`ensemble_parallel_invariance` checks bitwise 1-thread vs 4-thread ensemble outcomes.
On hybrid CPUs, physical-core counts are usually more efficient than using SMT
siblings (for example 8 vs 12 on an i5-13420H).

---

# 12. Image Formation

## Path

Both current executables pass **gnomonic angular** `std::vector<Eigen::Vector2d>` into
`form_image`. `PlaneArrival` overloads remain for older plane-coordinate workflows.

```text
angular (u_ang, v_ang)
  ↓
Imaging::ImageFormation::pixel_for(image, position)
  ↓
Image::at(x,y) += 1.0
  ↓
Image::normalized_to_max()
```

Files: `physics/imaging/Image.h/.cpp`, `physics/imaging/ImageFormation.h/.cpp`.

## `Image` representation

`class Imaging::Image` stores:

- `width_`, `height_`
- physical bounds `u_min_`, `u_max_`, `v_min_`, `v_max_`
- `std::vector<double> intensity_` — row-major, index `y * width + x`

Helpers: `du()`, `dv()`, `pixel_center(x,y)`, `at(x,y)`, `max_intensity()`,
`normalized_to_max()`.

`form_image(positions, width, height, coordinate_extent)` constructs square bounds:

```text
u,v ∈ [-extent/2, +extent/2]
```

1D default: `extent=0.8`, resolution `1024×1024`. 2D default: `extent=0.8`,
resolution `64×64`. These are **angular** tangent-plane coordinates, not
geometrized screen metres.

## Pixel mapping (exact)

```text
du = (u_max - u_min) / width
dv = (v_max - v_min) / height

pixel center: u = u_min + (x + 0.5) du
              v = v_min + (y + 0.5) dv

coordinate → pixel (half-open):
  if u < u_min or u >= u_max → out of bounds
  if v < v_min or v >= v_max → out of bounds
  x = floor((u - u_min) / du)
  y = floor((v - v_min) / dv)
```

Out-of-bounds arrivals are silently ignored. Intensity contribution is exactly `+1.0`
per in-bounds arrival (no weights, PSF, or radiometry).

## Normalization

`normalized_to_max()` divides every pixel by `max_intensity` if max `> 0`; otherwise
returns an all-zero copy. Canonical output writes the **normalized** image.

---

# 13. Output and Visualization

Writers are **local static functions** in each executable
(`experiments/canonical_sgl_image.cpp`, `experiments/true_2d_sgl_image.cpp`).
There is no separate rendering library.

## Scientific data: CSV

`write_csv(path, normalized_image)`:

- Metadata comment lines:
  `# width,...`, `# height,...`, `# u_min,...`, `# u_max,...`, `# v_min,...`,
  `# v_max,...`, `# normalized,true`
- Then `height` rows of comma-separated doubles (`y = 0 … height-1`).
- This is the machine-readable scientific product.

## Visualization: PGM

`write_pgm(path, normalized_image)`:

- ASCII `P2`
- Max gray value `65535`
- Pixel value = `round(clamp(I,0,1) * 65535)`
- Rows written from `y = height-1` down to `0` so positive `v` appears upward

## Summary

`write_summary` records CLI options, counts, `raw_image_max`, and output paths as
`key=value` lines. The 2D summary also records `rays_sampled`, `arrived_count`,
`seed_count`, `refined_observer_hits`, `median_angular_radius`, and `radial_stddev`.

## Dependencies

- C++ `<filesystem>`, `<fstream>` only for I/O
- Optional OpenMP for independent geodesics / Newton seeds (CPU)
- No GPU, OpenGL, textures, or UI components

---

# 14. Complete Data-Flow Trace

Representative **1D** ray for the default canonical configuration (`extent=0.8` so
image-plane half-extent is `0.4`; `b` scan is `[2, 20]`).

```text
canonical configuration
  Lens{(0,0,0), rs=1}, Source{(0,0,-30)}, Observer{(0,0,30), forward=-Z, up=+Y}
  ImagePlane{origin=(0,0,30), u=+X, v=+Y, normal=+Z, half=0.4}

impact-parameter scan (ray_count=801)
  b_i in [2, 20]
  RaySampler::sample → build_null_scatter → collect_arrivals (OpenMP)

observer-hit solve
  residual_u(b) brackets → bisection → primary refined arrival
  observer_angular_coordinates → signed u_ang
  expand_angular_azimuthally(u_ang, 720)

form_image(..., 1024, 1024, 0.8)
  each angular sample: pixel_for → intensity += 1
  normalized_to_max → einstein_ring.{csv,pgm}
```

The 1D scan geodesics are **not** binned. Only the refined observer-hit direction,
copied around the axis, is imaged.

**2D** counterpart: `N×N` `RayGrid2DSampler` search geodesics → residual field →
seeds → Newton → `refined_observer_hits` angular coordinates → `form_image` with
**no** azimuthal expansion. Search geodesics are not binned.

---

# 15. Complete Component Map

| Component | File | Responsibility | Consumes | Produces |
|---|---|---|---|---|
| `sgl_canonical_sgl_image` | `experiments/canonical_sgl_image.cpp` | 1D CLI, bisection, azimuthal fill, I/O | CLI args | `einstein_ring.*` |
| `sgl_true_2d_sgl_image` | `experiments/true_2d_sgl_image.cpp` | 2D CLI, search+Newton, I/O | CLI args | `true_2d_image.*` |
| `parameter_sweep.py` | `experiments/parameter_sweep.py` | Repeat CLI runs; set `OMP_NUM_THREADS` | config / `--threads` | sweep dirs + `summary.csv` |
| `PropagationProblem` | `physics/problem/PropagationProblem.*` | Own lens/source/observer/plane | geometry types | validated problem |
| `Lens` / `Source` / `Observer` / `ImagePlane` | `physics/geometry/*` | Geometry | world poses | frames, `(u,v)`, signed distance |
| `RaySampler` | `physics/rays/RaySampler.*` | 1D `b` sweep (`build_null_scatter`) | problem | `RayEnsemble` |
| `RayGrid2DSampler` | `physics/rays/RayGrid2DSampler.*` | 2D parallel launch grid (`build_custom`) | problem | ensemble + `RayGrid2DSample[]` |
| `propagate_ensemble` | `physics/rays/EnsemblePropagator.*` | Indexed OpenMP per-ray propagate | ensemble | `RayOutcomes` (`outcomes[i]` ↔ ray `i`) |
| `build_null_scatter` / `build_custom` | `physics/schwarzschild/InitialStates.cpp` | Null `State` | IC + `rs` | `State` |
| `collect_arrivals` | `physics/arrivals/ArrivalCollector.*` | Cross + localize | ensemble + problem | `RayArrival[]` |
| `observer_angular_coordinates` / `expand_angular_azimuthally` | `physics/arrivals/ObserverAngularCoordinates.*` | Gnomonic angles; 1D ring fill | `RayArrival` | `Vector2d` samples |
| `expand_azimuthally` / `PlaneArrival` | `physics/arrivals/AzimuthalExpansion.*` | Plane-coordinate symmetry expand (not used by current image mains) | `RayArrival[]` | `PlaneArrival[]` |
| `observer_hit_seeds` / `refine_observer_launches` | `physics/arrivals/ObserverLaunchRefiner.*` | 2D search residuals → Newton hits | grid + arrivals | `RefinedObserverHit[]` |
| `ImageFormation` | `physics/imaging/ImageFormation.*` | Bin `Vector2d` or `PlaneArrival` | samples | `Image` |

Numerical kernel rows (`SchwarzschildMetric`, `GeodesicDynamics`, `RK4Integrator`,
`propagate`, null projection) are unchanged from the previous map.

---

# 16. Scientific vs Numerical vs Presentation Layers

### Scientific / domain

- `PropagationProblem`, `Lens`, `Source`, `Observer`, `ImagePlane`, `WorldFrame`
- `Ray` / `RayEnsemble` / `RaySampler` / `RayGrid2DSampler`
- Schwarzschild launch ICs (`build_null_scatter`, `build_custom`)
- `RayArrival`, observer-hit refinement, gnomonic angular coordinates
- 1D azimuthal angular expansion (symmetry fill, not extra geodesics)
- `Image` as a scientific intensity product

### Numerical

- `State`, `Metric` / `SchwarzschildMetric`, `GeodesicDynamics`
- `Integrator` / `RK4Integrator`
- `Propagation::propagate`, `IntegrationSettings`, `TerminationPolicy`
- null-cone step correction
- linear crossing localization arithmetic
- OpenMP over independent geodesics / Newton seeds (same numerics, concurrent)

### Data transformation

- `CoordinateChart` cart↔sphere
- `ChartMapping` chart→world
- `expand_angular_azimuthally` (1D sky-circle fill)
- `ImageFormation::pixel_for` / `accumulate` / `form_image`
- `Image::normalized_to_max`

### Presentation / output

- `write_csv`, `write_pgm`, `write_summary` in each executable
- PGM gray-level scaling for viewing

### Dependency direction (as implemented)

```text
presentation (executable I/O)
        ↑
data transformation (imaging, angular expansion, chart maps)
        ↑
scientific domain (problem, rays, arrivals, launch refinement)
        ↑
numerical kernel (metric, dynamics, RK4, propagate)
```

Geometry (`sgl_geometry`) does not depend on the numerical kernel. Imaging depends on
arrivals, which depend on rays, which depend on both geometry and physics.

---

# 17. Current Assumptions and Limitations

These are facts of the current implementation, not recommendations.

| Assumption / limit | What it means scientifically |
|---|---|
| Schwarzschild-only lens | Gravity is exactly static, spherically symmetric vacuum; no spin, no multipoles, no plasma. |
| Two image paths | 1D rotates one equatorial observer-hit. 2D samples launch-plane azimuths with real geodesics. |
| 1D spherical-symmetry fill | 1D ring smoothness is `azimuth_count`, not independent geodesics. Off-axis 1D is rejected. |
| 2D search ≠ image | `N×N` search geodesics survey residuals; only refined observer hits are binned. Coarse `N` yields few ring samples. |
| 2D parallel incidence | All 2D rays share source→lens direction. `--source-distance` is launch-plane `z`, not a point-source fan, and not true `S=∞`. |
| Geometric optics | Light is treated as rays; no wave optics, interference, diffraction, or PSF. |
| Unit intensity weights | Each angular sample contributes `1`; not a calibrated flux or magnification map. |
| Finite timestep / budget | RK4 with fixed `step_size`; `NoCrossing` if `max_steps` is too small. |
| Unbounded plane for crossing | Arrival detection ignores `ImagePlane` half-extents; observer-hit is a later residual test. |
| OpenMP optional | Missing OpenMP builds serial. Nested teams are off. Thread count is `OMP_NUM_THREADS` only. |
| No trajectory storage on the image path | Only bracket states around termination are kept for localization. |

---

# 18. Glossary

Definitions match the **implemented** types/functions.

| Term | Definition |
|---|---|
| **PropagationProblem** | Validated composition of `Lens`, `Source`, `Observer`, and `ImagePlane`. |
| **Search geodesic** | A 2D grid launch used to map observer-plane miss distance. Not an image sample. |
| **Plane residual** | `image_plane.to_plane_coordinates(world_hit)`; `‖r‖=0` is an observer hit. |
| **Seed** | A `(b_u, b_v)` Newton starting guess from the search residual field. |
| **Refined observer hit** | A launch Newton moved until `‖r‖ ≤ hit_tolerance`; these are the 2D image samples. |
| **RaySampler** | 1D linear `b` sweep via `build_null_scatter`. |
| **RayGrid2DSampler** | 2D cell-centered parallel launch-plane grid via `build_custom`. |
| **RayArrival** | Per-ray observer-**plane** result (crossing, not necessarily observer hit). |
| **Gnomonic angular coordinates** | `(u_ang, v_ang) = (tan θ_right, tan θ_up)` from incoming direction. |
| **Azimuthal angular expansion** | `expand_angular_azimuthally`: rotate one signed `u_ang` into a circle (1D path). |
| **PlaneArrival** | Optional plane-coordinate sample; current executables image `Vector2d` angles instead. |
| **ImageFormation** | Half-open pixel binning of `Vector2d` or `PlaneArrival` with unit accumulation. |

---

# Appendix: Quick file index for onboarding

Start here, in order:

1. `docs/HOW_THE_EINSTEIN_RING_IS_FORMED.md` — conceptual 1D vs 2D, search vs ring
2. `experiments/canonical_sgl_image.cpp` — 1D wiring
3. `experiments/true_2d_sgl_image.cpp` — 2D wiring
4. `physics/rays/RaySampler.cpp` / `RayGrid2DSampler.cpp` — ensembles
5. `physics/schwarzschild/InitialStates.cpp` — null initial data
6. `physics/arrivals/ArrivalCollector.cpp` — propagate + localize
7. `physics/rays/EnsemblePropagator.cpp` — OpenMP ensemble loop
8. `physics/arrivals/ObserverLaunchRefiner.cpp` — 2D seeds + Newton
9. `physics/arrivals/ObserverAngularCoordinates.cpp` — gnomonic angles + 1D ring fill
10. `physics/imaging/ImageFormation.cpp` — pixels
11. `tests/canonical_image_pipeline.cpp` / `ensemble_parallel_invariance.cpp`
12. `experiments/parameter_sweep.py` — sweeps and `--threads`

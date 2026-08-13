# SGL Forward Pipeline — Developer Guide

This document describes the **implemented** Solar Gravitational Lensing (SGL) forward
pipeline in this repository. It is reverse-engineered from the current source and
canonical executable. It does not propose architecture changes.

A new developer should be able to read this once, then navigate the relevant source
without relying on prior project history.

---

# 1. Overview

The SGL forward pipeline computes a **geometric-optics intensity image** of an Einstein
ring for an aligned point-source / Schwarzschild-lens / observer configuration.

Scientifically, it:

1. Places a point source, Schwarzschild lens, and observer on a shared optical axis.
2. Samples a one-parameter family of null geodesics by impact parameter.
3. Integrates each geodesic in the Schwarzschild spherical chart.
4. Detects first arrival at the observer image plane.
5. Finds the observer-hit ray via `residual_u(b)` root localization.
6. Maps the incoming photon direction to observer-centered gnomonic angular coordinates.
7. Exploits spherical symmetry to rotate the equatorial angular coordinate into a 2D ring.
8. Bins angular coordinates into pixels with unit weights and writes a normalized image.

## Pipeline diagram (implemented stages)

```text
Physical problem
      ↓  Problem::make_aligned_problem  (physics/problem/PropagationProblem.cpp)
Ray sampling
      ↓  Rays::RaySampler::sample       (physics/rays/RaySampler.cpp)
Initial null states
      ↓  Schwarzschild::build_null_scatter  (physics/schwarzschild/InitialStates.cpp)
Schwarzschild geodesic integration
      ↓  Arrivals::collect_arrivals → Rays::propagate_ensemble → Propagation::propagate
         + Dynamics::GeodesicDynamics + Integration::RK4Integrator
Observer-plane crossing
      ↓  Arrivals::PlaneCrossingTermination + Arrivals::localize_arrival
Ray arrivals
      ↓  std::vector<Arrivals::RayArrival>
Observer-hit root search
      ↓  residual_u(b) scan + bisection (canonical executable)
Angular coordinates
      ↓  Arrivals::observer_angular_coordinates  (physics/arrivals/ObserverAngularCoordinates.cpp)
Azimuthal symmetry expansion
      ↓  Arrivals::expand_angular_azimuthally
2D angular samples
      ↓  std::vector<Eigen::Vector2d>
Image formation
      ↓  Imaging::ImageFormation::form_image  (physics/imaging/ImageFormation.cpp)
Intensity image
      ↓  Imaging::Image::normalized_to_max
Einstein ring
      ↓  write_csv / write_pgm          (experiments/canonical_sgl_image.cpp)
```

## Libraries involved

| CMake target | Role |
|---|---|
| `sgl_geometry` | `Lens`, `Source`, `Observer`, `ImagePlane`, `PropagationProblem` |
| `sgl_physics` | metric, dynamics, RK4, propagator, Schwarzschild helpers |
| `sgl_rays` | `Ray`, `RayEnsemble`, `RaySampler`, `propagate_ensemble` |
| `sgl_arrivals` | plane crossing, `RayArrival`, angular coordinates, azimuthal expansion |
| `sgl_imaging` | `Image`, `ImageFormation` |
| `sgl_canonical_sgl_image` | executable that wires the full path and writes outputs |

Dependency direction in CMake: `sgl_imaging` → `sgl_arrivals` → `sgl_rays` →
(`sgl_geometry`, `sgl_physics`). Geometry does **not** link the numerical kernel.

---

# 2. How to Run It

Verified against the repository build layout (`build/` out of source).

## Configure and build

```bash
cmake -B build -S .
cmake --build build
```

Prerequisites: C++20, CMake ≥ 3.22, Eigen3 (`find_package(Eigen3 CONFIG REQUIRED)` in
`CMakeLists.txt`).

## Run tests (optional, recommended)

```bash
ctest --test-dir build --output-on-failure
```

The end-to-end image path is covered by `canonical_image_pipeline`
(`tests/canonical_image_pipeline.cpp`).

## Run the canonical SGL experiment

```bash
./build/sgl_canonical_sgl_image --output-dir outputs/sgl_forward
```

## Locate outputs

Under the chosen `--output-dir` (default `outputs/sgl_forward`):

| File | Role |
|---|---|
| `einstein_ring.csv` | Normalized scalar intensity grid (scientific data) |
| `einstein_ring.pgm` | ASCII PGM visualization (`P2`, 0…65535) |
| `run_summary.txt` | CLI settings, arrival counts, raw max, paths |

A successful default run produces (example from an actual execution):

- `image_observable=observer_angular_gnomonic`
- `rays_sampled=801`
- `observer_hit_candidate_count=1` (or more; one selected)
- `observer_hit_count=1`
- `angular_samples=720`
- `raw_image_max=1` (after normalization in CSV)

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
  (on-axis). Off-axis runs are rejected for the canonical angular image path.

Units are geometrized (`G = c = 1`), as stated on `Spacetime::SchwarzschildParameters`
and `Rays::RaySamplingConfig`.

## Parameter sweeps

`experiments/parameter_sweep.py` is a thin Python orchestrator around the same
executable. It does not compute physics; it only launches repeated CLI runs.

1. Edit the configuration block at the top of `experiments/parameter_sweep.py`:
   - `BASE_PARAMS` — fixed CLI values for every run
   - `SWEEP_NAME` — output folder name under `outputs/sweeps/`
   - `SWEEP_PARAMETER` — one CLI flag name without `--`
   - `SWEEP_VALUES` — values to substitute for that flag
2. Run from the repository root:

```bash
python3 experiments/parameter_sweep.py
```

3. Outputs land in:

```text
outputs/sweeps/<SWEEP_NAME>/<value>/
    einstein_ring.csv
    einstein_ring.pgm
    run_summary.txt
    run_metadata.json
    executable_stdout.txt
    executable_stderr.txt
outputs/sweeps/<SWEEP_NAME>/summary.csv
```

4. To define a new sweep, change only `SWEEP_NAME`, `SWEEP_PARAMETER`, and
   `SWEEP_VALUES` (and optionally `BASE_PARAMS`). Parameter names must match the
   executable CLI above.

For a `source-distance` sweep, include the string `"inf"` in `SWEEP_VALUES` to
add a source-at-infinity case. That run uses `--ray-model parallel` (launch-plane
distance still comes from `BASE_PARAMS["source-distance"]`) and is stored under
`outputs/sweeps/source_distance/inf/`. Plot with:

```bash
python3 experiments/plot_source_distance_sweep.py
```

Meaning of the two observer-related knobs:

- `--observer-axial-distance`: how far the spacecraft is from the Sun/lens along the
  focal line.
- `--observer-distance`: how far the spacecraft is **off** the focal line
  (perpendicular offset). Use this for off-axis / arc experiments.

---

# 3. Entry Point

## Executable

- **Binary:** `sgl_canonical_sgl_image`
- **Source:** `experiments/canonical_sgl_image.cpp`
- **CMake:** `add_executable(sgl_canonical_sgl_image …)` linking `sgl_imaging`

## Where execution begins

`int main(int argc, char** argv)` at `experiments/canonical_sgl_image.cpp`.

Order of construction:

1. Parse CLI into `CliOptions`.
2. Validate parameter ranges.
3. Build `Problem::PropagationProblem` via `Problem::make_aligned_problem(...)`.
4. Build `Schwarzschild::PropagationContext` (metric, dynamics, null-projection correction).
5. Build fallback `Propagation::RadiusBoundTermination`, `Integration::RK4Integrator`,
   `Propagation::IntegrationSettings`.
6. Build `Rays::RaySampler` and call `sample(problem)` → `Rays::RayEnsemble`.
7. Call `Arrivals::collect_arrivals(...)` → `std::vector<Arrivals::RayArrival>`.
8. Call `Arrivals::expand_azimuthally(...)` → `std::vector<Arrivals::PlaneArrival>`.
9. Call `Imaging::ImageFormation::form_image(...)` → `Imaging::Image`.
10. Normalize with `Image::normalized_to_max()`.
11. Write CSV, PGM, and summary via local static helpers.

Scientific computation starts at `sampler.sample(problem)` for ray construction and at
`Arrivals::collect_arrivals` for geodesic integration.

## Call-flow diagram (actual symbols)

```text
main()
  ↓
parse_args() → CliOptions
  ↓
Problem::make_aligned_problem(rs=1, S=30, D=30, half=extent/2, half=extent/2)
  ↓
Schwarzschild::PropagationContext(parameters, PropagationOptions)
  ↓
Rays::RaySampler(RaySamplingConfig{ray_count, b_min, b_max})
  ↓
RaySampler::sample(problem)
  │   for each impact parameter:
  │     Schwarzschild::build_null_scatter → State
  │     RayEnsemble::add(State) → Ray{initial_state, id}
  ↓
Arrivals::collect_arrivals(ensemble, problem, context.dynamics(),
                           fallback, settings, RK4Integrator, context.correction())
  │   PlaneCrossingTermination(lens, image_plane, fallback)
  │   Rays::propagate_ensemble(...)
  │     for each Ray:
  │       Propagation::propagate(initial_state, dynamics, termination, settings, integrator, correction)
  │         loop: TerminationPolicy::should_terminate → Integrator::step → StepCorrection
  │   for each outcome:
  │     Arrivals::localize_arrival(ray_id, lens, plane, outcome) → RayArrival
  ↓
Arrivals::expand_azimuthally(arrivals, image_plane, azimuth_count) → PlaneArrival[]
  ↓
Imaging::ImageFormation::form_image(plane_arrivals, resolution, resolution, extent)
  │   Image(...)
  │   accumulate → pixel_for → intensity += 1
  ↓
Image::normalized_to_max()
  ↓
write_csv / write_pgm / write_summary
```

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

How it affects ray generation: `RaySampler::sample` converts the source world position
into chart spherical coordinates and uses that as the common launch point
`(t0, r0, θ0, φ0)` for every ray. Only the impact parameter varies per ray.

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

Defaults in the executable: `N=41`, `b ∈ [10.2, 11.6]`.

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
axis (`v ≈ 0`). That is why azimuthal expansion is required for a 2D ring.

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
| Ensemble loop | `Rays::propagate_ensemble` |

Canonical settings: `step_size=0.01`, `max_steps=300000`.

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

# 11. Azimuthal Expansion

## Why integrated arrivals are `(u, 0)`

Because the sampler integrates only the chart-equatorial family (`vθ = 0`, source on
axis), each arrived world point maps through `ImagePlane::to_plane_coordinates` to
approximately `(u, 0)`. The radial structure `u(b)` is physically meaningful; the
missing azimuthal coordinate is not sampled by geodesic integration.

## Why that is enough for the aligned Schwarzschild problem

The aligned configuration is axisymmetric about the optical axis. For spherical
Schwarzschild, rotating an equatorial solution about that axis yields another valid
solution of the same physical problem. The implementation therefore **does not
re-integrate** rotated geodesics; it rotates plane coordinates after arrival.

## Implemented transformation

`Arrivals::expand_azimuthally` (`physics/arrivals/AzimuthalExpansion.cpp`):

```text
for each RayArrival with status == Arrived:
    u = plane.to_plane_coordinates(world_position).x()   // v discarded
    if u == 0:
        emit one PlaneArrival{ray_id, (0,0)}
    else:
        for k = 0 .. N-1:
            ψ = 2π k / N
            emit PlaneArrival{ray_id, (u cos ψ, u sin ψ)}
```

`N` is `azimuth_count` (canonical default `720`).

## `PlaneArrival`

```cpp
struct Arrivals::PlaneArrival {
    std::size_t ray_id;
    Eigen::Vector2d plane_position;  // (u, v) in ImagePlane coordinates
};
```

- Association to the original ray is preserved by copying `ray_id`.
- Output is **not** index-aligned with `RayArrival[]` (non-arrivals are skipped;
  each arrival may expand to `N` plane points).
- No additional geodesic integration occurs.

## Physical validity scope

Valid for the current **spherical Schwarzschild + aligned axisymmetric** setup that
the sampler actually produces. It is an implementation mechanism reconstructing the
2D locus implied by symmetry, not a general substitute for 2D ray sampling in
asymmetric problems.

---

# 12. Image Formation

## Path

```text
PlaneArrival.plane_position (u,v)
  ↓
Imaging::ImageFormation::pixel_for(image, position)
  ↓
Image::at(x,y) += 1.0
  ↓
Imaging::Image
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

`form_image(arrivals, width, height, physical_extent)` constructs square bounds:

```text
u,v ∈ [-extent/2, +extent/2]
```

Canonical: `extent=40` → `[-20, 20]²`, resolution `512×512`.

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

All writers are **local static functions** in `experiments/canonical_sgl_image.cpp`.
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
`key=value` lines.

## Dependencies

- C++ `<filesystem>`, `<fstream>` only for I/O
- No GPU, OpenGL, textures, or UI components

---

# 14. Complete Data-Flow Trace

Representative single ray for the default canonical configuration.

```text
canonical configuration
  make_aligned_problem(rs=1, S=30, D=30, half=20, half=20)
  → Lens{(0,0,0), rs=1}, Source{(0,0,-30)}, Observer{(0,0,30), forward=-Z, up=+Y}
  → ImagePlane{origin=(0,0,30), u=+X, v=+Y, normal=+Z}

impact parameter (example index i=0)
  b = 10.2

RaySampler::sample
  source_chart = to_chart_frame(source) → ≈ (-30, 0, 0)
  source_spherical = cart_to_sphere → r0≈30, θ0=π/2, φ0=±π
  NullScatterInitialConditions{t0=0, r0, θ0, φ0, b=10.2}

build_null_scatter
  f = 1 - 1/r0
  E=1, L=b
  State X=(0,r0,π/2,φ0), U=(E/f, -√(E²-f L²/r0²), 0, L/(r0² sinθ0))

RayEnsemble::add
  Ray{initial_state, id=0}

collect_arrivals / propagate_ensemble / Propagation::propagate
  PlaneCrossingTermination + RK4 step_size=0.01
  each step:
    GeodesicDynamics::compute_derivative via SchwarzschildMetric::christoffel
    optional project_onto_null_cone every 1000 steps
  stop when world z reaches observer plane (signed_distance ≥ 0)
  PropagationOutcome{final_state, previous_state, Terminated, steps_taken}

localize_arrival
  RayArrival{
    ray_id=0,
    world_position ≈ (u, 0, 30),
    world_direction (unit),
    chart_state (interpolated),
    status=Arrived
  }

expand_azimuthally(..., azimuth_count=720)
  u = to_plane_coordinates(world_position).x()
  for k=0..719:
    PlaneArrival{ray_id=0, (u cos ψ_k, u sin ψ_k)}

form_image(..., 512, 512, 40)
  for each PlaneArrival:
    pixel_for → (x,y) or discard
    Image.at(x,y) += 1

normalized_to_max
  I ← I / max(I)

write_csv / write_pgm
  outputs/sgl_forward/einstein_ring.{csv,pgm}
```

Repeating for all 41 impact parameters fills an annular band; azimuthal expansion
turns each equatorial hit into a circle of samples.

---

# 15. Complete Component Map

| Component | File | Responsibility | Consumes | Produces |
|---|---|---|---|---|
| `sgl_canonical_sgl_image` / `main` | `experiments/canonical_sgl_image.cpp` | Wire pipeline, CLI, I/O | CLI args | CSV/PGM/summary |
| `make_aligned_problem` | `physics/problem/PropagationProblem.cpp` | Canonical geometry | `rs`, distances, half-extents | `PropagationProblem` |
| `PropagationProblem` | `physics/problem/PropagationProblem.h` | Own lens/source/observer/plane | geometry types | validated problem |
| `Lens` | `physics/geometry/Lens.h` | Mass + position + frame maps | world points | chart points / `rs` |
| `Source` | `physics/geometry/Source.h` | Point-source position | — | position |
| `Observer` | `physics/geometry/Observer.h/.cpp` | Pose | position/target/up | forward/up/right |
| `ImagePlane` | `physics/geometry/ImagePlane.h/.cpp` | Plane basis & maps | world / plane coords | `(u,v)`, signed distance |
| `WorldFrame` | `physics/geometry/WorldFrame.h` | Axis conventions + rotations | vectors | chart↔world |
| `RaySamplingConfig` / `RaySampler` | `physics/rays/RaySampler.*` | Impact-parameter sweep | `PropagationProblem` | `RayEnsemble` |
| `Ray` / `RayEnsemble` | `physics/rays/Ray.h`, `RayEnsemble.*` | Own initial states + ids | `State` | ordered rays |
| `NullScatterInitialConditions` | `physics/schwarzschild/InitialConditions.h` | Launch parameters | sampler fields | IC struct |
| `build_null_scatter` | `physics/schwarzschild/InitialStates.cpp` | Build null `State` | IC + `rs` | `State` |
| `CoordinateChart` | `physics/metrics/CoordinateChart.*` | Cart↔sphere maps | `State` | `State` |
| `SchwarzschildParameters` | `physics/core/SchwarzschildParameters.h` | Mass parameter | — | `rs` |
| `SchwarzschildMetric` | `physics/metrics/SchwarzschildMetric.*` | Christoffels | `X` | `Γ` |
| `GeodesicDynamics` | `physics/geodesics/GeodesicDynamics.*` | Geodesic RHS | `State` | `dState/dλ` |
| `PropagationContext` | `physics/schwarzschild/PropagationContext.*` | Compose metric/dynamics/correction | options + `rs` | dynamics, correction |
| `project_onto_null_cone` | `physics/schwarzschild/NullConstraint.*` | Null projection | `State`, `rs` | corrected `U^t` |
| `RK4Integrator` | `physics/integrators/RK4Integrator.*` | One RK4 step | state, dt, `f` | next state |
| `propagate` | `physics/propagation/Propagator.*` | Step loop | state + policies | `PropagationOutcome` |
| `RadiusBoundTermination` | `physics/propagation/TerminationPolicy.*` | Horizon/escape stop | `State` | bool |
| `propagate_ensemble` | `physics/rays/EnsemblePropagator.*` | Per-ray propagate | ensemble | `RayOutcomes` |
| `PlaneCrossingTermination` | `physics/arrivals/PlaneCrossingTermination.*` | Plane + fallback stop | `State` | bool |
| `world_position` / `world_direction` | `physics/arrivals/ChartMapping.*` | Chart→world maps | lens + `State` | vectors |
| `localize_arrival` / `collect_arrivals` | `physics/arrivals/ArrivalCollector.*` | Cross + localize | ensemble + problem | `RayArrival[]` |
| `expand_azimuthally` / `PlaneArrival` | `physics/arrivals/AzimuthalExpansion.*` | 1D→2D symmetry expand | `RayArrival[]` | `PlaneArrival[]` |
| `Image` | `physics/imaging/Image.*` | Intensity grid | bounds + size | scalar field |
| `ImageFormation` | `physics/imaging/ImageFormation.*` | Bin arrivals | `PlaneArrival[]` | `Image` |

---

# 16. Scientific vs Numerical vs Presentation Layers

### Scientific / domain

- `PropagationProblem`, `Lens`, `Source`, `Observer`, `ImagePlane`, `WorldFrame`
- `Ray` / `RayEnsemble` / `RaySampler` (problem sampling)
- Schwarzschild geometry inputs (`rs`, null scatter ICs)
- `RayArrival`, `PlaneArrival`, azimuthal symmetry expansion
- `Image` as a scientific intensity product

### Numerical

- `State`, `Metric` / `SchwarzschildMetric`, `GeodesicDynamics`
- `Integrator` / `RK4Integrator`
- `Propagation::propagate`, `IntegrationSettings`, `TerminationPolicy`
- null-cone step correction
- linear crossing localization arithmetic

### Data transformation

- `CoordinateChart` cart↔sphere
- `ChartMapping` chart→world
- `expand_azimuthally` (symmetry map on plane coordinates)
- `ImageFormation::pixel_for` / `accumulate` / `form_image`
- `Image::normalized_to_max`

### Presentation / output

- `write_csv`, `write_pgm`, `write_summary` in the executable
- PGM gray-level scaling for viewing

### Dependency direction (as implemented)

```text
presentation (executable I/O)
        ↑
data transformation (imaging, azimuthal expansion, chart maps)
        ↑
scientific domain (problem, rays, arrivals)
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
| Spherical symmetry | Enables replacing 2D geodesic sampling with azimuthal expansion of an equatorial family. |
| Aligned canonical geometry | Source, lens, and observer are colinear on `+Z`; produces an Einstein **ring**, not arcs from misalignment. |
| Equatorial integration | Only one orbital plane is integrated; off-equator geodesics are not sampled. |
| Azimuthal symmetry reconstruction | Ring fill comes from post-processing, not independent null geodesics at each azimuth. |
| Geometric optics | Light is treated as rays; no wave optics, interference, diffraction, or PSF. |
| Unit intensity weights | Each arrival contributes `1`; not a calibrated flux or magnification map. |
| Finite ray sampling | Radial structure is discrete in impact parameter (`ray_count`, `b_min`, `b_max`). |
| Finite azimuth sampling | Ring smoothness limited by `azimuth_count`. |
| Finite timestep | RK4 with fixed `step_size`; truncation error accumulates along long paths. |
| Finite integration budget | Rays may end as `NoCrossing` if `max_steps` is too small. |
| Image discretization | Pixel binning with half-open bounds; sub-pixel structure is lost. |
| Unbounded plane for crossing | Arrival detection ignores `ImagePlane` half-width/height; clipping is only at image formation. |
| Fixed `E=1` normalization | Overall affine scaling of the null tangent is conventional, not radiometric. |
| No trajectory storage in the canonical path | Only bracket states around termination are kept for localization. |

---

# 18. Glossary

Definitions match the **implemented** types/functions.

| Term | Definition |
|---|---|
| **PropagationProblem** | Validated composition of `Lens`, `Source`, `Observer`, and `ImagePlane` describing the physical setup without numerics or images. |
| **Lens** | Gravitating body: world `position` + `SchwarzschildParameters` (`rs`), with helpers to map world↔chart frames. |
| **Source** | Point source with a world `position` only. |
| **Observer** | World `position` plus orthonormal `forward`/`up` (and derived `right`) defining the viewing frame. |
| **ImagePlane** | Plane attached to the observer with origin, orthonormal `u`/`v`, `normal`, and half-extents; maps world↔`(u,v)` and provides signed distance. |
| **Ray** | One propagation input: initial `State` plus ensemble `id`. |
| **RayEnsemble** | Owned ordered collection of `Ray`s; assigns ids as indices. |
| **RaySampler** | Builds a `RayEnsemble` from a `PropagationProblem` by linearly sweeping impact parameter and calling `build_null_scatter`. |
| **NullScatterInitialConditions** | Launch parameters `(t0,r0,θ0,φ0)` plus impact-parameter fields for null scattering initial states. |
| **State** | Numerical geodesic state: coordinate 4-vector `X` and tangent 4-vector `U`. |
| **Metric** | Abstract provider of Christoffel symbols at a coordinate location. |
| **Christoffel symbols** | Connection coefficients `Γ^μ_{αβ}` used to form geodesic acceleration; supplied by `SchwarzschildMetric`. |
| **DynamicsModel** | Abstract RHS provider; `GeodesicDynamics` implements `dState/dλ` from Christoffels. |
| **Integrator** | Abstract one-step advance of a `State` given `dt` and a derivative function. |
| **RK4** | Classic four-stage Runge–Kutta integrator used for every geodesic step. |
| **RayArrival** | Per-ray observer-plane result: id, world hit/direction, interpolated chart state, and `ArrivalStatus`. |
| **PlaneArrival** | Post-expansion image-plane sample: `ray_id` + `(u,v)` position. |
| **Azimuthal expansion** | `expand_azimuthally`: rotate equatorial `(u,0)` arrivals to `(u cos ψ, u sin ψ)` without further integration. |
| **Image** | Scientific 2D scalar intensity grid with physical `(u,v)` bounds and contiguous storage. |
| **ImageFormation** | Maps `PlaneArrival`s into an `Image` by half-open pixel binning and unit accumulation. |

---

# Appendix: Quick file index for onboarding

Start here, in order:

1. `experiments/canonical_sgl_image.cpp` — end-to-end wiring
2. `physics/problem/PropagationProblem.cpp` — geometry construction
3. `physics/rays/RaySampler.cpp` — ensemble creation
4. `physics/schwarzschild/InitialStates.cpp` — null initial data
5. `physics/arrivals/ArrivalCollector.cpp` — propagate + localize
6. `physics/propagation/Propagator.cpp` + `physics/integrators/RK4Integrator.cpp` — numerics
7. `physics/geodesics/GeodesicDynamics.cpp` + `physics/metrics/SchwarzschildMetric.cpp` — physics RHS
8. `physics/arrivals/AzimuthalExpansion.cpp` — 1D→2D ring
9. `physics/imaging/ImageFormation.cpp` — pixels
10. `tests/canonical_image_pipeline.cpp` — automated end-to-end checks

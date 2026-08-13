# Off-Axis Solver Implementation

This note records the audit and the minimal changes made so the true-2D path
can be shown to find null geodesics that reach an observer with nonzero
perpendicular offset. It does not claim a high-resolution split-ring image.

---

## 1. Previous limitation

The Schwarzschild physics, 2D Newton residual, and observer geometry were
already capable of off-axis observer hits. The limitation was **not** “the
residual is locked to the optical axis.”

What *was* true:

- `ImagePlane::attached_to(observer, …)` places the plane origin at the
  observer. `to_plane_coordinates(arrival)` is therefore already
  `F = (u, v)` relative to the observer, not to `(0, 0, D)`.
- `refine_launch_to_observer` already solves `F(b_u, b_v) = 0` with that
  residual. Off-axis 5×5 runs already produced two refined hits.
- Those two hits are the two images of a **point source** as seen by a
  **point observer**. They are physically two sky directions, so the image
  is two samples (two pixels after binning). That is not a failed ring fill;
  a split Einstein ring of finite arcs would require an extended source or
  a finite aperture, which this pipeline does not model.

What *looked* like an axis-centered bug:

- `(b_u, b_v)` are launch-plane impact parameters measured from the
  **source–lens axis**, with every ray aimed source → lens. They are not
  observer-plane coordinates. That is the correct parallel-beam family for
  this executable. Moving the observer changes the **root target**, not the
  launch grid.
- The 2D image path previously wrote only a binned image and a summary
  count. It did not record `(b_u, b_v)`, residuals, world arrival, or
  observer position, so it was easy to mistake two valid observer hits for
  “two random plane crossings.”
- On-axis, isolated 2D Newton cannot traverse the Einstein-ring 1-manifold.
  That case is filled by `fill_aligned_observer_ring` (azimuthal expansion).
  Off-axis must **not** use that expansion: the zeros are isolated.

The 1D executable still rejects `--observer-distance != 0`. Only the 2D path
is valid off-axis.

---

## 2. Physical formulation

Observer:

```text
O = D * Ẑ + d * X̂
```

with `Observer::looking_at(O, lens origin, Ŷ)`. The image plane is attached
at `O`, with `(u, v)` along `(right, up)` and normal antiparallel to
`forward`.

A search or refined ray with launch parameters `(b_u, b_v)` is the parallel
null geodesic that starts at

```text
x0 = source + b_u X̂ + b_v Ŷ ,   direction = (lens − source)/|…|
```

It contributes to the point-observer image iff its trajectory meets `O`.
On the observer plane that is the two independent conditions

```text
F(b_u, b_v) = ImagePlane::to_plane_coordinates(x_arrival) = 0
```

i.e. residual origin is the observer, not the optical-axis foot at
`(0, 0, D)`. Plane crossing is `signed_distance >= 0`; localization
interpolates onto the plane. Incoming direction is then converted with
`observer_angular_coordinates` (gnomonic `(u_ang, v_ang)` on the observer
sky).

On-axis (`d = 0`), `F = 0` is a circle in launch space. Off-axis, `F = 0`
is a pair of isolated roots (primary and secondary image). Gauss–Newton on
`(b_u, b_v)` is the correct isolated-root solver for the off-axis case.

---

## 3. Code changes

None of these edits change the metric, Christoffel symbols, geodesic
equations, RK4, or termination physics.

| File | What changed | Why | Physics unchanged? |
|---|---|---|---|
| `physics/arrivals/ObserverLaunchRefiner.h` | Comment on `evaluate_launch`: residual origin is the observer | Makes the already-correct `F` explicit | Yes — comment only |
| `physics/arrivals/ObserverLaunchRefiner.cpp` | Comment on `plane_residual` | Same | Yes — comment only |
| `physics/rays/RayGrid2DSampler.h` | Comment: `(b_u, b_v)` are optical-axis impact parameters | Documents that the grid is not shifted with the observer | Yes — comment only |
| `experiments/true_2d_sgl_image.cpp` | Write `refined_observer_hits.csv`; record observer world position and `hits_path` in `run_summary.txt` | Phase 7 recording: prove arrivals are at `O`, not somewhere else on the plane | Yes — output only |
| `tests/image_plane.cpp` | Off-axis `attached_to`: origin is observer; axis foot is not residual 0 | Fast geometry lock of the boundary condition | Yes |
| `tests/ray_grid_2d_sampler.cpp` | Same `(b_u, b_v)` launch state for aligned vs off-axis problems | Locks “observer moves the target, not the parameterization” | Yes |
| `tests/off_axis_observer_hit.cpp` | Reduced 5×5 geodesic check: ≥2 hits, residual ≤ 1e-6, world miss ≤ 1e-5, finite angles, no azimuthal fill | Lasting reduced diagnostic in CTest | Yes — test only |
| `CMakeLists.txt` | Build and `add_test` for `off_axis_observer_hit` | 5×5 only; 21×21 validation remains manual | Yes |

`fill_aligned_observer_ring` is unchanged: expand iff `observer_distance == 0`.

---

## 4. On-axis behavior

Unchanged numerically:

- 2D Newton still finds isolated samples on the ring (5×5 → 2 unique hits at
  `b = ±8.49049`, `ρ = 0.295034`).
- `fill_aligned_observer_ring` still rotates the median radius into
  `azimuth_count` samples (default 720).
- 1D `sgl_canonical_sgl_image` is untouched (still rejects `d ≠ 0`).
- Parallel-ray construction is untouched.

Reduced on-axis 5×5 (`S = D = 30`, `d = 0`):

| Quantity | Value |
|---|---|
| Observer | `(0, 0, 30)` |
| Search rays / arrived | 25 / 24 |
| Seeds / unique refined hits | 14 / 2 |
| Max residual / world miss | `4.6e-10` / `4.6e-10` |
| Image samples after fill | 720 |
| Median `ρ` | `0.295034` |

Default CTest (21 tests, including existing on-axis pipelines) passed.

---

## 5. Off-axis behavior

The observer is placed at `(d, 0, D)` and looks at the lens. Search rays are
still the axis-centered `(b_u, b_v)` grid. Seeds are residual minima on that
grid. Gauss–Newton drives each seed until `‖F‖ ≤ 1e-6`. Surviving hits are
imaged **without** azimuthal expansion.

Both reduced off-axis runs found **two** observer-reaching trajectories.
`arrival` matches `O` to `~‖F‖` (world miss equals plane residual; signed
distance is ~0). These are not rays that merely crossed the observer plane
somewhere else.

Launch parameters move continuously with `d` (Einstein-lens two-image
branch): as `d` increases, the positive-`b_u` root moves out, the
negative-`b_u` root moves in, and the two gnomonic radii split.

---

## 6. Diagnostics

All runs: `samples-per-axis=5`, `b-max=20`, `S=D=30`, `rs=1`,
`step-size=0.01`, `max-steps=300000`, `hit-tolerance=1e-6`,
`max-root-iterations=12`, `resolution=64`, `OMP_NUM_THREADS=8`.
No 50×50 / 71×71 grids.

### On-axis (`d = 0`)

See §4. Arrivals at `(0, 0, 30)`.

### Small offset (`d = 0.5`)

| | Image A | Image B |
|---|---|---|
| `(b_u, b_v)` | `(-8.278, ~0)` | `(+8.712, ~0)` |
| `‖F‖` | `7.5e-9` | `1.0e-7` |
| World arrival | `(0.500, 0, 30)` | `(0.500, 0, 30)` |
| `(u_ang, v_ang)` | `(-0.287, ~0)` | `(+0.303, ~0)` |
| Newton iterations | 3 | 10 |

`refined_observer_hits=2`, `angular_samples=2`,
`on_axis_azimuthal_expansion=0`, `radial_stddev=0.00820`.

### Larger offset (`d = 1.0`)

| | Image A | Image B |
|---|---|---|
| `(b_u, b_v)` | `(-8.073, ~0)` | `(+8.942, ~0)` |
| `‖F‖` | `1.7e-11` | `1.5e-8` |
| World arrival | `(1.000, 0, 30)` | `(1.000, 0, 30)` |
| `(u_ang, v_ang)` | `(-0.279, ~0)` | `(+0.312, ~0)` |
| Newton iterations | 3 | 12 |

`radial_stddev=0.01640` (larger split than `d = 0.5`).
`sgl_off_axis_observer_hit` (same 5×5, `d = 1`) passed: ≥2 hits, residual
and world miss at `O`, finite angles, no azimuthal fill, nonempty image.

### Continuity

| `d` | `b_u` (−) | `b_u` (+) | `ρ` (−) | `ρ` (+) |
|---|---|---|---|---|
| 0 | −8.490 | +8.490 | 0.29503 | 0.29503 |
| 0.5 | −8.278 | +8.712 | 0.28698 | 0.30337 |
| 1.0 | −8.073 | +8.942 | 0.27920 | 0.31199 |

The two images do not disappear when `d` leaves 0.

### Tests

- Default CTest: 21/21 passed after the geometry/sampler assertions, including
  on-axis 1D pipelines (`canonical_image_pipeline`,
  `source_distance_angular_behavior`, `angular_image_pipeline`).
- `sgl_off_axis_observer_hit` (5×5, `d = 1`) passed and is registered as
  CTest `off_axis_observer_hit`.

---

## 7. Remaining limitations

- **Point observer + point source ⇒ two sky samples.** A visibly split
  *ring* (two arcs) is a different experiment (extended source or finite
  detector). This work does not add either.
- **5×5 only.** Qualitative two-image structure is confirmed at 5×5. The
  existing 21×21 `sgl_true_2d_off_axis_validation` binary is still not in
  default CTest and was not re-run here.
- **Polar-chart launches** along world `+Y` remain a known integrator
  issue (`WorldFrame.h`). The two images found here sit on the equatorial
  `b_v ≈ 0` plane, so they are not blocked by that singularity.
- **Launch-plane dedup** (`0.25 × cell_width`) still merges nearby Newton
  successes. Off-axis at 5×5 that leaves the two physical images. It is
  the wrong unique-filter for the on-axis 1-manifold (already handled by
  azimuthal fill).
- **No magnification / flux weighting.** Each refined hit contributes
  count 1.
- **High-resolution Experiment 2** still needs a stated source model
  (point vs extended), a decision on whether two point images are the
  desired observable, and grids larger than 5×5 only after that is fixed.

Before high-resolution off-axis production runs: keep the hits CSV as the
acceptance check (`world_miss` and `residual_norm` below tolerance at the
recorded observer position), and do not interpret pixel beauty as solver
correctness.

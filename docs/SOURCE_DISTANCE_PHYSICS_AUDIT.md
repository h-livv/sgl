# Source-Distance Physics Audit

> **Status (current tree).** This audit describes an earlier **screen-intersection**
> 1D experiment: fixed-`b` families, plane-crossing coordinates, azimuthal copies of
> those plane hits, and a measured radius that decreased with source distance.
>
> The **current** 1D executable implements the audit’s Fix A: the image is an
> **observer-centered gnomonic angular** map of the **observer-hit** geodesic
> (`observer_angular_coordinates` + `expand_angular_azimuthally`), not a bin of
> arbitrary plane crossings. The 2D executable is a parallel launch-plane beam;
> `--source-distance` is the launch-plane location, and only Newton-refined
> observer hits are imaged. See
> [HOW_THE_EINSTEIN_RING_IS_FORMED.md](HOW_THE_EINSTEIN_RING_IS_FORMED.md) and
> [SGL_FORWARD_PIPELINE.md](SGL_FORWARD_PIPELINE.md).
>
> The measured sequences and hypotheses below are retained as the record of that
> earlier observable. They are not a description of `sgl_canonical_sgl_image` or
> `sgl_true_2d_sgl_image` as they stand today.

This document audits the source-distance experiment as a physics and
numerical-validity question **as it was implemented when the audit was written**.
It documents what that experiment computed, why the measured ring-like radius
decreased with increasing source distance, and what should be changed if the
intended observable is the physical Einstein-ring radius.

No code changes are implemented here.

## 1. Executive finding

The decreasing sequence

```text
S = 50   -> measured radius ~9.5
S = 100  -> measured radius ~6.5
S = 200  -> measured radius ~5.0
S = inf  -> measured radius ~3.5
```

is real for the current experiment. It is reproduced both by the existing sweep
artifacts and by a smaller fresh run of the current executable in `/tmp`.

The strongest finding is that this measured quantity is **not yet justified as the
physical Einstein-ring radius** from the standard aligned point-lens formula. It is
the radius of a ray-count annulus produced by:

1. sampling a fixed interval of the conserved scattering parameter `b = L/E`;
2. integrating those rays forward;
3. recording where they cross an extended observer plane;
4. rotating those plane-crossing coordinates by spherical symmetry;
5. binning the resulting plane points into an image.

That is a valid computational observable, but it is a **screen-intersection
observable for a chosen ray family**, not automatically the observer's angular
Einstein radius.

The current finite-source construction does start all point-model rays at the
finite source location, but the parameter held fixed across source distances is the
Schwarzschild scattering parameter `b = L/E`. Holding that fixed as `S` changes
does not keep the same source-plane angular family or the same image observable
used by the thin-lens Einstein-radius formula.

The recommended correction depends on the scientific intent:

- If the goal is an **observer angular image**, the observable should be built from
  observer-viewing directions, not from arbitrary crossings of an extended plane.
- If the goal remains an **observer-plane screen hit distribution**, the current
  decreasing trend is not by itself a physics contradiction; the plot should not be
  labelled or interpreted as the thin-lens Einstein radius.

The minimum scientifically justified fix is therefore not “change the plot”; it is
to first choose and implement the intended observable.

## 2. Current computational pipeline

The source-distance parameter enters the canonical executable through
`CliOptions::source_distance` in `experiments/canonical_sgl_image.cpp`.

### Point-source path

For `--ray-model point`, the data flow is:

```text
--source-distance S
  -> source.position = -S * Geometry::WorldFrame::optical_axis()
  -> Problem::PropagationProblem(lens, source, observer, image_plane)
  -> Rays::RaySampler::sample(problem)
  -> Geometry::to_chart_frame(problem.lens(), problem.source().position)
  -> CoordinateChart::cart_to_sphere(...)
  -> source_spherical.X = (t0, r0, theta0, phi0)
  -> for each sampled b:
       Schwarzschild::NullScatterInitialConditions{
           t0 = 0,
           r0 = source_spherical.X[1],
           theta0 = source_spherical.X[2],
           phi0 = source_spherical.X[3],
           impact_parameter = b
       }
  -> Schwarzschild::build_null_scatter(...)
  -> State{X=(t,r,theta,phi), U=(vt,vr,0,vphi)}
  -> RayEnsemble
  -> Arrivals::collect_arrivals(...)
  -> Propagation::propagate(...)
  -> Arrivals::localize_arrival(...)
  -> RayArrival.world_position
  -> ImagePlane::to_plane_coordinates(...)
  -> Arrivals::expand_azimuthally(...)
  -> PlaneArrival(u cos psi, u sin psi)
  -> Imaging::ImageFormation::form_image(...)
```

Relevant files and symbols:

| Stage | File / symbol |
|---|---|
| CLI and experiment wiring | `experiments/canonical_sgl_image.cpp`, `main()` |
| Source position | `source.position = -S * optical_axis()` |
| Point ray sampling | `physics/rays/RaySampler.cpp`, `RaySampler::sample` |
| Initial state | `physics/schwarzschild/InitialStates.cpp`, `build_null_scatter` |
| Arrival collection | `physics/arrivals/ArrivalCollector.cpp`, `collect_arrivals` |
| Plane coordinates | `physics/geometry/ImagePlane.cpp`, `to_plane_coordinates` |
| Azimuthal expansion | `physics/arrivals/AzimuthalExpansion.cpp`, `expand_azimuthally` |
| Pixel binning | `physics/imaging/ImageFormation.cpp`, `form_image` |

### Parallel path

For `--ray-model parallel`, source distance is not a point-source distance. It is
used as the finite launch-plane location:

```text
world_position = -source_distance * optical_axis()
               + b * plane_u_axis()
chart_direction = world_to_chart(optical_axis())
```

The parallel path is implemented in `make_parallel_null_state` and
`sample_parallel_rays` inside `experiments/canonical_sgl_image.cpp`.

## 3. Physical model

The current executable solves the following physical/numerical problem:

- Lens: Schwarzschild, `rs = 1`.
- World frame: lens at origin, optical axis along `+Z`.
- Source for point model: `(0, 0, -S)`.
- Observer plane for on-axis runs: origin `(0, 0, D)`, with `D = 30` unless
  changed by `--observer-axial-distance`.
- Rays: one-dimensional equatorial family, sampled by a scalar `b`.
- Gravity: Schwarzschild geodesic equation integrated with RK4.
- Image: ray-count distribution of observer-plane crossings, followed by
  azimuthal symmetry expansion for on-axis observers.

This is **not** a full detector model, not a backwards observer-camera ray trace,
and not a direct angular image on the observer's sky.

## 4. Parameterization audit

### Numerical parameter `b`

In the point-source sampler, `b` is assigned by
`RaySampler::impact_parameter_at`:

```text
b_i = b_min + i/(ray_count - 1) * (b_max - b_min)
```

Then `build_null_scatter` uses:

```text
E = 1
L = b * E
vt   = E / f
vphi = L / (r0^2 sin(theta0))
vr   = -sqrt(E^2 - f L^2/r0^2)
vtheta = 0
```

where `f = 1 - rs/r0`.

Therefore, in the current implementation:

```text
b = L/E
```

It is a Schwarzschild scattering parameter. For rays launched very far away, this
corresponds to the usual asymptotic impact parameter. At finite source distance it
sets the local emission angle through approximately:

```text
sin(alpha_local) ~= b * sqrt(1 - rs/S) / S
```

It is not a literal transverse coordinate of the point source, because the point
source has no transverse extent. It is also not the observer-plane ring radius.

### Finite-source launch direction

For the canonical aligned point source, the source lies at chart position
approximately `x_chart = -S`, on the chart equator. At that point, the spatial
direction implied by `build_null_scatter` has approximately:

```text
axial component      vx ~= sqrt(1 - f b^2/S^2)
transverse component vy ~= -b/S
launch angle         atan(|vy|/|vx|)
```

Runtime diagnostic evidence recorded in `.cursor/debug-70340f.log` shows:

| S | b | launch angle from code | flat closest approach |
|---|---:|---:|---:|
| 50 | 10.2 | 11.77 deg | 10.41 |
| 50 | 11.6 | 13.41 deg | 11.92 |
| 100 | 10.2 | 5.85 deg | 10.25 |
| 100 | 11.6 | 6.66 deg | 11.68 |
| 200 | 10.2 | 2.92 deg | 10.21 |
| 200 | 11.6 | 3.32 deg | 11.62 |

This confirms that increasing `S` while holding `b` fixed narrows the launch cone.
The finite-source model is not a parallel beam, but the sampled family approaches
one as `S` grows.

### Physical impact parameter vs launch angle

The current `b` remains approximately the flat-space closest-approach distance of
the initial straight-line ray, not the observer-plane radius. For finite `S`, the
screen intercept at `z = D` in flat space would scale approximately like:

```text
R_screen_flat ~= b * (D + S) / S
```

Runtime diagnostic evidence recorded in `.cursor/debug-70340f.log` gives:

| S | b=10.2 flat screen intercept | b=11.6 flat screen intercept |
|---|---:|---:|
| 50 | 16.66 | 19.07 |
| 100 | 13.33 | 15.18 |
| 200 | 11.75 | 13.36 |
| inf | 10.20 | 11.60 |

This is already a decreasing trend with increasing `S` for the same fixed `b`
range, before adding gravity. The numerical results show the same qualitative
direction after Schwarzschild bending.

## 5. Observable audit

The plotted “ring radius” is computed from the generated CSV image, not from a
geodesic invariant and not from an observer angle.

The current chain is:

```text
RayArrival.world_position
  -> ImagePlane::to_plane_coordinates(world_position)
  -> u coordinate
  -> expand_azimuthally: (u,0) -> (u cos psi, u sin psi)
  -> ImageFormation::pixel_for
  -> intensity[x,y] += 1
  -> radial profile from pixel centers
  -> peak radial-bin intensity / weighted mean radius
```

In `ImageFormation::pixel_for`, the image coordinate is a physical coordinate on
the extended image plane:

```text
x = floor((u - u_min) / du)
y = floor((v - v_min) / dv)
```

The plotting script then uses pixel-center radii:

```text
r = sqrt(u_center^2 + v_center^2)
```

Therefore the reported radius is a **ray-count peak radius on the observer plane**.
It is not an angular radius unless an additional focal-length/angle mapping is
defined. No such mapping currently exists in the pipeline.

The existing full sweep artifacts record:

| Run | measured peak radius | nonzero radius range |
|---|---:|---|
| S=50 | 9.57 | 9.39 to 12.72 |
| S=100 | 6.52 | 6.35 to 9.21 |
| S=200 | 4.96 | 4.87 to 7.51 |
| S=inf | 3.55 | 3.43 to 5.89 |

A smaller fresh current-executable run in `/tmp` reproduced the decreasing trend:

| Run | measured peak radius | weighted mean |
|---|---:|---:|
| S=50 | 10.20 | 11.08 |
| S=100 | 7.07 | 7.81 |
| S=200 | 5.20 | 6.21 |
| S=inf | 3.79 | 4.68 |

## 6. Hypotheses

| Hypothesis | Evidence for | Evidence against | Confidence | Discriminating test |
|------------|--------------|------------------|------------|---------------------|
| H1: Existing source-distance sweep artifacts are stale or semantically mixed. | Existing finite runs have `observer_distance=30` and no `ray_model` or `observer_axial_distance`, while the current executable now distinguishes `observer_distance` from `observer_axial_distance`. | A fresh `/tmp` run with the current executable still reproduces the decreasing trend. | Medium for artifact interpretation; low as sole explanation. | Regenerate the complete sweep after deciding final semantics; compare metadata and trend. |
| H2: Finite-source initial conditions are physically questionable for the intended comparison because `b` is treated as an asymptotic scattering parameter at finite radius. | `build_null_scatter` sets `L=bE`; launch angle shrinks as `S` grows. Diagnostic logs show `angle_deg_code` falls from ~12-13 deg at S=50 to ~3 deg at S=200 for the same b range. | As a finite-radius null geodesic initial condition, `b=L/E` is mathematically meaningful; it is not automatically invalid. | High that this invalidates the thin-lens comparison; medium that it is a code bug. | Construct a finite-source ray family parameterized by source emission angle or by target observer angle and compare. |
| H3: The measured radius is a screen-crossing coordinate, not the physical observer angular Einstein radius. | Observable chain bins `PlaneArrival(u,v)` into `Image`; no direction-to-angle image is computed. Diagnostic flat-screen intercept decreases with S for fixed b before gravity. | If the intended experiment is literally an extended screen at `z=D`, this is a valid observable. | Very high. | Replace measurement with angular radius from observer-centered viewing directions, or explicitly define a screen experiment and compare against screen-intercept theory. |
| H4: Finite-S and S=inf cases are not the same limiting family. | `point` uses source position plus `build_null_scatter`; `parallel` uses launch plane `(b,0,-S)` and fixed +Z direction via `build_custom`. | As `S` grows, point-model launch angles approach parallel rays; the trend approaches the parallel result monotonically. | Medium. | Run point model at much larger S with adequate `max_steps` and compare to the parallel case at fixed b range. |
| H5: Fixed `b_min`, `b_max`, and `ray_count` do not track the physically relevant ray family as S changes. | Thin-lens expected image-plane impact radii for D=30, rs=1 are ~6.1, 6.8, 7.2, 7.75, while the sampled b range is 10.2-11.6. | Current images are not empty and annuli are clean, so the sweep does sample some coherent ray family. | High. | Sweep a broader b interval and locate the source-observer connecting rays / angular caustic instead of fixing b near 10-12. |
| H6: Radial-bin/weighted-mean methodology creates the trend. | Peak radius and weighted mean differ by 1-2 units; ring width shifts the mean. | Both peak and weighted mean decrease monotonically; fresh and existing runs agree qualitatively. | Low as root cause; medium as quantitative bias. | Repeat with higher resolution and different radial bin counts. |
| H7: The decreasing trend is legitimate for the current screen-intercept observable. | Flat no-gravity screen intercept for fixed b decreases as `(D+S)/S`; current geodesic results show the same direction. | This does not make it the physical Einstein radius. | High. | Compare against a derived screen-intercept model for fixed `L/E` rays rather than the thin-lens angular formula. |
| H8: The weak-field thin-lens formula is quantitatively inappropriate. | The setup uses rs=1, D=30, b around 10-12; deflection is not infinitesimal and integration is exact Schwarzschild rather than thin lens. | The formula's qualitative monotonic increase with S should still apply to the physical angular Einstein radius in the weak-field aligned limit. | Medium for quantitative mismatch; low for explaining opposite trend alone. | Increase all distances and b scales into a cleaner weak-field regime and compare angular observable. |

## 7. Mathematical sanity checks

### Thin-lens expectation

The quoted relation

```text
R_E^2 = 2 r_s D S / (D + S)
```

assumes:

- weak deflection;
- thin-lens geometry;
- aligned point source;
- image radius interpreted as observer angular image position multiplied by
  observer-lens distance;
- a ray family selected by the lens equation, not by an arbitrary fixed sampled
  interval of `L/E`.

For `rs=1`, `D=30`, it predicts:

| S | Thin-lens R_E |
|---|---:|
| 50 | 6.12 |
| 100 | 6.79 |
| 200 | 7.22 |
| inf | 7.75 |

This increases with S and approaches a finite limit.

### Current screen-intercept tendency

For the current finite-source point model, ignoring gravity but keeping the code's
launch parameterization, a ray with fixed `b` reaches the observer plane at roughly:

```text
R_screen_flat ~= b * (D + S) / S
```

This decreases as S grows:

```text
S=50  factor = 1.60
S=100 factor = 1.30
S=200 factor = 1.15
S=inf factor = 1.00
```

The observed trend therefore has a plausible explanation that does not require a
Schwarzschild integrator bug: the experiment is measuring a screen-intercept radius
of a fixed `b` family.

### Sampling mismatch

The weak-lens expected radii are near 6-8, while the sampled `b` range is 10.2-11.6.
If `b` is interpreted as a lens-plane impact parameter, the current sweep is not
centered on the thin-lens Einstein radius for most S values. If `b` is interpreted
as the code's conserved scattering parameter, fixed `b` is still not equivalent to
holding fixed the physical angular image coordinate.

## 8. Recommended final fix

There are two plausible fixes, depending on the intended experiment. They should
not both be conflated.

### Fix A (highest priority): define and compute an observer angular image

1. **What should change:** Image formation should consume an observer-centered
   angular coordinate, not the raw world-space crossing point on an extended plane.
2. **Where it should change:** In the arrivals-to-image stage, likely after
   `RayArrival`, using `world_direction` and the observer frame, or by changing the
   pipeline to trace rays from observer image directions.
3. **What physical quantity it corrects:** It converts the observable from
   screen-intersection radius to angular image radius.
4. **Why it should produce expected behavior:** The thin-lens Einstein radius is an
   angular image-position result; once the measured coordinate corresponds to
   observer viewing direction, comparison to `R_E` becomes meaningful.
5. **What should remain unchanged:** Schwarzschild metric, geodesic RHS, RK4,
   coordinate transforms, and `RayArrival` localization can remain initially.
6. **Validation test:** In a weak-field scaled setup, the measured angular radius
   should increase with S and approach the parallel-ray limit; compare against
   `sqrt(2 r_s D S/(D+S))` within an agreed tolerance.

This is the minimum correction if the scientific target is “Einstein-ring radius
seen by the observer”.

### Fix B: keep screen-intercept experiment but rename/reinterpret outputs

1. **What should change:** Documentation and plotting labels should say
   “observer-plane crossing radius” or “screen-intercept radius”, not Einstein radius.
2. **Where it should change:** Plot labels and docs around
   `plot_source_distance_sweep.py` and `HOW_THE_EINSTEIN_RING_IS_FORMED.md`.
3. **What physical quantity it corrects:** It corrects interpretation, not physics.
4. **Why it should produce expected behavior:** The decreasing trend is consistent
   with fixed-`b` screen intercepts as S grows.
5. **What should remain unchanged:** Forward integration and image formation.
6. **Validation test:** Compare against a derived screen-intercept model for fixed
   sampled `b`, not against the thin-lens Einstein-radius formula.

This is the minimum correction if the intended experiment is an extended physical
screen placed at the observer plane.

### Fix C: finite-source ray parameterization for a point-source experiment

1. **What should change:** Add a finite-source sampler parameterized by local
   emission angle or by target angular coordinate, not by an asymptotic scattering
   `b` interval copied from the parallel case.
2. **Where it should change:** In ray sampling (`RaySampler` or a new experiment-only
   sampler), not in RK4 or the metric.
3. **What physical quantity it corrects:** It makes the finite-source and parallel
   families comparable.
4. **Why it should produce expected behavior:** Holding a consistent physical
   angular variable fixed across S avoids mixing different ray families.
5. **What should remain unchanged:** Schwarzschild dynamics and arrival localization.
6. **Validation test:** For large S, finite-source results should converge smoothly
   to the parallel-ray result under the same observable definition.

This fix is secondary to choosing the observable, because a better sampler alone
does not make plane-crossing radius equal to angular Einstein radius.

## 9. Recommended experiment after the fix

After implementing the chosen observable correction, rerun:

```text
source distances: 50, 100, 200, 500, 1000, inf
observer axial distance: 30
observer perpendicular distance: 0
rs: 1
ray counts: at least 801 for final plots
azimuth count: 720 for on-axis symmetry expansion, if expansion remains applicable
b or angular sampling: wide enough to bracket the ring-producing rays for every S
resolution: 1024
extent / angular extent: chosen so no ring is clipped
```

Record at least:

- peak angular ring radius;
- intensity-weighted angular radius;
- nonzero radial support;
- number of arrivals;
- sampled parameter range;
- convergence with ray count and bin count.

Expected qualitative behavior if measuring physical angular Einstein radius:

- radius increases with finite S;
- radius approaches the parallel-ray limit from below;
- finite-source point model converges toward the parallel-ray result as S grows.

Validation criteria:

- weak-field scaled run agrees qualitatively and approximately quantitatively with
  the thin-lens expression;
- strong-field run may differ quantitatively but should be compared against an
  exact/strong-field ray solution, not just the weak-lens formula;
- no result should be interpreted as Einstein radius unless the observable is
  angular or otherwise explicitly mapped to an observer image coordinate.

## 10. What NOT to change

Unless further evidence implicates them, do not change:

- `Spacetime::SchwarzschildMetric`;
- `Dynamics::GeodesicDynamics`;
- `Integration::RK4Integrator`;
- `Propagation::propagate`;
- `PlaneCrossingTermination`;
- `ArrivalCollector::localize_arrival`;
- `ImageFormation::pixel_for` and row-major image storage;
- `AzimuthalExpansion` for the on-axis Schwarzschild symmetry case.

The evidence does not point to a geodesic-equation or RK4 failure. It points to an
experiment-definition issue: sampled ray family and measured observable are not the
same quantities assumed by the standard finite-source Einstein-radius comparison.


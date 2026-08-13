# Technical briefing: SGL codebase

This document is a study guide reconstructed from the **current source tree**, not from filenames or older notes. Historical documents that contradict the current executable are flagged as such.

**How to read the five-way distinction used throughout**

| Tag | Meaning |
|---|---|
| **CODE** | What the program actually does |
| **PHYSICS** | What that operation means in GR / optics |
| **INTENT** | What the surrounding comments/docs say you meant |
| **APPROX** | A simplification the code or the comparison formula makes |
| **NOT DETERMINABLE** | Not settled by this repository |

---

# 1. Executive technical overview

## What this project is

This repository is a **geometric-optics Schwarzschild forward model** for an aligned (and, in a second executable, off-axis) gravitational lens. A C++ kernel integrates null geodesics in Schwarzschild spacetime. SGL-specific layers place a point source, a point observer, and an image plane around that kernel, then extract one observer-hitting ray and report its incoming angle as an Einstein-ring radius.

CMake (`CMakeLists.txt` L16) labels the kernel “extracted from Penrose.” Penrose is a **sibling project**, not a linked library. The current build target is `sgl_physics`. There is no `penrose_*` CMake target and no Kerr/GPU code in this tree (`notes/EXTRACTION.md`).

The implementation is **intentionally not mission-grade**. `README.md` states that Kerr, plasma, wave optics, detector physics, and mission-level SGL modelling are not implemented.

## The computational / physical pipeline

```text
physical setup          make_aligned_problem / canonical main
                        lens at origin, source at (0,0,−S), observer at (0,0,+D)
        ↓
initial conditions      RaySampler::sample → build_null_scatter   (1D point)
                        or make_parallel_null_state / RayGrid2DSampler  (parallel / 2D)
        ↓
GR evolution            GeodesicDynamics + RK4Integrator + Propagator
                        (Penrose-derived kernel: Metric → Γ → 8 ODEs → RK4)
        ↓
photon trajectories     not stored in the imaging path
                        only last two states kept for plane-crossing interpolation
        ↓
observer / image plane  PlaneCrossingTermination + localize_arrival
                        then observer-hit root (1D bisection or 2D Newton)
        ↓
observable              observer_angular_coordinates → ρ, θ_E = atan(ρ)
                        image is a later visualization, not the scalar
```

## Penrose kernel vs SGL-specific layers

| Layer | CMake target | Belongs to |
|---|---|---|
| `Metric`, `SchwarzschildMetric`, `GeodesicDynamics`, `RK4Integrator`, `Propagator`, `State`, `NullConstraint`, `InitialStates`, `CoordinateChart`, `PropagationContext` | `sgl_physics` | **Generalized GR engine** (Penrose extraction) |
| `Lens`, `Source`, `Observer`, `ImagePlane`, `WorldFrame`, `PropagationProblem` | `sgl_geometry` | **SGL geometry** (deliberately does not link `sgl_physics`) |
| `RaySampler`, `RayGrid2DSampler`, `RayEnsemble`, `EnsemblePropagator` | `sgl_rays` | **SGL sampling + ensemble dispatch** |
| `ArrivalCollector`, `ObserverAngularCoordinates`, `ObserverLaunchRefiner`, `AzimuthalExpansion`, `PlaneCrossingTermination` | `sgl_arrivals` | **SGL observation** |
| `Image`, `ImageFormation` | `sgl_imaging` | **SGL visualization** |
| `experiments/canonical_sgl_image.cpp`, `experiments/true_2d_sgl_image.cpp` | executables | **SGL experiments** |
| `scripts/source_distance_test.py` etc. | Python | **SGL validation / plots** |

CMake states this split explicitly: `sgl_geometry` “must never link `sgl_physics`” (`CMakeLists.txt` L36–37).

## One-paragraph answer: “What does this code actually calculate?”

**CODE:** For a chosen source distance \(S\) and observer axial distance \(D\), with \(r_s = 1\) in geometrized units, the 1D executable scans a discrete grid of Schwarzschild impact parameters \(b = L/E\), integrates each null geodesic with fixed-step RK4 until it crosses the observer plane, finds the \(b\) whose plane intercept passes through the observer point (`residual_u(b) = 0`), converts that ray’s incoming direction into observer-centered gnomonic coordinates \(\rho = \|(u_{\mathrm{ang}}, v_{\mathrm{ang}})\|\), and reports \(\theta_E = \arctan\rho\) as the Einstein-ring angular radius. The ring *picture* is then made by rotating that one signed angular coordinate around the optical axis and binning unit counts. The code does **not** solve for a focal point, does **not** use solar SI units, does **not** include the solar corona or wave optics, and does **not** compute magnification.

---

# Photon trace and plot-point trace (read these first)

These two traces are the shortest path to being able to defend the project. Everything else is expansion.

## Trace A — one photon, source to observer

Take Experiment 1 with \(S = 100\), \(D = 30\), \(r_s = 1\), `--ray-model point`, and a scan ray with some \(b \in [2, 20]\). Later a *different* \(b\) (the refined root) is the one that defines \(\theta_E\). This trace is the scan photon.

### A1. Physical setup (`main`, `canonical_sgl_image.cpp` L649–665)

```text
Lens     at world (0, 0, 0),  rs = 1
Source   at world (0, 0, −100)
Observer at world (0, 0, +30), looking at the origin
Image plane origin = observer; u = +X, v = +Y, normal = +Z
```

`WorldFrame.h`: optical axis \(= +\hat Z\). Chart frame used for Schwarzschild coordinates is a permutation \((x_c, y_c, z_c) = (Z, X, Y)\) so the aligned axis lies on the equator \(\theta = \pi/2\), avoiding the polar singularity of \(\Gamma^\phi_{\theta\phi}\).

**PHYSICS:** Schwarzschild vacuum exterior of a non-rotating spherical mass. **APPROX:** Sun is replaced by a point mass with \(r_s = 1\), not \(r_{s,\odot} \approx 2.95\,\mathrm{km}\). **CODE:** `PhysicalConstants.h` is never included by any `.cpp`. Solar SI conversion is not in the execution path.

### A2. Chart placement (`RaySampler::sample`, L46–64)

World source \((0,0,-100)\) → chart \((-100, 0, 0)\) via `Geometry::to_chart_frame`.

`CoordinateChart::cart_to_sphere`:

\[
r = 100,\quad \phi = \pi,\quad \theta = \pi/2,\quad t = 0.
\]

Every 1D point-model ray shares this **same start event**. Only \(b\) changes.

### A3. Four-velocity from impact parameter (`build_null_scatter`, `InitialStates.cpp` L40–64)

With \(f = 1 - r_s/r_0 = 1 - 1/100 = 0.99\), \(E = 1\), \(L = b E = b\):

\[
\dot t = \frac{E}{f},\qquad
\dot\phi = \frac{L}{r_0^2 \sin\theta_0} = \frac{b}{100^2},\qquad
\dot\theta = 0,\qquad
\dot r = -\sqrt{E^2 - f\,\frac{L^2}{r_0^2}}.
\]

**CODE:** \(b\) is the conserved Schwarzschild quantity \(L/E\), not a Cartesian miss distance (that is the 2D/`parallel` path). **PHYSICS:** this is the standard equatorial null geodesic first integral. **APPROX:** motion is locked to \(\dot\theta = 0\) (equatorial). Rays are **not** initially parallel; they fan from one point.

If \(E^2 - f L^2/r_0^2 < 0\), `build_null_scatter` throws. Photons with too-large \(b\) at this \(r_0\) are unphysical in this constructor.

### A4. Null condition at init

The \((\dot t, \dot r, \dot\phi)\) choice is constructed so \(g_{\mu\nu} U^\mu U^\nu = 0\) for Schwarzschild with \(E,L\) and \(\dot\theta=0\). The metric tensor itself is **never stored**; the same \(f\) appears ad hoc in IC builders, `NullConstraint`, and `SchwarzschildObservables`.

### A5. Integration (`Propagator::run` → `RK4Integrator::step` → `GeodesicDynamics::compute_derivative`)

State is eight doubles:

\[
X^\mu = (t,r,\theta,\phi),\qquad U^\mu = \frac{dX^\mu}{d\lambda}.
\]

RHS (`GeodesicDynamics.cpp`):

\[
\frac{dX^\mu}{d\lambda} = U^\mu,\qquad
\frac{dU^\mu}{d\lambda} = -\sum_{\alpha,\beta=0}^{3} \Gamma^\mu_{\alpha\beta}(X)\, U^\alpha U^\beta.
\]

Christoffels are **analytically hardcoded** in `SchwarzschildMetric::christoffel` (not FD, not symbolic). RK4 is **classical fourth-order, fixed step**. Default/Experiment 1: \(\Delta\lambda = 0.01\), `max_steps = 300000`.

The integration parameter \(\lambda\) is **never named “affine” in the kernel**. It is whatever `IntegrationSettings::step_size` advances. With the \(E=1\) gauge, \(\lambda\) is an affine parameter for the null geodesic, not proper time (proper time is identically zero on a null worldline).

Termination for imaging (`PlaneCrossingTermination`): stop when signed distance to the observer plane \(\ge 0\), or when \(r \le 1.0001\, r_s\) (horizon safety). Escape radius is \(\infty\), so “went to infinity” is **not** a stop condition on this path.

Every 1000 steps, `project_onto_null_cone` resets \(\dot t\) from the spatial components so \(g(U,U)=0\) (`canonical_sgl_image.cpp` L670–671). **APPROX:** this is a constraint projection, not a structure-preserving integrator. Between projections, RK4 drift of the null Hamiltonian is allowed.

### A6. Arrival localization (`localize_arrival`, `ArrivalCollector.cpp` L10–38)

The geodesic is not stored. Only `previous_state` and `final_state` (straddling the plane) are kept. Linear interpolation in **world Cartesian position** and in **spherical chart state** estimates the crossing:

\[
t = \mathrm{clamp}\!\left(\frac{d_{\mathrm{prev}}}{d_{\mathrm{prev}}-d_{\mathrm{curr}}}, 0, 1\right).
\]

**APPROX:** the geodesic is not linear in those coordinates. Localization error is \(O((\Delta\lambda)^2)\) in the crossing coordinate for a smooth trajectory, independent of RK4’s \(O(h^4)\) local truncation.

`world_direction` is the normalized spatial Cartesian velocity at the interpolated state (`ChartMapping.cpp` L20–30). That direction — not the plane hit *position* — is the imaging observable.

### A7. What this photon contributes

If this scan photon’s plane intercept \(u = \texttt{to\_plane\_coordinates}(p).x()\) does **not** change sign relative to its neighbour, it is discarded for the scalar \(\theta_E\). It may still count in `arrived_count`. The Einstein-ring radius is **not** an average over photons. It is a **root** of `residual_u(b) = 0`.

After the root is found, **this scan photon is not the imaged ray** unless it already hit to tolerance. The imaged direction is the refined geodesic at the root \(b_\star\).

---

## Trace B — one point on the source-distance plot

Take the point \(S = 100\) on `theta_E_vs_source_distance.png` produced by `scripts/source_distance_test.py`.

### B1. Independent variable

Python list `SOURCE_DISTANCES = [20, 50, 100, 200, 1000, "inf"]`. For `100`:

```text
prepare_params(100)
  → --source-distance 100
  → --ray-model point
  → dirname "100"
```

All other knobs are `BASE_PARAMS`: `ray-count=41`, `b-min=2`, `b-max=20`, `step-size=0.01`, `max-steps=300000`, `observer-axial-distance=30`, `observer-distance=0`, `observer-hit-tolerance=1e-6`, `max-root-iterations=60`. Image `resolution=64` and `azimuth-count=64` are **visualization only**.

### B2. Executable invocation

`scripts/_common.run_canonical` runs `build/sgl_canonical_sgl_image` and reads `run_summary.txt`. It never looks at `einstein_ring.csv` for the plotted scalar.

### B3. Scan (`main` L681–711)

`RaySampler` builds 41 equatorial null geodesics, \(b_i = 2 + i/40 \cdot 18\). Each is propagated (`collect_arrivals` → OpenMP `propagate_ensemble` if OpenMP was found). For each arrival,

```text
residual_u(b) = ImagePlane::to_plane_coordinates(world_position).x()
```

On axis, equatorial motion, \(v \approx 0\). A true observer hit is \(u = 0\) (the photon passes through the observer point, not merely through the observer *plane*).

### B4. Bracket + bisection (`scan_observer_hit_brackets`, `refine_observer_hit_bisection`)

Adjacent \(b\) values whose `residual_u` changes sign (or already lies within \(10^{-6}\)) form a bracket. Bisection re-propagates `propagate_one_for_b` up to 60 times until \(|u| \le 10^{-6}\).

There can be **more than one** root (direct and relativistic images). `select_primary_observer_hit` keeps the candidate with **smallest positive** \(\rho = \| (u_{\mathrm{ang}}, v_{\mathrm{ang}}) \|\); ties break to smaller \(b\). That is the primary / direct image.

### B5. Angular observable (`observer_angular_coordinates`)

Incoming unit sky direction \(s = -\hat v_{\mathrm{world}}\):

\[
u_{\mathrm{ang}} = \frac{s\cdot \hat r}{s\cdot \hat f},\qquad
v_{\mathrm{ang}} = \frac{s\cdot \hat u}{s\cdot \hat f},\qquad
\rho = \sqrt{u_{\mathrm{ang}}^2 + v_{\mathrm{ang}}^2}.
\]

For a ray in the right–forward plane this is \(\rho = \tan\theta\), and

\[
\theta_E = \arctan\rho
\]

is the angle between the incoming photon and the observer’s optical axis (`write_summary` L588–595).

Also written: `R_equiv = D \cdot \rho = D \tan\theta_E` (geometrized length).

### B6. What is plotted

`extract_theta_E` reads `theta_E=` from `run_summary.txt`. That number is the y-value for \(S=100\).

**The image is not involved.** `expand_angular_azimuthally` then `form_image` build `einstein_ring.pgm` *after* the scalar is known, by copying \(\mathrm{sign}(u_{\mathrm{ang}})\,\rho\) around a circle of `azimuth_count` samples and adding \(+1\) per pixel, then dividing by the max.

### B7. What this number is *not*

- Not a pixel-peak radius.
- Not a spatial intercept on an extended screen of a fixed-\(b\) family. That was an earlier observable (a radius that decreased with \(S\)). The current tree uses observer-hit gnomonic angles. If you quote the old decreasing sequence \(9.5 \to 6.5 \to 5.0 \to 3.5\), you will be describing a **retired** quantity. See `docs/SOURCE_DISTANCE.md`.
- Not the solar Einstein ring in arcseconds.
- Not a focus of the lens. No caustic is computed.

---

# 2. Codebase architecture

## Library map

```text
sgl_physics          generalized GR engine (Penrose extract)
        ↑
sgl_geometry         SGL scene (Eigen only; no GR)
        ↑
sgl_rays             sampling + OpenMP ensemble propagate
        ↑
sgl_arrivals         plane crossing, angular map, Newton/bisection support
        ↑
sgl_imaging          count image + max-normalize
        ↑
sgl_canonical_sgl_image / sgl_true_2d_sgl_image
```

## Component catalogue (SGL calculation path)

### Metric representation

- **File:** `physics/core/Metric.h`, `physics/metrics/SchwarzschildMetric.{h,cpp}`
- **Purpose:** supply \(\Gamma^\mu_{\alpha\beta}(X)\)
- **Inputs:** indices \((\mu,\alpha,\beta)\), position \(X\)
- **Outputs:** one `double`
- **Physical quantity:** Christoffel symbols of the second kind
- **Math:** hardcoded Schwarzschild formulae; symmetrizes \(\alpha\leftrightarrow\beta\)
- **Called by:** `GeodesicDynamics::compute_derivative`
- **Does not exist:** \(g_{\mu\nu}\), \(g^{\mu\nu}\), \(\partial_\sigma g_{\mu\nu}\) APIs

### Coordinate system

- **Integration chart:** Schwarzschild spherical \((t,r,\theta,\phi)\), areal \(r\)
- **World frame:** Cartesian \((X,Y,Z)\), optical \(+\hat Z\) (`WorldFrame.h`)
- **Chart permutation:** \(x_c=Z,\; y_c=X,\; z_c=Y\)
- **Conversion:** `CoordinateChart::{cart_to_sphere,sph_to_cart}` plus Jacobians
- **Not used:** isotropic, harmonic, Eddington–Finkelstein, Boyer–Lindquist

### Four-position / state

- **File:** `physics/core/GeodesicState.h`
- **Representation:** `State { Eigen::Vector4d X; Eigen::Vector4d U; }`
- **Algebra:** componentwise `+` and scalar `*` for RK4 stages
- **Physical quantity:** coordinate event \(X^\mu\) and tangent \(U^\mu = dX^\mu/d\lambda\)

### Four-velocity / four-momentum

- **CODE:** `U` is the coordinate velocity w.r.t. the RK4 parameter. With \(E=1\) in `build_null_scatter`, \(U_t = g_{tt}\dot t = -E\) up to sign convention of the mostly-plus metric used in observables.
- **Observables:** `conserved_energy = f · U[0]`, `conserved_angular_momentum = r² sin²θ · U[3]` (`SchwarzschildObservables.h`)
- **Four-momentum vs four-velocity:** for a null geodesic they are parallel; the code never distinguishes \(p^\mu\) from \(U^\mu\). Frequency / redshift is **not** computed.

### Christoffel-symbol calculation

- **File:** `SchwarzschildMetric.cpp` L8–35
- **Method:** **analytical, handwritten** `if` ladder
- **Nonzero components implemented:**

| \(\Gamma^\mu_{\alpha\beta}\) | Formula in code |
|---|---|
| \(\Gamma^t_{tr}\) | \(r_s / (2 r (r-r_s))\) |
| \(\Gamma^r_{tt}\) | \(r_s (r-r_s)/(2 r^3)\) |
| \(\Gamma^r_{rr}\) | \(-r_s / (2 r (r-r_s))\) |
| \(\Gamma^r_{\theta\theta}\) | \(-(r-r_s)\) |
| \(\Gamma^r_{\phi\phi}\) | \(-(r-r_s)\sin^2\theta\) |
| \(\Gamma^\theta_{r\theta}\) | \(1/r\) |
| \(\Gamma^\theta_{\phi\phi}\) | \(-\sin\theta\cos\theta\) |
| \(\Gamma^\phi_{r\phi}\) | \(1/r\) |
| \(\Gamma^\phi_{\theta\phi}\) | \(\cos\theta/(\sin\theta + 10^{-8})\) |

- **APPROX:** pole guard \(10^{-8}\) on \(\Gamma^\phi_{\theta\phi}\). Horizon poles at \(r=r_s\) are unregularized (integration is stopped at \(1.0001 r_s\)).

### Geodesic equation

- **File:** `GeodesicDynamics.cpp`
- **Operation:** 4×4×4 loop, skip if \(\Gamma=0\)
- **Callers:** `Propagator` via `DynamicsModel` virtual
- **8 ODEs**, not a reduced 3-D system. Energy and angular momentum are *consequences* of Killing fields, not used to reduce the ODE.

### Numerical integrator

- **Files:** `Integrator.h`, `RK4Integrator.{h,cpp}`
- **Algorithm:** classical RK4, order 4, **fixed** \(\Delta\lambda\)
- **No** adaptive step, **no** embedded error estimate, **no** symplectic structure
- **Precision:** `double` / `Eigen::Vector4d`

### Photon initialization

- **1D point:** `RaySampler` + `build_null_scatter`
- **1D parallel / “∞”:** `make_parallel_null_state` + `build_custom(Null)`
- **2D:** `RayGrid2DSampler::state_for` + `build_custom(Null)`
- See §5.

### Ray / photon sampling

- **1D:** uniform linear grid in \(b\), not Monte Carlo, not importance sampling
- **2D:** cell-centered square grid in \((b_u, b_v)\)
- **Ring fill (1D, and 2D on-axis):** `expand_angular_azimuthally` — **no extra geodesics**

### Observer plane

- **Type:** `Geometry::ImagePlane` attached to the observer
- **Hit for imaging:** plane intercept at the **origin** of that plane (the observer point)
- **Termination:** `PlaneCrossingTermination` uses signed distance along the plane normal

### Image / ring extraction

- **Scalar ring radius:** observer-hit root → gnomonic \(\rho\) → \(\theta_E=\arctan\rho\)
- **Picture:** azimuthal copy (1D / 2D-on-axis) or sparse Newton hits (2D off-axis) → `ImageFormation::accumulate` (+1 per sample) → `normalized_to_max`

### Data accumulation

- Per-run: `run_summary.txt`, `einstein_ring.csv`, `einstein_ring.pgm`
- Experiment 1: `outputs/validation/source_distance_test/results.csv`

### Parallelization

- OpenMP `parallel for schedule(dynamic)` over ensemble index in `EnsemblePropagator.cpp` and over Newton seeds in `ObserverLaunchRefiner.cpp`
- Optional: if CMake does not find OpenMP, the same source builds serially
- Thread count: `OMP_NUM_THREADS` only

### Validation code

- CTest binaries under `tests/`
- Python: `scripts/{source_distance,ray_convergence,step_convergence,weak_field_validation}_test.py`
- 2D heavy binaries built but **not** in default CTest

### Plotting / analysis

- Matplotlib in the Python scripts above
- `experiments/plot_source_distance_sweep.py` for sweep folders
- `experiments/parameter_sweep.py` orchestrates runs; **currently active sweep is `observer_distance`**, not source distance (source-distance block is commented)

## Explicit call graph (1D Experiment 1)

```text
source_distance_test.main
  └─ _common.run_canonical
       └─ sgl_canonical_sgl_image.main
            ├─ PropagationProblem / Observer / ImagePlane
            ├─ PropagationContext (SchwarzschildMetric + GeodesicDynamics + null projection)
            ├─ RaySampler.sample → build_null_scatter          [scan rays]
            ├─ collect_arrivals
            │    ├─ PlaneCrossingTermination
            │    ├─ propagate_ensemble  [OpenMP]
            │    │    └─ propagate → RK4.step → GeodesicDynamics → Metric.christoffel
            │    └─ localize_arrival
            ├─ scan_observer_hit_brackets
            ├─ select_primary_observer_hit
            │    └─ refine_observer_hit_bisection
            │         └─ propagate_one_for_b → collect_arrivals (1 ray)
            ├─ observer_angular_coordinates
            ├─ write_summary  (θ_E)          ★ plotted scalar
            ├─ expand_angular_azimuthally    [visualization]
            └─ ImageFormation.form_image
```

---

# 3. Penrose as a generalized GR engine

## Why it is a GR engine, not an SGL simulator

The kernel never hears about sources, observers, Einstein rings, or the Sun. Its only physics contract is:

```text
Metric.christoffel(μ,α,β,X)  →  GeodesicDynamics.compute_derivative(State)
                             →  Integrator.step(State, Δλ)
                             →  Propagator loop + TerminationPolicy
```

That is the geodesic equation on an arbitrary pseudo-Riemannian 4-manifold, restricted only by the fact that the current `Metric` implementation is Schwarzschild. SGL is a **client** of this engine.

CMake encodes the claim: `sgl_physics` is “Schwarzschild GR / geodesic numerical engine (extracted from Penrose).” Geometry is a separate library that must not link it.

## Layer separation (actual, not aspirational)

| Layer | Abstraction | Schwarzschild knowledge? |
|---|---|---|
| `Metric` | \(\Gamma^\mu_{\alpha\beta}(X)\) | no (interface) |
| `SchwarzschildMetric` | hardcoded \(\Gamma\) | **yes** |
| `GeodesicDynamics` | \(\ddot X^\mu = -\Gamma^\mu_{\alpha\beta} \dot X^\alpha \dot X^\beta\) | no |
| `RK4Integrator` | \(y_{n+1} = y_n + \frac{h}{6}(k_1+2k_2+2k_3+k_4)\) | no |
| `Propagator` | step until terminate | no |
| `RadiusBoundTermination` | stop on `X[1]` | weakly: assumes \(r = X[1]\) |
| `NullConstraint` / `build_null_scatter` / `SchwarzschildObservables` | \(f=1-r_s/r\), \(E\), \(L\) | **yes** |
| `CoordinateChart` | spherical ↔ Cartesian | chart-specific, not Kerr BL |
| SGL geometry / imaging | scene + observable | **yes, SGL** |

## How the metric is represented

**CODE:** `SchwarzschildMetric` stores one `double rs_`. There is no 4×4 matrix of \(g_{\mu\nu}\). Curvature enters dynamics **only** through `christoffel`.

The line element is **implied**, and written out only in constraint/IC/observable code:

\[
ds^2 = -f\,dt^2 + f^{-1} dr^2 + r^2 d\theta^2 + r^2\sin^2\theta\, d\phi^2,
\quad f = 1 - \frac{r_s}{r}.
\]

## How metric derivatives are represented

**They are not.** Christoffels are not computed from \(\partial g\). Someone derived \(\Gamma\) on paper and typed the formulae.

## How Christoffels are generated

**Analytical, handwritten, runtime lookup.** Not symbolic codegen, not automatic differentiation, not finite difference.

## Geodesic state and what is integrated

Eight first-order ODEs for \((X^\mu, U^\mu)\). Affine parameter = RK4 `dt`. Sign of `dt` may be negative (API allows reverse integration; the SGL path uses `step_size > 0`).

## Assumptions the engine makes about spacetime

1. A `Metric` can return \(\Gamma^\mu_{\alpha\beta}\) at a coordinate point (Levi-Civita connection in some chart).
2. The state lives in a 4-dimensional chart with `double` components.
3. `RadiusBoundTermination` interprets `X[1]` as a radial coordinate with a horizon-like inner bound — **this is already Schwarzschild-shaped**, not metric-agnostic.
4. No signature check, no topology, no horizon regularization, no coordinate-patch switching.

## “If I replaced Schwarzschild with Kerr tomorrow, what would I have to change?”

**Can stay unmodified**

- `Metric` interface
- `GeodesicDynamics` (it only calls `christoffel`)
- `RK4Integrator`, `Integrator`
- `Propagator`, `IntegrationSettings`, `PropagationOutcome`
- `State` layout (still 4+4 if you stay in Boyer–Lindquist)
- `RayEnsemble`, `EnsemblePropagator` (they propagate `State`s)
- `Image`, `ImageFormation` (they bin 2-vectors)
- `Observer`, `ImagePlane` (Euclidean scene)

**Must replace or rewrite**

| Component | Why |
|---|---|
| `SchwarzschildMetric` | Kerr \(\Gamma^\mu_{\alpha\beta}(r,\theta; M,a)\) are different and more numerous |
| `SchwarzschildParameters` | need spin \(a\) |
| `build_null_scatter`, `build_custom` null \(vt\) | first integrals change; Kerr has frame-dragging; equatorial null radial potential is not \(E^2 - f L^2/r^2\) |
| `NullConstraint::project_onto_null_cone` | uses Schwarzschild \(g_{\mu\nu}\) |
| `SchwarzschildObservables` | \(E = f \dot t\) and \(L = r^2\sin^2\theta\,\dot\phi\) are Schwarzschild; Kerr needs \(E\), \(L_z\), Carter constant |
| `CoordinateChart` | spherical polar ≠ Boyer–Lindquist ↔ Cartesian |
| `RadiusBoundTermination` / `horizon_safety_factor` | Kerr horizon is \(r_+ = M + \sqrt{M^2-a^2}\), not \(r_s\), and is not spherical in all charts |
| 1D equatorial `vtheta=0` ICs | still valid for equatorial Kerr, **invalid** for off-equator Kerr (Carter constant) |
| Any comment that \(b = L/E\) is “the” impact parameter | Kerr impact parameters are a 2-D \((\alpha,\beta)\) screen |

**Would need a design decision (NOT DETERMINABLE as a single required change)**

- Whether to keep the `Metric`-only-Γ API or add \(g_{\mu\nu}\) so constraints are generic
- Whether SGL geometry (Sun as Kerr) is even the scientifically intended next step versus solar quadrupole / plasma

Kerr **used to exist in Penrose** and was **explicitly excluded** at extraction (`notes/EXTRACTION.md` L21). This repo does not contain Kerr formulae.

## Extensibility to other GR phenomena

Anything that is “integrate geodesics in a given metric” can reuse `GeodesicDynamics` + RK4 + `Propagator`: photon rings of a black hole, radar delay, light deflection, (with timelike ICs) bound orbits. `build_bound_orbit` and `build_radial_freefall` already exist in `InitialStates.cpp` and are **not** on the SGL imaging path; they show the engine was built for more than null scattering.

What you cannot do without new code: matter, EM, wave optics, radiative transfer, plasma refraction, time-dependent metrics, adaptive charts.

---

# 4. The physics: Schwarzschild spacetime and photon propagation

## Schwarzschild metric

**PHYSICS:** unique vacuum, static, spherically symmetric solution (Birkhoff). Assumptions: no charge, no spin, no cosmological constant, vacuum outside a spherical mass.

**CODE:** only the exterior chart \(r > r_s\) is used. Interior stellar metric is absent. Solar rotation is absent.

## Coordinate choice

**CODE:** standard Schwarzschild spherical coordinates. \(r\) is areal radius (\(4\pi r^2 =\) area of the 2-sphere). \(t\) is Killing time. This is **not** the isotropic radius used in some SGL mission papers.

**Coordinate effects vs observables**

| Symbol | Role |
|---|---|
| \(t,r,\theta,\phi\) | coordinate / numerical state |
| \(\lambda\) | numerical parameter of RK4 |
| \(U^\mu\) | numerical tangent |
| \(E, L\) | conserved; coordinate-invariant as scalars associated with Killing fields |
| \(b = L/E\) | invariant impact parameter (1D path) |
| \(\rho, \theta_E\) | observer-frame angular observable (the scientific output) |
| plane \((u,v)\) | used as a **root-finding residual**, not as the plotted radius |

## Null worldlines, proper time, affine parameter

A photon follows a null geodesic: \(g_{\mu\nu} \dot x^\mu \dot x^\nu = 0\) and \(\nabla_{\dot x} \dot x = 0\).

Proper time \(\tau\) satisfies \(d\tau^2 = -ds^2 = 0\) on the ray, so \(\tau\) cannot parametrize the curve. An affine parameter \(\lambda\) is any parameter in which the geodesic equation has no first-derivative term other than the Christoffel piece, i.e. exactly the ODE `GeodesicDynamics` integrates.

**CODE never renormalizes \(\lambda\).** The \(E=1\) choice in `build_null_scatter` fixes the scaling of \(\lambda\) relative to Killing energy.

## Geodesic equation and meaning of \(\Gamma\)

\(\Gamma^\mu_{\alpha\beta}\) are the connection coefficients: they quantify how basis vectors change from point to point. In this problem they are **gravitational deflection** in coordinate language. There is no Newtonian force in the code.

## Gravitational deflection and impact parameter

**PHYSICS (weak field, from-infinity):** \(\delta = 4GM/(c^2 b) = 2 r_s / b\).

**CODE does not use that formula to propagate.** Deflection is the accumulated result of integrating \(\Gamma\). The weak-field \(\delta\) appears only in the *comparison* formula of `weak_field_theta_E` (via the Einstein-radius construction), not in the integrator.

On the 1D point path, \(b = L/E\) is exact in Schwarzschild (conserved). It equals the asymptotic Cartesian miss distance only for a ray from infinity. At finite \(S\), \(b = L/E\) is **not** the Euclidean source-plane offset. That is a primary reason finite-\(S\) numerics need not match a formula that treats \(b\) as a Euclidean impact parameter at infinity.

## One photon: initial condition → near Sun → outgoing → observer

1. Start at \(r=S\), \(\theta=\pi/2\), \(\phi=\pi\), inbound \(\dot r < 0\).
2. \(r\) decreases; \(\Gamma^r_{tt}\), \(\Gamma^t_{tr}\), \(\Gamma^r_{\phi\phi}\), \(\Gamma^\phi_{r\phi}\) bend the trajectory. Closest approach is **not** computed as an explicit periapsis observable on the imaging path.
3. After periapsis \(\dot r > 0\) (if the ray is not captured). Capture: if \(b\) is below the critical value \(b_{\mathrm{crit}} = (3\sqrt{3}/2) r_s \approx 2.598 r_s\), the ray can fall in; `RadiusBoundTermination` stops at \(r = 1.0001 r_s\). Experiment 1’s \(b_{\min}=2\) **includes sub-critical rays**. Those typically terminate at the horizon and yield `NoCrossing` or a non-bracketing residual.
4. Outbound ray crosses \(r=D\) neighborhood and then the observer plane at \(Z=D\).
5. Only if the intercept is the observer point is the ray an Einstein-ring generator.

**NOT DETERMINABLE FROM THE CODEBASE:** periapsis radius of the selected root \(b_\star\) is not written to `run_summary.txt`. You cannot read off “how close to the Sun” from the plot files without re-integrating.

---

# 5. Photon initialization

## Where they start (1D point — Experiment 1)

All rays: source event, aligned \((t,r,\theta,\phi) = (0, S, \pi/2, \pi)\).

## Where the source is placed

`Source.position` is a 3-vector. Aligned: \((0,0,-S)\). No extent, spectrum, limb darkening, or angular size.

## How source distance is represented

`PropagationProblem::source_distance() = ‖source − lens‖` in geometrized length. There is no redshift, no angular-diameter distance object, no `S = ∞` flag in C++.

## How initial positions are generated

- **Point:** one position, copied \(N\) times.
- **Parallel:** \((b, 0, -S)\) along \(+\hat X\).
- **2D:** \((b_u, b_v, -S)\) cell-centered grid.

## How initial directions are generated

- **Point:** from \((E,L)\) as in §Photon trace A3. **Not parallel.**
- **Parallel / 2D:** `normalize(lens − source)` = \(+\hat Z\) when aligned. **Parallel.** `vt` filled by null constraint in `build_custom`.

## Impact parameter

| Path | Meaning of “b” |
|---|---|
| 1D point | \(b = L/E\), conserved Schwarzschild |
| 1D parallel | Euclidean offset along \(+\hat X\) on the launch plane |
| 2D | Euclidean offsets \((b_u, b_v)\) on the launch plane |

**Do not call these the same quantity in a presentation without saying so.**

## Finite source distance vs initialization

**If I change \(S\), what exactly changes in the initial conditions? (1D point)**

1. Start radius \(r_0 = S\) (aligned).
2. \(f = 1 - r_s/S\) changes, so \(\dot t = E/f\) changes.
3. \(\dot\phi = b / (S^2 \sin\theta)\) changes even at **fixed** \(b\).
4. \(\dot r = -\sqrt{E^2 - f b^2 / S^2}\) changes.
5. The Euclidean source–lens–observer triangle changes, so the \(b\) that hits the observer changes — that is the physical finite-distance effect on \(\theta_E\).

What does **not** automatically change: the scanned interval \([b_{\min}, b_{\max}]\), \(E=1\), \(\dot\theta=0\), \(D\), \(r_s\).

**Parallel “∞”:** \(S\) in the CLI is the **launch-plane location**, default 30 in Experiment 1’s `BASE_PARAMS["source-distance"]`, **not** infinite. Rays are parallel at that finite plane.

## Assumptions about source and observer

- Point source, point observer.
- Both outside the horizon (`PropagationProblem` ctor).
- Observer looks at the lens origin.
- Image-plane normal antiparallel to observer forward; origin on the optical axis.
- 1D executable **rejects** `observer-distance ≠ 0` (`canonical_sgl_image.cpp` L643–647). Off-axis is 2D-only.

---

# 6. Einstein-ring formation

## Physical argument

Spherical symmetry + source, lens, and observer on one axis ⇒ the set of observer-hitting null geodesics is invariant under rotation about that axis ⇒ the image is a circle (an Einstein ring), not a point. A point would require a unique undeflected ray (flat space, aligned). Gravity supplies a one-parameter family of deflected rays, labelled by azimuth, all at the same polar angle from the axis.

A blob would appear if that degeneracy were broken (off-axis source/observer, quadrupole, finite source disk). The 1D code **enforces** the ring by `expand_angular_azimuthally`. It does not *discover* azimuthal degeneracy from 3-D sampling.

## What happens to individual rays

Most scan rays miss the observer point. They still cross the observer *plane* at some \(u \neq 0\). Those misses are used only to bracket the hit. One refined \(b_\star\) hits. Its incoming direction is one point on the ring. The other azimuths are **copied**, not integrated.

## What the observer measures

The angle between the arriving photon’s direction and the line of sight to the lens. That is \(\theta_E = \arctan\rho\).

## What defines the ring radius

\(\rho\) from the primary observer-hit direction, or \(\theta_E = \arctan\rho\). Experiment 1 plots \(\theta_E(S)\).

## Focal point / focal line / intersection?

| Concept | Present? |
|---|---|
| Focal point of the lens | **No.** SGL “focal line” of the Sun (a region ~550 AU down-range where the PSF collapses) is **not computed**. |
| “Focal line” in CLI comments | synonym for the **optical axis** (`observer-distance` is a perpendicular offset from it) |
| Intersection solved | geodesic \(\cap\) observer plane, then \(u=0\) at the observer **point** |
| Plotted quantity | **incoming angle** at that hit |

## How the ring picture is reconstructed

```text
signed_u_ang = selection.angular_coordinate.x()
for k in 0 .. azimuth_count-1:
    ψ = 2π k / azimuth_count
    sample = (signed_u_ang * cos ψ, signed_u_ang * sin ψ)
form_image: each sample += 1 to its pixel; then /= max
```

(`ObserverAngularCoordinates.cpp` L34–54, `ImageFormation.cpp` L27–57, `Image.cpp` L74–82.)

Unit weights: **no magnification**, no Jacobian, no \(1/b\) flux factor.

## “Take one point on the radius-vs-source-distance plot. Exactly how does the code generate that number?”

Answer: Trace B in this document. Function list:

`prepare_params` → `run_canonical` → `main` → `RaySampler::sample` / `build_null_scatter` → `collect_arrivals` → `residual_u_for_arrival` → `scan_observer_hit_brackets` → `select_primary_observer_hit` → `refine_observer_hit_bisection` → `propagate_one_for_b` → `observer_angular_coordinates` → `write_summary` (`theta_E=atan(rho)`) → `extract_theta_E` → `plt.plot`.

---

# 7. Experiment 1: finite source distance

## Independent / dependent variables

| Role | Quantity | Units |
|---|---|---|
| Independent | \(S\) | geometrized length, same as \(r_s=1\) |
| Dependent (plotted) | \(\theta_E\) | radians |
| Also stored | \(\rho\), \(R_{\mathrm{equiv}}=D\rho\), selected \(b\), residual | mixed |
| Held fixed | \(D=30\), \(r_s=1\), \(b\in[2,20]\), \(h=0.01\), on-axis | |

`"inf"` is **not** \(S=\infty\). It switches `--ray-model parallel` with launch plane at `source-distance=30` (the unused default in `BASE_PARAMS`).

## Coordinate conventions

World optical \(+\hat Z\); observer at \(+D\hat Z\); source at \(-S\hat Z\). Integration in Schwarzschild spherical after the chart permutation.

## How each simulation is initialized / integrated / extracted / stored / plotted

See Trace B. Storage: `outputs/validation/source_distance_test/<S>/run_summary.txt` plus `results.csv`. Plots: `theta_E_vs_source_distance.png`, `theta_E_relative_vs_source_distance.png`. Qualitative verdict: \(\theta_E\) should increase with \(S\) and the largest finite \(S\) should be closer to the parallel limit than the smallest \(S\) (`verdict.json`).

CTest mirror: `tests/source_distance_angular_behavior.cpp` asserts \(R_{\mathrm{equiv}}\) increases from \(S=50\to100\to200\) and that \(S=200\) is closer to the parallel run than \(S=100\).

## Analytical / weak-field prediction

**Experiment 1 does not plot this.** The formula lives in `scripts/_common.py` `weak_field_theta_E` and is used by Test 4 (`weak_field_validation.py`).

\[
R_E =
\begin{cases}
\sqrt{2 r_s D S/(D+S)} & S \text{ finite} \\
\sqrt{2 r_s D} & S\to\infty
\end{cases}
\qquad
\theta_{\mathrm{an}} = \arctan(R_E / D).
\]

**PHYSICS derivation (standard thin-lens Einstein radius).** With \(G=c=1\), \(4M = 2 r_s\),

\[
\theta_E^2 = 2 r_s \frac{D_{LS}}{D_L D_S}.
\]

Identifying Euclidean \(D_L = D\), \(D_{LS} = S\), \(D_S = D+S\) gives \(\theta_E = \sqrt{2 r_s S / (D(D+S))}\). Then \(R_E := D \tan\theta_E\) is replaced in the script by \(R_E = D \cdot \theta_E^{\mathrm{(small)}}\) essentially via \(R_E = \sqrt{2 r_s D S/(D+S)}\) and \(\theta_{\mathrm{an}} = \arctan(R_E/D)\). That last `atan` is a partial undo of the small-angle assumption; it is **not** a strong-field GR result.

**Assumptions of that formula (APPROX):**

1. Weak field (deflection \(2 r_s/b\)).
2. Thin lens (deflection occurs in one plane).
3. Identification of coordinate distances with angular-diameter distances.
4. Aligned point source / point lens / point observer.
5. Small angles in the construction of \(R_E\), then `atan` at the end.
6. Source at finite Euclidean \(S\), observer at Euclidean \(D\).

## Why \(\theta_E(S)\) increases with \(S\)

**PHYSICS:** the finite-distance Einstein radius interpolates between “source close to the lens” (small lever arm \(D_{LS}/D_S\)) and the source-at-infinity limit \(\theta_E \to \sqrt{2 r_s / D}\) (small-angle). As \(S\) grows, \(S/(D+S)\to 1\), so \(\theta_E\) increases toward the parallel-ray value.

**CODE:** the test `source_distance_angular_behavior.cpp` encodes this as an assertion on \(R_{\mathrm{equiv}}\), i.e. the numerical observer-hit angle is expected to **increase**, opposite to the retired screen-intersection audit.

## Infinite-source limit

**INTENT:** \(S\to\infty\), parallel incoming rays, \(\theta_E \to \theta_E(\infty)\).

**CODE:** a finite parallel beam at launch plane \(z=-30\) (Experiment 1). That is a **stand-in**, not an actual limit \(S\to\infty\). Residual error: rays are parallel at \(r\sim 30 r_s\), not at infinity; they already feel curvature before a true asymptotic region.

## Should numerics converge to the weak-field expression exactly?

**No.** Even with perfect integration you should **not** expect exact agreement, because:

1. Experiment 1 uses \(D=30\), \(r_s=1\) ⇒ \(r_s/D = 1/30\). This is **not** a weak-field, small-angle SGL regime. Weak-field \(\theta_E(\infty) \sim \sqrt{2/30} \approx 0.258\,\mathrm{rad} \approx 15^\circ\).
2. The integrator is full Schwarzschild, the formula is linearized GR + thin lens.
3. `"inf"` is a finite parallel plane.
4. Localization is linear interpolation between RK4 steps.
5. The formula’s \(R_E\) uses the small-angle map \(\theta \approx R/D\) inside the square root.

Test 4 moves to \(D=200\), \(S\in\{200,500,\infty\}\), still \(r_s/D = 0.005\), and accepts **50%** relative error as “toward the prediction.” That is an **exploratory** check, not a precision validation.

## Discrepancy budget (if numerical \(\theta_E\) ≠ \(\theta_{\mathrm{an}}\))

| Source | Status |
|---|---|
| Physical approximation (full Schwarzschild vs linearized deflection) | **Expected, dominant at \(D=30\)** |
| Weak-field / thin-lens / small-angle formula | **Expected** |
| Finite-distance geometry vs angular-diameter distances | **Expected** |
| `"inf"` = parallel plane at 30, not infinity | **Expected** |
| Numerical integration error | Constrained only loosely by Test 3; see §9 |
| Finite sampling of \(b\) | After bisection, secondary if a bracket exists |
| Coordinate effects (Schwarzschild \(r\) vs isotropic) | Present in any comparison to isotropic-radius SGL formulae; **this formula uses the same \(D,S\) the code uses**, so it is internally consistent as coordinate lengths |
| Extraction error (linear plane interpolate, gnomonic map) | Present; not separately quantified |
| Implementation error | Possible; do not claim “validated” beyond what the tests actually check |
| Retired screen-intersection observable | If you mix audit numbers with current \(\theta_E\), you will invent a fake discrepancy |

**Do not say Experiment 1 is “validated against weak-field GR” unless you are talking about Test 4, and even then say the acceptance band is 50% at \(D=200\).**

---

# 8. Numerical integration

## Algorithm

Classical RK4 on the 8-vector \(y = (X,U)\).

\[
\begin{aligned}
k_1 &= f(y),\\
k_2 &= f(y + k_1 h/2),\\
k_3 &= f(y + k_2 h/2),\\
k_4 &= f(y + k_3 h),\\
y_{n+1} &= y + \frac{h}{6}(k_1 + 2k_2 + 2k_3 + k_4).
\end{aligned}
\]

Exact code: `RK4Integrator.cpp` L6–11. \(f =\) `GeodesicDynamics::compute_derivative`.

**Order:** 4 (local truncation \(O(h^5)\), global \(O(h^4)\) for smooth \(f\)).

**Timestep:** fixed. `step_size` must be finite and ≠ 0 (`Propagator.cpp` L9–11). SGL path requires `step_size > 0`.

**State vector:** 8 doubles.

**Derivative evaluation:** 64 `christoffel` virtual calls per \(f\), 4 stages ⇒ **256 Christoffel queries per accepted step**, plus a few more if \(\Gamma\neq 0\) skips do not fire. Many queries return 0.

**Termination:** plane crossing, or \(r \le r_{\min}\), or `max_steps`.

**Floating point:** IEEE `double`.

**Error control:** none (no tolerance, no step doubling).

**Pathological trajectories:** horizon stop; imaginary \(\dot r\) throws at init; `Γ^φ_{θφ}\) pole guard; if bisection mid-point fails to arrive, refinement **breaks** and returns the better endpoint (`refine_observer_hit_bisection` L416–418).

**Constraint checks:** optional null projection every `null_projection_interval` steps (enabled, 1000, on both imaging executables). Observables `null_hamiltonian` used in tests, not in the imaging loop.

## What happens if the timestep is too large?

RK4 global error \(\sim C h^4 L\) grows. The geodesic misses the true periapsis, the plane crossing interpolates a wrong state, `residual_u(b)` zeros shift, \(\theta_E\) biases. Too large \(h\) can also jump over the observer plane in one step; localization still interpolates, but the interpolant is a long chord. Horizon can be stepped over in principle; `r_min` is checked **before** the step on the current state, so a large step can overshoot into \(r < r_s\) before the next check — **CODE does not prevent that**.

## What happens if it is too small?

Cost \(\propto 1/h\). Round-off in the 8-D state can accumulate as \(O(\varepsilon_{\mathrm{mach}} L / h)\). For `double` and \(h=0.01\), \(L \sim (S+D)/h \sim 10^4\) steps, round-off is far below the physics discrepancy with weak-field at \(D=30\). Experiment 1 at \(S=1000\) may approach `max_steps=300000` (`path length / h ≈ 1030/0.01 = 1.03\times 10^5`, still under budget). **NOT DETERMINABLE** without a run whether any Experiment 1 point hits the step budget.

## How integration error propagates into \(\theta_E\)

1. State error at plane crossing → error in interpolated arrival position and direction.
2. Direction error → error in \((u_{\mathrm{ang}}, v_{\mathrm{ang}})\) → error in \(\rho\) and \(\theta_E=\arctan\rho\).
3. Position error → error in `residual_u` → the **root** \(b_\star\) is the \(b\) that zeroes a slightly wrong residual, so \(b_\star\) and the direction are both biased.

Test 3 compares \(\theta_E(h)\) to \(\theta_E(h_{\mathrm{finest}})\), **not** to an analytic geodesic. It can show self-consistency under refinement of \(h\); it cannot by itself prove the radius is physically correct.

---

# 9. Convergence and validation

## Inventory

| Test | What it varies | Observable | Registered? |
|---|---|---|---|
| `scripts/source_distance_test.py` | \(S\) | \(\theta_E(S)\) | manual |
| `scripts/ray_convergence_test.py` | `ray-count` ∈ {101,201,401} | \(\theta_E\) vs finest \(N\) | manual |
| `scripts/step_convergence_test.py` | \(h\) ∈ {0.02, 0.01, 0.005} | \(\theta_E\) vs finest \(h\) | manual |
| `scripts/weak_field_validation.py` | \(S\) at \(D=200\) | \(\theta_E\) vs `weak_field_theta_E` | manual |
| `tests/source_distance_angular_behavior.cpp` | \(S=50,100,200\) + parallel | \(R_{\mathrm{equiv}}\) ordering | CTest |
| `tests/null_constraint.cpp` | projection | \(H\to 0\) | CTest |
| `tests/null_scatter_regression.cpp` | one path | bit baseline, \(E,L,H\) | CTest |
| `tests/ensemble_parallel_invariance.cpp` | 1 vs 4 threads | bitwise outcomes | CTest |
| `tests/canonical_image_pipeline.cpp` / `angular_image_pipeline.cpp` | fixed scene | ring image / smallest \(\rho\) | CTest |
| `tests/true_2d_canonical_validation.cpp` | 1D vs 2D median \(\rho\) | agreement 0.02 | **not** default CTest |
| `tests/true_2d_off_axis_validation.cpp` | \(d=1\) | broken circularity | **not** default CTest |
| `tests/off_axis_observer_hit.cpp` | \(d=1\), 5×5 | ≥2 hits, no azimuthal fill | CTest |
| `tests/schwarzschild_scenarios.cpp` | kernel scenarios | **CODE:** exists; details not re-derived here beyond CTest registration | CTest |
| `tests/propagator_contract.cpp` | propagator API | contract | CTest |

## Ray-count convergence

- **Varied:** \(N =\) number of scan \(b\)-samples in \([2,20]\) at fixed \(S=100\).
- **Measured:** \(\theta_E\) from the **bisection-refined** observer-hit root.
- **What should theoretically converge:** the *existence and identity of the bracketed root*, not a Monte-Carlo mean. Once a sign-change is captured, bisection (tolerance \(10^{-6}\) in \(u\), up to 60 iterations) determines \(b_\star\) almost independently of \(N\).
- **Why:** this is a 1-D root find, not an image statistic.
- **Error type tested:** **sampling / bracket-resolution error**, and the risk of missing a root if the grid is coarser than the residual’s oscillation scale. It is **not** a test of RK4, and **not** a test of pixel binning.
- **Verdict in script:** `|rel_err|` vs highest \(N\) should be nonincreasing. Reference is the largest \(N\), not truth.

**What it does *not* prove:** that 41 rays in Experiment 1 are enough for all \(S\); that the image is converged; that \(\theta_E\) is physically accurate.

## Step-size convergence

- **Varied:** \(h \in \{0.02, 0.01, 0.005\}\), \(S=100\), `max-steps=600000`.
- **Numerical error tested:** RK4 truncation + localization chord error, as they affect \(\theta_E\).
- **Expected order:** if \(\theta_E\) error were dominated by RK4 global error, \(\|\theta(h)-\theta_\star\| \sim C h^4\). The script **does not fit an order**. It only checks that absolute relative error vs \(h=0.005\) shrinks as \(h\) decreases.
- **Does observed order match RK4?** **NOT DETERMINABLE FROM THE CODEBASE** (no order table is computed). Three points vs a numerical reference cannot confirm fourth order, and localization \(O(h^2)\) may dominate, which would look like order 2.

**What it does *not* prove:** agreement with GR, weak-field, or solar SGL.

## Weak-field validation

- **Formula:** see §7.
- **Compared:** numerical \(\theta_E\) vs \(\theta_{\mathrm{an}}\) at \(D=200\), \(S=200,500,\infty\), `step-size=0.05` (coarser than Experiment 1), `ray-count=21`, `b\in[8,40]`.
- **Dimensional consistency:** both sides radians; \(r_s,D,S\) share geometrized length. Internally consistent. **Not** compared to SI solar \(\theta_E\).
- **Agreement establishes:** the numerical observer-hit angle is within a **factor-of-two** of the thin-lens weak-field Einstein angle in a moderately weak regime (`within_50pct_of_analytic`).
- **Does *not* establish:** percent-level GR accuracy; correctness at \(D=30\); solar-scale SGL; that sampling and RK4 errors are negligible compared to model error (Test 4 uses coarser \(h\) and fewer rays than Experiment 1).

## Sampling error ≠ integration error ≠ model error

| Error | What it is | Constrained by |
|---|---|---|
| **Sampling** | discrete \(b\) grid misses/shifts a bracket | Test 2 (weakly, because bisection erases most of it) |
| **Integration** | RK4 + localization | Test 3 (self-convergence only); `null_scatter_regression` (one path vs stored baseline) |
| **Model** | Schwarzschild geometric optics vs nature or vs weak-field formula | Test 4 (vs formula, 50% band); **not** vs nature |

Constraint projection tests (`null_constraint.cpp`) constrain **constraint drift**, a fourth error source, not \(\theta_E\) directly.

**Do not say “the code is validated” without naming the test.** Say which error source which test bounds, and how loosely.

---

# 10. 2D photon sampling

## What symmetry was previously exploited

On-axis spherical symmetry: one equatorial geodesic + `expand_angular_azimuthally` fills the ring. That is the 1D path.

## What is sampled in 2D

A square launch plane at the source: cell-centered \((b_u, b_v) \in [-b_{\max}, b_{\max}]^2\), \(N\times N\) rays, \(N=\) `--samples-per-axis` (CLI default 5 in `true_2d_sgl_image.cpp`).

**Physical meaning of the two dimensions:** Euclidean offsets along world \(\hat X\) and \(\hat Y\) on the source plane. All rays share direction \(\widehat{\mathrm{lens}-\mathrm{source}}\). This is a **parallel beam**, not a point-source fan, and **not** observer-plane pixels.

## Distribution

Deterministic uniform grid in launch-parameter space. **Not** uniform on the sky, **not** importance-sampled, **not** Monte Carlo.

## How many trajectories

Search: \(N^2\). Then Newton on seeds (typically \(O(N)\) along a ring, from the fragmentation audit tables — those tables are **DOC**, empirical, not a theorem). Each Newton trial is another full geodesic (`evaluate_launch`).

## Accumulation / observable

Search geodesics are **not imaged**. Only Newton-refined observer hits enter `angular_coordinate`. **On-axis**, `fill_aligned_observer_ring` then **replaces** those hits by median \(\rho\) plus azimuthal expansion (`true_2d_sgl_image.cpp` L431–432). So the on-axis 2D *picture* is again a symmetry fill. Off-axis (`observer_distance ≠ 0`) skips the fill and images the real sparse hits.

## What is gained by abandoning 1D symmetry

**CODE, off-axis:** the 1D executable refuses to run. 2D can place the observer at \(D\hat Z + d\hat X\) and find hits without assuming a circular image. Tests assert broken circularity and no azimuthal copy.

**CODE, on-axis:** a consistency check that a 2D search + Newton recovers a \(\rho\) close to the 1D root (`true_2d_canonical_validation.cpp`, not in default CTest). The pretty ring is still azimuthally filled.

This section does not discuss off-axis *physics* beyond what the code implements: a displaced point observer and a Newton residual \(F=(u,v)=0\) on the observer plane (`ObserverLaunchRefiner.cpp` L20–27, Gauss–Newton with damped \(J^\top J\)).

---

# 11. Parallelization

## Work unit

Independent geodesic integrations: loop index `i` over `ensemble.size()` in `Rays::propagate_ensemble` (`EnsemblePropagator.cpp` L15–21):

```c++
#pragma omp parallel for schedule(dynamic) if (n > 1)
outcomes[i] = Propagation::propagate(ensemble.at(i).initial_state, ...);
```

Second parallel loop: Newton seeds in `refine_observer_launches`. Inner `evaluate_launch` uses a 1-ray ensemble, so `if (n > 1)` keeps nested OpenMP from firing.

## What each thread receives

A copy of the initial `State` (by const access to `ensemble.at(i)`), and shared const references to dynamics, integrator, settings, termination, correction.

## Shared vs thread-local

- **Shared:** `PropagationContext` (non-copyable), metric, dynamics, integrator, problem geometry, settings.
- **Thread-local:** current/previous `State`, step loop, `outcomes[i]` / `per_seed[i]`.

Correction lambda captures `rs` and `interval` **by value** (`PropagationContext.cpp` L19).

## Race conditions

Indexed writes, no `push_back` in the parallel region. Formal proof: **NOT DETERMINABLE**. Empirical: `ensemble_parallel_invariance` requires **bitwise identical** outcomes for 1 vs 4 threads on a 5-ray ensemble. Newton OpenMP is **not** under that test.

## Combining results / ordering

Outcomes are slotted by index; ray `id` is the ensemble index. Ordering of completion does not matter. Dedup/sort of Newton hits is serial after the parallel region.

## Embarrassingly parallel?

**Yes, at the geodesic level.** Photons do not interact. No shared accumulation during integration. Image `+= 1` is serial after the fact.

Why naturally parallel: the geodesic ODE for ray \(i\) does not depend on ray \(j\). Gravity is a fixed background metric (no backreaction).

## Expected vs measured scaling

Expected: near-linear in thread count until memory bandwidth / `christoffel` virtual-call overhead saturates, for \(N_{\mathrm{rays}} \gg n_{\mathrm{threads}}\). Dynamic schedule helps unbalanced lengths (captured vs escaping rays).

Measured speedup curves: **NOT DETERMINABLE FROM THE CODEBASE.** Anecdote in living docs (“8 vs 12 on i5-13420H”) is not a benchmark table. Penrose’s benchmark harness was excluded at extraction.

## Overheads / memory

- OpenMP fork/join per ensemble.
- Virtual `christoffel` calls: function-call overhead is real on the 256-queries/step path.
- Imaging path does **not** store full trajectories (`propagate`, not `propagate_recorded`), so memory is \(O(N_{\mathrm{rays}})\) states, not \(O(N_{\mathrm{rays}}\times N_{\mathrm{steps}})\).

## GPU

There is **no GPU code**. What would be accelerated is the **kernel**

```text
for steps:
    k1..k4 = GeodesicDynamics.compute_derivative  (64 Γ queries each)
    RK4 update of 8 doubles
    plane-distance / r check
```

i.e. one thread (or one CUDA thread) per photon, with \(\Gamma\) as a device function. Host-side Python, image binning, and bisection control flow would stay on CPU unless rewritten. A GPU port is not “make OpenMP into CUDA”; it is a rewrite of `christoffel` + RK4 + termination without virtual calls and with coalesced state arrays. Notes that mention GPU refer to **Penrose history / future requirements**, not this repo.

---

# 12. Computational complexity

Let \(N_b\) = 1D ray count, \(N\) = 2D samples per axis, \(M\) = `max_steps`, \(h\) = step size, \(N_S\) = number of source-distance points, \(I\) = root iterations, \(A\) = azimuth count, \(R\) = image resolution.

### 1D Experiment 1 (dominant scientific path)

\[
C \approx N_S \Big( N_b \cdot C_{\mathrm{geo}} + N_{\mathrm{brackets}} \cdot I \cdot C_{\mathrm{geo}} + O(A + R^2) \Big)
\]

\(C_{\mathrm{geo}} \sim 4 \times 64 \times N_{\mathrm{steps}} \times C_\Gamma\) with \(N_{\mathrm{steps}} \sim L/h\) and \(L \sim S+D\).

**Dominant bottleneck from the code:** **arithmetic in `christoffel` + RK4**, invoked inside the scan and again inside bisection. Image formation is negligible. Trajectory storage is off. Python orchestration is negligible vs C++.

For \(S=100\), \(h=0.01\), \(N_b=41\), \(I\le 60\), a few brackets: order \(10^6\)–\(10^7\) RK4 steps per data point (order-of-magnitude from \(N_{\mathrm{steps}}\sim 10^4\) times tens of geodesics), each 256 Γ queries.

### 2D

\[
C \approx N^2 C_{\mathrm{geo}} + S_{\mathrm{seeds}} \cdot O(I) \cdot C_{\mathrm{geo}}
\]

Search \(N^2\) often dominates geodesic count; Newton can dominate wall time because each trial is a full geodesic and line search multiplies trials.

### Bottleneck classification

| Candidate | Verdict |
|---|---|
| Arithmetic | **Yes — Γ and RK4** |
| Memory | Unlikely on imaging path (no path storage) |
| Branching | Moderate (`if Gamma != 0`, metric `if` ladder, termination) |
| Function-call overhead | **Yes — virtual `christoffel` 256×/step** |
| Integration | **The hot loop** |
| Trajectory storage | Off for imaging |
| Post-processing | Negligible |

Benchmarks: **no in-tree C++ benchmark binary.** Python scripts record `elapsed_seconds` per run in `results.csv`; those files exist only after you execute the tests. **NOT DETERMINABLE** here what the numbers were on any particular machine unless those artifacts are present in the working tree.

---

# 13. Physical approximations and limitations

Brutal list. “Now” = relevant to defending **this** code. “Mission” = needed before claiming a Solar Gravitational Lens spacecraft model.

| # | Assumption | Why it was made | Physics omitted | When it matters | SGL mission impact | To remove it |
|---|---|---|---|---|---|---|
| 1 | Geometric optics / null geodesics | Kernel is GR geodesics | Diffraction, interference, wave optics, PSF of the SGL | Always for imaging resolution; SGL’s scientific product *is* a diffraction-limited PSF | **Mission-critical.** Geometric ring radius ≠ telescope image | Helmholtz / Kirchhoff / Fresnel propagation after geodesics, or wave-optical GR |
| 2 | Discrete rays | Computational | Continuous bundle, étendue | Flux, magnification | Cannot predict SNR | Jacobian / magnification / more rays |
| 3 | Unit intensity, no magnification | Simplicity (`accumulate += 1`) | Inverse magnification, time delay, redshift | Photometry | Cannot predict collected power | Conservation of \(I_\nu/\nu^3\), amplification map |
| 4 | Point source | `Source` is a 3-vector | Finite stellar disk, limb darkening, structure | Ring width, resolvability of exoplanet | **Mission-critical** for the actual science target | Extended-source convolution |
| 5 | Point observer / no spacecraft | `Observer` is a point | Telescope aperture, formation flying, pointing | Focal-line depth ~ AU, meter-scale PSF | **Mission-critical** | Finite aperture, detector model |
| 6 | Schwarzschild, non-rotating | Penrose extract; Birkhoff | Frame dragging, solar \(J_2\), multipoles | Microarcsecond astrometry; solar oblateness already matters for light deflection | **Mission** (known issue in SGL literature) | Kerr + post-Newtonian multipoles |
| 7 | Vacuum | Metric is vacuum Schwarzschild | Solar corona / plasma refraction (\(\propto \lambda^2\)) | Radio vs optical; corona at few \(R_\odot\) | **Mission**, wavelength-dependent | Plasma refractive index on top of GR |
| 8 | No solar disk occultation as a physical emitter | Rays with small \(b\) just fall in or miss | Photosphere blocks rays with \(b \lesssim R_\odot\) | Which part of the Einstein ring exists | **Mission** — the ring hugs the solar limb | Absorb rays with periapsis \(< R_\odot\) (need periapsis output) |
| 9 | \(r_s = 1\), \(D=30\) toy scaling | Numerical convenience | Actual \(r_{s,\odot}\), 550 AU | Every SI number | **Do not quote solar \(\theta_E\) from these runs** | Insert `PhysicalConstants` and AU-scale \(D\) (cost: huge \(L/h\)) |
| 10 | Equatorial 1D + azimuthal copy | Cheap ring | True 3-D launch; polar \(\Gamma\) singularity | Off-axis, 2D without fill | On-axis OK by symmetry; off-axis **invalid** on 1D path | Already started: 2D Newton path |
| 11 | Fixed-step RK4 | Simplicity | Adaptive error control | Tight periapsis, long \(S\) | Accuracy vs cost at solar scale | Adaptive RK / symplectic / Gauss–Legendre |
| 12 | Linear arrival interpolate | Cheap localization | True geodesic chord | Large \(h\) | Biases \(\theta_E\) | Sub-step root on signed distance |
| 13 | Periodic null projection | Fight constraint drift | Exact \(g(U,U)=0\) preservation | Long integrations | Constraint error → direction error | Constraint-preserving integrator |
| 14 | No photon statistics / detector | Not an instrument model | Shot noise, QE, jitter | Detection | **Mission** | Instrument layer |
| 15 | Static metric | Schwarzschild | Solar oscillations, spacecraft motion during light-time | Timing | Later | Time-dependent background |
| 16 | `"inf"` = finite parallel plane | Cannot start at infinity numerically | True \(\mathcal{I}^-\) data | Comparison to asymptotic formulae | Conceptual | Start at large \(S\) with parallel rays; extrapolate |

**Relevant now** (you will be asked): 1, 4, 6, 9, 10, 11, 16, and the retired-vs-current observable distinction.

**Future mission modelling:** 1, 3, 4, 5, 7, 8, 9, 14.

---

# 14. Dimensional analysis and units

## System

**CODE:** geometrized units \(G = c = 1\). `Units.h` is a four-line comment and is **never included**. `PhysicalConstants.h` defines SI \(G, c, M_\odot, R_\odot\) and \(r_s = 2GM/c^2\), and is **never included** by any implementation TU.

**All current simulations are normalized, not solar-scaled.** \(r_s = 1\) is hard-coded in both imaging `main`s.

## Quantity audit

| Quantity | Internal | Physical | Conversion in execution path? |
|---|---|---|---|
| Schwarzschild radius \(r_s\) | `1.0` | \(2GM/c^2\) | **No** |
| Sun mass | unused SI constant | \(1.98847\times 10^{30}\,\mathrm{kg}\) | **No** |
| Source distance \(S\) | geometrized length | would be \(S \times r_s^{\mathrm{SI}}\) if scaled | **No** |
| Observer distance \(D\) | geometrized length | same | **No** |
| Impact parameter \(b\) | geometrized length (1D: \(L/E\)) | length | **No** |
| Affine parameter \(\lambda\) | RK4 `step_size` | time/length in \(G=c=1\) | unnamed |
| Coordinate time \(t\) | \(X[0]\) | Killing time | not converted |
| Spatial coordinates | Schwarzschild \(r,\theta,\phi\) / world Cartesian | geometrized | **No** |
| \(\rho\) | dimensionless (\(\tan\theta\)) | — | — |
| \(\theta_E\) | radians | radians | already an angle |
| \(R_{\mathrm{equiv}}\) | \(D\rho\) geometrized length | not metres | **No** |
| Image `extent` | gnomonic tangent-plane span | dimensionless | — |

## What you must not claim

- Any result in metres, kilometres, AU, or arcseconds **unless you do the conversion yourself outside this code**.
- “This is the Einstein ring of the Sun as seen from 550 AU.”
- “Our \(\theta_E\) matches the mission-study value of ~1 arcsecond.” The code has never been run at that scale in any in-tree script (`D=30` or \(200\) with \(r_s=1\)).
- That `PhysicalConstants.h` is “how we convert to SI.” It is dead code.

If asked for a solar number, compute it **on the whiteboard** from \(\theta_E \approx \sqrt{2 r_{s,\odot}/D}\) with \(D\sim 550\,\mathrm{AU}\), and state that this repository’s runs are a **dimensionless numerical laboratory** with \(r_s=1\).

---

# 15. “Defend every figure”

## Per-run C++ images (`einstein_ring.csv` / `.pgm`)

- **Shows:** max-normalized count of azimuthally copied gnomonic samples.
- **Axes:** gnomonic \((u_{\mathrm{ang}}, v_{\mathrm{ang}})\), extent `--extent` (default 0.8), dimensionless.
- **Expected:** a thin circle if the copy radius falls inside the frame.
- **Why:** spherical symmetry is **imposed** by `expand_angular_azimuthally`.
- **May conclude:** the pipeline can render a ring-shaped count image from one observer-hit angle.
- **Must not conclude:** photometric Einstein-ring surface brightness; diffraction PSF; that the radius was measured from pixels.
- **Expert questions:** “Is this a histogram of rays or a symmetry plot?” → symmetry plot. “Where is magnification?” → nowhere.

`true_2d_image.csv` / `.pgm`: same binning; on-axis still filled by median \(\rho\); off-axis is sparse real hits.

`assets/ring.png` in README: **NOT DETERMINABLE FROM THE CODEBASE** how it was generated (static asset).

## `theta_E_vs_source_distance.png` (Experiment 1) — deep analysis

- **Shows:** \(\theta_E(S)\) for \(S\in\{20,50,100,200,1000\}\) plus a horizontal line \(\theta_E(\infty_{\mathrm{stand-in}})\).
- **X:** source distance in geometrized units (\(r_s=1\)).
- **Y:** \(\theta_E=\arctan\rho\) in **radians**, from observer-hit root.
- **Expected:** increasing, approaching the parallel-beam value.
- **Why:** finite-distance Einstein radius grows toward the infinite-source limit (see §7).
- **May conclude:** within this numerical laboratory, the observer-hit angle increases with \(S\) and the parallel-beam run is consistent with that trend (also CTest-enforced on \(R_{\mathrm{equiv}}\)).
- **Must not conclude:** solar SGL; weak-field accuracy (formula not on this plot); that `"inf"` is true infinity; that the image pixels determined the points.
- **Likely expert questions:**
  - “Is \(S\) in metres?” → No, \(r_s=1\).
  - “Why isn’t this \(\sqrt{1/S}\) or decreasing?” → You may be remembering the **retired screen-intersection** radius. Current observable is angular and increases.
  - “Why \(D=30\)?” → Toy scale; \(r_s/D\) is not small.
  - “Show me the geodesic that produced \(S=100\).” → The refined observer-hit ray at `selected_observer_hit_b`, not the 41 scan rays.

Companion plot `theta_E_relative_vs_source_distance.png`: \((\theta_E(S)-\theta_E^{\mathrm{par}})/\theta_E^{\mathrm{par}}\). Tests approach to the parallel stand-in, not to weak-field theory.

## Sweep plotter (`plot_source_distance_sweep.py`)

Primary series still `selected_angular_radius` / \(\theta_E\). Image-derived `image_peak_rho` is a **cross-check only**. `R_equiv = D\rho` is a gnomonic screen equivalent, not a measured focal radius.

**If `parameter_sweep.py` is run as committed:** the active sweep is `observer_distance`, not source distance. Do not assume a source-distance sweep directory exists unless you produced it.

## Ray-count / step-size plots

Self-convergence diagnostics. X = \(N\) or \(h\); Y = \(\theta_E\) or relative error vs the finest setting. They do not include an analytic overlay.

## Weak-field plots

Numerical vs `weak_field_theta_E` at \(D=200\). Verdict band 50%. Defend as a **sanity check in a moderately weak regime**, not a precision GR test.

---

# 16. Expert interrogation (50+ questions)

For each: what is being tested; the code-based answer; math; common wrong answers; a spoken reply.

### A. SGL physics

**A1. What is the Solar Gravitational Lens, and what of it does this code contain?**  
*Tests:* conceptual vs implementation honesty.  
*Code:* geometric-optics Schwarzschild observer-hit angle for a point source, toy \(r_s=1\). No 550 AU focus, no corona, no wave PSF.  
*Math:* SGL uses the Sun as a lens with \(\theta_E \approx \sqrt{2 r_s/D}\).  
*Wrong:* “We simulated a flight SGL mission.”  
*Spoken:* “We have a dimensionless Schwarzschild ray-tracer that extracts the Einstein angle of an aligned point source. It is the GR kernel of an SGL forward model, not a mission simulation.”

**A2. Where is the SGL focal line in the code?**  
*Tests:* whether you confuse optical axis with the ~550 AU focal region.  
*Code:* CLI “focal line” = optical axis. The heliocentric distance where the PSF collapses is not computed.  
*Spoken:* “It is not in the code. We place a point observer at coordinate distance \(D\).”

**A3. Why a ring rather than an arc?**  
*Tests:* on-axis degeneracy.  
*Code:* on-axis + spherical symmetry; 1D *imposes* a circle by rotation. Off-axis 2D does not.  
*Spoken:* “Alignment plus spherical symmetry. We rotate one hit around the axis. Off-axis, that copy is forbidden.”

**A4. What would a finite solar disk do to this ring?**  
*Tests:* point-source limitation.  
*Code:* source is a point.  
*Spoken:* “The ring would thicken into an annulus set by the source angular size. We have not implemented that.”

**A5. Does the code occult the solar photosphere?**  
*Tests:* \(b_{\min}=2\) vs \(R_\odot/r_s \approx 2.3\times 10^5\).  
*Code:* no photosphere. \(b\) is \(O(1)\)–\(O(20)\) in units \(r_s=1\), i.e. near the photon sphere, **not** grazing the real Sun.  
*Spoken:* “No. Our impact parameters are a few Schwarzschild radii, a black-hole regime, not a solar-limb regime.”

**A6. Radio vs optical SGL — does plasma appear?**  
*Code:* vacuum Schwarzschild only.  
*Spoken:* “No plasma. Frequency never enters the geodesic.”

### B. General Relativity

**B1. Write the Schwarzschild line element the code uses.**  
\[
ds^2 = -(1-r_s/r)dt^2 + (1-r_s/r)^{-1}dr^2 + r^2 d\Omega^2.
\]  
*Code:* not stored as \(g_{\mu\nu}\); \(f=1-r_s/r\) appears in ICs and observables; \(\Gamma\) from that metric are hardcoded.

**B2. Why are photons affine-parametrized, not proper-timed?**  
Null \(ds=0\) ⇒ proper time does not progress.  
*Spoken:* “We integrate \(d^2 x^\mu/d\lambda^2 + \Gamma^\mu_{\alpha\beta} \dot x^\alpha \dot x^\beta = 0\) with RK4’s \(\lambda\). Proper time is zero along the ray.”

**B3. What is \(b_{\mathrm{crit}}\)?**  
*Code:* \((3\sqrt{3}/2) r_s\), photon-sphere impact parameter. Photon sphere \(r=1.5 r_s\).  
*Spoken:* “Below that, rays can be captured. Our scan starts at \(b=2\), which is sub-critical, so some rays hit the horizon and never reach the observer.”

**B4. Does the integrator use conserved \(E\) and \(L\) to reduce the ODE?**  
*Code:* **No.** Full 8-D system. \(E,L\) used at **initialization** and in **diagnostics**.  
*Wrong:* “We integrate the 1-D radial potential.”

**B5. How is the null condition \(g(U,U)=0\) maintained?**  
Constructed at init; projected every 1000 steps; not preserved by RK4.  
*Spoken:* “It is an initial constraint plus a periodic projection. RK4 will drift it.”

**B6. Is this Schwarzschild or isotropic coordinates?**  
Schwarzschild areal \(r\).  
*Spoken:* “Areal radius. I will not quote isotropic-radius SGL formulae without converting.”

**B7. Birkhoff’s theorem — why can a non-rotating Sun be Schwarzschild outside?**  
Vacuum spherical ⇒ unique.  
*Spoken:* “That is why Schwarzschild is the right *vacuum spherical* model. The real Sun rotates and has a quadrupole; that is not in the code.”

### C. Mathematical derivation

**C1. Derive \(\theta_E^2 = 2 r_s D_{LS}/(D_L D_S)\) in \(G=c=1\).**  
Einstein radius from \(\alpha = 2 r_s/b\) and thin-lens geometry \(b = D_L \theta\), \(\beta=0\) ⇒ \(\theta = \alpha D_{LS}/D_S\).  
*Code uses* Euclidean \(D,S\) in that slot in Test 4 only.

**C2. Why \(\theta_E = \arctan\rho\) rather than \(\rho\)?**  
\(\rho = \tan\theta\) by gnomonic construction.  
*Spoken:* “Our image plane is a tangent plane. \(\rho\) is tan of the true angle; we undo that with atan.”

**C3. Show that \(E = f \dot t\) is conserved.**  
\(\partial_t\) Killing vector; \(E = -k_\mu U^\mu = f \dot t\) (code’s sign convention in `conserved_energy`).  
*Code:* not used as a reduction; monitored in tests.

**C4. Write the equatorial null radial equation the IC uses.**  
\(\dot r^2 = E^2 - f L^2 / r^2\) (`inside_vr` in `build_null_scatter`).  
*Spoken:* “That is how we set inbound \(\dot r\) from \(b\).”

**C5. Why does finite \(S\) change \(\theta_E\) even at fixed \(D\)?**  
Lens equation: \(D_{LS}/D_S = S/(D+S)\) increases with \(S\).  
Also ICs: \(r_0=S\) changes \(f\) and \(\dot\phi\).

**C6. Is \(b=L/E\) equal to the Cartesian launch offset?**  
Only asymptotically. Finite-\(S\) point model: **no**. Parallel/2D: Cartesian offset is the sampled parameter; \(L\) follows after `build_custom`.

### D. Numerical methods

**D1. What integrator, what order, adaptive?**  
RK4, order 4, fixed \(h\).  
*Spoken:* “Classical RK4. No adaptive control.”

**D2. Write one RK4 step on the geodesic state.**  
See §8. \(f\) returns \((U, a)\) with \(a^\mu = -\Gamma^\mu_{\alpha\beta} U^\alpha U^\beta\).

**D3. Why might observed convergence not be \(O(h^4)\)?**  
Plane localization is linear in the last step (\(O(h^2)\)); constraint projection; event detection. Test 3 does not fit an order.

**D4. What is the bisection solving?**  
\(u(b)=0\) where \(u\) is the observer-plane intercept x-coordinate.  
*Spoken:* “Find the impact parameter whose geodesic goes through the observer, not merely through the plane.”

**D5. Gauss–Newton in 2D — residual and Jacobian?**  
\(F(b_u,b_v) = (u,v)\) plane coordinates of the hit. Jacobian by finite difference; damped \(J^\top J\) (`ObserverLaunchRefiner.cpp` L31–37).  
*Spoken:* “We Newton-solve launch parameters so the geodesic threads the observer point.”

**D6. Floating-point type?**  
`double` throughout.

**D7. What if `max_steps` is hit?**  
Status `StepBudgetExhausted`; may be treated as no valid crossing. Experiment 1 does not log this per ray in `run_summary.txt` beyond counts. **NOT DETERMINABLE** per-photon without stderr/debug.

### E. Code architecture

**E1. Why is Penrose a generalized engine?**  
See §3. Metric → Γ → geodesic ODE → RK4, with SGL as a client.  
*Spoken:* “The kernel integrates geodesics given Christoffel symbols. The Sun and the ring live in other libraries.”

**E2. What would Kerr require tomorrow?**  
New `Metric`, ICs, null constraint, observables, horizon, chart. Dynamics/RK4/Propagator stay.  
*Spoken:* “I would not touch RK4. I would replace the Christoffels and every place that assumes \(f=1-r_s/r\).”

**E3. Does geometry link the GR kernel?**  
**No.** CMake forbids `sgl_geometry` from linking `sgl_physics`.

**E4. Where is \(g_{\mu\nu}\) stored?**  
Nowhere in `Metric`. Rebuilt ad hoc as \(f\) where needed.

**E5. Two samplers — which does Experiment 1 use?**  
`RaySampler` + `build_null_scatter` (`--ray-model point`). Not `RayGrid2DSampler`.

**E6. Help text vs defaults?**  
`print_usage` says ray-count default 41 / resolution 512; `CliOptions` defaults are 801 / 1024. Experiment 1 passes flags explicitly, so it is safe; a bare `./sgl_canonical_sgl_image` uses **801** rays. Flag this if asked about “defaults.”

### F. Validation

**F1. What does ray-count convergence prove?**  
Bracket capture / stability of the refined root vs scan density. Not RK4 order, not weak-field truth.

**F2. What does step-size convergence prove?**  
Self-consistency of \(\theta_E\) under refining \(h\), vs the finest \(h\), not vs analytics.

**F3. What does weak-field validation prove?**  
Order-of-magnitude agreement (50% band) at \(D=200\) with a thin-lens formula.

**F4. Is the decreasing radius in the physics audit a bug in today’s code?**  
**No.** That audit is a historical screen-intersection observable. Current \(\theta_E\) increases with \(S\).  
*Spoken:* “If I showed you that decreasing sequence I would be quoting a retired metric. Today we plot the observer’s angle.”

**F5. Bitwise parallel invariance — what does it not cover?**  
Newton OpenMP path.

**F6. Are 2D validation binaries in CI?**  
Built, not in default `ctest`.

### G. Sampling and statistics

**G1. Is there Monte Carlo?**  
No.

**G2. Is sampling uniform on the sky?**  
No. Uniform in \(b\) or in \((b_u,b_v)\).

**G3. Does more azimuth improve \(\theta_E\)?**  
**No.** Azimuth is visualization. \(\theta_E\) is from one hit.

**G4. Why 41 rays in Experiment 1?**  
Scan grid for sign changes. Precision comes from bisection, not from 41.

**G5. Importance sampling?**  
Not implemented.

**G6. Photon noise?**  
Not implemented. Counts are +1 per symmetry sample.

### H. Parallel computing

**H1. What is parallelized?**  
Independent geodesics (and 2D Newton seeds). OpenMP `parallel for dynamic`.

**H2. Why embarrassingly parallel?**  
No photon–photon coupling; fixed background metric.

**H3. GPU kernel?**  
`compute_derivative` + RK4 + termination per ray. Not present.

**H4. How do I set threads?**  
`OMP_NUM_THREADS`. No C++ `--threads`. Python sweep `--threads` sets the env var.

**H5. Measured speedup?**  
**NOT DETERMINABLE** as a curve in-repo.

**H6. Nested OpenMP?**  
Guarded off: 1-ray ensembles skip the pragma.

### I. Mission implications

**I1. Can this design a coronagraph / spacecraft distance?**  
Not as-is. Toy \(D\), no photosphere, no wave optics.

**I2. What is the first thing to add for a mission-grade forward model?**  
Solar scaling + limb occultation + wave-optical PSF, then plasma, then \(J_2\), then finite source. (Engineering judgment; the repo’s README lists the gaps without a ranked roadmap in code.)

**I3. How far is 550 AU in code units?**  
\(D / r_{s,\odot} \approx 8.2\times 10^{13}\,\mathrm{m} / 2.95\times 10^3\,\mathrm{m} \sim 3\times 10^{10}\). At \(h=0.01\) that is an impossible step count without rescaling or adaptive methods. **This is why the lab uses \(r_s=1\), \(D=30\).**

**I4. Does the code predict integration time or SNR?**  
No photometry.

**I5. Off-axis exoplanet image?**  
2D path can displace the observer and refuse azimuthal fill. It still uses a point source and unit weights. Not a planet image.

### J. Limitations

**J1. Diffraction?**  
Absent.

**J2. Solar rotation / Kerr?**  
Excluded at Penrose extraction.

**J3. Detector?**  
PGM is a visualization of count images.

**J4. What is a toy-model assumption vs a genuine GR result?**  
*Genuine (within Schwarzschild geometric optics, toy scaling):* observer-hit angle from integrated null geodesics; increase of that angle with \(S\); existence of a critical \(b\).  
*Toy:* \(r_s=1\), \(D=30\), point source, azimuthal copy, unit weights, `"inf"` stand-in, no Sun disk.

**J5. Pole in \(\Gamma^\phi_{\theta\phi}\)?**  
`sinθ + 1e-8`. Aligned 1D path sits at \(\theta=\pi/2\). 2D rays along world \(+\hat Y\) can hit the pole (`WorldFrame.h` residual note). Phase-3 orbital-plane rotation is **documented, not implemented**.

**J6. Could RK4 step across the horizon?**  
Termination checks current state *before* the step; a large \(h\) can land inside \(r_s\).

---

# 17. Questions you currently struggle with (direct answers)

**1. Exactly how is one point on the source-distance vs Einstein-ring-radius plot generated?**  
Trace B. \(S\) → `sgl_canonical_sgl_image` → scan \(u(b)\) → bisection → \(\rho\) → \(\theta_E=\arctan\rho\) → Python plots that scalar. Image unused.

**2. What exactly is the code measuring when it says “Einstein-ring radius”?**  
The angle between the observer optical axis and the incoming direction of the unique primary geodesic that passes through the observer. Stored as \(\theta_E\) (rad) and \(\rho=\tan\theta_E\). Not a pixel radius, not a screen-intercept of a fixed-\(b\) family.

**3. Why does an Einstein ring form instead of a point or blob?**  
Physics: spherical symmetry + alignment ⇒ azimuthal degeneracy. Code: one hit is rotated into a circle. A point is the undeflected image in flat space; a blob needs broken symmetry or a finite source, neither of which the 1D on-axis path has.

**4. What changes mathematically when the source is at finite distance?**  
ICs: \(r_0=S\), \(f(S)\), \(\dot r(S,b)\), \(\dot\phi(S,b)\). Geometry: \(D_{LS}/D_S = S/(D+S)\). The \(b_\star\) that satisfies \(u(b)=0\) moves, so \(\theta_E\) moves.

**5. Why does the ring radius change with source distance?**  
The observer-hit direction’s angle grows toward the parallel-ray limit as \(S/(D+S)\to 1\). CTest enforces the increase on \(R_{\mathrm{equiv}}\).

**6. Why does the numerical result differ from the weak-field analytical result?**  
Different theories (full Schwarzschild ODE vs linearized thin lens), toy \(r_s/D\) not ≪ 1 in Experiment 1, `"inf"` not infinity, plus numerical error. Experiment 1 does not even overlay the formula.

**7. How do I know the discrepancy is physical rather than numerical?**  
I do **not** know that from Experiment 1 alone. Partial evidence: Test 3 self-convergence in \(h\); Test 2 stability in \(N\); Test 4 still disagrees at \(D=200\) inside a 50% band — that remaining gap is then mostly **model** error, but Test 4 also uses coarser \(h\). Honest answer: we have not published a controlled error budget that isolates the three sources to, say, 1%.

**8. What does ray-count convergence actually prove?**  
That the refined observer-hit \(\theta_E\) is stable once the \(b\)-grid resolves a sign change. It does **not** prove the image is converged or the physics is right.

**9. What does timestep convergence actually prove?**  
That \(\theta_E(h)\) approaches \(\theta_E(h_{\min})\) in the tested set. It does not prove RK4 order and does not prove physical accuracy.

**10. Why is Penrose genuinely a generalized GR engine?**  
Because SGL objects are not in `sgl_physics`. Any metric that implements `christoffel` can drive the same ODE + RK4 + propagator. Kerr was already a Penrose client and was cut at extraction.

**11. What exactly would have to change to support Kerr?**  
§3 table. Short: Christoffels, parameters \(a\), ICs, null constraint, observables, horizon, chart. Not RK4.

**12. Why is the photon calculation embarrassingly parallel?**  
Non-interacting geodesics on a fixed metric. OpenMP over ray index with disjoint output slots.

**13. What exactly would be accelerated on a GPU?**  
The per-ray RK4 loop evaluating hardcoded (or Kerr) Christoffels — `GeodesicDynamics::compute_derivative` + `RK4Integrator::step` + termination tests.

**14. What is currently a toy-model assumption versus a genuine physical result?**  
See J4. Genuine: Schwarzschild null-geodesic observer-hit angle and its qualitative \(S\)-dependence. Toy: scaling, Sun model, photometry, wave optics, `"inf"`.

**15. What would have to be added before this could be called a mission-grade SGL forward model?**  
SI solar scaling and feasible integration at \(D\sim 550\,\mathrm{AU}\); photospheric occultation; wave-optical PSF; plasma; solar multipoles; finite source; finite aperture/detector; photometry. README already lists the first half of that as “not yet implemented.”

---

# 18. Mental model

## One paragraph — physically

A point mass curves spacetime; photons fall on null geodesics; if a point source, the mass, and a point observer lie on a line, every azimuth offers an equivalent deflected path, so the observer sees a ring whose angular radius is the angle of those incoming photons to the line of sight. This code computes that angle in Schwarzschild geometry at toy scale, then *draws* the ring by spinning that angle around the axis.

## One paragraph — mathematically

The state \((X^\mu, U^\mu)\) obeys \(\dot X = U\), \(\dot U^\mu = -\Gamma^\mu_{\alpha\beta} U^\alpha U^\beta\) with handwritten Schwarzschild \(\Gamma\), started from a null vector with \(E=1\), \(L=b\), \(\dot\theta=0\) at \(r=S\). A 1-D root \(u(b)=0\) selects the geodesic through the observer. Gnomonic coordinates of \(-\hat U_{\mathrm{spatial}}\) give \(\rho=\tan\theta_E\). Weak-field comparison, when used, is \(\theta_{\mathrm{an}}=\arctan\sqrt{2 r_s S/(D(D+S))}\) (with \(S\to\infty\) reducing the radicand to \(2 r_s/D\)).

## One paragraph — computationally

C++ libraries separate a metric-agnostic RK4 geodesic engine from SGL geometry and imaging. Experiment 1 is a Python driver around `sgl_canonical_sgl_image`: 41 fixed-step geodesics, OpenMP if available, bisection, one scalar written to a text file, Matplotlib. Cost is \(O(N_{\mathrm{rays}} \times N_{\mathrm{steps}})\) Christoffel evaluations. No SI conversion, no GPU, no wave optics.

## Pipeline diagram

```text
┌────────┐    ┌─────────────────────┐    ┌──────────────────┐    ┌───────────────┐
│ Source │ →  │ initial photon ICs  │ →  │ Penrose / GR     │ →  │ geodesic      │
│ (0,0,−S)│    │ build_null_scatter  │    │ Γ → 8 ODEs → RK4 │    │ trajectories  │
└────────┘    └─────────────────────┘    └──────────────────┘    └───────┬───────┘
                                                                         ↓
┌────────────┐    ┌───────────────────────────┐    ┌─────────────────────────────┐
│ observable │ ←  │ observer-hit root +       │ ←  │ observer plane crossing     │
│ θ_E=atan(ρ)│    │ gnomonic (u_ang, v_ang)   │    │ residual_u(b)=0             │
└────────────┘    └───────────────────────────┘    └─────────────────────────────┘
        ↑
        visualization only: rotate ρ → count image
```

## Know this cold (20 items)

1. \(ds^2 = -f dt^2 + f^{-1} dr^2 + r^2 d\Omega^2\), \(f=1-r_s/r\), \(r_s=1\) in all current runs.
2. Geodesic ODE: \(\ddot x^\mu + \Gamma^\mu_{\alpha\beta} \dot x^\alpha \dot x^\beta = 0\), 8 first-order equations.
3. \(\Gamma\) are handwritten Schwarzschild, not computed from \(g_{\mu\nu}\) at runtime.
4. Integrator: fixed-step RK4, \(h=0.01\) in Experiment 1, `double`.
5. Affine parameter = RK4 `step_size`; proper time is zero on the ray.
6. 1D point IC: \(E=1\), \(L=b\), \(\dot\theta=0\), inbound \(\dot r\), start at \(r=S\).
7. \(b=L/E\) (1D point) is not the same object as Cartesian \((b_u,b_v)\) (2D/parallel).
8. \(b_{\mathrm{crit}} = (3\sqrt{3}/2) r_s\); photon sphere \(1.5 r_s\).
9. Experiment 1 scalar is \(\theta_E=\arctan\rho\) from the observer-**hit** geodesic, not from pixels.
10. \(\rho = \tan\theta\) is gnomonic; residual \(u(b)\) is a **root-finding** coordinate, not the plotted radius.
11. The ring *image* is `expand_angular_azimuthally`, which integrates **zero** extra geodesics.
12. Changing \(S\) changes \(r_0\), \(f\), \(\dot r\), \(\dot\phi\), and the geometric factor \(S/(D+S)\).
13. `"inf"` = `--ray-model parallel` at a finite launch plane, not \(S=\infty\).
14. Weak-field \(R_E=\sqrt{2 r_s D S/(D+S)}\) is Test 4 only; Experiment 1 does not use it; 50% band; \(D=30\) is not weak field.
15. Historical audit’s *decreasing* radius is a **retired** screen observable; current \(\theta_E\) **increases** with \(S\).
16. Penrose kernel = `sgl_physics`; SGL = geometry + rays + arrivals + imaging; geometry does not link physics.
17. Kerr tomorrow: replace \(\Gamma\), ICs, constraints, observables, horizon, chart — not RK4.
18. OpenMP over geodesics; GPU kernel would be Γ+RK4 per ray; no GPU in tree; `PhysicalConstants` unused.
19. Sampling error ≠ RK4 error ≠ Schwarzschild-vs-weak-field model error; each test bounds a different one, loosely.
20. Do not quote metres, AU, or solar arcseconds from these runs. This is a geometrized numerical laboratory, not a mission product.

---

## Contradictions and traps (read before the meeting)

1. An earlier **screen-intersection** radius decreased with \(S\). Current \(\theta_E\) **increases**. Quoting the old sequence as “our result” is wrong. See `docs/SOURCE_DISTANCE.md`.
2. **`print_usage` defaults ≠ `CliOptions` defaults** (ray-count 41 vs 801, resolution 512 vs 1024).
3. OpenMP is optional: `CMakeLists.txt` links it when found; otherwise the same source builds serially.
4. **`observer-distance`** is a perpendicular offset. **`observer-axial-distance`** is \(D\). Mixing them is a classic own-goal.
5. **On-axis “true 2D” still azimuthally fills** via `fill_aligned_observer_ring`. Do not claim that executable always images independent azimuths.
6. **`Units.h` / `PhysicalConstants.h` are not on the execution path.**

---

*Generated from the source tree as of this briefing. If the code changes, re-trace `canonical_sgl_image.cpp` `main`, `build_null_scatter`, `GeodesicDynamics`, `RK4Integrator`, `observer_angular_coordinates`, and `scripts/source_distance_test.py` before repeating any numerical claim.*

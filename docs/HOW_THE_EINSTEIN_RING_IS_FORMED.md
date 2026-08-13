# How the Einstein Ring Is Formed

A plain-language guide to the ray models and image formation used by this
project’s Schwarzschild SGL forward pipelines.

For file-level call traces and APIs, see [SGL_FORWARD_PIPELINE.md](SGL_FORWARD_PIPELINE.md).
Physics, units, validation: [TECHNICAL_BRIEFING.md](TECHNICAL_BRIEFING.md).
This document answers the conceptual questions first.

The repository has **two image paths**. They share the same Schwarzschild/RK4
kernel and the same pixel binning. They differ in how they sample launch
parameters and how they turn arrivals into a 2D ring.

| Path | Executable | Launch sampling | How the ring is filled |
|---|---|---|---|
| **1D symmetry-reduced** | `sgl_canonical_sgl_image` | 1D sweep in impact parameter `b`, one orbital plane | Rotate one observer-hit direction about the optical axis |
| **True 2D** | `sgl_true_2d_sgl_image` | Cartesian `(b_u, b_v)` launch-plane grid | On-axis: median refined \(\rho\) + azimuthal copy (`fill_aligned_observer_ring`). Off-axis: independent refined hits only; **no** azimuthal copy |

Off-axis geometry (`--observer-distance ≠ 0`) is valid only on the 2D path. The
1D executable rejects it, because azimuthal expansion assumes on-axis spherical
symmetry.

---

## Short answers

| Question | 1D path | 2D path |
|---|---|---|
| Are the rays parallel? | **Optional.** Default `--ray-model point` is a fan from one point. `--ray-model parallel` is a collimated beam. | **Always a parallel beam.** There is no `--ray-model`. Every search and refined ray starts on the launch plane `z = −S` and aims along source→lens (`+Z` when aligned). |
| Uniform random rays? | **No.** Deterministic `b` grid. | **No.** Deterministic cell-centered `(b_u, b_v)` grid. |
| What is sampled? | A **1D linear sweep in `b`**, in one equatorial plane. | A **2D launch-plane grid**. Those samples are **search geodesics**, not the ring. |
| What appears in the image? | The **observer-hit** ray’s incoming direction, copied around the axis. | Search rays are never binned. On-axis: median refined \(\rho\), then azimuthal copy. Off-axis: only the Newton-refined hits. |
| How does a 2D ring appear? | After finding one equatorial observer-hit, rotate that angle by symmetry. | Search + Newton find observer-hitting launches. On-axis the picture is then filled by symmetry; off-axis the sparse hits are imaged as-is. |
| What coordinate is imaged? | Observer-centered gnomonic `(u_ang, v_ang) = (tan θ_right, tan θ_up)`. | Same observable. |
| How is the image made? | Each 2D angular sample adds **+1** to one pixel; then **normalize by max**. | Same. |

---

## 1. The physical setup (what the light is doing)

Both experiments place a Schwarzschild lens at the origin. The optical axis is
`+Z`.

```text
  launch plane / source         Schwarzschild lens          observer / image plane
      ●  --------------------→  ●  --------------------→  ▯
   (0, 0, −S)                (0, 0, 0)                 (D·Z + d·X)
```

- `S` (`--source-distance`) is the axial distance from the lens to the launch
  plane (and, in the 1D point model, the point-source location).
- `D` (`--observer-axial-distance`) is how far the observer sits along `+Z`.
- `d` (`--observer-distance`) is a perpendicular offset along `+X`. `d = 0` is
  on-axis (a ring). `d ≠ 0` is off-axis (arcs). Only the 2D path can image that.

Light is treated as **geometric-optics rays**: null geodesics in Schwarzschild
spacetime. There is no wave optics, diffraction, or detector PSF.

When source, lens, and observer are aligned, the set of paths that **reach the
observer** forms an Einstein ring. Crossing the observer’s **plane** is not the
same as hitting the observer. Both pipelines image only observer-hitting rays
(after a 1D root find or a 2D Newton refine).

---

## 2. How rays are modelled

### 1D path: two launch models (`--ray-model`)

| Model | CLI | What it does |
|---|---|---|
| Point source (default) | `--ray-model point` | All rays start at one point and fan out with different impact parameters `b` |
| Parallel beam | `--ray-model parallel` | Rays start on the launch plane with offsets `(b, 0, −S)`, all aimed along `+Z` |

#### Point source (`point`)

Every ray:

1. Starts at the **same point** — the source position `(0, 0, −S)`.
2. Is launched toward the lens with a chosen **impact parameter** `b` (angular momentum).
3. Is integrated as a **curved null geodesic** via `build_null_scatter`.

This is a **point-source spray**, not a grid of parallel lines.

#### Parallel rays (`parallel`)

Every ray:

1. Starts on the launch plane `z = −S` at world position `(b, 0, −S)`.
2. Is aimed along **+Z** — same direction for all rays.
3. Is converted to a Schwarzschild null geodesic with `build_custom` (`vt` filled by the null constraint).

`--source-distance` is then the **launch-plane location**, not a point-source range.
There is no true `S = ∞`; a large `S` (or the sweep token `"inf"`, which switches
to `--ray-model parallel`) is the finite-plane stand-in for source-at-infinity.

### 2D path: always a parallel launch-plane beam

`RayGrid2DSampler` does **not** fan rays from one point. For launch offsets
`(b_u, b_v)`:

1. Start at `source.position + b_u·X̂ + b_v·Ŷ` = `(b_u, b_v, −S)` when aligned.
2. Aim every ray in the **same** direction: `normalize(lens − source)` = `+Z`.
3. Convert Cartesian chart position/direction to spherical coordinates and call
   `build_custom` with `vt = 0` so the null constraint fills energy.

That is the 2D analogue of 1D `--ray-model parallel`, with a square grid instead
of a 1D `b` line. The 2D CLI has no `--ray-model` and no point-source fan.

### Not random

There is **no Monte Carlo**.

**1D** impact parameters:

```text
b_i = b_min + (i / (N − 1)) · (b_max − b_min)     for i = 0 … N−1
```

Canonical defaults: `N = ray_count = 801`, `b_min = 2.0`, `b_max = 20.0`
(geometrized units, `rs = 1`). Used to scan `residual_u(b)` for observer-hit roots.

**2D** cell-centered grid on `[−b_max, +b_max]²`:

```text
cell = 2·b_max / N
b_i = −b_max + (i + 0.5)·cell     for i = 0 … N−1
```

Row-major: `b_v` outer, `b_u` inner. Same inputs always produce the same rays
and the same image.

### What “impact parameter” means here

**1D point model:** `build_null_scatter` builds a null geodesic with energy-like
scale `E = 1`, angular momentum `L = b·E`, inward radial velocity, and
`v_θ = 0`. `b` is how far the asymptotic straight-line aim would miss the lens
center.

**1D/2D parallel models:** `b` (or `(b_u, b_v)`) is a **transverse launch-plane
offset**, not an angular-momentum label assigned at a single point.

### Only one plane is integrated (1D only)

All 1D integrated rays live in a **single equatorial orbital plane**. After
propagation, arrivals on the image plane lie on a **line** through the origin —
typically `(u, 0)`, not a filled ring.

```text
Image plane after 1D integration only:

        v
        ▲
        │
  ──────●──●──●──●──●──────► u     ← hits from different b
        │
```

The 1D ring is filled later by symmetry, not by more geodesics. The 2D path
does **not** do this: it integrates many launch-plane azimuths for real.

---

## 3. What happens during integration (both paths)

For each geodesic (search ray, 1D scan ray, or Newton trial):

1. Start from a null initial state.
2. Advance with **fixed-step RK4** along the Schwarzschild geodesic equations.
3. Stop at the **first crossing** of the observer image plane
   (or near-horizon / max steps). The plane is treated as **unbounded** for
   termination; `ImagePlane::contains` is not used here.
4. Record a `RayArrival`: world hit, incoming direction, status.

No full trajectory is kept for imaging — only the localized arrival between the
last two integration states.

Independent geodesics in an ensemble run concurrently under OpenMP
(`Rays::propagate_ensemble`, `schedule(dynamic)`). Thread count is
`OMP_NUM_THREADS` (not a C++ CLI flag). A 1-ray ensemble, used inside Newton,
stays serial.

---

## 4. Search geodesics vs the rays that form the ring (2D)

This is the most common confusion on the 2D path.

### Search geodesics are a survey, not the picture

`--samples-per-axis N` launches an `N × N` Cartesian grid of parallel rays.
Those **search geodesics** exist to map launch-parameter space: for each
`(b_u, b_v)`, how far from the observer did the ray land on the observer plane?

They are **not** an imaging aperture and **not** the Einstein ring.

- Most search rays miss the observer by a large margin.
- Many still **arrive** in the sense of crossing the infinite observer **plane**.
  That is `arrived_count` in `run_summary.txt`. Crossing the plane anywhere is
  cheap; it does not mean the photon hit the spacecraft.
- Search rays are **never** passed to `form_image`.

A 5×5 run is 25 search geodesics. An 11×11 run is 121. Runtime is dominated by
this grid (plus later Newton). Newton typically returns only a handful of
observer-hitting samples; on-axis the PGM is then filled to `azimuth_count`
points, while off-axis those few hits are the image.

### Plane residual

For an arrival, the observer-plane residual is

```text
r(b_u, b_v) = image_plane.to_plane_coordinates(world_hit)
```

The observer sits at the plane origin, so a true hit is `‖r‖ = 0` (in practice
`≤ --observer-hit-tolerance`, default `1e-6`). A search ray with a small `‖r‖`
is a near miss. A search ray with a large `‖r‖` is useless as an image sample
even if it “arrived.”

### Seeds are starting guesses

`observer_hit_seeds` looks at the search-grid residuals and proposes
`(b_u, b_v)` starting points for Newton:

1. The global best (smallest residual) search sample.
2. Every 8-neighbor **local minimum** of residual norm on the grid.
3. **Edge interpolations** along neighboring grid pairs whose residual segment
   points toward the origin (a 1D estimate of a zero crossing between cells).

Seeds are still grid-scale guesses. They are not required to hit the observer.
`seed_count` is this list, not the number of ring pixels.

On a coarse grid (3×3) there may be **no** useful seeds and **no** image. On
5×5, typical on-axis numbers are ~14 seeds and **2** refined hits. On 11×11,
~24 seeds and **10** refined hits. Denser grids find more distinct launch-plane
azimuths around the ring.

### Refined observer hits are the actual ring rays

Each seed is handed to `refine_launch_to_observer`: a damped Gauss–Newton
iteration on `(b_u, b_v)`, with a finite-difference Jacobian on the first step
and Broyden updates afterward. Newton **inside** one seed is serial (each trial
is one geodesic). Independent seeds run concurrently under OpenMP.

A seed becomes a **refined observer hit** only if some launch in that sequence
satisfies `‖r‖ ≤ 1e-6`. That geodesic **does** pass through the observer. Its
incoming world direction is mapped to gnomonic angular coordinates
`(u_ang, v_ang)` and **that** point is what gets binned.

Failed seeds are discarded. Nearby successes that land within a fraction of a
grid cell of each other are deduplicated. The sort key is
`(residual norm, seed index)` so the kept set is deterministic under
multithreading.

```text
Search grid (many geodesics)
    │
    │  residual = how far the plane hit missed the observer
    ▼
Seeds (local near-misses)
    │
    │  Gauss–Newton on (b_u, b_v), still real geodesics
    ▼
Refined observer hits  ← these set ρ
    │
    │  incoming direction → (u_ang, v_ang)
    │  on-axis: fill_aligned_observer_ring
    ▼
form_image
```

### Why not just image the search rays?

A search ray that misses the observer by milliradians is a **different photon
path**. Binning it would paint a thick fuzzy annulus of “near the spacecraft”
instead of the geometric-optics image of rays that reach the observer. The
finite-aperture idea (keep search rays within some hit radius) was tried and
replaced by this refinement. Off-axis, the image is those observer-hitting
launches. On-axis, their median \(\rho\) is then copied around the axis. Neither
case is a blurred snapshot of the search grid.

### What the summary counts mean

| Field | Meaning | In the image? |
|---|---|---|
| `rays_sampled` | Search-grid geodesics (`N²`) | No |
| `arrived_count` | Search rays that crossed the observer **plane** | No |
| `seed_count` | Newton starting guesses | No |
| `refined_observer_hits` | Launches tuned to the observer **point** | Determine \(\rho\); on-axis PGM is then a symmetry fill |

The 1D analogue of search-vs-ring is the `b` scan vs the **one** bisection root
that is then rotated. The 1D scan also does not paint the ring; only the
refined observer-hit direction does.

---

## 5. From observer-hit direction to a ring

### Shared angular observable

Both paths convert a refined arrival’s incoming direction to **gnomonic angular
coordinates** (`Arrivals::observer_angular_coordinates`):

```text
s = −normalize(world_direction)          # sky direction toward the source
u_ang = dot(s, right) / dot(s, forward)
v_ang = dot(s, up)    / dot(s, forward)
```

The true angular radius is `θ = atan(sqrt(u_ang² + v_ang²))`. For small angles,
`sqrt(u_ang² + v_ang²) ≈ θ` in radians. An equivalent screen radius is
`R_equiv = D · sqrt(u_ang² + v_ang²)` where `D` is the observer axial distance.

### 1D: one hit, then rotate

1. Scan `residual_u(b) = image_plane.to_plane_coordinates(arrival).x()` over the `b` grid.
2. Find zero-crossing brackets; bisect; select the **primary/direct** branch
   (smallest positive angular radius).
3. Expand the signed equatorial angular coordinate azimuthally (**on-axis only**):

```text
For the selected signed u_ang:

    for k = 0 … N_az − 1:
        ψ = 2π k / N_az
        emit (u_ang cos ψ,  u_ang sin ψ)
```

Canonical default: `azimuth_count = 720`.

```text
After angular azimuthal expansion:

        v_ang
        ▲
        │    ● ●
        │  ●     ●
  ──────●─────────●────► u_ang
        │  ●     ●
        │    ● ●
```

This is a **symmetry reconstruction** on the observer sky, not a claim that 720
independent geodesics were flown at every azimuth. That is why 1D cannot do
off-axis observers: the ring would not stay a rotated copy of one equatorial hit.

### 2D: refined hits, then on-axis fill

Each refined observer hit has its own `(u_ang, v_ang)` from a geodesic launched
at a distinct `(b_u, b_v)`. Search geodesics are never binned.

**On-axis** (`observer-distance == 0`): `fill_aligned_observer_ring` replaces
those hits with the median \(\rho\) and calls `expand_angular_azimuthally`
(`azimuth_count`, default 720). The on-axis 2D *picture* is therefore a
symmetry fill, like 1D. The Newton hits still determine \(\rho\).

**Off-axis**: that fill is skipped. The image is the sparse set of real
refined hits. A 5×5 off-axis run can show only two spots: two images of a
point source as seen by a point observer.

That is why an on-axis 2D image can look like a continuous ring even at
`samples-per-axis = 5`, while off-axis cannot: on-axis **drew** the circle
after Newton; off-axis **sampled** isolated roots.

---

## 6. How the image is formed after that

Image formation is the same for both paths: **count samples in pixels**.

### Step A — empty grid

`form_image` creates a square `Image` covering gnomonic coordinates

```text
u_ang, v_ang ∈ [−extent/2, +extent/2]
```

1D default: `extent = 0.8`, resolution `1024 × 1024`.
2D default: `extent = 0.8`, resolution `64 × 64` (raise `--resolution` to match 1D).

### Step B — map each sample to a pixel

```text
x = floor( (u − u_min) / du )
y = floor( (v − v_min) / dv )
```

Half-open bounds: `u_max` and `v_max` are **out of bounds**.
Samples outside the rectangle are ignored.

### Step C — accumulate

```text
intensity[x, y] += 1
```

Every in-bounds angular sample contributes weight **1**.
No magnification weighting, flux calibration, PSF, or blur.

### Step D — normalize

```text
I_normalized = I / max(I)
```

(If the image is all zeros, it stays all zeros.)

### Step E — write outputs

**1D** (`sgl_canonical_sgl_image`): `einstein_ring.csv`, `einstein_ring.pgm`,
`run_summary.txt`.

**2D** (`sgl_true_2d_sgl_image`): `true_2d_image.csv`, `true_2d_image.pgm`,
`run_summary.txt` (includes `rays_sampled`, `arrived_count`, `seed_count`,
`refined_observer_hits`, `median_angular_radius`, `radial_stddev`).

The **ring you see** is places where angular samples landed, after
max-normalization so the brightest pixels are white.

---

## 7. End-to-end pictures

### 1D symmetry-reduced

```text
Point source  or  parallel beam (1D b sweep)
    │
    │  scan residual_u(b) for observer-hit root
    ▼
Refined observer-hit null geodesic  (one equatorial direction)
    │
    │  incoming direction → gnomonic (u_ang, v_ang)
    ▼
One signed angular coordinate
    │
    │  rotate about optical axis (azimuth_count angles)
    ▼
Circle in angular coordinates
    │
    │  bin: intensity += 1; divide by max
    ▼
Einstein-ring image
```

### True 2D

```text
Parallel beam on launch plane (b_u, b_v) grid     ← search geodesics
    │
    │  integrate; residual = miss distance on observer plane
    ▼
Seeds (local minima / edge interpolations)
    │
    │  Gauss–Newton until ‖residual‖ ≤ 1e-6
    ▼
Refined observer-hitting geodesics
    │
    │  each incoming direction → (u_ang, v_ang)
    │  on-axis: fill_aligned_observer_ring (median ρ + azimuthal copy)
    │  off-axis: no azimuthal expansion
    ▼
Circle (on-axis) or sparse sky samples (off-axis)
    │
    │  bin: intensity += 1; divide by max
    ▼
Einstein-ring image
```

---

## 8. Mental model checklist

1. **Two paths:** 1D integrates one plane and rotates; 2D integrates a launch-plane
   grid, keeps observer hits, then on-axis only fills a circle from median \(\rho\).
2. **2D search geodesics are not the ring.** They survey `(b_u, b_v)`. Newton
   finds observer hits. On-axis the image is then a symmetry fill of median \(\rho\);
   off-axis the refined hits are binned as-is.
3. **Plane crossing ≠ observer hit.** `arrived_count` is the former;
   `refined_observer_hits` is the latter.
4. **1D default source is a point fan;** 1D `--ray-model parallel` and **all 2D
   rays** are collimated beams on a finite launch plane. Neither is a true
   mathematical source at infinity.
5. **Pixels:** unweighted counts on gnomonic angular coordinates, then normalize
   by the brightest pixel.
6. **Threads:** OpenMP on independent geodesics and independent Newton seeds.
   Set `OMP_NUM_THREADS` or `parameter_sweep.py --threads N`. No C++ `--threads`
   flag.

---

## 9. What this is *not*

| It is not… | Because… |
|---|---|
| Parallel-ray lensing from infinity on the 1D default | 1D default is `--ray-model point`. Use `parallel`, or use the 2D executable (already parallel). |
| Imaging the 2D search grid | Search rays miss the observer; only refined hits are binned. |
| Random / Monte Carlo ray tracing | `b` and `(b_u, b_v)` samples are fixed grids. |
| Full 2D geodesic sampling on the **1D** path | 1D integrates one orbital plane and rotates. |
| A radiometric / CCD image | Unit counts + max-normalization only. |
| Wave optics / diffraction | Geometric optics only. |
| Off-axis 1D rings | `sgl_canonical_sgl_image` rejects `--observer-distance ≠ 0`. Use `sgl_true_2d_sgl_image`. |

---

## 10. Where to look in the code

| Step | 1D | 2D |
|---|---|---|
| Launch sampling | `experiments/canonical_sgl_image.cpp`, `physics/rays/RaySampler.cpp` | `physics/rays/RayGrid2DSampler.cpp` |
| Null initial state | `build_null_scatter` or `make_parallel_null_state` | `RayGrid2DSampler::state_for` → `build_custom` |
| Integration + plane hit | `physics/arrivals/ArrivalCollector.cpp`, `physics/rays/EnsemblePropagator.cpp` | Same kernel |
| Observer-hit solve | bisection on `residual_u(b)` in the 1D executable | `physics/arrivals/ObserverLaunchRefiner.cpp` |
| Angular coordinates | `physics/arrivals/ObserverAngularCoordinates.cpp` | Same |
| Ring fill | `expand_angular_azimuthally` | On-axis: `fill_aligned_observer_ring`; off-axis: refined hits only |
| Pixel binning | `physics/imaging/ImageFormation.cpp` | Same |
| Runnable experiment | `experiments/canonical_sgl_image.cpp` | `experiments/true_2d_sgl_image.cpp` |
| Sweep orchestrator | `experiments/parameter_sweep.py` (auto-selects 2D when sweeping `observer-distance`) | Same |

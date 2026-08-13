# How the Einstein Ring Is Formed

A plain-language guide to the ray model and image formation used by this project’s
canonical SGL forward pipeline.

For file-level call traces and APIs, see [SGL_FORWARD_PIPELINE.md](SGL_FORWARD_PIPELINE.md).
This document answers the conceptual questions first.

---

## Short answers

| Question | Answer in this codebase |
|---|---|
| Are the rays parallel? | **Optional.** Default is a **point-source fan**. Use `--ray-model parallel` for a parallel beam. |
| Uniform random rays? | **No.** Sampling is **deterministic**, not random. |
| What is sampled? | A **1D linear sweep in impact parameter** `b`, in one orbital plane. |
| How does a 2D ring appear? | After finding the **observer-hit ray**, its angular direction is **rotated about the optical axis** by symmetry. |
| What coordinate is imaged? | **Observer-centered gnomonic angular coordinates** `(u_ang, v_ang) = (tan θ_right, tan θ_up)`, not screen-plane hit positions. |
| How is the image made? | Each 2D angular sample adds **+1** to one pixel; the grid is then **normalized by its max**. |

---

## 1. The physical setup (what the light is doing)

The canonical experiment places three things on one straight line (the optical axis):

```text
  point source          Schwarzschild lens          observer / image plane
      ●  ----------------→  ●  ----------------→  ▯
   (0, 0, −S)            (0, 0, 0)               (0, 0, +D)
```

Light leaves the source, is bent by the lens’s gravity, and may cross the observer’s
image plane. When source, lens, and observer are perfectly aligned, the set of paths
that reach the observer forms an **Einstein ring** — a bright annulus around the axis.

In this code, light is treated as **geometric-optics rays**: null geodesics in
Schwarzschild spacetime. There is no wave optics, diffraction, or detector PSF.

---

## 2. How rays are modelled

The canonical executable supports two launch models via `--ray-model`:

| Model | CLI | What it does |
|---|---|---|
| Point source (default) | `--ray-model point` | All rays start at one point and fan out with different impact parameters |
| Parallel beam | `--ray-model parallel` | Rays start on a launch plane with different offsets `b`, all aimed the same way |

### Point source (`point`)

Every ray:

1. Starts at the **same point** — the source position `(0, 0, −S)`.
2. Is launched toward the lens with a chosen **impact parameter** `b` (angular momentum).
3. Is integrated as a **curved null geodesic**.

This is a **point-source spray**, not a grid of parallel lines.

### Parallel rays (`parallel`)

Every ray:

1. Starts on the launch plane `z = −S` at world position `(b, 0, −S)`.
2. Is aimed along **+Z** (toward the lens / observer) — same direction for all rays.
3. Is converted to a Schwarzschild null geodesic and integrated.

So the family is a **parallel beam** parameterized by transverse offset `b`.
`--source-distance` still sets the launch-plane distance `S`.

### Not random

There is **no Monte Carlo / random sampling**.

Impact parameters use a **fixed linear grid**:

```text
b_i = b_min + (i / (N − 1)) · (b_max − b_min)     for i = 0 … N−1
```

Canonical defaults (see `experiments/canonical_sgl_image.cpp`):

- `N = ray_count = 801` (used to scan `residual_u(b)` for observer-hit roots)
- `b_min = 2.0`, `b_max = 20.0` (in geometrized units with Schwarzschild radius `rs = 1`)

Same inputs always produce the same rays and the same image.

### What “impact parameter” means here

**Point model:** for each sample, `build_null_scatter` builds a null geodesic state with:

- energy-like scale `E = 1`
- angular momentum `L = b · E`
- radial velocity aimed **inward** toward the lens
- **zero** polar velocity (`v_θ = 0`)

So `b` controls how much angular momentum the ray carries — loosely, how far the
asymptotic straight-line aim would miss the lens center. Different `b` values bend
by different amounts and hit the observer plane at different radii.

### Only one plane is integrated

All integrated rays live in a **single equatorial orbital plane** of the Schwarzschild
chart. The code enforces that by construction:

- the aligned source sits on the optical axis → launch at chart latitude `θ = π/2`
- every initial state sets `v_θ = 0`

So after propagation, arrivals on the image plane lie on a **line** through the
plane origin — typically coordinates of the form `(u, 0)`, not a filled ring yet.

```text
Image plane after integration only:

        v
        ▲
        │
  ──────●──●──●──●──●──────► u     ← hits from different b
        │
```

That is intentional: the next step fills the circle using symmetry, not more geodesics.

---

## 3. What happens during integration (briefly)

For each sampled ray:

1. Start from the null initial state at the source.
2. Advance with **fixed-step RK4** along the geodesic equations
   (Schwarzschild Christoffel symbols).
3. Stop at the **first crossing** of the observer image plane
   (or if the ray hits the near-horizon bound / runs out of steps).
4. Record a `RayArrival`: where it hit in world space, and whether it arrived.

No full trajectory is kept for imaging — only the arrival (localized between the
last two integration states).

---

## 4. From one observer-hit direction to a ring

The canonical image is **not** formed from every plane-crossing position. Instead:

1. Scan `residual_u(b) = image_plane.to_plane_coordinates(arrival).x()` over the `b` grid.
2. Find zero-crossing brackets where the ray crosses the observer point on the plane.
3. Refine each candidate with bisection; select the **primary/direct** branch (smallest positive angular radius).
4. Convert the refined arrival’s incoming direction to **gnomonic angular coordinates**:

```text
s = −normalize(world_direction)          # sky direction toward source
u_ang = dot(s, right) / dot(s, forward)
v_ang = dot(s, up)    / dot(s, forward)
```

5. Expand the signed equatorial angular coordinate azimuthally (on-axis only):

```text
For the selected signed u_ang:

    for k = 0 … N_az − 1:
        ψ = 2π k / N_az
        emit (u_ang cos ψ,  u_ang sin ψ)
```

Canonical default: `azimuth_count = 720`.

The true angular radius is `θ = atan(sqrt(u_ang² + v_ang²))`. For small angles,
`sqrt(u_ang² + v_ang²) ≈ θ` in radians. The equivalent screen radius is
`R_equiv = D · sqrt(u_ang² + v_ang²)` where `D` is the observer axial distance.

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
independent geodesics were flown at every azimuth.

---

## 5. How the image is formed after that

Image formation is deliberately simple: **count rays in pixels**.

### Step A — build an empty grid

`form_image` creates a square `Image` covering **angular tangent-plane coordinates**

```text
u_ang, v_ang ∈ [−extent/2, +extent/2]
```

Canonical: `extent = 0.8` (dimensionless), resolution `1024 × 1024`.

Each pixel stores a scalar intensity (initially 0).

### Step B — map each 2D arrival to a pixel

For a point `(u_ang, v_ang)`:

```text
x = floor( (u − u_min) / du )
y = floor( (v − v_min) / dv )
```

with half-open bounds: `u_max` and `v_max` are **out of bounds**.
Arrivals outside the image rectangle are ignored.

### Step C — accumulate

```text
intensity[x, y] += 1
```

Every in-bounds angular sample contributes the same weight **1**.
There is no magnification weighting, flux calibration, PSF, or blur.

### Step D — normalize

```text
I_normalized = I / max(I)
```

(If the image is all zeros, it stays all zeros.)

### Step E — write outputs

The canonical executable writes:

- `einstein_ring.csv` — scientific normalized grid (+ metadata comments)
- `einstein_ring.pgm` — gray-scale picture of the same numbers (for viewing)
- `run_summary.txt` — counts and settings

The **ring you see** is therefore: places where many symmetry-expanded arrivals
landed, after max-normalization so the brightest pixels are white.

---

## 6. End-to-end picture

```text
Point source
    │
    │  scan impact parameter b for observer-hit root (residual_u = 0)
    ▼
Refined observer-hit null geodesic
    │
    │  map incoming direction → gnomonic angular (u_ang, v_ang)
    ▼
One equatorial angular coordinate
    │
    │  rotate about optical axis (azimuth_count angles)
    ▼
Many points on CIRCLE in angular coordinates → annular cloud
    │
    │  bin into pixels: intensity += 1
    │  divide by max
    ▼
Einstein-ring image (CSV + PGM)
```

---

## 7. Mental model checklist

If you remember only this:

1. **Source model:** default is one point source; optional `--ray-model parallel` uses a parallel beam on the launch plane.
2. **Sampling:** uniform **in impact parameter** on a 1D grid for root bracketing (even spacing in `b`), deterministic.
3. **Propagation:** real curved null geodesics; the canonical image uses the **observer-hit** ray’s incoming direction.
4. **Ring fill:** spherical symmetry → rotate one angular coordinate into a circle (on-axis only).
5. **Pixels:** unweighted ray counts on angular coordinates, then normalize by the brightest pixel.

That is the entire underlying process behind the ring in the current pipeline.

---

## 8. What this is *not*

To avoid common misunderstandings:

| It is not… | Because… |
|---|---|
| Parallel-ray lensing from infinity (by default) | Default `--ray-model point` starts all rays at one point; use `parallel` for a finite launch-plane beam |
| Random / Monte Carlo ray tracing | `b` samples are a fixed linear grid |
| Full 2D geodesic sampling on the sky | Only one orbital plane is integrated |
| A radiometric / CCD image | Unit counts + max-normalization only |
| Wave optics / diffraction | Geometric optics only |

---

## 9. Where to look in the code

| Step | Primary location |
|---|---|
| Impact-parameter grid / root scan | `experiments/canonical_sgl_image.cpp` |
| Null initial state from `b` | `physics/schwarzschild/InitialStates.cpp` (`build_null_scatter`) |
| Integration + plane hit | `physics/arrivals/ArrivalCollector.cpp` |
| Angular coordinates | `physics/arrivals/ObserverAngularCoordinates.cpp` |
| Azimuthal angular expansion | `physics/arrivals/ObserverAngularCoordinates.cpp` |
| Pixel binning | `physics/imaging/ImageFormation.cpp` |
| Runnable experiment | `experiments/canonical_sgl_image.cpp` |

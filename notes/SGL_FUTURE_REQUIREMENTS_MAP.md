# SGL Future Requirements Map

**Phase 4 artifact. Requirements and architectural-stress definition — not a design.**

This document defines the long-term target that the SGL architecture must eventually
survive. It does not propose an architecture, does not prescribe implementation, and
does not assume the current architecture is capable of any of it.

Its purpose is to be the fixed reference against which the current implementation is
later evaluated.

Prior phases:

- Phase 1 — `notes/ARCHITECTURE_RECONSTRUCTION.md` (repository reconnaissance)
- Phase 2 — `notes/SGL_ARCHITECTURE_RECONSTRUCTION.md` (architectural reconstruction)
- Phase 3 — `notes/SGL_ARCHITECTURAL_STRESS_TEST.md` (adversarial stress test)

### Support vocabulary used throughout

Every "current architectural support" judgement in this document uses one of five
levels, all grounded in the implementation as verified in Phases 1–3:

| Level | Meaning |
|---|---|
| **Present** | A working, wired implementation exists today. |
| **Partial** | A usable seam or fragment exists, but not through the public path, or not completely. |
| **Nominal** | A name or type exists but carries no working capability (single-valued enum, unused struct, uncalled function). |
| **Absent** | Nothing in the repository addresses it. |
| **Contradicted** | The current implementation encodes an assumption that actively conflicts with the requirement. |

`Contradicted` is the important category. `Absent` means work remains; `Contradicted`
means existing work points the other way.

---

## 1. Long-Term Scientific Identity

### What SGL must eventually be

A research framework for computational study of the Solar Gravitational Lens
observation problem, supporting increasing physical and numerical fidelity over a
multi-year horizon.

The defining property is **research extensibility**, not feature count. The framework
succeeds if researchers can pose new scientific questions — new models, new numerical
methods, new experiments, new observables — without restructuring the framework each
time.

### What this identity requires architecturally

1. **The scientific question must be expressible independently of how it is computed.**
   A researcher stating "propagate light from this source past the Sun to this observer
   at this wavelength" must be able to state it without committing to an integrator, a
   step size, a coordinate chart, a memory layout, or an execution device.

2. **The framework must own more than one kind of scientific output.** Trajectories,
   images, intensity fields, convergence tables, and validation reports are all
   first-class scientific results in the target domain.

3. **The framework must support comparison as a native activity.** Comparing two
   integrators, two physical models, two resolutions, or two independent implementations
   of the same prediction is a core research workflow, not an afterthought.

4. **The scientific boundary must stay explicit.** SGL is a physics and observation
   simulation framework. Mission engineering, spacecraft design, and operations planning
   may consume it, but the boundary between "scientific simulation" and "mission
   engineering" must remain legible.

### Identity gap to be evaluated later

Phase 2 established that the current implementation is a **single-trajectory
Schwarzschild geodesic propagation kernel with one convenience API and one smoke test**.
That is a legitimate Stage 1 foundation. It is not yet the identity described above.
The gap is not a defect at this stage; it is the distance this document is measuring.

---

## 2. Scientific Evolution

Eight conceptual stages. Later stages do not replace earlier ones — they must coexist,
because Stage 1 propagation remains the innermost kernel of every later stage.

### Stage 1 — Relativistic Propagation

**Scientific motivation.** Nothing downstream is trustworthy unless photon propagation
through the relevant gravitational field is numerically accurate and demonstrably so.

**Requirements.**

- Photon (null) and massive-particle (timelike) geodesic propagation.
- Initial conditions derived from physically meaningful quantities, satisfying the
  relevant normalization constraint.
- Trajectory generation with controllable resolution.
- Numerical integration decoupled from the physical model.
- Conservation and constraint-violation diagnostics as routine output.
- Comparison against closed-form analytical results where they exist.

**Required data concepts.** Geodesic state; affine parameterization; conserved
quantities; constraint residual.

**Required computational concepts.** ODE right-hand side; state advancement; stopping
condition; error/violation measurement.

**Current support: Present (with one gap).** Verified working: Schwarzschild Christoffel
symbols, geodesic dynamics, RK4 integration, horizon termination, four initial-condition
builders, and conserved-quantity observables. The smoke test measures relative drift of
`1.01e-14` in energy and `3.73e-15` in angular momentum over 50 000 steps.

The gap is **analytical validation**, which is `Absent`. Penrose carried
`analytical_freefall_time` (a closed-form proper-time reference); the extraction dropped
it. SGL currently has conservation checks — which detect drift — but no closed-form
reference solution, which is what detects a *systematically wrong but well-conserved*
answer. Those are different classes of error.

**Assumptions that may become invalid.** Affine parameterization is implicit and
unnamed; the derivative is autonomous with no context argument; the trajectory is always
fully retained.

---

### Stage 2 — Solar Gravitational Lens Geometry

**Scientific motivation.** SGL is defined by a three-body relationship — source, lens,
observer — and by the focal geometry beyond ~550 AU. Until that relationship is
representable, the framework cannot state the problem it exists to study.

**Requirements.**

- The Sun as a named lens entity with physical parameters, not an anonymous `rs` scalar.
- Source geometry with position and orientation.
- Observer geometry with position, and eventually velocity.
- Impact parameter as a derived relationship between source, lens, and observer — not as
  a free input the caller must precompute.
- Focal-region geometry, including the focal line rather than a focal point.
- The ability to ask connection questions: *which* photon paths link this source to this
  observer.

**Required data concepts.** Lens body; source geometry; observer geometry; observation
geometry (the source–lens–observer triple); impact parameter as derived quantity; focal
region.

**Required computational concepts.** Geometry construction; ray aiming toward a target;
boundary-crossing and intersection detection; connection-finding (a boundary-value
problem, structurally different from Stage 1's initial-value problem).

**Current support: Absent, with two `Nominal` fragments.**

- `NullScatterInitialConditions::impact_parameter` and
  `Physics::Observables::critical_impact_parameter(rs)` exist, so the *quantity* is
  represented — but as a caller-supplied input, not as a derived source–lens–observer
  relationship.
- `Constants::solar_radius_m = 6.957e8` exists and is never used. It is the only
  solar-specific value in the codebase.
- There is no source, no observer, no lens body, and no focal region. Grep-verified:
  the strings `Observer`, `source`, and `detector` do not appear as concepts anywhere in
  `physics/`. Penrose had `shared/observer/Observer.h`; the extraction did not take it.

**Assumptions that may become invalid.**

- **Initial-value framing.** Every current builder answers "given a starting state, where
  does the photon go?" SGL's actual question is frequently "given a source and an
  observer, which photon connects them?" That is a two-point boundary-value problem. No
  current abstraction expresses it, and the solver interface has no place for it.
- **Static geometry.** Nothing in the current model has a notion of geometry that changes.
- **Lens as scalar.** The lens is one `double` today.

---

### Stage 3 — Ray Ensembles

**Scientific motivation.** Single rays answer no observational question. Magnification,
image structure, and signal all emerge from populations of rays.

**Requirements.**

- Collections of rays as a first-class concept, from 10² to 10⁸ and beyond.
- Bundles of *nearby* rays where the relationship between neighbours carries physical
  meaning (beam convergence, area distortion, magnification).
- Sampling strategies over source area, impact parameter, or observer aperture, with
  those strategies being replaceable and recordable.
- Parameterized ray families.

**The separation this stage demands.** Three concerns that must be independently
variable:

```text
physical propagation   ←  what the photon does
how many rays          ←  ensemble size and sampling
storage and processing ←  what is retained and where it goes
```

**Required data concepts.** Ray; ensemble; bundle (with neighbour relationships);
sampling distribution; per-ray provenance linking a result back to its sample.

**Required computational concepts.** Batch propagation; sampling; reduction and
aggregation; independence-vs-coupling distinction between rays.

**Current support: Contradicted.**

- The public entry point returns exactly one `SimulationResult` for exactly one
  trajectory. Scaling by looping over `run_simulation` would allocate one
  `std::vector<State>` per ray.
- `TrajectorySolver::propagate` — history-free, returning only the final state — is the
  one component shaped for ensembles. It is not wired into the public API and has no
  caller. Its comment names the intended use: *"optics / image-formation use case."*
- Bundles are contradicted more deeply than ensembles: `State` has no representation for
  neighbour relationships or beam cross-section, so a bundle cannot be distinguished from
  an unordered collection of independent rays.

**Assumptions that may become invalid.** One ray per call; full history always retained;
result owns its states in process memory; serial iteration.

---

### Stage 4 — Image Formation

**Scientific motivation.** The framework must eventually answer *"what does the observer
measure?"* rather than *"where does this photon go?"*

**Requirements.**

- Image formation from propagated ray populations.
- Observer-plane sampling.
- Intensity distributions and surface-brightness accounting.
- Magnification, including the strong amplification that motivates SGL.
- Angular distributions.
- Point-spread functions.
- Image reconstruction and deconvolution as downstream analysis.

**Required data concepts.** Image plane; pixel/sample grid; intensity field; angular
coordinates; magnification map; PSF; reconstruction result.

**Required computational concepts.** Ray-to-pixel accumulation; flux conservation;
integration over source and aperture; convolution; inversion.

**Current support: Contradicted.**

This is the sharpest contradiction in the document. The current architecture's central
invariant — established in Phase 2 and confirmed by the graph, where `State` is the
highest-degree node — is:

```text
State  →  TrajectorySolver::solve  →  std::vector<State>  →  SimulationResult
```

Image formation requires the fundamental output of a computation to be an **image or
intensity field**, produced by aggregating over a population. The current model produces
a **path of one particle**. These are not the same shape, and the second is not a
special case of the first.

**Assumptions that may become invalid.** That `SimulationResult` is the universal output
type; that a result corresponds one-to-one with a run; that trajectory history is the
scientifically interesting artifact rather than an intermediate.

---

### Stage 5 — Optical / Instrument Modeling

**Scientific motivation.** A measurement is produced by an instrument, not by a
mathematical plane. Predicting observability requires modeling what the instrument does
to the incident field.

**Requirements.**

- Telescope and instrument geometry, including aperture and coronagraph-like occulting.
- Detector plane with finite resolution and pixel response.
- Sampling and quantization.
- Observational coordinate systems distinct from simulation coordinates.
- Signal formation, including noise sources where scientifically relevant.
- Instrument response functions.

**Critical requirement.** These must remain **conceptually distinct from relativistic
propagation**. Instrument parameters must not become propagation parameters. A change of
detector must not require touching the geodesic solver.

**Required data concepts.** Instrument; aperture; detector; pixel grid; response
function; observational coordinate frame; signal; noise model.

**Required computational concepts.** Field-to-signal transformation; sampling and
binning; response convolution; coordinate transformation between simulation and
observational frames.

**Current support: Absent.** No instrument concept of any kind exists. There is also no
observational coordinate frame — the only chart is
`CoordinateChartKind::SchwarzschildSpherical`, a single-valued enum.

**Assumptions that may become invalid.** That simulation coordinates are the only
coordinates that matter; that output needs no observational frame; that
`SimulationConfig` is the natural home for any new parameter (adding instrument fields
there would fuse instrument modeling to propagation configuration, violating the
separation this stage requires).

---

### Stage 6 — Extended Sources

**Scientific motivation.** The scientific payoff of SGL is resolving surface features on
an exoplanet. A point source cannot express the problem.

**Requirements.**

- Finite source size.
- Spatial brightness distributions across a source.
- Extended planetary surfaces, including surface maps.
- Source orientation and phase.
- Source sampling strategies.
- Source models that **evolve independently of propagation**.

**Required data concepts.** Source model; surface brightness distribution; source
geometry and orientation; source sample; emission spectrum (see Stage 7 interaction).

**Required computational concepts.** Source sampling; integration over source extent;
per-sample weighting; convergence of the source integral.

**Current support: Absent.** Initial conditions are four POD structs describing a single
geodesic's starting parameters (`r0`, `theta0`, `phi0`, velocities, impact parameter).
There is no emission, no extent, no brightness, and no sampling.

**Assumptions that may become invalid.** That "initial conditions" is the right concept
at all. An extended source does not have initial conditions; it has a geometry and a
brightness distribution, from which many rays are *derived*. The current
`InitialConditions` structs conflate "the scientific setup" with "the numerical starting
vector."

---

### Stage 7 — Observer / Spacecraft Dynamics

**Scientific motivation.** An SGL mission observes from a moving spacecraft over years.
Observation geometry changes continuously, and that change is scientifically meaningful
— it is how the image is scanned.

**Requirements.**

- Spacecraft position and trajectory.
- Observer motion and velocity, including relativistic aberration and Doppler effects
  where relevant.
- Changing observation geometry over mission time.
- Time-dependent observations and observation scheduling.

**Required data concepts.** Observer state (position and velocity); trajectory/ephemeris;
observation epoch; time-indexed observation geometry.

**Required computational concepts.** Time-parameterized geometry evaluation; frame
transformation between observer and simulation frames; scheduling and sequencing.

**Current support: Absent, and structurally Contradicted.**

There is no observer. Beyond that, the current dynamics interface is **autonomous**:

```cpp
virtual State compute_derivative(const State& state) const = 0;
```

There is no time, epoch, or environment argument anywhere in the propagation path. A
static Schwarzschild metric makes this correct today. Time-dependent geometry, a moving
observer that changes the aiming problem, or any epoch-dependent effect has no channel
through which to enter the computation.

**Assumptions that may become invalid.** That the observer is a fixed point; that
observation is instantaneous; that mission time does not exist; that the derivative
depends on nothing but the current state.

---

### Stage 8 — Mission-Level Simulation

**Scientific motivation.** Assessing SGL mission feasibility requires composing every
prior stage into an end-to-end model.

```text
source → solar gravitational lens → spacecraft → instrument
       → observation → image / data → scientific analysis
```

**Requirements.**

- Composition of all prior stages into a coherent campaign.
- Observation planning and sequencing over mission duration.
- End-to-end data products traceable from scientific input to final analysis.
- A clearly defined boundary: SGL models the *scientific* simulation; mission
  engineering (propulsion, thermal, comms, ops) lives outside and consumes SGL outputs.

**Required data concepts.** Mission scenario; observation campaign and plan; instrument
configuration set; data product catalog; provenance chain.

**Required computational concepts.** Multi-stage composition; long-running campaign
execution; artifact management; end-to-end traceability.

**Current support: Absent.** Every prior stage is a prerequisite.

**Assumptions that may become invalid.** That one process, one config object, and one
in-memory result is the unit of computation. At mission scale the unit is a campaign of
many runs producing persistent, catalogued artifacts.

---

## 3. Numerical Evolution

**Scientific motivation.** A scientific result must be separable from the numerical
method that produced it. Otherwise "the answer" and "an artifact of RK4 at `dt = 0.001`"
cannot be distinguished, and no convergence claim is meaningful.

### Requirements

| Requirement | What it demands |
|---|---|
| Multiple integration methods | RK4, higher-order Runge–Kutta, symplectic and structure-preserving schemes, implicit methods for stiff regimes |
| Adaptive integration | Local error estimation, step acceptance/rejection, step-size control |
| Configurable tolerance | Accuracy expressed as a scientific requirement, not as a fixed step count |
| Multiple precision levels | `double`, extended, and arbitrary precision for reference calculations |
| Alternative coordinate systems | Isotropic, harmonic, Cartesian, horizon-penetrating charts; chart choice as a numerical decision |
| Alternative state representations | Augmented state for variational equations, bundle derivatives, phase, or optical path length |
| Error estimation | Per-step and accumulated, as routine output |
| Convergence studies | Systematic refinement as a supported workflow |
| High-accuracy reference calculations | Slow, trustworthy computations used as truth for faster methods |

### Current support

| Requirement | Level | Evidence |
|---|---|---|
| Multiple integrators | **Partial** | `Integration::Integrator` is a genuine seam — `RK4Integrator.cpp` references no physical concept. `TrajectorySolver` accepts any `Integrator&`. But `run_simulation` always passes `default_integrator()`, so the public path is RK4-only. |
| Adaptive integration | **Contradicted** | `step(const State&, double dt, const DerivativeFunc&) const → State` returns only the next state. There is no channel for error estimate, step rejection, or a revised `dt`. The loop in `TrajectorySolver` owns progression with a fixed `dt` and a fixed iteration count. |
| Tolerances | **Absent** | `SimulationConfig` exposes `dt` and `max_steps` — method parameters, not accuracy requirements. |
| Precision levels | **Contradicted** | `double`, `Vector4d`, and `Matrix4d` are hard-coded throughout. No template parameter, no scalar type alias. |
| Alternative charts | **Nominal** | `CoordinateChartKind` has one enumerator. `CoordinateChart` provides Cartesian↔spherical conversion but has **no caller anywhere** and is not connected to `Metric` or `State`. Meanwhile `X[1]` and `X[2]` are indexed as radius and polar angle in the metric, builders, observables, termination policies, and the projection callback. |
| State representations | **Contradicted** | `State` is exactly two `Vector4d`s, with `operator+` and scalar `operator*` shaped for RK4. |
| Error estimation | **Absent** | Only `null_hamiltonian_error` exists — a constraint-violation diagnostic, not an integration error estimate — and it has no caller. |
| Convergence studies | **Absent** | No mechanism for systematic refinement. |
| Reference calculations | **Absent** | No analytical reference remains after extraction. |

### The central numerical assumption at risk

The framework currently equates **"the trajectory"** with **"the sequence of accepted
fixed-size RK4 steps."** Under adaptive integration those separate: the trajectory
becomes a continuous object sampled at requested points, while steps become an internal
detail of the method. Any consumer that treats `history[i]` as physically meaningful —
rather than as one sample of a continuous path — inherits this assumption. The smoke
test's `history.front()` / `history.back()` usage is benign; dense output, event
localization, and image accumulation would not be.

---

## 4. Computational Scaling

**Scientific motivation.** Observational predictions require ray counts many orders of
magnitude above one.

```text
1 trajectory → 10² → 10⁴ → 10⁶ → 10⁸+ rays / computational samples
```

### Requirements

- Vectorized computation over ray batches.
- Batch propagation as the natural unit, with single-ray as the degenerate case.
- Parallel execution across cores.
- GPU acceleration.
- CPU reference implementations that remain authoritative for correctness.
- Heterogeneous execution across CPU and GPU.
- HPC workloads across nodes.
- Distributed parameter sweeps.
- Large scientific datasets exceeding memory.

### The governing principle

> The scientific abstraction must remain independent of the eventual execution strategy.

This does **not** require optimizing now. It requires that the scientific description of
a computation not encode a memory layout, a dispatch mechanism, or a device.

### Current support: Contradicted at the representation level

Scaling is contradicted not by the absence of parallel code — which is expected and fine
— but by three properties of the *scientific* types:

1. **Array-of-structs by construction.** `State` holds two `Vector4d` members. A million
   rays is a million separate objects. Batch-friendly layouts require the opposite
   arrangement, and `State` is the highest-degree node in the dependency graph, so its
   layout is a framework-wide commitment.

2. **Virtual dispatch inside the innermost loop.** Each derivative evaluation performs 64
   virtual `christoffel` calls; each RK4 step performs 4 derivative evaluations through a
   `std::function`. Per accepted step that is 256 virtual calls plus 4 indirect calls.
   This is correct and clear on CPU, and it is the pattern that does not survive
   device-side execution.

3. **In-memory ownership as the only result model.** `std::vector<State> history` with
   `reserve(std::min(max_steps, 100000))` assumes results fit in process memory and are
   consumed in-process. Verified absence of any I/O: no `fstream`, no serialization, no
   data format anywhere in `physics/`.

Additionally, `Absent` across the board: no threading (`std::thread` verified absent), no
OpenMP, no device code, no build-system provision for backend targets — `CMakeLists.txt`
declares one static CPU library and one optional executable.

**Assumptions that may become invalid.** One ray per call; scalar `double` per component;
in-memory result ownership; synchronous return; single process; host-only memory.

---

## 5. Validation Requirements

**Scientific motivation.** For a framework whose outputs cannot be checked against a real
SGL observation, validation *is* the credibility mechanism. It is the primary scientific
deliverable, not a testing chore.

### The required workflow

```text
analytical prediction → numerical implementation → simulation
   → error measurement → convergence study → validation result
```

### Requirements

- Analytical geodesic solutions as reference truth.
- Known gravitational-deflection results (weak-field limit, Shapiro delay, photon-sphere
  behaviour).
- Comparison against published SGL predictions.
- Convergence studies with demonstrated order of accuracy.
- Numerical error characterization distinct from constraint violation.
- Conservation-law monitoring.
- Cross-method comparison (integrator vs integrator, chart vs chart).
- Independent implementations of the same prediction.
- **Validation separable from production simulation** — validation must not require
  special hooks compiled into the production path.

**Required data concepts.** Validation case; reference/expected result; tolerance
specification; error metric; convergence table with fitted order; validation report;
provenance record.

**Required computational concepts.** Systematic parameter refinement; error norms;
order-of-accuracy fitting; method-to-method comparison; regression detection over time.

### Current support: Partial, and weaker than it appears

| Element | Level | Evidence |
|---|---|---|
| Conservation monitoring | **Present** | `conserved_energy`, `conserved_angular_momentum`, `null_hamiltonian` — correct and θ-generalized during extraction. |
| Constraint-violation metric | **Nominal** | `null_hamiltonian_error` exists; no caller. |
| Executable check | **Partial** | One smoke test, hand-written `main()`, exit codes 0/1/2. |
| Test registration | **Absent** | Verified: no `enable_testing()`, no `add_test()` in `CMakeLists.txt`. Not discoverable by CTest or CI. |
| Analytical references | **Absent** | Penrose's `analytical_freefall_time` was dropped during extraction. Nothing replaced it. |
| Deflection-angle validation | **Absent** | The single most standard check for a lensing code — compare computed deflection against `4GM/(c²b)` in the weak-field limit — does not exist. |
| Convergence studies | **Absent** | No refinement mechanism. |
| Cross-method comparison | **Absent** | Only one integrator, one metric, one chart. |
| Validation reports | **Absent** | Output is two `std::cout` lines. |

**The important distinction.** Conservation checks and accuracy checks detect different
failures. A geodesic integrator can conserve `E` and `L` to `1e-14` while computing the
wrong deflection angle, because conserved quantities are functions of the state that the
constraint structure preserves regardless of trajectory correctness. The current
validation posture detects drift and would not detect a systematically wrong-but-conserved
result. The observed `|dE/E| = 1.01e-14` is genuine evidence of integration quality — and
it is not evidence of physical correctness.

**Assumptions that may become invalid.** That validation is a single pass/fail executable;
that tolerances are hard-coded literals (`1e-3`, chosen 11 orders of magnitude looser than
observed drift, with no recorded justification); that validation output is console text.

---

## 6. Experimentation Requirements

**Scientific motivation.** Research proceeds by controlled comparison, not by individual
runs. The framework must let a researcher ask *"how does changing X affect Y?"* without
modifying the framework.

### Requirements

- Parameter sweeps over arbitrary input dimensions.
- Controlled experiments with explicit independent and dependent variables.
- Reproducible configurations that can be stored, shared, and re-executed.
- Batch simulation over many configurations.
- Experiment metadata: what was run, when, with which code version, on what.
- Result datasets that outlive the process.
- Analysis pipelines consuming those datasets.
- Model-to-model comparison.
- Sensitivity analysis.

**Required data concepts.** Experiment/campaign definition; parameter space and sampling
plan; run identity; result artifact; dataset catalog; metadata and provenance record;
code and model version.

**Required computational concepts.** Sweep expansion; batch scheduling; partial-failure
handling; result aggregation and indexing; resumability.

### Current support: Absent

Nothing in the repository addresses experimentation. Specifically verified:

- Configuration exists only as a C++ aggregate assigned field-by-field in source code.
  There is no file format, no CLI, no environment configuration — `vcpkg.json` is the
  only non-source config file in the tree, and it is a package manifest.
- No serialization of any kind (no `fstream`, no JSON, no schema).
- No run identity. `SimulationConfig::name` is a `std::string` label that is copied into
  `SimulationResult::name` and **never read**.
- No dataset, no catalog, no provenance.

**The consequence.** Today, changing a parameter means editing C++ and recompiling. Every
experiment is a code change, which means no experiment is reproducible except by
reference to a source revision.

**Assumptions that may become invalid.** That configuration is compile-time; that one call
produces one result consumed immediately; that results need not persist; that the
researcher is also the person recompiling.

---

## 7. Physics Expansion

**Scientific motivation.** SGL is the focus, but the physical formulation will not remain
fixed. The solar field is not exactly Schwarzschild, and geometric optics is not the whole
story at the focal line.

### Requirements

- More accurate solar gravitational models: quadrupole moment `J₂`, oblateness, rotation
  (Lense–Thirring), realistic interior mass distribution.
- Alternative spacetime models where scientifically relevant.
- Higher-order gravitational effects and post-Newtonian corrections.
- Perturbations from planetary masses.
- Additional physical effects: solar corona plasma refraction, absorption, scattering.
- Frequency-dependent phenomena (plasma refraction scales with wavelength).
- **Wave-optical effects and diffraction.**
- Finite-wavelength behaviour.

### The major long-term question

> Can the framework eventually support wave-optical or diffraction-based modeling without
> replacing the fundamental architecture built around geometric-ray propagation?

This must be treated as the defining architectural question for SGL, because at the solar
focal line **it is not optional physics**. The Einstein ring is unresolved and diffraction
sets the achievable resolution. A geometric-optics-only SGL framework cannot produce the
central quantitative predictions the mission concept depends on.

The architecturally significant point: wave optics is not a new *model* plugged into the
existing propagation abstraction. It changes the fundamental object being computed.

| | Geometric optics | Wave optics |
|---|---|---|
| Object computed | Ray path | Complex field |
| State | Position, tangent | Amplitude, phase, polarization, wavelength |
| Result | Trajectory | Field distribution / diffraction integral |
| Composition | Independent rays | Coherent superposition |

A hybrid posture is the realistic target — geometric propagation to accumulate optical
path length and amplitude, with a diffraction integral evaluated at the observation plane.
That hybrid still requires the state to carry phase and the result to be a field.

### Current support

| Expansion | Level | Evidence |
|---|---|---|
| Higher-order solar models | **Partial** | `Spacetime::Metric` is a genuine seam and could express another Christoffel source. But `MetricKind` has one enumerator, `SchwarzschildParameters` is the only parameter type, and `require_spacetime` throws for anything else — so the public path is closed. |
| Non-vacuum / plasma | **Contradicted** | `GeodesicDynamics` computes `a^μ = -Γ^μ_{αβ}U^αU^β` and nothing else. A refractive medium adds terms with no channel to enter. |
| Rotating / time-dependent | **Contradicted** | `christoffel(mu, alpha, beta, X)` depends only on position; `compute_derivative(const State&)` is autonomous. Kerr was explicitly removed during extraction. |
| Perturbations | **Absent** | No multi-body or perturbation concept. |
| Frequency dependence | **Absent** | Grep-verified: no `wavelength`, `frequency`, or spectral concept in `physics/`. |
| Wave optics / diffraction | **Contradicted** | Grep-verified: no `phase`, `amplitude`, or `intensity` anywhere. `State` has exactly `X` and `U`. |

**Assumptions that may become invalid.** That physics enters exclusively through
Christoffel symbols; that the metric is static and vacuum; that a photon is fully described
by position and tangent; that rays are independent and never interfere; that propagation is
wavelength-independent.

---

## 8. Execution Requirements

**Scientific motivation.** The same scientific question must be answerable on a laptop, a
workstation GPU, and an HPC allocation, with identical meaning and comparable results.

### The target separation

```text
Physical Problem
       ↓
Physical Model
       ↓
Numerical Method
       ↓
Execution Backend
       ↓
Scientific Result
       ├──────→ Analysis
       └──────→ Visualization
```

This is a **target for evaluation, not a claim about the current architecture.**

### Requirements

- Execution strategy selectable without altering the scientific description.
- Serial CPU as the reference implementation, permanently.
- Parallel, GPU, heterogeneous, and distributed execution as alternative strategies.
- Asynchronous and long-running execution with progress and cancellation.
- Checkpoint and restart for long campaigns.
- Deterministic, comparable results across backends within stated tolerance.

**Required data concepts.** Execution context/policy; job and partition; checkpoint;
device-resident data; backend-independent result artifact.

**Required computational concepts.** Work partitioning; scheduling; synchronization;
cross-backend numerical equivalence.

### Evaluation against the target layering — where SGL sits today

| Target layer | Present in implementation? | Where it actually lives |
|---|---|---|
| Physical Problem | **No** | Not represented. The closest analogue is `Scenario` — a four-valued enum selecting an initial-condition formula, not a statement of a physical question. |
| Physical Model | **Partially** | Split across `SchwarzschildMetric` (Christoffel), `GeodesicDynamics` (equation), the four builders (constraints), `make_schwarzschild_post_step` (projection), and `SchwarzschildObservables.h` (diagnostics). No single boundary owns "the physical model." |
| Numerical Method | **Yes** | `Integrator`, `RK4Integrator`, `TrajectorySolver`, `TerminationPolicy`. This layer is real and clean. |
| Execution Backend | **No** | No such layer. CPU/Eigen is assumed everywhere, not selected anywhere. |
| Scientific Result | **Partially** | `SimulationResult` exists but is trajectory-only, and four of its five fields are never read. |
| Analysis | **Weakly** | Header-only observables, joined to simulation only by the consumer. |
| Visualization | **No** | Absent by design during extraction. |

**The mismatch, stated precisely.** The target has six layers between problem and result.
The implementation has approximately two-and-a-half: a numerical layer that is genuinely
well-separated, a physical model layer that is real but distributed across five locations,
and no problem or backend layer at all. The layers that *do* exist are compressed into
`SimulationPipeline.cpp`, which performs problem interpretation, model construction,
method selection, and result assembly inside one translation unit's anonymous namespace.

This is a reasonable shape for a Stage 1 kernel. It is two layers short of the target, and
the two missing layers — Physical Problem at the top and Execution Backend at the bottom —
are precisely the ones Stages 2–8 and Section 4 require.

---

## 9. Visualization Requirements

**Scientific motivation.** Visualization is an instrument of investigation. It must serve
the science and must not dictate how scientific data is represented.

### Requirements

- Trajectory and ray visualization.
- Lensing geometry visualization (source, lens, observer, focal line).
- Observer-plane and image visualization.
- Scientific plots: convergence, error, magnification, parameter dependence.
- 3D scene visualization.
- Interactive exploration.

### The governing constraint

> Rendering must consume scientific results, not determine how those results are
> represented internally.

The direction of dependency must be **visualization → results**, never
**results → visualization**.

### Current support: Absent as capability, and already violated in the data model

No visualization code exists — a deliberate extraction decision recorded in `README.md`
("Realtime GPU rendering, Kerr, GLFW/OpenGL viewers, and Penrose trajectory-viz were
excluded on purpose").

The requirement is nonetheless **already partly violated**, in the only way it can be
violated without any renderer present. `SimulationResult` and `SimulationMetadata` carry
presentation-shaped fields — `characteristic_radius`, `horizon_radius`,
`photon_sphere_radius`, `coordinate_chart`, `name` — that are written on every run and
read by nothing. Phase 1 established their consumer: Penrose's
`run/adapter/SimulationTrajectoryAdapter.h`, which turned `SimulationResult` into scene
data. The rendering contract outlived the renderer.

This matters as a requirement, not as a defect: it is direct evidence that the physics
result type has previously absorbed presentation concerns, which is exactly the failure
mode this section prohibits going forward.

**Assumptions that may become invalid.** That the physics result is the right vehicle for
display metadata; that a full in-memory trajectory is a suitable rendering input at
ensemble scale; that visualization consumes trajectories rather than images and fields.

---

## 10. Reproducibility Requirements

**Scientific motivation.** A result that cannot be reproduced is not a scientific result.
For a framework whose outputs will inform mission concept studies, reproducibility is a
correctness requirement.

### Requirements

- Deterministic configurations producing bit-comparable or tolerance-comparable results.
- Explicit parameters — every value affecting a result must be recorded, including
  defaults.
- Reproducible experiments re-executable from a stored specification.
- Versioned models: which physical model, which numerical method, which code revision.
- Saved results that outlive the process.
- Metadata attached to results.
- Validation provenance: which validation established confidence in this result.

**Required data concepts.** Configuration record with full parameter closure; code/model
version; run identity; result artifact with attached metadata; provenance chain.

**Required computational concepts.** Deterministic execution; configuration capture and
replay; content addressing or hashing; version stamping.

### Current support: split — strong foundation, absent mechanism

**What is genuinely strong (Present).** The computation is deterministic by construction,
and this is a real asset:

- No random number generation anywhere — verified: no `rand`, `random`, or `mt19937` in
  `physics/`.
- No threading, so no non-deterministic reduction order — verified: no `std::thread`, no
  OpenMP.
- No time or clock dependence — verified: no `chrono`.
- No I/O, no external state, no environment reads.
- Fixed step size and fixed iteration count.

Given identical inputs and an identical binary, the current implementation produces
identical outputs. That is the hard part of reproducibility, and SGL has it.

**What is Absent.** Every mechanism for *capturing* what was run:

- No configuration serialization, so a configuration cannot be recorded or replayed.
- No version stamping of code, model, or method.
- No run identity — `SimulationConfig::name` is carried into the result and never read.
- No result persistence.
- No provenance linking a result to the validation that supports it.

**The asymmetry to record.** SGL is deterministic but not reproducible. Determinism is a
property of the computation; reproducibility is a property of the record. The framework
currently has the first and none of the second. The risk direction is also worth stating:
determinism is easy to lose accidentally — the first parallel reduction, the first
GPU-accelerated sum, or the first stochastic source sampler removes it — and it is far
harder to recover than to preserve.

---

## 11. Architectural Principles

These are the principles against which the architecture will be evaluated. They are
stated as requirements, not as designs.

### P1 — Additive growth

```text
New scientific question → New model / experiment / module
    → Existing infrastructure → New result
```

and **not**

```text
New scientific question → Modify central simulation architecture
    → Modify existing abstractions → Modify unrelated subsystems
    → Risk destabilizing framework
```

**Evaluation against the current implementation.** Phase 3 identified
`SimulationPipeline.cpp`, `SimulationConfig.h`, `SimulationResult`, and `State` as
modification hotspots. Concretely, adding a second metric to the *public* path today
requires touching `MetricKind.h`, `SimulationConfig.h`, `SimulationPipeline.cpp`, the
initial-state builders, the metadata construction, the observables, and the CMake source
list. That is the second pattern, not the first.

The qualification that matters: at the *solver* level the first pattern already holds. A
new `Metric`, `DynamicsModel`, `Integrator`, or `TerminationPolicy` can be added and used
by calling `TrajectorySolver` directly, with no modification to existing code. The
additive property exists underneath and is lost at the public API.

### P2 — Scientific description independent of execution strategy

A stated scientific problem must not encode integrator, step size, memory layout, or
device. **Currently:** `SimulationConfig` contains `dt` and `max_steps`, so the numerical
method is embedded in the configuration object; `State` fixes the memory layout
framework-wide.

### P3 — Physical model separable from numerical method

**Currently:** honoured at the `DerivativeFunc` boundary — the strongest boundary in the
codebase — and broken above it, where `SimulationPipeline.cpp` inlines Schwarzschild
algebra (`1 - rs/r`, `1.5 * mass`, the null projection) into orchestration.

### P4 — Results independent of consumers

Scientific results must not be shaped by any particular downstream consumer.
**Currently:** violated in residual form by the write-only presentation metadata described
in Section 9.

### P5 — Multiple result kinds

Trajectories, images, fields, convergence tables, and validation reports must all be
expressible. **Currently:** one result kind exists, and it is trajectory-shaped.

### P6 — Validation separable from production

**Currently:** partially satisfied — observables are independent of the simulation library
— but there is no validation framework to be separate from.

### P7 — Reference implementations remain authoritative

A slow, simple, verifiable CPU path must always exist alongside accelerated paths.
**Currently:** the reference path is the *only* path, which satisfies the principle
trivially and does not test it.

### P8 — Determinism preserved under optimization

Any future parallel, GPU, or distributed execution must preserve or explicitly document
departures from determinism. **Currently:** determinism holds and nothing threatens it
yet.

---

## 12. Future Capability Matrix

### Summary

Support levels are as defined at the top of this document, and each is grounded in
Phase 1–3 verification.

| # | Capability | Stage | Current support | Primary at-risk assumption |
|---|---|---|---|---|
| 1 | Photon geodesic propagation | 1 | **Present** | — |
| 2 | Analytical validation | 1/5 | **Absent** | Conservation implies correctness |
| 3 | SGL lens/source/observer geometry | 2 | **Absent** | Initial-value framing; static geometry |
| 4 | Connection (source→observer) solving | 2 | **Absent** | Propagation is always an initial-value problem |
| 5 | Ray ensembles | 3 | **Contradicted** | One ray, one result, full history |
| 6 | Ray bundles | 3 | **Contradicted** | Rays are independent; `State` has no neighbours |
| 7 | Image formation | 4 | **Contradicted** | The output of a computation is a trajectory |
| 8 | PSF modeling | 4 | **Absent** | No image-domain data product |
| 9 | Instrument / detector modeling | 5 | **Absent** | Simulation coordinates are the only coordinates |
| 10 | Extended sources | 6 | **Absent** | "Initial conditions" is the right setup concept |
| 11 | Observer / spacecraft dynamics | 7 | **Absent + Contradicted** | Autonomous derivative; static observer |
| 12 | Mission-level simulation | 8 | **Absent** | One process, one config, one in-memory result |
| 13 | Multiple integrators | Num. | **Partial** | Public path hardwires RK4 |
| 14 | Adaptive integration | Num. | **Contradicted** | `step` returns only a state; solver owns fixed progression |
| 15 | Precision / tolerance control | Num. | **Contradicted** | `double` hard-coded; `dt` substitutes for tolerance |
| 16 | Alternative coordinate charts | Num. | **Nominal** | Raw `X[1]`/`X[2]` indexing framework-wide |
| 17 | Convergence studies | Num./Val. | **Absent** | No refinement mechanism |
| 18 | Batch / vectorized execution | Scale | **Contradicted** | Array-of-structs `State`; virtual calls in inner loop |
| 19 | GPU / heterogeneous execution | Scale | **Contradicted** | Host memory; dynamic dispatch; `std::function` |
| 20 | HPC / distributed execution | Scale | **Absent** | Single process; no serialization |
| 21 | Large trajectory datasets | Scale/Data | **Contradicted** | Full history materialized in memory |
| 22 | Parameter sweeps / experiments | Exp. | **Absent** | Configuration is compile-time |
| 23 | Reproducible experiment records | Repro. | **Split** | Deterministic, but nothing is captured |
| 24 | Higher-order / non-vacuum physics | Phys. | **Partial / Contradicted** | Physics enters only via Christoffel symbols |
| 25 | Wavelength dependence | Phys. | **Absent** | Propagation is wavelength-independent |
| 26 | Wave optics / diffraction | Phys. | **Contradicted** | A photon is position + tangent; rays never interfere |
| 27 | Scientific visualization | Vis. | **Absent** | Result type already carries presentation metadata |

### Detailed entries for the highest-risk capabilities

The seven capabilities below are the ones rated **Contradicted** where the contradiction
is structural rather than incidental.

---

#### C5 / C6 — Ray Ensembles and Bundles

- **Scientific motivation.** Every observational quantity — magnification, image
  structure, signal — is a population property. Bundles additionally encode local beam
  distortion, which *is* magnification.
- **Architectural implications.** Ray count must become independent of both the physics
  and the storage strategy. Bundles further require that neighbour relationships survive
  propagation.
- **Required data concepts.** Ray; ensemble; bundle with neighbour/derivative
  information; sampling plan; per-ray provenance.
- **Required computational concepts.** Batch propagation; sampling; reduction; the
  independent-vs-coupled distinction.
- **Likely extension point.** Above `TrajectorySolver` and below any optics layer —
  a region of the architecture that currently has no occupant.
- **Current architectural support.** `propagate` is the only ensemble-shaped component
  and is unwired. `run_simulation` is single-ray by signature.
- **Assumptions that may become invalid.** One result per call; history always retained;
  results owned in process memory; rays are independent.

---

#### C7 — Image Formation

- **Scientific motivation.** The framework must predict measurements, not paths.
- **Architectural implications.** The fundamental output type must generalize beyond
  trajectories. Aggregation over populations becomes a first-class operation.
- **Required data concepts.** Image plane; sample grid; intensity field; magnification
  map; angular coordinates.
- **Required computational concepts.** Ray-to-pixel accumulation; flux conservation;
  integration over source and aperture.
- **Likely extension point.** A new optics layer consuming ensembles — with the
  significant caveat that it would need a result model that does not exist.
- **Current architectural support.** None. The central invariant is trajectory-shaped.
- **Assumptions that may become invalid.** `SimulationResult` as the universal output;
  one run producing one result; trajectory history being the scientific artifact.

---

#### C11 — Observer / Spacecraft Dynamics

- **Scientific motivation.** SGL imaging is performed by scanning with a moving
  spacecraft over mission timescales.
- **Architectural implications.** Time and observer state must be able to enter the
  computation. Today there is no channel for either.
- **Required data concepts.** Observer state with velocity; ephemeris; observation epoch;
  time-indexed geometry.
- **Required computational concepts.** Time-parameterized geometry; frame transformation;
  aberration and Doppler; scheduling.
- **Likely extension point.** A problem/geometry layer above initial-state construction.
- **Current architectural support.** Absent, and contradicted by the autonomous signature
  `compute_derivative(const State&)` and by position-only `christoffel(..., X)`.
- **Assumptions that may become invalid.** Static observer; instantaneous observation;
  no mission time; state-only derivative.

---

#### C14 — Adaptive Integration

- **Scientific motivation.** Efficiency near the photon sphere and error control
  everywhere; accuracy stated as a scientific requirement rather than a step count.
- **Architectural implications.** The integrator and the solver must exchange more than a
  state — at minimum an error estimate and an accept/reject decision.
- **Required data concepts.** Step result with error estimate; tolerance specification;
  dense output; step-size history.
- **Required computational concepts.** Local error estimation; step control; rejection and
  retry; interpolation between accepted steps.
- **Likely extension point.** The `Integrator` boundary — which is genuinely clean and
  nonetheless the wrong shape, because its return type carries no diagnostic channel.
- **Current architectural support.** Contradicted: `step` returns `State`; the solver owns
  a fixed loop; `SimulationConfig` exposes `dt` and `max_steps`.
- **Assumptions that may become invalid.** Fixed `dt`; one stored state per loop
  iteration; `max_steps` bounding wall-clock cost; history index meaning anything
  physical.

---

#### C18 / C19 — Batch, Vectorized, and GPU Execution

- **Scientific motivation.** 10⁶–10⁸ rays are unreachable at current per-ray cost.
- **Architectural implications.** The scientific abstraction must stop implying a memory
  layout and a dispatch mechanism.
- **Required data concepts.** Batch/device-resident state; execution context; backend-
  independent result artifact.
- **Required computational concepts.** Work partitioning; vectorization; kernel execution;
  host-device transfer; cross-backend numerical comparison.
- **Likely extension point.** An execution-backend layer that does not currently exist.
- **Current architectural support.** Contradicted by `State`'s array-of-structs layout,
  by 256 virtual calls per accepted RK4 step, and by `std::vector<State>` result
  ownership.
- **Assumptions that may become invalid.** Object-per-ray; `double` scalars; virtual
  dispatch being free; host-only memory; synchronous return; determinism under parallel
  reduction.

---

#### C21 — Large Trajectory Datasets

- **Scientific motivation.** Ensemble-scale runs produce data exceeding memory, and
  results must be analysed after the fact by multiple methods.
- **Architectural implications.** Results must be able to stream out during computation
  rather than accumulate.
- **Required data concepts.** Trajectory sink; dataset with schema; chunking; index;
  attached metadata.
- **Required computational concepts.** Streaming output; chunked write; compression;
  out-of-core read.
- **Likely extension point.** Between the solver and the result — a boundary that does
  not exist, since `solve` materializes the full vector before returning.
- **Current architectural support.** Contradicted; no I/O of any kind exists.
- **Assumptions that may become invalid.** Results fit in memory; results consumed
  in-process; `std::vector<State>` as the canonical trajectory representation.

---

#### C26 — Wave Optics / Diffraction

- **Scientific motivation.** At the solar focal line, diffraction determines the
  achievable angular resolution. This is core SGL physics, not an optional refinement.
- **Architectural implications.** The computed object changes from a path to a field.
  Superposition becomes meaningful, so rays cease to be independent.
- **Required data concepts.** Complex amplitude; phase; optical path length; wavelength;
  polarization; field distribution; aperture.
- **Required computational concepts.** Phase accumulation along paths; coherent
  superposition; diffraction integrals; wavelength-resolved computation.
- **Likely extension point.** Unresolved — and this is the point. It may be a new state
  representation, a parallel computational branch, or a hybrid where geometric propagation
  accumulates phase and a diffraction integral is evaluated at the observation plane.
  Which of these is correct is a scientific question that must be answered before it is an
  architectural one.
- **Current architectural support.** Contradicted at the most fundamental level: `State`
  is `{X, U}`, and no phase, amplitude, or wavelength concept exists anywhere.
- **Assumptions that may become invalid.** A photon is fully described by position and
  tangent; rays are independent; propagation is wavelength-independent; the result of
  propagation is a path.

---

## 13. Architectural Questions That Must Remain Open

These questions must not be answered prematurely. Each represents a decision where
committing early — in either direction — would constrain the framework before the science
is understood well enough to justify it. They are recorded here as open, with the
consequences of resolving them in each direction.

**Q1 — Is the fundamental computed object a trajectory, a ray, a field, or a data
product?**
This determines the central data model. Committing to "trajectory" is the current implicit
answer and is the assumption most likely to fail (Stages 3, 4, and 7 all contradict it).
Committing prematurely to "field" would overweight wave optics before the geometric regime
is fully validated.

**Q2 — Is propagation an initial-value problem or a boundary-value problem?**
The current architecture assumes initial-value exclusively. SGL's characteristic question —
"which ray connects this source to this observer?" — is a two-point boundary-value problem.
Both are needed. Which is primary shapes the entire problem-definition layer.

**Q3 — Can wave optics coexist with geometric propagation, or does it require a parallel
branch?**
The single most consequential open question, per Section 7. Answering "coexist" too early
risks contorting the geometric path; answering "parallel branch" too early risks two
frameworks that cannot be compared against each other.

**Q4 — What is the atomic unit of computation: a ray, a bundle, a batch, or an
observation?**
This determines where parallelism, storage, and result identity naturally attach. The
current answer is "one ray with full history," which Section 4 shows does not scale.

**Q5 — Does the state representation stay fixed, or become extensible?**
Wave optics wants phase and amplitude; variational methods want bundle derivatives;
GPU execution wants a different layout; alternative charts want different components.
A fixed `State` is simple, fast, and currently a framework-wide commitment. An extensible
state costs performance and clarity. This trade-off should be made with evidence.

**Q6 — Where is the boundary between scientific simulation and mission engineering?**
Stage 8 requires this boundary to be explicit. Drawing it too tightly excludes
observation planning that is genuinely scientific; too loosely turns SGL into a mission
simulator with a physics component.

**Q7 — Should the framework own experiment orchestration, or delegate it?**
A native experiment layer gives provenance and reproducibility; delegation to external
tooling keeps the framework focused. Section 6 requires the *capability*, not necessarily
that SGL implement it.

**Q8 — What is the canonical persisted result format, and when does it stabilize?**
Committing early risks encoding today's trajectory-only assumption into stored data that
outlives the code. Committing late risks accumulating unreproducible results. Both
failure modes are real.

**Q9 — Is `SchwarzschildParameters::mass` normatively mass or Schwarzschild radius?**
Carried forward `UNVERIFIED` from Phase 1. The field is named `mass`, the comment says
`rs`, and all current usage treats it as `rs`. Harmless while there is one metric and one
unit convention; a correctness hazard the moment a second gravitational model, an SI
boundary, or an external data source appears. This is the cheapest open question to
resolve and the one most likely to cause a silent physical error if left ambiguous.

**Q10 — Does determinism remain a hard requirement under parallel and GPU execution?**
Section 10 establishes that SGL is deterministic today. Whether bit-reproducibility is a
requirement, or whether tolerance-level reproducibility suffices, changes what parallel
and GPU strategies are admissible. This should be decided before the first parallel
reduction is written, not after.

**Q11 — Do the four existing abstractions (`Metric`, `DynamicsModel`, `Integrator`,
`TerminationPolicy`) remain the right seams?**
Phase 3 found each of them adequate for current scope and inadequate against specific
future capabilities. Whether they evolve, are wrapped, or are replaced is open — but they
should be evaluated on evidence from real second implementations, not on inspection. Each
currently has exactly one meaningful implementation, so their generality is asserted
rather than demonstrated.

**Q12 — What accuracy is scientifically required, and against what reference?**
Unanswered and foundational. Section 5 shows the framework measures conservation drift
(`1e-14`) against a tolerance (`1e-3`) whose provenance is unrecorded. Until the required
accuracy for SGL predictions is stated, no numerical method choice can be justified and no
convergence study has a target.

---

## Document Status

This is a requirements artifact. It defines the target; it does not design toward it and
does not evaluate whether the target is achievable from the current codebase — that is the
next phase's work.

Every "current architectural support" rating is traceable to Phase 1–3 verification:
full source reading, `#include` tracing, linker-symbol enumeration, a clean out-of-tree
build and run, file-level diffs against the originating Penrose tree, targeted absence
checks, and the persisted graph in `graphify-out/`.

Claims inherited as `UNVERIFIED` from earlier phases remain `UNVERIFIED` here and are not
upgraded by restatement.

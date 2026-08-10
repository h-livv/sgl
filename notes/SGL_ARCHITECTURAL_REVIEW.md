# SGL Architectural Review

**Permanent architectural reference for the SGL project.**

This document is the synthesis of a six-phase architectural investigation. It is written to
be read by a technically competent person with no prior knowledge of SGL, of the Penrose
project it was extracted from, or of general-relativistic simulation. It describes what the
project is, what its architecture currently is, where that architecture should evolve, and
whether the distance between those two is survivable.

---

## How to read this document

### Three things kept separate throughout

| Label | Meaning |
|---|---|
| **CURRENT** | What the repository implements today. |
| **TARGET** | What the architecture should evolve toward, derived in Phase 5. |
| **FUTURE REQUIREMENTS** | What SGL is expected to eventually support, established in Phase 4. |

The target architecture does not exist. Nothing in this document should be read as
describing implemented structure unless marked **CURRENT**.

### Evidence standard

Every significant claim carries one of three labels:

| Label | Meaning |
|---|---|
| **[Observed]** | Directly demonstrated by the implementation — a file, signature, call, or measured run. |
| **[Inferred]** | An architectural conclusion drawn from several observations, stated as reasoning rather than fact. |
| **[Predicted]** | An expected consequence of future development. Not yet true. |

Unsupported generalities such as "the architecture is modular" or "this will scale" are
avoided. Where modularity or scalability is claimed, the specific boundary that produces it
is named.

### Verification basis

All **[Observed]** claims were verified against the working tree by full source reading,
`#include` tracing, absence checks by search, and a clean out-of-tree Release build and run
performed while writing this document. The repository is 30 source and configuration files
totalling roughly 1 000 lines of C++, small enough that every file was read in full.

### Companion documents

| Phase | Document | Contribution |
|---|---|---|
| 1 | `notes/ARCHITECTURE_RECONSTRUCTION.md` | Factual inventory of what exists |
| 2 | `notes/SGL_ARCHITECTURE_RECONSTRUCTION.md` | The architecture the implementation represents |
| 3 | `notes/SGL_ARCHITECTURAL_STRESS_TEST.md` | Adversarial failure-mode analysis |
| 4 | `notes/SGL_FUTURE_REQUIREMENTS_MAP.md` | Long-term requirements; defines capability IDs `C1`–`C27` |
| 5 | `notes/SGL_TARGET_ARCHITECTURE.md` | Minimal architecture supporting those requirements |
| 6 | *this document* | Final review and verdict |

---

## Executive Summary

### What SGL is

SGL is a C++20 scientific computing project whose purpose is to study the **Solar
Gravitational Lens** — the use of the Sun's gravitational field as an enormous optical
lens. General relativity predicts that mass bends light; light from a distant object
passing close to the Sun is deflected and comes to a focus beyond roughly 550 astronomical
units. A telescope placed in that focal region would receive light amplified by a factor of
order 10¹¹, potentially enough to resolve surface features on a planet orbiting another
star. Determining whether that is achievable, and what such an instrument would actually
measure, is a computational physics problem. SGL exists to answer it.

**[Observed]** Today the repository implements the foundation of that problem and nothing
above it: numerical propagation of light rays through the curved spacetime around a single
spherically symmetric mass. It contains one gravitational model, one numerical integration
method, four ways of specifying a starting ray, and one executable that propagates a single
ray and checks that two conserved physical quantities stay conserved.

### The scientific problem addressed

Light does not travel in straight lines near mass. Its path is a *geodesic* — the
straightest available path through curved spacetime — obtained by solving a system of
ordinary differential equations whose coefficients (Christoffel symbols) encode the
curvature. SGL solves that system numerically. Everything the project eventually needs —
lens geometry, image formation, instrument modeling — is built on the correctness and
efficiency of that inner computation.

### Relationship to Penrose

**[Observed]** SGL was created by copying a subdirectory out of a larger general-relativity
project called Penrose, which included real-time GPU rendering, black-hole visualization,
rotating (Kerr) black holes, and interactive viewers. The extraction retained the
CPU-side physics and numerics and deliberately discarded the rendering stack, the Kerr
metric, and the interactive layers. `README.md` and `notes/EXTRACTION.md` record these
decisions. The result is a small, dependency-light library (one external dependency: the
Eigen linear-algebra header library) rather than an application.

### Current architectural maturity

**Structured Application**, with one subsystem — the numerical core — at genuine framework
quality. Detailed assessment in Section 16.

### Strongest architectural property

**[Observed]** The numerical integration subsystem is completely independent of physics.
`physics/integrators/RK4Integrator.cpp` contains no reference to any physical concept: no
metric, no geodesic, no spacetime, no coordinate. It advances an abstract state using an
abstract derivative function supplied as `std::function<State(const State&)>`. Physics
reaches it only through that one callable.

This is the single hardest property in the target architecture and the one most scientific
codebases get wrong. SGL has it, and it was inherited intact from Penrose. Section 7
develops the evidence.

### Largest architectural weakness

**[Observed]** There is no representation of the scientific question being asked. The
entry point `run_simulation` takes a `SimulationConfig` that fuses four unrelated concerns
into one struct: which spacetime, which scenario, the integration step size `dt`, and the
iteration cap `max_steps`.

**[Inferred]** With no place to state *what is being computed*, every new scientific
question must be expressed as a new field, enum value, or branch in the central
configuration and pipeline. This is the mechanism behind every modification hotspot
identified in Phase 3, and it is why the framework's genuinely good internal boundaries do
not reach its public surface.

### Most important hidden assumption

**[Observed]** That the output of a computation is a trajectory — a `std::vector<State>`
recording the full path of one ray.

**[Inferred]** SGL's actual scientific question is not "where does this photon go" but
"what does an observer measure," whose answer is an image or an intensity distribution
aggregated over an enormous population of rays. The current output type is a single ray's
path, and an image is not a special case of a path. This assumption is embedded in the
solver's return type, the public API's return type, and the only test.

### Most likely future architectural problem

**[Predicted]** The transition from single rays to ray ensembles. It is the first future
capability that will actually be needed — every observational quantity is a population
property — and the current architecture allocates one heap-backed history vector per ray
inside a function that propagates exactly one ray. Section 17 develops this.

### Overall extensibility

**[Inferred]** Sharply two-sided, and this split is the central finding of the entire
review. At the solver level, extension already works: a new gravitational model, dynamics
law, integrator, or termination rule can be written and used by calling
`TrajectorySolver::solve` directly, with no modification to any existing file. At the public
API level, extension does not work: reaching the same functionality through
`run_simulation` requires touching the config header, the metric-kind enum, the pipeline,
the initial-state builders, and the metadata construction.

**The good architecture exists and is buried one layer below the surface.**

### Preliminary verdict

**[Inferred]** SGL does not need a fundamental redesign. It needs three contract
corrections and two additional layers, on a foundation whose hardest property is already
correct. Six of the eleven abstractions the target architecture requires already exist in
recognizable form. The distance from current to target is a reshaping, not a replacement —
and it is cheapest now, while every interface still has exactly one implementation behind
it. The full verdict is in Section 19.

---

## 1. Project Context

### 1.1 The science, from first principles

Einstein's general relativity describes gravity not as a force but as curvature of
spacetime. Objects — including light — follow the straightest available paths through that
curved geometry. Those paths are called **geodesics**.

A consequence is that mass bends light. Starlight grazing the Sun's edge is deflected by
about 1.75 arcseconds, measured during the 1919 solar eclipse. Because the Sun deflects
light passing on all sides toward the same axis, it acts as a lens. Its focal region begins
at roughly 550 AU — about fourteen times the distance to Pluto — and extends outward along
a line rather than converging to a point.

The **Solar Gravitational Lens** concept proposes sending a spacecraft to that region. The
light amplification is of order 10¹¹, and the angular resolution is extraordinary. A
mission there could, in principle, produce a resolved image of the surface of an exoplanet
— something no conventional telescope of any plausible size can do.

Turning that concept into a credible proposal requires answering quantitative questions:
How is light actually distributed in the focal region? How much of an image can be
reconstructed, and how long would it take? How badly do the Sun's corona and its departure
from perfect sphericity degrade it? Where exactly must the spacecraft fly, and how
precisely?

These are computational physics questions. SGL is the framework intended to answer them.

### 1.2 The computational problem

Answering them starts with propagating light rays through the Sun's gravitational field.
Concretely this means numerically solving the geodesic equation,

```text
d²x^μ/dλ²  =  −Γ^μ_αβ · (dx^α/dλ) · (dx^β/dλ)
```

a system of coupled second-order ODEs where `Γ` (the Christoffel symbols) encodes spacetime
curvature and `λ` parameterizes position along the ray. The simplest useful model of the
Sun's field is the **Schwarzschild metric**, the exact solution for a non-rotating
spherically symmetric mass.

**[Observed]** This is precisely what SGL currently implements. Reformulated as a
first-order system in a state of position and tangent 4-vectors, integrated with the
classical fourth-order Runge–Kutta method.

Two conventions are worth knowing when reading the code. **Geometrized units** set `G = c = 1`,
so masses, lengths, and times share a unit and the Schwarzschild radius is numerically
simple; `physics/core/Units.h` documents this. And **4-vectors** carry a time component
alongside three spatial ones, so both position and velocity are four-component objects.

### 1.3 Relationship to Penrose, and why the extraction happened

**[Observed]** Penrose is a separate, larger general-relativity project by the same author,
oriented toward real-time visualization: GPU-based ray tracing of black holes, both
Schwarzschild and rotating Kerr geometries, and interactive OpenGL viewers. SGL was
produced by copying Penrose's `SGL/` subdirectory into a standalone repository.

**[Observed]** `README.md` and `notes/EXTRACTION.md` record what was kept and what was
dropped. Kept: the Schwarzschild metric, geodesic dynamics, the RK4 integrator, the
trajectory solver, termination policies, initial-condition builders, conserved-quantity
observables, and coordinate-chart utilities. Dropped: all GPU rendering, the Kerr metric,
GLFW/OpenGL viewers, trajectory visualization, and the scene/adapter layer that fed the
renderer.

**[Inferred]** The motivation is legible from the result. Penrose optimizes for *looking
at* relativistic phenomena in real time; SGL needs to *measure* them accurately. Those
goals pull in opposite directions — visualization tolerates approximation for frame rate,
science does not tolerate approximation at all. Separating them lets each be judged on its
own terms, and it reduced the dependency footprint from a graphics stack to a single
header-only linear-algebra library.

### 1.4 Current scope

**[Observed]** Verified against the working tree:

- 30 files. One static library (`sgl_physics`), one optional executable (`sgl_null_smoke`).
- One external dependency: Eigen 3, declared in `vcpkg.json`.
- One gravitational model: Schwarzschild.
- One integration method: fixed-step RK4.
- One coordinate chart: Schwarzschild spherical.
- Four initial-condition scenarios: bound orbit, radial free-fall, null scatter, custom.
- One executable, which propagates one ray and checks two conservation laws.
- No SGL-specific content: no source, no observer, no lens body, no image, no instrument.

**[Observed]** The build was verified during this review. A clean Release build succeeds
and the smoke test reports:

```text
sgl_null_smoke: steps=50001 |dE/E|=1.0103e-14 |dL/L|=3.72693e-15
OK
```

Relative drift in conserved energy of 1.0×10⁻¹⁴ and in angular momentum of 3.7×10⁻¹⁵ over
50 000 steps. That is close to double-precision round-off accumulation and is genuine
evidence that the integration is implemented correctly.

The `README.md` describes several directories as placeholders. **[Observed]** They are
absent from the working tree rather than present and empty.

### 1.5 Long-term direction

Phase 4 established the intended trajectory across eight scientific stages: relativistic
propagation (the current state), SGL lens geometry, ray ensembles, image formation,
instrument modeling, extended sources, observer and spacecraft dynamics, and finally
mission-level simulation. Alongside these run numerical evolution (multiple methods,
adaptive integration, convergence studies), computational scaling (from 1 to 10⁸ rays), and
validation as a first-class capability.

The essential character of the target is that SGL should be a **research framework**, not a
demonstration. The measure of success is whether researchers can pose new scientific
questions without restructuring the framework each time.

---

## 2. Architectural Overview

**CURRENT.** This section describes the architecture as implemented today.

### 2.1 The shape of the system

**[Inferred]** SGL is a layered library with one public entry point. It is not an
application, a service, or a plugin host. There is no runtime configuration, no I/O, no
persistence, no user interface, and no concurrency. A consumer links the static library,
constructs three structs, calls one function, and receives one result in memory.

### 2.2 Conceptual diagram — actual current architecture

```text
                        ┌──────────────────────────────────┐
                        │  CONSUMER                        │
                        │  tests/null_geodesic_smoke.cpp   │  ← the only one
                        └────────────────┬─────────────────┘
                                         │ constructs config + params + ICs
                                         ↓
   ╔═════════════════════════════════════════════════════════════════════╗
   ║  PUBLIC API + CONFIGURATION  (one header)                           ║
   ║  physics/simulation/SimulationConfig.h                              ║
   ║    SimulationConfig, SolverOptions, Scenario, GeodesicKind          ║
   ║    SimulationMetadata, SimulationResult                             ║
   ║    4 × run_simulation(...) declarations                             ║
   ╚═════════════════════════════════════════════════════════════════════╝
                                         │
                                         ↓
   ╔═════════════════════════════════════════════════════════════════════╗
   ║  ORCHESTRATION  physics/simulation/SimulationPipeline.cpp           ║
   ║  ─────────────────────────────────────────────────────────────────  ║
   ║  require_spacetime()          validate: Schwarzschild only          ║
   ║  make_schwarzschild_metric()  construct concrete metric             ║
   ║  build_*()                    construct initial State               ║
   ║  make_schwarzschild_post_step()  ← inline Schwarzschild algebra     ║
   ║  schwarzschild_metadata()     ← inline Schwarzschild algebra        ║
   ║  integrate_schwarzschild()    compose + call solver + fill result   ║
   ╚═════════════════════════════════════════════════════════════════════╝
             │                    │                    │
             ↓                    ↓                    ↓
   ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────┐
   │ INITIAL          │  │ PHYSICS          │  │ NUMERICS             │
   │ CONDITIONS       │  │                  │  │                      │
   │                  │  │ Metric (ABC)     │  │ Integrator (ABC)     │
   │ 4 POD structs    │  │  └Schwarzschild  │  │  └RK4Integrator      │
   │ 4 builders       │  │    Metric        │  │                      │
   │ → State          │  │                  │  │ TrajectorySolver     │
   │                  │  │ DynamicsModel    │  │  solve()             │
   │                  │  │  (ABC)           │  │  propagate() ✗unused │
   │                  │  │  └GeodesicDyn.   │  │                      │
   │                  │  │                  │  │ TerminationPolicy    │
   │                  │  │                  │  │  └Horizon            │
   │                  │  │                  │  │  └RadiusBound ✗unused│
   └──────────────────┘  └──────────────────┘  └──────────────────────┘
             │                    │                    │
             └────────────────────┼────────────────────┘
                                  ↓
   ╔═════════════════════════════════════════════════════════════════════╗
   ║  CORE DATA + CONSTANTS  physics/core/                               ║
   ║  State {Vector4d X; Vector4d U;}   ← universal currency             ║
   ║  SchwarzschildParameters, MetricKind, PhysicalConstants, Units      ║
   ╚═════════════════════════════════════════════════════════════════════╝
                                  ↓
                        ┌──────────────────┐
                        │  Eigen3          │
                        └──────────────────┘

   ┌─── DETACHED (compiled or available, no caller in the execution path) ───┐
   │  physics/metrics/CoordinateChart.{h,cpp}    in library, zero callers    │
   │  physics/validation/observables/            header-only; used only by   │
   │      SchwarzschildObservables.h             the test, not the library   │
   │  TrajectorySolver::propagate()              zero callers                │
   │  RadiusBoundTermination                     zero callers                │
   │  physics/core/Units.h                       included by nothing         │
   └─────────────────────────────────────────────────────────────────────────┘
```

### 2.3 Major subsystems

**[Observed]** Seven, identified by responsibility rather than by directory.

| Subsystem | Location | Role |
|---|---|---|
| Core data | `physics/core/` | `State`, parameters, enums, constants |
| Physics | `physics/metrics/`, `physics/geodesics/` | Spacetime curvature and equations of motion |
| Numerics | `physics/integrators/` | State advancement |
| Simulation | `physics/simulation/` | Solver loop, termination, orchestration, public API |
| Initial conditions | `physics/simulation/initial_conditions/` | Scenario → starting `State` |
| Observables | `physics/validation/observables/` | Conserved quantities and diagnostics |
| Test | `tests/` | The only executable and only consumer |

**[Observed]** These are not one-to-one with directories. `physics/simulation/` alone holds
the public API, the orchestration, the solver loop, and the termination policies — four
distinct responsibilities.

### 2.4 Major interfaces

**[Observed]** Four abstract base classes and one callable type.

| Interface | File | Method | Implementations |
|---|---|---|---|
| `Spacetime::Metric` | `core/Metric.h` | `christoffel(mu, alpha, beta, X)` | 1 — `SchwarzschildMetric` |
| `Dynamics::DynamicsModel` | `geodesics/DynamicsModel.h` | `compute_derivative(state)` | 1 — `GeodesicDynamics` |
| `Integration::Integrator` | `integrators/Integrator.h` | `step(state, dt, derivative)` | 1 — `RK4Integrator` |
| `Simulation::TerminationPolicy` | `simulation/TerminationPolicy.h` | `should_terminate(state)` | 2 — `Horizon`, `RadiusBound` |
| `Integration::DerivativeFunc` | `integrators/Integrator.h` | `std::function<State(const State&)>` | — |

**[Inferred]** `DerivativeFunc` is the most architecturally significant of the five despite
being a type alias rather than a class. It is the boundary across which physics reaches
numerics, and because it is a plain callable, the numerics side has no way to learn
anything about the physics behind it.

### 2.5 Execution structure

**[Observed]** Entirely serial, synchronous, single-process, CPU-only, in-memory. One call
propagates one ray to completion and returns. No threads, no async, no I/O, no
device code. Verified by search: no `std::thread`, no OpenMP pragmas, no `fstream`, no
`chrono`, no random number generation anywhere in `physics/`.

### 2.6 Scientific data model

**[Observed]** One type dominates:

```cpp
struct State {
    Vector4d X;   // position 4-vector
    Vector4d U;   // tangent (velocity) 4-vector
    State operator+(const State& other) const;
    State operator*(double scalar) const;
};
```

`State` is the universal currency: produced by initial-condition builders, consumed and
produced by dynamics, advanced by the integrator, accumulated by the solver, inspected by
termination policies and observables. It is also the highest-degree node in the project's
dependency graph.

**[Inferred]** The two arithmetic operators are notable: they exist because RK4 forms
weighted sums of derivative evaluations. A numerical method's requirements are visible in
the shape of the central physics type — a small, contained instance of numerics influencing
the data model.

---

## 3. Dependency Graph

**CURRENT.**

### 3.1 The graph

**[Observed]** Verified from every `#include` in the tree. Arrows point from dependent to
dependency.

```text
   tests/null_geodesic_smoke.cpp
        │
        ├──────────────────────────────────────┐
        ↓                                      ↓
   simulation/SimulationConfig.h ◄────┐   validation/observables/
        │  (public API + config)      │        SchwarzschildObservables.h
        │                             │             │
        ↓                             │             ↓
   simulation/SimulationPipeline.cpp  │        core/GeodesicState.h
        │                             │             ↑
        ├──→ simulation/TrajectorySolver ─────────┐ │
        │         │                                │ │
        │         ├──→ geodesics/DynamicsModel.h ──┤ │
        │         ├──→ integrators/Integrator.h ───┤ │
        │         └──→ simulation/TerminationPolicy┤ │
        │                                          │ │
        ├──→ geodesics/GeodesicDynamics ──→ core/Metric.h
        │                                          │
        ├──→ metrics/SchwarzschildMetric ──────────┤
        │                                          │
        └──→ initial_conditions/                   │
                SchwarzschildInitialStateBuilders ─┘
                     │
                     └──→ simulation/SimulationConfig.h   ◄── back-edge
                          (build_custom needs GeodesicKind)

   integrators/RK4Integrator ──→ integrators/Integrator.h ──→ core/GeodesicState.h

   metrics/CoordinateChart  (no in-repo dependents)
   core/Units.h             (no dependents at all)
```

### 3.2 Central and leaf components

**[Observed]**

- **Most depended-upon:** `core/GeodesicState.h`. Reached, directly or transitively, by
  every compiled file. It is the graph's centre of mass.
- **Second:** `simulation/SimulationConfig.h`. Included by the pipeline, the builders, and
  every consumer — because it is simultaneously the configuration header, the result
  header, and the public API header.
- **Most depending:** `simulation/SimulationPipeline.cpp`, which includes eight headers
  spanning every subsystem.
- **Leaves:** `RK4Integrator.cpp` (physics-free), `SchwarzschildMetric.cpp`
  (numerics-free), `CoordinateChart.cpp` (unreferenced), `Units.h` (unreferenced).

### 3.3 Classification of dependencies

**Architecturally necessary.**

| Dependency | Why |
|---|---|
| Everything → `GeodesicState.h` | Shared vocabulary. Subsystems exchanging ray data need one definition of a ray. |
| `GeodesicDynamics` → `Metric.h` | The equation of motion requires curvature. Depends on the *interface*, not on `SchwarzschildMetric` — correct. |
| `TrajectorySolver` → the three ABCs | The loop composes a model, a method, and a stopping rule, and names none of their implementations. |
| `RK4Integrator` → `Integrator.h`, `GeodesicState.h` | Minimum possible: the contract and the data. |

**Convenient but avoidable.**

| Dependency | Why avoidable |
|---|---|
| Consumers → `SimulationConfig.h` for `SimulationResult` | Result and configuration are unrelated concepts sharing a header only because both were convenient to put there. |
| `SchwarzschildInitialStateBuilders` → `SimulationConfig.h` | **[Observed]** `build_custom` needs only the `GeodesicKind` enum, yet takes the full `SimulationConfig&` and pulls the entire configuration surface into a low-level builder. |

**Accidental.**

| Dependency | Evidence |
|---|---|
| `CoordinateChart.cpp` compiled into the library | **[Observed]** Listed in `CMakeLists.txt` line 17, zero callers. Compiled dead code carried over from extraction. |
| `Units.h` present, included by nothing | **[Observed]** Documentation held in a header. |

**Dangerous for future extensibility.**

| Dependency | Risk |
|---|---|
| `SimulationPipeline.cpp` → everything | **[Inferred]** It is the only place all subsystems meet, so it absorbs every cross-cutting change. Every new model, scenario, or method passes through this one file. |
| `SimulationConfig.h` as combined config + result + API header | **[Inferred]** Any change to configuration recompiles and potentially re-couples every consumer of results. |
| Builders → `SimulationConfig.h` (back-edge) | **[Inferred]** The one genuine layering violation: a low-level construction routine depends on the top-level configuration type. It makes the builders unusable outside the configured pipeline. |

### 3.4 Cycles

**[Observed]** No include cycles. The builders → `SimulationConfig.h` edge is a back-edge in
the *layering* — a lower layer depending on a higher one — but does not form a cycle,
because `SimulationConfig.h` does not include the builders.

### 3.5 Stable boundaries

**[Inferred]** Two boundaries are stable in the meaningful sense that neither side can
learn anything about the other:

1. **`DerivativeFunc`.** Numerics receives `std::function<State(const State&)>`. There is no
   channel through which the metric, the scenario, or the coordinate system could leak.
2. **`Metric::christoffel`.** Dynamics receives curvature coefficients indexed by tensor
   indices and position. It cannot discover which metric produced them.

**[Observed]** Both are demonstrated, not merely declared: `RK4Integrator.cpp` mentions no
physical concept, and `GeodesicDynamics.cpp` mentions no specific spacetime.

---

## 4. Subsystem Responsibilities

**CURRENT.**

---

### 4.1 Core Data — `physics/core/`

**Responsibility.** Define the vocabulary: `State`, `SchwarzschildParameters`, `MetricKind`,
`CoordinateChartKind`, physical constants, and the `Metric` interface.

**Inputs.** None. **Outputs.** Type definitions.
**Dependencies.** Eigen only. **Consumers.** Everything.
**Ownership.** No runtime state. `PhysicalConstants.h` holds `constexpr` values.

**Extension mechanism.** Adding fields to `State`, adding enum values, adding constants —
all of which modify a header that everything includes.

**Evaluation.**

- *Cohesion:* mixed. `State` and `Metric` are foundational; `SchwarzschildParameters` and
  the single-valued `MetricKind` are model-specific and sit oddly in a "core" layer.
- *Coupling:* correctly minimal downward — depends on nothing but Eigen.
- *Encapsulation:* weak. **[Observed]** `GeodesicState.h` line 4 has `using namespace Eigen;`
  at global scope in a public header, which propagates to every translation unit.
- *Abstraction quality:* `State` is a good domain object but a fixed one; the component set
  is a framework-wide commitment.
- *Composability:* high. `State`'s value semantics make it easy to pass and copy.
- *Maintainability:* good, given its size.

**Responsibility leakage.** **[Observed]** `State::operator+` and `operator*` exist to serve
RK4's weighted sums. A numerical method's requirement is encoded in the central physics type.
Contained, but it is numerics reaching into data.

---

### 4.2 Physics — `physics/metrics/`, `physics/geodesics/`

**Responsibility.** Supply spacetime curvature (`SchwarzschildMetric`) and turn it into an
equation of motion (`GeodesicDynamics`).

**Inputs.** A position 4-vector and tensor indices; a `State`.
**Outputs.** A Christoffel component; a derivative `State`.
**Dependencies.** `core/Metric.h`, `core/GeodesicState.h`, Eigen.
**Consumers.** `SimulationPipeline`, and `TrajectorySolver` via `DynamicsModel`.
**Ownership.** **[Observed]** `SchwarzschildMetric` owns one `double` (`rs`).
`GeodesicDynamics` holds a `const Metric&` — a non-owning reference, so the metric must
outlive it. **[Observed]** In practice the pipeline holds a `unique_ptr<Metric>` alive across
the call, so the lifetime assumption holds.

**Extension mechanism.** **Strong at this level.** A new metric subclasses `Metric` and
requires no modification anywhere. A new dynamics law subclasses `DynamicsModel`.

**Evaluation.**

- *Cohesion:* high. Each class does exactly one thing.
- *Coupling:* low. **[Observed]** `GeodesicDynamics` depends only on the `Metric` interface.
- *Abstraction quality:* good for its current scope, with one structural limit —
  **[Inferred]** all physics must be expressible as Christoffel symbols, which excludes
  refractive media and any non-geometric effect.
- *Composability:* high.

**Responsibility leakage.** **[Observed]** Schwarzschild-specific algebra appears *outside*
this subsystem, in `SimulationPipeline.cpp`: `1.5 * metric.mass` (photon sphere radius) at
line 37, and the null-constraint projection at lines 48–65 with `1.0 - rs / r` and
`std::sin(state.X[2])`. Physics knowledge has escaped the physics subsystem into
orchestration.

---

### 4.3 Numerics — `physics/integrators/`

**Responsibility.** Advance a state by one step.

**Inputs.** A `State`, a step size, a `DerivativeFunc`.
**Outputs.** The advanced `State`.
**Dependencies.** `core/GeodesicState.h` and `<functional>`. Nothing physical.
**Consumers.** `TrajectorySolver`, `SimulationPipeline` (via `default_integrator()`).
**Ownership.** **[Observed]** None. `RK4Integrator` is stateless.
`default_integrator()` returns a reference to a file-local `static const` instance —
immutable, safe to share, and consistent with the stateless design.

**Extension mechanism.** **Strong for new methods, missing for adaptive control.**
**[Observed]** `step(...)` returns `State` and nothing else: no error estimate, no accepted
step size, no rejection signal.

**Evaluation.**

- *Cohesion:* maximal.
- *Coupling:* the lowest in the project.
- *Encapsulation:* complete.
- *Abstraction quality:* **[Inferred]** the boundary is in exactly the right place with the
  wrong shape. It correctly excludes physics and incorrectly excludes diagnostics.
- *Composability:* high.

**Responsibility leakage.** None. This is the cleanest subsystem in the repository.

---

### 4.4 Simulation — `physics/simulation/`

This directory contains four distinct responsibilities and is evaluated as three components.

#### 4.4a `TrajectorySolver`

**Responsibility.** Drive the integration loop.

**Inputs.** Initial `State`, `DynamicsModel&`, `TerminationPolicy&`, `dt`, `max_steps`,
`Integrator&`, optional post-step callback.
**Outputs.** `std::vector<State>` from `solve`; a final `State` from `propagate`.
**Dependencies.** The three ABCs and `State`. **[Observed]** No concrete implementation is
named anywhere in `TrajectorySolver.{h,cpp}`.
**Ownership.** **[Observed]** Owns the history vector it builds and returns; holds
non-owning references to everything else.

**Extension mechanism.** Strong for the components it accepts; **[Observed]** the loop
structure itself is fixed — `for (int i = 0; i < max_steps; ++i)` with constant `dt`.

**Evaluation.** **[Inferred]** Genuinely generic and the best-designed component in the
project. Its two weaknesses are structural rather than local: it owns the progression
policy (fixed step, fixed count), which blocks adaptive integration, and `solve`
materializes the entire history before returning, which bounds ensemble scale by RAM.

**[Observed]** `propagate` — the history-free variant returning only the final state — has
zero callers. Its comment names the intended use: the optics and image-formation case.
Someone anticipated the ensemble requirement and built the right primitive; nothing yet
reaches it.

#### 4.4b `TerminationPolicy`

**Responsibility.** Decide when to stop.

**[Observed]** Two implementations. `HorizonTermination` is used; `RadiusBoundTermination`
has zero callers.

**Evaluation.** Clean and correctly abstracted. **[Inferred]** The limitation is expressive:
it answers a boolean question, so it can stop a ray but cannot report *where* a crossing
occurred — which is what Stage 2 geometry and image formation need.

**[Observed]** In the only executable, termination never fires: the run reports 50 001
states from `max_steps = 50000`, so the loop exhausted its budget rather than stopping on
physics. `max_steps` is the de facto stopping criterion.

#### 4.4c `SimulationPipeline` + `SimulationConfig`

**Responsibility.** Nominally, compose a run. Actually, six things: validate configuration,
construct the metric, construct initial conditions, compute Schwarzschild-specific
metadata, build a Schwarzschild-specific projection callback, and assemble the result.

**Inputs.** `SimulationConfig`, `SchwarzschildParameters`, one of four IC structs.
**Outputs.** `SimulationResult`.
**Dependencies.** Eight headers spanning every subsystem.
**Consumers.** All external users.
**Ownership.** **[Observed]** Creates `unique_ptr<Spacetime::Metric>`, passes it into
`integrate_schwarzschild` by value, and destroys it when that function returns. The
`GeodesicDynamics` reference to it is valid for exactly that scope.

**Extension mechanism.** **Missing.** **[Observed]** `require_spacetime` throws for any
non-Schwarzschild spacetime. Every function name in the anonymous namespace begins with or
contains `schwarzschild`.

**Evaluation.**

- *Cohesion:* low — six responsibilities in one translation unit.
- *Coupling:* the highest in the project.
- *Encapsulation:* the anonymous namespace hides the helpers, which is good practice and
  also means nothing inside can be reused or tested independently.
- *Abstraction quality:* **[Inferred]** this component *is* the abstraction failure. Below
  it, everything is generic; at it, everything becomes Schwarzschild.
- *Maintainability:* currently fine at ~120 lines. **[Predicted]** grows super-linearly with
  the product of models × scenarios.

**Responsibility leakage — the most significant in the project.**

1. **[Observed]** Physics into orchestration: the photon-sphere formula and the entire null
   projection are implemented here rather than in the physics subsystem.
2. **[Observed]** Configuration into construction: `build_custom` receives the full
   `SimulationConfig` to read one enum.
3. **[Observed]** Presentation into results: `characteristic_radius`, `horizon_radius`,
   `photon_sphere_radius`, `coordinate_chart`, and `name` are written on every run and read
   by nothing in the repository.

---

### 4.5 Initial Conditions — `physics/simulation/initial_conditions/`

**Responsibility.** Convert scenario parameters into a starting `State` satisfying the
appropriate normalization constraint.

**Inputs.** `SchwarzschildParameters` and one IC struct. **[Observed]** `build_custom`
additionally takes `SimulationConfig`.
**Outputs.** A `State`.
**Consumers.** `SimulationPipeline` only.
**Ownership.** None; pure functions.

**Extension mechanism.** **Conditional.** A new scenario requires a new struct, a new
builder, a new `Scenario` enum value, and a new `run_simulation` overload — four files.

**Evaluation.** Cohesive and readable. **[Inferred]** The conceptual limitation is that
"initial conditions" presumes a single ray with a hand-specified starting vector. Extended
sources and ray ensembles do not have initial conditions; they have geometry and brightness
distributions from which rays are *derived*.

**Responsibility leakage.** **[Observed]** The `SimulationConfig` dependency in
`build_custom`.

---

### 4.6 Observables — `physics/validation/observables/`

**Responsibility.** Compute conserved quantities and diagnostics: `conserved_energy`,
`conserved_angular_momentum`, `null_hamiltonian`, `null_hamiltonian_error`,
`critical_impact_parameter`.

**Inputs.** A `State` and `rs`. **Outputs.** Scalars.
**Consumers.** **[Observed]** Only `tests/null_geodesic_smoke.cpp`. Not part of
`sgl_physics` — header-only, absent from `CMakeLists.txt`.
**Ownership.** None; `inline` free functions.

**Extension mechanism.** **Strong.** New observables are new free functions with no
modification to anything.

**Evaluation.** **[Inferred]** The best-shaped subsystem for its purpose, and a good
precedent: analyses as free functions over results, depending on data but not on the
simulation machinery. **[Observed]** `null_hamiltonian_error` has no caller.

**Responsibility leakage.** None.

---

### 4.7 Cross-cutting evaluation

| Subsystem | Cohesion | Coupling | Abstraction | Extension |
|---|---|---|---|---|
| Core Data | Mixed | Minimal | Fixed but sound | Modify header |
| Physics | High | Low | Good, Christoffel-limited | **Strong** |
| Numerics | Maximal | Lowest | Right place, wrong shape | Strong / Missing |
| TrajectorySolver | High | Low | Genuinely generic | Strong |
| TerminationPolicy | High | Low | Boolean-limited | Strong |
| **Pipeline + Config** | **Low** | **Highest** | **Absent** | **Missing** |
| Initial Conditions | High | Medium | Single-ray-limited | Conditional |
| Observables | High | Minimal | Good | **Strong** |

**[Inferred]** The pattern is unambiguous. Seven of eight subsystems are well-formed. One —
the pipeline plus its configuration header — concentrates all coupling, all responsibility
leakage, and all extension failure. That is architecturally fortunate: the problem is
localized rather than diffuse.

---

## 5. Scientific Data Flow

**CURRENT.** Traced through the implementation, not assumed.

### 5.1 Actual flow

**[Observed]** The idealized chain in the phase prompt does not survive contact with the
code. Three stages are absent (no problem construction, no observation, no visualization),
and analysis exists but is not part of the flow — the consumer performs it after the fact.

```text
[1] COMPILE-TIME CONFIGURATION            tests/null_geodesic_smoke.cpp:14-28
    SimulationConfig, SchwarzschildParameters, NullScatterInitialConditions
    constructed by field assignment in C++ source
         │  three values by copy
         ↓
[2] PUBLIC ENTRY                          SimulationPipeline.cpp:104
    run_simulation(config, metric, initial)
    overload resolution on IC type selects the scenario path
         │
         ↓
[3] VALIDATION                            require_spacetime()  :17
    throws unless spacetime == Schwarzschild and scenario matches
         │
         ↓
[4] MODEL CONSTRUCTION                    make_schwarzschild_metric()  :27
    unique_ptr<Metric> ← new SchwarzschildMetric(params.mass)
         │  ownership created here; destroyed at :84
         ↓
[5] INITIAL STATE                         build_null_scatter()
    (r0, impact_parameter) → State{X, U}  with null constraint applied
         │  State returned by value
         ↓
[6] COMPOSITION                           integrate_schwarzschild()  :68
    GeodesicDynamics dynamics(*metric_impl);      ← non-owning ref
    HorizonTermination policy(mass, safety);
    post_step = make_schwarzschild_post_step(...)  ← physics as a lambda
         │
         ↓
[7] INTEGRATION LOOP                      TrajectorySolver::solve()  :5
    history.reserve(min(max_steps, 100000))
    derivative = [&dynamics](s){ return dynamics.compute_derivative(s); }
    for i in 0..max_steps:
        if policy.should_terminate(current): break
        current = integrator.step(current, dt, derivative)   ← RK4
        if post_step: post_step(current, i)                  ← MUTATES
        history.push_back(current)                           ← COPY
         │
         │  inner cost per accepted step:  4 derivative evaluations
         │  × 64 virtual christoffel calls = 256 virtual calls,
         │  reached through 4 std::function indirections
         ↓
[8] RESULT ASSEMBLY                       integrate_schwarzschild()  :75-83
    result.history   = <moved vector>
    result.characteristic_radius = mass      ← never read
    result.name                  = config.name  ← never read
    result.spacetime             = ...          ← never read
    result.metadata              = ...          ← never read
         │  SimulationResult returned by value
         ↓
[9] CONSUMER-SIDE ANALYSIS                null_geodesic_smoke.cpp:36-50
    first = history.front(); last = history.back()
    conserved_energy / conserved_angular_momentum on each
    compare relative drift against 1e-3
         │
         ↓
[10] OUTPUT                               two std::cout lines, exit code
```

### 5.2 Transition analysis

| Transition | Representation | Ownership | Copy / mutate | Coupling |
|---|---|---|---|---|
| 1 → 2 | Three structs | Caller | Copied (const ref, then read) | Caller must know all three types and the scenario/IC pairing |
| 2 → 3 | Same | — | Read only | Runtime type check standing in for a type-system guarantee |
| 3 → 4 | `unique_ptr<Metric>` | Pipeline, scoped to the call | Moved | Pipeline names the concrete class |
| 4 → 5 | `State` | Value | Returned by value | Builder knows the metric's parameterization |
| 5 → 6 | Refs to dynamics/policy | Stack-local in `integrate_schwarzschild` | — | **[Observed]** `GeodesicDynamics` holds `const Metric&`; validity depends on the `unique_ptr` outliving it, which it does |
| 6 → 7 | Three abstract refs + a lambda | Non-owning | — | **The clean boundary.** Solver names no concrete type |
| 7 (loop) | `State` | Solver owns history | **Copied** into the vector each step; **mutated** in place by `post_step` | The post-step callback mutates state mid-loop |
| 7 → 8 | `vector<State>` | Moved into result | Moved | — |
| 8 → 9 | `SimulationResult` | Returned to caller by value | Moved / copy-elided | Consumer must know `history` is the payload and the other four fields are inert |
| 9 → 10 | Scalars | — | — | Analysis is entirely consumer-side |

### 5.3 Is scientific data independent of presentation and execution?

**Presentation: no.** **[Observed]** `SimulationResult` carries `characteristic_radius`,
`name`, `spacetime`, and a `SimulationMetadata` with `horizon_radius`,
`photon_sphere_radius`, and `coordinate_chart`. All are written on every run. None is read
anywhere in the repository. **[Inferred]** These are display parameters — where to place a
camera, what radius to draw, what to label the trajectory — that survived the removal of the
renderer they were built for. The result type still has the shape a renderer imposed.

**Execution: partly.** **[Observed]** `State` contains no execution concern — no allocator,
no device handle, no index. **[Inferred]** But the *aggregate* representation does: results
are a `std::vector` in host memory, materialized in full before return, one per ray. The
element is execution-neutral; the container is not.

### 5.4 Two structural observations

**[Observed]** The one point of mutation in the entire flow is the `post_step` callback,
which takes `State&` and modifies `U[0]` in place to re-impose the null condition. Everything
else is value semantics. This is a clean design with one deliberate exception, and the
exception is where Schwarzschild-specific physics lives inside orchestration.

**[Observed]** The `Gamma != 0.0` guard in `GeodesicDynamics.cpp` skips the multiply for
zero Christoffel components but the virtual call has already happened. All 64 calls occur
per derivative evaluation regardless of sparsity.

---

## 6. Current Extension Points

**CURRENT.** Every meaningful extension point, classified.

| # | Extension point | Class | Evidence |
|---|---|---|---|
| 1 | New `Metric` (solver-level) | **Strong** | Subclass `Metric`, pass to `GeodesicDynamics`, call `TrajectorySolver::solve`. No file modified. |
| 2 | New `Metric` (via public API) | **Missing** | `require_spacetime` throws; `MetricKind` has one enumerator; `make_schwarzschild_metric` is hard-coded. Requires ≥6 files. |
| 3 | New `DynamicsModel` | **Strong** | Subclass and pass. `TrajectorySolver` names no implementation. |
| 4 | New `Integrator` | **Strong** at solver level, **Missing** via API | `solve` takes `const Integrator&`; `run_simulation` always passes `default_integrator()`. |
| 5 | Adaptive integration | **Missing** | `step(...) → State` has no error-estimate channel; solver owns a fixed loop. |
| 6 | New `TerminationPolicy` | **Strong** | Subclass and pass. `RadiusBoundTermination` demonstrates a second implementation is possible. |
| 7 | Sub-step event localization | **Missing** | The interface is boolean; no mechanism to report a crossing point. |
| 8 | New scenario / initial conditions | **Conditional** | New struct + builder + enum value + `run_simulation` overload. |
| 9 | New observable | **Strong** | Free functions in a header; nothing to modify. |
| 10 | Post-step processing | **Weak** | The hook exists (`std::function<void(State&,int)>`) but is constructed only by `make_schwarzschild_post_step` and cannot be supplied through `run_simulation`. |
| 11 | New coordinate chart | **Weak** | `CoordinateChart` utilities exist with zero callers; `X[1]`/`X[2]` are indexed as radius and polar angle across six components; `CoordinateChartKind` has one enumerator. |
| 12 | Alternative state representation | **Missing** | `State` is a concrete struct with fixed members, depended on by everything. |
| 13 | Alternative precision | **Missing** | `double`, `Vector4d`, `Matrix4d` hard-coded throughout. |
| 14 | Ray ensembles | **Missing** | `run_simulation` is single-ray by signature; `solve` allocates one history per call. |
| 15 | Alternative result types | **Missing** | `SimulationResult` is the only output type and is trajectory-shaped. |
| 16 | Streaming / large results | **Missing** | `solve` materializes the full vector before returning. |
| 17 | Parallel or device execution | **Missing** | No execution abstraction exists. |
| 18 | Runtime configuration | **Missing** | Configuration is compile-time C++ only. No file format, no CLI. |
| 19 | Parameter sweeps | **Missing** | No experiment concept. |
| 20 | Validation cases | **Missing** | One hand-written `main()`; no `enable_testing()` or `add_test()` in `CMakeLists.txt`. |
| 21 | Visualization | **Missing** | Absent by design. |

### The pattern

**[Inferred]** Six Strong extension points, all at the solver level, all reached by calling
`TrajectorySolver::solve` directly. Every Missing extension point is at or above the public
API. The dividing line is exact: **the architecture is extensible below `run_simulation` and
closed at it.**

Extension points 1 and 2 are the same capability at two levels — Strong internally, Missing
externally. That single contrast characterizes the project's architecture better than any
other observation in this review.

---

## 7. Architectural Strengths

**CURRENT.** Each claim names the specific evidence that establishes it.

### 7.1 Numerics is genuinely independent of physics

**[Observed]** `physics/integrators/RK4Integrator.cpp` and `Integrator.h` reference no
physical concept whatsoever. The interface is:

```cpp
using DerivativeFunc = std::function<State(const State&)>;
virtual State step(const State&, double dt, const DerivativeFunc&) const = 0;
```

**[Observed]** `TrajectorySolver.cpp` names no concrete class — it accepts
`const Dynamics::DynamicsModel&`, `const TerminationPolicy&`, and
`const Integration::Integrator&`, and wraps the dynamics in a lambda before use.

**[Inferred]** This is what makes multiple integrators and cross-method validation
achievable as additions rather than rewrites. It is the hardest property in the target
architecture, and SGL has it today.

### 7.2 Physics is independent of the specific spacetime *at the dynamics level*

**[Observed]** `GeodesicDynamics.cpp` performs a 4×4×4 loop over `metric_.christoffel(...)`
through the abstract `Metric&`. It never mentions Schwarzschild. **[Observed]**
`SchwarzschildMetric.cpp` contains no reference to integration, step size, or solver.

**[Inferred]** A second gravitational model needs no change to the equation of motion.

### 7.3 Ownership is simple, explicit, and correct

**[Observed]**
- Exactly one heap allocation in the whole flow: `make_unique<SchwarzschildMetric>`, whose
  lifetime is a single function scope.
- `GeodesicDynamics` holds `const Metric&`, valid for that scope.
- `State` is a value type, copied freely.
- `RK4Integrator` is stateless; `default_integrator()` returns a reference to a file-local
  `static const` instance.
- No shared ownership, no reference counting, no observer registration, no back-pointers.

**[Inferred]** There is no lifetime puzzle anywhere. For a scientific codebase this is
unusual and valuable: it makes reasoning about correctness a local activity.

### 7.4 Determinism by construction

**[Observed]** Verified by search across `physics/`: no random number generation, no
threading, no clock or time dependence, no I/O, no environment reads, no global mutable
state. Fixed step size, fixed iteration count.

**[Inferred]** Given identical inputs and binary, output is bit-identical. Determinism is
the hard half of reproducibility, and it is easy to lose accidentally later — the first
parallel reduction removes it. Having it now is a real asset. Note the asymmetry, though:
SGL is deterministic but not *reproducible*, because nothing records what was run
(Section 8.4).

### 7.5 The numerical implementation is demonstrably correct

**[Observed]** Verified during this review with a clean Release build:
`|dE/E| = 1.0103e-14`, `|dL/L| = 3.72693e-15` over 50 000 steps.

**[Inferred]** Drift at the level of double-precision round-off accumulation is strong
evidence the geodesic equation, the metric, and RK4 are implemented correctly. This is
evidence about *integration quality* specifically — it is not evidence of physical
correctness, since a wrong trajectory can still conserve these quantities. That distinction
is developed in Section 8.

### 7.6 Minimal dependency footprint

**[Observed]** `vcpkg.json` declares exactly one dependency: `eigen3`, a header-only
library. No graphics, no serialization, no threading library, no test framework.

**[Inferred]** For a project expected to run on HPC systems years from now, near-zero
dependency risk is a genuine architectural asset, and it was bought deliberately by
excluding Penrose's rendering stack.

### 7.7 The right primitive for ensembles already exists

**[Observed]** `TrajectorySolver::propagate` returns only the final state and accumulates no
history. Its comment names the optics and image-formation use case. It has zero callers.

**[Inferred]** Someone anticipated that ensemble-scale work cannot retain per-ray history
and built the primitive. It is unwired, so it provides no capability today — but the
recognition is recorded in the code.

### 7.8 Analysis is already correctly shaped

**[Observed]** `SchwarzschildObservables.h` is header-only `inline` free functions,
excluded from the library target, consuming `State` and returning scalars.

**[Inferred]** Analyses as pure functions over results — depending on the data model but not
on the simulation machinery — is exactly the target relationship. It is a precedent worth
preserving.

---

## 8. Hidden Assumptions

**CURRENT.** Each entry gives: where it appears, why it exists, whether it is necessary,
what invalidates it, and the severity of the resulting risk.

Severity is rated by the cost of removing the assumption *later*, not by whether it is
wrong today. Every one of these is a reasonable choice for the current scope.

---

### 8.1 Physics assumptions

#### A1 — Spacetime is Schwarzschild

- **Where.** **[Observed]** `MetricKind` has one enumerator;
  `require_spacetime` throws otherwise; `make_schwarzschild_metric` is hard-coded; every
  helper in the pipeline's anonymous namespace is named `*schwarzschild*`;
  `SchwarzschildParameters` is the only parameter type.
- **Why.** Correct scope for Stage 1, and the extraction deliberately dropped Kerr.
- **Necessary?** No. The `Metric` interface already generalizes it; only the public path
  hard-codes it.
- **Invalidated by.** `C24` — solar `J₂`, oblateness, rotation. The real Sun is not exactly
  Schwarzschild, and SGL's precision goals will eventually notice.
- **Severity: Medium.** The abstraction exists; the wiring does not.

#### A2 — All physics is expressible as Christoffel symbols

- **Where.** **[Observed]** `GeodesicDynamics::compute_derivative` computes
  `a^μ = -Γ^μ_αβ U^α U^β` and nothing else. `Metric` exposes only `christoffel`.
- **Why.** True for pure vacuum geodesic motion.
- **Necessary?** No — it conflates "the gravitational field" with "the equation of motion."
- **Invalidated by.** `C24` solar corona plasma refraction (a real, wavelength-dependent SGL
  effect that adds non-Christoffel terms), `C25`, `C26`.
- **Severity: High.** **[Inferred]** There is no channel through which a non-geometric
  effect can enter the derivative.

#### A3 — Spacetime is static

- **Where.** **[Observed]** `christoffel(mu, alpha, beta, X)` takes position only;
  `compute_derivative(const State&)` is autonomous — no time, epoch, or context argument
  anywhere in the propagation path.
- **Why.** Schwarzschild is static; the signature honestly reflects the model.
- **Necessary?** For a static solar model, yes.
- **Invalidated by.** `C11b` time-dependent fields, rotating models.
- **Severity: Medium.** Signature change across the physics interfaces, but not near-term:
  observer *motion* does not require it — that is geometry, not field dynamics.

#### A4 — A photon is fully described by position and tangent

- **Where.** **[Observed]** `State { Vector4d X; Vector4d U; }`. Verified absent from
  `physics/`: any concept of phase, amplitude, intensity, wavelength, or polarization.
- **Why.** Correct and complete for geometric optics.
- **Necessary?** No — it is a geometric-optics assumption presented as a data definition.
- **Invalidated by.** `C25` wavelength dependence, `C26` wave optics. At the solar focal
  line diffraction determines achievable resolution, so this is core SGL physics rather than
  a refinement.
- **Severity: High.** `State` is the highest-degree node; changing it touches everything.

#### A5 — Sources and observers are points, and neither exists

- **Where.** **[Observed]** No source, observer, or lens body anywhere. The lens is a single
  `double`. `Constants::solar_radius_m` exists and is never used. Impact parameter is a
  caller-supplied input rather than a derived source–lens–observer relationship.
- **Why.** Stage 1 does not need them.
- **Necessary?** No.
- **Invalidated by.** `C3`, `C4`, `C10`, `C11` — essentially all of SGL's actual science.
- **Severity: High** for the project's purpose, **Low** for existing code, since nothing has
  to be undone. This is the gap between "geodesic propagation code" and "SGL framework."

#### A6 — Propagation is an initial-value problem

- **Where.** **[Observed]** All four builders answer "given a starting state, where does the
  ray go?" `TrajectorySolver` marches forward from an initial state.
- **Why.** It is the natural framing for Stage 1.
- **Necessary?** No.
- **Invalidated by.** `C4`. SGL's characteristic question — *which* ray connects this source
  to this observer — is a two-point boundary-value problem, structurally different.
- **Severity: Medium.** **[Inferred]** A BVP solver can be built above the existing IVP
  machinery, but nothing currently expresses the problem.

---

### 8.2 Numerical assumptions

#### A7 — One integration method, fixed step, fixed count

- **Where.** **[Observed]** `run_simulation` always passes `default_integrator()`;
  `SimulationConfig` exposes `dt` and `max_steps`; the solver loop is
  `for (i = 0; i < max_steps; ++i)` with constant `dt`.
- **Why.** RK4 at small fixed steps is accurate and simple, as the 10⁻¹⁴ drift shows.
- **Necessary?** No. The `Integrator` interface already exists.
- **Invalidated by.** `C13`, `C14`, `C17`.
- **Severity: Medium** for alternate methods, **High** for adaptive — see A8.

#### A8 — A step produces only a state

- **Where.** **[Observed]** `virtual State step(const State&, double, const DerivativeFunc&) const`.
- **Why.** Sufficient for fixed-step methods.
- **Necessary?** No.
- **Invalidated by.** `C14`, `C15`, `C17`. Adaptive integration requires the method to
  report a local error estimate and the driver to accept, reject, or resize.
- **Severity: High.** **[Inferred]** The boundary is correctly placed and wrongly shaped, and
  every method implementation written before this contract changes must be revised after.

#### A9 — `max_steps` is a meaningful physical bound

- **Where.** **[Observed]** `history.reserve(std::min(max_steps, 100000))`; the loop bound;
  and the smoke test producing exactly 50 001 states from `max_steps = 50000` — the run ended
  by exhausting its budget, not by any physical event.
- **Why.** A safety valve against non-terminating integrations.
- **Necessary?** As a safety valve, yes. As the de facto stopping criterion, no.
- **Invalidated by.** `C14` — under adaptive stepping, step count and physical extent
  decouple entirely.
- **Severity: Medium.** **[Inferred]** Users currently tune `dt` and `max_steps` to reach a
  desired physical extent. That coupling silently breaks when steps stop being uniform.

#### A10 — Double precision, 4 dimensions, fixed layout

- **Where.** **[Observed]** `double`, `Vector4d`, `Matrix4d` throughout; no scalar type
  alias, no template parameter.
- **Why.** Correct for 4-dimensional spacetime on CPU.
- **Necessary?** Dimensionality yes; precision no.
- **Invalidated by.** `C15` — extended precision for reference calculations, reduced for GPU.
- **Severity: Low–Medium.** The requirement is a nameable scalar type, not full templating.

#### A11 — Coordinate meaning is conventional, not explicit

- **Where.** **[Observed]** `X[1]` is radius and `X[2]` is polar angle, replicated across
  `SchwarzschildMetric`, the builders, the observables, `HorizonTermination`, and the
  projection lambda. `CoordinateChart` exists with zero callers.
- **Why.** With one chart, the convention is harmless and readable.
- **Necessary?** No.
- **Invalidated by.** `C16`, `C9`.
- **Severity: Medium–High.** **[Predicted]** A second chart under this convention produces
  *silently wrong numbers* rather than compile or runtime errors. Severity is driven by
  failure mode, not by effort.

---

### 8.3 Data assumptions

#### A12 — The output is a trajectory

- **Where.** **[Observed]** `solve` returns `std::vector<State>`; `SimulationResult::history`
  is the only substantive field; the test consumes `front()` and `back()`.
- **Why.** Correct for Stage 1.
- **Necessary?** No.
- **Invalidated by.** `C7`, `C8`. An image is aggregated over a population; it is not a
  special case of one ray's path.
- **Severity: High.** **[Inferred]** This is the framework's central invariant, and SGL's
  actual scientific output is on the other side of it.

#### A13 — Full history is always retained, in memory, per ray

- **Where.** **[Observed]** `history.reserve(...)`, `push_back` every step, return by value.
  No streaming, no I/O anywhere.
- **Why.** Simple and correct for one ray.
- **Necessary?** No — `propagate` already demonstrates the alternative.
- **Invalidated by.** `C5`, `C18`, `C21`. **[Predicted]** 10⁶ rays × 10⁴ steps × 64 bytes is
  of order 10¹² bytes.
- **Severity: High.**

#### A14 — One call, one ray, one result

- **Where.** **[Observed]** All four `run_simulation` overloads take one IC struct and return
  one `SimulationResult`.
- **Why.** Matches the current scope.
- **Necessary?** No.
- **Invalidated by.** `C5`, `C6`, `C18`, `C19`, `C20`.
- **Severity: High.** **[Inferred]** This determines whether scaling is an extension or a
  retrofit of every path.

#### A15 — Results need no identity or provenance

- **Where.** **[Observed]** `SimulationConfig::name` is copied to `SimulationResult::name`
  and never read. No version stamping, no config capture, no serialization of any kind.
- **Why.** In-process consumption needs none.
- **Invalidated by.** `C22`, `C23`, `C17`.
- **Severity: Medium.** Additive to fix; costly in lost science if left until many results
  exist.

---

### 8.4 Execution assumptions

#### A16 — Serial, synchronous, single-process, CPU

- **Where.** **[Observed]** No threading, no async, no device code, no MPI. `CMakeLists.txt`
  declares one static CPU library.
- **Why.** Appropriate for the current scale.
- **Necessary?** No, but nothing needs undoing — this is absence, not obstruction.
- **Invalidated by.** `C18`, `C19`, `C20`.
- **Severity: Low by itself.** The risk lies in A17 and A14, not here.

#### A17 — Runtime polymorphism in the innermost loop is free

- **Where.** **[Observed]** 64 virtual `christoffel` calls per derivative evaluation, 4
  derivative evaluations per RK4 step, reached through `std::function` — 256 virtual calls
  plus 4 indirect calls per accepted step.
- **Why.** It is the clearest way to express the abstraction on CPU, and at current scale
  the cost is irrelevant.
- **Necessary?** No. Physics can be defined as plain functions with polymorphism as an
  adapter over them.
- **Invalidated by.** `C19`. Virtual dispatch through host-constructed objects does not
  execute device-side.
- **Severity: High, and time-dependent.** **[Inferred]** Nearly free to change now with one
  implementation per interface; increasingly expensive as implementations accumulate.

---

### 8.5 Visualization assumptions

#### A18 — Presentation metadata belongs in the scientific result

- **Where.** **[Observed]** `SimulationResult` carries `characteristic_radius`, `name`,
  `spacetime`; `SimulationMetadata` carries `horizon_radius`, `photon_sphere_radius`,
  `coordinate_chart`. All written every run, none read anywhere.
- **Why.** **[Inferred]** Phase 1 identified the former consumer: Penrose's
  `SimulationTrajectoryAdapter.h`, which converted `SimulationResult` into renderer scene
  data. The renderer was removed; the shape it imposed was not.
- **Necessary?** No — nothing reads these fields.
- **Invalidated by.** `C27`, and by the general principle that results must not be shaped by
  consumers.
- **Severity: Low as cost, High as evidence.** **[Inferred]** The fields are inert. What
  matters is the demonstrated precedent: this result type has previously absorbed
  presentation concerns, and it did so invisibly enough to survive the removal of the thing
  that required it.

---

### 8.6 Severity summary

| Severity | Assumptions |
|---|---|
| **High** | A2 (Christoffel-only physics), A4 (ray = position + tangent), A5 (no SGL geometry), A8 (step returns only a state), A12 (output is a trajectory), A13 (full history in memory), A14 (one ray per call), A17 (virtual dispatch in inner loop) |
| **Medium** | A1, A3, A6, A7, A9, A10, A11, A15 |
| **Low** | A16, A18 |

**[Inferred]** The eight High-severity assumptions concentrate in three places: the
definition of a ray (A2, A4), the definition of a result (A12, A13, A14), and the shape of
the step and dispatch contracts (A8, A17). Plus A5, which is an absence rather than an
obstruction. This clustering is the reason the current-to-target gap is tractable.

---

## 9. Architectural Weaknesses

**CURRENT.** Architecturally significant issues only. Implementation defects, style, and
naming are out of scope.

---

### W1 — No representation of the scientific question

```text
Problem
    ↓  SimulationConfig fuses spacetime selection, scenario selection, dt,
       max_steps, solver flags, and a display name into one struct that is
       also the public API header and the result header.
Root Cause
    ↓  There is no architectural home for "what is being computed," so the
       only available home is the configuration object and the pipeline
       that reads it.
Architectural Consequence
    ↓  Every new scientific question becomes a new enum value, a new field,
       a new overload, and a new branch in one central file. The scientific
       question and the numerical method are inseparable in the type system.
Future Impact
    [Predicted] Every Stage 2+ capability — lens geometry, sources,
    observers, sweeps — has nowhere to go but the centre. C3, C4, C22.
```

**[Inferred]** This is the root weakness. Most others below are its symptoms.

---

### W2 — Orchestration contains physics

```text
Problem
    ↓  [Observed] SimulationPipeline.cpp implements the photon-sphere radius
       (1.5 * mass) and the full null-constraint projection (1 - rs/r,
       sin(X[2]), U[0] rescaling) inside its anonymous namespace.
Root Cause
    ↓  The post-step hook and metadata are model-specific, and no physics
       component owns "corrections applied during propagation."
Architectural Consequence
    ↓  Schwarzschild knowledge exists in two places. A second metric must
       either reimplement projection in the pipeline or restructure it.
       The projection is untestable and unreusable in isolation.
Future Impact
    [Predicted] The pipeline accumulates one projection and one metadata
    builder per model — growth in models × scenarios. C24.
```

---

### W3 — The result type is trajectory-shaped and consumer-shaped

```text
Problem
    ↓  [Observed] SimulationResult holds vector<State> history plus four
       fields that are written every run and read nowhere.
Root Cause
    ↓  The type was designed when the consumer was a renderer, and the
       renderer's needs were satisfied by widening the physics result.
Architectural Consequence
    ↓  There is one output type and it is a path. Anything that is not a
       path — an image, an intensity field, a convergence table — has no
       representation. Results are also bounded by RAM.
Future Impact
    [Predicted] C7 image formation and C21 large datasets are both blocked
    at the type level, not the algorithm level.
```

---

### W4 — The step contract has no diagnostic channel

```text
Problem
    ↓  [Observed] step(state, dt, derivative) → State.
Root Cause
    ↓  Designed for fixed-step methods, where there is nothing else to say.
Architectural Consequence
    ↓  The abstraction is clean and cannot express the family of methods
       the project needs. Step-size policy lives in the solver loop rather
       than with the method.
Future Impact
    [Predicted] C14 and C17 require changing this contract, and every
    Integrator written before the change must be revised after it. The
    cost grows with the number of implementations.
```

**[Inferred]** Reconciling Phases 2 and 3: Phase 2 called this boundary strong, Phase 3
called adaptive integration contradicted. Both are correct. The boundary is in the right
*place* and has the wrong *shape*.

---

### W5 — Single ray is the base case, not the degenerate case

```text
Problem
    ↓  [Observed] run_simulation propagates one ray; solve allocates one
       history vector per call.
Root Cause
    ↓  The scope was one trajectory, and the API was shaped to it exactly.
Architectural Consequence
    ↓  Ensembles must be built by looping over an API that allocates,
       propagates, and returns per ray. Batching, vectorization, and device
       execution have no entry point.
Future Impact
    [Predicted] C5, C6, C18, C19, C20 all become retrofits of the public
    path rather than additions beside it.
```

---

### W6 — Physics is defined polymorphically rather than functionally

```text
Problem
    ↓  [Observed] 256 virtual calls per accepted RK4 step, reached through
       std::function.
Root Cause
    ↓  Runtime polymorphism is the idiomatic C++ expression of a swappable
       model, and it is genuinely the clearest form on CPU.
Architectural Consequence
    ↓  The physics cannot execute anywhere that virtual dispatch through
       host-constructed objects is unavailable.
Future Impact
    [Predicted] C19 requires re-expressing the physics rather than adding
    an execution strategy beside it. Cheapest to address now, while one
    implementation exists per interface.
```

---

### W7 — Configuration leaks downward into construction

```text
Problem
    ↓  [Observed] build_custom takes const SimulationConfig& to read one
       enum (GeodesicKind).
Root Cause
    ↓  Passing the whole config is easier than passing what is needed.
Architectural Consequence
    ↓  A low-level builder depends on the top-level configuration type —
       the one genuine layering back-edge in the project. Builders cannot
       be used outside the configured pipeline.
Future Impact
    [Predicted] Establishes the pattern by which SimulationConfig becomes
    a universal parameter bag reachable from every layer. C9, C15, C22.
```

---

### W8 — Coordinate meaning is conventional, not explicit

```text
Problem
    ↓  [Observed] X[1] = radius, X[2] = polar angle, assumed independently
       in six components. CoordinateChart utilities exist, unused.
Root Cause
    ↓  With one chart the convention is invisible and costs nothing.
Architectural Consequence
    ↓  Six components share an unenforced contract. A mismatch is
       undetectable by any mechanism.
Future Impact
    [Predicted] A second chart yields silently wrong physics rather than
    an error. Severity is driven by failure mode, not effort. C16.
```

---

### W9 — Validation cannot distinguish drift from wrongness

```text
Problem
    ↓  [Observed] One executable checks conservation of E and L against a
       1e-3 threshold. No analytical reference exists; Penrose's
       analytical_freefall_time was dropped in extraction and not replaced.
       No deflection-angle check against the weak-field 4GM/(c²b).
       No enable_testing() or add_test() in CMakeLists.txt.
Root Cause
    ↓  Conservation checking was inherited; analytical validation was not
       part of the extracted set.
Architectural Consequence
    ↓  Conserved quantities are functions the constraint structure
       preserves regardless of trajectory correctness. An integrator can
       conserve E and L to 1e-14 while computing the wrong deflection.
       The current posture detects drift and cannot detect systematically
       wrong-but-conserved physics.
Future Impact
    [Predicted] Without analytical benchmarks, no numerical claim about
    SGL predictions is supported, and no convergence study has a target.
    C2, C17.
```

**[Inferred]** This is the weakness with the greatest gap between its architectural cost
(low — validation is a pure consumer, addable anytime) and its scientific cost (high — it is
the credibility mechanism for a framework whose outputs cannot be checked against a real
observation).

---

### W10 — Dead and detached components

```text
Problem
    ↓  [Observed] CoordinateChart.cpp compiled with zero callers;
       RadiusBoundTermination, TrajectorySolver::propagate, and
       null_hamiltonian_error each with zero callers; Units.h included
       by nothing.
Root Cause
    ↓  Extraction retained components whose consumers stayed in Penrose.
Architectural Consequence
    ↓  The compiled artifact overstates the wired architecture. Any reading
       of capability from file listings is misleading — which is precisely
       why this investigation traced calls rather than names.
Future Impact
    [Predicted] Low direct risk. The relevant point is that these are
    unexercised: propagate and RadiusBoundTermination have never run.
```

---

### 9.1 Weakness ranking

| Rank | Weakness | Basis |
|---|---|---|
| 1 | W1 no problem representation | Root cause of W2, W7, and most extension failures |
| 2 | W3 result type | Blocks image formation and scaling at the type level |
| 3 | W5 single-ray base case | Determines extension vs retrofit for all scaling |
| 4 | W4 step contract | Blocks adaptive integration; cost grows with implementations |
| 5 | W6 polymorphic physics | Blocks device execution; cheapest to fix now |
| 6 | W9 validation | Low architectural cost, high scientific cost |
| 7 | W2 physics in orchestration | Grows with models × scenarios |
| 8 | W8 implicit charts | Silent-failure mode raises severity above effort |
| 9 | W7 config leakage | Precedent risk more than current damage |
| 10 | W10 dead components | Clarity, not capability |

---

## 10. Future Stress Test Results

**CURRENT architecture evaluated against FUTURE REQUIREMENTS (Phase 4).**

Classification vocabulary, in increasing order of cost:

| Classification | Meaning |
|---|---|
| **Extension** | New code against existing interfaces. No existing file modified. |
| **Interface Extension** | An existing interface gains capability; implementations follow. |
| **Modification** | Existing components must be changed internally. |
| **Refactor** | Structure must be reorganized across components. |
| **Fundamental Redesign** | A central architectural commitment must be replaced. |

### 10.1 Capability table

| Future Capability | Natural Location | Current Support | Architectural Friction | Required Change | Classification | Risk |
|---|---|---|---|---|---|---|
| Additional gravitational models | Physics | Interface exists; public path closed | `MetricKind` single-valued; `require_spacetime` throws; per-model metadata and projection in pipeline | Wire model selection; move projection into physics | **Modification** | Med |
| Non-vacuum / plasma effects | Physics | None | All physics routed through `christoffel`; no channel for extra terms | Separate propagation law from field model | **Refactor** | High |
| Multiple numerical integrators | Numerics | Interface exists; API hard-wires RK4 | `run_simulation` always passes `default_integrator()` | Expose method selection | **Interface Extension** | Low |
| Adaptive integration | Numerics | None | `step` returns only `State`; solver owns fixed loop | Step contract gains error + control | **Interface Extension** | Med |
| Precision / tolerance control | Numerics + core | None | `double` hard-coded; `dt` substitutes for tolerance | Nameable scalar; tolerance in method params | **Modification** | Low |
| Alternative coordinate charts | Core + physics | Utilities exist, unused | `X[1]`/`X[2]` convention in six components | Chart attached to data | **Modification** | Med |
| Convergence studies | Validation | None | No refinement mechanism; no reference solutions | Validation + sweep layers | **Extension** | Low |
| Analytical validation | Validation | None | No analytical references remain | Reference library as consumer | **Extension** | Low |
| SGL lens/source/observer geometry | Physics (domain) | None | No geometry concepts; impact parameter is an input, not derived | New geometry concepts | **Extension** | Low |
| Connection (source→observer) solving | Numerics over physics | None | Everything is initial-value framed | BVP method above IVP machinery | **Extension** | Med |
| Large photon ensembles | New layer | None | One ray per call; one history per ray | Ensemble as unit of work | **Refactor** | High |
| Ray bundles | Data + ensemble | None | `State` has no neighbour relationships | Ray attributes + ensemble topology | **Refactor** | High |
| Image formation | New observation layer | None | Universal output is a trajectory | Result generalization + new layer | **Refactor** | High |
| PSF modeling | Observation | None | No image-domain data product | Depends on image formation | **Extension**¹ | Med |
| Extended sources | Physics (source) | None | "Initial conditions" presumes one hand-specified ray | Source model + sampling | **Extension** | Low |
| Dynamic observers / spacecraft | Physics (geometry) | None | No observer; static geometry | Time-parameterized geometry | **Extension** | Med |
| Time-dependent fields | Physics | None | Derivative is autonomous | Context in derivative signature | **Interface Extension** | Med |
| Mission-level simulation | Experiment | None | No composition or campaign concept | Composition of Stages 1–7 | **Extension**¹ | Low |
| Parameter sweeps | Experiment | None | Configuration is compile-time; no serialization | Experiment layer above everything | **Extension** | Low |
| Reproducible experiment records | Data + experiment | Deterministic but nothing captured | No run identity, no config capture | Provenance in result envelope | **Interface Extension** | Med |
| Batch / vectorized execution | Execution | None | Array-of-structs `State`; per-ray allocation | Ensemble-first + layout freedom | **Refactor** | High |
| GPU execution | Execution | None | 256 virtual calls per step; host-memory results | Physics in functional form | **Refactor** | High |
| HPC / distributed execution | Execution | None | Single process; no serialization | Execution strategies + serialization | **Extension**² | Med |
| Large trajectory datasets | Data | None | Full history materialized before return | Streamable results | **Interface Extension** | High |
| Multiple analysis methods | Analysis | Good precedent | None — free functions over `State` already work | None | **Extension** | Low |
| Scientific visualization | Visualization | Absent by design | Result carries presentation residue | Consume results; purge residue | **Extension** | Low |
| Wave optics / diffraction | Open | None | Ray is position + tangent; rays never interfere; result is a path | Undetermined — scientifically undecided | **Fundamental Redesign**³ | High |

¹ Extension *given* its prerequisite layer exists.
² Extension *given* ensemble-first execution and result serialization.
³ Under the current architecture. Under the target architecture, plausibly reducible to a
parallel method plus extended ray attributes — but this remains genuinely open.

### 10.2 What the table shows

**[Inferred]** Counting classifications: 11 Extension, 5 Interface Extension, 3
Modification, 7 Refactor, 1 Fundamental Redesign.

The seven Refactors are the result of interest, and they are not seven independent problems.
They trace to four architectural commitments:

| Commitment | Refactors it causes |
|---|---|
| One ray per call, history always retained (A13, A14) | Ensembles, bundles, batch, GPU |
| The output is a trajectory (A12) | Image formation |
| All physics is Christoffel symbols (A2) | Non-vacuum physics |
| Physics defined polymorphically (A17) | GPU |

**[Inferred]** And the single Fundamental Redesign — wave optics — is classified that way
because a ray is defined as position and tangent (A4), which is the same family of
commitment as the first row.

**The most important result:** the capabilities requiring modification of existing
architecture cluster almost entirely in the **data model** (what a ray is, what a result is)
and in **two contract shapes** (the step contract, the dispatch form). They do not spread
across the physics or numerics subsystems, which absorb most future work as additions.

---

## 11. Target Architecture

**TARGET.** Summarized from Phase 5. This does not exist. See
`notes/SGL_TARGET_ARCHITECTURE.md` for the full derivation.

### 11.1 The organizing rule

> A component may depend on what a computation **is**. It may never depend on how, where,
> when, how many times, or for whom that computation is performed.

### 11.2 Stable scientific concepts

Eleven concepts, each justified by a specific future capability. Six already exist in
recognizable form:

| Target concept | Current counterpart | Change required |
|---|---|---|
| Ray | `State` | Attribute set must not be a framework-wide fixed commitment |
| Chart | `CoordinateChartKind` + unused utilities | Must be attached to data, not conventional |
| Field model | `Metric` | Largely as-is |
| Propagation law | `DynamicsModel` | Must accept influences beyond Christoffel symbols |
| Numerical method | `Integrator` | Contract must carry error and step control |
| Event | `TerminationPolicy` | Must localize crossings, not just stop |
| Result + provenance | `SimulationResult` | Typed, plural, streamable, provenance-bearing; no presentation |
| **Propagation Problem** | *none* | New — the keystone |
| **Lens / Source / Observer** | *none* | New — SGL's defining geometry |
| **Ensemble** | *none* | New — the unit of work |
| **Observation / Instrument** | *none* | New — Stage 4+ |

### 11.3 Boundaries and dependency direction

**TARGET.** Nine boundaries, dependencies pointing downward only:

```text
   B9 Experiment          sweeps, campaigns, provenance, configuration
        │                 (nothing depends on it — keeps config from
        ↓                  leaking downward)
   ┌────┴────┬─────────┬──────────┬──────────┐
   ↓         ↓         ↓          ↓          ↓
  B8 Vis → B7 Analysis  B6 Obs.   B4 Comp. → B5 Execution
                        + Instr.     │           │
                          │      ┌───┴───┐       │
                          └─────→│B2     │ │B3   │←─┘
                                 │Physics│ │Num. │
                                 └───┬───┘ └──┬──┘
                                     └────┬───┘
                                          ↓
                              B1 Domain Data (shared kernel)
                                          ↓
                              B0 Foundation (units, Eigen)
```

**Key absent dependencies, deliberately:** numerics never depends on physics; neither
depends on execution; observation never depends on numerics; nothing below the result layer
knows any consumer exists.

**The one near-cycle and its cut.** Propagation must know when a ray reaches the observer,
while observation needs propagated rays. The cut: **observer *geometry* is physics (B2);
image *formation* is observation (B6).** Propagation depends on a surface, never on what is
computed from arriving rays.

### 11.4 CURRENT vs TARGET at a glance

| Aspect | CURRENT | TARGET |
|---|---|---|
| Scientific question | Not represented; fused into config | Propagation Problem (B2) |
| Configuration | One central struct, depended on by all layers | No shared type; per-layer params composed by B9 |
| Unit of work | One ray | Ensemble; one ray is N=1 |
| Step contract | Returns `State` | Returns state + error + diagnostics |
| Physics definition | Virtual interfaces | Functions over data; polymorphism as adapter |
| Physics scope | Christoffel symbols only | Field model + propagation law separated |
| Coordinates | Conventional `X[1]`, `X[2]` | Chart attached to data |
| Result | One type, trajectory, in memory, with display fields | Typed, plural, streamable, provenance-bearing |
| Observation | Absent | Distinct layer above propagation |
| Validation | One executable, conservation only | Cases as consumers; five validation kinds |
| Visualization | Absent, but residue in results | Pure leaf consumer |
| Experiment | Absent | Top layer, depended on by nothing |
| Layers realized | ~2.5 of 6 | 6 |

---

## 12. Current → Target Architectural Gap

Meaningful architectural differences only. Not a refactoring plan.

Severity reflects the **cost of closing the gap later versus now**, not current damage.

| # | Architectural Area | Current State | Target State | Gap | Severity |
|---|---|---|---|---|---|
| G1 | Scientific problem representation | Absent; fused into `SimulationConfig` | Propagation Problem as a domain concept | A missing layer; every new question lands in the centre | **Foundational** |
| G2 | Configuration | Central struct, included by all layers, also the API and result header | No shared config type; per-layer params composed at the top | Direction reversal: config must stop being depended upon | **Foundational** |
| G3 | Unit of work | One ray per call | Ensemble; single ray degenerate | Determines extension vs retrofit for all scaling | **Foundational** |
| G4 | Step contract | `step → State` | State + error estimate + step control | Shape change to a correctly placed boundary | **Foundational** |
| G5 | Result model | One trajectory type, in memory, plus display fields | Typed, plural, streamable, with provenance | Generality and streaming absent; residue present | **Foundational** |
| G6 | Physics definition form | Virtual interfaces as the definition | Functions over data; polymorphism as adapter | Inversion of definition and adapter | **Foundational** (time-sensitive) |
| G7 | Physics scope | All physics via Christoffel | Field model + propagation law separated | One boundary split into two | **Significant** |
| G8 | Coordinate charts | Conventional indexing in six components | Chart attached to data | Convention → explicit contract | **Significant** |
| G9 | SGL geometry | No lens body, source, or observer | Domain concepts in physics | Pure addition; nothing to undo | **Significant, additive** |
| G10 | Physics in orchestration | Projection and metadata inline in pipeline | Owned by physics | Relocation of existing logic | **Moderate** |
| G11 | Observation layer | Absent | Distinct layer above propagation | Pure addition — but nothing below may assume trajectory output (see G5) | **Deferred** |
| G12 | Validation | One executable, conservation only, unregistered | Cases as consumers with references and justified tolerances | Pure addition; production path unaffected | **Deferred, urgent scientifically** |
| G13 | Experiment layer | Absent; config is compile-time | Top layer owning sweeps and provenance | Pure addition at the top of the dependency order | **Deferred** |
| G14 | Visualization | Absent; residue in result type | Pure leaf consumer | Only the residue removal is in scope now | **Minor** |
| G15 | Execution | Serial CPU, no abstraction | Strategies over ensembles | Mostly absence; enabled by G3 and G6 | **Enabled by others** |

### 12.1 The structure of the gap

**[Inferred]** Six foundational gaps, three significant, six deferred or minor. The
foundational six share a defining property: **every one is a contract** — what a ray is,
what a result is, what a step returns, what the unit of work is, how physics is defined,
and what configuration is allowed to be.

The deferred ones are all **layers** — observation, validation, experiment. Because the
target dependency direction places them above everything and lets nothing depend on them,
they can be added late at roughly constant cost.

**This asymmetry is the practical content of the entire review.** Contracts are cheap to
shape before implementations accumulate against them and expensive afterward. Layers are
cheap whenever added. SGL currently has one implementation behind each interface, so the
accumulated weight against its contracts is close to zero.

### 12.2 Is the current architecture structurally aligned with the target?

**[Inferred]** Partially, and the split is clean.

**Aligned:** the physics/numerics separation is real and verified. Ownership is simple and
correct. Determinism holds. Analysis already has the target relationship to data. Six of
eleven target concepts exist in recognizable form. The dependency direction below the public
API already points the right way.

**Not aligned:** two layers are entirely missing (problem at the top of the domain,
execution at the bottom), the configuration dependency runs the wrong way, and three
contracts have the wrong shape.

**[Inferred]** With two exceptions — G1 and G9, both additions rather than corrections — the
boundaries are in approximately the right *places* with the wrong *shapes*: a step that
returns too little, a result that carries too much, a ray that is too fixed, a unit of work
that is too small.

---

## 13. Missing Abstractions

An abstraction is listed only if it (1) represents a stable scientific or software concept,
(2) is required by multiple future capabilities, (3) causes real coupling by its absence,
and (4) materially improves extensibility. Speculative generality is excluded.

### 13.1 Genuinely missing

#### M1 — Propagation Problem

- **Concept.** The scientific statement of a computation: geometry, model, what is sought,
  what is retained.
- **Capabilities.** `C3`, `C4`, `C22`, `C23`, `C12`.
- **Coupling caused by its absence.** **[Observed]** `SimulationConfig` carries spacetime,
  scenario, `dt`, `max_steps`, solver flags, and a display name in one struct that is also
  the API and result header, included by every layer including the low-level builders.
- **Improvement.** **[Inferred]** Gives every new scientific question a home outside the
  centre. It is the keystone: most other target extension points work only because it
  exists.

#### M2 — Ray Ensemble

- **Concept.** A collection of rays with identity, extent, sampling provenance, and
  optionally neighbour topology.
- **Capabilities.** `C5`, `C6`, `C7`, `C10`, `C18`, `C19`, `C21`.
- **Coupling caused by its absence.** **[Observed]** Ray count is implicit in the API
  signature: `run_simulation` takes one IC struct and returns one result. Population size,
  physics, and storage are inseparable.
- **Improvement.** **[Inferred]** Converts four Refactor-class capabilities into Extensions.

#### M3 — Generalized Result with provenance

- **Concept.** A typed scientific output — trajectory, terminal states, events, observables,
  image, convergence table — bound to the record of what produced it, and streamable.
- **Capabilities.** `C7`, `C8`, `C17`, `C21`, `C22`, `C23`, `C27`.
- **Coupling caused by its absence.** **[Observed]** One output type, trajectory-shaped,
  RAM-bounded, carrying four inert display fields.
- **Improvement.** **[Inferred]** Removes the type-level block on image formation and
  ensemble-scale output simultaneously.

#### M4 — Lens / Source / Observer geometry

- **Concept.** The three-body relationship that defines gravitational lensing.
- **Capabilities.** `C3`, `C4`, `C10`, `C11`, and every Stage 4+ capability.
- **Coupling caused by its absence.** **[Observed]** The lens is one `double`. Impact
  parameter is a caller-supplied number rather than a derived relationship, so the physical
  relationship exists only in the user's head.
- **Improvement.** **[Inferred]** This is the difference between geodesic propagation code
  and an SGL framework. Three concepts rather than one, because Phase 4 shows them evolving
  on independent schedules.

#### M5 — Extensible ray attributes

- **Concept.** The set of quantities a ray carries is a property of the physical model in
  use, not a framework-wide constant.
- **Capabilities.** `C6`, `C25`, `C26`.
- **Coupling caused by its absence.** **[Observed]** `State` is a fixed struct and the
  highest-degree node in the graph. Adding phase, amplitude, wavelength, or bundle
  derivatives means modifying the type every component depends on.
- **Improvement.** **[Inferred]** The only route by which wave optics could become an
  extension rather than a redesign. The *mechanism* is deliberately left open (Phase 4 `Q5`).

### 13.2 Missing, lower priority but justified

#### M6 — Event with localization

- **Capabilities.** `C3`, `C4`, `C7`, `C9`, `C14`.
- **Coupling.** **[Observed]** `TerminationPolicy` answers a boolean; it cannot report where
  a crossing occurred, which is what observer-plane and detector geometry require.
- **Note.** A generalization of an existing abstraction, not a new one.

#### M7 — Execution strategy

- **Capabilities.** `C18`, `C19`, `C20`.
- **Deliberately minimal.** **[Inferred]** Phase 5 rated a full backend abstraction layer
  *premature*. Execution independence follows from M2 (ensemble as unit) plus physics in
  functional form, not from an interface with one speculative implementation.

### 13.3 Explicitly not missing

**[Inferred]** These were considered and rejected. Their absence is correct.

| Not missing | Why |
|---|---|
| A generic plugin or model registry | No capability requires models unknown at build time. |
| A general ODE framework | SGL integrates ray propagation; generalizing beyond that serves nothing stated. |
| An abstract `Analysis` base class | Analyses are free functions over results. The existing observables are the right shape. |
| A device/backend abstraction layer | Independence comes from M2 plus functional physics; an interface with one implementation would encode guesses. |
| Full multi-precision templating | `C15` needs precision to *vary*, satisfied more cheaply by a nameable scalar type. |
| A serialization or schema framework | Provenance and config capture are required; a general schema system is not. |
| A `Mission` abstraction | Stage 8 is composition of Stages 1–7. Building the composition before the parts is inverted. |
| A dependency-injection container | The composition seam is one thin component. |
| An abstract `Simulation` concept | Decomposes without remainder into Problem + Method + Execution + Result. |

**[Inferred]** Five genuinely missing abstractions, two lower-priority, nine explicitly
rejected. The rejections matter as much as the additions: an architecture adopting all
sixteen would be more abstract and *less* extensible, because each unbacked abstraction
fixes a guess about a future that has not arrived. SGL's existing four abstractions are each
backed by one meaningful implementation, and that critique applies to any addition proposed
here.

---

## 14. Penrose Extraction Analysis

All conclusions are based on the SGL repository as it stands.

### 14.1 What was successfully extracted

**[Observed]** A working, verifiable Schwarzschild geodesic engine: the metric with its
Christoffel symbols, the geodesic equation, the RK4 integrator, the trajectory solver,
termination policies, four initial-condition builders, and conserved-quantity observables.
It builds clean and produces 10⁻¹⁴ conservation drift.

**[Inferred]** The extraction preserved something more valuable than the code: the
**physics/numerics separation**. `RK4Integrator.cpp` arrived physics-free and remains so.
That property is the foundation of the target architecture, and it was inherited rather than
designed for SGL.

### 14.2 What was successfully generalized

**[Observed]** Three concrete generalizations are visible in the current source:

1. **Observables generalized from the equatorial plane.** `conserved_angular_momentum` and
   `null_hamiltonian` handle general polar angle θ rather than assuming θ = π/2. Penrose's
   visualization use case only needed the equatorial case; the SGL version is correct for
   arbitrary orbital planes — which SGL's off-axis lensing geometry will require.
2. **Metric selection abstracted.** `MetricKind` and `CoordinateChartKind` exist as named
   concepts even though each currently has one value — the vocabulary for multiple models
   survived the removal of Kerr.
3. **Rendering-driven approximations excluded.** **[Inferred]** The retained code contains
   no frame-rate-motivated shortcuts; the numerics are the accurate path throughout.

### 14.3 What assumptions remain inherited

| Inherited assumption | Evidence | Still appropriate? |
|---|---|---|
| Fixed-step RK4 as *the* method | **[Observed]** `default_integrator()`, hard-wired in the pipeline | Appropriate for Stage 1; a real-time renderer needs fixed cost per frame, science needs error control |
| Trajectory as the universal output | **[Observed]** `solve → vector<State>`; `SimulationResult::history` | **No.** A renderer needs a polyline; science needs images and fields |
| One ray per call | **[Observed]** All four `run_simulation` overloads | **No.** A renderer traces per-pixel rays independently; SGL needs coupled ensembles |
| `State` as a fixed pair of 4-vectors | **[Observed]** `GeodesicState.h` | Appropriate for geometric optics; blocks wave optics |
| Display metadata in results | **[Observed]** Five write-only fields | **No.** Their consumer no longer exists |
| Virtual dispatch in the inner loop | **[Observed]** 256 virtual calls per step | Appropriate on CPU; blocks device execution |

### 14.4 Penrose-specific boundaries that remain

**[Observed]**

1. **`SimulationResult`'s display fields.** Phase 1 identified the former consumer as
   Penrose's `SimulationTrajectoryAdapter.h`. The renderer is gone; the shape it imposed
   remains, written every run and read by nothing.
2. **`CoordinateChart` compiled with zero callers.** Its Cartesian↔spherical conversions and
   Jacobians served Penrose's camera and scene transforms. In SGL it is compiled dead code —
   while `X[1]`/`X[2]` indexing proliferates because nothing connects the chart utilities to
   the state.
3. **`TrajectorySolver::propagate` unwired.** Written for the optics case, never reached.
4. **`RadiusBoundTermination` unused.** Plausibly a Penrose scene-bounds policy.

**[Inferred]** These are residue, not damage. All are inert. Their significance is
evidentiary: they show precisely which couplings the previous architecture imposed, which is
why Section 8.5 rates the display-metadata assumption low as cost and high as evidence.

### 14.5 Which inherited abstractions remain appropriate

**Appropriate, keep:** `Metric` (Christoffel-only is a limit, but the boundary is right),
`DynamicsModel`, `Integrator` (right place, wrong shape), `TerminationPolicy` (generalize to
events), the `DerivativeFunc` seam (the best boundary in the project), value-semantic
`State`, and the header-only free-function form of the observables.

**No longer appropriate:** `SimulationResult` as a renderer-shaped universal output; the
single-ray API shape; display metadata in scientific results; `SimulationConfig` as a
combined configuration, result, and API header.

### 14.6 Did the extraction establish genuine architectural independence?

**[Observed] Yes, at the dependency level.** No Penrose header, type, or symbol is
referenced. One external dependency (Eigen). The build succeeds standalone from a clean
tree. There is no coupling of any kind to the parent project.

**[Inferred] Partially, at the conceptual level.** Three shapes that exist because of
Penrose's requirements survive: the trajectory-as-output model, the single-ray API, and
display metadata in results. Independence of *code* is complete; independence of
*assumptions* is not.

**[Observed]** One artifact deserves mention because it establishes how the extraction was
performed: a stale `build/CMakeCache.txt` in the original working tree contained absolute
paths to `/home/h-livv/Projects/penrose/SGL`, confirming a directory copy rather than a
history-preserving split. That is why the git history is short and why the extraction is
best analysed from file content rather than commits.

### 14.7 Did it produce a foundation suitable for SGL's future?

**[Inferred] Yes, with one qualification.**

Suitable: the physics/numerics separation is the correct foundation and arrived intact. The
numerics are accurate. The dependency footprint is near zero. Ownership is clean.
Determinism holds. Six of eleven target concepts exist in recognizable form.

The qualification: the extraction took a **visualization-oriented propagation engine** and
placed it under a science-oriented mandate without changing the shapes that visualization
imposed. Those shapes — trajectory output, single ray, display metadata — are precisely
three of the review's high-severity assumptions (A12, A14, A18).

**[Inferred]** The extraction was well-executed as an extraction. It correctly identified
what to keep and what to discard, and it improved what it kept. It did not — and arguably
should not have, at that stage — re-derive the retained components' contracts against SGL's
own requirements. That re-derivation is the work this review identifies as outstanding.

---

## 15. Framework Identity

### 15.1 What SGL is today

**[Inferred]** SGL is currently a **single-trajectory Schwarzschild geodesic propagation
kernel with a fixed-step integrator, a scenario-based convenience API, and one smoke test.**

Evidence for each qualifier:

| Qualifier | Evidence |
|---|---|
| *single-trajectory* | **[Observed]** All four `run_simulation` overloads take one IC struct and return one `SimulationResult`. No ensemble concept. |
| *Schwarzschild* | **[Observed]** `MetricKind` single-valued; `require_spacetime` throws otherwise; every pipeline helper named `*schwarzschild*`. |
| *geodesic propagation* | **[Observed]** `GeodesicDynamics` computes `-Γ^μ_αβU^αU^β` and nothing else. |
| *kernel* | **[Observed]** A static library with no I/O, no configuration mechanism, no persistence, no UI. |
| *fixed-step* | **[Observed]** `step(state, dt, derivative)`; the solver's fixed loop; `dt` in config. |
| *scenario-based convenience API* | **[Observed]** Four overloads distinguished by IC struct type, dispatching to one shared implementation. |
| *one smoke test* | **[Observed]** `tests/null_geodesic_smoke.cpp`, self-described as "not an SGL science experiment." |

### 15.2 Evaluating the candidate identities

| Candidate | Verdict | Evidence |
|---|---|---|
| **A specialized SGL simulator** | **No** | **[Observed]** Nothing SGL-specific exists: no Sun as lens, no source, no observer, no focal region, no image. `Constants::solar_radius_m` is present and unused — the *only* solar-specific value, and it is dead. |
| **A general relativistic propagation framework** | **Closest, but overstated** | **[Observed]** The internal abstractions are general: `Metric`, `DynamicsModel`, `Integrator` name no specific spacetime. **[Observed]** But the public path admits exactly one metric, and "framework" implies extension by users, which `require_spacetime` forbids. |
| **A gravitational-lensing framework** | **No** | **[Observed]** Lensing requires a source, a lens, and an observer. None exists. `critical_impact_parameter` is the sole lensing-adjacent function, and it is a one-line formula used to pick a test parameter. |
| **A broader scientific computing framework** | **No** | **[Observed]** No experiment layer, no configuration mechanism, no data management, no validation architecture, no parallelism. |
| **A propagation *kernel* with a convenience API** | **Yes** | The description in 15.1, supported point by point. |

**[Inferred]** The most precise statement: SGL is a **correct, well-factored, single-model
propagation kernel** — the innermost computational layer of the framework it intends to
become, with the layers above it not yet built.

That is an accurate description of a Stage 1 project, and Stage 1 is where SGL is.

### 15.3 Does the architecture match the identity SGL is becoming?

**[Inferred] Partially, and the mismatch is specific rather than pervasive.**

**Where it matches.** The intended identity is a research framework where scientific models,
numerical methods, and analyses evolve independently. The physics/numerics separation
already delivers that for the numerical dimension — new integrators and new metrics are
genuinely additive at the solver level. Analyses are already free functions over results.
Ownership and determinism support reproducible research.

**Where it does not.**

1. **[Observed]** No SGL-specific concept exists. A framework for studying the Solar
   Gravitational Lens contains no representation of the Sun as a lens, a source, an observer,
   or a focal region.
2. **[Observed]** The public API admits one model, so users cannot extend the framework
   through its own surface.
3. **[Observed]** The output type is a single ray's path, while the intended scientific
   output is an image formed from a population.
4. **[Observed]** No experiment, configuration, or validation architecture exists, so
   research workflows are not expressible.

**[Inferred]** Items 1 and 4 are absences — nothing must be undone to address them. Items 2
and 3 are shapes that must change. That distinction is what makes the mismatch tractable.

### 15.4 Assumptions anchoring SGL to Penrose

**[Inferred]** Three conceptual anchors remain, all identified in Section 14.3 and all
traceable to a real-time renderer's needs:

1. **The trajectory is the output.** A renderer draws polylines. Science produces images.
2. **One ray per call.** A ray tracer treats pixels independently. Lensing couples rays.
3. **Display metadata belongs in the result.** The adapter that consumed it is gone.

Plus one performance anchor: **[Observed]** virtual dispatch in the innermost loop, which is
idiomatic and correct for a CPU reference path and blocks the device execution SGL will
eventually want.

**[Inferred]** None of these is a defect in Penrose's context. All four are shapes optimized
for a different objective, carried into a project with a different one.

---

## 16. Architectural Maturity

```text
Prototype
    ↓
Structured Application        ◄── SGL is here
    ↓
Modular Application           ◄── the numerical core alone is here
    ↓
Scientific Framework
    ↓
Extensible Scientific Platform
```

### 16.1 Assessment: Structured Application

**[Inferred]** SGL is a **Structured Application** with one subsystem — the numerical core —
independently at Modular Application quality or better.

Maturity is assessed by structure, not size. A thousand-line project can be an extensible
platform, and a hundred-thousand-line project can be a prototype.

### 16.2 Why above Prototype

**[Observed]**

- Boundaries are deliberate, not incidental: four abstract interfaces exist, and
  `TrajectorySolver` names no concrete implementation.
- Ownership is explicit and correct throughout; one heap allocation with a clear scope.
- The build is clean and reproducible from a fresh tree with one declared dependency.
- The numerics are verifiably accurate (10⁻¹⁴ conservation drift).
- Documentation of extraction decisions exists (`README.md`, `notes/EXTRACTION.md`).

**[Inferred]** These are not prototype properties. Somebody made architectural decisions
deliberately and recorded why.

### 16.3 Why below Modular Application overall

**[Observed]**

- Extension through the public API is impossible: `require_spacetime` throws for anything
  but Schwarzschild.
- One translation unit concentrates six responsibilities and all cross-subsystem coupling.
- Every interface has exactly one meaningful implementation, so modularity is asserted
  rather than demonstrated — with the partial exception of `TerminationPolicy`, which has
  two (one unused).
- Physics has leaked into orchestration (`1.5 * mass`, the null projection).
- Configuration flows downward into low-level builders.

**[Inferred]** A Modular Application can be recomposed by its users. SGL can be recomposed
only by someone who bypasses its public API — which is possible, and is exactly what makes
the numerical core's rating higher than the whole.

### 16.4 Why far below Scientific Framework

A Scientific Framework supports research workflows. **[Observed]** SGL has:

- No configuration mechanism — every parameter change is a code change.
- No experiment concept, no sweeps, no batch execution.
- No result persistence, no dataset, no catalog, no provenance.
- No validation architecture — one unregistered smoke test, no analytical references, no
  convergence machinery.
- No execution abstraction — serial CPU only.

**[Inferred]** These are absences rather than obstructions, and the target dependency
direction permits adding all of them at the top of the stack. But a framework is judged by
what researchers can do with it, and today a researcher cannot run an experiment without
editing C++.

### 16.5 Dimension-by-dimension

| Dimension | Level | Evidence |
|---|---|---|
| Abstraction quality | **Modular** | Four interfaces, correctly placed; `DerivativeFunc` is exemplary |
| Subsystem boundaries | **Modular** below the API, **Structured** at it | Solver names no concrete type; pipeline names all of them |
| Extensibility | **Structured** | 6 Strong extension points, all internal; all API-level points Missing |
| Composability | **Modular** | `TrajectorySolver` genuinely composes any model + method + policy |
| Scientific data architecture | **Structured** | One type, trajectory-shaped, carrying display residue |
| Numerical independence | **Framework** | `RK4Integrator.cpp` references no physical concept |
| Execution independence | **Prototype** | No abstraction; virtual dispatch in the inner loop |
| Validation architecture | **Prototype** | One unregistered executable; no references |
| Future resilience | **Structured** | 7 Refactor-class capabilities, tracing to 4 commitments |

**[Inferred]** The variance across dimensions is unusually wide — Framework-grade numerical
independence alongside Prototype-grade validation and execution. That variance is the
project's defining characteristic: it is not uniformly immature, it is **deeply developed in
one dimension and undeveloped in the others.**

---

## 17. Future Refactoring Risks

**[Predicted]** throughout. Ordered by expected time-to-impact.

---

### R1 — The ensemble transition

- **Root cause.** One ray per call; full history retained per ray (A13, A14).
- **Current evidence.** **[Observed]** All four `run_simulation` overloads are single-ray.
  `solve` calls `history.reserve(std::min(max_steps, 100000))` and `push_back`s every step.
  `propagate` — the ensemble-shaped primitive — exists with zero callers.
- **Triggering capability.** `C5`. The first computation requiring more than one ray, which
  is the first genuinely SGL-scientific computation attempted: any magnification, image, or
  intensity result.
- **Affected subsystems.** Public API, pipeline, solver, result type.
- **Blast radius.** Moderate–large. The API signature, the result model, and the solver's
  ownership of history all change together.
- **Severity: High.** **[Inferred]** This is the *first* future capability to be needed and
  it is Refactor-class. Nothing above Stage 1 is reachable without passing through it.

---

### R2 — The adaptive integration contract change

- **Root cause.** `step` returns only a state (A8); the solver owns fixed progression.
- **Current evidence.** **[Observed]**
  `virtual State step(const State&, double, const DerivativeFunc&) const`;
  `for (int i = 0; i < max_steps; ++i)` with constant `dt`.
- **Triggering capability.** `C14`, or `C17` convergence studies, or the first ray passing
  near the photon sphere where fixed steps become inefficient or inaccurate.
- **Affected subsystems.** Numerics, solver, configuration.
- **Blast radius.** Small today — one implementation exists. **[Predicted]** Proportional to
  the number of `Integrator` implementations at the time.
- **Severity: Medium, and increasing.** **[Inferred]** The cheapest of the major risks to
  address now and one that grows strictly with delay.

---

### R3 — The result-type generalization

- **Root cause.** The output is a trajectory (A12).
- **Current evidence.** **[Observed]** `solve → std::vector<State>`;
  `SimulationResult::history` is the only substantive field; the sole test reads `front()`
  and `back()`.
- **Triggering capability.** `C7` image formation, or `C21` when results exceed memory.
- **Affected subsystems.** Data model, solver, API, and every consumer.
- **Blast radius.** Large. The central invariant of the current architecture.
- **Severity: High.** Mitigated only by the fact that there is currently exactly one
  consumer.

---

### R4 — The device-execution wall

- **Root cause.** Physics defined polymorphically (A17); host-memory results (A13).
- **Current evidence.** **[Observed]** 256 virtual calls per accepted RK4 step through
  `std::function`; `std::vector<State>` returned by value.
- **Triggering capability.** `C19`, arriving when CPU ensemble throughput becomes limiting —
  **[Predicted]** somewhere around 10⁶ rays.
- **Affected subsystems.** Physics definition form, data layout, result model, build system.
- **Blast radius.** Large if deferred; small now.
- **Severity: High, and strongly time-dependent.** **[Inferred]** With one implementation per
  interface, defining physics functionally and wrapping it for polymorphic use costs almost
  nothing. Each additional implementation raises that cost.

---

### R5 — The second gravitational model

- **Root cause.** The public path hard-codes Schwarzschild (A1); per-model projection and
  metadata live in the pipeline (W2).
- **Current evidence.** **[Observed]** `require_spacetime` throws; `MetricKind` single-valued;
  `make_schwarzschild_metric`, `schwarzschild_metadata`, and
  `make_schwarzschild_post_step` all model-specific and all in orchestration.
- **Triggering capability.** `C24`, arriving when solar `J₂` or oblateness matters to a
  prediction's accuracy.
- **Affected subsystems.** Core enums, config, pipeline, builders, observables.
- **Blast radius.** Moderate — roughly six files, all in the coupling hotspot.
- **Severity: Medium.** **[Inferred]** Painful but bounded, and the pain is concentrated in
  the one component already identified as the coupling hotspot.

---

### R6 — The wave-optics question

- **Root cause.** A ray is position and tangent (A4); rays never interfere; the result is a
  path (A12).
- **Current evidence.** **[Observed]** `State { Vector4d X; Vector4d U; }`; verified absent
  from `physics/`: phase, amplitude, intensity, wavelength, polarization.
- **Triggering capability.** `C26`, arriving when focal-region resolution predictions are
  required — which is a central deliverable of any serious SGL mission study, not an
  optional refinement.
- **Affected subsystems.** Potentially all.
- **Blast radius.** Unbounded under the current architecture; **[Inferred]** substantially
  reduced but not eliminated under the target.
- **Severity: High, timing uncertain.** The formulation is scientifically undecided
  (Phase 4 `Q3`), so this is the one risk that cannot be responsibly designed away in
  advance — only kept reachable.

---

### 17.1 The single most likely fundamental refactor

**[Predicted] The data model — jointly, what a ray is and what a result is.**

Not the pipeline, despite its being the coupling hotspot. The pipeline is a hundred and
twenty lines in one file with one caller; rewriting it is an afternoon and touches nothing
else. Its coupling is real and its blast radius is small.

The data model is the opposite. `State` and `SimulationResult` are the two types every
component depends on, and **four separate high-severity assumptions live in them**: a ray is
position and tangent (A4), the output is a trajectory (A12), history is fully retained in
memory (A13), and one call means one ray (A14).

**Why this is the most likely.** Every one of the seven Refactor-class capabilities in
Section 10 requires changing one of those four. Ensembles need A13 and A14. Image formation
needs A12. Bundles and wave optics need A4. GPU needs A13 plus the dispatch form. They are
not independent risks that might each occur; they are one risk that will be triggered by
whichever capability arrives first.

**What triggers it.** **[Predicted]** Realistically, ray ensembles (R1) — the first genuinely
SGL-scientific computation. At that moment three of the four assumptions are hit at once.

**Why the current architecture is vulnerable.** **[Observed]** `State` is the
highest-degree node in the dependency graph, and `SimulationResult` is the only output type.
There is no indirection between them and their consumers: components use `State` directly,
index into `history` directly, and assume in-memory ownership directly. There is no seam at
which the representation could change without the change being visible everywhere.

**Is it a *fundamental* redesign?** **[Inferred] No — provided it happens soon.** The
distinction is concrete and measurable:

- **Today:** one consumer of `SimulationResult`, one implementation behind each interface,
  one execution path. Changing the data model means changing roughly a thousand lines with
  a compiler enumerating every site.
- **[Predicted] After several models, methods, and consumers accumulate:** the same change
  means changing every one of them, with behaviour to preserve at each, and it becomes a
  fundamental redesign by cost even though it is the same change by content.

**[Inferred]** The nature of this refactor is therefore not fixed. It is currently a
tractable reshaping and it converts into a redesign by accumulation. That is the single most
important timing fact in this review.

---

## 18. Overall Assessment

### Strongest architectural property

**[Observed]** Complete independence of numerical methods from physics.
`physics/integrators/RK4Integrator.cpp` references no physical concept; `TrajectorySolver`
names no concrete implementation; physics reaches numerics only through
`std::function<State(const State&)>`.

**[Inferred]** This is the hardest property in the target architecture, the one most
scientific codebases fail, and SGL has it today — inherited intact from Penrose.

### Most dangerous hidden assumption

**[Observed]** That the output of a computation is a trajectory (A12), together with its
companions that one call means one ray (A14) and that history is fully retained in memory
(A13).

**[Inferred]** Dangerous specifically because it is invisible at current scope — with one
ray it is not merely reasonable, it is optimal — and because SGL's actual scientific output
is on the other side of it. An image is not a special case of a path.

### Most important architectural boundary

**[Observed]** `Integration::DerivativeFunc` — the `std::function<State(const State&)>` seam
between physics and numerics.

**[Inferred]** It is the boundary that already delivers the target's core property. Everything
good about the current architecture's extensibility flows through it, and it should be
treated as the reference standard for every other boundary in the project.

### Most significant coupling

**[Observed]** `physics/simulation/SimulationPipeline.cpp` together with
`SimulationConfig.h`. The pipeline includes eight headers spanning every subsystem and holds
six responsibilities; the config header is simultaneously the configuration type, the result
type, and the public API declaration, and is included by every layer including the low-level
builders.

**[Inferred]** Architecturally fortunate: all coupling, all responsibility leakage, and all
extension failure are concentrated in two files rather than spread across the codebase.

### Most important missing abstraction

**[Inferred]** The **Propagation Problem** — a representation of the scientific question,
independent of how it is computed.

It is the keystone. Its absence is the root cause of the configuration god-struct, of
physics leaking into orchestration, and of every new scientific question having nowhere to
go but the centre. Four other missing abstractions (ensemble, generalized result, SGL
geometry, extensible ray attributes) are individually justified, but this one is what makes
the others reachable.

### Most likely future refactor

**[Predicted]** The data model — `State` and `SimulationResult` jointly — triggered by the
transition from single rays to ray ensembles. Four high-severity assumptions live in those
two types, and every Refactor-class capability in Section 10 requires changing one of them.

### Most important architectural risk

**[Inferred]** **Timing, not structure.**

The corrections SGL needs are inexpensive *now*, when every interface has exactly one
implementation, the result type has one consumer, and the whole library is about a thousand
lines. Each one becomes more expensive in direct proportion to what accumulates against it.
The risk is not that the architecture is wrong; it is that the window in which it is cheap
to reshape is open and will close quietly.

### Current architectural maturity

**Structured Application**, with the numerical subsystem independently at Scientific
Framework quality. Unusually wide variance across dimensions: Framework-grade numerical
independence, Prototype-grade validation and execution independence.

---

## 19. Final Verdict

> ## SGL can grow primarily through extension.

**[Inferred]** — conditional on three contract corrections made while the codebase is small.
The condition is part of the verdict, not a hedge attached to it, and Section 19.3 states
exactly what would reverse it.

### 19.1 Reasoning

**From the current architecture.** The hardest property is already correct. **[Observed]**
Numerics is completely independent of physics; the solver names no concrete implementation;
ownership is simple and correct; the computation is deterministic; the numerics are verified
accurate to 10⁻¹⁴. Six of eight subsystems are well-formed, and six Strong extension points
already exist at the solver level.

**From the future requirements.** Phase 4 identified 27 capabilities across eight scientific
stages. Most are additions to a system that currently has room for them: SGL geometry,
sources, observers, validation, experiments, analysis, and visualization are all **absences
rather than obstructions**. Nothing must be undone to add them, and the target dependency
direction places every one of them at the top of the stack where nothing depends on them.

**From the stress test.** Phase 3 and Section 10 agree on the shape of the problem: 11
Extension, 5 Interface Extension, 3 Modification, 7 Refactor, 1 Fundamental Redesign. The
seven Refactors are not seven problems — they trace to four commitments, all located in the
data model and the dispatch form.

**From the target architecture.** Phase 5 found that 19 of 28 capability rows become new
components against existing interfaces, and that the eight requiring modification reduce to
**three contract decisions**: ray attribute extensibility, a step contract carrying error
and control, and result generality with streaming and provenance.

**From the current-to-target gap.** Six foundational gaps, all of them *contracts*; the rest
are *layers* addable late at constant cost. **[Inferred]** With two exceptions — the missing
problem layer and the missing SGL geometry, both pure additions — the boundaries are in
approximately the right places with the wrong shapes.

**From the Penrose extraction.** It delivered the right foundation. **[Observed]** Complete
dependency independence, one external library, the physics/numerics separation intact, and
observables genuinely generalized from Penrose's equatorial-only case. What it did not do
was re-derive the retained contracts against SGL's own requirements — which is precisely the
outstanding work this review identifies, and which is a reshaping rather than a rebuild.

**From the hidden assumptions.** Eighteen catalogued, eight High severity. **[Inferred]**
They cluster in three places — the definition of a ray, the definition of a result, and two
contract shapes — rather than spreading across the physics and numerics subsystems, which
absorb most future work as additions.

**From the predicted refactors.** The largest, the data model, is currently a tractable
reshaping affecting one consumer and one implementation per interface. Its cost grows
strictly with accumulation.

### 19.2 Why this is not a fundamental redesign

A fundamental redesign means the structure must be discarded. SGL's structure survives:

- **[Observed]** The physics/numerics boundary is correct and needs no change.
- **[Observed]** Six of eleven target concepts already exist in recognizable form.
- **[Observed]** The dependency direction below the public API already points the right way.
- **[Inferred]** The changes needed are three contract shapes, one dependency-direction
  reversal (configuration), and two added layers.

**[Inferred]** Three contract changes and two additions to a thousand-line library, on a
foundation whose hardest property is already right, is extension with corrections — not
redesign.

### 19.3 What would reverse this verdict

**[Predicted]** The verdict is time-sensitive, and its reversal conditions are specific and
observable:

1. **Accumulation before correction.** Several `Metric` and `Integrator` implementations, and
   several consumers of `SimulationResult`, written against the current contracts. Then the
   same three changes become a redesign by cost while remaining identical in content.
2. **Growth through the pipeline.** If the next several capabilities are added as branches in
   `SimulationPipeline.cpp` and fields in `SimulationConfig`, the coupling hotspot becomes
   the architecture, and W1 stops being a missing layer and becomes an entrenched one.
3. **Wave optics requiring a parallel branch.** **[Predicted]** If `C26` turns out to demand
   a fundamentally different computational object rather than extended ray attributes, that
   is a fundamental architectural event under any architecture. The target reduces its blast
   radius; it does not eliminate it. This is the one genuine unknown, and it is scientific
   before it is architectural.

**[Inferred]** Conditions 1 and 2 are within the project's control. Condition 3 is not.

### 19.4 Summary judgement

SGL is a small, correct, well-factored Stage 1 propagation kernel with one exemplary
architectural boundary, one concentrated coupling hotspot, and four high-severity
assumptions living in two data types.

It is not yet an SGL framework — it contains no representation of the Sun as a lens, of a
source, of an observer, or of an image. But it is the right innermost layer for one, and the
layers above it are additions rather than replacements.

The architecture will not need to be rebuilt. It will need three contracts reshaped, and it
is currently as cheap to do that as it will ever be.

---

## Final Architectural Principle

> **A ray, a result, and a step are contracts; a model, a method, a strategy, an
> observation, an analysis, and an experiment are additions. Keep the contracts small enough
> that everything else can be added beside them rather than inside them.**

**[Inferred]** This principle is derived from the review rather than assumed. The evidence
for it is that every architectural success in the current codebase comes from a small,
uninformative contract — `DerivativeFunc` tells numerics nothing about physics, and that
single property is what makes new integrators and new metrics genuinely additive. Every
architectural failure comes from a contract that is too informative or too specific: a
result that describes a renderer, a configuration that names a spacetime, a step that
returns only an answer, an API that admits only one ray.

The distinction to preserve is between the few things that must be *agreed* and the many
things that should merely be *added*. SGL currently has the right number of the first kind
and needs to shape three of them better.

---

## Appendix A — Reconciliation of prior phases

Where earlier phases appear to disagree, the resolution:

| Apparent conflict | Resolution |
|---|---|
| Phase 2 called the `Integrator` boundary strong; Phase 3 called adaptive integration contradicted | Both correct. The boundary is in the right **place** and has the wrong **shape**. It correctly excludes physics and incorrectly excludes diagnostics. |
| Phase 2 described a modular architecture; Phase 3 alleged false modularity | Both correct at different levels. Modular below `run_simulation`, closed at it. Extension points 1 and 2 in Section 6 are the same capability rated Strong internally and Missing externally. |
| Phase 4 rated `C24` both Partial and Contradicted | Split into two claims: higher-order *gravitational* models are Partial (the `Metric` interface generalizes; the public path does not), while *non-vacuum* physics is Contradicted (no channel exists for non-Christoffel terms). |
| Phase 3 treated the pipeline as the top architectural risk; Section 17 names the data model | Not a conflict but a refinement by blast radius. The pipeline has the highest coupling and the smallest blast radius (120 lines, one file, one caller). The data model has lower coupling and unbounded blast radius. Coupling measures present disorder; blast radius predicts future cost. |
| Phase 4 called SGL deterministic; Phase 4 also called reproducibility absent | Both correct and worth preserving as a distinction. Determinism is a property of the computation and is **[Observed]** present. Reproducibility is a property of the record and is absent — nothing captures what was run. |

## Appendix B — Claims carried forward as unresolved

**[Observed]** `SchwarzschildParameters::mass` is named `mass`, documented in-comment as
`rs`, and used everywhere as `rs`. Whether the field is normatively mass or Schwarzschild
radius has been marked `UNVERIFIED` since Phase 1 and is not upgraded by restatement here.

**[Inferred]** Harmless while there is one metric and one unit convention. It becomes a
silent physical-correctness hazard the moment a second gravitational model, an SI boundary,
or an external data source appears. It is the cheapest open question in the project to
resolve.

Phase 4's open questions `Q1`–`Q12` remain open, with the exception of the three that
Phase 5 answered because the target extension model could not be derived without them:
results are plural and typed (`Q1`), both initial-value and boundary-value framings must be
expressible (`Q2`), and the atomic unit of computation is the ensemble (`Q4`).

---

## Document Status

Permanent architectural reference. Reflects the repository as verified during this review:
30 files, one static library, one executable, one external dependency, clean Release build,
smoke test reporting `|dE/E| = 1.0103e-14` and `|dL/L| = 3.72693e-15` over 50 001 states.

All **[Observed]** claims were verified against the working tree. All **[Inferred]** claims
are reasoning over those observations and are labelled as such. All **[Predicted]** claims
concern future development and are not yet true.

This document should be revised when any of the following occurs, each of which invalidates
a load-bearing observation: a second `Metric` implementation is wired into the public path;
the `Integrator` contract changes; `SimulationResult` gains a real consumer or a second
result type; or the single-ray API acquires an ensemble form.

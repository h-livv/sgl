# SGL Target Architecture

**Phase 5 artifact. Architectural destination — not an implementation path.**

This document derives the minimum architectural structure necessary for SGL to evolve
into the framework described by the Phase 4 Future Requirements Map. It proposes no code,
no refactoring sequence, and no optimization of the current implementation.

Prior phases:

- Phase 1 — `notes/ARCHITECTURE_RECONSTRUCTION.md` (repository reconnaissance)
- Phase 2 — `notes/SGL_ARCHITECTURE_RECONSTRUCTION.md` (architectural reconstruction)
- Phase 3 — `notes/SGL_ARCHITECTURAL_STRESS_TEST.md` (adversarial stress test)
- Phase 4 — `notes/SGL_FUTURE_REQUIREMENTS_MAP.md` (future requirements; capability IDs
  `C1`–`C27` referenced throughout are defined there)

---

## 1. Architectural Objective

SGL must grow primarily by **adding** models, numerical methods, execution strategies,
observations, analyses, and experiments — not by repeatedly modifying a central core.

Phase 3 established that the current architecture fails this test at the public API while
satisfying it at the solver level. The target is to extend the property that already works
upward through the framework, not to invent a new one.

### The single organizing rule

> **A component may depend on what a computation *is*. It may never depend on how, where,
> when, how many times, or for whom that computation is performed.**

Every boundary in this document is a consequence of that rule. Concretely it forbids:
physics knowing about integrators, integrators knowing about physical models, either
knowing about batching or devices, results being shaped by consumers, and any of them
knowing about experiments or rendering.

### What the objective explicitly does not include

Maximizing abstraction. Phase 3 found that the current architecture's four abstractions
(`Metric`, `DynamicsModel`, `Integrator`, `TerminationPolicy`) are each backed by exactly
one meaningful implementation, so their generality is asserted rather than demonstrated.
Adding more unbacked abstractions would compound that. Section 13 is therefore mandatory
and adversarial: every abstraction proposed here must name the Phase 4 capability that
forces it, and several candidate abstractions are rejected.

---

## 2. Stable Scientific Concepts

Derived from Phase 4, not from the existing directory structure. Each concept is listed
with why it exists, what it owns, what it must **not** own, and which future capabilities
depend on it.

A concept earns a place here only if it satisfies two tests: it is meaningful to a
physicist independent of any code, and at least one Phase 4 capability is impossible to
express without it.

---

### 2.1 Ray

**Why it exists.** The elementary object being propagated. Every capability from Stage 1
to Stage 8 ultimately reduces to rays.

**Owns.** The complete physical description of one propagating entity: its position and
tangent in a named chart, and — as physics expands — the attributes that physics assigns
to it (wavelength, accumulated phase and amplitude, polarization, provenance back to the
sample that generated it).

**Must not own.** Its own storage layout, its own memory location, its own history, any
knowledge of how many other rays exist, or how it is integrated. This is the sharpest
departure from the current `State`, which is a fixed two-`Vector4d` struct that
simultaneously fixes the physics content, the memory layout, and the arithmetic operations
convenient for RK4.

**Depends on it.** Everything. C5, C6, C18, C19, C25, C26 depend specifically on the *set
of attributes* not being a global, permanent commitment.

**Critical property.** The attribute set is a property of the *physical model in use*, not
a framework-wide constant. Geometric propagation in vacuum needs position and tangent;
plasma refraction adds wavelength; wave optics adds phase and amplitude; variational
bundle tracking adds derivative components. The architecture must not require every
computation to pay for every attribute, and must not require modifying a universal struct
to add one.

**Deliberately left open.** Whether this is achieved by templating, by a wider fixed state,
or by composition is an implementation question (Phase 4 `Q5`). The architectural
requirement is only that ray content not be a single framework-wide commitment.

---

### 2.2 Chart

**Why it exists.** Phase 4 `C16` and Phase 3 both identified raw `X[1]`/`X[2]` indexing —
"component 1 is radius, component 2 is polar angle" — replicated across the metric, the
initial-state builders, the observables, the termination policies, and the projection
callback. That is an unnamed convention holding six components together.

**Owns.** The meaning of coordinate components, and the transformations between charts.

**Must not own.** Anything physical. A chart is a description, not a model.

**Depends on it.** `C16` directly. Indirectly `C9` (observational coordinates must be
distinguishable from simulation coordinates) and `C24` (a different gravitational model
may be naturally expressed in a different chart).

**Why this is a concept and not a utility.** `CoordinateChart` already exists as a utility
in the current repository and has **no caller anywhere**. Being available as a helper is
demonstrably insufficient; the chart must be attached to the data so that a mismatch is
detectable rather than conventional.

---

### 2.3 Field Model (Gravitational / Spacetime)

**Why it exists.** The gravitational environment. `C24` requires solar `J₂`, oblateness,
rotation, and perturbations; SGL's Sun is measurably not Schwarzschild.

**Owns.** The spacetime geometry as a function of position, and eventually of time.

**Must not own.** How photons move through it (see 2.4), the numerical method, coordinate
conventions of consumers, or anything about sources and observers.

**Depends on it.** `C24`, and any comparison between gravitational models.

---

### 2.4 Propagation Law

**Why it exists — and why it is separate from the field model.** This separation is the
most consequential physics decision in the document, so it is justified explicitly rather
than assumed.

The current architecture routes all physics through Christoffel symbols:
`GeodesicDynamics` computes `a^μ = -Γ^μ_{αβ}U^αU^β` and nothing else. Phase 4 `C24` rated
non-vacuum propagation **Contradicted** for exactly this reason. Solar corona plasma
refraction is a genuine SGL effect: it is wavelength-dependent, it adds terms to the
equation of motion that are not Christoffel symbols, and it cannot enter through the metric.
Wave-optical phase accumulation (`C26`) has the same shape — an additional quantity evolving
along the ray that the metric does not produce.

So the equation of motion must be its own boundary, able to consume a field model *plus*
other physical influences.

**Owns.** The right-hand side of the evolution: given a ray and its environment, how the
ray's attributes change.

**Must not own.** Integration, step size, termination, storage, or ensemble size.

**Depends on it.** `C24`, `C25`, `C26`, and any higher-order or perturbative effect.

---

### 2.5 Geometry: Lens, Source, Observer

**Why they exist.** Stage 2 is unreachable without them. Phase 4 found no source, no
observer, and no lens body anywhere in `physics/` — the lens is currently one `double`.

**Why three concepts rather than one.** Because Phase 4 shows them evolving on independent
schedules: the source becomes extended at Stage 6, the observer becomes dynamic at
Stage 7, and the lens gains multipole structure at `C24`. Fusing them into a single "scene"
would couple three independent evolution paths.

**Own.** Respectively: the gravitating body and its field-model parameters; emission
geometry and brightness distribution; reception geometry, position, and motion.

**Must not own.** Propagation, sampling strategy, image formation, or instrument behaviour.
In particular the **observer must not own image formation** — that separation is what keeps
Section 10's layering acyclic.

**Depends on them.** `C3`, `C4`, `C10`, `C11`, and everything at Stage 4 and above.

---

### 2.6 Propagation Problem

**Why it exists.** This is the layer Phase 4 Section 8 identified as entirely missing —
the "Physical Problem" tier. Its absence is why the current `SimulationConfig` mixes a
scenario enum, spacetime parameters, `dt`, and `max_steps` into one struct: with no place
to state *what is being asked*, everything collapses into *how it is run*.

**Owns.** The complete scientific statement of a computation: the geometry, the field model
and propagation law to use, what is being sought, and what should be retained. It must be
able to express both framings from Phase 4 `Q2`:

- *Initial-value:* given these emitted rays, where do they go?
- *Boundary-value:* which rays connect this source to this observer? (`C4`)

**Must not own.** Numerical method, step size, tolerance, execution strategy, ensemble
storage, or output destination. A problem stated at 10 rays and at 10⁸ rays is the same
problem.

**Depends on it.** `C3`, `C4`, `C22` (a sweep is a family of problems), `C23` (a problem is
the reproducible unit), and `C12`.

**This is the keystone.** Most of the extension model in Section 6 works only because this
concept exists. Without it, every new scientific question has nowhere to live except the
central pipeline — which is precisely the current failure mode.

---

### 2.7 Ensemble

**Why it exists.** `C5`. Observational quantities are population properties; no single ray
answers any Stage 4 question.

**Owns.** The identity and extent of a collection of rays, and the sampling plan that
generated it.

**Must not own.** Storage layout, residency (host or device), or iteration order.

**Must support** an optional neighbour topology, because `C6` (bundles) requires that
nearby rays remain identifiable as neighbours after propagation — that relationship *is*
local magnification.

**Depends on it.** `C5`, `C6`, `C7`, `C10`, `C18`, `C19`, `C21`.

**Governing rule.** A single ray is the degenerate case of an ensemble, not the base case
with ensembles built on top. The current architecture takes the opposite position and
Phase 4 rated the consequences **Contradicted**.

---

### 2.8 Numerical Method

**Why it exists.** `C13`, `C14`, `C15`, `C17`. A scientific result must be separable from
the algorithm that produced it, or convergence claims are meaningless.

**Owns.** How state is advanced, local error estimation, step-size control, and event
localization.

**Must not own.** Any physics. The current `RK4Integrator.cpp` already achieves this — it
references no physical concept — and it is the best-preserved boundary in the codebase.
The target keeps it and fixes its shape (Section 8).

**Depends on it.** `C13`, `C14`, `C15`, `C17`, and all of validation.

---

### 2.9 Event

**Why it exists — a generalization, not an invention.** `TerminationPolicy` already exists
and answers a yes/no question: stop or continue. Stage 2 needs more: *where exactly* did
the ray cross the observer plane, the detector aperture, or a given radius. Sub-step
localization is required for accuracy — with adaptive steps, "the state after the step
that crossed" can be far from the crossing itself.

**Owns.** Conditions of interest along a ray, and their localization in the affine
parameter.

**Must not own.** What to do about them scientifically.

**Depends on it.** `C3`, `C4`, `C7`, `C9`, and `C14` (adaptive integration makes crude
crossing detection inaccurate).

---

### 2.10 Result and Provenance

**Why it exists.** `C7` rated **Contradicted** because the current architecture's universal
output is trajectory-shaped. `C23` requires that a result be inseparable from the record of
what produced it.

**Owns.** A typed scientific output — trajectory, terminal states, observables, image,
field, convergence table, validation report — bound to its provenance: the problem, the
method, the model versions, and the code revision.

**Must not own.** Presentation metadata. Phase 4 Section 9 documented `characteristic_radius`,
`horizon_radius`, `photon_sphere_radius`, `coordinate_chart`, and `name` as write-only
fields inherited from a Penrose renderer that no longer exists. The rendering contract
outlived the renderer; the target severs it.

**Depends on it.** `C7`, `C8`, `C17`, `C21`, `C22`, `C23`, `C27`.

**Two properties are load-bearing.** Results are **plural and typed** (a trajectory is one
kind, not the kind), and they are **streamable** — `C21` requires that a result be
consumable as it is produced rather than materialized in full, since `solve` currently
builds the entire `std::vector<State>` before returning.

---

### 2.11 Observation

**Why it exists.** Phase 4 Stage 4. The transition from "where does this photon go" to
"what does the observer measure."

**Owns.** The mapping from propagated rays to observable quantities: intensity,
magnification, angular distribution, image.

**Must not own.** Propagation, instrument behaviour (2.12), or analysis.

**Depends on it.** `C7`, `C8`, `C26`.

---

### 2.12 Instrument

**Why it is separate from Observation.** Phase 4 Stage 5 states the requirement directly:
instrument modeling must remain conceptually distinct from relativistic propagation. The
distinction is physical, not organizational — "the intensity field arriving at the focal
region" is a property of the universe; "what this detector records" is a property of a
piece of hardware. They must be independently variable, because comparing instruments
against a fixed physical prediction is a core mission-design question.

**Owns.** Aperture, detector geometry and resolution, response, sampling, signal formation.

**Must not own.** Anything above it in the physics stack.

**Depends on it.** `C9`, `C12`.

---

### 2.13 Experiment

**Why it exists.** `C22`, `C23`, `C12`. Phase 4 rated experimentation **Absent**: today
every parameter change is a code change, so no experiment is reproducible except by
reference to a source revision.

**Owns.** Families of problems, sweep specification, batch execution, result collection,
and the reproducibility record.

**Must not own.** Physics, numerics, or execution mechanics — it composes them.

**Depends on it.** `C22`, `C17`, `C12`, `C20`.

---

### 2.14 Validation Case

**Why it exists.** Phase 4 Section 5 established validation as the credibility mechanism
for a framework whose predictions cannot be checked against a real SGL observation.

**Owns.** A reference prediction, a comparison metric, a tolerance, and the justification
for that tolerance. Phase 4 noted the current smoke test's `1e-3` threshold sits 11 orders
of magnitude above observed drift with no recorded rationale.

**Must not own.** Any part of the production simulation, and — critically — production must
own nothing on its behalf. No validation hooks compiled into the propagation path.

**Depends on it.** `C2`, `C17`, and cross-method comparison.

---

### 2.15 Concepts deliberately rejected

| Rejected concept | Why |
|---|---|
| **Central configuration object** | The current `SimulationConfig` is the framework's coupling hotspot precisely because it exists. In the target there is no shared config type: each layer owns its own parameters, and the Experiment layer composes and serializes them. Configuration is a *projection* of Problem + Method + Execution, not a thing subsystems depend on. |
| **Simulation** | Too vague to own a boundary. It decomposes without remainder into Problem + Method + Execution + Result. Keeping it invites exactly the god-module Phase 3 predicted. |
| **Scene** | Fusing lens, source, and observer would couple three independently evolving concepts (2.5). |
| **Backend** as a first-class domain concept | Execution is a strategy applied to a problem, not something the domain represents. See Section 9. |
| **Mission** | Stage 8 is composition of Stages 1–7. Introducing it now would be an abstraction with no implementation and no near-term consumer. |

---

## 3. Architectural Boundaries

Ten boundaries were nominated for evaluation. Eight survive as real boundaries; two
collapse, and the reasoning for collapsing them matters as much as the ones retained.

**Configuration is not a boundary.** It is a serialization concern owned by Experiment.
Making configuration a boundary is what produced the current god-struct.

**Simulation orchestration is not a boundary — it is a *seam*.** It should be the thinnest
component in the framework: it composes a problem, a method, and an execution strategy, and
owns no policy of its own. Today `SimulationPipeline.cpp` does the opposite, inlining
Schwarzschild algebra (`1 - rs/r`, `1.5 * mass`, the null projection) into orchestration.
The target treats "orchestration contains physics" as the primary anti-pattern to avoid.

---

### B1 — Domain Data (shared kernel)

| | |
|---|---|
| **Responsibility** | Ray, chart, ensemble, trajectory, result envelope, provenance. The vocabulary every other layer speaks. |
| **Inputs** | None. |
| **Outputs** | Type definitions and invariants only. |
| **Dependencies** | Foundation only (units, constants, linear algebra). Depends on nothing else in the framework, ever. |
| **Stability** | **Highest.** This is the layer that must not churn, because everything depends on it. |
| **Extension mechanism** | Attribute extension for rays (2.1); new result kinds added alongside existing ones, never by widening a universal struct. |

The tension is deliberate and must be managed explicitly: this layer must be simultaneously
the most stable and the most extensible. That is only reconcilable if extension is
*additive* (new ray attributes, new result kinds) rather than *modificative*.

---

### B2 — Physics / Domain Model

| | |
|---|---|
| **Responsibility** | Field models, propagation laws, lens/source/observer geometry, source emission. |
| **Inputs** | Rays, geometry parameters. |
| **Outputs** | Field quantities, ray attribute derivatives, geometric relationships. |
| **Dependencies** | B1 only. Must not depend on numerics, execution, results-as-artifacts, observation, or configuration. |
| **Stability** | High for the *boundary*; low for its contents, which should proliferate. |
| **Extension mechanism** | New field model, new propagation law, new source model — each a new component implementing an existing contract. |

**The static-composability requirement.** For `C19` (GPU), the physics must be expressible
as plain functions over plain data, with runtime polymorphism as an *adapter* over that
form rather than the definition of it. This is the one place where a downstream execution
concern legitimately constrains an upstream boundary, and it must be acknowledged rather
than hidden: the current inner loop performs 256 virtual calls per accepted RK4 step, which
is fine on CPU and does not survive device-side execution. Defining physics functionally
and wrapping it for polymorphic use costs nothing on CPU and preserves the option.

---

### B3 — Numerical Methods

| | |
|---|---|
| **Responsibility** | State advancement, error estimation, step control, event localization. |
| **Inputs** | An initial state, a derivative callable, method parameters, event conditions. |
| **Outputs** | Advanced states with error estimates and step diagnostics. |
| **Dependencies** | B1 only. **Never B2.** |
| **Stability** | **Highest after B1.** ODE integration theory is settled; this boundary should outlive every physics decision in the project. |
| **Extension mechanism** | New method as a new component. |

The current `DerivativeFunc` seam already achieves the essential property — Phase 2 called
it the strongest boundary in the codebase — and the target preserves it unchanged in spirit.

---

### B4 — Composition (thin seam, not a subsystem)

| | |
|---|---|
| **Responsibility** | Bind problem + model + method + execution. Nothing else. |
| **Inputs** | Problem, method selection, execution strategy. |
| **Outputs** | Results. |
| **Dependencies** | B1, B2, B3, B5. |
| **Stability** | Moderate — it changes when the *set of things being composed* changes, which is rare if the other boundaries hold. |
| **Extension mechanism** | Ideally none needed. If this component grows, a boundary above or below it is wrong. |

**This component is the canary.** Its size is the framework's health metric.

---

### B5 — Execution

| | |
|---|---|
| **Responsibility** | How much work runs where: serial, batched, parallel, device, distributed. |
| **Inputs** | Work units (problems or ensembles) and a method. |
| **Outputs** | Results, possibly streamed. |
| **Dependencies** | B1, B3, and B2's functional form. |
| **Stability** | Moderate — hardware changes; the *boundary* should not. |
| **Extension mechanism** | New strategy as a new component. |

---

### B6 — Observation and Instrument

| | |
|---|---|
| **Responsibility** | Rays → observables → images → instrument signal. |
| **Inputs** | Propagated ensembles, observer and instrument geometry. |
| **Outputs** | Observables, images, fields, signals. |
| **Dependencies** | B1, B2 (geometry only). **Not** B3, **not** B5. |
| **Stability** | Low initially — this is the least understood part of the domain and should be expected to churn. |
| **Extension mechanism** | New observable, new image formation method, new instrument model. |

Its lower stability is why it is a separate boundary rather than an extension of B2:
churn must be contained where it cannot damage the stable layers.

---

### B7 — Analysis and Validation

| | |
|---|---|
| **Responsibility** | Compute derived quantities from results; compare results against references, against each other, and across refinement. |
| **Inputs** | Results (in memory or persisted). |
| **Outputs** | Analysis products, convergence tables, validation reports. |
| **Dependencies** | B1. Optionally B2 for analytical references. |
| **Stability** | High boundary, freely growing contents. |
| **Extension mechanism** | New analysis or validation case as a new component. Pure consumers — nothing depends on them. |

Analyses should be **functions over results**, not stateful subsystems. The existing
header-only `SchwarzschildObservables.h` is already the right shape and is a good precedent
to preserve.

---

### B8 — Visualization

| | |
|---|---|
| **Responsibility** | Render scientific results. |
| **Inputs** | Results and analysis products. |
| **Outputs** | Images, plots, interactive views. |
| **Dependencies** | B1, B7. Nothing depends on it. |
| **Stability** | Irrelevant — it is a leaf, so it may churn freely. |
| **Extension mechanism** | New view as a new component. |

---

### B9 — Experiment

| | |
|---|---|
| **Responsibility** | Sweeps, campaigns, batch runs, configuration serialization, provenance capture, result cataloguing. |
| **Inputs** | Experiment specification. |
| **Outputs** | Result datasets with provenance. |
| **Dependencies** | Everything below. |
| **Stability** | Moderate. |
| **Extension mechanism** | New experiment type; new sweep strategy. |

Top of the stack. **Nothing may depend on it** — that is what keeps configuration from
leaking back down, which is the failure the current `SimulationConfig` exhibits.

---

## 4. Target Dependency Graph

Dependencies point downward only.

```text
                        ┌─────────────────────────┐
                        │  B9  Experiment         │   sweeps, campaigns,
                        │      (top; no deps in)  │   provenance, config
                        └────────────┬────────────┘
                                     │
        ┌──────────────┬─────────────┼──────────────┬──────────────┐
        ↓              ↓             ↓              ↓              ↓
  ┌───────────┐  ┌───────────┐  ┌─────────┐  ┌───────────┐  ┌───────────┐
  │ B8  Visu- │→ │ B7 Analy- │  │ B6 Obs. │  │ B4 Compo- │  │ B5 Execu- │
  │   alization│  │ sis+Valid │  │ + Instr.│  │  sition   │→ │   tion    │
  └─────┬─────┘  └─────┬─────┘  └────┬────┘  └─────┬─────┘  └─────┬─────┘
        │              │             │             │              │
        │              │             │       ┌─────┴─────┐        │
        │              │             │       ↓           ↓        │
        │              │             │  ┌─────────┐ ┌─────────┐   │
        │              │             └─→│ B2      │ │ B3      │←──┘
        │              └────────────────→│ Physics │ │ Numerics│
        │                                └────┬────┘ └────┬────┘
        │                                     │           │
        └──────────────┬──────────────────────┴───────────┘
                       ↓
              ┌──────────────────┐
              │ B1 Domain Data   │   ray, chart, ensemble,
              │  (shared kernel) │   trajectory, result, provenance
              └────────┬─────────┘
                       ↓
              ┌──────────────────┐
              │ B0 Foundation    │   units, constants, linear algebra
              └──────────────────┘
```

### Why each dependency exists

| Dependency | Justification |
|---|---|
| Everything → B1 | Shared vocabulary. A framework where subsystems exchange data needs one definition of what a ray is. |
| B2 → B1 | Physics operates on rays. |
| B3 → B1 | Methods advance states. |
| **B3 ↛ B2** | Deliberately absent. RK4 does not know what a geodesic is, and must not. Already true today. |
| **B2 ↛ B3** | Deliberately absent. A field model must not know its step size. |
| B4 → B2, B3, B5 | Composition is the only place these meet. |
| B5 → B3, B2ᶠ | Execution applies a method to physics in functional form. |
| **B2, B3 ↛ B5** | Deliberately absent. This is what makes `C18`/`C19`/`C20` extensions. |
| B6 → B2 (geometry only) | Image formation needs observer geometry, not propagation. |
| **B6 ↛ B3, B5** | Observation consumes results; it must not care how they were computed. |
| B7 → B1, B2ᵒᵖᵗ | Analysis reads results; validation may need analytical references from physics. |
| B8 → B1, B7 | Visualization consumes results and analysis products. |
| **B1..B7 ↛ B8** | The core requirement of Section 12. |
| B9 → all | Orchestration is composition. |
| **nothing → B9** | Prevents configuration leaking downward. |

### The one near-cycle, and how it is cut

Stage 2 creates apparent circularity: propagation needs to know when a ray reaches the
observer (an observation concern), while observation needs propagated rays.

The cut: **observer *geometry* lives in B2; image *formation* lives in B6.** Propagation
depends on geometry — a position, an orientation, a surface — which is domain physics.
It never depends on what is subsequently computed from the arriving rays. Events (2.9)
express "ray crossed this surface" without any notion of intensity or pixels.

This is why 2.5 insists the observer must not own image formation. Fusing them would create
a genuine cycle between the two layers.

---

## 5. Target Data Flow

The Phase 5 prompt offers a linear chain as a starting point. The requirements do not
support a purely linear flow, for three reasons: sampling sits between the problem and
propagation and is independently variable (`C5`, `C10`); results fan out to several
independent consumers; and validation is a parallel path, not a downstream stage.

```text
                    Experiment / Sweep  (B9)
                            │  expands into many
                            ↓
                   Propagation Problem  (B2)
                   geometry + model + question
                            │
                            ↓
                  Sampling / Ensemble Generation
                   (how many rays, drawn how)
                            │
                            ↓
              ┌─────────────────────────────┐
              │  Composition (B4)           │
              │  problem × method × exec    │
              └──────────────┬──────────────┘
                             │
                    Execution (B5) applies
                    Numerical Method (B3) to
                    Propagation Law (B2)
                             │
                             ↓
                    Result + Provenance  (B1)
                    ├─ trajectories
                    ├─ terminal states
                    ├─ event records
                    └─ streamed samples
                             │
        ┌──────────────┬─────┴───────┬──────────────┐
        ↓              ↓             ↓              ↓
   Observation     Analysis      Validation     Persistence
      (B6)           (B7)          (B7)            (B9)
        │              │             │              │
        ↓              ↓             ↓              ↓
  Observables/     Derived      Convergence     Datasets
  Images/Signal    quantities   + reports       + catalog
        │              │
        └──────┬───────┘
               ↓
        Visualization (B8)
```

### Which structures are stable architectural contracts

Only four. Everything else may change freely, and keeping this list short is the point —
each entry is a permanent constraint on the project.

| Contract | Why it must be stable | Why it must be small |
|---|---|---|
| **Ray** | The universal currency; every layer touches it. | Every attribute is a cost paid by every computation. |
| **Propagation Problem** | The reproducible unit and the sweep unit. | It must express the science and nothing else. |
| **Result + Provenance envelope** | Everything downstream consumes it; reproducibility depends on it. | The *envelope* is stable; the payload types are open. |
| **Ensemble handle** | The unit of execution and storage. | It must describe extent and identity, never layout. |

Notably **absent** from this list: trajectory (one payload among several), configuration
(a projection, not a contract), and anything visualization-related.

---

## 6. Target Extension Model

All 27 Phase 4 capabilities, mapped to where they would attach.

| # | Future Capability | Target Extension Point | Existing Component Modified? | New Component? |
|---|---|---|---|---|
| C1 | Photon geodesic propagation | B2 field model + propagation law | No | Exists |
| C2 | Analytical validation | B7 validation case | No | Yes |
| C3 | Lens / source / observer geometry | B2 geometry | No | Yes |
| C4 | Connection (source→observer) solving | B3 method (BVP alongside IVP), over B2 | No | Yes |
| C5 | Ray ensembles | B1 ensemble + sampling | No¹ | Yes |
| C6 | Ray bundles | B1 ensemble topology + ray attributes | **Yes** — ray attributes | Yes |
| C7 | Image formation | B6 observation | No | Yes |
| C8 | PSF modeling | B6 observation | No | Yes |
| C9 | Instrument / detector | B6 instrument | No | Yes |
| C10 | Extended sources | B2 source model + sampling | No | Yes |
| C11a | Observer / spacecraft motion | B2 geometry (time-parameterized) | No | Yes |
| C11b | Time-dependent gravitational field | B2 propagation law signature | **Yes** — derivative context | Yes |
| C12 | Mission-level simulation | B9 campaign composition | No | Yes |
| C13 | Multiple integrators | B3 method | No | Yes |
| C14 | Adaptive integration | B3 step contract | **Yes** — step result | Yes |
| C15 | Precision / tolerance control | B3 method parameters; B0 scalar policy | **Yes** — localized | Partly |
| C16 | Alternative coordinate charts | B1 chart + B2 model | **Yes** — chart made explicit | Yes |
| C17 | Convergence studies | B7 validation + B9 sweep | No | Yes |
| C18 | Batch / vectorized execution | B5 strategy | No¹ | Yes |
| C19 | GPU execution | B5 strategy over B2 functional form | No² | Yes |
| C20 | HPC / distributed | B5 strategy + B9 serialization | No | Yes |
| C21 | Large trajectory datasets | B1 result sink (streaming) | **Yes** — result is streamable | Yes |
| C22 | Parameter sweeps | B9 experiment | No | Yes |
| C23 | Reproducible records | B1 provenance envelope | **Yes** — built in once | Yes |
| C24 | Higher-order / non-vacuum physics | B2 field model + propagation law | No³ | Yes |
| C25 | Wavelength dependence | B1 ray attributes | **Yes** — ray attributes | Yes |
| C26 | Wave optics / diffraction | **Open** — see below | **Probably** | Yes |
| C27 | Scientific visualization | B8 | No | Yes |

¹ Only if ensemble-first is adopted; retrofitting it later is a modification.
² Only if B2 is defined functionally with polymorphism as adapter.
³ Only if propagation law is separate from field model (2.4).

### Reading the table

Nineteen of twenty-eight rows are **new component against existing interfaces** — the
desired outcome. Eight require modification, and they cluster into exactly three
architectural decisions:

1. **Ray attribute extensibility** (`C6`, `C25`, `C26`) — one decision.
2. **The step contract carrying error and control** (`C14`, and `C15` partially) — one
   decision.
3. **Result generality: streamable, typed, provenance-bearing** (`C21`, `C23`) — one
   decision.

Plus two localized items: explicit charts (`C16`) and derivative context for
time-dependent fields (`C11b`).

**This is the central finding of Phase 5.** The eight modifications are not eight problems.
They are three contract decisions that, if made once at the B1/B3 boundaries, convert the
remaining capabilities into extensions. The footnoted conditionals (`C5`, `C18`, `C19`)
follow the same logic: each is an extension *if* a single early decision went the right
way, and a refactor otherwise.

### The genuine exception

**`C26` (wave optics) remains open, and this document does not close it.** It is the one
capability where the required architecture cannot be responsibly derived yet, because the
scientific formulation is undecided (Phase 4 `Q3`). The target's obligation is narrower and
achievable: **do not preclude it.** Concretely, that means ray attributes must be
extensible enough to carry phase and amplitude, results must be able to be fields rather
than paths, and B3 must not assume the evolved object is a real-valued trajectory. Whether
wave optics is a new propagation law, a parallel computational branch, or a hybrid where
geometric propagation accumulates phase for a diffraction integral evaluated at the
observation plane is a scientific question that must be answered before it is an
architectural one.

---

## 7. Target Physics Architecture

### The proposed structure

Three separated concerns within B2:

```text
Field Model          what the gravitational environment is
      +
Propagation Law      how a ray's attributes evolve in that environment
      +
Geometry             where the lens, source, and observer are
```

### Why the field model and propagation law must be separate

Justified in 2.4 and restated here as the architectural claim: the current architecture
makes "physics" synonymous with "Christoffel symbols," and Phase 4 rated three separate
capabilities **Contradicted** as a direct consequence (`C24` non-vacuum, `C25` wavelength
dependence, `C26` wave optics). Solar corona plasma refraction alone justifies the split —
it is real SGL physics, wavelength-dependent, and inexpressible through a metric.

The propagation law consumes the field model and may consume additional physical
influences. Geodesic motion becomes one propagation law among several rather than the
definition of physics.

### Geometric optics and wave optics

Geometric optics is a propagation law. Wave optics probably is not — it likely changes the
evolved object rather than the evolution rule. The architecture's commitment is limited to
not foreclosing it (Section 6).

### What is deliberately not generalized

- **No plugin registry or dynamic model discovery.** Models are known at build time. No
  Phase 4 capability requires runtime discovery.
- **No abstract "physical effect" composition framework.** Adding effects to a propagation
  law is a concrete need; a general composition algebra for arbitrary effects is not
  justified by anything in Phase 4.
- **No unified "theory of gravity" abstraction.** SGL needs better *solar* models and
  possibly alternative spacetimes. It does not need a general framework for arbitrary
  gravitational theories.

---

## 8. Target Numerical Architecture

### The composition

```text
Propagation Law  +  Numerical Method  →  Propagation
```

confirmed as correct, and already partially achieved: `RK4Integrator.cpp` references no
physical concept, and `TrajectorySolver` accepts any `Integrator&`. The target keeps this
and repairs two things.

### Repair 1 — the step contract must carry diagnostics

The current signature is:

```text
step(state, dt, derivative) → State
```

This returns the answer and nothing else. Adaptive integration (`C14`) requires the method
to report a local error estimate and the controller to accept, reject, or resize a step.
There is no channel for any of that, which is why Phase 4 rated `C14` **Contradicted**
despite the boundary being clean. The boundary is in the right place with the wrong shape.

The target contract carries the advanced state, an error estimate, and step diagnostics.
Fixed-step methods report zero error and are the degenerate case — exactly the
degenerate-case pattern used for ensembles in 2.7.

### Repair 2 — the solver must not own progression policy

Today `TrajectorySolver` owns a fixed loop over `max_steps` with constant `dt`, so the
progression policy is baked into the orchestrator. In the target, step-size control is a
replaceable component collaborating with the method, and the loop merely drives it.

### What this makes possible without further change

Higher-order and symplectic methods; embedded error-estimating pairs; dense output for
event localization and image accumulation; and the trajectory-as-continuous-object model
Phase 4 identified as necessary once steps stop being uniform.

### Precision and state representation

Precision (`C15`) is a **localized policy**, not a framework-wide templating exercise.
Reference calculations need extended precision; production does not; GPU may want reduced
precision. Templating everything is rated **Premature** in Section 13. The requirement is
only that the scalar type be nameable in one place rather than spelled `double` in fifty.

State representation follows the ray-attribute decision (2.1) and is not a separate
numerical concern.

### What is deliberately not generalized

- **No generic ODE framework.** SGL integrates ray propagation. A general-purpose
  differential-equation library is not a Phase 4 requirement.
- **No abstract "solver" concept above method + execution.** It would have exactly one
  meaning and would recreate the pipeline god-module.

---

## 9. Target Execution Architecture

### The minimum abstraction

This section deliberately proposes **less** than the prompt's list of execution modes might
suggest, because the requirement is independence, not capability:

> The scientific abstraction must remain independent of the eventual execution strategy.

Independence is achieved by two properties, not by an execution framework:

**1. The unit of work is an ensemble, not a ray.** If the natural call propagates N rays,
then N=1 is serial, N=10⁶ is batch, N distributed across nodes is HPC, and N on a device is
GPU — all without the scientific description changing. If the natural call propagates one
ray, every scaling strategy is a retrofit. This single decision is what makes `C18`, `C19`,
and `C20` extensions.

**2. Physics is expressible without runtime polymorphism.** Defined as plain functions over
plain data, with virtual interfaces as adapters. On CPU this costs nothing and preserves
CPU flexibility; on GPU it is the difference between possible and impossible.

That is the entire minimum. **No device abstraction layer, no memory manager, no scheduler
abstraction, no unified kernel language.** Those are implementation concerns of individual
execution strategies, and Section 13 rates them Premature.

### Consequences for the data model

- Results must be streamable, not materialized (`C21`), because a device or a remote node
  cannot return a `std::vector` of a billion states.
- Ensembles must describe extent and identity without prescribing residency.
- Determinism must be a stated property of each strategy (Phase 4 `Q10`). The serial CPU
  reference path is permanently authoritative, and any strategy that departs from
  bit-reproducibility must say so.

---

## 10. Target Observation Architecture

### Three distinct concepts

Phase 4 traces the progression from photon trajectories to observable quantities to images
and reconstructed observations. These must be **separate architectural concepts**, because
they answer different questions, have different lifetimes, and change for different reasons:

```text
Propagation   →   Observation   →   Instrument / Measurement
  (B2 + B3)          (B6)                   (B6)

  where rays go    what arrives         what is recorded
  physics of       physics of the       properties of a
  spacetime        light field          detector
```

Trajectories are intermediates; observables are physical predictions; measurements are
instrument-dependent. Comparing three instruments against one physical prediction is a core
mission-design question — impossible if instrument response is fused into image formation.
Comparing two gravitational models against one instrument is equally core — impossible if
observation is fused into propagation.

### Why observation is a consumer, never a driver

Observation depends on results and on geometry. Nothing in propagation or numerics depends
on observation. This is the property that keeps Stage 4 from destabilizing Stage 1, and it
is enforced by the geometry/formation cut described in Section 4.

### The assumption being retired

The current architecture's invariant — `State → solve → vector<State> → SimulationResult` —
makes trajectory the universal output. In the target, a trajectory is one result payload
among several, and for large ensembles it is frequently *not* the retained one: terminal
states, event crossings, or accumulated intensity may be all that is kept. Retaining a full
history per ray is a choice expressed in the problem, not a property of the framework.

---

## 11. Target Validation Architecture

### Direction of dependency

Validation depends on the simulation. The simulation depends on nothing from validation —
no hooks, no instrumentation, no test-only branches in the propagation path.

### What the architecture must provide

Validation needs exactly three things from the framework, and providing them costs the
production path nothing:

1. **Results with provenance** — what was computed, with which model, method, and version.
2. **Parameter control** — the ability to re-run with refined settings, which is the
   Experiment layer (`C17` convergence studies are sweeps over resolution).
3. **Analytical references** — closed-form predictions from B2, callable without running a
   simulation. Phase 4 noted the extraction dropped Penrose's `analytical_freefall_time`
   and nothing replaced it.

### The distinction that must be structural

Phase 4 established that conservation checks and accuracy checks catch different failures:
an integrator can conserve `E` and `L` to `1e-14` while computing the wrong deflection
angle. The target treats these as different validation *kinds*:

| Kind | Detects | Reference needed |
|---|---|---|
| Conservation / constraint | Integration drift, constraint violation | None — internal consistency |
| Analytical benchmark | Systematically wrong physics | Closed-form solution |
| Convergence | Wrong order of accuracy, non-convergence | Refinement sequence |
| Cross-method | Method-specific artifacts | A second method |
| Published comparison | Modeling errors | External literature |

Only the first exists today. The target requires all five to be expressible as validation
cases without any of them being privileged by the engine.

### Tolerance as data

Tolerances belong to validation cases, with recorded justification — not as literals in
test code. Phase 4 flagged the current `1e-3` threshold against `1e-14` observed drift with
no rationale as exactly the pattern to avoid.

---

## 12. Target Visualization Architecture

### Required direction

```text
Scientific computation → Scientific results → Visualization
```

Visualization is a pure consumer and a leaf. Nothing depends on it.

### Does the current architecture satisfy this?

**Structurally yes, in residue no.** No visualization code exists — the extraction excluded
Penrose's GPU rendering, GLFW/OpenGL viewers, and trajectory visualization deliberately, as
recorded in `README.md`. There is no renderer to depend on anything.

But the requirement is already violated in the only way it can be with no renderer present.
`SimulationResult` and `SimulationMetadata` carry `characteristic_radius`, `horizon_radius`,
`photon_sphere_radius`, `coordinate_chart`, and `name` — written on every run, read by
nothing. Phase 1 identified their former consumer: Penrose's
`SimulationTrajectoryAdapter.h`, which converted `SimulationResult` into scene data. The
renderer is gone; the shape it imposed remains.

This is evidence, not a defect to be scored: it demonstrates that this physics result type
has previously absorbed presentation concerns, which is precisely what the target boundary
must prevent.

### Target boundary

Results carry scientific content and provenance only. Anything a renderer needs that is not
scientific content — display scales, reference radii, labels — belongs to the visualization
layer and is derived there from physics parameters it can read for itself. If a renderer
needs a quantity that is genuinely scientific, it is an analysis product (B7), computed by
an analysis and not smuggled into the result envelope.

---

## 13. Minimality Analysis

Mandatory section. Every abstraction proposed above is tested against: *what future
requirement justifies this?* Abstractions without a strong answer are removed.

### Necessary

Required for long-term extensibility. Removing any of these makes one or more Phase 4
capabilities a refactor rather than an extension.

| Abstraction | Justifying requirement | What breaks without it |
|---|---|---|
| Ray with extensible attributes | `C6`, `C25`, `C26` | Wave optics, wavelength dependence, and bundles each require modifying a universal struct. |
| Explicit chart | `C16`, `C9` | `X[1]`/`X[2]` convention replicated across six components; a second chart is a silent-error hazard. |
| Field model / propagation law separation | `C24`, `C25`, `C26` | All physics must be a Christoffel symbol; plasma refraction is inexpressible. |
| Lens / source / observer geometry | `C3`, `C4`, `C10`, `C11` | Stage 2 unreachable; SGL's defining geometry unrepresentable. |
| Propagation Problem | `C3`, `C4`, `C22`, `C23` | No home for the scientific question; every new question modifies the pipeline. |
| Ensemble as unit of work | `C5`, `C18`, `C19`, `C20` | Scaling becomes a retrofit of the entire execution path. |
| Step contract with error/control | `C14`, `C15`, `C17` | Adaptive integration and convergence studies impossible without changing every method. |
| Result envelope + provenance | `C7`, `C21`, `C22`, `C23` | Results are trajectory-only and unreproducible. |
| Streamable results | `C21`, `C19`, `C20` | Ensemble-scale runs bounded by RAM. |
| Observation as a distinct layer | `C7`, `C8`, `C9` | Image formation fuses into propagation. |
| Physics in functional form | `C19` | GPU execution requires rewriting the physics. |

Eleven abstractions. Six of them already exist in some form in the current codebase
(`Metric` → field model, `DynamicsModel` → propagation law, `Integrator` → step contract,
`TerminationPolicy` → events, `State` → ray, `SimulationResult` → result envelope). The
target is closer to a reshaping of existing boundaries than to a new architecture.

### Useful

Meaningful flexibility, not currently essential. Adopt when a second concrete case appears
— not before.

| Abstraction | Why useful | Why not yet necessary |
|---|---|---|
| Event localization (sub-step) | Accurate crossing detection under adaptive steps | Simple termination suffices until adaptive integration exists. |
| Ensemble neighbour topology | `C6` bundles, local magnification | Needed at Stage 3; premature to fix its shape now. |
| Named scalar type | `C15` precision variation | One place to change beats fifty, but full templating is not justified. |
| Result persistence format | `C20`, `C22`, `C23` | Required eventually; committing to a format early risks encoding today's trajectory-only assumption into stored data. |
| Instrument as its own boundary | `C9` | The slot must exist; its contents should not be guessed at Stage 1. |
| Analytical reference library | `C2`, `C17` | High value, low architectural cost — it is a consumer, addable anytime. |

### Premature

Adds abstraction without sufficient justification. Explicitly rejected.

| Rejected abstraction | Why premature |
|---|---|
| Device/backend abstraction layer | Section 9 shows independence comes from ensemble-as-unit plus functional physics. A backend layer would be an interface with one implementation and speculative semantics. |
| Plugin registry / runtime model discovery | No Phase 4 capability requires models unknown at build time. |
| Generic ODE framework | SGL integrates ray propagation. Generalizing beyond that serves no stated requirement. |
| Full multi-precision templating | `C15` needs precision to *vary*, not to be a universal type parameter. Templating everything costs compile time, readability, and debuggability for a need satisfied more cheaply. |
| Abstract Analysis base class | Analyses are functions over results. The existing header-only observables are the right precedent. |
| Generic serialization / schema registry | Provenance and config capture are required; a general schema system is not. |
| Mission simulation framework | Stage 8 is composition of Stages 1–7. Building the composition before the parts is inverted. |
| Unified "physical effect" algebra | Adding effects to a propagation law is concrete; a general algebra for arbitrary effects is speculative. |
| Central configuration object | Not merely unnecessary — actively harmful. It is the current architecture's primary coupling hotspot. |
| Dependency injection container | The composition seam (B4) is one thin component. |
| Abstract "Simulation" concept | Decomposes without remainder into Problem + Method + Execution + Result. |

### Minimality summary

Eleven necessary abstractions, six useful, eleven explicitly rejected. The rejections are
the substance of this section: an architecture that adopted all twenty-eight candidates
would be more abstract and less extensible, because each unbacked abstraction fixes a
guess about a future that has not arrived.

---

## 14. Current → Target Architectural Delta

Architectural deltas only. Not tasks, not sequenced, not a plan.

**Foundational** means the delta is expensive to make later because other things
accumulate on top of the current shape. **Optional** means it can be made whenever the
motivating capability arrives, at roughly constant cost.

---

### D1 — Missing problem layer

- **Current** — no representation of the scientific question. `SimulationConfig` fuses
  scenario selection, spacetime parameters, `dt`, and `max_steps`.
- **Target** — Propagation Problem (B2) states the science; method and execution
  parameters live elsewhere.
- **Why it must change** — with no home for the question, every new question lands in the
  central pipeline. This is the mechanism behind Phase 3's modification hotspots.
- **Motivated by** — `C3`, `C4`, `C22`, `C23`.
- **Foundational.**

---

### D2 — Configuration as a central dependency

- **Current** — `SimulationConfig.h` is included by the pipeline, the builders, and every
  consumer, and also declares `run_simulation`. Phase 2 identified it as a coupling
  hotspot.
- **Target** — no shared configuration type. Per-layer parameters; Experiment composes and
  serializes.
- **Why it must change** — a shared config struct is a channel through which any layer's
  concerns reach every other layer.
- **Motivated by** — `C9`, `C15`, `C22` (each of which would otherwise widen the struct).
- **Foundational.**

---

### D3 — Result is trajectory-shaped and consumer-shaped

- **Current** — `SimulationResult` holds `std::vector<State> history` plus four write-only
  presentation fields inherited from a removed Penrose renderer.
- **Target** — typed result payloads with a provenance envelope; no presentation content.
- **Why it must change** — `C7` is **Contradicted** by the trajectory-only assumption, and
  the presentation residue violates Section 12's direction requirement.
- **Motivated by** — `C7`, `C21`, `C23`, `C27`.
- **Foundational** for generality; the presentation-residue removal alone is optional and
  cheap.

---

### D4 — Step contract carries no diagnostics

- **Current** — `step(...) → State`; the solver owns a fixed loop over `max_steps`.
- **Target** — step returns state plus error estimate plus diagnostics; step control is
  replaceable.
- **Why it must change** — no channel for adaptive integration. The boundary is correctly
  placed and wrongly shaped.
- **Motivated by** — `C14`, `C15`, `C17`.
- **Foundational** — every method implementation written before this change must be revised
  after it.

---

### D5 — Single ray is the base case

- **Current** — `run_simulation` propagates one ray and returns one result with full
  history.
- **Target** — ensemble is the unit; a single ray is N=1.
- **Why it must change** — determines whether `C18`, `C19`, and `C20` are extensions or
  retrofits.
- **Motivated by** — `C5`, `C6`, `C18`, `C19`, `C20`, `C21`.
- **Foundational.**

---

### D6 — Physics defined polymorphically

- **Current** — physics is defined *as* virtual interfaces; 64 virtual `christoffel` calls
  per derivative evaluation, 256 per accepted RK4 step, reached through a `std::function`.
- **Target** — physics defined as functions over plain data; polymorphism as an adapter.
- **Why it must change** — the current form is correct and clear on CPU and cannot execute
  device-side. Inverting definition and adapter costs nothing on CPU.
- **Motivated by** — `C19`, `C18`.
- **Foundational** in the specific sense that it is nearly free before physics
  implementations accumulate and increasingly expensive after.

---

### D7 — Implicit coordinate convention

- **Current** — `X[1]` is radius and `X[2]` is polar angle by convention, replicated across
  six components. `CoordinateChart` exists and has no caller.
- **Target** — chart is explicit and attached to data.
- **Why it must change** — a second chart under the current convention produces silent
  wrong answers rather than errors.
- **Motivated by** — `C16`, `C9`, `C24`.
- **Foundational** for correctness, though narrow in scope.

---

### D8 — No geometry concepts

- **Current** — the lens is a `double`; no source, no observer. `Constants::solar_radius_m`
  is the only solar-specific value and is unused.
- **Target** — lens, source, and observer as domain concepts in B2.
- **Why it must change** — Stage 2 and everything above it are unreachable.
- **Motivated by** — `C3`, `C4`, `C10`, `C11`.
- **Foundational** for SGL as a science project; it is the gap between "geodesic
  propagation code" and "SGL framework."

---

### D9 — No observation layer

- **Current** — output is trajectories; no observable, image, or instrument concept.
- **Target** — B6 as a distinct consumer layer.
- **Why it must change** — Stage 4 is where SGL answers its actual scientific question.
- **Motivated by** — `C7`, `C8`, `C9`.
- **Optional now, foundational at Stage 4.** What must exist early is the *slot* — nothing
  below may assume the output is a trajectory. That obligation is already carried by D3.

---

### D10 — Autonomous derivative

- **Current** — `compute_derivative(const State&)` and `christoffel(..., X)`: no time, no
  epoch, no environment.
- **Target** — a context channel for time-dependent fields.
- **Why it must change** — no way for time dependence to enter the computation.
- **Motivated by** — `C11b`, `C24` (rotating models).
- **Optional.** Observer *motion* (`C11a`) does not require it — that is geometry and
  frames. Only a genuinely time-dependent field does, and that is not near-term for a
  static solar model.

---

### D11 — Validation is a single executable

- **Current** — one smoke test with a hand-written `main()`, not registered with CTest
  (verified: no `enable_testing()` or `add_test()`), no analytical reference, no
  deflection-angle check.
- **Target** — validation cases as components consuming results, with references and
  justified tolerances.
- **Why it must change** — conservation checks alone cannot detect systematically wrong
  physics.
- **Motivated by** — `C2`, `C17`.
- **Optional architecturally, urgent scientifically.** It costs the production path
  nothing, and until it exists no numerical claim about SGL predictions is supported.

---

### D12 — No experiment layer

- **Current** — configuration is compile-time; every parameter change is a code change.
  `SimulationConfig::name` is written and never read.
- **Target** — B9 owning sweeps, batch runs, provenance, and datasets.
- **Why it must change** — research proceeds by controlled comparison.
- **Motivated by** — `C22`, `C17`, `C20`, `C23`.
- **Optional** — B9 sits at the top and depends downward, so it can be added late without
  disturbing lower layers. That is the payoff of the dependency direction in Section 4.

---

### Delta summary

Seven foundational (D1–D8, with D3 partially and D7 narrowly), five optional. The
foundational ones share a common character: each concerns a **contract** — what a ray is,
what a step returns, what a result is, what the unit of work is, how physics is defined.
The optional ones concern **layers** — observation, validation, experiment — which the
dependency direction permits adding later.

That asymmetry is the practical content of this document. Contracts are expensive to change
because implementations accumulate against them; layers are cheap to add because nothing
depends on them.

---

## 15. Architectural Invariants

Properties that should hold for the lifetime of the project. Each is falsifiable — stated
so a violation is recognizable.

**I1 — The data model depends on nothing.**
B1 imports no physics, no numerics, no execution, no rendering. *Violated when:* a result
type gains a field that only one consumer reads.

**I2 — Physics never names a numerical method.**
No field model or propagation law mentions a step size, tolerance, or integrator.
*Violated when:* a physics component takes `dt`.

**I3 — Numerics never names a physical model.**
Integration is defined over states and derivative callables. *Currently holds* and must
continue to. *Violated when:* a method special-cases a metric.

**I4 — Nothing below the result layer names a consumer.**
No physics, numerics, or execution component knows that observation, analysis, or
visualization exists. *Violated when:* a result carries a field for a renderer.

**I5 — Execution is chosen, never assumed.**
Scientific components contain no assumption about serial versus parallel, host versus
device, or one process versus many. *Violated when:* a physics component allocates or
assumes contiguous host memory.

**I6 — A single ray is the degenerate case.**
Every scaling concept is expressed for N and specialized to 1, never the reverse.
*Violated when:* an API exists only in single-ray form.

**I7 — A result is inseparable from its provenance.**
No result circulates without the record of what produced it. *Violated when:* a
computation returns bare data.

**I8 — The serial CPU reference path is permanently authoritative.**
Every accelerated strategy is verifiable against it. *Violated when:* a capability exists
only on an accelerated path.

**I9 — Determinism is the default and departures are declared.**
Currently holds — no RNG, no threading, no clock dependence. *Violated when:* a
non-deterministic reduction is introduced silently.

**I10 — Validation depends on the simulation; the simulation depends on nothing from
validation.**
*Violated when:* a test-only branch appears in the propagation path.

**I11 — The composition seam stays thin.**
If B4 accumulates physics or policy, a boundary elsewhere is wrong. *Violated when:*
model-specific algebra appears in orchestration — which is the current state of
`SimulationPipeline.cpp`, and the clearest single indicator this invariant is worth stating.

---

## 16. Target Architecture Verdict

> **If SGL were rebuilt or refactored toward this target architecture, would the future
> capabilities in Phase 4 primarily become extensions?**

**Yes — with three qualifications that are themselves the substance of the answer.**

### The quantitative shape

Of the twenty-eight capability rows in Section 6, nineteen become new components against
existing interfaces. Eight require modification, and those eight reduce to **three contract
decisions**: ray attribute extensibility, the step contract carrying error and control, and
result generality with provenance. One capability — wave optics — remains genuinely open.

That ratio is the answer. The target does not eliminate modification; it *concentrates*
it into a small number of decisions at the two most stable boundaries (B1 and B3), made
once, early, and deliberately.

### Qualification 1 — the benefit is front-loaded and the cost is too

The seven foundational deltas are all contract decisions, and contracts are cheap to shape
before implementations accumulate against them and expensive afterward. The target's value
is therefore highly sensitive to timing in a way the optional deltas are not. This cuts
favourably right now: with one metric, one integrator, and one termination policy in
existence, the accumulated weight against these contracts is close to zero. It will not
stay that way.

### Qualification 2 — wave optics is not resolved, only kept reachable

`C26` is the one capability where this document deliberately declines to derive an
architecture, because the scientific formulation is undecided (Phase 4 `Q3`). The target's
commitment is limited to not precluding it: extensible ray attributes, non-trajectory
results, and no assumption in B3 that the evolved object is a real-valued path. If wave
optics eventually requires a parallel computational branch rather than a new propagation
law, that will be a significant architectural event under any architecture. The target
reduces its blast radius; it does not eliminate it.

### Qualification 3 — the target is a reshaping, not a replacement

This is the most important qualification, and it argues *for* the current codebase rather
than against it. Six of the eleven necessary abstractions already exist in recognizable
form: `Metric` is a field model, `DynamicsModel` is a propagation law, `Integrator` is a
step contract, `TerminationPolicy` is an event, `State` is a ray, `SimulationResult` is a
result envelope. Phase 2 found the `DerivativeFunc` boundary to be genuinely strong, and
`RK4Integrator.cpp` already achieves complete physics independence — the single hardest
property in the target and the one most often gotten wrong.

The gap is not that the boundaries are in the wrong places. With two exceptions — the
missing problem layer (D1) and the missing geometry concepts (D8) — the boundaries are in
approximately the right places with the wrong **shapes**: a step that returns too little, a
result that carries too much, a ray that is too fixed, a unit of work that is too small.

### Assessment

The architecture described here is a modest, mostly-existing structure with three contracts
corrected and two layers added. It is achievable from the current codebase, and it becomes
harder each time an implementation accumulates against an uncorrected contract.

The honest counter-position deserves recording: this document proposes eleven necessary
abstractions for a codebase that currently has four, and Phase 3 observed that the existing
four are each backed by exactly one implementation — their generality asserted rather than
demonstrated. The same critique applies with equal force here, and Section 13 is the
response: eleven candidate abstractions were rejected outright, and each retained one names
the specific Phase 4 capability that forces it. Whether that discipline held is the right
question to ask of this document, and it should be re-asked whenever a second concrete
implementation of any boundary finally appears — at that point the abstraction stops being
a prediction and starts being evidence.

---

## Document Status

Architectural destination. No implementation path, no sequencing, no task decomposition.

All statements about the current implementation are traceable to Phase 1–3 verification:
full source reading, `#include` tracing, linker-symbol enumeration, a clean out-of-tree
build and run, file-level diffs against the originating Penrose tree, targeted absence
checks, and the graph in `graphify-out/`.

Open questions from Phase 4 (`Q1`–`Q12`) remain open. This document deliberately answers
`Q1` (results are plural and typed), `Q2` (both framings must be expressible), and `Q4`
(the atomic unit is the ensemble) because Section 6 cannot be derived without them. It
leaves `Q3` (wave optics), `Q5` (state representation mechanism), `Q6` (mission boundary),
`Q8` (persisted format), `Q10` (determinism under parallelism), and `Q12` (required
accuracy) open, and none of these should be closed by architectural reasoning alone.

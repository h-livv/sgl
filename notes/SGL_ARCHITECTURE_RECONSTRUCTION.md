# SGL Architecture Reconstruction

> **Historical snapshot.** Phase 2 analysis of an earlier kernel-centric tree.
> Not the current 1D/2D image pipeline. Living docs:
> [`docs/SGL_FORWARD_PIPELINE.md`](../docs/SGL_FORWARD_PIPELINE.md),
> [`docs/HOW_THE_EINSTEIN_RING_IS_FORMED.md`](../docs/HOW_THE_EINSTEIN_RING_IS_FORMED.md).

Phase 2 architectural reconstruction. This is an analysis artifact, not the
final architectural review.

Source of truth: the implementation in this repository. Phase 1 notes at
`notes/ARCHITECTURE_RECONSTRUCTION.md` were used as a map and important claims
were re-checked against the source and the graphify repository graph.

## 1. Conceptual Architecture

SGL currently consists of a small Schwarzschild geodesic propagation kernel with
one orchestration layer. Its architecture is best described as:

```text
caller-owned configuration and scenario parameters
        |
        v
Schwarzschild simulation pipeline
        |
        +--> initial-state builders
        +--> metric construction
        +--> geodesic dynamics
        +--> trajectory solver
                 |
                 +--> termination policy
                 +--> integrator
                 +--> optional post-step state projection
        |
        v
SimulationResult containing trajectory history
```

The implementation does not contain an SGL optical pipeline, rendering system,
experiment runner, backend abstraction, or external configuration layer. The
directories for those concepts are placeholders only.

### Architectural Layers That Actually Exist

1. **Core scientific vocabulary**
   - `State`
   - `Metric`
   - `MetricKind`
   - `CoordinateChartKind`
   - `SchwarzschildParameters`
   - physical constants and units comments

2. **Physical model**
   - `SchwarzschildMetric`
   - `GeodesicDynamics`
   - Schwarzschild initial-state builders
   - Schwarzschild observable formulas

3. **Numerical propagation**
   - `Integrator`
   - `RK4Integrator`
   - `TrajectorySolver`
   - `TerminationPolicy`

4. **Simulation orchestration**
   - `SimulationConfig`
   - `run_simulation(...)`
   - anonymous-namespace helpers in `SimulationPipeline.cpp`
   - `SimulationResult`

5. **Single executable consumer**
   - `tests/null_geodesic_smoke.cpp`

### Boundary Types

| Boundary | Actual status | Reason it exists |
|---|---|---|
| Metric -> dynamics | Intentional abstraction | `GeodesicDynamics` needs Christoffel symbols but not the concrete metric class. |
| Dynamics -> solver | Intentional abstraction | `TrajectorySolver` integrates a generic derivative model. |
| Solver -> integrator | Intentional abstraction | `TrajectorySolver` delegates stepping to an `Integrator`. |
| Solver -> termination | Intentional abstraction | `TrajectorySolver` delegates stop conditions to a `TerminationPolicy`. |
| Pipeline -> physics components | Implementation wiring | `SimulationPipeline.cpp` chooses the only metric, builder, solver, and policy. |
| Config -> pipeline | Domain/control data fused with API | `SimulationConfig.h` holds the config structs and the public `run_simulation` declarations. |
| State -> every subsystem | Domain concept and implementation detail at once | `State` is the physics state, the numerical vector, and the trajectory storage element. |
| Result -> visualization/analysis | Vestigial boundary | `SimulationResult` still carries metadata fields, but no renderer or analyzer consumes them in SGL. |

### Intentional Abstractions

- `Spacetime::Metric`: abstracts Christoffel-symbol lookup.
- `Dynamics::DynamicsModel`: abstracts the ODE right-hand side.
- `Integration::Integrator`: abstracts a fixed-step state advancement.
- `Simulation::TerminationPolicy`: abstracts stopping predicates.
- `Integration::DerivativeFunc`: erases a `DynamicsModel` into the function signature consumed by the integrator.

### Accidental or Residual Abstractions

- `MetricKind` and `CoordinateChartKind`: single-enumerator identity vocabularies written into metadata but not read.
- `SimulationMetadata`: populated in `SimulationPipeline.cpp`, but no SGL component consumes it.
- `SimulationResult::characteristic_radius`, `name`, `spacetime`, and `metadata`: written, then unused in-repo.
- `CoordinateChart`: compiled and exported, but has no caller.
- `RadiusBoundTermination`: implemented and exported, but has no caller.
- `TrajectorySolver::propagate`: implemented and exported, but has no caller.
- `PhysicalConstants.h` and `Units.h`: present as vocabulary, but unused by implementation.

### Implementation Details

- `SimulationPipeline.cpp` owns the concrete Schwarzschild wiring through
  anonymous-namespace functions.
- `RK4Integrator.cpp` owns the process-global immutable default integrator.
- `TrajectorySolver` uses a mutable `State current` and optionally lets a
  callback mutate it after each step.
- Coordinate ordering is implicit in direct vector indexes: `X[0] = t`,
  `X[1] = r`, `X[2] = theta`, `X[3] = phi`.

### Domain Concepts

- Geodesic state: position and tangent in a coordinate chart.
- Schwarzschild radius/parameter: represented as `SchwarzschildParameters::mass`
  but used as `rs`.
- Scenario: one of bound orbit, radial freefall, null scatter, custom.
- Geodesic kind: timelike or null.
- Trajectory: an ordered sequence of `State`.
- Conserved quantities: energy, angular momentum, null Hamiltonian, critical
  impact parameter.

## 2. Subsystem Model

### Core State and Vocabulary

- **Responsibility:** provide the state vector and shared enum/parameter
  vocabulary.
- **Ownership:** header-only value definitions owned by `physics/core`.
- **Inputs:** none, except constructor arguments for `State`.
- **Outputs:** `State`, `MetricKind`, `CoordinateChartKind`,
  `SchwarzschildParameters`.
- **Dependencies:** Eigen for `State` and `Metric`; otherwise none.
- **Consumers:** all major subsystems consume `State`; pipeline and metadata use
  metric/chart identity; initial builders and metric construction use
  `SchwarzschildParameters`.
- **Lifecycle:** value objects are caller-owned, stack-created, copied or moved.
- **Extension mechanism:** adding fields/enums or new core types requires direct
  header modification. The core model is reusable but not insulated from
  consumers because fields are public.

Boundary significance: this is the conceptual center of the system. It is also
the implementation center because the same `State` type is used for physical
meaning, numerical arithmetic, and storage.

### Metric Model

- **Responsibility:** map a coordinate point to Christoffel coefficients.
- **Ownership:** `Spacetime::Metric` defines the interface; `SchwarzschildMetric`
  is the only implementation.
- **Inputs:** constructor parameter `rs`; per-call coordinate vector `X`.
- **Outputs:** one `double` Christoffel coefficient.
- **Dependencies:** Eigen and `Metric`.
- **Consumers:** `GeodesicDynamics`; `SimulationPipeline.cpp` constructs the
  concrete metric.
- **Lifecycle:** metric is heap-allocated in a `std::unique_ptr<Metric>` inside
  the pipeline, borrowed by `GeodesicDynamics`, destroyed at the end of
  `integrate_schwarzschild`.
- **Extension mechanism:** strong at the dynamics boundary, weak at the pipeline
  boundary. A new `Metric` subclass can satisfy `GeodesicDynamics`, but
  `SimulationPipeline.cpp` must be changed to construct and configure it.

Boundary significance: the metric abstraction is real because dynamics consumes
only the abstract `Metric`. It is unexercised because there is only one concrete
metric in SGL.

### Geodesic Dynamics

- **Responsibility:** convert metric geometry into a first-order derivative
  `State`.
- **Ownership:** interface in `DynamicsModel.h`; implementation in
  `GeodesicDynamics`.
- **Inputs:** `State` and a borrowed `Metric`.
- **Outputs:** derivative `State(state.U, acceleration)`.
- **Dependencies:** `State`, `Metric`.
- **Consumers:** `TrajectorySolver`, via `DynamicsModel`.
- **Lifecycle:** constructed on the stack in `integrate_schwarzschild`; borrowed
  by `TrajectorySolver` for the duration of one call.
- **Extension mechanism:** strong at solver boundary. A new `DynamicsModel` can
  be supplied to `TrajectorySolver` directly. Conditional at pipeline boundary,
  because `run_simulation` does not accept arbitrary dynamics.

Boundary significance: this is the physical ODE boundary. It cleanly separates
the geodesic equation from numerical stepping, but it is still coupled to the
global `State` representation and implicit coordinate convention.

### Numerical Integration

- **Responsibility:** advance a `State` by one fixed step using a derivative
  function.
- **Ownership:** interface in `Integrator.h`; default implementation in
  `RK4Integrator`.
- **Inputs:** current `State`, `dt`, `DerivativeFunc`.
- **Outputs:** next `State`.
- **Dependencies:** `State`, `std::function`.
- **Consumers:** `TrajectorySolver`.
- **Lifecycle:** default integrator is a file-local static
  `const RK4Integrator`; callers can also pass any `Integrator&`.
- **Extension mechanism:** strong. New fixed-step integrators can implement
  `Integrator` and be passed to `TrajectorySolver`.

Boundary significance: this is the strongest boundary in the current
architecture. The integrator knows nothing about Schwarzschild, metrics,
geodesics, scenarios, or simulation configs.

### Trajectory Solver and Termination

- **Responsibility:** own the integration loop, apply termination, optionally
  apply a post-step mutation hook, and return either full history or final state.
- **Ownership:** `TrajectorySolver` static functions; `TerminationPolicy`
  interface and two implementations.
- **Inputs:** initial `State`, `DynamicsModel`, `TerminationPolicy`, `dt`,
  `max_steps`, `Integrator`, optional `post_step`.
- **Outputs:** `std::vector<State>` from `solve`; final `State` from
  `propagate`.
- **Dependencies:** all four interface families converge here.
- **Consumers:** `SimulationPipeline.cpp` calls `solve`; no in-repo code calls
  `propagate`.
- **Lifecycle:** local `current` state is copied from the initial state,
  repeatedly replaced by integrator output, optionally mutated, then copied into
  history.
- **Extension mechanism:** strong for new dynamics/integrators/policies when
  calling `TrajectorySolver` directly; conditional through the public
  `run_simulation` API because the pipeline chooses the components.

Boundary significance: this is the numerical orchestration boundary. It is not
physics-specific, except through shared dependence on `State`.

### Initial Conditions

- **Responsibility:** translate scenario-specific parameters into an initial
  `State` satisfying timelike or null constraints.
- **Ownership:** four POD input structs in `InitialConditions.h`; builder
  functions in `SchwarzschildInitialStateBuilders`.
- **Inputs:** `SchwarzschildParameters`, one IC struct; `build_custom` also
  consumes `SimulationConfig`.
- **Outputs:** initial `State`.
- **Dependencies:** `State`, `SchwarzschildParameters`, and for custom ICs
  `SimulationConfig`.
- **Consumers:** only `SimulationPipeline.cpp`.
- **Lifecycle:** IC structs are caller-owned and borrowed by const reference;
  builders return `State` by value.
- **Extension mechanism:** weak. New scenarios require new IC structs, new
  builder functions, new `Scenario` values, and new `run_simulation` overloads
  or branches.

Boundary significance: this is partly a domain boundary and partly pipeline
implementation. The boundary exists because each scenario has different input
fields and normalization formulas. It is not independent of the pipeline because
`build_custom` reads `config.geodesic`.

### Simulation Pipeline

- **Responsibility:** validate config, construct the concrete components, invoke
  the solver, populate `SimulationResult`.
- **Ownership:** public declarations in `SimulationConfig.h`; implementation and
  all helper functions in `SimulationPipeline.cpp`.
- **Inputs:** `SimulationConfig`, `SchwarzschildParameters`, one IC struct.
- **Outputs:** `SimulationResult`.
- **Dependencies:** metric, dynamics, solver, integrator, termination policy,
  initial-state builders, `SimulationMetadata`.
- **Consumers:** only the smoke test in-repo.
- **Lifecycle:** one function call creates all transient objects and returns a
  value result.
- **Extension mechanism:** weak. The public API is overload-based and
  Schwarzschild-specific; changing the orchestration means editing
  `SimulationPipeline.cpp`.

Boundary significance: this is the high-level computation entry point. It is
not a generic simulation engine; it is a private Schwarzschild assembly path
behind a small public API.

### Observables

- **Responsibility:** compute conserved quantities and null Hamiltonian checks
  from `State`.
- **Ownership:** header-only inline functions in
  `physics/validation/observables/SchwarzschildObservables.h`.
- **Inputs:** `State`, `rs`.
- **Outputs:** scalar diagnostic values.
- **Dependencies:** `State`, `SchwarzschildParameters` include, `<cmath>`.
- **Consumers:** smoke test only.
- **Lifecycle:** no state.
- **Extension mechanism:** conditional. New observables can be added as
  functions, but there is no analysis interface, result type, or registration
  mechanism.

Boundary significance: this is an analysis helper library, not an analysis
subsystem. It is outside the simulation pipeline and is joined to it only by
consumer code.

### Coordinate Chart Utilities

- **Responsibility:** convert `State` between Cartesian and spherical
  coordinates using Jacobians.
- **Ownership:** free functions under namespace `CoordinateChart`.
- **Inputs:** `State` or spherical coordinates.
- **Outputs:** transformed `State` or Jacobian matrix.
- **Dependencies:** `State`, Eigen.
- **Consumers:** none in-repo.
- **Lifecycle:** stateless functions.
- **Extension mechanism:** missing as an architecture; functions can be added,
  but no subsystem currently depends on chart conversion.

Boundary significance: this is a compiled utility island. It is not part of the
actual execution model.

## 3. Dependency Model

### Subsystem Dependency Graph

```text
Smoke executable
    -> Simulation pipeline
    -> Observables

Simulation pipeline
    -> Initial-condition builders
    -> Schwarzschild metric
    -> Geodesic dynamics
    -> Trajectory solver
    -> HorizonTermination
    -> default RK4 integrator
    -> SimulationConfig / SimulationResult / SimulationMetadata

Initial-condition builders
    -> State
    -> SchwarzschildParameters
    -> SimulationConfig (custom case only)

Trajectory solver
    -> State
    -> DynamicsModel
    -> TerminationPolicy
    -> Integrator
    -> post_step callback

Geodesic dynamics
    -> State
    -> Metric

Schwarzschild metric
    -> Metric
    -> Eigen::Vector4d

RK4 integrator
    -> State
    -> DerivativeFunc

Observables
    -> State
    -> SchwarzschildParameters

CoordinateChart
    -> State
    -> Eigen
    -> no consumers
```

### Central Modules

- `State`: central data object and dependency hub.
- `SimulationPipeline.cpp`: central assembly point and coupling hotspot.
- `TrajectorySolver`: central numerical execution point.
- `SimulationConfig.h`: central public API and data contract header.

### Stable Boundaries

- `Integrator` boundary: stable because it is narrow and physics-free.
- `DynamicsModel` boundary: stable for autonomous derivative models using
  `State`.
- `Metric` boundary: stable for Christoffel-based metrics using the same
  coordinate-state representation.
- `TerminationPolicy` boundary: stable for single-state stop predicates.

### Unstable Boundaries

- `SimulationConfig` boundary: unstable because it encodes fixed scenario and
  metric enumerations and is also the API declaration header.
- Initial conditions boundary: unstable because scenario types and
  `run_simulation` overloads must move together.
- `SimulationResult` boundary: unstable because most fields are not consumed
  by any current subsystem, so their real contract is inherited rather than
  validated.
- Coordinate chart boundary: unstable because it has no consumer.

### Cycles

There is no hard header include cycle. There is one architectural cycle:

```text
SimulationConfig.h
    -> InitialConditions.h

SchwarzschildInitialStateBuilders.cpp
    -> SimulationConfig.h
```

The cycle exists only at the implementation level and only because
`build_custom` reads `config.geodesic`. Its significance is that low-level
initial-state construction depends on high-level simulation configuration for
one path.

### Coupling Hotspots

- `SimulationPipeline.cpp`: concrete metric selection, concrete dynamics
  construction, termination selection, null projection, result metadata, and
  scenario validation all live here.
- `State`: every physical, numerical, and result-storage concept shares the
  same representation.
- Raw coordinate indexing: metric, dynamics, builders, observables,
  termination policies, projection callback, and chart utilities all assume
  the same vector layout.

### Inappropriate or Mismatched Dependencies

This section records architectural mismatches without prescribing changes.

- `InitialStateBuilders.cpp` depends on `SimulationConfig` for `GeodesicKind`.
  This reverses the otherwise downward dependency direction.
- `TrajectorySolver.h` includes `RK4Integrator.h` only to expose a default
  integrator argument. That makes the generic solver header know the concrete
  default implementation.
- `SimulationConfig.h` declares `run_simulation`, making the configuration and
  API boundary inseparable.
- `SimulationPipeline.cpp` duplicates Schwarzschild formulas also present in
  observables and builders.
- `Observables.h` includes `SchwarzschildParameters.h` but its functions accept
  `double rs`; the parameter type is not actually part of the function
  signatures.

## 4. Data Model

### `State`

- **Represents:** position `X` and tangent `U` in a chosen coordinate chart.
- **Created by:** initial-state builders, coordinate transform functions,
  arithmetic operators, integrator stages.
- **Owned by:** callers, solver local variables, trajectory vectors.
- **Mutated by:** `TrajectorySolver` reassigns `current`; `post_step` mutates
  `State&`; no other in-repo function mutates a caller-owned state.
- **Consumed by:** metric, dynamics, integrator, termination policies, builders,
  observables, chart utilities, result storage.
- **Domain-level or implementation-level:** both. It is the scientific state and
  the numerical vector container.
- **Reusable across subsystems:** yes, but only for systems accepting the same
  4-vector layout and coordinate convention.
- **Independence:** not independent from numerical algorithms because it embeds
  arithmetic operations specifically used by RK4; not independent from physics
  because indexes encode spacetime coordinates; independent from visualization
  only because no visualization code exists.

### `SchwarzschildParameters`

- **Represents:** the Schwarzschild radius `rs` in geometrized units, despite
  field name `mass`.
- **Created by:** caller code.
- **Owned by:** caller; borrowed by pipeline and builders.
- **Mutated by:** no in-repo component.
- **Consumed by:** pipeline, metric construction, initial-state builders,
  metadata builder.
- **Domain-level or implementation-level:** domain-level parameter object.
- **Reusable across subsystems:** yes, but only for Schwarzschild-specific code.
- **Independence:** independent from numerical algorithm and backend;
  intertwined with configuration because the public API is specialized to it.

### Initial Condition Structs

- **Represents:** scenario-specific input parameters for bound orbit, radial
  freefall, null scatter, or custom state construction.
- **Created by:** caller code.
- **Owned by:** caller; passed by const reference.
- **Mutated by:** no in-repo component.
- **Consumed by:** corresponding builder function.
- **Domain-level or implementation-level:** domain-level inputs.
- **Reusable across subsystems:** limited. They are reusable only with the
  existing Schwarzschild builder formulas.
- **Independence:** independent from numerical backend; coupled to scenario
  enumeration and `run_simulation` overloads.

### `SimulationConfig`

- **Represents:** a mixed control object: metric identity, scenario identity,
  geodesic kind, step size, step limit, horizon safety factor, solver options,
  and a result name.
- **Created by:** caller code.
- **Owned by:** caller; borrowed by pipeline and custom builder.
- **Mutated by:** caller before execution only.
- **Consumed by:** `require_spacetime`, `make_schwarzschild_post_step`,
  `integrate_schwarzschild`, `build_custom`.
- **Domain-level or implementation-level:** mixed. `scenario` and `geodesic`
  are domain concepts; `dt`, `max_steps`, and projection interval are numerical
  control; `name` is result metadata.
- **Reusable across subsystems:** reusable as the current public API input, but
  not a backend-neutral or file-format-independent configuration model.
- **Independence:** not independent from numerical algorithm or physical model;
  it assumes fixed-step integration and Schwarzschild-only execution.

### `SimulationMetadata`

- **Represents:** metric/chart identity and derived Schwarzschild radii.
- **Created by:** `schwarzschild_metadata`.
- **Owned by:** `SimulationResult`.
- **Mutated by:** populated once during result creation.
- **Consumed by:** no in-repo component.
- **Domain-level or implementation-level:** intended domain/result metadata;
  operationally residual data.
- **Reusable across subsystems:** unverified by current implementation.
- **Independence:** independent from numerics; shaped for absent presentation or
  downstream consumers.

### `SimulationResult`

- **Represents:** completed simulation output.
- **Created by:** `integrate_schwarzschild`.
- **Owned by:** returned by value to caller.
- **Mutated by:** populated inside pipeline; not mutated afterward in current
  code.
- **Consumed by:** smoke test reads only `history`.
- **Domain-level or implementation-level:** mixed. `history` is computational
  output; other fields are domain/presentation metadata.
- **Reusable across subsystems:** currently only as trajectory storage.
- **Independence:** independent from rendering by absence, but retains
  visualization-shaped fields; coupled to in-memory `std::vector<State>` and
  therefore not backend-neutral or streaming-friendly.

### `DerivativeFunc`

- **Represents:** derivative computation as a callable.
- **Created by:** `TrajectorySolver` lambdas capturing `DynamicsModel&`.
- **Owned by:** solver stack and integrator call frames.
- **Mutated by:** not mutated.
- **Consumed by:** integrators.
- **Domain-level or implementation-level:** implementation-level adapter.
- **Reusable across subsystems:** yes for any `State`-based autonomous ODE.
- **Independence:** independent from physical model; not independent from
  `State`.

### `post_step` Callback

- **Represents:** optional after-step state correction or projection.
- **Created by:** `make_schwarzschild_post_step`.
- **Owned by:** copied into solver call.
- **Mutated by:** callback mutates `State&`.
- **Consumed by:** `TrajectorySolver`.
- **Domain-level or implementation-level:** implementation-level hook carrying
  domain math.
- **Reusable across subsystems:** mechanism is reusable; current callback is
  Schwarzschild-specific.
- **Independence:** not independent from physics or coordinate layout.

## 5. Execution Model

### What Starts a Simulation?

Only caller code starts a simulation. In-repo, that caller is
`tests/null_geodesic_smoke.cpp`. It constructs `SimulationConfig`,
`SchwarzschildParameters`, and a scenario IC struct, then calls the matching
`run_simulation` overload.

There is no runtime configuration loader, command-line interface, registry, or
factory exposed to the executable.

### Where Is Computation Orchestrated?

`SimulationPipeline.cpp` orchestrates computation. Its public surface is four
overloads declared in `SimulationConfig.h`; its internal orchestration helpers
are anonymous-namespace functions:

- `require_spacetime`
- `make_schwarzschild_metric`
- `schwarzschild_metadata`
- `make_schwarzschild_post_step`
- `integrate_schwarzschild`

The architectural significance is that orchestration is centralized and private.
External callers can choose one of four IC overloads, but cannot alter the
metric construction, termination policy, projection logic, or default integrator
through `run_simulation`.

### Where Does Numerical Integration Happen?

`TrajectorySolver::solve` owns the loop and delegates each step to an
`Integrator`. In the public pipeline, the integrator is always
`Integration::default_integrator()`, which returns the file-local static
`RK4Integrator`.

The RK4 algorithm itself lives in the private `rk4_step` function in
`RK4Integrator.cpp`.

### Where Does Physical Modeling Happen?

Physical modeling is split across multiple places:

- `SchwarzschildMetric::christoffel` computes Christoffel symbols.
- `GeodesicDynamics::compute_derivative` applies the geodesic equation.
- `SchwarzschildInitialStateBuilders` compute initial velocities from scenario
  parameters and constraints.
- `make_schwarzschild_post_step` reprojects the null constraint after steps.
- `SchwarzschildObservables.h` computes diagnostic conserved quantities.

This means the physical model is not located in a single subsystem. The metric
and dynamics boundary is clean, but Schwarzschild-specific formulas also live in
pipeline, builders, and observables.

### Where Is State Stored?

During execution, state is stored in:

- `State current` in `TrajectorySolver`.
- temporary RK4 stage states `k1` through `k4`.
- `std::vector<State> history` when using `solve`.
- `SimulationResult::history` after the vector is returned.

There is no external storage, streaming result sink, trajectory file format, or
database.

### How Are Results Returned?

`run_simulation` returns `SimulationResult` by value. Its `history` vector is
assigned from the vector returned by `TrajectorySolver::solve`; the remaining
metadata fields are populated inside `integrate_schwarzschild`.

### How Are Results Consumed?

The only in-repo consumer is the smoke test. It reads:

- `result.history.front()`
- `result.history.back()`
- `State.X` and `State.U` indirectly through observables

No in-repo consumer reads `SimulationResult::metadata`,
`characteristic_radius`, `name`, or `spacetime`.

### Coupling to Algorithm, Backend, Hardware, and Configuration

- **Algorithm:** public pipeline is tightly coupled to RK4 because it always
  calls `default_integrator`. `TrajectorySolver` itself is not tightly coupled;
  direct callers can pass another `Integrator`.
- **Backend:** CPU-only C++/Eigen. No backend abstraction exists.
- **Hardware model:** single-threaded scalar CPU execution. No GPU, SIMD policy,
  device memory, or task system appears in the code.
- **Simulation configuration:** public execution is tightly coupled to a
  compile-time C++ aggregate. There is no runtime config format.

## 6. Extension Points

| Boundary / feature | Classification | Existing mechanism | What new functionality entails |
|---|---|---|---|
| New fixed-step integrator used by direct solver callers | Strong | Implement `Integration::Integrator` and pass it to `TrajectorySolver`. | No existing architecture must change if caller bypasses `run_simulation`. |
| New default public-pipeline integrator | Conditional | `default_integrator()` is hardwired in `SimulationPipeline.cpp`. | Existing pipeline code must change to select or accept the integrator. |
| New dynamics model used by direct solver callers | Strong | Implement `Dynamics::DynamicsModel`. | Can be supplied to `TrajectorySolver` directly. |
| New dynamics model through `run_simulation` | Weak | No public hook; pipeline constructs `GeodesicDynamics` internally. | Pipeline must change. |
| New metric consumed by `GeodesicDynamics` | Strong at metric/dynamics boundary | Implement `Spacetime::Metric`. | Works with `GeodesicDynamics` if same `State`/chart contract holds. |
| New metric exposed through public simulation API | Weak | `SimulationConfig` and `run_simulation` are Schwarzschild-only. | Requires enum/parameter/API/pipeline changes. |
| New termination condition for direct solver callers | Strong | Implement `TerminationPolicy`. | Can be supplied directly to `TrajectorySolver`. |
| New termination condition through `run_simulation` | Conditional | Pipeline constructs `HorizonTermination` only. | Pipeline and config must change to choose it. |
| History-free propagation | Conditional | `TrajectorySolver::propagate` exists. | Direct callers can use it; public pipeline does not expose it. |
| New scenario / initial condition | Weak | Four IC structs and four overloads. | Requires new struct, builder, scenario enum, and public overload or branch. |
| New analysis observable | Conditional | Add header-only function. | Easy to add formulas, but no analysis subsystem consumes them. |
| Coordinate transforms | Conditional | Free functions exist. | Usable by external callers; no current integration point. |
| SGL optics / source-lens-observer pipeline | Missing | Empty directories only. | No current extension point or data contract exists. |
| Visualization/rendering | Missing in implementation; residual in data | Metadata fields exist but no consumer. | No rendering boundary exists in SGL. |
| Runtime configuration | Missing | C++ aggregate initialization only. | No parser/schema/loader exists. |
| Execution backend selection | Missing | CPU/Eigen only. | No backend boundary exists. |

## 7. Architectural Contracts

### `State` Contract

- `X` and `U` are four-vectors.
- For Schwarzschild code, components are ordered `(t, r, theta, phi)`.
- `X[1]` is radius and must be meaningful anywhere termination, metrics,
  builders, observables, or projection are used.
- `U` is the derivative of `X` with respect to the integration parameter.
- `State` supports linear combinations; RK4 relies on `operator+` and scalar
  multiplication.

This is the most important implicit contract in the repository. It is not
encoded as a type.

### Metric Contract

- `christoffel(mu, alpha, beta, X)` returns coefficients for the same coordinate
  chart used by `State`.
- The method may be called 64 times per derivative evaluation.
- Symmetry is handled by the metric implementation, not by dynamics.
- `X` is borrowed and not mutated.

### Dynamics Contract

- `compute_derivative` returns a `State` whose `X` component is the input
  tangent and whose `U` component is acceleration.
- The derivative is autonomous: no explicit time or step argument.
- The dynamics object must outlive solver lambdas that capture it by reference.

### Integrator Contract

- `step` computes exactly one step of size `dt`.
- It receives the derivative as an opaque callable and must return a new
  `State`.
- It has no place to report local error, reject a step, or request a different
  step size.

### Termination Contract

- `should_terminate` is a pure predicate on current state only.
- It is checked before each step.
- It cannot observe history, step number, derivative, or future state.

### Pipeline Contract

- `SimulationConfig::spacetime` must be `Schwarzschild`.
- `SimulationConfig::scenario` must match the concrete IC overload chosen by the
  caller.
- The pipeline always builds `SchwarzschildMetric`,
  `GeodesicDynamics`, `HorizonTermination`, and the default integrator.
- Null projection is optional and encoded as a post-step mutation callback.

### Initial Condition Contract

- All built states must have `r0 > rs`.
- A zero `CustomInitialConditions::vt` means the builder should infer it from
  null/timelike constraints.
- `NullScatterInitialConditions::impact_parameter <= 0` means use
  `b_crit + impact_parameter_offset`.
- The IC structs are scenario-specific and are interpreted by the pipeline, not
  by a generic problem-definition layer.

### Result Contract

- `history` is a full in-memory sequence of `State`.
- The first element is the initial state.
- Stored states include post-step projection if projection is enabled.
- Metadata fields are populated as if downstream consumers may need metric and
  chart identity, but current SGL code does not consume them.

### Units Contract

- Implementation assumes geometrized units with `G = c = 1`.
- `SchwarzschildParameters::mass` is used as `rs`, the Schwarzschild radius.
- `PhysicalConstants.h` provides SI anchors, but no conversion code connects
  them to simulation execution.

## 8. Framework Identity

Based strictly on implementation, SGL is currently:

> A standalone Schwarzschild geodesic propagation library with a small
> simulation convenience API and a smoke-test executable.

It is not currently:

- an SGL-specific simulator, because there is no source/lens/observer/image
  formation model and no optics code;
- a gravitational-lensing framework, because there is no lensing problem
  abstraction beyond null geodesic propagation;
- a full scientific computing framework, because there is no experiment layer,
  data I/O layer, runtime configuration, backend abstraction, analysis pipeline,
  or visualization implementation;
- a general GR framework in practice, because the only metric exposed through
  public execution is Schwarzschild, even though the `Metric` abstraction can
  support more.

The closest identity supported by the architecture is **a GR propagation
foundation extracted for future SGL work**. The evidence is the strong
metric/dynamics/integrator/termination seams, the direct Schwarzschild pipeline,
the absence of SGL-specific subsystems, and the few forward-looking but unused
elements (`propagate`, `RadiusBoundTermination`, `solar_radius_m`).

## 9. Architectural Tensions

### General Interfaces vs Single Concrete World

`Metric`, `DynamicsModel`, `Integrator`, and `TerminationPolicy` are general
interfaces. The public pipeline, however, always constructs one concrete metric,
one concrete dynamics model, one default integrator, and one termination policy.
The generality is real at the lower boundary and absent at the public entry
point.

### Physics Abstraction vs Pipeline Convenience

Schwarzschild geometry is abstracted behind `Metric`, but Schwarzschild formulas
also appear in the initial builders, observables, metadata construction, and
post-step projection. The architecture separates Christoffel lookup from
dynamics, but not the broader Schwarzschild physical model from orchestration.

### Reusable State vs Typed Scientific Data

`State` is reusable because every subsystem accepts it. It is also weakly typed:
coordinate meaning, tangent meaning, chart identity, and units are all implicit.
The same structure serves as physical state, numerical vector, and stored
trajectory element.

### Simulation Result as Current Output vs Future Consumer Contract

`SimulationResult::history` is the only output field currently consumed. Other
fields are populated but unused. This creates tension between the actual current
output model and a retained future/downstream contract inherited from Penrose.

### Orchestration Simplicity vs Extensibility

The pipeline is simple because one translation unit wires one supported
scenario family. The same design makes extension through the public API weak:
new scenarios, metrics, policies, or integrators require editing the central
pipeline.

### Full History vs Propagation Use Case

`solve` returns a full trajectory history and is the only method used by
`run_simulation`. `propagate` exists for history-free use, and its comment names
optics/image formation, but the public API does not expose it. The codebase
therefore contains both an experiment/visualization-shaped trajectory model and
a ray-propagation-shaped final-state model, with only the former wired.

### Domain Units vs SI Anchors

Execution assumes geometrized units. SI constants exist but are unused. The
presence of `solar_radius_m` indicates solar-scale intent, while the runtime
model remains unitless and Schwarzschild-radius based.

## 10. Architectural Invariants

The following concepts are stable in the implementation as it exists now.

### `State` as the Universal Propagation Object

Every subsystem accepts or produces `State`. It is the core invariant of the
architecture.

### Schwarzschild Spherical Coordinates

All meaningful execution assumes `(t, r, theta, phi)` and indexes directly into
those positions. `CoordinateChartKind::SchwarzschildSpherical` is the only chart
identity represented.

### Christoffel-Based Physical Modeling

The physical model enters dynamics through `Metric::christoffel`. This is the
stable physics/dynamics interface.

### Autonomous State Derivative

`DynamicsModel::compute_derivative(const State&)` has no explicit time,
parameter, environment, backend, or context. The solver assumes this shape.

### Fixed-Step State Integration

The integrator contract is fixed-step and state-to-state. There is no adaptive
control model or error-estimate result.

### Policy-Based Termination

Termination is a stateless predicate on current state. That contract is stable
across both existing policies and both solver functions.

### In-Memory Trajectory as Primary Result

The public pipeline returns `SimulationResult` containing
`std::vector<State> history`. This is the wired result invariant, even though
`propagate` offers an unwired final-state alternative.

### Caller-Owned Configuration

Configuration is a C++ aggregate created by caller code and borrowed by const
reference. There is no repository-supported external configuration lifecycle.

### CPU/Eigen Execution

The only backend model is CPU execution with Eigen fixed-size vectors and
matrices.

## 11. Unresolved Questions

- `UNVERIFIED` - Whether unused result metadata is intended as a future SGL
  contract or is simply residual Penrose data.
- `UNVERIFIED` - Whether `TrajectorySolver::propagate` is intended to become
  the primary optics/ray API or remain a utility for external callers.
- `UNVERIFIED` - Whether `RadiusBoundTermination` is intended for SGL image-plane
  or outer-boundary propagation.
- `UNVERIFIED` - Whether `CoordinateChart` is intended to mediate future
  Cartesian optics inputs/outputs or is only leftover reusable Penrose utility.
- `UNVERIFIED` - Whether `SchwarzschildParameters::mass` should be understood
  long-term as physical mass or as Schwarzschild radius `rs`; current execution
  uses it as `rs`.
- `UNVERIFIED` - Whether `SimulationConfig::Scenario` is meant to remain the
  problem-definition mechanism or will be replaced by an optics/lensing problem
  model.
- `UNVERIFIED` - Whether the four abstract interfaces are intended as public
  extension points for external users or only internal seams inherited from
  Penrose.
- `UNVERIFIED` - Whether SGL intends to support multiple metrics again. The
  `Metric` abstraction remains, but public API and enum vocabulary are
  Schwarzschild-only.
- `UNVERIFIED` - Whether future analysis/visualization will consume
  `SimulationResult` directly or use a new data model.
- `UNVERIFIED` - Whether unit conversion will become an executable part of the
  architecture. `Units.h` and `PhysicalConstants.h` do not currently participate
  in execution.

# Architecture Reconstruction Notes

**Internal factual reconstruction. Phase 1 — repository reconnaissance only.**
Not an architectural review. No recommendations. Implementation treated as source of truth.

- Repository: `/home/h-livv/Projects/sgl`, remote `git@github.com:h-livv/sgl.git`, branch `main`
- HEAD: `2254836` "Update README"; extraction commit `65bf01c` "Extraction of the physics from Penrose, SGL directory structure"
- Git history contains **two commits only**. Penrose history was not preserved, so provenance was reconstructed from code, not from VCS ancestry.
- Verification method: read all 27 C++ translation units and headers end-to-end; traced every `#include`; enumerated linker symbols from `libsgl_physics.a`; performed a clean out-of-tree configure/build/run; diffed every SGL source file against its Penrose counterpart in the still-present `/home/h-livv/Projects/penrose` tree; cross-checked with a graphify knowledge graph (180 nodes / 312 edges, `graphify-out/`).
- Build verified working: `cmake -B <dir> -S .` + `cmake --build` compiles 8 library TUs and 1 executable with zero warnings surfaced; `sgl_null_smoke` exits 0 with `steps=50001 |dE/E|=1.01e-14 |dL/L|=3.73e-15`.

---

## Repository Map

36 tracked files. Grouped by what the implementation actually does, not by directory name.

### Compiled library sources (8 translation units → `libsgl_physics.a`)

Listed explicitly in `CMakeLists.txt:16-25`. This list is the authoritative definition of what is code in this repository:

| File | Role in implementation |
|---|---|
| `physics/metrics/CoordinateChart.cpp` | Cartesian↔spherical Jacobians and state conversion |
| `physics/metrics/SchwarzschildMetric.cpp` | Christoffel symbol lookup table |
| `physics/geodesics/GeodesicDynamics.cpp` | Geodesic equation right-hand side |
| `physics/integrators/RK4Integrator.cpp` | RK4 step + the single global integrator instance |
| `physics/simulation/TrajectorySolver.cpp` | The integration loops (`solve`, `propagate`) |
| `physics/simulation/TerminationPolicy.cpp` | Two stopping predicates |
| `physics/simulation/SimulationPipeline.cpp` | The only orchestrator; 4 `run_simulation` overloads |
| `physics/simulation/initial_conditions/SchwarzschildInitialStateBuilders.cpp` | 4 analytic initial-state constructors |

### Headers that carry actual logic (not just declarations)

- `physics/validation/observables/SchwarzschildObservables.h` — 6 `inline` functions, entire "validation subsystem". **Header-only; no `.cpp`; not in `SGL_PHYSICS_SOURCES`.** Contributes zero symbols to the archive.
- `physics/core/PhysicalConstants.h` — `constexpr` SI constants + one `inline` function.
- `physics/core/GeodesicState.h` — `struct State` with constructors and arithmetic operators; the value type everything else moves around.

### Pure interface / vocabulary headers (no logic)

`physics/core/Metric.h`, `physics/core/MetricKind.h`, `physics/core/SchwarzschildParameters.h`, `physics/geodesics/DynamicsModel.h`, `physics/integrators/Integrator.h`, `physics/simulation/TerminationPolicy.h`, `physics/simulation/initial_conditions/InitialConditions.h`, `physics/simulation/initial_conditions/SchwarzschildInitialStateBuilders.h`.

### Configuration-as-code header

`physics/simulation/SimulationConfig.h` — holds `SimulationConfig`, `SolverOptions`, `SimulationMetadata`, `SimulationResult`, the `Scenario`/`GeodesicKind` enums, **and** the four `run_simulation` declarations. There is no `SimulationPipeline.h`; the config header doubles as the pipeline's public API header.

### Documentation-only files

- `physics/core/Units.h` — 4 lines, entirely comments, **zero declarations**. Not included by any file in the repository.
- `README.md`, `notes/EXTRACTION.md`.

### Build system

- `CMakeLists.txt` — 39 lines. One `find_package(Eigen3 CONFIG REQUIRED)`, one static library, one optional executable behind `option(SGL_BUILD_SMOKE_TEST … ON)`. Conditional vcpkg toolchain pickup if `./vcpkg/scripts/buildsystems/vcpkg.cmake` exists (that directory does not exist here).
- `vcpkg.json` — single dependency: `eigen3`.
- **No CI configuration, no linter/formatter config, no test framework, no scripts, no generated or derived source.** The only non-source config file in the tree is `vcpkg.json`.

### Executables

Exactly one: `sgl_null_smoke` from `tests/null_geodesic_smoke.cpp`. It is a hand-written `main()` returning `0`/`1`/`2`; no framework, no test registration, not wired to `ctest` (`enable_testing()` is never called).

### Empty directories

`optics/`, `experiments/`, `analysis/`, `visualization/` each contain only `.gitkeep`. `docs/` exists on disk but is **empty and untracked** (no `.gitkeep`, absent from `git ls-files`). None of the five are referenced by `CMakeLists.txt`. They are namespace reservations, not components.

### Stale build directory (gitignored)

`build/` contains artifacts from a **different source tree**: `CMakeCache.txt` records `CMAKE_HOME_DIRECTORY:INTERNAL=/home/h-livv/Projects/penrose/SGL` and `SGL_BINARY_DIR:STATIC=/home/h-livv/Projects/penrose/SGL/build`. Reconfiguring in place fails with a CMake source-mismatch error. This is the hard evidence that the repository was physically copied out of `penrose/SGL/`.

---

## Major Subsystems

Seven subsystems. Directory layout and subsystem boundaries mostly coincide, with two exceptions noted below.

### 1. Geodesic State (the universal currency)

- **Purpose:** the single data structure passed between every other subsystem.
- **Location:** `physics/core/GeodesicState.h`.
- **Responsibilities:** hold position `X` and tangent `U` as `Vector4d`; provide `operator+`, `operator*` (both orders) so the integrator can do arithmetic on states generically.
- **Inputs/Outputs:** value type; no I/O.
- **Dependencies:** Eigen only.
- **Consumers:** every other subsystem. Highest-degree node in the graph (degree 27).
- **Notable:** `struct State` is declared **in the global namespace**, and the header does `using namespace Eigen;` at global scope (line 4). Every consumer of any SGL header therefore transitively imports all of `Eigen` into the global namespace. This is the one type in the codebase that is not namespaced — `Spacetime`, `Dynamics`, `Integration`, `Simulation`, `CoordinateChart`, `Physics::Observables`, `Constants` all are.

### 2. Spacetime Geometry

- **Purpose:** supply Christoffel symbols at a coordinate point.
- **Location:** `physics/core/Metric.h` (interface), `physics/metrics/SchwarzschildMetric.{h,cpp}` (sole implementation), `physics/core/SchwarzschildParameters.h` (parameters), `physics/core/MetricKind.h` (identity enums).
- **Responsibilities:** `christoffel(mu, alpha, beta, X)` returns one connection coefficient per call. Symmetry is handled inside by swapping `alpha`/`beta`; unmatched index triples return `0.0`.
- **Inputs:** `rs` at construction; `Eigen::Vector4d X` per call.
- **Outputs:** one `double`.
- **Dependencies:** Eigen. **Nothing else.** This is a leaf.
- **Consumers:** `GeodesicDynamics` (via the `Metric&` reference), `SimulationPipeline` (constructs the concrete type).
- **Encoded assumptions:** the chart is `(t, r, θ, φ)`; `X[1]` is radius and `X[2]` is polar angle. `1e-8` guards appear in the `Γ^φ_{θφ}` denominator. Nothing in the interface communicates the chart contract — `MetricKind.h` records it only as a comment on the enumerator.

### 3. Dynamics

- **Purpose:** turn geometry into a first-order ODE right-hand side.
- **Location:** `physics/geodesics/DynamicsModel.h` (interface), `physics/geodesics/GeodesicDynamics.{h,cpp}`.
- **Responsibilities:** evaluate `a^μ = -Γ^μ_{αβ} U^α U^β` over the full 4×4×4 loop and return `State(U, a)`.
- **Dependencies:** `State`, `Metric`. Holds `const Spacetime::Metric& metric_` — non-owning.
- **Consumers:** `TrajectorySolver` (as `const DynamicsModel&`), `SimulationPipeline` (constructs it).
- **Encoded assumptions:** derivative parameterisation is affine-parameter-based; the 64-iteration loop with a `Gamma != 0.0` guard assumes `christoffel` is cheap.

### 4. Numerical Integration

- **Purpose:** advance a `State` by `dt`.
- **Location:** `physics/integrators/Integrator.h` (interface + `DerivativeFunc` typedef + `default_integrator()` declaration), `physics/integrators/RK4Integrator.{h,cpp}`.
- **Responsibilities:** classic 4-stage RK4 in `rk4_step` (file-local), exposed three ways: the virtual `RK4Integrator::step`, the free function `stepRK4`, and the `default_integrator()` accessor.
- **Dependencies:** `State`, `<functional>`. **No dependency on `Metric`, `DynamicsModel`, or anything physical.** This is the cleanest boundary in the repository: the integrator only knows `State` arithmetic and a `std::function`.
- **Consumers:** `TrajectorySolver`.
- **Encoded assumptions:** fixed step size; `State` supports `+` and scalar `*`; no error estimate, no adaptivity, no step rejection — so no interface surface exists for a controller.

### 5. Trajectory Solving

- **Purpose:** run the integration loop with a stopping rule and an optional per-step mutation hook.
- **Location:** `physics/simulation/TrajectorySolver.{h,cpp}`, `physics/simulation/TerminationPolicy.{h,cpp}`.
- **Responsibilities:** two static functions. `solve` accumulates and returns the full `std::vector<State>`; `propagate` keeps only the current state and reports `steps_taken` through an `int&` out-parameter.
- **Inputs:** initial `State`, `DynamicsModel&`, `TerminationPolicy&`, `dt`, `max_steps`, `Integrator&` (defaulted to `default_integrator()`), `std::function<void(State&, int)> post_step` (defaulted `nullptr`).
- **Outputs:** `std::vector<State>` (by value) or a single `State`.
- **Dependencies:** all four abstract interfaces. This is where they converge.
- **Consumers:** only `SimulationPipeline`, and only `solve`.
- **Encoded assumptions:** `TerminationPolicy` is checked **before** the step, so a trajectory always contains at least the initial state and terminates at the first state satisfying the predicate rather than the last state outside it. `history.reserve(std::min(max_steps, 100000))` treats 100 000 as a memory cap, not a hint — the smoke test's 50 001 entries exceed the reserve by one and force a reallocation.
- **Note:** `TerminationPolicy` lives in `simulation/` but depends only on `State`. Functionally it is a numerical-methods leaf, not orchestration; the directory placement does not reflect its dependency profile.

### 6. Simulation Orchestration

- **Purpose:** the only place where configuration, geometry, initial conditions, dynamics, integration and termination are wired together.
- **Location:** `physics/simulation/SimulationPipeline.cpp` + declarations in `physics/simulation/SimulationConfig.h`.
- **Responsibilities:** validate the config (`require_spacetime`), construct the metric (`make_schwarzschild_metric`), build metadata (`schwarzschild_metadata`), optionally build the null-constraint projection callback (`make_schwarzschild_post_step`), call the solver and populate `SimulationResult` (`integrate_schwarzschild`).
- **Inputs:** `SimulationConfig`, `SchwarzschildParameters`, one of four IC structs.
- **Outputs:** `SimulationResult`.
- **Dependencies:** every other subsystem except `CoordinateChart` and `Observables`. Highest fan-out module.
- **Consumers:** `tests/null_geodesic_smoke.cpp` — the only caller in the repository.
- **Encoded assumptions:** every helper except the four public overloads is inside an anonymous namespace, so the entire orchestration strategy is private to the TU. There is no seam to substitute a different pipeline.

### 7. Initial Conditions

- **Purpose:** convert scenario parameters into a physically consistent 4-velocity.
- **Location:** `physics/simulation/initial_conditions/InitialConditions.h` (4 POD structs), `SchwarzschildInitialStateBuilders.{h,cpp}` (4 builders).
- **Responsibilities:** solve the normalisation constraint for `U[0]`. Timelike builders use `(1 + spatial)/f`; the null branch of `build_custom` uses `spatial/f`. `build_null_scatter` derives `L = b·E` with `E = 1`, computes `b_crit = (3√3/2)·rs` inline, and throws when the discriminant is negative.
- **Dependencies:** `State`, `SchwarzschildParameters`, and — only for `build_custom` — `SimulationConfig`.
- **Consumers:** `SimulationPipeline` only.
- **Notable coupling:** the header forward-declares `struct SimulationConfig;` (line 10) and the `.cpp` includes `../SimulationConfig.h`. `build_custom` reads `config.geodesic` to choose the null vs timelike normalisation. This is the **one place where a low-level builder reaches back up into the configuration object**, and it is the only cycle-shaped relationship in the repository (`SimulationConfig.h` → `InitialConditions.h`, and `SchwarzschildInitialStateBuilders.cpp` → `SimulationConfig.h`; broken at header level by the forward declaration, real at TU level).

### Not a subsystem: `physics/metrics/CoordinateChart`

Compiled into the archive and exports four symbols, but **nothing in the repository includes `CoordinateChart.h`**. It has no consumer, no caller, and no participation in any execution path. Functionally it is an exported utility, not a subsystem.

### Not a subsystem: `physics/validation/`

Contains one header of `inline` functions. It has no build participation, no `.cpp`, and its only consumer is the smoke test. "Validation" as an activity does not exist in the repository; the directory holds physics formulas used for after-the-fact checking.

---

## Public Interfaces

### Abstract base classes (4)

All four follow the identical shape: `virtual ~X() = default;` plus exactly one pure virtual method. None has data members. None uses `final` except `RK4Integrator`.

| Interface | Method | Owner | Implementers | Consumers | Assumptions encoded |
|---|---|---|---|---|---|
| `Spacetime::Metric` | `christoffel(int,int,int,const Vector4d&) const` | `physics/core/` | `SchwarzschildMetric` | `GeodesicDynamics` | One coefficient per call (64 virtual calls per derivative evaluation); coordinates are 4-vectors; caller knows the chart |
| `Dynamics::DynamicsModel` | `compute_derivative(const State&) const` | `physics/geodesics/` | `GeodesicDynamics` | `TrajectorySolver` | Derivative is itself a `State`; autonomous system (no explicit parameter/time argument) |
| `Integration::Integrator` | `step(const State&, double, const DerivativeFunc&) const` | `physics/integrators/` | `RK4Integrator` | `TrajectorySolver` | Fixed step, no error output, stateless/`const` so one instance is shareable |
| `Simulation::TerminationPolicy` | `should_terminate(const State&) const` | `physics/simulation/` | `HorizonTermination`, `RadiusBoundTermination` | `TrajectorySolver` | Stateless predicate on a single state; cannot see step index, history, or elapsed parameter |

Graph cross-check: these four form a hyperedge (`abstract_interface_seam_set`) whose only convergence point is `TrajectorySolver::solve`.

### Function-object boundaries (2)

- `Integration::DerivativeFunc = std::function<State(const State&)>` (`Integrator.h:9`). `TrajectorySolver` wraps `DynamicsModel` into this at both call sites. The integrator therefore never sees the `DynamicsModel` type at all — this is the deliberate erasure point between numerics and physics.
- `std::function<void(State&, int)> post_step`. A **mutating** callback: it receives `State&` and the step index. Used by exactly one producer (`make_schwarzschild_post_step`) to rescale `U[0]` from the null constraint every `null_projection_interval` steps.

### Domain / vocabulary objects

- `State` — global namespace, value semantics, 2×`Vector4d`.
- `Spacetime::SchwarzschildParameters` — single field `double mass = 1.0`. The comment above it says "Schwarzschild radius rs in geometrized units (G = c = 1)", and it is passed directly to `SchwarzschildMetric(double rs)` and used as `rs` in every builder. **The field name and its semantic content disagree; the usage is internally self-consistent as `rs`.**
- `Spacetime::MetricKind`, `Spacetime::CoordinateChartKind` — single-enumerator enums. Written only into `SimulationMetadata`; never read, never branched on.
- `Simulation::Scenario`, `Simulation::GeodesicKind` — `Scenario` is only used by `require_spacetime` to confirm it matches the IC type the caller already chose via overload resolution. `GeodesicKind` is read only by `build_custom`.

### Configuration objects

`Simulation::SimulationConfig` (+ nested `SolverOptions`). Plain aggregate, all fields defaulted, populated by direct member assignment in C++. **There is no configuration parsing anywhere in the repository** — no file format, no CLI, no environment. Configuration flow is compile-time source code.

### Result objects

`Simulation::SimulationResult` — `history`, `characteristic_radius`, `name`, `spacetime`, `metadata` (itself `SimulationMetadata` with `metric`, `coordinate_chart`, `horizon_radius`, `photon_sphere_radius`).

Read/write audit of every field:

| Field | Written | Read anywhere in SGL |
|---|---|---|
| `history` | `integrate_schwarzschild` | **yes** — smoke test |
| `characteristic_radius` | `integrate_schwarzschild:79` | **no** |
| `name` | `integrate_schwarzschild:80` | **no** |
| `spacetime` | `integrate_schwarzschild:81` | **no** |
| `metadata.*` | `schwarzschild_metadata` | **no** |

Four of five fields are write-only within this repository. See Penrose section for what read them previously.

### Library-level public surface

27 `T`-linkage symbols in `libsgl_physics.a`. Everything reachable via `run_simulation` plus five symbol groups with no in-repo caller: `CoordinateChart::*` (4), `TrajectorySolver::propagate`, `RadiusBoundTermination::*` (2), `Integration::stepRK4`, `SchwarzschildMetric::get_rs` (inline, header-only). Header-only and also uncalled: `Physics::Observables::null_hamiltonian_error`, `Constants::schwarzschild_radius`, `Constants::solar_mass`, `Constants::solar_radius_m`.

---

## Dependency Graph

### Include-level dependencies (all edges, verified from every `#include` in the tree)

```text
Eigen/Dense
   ↑            ↑
core/GeodesicState.h   core/Metric.h ──────────► (Eigen only)
   ↑    ↑    ↑    ↑         ↑          ↑
   │    │    │    │         │          │
   │    │    │    │  metrics/SchwarzschildMetric.h
   │    │    │    │         ↑
   │    │    │  geodesics/DynamicsModel.h
   │    │    │         ↑
   │    │    │  geodesics/GeodesicDynamics.h ──────┐
   │    │    │                                     │
   │    │  integrators/Integrator.h                │
   │    │         ↑                                │
   │    │  integrators/RK4Integrator.h ────┐       │
   │    │                                  │       │
   │  simulation/TerminationPolicy.h ──┐   │       │
   │                                   │   │       │
   │                     simulation/TrajectorySolver.h
   │                                   ▲
core/SchwarzschildParameters.h         │
core/MetricKind.h                      │
   ↑                                   │
simulation/SimulationConfig.h ◄────────┼─── (fwd-decl'd by, and included by, IC builders)
   ↑         ↑                         │
   │   simulation/initial_conditions/InitialConditions.h
   │         ↑                         │
   │   .../SchwarzschildInitialStateBuilders.h
   │         ↑                         │
   └─── simulation/SimulationPipeline.cpp ─┘  (also → SchwarzschildMetric.h, GeodesicDynamics.h)

validation/observables/SchwarzschildObservables.h → core/{GeodesicState,SchwarzschildParameters}.h   [leaf, no in-library consumer]
metrics/CoordinateChart.h → core/GeodesicState.h, Eigen                                             [leaf, no consumer at all]
core/Units.h                                                                                        [isolated: includes nothing, included by nothing]
core/PhysicalConstants.h                                                                            [isolated: included by nothing]

tests/null_geodesic_smoke.cpp → simulation/SimulationConfig.h, simulation/initial_conditions/InitialConditions.h, validation/observables/SchwarzschildObservables.h
```

### Centrality

- **Most depended-upon:** `core/GeodesicState.h` — included directly by 7 headers and transitively by all. Graph degree 27, more than double the next node.
- **Highest fan-out:** `SimulationPipeline.cpp` — 6 first-party includes, reaches every subsystem except `CoordinateChart` and `Observables`.
- **True leaves (depend on nothing first-party):** `core/Metric.h`, `core/GeodesicState.h`, `core/MetricKind.h`, `core/SchwarzschildParameters.h`, `core/Units.h`, `core/PhysicalConstants.h`, `InitialConditions.h`.
- **Structurally isolated:** `core/Units.h` and `core/PhysicalConstants.h` participate in zero include edges in either direction. `CoordinateChart` participates only downward.

### Directionality and cycles

Dependencies are **one-directional at header level with a single exception**: `SchwarzschildInitialStateBuilders.h` forward-declares `Simulation::SimulationConfig` (which is defined in `SimulationConfig.h`, which includes `InitialConditions.h`, which is the sibling of the builders header). The forward declaration keeps the header graph acyclic; the `.cpp` closes the loop with `#include "../SimulationConfig.h"`. **No other cycle exists.** No `#ifdef` guards for circularity, no PIMPL, no dependency-inversion shims anywhere.

### Coupling concentration, per the axes requested

| Axis | Actual state |
|---|---|
| physics ↔ numerical methods | **Decoupled, and the decoupling is real.** `Integrator.h` and `RK4Integrator.cpp` contain no reference to `Metric`, `Christoffel`, `Schwarzschild`, or `rs`. Coupling is only via `State` arithmetic and `DerivativeFunc`. |
| physics ↔ simulation orchestration | **Tightly coupled, in one direction.** `SimulationPipeline.cpp` names `SchwarzschildMetric`, computes `1.5 * metric.mass` for the photon sphere, and inlines the Schwarzschild null-constraint algebra (`f = 1 - rs/r`, the `spatial/f` projection) directly in a lambda. Physics does not know about orchestration; orchestration knows the physics intimately. |
| simulation ↔ data | **Fused.** `SimulationConfig.h` defines both the inputs and the output struct and declares the entry points. There is no separate data-representation module. |
| simulation ↔ rendering | **Nonexistent.** No rendering code, no rendering dependency, no rendering-shaped interface — except the write-only `SimulationMetadata` fields, whose content (horizon radius, photon-sphere radius) is presentation-oriented. |
| rendering ↔ physics | **Absent.** |
| configuration ↔ implementation | **Fused.** Config is a C++ aggregate in the same header as the API it configures. |
| execution ↔ computation | **Weakly separated.** Exactly one executable; it constructs config, calls `run_simulation`, and asserts on conserved quantities in the same `main()`. |

---

## Data Flow

Traced from the only entry point that exists: `tests/null_geodesic_smoke.cpp:main`.

```text
[1] main() constructs Simulation::SimulationConfig on the stack, sets 7 fields by assignment
    (spacetime, scenario, geodesic, dt, max_steps, horizon_safety_factor, solver.null_constraint_projection, name)
        │  value
[2] main() constructs Spacetime::SchwarzschildParameters{.mass = 1.0}
    and Simulation::NullScatterInitialConditions, setting r0 = 30.0 and
    impact_parameter = Physics::Observables::critical_impact_parameter(1.0) + 0.5
        │  ← note: the test reaches into the observables module to seed an initial condition,
        │     duplicating b_crit logic that build_null_scatter also computes internally
        │  const&, const&, const&
[3] Simulation::run_simulation(config, metric, initial)   [SimulationPipeline.cpp:104]
        │
        ├─► require_spacetime(config, Scenario::NullScatter)
        │       throws std::runtime_error if config.spacetime != Schwarzschild
        │       throws std::runtime_error if config.scenario != NullScatter
        │
        ├─► make_schwarzschild_metric(metric)
        │       returns std::unique_ptr<Spacetime::Metric> holding a SchwarzschildMetric(params.mass)
        │       ← the abstract type is erased here; the concrete type is never named again
        │
        └─► InitialStateBuilders::build_null_scatter(metric, initial)
                computes f, b_crit, b, E=1, L=b·E, vt=E/f, vph=L/(r0² sinθ), vr=-√(E²-f·L²/r0²)
                throws if f ≤ 0 or if the radial discriminant < 0
                returns State by value
        │
        │  unique_ptr moved by value; State by const&
[4] integrate_schwarzschild(config, std::move(metric_impl), metric, initial)   [line 68]
        │
        ├─ constructs Dynamics::GeodesicDynamics dynamics(*metric_impl)  ← stores const Metric&
        ├─ constructs HorizonTermination policy(metric_params.mass, config.horizon_safety_factor)
        │      ← passes rs directly as the horizon radius; the header's default safety_factor=1.001
        │        is bypassed because SimulationConfig supplies its own (1.0 by default, 1.0001 in the test)
        ├─ builds post_step = make_schwarzschild_post_step(config, metric_params.mass)
        │      returns nullptr unless solver.null_constraint_projection is set;
        │      otherwise a std::function capturing rs and interval BY VALUE
        │
        └─► TrajectorySolver::solve(initial, dynamics, policy, dt, max_steps, default_integrator(), post_step)
        │
[5]     inside solve():
            history.reserve(min(max_steps, 100000)); history.push_back(initial)
            derivative = lambda capturing &dynamics BY REFERENCE
            loop i in [0, max_steps):
                 if policy.should_terminate(current) break          ← checked BEFORE stepping
                 current = integrator.step(current, dt, derivative)  ← State returned by value
                     └─ rk4_step calls derivative() 4×
                           └─ dynamics.compute_derivative(state)
                                 └─ 64× metric_.christoffel(mu, alpha, beta, state.X)
                 if (post_step) post_step(current, i)                ← MUTATES current in place
                 history.push_back(current)                          ← copy appended
            returns std::vector<State> by value
        │
[6]     result.history = <moved-from return value>
        result.characteristic_radius = metric_params.mass
        result.name = config.name
        result.spacetime = config.spacetime
        result.metadata = schwarzschild_metadata(metric_params)   ← horizon_radius = rs, photon_sphere = 1.5·rs
        returns SimulationResult by value
        │
[7] main() binds `const SimulationResult result`, checks history.size() >= 2,
    takes const State& to history.front() and history.back(),
    calls conserved_energy / conserved_angular_momentum on each,
    prints to std::cout, returns 0 / 1 / 2
```

### Transition properties

| Transition | Object | Created by | Owned by | Copied or mutated | Receiver depends on impl details? |
|---|---|---|---|---|---|
| 1→3 | `SimulationConfig` | caller (`main`) | caller's stack | passed `const&`, never copied, never mutated | Yes — pipeline reads `.spacetime`, `.scenario`, `.solver.*` fields directly; no accessors |
| 2→3 | IC struct | caller | caller's stack | `const&` | Yes — builder reads every field by name |
| 3→4 | `unique_ptr<Metric>` | `make_schwarzschild_metric` | transferred by value into `integrate_schwarzschild` | moved | No — only the abstract type is used downstream |
| 3→4 | `State` (initial) | builder, by value | `integrate_schwarzschild` parameter (`const&`) | copied once into `history` | No |
| 4→5 | `DynamicsModel&`, `TerminationPolicy&`, `Integrator&` | `integrate_schwarzschild` stack / global | **not transferred** — borrowed references | neither | No — pure interface use |
| 5 internal | `State current` | `solve` | `solve`'s stack | **mutated in place** by `post_step`, reassigned each step, copied into `history` | `post_step` depends deeply: it indexes `state.X[1]`, `state.X[2]`, `state.U[0..3]` and reimplements Schwarzschild `f` |
| 5→6 | `vector<State>` | `solve` | moved into `SimulationResult` | moved, not copied | No |
| 6→7 | `SimulationResult` | `integrate_schwarzschild` | returned by value to caller | moved | Yes — `main` reads `.history` and indexes `State` members |

Two facts worth recording precisely:

1. **The only mutation-in-place in the entire data flow** is `post_step(current, i)`. Everything else is value semantics with copies or moves.
2. **The null-constraint projection is silently order-dependent.** `post_step` runs *after* `integrator.step` and *before* `history.push_back`, so stored history always reflects the projected state, and step `i` in the callback is the loop index, not a count of projections.

---

## Ownership Model

| Resource | Owner | Mechanism | Lifetime assumption |
|---|---|---|---|
| Concrete metric object | `integrate_schwarzschild`'s local `std::unique_ptr<Spacetime::Metric>` parameter | unique ownership, moved in from the caller | Destroyed at end of `integrate_schwarzschild`. `GeodesicDynamics::metric_` is a `const Metric&` into this object — **the reference is valid only because both live in the same stack frame**. Nothing in the type system enforces it; `GeodesicDynamics` outliving the `unique_ptr` would dangle. |
| `Dynamics::GeodesicDynamics` | `integrate_schwarzschild` stack | value | Passed to `solve` as `const&`; `solve`'s `derivative` lambda captures it **by reference** (`[&dynamics]`) and the lambda does not escape the function. Safe by construction, unenforced. |
| `Simulation::HorizonTermination` | `integrate_schwarzschild` stack | value | Passed as `const&`; not retained. |
| Default integrator | **Global** — `const RK4Integrator kDefaultIntegrator{}` at namespace scope inside an anonymous namespace in `RK4Integrator.cpp:6` | static storage duration | The single piece of global state in the repository. It is `const` and stateless, so it is thread-safe by construction and has no initialization-order dependency (no dynamic initialization needed). `default_integrator()` returns `const Integrator&` to it. Also used directly by `stepRK4`. |
| Trajectory (`std::vector<State>`) | created inside `solve`, ownership transferred to `SimulationResult::history` | move | Sole owner at any time. No aliasing, no shared views, no spans handed out. |
| `SimulationResult` | returned by value; ultimately the caller's `const SimulationResult result` in `main` | value | `main` holds `const State&` into `result.history` (`front()`/`back()`) — valid because `result` outlives them in the same scope. |
| `post_step` callback | `std::function` owned by `integrate_schwarzschild`'s local, passed **by value** into `solve` | value | Captures `rs` and `interval` **by copy** (`[rs, interval]`), so it holds no references and cannot dangle. |
| `SimulationConfig`, `SchwarzschildParameters`, IC structs | caller (`main`) | stack values | Borrowed as `const&` through the whole call chain; never stored. `build_custom` borrows `SimulationConfig` too. |
| Metric parameters inside `SchwarzschildMetric` | the object itself | `double rs_` by value | No aliasing. |

Summary of the ownership regime: **single-owner, stack-scoped, value-semantic throughout.** One `unique_ptr`, zero `shared_ptr`, zero raw owning pointers, zero manual `new`/`delete`, one immutable global, one by-value callback, and four non-owning `const&` interface parameters whose validity rests on same-frame lifetime rather than on any enforced contract.

---

## Execution Flow

There is exactly one execution path in the repository.

```text
$ ./sgl_null_smoke
  main()                                   tests/null_geodesic_smoke.cpp:13
   ├─ populate SimulationConfig            (dt=0.001, max_steps=50000, safety=1.0001, projection=on)
   ├─ SchwarzschildParameters{.mass=1.0}
   ├─ NullScatterInitialConditions{r0=30, b = critical_impact_parameter(1.0) + 0.5}
   ├─ run_simulation(...)                  → 50 001 states, ~50 000 × 4 × 64 christoffel evaluations
   ├─ guard: history.size() >= 2           → exit 1 on failure
   ├─ conserved_energy / conserved_angular_momentum at front() and back()
   ├─ print steps, |dE/E|, |dL/L|
   └─ guard: both relative drifts < 1e-3   → exit 2 on failure; else print "OK", exit 0
```

Observed on a clean build: `steps=50001 |dE/E|=1.0103e-14 |dL/L|=3.72693e-15`, exit 0.

Facts about the execution surface:

- No argument parsing, no input files, no output files, no logging framework. Output is two `std::cout` lines and error text on `std::cerr`.
- No `enable_testing()` / `add_test()`; the executable is not registered with CTest.
- Termination is by `max_steps` exhaustion here, not by `HorizonTermination` — with `b > b_crit` the ray scatters and never reaches `r ≤ rs·1.0001`.
- The library builds and links independently of the executable (`-DSGL_BUILD_SMOKE_TEST=OFF`), and produces no `main`, so `libsgl_physics.a` is consumable but currently has no consumer besides the smoke test.
- Error handling strategy is uniform: `throw std::runtime_error` from `require_spacetime` and all four builders. Nothing in the repository catches these; an invalid configuration terminates the process via unhandled exception.

---

## Architectural Boundaries

| Boundary | Strength | Evidence |
|---|---|---|
| **Numerical integration ↔ physics** | **Strong / explicit** | `Integrator` + `DerivativeFunc` type-erase the physics entirely. `RK4Integrator.cpp` mentions no physical concept. Substituting an integrator requires no physics change and vice versa. |
| **Geometry ↔ dynamics** | **Strong / explicit** | `GeodesicDynamics` holds `const Metric&` and calls one virtual method. Any `Metric` subclass works without touching dynamics. |
| **Dynamics ↔ solver** | **Strong / explicit** | `DynamicsModel` abstract interface; solver takes it by `const&`. |
| **Termination ↔ solver** | **Strong / explicit** | `TerminationPolicy` abstract interface. |
| **Physics ↔ simulation orchestration** | **Weak / one-sided** | `SimulationPipeline.cpp` inlines Schwarzschild algebra (`1 - rs/r`, `1.5 * mass`, the null projection formula) inside anonymous-namespace lambdas. The physics does not depend on orchestration, but orchestration is not substitutable and duplicates formulas that also live in `SchwarzschildObservables.h` and the IC builders. |
| **Configuration ↔ implementation** | **Absent as a boundary** | `SimulationConfig`, `SimulationResult`, and the `run_simulation` declarations occupy one header. No serialization, no schema, no validation layer separate from the pipeline. |
| **Data representation ↔ everything** | **Absent as a boundary** | `State` is a bare aggregate in the global namespace with `using namespace Eigen`. Every subsystem indexes `X[1]`, `U[0]` etc. by raw integer with the chart contract carried only in comments. |
| **Simulation ↔ analysis** | **Implicit** | `SchwarzschildObservables.h` reads `SimulationResult`-derived `State`s but is not depended on by the library. The dependency is created by the *consumer* (the test), which is the only thing that joins the two. |
| **Simulation ↔ visualization** | **Absent (no code) but vestigially present in data** | No visualization code exists, yet `SimulationMetadata`/`SimulationResult` carry `horizon_radius`, `photon_sphere_radius`, `characteristic_radius`, `name`, `spacetime` — all write-only. The data contract for a rendering consumer survived; the consumer did not. |
| **Execution ↔ computation** | **Weak** | One `main()` that both configures and asserts. No driver layer, no experiment abstraction. |
| **Infrastructure ↔ domain** | **Minimal by absence** | One static library, one optional executable, one external dependency (Eigen). No logging, no I/O, no threading, no allocation strategy, no error-reporting infrastructure. |
| **Physics ↔ units** | **Implicit and undocumented in code** | Geometrized units are asserted in a comment in `Units.h`, a file included by nothing. `PhysicalConstants.h` provides SI constants and `schwarzschild_radius(mass)`; nothing converts between the two conventions, and nothing calls them. `SchwarzschildParameters::mass` actually holds `rs`. |
| **`optics` / `experiments` / `analysis` / `visualization`** | **Absent** — directories only | `.gitkeep` only; not referenced by the build. |

---

## Penrose-Derived Components

Provenance is established by three independent lines of evidence: (a) the stale `build/CMakeCache.txt` naming `/home/h-livv/Projects/penrose/SGL`; (b) the extraction commit message and `notes/EXTRACTION.md`; (c) direct file-level diffs against the Penrose tree still present at `/home/h-livv/Projects/penrose`.

**Caveat on (c):** the local Penrose tree is a live working copy. Comparisons below reflect its current state, which may have advanced past the revision SGL was cut from. Penrose file mtimes (2026-07-17 … 2026-08-09 23:51) all precede the SGL extraction (2026-08-10 00:10–00:16), which is consistent with an unchanged base, but this is **UNVERIFIED** as an exact-revision claim.

### Byte-identical from Penrose (2 files)

`physics/core/GeodesicState.h` (was `shared/state/GeodesicState.h`) and `physics/integrators/RK4Integrator.cpp` — zero-line diff. The central data type and the integration kernel were taken verbatim.

### Mechanically transformed only (include-path rewrite + reformat + comment removal)

`Metric.h`, `MetricKind.h`, `PhysicalConstants.h`, `Units.h`, `SchwarzschildParameters.h`, `DynamicsModel.h`, `GeodesicDynamics.{h,cpp}`, `Integrator.h`, `RK4Integrator.h`, `CoordinateChart.{h,cpp}`, `SchwarzschildMetric.{h,cpp}`, `TrajectorySolver.cpp` (signature realignment only), `InitialConditions.h`, `SchwarzschildInitialStateBuilders.h`.

The systematic edits were: `<state/…>`, `<spacetime/…>`, `<metrics/…>`, `<constants/…>`, `<units/…>` → `<core/…>`; Penrose's 4-space-indented-inside-namespace style → non-indented namespace bodies with `} // namespace X` closers. **No logic changed in any of these.**

### Genuinely generalized during extraction (physics broadened from equatorial to 3D)

This is the substantive scientific change, and it is consistent across three files:

- `SchwarzschildObservables.h`: `conserved_angular_momentum` gained the `sin²θ` factor (Penrose: `r*r*U[3]`); `null_hamiltonian` gained both the `vth` term and the `sin²θ` factor.
- `SimulationPipeline.cpp` `make_schwarzschild_post_step`: the null projection gained `r²vθ²` and `sin²θ` terms (Penrose: `(vr²/f + r²vφ²)/f`).
- `SchwarzschildInitialStateBuilders.cpp` `build_null_scatter`: a block of exploratory reasoning comments was resolved and removed, leaving the θ-independent `inside_vr = E² − f·L²/r0²`.

Penrose's versions assume motion in the equatorial plane. SGL's do not. This is the clearest evidence of adaptation rather than copying, and it is directly relevant to off-axis ray propagation.

### Additions that do not exist in Penrose

- `TrajectorySolver::propagate` — history-free integration with an `int& steps_taken` out-parameter. Its comment states the intent explicitly: *"Integrate without retaining the full path (optics / image-formation use case)."* This is the single piece of SGL-forward-looking API in the repository. **It has no caller.**
- `Simulation::RadiusBoundTermination` — `r ≤ r_min || r ≥ r_max`. No caller. Shaped for a ray that must be stopped at an outer boundary rather than at a horizon.
- `Constants::solar_radius_m = 6.957e8` — added to `PhysicalConstants.h`. No caller. The only solar-scale-specific value in the codebase.
- `tests/null_geodesic_smoke.cpp` — a heavy reduction of Penrose's `physics/validation/null_geodesic.cpp` (268 → 54 lines). Retains the config-population pattern and the conserved-quantity check; drops CSV export, `benchmark_io.h`, per-step tabulation, and the Kerr comparison. Switched from `Scenario::Custom` to `Scenario::NullScatter`.

### Removed during extraction

| Removed | Was in Penrose | Consequence for SGL |
|---|---|---|
| All Kerr code | `KerrMetric.{h,cpp}`, `KerrParameters.h`, `KerrInitialStateBuilders.{h,cpp}`, `KerrObservables.h`, 4 Kerr `run_simulation` overloads, `integrate_kerr`, `make_kerr_post_step`, `kerr_metadata` | `MetricKind` collapsed to one enumerator; `Metric` now has exactly one implementation, so the abstraction has no second instance to validate it |
| `SimulationRequest.h` | `std::variant`-based `MetricParameters`/`InitialConditions`, `SimulationRequest`, `run_simulation(request)`, `run_all(span)`, `make_schwarzschild_request`, `make_kerr_request` | The batch/type-erased request API is gone. SGL has only the four directly-overloaded entry points. This is the "multi-metric request API reduced to Schwarzschild" noted in `EXTRACTION.md`. |
| `using PhysicsTrajectory = SimulationResult;` | `SimulationConfig.h` | The alias that named the physics→storage handoff |
| `shared/observer/Observer.h` | `struct Observer { Vector4d position; Vector3d velocity; }` | **Not extracted**, despite `optics/` naming "observer" as intended future work |
| Penrose validation harness | `BenchmarkRunner.{h,cpp}`, `freefall.*`, `orbital.*`, `null_geodesic.*`, `export/benchmark_io.h`, `physics/analysis/*.py` (14 Python modules) | The consumers of `SchwarzschildObservables.h` and of the observables SGL deleted (`analytical_freefall_time`, `timelike_norm`) |
| Physics→visualization adapter | `run/adapter/SimulationTrajectoryAdapter.h` (`store_trajectories`, `prepare_scene_from_results` over `span<const SimulationResult>`) | **This is what read `SimulationResult::metadata`, `characteristic_radius`, `name`, and `spacetime`.** Their write-only status in SGL is the direct footprint of this removal. |
| `realtime/` (44 src files), `visualization/` (77 src files), `run/viewer`, `run/export`, glad/glfw/glm vendoring | | No rendering subsystem, and only one external dependency remains |
| Penrose's layer-boundary comments | `SimulationConfig.h` carried `// Layer 1 — physics settings / orchestration only`, `// Physics → Trajectory Storage boundary`, `// Metric-derived metadata carried to consumers without visualization coupling`, `// Backend-independent metric identity vocabulary`, `// CPU-side integrator extension point` | The structs survived; the prose that declared them boundaries did not. The architectural intent is now recoverable only from the Penrose tree. |
| `penrose_shared` INTERFACE target | Penrose: separate `penrose_shared` (INTERFACE, `shared/`) + `penrose_physics` (STATIC, `physics/`) with `penrose_physics PUBLIC penrose_shared` | SGL has one `sgl_physics` STATIC target with **two** PUBLIC include roots (`physics/` and `physics/core/`). The shared/physics target split — an explicit build-level boundary in Penrose — became a single target with a mixed include convention (`<core/X.h>` for core, `"../subsystem/X.h"` for siblings). |

### Duplication inherited and resolved

Penrose had `SchwarzschildParameters` in two places: the real definition in `shared/metrics/` and a compatibility shim at `physics/metrics/parameters/SchwarzschildParameters.h` (`// Compatibility include — SchwarzschildParameters lives in shared vocabulary`). SGL kept one copy at `physics/core/` and dropped the shim.

### Does SGL now have independent architectural boundaries?

Recorded as observations, not judgements:

- **Independently buildable and runnable: yes, verified.** No Penrose path, target, or dependency remains in the build. Only Eigen is external.
- **The four abstract seams are intact and self-consistent** (`Metric`, `DynamicsModel`, `Integrator`, `TerminationPolicy`) and none references anything Penrose-specific.
- **Three abstractions are now single-implementation** (`Metric`, `DynamicsModel`, `Integrator`), so their generality is asserted rather than exercised. `TerminationPolicy` is the only one with two implementers, and the second has no caller.
- **The `SimulationResult` contract still encodes a consumer SGL does not have.** Four of its five fields, plus all four `SimulationMetadata` fields, plus the two identity enums, exist to be read by something that was left behind in Penrose.
- **The SGL-specific direction is visible only in three uncalled additions** (`propagate`, `RadiusBoundTermination`, `solar_radius_m`) and in the θ-generalization of the observables. No optics, source, lens, observer, image-plane, or experiment-driver abstraction exists in any form.
- **Naming has been rebased but not the vocabulary.** Namespaces are still `Spacetime`, `Dynamics`, `Integration`, `Simulation`, `Physics::Observables`, `Constants` — Penrose's names, none SGL-specific. `MetricKind`'s deleted comment in Penrose listed `SolarGravitationalLens` as a future enumerator; SGL removed that comment without adding the enumerator.

---

## Uncertainties

Explicitly marked. Not filled in with assumptions.

- `UNVERIFIED` — Whether `/home/h-livv/Projects/penrose` is at the exact revision SGL was extracted from. All Penrose file mtimes precede the extraction commit, which is consistent, but the tree is a live working copy and its git state was not examined. Every diff-based claim above inherits this caveat.
- `UNVERIFIED` — Whether the write-only `SimulationResult` / `SimulationMetadata` fields were retained deliberately (as a forward contract for future SGL consumers) or incidentally (because the struct was copied whole). The Penrose adapter proves what *used* to read them; it does not establish intent for SGL.
- `UNVERIFIED` — Whether `CoordinateChart` is intended for future optics use (Cartesian ray setup → spherical integration is a plausible use, and `sph_to_cart` would suit output transformation) or is simply residual. No comment, caller, or note states either. `notes/EXTRACTION.md` lists it under "Included (reusable GR foundation)" without a rationale.
- `UNVERIFIED` — Whether `SchwarzschildParameters::mass` holding `rs` rather than `M` is an intentional convention or a naming drift carried from Penrose. The comment asserts `rs`; the field name says `mass`; all uses are consistent with `rs`. Penrose had the same conflict with the comment inline (`double mass = 1.0; // Schwarzschild radius rs in code units where G=c=1`), so it predates SGL. Which reading is normative for future SGL code is not established anywhere.
- `UNVERIFIED` — Whether `physics/core/Units.h` is meant to become a conversion layer. Penrose's version said "placeholder for a future unit-conversion system"; SGL rewrote it to a statement of the current convention, which reads as a narrowing of scope, but nothing confirms that.
- `UNVERIFIED` — Whether `RadiusBoundTermination` and `TrajectorySolver::propagate` were written for SGL or backported from an unexamined Penrose branch. They are absent from the local Penrose tree, which suggests SGL-authored, but only one Penrose working copy was inspected.
- `UNVERIFIED` — Whether the smoke test's tolerance of `1e-3` on relative drift, and `dt = 0.001` / `max_steps = 50000`, were chosen from a convergence study or set empirically. Observed drift (~1e-14) is 11 orders of magnitude inside the tolerance; no study is recorded.
- `UNVERIFIED` — Whether `history.reserve(std::min(max_steps, 100000))` intends 100 000 as a memory cap or as a heuristic. Inherited verbatim from Penrose.
- `UNVERIFIED` — Whether `graphify`'s 30 dangling-endpoint edges indicate any missed relationship. They were inspected and all resolve to external/templated types (`Vector4d`, `Matrix4d`, `unique_ptr`, `std::function`, `std::string`, `std::vector`) that the AST extractor could not bind to a first-party node. No first-party edge appears to be lost, but the extractor's coverage was not exhaustively audited.
- **Not applicable rather than uncertain:** thread-safety, concurrency model, GPU/backend selection, serialization format, plugin/registry mechanism, logging, and dependency injection. Grep-verified: no such code exists in the repository, so there is nothing to reconstruct.

---

## Cross-reference: knowledge graph

`graphify-out/` holds the persisted graph used to cross-check the manual trace. 180 nodes, 312 edges, 12 communities. Highest-degree nodes: `State` (27), `SimulationConfig` (14), `SimulationResult` (13), `run_simulation()` (13), `integrate_schwarzschild()` (12), `TrajectorySolver::solve` (12). The community partition recovered the seven subsystems above plus `CoordinateChart` as its own high-cohesion island (0.60) with no bridge into the simulation path — independent confirmation that it is unwired.

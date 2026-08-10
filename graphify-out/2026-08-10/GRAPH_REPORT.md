# Graph Report - sgl  (2026-08-10)

## Corpus Check
- 35 files · ~38,287 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 538 nodes · 641 edges · 47 communities (46 shown, 1 thin omitted)
- Extraction: 100% EXTRACTED · 0% INFERRED · 0% AMBIGUOUS
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `2254836f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- SimulationConfig
- solve
- BoundOrbitInitialConditions
- State
- Architecture Reconstruction Notes
- 2. Stable Scientific Concepts
- SimulationPipeline.cpp
- SGL Architecture Reconstruction
- CustomInitialConditions
- 4. Abstraction Failure Analysis
- 1. Future Capability Analysis
- 14. Current → Target Architectural Delta
- Repository Map
- 7. Architectural Contracts
- 3. Architectural Boundaries
- Detailed entries for the highest-risk capabilities
- 4. Data Model
- 11. Architectural Principles
- 2. Scientific Evolution
- SGL — scientific computing foundation
- 3. Dependency Model
- SGL Future Requirements Map
- Extraction inventory
- 1. Long-Term Scientific Identity
- 3. Numerical Evolution
- 4. Computational Scaling
- 5. Validation Requirements
- 7. Physics Expansion
- 8. Execution Requirements
- 9. Visualization Requirements
- 10. Reproducibility Requirements
- PUBLIC Include Directory Convention
- SGL Target Architecture
- 8. Target Numerical Architecture
- NullScatterInitialConditions
- 16. Target Architecture Verdict
- 11. Target Validation Architecture
- 13. Minimality Analysis
- 7. Target Physics Architecture
- 10. Target Observation Architecture
- 12. Target Visualization Architecture
- 1. Architectural Objective
- 4. Target Dependency Graph
- 6. Target Extension Model

## God Nodes (most connected - your core abstractions)
1. `State` - 27 edges
2. `1. Future Capability Analysis` - 21 edges
3. `SGL Target Architecture` - 18 edges
4. `SGL Future Requirements Map` - 16 edges
5. `2. Stable Scientific Concepts` - 16 edges
6. `SimulationConfig` - 14 edges
7. `14. Current → Target Architectural Delta` - 14 edges
8. `SimulationResult` - 13 edges
9. `Architecture Reconstruction Notes` - 12 edges
10. `4. Abstraction Failure Analysis` - 12 edges

## Surprising Connections (you probably didn't know these)
- `sgl_null_smoke Test Executable Target` --references--> `main()`  [EXTRACTED]
  CMakeLists.txt → tests/null_geodesic_smoke.cpp
- `sgl_physics Static Library Target` --references--> `eigen3`  [EXTRACTED]
  CMakeLists.txt → vcpkg.json
- `compute_derivative` --references--> `State`  [EXTRACTED]
  physics/geodesics/GeodesicDynamics.h → physics/core/GeodesicState.h
- `rk4_step()` --references--> `State`  [EXTRACTED]
  physics/integrators/RK4Integrator.cpp → physics/core/GeodesicState.h
- `step` --references--> `State`  [EXTRACTED]
  physics/integrators/RK4Integrator.h → physics/core/GeodesicState.h

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Four Abstract Interface Seams of the Solver** — physics_core_metric_metric, physics_geodesics_dynamicsmodel_dynamicsmodel, physics_integrators_integrator_integrator, physics_simulation_terminationpolicy_terminationpolicy [INFERRED 0.95]
- **Schwarzschild Initial Condition to Trajectory Flow** — physics_simulation_simulationconfig_simulationconfig, physics_simulation_initial_conditions_initialconditions_nullscatterinitialconditions, physics_simulation_initial_conditions_schwarzschildinitialstatebuilders_build_null_scatter, physics_simulation_simulationpipeline_integrate_schwarzschild, physics_simulation_simulationconfig_simulationresult [EXTRACTED 1.00]
- **Components Not Reachable From run_simulation** — physics_metrics_coordinatechart_cart_to_sphere, physics_metrics_coordinatechart_sph_to_cart, physics_simulation_terminationpolicy_radiusboundtermination, physics_core_physicalconstants_schwarzschild_radius, physics_simulation_trajectorysolver_trajectorysolver_propagate [INFERRED 0.95]

## Communities (47 total, 1 thin omitted)

### Community 0 - "SimulationConfig"
Cohesion: 0.07
Nodes (32): CoordinateChartKind, GeodesicKind, MetricKind, SimulationConfig, Scenario, vector, SimulationConfig, dt (+24 more)

### Community 1 - "solve"
Cohesion: 0.09
Nodes (23): DerivativeFunc, DynamicsModel, compute_derivative, Integrator, step, rk4_step(), RK4Integrator, step (+15 more)

### Community 2 - "BoundOrbitInitialConditions"
Cohesion: 0.25
Nodes (8): BoundOrbitInitialConditions, phi0, r0, t0, theta0, vphi, vr, vtheta

### Community 3 - "State"
Cohesion: 0.10
Nodes (25): Matrix4d, Vector4d, operator*(), State, U, X, cart_to_sph_Jacobian(), cart_to_sphere() (+17 more)

### Community 4 - "Architecture Reconstruction Notes"
Cohesion: 0.05
Nodes (38): 1. Geodesic State (the universal currency), 2. Spacetime Geometry, 3. Dynamics, 4. Numerical Integration, 5. Trajectory Solving, 6. Simulation Orchestration, 7. Initial Conditions, Abstract base classes (4) (+30 more)

### Community 5 - "2. Stable Scientific Concepts"
Cohesion: 0.12
Nodes (16): 2.10 Result and Provenance, 2.11 Observation, 2.12 Instrument, 2.13 Experiment, 2.14 Validation Case, 2.15 Concepts deliberately rejected, 2.1 Ray, 2.2 Chart (+8 more)

### Community 6 - "SimulationPipeline.cpp"
Cohesion: 0.08
Nodes (27): sgl_null_smoke Test Executable Target, sgl_physics Static Library Target, Metric, christoffel, SchwarzschildParameters, mass, GeodesicDynamics, compute_derivative (+19 more)

### Community 7 - "SGL Architecture Reconstruction"
Cohesion: 0.04
Nodes (48): 10. Architectural Invariants, 11. Unresolved Questions, 1. Conceptual Architecture, 2. Subsystem Model, 5. Execution Model, 6. Extension Points, 8. Framework Identity, 9. Architectural Tensions (+40 more)

### Community 8 - "CustomInitialConditions"
Cohesion: 0.22
Nodes (9): CustomInitialConditions, phi0, r0, t0, theta0, vphi, vr, vt (+1 more)

### Community 9 - "4. Abstraction Failure Analysis"
Cohesion: 0.04
Nodes (44): 2. Modification Hotspots, 3. Hidden Assumption Analysis, 4. Abstraction Failure Analysis, 5. Future Dependency Risks, 6. Predicted Refactoring, 7. Adversarial Assessment, 8. Final Stress-Test Verdict, Affected Subsystems (+36 more)

### Community 11 - "1. Future Capability Analysis"
Cohesion: 0.10
Nodes (21): 10. Finite Solar-Disk Effects, 11. Parameter Sweeps, 12. CPU/GPU Execution, 13. HPC Execution, 14. Distributed Experiments, 15. Large-Scale Trajectory Datasets, 16. Multiple Analysis Methods, 17. Scientific Validation Pipelines (+13 more)

### Community 12 - "14. Current → Target Architectural Delta"
Cohesion: 0.14
Nodes (14): 14. Current → Target Architectural Delta, D10 — Autonomous derivative, D11 — Validation is a single executable, D12 — No experiment layer, D1 — Missing problem layer, D2 — Configuration as a central dependency, D3 — Result is trajectory-shaped and consumer-shaped, D4 — Step contract carries no diagnostics (+6 more)

### Community 13 - "Repository Map"
Cohesion: 0.20
Nodes (10): Build system, Compiled library sources (8 translation units → `libsgl_physics.a`), Configuration-as-code header, Documentation-only files, Empty directories, Executables, Headers that carry actual logic (not just declarations), Pure interface / vocabulary headers (no logic) (+2 more)

### Community 14 - "7. Architectural Contracts"
Cohesion: 0.20
Nodes (10): 7. Architectural Contracts, Dynamics Contract, Initial Condition Contract, Integrator Contract, Metric Contract, Pipeline Contract, Result Contract, `State` Contract (+2 more)

### Community 15 - "3. Architectural Boundaries"
Cohesion: 0.20
Nodes (10): 3. Architectural Boundaries, B1 — Domain Data (shared kernel), B2 — Physics / Domain Model, B3 — Numerical Methods, B4 — Composition (thin seam, not a subsystem), B5 — Execution, B6 — Observation and Instrument, B7 — Analysis and Validation (+2 more)

### Community 16 - "Detailed entries for the highest-risk capabilities"
Cohesion: 0.20
Nodes (10): 12. Future Capability Matrix, C11 — Observer / Spacecraft Dynamics, C14 — Adaptive Integration, C18 / C19 — Batch, Vectorized, and GPU Execution, C21 — Large Trajectory Datasets, C26 — Wave Optics / Diffraction, C5 / C6 — Ray Ensembles and Bundles, C7 — Image Formation (+2 more)

### Community 17 - "4. Data Model"
Cohesion: 0.22
Nodes (9): 4. Data Model, `DerivativeFunc`, Initial Condition Structs, `post_step` Callback, `SchwarzschildParameters`, `SimulationConfig`, `SimulationMetadata`, `SimulationResult` (+1 more)

### Community 18 - "11. Architectural Principles"
Cohesion: 0.22
Nodes (9): 11. Architectural Principles, P1 — Additive growth, P2 — Scientific description independent of execution strategy, P3 — Physical model separable from numerical method, P4 — Results independent of consumers, P5 — Multiple result kinds, P6 — Validation separable from production, P7 — Reference implementations remain authoritative (+1 more)

### Community 19 - "2. Scientific Evolution"
Cohesion: 0.22
Nodes (9): 2. Scientific Evolution, Stage 1 — Relativistic Propagation, Stage 2 — Solar Gravitational Lens Geometry, Stage 3 — Ray Ensembles, Stage 4 — Image Formation, Stage 5 — Optical / Instrument Modeling, Stage 6 — Extended Sources, Stage 7 — Observer / Spacecraft Dynamics (+1 more)

### Community 20 - "SGL — scientific computing foundation"
Cohesion: 0.22
Nodes (8): Build, Dependencies, Layout, Next steps, Penrose → SGL mapping, SGL — scientific computing foundation, What this is, What this is not

### Community 21 - "3. Dependency Model"
Cohesion: 0.25
Nodes (8): 3. Dependency Model, Central Modules, Coupling Hotspots, Cycles, Inappropriate or Mismatched Dependencies, Stable Boundaries, Subsystem Dependency Graph, Unstable Boundaries

### Community 22 - "SGL Future Requirements Map"
Cohesion: 0.25
Nodes (7): 13. Architectural Questions That Must Remain Open, 6. Experimentation Requirements, Current support: Absent, Document Status, Requirements, SGL Future Requirements Map, Support vocabulary used throughout

### Community 23 - "Extraction inventory"
Cohesion: 0.40
Nodes (4): Excluded, Extraction inventory, Included (reusable GR foundation), Restructuring vs Penrose

### Community 24 - "1. Long-Term Scientific Identity"
Cohesion: 0.50
Nodes (4): 1. Long-Term Scientific Identity, Identity gap to be evaluated later, What SGL must eventually be, What this identity requires architecturally

### Community 25 - "3. Numerical Evolution"
Cohesion: 0.50
Nodes (4): 3. Numerical Evolution, Current support, Requirements, The central numerical assumption at risk

### Community 26 - "4. Computational Scaling"
Cohesion: 0.50
Nodes (4): 4. Computational Scaling, Current support: Contradicted at the representation level, Requirements, The governing principle

### Community 27 - "5. Validation Requirements"
Cohesion: 0.50
Nodes (4): 5. Validation Requirements, Current support: Partial, and weaker than it appears, Requirements, The required workflow

### Community 28 - "7. Physics Expansion"
Cohesion: 0.50
Nodes (4): 7. Physics Expansion, Current support, Requirements, The major long-term question

### Community 29 - "8. Execution Requirements"
Cohesion: 0.50
Nodes (4): 8. Execution Requirements, Evaluation against the target layering — where SGL sits today, Requirements, The target separation

### Community 30 - "9. Visualization Requirements"
Cohesion: 0.50
Nodes (4): 9. Visualization Requirements, Current support: Absent as capability, and already violated in the data model, Requirements, The governing constraint

### Community 31 - "10. Reproducibility Requirements"
Cohesion: 0.67
Nodes (3): 10. Reproducibility Requirements, Current support: split — strong foundation, absent mechanism, Requirements

### Community 35 - "SGL Target Architecture"
Cohesion: 0.22
Nodes (8): 15. Architectural Invariants, 5. Target Data Flow, 9. Target Execution Architecture, Consequences for the data model, Document Status, SGL Target Architecture, The minimum abstraction, Which structures are stable architectural contracts

### Community 36 - "8. Target Numerical Architecture"
Cohesion: 0.29
Nodes (7): 8. Target Numerical Architecture, Precision and state representation, Repair 1 — the step contract must carry diagnostics, Repair 2 — the solver must not own progression policy, The composition, What is deliberately not generalized, What this makes possible without further change

### Community 37 - "NullScatterInitialConditions"
Cohesion: 0.29
Nodes (7): NullScatterInitialConditions, impact_parameter, impact_parameter_offset, phi0, r0, t0, theta0

### Community 38 - "16. Target Architecture Verdict"
Cohesion: 0.33
Nodes (6): 16. Target Architecture Verdict, Assessment, Qualification 1 — the benefit is front-loaded and the cost is too, Qualification 2 — wave optics is not resolved, only kept reachable, Qualification 3 — the target is a reshaping, not a replacement, The quantitative shape

### Community 39 - "11. Target Validation Architecture"
Cohesion: 0.40
Nodes (5): 11. Target Validation Architecture, Direction of dependency, The distinction that must be structural, Tolerance as data, What the architecture must provide

### Community 40 - "13. Minimality Analysis"
Cohesion: 0.40
Nodes (5): 13. Minimality Analysis, Minimality summary, Necessary, Premature, Useful

### Community 41 - "7. Target Physics Architecture"
Cohesion: 0.40
Nodes (5): 7. Target Physics Architecture, Geometric optics and wave optics, The proposed structure, What is deliberately not generalized, Why the field model and propagation law must be separate

### Community 42 - "10. Target Observation Architecture"
Cohesion: 0.50
Nodes (4): 10. Target Observation Architecture, The assumption being retired, Three distinct concepts, Why observation is a consumer, never a driver

### Community 43 - "12. Target Visualization Architecture"
Cohesion: 0.50
Nodes (4): 12. Target Visualization Architecture, Does the current architecture satisfy this?, Required direction, Target boundary

### Community 44 - "1. Architectural Objective"
Cohesion: 0.67
Nodes (3): 1. Architectural Objective, The single organizing rule, What the objective explicitly does not include

### Community 45 - "4. Target Dependency Graph"
Cohesion: 0.67
Nodes (3): 4. Target Dependency Graph, The one near-cycle, and how it is cut, Why each dependency exists

### Community 46 - "6. Target Extension Model"
Cohesion: 0.67
Nodes (3): 6. Target Extension Model, Reading the table, The genuine exception

## Knowledge Gaps
- **363 isolated node(s):** `X`, `U`, `christoffel`, `mass`, `compute_derivative` (+358 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `State` connect `State` to `SimulationConfig`, `solve`, `SimulationPipeline.cpp`?**
  _High betweenness centrality (0.029) - this node is a cross-community bridge._
- **Why does `SGL Target Architecture` connect `SGL Target Architecture` to `8. Target Numerical Architecture`, `2. Stable Scientific Concepts`, `16. Target Architecture Verdict`, `11. Target Validation Architecture`, `13. Minimality Analysis`, `7. Target Physics Architecture`, `10. Target Observation Architecture`, `12. Target Visualization Architecture`, `14. Current → Target Architectural Delta`, `1. Architectural Objective`, `4. Target Dependency Graph`, `3. Architectural Boundaries`, `6. Target Extension Model`?**
  _High betweenness centrality (0.027) - this node is a cross-community bridge._
- **Why does `SGL Architecture Reconstruction` connect `SGL Architecture Reconstruction` to `4. Data Model`, `3. Dependency Model`, `7. Architectural Contracts`?**
  _High betweenness centrality (0.017) - this node is a cross-community bridge._
- **What connects `X`, `U`, `christoffel` to the rest of the system?**
  _363 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `SimulationConfig` be split into smaller, more focused modules?**
  _Cohesion score 0.06756756756756757 - nodes in this community are weakly interconnected._
- **Should `solve` be split into smaller, more focused modules?**
  _Cohesion score 0.08739495798319327 - nodes in this community are weakly interconnected._
- **Should `State` be split into smaller, more focused modules?**
  _Cohesion score 0.09659090909090909 - nodes in this community are weakly interconnected._
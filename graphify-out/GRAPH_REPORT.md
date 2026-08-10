# Graph Report - sgl  (2026-08-10)

## Corpus Check
- 36 files · ~56,602 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 706 nodes · 808 edges · 75 communities (74 shown, 1 thin omitted)
- Extraction: 100% EXTRACTED · 0% INFERRED · 0% AMBIGUOUS
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `2254836f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- SimulationConfig.h
- State
- BoundOrbitInitialConditions
- SchwarzschildParameters
- Architecture Reconstruction Notes
- 2. Stable Scientific Concepts
- Metric
- SGL Architecture Reconstruction
- CustomInitialConditions
- 4. Abstraction Failure Analysis
- 1. Future Capability Analysis
- 14. Current → Target Architectural Delta
- 8.1 Physics assumptions
- 7. Architectural Contracts
- 3. Architectural Boundaries
- Detailed entries for the highest-risk capabilities
- SimulationConfig
- 11. Architectural Principles
- 2. Scientific Evolution
- SGL — scientific computing foundation
- 9. Architectural Weaknesses
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
- SGL Architectural Review
- 4. Target Dependency Graph
- 6. Target Extension Model
- 13.1 Genuinely missing
- 4. Subsystem Responsibilities
- Executive Summary
- SimulationPipeline.cpp
- 10. Architectural Invariants
- 2. Subsystem Model
- 18. Overall Assessment
- 7. Architectural Strengths
- 14. Penrose Extraction Analysis
- 17. Future Refactoring Risks
- 6. Predicted Refactoring
- 7. Adversarial Assessment
- 2. Architectural Overview
- 5. Future Dependency Risks
- InitialConditions.h
- 16. Architectural Maturity
- 1. Project Context
- 3. Dependency Graph
- 3. Hidden Assumption Analysis
- 11. Target Architecture
- 15. Framework Identity
- 19. Final Verdict
- 5. Scientific Data Flow
- How to read this document
- vcpkg.json
- SGL Architectural Stress Test
- 12. Current → Target Architectural Gap
- 9. Target Execution Architecture

## God Nodes (most connected - your core abstractions)
1. `State` - 27 edges
2. `SGL Architectural Review` - 26 edges
3. `1. Future Capability Analysis` - 21 edges
4. `SGL Target Architecture` - 18 edges
5. `SGL Future Requirements Map` - 16 edges
6. `2. Stable Scientific Concepts` - 16 edges
7. `SimulationConfig` - 14 edges
8. `14. Current → Target Architectural Delta` - 14 edges
9. `SimulationResult` - 13 edges
10. `Architecture Reconstruction Notes` - 12 edges

## Surprising Connections (you probably didn't know these)
- `sgl_physics Static Library Target` --references--> `eigen3`  [EXTRACTED]
  CMakeLists.txt → vcpkg.json
- `sgl_null_smoke Test Executable Target` --references--> `main()`  [EXTRACTED]
  CMakeLists.txt → tests/null_geodesic_smoke.cpp
- `compute_derivative` --references--> `State`  [EXTRACTED]
  physics/geodesics/GeodesicDynamics.h → physics/core/GeodesicState.h
- `build_bound_orbit()` --references--> `State`  [EXTRACTED]
  physics/simulation/initial_conditions/SchwarzschildInitialStateBuilders.cpp → physics/core/GeodesicState.h
- `build_custom()` --references--> `State`  [EXTRACTED]
  physics/simulation/initial_conditions/SchwarzschildInitialStateBuilders.cpp → physics/core/GeodesicState.h

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Four Abstract Interface Seams of the Solver** — physics_core_metric_metric, physics_geodesics_dynamicsmodel_dynamicsmodel, physics_integrators_integrator_integrator, physics_simulation_terminationpolicy_terminationpolicy [INFERRED 0.95]
- **Schwarzschild Initial Condition to Trajectory Flow** — physics_simulation_simulationconfig_simulationconfig, physics_simulation_initial_conditions_initialconditions_nullscatterinitialconditions, physics_simulation_initial_conditions_schwarzschildinitialstatebuilders_build_null_scatter, physics_simulation_simulationpipeline_integrate_schwarzschild, physics_simulation_simulationconfig_simulationresult [EXTRACTED 1.00]
- **Components Not Reachable From run_simulation** — physics_metrics_coordinatechart_cart_to_sphere, physics_metrics_coordinatechart_sph_to_cart, physics_simulation_terminationpolicy_radiusboundtermination, physics_core_physicalconstants_schwarzschild_radius, physics_simulation_trajectorysolver_trajectorysolver_propagate [INFERRED 0.95]

## Communities (75 total, 1 thin omitted)

### Community 0 - "SimulationConfig.h"
Cohesion: 0.15
Nodes (15): CoordinateChartKind, MetricKind, SimulationMetadata, coordinate_chart, horizon_radius, metric, photon_sphere_radius, SimulationResult (+7 more)

### Community 1 - "State"
Cohesion: 0.06
Nodes (42): sgl_null_smoke Test Executable Target, sgl_physics Static Library Target, DerivativeFunc, Matrix4d, Vector4d, operator*(), State, U (+34 more)

### Community 2 - "BoundOrbitInitialConditions"
Cohesion: 0.25
Nodes (8): BoundOrbitInitialConditions, phi0, r0, t0, theta0, vphi, vr, vtheta

### Community 3 - "SchwarzschildParameters"
Cohesion: 0.25
Nodes (8): SchwarzschildParameters, mass, build_bound_orbit(), build_custom(), build_null_scatter(), build_radial_freefall(), SimulationConfig, SimulationConfig

### Community 4 - "Architecture Reconstruction Notes"
Cohesion: 0.04
Nodes (48): 1. Geodesic State (the universal currency), 2. Spacetime Geometry, 3. Dynamics, 4. Numerical Integration, 5. Trajectory Solving, 6. Simulation Orchestration, 7. Initial Conditions, Abstract base classes (4) (+40 more)

### Community 5 - "2. Stable Scientific Concepts"
Cohesion: 0.12
Nodes (16): 2.10 Result and Provenance, 2.11 Observation, 2.12 Instrument, 2.13 Experiment, 2.14 Validation Case, 2.15 Concepts deliberately rejected, 2.1 Ray, 2.2 Chart (+8 more)

### Community 6 - "Metric"
Cohesion: 0.14
Nodes (9): Metric, christoffel, GeodesicDynamics, compute_derivative, GeodesicDynamics::GeodesicDynamics(), Vector4d, SchwarzschildMetric, christoffel (+1 more)

### Community 7 - "SGL Architecture Reconstruction"
Cohesion: 0.04
Nodes (45): 11. Unresolved Questions, 1. Conceptual Architecture, 3. Dependency Model, 4. Data Model, 5. Execution Model, 6. Extension Points, 8. Framework Identity, 9. Architectural Tensions (+37 more)

### Community 8 - "CustomInitialConditions"
Cohesion: 0.22
Nodes (9): CustomInitialConditions, phi0, r0, t0, theta0, vphi, vr, vt (+1 more)

### Community 9 - "4. Abstraction Failure Analysis"
Cohesion: 0.17
Nodes (12): 4. Abstraction Failure Analysis, CoordinateChart, `DynamicsModel`, Initial Condition Structs, `Integrator`, `Metric`, Observables, `SimulationConfig` (+4 more)

### Community 11 - "1. Future Capability Analysis"
Cohesion: 0.10
Nodes (21): 10. Finite Solar-Disk Effects, 11. Parameter Sweeps, 12. CPU/GPU Execution, 13. HPC Execution, 14. Distributed Experiments, 15. Large-Scale Trajectory Datasets, 16. Multiple Analysis Methods, 17. Scientific Validation Pipelines (+13 more)

### Community 12 - "14. Current → Target Architectural Delta"
Cohesion: 0.14
Nodes (14): 14. Current → Target Architectural Delta, D10 — Autonomous derivative, D11 — Validation is a single executable, D12 — No experiment layer, D1 — Missing problem layer, D2 — Configuration as a central dependency, D3 — Result is trajectory-shaped and consumer-shaped, D4 — Step contract carries no diagnostics (+6 more)

### Community 13 - "8.1 Physics assumptions"
Cohesion: 0.08
Nodes (25): 8.1 Physics assumptions, 8.2 Numerical assumptions, 8.3 Data assumptions, 8.4 Execution assumptions, 8.5 Visualization assumptions, 8.6 Severity summary, 8. Hidden Assumptions, A10 — Double precision, 4 dimensions, fixed layout (+17 more)

### Community 14 - "7. Architectural Contracts"
Cohesion: 0.20
Nodes (10): 7. Architectural Contracts, Dynamics Contract, Initial Condition Contract, Integrator Contract, Metric Contract, Pipeline Contract, Result Contract, `State` Contract (+2 more)

### Community 15 - "3. Architectural Boundaries"
Cohesion: 0.20
Nodes (10): 3. Architectural Boundaries, B1 — Domain Data (shared kernel), B2 — Physics / Domain Model, B3 — Numerical Methods, B4 — Composition (thin seam, not a subsystem), B5 — Execution, B6 — Observation and Instrument, B7 — Analysis and Validation (+2 more)

### Community 16 - "Detailed entries for the highest-risk capabilities"
Cohesion: 0.20
Nodes (10): 12. Future Capability Matrix, C11 — Observer / Spacecraft Dynamics, C14 — Adaptive Integration, C18 / C19 — Batch, Vectorized, and GPU Execution, C21 — Large Trajectory Datasets, C26 — Wave Optics / Diffraction, C5 / C6 — Ray Ensembles and Bundles, C7 — Image Formation (+2 more)

### Community 17 - "SimulationConfig"
Cohesion: 0.14
Nodes (14): GeodesicKind, Scenario, SimulationConfig, dt, geodesic, horizon_safety_factor, max_steps, name (+6 more)

### Community 18 - "11. Architectural Principles"
Cohesion: 0.22
Nodes (9): 11. Architectural Principles, P1 — Additive growth, P2 — Scientific description independent of execution strategy, P3 — Physical model separable from numerical method, P4 — Results independent of consumers, P5 — Multiple result kinds, P6 — Validation separable from production, P7 — Reference implementations remain authoritative (+1 more)

### Community 19 - "2. Scientific Evolution"
Cohesion: 0.22
Nodes (9): 2. Scientific Evolution, Stage 1 — Relativistic Propagation, Stage 2 — Solar Gravitational Lens Geometry, Stage 3 — Ray Ensembles, Stage 4 — Image Formation, Stage 5 — Optical / Instrument Modeling, Stage 6 — Extended Sources, Stage 7 — Observer / Spacecraft Dynamics (+1 more)

### Community 20 - "SGL — scientific computing foundation"
Cohesion: 0.22
Nodes (8): Build, Dependencies, Layout, Next steps, Penrose → SGL mapping, SGL — scientific computing foundation, What this is, What this is not

### Community 21 - "9. Architectural Weaknesses"
Cohesion: 0.17
Nodes (12): 9.1 Weakness ranking, 9. Architectural Weaknesses, W10 — Dead and detached components, W1 — No representation of the scientific question, W2 — Orchestration contains physics, W3 — The result type is trajectory-shaped and consumer-shaped, W4 — The step contract has no diagnostic channel, W5 — Single ray is the base case, not the degenerate case (+4 more)

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
Nodes (8): 15. Architectural Invariants, 1. Architectural Objective, 5. Target Data Flow, Document Status, SGL Target Architecture, The single organizing rule, What the objective explicitly does not include, Which structures are stable architectural contracts

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

### Community 44 - "SGL Architectural Review"
Cohesion: 0.18
Nodes (10): 10.1 Capability table, 10.2 What the table shows, 10. Future Stress Test Results, 6. Current Extension Points, Appendix A — Reconciliation of prior phases, Appendix B — Claims carried forward as unresolved, Document Status, Final Architectural Principle (+2 more)

### Community 45 - "4. Target Dependency Graph"
Cohesion: 0.67
Nodes (3): 4. Target Dependency Graph, The one near-cycle, and how it is cut, Why each dependency exists

### Community 46 - "6. Target Extension Model"
Cohesion: 0.67
Nodes (3): 6. Target Extension Model, Reading the table, The genuine exception

### Community 47 - "13.1 Genuinely missing"
Cohesion: 0.18
Nodes (11): 13.1 Genuinely missing, 13.2 Missing, lower priority but justified, 13.3 Explicitly not missing, 13. Missing Abstractions, M1 — Propagation Problem, M2 — Ray Ensemble, M3 — Generalized Result with provenance, M4 — Lens / Source / Observer geometry (+3 more)

### Community 48 - "4. Subsystem Responsibilities"
Cohesion: 0.18
Nodes (11): 4.1 Core Data — `physics/core/`, 4.2 Physics — `physics/metrics/`, `physics/geodesics/`, 4.3 Numerics — `physics/integrators/`, 4.4 Simulation — `physics/simulation/`, 4.4a `TrajectorySolver`, 4.4b `TerminationPolicy`, 4.4c `SimulationPipeline` + `SimulationConfig`, 4.5 Initial Conditions — `physics/simulation/initial_conditions/` (+3 more)

### Community 49 - "Executive Summary"
Cohesion: 0.18
Nodes (11): Current architectural maturity, Executive Summary, Largest architectural weakness, Most important hidden assumption, Most likely future architectural problem, Overall extensibility, Preliminary verdict, Relationship to Penrose (+3 more)

### Community 50 - "SimulationPipeline.cpp"
Cohesion: 0.35
Nodes (10): function, Scenario, SimulationConfig, integrate_schwarzschild(), make_schwarzschild_metric(), make_schwarzschild_post_step(), require_spacetime(), run_simulation() (+2 more)

### Community 51 - "10. Architectural Invariants"
Cohesion: 0.20
Nodes (10): 10. Architectural Invariants, Autonomous State Derivative, Caller-Owned Configuration, Christoffel-Based Physical Modeling, CPU/Eigen Execution, Fixed-Step State Integration, In-Memory Trajectory as Primary Result, Policy-Based Termination (+2 more)

### Community 52 - "2. Subsystem Model"
Cohesion: 0.20
Nodes (10): 2. Subsystem Model, Coordinate Chart Utilities, Core State and Vocabulary, Geodesic Dynamics, Initial Conditions, Metric Model, Numerical Integration, Observables (+2 more)

### Community 53 - "18. Overall Assessment"
Cohesion: 0.22
Nodes (9): 18. Overall Assessment, Current architectural maturity, Most dangerous hidden assumption, Most important architectural boundary, Most important architectural risk, Most important missing abstraction, Most likely future refactor, Most significant coupling (+1 more)

### Community 54 - "7. Architectural Strengths"
Cohesion: 0.22
Nodes (9): 7.1 Numerics is genuinely independent of physics, 7.2 Physics is independent of the specific spacetime *at the dynamics level*, 7.3 Ownership is simple, explicit, and correct, 7.4 Determinism by construction, 7.5 The numerical implementation is demonstrably correct, 7.6 Minimal dependency footprint, 7.7 The right primitive for ensembles already exists, 7.8 Analysis is already correctly shaped (+1 more)

### Community 55 - "14. Penrose Extraction Analysis"
Cohesion: 0.25
Nodes (8): 14.1 What was successfully extracted, 14.2 What was successfully generalized, 14.3 What assumptions remain inherited, 14.4 Penrose-specific boundaries that remain, 14.5 Which inherited abstractions remain appropriate, 14.6 Did the extraction establish genuine architectural independence?, 14.7 Did it produce a foundation suitable for SGL's future?, 14. Penrose Extraction Analysis

### Community 56 - "17. Future Refactoring Risks"
Cohesion: 0.25
Nodes (8): 17.1 The single most likely fundamental refactor, 17. Future Refactoring Risks, R1 — The ensemble transition, R2 — The adaptive integration contract change, R3 — The result-type generalization, R4 — The device-execution wall, R5 — The second gravitational model, R6 — The wave-optics question

### Community 57 - "6. Predicted Refactoring"
Cohesion: 0.25
Nodes (8): 6. Predicted Refactoring, Affected Subsystems, Current Evidence, Future Trigger, Likely Blast Radius, Most Likely Future Architectural Failure, Project Stage When It Matters, Root Cause

### Community 58 - "7. Adversarial Assessment"
Cohesion: 0.25
Nodes (8): 7. Adversarial Assessment, Boundaries That Will Probably Move, Claims That May Be Overly Optimistic, False Modularity, Future Requirements That Do Not Fit, Hidden Coupling, Missing Abstractions, Premature Abstractions

### Community 59 - "2. Architectural Overview"
Cohesion: 0.29
Nodes (7): 2.1 The shape of the system, 2.2 Conceptual diagram — actual current architecture, 2.3 Major subsystems, 2.4 Major interfaces, 2.5 Execution structure, 2.6 Scientific data model, 2. Architectural Overview

### Community 60 - "5. Future Dependency Risks"
Cohesion: 0.29
Nodes (7): 5. Future Dependency Risks, Backend Paths Fork the Architecture, Data Products Depend on Simulation Internals, Physical Model Logic Spreads Across Layers, Pipeline Becomes a God Module, `SimulationConfig` Becomes a Cross-Subsystem Dumping Ground, `State` Locks the Entire Framework to One Memory Model

### Community 61 - "InitialConditions.h"
Cohesion: 0.29
Nodes (5): RadialFreefallInitialConditions, phi0, r0, t0, theta0

### Community 62 - "16. Architectural Maturity"
Cohesion: 0.33
Nodes (6): 16.1 Assessment: Structured Application, 16.2 Why above Prototype, 16.3 Why below Modular Application overall, 16.4 Why far below Scientific Framework, 16.5 Dimension-by-dimension, 16. Architectural Maturity

### Community 63 - "1. Project Context"
Cohesion: 0.33
Nodes (6): 1.1 The science, from first principles, 1.2 The computational problem, 1.3 Relationship to Penrose, and why the extraction happened, 1.4 Current scope, 1.5 Long-term direction, 1. Project Context

### Community 64 - "3. Dependency Graph"
Cohesion: 0.33
Nodes (6): 3.1 The graph, 3.2 Central and leaf components, 3.3 Classification of dependencies, 3.4 Cycles, 3.5 Stable boundaries, 3. Dependency Graph

### Community 65 - "3. Hidden Assumption Analysis"
Cohesion: 0.33
Nodes (6): 3. Hidden Assumption Analysis, Data Assumptions, Execution Assumptions, Numerics Assumptions, Physics Assumptions, Rendering / Visualization Assumptions

### Community 66 - "11. Target Architecture"
Cohesion: 0.40
Nodes (5): 11.1 The organizing rule, 11.2 Stable scientific concepts, 11.3 Boundaries and dependency direction, 11.4 CURRENT vs TARGET at a glance, 11. Target Architecture

### Community 67 - "15. Framework Identity"
Cohesion: 0.40
Nodes (5): 15.1 What SGL is today, 15.2 Evaluating the candidate identities, 15.3 Does the architecture match the identity SGL is becoming?, 15.4 Assumptions anchoring SGL to Penrose, 15. Framework Identity

### Community 68 - "19. Final Verdict"
Cohesion: 0.40
Nodes (5): 19.1 Reasoning, 19.2 Why this is not a fundamental redesign, 19.3 What would reverse this verdict, 19.4 Summary judgement, 19. Final Verdict

### Community 69 - "5. Scientific Data Flow"
Cohesion: 0.40
Nodes (5): 5.1 Actual flow, 5.2 Transition analysis, 5.3 Is scientific data independent of presentation and execution?, 5.4 Two structural observations, 5. Scientific Data Flow

### Community 70 - "How to read this document"
Cohesion: 0.40
Nodes (5): Companion documents, Evidence standard, How to read this document, Three things kept separate throughout, Verification basis

### Community 71 - "vcpkg.json"
Cohesion: 0.40
Nodes (4): eigen3, dependencies, name, version

### Community 72 - "SGL Architectural Stress Test"
Cohesion: 0.50
Nodes (3): 2. Modification Hotspots, 8. Final Stress-Test Verdict, SGL Architectural Stress Test

### Community 73 - "12. Current → Target Architectural Gap"
Cohesion: 0.67
Nodes (3): 12.1 The structure of the gap, 12.2 Is the current architecture structurally aligned with the target?, 12. Current → Target Architectural Gap

### Community 74 - "9. Target Execution Architecture"
Cohesion: 0.67
Nodes (3): 9. Target Execution Architecture, Consequences for the data model, The minimum abstraction

## Knowledge Gaps
- **500 isolated node(s):** `X`, `U`, `christoffel`, `mass`, `compute_derivative` (+495 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `SGL Architectural Review` connect `SGL Architectural Review` to `8.1 Physics assumptions`, `9. Architectural Weaknesses`, `13.1 Genuinely missing`, `4. Subsystem Responsibilities`, `Executive Summary`, `18. Overall Assessment`, `7. Architectural Strengths`, `14. Penrose Extraction Analysis`, `17. Future Refactoring Risks`, `2. Architectural Overview`, `16. Architectural Maturity`, `1. Project Context`, `3. Dependency Graph`, `11. Target Architecture`, `15. Framework Identity`, `19. Final Verdict`, `5. Scientific Data Flow`, `How to read this document`, `12. Current → Target Architectural Gap`?**
  _High betweenness centrality (0.053) - this node is a cross-community bridge._
- **Why does `State` connect `State` to `SimulationConfig.h`, `SimulationPipeline.cpp`, `SchwarzschildParameters`, `Metric`?**
  _High betweenness centrality (0.017) - this node is a cross-community bridge._
- **Why does `SGL Target Architecture` connect `SGL Target Architecture` to `8. Target Numerical Architecture`, `2. Stable Scientific Concepts`, `16. Target Architecture Verdict`, `11. Target Validation Architecture`, `13. Minimality Analysis`, `7. Target Physics Architecture`, `10. Target Observation Architecture`, `12. Target Visualization Architecture`, `14. Current → Target Architectural Delta`, `4. Target Dependency Graph`, `6. Target Extension Model`, `3. Architectural Boundaries`, `9. Target Execution Architecture`?**
  _High betweenness centrality (0.016) - this node is a cross-community bridge._
- **What connects `X`, `U`, `christoffel` to the rest of the system?**
  _500 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `State` be split into smaller, more focused modules?**
  _Cohesion score 0.05737704918032787 - nodes in this community are weakly interconnected._
- **Should `Architecture Reconstruction Notes` be split into smaller, more focused modules?**
  _Cohesion score 0.04081632653061224 - nodes in this community are weakly interconnected._
- **Should `2. Stable Scientific Concepts` be split into smaller, more focused modules?**
  _Cohesion score 0.125 - nodes in this community are weakly interconnected._
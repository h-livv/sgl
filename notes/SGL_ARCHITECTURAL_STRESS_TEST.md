# SGL Architectural Stress Test

Phase 3 adversarial stress test. This document does not propose implementation
steps and does not prescribe refactors. It tests whether the architecture that
exists now can absorb several years of likely scientific expansion.

Source artifacts:

- Phase 1: `notes/ARCHITECTURE_RECONSTRUCTION.md`
- Phase 2: `notes/SGL_ARCHITECTURE_RECONSTRUCTION.md`
- Implementation graph: `graphify-out/graph.json`
- Key implementation files: `SimulationConfig.h`, `SimulationPipeline.cpp`,
  `TrajectorySolver.{h,cpp}`, `GeodesicDynamics.cpp`,
  `RK4Integrator.cpp`, `SchwarzschildInitialStateBuilders.cpp`,
  `SchwarzschildMetric.cpp`, `SchwarzschildObservables.h`

## 1. Future Capability Analysis

Stress-test scale:

- **Extension:** fits by adding a module or concrete implementation.
- **Interface Extension:** current boundary mostly survives, but signatures or
  result/config types need to grow.
- **Modification:** existing concrete modules must change, but core architecture
  can remain.
- **Refactor:** responsibility boundaries must move.
- **Fundamental Redesign:** current core model is the wrong shape for the
  capability.

### 1. Additional Gravitational / Physical Models

- **Natural home:** metric/physics layer plus pipeline orchestration.
- **Current support:** `Spacetime::Metric` can represent another Christoffel
  source; `Dynamics::GeodesicDynamics` can consume it.
- **Invalid assumptions:** `SimulationConfig::spacetime` has one value;
  `SchwarzschildParameters` is the only metric parameter type; metadata assumes
  Schwarzschild spherical chart; pipeline throws for any non-Schwarzschild
  spacetime.
- **Architectural friction:** the lower metric seam survives, but the public
  API and pipeline are not model-neutral.
- **Existing modules needing modification:** `MetricKind.h`,
  `SimulationConfig.h`, `SimulationPipeline.cpp`, initial-state builders,
  observables, metadata construction, CMake source list.
- **New module possible:** partially. A new metric class can be added, but it
  cannot participate in public simulation without modifying the pipeline.
- **Existing interface change:** likely yes for metric parameters and
  `run_simulation`; not necessarily for `Metric` itself.
- **New abstraction required:** likely a physical-model or propagation-problem
  abstraction that binds metric, parameters, chart, IC builder, and metadata.
- **Dependency graph change:** yes; the central pipeline would gain more
  concrete dependencies unless an intermediate model layer appears.
- **Coupling risk:** high coupling between simulation config, metric selection,
  initial conditions, and observables.
- **Classification:** **Refactor** for public support; **Extension** only for
  direct low-level solver use.

### 2. Multiple Numerical Integrators

- **Natural home:** `physics/integrators`.
- **Current support:** strong. `Integration::Integrator` is narrow and physics
  free; `TrajectorySolver` accepts an `Integrator&`.
- **Invalid assumptions:** public `run_simulation` always passes
  `Integration::default_integrator()`.
- **Architectural friction:** direct solver callers can use another integrator;
  public simulation callers cannot choose one.
- **Existing modules needing modification:** only `SimulationConfig.h` and
  `SimulationPipeline.cpp` if integrator selection must be public.
- **New module possible:** yes, for another fixed-step integrator.
- **Existing interface change:** no for `TrajectorySolver`; yes for public
  pipeline selection.
- **New abstraction required:** not for fixed-step integrators.
- **Dependency graph change:** low if direct; moderate if pipeline selection is
  added.
- **Coupling risk:** low in numerics, moderate in pipeline.
- **Classification:** **Extension** at solver level; **Interface Extension** at
  public simulation level.

### 3. Adaptive Integration

- **Natural home:** numerical integration and trajectory solving.
- **Current support:** weak. `Integrator::step` returns only `State`; no error
  estimate, accepted/rejected step, or updated `dt`.
- **Invalid assumptions:** fixed `dt`; fixed number of loop iterations;
  termination checked once per accepted step; history stores one state per loop;
  solver config has scalar `dt` and `max_steps`.
- **Architectural friction:** adaptivity is not just a new integrator because
  the solver controls loop progression and history semantics.
- **Existing modules needing modification:** `Integrator.h`, `TrajectorySolver`,
  `SimulationConfig.h`, `SimulationPipeline.cpp`, possibly `SimulationResult`
  if per-step metadata is needed.
- **New module possible:** not cleanly. An adaptive integrator needs a richer
  conversation with the solver.
- **Existing interface change:** yes, `Integrator` and likely solver result
  shape.
- **New abstraction required:** likely a step result/controller concept.
- **Dependency graph change:** yes; numerical control becomes a separate
  participant instead of a single function call.
- **Coupling risk:** high between solver, integrator, config, and result
  storage.
- **Classification:** **Refactor**.

### 4. Large Photon Ensembles

- **Natural home:** a propagation or experiment layer above single-ray solving.
- **Current support:** `TrajectorySolver::propagate` can avoid storing full
  history for one ray, but public `run_simulation` always stores full history
  for one trajectory.
- **Invalid assumptions:** one `State` current, one `std::vector<State>`
  history, one config, one result, serial loop, scalar `double` fields.
- **Architectural friction:** billions of rays do not fit the object-per-ray
  full-history model.
- **Existing modules needing modification:** `SimulationResult`,
  `SimulationPipeline.cpp`, possibly `State`, `TrajectorySolver`, and CMake if
  parallel/runtime infrastructure appears.
- **New module possible:** only if it bypasses `run_simulation` and calls
  lower-level pieces directly. Existing public architecture does not provide an
  ensemble model.
- **Existing interface change:** yes for public result and execution API.
- **New abstraction required:** photon ensemble, ray batch, or propagation job.
- **Dependency graph change:** yes; an ensemble layer would become central.
- **Coupling risk:** very high if implemented by looping over
  `run_simulation`.
- **Classification:** **Fundamental Redesign** for framework-scale ensembles;
  **Modification** for small ad hoc loops.

### 5. Ray Bundles

- **Natural home:** optics/geometric propagation layer.
- **Current support:** none beyond single `State` and possible direct use of
  `propagate`.
- **Invalid assumptions:** state is a single geodesic; result is a single
  trajectory; no bundle metadata, no neighboring-ray derivatives, no beam
  cross-section state.
- **Architectural friction:** bundle evolution is not just many independent
  `State` objects if bundle shape, convergence, or correlations matter.
- **Existing modules needing modification:** likely `State` or a parallel data
  model, `TrajectorySolver`, `SimulationResult`, public pipeline.
- **New module possible:** partially, if modeled as external collection of
  independent trajectories; not if bundle state is first-class.
- **Existing interface change:** likely yes.
- **New abstraction required:** ray bundle or beam state.
- **Dependency graph change:** yes; optics would depend on propagation while
  propagation may need batch/bundle-aware state.
- **Coupling risk:** high between optics, state representation, and solver.
- **Classification:** **Refactor** to **Fundamental Redesign**, depending on
  whether bundles are first-class.

### 6. Dynamic Source / Observer Geometry

- **Natural home:** problem-definition and optics layers above propagation.
- **Current support:** none. No source, observer, time-varying geometry, or
  external ephemeris model exists.
- **Invalid assumptions:** initial conditions are static POD structs; observer
  information is absent; metric is static; `DynamicsModel` has no explicit time
  or environment context.
- **Architectural friction:** moving observers/sources require a problem model
  that can generate rays over time and consume final/intersection states.
- **Existing modules needing modification:** public API, initial-condition
  model, result model, possibly `DynamicsModel` if motion affects propagation
  equations or termination surfaces.
- **New module possible:** only as an external driver that repeatedly creates
  current IC structs; that would not be an architectural fit.
- **Existing interface change:** yes for a first-class source/observer model.
- **New abstraction required:** source, observer, observation geometry, and
  propagation problem.
- **Dependency graph change:** yes; problem definition becomes upstream of
  initial-state construction.
- **Coupling risk:** high if source/observer details leak into
  `SimulationConfig`.
- **Classification:** **Fundamental Redesign** for first-class mission geometry.

### 7. Image Formation

- **Natural home:** optics layer consuming propagated rays or bundles.
- **Current support:** no image plane, detector, lens equation, ray collection,
  or intensity accumulation. `SimulationResult` contains trajectory history,
  not image data.
- **Invalid assumptions:** output is trajectory; one simulation returns one
  `SimulationResult`; no mapping from source intensity to pixels.
- **Architectural friction:** image formation wants ensembles and aggregation,
  not single-ray history.
- **Existing modules needing modification:** `SimulationResult`, public API,
  likely trajectory storage and pipeline.
- **New module possible:** partially as an external consumer if it owns ray
  generation and calls low-level propagation. Not through current public API.
- **Existing interface change:** yes for framework-native image output.
- **New abstraction required:** image plane, detector/sampler, ray-to-pixel
  accumulation, source model.
- **Dependency graph change:** yes; optics/image layer becomes a major consumer
  and probably drives propagation.
- **Coupling risk:** high if `SimulationResult` becomes a catch-all for both
  trajectories and images.
- **Classification:** **Fundamental Redesign** at application level.

### 8. PSF Modeling

- **Natural home:** analysis/optics layer downstream of propagation and image
  formation.
- **Current support:** none. No point-spread-function concept, no wavelength,
  aperture, detector, or convolution data.
- **Invalid assumptions:** results are trajectories; observables are scalar
  geodesic diagnostics; no image-domain data model.
- **Architectural friction:** PSF work requires image/intensity fields and often
  wavelength-dependent or wave-optical terms.
- **Existing modules needing modification:** result model and analysis boundary
  if native; no existing module naturally owns PSF.
- **New module possible:** yes only outside current propagation API, after a new
  image data product exists.
- **Existing interface change:** yes if PSF is integrated into simulations.
- **New abstraction required:** PSF model, optical system/detector model, image
  data object.
- **Dependency graph change:** yes, but mostly by adding downstream analysis
  layers after missing image abstractions.
- **Coupling risk:** high if PSF parameters are stuffed into
  `SimulationConfig`.
- **Classification:** **Fundamental Redesign** until image formation exists;
  later likely **Extension**.

### 9. Extended Sources

- **Natural home:** source model and image formation layer.
- **Current support:** none. Initial conditions describe one geodesic scenario,
  not a spatially extended emission model.
- **Invalid assumptions:** point-like ray launch; no source surface/brightness
  distribution; no sampling domain.
- **Architectural friction:** extended sources require sampling, integration,
  and output aggregation, not just a different initial `State`.
- **Existing modules needing modification:** initial-condition architecture,
  public pipeline, result model.
- **New module possible:** partially as an external source sampler driving many
  calls; not as native architecture.
- **Existing interface change:** yes.
- **New abstraction required:** source distribution and sampling strategy.
- **Dependency graph change:** yes; source model becomes upstream of ray
  generation.
- **Coupling risk:** high between source sampling and solver execution if no
  batch boundary exists.
- **Classification:** **Fundamental Redesign** for native support.

### 10. Finite Solar-Disk Effects

- **Natural home:** solar/lens model inside optics and physical modeling.
- **Current support:** only `Constants::solar_radius_m` exists and is unused.
- **Invalid assumptions:** lens is a Schwarzschild point mass/radius parameter;
  spacetime is vacuum; no occultation, extended mass distribution, solar limb,
  plasma, or boundary geometry.
- **Architectural friction:** finite disk effects cross physics, ray
  termination/intersection, source/observer geometry, and image formation.
- **Existing modules needing modification:** metric/physical model, termination
  policies, initial/problem model, result/analysis, possibly units.
- **New module possible:** only for post-hoc analysis; core effects need
  propagation-time participation.
- **Existing interface change:** likely yes.
- **New abstraction required:** solar body/lens object, occlusion or material
  interaction boundary, possibly non-vacuum model.
- **Dependency graph change:** yes; lens/body geometry becomes a dependency of
  propagation and analysis.
- **Coupling risk:** high because no current body/lens abstraction exists.
- **Classification:** **Fundamental Redesign** if physically modeled during
  propagation; **Modification** for simple post-filtering.

### 11. Parameter Sweeps

- **Natural home:** experiment/execution orchestration above simulations.
- **Current support:** none except external loops over C++ calls.
- **Invalid assumptions:** one config, one result, synchronous execution,
  in-memory trajectory result.
- **Architectural friction:** sweep metadata, reproducibility, result indexing,
  and failures are not modeled.
- **Existing modules needing modification:** not required for an external
  caller; required if SGL owns experiments.
- **New module possible:** yes as an experiment layer that treats current API as
  a black box, but scalability is limited by result shape.
- **Existing interface change:** no for small sweeps; yes for large or
  persisted sweeps.
- **New abstraction required:** experiment/run set, parameter grid, result
  catalog.
- **Dependency graph change:** moderate; experiment layer would depend on
  simulation API and storage.
- **Coupling risk:** moderate if `SimulationConfig` becomes the only parameter
  schema.
- **Classification:** **Extension** for small external sweeps;
  **Interface Extension** for native, large sweeps.

### 12. CPU/GPU Execution

- **Natural home:** execution backend and numerical kernels.
- **Current support:** CPU-only Eigen with virtual interfaces and
  `std::function`.
- **Invalid assumptions:** scalar `State` objects, dynamic dispatch,
  `std::vector<State>`, `std::function`, heap-owned metric, host memory,
  synchronous single-thread loop.
- **Architectural friction:** GPU wants batch data layouts, device kernels,
  explicit memory movement, and fewer virtual/function-call boundaries.
- **Existing modules needing modification:** `State`, `Metric`, `DynamicsModel`,
  `Integrator`, `TrajectorySolver`, `SimulationResult`, CMake/build.
- **New module possible:** not cleanly if it must share current interfaces; a
  GPU path would likely bypass them.
- **Existing interface change:** yes for backend-native execution.
- **New abstraction required:** backend/execution policy, batch state storage,
  device-compatible kernels.
- **Dependency graph change:** major; backend layer becomes central and splits
  CPU and GPU paths.
- **Coupling risk:** very high. Current abstraction style is CPU object-model
  oriented.
- **Classification:** **Fundamental Redesign** for real GPU execution.

### 13. HPC Execution

- **Natural home:** experiment/execution layer above propagation plus storage.
- **Current support:** none beyond pure deterministic function calls.
- **Invalid assumptions:** single process, in-memory result, one simulation per
  call, synchronous execution, no checkpoints, no job identity.
- **Architectural friction:** HPC needs partitioning, checkpoint/restart,
  deterministic run metadata, storage layout, failure handling, and often
  batch-friendly kernels.
- **Existing modules needing modification:** public API, result model, storage,
  possibly solver if checkpoints occur mid-trajectory.
- **New module possible:** partially as an external harness, but current result
  shape does not scale.
- **Existing interface change:** likely yes for native HPC workflows.
- **New abstraction required:** job, partition, checkpoint, dataset, run
  metadata.
- **Dependency graph change:** yes; execution orchestration and storage become
  first-class.
- **Coupling risk:** high if HPC state is threaded through `SimulationConfig`.
- **Classification:** **Fundamental Redesign** for native HPC; **Extension** for
  simple external embarrassingly parallel runs.

### 14. Distributed Experiments

- **Natural home:** experiment orchestration and persistent storage layers.
- **Current support:** none.
- **Invalid assumptions:** local object ownership, process-local `std::vector`,
  no serialization, no run IDs, no resumability.
- **Architectural friction:** distributed runs need data contracts independent
  of process memory and C++ object lifetimes.
- **Existing modules needing modification:** `SimulationConfig`,
  `SimulationResult`, execution API, storage.
- **New module possible:** only as an outside wrapper with its own schemas.
- **Existing interface change:** yes for native support.
- **New abstraction required:** serializable problem spec, run artifact, dataset
  manifest, scheduler boundary.
- **Dependency graph change:** major.
- **Coupling risk:** high around config/result formats.
- **Classification:** **Fundamental Redesign** for native distributed
  experiments.

### 15. Large-Scale Trajectory Datasets

- **Natural home:** data/storage layer downstream of propagation.
- **Current support:** `SimulationResult::history` is an in-memory
  `std::vector<State>`.
- **Invalid assumptions:** entire trajectory fits in memory; one result owns all
  states; data is immediately consumed in-process.
- **Architectural friction:** large datasets require streaming, chunking,
  compression, schema, indexing, and metadata beyond a vector.
- **Existing modules needing modification:** `TrajectorySolver`,
  `SimulationResult`, public API, possibly solver callback model.
- **New module possible:** partially through external conversion after a run;
  not during huge runs because full history is materialized first.
- **Existing interface change:** yes for scalable native output.
- **New abstraction required:** trajectory sink/dataset writer or storage
  artifact.
- **Dependency graph change:** yes; storage becomes a solver consumer.
- **Coupling risk:** high if storage concerns enter the solver directly.
- **Classification:** **Refactor** to **Fundamental Redesign**, depending on
  scale.

### 16. Multiple Analysis Methods

- **Natural home:** analysis layer consuming trajectories, images, or datasets.
- **Current support:** one header of inline Schwarzschild observables; smoke
  test calls a subset.
- **Invalid assumptions:** analysis is scalar and local to a single `State`;
  no analysis result object, validation suite, provenance, or dataset input.
- **Architectural friction:** multiple analysis methods need common input data
  contracts and outputs separate from simulation execution.
- **Existing modules needing modification:** not for ad hoc header functions;
  yes for native analysis framework.
- **New module possible:** yes if it consumes `SimulationResult` or future
  datasets; current `SimulationResult` is narrow.
- **Existing interface change:** no for simple trajectory diagnostics; yes for
  images/datasets.
- **New abstraction required:** analysis method/result if analysis is
  first-class.
- **Dependency graph change:** moderate; analysis should depend on data
  products, not pipeline internals.
- **Coupling risk:** moderate if analysis continues to reach into raw `State`
  indexes.
- **Classification:** **Extension** for small methods; **Interface Extension**
  for a validation/analysis framework.

### 17. Scientific Validation Pipelines

- **Natural home:** validation layer plus experiment/result storage.
- **Current support:** one smoke executable checks energy/angular momentum drift.
- **Invalid assumptions:** validation is a single executable, single scenario,
  single tolerance, no benchmark corpus, no reporting format.
- **Architectural friction:** validation requires suites, expected models,
  tolerances, datasets, and reproducibility metadata.
- **Existing modules needing modification:** CMake/test registration,
  observables, result metadata, possibly config to expose validation cases.
- **New module possible:** yes for validation harnesses, but current API and
  data products constrain scope.
- **Existing interface change:** likely no for initial validation; yes for
  richer diagnostics or step metadata.
- **New abstraction required:** validation case/result/report.
- **Dependency graph change:** moderate; validation becomes a real consumer of
  simulation and analysis.
- **Coupling risk:** low to moderate unless validation drives API design.
- **Classification:** **Extension** initially; **Interface Extension** for
  rigorous pipelines.

### 18. Wave Optics / Diffraction

- **Natural home:** new physics/optics layer, not current geodesic solver.
- **Current support:** almost none. Current code is geometric ray propagation.
- **Invalid assumptions:** photon trajectory is sufficient; state is position
  plus tangent only; results are paths, not complex fields; no phase, amplitude,
  wavelength, aperture, or interference.
- **Architectural friction:** wave optics changes the fundamental data product
  from trajectories to fields and integrals.
- **Existing modules needing modification:** likely none can be simply extended;
  `State`, `TrajectorySolver`, `SimulationResult`, observables, and pipeline
  would be bypassed or reinterpreted.
- **New module possible:** yes as a separate module sharing constants and maybe
  geometry; not as a natural extension of current public pipeline.
- **Existing interface change:** yes if unified under one SGL framework API.
- **New abstraction required:** field, phase/amplitude, aperture, diffraction
  operator, wavelength model.
- **Dependency graph change:** major; wave optics is a parallel computational
  branch.
- **Coupling risk:** very high if forced into geodesic abstractions.
- **Classification:** **Fundamental Redesign** for unified architecture;
  **Extension** only as an independent sibling framework.

### 19. Wavelength-Dependent Modeling

- **Natural home:** optical/physical model and problem definition.
- **Current support:** none. No wavelength parameter exists.
- **Invalid assumptions:** propagation depends only on spacetime state and
  `rs`; observables are wavelength-free; `State` has no spectral component.
- **Architectural friction:** wavelength may affect source, medium/plasma,
  diffraction, PSF, and detector response, not just geodesic propagation.
- **Existing modules needing modification:** config/problem model, result data,
  observables/analysis, possibly physical models.
- **New module possible:** only for post-processing if propagation is
  wavelength-independent.
- **Existing interface change:** yes if wavelength affects propagation or
  output.
- **New abstraction required:** spectral model or wavelength-aware problem
  definition.
- **Dependency graph change:** yes; wavelength becomes a cross-cutting
  dimension.
- **Coupling risk:** high if wavelength is appended to `SimulationConfig`
  without a broader optical model.
- **Classification:** **Refactor** to **Fundamental Redesign**, depending on
  whether wavelength affects propagation.

### 20. Mission-Level Simulation

- **Natural home:** top-level mission/experiment framework spanning geometry,
  propagation, optics, analysis, validation, and storage.
- **Current support:** none beyond a propagator kernel.
- **Invalid assumptions:** single simulation, single process, one result in
  memory, no observation schedule, no instruments, no spacecraft/ephemeris, no
  data management.
- **Architectural friction:** mission simulation is a system-of-systems problem;
  current SGL has only the propagation kernel.
- **Existing modules needing modification:** nearly all current public-facing
  types if they are used as the mission API.
- **New module possible:** yes only if the current library is treated as a
  low-level propagation component.
- **Existing interface change:** yes for an integrated framework.
- **New abstraction required:** mission scenario, observation plan, instrument,
  source catalog, execution campaign, dataset, validation pipeline.
- **Dependency graph change:** fundamental; propagation becomes a leaf service
  inside a larger graph.
- **Coupling risk:** extreme if mission concepts are added directly to current
  simulation structs.
- **Classification:** **Fundamental Redesign** for the framework; current
  propagation kernel can survive as a component.

## 2. Modification Hotspots

| Existing Component | Future Feature | Why Modification Is Required | Severity |
|---|---|---|---|
| `SimulationPipeline.cpp` | New physical models | It hard-codes Schwarzschild metric construction, metadata, horizon policy, and null projection. | High |
| `SimulationPipeline.cpp` | Public integrator/policy selection | It always passes `default_integrator()` and constructs `HorizonTermination`. | Medium |
| `SimulationPipeline.cpp` | Ensembles, images, mission simulation | It returns one `SimulationResult` for one trajectory. | Critical |
| `SimulationConfig.h` | New metrics/scenarios | It contains fixed enums, fixed fields, and the public overload declarations. | High |
| `SimulationConfig.h` | Runtime/HPC/distributed execution | It is a C++ aggregate with no serialization, run identity, backend, or storage model. | Critical |
| `SimulationResult` | Large datasets | It owns `std::vector<State>` in memory. | Critical |
| `SimulationResult` | Image formation / PSF / wave optics | It represents trajectory history, not images, fields, spectra, or datasets. | Critical |
| `State` | GPU / batch execution | It is an object-of-structs single-ray Eigen type with public fields and arithmetic operators. | Critical |
| `State` | Wave optics / ray bundles | It stores only position and tangent, not phase, amplitude, polarization, bundle derivatives, or wavelength. | Critical |
| `Integrator` | Adaptive integration | The method returns only `State` and cannot communicate error, accepted step, or new `dt`. | High |
| `TrajectorySolver` | Adaptive integration | It owns the fixed-step loop and history semantics. | High |
| `TrajectorySolver` | Streaming datasets | It materializes history before returning. | High |
| `TrajectorySolver.h` | Solver/backend independence | It includes `RK4Integrator.h` to expose the default integrator. | Medium |
| `InitialConditions.h` | Dynamic sources / extended sources | It models fixed scenario POD structs, not source/observer geometry or sampling domains. | High |
| `SchwarzschildInitialStateBuilders.cpp` | New source models | Builders convert fixed scenarios, not general observation geometry. | High |
| `MetricKind.h` | Multi-metric framework | Single-enumerator identity vocabulary. | Medium |
| `SchwarzschildParameters.h` | Physical model clarity | Field `mass` is used as `rs`; future mass/radius/unit handling will stress this. | Medium |
| `SchwarzschildObservables.h` | Validation pipelines | Header functions provide formulas but no validation case/result/report model. | Medium |
| `CoordinateChart` | Dynamic geometry / optics | Utility functions are isolated and not integrated into problem setup or result conversion. | Medium |
| `CMakeLists.txt` | GPU/HPC backends | Single static CPU library and one optional executable; no backend target structure. | Medium |

## 3. Hidden Assumption Analysis

### Physics Assumptions

| Assumption | Where It Appears | Stress-Test Impact |
|---|---|---|
| Schwarzschild geometry only | `MetricKind` has only `Schwarzschild`; `require_spacetime` rejects anything else; `SimulationPipeline.cpp` constructs `SchwarzschildMetric`. | Multi-model support concentrates changes in config, pipeline, metadata, initial builders, and observables. |
| Static spacetime | `Metric::christoffel` depends only on `X`; `DynamicsModel::compute_derivative` has no environment/time context beyond state. | Time-dependent metrics or moving lens models do not fit naturally. |
| Vacuum propagation | Geodesic equation only; no medium/plasma/interaction terms. | Solar plasma, wavelength effects, absorption, or scattering require more than a metric. |
| Point lens / no finite body | Metric parameter is scalar `rs`; no body radius participates in propagation. | Finite solar disk, occultation, and surface effects need new geometry. |
| No source object | Initial conditions are scenario POD structs. | Extended and moving sources require a new problem-definition layer. |
| No observer object | No observer type in SGL; `SimulationResult` has no detector/image-plane relation. | Dynamic observer and image formation have no architectural home. |
| Photon/timelike split is local | `GeodesicKind` is only read by `build_custom`; null projection depends on solver options. | Photon-only optical pipelines or mixed particle models are not first-class. |
| Fixed coordinate system | `State.X[1]`, `X[2]`, `U[0..3]` are directly indexed throughout. | Multiple charts or coordinate-free interfaces would invalidate raw indexing. |

### Numerics Assumptions

| Assumption | Where It Appears | Stress-Test Impact |
|---|---|---|
| One default integration method | `default_integrator()` returns static `RK4Integrator`; pipeline always uses it. | Public algorithm selection requires API/pipeline change. |
| Fixed timestep | `SimulationConfig::dt`; `Integrator::step(state, dt, derivative)`; fixed loop in `TrajectorySolver`. | Adaptive integration requires solver/integrator contract changes. |
| Fixed precision | All values are `double`/`Vector4d`/`Matrix4d`. | Mixed precision, high precision, or GPU-native precision policies do not fit. |
| Fixed state representation | `State` is exactly two `Vector4d`s. | Bundles, phase, wavelength, amplitude, or batch layouts need new state/data types. |
| Fixed dimensionality | Hard-coded loops over 4 in `GeodesicDynamics`; `Vector4d` everywhere. | Non-4D extensions or augmented systems need a new model. |
| Local state derivative | `DerivativeFunc` accepts only `const State&`. | Derivatives needing context, fields, cache, or external time are awkward. |

### Execution Assumptions

| Assumption | Where It Appears | Stress-Test Impact |
|---|---|---|
| Serial execution | `TrajectorySolver` uses one local `State current` loop. | Ensembles/HPC require external orchestration or new execution layer. |
| Synchronous execution | `run_simulation` returns a value result after completion. | Async jobs, checkpoints, and distributed runs are not modeled. |
| CPU execution | Eigen, virtual calls, `std::function`, `std::vector`. | GPU execution likely bypasses current interfaces. |
| Single simulation | Public API returns one `SimulationResult`; no batch/run-set type. | Parameter sweeps and mission campaigns lack identity and aggregation. |
| Single process | No serialization, IDs, or storage boundaries. | Distributed experiments require new data contracts. |

### Data Assumptions

| Assumption | Where It Appears | Stress-Test Impact |
|---|---|---|
| Trajectory-only result | `SimulationResult::history` is the only consumed output. | Images, fields, PSFs, and datasets do not fit. |
| Full trajectory in memory | `TrajectorySolver::solve` accumulates `std::vector<State>`. | Large datasets and ensembles exceed memory model. |
| Mutable local state | `post_step(State&, int)` mutates solver state after integration. | Pure functional, GPU, or reproducible audit trails need explicit correction semantics. |
| Borrowed lifetimes | `GeodesicDynamics` stores `const Metric&`; solver lambda captures `&dynamics`. | Safe only under current stack-scoped orchestration. |
| Metadata without consumer | `SimulationMetadata` is populated and stored but unused. | The actual downstream contract is unvalidated and may be wrong. |

### Rendering / Visualization Assumptions

| Assumption | Where It Appears | Stress-Test Impact |
|---|---|---|
| Visualization can consume trajectories | `SimulationResult` stores full history and metadata fields. | Image formation and PSF modeling may need aggregated fields, not paths. |
| Rendering-specific metadata belongs in physics result | `horizon_radius`, `photon_sphere_radius`, `coordinate_chart`, `characteristic_radius`. | Physics result may become coupled to presentation/instrument needs. |
| No renderer exists | Placeholder dirs only; README states realtime GPU and viewer stack were excluded. | Future visualization boundary is unconstrained and may cut across existing result types. |

## 4. Abstraction Failure Analysis

### `State`

- **Breaks when:** rays are batched, propagated on GPU, carry phase/amplitude,
  represent bundles, use alternative coordinate charts, or require auxiliary
  per-ray metadata.
- **Reason:** it is both domain state and numeric vector container.
- **Failure mode:** every subsystem depends on its public fields and raw
  indexes, so changing it has repository-wide blast radius.

### `Metric`

- **Breaks when:** physics is not expressible as Christoffel symbols alone, is
  time-dependent, includes media/plasma, or needs cached/global context.
- **Reason:** method signature is one coefficient at one point.
- **Failure mode:** richer physical models either overload the meaning of
  `christoffel` or bypass `Metric`.

### `DynamicsModel`

- **Breaks when:** derivative depends on external environment, wavelength,
  source/observer state, adaptive solver context, or non-`State` fields.
- **Reason:** `compute_derivative(const State&)` has no context argument and
  returns only `State`.
- **Failure mode:** dependencies become captured hidden state, or the interface
  fragments.

### `Integrator`

- **Breaks when:** integration is adaptive, symplectic with internal state,
  event-detecting, batch/GPU-based, or returns diagnostics.
- **Reason:** `step` returns only the next `State`.
- **Failure mode:** solver and integrator responsibilities become tangled.

### `TerminationPolicy`

- **Breaks when:** termination depends on image-plane intersection, observer
  motion, accumulated optical depth, history, step size, or event localization.
- **Reason:** predicate sees only current `State`.
- **Failure mode:** termination logic migrates into solver or post-step hooks.

### `TrajectorySolver`

- **Breaks when:** one trajectory becomes many rays, history cannot be stored,
  integration needs streaming output, jobs are async, or execution is device
  parallel.
- **Reason:** it is single-state, fixed-loop, synchronous, and vector-returning.
- **Failure mode:** external callers bypass it, producing two propagation
  architectures.

### `SimulationConfig`

- **Breaks when:** configuration includes physical model variants, source and
  observer geometry, instruments, backend selection, datasets, or validation
  cases.
- **Reason:** it is a flat aggregate for current Schwarzschild trajectory runs.
- **Failure mode:** it becomes a catch-all object coupling unrelated subsystems.

### `SimulationResult`

- **Breaks when:** output is an image, PSF, field, dataset, statistics, or
  distributed artifact.
- **Reason:** result is centered on `std::vector<State> history`.
- **Failure mode:** result type either balloons or multiple incompatible result
  paths appear.

### Initial Condition Structs

- **Breaks when:** rays are generated from source/observer geometry, extended
  source distributions, detector pixels, or time-varying schedules.
- **Reason:** they encode a small fixed set of analytic geodesic scenarios.
- **Failure mode:** scenario enum and overload set grow linearly with use cases.

### Observables

- **Breaks when:** validation needs suite-level reporting, datasets, statistics,
  or non-Schwarzschild models.
- **Reason:** it is a header of scalar helper formulas.
- **Failure mode:** analysis logic spreads into tests and consumers.

### CoordinateChart

- **Breaks when:** multiple charts or chart-aware pipelines are needed.
- **Reason:** it is a stateless utility island, not part of the state/metric
  contract.
- **Failure mode:** chart conversions are called ad hoc, with no guarantee that
  `State` and `Metric` agree about coordinate meaning.

## 5. Future Dependency Risks

### Pipeline Becomes a God Module

If new metrics, new scenarios, source/observer models, integrator selection,
termination policies, and output choices are added through the current public
API, `SimulationPipeline.cpp` becomes the dependency center for the whole
framework. That would turn today's private wiring file into a long-term
architectural bottleneck.

### `SimulationConfig` Becomes a Cross-Subsystem Dumping Ground

The current config already mixes physical identity, scenario identity,
geodesic kind, numerical step control, solver projection options, and result
name. Future features would be tempted to add wavelength, source, observer,
instrument, backend, output, and sweep controls to the same struct.

### `State` Locks the Entire Framework to One Memory Model

Because `State` is used everywhere, CPU scalar `Vector4d` layout becomes a
framework-wide dependency. GPU, HPC, batch layout, wave optics, and bundle
models all conflict with that dependence.

### Data Products Depend on Simulation Internals

Current analysis reads `State` directly. Future visualization or analysis could
also consume `SimulationResult` directly. Without a separate data product model,
downstream science code will depend on solver internals.

### Physical Model Logic Spreads Across Layers

Schwarzschild formulas already appear in metric, builders, pipeline projection,
and observables. Additional effects could multiply this pattern, making the
dependency graph look modular while the physics is actually distributed across
uncoordinated modules.

### Backend Paths Fork the Architecture

GPU/HPC execution is unlikely to use virtual `Metric`, `std::function`, and
`std::vector<State>` efficiently. A future backend may bypass the current
interfaces, leaving CPU and GPU paths semantically similar but architecturally
separate.

## 6. Predicted Refactoring

### Most Likely Future Architectural Failure

The most likely failure is that the public architecture cannot cross the
boundary from **single-trajectory Schwarzschild propagation** to **many-ray
optical simulation with image/data products**.

### Root Cause

The architecture's real invariant is `State -> TrajectorySolver::solve ->
std::vector<State> -> SimulationResult`. That is appropriate for validating one
geodesic. It is the wrong central shape for ray ensembles, image formation,
large datasets, GPU execution, or mission campaigns.

### Current Evidence

- Public pipeline always returns full trajectory history.
- `TrajectorySolver::propagate` exists for history-free propagation but is not
  wired into the public API.
- `SimulationResult` carries visualization-shaped metadata, but no visualization
  consumer exists.
- `SimulationPipeline.cpp` is the only assembly point and is
  Schwarzschild-specific.
- `State` is scalar, Eigen-based, mutable, and globally shared.

### Future Trigger

The trigger will likely be the first attempt to model many rays from a
source/observer/image-plane geometry. At that point, the core product is no
longer "one trajectory history"; it is a sampled optical/mission data product.

### Affected Subsystems

- `SimulationConfig`
- `SimulationPipeline`
- `TrajectorySolver`
- `SimulationResult`
- `State`
- initial-condition builders
- observables/analysis
- future optics/visualization/storage layers

### Likely Blast Radius

High. The change would touch public API, data model, execution model, and
dependency direction. The lower metric/dynamics/integrator ideas may survive,
but the public simulation architecture would move around them.

### Project Stage When It Matters

- **Minor refactoring stage:** adding another fixed-step integrator, adding
  another observable, or running small parameter sweeps.
- **Fundamental redesign stage:** first-class ray ensembles, image formation,
  GPU/HPC execution, large trajectory datasets, wave optics, or mission-level
  simulation.

## 7. Adversarial Assessment

### Claims That May Be Overly Optimistic

- **"The metric abstraction makes the physics extensible."** True only below
  the pipeline. Public execution is Schwarzschild-specific.
- **"The integrator abstraction makes numerics extensible."** True for
  fixed-step direct solver callers. False for adaptive integration through the
  public pipeline.
- **"SimulationResult is a reusable output contract."** Unsupported by current
  consumers. Only `history` is read.
- **"CoordinateChart is an optics-ready utility."** Unsupported. It has no
  caller and no integration with metric/chart identity.
- **"SGL is an SGL framework."** Unsupported by implementation. It is a
  propagation kernel with placeholders.

### Premature Abstractions

- `MetricKind` and `CoordinateChartKind`: identity enums with one value and no
  branching consumer.
- `SimulationMetadata`: populated but unconsumed.
- `Metric`, `DynamicsModel`, and `Integrator`: valid seams, but single
  implementation means future fit is not yet tested.
- `RadiusBoundTermination` and `propagate`: plausible future-facing APIs, but
  unwired.

### Missing Abstractions

- Propagation problem.
- Physical model package that binds metric, chart, parameters, IC generation,
  and observables.
- Source and observer geometry.
- Ray ensemble or bundle.
- Image plane and detector/instrument.
- Dataset/storage sink.
- Execution backend.
- Experiment/run campaign.
- Validation case and validation report.
- Wavelength/spectral model.

### Hidden Coupling

- Coordinate layout coupling through raw `State.X` and `State.U` indexes.
- Schwarzschild formula coupling across metric, builders, pipeline projection,
  and observables.
- Config/API coupling in `SimulationConfig.h`.
- Solver/default integrator coupling through `TrajectorySolver.h` including
  `RK4Integrator.h`.
- Result/visualization coupling through metadata fields retained from Penrose.

### False Modularity

The directory tree suggests physics, simulation, validation, optics, analysis,
and visualization. The implementation only has a physics/numerics kernel,
one simulation pipeline, and a smoke test. The named empty directories do not
establish boundaries.

### Future Requirements That Do Not Fit

- Billion-ray propagation.
- GPU kernels.
- Adaptive integration with error control.
- Image or field outputs.
- Distributed experiments.
- Moving observers.
- Wave optics.
- Wavelength-dependent media or instruments.
- Mission-level runs.

### Boundaries That Will Probably Move

- Initial conditions will likely move from scenario PODs to problem geometry.
- `SimulationResult` will likely stop being the universal output.
- `TrajectorySolver` may become a low-level single-ray kernel rather than the
  center of public execution.
- Physics will likely need a broader model boundary than `Metric`.
- Analysis and validation will likely move from header helpers and smoke tests
  into real consumers of data products.

## 8. Final Stress-Test Verdict

The current architecture survives as a **small Schwarzschild geodesic
propagation kernel**. Its strongest long-term pieces are:

- `Metric` as a Christoffel provider;
- `DynamicsModel` as an ODE right-hand-side boundary;
- `Integrator` for fixed-step state advancement;
- `TerminationPolicy` for simple stop predicates;
- `TrajectorySolver` for single-trajectory CPU propagation;
- `State` as a compact scalar representation for one geodesic.

It does **not** survive unchanged as a multi-year SGL scientific framework. The
architecture fails the ideal "add new modules rather than modify existing
architecture" at the first serious move toward:

- multi-physics public execution;
- adaptive numerical methods;
- ray ensembles and image formation;
- large datasets;
- GPU/HPC/distributed execution;
- wave optics;
- mission-level simulation.

The adversarial conclusion is therefore split:

1. **The extracted Penrose kernel is coherent and useful as a foundation
   component.**
2. **The current public architecture is not yet the architecture of SGL.**

The most dangerous hidden assumption is that "trajectory of `State`" remains
the central data product. For future SGL work, that assumption is likely to
fail before the lower geodesic math fails.

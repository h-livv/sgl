cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

project(SGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Eigen3 CONFIG REQUIRED)

# Schwarzschild GR / geodesic numerical engine (extracted from Penrose).
set(SGL_PHYSICS_SOURCES
    physics/metrics/CoordinateChart.cpp
    physics/metrics/SchwarzschildMetric.cpp
    physics/geodesics/GeodesicDynamics.cpp
    physics/integrators/RK4Integrator.cpp
    physics/propagation/Propagator.cpp
    physics/propagation/TerminationPolicy.cpp
    physics/schwarzschild/InitialStates.cpp
    physics/schwarzschild/NullConstraint.cpp
    physics/schwarzschild/PropagationContext.cpp
)

add_library(sgl_physics STATIC ${SGL_PHYSICS_SOURCES})
target_include_directories(sgl_physics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/physics
)
target_link_libraries(sgl_physics PUBLIC Eigen3::Eigen)

# SGL problem geometry. Deliberately independent of the numerical kernel:
# this target links Eigen only and must never link sgl_physics.
add_library(sgl_geometry STATIC
    physics/geometry/Observer.cpp
    physics/geometry/ImagePlane.cpp
    physics/problem/PropagationProblem.cpp
)
target_include_directories(sgl_geometry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/physics)
target_link_libraries(sgl_geometry PUBLIC Eigen3::Eigen)

# Ray ensembles. First layer permitted to link both the geometry and the kernel.
add_library(sgl_rays STATIC
    physics/rays/RayEnsemble.cpp
    physics/rays/RaySampler.cpp
    physics/rays/EnsemblePropagator.cpp
)
target_include_directories(sgl_rays PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/physics)
target_link_libraries(sgl_rays PUBLIC sgl_geometry sgl_physics)

# Observer-plane arrivals. Consumes the ensemble path; the kernel never depends on it.
add_library(sgl_arrivals STATIC
    physics/arrivals/ChartMapping.cpp
    physics/arrivals/PlaneCrossingTermination.cpp
    physics/arrivals/ArrivalCollector.cpp
    physics/arrivals/AzimuthalExpansion.cpp
    physics/arrivals/ObserverAngularCoordinates.cpp
)
target_include_directories(sgl_arrivals PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/physics)
target_link_libraries(sgl_arrivals PUBLIC sgl_rays)

# Scientific image formation from plane arrivals.
add_library(sgl_imaging STATIC
    physics/imaging/Image.cpp
    physics/imaging/ImageFormation.cpp
)
target_include_directories(sgl_imaging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/physics)
target_link_libraries(sgl_imaging PUBLIC sgl_arrivals)

add_executable(sgl_canonical_sgl_image experiments/canonical_sgl_image.cpp)
target_link_libraries(sgl_canonical_sgl_image PRIVATE sgl_imaging)

option(SGL_BUILD_TESTS "Build the test suite" ON)
if(SGL_BUILD_TESTS)
    enable_testing()

    add_executable(sgl_null_scatter_regression tests/null_scatter_regression.cpp)
    target_link_libraries(sgl_null_scatter_regression PRIVATE sgl_physics)
    add_test(NAME null_scatter_regression COMMAND sgl_null_scatter_regression)

    add_executable(sgl_schwarzschild_scenarios tests/schwarzschild_scenarios.cpp)
    target_link_libraries(sgl_schwarzschild_scenarios PRIVATE sgl_physics)
    add_test(NAME schwarzschild_scenarios COMMAND sgl_schwarzschild_scenarios)

    add_executable(sgl_null_constraint tests/null_constraint.cpp)
    target_link_libraries(sgl_null_constraint PRIVATE sgl_physics)
    add_test(NAME null_constraint COMMAND sgl_null_constraint)

    add_executable(sgl_propagator_contract tests/propagator_contract.cpp)
    target_link_libraries(sgl_propagator_contract PRIVATE sgl_physics)
    add_test(NAME propagator_contract COMMAND sgl_propagator_contract)

    add_executable(sgl_ensemble_readiness tests/ensemble_readiness.cpp)
    target_link_libraries(sgl_ensemble_readiness PRIVATE sgl_physics)
    add_test(NAME ensemble_readiness COMMAND sgl_ensemble_readiness)

    add_executable(sgl_geometry_frames tests/geometry_frames.cpp)
    target_link_libraries(sgl_geometry_frames PRIVATE sgl_geometry)
    add_test(NAME geometry_frames COMMAND sgl_geometry_frames)

    add_executable(sgl_image_plane tests/image_plane.cpp)
    target_link_libraries(sgl_image_plane PRIVATE sgl_geometry)
    add_test(NAME image_plane COMMAND sgl_image_plane)

    add_executable(sgl_canonical_problem tests/canonical_problem.cpp)
    target_link_libraries(sgl_canonical_problem PRIVATE sgl_geometry)
    add_test(NAME canonical_problem COMMAND sgl_canonical_problem)

    add_executable(sgl_ray_ensemble tests/ray_ensemble.cpp)
    target_link_libraries(sgl_ray_ensemble PRIVATE sgl_rays)
    add_test(NAME ray_ensemble COMMAND sgl_ray_ensemble)

    add_executable(sgl_ray_sampler tests/ray_sampler.cpp)
    target_link_libraries(sgl_ray_sampler PRIVATE sgl_rays)
    add_test(NAME ray_sampler COMMAND sgl_ray_sampler)

    add_executable(sgl_ensemble_propagation tests/ensemble_propagation.cpp)
    target_link_libraries(sgl_ensemble_propagation PRIVATE sgl_rays)
    add_test(NAME ensemble_propagation COMMAND sgl_ensemble_propagation)

    add_executable(sgl_arrival_localization tests/arrival_localization.cpp)
    target_link_libraries(sgl_arrival_localization PRIVATE sgl_arrivals)
    add_test(NAME arrival_localization COMMAND sgl_arrival_localization)

    add_executable(sgl_observer_plane_arrivals tests/observer_plane_arrivals.cpp)
    target_link_libraries(sgl_observer_plane_arrivals PRIVATE sgl_arrivals)
    add_test(NAME observer_plane_arrivals COMMAND sgl_observer_plane_arrivals)

    add_executable(sgl_azimuthal_expansion tests/azimuthal_expansion.cpp)
    target_link_libraries(sgl_azimuthal_expansion PRIVATE sgl_arrivals)
    add_test(NAME azimuthal_expansion COMMAND sgl_azimuthal_expansion)

    add_executable(sgl_image_formation tests/image_formation.cpp)
    target_link_libraries(sgl_image_formation PRIVATE sgl_imaging)
    add_test(NAME image_formation COMMAND sgl_image_formation)

    add_executable(sgl_observer_angular_coordinates tests/observer_angular_coordinates.cpp)
    target_link_libraries(sgl_observer_angular_coordinates PRIVATE sgl_arrivals)
    add_test(NAME observer_angular_coordinates COMMAND sgl_observer_angular_coordinates)

    add_executable(sgl_angular_image_pipeline tests/angular_image_pipeline.cpp)
    target_link_libraries(sgl_angular_image_pipeline PRIVATE sgl_imaging)
    add_test(NAME angular_image_pipeline COMMAND sgl_angular_image_pipeline)

    add_executable(sgl_source_distance_angular_behavior tests/source_distance_angular_behavior.cpp)
    target_link_libraries(sgl_source_distance_angular_behavior PRIVATE sgl_imaging)
    add_test(NAME source_distance_angular_behavior COMMAND sgl_source_distance_angular_behavior)

    add_executable(sgl_canonical_image_pipeline tests/canonical_image_pipeline.cpp)
    target_link_libraries(sgl_canonical_image_pipeline PRIVATE sgl_imaging)
    add_test(NAME canonical_image_pipeline COMMAND sgl_canonical_image_pipeline)
endif()

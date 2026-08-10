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

option(SGL_BUILD_TESTS "Build the Phase 1 test suite" ON)
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
endif()

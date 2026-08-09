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
    physics/simulation/TrajectorySolver.cpp
    physics/simulation/TerminationPolicy.cpp
    physics/simulation/SimulationPipeline.cpp
    physics/simulation/initial_conditions/SchwarzschildInitialStateBuilders.cpp
)

add_library(sgl_physics STATIC ${SGL_PHYSICS_SOURCES})
target_include_directories(sgl_physics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/physics
        ${CMAKE_CURRENT_SOURCE_DIR}/physics/core
)
target_link_libraries(sgl_physics PUBLIC Eigen3::Eigen)

option(SGL_BUILD_SMOKE_TEST "Build a minimal null-geodesic smoke test" ON)
if(SGL_BUILD_SMOKE_TEST)
    add_executable(sgl_null_smoke tests/null_geodesic_smoke.cpp)
    target_link_libraries(sgl_null_smoke PRIVATE sgl_physics)
endif()

cmake_minimum_required(VERSION 3.5)
project(fluid-sim VERSION 0.1.0 LANGUAGES CXX C)

option(BUILD_BENCHMARKS "Build Google Benchmark targets" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Core library (simulation / math only) ----
add_library(
  fluid_core
  src/particles.cpp
  src/linear_algebra.cpp
  src/colors.h
)

target_include_directories(fluid_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies
)

# ---- App ----
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(fluid-sim
  src/main.cpp
  src/glad.c
  src/triangle_mesh.cpp
  # headers do not need to be listed, optional:
  # src/config.h
  # src/helpers.h
)

target_include_directories(fluid-sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fluid-sim PRIVATE
  fluid_core
  glfw
  OpenGL::GL
)

# ---- Google Benchmark ----
# ---- Google Benchmark ----
if (BUILD_BENCHMARKS)

  include(FetchContent)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
  )
  FetchContent_MakeAvailable(benchmark)

  add_executable(fluid-sim-bench
    benchmarks/benchmark.cpp
  )

  target_link_libraries(fluid-sim-bench PRIVATE
    fluid_core
    benchmark::benchmark
  )

  find_package(Threads QUIET)
  if (Threads_FOUND)
    target_link_libraries(fluid-sim-bench PRIVATE Threads::Threads)
  endif()

endif()
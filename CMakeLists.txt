cmake_minimum_required(VERSION 3.10)
project(fluid-sim VERSION 0.1.0 LANGUAGES CXX C)

option(BUILD_BENCHMARKS "Build Google Benchmark targets" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Core library (simulation / math only) ----
add_library(
  fluid_core
  src/particles.cpp
  src/linear_algebra.h
  src/cell.h
  src/cell.cpp
  src/particle_config.h
  src/particle_config.cpp
  src/line_renderer.h
  src/line_renderer.cpp
  src/geometry.h
  src/geometry.cpp
  src/systems/camera_system.h
  src/systems/camera_system.cpp
  src/systems/camera_system.h
  src/systems/camera_system.cpp
  src/systems/hud_system.h
  src/systems/hud_system.cpp
  src/systems/input_system.h
  src/systems/input_system.cpp
  src/systems/control_system.h
  src/systems/control_system.cpp
  src/systems/raycasting_system.h
  src/systems/raycasting_system.cpp
  dependencies/imgui/imgui.cpp
  dependencies/imgui/imgui_demo.cpp
  dependencies/imgui/imgui_draw.cpp
  dependencies/imgui/imgui_tables.cpp
  dependencies/imgui/imgui_widgets.cpp
  dependencies/imgui/imgui_impl_glfw.cpp
  dependencies/imgui/imgui_impl_opengl3.cpp
  dependencies/imgui/imstb_rectpack.h
  dependencies/imgui/imstb_textedit.h
  dependencies/imgui/imstb_truetype.h
)

target_include_directories(fluid_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies
  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui
  ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/imgui/backends
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# TBB — used on platforms where std::execution::par_unseq is unavailable (e.g. Mac)
find_package(TBB QUIET)
if(TBB_FOUND)
  message(STATUS "TBB found — using parallel TBB backend")
  target_link_libraries(fluid_core PRIVATE TBB::tbb)
  target_compile_definitions(fluid_core PUBLIC USE_TBB)
else()
  message(STATUS "TBB not found — using std::execution backend")
endif()

# ---- App ----
if(NOT BUILD_BENCHMARKS)
  include(FetchContent)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
  )
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)

  find_package(OpenGL REQUIRED)

  add_executable(fluid-sim
    src/main.cpp
    src/glad.c
    src/particle_mesh.cpp
    src/cell.h
    src/cell.cpp
    src/line_renderer.h
    src/line_renderer.cpp
    src/geometry.h
    src/geometry.cpp
    src/particle_config.h
    src/particle_config.cpp
    src/systems/camera_system.h
    src/systems/camera_system.cpp
    src/systems/render_system.h
    src/systems/render_system.cpp
    src/systems/hud_system.h
    src/systems/hud_system.cpp
    src/systems/input_system.h
    src/systems/input_system.cpp
    src/systems/control_system.h
    src/systems/control_system.cpp
    src/systems/raycasting_system.h
    src/systems/raycasting_system.cpp
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
endif()

# ---- Google Benchmark ----
if(BUILD_BENCHMARKS)
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

  if(TBB_FOUND)
    target_link_libraries(fluid-sim-bench PRIVATE TBB::tbb)
    target_compile_definitions(fluid-sim-bench PUBLIC USE_TBB)
  endif()

  find_package(Threads QUIET)
  if(Threads_FOUND)
    target_link_libraries(fluid-sim-bench PRIVATE Threads::Threads)
  endif()
endif()

cmake_minimum_required(VERSION 3.5)
project(fluid-sim VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
add_executable(fluid-sim 
    src/config.h 
    src/main.cpp 
    src/glad.c
    src/triangle_mesh.cpp
    src/linear_algebra.cpp
)

target_include_directories(fluid-sim PRIVATE dependencies)

target_link_libraries(fluid-sim glfw OpenGL::GL)
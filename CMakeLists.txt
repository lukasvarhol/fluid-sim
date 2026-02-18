cmake_minimum_required(VERSION 3.5)
project(fluid-sim VERSION 1.0.0)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
add_executable(fluid-sim 
    src/config.h 
    src/main.cpp 
    src/glad.c
)

target_include_directories(fluid-sim PRIVATE dependencies)

target_link_libraries(fluid-sim glfw OpenGL::GL)
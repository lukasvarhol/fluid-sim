#pragma once

#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "raycasting_system.h"
#include "../app/input_state.h"
#include "camera_system.h"
#include "../particle_config.h"

struct MouseRay {
  Vec3 origin;
  Vec3 direction;
  float strength;
};

MouseRay MouseRaycast(InputState& inputState, CameraState& cameraState, GLFWwindow* window);

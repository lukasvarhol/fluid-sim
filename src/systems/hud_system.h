#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include "../particles.h"
#include "../app/simulation_control.h"

void DrawHUD(Particles &particles, SimulationControl& simulationControl);

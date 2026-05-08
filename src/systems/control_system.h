#pragma once

#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "../app/simulation_control.h"
#include "../app/input_state.h"
#include "../particles.h"

float HandleSimulationControl(SimulationControl &simulationControl, 
			      float dtMeasured, Particles& particles);




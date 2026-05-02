#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

#include "particle_mesh.h"
#include "linear_algebra.h"
#include "particles.h"
#include "grid.h"
#include "app/app_state.h"
#include "systems/camera_system.h"
#include "systems/render_system.h"
#include "systems/hud_system.h"
#include "systems/input_system.h"
#include "systems/control_system.h"

#include "helpers.h"

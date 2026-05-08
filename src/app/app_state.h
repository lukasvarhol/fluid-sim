#pragma once
#include "camera.h"
#include "input_state.h"
#include "simulation_control.h"
#include "viewport.h"

struct AppState {
  Camera *camera;
  InputState *inputState;
  SimulationControl *simulationControl;
  Viewport *viewport;
};

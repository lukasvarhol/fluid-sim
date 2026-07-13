#pragma once
#include "camera.h"
#include "input_state.h"
#include "simulation_control.h"
#include "viewport.h"
#include "objects3d/editor_state.h"

#ifdef USE_CUDA
#include "../cuda_buffers.cuh"
#endif

struct AppState {
  Camera            *camera;
  InputState        *inputState;
  SimulationControl *simulationControl;
  Viewport          *viewport;
  EditorState *editorState;
#ifdef USE_CUDA
  CudaBuffers* cudaBuffers;
#endif
};

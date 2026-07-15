#include "control_system.h"

float HandleSimulationControl( SimulationControl &simulationControl,
			       float dtMeasured, Particles& particles, AppState* as) {
  float dtToSim = 0.0f;
  
  if (simulationControl.isReset) {
    particles.Reset(smoothingRadius, as);
    simulationControl.isReset = false;
    simulationControl.isPaused = false;
    simulationControl.isStepping = false;
  }

  if (!simulationControl.isPaused) {
    dtToSim = dtMeasured;
  } else if (simulationControl.isStepping) {
    dtToSim = 1.0f / 60.0f;
    simulationControl.isStepping = false;
  }

  return dtToSim;
}


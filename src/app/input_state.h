#pragma once

struct InputState {
  double lastMouseX = 0.0;
  double lastMouseY = 0.0;

  bool isPushing = false;
  bool isPulling = false;
  bool isOrbiting = false;
  bool isPanning = false;
};

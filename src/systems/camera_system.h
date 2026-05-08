#pragma once

#include "../linear_algebra.h"
#include "../app/camera.h"

struct CameraState {
  Mat4 view;
  Vec3 position;
};

CameraState ComputeViewMatrix(const Camera& camera);

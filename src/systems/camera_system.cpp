#include "camera_system.h"

CameraState ComputeViewMatrix(const Camera &camera) {
  CameraState state;
  state.position.x = camera.panX +
               camera.radius * cos(camera.elevation) * sin(camera.azimuth);
  state.position.y = camera.panY + camera.radius * sin(camera.elevation);
  state.position.z = camera.radius * cos(camera.elevation) * cos(camera.azimuth);

  state.view = LookAt(Vec3{state.position.x, state.position.y, state.position.z},
                     Vec3{camera.panX, camera.panY, 0.0f}, {0, 1.0, 0});
  return state;
}

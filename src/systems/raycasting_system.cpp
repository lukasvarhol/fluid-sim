#include "raycasting_system.h"

MouseRay MouseRaycast(InputState &inputState, CameraState &cameraState,
                      GLFWwindow *window) {
  MouseRay mouseRay = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f};
  
  if ((inputState.isPushing || inputState.isPulling) && !ImGui::GetIO().WantCaptureMouse) {
    double xPos, yPos;
    glfwGetCursorPos(window, &xPos, &yPos);
    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);

    float xNDC = (float)((xPos / winW) * 2.0 - 1.0);
    float yNDC = (float)(1.0 - (yPos / winH) * 2.0);

    float fov = 45.0f * PI / 180.0f;
    float aspect = (float)winW / winH;
    float tanHalfFOV = tan(fov / 2.0f);

    Vec3 rayView = {
      xNDC * aspect * tanHalfFOV,
      yNDC * tanHalfFOV,
      -1.0f
    };
    // transform ray to world space using inverse view
    Mat4 invView = InverseView(cameraState.view);

    Vec3 rayWorld = {
      invView.entries[0] * rayView.x +
      invView.entries[4] * rayView.y +
      invView.entries[8] * rayView.z,
      invView.entries[1] * rayView.x +
      invView.entries[5] * rayView.y +
      invView.entries[9] * rayView.z,
      invView.entries[2] * rayView.x +
      invView.entries[6] * rayView.y +
      invView.entries[10] * rayView.z,
    };
    float len = rayWorld.Magnitude();
    rayWorld = rayWorld * (1.0f / len);

    Vec3 rayOrigin = cameraState.position;

    float t = -rayOrigin.z / rayWorld.z;
    float worldX = rayOrigin.x + t * rayWorld.x;
    float worldY = rayOrigin.y + t * rayWorld.y;

    mouseRay.origin = cameraState.position;
    mouseRay.direction = rayWorld;
    mouseRay.strength = inputState.isPulling ? pullStrength : pushStrength;
  }
  return mouseRay;
}

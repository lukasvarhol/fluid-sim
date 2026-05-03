#include "input_system.h"

void ScrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
  AppState *appState =
    static_cast<AppState *>(glfwGetWindowUserPointer(window));
  
  appState->camera->radius -= (float)yOffset * 0.2f;
  appState->camera->radius = std::max(0.001f, appState->camera->radius); // don't go through the scene
}

void KeyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods) {
  AppState *appState = static_cast<AppState*>(glfwGetWindowUserPointer(window));
					       
  if (action != GLFW_PRESS)
    return;

  if (key == GLFW_KEY_SPACE)
    {
      appState->simulationControl->isPaused = !appState->simulationControl->isPaused;
    }
  if (key == GLFW_KEY_RIGHT)
    {
      appState->simulationControl->isStepping = true;
    }
  if (key == GLFW_KEY_R)
    {
      appState->simulationControl->isReset = true;
    }
  if (key == GLFW_KEY_ESCAPE)
    appState->simulationControl->isShowHUD = !appState->simulationControl->isShowHUD;
}

void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
  AppState *appState =
    static_cast<AppState *>(glfwGetWindowUserPointer(window));
  
  ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
  if (ImGui::GetIO().WantCaptureMouse) return;

  if (button == GLFW_MOUSE_BUTTON_LEFT){
    if (action == GLFW_PRESS)	{
      if (mods & GLFW_MOD_CONTROL) {
	appState->inputState->isPanning = true;   
	glfwGetCursorPos(window, &appState->inputState->lastMouseX, &appState->inputState->lastMouseY);
      }
      else if (mods & GLFW_MOD_SHIFT) {
	appState->inputState->isOrbiting = true;
	glfwGetCursorPos(window,  &appState->inputState->lastMouseX, &appState->inputState->lastMouseY);
      }
      else {
	appState->inputState->isPulling = true;
      }
    }
    if (action == GLFW_RELEASE) {
      appState->inputState->isPulling  = false;
      appState->inputState->isPanning  = false;
      appState->inputState->isOrbiting = false;
    }
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
      if (action == GLFW_PRESS) appState->inputState->isPushing = true;
      if (action == GLFW_RELEASE) appState->inputState->isPushing = false;
    }
}

void CursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
  AppState *appState =
    static_cast<AppState *>(glfwGetWindowUserPointer(window));
  
  double dx = xpos - appState->inputState->lastMouseX;
  double dy = ypos -  appState->inputState->lastMouseY;
  appState->inputState->lastMouseX = xpos;
  appState->inputState->lastMouseY = ypos;

  if (appState->inputState->isOrbiting)
    {
      appState->camera->azimuth   -= (float)dx * 0.005f;
      appState->camera->elevation += (float)dy * 0.005f;
      appState->camera->elevation  = std::clamp(appState->camera->elevation, -PI/2.0f + 0.01f, PI/2.0f - 0.01f);
    }
  else if (appState->inputState->isPanning)
    {
      // right and up vectors of the camera
      float sinA = sin(appState->camera->azimuth), cosA = cos(appState->camera->azimuth);
      float sinE = sin(appState->camera->elevation), cosE = cos(appState->camera->elevation);

      // camera right vector (perpendicular to forward, in XZ plane)
      Vec3 right = {cosA, 0.0f, -sinA};
      // camera up vector
      Vec3 up = {-sinA * sinE, cosE, -cosA * sinE};

      float speed = appState->camera->radius * 0.001f;
      appState->camera->panX -= ((float)dx * right.x - (float)dy * up.x) * speed;
      appState->camera->panY -= ((float)dx * right.y - (float)dy * up.y) * speed;
    }
}

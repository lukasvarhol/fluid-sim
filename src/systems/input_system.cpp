#include "input_system.h"
#include "objects3d/object_builder.h"
#include <algorithm>
#include <string>

static const char *kFeatureNames[] = {"Empty", "Ramp", "Straight Channel",
                                      "L Channel"};
constexpr int FEATURE_COUNT = std::size(kFeatureNames);
static const char* kOrientNames[]  = { "North", "East", "South", "West" };

void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    AppState* appState = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    appState->camera->radius -= (float)yOffset * 0.2f;
    appState->camera->radius  = std::max(0.001f, appState->camera->radius);
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    AppState*    appState = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    EditorState* es       = appState->editorState;
    GridState&   grid     = es->grid;

    bool isPress  = (action == GLFW_PRESS);
    bool isRepeat = (action == GLFW_REPEAT);
    if (!isPress && !isRepeat) return;

    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    if (ImGui::GetIO().WantCaptureKeyboard && isPress) return;

    // ESC: cancel preview (if Objects Mode on) or toggle HUD — always works
    if (key == GLFW_KEY_ESCAPE && isPress) {
        if (es->objectsModeEnabled && es->previewActive) {
            CancelPreview(grid, *es);
            es->statusMsg   = "Cancelled preview.";
            es->statusTimer = 2.0f;
        } else {
            appState->simulationControl->isShowHUD = !appState->simulationControl->isShowHUD;
        }
        return;
    }

    // -----------------------------------------------------------------------
    // Object editing keys — only active when Objects Mode is enabled
    // -----------------------------------------------------------------------
    if (es->objectsModeEnabled) {

        // Cell navigation — PRESS and REPEAT (cancels active preview)
        auto doNavigate = [&](int dx, int dy, int dz) {
            if (es->previewActive) CancelPreview(grid, *es);
            NavigateCell(grid, dx, dy, dz);
        };

        if (key == GLFW_KEY_LEFT)       { doNavigate(-1,  0,  0); return; }
        if (key == GLFW_KEY_RIGHT)      { doNavigate( 1,  0,  0); return; }
        if (key == GLFW_KEY_UP)         { doNavigate( 0,  0, -1); return; }
        if (key == GLFW_KEY_DOWN)       { doNavigate( 0,  0,  1); return; }
        if (key == GLFW_KEY_T)          { doNavigate( 0,  1,  0); return; }
        if (key == GLFW_KEY_G)          { doNavigate( 0, -1,  0); return; }
        if (key == GLFW_KEY_PAGE_UP)    { doNavigate( 0,  1,  0); return; }
        if (key == GLFW_KEY_PAGE_DOWN)  { doNavigate( 0, -1,  0); return; }

        // Cell mutations — PRESS only
        if (isPress) {

            // F: cycle feature (through preview)
            if (key == GLFW_KEY_F) {
                int delta = (mods & GLFW_MOD_SHIFT) ? -1 : 1;
                if (!es->previewActive) StartPreview(grid, *es);
                int n = (((int)es->previewCell.feature + delta) % FEATURE_COUNT + FEATURE_COUNT) % FEATURE_COUNT;
                es->previewCell.feature = static_cast<Feature>(n);
                RegeneratePreviewObjects(*es);
                es->statusMsg   = std::string("Preview: ") + kFeatureNames[n]
                                  + "  [Enter]=Place  [Esc]=Cancel";
                es->statusTimer = 4.0f;
                return;
            }

            // Q: rotate orientation CW (+1)
            if (key == GLFW_KEY_Q) {
                if (!es->previewActive) StartPreview(grid, *es);
                int n = ((int)es->previewCell.facing + 1 + 4) % 4;
                es->previewCell.facing = static_cast<Orientation>(n);
                RegeneratePreviewObjects(*es);
                es->statusMsg   = std::string("Facing: ") + kOrientNames[n]
                                  + "  [Enter]=Place  [Esc]=Cancel";
                es->statusTimer = 4.0f;
                return;
            }

            // E: rotate orientation CCW (-1)
            if (key == GLFW_KEY_E) {
                if (!es->previewActive) StartPreview(grid, *es);
                int n = ((int)es->previewCell.facing - 1 + 4) % 4;
                es->previewCell.facing = static_cast<Orientation>(n);
                RegeneratePreviewObjects(*es);
                es->statusMsg   = std::string("Facing: ") + kOrientNames[n]
                                  + "  [Enter]=Place  [Esc]=Cancel";
                es->statusTimer = 4.0f;
                return;
            }

            // V: cycle variant (through preview)
            if (key == GLFW_KEY_V) {
                int delta = (mods & GLFW_MOD_SHIFT) ? -1 : 1;
                if (!es->previewActive) StartPreview(grid, *es);
                int v = std::max(0, std::min(4, es->previewCell.variant + delta));
                es->previewCell.variant = v;
                RegeneratePreviewObjects(*es);
                es->statusMsg   = "Preview: variant " + std::to_string(v)
                                  + "  [Enter]=Place  [Esc]=Cancel";
                es->statusTimer = 4.0f;
                return;
            }

            // Enter: commit preview
            if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                if (es->previewActive) {
                    int px = es->previewX, py = es->previewY, pz = es->previewZ;
                    int fn = (int)es->previewCell.feature;
                    CommitPreview(grid, *es);
                    es->statusMsg   = std::string("Placed ") + kFeatureNames[fn]
                                      + " at (" + std::to_string(px) + ","
                                      + std::to_string(py) + "," + std::to_string(pz) + ").";
                    es->statusTimer = 2.0f;
                }
                return;
            }

            // Delete / Backspace: cancel any preview, then clear the cell
            if (key == GLFW_KEY_DELETE || key == GLFW_KEY_BACKSPACE) {
                if (es->previewActive) CancelPreview(grid, *es);
                ClearCell(grid, *es);
                es->statusMsg   = "Cell cleared.";
                es->statusTimer = 2.0f;
                return;
            }

        } // end isPress
    } // end objectsModeEnabled

    // -----------------------------------------------------------------------
    // Simulation controls — always work regardless of Objects Mode
    // -----------------------------------------------------------------------
    if (key == GLFW_KEY_SPACE && isPress)
        appState->simulationControl->isPaused = !appState->simulationControl->isPaused;

    if (key == GLFW_KEY_R && isPress && (mods & GLFW_MOD_CONTROL))
        appState->simulationControl->isReset = true;
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    AppState*   appState = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    InputState* is       = appState->inputState;

    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            glfwGetCursorPos(window, &is->lastMouseX, &is->lastMouseY);
            if (mods & GLFW_MOD_CONTROL)     is->isPanning  = true;
            else if (mods & GLFW_MOD_SHIFT)  is->isOrbiting = true;
            else                             is->isPulling   = true;
        }
        if (action == GLFW_RELEASE) {
            is->isPulling  = false;
            is->isPanning  = false;
            is->isOrbiting = false;
        }
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS)   is->isPushing = true;
        if (action == GLFW_RELEASE) is->isPushing = false;
    }

    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (action == GLFW_PRESS) {
            glfwGetCursorPos(window, &is->lastMouseX, &is->lastMouseY);
            is->isMiddleMouseHeld = true;
        }
        if (action == GLFW_RELEASE)
            is->isMiddleMouseHeld = false;
    }
}

void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    AppState*   appState = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    InputState* is       = appState->inputState;

    double dx = xpos - is->lastMouseX;
    double dy = ypos - is->lastMouseY;
    is->lastMouseX = xpos;
    is->lastMouseY = ypos;

    if (ImGui::GetIO().WantCaptureMouse) return;

    if (is->isOrbiting || is->isMiddleMouseHeld) {
        appState->camera->azimuth   -= (float)dx * 0.005f;
        appState->camera->elevation += (float)dy * 0.005f;
        appState->camera->elevation  = std::clamp(appState->camera->elevation,
                                                   -PI / 2.0f + 0.01f,
                                                    PI / 2.0f - 0.01f);
    } else if (is->isPanning) {
        float sinA = sinf(appState->camera->azimuth);
        float cosA = cosf(appState->camera->azimuth);
        float sinE = sinf(appState->camera->elevation);
        float cosE = cosf(appState->camera->elevation);
        Vec3 right = {cosA, 0.0f, -sinA};
        Vec3 up    = {-sinA * sinE, cosE, -cosA * sinE};
        float speed = appState->camera->radius * 0.001f;
        appState->camera->panX -= ((float)dx * right.x - (float)dy * up.x) * speed;
        appState->camera->panY -= ((float)dx * right.y - (float)dy * up.y) * speed;
    }
}

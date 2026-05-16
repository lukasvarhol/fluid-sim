#pragma once
#include <vector>
#include <string>
#include "linear_algebra.h"
#include "grid_state.h"

enum class RGPortKind { Input, Output, Bidirectional };

struct RGPort {
    const char* name;
    RGPortKind  kind;
    Vec3        localPos;
    Vec3        localDir;
};

enum class RGObjectType {
  BOX,
  L_CHANNEL,
  S_CHANNEL,
  B_RAMP,
  T_RAMP
};

struct RGObject {
    RGObjectType type          = RGObjectType::BOX;
    std::string  name          = "Object";
    Vec3         position      = {0.0f, 0.0f, 0.0f};
    Vec3         rotation      = {0.0f, 0.0f, 0.0f}; // Euler radians: pitch(X), yaw(Y), roll(Z)
    Vec3         halfExtents   = {0.1f, 0.1f, 0.1f};
    float        radius        = 0.1f;
    float        wallThickness = 0.025f;

    bool         active        = true;

// Per-object color override for cell-generated objects (x<0 = use type default)
    Vec3         colorOverride = {-1.0f, -1.0f, -1.0f};
};

struct EditorState {
    std::vector<RGObject> objects;        // rebuilt from committed grid
    std::vector<RGObject> previewObjects; // ghost render only, no collision
    GridState             grid;

    // Preview state
    bool        previewActive = false;
    int         previewX = 2, previewY = 2, previewZ = 2;
    CellFeature previewCell;

    // Status message (transient HUD toast)
    std::string statusMsg;
    float       statusTimer = 0.0f;

    // Grid visibility
    bool showGrid             = true;
    bool showSelectedCell     = true;
    bool showOccupiedOutlines = true;

    bool resetObjectsOnR = false;

    // Objects Mode: must be enabled via HUD checkbox before editing keys work
    bool objectsModeEnabled = false;
};

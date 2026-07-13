#pragma once
#include "objects3d/editor_state.h"
#include "sdf_collision.h"

float orientationYaw(Orientation o);

void regenerateObjectFromGrid(const GridState& grid, EditorState& state);

// Preview workflow
void startPreview(GridState& grid, EditorState& state);   // snapshot current cell, exclude from solid objects
void addPreviewObject(EditorState& state, size_t cellIdx);         // rebuild previewObjects from previewCell
void commitPreview(GridState& grid, EditorState& state, AppState* as);   // write previewCell to grid, rebuild objects
void cancelPreview(GridState& grid, EditorState& state);   // discard preview, rebuild objects

// Scene management
void loadDefaultScene(EditorState& state, AppState* as);
void clearScene(EditorState& state, AppState* as);

// Cell navigation (no rebuild)
void navigateCell(GridState& grid, int dx, int dy, int dz);

// Direct-commit mutations (no preview — used by Delete/ClearScene internally)
void cycleCellFeature(GridState& grid, EditorState& state, int delta);
void rotateCellOrientation(GridState& grid, EditorState& state, int delta);
void cycleCellVariant(GridState& grid, EditorState& state, int delta);
void clearCell(GridState& grid, EditorState& state, AppState* as);



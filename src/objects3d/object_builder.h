#pragma once
#include "objects3d/editor_state.h"

float OrientationYaw(Orientation o);

// Rebuild editorState.objects from the grid, skipping cell (skipX,skipY,skipZ).
// All three must be >= 0 to activate the skip; default (-1,-1,-1) = no skip.
void RegenerateObjectsFromGrid(const GridState& grid, EditorState& state,
                               int skipX = -1, int skipY = -1, int skipZ = -1);

// Preview workflow
void StartPreview(GridState& grid, EditorState& state);   // snapshot current cell, exclude from solid objects
void RegeneratePreviewObjects(EditorState& state);         // rebuild previewObjects from previewCell
void CommitPreview(GridState& grid, EditorState& state);   // write previewCell to grid, rebuild objects
void CancelPreview(GridState& grid, EditorState& state);   // discard preview, rebuild objects

// Scene management
void LoadDefaultScene(EditorState& state);
void ClearScene(EditorState& state);

// Cell navigation (no rebuild)
void NavigateCell(GridState& grid, int dx, int dy, int dz);

// Direct-commit mutations (no preview — used by Delete/ClearScene internally)
void CycleCellFeature(GridState& grid, EditorState& state, int delta);
void RotateCellOrientation(GridState& grid, EditorState& state, int delta);
void CycleCellVariant(GridState& grid, EditorState& state, int delta);
void ClearCell(GridState& grid, EditorState& state);

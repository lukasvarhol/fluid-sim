#include "object_builder.h"
#include "particle_config.h"
#include <cmath>
#include <algorithm>

struct ScenePlacement {
  int x, y, z;
  CellFeature feature;
};

float orientationYaw(Orientation o)
{
  switch (o) {
  case Orientation::North: return 0.0f;
  case Orientation::East:  return  PI / 2.0f;
  case Orientation::South: return  PI;
  case Orientation::West:  return  3.0f * PI / 2.0f;
  }
  return 0.0f;
}

static void addCellObject(RGObject* objects,
                              const CellFeature& cf,
			  Vec3 c, float yaw, size_t idx)
{
  RGObjectType type;
  switch (cf.feature) {
  case Feature::L_CHANNEL:  type = RGObjectType::L_CHANNEL; break;
  case Feature::S_CHANNEL:
    type = RGObjectType::S_CHANNEL;
    break;
  case Feature::RAMP: type = RGObjectType::RAMP; break;
  case Feature::EMPTY:     return;
  default:                 return; // unimplemented features ignored for now
  }

  RGObject obj;
  obj.type     = type;
  obj.active   = true;
  obj.position = c;
  obj.rotation = {0.0f, yaw, 0.0f};
  obj.halfExtents = {CELL_SIZE * 0.5f, CELL_SIZE * 0.5f, CELL_SIZE * 0.5f};
  objects[idx] = obj;
}

void regenerateObjectFromGrid(const GridState& grid, EditorState& state)
{
  const CellFeature &cf = grid.GetCell(grid.selX, grid.selY, grid.selZ);
  size_t cellIdx = grid.CellIndex(grid.selX, grid.selY, grid.selZ);
  if (cf.feature == Feature::EMPTY) {
    state.objects[cellIdx] = RGObject{};
    return;
  }
  Vec3  c   = grid.CellCenterWorld(grid.selX, grid.selY, grid.selZ);
  float yaw = orientationYaw(cf.facing);
  addCellObject(state.objects, cf, c, yaw, cellIdx);
}

void startPreview(GridState& grid, EditorState& state)
{
  state.previewX    = grid.selX;
  state.previewY    = grid.selY;
  state.previewZ    = grid.selZ;
  state.previewCell = grid.GetCell(grid.selX, grid.selY, grid.selZ);
  state.previewActive = true;
  // Exclude the preview cell from solid collision objects
  regenerateObjectFromGrid(grid, state);
  // Build initial ghost from current cell content (may be empty)
  addPreviewObject(state, grid.CellIndex(grid.selX, grid.selY, grid.selZ));
}

void addPreviewObject(EditorState& state, size_t cellIdx)
{
  state.previewObjects[cellIdx] = RGObject{};
  state.objects[cellIdx] = RGObject{};
  if (!state.previewActive) return;
  Vec3  c   = state.grid.CellCenterWorld(state.previewX, state.previewY, state.previewZ);
  float yaw = orientationYaw(state.previewCell.facing);
  addCellObject(state.previewObjects, state.previewCell, c, yaw, cellIdx);
}


void commitPreview(GridState& grid, EditorState& state, AppState* as)
{
  grid.GetCell(state.previewX, state.previewY, state.previewZ) =
      state.previewCell;
  size_t cellIdx = grid.CellIndex(state.previewX, state.previewY, state.previewZ); 
  state.previewActive = false;
  state.previewObjects[cellIdx] = RGObject{};
  regenerateObjectFromGrid(grid,state); 
  addCollider(state.colliders, state.objects, cellIdx, as);
}

void cancelPreview(GridState& grid, EditorState& state)
{
  state.previewActive = false;
  state.previewObjects[grid.CellIndex(state.previewX, state.previewY, state.previewZ)] = RGObject{};
  regenerateObjectFromGrid(grid, state); // restore old cell's geometry
}

void clearScene(EditorState& state, AppState* as)
{
  for (size_t j{}; j < MAX_OBJECTS; ++j) {
    state.objects[j] = RGObject{};
    deleteCollider(state.colliders, j, as);
  }
}

void buildFromList(std::vector<ScenePlacement> &placeList, GridState& grid, EditorState &state, AppState* as) {
  for (const auto &e : placeList) {
    const CellFeature &cf = e.feature;
    size_t cellIdx = grid.CellIndex(e.x, e.y, e.z);
    if (cf.feature == Feature::EMPTY) continue;
    Vec3  c   = grid.CellCenterWorld(e.x, e.y, e.z);
    float yaw = orientationYaw(cf.facing);
    addCellObject(state.objects, cf, c, yaw, cellIdx);
    addCollider(state.colliders, state.objects, cellIdx, as);
  }
}

void loadDefaultScene(EditorState& state, AppState* as)
{
  clearScene(state, as);

  tricklerMode      = true;
  tricklerOriginX   = -0.8f;
  tricklerOriginY   =  1.0f;
  tricklerOriginZ   =  -0.8f;
  tricklerSpawnRate = 300.0f;

  std::vector<ScenePlacement> placeList;

  placeList.push_back({0, 3, 0, { Feature::RAMP,      Orientation::West,  0 }});
  placeList.push_back({1, 3, 0, { Feature::S_CHANNEL, Orientation::East,  0 }});
  placeList.push_back({2, 3, 0, { Feature::S_CHANNEL, Orientation::East,  0 }});
  placeList.push_back({3, 3, 0, { Feature::L_CHANNEL, Orientation::North, 0 }});

  placeList.push_back({4, 2, 2, { Feature::L_CHANNEL, Orientation::West,  0 }});
  placeList.push_back({3, 2, 2, { Feature::S_CHANNEL, Orientation::West,  0 }});
  placeList.push_back({2, 2, 2, { Feature::S_CHANNEL, Orientation::West,  0 }});
  placeList.push_back({1, 2, 2, { Feature::S_CHANNEL, Orientation::West,  0 }});
  placeList.push_back({0, 2, 2, { Feature::L_CHANNEL, Orientation::East, 0 }});

  placeList.push_back({0, 1, 3, { Feature::RAMP,      Orientation::South, 0 }});

  placeList.push_back({0, 1, 4, { Feature::L_CHANNEL, Orientation::South,  0 }});
  placeList.push_back({1, 1, 4, { Feature::S_CHANNEL, Orientation::East,  0 }});
  placeList.push_back({2, 1, 4, { Feature::S_CHANNEL, Orientation::East,  0 }});
  placeList.push_back({3, 1, 4, { Feature::S_CHANNEL, Orientation::East,  0 }});
  placeList.push_back({4, 1, 4, { Feature::L_CHANNEL, Orientation::West, 0 }});

  placeList.push_back({4, 0, 3, {Feature::RAMP, Orientation::North, 0}});

  placeList.push_back({4, 1, 1, {Feature::RAMP, Orientation::North, 0}});
  placeList.push_back({4, 1, 0, {Feature::L_CHANNEL, Orientation::North, 0}});

  buildFromList(placeList, state.grid,state, as);
}

// ---------------------------------------------------------------------------
// Grid navigation and mutation
// ---------------------------------------------------------------------------

void navigateCell(GridState& grid, int dx, int dy, int dz)
{
  grid.selX = std::max(0, std::min(GRID_X - 1, grid.selX + dx));
  grid.selY = std::max(0, std::min(GRID_Y - 1, grid.selY + dy));
  grid.selZ = std::max(0, std::min(GRID_Z - 1, grid.selZ + dz));
}

void cycleCellFeature(GridState& grid, EditorState& state, int delta)
{
  CellFeature& cf = grid.GetCell(grid.selX, grid.selY, grid.selZ);
  int n = (((int)cf.feature + delta) % NUM_FEATURES + NUM_FEATURES) % NUM_FEATURES;
  cf.feature = static_cast<Feature>(n);
  regenerateObjectFromGrid(grid, state);
}

void rotateCellOrientation(GridState& grid, EditorState& state, int delta)
{
  CellFeature& cf = grid.GetCell(grid.selX, grid.selY, grid.selZ);
  int n = (static_cast<int>(cf.facing) + delta + 4) % 4;
  cf.facing = static_cast<Orientation>(n);
  regenerateObjectFromGrid(grid, state);
}

void cycleCellVariant(GridState& grid, EditorState& state, int delta)
{
  CellFeature& cf = grid.GetCell(grid.selX, grid.selY, grid.selZ);
  cf.variant = std::max(0, std::min(4, cf.variant + delta));
  regenerateObjectFromGrid(grid, state);
}

void clearCell(GridState &grid, EditorState &state, AppState* as) {
  size_t cellIdx = grid.CellIndex(grid.selX, grid.selY, grid.selZ);
  state.objects[cellIdx] = RGObject{};
  deleteCollider(state.colliders, cellIdx, as);
}

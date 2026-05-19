#include "object_builder.h"
#include "particle_config.h"
#include <cmath>
#include <algorithm>


float OrientationYaw(Orientation o)
{
  switch (o) {
  case Orientation::North: return 0.0f;
  case Orientation::East:  return  PI / 2.0f;
  case Orientation::South: return  PI;
  case Orientation::West:  return  3.0f * PI / 2.0f;
  }
  return 0.0f;
}

static void AppendCellObjects(std::vector<RGObject>& objects,
                              const CellFeature& cf,
                              Vec3 c, float yaw)
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
  objects.push_back(obj);
}

void RegenerateObjectsFromGrid(const GridState& grid, EditorState& state,
                               int skipX, int skipY, int skipZ)
{
  state.objects.clear();
  for (int z = 0; z < GRID_Z; ++z)
    for (int y = 0; y < GRID_Y; ++y)
      for (int x = 0; x < GRID_X; ++x) {
        if (skipX >= 0 && x == skipX && y == skipY && z == skipZ) continue;
        const CellFeature& cf = grid.GetCell(x, y, z);
        if (cf.feature == Feature::EMPTY) continue;
        Vec3  c   = grid.CellCenterWorld(x, y, z);
        float yaw = OrientationYaw(cf.facing);
        AppendCellObjects(state.objects, cf, c, yaw);
      }
}

void StartPreview(GridState& grid, EditorState& state)
{
  state.previewX    = grid.selX;
  state.previewY    = grid.selY;
  state.previewZ    = grid.selZ;
  state.previewCell = grid.GetCell(grid.selX, grid.selY, grid.selZ);
  state.previewActive = true;
  // Exclude the preview cell from solid collision objects
  RegenerateObjectsFromGrid(grid, state, state.previewX, state.previewY, state.previewZ);
  // Build initial ghost from current cell content (may be empty)
  RegeneratePreviewObjects(state);
}

void RegeneratePreviewObjects(EditorState& state)
{
  state.previewObjects.clear();
  if (!state.previewActive) return;
  Vec3  c   = state.grid.CellCenterWorld(state.previewX, state.previewY, state.previewZ);
  float yaw = OrientationYaw(state.previewCell.facing);
  AppendCellObjects(state.previewObjects, state.previewCell, c, yaw);
}

void CommitPreview(GridState& grid, EditorState& state)
{
  grid.GetCell(state.previewX, state.previewY, state.previewZ) = state.previewCell;
  state.previewActive = false;
  state.previewObjects.clear();
  RegenerateObjectsFromGrid(grid, state); // no skip — include the now-committed cell
}

void CancelPreview(GridState& grid, EditorState& state)
{
  state.previewActive = false;
  state.previewObjects.clear();
  RegenerateObjectsFromGrid(grid, state); // restore old cell's geometry
}

void ClearScene(EditorState& state)
{
  state.grid.layout.fill(CellFeature{});
  state.grid.selX = 2;
  state.grid.selY = 2;
  state.grid.selZ = 2;
  state.objects.clear();
  state.previewActive = false;
  state.previewObjects.clear();
}

void LoadDefaultScene(EditorState& state)
{
  ClearScene(state);

  tricklerMode      = true;
  tricklerOriginX   = -0.8f;
  tricklerOriginY   =  1.0f;
  tricklerOriginZ   =  -0.8f;
  tricklerSpawnRate = 300.0f;

  GridState &g = state.grid;

  g.GetCell(0, 3, 0) = { Feature::RAMP,      Orientation::West,  0 };
  g.GetCell(1, 3, 0) = { Feature::S_CHANNEL, Orientation::East,  0 };
  g.GetCell(2, 3, 0) = { Feature::S_CHANNEL, Orientation::East,  0 };
  g.GetCell(3, 3, 0) = { Feature::L_CHANNEL, Orientation::North, 0 };

  //g.GetCell(4, 2, 1) = { Feature::RAMP,      Orientation::South, 0 };

  g.GetCell(4, 2, 2) = { Feature::L_CHANNEL, Orientation::West,  0 };
  g.GetCell(3, 2, 2) = { Feature::S_CHANNEL, Orientation::West,  0 };
  g.GetCell(2, 2, 2) = { Feature::S_CHANNEL, Orientation::West,  0 };
  g.GetCell(1, 2, 2) = { Feature::S_CHANNEL, Orientation::West,  0 };
  g.GetCell(0, 2, 2) = { Feature::L_CHANNEL, Orientation::East, 0 };

  g.GetCell(0, 1, 3) = { Feature::RAMP,      Orientation::South, 0 };

  g.GetCell(0, 1, 4) = { Feature::L_CHANNEL, Orientation::South,  0 };
  g.GetCell(1, 1, 4) = { Feature::S_CHANNEL, Orientation::East,  0 };
  g.GetCell(2, 1, 4) = { Feature::S_CHANNEL, Orientation::East,  0 };
  g.GetCell(3, 1, 4) = { Feature::S_CHANNEL, Orientation::East,  0 };
  g.GetCell(4, 1, 4) = { Feature::L_CHANNEL, Orientation::West, 0 };

  g.GetCell(4, 0, 3) = {Feature::RAMP, Orientation::North, 0};

  g.GetCell(4, 1, 1) = {Feature::RAMP, Orientation::North, 0};
  g.GetCell(4, 1, 0) = {Feature::L_CHANNEL, Orientation::North, 0};

  RegenerateObjectsFromGrid(g, state);
}

// ---------------------------------------------------------------------------
// Grid navigation and mutation
// ---------------------------------------------------------------------------

void NavigateCell(GridState& grid, int dx, int dy, int dz)
{
  grid.selX = std::max(0, std::min(GRID_X - 1, grid.selX + dx));
  grid.selY = std::max(0, std::min(GRID_Y - 1, grid.selY + dy));
  grid.selZ = std::max(0, std::min(GRID_Z - 1, grid.selZ + dz));
}

void CycleCellFeature(GridState& grid, EditorState& state, int delta)
{
  CellFeature& cf = grid.GetCell(grid.selX, grid.selY, grid.selZ);
  int n = (((int)cf.feature + delta) % NUM_FEATURES + NUM_FEATURES) % NUM_FEATURES;
  cf.feature = static_cast<Feature>(n);
  RegenerateObjectsFromGrid(grid, state);
}

void RotateCellOrientation(GridState& grid, EditorState& state, int delta)
{
  CellFeature& cf = grid.GetCell(grid.selX, grid.selY, grid.selZ);
  int n = (static_cast<int>(cf.facing) + delta + 4) % 4;
  cf.facing = static_cast<Orientation>(n);
  RegenerateObjectsFromGrid(grid, state);
}

void CycleCellVariant(GridState& grid, EditorState& state, int delta)
{
  CellFeature& cf = grid.GetCell(grid.selX, grid.selY, grid.selZ);
  cf.variant = std::max(0, std::min(4, cf.variant + delta));
  RegenerateObjectsFromGrid(grid, state);
}

void ClearCell(GridState& grid, EditorState& state)
{
  grid.GetCell(grid.selX, grid.selY, grid.selZ) = CellFeature{};
  RegenerateObjectsFromGrid(grid, state);
}

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

// Rotate a local offset by yaw around the Y axis
static Vec3 RotateOffsetByYaw(Vec3 offset, float yaw)
{
    Mat4 m = CreateMatrixRotationY(yaw);
    return TransformDir(m, offset);
}

// ---------------------------------------------------------------------------
// Feature geometry — all dimensions in world units, CELL_SIZE = 0.4
// R = 0.18 (inner reach, leaves 0.02 margin to cell wall)
// t = 0.02 (wall half-thickness)
// ---------------------------------------------------------------------------

static const Vec3 COLOR_CHANNEL   = {0.20f, 0.75f, 0.25f}; // green
static const Vec3 COLOR_TUBE      = {0.10f, 0.80f, 0.80f}; // cyan
static const Vec3 COLOR_CONTAINER = {0.20f, 0.45f, 0.95f}; // blue
static const Vec3 COLOR_FUNNEL    = {0.75f, 0.25f, 0.90f}; // purple
static const Vec3 COLOR_FLAT      = {0.85f, 0.75f, 0.30f}; // yellow/brown
static const Vec3 COLOR_LCHANNEL  = {0.10f, 0.65f, 0.20f}; // slightly different green

static void AppendCellObjects(std::vector<RGObject>& objects,
                              const CellFeature& cf,
                              Vec3 c,     // cell center in world space
                              float yaw)
{
    constexpr float R = 0.18f;
    constexpr float t = 0.02f;

    // Helper: add one solid BOX child at (c + rotated localOffset)
    auto addBox = [&](Vec3 localOffset, Vec3 halfExtents, Vec3 color) {
        RGObject obj;
        obj.type          = RGObjectType::BOX;
        obj.active        = true;
        obj.wallThickness = t;
        obj.colorOverride = color;
        obj.position      = c + RotateOffsetByYaw(localOffset, yaw);
        obj.rotation      = {0.0f, yaw, 0.0f};
        obj.halfExtents   = halfExtents;
        objects.push_back(obj);
    };

    // Helper: add one solid BOX with per-wall extra rotation (for angled funnel walls)
    // rotXYZ.y is stacked on top of yaw so the wall faces correctly in world space
    auto addBoxR = [&](Vec3 localOffset, Vec3 halfExtents, Vec3 rotXYZ, Vec3 color) {
        RGObject obj;
        obj.type          = RGObjectType::BOX;
        obj.active        = true;
        obj.wallThickness = t;
        obj.colorOverride = color;
        obj.position      = c + RotateOffsetByYaw(localOffset, yaw);
        obj.rotation      = {rotXYZ.x, rotXYZ.y + yaw, rotXYZ.z};
        obj.halfExtents   = halfExtents;
        objects.push_back(obj);
    };

    switch (cf.feature) {
    case Feature::Empty:
        break;

    case Feature::Channel:
        // U-trough: closed bottom, closed sides (±X in local), open top and flow ends (±Z)
        addBox({0.0f, -R, 0.0f}, {R, t, R}, COLOR_CHANNEL); // floor
        addBox({-R,  0.0f, 0.0f}, {t, R, R}, COLOR_CHANNEL); // left wall
        addBox({ R,  0.0f, 0.0f}, {t, R, R}, COLOR_CHANNEL); // right wall
        break;

    case Feature::Tube:
        // Vertical tube: 4 side walls only, open top and bottom
        addBox({-R,  0.0f, 0.0f}, {t, R, R}, COLOR_TUBE); // left wall  (-X)
        addBox({ R,  0.0f, 0.0f}, {t, R, R}, COLOR_TUBE); // right wall (+X)
        addBox({0.0f, 0.0f, -R}, {R, R, t}, COLOR_TUBE);  // front wall (-Z)
        addBox({0.0f, 0.0f,  R}, {R, R, t}, COLOR_TUBE);  // back wall  (+Z)
        break;

    case Feature::Container:
        // Basin: closed bottom + all 4 side walls, open top
        addBox({0.0f, -R, 0.0f}, {R, t, R}, COLOR_CONTAINER); // floor
        addBox({-R,  0.0f, 0.0f}, {t, R, R}, COLOR_CONTAINER); // -X wall
        addBox({ R,  0.0f, 0.0f}, {t, R, R}, COLOR_CONTAINER); // +X wall
        addBox({0.0f, 0.0f, -R}, {R, R, t}, COLOR_CONTAINER);  // -Z wall
        addBox({0.0f, 0.0f,  R}, {R, R, t}, COLOR_CONTAINER);  // +Z wall
        break;

    case Feature::Funnel: {
        // V-funnel: 2 angled walls (tilted around Z) + 2 flat front/back containment walls
        // Mouth at ±R (wide), outlet at ±0.06 (narrow) at the bottom.
        // tilt angle = atan2(R - outlet_half, 2*R) = atan2(0.12, 0.36) ≈ 18.4°
        constexpr float FUNNEL_ANGLE = 0.321f; // radians
        constexpr float PANEL_HALF_LEN = 0.190f; // half-length along slope

        addBoxR({-0.12f, 0.0f, 0.0f}, {t, PANEL_HALF_LEN, R}, {0.0f, 0.0f, +FUNNEL_ANGLE}, COLOR_FUNNEL);
        addBoxR({ 0.12f, 0.0f, 0.0f}, {t, PANEL_HALF_LEN, R}, {0.0f, 0.0f, -FUNNEL_ANGLE}, COLOR_FUNNEL);
        addBox({0.0f, 0.0f, -R}, {R, R, t}, COLOR_FUNNEL); // front flat wall
        addBox({0.0f, 0.0f,  R}, {R, R, t}, COLOR_FUNNEL); // back flat wall
        break;
    }

    case Feature::Ramp: {
        // Sloped slab. Variant selects the height band within the cell.
        //   rampStep = CELL_SIZE / 5 = 0.08
        //   centerY  = 0.14 - variant * rampStep
        //   pitch    = atan2(rampStep, 2*R) ≈ 12.5°  (drops one step across one cell width)
        //
        // Continuous path across 5 adjacent same-Y cells:
        //   output end of variant v ≈ input end of variant v+1 (within ~0.002 world units)
        constexpr float rampStep = CELL_SIZE / 5.0f; // 0.08
        float centerY = 0.14f - cf.variant * rampStep; // 0.14, 0.06, -0.02, -0.10, -0.18
        float pitch   = std::atan2f(rampStep, 2.0f * R); // ~0.2187 rad (~12.5°)

        RGObject obj;
        obj.type          = RGObjectType::RAMP;
        obj.active        = true;
        obj.wallThickness = t;
        obj.position      = {c.x, c.y + centerY, c.z};
        obj.rotation      = {pitch, yaw, 0.0f};
        obj.halfExtents   = {R, t, R};
        // colorOverride stays negative: RAMP gets its natural orange color
        objects.push_back(obj);
        break;
    }

    case Feature::FlatSurface:
        // Simple flat platform slab at cell center height
        addBox({0.0f, 0.0f, 0.0f}, {R, t, R}, COLOR_FLAT);
        break;

    case Feature::LChannel: {
        // Open-top 90-degree elbow trough.
        // Arm-1 runs along +X (open at x=+R); arm-2 runs along -Z (open at z=-R).
        // Corner region at local origin. CW = half-channel-width.
        constexpr float CW = R * 0.5f; // 0.09

        // Floor: two slabs that naturally overlap at the corner (no gap)
        addBox({ (R-CW)*0.5f, -R,        0.0f},  {(R+CW)*0.5f, t,        CW},  COLOR_LCHANNEL); // arm-1 floor (x: -CW→+R, z: -CW→+CW)
        addBox({       0.0f,  -R, -(R*0.5f)},     {       CW,   t, R*0.5f},     COLOR_LCHANNEL); // arm-2 floor (x: -CW→+CW, z: -R→0)

        // Outer walls
        addBox({ (R-CW)*0.5f,  0.0f,        CW},  {(R+CW)*0.5f, R, t},          COLOR_LCHANNEL); // arm-1 outer (z=+CW)
        addBox({      -CW,     0.0f, -(R-CW)*0.5f},{       t,    R, (R+CW)*0.5f},COLOR_LCHANNEL); // arm-2 outer (x=-CW)

        // Inner walls — each covers only its own arm's non-corner stretch to seal the dead zone
        addBox({ (R+CW)*0.5f,  0.0f,       -CW},  {(R-CW)*0.5f, R, t},          COLOR_LCHANNEL); // arm-1 inner (z=-CW, x: +CW→+R)
        addBox({       CW,     0.0f, -(R+CW)*0.5f},{       t,    R, (R-CW)*0.5f},COLOR_LCHANNEL); // arm-2 inner (x=+CW, z: -R→-CW)
        break;
    }
    }
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
        if (cf.feature == Feature::Empty) continue;
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
    tricklerOriginY   =  0.8f;
    tricklerOriginZ   =  0.0f;
    tricklerSpawnRate = 30.0f;

    GridState& g = state.grid;

    // Five ramps in a horizontal row at Y=3, Z=2, flowing East (+X).
    // Each successive variant lowers the ramp board by rampStep=0.08 within the cell,
    // forming a continuous descending path.
    g.GetCell(0, 3, 2) = { Feature::Ramp,      Orientation::East,  0 };
    g.GetCell(1, 3, 2) = { Feature::Ramp,      Orientation::East,  1 };
    g.GetCell(2, 3, 2) = { Feature::Ramp,      Orientation::East,  2 };
    g.GetCell(3, 3, 2) = { Feature::Ramp,      Orientation::East,  3 };
    g.GetCell(4, 3, 2) = { Feature::Ramp,      Orientation::East,  4 };
    // Channel below the last ramp catches the descending fluid
    g.GetCell(4, 2, 2) = { Feature::Channel,   Orientation::East,  0 };
    // Funnel collects pooled fluid and guides it downward
    g.GetCell(4, 1, 2) = { Feature::Funnel,    Orientation::North, 0 };
    // Container at the base collects everything
    g.GetCell(4, 0, 2) = { Feature::Container, Orientation::North, 0 };

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

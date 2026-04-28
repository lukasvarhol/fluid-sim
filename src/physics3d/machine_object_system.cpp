#include "physics3d/machine_object_system.h"
#include "imgui.h"
#include <cmath>
#include <cstdio>

namespace physics3d {

// ---------------------------------------------------------------------------
static const char* typeName(MachineObjectType t)
{
    switch (t) {
    case MachineObjectType::OpenContainer: return "Open Container";
    case MachineObjectType::Channel:       return "Channel";
    case MachineObjectType::Ramp:          return "Ramp";
    case MachineObjectType::BoxBlock:      return "Box Block";
    case MachineObjectType::SphereBlock:   return "Sphere Block";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
void MachineObjectSystem::addObject(MachineObjectType type,
                                     const std::string& name,
                                     const Vec3& position,
                                     const Vec3& halfSize,
                                     float angle)
{
    MachineObject obj;
    obj.type     = type;
    obj.name     = name;
    obj.position = position;
    obj.halfSize = halfSize;
    obj.angle    = angle;
    objects.push_back(obj);
    selectedIndex = (int)objects.size() - 1;
}

void MachineObjectSystem::removeSelected()
{
    if (selectedIndex < 0 || selectedIndex >= (int)objects.size())
        return;
    objects.erase(objects.begin() + selectedIndex);
    if (selectedIndex >= (int)objects.size())
        selectedIndex = (int)objects.size() - 1;
}

// ---------------------------------------------------------------------------
// Convert one high-level object into low-level obstacle primitives.
//
// OpenContainer — 5 boxes (bottom + 4 walls, open top):
//   Left/right walls extend the full outer Z to seal the corners.
//   Front/back walls span the inner X range only (corners covered by side walls).
//
// Channel — 3 boxes (bottom + left + right walls, open top and both Z ends).
//
// Ramp — 1 PlaneObstacle. The plane is infinite; halfSize only affects wireframe.
//
// BoxBlock — 1 BoxObstacle.
// SphereBlock — 1 SphereObstacle (radius = halfSize.x).
// ---------------------------------------------------------------------------
void MachineObjectSystem::convertObject(const MachineObject& obj,
                                         ObstacleSystem& obs) const
{
    const float wt = obj.wallThickness;
    const Vec3& p  = obj.position;   // center of inner volume
    const Vec3& hs = obj.halfSize;   // inner half-extents

    switch (obj.type) {

    case MachineObjectType::OpenContainer: {
        // Bottom slab — covers full outer footprint
        obs.boxes.push_back({
            {p.x,                       p.y - hs.y - wt * 0.5f, p.z},
            {hs.x + wt,                 wt * 0.5f,               hs.z + wt},
            obj.restitution, obj.friction
        });
        // Left wall (-X) — spans full outer Z so corners are sealed
        obs.boxes.push_back({
            {p.x - hs.x - wt * 0.5f,   p.y,  p.z},
            {wt * 0.5f,                 hs.y, hs.z + wt},
            obj.restitution, obj.friction
        });
        // Right wall (+X)
        obs.boxes.push_back({
            {p.x + hs.x + wt * 0.5f,   p.y,  p.z},
            {wt * 0.5f,                 hs.y, hs.z + wt},
            obj.restitution, obj.friction
        });
        // Front wall (-Z) — inner X width only; corners covered by side walls
        obs.boxes.push_back({
            {p.x,  p.y,                 p.z - hs.z - wt * 0.5f},
            {hs.x, hs.y,                wt * 0.5f},
            obj.restitution, obj.friction
        });
        // Back wall (+Z)
        obs.boxes.push_back({
            {p.x,  p.y,                 p.z + hs.z + wt * 0.5f},
            {hs.x, hs.y,                wt * 0.5f},
            obj.restitution, obj.friction
        });
        break;
    }

    case MachineObjectType::Channel: {
        // Bottom slab
        obs.boxes.push_back({
            {p.x,                       p.y - hs.y - wt * 0.5f, p.z},
            {hs.x + wt,                 wt * 0.5f,               hs.z + wt},
            obj.restitution, obj.friction
        });
        // Left wall (-X)
        obs.boxes.push_back({
            {p.x - hs.x - wt * 0.5f,   p.y,  p.z},
            {wt * 0.5f,                 hs.y, hs.z + wt},
            obj.restitution, obj.friction
        });
        // Right wall (+X)
        obs.boxes.push_back({
            {p.x + hs.x + wt * 0.5f,   p.y,  p.z},
            {wt * 0.5f,                 hs.y, hs.z + wt},
            obj.restitution, obj.friction
        });
        // No front/back walls — open at both Z ends
        break;
    }

    case MachineObjectType::Ramp: {
        float rad = obj.angle * 3.14159265f / 180.0f;
        Vec3 normal = normalize(Vec3{-std::sin(rad), std::cos(rad), 0.f});
        obs.planes.push_back({p, normal, obj.restitution, obj.friction});
        break;
    }

    case MachineObjectType::BoxBlock:
        obs.boxes.push_back({p, hs, obj.restitution, obj.friction});
        break;

    case MachineObjectType::SphereBlock:
        obs.spheres.push_back({p, hs.x, obj.restitution, obj.friction});
        break;
    }
}

// ---------------------------------------------------------------------------
void MachineObjectSystem::rebuildObstacleSystem(ObstacleSystem& obs) const
{
    obs.planes.clear();
    obs.spheres.clear();
    obs.boxes.clear();
    for (const auto& obj : objects)
        convertObject(obj, obs);
}

// ---------------------------------------------------------------------------
bool MachineObjectSystem::renderImGui(ObstacleSystem& obs)
{
    bool rebuilt = false;

    if (!ImGui::CollapsingHeader("Machine Objects"))
        return false;

    // --- Add buttons --------------------------------------------------------
    bool     addNew  = false;
    MachineObjectType newType = MachineObjectType::BoxBlock;

    if (ImGui::Button("+ Container")) { addNew = true; newType = MachineObjectType::OpenContainer; }
    ImGui::SameLine();
    if (ImGui::Button("+ Channel"))   { addNew = true; newType = MachineObjectType::Channel; }
    ImGui::SameLine();
    if (ImGui::Button("+ Ramp"))      { addNew = true; newType = MachineObjectType::Ramp; }
    ImGui::SameLine();
    if (ImGui::Button("+ Box"))       { addNew = true; newType = MachineObjectType::BoxBlock; }
    ImGui::SameLine();
    if (ImGui::Button("+ Sphere"))    { addNew = true; newType = MachineObjectType::SphereBlock; }

    if (addNew) {
        Vec3  hs  = {0.15f, 0.15f, 0.15f};
        float ang = 0.0f;
        switch (newType) {
        case MachineObjectType::OpenContainer: hs = {0.2f,  0.15f, 0.15f}; break;
        case MachineObjectType::Channel:       hs = {0.1f,  0.1f,  0.3f};  break;
        case MachineObjectType::Ramp:          hs = {0.3f,  0.01f, 0.25f}; ang = 25.0f; break;
        case MachineObjectType::BoxBlock:      hs = {0.15f, 0.15f, 0.15f}; break;
        case MachineObjectType::SphereBlock:   hs = {0.2f,  0.2f,  0.2f};  break;
        }
        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s %d",
                      typeName(newType), (int)objects.size() + 1);
        addObject(newType, nameBuf, {0.f, 0.f, 0.f}, hs, ang);
        rebuildObstacleSystem(obs);
        rebuilt = true;
    }

    // --- Object list --------------------------------------------------------
    if (objects.empty()) {
        ImGui::TextDisabled("No objects. Use buttons above to add one.");
        return rebuilt;
    }

    if (selectedIndex < 0 || selectedIndex >= (int)objects.size())
        selectedIndex = 0;

    // Build pointer array for Combo (pointers stay valid within the frame)
    std::vector<const char*> names;
    names.reserve(objects.size());
    for (const auto& o : objects)
        names.push_back(o.name.c_str());

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##objlist", &selectedIndex, names.data(), (int)names.size());

    // --- Selected object properties -----------------------------------------
    MachineObject& sel = objects[selectedIndex];

    ImGui::Separator();
    ImGui::TextUnformatted(typeName(sel.type));

    bool changed = false;

    changed |= ImGui::SliderFloat("Pos X",  &sel.position.x, -1.0f, 1.0f);
    changed |= ImGui::SliderFloat("Pos Y",  &sel.position.y, -1.0f, 1.0f);
    changed |= ImGui::SliderFloat("Pos Z",  &sel.position.z, -1.0f, 1.0f);

    if (sel.type == MachineObjectType::SphereBlock) {
        changed |= ImGui::SliderFloat("Radius",     &sel.halfSize.x, 0.02f, 0.8f);
    } else if (sel.type == MachineObjectType::Ramp) {
        changed |= ImGui::SliderFloat("Half Width", &sel.halfSize.x, 0.05f, 1.0f);
        changed |= ImGui::SliderFloat("Half Depth", &sel.halfSize.z, 0.05f, 1.0f);
        changed |= ImGui::SliderFloat("Angle (deg)",&sel.angle,     -60.0f, 60.0f);
    } else {
        changed |= ImGui::SliderFloat("Half Width", &sel.halfSize.x, 0.02f, 1.0f);
        changed |= ImGui::SliderFloat("Half Height",&sel.halfSize.y, 0.02f, 0.8f);
        changed |= ImGui::SliderFloat("Half Depth", &sel.halfSize.z, 0.02f, 1.0f);
    }

    if (sel.type == MachineObjectType::OpenContainer ||
        sel.type == MachineObjectType::Channel) {
        changed |= ImGui::SliderFloat("Wall Thick", &sel.wallThickness, 0.005f, 0.1f);
    }

    changed |= ImGui::SliderFloat("Restitution", &sel.restitution, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Friction",    &sel.friction,    0.0f, 1.0f);

    if (changed) {
        rebuildObstacleSystem(obs);
        rebuilt = true;
    }

    ImGui::Separator();
    if (ImGui::Button("Delete Selected")) {
        removeSelected();
        rebuildObstacleSystem(obs);
        rebuilt = true;
    }

    return rebuilt;
}

// ---------------------------------------------------------------------------
void MachineObjectSystem::setDefaultScene(ObstacleSystem& obs)
{
    objects.clear();
    selectedIndex = -1;

    // Ramp: upper-left area, tilted 30° (right side lower), deflects fluid rightward
    addObject(MachineObjectType::Ramp,
              "Ramp 1",
              {-0.25f, -0.05f, 0.f},
              {0.3f, 0.01f, 0.3f},
              /*angle=*/ 30.0f);

    // Channel: mid-area, catches overflow from the ramp and funnels it along Z
    addObject(MachineObjectType::Channel,
              "Channel 1",
              {0.1f, -0.38f, 0.f},
              {0.12f, 0.1f, 0.35f});

    // Open container: lower-right, catch basin at the bottom
    addObject(MachineObjectType::OpenContainer,
              "Container 1",
              {0.4f, -0.72f, 0.f},
              {0.22f, 0.18f, 0.22f});

    selectedIndex = -1;
    rebuildObstacleSystem(obs);
}

} // namespace physics3d

#pragma once
#include "physics3d/machine_object.h"
#include "physics3d/obstacle_system.h"
#include <vector>

namespace physics3d {

class MachineObjectSystem {
public:
    std::vector<MachineObject> objects;
    int selectedIndex = -1;

    void addObject(MachineObjectType type, const std::string& name,
                   const Vec3& position, const Vec3& halfSize, float angle = 0.0f);
    void removeSelected();

    // Clears and repopulates obs from all high-level objects.
    void rebuildObstacleSystem(ObstacleSystem& obs) const;

    // Renders an ImGui "Machine Objects" collapsing section.
    // Calls rebuildObstacleSystem internally whenever a parameter changes.
    // Returns true if a rebuild occurred this frame.
    bool renderImGui(ObstacleSystem& obs);

    // Populates a simple default Rube Goldberg layout.
    void setDefaultScene(ObstacleSystem& obs);

private:
    void convertObject(const MachineObject& obj, ObstacleSystem& obs) const;
};

} // namespace physics3d

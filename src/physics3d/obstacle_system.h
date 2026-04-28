#pragma once
#include "physics3d/obstacle.h"
#include <vector>

namespace physics3d {

// Aggregates all obstacles in the scene. Call resolveAll() once per particle
// per PBF iteration (and once after initial position prediction).
class ObstacleSystem {
public:
    std::vector<PlaneObstacle>  planes;
    std::vector<SphereObstacle> spheres;
    std::vector<BoxObstacle>    boxes;

    // Apply every obstacle's collision response to a single particle.
    // Safe to call from a parallel loop — only touches pos and vel for particle i.
    void resolveAll(Vec3& pos, Vec3& vel, float particleRadius) const;
};

} // namespace physics3d

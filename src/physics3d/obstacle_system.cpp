#include "physics3d/obstacle_system.h"

namespace physics3d {

void ObstacleSystem::resolveAll(Vec3& pos, Vec3& vel, float particleRadius) const
{
    for (const auto& p : planes)  p.resolveParticle(pos, vel, particleRadius);
    for (const auto& s : spheres) s.resolveParticle(pos, vel, particleRadius);
    for (const auto& b : boxes)   b.resolveParticle(pos, vel, particleRadius);
}

} // namespace physics3d

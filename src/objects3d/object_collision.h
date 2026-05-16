#pragma once
#include <vector>
#include "objects3d/editor_state.h"
#include "linear_algebra.h"


// Flat OBB collider ready for particle tests
struct OBBCollider {
    Vec3  center;
    Vec3  halfExtents;
    Vec3  axes[3];    // local X, Y, Z unit vectors in world space
    float restitution;
};

// Flat sphere collider
struct SphereCollider {
    Vec3  center;
    float radius;
    float restitution;
};

int GetProjectionHits();

// Build all collision shapes from the scene for one frame.
// Only includes active (non-ghost) objects.
void BuildCollisionShapes(const std::vector<RGObject>& objects,
                          std::vector<OBBCollider>&    obbs,
                          std::vector<SphereCollider>& spheres);

// Resolve a single particle position/velocity against one OBB.
// Returns true if the particle was inside and was pushed out.
bool ResolveParticleOBB(Vec3 &pos, Vec3 &vel, const OBBCollider &obb);

void ProjectParticleOBB(Vec3 *pos, const OBBCollider &obb);

// Resolve a single particle position/velocity against one sphere.
bool ResolveParticleSphere(Vec3& pos, Vec3& vel, const SphereCollider& s);

// Apply all object collisions to all active particles.
// Called once per frame after particles.Update().
void ApplyObjectCollisions(std::vector<Vec3>& positions,
                           std::vector<Vec3>& velocities,
                           int                nActive,
                           const std::vector<RGObject>& objects);

#pragma once
#include "linear_algebra.h"

namespace physics3d {

// Half-space defined by a point on the plane and an outward unit normal.
// Particles are kept on the side the normal points toward.
struct PlaneObstacle {
    Vec3  point;
    Vec3  normal;        // must be unit length
    float restitution = 0.3f;
    float friction    = 0.1f;

    void resolveParticle(Vec3& pos, Vec3& vel, float particleRadius) const;
};

// Solid sphere: particles are pushed to the outside surface.
struct SphereObstacle {
    Vec3  center;
    float radius;
    float restitution = 0.3f;
    float friction    = 0.1f;

    void resolveParticle(Vec3& pos, Vec3& vel, float particleRadius) const;
};

// Axis-aligned solid box: particles are pushed out through the nearest face.
struct BoxObstacle {
    Vec3  center;
    Vec3  halfSize;
    float restitution = 0.3f;
    float friction    = 0.1f;

    void resolveParticle(Vec3& pos, Vec3& vel, float particleRadius) const;
};

} // namespace physics3d

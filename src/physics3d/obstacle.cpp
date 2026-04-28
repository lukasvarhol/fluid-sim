#include "physics3d/obstacle.h"
#include <algorithm>
#include <cmath>

namespace physics3d {

// ---------------------------------------------------------------------------
// PlaneObstacle
// ---------------------------------------------------------------------------
void PlaneObstacle::resolveParticle(Vec3& pos, Vec3& vel, float particleRadius) const
{
    // Signed distance from the particle center to the plane (positive = safe side)
    float signedDist = (pos - point).dot(normal);
    if (signedDist >= particleRadius)
        return;

    // Push particle out so its surface just touches the plane
    pos += normal * (particleRadius - signedDist);

    // Cancel the velocity component driving into the plane, apply restitution
    float vn = vel.dot(normal);
    if (vn < 0.0f) {
        Vec3 vNormal  = normal * vn;
        Vec3 vTangent = vel - vNormal;
        vel -= vNormal * (1.0f + restitution);
        vel -= vTangent * friction;
    }
}

// ---------------------------------------------------------------------------
// SphereObstacle
// ---------------------------------------------------------------------------
void SphereObstacle::resolveParticle(Vec3& pos, Vec3& vel, float particleRadius) const
{
    Vec3  diff    = pos - center;
    float dist2   = diff.dot(diff);
    float minDist = radius + particleRadius;

    if (dist2 >= minDist * minDist)
        return;

    Vec3  n;
    float dist;
    if (dist2 < 1e-10f) {
        // Particle exactly at sphere center — push straight up to avoid divide-by-zero
        n    = {0.0f, 1.0f, 0.0f};
        dist = 0.0f;
    } else {
        dist = std::sqrt(dist2);
        n    = diff * (1.0f / dist);
    }

    pos += n * (minDist - dist);

    float vn = vel.dot(n);
    if (vn < 0.0f) {
        Vec3 vNormal  = n * vn;
        Vec3 vTangent = vel - vNormal;
        vel -= vNormal * (1.0f + restitution);
        vel -= vTangent * friction;
    }
}

// ---------------------------------------------------------------------------
// BoxObstacle
// ---------------------------------------------------------------------------
void BoxObstacle::resolveParticle(Vec3& pos, Vec3& vel, float particleRadius) const
{
    const Vec3 boxMin = {center.x - halfSize.x, center.y - halfSize.y, center.z - halfSize.z};
    const Vec3 boxMax = {center.x + halfSize.x, center.y + halfSize.y, center.z + halfSize.z};

    // Closest point on the AABB surface to the particle center
    const Vec3 closest = {
        std::clamp(pos.x, boxMin.x, boxMax.x),
        std::clamp(pos.y, boxMin.y, boxMax.y),
        std::clamp(pos.z, boxMin.z, boxMax.z)
    };

    const bool inside = (closest.x == pos.x && closest.y == pos.y && closest.z == pos.z);

    Vec3  n;
    float penetration;

    if (inside) {
        // Particle center is inside the box: push out through nearest face
        const float dists[6] = {
            pos.x - boxMin.x,   // distance to -X face
            boxMax.x - pos.x,   // distance to +X face
            pos.y - boxMin.y,   // distance to -Y face
            boxMax.y - pos.y,   // distance to +Y face
            pos.z - boxMin.z,   // distance to -Z face
            boxMax.z - pos.z,   // distance to +Z face
        };
        const Vec3 normals[6] = {
            {-1.0f,  0.0f,  0.0f},
            { 1.0f,  0.0f,  0.0f},
            { 0.0f, -1.0f,  0.0f},
            { 0.0f,  1.0f,  0.0f},
            { 0.0f,  0.0f, -1.0f},
            { 0.0f,  0.0f,  1.0f},
        };
        int minFace = 0;
        for (int f = 1; f < 6; ++f)
            if (dists[f] < dists[minFace]) minFace = f;

        n           = normals[minFace];
        penetration = dists[minFace] + particleRadius;
    } else {
        // Particle center is outside: check proximity to box surface
        Vec3  diff = pos - closest;
        float dist = diff.magnitude();
        if (dist >= particleRadius)
            return;  // outside and far enough — no collision

        n           = diff * (1.0f / dist);
        penetration = particleRadius - dist;
    }

    pos += n * penetration;

    float vn = vel.dot(n);
    if (vn < 0.0f) {
        Vec3 vNormal  = n * vn;
        Vec3 vTangent = vel - vNormal;
        vel -= vNormal * (1.0f + restitution);
        vel -= vTangent * friction;
    }
}

} // namespace physics3d

#include "object_collision.h"
#include "particle_config.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <execution>

// ---------------------------------------------------------------------------
// Child-box definitions — must stay consistent with object_renderer.cpp
// ---------------------------------------------------------------------------

struct ChildBoxLocal { Vec3 localCenter; Vec3 halfExtents; };

static std::vector<ChildBoxLocal> GetChildBoxesLocal(const RGObject& obj)
{
    float t  = obj.wallThickness;
    Vec3  he = obj.halfExtents;
    std::vector<ChildBoxLocal> out;

    switch (obj.type) {
    case RGObjectType::BOX:
    case RGObjectType::RAMP:
    case RGObjectType::BRIDGE:
    case RGObjectType::SPLASH_WEDGE:
    case RGObjectType::CHANNEL:
        out.push_back({ {0, -he.y + t*0.5f, 0}, {he.x, t*0.5f, he.z} }); // bottom
        out.push_back({ {-he.x + t*0.5f, 0, 0}, {t*0.5f, he.y, he.z} }); // left
        out.push_back({ { he.x - t*0.5f, 0, 0}, {t*0.5f, he.y, he.z} }); // right
        break;

    case RGObjectType::CONTAINER:
        out.push_back({ {0, -he.y + t*0.5f, 0}, {he.x, t*0.5f, he.z} });        // bottom
        out.push_back({ {-he.x + t*0.5f, 0, 0}, {t*0.5f, he.y, he.z} });        // left
        out.push_back({ { he.x - t*0.5f, 0, 0}, {t*0.5f, he.y, he.z} });        // right
        out.push_back({ {0, 0,  he.z - t*0.5f}, {he.x, he.y, t*0.5f} });        // front
        out.push_back({ {0, 0, -he.z + t*0.5f}, {he.x, he.y, t*0.5f} });        // back
        break;

    case RGObjectType::FUNNEL:
        // No longer used by cell builder (Feature::Funnel emits BOX children).
        // Kept as fallback for any legacy RGObjectType::FUNNEL objects.
        break;

    case RGObjectType::DROP_CHUTE:
        out.push_back({ {-he.x + t*0.5f, 0, 0}, {t*0.5f, he.y, he.z} }); // left
        out.push_back({ { he.x - t*0.5f, 0, 0}, {t*0.5f, he.y, he.z} }); // right
        out.push_back({ {0, 0,  he.z - t*0.5f}, {he.x, he.y, t*0.5f} }); // front
        out.push_back({ {0, 0, -he.z + t*0.5f}, {he.x, he.y, t*0.5f} }); // back
        break;

    case RGObjectType::SPHERE:
        break; // handled separately
    }
    return out;
}

// ---------------------------------------------------------------------------
// Build flat collision shape lists from the scene
// ---------------------------------------------------------------------------

void BuildCollisionShapes(const std::vector<RGObject>& objects,
                          std::vector<OBBCollider>&    obbs,
                          std::vector<SphereCollider>& spheres)
{
    obbs.clear();
    spheres.clear();

    for (const auto& obj : objects) {
        if (!obj.active) continue; // ghosts excluded

        Vec3 worldPos = obj.position;

        if (obj.type == RGObjectType::SPHERE) {
            spheres.push_back({ worldPos, obj.radius, energyRetention });
            continue;
        }

        // Build rotation axes from Euler angles
        Mat4 rotMat = CreateMatrixRotationXYZ(obj.rotation);
        Vec3 axes[3] = {
            TransformDir(rotMat, {1,0,0}),
            TransformDir(rotMat, {0,1,0}),
            TransformDir(rotMat, {0,0,1})
        };

        auto children = GetChildBoxesLocal(obj);
        for (const auto& cb : children) {
            OBBCollider obb;
            obb.center      = worldPos + TransformDir(rotMat, cb.localCenter);
            obb.halfExtents = cb.halfExtents;
            obb.axes[0]     = axes[0];
            obb.axes[1]     = axes[1];
            obb.axes[2]     = axes[2];
            obb.restitution = energyRetention;
            obbs.push_back(obb);
        }
    }
}

// ---------------------------------------------------------------------------
// Single-particle collision resolution
// ---------------------------------------------------------------------------

bool ResolveParticleOBB(Vec3& pos, Vec3& vel, const OBBCollider& obb)
{
    // Transform particle to OBB local space
    Vec3 d = pos - obb.center;
    float lx = d.Dot(obb.axes[0]);
    float ly = d.Dot(obb.axes[1]);
    float lz = d.Dot(obb.axes[2]);

    float hx = obb.halfExtents.x;
    float hy = obb.halfExtents.y;
    float hz = obb.halfExtents.z;

    // Early-out: not inside OBB
    if (std::abs(lx) >= hx) return false;
    if (std::abs(ly) >= hy) return false;
    if (std::abs(lz) >= hz) return false;

    // Find face with minimum penetration depth
    float penX = hx - std::abs(lx);
    float penY = hy - std::abs(ly);
    float penZ = hz - std::abs(lz);

    Vec3  pushDir;
    float pushDist;

    if (penX <= penY && penX <= penZ) {
        pushDir  = obb.axes[0] * (lx >= 0.0f ? 1.0f : -1.0f);
        pushDist = penX;
    } else if (penY <= penX && penY <= penZ) {
        pushDir  = obb.axes[1] * (ly >= 0.0f ? 1.0f : -1.0f);
        pushDist = penY;
    } else {
        pushDir  = obb.axes[2] * (lz >= 0.0f ? 1.0f : -1.0f);
        pushDist = penZ;
    }

    // Resolve position (small bias to prevent re-penetration)
    pos = pos + pushDir * (pushDist + 0.0002f);

    // Dampen velocity component along push direction
    float vDot = vel.Dot(pushDir);
    if (vDot < 0.0f) {
        vel = vel - pushDir * ((1.0f + obb.restitution) * vDot);
    }

    return true;
}

bool ResolveParticleSphere(Vec3& pos, Vec3& vel, const SphereCollider& s)
{
    Vec3  d    = pos - s.center;
    float dist = d.Magnitude();
    if (dist >= s.radius) return false;
    if (dist < 0.0001f) { pos.y += s.radius; return true; } // degenerate: just push up

    Vec3  n     = d * (1.0f / dist);
    float pen   = s.radius - dist;
    pos = pos + n * (pen + 0.0002f);

    float vDot = vel.Dot(n);
    if (vDot < 0.0f) vel = vel - n * ((1.0f + s.restitution) * vDot);

    return true;
}

// ---------------------------------------------------------------------------
// Per-frame collision pass
// ---------------------------------------------------------------------------

void ApplyObjectCollisions(std::vector<Vec3>& positions,
                           std::vector<Vec3>& velocities,
                           int                nActive,
                           const std::vector<RGObject>& objects)
{
    if (objects.empty()) return;

    // Build flat collider lists for this frame
    static std::vector<OBBCollider>    obbs;
    static std::vector<SphereCollider> spheres;
    BuildCollisionShapes(objects, obbs, spheres);

    if (obbs.empty() && spheres.empty()) return;

    // Solve multiple iterations to handle stacking / simultaneous contacts
    const int NUM_ITERS = 2;
    for (int iter = 0; iter < NUM_ITERS; ++iter) {
        static std::vector<int> idx;
        idx.resize(nActive);
        std::iota(idx.begin(), idx.end(), 0);

        std::for_each(std::execution::par_unseq, idx.begin(), idx.end(),
            [&](int i) {
                for (const auto& obb : obbs)
                    ResolveParticleOBB(positions[i], velocities[i], obb);
                for (const auto& s : spheres)
                    ResolveParticleSphere(positions[i], velocities[i], s);
            });
    }
}

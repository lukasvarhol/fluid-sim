#pragma once
#include "editor_state.h"
#include "../linear_algebra.h"
#include "particle_config.h"
#include <array>
#include <cmath>
#include <numeric>

struct SDFCollider {
  RGObjectType type;
  Vec3 worldPosition;
  std::array<Vec3, 3> rotationAxes;
  float restitution;
};
  
void BuildSDFColliders(const std::vector<RGObject>& obbjects, std::vector<SDFCollider>& colliders);

Vec3 sdfGradient(RGObjectType type, Vec3 localPosition);
void ProjectParticleSDF(Vec3& position, Vec3& velocity, const SDFCollider& collider);
std::vector<Vec3> SampleSDFInside(const SDFCollider &collider, float gridStep);

// Triangle Collision (for Benchmarking)

struct TriCollider {
  std::vector<Vec3> triangles;
  float restitution;
};

void ProjectParticleTri(const Vec3 &p, const Vec3 &velocity,
                        const TriCollider &triCollider, Vec3 &closestPointOut);
Vec3 ClosestPtPointTriangle(const Vec3 &p, const Vec3 &a, const Vec3 &b,
                            const Vec3 &c);

DEVICE_CALLABLE
inline float sdfDispatch(RGObjectType type, Vec3 localPosition) {
    switch (type) {
  case RGObjectType::S_CHANNEL:
    return sdfSChannel(localPosition);
  case RGObjectType::L_CHANNEL:
    return sdfLChannel(localPosition);
  case RGObjectType::RAMP:
    return sdfBRamp(localPosition);
  default:
    return 1e9f; // too far away
  }
}
